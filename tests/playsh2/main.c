/* tests/playsh2: the level generator and the play loop (the real src/game code, PIN_MAX patched to fit main RAM:
   Makefile) on the SH-2. Runs the jobs of build/jobs.h (mkjobs.py); before each job main RAM's .data / .bss are
   set up again, as a new host process starts. Results go to character RAM (the 1 MB window at 0x04100000, bank 0;
   unused: no video; MAME only), read by scripts/lua/playsh2.lua when P_STATE = 1; the game's largest arrays go to
   sprite RAM (sprbss.ld):
     0x04100000  'PSH2', state (1 done), records, jobs, overflow count, prof tag, prof samples, prof period (ticks),
                 prof kind, skip counter, skip,
                 soft-float caller (wrap.S), PROF_WRAP
     0x04100100  records, 5 words: job << 16 | kind << 12 | idx, checksum, instances, extra, CPU clocks
     0x04140000  PROF builds: samples, ((PC - 0x06000000) >> 4) << 16 | (PR - 0x06000000) >> 4, at most 65,536
     0x04180000  ATTR builds: time by category and by event x object (attr.c)
   CPU clocks from the SH7604 FRC (phi / 32: one tick 32 clocks, a wrap 2.1 M clocks) extended by the wrap count
   isr.S keeps, so any length is measured;
   the work is bracketed by plat_begin / plat_end (core.c). */
#include <stdint.h>
#include "core.h"
#include "jobs.h"

#define R32(a) (*(volatile uint32_t *)(a))
/* the header block (isr.S uses it too): character RAM in the MAME builds; JT builds (jtcps3: no MAME-only memory)
   keep it at the start of main RAM, the .trace section (not cleared between jobs) */
#ifdef JT
#define PBASE 0x02000000
static uint32_t p_block[64] __attribute__((section(".trace"), used));
#else
#define PBASE 0x04100000
#endif
#define P_MAGIC R32(PBASE + 0x00)
#define P_STATE R32(PBASE + 0x04)
#define P_NREC R32(PBASE + 0x08)
#define P_NJOB R32(PBASE + 0x0c)
#define P_OVF R32(PBASE + 0x10)
#define P_TAG R32(PBASE + 0x14)
#define P_PN R32(PBASE + 0x18)
#define P_PER R32(PBASE + 0x1c)
#define P_WANT R32(PBASE + 0x20)
#define P_SKC R32(PBASE + 0x24)
#define P_SKIP R32(PBASE + 0x28)
#define P_WRAPF R32(PBASE + 0x30)
#ifndef PROF_WRAP
#define PROF_WRAP 0
#endif
#define P_REC ((volatile uint32_t *)0x04100100)
#define REC_MAX ((0x40000 - 0x100) / 20)
#ifndef PROF_PERIOD
#define PROF_PERIOD 0                 /* PROF builds: ticks (x 32 clocks) between samples, at most 65,535 */
#endif
#ifndef PROF_SKIP
#define PROF_SKIP 1                   /* PROF builds: one interrupt in PROF_SKIP logs */
#endif
#define HEART (PROF_PERIOD ? PROF_PERIOD : 4096)
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
/* FRC ticks since start-up, 32 bits: the wrap count kept by the output-compare interrupt (isr.S, every period <
   65,536 ticks) and the FRC at that interrupt; an FRC below that value has wrapped once since */
#define P_EXTH R32(PBASE + 0x34)
#define P_LASTF R32(PBASE + 0x38)
static uint32_t now(void)
{
    uint32_t h, lf, f;
    do {
        h = P_EXTH;
        lf = P_LASTF;
        f = frc();
    } while (h != P_EXTH || lf != P_LASTF);
    return f >= lf ? (h << 16 | f) : ((h + 1) << 16 | f);
}

