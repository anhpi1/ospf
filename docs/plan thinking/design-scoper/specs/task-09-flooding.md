# Task Spec: T-09 — LSA Flooding

> Flood LSAs mới ra tất cả interfaces (trừ interface nhận).

---

## 1. Overview
Khi install LSA mới vào LSDB, flood ra tất cả neighbors để đồng bộ LSDB toàn mạng.

## 2. Requirements
- Flood LSA ra tất cả ppg[] ngoại trừ ppg nhận
- Retransmission nếu không nhận LSAck trong rxmtInterval
- Chỉ flood LSA mới hơn (sequence number check)

## 3. Input
- RouterLSA vừa install (từ T-07b)
- Danh sách interfaces + neighbor states
- rxmtInterval parameter

## 4. Process

```cpp
void OspfRouter::floodLSA(const RouterLSA& lsa, int incomingGate) {
    for (int i = 0; i < gateSize("ppg"); i++) {
        if (i == incomingGate) continue;  // Không flood lại interface nhận
        
        int nbrID = getNeighborID(i);
        if (neighbors[nbrID].state != FULL) continue;  // Chỉ flood qua Full adj
        
        auto* lsu = new OspfLSU("LSU");
        lsu->appendLsaRouters(lsa.advertisingRouter);
        lsu->appendLsaSeqNums(lsa.lsSequenceNumber);
        lsu->appendLsaLinkCounts(lsa.links.size());
        send(lsu, "ppg$o", i);
        
        // Schedule retransmission
        cMessage* retxTimer = new cMessage("RETX_TIMER");
        retxTimer->setContextPointer((void*)(intptr_t)(i * 1000 + lsa.advertisingRouter));
        scheduleAt(simTime() + rxmtInterval, retxTimer);
        
        logTx(routerID, nbrID, "LSU", "flood LSA from " + to_string(lsa.advertisingRouter));
    }
}

void OspfRouter::processLSAck(OspfLSAck* ack, int fromGate) {
    for (int i = 0; i < ack->getAckedRoutersArraySize(); i++) {
        int advRouter = ack->getAckedRouters(i);
        // Cancel retransmission timer for this LSA on this interface
        cancelRetxTimer(fromGate, advRouter);
    }
}
```

**Duplicate detection:** Trong processLSU (T-07b), kiểm tra seq# trước khi install.

## 5. Output
- LSU packets gửi đến tất cả neighbors
- Retransmission timers quản lý
- LSAck xác nhận

## 6. Acceptance Criteria
- Router install LSA mới → flood ra tất cả neighbors (trừ interface nhận)
- Neighbor nhận → install → flood tiếp
- LSA đến được tất cả routers trong mạng (flooding)
- LSAck đến → cancel timer

## 7. Related Tasks
- T-07b (LSR/LSU): process LSU + gọi floodLSA
- T-08 (LSDB): trung tâm của flooding
- T-10a (SPF): schedule SPF sau flood

## 8. Notes
- Mạng P2P nhỏ → không cần rate-limiting
- Retransmission: nếu timer fire mà chưa có LSAck → gửi lại
- Sequence number + LSDB duplicate check prevent broadcast storm
