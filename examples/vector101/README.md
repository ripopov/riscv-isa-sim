# Vector 101

Small RVV 1.0 introduction written in pure assembly.

This example is intentionally narrow:

- no C or C++
- no terminal output
- one short sequence of 22 documented instructions
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

## The `mstatus` Register

Before any vector (or floating-point) instruction can execute, software must
enable the corresponding extension state in the `mstatus` CSR (Machine Status,
address `0x300`).  In this example the very first instructions build the mask
`0x6600` and apply it with `csrs mstatus, t0`.

### Why `0x6600`?

The mask sets two 2-bit fields to `0b11` (Dirty):

| Mask bits  | Field | Bits   | Meaning when set to `0b11` |
|------------|-------|--------|---------------------------|
| `0x6000`   | FS    | 14:13  | Floating-point state = Dirty |
| `0x0600`   | VS    | 10:9   | Vector state = Dirty |

Both fields must be non-zero before the hart will accept FP or vector
instructions; otherwise every such instruction raises an illegal-instruction
exception.

### Complete `mstatus` bit layout (RV64)

```
 63    62       42 41   40   39   38  37  36  35:34 33:32
+----+-----------+----+----+----+----+---+---+-----+-----+
| SD |   WPRI    |MPELP|GVA| MPV|WPRI|MBE|SBE| SXL | UXL |
+----+-----------+----+----+----+----+---+---+-----+-----+

 31:23   22  21  20  19  18  17  16:15 14:13 12:11 10:9
+-------+---+---+---+---+---+----+-----+-----+-----+----+
| WPRI  |TSR| TW|TVM|MXR|SUM|MPRV| XS  | FS  | MPP | VS |
+-------+---+---+---+---+---+----+-----+-----+-----+----+

  8    7    6    5    4    3    2    1    0
+---+----+---+----+----+---+----+---+----+
|SPP|MPIE|UBE|SPIE|UPIE|MIE|WPRI|SIE|UIE |
+---+----+---+----+----+---+----+---+----+
```

### Field reference

| Bit(s) | Field | Description |
|--------|-------|-------------|
| 63     | SD    | State Dirty summary (read-only). `1` when any of FS, VS, or XS is Dirty. Placed at the sign bit so a single branch-on-negative can check it. |
| 41     | MPELP | M-mode Previous Expected Landing Pad (Zicfilp extension) |
| 40     | GVA   | Guest Virtual Address (H extension) |
| 39     | MPV   | Machine Previous Virtualization mode (H extension) |
| 37     | MBE   | M-mode Big-Endian (`0`=little, `1`=big) |
| 36     | SBE   | S-mode Big-Endian |
| 35:34  | SXL   | S-mode XLEN (`1`=32, `2`=64, `3`=128) |
| 33:32  | UXL   | U-mode XLEN |
| 22     | TSR   | Trap SRET -- `1` causes SRET in S-mode to trap |
| 21     | TW    | Timeout Wait -- `1` causes WFI in lower-privilege modes to trap |
| 20     | TVM   | Trap Virtual Memory -- `1` traps `satp` access and SFENCE.VMA in S-mode |
| 19     | MXR   | Make eXecutable Readable -- `1` allows loads from execute-only pages |
| 18     | SUM   | Permit Supervisor User Memory access |
| 17     | MPRV  | Modify PRiVilege -- `1` uses MPP instead of current privilege for loads/stores |
| 16:15  | XS    | Custom eXtension State (same encoding as FS/VS) |
| 14:13  | FS    | Floating-point State (see encoding table below) |
| 12:11  | MPP   | Machine Previous Privilege (`0`=U, `1`=S, `3`=M) |
| 10:9   | VS    | Vector extension State (see encoding table below) |
| 8      | SPP   | Supervisor Previous Privilege (`0`=U, `1`=S) |
| 7      | MPIE  | Machine Previous Interrupt Enable |
| 6      | UBE   | U-mode Big-Endian |
| 5      | SPIE  | Supervisor Previous Interrupt Enable |
| 3      | MIE   | Machine Interrupt Enable |
| 1      | SIE   | Supervisor Interrupt Enable |

### FS / VS / XS state encoding

All three 2-bit fields share the same encoding:

| Value | Name    | Meaning |
|-------|---------|---------|
| `00`  | Off     | Extension disabled. Every instruction that uses it raises an illegal-instruction exception. |
| `01`  | Initial | Enabled; state is in its initial (reset) condition. No save needed on context switch. |
| `10`  | Clean   | Enabled; state has been saved and not modified since. |
| `11`  | Dirty   | Enabled; state has potentially been modified and must be saved on context switch. |

Hardware only transitions these fields **toward** Dirty (i.e., any instruction
that modifies extension state sets the field to `11`).  Software (the OS) is
responsible for transitioning back to Clean or Initial after saving context.

### How the example uses `mstatus`

