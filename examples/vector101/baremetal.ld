/* Linker script for bare-metal RISC-V programs on Spike */

OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS
{
    /* Start at Spike's default load address. */
    . = 0x80000000;

    .text : {
        *(.text.init)
        *(.text)
        *(.text.*)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
        *(.data.*)
    }

    .bss : {
        . = ALIGN(64);
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }

    . = ALIGN(64);
    .htif : {
        *(.htif)
    }

    . = ALIGN(16);
    _end = .;
    . = . + 0x20000;
    _stack_top = .;
}
