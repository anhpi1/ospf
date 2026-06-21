# OSPF Debug Chart Tool Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Xây dựng tool web-based trực quan hóa trạng thái interface OSPF với dual Y-axis ECharts chart — trái Y = neighbor state (step), phải Y = tổng độ dài 3 vector (DSL+LSR+RET).

**Architecture:** Python script `tools/ospf_chart.py` quét toàn bộ `log/**/*.json`, parse trạng thái interface, sắp xếp theo simTime thành timeline toàn cục, rồi nhúng data + ECharts option vào HTML template để sinh ra `tools/ospf_chart.html`. Mở HTML bằng browser, không cần server.

**Tech Stack:** Python 3 (stdlib: json, glob, pathlib), ECharts 5.x (CDN), HTML/CSS/JS

---

## File Structure

| File | Vai trò |
|------|---------|
| `tools/ospf_chart.py` | Script chính: parse, build timeline, render HTML (tạo mới) |
| `tools/ospf_chart.html` | Output, tự sinh bởi script (không commit) |

Không sửa file C++ nào. Dữ liệu `dsl`, `lsr`, `ret` đã có sẵn trong JSON.

---

### Task 1: Parse dữ liệu từ log/

**Files:**
- Create: `tools/ospf_chart.py`

- [ ] **Step 1: Tạo file và viết hàm `parse_all_logs()`**

```python
#!/usr/bin/env python3
"""OSPF Debug Chart Tool — sinh HTML chart từ log/ JSON dumps."""

import json
import glob
import os
import sys
from collections import defaultdict

# ── Constants ──────────────────────────────────────────────────
STATE_MAP = {
    "NBR_DOWN": 0, "NBR_ATTEMPT": 0, "NBR_INIT": 1,
    "NBR_TWOWAY": 2, "NBR_EXSTART": 3, "NBR_EXCHANGE": 4,
    "NBR_LOADING": 5, "NBR_FULL": 6
}
STATE_NAMES = ["DOWN", "INIT", "2WAY", "EXST", "EXCH", "LOAD", "FULL"]

ROUTER_COLORS = {
    "r1": "#e06c75", "r2": "#d19a66", "r3": "#e5c07b",
    "r4": "#98c379", "r5": "#56b6c2", "r6": "#61afef",
    "r7": "#c678dd", "r8": "#be5046", "r9": "#7ec8a0", "r10": "#528bff"
}


def parse_all_logs(log_dir="log"):
    """Quét log/**/*.json, trả về list snapshot thô (chưa sắp xếp)."""
    snapshots = []
    pattern = os.path.join(log_dir, "*", "*.json")
    files = sorted(glob.glob(pattern))

    if not files:
        print(f"ERROR: Không tìm thấy file JSON nào trong {log_dir}/")
        sys.exit(1)

    print(f"Tìm thấy {len(files)} file JSON...")

    for fpath in files:
        try:
            with open(fpath, "r") as f:
                d = json.load(f)
        except (json.JSONDecodeError, IOError) as e:
            print(f"  WARN: bỏ qua {fpath}: {e}")
            continue

        # Trích xuất router name từ đường dẫn: log/r3/123.json → r3
        router = os.path.basename(os.path.dirname(fpath))
        sim_time = d.get("simTime", 0)

        ifaces = []
        for iface in d.get("state", {}).get("interfaces", []):
            nbr = iface.get("neighbor")
            if not nbr:
                continue

            state_str = nbr.get("state", "NBR_DOWN")
            state_val = STATE_MAP.get(state_str, 0)
            dsl = len(nbr.get("databaseSummaryList", []))
            lsr = len(nbr.get("linkStateRequestList", []))
            ret = len(nbr.get("linkStateRetransmissionList", []))

            ifaces.append({
                "idx": iface.get("index", -1),
                "neighborId": nbr.get("IDNeighbor", "0.0.0.0"),
                "state": state_str,
                "stateVal": state_val,
                "dsl": dsl,
                "lsr": lsr,
                "ret": ret,
                "total": dsl + lsr + ret,
                "blocked": iface.get("linkDisabled", False)
            })

        snapshots.append({
            "simTime": sim_time,
            "router": router,
            "interfaces": ifaces
        })

    return snapshots
```

