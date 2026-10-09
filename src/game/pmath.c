/* Real functions the enemies use (no libm on the SH-2): sqrt, sin, cos, atan2f, point_distance, point_direction.
 * sqrt and sin / cos are correctly rounded (the runner's glibc sin / cos are); atan2f is glibc's (fdlibm float). The
 * runner calls its C library's, so a last-bit difference shows as an xVel / yVel difference in tools/playcmp.py. */
#include "pmath.h"
#ifdef PLAY_STATS
#include <stdio.h>
#include <stdlib.h>
#endif
#ifdef __clang__
#pragma STDC FP_CONTRACT OFF   /* Dekker's products below need separate multiply and add (GCC -std=c99: off) */
#endif

static int32_t hiw(double x) { union { double d; uint64_t u; } v; v.d = x; return (int32_t)(v.u >> 32); }
static uint32_t low(double x) { union { double d; uint64_t u; } v; v.d = x; return (uint32_t)v.u; }
static double mkd(int32_t hi, uint32_t lo)
{
    union { double d; uint64_t u; } v;
    v.u = ((uint64_t)(uint32_t)hi << 32) | lo;
    return v.d;
}

/* sqrt correctly rounded: Newton, then the neighbour whose square is nearest (Dekker's exact product) */
static void two_prod(double a, double b, double *p, double *e)
{
    const double split = 134217729.0;                  /* 2^27 + 1 */
    double ca = split * a, ah = ca - (ca - a), al = a - ah;
    double cb = split * b, bh = cb - (cb - b), bl = b - bh;
    *p = a * b;
    *e = ((ah * bh - *p) + ah * bl + al * bh) + al * bl;
}

static double sq_err(double s, double d)               /* s * s - d, nearly exact */
{
    double p, e;
    two_prod(s, s, &p, &e);
    return (p - d) + e;
}

/* sqrt of a positive normal x, correctly rounded (round to nearest even): fdlibm e_sqrt.c's bit-by-bit square root
   in 32-bit integers, its rounding step in round-to-nearest. Exponent field 1..2046 only (no zero, subnormal,
   Inf or NaN) */
static double isqrt_d(double x)
{
    const uint32_t sign = 0x80000000u;
    uint32_t r, t1, s1, ix1, q1, ix0, s0, q, t;
    int32_t m;
    ix0 = (uint32_t)hiw(x);
    ix1 = low(x);
    m = (int32_t)(ix0 >> 20) - 1023;
    ix0 = (ix0 & 0x000fffffu) | 0x00100000u;
    if (m & 1) {                                      /* odd m: double x to make it even */
        ix0 += ix0 + ((ix1 & sign) >> 31);
        ix1 += ix1;
    }
    m >>= 1;
    ix0 += ix0 + ((ix1 & sign) >> 31);
    ix1 += ix1;
    q = q1 = s0 = s1 = 0;
    r = 0x00200000u;
    while (r != 0) {
        t = s0 + r;
        if (t <= ix0) {
            s0 = t + r;
            ix0 -= t;
            q += r;
        }
        ix0 += ix0 + ((ix1 & sign) >> 31);
        ix1 += ix1;
        r >>= 1;
    }
    r = sign;
    while (r != 0) {
        t1 = s1 + r;
        t = s0;
        if (t < ix0 || (t == ix0 && t1 <= ix1)) {
            s1 = t1 + r;
            if ((t1 & sign) == sign && (s1 & sign) == 0) s0 += 1;
            ix0 -= t;
            if (ix1 < t1) ix0 -= 1;
            ix1 -= t1;
            q1 += r;
        }
        ix0 += ix0 + ((ix1 & sign) >> 31);
        ix1 += ix1;
        r >>= 1;
    }
    if ((ix0 | ix1) != 0) {                           /* inexact: round to nearest */
        if (q1 == 0xffffffffu) { q1 = 0; q += 1; }
        else q1 += (q1 & 1);
    }
    ix0 = (q >> 1) + 0x3fe00000u;
    ix1 = q1 >> 1;
    if (q & 1) ix1 |= sign;
    ix0 += (uint32_t)m << 20;
    return mkd((int32_t)ix0, ix1);
}

/* d in [2^-60, 2^101): isqrt_d (docs/PERF2.md F; equal to the C library's sqrt and to the Newton path below on the
   game's distances and 20 M random inputs of that range). Outside it, Newton then the nearest neighbour */
double psqrt(double d)
{
    double s, prev = 0;
    int it;
    if (((uint32_t)hiw(d) >> 20) - (1023u - 60u) <= 160u) return isqrt_d(d);
    if (d <= 0) return 0;
    s = d > 1 ? d * 0.5 : 1;
    for (it = 0; it < 200 && s != prev; it++) { prev = s; s = 0.5 * (s + d / s); }
    for (it = 0; it < 4; it++) {
        union { double d; uint64_t u; } up, dn;
        double e = sq_err(s, d), eu, ed;
        up.d = s; up.u++;
        dn.d = s; dn.u--;
        eu = sq_err(up.d, d);
        ed = sq_err(dn.d, d);
        if ((eu < 0 ? -eu : eu) < (e < 0 ? -e : e)) s = up.d;
        else if ((ed < 0 ? -ed : ed) < (e < 0 ? -e : e)) s = dn.d;
        else break;
    }
    return s;
}

/* 2 / pi (cr_trig_dd, cr_reduce) */
static const double invpio2 = 6.36619772367581382433e-01;

/* ---- sin / cos correctly rounded (the runner's glibc sin / cos are; fdlibm's and the host's miss by an ulp:
   build/trace/p5_spider_s121, the bat's velocities): reduction by a three-part pi / 2 and the Taylor series in
   double-double (about 106 bits), then rounded once ---------------------------------------------------------- */
typedef struct { double hi, lo; } ddbl;
#ifndef SC_EPS_S
#define SC_EPS_S 0x1p-58                     /* sincos_r's error bounds (relative for sin, absolute for cos) */
#define SC_EPS_C 0x1.8p-57
#endif

