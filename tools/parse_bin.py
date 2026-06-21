#!/usr/bin/env python3
"""
Công cụ parse gói tin OSPF từ file .bin, hiển thị theo đúng RFC 2328 format
dạng đệ quy: tên field bên trái, giá trị bên phải trên cùng một dòng.
Cấu trúc lồng (LSA header, LSA body) hiển thị lùi vào với khung +-...-+.

Usage:
  python3 tools/parse_bin.py 1000                     # tìm theo số thứ tự
  python3 tools/parse_bin.py bin/000001_0_010000.bin  # đường dẫn cụ thể
  python3 tools/parse_bin.py bin/*.bin                # batch mode
"""

import struct, sys, os, argparse
import glob as _glob
from concurrent.futures import ThreadPoolExecutor, as_completed
import subprocess

SCRIPT_PATH = os.path.abspath(__file__)
PROJECT_ROOT = os.path.dirname(os.path.dirname(SCRIPT_PATH))


# ═══════════════════════════════════════════════════════════════════
# Helpers
# ═══════════════════════════════════════════════════════════════════

def ip_str(v):
    return f"{v>>24 & 0xFF}.{v>>16 & 0xFF}.{v>>8 & 0xFF}.{v & 0xFF}"

def ip_short(v):
    if 1 <= v <= 10:
        return f"{ip_str(v)} (r{v})"
    return ip_str(v)

def mask_to_prefix(m):
    if m == 0: return "0"
    bits = 0
    while m & 0x80000000:
        bits += 1; m <<= 1
    return str(bits)

def flags_imms(v):
    p = []
    if v & 0x04: p.append("I")
    if v & 0x02: p.append("M")
    if v & 0x01: p.append("MS")
    return "+".join(p) if p else "0"

def options_str(v):
    return "E" if v & 0x02 else "0"

PTYPE_NAMES = {1: "Hello", 2: "DD", 3: "LSR", 4: "LSU", 5: "LSAck"}
LSA_TYPE_NAMES = {1: "Router", 2: "Network", 3: "Summary-IP", 4: "Summary-ASBR", 5: "AS-ext"}
LINK_TYPE_NAMES = {1: "P2P", 2: "Transit", 3: "Stub", 4: "Virtual"}

