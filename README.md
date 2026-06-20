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

### `tools/viz_topology_ascii.py`

Đọc file JSON dump OSPF, vẽ topology ASCII + bảng định tuyến + trạng thái interface cho **toàn bộ 10 router**. Cách hoạt động:

- Chọn 1 file dump làm thời điểm tham chiếu (`t_ref`)
- Với mỗi router, tự động tìm file JSON có `simTime` gần `t_ref` nhất (delta tối thiểu)
- Tìm kiếm 2 chiều: tiến và lùi theo thời gian
- Vẽ topology với vị trí node cố định, link P2P là dấu `.`
- Đánh dấu router tham chiếu bằng `R#` và `<--`

```bash
# Liệt kê các dump
python3 tools/viz_topology_ascii.py --list

# Vẽ topology + routing table (từ thời điểm file 100.json)
python3 tools/viz_topology_ascii.py log/100.json

# Dump mới nhất
python3 tools/viz_topology_ascii.py --latest

# Ghi ra file
python3 tools/viz_topology_ascii.py log/100.json -o output.txt
```

Output là file text chứa: topology ASCII + bảng định tuyến + trạng thái interface của **từng router trong 10 router**.


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