static uint32_t t0, t1;
static int cur_tag;
uint32_t plat_now(void) { return now(); }
#ifdef ATTR
void attr_step_begin(void);
uint32_t attr_step_end(void);
static uint32_t attr_coll;
#endif
void plat_begin(void)
{
    if (PROF_PERIOD) P_TAG = (uint32_t)cur_tag;
    t0 = now();
#ifdef ATTR
    if (cur_tag == KIND_STEP) attr_step_begin();
#endif
}
void plat_end(void)
{
    t1 = now();
#ifdef ATTR
    attr_coll = cur_tag == KIND_STEP ? attr_step_end() : 0;
#endif
    P_TAG = 0;
}
#ifdef JT
/* JT builds: per job in the header block, words 16 + 8 * job: total clocks, steps, the steps' clocks, the largest
   step, the records' hash (sum and instances of each, FNV-1a), the level start's clocks */
#define JTR(j, k) R32(PBASE + 4 * (16 + 8 * (j) + (k)))
static uint32_t fnv(uint32_t h, uint32_t v)
{
    int k;
    for (k = 0; k < 4; k++) { h ^= (v >> (8 * k)) & 0xff; h *= 16777619u; }
    return h;
}
#endif
void plat_rec(int job, int kind, int idx, uint32_t sum, uint32_t n, uint32_t extra)
{
#ifdef JT
    uint32_t c = (t1 - t0) * 32;
    (void)extra;
    (void)idx;
    JTR(job, 0) += c;
    if (kind == KIND_STEP || kind == KIND_EARLY) {
        JTR(job, 1) += 1;
        JTR(job, 2) += c;
        if (c > JTR(job, 3)) JTR(job, 3) = c;
    } else
        JTR(job, 5) = c;
    JTR(job, 4) = fnv(fnv(JTR(job, 4), sum), n);
    cur_tag = kind == KIND_START ? KIND_STEP : kind;
    return;
#else
    uint32_t r = P_NREC;
    volatile uint32_t *p = P_REC + 5 * r;
    if (r >= REC_MAX) return;
    p[0] = (uint32_t)job << 16 | (uint32_t)kind << 12 | ((uint32_t)idx & 0xfff);
    p[1] = sum;
    p[2] = n;
#ifdef ATTR
    extra = attr_coll;                            /* ATTR builds: the step's collision clocks (attr.c) */
#endif
    p[3] = extra;
    p[4] = (t1 - t0) * 32;
    P_NREC = r + 1;
    cur_tag = kind == KIND_START ? KIND_STEP : kind;  /* the steps after a level start are tagged as steps */
#endif
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

#ifndef JT
extern char __sprbss_start[] __asm__("__sprbss_start"), __sprbss_end[] __asm__("__sprbss_end");
#endif
static void __attribute__((noinline)) ram_init(void)
{
    uint32_t *d = (uint32_t *)__data_start, *s = (uint32_t *)__data_load;
    while (d < (uint32_t *)__data_end) *d++ = *s++;
    for (d = (uint32_t *)__bss_start; d < (uint32_t *)__bss_end; d++) *d = 0;
#ifndef JT
    for (d = (uint32_t *)__sprbss_start; d < (uint32_t *)__sprbss_end; d++) *d = 0;
#endif
}

#ifdef JT
#include "cps3.h"
#include "jt_expect.h"
void big_init(void);
void big_text(int row, const char *s);       /* bigtext.c: 24 characters a row, rows 0-8 */
static char *put_u(char *p, uint32_t v)
{
    char t[12];
    int n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *p++ = t[--n];
    *p = 0;
    return p;
}
static char *put_s(char *p, const char *s)
{
    while (*s) *p++ = *s++;
    *p = 0;
    return p;
}
/* sprite RAM as plain memory: 128 KB at 0x04060000 written through the cached address, read back through the
   uncached mirror (0x24060000) and the cached one; the words that differ */
static uint32_t spr_test(void)
{
    volatile uint32_t *w = (volatile uint32_t *)0x04060000u, *u = (volatile uint32_t *)0x24060000u;
    uint32_t k, bad = 0, x;
    for (k = 0, x = 0x12345678u; k < 0x8000; k++) { x = x * 1664525u + 1013904223u; w[k] = x; }
    for (k = 0, x = 0x12345678u; k < 0x8000; k++) { x = x * 1664525u + 1013904223u; if (u[k] != x) bad++; if (w[k] != x) bad++; }
    for (k = 0; k < 0x8000; k++) w[k] = ~u[k];
    for (k = 0, x = 0x12345678u; k < 0x8000; k++) { x = x * 1664525u + 1013904223u; if (u[k] != ~x) bad++; }
    for (k = 0; k < 0x8000; k++) w[k] = 0;
    return bad;
}
static void jt_show(uint32_t spr_bad)
{
    char line[64], *p;
    int j, row = 0, pass = 0;
    big_init();
    p = put_s(line, "SPRITE RAM ");
    if (spr_bad) put_u(put_s(p, "BAD "), spr_bad); else put_s(p, "OK");
    big_text(row++, line);
    for (j = 0; j < NJOBS; j++) {
        int ok = JTR(j, 4) == jt_expect[j], r = jobs[j].route >= 0;
        static int ng, nr;
        pass += ok;
        p = line;
        *p++ = r ? 'R' : 'G';
        p = put_u(p, (uint32_t)(r ? ++nr : ++ng));
        p = put_s(p, " T ");
        p = put_u(p, JTR(j, 0));
        p = put_s(p, ok ? " OK" : " BAD");
        big_text(row++, line);
        if (r) {
            p = put_s(line, "  AV ");
            p = put_u(p, JTR(j, 1) ? JTR(j, 2) / JTR(j, 1) : 0);
            p = put_s(p, " X ");
            put_u(p, JTR(j, 3));
            big_text(row++, line);
        }
    }
    p = put_s(line, pass == NJOBS ? "PASS " : "FAIL ");
    p = put_u(p, (uint32_t)pass);
    p = put_s(p, "/");
    put_u(p, NJOBS);
    big_text(row++, line);
    cps3v_text(0, 27, JT_LABEL);
}
#endif

int main(void)
{
    int j;
    uint32_t sr;
#ifdef JT
    uint32_t spr_bad;
    cps3_init();                                          /* video, the text layer (VBlank masked again below) */
    cps3v_text(2, 2, "SPELUNKY CPS3 PLAYSH2 JTCPS3 TIMING: RUNNING");
    spr_bad = spr_test();
    for (j = 0; j < 64; j++) R32(PBASE + 4 * j) = 0;
    R32(PBASE + 4 * 60) = spr_bad;
#endif
    P_STATE = 0;
    P_NREC = 0;
    P_NJOB = NJOBS;
    P_OVF = 0;
    P_TAG = 0;
    P_PN = 0;
    P_PER = HEART;
    P_WANT = PROF_PERIOD ? PROF_KIND : 0xffffffffu;   /* no samples unless PROF */
    P_SKC = 0;
    P_SKIP = PROF_SKIP;
    P_WRAPF = PROF_WRAP;
    P_MAGIC = 0x50534832;
#ifdef ATTR
    for (uint32_t a = 0x04180000; a < 0x04190000; a += 4) R32(a) = 0;   /* attr.c's tables */
#endif
    __asm__ volatile("ldc %0, vbr" : : "r"(vtab));
    *(volatile uint16_t *)0xfffffe66 = 72;               /* VCRC: FRT output compare vector */
    *(volatile uint16_t *)0xfffffe68 = 73 << 8;          /* VCRD: FRT overflow vector */
    *(volatile uint16_t *)0xfffffe60 = 0x0f00;           /* IPRB: FRT priority 15 */
    *(volatile uint8_t *)0xfffffe16 = 0x01;              /* TCR: phi / 32 (MAME raises FRT interrupts at its
                                                            timeslices, up to about a frame late: a wrap every
                                                            2.1 M clocks leaves room for that) */
    {   /* output compare A every HEART ticks: the wrap count (now()) and the PROF sampler */
        uint32_t t = frc();
        P_EXTH = 0;
        P_LASTF = t;
        t += HEART;
        *(volatile uint8_t *)0xfffffe14 = (uint8_t)(t >> 8);
        *(volatile uint8_t *)0xfffffe15 = (uint8_t)t;
    }
    *(volatile uint8_t *)0xfffffe10 = 0x09;              /* TIER: OCIAE; bit 0 reads 1 */
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
        else run_route(j, jobs[j].seed, &routes[jobs[j].route], 30);
    }
#ifdef JT
    jt_show(spr_bad);
#endif
    P_STATE = 1;
    for (;;) ;
}
