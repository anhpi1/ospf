# Spec: OSPF Debug Chart Tool — Trực quan hóa trạng thái Interface

**Ngày:** 2026-06-21
**Mục tiêu:** Xây dựng tool web-based để debug trạng thái interface của từng router OSPF theo thời gian, trực quan hóa neighbor state và danh sách chờ (database summary, request, retransmission).

---

## 1. Tổng quan

Tool đọc dữ liệu từ thư mục `log/` (JSON dump của `dumpStateToJson()`) của tất cả router, sắp xếp theo thời gian mô phỏng thành một timeline toàn cục, rồi render thành biểu đồ tương tác trên browser.

## 2. Công nghệ

| Thành phần | Lựa chọn |
|-----------|----------|
| Nền tảng | Web App (HTML + JS), mở bằng browser |
| Chart | ECharts 5.x (CDN) |
| Pipeline | Python script (`tools/ospf_chart.py`) sinh HTML tĩnh, nhúng toàn bộ data |
| Dữ liệu nguồn | File JSON trong `log/{router}/` |

## 3. Luồng dữ liệu

```
log/r1/*.json  ─┐
log/r2/*.json  ─┤
   ...          ├──► ospf_chart.py ──► ospf_chart.html ──► Browser
log/r10/*.json ─┘
```

1. Python glob đệ quy `log/**/*.json`
2. Parse từng file → trích xuất: `routerId`, `simTime`, mỗi interface: `neighbor.state`, `dsl`, `lsr`, `ret`
3. Sắp xếp toàn bộ theo `simTime` → gán `globalSeq` (0, 1, 2, ...)
4. Nhúng toàn bộ timeline vào biến JS trong HTML template
5. Mở HTML bằng browser, ECharts render

## 4. Thiết kế Chart

### 4.1. Bố cục

- **1 ECharts instance**, 1 grid, 1 panel
- **Trục X:** Global sequence (0 → N, với N = tổng số file JSON)
- **Trục Y trái:** Neighbor state — 7 mức: `NBR_DOWN(0)`, `NBR_INIT(1)`, `NBR_TWOWAY(2)`, `NBR_EXSTART(3)`, `NBR_EXCHANGE(4)`, `NBR_LOADING(5)`, `NBR_FULL(6)`. Hiển thị tên state thay vì số.
- **Trục Y phải:** Tổng độ dài 3 vector (`dsl + lsr + ret`), đơn vị: số lượng.

### 4.2. Series

- Mỗi interface của mỗi router → **2 series** (cùng tên):
  - **State series:** `type: 'line'`, `step: 'end'`, `yAxisIndex: 0` (trái Y)
  - **Vector series:** `type: 'line'`, `yAxisIndex: 1` (phải Y)
- Cùng tên → ECharts legend tự động gom thành 1 entry. Click legend toggle cả 2 series đồng thời.
- Tổng số: ~24 interface × 2 = ~48 series.

### 4.3. Màu sắc

Mỗi router một màu chủ đạo, mỗi interface trong router đó là một sắc thái:

| Router | Màu chính |
|--------|----------|
| r1 | #e06c75 |
| r2 | #d19a66 |
| r3 | #e5c07b |
| r4 | #98c379 |
| r5 | #56b6c2 |
| r6 | #61afef |
| r7 | #c678dd |
| r8 | #be5046 |
| r9 | #7ec8a0 |
| r10 | #528bff |

### 4.4. Hover Tooltip

**Khi hover vào state line:**
```
r2 / if1 → neighbor 0.0.0.3
State: NBR_EXCHANGE
Global Seq: 847 | simTime: 25.3s
```

**Khi hover vào vector line:**
```
r2 / if1 → neighbor 0.0.0.3
databaseSummaryList:      10
linkStateRequestList:       5
linkStateRetransmissionList: 2
────────────────────────────
TOTAL:                     17
Global Seq: 847 | simTime: 25.3s
```

### 4.5. Toggle / Điều khiển

- **Thanh điều khiển trên cùng:**
  - Nút `[Hiện tất cả]` — bật tất cả series
  - Nút `[Ẩn tất cả]` — tắt tất cả series
  - Checkbox `☑ r1` ... `☑ r10` — bật/tắt tất cả interface của router đó
- **ECharts Legend (dưới cùng):** Click từng entry để bật/tắt interface riêng lẻ

## 5. Cấu trúc dữ liệu nhúng

```javascript
const timeline = [
  {
    globalSeq: 0,
    simTime: 0.0,
    router: "r1",
    interfaces: [
      {
        idx: 0,
        neighborId: "0.0.0.2",
        state: "NBR_DOWN",    // → map sang số 0-6 cho chart
        stateVal: 0,
        dsl: 0,               // databaseSummaryList size
        lsr: 0,               // linkStateRequestList size
        ret: 0,               // linkStateRetransmissionList size
        total: 0              // dsl + lsr + ret
      },
      ...
    ]
  },
  ...
];
```

## 6. Cấu trúc file

```
tools/
  ospf_chart.py          ← Script chính (~200 dòng Python)
  ospf_chart.html         ← Output (tự sinh, không commit)
```

## 7. Hàm chính trong ospf_chart.py

- `parse_all_logs(log_dir)` → `List[dict]`: quét `log/**/*.json`, parse, trả về list các snapshot
- `build_timeline(snapshots)` → `List[dict]`: sắp xếp theo simTime, gán globalSeq, tạo cấu trúc `timeline[]`
- `build_echarts_option(timeline)` → `dict`: tạo ECharts option object với dual Y-axis, step chart, line chart, legend
- `render_html(timeline, option, output_path)`: nhúng data + option vào template HTML, ghi ra file
- `main()`: argparse, gọi pipeline

## 8. Giới hạn & Phạm vi

- **Không sửa C++**: Dữ liệu JSON hiện tại đã có đủ `dsl`, `lsr`, `ret` (số lượng). Không serialize nội dung chi tiết từng LSAHeader.
- **Chỉ đọc từ `log/`**: không dùng dữ liệu từ `bin/` (binary dump)
- **Single-area, P2P only**: phù hợp với scope mô phỏng hiện tại
- **10 router cố định**: phù hợp topology `ospf.ned`
