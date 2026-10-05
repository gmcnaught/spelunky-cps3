/* Untranslated-code survey (docs/CONTENT.md): plays levels from their start with random cabinet inputs and prints
 * every PUNTR(code) the play loop reaches (play.h PLAY_UNTR_LOG: all of them, not only a step's first).
 *
 *   build/host/untrsurvey [--levels A-B] [--seeds A-B] [--steps N] [--global name=value,...] [--jobs N] > out.txt
 *
 * One run per (level, seed): scrClearGlobals, level L, life / bombs / rope 4, noDarkLevel 0, the enemies kept,
 * rng_seed(seed), then N steps (default 1,500) of random inputs: the stick (left / right / none, up / down at
 * times), jump, whip, item, run, bomb, rope and pay (Start in play), each held for a random 1-30 steps. A run
 * goes on through the exit, the transition room and the next level; it ends at a room the play loop does not
 * enter. After the player's death the inputs stop (the level runs on, as on the cabinet's game-over panel). Each run is a forked child (no state carried between runs). Output, one line per distinct site per run:
 *   U <level> <seed> <code> <object>[/<other object> in a collision] <file>:<line> <first step>
 *   E <level> <seed> <steps done> <stop room or 0> <level at the end> <life at the end>
 *   H <level> <seed> step <k> object <name>: on stderr, a run stopped after 60 s (a loop in the play code)
 * --global: as playhost (tools/tracer.py TRACE_GLOBALS), e.g. pickupItem=Shotgun or hasJetpack=1.
 * --god 1: the developer option INVINCIBLE (play_god, as src/main/game.c: no life lost, no outright deaths), so
 *   a run explores longer (not the cabinet's play: its codes are reported apart).
 * The first level starts in the room the cabinet uses (gen_room_for_level), except with --route or --rlevel 1
 * (rLevel: the runs a route can reproduce in the runner).
 * --route FILE (one level, one seed): the run's inputs as a route (and the first level in rLevel, as the tracer) with its "# seed / level / nodark / globals"
 *   lines, for playhost and the HD runner (scripts/hd_trace.sh; content_traces.sh for tests/routes/c_*.txt).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <execinfo.h>
#include "pint.h"
#include "pcol.h"
#include "rng.h"
#include "../../src/snd/snd.h"

#define NSITE 256
static struct { int code, obj, other, line, step; const char *file; } site[NSITE];
static int nsite, cur_step, col_other = -1;

void play_untr_log(int code, const char *file, int line)
{
    int k;
    for (k = 0; k < nsite; k++)
        if (site[k].code == code && site[k].line == line && site[k].obj == play_cur_obj && site[k].other == col_other &&
            site[k].file == file) return;
    if (nsite == NSITE) return;
    site[nsite].code = code; site[nsite].obj = play_cur_obj; site[nsite].other = col_other; site[nsite].line = line;
    site[nsite].file = file; site[nsite].step = cur_step;
    nsite++;
}

/* pobj.c's dispatchers and ptrans.c's room start, renamed in this build (test/host/Makefile): the event's instance (and the collision's
   other one) for the log; play_cur_obj is not set there in the play loop */
void ev_collision_real(int self, int other);
void ev_outside_real(int i);
void ev_collision(int self, int other)
{
    int c = play_cur_obj;
    play_cur_obj = PX(self).obj;
    col_other = PX(other).obj;
    ev_collision_real(self, other);
    col_other = -1;
    play_cur_obj = c;
}
void play_transition_start_real(int room);
void play_transition_start(int room)            /* the transition room's loader and Creates: "-" */
{
    int c = play_cur_obj;
    play_cur_obj = -1;
    play_transition_start_real(room);
    play_cur_obj = c;
}
void ev_outside(int i)
{
    int c = play_cur_obj;
    play_cur_obj = PX(i).obj;
    ev_outside_real(i);
    play_cur_obj = c;
}

