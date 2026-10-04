/* tests/playsh2 BENCH builds: clocks of libgcc's soft-float operations on the SH-2 (fp-bit), 1,000 calls each on
   values like the game's (positions, velocities, scales); a record per operation (kind 5, idx = operation,
   extra = calls, clocks = all of them, the loop's own cost included: op "loop" alone is idx 0). */
#include <stdint.h>
#include "core.h"

#define N 1000
static volatile double va[8] = { 216.0, 1.0, -3.4500000000000002, 0.15, 497.0, 16.0, 0.3, -1.0 };
static volatile float vf[4] = { 216.0f, 352.0f, 0.4f, 1.0f };
static volatile double sink;
static volatile int isink;

void run_bench(int job)
{
    int k, op;
    for (op = 0; op < 12; op++) {
        double a = va[op & 7], b = va[(op + 3) & 7];
        float f = vf[op & 3];
        plat_begin();
        for (k = 0; k < N; k++) {
            switch (op) {
            case 0: break;
            case 1: sink = a + b; break;
            case 2: sink = a - b; break;
            case 3: sink = a * b; break;
            case 4: sink = a / b; break;
            case 5: isink = a < b; break;
            case 6: isink = a >= b; break;
            case 7: isink = a == b; break;
            case 8: sink = (double)f; break;
            case 9: sink = (double)k; break;
            case 10: isink = (int)a; break;
            case 11: vf[0] = (float)a; break;
            }
            a = va[k & 7];
        }
        plat_end();
        plat_rec(job, 5, op, 0, N, 0);
    }
}
