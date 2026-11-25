#!/bin/sh
# ABOUTME: Prints a web dist's page id (wl-<16 hex>): a hash of the full id, the simulation id and every file's name and content. The site
# ABOUTME: serves the dist at b/<page id>/ (#187) with its files cached as never changing, so any change must give a new id. By build_web.sh.
set -eu

case ${1:-} in -h|--help|'') echo "usage: sh tools/page_id.sh <dist dir> <full id> <simulation id>   (prints wl-<16 hex>)"; exit 0 ;; esac
[ $# -eq 3 ] || { echo "error: wants <dist dir> <full id> <simulation id>" >&2; exit 2; }
[ -d "$1" ] || { echo "error: no dist at $1" >&2; exit 1; }
sha256() { if command -v shasum >/dev/null 2>&1; then shasum -a 256; else sha256sum; fi; }
# one line a file, '<sha256> <path>', in a fixed order; the ids first
id=$( { echo "$2 $3"; (cd "$1" && find . -type f | LC_ALL=C sort | while IFS= read -r f; do
    printf '%s %s\n' "$(sha256 <"$f" | cut -d' ' -f1)" "$f"; done); } | sha256 | cut -c1-16)
echo "wl-$id"
