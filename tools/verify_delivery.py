#!/usr/bin/env python3
"""
verify_delivery.py — Kiểm tra delivery data packets từ bin/ và log/

Đọc tất cả bin files, xác định gói tin data (8-byte), tái tạo đường đi
và kiểm tra delivery cho từng cặp (src, dest) tại mỗi event group.

Usage:
  python3 tools/verify_delivery.py                  # ghi delivery_report.txt
  python3 tools/verify_delivery.py --events 200,400 # lọc event 200, 400
"""

import argparse
import contextlib
import glob
import os
import struct
import sys
from collections import defaultdict

# ── Topology interface map (từ ospf.ned) ────────────────────────────
# IFACE_MAP[router][ifIndex] = neighbor_router
IFACE_MAP = {
    'r1':  {0: 'r2', 1: 'r4'},
    'r2':  {0: 'r1', 1: 'r3', 2: 'r4', 3: 'r5'},
    'r3':  {0: 'r2', 1: 'r8', 2: 'r9'},
    'r4':  {0: 'r1', 1: 'r2', 2: 'r5', 3: 'r6'},
    'r5':  {0: 'r2', 1: 'r4', 2: 'r7', 3: 'r9'},
    'r6':  {0: 'r4'},
    'r7':  {0: 'r5'},
    'r8':  {0: 'r3', 1: 'r10'},
    'r9':  {0: 'r3', 1: 'r5'},
    'r10': {0: 'r8'},
}

# Reverse map: NEIGHBOR_IFACE[router][neighbor] = ifIndex
NEIGHBOR_IFACE = {}
for r, imap in IFACE_MAP.items():
    NEIGHBOR_IFACE[r] = {v: k for k, v in imap.items()}

# Topology edges cho path validation
TOPO_EDGES = set()
for r, imap in IFACE_MAP.items():
    for nbr in imap.values():
        TOPO_EDGES.add(tuple(sorted((r, nbr))))


def parse_path(filepath):
    """Trích router, interface, nbr_state, seq, simTime từ đường dẫn bin file.

    Format: bin/<router>/<iface>[/<nbr_state>]/<seq>_<simTime>.bin
    Ví dụ:  bin/r1/if0/NBR_FULL/000001_100_010000.bin
            bin/r1/client/000001_100_010000.bin
    """
    rel = os.path.relpath(filepath, 'bin')
    parts = rel.replace(os.sep, '/').split('/')

    router = parts[0] if len(parts) >= 1 else '?'
    iface = '?'
    nbr_state = '-'
    seq = 0
    sim_time = 0.0

    fname = parts[-1]
    # Parse seq và simTime từ filename
    if '_' in fname:
        seq_str = fname.split('_')[0]
        seq = int(seq_str) if seq_str.isdigit() else 0
        time_part = fname[len(seq_str)+1:].replace('.bin', '').replace('_', '.', 1)
        try:
            sim_time = float(time_part)
        except ValueError:
            sim_time = 0.0

    # Xác định interface và nbr_state từ path
    if len(parts) >= 3:
        if parts[-2] == 'client':
            iface = 'client'
            nbr_state = '-'
        elif parts[-2].startswith('if'):
            iface = parts[-2]
            nbr_state = parts[-3] if len(parts) >= 4 else '?'

    return router, iface, nbr_state, seq, sim_time


def get_interface_index(iface_str):
    """if0 -> 0, if1 -> 1, client -> -1"""
    if iface_str.startswith('if'):
        return int(iface_str[2:])
    return -1


def router_id_to_name(rid):
    """1 -> 'r1', 10 -> 'r10'"""
    return f'r{rid}'


def scan_bin_files(bin_dir='bin', event_filter=None):
    """Quét tất cả bin files, trả về dict:
    data_packets[event_time][(src, dest)] = [observation, ...]

    observation = {
        'router': 'r1', 'iface': 'if0', 'nbr_state': 'NBR_FULL',
        'sim_time': 100.01, 'seq': 123
    }
    """
    all_bins = sorted(glob.glob(os.path.join(bin_dir, '**', '*.bin'), recursive=True))

    data_packets = defaultdict(lambda: defaultdict(list))  # event -> (src,dest) -> [obs]
    total_data = 0

    for fp in all_bins:
        with open(fp, 'rb') as f:
            payload = f.read()

        if len(payload) != 8:
            continue  # skip OSPF protocol packets

        # Parse dest, src
        dest = struct.unpack_from('!I', payload, 0)[0]
        src = struct.unpack_from('!I', payload, 4)[0]
        total_data += 1

        # Parse metadata
        router, iface, nbr_state, seq, sim_time = parse_path(fp)

        # Xác định event group (làm tròn đến giây)
        event_time = round(sim_time)

        # Lọc nếu có event_filter
        if event_filter is not None and event_time not in event_filter:
            continue

        obs = {
            'router': router,
            'iface': iface,
            'nbr_state': nbr_state,
            'sim_time': sim_time,
            'seq': seq,
            'filepath': fp,
        }
        data_packets[event_time][(src, dest)].append(obs)

    # Sắp xếp observations theo simTime trong mỗi cặp
    for event in data_packets:
        for key in data_packets[event]:
            data_packets[event][key].sort(key=lambda o: o['sim_time'])

    return data_packets, total_data


