#!/usr/bin/env python3
"""
OSPF LSDB Topology Visualizer

Đọc file JSON dump OSPF, vẽ đồ thị topology từ LSDB (area.lsdb).
Kèm bảng định tuyến của mỗi router.

Usage:
    python3 tools/viz_topology_ascii.py log/r1/10.json
    python3 tools/viz_topology_ascii.py --latest
"""

import json, argparse, os, sys, glob
from concurrent.futures import ThreadPoolExecutor, as_completed

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
    # p2p_map: adv_router → set of P2P neighbor IDs (để kiểm tra 2 chiều)
    p2p_map = {}  # "R1" → {"R2", "R4"}
    for lsa in router_obj.get('state', {}).get('area', {}).get('routerLSAs', []):
        if lsa.get('header', {}).get('type') != 1:
            continue
        adv = ip2r(lsa['header']['advertisingRouter'])
        p2p_set = set()
        for link in lsa.get('links', []):
            if link.get('type') == 'LINK_P2P':
                nb = ip2r(link['linkID'])
                p2p_set.add(nb)
        p2p_map[adv] = p2p_set

    # Xây edges từ LSDB: chỉ giữ link nếu CẢ 2 phía cùng xác nhận (bidirectional)
    edges = set()
    for adv, neighbors in p2p_map.items():
        for nb in neighbors:
            # Kiểm tra bidirectional: nb cũng phải có P2P link đến adv
            if nb in p2p_map and adv in p2p_map[nb]:
                edges.add(tuple(sorted((adv, nb))))
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
        # Bỏ qua route type N (network) — các interface chưa có IP riêng
        # nên network route trùng với router ID, gây nhiễu
        if tp == 'N':
            continue
        dst = fmt_dst(e.get('destinationId', '?'), tp)
        cost = e.get('cost', '?')
        nh = fmt_nh(e.get('nextHop', '?'))
        lines.append(f"  {dst:<16} {tp:<4} {cost:<6} {nh:<10}")
    return '\n'.join(lines)

def find_all_jsons(dumps_dir):
    """Trả về tất cả file JSON trong dumps_dir: cả flat (cũ) lẫn thư mục con (mới)."""
    # Cấu trúc mới: log/r1/xxx.json
    files = glob.glob(os.path.join(dumps_dir, '*', '*.json'))
    # Cấu trúc cũ: log/xxx.json (flat)
    files.extend(glob.glob(os.path.join(dumps_dir, '*.json')))
    return files

def find_best_dumps(dumps_dir, ref_dump_path):
    """Tìm file JSON gần simTime nhất cho mỗi router (R1-R10) quanh thời điểm ref.
    Trả về dict {router_name: (file_path, sim_time, dump_num)}."""
    # Đọc simTime của file tham chiếu
    with open(ref_dump_path) as f:
        ref_data = json.load(f)
    ref_time = ref_data.get('simTime', 0)

    # Duyệt tất cả file, tìm file tốt nhất cho mỗi router
    best = {}  # router_name → (path, sim_time, best_delta, dump_num)

    for fpath in find_all_jsons(dumps_dir):
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
    files = sorted(glob.glob(os.path.join(dumps_dir, '*', '*.json')),
                   key=lambda x: int(x.split('/')[-1].split('.')[0]))
    print(f"{'File':<24} {'LSAs':<8} {'Routes':<8} {'Trang thai'}")
    print('-' * 54)
    for fpath in files:
        router_dir = os.path.basename(os.path.dirname(fpath))
        fname = os.path.basename(fpath)
        display_name = f"{router_dir}/{fname}"
        try:
            with open(fpath) as f:
                data = json.load(f)
            # File JSON hiện tại: 1 router, LSDB nằm ở state.area.routerLSAs
            n_lsa = len(data.get('state', {}).get('area', {}).get('routerLSAs', []))
            n_rt = len(data.get('state', {}).get('routingTable', []))
            stt = 'Co routing' if n_rt > 0 else f'{n_lsa} LSA'
            print(f"{display_name:<24} {n_lsa:<8} {n_rt:<8} {stt}")
        except Exception as e:
            print(f"{display_name:<24} {'LOI':<8} {'':<8} {e}")

def process_single_file(json_path, out_path):
    """Xử lý 1 file JSON của 1 router, vẽ topology cho router đó, ghi ra out_path."""
    lsdb, rt, ifaces, simtimes = parse_dump(json_path)

    # Lấy tên router từ dữ liệu trong file
    with open(json_path) as f:
        data = json.load(f)
    center = ip2r(data['routerId'])
    ftime = data.get('simTime', 0)

    edges = lsdb.get(center, set())

    c = Canvas(ROWS, COLS)
    c.draw_graph(edges, center)
    c.border()
    c.write(1, 2, f"t={ftime:.2f}s {center}")
    fdnum = os.path.splitext(os.path.basename(json_path))[0]
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

    diagram = f"=== {center} === (dump {fdnum})\nt={ftime:.2f}s\n{c.render()}\n{rt_text}\n\n{iface_text}"

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(diagram + '\n')
    return 1


