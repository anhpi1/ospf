# Task Spec: T-17 — Integration Test + Verification

> Chạy toàn bộ hệ thống, verify tất cả success criteria.

---

## 1. Overview
Kiểm tra toàn diện: compile, run tất cả 5 scenarios, verify outputs.

## 2. Requirements
- All tasks T-01→T-16 completed
- Target: tất cả success criteria từ Design Brief

## 3. Input
- Compiled binary
- 5 scenario configs
- Design Brief success criteria

## 4. Process

**Step 1: Build**
```bash
cd ospf-sim
make clean && make
```

**Step 2: Quick syntax check**
```bash
opp_nedtool -n simulations simulations/OspfNetwork.ned
opp_msgc src/OspfPacket.msg
```

**Step 3: Run scenarios**
```bash
# Command-line run for all scenarios (save results)
for cfg in S1_steady S2_leaf_fail S3_backbone_fail S4_recovery S5_multi_fail; do
    opp_run -l ../src/ospf_sim -n simulations:. -u Cmdenv \
            -c $cfg --vector-recording=false \
            > results/${cfg}.out 2>&1
done
```

**Step 4: Verification checklist**

| # | Check | Pass/Fail |
|---|---|---|
| SC-01 | 10 routers reach Full state | |
| SC-02 | LSDB synchronized (all routers have same entries) | |
| SC-03 | Routing table shortest path correct (verify manually) | |
| SC-04 | ECMP if equal costs exist (≥2 next-hops) | |
| SC-05 | Data packet R6→R10 delivered | |
| SC-06 | S2: R6 leaf failure → routing table updates | |
| SC-07 | S3: R2-R5 failure → routes recalculate | |
| SC-08 | S4: recovery → routing restores original | |
| SC-09 | S5: multi-fail → OSPF handles concurrent failures | |
| SC-10 | transaction.log has format `[TX-001] T=...` | |
| SC-11 | content.log has format `[CONTENT] T=...` | |
| SC-12 | Convergence time measured in all scenarios | |

**Step 5: Analyze outputs**
- Check transaction.log: mỗi action unique TX ID
- Check content.log: fields correct
- Compare routing dumps between scenarios
- Calculate convergence times

## 5. Output
- Verified results (routing tables correct, logs correct)
- `results/` directory with all logs + dumps

## 6. Acceptance Criteria
- Tất cả 12 checks (SC-01→SC-12) đều Pass
- Không crash simulation
- Transaction + content logs đúng format

## 7. Related Tasks
- T-01→T-16: tất cả

## 8. Notes
- Chạy Cmdenv cho batch (không GUI) để test nhanh
- Dùng Qtenv (`-u Qtenv`) cho 1 scenario để visual verify
- Nếu fail → debug: Qtenv để xem state machines, LSDB, routing table
