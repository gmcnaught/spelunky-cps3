/* GameMaker 2024.14 runtime RNG (WELL512a), bit-exact with the runner Spelunky Classic HD 1.2.2 uses.
 * Model and evidence: tools/gmrand.py (60,000 reference draws, 6 seeds), PLAN.md section 5. */
#ifndef RNG_H
#define RNG_H
#include <stdint.h>

struct rng { uint32_t s[16]; uint32_t i; };

extern struct rng g_rng;

/* random_set_seed(seed): x0 = seed; x(k+1) = ((x(k) * 0x343FD + 0x269EC3) mod 2^32) >> 16; s[k] = x(k+1) */
void rng_seed(struct rng *r, uint32_t seed);
/* one WELL512a step: the raw 32-bit word (random(n) = u * 2^-32 * n) */
uint32_t rng_next(struct rng *r);
/* scripts/rand.gml: floor(random(b - a + 1)) + a. For |b - a + 1| <= 2^21 the double product u * n is exact, so
 * this is floor(u * n / 2^32) + a in integers (n may be 0 or negative, as rand(1, k) with k <= 0 gives) */
int32_t rng_rand(struct rng *r, int32_t a, int32_t b);

#define RAND(a, b) rng_rand(&g_rng, (a), (b))
/* a draw whose value the GML throws away (`rand(1, 1);`, `if (rand(1, 2))` that is always true): it advances the RNG */
#define RAND_DISCARD(a, b) ((void)RAND((a), (b)))
/* rand(lo, hi) - rand(lo, hi): GML evaluates the left operand first; C leaves the order of a - b unspecified, so the
   two draws are separate statements here (do not fold them into one expression) */
static inline int32_t rand_diff(int32_t lo, int32_t hi)
{
    int32_t a = RAND(lo, hi);
    int32_t b = RAND(lo, hi);
    return a - b;
}

#endif