static ddbl two_sum(double a, double b)
{
    ddbl r;
    double s = a + b, bb = s - a;
    r.hi = s;
    r.lo = (a - (s - bb)) + (b - bb);
    return r;
}
static ddbl dd_add(ddbl a, ddbl b)
{
    ddbl s = two_sum(a.hi, b.hi), t = two_sum(a.lo, b.lo);
    s.lo += t.hi;
    s = two_sum(s.hi, s.lo);
    s.lo += t.lo;
    return two_sum(s.hi, s.lo);
}
static ddbl dd_mul(ddbl a, ddbl b)
{
    ddbl p;
    two_prod(a.hi, b.hi, &p.hi, &p.lo);
    p.lo += a.hi * b.lo + a.lo * b.hi;
    return two_sum(p.hi, p.lo);
}
static ddbl dd_div_d(ddbl a, double b)
{
    ddbl p, r;
    double q1 = a.hi / b, q2;
    two_prod(q1, b, &p.hi, &p.lo);
    q2 = ((a.hi - p.hi) - p.lo + a.lo) / b;
    r = two_sum(q1, q2);
    return r;
}

/* sin (odd = 1) or cos (odd = 0) of x (|x| <= pi / 4) in double-double */
static ddbl dd_sincos(ddbl x, int odd)
{
    ddbl x2 = dd_mul(x, x), term, sum;
    int n;
    if (odd) { term = x; n = 1; }
    else { term.hi = 1; term.lo = 0; n = 0; }
    sum = term;
    while (n < 40) {
        term = dd_mul(term, x2);
        term = dd_div_d(term, -(double)((n + 1) * (n + 2)));
        n += 2;
        sum = dd_add(sum, term);
        if ((term.hi < 0 ? -term.hi : term.hi) < 1e-36) break;
    }
    return sum;
}

/* the double-double series of the reduced argument, rounded once: the reference (tests/sincos); psin_cr and pcos_cr
   below give its bits through sincos_r, which reaches this series only when its two error tests both fail */
static ddbl cr_reduce(double a, int *q);
static __attribute__((unused)) double cr_trig_dd(double a, int want_sin)
{
    ddbl t;
    int q;
    ddbl r = cr_reduce(a, &q);
    if (want_sin) {
        switch (q) {
        case 0: t = dd_sincos(r, 1); break;
        case 1: t = dd_sincos(r, 0); break;
        case 2: t = dd_sincos(r, 1); t.hi = -t.hi; t.lo = -t.lo; break;
        default: t = dd_sincos(r, 0); t.hi = -t.hi; t.lo = -t.lo; break;
        }
    } else {
        switch (q) {
        case 0: t = dd_sincos(r, 0); break;
        case 1: t = dd_sincos(r, 1); t.hi = -t.hi; t.lo = -t.lo; break;
        case 2: t = dd_sincos(r, 0); t.hi = -t.hi; t.lo = -t.lo; break;
        default: t = dd_sincos(r, 1); break;
        }
    }
    return t.hi + t.lo;
}


/* sincos_r's second test (about 1 call in 8 of the first fails): the Taylor series of sin x = x + x^3 P(x^2) (to
   x^25) or cos x = 1 - x^2 / 2 + x^4 Q(x^2) (to x^26), the leading 6 (sin) or 5 (cos) coefficients of P / Q and
   Horner's steps on them in double-double (1 / n! as hi + lo), the smaller rest in double; r.lo as y cos x or
   -y sin x in double. Kept when hi + (lo +- e) round alike, e = 2^-86 |x| (sin) or 2^-86 (cos), else 0 (then
   dd_sincos). tests/sincos (every float direction in [+0, 360] and -0 through psincos_cr): the first test fails for
   10,740,273 sins and 46,834,802 cosines; this one keeps all of them, each equal to dd_sincos's (also with e = 2^-104),
   so bat_fly never reaches dd_sincos (about 150 K instructions a call) */
#ifndef SC2_EPS_S
#define SC2_EPS_S 0x1p-86
#define SC2_EPS_C 0x1p-86
#endif
static const double SC2_P[6][2] = {                          /* 1 / 3!, 1 / 5!, ..., 1 / 13! (signs in the steps) */
    { 0.16666666666666666, 9.25185853854297e-18 }, { 0.008333333333333333, 1.1564823173178714e-19 },
    { 0.0001984126984126984, 1.7209558293420705e-22 }, { 2.7557319223985893e-06, -1.858393274046472e-22 },
    { 2.505210838544172e-08, -1.448814070935912e-24 }, { 1.6059043836821613e-10, 1.2585294588752098e-26 } };
static const double SC2_Q[5][2] = {                          /* 1 / 4!, 1 / 6!, ..., 1 / 12! */
    { 0.041666666666666664, 2.3129646346357427e-18 }, { 0.001388888888888889, -5.300543954373577e-20 },
    { 2.48015873015873e-05, 2.1511947866775882e-23 }, { 2.755731922398589e-07, 2.3767714622250297e-23 },
    { 2.08767569878681e-09, -1.20734505911326e-25 } };

/* c + a z for double-double a, z and c = s (ch + cl), s = +-1, |a z| < |c| (the next coefficient is under 1 / 20 of
   this one and z < 0.62): one two_prod, Fast2Sum on the leading parts */
static ddbl dd_hstep(ddbl a, double zh, double zl, double s, const double *c)
{
    ddbl r;
    double p, e, ch = s * c[0], h, t;
    two_prod(a.hi, zh, &p, &e);
    e += a.hi * zl + a.lo * zh;
    h = ch + p;
    t = p - (h - ch);
    t += e + s * c[1];
    r.hi = h + t;
    r.lo = t - (r.hi - h);
    return r;
}

