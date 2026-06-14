# Sketch 2: Problem Map

> Bản đồ vấn đề — các thách thức và giải pháp tương ứng cho dự án OSPF routing simulation.

---

## Problem Tree

```
                    ╔══════════════════════════════════╗
                    ║   Mô phỏng OSPF routing trên     ║
                    ║   OMNeT++ (cSimpleModule only)   ║
                    ╚══════════════════════════════════╝
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
        ▼                     ▼                     ▼
┌───────────────┐   ┌───────────────┐   ┌───────────────┐
│  OSPF Protocol │   │  Simulation   │   │  Verification │
│  Complexity    │   │  Correctness  │   │  & Output     │
└───────┬───────┘   └───────┬───────┘   └───────┬───────┘
        │                     │                     │
        ▼                     ▼                     ▼
┌───────────────┐   ┌───────────────┐   ┌───────────────┐
│  P1: Packet   │   │  P6: State   │   │  P9: Log      │
│  format       │   │  machines    │   │  formatting   │
├───────────────┤   ├───────────────┤   ├───────────────┤
│  P2: Hello +  │   │  P7: Event   │   │  P10: Routing │
│  Neighbor FSM │   │  ordering    │   │  table dump   │
├───────────────┤   ├───────────────┤   ├───────────────┤
│  P3: Database │   │  P8: Link    │   │  P11: Conver- │
│  Exchange     │   │  failure/re- │   │  gence time   │
├───────────────┤   │  covery sim  │   │  measurement  │
│  P4: LSDB +   │   └───────────────┘   └───────────────┘
│  Flooding     │
├───────────────┤
│  P5: SPF +    │
│  Routing Tbl  │
└───────────────┘
```

---

## Problem & Solution

### OSPF Protocol Complexity

| ID | Problem | Solution | Độ khó |
|---|---|---|---|
| **P1** | 5 OSPF packet types với nhiều trường, cần mapping .msg | Chỉ lấy các trường cần thiết (OSPF header, LSA header, neighbor list...) | Thấp |
| **P2** | Hello protocol: neighbor discovery, timer management, state transitions (Down→Init→2Way→...) | cFSM + scheduleAt() cho Hello timer, timeout = RouterDeadInterval | Trung bình |
| **P3** | Database Exchange: master/slave, DD sequence, LSR/LSU/LSAck handshake | Implement tuần tự: DD exchange → LSR → LSU → ACK | Cao |
| **P4** | LSDB lưu trữ + flooding + duplicate detection | `std::map<LSAKey, LSA>` + sequence number + age | Trung bình |
| **P5** | Dijkstra SPF với 10 router đồ thị có loop + ECMP | Priority queue + predecessor list cho ECMP | Trung bình |

### Simulation Correctness

| ID | Problem | Solution | Độ khó |
|---|---|---|---|
| **P6** | 2 state machines (interface + neighbor) cần quản lý | Gom vào OspfRouter::handleMessage(), dispatch theo message type | Trung bình |
| **P7** | Thứ tự event (hello timeout, dead timeout, retransmit) có thể race | Dùng simulation time + priority, OMNeT++ guarantees correct ordering | Thấp |
| **P8** | Link failure/recovery simulation | Dùng `cModule::modParameter()` hoặc disconnect/reconnect gates trong omnetpp.ini scenario | Trung bình |

### Verification & Output

| ID | Problem | Solution | Độ khó |
|---|---|---|---|
| **P9** | Log phải có cấu trúc, mỗi action có mã số | Transaction ID counter global + format: `[TxID] TIME SRC→DST TYPE DATA` | Thấp |
| **P10** | Routing table dump dạng dễ đọc | EV << "dest\tnextHop\tcost\n" | Thấp |
| **P11** | Đo convergence time | Marker time khi topology change, check khi routing table ổn định | Trung bình |

---

## Dependency giữa các Problem

```
P1 (messages) ──→ P2 (Hello) ──→ P3 (DB exchange) ──→ P4 (LSDB) ──→ P5 (SPF)
                     │                                      │              │
                     │                                      │              ▼
                     └──→ P6 (state machines) ──────────────┘         Routing Table
                                                                          │
                                                                    P10 (dump)
                                                                          │
                                                                    P11 (convergence)
```

