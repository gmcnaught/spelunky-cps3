/* tests/playsh2: the level generator and the play loop (the real src/game code, PIN_MAX patched to fit main RAM:
   Makefile) on the SH-2. Runs the jobs of build/jobs.h (mkjobs.py); before each job main RAM's .data / .bss are
   set up again, as a new host process starts. Results go to sprite RAM (unused: no video), read by
   scripts/lua/playsh2.lua when P_STATE = 1:
     0x04000000  'PSH2', state (1 done), records, jobs, overflow count, prof tag, prof samples, prof period (ticks),
                 prof kind, skip counter, skip,
                 soft-float caller (wrap.S), PROF_WRAP
     0x04000100  records, 5 words: job << 16 | kind << 12 | idx, checksum, instances, extra, CPU clocks
     0x04010000  PROF builds: samples, ((PC - 0x06000000) >> 4) << 16 | (PR - 0x06000000) >> 4, at most 49,152
     0x04040000  the generator's instance list (sprbss.ld)
   CPU clocks from the SH7604 FRC (phi / 8) extended by its overflow interrupt (isr.S), so any length is measured;
   the work is bracketed by plat_begin / plat_end (core.c). */
#include <stdint.h>
#include "core.h"
#include "jobs.h"

#define R32(a) (*(volatile uint32_t *)(a))
#define P_MAGIC R32(0x04000000)
#define P_STATE R32(0x04000004)
#define P_NREC R32(0x04000008)
#define P_NJOB R32(0x0400000c)
#define P_OVF R32(0x04000010)
#define P_TAG R32(0x04000014)
#define P_PN R32(0x04000018)
#define P_PER R32(0x0400001c)
#define P_WANT R32(0x04000020)
#define P_SKC R32(0x04000024)
#define P_SKIP R32(0x04000028)
#define P_WRAPF R32(0x04000030)
#ifndef PROF_WRAP
#define PROF_WRAP 0
#endif
#define P_REC ((volatile uint32_t *)0x04000100)
#define REC_MAX ((0x10000 - 0x100) / 20)
#ifndef PROF_PERIOD
#define PROF_PERIOD 0                 /* PROF builds: ticks (x 8 clocks) between samples, at most 65,535 */
#endif
#ifndef PROF_SKIP
#define PROF_SKIP 1                   /* PROF builds: one interrupt in PROF_SKIP logs */
#endif
#ifndef PROF_KIND
#define PROF_KIND KIND_STEP           /* the work sampled: KIND_STEP (route steps and level starts) or KIND_GEN */
#endif

extern char __data_start[] __asm__("__data_start"), __data_end[] __asm__("__data_end"), __data_load[] __asm__("__data_load"), __bss_start[] __asm__("__bss_start"), __bss_end[] __asm__("__bss_end");
void ovf_isr(void);
void prof_isr(void);
static void hang(void) { for (;;) ; }

/* the vector table (VBR): FRT output compare A 72, overflow 73; the rest stops */
#define V(n) [n] = hang
static void (*const vtab[128])(void) __attribute__((aligned(1024))) = {
    V(0), V(1), V(2), V(3), V(4), V(5), V(6), V(7), V(8), V(9), V(10), V(11), V(12), V(13), V(14), V(15),
    V(16), V(17), V(18), V(19), V(20), V(21), V(22), V(23), V(24), V(25), V(26), V(27), V(28), V(29), V(30),
    V(31), V(32), V(33), V(34), V(35), V(36), V(37), V(38), V(39), V(40), V(41), V(42), V(43), V(44), V(45),
    V(46), V(47), V(48), V(49), V(50), V(51), V(52), V(53), V(54), V(55), V(56), V(57), V(58), V(59), V(60),
    V(61), V(62), V(63), V(64), V(65), V(66), V(67), V(68), V(69), V(70), V(71), [72] = prof_isr,
    [73] = ovf_isr,
};

static uint32_t frc(void)
{
    volatile uint8_t *f = (volatile uint8_t *)0xfffffe12u;
    uint32_t h = f[0], l = f[1], h2 = f[0];
    if (h2 != h) { l = f[1]; h = h2; }
    return h << 8 | l;
}
static uint32_t now(void)                 /* FRC ticks since start-up, 32 bits */
{
    uint32_t o, f;
    do {
        o = P_OVF;
        f = frc();
    } while (o != P_OVF);
    return o << 16 | f;
}

