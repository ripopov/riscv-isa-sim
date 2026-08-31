#!/bin/bash
# Spike's auto-generated DT only carries the deprecated "riscv,isa" string, from
# which Linux parses single-letter extensions only ("base ISA extensions acdfim").
# Take that DT and add the modern riscv,isa-base / riscv,isa-extensions bindings
# so the kernel actually sees (and patches in) the RVA22 extensions.
source "$(dirname "$0")/env.sh"
SPIKE="$BLD/spike-install/bin/spike"

"$SPIKE" --isa="$SPIKE_ISA" --priv=MSU -p1 -m0x80000000:0x40000000 \
         --dump-dts "$OUT/fw_payload.elf" > "$BLD/spike.dts"

python3 - "$BLD/spike.dts" "$BLD/spike-rva22.dts" <<'PY'
import sys, re
src, dst = sys.argv[1], sys.argv[2]
s = open(src).read()
exts = ["i","m","a","f","d","c","zicsr","zifencei","zicntr","zihpm",
        "zihintpause","zfhmin","zba","zbb","zbs","zicbom","zicbop","zicboz",
        "zkt","svpbmt","svinval","svnapot"]
add = ('\t\t\triscv,isa-base = "rv64i";\n'
       '\t\t\triscv,isa-extensions = ' + ", ".join('"%s"' % e for e in exts) + ';\n'
       '\t\t\triscv,cbom-block-size = <0x40>;\n'
       '\t\t\triscv,cbop-block-size = <0x40>;\n'
       '\t\t\triscv,cboz-block-size = <0x40>;\n')
s2, n = re.subn(r'(\t\t\triscv,isa = "[^"]*";\n)', r'\1' + add, s)
assert n == 1, n
open(dst, "w").write(s2)
PY

dtc -I dts -O dtb -o "$OUT/spike-rva22.dtb" "$BLD/spike-rva22.dts" 2>/dev/null
ls -l "$OUT/spike-rva22.dtb"
echo "DTB OK"
