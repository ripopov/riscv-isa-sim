/*
 * RISC-V Vector Extension (RVV 1.0) Comprehensive Demonstration
 * ==============================================================
 *
 * This bare-metal program demonstrates the RISC-V Vector extension
 * instructions running on the Spike simulator. It covers all major
 * categories of vector ISA instructions:
 *
 *   1.  Vector Configuration (vsetvli, vsetivli)
 *   2.  Vector Loads & Stores (unit-stride, strided, indexed, mask,
 *       whole-register, fault-only-first)
 *   3.  Integer Arithmetic (add, sub, widening, saturating, multiply,
 *       divide, multiply-accumulate)
 *   4.  Bitwise & Logical (and, or, xor)
 *   5.  Shift Operations (sll, srl, sra, narrowing shifts)
 *   6.  Integer Comparisons (eq, ne, lt, le, gt, producing mask results)
 *   7.  Integer Min/Max & Absolute Value
 *   8.  Integer Merge & Move
 *   9.  Sign/Zero Extension & Narrowing
 *  10.  Reduction Operations (sum, min, max, and, or, xor, widening sum)
 *  11.  Mask Operations (logical, population count, find-first, iota)
 *  12.  Permutation (gather, slide, compress)
 *  13.  Carry/Borrow arithmetic (adc, sbc)
 *  14.  Fixed-Point (averaging add/sub, saturating multiply-add)
 *  15.  Floating-Point Arithmetic (add, sub, mul, div, sqrt, fma)
 *  16.  FP Comparison, Min/Max, Sign-Injection, Classification
 *  17.  FP Conversion & Widening/Narrowing
 *  18.  FP Reductions (ordered/unordered sum, min, max)
 *  19.  Whole-Register Move
 *
 * Each test section writes a pass/fail status. At the end, the overall
 * result is printed via HTIF syscalls.
 *
 * Build: riscv64-unknown-elf-gcc -march=rv64gcv_zvl256b -mabi=lp64d
 *        -nostdlib -nostartfiles -T baremetal.ld -static -mcmodel=medany
 *        -O2 -fno-builtin -o vector_demo.elf vector_demo.c
 *
 * Run:   spike --isa=rv64gcv_zvl256b vector_demo.elf
 */

#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  HTIF (Host-Target Interface) for bare-metal I/O on Spike          */
/* ------------------------------------------------------------------ */

volatile unsigned long tohost   __attribute__((section(".htif")));
volatile unsigned long fromhost __attribute__((section(".htif")));

static volatile unsigned long syscall_mem[8] __attribute__((aligned(64)));

#define SYS_write 64
#define SYS_exit  93

static void htif_syscall(unsigned long n,
                         unsigned long a0, unsigned long a1,
                         unsigned long a2, unsigned long a3,
                         unsigned long a4, unsigned long a5)
{
    syscall_mem[0] = n;
    syscall_mem[1] = a0;
    syscall_mem[2] = a1;
    syscall_mem[3] = a2;
    syscall_mem[4] = a3;
    syscall_mem[5] = a4;
    syscall_mem[6] = a5;
    syscall_mem[7] = 0;

    tohost = (unsigned long)syscall_mem;
    while (fromhost == 0)
        ;
    fromhost = 0;
}

static void htif_exit(int code)
{
    tohost = ((unsigned long)code << 1) | 1;
    while (1)
        ;
}

static void print(const char *s)
{
    unsigned long len = 0;
    while (s[len]) len++;
    htif_syscall(SYS_write, 1, (unsigned long)s, len, 0, 0, 0);
}

/* Minimal integer-to-string (decimal, signed) */
static char num_buf[24];
static const char *itoa(long v)
{
    char *p = num_buf + sizeof(num_buf) - 1;
    *p = '\0';
    int neg = 0;
    unsigned long u;
    if (v < 0) { neg = 1; u = (unsigned long)(-v); }
    else        { u = (unsigned long)v; }
    if (u == 0) { *--p = '0'; }
    else {
        while (u) { *--p = '0' + (u % 10); u /= 10; }
    }
    if (neg) *--p = '-';
    return p;
}

/* Hex-to-string for unsigned long */
static char hex_buf[20];
static const char *utohex(unsigned long v)
{
    const char *digits = "0123456789abcdef";
    char *p = hex_buf + sizeof(hex_buf) - 1;
    *p = '\0';
    if (v == 0) { *--p = '0'; }
    else {
        while (v) { *--p = digits[v & 0xf]; v >>= 4; }
    }
    *--p = 'x';
    *--p = '0';
    return p;
}

static void print_int(long v)   { print(itoa(v)); }
static void print_hex(unsigned long v) { print(utohex(v)); }

/* ------------------------------------------------------------------ */
/*  Test tracking                                                     */
/* ------------------------------------------------------------------ */

static int tests_passed = 0;
static int tests_failed = 0;

static void check(int cond, const char *name)
{
    if (cond) {
        print("  [PASS] ");
        print(name);
        print("\n");
        tests_passed++;
    } else {
        print("  [FAIL] ");
        print(name);
        print("\n");
        tests_failed++;
    }
}

/* ------------------------------------------------------------------ */
/*  Aligned data buffers                                              */
/* ------------------------------------------------------------------ */

static int32_t  a32[64] __attribute__((aligned(64)));
static int32_t  b32[64] __attribute__((aligned(64)));
static int32_t  c32[64] __attribute__((aligned(64)));
static int64_t  a64[64] __attribute__((aligned(64)));
static int64_t  b64[64] __attribute__((aligned(64)));
static uint32_t u32[64] __attribute__((aligned(64)));
static int16_t  a16[64] __attribute__((aligned(64)));
static int16_t  b16[64] __attribute__((aligned(64)));
static int8_t   a8[128] __attribute__((aligned(64)));
static int8_t   b8[128] __attribute__((aligned(64)));
static uint8_t  mask_buf[16] __attribute__((aligned(64)));

/* Floating-point buffers */
static double   fa64[32] __attribute__((aligned(64)));
static double   fb64[32] __attribute__((aligned(64)));
static double   fc64[32] __attribute__((aligned(64)));
static float    fa32[64] __attribute__((aligned(64)));
static float    fb32[64] __attribute__((aligned(64)));
static float    fc32[64] __attribute__((aligned(64)));

/* ------------------------------------------------------------------ */
/*  1. Vector Configuration                                           */
/* ------------------------------------------------------------------ */

static void test_config(void)
{
    print("\n=== 1. Vector Configuration (vsetvli / vsetivli) ===\n");

    unsigned long vl;

    /* vsetvli: request e32, m1, ta, ma - returns actual VL */
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma"
                 : "=r"(vl) : "r"(16UL));
    check(vl > 0 && vl <= 16, "vsetvli e32,m1 returns valid VL");
    print("    VL = "); print_int((long)vl); print("\n");

    /* vsetivli: set AVL from immediate */
    unsigned long vl2;
    asm volatile("vsetivli %0, 8, e32, m1, ta, ma" : "=r"(vl2));
    check(vl2 == 8 || vl2 > 0, "vsetivli with AVL=8");

    /* Different LMUL groupings */
    unsigned long vl_m2, vl_m4;
    asm volatile("vsetvli %0, %1, e32, m2, ta, ma"
                 : "=r"(vl_m2) : "r"(64UL));
    asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                 : "=r"(vl_m4) : "r"(64UL));
    check(vl_m4 >= vl_m2, "LMUL m4 >= m2 vector length");
    print("    VL(m2) = "); print_int((long)vl_m2);
    print(", VL(m4) = "); print_int((long)vl_m4); print("\n");

    /* Different element widths */
    unsigned long vl_e8, vl_e16, vl_e32, vl_e64;
    asm volatile("vsetvli %0, %1, e8,  m1, ta, ma" : "=r"(vl_e8)  : "r"(128UL));
    asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl_e16) : "r"(128UL));
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl_e32) : "r"(128UL));
    asm volatile("vsetvli %0, %1, e64, m1, ta, ma" : "=r"(vl_e64) : "r"(128UL));
    check(vl_e8 >= vl_e16 && vl_e16 >= vl_e32 && vl_e32 >= vl_e64,
          "smaller elements => larger VL");
    print("    VL: e8="); print_int((long)vl_e8);
    print(" e16="); print_int((long)vl_e16);
    print(" e32="); print_int((long)vl_e32);
    print(" e64="); print_int((long)vl_e64); print("\n");
}

/* ------------------------------------------------------------------ */
/*  2. Vector Loads & Stores                                          */
/* ------------------------------------------------------------------ */

