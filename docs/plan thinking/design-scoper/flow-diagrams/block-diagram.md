# Block Diagram — Dependency & Execution Order

> Phân tích dependency và thứ tự thực thi của 20 tasks.
> Phase 3, B1.

---

## Dependency Graph

```
T-01 ──→ T-02 ──→ T-15 ──→ T-16
  │      └──→ (parallel)
  │
  └──→ T-03 ──→ T-04 ──→ ┌──────┐
                          │ G2   │
  T-08 (class) ──────────→│      │
                          │ T-05 │
                          │ T-06 │
                          │ T-07a│
                          │ T-07b│
                          │ T-08 │
                          │ T-09 │
                          └──┬───┘
                             │
                             ▼
                          T-10a ──→ T-10b ──→ ┌─────┐
                                                │ G5  │
                          T-11 ───────────────→│ T-12│
                                                │ T-14│
                          T-13 ───────────────→│     │
                                                └──┬──┘
                                                   │
                                                   ▼
                                                T-17
```

## Execution Phases

```
PHASE A — Infrastructure
──────────────────────────────────────────────────
T-01  Project Setup         ████████░░░░░░░░░░░░
T-02  NED Topology          ░░████████░░░░░░░░░░
T-03  .msg Definitions      ░░████████░░░░░░░░░░
T-08  LSDB Class (partial)  ░░██████░░░░░░░░░░░░

PHASE B — Protocol Core Initial
──────────────────────────────────────────────────
T-04  Router Skeleton       ░░░░████████░░░░░░░░
T-05  Hello Protocol        ░░░░░░████████░░░░░░
T-06  Neighbor FSM          ░░░░░░░░████████░░░░
T-08  LSDB (full)           ░░░░░░████████░░░░░░

PHASE C — Database Exchange
──────────────────────────────────────────────────
T-07a DD Exchange           ░░░░░░░░░░████████░░
T-07b LSR/LSU/LSAck         ░░░░░░░░░░░░████████

PHASE D — SPF & Routing
──────────────────────────────────────────────────
T-09  LSA Flooding          ░░░░░░░░░░░░██████░░
T-10a SPF (Dijkstra)        ░░░░░░░░░░░░░░██████
T-10b Routing Table ECMP    ░░░░░░░░░░░░░░░░████

PHASE E — Data Plane & Observation
──────────────────────────────────────────────────
T-11  Client Module         ░░░░░░░░░░░░░░████░░
T-12  Data Forwarding       ░░░░░░░░░░░░░░░░████
T-13  Logger                ░░░░░░░░░░░░░░████░░
T-14  Dump + Convergence    ░░░░░░░░░░░░░░░░████

PHASE F — Config & Scenarios
──────────────────────────────────────────────────
T-15  omnetpp.ini           ░░░░░░░░░░████████░░
T-16  5 Scenarios           ░░░░░░░░░░░░████████

PHASE G — Integration
──────────────────────────────────────────────────
T-17  Integration Test      ░░░░░░░░░░░░░░░░░░██

Time ──────────────────────────────────────────────────>
```

## Build Order Summary

| Order | Tasks | Lý do |
|---|---|---|
| **1** | T-01 (setup) | Foundation |
| **2** | T-02, T-03, T-08-class (parallel) | Độc lập, cùng cần sau setup |
| **3** | T-04 (skeleton) | Cần T-03 |
| **4** | T-05, T-08-impl, T-11, T-13 (parallel) | Cần T-04, độc lập nhau |
| **5** | T-06 (neighbor FSM) | Cần T-05 |
| **6** | T-07a (DD exchange) | Cần T-06 |
| **7** | T-07b (LSR/LSU) | Cần T-07a |
| **8** | T-09 (flooding) | Cần T-07b + T-08 |
| **9** | T-10a (SPF) | Cần T-08 + T-09 |
| **10** | T-10b (RT ECMP) | Cần T-10a |
| **11** | T-12 (forward), T-14 (dump) | Cần T-10b |
|  | T-15, T-16 (config) | Bất kỳ khi nào sau T-02 |
| **12** | T-17 (integration) | Cần tất cả |

## Notes

- **T-08** bị split: class definition sau T-03, full impl sau T-07b
- **T-11, T-13** có thể làm sớm (sau T-04) vì không phụ thuộc OSPF core
- **T-15, T-16** độc lập với OSPF core — có thể làm song song
- **T-17** phải đợi tất cả tasks khác
