# Sketch 3: Architecture Diagram

> Kiến trúc tổng thể hệ thống mô phỏng OSPF routing.

---

## System Overview

```
┌────────────────────────────────────────────────────────────────────────────┐
│                         OMNeT++ Simulation Kernel                          │
│  (cSimulation, cModule, cMessage, cGate, cFSM, cPacket...)                │
└────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────────┐
│                     Network: OspfNetwork (NED)                            │
│                                                                           │
│        router[0]         router[1]         ...          router[9]        │
│    ┌──────────────┐   ┌──────────────┐              ┌──────────────┐     │
│    │   Router     │   │   Router     │              │   Router     │     │
│    │ ┌──────────┐ │   │ ┌──────────┐ │              │ ┌──────────┐ │     │
│    │ │ Client   │ │   │ │ Client   │ │              │ │ Client   │ │     │
│    │ │ (App)    │ │   │ │ (App)    │ │              │ │ (App)    │ │     │
│    │ └────┬─────┘ │   │ └────┬─────┘ │              │ └────┬─────┘ │     │
│    │      │       │   │      │       │              │      │       │     │
│    │ ┌────▼─────┐ │   │ ┌────▼─────┐ │              │ ┌────▼─────┐ │     │
│    │ │OSPF      │◄├───├►│OSPF      │◄├─── ... ────├►│OSPF      │ │     │
│    │ │Router    │ │   │ │Router    │ │              │ │Router    │ │     │
│    │ └──────────┘ │   │ └──────────┘ │              │ └──────────┘ │     │
│    └──────────────┘   └──────────────┘              └──────────────┘     │
│    10.0.0.1           10.0.0.2                    10.0.0.10             │
└────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────────┐
│                             omnetpp.ini                                   │
│  - Network topology (OspfNetwork)                                         │
│  - Router parameters (timers, costs)                                      │
│  - Scenario selection (S1-S5)                                             │
│  - GUI settings (Qtenv)                                                   │
└────────────────────────────────────────────────────────────────────────────┘
```

---

## Router Internal Architecture

```
┌────────────────────────────────────────────────────────────────┐
│                    Router (compound module)                    │
│  ID: 10.0.0.x                                                 │
│                                                                │
│  ┌────────────────────────────────────────────────────────┐   │
│  │ Client (cSimpleModule)                                 │   │
│  │ - generateDataPacket()                                 │   │
│  │ - sendToOspfRouter()                                   │   │
│  │ - receiveFromOspfRouter()                              │   │
│  └──────────┬─────────────────────────────────────────────┘   │
│             │ localGate                                       │
│             ▼                                                 │
│  ┌────────────────────────────────────────────────────────┐   │
│  │ OspfRouter (cSimpleModule)                             │   │
│  │                                                        │   │
│  │  ┌──────────────────────────────────────────────────┐  │   │
│  │  │           handleMessage(cMessage *msg)           │  │   │
│  │  │                                                  │  │   │
│  │  │  msg->isSelfMessage() ? TimerHandler(msg)        │  │   │
│  │  │                    : PacketHandler(msg)          │  │   │
│  │  └──────────────────────┬───────────────────────────┘  │   │
│  │                         │                               │   │
│  │  ┌──────────────────────▼───────────────────────────┐  │   │
│  │  │              Packet Dispatcher                    │  │   │
│  │  │  ┌─────────┐ ┌────────┐ ┌───────┐ ┌─────────┐   │  │   │
│  │  │  │ Ospf    │ │ Ospf   │ │ Ospf  │ │ Ospf    │   │  │   │
│  │  │  │ Hello   │ │ DD     │ │ LSR   │ │ LSU     │   │  │   │
│  │  │  │Handler  │ │Handler │ │Handler│ │Handler  │   │  │   │
│  │  │  └────┬────┘ └───┬────┘ └───┬───┘ └────┬────┘   │  │   │
│  │  │       │          │          │          │        │  │   │
│  │  │  ┌────▼──────────▼──────────▼──────────▼────┐   │  │   │
│  │  │  │           OspfProtocolEngine              │   │  │   │
│  │  │  │  ┌─────────┐ ┌────────┐ ┌─────────────┐  │   │  │   │
│  │  │  │  │Neighbor │ │Interface│ │  Database   │  │   │  │   │
│  │  │  │  │FSM (xN) │ │FSM(xN) │ │  Exchange   │  │   │  │   │
│  │  │  │  └─────────┘ └────────┘ └─────────────┘  │   │  │   │
│  │  │  │  ┌─────────┐ ┌────────────────────┐      │   │  │   │
│  │  │  │  │  LSDB   │ │  Routing Table     │      │   │  │   │
│  │  │  │  │(LSA map)│ │  (ECMP support)    │      │   │  │   │
│  │  │  │  └─────────┘ └────────────────────┘      │   │  │   │
│  │  │  │  ┌────────────────────────────────────┐  │   │  │   │
│  │  │  │  │  SPF Calculator (Dijkstra)         │  │   │  │   │
│  │  │  │  └────────────────────────────────────┘  │   │  │   │
│  │  │  └──────────────────────────────────────────┘   │  │   │
│  │  │                                                  │  │   │
│  │  │  ┌──────────────────────────────────────────┐   │  │   │
│  │  │  │  Data Forwarder                          │   │  │   │
│  │  │  │  (forward based on routing table)        │   │  │   │
│  │  │  └──────────────────────────────────────────┘   │  │   │
│  │  │                                                  │  │   │
│  │  │  ┌──────────────────────────────────────────┐   │  │   │
│  │  │  │  Logger                                  │   │  │   │
│  │  │  │  - Transaction log (action ID)           │   │  │   │
│  │  │  │  - Message content log                   │   │  │   │
│  │  │  └──────────────────────────────────────────┘   │  │   │
│  │  └──────────────────────────────────────────────────┘  │   │
│  │                                                        │   │
│  │  Gates: ppg[0..MAX_INTERFACES] (P2P)                   │   │
│  └────────────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────────┘
```

---

## Communication Flow

```
┌─────────┐         ┌─────────┐               ┌─────────┐
│ RouterA │         │ RouterB │               │ RouterC │
└────┬────┘         └────┬────┘               └────┬────┘
     │                   │                         │
     │---Hello---------->│                         │     [1s interval]
     │<--Hello-----------│                         │
     │                   │                         │
     │---DD (master)---->│                         │     [DB Exchange]
     │<--DD (slave)------│                         │
     │---LSR------------>│                         │
     │<--LSU (LSA)-------│                         │
     │---LSAck---------->│                         │
     │                   │                         │
     │---LSU (flood)---->│---LSU (flood)---------->│     [Flooding]
     │                   │                         │
     │                   │                         │
     │         [SPF Delay]                         │
     │         [Run Dijkstra]                      │
     │         [Update Routing Table]              │
     │                   │                         │
     │---Data Packet---->│---Data Packet---------->│     [Forwarding]
     │                   │                         │
```

---

## File Structure

```
ospf-simulation/
├── Makefile
├── src/
│   ├── OspfRouter.h
│   ├── OspfRouter.cc
│   ├── OspfPacket.msg
│   ├── Client.h
│   ├── Client.cc
│   └── Logger.h
├── simulations/
│   ├── OspfNetwork.ned
│   ├── omnetpp.ini
│   ├── s1_steady.ini
│   ├── s2_leaf_fail.ini
│   ├── s3_backbone_fail.ini
│   ├── s4_recovery.ini
│   └── s5_multi_fail.ini
└── results/
    ├── transaction.log
    ├── content.log
    └── routing_dumps/
```

