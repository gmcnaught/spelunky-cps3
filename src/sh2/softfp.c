/* IEEE-754 binary64 / binary32 soft-float for the SH-2 (no FPU), replacing libgcc's fp-bit routines: the same entry
 * points and calling convention (a double travels as two 32-bit words, high word first; a float as one word), so a
 * program links these instead of libgcc's (an object file comes before -lgcc).
 *
 * Results: round to nearest, ties to even, correctly rounded for every finite operand (subnormals included), so
 * the bits equal any IEEE implementation's (fp-bit's, the host's FPU). Where IEEE leaves the result open this
 * follows fp-bit (GCC libgcc/fp-bit.c), so a program sees what it saw with fp-bit:
 *   - a NaN operand gives that NaN quieted (fraction bit 51 / 22 set, payload and sign kept), the first operand's
 *     when both are NaN; subtraction flips the second operand's sign first (a NaN's too); multiplication gives
 *     the NaN the sign a.sign ^ b.sign; an invalid operation (inf - inf, 0 * inf, 0 / 0, inf / inf) gives
 *     0x7ff8000000000000 / 0x7fc00000 (fp-bit's __thenan_df / __thenan_sf);
 *   - compares return -1 / 0 / 1 as fp-bit's __fpcmp_parts; with a NaN: eq, ne, lt, le 1; gt, ge -1; unord 1;
 *   - (int) of a NaN is 0; of an infinity or a value outside int32, INT32_MIN / INT32_MAX by sign.
 * Checked against fp-bit on the SH-2 and against the host's FPU: tests/softfp (scripts/softfp_check.sh).
 *
 * Speed (the SH-2 shifts only by constants: a variable shift is a library call): normal operands with a normal
 * result take a fast path on 32-bit halves, variable shifts as dmulu.l products by 2^k; division with the SH7604
 * divide unit (DIVU) in two 29-bit quotient steps; compares on the bit patterns. Other operands take a general
 * path (unpack, 64-bit significand with sticky bit, one rounding).
 *
 * SOFTFP_HOST: the functions are named sf_* and divide in C (the host test); SOFTFP_SFNAMES: named sf_* on the SH-2
 * (tests/softfp, beside libgcc's); otherwise __adddf3 ... (libgcc's names). */
#include <stdint.h>

#if defined(SOFTFP_HOST) || defined(SOFTFP_SFNAMES)
#define SF(n) sf_##n
#else
#define SF(n) __##n
#endif
#define HOT __attribute__((used))
#ifndef SOFTFP_HOST
#define SOFTFP_DIVU 1
#endif

typedef uint64_t f64;
typedef uint32_t f32;

#define D_QNAN 0x7ff8000000000000ull
#define D_QBIT 0x0008000000000000ull
#define D_SIGN 0x8000000000000000ull
#define D_INF  0x7ff0000000000000ull
#define D_FRAC 0x000fffffffffffffull
#define D_IMPL 0x0010000000000000ull
#define F_QNAN 0x7fc00000u
#define F_INF  0x7f800000u

#define HI(a) ((uint32_t)((a) >> 32))
#define LO(a) ((uint32_t)(a))
#define MK(h, l) (((uint64_t)(uint32_t)(h) << 32) | (uint32_t)(l))

/* 2^k, k = 0..31: x << k = x * 2^k; x >> k = the high word of x * 2^(32 - k) */
static const uint32_t p2[32] = {
    1u << 0, 1u << 1, 1u << 2, 1u << 3, 1u << 4, 1u << 5, 1u << 6, 1u << 7, 1u << 8, 1u << 9, 1u << 10, 1u << 11,
    1u << 12, 1u << 13, 1u << 14, 1u << 15, 1u << 16, 1u << 17, 1u << 18, 1u << 19, 1u << 20, 1u << 21, 1u << 22,
    1u << 23, 1u << 24, 1u << 25, 1u << 26, 1u << 27, 1u << 28, 1u << 29, 1u << 30, 1u << 31,
};

static inline int clz32(uint32_t v)                /* v != 0 */
{
    int n = 0;
    if (!(v & 0xffff0000u)) { n += 16; v <<= 16; }
    if (!(v & 0xff000000u)) { n += 8; v <<= 8; }
    if (!(v & 0xf0000000u)) { n += 4; v <<= 4; }
    if (!(v & 0xc0000000u)) { n += 2; v <<= 2; }
    if (!(v & 0x80000000u)) n += 1;
    return n;
}

static inline int clz64(uint64_t v)                /* v != 0 */
{
    return HI(v) ? clz32(HI(v)) : 32 + clz32(LO(v));
}