static void test_load_store(void)
{
    print("\n=== 2. Vector Loads & Stores ===\n");

    /* Initialize data */
    for (int i = 0; i < 16; i++) {
        a32[i] = 100 + i;
        b32[i] = 0;
        u32[i] = (uint32_t)(i * 4);  /* byte offsets for indexed loads */
    }

    unsigned long vl;
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(16UL));

    /* --- Unit-stride load & store (vle32 / vse32) --- */
    asm volatile(
        "vle32.v v1, (%0)\n\t"     /* load from a32 */
        "vse32.v v1, (%1)\n\t"     /* store to b32 */
        : : "r"(a32), "r"(b32) : "memory"
    );
    int ok = 1;
    for (unsigned long i = 0; i < vl; i++)
        if (b32[i] != a32[i]) ok = 0;
    check(ok, "vle32.v / vse32.v (unit-stride load/store)");

    /* --- Strided load (vlse32) --- */
    /* Load every other element (stride = 8 bytes = 2 * sizeof(int32_t)) */
    for (int i = 0; i < 16; i++) c32[i] = 0;
    long stride = 8;
    unsigned long vl_strided;
    asm volatile(
        "vsetvli %0, %3, e32, m1, ta, ma\n\t"
        "vlse32.v v2, (%1), %2\n\t"
        "vse32.v v2, (%4)\n\t"         /* unit-stride store for easy verify */
        : "=r"(vl_strided)
        : "r"(a32), "r"(stride), "r"(8UL), "r"(c32) : "memory"
    );
    ok = 1;
    for (unsigned long i = 0; i < vl_strided; i++)
        if (c32[i] != a32[i * 2]) ok = 0;
    check(ok, "vlse32.v (strided load, stride=2 elements)");

    /* --- Strided store (vsse32) --- */
    for (int i = 0; i < 16; i++) c32[i] = 0;
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v2, (%1)\n\t"           /* unit-stride load */
        "vsse32.v v2, (%2), %3\n\t"      /* strided store */
        : : "r"(vl_strided), "r"(a32), "r"(c32), "r"(stride) : "memory"
    );
    ok = 1;
    for (unsigned long i = 0; i < vl_strided; i++)
        if (c32[i * 2] != a32[i]) ok = 0;
    check(ok, "vsse32.v (strided store, stride=2 elements)");

    /* --- Indexed load (vluxei32) --- */
    /* u32 contains byte offsets: 0, 4, 8, 12, ... */
    for (int i = 0; i < 16; i++) { u32[i] = (uint32_t)(i * 4); c32[i] = 0; }
    asm volatile(
        "vsetvli zero, %2, e32, m1, ta, ma\n\t"
        "vle32.v  v3, (%1)\n\t"     /* load index vector */
        "vluxei32.v v4, (%0), v3\n\t"
        "vse32.v v4, (%3)\n\t"
        : : "r"(a32), "r"(u32), "r"(8UL), "r"(c32) : "memory"
    );
    ok = 1;
    for (int i = 0; i < 8; i++)
        if (c32[i] != a32[i]) ok = 0;
    check(ok, "vluxei32.v (indexed unordered load)");

    /* --- Mask load/store (vlm.v / vsm.v) --- */
    mask_buf[0] = 0xAA;  /* alternating mask bits */
    mask_buf[1] = 0x55;
    uint8_t mask_out[16];
    for (int i = 0; i < 16; i++) mask_out[i] = 0;
    asm volatile(
        "vsetvli zero, %2, e8, m1, ta, ma\n\t"
        "vlm.v v0, (%0)\n\t"
        "vsm.v v0, (%1)\n\t"
        : : "r"(mask_buf), "r"(mask_out), "r"(16UL) : "memory"
    );
    check(mask_out[0] == 0xAA && mask_out[1] == 0x55,
          "vlm.v / vsm.v (mask load/store)");

    /* --- Whole-register load/store (vl1re32 / vs1r) --- */
    for (int i = 0; i < 16; i++) c32[i] = 0;
    asm volatile(
        "vl1re32.v v5, (%0)\n\t"
        "vs1r.v    v5, (%1)\n\t"
        : : "r"(a32), "r"(c32) : "memory"
    );
    /* Check first few elements match */
    ok = 1;
    for (int i = 0; i < (int)vl; i++)
        if (c32[i] != a32[i]) ok = 0;
    check(ok, "vl1re32.v / vs1r.v (whole-register load/store)");

    /* --- Fault-only-first load (vle32ff) --- */
    unsigned long new_vl;
    for (int i = 0; i < 16; i++) c32[i] = 0;
    asm volatile(
        "vsetvli zero, %1, e32, m1, ta, ma\n\t"
        "vle32ff.v v6, (%2)\n\t"
        "csrr %0, vl\n\t"
        "vse32.v v6, (%3)\n\t"
        : "=r"(new_vl)
        : "r"(8UL), "r"(a32), "r"(c32)
        : "memory"
    );
    ok = 1;
    for (unsigned long i = 0; i < new_vl; i++)
        if (c32[i] != a32[i]) ok = 0;
    check(ok, "vle32ff.v (fault-only-first load)");

    /* --- 8-bit and 16-bit load/store --- */
    for (int i = 0; i < 32; i++) { a8[i] = (int8_t)(i * 3); b8[i] = 0; }
    asm volatile(
        "vsetvli zero, %2, e8, m1, ta, ma\n\t"
        "vle8.v v7, (%0)\n\t"
        "vse8.v v7, (%1)\n\t"
        : : "r"(a8), "r"(b8), "r"(32UL) : "memory"
    );
    ok = 1;
    for (int i = 0; i < 32; i++)
        if (b8[i] != a8[i]) ok = 0;
    check(ok, "vle8.v / vse8.v (8-bit element load/store)");

    for (int i = 0; i < 16; i++) { a16[i] = (int16_t)(i * 7); b16[i] = 0; }
    asm volatile(
        "vsetvli zero, %2, e16, m1, ta, ma\n\t"
        "vle16.v v8, (%0)\n\t"
        "vse16.v v8, (%1)\n\t"
        : : "r"(a16), "r"(b16), "r"(16UL) : "memory"
    );
    ok = 1;
    for (int i = 0; i < 16; i++)
        if (b16[i] != a16[i]) ok = 0;
    check(ok, "vle16.v / vse16.v (16-bit element load/store)");
}

/* ------------------------------------------------------------------ */
/*  3. Integer Arithmetic                                             */
/* ------------------------------------------------------------------ */

