#!/bin/bash
set -euo pipefail

# Build, run, and verify the minimal RVV assembly example on Spike.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(git rev-parse --show-toplevel)"
BUILD_DIR="$ROOT_DIR/build"

SOURCE_FILE="$SCRIPT_DIR/vector101.S"
LINKER_SCRIPT="$SCRIPT_DIR/baremetal.ld"
OUTPUT_ELF="$SCRIPT_DIR/vector101.elf"
DISASM_FILE="$SCRIPT_DIR/vector101.dis"
TRACE_FILE="$SCRIPT_DIR/vector101_trace.log"
SPIKE_BIN="$BUILD_DIR/spike"

expect_commit_after() {
    local description="$1"
    local instruction_text="$2"
    local commit_text="$3"

    if awk -v instruction_text="$instruction_text" -v commit_text="$commit_text" '
        index($0, instruction_text) { waiting = 1; next }
        waiting && index($0, commit_text) { matched = 1; exit 0 }
        waiting && $0 ~ /^core/ { waiting = 0 }
        END { exit matched ? 0 : 1 }
    ' "$TRACE_FILE"; then
        echo "PASS: $description"
    else
        echo "FAIL: $description"
        echo "  instruction text: $instruction_text"
        echo "  commit text:      $commit_text"
        exit 1
    fi
}

echo "=== Compiling vector101.S ==="
riscv64-unknown-elf-gcc \
    -march=rv64gv_zvl256b \
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

echo "=== Running on Spike ==="
SPIKE_OUTPUT=$("$SPIKE_BIN" --isa=rv64gv_zvl256b -l --log-commits --log="$TRACE_FILE" "$OUTPUT_ELF" 2>&1)

if [[ -n "$SPIKE_OUTPUT" ]]; then
    echo "FAIL: example produced terminal output, but it should be silent"
    printf '%s\n' "$SPIKE_OUTPUT"
    exit 1
fi

echo "=== Verifying Trace ==="
expect_commit_after \
    "vsetivli configures VL=8" \
    "vsetivli t1, 8, e32, m1, ta, ma" \
    "x6  0x0000000000000008"

expect_commit_after \
    "v1 receives input_a" \
    "vle32.v v1, (a1)" \
    "v1  0x0000000800000007000000060000000500000004000000030000000200000001"

expect_commit_after \
    "v2 receives input_b" \
    "vle32.v v2, (a2)" \
    "v2  0x00000050000000460000003c00000032000000280000001e000000140000000a"

expect_commit_after \
    "v3 contains v1 + v2" \
    "vadd.vv v3, v1, v2" \
    "v3  0x000000580000004d00000042000000370000002c00000021000000160000000b"

expect_commit_after \
    "v4 contains v3 + 5" \
    "vadd.vi v4, v3, 5" \
    "v4  0x0000005d00000052000000470000003c00000031000000260000001b00000010"

expect_commit_after \
    "v5 contains v4 * 2" \
    "vmul.vx v5, v4, a3" \
    "v5  0x000000ba000000a40000008e00000078000000620000004c0000003600000020"

expect_commit_after \
    "v6 is a zero seed vector" \
    "vmv.v.i v6, 0" \
    "v6  0x0000000000000000000000000000000000000000000000000000000000000000"

expect_commit_after \
    "a4 receives the reduced sum 872" \
    "vmv.x.s a4, v7" \
    "x14 0x0000000000000368"

echo "SUCCESS: trace values match expected RVV results"
