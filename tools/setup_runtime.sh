#!/bin/sh
# Sets up everything the port needs from your ISO, in one command (macOS/Linux counterpart of setup_runtime.ps1): extracted disc,
# PS2Recomp with the repo's patches, the translated game (recomp/retail), the runtime folder (runtime/) and the first build of wotm.
# Safe to re-run: every step skips what already exists. Nothing from the game goes into the repo (disc/, recomp/ and runtime/ are
# in .gitignore).
set -eu
. "$(dirname "$0")/posix_env.sh"

ELF_SHA1=1ed7f8457436a9170d8f85386e267369c7fcdd4f        # SCUS_971.97 from the NTSC-U disc (SCUS-97197)
PS2RECOMP_COMMIT=2c5fbb9389e11dd95693385969490c9e8e6f57b4  # base of tools/runtime_patch/wotm-runtime.patch

usage() {
    cat <<EOF
usage: sh tools/setup_runtime.sh [--iso <path to .iso>] [--skip-build]
  --iso         your SCUS-97197 ISO (default: the first .iso in ISO/)
  --skip-build  stop after configuring build_dev (build later with sh tools/build_runtime.sh)
Needs: git, python3, cmake 3.24+, ninja, a C++ compiler, pkg-config and FFmpeg (macOS: brew install cmake ninja pkgconf ffmpeg
sevenzip). Logs of each step go to runtime/logs/setup_*.log.
EOF
}

iso=
skip_build=0
while [ $# -gt 0 ]; do
    case $1 in
        -h|--help) usage; exit 0 ;;
        --iso) iso=$2; shift 2 ;;
        --skip-build) skip_build=1; shift ;;
        *) usage >&2; exit 2 ;;
    esac
done

rt=$REPO/runtime
logs=$rt/logs

step tools
need git python3 cmake ninja c++ pkg-config
cmake_ver=$(cmake --version | sed -n 's/^cmake version \([0-9]*\.[0-9]*\).*/\1/p')
[ "$(printf '%s\n3.24\n' "$cmake_ver" | sort -t. -k1,1n -k2,2n | head -n1)" = 3.24 ] || fail "need CMake 3.24 or newer (have $cmake_ver)"
pkg-config --exists libavcodec libavformat libavutil libswresample libswscale || fail "FFmpeg not found by pkg-config (brew install ffmpeg)"
if command -v shasum >/dev/null 2>&1; then sha1() { shasum -a 1 "$1" | cut -d' ' -f1; }; else need sha1sum; sha1() { sha1sum "$1" | cut -d' ' -f1; }; fi

step disc
if [ -z "$iso" ]; then
    for f in "$REPO"/ISO/*.iso; do [ -f "$f" ] && { iso=$f; break; }; done
fi
[ -n "$iso" ] && [ -f "$iso" ] || fail "no .iso: put yours in ISO/ or pass --iso"
iso=$(cd "$(dirname "$iso")" && pwd)/$(basename "$iso")
disc=$REPO/disc
if [ ! -f "$disc/SCUS_971.97" ]; then
    need 7zz
    run_logged "$logs/setup_disc.log" 7zz x -y "-o$disc" "$iso"
fi
sha=$(sha1 "$disc/SCUS_971.97")
[ "$sha" = "$ELF_SHA1" ] || fail "SCUS_971.97 has SHA1 $sha: the port only supports the NTSC-U disc (SCUS-97197, $ELF_SHA1)"

step PS2Recomp
pr=$rt/PS2Recomp
mkdir -p "$rt"
if [ ! -d "$pr/.git" ]; then
    run_logged "$logs/setup_clone.log" git clone https://github.com/ran-j/PS2Recomp.git "$pr"
    git -c advice.detachedHead=false -C "$pr" checkout -q "$PS2RECOMP_COMMIT"
fi
for p in wotm-runtime.patch ps2recomp-fpu-sqrt.patch; do
    patch=$REPO/tools/runtime_patch/$p
    if git -C "$pr" apply --whitespace=nowarn --reverse --check "$patch" >/dev/null 2>&1; then echo "$p already applied"; continue; fi
    git -C "$pr" apply --whitespace=nowarn "$patch" || fail "$p doesn't apply to $pr (did the clone change?)"
    echo "$p applied"
done

step 'runtime folder'
printf '%s\n' "$rt" >"$REPO/.wotm_recomp_dir"
printf '%s\n' "$iso" >"$rt/.wotm_iso"   # run_live.sh reads the ISO from here (the game reads raw disc sectors: PS2X_CD_IMAGE)
mkdir -p "$rt/discroot"
cp "$disc/SCUS_971.97" "$rt/discroot/SCUS_971.97"
sh "$REPO/tools/sync_mods.sh" "$rt" "$REPO"

step 'translated game'
retail=$REPO/recomp/retail
if [ ! -f "$retail/register_functions.cpp" ]; then
    if [ ! -x "$rt/build_tools/ps2xRecomp/ps2_recomp" ]; then
        run_logged "$logs/setup_tools_configure.log" cmake -S "$pr" -G Ninja -DCMAKE_BUILD_TYPE=Release -DPS2X_BUILD_RUNTIME=OFF \
            -DPS2X_BUILD_TEST=OFF -DPS2X_BUILD_STUDIO=OFF -B "$rt/build_tools"
        run_logged "$logs/setup_tools_build.log" cmake --build "$rt/build_tools"
    fi
    work=$REPO/recomp/work
    mkdir -p "$work"
    cp "$disc/SCUS_971.97" "$work/game.elf"
    (cd "$work" && run_logged "$logs/setup_analyzer.log" "$rt/build_tools/ps2xAnalyzer/ps2_analyzer" game.elf config.toml)
    sed 's|^output = .*|output = "../retail/"|' "$work/config.toml" >"$work/config.toml.tmp" && mv "$work/config.toml.tmp" "$work/config.toml"
    (cd "$work" && run_logged "$logs/setup_recomp.log" "$rt/build_tools/ps2xRecomp/ps2_recomp" config.toml)
fi
echo "$(find "$retail" -maxdepth 1 -name '*.cpp' | wc -l | tr -d ' ') translated files"

step 'configure build_dev'
if [ ! -f "$rt/build_dev/build.ninja" ]; then
    # same -D flags as tools/runtime_template/configure_dev.bat
    run_logged "$logs/setup_configure_dev.log" cmake -S "$pr" -G Ninja -DCMAKE_BUILD_TYPE=Release -DWOTM_NATIVE_DIR="$REPO/native" \
        -DPS2X_FAST_LINK=ON -DPS2X_ENABLE_AGRESSIVE_LOGS=OFF -DPS2X_ENABLE_IOP_RPC_TRACE=OFF -DPS2X_BUILD_RECOMP=OFF \
        -DPS2X_BUILD_ANALYZER=OFF -DPS2X_BUILD_TEST=OFF -DPS2X_BUILD_STUDIO=OFF -DPS2X_ENABLE_DEBUG_UI=OFF \
        -DPS2X_RUNNER_DIR="$REPO/recomp/retail" -B "$rt/build_dev"
fi

if [ "$skip_build" -eq 0 ]; then
    step 'build (the first one takes several minutes)'
    sh "$REPO/tools/build_runtime.sh"
fi
step done
