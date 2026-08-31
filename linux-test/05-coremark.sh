#!/bin/bash
# Build CoreMark as a bare-metal RVA22U64 image for Spike.
#
# Unlike the Linux boot, this is a compute kernel with a tiny working set: it
# stresses the interpreter's dispatch loop rather than its TLB and page walker,
# so the two benchmarks bracket Spike's throughput from both sides.
source "$(dirname "$0")/env.sh"

CM="$SRC/coremark"
PORT="$LT/coremark-port"
OBJ="$BLD/coremark"
CC=riscv64-unknown-elf-gcc
# Sized for a ~2 s host run: long enough to swamp process startup, short enough
# to keep a seven-run benchmark under half a minute.
ITERATIONS="${COREMARK_ITERATIONS:-5000}"
# medany: the image lives at 0x80000000, out of reach of medlow's lui/addi pair.
GUEST_OPT="-O2 -fno-common -mcmodel=medany"

mkdir -p "$OBJ"

# Upstream's ee_printf.c ships an unbuildable stub for the character sink -- an
# #error every port is expected to replace.  Substituting the one line keeps
# upstream as the single source of truth for the other 700.
sed 's|^#error "You must implement the method uart_send_char.*|    spike_putchar(c);|' \
    "$CM/barebones/ee_printf.c" > "$OBJ/ee_printf.c"
grep -q 'spike_putchar(c);' "$OBJ/ee_printf.c" || {
  echo "ee_printf.c stub not found -- upstream changed, update the substitution" >&2
  exit 1
}

FLAGS_STR="$GUEST_OPT -march=$RVA22U64_MARCH -mabi=$RVA22_ABI"

$CC $GUEST_OPT -march="$RVA22U64_MARCH" -mabi="$RVA22_ABI" \
    --specs=picolibc.specs -nostartfiles -ffreestanding \
    -I"$CM" -I"$PORT" -T "$PORT/spike.ld" \
    -DPERFORMANCE_RUN=1 -DITERATIONS="$ITERATIONS" \
    -DFLAGS_STR="\"$FLAGS_STR\"" \
    "$PORT/crt.S" "$PORT/spike_port.c" "$PORT/core_portme.c" \
    "$CM/core_main.c" "$CM/core_list_join.c" "$CM/core_matrix.c" \
    "$CM/core_state.c" "$CM/core_util.c" \
    "$OBJ/ee_printf.c" "$CM/barebones/cvt.c" \
    -lm -o "$OUT/coremark.elf" 2>&1 | tee "$BLD/coremark.log"

test -f "$OUT/coremark.elf" || { echo "COREMARK BUILD FAILED"; exit 1; }
echo "COREMARK OK ($ITERATIONS iterations): $OUT/coremark.elf"