static void test_integer_arith(void)
{
    print("\n=== 3. Integer Arithmetic ===\n");

    unsigned long vl;

    /* --- vadd --- */
    for (int i = 0; i < 16; i++) { a32[i] = i; b32[i] = 100; c32[i] = 0; }
    asm volatile(
        "vsetvli %0, %1, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%2)\n\t"
        "vle32.v v2, (%3)\n\t"
        "vadd.vv v3, v1, v2\n\t"
        "vse32.v v3, (%4)\n\t"
        : "=r"(vl)
        : "r"(16UL), "r"(a32), "r"(b32), "r"(c32)
        : "memory"
    );
    int ok = 1;
    for (unsigned long i = 0; i < vl; i++)
        if (c32[i] != (int32_t)(i + 100)) ok = 0;
    check(ok, "vadd.vv (vector-vector add)");

    /* vadd.vx (vector-scalar add) */
    for (int i = 0; i < 16; i++) { a32[i] = i * 10; c32[i] = 0; }
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vadd.vx v2, v1, %2\n\t"
        "vse32.v v2, (%3)\n\t"
        : : "r"(vl), "r"(a32), "r"(5L), "r"(c32)
        : "memory"
    );
    ok = 1;
    for (unsigned long i = 0; i < vl; i++)
        if (c32[i] != (int32_t)(i * 10 + 5)) ok = 0;
    check(ok, "vadd.vx (vector-scalar add)");

    /* vadd.vi (vector-immediate add) */
    for (int i = 0; i < 16; i++) c32[i] = 0;
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vadd.vi v2, v1, 3\n\t"
        "vse32.v v2, (%2)\n\t"
        : : "r"(vl), "r"(a32), "r"(c32)
        : "memory"
    );
    ok = 1;
    for (unsigned long i = 0; i < vl; i++)
        if (c32[i] != (int32_t)(i * 10 + 3)) ok = 0;
    check(ok, "vadd.vi (vector-immediate add)");

    /* --- vsub --- */
    for (int i = 0; i < 16; i++) { a32[i] = 200 + i; b32[i] = 100; c32[i] = 0; }
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vle32.v v2, (%2)\n\t"
        "vsub.vv v3, v1, v2\n\t"
        "vse32.v v3, (%3)\n\t"
        : : "r"(vl), "r"(a32), "r"(b32), "r"(c32)
        : "memory"
    );
    ok = 1;
    for (unsigned long i = 0; i < vl; i++)
        if (c32[i] != (int32_t)(100 + (int)i)) ok = 0;
    check(ok, "vsub.vv (vector-vector subtract)");

    /* vsub.vx */
    for (int i = 0; i < 16; i++) { a32[i] = 50 + i; c32[i] = 0; }
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vsub.vx v2, v1, %2\n\t"
        "vse32.v v2, (%3)\n\t"
        : : "r"(vl), "r"(a32), "r"(10L), "r"(c32)
        : "memory"
    );
    ok = 1;
    for (unsigned long i = 0; i < vl; i++)
        if (c32[i] != (int32_t)(40 + (int)i)) ok = 0;
    check(ok, "vsub.vx (vector-scalar subtract)");

    /* --- vrsub.vi (reverse subtract: imm - v) --- */
    for (int i = 0; i < 16; i++) { a32[i] = i; c32[i] = 0; }
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vrsub.vi v2, v1, 15\n\t"
        "vse32.v v2, (%2)\n\t"
        : : "r"(vl), "r"(a32), "r"(c32)
        : "memory"
    );
    ok = 1;
    for (unsigned long i = 0; i < vl; i++)
        if (c32[i] != (int32_t)(15 - (int)i)) ok = 0;
    check(ok, "vrsub.vi (reverse subtract immediate)");

    /* --- vmul --- */
    for (int i = 0; i < 16; i++) { a32[i] = i + 1; b32[i] = i + 2; c32[i] = 0; }
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vle32.v v2, (%2)\n\t"
        "vmul.vv v3, v1, v2\n\t"
        "vse32.v v3, (%3)\n\t"
        : : "r"(vl), "r"(a32), "r"(b32), "r"(c32)
        : "memory"
    );
    ok = 1;
    for (unsigned long i = 0; i < vl; i++)
        if (c32[i] != (int32_t)((i + 1) * (i + 2))) ok = 0;
    check(ok, "vmul.vv (vector-vector multiply)");

    /* vmul.vx */
    for (int i = 0; i < 16; i++) { a32[i] = i + 1; c32[i] = 0; }
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vmul.vx v2, v1, %2\n\t"
        "vse32.v v2, (%3)\n\t"
        : : "r"(vl), "r"(a32), "r"(7L), "r"(c32)
        : "memory"
    );
    ok = 1;
    for (unsigned long i = 0; i < vl; i++)
        if (c32[i] != (int32_t)((i + 1) * 7)) ok = 0;
    check(ok, "vmul.vx (vector-scalar multiply)");

    /* --- vdiv / vrem --- */
    for (int i = 0; i < 16; i++) { a32[i] = (i + 1) * 6; b32[i] = 3; c32[i] = 0; }
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vle32.v v2, (%2)\n\t"
        "vdiv.vv v3, v1, v2\n\t"
        "vse32.v v3, (%3)\n\t"
        : : "r"(vl), "r"(a32), "r"(b32), "r"(c32)
        : "memory"
    );
    ok = 1;
    for (unsigned long i = 0; i < vl; i++)
        if (c32[i] != (int32_t)((i + 1) * 2)) ok = 0;
    check(ok, "vdiv.vv (vector-vector divide)");

    for (int i = 0; i < 16; i++) { a32[i] = i * 3 + 1; c32[i] = 0; }
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vrem.vx v2, v1, %2\n\t"
        "vse32.v v2, (%3)\n\t"
        : : "r"(vl), "r"(a32), "r"(3L), "r"(c32)
        : "memory"
    );
    ok = 1;
    for (unsigned long i = 0; i < vl; i++)
        if (c32[i] != (int32_t)((i * 3 + 1) % 3)) ok = 0;
    check(ok, "vrem.vx (vector-scalar remainder)");

    /* --- vmacc (multiply-accumulate: vd = vd + vs1 * vs2) --- */
    for (int i = 0; i < 16; i++) { a32[i] = i; b32[i] = 2; c32[i] = 10; }
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"   /* vs2 = a32 */
        "vle32.v v2, (%2)\n\t"   /* vs1 = b32 */
        "vle32.v v3, (%3)\n\t"   /* vd  = c32 (accumulator) */
        "vmacc.vv v3, v2, v1\n\t"
        "vse32.v v3, (%3)\n\t"
        : : "r"(vl), "r"(a32), "r"(b32), "r"(c32)
        : "memory"
    );
    ok = 1;
    for (unsigned long i = 0; i < vl; i++)
        if (c32[i] != (int32_t)(10 + 2 * (int)i)) ok = 0;
    check(ok, "vmacc.vv (multiply-accumulate)");

    /* --- vmadd (multiply-add: vd = vs1 * vd + vs2) --- */
    for (int i = 0; i < 16; i++) { a32[i] = 3; b32[i] = 5; c32[i] = i; }
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"   /* vs1 = a32 = 3 */
        "vle32.v v2, (%2)\n\t"   /* vs2 = b32 = 5 */
        "vle32.v v3, (%3)\n\t"   /* vd  = c32 = i */
        "vmadd.vv v3, v1, v2\n\t" /* vd = vs1 * vd + vs2 = 3*i + 5 */
        "vse32.v v3, (%3)\n\t"
        : : "r"(vl), "r"(a32), "r"(b32), "r"(c32)
        : "memory"
    );
    ok = 1;
    for (unsigned long i = 0; i < vl; i++)
        if (c32[i] != (int32_t)(3 * (int)i + 5)) ok = 0;
    check(ok, "vmadd.vv (multiply-add)");

    /* --- vnmsub / vnmsac (negate multiply-sub/acc) --- */
    for (int i = 0; i < 16; i++) { a32[i] = 2; b32[i] = 5; c32[i] = i + 1; }
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"   /* vs1 = 2 */
        "vle32.v v2, (%2)\n\t"   /* vs2 = 5 */
        "vle32.v v3, (%3)\n\t"   /* vd  = i+1 */
        "vnmsac.vv v3, v1, v2\n\t" /* vd = vd - vs1 * vs2 = (i+1) - 2*5 */
        "vse32.v v3, (%3)\n\t"
        : : "r"(vl), "r"(a32), "r"(b32), "r"(c32)
        : "memory"
    );
    ok = 1;
    for (unsigned long i = 0; i < vl; i++)
        if (c32[i] != (int32_t)((int)i + 1 - 10)) ok = 0;
    check(ok, "vnmsac.vv (negate multiply-subtract accumulate)");

    /* --- vmulh (multiply high, signed) --- */
    for (int i = 0; i < 16; i++) { a32[i] = 0x40000000; b32[i] = 4; c32[i] = 0; }
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vle32.v v2, (%2)\n\t"
        "vmulh.vv v3, v1, v2\n\t"
        "vse32.v v3, (%3)\n\t"
        : : "r"(vl), "r"(a32), "r"(b32), "r"(c32)
        : "memory"
    );
    /* 0x40000000 * 4 = 0x100000000, high 32 bits = 1 */
    check(c32[0] == 1, "vmulh.vv (multiply high signed)");

    /* --- Widening add (vwadd) --- */
    for (int i = 0; i < 16; i++) { a32[i] = 0x7FFFFFFF; b32[i] = 1; }
    for (int i = 0; i < 16; i++) a64[i] = 0;
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vle32.v v2, (%2)\n\t"
        "vwadd.vv v4, v1, v2\n\t"  /* result in v4-v5 (e64, m2) */
        "vsetvli zero, %0, e64, m2, ta, ma\n\t"
        "vse64.v v4, (%3)\n\t"
        : : "r"(vl), "r"(a32), "r"(b32), "r"(a64)
        : "memory"
    );
    check(a64[0] == (int64_t)0x7FFFFFFF + 1, "vwadd.vv (widening add)");

    /* --- Saturating add (vsadd) --- */
    for (int i = 0; i < 16; i++) { a32[i] = 0x7FFFFFF0; b32[i] = 0x100; c32[i] = 0; }
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vle32.v v2, (%2)\n\t"
        "vsadd.vv v3, v1, v2\n\t"
        "vse32.v v3, (%3)\n\t"
        : : "r"(vl), "r"(a32), "r"(b32), "r"(c32)
        : "memory"
    );
    check(c32[0] == 0x7FFFFFFF, "vsadd.vv (saturating add clamps to INT32_MAX)");

    /* --- Unsigned saturating add (vsaddu) --- */
    for (int i = 0; i < 16; i++) {
        u32[i] = 0xFFFFFFF0u;
        b32[i] = 0x100;
        c32[i] = 0;
    }
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vle32.v v2, (%2)\n\t"
        "vsaddu.vv v3, v1, v2\n\t"
        "vse32.v v3, (%3)\n\t"
        : : "r"(vl), "r"(u32), "r"(b32), "r"(c32)
        : "memory"
    );
    check((uint32_t)c32[0] == 0xFFFFFFFFu,
          "vsaddu.vv (unsigned saturating add clamps to UINT32_MAX)");
}

/* ------------------------------------------------------------------ */
/*  4. Bitwise & Logical Operations                                   */
/* ------------------------------------------------------------------ */

static void test_bitwise(void)
{
    print("\n=== 4. Bitwise & Logical Operations ===\n");

    unsigned long vl;
    for (int i = 0; i < 16; i++) {
        a32[i] = 0xFF00FF00;
        b32[i] = 0x0F0F0F0F;
        c32[i] = 0;
    }

    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(16UL));

    /* vand */
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vand.vv v3, v1, v2\n\t"
        "vse32.v v3, (%2)\n\t"
        : : "r"(a32), "r"(b32), "r"(c32) : "memory"
    );
    check((uint32_t)c32[0] == (0xFF00FF00u & 0x0F0F0F0Fu),
          "vand.vv (bitwise AND)");

    /* vor */
    asm volatile(
        "vor.vv v3, v1, v2\n\t"
        "vse32.v v3, (%0)\n\t"
        : : "r"(c32) : "memory"
    );
    check((uint32_t)c32[0] == (0xFF00FF00u | 0x0F0F0F0Fu),
          "vor.vv (bitwise OR)");

    /* vxor */
    asm volatile(
        "vxor.vv v3, v1, v2\n\t"
        "vse32.v v3, (%0)\n\t"
        : : "r"(c32) : "memory"
    );
    check((uint32_t)c32[0] == (0xFF00FF00u ^ 0x0F0F0F0Fu),
          "vxor.vv (bitwise XOR)");

    /* vand.vi */
    for (int i = 0; i < 16; i++) { a32[i] = 0x1F; c32[i] = 0; }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vand.vi v3, v1, 7\n\t"
        "vse32.v v3, (%1)\n\t"
        : : "r"(a32), "r"(c32) : "memory"
    );
    check(c32[0] == (0x1F & 7), "vand.vi (AND with immediate)");

    /* vnot via vxor with -1 */
    for (int i = 0; i < 16; i++) { a32[i] = 0x12345678; c32[i] = 0; }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vxor.vi v3, v1, -1\n\t"   /* NOT = XOR with all 1s */
        "vse32.v v3, (%1)\n\t"
        : : "r"(a32), "r"(c32) : "memory"
    );
    check((uint32_t)c32[0] == ~0x12345678u,
          "vxor.vi -1 (bitwise NOT via XOR)");
}

/* ------------------------------------------------------------------ */
/*  5. Shift Operations                                               */
/* ------------------------------------------------------------------ */

static void test_shift(void)
{
    print("\n=== 5. Shift Operations ===\n");

    unsigned long vl;
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(16UL));

    /* vsll (shift left logical) */
    for (int i = 0; i < 16; i++) { a32[i] = 1; c32[i] = 0; }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vsll.vi v2, v1, 4\n\t"
        "vse32.v v2, (%1)\n\t"
        : : "r"(a32), "r"(c32) : "memory"
    );
    check(c32[0] == 16, "vsll.vi (shift left logical by 4)");

    /* vsrl (shift right logical) */
    for (int i = 0; i < 16; i++) { a32[i] = 256; c32[i] = 0; }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vsrl.vi v2, v1, 3\n\t"
        "vse32.v v2, (%1)\n\t"
        : : "r"(a32), "r"(c32) : "memory"
    );
    check(c32[0] == 32, "vsrl.vi (shift right logical by 3)");

    /* vsra (shift right arithmetic) */
    for (int i = 0; i < 16; i++) { a32[i] = -128; c32[i] = 0; }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vsra.vi v2, v1, 2\n\t"
        "vse32.v v2, (%1)\n\t"
        : : "r"(a32), "r"(c32) : "memory"
    );
    check(c32[0] == -32, "vsra.vi (shift right arithmetic by 2)");

    /* vsll.vv (variable shift) */
    for (int i = 0; i < 16; i++) { a32[i] = 1; b32[i] = i; c32[i] = 0; }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vsll.vv v3, v1, v2\n\t"
        "vse32.v v3, (%2)\n\t"
        : : "r"(a32), "r"(b32), "r"(c32) : "memory"
    );
    int ok = 1;
    for (unsigned long i = 0; i < vl; i++)
        if (c32[i] != (1 << (int)i)) ok = 0;
    check(ok, "vsll.vv (variable shift left)");

    /* Narrowing shift right (vnsrl) */
    for (int i = 0; i < 16; i++) a64[i] = 0x00000002DEADBEEFLL;
    for (int i = 0; i < 16; i++) c32[i] = 0;
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle64.v v2, (%1)\n\t"   /* load 64-bit values into v2-v3 */
        "vnsrl.wi v4, v2, 0\n\t" /* narrow: take low 32 bits */
        "vse32.v v4, (%2)\n\t"
        : : "r"(vl), "r"(a64), "r"(c32)
        : "memory"
    );
    check((uint32_t)c32[0] == 0xDEADBEEFu,
          "vnsrl.wi (narrowing shift right - extract low 32)");
}

