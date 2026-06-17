#!/usr/bin/env python3
"""Phân tích kết quả Phase 2b — Test Forwarding từ state_dump/2b/"""

import os, re, sys
from collections import Counter, defaultdict

STATE_DIR = os.path.join(os.path.dirname(__file__), "..", "state_dump", "2b")
RESULT_FILE = os.path.join(os.path.dirname(__file__), "2b_forwarding_test_result.txt")


def parse_logs(log_dir):
    """Đọc logTransition files và trích xuất các events với router ID."""
    test_sent = []   # (src, dst)
    rcvd = []        # (dst_router, src, cost) — dst_router từ filename
    fwd = []         # (forwarder_router_id, src, dst, gate, nextHop)
    errors = []

    if not os.path.isdir(log_dir):
        print(f"❌ Không tìm thấy thư mục {log_dir}")
        sys.exit(1)

    for fname in os.listdir(log_dir):
        if not fname.endswith(".log"):
            continue
        # Parse router ID từ filename: {seq}_r{routerId}_{ifIndex}.log
        m = re.match(r"\d+_r(\d+)_-?\d+\.log", fname)
        if not m:
            continue
        rid = int(m.group(1))

        path = os.path.join(log_dir, fname)
        with open(path, "r") as f:
            first_line = f.readline().strip()
            if not first_line:
                continue

            # Test:src->dst (từ initForwardingTest, ifIndex=-1)
            m = re.search(r"Test:(\d+)->(\d+)", first_line)
            if m:
                src, dst = int(m.group(1)), int(m.group(2))
                if rid == src:
                    test_sent.append((src, dst))
                continue

            # Fwd:src->dst g[gate]->nextHop
            m = re.search(r"Fwd:(\d+)->(\d+) g\[(\d+)\]->(\d+)", first_line)
            if m:
                src, dst, gate, nh = int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4))
                fwd.append((rid, src, dst, gate, nh))
                continue

            # Rcvd:src->self cost=X — xảy ra tại router = rid = dst
            m = re.search(r"Rcvd:(\d+)->self cost=(\d+)", first_line)
            if m:
                rcvd.append((rid, int(m.group(1)), int(m.group(2))))
                continue

            if "NoRoute" in first_line or "NHNotConn" in first_line:
                errors.append((rid, first_line))

    return test_sent, rcvd, fwd, errors


def build_paths(test_sent, fwd, rcvd):
    """
    Dựng đường đi cho 90 test cases.
    Path = [src, hop1, hop2, ..., dst] với cost chính xác.
    """
    paths = {}

    # Build lookup: (src, dst) -> list of (forwarder_rid, gate, nextHop)
    fwd_by_pair = defaultdict(list)
    for fwd_rid, src, dst, gate, nh in fwd:
        fwd_by_pair[(src, dst)].append((fwd_rid, gate, nh))

    # Build lookup: (src, dst) -> cost (từ Rcvd tại dst_router, với dst_router == dst)
    cost_by_pair = {}
    for dst_router, src, cost in rcvd:
        cost_by_pair[(src, dst_router)] = cost

    # Reconstruct: for each (src, dst), trace the path
    packet_number = 0
    for src, dst in sorted(test_sent, key=lambda x: (x[0], x[1])):
        packet_number += 1
        hops = fwd_by_pair.get((src, dst), [])

        # Follow the nextHop chain starting from src
        path = [src]
        hop_descs = []
        current_router = src

        for _ in range(15):  # max 15 hops safety
            matching = [h for h in hops if h[0] == current_router]
            if not matching:
                break
            fwd_rid, gate, nh = matching[0]
            hop_descs.append(f"g[{gate}]→R{nh}")
            path.append(nh)
            current_router = nh

        # Ensure destination is included
        if path[-1] != dst:
            path.append(dst)

        # Get cost
        cost = cost_by_pair.get((src, dst), "?")

        paths[(src, dst)] = {
            "path": path,
            "hop_descs": hop_descs,
            "path_str": " → ".join(f"R{r}" for r in path) + f" (cost={cost})",
            "cost": cost,
            "packet_num": packet_number,
        }

    return paths


