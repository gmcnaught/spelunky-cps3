/* Arcade shell test (PLAN.md P8): shell_run (src/shell) with game hooks that log every call; scripts/shell_check.sh
   presses coins, starts and buttons in MAME (scripts/lua/shellin.lua) and tools/shellcheck.py checks the log.
   The game ends when its rope button is pressed. */
#include "cps3.h"
#include "shell.h"

#define LOG_N 1024
/* entry: frame, kind (0 attract step, 1 begin, 2 play step), mode, panel, credits, down, pressed, released */
struct shlog_e { uint32_t frame; uint8_t kind, mode, panel, credits; uint16_t down, pressed, released, pad; };
struct shlog { uint32_t magic, n; struct shlog_e e[LOG_N]; int32_t tunnel1, tunnel2, first_time, money; };
struct shlog shlog = { 0x53484c47u, 0, { { 0, 0, 0, 0, 0, 0, 0, 0, 0 } }, 0, 0, 0, 0 };

static void log_e(int kind, const struct shell_input *in)
{
    if (shlog.n >= LOG_N)
        return;
    struct shlog_e *e = &shlog.e[shlog.n++];
    e->frame = SH.frame - 1;
    e->kind = (uint8_t)kind;
    e->mode = (uint8_t)SH.mode;
    e->panel = SH.panel;
    e->credits = SH.cr.credits;
    e->down = in ? in->down : 0;
    e->pressed = in ? in->pressed : 0;
    e->released = in ? in->released : 0;
}

/* the high-score block as hs_boot read it (oGlobals Create's globals) */
void game_boot(void)
{
    shlog.tunnel1 = SH.g.tunnel1;
    shlog.tunnel2 = SH.g.tunnel2;
    shlog.first_time = SH.g.first_time;
    shlog.money = SH.hs.value[HS_MONEY];
}
void game_attract_step(void) { log_e(0, 0); }
void game_begin(void) { log_e(1, 0); }
/* the game ends at a rope press; game 1 stores a death with money 1234 (scrUpdateHighscores(0)) */
int game_step(const struct shell_input *in)
{
    log_e(2, in);
    if (!(in->pressed & KEY_ROPE))
        return 0;
    struct hs_run r = { .money = 1234, .time = 30000, .kills = 2, .damsels = 1, .tunnel1 = SH.g.tunnel1,
                        .tunnel2 = SH.g.tunnel2, .keep_score = 1 };
    hs_update(&SH.hs, &SH.st, &r, HS_DEATH);
    return 1;
}
void game_draw(void)
{
    cps3v_sprite(0, 0, 1, 1, 0, 0, 0);          /* something in the list */
}

int main(void)
{
    cps3_init();
    shell_run();
}
