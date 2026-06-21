#!/usr/bin/env python3
"""OSPF Debug Chart Tool — sinh HTML chart từ log/ JSON dumps."""

import json
import glob
import os
import sys

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


def build_timeline(snapshots):
    """Sắp xếp snapshot theo simTime, gán globalSeq."""
    snapshots.sort(key=lambda s: s["simTime"])

    timeline = []
    for seq, snap in enumerate(snapshots):
        snap["globalSeq"] = seq
        timeline.append(snap)

    print(f"Timeline: {len(timeline)} global sequence points")
    return timeline


def build_echarts_option(timeline):
    """Tạo ECharts option object với dual Y-axis, step chart + line chart."""

    # Gom dữ liệu theo interface key: "r2/if0"
    iface_keys = {}
    all_keys = []

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
        state_name = key + " [state]"
        series.append({
            "name": state_name,
            "type": "line",
            "step": "end",
            "yAxisIndex": 0,
            "data": state_data[key],
            "lineStyle": {"color": color, "width": 1.5},
            "itemStyle": {"color": color},
            "symbol": "none",
        })

        # Total series (phải Y, line chart)
        total_name = key + " [total]"
        series.append({
            "name": total_name,
            "type": "line",
            "yAxisIndex": 1,
            "data": total_data[key],
            "lineStyle": {"color": color, "width": 1, "type": "dashed"},
            "itemStyle": {"color": color},
            "symbol": "circle",
            "symbolSize": 3,
        })

        legend_data.append(state_name)
        legend_data.append(total_name)

    option = {
        "tooltip": {
            "trigger": "axis"
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

    return option


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
    <span style="color:#6c7086;margin:0 4px;">|</span>
    <button onclick="zoomIn()" title="Phóng to">🔍➕</button>
    <button onclick="zoomOut()" title="Thu nhỏ">🔍➖</button>
    <button onclick="zoomReset()" title="Reset zoom">↺</button>
  </div>
</div>
<div id="chart"></div>
<script>
var STATE_NAMES = STATE_NAMES_PLACEHOLDER;
var CHART_OPTION = CHART_OPTION_PLACEHOLDER;
var TIMELINE_DATA = TIMELINE_DATA_PLACEHOLDER;

// Thay đổi trục Y trái hiển thị tên state
CHART_OPTION.yAxis[0].axisLabel.formatter = function(value) {
  return STATE_NAMES[value] || String(value);
};

// Custom tooltip formatter
CHART_OPTION.tooltip.formatter = function(params) {
  if (!params || params.length === 0) return '';
  var html = '';
  for (var i = 0; i < params.length; i++) {
    var p = params[i];
    if (!p.value || p.value.length < 2) continue;
    var seq = p.value[0];
    var val = p.value[1];
    var name = p.seriesName;
    var color = p.color;
    // Detect type from series name suffix
    var isTotal = (name.indexOf(' [total]') !== -1);
    // Strip suffix to get base key: "r2/if0 [total]" → "r2/if0"
    var baseKey = name.replace(' [state]', '').replace(' [total]', '');

    // Tìm interface data trong timeline
    var snap = TIMELINE_DATA[seq];
    var ifaceData = null;
    if (snap && snap.interfaces) {
      for (var j = 0; j < snap.interfaces.length; j++) {
        var ifKey = snap.router + '/if' + snap.interfaces[j].idx;
        if (ifKey === baseKey) { ifaceData = snap.interfaces[j]; break; }
      }
    }

    if (isTotal && ifaceData) {
      html += '<div style="margin-bottom:8px;">' +
        '<b style="color:' + color + '">' + baseKey + ' → ' + ifaceData.neighborId + '</b><br>' +
        'databaseSummaryList: &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;' + ifaceData.dsl + '<br>' +
        'linkStateRequestList: &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;' + ifaceData.lsr + '<br>' +
        'linkStateRetransmissionList: ' + ifaceData.ret + '<br>' +
        '────────────────────────<br>' +
        '<b>TOTAL: ' + val + '</b> | Seq: ' + seq + '<br>' +
        '</div>';
    } else if (!isTotal && ifaceData) {
      var stateName = STATE_NAMES[val] || ('?' + val);
      html += '<div style="margin-bottom:4px;">' +
        '<b style="color:' + color + '">' + baseKey + ' → ' + ifaceData.neighborId + '</b><br>' +
        'State: <b>' + stateName + '</b> | Seq: ' + seq + '<br>' +
        '</div>';
    }
  }
  return html || '';
};

var chartDom = document.getElementById('chart');
var myChart = echarts.init(chartDom, 'dark');
myChart.setOption(CHART_OPTION);

window.addEventListener('resize', function() { myChart.resize(); });

function showAll() { myChart.dispatchAction({type: 'legendAllSelect'}); }
function hideAll() {
  var legend = myChart.getModel().getComponentsByType('legend')[0];
  if (!legend) return;
  var allData = legend.get('data');
  for (var i = 0; i < allData.length; i++) {
    myChart.dispatchAction({type: 'legendUnSelect', name: allData[i]});
  }
}

function toggleRouter(router, checked) {
  var legend = myChart.getModel().getComponentsByType('legend')[0];
  if (!legend) return;
  var allData = legend.get('data');
  for (var i = 0; i < allData.length; i++) {
    var name = allData[i];
    // Match both "r2/if0 [state]" and "r2/if0 [total]"
    if (name.indexOf(router + '/') === 0) {
      myChart.dispatchAction({
        type: checked ? 'legendSelect' : 'legendUnSelect',
        name: name
      });
    }
  }
}

// ── Zoom controls ──
function zoomIn() {
  var option = myChart.getOption();
  if (!option.dataZoom || !option.dataZoom[0]) return;
  var dz = option.dataZoom[0];
  var start = dz.start || 0;
  var end = dz.end || 100;
  var range = end - start;
  if (range <= 1) return; // đã zoom tối đa
  var center = (start + end) / 2;
  var newRange = Math.max(1, range * 0.5);
  var newStart = Math.max(0, center - newRange / 2);
  var newEnd = Math.min(100, center + newRange / 2);
  myChart.dispatchAction({type: 'dataZoom', dataZoomIndex: 0, start: newStart, end: newEnd});
}

function zoomOut() {
  var option = myChart.getOption();
  if (!option.dataZoom || !option.dataZoom[0]) return;
  var dz = option.dataZoom[0];
  var start = dz.start || 0;
  var end = dz.end || 100;
  var range = end - start;
  if (range >= 100) return; // đã zoom out tối đa
  var center = (start + end) / 2;
  var newRange = Math.min(100, range * 2);
  var newStart = Math.max(0, center - newRange / 2);
  var newEnd = Math.min(100, center + newRange / 2);
  myChart.dispatchAction({type: 'dataZoom', dataZoomIndex: 0, start: newStart, end: newEnd});
}

function zoomReset() {
  myChart.dispatchAction({type: 'dataZoom', dataZoomIndex: 0, start: 0, end: 100});
}
</script>
</body>
</html>'''


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

    # Serialize data cho JS
    timeline_js = json.dumps(timeline, ensure_ascii=False)
    state_names_js = json.dumps(STATE_NAMES, ensure_ascii=False)
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
