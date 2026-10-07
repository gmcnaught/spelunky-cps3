/* src/game/pcmpc.h's cmpc_lt / cmpc_gt with each L / H constant against pnum.h's NLT(v, N(c)) / NGT(v, N(c)), and
 * the same through NABS (jar_step's |xVel| / |yVel| tests), bit for bit. Case sets per constant: every double within
 * 2^22 ulps of L(c), H(c) and c, both signs; the specials (+-0, +-inf, NaNs of both signs, the extreme normals and
 * subnormals); random bit patterns (half of them with exponents near c's).
 *   make -C tests/cmpc && build/host/cmpc [millions of random cases a constant]   exit status 1 on a difference */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pint.h"
#include "pcmpc.h"

#pragma STDC FP_CONTRACT OFF

static uint64_t s = 0x9e3779b97f4a7c15ull;
static uint64_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
static double u2d(uint64_t u) { double d; memcpy(&d, &u, 8); return d; }
static long n, bad;
struct cc { double c; uint64_t l, h; };     /* h or l 0: not used */
static const struct cc cs[] = {
    { 6, CMPC_L_6, 0 }, { 3, 0, CMPC_H_3 }, { 2, CMPC_L_2, 0 }, { 1, CMPC_L_1, CMPC_H_1 }, { -3, CMPC_L_M3, 0 },
    { 0.1, CMPC_L_0_1, 0 }, { 90, 0, CMPC_H_90 }, { 270, CMPC_L_270, 0 },
};

static void check(const struct cc *c, uint64_t u)
{
    double v = u2d(u), a = NABS(v);
    n++;
    if (c->l && (NLT(v, c->c) != cmpc_lt(v, c->l) || NLT(a, c->c) != cmpc_lt(a, c->l)) && bad++ < 10)
        printf("lt %g: %016llx\n", c->c, (unsigned long long)u);
    if (c->h && (NGT(v, c->c) != cmpc_gt(v, c->h) || NGT(a, c->c) != cmpc_gt(a, c->h)) && bad++ < 10)
        printf("gt %g: %016llx\n", c->c, (unsigned long long)u);
}

static void around(const struct cc *c, uint64_t m, uint64_t r)
{
    uint64_t k;
    for (k = 0; k <= 2 * r; k++) { check(c, m - r + k); check(c, (m - r + k) ^ 0x8000000000000000ull); }
}

int main(int argc, char **argv)
{
    long m = (argc > 1 ? atol(argv[1]) : 100) * 1000000L, k;
    static const uint64_t sp[] = { 0, 0x7ff0000000000000ull, 0x7ff8000000000000ull, 0x7ff0000000000001ull,
                                   0x7fffffffffffffffull, 1, 0x000fffffffffffffull, 0x0010000000000000ull,
                                   0x7fefffffffffffffull };
    size_t j, q;
    for (j = 0; j < sizeof cs / sizeof cs[0]; j++) {
        const struct cc *c = &cs[j];
        uint64_t cb; memcpy(&cb, &c->c, 8);
        for (q = 0; q < sizeof sp / sizeof sp[0]; q++) { check(c, sp[q]); check(c, sp[q] | 0x8000000000000000ull); }
        if (c->l) around(c, c->l, 1u << 22);
        if (c->h) around(c, c->h, 1u << 22);
        around(c, cb & 0x7fffffffffffffffull, 1u << 22);
        for (k = 0; k < m; k++) {
            uint64_t u = rnd();
            if (k & 1) u = (u & 0x800fffffffffffffull) | (((cb >> 52) & 0x7ff) - 1 + rnd() % 3) << 52;
            check(c, u);
        }
    }
    printf("cmpc: %ld cases, %ld differ\n", n, bad);
    return bad != 0;
}
