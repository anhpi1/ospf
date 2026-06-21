#!/usr/bin/env python3
"""
OSPF Tools Web Viewer — HTTP server + HTML frontend.
Server đọc file tĩnh từ resultlog/ và resultbin/ (đã biên dịch trước bằng --all).

Usage:
    # Bước 1: Biên dịch toàn bộ (chạy 1 lần)
    python3 tools/viz_topology_ascii.py --all
    python3 tools/parse_bin.py --all

    # Bước 2: Chạy server
    python3 tools/viewer.py [--port PORT]
    Sau đó mở browser: http://localhost:8080
"""

import argparse
import glob as _glob
import http.server
import os
import sys
import urllib.parse

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

HTML = r"""<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>OSPF Tools Viewer</title>
<style>
  :root {
    --bg: #f5f5f5;
    --surface: #ffffff;
    --primary: #e0e0e0;
    --accent: #1976d2;
    --text: #222;
    --text-dim: #666;
    --border: #ccc;
    --pre-bg: #fafafa;
    --btn-hover: #bbdefb;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: 'Segoe UI', system-ui, sans-serif;
    background: var(--bg);
    color: var(--text);
    height: 100vh;
    display: flex;
    flex-direction: column;
  }
  header {
    background: var(--surface);
    padding: 8px 16px;
    display: flex;
    align-items: center;
    gap: 16px;
    border-bottom: 1px solid var(--border);
  }
  header h1 { font-size: 1.1rem; color: var(--accent); white-space: nowrap; }
  .tabs { display: flex; gap: 4px; }
  .tab {
    background: var(--primary);
    color: var(--text-dim);
    border: 1px solid var(--border);
    padding: 6px 16px;
    cursor: pointer;
    border-radius: 6px 6px 0 0;
    font-size: 0.9rem;
    transition: background .15s;
  }
  .tab.active { background: var(--accent); color: #fff; border-color: var(--accent); }
  .tab:hover:not(.active) { background: var(--btn-hover); color: var(--text); }
  .toolbar {
    background: var(--surface);
    padding: 10px 16px;
    display: flex;
    align-items: center;
    gap: 8px;
    border-bottom: 1px solid var(--border);
    flex-wrap: wrap;
  }
  .toolbar label { font-size: 0.85rem; color: var(--text-dim); }
  .toolbar input[type=number] {
    background: var(--bg);
    color: var(--text);
    border: 1px solid var(--border);
    padding: 6px 10px;
    border-radius: 4px;
    font-size: 0.95rem;
    width: 100px;
    font-family: 'Courier New', monospace;
    text-align: center;
  }
  .toolbar input[type=number]:focus { outline: none; border-color: var(--accent); }
  .btn {
    padding: 6px 14px;
    border-radius: 4px;
    border: 1px solid var(--border);
    cursor: pointer;
    font-size: 0.85rem;
    font-weight: 600;
    transition: background .15s, border-color .15s;
    color: var(--text);
  }
  .btn-prev { background: var(--primary); }
  .btn-next { background: var(--primary); }
  .btn-run  { background: var(--accent); border-color: var(--accent); color: #fff; }
  .btn:hover { filter: brightness(0.95); }
  .btn:active { filter: brightness(0.9); }
  .status {
    font-size: 0.8rem;
    color: var(--text-dim);
    margin-left: auto;
    white-space: nowrap;
  }
  .status.error { color: #c62828; }
  .status.ok { color: #2e7d32; }
  main {
    flex: 1;
    overflow: auto;
    padding: 0;
  }
  pre {
    background: var(--pre-bg);
    color: #333;
    padding: 16px;
    margin: 0;
    min-height: 100%;
    font-family: 'Courier New', 'Liberation Mono', monospace;
    font-size: 0.82rem;
    line-height: 1.45;
    white-space: pre-wrap;
    word-break: break-all;
  }
  .placeholder {
    display: flex;
    align-items: center;
    justify-content: center;
    height: 100%;
    color: var(--text-dim);
    font-size: 1rem;
  }
  kbd {
    background: var(--primary);
    border: 1px solid var(--border);
    border-radius: 3px;
    padding: 1px 5px;
    font-size: 0.8rem;
  }
  @media (max-width: 600px) {
    .toolbar { gap: 4px; padding: 8px; }
    .toolbar input[type=number] { width: 70px; }
    .btn { padding: 6px 10px; font-size: 0.8rem; }
  }
</style>
</head>
<body>

<header>
  <h1>&#9881; OSPF Tools Viewer</h1>
  <div class="tabs">
    <button class="tab active" data-tab="viz">Topology Viz</button>
    <button class="tab" data-tab="bin">Parse Bin</button>
  </div>
</header>

<div class="toolbar">
  <span id="router-group">
    <label for="router">Router:</label>
    <select id="router">
      <option>r1</option><option>r2</option><option>r3</option><option>r4</option><option>r5</option>
      <option>r6</option><option>r7</option><option>r8</option><option>r9</option><option>r10</option>
    </select>
  </span>
  <label for="seq">Seq #:</label>
  <input type="number" id="seq" value="500" min="0" step="1">
  <button class="btn btn-prev" id="btn-prev" title="Lui 1 su kien">&larr; Prev</button>
  <button class="btn btn-next" id="btn-next" title="Tien 1 su kien">Next &rarr;</button>
  <button class="btn btn-run" id="btn-run">&#9654; Run</button>
  <span class="status" id="status">San sang</span>
</div>

<main>
  <pre id="output"><span class="placeholder">Nhap so sequence va bam <kbd>Run</kbd> hoac <kbd>Enter</kbd> de bat dau.</span></pre>
</main>

<script>
  const $ = s => document.getElementById(s);
  const seqInp     = $('seq');
  const routerSel  = $('router');
  const routerGroup = $('router-group');
  const outPre     = $('output');
  const status  = $('status');
  const btnRun  = $('btn-run');
  const btnPrev = $('btn-prev');
  const btnNext = $('btn-next');
  const tabs    = document.querySelectorAll('.tab');

  let currentTab = 'viz';

  // --- Tab switching ---
  tabs.forEach(t => t.addEventListener('click', () => {
    tabs.forEach(x => x.classList.remove('active'));
    t.classList.add('active');
    currentTab = t.dataset.tab;
    if (currentTab === 'viz') {
      routerGroup.style.display = '';
      seqInp.value = seqInp.value || '500';
    } else {
      routerGroup.style.display = 'none';
      seqInp.value = seqInp.value || '1000';
    }
    seqInp.focus();
  }));

  // --- AbortController de huy request cu khi giu nut ---
  let abortCtrl = null;

  async function load(seq, dir) {
    if (abortCtrl) { abortCtrl.abort(); abortCtrl = null; }
    const ctrl = new AbortController();
    abortCtrl = ctrl;

    seqInp.value = seq;
    const endpoint = currentTab === 'viz' ? 'api/viz' : 'api/bin';
    const d = dir || 'next';
    let url = `/${endpoint}?seq=${encodeURIComponent(seq)}&dir=${d}`;
    if (currentTab === 'viz') {
      url += `&router=${encodeURIComponent(routerSel.value)}`;
    }
    status.textContent = 'Dang tai...';
    status.className = 'status';
    outPre.innerHTML = '';
    try {
      const resp = await fetch(url, {signal: ctrl.signal});
      const text = await resp.text();
      if (ctrl.signal.aborted) return;
      if (!resp.ok) {
        outPre.textContent = `[HTTP ${resp.status}]\n${text}`;
        status.textContent = `Loi HTTP ${resp.status}`;
        status.className = 'status error';
      } else {
        outPre.textContent = text;
        const actualSeq = resp.headers.get('X-Actual-Seq') || seq;
        if (String(actualSeq) !== String(seq)) {
          seqInp.value = actualSeq;
        }
        const meta = resp.headers.get('X-File-Path') || `${seq}`;
        status.textContent = `OK - ${meta} - ${text.length} bytes`;
        status.className = 'status ok';
      }
    } catch (err) {
      if (ctrl.signal.aborted) return;
      outPre.textContent = `[Loi ket noi]\n${err.message || err}`;
      status.textContent = 'Loi ket noi';
      status.className = 'status error';
    } finally {
      if (abortCtrl === ctrl) abortCtrl = null;
    }
  }

  // --- Hold-to-scroll (giu nut de quet nhanh) ---
  let holdTimer = null;
  let holdTick = 0;
  const HOLD_START_MS = 280;
  const HOLD_FAST_MS  = 60;
  const HOLD_ACCEL    = 0.75;

  function startHold(delta, dir) {
    stopHold();
    holdTick = 0;
    function tick() {
      const v = parseInt(seqInp.value, 10);
      if (isNaN(v)) { stopHold(); return; }
      const next = v + delta;
      if (next < 0) { stopHold(); return; }
      load(next, dir);
      holdTick++;
      const delay = Math.max(HOLD_FAST_MS, HOLD_START_MS * Math.pow(HOLD_ACCEL, holdTick));
      holdTimer = setTimeout(tick, delay);
    }
    tick();
  }

  function stopHold() {
    if (holdTimer) { clearTimeout(holdTimer); holdTimer = null; }
    holdTick = 0;
  }

  // --- Buttons ---
  btnRun.addEventListener('click', () => {
    const v = parseInt(seqInp.value, 10);
    if (isNaN(v) || v < 0) { status.textContent = 'So khong hop le'; status.className = 'status error'; return; }
    load(v);
  });

  btnPrev.addEventListener('mousedown', e => { e.preventDefault(); startHold(-1, 'prev'); });
  btnPrev.addEventListener('mouseup', stopHold);
  btnPrev.addEventListener('mouseleave', stopHold);

  btnNext.addEventListener('mousedown', e => { e.preventDefault(); startHold(+1, 'next'); });
  btnNext.addEventListener('mouseup', stopHold);
  btnNext.addEventListener('mouseleave', stopHold);

  btnPrev.addEventListener('touchstart', e => { e.preventDefault(); startHold(-1, 'prev'); });
  btnPrev.addEventListener('touchend', stopHold);
  btnNext.addEventListener('touchstart', e => { e.preventDefault(); startHold(+1, 'next'); });
  btnNext.addEventListener('touchend', stopHold);

  // --- Keyboard shortcuts ---
  seqInp.addEventListener('keydown', e => {
    if (e.key === 'Enter')  { e.preventDefault(); btnRun.click(); }
    if (e.key === 'ArrowLeft')  { e.preventDefault(); startHold(-1, 'prev'); }
    if (e.key === 'ArrowRight') { e.preventDefault(); startHold(+1, 'next'); }
  });
  seqInp.addEventListener('keyup', e => {
    if (e.key === 'ArrowLeft' || e.key === 'ArrowRight') { stopHold(); }
  });

  document.addEventListener('keydown', e => {
    if (e.key === 't' && e.ctrlKey) {
      e.preventDefault();
      seqInp.focus();
      seqInp.select();
    }
  });
</script>
</body>
</html>"""


