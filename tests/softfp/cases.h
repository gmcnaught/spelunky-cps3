/* tests/softfp: the operand generator shared by the host test (host.c) and the SH-2 test (main.c), so both see the
   same cases in the same order. xorshift64*, then a mix of: random bit patterns; game-like magnitudes; exponents
   near the subnormal and overflow ends; special values (zeros, infinities, quiet and signalling NaNs with
   payloads, the extreme normals and subnormals); significands of all ones / single bits (carries); and second
   operands made from the first (the same exponent, a few apart, one unit in the last place apart, exponent
   differences up to 64) for cancellation, alignment and rounding ties. */
#ifndef SOFTFP_CASES_H
#define SOFTFP_CASES_H
#include <stdint.h>

static uint64_t cs_state = 0x9e3779b97f4a7c15ull;
static inline uint64_t cs_next(void)
{
    uint64_t x = cs_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    cs_state = x;
    return x * 0x2545f4914f6cdd1dull;
}
static inline uint32_t cs_u32(uint32_t n) { return (uint32_t)((cs_next() >> 32) % n); }

static const uint64_t cs_dspecial[] = {
    0x0000000000000000ull, 0x8000000000000000ull, 0x7ff0000000000000ull, 0xfff0000000000000ull,
    0x7ff8000000000000ull, 0xfff8000000000000ull, 0x7ff0000000000001ull, 0x7ff4000000000123ull,
    0xfff7ffffffffffffull, 0x7fffffffffffffffull, 0x0000000000000001ull, 0x8000000000000001ull,
    0x000fffffffffffffull, 0x0010000000000000ull, 0x001fffffffffffffull, 0x7fefffffffffffffull,
    0xffefffffffffffffull, 0x3ff0000000000000ull, 0xbff0000000000000ull, 0x3ff8000000000000ull,
    0x4000000000000000ull, 0x3fe0000000000000ull, 0x3ff0000000000001ull, 0x3fefffffffffffffull,
    0x4330000000000000ull, 0x41dfffffffc00000ull, 0xc1e0000000000000ull, 0x41e0000000000000ull,
    0x3ee4f8b588e368f1ull, 0x3fc3333333333333ull, 0x4070000000000000ull, 0x0008000000000000ull,
};
static const uint32_t cs_fspecial[] = {
    0x00000000u, 0x80000000u, 0x7f800000u, 0xff800000u, 0x7fc00000u, 0xffc00000u, 0x7f800001u, 0x7fa00123u,
    0x00000001u, 0x80000001u, 0x007fffffu, 0x00800000u, 0x7f7fffffu, 0xff7fffffu, 0x3f800000u, 0xbf800000u,
    0x3f800001u, 0x3f7fffffu, 0x4b000000u, 0x4f000000u, 0xcf000000u, 0x3ecccccdu, 0x43580000u, 0x00400000u,
};

static inline uint64_t cs_d1(void)
{
    uint64_t r = cs_next();
    uint32_t k = cs_u32(100), s = (uint32_t)(r >> 63), ex;
    uint64_t m = cs_next() & 0x000fffffffffffffull;
    if (k < 30) return r;
    if (k < 52) ex = 1000 + cs_u32(48);                         /* game-like: 2^-23 .. 2^24 */
    else if (k < 62) ex = cs_u32(4);                            /* subnormals, the least normals */
    else if (k < 70) ex = 0x7fb + cs_u32(5);                    /* overflow end, inf, NaN */
    else if (k < 82) return cs_dspecial[cs_u32(sizeof cs_dspecial / 8)] ^ ((uint64_t)cs_u32(2) << 63);
    else {                                                      /* significand patterns */
        uint32_t p = cs_u32(4);
        m = p == 0 ? 0x000fffffffffffffull : p == 1 ? 0 : p == 2 ? (1ull << cs_u32(52)) :
            (0x000fffffffffffffull >> cs_u32(52)) << cs_u32(12);
        m &= 0x000fffffffffffffull;
        ex = cs_u32(10) < 2 ? cs_u32(3) : 1000 + cs_u32(48);
    }
    return ((uint64_t)s << 63) | ((uint64_t)ex << 52) | m;
}

