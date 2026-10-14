#!/bin/sh
# Convert game TUs to cpp stubs one at a time; keep each only if the ROM still matches. No git here (commit from Windows).
# Usage: sh tools/wsl/convert_tus.sh TuA TuB ...
cd "$(dirname "$0")/../.."
mkdir -p build
py=${PYTHON:-$HOME/.venvs/wotm/bin/python}
for t in "$@"; do
    cp SCUS_971.97.yaml build/.conv_yaml.bak
    cp config/decomp_tus.txt build/.conv_tus.bak
    $py tools/new_tu.py "game/$t" > /dev/null 2>&1
    ok=0
    if sh tools/wsl/gate.sh > /dev/null 2>&1; then
        ok=1
    else
        # data-only stubs (vtables) must sit at their retail rodata position
        $py tools/place_rodata.py "game/$t" > /dev/null 2>&1
        if sh tools/wsl/gate.sh > /dev/null 2>&1; then ok=1; fi
        [ $ok = 1 ] || {
        for f in asm/nonmatchings/game/$t/*.s; do
            if ! grep -q '^glabel' "$f"; then
                $py tools/place_data.py "game/$t" "$(basename "$f" .s)" > /dev/null 2>&1
            fi
        done
        if sh tools/wsl/gate.sh > /dev/null 2>&1; then ok=1; fi
        }
    fi
    if [ $ok = 1 ]; then
        echo "ok $t"
    else
        echo "FAIL $t"
        rm -f "src/game/$t.cpp"
        cp build/.conv_yaml.bak SCUS_971.97.yaml
        cp build/.conv_tus.bak config/decomp_tus.txt
        $py configure.py --split > /dev/null 2>&1
    fi
done
