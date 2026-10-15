#!/bin/sh
# Side-by-side disassembly: retail function vs compiled object. Usage: objdiff.sh <func> <obj.o>
f=$1; o=$2
root=$(cd "$(dirname "$0")/../.." && pwd)
t=$root/build; mkdir -p "$t"
addr=$(mips-linux-gnu-nm "$root/disc/SCUS_971.97" | awk -v f="$f" '$3==f {print $1}')
size=$(mips-linux-gnu-nm -S "$root/disc/SCUS_971.97" | awk -v f="$f" '$4==f {print $2}')
end=$(printf '0x%x' $((0x$addr + 0x$size)))
mips-linux-gnu-objdump -d -M no-aliases,gpr-names=o32 --start-address=0x$addr --stop-address=$end "$root/disc/SCUS_971.97" \
    | grep -E '^ +[0-9a-f]+:' | cut -f3- | sed 's/ *<.*//' > "$t/od_retail.txt"
mips-linux-gnu-objdump -d -r -M no-aliases,gpr-names=o32 --disassemble="$f" "$o" \
    | grep -E '^ +[0-9a-f]+:' | cut -f3- | sed 's/ *<.*//' > "$t/od_obj.txt"
diff -y -W 110 "$t/od_retail.txt" "$t/od_obj.txt"
