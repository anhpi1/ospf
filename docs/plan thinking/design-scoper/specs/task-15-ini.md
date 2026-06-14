# Task Spec: T-15 — omnetpp.ini Configuration

> Viết file cấu hình omnetpp.ini với tất cả parameters.

---

## 1. Overview
File cấu hình trung tâm cho simulation: network, router parameters, link parameters.

## 2. Requirements
- Tất cả configurable parameters
- Network selection
- Router parameters (timers, IDs)
- Link parameters (delay, cost)

## 3. Input
- Design Brief parameters
- NED topology từ T-02

## 4. Process

```ini
[General]
network = OspfNetwork
description = "OSPF Routing Simulation - Single Area P2P"
sim-time-limit = 100s
cpu-time-limit = 300s

# ===== Router Parameters =====
**.router[*].helloInterval = 1s
**.router[*].routerDeadInterval = 4s
**.router[*].rxmtInterval = 0.5s
**.router[*].spfDelay = 0.5s

# Router IDs (10.0.0.1 → 10.0.0.10)
**.router[0].routerID = "10.0.0.1"
**.router[1].routerID = "10.0.0.2"
**.router[2].routerID = "10.0.0.3"
**.router[3].routerID = "10.0.0.4"
**.router[4].routerID = "10.0.0.5"
**.router[5].routerID = "10.0.0.6"
**.router[6].routerID = "10.0.0.7"
**.router[7].routerID = "10.0.0.8"
**.router[8].routerID = "10.0.0.9"
**.router[9].routerID = "10.0.0.10"

# ===== Link Parameters =====
**.delay = 10ms

# Client parameters (only R6 sends, R10 receives)
**.router[5].client.sendTime = 10s
**.router[5].client.srcRouterID = 6
**.router[5].client.dstRouterID = 10
**.router[9].client.sendTime = 0s  # never sends

# ===== GUI Settings =====
*.display-string = "i=abstract/network"
**.router[*].display-string = "i=device/router"

# ===== Logging =====
**.enableLogging = true
```

## 5. Output
- `simulations/omnetpp.ini`

## 6. Acceptance Criteria
- opp_run loads file không lỗi
- Parameters đọc được trong OspfRouter::initialize()
- Các giá trị timer đúng (1s, 4s, ...)

## 7. Related Tasks
- T-02 (NED): network definition
- T-04 (Skeleton): parameter reading
- T-16 (Scenarios): add scenario configs

## 8. Notes
- Parameters có thể override trong từng [Config]
- Dùng `sim-time-limit` để simulation tự kết thúc
