#!/bin/sh
# ABOUTME: Checks posix_env.sh check_build_dir: a CMake build folder configured for this tree (or not configured yet) is accepted, one
# ABOUTME: configured for another tree's native/ or PS2Recomp (a build folder copied from another worktree) is refused.
set -eu
. "$(dirname "$0")/posix_env.sh"

case ${1:-} in -h|--help) echo "usage: sh tools/test_build_dir_guard.sh"; exit 0 ;; esac
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fails=0
check() { if [ "$2" = "$3" ]; then echo "ok     $1"; else echo "FAILED $1: got \"$2\", wanted \"$3\""; fails=$((fails + 1)); fi; }
verdict() { (check_build_dir "$1" "$2") >"$tmp/out" 2>&1 && echo accepted || echo refused; }
cache() { mkdir -p "$tmp/$1"; printf 'WOTM_NATIVE_DIR:PATH=%s\nCMAKE_HOME_DIRECTORY:INTERNAL=%s\n' "$2" "$3" >"$tmp/$1/CMakeCache.txt"; }
mkdir -p "$tmp/rt/PS2Recomp" "$tmp/elsewhere/native" "$tmp/elsewhere/PS2Recomp"

cache mine "$REPO/native" "$tmp/rt/PS2Recomp"
check "a folder configured for this tree" "$(verdict "$tmp/mine" "$tmp/rt")" accepted
mkdir -p "$tmp/fresh"
check "a folder not configured yet" "$(verdict "$tmp/fresh" "$tmp/rt")" accepted
cache other_native "$tmp/elsewhere/native" "$tmp/rt/PS2Recomp"
check "another tree's native/" "$(verdict "$tmp/other_native" "$tmp/rt")" refused
check "the refusal names both folders and says what to do" \
    "$(grep -c "$tmp/elsewhere/native" "$tmp/out")$(grep -c "$REPO/native" "$tmp/out")$(grep -c 'delete' "$tmp/out")" 111
cache other_recomp "$REPO/native" "$tmp/elsewhere/PS2Recomp"
check "another tree's PS2Recomp" "$(verdict "$tmp/other_recomp" "$tmp/rt")" refused
ln -s "$tmp/rt" "$tmp/rt_link"
cache via_link "$REPO/native" "$tmp/rt_link/PS2Recomp"
check "the same folders through a symlink" "$(verdict "$tmp/via_link" "$tmp/rt")" accepted

[ "$fails" -eq 0 ] && echo PASSED || { echo "FAILED ($fails)"; exit 1; }
