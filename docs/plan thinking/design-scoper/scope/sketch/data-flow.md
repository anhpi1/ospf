# Sketch 5: Data Flow Diagram

> Luồng dữ liệu giữa các thành phần trong hệ thống mô phỏng OSPF routing.

---

## 1. Overall Data Flow

```
┌─────────────┐
│ omnetpp.ini │─── parameters (timers, costs, scenario)
└─────────────┘
      │
      ▼
┌─────────────────────────────────────────────────────────────────────┐
│                     OSPF Simulation System                           │
│                                                                     │
│  ┌──────────┐   data_packet   ┌──────────────┐   OSPF_Packet      │
│  │  Client  │ ───────────────→│ OspfRouter   │──────────────────→  │
│  │  (App)   │←─────────────── │              │←──────────────────  │
│  └──────────┘  data_packet    │              │   OSPF_Packet       │
│                                │              │                     │
│                                │    ┌───────┐ │                     │
│                                │    │ LSDB  │ │                     │
│                                │    └───┬───┘ │                     │
│                                │        │     │                     │
│                                │        ▼     │                     │
│                                │  ┌─────────┐ │                     │
│                                │  │ SPF     │ │                     │
│                                │  │(Dijkstra)│ │                     │
│                                │  └────┬────┘ │                     │
│                                │       │      │                     │
│                                │       ▼      │                     │
│                                │  ┌─────────┐ │                     │
│                                │  │Routing  │ │                     │
│                                │  │Table    │ │                     │
│                                │  └─────────┘ │                     │
│                                └──────────────┘                     │
│                                      │                              │
│                                      ▼                              │
│                               ┌──────────────┐                     │
│                               │   Logger     │                     │
│                               │  (2 files)   │                     │
│                               └──────────────┘                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 2. OSPF Protocol Data Flow (Router-to-Router)

### 2.1 Hello Protocol Flow

```
RouterA                              RouterB
   │                                    │
   │  ┌── Hello Timer fires ──┐         │
   │  │  create OspfHello     │         │
   │  │  set routerID=10.0.0.1│         │
   │  │  set neighbors=[...]  │         │
   │  └────────┬──────────────┘         │
   │           │                        │
   │           ▼                        │
   │     send(hello, ppg[0])            │
   │  ─────────────────────────────────→│
   │                                    │  ┌── receive Hello ──┐
   │                                    │  │ processHello()    │
   │                                    │  │ update neighbor   │
   │                                    │  │ FSM: Init→2Way   │
   │                                    │  └───────────────────┘
   │                                    │
   │                                    │  ┌── reply with Hello ──┐
   │                                    │  │ create OspfHello     │
   │  ┌── receive Hello ──┐            │  │ set neighbors=[...]  │
   │  │ processHello()    │            │  └────────┬─────────────┘
   │  │ update neighbor   │◄───────────────────────┤
   │  │ FSM: Init→2Way    │        send(hello)
   │  └───────────────────┘
   │
```

### 2.2 Database Exchange Flow (ExStart → Loading → Full)

```
RouterA (Master)                    RouterB (Slave)
   │                                    │
   │  ┌─ ExStart ────────────────────── │
   │  │ create DD(iBit=1, M=1, MS=1)   │
   │  │ send DD                         │
   │  ─────────────────────────────────→│
   │                                    │ reply DD(i=1,M=1,MS=0)
   │  ←──────────────────────────────── │
   │  │                                 │
   │  │  ┌─ Master/Slave resolved ─┐    │
   │  │  │ A=Master, B=Slave       │    │
   │  │  │ seqNum = ddSeqA         │    │
   │  │  └─────────────────────────┘    │
   │  │                                 │
   │  ├─ Exchange ───────────────────── │
   │  │ DD (seq, LSAheaders...)          │
   │  ─────────────────────────────────→│
   │                                    │
   │  ←──────────────────────────────── │
   │  │ DD (seq+1, LSAheaders...)       │
   │  │                                 │
   │  │ ... exchange continues ...      │
   │  │                                 │
   │  │ DD(M=0) — last packet           │
   │  ─────────────────────────────────→│
   │                                    │
   │  ←──────────────────────────────── │
   │  │ DD(M=0) — slave done            │
   │  │                                 │
   │  ├─ Loading ────────────────────── │
   │  │ LSR(requests)                   │
   │  ─────────────────────────────────→│
   │                                    │
   │  ←──────────────────────────────── │
   │  │ LSU(LSA)                        │
   │  │                                 │
   │  │ LSAck                           │
   │  ─────────────────────────────────→│
   │                                    │
   │  │ ... continue for each LSA ...   │
   │  │                                 │
   │  │  ┌─ LoadingDone ─┐             │
   │  │  │ State→Full    │             │
   │  │  └───────────────┘             │
   │                                    │