/* ------------------------------------------------------------------ */
/*  6. Integer Comparisons                                            */
/* ------------------------------------------------------------------ */

static void test_compare(void)
{
    print("\n=== 6. Integer Comparisons ===\n");

    unsigned long vl;
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(8UL));

    for (int i = 0; i < 8; i++) {
        a32[i] = i;
        b32[i] = 4;
    }

    /* vmseq - set mask where a32[i] == 4 (only i=4) */
    mask_buf[0] = 0;
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vmseq.vv v0, v1, v2\n\t"
        "vsm.v v0, (%2)\n\t"
        : : "r"(a32), "r"(b32), "r"(mask_buf) : "memory"
    );
    check(mask_buf[0] == 0x10, "vmseq.vv (equal => mask bit 4 set)");

    /* vmsne - not equal */
    mask_buf[0] = 0;
    asm volatile(
        "vmsne.vv v0, v1, v2\n\t"
        "vsm.v v0, (%0)\n\t"
        : : "r"(mask_buf) : "memory"
    );
    check(mask_buf[0] == 0xEF, "vmsne.vv (not-equal mask)");

    /* vmslt - less than */
    mask_buf[0] = 0;
    asm volatile(
        "vmslt.vv v0, v1, v2\n\t"  /* a32[i] < 4 ? */
        "vsm.v v0, (%0)\n\t"
        : : "r"(mask_buf) : "memory"
    );
    check(mask_buf[0] == 0x0F, "vmslt.vv (less-than: elements 0-3)");

    /* vmsle - less than or equal */
    mask_buf[0] = 0;
    asm volatile(
        "vmsle.vv v0, v1, v2\n\t"
        "vsm.v v0, (%0)\n\t"
        : : "r"(mask_buf) : "memory"
    );
    check(mask_buf[0] == 0x1F, "vmsle.vv (less-equal: elements 0-4)");

    /* vmsgt.vi - greater than immediate */
    mask_buf[0] = 0;
    asm volatile(
        "vmsgt.vi v0, v1, 5\n\t"   /* a32[i] > 5 ? => {6,7} */
        "vsm.v v0, (%0)\n\t"
        : : "r"(mask_buf) : "memory"
    );
    check(mask_buf[0] == 0xC0, "vmsgt.vi (greater-than 5: elements 6-7)");

    /* vmseq.vi - equal to immediate */
    mask_buf[0] = 0;
    asm volatile(
        "vmseq.vi v0, v1, 3\n\t"
        "vsm.v v0, (%0)\n\t"
        : : "r"(mask_buf) : "memory"
    );
    check(mask_buf[0] == 0x08, "vmseq.vi (equal to 3: element 3)");
}

/* ------------------------------------------------------------------ */
/*  7. Min / Max                                                      */
/* ------------------------------------------------------------------ */

static void test_minmax(void)
{
    print("\n=== 7. Integer Min/Max ===\n");

    unsigned long vl;
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(8UL));

    for (int i = 0; i < 8; i++) {
        a32[i] = i * 3 - 10;    /* -10, -7, -4, -1, 2, 5, 8, 11 */
        b32[i] = 0;
        c32[i] = 0;
    }

    /* vmin */
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vmin.vx v2, v1, %1\n\t"
        "vse32.v v2, (%2)\n\t"
        : : "r"(a32), "r"(0L), "r"(c32) : "memory"
    );
    int ok = 1;
    for (int i = 0; i < 8; i++) {
        int32_t expected = (a32[i] < 0) ? a32[i] : 0;
        if (c32[i] != expected) ok = 0;
    }
    check(ok, "vmin.vx (clamp to min of element and 0)");

    /* vmax */
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vmax.vx v2, v1, %1\n\t"
        "vse32.v v2, (%2)\n\t"
        : : "r"(a32), "r"(0L), "r"(c32) : "memory"
    );
    ok = 1;
    for (int i = 0; i < 8; i++) {
        int32_t expected = (a32[i] > 0) ? a32[i] : 0;
        if (c32[i] != expected) ok = 0;
    }
    check(ok, "vmax.vx (clamp to max of element and 0)");

    /* vminu / vmaxu (unsigned) */
    for (int i = 0; i < 8; i++) {
        u32[i] = (uint32_t)(i * 50);
        c32[i] = 0;
    }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vminu.vx v2, v1, %1\n\t"
        "vse32.v v2, (%2)\n\t"
        : : "r"(u32), "r"(200UL), "r"(c32) : "memory"
    );
    ok = 1;
    for (int i = 0; i < 8; i++) {
        uint32_t expected = (u32[i] < 200) ? u32[i] : 200;
        if ((uint32_t)c32[i] != expected) ok = 0;
    }
    check(ok, "vminu.vx (unsigned min clamped to 200)");
}

/* ------------------------------------------------------------------ */
/*  8. Merge & Move                                                   */
/* ------------------------------------------------------------------ */

static void test_merge_move(void)
{
    print("\n=== 8. Integer Merge & Move ===\n");

    unsigned long vl;
    asm volatile("vsetvli %0, %1, e32, m1, ta, mu" : "=r"(vl) : "r"(8UL));

    /* vmerge.vvm - select from v1 or v2 based on mask */
    for (int i = 0; i < 8; i++) {
        a32[i] = 111;
        b32[i] = 222;
        c32[i] = 0;
    }
    mask_buf[0] = 0xAA;  /* bits: 10101010 => elements 1,3,5,7 from v2 */
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vlm.v v0, (%4)\n\t"
        "vle32.v v1, (%1)\n\t"
        "vle32.v v2, (%2)\n\t"
        "vmerge.vvm v3, v1, v2, v0\n\t"
        "vse32.v v3, (%3)\n\t"
        : : "r"(8UL), "r"(a32), "r"(b32), "r"(c32), "r"(mask_buf)
        : "memory"
    );
    int ok = 1;
    for (int i = 0; i < 8; i++) {
        int32_t expected = (mask_buf[0] & (1 << i)) ? 222 : 111;
        if (c32[i] != expected) ok = 0;
    }
    check(ok, "vmerge.vvm (merge with mask select)");

    /* vmv.v.x - splat scalar to vector */
    for (int i = 0; i < 16; i++) c32[i] = 0;
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vmv.v.x v1, %1\n\t"
        "vse32.v v1, (%2)\n\t"
        : : "r"(8UL), "r"(42L), "r"(c32) : "memory"
    );
    ok = 1;
    for (int i = 0; i < 8; i++)
        if (c32[i] != 42) ok = 0;
    check(ok, "vmv.v.x (splat scalar 42 to all elements)");

    /* vmv.v.i - splat immediate */
    for (int i = 0; i < 16; i++) c32[i] = 0;
    asm volatile(
        "vmv.v.i v1, 7\n\t"
        "vse32.v v1, (%0)\n\t"
        : : "r"(c32) : "memory"
    );
    ok = 1;
    for (int i = 0; i < 8; i++)
        if (c32[i] != 7) ok = 0;
    check(ok, "vmv.v.i (splat immediate 7)");

    /* vmv.x.s / vmv.s.x - scalar extract/insert */
    for (int i = 0; i < 8; i++) a32[i] = (i + 1) * 100;
    long scalar_val = 0;
    asm volatile(
        "vle32.v v1, (%1)\n\t"
        "vmv.x.s %0, v1\n\t"
        : "=r"(scalar_val) : "r"(a32) : "memory"
    );
    check((int32_t)scalar_val == 100, "vmv.x.s (extract element 0 = 100)");

    for (int i = 0; i < 8; i++) c32[i] = 0;
    asm volatile(
        "vmv.v.i v1, 0\n\t"
        "vmv.s.x v1, %0\n\t"
        "vse32.v v1, (%1)\n\t"
        : : "r"(999L), "r"(c32) : "memory"
    );
    check(c32[0] == 999, "vmv.s.x (insert scalar 999 at element 0)");
}

/* ------------------------------------------------------------------ */
/*  9. Sign / Zero Extension & Type Narrowing                         */
/* ------------------------------------------------------------------ */

static void test_extension(void)
{
    print("\n=== 9. Sign/Zero Extension ===\n");

    unsigned long vl;

    /* vsext.vf2 - sign extend 16-bit to 32-bit */
    for (int i = 0; i < 16; i++) { a16[i] = (int16_t)(-(i + 1)); c32[i] = 0; }
    asm volatile(
        "vsetvli %0, %1, e32, m1, ta, ma\n\t"
        "vle16.v v1, (%2)\n\t"
        "vsext.vf2 v2, v1\n\t"
        "vse32.v v2, (%3)\n\t"
        : "=r"(vl) : "r"(8UL), "r"(a16), "r"(c32)
        : "memory"
    );
    int ok = 1;
    for (int i = 0; i < 8; i++)
        if (c32[i] != -(i + 1)) ok = 0;
    check(ok, "vsext.vf2 (sign extend i16 -> i32)");

    /* vzext.vf2 - zero extend 16-bit to 32-bit */
    for (int i = 0; i < 16; i++) { a16[i] = (int16_t)0xFF00; c32[i] = 0; }
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle16.v v1, (%1)\n\t"
        "vzext.vf2 v2, v1\n\t"
        "vse32.v v2, (%2)\n\t"
        : : "r"(8UL), "r"(a16), "r"(c32)
        : "memory"
    );
    check((uint32_t)c32[0] == 0x0000FF00u,
          "vzext.vf2 (zero extend u16 -> u32)");
}

