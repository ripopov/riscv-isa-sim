#!/bin/bash
set -e

# Script to compile and run hello.c on Spike simulator (bare-metal version)

# Directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(git rev-parse --show-toplevel)"
BUILD_DIR="$ROOT_DIR/build"
OUTPUT_DIR="$SCRIPT_DIR"

# Files
SOURCE_FILE="$SCRIPT_DIR/hello_htif.c"
LINKER_SCRIPT="$SCRIPT_DIR/baremetal.ld"
OUTPUT_ELF="$OUTPUT_DIR/hello_htif.elf"
SPIKE_BIN="$BUILD_DIR/spike"

# Expected output from the program
EXPECTED_OUTPUT="Hello, world! (bare-metal with HTIF syscalls)"

echo "=== Compiling hello_htif.c ==="
echo "Source: $SOURCE_FILE"
echo "Linker script: $LINKER_SCRIPT"
echo "Output: $OUTPUT_ELF"

# Compile with riscv64-unknown-elf-gcc
# Using -march=rv64g for RV64 with IMAFD extensions
# -mabi=lp64d for 64-bit ABI with double-precision float
# -nostdlib: no standard library
# -nostartfiles: no crt0
# -T: use custom linker script
# -static: static linking
riscv64-unknown-elf-gcc \
    -march=rv64g \
    -mabi=lp64d \
    -nostdlib \
    -nostartfiles \
    -T "$LINKER_SCRIPT" \
    -static \
    -mcmodel=medany \
    -O2 \
    -o "$OUTPUT_ELF" \
    "$SOURCE_FILE"

echo "✓ Compilation successful"
echo ""

echo "=== Disassembling ELF ==="
DISASM_FILE="$OUTPUT_DIR/hello_htif.dis"
riscv64-unknown-elf-objdump -d "$OUTPUT_ELF" > "$DISASM_FILE"
echo "Disassembly saved to: $DISASM_FILE"
echo ""

echo "=== Running on Spike ==="
echo "Simulator: $SPIKE_BIN"

# Trace files
TRACE_FILE="$OUTPUT_DIR/hello_trace.log"
COMMIT_LOG_FILE="$OUTPUT_DIR/hello_commit.log"

# Run on Spike and capture output
# Using --isa=rv64g to match our compilation target
# -l enables execution trace
# --log-commits enables commit logging with register changes
# --log specifies the trace output file
OUTPUT=$("$SPIKE_BIN" --isa=rv64g -l --log-commits --log="$TRACE_FILE" "$OUTPUT_ELF" 2>&1)

# Also generate a separate commit-only log for comparison
"$SPIKE_BIN" --isa=rv64g --log-commits --log="$COMMIT_LOG_FILE" "$OUTPUT_ELF" > /dev/null 2>&1

echo "Instruction trace saved to: $TRACE_FILE"
echo "Commit log (with register changes) saved to: $COMMIT_LOG_FILE"

echo "Output:"
echo "$OUTPUT"
echo ""

echo "=== Verifying Output ==="
if echo "$OUTPUT" | grep -q "$EXPECTED_OUTPUT"; then
    echo "✓ SUCCESS: Output matches expected result!"
    echo "  Expected: $EXPECTED_OUTPUT"
    exit 0
else
    echo "✗ FAILED: Output does not match expected result!"
    echo "  Expected: $EXPECTED_OUTPUT"
    echo "  Got: $OUTPUT"
    exit 1
fi