/* v << k, 0 <= k < 64 */
static uint64_t shl64(uint64_t v, int k)
{
    uint32_t h = HI(v), l = LO(v);
    if (k == 0) return v;
    if (k >= 32) return MK(l * p2[k - 32], 0);
    uint64_t q = (uint64_t)l * p2[k];
    return MK(h * p2[k] | HI(q), LO(q));
}

/* v >> k, any k >= 0, the bits shifted out ORed into bit 0 (sticky) */
static uint64_t shr64s(uint64_t v, int k)
{
    uint32_t h = HI(v), l = LO(v);
    if (k == 0) return v;
    if (k >= 64) return v != 0;
    if (k == 32) return (uint64_t)h | (l != 0);
    if (k > 32) {
        uint64_t q = (uint64_t)h * p2[64 - k];
        return (uint64_t)HI(q) | ((l | LO(q)) != 0);
    }
    uint64_t p = (uint64_t)l * p2[32 - k], q = (uint64_t)h * p2[32 - k];
    return MK(HI(q), HI(p) | LO(q)) | (LO(p) != 0);
}

static inline int isnan_d(f64 a) { return (a & ~D_SIGN) > D_INF; }
static inline int isnan_f(f32 a) { return (a & 0x7fffffffu) > F_INF; }

/* ---- binary64: the general path -------------------------------------------------------------------------- */

/* sign s, exponent e (biased, of bit 62 of m), significand m (any nonzero; normalised here); the bits below the
   53-bit result are round and sticky. Round to nearest even; subnormal and overflowing results included */
static f64 pack_d(uint32_t s, int e, uint64_t m)
{
    uint64_t sign = (uint64_t)s << 63;
    if (m == 0) return sign;
    int z = clz64(m) - 1;
    if (z > 0) { m = shl64(m, z); e -= z; }
    else if (z < 0) { m = shr64s(m, 1); e += 1; }
    if (e <= 0) {                                   /* subnormal: exponent 1 with the significand moved right */
        m = shr64s(m, 1 - e);
        e = 0;
    }
    uint32_t r = LO(m) & 0x3ff;
    uint64_t q = MK(HI(m) >> 10, (LO(m) >> 10) | (HI(m) << 22));
    if (r > 0x200 || (r == 0x200 && (LO(q) & 1))) q++;
    if (e == 0) return sign | q;                    /* q = 2^52: the smallest normal, encoded by the carry */
    if (q >> 53) { q >>= 1; e++; }
    if (e >= 0x7ff) return sign | D_INF;
    return sign | (MK((uint32_t)(e - 1) << 20, 0) + q);
}

/* the significand with its leading 1 at bit 52; *e such that the value is m * 2^(*e - 1075). a finite, not zero */
static uint64_t unpk_d(f64 a, int *e)
{
    int ex = (int)(HI(a) >> 20) & 0x7ff;
    uint64_t m = a & D_FRAC;
    if (ex == 0) {
        int z = clz64(m) - 11;
        *e = 1 - z;
        return shl64(m, z);
    }
    *e = ex;
    return m | D_IMPL;
}

static f64 qnan_d(f64 a) { return a | D_QBIT; }

static f64 add_gen(f64 a, f64 b)                   /* finite operands */
{
    uint32_t sa = HI(a) >> 31, sb = HI(b) >> 31;
    if ((a << 1) == 0) {
        if ((b << 1) == 0) return (uint64_t)(sa & sb) << 63;
        return b;
    }
    if ((b << 1) == 0) return a;
    int ea, eb;
    uint64_t ma = unpk_d(a, &ea), mb = unpk_d(b, &eb);
    if (ea < eb || (ea == eb && ma < mb)) {
        uint64_t t = ma; ma = mb; mb = t;
        int te = ea; ea = eb; eb = te;
        uint32_t ts = sa; sa = sb; sb = ts;
    }
    ma = shl64(ma, 9);                              /* leading 1 at bit 61 */
    mb = shr64s(shl64(mb, 9), ea - eb);
    uint64_t m = sa == sb ? ma + mb : ma - mb;
    if (m == 0) return 0;                           /* x - x = +0 */
    return pack_d(sa, ea + 1, m);
}

static f64 add_special(f64 a, f64 b)               /* an operand is NaN or infinite */
{
    if (isnan_d(a)) return qnan_d(a);
    if (isnan_d(b)) return qnan_d(b);
    if ((a << 1) == (D_INF << 1)) {
        if ((b << 1) == (D_INF << 1) && a != b) return D_QNAN;
        return a;
    }
    return b;
}

