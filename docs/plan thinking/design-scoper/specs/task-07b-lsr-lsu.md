# Task Spec: T-07b — LSR/LSU/LSAck Handling

> Implement Link State Request, Update, Acknowledgment messages.

---

## 1. Overview
Sau DD exchange (Loading state), gửi LSR để request LSAs còn thiếu, nhận LSU, install, và ACK.

## 2. Requirements
- Gửi LSR cho missing LSAs
- Nhận LSU → install LSAs vào LSDB
- Gửi LSAck xác nhận
- Xử lý LSR từ neighbor → gửi LSU đáp ứng

## 3. Input
- `requestedLSAs` list từ T-07a
- LSDB từ T-08
- OspfLSR, OspfLSU, OspfLSAck classes

## 4. Process

**sendLSR(nbrID):**
```cpp
void OspfRouter::sendLSR(int nbrID) {
    auto* lsr = new OspfLSR("LSR");
    auto& nbr = neighbors[nbrID];
    for (int advRouter : nbr.requestedLSAs) {
        lsr->appendRequestTypes(1);  // Router-LSA
        lsr->appendRequestIDs(advRouter);
        lsr->appendRequestRouters(advRouter);
    }
    send(lsr, "ppg$o", getGateIndex(nbrID));
}
```

**processLSU(OspfLSU* lsu, int fromGate):**
```cpp
void OspfRouter::processLSU(OspfLSU* lsu, int fromGate) {
    int count = lsu->getLsaRoutersArraySize();
    for (int i = 0; i < count; i++) {
        RouterLSA lsa;
        lsa.advertisingRouter = lsu->getLsaRouters(i);
        lsa.lsSequenceNumber = lsu->getLsaSeqNums(i);
        // Parse links...
        
        bool installed = lsdb->installLSA(lsa);
        if (installed) {
            // Flood further (T-09)
            floodLSA(lsa, fromGate);
            // Schedule SPF (T-10a)
            scheduleSPF();
        }
    }
    
    // Gửi LSAck
    sendLSAck(lsu, fromGate);
}

void OspfRouter::processLSR(OspfLSR* lsr, int fromGate) {
    auto* lsu = new OspfLSU("LSU");
    int nbrID = getNeighborID(fromGate);
    
    for (int i = 0; i < lsr->getRequestRoutersArraySize(); i++) {
        int advRouter = lsr->getRequestRouters(i);
        auto* lsa = lsdb->lookupLSA(advRouter);
        if (lsa) {
            lsu->appendLsaRouters(lsa->advertisingRouter);
            lsu->appendLsaSeqNums(lsa->lsSequenceNumber);
            lsu->appendLsaLinkCounts(lsa->links.size());
        }
    }
    
    send(lsu, "ppg$o", fromGate);
}
```

**Transition LoadingDone:**
Khi all requested LSAs received → `updateNeighborFSM(nbrID, LOADING_DONE)` → Full

## 5. Output
- LSR/LSU/LSAck packets
- LSDB synchronized (state = Full)

## 6. Acceptance Criteria
- Sau DD exchange → gửi LSR
- Neighbor gửi LSU → install LSA vào LSDB
- Gửi LSAck khi nhận LSU
- Khi all LSAs installed → state = Full
- 2 routers với LSDB khác nhau → sync thành Full

## 7. Related Tasks
- T-06 (FSM): Loading, Full states
- T-07a (DD): cung cấp requested LSAs
- T-08 (LSDB): install + lookup LSAs
- T-09 (Flooding): flood LSA sau khi install
- T-10a (SPF): schedule SPF sau LSA install

## 8. Notes
- Retransmission: nếu không nhận LSAck trong rxmtInterval → gửi lại LSU
- Dùng request list để track LSAs chưa nhận
