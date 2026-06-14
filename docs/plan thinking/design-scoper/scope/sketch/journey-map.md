# Sketch 1: Journey Map

> Hành trình của developer qua quy trình xây dựng và chạy mô phỏng OSPF routing.

---

## Tổng quan

```
Phase A: Setup                      Phase B: Build                    Phase C: Run & Analyze
─────────────────────────    ─────────────────────────        ─────────────────────────────

  [1] Cấu hình project       [4] Viết .msg files               [8] Build project
       opp_makemake               OspfHello.msg                      make
       Makefile                   OspfDD.msg                   
                                  OspfLSR.msg                  [9] Run simulation
  [2] Thiết kế topology            OspfLSU.msg                      opp_run -u Qtenv
       OspfNetwork.ned             OspfLSack.msg              
                                                            [10] Quan sát Qtenv
  [3] Xác định cấu hình      [5] Code OSPF Router                 - Animation gói tin
       omnetpp.ini                 OspfRouter.h/.cc               - Module inspector
                                  - Hello protocol               - Bảng routing
  ═══════════════════              - Neighbor FSM           
  Milestone: Setup done            - Database Exchange        [11] Chạy 5 kịch bản
  ═══════════════════              - LSDB + Flooding               S1: steady
                                   - SPF (Dijkstra)               S2: leaf fail
                              [6] Code Data Forwarding            S3: backbone fail
                                   - Client module                S4: recovery
                                   - Forward logic                S5: multi-fail
                              [7] Code Logging               [12] Phân tích kết quả
                                   - Transaction log              - So sánh routing tables
                                   - Message content log          - Đo convergence time
                                                                  - Check 2 file log
                              ═══════════════════
                              Milestone: Build done         ═══════════════════════════
                                                            Milestone: Analysis done
```

---

## Các bước chi tiết

### Phase A: Setup
| Bước | Mô tả | Output |
|---|---|---|
| A1 | Khởi tạo OMNeT++ project | Makefile |
| A2 | Viết NED topology | `OspfNetwork.ned` |
| A3 | Cấu hình tham số | `omnetpp.ini` |

### Phase B: Build
| Bước | Mô tả | Output |
|---|---|---|
| B1 | Định nghĩa 5 OSPF packet types | 5 .msg files |
| B2 | Code router module | `OspfRouter.h/.cc` |
| B3 | Code client + forwarding | `Client.h/.cc` |
| B4 | Code logging subsystem | Logger classes |
| B5 | Compile | Binary |

### Phase C: Run & Analyze
| Bước | Mô tả | Output |
|---|---|---|
| C1 | Run S1 (steady) trong Qtenv | Animation + logs |
| C2 | Run S2-S5 (các scenario khác) | So sánh behavior |
| C3 | Đọc routing table dumps | Verify correctness |
| C4 | Đọc transaction + content logs | Trace packet flow |
| C5 | Đo convergence time | Metrics |

---

## Touchpoints trong hành trình

| Giai đoạn | Touchpoint | Trải nghiệm mong đợi |
|---|---|---|
| Build | IDE / text editor | Code completion, syntax highlight |
| Compile | Terminal | `make` clean build |
| Run | Qtenv | GUI hiện ra, animation chạy |
| Debug | Qtenv Inspector | Xem module state, LSDB content |
| Analyze | Terminal / log files | Log có cấu trúc, grep được |