def center(s, w):
    s = str(s)
    if len(s) >= w: return s[:w]
    pad = w - len(s)
    return ' ' * (pad // 2) + s + ' ' * (pad - pad // 2)


# ═══════════════════════════════════════════════════════════════════
# RFC layout — mỗi dòng 68 ký tự
# ═══════════════════════════════════════════════════════════════════

RULER  = "    0                   1                   2                   3"
RULER2 = "    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1"
SEP    = "   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+"
SEP_I  = "   +-                                                             -+"
BLANK  = "   |                                                               |"
FW     = 63   # full-width giữa 2 dấu |
W3     = [15, 15, 31]
W2     = [31, 31]
W_HI   = [31, 15, 15]
W_LA   = [31, 15, 15]
W_LK   = [15, 15, 31]


def field(items, widths):
    """Dựng dòng '   |  item0  |  item1  | ... |'"""
    s = "   "
    for item, w in zip(items, widths):
        s += "|" + center(str(item), w)
    s += "|"
    return s

def full(text):
    """Dựng dòng full-width '   |           text           |'"""
    return f"   |{center(str(text), FW)}|"


# ═══════════════════════════════════════════════════════════════════
# In ấn: row(left, right) — top-level 2 cột
#         row3(left, mid, right) — lồng 3 cột: khung | tên field | giá trị
# ═══════════════════════════════════════════════════════════════════

GAP = "   "   # 3 spaces giữa các cột

def row(left, right=None):
    """2 cột: trái | phải"""
    if right is None:
        print(left)
    else:
        print(f"{left}{GAP}{right}")

def row3(left, mid, right):
    """3 cột: khung | tên field | giá trị (cho cấu trúc lồng)"""
    print(f"{left}{GAP}{mid}{GAP}{right}")

def sep():
    """Border SEP trên cả 2 cột (top-level)"""
    row(SEP, SEP)

def sep3():
    """Border SEP trên cả 3 cột (kết thúc khung lồng)"""
    row3(SEP, SEP, SEP)

def sepi():
    """Border SEP_I trên 1 cột (khung lồng)"""
    row(SEP_I)


# ═══════════════════════════════════════════════════════════════════
# Parsers
# ═══════════════════════════════════════════════════════════════════

def parse_header(data, off=0):
    ver, typ, length = struct.unpack_from('!BBH', data, off)
    routerId = struct.unpack_from('!I', data, off+4)[0]
    areaId = struct.unpack_from('!I', data, off+8)[0]
    checksum, auType = struct.unpack_from('!HH', data, off+12)
    auth1 = struct.unpack_from('!I', data, off+16)[0]
    auth2 = struct.unpack_from('!I', data, off+20)[0]
    return {
        'version': ver, 'type': typ, 'length': length,
        'routerId': routerId, 'areaId': areaId,
        'checksum': checksum, 'auType': auType,
        'auth': (auth1 << 32) | auth2
    }

def parse_lsa_header(data, off):
    age, opts, lsType = struct.unpack_from('!HBB', data, off)
    lsId = struct.unpack_from('!I', data, off+4)[0]
    advRtr = struct.unpack_from('!I', data, off+8)[0]
    seqNum = struct.unpack_from('!i', data, off+12)[0]
    chksum, length = struct.unpack_from('!HH', data, off+16)
    return {
        'age': age, 'options': opts, 'lsType': lsType,
        'linkStateId': lsId, 'advRouter': advRtr,
        'seqNum': seqNum, 'checksum': chksum, 'length': length
    }, off + 20

def parse_lsalink(data, off):
    linkId = struct.unpack_from('!I', data, off)[0]
    linkData = struct.unpack_from('!I', data, off+4)[0]
    linkType, numTOS = struct.unpack_from('!BB', data, off+8)
    metric = struct.unpack_from('!H', data, off+10)[0]
    return {
        'linkId': linkId, 'linkData': linkData,
        'type': linkType, 'numTOS': numTOS, 'metric': metric
    }, off + 12


# ═══════════════════════════════════════════════════════════════════
# OSPF Header (dùng chung)
# ═══════════════════════════════════════════════════════════════════

def print_header(hdr):
    tname = PTYPE_NAMES.get(hdr['type'], f"Type {hdr['type']}")
    row(RULER)
    row(RULER2)
    sep()
    row(field(["Version #", "Type", "Packet length"], W3),
        field([hdr['version'], f"{hdr['type']} ({tname})", hdr['length']], W3))
    sep()
    row(full("Router ID"),
        full(ip_short(hdr['routerId'])))
    sep()
    row(full("Area ID"),
        full(ip_str(hdr['areaId'])))
    sep()
    row(field(["Checksum", "AuType"], W2),
        field([f"0x{hdr['checksum']:04X}", hdr['auType']], W2))
    sep()
    row(full("Authentication"),
        full(f"0x{hdr['auth']:016X}"))
    sep()
    row(full("Authentication"),
        full(""))


# ═══════════════════════════════════════════════════════════════════
# Hello (type 1)
# ═══════════════════════════════════════════════════════════════════

def print_hello(data):
    hdr = parse_header(data)
    print_header(hdr)
    sep()

    off = 24
    mask = struct.unpack_from('!I', data, off)[0]
    helloInt, opts, prio = struct.unpack_from('!HBB', data, off+4)
    deadInt = struct.unpack_from('!I', data, off+8)[0]
    dr   = struct.unpack_from('!I', data, off+12)[0]
    bdr  = struct.unpack_from('!I', data, off+16)[0]
    off += 20

    neighbors = []
    while off + 4 <= len(data):
        neighbors.append(struct.unpack_from('!I', data, off)[0])
        off += 4

    row(full("Network Mask"),
        full(f"{ip_str(mask)} (/{mask_to_prefix(mask)})"))
    sep()
    row(field(["HelloInterval", "Options", "Rtr Pri"], W_HI),
        field([helloInt, f"0x{opts:02X} ({options_str(opts)})", prio], W_HI))
    sep()
    row(full("RouterDeadInterval"),
        full(deadInt))
    sep()
    dr_t = ip_str(dr) if dr != 0 else "0.0.0.0 (none)"
    row(full("Designated Router"), full(dr_t))
    sep()
    bdr_t = ip_str(bdr) if bdr != 0 else "0.0.0.0 (none)"
    row(full("Backup Designated Router"), full(bdr_t))

    for n in neighbors:
        sep()
        row(full("Neighbor"), full(ip_short(n)))
    if not neighbors:
        sep()
        row(full("Neighbor"), full("(none)"))


# ═══════════════════════════════════════════════════════════════════
# LSA Header (lồng trong DD, LSU, LSAck)
# ═══════════════════════════════════════════════════════════════════

def print_lsa_header_nested(lh):
    """In LSA header dạng lồng 3 cột: khung | tên field | giá trị"""
    tname = LSA_TYPE_NAMES.get(lh['lsType'], str(lh['lsType']))

    # Dòng 1: LS age + Options + LS type (có border trên)
    row3(SEP_I, SEP_I, SEP_I)
    row3(BLANK,
         field(["LS age", "Options", "LS type"], W_LA),
         field([lh['age'], f"0x{lh['options']:02X} ({options_str(lh['options'])})", f"{lh['lsType']} ({tname})"], W_LA))

    # Dòng label "An LSA Header"
    row3(f"{SEP_I[:6]}{center('An LSA Header', 62)}", SEP_I, SEP_I)

    # Dòng 3: Link State ID
    row3(BLANK,
         full("Link State ID"),
         full(ip_str(lh['linkStateId'])))
    row3(SEP_I, SEP_I, SEP_I)

    # Dòng 4: Advertising Router
    row3(BLANK,
         full("Advertising Router"),
         full(ip_short(lh['advRouter'])))
    row3(SEP_I, SEP_I, SEP_I)

    # Dòng 5: LS sequence number
    row3(BLANK,
         full("LS sequence number"),
         full(f"0x{lh['seqNum'] & 0xFFFFFFFF:08X}"))

    # Dòng 6: LS checksum + length
    row3(BLANK,
         field(["LS checksum", "length"], W2),
         field([f"0x{lh['checksum']:04X}", lh['length']], W2))
    sep3()


# ═══════════════════════════════════════════════════════════════════
# Router-LSA body (lồng trong LSU)
# ═══════════════════════════════════════════════════════════════════

def print_router_lsa_body(lh, body):
    """In Router-LSA body: flags + links, dạng 3 cột"""
    flags = body[0]
    nlinks = struct.unpack_from('!H', body, 2)[0]

    v = '1' if flags & 0x04 else '0'
    e = '1' if flags & 0x02 else '0'
    b = '1' if flags & 0x01 else '0'

    # Flags + #links
    row3(SEP, SEP, SEP)
    row3(field(["0", "V", "E", "B", "0", "# links"], [9, 1, 1, 1, 15, 31]),
         field(["0", "V", "E", "B", "0", "# links"], [9, 1, 1, 1, 15, 31]),
         field(["0", v, e, b, "0", str(nlinks)], [9, 1, 1, 1, 15, 31]))

    off = 4
    while off + 12 <= len(body):
        link, off = parse_lsalink(body, off)
        ltype_name = LINK_TYPE_NAMES.get(link['type'], str(link['type']))

        row3(SEP, SEP, SEP)
        row3(full("Link ID"),
             full("Link ID"),
             full(ip_str(link['linkId'])))
        row3(SEP, SEP, SEP)
        row3(full("Link Data"),
             full("Link Data"),
             full(ip_str(link['linkData'])))
        row3(SEP, SEP, SEP)
        row3(field(["Type", "# TOS", "metric"], W_LK),
             field(["Type", "# TOS", "metric"], W_LK),
             field([f"{link['type']} ({ltype_name})", link['numTOS'], link['metric']], W_LK))
    sep3()


# ═══════════════════════════════════════════════════════════════════
# Database Description (type 2)
# ═══════════════════════════════════════════════════════════════════

def print_dd(data):
    hdr = parse_header(data)
    print_header(hdr)
    sep()

    off = 24
    mtu, opts, flags = struct.unpack_from('!HBB', data, off)
    ddSeq = struct.unpack_from('!I', data, off+4)[0]
    off += 8

    lsa_headers = []
    while off + 20 <= len(data):
        lh, off = parse_lsa_header(data, off)
        lsa_headers.append(lh)

    flags_str = f"0x{flags:02X} ({flags_imms(flags)})"
    row(field(["Interface MTU", "Options", "0|0|0|0|0|I|M|MS"], W3),
        field([mtu, f"0x{opts:02X} ({options_str(opts)})", flags_str], W3))
    sep()
    row(full("DD sequence number"),
        full(f"0x{ddSeq:08X}"))

    for lh in lsa_headers:
        sep()
        print_lsa_header_nested(lh)

    if not lsa_headers:
        sep()
        row(full("..."), full("(no LSA headers)"))


# ═══════════════════════════════════════════════════════════════════
# Link State Request (type 3)
# ═══════════════════════════════════════════════════════════════════

def print_lsr(data):
    hdr = parse_header(data)
    print_header(hdr)
    sep()

    off = 24
    entries = []
    while off + 12 <= len(data):
        lsType = struct.unpack_from('!I', data, off)[0]
        lsId = struct.unpack_from('!I', data, off+4)[0]
        advRtr = struct.unpack_from('!I', data, off+8)[0]
        entries.append((lsType, lsId, advRtr))
        off += 12

    for lsType, lsId, advRtr in entries:
        tname = LSA_TYPE_NAMES.get(lsType, str(lsType))
        row(full("LS type"), full(f"{lsType} ({tname})"))
        sep()
        row(full("Link State ID"), full(ip_str(lsId)))
        sep()
        row(full("Advertising Router"), full(ip_short(advRtr)))
        sep()

    if not entries:
        row(full("..."), full("(no requests)"))
        sep()


# ═══════════════════════════════════════════════════════════════════
# Link State Update (type 4)
# ═══════════════════════════════════════════════════════════════════

def print_lsu(data):
    hdr = parse_header(data)
    print_header(hdr)
    sep()

    off = 24
    numLSAs = struct.unpack_from('!I', data, off)[0]
    off += 4

    lsas = []
    for _ in range(numLSAs):
        if off + 20 > len(data): break
        lh, _ = parse_lsa_header(data, off)
        lsa_len = lh['length']
        body = data[off+20:off+lsa_len]
        lsas.append((lh, body))
        off += lsa_len

    row(full("# LSAs"), full(str(numLSAs)))
    sep()

    for lh, body in lsas:
        print_lsa_header_nested(lh)
        if lh['lsType'] == 1 and len(body) >= 4:
            print_router_lsa_body(lh, body)


# ═══════════════════════════════════════════════════════════════════
# Link State Acknowledgment (type 5)
# ═══════════════════════════════════════════════════════════════════

def print_lsack(data):
    hdr = parse_header(data)
    print_header(hdr)
    sep()

    off = 24
    lsa_headers = []
    while off + 20 <= len(data):
        lh, off = parse_lsa_header(data, off)
        lsa_headers.append(lh)

    for lh in lsa_headers:
        print_lsa_header_nested(lh)

    if not lsa_headers:
        row(full("..."), full("(no LSA headers)"))
        sep()


# ═══════════════════════════════════════════════════════════════════
# Client Data (8 byte)
# ═══════════════════════════════════════════════════════════════════

def print_client_data(data):
    dest = struct.unpack_from('!I', data, 0)[0]
    src = struct.unpack_from('!I', data, 4)[0]
    print("+==============================================================+")
    print(f"|  CLIENT DATA PACKET  (8 bytes)                               |")
    print(f"|  Destination Router: {ip_short(dest):<40} |")
    print(f"|  Source Router:      {ip_short(src):<40} |")
    print("+==============================================================+")


# ═══════════════════════════════════════════════════════════════════
# Main
# ═══════════════════════════════════════════════════════════════════

def display_packet(filepath):
    with open(filepath, 'rb') as f:
        data = f.read()

    # Parse path: bin/r1/if0/NBR_FULL/000001_0_010000.bin
    #            bin/r1/client/000001_0_010000.bin  (flat cũ: bin/000001_0_010000.bin)
    path_parts = filepath.replace('.bin', '').split(os.sep)
    filename = path_parts[-1]
    parts = filename.split('_', 1)
    seq = parts[0] if parts else "?"
    simTime_str = parts[1].replace('_', '.') if len(parts) > 1 else "?"

    # Trích xuất router, interface, neighbor state từ path
    router = "?"; iface = "?"; nbr_state = "?"
    if len(path_parts) >= 3 and path_parts[-2] == 'client':
        # bin/r1/client/xxx.bin → parts = ['bin', 'r1', 'client', 'xxx']
        router = path_parts[-3]
        iface = 'client'
        nbr_state = '-'
    elif len(path_parts) >= 4:
        # bin/r1/if0/NBR_FULL/xxx.bin → parts = ['bin', 'r1', 'if0', 'NBR_FULL', 'xxx']
        router = path_parts[-4]
        iface = path_parts[-3]
        nbr_state = path_parts[-2]

    print(f"\n{'='*70}")
    print(f"  Router: {router}  Iface: {iface}  NbrState: {nbr_state}")
    print(f"  File: {filename}   Seq: {seq}   SimTime: {simTime_str}s   Size: {len(data)} bytes")
    print(f"{'='*70}")

    if len(data) == 8:
        print_client_data(data)
        return

    if len(data) >= 24 and data[0] == 0x02:
        pkt_type = data[1]
        if pkt_type == 1:   print_hello(data)
        elif pkt_type == 2: print_dd(data)
        elif pkt_type == 3: print_lsr(data)
        elif pkt_type == 4: print_lsu(data)
        elif pkt_type == 5: print_lsack(data)
        else:               print(f"  Unknown OSPF type: {pkt_type}")
        return

    print(f"  Unknown format ({len(data)} bytes)")
    for i in range(0, min(len(data), 128), 16):
        chunk = data[i:i+16]
        hex_str = ' '.join(f'{b:02x}' for b in chunk)
        ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)
        print(f"  {i:04x}: {hex_str:<48} {ascii_str}")


