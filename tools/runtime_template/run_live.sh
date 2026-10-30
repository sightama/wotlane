#!/bin/sh
# Launches the recompiled War of the Monsters (wotm, or the ps2EntryRunner test build) from discroot. macOS/Linux counterpart of
# run_live.ps1; tools/setup_runtime.sh copies it into the runtime folder. stderr goes to live.err.log and stdout to live.out.log in
# this folder.
set -eu

usage() {
    cat <<EOF
usage: sh run_live.sh [--auto 0|1] [--stats] [--build <dir>] [--exe test]
  --auto 1      play the pad script (pad_script.sh in this folder, which must set PS2X_PAD); 0 = you play (default 1, as on Windows)
  --stats       PS2X_STATS=1 ([native] timing lines)
  --build <dir> build folder (default build_dev)
  --exe test    run ps2EntryRunner (also picked automatically with WOTM_VU1CHECK or WOTM_FOTOS, or when wotm is missing)
The ISO comes from PS2X_CD_IMAGE, else the one setup_runtime.sh used (.wotm_iso), else the first .iso in <repo>/ISO.
EOF
}

auto=1 stats=0 build=build_dev exe=
while [ $# -gt 0 ]; do
    case $1 in
        -h|--help) usage; exit 0 ;;
        --auto) auto=$2; shift 2 ;;
        --stats) stats=1; shift ;;
        --build) build=$2; shift 2 ;;
        --exe) exe=$2; shift 2 ;;
        *) usage >&2; exit 2 ;;
    esac
done

rt=$(cd "$(dirname "$0")" && pwd)
repo=${WOTM_REPO:-$(dirname "$rt")}
bin=$rt/$build/ps2xRuntime
path=$bin/wotm
if [ "$exe" = test ] || [ -n "${WOTM_VU1CHECK:-}" ] || [ -n "${WOTM_FOTOS:-}" ] || [ ! -x "$path" ]; then path=$bin/ps2EntryRunner; fi
[ -x "$path" ] || { echo "error: executable not found: $path (run sh tools/build_runtime.sh)" >&2; exit 1; }

disc=$rt/discroot
elf=$disc/SCUS_971.97
[ -f "$elf" ] || { echo "error: ELF not found: $elf" >&2; exit 1; }

# the game reads raw disc sectors (fileCdSearchFile walks ISO9660 from sector 0x10): it needs the ISO, not just the loose files
if [ -z "${PS2X_CD_IMAGE:-}" ]; then
    if [ -f "$rt/.wotm_iso" ] && [ -f "$(cat "$rt/.wotm_iso")" ]; then PS2X_CD_IMAGE=$(cat "$rt/.wotm_iso")
    else
        for f in "$repo"/ISO/*.iso; do [ -f "$f" ] && { PS2X_CD_IMAGE=$f; break; }; done
    fi
    [ -n "${PS2X_CD_IMAGE:-}" ] || { echo "error: no .iso in $repo/ISO (set PS2X_CD_IMAGE)" >&2; exit 1; }
fi
export PS2X_CD_IMAGE

# the repo's mods (mods/<mod>/) into discroot/mods/ (tools/sync_mods.sh; docs/guides/MODS.md)
[ -f "$repo/tools/sync_mods.sh" ] && sh "$repo/tools/sync_mods.sh" "$rt" "$repo"

export PS2X_NO_VU1=1 PS2X_NO_VIF1=1 PS2X_NO_GS=1 PS2X_SCENE=1 PS2X_FMV=1
[ "$stats" -eq 1 ] && export PS2X_STATS=1
if [ "$auto" -eq 1 ]; then
    if [ -f "$rt/pad_script.sh" ]; then . "$rt/pad_script.sh"; export PS2X_PAD
    else echo "pad_script.sh doesn't exist: --auto 1 runs without a script" >&2; unset PS2X_PAD; fi
fi   # --auto 0: an inherited PS2X_PAD (checks) still applies

echo "Running $path"
cd "$disc"
code=0
"$path" "$elf" 2>"$rt/live.err.log" >"$rt/live.out.log" || code=$?
exit $code
