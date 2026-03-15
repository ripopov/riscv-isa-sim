# Hello ASM

Minimal RISC-V assembly scratchpad for Spike.

This example demonstrates basic RISC-V instructions and function calls in pure assembly. It's designed as a playground for experimenting with instructions—watch the instruction trace to see register values change after each instruction.

## Files

- `hello_asm.S` - assembly program with instruction-by-instruction commentary
- `baremetal.ld` - linker script for Spike bare-metal execution
- `run_hello_asm.sh` - build and run script with trace output
- `hello_asm.elf` - generated ELF image
- `hello_asm.dis` - generated disassembly
- `hello_asm_trace.log` - Spike trace with commit data

## What It Demonstrates

The program performs a simple computation:

1. Sets up stack pointer
2. Loads test values: `a=100`, `b=200`
3. Calls `compute_sum(100, 200)` which returns `300`
4. Stores result to memory
5. Calls `multiply_by_4(300)` which returns `1200`
6. Stores final result and exits

### Instructions Used

- **Arithmetic**: `add`, `addi`, `slli`
- **Memory**: `lw`, `sw`, `ld`, `sd`
- **Control Flow**: `jal`, `ret` (pseudo for `jalr x0, ra, 0`)
- **Addressing**: `auipc` + `addi` for PC-relative loads
- **Function Calls**: stack frame setup/teardown with `sp`/`ra`/`s0`

### Expected Register Values

From the trace, you should see:
```
a0 = 0x64 (100)   after loading test_data_a
a1 = 0xC8 (200)   after loading test_data_b
a0 = 0x12C (300)  after compute_sum returns
a0 = 0x4B0 (1200) after multiply_by_4 returns
```

## Usage

```bash
./run_hello_asm.sh
```

This will:
1. Compile the assembly to an ELF binary
2. Generate a disassembly listing
3. Run on Spike with full instruction trace (`-l --log-commits`)
4. Save the trace to `hello_asm_trace.log`

### Viewing the Trace

```bash
# View the full trace
cat hello_asm_trace.log

# Search for specific instructions
grep "add     a0, a0, a1" hello_asm_trace.log
grep "slli    a0, a0, 2" hello_asm_trace.log

# See just the register commits
grep "core.*x" hello_asm_trace.log
```

## Requirements

- `riscv64-unknown-elf-gcc`
- `riscv64-unknown-elf-objdump`
- Built Spike binary in `../../build/spike`

## Modifying the Example

This is a scratchpad—feel free to edit `hello_asm.S` and experiment:

- Change `test_data_a` and `test_data_b` values
- Add more arithmetic operations
- Create additional functions
- Try different shift amounts or multiply by other constants
- Add conditional branches or loops

Re-run `./run_hello_asm.sh` to see your changes in the trace.
