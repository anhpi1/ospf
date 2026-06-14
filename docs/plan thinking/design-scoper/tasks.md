# Task Decomposition — OSPF Routing Simulation

> Detailed task specs: Input, Process, Output, Acceptance Criteria.
> 20 tasks across 5 groups.

---

## T-01: Project Setup (Makefile + Structure)

**Mô tả:** Khởi tạo OMNeT++ project, tạo Makefile và cấu trúc thư mục.

| Field | Detail |
|---|---|
| **Input** | OMNeT++ 6.4.0 installed, `opp_makemake` tool |
| **Process** | `opp_makemake -f --deep -o ospf_sim -O out -I.` |
| **Output** | Makefile, `src/`, `simulations/`, `results/` directories |
| **Acceptance** | `make` chạy thành công (dù chưa có code) |

**Files:** `Makefile`

---

## T-02: NED Topology Definition

**Mô tả:** Định nghĩa topology 10 router P2P trong NED.

| Field | Detail |
|---|---|
| **Input** | Design Brief topology: 10 router mesh |
| **Process** | Viết NED compound module `OspfNetwork` với 10 router con, P2P connections |
| **Output** | `OspfNetwork.ned` |
| **Acceptance** | NED syntax đúng, Qtenv hiển thị đúng topology |

**Topology:**
```
R1───R2───R3───R8
│   /│    │    │
│  / │    │    │
│ /  │    │    │
R4───R5───R9   R10
 │   │
 │   │
 R6  R7
```

**Files:** `simulations/OspfNetwork.ned`

---

## T-03: OSPF Packet Definitions (.msg)

**Mô tả:** Định nghĩa 5 loại OSPF packets + LSA format trong .msg file.

| Field | Detail |
|---|---|
| **Input** | RFC 2328 §A.3 (packet formats), §A.4 (LSA formats) |
| **Process** | Viết .msg definitions cho: OspfPacket (base), OspfHello, OspfDD, OspfLSR, OspfLSU, OspfLSAck, RouterLSA |
| **Output** | `OspfPacket.msg` (compiled → OspfPacket_m.h/.cc) |
| **Acceptance** | `make` compile .msg thành công, tất cả classes có sẵn trong C++ |

**Key fields per packet:**
- **OspfPacket:** ospfType, routerID, areaID
- **OspfHello:** helloInterval, deadInterval, neighborIDs[]
- **OspfDD:** iBit, mBit, msBit, ddSequenceNumber, lsaHeaders[]
- **OspfLSR:** requests[] (lsType, lsID, advertisingRouter)
- **OspfLSU:** lsas[] (RouterLSA)
- **OspfLSAck:** lsaHeaders[]
- **RouterLSA:** lsAge, lsSequenceNumber, links[] (linkID, linkData, type, metric)

**Files:** `src/OspfPacket.msg`

---

## T-04: OspfRouter Module Skeleton

**Mô tả:** Tạo cSimpleModule OspfRouter với cấu trúc handleMessage(), gates, parameters.

| Field | Detail |
|---|---|
| **Input** | OMNeT++ cSimpleModule API, các parameter từ Design Brief |
| **Process** | Viết class OspfRouter kế thừa cSimpleModule: initialize(), handleMessage(), finish() |
| **Output** | `OspfRouter.h`, `OspfRouter.cc` |
| **Acceptance** | Module compile được, 1 router có thể start (dù chưa xử lý gì) |

**Parameters:**
- `helloInterval` (double, default=1s) — @unit(s)
- `routerDeadInterval` (double, default=4s)
- `rxmtInterval` (double, default=0.5s)
- `spfDelay` (double, default=0.5s)
- `routerID` (string, default="10.0.0.x")

**Gates:** `ppg[]` — vector gate cho P2P interfaces

**Files:** `src/OspfRouter.h`, `src/OspfRouter.cc`

---

## T-05: Hello Protocol

**Mô tả:** Implement gửi/nhận Hello packets, timer management.

| Field | Detail |
|---|---|
| **Input** | OspfHello packet, T-04 OspfRouter skeleton |
| **Process** | `sendHello()`: tạo OspfHello → gửi ra tất cả ppg[]; `processHello()`: parse, update neighbor state |
| **Output** | Hello packets trên wire, neighbor state updates |
| **Acceptance** | RouterA gửi Hello → RouterB nhận được, log có [TX-001] HELLO |

