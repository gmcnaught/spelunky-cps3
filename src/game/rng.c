#include "rng.h"

struct rng g_rng;

void rng_seed(struct rng *r, uint32_t seed)
{
    uint32_t x = seed;
    int k;
    for (k = 0; k < 16; k++) {
        x = (x * 0x343FDu + 0x269EC3u) >> 16;
        r->s[k] = x;
    }
    r->i = 0;
}

uint32_t rng_next(struct rng *r)
{
    uint32_t *S = r->s, i = r->i, a, b, c, d;
    a = S[i];
    c = S[(i + 13) & 15];
    b = a ^ c ^ (a << 16) ^ (c << 15);
    c = S[(i + 9) & 15];
    c ^= c >> 11;
    a = S[i] = b ^ c;
    d = a ^ ((a << 5) & 0xDA442D24u);
    i = (i + 15) & 15;
    a = S[i];
    S[i] = a ^ b ^ d ^ (a << 2) ^ (b << 18) ^ (c << 28);
    r->i = i;
    return S[i];
}

int32_t rng_rand(struct rng *r, int32_t a, int32_t b)
{
    uint32_t u = rng_next(r);
    int32_t n = b - a + 1;
    if (n >= 0)
        return (int32_t)(((uint64_t)u * (uint32_t)n) >> 32) + a;
    /* negative range: floor of a negative product (a 32x32 -> 64 multiply and a floor shift) */
    {
        uint64_t p = (uint64_t)u * (uint32_t)(-n);        /* |u * n| */
        int32_t q = (int32_t)(p >> 32);
        if ((uint32_t)p != 0) q += 1;                      /* floor(-p / 2^32) = -ceil(p / 2^32) */
        return -q + a;
    }
}
