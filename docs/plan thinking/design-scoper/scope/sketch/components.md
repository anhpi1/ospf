# Sketch 4: Component Map

> Bản đồ thành phần — mỗi class/component trong hệ thống và trách nhiệm của nó.

---

## Component Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                      Core Components                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  OspfPacket.msg                                          │   │
│  │  (5 packet types + LSA format)                           │   │
│  └────────────────────────────┬─────────────────────────────┘   │
│                               │                                  │
│  ┌────────────────────────────▼─────────────────────────────┐   │
│  │  OspfRouter (cSimpleModule)                               │   │
│  │  - handleMessage() → dispatch                             │   │
│  │  - initialize() → setup timers + LSDB                    │   │
│  │  - owns all sub-components                                │   │
│  └──┬──────────┬──────────┬──────────┬──────────┬───────────┘   │
│     │          │          │          │          │                │
│     ▼          ▼          ▼          ▼          ▼                │
│  ┌──────┐ ┌──────┐ ┌─────────┐ ┌──────────┐ ┌──────┐          │
│  │ Hello│ │Neigh-│ │Database │ │   LSDB   │ │ Data │          │
│  │ Proto│ │bor   │ │Exchange │ │ (LSA     │ │ Forw- │          │
│  │      │ │FSM   │ │Protocol │ │  Store)  │ │ arder │          │
│  └──────┘ └──────┘ └─────────┘ └──────────┘ └──────┘          │
│                                            │                    │
│                                            ▼                    │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  SPF Calculator (Dijkstra)                                │   │
│  │  ← reads LSDB                                             │   │
│  │  → produces Routing Table                                 │   │
│  └──────┬───────────────────────────────────────────────────┘   │
│         │                                                        │
│         ▼                                                        │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  Routing Table (ECMP)                                     │   │
│  │  - dest → [next-hop list]                                 │   │
│  │  - used by Data Forwarder                                 │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                     Support Components                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐    ┌──────────────────┐    ┌──────────────┐  │
│  │   Client     │    │     Logger       │    │   Topology   │  │
│  │ (App Module) │    │ - Transaction    │    │ (NED file)   │  │
│  │ - generate   │    │ - MessageContent │    │              │  │
│  │   data       │    └──────────────────┘    └──────────────┘  │
│  └──────────────┘                                              │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Detailed Component Specifications

### 1. OspfPacket.msg

```
OspfPacket (base) ← cPacket
├── ospfType (Hello|DD|LSR|LSU|LSAck)
├── routerID (int → 10.0.0.x)
├── areaID (always 0)
└── authType (bỏ qua)

OspfHello ← OspfPacket
├── helloInterval
├── deadInterval
├── neighborIDs[] (list of seen neighbors)

OspfDD ← OspfPacket
├── interfaceMTU
├── options
├── iBit (init)
├── mBit (more)
├── msBit (master/slave)
├── ddSequenceNumber
└── lsaHeaders[] (summary list)

OspfLSR ← OspfPacket
└── requests[] (lsType, lsID, advertisingRouter)

OspfLSU ← OspfPacket
├── numLSAs
└── lsas[] (RouterLSA)

OspfLSAck ← OspfPacket
└── lsaHeaders[] (acked LSAs)

RouterLSA ← LSA
├── lsAge
├── lsSequenceNumber
├── links[]
│   ├── linkID (neighbor router)
│   ├── linkData (interface IP)
│   ├── type (1 = P2P)
│   └── metric (cost 1-30)
```

### 2. OspfRouter

| Method | Trách nhiệm |
|---|---|
| `initialize()` | Parse parameters, init LSDB, schedule Hello timer |
| `handleMessage(msg)` | Dispatch: self-msg → timer, OSPF pkt → process, data pkt → forward |
| `processHello(pkt)` | Update neighbor FSM, reply with Hello |
| `processDD(pkt)` | Handle database description exchange |
| `processLSR(pkt)` | Lookup LSDB, send LSU |
| `processLSU(pkt)` | Install LSA in LSDB, flood, ack, schedule SPF |
| `processLSAck(pkt)` | Mark retransmission list |
| `forwardData(pkt)` | Lookup routing table, send to next-hop |
| `runSPF()` | Dijkstra → update routing table |
| `sendHello()` | Broadcast Hello to all interfaces |
| `floodLSU(lsu, exceptInterface)` | Forward LSU to all neighbors |

