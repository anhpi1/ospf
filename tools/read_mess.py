#!/usr/bin/env python3
"""Đọc file .bin trong mess/r*/ và hiển thị ASCII-art RFC format."""

import sys
import struct
import os

# ─── OSPF message type names ───
TYPE_NAMES = {0: "Data", 1: "Hello", 2: "Database Description",
              3: "Link State Request", 4: "Link State Update",
              5: "Link State Acknowledgement"}

LSA_TYPE_NAMES = {1: "Router-LSA", 2: "Network-LSA", 3: "Summary-LSA(IP)",
                  4: "Summary-LSA(ASBR)", 5: "AS-External-LSA"}

LINK_TYPE_NAMES = {1: "Point-to-point", 2: "Transit", 3: "Stub", 4: "Virtual"}


def read_ospf_header(data, off):
    """Đọc 24-byte OSPF header."""
    ver, typ, length = data[off], data[off+1], (data[off+2]<<8)|data[off+3]
    rid  = (data[off+4]<<24)|(data[off+5]<<16)|(data[off+6]<<8)|data[off+7]
    aid  = (data[off+8]<<24)|(data[off+9]<<16)|(data[off+10]<<8)|data[off+11]
    csum = (data[off+12]<<8)|data[off+13]
    atyp = (data[off+14]<<8)|data[off+15]
    ad1  = (data[off+16]<<24)|(data[off+17]<<16)|(data[off+18]<<8)|data[off+19]
    ad2  = (data[off+20]<<24)|(data[off+21]<<16)|(data[off+22]<<8)|data[off+23]
    return ver, typ, length, rid, aid, csum, atyp, ad1, ad2


def ip_str(val):
    return f"{val>>24}.{(val>>16)&0xFF}.{(val>>8)&0xFF}.{val&0xFF}"


def box_line(fmt, *args):
    """In một dòng trong box. fmt dạng: '| Label=value' hoặc '| Label1=val1  Label2=val2'"""
    s = fmt.format(*args)
    print(f"| {s:<76} |")


def box_sep():
    print("+" + "-"*78 + "+")


def print_ospf_common(ver, typ, length, rid, aid, csum, atyp, ad1, ad2):
    box_sep()
    box_line("Version={}  Type={} ({})  Length={}",
             ver, typ, TYPE_NAMES.get(typ, "?"), length)
    box_line("Router ID = {}", ip_str(rid))
    box_line("Area ID = {}", ip_str(aid))
    box_line("Checksum=0x{:04X}  AuthType={}", csum, atyp)
    box_line("AuthData = 0x{:08X} 0x{:08X}", ad1, ad2)


def print_hello_body(data, off, length):
    """RFC A.3.2 Hello body."""
    if length < 20:
        print("  (Hello body too short: {} bytes)".format(length))
        return
    mask   = (data[off]<<24)|(data[off+1]<<16)|(data[off+2]<<8)|data[off+3]; off+=4
    hint   = (data[off]<<8)|data[off+1]; off+=2
    opt    = data[off]; off+=1
    pri    = data[off]; off+=1
    dint   = (data[off]<<24)|(data[off+1]<<16)|(data[off+2]<<8)|data[off+3]; off+=4
    dr     = (data[off]<<24)|(data[off+1]<<16)|(data[off+2]<<8)|data[off+3]; off+=4
    bdr    = (data[off]<<24)|(data[off+1]<<16)|(data[off+2]<<8)|data[off+3]; off+=4
    remain = length - 20
    neighbors = []
    while remain >= 4:
        nid = (data[off]<<24)|(data[off+1]<<16)|(data[off+2]<<8)|data[off+3]
        neighbors.append(ip_str(nid))
        off += 4; remain -= 4

    box_line("Network Mask = {}", ip_str(mask))
    box_line("HelloInterval={}  Options=0x{:02X}  Priority={}", hint, opt, pri)
    box_line("RouterDeadInterval = {}", dint)
    box_line("Designated Router = {}", ip_str(dr))
    box_line("Backup DR = {}", ip_str(bdr))
    box_line("Neighbors ({}): {}", len(neighbors), ", ".join(neighbors[:8]))
    if len(neighbors) > 8:
        box_line("  ... +{} more", len(neighbors)-8)


