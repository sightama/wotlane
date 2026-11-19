#!/usr/bin/env python3
# ABOUTME: Local server for the browser version (tools/build_web.sh): serves the page from <runtime>/web_dist and the disc under /data/
# ABOUTME: as numbered chunk files and as whole files with byte ranges, with the cross-origin isolation headers. Binds to 127.0.0.1 only.
"""Usage:
  python tools/serve_web.py [--port 8642] [--site <dir>] [--data <dir>]
      --site  the built page (default: <runtime>/web_dist)
      --data  a folder with game.iso and SCUS_971.97 (default: links to your ISO, from <runtime>/.wotm_iso, and to
              <runtime>/discroot/SCUS_971.97, made in <runtime>/web_data)
      --chunks  both (default): the disc as chunk files (/data/index.json, /data/<digest>/<n>.bin, 1 MiB each, what the site serves)
                and as whole files; only: no whole files, as the site once the page has switched; off: whole files only, as a
                site that is not split yet. The index and hashes of a file are computed from the file on first use (a few seconds
                for the ISO).
  Then open http://127.0.0.1:<port>/ in Chrome.
"""
import argparse
import functools
import hashlib
import http.server
import json
import os
import re
import socketserver
import sys
import threading
from pathlib import Path

RANGE = re.compile(r'^bytes=(\d*)-(\d*)$')
CHUNK_PATH = re.compile(r'^(?P<digest>[0-9a-f]{16})/(?:manifest\.json|(?P<n>\d+)\.bin)$')
CHUNK_BYTES = 1 << 20
LAYOUT_LOCK = threading.Lock()   # the first requests come together: one pass over the disc, not one each
TYPES = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.json': 'application/json'}


@functools.lru_cache(maxsize=None)
def chunk_layout(path, mtime_ns, size, chunk_bytes):
    """The digest (first 16 hex of the file's SHA-256) and the SHA-256 of each chunk of a file, in one pass over it."""
    whole = hashlib.sha256()
    chunks = []
    with open(path, 'rb') as f:
        while True:
            chunk = f.read(chunk_bytes)
            if not chunk:
                break
            whole.update(chunk)
            chunks.append(hashlib.sha256(chunk).hexdigest())
    return whole.hexdigest()[:16], chunks


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, site, data, chunks='both', chunk_bytes=CHUNK_BYTES, **kwargs):
        self.data_dir = Path(data).resolve()
        self.chunks = chunks
        self.chunk_bytes = chunk_bytes
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

    def data_name(self):
        name = re.sub(r'/+', '/', self.path.split('?', 1)[0][len('/data/'):]).strip('/')
        # the name must stay inside data/
        return None if not name or any(part in ('', '.', '..') for part in name.split('/')) else name

    def data_file(self):
        name = self.data_name()
        # the files there may be links to the disc elsewhere (default_data)
        target = self.data_dir / name if name else None
        return target if target and target.is_file() else None

    def chunked_files(self):
        """name -> (path, digest, chunk hashes) of each file in data/ (the chunks of the whole-file URL's version)."""
        files = {}
        with LAYOUT_LOCK:
            for target in sorted(self.data_dir.iterdir()):
                if target.is_file():
                    st = target.stat()
                    digest, hashes = chunk_layout(str(target), st.st_mtime_ns, st.st_size, self.chunk_bytes)
                    files[target.name] = (target, st.st_size, digest, hashes)
        return files

    def chunk_answer(self, name):
        """(content type, body) of /data/<name> in the chunk layout (web/disc_chunks.js), or None."""
        files = self.chunked_files()
        if name == 'index.json':
            index = {'chunkBytes': self.chunk_bytes, 'files': {n: {'digest': d, 'size': size} for n, (_, size, d, _) in files.items()}}
            return 'application/json', json.dumps(index).encode()
        m = CHUNK_PATH.match(name)
        found = m and next(((n, f) for n, f in files.items() if f[2] == m.group('digest')), None)
        if not found:
            return None
        file_name, (target, size, digest, hashes) = found
        if m.group('n') is None:
            manifest = {'name': file_name, 'size': size, 'chunkBytes': self.chunk_bytes, 'chunks': len(hashes), 'sha256': hashes}
            return 'application/json', json.dumps(manifest).encode()
        n = int(m.group('n'))
        if n >= len(hashes):
            return None
        with open(target, 'rb') as f:
            f.seek(n * self.chunk_bytes)
            return 'application/octet-stream', f.read(self.chunk_bytes)

    def send_chunk(self, head):
        answer = self.chunk_answer(self.data_name() or '')
        if answer is None:
            self.send_error(404)
            return
        content_type, body = answer
        self.send_response(200)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        if not head:
            self.wfile.write(body)

    def send_data(self, head):
        name = self.data_name() or ''
        if self.chunks != 'off' and (name == 'index.json' or CHUNK_PATH.match(name)):
            return self.send_chunk(head)
        target = self.data_file() if self.chunks != 'only' else None
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
        st = target.stat()   # the file's version, as the site's Caddy names it (web/disc_cache.js keys by it)
        self.send_header('ETag', f'"{st.st_mtime_ns:x}-{st.st_size:x}"')
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


class Server(http.server.ThreadingHTTPServer):
    def server_bind(self):
        # binds without http.server's socket.getfqdn: a reverse lookup of the address that waited 35 s on a slow resolver
        socketserver.TCPServer.server_bind(self)
        self.server_name, self.server_port = self.server_address[:2]


def make_server(host, port, site, data, chunks='both', chunk_bytes=CHUNK_BYTES):
    return Server((host, port), functools.partial(Handler, site=site, data=data, chunks=chunks, chunk_bytes=chunk_bytes))


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
    ap.add_argument('--chunks', choices=('both', 'only', 'off'), default='both')
    a = ap.parse_args()
    if not (a.site / 'index.html').is_file():
        sys.exit(f'error: no page in {a.site} (run sh tools/build_web.sh)')
    data = a.data or default_data(rt)
    server = make_server('127.0.0.1', a.port, a.site, data, chunks=a.chunks)
    print(f'serving {a.site} and {data} at http://127.0.0.1:{a.port}/')
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()
