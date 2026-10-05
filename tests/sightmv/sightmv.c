/* penemy.c pen_motion's sight fast path: for every float x with 1 <= |x| < 2^20, (float)((double)x + h) == x + (float)h
 * for h = +-10, and (float)((double)x + v) == x for v in +-0, the largest doubles below 2^-47 (both signs) and the
 * sight's -10 * psin_cr(pi) (-1.2246467991473532e-15).   make -C tests/sightmv && build/host/sightmv */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#pragma STDC FP_CONTRACT OFF

int main(void)
{
    static const double vs[] = { 0.0, -0.0, 7.105427357601001e-15, -7.105427357601001e-15, -1.2246467991473532e-15 };
    long n = 0, bad = 0;
    uint32_t e, m, s;
    for (s = 0; s < 2; s++)
        for (e = 127; e < 147; e++)
            for (m = 0; m < (1u << 23); m++) {
                uint32_t u = (s << 31) | (e << 23) | m;
                volatile float x, a, b;
                size_t k;
                memcpy((void *)&x, &u, 4);
                a = (float)((double)x + 10.0); b = x + 10.0f; n++;
                if (memcmp((const void *)&a, (const void *)&b, 4) && bad++ < 5) printf("+10 %08x\n", u);
                a = (float)((double)x - 10.0); b = x - 10.0f; n++;
                if (memcmp((const void *)&a, (const void *)&b, 4) && bad++ < 5) printf("-10 %08x\n", u);
                for (k = 0; k < sizeof vs / sizeof vs[0]; k++) {
                    a = (float)((double)x + vs[k]); n++;
                    if (memcmp((const void *)&a, (const void *)&x, 4) && bad++ < 5) printf("v %g %08x\n", vs[k], u);
                }
            }
    printf("sightmv: %ld cases, %ld differ\n", n, bad);
    return bad != 0;
}
