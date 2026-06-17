# OSPFv2 Simulation trên OMNeT++ 6.4

Mô phỏng giao thức OSPFv2 (RFC 2328) single-area, P2P links, 10 router + 10 client.
 


## Tài liệu quan trọng nhất

| File | Mô tả |
|------|-------|
| [`docs/ospf_flow_v2.txt`](docs/ospf_flow_v2.txt) | **Sơ đồ luồng tổng thể** — đây là file duy nhất mô tả toàn bộ hoạt động ospf chỉ cần đọc file này các file linh tinh khác rất dài tốn thời gian và chủ yếu do AI tạo. **Phải đọc đầu tiên.** |
| [`docs/ospfv2.txt`](docs/ospfv2.txt) | RFC 2328 full text (tiếng Việt) — tham chiếu kỹ thuật gốc. |
| [`docs/plan thinking/QUESTIONS.md`](docs/plan%20thinking/QUESTIONS.md) | **Người đọc bắt buộc phải đọc.** Ghi lại toàn bộ câu hỏi thiết kế và giới hạn phạm vi dự án: tại sao chỉ P2P? Tại sao không có DR/BDR? Tại sao không có checksum? Timer rút gọn thế nào? — Tất cả đều có câu trả lời ở đây. |
| [`docs/plan thinking/DECISION-LOG.md`](docs/plan%20thinking/DECISION-LOG.md) | **Nhật ký kỹ thuật.** Ghi lại từng quyết định thiết kế, lý do, và hệ quả. Ví dụ: tại sao chọn cost random 1-30, tại sao không dùng INET, vì sao không thể implement toàn bộ RFC 2328. |
| [`docs/nhật kí kĩ thuật.txt`](docs/nh%E1%BA%ADt%20k%C3%AD%20k%C4%A9%20thu%E1%BA%ADt.txt) | **Nhật ký coding thực tế.** Ghi lại giơi hạn của omnetpp và hướng xử lý



## Cấu trúc quan trọng

| Đường dẫn | Mô tả |
|-----------|-------|
| `ospf.msg` | Định nghĩa message class — sinh tự động ra `ospf_m.h`/`ospf_m.cc` |
| `ospf.h` / `ospf.cc` | Module routerOspf — code chính của toàn bộ dự án |
| `ospf_struct.h` | Cấu trúc dữ liệu OSPF (InterfaceData, NeighborData, LSA, RoutingTableEntry, ...) |
| `state_dump/` | Log trạng thái sau mỗi transition — **dữ liệu đầu vào cho tools** |
| `docs/specs/` | Đặc tả kỹ thuật từng subphase (0 → 1a → 1b1 → 1b2 → 1c → 2a → 2b) |



## Hướng dẫn đọc code

Bắt đầu từ `ospf.h` — lớp `routerOspf` chỉ có 3 hàm chính:

- **`initialize()`** — khởi tạo state, gửi Hello đầu tiên, khởi tạo các bộ timer
- **`handleMessage(cMessage *msg)`** chứa luồng xử lý chính thay đổi các trạng thái mọi gói tin đều được xử lý ở đây
- **`finish()`** — dọn dẹp timer, ghi state dump cuối



## Phân tích kết quả — Chỉ dùng tool, không dùng IDE

**CẢNH BÁO:** Không mở `state_dump/` bằng IDE để đọc thủ công. Dữ liệu log quá lớn (hàng trăm file).  
Phân tích phải thông qua các tool chuyên dụng:

| Tool | Chức năng | Dùng cho giai đoạn |
|------|-----------|--------------------|
| [`tools/2b_analyze_results.py`](tools/2b_analyze_results.py) | Phân tích 90 test case forwarding — dựng đường đi + thống kê | Phase 2b |
| [`tools/viz_states.py`](tools/viz_states.py) | Trực quan hóa trạng thái router (interface, neighbor, LSDB) | Mọi giai đoạn |
| [`tools/show_routes.py`](tools/show_routes.py) | Hiển thị routing table của tất cả router | Phase 2a, 2b |
| [`tools/plot_lsdb.py`](tools/plot_lsdb.py) | Vẽ topology từ LSDB → `.dot` / `.png` | Phase 1c, 2a |

### Usage

```bash
# Phân tích forwarding test (Phase 2b)
python3 tools/2b_analyze_results.py
# → Kết quả: tools/2b_forwarding_test_result.txt

# Vẽ topology
python3 tools/plot_lsdb.py
# → tools/ospf_topology.png

# Hiển thị routing table
python3 tools/show_routes.py
```

Mỗi tool đọc từ `state_dump/`, xử lý, và ghi kết quả ra file riêng.  
Không cần mở IDE — tất cả đều chạy từ command line.


## Build và chạy

```bash
source /home/k/omnetpp-6.4.0/setenv
make
./ospf_sim -c General -u Cmdenv --sim-time-limit=60s
```

## Giới hạn dự án (scope)

Dự án chỉ implement một tập con của OSPFv2. Đọc QUESTIONS.md để hiểu lý do:

- **P2P only** — không broadcast, không NBMA, không DR/BDR
- **Single-area** — chỉ area 0.0.0.0
- **Router-LSA only** — không Network-LSA, Summary-LSA, AS-external-LSA
- **Timer rút gọn 10×** — Hello 1s (chuẩn 10s), Dead 4s (chuẩn 40s), ...
- **No checksum** — môi trường simulation không có lỗi bit
- **No authentication** — không cần trong simulation
