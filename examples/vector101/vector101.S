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

    # 4. Form the PC-relative high part of the reserved stack address.
1:  auipc   sp, %pcrel_hi(_stack_top)

    # 5. Complete the stack pointer so normal stores and spills are safe.
    addi    sp, sp, %pcrel_lo(1b)

    # 6. Configure RVV for 8 elements of 32-bit integers in one register group.
    vsetivli t1, 8, e32, m1, ta, ma

    # 7. Form the PC-relative high part of the first input array address.
2:  auipc   a1, %pcrel_hi(input_a)

    # 8. Complete the address of input_a.
    addi    a1, a1, %pcrel_lo(2b)

    # 9. Load eight 32-bit elements from input_a into v1.
    vle32.v v1, (a1)

    # 10. Form the PC-relative high part of the second input array address.
3:  auipc   a2, %pcrel_hi(input_b)

    # 11. Complete the address of input_b.
    addi    a2, a2, %pcrel_lo(3b)

    # 12. Load eight 32-bit elements from input_b into v2.
    vle32.v v2, (a2)

    # 13. Add the two source vectors lane by lane: v3[i] = v1[i] + v2[i].
    vadd.vv v3, v1, v2

    # 14. Add the immediate value 5 to every lane: v4[i] = v3[i] + 5.
    vadd.vi v4, v3, 5

    # 15. Load the scalar multiplier 2 into a general-purpose register.
    addi    a3, zero, 2

    # 16. Multiply every lane by that scalar: v5[i] = v4[i] * 2.
    vmul.vx v5, v4, a3

    # 17. Create a zero seed vector so the reduction starts from 0.
    vmv.v.i v6, 0

    # 18. Reduce v5 to one sum in element 0 of v7.
    vredsum.vs v7, v5, v6

    # 19. Copy the reduced scalar from v7[0] into integer register a4.
    vmv.x.s a4, v7

    # 20. Form the PC-relative high part of the output buffer address.
4:  auipc   a5, %pcrel_hi(output)

    # 21. Complete the address of output.
    addi    a5, a5, %pcrel_lo(4b)

    # 22. Store the transformed vector to memory for post-run inspection.
    vse32.v v5, (a5)

    # 23. Prepare HTIF exit code 0 encoded as the value 1.
    addi    a0, zero, 1

    # 24. Form the PC-relative high part of the tohost symbol.
5:  auipc   t0, %pcrel_hi(tohost)

    # 25. Complete the address of tohost.
    addi    t0, t0, %pcrel_lo(5b)

    # 26. Signal Spike that the program completed successfully.
    sd      a0, 0(t0)

hang:
    # 27. Stay parked locally in case the host observes the exit asynchronously.
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
