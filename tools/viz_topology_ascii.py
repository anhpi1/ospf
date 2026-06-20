#!/usr/bin/env python3
"""
OSPF LSDB Topology Visualizer

Đọc file JSON dump OSPF, vẽ đồ thị topology từ LSDB (area.lsdb).
Kèm bảng định tuyến của mỗi router.

Usage:
    python3 tools/viz_topology_ascii.py log/10.json
    python3 tools/viz_topology_ascii.py --latest
"""

import json, argparse, os, sys, glob

FIXED_POSITIONS = {
    'R1':  (3, 3), 'R2':  (3, 10), 'R3':  (3, 17), 'R8':  (3, 24),
    'R4':  (7, 3), 'R5':  (7, 10), 'R9':  (7, 17), 'R10': (7, 24),
    'R6':  (11, 3), 'R7':  (11, 10),
}
ROWS, COLS = 18, 31

def ip2r(ip):
    return f"R{ip.split('.')[-1]}"

def parse_dump(path):
    """Đọc file JSON dump (1 router), trả về (lsdb_dict, rt_dict, ifaces_dict, simtimes_dict)."""
    with open(path) as f:
        data = json.load(f)

    lsdb = {}
    rt = {}
    ifaces = {}
    simtimes = {}

    # File JSON hiện tại chứa 1 router duy nhất (không phải mảng)
    router_obj = data
    name = ip2r(router_obj['routerId'])
    simtimes[name] = router_obj.get('simTime', 0)

    # LSDB: state.area.routerLSAs
    edges = set()
    for lsa in router_obj.get('state', {}).get('area', {}).get('routerLSAs', []):
        if lsa.get('header', {}).get('type') != 1:
            continue
        adv = ip2r(lsa['header']['advertisingRouter'])
        for link in lsa.get('links', []):
            if link.get('type') == 'LINK_P2P':
                edges.add(tuple(sorted((adv, ip2r(link['linkID'])))))
    lsdb[name] = edges

    # Routing table: state.routingTable
    rt[name] = router_obj.get('state', {}).get('routingTable', [])

    # Interfaces: state.interfaces
    ifaces[name] = router_obj.get('state', {}).get('interfaces', [])

    return lsdb, rt, ifaces, simtimes

class Canvas:
    def __init__(self, rows, cols):
        self.g = [[' ' for _ in range(cols)] for _ in range(rows)]

    def write(self, r, c, text):
        for i, ch in enumerate(text):
            if 0 <= r < len(self.g) and c + i < len(self.g[0]):
                self.g[r][c + i] = ch

    def border(self):
        r, c = len(self.g)-1, len(self.g[0])-1
        self.g[0][0] = '+'; self.g[0][c] = '+'
        self.g[r][0] = '+'; self.g[r][c] = '+'
        for i in range(1, c):
            self.g[0][i] = '-'; self.g[r][i] = '-'
        for i in range(1, r):
            self.g[i][0] = '|'; self.g[i][c] = '|'

    def dotline(self, r1, c1, r2, c2):
        dr, dc = r2 - r1, c2 - c1
        if dr == 0 and dc == 0: return
        steps = max(abs(dr), abs(dc))
        for i in range(1, steps):
            r = round(r1 + dr * i / steps)
            c = round(c1 + dc * i / steps)
            if 0 <= r < len(self.g) and 0 <= c < len(self.g[0]):
                if self.g[r][c] == ' ':
                    self.g[r][c] = '.'

    def draw_graph(self, edges, center):
        for u, v in edges:
            if u not in FIXED_POSITIONS or v not in FIXED_POSITIONS: continue
            ru, cu = FIXED_POSITIONS[u]; rv, cv = FIXED_POSITIONS[v]
            if ru <= rv: self.dotline(ru, cu, rv, cv)
            else: self.dotline(rv, cv, ru, cu)
        nodes_shown = set()
        for u, v in edges: nodes_shown.add(u); nodes_shown.add(v)
        for node in nodes_shown:
            r, c = FIXED_POSITIONS.get(node, (None, None))
            if r is None: continue
            self.write(r, c, 'R#' if node == center else node)
        if center not in nodes_shown:
            r, c = FIXED_POSITIONS[center]
            self.write(r, c, 'R#')

    def render(self):
        return '\n'.join(''.join(row) for row in self.g)

def fmt_dst(ip_str, dst_type):
    """Hiển thị destination: Router ID → Rx, network → giữ IP."""
    if dst_type == 'R':
        return ip2r(ip_str)
    return ip_str  # network: giữ nguyên IP

def fmt_nh(ip_str):
    """Hiển thị next-hop: 0.0.0.0 → self, còn lại → Rx."""
    if ip_str == '0.0.0.0':
        return 'self'
    return ip2r(ip_str)

def fmt_rt(entries):
    """Định dạng bảng định tuyến."""
    if not entries:
        return "  (rong)"
    lines = []
    lines.append(f"  {'Dich':<16} {'Loai':<4} {'Cost':<6} {'NextHop':<10}")
    lines.append(f"  {'-'*16} {'-'*4} {'-'*6} {'-'*10}")
    for e in entries:
        tp = e.get('destinationType', '?')
        # Xử lý cả format cũ (số) và format mới (chuỗi 'R'/'N')
        if isinstance(tp, int):
            tp = chr(tp) if tp in (78, 82) else str(tp)
        dst = fmt_dst(e.get('destinationId', '?'), tp)
        cost = e.get('cost', '?')
        nh = fmt_nh(e.get('nextHop', '?'))
        lines.append(f"  {dst:<16} {tp:<4} {cost:<6} {nh:<10}")
    return '\n'.join(lines)

