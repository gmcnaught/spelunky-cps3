/* The shell's game hooks (src/shell/shell.h) on the play loop (game.h) */
#include "cps3.h"
#include "shell.h"
#include "pint.h"
#include "draw.h"
#include "front.h"
#include "pmsg.h"
#include "game.h"

struct game_cfg game_cfg = { .tail = 30, .level = 1, .enemies = 1, .nodark = -1, .room = -1 };
int32_t game_rec, game_rec1 = -1, game_steps;
void (*game_rec_hook)(int32_t rec);               /* tests: called at each record point with its number */
uint8_t game_over;
int32_t game_end_room = -1;
#define GAME_OVER_WAIT 900                        /* steps: 30 s at room_speed 30 */
static int32_t over_wait;

/* scrUpdateHighscores' store (play.h play_hs_hook): the run's globals into the EEPROM (src/shell hs_update); the
   "new" marks go to the scores room (front_new). The tunnel man and the shortcuts are not on the cabinet (docs/
   GAMELOOP.md section 3): global.tunnel1 / 2 keep the EEPROM's values, usedShortcut is false; no minigames */
static void hs_store(int type)
{
    struct hs_run r;
    if (game_cfg.route && !game_cfg.scores) return;
    if (play_god) return;                         /* INVINCIBLE (dev): testing only, no scores */
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

/* ---- the game capture (game.h): read-only on the play state ---- */
#ifndef CAPTURE_SECTION
#define CAPTURE_SECTION
#endif
#ifndef CAPTURE_REV
#define CAPTURE_REV 0u                            /* the build's git commit (32 bits; tests/game REV=) */
#endif
#ifndef CAPTURE_DIRTY
#define CAPTURE_DIRTY 0
#endif
struct capture capture CAPTURE_SECTION;
static uint8_t cap_on;                            /* this game is recorded (a cabinet game) */
static uint32_t cap_crc;

/* FNV-1a over 32-bit values, folded to 16 bits; the position's bits as stored (float, or the fixed-point int) */
static uint32_t cap_h;
static void cap_w(uint32_t v)
{
    int k;
    for (k = 0; k < 4; k++) {
        cap_h ^= (v >> (8 * k)) & 0xff;
        cap_h *= 16777619u;
    }
}
uint16_t capture_hash(void)
{
    int k;
    cap_h = 2166136261u;
    cap_w((uint32_t)PW.n); cap_w((uint32_t)PW.next_id); cap_w((uint32_t)PW.room);
    cap_w((uint32_t)PW.xview); cap_w((uint32_t)PW.yview);
    cap_w((uint32_t)PG.plife); cap_w((uint32_t)PG.bombs); cap_w((uint32_t)PG.rope); cap_w((uint32_t)PG.money);
    cap_w((uint32_t)G.currLevel);
    for (k = 0; k < 16; k++) cap_w(g_rng.s[k]);
    cap_w(g_rng.i);
    if (PL.idx != NOONE) {
        union { pos p; uint32_t u; } x, y;
        x.p = PX(PL.idx).x;
        y.p = PX(PL.idx).y;
        cap_w(x.u); cap_w(y.u); cap_w(PL.dead);
    }
    return (uint16_t)(cap_h ^ cap_h >> 16);
}

/* tests: the state compared step by step between a cabinet run and its host replay (tests/game main.c's marker
   block for MAME, host.c HOST_STEPLOG): life, money, level, the player's x / y bits (0 without a player), the RNG's
   hash */
void game_probe(uint32_t *o)
{
    int k;
    o[0] = (uint32_t)PG.plife;
    o[1] = (uint32_t)PG.money;
    o[2] = (uint32_t)G.currLevel;
    o[3] = o[4] = 0;
    if (PL.idx != NOONE) {
        union { pos p; uint32_t u; } x, y;
        x.p = PX(PL.idx).x;
        y.p = PX(PL.idx).y;
        o[3] = x.u;
        o[4] = y.u;
    }
    cap_h = 2166136261u;
    for (k = 0; k < 16; k++) cap_w(g_rng.s[k]);
    cap_w(g_rng.i);
    o[5] = cap_h;
}

static void cap_begin(uint32_t seed)
{
    uint32_t *h = capture.h;
    cap_on = !game_cfg.route;
    if (!cap_on) return;
    h[CAP_W_MAGIC] = 0;                           /* invalid while it is rewritten */
    h[CAP_W_VERSION] = CAP_VERSION;
    h[CAP_W_FLAGS] = (play_toggle_run_on ? CAP_F_TOGGLE_RUN : 0) | (draw_smooth ? CAP_F_SMOOTH : 0) |
                     (play_god ? CAP_F_GOD : 0) | (CAPTURE_DIRTY ? CAP_F_DIRTY : 0)
#ifdef SHELL_DEV
                     | CAP_F_DEV
#endif
        ;
    h[CAP_W_REV] = CAPTURE_REV;
    h[CAP_W_SEED] = seed;
    h[CAP_W_STEPS] = h[CAP_W_NENT] = h[CAP_W_NCHK] = 0;
    h[CAP_W_LEVEL] = (uint32_t)G.currLevel;
    h[CAP_W_PLIFE] = (uint32_t)PG.plife;
    h[CAP_W_MONEY] = (uint32_t)PG.money;
    h[CAP_W_ROOM] = (uint32_t)PW.room;
    h[CAP_W_DEAD] = 0;
    h[CAP_W_END_ROOM] = (uint32_t)-1;
    h[CAP_W_MAGIC] = CAP_MAGIC;
}

/* a step's controls, before play_step */
static void cap_keys(uint16_t down)
{
    uint32_t *h = capture.h, n = h[CAP_W_NENT];
    uint16_t m = (uint16_t)((down & 0x3ffu) | (down & KEY_PAY ? 0x400u : 0));
    if (h[CAP_W_FLAGS] & CAP_F_TRUNC) return;
    if (down & ~(0x3ffu | KEY_PAY)) h[CAP_W_FLAGS] |= CAP_F_LOSSY;
    if (n && (capture.ent[n - 1] & 0x7ffu) == m && capture.ent[n - 1] < 0xf800u)
        capture.ent[n - 1] += 0x800u;
    else if (n < CAP_ENT_MAX) {
        capture.ent[n] = m;
        h[CAP_W_NENT] = n + 1;
    } else {
        h[CAP_W_FLAGS] |= CAP_F_TRUNC;
        return;
    }
    h[CAP_W_STEPS]++;
}

/* after the step: the checkpoint hash and the state at the end */
static void cap_after(int r)
{
    uint32_t *h = capture.h;
    if (!(h[CAP_W_FLAGS] & CAP_F_TRUNC) && h[CAP_W_STEPS] % CAP_CHK_EVERY == 0) {
        if (h[CAP_W_NCHK] < CAP_CHK_MAX) capture.chk[h[CAP_W_NCHK]++] = capture_hash();
        else h[CAP_W_FLAGS] |= CAP_F_TRUNC;
    }
    h[CAP_W_LEVEL] = (uint32_t)G.currLevel;
    h[CAP_W_PLIFE] = (uint32_t)PG.plife;
    h[CAP_W_MONEY] = (uint32_t)PG.money;
    h[CAP_W_ROOM] = (uint32_t)PW.room;
    h[CAP_W_DEAD] = PL.idx != NOONE ? PL.dead : (uint32_t)-1;
    if (r) {
        h[CAP_W_FLAGS] |= CAP_F_OVER;
        h[CAP_W_END_ROOM] = (uint32_t)r;
    }
}

static uint32_t cap_size(void)                    /* without the CRC */
{
    const uint32_t *h = capture.h;
    if (h[CAP_W_MAGIC] != CAP_MAGIC || h[CAP_W_VERSION] != CAP_VERSION || h[CAP_W_NENT] > CAP_ENT_MAX ||
        h[CAP_W_NCHK] > CAP_CHK_MAX)
        return 0;
    return 4 * CAP_HDR + 2 * (h[CAP_W_NENT] + h[CAP_W_NCHK]);
}
static uint8_t cap_byte(uint32_t k)
{
    const uint32_t *h = capture.h;
    uint16_t v;
    if (k < 4 * CAP_HDR) return (uint8_t)(h[k >> 2] >> (24 - 8 * (k & 3)));
    k -= 4 * CAP_HDR;
    if (k < 2 * h[CAP_W_NENT]) v = capture.ent[k >> 1];
    else {
        k -= 2 * h[CAP_W_NENT];
        if (k >= 2 * h[CAP_W_NCHK]) return 0;
        v = capture.chk[k >> 1];
    }
    return (uint8_t)(k & 1 ? v : v >> 8);
}
uint32_t game_capture_size(void)
{
    uint32_t n = cap_size(), k, c = 0xffffffffu;
    int b;
    if (!n) return 0;
    for (k = 0; k < n; k++) {                     /* CRC-32 (zlib's) */
        c ^= cap_byte(k);
        for (b = 0; b < 8; b++) c = c >> 1 ^ (0xedb88320u & -(c & 1));
    }
    cap_crc = ~c;
    return n + 4;
}
uint8_t game_capture_byte(uint32_t k)
{
    uint32_t n = cap_size();
    if (k < n) return cap_byte(k);
    k -= n;
    return k < 4 ? (uint8_t)(cap_crc >> (24 - 8 * k)) : 0;
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
#ifdef SHELL_DEV
    play_god = !game_cfg.route && SH.st.invincible;   /* the developer option; routes: off */
#endif
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
    play_god_life = PG.plife;
    {
        uint32_t seed = game_cfg.seed ? game_cfg.seed : shell_seed();
        cap_begin(seed);                          /* a cabinet game: its capture starts (game.h) */
        rng_seed(&g_rng, seed);
    }
    gen_room_force = game_cfg.room <= 3 ? game_cfg.room : -1;
    play_level_start(RUNNER_ID_LEVEL);                     /* the runner's instance id counter at rLevel (playhost) */
    gen_room_force = -1;
    if (game_cfg.room == R_rEnd) {                /* a route from the ending's first room (tools/tracer.py
                                                     TRACE_ROOM=rEnd): the play loop goes there at its first step,
                                                     as rOlmec's oXEnd does; the RNG and the instance id counter as
                                                     the title flow leaves them (random_set_seed(SEED)) */
        rng_seed(&g_rng, game_cfg.seed);
        PW.next_id = RUNNER_ID_LEVEL;
        play_goto_room = R_rEnd;
    }
    game_rec = 0;
    game_rec1 = -1;
    game_steps = 0;
    game_over = 0;
    game_end_room = -1;
    over_wait = 0;
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
    } else {
        keys = in->down;
        if (cap_on) cap_keys(keys);
        /* an unattended game-over panel (HD waits for a press): after GAME_OVER_WAIT steps on it the cabinet presses
           attack itself, every other step (the first press shows the final score, the next goes to rHighscores) */
        if (PL.idx != NOONE && PL.dead && PGAME.drawStatus > 0) {
            if (++over_wait > GAME_OVER_WAIT && (over_wait & 1)) keys |= K_ATTACK;
        } else
            over_wait = 0;
    }
    do                                            /* the frame ended at a room change before oGamepad's Step: the */
        r = play_step(keys, rec_cb);              /* same keys again (playhost), nothing drawn in that frame */
    while (r == PLAY_ROOM_EARLY);
    if (play_god) play_god_hold();                /* the life the step's collisions took, before the HUD */
    game_steps++;
    if (cap_on) cap_after(r);
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