/* ------------------------------------------------------------------ */
/*  10. Reduction Operations                                          */
/* ------------------------------------------------------------------ */

static void test_reductions(void)
{
    print("\n=== 10. Reduction Operations ===\n");

    unsigned long vl;
    long result;

    /* vredsum - sum reduction */
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(8UL));
    for (unsigned long i = 0; i < vl; i++) a32[i] = (int32_t)(i + 1);
    long expected_sum = 0;
    for (unsigned long i = 0; i < vl; i++) expected_sum += (long)(i + 1);
    asm volatile(
        "vle32.v v1, (%1)\n\t"
        "vmv.v.i v2, 0\n\t"
        "vredsum.vs v3, v1, v2\n\t"
        "vmv.x.s %0, v3\n\t"
        : "=r"(result) : "r"(a32) : "memory"
    );
    check((int32_t)result == (int32_t)expected_sum,
          "vredsum.vs (sum reduction)");

    /* vredmax - max reduction */
    for (unsigned long i = 0; i < vl; i++) a32[i] = (int32_t)i;
    a32[vl > 3 ? 3 : 0] = 999;
    asm volatile(
        "vle32.v v1, (%1)\n\t"
        "vmv.v.x v2, %2\n\t"
        "vredmax.vs v3, v1, v2\n\t"
        "vmv.x.s %0, v3\n\t"
        : "=r"(result) : "r"(a32), "r"(-1000L) : "memory"
    );
    check((int32_t)result == 999, "vredmax.vs (max element = 999)");

    /* vredmin - min reduction */
    for (unsigned long i = 0; i < vl; i++) a32[i] = (int32_t)(i * 10);
    a32[vl > 2 ? 2 : 0] = -42;
    asm volatile(
        "vle32.v v1, (%1)\n\t"
        "vmv.v.x v2, %2\n\t"
        "vredmin.vs v3, v1, v2\n\t"
        "vmv.x.s %0, v3\n\t"
        : "=r"(result) : "r"(a32), "r"(1000L) : "memory"
    );
    check((int32_t)result == -42, "vredmin.vs (min element = -42)");

    /* vredor - OR reduction */
    for (unsigned long i = 0; i < vl; i++) a32[i] = 1 << (int)i;
    asm volatile(
        "vle32.v v1, (%1)\n\t"
        "vmv.v.i v2, 0\n\t"
        "vredor.vs v3, v1, v2\n\t"
        "vmv.x.s %0, v3\n\t"
        : "=r"(result) : "r"(a32) : "memory"
    );
    int32_t expected_or = (1 << (int)vl) - 1;
    check((int32_t)result == expected_or, "vredor.vs (OR of powers of 2)");

    /* vredand - AND reduction */
    for (unsigned long i = 0; i < vl; i++) a32[i] = 0xFF;
    a32[vl > 3 ? 3 : 0] = 0x0F;
    asm volatile(
        "vle32.v v1, (%1)\n\t"
        "vmv.v.x v2, %2\n\t"
        "vredand.vs v3, v1, v2\n\t"
        "vmv.x.s %0, v3\n\t"
        : "=r"(result) : "r"(a32), "r"(0xFFL) : "memory"
    );
    check((int32_t)result == 0x0F,
          "vredand.vs (AND reduction limited by 0x0F element)");

    /* vredxor - XOR reduction */
    for (unsigned long i = 0; i < vl; i++) a32[i] = 1;
    /* Make sure we have an even count of 1s */
    if (vl & 1) a32[vl - 1] = 0;
    asm volatile(
        "vle32.v v1, (%1)\n\t"
        "vmv.v.i v2, 0\n\t"
        "vredxor.vs v3, v1, v2\n\t"
        "vmv.x.s %0, v3\n\t"
        : "=r"(result) : "r"(a32) : "memory"
    );
    check((int32_t)result == 0, "vredxor.vs (XOR of even count of 1s = 0)");

    /* Widening sum reduction (vwredsum) */
    for (unsigned long i = 0; i < vl; i++) a32[i] = 0x40000000;
    asm volatile(
        "vsetvli zero, %2, e64, m1, ta, ma\n\t"
        "vmv.v.i v4, 0\n\t"
        "vsetvli zero, %2, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vwredsum.vs v4, v1, v4\n\t"
        "vsetvli zero, %2, e64, m1, ta, ma\n\t"
        "vmv.x.s %0, v4\n\t"
        : "=r"(result) : "r"(a32), "r"(vl) : "memory"
    );
    check(result == (long)vl * 0x40000000LL,
          "vwredsum.vs (widening sum avoids overflow)");
}

/* ------------------------------------------------------------------ */
/*  11. Mask Operations                                               */
/* ------------------------------------------------------------------ */

static void test_mask_ops(void)
{
    print("\n=== 11. Mask Operations ===\n");

    unsigned long vl;
    asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vl) : "r"(8UL));

    /* vmand.mm */
    mask_buf[0] = 0xF0;
    mask_buf[1] = 0x3C;
    uint8_t mout[4] = {0};
    asm volatile(
        "vlm.v v1, (%0)\n\t"
        "vlm.v v2, (%1)\n\t"
        "vmand.mm v3, v1, v2\n\t"
        "vsm.v v3, (%2)\n\t"
        : : "r"(&mask_buf[0]), "r"(&mask_buf[1]), "r"(mout) : "memory"
    );
    check(mout[0] == (0xF0 & 0x3C), "vmand.mm (mask AND)");

    /* vmor.mm */
    asm volatile(
        "vmor.mm v3, v1, v2\n\t"
        "vsm.v v3, (%0)\n\t"
        : : "r"(mout) : "memory"
    );
    check(mout[0] == (0xF0 | 0x3C), "vmor.mm (mask OR)");

    /* vmxor.mm */
    asm volatile(
        "vmxor.mm v3, v1, v2\n\t"
        "vsm.v v3, (%0)\n\t"
        : : "r"(mout) : "memory"
    );
    check(mout[0] == (0xF0 ^ 0x3C), "vmxor.mm (mask XOR)");

    /* vmnand.mm */
    asm volatile(
        "vmnand.mm v3, v1, v2\n\t"
        "vsm.v v3, (%0)\n\t"
        : : "r"(mout) : "memory"
    );
    check(mout[0] == (uint8_t)~(0xF0 & 0x3C), "vmnand.mm (mask NAND)");

    /* vcpop.m - population count of mask */
    mask_buf[0] = 0xA5;  /* 10100101 => 4 bits set */
    long popcnt = 0;
    asm volatile(
        "vlm.v v0, (%1)\n\t"
        "vcpop.m %0, v0\n\t"
        : "=r"(popcnt) : "r"(mask_buf)
    );
    check(popcnt == 4, "vcpop.m (mask popcount of 0xA5 = 4)");

    /* vfirst.m - find first set bit */
    mask_buf[0] = 0x30;  /* 00110000 => first set at position 4 */
    long first = -1;
    asm volatile(
        "vlm.v v0, (%1)\n\t"
        "vfirst.m %0, v0\n\t"
        : "=r"(first) : "r"(mask_buf)
    );
    check(first == 4, "vfirst.m (first set bit of 0x30 = position 4)");

    /* vid.v - vector of element indices */
    for (int i = 0; i < 8; i++) c32[i] = 0;
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vid.v v1\n\t"
        "vse32.v v1, (%1)\n\t"
        : : "r"(8UL), "r"(c32) : "memory"
    );
    int ok = 1;
    for (int i = 0; i < 8; i++)
        if (c32[i] != i) ok = 0;
    check(ok, "vid.v (vector of indices 0,1,2,...,7)");

    /* viota.m - prefix sum of mask bits */
    mask_buf[0] = 0x55;  /* 01010101 - bits 0,2,4,6 are set */
    for (int i = 0; i < 8; i++) c32[i] = -1;
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vlm.v v0, (%1)\n\t"
        "viota.m v1, v0\n\t"
        "vse32.v v1, (%2)\n\t"
        : : "r"(8UL), "r"(mask_buf), "r"(c32)
        : "memory"
    );
    /* viota result: [0,0,1,1,2,2,3,3] (count of set mask bits before each position) */
    check(c32[0] == 0 && c32[2] == 1 && c32[4] == 2 && c32[6] == 3,
          "viota.m (prefix exclusive popcount of mask)");
}

/* ------------------------------------------------------------------ */
/*  12. Permutation (gather, slide, compress)                         */
/* ------------------------------------------------------------------ */

