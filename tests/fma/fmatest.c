/* src/sh2/fma.c (compiled here as sh2_fmaf) against the host libm's fmaf (arm64 / x86-64: the FMA instruction),
 * bit for bit (NaN: any NaN). Case sets: random bit patterns over the whole range; cancellation (z near -x*y);
 * sums within 2^-24 ulp of a float midpoint (where double rounding fails); exact-midpoint products plus a tiny z;
 * subnormal results; the collision tree's rectangles (pcol.c: fractional widths and areas).
 * Also counts how often (float)((double)x * y + z) (two roundings) differs from fmaf.
 *   make -C tests/fma && build/host/fmatest [millions per set]   exit status 1 on a difference */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma STDC FP_CONTRACT OFF

float sh2_fmaf(float x, float y, float z);

static uint64_t s = 0x9e3779b97f4a7c15ull;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 16); }
static float u2f(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static uint32_t f2u(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static float rf(int elo, int ehi)                  /* random sign, biased exponent in [elo, ehi], mantissa */
{
    uint32_t e = (uint32_t)(elo + (int)(rnd() % (uint32_t)(ehi - elo + 1)));
    return u2f((rnd() & 0x80000000u) | (e << 23) | (rnd() & 0x7fffff));
}

static long n, bad, dbl;
static void check(float x, float y, float z)
{
    volatile float vx = x, vy = y, vz = z;
    float a = fmaf(vx, vy, vz), b = sh2_fmaf(vx, vy, vz);
    float d = (float)((double)vx * (double)vy + (double)vz);
    n++;
    if (!(isnan(a) && isnan(d)) && f2u(a) != f2u(d)) dbl++;
    if (isnan(a) && isnan(b)) return;
    if (f2u(a) != f2u(b)) {
        if (bad++ < 20)
            printf("DIFF x=%a y=%a z=%a libm=%a (%08x) sh2=%a (%08x)\n", x, y, z, a, f2u(a), b, f2u(b));
    }
}

int main(int argc, char **argv)
{
    long m = (argc > 1 ? atol(argv[1]) : 2) * 1000000L, k;
    static const float sp[] = { 0.0f, -0.0f, 1.0f, -1.0f, 0x1p-149f, -0x1p-149f, 0x1p-126f, 0x1.fffffcp-127f,
                                0x1.fffffep127f, -0x1.fffffep127f, INFINITY, -INFINITY, NAN, 0.5f, 3.0f, 0x1p-75f };
    int i, j, l, ns = (int)(sizeof sp / sizeof sp[0]);
    long n0;
    /* specials: every triple */
    for (i = 0; i < ns; i++)
        for (j = 0; j < ns; j++)
            for (l = 0; l < ns; l++) check(sp[i], sp[j], sp[l]);
    printf("specials       %8ld cases\n", n);
    /* random bit patterns */
    n0 = n;
    for (k = 0; k < m; k++) check(u2f(rnd()), u2f(rnd()), u2f(rnd()));
    printf("random bits    %8ld cases\n", n - n0);
    /* random, exponents near each other (no NaN / inf inputs) */
    n0 = n;
    for (k = 0; k < m; k++) check(rf(100, 154), rf(100, 154), rf(73, 181));
    printf("near range     %8ld cases\n", n - n0);
    /* cancellation: z = -round(x * y) moved by up to 2 ulp, or z = -x (y near 1) */
    n0 = n;
    for (k = 0; k < m; k++) {
        float x = rf(1, 254), y = rf(1, 254), p = x * y, z;
        if (isinf(p) || p == 0) { y = rf(110, 144); x = rf(110, 144); p = x * y; }
        z = u2f(f2u(-p) + (int32_t)(rnd() % 5) - 2);
        check(x, y, z);
        y = u2f(0x3f800000u + (rnd() % 7) - 3);
        check(x, y, -x);
    }
    printf("cancellation   %8ld cases\n", n - n0);
    /* within 2^-24 ulp of a float midpoint: z = round_float(mid - x * y), mid = f +- ulp(f) / 2, f = round(x * y)
       (computed in double: exact for these exponents) */
    n0 = n;
    for (k = 0; k < m; k++) {
        float x = rf(97, 157), y = rf(97, 157), f;
        double p = (double)x * y, mid, ulp;
        int e;
        f = (float)p;
        frexp((double)f, &e);
        ulp = ldexp(1.0, e - 24);
        mid = (double)f + ((rnd() & 1) ? ulp / 2 : -ulp / 2);
        check(x, y, (float)(mid - p));
        check(x, y, (float)(mid - p) + u2f(0x34000000u + (rnd() % 0x01000000u)) * (float)(mid - p));
    }
    printf("near midpoint  %8ld cases\n", n - n0);
    /* exact midpoint products (25 significant bits ending in 1) plus a tiny or zero z */
    n0 = n;
    for (k = 0; k < m; k++) {
        uint32_t a = (rnd() & 0xfff) | 0x1001, b = (rnd() & 0x1fff) | 0x1001;  /* 13 + 14 bits, odd */
        float x = ldexpf((float)a, (int)(rnd() % 40) - 20), y = ldexpf((float)b, (int)(rnd() % 40) - 20);
        double p = (double)x * y;
        float z = (float)(p * ldexp(1.0, -(int)(30 + rnd() % 40)));
        if (rnd() & 1) z = -z;
        if ((rnd() & 7) == 0) z = 0;
        check(x, y, z);
    }
    printf("exact midpoint %8ld cases\n", n - n0);
    /* subnormal and near-underflow results */
    n0 = n;
    for (k = 0; k < m; k++) {
        float x = rf(1, 70), y = rf(1, 70), z = rf(0, 3);
        check(x, y, z);
        check(x, y, -(x * y) + rf(0, 2));
        check(rf(0, 1), rf(120, 160), rf(0, 2));
    }
    printf("subnormal      %8ld cases\n", n - n0);
    /* the collision tree: rectangle sides on a 1/64 grid up to 2048, areas of nearby rectangles */
    n0 = n;
    for (k = 0; k < m; k++) {
        float w = (float)(rnd() % 131072) / 64.0f, h = (float)(rnd() % 131072) / 64.0f;
        float w2 = w - (float)(rnd() % 64) / 64.0f, h2 = h - (float)(rnd() % 64) / 64.0f;
        float xs = (rnd() & 1) ? 1.0f : u2f(0x3f000000u + (rnd() % 0x01000000u));
        check(w, h, -(w2 * h2));
        check((float)(int)(rnd() % 64) - 32.0f, xs, (float)(rnd() % 65536) / 16.0f);
    }
    printf("rectangles     %8ld cases\n", n - n0);
    printf("total %ld cases, %ld differ from libm fmaf; (float)((double)x*y+z) differs in %ld\n", n, bad, dbl);
    return bad != 0;
}
