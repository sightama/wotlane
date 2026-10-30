#!/bin/sh
# Regenerates tools/runtime_patch/wotm-runtime.patch from the runtime's PS2Recomp clone (macOS/Linux).
# Use it after changing a PS2Recomp seam (CMake, main.cpp, taps) in <runtime>/PS2Recomp, in the same commit as that change.
# It diffs exactly the files already in the patch, plus any given with --add (new files are staged with `git add -N` so they show
# up). The files of ps2recomp-fpu-sqrt.patch are never included: that patch is applied on its own.
set -eu
. "$(dirname "$0")/posix_env.sh"

usage() {
    cat <<EOF
usage: sh tools/regen_runtime_patch.sh [--runtime <dir>] [--add <path inside PS2Recomp>]...
  --runtime  runtime folder (default: WOTM_RECOMP_DIR, .wotm_recomp_dir, or <repo>/runtime)
  --add      also include this file (e.g. a new source file the seam adds)
Rewrites tools/runtime_patch/wotm-runtime.patch; check the result with git diff tools/runtime_patch/.
EOF
}

rt=$(runtime_dir)
extra=
while [ $# -gt 0 ]; do
    case $1 in
        -h|--help) usage; exit 0 ;;
        --runtime) rt=$2; shift 2 ;;
        --add) extra="$extra $2"; shift 2 ;;
        *) usage >&2; exit 2 ;;
    esac
done

pr=$rt/PS2Recomp
patch=$REPO/tools/runtime_patch/wotm-runtime.patch
sqrt=$REPO/tools/runtime_patch/ps2recomp-fpu-sqrt.patch
[ -d "$pr/.git" ] || fail "no PS2Recomp clone at $pr (run sh tools/setup_runtime.sh)"

files=$(sed -n 's|^diff --git a/\([^ ]*\) b/.*|\1|p' "$patch")
sqrt_files=$(sed -n 's|^diff --git a/\([^ ]*\) b/.*|\1|p' "$sqrt")
list=
for f in $files $extra; do
    case " $sqrt_files " in *" $f "*) continue ;; esac
    case " $list " in *" $f "*) continue ;; esac
    list="$list $f"
done

# new files (in the patch, or added) are untracked in the clone: intent-to-add makes git diff HEAD show them
for f in $list; do
    if ! git -C "$pr" ls-files --error-unmatch -- "$f" >/dev/null 2>&1; then
        [ -e "$pr/$f" ] || fail "$f is in the file list but missing from $pr"
        git -C "$pr" add -N -- "$f"
    fi
done

# shellcheck disable=SC2086  # the list is space-separated paths without spaces
git -C "$pr" diff HEAD -- $list >"$patch.tmp"
[ -s "$patch.tmp" ] || { rm -f "$patch.tmp"; fail "empty diff: is the patch applied in $pr?"; }
mv "$patch.tmp" "$patch"
printf 'wrote %s (%s files)\n' "$patch" "$(grep -c '^diff --git' "$patch")"
