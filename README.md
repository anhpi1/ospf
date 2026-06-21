# OSPFv2 Simulation trên OMNeT++ 6.4

Mô phỏng giao thức OSPFv2 (RFC 2328) single-area, P2P links, 10 router + 10 client.
 


## Tài liệu quan trọng nhất

| File | Mô tả |
|------|-------|
| [`docs/ospf_flow_v2.txt`](docs/ospf_flow_v2.txt) | **Sơ đồ luồng tổng thể** — file duy nhất mô tả toàn bộ hoạt động ospf. **Phải đọc đầu tiên.** |
| [`docs/ospfv2.txt`](docs/ospfv2.txt) | RFC 2328 full text (tiếng Việt) — tham chiếu kỹ thuật gốc. |
| [`docs/plan thinking/QUESTIONS.md`](docs/plan%20thinking/QUESTIONS.md) | **Người đọc bắt buộc phải đọc.** Câu hỏi thiết kế và giới hạn phạm vi: tại sao chỉ P2P? Tại sao không DR/BDR? Tại sao không checksum? Timer rút gọn thế nào? |
| [`docs/plan thinking/DECISION-LOG.md`](docs/plan%20thinking/DECISION-LOG.md) | **Nhật ký kỹ thuật.** Từng quyết định thiết kế, lý do, và hệ quả. |
| [`docs/nhật kí kĩ thuật.txt`](docs/nh%E1%BA%ADt%20k%C3%AD%20k%C4%A9%20thu%E1%BA%ADt.txt) | **Nhật ký coding thực tế.** Giới hạn của omnetpp và hướng xử lý. |
| [`docs/disconect_link.txt`](docs/disconect_link.txt) | Cơ chế Link Flap — cách cấu hình và hoạt động. |



## Cấu trúc quan trọng

| Đường dẫn | Mô tả |
|-----------|-------|
| `ospf.msg` | Định nghĩa message class — sinh tự động ra `ospf_m.h`/`ospf_m.cc` |
| `ospf.h` / `ospf.cc` | Module routerOspf — code chính của toàn bộ dự án |
| `ospf_struct.h` / `ospf_struct.cc` | Cấu trúc dữ liệu OSPF (InterfaceData, NeighborData, LSA, RoutingTableEntry, ...) |
| `log/` | JSON dump trạng thái router sau mỗi sự kiện — **dữ liệu đầu vào cho tools** |
| `tools/` | Công cụ phân tích kết quả mô phỏng |
| `docs/specs/` | Đặc tả kỹ thuật từng subphase (0 → 1a → 1b1 → 1b2 → 1c → 2a → 2b) |



## Hướng dẫn đọc code

Bắt đầu từ `ospf.h` — lớp `routerOspf` chỉ có 3 hàm chính:

- **`initialize()`** — khởi tạo state, gửi Hello đầu tiên, khởi tạo các bộ timer, nạp kịch bản link flap
- **`handleMessage(cMessage *msg)`** — luồng xử lý chính: Hello, DD, LSR, LSU, LSAck, timer (Hello, SPF, inactivity, rxmt, flap)
- **`finish()`** — dọn dẹp timer



## Phân tích kết quả

### Quy trình phân tích nhanh (khuyến nghị)

```bash
# Bước 1: Biên dịch toàn bộ output (chạy 1 lần, ~vài phút)
python3 tools/viz_topology_ascii.py --all --workers 8
python3 tools/parse_bin.py --all --workers 8

# Bước 2: Khởi động web viewer
python3 tools/viewer.py

# Bước 3: Mở browser http://localhost:8080
#   Tab "Topology Viz" — duyệt topology theo router + seq
#   Tab "Parse Bin"    — duyệt gói tin theo seq, hiện đường dẫn + nội dung
#   Giữ nút Prev/Next để quét nhanh, tự động nhảy seq gần nhất nếu không có
```

### `tools/viewer.py` — Web Viewer

Server HTTP đọc file tĩnh từ `resultlog/` và `resultbin/` (đã biên dịch trước). Giao diện 2 tab:

- **Tab Topology Viz**: chọn router (r1-r10) → nhập seq → Prev/Next ±1 → hiển thị topology ASCII của riêng router đó
- **Tab Parse Bin**: nhập seq → Prev/Next ±1 → hiển thị danh sách đường dẫn + nội dung gói tin OSPF đã parse

```bash
python3 tools/viewer.py                  # mặc định port 8080
python3 tools/viewer.py --port 9090      # đổi port
```

### `tools/viz_topology_ascii.py`

