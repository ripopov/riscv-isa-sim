/*
 * Linker script for bare-metal RISC-V programs on Spike.
 *
 * A linker script tells the linker HOW and WHERE to place the various pieces
 * of your program (code, data, stack, etc.) in memory.  Without a linker
 * script the linker uses built-in defaults aimed at a hosted OS environment,
 * which won't work for bare-metal execution on Spike.
 *
 * Key syntax concepts used below:
 *
 *   .          The "location counter" — a built-in variable that tracks the
 *              current address being assigned.  Setting it (`. = 0x80000000`)
 *              moves the cursor; reading it (`_end = .`) captures the current
 *              address into a symbol.
 *
 *   SECTIONS   The top-level block that defines every output section and the
 *              order they appear in the final binary.
 *
 *   .name : { ... }
 *              Defines an output section called ".name".  Inside the braces
 *              you list which input sections (from object files) go into it.
 *
 *   *(.foo)    A wildcard pattern meaning "take the .foo section from every
 *              input object file and place it here".  The * matches all files.
 *
 *   ALIGN(n)   Rounds the location counter up to the next multiple of n.
 */

/* Tell the linker we are targeting the RISC-V architecture. */
OUTPUT_ARCH("riscv")

/*
 * ENTRY defines which symbol is the program's entry point.  The simulator
 * (or a debugger/loader) jumps to this address to begin execution.
 */
ENTRY(_start)

SECTIONS
{
    /*
     * Set the location counter to 0x80000000.  This is the first address of
     * DRAM on Spike's default memory map.  Everything that follows will be
     * placed starting here.
     */
    . = 0x80000000;

    /*
     * .text — executable code.
     *
     * .text.init is listed first so that _start (which lives in .text.init)
     * appears at the very beginning of the binary — exactly where the
     * simulator starts executing.
     */
    .text : {
        *(.text.init)   /* startup code — must come first */
        *(.text)        /* normal code */
        *(.text.*)      /* any compiler-generated subsections like .text.foo */
    }

    /*
     * .rodata — read-only data (string literals, const tables, etc.).
     * Placed right after code so it stays close and is easy to address.
     */
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    /*
     * .data — initialized read-write data.
     * This is where our input_a, input_b, and output arrays end up because
     * the assembly file puts them in ".section .data".
     */
    .data : {
        *(.data)
        *(.data.*)
    }

    /*
     * .bss — uninitialized (zero-initialized) data.
     * The linker records the size but does not store actual bytes in the
     * binary.  A real OS loader would zero this region at startup.
     * COMMON is an older C convention for tentative definitions.
     */
    .bss : {
        . = ALIGN(64);  /* align to a 64-byte boundary */
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }

    /*
     * .htif — Host-Target Interface.
     * Spike monitors the tohost/fromhost symbols for communication between
     * the simulated program and the host.  They must live in their own
     * section so the linker places them at a known, aligned address.
     */
    . = ALIGN(64);
    .htif : {
        *(.htif)
    }

    /*
     * Stack setup.
     *
     * _end marks where all program sections finish.  We then reserve
     * 0x20000 bytes (128 KiB) of empty space and place the _stack_top
     * symbol at the high end.  The stack grows DOWNWARD, so the program
     * sets sp = _stack_top and pushes subtract from sp toward _end.
     *
     *   _end                          _stack_top
     *    |<-------- 128 KiB ---------->|
     *    low address            high address
     *                  stack grows <---
     */
    . = ALIGN(16);      /* keep the stack 16-byte aligned (RISC-V ABI requirement) */
    _end = .;           /* symbol marking the end of all sections */
    . = . + 0x20000;    /* reserve 128 KiB for the stack */
    _stack_top = .;     /* the assembly code loads this into sp */
}
