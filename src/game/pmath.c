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

double psqrt(double d)
{
    double s, prev = 0;
    int it;
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

static double cr_trig(double a, int want_sin)
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

double psin_cr(double x) { return cr_trig(x, 1); }
double pcos_cr(double x) { return cr_trig(x, 0); }

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