static uint32_t t0, t1;
static int cur_tag;
void plat_begin(void) { if (PROF_PERIOD) P_TAG = (uint32_t)cur_tag; t0 = now(); }
void plat_end(void) { t1 = now(); P_TAG = 0; }
void plat_rec(int job, int kind, int idx, uint32_t sum, uint32_t n, uint32_t extra)
{
    uint32_t r = P_NREC;
    volatile uint32_t *p = P_REC + 5 * r;
    if (r >= REC_MAX) return;
    p[0] = (uint32_t)job << 16 | (uint32_t)kind << 12 | ((uint32_t)idx & 0xfff);
    p[1] = sum;
    p[2] = n;
    p[3] = extra;
    p[4] = (t1 - t0) * 8;
    P_NREC = r + 1;
    cur_tag = kind == KIND_START ? KIND_STEP : kind;  /* the steps after a level start are tagged as steps */
}

/* the C library calls GCC may emit (-fno-builtin, no libc) */
void *memcpy(void *d, const void *s, unsigned long n)
{
    char *a = d;
    const char *b = s;
    while (n--) *a++ = *b++;
    return d;
}
void *memset(void *d, int c, unsigned long n)
{
    char *a = d;
    while (n--) *a++ = (char)c;
    return d;
}

static void __attribute__((noinline)) ram_init(void)
{
    uint32_t *d = (uint32_t *)__data_start, *s = (uint32_t *)__data_load;
    while (d < (uint32_t *)__data_end) *d++ = *s++;
    for (d = (uint32_t *)__bss_start; d < (uint32_t *)__bss_end; d++) *d = 0;
}

int main(void)
{
    int j;
    uint32_t sr;
    P_STATE = 0;
    P_NREC = 0;
    P_NJOB = NJOBS;
    P_OVF = 0;
    P_TAG = 0;
    P_PN = 0;
    P_PER = PROF_PERIOD;
    P_WANT = PROF_KIND;
    P_SKC = 0;
    P_SKIP = PROF_SKIP;
    P_WRAPF = PROF_WRAP;
    P_MAGIC = 0x50534832;
    __asm__ volatile("ldc %0, vbr" : : "r"(vtab));
    *(volatile uint16_t *)0xfffffe66 = 72;               /* VCRC: FRT output compare vector */
    *(volatile uint16_t *)0xfffffe68 = 73 << 8;          /* VCRD: FRT overflow vector */
    *(volatile uint16_t *)0xfffffe60 = 0x0f00;           /* IPRB: FRT priority 15 */
    *(volatile uint8_t *)0xfffffe16 = 0x00;              /* TCR: phi / 8 */
    if (PROF_PERIOD) {
        uint32_t t = frc() + PROF_PERIOD;
        *(volatile uint8_t *)0xfffffe14 = (uint8_t)(t >> 8);
        *(volatile uint8_t *)0xfffffe15 = (uint8_t)t;
    }
    *(volatile uint8_t *)0xfffffe10 = PROF_PERIOD ? 0x0b : 0x03; /* TIER: OVIE (+ OCIAE); bit 0 reads 1 */
    __asm__ volatile("stc sr, %0" : "=r"(sr));
    sr = (sr & ~0xf0u) | (14u << 4);                     /* only level 15 (the FRT): VBlank stays masked */
    __asm__ volatile("ldc %0, sr" : : "r"(sr));
#ifdef BENCH
    ram_init();
    run_bench(0);
    P_STATE = 1;
    for (;;) ;
#endif
    for (j = 0; j < NJOBS; j++) {
        ram_init();
        cur_tag = jobs[j].route < 0 ? KIND_GEN : KIND_START;
        if (jobs[j].route < 0) run_gen(j, jobs[j].seed, jobs[j].level);
        else run_route(j, jobs[j].seed, routes[jobs[j].route].masks, routes[jobs[j].route].n, 30);
    }
    P_STATE = 1;
    for (;;) ;
}
