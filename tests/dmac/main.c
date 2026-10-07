/* tests/dmac: the SH-2 DMAC on the CPS3 (docs/DRAW.md section 8). Channel 0, dual address, auto request, source and
   destination incrementing, 4 KB (TCR 1024: TCR counts longwords in every unit size), from main RAM to main RAM (MR)
   or to sprite RAM (SP), in four modes: 16-byte units cycle steal (CHCR 0x5e01) / burst (0x5e11), longwords cycle
   steal (0x5a01) / burst (0x5a11). Times in CPU clocks from the FRT (phi / 8: 8-clock resolution), interrupts masked.
   Rows:
     RB  readback: words that differ after the transfer (destination read uncached), and the word after the end
         (must stay 55AA55AA), per mode, main -> sprite
     T   transfer clocks from the CHCR write to TE, per mode, MR and SP
     ST  4 KB of CPU 32-bit stores (1,024, unrolled by 8): main RAM, sprite RAM
     OV  a cached register loop (DT / BF, 4 clocks an iteration, 12,000 iterations) alone, and with a 4 KB main ->
         sprite transfer started just before it, per mode: the loop's clocks, the loop's extra clocks (CPU clocks
         lost), and the transfer's end (TE seen after the loop: 0 = it ended during the loop)
   The last pass's values also at dm_res (main RAM, the .bss symbol). */
#include "cps3v.h"

#define REG8(a)   (*(volatile uint8_t *)(a))
#define REG32(a)  (*(volatile uint32_t *)(a))
#define SAR0      0xffffff80u
#define DAR0      0xffffff84u
#define TCR0      0xffffff88u
#define CHCR0     0xffffff8cu
#define DMAOR     0xffffffb0u
#define SPR_DST   0x04050000u                 /* sprite RAM: nothing displayed uses it here */
#define NB        4096u

static uint32_t src[NB / 4] __attribute__((aligned(16)));
static uint32_t dst[NB / 4 + 4] __attribute__((aligned(16)));
static const uint32_t modes[4] = { 0x5e01, 0x5e11, 0x5a01, 0x5a11 };
static const char *const mname[4] = { "16 STEAL", "16 BURST", "LW STEAL", "LW BURST" };
volatile uint32_t dm_res[64];

static void sr(uint32_t v) { __asm__ volatile("ldc %0, sr" : : "r"(v)); }
static inline uint32_t frc(void)
{
    uint32_t h = REG8(0xfffffe12), l = REG8(0xfffffe13), h2 = REG8(0xfffffe12);
    if (h2 != h) {
        l = REG8(0xfffffe13);
        h = h2;
    }
    return h << 8 | l;
}
#define CLK(t0) ((((frc() - (t0)) & 0xffff)) * 8)

static void dmac_go(uint32_t s, uint32_t d, uint32_t chcr)
{
    (void)REG32(CHCR0);
    REG32(CHCR0) = 0;
    REG32(SAR0) = s;
    REG32(DAR0) = d;
    REG32(TCR0) = NB / 4;
    REG32(CHCR0) = chcr;
}
static void dmac_end(void)
{
    while (!(REG32(CHCR0) & 2)) ;
    REG32(CHCR0) = 0;
}
static uint32_t xfer(uint32_t d, uint32_t chcr)
{
    uint32_t t0 = frc();
    dmac_go((uint32_t)src, d, chcr);
    while (!(REG32(CHCR0) & 2)) ;
    uint32_t t = CLK(t0);
    REG32(CHCR0) = 0;
    return t;
}
static void spin(uint32_t n) { __asm__ volatile("1: dt %0\n\tbf 1b" : "+r"(n)); }
static uint32_t stores(volatile uint32_t *p)
{
    uint32_t t0 = frc(), v = 0x12345678u;
    for (int k = 0; k < (int)(NB / 4); k += 8) {
        p[k] = v; p[k + 1] = v; p[k + 2] = v; p[k + 3] = v; p[k + 4] = v; p[k + 5] = v; p[k + 6] = v; p[k + 7] = v;
    }
    return CLK(t0);
}

