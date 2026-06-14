# B4: Design Brief

> Bản thiết kế tổng thể — tổng hợp từ POV, HMW questions, và Technical Assessment.
> Ngày: 2026-06-07

---

## 1. Goal

Xây dựng một **mô phỏng OSPF routing** trên OMNeT++ 6.x **từ đầu** (chỉ dùng `cSimpleModule`, không INET, không thư viện vật lý) để:
- Hiểu và trực quan hóa cơ chế hoạt động của OSPF link-state routing
- Đánh giá hành vi hội tụ trong nhiều kịch bản mạng
- Có khả năng mở rộng để thử nghiệm các thuật toán định tuyến khác

---

## 2. In Scope

### OSPF Protocol Core
- 5 OSPF packet types: **Hello**, **Database Description (DD)**, **Link State Request (LSR)**, **Link State Update (LSU)**, **Link State Acknowledgment (LSAck)**
- **Neighbor discovery** qua Hello protocol + neighbor state machine (Down→Init→2Way→ExStart→Exchange→Loading→Full)
- **Database exchange** (master/slave negotiation + DD packets)
- **LSDB** — quản lý Router-LSAs (Type 1) và Network-LSAs (Type 2)
- **Flooding** LSAs qua mạng P2P
- **Dijkstra SPF** — tính shortest-path tree từ LSDB
- **Routing table** với ECMP support

### Network Model
- Point-to-point links giữa các router (không broadcast, không NBMA)
- **10 router** trong single area (Area 0)
- Random link cost
- Topology mesh bao gồm: loops, cut-vertices, leaf routers, equal-cost paths

### Data Traffic
- Client module gửi data packets
- Router forward data packets theo OSPF routing table

### Simulation
- Qtenv GUI
- 5 kịch bản: S1 (steady-state), S2 (leaf fail), S3 (backbone fail), S4 (recovery), S5 (multi-fail)

### Output
- Routing table dump
- Packet trace (OSPF + data)
- Convergence time measurement
- **Log file 1:** Packet transaction log (mã số cho mỗi hành động gửi/nhận)
- **Log file 2:** Message content log

---

## 3. Out of Scope

| Tính năng | Lý do loại bỏ |
|---|---|
| Multi-area (OSPF areas) | Single area 0 là đủ cho routing simulation |
| Virtual links | Chỉ cần khi có multi-area |
| Authentication (null/simple/crypto) | Không cần cho simulation thuần routing |
| TOS routing | OSPF TOS không được dùng phổ biến |
| AS-external LSAs (Type 5) | Không có external routes trong simulation |
| Summary-LSAs (Type 3, 4) | Không có inter-area routing |
| NBMA / Point-to-MultiPoint | Chỉ P2P |
| DR/BDR election | Chỉ P2P, không cần DR |
| Variable Length Subnetting / CIDR | Dùng Router ID đơn giản |
| LSA flooding rate-limiting | Mạng nhỏ, không cần |
| IP fragmentation | Gói tin nhỏ, không cần |
| QoS / Traffic engineering | Ngoài scope |
| INET Framework / Physical layer | Mục tiêu là cSimpleModule thuần |

---

## 4. Solution Approach

### Kiến trúc tổng quan

```
┌─────────────────────────────────────────────────────────┐
│                    10 Router Network                     │
│                                                          │
│   R1 ─── R2 ─── R3 ─── R8                               │
│   │   / │       │       │                               │
│   │  /  │       │       │                               │
│   │ /   │       │       │                               │
│   R4 ─── R5 ─── R9      R10                             │
│   │       │                                             │
│   │       │                                             │
│   R6      R7                                            │
│                                                          │
│   Mỗi router: [Client] → [OspfRouter]                   │
└─────────────────────────────────────────────────────────┘
```

### Mỗi Router gồm:

