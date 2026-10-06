/* tests/gametime: the game program's frame budget in three sections: the attract mode (src/front, ATTRACT_FRAMES
 * frames from boot), game 1 (route 1, build/route.h) and game 2 (route 2, build/route2.h), each started by a coin
 * and Start. src/main/main.c's frame loop is compiled here (included below)
 * with its calls wrapped, so each part of every frame is timed with the FRT (phi / 32, wraps counted at each read:
 * the frame loop reads it many times a frame):
 *   vbl   draw_vblank() + draw_list_send() + draw_list_sync()   (the VBlank work: tilemap registers, scrolls,
 *         the sprite-list DMA and the check that it ended)
 *   snd   snd_frame()
 *   step  shell_frame() of a step frame minus its draw (the game step, input, credits)
 *   draw  game_draw() (src/draw: the display list)
 *   shl   shell_frame() of the other frame
 * A step pair is the frames from one game step to the next (two at 30 Hz): its busy clocks = vbl + snd + shell
 * of both frames. The frames before the first step (attract, and game_begin's level start: start_clk, the
 * largest of them) are left out of the frame figures. The route (tests/game's mkroute.py, build/route.h) plays from the cabinet's coin and Start.
 * Results, when the route has ended (marker state 1), in sprite RAM at 0x0402e000 (marker.ld) for
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
static uint32_t fr_vbl, fr_v, fr_snd, pair_acc;
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
static void t_draw_list_send(void) { uint32_t t = now(); draw_list_send(); fr_vbl += (now() - t) * 32; }
static void t_draw_list_sync(void) { uint32_t t = now(); draw_list_sync(); fr_vbl += (now() - t) * 32; }
static void t_snd_frame(void) { uint32_t t = now(); snd_frame(); fr_snd = (now() - t) * 32; }
static uint8_t seen0[NSEC];
static int t_shell_frame(uint32_t p0, uint32_t p1, uint32_t lines)
{
    uint32_t t, c, steps0 = (uint32_t)game_steps;
    int r, sec = (int)M.sec;
    volatile struct sec *s;
    if (M.state) {                                /* done: the results screen, an empty display list */
        cps3v_begin();
        cps3v_end();
        return 0;
    }
    tdraw = 0;
    fr_v = fr_vbl;                                /* this frame's VBlank work (0 on a late frame: src/main/main.c) */
    fr_vbl = 0;
    t = now();
    r = shell_frame(p0, p1, lines);
    c = (now() - t) * 32;
    if (sec >= NSEC) return r;
    s = &M.s[sec];
    if (sec > 0 && !seen0[sec]) {                 /* a game section begins with game_begin (game_steps back to 0, */
        if (game_steps != 0 && (uint32_t)game_steps >= steps0) return r;   /* maybe a step in the same frame): */
        seen0[sec] = 1;                           /* the attract frames before it are not counted, the frame */
        if (c > s->start_clk) s->start_clk = c;   /* with game_begin is the start */
        return r;
    }
    if (sec > 0 && game_steps == 0) {             /* a game's frames before its first step: the start (game_begin's */
        if (c > s->start_clk) s->start_clk = c;   /* level start in one of them): not in the frame figures */
        return r;
    }
    s->frames++;
    s->vbl_sum += fr_v; max_to(&s->vbl_max, fr_v);
    s->snd_sum += fr_snd; max_to(&s->snd_max, fr_snd);
    if (r && (sec == 0 || (uint32_t)game_steps != steps0)) {   /* a step (attract step or game step) ran */
        if (pair_open) {                          /* the previous pair is complete */
            uint32_t lo = s->pair_sum_lo + pair_acc;
            if (lo < s->pair_sum_lo) s->pair_sum_hi++;
            s->pair_sum_lo = lo;
            if (s->pairs < PAIRS) s->pair[s->pairs] = pair_acc;
            if (pair_acc > s->pair_max) { s->pair_max = pair_acc; s->pair_max_at = s->pairs; }
            if (pair_acc > 2 * 419470u) s->over++;
            s->pairs++;
        }
        pair_open = 1;
        pair_acc = 0;
        s->steps++;
        s->step_sum += c - tdraw; max_to(&s->step_max, c - tdraw);
        s->draw_sum += tdraw; max_to(&s->draw_max, tdraw);
    } else {
        s->shl_sum += c; max_to(&s->shl_max, c);
    }
    pair_acc += fr_v + fr_snd + c;
    if (sec > 0 && game_over) {                   /* the game ended: next section */
        pair_open = 0;
        gt_section_end();
    } else if (sec == 0 && M.sec != 0) {
        pair_open = 0;
    }
    return r;
}

/* src/main/main.c with the calls above in place of the real ones (its headers are already included) */
#define draw_vblank() t_draw_vblank()
#define draw_list_send() t_draw_list_send()
#define draw_list_sync() t_draw_list_sync()
#define snd_frame() t_snd_frame()
#define shell_frame(a, b, c) t_shell_frame(a, b, c)
#include "../../src/main/main.c"
#undef draw_vblank
#undef draw_list_send
#undef draw_list_sync
#undef snd_frame
#undef shell_frame

