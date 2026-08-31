/* Platform glue for running CoreMark bare-metal on Spike.

   The only host services the benchmark needs are a character output and a way
   to stop the simulation, and Spike's front end already provides both over
   HTIF: the target leaves a request in "tohost", the front end takes it,
   zeroes the word and answers in "fromhost". */

#include <stdint.h>

/* The front end locates the mailbox by symbol name, and reads it eight bytes
   at a time, so both words must be naturally aligned and must not share a
   cache line with anything the compiler might reorder around them. */
volatile uint64_t tohost   __attribute__((section(".tohost"), aligned(64)));
volatile uint64_t fromhost __attribute__((section(".tohost"), aligned(64)));

static void
htif_send(uint64_t device, uint64_t command, uint64_t payload)
{
    while (tohost != 0) /* previous request not taken yet */
        ;
    /* Acknowledging the previous reply keeps the front end's response queue
       from growing one entry per character printed. */
    if (fromhost != 0)
        fromhost = 0;
    tohost = (device << 56) | (command << 48) | payload;
}

/* Device 1 is the blocking character device; command 1 writes one byte.
   CoreMark's ee_printf is redirected here. */
void
spike_putchar(char c)
{
    htif_send(1, 1, (uint8_t)c);
}

/* Device 0 is the syscall proxy, where an odd payload means "stop, and take
   the rest of the payload as the exit status". */
void
spike_exit(int code)
{
    htif_send(0, 0, ((uint64_t)code << 1) | 1);
    for (;;)
        ;
}
