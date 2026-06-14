# Task Spec: T-14 — Routing Table Dump + Convergence Time

> In routing table định kỳ, đo convergence time khi topology thay đổi.

---

## 1. Overview
Khi SPF chạy xong, dump routing table ra console và ghi vào file. Đo thời gian từ lúc topology change đến khi tất cả routers ổn định.

## 2. Requirements
- dump() method in OspfRoutingTable
- Gọi dump sau mỗi SPF calculation
- Đo convergence time: timestamp topology change → timestamp route stable
- Ghi dump vào file riêng

## 3. Input
- Routing table từ T-10b
- Topology change events (link disconnect/reconnect)

## 4. Process

**dumpRoutingTable():**
```cpp
void OspfRouter::dumpRoutingTable() {
    EV << "╔═══════════════════════════════════════╗" << endl;
    EV << "║ Routing Table for Router " << ipFormat(routerID) << "          ║" << endl;
    EV << "╠═══════════════════════════════════════╣" << endl;
    EV << "║ Dest        Cost  NextHops            ║" << endl;
    EV << "╠═══════════════════════════════════════╣" << endl;
    
    for (const auto& entry : routingTable->getEntries()) {
        std::string nhStr;
        for (size_t i = 0; i < entry.nextHops.size(); i++) {
            if (i > 0) nhStr += ",";
            nhStr += ipFormat(entry.nextHops[i].neighborID);
        }
        EV << "║ " << std::left << std::setw(10) << ipFormat(entry.destination)
           << std::right << std::setw(5) << entry.cost << "  "
           << std::left << std::setw(20) << nhStr << " ║" << endl;
    }
    EV << "╚═══════════════════════════════════════╝" << endl;
    
    // Ghi vào file results/routing_dumps/s{scenario}_r{routerID}.txt
    // ...
}
```

**Convergence measurement:**
```cpp
class OspfRouter {
    simtime_t topologyChangeTime;
    bool measuringConvergence = false;
    bool converged = false;
    int lastRouteCount = 0;
    
    void onTopologyChange() {
        topologyChangeTime = simTime();
        measuringConvergence = true;
        converged = false;
        EV << "Topology change detected at T=" << simTime() << endl;
    }
    
    void onSPFComplete() {
        dumpRoutingTable();
        
        if (measuringConvergence && !converged) {
            // Check if routing table stabilized (no change since last SPF)
            int currentCount = routingTable->size();
            if (currentCount == lastRouteCount && currentCount > 0) {
                converged = true;
                simtime_t convergeTime = simTime() - topologyChangeTime;
                EV << "═══ CONVERGED at T=" << simTime() 
                   << " (took " << convergeTime << ") ═══" << endl;
                logConvergence(convergeTime);
            }
            lastRouteCount = currentCount;
        }
    }
};
```

## 5. Output
- Console dump sau mỗi SPF
- `results/routing_dumps/s{scenario}_r{routerID}.txt`
- Convergence time log

## 6. Acceptance Criteria
- Routing table hiển thị đúng format
- Convergence time được đo và log
- Sau topology change → routing table thay đổi → ổn định
- File dump ghi được

## 7. Related Tasks
- T-10b (Routing Table): dữ liệu
- T-16 (Scenarios): topology change events

## 8. Notes
- Convergence = tất cả routers có routing table ổn định (không thay đổi qua 2 SPF cycles)
- Dùng in ngưỡng + console EV hoặc ghi riêng file