def run_all(bin_dir, out_dir, workers=8):
    """Quét tất cả file .bin trong bin/, parse song song ra out_dir/,
    mirror cấu trúc thư mục."""
    os.makedirs(out_dir, exist_ok=True)

    # Thu thập tất cả file .bin
    print(f"Quét {bin_dir} ...", file=sys.stderr, flush=True)
    all_files = sorted(_glob.glob(os.path.join(bin_dir, '**', '*.bin'), recursive=True))
    total = len(all_files)
    print(f"Tìm thấy {total} file .bin. Bắt đầu parse với {workers} worker...",
          file=sys.stderr, flush=True)

    done = [0]
    lock = __import__('threading').Lock()

    def worker(fpath):
        # Tính đường dẫn output mirror: bin/... → resultbin/... .bin → .txt
        rel = os.path.relpath(fpath, bin_dir)
        out_path = os.path.join(out_dir, os.path.splitext(rel)[0] + '.txt')

        if os.path.exists(out_path):
            with lock:
                done[0] += 1
            return fpath, True

        try:
            os.makedirs(os.path.dirname(out_path), exist_ok=True)
            result = subprocess.run(
                ["python3", SCRIPT_PATH, fpath, "-o", out_path],
                cwd=PROJECT_ROOT,
                capture_output=True, text=True, timeout=180,
            )
            with lock:
                done[0] += 1
                if done[0] % 200 == 0:
                    print(f"  Bin: {done[0]}/{total} ({100*done[0]//total}%)",
                          file=sys.stderr, flush=True)
            return fpath, result.returncode == 0
        except Exception as e:
            with lock:
                done[0] += 1
            return fpath, False, str(e)

    with ThreadPoolExecutor(max_workers=workers) as pool:
        futures = [pool.submit(worker, fp) for fp in all_files]
        errors = 0
        for f in as_completed(futures):
            result = f.result()
            if len(result) == 3:
                fpath, ok, err = result
                if not ok:
                    errors += 1
                    print(f"  [LỖI] {fpath}: {err}", file=sys.stderr)
            elif len(result) == 2:
                fpath, ok = result
                if not ok:
                    errors += 1

    print(f"  Bin done: {total} file -> {out_dir}/  (lỗi: {errors})", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(
        description="Parse OSPF .bin files và hiển thị theo RFC 2328 format"
    )
    parser.add_argument('args', nargs='*',
                        help='Số thứ tự (vd: 1000) hoặc đường dẫn file .bin')
    parser.add_argument('-o', '--output', help='Ghi ra file thay vì in terminal')
    parser.add_argument('--all', action='store_true',
                        help='Parse toan bo seq ra resultbin/')
    parser.add_argument('--workers', type=int, default=8,
                        help='So worker cho --all (default: 8)')
    args = parser.parse_args()

    if args.all:
        out_dir = os.path.join(PROJECT_ROOT, 'resultbin')
        bin_dir = os.path.join(PROJECT_ROOT, 'bin')
        run_all(bin_dir, out_dir, args.workers)
        return

    if not args.args:
        parser.print_help()
        sys.exit(1)

    # Nếu có -o, redirect stdout sang file
    out_fh = None
    if args.output:
        out_fh = open(args.output, 'w', encoding='utf-8')
        sys.stdout = out_fh

    all_files = []
    for pat in args.args:
        if pat.isdigit():
            seq_padded = f"{int(pat):06d}"
            pattern = f"bin/**/{seq_padded}_*.bin"
            matched = _glob.glob(pattern, recursive=True)
        else:
            matched = _glob.glob(pat)

        if matched:
            all_files.extend(sorted(matched))
        else:
            if pat.isdigit():
                print(f"  [skip] không tìm thấy file có seq={seq_padded}", file=sys.stderr)
            else:
                print(f"  [skip] không tìm thấy: {pat}", file=sys.stderr)

    if not all_files:
        print("Không có file nào để parse.", file=sys.stderr)
        if out_fh:
            sys.stdout = sys.__stdout__
            out_fh.close()
        sys.exit(1)

    for fpath in all_files:
        try:
            display_packet(fpath)
        except Exception as e:
            print(f"  [ERROR] {fpath}: {e}", file=sys.stderr)

    print(f"\n  Total: {len(all_files)} file(s)")

    if out_fh:
        sys.stdout = sys.__stdout__
        out_fh.close()
        print(f"  Đã ghi ra: {args.output}", file=sys.stderr)


if __name__ == '__main__':
    main()
