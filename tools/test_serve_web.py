#!/usr/bin/env python3
# ABOUTME: Tests for tools/serve_web.py (the local server for the browser version): cross-origin isolation headers on every answer,
# ABOUTME: byte ranges for the streamed disc, and the data/ folder mapped apart from the page. Run: python tools/test_serve_web.py
import http.client
import sys
import tempfile
import threading
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import serve_web  # noqa: E402


class ServeWebTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        root = Path(cls.tmp.name)
        (root / 'site').mkdir()
        (root / 'data').mkdir()
        (root / 'site' / 'index.html').write_text('<html></html>')
        (root / 'site' / 'wotm.wasm').write_bytes(b'\0asm')
        cls.blob = bytes(range(256)) * 64   # 16 KiB
        (root / 'data' / 'game.iso').write_bytes(cls.blob)
        (root / 'elsewhere.bin').write_bytes(b'linked')
        (root / 'data' / 'linked.bin').symlink_to(root / 'elsewhere.bin')
        cls.server = serve_web.make_server('127.0.0.1', 0, root / 'site', root / 'data')
        cls.port = cls.server.server_address[1]
        threading.Thread(target=cls.server.serve_forever, daemon=True).start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.tmp.cleanup()

    def get(self, path, method='GET', headers=None):
        c = http.client.HTTPConnection('127.0.0.1', self.port, timeout=5)
        c.request(method, path, headers=headers or {})
        r = c.getresponse()
        body = r.read()
        c.close()
        return r, body

    def test_isolation_headers_on_page(self):
        r, _ = self.get('/')
        self.assertEqual(r.status, 200)
        self.assertEqual(r.getheader('Cross-Origin-Opener-Policy'), 'same-origin')
        self.assertEqual(r.getheader('Cross-Origin-Embedder-Policy'), 'require-corp')

    def test_wasm_mime_type(self):
        r, _ = self.get('/wotm.wasm')
        self.assertEqual(r.getheader('Content-Type'), 'application/wasm')

    def test_data_whole_file_advertises_ranges(self):
        r, body = self.get('/data/game.iso')
        self.assertEqual(r.status, 200)
        self.assertEqual(r.getheader('Accept-Ranges'), 'bytes')
        self.assertEqual(body, self.blob)

    def test_head_reports_length(self):
        r, body = self.get('/data/game.iso', method='HEAD', headers={'Range': 'bytes=0-'})
        self.assertEqual(int(r.getheader('Content-Length')), len(self.blob))
        self.assertEqual(r.getheader('Accept-Ranges'), 'bytes')
        self.assertEqual(body, b'')

    def test_range_returns_206_and_exact_bytes(self):
        r, body = self.get('/data/game.iso', headers={'Range': 'bytes=1000-1999'})
        self.assertEqual(r.status, 206)
        self.assertEqual(r.getheader('Content-Range'), f'bytes 1000-1999/{len(self.blob)}')
        self.assertEqual(body, self.blob[1000:2000])

    def test_range_past_end_is_clamped(self):
        r, body = self.get('/data/game.iso', headers={'Range': f'bytes=16000-99999'})
        self.assertEqual(r.status, 206)
        self.assertEqual(body, self.blob[16000:])

    def test_unsatisfiable_range_is_416(self):
        r, _ = self.get('/data/game.iso', headers={'Range': 'bytes=999999-'})
        self.assertEqual(r.status, 416)

    def test_double_slash_from_the_fetch_backend(self):
        r, body = self.get('/data//game.iso', headers={'Range': 'bytes=0-15'})
        self.assertEqual(r.status, 206)
        self.assertEqual(body, self.blob[:16])

    def test_symlinked_data_file_is_served(self):
        r, body = self.get('/data//linked.bin')
        self.assertEqual(r.status, 200)
        self.assertEqual(body, b'linked')

    def test_no_escape_from_data(self):
        r, _ = self.get('/data/../site/index.html')
        self.assertIn(r.status, (403, 404))


if __name__ == '__main__':
    unittest.main()