/* ---- binary64 entry points ------------------------------------------------------------------------------- */

HOT f64 SF(adddf3)(f64 a, f64 b)
{
    uint32_t ah = HI(a), al = LO(a), bh = HI(b), bl = LO(b);
    int ea = (ah >> 20) & 0x7ff, eb = (bh >> 20) & 0x7ff;
    if (ea == 0x7ff || eb == 0x7ff) return add_special(a, b);
    if (ea == 0 || eb == 0) {
        if (((bh << 1) | bl) == 0 && ea != 0) return a;  /* a + (+-0), a normal (pin_bbox: x + 1 * 0) */
        if (((ah << 1) | al) == 0 && eb != 0) return b;
        return add_gen(a, b);
    }
    uint32_t sa = ah >> 31, sb = bh >> 31;
    /* 53-bit significands << 10: (h:l) with the leading 1 at bit 30 of h */
    uint32_t mah = (((ah & 0xfffff) | 0x100000) << 10) | (al >> 22), mal = al << 10;
    uint32_t mbh = (((bh & 0xfffff) | 0x100000) << 10) | (bl >> 22), mbl = bl << 10;
    if (ea < eb || (ea == eb && (mah < mbh || (mah == mbh && mal < mbl)))) {
        uint32_t t;
        t = mah; mah = mbh; mbh = t;
        t = mal; mal = mbl; mbl = t;
        int te = ea; ea = eb; eb = te;
        t = sa; sa = sb; sb = t;
    }
    int d = ea - eb;
    if (d >= 64) {
        mbl = 1;                                    /* (mbh | mbl) != 0: the sticky bit alone */
        mbh = 0;
    } else if (d >= 32) {
        int k = d - 32;
        uint64_t q = k ? (uint64_t)mbh * p2[32 - k] : (uint64_t)mbh << 32;
        uint32_t lost = mbl | LO(q);
        mbl = HI(q) | (lost != 0);
        mbh = 0;
    } else if (d > 0) {                             /* (mbh:mbl) >> d as two products by 2^(32 - d) */
        uint64_t p = (uint64_t)mbl * p2[32 - d], q = (uint64_t)mbh * p2[32 - d];
        mbl = HI(p) | LO(q) | (LO(p) != 0);
        mbh = HI(q);
    }
    uint32_t mh, ml;
    int e = ea;
    if (sa == sb) {
        ml = mal + mbl;
        mh = mah + mbh + (ml < mal);
        if (mh & 0x80000000u) {                     /* carry into bit 63: back to bit 62, keep the sticky bit */
            ml = (ml >> 1) | (mh << 31) | (ml & 1);
            mh >>= 1;
            e++;
        }
    } else {
        ml = mal - mbl;
        mh = mah - mbh - (mal < mbl);
        if ((mh | ml) == 0) return 0;               /* x - x = +0 */
        int z = mh ? clz32(mh) - 1 : 31 + clz32(ml);
        if (z >= 32) {
            mh = ml * p2[z - 32];
            ml = 0;
        } else if (z > 0) {
            uint64_t q = (uint64_t)ml * p2[z];
            mh = mh * p2[z] | HI(q);
            ml = LO(q);
        }
        e -= z;
        if (e <= 0) return add_gen(a, b);           /* subnormal result */
    }
    uint32_t r = ml & 0x3ff;
    uint32_t rl = (ml >> 10) | (mh << 22), rh = mh >> 10;
    if (r > 0x200 || (r == 0x200 && (rl & 1))) {
        rl++;
        if (rl == 0) {
            rh++;
            if (rh == 0x200000u) {                  /* rounded up to 2^53 */
                rh = 0x100000u;
                e++;
            }
        }
    }
    if (e >= 0x7ff) return MK((sa << 31) | 0x7ff00000u, 0);
    return MK((sa << 31) | ((uint32_t)e << 20) | (rh & 0xfffff), rl);
}

HOT f64 SF(subdf3)(f64 a, f64 b) { return SF(adddf3)(a, b ^ D_SIGN); }

