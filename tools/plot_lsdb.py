#!/usr/bin/env python3
"""
Vẽ đồ thị OSPF từ LSDB trong state_dump log file.
Usage: python3 plot_lsdb.py <log_file> [output.png]
"""

import sys
import re
import subprocess
import os

def parse_lsdb(filepath):
    """Đọc file log → trả về dict LSA: advRouter → [(linkID, cost)]"""
    lsdb = {}
    current_adv = None

    with open(filepath, 'r') as f:
        for line in f:
            # VD: [LSA] adv=1 seq=0x80000003 age=0 links=4
            m = re.match(r'\s*\[LSA\]\s+adv=(\S+)', line)
            if m:
                current_adv = int(m.group(1), 16) if m.group(1).startswith('0x') else int(m.group(1))
                if current_adv not in lsdb:
                    lsdb[current_adv] = []
                continue

            # VD: [P2P] id=0x2 data=0x1 metric=1
            m2 = re.match(r'\s*\[P2P\]\s+id=(\S+).*metric=(\S+)', line)
            if m2 and current_adv is not None:
                nbr = int(m2.group(1), 16) if m2.group(1).startswith('0x') else int(m2.group(1))
                cost = int(m2.group(2), 16) if m2.group(2).startswith('0x') else int(m2.group(2))
                lsdb[current_adv].append((nbr, cost))

    return lsdb

def write_dot(lsdb, dot_path):
    """Tạo file DOT từ LSDB"""
    # Gom các cạnh (undirected, lấy cost nhỏ nhất)
    edges = {}
    for adv, links in lsdb.items():
        for nbr, cost in links:
            if nbr > 0:
                key = (min(adv, nbr), max(adv, nbr))
                if key not in edges or cost < edges[key]:
                    edges[key] = cost

    with open(dot_path, 'w') as f:
        f.write('graph OSPF {\n')
        f.write('  rankdir=LR;\n')
        f.write('  node [shape=circle, style=filled, fillcolor=lightyellow, fontsize=14, width=0.6];\n')
        f.write('  edge [fontsize=12, penwidth=2];\n\n')

        for (a, b), cost in sorted(edges.items()):
            f.write(f'  n{a} -- n{b} [label="cost={cost}", len=2];\n')

        f.write('}\n')

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <state_dump_log> [output.png]")
        sys.exit(1)

    log_path = sys.argv[1]
    out_path = sys.argv[2] if len(sys.argv) > 2 else 'ospf_topology.png'
    dot_path = out_path.replace('.png', '.dot')

    if not os.path.exists(log_path):
        print(f"File not found: {log_path}")
        sys.exit(1)

    lsdb = parse_lsdb(log_path)
    print(f"Parsed {len(lsdb)} routers from LSDB:")
    for adv in sorted(lsdb.keys()):
        links = ', '.join(f"→R{nbr}(c={c})" for nbr, c in lsdb[adv])
        print(f"  R{adv}: {links}")

   # Số liên kết P2P
    p2p_count = sum(1 for links in lsdb.values() for nbr, c in links if nbr > 0)
    print(f"  Total P2P links: {p2p_count}")

    write_dot(lsdb, dot_path)
    print(f"\nDOT file: {dot_path}")

    # Render PNG
    try:
        subprocess.run(['dot', '-Tpng', dot_path, '-o', out_path], check=True)
        print(f"Image: {out_path}")
    except FileNotFoundError:
        print("Graphviz 'dot' not found — install with: sudo apt install graphviz")
    except subprocess.CalledProcessError as e:
        print(f"dot error: {e}")

if __name__ == '__main__':
    main()