static int set_global(const char *kv)      /* playhost.c's set_global, the names the c_* routes use */
{
    char k[32], v[64];
    const char *e = strchr(kv, '=');
    int n, x;
    if (!e || e - kv >= (int)sizeof k) return 0;
    memcpy(k, kv, e - kv); k[e - kv] = 0;
    strncpy(v, e + 1, sizeof v - 1); v[sizeof v - 1] = 0;
    for (n = 0; v[n]; n++) if (v[n] == '~') v[n] = ' ';
    x = atoi(v);
    if (!strcmp(k, "pickupItem")) {
        for (n = 0; n < PICK_COUNT; n++)
            if (!strcmp(pickup_names[n], v)) { G.pickupItem = (uint8_t)n; return 1; }
        return 0;
    }
#define GI(name, dst) if (!strcmp(k, name)) { dst = x; return 1; }
    GI("madeMoai", G.madeMoai) GI("lake", G.lake) GI("kaliPunish", G.kaliPunish) GI("arrows", PG.arrows)
    GI("bombs", PG.bombs) GI("rope", PG.rope) GI("plife", PG.plife) GI("money", PG.money)
    GI("hasJetpack", PG.hasJetpack) GI("hasCape", PG.hasCape) GI("hasParachute", PG.hasParachute)
    GI("hasMitt", PG.hasMitt) GI("hasGloves", PG.hasGloves) GI("hasSpringShoes", PG.hasSpringShoes)
    GI("hasSpikeShoes", PG.hasSpikeShoes) GI("hasKapala", PG.hasKapala) GI("hasAnkh", PG.hasAnkh)
    GI("hasCompass", PG.hasCompass) GI("hasStickyBombs", PG.hasStickyBombs) GI("hasUdjatEye", PG.hasUdjatEye)
    GI("hasCrown", PG.hasCrown)
#undef GI
    return 0;
}

static uint32_t ks;                          /* the inputs' own generator (not g_rng) */
static uint32_t krand(uint32_t n) { ks = ks * 1664525u + 1013904223u; return (ks >> 8) % n; }

static uint16_t next_keys(void)
{
    uint16_t m = 0;
    uint32_t r = krand(10);
    if (r < 4) m |= K_RIGHT; else if (r < 8) m |= K_LEFT;
    r = krand(10);
    if (r == 0) m |= K_UP; else if (r == 1) m |= K_DOWN;
    if (krand(3) == 0) m |= K_JUMP;
    if (krand(4) == 0) m |= K_ATTACK;
    if (krand(12) == 0) m |= K_ITEM;
    if (krand(6) == 0) m |= K_RUN;
    if (krand(25) == 0) m |= K_BOMB;
    if (krand(30) == 0) m |= K_ROPE;
    if (krand(30) == 0) m |= K_PAY;
    return m;
}

/* --route FILE: the run's inputs as a route (tests/routes format, playhost / tools/tracer.py), one run only */
static FILE *route;
static uint16_t rt_m;
static int rt_n;
static void rt_flush(void)
{
    static const char letters[] = "RLUDJAINBOFPS";
    char b[16];
    int j, n = 0;
    if (!rt_n) return;
    for (j = 0; j < 13; j++) if (rt_m & (1u << j)) b[n++] = letters[j];
    if (!n) b[n++] = '-';
    b[n] = 0;
    fprintf(route, "%d %s\n", rt_n, b);
}
static void rt_key(uint16_t m)
{
    if (!route) return;
    if (rt_n && m != rt_m) { rt_flush(); rt_n = 0; }
    rt_m = m;
    rt_n++;
}

/* the record point: the tracer (and playhost) read the view there */
static void rec_cb(int phase) { (void)phase; view_read(); }

static int god, rlevel;                         /* --rlevel 1: the first level in rLevel (as --route) */                                  /* --god 1: the developer option INVINCIBLE (src/main/game.c) */

static void run(int level, int seed, int steps, const char *globals)
{
    int k, r = 0, hold = 0, done;
    uint16_t m = 0;
    scrClearGlobals();
    G.currLevel = level;
    PG.plife = 4;
    PG.bombs = 4;
    PG.rope = 4;
    G.noDarkLevel = 0;
    play_noenemy = 0;
    if (globals) {
        char b[256], *t;
        strncpy(b, globals, sizeof b - 1); b[sizeof b - 1] = 0;
        for (t = strtok(b, ","); t; t = strtok(NULL, ","))
            if (!set_global(t)) { fprintf(stderr, "untrsurvey: --global %s: not known\n", t); exit(2); }
    }
    play_god = (uint8_t)god;
    play_god_life = PG.plife;
    rng_seed(&g_rng, (uint32_t)seed);
    ks = (uint32_t)(level * 7919 + seed * 104729);
    snd_init(15, 15);
    cur_step = -1;
    /* a route: the first level in rLevel, as the tracer's room_goto(TRACE_ROOM) and playhost (an ice level is
       rLevel2 and a lake level rLevel3 on the cabinet, src/main/game.c) */
    gen_room_force = route || rlevel ? 0 : -1;
    play_level_start(110325);
    gen_room_force = -1;
    for (k = 0; k < steps; k++) {
        if (hold-- <= 0) { m = next_keys(); hold = (int)krand(30); }
        cur_step = k;
        /* a dead player: no input, so the level runs on (the cabinet's game-over panel waits 900 steps) */
        {
            uint16_t km = PL.idx != NOONE && PL.dead ? 0 : m;
            do {                                   /* PLAY_ROOM_EARLY: the same input again (playhost, game.c) */
                r = play_step(km, rec_cb);
                if (play_god) play_god_hold();
                play_untranslated = 0;
                snd_frame();
            } while (r == PLAY_ROOM_EARLY);
            rt_key(km);
        }
        if (r != 0) break;
    }
    done = k;
    if (route) {
        rt_flush();
        fclose(route);
    }
    for (k = 0; k < nsite; k++) {
        const char *f = strrchr(site[k].file, '/');
        printf("U %d %d %d %s%s%s %s:%d %d\n", level, seed, site[k].code, site[k].obj >= 0 ? objdefs[site[k].obj].name : "-",
               site[k].other >= 0 ? "/" : "", site[k].other >= 0 ? objdefs[site[k].other].name : "",
               f ? f + 1 : site[k].file, site[k].line, site[k].step);
    }
    printf("E %d %d %d %d %d %d\n", level, seed, done, r > 0 ? r : 0, G.currLevel, PG.plife);
    fflush(stdout);
}

