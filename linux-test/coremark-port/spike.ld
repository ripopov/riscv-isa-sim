/* Bare-metal CoreMark on Spike: one flat image loaded at the base of DRAM.
   The front end finds the tohost/fromhost mailbox by symbol name, so the only
   thing this script has to guarantee is that the words land in ordinary
   memory. */
OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS
{
  . = 0x80000000;

  .text.init : { *(.text.init) }
  .text      : { *(.text .text.*) }

  . = ALIGN(0x1000);
  .tohost : { *(.tohost) }

  .rodata : { *(.rodata .rodata.*) *(.srodata .srodata.*) }

  .data : {
    . = ALIGN(8);
    /* Anchors the small-data area so the linker may relax accesses to it into
       gp-relative form; crt.S loads gp to match. */
    PROVIDE(__global_pointer$ = . + 0x800);
    *(.data .data.*) *(.sdata .sdata.*)
  }

  .bss (NOLOAD) : {
    _bss_start = .;
    *(.bss .bss.*) *(.sbss .sbss.*) *(COMMON)
    . = ALIGN(8);
    _bss_end = .;
  }

  . = ALIGN(16);
  . += 0x10000;
  _stack_top = .;
}
