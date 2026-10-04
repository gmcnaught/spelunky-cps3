/* moveTo's velocity parts without soft-float (PERF item 6): for a binary64 v, floor(|v|), N = round(1 / frac(|v|))
 * (GML round: half to even; 0 when frac(|v|) <= GML_EPS, as NNE(frac, 0) decides) and v < 0, from v's bits with
 * integer operations. N is a step function of f = frac(|v|): N <= k exactly when f >= T[k], the thresholds of
 * preciptab.h (tools/mkrecip.c finds each by bisection on the bit patterns against dround(1.0 / f) and checks the
 * table: tools/mkrecip.c check). f as Q0.64 (frac * 2^64, exact while |v| >= 2^-12: its lowest bit is 2^-64 or
 * above) is compared with the thresholds' Q0.64 (exact: T[k] >= 2^-8). precip_parts returns 0 (the caller computes
 * it in binary64) when |v| >= 2^20, |v| < 2^-12, v is not finite, or 0 < f < T[PRECIP_K] */
#ifndef PRECIP_H
#define PRECIP_H
#include <stdint.h>
#include "preciptab.h"

/* 2^(p + 12) for p = -12 .. 19 */
static const uint32_t precip_mul[32] = {
    1u << 0, 1u << 1, 1u << 2, 1u << 3, 1u << 4, 1u << 5, 1u << 6, 1u << 7, 1u << 8, 1u << 9, 1u << 10, 1u << 11,
    1u << 12, 1u << 13, 1u << 14, 1u << 15, 1u << 16, 1u << 17, 1u << 18, 1u << 19, 1u << 20, 1u << 21, 1u << 22,
    1u << 23, 1u << 24, 1u << 25, 1u << 26, 1u << 27, 1u << 28, 1u << 29, 1u << 30, 1u << 31 };

static inline int precip_parts(double v, int32_t *n, int32_t *fl, int *neg)
{
    union { double d; uint64_t u; } c;
    uint32_t hi, lo, e, mh, m, fh, fo;
    int32_t p;
    uint64_t p1, p2;
    c.d = v;
    hi = (uint32_t)(c.u >> 32);
    lo = (uint32_t)c.u;
    if ((hi & 0x7fffffffu) == 0 && lo == 0) {         /* +-0: frac 0, floor 0, not < 0 */
        *n = 0; *fl = 0; *neg = 0;
        return 1;
    }
    e = (hi >> 20) & 0x7ffu;
    p = (int32_t)e - 1023;
    if (p < -12 || p > 19) return 0;
    mh = (hi & 0xfffffu) | 0x100000u;                 /* |v| = (mh * 2^32 + lo) * 2^(p - 52) */
    m = precip_mul[p + 12];
    p1 = (uint64_t)mh * m;
    p2 = (uint64_t)lo * m;
    *fl = (int32_t)(uint32_t)(p1 >> 32);              /* floor(|v|) (0 when p < 0) */
    fh = (uint32_t)p1 + (uint32_t)(p2 >> 32);         /* frac(|v|) * 2^64: high, low words */
    fo = (uint32_t)p2;
    *neg = (hi & 0x80000000u) != 0;
    if (fh == 0 && fo == 0) { *n = 0; return 1; }
    {   /* the smallest k with f >= T[k] (thresholds descending) */
        int a = 0, b = PRECIP_K - 1;
        if (fh < precip_th[b] || (fh == precip_th[b] && fo < precip_tl[b])) return 0;
        while (a < b) {
            int mid = (a + b) >> 1;
            if (fh > precip_th[mid] || (fh == precip_th[mid] && fo >= precip_tl[mid])) b = mid;
            else a = mid + 1;
        }
        *n = a + 1;
    }
    return 1;
}
#endif
