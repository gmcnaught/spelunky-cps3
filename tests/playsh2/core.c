/* tests/playsh2 core, shared by the SH-2 program (main.c) and the host reference (host.c): level generation cases
 * and route replays on the real src/game code, with a checksum of the state after each generation / step.
 * The platform supplies plat_begin() / plat_end() around each measured piece of work (timing) and plat_rec() (the
 * result). The route driver mirrors test/host/playhost.c (initial globals, next id 110325, 30 tail steps,
 * PLAY_ROOM_EARLY repeats the input, view_read() at each record point). */
#include "pint.h"
#include "core.h"
#include "snd.h"

/* the GML instance variables: in struct pin_ext (PE(p)) since 18aca5f, in struct pin before; alpha moved there
   later (snapcfg.h: written by the check scripts from the snapshot's play.h) */
#include "snapcfg.h"
#ifdef ALPHA_IN_EXT
#define ALPHA(p) (PE(p)->alpha)
#else
#define ALPHA(p) ((p)->alpha)
#endif
#ifdef PE
#define XT struct pin_ext
#define XP(p) PE(p)
#else
#define XT struct pin
#define XP(p) (p)
#endif

/* FNV-1a over 32-bit words (byte order independent: the words are mixed as values) */
static uint32_t H;
static void hw(uint32_t v)
{
    int k;
    for (k = 0; k < 4; k++) {
        H ^= (v >> (8 * k)) & 0xff;
        H *= 16777619u;
    }
}
static void hf(float f) { union { float f; uint32_t u; } c; c.f = f; hw(c.u); }
static void hd(double d) { union { double d; uint64_t u; } c; c.d = d; hw((uint32_t)c.u); hw((uint32_t)(c.u >> 32)); }
static void hrng(void) { int k; for (k = 0; k < 16; k++) hw(g_rng.s[k]); hw(g_rng.i); }

uint32_t sum_gen(void)
{
    int i, a;
    H = 2166136261u;
    hw((uint32_t)W.n);
    hw((uint32_t)W.next_id);
    for (i = 0; i < W.n; i++) {
        const struct inst *p = &W.in[i];
        if (!p->alive) continue;
        hw((uint32_t)p->id); hw((uint32_t)p->obj); hw((uint32_t)(uint16_t)p->x); hw((uint32_t)(uint16_t)p->y);
        hw((uint32_t)p->spr); hw((uint32_t)p->depth); hw(p->flags); hw((uint32_t)p->cost); hw((uint32_t)p->value);
        hw((uint32_t)(uint16_t)p->xvel); hw((uint32_t)(uint16_t)p->yvel);
        for (a = 0; a < ALARMS; a++) hw((uint32_t)p->alarm[a]);
    }
    hw((uint32_t)gntiles);
    for (i = 0; i < gntiles; i++) {
        hw((uint32_t)gtiles[i].bg); hw((uint32_t)gtiles[i].x); hw((uint32_t)gtiles[i].y);
        hw((uint32_t)gtiles[i].left); hw((uint32_t)gtiles[i].top); hw((uint32_t)gtiles[i].depth);
    }
    hrng();
    return H;
}

uint32_t sum_play(void)
{
    int i, a;
    H = 2166136261u;
    hw((uint32_t)PW.n); hw((uint32_t)PW.next_id); hw((uint32_t)PW.room); hw((uint32_t)PW.xview);
    hw((uint32_t)PW.yview); hw(play_time); hw((uint32_t)play_untranslated);
    hw((uint32_t)PG.plife); hw((uint32_t)PG.bombs); hw((uint32_t)PG.rope); hw((uint32_t)PG.money);
    hw((uint32_t)G.currLevel);
    for (i = 0; i < PW.n; i++) {
        const struct pin *p = &PW.in[i];
        if (!p->alive) continue;
        const XT *e = XP(p);
        hw((uint32_t)p->id); hw((uint32_t)p->obj); hw((uint32_t)p->spr);
        hf(p->x); hf(p->y); hf(p->img); hf(p->ispd); hf(p->depth);
        hd(p->xscale); hd(p->yscale); hd(p->angle); hd(ALPHA(p));
        hd(e->xVel); hd(e->yVel); hd(e->xAcc); hd(e->yAcc); hd(e->myGrav); hd(e->grav); hd(e->life);
        hd(e->px); hd(e->py); hd(e->direction);
        hw(p->visible); hw(e->held); hw((uint32_t)e->state); hw((uint32_t)e->counter);
        for (a = 0; a < 12; a++) hw((uint32_t)e->alarm[a]);
    }
    if (PL.idx != NOONE) {
        hw((uint32_t)PL.state); hw((uint32_t)PL.facing); hw((uint32_t)PL.fallTimer); hw((uint32_t)PL.jumpTime);
        hw((uint32_t)PL.holdItem); hd(PL.gravityIntensity); hw((uint32_t)PL.hangCount); hw(PL.dead);
    }
    hrng();
    return H;
}

/* level generation, as test/host/genhost.c runs a case "<seed> <level> 0 1" */
void run_gen(int job, uint32_t seed, int level)
{
    int st;
    gen_new_game();
    G.currLevel = (int16_t)level;
    G.noDarkLevel = 1;
    G.gameStart = 1;
    rng_seed(&g_rng, seed);
    plat_begin();
    st = gen_level(0);
    plat_end();
    plat_rec(job, KIND_GEN, 0, sum_gen(), (uint32_t)W.n, (uint32_t)(st != 0));
}

static void rec_cb(int phase)
{
    (void)phase;
    view_read();                                  /* playhost's record() reads the view */
}

/* a route: its masks, then `tail` steps without input; level, money and enemies as playhost's --level, --money,
   --enemies */
void run_route(int job, uint32_t seed, const struct route *rt, int tail)
{
    int k, r, idx = 0, n = rt->n;
    const uint16_t *masks = rt->masks;
    play_noenemy = !rt->enemies;
    gen_new_game();
    G.currLevel = (int16_t)rt->level;
    PG.plife = 4;
    PG.bombs = 4;
    PG.rope = 4;
    PG.money = rt->money;
    if (rt->nodark >= 0) G.noDarkLevel = (uint8_t)rt->nodark;   /* playhost --nodark (TRACE_NODARK: dark levels) */
    rng_seed(&g_rng, seed);
    snd_init(15, 15);                             /* as playhost (sndhost_init): src/snd's state at the start */
    plat_begin();
    play_level_start(110325);
    plat_end();
    plat_rec(job, KIND_START, idx++, sum_play(), (uint32_t)PW.n, play_dops);
    for (k = 0; k < n + tail; k++) {
        plat_begin();
        r = play_step(k < n ? masks[k] : 0, rec_cb);
        plat_end();
        snd_frame();                              /* as playhost: a frame of the sounds' time per step (not timed) */
        plat_rec(job, r == PLAY_ROOM_EARLY ? KIND_EARLY : KIND_STEP, idx++, sum_play(), (uint32_t)PW.n,
                 play_dops);
        if (r == PLAY_ROOM_EARLY) {
            k--;
            continue;
        }
        if (r != 0) break;
    }
}