- [ ] **Step 2: Viết hàm `build_timeline()`**

```python
def build_timeline(snapshots):
    """Sắp xếp snapshot theo simTime, gán globalSeq."""
    snapshots.sort(key=lambda s: s["simTime"])

    timeline = []
    for seq, snap in enumerate(snapshots):
        snap["globalSeq"] = seq
        timeline.append(snap)

    print(f"Timeline: {len(timeline)} global sequence points")
    return timeline
```

- [ ] **Step 3: Test parse + timeline với dữ liệu thực tế**

```bash
cd /home/k/omnetpp-6.4.0/workspace/ospf && python3 -c "
import sys; sys.path.insert(0, 'tools')
from ospf_chart import parse_all_logs, build_timeline
snaps = parse_all_logs('log')
tl = build_timeline(snaps)
print(f'Total snapshots: {len(tl)}')
print(f'First: seq={tl[0][\"globalSeq\"]}, router={tl[0][\"router\"]}, time={tl[0][\"simTime\"]}')
print(f'Last:  seq={tl[-1][\"globalSeq\"]}, router={tl[-1][\"router\"]}, time={tl[-1][\"simTime\"]}')
# In các router có trong timeline
routers = set(s['router'] for s in tl)
print(f'Routers: {sorted(routers)}')
# In số interface của router đầu tiên
print(f'First router interfaces: {len(tl[0][\"interfaces\"])}')
"
```

Expected: ~3403 snapshots, routers r1..r10, mỗi snapshot có 1-4 interfaces.

- [ ] **Step 4: Commit**

```bash
git add tools/ospf_chart.py
git commit -m "feat: add parse_all_logs + build_timeline for ospf_chart tool

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 2: Xây dựng ECharts option + HTML template

**Files:**
- Modify: `tools/ospf_chart.py`

- [ ] **Step 1: Viết hàm `build_echarts_option()`**

```python
def build_echarts_option(timeline):
    """Tạo ECharts option object với dual Y-axis, step chart + line chart."""

    # Gom dữ liệu theo interface key: "r2/if0"
    iface_keys = {}  # key → index trong all_keys
    all_keys = []    # ["r2/if0", "r2/if1", ...]

    # Duyệt toàn bộ timeline để thu thập tất cả interface key
    for snap in timeline:
        for iface in snap["interfaces"]:
            key = f"{snap['router']}/if{iface['idx']}"
            if key not in iface_keys:
                iface_keys[key] = len(all_keys)
                all_keys.append(key)

    n_ifaces = len(all_keys)
    if n_ifaces == 0:
        print("ERROR: Không có interface nào trong timeline!")
        sys.exit(1)

    print(f"Tổng số interface: {n_ifaces}")

    # Chuẩn bị data series: mỗi interface 2 series (state + total)
    # state_data[key] = [[globalSeq, stateVal], ...]
    # total_data[key] = [[globalSeq, total], ...]
    state_data = {k: [] for k in all_keys}
    total_data = {k: [] for k in all_keys}

    for snap in timeline:
        seq = snap["globalSeq"]
        for iface in snap["interfaces"]:
            key = f"{snap['router']}/if{iface['idx']}"
            if key in state_data:
                state_data[key].append([seq, iface["stateVal"]])
                total_data[key].append([seq, iface["total"]])

    # Xây dựng series cho ECharts
    series = []
    legend_data = []

    for key in all_keys:
        router = key.split("/")[0]
        color = ROUTER_COLORS.get(router, "#999999")

        # State series (trái Y, step chart)
        series.append({
            "name": key,
            "type": "line",
            "step": "end",
            "yAxisIndex": 0,
            "data": state_data[key],
            "lineStyle": {"color": color, "width": 1.5},
            "itemStyle": {"color": color},
            "symbol": "none",
            "emphasis": {"focus": "series"}
        })

        # Total series (phải Y, line chart)
        series.append({
            "name": key,
            "type": "line",
            "yAxisIndex": 1,
            "data": total_data[key],
            "lineStyle": {"color": color, "width": 1, "type": "dashed"},
            "itemStyle": {"color": color},
            "symbol": "circle",
            "symbolSize": 3,
            "emphasis": {"focus": "series"}
        })

        legend_data.append(key)

    option = {
        "tooltip": {
            "trigger": "axis",
            "formatter": None  # sẽ define trong JS (custom formatter)
        },
        "legend": {
            "type": "scroll",
            "orient": "horizontal",
            "bottom": 0,
            "data": legend_data,
            "textStyle": {"fontSize": 10}
        },
        "grid": {
            "left": 100,
            "right": 80,
            "top": 50,
            "bottom": 60
        },
        "xAxis": {
            "type": "value",
            "name": "Global Sequence",
            "nameLocation": "center",
            "nameGap": 30,
            "min": 0,
            "max": len(timeline) - 1
        },
        "yAxis": [
            {
                "type": "value",
                "name": "Neighbor State",
                "min": 0,
                "max": 6,
                "interval": 1,
                "axisLabel": {
                    "formatter": "{value}"
                }
            },
            {
                "type": "value",
                "name": "Vector Total (items)",
                "min": 0
            }
        ],
        "series": series,
        "dataZoom": [
            {
                "type": "slider",
                "xAxisIndex": 0,
                "start": 0,
                "end": 100,
                "height": 20,
                "bottom": 40
            },
            {
                "type": "inside",
                "xAxisIndex": 0
            }
        ]
    }

    # Nhúng state name map vào option để JS dùng trong tooltip formatter
    option["_stateNames"] = STATE_NAMES
    option["_routerColors"] = ROUTER_COLORS

    return option
