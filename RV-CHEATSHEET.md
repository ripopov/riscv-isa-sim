# RISC-V Instruction Set Sheat Sheet

The most common is **RV64GC**, which breaks down as:

**G** = IMAFD ( the "general-purpose" bundle ):

| Letter   | Extension            | Description                            |
|----------|----------------------|----------------------------------------|
| I        | Base Integer         | Core ALU, load/stores, branches, jumps |
| M        | Multiply/Divide      | `mul`, `div`, `rem` and variants       |
| A        | Atomics              | lr/sc, amo* instructions               |
| F        | Single-Precision FP  | 32-bit float ops, f0-f31 registers     |
| D        | Double-Precision FP  | 64-bit float ops (extends F)           |

**C** = Compressed - 16-bit encodings for common instructions

So **RV64GC = RV64IMAFDC**.

This is what virtually all Linux-capable RISC-V chips target — SiFive U74, T-Head C910, SpacemiT K1, etc. It's also the target triple you'll see in toolchains: `riscv64-unknown-linux-gnu` defaults to `rv64gc` with `lp64d` ABI (hard-float, 64-bit doubles passed in FP registers).

Newer chips also add **V** (vector), **B** (bit manipulation, now split into Zba/Zbb/Zbs), and various **Z** sub-extensions, but `GC` remains the baseline expectation for application processors.

## RV64 ABI Registers

|ABI Name| Register| Description        | Preserved |
|--------|---------|--------------------|-----------|
| zero   | x0      | Hard-wired 0       | -         |
| ra     | x1      | Return Address     | No        |
| sp     | x2      | Stack Pointer      | Yes       |
| gp     | x3      | Global Pointer     | -         |
| tp     | x4      | Thread Pointer     | -         |
| t0     | x5      | Temporary          | No        |
| t1     | x6      | Temporary          | No        |
| t2     | x7      | Temporary          | No        |
| fp(s0) | x8      | Frame Pointer      | Yes       |
| s1     | x9      | Saved register     | Yes       |
| a0     | x10     | Function argument  | No        |
| a1     | x11     | Function argument  | No        |
| a2     | x12     | Function argument  | No        |
| a3     | x13     | Function argument  | No        |
| a4     | x14     | Function argument  | No        |
| a5     | x15     | Function argument  | No        |
| a6     | x16     | Function argument  | No        |
| a7     | x17     | Saved register     | Yes       |
| s2     | x18     | Saved register     | Yes       |
| s3     | x19     | Saved register     | Yes       |
| s4     | x20     | Saved register     | Yes       |
| s5     | x21     | Saved register     | Yes       |
| s6     | x22     | Saved register     | Yes       |
| s7     | x23     | Saved register     | Yes       |
| s8     | x24     | Saved register     | Yes       |
| s9     | x25     | Saved register     | Yes       |
| s10    | x26     | Saved register     | Yes       |
| s11    | x27     | Saved register     | Yes       |
| t3     | x28     | Temporary          | No        |
| t4     | x29     | Temporary          | No        |
| t5     | x30     | Temporary          | No        |
| t6     | x31     | Temporary          | No        |

- `ra` is the return address, conventionally used to hold the return address for function calls
- `sp`, stack pointer, is the address of the top of current stack frame. Stack grows downward in memory (toward lower addresses). To allocate stack space substract from `sp`.
- `gp`, global pointer, holds a fixed address in the middle of .sdata ( small data ), enabling efficient single-instruction access to global variables
- `tp`, thread pointer, holds the base address of the current thread's Thread Local Storage (TLS) block. 
- `fp`, frame pointer, holds the base address of current function's stack frame, providing stable reference point for accessing local variables. It stays fixed for the duration of function.