static int sincos_r2(ddbl r, int odd, double *out)
{
    double x = r.hi, y = r.lo, zh, zl, h, l, t, e, a, b;
    ddbl acc, u;
    int k;
    two_prod(x, x, &zh, &zl);                                  /* z = x^2 exactly */
    if (odd) {
        double x3h, x3l, cd;
        /* P's tail: -1 / 15! + z / 17! - ... + z^5 / 25! */
        acc.hi = -7.647163731819816e-13 + zh * (2.8114572543455206e-15 + zh * (-8.22063524662433e-18 +
                 zh * (1.9572941063391263e-20 + zh * (-3.868170170630684e-23 + zh * 6.446950284384474e-26))));
        acc.lo = 0;
        for (k = 5; k >= 0; k--) acc = dd_hstep(acc, zh, zl, (k & 1) ? 1.0 : -1.0, SC2_P[k]);
        two_prod(x, zh, &x3h, &x3l);                           /* x^3 */
        x3l += x * zl;
        u.hi = x3h; u.lo = x3l;
        u = dd_mul(u, acc);                                    /* x^3 P */
        cd = 1.0 - zh * (0.5 - zh * (0.041666666666666664 - zh * (0.001388888888888889 - zh * (2.48015873015873e-05 -
             zh * (2.755731922398589e-07 - zh * 2.08767569878681e-09)))));   /* cos x for y cos x */
        h = x + u.hi;
        t = u.hi - (h - x);
        l = ((t + u.lo) + y * cd);
        e = (x < 0 ? -x : x) * SC2_EPS_S;
    } else {
        double sd, z2h, z2l, wh, we;
        /* Q's tail: -1 / 14! + z / 16! - ... - z^6 / 26! */
        acc.hi = -1.1470745597729725e-11 + zh * (4.779477332387385e-14 + zh * (-1.5619206968586225e-16 +
                 zh * (4.110317623312165e-19 + zh * (-8.896791392450574e-22 + zh * (1.6117375710961184e-24 +
                 zh * -2.4795962632247976e-27)))));
        acc.lo = 0;
        for (k = 4; k >= 0; k--) acc = dd_hstep(acc, zh, zl, (k & 1) ? -1.0 : 1.0, SC2_Q[k]);
        two_prod(zh, zh, &z2h, &z2l);                          /* z^2 */
        z2l += 2.0 * zh * zl;
        u.hi = z2h; u.lo = z2l;
        u = dd_mul(u, acc);                                    /* z^2 Q */
        sd = x * (1.0 - zh * (0.16666666666666666 - zh * (0.008333333333333333 - zh * (0.0001984126984126984 -
             zh * (2.7557319223985893e-06 - zh * 2.505210838544172e-08)))));  /* sin x for -y sin x */
        wh = 1.0 - 0.5 * zh;                                   /* exact Fast2Sum: 0.5 z <= 0.31 */
        we = (1.0 - wh) - 0.5 * zh;
        h = wh + u.hi;
        t = u.hi - (h - wh);
        l = (((t + we) - 0.5 * zl) + u.lo) - y * sd;
        e = SC2_EPS_C;
    }
    a = h + (l + e);
    b = h + (l - e);
    if (a != b) return 0;
    *out = h + l;
    return 1;
}

/* sin (odd 1) or cos (odd 0) of the reduced r, as cr_trig_dd rounds it: the Taylor series with x^2, x^3 and the x^3 / 6
   term in double-double (two_prod) and the rest (to x^21 / x^22) in double, as hi + lo with an error of about 2^-60;
   kept when hi + (lo + e) and hi + (lo - e) round alike (Ziv's test), else the double-double series, cr_trig_dd's own
   result. Checked equal to psin_cr / pcos_cr for every float argument in [0, 360] degrees (bat_fly's directions) */
static double sincos_r(ddbl r, int odd)
{
    static const double I6H = 0.16666666666666666, I6L = 9.25185853854297e-18,
        s5 = 0.008333333333333333, s7 = -0.0001984126984126984, s9 = 2.7557319223985893e-06,
        s11 = -2.505210838544172e-08, s13 = 1.6059043836821613e-10, s15 = -7.647163731819816e-13,
        s17 = 2.8114572543455206e-15, s19 = -8.22063524662433e-18, s21 = 1.9572941063391263e-20,
        c4 = 0.041666666666666664, c6 = -0.001388888888888889, c8 = 2.48015873015873e-05,
        c10 = -2.755731922398589e-07, c12 = 2.08767569878681e-09, c14 = -1.1470745597729725e-11,
        c16 = 4.779477332387385e-14, c18 = -1.5619206968586225e-16, c20 = 4.110317623312165e-19,
        c22 = -8.896791392450574e-22;
    double x = r.hi, y = r.lo, zh, zl, hi, lo, e, a, b;
    ddbl t;
    two_prod(x, x, &zh, &zl);                                  /* x^2 */
    if (odd) {
        double vh, vl, th, tl, rest;
        ddbl u;
        two_prod(x, zh, &vh, &vl);                             /* x^3 */
        vl += x * zl;
        two_prod(vh, I6H, &th, &tl);                           /* x^3 / 6 */
        tl += vh * I6L + vl * I6H;
        rest = vh * zh * (s5 + zh * (s7 + zh * (s9 + zh * (s11 + zh * (s13 + zh * (s15 + zh * (s17 +
               zh * (s19 + zh * s21)))))))) + (vl * zh + vh * zl) * s5;     /* x^5 (s5 + ...) */
        u = two_sum(x, -th);
        hi = u.hi;
        lo = u.lo + (((rest - tl) + y * (1.0 - zh * (0.5 - zh * c4))));      /* y cos x */
        e = (x < 0 ? -x : x) * SC_EPS_S;
    } else {
        double hz = 0.5 * zh, w = 1.0 - hz, rest;
        rest = zh * (zh * (c4 + zh * (c6 + zh * (c8 + zh * (c10 + zh * (c12 + zh * (c14 + zh * (c16 + zh * (c18 +
               zh * (c20 + zh * c22))))))))) + 2.0 * zl * c4);     /* z^2 (c4 + ...), z = zh + zl */
        hi = w;
        lo = ((((1.0 - w) - hz) - 0.5 * zl) + rest) - x * y * (1.0 - zh * I6H);   /* - y sin x */
        e = SC_EPS_C;
    }
    a = hi + (lo + e);
    b = hi + (lo - e);
    if (a == b) return hi + lo;
    if (sincos_r2(r, odd, &a)) return a;
    t = dd_sincos(r, odd);
    return t.hi + t.lo;
}

/* cr_trig_dd's reduction: r = a - k pi / 2 in double-double, quadrant k & 3 */
static ddbl cr_reduce(double a, int *q)
{
    static const double P1 = 1.57079632673412561417e+00, P2 = 6.07710050630396597660e-11,
                        P3 = 2.02226624871116645580e-21, P4 = 8.47842766036889956997e-32;
    double k = (double)(int32_t)(a * invpio2 + (a < 0 ? -0.5 : 0.5));
    ddbl r, t;
    r = two_sum(a, -k * P1);                                   /* k * P1 exact (P1: 33 bits, |k| small) */
    r = dd_add(r, two_sum(-k * P2, 0));                        /* k * P2 exact */
    two_prod(-k, P3, &t.hi, &t.lo);
    r = dd_add(r, t);
    t.hi = -k * P4;
    t.lo = 0;
    r = dd_add(r, t);
    *q = ((int)k) & 3;
    return r;
}

