/* characterStepEvent's slope branch (src/game/pplayer.c :973): the point_distance loop's result s against
 * DGT(s, a) for a = |xVelInteger|. The port skips the loop when d2 <= a * a; this checks that DGT is then false:
 * every a in 0 .. 64 with d2 = a * a, the 2^20 doubles below a * a, d2 = dx * dx + dy * dy for random float
 * positions with |dx| <= a and dy = 0 or small, and random d2 in [0, a * a]; also that the loop never hits its cap.
 *   make -C tests/slopedist && build/host/slopedist [millions of random cases]   exit status 1 on a failure */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pint.h"

#pragma STDC FP_CONTRACT OFF

static uint64_t s_ = 0x9e3779b97f4a7c15ull;
static uint64_t rnd(void) { s_ ^= s_ << 13; s_ ^= s_ >> 7; s_ ^= s_ << 17; return s_; }
static double u01(void) { return (rnd() >> 11) * (1.0 / 9007199254740992.0); }
static long n, bad, capped;

static void check(double d2, int a)
{
    double s = d2, prev = 0, dist;
    int it;
    n++;
    for (it = 0; it < 64 && s != prev && d2 > 0; it++) { prev = s; s = 0.5 * (s + d2 / s); }
    if (it == 64) capped++;
    dist = d2 > 0 ? s : 0;
    if (DGT(dist, a) && bad++ < 10) printf("a %d d2 %.17g dist %.17g\n", a, d2, dist);
}

int main(int argc, char **argv)
{
    long m = (argc > 1 ? atol(argv[1]) : 10) * 1000000L, k;
    int a;
    for (a = 0; a <= 64; a++) {
        double t = (double)a * a, d;
        uint64_t u;
        check(t, a);
        memcpy(&u, &t, 8);
        for (k = 1; k <= (1 << 20) && a > 0; k++) { uint64_t v = u - k; memcpy(&d, &v, 8); check(d, a); }
        for (k = 0; k < m / 65; k++) {
            float x0 = (float)((u01() - 0.5) * 8192), x1 = (float)(x0 + (u01() * 2 - 1) * a);
            float y0 = (float)((u01() - 0.5) * 8192), y1 = (rnd() & 1) ? y0 : (float)(y0 + (u01() - 0.5) * 0.01);
            double dx = (double)x1 - (double)x0, dy = (double)y1 - (double)y0, d2 = dx * dx + dy * dy;
            if (d2 <= t) check(d2, a);
            check(u01() * t, a);
        }
    }
    printf("slopedist: %ld cases, %ld with DGT true, %ld capped\n", n, bad, capped);
    return bad != 0 || capped != 0;
}
