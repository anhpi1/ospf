#!/usr/bin/env python3
"""
OSPF States Visualization — 1 đồ thị duy nhất, 24 đường
Đọc log từ state_dump/{1a,1b1,...}/. Mỗi file = 1 event cho 1 interface.
- Y1 (trái): Neighbor State (line solid)
- Y2 (phải): Data usage % (line dashed)
- Click legend để bật/tắt từng interface

Usage:
    bash tools/viz_states.sh
    bash tools/viz_states.sh -s 1b1
"""

import os, re, sys, argparse
from collections import defaultdict

import plotly.graph_objects as go
from plotly.colors import qualitative

# ─── state mappings ─────────────────────────────────────────────
NBR_STATE_NAMES = ["Down", "Attempt", "Init", "2Way",
                   "ExStart", "Exchange", "Loading", "Full"]
NBR_STATE_VAL  = {n: i for i, n in enumerate(NBR_STATE_NAMES)}

RE_TIMESTAMP = re.compile(r'^t=([\d.]+)\s*[—–-]+\s*(.+)$')
RE_HEADER   = re.compile(r'Router ID:\s*(\d+)\s+ifIndex=(\d+)')
RE_IFSTATE  = re.compile(r'\[\d+\]\s+type=\d+\s+state=(\S+)')
RE_NBRSTATE = re.compile(r'nbr:\s*id=(\d+)\s+state=(\S+)')
RE_DBLIST   = re.compile(r'(databaseSummaryList|linkStateRequestList|linkStateRetransmissionList)\s*\((\d+)\)')


def parse_events(base, subphases=None):
    if subphases is None:
        subphases = sorted(d for d in os.listdir(base)
                           if os.path.isdir(os.path.join(base, d)))

    raw = []

    for sub in subphases:
        d = os.path.join(base, sub)
        if not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d), key=lambda x: int(x.split('_')[0])):
            if not fn.endswith('.log'):
                continue
            fp = os.path.join(d, fn)
            with open(fp) as f:
                text = f.read()

            time = ev = router = ev_ifIdx = None
            interfaces = {}
            current_idx = -1

            for line in text.splitlines():
                m = RE_TIMESTAMP.match(line)
                if m:
                    time = float(m.group(1))
                    ev = m.group(2).strip()
                    continue
                m = RE_HEADER.match(line)
                if m:
                    router = int(m.group(1))
                    ev_ifIdx = int(m.group(2))
                    continue
                m = RE_IFSTATE.search(line)
                if m:
                    current_idx = int(line.strip().split(']')[0].strip('['))
                    interfaces.setdefault(current_idx,
                        {'ifState': m.group(1), 'nid': 0, 'nbrState': 'Down',
                         'db': 0, 'lr': 0, 'rt': 0})
                    continue
                m = RE_NBRSTATE.search(line)
                if m and current_idx >= 0:
                    nd = interfaces.setdefault(current_idx,
                        {'ifState': '?', 'nid': 0, 'nbrState': 'Down',
                         'db': 0, 'lr': 0, 'rt': 0})
                    nd['nid'] = int(m.group(1))
                    nd['nbrState'] = m.group(2)
                    continue
                m = RE_DBLIST.search(line)
                if m and current_idx in interfaces:
                    val = int(m.group(2))
                    if 'databaseSummary' in m.group(1):
                        interfaces[current_idx]['db'] = val
                    elif 'linkStateRequest' in m.group(1):
                        interfaces[current_idx]['lr'] = val
                    elif 'linkStateRetransmission' in m.group(1):
                        interfaces[current_idx]['rt'] = val

            if time is None or router is None or ev_ifIdx is None:
                continue

            changed = {}
            if ev_ifIdx in interfaces:
                idata = interfaces[ev_ifIdx]
                changed[(router, ev_ifIdx)] = (
                    idata['ifState'], idata['nbrState'], idata['nid'],
                    idata['db'], idata['lr'], idata['rt']
                )

            seq_num = int(fn.split("_")[0])
            raw.append((time, fn, ev, router, ev_ifIdx, changed, subphases.index(sub), seq_num))

    raw.sort(key=lambda x: (x[0], x[6], x[7]))  # time, subphase, seq_num
    return raw


def build_timelines(events_global):
    current = {}
    timelines = defaultdict(list)

    for _, _, _, router, ev_ifIdx, changed, _sub, _sq in events_global:
        for key in changed:
            if key not in current:
                current[key] = ('Down', 'Down', 0, 0, 0, 0)

    for seq, (time, fn, ev_name, router, ev_ifIdx, changed, _sub, _sq) in enumerate(events_global):
        key = (router, ev_ifIdx)
        if key in changed:
            current[key] = changed[key]
        ifState, nbrState, nbrId, db, lr, rt = current[key]
        timelines[key].append((seq, ifState, nbrState, ev_name, time, nbrId, db, lr, rt))

    return timelines


# ─── build chart ────────────────────────────────────────────────
COLORS = qualitative.Plotly * 3

