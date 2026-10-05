/* The shell's game hooks (src/shell/shell.h) on the play loop (game.h) */
#include "cps3.h"
#include "shell.h"
#include "pint.h"
#include "draw.h"
#include "front.h"
#include "pmsg.h"
#include "game.h"

struct game_cfg game_cfg = { 0, 0, 30, 0, 1, 0, 1, -1, -1, 0, 0 };
int32_t game_rec, game_rec1 = -1, game_steps;
void (*game_rec_hook)(int32_t rec);               /* tests: called at each record point with its number */
uint8_t game_over;
int32_t game_end_room = -1;

/* scrUpdateHighscores' store (play.h play_hs_hook): the run's globals into the EEPROM (src/shell hs_update); the
   "new" marks go to the scores room (front_new). The tunnel man and the shortcuts are not on the cabinet (docs/
   GAMELOOP.md section 3): global.tunnel1 / 2 keep the EEPROM's values, usedShortcut is false; no minigames */
static void hs_store(int type)
{
    struct hs_run r;
    if (game_cfg.route && !game_cfg.scores) return;
    r.money = PG.money;
    r.time = PG.time;
    r.kills = PG.kills;
    r.damsels = PG.damsels;
    r.tunnel1 = SH.g.tunnel1;
    r.tunnel2 = SH.g.tunnel2;
    r.mini1 = r.mini2 = r.mini3 = 0;
    r.used_shortcut = 0;
    r.keep_score = 1;
    front_new = (uint8_t)hs_update(&SH.hs, &SH.st, &r, (enum hs_type)type);
}

/* tools/tracer.py's record points: the tracer (and test/host/playhost.c) reads the view there, which the play
   state depends on (view_read: the follow is applied when the view was set this frame) */
static void rec_cb(int phase)
{
    view_read();
    if (phase == 1) game_rec1 = game_rec;
    if (game_rec_hook) game_rec_hook(game_rec);
    game_rec++;
}

/* attract: HD's intro, title and high-scores rooms (src/front) */
void game_attract_step(void)
{
    front_rec_cb = rec_cb;
    if (!front_on) {
        game_rec = 0, game_rec1 = -1;
        if (game_end_room == R_rHighscores) front_start_at(R_rHighscores);   /* the game's room_goto(rHighscores) */
        game_end_room = -1;
    }
    front_step();
}

/* game_cfg.globals: one "name=value" (test/host/playhost.c set_global's names); 0 if the name is not known */
static int str_is(const char *a, const char *b, int n)
{
    int k;
    for (k = 0; k < n; k++)
        if (a[k] != b[k]) return 0;
    return b[n] == 0;
}
static int set_global(const char *s, int n)
{
    /* name, size and place of each global (sizes from the structs: 1, 2 or 4 bytes) */
#define GV(st, f) { #f, sizeof(st.f), &st.f }
    static const struct { const char *name; uint8_t sz; void *at; } gl[] = {
        GV(G, madeMoai), GV(G, kaliPunish), GV(PG, arrows), GV(PG, bombs), GV(PG, rope), GV(PG, plife),
        GV(PG, money), GV(PG, hasJetpack), GV(PG, hasCape), GV(PG, hasParachute), GV(PG, hasMitt),
        GV(PG, hasGloves), GV(PG, hasSpringShoes), GV(PG, hasSpikeShoes), GV(PG, hasKapala), GV(PG, hasAnkh),
        GV(PG, hasCompass), GV(PG, hasStickyBombs), GV(PG, hasUdjatEye), GV(PG, hasCrown), GV(PG, hasJordans),
        GV(G, lake), GV(G, cityOfGold), GV(PG, kills), GV(PG, damsels), GV(PG, time),
    };
#undef GV
    char v[32];
    int e = 0, k, m, x = 0, neg = 0;
    while (e < n && s[e] != '=') e++;
    if (e == n) return 0;
    for (m = 0; e + 1 + m < n && m < 31; m++) v[m] = s[e + 1 + m] == '~' ? ' ' : s[e + 1 + m];
    v[m] = 0;
    if (str_is(s, "pickupItem", e)) {
        for (k = 0; k < PICK_COUNT; k++) {
            const char *p = pickup_names[k];
            int j = 0;
            while (p[j] && p[j] == v[j]) j++;
            if (!p[j] && !v[j]) { G.pickupItem = (uint8_t)k; return 1; }
        }
        return 0;
    }
    for (k = v[0] == '-'; v[k] >= '0' && v[k] <= '9'; k++) x = 10 * x + (v[k] - '0');
    neg = v[0] == '-';
    if (neg) x = -x;
    for (k = 0; k < (int)(sizeof gl / sizeof gl[0]); k++)
        if (str_is(s, gl[k].name, e)) {
            if (gl[k].sz == 1) *(uint8_t *)gl[k].at = (uint8_t)x;
            else if (gl[k].sz == 2) *(int16_t *)gl[k].at = (int16_t)x;
            else *(int32_t *)gl[k].at = x;
            return 1;
        }
    return 0;
}

void game_begin(void)
{
    front_stop();
    scrClearGlobals();                            /* plife / bombs / rope 4, every has* flag and count 0 */
    G.currLevel = game_cfg.level;
    PG.money = game_cfg.money;
    play_noenemy = !game_cfg.enemies;
    play_toggle_run_on = !game_cfg.route && SH.st.toggle_run;   /* the cabinet's setting; routes: HD's default */
    play_toggle_run = 0;                          /* each game starts walking (HD: once, scrInit) */
    if (game_cfg.nodark >= 0) G.noDarkLevel = (uint8_t)game_cfg.nodark;
    if (game_cfg.globals) {
        const char *s = game_cfg.globals;
        while (*s) {
            int n = 0;
            while (s[n] && s[n] != ',') n++;
            set_global(s, n);                     /* mkroute.py checked the names */
            s += n + (s[n] == ',');
        }
    }
    rng_seed(&g_rng, game_cfg.seed ? game_cfg.seed : SH.frame * 2654435761u + 1);
    gen_room_force = game_cfg.room <= 3 ? game_cfg.room : -1;
    play_level_start(110325);                     /* the runner's instance id counter at rLevel (playhost) */
    gen_room_force = -1;
    if (game_cfg.room == R_rEnd) {                /* a route from the ending's first room (tools/tracer.py
                                                     TRACE_ROOM=rEnd): the play loop goes there at its first step,
                                                     as rOlmec's oXEnd does; the RNG and the instance id counter as
                                                     the title flow leaves them (random_set_seed(SEED)) */
        rng_seed(&g_rng, game_cfg.seed);
        PW.next_id = 110325;
        play_goto_room = R_rEnd;
    }
    game_rec = 0;
    game_rec1 = -1;
    game_steps = 0;
    game_over = 0;
    game_end_room = -1;
    play_hs_hook = hs_store;
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
        game_end_room = r;
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
