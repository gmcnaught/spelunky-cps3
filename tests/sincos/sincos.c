/* pmath.c psin_cr, pcos_cr and psincos_cr against cr_trig_dd (the double-double series of the reduced argument,
 * rounded once): for every float dir in [+0, 360] and -0 (1,135,869,954 of them) at degtorad_d(dir), or (rand mode)
 * for random doubles: directions in degrees in [-720, 1080] through degtorad_d (the piranhas' and move_dir's
 * point_direction + a - b) and radians in [-20, 20], half each. Also counts, where sincos_r's first test fails,
 * sincos_r2's answers at its shipping e and at smaller ones (an accepted answer that differs from dd_sincos's at e
 * means stage 2's error reached e there).
 *   make -C tests/sincos && build/host/sincos [nproc [end bits, hex: a partial run]]
 *   build/host/sincos nproc rand <count> [seed]      (host, -ffp-contract=off: IEEE binary64 as the SH-2) */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static double sc2_eps_s = 0x1p-86, sc2_eps_c = 0x1p-86;
#define SC2_EPS_S sc2_eps_s
#define SC2_EPS_C sc2_eps_c
#include "../../src/game/pmath.c"

#pragma STDC FP_CONTRACT OFF

double patan(double x) { return x; }                /* pscript.c's (patan2 only; unused here) */

#define NE 4
static const double EPS[NE] = { 0x1p-86, 0x1p-92, 0x1p-98, 0x1p-104 };

struct res { long n, bad, fail1[2], acc[NE][2], accbad[NE][2]; };

static ddbl reduce(double a, int *q)                /* psincos_cr's reduction */
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

static int stage1_ok(ddbl r, int odd)               /* sincos_r's first test, as written there */
{
    static const double I6H = 0.16666666666666666, I6L = 9.25185853854297e-18,
        s5 = 0.008333333333333333, s7 = -0.0001984126984126984, s9 = 2.7557319223985893e-06,
        s11 = -2.505210838544172e-08, s13 = 1.6059043836821613e-10, s15 = -7.647163731819816e-13,
        s17 = 2.8114572543455206e-15, s19 = -8.22063524662433e-18, s21 = 1.9572941063391263e-20,
        c4 = 0.041666666666666664, c6 = -0.001388888888888889, c8 = 2.48015873015873e-05,
        c10 = -2.755731922398589e-07, c12 = 2.08767569878681e-09, c14 = -1.1470745597729725e-11,
        c16 = 4.779477332387385e-14, c18 = -1.5619206968586225e-16, c20 = 4.110317623312165e-19,
        c22 = -8.896791392450574e-22;
    double x = r.hi, y = r.lo, zh, zl, hi, lo, e;
    two_prod(x, x, &zh, &zl);
    if (odd) {
        double vh, vl, th, tl, rest;
        ddbl u;
        two_prod(x, zh, &vh, &vl);
        vl += x * zl;
        two_prod(vh, I6H, &th, &tl);
        tl += vh * I6L + vl * I6H;
        rest = vh * zh * (s5 + zh * (s7 + zh * (s9 + zh * (s11 + zh * (s13 + zh * (s15 + zh * (s17 +
               zh * (s19 + zh * s21)))))))) + (vl * zh + vh * zl) * s5;
        u = two_sum(x, -th);
        hi = u.hi;
        lo = u.lo + (((rest - tl) + y * (1.0 - zh * (0.5 - zh * c4))));
        e = (x < 0 ? -x : x) * SC_EPS_S;
    } else {
        double hz = 0.5 * zh, w = 1.0 - hz, rest;
        rest = zh * (zh * (c4 + zh * (c6 + zh * (c8 + zh * (c10 + zh * (c12 + zh * (c14 + zh * (c16 + zh * (c18 +
               zh * (c20 + zh * c22))))))))) + 2.0 * zl * c4);
        hi = w;
        lo = ((((1.0 - w) - hz) - 0.5 * zl) + rest) - x * y * (1.0 - zh * I6H);
        e = SC_EPS_C;
    }
    return hi + (lo + e) == hi + (lo - e);
}

