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
RE_LSDB     = re.compile(r'LSDB:\s*(\d+)\s+Router')
RE_RTABLE   = re.compile(r'RoutingTable:\s*(\d+)\s+entries')


def parse_events(base, subphases=None):
    if subphases is None:
        subphases = sorted(d for d in os.listdir(base)
                           if os.path.isdir(os.path.join(base, d)))

    raw = []

    for sub in subphases:
        d = os.path.join(base, sub)
        if not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d), key=lambda x: int(x.split('_')[0].split('.')[0])):
            if not fn.endswith('.log'):
                continue
            fp = os.path.join(d, fn)
            with open(fp) as f:
                text = f.read()

            time = ev = router = ev_ifIdx = None
            router_data = {}  # per-router: lsdb, rt
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
                    continue
                m = RE_LSDB.search(line)
                if m and router is not None:
                    router_data['lsdb'] = int(m.group(1))
                    continue
                m = RE_RTABLE.search(line)
                if m and router is not None:
                    router_data['rtable'] = int(m.group(1))
                    continue

            if time is None or router is None or ev_ifIdx is None:
                continue

            changed = {}
            if ev_ifIdx in interfaces:
                idata = interfaces[ev_ifIdx]
                changed[(router, ev_ifIdx)] = (
                    idata['ifState'], idata['nbrState'], idata['nid'],
                    idata['db'], idata['lr'], idata['rt'],
                    router_data.get('lsdb', 0), router_data.get('rtable', 0)
                )

            seq_num = int(fn.split("_")[0])
            raw.append((time, fn, ev, router, ev_ifIdx, changed, subphases.index(sub), seq_num))

    raw.sort(key=lambda x: (x[0], x[6], x[7]))  # time, subphase, seq_num
    return raw


def build_timelines(events_global):
    current = {}
    router_current = {}  # per-router: lsdb, rtable
    timelines = defaultdict(list)
    router_timelines = defaultdict(list)

    # Thu thập tất cả router ID xuất hiện
    all_rids = set()
    for _, _, _, router, ev_ifIdx, changed, _sub, _sq in events_global:
        all_rids.add(router)
        for (r, i) in changed:
            all_rids.add(r)
            if (r, i) not in current:
                current[(r, i)] = ('Down', 'Down', 0, 0, 0, 0, 0, 0)

    # Khởi tạo router_current = 0 cho tất cả router
    for rid in all_rids:
        router_current[rid] = (0, 0)

    for seq, (time, fn, ev_name, router, ev_ifIdx, changed, _sub, _sq) in enumerate(events_global):
        key = (router, ev_ifIdx)
        if key in changed:
            current[key] = changed[key]
        ifState, nbrState, nbrId, db, lr, rt, lsdb, rtable = current[key]
        timelines[key].append((seq, ifState, nbrState, ev_name, time, nbrId, db, lr, rt, lsdb, rtable))
        # Cập nhật router data nếu event có dữ liệu
        if lsdb > 0 or rtable > 0:
            router_current[router] = (lsdb, rtable)
        # Snapshot tất cả router tại seq này
        for rid in sorted(router_current.keys()):
            router_timelines[rid].append((seq, router_current[rid][0], router_current[rid][1]))

    return timelines, router_timelines


# ─── build chart ────────────────────────────────────────────────
COLORS = qualitative.Plotly * 3

