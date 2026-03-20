/*
 * hello_asm.S
 * ===========
 *
 * Minimal RISC-V assembly example for Spike.
 *
 * Goals:
 * - pure assembly only
 * - no terminal output
 * - short, trace-friendly program
 * - demonstrates basic instructions and function calls
 *
 * This is a scratchpad example for experimenting with RISC-V instructions.
 * Watch the instruction trace to see register changes.
 */

    .option norvc

    .section .text.init
    .globl _start

_start:
    # 0. Install trap handler so unexpected exceptions exit with failure.
    la      t0, trap_handler
    csrw    mtvec, t0

    rdcycle t0

    # 1. Set up the stack pointer using PC-relative addressing.
1:  auipc   sp, %pcrel_hi(_stack_top)
    addi    sp, sp, %pcrel_lo(1b)

    # 2. Load test data into argument registers.
2:  auipc   a0, %pcrel_hi(test_data_a)
    addi    a0, a0, %pcrel_lo(2b)
    lw      a0, 0(a0)           # a0 = 100 (0x64)

3:  auipc   a1, %pcrel_hi(test_data_b)
    addi    a1, a1, %pcrel_lo(3b)
    lw      a1, 0(a1)           # a1 = 200 (0xC8)

    # 3. Call compute_sum(a0, a1) -> a0 = a0 + a1
    jal     ra, compute_sum

    # 4. Store result to memory (demonstrates SW).
4:  auipc   t0, %pcrel_hi(result)
    addi    t0, t0, %pcrel_lo(4b)
    sw      a0, 0(t0)           # result = 300 (0x12C)

    # 5. Call multiply_by_4(a0) -> a0 = a0 * 4
    jal     ra, multiply_by_4

    # 6. Store final result.
5:  auipc   t0, %pcrel_hi(final_result)
    addi    t0, t0, %pcrel_lo(5b)
    sw      a0, 0(t0)           # final_result = 1200 (0x4B0)

    # 7. HTIF exit: signal success to Spike.
6:  auipc   t0, %pcrel_hi(tohost)
    addi    t0, t0, %pcrel_lo(6b)
    li      a0, 1               # Exit code 0 is encoded as 1
    sd      a0, 0(t0)

hang:
    # 8. Loop forever waiting for host to observe exit.
    j       hang

#
# trap_handler: catch unexpected exceptions and exit with failure.
#
# mtvec requires the handler address to be aligned to a 4-byte boundary.
# .align 2 means 2^2 = 4-byte alignment, satisfying that requirement.
#
    .align  2
trap_handler:
    la      t0, tohost
    # HTIF exit protocol: tohost = (exit_code << 1) | 1.
    # exit_code 0 = success (tohost=1), exit_code 1 = failure (tohost=3).
    li      t1, 3
    sd      t1, 0(t0)           # Write failure code to HTIF tohost register.
                                # Spike polls tohost and terminates the simulation.
1:  j       1b                  # Spin until Spike observes tohost and shuts down.

#
# compute_sum(int a, int b) -> int
# Arguments: a0, a1
# Returns: a0 = a + b
# Clobbers: none (preserves callee-saved registers)
#
compute_sum:
    # Prologue: save return address on stack.
    addi    sp, sp, -16
    sd      ra, 8(sp)
    sd      s0, 0(sp)

    # Function body: add the two arguments.
    mv      s0, a0              # Save a0 to s0 (callee-saved)
    add     a0, a0, a1          # a0 = a0 + a1

    # Epilogue: restore registers and return.
    ld      ra, 8(sp)
    ld      s0, 0(sp)
    addi    sp, sp, 16
    ret

#
# multiply_by_4(int x) -> int
# Arguments: a0
# Returns: a0 = x * 4
# Uses shift-left for efficient multiplication.
#
multiply_by_4:
    # Prologue.
    addi    sp, sp, -8
    sd      ra, 0(sp)

    # Function body: multiply by 4 using SLLI.
    slli    a0, a0, 2           # a0 = a0 << 2 = a0 * 4

    # Epilogue.
    ld      ra, 0(sp)
    addi    sp, sp, 8
    ret

    .section .data
    .align  4
test_data_a:
    .word   100                 # First input value

    .align  4
test_data_b:
    .word   200                 # Second input value

    .align  4
result:
    .word   0                   # Intermediate result storage

    .align  4
final_result:
    .word   0                   # Final result storage

    .section .htif
    .align  6
    .globl  tohost
    .globl  fromhost
tohost:
    .dword  0
fromhost:
    .dword  0
