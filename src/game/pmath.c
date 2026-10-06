/* Real functions the enemies use (no libm on the SH-2): sqrt, sin, cos, atan2, point_distance, point_direction.
 * sin / cos / atan are fdlibm's (s_sin.c, s_cos.c, k_sin.c, k_cos.c, e_rem_pio2.c medium path, e_atan2.c); the
 * runner calls its C library's, so a last-bit difference shows as an xVel / yVel difference in tools/playcmp.py. */
#include "pmath.h"
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

/* ---- fdlibm sin / cos ---------------------------------------------------------------------------------- */
static const double
    S1 = -1.66666666666666324348e-01, S2 = 8.33333333332248946124e-03, S3 = -1.98412698298579493134e-04,
    S4 = 2.75573137070700676789e-06, S5 = -2.50507602534068634195e-08, S6 = 1.58969099521155010221e-10,
    C1 = 4.16666666666666019037e-02, C2 = -1.38888888888741095749e-03, C3 = 2.48015872894767294178e-05,
    C4 = -2.75573143513906633035e-07, C5 = 2.08757232129817482790e-09, C6 = -1.13596475577881948265e-11;

static double k_sin(double x, double y, int iy)
{
    double z, r, v;
    int32_t ix = hiw(x) & 0x7fffffff;
    if (ix < 0x3e400000 && (int)x == 0) return x;
    z = x * x;
    v = z * x;
    r = S2 + z * (S3 + z * (S4 + z * (S5 + z * S6)));
    if (iy == 0) return x + v * (S1 + z * r);
    return x - ((z * (0.5 * y - v * r) - y) - v * S1);
}

static double k_cos(double x, double y)
{
    double a, hz, z, r, qx;
    int32_t ix = hiw(x) & 0x7fffffff;
    if (ix < 0x3e400000 && (int)x == 0) return 1.0;
    z = x * x;
    r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * C6)))));
    if (ix < 0x3FD33333) return 1.0 - (0.5 * z - (z * r - x * y));
    if (ix > 0x3fe90000) qx = 0.28125;
    else qx = mkd(ix - 0x00200000, 0);
    hz = 0.5 * z - qx;
    a = 1.0 - qx;
    return a - (hz - (z * r - x * y));
}

static const double
    invpio2 = 6.36619772367581382433e-01, pio2_1 = 1.57079632673412561417e+00, pio2_1t = 6.07710050650619224932e-11,
    pio2_2 = 6.07710050630396597660e-11, pio2_2t = 2.02226624879595063154e-21, pio2_3 = 2.02226624871116645580e-21,
    pio2_3t = 8.47842766036889956997e-32;

/* e_rem_pio2.c for |x| < 2^19 * pi/2 (the medium case; the enemies' angles are below 2 pi) */
static int rem_pio2(double x, double *y)
{
    double z, w, t, r, fn;
    int32_t ix = hiw(x) & 0x7fffffff, hx = hiw(x);
    int n, i, j;
    if (ix <= 0x3fe921fb) { y[0] = x; y[1] = 0; return 0; }
    if (ix < 0x4002d97c) {                                 /* |x| < 3pi/4 */
        if (hx > 0) {
            z = x - pio2_1;
            if (ix != 0x3ff921fb) { y[0] = z - pio2_1t; y[1] = (z - y[0]) - pio2_1t; }
            else { z -= pio2_2; y[0] = z - pio2_2t; y[1] = (z - y[0]) - pio2_2t; }
            return 1;
        }
        z = x + pio2_1;
        if (ix != 0x3ff921fb) { y[0] = z + pio2_1t; y[1] = (z - y[0]) + pio2_1t; }
        else { z += pio2_2; y[0] = z + pio2_2t; y[1] = (z - y[0]) + pio2_2t; }
        return -1;
    }
    t = x < 0 ? -x : x;
    n = (int)(t * invpio2 + 0.5);
    fn = (double)n;
    r = t - fn * pio2_1;
    w = fn * pio2_1t;
    j = ix >> 20;
    y[0] = r - w;
    i = j - ((hiw(y[0]) >> 20) & 0x7ff);
    if (i > 16) {
        t = r;
        w = fn * pio2_2;
        r = t - w;
        w = fn * pio2_2t - ((t - r) - w);
        y[0] = r - w;
        i = j - ((hiw(y[0]) >> 20) & 0x7ff);
        if (i > 49) {
            t = r;
            w = fn * pio2_3;
            r = t - w;
            w = fn * pio2_3t - ((t - r) - w);
            y[0] = r - w;
        }
    }
    y[1] = (r - y[0]) - w;
    if (hx < 0) { y[0] = -y[0]; y[1] = -y[1]; return -n; }
    return n;
}

