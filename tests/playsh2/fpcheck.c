/* tests/playsh2 FPCHECK builds: every call of softfp_sh2.S's entry points (linker wraps) also goes through
   softfp.c's C versions (fpref.c: sf_* names, no SOFTFP_ASM); a different answer is logged to character RAM at
   0x041a0000: count, then per entry op, a hi, a lo, b hi, b lo, asm hi, asm lo, C hi, C lo (at most 64). */
#include <stdint.h>

#define LOG ((volatile uint32_t *)0x041a0000)
static void put(int op, uint64_t a, uint64_t b, uint64_t r, uint64_t c)
{
    uint32_t n = LOG[0];
    if (n < 64) {
        volatile uint32_t *e = LOG + 1 + 9 * n;
        e[0] = (uint32_t)op;
        e[1] = (uint32_t)(a >> 32); e[2] = (uint32_t)a; e[3] = (uint32_t)(b >> 32); e[4] = (uint32_t)b;
        e[5] = (uint32_t)(r >> 32); e[6] = (uint32_t)r; e[7] = (uint32_t)(c >> 32); e[8] = (uint32_t)c;
    }
    LOG[0] = n + 1;
}

#define D2(op, n)                                                                                            \
    uint64_t __real___##n(uint64_t, uint64_t); uint64_t sf_##n(uint64_t, uint64_t);                          \
    uint64_t __wrap___##n(uint64_t a, uint64_t b)                                                             \
    { uint64_t r = __real___##n(a, b), c = sf_##n(a, b); if (r != c) put(op, a, b, r, c); return r; }
#define F2(op, n)                                                                                            \
    uint32_t __real___##n(uint32_t, uint32_t); uint32_t sf_##n(uint32_t, uint32_t);                          \
    uint32_t __wrap___##n(uint32_t a, uint32_t b)                                                             \
    { uint32_t r = __real___##n(a, b), c = sf_##n(a, b); if (r != c) put(op, a, b, r, c); return r; }
#define C2(op, n)                                                                                            \
    int __real___##n(uint64_t, uint64_t); int sf_##n(uint64_t, uint64_t);                                    \
    int __wrap___##n(uint64_t a, uint64_t b)                                                                  \
    { int r = __real___##n(a, b), c = sf_##n(a, b); if (r != c) put(op, a, b, (uint64_t)(int64_t)r, (uint64_t)(int64_t)c); return r; }
D2(1, adddf3) D2(2, subdf3) F2(3, addsf3) F2(4, subsf3)
C2(5, eqdf2) C2(6, nedf2) C2(7, ltdf2) C2(8, ledf2) C2(9, gtdf2) C2(10, gedf2)
