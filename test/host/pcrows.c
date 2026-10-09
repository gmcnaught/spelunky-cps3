/* pworld.c pc_rows (precise_collision's unrotated loop with each row's floats once) against pc_loop (the loop as the
 * runner has it) on random instance pairs: fractional and whole positions, scales +-1 and others, masks of random
 * bits, boxes from the transform or perturbed, rows past PC_ROWS.  make -C test/host build/host/pcrows && build/host/pcrows [n] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/game/pworld.c"

static uint64_t rs = 88172645463325252ull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 16); }
static float frac(void) { return (float)(rnd() & 0xffffff) / 16777216.0f; }
static uint8_t mbuf[2][64 * 64];

static float pos_(void)
{
    switch (rnd() % 4) {
    case 0: return (float)(rnd() % 2000);
    case 1: return (float)(rnd() % 2000) + frac();
    case 2: return (float)(rnd() % 2000) + (float)(rnd() % 16) / 16.0f;
    default: return (float)(rnd() % 64) + frac();
    }
}

static float scale(void)
{
    static const float s[] = { 1, 1, 1, -1, -1, 2, -2, 0.5f, 1.5f, 3 };
    return s[rnd() % 10];
}

static void inst(struct pcinst *q, int k, float x, float y)
{
    int w = 1 + rnd() % 40, h = 1 + rnd() % 40, j;
    float l, r, t, b;
    q->x = x; q->y = y; q->xs = scale(); q->ys = scale(); q->ang = 0;
    q->ml = (float)(rnd() % 4); q->mt = (float)(rnd() % 4); q->mr = q->ml + (float)(w - 1); q->mb = q->mt + (float)(h - 1);
    q->xo = (float)(rnd() % 20); q->yo = (float)(rnd() % 20);
    q->bpr = (w + 7) >> 3;
    for (j = 0; j < q->bpr * h; j++) mbuf[k][j] = (uint8_t)(rnd() % 3 ? rnd() : 0);
    q->mask = rnd() % 5 ? mbuf[k] : 0;
    l = x + (q->ml - q->xo) * q->xs; r = x + (q->mr + 1 - q->xo) * q->xs;
    t = y + (q->mt - q->yo) * q->ys; b = y + (q->mb + 1 - q->yo) * q->ys;
    q->bl = l < r ? l : r; q->br = l < r ? r : l; q->bt = t < b ? t : b; q->bb = t < b ? b : t;
    if (rnd() % 4 == 0) { q->bl -= frac() * 3; q->br += frac() * 3; q->bt -= frac() * 3; q->bb += frac() * 3; }
}

int main(int argc, char **argv)
{
    long n = argc > 1 ? atol(argv[1]) : 2000000, i, hits = 0, big = 0;
    for (i = 0; i < n; i++) {
        struct pcinst A, B;
        float x = pos_(), y = pos_();
        float bl, bt, br, bb, x0, y0, x1, y1, ixA, ixB, iyA, iyB, jxA, jxB, jyA, jyB;
        int a, b;
        inst(&A, 0, x, y);
        inst(&B, 1, x + (float)((int)(rnd() % 48) - 24) + (rnd() & 1 ? frac() : 0),
             y + (float)((int)(rnd() % 48) - 24) + (rnd() & 1 ? frac() : 0));
        bl = A.bl > B.bl ? A.bl : B.bl; bt = A.bt > B.bt ? A.bt : B.bt;
        br = A.br < B.br ? A.br : B.br; bb = A.bb < B.bb ? A.bb : B.bb;
        x0 = (float)((int)(bl + 32768.0f) - 32768) + 0.5f;
        y0 = (float)((int)(bt + 32768.0f) - 32768) + 0.5f;
        x1 = (float)(32768 - (int)(32768.0f - br));
        y1 = (float)(32768 - (int)(32768.0f - bb));
        ixA = 1.0f / A.xs; ixB = 1.0f / B.xs; iyA = 1.0f / A.ys; iyB = 1.0f / B.ys;
        jxA = pc_inv(A.xs); jxB = pc_inv(B.xs); jyA = pc_inv(A.ys); jyB = pc_inv(B.ys);
        if (memcmp(&ixA, &jxA, 4) || memcmp(&ixB, &jxB, 4) || memcmp(&iyA, &jyA, 4) || memcmp(&iyB, &jyB, 4)) {
            printf("pc_inv differs at case %ld\n", i);
            return 1;
        }
        a = pc_rows(&A, &B, x0, y0, x1, y1, ixA, ixB, iyA, iyB);
        b = pc_loop(&A, &B, x0, y0, x1, y1, ixA, ixB, iyA, iyB);
        if (a != b) {
            printf("differs at case %ld: rows %d loop %d\n", i, a, b);
            return 1;
        }
        hits += a;
        big += y1 - y0 > PC_ROWS;
    }
    printf("PCROWS %ld cases equal (%ld hits, %ld past PC_ROWS)\n", n, hits, big);
    return 0;
}
