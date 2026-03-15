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

## RISC-V Instruction Set


### Terminology

- `imm`    - immediate value, normally sign extended
- `mem`    - memory
- `(p)`    - pseudoinstruction
- `pc`     - program counter
- `ra`     - return address (x1)
- `rd`     - destination register
- `rs1`    - first source register
- `rs2`    - second source register
- `symbol` - symbol, may be label in asm

### Insturction tables


**Arithmetic**

| Instr | Description                   | Use                    | Result                   |
|-------|-------------------------------|------------------------|--------------------------|
| add   | Add                           | add    rd, rs1, rs2    | rd = rs1 + rs2           |
| addi  | Add Immediate                 | addi   rd, rs1, imm    | rd = rs1 + imm           |
| neg   | Negate (p)                    | reg    rd,      rs2    | rd = -rs2                |
| sub   | Subtract                      | sub    rd, rs1, rs2    | rd = rs1 - rs2           |
| mul   | Multiply                      | mul    rd, rs1, rs2    | rd = (rs1 * rs2)[31:0]   |
| mulh  | Multiply High                 | mulh   rd, rs1, rs2    | rd = (rs1 * rs2)[63:32]  |
| mulhu | Multiply High Unsigned        | mulhu  rd, rs1, rs2    | rd = (rs1 * rs2)[63:32]  |
| mulsu | Multiply High Signed Unsigned | mulhsu rd, rs1, rs2    | rd = (rs1 * rs2)[63:32]  |
| div   | Divide                        | div    rd, rs1, rs2    | rd = rs1 / rs2           |
| rem   | Remainder                     | rem    rd, rs1, rs2    | rd = rs1 % rs2           |


Multiply and Divide require M extension.

**Bitwise logic**

| Instr | Description                   | Use                    | Result                   |
|-------|-------------------------------|------------------------|--------------------------|
| and   | AND                           | and    rd, rs1, rs2    | rd = rs1 & rs2           |
| andi  | AND immediate                 | andi   rd, rs1, imm    | rd = rs1 & imm           |
| not   | NOT (p)                       | not    rd, rs1         | rd = ~rs2                |
| or    | OR                            | or     rd, rs1, rs2    | rd = rs1 | rs2           |
| ori   | OR immediate                  | ori    rd, rs1, imm    | rd = rs1 | imm           |
| xor   | XOR                           | xor    rd, rs1, rs2    | rd = rs1 ^ rs2           |
| xori  | XOR immediate                 | xori   rd, rs1, imm    | rd = rs1 ^ imm           |

**Shift**

| Instr | Description                        | Use                    | Result              |
|-------|------------------------------------|------------------------|---------------------|
| sll   | Shift Left Logical                 | sll    rd, rs1, rs2    | rd = rs1 <<  rs2    |
| slli  | Shift Left Logical Immediate       | slli   rd, rs1, imm    | rd = rs1 <<  imm    |
| srl   | Shift Right Logical                | srl    rd, rs1, rs2    | rd = rs1 >>  rs2    |
| srli  | Shift Right Logical Immediate      | srli   rd, rs1, imm    | rd = rs1 >>  rs2    |
| sra   | Shift Right Arithmetic             | sra    rd, rs1, rs2    | rd = rs1 >>> imm    |
| srai  | Shift Right Arithmetic Immediate   | srai   rd, rs1, imm    | rd = rs1 >>> imm    |

**Load Immediate**

| Instr | Description                        | Use                    | Result                |
|-------|------------------------------------|------------------------|-----------------------|
| li    | Load Immediate (p)                 | li rd, imm             | rd = imm              |
| lui   | Load Upper Immediate               | lui rd, imm            | rd = imm << 12        |
| auipc | Add Upper Immediate to PC          | auipc rd, imm          | rd = pc + (imm << 12) |

**Load and Store**

| Instr | Description                        | Use                    | Result                   |
|-------|------------------------------------|------------------------|--------------------------|
| lw    | Load Word                          | lw  rd, imm(rs1)       | rd = mem[rs1+imm]        |
| lh    | Load Half                          | lh  rd, imm(rs1)       | rd = mem[rs1+imm][0:15]  |
| lhu   | Load Half Unsigned                 | lhu rd, imm(rs1)       | rd = mem[rs1+imm][0:15]  |
| lb    | Load Byte                          | lb  rd, imm(rs1)       | rd = mem[rs1+imm][0:7]   |
| lbu   | Load Byte Unsigned                 | lbu rd, imm(rs1)       | rd = mem[rs1+imm][0:7]   |
| la    | Load Symbol Address (p)            | la  rd, symbol         | rd = mem[rs1+imm][0:7]   |
| sw    | Store Word                         | sw  rs2, imm(rs1)      | mem[rs1+imm]       = rs2 |
| sh    | Store Half                         | sh  rs2, imm(rs1)      | mem[rs1+imm][0:15] = rs2 |
| sb    | Store Byte                         | sb  rs2, imm(rs1)      | mem[rs1+imm][0:7]  = rs2 |