def print_lsa_header(data, off, label="LSA Header"):
    """In 20-byte LSA header."""
    age  = (data[off]<<8)|data[off+1]; off+=2
    opt  = data[off]; off+=1
    typ  = data[off]; off+=1
    lsid = (data[off]<<24)|(data[off+1]<<16)|(data[off+2]<<8)|data[off+3]; off+=4
    adv  = (data[off]<<24)|(data[off+1]<<16)|(data[off+2]<<8)|data[off+3]; off+=4
    seq  = struct.unpack('>i', bytes(data[off:off+4]))[0]; off+=4
    csum = (data[off]<<8)|data[off+1]; off+=2
    llen = (data[off]<<8)|data[off+1]; off+=2
    box_line("{}: Age={}  Opt=0x{:02X}  Type={} ({})",
             label, age, opt, typ, LSA_TYPE_NAMES.get(typ, "?"))
    box_line("  LinkStateID={}  AdvRouter={}", ip_str(lsid), ip_str(adv))
    box_line("  Seq=0x{:08X}  Checksum=0x{:04X}  Length={}", seq & 0xFFFFFFFF, csum, llen)
    return off, typ, llen


def print_router_lsa(data, off, lsa_len):
    """In Router-LSA body (sau 20-byte header)."""
    body_start = off - 20
    body_end = body_start + lsa_len
    remain = body_end - off
    if remain < 4:
        box_line("  (Router-LSA body too short)")
        return
    flags = data[off]; off+=1
    zero  = data[off]; off+=1
    nlink = (data[off]<<8)|data[off+1]; off+=2

    bits = []
    if flags & 0x01: bits.append("B")
    if flags & 0x02: bits.append("E")
    if flags & 0x04: bits.append("V")
    box_line("  Flags=0x{:02X} [{}]  Zero={}  #Links={}", flags, ",".join(bits) or "none", zero, nlink)

    for i in range(nlink):
        if off + 12 > body_end:
            box_line("  Link#{}: (truncated)", i+1)
            break
        lid  = (data[off]<<24)|(data[off+1]<<16)|(data[off+2]<<8)|data[off+3]; off+=4
        ldat = (data[off]<<24)|(data[off+1]<<16)|(data[off+2]<<8)|data[off+3]; off+=4
        ltyp = data[off]; off+=1
        ntos = data[off]; off+=1
        met  = (data[off]<<8)|data[off+1]; off+=2
        box_line("  Link#{}: ID={}  Data={}  Type={} ({})  Metric={}",
                 i+1, ip_str(lid), ip_str(ldat), ltyp,
                 LINK_TYPE_NAMES.get(ltyp, "?"), met)
        # skip TOS entries
        for _ in range(ntos):
            if off + 12 <= body_end:
                off += 12


def print_dd_body(data, off, length):
    """RFC A.3.3 DD body."""
    if length < 8:
        box_line("(DD body too short: {} bytes)", length)
        return
    mtu  = (data[off]<<8)|data[off+1]; off+=2
    opt  = data[off]; off+=1
    flg  = data[off]; off+=1
    seq  = struct.unpack('>i', bytes(data[off:off+4]))[0]; off+=4

    bits = []
    if flg & 0x04: bits.append("I")
    if flg & 0x02: bits.append("M")
    if flg & 0x01: bits.append("MS")
    box_line("MTU={}  Options=0x{:02X}  Flags=0x{:02X} [{}]  Seq=0x{:08X}",
             mtu, opt, flg, ",".join(bits) or "none", seq & 0xFFFFFFFF)

    # LSA headers
    remain = length - 8
    n = 1
    while remain >= 20:
        box_line("── LSA Header #{} ──", n)
        off, _, _ = print_lsa_header(data, off)
        remain -= 20; n += 1


