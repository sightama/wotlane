#!/bin/sh
# Create a ready-to-build worktree for working on a TU in parallel with this checkout (run from Git Bash in the repo).
#   sh tools/new_worktree.sh <name> [base]   ->  ../WoTM-wt/<name> on branch wt/<name>, built and gated
# The game files are not in git: ISO, disc and tools/cc are linked to this checkout's, asm/ and the other split outputs are copied
# (so base must have the same yaml and TU list as this checkout). Every tool writes its temporaries under the checkout's build/ or a
# per-checkout cache, so the worktrees can build and score at the same time. Remove with: git worktree remove ../WoTM-wt/<name>
set -e
name=$1; base=${2:-HEAD}
[ -n "$name" ] || { echo "usage: sh tools/new_worktree.sh <name> [base]"; exit 1; }
root=$(git rev-parse --show-toplevel)
git -C "$root" diff --quiet "$base" HEAD -- SCUS_971.97.yaml config/decomp_tus.txt ||
    { echo "$base splits differently from this checkout (yaml / decomp_tus.txt): its asm/ would not match"; exit 1; }
dst=$(dirname "$root")/WoTM-wt/$name
git -C "$root" worktree add -b "wt/$name" "$dst" "$base"
wslp() { p=$(cygpath -m "$1"); d=$(printf %s "${p%%:*}" | tr A-Z a-z); printf /mnt/%s%s "$d" "${p#?:}"; }
MSYS_NO_PATHCONV=1 wsl -d Ubuntu sh "$(wslp "$root")/tools/wsl/worktree_setup.sh" "$(wslp "$root")" "$(wslp "$dst")"
