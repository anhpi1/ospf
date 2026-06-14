# Task Spec: T-06 — Neighbor State Machine

> Implement OSPF Neighbor FSM với cFSM.

---

## 1. Overview
State machine quản lý trạng thái quan hệ với mỗi neighbor: Down→Init→2Way→ExStart→Exchange→Loading→Full.

## 2. Requirements
- Mỗi neighbor có 1 state instance
- cFSM transitions theo OSPF events
- P2P simplified: bỏ qua Negotiation, tự động ExStart sau 2Way

## 3. Input
- NeighborState từ T-05 (Down/Init/2Way)
- OMNeT++ cFSM class

## 4. Process

**State enum:**
```cpp
enum NeighborState {
    DOWN = 0,
    INIT = 1,
    TWOWAY = 2,
    EXSTART = 3,
    EXCHANGE = 4,
    LOADING = 5,
    FULL = 6
};
```

**NeighborData:**
```cpp
struct NeighborData {
    int routerID;
    NeighborState state;
    cFSM fsm;
    cMessage* inactivityTimer;
    // Database exchange
    bool isMaster;
    int ddSequenceNumber;
    std::vector<int> requestedLSAs;  // for Loading
};
```

**Transition logic (handleMessage dispatch):**
```cpp
void OspfRouter::updateNeighborFSM(int nbrID, OspfEvent event) {
    auto& nbr = neighbors[nbrID];
    FSM_Switch(nbr.fsm) {
        case FSM_Exit(DOWN):
            if (event == HELLO_RECEIVED) FSM_Goto(nbr.fsm, INIT);
            break;
        case FSM_Exit(INIT):
            if (event == TWOWAY_RECEIVED) FSM_Goto(nbr.fsm, TWOWAY);
            if (event == ONE_WAY) FSM_Goto(nbr.fsm, INIT);
            break;
        case FSM_Exit(TWOWAY):
            if (event == ADJACENCY_OK) FSM_Goto(nbr.fsm, EXSTART);
            break;
        case FSM_Exit(EXSTART):
            if (event == NEGOTIATION_DONE) FSM_Goto(nbr.fsm, EXCHANGE);
            if (event == SEQ_NUM_MISMATCH) FSM_Goto(nbr.fsm, EXSTART);
            break;
        case FSM_Exit(EXCHANGE):
            if (event == EXCHANGE_DONE) FSM_Goto(nbr.fsm, LOADING);
            if (event == SEQ_NUM_MISMATCH) FSM_Goto(nbr.fsm, EXSTART);
            break;
        case FSM_Exit(LOADING):
            if (event == LOADING_DONE) FSM_Goto(nbr.fsm, FULL);
            if (event == SEQ_NUM_MISMATCH) FSM_Goto(nbr.fsm, EXSTART);
            break;
        case FSM_Exit(FULL):
            if (event == INACTIVITY_TIMER) FSM_Goto(nbr.fsm, DOWN);
            break;
    }
}
```

**Events:**
```cpp
enum OspfEvent {
    HELLO_RECEIVED,
    TWOWAY_RECEIVED,    // bidirectional Hello
    ONE_WAY,            // unidirectional
    ADJACENCY_OK,       // P2P: automatically after 2Way
    NEGOTIATION_DONE,   // DD master/slave settled
    EXCHANGE_DONE,      // all DD exchanged
    LOADING_DONE,       // all LSAs received
    SEQ_NUM_MISMATCH,
    INACTIVITY_TIMER,
    KILL_NBR
};
```

## 5. Output
- Neighbor state transitions (loggable)
- Event dispatch cho T-07a (DD Exchange)

## 6. Acceptance Criteria
- 2 routers: Down → Init → 2Way → ExStart → Exchange → Loading → Full
- InactivityTimer: không Hello 4s → Down
- Có thể log state transitions

## 7. Related Tasks
- T-05 (Hello): cung cấp HELLO_RECEIVED event
- T-07a (DD): cung cấp NEGOTIATION_DONE, EXCHANGE_DONE events
- T-07b (LSR/LSU): cung cấp LOADING_DONE event
- T-09 (Flooding): yêu cầu neighbor ở trạng thái Full

## 8. Notes
- Trên P2P: 2Way → ExStart tự động (AdjacencyOK)
- cFSM tự động gọi FSM_Enter/FSM_Exit
- Dùng simtime_t cho inactivity timer chính xác
