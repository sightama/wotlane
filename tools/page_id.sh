#!/bin/sh
# ABOUTME: Prints a web dist's page id (wl-<16 hex>): a hash of the full id, the simulation id and every file's name and content. The site
# ABOUTME: serves the dist at b/<page id>/ (#187) with its files cached as never changing, so any change must give a new id. By build_web.sh.
set -eu

case ${1:-} in -h|--help|'') echo "usage: sh tools/page_id.sh <dist dir> <full id> <simulation id>   (prints wl-<16 hex>)"; exit 0 ;; esac
[ $# -eq 3 ] || { echo "error: wants <dist dir> <full id> <simulation id>" >&2; exit 2; }
[ -d "$1" ] || { echo "error: no dist at $1" >&2; exit 1; }
sha256() { if command -v shasum >/dev/null 2>&1; then shasum -a 256; else sha256sum; fi; }
list=$(mktemp)
files=$(mktemp)
trap 'rm -f "$list" "$files"' EXIT
# one line a file, '<sha256> <path>', in a fixed order, after the ids; a folder that cannot be listed or a file that cannot be hashed
# stops it (an id without them would name another dist). find writes to a file, not a pipe, so its failure is seen
echo "$2 $3" >"$list"
(cd "$1" && find . -type f) >"$files" || { echo "error: cannot list every file of $1" >&2; exit 1; }
LC_ALL=C sort "$files" | while IFS= read -r f; do
    h=$(sha256 <"$1/$f" | cut -d' ' -f1) || { echo "error: cannot read $1/$f" >&2; exit 1; }
    case $h in *[!0-9a-f]*|'') echo "error: no sha256 of $1/$f" >&2; exit 1 ;; esac
    [ ${#h} -eq 64 ] || { echo "error: no sha256 of $1/$f" >&2; exit 1; }
    printf '%s %s\n' "$h" "$f" >>"$list"
done
echo "wl-$(sha256 <"$list" | cut -c1-16)"