```

### 2.3 Flooding Flow (LSA Propagation)

```
RouterA                              RouterB                    RouterC
   │                                    │                         │
   │   ┌─ Topology change ─┐           │                         │
   │   │ runSPF triggered   │          │                         │
   │   │ new LSA generated  │          │                         │
   │   └────────┬───────────┘          │                         │
   │            │                      │                         │
   │            ▼                      │                         │
   │   create LSU(1 LSA)               │                         │
   │   send LSU to all neighbors       │                         │
   │  ────────────────────────────────→│                         │
   │            │                      │                         │
   │            │              ┌───────▼────────┐                │
   │            │              │ processLSU()   │                │
   │            │              │ is newer? yes  │                │
   │            │              │ install in LSDB│                │
   │            │              │ schedule SPF   │                │
   │            │              │ flood further? │                │
   │            │              └───────┬────────┘                │
   │            │                      │                         │
   │            │          send LSU    │  (except from A)        │
   │            │  ──────────────────────────────────────────────→│
   │            │                      │                         │
   │            │              ┌───────▼────────┐                │
   │            │              │ send LSAck     │                │
   │  ←────────────────────────┤ (to RouterA)  │                │
   │  LSAck      │              └────────────────┘                │
   │            │                      │                         │
   │            │                      │                    ┌────▼────┐
   │            │                      │                    │process  │
   │            │                      │                    │LSU      │
   │            │                      │                    │install  │
   │            │                      │                    │flood    │
   │            │                      │                    │ack      │
   │            │                      │                    └─────────┘
```

---

## 3. Data Packet Flow

```
Client@R6                         OspfRouter@R6                    ...RouterX...          OspfRouter@R10        Client@R10
   │                                    │                              │                       │                   │
   │  create DataPacket                  │                              │                       │                   │
   │  src=10.0.0.6                       │                              │                       │                   │
   │  dst=10.0.0.10                      │                              │                       │                   │
   │  send(OspfRouter, localGate)        │                              │                       │                   │
   │────────────────────────────────────→│                              │                       │                   │
   │                                    │  receive DataPacket           │                       │                   │
   │                                    │  forwardData()                │                       │                   │
   │                                    │  lookup routing table         │                       │                   │
   │                                    │  dest=10.0.0.10 → nextHop    │                       │                   │
   │                                    │  send(ppg[i])                 │                       │                   │
   │                                    │──────────────────────────────→│                       │                   │
   │                                    │                              │  forwardData()         │                   │
   │                                    │                              │  lookup routing table  │                   │
   │                                    │                              │  forward...            │                   │
   │                                    │                              │────────────────────────→│                   │
   │                                    │                              │                        │  receive DataPkt  │
   │                                    │                              │                        │  forward to Client│
   │                                    │                              │                        │───────────────────→│
   │                                    │                              │                        │                   │  receive data
   │                                    │                              │                        │                   │  done!
```

---

## 4. Scenario Control Flow

```
omnetpp.ini (configures which scenario)
      │
      ▼
┌──────────────────────────┐
│  OspfNetwork (NED)       │
│  submodules: 10 routers  │
│  connections: P2P links  │
└──────────────────────────┘
      │
      ▼
