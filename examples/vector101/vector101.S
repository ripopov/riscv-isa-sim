/*
 * vector101.S
 * ===========
 *
 * Minimal RVV 1.0 introduction for Spike.
 *
 * Goals:
 * - pure assembly only
 * - no terminal output
 * - short, trace-friendly program
 * - every instruction is documented in place
 *
 * The program performs one small vector workflow:
 *   input_a = [ 1,  2,  3,  4,  5,  6,  7,  8]
 *   input_b = [10, 20, 30, 40, 50, 60, 70, 80]
 *
 *   v3 = v1 + v2
 *   v4 = v3 + 5
 *   v5 = v4 * 2
 *   a4 = sum(v5) = 872 (0x368)
 *
 * The companion run script verifies the expected register dumps in Spike's
 * instruction trace, so this source intentionally does not print anything.
 */

    .option norvc

    .section .text.init
    .globl _start

_start:
    # 1. Build the upper part of the mstatus enable mask (0x6000).
    lui     t0, 0x6

    # 2. Add the low bits so the final mask becomes 0x6600.
    addiw   t0, t0, 1536

    # 3. Set mstatus.FS and mstatus.VS before executing vector code.
    csrs    mstatus, t0

    # 4. Set up the stack pointer (la expands to auipc+addi for PC-relative addressing).
    la      sp, _stack_top

    # 5. Configure RVV for 8 elements of 32-bit integers in one register group.
    vsetivli t1, 8, e32, m1, ta, ma

    # 6. Load address of input_a (la expands to auipc+addi).
    la      a1, input_a

    # 7. Load eight 32-bit elements from input_a into v1.
    vle32.v v1, (a1)

    # 8. Load address of input_b (la expands to auipc+addi).
    la      a2, input_b

    # 9. Load eight 32-bit elements from input_b into v2.
    vle32.v v2, (a2)

    # 10. Add the two source vectors lane by lane: v3[i] = v1[i] + v2[i].
    vadd.vv v3, v1, v2

    # 11. Add the immediate value 5 to every lane: v4[i] = v3[i] + 5.
    vadd.vi v4, v3, 5

    # 12. Load the scalar multiplier 2 into a general-purpose register.
    addi    a3, zero, 2

    # 13. Multiply every lane by that scalar: v5[i] = v4[i] * 2.
    vmul.vx v5, v4, a3

    # 14. Create a zero seed vector so the reduction starts from 0.
    vmv.v.i v6, 0

    # 15. Reduce v5 to one sum in element 0 of v7.
    vredsum.vs v7, v5, v6

    # 16. Copy the reduced scalar from v7[0] into integer register a4.
    vmv.x.s a4, v7

    # 17. Load address of output buffer (la expands to auipc+addi).
    la      a5, output

    # 18. Store the transformed vector to memory for post-run inspection.
    vse32.v v5, (a5)

    # 19. Prepare HTIF exit code 0 encoded as the value 1.
    addi    a0, zero, 1

    # 20. Load address of tohost (la expands to auipc+addi).
    la      t0, tohost

    # 21. Signal Spike that the program completed successfully.
    sd      a0, 0(t0)

hang:
    # 22. Stay parked locally in case the host observes the exit asynchronously.
    j       hang

    .section .data
    .align  6
input_a:
    .word   1, 2, 3, 4, 5, 6, 7, 8

    .align  6
input_b:
    .word   10, 20, 30, 40, 50, 60, 70, 80

    .align  6
output:
    .zero   32

    .section .htif
    .align  6
    .globl  tohost
    .globl  fromhost
tohost:
    .dword  0
fromhost:
    .dword  0
