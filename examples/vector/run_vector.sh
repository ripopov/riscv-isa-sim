#!/bin/bash
set -e

# Script to compile and run the RISC-V Vector Extension demo on Spike simulator

# Directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(git rev-parse --show-toplevel)"
BUILD_DIR="$ROOT_DIR/build"
OUTPUT_DIR="$SCRIPT_DIR"

# Files
SOURCE_FILE="$SCRIPT_DIR/vector_demo.cpp"
LINKER_SCRIPT="$SCRIPT_DIR/baremetal.ld"
OUTPUT_ELF="$OUTPUT_DIR/vector_demo.elf"
SPIKE_BIN="$BUILD_DIR/spike"

echo "=== Compiling vector_demo.cpp ==="
echo "Source: $SOURCE_FILE"
echo "Linker script: $LINKER_SCRIPT"
echo "Output: $OUTPUT_ELF"

# Compile with vector extension enabled
# -march=rv64gcv_zvl256b: RV64 with G (IMAFD) + C (compressed) + V (vector)
#   + zvl256b (min VLEN 256 bits) extensions
# -mabi=lp64d: 64-bit ABI with double-precision float
# -nostdlib: no standard library
# -nostartfiles: no crt0
# -T: use custom linker script
# -static: static linking
riscv64-unknown-elf-g++ \
    -march=rv64gcv_zvl256b \
    -mabi=lp64d \
    -nostdlib \
    -nostartfiles \
    -T "$LINKER_SCRIPT" \
    -static \
    -mcmodel=medany \
    -O2 \
    -fno-builtin \
    -o "$OUTPUT_ELF" \
    "$SOURCE_FILE"

echo "Compilation successful"
echo ""

echo "=== Disassembling ELF ==="
DISASM_FILE="$OUTPUT_DIR/vector_demo.dis"
riscv64-unknown-elf-objdump -d "$OUTPUT_ELF" > "$DISASM_FILE"
echo "Disassembly saved to: $DISASM_FILE"
echo ""

echo "=== Running on Spike ==="
echo "Simulator: $SPIKE_BIN"

# Run on Spike with vector extension support
# --isa=rv64gcv_zvl256b: RV64 with G+C+V extensions, VLEN >= 256 bits
# This gives VL=8 for e32/m1, which matches the test expectations.
# The zvl256b extension guarantees a minimum vector register length of 256 bits.
OUTPUT=$("$SPIKE_BIN" --isa=rv64gcv_zvl256b "$OUTPUT_ELF" 2>&1)

echo "$OUTPUT"
echo ""

echo "=== Verifying Output ==="
if echo "$OUTPUT" | grep -q "ALL TESTS PASSED"; then
    echo "SUCCESS: All vector extension tests passed!"
    exit 0
else
    echo "FAILED: Some tests did not pass!"
    echo "$OUTPUT"
    exit 1
fi
