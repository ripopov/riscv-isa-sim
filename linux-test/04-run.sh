#!/bin/bash
# Boot the image on Spike (RVA22S64), time it, and derive MIPS.
source "$(dirname "$0")/env.sh"
SPIKE="$BLD/spike-install/bin/spike"
LOG="${1:-$OUT/run.log}"

START=$(date +%s.%N)
"$SPIKE" --isa="$SPIKE_ISA" --priv=MSU -p1 -m0x80000000:0x40000000 \
         --dtb="$OUT/spike-rva22.dtb" "$OUT/fw_payload.elf" 2>&1 | tee "$LOG"
END=$(date +%s.%N)

WALL=$(echo "$END - $START" | bc -l)
INSTR=$(grep -o 'SPIKE_INSTRET=[0-9]*' "$LOG" | head -1 | cut -d= -f2)
echo
echo "wall clock      : $(printf '%.3f' "$WALL") s"
if [ -n "$INSTR" ]; then
  echo "instructions    : $INSTR"
  echo "MIPS            : $(echo "$INSTR / $WALL / 1000000" | bc -l | xargs printf '%.2f')"
fi
