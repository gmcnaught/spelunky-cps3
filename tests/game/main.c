/* tests/game: src/main's game program on a route (build/route.h), with a marker block for scripts/lua/gamesnap.lua
 * at 0x02000000 (main RAM, the SDK's .trace section):
 *   +0  'SGM1'           +4  state: 1 = the route ended
 *   +8  shown: the phase-1 record whose frame the last sent list shows (-1 none)
 *   +12 ack: written by the Lua script after its snapshot of `shown`
 *   +16 wait: 1 while the program holds a snapshot frame (until ack == shown)
 *   +20 draws, +24 draw clocks last, +28 max, +32 sum (low), +36 sum (high)
 *   +40 vblank clocks last, +44 max, +48 entries last, +52 entries max, +56 sprites, +60 cells
 *   +64 unsup, +68 todo, +72 noart, +76 dropped, +80 route steps, +84 view x, +88 view y, +92 room
 *   +96 records, +100 snapshot index, +104..+120 DRAW_PROFILE: FRC ticks (x 32 clocks) of the draw's parts
 * The cabinet's coin and Start are simulated (frames 20 and 40): the game starts through the shell. CPU clocks from
 * the FRC at phi / 32 (32 clocks a tick, 2.1 M clocks before it wraps; a draw is far below that). */
#include "cps3.h"
#include "shell.h"
#include "draw.h"
#include "game.h"
#include "play.h"
#include "route.h"

struct marker {
    uint32_t magic, state;
    int32_t shown, ack;
    uint32_t wait, draws, dclk, dmax, dsum_lo, dsum_hi, vclk, vmax, ent, ent_max, sprites, cells;
    uint32_t unsup, todo, noart, dropped, steps;
    int32_t vx, vy, room, recs, snap;
    uint32_t prof[5];
};
static volatile struct marker M __attribute__((section(".trace"), used));

static uint32_t ticks(void)
{
    volatile uint8_t *f = (volatile uint8_t *)0xfffffe12u;
    uint32_t h = f[0], l = f[1], h2 = f[0];
    if (h2 != h) { l = f[1]; h = h2; }
    return h << 8 | l;
}

extern char __sprbss_a_start[] __asm__("__sprbss_a_start"), __sprbss_a_end[] __asm__("__sprbss_a_end");
extern char __sprbss_b_start[] __asm__("__sprbss_b_start"), __sprbss_b_end[] __asm__("__sprbss_b_end");
void main_boot(void)
{
    uint32_t *d;
    for (d = (uint32_t *)__sprbss_a_start; d < (uint32_t *)__sprbss_a_end; d++) *d = 0;
    for (d = (uint32_t *)__sprbss_b_start; d < (uint32_t *)__sprbss_b_end; d++) *d = 0;
    *(volatile uint8_t *)0xfffffe16 = 0x01;       /* TCR: FRC at phi / 32 */
    M.state = 0;
    M.shown = -1;
    M.ack = -1;
    M.wait = 0;
    M.draws = M.dmax = M.dsum_lo = M.dsum_hi = M.vmax = M.ent_max = 0;
    M.snap = 0;
    M.magic = 0x53474d31;
#ifdef GAME_NOHUD
    draw_hud_on = 0;
#endif
#ifdef GAME_DARK
    draw_dark_force = GAME_DARK;                  /* the fade path on any level (scripts/game_check.sh DARK=a8) */
#endif
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

static uint32_t t0, pend_draw;
static int32_t pend_rec = -1;
void main_draw_begin(void) { t0 = ticks(); }
void main_draw_end(void)
{
    uint32_t c = ((ticks() - t0) & 0xffff) * 32, lo;
    M.dclk = c;
    if (c > M.dmax) M.dmax = c;
    lo = M.dsum_lo + c;
    if (lo < M.dsum_lo) M.dsum_hi++;
    M.dsum_lo = lo;
    M.draws++;
    M.ent = draw_st.entries;
    if (draw_st.entries > M.ent_max) M.ent_max = draw_st.entries;
    M.sprites = draw_st.sprites;
    M.unsup = draw_st.unsup;
    M.todo = draw_st.todo;
    M.noart = draw_st.noart;
    M.dropped = draw_st.dropped;
    M.steps = (uint32_t)game_steps;
    M.vx = PW.xview;
    M.vy = PW.yview;
    M.room = PW.room;
    M.recs = game_rec;
    for (int k = 0; k < 5; k++) M.prof[k] = draw_st.prof[k] * 32;
    pend_rec = game_rec1;
    pend_draw = 1;
}
static uint32_t v0;
void main_vblank_begin(void) { v0 = ticks(); }
void main_vblank_end(void)
{
    uint32_t c = ((ticks() - v0) & 0xffff) * 32;
    if (!pend_draw) return;                       /* the frame after a draw: its tilemap work */
    M.vclk = c;
    if (c > M.vmax) M.vmax = c;
    M.cells = draw_st.cells;
}

static int is_snap(int32_t r)
{
    int k;
    for (k = 0; k < NSNAPS; k++)
        if (snaps[k] == r) return 1;
    return 0;
}

/* after the VBlank that sent the list drawn after record pend_rec: hold a snapshot frame until the script acks */
void main_frame_done(void)
{
    if (game_over) M.state = 1;
    if (!pend_draw) return;
    pend_draw = 0;
    M.shown = pend_rec;
    if (!is_snap(pend_rec)) return;
    M.wait = 1;
    while (M.ack != pend_rec) {
        cps3v_wait_vblank();
        cps3v_vblank();                           /* the same list again */
    }
    M.wait = 0;
    M.snap++;
}
