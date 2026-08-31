#!/bin/bash
# Run the boot N times and report simulator throughput statistics.
source "$(dirname "$0")/env.sh"
SPIKE="${SPIKE_BIN:-$BLD/spike-install/bin/spike}"
N="${1:-5}"

for i in $(seq 1 "$N"); do
  S=$(date +%s.%N)
  "$SPIKE" --stats --isa="$SPIKE_ISA" --priv=MSU -p1 -m0x80000000:0x40000000 \
           --dtb="$OUT/spike-rva22.dtb" "$OUT/fw_payload.elf" \
           >"$OUT/bench.log" 2>"$OUT/bench.stats"
  E=$(date +%s.%N)
  grep -q '=== ls done ===' "$OUT/bench.log" || { echo "run $i FAILED"; tail -20 "$OUT/bench.log"; exit 1; }
  I=$(awk '/instructions retired/{print $4}' "$OUT/bench.stats")
  T=$(awk '/elapsed wall time/{print $5}' "$OUT/bench.stats")
  echo "$T $I $(echo "$E - $S" | bc -l)"
done | awk -v spike="$SPIKE" '
  { t=$1; ins=$2; s+=t; p+=$3; if(NR==1||t<min)min=t }
  END {
    mean=s/NR
    printf "binary          : %s\n", spike
    printf "runs            : %d\n", NR
    printf "instructions    : %d\n", ins
    printf "sim time mean   : %.3f s\n", mean
    printf "sim time best   : %.3f s\n", min
    printf "process mean    : %.3f s\n", p/NR
    printf "MIPS mean       : %.2f\n", ins/mean/1e6
    printf "MIPS best       : %.2f\n", ins/min/1e6
  }'
