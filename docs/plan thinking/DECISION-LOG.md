# Decision History

> Records every decision made during the project — rationale, source, and consequences.

---
## [2026-06-07] — Q&A Session Summary (Phase 1, B2)
- **Decision:** Specs xác định cho mô phỏng OSPF routing
  - Network: P2P-only, ~10 router, mesh topology
  - Link cost: random 1-30
  - ECMP: có hỗ trợ
  - Data traffic: có (client gửi 1 lần, ~100 bytes)
  - Simulation: Qtenv GUI, 5 kịch bản (steady, leaf fail, backbone fail, recovery, multi-fail)
  - Output: routing table dump, packet trace, convergence time, 2 file log (transaction + content)
- **Rationale:** Tập trung vào core routing, bỏ tính năng phức tạp không cần thiết
- **Source:** Q1-Q9 từ QUESTIONS.md
- **Consequences:** Implementation scope rõ ràng, không dùng INET, chỉ cSimpleModule

## [2026-06-07] — Chi tiết kỹ thuật bổ sung (Q10-Q13)
- **Decision:** 
  - Router ID: IP giả (10.0.0.x)
  - OSPF timers: rút gọn 10× chuẩn, configurable trong omnetpp.ini
  - Topology: mesh đã phê duyệt (R1-R10)
- **Rationale:** Tối ưu cho simulation routing, không cần đúng chuẩn thời gian thực
- **Source:** Q10-Q13 từ QUESTIONS.md
- **Consequences:** Timer configurable, dễ điều chỉnh sau

## [2026-06-07] — Chi tiết mô phỏng (Q14-Q16)
- **Decision:**
  - Link delay: 10ms cố định
  - Scenario timeline: 5 kịch bản (mỗi kịch bản 1 timeline riêng)
  - Client traffic: R6 → R10, 1 lần ~100 bytes
- **Rationale:** Đủ coverage cho routing test, không thừa
- **Source:** Q14-Q16 từ QUESTIONS.md
- **Consequences:** Topology + timeline + traffic đã chốt

## [2026-06-07] — Phase 2: Task Decomposition
- **Decision:** 20 tasks được phân rã từ kiến trúc thiết kế
  - Infrastructure: T-01→T-03 (setup, NED, .msg)
  - Protocol Core: T-04→T-10b (skeleton→routing table)
  - Data Plane: T-11→T-12 (client, forwarding)
  - Observability: T-13→T-14 (logger, dumps)
  - Simulation: T-15→T-17 (ini, scenarios, integration)
- **Rationale:** Mỗi task độc lập, dễ implement và verify riêng
- **Source:** Architecture Design (Phase 1 components + context map)
- **Consequences:** Cơ sở cho spec writing và implementation ordering

## [2026-06-07] — Phase 3: Logic Flow
- **Decision:** Execution order 7 phases A→G, parallel groups G1-G5
  - File structure: src/ + simulations/ + results/
  - Data flow: In-memory + OMNeT++ send() + File system channels
- **Rationale:** Tối ưu dependency, cho phép parallel development
- **Source:** Task dependencies từ Phase 2
- **Consequences:** Implementation có thể tiến hành theo phases

## [2026-06-07] — Phase 4: Spec Writing + Cross-Check
- **Decision:** 17 spec files written (1 per task)
  - Cross-check: 22/22 Design Brief coverage ✅, 18/18 IO chains ✅
  - 1 minor supplement: T-02b (local gates)
- **Rationale:** Đảm bảo implementation đúng spec
- **Source:** Phase 1→3 outputs
- **Consequences:** Implementation ready to start

---
*Created automatically when Phase 1 starts. Add entries after each Q&A session or phase completion.*