**Schedule:** Hello timer = scheduleAt(simTime() + helloInterval)

**Files:** Sửa `OspfRouter.h/.cc`

---

## T-06: Neighbor State Machine

**Mô tả:** Implement OSPF neighbor FSM (Down→Init→2Way→ExStart→Exchange→Loading→Full) với cFSM.

| Field | Detail |
|---|---|
| **Input** | HelloReceived events, InactivityTimer |
| **Process** | Dùng cFSM, mỗi neighbor có 1 state, transition theo events |
| **Output** | Neighbor state transitions, event logs |

**Transitions (P2P simplified):**
```
Down → (HelloReceived) → Init → (2Way) → ExStart → (NegotiationDone) → Exchange → (ExchangeDone) → Loading → (LoadingDone) → Full
```

**Dead timer:** Nếu không nhận Hello trong routerDeadInterval → neighbor Down

**Files:** Sửa `OspfRouter.h/.cc`

---

## T-07a: Database Description (DD) Exchange

**Mô tả:** Implement master/slave negotiation và DD packet exchange.

| Field | Detail |
|---|---|
| **Input** | Neighbor state = ExStart, LSDB content |
| **Process** | Master/slave negotiation → exchange LSA headers via DD packets → track sequence numbers |
| **Output** | DD packets, slave's LSA request list |