static void test_permutation(void)
{
    print("\n=== 12. Permutation ===\n");

    unsigned long vl;
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(8UL));

    /* vrgather.vv - gather elements by index vector */
    for (int i = 0; i < 8; i++) a32[i] = (i + 1) * 10;  /* 10,20,...,80 */
    int32_t idx32[8] = {7, 6, 5, 4, 3, 2, 1, 0};  /* reverse order */
    for (int i = 0; i < 8; i++) c32[i] = 0;
    asm volatile(
        "vle32.v v1, (%0)\n\t"    /* data */
        "vle32.v v2, (%1)\n\t"    /* indices (reverse) */
        "vrgather.vv v3, v1, v2\n\t"
        "vse32.v v3, (%2)\n\t"
        : : "r"(a32), "r"(idx32), "r"(c32) : "memory"
    );
    int ok = 1;
    for (int i = 0; i < 8; i++)
        if (c32[i] != (int32_t)((8 - i) * 10)) ok = 0;
    check(ok, "vrgather.vv (reverse permutation via gather)");

    /* vrgather.vi - gather with immediate index (broadcast element) */
    for (int i = 0; i < 8; i++) c32[i] = 0;
    asm volatile(
        "vrgather.vi v3, v1, 2\n\t"   /* broadcast a32[2]=30 */
        "vse32.v v3, (%0)\n\t"
        : : "r"(c32) : "memory"
    );
    ok = 1;
    for (int i = 0; i < 8; i++)
        if (c32[i] != 30) ok = 0;
    check(ok, "vrgather.vi (broadcast element 2)");

    /* vslideup - slide elements up by offset */
    for (int i = 0; i < 8; i++) { a32[i] = i + 1; c32[i] = 0; }
    asm volatile(
        "vmv.v.i v3, 0\n\t"         /* clear destination */
        "vle32.v v1, (%0)\n\t"
        "vslideup.vi v3, v1, 2\n\t"  /* shift up by 2 */
        "vse32.v v3, (%1)\n\t"
        : : "r"(a32), "r"(c32) : "memory"
    );
    /* c32[0..1] = 0, c32[2] = a32[0] = 1, c32[3] = a32[1] = 2, etc */
    check(c32[0] == 0 && c32[1] == 0 && c32[2] == 1 && c32[3] == 2,
          "vslideup.vi (slide elements up by 2)");

    /* vslidedown - slide elements down by offset */
    for (int i = 0; i < 8; i++) { a32[i] = (i + 1) * 10; c32[i] = 0; }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vslidedown.vi v3, v1, 3\n\t"
        "vse32.v v3, (%1)\n\t"
        : : "r"(a32), "r"(c32) : "memory"
    );
    /* c32[0] = a32[3] = 40, c32[1] = a32[4] = 50, ... */
    check(c32[0] == 40 && c32[1] == 50 && c32[2] == 60,
          "vslidedown.vi (slide elements down by 3)");

    /* vslide1up - shift up by 1, inserting scalar at position 0 */
    for (int i = 0; i < 8; i++) { a32[i] = (i + 1) * 10; c32[i] = 0; }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vslide1up.vx v3, v1, %1\n\t"
        "vse32.v v3, (%2)\n\t"
        : : "r"(a32), "r"(99L), "r"(c32) : "memory"
    );
    check(c32[0] == 99 && c32[1] == 10 && c32[2] == 20,
          "vslide1up.vx (insert 99 at front, shift right)");

    /* vslide1down - shift down by 1, inserting scalar at end */
    for (int i = 0; i < 8; i++) { a32[i] = (i + 1) * 10; c32[i] = 0; }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vslide1down.vx v3, v1, %1\n\t"
        "vse32.v v3, (%2)\n\t"
        : : "r"(a32), "r"(88L), "r"(c32) : "memory"
    );
    check(c32[0] == 20 && c32[1] == 30 && c32[vl - 1] == 88,
          "vslide1down.vx (shift left, insert 88 at end)");

    /* vcompress - compress selected elements */
    for (int i = 0; i < 8; i++) { a32[i] = (i + 1) * 10; c32[i] = 0; }
    mask_buf[0] = 0x55;  /* select elements 0,2,4,6 */
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vlm.v v0, (%2)\n\t"
        "vcompress.vm v3, v1, v0\n\t"
        "vse32.v v3, (%1)\n\t"
        : : "r"(a32), "r"(c32), "r"(mask_buf) : "memory"
    );
    check(c32[0] == 10 && c32[1] == 30 && c32[2] == 50 && c32[3] == 70,
          "vcompress.vm (compress even-indexed elements)");
}

/* ------------------------------------------------------------------ */
/*  13. Add/Sub with Carry/Borrow                                     */
/* ------------------------------------------------------------------ */

static void test_carry_borrow(void)
{
    print("\n=== 13. Carry/Borrow Arithmetic ===\n");

    unsigned long vl;
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(8UL));

    /* vadc - add with carry from mask */
    for (int i = 0; i < 8; i++) { a32[i] = 100; b32[i] = 50; c32[i] = 0; }
    mask_buf[0] = 0xAA;  /* carry=1 for elements 1,3,5,7 */
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vlm.v v0, (%3)\n\t"
        "vadc.vvm v3, v1, v2, v0\n\t"
        "vse32.v v3, (%2)\n\t"
        : : "r"(a32), "r"(b32), "r"(c32), "r"(mask_buf)
        : "memory"
    );
    int ok = 1;
    for (int i = 0; i < 8; i++) {
        int carry = (mask_buf[0] >> i) & 1;
        if (c32[i] != 100 + 50 + carry) ok = 0;
    }
    check(ok, "vadc.vvm (add with carry from mask)");

    /* vmadc - compute carry-out into mask */
    for (int i = 0; i < 8; i++) {
        a32[i] = (i < 4) ? (int32_t)0x7FFFFFFF : 0;
        b32[i] = (i < 4) ? 1 : 0;
    }
    uint8_t carry_out[2] = {0};
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vmadc.vv v3, v1, v2\n\t"
        "vsm.v v3, (%2)\n\t"
        : : "r"(a32), "r"(b32), "r"(carry_out) : "memory"
    );
    /* 0x7FFFFFFF + 1 = 0x80000000 - no unsigned carry, but signed overflow.
       vmadc works on unsigned carry, so 0x7FFFFFFF + 1 doesn't carry unsigned */
    /* For unsigned carry: 0xFFFFFFFF + 1 would carry */
    for (int i = 0; i < 8; i++) u32[i] = (i < 4) ? 0xFFFFFFFFu : 0;
    for (int i = 0; i < 8; i++) b32[i] = (i < 4) ? 1 : 0;
    carry_out[0] = 0;
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vmadc.vv v3, v1, v2\n\t"
        "vsm.v v3, (%2)\n\t"
        : : "r"(u32), "r"(b32), "r"(carry_out) : "memory"
    );
    check((carry_out[0] & 0x0F) == 0x0F,
          "vmadc.vv (carry-out for 0xFFFFFFFF + 1)");

    /* vsbc - subtract with borrow from mask */
    for (int i = 0; i < 8; i++) { a32[i] = 200; b32[i] = 50; c32[i] = 0; }
    mask_buf[0] = 0x55;  /* borrow=1 for elements 0,2,4,6 */
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vlm.v v0, (%3)\n\t"
        "vsbc.vvm v3, v1, v2, v0\n\t"
        "vse32.v v3, (%2)\n\t"
        : : "r"(a32), "r"(b32), "r"(c32), "r"(mask_buf)
        : "memory"
    );
    ok = 1;
    for (int i = 0; i < 8; i++) {
        int borrow = (mask_buf[0] >> i) & 1;
        if (c32[i] != 200 - 50 - borrow) ok = 0;
    }
    check(ok, "vsbc.vvm (subtract with borrow from mask)");
}

/* ------------------------------------------------------------------ */
/*  14. Fixed-Point (Averaging, Saturating Multiply)                  */
/* ------------------------------------------------------------------ */

static void test_fixed_point(void)
{
    print("\n=== 14. Fixed-Point Arithmetic ===\n");

    unsigned long vl;
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(8UL));

    /* vaadd - averaging add: (a + b + rounding) >> 1 */
    for (int i = 0; i < 8; i++) { a32[i] = 10; b32[i] = 21; c32[i] = 0; }
    asm volatile(
        "csrwi vxrm, 0\n\t"   /* rounding mode = round-to-nearest-up */
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vaadd.vv v3, v1, v2\n\t"
        "vse32.v v3, (%2)\n\t"
        : : "r"(a32), "r"(b32), "r"(c32) : "memory"
    );
    /* (10 + 21 + 1) >> 1 = 16 with rnu, or (10+21)>>1 = 15 with rdn */
    check(c32[0] == 16 || c32[0] == 15,
          "vaadd.vv (averaging add)");

    /* vasub - averaging subtract */
    for (int i = 0; i < 8; i++) { a32[i] = 21; b32[i] = 10; c32[i] = 0; }
    asm volatile(
        "csrwi vxrm, 2\n\t"   /* rounding mode = round-down (truncate) */
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vasub.vv v3, v1, v2\n\t"
        "vse32.v v3, (%2)\n\t"
        : : "r"(a32), "r"(b32), "r"(c32) : "memory"
    );
    /* (21 - 10) >> 1 = 5 (truncated) */
    check(c32[0] == 5, "vasub.vv (averaging subtract, truncated)");

    /* vssub - saturating subtract */
    for (int i = 0; i < 8; i++) { a32[i] = (int32_t)0x80000000; b32[i] = 1; c32[i] = 0; }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vssub.vv v3, v1, v2\n\t"
        "vse32.v v3, (%2)\n\t"
        : : "r"(a32), "r"(b32), "r"(c32) : "memory"
    );
    check(c32[0] == (int32_t)0x80000000,
          "vssub.vv (saturating subtract clamps to INT32_MIN)");
}

/* ------------------------------------------------------------------ */
/*  15. Floating-Point Arithmetic                                     */
/* ------------------------------------------------------------------ */

