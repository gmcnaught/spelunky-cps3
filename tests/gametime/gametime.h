/* tests/gametime: shared between main.c (the timed frame loop) and hooks.c */
#ifndef GAMETIME_H
#define GAMETIME_H
#include <stdint.h>
#define PAIRS 1024
struct marker {
    uint32_t magic, state, frames, steps, pairs;
    uint32_t vbl_sum, vbl_max, snd_sum, snd_max, step_sum, step_max, draw_sum, draw_max, shl_sum, shl_max;
    uint32_t pair_sum_lo, pair_sum_hi, pair_max, pair_max_at, start_clk, over;
    uint32_t pair[PAIRS];                         /* each pair's busy clocks, in order (the first: the game start) */
};
extern volatile struct marker M;
extern uint32_t tdraw;                            /* the last game_draw's clocks (hooks.c) */
uint32_t now(void);                               /* FRC ticks (x 32 clocks) */
void jt_show(void);
#endif
