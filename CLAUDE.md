# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## RULE
# Tuyệt đối không được bịa thông tin
# Trước khi suy luận phải kiểm tra file tài liệu docs/ospfv2.txt

## Tài liệu quan trọng (đọc trước khi làm)

| File | Vai trò |
|------|---------|
| `docs/ospf_flow_v2.txt` | **Sơ đồ luồng tổng thể** — file đầu tiên phải đọc, mô tả toàn bộ hoạt động OSPF |
| `docs/ospfv2.txt` | RFC 2328 full text (tiếng Việt) — tham chiếu kỹ thuật gốc |
| `docs/plan thinking/QUESTIONS.md` | Câu hỏi thiết kế và giới hạn phạm vi |
| `docs/plan thinking/DECISION-LOG.md` | Nhật ký từng quyết định thiết kế, lý do, hệ quả |
| `docs/nhật kí kĩ thuật.txt` | Giới hạn của OMNeT++ và hướng xử lý |
| `docs/disconect_link.txt` | Cơ chế Link Flap |
| `docs/HUONGDAN_cSimpleModule.md` | Hướng dẫn lập trình cSimpleModule |
| `docs/HUONGDAN_using_msg.md` | Hướng dẫn làm việc với message class |

## Build và chạy

```bash
cd /home/k/omnetpp-6.4.0/workspace/ospf
source /home/k/omnetpp-6.4.0/setenv   # nạp môi trường OMNeT++
make -j$(nproc)                         # build
./out/clang-release/ospf -c General --sim-time-limit=180s   # chạy simulation
```

File cấu hình: `omnetpp.ini` — network=`Ospf`, sim-time-limit, record-eventlog.
File cấu hình Qtenv: `.qtenvrc` — tốc độ playback cho từng chế độ run/fast.

## Dự án: Mô phỏng OSPFv2 trên OMNeT++ 6.4

Mô phỏng giao thức OSPFv2 (RFC 2328) single-area, chỉ dùng P2P links, chỉ Router-LSA.
Topology gồm 10 router + 10 client kết nối như sơ đồ trong `ospf.ned`.

**Trạng thái hiện tại:** Đã implement đầy đủ luồng OSPF:
- Phase 0: Hello protocol (neighbor discovery, 2-Way)
- Phase 1a: Database Exchange (DD negotiation ExStart→Exchange→Loading→Full)
- Phase 1b1: Link State Request (LSR) — request missing LSAs
- Phase 1b2: Link State Update (LSU) — flooding + acknowledgment
- Phase 1c: SPF calculation (Dijkstra) + routing table build
- Phase 2a: Data forwarding (routing table lookup)
- Phase 2b: Client data test (client send events)
- Link Flap scheduler: tắt/bật link theo kịch bản
- Debug tools: JSON dump state, binary packet dump, web viewer

## Kiến trúc code

### File nguồn chính

```
ospf.msg              → Định nghĩa 1 message class (Mess { uint8_t payload[] })
                         → opp_msgtool sinh ra ospf_m.h / ospf_m.cc (KHÔNG SỬA)

ospf_struct.h / .cc   → TẤT CẢ cấu trúc dữ liệu OSPF và message handlers:
                         - headerOspf, LSAHeader, LSA, LSALink, LSARequest
                         - Enums: OspfNeighborState, OspfInterfaceState, OspfRouterLinkType, OspfPathType
                         - InterfaceData, NeighborData, AreaData, SpfVertex
                         - RoutingTableEntry, OspfRouterState
                         - Protocol handlers: helloData, databaseDescriptionData,
                           linkStateRequestData, linkStateUpdateData,
                           linkStateAcknowledgementData
                         - OspfMess: class kế thừa cMessage, có send()/parse()

ospf.h                → Class routerOspf : public cSimpleModule
                         - Members: state*, routerId, numRouters, timers
                         - Link Flap + Client Send scheduler
                         - Debug: dumpStateToJson(), dumpMessageBinary()
                         - SPF: calculateSpf(), calcNextHop(), forwardData()

ospf.cc               → Toàn bộ implement của routerOspf (~1600 dòng):
                         initialize(), handleMessage(), finish()
                         + SPF, data forwarding, debug dump, link flap

ospf.ned              → Topology: network Ospf { r1..r10, client1..client10 }
                         routerOspf có gate[ ] (inout P2P) + clientGate

client.h / client.cc  → Module Client đơn giản: nhận data packet từ router, parse src

omnetpp.ini           → Cấu hình simulation (network, sim-time-limit, eventlog)
```

### Luồng xử lý chính trong `handleMessage()`

1. **Self-message (timer):** helloTimer → gửi Hello định kỳ; spfTimer → tính SPF; flapTimer → xử lý link flap; clientSendTimer → gửi data test; inactivityTimer → neighbor dead; rxmtTimer → retransmit DD/LSR/LSU
2. **Link flap block check:** Nếu interface bị block (link down) → drop gói, không xử lý
3. **clientGate:** Data packet từ Client → forwardData()
4. **OSPF packets (type 1-5):** Hello → DD → LSR → LSU → LSAck, mỗi loại gọi hàm processXxx tương ứng trong `ospf_struct.cc`
5. **Data packet (type 0):** Forward data theo routing table

