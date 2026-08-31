#!/bin/bash
# Boot the image on Spike (RVA22S64) and report throughput.
# Spike's own --stats is the authority for the instruction count: the target's
# instret CSR is reprogrammed by Linux's SBI PMU driver and undercounts badly.
source "$(dirname "$0")/env.sh"
SPIKE="${SPIKE_BIN:-$BLD/spike-install/bin/spike}"
LOG="${1:-$OUT/run.log}"

START=$(date +%s.%N)
"$SPIKE" --stats --isa="$SPIKE_ISA" --priv=MSU -p1 -m0x80000000:0x40000000 \
         --dtb="$OUT/spike-rva22.dtb" "$OUT/fw_payload.elf" 2>"$OUT/stats.txt" | tee "$LOG"
END=$(date +%s.%N)
cat "$OUT/stats.txt" >> "$LOG"

sed -n '/simulation statistics/,$p' "$OUT/stats.txt"
echo "  total process time   : $(echo "$END - $START" | bc -l | xargs printf '%.3f') s"
