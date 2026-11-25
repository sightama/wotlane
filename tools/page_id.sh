#!/bin/sh
# ABOUTME: Prints a web dist's page id (wl-<16 hex>): a hash of the full id, the simulation id and every file's name and content. The site
# ABOUTME: serves the dist at b/<page id>/ (#187) with its files cached as never changing, so any change must give a new id. By build_web.sh.
set -eu

case ${1:-} in -h|--help|'') echo "usage: sh tools/page_id.sh <dist dir> <full id> <simulation id>   (prints wl-<16 hex>)"; exit 0 ;; esac
[ $# -eq 3 ] || { echo "error: wants <dist dir> <full id> <simulation id>" >&2; exit 2; }
[ -d "$1" ] || { echo "error: no dist at $1" >&2; exit 1; }
sha256() { if command -v shasum >/dev/null 2>&1; then shasum -a 256; else sha256sum; fi; }
# hex64 <text>: whether it is a sha256 in hex
hex64() { case $1 in *[!0-9a-f]*|'') return 1 ;; esac; [ ${#1} -eq 64 ]; }
list=$(mktemp)
files=$(mktemp)
sorted=$(mktemp)
trap 'rm -f "$list" "$files" "$sorted"' EXIT
# one line a file, '<sha256> <path>', in a fixed order, after the ids. Anything that would leave content out of the id stops it (the id
# would then name another dist): a folder that cannot be listed, a file that cannot be hashed, a link (the site serves what it points
# at), no files at all. find and sort write to files, not pipes, so their failures are seen
(cd "$1" && find . -type l) >"$files" || { echo "error: cannot list every file of $1" >&2; exit 1; }
[ ! -s "$files" ] || { echo "error: $1 holds links, which the id would not cover: $(head -1 "$files")" >&2; exit 1; }
(cd "$1" && find . -type f) >"$files" || { echo "error: cannot list every file of $1" >&2; exit 1; }
[ -s "$files" ] || { echo "error: $1 holds no files" >&2; exit 1; }
LC_ALL=C sort "$files" >"$sorted" || { echo "error: sort failed" >&2; exit 1; }
echo "$2 $3" >"$list"
while IFS= read -r f; do
    h=$(sha256 <"$1/$f" | cut -d' ' -f1)
    hex64 "$h" || { echo "error: no sha256 of $1/$f" >&2; exit 1; }
    printf '%s %s\n' "$h" "$f" >>"$list"
done <"$sorted"
h=$(sha256 <"$list" | cut -d' ' -f1)
hex64 "$h" || { echo "error: no sha256 of the file list" >&2; exit 1; }
echo "wl-$(echo "$h" | cut -c1-16)"
