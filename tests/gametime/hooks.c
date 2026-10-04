/* tests/gametime: the marker block, the FRT clock, the route's inputs, the draw timing hooks and the results screen
   (main.c times the frame loop) */
#include "cps3.h"
#include "shell.h"
#include "draw.h"
#include "game.h"
#include "play.h"
#include "route.h"
#include "gametime.h"

volatile struct marker M __attribute__((section(".trace"), used));

/* FRC ticks since boot, 32 bits: the wrap count kept by the output-compare interrupt (frt.S, every 4,096 ticks) and
   the FRC at that interrupt; an FRC below that value has wrapped once since */
volatile uint32_t frt_exth, frt_lastf;
static uint32_t frc(void)
{
    volatile uint8_t *f = (volatile uint8_t *)0xfffffe12u;
    uint32_t h = f[0], l = f[1], h2 = f[0];
    if (h2 != h) { l = f[1]; h = h2; }
    return h << 8 | l;
}
uint32_t now(void)
{
    uint32_t h, lf, f;
    do {
        h = frt_exth;
        lf = frt_lastf;
        f = frc();
    } while (h != frt_exth || lf != frt_lastf);
    return f >= lf ? (h << 16 | f) : ((h + 1) << 16 | f);
}

/* the vector table moved to RAM (VBR) with the FRT output compare (vector 72) added: the SDK's table (BIOS ROM at 0)
   has no FRT entry */
void frt_isr(void);
static void (*vtab[128])(void) __attribute__((aligned(1024)));
static void frt_start(void)
{
    int k;
    uint32_t t, sr;
    for (k = 0; k < 128; k++) vtab[k] = ((void (**)(void))0)[k];
    vtab[72] = frt_isr;
    __asm__ volatile("stc sr, %0" : "=r"(sr));
    __asm__ volatile("ldc %0, sr" : : "r"(sr | 0xf0));  /* no interrupts while VBR moves */
    __asm__ volatile("ldc %0, vbr" : : "r"(vtab));
    *(volatile uint16_t *)0xfffffe66 = 72;          /* VCRC: FRT output compare vector */
    *(volatile uint16_t *)0xfffffe60 = 0x0f00;      /* IPRB: FRT priority 15 */
    *(volatile uint8_t *)0xfffffe16 = 0x01;         /* TCR: phi / 32 */
    t = frc();
    frt_exth = 0;
    frt_lastf = t;
    t += 4096;
    *(volatile uint8_t *)0xfffffe14 = (uint8_t)(t >> 8);
    *(volatile uint8_t *)0xfffffe15 = (uint8_t)t;
    *(volatile uint8_t *)0xfffffe10 = 0x09;         /* TIER: OCIAE */
    __asm__ volatile("ldc %0, sr" : : "r"(sr));
}

extern char __sprbss_a_start[] __asm__("__sprbss_a_start"), __sprbss_a_end[] __asm__("__sprbss_a_end");
extern char __sprbss_b_start[] __asm__("__sprbss_b_start"), __sprbss_b_end[] __asm__("__sprbss_b_end");
void main_boot(void)
{
    uint32_t *d;
    int k;
    for (d = (uint32_t *)__sprbss_a_start; d < (uint32_t *)__sprbss_a_end; d++) *d = 0;
    for (d = (uint32_t *)__sprbss_b_start; d < (uint32_t *)__sprbss_b_end; d++) *d = 0;
    frt_start();
    for (d = (uint32_t *)&M, k = 0; k < (int)(sizeof M / 4); k++) d[k] = 0;
    M.magic = 0x47544d31;                         /* 'GTM1' */
    game_cfg.route = route_keys;
    game_cfg.nroute = ROUTE_N;
    game_cfg.tail = ROUTE_TAIL;
    game_cfg.seed = ROUTE_SEED;
    game_cfg.level = ROUTE_LEVEL;
    game_cfg.money = ROUTE_MONEY;
    game_cfg.enemies = ROUTE_ENEMIES;
}

static uint32_t frame;
void main_inputs(uint32_t *pad0, uint32_t *pad1, uint32_t *lines)
{
    frame++;
    *pad0 = frame >= 40 && frame < 44 ? CPS3_START : 0;
    *pad1 = 0;
    *lines = frame >= 20 && frame < 24 ? CR_COIN1 : 0;
}

static uint32_t td0;
uint32_t tdraw;
void main_draw_begin(void) { td0 = now(); }
void main_draw_end(void) { tdraw = (now() - td0) * 32; }


/* ---- the results on screen (large text: ../playsh2/bigtext.c) ----------------------------------------------- */
void big_init(void);
void big_text(int row, const char *s);
static char *put_u(char *p, uint32_t v)
{
    char t[12];
    int n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *p++ = t[--n];
    *p = 0;
    return p;
}
static char *put_s(char *p, const char *s) { while (*s) *p++ = *s++; *p = 0; return p; }
static void line2(int row, const char *a, uint32_t x, const char *b, uint32_t y)
{
    char l[32], *p = put_u(put_s(l, a), x);
    if (b) put_u(put_s(p, b), y);
    big_text(row, l);
}
void jt_show(void)
{
    uint32_t n = M.steps ? M.steps : 1, f = M.frames ? M.frames : 1, pn = M.pairs ? M.pairs : 1;
    uint64_t ps = (uint64_t)M.pair_sum_hi << 32 | M.pair_sum_lo;
    big_init();
    line2(0, "VB ", M.vbl_sum / f, " X ", M.vbl_max);
    line2(1, "SN ", M.snd_sum / f, " X ", M.snd_max);
    line2(2, "ST ", M.step_sum / n, " X ", M.step_max);
    line2(3, "DR ", M.draw_sum / n, " X ", M.draw_max);
    line2(4, "SH ", M.shl_sum / (f > n ? f - n : 1), " X ", M.shl_max);
    line2(5, "PR ", (uint32_t)(ps / pn), " X ", M.pair_max);
    line2(6, "OVER ", M.over, " OF ", pn);
    line2(7, "START ", M.start_clk, 0, 0);
    cps3v_text(0, 26, "VB VBLANK SN SOUND ST STEP DR DRAW SH SHELL");
    cps3v_text(0, 27, "PR 2-FRAME PAIR; MEAN X MAX; " ROUTE_NAME);
}
