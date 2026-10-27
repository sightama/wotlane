*English translation of [docs/passos/VIEWER_WEB.md](../../passos/VIEWER_WEB.md). The original is authoritative; this copy may lag behind it.*

# 3D model viewer in the browser (idea, evaluated on 2026-10-14)

(Docs index in [CLAUDE.md](../CLAUDE.md).) Only the feasibility evaluation; nothing has been implemented yet.

## What already exists
- **Format solved** ([FORMATS.md](../FORMATS.md) (`docs/FORMATOS.md`), [NATIVE.md](NATIVE.md) (`NATIVO.md`)): `.NGP` (zip `IE\3\4` in the level, raw in the monster; pre-linked image at
  `0xA00000`; hierarchy GROUP/LOD/SWITCH/ROTATE/TRANSLATE/SCALE/CHAR_INSTANCE/DESTRUCTIBLE/CONTROL), `polyPkt` mesh (positions,
  normals, strips with ADC, per-vertex color, UV 4.12, multitexture per section, batches of 190 vertices), CT32/24/16, T8, T4,
  T8H/4HL/4HH textures with palettes from the `.RTX` (CSM1 swap), monster skinning (bone code in the low bits of xyz, matrix palette,
  3 weights) and animation curves + mixer.
- **Code:** `tools/ngp.py` reads meshes straight from the disc (Python; still uses the old ADC model); `native/viewer.cpp` (raylib) is a
  viewer with a free camera, but it starts from a **RAM dump** (textures via `texInfo` and bone matrices computed by the game).

## What is missing to read from the disc alone
1. **Textures without a dump:** read the pixel packets of the `.TEX` and apply the `.PTR` (`TBP0`, texture ids), instead of `texInfo` in RAM. Moderate.
2. **Monster pose:** today the matrices come from RAM. Options: evaluate the curves offline and build the matrices of the `ANIM_XFORM`/`SKEL_BONE`
   (the format is known; building them outside the game is not), which as a bonus becomes an animation player; or accept a dump as a shortcut.
3. Levels are almost all rigid: `ngp.py` already extracts them (fix the ADC: it skips only the triangle, the strip continues).

## Browser
- Rule 1: **no game file on the server.** A static page that reads the user's `disc/` folder (or the ISO) through the File API;
  everything processed locally. It can be hosted (e.g. GitHub Pages) without containing anything from the game.
- **Recommended path:** JS/TS + three.js, porting `ngp.py` + `wotm_gs.hpp`. Alternative: Emscripten of `wotm_native.hpp`, but it
  depends on the EE RAM and would have to be adapted in the same way.

## Estimate and suggested order
| Stage | Difficulty |
|---|---|
| Levels/rigid objects with textures | easy to moderate (porting) |
| Monsters in a fixed pose from a dump | easy (the native layer already does it) |
| Monsters from the disc with animation | moderate to hard (skeleton + offline curves) |
| Export glTF/OBJ (useful for mods) | little extra after the reader |

Order: web viewer for levels/objects first; then monsters with animation.