### Kiến trúc dữ liệu (từ dưới lên)

- Mỗi `InterfaceData` có đúng 1 `NeighborData` (P2P, cấp phát tĩnh từ `OspfRouterState` constructor)
- Tất cả interface thuộc về 1 `AreaData` duy nhất (single-area)
- `AreaData` chứa LSDB (`routerLSAs[]`), SPF tree (`spfVertices[]`), transit/external capability
- `OspfRouterState` là root struct: routerID, interfaces, area, RoutingTable, externalRoutes

### Pattern gửi/nhận message

- **Gửi:** `OspfMess::send(type, body, routerId, areaId, ifIndex, mod)` — đóng OSPF header + body vào `Mess::payload[]` → `send()` ra gate
- **Nhận:** `OspfMess::parse(msg, iface, hdr, data)` — tách `Mess::payload[]` → điền `headerOspf` + `vector<uint8_t>` data, trả về type (0 nếu fail)
- **Phân biệt loại gói:** `dynamic_cast<Mess*>(msg)` → parse type → `if (pktType == 1)` Hello, `== 2` DD, v.v.
- **Timer:** `msg->isSelfMessage()` → so sánh con trỏ với các timer member
- **Xóa gói:** `delete msg` sau khi xử lý xong; dùng `msg->dup()` nếu cần gửi ra nhiều cổng

### Kịch bản Link Flap (`link_flaps.txt`)

Định dạng mỗi dòng:
```
r1 r2 down 100      # t=100: tắt link r1-r2
r8 r10 up 100       # t=100: bật link r8-r10
client send 300     # t=300: tất cả router gửi data test đến nhau
```
- `#` = comment, dòng trống bỏ qua
- `parseLinkFlaps()` trong `initialize()` đọc file này, mỗi router chỉ lọc sự kiện liên quan đến mình
- Để không flap: để file trống

## Phân tích kết quả sau khi chạy

### Quy trình phân tích nhanh

```bash
# Bước 1: Biên dịch toàn bộ output (~vài phút, chạy 1 lần)
python3 tools/viz_topology_ascii.py --all --workers 8
python3 tools/parse_bin.py --all --workers 8

# Bước 2: Khởi động web viewer
python3 tools/viewer.py

# Bước 3: Mở browser http://localhost:8080
#   Tab "Topology Viz" — duyệt topology theo router + seq
#   Tab "Parse Bin"    — duyệt gói tin theo seq, hiện đường dẫn + nội dung
```

### Các công cụ trong `tools/`

| Tool | Chức năng |
|------|-----------|
| `viewer.py` | Web server (port 8080) — 2 tab: topology viz + parse bin |
| `viz_topology_ascii.py` | Đọc JSON dump → vẽ topology ASCII + bảng định tuyến. `--all` biên dịch toàn bộ `log/` → `resultlog/` |
| `parse_bin.py` | Đọc file binary `.bin` → parse nội dung OSPF. `--all` biên dịch `bin/` → `resultbin/` |
| `verify_delivery.py` | Kiểm tra kết quả data delivery |
| `ospf_chart.py` / `ospf_chart.html` | Biểu đồ timeline trạng thái neighbor |

### Cấu trúc thư mục dữ liệu

```
log/          ← JSON dump trạng thái router (tự động sinh khi chạy sim)
  {router}/   ← r1..r10, mỗi file {counter}.json

bin/          ← Gói tin OSPF nhị phân (tự động sinh khi chạy sim)
  {router}/{interface}/{neighbor_state}/{seq:06d}_{simTime}.bin
  {router}/client/...   ← gói từ clientGate

resultlog/    ← Output của viz_topology_ascii.py --all (mirror log/)
resultbin/    ← Output của parse_bin.py --all (mirror bin/)
```

**Seq/counter là mã định danh duy nhất** dùng để tra cứu file không cần biết đường dẫn cha.

## File sinh tự động — không sửa

- `ospf_m.h` / `ospf_m.cc` — sinh từ `ospf.msg` bởi `opp_msgtool`
- `Makefile` — sinh từ `opp_makemake -f`
- `out/` — thư mục build output

## Scope giới hạn (RFC 2328 subset)

- **P2P only** — không broadcast, không NBMA, không DR/BDR
- **Single-area** — chỉ area 0.0.0.0
- **Router-LSA only** — không Network-LSA, Summary-LSA, AS-external-LSA
- **Mỗi interface có đúng 1 neighbor** — cấp phát tĩnh từ `OspfRouterState` constructor
- **HelloInterval = 10s**, **RouterDeadInterval = 40s** (đúng chuẩn RFC)
- **No checksum** — môi trường simulation không có lỗi bit
- **No authentication** — không cần trong simulation
