// Bare-metal hello world using HTIF syscalls for RISC-V Spike simulator

// HTIF protocol symbols - Spike looks for these
volatile unsigned long tohost __attribute__((section(".htif")));
volatile unsigned long fromhost __attribute__((section(".htif")));

// Magic memory region for syscalls
static volatile unsigned long syscall_mem[8] __attribute__((aligned(64)));

// HTIF protocol functions
static void htif_syscall(unsigned long n, unsigned long a0, unsigned long a1, unsigned long a2)
{
    syscall_mem[0] = n;     // syscall number
    syscall_mem[1] = a0;    // arg0
    syscall_mem[2] = a1;    // arg1
    syscall_mem[3] = a2;    // arg2
    syscall_mem[4] = 0;     // arg3
    syscall_mem[5] = 0;     // arg4
    syscall_mem[6] = 0;     // arg5
    syscall_mem[7] = 0;     // arg6

    // Send syscall command to HTIF
    // Device = 0 (syscall), Cmd = 0, Payload = address of syscall_mem
    tohost = ((unsigned long)syscall_mem);

    // Wait for response
    while (fromhost == 0);
    fromhost = 0;
}

static void htif_exit(int code)
{
    // HTIF exit command: payload = (code << 1) | 1
    tohost = (code << 1) | 1;
    while (1); // Hang
}

// Syscall numbers (from Linux RISC-V ABI)
#define SYS_write 64

// Write string to stdout
static void print(const char* s)
{
    int len = 0;
    while (s[len]) len++;

    // sys_write(fd=1, buf=s, len=len)
    htif_syscall(SYS_write, 1, (unsigned long)s, len);
}

void _start(void)
{
    print("Hello, world! (bare-metal with HTIF syscalls)\n");
    htif_exit(0);
}
