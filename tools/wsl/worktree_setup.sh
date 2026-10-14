#!/bin/sh
# Fill a fresh git worktree with the game files it needs to build, then gate it. Called by tools/new_worktree.sh.
#   sh tools/wsl/worktree_setup.sh <source checkout> <new worktree>
set -e
src=$1; dst=$2
for l in ISO disc tools/cc; do
    ln -sfn "$(readlink -f "$src/$l")" "$dst/$l"
done
cp -a "$src/asm" "$dst/"
[ -d "$src/assets" ] && cp -a "$src/assets" "$dst/"
for f in SCUS_971.97.rom SCUS_971.97.ld SCUS_971.97.d undefined_funcs_auto.txt undefined_syms_auto.txt build.ninja; do
    cp -a "$src/$f" "$dst/"
done
cd "$dst"
sh tools/wsl/gate.sh
