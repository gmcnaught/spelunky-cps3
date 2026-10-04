/* tests/playsh2 host reference: runs one job (fresh process per job, as the SH-2 program clears its RAM before
   each) and prints "R <job> <kind> <idx> <sum> <n> <extra>" per record (tests/playsh2/check.py compares).
     build/host <job> | build/host --count | build/host --jthash <job> (the JT builds' hash of the job's records) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core.h"
#include "jobs.h"

void plat_begin(void) {}
void plat_end(void) {}
static int jthash;
static uint32_t jth = 0;
static uint32_t fnv(uint32_t h, uint32_t v)
{
    int k;
    for (k = 0; k < 4; k++) { h ^= (v >> (8 * k)) & 0xff; h *= 16777619u; }
    return h;
}
void plat_rec(int job, int kind, int idx, uint32_t sum, uint32_t n, uint32_t extra)
{
    jth = fnv(fnv(jth, sum), n);
    if (!jthash) printf("R %d %d %d %08x %u %u\n", job, kind, idx, sum, n, extra);
}

int main(int argc, char **argv)
{
    int j;
    if (argc > 1 && !strcmp(argv[1], "--count")) { printf("%d\n", NJOBS); return 0; }
    if (argc > 2 && !strcmp(argv[1], "--jthash")) jthash = 1, argv++, argc--;   /* main.c JT builds' job hash */
    j = argc > 1 ? atoi(argv[1]) : 0;
    if (jobs[j].route < 0) run_gen(j, jobs[j].seed, jobs[j].level);
    else run_route(j, jobs[j].seed, &routes[jobs[j].route], 30);
    if (jthash) printf("0x%08xu\n", jth);
    return 0;
}
