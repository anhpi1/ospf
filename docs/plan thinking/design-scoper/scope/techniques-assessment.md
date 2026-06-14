# B1: Technical Assessment

> Đánh giá kỹ thuật cho dự án mô phỏng OSPF routing trên OMNeT++ (cSimpleModule).
> Ngày: 2026-06-07

---

## 1. Phạm vi OSPF cần implement

Dựa trên RFC 2328, ta chọn **core OSPF protocol** cho single-area, bỏ qua các tính năng phức tạp:

| Thành phần | Bao gồm | Loại bỏ |
|---|---|---|
| **Packet types** | Hello, DD, LSR, LSU, LSAck | — |
| **LSA types** | Router-LSA (T1), Network-LSA (T2) | Summary-LSA (T3, T4), AS-External-LSA (T5) |
| **Areas** | Single Area 0 (backbone) | Multi-area, virtual links, stub areas, NSSA |
| **Network types** | Point-to-point, simple broadcast | NBMA, Point-to-MultiPoint |
| **Authentication** | — | Null auth (bỏ qua xác thực) |
| **TOS** | Metric đơn (cost) | TOS routing |
| **Multi-path** | ECMP (tùy chọn) | — |
| **IP model** | Router ID (simple addressing) | CIDR, VLSM, subnet masks phức tạp |

---

## 2. OSPF Core Components & OMNeT++ Translation

### 2.1 OSPF Packet Types (5 types)

| Packet | RFC Section | Vai trò | Kích thước OMNeT++ message |
|---|---|---|---|
| **Hello** (Type 1) | §9.5 | Neighbor discovery & keepalive | ~80 bytes |
| **Database Description (DD)** (Type 2) | §10.8 | Mô tả LSDB khi thiết lập adjacency | ~100-200 bytes |
| **Link State Request (LSR)** (Type 3) | §10.9 | Yêu cầu LSA cụ thể | ~40 bytes |
| **Link State Update (LSU)** (Type 4) | §13 | Chứa 1+ LSA để flood | biến đổi |
| **Link State Acknowledgment (LSACK)** (Type 5) | §13.5 | Xác nhận nhận LSA | ~40-60 bytes |

### 2.2 OSPF State Machines

**Interface State Machine** (§9.3):
- States: Down → Loopback → Waiting → Point-to-Point → DR → Backup → DR Other
- **OMNeT++**: Dùng FSM (`cFSM`) trong `cSimpleModule::handleMessage()`

**Neighbor State Machine** (§10.3):
- States: Down → Init → 2-Way → ExStart → Exchange → Loading → Full
- **OMNeT++**: Dùng FSM + self-messages (`scheduleAt()`) cho retransmission timers

### 2.3 Link-State Database (LSDB)

- Mỗi router duy trì LSDB riêng
- Lưu trữ: `vector<LSA>` hoặc `map<LSAKey, LSA>`
- Flooding: gửi LSU ra tất cả interface (trừ interface nhận)
- **OMNeT++**: Cấu trúc C++ thuần, không cần thư viện đặc biệt

### 2.4 SPF Algorithm (Dijkstra)

- Input: Graph từ LSDB (vertices = routers/networks, edges = links)
- Output: Shortest-path tree, routing table
- **OMNeT++**: Implement bằng C++ thuần (priority queue + adjacency list)

### 2.5 Timers & Events

| Timer | RFC | OMNeT++ Mechanism |
|---|---|---|
| HelloInterval | §9.5 | `scheduleAt(simTime() + interval, helloMsg)` |
| RouterDeadInterval | §10.5 | `scheduleAt()` trên mỗi neighbor |
| RxmtInterval | §10.8 | `scheduleAt()` cho retransmission |
| LSRefreshTime | §14 | `scheduleAt()` cho aging |
| SPFDelay | §16 | `scheduleAt()` sau khi topology thay đổi |

---

## 3. OMNeT++ Techniques

### 3.1 Cấu trúc module

