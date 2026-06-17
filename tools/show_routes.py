#!/usr/bin/env python3
"""
Đọc state_dump logs → in routing table của từng router.
Usage: python3 show_routes.py [state_dump_dir]
"""

import sys
import re
import os

def parse_log(filepath):
    """Đọc 1 file log → trả về (routerId, routing_table[])"""
    rid = None
    rt = []
    in_rt = False

    with open(filepath) as f:
        for line in f:
            m = re.match(r'Router ID:\s*(\d+)', line)
            if m: rid = int(m.group(1))

            m2 = re.match(r'--- Routing Table \((\d+)\) ---', line)
            if m2: in_rt = True; continue

            if in_rt:
                m3 = re.match(r'\s*\[\d+\]\s+dest=(0x[0-9a-f]+)\s+mask=(0x[0-9a-f]+)\s+pathType=(\d+)\s+cost=(\d+)\s+nextHop=(\d+)', line)
                if m3:
                    dest, cost, nh = int(m3.group(1), 16), int(m3.group(4)), int(m3.group(5))
                    mask = int(m3.group(2), 16)
                    if mask == 0:  # router entry
                        rt.append({'dest': dest, 'cost': cost, 'nh': nh, 'type': 'R'})
                elif line.strip() == '':
                    in_rt = False
    return rid, rt

def main():
    dump_dir = sys.argv[1] if len(sys.argv) > 1 else 'state_dump'

    # Tìm file finish (số lớn nhất cho mỗi router)
    files = {}
    for f in os.listdir(dump_dir):
        path = os.path.join(dump_dir, f)
        if not os.path.isfile(path) or not f.endswith('.log'): continue
        try:
            num = int(f.replace('.log', ''))
        except:
            continue
        rid, rt = parse_log(path)
        if rid and rt:
            if rid not in files or num > files[rid][0]:
                files[rid] = (num, path, rt)

    if not files:
        print(f"Không tìm thấy log files trong {dump_dir}/")
        sys.exit(1)

    # In routing table từng router
    for rid in sorted(files.keys()):
        _, path, rt = files[rid]
        print(f"{'='*55}")
        print(f"  Router R{rid}  ({path})")
        print(f"{'='*55}")
        print(f"  {'Đích':<8} {'Cost':<6} {'NextHop':<8} {'Path':<6}")
        print(f"  {'─'*8} {'─'*6} {'─'*8} {'─'*6}")
        for e in sorted(rt, key=lambda x: x['dest']):
            dest_s = f"R{e['dest']}" if e['dest'] <= 30 else f"IP{hex(e['dest'])}"
            nh_s = f"R{e['nh']}" if e['nh'] != 0 else "direct"
            print(f"  {dest_s:<8} {e['cost']:<6} {nh_s:<8} intra")
        print()

    # Thống kê
    total_entries = sum(len(rt) for _, _, rt in files.values())
    print(f"{'─'*55}")
    print(f"  Tổng: {len(files)} router, {total_entries} routing entries")

if __name__ == '__main__':
    main()
