// ABOUTME: Builds the test creations as one zip the front panel imports: a map from the disc's CENTRAL stage (tools/stage_map.py make)
// ABOUTME: and a skin of Togera (skin.json alone, as the character creator writes one), under Creator/stages and Creator/skins.
import { spawnSync } from 'node:child_process';
import { mkdirSync, readFileSync, writeFileSync, rmSync } from 'node:fs';
import { join } from 'node:path';
import { REPO } from './infra.mjs';
import { zip } from '../../../web/zip.js';

const MAP_ID = 't1';

// {zip: path of the zip, mapId}; `disc` is the extracted disc folder (the map's base hash comes from it)
export async function makeCreations(dir, disc) {
  const work = join(dir, 'creations_src');
  rmSync(work, { recursive: true, force: true });
  const stages = join(work, 'Creator', 'stages');
  mkdirSync(stages, { recursive: true });
  const made = spawnSync('python3', [join(REPO, 'tools/stage_map.py'), 'make', '--base', 'CENTRAL', '--id', MAP_ID, '--name', 'TEST MAP',
    '--out', stages, '--disc', disc, '--remove', '0x18a410', '--move', '0x165a70=-185.7,475.8,0', '--turn', '0x4da6b0=90'], { encoding: 'utf8' });
  if (made.status !== 0) throw new Error(`stage_map.py make failed: ${made.stderr || made.stdout}`);
  const skin = '{"monster": "togera", "id": "green", "name": "GREEN", "base": 1, "ai": false}\n';
  const entries = [
    { name: `Creator/stages/${MAP_ID}/stage.json`, data: readFileSync(join(stages, MAP_ID, 'stage.json')) },
    { name: 'Creator/skins/green/skin.json', data: Buffer.from(skin) },
  ];
  const path = join(dir, 'creations.zip');
  writeFileSync(path, await zip(entries.map((e) => ({ name: e.name, data: new Uint8Array(e.data) }))));
  return { zip: path, mapId: MAP_ID };
}