double psin(double x)
{
    double y[2];
    int n;
    if ((hiw(x) & 0x7fffffff) <= 0x3fe921fb) return k_sin(x, 0, 0);
    n = rem_pio2(x, y);
    switch (n & 3) {
    case 0: return k_sin(y[0], y[1], 1);
    case 1: return k_cos(y[0], y[1]);
    case 2: return -k_sin(y[0], y[1], 1);
    default: return -k_cos(y[0], y[1]);
    }
}

double pcos(double x)
{
    double y[2];
    int n;
    if ((hiw(x) & 0x7fffffff) <= 0x3fe921fb) return k_cos(x, 0);
    n = rem_pio2(x, y);
    switch (n & 3) {
    case 0: return k_cos(y[0], y[1]);
    case 1: return -k_sin(y[0], y[1], 1);
    case 2: return -k_cos(y[0], y[1]);
    default: return k_sin(y[0], y[1], 1);
    }
}

/* ---- fdlibm atan2 (e_atan2.c) on pscript.c's atan ------------------------------------------------------- */
double patan2(double y, double x)
{
    static const double pi_o_2 = 1.5707963267948965580E+00,
                        pi = 3.1415926535897931160E+00, pi_lo = 1.2246467991473531772E-16;
    double z;
    int32_t hx = hiw(x), ix = hx & 0x7fffffff, hy = hiw(y), iy = hy & 0x7fffffff, k, m;
    uint32_t lx = low(x), ly = low(y);
    if (((hx - 0x3ff00000) | (int32_t)lx) == 0) return patan(y);  /* x = 1.0 */
    m = ((hy >> 31) & 1) | ((hx >> 30) & 2);
    if ((iy | (int32_t)ly) == 0) {
        switch (m) {
        case 0: case 1: return y;
        case 2: return pi;
        default: return -pi;
        }
    }
    if ((ix | (int32_t)lx) == 0) return hy < 0 ? -pi_o_2 : pi_o_2;
    k = (iy - ix) >> 20;
    if (k > 60) z = pi_o_2 + 0.5 * pi_lo;
    else if (hx < 0 && k < -60) z = 0.0;
    else z = patan((y / x) < 0 ? -(y / x) : (y / x));
    switch (m) {
    case 0: return z;
    case 1: return -z;
    case 2: return pi - (z - pi_lo);
    default: return (z - pi_lo) - pi;
    }
}

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
static __attribute__((unused)) double cr_trig_dd(double a, int want_sin)
{
    static const double P1 = 1.57079632673412561417e+00, P2 = 6.07710050630396597660e-11,
                        P3 = 2.02226624871116645580e-21, P4 = 8.47842766036889956997e-32;
    double k = (double)(int32_t)(a * invpio2 + (a < 0 ? -0.5 : 0.5));
    ddbl r, t;
    int q;
    r = two_sum(a, -k * P1);                                   /* k * P1 exact (P1: 33 bits, |k| small) */
    r = dd_add(r, two_sum(-k * P2, 0));                        /* k * P2 exact */
    two_prod(-k, P3, &t.hi, &t.lo);
    r = dd_add(r, t);
    t.hi = -k * P4;
    t.lo = 0;
    r = dd_add(r, t);
    q = ((int)k) & 3;
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
    r = two_sum(a, -k * P1);
    r = dd_add(r, two_sum(-k * P2, 0));
    two_prod(-k, P3, &t.hi, &t.lo);
    r = dd_add(r, t);
    t.hi = -k * P4;
    t.lo = 0;
    r = dd_add(r, t);
    *q = ((int)k) & 3;
    return r;
}

/* sin / cos correctly rounded: cr_trig_dd's bits (its quadrant signs negate the rounded result exactly). sincos_r
   keeps its fast sums only when Ziv's test shows they round as the series does (tests/sincos: every float direction
   in degrees, and random double directions as the piranhas' and move_dir's, against cr_trig_dd) */
double psin_cr(double x)
{
    int q;
    ddbl r = cr_reduce(x, &q);
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
    ddbl r = cr_reduce(x, &q);
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
   one with a * (180 / pi) each miss one of the two). atan2f here: fdlibm's double atan2 rounded to float */
double point_direction_d(double x1, double y1, double x2, double y2)
{
    float a = patan2f((float)(y2 - y1), (float)(x2 - x1));
    float dd = 180.f * a / (float)3.14159265358979323846;
    return (double)(dd <= 0 ? -dd : 360.f - dd);
}

/* degtorad(d) = d * pi / 180 (the form that gives the runner's bat velocities) */
double degtorad_d(double d) { return d * 3.14159265358979323846 / 180.0; }