static void one(double a, struct res *R)
{
    double s, c, s0, c0, s1, c1;
    ddbl r;
    int q, odd, k;
    psincos_cr(a, &s, &c);
    s1 = psin_cr(a);
    c1 = pcos_cr(a);
    s0 = cr_trig_dd(a, 1);
    c0 = cr_trig_dd(a, 0);
    R->n++;
    if (memcmp(&s, &s0, 8) || memcmp(&c, &c0, 8) || memcmp(&s1, &s0, 8) || memcmp(&c1, &c0, 8)) {
        if (R->bad++ < 5) printf("differ: a %.17g (bits %016llx)\n", a, (unsigned long long)*(uint64_t *)&a);
    }
    {
        r = reduce(a, &q);
        for (odd = 0; odd < 2; odd++) {
            ddbl t;
            double ref;
            if (stage1_ok(r, odd)) continue;
            R->fail1[odd]++;
            t = dd_sincos(r, odd);
            ref = t.hi + t.lo;
            for (k = 0; k < NE; k++) {
                double v;
                sc2_eps_s = sc2_eps_c = EPS[k];
                if (!sincos_r2(r, odd, &v)) continue;
                R->acc[k][odd]++;
                if (memcmp(&v, &ref, 8)) R->accbad[k][odd]++;
            }
            sc2_eps_s = sc2_eps_c = EPS[0];
        }
    }
}

static void run(uint32_t u0, uint32_t u1, struct res *R)
{
    uint32_t u;
    memset(R, 0, sizeof *R);
    for (u = u0; u < u1; u++) {
        float f;
        memcpy(&f, &u, 4);
        one(degtorad_d((double)f), R);
    }
}

static void run_rand(uint64_t seed, long n, struct res *R)
{
    uint64_t x = seed * 0x9e3779b97f4a7c15ull + 1;
    long j;
    memset(R, 0, sizeof *R);
    for (j = 0; j < n; j++) {
        double v;
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        v = (double)(x >> 11) * 0x1p-53;                       /* [0, 1) with 53 random bits */
        if (j & 1) one(v * 40.0 - 20.0, R);
        else one(degtorad_d(v * 1800.0 - 720.0), R);
    }
}

int main(int argc, char **argv)
{
    int np = argc > 1 ? atoi(argv[1]) : 8, i, j, k, fd[64][2];
    int rnd = argc > 3 && !strcmp(argv[2], "rand");
    long rn = rnd ? atol(argv[3]) : 0;
    uint64_t rseed = rnd && argc > 4 ? strtoull(argv[4], 0, 10) : 1;
    const uint32_t hi = argc > 2 && !rnd ? (uint32_t)strtoul(argv[2], 0, 16) : 0x43b40001u;   /* (float)360 = 0x43b40000, inclusive */
    struct res T, R;
    memset(&T, 0, sizeof T);
    if (np < 1 || np > 64) np = 8;
    for (i = 0; i < np; i++) {
        if (pipe(fd[i])) return 2;
        if (fork() == 0) {
            uint32_t a = (uint32_t)((uint64_t)hi * i / np), b = (uint32_t)((uint64_t)hi * (i + 1) / np);
            if (rnd) run_rand(rseed * 1000 + (uint64_t)i, rn / np, &R);
            else run(a, b, &R);
            if (i == 0 && !rnd) {                   /* -0 */
                struct res M;
                run(0x80000000u, 0x80000001u, &M);
                R.n += M.n; R.bad += M.bad;
            }
            if (write(fd[i][1], &R, sizeof R) != (ssize_t)sizeof R) _exit(2);
            _exit(0);
        }
        close(fd[i][1]);
    }
    for (i = 0; i < np; i++) {
        if (read(fd[i][0], &R, sizeof R) != (ssize_t)sizeof R) return 2;
        T.n += R.n; T.bad += R.bad;
        for (j = 0; j < 2; j++) {
            T.fail1[j] += R.fail1[j];
            for (k = 0; k < NE; k++) { T.acc[k][j] += R.acc[k][j]; T.accbad[k][j] += R.accbad[k][j]; }
        }
    }
    while (wait(0) > 0) ;
    printf("sincos: %ld arguments, %ld differ from cr_trig_dd\n", T.n, T.bad);
    for (j = 1; j >= 0; j--) {
        printf("  %s: first test fails %ld;", j ? "sin" : "cos", T.fail1[j]);
        for (k = 0; k < NE; k++) printf(" e 2^%d: kept %ld, of them differ %ld;", k ? -86 - 6 * k : -86, T.acc[k][j], T.accbad[k][j]);
        printf("\n");
    }
    return T.bad != 0 || T.accbad[0][0] || T.accbad[0][1];
}
