# Task Spec: T-02 — NED Topology Definition

> Định nghĩa topology mạng 10 router P2P trong NED.

---

## 1. Overview
Viết compound module `OspfNetwork` trong NED, định nghĩa 10 router con kết nối P2P.

## 2. Requirements
- 10 submodules: `router[0]`...`router[9]` với type `OspfRouter`
- Mỗi router có vector gate `ppg[]` cho P2P connections
- Connections theo topology mesh đã duyệt

## 3. Input
- Topology mesh từ Design Brief
- OspfRouter module type (sẽ define trong T-04)

## 4. Process

```
network OspfNetwork {
    submodules:
        router[0]: OspfRouter { parameters... gates: ppg[3]; }
        router[1]: OspfRouter { ... ppg[4]; }
        ...
    connections:
        router[0].ppg[0] <--> { delay = 10ms; } <--> router[1].ppg[0];
        router[0].ppg[1] <--> { delay = 10ms; } <--> router[3].ppg[0];
        ...
}
```

**Full connection list:**
- R0–R1, R0–R3
- R1–R2, R1–R3, R1–R4
- R2–R3, R2–R7
- R3–R4, R3–R5
- R4–R5, R4–R6, R4–R8
- R5–R6, R5–R7
- R7–R9

## 5. Output
- `simulations/OspfNetwork.ned`

## 6. Acceptance Criteria
- NED syntax check passes (`opp_nedtool -n simulations simulations/OspfNetwork.ned`)
- Qtenv hiển thị 10 router với connections đúng topology

## 7. Related Tasks
- T-01 (Setup): cần directory simulations/
- T-04 (Skeleton): module OspfRouter phải tồn tại trong C++
- T-15 (.ini): network name cho omnetpp.ini

## 8. Notes
- Dùng `@display` properties để visual position trong Qtenv
- Vector gate `ppg[]` size = số connections của router đó
