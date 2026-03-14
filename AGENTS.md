# AGENTS.md

Canonical instructions for coding agents working in this repository.

If a tool expects an agent-specific file such as `CLAUDE.md`, treat this file as
the source of truth and keep the compatibility file thin.

## Project Overview

Spike is the official RISC-V ISA Simulator. It implements a functional model of
one or more RISC-V harts, supporting the full range of standard and many custom
extensions. It serves as the reference implementation for RISC-V and is used
for software development, architecture exploration, and compliance testing.

## Build Commands

```bash
# Configure and build (out-of-tree build recommended)
mkdir build && cd build
../configure --prefix=$RISCV
make -j$(nproc)
make install

# Run unit tests
make check

# Full distribution check
make distcheck
```

Requires C++20, Boost (ASIO, Regex), and `dtc` (device-tree-compiler). The
build system uses autoconf with a modular C++ build system (MCPPBS).

## Running Spike

```bash
spike [options] <ELF binary>
spike -d <ELF binary>            # interactive debug mode
spike -l <ELF binary>            # instruction trace log
spike --isa=rv64gcv <ELF binary> # ISA string override
spike -p4 <ELF binary>           # 4 harts
spike --extlib=<path.so>         # load extension shared library
```

## Architecture

### Subproject Build Order

`softfloat` -> `fdt` -> `fesvr` -> `disasm` -> `riscv` -> `customext` ->
`spike_main` / `spike_dasm`

### Core Components (`riscv/`)

- `sim.h/cc`: Top-level simulator. Orchestrates multiple harts, memory bus,
  devices (CLINT, PLIC), and the device tree. Runs an interleaved execution
  loop (5000-instruction quanta per hart).
- `processor.h/cc`: A single RISC-V hart. Holds architectural state
  (`state_t`): PC, integer/FP registers, CSRs, privilege level. Manages the
  instruction fetch->decode->execute loop with a fast path (icache) and a
  logged path (for tracing).
- `mmu.h/cc`: Memory management unit. TLB, page table walks (Sv32/39/48),
  instruction cache. All memory accesses (load/store/fetch) go through the
  MMU.
- `csrs.h/cc`: CSR definitions as classes. Each CSR type derives from `csr_t`
  with custom read/write behavior. Registered in `processor_t::csrmap`.
- `decode.h`: Instruction bit-field extraction. Defines `insn_t` wrapper and
  field accessors (rd, rs1, rs2, imm variants).
- `decode_macros.h`: Macros used inside instruction implementations: `RS1`,
  `RS2`, `WRITE_RD`, `MMU.load/store`, privilege checks, and similar helpers.
- `devices.h/cc`: Abstract device interface, memory bus (`bus_t`), ROM
  (`rom_device_t`), physical memory (`mem_t`).
- `extension.h`: Base class for custom extensions. Provides hooks to register
  instructions, disassembly, and CSRs.

### Instruction Implementations (`riscv/insns/`)

There are roughly 900 header files, one per instruction. Each file is a snippet
using macros from `decode_macros.h`. Example (`add.h`):

```cpp
WRITE_RD(sext_xlen(RS1 + RS2));
```

These snippets are `#include`d by generated functions via `insn_template.cc`,
which produces 8 variants per instruction (RV32I/RV64I/RV32E/RV64E x
fast/logged).

### Instruction Dispatch

Each instruction is described by `insn_desc_t` with a `match`/`mask` pair and 8
function pointers. The processor builds a hash table of these descriptors. The
fast path uses an instruction cache (`icache_entry_t`) that links directly to
the execution function.

### Front-End Server (`fesvr/`)

Host-Target Interface (HTIF) for communication between host and simulated
target. Handles ELF loading, syscall proxying, and the `tohost`/`fromhost`
communication protocol.

### Custom Extensions (`customext/`)

Example extensions demonstrating the plugin system:

- `cflush.cc`: Cache flush/discard instructions
- `dummy_rocc.cc`: RoCC accelerator interface example

Extensions are loaded as shared libraries via `--extlib=<path>` and registered
with `REGISTER_EXTENSION(name, constructor)`.

## Adding a New Instruction

1. Create `riscv/insns/<name>.h` with the implementation using macros from
   `decode_macros.h`.
2. Add opcode encoding to `riscv/opcodes.h` (`MATCH`/`MASK` defines and
   `DECLARE_INSN`).
3. Add the instruction name to `riscv/riscv.mk.in` in the `riscv_insn_list`
   variable.
4. Add disassembly support in `disasm/disasm.cc` if needed.
5. Register the instruction in the appropriate `processor.cc` section.

## CI

GitHub Actions runs on push to `master` and pull requests (Ubuntu 24.04 and
macOS 15). CI scripts are in `ci-tests/`:

- `build-spike`: builds with strict warnings
- `test-spike`: integration tests including conformance tests, vector extension
  tests, and custom extension tests