```asm
lui     t0, 0x6          # t0 = 0x6000 (FS = 0b11)
addiw   t0, t0, 1536     # t0 = 0x6600 (FS = 0b11, VS = 0b11)
csrs    mstatus, t0       # set bits → both fields become Dirty (enabled)
```

The `csrs` (CSR Set) instruction ORs the mask into `mstatus`, turning on FS and
VS without disturbing other fields.  After this point the hart accepts both
floating-point and vector instructions.

## Loading Addresses with `la` (the `auipc`+`addi` Pattern)

Throughout the program you will see lines like:

```asm
la      sp, _stack_top
la      a1, input_a
```

`la` (Load Address) is a **pseudo-instruction** — it is not a real RISC-V
hardware instruction.  The assembler silently expands each `la` into a pair of
real instructions.  Understanding this expansion is important because it reveals
how RISC-V handles a fundamental constraint: every instruction is exactly 32
bits wide, but a full memory address can be 32 or 64 bits.  There is simply no
room to embed a complete address inside a single instruction.

### The problem

Suppose the label `_stack_top` lives at address `0x8000_1234`.  You need to get
that value into the `sp` register.  A single 32-bit instruction cannot carry all
32 (let alone 64) address bits as an immediate operand — some bits are consumed
by the opcode, the destination register number, and other encoding fields.

### The solution: split the address into two halves

RISC-V solves this with a two-step approach.  The assembler rewrites every `la`
into:

```asm
auipc   sp, %pcrel_hi(_stack_top)   # step 1: upper 20 bits
addi    sp, sp, %pcrel_lo(label)     # step 2: lower 12 bits
```

Together the two instructions reconstruct the full **absolute** address of the
symbol.  The *mechanism* used to get there is called *PC-relative addressing*
because the offset encoded in the instructions is measured from the current
program counter (PC) — but the **end result in the register is an absolute
address** (absolute PC + fixed offset = absolute target address).

You might wonder: "the PC changes every instruction — how can this work?"  The
answer is that each `auipc` only reads **its own** PC at the moment **it**
executes.  The linker already knows the exact distance from that specific
`auipc` instruction to the target symbol, so it bakes the right offset into the
instruction bits at link time.  It does not matter that the PC was different one
instruction earlier or will be different one instruction later.

### Step-by-step walkthrough

#### Step 1 — `auipc` (Add Upper Immediate to PC)

`auipc rd, imm20` does the following:

1. Take the 20-bit immediate (`imm20`).
2. Shift it left by 12 positions to form a 32-bit value with 12 zero bits at
   the bottom.
3. Add the current PC.
4. Write the result into the destination register.

After this single instruction the register holds an address that is accurate to
within ±2048 bytes of the target (because the bottom 12 bits are still missing).

```
rd = PC + (imm20 << 12)
```

The `%pcrel_hi(symbol)` relocation tells the linker to compute the upper 20 bits
of the signed offset from the `auipc` site to the symbol.

#### Step 2 — `addi` (Add Immediate)

`addi rd, rd, imm12` simply adds the remaining 12-bit signed offset:

```
rd = rd + sign_extend(imm12)
```

The `%pcrel_lo(label)` relocation tells the linker to fill in the low 12 bits of
the same offset that was started by the `auipc` at `label`.

After both instructions execute, the register contains the exact runtime address
of the symbol.

### Concrete numeric example

Assume the `auipc` sits at address `0x8000_0000` and the target symbol is at
`0x8000_1234`:

| Step | Instruction | Computation | Register value |
|------|-------------|-------------|----------------|
| 1 | `auipc sp, 0x1` | `0x8000_0000 + (0x1 << 12)` | `0x8000_1000` |
| 2 | `addi sp, sp, 0x234` | `0x8000_1000 + 0x234` | `0x8000_1234` |

The offset from PC to symbol is `0x1234`.  The linker splits it:

- Upper 20 bits → `0x1` (used by `auipc`)
- Lower 12 bits → `0x234` (used by `addi`)

### Why PC-relative instead of absolute?

Position-independent code (PIC) never embeds fixed addresses — it always
computes addresses relative to where the code is currently running.  This means
the same binary works correctly no matter where it is loaded in memory.  Even in
a bare-metal program like this one, PC-relative addressing is the default
because it keeps the code simple and relocatable.

### Why not just use `la` everywhere?

You can — and this example does.  Writing the raw `auipc`/`addi` pair by hand is
only necessary when you need fine-grained control (for example, sharing one
`auipc` across multiple nearby `addi`/`lw`/`sw` instructions to save code
size).  For most code the `la` pseudo-instruction is clearer and the assembler
handles the details.

## Usage

```bash
./run_vector101.sh
```

## Requirements

- `riscv64-unknown-elf-gcc`
- `riscv64-unknown-elf-objdump`
- a built Spike binary in `../../build/spike`
