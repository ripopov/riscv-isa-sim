/* CoreMark port for bare-metal Spike (RVA22U64, M-mode).

   Derived from CoreMark's own "barebones" port; the differences are the ones
   a 64-bit target forces: pointer-sized ee_ptr_int, and a 64-bit cycle counter
   as the time source. */

#ifndef CORE_PORTME_H
#define CORE_PORTME_H

#include <stddef.h>

#define HAS_FLOAT   1
#define HAS_TIME_H  0
#define USE_CLOCK   0
#define HAS_STDIO   0
#define HAS_PRINTF  0

#ifndef COMPILER_VERSION
#define COMPILER_VERSION "GCC" __VERSION__
#endif
#ifndef COMPILER_FLAGS
#define COMPILER_FLAGS FLAGS_STR
#endif
#ifndef MEM_LOCATION
#define MEM_LOCATION "STATIC"
#endif

typedef signed short   ee_s16;
typedef unsigned short ee_u16;
typedef signed int     ee_s32;
typedef double         ee_f32;
typedef unsigned char  ee_u8;
typedef unsigned int   ee_u32;
typedef unsigned long  ee_u64;
/* Must hold a pointer: on rv64 that is 64 bits, which is where the stock
   barebones port (ee_u32) would silently truncate. */
typedef unsigned long  ee_ptr_int;
typedef size_t         ee_size_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

#define align_mem(x) (void *)(4 + (((ee_ptr_int)(x)-1) & ~3))

/* Time is measured in retired cycles read from the cycle CSR.  Spike retires
   one instruction per cycle, so seconds are reported against a nominal 1 GHz
   IPC=1 machine -- useful for comparing guest builds, not a CoreMark score. */
#define CORETIMETYPE ee_u64
typedef ee_u64 CORE_TICKS;
#define EE_TICKS_PER_SEC 1000000000UL

#ifndef SEED_METHOD
#define SEED_METHOD SEED_VOLATILE
#endif
#ifndef MEM_METHOD
#define MEM_METHOD MEM_STATIC
#endif

#ifndef MULTITHREAD
#define MULTITHREAD 1
#define USE_PTHREAD 0
#define USE_FORK    0
#define USE_SOCKET  0
#endif

/* Spike's boot ROM does not hand main a command line. */
#define MAIN_HAS_NOARGC   1
#define MAIN_HAS_NORETURN 0

extern ee_u32 default_num_contexts;

typedef struct CORE_PORTABLE_S
{
    ee_u8 portable_id;
} core_portable;

/* Pick the run type from the data size the way CoreMark's own ports do, unless
   the build already named one. */
#if !defined(PROFILE_RUN) && !defined(PERFORMANCE_RUN) && !defined(VALIDATION_RUN)
#if (TOTAL_DATA_SIZE == 1200)
#define PROFILE_RUN 1
#elif (TOTAL_DATA_SIZE == 2000)
#define PERFORMANCE_RUN 1
#else
#define VALIDATION_RUN 1
#endif
#endif

void portable_init(core_portable *p, int *argc, char *argv[]);
void portable_fini(core_portable *p);

int ee_printf(const char *fmt, ...);

/* Character sink behind ee_printf, implemented on HTIF in spike_port.c. */
void spike_putchar(char c);

#endif /* CORE_PORTME_H */