def find_best_dumps(dumps_dir, ref_dump_path):
    """Tìm file JSON gần simTime nhất cho mỗi router (R1-R10) quanh thời điểm ref.
    Trả về dict {router_name: (file_path, sim_time, dump_num)}."""
    # Đọc simTime của file tham chiếu
    with open(ref_dump_path) as f:
        ref_data = json.load(f)
    ref_time = ref_data.get('simTime', 0)

    # Duyệt tất cả file, tìm file tốt nhất cho mỗi router
    best = {}  # router_name → (path, sim_time, best_delta, dump_num)

    for fpath in glob.glob(os.path.join(dumps_dir, '*.json')):
        try:
            with open(fpath) as f:
                data = json.load(f)
        except Exception:
            continue

        rid = data.get('routerId', '')
        t = data.get('simTime', 0)
        rname = ip2r(rid)
        dump_num = os.path.splitext(os.path.basename(fpath))[0]

        delta = abs(t - ref_time)
        if rname not in best or delta < best[rname][2]:
            best[rname] = (fpath, t, delta, dump_num, rid)

    # Trả về dict gọn hơn
    return {r: (p, t, dn) for r, (p, t, _d, dn, _rid) in best.items()}

def list_dumps(dumps_dir):
    files = sorted(glob.glob(os.path.join(dumps_dir, '*.json')),
                   key=lambda x: int(x.split('/')[-1].split('.')[0]))
    print(f"{'File':<20} {'LSAs':<8} {'Routes':<8} {'Trang thai'}")
    print('-' * 50)
    for fpath in files:
        fname = os.path.basename(fpath)
        try:
            with open(fpath) as f:
                data = json.load(f)
            # File JSON hiện tại: 1 router, LSDB nằm ở state.area.routerLSAs
            n_lsa = len(data.get('state', {}).get('area', {}).get('routerLSAs', []))
            n_rt = len(data.get('state', {}).get('routingTable', []))
            stt = 'Co routing' if n_rt > 0 else f'{n_lsa} LSA'
            print(f"{fname:<20} {n_lsa:<8} {n_rt:<8} {stt}")
        except Exception as e:
            print(f"{fname:<20} {'LOI':<8} {'':<8} {e}")

def main():
    p = argparse.ArgumentParser(description='Ve topology + bang dinh tuyen tu LSDB')
    p.add_argument('dump', nargs='?')
    p.add_argument('--latest', '-l', action='store_true')
    p.add_argument('--list', '-L', action='store_true')
    p.add_argument('--output', '-o')
    args = p.parse_args()

    dumps_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','log')
    if not os.path.isdir(dumps_dir):
        print(f"Loi: khong tim thay {dumps_dir}", file=sys.stderr); sys.exit(1)

    if args.list:
        list_dumps(dumps_dir); return

    dump_path = None
    if args.dump: dump_path = args.dump
    elif args.latest:
        dump_path = max(glob.glob(os.path.join(dumps_dir,'*.json')),
                        key=lambda x: int(x.split('/')[-1].split('.')[0]))
        print(f"Dung dump: {dump_path}")
    else:
        p.print_help(); return

    # Tìm file gần nhất theo simTime cho từng router (R1-R10)
    best = find_best_dumps(dumps_dir, dump_path)
    ref_name = ip2r(json.load(open(dump_path))['routerId'])
    ref_time = json.load(open(dump_path)).get('simTime', 0)
    print(f"Thoi diem tham chieu: t={ref_time:.2f}s (file {os.path.basename(dump_path)}, router {ref_name})")

    all_routers = sorted(FIXED_POSITIONS.keys())
    diagrams = []
    for center in all_routers:
        if center not in best:
            print(f"  Khong tim thay dump cho {center}")
            continue

        fpath, ftime, fdnum = best[center]
        lsdb, rt, ifaces, simtimes = parse_dump(fpath)
        edges = lsdb.get(center, set())

        c = Canvas(ROWS, COLS)
        c.draw_graph(edges, center)
        c.border()
        c.write(1, 2, f"t={ftime:.2f}s {center}")
        info = f"LSDB:{len(edges)} link  (dump {fdnum})"
        c.write(ROWS-2, 2, info)

        rt_text = f"Bang dinh tuyen {center}:\n{fmt_rt(rt.get(center, []))}"

        iface_lines = [f"Trang thai interface {center}:"]
        for ifc in ifaces.get(center, []):
            nb = ifc.get('neighbor', {})
            nb_id = nb.get('IDNeighbor', '?')
            nb_name = ip2r(nb_id) if nb_id != '?' else '?'
            st = nb.get('state', '?')
            cost = ifc.get('cost', '?')
            disabled = ifc.get('linkDisabled', False)
            d = ' (DOWN)' if disabled else ''
            iface_lines.append(f"  -> {nb_name}: {st}  cost={cost}{d}")
        iface_text = '\n'.join(iface_lines)

        marker = ' <--' if center == ref_name else ''
        diagrams.append(f"=== {center} === (dump {fdnum}{marker})\nt={ftime:.2f}s\n{c.render()}\n{rt_text}\n\n{iface_text}")

    out_path = args.output or f"topology_{os.path.splitext(os.path.basename(dump_path))[0]}.txt"
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write('\n\n'.join(diagrams) + '\n')
    print(f"Da tao {len(diagrams)} so do -> {out_path}")

if __name__ == '__main__':
    main()