**Jump and Function**

| Instr | Description                        | Use                    | Result                     |
|-------|------------------------------------|------------------------|----------------------------|
| j     | Jump (p)                           | j imm                  | pc += imm                  |
| jal   | Jump and Link                      | jal rd, imm            | rd = pc + 4; pc += imm     |
| jalr  | Jump and Link Register             | jalr rd, rs1, imm      | rd = pc + 4; pc = rs1+imm  |
| call  | Call Function (p)                  | call symbol            | ra = pc + 4; pc = &symbol  |
| ret   | Return from Function (p)           | ret                    | pc = ra                    |


**Branch**

| Instr | Description                        | Use                    | Result                     |
|-------|------------------------------------|------------------------|----------------------------|
| beq   | Branch Equal                       | beq  rs1, rs2, imm     | if(rs1 == rs2) pc += imm   |
| beqz  | Branch Equal Zero (p)              | beqz rs1, imm          | if(rs1 == 0)   pc += imm   |
| bne   | Branch Not Equal                   | bne  rs1, rs2, imm     | if(rs1 ≠ rs2)  pc += imm   |
| bnez  | Branch Not Equal Zero (p)          | bnez rs1, imm          | if(rs1 ≠ 0)    pc += imm   |
| blt   | Branch Less Than                   | blt  rs1, rs2, imm     | if(rs1 < rs2)  pc += imm   |
| bltu  | Branch Less Than Unsigned          | bltu rs1, rs2, imm     | if(rs1 < rs2)  pc += imm   |
| bltz  | Branch Less Than Zero (p)          | bltz rs1, imm          | if(rs1 < 0)    pc += imm   |
| bgt   | Branch Greater Than (p)            | bgt  rs1, rs2, imm     | if(rs1 > rs2)  pc += imm   |
| bgtu  | Branch Greater Than Unsigned (p)   | bgtu rs1, rs2, imm     | if(rs1 > rs2)  pc += imm   |
| bgtz  | Branch Greater Than Zero (p)       | bgtz rs1, imm          | if(rs1 > 0)    pc += imm   |
| ble   | Branch Less or Equal (p)           | ble rs1, rs2, imm      | if(rs1 ≤ rs2)  pc += imm   |
| bleu  | Branch Less or Equal Unsigned (p)  | bleu rs1, rs2, imm     | if(rs1 ≤ rs2)  pc += imm   |
| blez  | Branch Less or Equal Zero (p)      | blez rs1, imm          | if(rs1 ≤ 0)    pc += imm   |
| bge   | Branch Greater or Equal            | bge rs1, rs2, imm      | if(rs1 ≥ rs2)  pc += imm   |
| bgeu  | Branch Greater or Equal Unsigned   | bgeu rs1, rs2, imm     | if(rs1 ≥ rs2)  pc += imm   |
| bgez  | Branch Greater or Equal Zero (p)   | bgez rs1, imm          | if(rs1 ≥ 0)    pc += imm   |

Label can be used in place of branch immediate, for example: `beq t0, t1, label_name`

**Set**

| Instr | Description                        | Use                    | Result                     |
|-------|------------------------------------|------------------------|----------------------------|
| slt   | Set Less Than                      | slt  rd, rs1, rs2      | rd = (rs1 < rs2 )          |
| slti  | Set Less Than Immediate            | slti rd, rs1, imm      | rd = (rs1 < imm )          |
| sltu  | Set Less Than Unsigned             | sltu rd, rs1, rs2      | rd = (rs1 < rs2 )          |
| sltiu | Set Less Than Immediate Unsigned   | sltui rd, rs1, imm     | rd = (rs1 < imm )          |
| seqz  | Set Equal Zero (p)                 | seqz rd, rs1           | rd = (rs1 == 0 )           |
| snez  | Set Not Equal Zero (p)             | snez rd, rs1           | rd = (rs1 ≠ rs2 )          |
| sltz  | Set Less Than Zero (p)             | sltz rd, rs1           | rd = (rs1 < 0 )            |
| sgtz  | Set Greater Than Zero (p)          | sgtz rd, rs1           | rd = (rs1 > 0 )            |

**Counters**

| Instr      | Description                        | Use               | Result                  |
|------------|------------------------------------|-------------------|-------------------------|
| rdcycle    | CPU Cycle Count (p)                | rdcycle rd        | rd = csr_cycle[31:0]    |
| rdcycleh   | CPU Cycle Count High (p)           | rdcycleh rd       | rd = csr_cycle[63:32]   |
| rdtime     | Current Time (p)                   | rdtime rd         | rd = csr_time[31:0]     |
| rdtimeh    | Current Time High (p)              | rdtimeh rd        | rd = csr_time[63:32]    |
| rdinstret  | CPU Instructions Retired (p)       | rdinstret rd      | rd = csr_instret[31:0]  |
| rdinstreth | CPU Instructions Retired High (p)  | rdinstreth rd     | rd = csr_instret[63:32] |


