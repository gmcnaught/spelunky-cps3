/* fmaf for the SH-2 (the toolchain has no libm): x * y + z with one rounding (round to nearest, ties to even),
 * IEEE binary32, subnormals in and out. Integers only (no soft-float calls): the 48-bit product and z are added in
 * a 64-bit frame whose top bit is the larger operand's, the smaller one shifted right with a sticky bit, then one
 * rounding to the float's precision (24 bits, fewer when the result is subnormal).
 * Used by src/game/pcol.c (the runner's fmadd / fnmsub in the collision tree). Checked against the host libm by
 * tests/fma (FMAF_NAME renames the function there). NaN results are the default quiet NaN (no payload). */
#include <stdint.h>

#ifndef FMAF_NAME
#define FMAF_NAME fmaf
#endif

float FMAF_NAME(float x, float y, float z);

static uint32_t f2u(float f) { union { float f; uint32_t u; } v; v.f = f; return v.u; }
static float u2f(uint32_t u) { union { float f; uint32_t u; } v; v.u = u; return v.f; }

/* the number of bits of v (v != 0) */
static int blen64(uint64_t v)
{
    int n = 0;
    uint32_t h = (uint32_t)(v >> 32), w;
    if (h) { n = 32; w = h; } else w = (uint32_t)v;
    if (w >= 0x10000u) { n += 16; w >>= 16; }
    if (w >= 0x100u) { n += 8; w >>= 8; }
    if (w >= 0x10u) { n += 4; w >>= 4; }
    if (w >= 0x4u) { n += 2; w >>= 2; }
    if (w >= 0x2u) { n += 1; w >>= 1; }
    return n + (int)w;
}

/* v >> s with the bits shifted out ORed into bit 0 (s >= 0) */
static uint64_t shr_jam(uint64_t v, int s)
{
    if (s == 0) return v;
    if (s >= 64) return v != 0;
    return (v >> s) | ((v & (((uint64_t)1 << s) - 1)) != 0);
}

float FMAF_NAME(float x, float y, float z)
{
    uint32_t ux = f2u(x), uy = f2u(y), uz = f2u(z);
    uint32_t sp = (ux ^ uy) >> 31, sz = uz >> 31, ex = (ux >> 23) & 255, ey = (uy >> 23) & 255, ez = (uz >> 23) & 255;
    uint32_t mx = ux & 0x7fffff, my = uy & 0x7fffff, mz = uz & 0x7fffff;
    uint64_t p, a, b, r, q, rem, half;
    int lp, lz, tp, tz, ep, ezz, ea, eb, la, sr, top, lsb, sh;
    uint32_t sa, sb, bits;

    /* NaN, infinity */
    if (ex == 255 || ey == 255 || ez == 255) {
        int pnan = (ex == 255 && mx) || (ey == 255 && my);
        int pinf = (ex == 255 || ey == 255);
        if (pnan || (ez == 255 && mz)) return u2f(0x7fc00000);
        if (pinf) {
            if ((ex == 0 && mx == 0) || (ey == 0 && my == 0)) return u2f(0x7fc00000);     /* inf * 0 */
            if (ez == 255 && sz != sp) return u2f(0x7fc00000);                            /* inf - inf */
            return u2f((sp << 31) | 0x7f800000);
        }
        return z;                                                                          /* finite + inf */
    }
    /* a zero product: exact, its sign sp; x * y + z = z unless z is zero too */
    if ((ex == 0 && mx == 0) || (ey == 0 && my == 0)) {
        if (ez == 0 && mz == 0) return u2f(sp == sz ? sp << 31 : 0);                       /* +0 unless both -0 */
        return z;
    }
    /* significands and the exponents of their lowest bits */
    if (ex) mx |= 0x800000; else ex = 1;
    if (ey) my |= 0x800000; else ey = 1;
    p = (uint64_t)mx * my;
    ep = (int)ex + (int)ey - 300;                       /* (ex - 150) + (ey - 150) */
    lp = blen64(p);
    tp = ep + lp - 1;                                    /* the product's top bit */
    if (ez == 0 && mz == 0) {
        a = p; ea = ep; la = lp; sa = sp; b = 0; eb = 0; sb = 0;
    } else {
        uint64_t zz;
        if (ez) mz |= 0x800000; else ez = 1;
        zz = mz;
        ezz = (int)ez - 150;
        lz = blen64(zz);
        tz = ezz + lz - 1;
        if (tp >= tz) { a = p; ea = ep; la = lp; sa = sp; b = zz; eb = ezz; sb = sz; }
        else { a = zz; ea = ezz; la = lz; sa = sz; b = p; eb = ep; sb = sp; }
    }
    /* a's top bit to bit 62; b to the same lowest exponent (b's top is not above a's: a left shift fits, a right
       shift only drops bits more than 62 places below a's top, kept as a sticky bit) */
    a <<= 62 - (la - 1);
    ea -= 62 - (la - 1);
    if (b) {
        sr = eb - ea;
        if (sr >= 0) b <<= sr;
        else b = shr_jam(b, -sr);
    }
    if (sa == sb) r = a + b;                             /* at most 2^63 + 2^63 - 1: fits */
    else if (a >= b) r = a - b;
    else { r = b - a; sa = sb; }
    if (r == 0) return u2f(0);                           /* exact cancellation: +0 */
    /* one rounding: the result's lowest bit is 23 below its top, not below 2^-149 */
    top = ea + blen64(r) - 1;
    lsb = top - 23;
    if (lsb < -149) lsb = -149;
    sh = lsb - ea;
    if (sh <= 0) {
        q = r << -sh;                                    /* exact (fewer than 24 bits) */
    } else {
        if (sh > 64) { q = 0; rem = 1; half = 2; }       /* below half of the lowest subnormal */
        else if (sh == 64) { q = 0; rem = r; half = (uint64_t)1 << 63; }
        else { q = r >> sh; rem = r & (((uint64_t)1 << sh) - 1); half = (uint64_t)1 << (sh - 1); }
        if (rem > half || (rem == half && (q & 1))) q++;
    }
    /* q < 2^23 only when subnormal (lsb = -149: biased exponent field 0); q = 2^24 after a carry: the next
       exponent. (E - 1) << 23 + q covers the three */
    {
        int e1 = lsb + 149;                              /* (lsb + 150) - 1 */
        uint32_t qq = (uint32_t)q;
        if (e1 > 254) return u2f((sa << 31) | 0x7f800000);
        bits = ((uint32_t)e1 << 23) + qq;
        if (bits >= 0x7f800000) bits = 0x7f800000;       /* overflow: infinity (round to nearest) */
    }
    return u2f((sa << 31) | bits);
}
