/* The shell's game hooks (src/shell/shell.h) on the play loop (game.h) */
#include "cps3.h"
#include "shell.h"
#include "pint.h"
#include "draw.h"
#include "front.h"
#include "game.h"

struct game_cfg game_cfg = { 0, 0, 30, 0, 1, 0, 1 };
int32_t game_rec, game_rec1 = -1, game_steps;
uint8_t game_over;

/* tools/tracer.py's record points: the tracer (and test/host/playhost.c) reads the view there, which the play
   state depends on (view_read: the follow is applied when the view was set this frame) */
static void rec_cb(int phase)
{
    view_read();
    if (phase == 1) game_rec1 = game_rec;
    game_rec++;
}

/* attract: HD's intro, title and high-scores rooms (src/front) */
void game_attract_step(void)
{
    front_rec_cb = rec_cb;
    if (!front_on) game_rec = 0, game_rec1 = -1;
    front_step();
}

void game_begin(void)
{
    front_stop();
    gen_new_game();
    G.currLevel = game_cfg.level;
    PG.plife = 4;
    PG.bombs = 4;
    PG.rope = 4;
    PG.money = game_cfg.money;
    play_noenemy = !game_cfg.enemies;
    rng_seed(&g_rng, game_cfg.seed ? game_cfg.seed : SH.frame * 2654435761u + 1);
    play_level_start(110325);                     /* the runner's instance id counter at rLevel (playhost) */
    game_rec = 0;
    game_rec1 = -1;
    game_steps = 0;
    game_over = 0;
    draw_new_game();
}

int game_step(const struct shell_input *in)
{
    uint16_t keys;
    int r;
    if (game_over) return 1;
    if (game_cfg.route) {
        if (game_steps >= game_cfg.nroute + game_cfg.tail) {
            game_over = 1;
            return 1;
        }
        keys = game_steps < game_cfg.nroute ? game_cfg.route[game_steps] : 0;
    } else
        keys = in->down;
    do                                            /* the frame ended at a room change before oGamepad's Step: the */
        r = play_step(keys, rec_cb);              /* same keys again (playhost), nothing drawn in that frame */
    while (r == PLAY_ROOM_EARLY);
    game_steps++;
    if (r != 0) {                                 /* a room the play loop does not model */
        game_over = 1;
        return 1;
    }
    return 0;
}

void game_draw(void)
{
    if (front_on) {                               /* attract */
        main_draw_begin();
        draw_frame();
        main_draw_end();
        return;
    }
    if (game_steps == 0) return;
    main_draw_begin();
    draw_frame();
    main_draw_end();
}
