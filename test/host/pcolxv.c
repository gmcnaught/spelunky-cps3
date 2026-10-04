/* pcol.c's exact integer path of the R-tree area arithmetic (struct xv: rarea, rcomb_growth, xv_sub, xv_gt, xv_eq,
 * kint, the rectangles as keys) against the plain float expressions, bit for bit but -0 == +0, on random rectangles: whole numbers small and near 2^14,
 * fractional, mixed, signs, areas near 2^24.  make -C test/host build/host/pcolxv && build/host/pcolxv */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../../src/game/pcol.c"

static uint64_t rs = 88172645463325252ull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 16); }
/* the value's bits with -0 as +0: areas and growths only feed comparisons, which take -0 == +0 */
static uint32_t fb(float f) { uint32_t u; memcpy(&u, &f, 4); return u == 0x80000000u ? 0 : u; }

static float rv(void)
{
    switch (rnd() % 6) {
    case 0: return (float)((int)(rnd() % 1400) - 200);                         /* whole, room range */
    case 1: return (float)((int)(rnd() % 32768) - 16384);                       /* whole, around 2^14 */
    case 2: return (float)((int)(rnd() % 1400) - 200) + (float)(rnd() % 64) / 64.0f;   /* fractional */
    case 3: return ((float)(rnd() % 100000) - 50000.0f) * 0.37f;
    case 4: return (float)((int)(rnd() % 9000));                               /* areas toward 2^24 */
    default: return (rnd() & 1) ? -0.0f : 0.0f;
    }
}

static void rect(float *r)
{
    float a = rv(), b = rv(), c = rv(), d = rv();
    r[0] = a < c ? a : c; r[2] = a < c ? c : a;
    r[1] = b < d ? b : d; r[3] = b < d ? d : b;
}

int main(void)
{
    long n, bad = 0, ints = 0;
    for (n = 0; n < 20000000; n++) {
        float a[4], b[4], o[4], fa, fg, fw;
        rk ka[4], kb[4], ko[4];
        struct xv xa, xb, xg, xw;
        int32_t t;
        rect(a); rect(b);
        keys_of(ka, a); keys_of(kb, b);
        fa = (a[2] - a[0]) * (a[3] - a[1]);
        rarea(&xa, ka);
        if (xa.k & 1) ints++;
        if (fb(xv_f(&xa)) != fb(fa)) { if (bad++ < 10) printf("area %a %a %a %a: %a vs %a\n", a[0], a[1], a[2], a[3], xv_f(&xa), fa); }
        o[0] = a[0] < b[0] ? a[0] : b[0]; o[1] = a[1] < b[1] ? a[1] : b[1];
        o[2] = a[2] > b[2] ? a[2] : b[2]; o[3] = a[3] > b[3] ? a[3] : b[3];
        fg = fmaf(o[2] - o[0], o[3] - o[1], -fa);
        rcomb_growth(&xg, ka, kb, &xa);
        rcomb(ko, ka, kb);
        if (fb(kf(ko[0])) != fb(o[0]) || fb(kf(ko[3])) != fb(o[3])) { if (bad++ < 10) printf("rcomb\n"); }
        if (fb(xv_f(&xg)) != fb(fg)) { if (bad++ < 10) printf("growth: %a vs %a\n", xv_f(&xg), fg); }
        rarea(&xb, kb);
        fw = fg - ((b[2] - b[0]) * (b[3] - b[1]));
        xv_sub(&xw, &xg, &xb);
        if (fb(xv_f(&xw)) != fb(fw)) { if (bad++ < 10) printf("sub: %a vs %a\n", xv_f(&xw), fw); }
        {
            float fbb = (b[2] - b[0]) * (b[3] - b[1]);
            if (xv_gt(&xa, &xb) != (fa > fbb) || xv_eq(&xa, &xb) != (fa == fbb)) { if (bad++ < 10) printf("cmp\n"); }
        }
        {
            float f = rv();
            if (kint(fkey(f), &t) && (float)t != f) { if (bad++ < 10) printf("kint %a -> %d\n", f, t); }
            if (!kint(fkey(f), &t) && f == (float)(int32_t)f && fabsf(f) < 16384.0f) { if (bad++ < 10) printf("kint missed %a\n", f); }
            if (fb(kf(fkey(f))) != fb(f)) { if (bad++ < 10) printf("kf %a\n", f); }
        }
    }
    {   /* ikey: every whole number below 2^24 */
        int32_t v;
        for (v = -16777215; v <= 16777215; v++)
            if (ikey(v) != fkey((float)v)) { if (bad++ < 10) printf("ikey %d\n", v); }
        n += 2 * 16777215 + 1;
    }
    printf("%ld cases (%ld areas on the integer path), %ld differ\n", n, ints, bad);
    return bad != 0;
}
