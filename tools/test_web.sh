#!/bin/sh
# ABOUTME: Checks the browser build's own code without the game: the native layer's unit tests built with Emscripten and run under
# ABOUTME: node, a compile of the browser-only sources (WebSocket transport, GL shim, web mounts) and node --test web/. Exit 0 when all pass.
set -eu
. "$(dirname "$0")/posix_env.sh"

usage() {
    cat <<EOF
usage: sh tools/test_web.sh   (full build log: native/tests/build/web/build.log)
  Uses the Emscripten in EMSDK (default ~/emsdk) when there is one, else the one on PATH. Needs no runtime
  and no game files, so CI runs it too. The game itself (sh tools/build_web.sh) needs the translated game and is not built here.
EOF
}
case ${1:-} in -h|--help) usage; exit 0 ;; esac

emsdk=${EMSDK:-$HOME/emsdk}
if [ -f "$emsdk/emsdk_env.sh" ]; then . "$emsdk/emsdk_env.sh" >/dev/null 2>&1; fi

need emcmake em++ cmake ninja node

step 'native tests (wasm, node)'
build=$REPO/native/tests/build/web
if [ ! -f "$build/build.ninja" ]; then
    run_logged "$build/configure.log" emcmake cmake -S "$REPO/native/tests" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release
fi
run_logged "$build/build.log" cmake --build "$build"
status=0
node "$build/wotm_native_test.js" >"$build/test.log" 2>&1 || status=$?
grep -v '^ok ' "$build/test.log" || true   # failures and the totals line; the full list is in test.log
[ "$status" -eq 0 ] || fail "native tests failed under wasm"

step 'browser-only sources compile'
for f in native/net/wotm_net_transport_web.cpp native/wotm_glad_web.cpp native/wotm_webfs.cpp; do
    em++ -std=c++20 -pthread -fsyntax-only -Wall -Werror -I"$REPO/native" -I"$REPO/native/net" "$REPO/$f" || fail "$f does not compile"
    echo "ok      $f"
done

step 'page scripts (node --test web/)'
node --test "$REPO/web/" >"$build/node_test.log" 2>&1 || { cat "$build/node_test.log"; fail "node --test web/ failed"; }
grep -E "(tests|pass|fail) [0-9]+$" "$build/node_test.log"
