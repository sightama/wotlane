#!/bin/sh
# Copies the repo's mods (mods/<mod>/) into the folder the game reads (<runtime>/discroot/mods/<mod>/), mirroring each repo folder.
# Mods that only exist in discroot (your own, _work) are left alone. macOS/Linux counterpart of sync_mods.ps1; called by
# setup_runtime.sh and run_live.sh.
set -eu
. "$(dirname "$0")/posix_env.sh"

case ${1:-} in -h|--help) echo "usage: sh tools/sync_mods.sh [runtime dir] [repo dir]"; exit 0 ;; esac
rt=${1:-$(runtime_dir)}
repo=${2:-$REPO}
[ -d "$rt/discroot" ] || fail "runtime folder (with discroot) not found: $rt"
[ -d "$repo/mods" ] || exit 0
need rsync
mkdir -p "$rt/discroot/mods"
for mod in "$repo"/mods/*/; do
    name=$(basename "$mod")
    case $name in _*|.*) continue ;; esac
    n=$(rsync -a --delete --itemize-changes "$mod" "$rt/discroot/mods/$name/" | grep -c '^[>c*]' || true)
    [ "$n" -gt 0 ] && echo "[mods] $name: $n file(s) updated"
done
exit 0