static void test_fp_arith(void)
{
    print("\n=== 15. Floating-Point Arithmetic ===\n");

    unsigned long vl;

    /* --- Single precision (float) --- */
    for (int i = 0; i < 8; i++) {
        fa32[i] = (float)(i + 1) * 1.5f;
        fb32[i] = (float)(i + 1) * 0.5f;
        fc32[i] = 0.0f;
    }

    /* vfadd */
    asm volatile(
        "vsetvli %0, %1, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%2)\n\t"
        "vle32.v v2, (%3)\n\t"
        "vfadd.vv v3, v1, v2\n\t"
        "vse32.v v3, (%4)\n\t"
        : "=r"(vl)
        : "r"(8UL), "r"(fa32), "r"(fb32), "r"(fc32)
        : "memory"
    );
    int ok = 1;
    for (int i = 0; i < 8; i++) {
        float expected = (float)(i + 1) * 2.0f;
        if (fc32[i] != expected) ok = 0;
    }
    check(ok, "vfadd.vv (FP32 vector add)");

    /* vfsub */
    asm volatile(
        "vfsub.vv v3, v1, v2\n\t"
        "vse32.v v3, (%0)\n\t"
        : : "r"(fc32) : "memory"
    );
    ok = 1;
    for (int i = 0; i < 8; i++) {
        float expected = (float)(i + 1) * 1.0f;
        if (fc32[i] != expected) ok = 0;
    }
    check(ok, "vfsub.vv (FP32 vector subtract)");

    /* vfmul */
    for (int i = 0; i < 8; i++) { fa32[i] = (float)(i + 1); fb32[i] = 3.0f; }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vfmul.vv v3, v1, v2\n\t"
        "vse32.v v3, (%2)\n\t"
        : : "r"(fa32), "r"(fb32), "r"(fc32) : "memory"
    );
    ok = 1;
    for (int i = 0; i < 8; i++)
        if (fc32[i] != (float)((i + 1) * 3)) ok = 0;
    check(ok, "vfmul.vv (FP32 vector multiply)");

    /* vfdiv */
    for (int i = 0; i < 8; i++) { fa32[i] = (float)((i + 1) * 12); fb32[i] = 4.0f; }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vfdiv.vv v3, v1, v2\n\t"
        "vse32.v v3, (%2)\n\t"
        : : "r"(fa32), "r"(fb32), "r"(fc32) : "memory"
    );
    ok = 1;
    for (int i = 0; i < 8; i++)
        if (fc32[i] != (float)((i + 1) * 3)) ok = 0;
    check(ok, "vfdiv.vv (FP32 vector divide)");

    /* vfsqrt */
    for (int i = 0; i < 8; i++) fa32[i] = (float)((i + 1) * (i + 1));
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vfsqrt.v v2, v1\n\t"
        "vse32.v v2, (%1)\n\t"
        : : "r"(fa32), "r"(fc32) : "memory"
    );
    ok = 1;
    for (int i = 0; i < 8; i++)
        if (fc32[i] != (float)(i + 1)) ok = 0;
    check(ok, "vfsqrt.v (FP32 square root of perfect squares)");

    /* vfmacc (fused multiply-accumulate: vd = vd + vs1 * vs2) */
    for (int i = 0; i < 8; i++) {
        fa32[i] = (float)(i + 1);   /* vs2 */
        fb32[i] = 2.0f;             /* vs1 */
        fc32[i] = 10.0f;            /* vd (accumulator) */
    }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vle32.v v3, (%2)\n\t"
        "vfmacc.vv v3, v2, v1\n\t"
        "vse32.v v3, (%2)\n\t"
        : : "r"(fa32), "r"(fb32), "r"(fc32) : "memory"
    );
    ok = 1;
    for (int i = 0; i < 8; i++)
        if (fc32[i] != 10.0f + 2.0f * (float)(i + 1)) ok = 0;
    check(ok, "vfmacc.vv (FP32 fused multiply-accumulate)");

    /* vfnmacc (negated fused multiply-accumulate: vd = -(vs1*vs2) - vd) */
    for (int i = 0; i < 8; i++) {
        fa32[i] = (float)(i + 1);
        fb32[i] = 2.0f;
        fc32[i] = 5.0f;
    }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vle32.v v3, (%2)\n\t"
        "vfnmacc.vv v3, v2, v1\n\t"
        "vse32.v v3, (%2)\n\t"
        : : "r"(fa32), "r"(fb32), "r"(fc32) : "memory"
    );
    ok = 1;
    for (int i = 0; i < 8; i++) {
        float expected = -(2.0f * (float)(i + 1)) - 5.0f;
        if (fc32[i] != expected) ok = 0;
    }
    check(ok, "vfnmacc.vv (FP32 negated fused multiply-accumulate)");

    /* vfmsac (fused multiply-subtract: vd = vs1*vs2 - vd) */
    for (int i = 0; i < 8; i++) {
        fa32[i] = (float)(i + 1);
        fb32[i] = 10.0f;
        fc32[i] = 3.0f;
    }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vle32.v v3, (%2)\n\t"
        "vfmsac.vv v3, v2, v1\n\t"
        "vse32.v v3, (%2)\n\t"
        : : "r"(fa32), "r"(fb32), "r"(fc32) : "memory"
    );
    ok = 1;
    for (int i = 0; i < 8; i++) {
        float expected = 10.0f * (float)(i + 1) - 3.0f;
        if (fc32[i] != expected) ok = 0;
    }
    check(ok, "vfmsac.vv (FP32 fused multiply-subtract accumulate)");

    /* --- Double precision (double) --- */
    for (int i = 0; i < 4; i++) {
        fa64[i] = (double)(i + 1) * 1.25;
        fb64[i] = (double)(i + 1) * 0.75;
        fc64[i] = 0.0;
    }
    asm volatile(
        "vsetvli zero, %0, e64, m1, ta, ma\n\t"
        "vle64.v v1, (%1)\n\t"
        "vle64.v v2, (%2)\n\t"
        "vfadd.vv v3, v1, v2\n\t"
        "vse64.v v3, (%3)\n\t"
        : : "r"(4UL), "r"(fa64), "r"(fb64), "r"(fc64)
        : "memory"
    );
    ok = 1;
    for (int i = 0; i < 4; i++) {
        double expected = (double)(i + 1) * 2.0;
        if (fc64[i] != expected) ok = 0;
    }
    check(ok, "vfadd.vv e64 (FP64 double-precision add)");

    /* vfadd.vf - add scalar */
    for (int i = 0; i < 8; i++) { fa32[i] = (float)i; fc32[i] = 0.0f; }
    double scalar = 100.0;
    asm volatile(
        "vsetvli zero, %0, e64, m1, ta, ma\n\t"
        "vle64.v v1, (%1)\n\t"
        "vfadd.vf v2, v1, %2\n\t"
        "vse64.v v2, (%3)\n\t"
        : : "r"(4UL), "r"(fa64), "f"(scalar), "r"(fc64)
        : "memory"
    );
    ok = 1;
    for (int i = 0; i < 4; i++)
        if (fc64[i] != fa64[i] + 100.0) ok = 0;
    check(ok, "vfadd.vf (FP add vector + scalar)");

    /* vfmul.vf - multiply by scalar */
    for (int i = 0; i < 4; i++) { fa64[i] = (double)(i + 1); fc64[i] = 0.0; }
    double mul_scalar = 2.5;
    asm volatile(
        "vsetvli zero, %0, e64, m1, ta, ma\n\t"
        "vle64.v v1, (%1)\n\t"
        "vfmul.vf v2, v1, %2\n\t"
        "vse64.v v2, (%3)\n\t"
        : : "r"(4UL), "r"(fa64), "f"(mul_scalar), "r"(fc64)
        : "memory"
    );
    ok = 1;
    for (int i = 0; i < 4; i++)
        if (fc64[i] != (double)(i + 1) * 2.5) ok = 0;
    check(ok, "vfmul.vf (FP multiply vector * scalar)");
}

/* ------------------------------------------------------------------ */
/*  16. FP Comparison, Min/Max, Sign-Injection, Classification        */
/* ------------------------------------------------------------------ */

static void test_fp_compare(void)
{
    print("\n=== 16. FP Compare, Min/Max, Sign-Inject, Classify ===\n");

    unsigned long vl;
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(8UL));

    /* vmfeq - FP equal */
    for (int i = 0; i < 8; i++) {
        fa32[i] = (float)i;
        fb32[i] = (i == 3 || i == 5) ? (float)i : (float)(i + 1);
    }
    mask_buf[0] = 0;
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vmfeq.vv v0, v1, v2\n\t"
        "vsm.v v0, (%2)\n\t"
        : : "r"(fa32), "r"(fb32), "r"(mask_buf) : "memory"
    );
    check((mask_buf[0] & (1 << 3)) && (mask_buf[0] & (1 << 5)),
          "vmfeq.vv (FP equal comparison)");

    /* vmflt - FP less than */
    for (int i = 0; i < 8; i++) { fa32[i] = (float)i; fb32[i] = 4.0f; }
    mask_buf[0] = 0;
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vmflt.vv v0, v1, v2\n\t"
        "vsm.v v0, (%2)\n\t"
        : : "r"(fa32), "r"(fb32), "r"(mask_buf) : "memory"
    );
    check(mask_buf[0] == 0x0F, "vmflt.vv (FP less-than: elements 0-3)");

    /* vfmin / vfmax */
    for (int i = 0; i < 8; i++) {
        fa32[i] = (float)(i - 4);  /* -4, -3, ..., 3 */
        fb32[i] = 0.0f;
    }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vfmin.vv v3, v1, v2\n\t"
        "vfmax.vv v4, v1, v2\n\t"
        "vse32.v v3, (%2)\n\t"
        "vse32.v v4, (%3)\n\t"
        : : "r"(fa32), "r"(fb32), "r"(fc32), "r"(fc32 + 8)
        : "memory"
    );
    int ok = 1;
    for (int i = 0; i < 8; i++) {
        float lo = fa32[i] < fb32[i] ? fa32[i] : fb32[i];
        float hi = fa32[i] > fb32[i] ? fa32[i] : fb32[i];
        if (fc32[i] != lo || fc32[i + 8] != hi) ok = 0;
    }
    check(ok, "vfmin.vv / vfmax.vv (FP min/max)");

    /* vfsgnj - sign injection (copy sign of second operand) */
    for (int i = 0; i < 8; i++) { fa32[i] = (float)(i + 1); fb32[i] = -1.0f; }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vfsgnj.vv v3, v1, v2\n\t"
        "vse32.v v3, (%2)\n\t"
        : : "r"(fa32), "r"(fb32), "r"(fc32) : "memory"
    );
    ok = 1;
    for (int i = 0; i < 8; i++)
        if (fc32[i] != -(float)(i + 1)) ok = 0;
    check(ok, "vfsgnj.vv (sign injection -> negate all)");

    /* vfsgnjn - negated sign injection (negate sign of second) -> abs */
    asm volatile(
        "vfsgnjn.vv v3, v1, v2\n\t"
        "vse32.v v3, (%0)\n\t"
        : : "r"(fc32) : "memory"
    );
    ok = 1;
    for (int i = 0; i < 8; i++)
        if (fc32[i] != (float)(i + 1)) ok = 0;
    check(ok, "vfsgnjn.vv (negated sign injection)");

    /* vfsgnjx - XOR sign injection (flip sign) -> negate */
    for (int i = 0; i < 8; i++) { fa32[i] = (float)(i + 1); fb32[i] = -1.0f; }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vfsgnjx.vv v3, v1, v2\n\t"
        "vse32.v v3, (%2)\n\t"
        : : "r"(fa32), "r"(fb32), "r"(fc32) : "memory"
    );
    ok = 1;
    for (int i = 0; i < 8; i++)
        if (fc32[i] != -(float)(i + 1)) ok = 0;
    check(ok, "vfsgnjx.vv (XOR sign injection -> negate)");

    /* vfclass - classify FP values */
    fa32[0] = -1.0f / 0.0f;  /* -inf */
    fa32[1] = -1.0f;          /* negative normal */
    fa32[2] = -0.0f;          /* negative zero */
    fa32[3] = 0.0f;           /* positive zero */
    fa32[4] = 1.0f;           /* positive normal */
    fa32[5] = 1.0f / 0.0f;    /* +inf */
    /* classes: -inf=0, -norm=1, -sub=2, -0=3, +0=4, +sub=5, +norm=6, +inf=7, sNaN=8, qNaN=9 */
    /* vfclass returns a bitmask: bit i set means class i */
    uint32_t class_results[8];
    for (int i = 0; i < 8; i++) class_results[i] = 0;
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vfclass.v v2, v1\n\t"
        "vse32.v v2, (%2)\n\t"
        : : "r"(6UL), "r"(fa32), "r"(class_results) : "memory"
    );
    check(class_results[0] == 1      /* -inf = bit 0 */
       && class_results[3] == 16     /* +zero = bit 4 */
       && class_results[5] == 128,   /* +inf = bit 7 */
          "vfclass.v (classify -inf, +0, +inf)");
}

