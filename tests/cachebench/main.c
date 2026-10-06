/* tests/cachebench: the jtcps3 costs behind the cache-bypass model (docs/ICACHE.md sections 2 and 6), measured with
   the SH7604 free-running timer (FRC, phi / 8: one tick 8 CPU clocks) with interrupts masked, each the median of 3.
   cps3-testgame's ttest gives uncached code and loads but not a cached line miss; these rows give both on the same
   board, plus the two cases the model predicts: hot code (or data) kept by running the streaming part past the cache.
   Each kernel (kernels.S) runs from SIMM 1's cached address and from its cache-through mirror (| 0x20000000); data is
   64 KB of main RAM (0x02..., mirror 0x22...) and 8 KB of SIMM 1 (k_lin's own code read as data).
   Results: the text layer (name and value; x100 values as ddd.dd) and 0x02000000 (cb_res: magic 'CBEN', passes,
   rows, values; scripts/lua/cachebench.lua dumps them in MAME). The derived rows at the end are the model's
   constants (tools/jtbypass.py C): IMISS fetch line miss, UWORD per 32-bit word fetched past the cache, DMISS RAM /
   SIMM data line miss over a hit, UNC RAM / SIMM uncached load over a hit, all in clocks x100. MAME has no cache and
   no wait states: its values only check that every row runs. */
#include <stdint.h>
#include "cps3v.h"

volatile uint32_t vbl_count;

typedef void (*kf)(uint32_t n, uint32_t a);
void k_lin(uint32_t, uint32_t), k_hot(uint32_t, uint32_t), k_sparse(uint32_t, uint32_t);
void k_ld16(uint32_t, uint32_t), k_ld4(uint32_t, uint32_t);
#define UNC(f) ((kf)((uint32_t)(f) | 0x20000000u))
#define UA(a)  ((uint32_t)(a) | 0x20000000u)

enum { LIN_C, LIN_U, SPARSE_C, SPARSE_U, HOT_C, HOTLIN_C, HOTLIN_U, LD16_C, LD16_HIT, LD16_U, LD4_C, LD4_U,
       SLD16_C, SLD16_U, HOTD_C, HOTD_U, D_IMISS, D_UWORD, D_DMISS_RAM, D_DMISS_SIMM, D_UNC_RAM, D_UNC_SIMM, NROW };
static const char *const names[NROW] = {
    "LIN C    /INS", "LIN U    /INS", "SPARSE C /LINE", "SPARSE U /LINE", "HOT C    /INS", "HOT+LIN C /IT",
    "HOT+LIN U /IT", "LD16 C   /LD", "LD16 HIT /LD", "LD16 U   /LD", "LD4 C    /LD", "LD4 U    /LD",
    "SIMM16 C /LD", "SIMM16 U /LD", "HOTD+ST C /IT", "HOTD+ST U /IT", "=IMISS", "=UWORD", "=DMISS RAM",
    "=DMISS SIMM", "=UNC RAM", "=UNC SIMM" };
/* x100 per unit, or whole clocks a loop iteration (the /IT rows) */
static const uint8_t x100[NROW] = { 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1 };

volatile struct {
    uint32_t magic, pass, n;
    int32_t val[NROW];
} cb_res __attribute__((section(".trace")));

static uint32_t buf[16384] __attribute__((aligned(16)));   /* 64 KB */

#define REG8(a) (*(volatile uint8_t *)(a))
static inline uint32_t frc(void)
{
    uint32_t h = REG8(0xfffffe12), l = REG8(0xfffffe13), h2 = REG8(0xfffffe12);
    if (h2 != h) {
        l = REG8(0xfffffe13);
        h = h2;
    }
    return h << 8 | l;
}
static void sr(uint32_t v) { __asm__ volatile("ldc %0, sr" : : "r"(v)); }

/* CPU clocks of reps calls of k(n, a) (under 65,536 ticks), after one warm-up call; median of 3 */
static uint32_t clocks(kf k, uint32_t n, uint32_t a, int reps)
{
    uint32_t d[3];
    k(n, a);
    for (int r = 0; r < 3; r++) {
        uint32_t t0 = frc();
        for (int i = 0; i < reps; i++)
            k(n, a);
        d[r] = ((frc() - t0) & 0xffff) * 8;
    }
    uint32_t lo = d[0] < d[1] ? d[0] : d[1], hi = d[0] < d[1] ? d[1] : d[0];
    return d[2] < lo ? lo : d[2] > hi ? hi : d[2];
}

/* the same for a pair: k1(n1, a1) then k2(n2, a2), reps times */
static uint32_t clocks2(kf k1, uint32_t n1, uint32_t a1, kf k2, uint32_t n2, uint32_t a2, int reps)
{
    uint32_t d[3];
    k1(n1, a1);
    k2(n2, a2);
    for (int r = 0; r < 3; r++) {
        uint32_t t0 = frc();
        for (int i = 0; i < reps; i++) {
            k1(n1, a1);
            k2(n2, a2);
        }
        d[r] = ((frc() - t0) & 0xffff) * 8;
    }
    uint32_t lo = d[0] < d[1] ? d[0] : d[1], hi = d[0] < d[1] ? d[1] : d[0];
    return d[2] < lo ? lo : d[2] > hi ? hi : d[2];
}