def build_figure(timelines, router_timelines, total_events):
    fig = go.Figure()
    color_idx = 0

    # === State lines: 1 đường / interface (solid) ===
    for (rid, idx), pts in sorted(timelines.items(), key=lambda kv: (kv[0][0], kv[0][1])):
        seqs    = [p[0] for p in pts]
        nbrVals = [NBR_STATE_VAL.get(p[2], 0) for p in pts]
        nbrIds  = [p[5] for p in pts]
        nbr_hover = []
        for s, p in enumerate(pts):
            nbr_hover.append(
                f'<b>Event #{s}</b>  t={p[4]:.0f}s<br>'
                f'<b>R{rid} IF{idx}</b>→R{p[5] if p[5] else "?"}<br>'
                f'──── State ────<br>'
                f'Interface: {p[1]}<br>'
                f'Neighbor:  <b>{p[2]}</b> ({p[3]})<br>'
                f'──── Per-IF Data ────<br>'
                f'DB Summary List:       {p[6]} items<br>'
                f'LinkStateReq List:     <b>{p[7]}</b> items<br>'
                f'LinkStateRetr List:    {p[8]} items<br>'
                f'──── Router Data ────<br>'
                f'LSDB (Router-LSAs):    <b>{p[9]}</b><br>'
                f'RoutingTable entries:  {p[10]}'
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

    # === Data lines: 1 đường / router, hover chi tiết từng thành phần ===
    by_router = defaultdict(list)
    for (rid, idx), pts in timelines.items():
        by_router[rid].append((idx, pts))

    for rid in sorted(by_router.keys()):
        items = by_router[rid]
        # Build aggregated data per event
        agg_all = []
        for seq in range(total_events):
            total_db = total_lr = total_rtr = 0
            detail_if = []
            for ifIdx, pts in items:
                db_v = lr_v = rtr_v = 0
                for p in pts:
                    if p[0] > seq: break
                    db_v = p[6]; lr_v = p[7]; rtr_v = p[8]
                total_db += db_v; total_lr += lr_v; total_rtr += rtr_v
                if db_v or lr_v or rtr_v:
                    detail_if.append(f'IF{ifIdx}: db={db_v} lr={lr_v} rt={rtr_v}')
            # Per-router data
            lsdb_v = rtable_v = 0
            for p in router_timelines.get(rid, []):
                if p[0] > seq: break
                lsdb_v = p[1]; rtable_v = p[2]
            agg_all.append((total_db, total_lr, total_rtr, lsdb_v, rtable_v, detail_if))

        c = COLORS[list(by_router.keys()).index(rid) % len(COLORS)]
        hover = []
        y_vals = []
        for s in range(total_events):
            db, lr, rtr, lsdb, rtable, details = agg_all[s]
            # Y value = tổng tất cả vector fields
            y = db + lr + rtr + lsdb + rtable
            y_vals.append(y)
            detail_str = '<br>'.join(details) if details else '<i>(all empty)</i>'
            hover.append(
                f'<b>Event #{s}</b>  |  <b>R{rid}</b><br>'
                f'──── Per Interface ────<br>'
                f'{detail_str}<br>'
                f'──── Router Totals ────<br>'
                f'DB Summary List:       {db}<br>'
                f'LinkStateReq List:     {lr}<br>'
                f'LinkStateRetr List:    {rtr}<br>'
                f'LSDB (Router-LSAs):    {lsdb}<br>'
                f'RoutingTable entries:  {rtable}<br>'
                f'<b>Tổng data: {y}</b>'
            )

        fig.add_trace(go.Scatter(
            x=list(range(total_events)), y=y_vals,
            mode='lines',
            name=f'R{rid} data',
            legendgroup=f'R{rid}',
            showlegend=True,
            line=dict(color=c, width=2, dash='dash'),
            hovertemplate='%{text}<extra></extra>',
            text=hover,
            yaxis='y2',
        ))

    n_traces = len(fig.data)
    n_state = len(timelines)
    n_data  = n_traces - n_state
    # Y2 range based on max tổng data
    all_y = []
    for pts in timelines.values():
        for p in pts:
            all_y.append(p[6] + p[7] + p[8] + p[9] + p[10])
    for pts in router_timelines.values():
        for p in pts:
            all_y.append(p[1] + p[2])
    global_max = max(all_y) if all_y else 10
    fig.update_layout(
        title=dict(text=f'OSPF States — {total_events} events — Data = DB+Lr+RT+LSDB+RTab',
                   font=dict(size=14)),
        xaxis=dict(title='Event #', tickmode='linear', tick0=0,
                   dtick=max(1, total_events // 20), range=[-0.5, total_events - 0.5],
                   gridcolor='#eee'),
        yaxis=dict(title='Neighbor State',
                   tickvals=list(range(len(NBR_STATE_NAMES))),
                   ticktext=NBR_STATE_NAMES,
                   range=[-0.2, len(NBR_STATE_NAMES) - 0.8],
                   gridcolor='#eee'),
        yaxis2=dict(title='Total data (items)',
                    range=[0, global_max + 1],
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

    timelines, router_timelines = build_timelines(events)
    print(f'Interfaces tracked: {len(timelines)}')

    fig = build_figure(timelines, router_timelines, len(events))
    fig.write_html(args.output, include_plotlyjs='cdn', auto_open=True)
    print(f'Output: {args.output}')


if __name__ == '__main__':
    main()
