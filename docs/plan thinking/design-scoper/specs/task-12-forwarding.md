# Task Spec: T-12 — Data Forwarding

> Forward data packets dựa trên OSPF routing table.

---

## 1. Overview
Khi OspfRouter nhận DataPacket (từ Client hoặc từ neighbor), lookup routing table và forward.

## 2. Requirements
- Lookup destination trong routing table
- Nếu self là destination → deliver lên Client
- Nếu không → forward đến next-hop
- ECMP: chọn next-hop (first hoặc round-robin)

## 3. Input
- DataPacket từ Client (local gate) hoặc từ neighbor (ppg[i])
- Routing table từ T-10b

## 4. Process

```cpp
void OspfRouter::handleData(cMessage* msg) {
    DataPacket* data = check_and_cast<DataPacket*>(msg);
    int dest = data->getDstRouterID();
    
    if (dest == routerID) {
        // Đã đến đích → deliver lên Client
        send(data, "localOut");
        logTx(routerID, routerID, "DATA_DELIVER", 
              "from " + to_string(data->getSrcRouterID()));
        return;
    }
    
    // Lookup routing table
    auto* entry = routingTable->lookup(dest);
    if (!entry || entry->nextHops.empty()) {
        EV << "No route to " << dest << endl;
        delete data;
        return;
    }
    
    // ECMP: chọn next-hop (simple: first, hoặc round-robin)
    const auto& nextHop = entry->nextHops[0];  // hoặc roundRobin
    send(data, "ppg$o", nextHop.interfaceIndex);
    logTx(routerID, nextHop.neighborID, "DATA_FWD", 
          "to " + to_string(dest));
}
```

## 5. Output
- DataPacket forwarded đến next-hop
- Hoặc delivered đến Client nếu là đích

## 6. Acceptance Criteria
- R6 gửi Data → R10: packet đến R10
- Packet đi qua intermediate routers
- Mỗi hop có log transaction

## 7. Related Tasks
- T-10b (Routing Table): lookup + ECMP
- T-11 (Client): nguồn và đích data
- T-13 (Logger): log forwarding actions
- T-14 (Dump): convergence tracking

## 8. Notes
- Nếu không có route → drop packet + log
- ECMP: có thể implement round-robin để load-balance
- Data packets không có TTL → cẩn thận loop nếu routing table sai
