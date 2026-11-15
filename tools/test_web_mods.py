#!/usr/bin/env python3
# ABOUTME: Tests for tools/web_mods.py: the repo's enabled mods copied into the browser dist with a manifest (file list and a hash
# ABOUTME: that changes with any file), skipping disabled mods like tools/sync_mods.sh does. Run: python3 tools/test_web_mods.py
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import web_mods  # noqa: E402


class WebModsTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.repo = self.root / 'mods'
        (self.repo / 'Goliath Prime' / 'monsters' / 'goliath').mkdir(parents=True)
        (self.repo / 'Goliath Prime' / 'monsters' / 'goliath' / 'monster.json').write_text('{"g":1}')
        (self.repo / 'Cerebulon').mkdir()
        (self.repo / 'Cerebulon' / 'a.json').write_text('{}')
        (self.repo / '_work').mkdir()
        (self.repo / '_work' / 'x.json').write_text('{}')
        (self.repo / '.hidden').mkdir()
        (self.repo / 'README.md').write_text('not a mod')
        self.out = self.root / 'dist' / 'mods'

    def tearDown(self):
        self.tmp.cleanup()

    def test_copies_enabled_mods_and_lists_them_in_name_order(self):
        manifest = web_mods.build(self.repo, self.out)
        self.assertEqual([f['path'] for f in manifest['files']],
                         ['Cerebulon/a.json', 'Goliath Prime/monsters/goliath/monster.json'])
        self.assertEqual((self.out / 'Goliath Prime' / 'monsters' / 'goliath' / 'monster.json').read_text(), '{"g":1}')
        self.assertFalse((self.out / '_work').exists())
        self.assertFalse((self.out / 'README.md').exists())
        self.assertEqual(json.loads((self.out / 'manifest.json').read_text()), manifest)

    def test_hash_changes_with_any_file(self):
        first = web_mods.build(self.repo, self.out)['hash']
        self.assertEqual(web_mods.build(self.repo, self.out)['hash'], first)
        (self.repo / 'Cerebulon' / 'a.json').write_text('{"changed":1}')
        self.assertNotEqual(web_mods.build(self.repo, self.out)['hash'], first)

    def test_no_mods_folder_gives_an_empty_manifest(self):
        manifest = web_mods.build(self.root / 'none', self.out)
        self.assertEqual(manifest['files'], [])


if __name__ == '__main__':
    unittest.main()
