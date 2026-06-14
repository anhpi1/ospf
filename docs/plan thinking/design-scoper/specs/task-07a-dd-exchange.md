# Task Spec: T-07a — Database Description (DD) Exchange

> Implement master/slave negotiation và exchange LSA headers qua DD packets.

---

## 1. Overview
Khi neighbor state = ExStart, bắt đầu DD exchange: master elect, exchange LSA headers, xác định LSAs cần request.

## 2. Requirements
- Master/slave negotiation (MS bit)
- Sequence number tracking
- LSA header list exchange (M bit for more)
- P2P simplified: master = higher routerID

## 3. Input
- Neighbor state = ExStart (từ T-06)
- LSDB headers từ T-08

## 4. Process

**startDatabaseExchange(nbrID)** — gọi khi ExStart:
1. Determine master: higher routerID = master
2. Slave trả lời DD với MS=0, seq=master's seq
3. Master gửi DD với LSA headers từ LSDB, M=1 nếu còn
4. Slave gửi DD với LSA headers (nếu có)
5. Khi tất cả headers exchanged (M=0), tạo request list cho LSAs không có trong local LSDB
6. Transition: ExchangeDone → Loading

```cpp
void OspfRouter::startDatabaseExchange(int nbrID) {
    auto& nbr = neighbors[nbrID];
    nbr.isMaster = (routerID > nbrID);
    nbr.ddSequenceNumber = nbr.isMaster ? getNextDDSeq() : 0;
    
    // Slave gửi DD trước (trả lời)
    if (!nbr.isMaster) {
        sendDD(nbrID, true, true, false, nbr.ddSequenceNumber, {});
    }
}

void OspfRouter::processDD(OspfDD* dd, int nbrID) {
    auto& nbr = neighbors[nbrID];
    
    if (nbr.state == EXSTART) {
        if (dd->getMsBit() && !nbr.isMaster && dd->getIBit()) {
            // Slave nhận được DD từ master → đồng ý
            nbr.ddSequenceNumber = dd->getDdSequenceNumber();
            updateNeighborFSM(nbrID, NEGOTIATION_DONE);
        }
    }
    
    if (nbr.state == EXCHANGE) {
        // Compare sequence number
        if (dd->getDdSequenceNumber() != nbr.ddSequenceNumber + 1) {
            updateNeighborFSM(nbrID, SEQ_NUM_MISMATCH);
            return;
        }
        
        // Check LSA headers — add missing ones to request list
        for (int i = 0; i < dd->getLsaHeadersArraySize(); i++) {
            int advRouter = dd->getLsaHeaders(i);
            if (!lsdb->lookupLSA(advRouter)) {
                nbr.requestedLSAs.push_back(advRouter);
            }
        }
        
        nbr.ddSequenceNumber = dd->getDdSequenceNumber();
        
        // Gửi DD response (hoặc next batch)
        bool hasMore = !nbr.requestedLSAs.empty();
        sendDD(nbrID, false, hasMore, nbr.isMaster, nbr.ddSequenceNumber, getLSABatch());
        
        if (!dd->getMBit() && !hasMore) {
            updateNeighborFSM(nbrID, EXCHANGE_DONE);
        }
    }
}
```

## 5. Output
- DD packets exchanged
- `requestedLSAs` list (cho T-07b)

## 6. Acceptance Criteria
- Master/slave election thành công
- LSDB header list exchanged
- DD sequence numbers tăng đều
- ExchangeDone → state = Loading

## 7. Related Tasks
- T-06 (FSM): ExStart, Exchange transitions
- T-07b (LSR/LSU): dùng requestedLSAs list
- T-08 (LSDB): cung cấp LSDB headers cho DD packets

## 8. Notes
- P2P simplification: không cần DR negotiation
- Nếu seq mismatch: reset về ExStart
