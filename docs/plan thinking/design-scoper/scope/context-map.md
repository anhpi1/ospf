# B5: Context Map

> Bản đồ bối cảnh dự án — phân tích 3 chiều: Users, Technology, Business.
> Ngày: 2026-06-07

---

## 1. Users Dimension

### 1.1 Người dùng

| Vai trò | Mô tả | Mức độ tương tác |
|---|---|---|
| **Developer (bạn)** | Người xây dựng và chạy mô phỏng | Toàn bộ — thiết kế, code, debug, phân tích |
| **Người học/research** | Ai đó dùng lại code để học OSPF (secondary) | Đọc code, chạy scenario, xem log |

### 1.2 Goals & Needs

| Goal | Nhu cầu tương ứng |
|---|---|
| Hiểu OSPF từ message → protocol → algorithm | Code rõ ràng, có comment, có log |
| Build routing simulation từ đầu | Không dùng INET, chỉ cSimpleModule |
| Test OSPF trong nhiều kịch bản | 5 kịch bản với topology change |
| Quan sát quá trình routing | Qtenv GUI + log files |
| Có thể tái cấu hình dễ dàng | Tham số trong omnetpp.ini |

### 1.3 Journey Map

```
┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
│   Đọc    │ →  │  Build   │ →  │   Run    │ →  │  Debug   │ →  │ Analyze  │
│  Design  │    │ Modules  │    │ Scenario │    │ & Fix    │    │ Results  │
│  Brief   │    │  + Code  │    │ (Qtenv)  │    │ Routing  │    │ (Logs)   │
└──────────┘    └──────────┘    └──────────┘    └──────────┘    └──────────┘
     ↓              ↓              ↓              ↓              ↓
 Design brief   Code struct   See packets   Check states   Routing dump
 specs         .msg, .ned    in animation  Fix bugs       Converge time
```

### 1.4 Touchpoints

| Touchpoint | Mục đích | Kỳ vọng |
|---|---|---|
| **Qtenv GUI** | Quan sát gói tin di chuyển | Animation rõ ràng giữa các router |
| **Terminal output** | Xem routing table dump | Dạng bảng, dễ đọc |
| **Transaction log** | Trace từng hành động | Mỗi action có mã số, rõ gửi/nhận |
| **Message content log** | Nội dung từng gói tin | Các field quan trọng |
| **Convergence time** | Đo thời gian hội tụ | Từ lúc topology change đến khi routing table ổn định |

---

## 2. Technology Dimension

### 2.1 Hệ thống

```
                    ┌─────────────────────┐
                    │   OMNeT++ 6.4.0     │
                    │  (Simulation Kernel)│
                    └──────────┬──────────┘
                               │
        ┌──────────────────────┼──────────────────────┐
        │                      │                      │
        ▼                      ▼                      ▼
┌──────────────┐   ┌──────────────┐   ┌──────────────────┐
│   NED Lang   │   │  .msg Files  │   │  C++ Modules     │
│ (Topology)   │   │ (Packets)    │   │  (cSimpleModule) │
│ OspfNet.ned  │   │ OspfPkt.msg  │   │  OspfRouter.cc   │
└──────────────┘   └──────────────┘   └──────────────────┘
        │                                      │
        ▼                                      ▼
┌──────────────┐                    ┌──────────────────┐
│ omnetpp.ini  │                    │  C++ STL          │
│ (Cấu hình)   │                    │  (vector, map)    │
│ Timer/cost   │                    │  + cFSM            │
└──────────────┘                    └──────────────────┘
```

### 2.2 Platform

| Layer | Công nghệ | Ghi chú |
|---|---|---|
| **Simulation framework** | OMNeT++ 6.4.0 | Discrete event simulation |
| **Module base** | `cSimpleModule` | Không dùng `cCompoundModule` phức tạp |
| **Events** | `cMessage`, `scheduleAt()` | Timer cho Hello, Dead, Retransmit |
| **FSM** | `cFSM` | Interface + Neighbor state machines |
| **Packets** | `cPacket` via `.msg` | OSPF packets kế thừa từ cPacket |
| **Topology** | NED language | P2P connections |
| **GUI** | Qtenv | Animation, module inspector |
| **Build** | `opp_makemake` + Makefile | OMNeT++ build system |
| **Data structures** | C++ STL | `std::map` cho LSDB, `std::vector` cho routing table |

### 2.3 Kết nối giữa các thành phần

```
cMessage (generic) ← cPacket (data + bit length)
    ↑
OspfPacket (msg definition)
    ├── OspfHello
    ├── OspfDatabaseDescription
    ├── OspfLinkStateRequest
    ├── OspfLinkStateUpdate
    └── OspfLinkStateAck

OspfRouter (cSimpleModule)
    ├── handleMessage() → dispatch
    │   ├── self-message (timer)
    │   ├── OSPF packet → processOSPF()
    │   └── Data packet → forwardData()
    ├── scheduleAt() → future events
    └── send() → gate → link → neighbor
```

---

## 3. Business Dimension

### 3.1 Constraints

| Ràng buộc | Mức độ | Tác động |
|---|---|---|
| Chỉ cSimpleModule | Cứng | Không dùng INET, tự build mọi thứ |
| OMNeT++ 6.x | Cứng | Phải tương thích version |
| C++ | Cứng | Không dùng Python/Java |
| Simulation time | Mềm | Cần tối ưu để chạy nhanh (5-10 kịch bản) |
| Code maintainability | Mềm | Code phải clear để học OSPF từ code |

### 3.2 Uncertainties

| Uncertainty | Risk | Mitigation |
|---|---|---|
| OSPF flooding có thể gây trùng lặp tin | Thấp | Sequence number + age checking |
| Thứ tự packet simulation | Thấp | OMNeT++ event queue xử lý |
| SPF performance với 10 router | Rất thấp | 10 router là nhỏ |
| Làm quen OMNeT++ API | Trung bình | Tham khảo Simulation Manual |

### 3.3 Dependencies

```
OSPF Simulation
├── OMNeT++ 6.4.0 (phải cài đặt sẵn)
│   ├── opp_makemake
│   ├── Qtenv
│   └── Simulation Library (liboppsim)
├── C++ compiler (g++/clang)
└── make
```

---

## 4. Open Questions (đã giải quyết)

Tất cả các câu hỏi đã được trả lời trong Q1-Q16. Không còn open questions cho Phase 1.

| ID | Câu hỏi | Status |
|---|---|---|
| Q1 | Network topology type | ✅ P2P |
| Q2 | Network scale | ✅ 10 router |
| Q3 | Link cost | ✅ Random 1-30 |
| Q4 | ECMP | ✅ Có |
| Q5 | Simulation output | ✅ 6 loại |
| Q6 | Interface | ✅ Qtenv GUI |
| Q7 | Topology | ✅ Mesh đã duyệt |
| Q8 | Data traffic | ✅ Có client đơn giản |
| Q9 | Scenarios | ✅ 5 kịch bản |
| Q10 | Cost range | ✅ 1-30 |
| Q11 | Client pattern | ✅ 1 lần, ~100B |
| Q12 | Router ID | ✅ IP giả |
| Q13 | OSPF timers | ✅ Rút gọn 10× |
| Q14 | Link delay | ✅ 10ms |
| Q15 | Timeline | ✅ 5 timeline |
| Q16 | Traffic pair | ✅ R6→R10 |

---

*Tài liệu tham khảo: Design Brief, Technical Assessment*
