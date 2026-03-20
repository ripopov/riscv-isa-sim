#!/bin/bash
set -euo pipefail

# Build and run the minimal RISC-V assembly example on Spike.
# This example always dumps an instruction trace with register changes.
# No verification is performed - test passes if Spike exits normally.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(git rev-parse --show-toplevel)"
BUILD_DIR="$ROOT_DIR/build"

SOURCE_FILE="$SCRIPT_DIR/hello_asm.S"
LINKER_SCRIPT="$SCRIPT_DIR/baremetal.ld"
OUTPUT_ELF="$SCRIPT_DIR/hello_asm.elf"
DISASM_FILE="$SCRIPT_DIR/hello_asm.dis"
TRACE_FILE="$SCRIPT_DIR/hello_asm_trace.log"
SPIKE_BIN="$BUILD_DIR/spike"

echo "=== Compiling hello_asm.S ==="
riscv64-unknown-elf-gcc \
    -march=rv64gc \
    -mabi=lp64d \
    -nostdlib \
    -nostartfiles \
    -Wl,--no-warn-rwx-segments \
    -T "$LINKER_SCRIPT" \
    -static \
    -mcmodel=medany \
    -o "$OUTPUT_ELF" \
    "$SOURCE_FILE"

echo "=== Disassembling ELF ==="
riscv64-unknown-elf-objdump -d "$OUTPUT_ELF" > "$DISASM_FILE"

echo "=== Running on Spike (with instruction trace) ==="
"$SPIKE_BIN" --isa=rv64gc --instructions=10000 -l --log-commits --log="$TRACE_FILE" "$OUTPUT_ELF"

echo "=== SUCCESS: Program exited normally ==="
echo ""
echo "Trace file: $TRACE_FILE"
echo "Disassembly: $DISASM_FILE"
echo ""
echo "The trace shows every instruction with register changes."
echo "Look for patterns like:"
echo "  add     a0, a0, a1      -> a0  0x000000000000012c  (300)"
echo "  slli    a0, a0, 2       -> a0  0x00000000000004b0  (1200)"
