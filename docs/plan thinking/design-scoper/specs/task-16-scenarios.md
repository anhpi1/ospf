# Task Spec: T-16 — 5 Simulation Scenarios

> Tạo 5 scenario configs với link failure/recovery events.

---

## 1. Overview
5 scenario configs trong omnetpp.ini, mỗi scenario có link disconnect/reconnect events qua ScenarioManager.

## 2. Requirements
- 5 [Config] sections
- ScenarioManager triggers cho link actions
- Đúng timeline từ Design Brief

## 3. Input
- Base .ini config (T-15)
- Timeline từ Design Brief (Q15)

## 4. Process

### S1: Steady-State
```ini
[Config S1_steady]
description = "Baseline: steady-state, no failures"
**.client.sendTime = 10s  # client gửi data sau khi OSPF hội tụ
```

### S2: Leaf Failure
```ini
[Config S2_leaf_fail]
description = "Leaf router R6 loses connection to R4"
**.client.sendTime = 30s  # data sau khi hội tụ lại

# R6-R4 disconnect at T=20s
*.manager.module = "ScenarioManager"
*.manager.trigger[0].at = 20s
*.manager.trigger[0].event = "disconnect"
*.manager.trigger[0].module = "router[5].ppg[0]"  # R6's link to R4
*.manager.trigger[0].gate = "router[3].ppg[2]"    # R4's link to R6

# R6-R4 reconnect at T=40s
*.manager.trigger[1].at = 40s
*.manager.trigger[1].event = "connect"
*.manager.trigger[1].module = "router[5].ppg[0]"
*.manager.trigger[1].gate = "router[3].ppg[2]"
```

### S3: Backbone Failure
```ini
[Config S3_backbone_fail]
description = "Backbone link R2-R5 fails"
*.client.sendTime = 30s

# R2-R5 disconnect at T=20s
*.manager.module = "ScenarioManager"
*.manager.trigger[0].at = 20s
*.manager.trigger[0].event = "disconnect"
*.manager.trigger[0].module = "router[1].ppg[2]"  # R2's link to R5
*.manager.trigger[0].gate = "router[4].ppg[2]"    # R5's link to R2
```

### S4: Recovery
```ini
[Config S4_recovery]
description = "Backbone link R2-R5 fails, then recovers"
*.client.sendTime = 60s

*.manager.module = "ScenarioManager"
*.manager.trigger[0].at = 20s
*.manager.trigger[0].event = "disconnect"
*.manager.trigger[0].module = "router[1].ppg[2]"
*.manager.trigger[0].gate = "router[4].ppg[2]"

*.manager.trigger[1].at = 50s
*.manager.trigger[1].event = "connect"
*.manager.trigger[1].module = "router[1].ppg[2]"
*.manager.trigger[1].gate = "router[4].ppg[2]"
```

### S5: Multiple Failures
```ini
[Config S5_multi_fail]
description = "Two links fail simultaneously: R1-R2, R4-R5"
*.client.sendTime = 30s

*.manager.module = "ScenarioManager"
*.manager.trigger[0].at = 20s
*.manager.trigger[0].event = "disconnect"
*.manager.trigger[0].module = "router[0].ppg[0]"  # R1-R2
*.manager.trigger[0].gate = "router[1].ppg[0]"

*.manager.trigger[1].at = 20s
*.manager.trigger[1].event = "disconnect"
*.manager.trigger[1].module = "router[3].ppg[3]"  # R4-R5
*.manager.trigger[1].gate = "router[4].ppg[1]"
```

## 5. Output
- 5 [Config] sections trong `simulations/omnetpp.ini`

## 6. Acceptance Criteria
- Mỗi scenario chạy được với `opp_run -u Qtenv -c S1_steady`
- Link failure tại đúng thời điểm T
- OSPF phát hiện neighbor down (InactivityTimer)
- OSPF tính lại routing table

## 7. Related Tasks
- T-15 (.ini): base config
- T-09 (Flooding): phản ứng với topology change
- T-14 (Dump): đo convergence time

## 8. Notes
- Gate indices phải khớp với NED topology (T-02)
- ScenarioManager là built-in OMNeT++ module
- Dùng `opp_run -c ConfigName` để chọn scenario