/* ---- the integer fast path of psin_cr / pcos_cr / psincos_cr (no soft-float): for 2^-6 <= |a| < 8,
   r = |a| - k pi / 2 in 128-bit fixed point (|a| 2^124 is exact; pi / 2 2^124 rounded: |error| <= 2.5 2^-124 for
   k <= 5), kept when 2^-30 <= |r| <= 0.8, normalized to 64 bits (N, relative error < 2^-62.9); z = r^2 and the series
   sin r = r S(z), cos r = C(z) by Horner in unsigned Q63 (every partial sum is positive: z < 0.64 and each term is
   under a sixth (sin) or a half (cos) of the one before; the terms dropped are below 2^-72). Error bounds in Q63
   units: z <= 3.3 (N's truncation, the two floors); S(z) <= (0.5 + 1 + 3.3 / 6) / (1 - 0.64) < 5.7 (coefficient
   rounding, the floor of each step, z's error times the next sum <= 1 / 6); C(z) <= (0.5 + 1 + 3.3 / 2) / 0.36 < 8.8.
   So sin |r| = N S 2^.. within a relative 7 2^-63 + one unit of the product's floor (err: Y 2^-59 + 3 units), cos r
   within 12 units. Ziv's test: the result is kept when both ends of [Y - err, Y + err] round (to nearest even) to the
   same double, which is then the correctly rounded value: the one sincos_r's tests and its double-double series
   give (tests/sincos fast). Otherwise the caller takes the old path */
static const uint64_t SC_P124H = 0x1921fb54442d1846ull, SC_P124L = 0x9898cc51701b839aull;   /* pi / 2 2^124 */
static const uint64_t SC_I64 = 0xa2f9836e4e44152aull;                                    /* 2 / pi 2^64 */
static const uint64_t SC_S[10] = {                                                       /* 2^63 / (2i + 1)! */
    0x8000000000000000ull, 0x1555555555555555ull, 0x0111111111111111ull, 0x0006806806806807ull,
    0x0000171de3a556c7ull, 0x00000035cc8acfebull, 0x000000005849184full, 0x00000000006b9fd0ull,
    0x000000000000654bull, 0x000000000000004cull };
static const uint64_t SC_C[11] = {                                                       /* 2^63 / (2i)! */
    0x8000000000000000ull, 0x4000000000000000ull, 0x0555555555555555ull, 0x002d82d82d82d82eull,
    0x0000d00d00d00d01ull, 0x0000024fc9f6ef14ull, 0x000000047bb63bfeull, 0x00000000064e5d2aull,
    0x000000000006b9fdull, 0x00000000000005a1ull, 0x0000000000000004ull };

static void sc_mul(uint64_t a, uint64_t b, uint64_t *hi, uint64_t *lo)     /* a b = hi 2^64 + lo (32-bit products) */
{
    uint64_t a0 = (uint32_t)a, a1 = a >> 32, b0 = (uint32_t)b, b1 = b >> 32;
    uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    uint64_t mid = (p00 >> 32) + (uint32_t)p01 + (uint32_t)p10;
    *lo = (mid << 32) | (uint32_t)p00;
    *hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}
static uint64_t sc_mulq63(uint64_t a, uint64_t b)                          /* floor(a b / 2^63) */
{
    uint64_t h, l;
    sc_mul(a, b, &h, &l);
    return h << 1 | l >> 63;
}
static int sc_clz(uint64_t v)                                              /* v != 0 */
{
    int n = 0;
    if (!(v >> 32)) { n += 32; v <<= 32; }
    if (!(v >> 48)) { n += 16; v <<= 16; }
    if (!(v >> 56)) { n += 8; v <<= 8; }
    if (!(v >> 60)) { n += 4; v <<= 4; }
    if (!(v >> 62)) { n += 2; v <<= 2; }
    if (!(v >> 63)) n += 1;
    return n;
}
static uint64_t sc_round(uint64_t y, int e)                 /* y 2^e, y >= 2^63, to nearest even: a normal double's bits */
{
    uint64_t m = y >> 11, rem = y & 0x7ff;
    if (rem > 0x400 || (rem == 0x400 && (m & 1))) m++;
    e += 11;
    if (m >> 53) { m >>= 1; e++; }
    return ((uint64_t)(e + 52 + 1023) << 52) | (m & 0xfffffffffffffull);
}
static int sc_ziv(uint64_t y, uint64_t err, int e, uint64_t *out)          /* y 2^e within err 2^e; y >= 2^61 */
{
    int s = sc_clz(y);
    uint64_t a, b;
    y <<= s;
    err <<= s;
    e -= s;
    if (y - err < 0x8000000000000000ull || y + err < y) return 0;             /* (another binade: not tried) */
    a = sc_round(y - err, e);
    b = sc_round(y + err, e);
    if (a != b) return 0;
    *out = a;
    return 1;
}

