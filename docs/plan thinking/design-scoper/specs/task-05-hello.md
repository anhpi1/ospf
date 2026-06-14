# Task Spec: T-05 — Hello Protocol

> Implement OSPF Hello Protocol — neighbor discovery và maintenance.

---

## 1. Overview
Gửi Hello packets định kỳ, xử lý Hello đến từ neighbors, cập nhật neighbor list.

## 2. Requirements
- Hello timer: gửi Hello mỗi helloInterval (default 1s)
- Process Hello từ neighbors
- Theo dõi InactivityTimer cho mỗi neighbor

## 3. Input
- `helloInterval`, `routerDeadInterval` parameters
- OspfHello class từ T-03
- OspfRouter skeleton từ T-04

## 4. Process

**sendHello():**
```cpp
void OspfRouter::sendHello() {
    auto* hello = new OspfHello("HELLO");
    hello->setRouterID(routerID);
    hello->setHelloInterval(helloInterval);
    hello->setDeadInterval(routerDeadInterval);
    
    // Điền list neighbors đã thấy (đang ở Init, 2Way, ...)
    for (auto& [nbrID, state] : neighborStates) {
        hello->appendNeighborIDs(nbrID);
    }
    
    // Gửi ra tất cả interfaces
    for (int i = 0; i < gateSize("ppg"); i++) {
        OspfHello* copy = i == 0 ? hello : hello->dup();
        send(copy, "ppg$o", i);
        logTx(routerID, neighborID(i), "HELLO", ...);
    }
}

void OspfRouter::processHello(OspfHello* hello, int fromGate) {
    int srcRouter = hello->getRouterID();
    
    // Kiểm tra xem self có trong neighbor list không (bidirectional)
    bool bidirectional = false;
    for (int nbr : hello->getNeighborIDs()) {
        if (nbr == routerID) { bidirectional = true; break; }
    }
    
    // Update neighbor state
    auto& state = neighborStates[srcRouter];
    if (!bidirectional && state == Down) {
        state = Init;
    } else if (bidirectional && state == Init) {
        state = TwoWay;
        // Trên P2P, TwoWay → ngay lập tức chuyển ExStart
        startDatabaseExchange(srcRouter);
    }
    
    // Reset InactivityTimer cho neighbor này
    rescheduleInactivityTimer(srcRouter);
}
```

**Hello timer:**
```cpp
void OspfRouter::initialize() {
    helloTimer = new cMessage("HELLO_TIMER");
    scheduleAt(simTime() + helloInterval, helloTimer);
}

void OspfRouter::handleTimer(cMessage* msg) {
    if (msg == helloTimer) {
        sendHello();
        scheduleAt(simTime() + helloInterval, msg);
    }
    // InactivityTimer ...
}
```

## 5. Output
- OspfHello packets trên wire mỗi helloInterval
- `neighborStates` map được cập nhật

## 6. Acceptance Criteria
- Router gửi Hello ra tất cả ppg[] mỗi 1s
- RouterB nhận Hello từ RouterA → neighbor state = Init
- RouterB thấy RouterA trong neighbor list → state = TwoWay
- Log transaction: `HELLO` entries

## 7. Related Tasks
- T-03 (.msg): OspfHello packet
- T-04 (Skeleton): OspfRouter base
- T-06 (Neighbor FSM): state transitions triggered by Hello

## 8. Notes
- InactivityTimer = scheduleAt(simTime() + routerDeadInterval)
- Nếu không nhận Hello trong deadInterval → neighbor Down
