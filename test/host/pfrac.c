/* pworld.c pfrac_ok's identities, for every float x it accepts (+-0, 1/2 <= |x| < 2^13) and the offsets d below
 * (every int in [-100, 100] for one x in 64; the listed ones for all): (float)((double)x + d) is x + (float)d (bits),
 * floor((double)x + d) is pfloor_int(x) + d, floor((double)x + d + 0.5) is pfr's rounding + d, and pq_init's ints of
 * the sum are pfloor_int of the float sum.  make -C test/host build/host/pfrac && build/host/pfrac */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../../src/game/pworld.c"

static const int dl[] = { -100, -66, -32, -17, -16, -9, -8, -6, -4, -3, -2, -1, 0, 1, 2, 3, 4, 6, 7, 8, 9, 14, 16, 17, 18,
                          32, 48, 62, 64, 66, 100 };

static int one(float x, int d)
{
    double s = (double)x + d;
    float f = x + PLACE_F(d), g = (float)s;
    int32_t a, b, v, c;
    uint32_t fu, gu;
    memcpy(&fu, &f, 4); memcpy(&gu, &g, 4);
    if (fu != gu) return 1;
    if (!pfloor_int(x, &a) || (double)(a + d) != floor(s)) return 2;
    if (!pfr(x, &v) || (double)((v >> 2) + ((v & 3) >= 2) + d) != floor(s + 0.5)) return 3;
    if (!pfloor_int(f, &b) || (double)b != floor((double)g)) return 4;
    (void)c;
    return 0;
}

int main(void)
{
    uint32_t u;
    unsigned long n = 0, k;
    for (u = 0; u < 0x80000000u; u++) {
        uint32_t e = (u >> 23) & 0xffu;
        int s;
        if (!((u << 1) == 0 || (e >= 126 && e <= 139))) continue;
        for (s = 0; s < 2; s++) {
            union { uint32_t u; float f; } x;
            int r, d;
            x.u = u | (s ? 0x80000000u : 0);
            if (!pfrac_ok(x.f)) { printf("pfrac_ok rejects %08x\n", x.u); return 1; }
            for (k = 0; k < sizeof dl / sizeof dl[0]; k++)
                if ((r = one(x.f, dl[k])) != 0) { printf("x %08x d %d: identity %d fails\n", x.u, dl[k], r); return 1; }
            if ((u & 63) == 0)
                for (d = -100; d <= 100; d++)
                    if ((r = one(x.f, d)) != 0) { printf("x %08x d %d: identity %d fails\n", x.u, d, r); return 1; }
            n++;
        }
    }
    printf("PFRAC %lu floats equal\n", n);
    return 0;
}