**Sequence:**
1. Master: DD(i=1, M=1, MS=1, seq#)
2. Slave: DD(i=1, M=1, MS=0, seq#)
3. Master: DD(M=1, seq#+1, LSA headers)
4. Slave: DD(M=1, seq#+1, LSA headers)
5. ... repeat until M=0

**Files:** Sửa `OspfRouter.h/.cc`

---

## T-07b: LSR/LSU/LSAck Handling

**Mô tả:** Implement Link State Request, Update, Acknowledgment.

| Field | Detail |
|---|---|
| **Input** | DD exchange completed (Loading state), requested LSAs |
| **Process** | Send LSR for missing LSAs → receive LSU → install LSAs → send LSAck |
| **Output** | LSDB synchronized (Full state) |

**Files:** Sửa `OspfRouter.h/.cc`

---

## T-08: LSDB + LSA Definitions

**Mô tả:** Implement Link State Database — lưu trữ, index, lookup Router-LSAs.

| Field | Detail |
|---|---|
| **Input** | RouterLSA từ LSU packets |
| **Process** | `std::map<LSAKey, RouterLSA>`, install new LSAs (so sánh sequence number), lookup, iterate |
| **Output** | LSDB operations: install, lookup, getAll, getSequenceNumber |

**LSAKey:** `(advertisingRouter, lsType, lsID)` → `std::tuple<int,int,int>`

**Files:** Tạo `OspfLsdb.h/.cc`

---

## T-09: LSA Flooding

**Mô tả:** Flood LSAs ra tất cả interfaces (trừ interface nhận).

| Field | Detail |
|---|---|
| **Input** | New/updated LSA vừa install trong LSDB |
| **Process** | Tạo LSU chứa LSA → send ra tất cả ppg[] ngoại trừ ppg nhận → chờ LSAck → retransmit nếu timeout |
| **Output** | LSU packets trên tất cả links |

**Duplicate detection:** Nếu LSA đã có trong LSDB với seq# không cao hơn → drop

**Files:** Sửa `OspfRouter.h/.cc`

---

## T-10a: SPF Calculator (Dijkstra)

**Mô tả:** Implement Dijkstra Shortest Path First từ LSDB.

| Field | Detail |
|---|---|
| **Input** | LSDB (map of RouterLSAs), self routerID |
| **Process** | Build graph → priority queue → relax edges → track predecessors (cho ECMP) |
| **Output** | Shortest path tree dạng `{dest, cost, [prevHops]}` |

**Algorithm:**
```
dist[self] = 0, prev[u] = []
PQ: (dist, u)
while PQ:
  u = pop_min
  for each link in u's RouterLSA:  // neighbor v with cost c
    newDist = dist[u] + c
    if newDist < dist[v]: dist[v]=newDist, prev[v]=[u]
    if newDist == dist[v]: prev[v].push_back(u)  // ECMP
```

**Files:** Tạo `OspfSpf.h/.cc`

---

## T-10b: Routing Table + ECMP

**Mô tả:** Xây dựng routing table từ SPF output, hỗ trợ ECMP.

| Field | Detail |
|---|---|
| **Input** | SPF result: `{dest, cost, prev[]}` |
| **Process** | Build RoutingEntry(dest, cost, [nextHops]), xử lý ECMP (nếu prev có nhiều phần tử → thêm tất cả next-hops) |
| **Output** | Routing table: `vector<RoutingEntry>`, có `dump()` method |

**ECMP:** Nếu 2 đường có cùng cost → 2 next-hops trong 1 RoutingEntry

**Files:** Tạo `OspfRoutingTable.h/.cc`

---

## T-11: Client Module

**Mô tả:** Simple module gửi 1 data packet đến destination.

| Field | Detail |
|---|---|
| **Input** | Parameters: srcRouterID, dstRouterID, packetSize, sendTime |
| **Process** | Tạo DataPacket → gửi xuống OspfRouter qua localGate |
| **Output** | Data packet gửi vào OspfRouter để forward |

**Client gắn với mỗi router:** Chỉ router nguồn (R6) có Client gửi, chỉ router đích (R10) nhận.

**Files:** Tạo `Client.h/.cc`, sửa `OspfNetwork.ned` (thêm Client submodule)

---

## T-12: Data Forwarding

**Mô tả:** Forward data packets dựa trên OSPF routing table.

| Field | Detail |
|---|---|
| **Input** | Data packet từ Client (hoặc từ neighbor), routing table |
| **Process** | `forwardData()`: lookup dest trong routing table → get next-hop(s) → send đến đúng interface |
| **Output** | Data packet forwarded đến next-hop |

**ECMP handling:** Nếu có nhiều next-hop → round-robin hoặc chọn first

**Files:** Sửa `OspfRouter.h/.cc`

---

## T-13: Logger (Transaction + Content)

**Mô tả:** Ghi 2 file log: transaction log (có mã số) + message content log.

| Field | Detail |
|---|---|
| **Input** | Tất cả actions (send/receive các packet), packet contents |
| **Process** | `logTx()`: increment txID → ghi `[TX-{ID}] T={time} {src}→{dst} {type} {detail}`. `logContent()`: ghi `[CONTENT] T={time} TYPE={type} fields={...}` |
| **Output** | `transaction.log`, `content.log` (trong results/) |

**Transaction ID format:** TX-001, TX-002, ...

**Files:** Tạo `Logger.h/.cc`

---

## T-14: Routing Table Dump + Convergence Time

**Mô tả:** In routing table định kỳ, đo thời gian hội tụ khi topology thay đổi.

| Field | Detail |
|---|---|
| **Input** | Routing table, topology change events |
| **Process** | `dumpRoutingTable()`: EV << formatted table. Convergence: mark time when topology changes, check khi routing table ổn định |
| **Output** | Routing table in console/elog, convergence time value |

**Convergence =** time when all routers' routing tables stabilize - time when topology changed

**Files:** Sửa `OspfRouter.h/.cc`

---

## T-15: omnetpp.ini Configuration

**Mô tả:** Viết file cấu hình omnetpp.ini với tất cả parameters.

| Field | Detail |
|---|---|
| **Input** | Design Brief parameters: timers, costs, topology |
| **Process** | Viết `.ini` với network config, router parameters, scenario definitions |
| **Output** | `omnetpp.ini` |

**Parameters to include:**
```
network = OspfNetwork

**.router*.helloInterval = 1s
**.router*.routerDeadInterval = 4s
**.router*.rxmtInterval = 0.5s
**.router*.spfDelay = 0.5s
**.router*.routerID = "10.0.0.x"

**.link.delay = 10ms

# Random link costs
**.link.cost = ${uniform(1,30)}
```

**Files:** `simulations/omnetpp.ini`

---

## T-16: 5 Simulation Scenarios

**Mô tả:** Tạo 5 scenario configs với link failure/recovery events.

| Field | Detail |
|---|---|
| **Input** | Timeline từ Design Brief (S1-S5) |
| **Process** | Mỗi scenario 1 [Config] trong .ini + ScenarioManager triggers cho disconnect/connect |
| **Output** | 5 configs trong omnetpp.ini |

**Example trigger:**
```
[Config S2_leaf_fail]
**.manager.module = "ScenarioManager"
**.manager.trigger[0].at = 20s
**.manager.trigger[0].module = "router4.ppg[0]"
**.manager.trigger[0].event = "disconnect"
```

**Files:** `simulations/omnetpp.ini` (thêm 5 [Config] sections)

---

## T-17: Integration Test + Verification

**Mô tả:** Chạy toàn bộ hệ thống, verify tất cả success criteria.

| Field | Detail |
|---|---|
| **Input** | Tất cả tasks T-01→T-16 hoàn thành |
| **Process** | Run `make` → Run each scenario → Check outputs |
| **Output** | Verified results: routing tables đúng, logs đúng format |

**Checklist:**
- [ ] 10 routers neighbor state = Full
- [ ] LSDB đồng bộ giữa tất cả routers
- [ ] Routing table có đúng shortest path
- [ ] ECMP: nếu có đường bằng cost → routing table có ≥2 next-hops
- [ ] Data packet từ R6 đến R10
- [ ] 5 scenarios: các action disconnect/connect đúng timeline
- [ ] transaction.log + content.log đúng format
- [ ] Convergence time đo được

---

## Task Dependency Summary

```
T-01 (setup) ──→ T-02 (NED) ──→ T-15 (.ini) ──→ T-16 (scenarios)
     │
     └──→ T-03 (.msg) ──→ T-04 (skeleton) ──→ T-05 (Hello)
                                 │               │
                                 │               └──→ T-06 (Neighbor FSM)
                                 │                      │
                                 │                      └──→ T-07a (DD Exchange)
                                 │                             │
                                 │                             └──→ T-07b (LSR/LSU)
                                 │                                    │
                                 │                               ┌────┘
                                 │                               ▼
                                 │                         T-08 (LSDB) ──→ T-09 (Flooding)
                                 │                                            │
                                 │                                            ▼
                                 │                                      T-10a (SPF)
                                 │                                            │
                                 │                                            ▼
                                 │                                      T-10b (Routing Table)
                                 │                                            │
                                 │                                  T-12 (Data Forwarding)
                                 │                                            │
                                 │                                  ┌─────────┘
                                 │                                  ▼
                                 │                           T-11 (Client)
                                 │                            T-13 (Logger)
                                 │                            T-14 (Dumps)
                                 │
                                 └──→ T-17 (Integration Test)
```

---

## Task Summary Table

| ID | Task | Group | Depends On | Files |
|---|---|---|---|---|
| T-01 | Project Setup | Infrastructure | — | Makefile |
| T-02 | NED Topology | Infrastructure | T-01 | .ned |
| T-03 | .msg Definitions | Infrastructure | T-01 | .msg |
| T-04 | Router Skeleton | Protocol Core | T-01, T-03 | .h/.cc |
| T-05 | Hello Protocol | Protocol Core | T-04 | .h/.cc |
| T-06 | Neighbor FSM | Protocol Core | T-05 | .h/.cc |
| T-07a | DD Exchange | Protocol Core | T-06, T-08 | .h/.cc |
| T-07b | LSR/LSU/LSAck | Protocol Core | T-07a, T-08 | .h/.cc |
| T-08 | LSDB + LSA | Protocol Core | T-03 | .h/.cc |
| T-09 | LSA Flooding | Protocol Core | T-07b, T-08 | .h/.cc |
| T-10a | SPF Calculator | Protocol Core | T-08, T-09 | .h/.cc |
| T-10b | Routing Table ECMP | Protocol Core | T-10a | .h/.cc |
| T-11 | Client Module | Data Plane | T-04 | .h/.cc, .ned |
| T-12 | Data Forwarding | Data Plane | T-10b, T-11 | .h/.cc |
| T-13 | Logger | Observability | T-04 | .h/.cc |
| T-14 | Dump + Convergence | Observability | T-10b | .h/.cc |
| T-15 | omnetpp.ini | Simulation | T-02 | .ini |
| T-16 | 5 Scenarios | Simulation | T-15 | .ini |
| T-17 | Integration Test | Simulation | Tất cả | scripts |