static f64 mul_gen(f64 a, f64 b)                   /* finite, neither zero */
{
    uint32_t s = (HI(a) ^ HI(b)) >> 31;
    int ea, eb;
    uint64_t ma = unpk_d(a, &ea), mb = unpk_d(b, &eb);
    uint32_t al = LO(ma), ah = HI(ma), bl = LO(mb), bh = HI(mb);
    uint64_t p0 = (uint64_t)al * bl, p1 = (uint64_t)al * bh, p2_ = (uint64_t)ah * bl, p3 = (uint64_t)ah * bh;
    uint64_t mid = (uint64_t)HI(p0) + LO(p1) + LO(p2_);
    uint32_t w0 = LO(p0), w1 = LO(mid);
    uint64_t hi = p3 + HI(p1) + HI(p2_) + HI(mid);  /* the product: (hi : w1 : w0), leading 1 at bit 104 or 105 */
    /* >> 42: bits 105..42 to 63..0 */
    uint64_t m = MK((HI(hi) << 22) | (LO(hi) >> 10), (LO(hi) << 22) | (w1 >> 10));
    if ((w1 & 0x3ff) | w0) m |= 1;
    return pack_d(s, ea + eb - 1023, m);
}

HOT f64 SF(muldf3)(f64 a, f64 b)
{
    uint32_t ah = HI(a), bh = HI(b), al = LO(a), bl = LO(b);
    uint32_t ea = (ah >> 20) & 0x7ff, eb = (bh >> 20) & 0x7ff;
    uint32_t s = (ah ^ bh) & 0x80000000u;
    if (ea == 0x7ff || eb == 0x7ff) {
        if (isnan_d(a)) return (qnan_d(a) & ~D_SIGN) | MK(s, 0);
        if (isnan_d(b)) return (qnan_d(b) & ~D_SIGN) | MK(s, 0);
        if ((a << 1) == 0 || (b << 1) == 0) return D_QNAN;   /* inf * 0 */
        return MK(s | 0x7ff00000u, 0);
    }
    if ((a << 1) == 0 || (b << 1) == 0) return MK(s, 0);
    if (ea == 0 || eb == 0) return mul_gen(a, b);
    /* a power-of-two factor (scales of 1, -1, 2): the other's exponent moved, exact while the result is normal */
    if (((bh & 0xfffff) | bl) == 0 || ((ah & 0xfffff) | al) == 0) {
        int e = (int)ea + (int)eb - 1023;
        if (e > 0 && e < 0x7ff) {
            int bp = ((bh & 0xfffff) | bl) == 0;
            uint32_t mh = bp ? ah : bh, ml = bp ? al : bl;
            return MK(s | ((uint32_t)e << 20) | (mh & 0xfffff), ml);
        }
    }
    uint32_t mah = (ah & 0xfffff) | 0x100000, mbh = (bh & 0xfffff) | 0x100000;
    uint64_t p0 = (uint64_t)al * bl, p1 = (uint64_t)mah * bl, p2_ = (uint64_t)al * mbh, p3 = (uint64_t)mah * mbh;
    uint64_t c1 = (uint64_t)HI(p0) + LO(p1) + LO(p2_);
    uint64_t c2 = (uint64_t)HI(c1) + HI(p1) + HI(p2_) + LO(p3);
    uint32_t w0 = LO(p0), w1 = LO(c1), w2 = LO(c2), w3 = HI(c2) + HI(p3);
    int e = (int)ea + (int)eb - 1023;
    uint32_t rh, rl, round, sticky;
    if (w3 & 0x200u) {                              /* leading bit 105 */
        e++;
        rh = (w3 << 11) | (w2 >> 21);
        rl = (w2 << 11) | (w1 >> 21);
        round = (w1 >> 20) & 1;
        sticky = (w1 & 0xfffff) | w0;
    } else {                                        /* leading bit 104 */
        rh = (w3 << 12) | (w2 >> 20);
        rl = (w2 << 12) | (w1 >> 20);
        round = (w1 >> 19) & 1;
        sticky = (w1 & 0x7ffff) | w0;
    }
    if (round && (sticky || (rl & 1))) {
        rl++;
        if (rl == 0) {
            rh++;
            if (rh == 0x200000u) {
                rh = 0x100000u;
                e++;
            }
        }
    }
    if (e <= 0 || e >= 0x7ff) return mul_gen(a, b);
    return MK(s | ((uint32_t)e << 20) | (rh & 0xfffff), rl);
}

/* one quotient step: R < B (B's leading 1 at bit 61); q = floor(R * 2^29 / B), R = the remainder */
static inline uint32_t div_step(uint64_t *R, uint64_t B, uint32_t bt)
{
    uint64_t n = *R >> 2;
    uint32_t q;
#ifdef SOFTFP_DIVU
    volatile uint32_t *dv = (volatile uint32_t *)0xffffff00u;   /* SH7604 DIVU: DVSR, DVDNTH, DVDNTL (starts) */
    dv[0] = bt;
    dv[4] = HI(n);
    dv[5] = LO(n);
    q = dv[5];
#else
    q = (uint32_t)(n / bt);
#endif
    /* the estimate is q - 1, q or q + 1: the remainder tells (mod 2^64; its true value is in (-B, 2B)) */
    uint64_t r = (*R << 29) - ((uint64_t)q * LO(B) + MK(q * HI(B), 0));
    if ((int64_t)r < 0) { q--; r += B; }
    else if (r >= B) { q++; r -= B; }
    *R = r;
    return q;
}