```

- [ ] **Step 2: Viết HTML template string trong Python**

```python
HTML_TEMPLATE = r'''<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>OSPF Debug Chart</title>
<script src="https://cdn.jsdelivr.net/npm/echarts@5.5.0/dist/echarts.min.js"></script>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
       background: #1a1a2e; color: #cdd6f4; }
.header { background: #16213e; padding: 10px 20px; display: flex;
          align-items: center; gap: 16px; flex-wrap: wrap;
          border-bottom: 1px solid #45475a; }
.header h1 { font-size: 18px; color: #89b4fa; white-space: nowrap; }
.ctrl-group { display: flex; gap: 6px; flex-wrap: wrap; align-items: center; }
.ctrl-group button { padding: 4px 12px; border: 1px solid #45475a;
    border-radius: 4px; background: #313244; color: #cdd6f4; cursor: pointer;
    font-size: 12px; }
.ctrl-group button:hover { background: #45475a; }
.ctrl-group label { font-size: 12px; cursor: pointer; user-select: none;
    display: flex; align-items: center; gap: 3px; }
.ctrl-group input[type="checkbox"] { cursor: pointer; }
#chart { width: 100%; height: calc(100vh - 60px); }
</style>
</head>
<body>
<div class="header">
  <h1>OSPF Debug Chart</h1>
  <div class="ctrl-group">
    <button onclick="showAll()">Hiện tất cả</button>
    <button onclick="hideAll()">Ẩn tất cả</button>
    <span style="color:#6c7086;margin:0 4px;">|</span>
    ROUTER_FILTERS_PLACEHOLDER
  </div>
</div>
<div id="chart"></div>
<script>
const STATE_NAMES = STATE_NAMES_PLACEHOLDER;
const CHART_OPTION = CHART_OPTION_PLACEHOLDER;

// Custom tooltip formatter
CHART_OPTION.tooltip.formatter = function(params) {
  if (!params || params.length === 0) return '';
  let html = '';
  for (const p of params) {
    if (p.value === undefined || p.value.length < 2) continue;
    const seq = p.value[0];
    const val = p.value[1];
    const name = p.seriesName; // "r2/if0"
    const isState = (p.componentIndex === 0); // heuristic: check yAxisIndex
    // Phân biệt state vs total dựa vào seriesIndex lẻ/chẵn
    const isTotal = (p.seriesIndex % 2 === 1);
    const color = p.color;

    if (isTotal) {
      // Tìm data point tương ứng từ timeline
      const dsl = TIMELINE_DATA[seq]?.interfaces?.find(
        i => name === TIMELINE_DATA[seq].router + '/if' + i.idx
      )?.dsl || 0;
      const lsr = TIMELINE_DATA[seq]?.interfaces?.find(
        i => name === TIMELINE_DATA[seq].router + '/if' + i.idx
      )?.lsr || 0;
      const ret = TIMELINE_DATA[seq]?.interfaces?.find(
        i => name === TIMELINE_DATA[seq].router + '/if' + i.idx
      )?.ret || 0;
      const nbrId = TIMELINE_DATA[seq]?.interfaces?.find(
        i => name === TIMELINE_DATA[seq].router + '/if' + i.idx
      )?.neighborId || '?';
      html += '<div style="margin-bottom:8px;">' +
        '<b style="color:' + color + '">' + name + ' → ' + nbrId + '</b><br>' +
        'databaseSummaryList: &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;' + dsl + '<br>' +
        'linkStateRequestList: &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;' + lsr + '<br>' +
        'linkStateRetransmissionList: ' + ret + '<br>' +
        '────────────────────────<br>' +
        '<b>TOTAL: ' + val + '</b><br>' +
        'Global Seq: ' + seq + '<br>' +
        '</div>';
    } else {
      const state = STATE_NAMES[val] || ('?' + val);
      const nbrId = TIMELINE_DATA[seq]?.interfaces?.find(
        i => name === TIMELINE_DATA[seq].router + '/if' + i.idx
      )?.neighborId || '?';
      html += '<div style="margin-bottom:4px;">' +
        '<b style="color:' + color + '">' + name + ' → ' + nbrId + '</b><br>' +
        'State: <b>' + state + '</b> | Seq: ' + seq + '<br>' +
        '</div>';
    }
  }
  return html || '—';
};

const TIMELINE_DATA = TIMELINE_DATA_PLACEHOLDER;

const chartDom = document.getElementById('chart');
const myChart = echarts.init(chartDom, 'dark');
myChart.setOption(CHART_OPTION);

// Resize khi cửa sổ thay đổi
window.addEventListener('resize', () => myChart.resize());

// ── Toggle controls ──
function showAll() { myChart.dispatchAction({type:'legendAllSelect'}); }
function hideAll() { myChart.dispatchAction({type:'legendInverseSelect'}); }

function toggleRouter(router, checked) {
  const legend = myChart.getModel().getComponentsByType('legend')[0];
  if (!legend) return;
  const allData = legend.get('data');
  allData.forEach(function(name) {
    if (name.startsWith(router + '/')) {
      myChart.dispatchAction({
        type: checked ? 'legendSelect' : 'legendUnSelect',
        name: name
      });
    }
  });
}
</script>
</body>
</html>'''
```

- [ ] **Step 3: Viết hàm `render_html()`**

```python
def render_html(timeline, option, output_path="tools/ospf_chart.html"):
    """Nhúng data + option vào HTML template, ghi ra file."""

    # Tạo router filter checkboxes
    routers = sorted(set(s["router"] for s in timeline))
    router_filters = ""
    for r in routers:
        color = ROUTER_COLORS.get(r, "#999")
        router_filters += (
            f'<label style="color:{color}">'
            f'<input type="checkbox" checked '
            f'onchange="toggleRouter(\'{r}\', this.checked)">'
            f'{r}</label>\n'
        )

    # Serialize timeline data cho JS tooltip
    timeline_js = json.dumps(timeline, ensure_ascii=False)

    # Serialize option (bỏ _stateNames và _routerColors ra, pass riêng)
    state_names_js = json.dumps(option.pop("_stateNames", []))
    router_colors = option.pop("_routerColors", {})  # không cần pass riêng
    option_js = json.dumps(option, ensure_ascii=False)

    # Thay placeholders
    html = HTML_TEMPLATE
    html = html.replace("ROUTER_FILTERS_PLACEHOLDER", router_filters)
    html = html.replace("STATE_NAMES_PLACEHOLDER", state_names_js)
    html = html.replace("CHART_OPTION_PLACEHOLDER", option_js)
    html = html.replace("TIMELINE_DATA_PLACEHOLDER", timeline_js)

    with open(output_path, "w", encoding="utf-8") as f:
        f.write(html)

    file_size = os.path.getsize(output_path)
    print(f"Đã sinh {output_path} ({file_size / 1024 / 1024:.1f} MB)")
    return output_path
