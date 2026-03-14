# Vector 101

Small RVV 1.0 introduction written in pure assembly.

This example is intentionally narrow:

- no C or C++
- no terminal output
- one short sequence of 27 documented instructions
- verification based on Spike's instruction trace and register dumps

## Files

- `vector101.S` - assembly program with instruction-by-instruction commentary
- `baremetal.ld` - linker script for Spike bare-metal execution
- `run_vector101.sh` - build, run, and trace verification script
- `vector101.elf` - generated ELF image
- `vector101.dis` - generated disassembly
- `vector101_trace.log` - generated Spike trace with commit data

## What It Demonstrates

The program configures `VL=8` for `e32,m1`, loads two vectors, performs a
vector add, adds an immediate, multiplies by a scalar, reduces the final vector
to one scalar sum, stores the transformed vector to memory, and exits through
HTIF without printing anything.

The verified values are:

- `v1 = [1, 2, 3, 4, 5, 6, 7, 8]`
- `v2 = [10, 20, 30, 40, 50, 60, 70, 80]`
- `v3 = [11, 22, 33, 44, 55, 66, 77, 88]`
- `v4 = [16, 27, 38, 49, 60, 71, 82, 93]`
- `v5 = [32, 54, 76, 98, 120, 142, 164, 186]`
- `a4 = 872`

## Usage

```bash
./run_vector101.sh
```

## Requirements

- `riscv64-unknown-elf-gcc`
- `riscv64-unknown-elf-objdump`
- a built Spike binary in `../../build/spike`