def process_seq(dump_path, dumps_dir, out_path):
    """Xử lý 1 seq: vẽ topology cho tất cả router (merge mode - giữ cho single-seq)."""
    best = find_best_dumps(dumps_dir, dump_path)
    ref_name = ip2r(json.load(open(dump_path))['routerId'])
    ref_time = json.load(open(dump_path)).get('simTime', 0)

    all_routers = sorted(FIXED_POSITIONS.keys())
    diagrams = []
    for center in all_routers:
        if center not in best:
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
        diagrams.append(
            f"=== {center} === (dump {fdnum}{marker})\n"
            f"t={ftime:.2f}s\n{c.render()}\n{rt_text}\n\n{iface_text}"
        )

    with open(out_path, 'w', encoding='utf-8') as f:
        f.write('\n\n'.join(diagrams) + '\n')
    return len(diagrams)


def run_all(dumps_dir, out_dir, workers=8):
    """Quét tất cả file JSON trong log/, sinh output song song ra out_dir/,
    mirror cấu trúc thư mục: resultlog/<router>/<seq>.txt"""
    os.makedirs(out_dir, exist_ok=True)

    # Thu thập tất cả file JSON
    all_files = find_all_jsons(dumps_dir)
    total = len(all_files)
    print(f"Tìm thấy {total} file JSON. Bắt đầu sinh với {workers} worker...",
          file=sys.stderr, flush=True)

    done = [0]
    lock = __import__('threading').Lock()

    def worker(fpath):
        # Xác định router và seq từ đường dẫn
        # log/r1/500.json → router=r1, seq=500
        rel = os.path.relpath(fpath, dumps_dir)
        router_dir = os.path.dirname(rel)   # vd: "r1"
        fname = os.path.basename(rel)        # vd: "500.json"
        seq = os.path.splitext(fname)[0]     # vd: "500"
        out_path = os.path.join(out_dir, router_dir, f"{seq}.txt")

        if os.path.exists(out_path):
            with lock:
                done[0] += 1
            return fpath, True, 0

        try:
            n = process_single_file(fpath, out_path)
            with lock:
                done[0] += 1
                if done[0] % 200 == 0:
                    print(f"  Viz: {done[0]}/{total} ({100*done[0]//total}%)",
                          file=sys.stderr, flush=True)
            return fpath, True, n
        except Exception as e:
            with lock:
                done[0] += 1
            return fpath, False, str(e)

    with ThreadPoolExecutor(max_workers=workers) as pool:
        futures = [pool.submit(worker, fp) for fp in sorted(all_files)]
        errors = 0
        for f in as_completed(futures):
            fpath, ok, info = f.result()
            if not ok:
                errors += 1
                print(f"  [LỖI] {fpath}: {info}", file=sys.stderr)

    print(f"  Viz done: {total} file -> {out_dir}/  (lỗi: {errors})", file=sys.stderr)


def main():
    p = argparse.ArgumentParser(description='Ve topology + bang dinh tuyen tu LSDB')
    p.add_argument('dump', nargs='?')
    p.add_argument('--latest', '-l', action='store_true')
    p.add_argument('--list', '-L', action='store_true')
    p.add_argument('--output', '-o')
    p.add_argument('--all', action='store_true',
                   help='Sinh toan bo output cho tat ca seq ra resultlog/')
    p.add_argument('--workers', type=int, default=8,
                   help='So worker cho --all (default: 8)')
    args = p.parse_args()

    dumps_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'log')
    if not os.path.isdir(dumps_dir):
        print(f"Loi: khong tim thay {dumps_dir}", file=sys.stderr); sys.exit(1)

    if args.list:
        list_dumps(dumps_dir); return

    if args.all:
        out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'resultlog')
        run_all(dumps_dir, out_dir, args.workers)
        return

    dump_path = None
    if args.dump:
        if args.dump.isdigit():
            matched = glob.glob(os.path.join(dumps_dir, '*', f'{args.dump}.json'))
            if not matched:
                matched = glob.glob(os.path.join(dumps_dir, f'{args.dump}.json'))
            if matched:
                dump_path = matched[0]
            else:
                print(f"Loi: khong tim thay file co so thu tu {args.dump}", file=sys.stderr)
                sys.exit(1)
        else:
            dump_path = args.dump
    elif args.latest:
        dump_path = max(glob.glob(os.path.join(dumps_dir, '*', '*.json')),
                        key=lambda x: int(x.split('/')[-1].split('.')[0]))
        print(f"Dung dump: {dump_path}")
    else:
        p.print_help(); return

    out_path = args.output or f"topology_{os.path.splitext(os.path.basename(dump_path))[0]}.txt"
    n = process_seq(dump_path, dumps_dir, out_path)
    print(f"Da tao {n} so do -> {out_path}")

if __name__ == '__main__':
    main()