/* need: 1 sin, 2 cos, 3 both. 1 when every needed value passed (then *s / *c hold them), else 0 */
static int sc_fast(double a, int need, double *s, double *c)
{
    union { double d; uint64_t u; } v;
    uint64_t m, ah, h, l, kph, kpl, rh, rl, n, z, t, sb = 0, cb = 0;
    int e, k, sh, rneg, i;
    v.d = a;
    e = (int)((v.u >> 52) & 0x7ff);
    if (e < 1023 - 6 || e > 1023 + 2) return 0;                               /* 2^-6 <= |a| < 8 */
    m = (v.u & 0xfffffffffffffull) | (1ull << 52);
    ah = m << (e - 1015);                                                     /* |a| 2^60 < 2^63, exact */
    sc_mul(ah, SC_I64, &h, &l);
    k = (int)((h + (1ull << 59)) >> 60);                                      /* round(|a| 2 / pi), 0 .. 5 */
    sc_mul(SC_P124L, (uint64_t)k, &h, &kpl);
    kph = SC_P124H * (uint64_t)k + h;
    rl = 0 - kpl;                                                             /* |a| 2^124 - k pi / 2 2^124 */
    rh = ah - kph - (kpl != 0);
    rneg = (int)(rh >> 63);
    if (rneg) { rl = ~rl + 1; rh = ~rh + (rl == 0); }
    if (rh < (1ull << 30)) return 0;                                          /* |r| < 2^-30 */
    sh = sc_clz(rh);                                                          /* |r| = n 2^(-60 - sh) */
    if (sh < 4) return 0;
    n = (rh << sh) | (rl >> (64 - sh));
    if (sh == 4 && n > 0xccccccccccccccccull) return 0;                       /* |r| > 0.8 */
    sc_mul(n, n, &h, &l);
    z = h >> (2 * sh - 7);                                                    /* r^2 2^63 */
    if (need & ((k & 1) ? 2 : 1)) {                                           /* sin r */
        t = SC_S[9];
        for (i = 8; i >= 0; i--) t = SC_S[i] - sc_mulq63(z, t);
        sc_mul(n, t, &h, &l);                                                 /* |sin r| = h 2^(-59 - sh) */
        if (!sc_ziv(h, (h >> 59) + 3, -59 - sh, &sb)) return 0;
        if (rneg) sb |= 0x8000000000000000ull;
    }
    if (need & ((k & 1) ? 1 : 2)) {                                           /* cos r */
        t = SC_C[10];
        for (i = 9; i >= 0; i--) t = SC_C[i] - sc_mulq63(z, t);
        if (!sc_ziv(t, 12, -63, &cb)) return 0;
    }
    /* psincos_cr's quadrants (q = k & 3), then sin(-x) = -sin x */
    switch (k & 3) {
    case 0: v.u = sb; *s = v.d; v.u = cb; *c = v.d; break;
    case 1: v.u = cb; *s = v.d; v.u = sb ^ 0x8000000000000000ull; *c = v.d; break;
    case 2: v.u = sb ^ 0x8000000000000000ull; *s = v.d; v.u = cb ^ 0x8000000000000000ull; *c = v.d; break;
    default: v.u = cb ^ 0x8000000000000000ull; *s = v.d; v.u = sb; *c = v.d; break;
    }
    if (a < 0) *s = -*s;
    return 1;
}

/* ---- sc_fast's second stage, where its Ziv test fails (about 1 call in 50 of the piranhas': each sent the call to
   sincos_r and its soft-float series, about 180 K jtcps3 clocks): the same reduction and series in 128-bit fixed
   point (32-bit limbs, least significant first; the products by dmulu, no variable shift).
   r = |a| - k pi / 2 as R = |a| 2^188 - k P (P = pi / 2 2^188 rounded: |error| <= 2.5 2^-188 for k <= 5); kept when
   2^-30 <= |r| <= 0.8 (R's limb 5 at most 0x0ccccccc). In units u = 2^-127 (Q127): rq = R >> 61 (r 2^127, truncated:
   error < 1 u); z = rq rq >> 127 (error < 2 0.8 1 + 1 = 2.6 u); S(z) = sum (-1)^i z^i / (2i + 1)! (i <= 16) and
   C(z) = sum (-1)^i z^i / (2i)! (i <= 17) by Horner, t = c_i - (z t >> 127), every partial sum positive (as in sc_fast:
   z < 0.65 and each term under a sixth (sin) or a half (cos) of the one before); per step the coefficient's rounding
   0.5 u, the product's floor 1 u, z's error times the next sum (1 / 6 or 1 / 2 + 1 %), the step's error times z < 0.65:
   S within (1.5 + 2.6 / 5.9) / 0.35 < 5.6 u, C within (1.5 + 2.6 / 1.98) / 0.35 < 8.1 u (the dropped terms: below
   0.01 u). sin |r| = rq S 2^-254 within (1 u S + |r| 5.6 u) < 5.5 u, at least 0.89 |r| >= 2^-30.2: relative error
   below 5.5 2^-127 2^30.2 < 2^-94; cos r = C 2^-127 within 8.1 u, at least 0.69. sf2_ziv keeps a result when both ends
   of [Y - err, Y + err] (Y the value's top 128 bits, normalized; err 2^36 units of Y for sin, 32 for cos, above those
   bounds with the window's floor) round to the same double: then the correctly rounded value. tests/sincos fast2:
   every value kept equals the old path's at degtorad_d of every float dir in [+0, 360], dir - 1 and dir + 1 */
static const uint32_t SF2_PI[6] = { 0x114cf98f, 0x252049c1, 0x701b839a, 0x9898cc51, 0x442d1846, 0x1921fb54 };
static const uint32_t SF2_S[17][4] = {
    { 0x00000000, 0x00000000, 0x00000000, 0x80000000 }, { 0x55555555, 0x55555555, 0x55555555, 0x15555555 },
    { 0x11111111, 0x11111111, 0x11111111, 0x01111111 }, { 0x68068068, 0x80680680, 0x06806806, 0x00068068 },
    { 0xc88e5001, 0x338faac1, 0xe3a556c7, 0x0000171d }, { 0x8fc97070, 0x89c71fce, 0xcc8acfea, 0x00000035 },
    { 0x8e0cc749, 0xa1b425f2, 0x5849184e, 0x00000000 }, { 0x476195ac, 0x9ccee07c, 0x006b9fcf, 0x00000000 },
    { 0x29ac9814, 0x1dc0c2b5, 0x0000654b, 0x00000000 }, { 0x055c9328, 0xd26d1a05, 0x0000004b, 0x00000000 },
    { 0xb9eae318, 0x2e371ded, 0x00000000, 0x00000000 }, { 0x1316381a, 0x001761b4, 0x00000000, 0x00000000 },
    { 0xe66e8b30, 0x000009f9, 0x00000000, 0x00000000 }, { 0xa356385c, 0x00000003, 0x00000000, 0x00000000 },
    { 0x01259f99, 0x00000000, 0x00000000, 0x00000000 }, { 0x000050d3, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000014, 0x00000000, 0x00000000, 0x00000000 } };                /* 2^127 / (2i + 1)!, rounded */
