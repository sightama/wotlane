#!/bin/sh
# ABOUTME: Checks the Mac friends package the way two friends would use it: unzips it, runs the app's first-run setup from the ISO, then
# ABOUTME: one copy hosts on the shared server and another joins with the code only; their match must stay identical. Exit 0 if all pass.
set -eu
. "$(dirname "$0")/posix_env.sh"

usage() {
    cat <<EOF
usage: sh tools/package_check_mac.sh [--zip <package.zip>] [--iso <iso>] [--secs 60] [--server host:port]
  --zip     default: the newest dist/wotlane-mac-*.zip (sh tools/package_friends_mac.sh builds it)
  --iso     default: the runtime's ISO      --secs  how long the match runs (default 60)
  --server  another lobby server (default: the shared one, as friends use)
Work folder and logs: dist/package_check_mac/ (each player's logs in home_<n>/logs).
EOF
}
zip= iso= secs=60 server=
while [ $# -gt 0 ]; do
    case $1 in
        -h|--help) usage; exit 0 ;;
        --zip) zip=$2; shift 2 ;;
        --iso) iso=$2; shift 2 ;;
        --secs) secs=$2; shift 2 ;;
        --server) server=$2; shift 2 ;;
        *) usage >&2; exit 2 ;;
    esac
done
rt=$(runtime_dir)
[ -n "$zip" ] || zip=$(ls -t "$REPO"/dist/wotlane-mac-*.zip 2>/dev/null | head -1)
[ -f "$zip" ] || fail "no package (sh tools/package_friends_mac.sh)"
[ -n "$iso" ] || iso=$(cat "$rt/.wotm_iso")
work=$REPO/dist/package_check_mac
rm -rf "$work" && mkdir -p "$work"
failed=0
ok() { printf 'ok      %s\n' "$1"; }
bad() { printf 'FAILED  %s\n' "$1"; failed=1; }

ditto -x -k "$zip" "$work"
app=$(find "$work" -maxdepth 2 -name '*.app' | head -1)
[ -n "$app" ] || fail "no .app in $zip"
codesign --verify --deep --strict "$app" && ok "signature valid" || bad "signature"
left=$(otool -L "$app/Contents/Resources/game/wotm" | grep -E '/opt/homebrew|/usr/local' || true)
[ -z "$left" ] && ok "no libraries from Homebrew" || bad "libraries outside the app: $left"
run=$app/Contents/MacOS/wotlane
export WOTM_MUTE=1 WOTM_FULLSCREEN=0 WOTM_RES=360 WOTM_SETTINGS=0 WOTM_NOINPUT=1   # small silent windows; the app passes them to the game

# a file that is not the USA disc is refused
printf 'not a disc' >"$work/fake.iso"
if WOTLANE_HOME=$work/home_fake WOTLANE_ISO=$work/fake.iso WOTLANE_ACTION=offline sh "$run" >"$work/fake.out" 2>&1; then
    bad "a file that isn't the disc was accepted"
else
    grep -q 'not War of the Monsters for the USA' "$work/fake.out" && ok "a wrong ISO is refused in plain words" || bad "wrong ISO message: $(cat "$work/fake.out")"
fi

# player 1 hosts: the code comes back on its output
WOTLANE_HOME=$work/home_1 WOTLANE_ISO=$iso WOTLANE_ACTION=host WOTLANE_NAME=Host WOTLANE_MONSTER=5 WOTLANE_STAGE=9 WOTLANE_PLAYERS=2 \
    WOTLANE_SERVER=$server WOTM_STATEHASH=$work/p1.hash PS2X_PAD='19:5:up;21:1:triangle;23:2:right;26:1:square' \
    sh "$run" >"$work/host.out" 2>&1 &
host=$!
code=
for _ in $(seq 1 100); do
    code=$(sed -n 's/^Your game code is \([A-Z0-9]\{4\}\).*/\1/p' "$work/host.out")
    [ -n "$code" ] && break
    kill -0 "$host" 2>/dev/null || break
    sleep 0.3
done
[ -n "$code" ] && ok "host got code $code" || { bad "host got no code: $(cat "$work/host.out")"; exit 1; }
[ "$(shasum "$work/home_1/discroot/SCUS_971.97" | cut -c1-40)" = 1ed7f8457436a9170d8f85386e267369c7fcdd4f ] && ok "first run took the ELF from the ISO" || bad "ELF"

# player 2 joins with the code only
WOTLANE_HOME=$work/home_2 WOTLANE_ISO=$iso WOTLANE_ACTION=join WOTLANE_NAME=Guest WOTLANE_MONSTER=1 WOTLANE_CODE=$code WOTLANE_SERVER=$server \
    WOTM_STATEHASH=$work/p2.hash PS2X_PAD='18:6:up;20:1:square;22:1:cross;24:3:left' \
    sh "$run" >"$work/guest.out" 2>&1 &
guest=$!
sleep "$secs"
pkill -TERM -f "$work/home_[12]/discroot/SCUS_971.97" 2>/dev/null || true
sleep 3
pkill -KILL -f "$work/home_[12]/discroot/SCUS_971.97" 2>/dev/null || true
wait "$host" "$guest" 2>/dev/null || true

for n in 1 2; do
    log=$work/home_$n/logs/live.err.log
    if grep -q "\[net\] start: player $n of 2" "$log" 2>/dev/null; then ok "player $n started the match"; else bad "player $n did not start (see $log)"; fi
    grep -E '\[CRASH\]|\[watchdog\] FROZE|\[net\] stopped' "$log" 2>/dev/null && bad "player $n: problem in its log"
done
r=$(python3 "$REPO/tools/statediff.py" "$work/p1.hash" "$work/p2.hash" 2>&1) && ok "state: $r" || bad "state differs: $r"
[ "$failed" -eq 0 ] && echo PASSED || echo FAILED
exit "$failed"