```

- [ ] **Step 4: Viết `main()` entry point**

```python
def main():
    import argparse
    parser = argparse.ArgumentParser(
        description="OSPF Debug Chart Tool — sinh HTML chart từ log/ JSON")
    parser.add_argument("--log-dir", default="log",
                        help="Thư mục chứa log (mặc định: log/)")
    parser.add_argument("-o", "--output", default="tools/ospf_chart.html",
                        help="File HTML output (mặc định: tools/ospf_chart.html)")
    args = parser.parse_args()

    snapshots = parse_all_logs(args.log_dir)
    timeline = build_timeline(snapshots)
    option = build_echarts_option(timeline)
    path = render_html(timeline, option, args.output)
    print(f"\n✅ Mở {path} trong browser để xem chart.")


if __name__ == "__main__":
    main()
```

- [ ] **Step 5: Chạy thử sinh HTML**

```bash
cd /home/k/omnetpp-6.4.0/workspace/ospf && python3 tools/ospf_chart.py
```

Expected: Tạo `tools/ospf_chart.html` (~2-4 MB), không lỗi.

- [ ] **Step 6: Kiểm tra HTML mở được và chart render đúng**

```bash
# Kiểm tra file tồn tại và có nội dung
ls -lh /home/k/omnetpp-6.4.0/workspace/ospf/tools/ospf_chart.html
# Kiểm tra các placeholder đã được thay
grep -c "PLACEHOLDER" /home/k/omnetpp-6.4.0/workspace/ospf/tools/ospf_chart.html || echo "OK: no placeholders"
```

Expected: File > 1MB, không còn PLACEHOLDER nào.

- [ ] **Step 7: Commit**

```bash
git add tools/ospf_chart.py
git commit -m "feat: complete ospf_chart.py with ECharts option builder + HTML render

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 3: Fix & polish sau khi mở browser kiểm tra

