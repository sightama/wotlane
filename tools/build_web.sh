#!/bin/sh
# ABOUTME: Builds the browser version of the game (WebAssembly, Emscripten) into <runtime>/build_web and assembles the page that serves it
# ABOUTME: in <runtime>/web_dist: web/, wotm.js, wotm.wasm and version.json. No game file goes there; the disc is served apart (data/).
set -eu
. "$(dirname "$0")/posix_env.sh"

usage() {
    cat <<EOF
usage: sh tools/build_web.sh
  Needs a runtime set up by sh tools/setup_runtime.sh (the translated game in recomp/retail) and Emscripten (EMSDK, or ~/emsdk with
  ./emsdk install latest && ./emsdk activate latest). The first configure takes a minute; the first build about two.
  Serve <runtime>/web_dist with cross-origin isolation (COOP same-origin, COEP require-corp) and the disc under data/: see
  docs/passos/WEB.md.
EOF
}
case ${1:-} in -h|--help) usage; exit 0 ;; esac

emsdk=${EMSDK:-$HOME/emsdk}
use_emsdk "$emsdk" || fail "no Emscripten at $emsdk (set EMSDK)"
need emcmake cmake ninja python3

rt=$(runtime_dir)
bdir=$rt/build_web
retail=$REPO/recomp/retail
[ -f "$retail/register_functions.cpp" ] || fail "no translated game in $retail (run sh tools/setup_runtime.sh)"

if [ ! -f "$bdir/build.ninja" ]; then
    step 'configure build_web'
    # the same -D flags as build_dev (tools/setup_runtime.sh), with raylib's web platform on WebGL2 and without FFmpeg (no movies)
    run_logged "$rt/logs/configure_web.log" emcmake cmake -S "$rt/PS2Recomp" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DWOTM_NATIVE_DIR="$REPO/native" -DPS2X_FAST_LINK=ON -DPS2X_ENABLE_AGRESSIVE_LOGS=OFF -DPS2X_ENABLE_IOP_RPC_TRACE=OFF \
        -DPS2X_BUILD_RECOMP=OFF -DPS2X_BUILD_ANALYZER=OFF -DPS2X_BUILD_TEST=OFF -DPS2X_BUILD_STUDIO=OFF -DPS2X_ENABLE_DEBUG_UI=OFF \
        -DPS2X_ENABLE_SCCACHE=OFF -DPS2X_ENABLE_FFMPEG=OFF -DPS2X_RUNNER_DIR="$retail" -DPLATFORM=Web \
        -DGRAPHICS=GRAPHICS_API_OPENGL_ES3 "-DCMAKE_CXX_FLAGS=-pthread -fwasm-exceptions -msimd128 -msse4.1" \
        "-DCMAKE_C_FLAGS=-pthread -fwasm-exceptions" -B "$bdir"
fi

step 'build wotm (wasm)'
sh "$REPO/tools/build_runtime.sh" build_web wotm

step 'assemble web_dist'
dist=$rt/web_dist
rm -rf "$dist"
mkdir -p "$dist"
cp "$REPO"/web/*.html "$REPO"/web/*.js "$REPO"/web/*.json "$dist/"
python3 "$REPO/tools/web_mods.py" "$REPO/mods" "$dist/mods" || fail "web_mods.py failed"   # the repo's mods, as natively
cp "$bdir/ps2xRuntime/wotm.js" "$bdir/ps2xRuntime/wotm.wasm" "$dist/"
if command -v shasum >/dev/null 2>&1; then wasm_sha=$(shasum -a 256 "$dist/wotm.wasm" | cut -d' ' -f1)
else wasm_sha=$(sha256sum "$dist/wotm.wasm" | cut -d' ' -f1); fi
# build: the full id (which build this is); sim: the simulation id (tools/build_id.py), the one the page sends and lobbies compare
full_id=$(cat "$bdir/ps2xRuntime/wotm.fullid" 2>/dev/null || true)
sim_id=$(cat "$bdir/ps2xRuntime/wotm.buildid" 2>/dev/null || true)
[ -n "$full_id" ] && [ -n "$sim_id" ] || fail "no wotm.fullid or wotm.buildid in $bdir/ps2xRuntime (tools/build_id.py --write runs in the build)"
# page: the dist's own id, from both ids and every file in it (tools/page_id.sh: the page scripts and the mods are not in the full
# id). The site serves the dist at b/<page id>/ (#187) and caches its files as never changing, so any change must be a new address
page_id=$(sh "$REPO/tools/page_id.sh" "$dist" "$full_id" "$sim_id") || fail "page_id.sh failed"
printf '{"wasm": "%s", "build": "%s", "sim": "%s", "page": "%s"}\n' "$wasm_sha" "$full_id" "$sim_id" "$page_id" >"$dist/version.json"
# the site's entry page (/, by deploy_web.sh): the dist lives at b/<page id>/, and this page only sends the browser there with
# its query (an invite's ?join=) and hash; inline, so the extra step costs one small request. It carries index.html's head from the
# charset to the icon (description, canonical, social cards, JSON-LD) for crawlers and link previews, and its dark background
head=$(sed -n '/^<meta charset/,/^<link rel="icon"/p' "$REPO/web/index.html")
printf '%s\n' "$head" | grep -q '^<link rel="icon"' || fail "web/index.html has no head from <meta charset> to <link rel=\"icon\">"
cat >"$dist/entry.html" <<EOF
<!doctype html>
<html lang="en">
<head>
$head
<style>html { background: #0b0b0d; color-scheme: dark; }</style>
<script>location.replace('b/$page_id/' + location.search + location.hash);</script>
</head>
<body><noscript><a href="b/$page_id/">War of the Monsters</a></noscript></body>
</html>
EOF
echo "web_dist: $dist ($(du -sh "$dist" | cut -f1))"
