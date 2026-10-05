/* src/game/prand.h's prand_scale (pscript.c prandom) against the expression it replaces, (double)u * (1.0 /
 * 4294967296.0) * n, bit for bit: every u (2^32) for the power-of-two n the game draws with and some around them
 * (1 .. 128, 0.5, 0.25); 10^8 random u for other n (3, 5, 6, 10, 100, 0.1, 1.5, 360, -4, 0, 1e300, 1e-300).
 *   make -C tests/prand && build/host/prand   exit status 1 on a difference */
#include <stdio.h>
#include <string.h>
#include "prand.h"

#pragma STDC FP_CONTRACT OFF

static long n, bad;
static void check(uint32_t u, double v)
{
    volatile double want = (double)u * (1.0 / 4294967296.0) * v;
    double got = prand_scale(u, v);
    uint64_t a, b;
    memcpy(&a, (const void *)&want, 8); memcpy(&b, &got, 8);
    n++;
    if (a != b && bad++ < 10) printf("u %08x n %g: %016llx want %016llx\n", u, v, (unsigned long long)b, (unsigned long long)a);
}

int main(void)
{
    static const double all[] = { 1, 2, 4, 8, 16, 32, 64, 128, 0.5, 0.25 };
    static const double some[] = { 3, 5, 6, 10, 100, 0.1, 1.5, 360, -4, 0, 1e300, 1e-300, 7, 12, 20 };
    uint64_t s = 0x9e3779b97f4a7c15ull;
    size_t j;
    long k;
    for (j = 0; j < sizeof all / sizeof all[0]; j++) {
        uint64_t u;
        for (u = 0; u <= 0xffffffffull; u++) check((uint32_t)u, all[j]);
    }
    for (j = 0; j < sizeof some / sizeof some[0]; j++)
        for (k = 0; k < 100000000L / 15; k++) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; check((uint32_t)(s >> 16), some[j]); }
    printf("prand: %ld cases, %ld differ\n", n, bad);
    return bad != 0;
}
