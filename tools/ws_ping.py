#!/usr/bin/env python3
# ABOUTME: WebSocket round-trip probe for a wotm_server endpoint: a ping frame every --every ms, one line per pong (unix time, rtt ms), so
# ABOUTME: series taken at the same time from several places (this PC, the server's VM, another network) can be lined up. Stdlib only.
"""Usage: python3 tools/ws_ping.py <url> [--secs 60] [--every 100] [--connect-to <address> [--insecure]]

  url           ws:// or wss:// endpoint, e.g. wss://playwotm.com/lobby, or ws://127.0.0.1:7780/lobby on the server's VM
  --secs        how long to measure (default 60)
  --every       ms between pings (default 100: the server pings each browser this often)
  --connect-to  connect to this address instead of the URL's host; the TLS name and Host stay the URL's (e.g. the origin's IP, to
                leave Cloudflare out)
  --insecure    with --connect-to an origin whose certificate is not for its IP: skip the certificate check (pings carry nothing)

Prints `<unix s> <rtt ms>` per pong on stdout and `# sent N lost M` on stderr (a ping with no pong within 2 s is lost). The server answers
ping frames itself (native/net/wotm_net_wslink.hpp), so the round trip is the path plus the server's loop, as the server's own pings of a
browser measure it. Exit 1 with `error:` when the connection or the handshake fails. A series is investigated with any tool; the
investigation that uses it is docs/investigacoes/2026-11-25/ping-spikes.md.
"""
import argparse
import base64
import os
import select
import socket
import ssl
import struct
import sys
import time
import urllib.parse

LOST_AFTER_S = 2.0


def client_frame(opcode, payload):
    """One final frame from a client: masked, as RFC 6455 requires (payload under 126 bytes)."""
    mask = os.urandom(4)
    return bytes([0x80 | opcode, 0x80 | len(payload)]) + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(payload))


def server_frames(buf):
    """The complete (opcode, payload) frames at the start of `buf` (a server's frames are unmasked), and the bytes left over."""
    frames = []
    while len(buf) >= 2:
        n, off = buf[1] & 0x7F, 2
        if n == 126:
            if len(buf) < 4:
                break
            n, off = struct.unpack('>H', buf[2:4])[0], 4
        elif n == 127:
            if len(buf) < 10:
                break
            n, off = struct.unpack('>Q', buf[2:10])[0], 10
        if len(buf) < off + n:
            break
        frames.append((buf[0] & 0x0F, buf[off:off + n]))
        buf = buf[off + n:]
    return frames, buf


def connect(url, connect_to=None, insecure=False):
    u = urllib.parse.urlparse(url)
    if u.scheme not in ('ws', 'wss') or not u.hostname:
        raise SystemExit(f'error: {url}: not a ws:// or wss:// URL')
    port = u.port or (443 if u.scheme == 'wss' else 80)
    try:
        s = socket.create_connection((connect_to or u.hostname, port), timeout=10)
        s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        if u.scheme == 'wss':
            ctx = ssl.create_default_context()
            if insecure:
                ctx.check_hostname = False
                ctx.verify_mode = ssl.CERT_NONE
            s = ctx.wrap_socket(s, server_hostname=u.hostname)
        key = base64.b64encode(os.urandom(16)).decode()
        s.sendall((f'GET {u.path or "/"} HTTP/1.1\r\nHost: {u.hostname}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n'
                   f'Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\nOrigin: https://{u.hostname}\r\n'
                   'User-Agent: ws_ping\r\n\r\n').encode())
        head = b''
        while b'\r\n\r\n' not in head:
            chunk = s.recv(4096)
            if not chunk:
                break
            head += chunk
    except OSError as e:
        raise SystemExit(f'error: {url}: {e}')
    status = head.split(b'\r\n', 1)[0].decode(errors='replace')
    if ' 101 ' not in status + ' ':
        raise SystemExit(f'error: {url}: handshake refused ({status or "connection closed"})')
    return s, head.split(b'\r\n\r\n', 1)[1]


def send_all(s, data):
    while data:
        try:
            data = data[s.send(data):]
        except (ssl.SSLWantWriteError, BlockingIOError):
            select.select([], [s], [], 0.05)


def run(url, secs, every_ms, connect_to=None, insecure=False, out=sys.stdout):
    s, buf = connect(url, connect_to, insecure)
    s.setblocking(False)
    sent, seq, lost = {}, 0, 0
    end = time.time() + secs
    next_ping = time.perf_counter()
    while time.time() < end:
        if time.perf_counter() >= next_ping:
            sent[seq] = time.perf_counter()
            send_all(s, client_frame(0x9, struct.pack('>Q', seq)))
            seq += 1
            next_ping += every_ms / 1000
        pending = isinstance(s, ssl.SSLSocket) and s.pending()
        if pending or select.select([s], [], [], max(0.0, min(next_ping - time.perf_counter(), 0.01)))[0]:
            try:
                chunk = s.recv(65536)
                if not chunk:
                    raise SystemExit(f'error: {url}: closed by the server')
                buf += chunk
            except (ssl.SSLWantReadError, BlockingIOError):
                pass
        frames, buf = server_frames(buf)
        for op, payload in frames:
            if op == 0xA and len(payload) == 8:
                t0 = sent.pop(struct.unpack('>Q', payload)[0], None)
                if t0 is not None:
                    print(f'{time.time():.3f} {(time.perf_counter() - t0) * 1000:.1f}', file=out, flush=True)
            elif op == 0x8:
                raise SystemExit(f'error: {url}: closed by the server')
        for k in [k for k, t in sent.items() if time.perf_counter() - t > LOST_AFTER_S]:
            del sent[k]
            lost += 1
    print(f'# sent {seq} lost {lost + len(sent)}', file=sys.stderr)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0], formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=__doc__.split('\n\n', 2)[2])
    ap.add_argument('url')
    ap.add_argument('--secs', type=float, default=60)
    ap.add_argument('--every', type=float, default=100)
    ap.add_argument('--connect-to')
    ap.add_argument('--insecure', action='store_true')
    a = ap.parse_args()
    run(a.url, a.secs, a.every, a.connect_to, a.insecure)


if __name__ == '__main__':
    main()