def reconstruct_path(obs_list, src_router, dest_router):
    """Tái tạo đường đi từ observations.

    De-duplicate: bỏ observation trùng router, giữ cái đầu tiên.
    Xác định delivery: có observation tại dest_router.

    Trả về: (path_list, hops, status)
    - path_list: ['r1', 'r2', 'r3', ...]
    - hops: số lần forward (len(path) - 1)
    - status: 'DELIVERED' nếu có observation ở dest_router
             'DROPPED' nếu không
    """
    # De-duplicate: keep first observation per router
    seen_routers = set()
    deduped = []
    for obs in obs_list:
        if obs['router'] not in seen_routers:
            seen_routers.add(obs['router'])
            deduped.append(obs)

    # Build path: src → mỗi router duy nhất theo thứ tự xuất hiện
    path = [src_router]
    for obs in deduped:
        if obs['router'] != src_router:
            path.append(obs['router'])

    delivered = dest_router in path
    if delivered and path[-1] != dest_router:
        path.append(dest_router)

    hops = len(path) - 1
    status = 'DELIVERED' if delivered else 'DROPPED'

    return path, hops, status


def find_event_groups(bin_dir='bin'):
    """Tự động phát hiện event groups (chỉ data packet 8-byte)."""
    all_bins = glob.glob(os.path.join(bin_dir, '**', '*.bin'), recursive=True)
    events = set()
    for fp in all_bins:
        with open(fp, 'rb') as f:
            payload = f.read()
        if len(payload) != 8:
            continue  # chỉ data packet
        _, _, _, _, sim_time = parse_path(fp)
        events.add(round(sim_time))
    return sorted(events)


def print_report(data_packets, event_filter=None, num_routers=10, out=sys.stdout):
    """In báo cáo phân tích delivery."""
    expected_pairs = num_routers * (num_routers - 1)  # 10 * 9 = 90

    for event_time in sorted(data_packets.keys()):
        if event_filter and event_time not in event_filter:
            continue

        pairs = data_packets[event_time]
        observed_pairs = set()
        for (src, dest), obs_list in pairs.items():
            src_name = router_id_to_name(src)
            dest_name = router_id_to_name(dest)
            path, hops, status = reconstruct_path(obs_list, src_name, dest_name)
            observed_pairs.add((src, dest, src_name, dest_name, tuple(path), hops, status, len(obs_list)))

        # Xác định missing pairs (không có observation nào)
        all_expected = set()
        for src in range(1, num_routers + 1):
            for dest in range(1, num_routers + 1):
                if src == dest: continue
                all_expected.add((src, dest))

        observed_keys = set((s, d) for s, d, _, _, _, _, _, _ in observed_pairs)
        missing = all_expected - observed_keys
        delivered_count = sum(1 for _, _, _, _, _, _, st, _ in observed_pairs if st == 'DELIVERED')
        dropped_count = len(missing) + sum(1 for _, _, _, _, _, _, st, _ in observed_pairs if st == 'DROPPED')

        print(f"\n{'='*78}")
        print(f"  EVENT GROUP: t ≈ {event_time}s")
        print(f"  Kỳ vọng: {expected_pairs} cặp (src→dest)  |  Phát hiện: {len(observed_pairs)} cặp  |  Mất: {len(missing)} cặp")
        print(f"{'='*78}")

        # Summary line
        total_delivered = delivered_count
        total_dropped = dropped_count
        print(f"\n  Kết quả: {total_delivered}/{expected_pairs} DELIVERED ({100*total_delivered/expected_pairs:.1f}%)")
        print(f"           {total_dropped}/{expected_pairs} DROPPED ({100*total_dropped/expected_pairs:.1f}%)")
        if missing:
            # Nhóm missing theo src để dễ đọc
            from collections import defaultdict
            missing_by_src = defaultdict(list)
            for s, d in sorted(missing):
                missing_by_src[router_id_to_name(s)].append(router_id_to_name(d))
            print(f"\n  Cặp bị mất (không có observation nào - router không có route):")
            for src_name, dests in sorted(missing_by_src.items()):
                print(f"    {src_name} → {', '.join(dests)}")

        # ── Bảng chi tiết ──
        print(f"\n  {'Src':<5} {'Dest':<5} {'Status':<10} {'Hops':<5} {'Obs':<4} Path")
        print(f"  {'-'*5} {'-'*5} {'-'*10} {'-'*5} {'-'*4} {'-'*40}")

        for src, dest, src_name, dest_name, path, hops, status, nobs in sorted(observed_pairs):
            path_str = ' → '.join(path[:6])
            if len(path) > 6:
                path_str += '...'
            if status == 'DELIVERED':
                path_str += ' ✓'
            print(f"  {src_name:<5} {dest_name:<5} {status:<10} {hops:<5} {nobs:<4} {path_str}")

        # Thêm missing pairs vào bảng
        for src, dest in sorted(missing):
            src_name = router_id_to_name(src)
            dest_name = router_id_to_name(dest)
            print(f"  {src_name:<5} {dest_name:<5} {'DROPPED':<10} {'-':<5} {'0':<4} (no route)")

        # ── Thống kê theo src ──
        print(f"\n  Thống kê theo nguồn (source router):")
        print(f"  {'Src':<5} {'Kỳ vọng':<8} {'Delivered':<10} {'Dropped':<9} {'Rate':<7}")
        print(f"  {'-'*5} {'-'*8} {'-'*10} {'-'*9} {'-'*7}")
        for src_id in range(1, num_routers + 1):
            src_name = router_id_to_name(src_id)
            sent = num_routers - 1  # 9 destinations
            # Delivered: từ observed_pairs
            delivered_src = sum(1 for s,d,_,_,_,_,st,_ in observed_pairs if s==src_id and st=='DELIVERED')
            # Missing từ src này
            missing_src = sum(1 for s,d in missing if s==src_id)
            dropped_src = (sent - delivered_src)
            rate = 100 * delivered_src / sent if sent else 0
            print(f"  {src_name:<5} {sent:<8} {delivered_src:<10} {dropped_src:<9} {rate:<6.0f}%")

        # ── Thống kê hops ──
        hops_list = [h for _,_,_,_,_,h,st,_ in observed_pairs if st == 'DELIVERED']
        if hops_list:
            print(f"\n  Thống kê hops (DELIVERED):")
            print(f"    Min: {min(hops_list)}   Max: {max(hops_list)}   Avg: {sum(hops_list)/len(hops_list):.1f}")


