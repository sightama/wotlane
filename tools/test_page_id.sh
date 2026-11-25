#!/bin/sh
# ABOUTME: Checks tools/page_id.sh: a dist's page id (its folder under the site's b/, #187) is the same for the same files and ids, and
# ABOUTME: changes with any file's content or name, an added file or either id; an unreadable file or folder, a link or no files is an error.
set -eu

case ${1:-} in -h|--help) echo "usage: sh tools/test_page_id.sh"; exit 0 ;; esac
here=$(cd "$(dirname "$0")" && pwd)
tmp=$(mktemp -d)
trap 'chmod -R u+rwx "$tmp" 2>/dev/null; rm -rf "$tmp"' EXIT   # the permission checks may be cut short
fails=0
check() { if [ "$2" = "$3" ]; then echo "ok     $1"; else echo "FAILED $1: got \"$2\", wanted \"$3\""; fails=$((fails + 1)); fi; }
page_id() { sh "$here/page_id.sh" "$tmp/dist" "${1:-wl-1111111111111111}" "${2:-wl-5111111111111111}"; }

mkdir -p "$tmp/dist/mods/a b"
echo page >"$tmp/dist/index.html"
echo front >"$tmp/dist/front.js"
echo png >"$tmp/dist/mods/a b/t.png"
first=$(page_id)
check "a page id is wl- and 16 hex" "$(echo "$first" | grep -c '^wl-[0-9a-f]\{16\}$')" 1
check "the same files and ids give the same id" "$(page_id)" "$first"
echo front2 >"$tmp/dist/front.js"
changed=$(page_id)
check "a page script's content changes it" "$( [ "$changed" != "$first" ] && echo changed || echo same)" changed
echo front >"$tmp/dist/front.js"
check "and back gives the first id" "$(page_id)" "$first"
echo png2 >"$tmp/dist/mods/a b/t.png"
check "a mod file in a folder with a space changes it" "$( [ "$(page_id)" != "$first" ] && echo changed || echo same)" changed
echo png >"$tmp/dist/mods/a b/t.png"
mv "$tmp/dist/front.js" "$tmp/dist/front2.js"
check "a renamed file changes it" "$( [ "$(page_id)" != "$first" ] && echo changed || echo same)" changed
mv "$tmp/dist/front2.js" "$tmp/dist/front.js"
echo x >"$tmp/dist/extra.js"
check "an added file changes it" "$( [ "$(page_id)" != "$first" ] && echo changed || echo same)" changed
rm "$tmp/dist/extra.js"
check "another full id changes it" "$( [ "$(page_id wl-2222222222222222)" != "$first" ] && echo changed || echo same)" changed
check "another simulation id changes it" "$( [ "$(page_id wl-1111111111111111 wl-5222222222222222)" != "$first" ] && echo changed || echo same)" changed
rc=0
sh "$here/page_id.sh" "$tmp/nothing" wl-1111111111111111 wl-5111111111111111 >/dev/null 2>&1 || rc=$?
check "a missing dist is an error" "$rc" 1
if [ "$(id -u)" != 0 ]; then   # root reads any file
    chmod 000 "$tmp/dist/front.js"
    rc=0
    page_id >/dev/null 2>&1 || rc=$?
    check "a file it cannot read is an error, not an id without it" "$rc" 1
    chmod 644 "$tmp/dist/front.js"
    chmod 000 "$tmp/dist/mods/a b"
    rc=0
    page_id >/dev/null 2>&1 || rc=$?
    check "a folder it cannot list is an error, not an id without its files" "$rc" 1
    chmod 755 "$tmp/dist/mods/a b"
else
    echo "skip   the unreadable file and folder checks (root reads them all)"
fi
# a link is not hashed through: the site would serve what it points at, outside the id
ln -s front.js "$tmp/dist/link.js"
rc=0
page_id >/dev/null 2>&1 || rc=$?
check "a symbolic link in the dist is an error" "$rc" 1
rm "$tmp/dist/link.js"
check "and without it the first id again" "$(page_id)" "$first"
mkdir -p "$tmp/empty"
rc=0
sh "$here/page_id.sh" "$tmp/empty" wl-1111111111111111 wl-5111111111111111 >/dev/null 2>&1 || rc=$?
check "a dist with no files is an error" "$rc" 1

if [ $fails -eq 0 ]; then echo PASSED; else echo "FAILED ($fails)"; exit 1; fi
