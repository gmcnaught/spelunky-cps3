/* pworld.c ovl_frac (overlap_at's float path on ints) against overlap_float on random instance pairs: every sprite,
 * positions whole, fractional (random, exact halves, a few units around a half, tiny), small and large, scales +-1
 * (and 2, angle 90: the cases ovl_frac leaves to the float path), mask frames, the second instance near the first.
 * make -C test/host build/host/ovlfrac && build/host/ovlfrac [n] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/game/pworld.c"

static uint64_t rs = 0x2545f4914f6cdd1dull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 16); }

/* a float near v: whole, random fraction, a half, a few units in the last place around a half or a whole, tiny */
static float near_(float v)
{
    union { float f; uint32_t u; } b;
    float w = (float)(int32_t)v;
    switch (rnd() % 6) {
    case 0: return w;
    case 1: return w + (float)(rnd() & 0xffffff) / 16777216.0f;
    case 2: return w + 0.5f;
    case 3: b.f = w + (rnd() & 1 ? 0.5f : 1.0f); b.u += (int32_t)(rnd() % 7) - 3; return b.f;
    case 4: return w + (float)(rnd() % 256) / 256.0f;
    default: return v;
    }
}

static float place(void)
{
    switch (rnd() % 5) {
    case 0: return (float)(rnd() % 64) + 1.0f;
    case 1: return (float)(rnd() % 30000) + 1.0f;
    case 2: return (float)(32700 + rnd() % 68);
    default: return (float)(rnd() % 1500) + 16.0f;
    }
}

static void inst(int i, float x, float y)
{
    struct pin *p = &PW.in[i];
    int s = (int)(rnd() % GSPR_COUNT);
    if (rnd() % 3 == 0) {                              /* prefer precise sprites with masks */
        int k;
        for (k = 0; k < 64; k++) {
            s = (int)(rnd() % GSPR_COUNT);
            if (gsprcol[s].kind == 1 && psprite[s].nmasks > 0) break;
        }
    }
    memset(p, 0, sizeof *p);
    p->alive = 1;
    p->obj = 0;
    p->spr = (int16_t)s;
    p->mask = -1;
    p->x = near_(x); p->y = near_(y);
    p->ix = p->iy = PXY_UNK;
    p->xscale = rnd() % 2 ? 1.0f : -1.0f;
    p->yscale = rnd() % 8 ? 1.0f : -1.0f;
    if (rnd() % 40 == 0) p->xscale = 2.0f;
    p->angle = rnd() % 40 == 0 ? 90.0f : 0.0f;
    p->img = (img_t)(rnd() % 12) + (rnd() % 2 ? 0.5f : 0.0f);
    p->bbk = 0;
}

int main(int argc, char **argv)
{
    long n = argc > 1 ? atol(argv[1]) : 2000000, i, done = 0, hits = 0, prec = 0, fb = 0, scan = 0, shits = 0;
    for (i = 0; i < n; i++) {
        float x = place(), y = place();
        int r, f;
        inst(1, x, y);
        { int d = rnd() % 2 ? 64 : 24; inst(2, x + (float)((int)(rnd() % d) - d / 2), y + (float)((int)(rnd() % d) - d / 2)); }
        r = ovl_frac(1, 2);
        PW.in[1].bbk = PW.in[2].bbk = 0;
        f = overlap_float(1, 0.0, 0.0, 2);
        if (r < 0) { fb++; continue; }
        if (r != f) {
            printf("differs at case %ld: ovl_frac %d float %d (x %.9g %.9g s %g %g spr %d | x %.9g %.9g s %g %g spr %d)\n", i, r, f,
                   (double)PW.in[1].x, (double)PW.in[1].y, (double)PW.in[1].xscale, (double)PW.in[1].yscale, PW.in[1].spr,
                   (double)PW.in[2].x, (double)PW.in[2].y, (double)PW.in[2].xscale, (double)PW.in[2].yscale, PW.in[2].spr);
            return 1;
        }
        done++; hits += r; prec += precise(1) || precise(2);
        {   /* the boxes overlap and one is precise: the pixel scan decided */
            double l, t, rr, b, l2, t2, r2, b2;
            if (pin_bbox(1, &l, &t, &rr, &b) && pin_bbox(2, &l2, &t2, &r2, &b2) && l < r2 && l2 < rr && t < b2 && t2 < b &&
                (precise(1) || precise(2))) { scan++; shits += r; }
        }
    }
    printf("OVLFRAC %ld cases: %ld on ints equal to the float path (%ld hits, %ld with a precise one; %ld pixel scans, %ld hits), "
           "%ld to the float path\n", n, done, hits, prec, scan, shits, fb);
    return 0;
}