static const uint32_t SF2_C[18][4] = {
    { 0x00000000, 0x00000000, 0x00000000, 0x80000000 }, { 0x00000000, 0x00000000, 0x00000000, 0x40000000 },
    { 0x55555555, 0x55555555, 0x55555555, 0x05555555 }, { 0xd82d82d8, 0x82d82d82, 0x2d82d82d, 0x002d82d8 },
    { 0x0d00d00d, 0xd00d00d0, 0x00d00d00, 0x0000d00d }, { 0x2da7d4cd, 0xeb8e5de0, 0xc9f6ef13, 0x0000024f },
    { 0x36a61eb4, 0x3625ed51, 0x7bb63bfe, 0x00000004 }, { 0x2eb7c517, 0x301f2748, 0x064e5d2a, 0x00000000 },
    { 0xc476195b, 0xf9ccee07, 0x0006b9fc, 0x00000000 }, { 0x65deec01, 0x9e18ee5f, 0x000005a0, 0x00000000 },
    { 0x4044a0f5, 0xca857480, 0x00000003, 0x00000000 }, { 0xb6ff0a53, 0x0219c72d, 0x00000000, 0x00000000 },
    { 0x80cb97ac, 0x0000f967, 0x00000000, 0x00000000 }, { 0x3a17f1a9, 0x00000062, 0x00000000, 0x00000000 },
    { 0x2143144c, 0x00000000, 0x00000000, 0x00000000 }, { 0x0009c996, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000287, 0x00000000, 0x00000000, 0x00000000 }, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 } };  /* 2^127 / (2i)! */
static const uint32_t SF2_PW2[32] = {                                       /* 2^i: a shift by i as a dmulu */
    1u << 0, 1u << 1, 1u << 2, 1u << 3, 1u << 4, 1u << 5, 1u << 6, 1u << 7, 1u << 8, 1u << 9, 1u << 10, 1u << 11,
    1u << 12, 1u << 13, 1u << 14, 1u << 15, 1u << 16, 1u << 17, 1u << 18, 1u << 19, 1u << 20, 1u << 21, 1u << 22,
    1u << 23, 1u << 24, 1u << 25, 1u << 26, 1u << 27, 1u << 28, 1u << 29, 1u << 30, 1u << 31 };

static void sf2_mul(const uint32_t *a, const uint32_t *b, uint32_t *p)      /* p[0..7] = a b (4 limbs each) */
{
    int i, j;
    for (i = 0; i < 8; i++) p[i] = 0;
    for (i = 0; i < 4; i++) {
        uint64_t c = 0;
        for (j = 0; j < 4; j++) {
            c += (uint64_t)a[i] * b[j] + p[i + j];
            p[i + j] = (uint32_t)c;
            c >>= 32;
        }
        p[i + 4] = (uint32_t)c;
    }
}

static void sf2_series(const uint32_t *z, const uint32_t (*cf)[4], int n, uint32_t *t)   /* t = cf[0] - z (cf[1] - ..) */
{
    uint32_t p[8];
    int i, k;
    for (k = 0; k < 4; k++) t[k] = cf[n][k];
    for (i = n - 1; i >= 0; i--) {
        uint64_t b = 0;
        sf2_mul(z, t, p);
        for (k = 0; k < 4; k++) {                           /* t = cf[i] - (p >> 127) */
            uint32_t q = (p[k + 3] >> 31) | (p[k + 4] << 1);
            b = (uint64_t)cf[i][k] - q - b;
            t[k] = (uint32_t)b;
            b = (b >> 32) & 1;
        }
    }
}

static int sf2_clz(uint32_t v)                                             /* v != 0 */
{
    int n = 0;
    if (!(v >> 16)) { n += 16; v <<= 16; }
    if (!(v >> 24)) { n += 8; v <<= 8; }
    if (!(v >> 28)) { n += 4; v <<= 4; }
    if (!(v >> 30)) { n += 2; v <<= 2; }
    if (!(v >> 31)) n += 1;
    return n;
}

/* y[0..3] (y[3] != 0) times 2^e: 1 and the double's bits in *out when y - err and y + err round alike, the window
   normalized first (lo: the next lower limb, its bits shifted in) */
static int sf2_ziv(const uint32_t *y, uint32_t lo, uint64_t err, int e, uint64_t *out)
{
    uint32_t w[4], a[4], b[4];
    uint64_t c, ma, mb;
    int s = sf2_clz(y[3]), k;
    if (s) {                                                /* w = (y:lo) << s, the top 128 bits */
        uint32_t m = SF2_PW2[s];
        uint64_t pr = (uint64_t)lo * m;
        uint32_t cin = (uint32_t)(pr >> 32);
        for (k = 0; k < 4; k++) {
            pr = (uint64_t)y[k] * m;
            w[k] = (uint32_t)pr | cin;
            cin = (uint32_t)(pr >> 32);
        }
        e -= s;
    } else
        for (k = 0; k < 4; k++) w[k] = y[k];
    c = err;                                                /* a = w - err, b = w + err */
    {
        uint64_t bo = 0, ca = 0;
        for (k = 0; k < 4; k++) {
            uint32_t ek = (uint32_t)(k == 0 ? c : k == 1 ? c >> 32 : 0);
            bo = (uint64_t)w[k] - ek - bo;
            a[k] = (uint32_t)bo;
            bo = (bo >> 32) & 1;
            ca += (uint64_t)w[k] + ek;
            b[k] = (uint32_t)ca;
            ca >>= 32;
        }
        if (bo || ca || !(a[3] & 0x80000000u)) return 0;    /* (another binade: not tried) */
    }
    /* the 53-bit mantissa is bits 127 .. 75; round to nearest even on bits 74 .. 0 */
    {
        uint64_t ha = ((uint64_t)a[3] << 32) | a[2], hb = ((uint64_t)b[3] << 32) | b[2];
        uint32_t ra = (uint32_t)ha & 0x7ff, rb = (uint32_t)hb & 0x7ff;
        int sa = (a[1] | a[0]) != 0, sb = (b[1] | b[0]) != 0;
        ma = ha >> 11; mb = hb >> 11;
        if (ra > 0x400 || (ra == 0x400 && (sa || (ma & 1)))) ma++;
        if (rb > 0x400 || (rb == 0x400 && (sb || (mb & 1)))) mb++;
        if (ma != mb) return 0;
    }
    e += 75;                                                /* value = ma 2^e */
    if (ma >> 53) { ma >>= 1; e++; }
    *out = ((uint64_t)(e + 52 + 1023) << 52) | (ma & 0xfffffffffffffull);
    return 1;
}