**Files:**
- Modify: `tools/ospf_chart.py`

- [ ] **Step 1: Mở HTML trong browser, kiểm tra các mục sau**

Tự mở `tools/ospf_chart.html` bằng browser:
- [ ] Chart render được, có 2 trục Y
- [ ] Trục Y trái hiển thị state (step chart)
- [ ] Trục Y phải hiển thị vector total (line chart)
- [ ] Tooltip khi hover: hiển thị state hoặc vector breakdown
- [ ] Toggle "Hiện tất cả" / "Ẩn tất cả" hoạt động
- [ ] Checkbox router bật/tắt hoạt động
- [ ] Click legend entry bật/tắt từng interface
- [ ] DataZoom slider hoạt động
- [ ] Resize cửa sổ không vỡ layout

- [ ] **Step 2: Sửa các lỗi phát sinh**

Nếu tooltip không hiển thị đúng vector breakdown, sửa formatter trong HTML template:

```python
# Trong HTML_TEMPLATE, thay đoạn tooltip formatter bằng code chính xác hơn:

# Cần phân biệt state series (yAxisIndex=0) vs total series (yAxisIndex=1)
# Cách đơn giản: dựa vào seriesIndex lẻ/chẵn trong mảng series
```

Sửa trực tiếp trong `HTML_TEMPLATE` string của `tools/ospf_chart.py`.

- [ ] **Step 3: Commit fix**

```bash
git add tools/ospf_chart.py
git commit -m "fix: polish ospf_chart tooltip and interactions

Co-Authored-By: Claude <noreply@anthropic.com>"
```
