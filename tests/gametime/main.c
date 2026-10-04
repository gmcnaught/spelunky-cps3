/* tests/gametime: the game program's frame budget. src/main/main.c's frame loop is compiled here (included below)
 * with its calls wrapped, so each part of every frame is timed with the FRT (phi / 32, wraps counted at each read:
 * the frame loop reads it many times a frame):
 *   vbl   draw_vblank() + cps3v_vblank()      (the VBlank work: tilemap registers, scrolls, sprite-list DMA)
 *   snd   snd_frame()
 *   step  shell_frame() of a step frame minus its draw (the game step, input, credits)
 *   draw  game_draw() (src/draw: the display list)
 *   shl   shell_frame() of the other frame
 * A step pair is the frames from one game step to the next (two at 30 Hz): its busy clocks = vbl + snd + shell
 * of both frames. The frames before the first step (attract, and game_begin's level start: start_clk, the
 * largest of them) are left out of the frame figures. The route (tests/game's mkroute.py, build/route.h) plays from the cabinet's coin and Start.
 * Results, when the route has ended (marker state 1), in main RAM at 0x02000000 (.trace) for
 * scripts/lua/gametime.lua, and on screen as large text (jtcps3: ../playsh2/bigtext.c). */
#include "cps3.h"
#include "sprites.h"
#include "hudart.h"
#include "shell.h"
#include "snd.h"
#include "draw.h"
#include "game.h"
#include "play.h"
#include "gametime.h"

/* the frame loop's calls, timed */
static uint32_t fr_vbl, fr_snd, pair_acc;
static int pair_open;
static void max_to(volatile uint32_t *m, uint32_t v) { if (v > *m) *m = v; }
static void t_draw_vblank(void)
{
    uint32_t t;
    int k;
    if (M.state) {                                /* the results screen: the tilemaps off */
        for (k = 0; k < 4; k++) cps3v_tilemap(k, 0, 0, CPS3V_MAP_UNIT(0), 0);
        return;
    }
    t = now();
    draw_vblank();
    fr_vbl = (now() - t) * 32;
}
static void t_cps3v_vblank(void) { uint32_t t = now(); cps3v_vblank(); fr_vbl += (now() - t) * 32; }
static void t_snd_frame(void) { uint32_t t = now(); snd_frame(); fr_snd = (now() - t) * 32; }
static int t_shell_frame(uint32_t p0, uint32_t p1, uint32_t lines)
{
    uint32_t t, c, steps0 = (uint32_t)game_steps;
    int r;
    if (M.state) {                                /* the route has ended: the results screen, an empty display list */
        cps3v_begin();
        cps3v_end();
        return 0;
    }
    tdraw = 0;
    t = now();
    r = shell_frame(p0, p1, lines);
    c = (now() - t) * 32;
    if (steps0 == 0 && game_steps == 0) {         /* attract and the frame that starts the game (game_begin: the */
        if (c > M.start_clk) M.start_clk = c;     /* level start): not in the frame figures */
        return r;
    }
    M.frames++;
    M.vbl_sum += fr_vbl; max_to(&M.vbl_max, fr_vbl);
    M.snd_sum += fr_snd; max_to(&M.snd_max, fr_snd);
    if (r && (uint32_t)game_steps != steps0) {    /* a game step ran in this frame: a new pair starts */
        if (pair_open) {                          /* the previous pair is complete */
            uint32_t lo = M.pair_sum_lo + pair_acc;
            if (lo < M.pair_sum_lo) M.pair_sum_hi++;
            M.pair_sum_lo = lo;
            if (M.pairs < PAIRS) M.pair[M.pairs] = pair_acc;
            if (pair_acc > M.pair_max) { M.pair_max = pair_acc; M.pair_max_at = M.pairs; }
            if (pair_acc > 2 * 419470u) M.over++;
            M.pairs++;
        }
        pair_open = 1;
        pair_acc = 0;
        M.steps++;
        M.step_sum += c - tdraw; max_to(&M.step_max, c - tdraw);
        M.draw_sum += tdraw; max_to(&M.draw_max, tdraw);
    } else {
        M.shl_sum += c; max_to(&M.shl_max, c);
    }
    pair_acc += fr_vbl + fr_snd + c;
    if (game_over) {
        M.state = 1;
        jt_show();
    }
    return r;
}

/* src/main/main.c with the calls above in place of the real ones (its headers are already included) */
#define draw_vblank() t_draw_vblank()
#define cps3v_vblank() t_cps3v_vblank()
#define snd_frame() t_snd_frame()
#define shell_frame(a, b, c) t_shell_frame(a, b, c)
#include "../../src/main/main.c"
#undef draw_vblank
#undef cps3v_vblank
#undef snd_frame
#undef shell_frame

