/* Linker script for bare-metal RISC-V programs on Spike */

OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS
{
    /* Start at default Spike load address */
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
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }

    /* HTIF communication region */
    .htif : {
        *(.htif)
    }

    /* Stack grows downward from here */
    . = ALIGN(16);
    _end = .;
    . = . + 0x10000; /* 64KB stack */
    _stack_top = .;
}
