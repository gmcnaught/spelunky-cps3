/* tests/softfp: one case of an operation through a soft-float implementation (the SF_* macros name it), and the
   canonical form of its result that the host's FPU must give too: arithmetic and conversion results as bits with
   every NaN as the default NaN; compares as the truth of the relation GCC tests (eq: == 0, lt: < 0, ...). The
   host (host.c) and the SH-2 (main.c) both hash these per operation; equal hashes = equal to the host's FPU. */
#ifndef SOFTFP_CHECK_H
#define SOFTFP_CHECK_H
#include "cases.h"

struct cs_case { uint64_t a, b, c; };

/* binary32 to binary64, exact (normal, subnormal, zero, infinity; a NaN keeps its payload): the case builders' */
static inline uint64_t cs_widen(uint32_t f)
{
    uint32_t e = (f >> 23) & 0xff, m = f & 0x7fffff;
    uint64_t s = (uint64_t)(f >> 31) << 63;
    if (e == 0xff) return s | 0x7ff0000000000000ull | ((uint64_t)m << 29);
    if (e == 0) {
        if (!m) return s;
        e = 1;
        while (!(m & 0x800000)) { m <<= 1; e--; }
        m &= 0x7fffff;
    }
    return s | ((uint64_t)(e + 896) << 52) | ((uint64_t)m << 29);
}

/* a binary64 near (2 k + 1) 2^(x - 1) (x a binary64 biased exponent): y + v then lies near a binary32 rounding tie of
   y when 2^x is y's float ulp (j units in the last place off the exact half) */
static inline uint64_t cs_half(int x, uint32_t k, int j)
{
    uint64_t m = 2 * (uint64_t)k + 1;
    int e = x - 1 + 52;                               /* m 2^(x - 1 - 1023) = (m 2^-52) 2^(e - 1023) */
    while (!(m & 0x0010000000000000ull)) { m <<= 1; e--; }
    if (e < 1) e = 1;
    return (((uint64_t)e << 52) | (m & 0x000fffffffffffffull)) + (uint64_t)(int64_t)j;
}
/* (3% of the double pairs are carry / sticky shapes, 3% long cancellations one binade apart; floats too) */

static inline uint64_t canon_d(uint64_t r) { return ((r & 0x7fffffffffffffffull) > 0x7ff0000000000000ull) ? 0x7ff8000000000000ull : r; }
static inline uint32_t canon_f(uint32_t r) { return ((r & 0x7fffffffu) > 0x7f800000u) ? 0x7fc00000u : r; }