def print_lsr_body(data, off, length):
    """RFC A.3.4 LSR body — mỗi request 12 byte."""
    n = (length // 12)
    for i in range(n):
        typ  = (data[off]<<24)|(data[off+1]<<16)|(data[off+2]<<8)|data[off+3]; off+=4
        lsid = (data[off]<<24)|(data[off+1]<<16)|(data[off+2]<<8)|data[off+3]; off+=4
        adv  = (data[off]<<24)|(data[off+1]<<16)|(data[off+2]<<8)|data[off+3]; off+=4
        box_line("Req#{}: LSType={}  LSID={}  AdvRouter={}",
                 i+1, typ, ip_str(lsid), ip_str(adv))
    if length % 12:
        box_line("  (+ {} byte(s) padding)", length % 12)


def print_lsu_body(data, off, length):
    """RFC A.3.5 LSU body — 4 byte count + các LSA."""
    if length < 4:
        return
    nlsa = (data[off]<<24)|(data[off+1]<<16)|(data[off+2]<<8)|data[off+3]; off+=4
    box_line("#LSAs = {}", nlsa)
    remain = length - 4
    for i in range(nlsa):
        if remain < 20:
            box_line("LSA#{}: (truncated)", i+1)
            break
        box_line("── LSA #{} ──", i+1)
        off, lsa_type, lsa_len = print_lsa_header(data, off)
        remain -= 20
        body_len = lsa_len - 20
        if body_len > 0 and remain >= body_len:
            if lsa_type == 1:
                print_router_lsa(data, off, lsa_len)
            else:
                box_line("  (LSA type={} body: {} bytes, skipped)", lsa_type, body_len)
            off += body_len; remain -= body_len


def print_lsack_body(data, off, length):
    """RFC A.3.6 LSAck body — danh sách LSA headers."""
    remain = length
    n = 1
    while remain >= 20:
        box_line("── LSA Header #{} ──", n)
        off, _, _ = print_lsa_header(data, off)
        remain -= 20; n += 1


def print_data_packet(data):
    """In data packet payload."""
    if len(data) >= 8:
        dest = (data[0]<<24)|(data[1]<<16)|(data[2]<<8)|data[3]
        src  = (data[4]<<24)|(data[5]<<16)|(data[6]<<8)|data[7]
        box_sep()
        box_line("DATA PACKET (Type=0)")
        box_line("Destination = R{} (0x{:08X})", dest, dest)
        box_line("Source = R{} (0x{:08X})", src, src)
        box_line("Payload = {} bytes", len(data))
        if len(data) > 8:
            hexstr = " ".join(f"{b:02x}" for b in data[8:48])
            box_line("Extra data: {}", hexstr)
    else:
        box_sep()
        box_line("DATA PACKET (Type=0) — {} bytes raw", len(data))
        box_line("Hex: {}", " ".join(f"{b:02x}" for b in data))


def parse_and_print(filepath):
    with open(filepath, "rb") as f:
        data = f.read()

    if len(data) < 1:
        print("File rỗng")
        return

    fname = os.path.basename(filepath)
    rdir  = os.path.basename(os.path.dirname(filepath))
    print(f"\n{'='*80}")
    print(f"  File: {rdir}/{fname}  ({len(data)} bytes)")
    print(f"{'='*80}")

    # Không phải OSPF packet (version != 2) hoặc data quá ngắn
    if len(data) <= 8 or data[0] != 2:
        if 8 <= len(data) <= 8:
            print_data_packet(data)
        else:
            # Raw dump cho dữ liệu không xác định
            box_sep()
            box_line("RAW DATA ({} bytes, version={})", len(data), data[0] if data else "N/A")
            for i in range(0, min(len(data), 128), 16):
                hexstr = " ".join(f"{data[j]:02x}" for j in range(i, min(i+16, len(data))))
                box_line("  {:04x}: {}", i, hexstr)
        box_sep()
        return

    ver, typ, length, rid, aid, csum, atyp, ad1, ad2 = read_ospf_header(data, 0)
    body = data[24:]
    body_len = len(body)

    print_ospf_common(ver, typ, length, rid, aid, csum, atyp, ad1, ad2)

    if typ == 1:
        print_hello_body(body, 0, body_len)
    elif typ == 2:
        print_dd_body(body, 0, body_len)
    elif typ == 3:
        print_lsr_body(body, 0, body_len)
    elif typ == 4:
        print_lsu_body(body, 0, body_len)
    elif typ == 5:
        print_lsack_body(body, 0, body_len)
    else:
        box_line("(Unknown type={}, {} bytes body)", typ, body_len)
        if body_len > 0:
            box_line("Hex: {}", " ".join(f"{b:02x}" for b in body[:64]))

    box_sep()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 read_mess.py <file.bin> [file2.bin ...]")
        print("  Hoặc: python3 read_mess.py mess/r1/a1.bin")
        sys.exit(1)

    for path in sys.argv[1:]:
        if os.path.isfile(path):
            parse_and_print(path)
        else:
            print(f"Không tìm thấy: {path}")
