#!/usr/bin/env python3
# ABOUTME: Local server for the browser version (tools/build_web.sh): serves the page from <runtime>/web_dist and the disc under /data/
# ABOUTME: with byte ranges, and sends the cross-origin isolation headers the game's threads need. Binds to 127.0.0.1 only.
"""Usage:
  python tools/serve_web.py [--port 8642] [--site <dir>] [--data <dir>]
      --site  the built page (default: <runtime>/web_dist)
      --data  a folder with game.iso and SCUS_971.97 (default: links to your ISO, from <runtime>/.wotm_iso, and to
              <runtime>/discroot/SCUS_971.97, made in <runtime>/web_data)
  Then open http://127.0.0.1:<port>/ in Chrome.
"""
import argparse
import functools
import http.server
import os
import re
import sys
from pathlib import Path

RANGE = re.compile(r'^bytes=(\d*)-(\d*)$')
TYPES = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.json': 'application/json'}


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, site, data, **kwargs):
        self.data_dir = Path(data).resolve()
        super().__init__(*args, directory=str(site), **kwargs)

    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()

    def guess_type(self, path):
        return TYPES.get(os.path.splitext(str(path))[1], None) or super().guess_type(path)

    def log_message(self, fmt, *args):
        pass

    def do_GET(self):
        if self.path.startswith('/data/'):
            self.send_data(head=False)
        else:
            super().do_GET()

    def do_HEAD(self):
        if self.path.startswith('/data/'):
            self.send_data(head=True)
        else:
            super().do_HEAD()

    def data_file(self):
        name = re.sub(r'/+', '/', self.path.split('?', 1)[0][len('/data/'):]).strip('/')
        # the name must stay inside data/; the files there may be links to the disc elsewhere (default_data)
        if not name or any(part in ('', '.', '..') for part in name.split('/')):
            return None
        target = self.data_dir / name
        return target if target.is_file() else None

    def send_data(self, head):
        target = self.data_file()
        if target is None:
            self.send_error(404)
            return
        size = target.stat().st_size
        start, end, status = 0, size - 1, 200
        m = RANGE.match(self.headers.get('Range', ''))
        if m and (m.group(1) or m.group(2)):
            if m.group(1):
                start = int(m.group(1))
                end = min(int(m.group(2)), size - 1) if m.group(2) else size - 1
            else:
                start = max(0, size - int(m.group(2)))
            if start >= size or start > end:
                self.send_response(416)
                self.send_header('Content-Range', f'bytes */{size}')
                self.send_header('Content-Length', '0')
                self.end_headers()
                return
            status = 206
        # a HEAD with "Range: bytes=0-" (the fetch backend's size probe) answers with the whole length
        if head:
            status = 200
            start, end = 0, size - 1
        self.send_response(status)
        self.send_header('Accept-Ranges', 'bytes')
        self.send_header('Content-Type', 'application/octet-stream')
        self.send_header('Content-Length', str(end - start + 1))
        if status == 206:
            self.send_header('Content-Range', f'bytes {start}-{end}/{size}')
        self.end_headers()
        if head:
            return
        with open(target, 'rb') as f:
            f.seek(start)
            left = end - start + 1
            while left > 0:
                chunk = f.read(min(left, 1 << 20))
                if not chunk:
                    break
                self.wfile.write(chunk)
                left -= len(chunk)


def make_server(host, port, site, data):
    return http.server.ThreadingHTTPServer((host, port), functools.partial(Handler, site=site, data=data))


def runtime_dir(repo):
    if os.environ.get('WOTM_RECOMP_DIR'):
        return Path(os.environ['WOTM_RECOMP_DIR'])
    marker = repo / '.wotm_recomp_dir'
    return Path(marker.read_text().strip()) if marker.exists() else repo / 'runtime'


def default_data(rt):
    """<runtime>/web_data with links to the ISO setup_runtime.sh used and to the ELF in discroot (nothing is copied)."""
    data = rt / 'web_data'
    data.mkdir(exist_ok=True)
    links = {'game.iso': Path((rt / '.wotm_iso').read_text().strip()), 'SCUS_971.97': rt / 'discroot' / 'SCUS_971.97'}
    for name, src in links.items():
        if not src.is_file():
            sys.exit(f'error: {src} not found (run sh tools/setup_runtime.sh)')
        link = data / name
        if link.is_symlink() or link.exists():
            link.unlink()
        link.symlink_to(src)
    return data


def main():
    repo = Path(__file__).resolve().parent.parent
    rt = runtime_dir(repo)
    ap = argparse.ArgumentParser(description='Local server for the browser version.')
    ap.add_argument('--port', type=int, default=8642)
    ap.add_argument('--site', type=Path, default=rt / 'web_dist')
    ap.add_argument('--data', type=Path)
    a = ap.parse_args()
    if not (a.site / 'index.html').is_file():
        sys.exit(f'error: no page in {a.site} (run sh tools/build_web.sh)')
    data = a.data or default_data(rt)
    server = make_server('127.0.0.1', a.port, a.site, data)
    print(f'serving {a.site} and {data} at http://127.0.0.1:{a.port}/')
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()
