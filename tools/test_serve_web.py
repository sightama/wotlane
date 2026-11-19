#!/usr/bin/env python3
# ABOUTME: Tests for tools/serve_web.py (the local server for the browser version): cross-origin isolation headers on every answer,
# ABOUTME: byte ranges for the streamed disc, the disc as numbered chunk files, and the data/ folder mapped apart from the page.
# ABOUTME: Run: python tools/test_serve_web.py
import hashlib
import http.client
import json
import sys
import tempfile
import threading
import unittest
import unittest.mock
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

    def test_data_has_an_etag_that_names_the_file_version(self):
        # web/disc_cache.js keys its cached chunks by it, as it does with the site's Caddy
        r, _ = self.get('/data/game.iso', method='HEAD')
        etag = r.getheader('ETag')
        self.assertTrue(etag and etag.startswith('"'))
        r2, _ = self.get('/data/game.iso', headers={'Range': 'bytes=0-9'})
        self.assertEqual(r2.getheader('ETag'), etag)

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


class ServeWebChunksTest(unittest.TestCase):
    """The disc as the site serves it: /data/index.json, /data/<digest>/manifest.json and /data/<digest>/<n>.bin (web/disc_chunks.js)."""
    CHUNK = 4096

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        root = Path(cls.tmp.name)
        (root / 'site').mkdir()
        (root / 'data').mkdir()
        (root / 'site' / 'index.html').write_text('<html></html>')
        cls.blob = bytes((i * 7 + i // 256) % 256 for i in range(10000))   # chunks of 4096, 4096 and 1808
        (root / 'data' / 'game.iso').write_bytes(cls.blob)
        (root / 'elsewhere.bin').write_bytes(b'linked')
        (root / 'data' / 'SCUS_971.97').symlink_to(root / 'elsewhere.bin')
        cls.root = root
        cls.servers = {}

    @classmethod
    def tearDownClass(cls):
        for server in cls.servers.values():
            server.shutdown()
            server.server_close()
        cls.tmp.cleanup()

    def start(self, chunks):
        if chunks not in self.servers:
            server = serve_web.make_server('127.0.0.1', 0, self.root / 'site', self.root / 'data', chunks=chunks, chunk_bytes=self.CHUNK)
            threading.Thread(target=server.serve_forever, daemon=True).start()
            self.servers[chunks] = server
        return self.servers[chunks].server_address[1]

    def get(self, path, chunks='both', method='GET', headers=None):
        c = http.client.HTTPConnection('127.0.0.1', self.start(chunks), timeout=5)
        c.request(method, path, headers=headers or {})
        r = c.getresponse()
        body = r.read()
        c.close()
        return r, body

    def index(self):
        r, body = self.get('/data/index.json')
        self.assertEqual(r.status, 200)
        return json.loads(body)

    def test_index_lists_each_file_with_its_digest_and_size(self):
        index = self.index()
        self.assertEqual(index['chunkBytes'], self.CHUNK)
        self.assertEqual(sorted(index['files']), ['SCUS_971.97', 'game.iso'])
        self.assertEqual(index['files']['game.iso'], {'digest': hashlib.sha256(self.blob).hexdigest()[:16], 'size': len(self.blob)})
        self.assertEqual(index['files']['SCUS_971.97']['size'], 6)   # a link to the disc elsewhere is followed

    def test_manifest_has_a_hash_for_every_chunk(self):
        entry = self.index()['files']['game.iso']
        r, body = self.get(f"/data/{entry['digest']}/manifest.json")
        self.assertEqual(r.status, 200)
        manifest = json.loads(body)
        self.assertEqual({k: manifest[k] for k in ('name', 'size', 'chunkBytes', 'chunks')}, {'name': 'game.iso', 'size': len(self.blob), 'chunkBytes': self.CHUNK, 'chunks': 3})
        self.assertEqual(manifest['sha256'], [hashlib.sha256(self.blob[i:i + self.CHUNK]).hexdigest() for i in range(0, len(self.blob), self.CHUNK)])

    def test_chunks_are_plain_200s_that_join_into_the_file(self):
        digest = self.index()['files']['game.iso']['digest']
        parts = []
        for n in range(3):
            r, body = self.get(f'/data/{digest}/{n}.bin')
            self.assertEqual(r.status, 200)
            self.assertEqual(int(r.getheader('Content-Length')), len(body))
            parts.append(body)
        self.assertEqual([len(p) for p in parts], [4096, 4096, 1808])
        self.assertEqual(b''.join(parts), self.blob)

    def test_a_chunk_that_does_not_exist_is_404(self):
        digest = self.index()['files']['game.iso']['digest']
        self.assertEqual(self.get(f'/data/{digest}/3.bin')[0].status, 404)
        self.assertEqual(self.get('/data/0123456789abcdef/0.bin')[0].status, 404)
        self.assertEqual(self.get(f'/data/{digest}/manifest.json/x')[0].status, 404)

    def test_without_chunks_there_is_no_index_and_the_whole_file_is_served(self):
        self.assertEqual(self.get('/data/index.json', chunks='off')[0].status, 404)
        r, body = self.get('/data/game.iso', chunks='off')
        self.assertEqual((r.status, body), (200, self.blob))

    def test_chunks_only_stops_serving_the_whole_file(self):
        self.assertEqual(self.get('/data/index.json', chunks='only')[0].status, 200)
        self.assertEqual(self.get('/data/game.iso', chunks='only')[0].status, 404)
        self.assertEqual(self.get('/data/game.iso', chunks='only', method='HEAD')[0].status, 404)


class ServeWebStartTest(unittest.TestCase):
    def test_starts_without_a_reverse_name_lookup(self):
        # http.server names itself with socket.getfqdn, which waited 35 s on a Mac with a slow resolver (the harness gives up sooner)
        def no_lookup(*_):
            raise AssertionError('make_server looked up a host name')
        with tempfile.TemporaryDirectory() as tmp, unittest.mock.patch('socket.getfqdn', no_lookup):
            (Path(tmp) / 'index.html').write_text('<html></html>')
            server = serve_web.make_server('127.0.0.1', 0, Path(tmp), Path(tmp))
            server.server_close()


if __name__ == '__main__':
    unittest.main()