┌─────────────────────────────────────────────────────────────────────┐
│  omnetpp.ini [Config S2_leaf_fail]                                  │
│  manager = ScenarioManager                                          │
│  triggers: {T=20s: disconnect(R6.ppg[0], R4.ppg[0]),               │
│             T=40s: connect(...)}                                    │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 5. Logging Data Flow

```
              ┌──────────────────────┐
              │  OspfRouter actions   │
              └──────────┬───────────┘
                         │
                         ▼
              ┌──────────────────────┐
              │  Logger::logTx()     │
              │  +1 transactionID    │
              │  format:             │
              │  [TX-{ID}] T={time}  │
              │  {src}→{dst}         │
              │  {type} {detail}     │
              └──────────┬───────────┘
                         │
           ┌─────────────┴─────────────┐
           │                           │
           ▼                           ▼
  ┌──────────────────┐      ┌──────────────────┐
  │transaction.log   │      │  content.log     │
  │[TX-001] T=1.5    │      │  [CONTENT] T=1.5 │
  │10.0.0.1→10.0.0.2 │      │  TYPE=HELLO      │
  │HELLO neighbors=[]│      │  routerID=1      │
  │[TX-002] T=2.0    │      │  helloInt=1      │
  │10.0.0.2→10.0.0.1 │      │  neighbors=[1]   │
  │HELLO neighbors=[]│      │                  │
  │...               │      │  [CONTENT] T=3.0 │
  └──────────────────┘      │  TYPE=DD         │
                             │  master=1        │
                             │  seq=123         │
                             │  LSAheaders=[...]│
                             └──────────────────┘
```

---

## 6. SPF Calculation Data Flow

```
LSDB (input)
  │
  │  map<LSAKey, RouterLSA>
  │  [R1: {links: [(R2, cost=5), (R4, cost=3)]}]
  │  [R2: {links: [(R1, cost=5), (R3, cost=7), (R5, cost=2)]}]
  │  ...
  ▼
┌────────────────┐
│  Build Graph    │
│  vertices:      │
│    {R1,R2,...}  │
│  edges:         │
│    R1─(5)─R2   │
│    R1─(3)─R4   │
│    R2─(7)─R3   │
│    R2─(2)─R5   │
│    ...          │
└───────┬────────┘
        │
        ▼
┌────────────────┐
│  Dijkstra SPF   │  O((V+E)logV)
│  1. dist[src]=0 │
│  2. priority Q  │
│  3. while Q:    │
│     u = pop_min │
│     for each v: │
│       relax(u,v)│
└───────┬────────┘
        │
        ▼
┌──────────────────────────┐
│  Routing Table            │
│  dest  cost  nextHops     │
│  R2     5    [R2]         │
│  R3    12    [R2,R5] (ECMP)│
│  R4     3    [R4]         │
│  R5     7    [R2,R4]      │
│  ...                       │
└──────────────────────────┘
```

---

## 7. File I/O Data Flow

```
Files (Input)                              Files (Output)
                                        
┌─────────────┐                          ┌──────────────────┐
│ omnetpp.ini ├──→ Config parameters     │ transaction.log  │
└─────────────┘                          │ (append during   │
                                          │  simulation)     │
┌─────────────┐                          └──────────────────┘
│OspfNetwork  │
│.ned         ├──→ Topology definition    ┌──────────────────┐
└─────────────┘                          │ content.log      │
                                          │ (append during   │
┌─────────────┐                          │  simulation)     │
│ Makefile    ├──→ Build system           └──────────────────┘
└─────────────┘
                                          ┌──────────────────┐
┌─────────────┐                          │ routing_dump/    │
│*.msg        ├──→ Packet definitions    │ s1_routing.txt   │
└─────────────┘                          │ s2_routing.txt   │
                                          │ ...              │
┌─────────────┐                          └──────────────────┘
│*.h / *.cc   │
│(source code)├──→ Compiled binary        ┌──────────────────┐
└─────────────┘                          │ Qtenv (GUI)      │
                                          │ (live display)   │
                                          └──────────────────┘
```