/* a second operand, often made from the first */
static inline uint64_t cs_d2(uint64_t a)
{
    uint32_t k = cs_u32(100);
    uint64_t sgn = (uint64_t)cs_u32(2) << 63;
    if (k < 45) return cs_d1();
    if (k < 55) return (a & ~0x8000000000000000ull) ^ sgn;                 /* +-a: x - x, x + x */
    if (k < 70) return ((a & ~0x8000000000000000ull) + (int64_t)(cs_u32(5)) - 2) ^ sgn; /* ulps apart */
    if (k < 90) {                                                         /* exponent d below, random significand */
        int ex = (int)((a >> 52) & 0x7ff) - (int)cs_u32(66);
        if (ex < 0) ex = 0;
        uint64_t m = cs_u32(3) ? (cs_next() & 0x000fffffffffffffull) : (a & 0x000fffffffffffffull);
        return sgn | ((uint64_t)ex << 52) | m;
    }
    /* a tie candidate: a one-bit significand 53 or 54 places below a */
    {
        int ex = (int)((a >> 52) & 0x7ff) - 52 - (int)cs_u32(3);
        if (ex < 0) ex = 0;
        return sgn | ((uint64_t)ex << 52) | (cs_u32(2) ? 0 : (1ull << cs_u32(52)));
    }
}

static inline uint32_t cs_f1(void)
{
    uint32_t r = (uint32_t)(cs_next() >> 32), k = cs_u32(100), s = r >> 31, ex, m = (uint32_t)cs_next() & 0x7fffff;
    if (k < 30) return r;
    if (k < 52) ex = 110 + cs_u32(40);
    else if (k < 62) ex = cs_u32(4);
    else if (k < 70) ex = 0xfb + cs_u32(5);
    else if (k < 82) return cs_fspecial[cs_u32(sizeof cs_fspecial / 4)] ^ (cs_u32(2) << 31);
    else {
        uint32_t p = cs_u32(4);
        m = p == 0 ? 0x7fffff : p == 1 ? 0 : p == 2 ? (1u << cs_u32(23)) : (0x7fffffu >> cs_u32(23)) << cs_u32(6);
        m &= 0x7fffff;
        ex = cs_u32(10) < 2 ? cs_u32(3) : 110 + cs_u32(40);
    }
    return (s << 31) | (ex << 23) | m;
}

static inline uint32_t cs_f2(uint32_t a)
{
    uint32_t k = cs_u32(100), sgn = cs_u32(2) << 31;
    if (k < 45) return cs_f1();
    if (k < 55) return (a & 0x7fffffffu) ^ sgn;
    if (k < 70) return ((a & 0x7fffffffu) + cs_u32(5) - 2) ^ sgn;
    {
        int ex = (int)((a >> 23) & 0xff) - (int)cs_u32(30);
        if (ex < 0) ex = 0;
        uint32_t m = cs_u32(3) ? ((uint32_t)cs_next() & 0x7fffff) : (a & 0x7fffff);
        return sgn | ((uint32_t)ex << 23) | m;
    }
}

/* an int32 operand: random, small, near the powers of two and the float rounding range (2^24 .. 2^31) */
static inline int32_t cs_i32(void)
{
    uint32_t k = cs_u32(4), r = (uint32_t)(cs_next() >> 32);
    if (k == 0) return (int32_t)r;
    if (k == 1) return (int32_t)(r % 2001) - 1000;
    if (k == 2) return (int32_t)((1u << cs_u32(32)) + cs_u32(5) - 2);
    return (int32_t)(r >> cs_u32(9));
}

/* the operations, in the order of the case table */
enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE, OP_UNORD, OP_EXT, OP_TRUNC,
       OP_I2D, OP_U2D, OP_D2I, OP_FADD, OP_FSUB, OP_FMUL, OP_FDIV, OP_FEQ, OP_FNE, OP_FLT, OP_FLE, OP_FGT, OP_FGE,
       OP_I2F, OP_F2I, OP_N };
static const char *const cs_opname[OP_N] = { "adddf3", "subdf3", "muldf3", "divdf3", "eqdf2", "nedf2", "ltdf2",
    "ledf2", "gtdf2", "gedf2", "unorddf2", "extendsfdf2", "truncdfsf2", "floatsidf", "floatunsidf", "fixdfsi",
    "addsf3", "subsf3", "mulsf3", "divsf3", "eqsf2", "nesf2", "ltsf2", "lesf2", "gtsf2", "gesf2", "floatsisf",
    "fixsfsi" };
#endif