HOT f64 SF(divdf3)(f64 a, f64 b)
{
    uint32_t ah = HI(a), bh = HI(b);
    uint32_t s = (ah ^ bh) >> 31;
    int ea = (ah >> 20) & 0x7ff, eb = (bh >> 20) & 0x7ff;
    if (ea == 0x7ff || eb == 0x7ff || (a << 1) == 0 || (b << 1) == 0) {
        if (isnan_d(a)) return qnan_d(a);
        if (isnan_d(b)) return qnan_d(b);
        uint64_t sign = (uint64_t)s << 63;
        int ia = ea == 0x7ff, za = (a << 1) == 0, ib = eb == 0x7ff, zb = (b << 1) == 0;
        if (ia || za) {
            if ((ia && ib) || (za && zb)) return D_QNAN;
            return sign | (ia ? D_INF : 0);
        }
        if (ib) return sign;
        return sign | D_INF;                        /* x / 0 */
    }
    uint64_t ma, mb;
    if (ea == 0 || eb == 0) {
        ma = unpk_d(a, &ea);
        mb = unpk_d(b, &eb);
    } else {
        ma = (a & D_FRAC) | D_IMPL;
        mb = (b & D_FRAC) | D_IMPL;
    }
    uint64_t B = shl64(mb, 9), R;                   /* B: leading 1 at bit 61; R = A in [B / 2, B) */
    int ge = ma >= mb;
    R = ge ? shl64(ma, 8) : shl64(ma, 9);
    uint32_t bt = (uint32_t)(B >> 31);              /* B's top 31 bits (bit 30 set) */
    uint32_t q1 = div_step(&R, B, bt), q0 = div_step(&R, B, bt);
    uint64_t Q = MK(q1 >> 3, (q1 << 29) | q0);      /* 58 bits, the leading 1 at bit 57 */
    int e = ea - eb + 1022 + ge;
    if (e > 0 && e < 0x7ff) {
        uint32_t r = LO(Q) & 31;
        uint64_t m = MK(HI(Q) >> 5, (LO(Q) >> 5) | (HI(Q) << 27));
        if (r > 16 || (r == 16 && (R != 0 || (LO(m) & 1)))) {
            m++;
            if (m >> 53) { m >>= 1; e++; }
        }
        if (e < 0x7ff) return ((uint64_t)s << 63) | (MK((uint32_t)(e - 1) << 20, 0) + m);
        return ((uint64_t)s << 63) | D_INF;
    }
    return pack_d(s, e, shl64(Q, 5) | (R != 0));
}

/* fp-bit's __fpcmp_parts order: -1, 0, 1; nanres when a NaN is involved */
static inline int cmp_d(f64 a, f64 b, int nanres)
{
    uint32_t ah = HI(a), bh = HI(b);
    if (((ah & 0x7ff00000u) == 0x7ff00000u && isnan_d(a)) || ((bh & 0x7ff00000u) == 0x7ff00000u && isnan_d(b)))
        return nanres;
    if ((((ah | bh) << 1) | LO(a) | LO(b)) == 0) return 0;      /* +0 == -0 */
    uint32_t sa = ah >> 31;
    if (sa != (bh >> 31)) return sa ? -1 : 1;
    if (a == b) return 0;
    return ((a > b) ^ sa) ? 1 : -1;
}

HOT int SF(eqdf2)(f64 a, f64 b) { return cmp_d(a, b, 1); }
HOT int SF(nedf2)(f64 a, f64 b) { return cmp_d(a, b, 1); }
HOT int SF(ltdf2)(f64 a, f64 b) { return cmp_d(a, b, 1); }
HOT int SF(ledf2)(f64 a, f64 b) { return cmp_d(a, b, 1); }
HOT int SF(gtdf2)(f64 a, f64 b) { return cmp_d(a, b, -1); }
HOT int SF(gedf2)(f64 a, f64 b) { return cmp_d(a, b, -1); }
HOT int SF(unorddf2)(f64 a, f64 b) { return isnan_d(a) || isnan_d(b); }

