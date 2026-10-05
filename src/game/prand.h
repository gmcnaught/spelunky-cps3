/* random(n) = u * 2^-32 * n (pscript.c prandom; u the generator's 32 bits) without the soft-float conversion and
 * scaling: u * 2^-32 is exact in binary64 (u has at most 32 significant bits), so its bits are u's normalized: the
 * leading one dropped, the rest at the top of the 52-bit mantissa, the exponent -1 - (u's leading zeros). For n a
 * positive power of two (normal) the product with n is exact as well: the exponent plus n's. Any other n: one
 * multiplication, as before. Every result is the double of the expression (tests/prand: all 2^32 u for n = 1, 2, 4,
 * 8, 16, 32, 64, 128, 0.5 and a random sample for other n) */
#ifndef PRAND_H
#define PRAND_H
#include <stdint.h>

static inline double prand_bits(uint64_t b) { union { double d; uint64_t u; } c; c.u = b; return c.d; }

static inline double prand_scale(uint32_t u, double n)
{
    union { double d; uint64_t u; } c;
    uint64_t m, nb;
    int lz = 0, e;
    if (u == 0) return 0.0 * n;
    if (!(u & 0xffff0000u)) { lz += 16; u <<= 16; }
    if (!(u & 0xff000000u)) { lz += 8; u <<= 8; }
    if (!(u & 0xf0000000u)) { lz += 4; u <<= 4; }
    if (!(u & 0xc0000000u)) { lz += 2; u <<= 2; }
    if (!(u & 0x80000000u)) { lz += 1; u <<= 1; }
    m = ((uint64_t)(u & 0x7fffffffu)) << 21;                /* the 31 bits after the leading one, at the top */
    e = 1022 - lz;                                           /* biased exponent of u * 2^-32 */
    c.d = n;
    nb = c.u;
    if ((nb >> 63) == 0 && (nb & 0x000fffffffffffffull) == 0) {
        int en = (int)(nb >> 52);                            /* n = 2^(en - 1023), normal, positive */
        if (en > 0 && en < 0x7ff && e + en - 1023 > 0 && e + en - 1023 < 0x7ff)
            return prand_bits(((uint64_t)(e + en - 1023) << 52) | m);
    }
    return prand_bits(((uint64_t)e << 52) | m) * n;
}
#endif