/* the operands of case k of op (drawn in order: call once per case) */
static inline struct cs_case cs_case_for(int op)
{
    struct cs_case c;
    switch (op) {
    case OP_EXT: case OP_FADD: case OP_FSUB: case OP_FMUL: case OP_FDIV: case OP_FEQ: case OP_FNE: case OP_FLT:
    case OP_FLE: case OP_FGT: case OP_FGE: case OP_F2I:
        if (cs_u32(100) < 3) {                       /* the same shape in binary32 */
            uint32_t ex = 2 + cs_u32(250), s = cs_u32(2) << 31;
            c.a = s | (ex << 23) | ((uint32_t)cs_next() & ((1u << cs_u32(23)) - 1));
            c.b = (s ^ 0x80000000u) | ((ex - 1) << 23) |
                  ((0x7fffffu - ((uint32_t)cs_next() & ((1u << cs_u32(23)) - 1))) | cs_u32(2));
            break;
        }
        c.a = cs_f1();
        c.b = cs_f2((uint32_t)c.a);
        break;
    case OP_PIECEY: case OP_PIECEV: {                /* a float y, its velocity v (b), the acceleration (c) */
        uint32_t y = cs_f1(), k = cs_u32(8), ey = (y >> 23) & 0xff;
        uint64_t sg = (uint64_t)cs_u32(2) << 63;
        c.a = y;
        if (k < 2) c.b = cs_d1();
        else if (k < 4) c.b = cs_d2(cs_widen(y));
        else if (k < 7 && ey > 1 && ey < 0xfe)        /* near y's float rounding ties, either sign */
            c.b = sg | cs_half((int)ey + 896 - 23, cs_u32(4) ? cs_u32(16) : cs_u32(1u << 20), (int)cs_u32(5) - 2);
        else c.b = sg | ((uint64_t)(1000 + cs_u32(30)) << 52) | (cs_next() & 0x000fffffffffffffull);
        k = cs_u32(4);
        c.c = k == 0 ? cs_d1() : k == 1 ? cs_d2(c.b) : k == 2 ? 0x3fe3333333333333ull :     /* 0.6 */
              ((uint64_t)(1015 + cs_u32(10)) << 52) | (cs_next() & 0x000fffffffffffffull);
        {   /* half the velocity cases the previous case's v and acceleration (the memo's hits) */
            static uint64_t pb, pc;
            if (op == OP_PIECEV && cs_u32(2)) { c.b = pb; c.c = pc; }
            pb = c.b; pc = c.c;
        }
        break;
    }
    case OP_TRUNC:                                   /* a third near the rounding: the 29 bits below a tie, 0, all
                                                        ones; exponents at the float's normal ends */
        if (cs_u32(3) == 0) {
            static const uint32_t lo[] = { 0x10000000u, 0x0fffffffu, 0x10000001u, 0, 0x1fffffffu, 1 };
            uint32_t k = cs_u32(4);
            uint64_t ex = k == 0 ? 897 + cs_u32(3) : k == 1 ? 1148 + cs_u32(3) : k == 2 ? 896 - cs_u32(2) : 900 + cs_u32(240);
            uint64_t m = cs_u32(4) ? cs_next() : 0xfffffffffffffull;
            c.a = ((uint64_t)cs_u32(2) << 63) | (ex << 52) | (m & 0x000fffffe0000000ull) | lo[cs_u32(6)];
            c.b = 0;
            break;
        }
        c.a = cs_d1();
        c.b = cs_d2(c.a);
        break;
    case OP_I2D: case OP_U2D: case OP_I2F:
        c.a = (uint32_t)cs_i32();
        c.b = 0;
        break;
    default:
        if (cs_u32(100) < 3) {                       /* a carry with a sticky bit: a's significand near all ones, b
                                                        32..63 binades below with a few low bits */
            uint64_t ex = 1 + cs_u32(2000), s = (uint64_t)cs_u32(2) << 63;
            c.a = s | (ex << 52) | (0x000fffffffffffffull - cs_u32(4));
            uint32_t d = 32 + cs_u32(32), lo = cs_u32(32);
            c.b = s | ((ex > d ? ex - d : 1) << 52) | (uint64_t)(cs_next() & ((1ull << lo) - 1)) | ((uint64_t)cs_u32(4) << 50);
            if (cs_u32(4) == 0) c.b ^= 0x8000000000000000ull;
            break;
        }
        if (cs_u32(100) < 3) {                       /* a long cancellation one binade apart: a = 2^e (1 + small),
                                                        b = -2^(e - 1) (2 - small) with its last bits set */
            uint64_t ex = 2 + cs_u32(2000), s = (uint64_t)cs_u32(2) << 63;
            c.a = s | (ex << 52) | (cs_next() & ((1ull << cs_u32(52)) - 1));
            c.b = (s ^ 0x8000000000000000ull) | ((ex - 1) << 52) |
                  ((0x000fffffffffffffull - (cs_next() & ((1ull << cs_u32(52)) - 1))) | cs_u32(2));
            break;
        }
        c.a = cs_d1();
        c.b = cs_d2(c.a);
        break;
    }
    return c;
}

/* compare results: the relation GCC derives from the return value */
static inline uint64_t rel(int op, int r)
{
    switch (op) {
    case OP_EQ: case OP_FEQ: return r == 0;
    case OP_NE: case OP_FNE: return r != 0;
    case OP_LT: case OP_FLT: return r < 0;
    case OP_LE: case OP_FLE: return r <= 0;
    case OP_GT: case OP_FGT: return r > 0;
    case OP_GE: case OP_FGE: return r >= 0;
    default: return r != 0;
    }
}
#endif
