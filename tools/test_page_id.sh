#!/bin/sh
# ABOUTME: Checks tools/page_id.sh: a dist's page id (its folder under the site's b/, #187) is the same for the same files and ids, and
# ABOUTME: changes with any file's content or name, an added file, the full id or the simulation id.
set -eu

case ${1:-} in -h|--help) echo "usage: sh tools/test_page_id.sh"; exit 0 ;; esac
here=$(cd "$(dirname "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fails=0
check() { if [ "$2" = "$3" ]; then echo "ok     $1"; else echo "FAILED $1: got \"$2\", wanted \"$3\""; fails=$((fails + 1)); fi; }
id() { sh "$here/page_id.sh" "$tmp/dist" "${1:-wl-1111111111111111}" "${2:-wl-5111111111111111}"; }

mkdir -p "$tmp/dist/mods/a b"
echo page >"$tmp/dist/index.html"
echo front >"$tmp/dist/front.js"
echo png >"$tmp/dist/mods/a b/t.png"
first=$(id)
check "a page id is wl- and 16 hex" "$(echo "$first" | grep -c '^wl-[0-9a-f]\{16\}$')" 1
check "the same files and ids give the same id" "$(id)" "$first"
echo front2 >"$tmp/dist/front.js"
changed=$(id)
check "a page script's content changes it" "$( [ "$changed" != "$first" ] && echo changed || echo same)" changed
echo front >"$tmp/dist/front.js"
check "and back gives the first id" "$(id)" "$first"
echo png2 >"$tmp/dist/mods/a b/t.png"
check "a mod file in a folder with a space changes it" "$( [ "$(id)" != "$first" ] && echo changed || echo same)" changed
echo png >"$tmp/dist/mods/a b/t.png"
mv "$tmp/dist/front.js" "$tmp/dist/front2.js"
check "a renamed file changes it" "$( [ "$(id)" != "$first" ] && echo changed || echo same)" changed
mv "$tmp/dist/front2.js" "$tmp/dist/front.js"
echo x >"$tmp/dist/extra.js"
check "an added file changes it" "$( [ "$(id)" != "$first" ] && echo changed || echo same)" changed
rm "$tmp/dist/extra.js"
check "another full id changes it" "$( [ "$(id wl-2222222222222222)" != "$first" ] && echo changed || echo same)" changed
check "another simulation id changes it" "$( [ "$(id wl-1111111111111111 wl-5222222222222222)" != "$first" ] && echo changed || echo same)" changed
rc=0
sh "$here/page_id.sh" "$tmp/nothing" wl-1111111111111111 wl-5111111111111111 >/dev/null 2>&1 || rc=$?
check "a missing dist is an error" "$rc" 1
if [ "$(id -u)" != 0 ]; then   # root reads any file
    chmod 000 "$tmp/dist/front.js"
    rc=0
    id >/dev/null 2>&1 || rc=$?
    check "a file it cannot read is an error, not an id without it" "$rc" 1
    chmod 644 "$tmp/dist/front.js"
fi

if [ $fails -eq 0 ]; then echo PASSED; else echo "FAILED ($fails)"; exit 1; fi
