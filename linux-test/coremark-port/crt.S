# Entry point for the bare-metal CoreMark image.  Spike's boot ROM enters here
# in M-mode with nothing set up, so this establishes the C environment and
# hands the exit status back through HTIF.

  .section .text.init, "ax", @progbits
  .globl _start
_start:
  # Spike is run with one hart, but park any others rather than let them race
  # through main.
  csrr  t0, mhartid
  bnez  t0, park

.option push
.option norelax
  la    gp, __global_pointer$
.option pop
  la    sp, _stack_top

  # mstatus.FS comes up Off; CoreMark's result printing uses the FPU.
  li    t0, 0x6000              # FS = Dirty
  csrs  mstatus, t0

  la    t0, _bss_start
  la    t1, _bss_end
1:
  bgeu  t0, t1, 2f
  sd    zero, 0(t0)
  addi  t0, t0, 8
  j     1b
2:
  call  main
  call  spike_exit              # exit status is main's return value in a0

park:
  wfi
  j     park