static void hex(char *b, uint32_t v) { for (int i = 7; i >= 0; i--, v >>= 4) b[i] = "0123456789ABCDEF"[v & 15]; }
static void dec(char *b, int w, uint32_t v)
{
    for (int i = w - 1; i >= 0; i--) {
        b[i] = (char)('0' + v % 10);
        v /= 10;
        if (!v) { while (--i >= 0) b[i] = ' '; break; }
    }
}
static void row(int r, const char *name, uint32_t a, uint32_t b, uint32_t c, int c_hex)
{
    char l[40];
    int k;
    for (k = 0; k < 39; k++) l[k] = ' ';
    l[39] = 0;
    for (k = 0; name[k] && k < 12; k++) l[k] = name[k];
    dec(l + 12, 8, a);
    dec(l + 21, 8, b);
    if (c_hex) hex(l + 30, c); else dec(l + 30, 8, c);
    cps3v_text(1, r, l);
}

int main(void)
{
    cps3v_init();
    cps3v_text_init();
    cps3v_begin();
    cps3v_end();
    cps3v_text(1, 1, "SH-2 DMAC TEST 4 KB  CPU CLOCKS");
    (void)REG32(DMAOR);
    REG32(DMAOR) = 0;
    REG32(DMAOR) = 1;
    for (uint32_t pass = 0;; pass++) {
        int r = 3, n = 0, m;
        sr(0xf0);
        /* readback, main -> sprite */
        for (m = 0; m < 4; m++) {
            volatile uint32_t *sp = (volatile uint32_t *)SPR_DST;
            uint32_t k, bad = 0;
            for (k = 0; k < NB / 4; k++) src[k] = (k + 1) * 0x9e3779b9u + pass + (uint32_t)m;
            for (k = 0; k < NB / 4; k++) sp[k] = 0;
            sp[NB / 4] = 0x55aa55aau;
            xfer(SPR_DST, modes[m]);
            for (k = 0; k < NB / 4; k++) bad += sp[k] != src[k];
            dm_res[n++] = bad;
            dm_res[n++] = sp[NB / 4];
            row(r++, "RB", m, bad, sp[NB / 4], 1);
        }
        cps3v_text(1, r++, "T  MODE       MAIN    SPRITE");
        for (m = 0; m < 4; m++) {
            uint32_t a = xfer((uint32_t)dst, modes[m]), b = xfer(SPR_DST, modes[m]);
            dm_res[n++] = a;
            dm_res[n++] = b;
            row(r++, mname[m], a, b, 0, 0);
        }
        {
            uint32_t a = stores((volatile uint32_t *)dst), b = stores((volatile uint32_t *)SPR_DST);
            dm_res[n++] = a;
            dm_res[n++] = b;
            row(r++, "ST CPU", a, b, 0, 0);
        }
        cps3v_text(1, r++, "OV MODE       LOOP      LOST  TE AFTER");
        {
            uint32_t t0, alone;
            spin(16);
            t0 = frc();
            spin(12000);
            alone = CLK(t0);
            dm_res[n++] = alone;
            row(r++, "LOOP ALONE", alone, 0, 0, 0);
            for (m = 0; m < 4; m++) {
                uint32_t loop, after;
                spin(16);
                t0 = frc();
                dmac_go((uint32_t)src, SPR_DST, modes[m]);
                spin(12000);
                loop = CLK(t0);
                t0 = frc();
                dmac_end();
                after = CLK(t0);
                dm_res[n++] = loop;
                dm_res[n++] = after;
                row(r++, mname[m], loop, loop - alone, after, 0);
            }
        }
        {
            char l[16] = "PASS          ";
            dec(l + 5, 8, pass + 1);
            cps3v_text(1, r + 1, l);
        }
        sr(0xa0);
        for (int f = 0; f < 30; f++) cps3v_wait_vblank();
    }
}
