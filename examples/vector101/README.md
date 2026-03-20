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

## Usage

```bash
./run_vector101.sh
```

## Requirements

- `riscv64-unknown-elf-gcc`
- `riscv64-unknown-elf-objdump`
- a built Spike binary in `../../build/spike`
