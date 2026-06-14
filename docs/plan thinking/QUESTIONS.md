# Question History

> Master question tracker for the project. Questions are added dynamically — one question can spawn child questions when answers are unclear.
> Auto-generated when Phase 1 starts.

---
## Rules
- `[x]` = answered
- `[-]` = in progress (has sub-questions pending)
- `[ ]` = unanswered
- **Dynamic:** When an answer is unclear or incomplete → add a child question (indent 1 tab under parent). Label source: *(from answer)*, *(from challenge)*, *(from analysis)*.
- Sub-questions: indent 1 tab under parent
- Blocker questions: prefix with `**ID:**`
- Each question has: ID, problem statement, status, answer

---

## Q1: Network topology type
- **Problem:** Loại network topology nào cho các kết nối giữa các router?
- **Answer:** Chỉ Point-to-Point (P2P) — mỗi link nối trực tiếp 2 router. Không cần DR/BDR election.
- **Status:** `[x]`
- **Source:** User request

## Q2: Network scale
- **Problem:** Số lượng router trong mô phỏng?
- **Answer:** ~10 router.
- **Status:** `[x]`
- **Source:** User request

## Q3: Link cost
- **Problem:** Giá trị link cost giữa các router?
- **Answer:** Random cost (giá trị khác nhau giữa các link).
- **Status:** `[x]`
- **Source:** User request (đổi từ cost=1 → random)

## Q4: ECMP support
- **Problem:** Có hỗ trợ Equal-Cost Multi-Path không?
- **Answer:** Có — routing table giữ nhiều next-hop cho 1 destination nếu có nhiều đường cùng cost.
- **Status:** `[x]`
- **Source:** User request

## Q5: Simulation output
- **Problem:** Những gì cần xuất ra từ mô phỏng?
- **Answer:** 
  - Routing table dump
  - Packet trace
  - Convergence time
  - Topology change test
  - **Log file 1:** Packet transaction log — mỗi hành động có mã số
  - **Log file 2:** Message content log
- **Status:** `[x]`
- **Source:** User request

## Q6: Simulation interface
- **Problem:** Chạy mô phỏng với giao diện nào?
- **Answer:** Có GUI (Qtenv) — để thấy animation gói tin và debug trực quan.
- **Status:** `[x]`
- **Source:** User request

## Q7: Topology
- **Problem:** Cấu trúc kết nối giữa 10 router?
- **Answer:** Mesh P2P do tôi đề xuất — cover ECMP, loop, leaf, cut-vertex, backup path.
- **Status:** `[x]`
- **Source:** User request

## Q8: Data traffic
- **Problem:** Có mô phỏng data traffic ngoài gói OSPF không?
- **Answer:** Có — đơn giản: mỗi mạng có 1 client gửi gói tin cho nhau. Router dùng routing table OSPF để forward.
- **Status:** `[x]`
- **Source:** User request

## Q9: Topology change scenarios
- **Problem:** Cần các kịch bản thay đổi topology để đánh giá hoạt động mạng?
- **Answer:** Có. Các kịch bản:
  - S1: Steady-state (baseline)
  - S2: Link failure leaf (R6-R4)
  - S3: Link failure backbone (R2-R5)
  - S4: Link recovery (phục hồi link S3)
  - S5: Multiple failures (R1-R2 + R4-R5)
- **Status:** `[x]`
- **Source:** User request

## Q10: Random cost range
- **Problem:** Khoảng giá trị cho random link cost?
- **Answer:** 1 — 30.
- **Status:** `[x]`
- **Source:** User request

## Q11: Client traffic pattern
- **Problem:** Client gửi data packet tần suất và kích thước thế nào?
- **Answer:** Gửi 1 lần (không định kỳ). Kích thước nhỏ (~100 bytes). Làm sau cùng cho có — mục đích chính là routing.
- **Status:** `[x]`
- **Source:** User request

## Q12: Router ID format
- **Problem:** Router dùng định danh kiểu gì?
- **Answer:** IP address giả (ví dụ 10.0.0.1, 10.0.0.2,...).
- **Status:** `[x]`
- **Source:** User request

## Q13: OSPF timers
- **Problem:** Rút ngắn timer để simulation chạy nhanh hơn?
- **Answer:** Có — rút gọn 10 lần so với chuẩn:
  - HelloInterval: 10s → 1s
  - RouterDeadInterval: 40s → 4s
  - RxmtInterval: 5s → 0.5s
  - LSRefreshTime: 1800s → 180s
  - SPF Delay: 5s → 0.5s
  Tất cả đặt trong file cấu hình (omnetpp.ini hoặc par định nghĩa trong .ned).
- **Status:** `[x]`
- **Source:** User request

## Q14: Link delay
- **Problem:** Propagation delay trên mỗi link P2P?
- **Answer:** Cố định, 10ms.
- **Status:** `[x]`
- **Source:** User request

## Q15: Scenario timeline
- **Problem:** Thời điểm cụ thể cho các sự kiện trong từng kịch bản?
- **Answer:** 
  - S1: steady-state, không sự kiện
  - S2: T=20s (đứt R6-R4), T=40s (phục hồi)
  - S3: T=20s (đứt R2-R5)
  - S4: T=50s (phục hồi R2-R5)
  - S5: T=20s (đứt R1-R2 + R4-R5)
- **Status:** `[x]`
- **Source:** User request

## Q16: Client traffic pair
- **Problem:** Client nào gửi data packet cho client nào?
- **Answer:** R6 → R10 (leaf→leaf, đường xa nhất).
- **Status:** `[x]`
- **Source:** User request**
- **Source:** User request