/* ------------------------------------------------------------------ */
/*  17. FP Conversion & Widening/Narrowing                            */
/* ------------------------------------------------------------------ */

static void test_fp_convert(void)
{
    print("\n=== 17. FP Conversion & Widening/Narrowing ===\n");

    unsigned long vl;

    /* vfcvt.x.f.v - FP to integer (round toward zero) */
    for (int i = 0; i < 8; i++) fa32[i] = (float)(i + 1) * 1.7f;
    for (int i = 0; i < 8; i++) c32[i] = 0;
    asm volatile(
        "vsetvli %0, %1, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%2)\n\t"
        "vfcvt.rtz.x.f.v v2, v1\n\t"
        "vse32.v v2, (%3)\n\t"
        : "=r"(vl) : "r"(8UL), "r"(fa32), "r"(c32)
        : "memory"
    );
    int ok = 1;
    for (int i = 0; i < 8; i++) {
        int32_t expected = (int32_t)((float)(i + 1) * 1.7f);
        if (c32[i] != expected) ok = 0;
    }
    check(ok, "vfcvt.rtz.x.f.v (FP32 to int32 with truncation)");

    /* vfcvt.f.x.v - integer to FP */
    for (int i = 0; i < 8; i++) a32[i] = (i + 1) * 7;
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vfcvt.f.x.v v2, v1\n\t"
        "vse32.v v2, (%2)\n\t"
        : : "r"(8UL), "r"(a32), "r"(fc32)
        : "memory"
    );
    ok = 1;
    for (int i = 0; i < 8; i++)
        if (fc32[i] != (float)((i + 1) * 7)) ok = 0;
    check(ok, "vfcvt.f.x.v (int32 to FP32)");

    /* vfwcvt - widening convert float32 to float64 */
    for (int i = 0; i < 4; i++) fa32[i] = (float)(i + 1) * 1.5f;
    for (int i = 0; i < 4; i++) fc64[i] = 0.0;
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vfwcvt.f.f.v v2, v1\n\t"     /* f32 -> f64, result in v2-v3 */
        "vsetvli zero, %0, e64, m2, ta, ma\n\t"
        "vse64.v v2, (%2)\n\t"
        : : "r"(4UL), "r"(fa32), "r"(fc64) : "memory"
    );
    ok = 1;
    for (int i = 0; i < 4; i++)
        if (fc64[i] != (double)((float)(i + 1) * 1.5f)) ok = 0;
    check(ok, "vfwcvt.f.f.v (widen FP32 -> FP64)");

    /* vfncvt - narrowing convert float64 to float32 */
    for (int i = 0; i < 4; i++) fa64[i] = (double)(i + 1) * 2.5;
    for (int i = 0; i < 8; i++) fc32[i] = 0.0f;
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle64.v v2, (%1)\n\t"
        "vfncvt.f.f.w v1, v2\n\t"
        "vse32.v v1, (%2)\n\t"
        : : "r"(4UL), "r"(fa64), "r"(fc32)
        : "memory"
    );
    ok = 1;
    for (int i = 0; i < 4; i++)
        if (fc32[i] != (float)((double)(i + 1) * 2.5)) ok = 0;
    check(ok, "vfncvt.f.f.w (narrow FP64 -> FP32)");
}

/* ------------------------------------------------------------------ */
/*  18. FP Reductions                                                 */
/* ------------------------------------------------------------------ */

static void test_fp_reductions(void)
{
    print("\n=== 18. FP Reductions ===\n");

    /* vfredusum - unordered FP sum reduction */
    for (int i = 0; i < 8; i++) fa32[i] = (float)(i + 1);
    float fsum_result = 0.0f;
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vmv.v.x v2, zero\n\t"    /* init scalar element to 0 */
        "vfredusum.vs v3, v1, v2\n\t"
        "vse32.v v3, (%2)\n\t"
        : : "r"(8UL), "r"(fa32), "r"(&fsum_result)
        : "memory"
    );
    check(fsum_result == 36.0f, "vfredusum.vs (FP sum 1+2+...+8 = 36)");

    /* vfredosum - ordered FP sum reduction */
    fsum_result = 0.0f;
    asm volatile(
        "vfredosum.vs v3, v1, v2\n\t"
        "vse32.v v3, (%0)\n\t"
        : : "r"(&fsum_result)
        : "memory"
    );
    check(fsum_result == 36.0f, "vfredosum.vs (ordered FP sum = 36)");

    /* vfredmin / vfredmax */
    for (int i = 0; i < 8; i++) fa32[i] = (float)(i - 3);  /* -3..4 */
    float fmin_result, fmax_result;
    float big = 1e10f, small = -1e10f;
    asm volatile(
        "vsetvli zero, %0, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vfmv.v.f v4, %4\n\t"
        "vfredmin.vs v5, v1, v4\n\t"
        "vse32.v v5, (%2)\n\t"
        "vfmv.v.f v4, %5\n\t"
        "vfredmax.vs v5, v1, v4\n\t"
        "vse32.v v5, (%3)\n\t"
        : : "r"(8UL), "r"(fa32), "r"(&fmin_result), "r"(&fmax_result),
            "f"(big), "f"(small)
        : "memory"
    );
    check(fmin_result == -3.0f && fmax_result == 4.0f,
          "vfredmin/vfredmax.vs (FP min=-3, max=4)");
}

/* ------------------------------------------------------------------ */
/*  19. Whole-Register Move                                           */
/* ------------------------------------------------------------------ */

static void test_whole_reg_move(void)
{
    print("\n=== 19. Whole-Register Move ===\n");

    unsigned long vl;
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(8UL));

    for (int i = 0; i < 8; i++) { a32[i] = (i + 1) * 11; c32[i] = 0; }
    asm volatile(
        "vle32.v v1, (%0)\n\t"
        "vmv1r.v v5, v1\n\t"      /* copy v1 -> v5 */
        "vse32.v v5, (%1)\n\t"
        : : "r"(a32), "r"(c32) : "memory"
    );
    int ok = 1;
    for (int i = 0; i < 8; i++)
        if (c32[i] != (i + 1) * 11) ok = 0;
    check(ok, "vmv1r.v (whole register copy)");

    /* vmv2r.v - copy 2 registers */
    for (int i = 0; i < 16; i++) { a32[i] = i * 5; c32[i] = 0; }
    asm volatile(
        "vsetvli zero, %0, e32, m2, ta, ma\n\t"
        "vle32.v v2, (%1)\n\t"
        "vmv2r.v v6, v2\n\t"
        "vse32.v v6, (%2)\n\t"
        : : "r"(16UL), "r"(a32), "r"(c32) : "memory"
    );
    unsigned long vl2;
    asm volatile("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(vl2) : "r"(16UL));
    ok = 1;
    for (unsigned long i = 0; i < vl2; i++)
        if (c32[i] != (int32_t)(i * 5)) ok = 0;
    check(ok, "vmv2r.v (2-register group copy)");
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                       */
/* ------------------------------------------------------------------ */

/* Naked assembly entry: set up stack and enable FP/Vector before any C code */
void __attribute__((naked, section(".text.init"))) _start(void)
{
    asm volatile(
        /* Enable FP (mstatus.FS=01, bits 14:13) and Vector (mstatus.VS=01, bits 10:9) */
        "li   t0, 0x6600\n\t"   /* FS=Dirty(11), VS=Dirty(11) */
        "csrs mstatus, t0\n\t"
        "la   sp, _stack_top\n\t"
        "j    _main\n\t"
    );
}

void _main(void)
{
    print("============================================================\n");
    print("  RISC-V Vector Extension (RVV 1.0) Comprehensive Demo\n");
    print("============================================================\n");
    print("  Running on Spike ISA Simulator\n");

    /* Run all test suites */
    test_config();
    test_load_store();
    test_integer_arith();
    test_bitwise();
    test_shift();
    test_compare();
    test_minmax();
    test_merge_move();
    test_extension();
    test_reductions();
    test_mask_ops();
    test_permutation();
    test_carry_borrow();
    test_fixed_point();
    test_fp_arith();
    test_fp_compare();
    test_fp_convert();
    test_fp_reductions();
    test_whole_reg_move();

    /* Summary */
    print("\n============================================================\n");
    print("  Results: ");
    print_int(tests_passed); print(" passed, ");
    print_int(tests_failed); print(" failed out of ");
    print_int(tests_passed + tests_failed); print(" tests\n");
    print("============================================================\n");

    if (tests_failed == 0) {
        print("  ALL TESTS PASSED!\n");
    } else {
        print("  SOME TESTS FAILED!\n");
    }
    print("============================================================\n");

    htif_exit(tests_failed);
}