static void evict(void) { k_ld16(1024, (uint32_t)buf); }         /* 64 KB of loads: nothing else stays cached */

static void dec(char *b, int w, uint32_t v)
{
    for (int i = w - 1; i >= 0; i--) {
        b[i] = (char)('0' + v % 10);
        v /= 10;
        if (!v) {
            while (--i >= 0) b[i] = ' ';
            return;
        }
    }
}

static void show(int i, int32_t v)
{
    char s[25];
    int k;
    for (k = 0; k < 24; k++) s[k] = ' ';
    s[24] = 0;
    for (k = 0; k < 14 && names[i][k]; k++) s[k] = names[i][k];
    uint32_t m = (uint32_t)(v < 0 ? -v : v);
    char *f = s + 15;
    if (x100[i]) {
        dec(f, 5, m / 100);
        f[5] = '.';
        f[6] = (char)('0' + m / 10 % 10);
        f[7] = (char)('0' + m % 10);
    } else {
        dec(f, 8, m);
    }
    if (v < 0) {
        k = 7;
        while (k > 0 && f[k] != ' ') k--;
        f[k] = '-';
    }
    cps3v_text(1 + (i / 22) * 24, 2 + i % 22, s);
}

static void title(void)
{
    char s[] = "CACHEBENCH PASS 00000  CLOCKS X100, /IT X1";
    dec(s + 16, 5, cb_res.pass);
    cps3v_text(1, 0, s);
}

static void measure(int32_t *v)
{
    uint32_t sl = (uint32_t)k_lin;                         /* 8 KB of SIMM 1 as data */
    v[LIN_C] = (int32_t)(clocks(k_lin, 0, 0, 8) * 100u / (4096u * 8));
    v[LIN_U] = (int32_t)(clocks(UNC(k_lin), 0, 0, 8) * 100u / (4096u * 8));
    v[SPARSE_C] = (int32_t)(clocks(k_sparse, 0, 0, 8) * 100u / (512u * 8));
    v[SPARSE_U] = (int32_t)(clocks(UNC(k_sparse), 0, 0, 8) * 100u / (512u * 8));
    v[HOT_C] = (int32_t)(clocks(k_hot, 0, 0, 16) * 100u / (1024u * 16));
    v[HOTLIN_C] = (int32_t)(clocks2(k_hot, 0, 0, k_lin, 0, 0, 8) / 8);
    v[HOTLIN_U] = (int32_t)(clocks2(k_hot, 0, 0, UNC(k_lin), 0, 0, 8) / 8);
    v[LD16_C] = (int32_t)(clocks(k_ld16, 1024, (uint32_t)buf, 1) * 100u / 4096u);
    v[LD16_HIT] = (int32_t)(clocks(k_ld16, 32, (uint32_t)buf, 32) * 100u / (128u * 32));
    v[LD16_U] = (int32_t)(clocks(k_ld16, 1024, UA(buf), 1) * 100u / 4096u);
    v[LD4_C] = (int32_t)(clocks(k_ld4, 1024, (uint32_t)buf, 1) * 100u / 16384u);
    v[LD4_U] = (int32_t)(clocks(k_ld4, 1024, UA(buf), 1) * 100u / 16384u);
    evict();
    v[SLD16_C] = (int32_t)(clocks(k_ld16, 128, sl, 4) * 100u / (512u * 4));
    v[SLD16_U] = (int32_t)(clocks(k_ld16, 128, UA(sl), 4) * 100u / (512u * 4));
    /* 2 KB of hot data read in full, then 8 KB streamed one load a line: cached, or the stream past the cache */
    v[HOTD_C] = (int32_t)(clocks2(k_ld4, 32, (uint32_t)buf, k_ld16, 128, (uint32_t)(buf + 4096), 8) / 8);
    v[HOTD_U] = (int32_t)(clocks2(k_ld4, 32, (uint32_t)buf, k_ld16, 128, UA(buf + 4096), 8) / 8);
    /* the model's constants: a line of 8 instructions, a word of 2; loads over the hit's cost */
    v[D_IMISS] = (v[LIN_C] - v[HOT_C]) * 8;
    v[D_UWORD] = (v[LIN_U] - v[HOT_C]) * 2;
    v[D_DMISS_RAM] = v[LD16_C] - v[LD16_HIT];
    v[D_DMISS_SIMM] = v[SLD16_C] - v[LD16_HIT];
    v[D_UNC_RAM] = v[LD16_U] - v[LD16_HIT];
    v[D_UNC_SIMM] = v[SLD16_U] - v[LD16_HIT];
}

int main(void)
{
    int32_t v[NROW];
    cps3v_init();
    cps3v_text_init();
    cb_res.magic = 0x4342454eu;                            /* 'CBEN' */
    cb_res.pass = 0;
    cb_res.n = NROW;
    title();
    for (;;) {
        sr(0xf0);
        measure(v);
        for (int i = 0; i < NROW; i++) {
            cb_res.val[i] = v[i];
            show(i, v[i]);
        }
        cb_res.pass++;
        title();
        sr(0xa0);
        for (int f = 0; f < 60; f++)
            cps3v_wait_vblank();
    }
}
