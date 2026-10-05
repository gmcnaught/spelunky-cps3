/* GML compares of a double against a constant on the bits (no soft-float call): NLT(v, N(c)) and NGT(v, N(c)) are
 * gcmp_z(v - c) (pnum.h: 0 when |v - c| <= GML_EPS, NaN -1), and v - c rounds monotonically in v, so for a finite
 * constant c there are doubles L(c) = the largest v with v - c < -eps and H(c) = the smallest with v - c > eps:
 *   NLT(v, N(c)) == v is a NaN or v <= L(c);   NGT(v, N(c)) == v is not a NaN and v >= H(c).
 * The double order on the bits: key(u) = u's magnitude bits with the sign bit set for v >= +0, all bits inverted
 * for a negative v (-0 orders below +0; no threshold here is 0). L and H by tools (round-to-nearest binary search
 * over the keys); tests/cmpc checks each pair against the macros. num as double only: PLAY_FIXED and NUM_IS_CLASS
 * builds keep the macros (CLT / CGT below) */
#ifndef PCMPC_H
#define PCMPC_H
#include <stdint.h>

static inline uint64_t cmpc_bits(double v) { union { double d; uint64_t u; } c; c.d = v; return c.u; }
static inline uint64_t cmpc_key(uint64_t u) { return (u >> 63) ? ~u : u | 0x8000000000000000ull; }
static inline int cmpc_nan(uint64_t u) { return (u & 0x7fffffffffffffffull) > 0x7ff0000000000000ull; }
static inline int cmpc_lt(double v, uint64_t l)     /* NLT(v, N(c)), l = L(c)'s bits */
{
    uint64_t u = cmpc_bits(v);
    return cmpc_nan(u) || cmpc_key(u) <= cmpc_key(l);
}
static inline int cmpc_gt(double v, uint64_t h)     /* NGT(v, N(c)), h = H(c)'s bits */
{
    uint64_t u = cmpc_bits(v);
    return !cmpc_nan(u) && cmpc_key(u) >= cmpc_key(h);
}

/* L(c) / H(c) of the constants used (tests/cmpc's table) */
#define CMPC_L_6    0x4017fffd60e94ee3ull   /* 5.99999 */
#define CMPC_H_3    0x400800053e2d6239ull   /* 3.00001 */
#define CMPC_L_2    0x3ffffff583a53b8eull   /* 1.99999 */
#define CMPC_H_1    0x3ff0000a7c5ac472ull   /* 1.00001 */
#define CMPC_L_1    0x3fefffeb074a771cull   /* 0.99999 */
#define CMPC_L_M3   0xc00800053e2d6239ull   /* -3.00001 */
#define CMPC_L_0_1  0x3fb998f1d3ed527eull   /* 0.09999 */

#if defined(PLAY_FIXED) || defined(NUM_IS_CLASS)
#define CLT(v, c, l) NLT((v), N(c))
#define CGT(v, c, h) NGT((v), N(c))
#else
#define CLT(v, c, l) cmpc_lt((v), (l))
#define CGT(v, c, h) cmpc_gt((v), (h))
#endif
#endif