HOT f64 SF(floatsidf)(int32_t v)                   /* exact */
{
    if (v == 0) return 0;
    uint32_t s = v < 0 ? 0x80000000u : 0, m = v < 0 ? -(uint32_t)v : (uint32_t)v;
    int z = clz32(m);
    m *= p2[z];                                     /* the leading 1 at bit 31 */
    return MK(s | ((uint32_t)(1023 + 31 - z) << 20) | ((m >> 11) & 0xfffff), m << 21);
}

HOT f64 SF(floatunsidf)(uint32_t m)
{
    if (m == 0) return 0;
    int z = clz32(m);
    m *= p2[z];
    return MK(((uint32_t)(1023 + 31 - z) << 20) | ((m >> 11) & 0xfffff), m << 21);
}

HOT int32_t SF(fixdfsi)(f64 a)                     /* toward zero; fp-bit's NaN / range answers */
{
    uint32_t h = HI(a);
    int ex = (h >> 20) & 0x7ff;
    if (ex == 0x7ff && isnan_d(a)) return 0;
    if (ex < 1023) return 0;                        /* |a| < 1, zeros, subnormals */
    int e = ex - 1023;
    if (e > 30) return (h >> 31) ? (int32_t)0x80000000u : 0x7fffffff;
    uint32_t mh = (h & 0xfffff) | 0x100000, v;      /* the integer part: (mh:lo) >> (52 - e) */
    if (e == 20) v = mh;
    else if (e < 20) v = HI((uint64_t)mh * p2[12 + e]);   /* mh >> (20 - e) */
    else {
        uint64_t q = (uint64_t)LO(a) * p2[e - 20];  /* lo >> (52 - e) = high word of lo * 2^(e - 20) */
        v = mh * p2[e - 20] | HI(q);
    }
    return (h >> 31) ? -(int32_t)v : (int32_t)v;
}

/* ---- binary32 ---------------------------------------------------------------------------------------------- */

/* sign s, exponent e (biased, of bit 62 of m), significand m (nonzero), rounded to binary32 */
static f32 pack_f(uint32_t s, int e, uint64_t m)
{
    uint32_t sign = s << 31;
    if (m == 0) return sign;
    int z = clz64(m) - 1;
    if (z > 0) { m = shl64(m, z); e -= z; }
    else if (z < 0) { m = shr64s(m, 1); e += 1; }
    if (e <= 0) {
        m = shr64s(m, 1 - e);
        e = 0;
    }
    /* 24 bits from bit 62: q = m >> 39, the rest round and sticky */
    uint32_t q = HI(m) >> 7, rh = HI(m) & 0x7f, rl = LO(m);
    if (rh > 0x40 || (rh == 0x40 && (rl || (q & 1)))) q++;
    if (e == 0) return sign | q;
    if (q >> 24) { q >>= 1; e++; }
    if (e >= 0xff) return sign | F_INF;
    return sign | (((uint32_t)(e - 1) << 23) + q);
}

HOT f64 SF(extendsfdf2)(f32 a)                     /* exact */
{
    uint32_t e8 = (a >> 23) & 0xff, s = a & 0x80000000u, m = a & 0x7fffff;
    if (e8 != 0 && e8 != 0xff)
        return MK(s | ((e8 + 1023 - 127) << 20) | (m >> 3), m << 29);
    if (e8 == 0xff) {
        if (m) m |= 0x400000;                       /* NaN: quieted, payload kept */
        return MK(s | 0x7ff00000u | (m >> 3), m << 29);
    }
    if (m == 0) return MK(s, 0);
    int z = clz32(m) - 8;                           /* subnormal float: normal double */
    m *= p2[z];                                     /* leading 1 at bit 23 */
    return MK(s | ((uint32_t)(1023 - 126 - z) << 20) | ((m & 0x7fffff) >> 3), m << 29);
}

HOT f32 SF(truncdfsf2)(f64 a)
{
    uint32_t h = HI(a), l = LO(a), s = h & 0x80000000u;
    int ex = (h >> 20) & 0x7ff;
    if (ex == 0x7ff) {
        if ((h & 0xfffff) | l) return s | F_QNAN | ((h & 0xfffff) << 3) | (l >> 29);
        return s | F_INF;
    }
    int e = ex - 1023 + 127;
    if (e > 0 && e < 0xff) {                        /* a normal float: 24 bits from 53 */
        uint32_t m = (((h & 0xfffff) | 0x100000) << 3) | (l >> 29), lost = l & 0x1fffffff;
        if (lost > 0x10000000 || (lost == 0x10000000 && (m & 1))) {
            m++;
            if (m == 0x1000000) { m >>= 1; e++; }
        }
        if (e < 0xff) return s | ((uint32_t)e << 23) | (m & 0x7fffff);
        return s | F_INF;
    }
    if (ex == 0) return s;                          /* zero, or a double subnormal: far below half the least float */
    if (e >= 0xff) return s | F_INF;
    return pack_f(s >> 31, e, shl64((a & D_FRAC) | D_IMPL, 10));
}

