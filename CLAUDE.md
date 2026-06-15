# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## RULE
# Tuyệt đối không được bịa thông tin
# Trước khi suy luận phải kiểm tra file tài liệu docs/ospfv2.txt

## các tài liệu quan trọng
docs/OMNeT++ - Simulation Manual.html đây là tài liệu hướng dẫn lập trình omnet
docs/ospfv2.txt đây là tài liệu kĩ thuật dự án triển khai
docs/plan thinking đây là giời hạn phạm vi mô phỏng

## Dự án: Mô phỏng OSPFv2 trên OMNeT++ 6.4

Đây là một mô phỏng giao thức OSPFv2 (RFC 2328) single-area, chỉ dùng P2P links, chỉ Router-LSA.
Topology gồm 10 router + 10 client kết nối như sơ đồ trong `ospf.ned`.

**Trạng thái hiện tại:** Khung dữ liệu và message classes đã có. Module `routerOspf` mới chỉ khởi tạo cấu trúc dữ liệu qua `initState()`; `handleMessage()` và `finish()` đang trống `// TODO`.


## Cấu trúc code

```
ospf.msg          → Định nghĩa 8 message class (headerOspf, helloOspf, LSAHeaderOspf,
                     databaseDescriptionOspf, linkStateBodyOspf, linkStateRequestOspf,
                     linkStateUpdateOspf, linkStateAcknowledgementOspf)
                     → opp_msgtool sinh ra ospf_m.h / ospf_m.cc (~4k dòng, KHÔNG SỬA)

ospf.h            → Các enum trạng thái (OspfNeighborState, OspfInterfaceState…),
                     các struct dữ liệu OSPF (InterfaceData, NeighborData, RouterLsa,
                     AreaData, RoutingTableEntry, OspfRouterState),
                     khai báo class routerOspf : public cSimpleModule

ospf.cc           → Implement routerOspf (Define_Module, initialize, initState,
                     handleMessage, finish)

ospf.ned          → Topology: network Ospf { r1..r10, client1..client10 }
                     routerOspf có gate[ ] (inout, dùng cho P2P link)
```

**Kiến trúc dữ liệu (từ dưới lên):**
- Mỗi `InterfaceData` có đúng 1 `NeighborData` (P2P, cấp phát tĩnh từ `initState`)
- Tất cả interface thuộc về 1 `AreaData` duy nhất (single-area)
- `AreaData` chứa LSDB (`routerLSAs[]`) và kết quả SPF tree (`spfTree[]`)
- `OspfRouterState` là root struct chứa routerID, interfaces, area, routingTable

---

## Hướng dẫn làm việc với message classes

Xem `HUONGDAN.md` — tóm tắt:
- `getX()` / `setX(v)` cho field số; `getFieldForUpdate()` cho field object (tránh copy)
- Mảng động: `setXArraySize(n)`, `appendX(v)`, `insertX(k,v)`, `eraseX(k)`, `getX(k)`
- Phân biệt message bằng `dynamic_cast<helloOspf*>(msg)`, `dynamic_cast<linkStateUpdateOspf*>(msg)`, v.v.

Hướng dẫn `cSimpleModule` ở `HUONGDAN_cSimpleModule.md`:
- Chỉ có 3 method để override: `initialize()`, `handleMessage(cMessage*)`, `finish()`
- Timer dùng `scheduleAt(simTime()+t, msg)` — kiểm tra bằng `msg->isSelfMessage()`
- Gửi gói tin: `send(msg, "gate", index)` — dùng `msg->dup()` nếu gửi ra nhiều cổng
- Xóa gói tin đã xử lý xong: `delete msg`

---

## File sinh tự động — không sửa

- `ospf_m.h` / `ospf_m.cc` — sinh từ `ospf.msg` bởi `opp_msgtool`
- `Makefile` — sinh từ `opp_makemake -f`
- `out/` — thư mục build output

---

## Scope giới hạn (RFC 2328 subset)

Dự án này chỉ implement một tập con của OSPFv2:
- **P2P only** — không broadcast, không NBMA, không DR/BDR
- **Single-area** — chỉ area 0.0.0.0
- **Router-LSA only** — không Network-LSA, Summary-LSA, AS-external-LSA
- **Mỗi interface có đúng 1 neighbor** — cấp phát tĩnh từ `initState()`

Các file tham khảo: `ospfv2.txt` (RFC 2328 full text, tiếng Việt).
