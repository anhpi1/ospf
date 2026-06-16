#!/usr/bin/env python3
"""Đọc log cuối cùng trong state_dump/, in danh sách link từng router."""

import os, re, sys
from collections import defaultdict

RE_LSA  = re.compile(r'\[LSA\] adv=(\S+) seq=(\S+) age=(\S+) links=(\S+)')
RE_LINK = re.compile(r'\[(Stub|P2P)\] id=(\S+) data=(\S+) metric=(\S+)')
RE_RID  = re.compile(r'Router ID:\s*(\S+)')

def parse_dump(path):
    with open(path) as f:
        text = f.read()
    m = RE_RID.search(text)
    router_id = m.group(1) if m else "?"
    lsas = []
    for line in text.splitlines():
        m = RE_LSA.search(line)
        if m:
            lsas.append({"adv": m.group(1), "links": [], "seq": m.group(2)})
        m = RE_LINK.search(line)
        if m and lsas:
            lsas[-1]["links"].append({"type": m.group(1), "id": m.group(2),
                                       "data": m.group(3), "metric": m.group(4)})
    return router_id, lsas

# Tìm file log cuối cùng
dump_dir = "state_dump"
files = [f for f in os.listdir(dump_dir) if f.endswith(".log") and f.split(".")[0].isdigit()]
if not files:
    print("Không tìm thấy file log trong state_dump/")
    sys.exit(1)

last_file = sorted(files, key=lambda x: int(x.split(".")[0]))[-1]
path = os.path.join(dump_dir, last_file)

router_id, lsas = parse_dump(path)
print(f"Trạng thái cuối: file={last_file}, router={router_id}\n")

# Gom theo adv router
by_adv = defaultdict(list)
for lsa in lsas:
    by_adv[lsa["adv"]].extend(lsa["links"])

print("=" * 65)
print(f"{'Router':<8} {'Stub links':<35} {'P2P links'}")
print("=" * 65)
for adv in sorted(by_adv.keys(), key=lambda x: int(x)):
    links = by_adv[adv]
    stubs = []
    p2ps = []
    for ln in links:
        if ln["type"] == "Stub":
            nid = int(ln["id"], 16) if ln["id"].startswith("0x") else ln["id"]
            stubs.append(str(nid))
        else:
            p2ps.append(f"R{int(ln['id'],16)}(c{ln['metric']})"
                        if ln["id"].startswith("0x") else f"R{ln['id']}(c{ln['metric']})")
    stub_str = ", ".join(stubs) if stubs else "-"
    p2p_str = ", ".join(p2ps) if p2ps else "-"
    print(f"R{adv:<5}  {stub_str:<35} {p2p_str}")

print()

# Tổng số link
total_stub = sum(1 for ln in by_adv.values() for l in ln if l["type"] == "Stub")
total_p2p  = sum(1 for ln in by_adv.values() for l in ln if l["type"] == "P2P")
print(f"Tổng Stub links: {total_stub}, P2P links: {total_p2p}")