static void reap(void)     /* a run that fails has no E line (scripts/untr_survey.sh lists it) */
{
    int st;
    if (wait(&st) > 0 && (!WIFEXITED(st) || WEXITSTATUS(st))) fprintf(stderr, "untrsurvey: a run failed (status %d)\n", st);
}

/* a run that does not finish in 60 s (a loop in the play code): its step and the event's object on stderr */
static int run_level, run_seed;
static void hung(int sig)
{
    char b[160];
    int n = snprintf(b, sizeof b, "H %d %d step %d object %s: no end after 60 s\n", run_level, run_seed, cur_step,
                     play_cur_obj >= 0 ? objdefs[play_cur_obj].name : "-");
    (void)sig;
    write(2, b, (size_t)n);
    {
        void *bt[32];
        backtrace_symbols_fd(bt, backtrace(bt, 32), 2);
    }
    _exit(3);
}

static int range(const char *s, int *a, int *b)
{
    return sscanf(s, "%d-%d", a, b) == 2 || (sscanf(s, "%d", a) == 1 && (*b = *a, 1));
}

int main(int argc, char **argv)
{
    int l0 = 1, l1 = 16, s0 = 1, s1 = 30, steps = 1500, jobs = 8, running = 0, k, l, s;
    const char *globals = NULL, *route_path = NULL;
    for (k = 1; k + 1 < argc; k += 2) {
        if (!strcmp(argv[k], "--levels")) range(argv[k + 1], &l0, &l1);
        else if (!strcmp(argv[k], "--seeds")) range(argv[k + 1], &s0, &s1);
        else if (!strcmp(argv[k], "--steps")) steps = atoi(argv[k + 1]);
        else if (!strcmp(argv[k], "--global")) globals = argv[k + 1];
        else if (!strcmp(argv[k], "--jobs")) jobs = atoi(argv[k + 1]);
        else if (!strcmp(argv[k], "--route")) route_path = argv[k + 1];
        else if (!strcmp(argv[k], "--god")) god = atoi(argv[k + 1]);
        else if (!strcmp(argv[k], "--rlevel")) rlevel = atoi(argv[k + 1]);
        else { fprintf(stderr, "untrsurvey: %s: not known\n", argv[k]); return 2; }
    }
    if (route_path && god) { fprintf(stderr, "untrsurvey: --route with --god: the runner has no INVINCIBLE\n"); return 2; }
    if (route_path) {
        if (l0 != l1 || s0 != s1) { fprintf(stderr, "untrsurvey: --route needs one level and one seed\n"); return 2; }
        route = fopen(route_path, "w");
        if (!route) { perror(route_path); return 2; }
        fprintf(route, "# untrsurvey run: level %d seed %d, %d steps of random inputs (none after the player's death)\n"
                "# seed %d\n# level %d\n# nodark 0\n", l0, s0, steps, s0, l0);
        if (globals) fprintf(route, "# globals %s\n", globals);
        fflush(route);                           /* not again from the parent's buffer at its exit */
    }
    fflush(stdout);
    for (l = l0; l <= l1; l++)
        for (s = s0; s <= s1; s++) {
            pid_t p;
            if (running >= jobs) { reap(); running--; }
            p = fork();
            if (p < 0) { perror("fork"); return 1; }
            if (p == 0) {
                /* a run's lines go out in one write (the children share stdout) */
                static char buf[1 << 16];
                setvbuf(stdout, buf, _IOFBF, sizeof buf);
                run_level = l; run_seed = s;
                signal(SIGALRM, hung);
                alarm(60);
                run(l, s, steps, globals);
                _exit(0);
            }
            running++;
        }
    while (running > 0) { reap(); running--; }
    return 0;
}