# ============================================================
# Server — doc file tinh tu resultlog/ va resultbin/
# ============================================================

class Handler(http.server.BaseHTTPRequestHandler):

    def log_message(self, fmt, *args):
        print(f"  [{self.client_address[0]}] {args[0]}", file=sys.stderr)

    def serve_html(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(HTML.encode("utf-8"))

    def serve_text(self, code, text):
        self.send_response(code)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(text.encode("utf-8"))

    def _find_nearest(self, base_dir, router, seq, direction="next"):
        """Tìm seq gần nhất theo hướng.
        Với resultlog: file tên <seq>.txt.
        Với resultbin: file tên <seq6>_<...>.txt, lấy 6 ký tự đầu làm seq."""
        base = os.path.join(PROJECT_ROOT, base_dir, router)
        if not os.path.isdir(base):
            return None
        seqs = set()
        for fpath in _glob.glob(os.path.join(base, "**", "*.txt"), recursive=True):
            name = os.path.splitext(os.path.basename(fpath))[0]
            if name.isdigit():
                seqs.add(int(name))
            elif '_' in name and name.split('_')[0].isdigit():
                # Dạng bin: 000500_10_010000
                seqs.add(int(name.split('_')[0]))
        if not seqs:
            return None
        if direction == "prev":
            candidates = [s for s in seqs if s <= seq]
            nearest = max(candidates) if candidates else max(seqs)
        else:
            candidates = [s for s in seqs if s >= seq]
            nearest = min(candidates) if candidates else min(seqs)
        fpath = os.path.join(PROJECT_ROOT, base_dir, router, f"{nearest}.txt")
        if os.path.exists(fpath):
            return nearest, fpath
        if base_dir == "resultbin":
            pattern = f"{int(nearest):06d}"
            matched = _glob.glob(os.path.join(base, "**", f"{pattern}*.txt"), recursive=True)
            if matched:
                return nearest, matched[0]
        return None

    def _serve_file(self, base_dir, router, seq_str, direction="next"):
        if not seq_str.isdigit():
            self.serve_text(400, "Tham so seq phai la so nguyen.")
            return
        seq = int(seq_str)
        fpath = os.path.join(PROJECT_ROOT, base_dir, router, f"{seq_str}.txt")
        actual_seq = seq_str
        if not os.path.exists(fpath):
            nearest = self._find_nearest(base_dir, router, seq, direction)
            if nearest:
                actual_seq, fpath = str(nearest[0]), nearest[1]
            else:
                self.serve_text(404, f"Khong tim thay {base_dir}/{router}/{seq_str}.txt")
                return
        try:
            with open(fpath, "r", encoding="utf-8") as f:
                text = f.read()
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("X-File-Path", f"{base_dir}/{router}/{actual_seq}.txt")
            self.send_header("X-Actual-Seq", actual_seq)
            self.end_headers()
            self.wfile.write(text.encode("utf-8"))
        except Exception as e:
            self.serve_text(500, f"Loi doc file: {e}")

    def _serve_bin_files(self, seq_str, direction="next"):
        """Liệt kê đường dẫn file trong toàn bộ resultbin/ khớp seq."""
        if not seq_str.isdigit():
            self.serve_text(400, "Tham so seq phai la so nguyen.")
            return
        seq = int(seq_str)
        seq_padded = f"{seq:06d}"
        base = os.path.join(PROJECT_ROOT, "resultbin")
        if not os.path.isdir(base):
            self.serve_text(404, "Khong tim thay thu muc resultbin")
            return
        matched = []
        for fpath in _glob.glob(os.path.join(base, "**", "*.txt"), recursive=True):
            fname = os.path.basename(fpath)
            if fname.startswith(seq_padded):
                matched.append(fpath)
        if not matched:
            # Tìm seq gần nhất trong toàn bộ resultbin
            nearest_seq = None
            all_seqs = set()
            for fpath in _glob.glob(os.path.join(base, "**", "*.txt"), recursive=True):
                name = os.path.splitext(os.path.basename(fpath))[0]
                if name.isdigit():
                    all_seqs.add(int(name))
                elif '_' in name and name.split('_')[0].isdigit():
                    all_seqs.add(int(name.split('_')[0]))
            if all_seqs:
                if direction == "prev":
                    candidates = [s for s in all_seqs if s <= seq]
                    nearest_seq = max(candidates) if candidates else max(all_seqs)
                else:
                    candidates = [s for s in all_seqs if s >= seq]
                    nearest_seq = min(candidates) if candidates else min(all_seqs)
                seq_padded = f"{nearest_seq:06d}"
                for fpath in _glob.glob(os.path.join(base, "**", "*.txt"), recursive=True):
                    fn = os.path.basename(fpath)
                    if fn.startswith(seq_padded):
                        matched.append(fpath)
            if not matched:
                self.serve_text(404, "Khong tim thay file nao trong resultbin")
                return

        lines = [f"=== Bin files for seq={seq_padded} ({len(matched)} files) ==="]
        for fp in sorted(matched):
            rel = os.path.relpath(fp, base)
            lines.append(f"\n--- {rel} ---")
            try:
                with open(fp, "r", encoding="utf-8") as f:
                    content = f.read()
                lines.append(content.rstrip())
            except Exception as e:
                lines.append(f"[Loi doc: {e}]")
        text = "\n".join(lines)

        self.send_response(200)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("X-File-Path", f"resultbin/**/*{seq_padded}* ({len(matched)} files)")
        self.send_header("X-Actual-Seq", seq_padded)
        self.end_headers()
        self.wfile.write(text.encode("utf-8"))

    def handle_api_viz(self, query):
        router = query.get("router", ["r1"])[0]
        direction = query.get("dir", ["next"])[0]
        self._serve_file("resultlog", router, query.get("seq", [""])[0], direction)

    def handle_api_bin(self, query):
        direction = query.get("dir", ["next"])[0]
        self._serve_bin_files(query.get("seq", [""])[0], direction)

    def do_GET(self):
        try:
            self._do_GET()
        except (BrokenPipeError, ConnectionResetError):
            pass

    def _do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path.rstrip("/") or "/"
        query = urllib.parse.parse_qs(parsed.query)

        if path == "/" or path == "/index.html":
            self.serve_html()
        elif path == "/api/viz":
            self.handle_api_viz(query)
        elif path == "/api/bin":
            self.handle_api_bin(query)
        else:
            self.serve_text(404, "404 Not Found")


def main():
    parser = argparse.ArgumentParser(description="OSPF Tools Web Viewer")
    parser.add_argument("--port", type=int, default=8080, help="HTTP port (default: 8080)")
    args = parser.parse_args()

    os.chdir(PROJECT_ROOT)

    viz_dir = os.path.join(PROJECT_ROOT, "resultlog")
    bin_dir = os.path.join(PROJECT_ROOT, "resultbin")
    viz_count = len(os.listdir(viz_dir)) if os.path.isdir(viz_dir) else 0
    bin_count = len(os.listdir(bin_dir)) if os.path.isdir(bin_dir) else 0

    class ReusableServer(http.server.HTTPServer):
        allow_reuse_address = True

    server = ReusableServer(("0.0.0.0", args.port), Handler)
    print(f"\n  OSPF Tools Viewer")
    print(f"  Project root : {PROJECT_ROOT}")
    print(f"  URL          : http://localhost:{args.port}")
    print(f"  resultlog/   : {viz_count} files" if viz_count else
          "  resultlog/   : CHUA CO - chay: python3 tools/viz_topology_ascii.py --all")
    print(f"  resultbin/   : {bin_count} files" if bin_count else
          "  resultbin/   : CHUA CO - chay: python3 tools/parse_bin.py --all")
    print(f"\n  Nhan Ctrl+C de dung.\n")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n  Da dung server.")
        server.server_close()


if __name__ == "__main__":
    main()
