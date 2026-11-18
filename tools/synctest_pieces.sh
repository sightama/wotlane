#!/bin/sh
# ABOUTME: Leaves each rollback snapshot piece out in turn (WOTM_SYNCTEST_SKIP) and runs the native synctest, to show what each piece's
# ABOUTME: absence breaks: prints, per piece, the first mismatch line and its differing ranges, or that none showed in the run.
set -eu
. "$(dirname "$0")/posix_env.sh"

usage() {
    cat <<EOF
usage: sh tools/synctest_pieces.sh [--seconds <s>] [--out <dir>] [--jobs <n>] [piece...] [-- synctest.sh options]
  piece      name prefixes as WOTM_SYNCTEST_SKIP takes them (default: every piece the snapshot has)
  --seconds  per run (default 75)
  --out      folder for the logs, one per piece (default <runtime>/synctest_pieces)
  --jobs     runs at the same time (default 2; each opens a game window)
Anything after -- goes to tools/synctest.sh (--match, --step, --back, VAR=value). A piece whose absence shows nothing in the run may
still matter elsewhere (the modifiers only with WOTM_MODIFIERS, a stream only when one starts in the window).
EOF
}

seconds=75 out= jobs=2 pieces= extra=
while [ $# -gt 0 ]; do
    case $1 in --seconds|--out|--jobs) [ $# -ge 2 ] || { usage >&2; exit 2; } ;; esac
    case $1 in
        -h|--help) usage; exit 0 ;;
        --seconds) seconds=$2; shift 2 ;;
        --out) out=$2; shift 2 ;;
        --jobs) jobs=$2; shift 2 ;;
        --) shift; extra="$*"; break ;;
        -*) usage >&2; exit 2 ;;
        *) pieces="$pieces $1"; shift ;;
    esac
done
rt=$(runtime_dir)
[ -n "$out" ] || out=$rt/synctest_pieces
mkdir -p "$out"
[ -n "$pieces" ] || pieces="upper scratchpad CPU VU0 runtime:_newlib runtime:_pad_ports runtime:_pad_frame fixed-step:_virtual fixed-step:_clock fixed-step:_vsync fixed-step:_frame orbit_camera:_cameras orbit_camera:_inputs modifiers:_gravity modifiers:_fast intro sound"

running=0
for p in $pieces; do
    skip=$(printf '%s' "$p" | tr _ ' ')
    name=$(printf '%s' "$p" | tr -c 'A-Za-z0-9\n' '_')
    # shellcheck disable=SC2086
    sh "$REPO/tools/synctest.sh" --seconds "$seconds" --log "$out/$name.log" $extra "WOTM_SYNCTEST_SKIP=$skip" >"$out/$name.txt" 2>&1 &
    running=$((running + 1))
    if [ "$running" -ge "$jobs" ]; then wait; running=0; fi
done
wait

for p in $pieces; do
    name=$(printf '%s' "$p" | tr -c 'A-Za-z0-9\n' '_')
    log=$out/$name.log
    printf '== without "%s": ' "$(printf '%s' "$p" | tr _ ' ')"
    if first=$(grep -m1 -n '^\[synctest\] MISMATCH' "$log" 2>/dev/null); then
        echo "${first#*:}" | cut -c1-260
        line=${first%%:*}
        sed -n "$((line + 1)),$((line + 8))p" "$log" | grep '^\[synctest\]   ' | cut -c1-160 || true
    else
        lines=$(grep -c '^\[synctest\] frame' "$log" 2>/dev/null) || true   # grep -c prints 0 itself when nothing matches
        echo "no mismatch in ${seconds}s (${lines:-0} cost lines)"
    fi
done