def count_bin_files(bin_dir='bin'):
    """Đếm tổng số bin files và data packets."""
    all_bins = sorted(glob.glob(os.path.join(bin_dir, '**', '*.bin'), recursive=True))
    total = len(all_bins)
    data_count = 0
    for fp in all_bins:
        with open(fp, 'rb') as f:
            if len(f.read()) == 8:
                data_count += 1
    return total, data_count


def main():
    parser = argparse.ArgumentParser(
        description='Kiểm tra delivery data packets từ bin files'
    )
    parser.add_argument('--events', '-e',
                        help='Event times cần phân tích, cách dấu phẩy (vd: 100,300)')
    parser.add_argument('--list-events', '-l', action='store_true',
                        help='Liệt kê event groups có trong bin files')
    parser.add_argument('--bin-dir', default='bin',
                        help='Thư mục bin files (default: bin)')
    args = parser.parse_args()

    # Count
    total_bins, data_count = count_bin_files(args.bin_dir)

    # Liệt kê events
    if args.list_events:
        events = find_event_groups(args.bin_dir)
        print(f"Bin files: {total_bins} total, {data_count} data packets (8-byte)")
        print(f"Event groups (rounded simTime): {', '.join(str(e) for e in events)}")
        return

    # Parse event filter
    event_filter = None
    if args.events:
        event_filter = set(int(e.strip()) for e in args.events.split(','))

    # Scan + analyze
    print(f"Quét bin files từ '{args.bin_dir}/' ...")
    data_packets, total_data = scan_bin_files(args.bin_dir, event_filter)

    event_count = len(data_packets)

    print(f"  Tổng bin: {total_bins}  |  Data packets (8-byte): {total_data}  |  Event groups: {event_count}")

    if event_count == 0:
        print("\nKhông tìm thấy event group nào. Kiểm tra --events hoặc --bin-dir.")
        return

    # Ghi tất cả event vào 1 file
    outfile = 'delivery_report.txt'
    with open(outfile, 'w', encoding='utf-8') as f:
        with contextlib.redirect_stdout(f):
            print_report(data_packets, event_filter)

    print(f"Đã ghi báo cáo vào '{outfile}' ({os.path.getsize(outfile)} bytes)")


if __name__ == '__main__':
    main()