def build_figure(timelines, total_events):
    # Tính max data usage toàn cục
    all_totals = []
    for pts in timelines.values():
        for _, _, _, _, _, _, db, lr, rt in pts:
            all_totals.append(lr)        # chỉ tính lr
    global_max = max(all_totals) if all_totals else 1
    if global_max == 0:
        global_max = 1

    fig = go.Figure()
    color_idx = 0

    # === State lines: 1 đường / interface (solid) ===
    for (rid, idx), pts in sorted(timelines.items(), key=lambda kv: (kv[0][0], kv[0][1])):
        seqs    = [p[0] for p in pts]
        nbrVals = [NBR_STATE_VAL.get(p[2], 0) for p in pts]
        nbrIds  = [p[5] for p in pts]
        nbr_hover = []
        for s, p in enumerate(pts):
            totals = p[7]  # lr
            nbr_hover.append(
                f'#{s} t={p[4]:.0f}s | {p[2]} ({p[3]})<br>'
                f'db={p[6]} lr={p[7]} rt={p[8]} total={totals}'
            )

        final_nbr = next((n for n in reversed(nbrIds) if n != 0), 0)
        label = f'R{rid} IF{idx}→R{final_nbr}' if final_nbr else f'R{rid} IF{idx}'

        fig.add_trace(go.Scatter(
            x=seqs, y=nbrVals,
            mode='lines+markers',
            name=label,
            legendgroup=label,
            showlegend=True,
            line=dict(shape='hv', color=COLORS[color_idx % len(COLORS)], width=1.5),
            marker=dict(size=6, symbol='circle',
                        line=dict(width=0.5, color='white')),
            hovertemplate='%{text}<extra></extra>',
            text=nbr_hover,
            yaxis='y',
        ))
        color_idx += 1

    # === Data lines: 1 tổng / router (dashed) ===
    # Chỉ tính linkStateRequestList (số LSA cần fetch) — dbSummary và retrans
    # là temporary, không phản ánh dữ liệu thực tế cần xử lý.
    by_router = defaultdict(list)
    for (rid, idx), pts in timelines.items():
        by_router[rid].append((idx, pts))

    for rid in sorted(by_router.keys()):
        items = by_router[rid]
        # Tính tổng linkStateRequestList tại mỗi event seq
        agg = []
        for seq in range(total_events):
            total = 0
            for _, pts in items:
                # Dùng MAX lr: lr chỉ tăng, không giảm
                lr_val = 0
                for p in pts:
                    if p[0] > seq:
                        break
                    if p[7] > lr_val:
                        lr_val = p[7]
                total += lr_val
            agg.append(total)
        pcts = agg[:]

        # Màu: lấy màu của interface đầu tiên của router này
        c = COLORS[list(by_router.keys()).index(rid) % len(COLORS)]
        label = f'R{rid} data'

        hover = [
            f'seq={s}<br>R{rid} LSAs to fetch: {agg[s]}'
            for s in range(total_events)
        ]

        fig.add_trace(go.Scatter(
            x=list(range(total_events)), y=pcts,
            mode='lines',
            name=label,
            legendgroup=label,
            showlegend=True,
            line=dict(color=c, width=2, dash='dash'),
            hovertemplate='%{text}<extra></extra>',
            text=hover,
            yaxis='y2',
        ))

        color_idx += 1

    n_traces = len(fig.data)
    n_state = len(timelines)       # 24 state lines
    n_data  = n_traces - n_state   # data lines
    fig.update_layout(
        title=dict(text=f'OSPF States — {total_events} events ({global_max} max data)',
                   font=dict(size=14)),
        xaxis=dict(title='Event #', tickmode='linear', tick0=0,
                   dtick=10, gridcolor='#eee'),
        yaxis=dict(title='Neighbor State',
                   tickvals=list(range(len(NBR_STATE_NAMES))),
                   ticktext=NBR_STATE_NAMES,
                   range=[-0.2, len(NBR_STATE_NAMES) - 0.8],
                   gridcolor='#eee'),
        yaxis2=dict(title='LSAs to fetch',
                    range=[0, 11],
                    overlaying='y', side='right',
                    gridcolor='#f0f0f0'),
        legend=dict(
            title=dict(text='Click line to toggle', font=dict(size=10)),
            font=dict(size=8),
            itemsizing='constant',
            traceorder='normal',
            y=0.99,
        ),
        hovermode='x unified',
        plot_bgcolor='white',
        margin=dict(l=80, r=80, t=80, b=60),
        height=700, width=1400,

        # Nút bật/tắt
        updatemenus=[dict(
            type='buttons', direction='right',
            x=1.02, y=1.08, xanchor='left', yanchor='top',
            pad=dict(r=10, t=0),
            buttons=[
                dict(label='☰ All', method='update',
                     args=[{'visible': [True] * n_traces}]),
                dict(label='State', method='update',
                     args=[{'visible': [True] * n_state + ['legendonly'] * n_data}]),
                dict(label='Data', method='update',
                     args=[{'visible': ['legendonly'] * n_state + [True] * n_data}]),
                dict(label='✕ Hide', method='update',
                     args=[{'visible': ['legendonly'] * n_traces}]),
            ],
        )],
    )

    return fig


# ─── main ───────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser(description='OSPF States by Router')
    ap.add_argument('-o', '--output', default='state_timeline.html')
    ap.add_argument('-s', '--subphase', default=None)
    args = ap.parse_args()

    events = parse_events('state_dump',
                          [args.subphase] if args.subphase else None)
    print(f'Global events: {len(events)}')

    timelines = build_timelines(events)
    print(f'Interfaces tracked: {len(timelines)}')

    fig = build_figure(timelines, len(events))
    fig.write_html(args.output, include_plotlyjs='cdn', auto_open=True)
    print(f'Output: {args.output}')


if __name__ == '__main__':
    main()
