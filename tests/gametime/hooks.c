/* tests/gametime: the marker block, the FRT clock, the route's inputs, the draw timing hooks and the results screen
   (main.c times the frame loop) */
#include "cps3.h"
#include "shell.h"
#include "draw.h"
#include "game.h"
#include "play.h"
#include "route.h"
#include "gametime.h"
#include "front.h"
extern uint32_t front_seed;

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
#include "route2.h"
#ifndef ATTRACT_FRAMES
#define ATTRACT_FRAMES 3600                       /* the attract section: 60 s from boot */
#endif
void main_boot(void)
{
    uint32_t *d;
    int k;
    for (d = (uint32_t *)__sprbss_a_start; d < (uint32_t *)__sprbss_a_end; d++) *d = 0;
    for (d = (uint32_t *)__sprbss_b_start; d < (uint32_t *)__sprbss_b_end; d++) *d = 0;
    frt_start();
    for (d = (uint32_t *)&M, k = 0; k < (int)(sizeof M / 4); k++) d[k] = 0;
    M.magic = 0x47544d32;                         /* 'GTM2' */
#ifdef GAME_SMOOTH
    draw_smooth = 1;                              /* smooth motion (SMOOTH=1): its draw and interrupt costs */
#endif
    front_seed = ROUTE_SEED;                      /* the intro's randomize() */
    game_cfg.route = route_keys;
    game_cfg.nroute = ROUTE_N;
    game_cfg.tail = ROUTE_TAIL;
    game_cfg.seed = ROUTE_SEED;
    game_cfg.level = ROUTE_LEVEL;
    game_cfg.money = ROUTE_MONEY;
    game_cfg.enemies = ROUTE_ENEMIES;
    game_cfg.nodark = ROUTE_NODARK;
    game_cfg.room = ROUTE_ROOM;
    game_cfg.globals = ROUTE_GLOBALS;
}

/* the cabinet: nothing during the attract section; then a coin and Start (game 1, route 1); after game 1 is over,
   route 2 set up, another coin and Start (game 2) */
static uint32_t frame, coin_at = ATTRACT_FRAMES;
void gt_section_end(void)
{
    M.sec++;
    if (M.sec == 1) return;                       /* game 1 starts with the coin at ATTRACT_FRAMES */
    if (M.sec == 2) {
        game_cfg.route = route2_keys;
        game_cfg.nroute = ROUTE2_N;
        game_cfg.tail = ROUTE2_TAIL;
        game_cfg.seed = ROUTE2_SEED;
        game_cfg.level = ROUTE2_LEVEL;
        game_cfg.money = ROUTE2_MONEY;
        game_cfg.enemies = ROUTE2_ENEMIES;
        game_cfg.nodark = ROUTE2_NODARK;
        game_cfg.room = ROUTE2_ROOM;
        game_cfg.globals = ROUTE2_GLOBALS;
        coin_at = frame + 60;
        return;
    }
    M.state = 1;
    jt_show();
}
void main_inputs(uint32_t *pad0, uint32_t *pad1, uint32_t *lines)
{
    frame++;
    if (M.sec == 0 && frame == ATTRACT_FRAMES) gt_section_end();
    *pad0 = frame >= coin_at + 20 && frame < coin_at + 24 ? CPS3_START : 0;
    *pad1 = 0;
    *lines = frame >= coin_at && frame < coin_at + 4 ? CR_COIN1 : 0;
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
static void put_col(char *l, int col, uint32_t v)
{
    char t[12];
    put_u(t, v);
    for (int k = 0; t[k]; k++) l[col + k] = t[k];
}
void jt_show(void)
{
    static const char *const rows[] = { "VB AV", "VB MAX", "SN AV", "SN MAX", "ST AV", "ST MAX", "DR AV", "DR MAX",
                                        "SH AV", "SH MAX", "PR AV", "PR MAX", "PR OVER", "PAIRS", "START",
                                        "PR AT" };
    char l[48];
    int r, c;
    big_init();
    big_text(0, "FRAME BUDGET");
    cps3v_text(0, 3, "        ATTRACT    GAME 1     GAME 2");
    for (r = 0; r < 16; r++) {
        for (c = 0; c < 47; c++) l[c] = ' ';
        l[47] = 0;
        for (c = 0; rows[r][c]; c++) l[c] = rows[r][c];
        for (c = 0; c < NSEC; c++) {
            const volatile struct sec *s = &M.s[c];
            uint32_t f = s->frames ? s->frames : 1, n = s->steps ? s->steps : 1, p = s->pairs ? s->pairs : 1;
            uint64_t ps = (uint64_t)s->pair_sum_hi << 32 | s->pair_sum_lo;
            uint32_t v[16] = { s->vbl_sum / f, s->vbl_max, s->snd_sum / f, s->snd_max, s->step_sum / n, s->step_max,
                               s->draw_sum / n, s->draw_max, s->shl_sum / (f > n ? f - n : 1), s->shl_max,
                               (uint32_t)(ps / p), s->pair_max, s->over, s->pairs, s->start_clk, s->pair_max_at };
            put_col(l, 8 + 11 * c, v[r]);
        }
        l[42] = 0;
        cps3v_text(0, 4 + r, l);
    }
    cps3v_text(0, 20, "VB VBLANK SN SOUND ST STEP DR DRAW SH SHELL");
    cps3v_text(0, 21, "PR 2-FRAME PAIR, OVER: > 838940, AT: MAX PAIR");
    cps3v_text(0, 22, "GAME 1 " ROUTE_NAME);
    cps3v_text(0, 23, "GAME 2 " ROUTE2_NAME);
}
