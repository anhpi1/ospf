# B3: How Might We (HMW) Questions

> Từ POV statement, đặt các câu hỏi "Làm thế nào để..." khai phá hướng giải pháp.
> Mỗi HMW được kiểm tra: Seed? Không quá hẹp? Không quá rộng? Cân bằng?

---

## 1. Kiến trúc Module

### HMW-01: Thiết kế router module
> **HMW** thiết kế một router module trên OMNeT++ chỉ dùng `cSimpleModule` mà vẫn đủ linh hoạt cho OSPF?

- **Seed:** OSPF router = cSimpleModule
- **Không quá hẹp:** ✅ Không gò vào 1 design pattern cụ thể
- **Không quá rộng:** ✅ Gắn với OMNeT++ và cSimpleModule
- **Cân bằng:** ✅

### HMW-02: Tổ chức state machines
> **HMW** tổ chức các state machines (interface, neighbor, database exchange) trong handleMessage()?

- **Seed:** cFSM
- **Không quá hẹp:** ✅ Có thể dùng cFSM, switch/case, hoặc state pattern
- **Không quá rộng:** ✅ Chỉ trong phạm vi 1 router module
- **Cân bằng:** ✅

---

## 2. OSPF Protocol

### HMW-03: Định nghĩa OSPF messages
> **HMW** định nghĩa 5 loại OSPF packets (Hello, DD, LSR, LSU, LSAck) bằng .msg files sao cho vừa đủ cho routing?

- **Seed:** .msg file definitions
- **Không quá hẹp:** ✅ Có thể chọn field nào cần, field nào bỏ
- **Không quá rộng:** ✅ Chỉ 5 packet types
- **Cân bằng:** ✅

### HMW-04: Neighbor discovery
> **HMW** implement Hello protocol cho neighbor discovery/maintenance trên P2P link?

- **Seed:** Hello packets + timers
- **Không quá hẹp:** ✅ Không gò vào cách handle timeout
- **Không quá rộng:** ✅ Chỉ P2P, không broadcast
- **Cân bằng:** ✅

### HMW-05: Database exchange
> **HMW** implement quá trình database exchange (DD/exchange/loading) giữa 2 routers?

- **Seed:** DD/Request/Update/Ack handshake
- **Không quá hẹp:** ✅ Có thể chọn master/slave hoặc simplified
- **Không quá rộng:** ✅ Chỉ trong scope single-area
- **Cân bằng:** ✅

### HMW-06: Flooding LSAs
> **HMW** flood LSA updates qua mạng P2P một cách hiệu quả?

- **Seed:** Flooding ra tất cả interface trừ interface nhận
- **Không quá hẹp:** ✅ Không định nghĩa cấu trúc dữ liệu cụ thể
- **Không quá rộng:** ✅ Chỉ trong mạng P2P
- **Cân bằng:** ✅

---

## 3. SPF & Routing

### HMW-07: Tính SPF tree
> **HMW** implement Dijkstra SPF từ LSDB để xây shortest-path tree?

- **Seed:** Dijkstra's algorithm
- **Không quá hẹp:** ✅ Có thể chọn priority queue hoặc array-based
- **Không quá rộng:** ✅ Chỉ single-area SPF
- **Cân bằng:** ✅

### HMW-08: Routing table với ECMP
> **HMW** xây dựng routing table hỗ trợ ECMP (nhiều next-hop cho 1 destination)?

- **Seed:** Routing table với danh sách next-hop
- **Không quá hẹp:** ✅ Có thể chọn cấu trúc lưu trữ khác nhau
- **Không quá rộng:** ✅ ECMP là 1 tính năng rõ ràng
- **Cân bằng:** ✅

---

## 4. Data Traffic

### HMW-09: Data packet forwarding
> **HMW** implement data packet forwarding dùng OSPF routing table?

- **Seed:** Client → Router → forward → Router → Client
- **Không quá hẹp:** ✅ Có thể chọn cách gửi (send, sendDelayed, v.v.)
- **Không quá rộng:** ✅ Chỉ forward cơ bản, không QoS
- **Cân bằng:** ✅

---

## 5. Logging & Visualization

### HMW-10: Logging cấu trúc
> **HMW** ghi log có cấu trúc (transaction log + message content) không làm chậm simulation?

- **Seed:** Ghi file với format có cấu trúc
- **Không quá hẹp:** ✅ Có thể chọn output format
- **Không quá rộng:** ✅ Chỉ 2 file log cụ thể
- **Cân bằng:** ✅

---

## Nhóm HMW theo theme

| Theme | HMW IDs |
|---|---|
| **Module Architecture** | HMW-01, HMW-02 |
| **OSPF Protocol** | HMW-03, HMW-04, HMW-05, HMW-06 |
| **SPF & Routing** | HMW-07, HMW-08 |
| **Data Plane** | HMW-09 |
| **Observability** | HMW-10 |

---

*Tài liệu tham khảo: POV Statement, Technical Assessment*