```
┌─────────────────────────────┐
│         Router (compound)    │
│  ┌───────────────────────┐  │
│  │   Client (simple)     │  │  ← gửi/nhận data packets
│  └──────────┬────────────┘  │
│             │               │
│  ┌──────────▼────────────┐  │
│  │ OspfRouter (simple)   │  │  ← OSPF protocol + routing
│  │ ┌──────────────────┐  │  │
│  │ │ State Machines   │  │  │
│  │ │ Hello Timer      │  │  │
│  │ │ Neighbor FSM     │  │  │
│  │ │ LSDB             │  │  │
│  │ │ SPF (Dijkstra)   │  │  │
│  │ │ Routing Table    │  │  │
│  │ │ Data Forwarder   │  │  │
│  │ └──────────────────┘  │  │
│  └────────────────────────┘  │
└─────────────────────────────┘
```

### Implementation Sequence (dự kiến)

| Bước | Module | Mô tả |
|---|---|---|
| 1 | OSPF Messages (.msg) | Định nghĩa 5 packet types trong .msg files |
| 2 | OSPF Router skeleton | cSimpleModule với handleMessage + gates |
| 3 | Hello Protocol | Gửi/nhận Hello, neighbor discovery |
| 4 | Neighbor State Machine | 8 states P2P |
| 5 | Database Exchange | DD/LSR/LSU/LSAck handshake |
| 6 | LSDB + Flooding | Lưu trữ + flood Router-LSAs |
| 7 | SPF (Dijkstra) | Tính shortest-path tree |
| 8 | Routing Table + ECMP | Xây routing table từ SPF |
| 9 | Data Forwarding | Client gửi data → router forward |
| 10 | Logging | Transaction log + message content log |
| 11 | Scenarios | 5 kịch bản mô phỏng |
| 12 | GUI + Output | Qtenv + routing dump + convergence time |

---

## 5. Success Criteria

| # | Tiêu chí | Cách kiểm tra |
|---|---|---|
| SC-01 | 10 router P2P gửi/nhận Hello thành công | Log neighbor states → Full |
| SC-02 | LSDB đồng bộ giữa tất cả router | So sánh LSDB content |
| SC-03 | SPF tính đúng shortest-path tree | Kiểm tra routing table manual |
| SC-04 | ECMP hoạt động (nếu có equal-cost paths) | Routing table có ≥2 next-hop |
| SC-05 | Data packet đi từ source đến destination | Packet trace log |
| SC-06 | Link failure → OSPF hội tụ lại | Convergence time < expected |
| SC-07 | Link recovery → OSPF phục hồi | Routing table khớp với steady-state |
| SC-08 | 2 file log có format đúng yêu cầu | Transaction log + content log |

---

## 6. Coverage Check

| Yêu cầu từ POV | Coverage trong Design Brief |
|---|---|
| cSimpleModule only | ✅ Solution approach, mục 4 |
| OSPF core protocol | ✅ In Scope, mục 2 |
| P2P topology | ✅ In Scope, mục 2 |
| Data traffic (client) | ✅ Solution approach, mục 4 |
| ECMP | ✅ In Scope + Success Criteria |
| 5 scenarios | ✅ In Scope, mục 2 |
| 2 file log | ✅ In Scope, mục 2 |
| Qtenv GUI | ✅ In Scope, mục 2 |

---

## 7. Placeholder Check

| Hạng mục | Trạng thái |
|---|---|
| Link cost range cho random | ✅ 1-30 |
| Router ID format | ✅ IP giả (10.0.0.x) |
| Link delay | ✅ 10ms cố định |
| OSPF timers | ✅ Rút gọn 10× (Hello 1s, Dead 4s,...), configurable |
| Topology mesh | ✅ Đã phê duyệt |
| Scenario timeline | ✅ 5 kịch bản với timeline cụ thể |
| Client traffic pair | ✅ R6 → R10, 1 lần, ~100 bytes |
| Convergence time threshold | ✅ Sẽ đo từ simulation |

> Các placeholders trên sẽ được giải quyết trong Phase 2 (Task Decomposition) và Phase 4 (Spec Writing).

---

*Tài liệu tham khảo: Technical Assessment, POV Statement, HMW Questions*