static f32 qnan_f(f32 a) { return a | 0x400000u; }

/* binary32 + - * /: normal operands with a normal result directly (one rounding of the exact result; Maldita's
   gmlibm.c f_add_c / f_mul_c / f_div_c); everything else through binary64: the binary64 result of two floats
   rounded once more to binary32 is the correctly rounded binary32 result (53 >= 2 * 24 + 2), infinities and
   subnormals included */
static f32 add_f_wide(f32 a, f32 b) { return SF(truncdfsf2)(SF(adddf3)(SF(extendsfdf2)(a), SF(extendsfdf2)(b))); }

HOT f32 SF(addsf3)(f32 a, f32 b)
{
    uint32_t ua = a & 0x7fffffffu, ub = b & 0x7fffffffu;
    if (ua > F_INF) return qnan_f(a);
    if (ub > F_INF) return qnan_f(b);
    if (ua < ub) {                                  /* |a| >= |b| */
        f32 t = a; a = b; b = t;
        t = ua; ua = ub; ub = t;
    }
    if (ub == 0) return ua == 0 ? (a & b) : a;     /* a + (+-0); two zeros: -0 only if both are */
    int ea = (int)(ua >> 23), eb = (int)(ub >> 23);
    if (eb == 0 || ea == 0xff) return add_f_wide(a, b);
    /* significands with 6 guard bits (the leading 1 at bit 29); b aligned with a sticky bit */
    uint32_t ma = ((ua & 0x7fffffu) | 0x800000u) << 6, mb = ((ub & 0x7fffffu) | 0x800000u) << 6;
    int d = ea - eb;
    if (d > 31) mb = 1;
    else if (d > 0) {
        uint64_t p = (uint64_t)mb * p2[32 - d];
        mb = HI(p) | (LO(p) != 0);
    }
    uint32_t m;
    int e = ea;
    if (!((a ^ b) >> 31)) {
        m = ma + mb;
        if (m & 0x40000000u) { m = (m >> 1) | (m & 1); e++; }
    } else {
        m = ma - mb;
        if (m == 0) return 0;                       /* x - x = +0 */
        int z = clz32(m) - 2;
        m *= p2[z];
        e -= z;
        if (e <= 0) return add_f_wide(a, b);
    }
    uint32_t r = m & 0x3f;
    m >>= 6;
    if (r > 0x20 || (r == 0x20 && (m & 1))) {
        m++;
        if (m == 0x1000000u) { m >>= 1; e++; }
    }
    if (e >= 0xff) return (a & 0x80000000u) | F_INF;
    return (a & 0x80000000u) | ((uint32_t)e << 23) | (m & 0x7fffffu);
}
HOT f32 SF(subsf3)(f32 a, f32 b) { return SF(addsf3)(a, b ^ 0x80000000u); }

HOT f32 SF(mulsf3)(f32 a, f32 b)
{
    uint32_t s = (a ^ b) & 0x80000000u;
    if (isnan_f(a)) return (qnan_f(a) & 0x7fffffffu) | s;
    if (isnan_f(b)) return (qnan_f(b) & 0x7fffffffu) | s;
    int ea = (int)((a >> 23) & 0xff), eb = (int)((b >> 23) & 0xff);
    if (ea == 0 || eb == 0 || ea == 0xff || eb == 0xff)
        return SF(truncdfsf2)(SF(muldf3)(SF(extendsfdf2)(a), SF(extendsfdf2)(b)));
    uint64_t p = (uint64_t)((a & 0x7fffffu) | 0x800000u) * ((b & 0x7fffffu) | 0x800000u);   /* 2^46 .. 2^48 */
    uint32_t hi = HI(p), lo = LO(p), q, rest, half;
    int e;
    if (hi & 0x8000u) {                             /* leading bit 47 */
        q = (hi << 8) | (lo >> 24); rest = lo & 0xffffffu; half = 0x800000u; e = ea + eb - 126;
    } else {                                        /* leading bit 46 */
        q = (hi << 9) | (lo >> 23); rest = lo & 0x7fffffu; half = 0x400000u; e = ea + eb - 127;
    }
    if (rest > half || (rest == half && (q & 1))) {
        q++;
        if (q == 0x1000000u) { q >>= 1; e++; }
    }
    if (e <= 0 || e >= 0xff) return SF(truncdfsf2)(SF(muldf3)(SF(extendsfdf2)(a), SF(extendsfdf2)(b)));
    return s | ((uint32_t)e << 23) | (q & 0x7fffffu);
}