def write_report(test_sent, rcvd, fwd, errors, out_path):
    sent_counter = Counter()
    for src, _ in test_sent:
        sent_counter[src] += 1

    costs = Counter()
    rcvd_by_src = Counter()
    for dst_router, src, cost in rcvd:
        rcvd_by_src[src] += 1
        costs[cost] += 1

    paths = build_paths(test_sent, fwd, rcvd)

    lines = []
    lines.append("=" * 60)
    lines.append("  2b — FORWARDING TEST RESULTS")
    lines.append("=" * 60)
    lines.append("")
    lines.append(f"  Simulation: General, t=11.1 (sau SPF hoàn tất)")
    lines.append("")

    # Tổng quan
    lines.append("─── OVERALL ───")
    lines.append(f"  Packets sent:      {len(test_sent)}")
    lines.append(f"  Packets received:  {len(rcvd)}")
    lines.append(f"  Errors (drop):     {len(errors)}")
    lines.append(f"  Forward actions:   {len(fwd)}")
    pass_rate = len(rcvd) * 100 // len(test_sent) if test_sent else 0
    lines.append(f"  PASS rate:         {pass_rate}% ({len(rcvd)}/{len(test_sent)})")
    lines.append("")

    # Per router
    lines.append("─── PER ROUTER ───")
    for rid in range(1, 11):
        s = sent_counter.get(rid, 0)
        r = rcvd_by_src.get(rid, 0)
        status = "✅" if s == r else "❌"
        lines.append(f"  R{rid}: sent={s}, received={r}  {status}")
    lines.append("")

    # Cost distribution
    lines.append("─── COST DISTRIBUTION ───")
    for cost in sorted(costs):
        bar = "█" * (costs[cost] // 2)
        lines.append(f"  cost={cost}: {costs[cost]} packets  {bar}")
    lines.append("")

    # Chi tiết 90 đường đi
    lines.append("=" * 60)
    lines.append("  CHI TIẾT 90 ĐƯỜNG ĐI")
    lines.append("=" * 60)
    lines.append("")

    current_src = 0
    for (src, dst), info in sorted(paths.items(), key=lambda x: (x[0][0], x[0][1])):
        if src != current_src:
            if current_src > 0:
                lines.append("")
            current_src = src
            lines.append(f"  ─── R{src} gửi ───")

        hop_details = ""
        if info["hop_descs"]:
            hop_details = " [" + ", ".join(info["hop_descs"][:1])
            for h in info["hop_descs"][1:]:
                hop_details += " → " + h
            hop_details += "]"

        lines.append(f"  T#{info['packet_num']:3d}  R{src} → R{dst}")
        path_str = " → ".join(f"R{r}" for r in info["path"])
        lines.append(f"         Path: {path_str}  cost={info['cost']}")
        lines.append("")

    # Forward summary
    lines.append("─── FORWARD STATS ───")
    router_fwd_count = Counter()
    for fwd_rid, src, dst, gate, nh in fwd:
        router_fwd_count[fwd_rid] += 1
    for rid in range(1, 11):
        count = router_fwd_count.get(rid, 0)
        if count > 0:
            gates = sorted(set(g for fr, _, _, g, _ in fwd if fr == rid))
            lines.append(f"  R{rid}: forwarded {count} packets (gate indices: {', '.join(str(g) for g in gates)})")
    lines.append("")

    # Errors
    if errors:
        lines.append("─── ERRORS ───")
        for rid, err in errors[:30]:
            lines.append(f"  R{rid}: {err}")

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w") as f:
        f.write("\n".join(lines) + "\n")
    print("\n".join(lines))
    print(f"\n📁 Đã ghi vào: {out_path}")


if __name__ == "__main__":
    test_sent, rcvd, fwd, errors = parse_logs(STATE_DIR)

    # Kiểm tra 90 test cases
    assert len(test_sent) == 90, f"Expected 90 sent, got {len(test_sent)}"
    assert len(rcvd) == 90, f"Expected 90 received, got {len(rcvd)}"
    assert len(errors) == 0, f"Expected 0 errors, got {len(errors)}"

    write_report(test_sent, rcvd, fwd, errors, RESULT_FILE)
    sys.exit(0)