/* sc_fast's contract (need 1 sin, 2 cos, 3 both; 1 when every needed value is kept), its range 2^-6 <= |a| < 8 */
static int sc_fast2(double a, int need, double *s, double *c)
{
    union { double d; uint64_t u; } v;
    uint64_t m, ah, h, l, sb = 0, cb = 0, sw;
    uint32_t R[6], rq[4], z[4], t[4], p[8], mlo, mhi;
    int e, k, i, rneg, sh;
    v.d = a;
    e = (int)((v.u >> 52) & 0x7ff);
    if (e < 1023 - 6 || e > 1023 + 2) return 0;
    m = (v.u & 0xfffffffffffffull) | (1ull << 52);
    sh = e - 1015;                                          /* 2 .. 10: ah = m 2^sh = |a| 2^60 */
    mlo = (uint32_t)m; mhi = (uint32_t)(m >> 32);
    sw = (uint64_t)mlo * SF2_PW2[sh];
    ah = ((uint64_t)(mhi * SF2_PW2[sh] + (uint32_t)(sw >> 32)) << 32) | (uint32_t)sw;
    sc_mul(ah, SC_I64, &h, &l);
    k = (int)((h + (1ull << 59)) >> 60);                     /* round(|a| 2 / pi), 0 .. 5 (sc_fast's) */
    {                                                       /* R = ah 2^128 - k P */
        uint64_t mc = 0, bo = 0;
        for (i = 0; i < 6; i++) {
            uint32_t ai = i == 4 ? (uint32_t)ah : i == 5 ? (uint32_t)(ah >> 32) : 0, kp;
            mc += (uint64_t)SF2_PI[i] * (uint32_t)k;
            kp = (uint32_t)mc;
            mc >>= 32;
            bo = (uint64_t)ai - kp - bo;
            R[i] = (uint32_t)bo;
            bo = (bo >> 32) & 1;
        }
        rneg = (int)bo;
        if (rneg) {                                         /* R = -R */
            uint64_t ca = 1;
            for (i = 0; i < 6; i++) {
                ca += (uint32_t)~R[i];
                R[i] = (uint32_t)ca;
                ca >>= 32;
            }
        }
    }
    if (R[5] > 0x0ccccccc || (R[5] == 0 && R[4] < (1u << 30))) return 0;      /* |r| > 0.8, |r| < 2^-30 */
    for (i = 0; i < 4; i++) rq[i] = (R[i + 1] >> 29) | (R[i + 2] << 3);
    sf2_mul(rq, rq, p);
    for (i = 0; i < 4; i++) z[i] = (p[i + 3] >> 31) | (p[i + 4] << 1);
    if (need & ((k & 1) ? 2 : 1)) {                          /* sin r = rq S 2^-254 */
        sf2_series(z, SF2_S, 16, t);
        sf2_mul(rq, t, p);
        i = p[7] ? 7 : p[6] ? 6 : 5;                         /* (>= 2^223 0.89: p[6] or p[7]) */
        if (!sf2_ziv(p + i - 3, p[i - 4], 1ull << 36, 32 * (i - 3) - 254, &sb)) return 0;
        if (rneg) sb |= 0x8000000000000000ull;
    }
    if (need & ((k & 1) ? 1 : 2)) {                          /* cos r = C 2^-127 */
        sf2_series(z, SF2_C, 17, t);
        if (!sf2_ziv(t, 0, 32, -127, &cb)) return 0;
    }
    {                                                       /* sc_fast's quadrants, then sin(-x) = -sin x on the bits */
        uint64_t neg = v.u & 0x8000000000000000ull, so, co;
        switch (k & 3) {
        case 0: so = sb; co = cb; break;
        case 1: so = cb; co = sb ^ 0x8000000000000000ull; break;
        case 2: so = sb ^ 0x8000000000000000ull; co = cb ^ 0x8000000000000000ull; break;
        default: so = cb ^ 0x8000000000000000ull; co = sb; break;
        }
        v.u = so ^ neg; *s = v.d;
        v.u = co; *c = v.d;
    }
    return 1;
}

/* sin and cos of a by the slow path (cr_reduce, sincos_r): psincos_cr's own */
static void sc_slow(double a, double *s, double *c)
{
    double sr, cr;
    int q;
    ddbl r = cr_reduce(a, &q);
    sr = sincos_r(r, 1);
    cr = sincos_r(r, 0);
    switch (q) {
    case 0: *s = sr; *c = cr; break;
    case 1: *s = cr; *c = -sr; break;
    case 2: *s = -sr; *c = -cr; break;
    default: *s = -cr; *c = sr; break;
    }
}

/* the arguments sc_fast leaves to the slow path most (host count over the routes, 632 of 683 calls): +-0 (below its
   2^-6) and degtorad of 90, 180, 270 (pi / 2, pi, 3 pi / 2 as doubles: tiny results, Ziv's test fails). Their slow-path
   sin and cos are kept the first time they are computed (the functions read only their argument); psin_cr / pcos_cr
   give the bits psincos_cr gives (its comment). The host builds compute them again on every use and compare */
static const uint64_t sc_kbits[5] = { 0x0000000000000000ull, 0x8000000000000000ull, 0x3ff921fb54442d18ull,
                                      0x400921fb54442d18ull, 0x4012d97c7f3321d2ull };
static double sc_ks[5], sc_kc[5];
static uint8_t sc_kok[5];
static int sc_kept(double a, double *s, double *c)
{
    union { double d; uint64_t u; } v;
    int k;
    v.d = a;
    for (k = 0; k < 5; k++)
        if (v.u == sc_kbits[k]) {
            if (!sc_kok[k]) { sc_slow(a, &sc_ks[k], &sc_kc[k]); sc_kok[k] = 1; }
#ifdef PLAY_STATS
            {
                double ts, tc;
                union { double d; uint64_t u; } a1, a2, b1, b2;
                sc_slow(a, &ts, &tc);
                a1.d = ts; a2.d = sc_ks[k]; b1.d = tc; b2.d = sc_kc[k];
                if (a1.u != a2.u || b1.u != b2.u) { fprintf(stderr, "sc_kept: %d differs\n", k); abort(); }
            }
#endif
            *s = sc_ks[k];
            *c = sc_kc[k];
            return 1;
        }
    return 0;
}

/* sin / cos correctly rounded: cr_trig_dd's bits (its quadrant signs negate the rounded result exactly). sincos_r
   keeps its fast sums only when Ziv's test shows they round as the series does (tests/sincos: every float direction
   in degrees, and random double directions as the piranhas' and move_dir's, against cr_trig_dd) */