Đọc file JSON dump OSPF, vẽ topology ASCII + bảng định tuyến + trạng thái interface.

```bash
# Single-seq mode: vẽ topology cho 1 seq (merge 10 router tại cùng thời điểm)
python3 tools/viz_topology_ascii.py 500
python3 tools/viz_topology_ascii.py log/r1/500.json
python3 tools/viz_topology_ascii.py --latest
python3 tools/viz_topology_ascii.py 500 -o output.txt

# List mode: liệt kê tất cả dump
python3 tools/viz_topology_ascii.py --list

# --all mode: biên dịch toàn bộ log/ → resultlog/ (mỗi file JSON → 1 file txt riêng cho router đó)
python3 tools/viz_topology_ascii.py --all --workers 8
```

### `tools/parse_bin.py`

Đọc file binary `.bin` chứa gói tin OSPF và hiển thị nội dung theo RFC 2328 format.

```bash
# Single-seq mode
python3 tools/parse_bin.py 1032
python3 tools/parse_bin.py 1032 -o parsed_1032.txt
python3 tools/parse_bin.py bin/r1/if0/NBR_FULL/000001_0_010000.bin

# --all mode: biên dịch toàn bộ bin/ → resultbin/ (mirror cấu trúc thư mục)
python3 tools/parse_bin.py --all --workers 8
```

### `tools/ospf_chart.py`

Sinh HTML chart hiển thị timeline trạng thái neighbor của tất cả router. Mỗi router một hàng, màu theo trạng thái (DOWN→INIT→2WAY→EXST→EXCH→LOAD→FULL).

```bash
python3 tools/ospf_chart.py                          # mặc định đọc log/, xuất ra tools/ospf_chart.html
python3 tools/ospf_chart.py --log-dir log -o chart.html
```

### `tools/verify_delivery.py`

Kiểm tra delivery data packet giữa các cặp router. Đọc bin files, tái tạo đường đi và xác nhận gói tin đến được đích.

```bash
python3 tools/verify_delivery.py                     # ghi delivery_report.txt
python3 tools/verify_delivery.py --list-events       # liệt kê các event group có sẵn
python3 tools/verify_delivery.py -e 200,400          # chỉ phân tích event 200 và 400
```

### Cấu trúc thư mục dữ liệu

```
bin/                          ← gói tin OSPF nhị phân (đầu vào cho parse_bin.py)
  {router}/                   ← r1..r10
    {interface}/              ← if0, if1, if2...
      {neighbor_state}/       ← NBR_DOWN, NBR_INIT, NBR_TWOWAY, NBR_EXSTART,
                                NBR_EXCHANGE, NBR_LOADING, NBR_FULL
        {seq:06d}_{simTime}.bin
    client/                   ← gói từ clientGate (không có neighbor state)

log/                          ← JSON dump trạng thái router (đầu vào cho viz_topology_ascii.py)
  {router}/                   ← r1..r10
    {counter}.json

resultlog/                    ← output của viz_topology_ascii.py --all (mirror log/)
  {router}/                   ← r1..r10
    {counter}.txt

resultbin/                    ← output của parse_bin.py --all (mirror bin/)
  {router}/{interface}/{state}/{seq}_{simTime}.txt
```

- **Seq/counter là mã định danh duy nhất** — dùng để tra cứu file không cần biết đường dẫn cha
- `--all` biên dịch 1 lần, viewer đọc file tĩnh → phản hồi microsecond


## Build và chạy

```bash
cd /home/k/omnetpp-6.4.0/workspace/ospf
source /home/k/omnetpp-6.4.0/setenv  # nạp môi trường OMNeT++
make -j$(nproc)
./out/clang-release/ospf -c General --sim-time-limit=180s
```

### Kịch bản Link Flap

Cấu hình trong `link_flaps.txt`:
```
r1 r2 down 100     # t=100: link r1-r2 đứt
```

Định dạng: `<router1> <router2> down|up <thời điểm>`

Để không flap: xóa hết nội dung file hoặc để file trống.


## Giới hạn dự án (scope)

Dự án chỉ implement một tập con của OSPFv2:

- **P2P only** — không broadcast, không NBMA, không DR/BDR
- **Single-area** — chỉ area 0.0.0.0
- **Router-LSA only** — không Network-LSA, Summary-LSA, AS-external-LSA
- **HelloInterval = 10s**, **RouterDeadInterval = 40s** (đúng chuẩn RFC)
- **Cơ chế Link Flap** — xem `docs/disconect_link.txt`
- **No checksum** — môi trường simulation không có lỗi bit
- **No authentication** — không cần trong simulation