```
Router (cSimpleModule)
├── handleMessage(cMessage *msg)
│   ├── if (msg->isSelfMessage()) → timer event
│   └── else → received packet
├── initialize()
│   ├── khởi tạo interface parameters
│   ├── scheduleAt() cho Hello timer
│   └── khởi tạo LSDB / routing table
└── finish()
    └── ghi kết quả thống kê
```

### 3.2 Message definition (.msg files)

```
packet OspfHello {
    int helloInterval;
    int deadInterval;
    int networkMask;        // simplified: router ID
    int neighborIDs[];      // list of neighbors seen
}
```

### 3.3 NED Topology

```
network SimpleOSPFNetwork {
    submodules:
        router1: Router;
        router2: Router;
        router3: Router;
    connections:
        router1.ppg[0] <--> {delay = 10ms;} <--> router2.ppg[0];
        router2.ppg[1] <--> {delay = 10ms;} <--> router3.ppg[0];
        // ...
}
```

### 3.4 Finite State Machine (cFSM)

Dùng `cFSM` trong OMNeT++ để implement state machines của OSPF:

```cpp
cFSM fsm;
#define FSM_...
#define FSM_Transition(...)

handleMessage() {
    FSM_Switch(fsm) {
        case FSM_Exit(INIT):
            // send Hello
            break;
        case FSM_Enter(2WAY):
            // if all neighbors known, decide to become adjacent
            break;
    }
}
```

---

## 4. Đánh giá Feasibility

| Technique | Độ phức tạp | Khả thi? | Ghi chú |
|---|---|---|---|
| OSPF packet definition (.msg) | Thấp | ✅ | 5 file .msg cho 5 packet types |
| Hello Protocol | Trung bình | ✅ | Timer-driven, gửi/nhận Hello |
| Neighbor State Machine | Trung bình | ✅ | ~8 states, cFSM phù hợp |
| Database Exchange (DD) | Cao | ⚠️ | Cần cẩn thận với sequencing |
| LSDB + Flooding | Trung bình | ✅ | STL containers + broadcast |
| SPF/Dijkstra | Thấp | ✅ | Thuật toán kinh điển |
| Routing Table | Thấp | ✅ | prefix → next-hop mapping |
| ECMP | Thấp | ✅ | Nhiều next-hop cho 1 destination |
| DR/BDR election (broadcast) | Cao | ⚠️ | Có thể simplify: point-to-point only |
| Retransmission | Trung bình | ✅ | Schedule + sequence numbers |

**Kết luận**: Rất khả thi. Core OSPF với ~2000-3000 dòng C++ là đủ.

---

## 5. Kiến trúc đề xuất

```
src/
├── ospf/
│   ├── OspfRouter.h/.cc      # main router module (cSimpleModule)
│   ├── OspfPacket.msg         # tất cả packet types
│   ├── OspfNeighbor.h/.cc     # neighbor state machine
│   ├── OspfInterface.h/.cc    # interface state machine
│   ├── OspfLsa.h/.cc          # LSA types
│   ├── OspfLsdb.h/.cc         # link-state database
│   ├── OspfRoutingTable.h/.cc # routing table
│   └── OspfSpf.h/.cc          # Dijkstra SPF calculation
├── network/
│   └── OspfNetwork.ned        # topology definitions
└── simulations/
    └── omnetpp.ini             # simulation config
```

---

## 6. Rủi ro & Giảm thiểu

| Rủi ro | Tác động | Giảm thiểu |
|---|---|---|
| Flooding có thể tạo broadcast storm | Trung bình | Sequence number + age + duplicate check |
| State machine phức tạp | Cao | Unit test từng state transition |
| Simulation chậm với nhiều router | Thấp | Vì là simulation routing thuần, không physical layer |
| Thứ tự event sai | Trung bình | Dùng cMessage với priority + simulation time |

---

*Tài liệu tham khảo: RFC 2328 §1-§16, OMNeT++ Simulation Manual (Simple Modules, Messages, NED)*
