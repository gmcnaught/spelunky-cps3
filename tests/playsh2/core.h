/* tests/playsh2: shared core (core.c) and the platform hooks (main.c on the SH-2, host.c on the host) */
#ifndef PLAYSH2_CORE_H
#define PLAYSH2_CORE_H
#include <stdint.h>

enum { KIND_GEN = 1, KIND_START = 2, KIND_STEP = 3, KIND_EARLY = 4, KIND_BENCH = 5 };

/* the jobs (build/jobs.h, mkjobs.py): a generation case (level > 0, route < 0) or a route replay */
struct job { uint32_t seed; int16_t level; int16_t route; };
struct route { const char *name; const uint16_t *masks; int n; int level, money, enemies; };

void plat_begin(void);
void plat_end(void);
void plat_rec(int job, int kind, int idx, uint32_t sum, uint32_t n, uint32_t extra);

uint32_t sum_gen(void);
uint32_t sum_play(void);
void run_gen(int job, uint32_t seed, int level);
void run_bench(int job);                         /* bench.c (BENCH builds) */
void run_route(int job, uint32_t seed, const struct route *r, int tail);
#endif
