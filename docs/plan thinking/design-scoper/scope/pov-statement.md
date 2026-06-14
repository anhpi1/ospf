# B2: Point of View (POV) Statement

> **POV**: Một người nghiên cứu/mô phỏng mạng cần một **mô phỏng OSPF routing tối giản nhưng đúng đắn** trên OMNeT++ (chỉ dùng cSimpleModule) bởi vì **bản đặc tả RFC 2328 quá đồ sộ và các framework có sẵn (INET) quá nặng** — không thể tập trung vào bản chất của routing.

---

## Chi tiết POV

**Người dùng:** Kỹ sư/nhà nghiên cứu mạng muốn:

1. **Hiểu sâu** cách OSPF hoạt động — từ message format đến state machine đến SPF algorithm
2. **Kiểm soát** từng khía cạnh của mô phỏng — không bị giới hạn bởi thư viện có sẵn
3. **Trực quan hóa** quá trình routing — thấy được gói tin đi đâu, routing table thay đổi thế nào
4. **Đánh giá** OSPF trong nhiều kịch bản — link fail, recovery, multi-failure

**Nhu cầu cốt lõi:**

| Nhu cầu | Giải thích |
|---|---|
| **Từ đầu** | Xây dựng messages → protocol → algorithm, không dùng thư viện có sẵn |
| **Tối giản** | Chỉ OSPF core — single area, P2P, bỏ authentication, TOS, areas phức tạp |
| **Có data traffic** | Client gửi gói tin qua network, router dùng OSPF routing table để forward |
| **ECMP** | Hỗ trợ multi-path khi có nhiều đường bằng cost |
| **Đa kịch bản** | 5 kịch bản: steady-state, leaf fail, backbone fail, recovery, multi-fail |
| **Logging** | 2 file log đặc biệt: packet transaction + message content |
| **GUI** | Qtenv để quan sát animation |

**Ràng buộc:**
- OMNeT++ 6.x, C++
- Chỉ `cSimpleModule` — không INET, không physical layer libraries
- ~10 router P2P, topology mesh có loop/leaf/cut-vertex
- Random link cost
- Đầu ra: routing table dump, convergence time, packet trace, log files

---

## Kiểm tra POV

| Tiêu chí | Kết quả |
|---|---|
| Không chứa giải pháp cụ thể? | ✅ Chỉ nêu nhu cầu và ràng buộc |
| Không chứa chỉ thị kỹ thuật? | ✅ Không nói "phải dùng Dijkstra" hay "phải code thế nào" |
| Phản ánh đúng người dùng? | ✅ Người muốn hiểu routing từ gốc |
| Đủ hẹp để actionable? | ✅ OSPF core simulation trong OMNeT++ với scope rõ ràng |

---

*Tài liệu tham khảo: RFC 2328, OMNeT++ Manual, Technical Assessment*
