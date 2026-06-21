#!/usr/bin/env python3
"""OSPF Debug Chart Tool — sinh HTML chart từ log/ JSON dumps."""

import json
import glob
import os
import sys
from collections import defaultdict

# ── Constants ──────────────────────────────────────────────────
STATE_MAP = {
    "NBR_DOWN": 0, "NBR_ATTEMPT": 0, "NBR_INIT": 1,
    "NBR_TWOWAY": 2, "NBR_EXSTART": 3, "NBR_EXCHANGE": 4,
    "NBR_LOADING": 5, "NBR_FULL": 6
}
STATE_NAMES = ["DOWN", "INIT", "2WAY", "EXST", "EXCH", "LOAD", "FULL"]

ROUTER_COLORS = {
    "r1": "#e06c75", "r2": "#d19a66", "r3": "#e5c07b",
    "r4": "#98c379", "r5": "#56b6c2", "r6": "#61afef",
    "r7": "#c678dd", "r8": "#be5046", "r9": "#7ec8a0", "r10": "#528bff"
}


def parse_all_logs(log_dir="log"):
    """Quét log/**/*.json, trả về list snapshot thô (chưa sắp xếp)."""
    snapshots = []
    pattern = os.path.join(log_dir, "*", "*.json")
    files = sorted(glob.glob(pattern))

    if not files:
        print(f"ERROR: Không tìm thấy file JSON nào trong {log_dir}/")
        sys.exit(1)

    print(f"Tìm thấy {len(files)} file JSON...")

    for fpath in files:
        try:
            with open(fpath, "r") as f:
                d = json.load(f)
        except (json.JSONDecodeError, IOError) as e:
            print(f"  WARN: bỏ qua {fpath}: {e}")
            continue

        # Trích xuất router name từ đường dẫn: log/r3/123.json → r3
        router = os.path.basename(os.path.dirname(fpath))
        sim_time = d.get("simTime", 0)

        ifaces = []
        for iface in d.get("state", {}).get("interfaces", []):
            nbr = iface.get("neighbor")
            if not nbr:
                continue

            state_str = nbr.get("state", "NBR_DOWN")
            state_val = STATE_MAP.get(state_str, 0)
            dsl = len(nbr.get("databaseSummaryList", []))
            lsr = len(nbr.get("linkStateRequestList", []))
            ret = len(nbr.get("linkStateRetransmissionList", []))

            ifaces.append({
                "idx": iface.get("index", -1),
                "neighborId": nbr.get("IDNeighbor", "0.0.0.0"),
                "state": state_str,
                "stateVal": state_val,
                "dsl": dsl,
                "lsr": lsr,
                "ret": ret,
                "total": dsl + lsr + ret,
                "blocked": iface.get("linkDisabled", False)
            })

        snapshots.append({
            "simTime": sim_time,
            "router": router,
            "interfaces": ifaces
        })

    return snapshots


def build_timeline(snapshots):
    """Sắp xếp snapshot theo simTime, gán globalSeq."""
    snapshots.sort(key=lambda s: s["simTime"])

    timeline = []
    for seq, snap in enumerate(snapshots):
        snap["globalSeq"] = seq
        timeline.append(snap)

    print(f"Timeline: {len(timeline)} global sequence points")
    return timeline