### 3. Neighbor FSM

```
States:  Down → Init → 2Way → ExStart → Exchange → Loading → Full
Events:  HelloReceived, NegotiationDone, ExchangeDone, LoadingDone
AdjacencyOK, SeqNumberMismatch, OneWay, KillNbr, InactivityTimer

Transitions (P2P simplified):
┌─────────┐    HelloReceived    ┌────────┐
│  Down   │ ──────────────────→ │  Init  │
└─────────┘                     └────────┘
                                     │
                            HelloReceived (bidirectional)
                                     │
                                     ▼
                               ┌──────────┐
                               │  2Way    │
                               └──────────┘
                                     │
                              AdjacencyOK
                                     │
                                     ▼
                               ┌──────────┐
                               │  ExStart │
                               └──────────┘
                                     │
                              NegotiationDone
                                     │
                                     ▼
                               ┌──────────┐
                               │ Exchange │
                               └──────────┘
                                     │
                              ExchangeDone
                                     │
                                     ▼
                               ┌──────────┐
                               │  Loading │
                               └──────────┘
                                     │
                              LoadingDone
                                     │
                                     ▼
                               ┌──────────┐
                               │   Full   │
                               └──────────┘
```

### 4. LSDB (Link State Database)

```
LSDB:
  Container: std::map<LSAKey, RouterLSA>
  Key: (advertisingRouter, lsType, lsID)
  
  Methods:
  - installLSA(lsa) → add/update, return bool (newer?)
  - lookupLSA(key) → LSA*
  - getAllLSAs() → vector<LSA>
  - getLSAsByRouter(routerID) → filtered view
  - getSequenceNumber() → next seq
```

### 5. Routing Table

```
RoutingEntry:
  - destination: RouterID
  - cost: int
  - nextHops: vector<NextHop>
       NextHop: {interfaceID, neighborRouterID}
  - isECMP: bool

RoutingTable:
  Container: std::vector<RoutingEntry>
  
  Methods:
  - addRoute(dest, cost, nextHop)
  - lookup(dest) → RoutingEntry* (ECMP-aware)
  - clear()
  - dump() → EV output
```

### 6. SPF Calculator

```
SPFCalculator:
  Input: LSDB (graph representation)
  Output: RoutingTable (shortest path tree)
  
  Algorithm:
  1. Khởi tạo: distance[routerID] = ∞, prev[routerID] = []
  2. distance[self] = 0
  3. priority_queue: (dist, routerID)
  4. Với mỗi vertex, relax:
     newDist = dist[u] + linkCost(u, v)
     if newDist < dist[v]: cập nhật, prev[v] = [u]
     if newDist == dist[v]: prev[v] += [u] (ECMP!)
  5. Build routing table từ prev[]
```

### 7. Logger

```
Logger (singleton hoặc module-level):
  - transactionID: int (tăng dần mỗi action)
  - logTransaction(action, src, dst, type, detail)
      Format: [TX-001] T=1.5s 10.0.0.1 → 10.0.0.2 HELLO neighbors=[2]
  - logContent(msg)
      Format: [CONTENT] T=1.5s TYPE=HELLO fields={...}
  
  Output files:
  - transaction.log (file descriptor 1)
  - content.log (file descriptor 2)
```

---

## Component Responsibilities Matrix

| Component | Input | Process | Output |
|---|---|---|---|
| OspfRouter | Config, packets | Dispatch, orchestrate | Forwarded packets, state changes |
| Hello Protocol | HelloInterval config | Generate timers, send/recv Hellos | Neighbor state updates |
| Neighbor FSM | Hello/DD events | State transitions | Adjacency state |
| DB Exchange | DD/LSR/LSU/Ack | Master/slave negotiation | Synced LSDB |
| LSDB | LSAs from LSU | Store, index, lookup | LSA retrieval for SPF |
| SPF Calculator | LSDB content | Dijkstra algorithm | Routing table entries |
| Routing Table | SPF output + ECMP | Sort, dedup, lookup | Forwarding decisions |
| Data Forwarder | Data packets + routing table | Table lookup | Forwarded packets |
| Logger | All events | Format + write | transaction.log + content.log |
| Client | Scenario config | Generate data packet | Data packet to OspfRouter |
