#!/bin/bash
# Run bare-metal CoreMark on Spike N times and report simulator throughput.
#
# CoreMark's own "Errors detected" line is expected here: the only failing check
# is its rule that a valid submission must run for at least ten *simulated*
# seconds, which would cost minutes of host time.  Correctness is instead taken
# from the three data CRCs, which are fixed for the 2K performance run.
source "$(dirname "$0")/env.sh"
SPIKE="${SPIKE_BIN:-$BLD/spike-install/bin/spike}"
N="${1:-5}"

for i in $(seq 1 "$N"); do
  S=$(date +%s.%N)
  "$SPIKE" --stats --isa="$SPIKE_ISA" "$OUT/coremark.elf" \
           >"$OUT/coremark.log" 2>"$OUT/coremark.stats"
  E=$(date +%s.%N)
  for crc in 'crclist       : 0xe714' 'crcmatrix     : 0x1fd7' 'crcstate      : 0x8e3a'; do
    grep -q "$crc" "$OUT/coremark.log" || { echo "run $i FAILED ($crc)"; cat "$OUT/coremark.log"; exit 1; }
  done
  I=$(awk '/instructions retired/{print $4}' "$OUT/coremark.stats")
  T=$(awk '/elapsed wall time/{print $5}' "$OUT/coremark.stats")
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