double psin_cr(double x)
{
    int q;
    ddbl r;
    double fs, fc;
    if (sc_fast(x, 1, &fs, &fc) || sc_kept(x, &fs, &fc) || sc_fast2(x, 1, &fs, &fc)) return fs;
    r = cr_reduce(x, &q);
    switch (q) {
    case 0: return sincos_r(r, 1);
    case 1: return sincos_r(r, 0);
    case 2: return -sincos_r(r, 1);
    default: return -sincos_r(r, 0);
    }
}

double pcos_cr(double x)
{
    int q;
    ddbl r;
    double fs, fc;
    if (sc_fast(x, 2, &fs, &fc) || sc_kept(x, &fs, &fc) || sc_fast2(x, 2, &fs, &fc)) return fc;
    r = cr_reduce(x, &q);
    switch (q) {
    case 0: return sincos_r(r, 0);
    case 1: return -sincos_r(r, 1);
    case 2: return -sincos_r(r, 0);
    default: return sincos_r(r, 1);
    }
}

/* sin and cos of a with one reduction (bat_fly): the same bits as psin_cr(a), pcos_cr(a) */
void psincos_cr(double a, double *s, double *c)
{
    if (sc_fast(a, 3, s, c) || sc_kept(a, s, c) || sc_fast2(a, 3, s, c)) return;
    sc_slow(a, s, c);
}

/* ---- fdlibm's float atan2f (e_atan2f.c, s_atanf.c: glibc's flt-32 versions, in float arithmetic): the runner's
   point_direction (Observed: all 33 bat directions of build/trace/p5_spider_s121 records 176-208) -------------- */
static int32_t fword(float x) { union { float f; int32_t i; } v; v.f = x; return v.i; }
static float wordf(int32_t i) { union { float f; int32_t i; } v; v.i = i; return v.f; }
static const float atanhif[] = { 4.6364760399e-01f, 7.8539812565e-01f, 9.8279368877e-01f, 1.5707962513e+00f };
static const float atanlof[] = { 5.0121582440e-09f, 3.7748947079e-08f, 3.4473217170e-08f, 7.5497894159e-08f };
static const float aTf[] = { 3.3333334327e-01f, -2.0000000298e-01f, 1.4285714924e-01f, -1.1111110449e-01f,
                             9.0908870101e-02f, -7.6918758452e-02f, 6.6610731184e-02f, -5.8335702866e-02f,
                             4.9768779427e-02f, -3.6531571299e-02f, 1.6285819933e-02f };

static float fatanf(float x)
{
    float w, s1, s2, z;
    int32_t ix, hx = fword(x), id;
    ix = hx & 0x7fffffff;
    if (ix >= 0x4c000000) return hx > 0 ? atanhif[3] + atanlof[3] : -atanhif[3] - atanlof[3];
    if (ix < 0x3ee00000) {
        if (ix < 0x31000000) return x;
        id = -1;
    } else {
        x = x < 0 ? -x : x;
        if (ix < 0x3f980000) {
            if (ix < 0x3f300000) { id = 0; x = (2.0f * x - 1.0f) / (2.0f + x); }
            else { id = 1; x = (x - 1.0f) / (x + 1.0f); }
        } else if (ix < 0x401c0000) { id = 2; x = (x - 1.5f) / (1.0f + 1.5f * x); }
        else { id = 3; x = -1.0f / x; }
    }
    z = x * x;
    w = z * z;
    s1 = z * (aTf[0] + w * (aTf[2] + w * (aTf[4] + w * (aTf[6] + w * (aTf[8] + w * aTf[10])))));
    s2 = w * (aTf[1] + w * (aTf[3] + w * (aTf[5] + w * (aTf[7] + w * aTf[9]))));
    if (id < 0) return x - x * (s1 + s2);
    z = atanhif[id] - ((x * (s1 + s2) - atanlof[id]) - x);
    return hx < 0 ? -z : z;
}

float patan2f(float y, float x)
{
    const float pi_o_2 = 1.5707963705e+00f, pi = 3.1415927410e+00f, pi_lo = -8.7422776573e-08f, tiny = 1.0e-30f;
    float z;
    int32_t k, m, hx = fword(x), hy = fword(y), ix, iy;
    ix = hx & 0x7fffffff;
    iy = hy & 0x7fffffff;
    if (hx == 0x3f800000) return fatanf(y);
    m = ((hy >> 31) & 1) | ((hx >> 30) & 2);
    if (iy == 0) {
        switch (m) {
        case 0: case 1: return y;
        case 2: return pi + tiny;
        default: return -pi - tiny;
        }
    }
    if (ix == 0) return hy < 0 ? -pi_o_2 - tiny : pi_o_2 + tiny;
    k = (iy - ix) >> 23;
    if (k > 26) z = pi_o_2 + 0.5f * pi_lo;
    else if (hx < 0 && k < -26) z = 0.0f;
    else {
        float q = y / x;
        z = fatanf(q < 0 ? -q : q);
    }
    switch (m) {
    case 0: return z;
    case 1: return wordf(fword(z) ^ (int32_t)0x80000000);
    case 2: return pi - (z - pi_lo);
    default: return (z - pi_lo) - pi;
    }
}

/* GameMaker's point_distance / point_direction (degrees, 0 <= d < 360) */
double point_distance_d(double x1, double y1, double x2, double y2)
{
    double dx = x2 - x1, dy = y2 - y1;
    return psqrt(dx * dx + dy * dy);
}

/* point_direction in single precision (Observed: build/trace/p5_spider_s121 records 176 and 180, the bat's xVel /
   yVel = cos / -sin of 191.44105529785156 and 210.46554565429688): a = atan2f(y2 - y1, x2 - x1),
   dd = 180.f * a / (float)pi, then dd <= 0 ? -dd : 360.f - dd, all in float (the double formula and the float
   one with a * (180 / pi) each miss one of the two). atan2f here: patan2f, glibc's (fdlibm float) */
double point_direction_d(double x1, double y1, double x2, double y2)
{
    float a = patan2f((float)(y2 - y1), (float)(x2 - x1));
    float dd = 180.f * a / (float)3.14159265358979323846;
    return (double)(dd <= 0 ? -dd : 360.f - dd);
}

/* degtorad(d) = d * pi / 180 (the form that gives the runner's bat velocities) */
double degtorad_d(double d) { return d * 3.14159265358979323846 / 180.0; }