HOT f32 SF(divsf3)(f32 a, f32 b)
{
    if (isnan_f(a)) return qnan_f(a);
    if (isnan_f(b)) return qnan_f(b);
    int ea = (int)((a >> 23) & 0xff), eb = (int)((b >> 23) & 0xff);
    if (ea == 0 || eb == 0 || ea == 0xff || eb == 0xff)
        return SF(truncdfsf2)(SF(divdf3)(SF(extendsfdf2)(a), SF(extendsfdf2)(b)));
    uint32_t ma = (a & 0x7fffffu) | 0x800000u, mb = (b & 0x7fffffu) | 0x800000u;
    int e = ea - eb + 127;
    if (ma < mb) { ma <<= 1; e--; }
    /* q = (ma << 25) / mb: 26 bits (24, the round bit, and with the remainder the sticky bit) */
    uint64_t n = (uint64_t)ma << 25;
    uint32_t q, r;
#ifdef SOFTFP_DIVU
    volatile uint32_t *dv = (volatile uint32_t *)0xffffff00u;
    dv[0] = mb;
    dv[4] = HI(n);
    dv[5] = LO(n);
    q = dv[5];
    r = dv[4];
#else
    q = (uint32_t)(n / mb);
    r = (uint32_t)(n % mb);
#endif
    uint32_t round = (q >> 1) & 1, sticky = (q & 1) | (r != 0);
    q >>= 2;
    if (round && (sticky || (q & 1))) {
        q++;
        if (q == 0x1000000u) { q >>= 1; e++; }
    }
    if (e <= 0 || e >= 0xff) return SF(truncdfsf2)(SF(divdf3)(SF(extendsfdf2)(a), SF(extendsfdf2)(b)));
    return ((a ^ b) & 0x80000000u) | ((uint32_t)e << 23) | (q & 0x7fffffu);
}

static inline int cmp_f(f32 a, f32 b, int nanres)
{
    if (isnan_f(a) || isnan_f(b)) return nanres;
    if (((a | b) << 1) == 0) return 0;
    uint32_t sa = a >> 31;
    if (sa != (b >> 31)) return sa ? -1 : 1;
    if (a == b) return 0;
    return ((a > b) ^ sa) ? 1 : -1;
}

HOT int SF(eqsf2)(f32 a, f32 b) { return cmp_f(a, b, 1); }
HOT int SF(nesf2)(f32 a, f32 b) { return cmp_f(a, b, 1); }
HOT int SF(ltsf2)(f32 a, f32 b) { return cmp_f(a, b, 1); }
HOT int SF(lesf2)(f32 a, f32 b) { return cmp_f(a, b, 1); }
HOT int SF(gtsf2)(f32 a, f32 b) { return cmp_f(a, b, -1); }
HOT int SF(gesf2)(f32 a, f32 b) { return cmp_f(a, b, -1); }
HOT int SF(unordsf2)(f32 a, f32 b) { return isnan_f(a) || isnan_f(b); }

HOT f32 SF(floatsisf)(int32_t v)
{
    if (v == 0) return 0;
    uint32_t s = v < 0 ? 0x80000000u : 0, m = v < 0 ? -(uint32_t)v : (uint32_t)v;
    int z = clz32(m);
    m *= p2[z];                                     /* the leading 1 at bit 31: 24 bits, 8 to round */
    uint32_t q = m >> 8, r = m & 0xff;
    int e = 127 + 31 - z;
    if (r > 0x80 || (r == 0x80 && (q & 1))) {
        q++;
        if (q == 0x1000000) { q >>= 1; e++; }
    }
    return s | ((uint32_t)e << 23) | (q & 0x7fffff);
}

HOT int32_t SF(fixsfsi)(f32 a)
{
    int ex = (a >> 23) & 0xff;
    if (isnan_f(a)) return 0;
    if (ex < 127) return 0;
    int e = ex - 127;
    if (e > 30) return (a >> 31) ? (int32_t)0x80000000u : 0x7fffffff;
    uint32_t m = (a & 0x7fffff) | 0x800000, v;
    if (e >= 23) v = m * p2[e - 23];
    else v = (uint32_t)(((uint64_t)m * p2[32 - 23 + e]) >> 32);   /* m >> (23 - e) */
    return (a >> 31) ? -(int32_t)v : (int32_t)v;
}
