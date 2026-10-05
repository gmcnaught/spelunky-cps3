/* olmecbot: a player for rOlmec (level 16) on the host's exact play loop (src/game, PCOL_EXACT, as playhost), which
 * writes the keys it pressed as a route (tests/routes/end_olmec.txt; built and run by scripts/olmecbot.sh). Olmec only
 * digs where he slams, and he slams when the player is below him and within 32 px of his middle while he is in the
 * air (oOlmec Step :120): each of his cycles the player stands at a target x and runs out of his columns when he
 * prepares the slam; the target is chosen by trying them all (fork(), two cycles ahead: Olmec deepest, the player
 * alive with most life). Once he drowns, a best-first search over 8-frame key macros (fork() per node; nodes kept
 * apart by place, state, life and the door) takes the player out of the pit to oXEnd (640, 544), then up.
 *   olmecbot <seed> <out route> [--depth <cycles ahead>] [--dstep / --range <target x step / range about Olmec's
 *            middle>] [--maxn <steps a cycle>] [--hm <macro frames>] [--life <global.plife>]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <math.h>
#include <time.h>
#include "pint.h"
#include "pcol.h"
#include "rng.h"
#include "snd.h"
void sndhost_init(void);

static uint16_t klog[200000];
static int nlog;
static void nocb(int phase) { (void)phase; view_read(); }   /* as playhost / the tracer: the view read at each record */

static int olm(void) { return instance_first_p(OBJ_oOlmec); }
static double pxv(void) { return PL.idx >= 0 ? PTOD(PX(PL.idx).x) : -1; }
static double pyv(void) { return PL.idx >= 0 ? PTOD(PX(PL.idx).y) : -1; }
static double oxv(void) { int o = olm(); return o >= 0 && o != NOONE ? PTOD(PX(o).x) : -1; }
static double oyv(void) { int o = olm(); return o >= 0 && o != NOONE ? PTOD(PX(o).y) : 9999; }
static int ost(void) { int o = olm(); return o >= 0 && o != NOONE ? PE(&PX(o))->status : 99; }

static int room_left;
static int step(uint16_t k)
{
    int r;
    do r = play_step(k, nocb); while (r == PLAY_ROOM_EARLY);
    snd_frame();
    if (nlog < 200000) klog[nlog++] = k;
    if (r) room_left = r;
    return r;
}

/* low-level: walk / run toward x; jump when stuck */
static double lastx = -1;
static int stuck, jumping;
static uint16_t toward(double X, int run)
{
    double x = pxv();
    uint16_t k = 0;
    if (x < X - 1.5) k = K_RIGHT;
    else if (x > X + 1.5) k = K_LEFT;
    if (k && fabs(x - lastx) < 0.3) stuck++; else stuck = 0;
    lastx = x;
    if (jumping > 0) { jumping--; k |= K_JUMP; }
    else if (stuck > 3) { jumping = 12; stuck = 0; k |= K_JUMP; }
    if (run && k) k |= K_RUN;
    return k;
}

/* one Olmec cycle with target X: stand at X until Olmec prepares a slam, then run out of his columns; ends when he
   is on the ground again after the slam (or after maxn steps). Returns steps run, -1 if the room was left */
enum { OL_IDLE = 0, OL_BOUNCE = 1, OL_RECOVER = 2, OL_DROWNING = 4, OL_PREPARE = 5, OL_SLAM = 6, OL_CREATE = 7 };
static int cycle(double X, int esc_dir, int maxn)
{
    int n = 0, seen_slam = 0;
    while (n < maxn) {
        int s = ost();
        uint16_t k;
        if (PL.dead) return n;
        if (s == OL_DROWNING || s == 99) return n;
        if (s == OL_PREPARE || s == OL_SLAM) {
            double ox = oxv();
            double ex = esc_dir < 0 ? ox - 20 : ox + 64 + 20;
            seen_slam = 1;
            k = toward(ex, 1);
        } else {
            if (seen_slam && (s == OL_IDLE || s == OL_CREATE)) return n;
            k = toward(X, 0);
        }
        if (step(k)) return -1;
        n++;
    }
    return n;
}

static double leaf(int n)
{
    return oyv() * 10 + PG.plife * 50 - (PL.dead ? 1e9 : 0) + ((ost() == OL_DROWNING || G.olmecDead) ? 1e7 : 0)
           - (n < 0 ? 1e9 : 0);
}

static int DSTEP = 8, DRANGE = 112, DEPTH = 2;
/* the best cycle from here, looking depth cycles ahead: its score (bx, be: the first cycle's target and escape).
   The candidates run in up to npar child processes at a time */
static int MAXN = 400;
static double search(int depth, double *bx, int *be, int npar)
{
    double ox = oxv(), best = -1e18;
    double cx[64];
    int ce[64], nc = 0, d, e, k;
    pid_t pid[64];
    int fd[64];
    for (d = -DRANGE; d <= DRANGE; d += DSTEP)
        for (e = -1; e <= 1; e += 2) {
            double X = ox + 32 + d;
            if (X < 24 || X > 648 || nc >= 64) continue;
            cx[nc] = X;
            ce[nc++] = e;
        }
    for (k = 0; k < nc; k++) {
        int p2[2];
        if (k >= npar) {                          /* wait for the oldest running one */
            double sc;
            int j = k - npar;
            if (read(fd[j], &sc, sizeof sc) != sizeof sc) { fprintf(stderr, "child failed\n"); exit(3); }
            close(fd[j]);
            waitpid(pid[j], 0, 0);
            if (sc > best) { best = sc; *bx = cx[j]; *be = ce[j]; }
        }
        if (pipe(p2)) exit(3);
        pid[k] = fork();
        if (pid[k] == 0) {
            int n, j;
            double sc;
            close(p2[0]);
            n = cycle(cx[k], ce[k], MAXN);
            if (n >= 0 && n < MAXN && !PL.dead && ost() != OL_DROWNING && !G.olmecDead && depth > 1) {
                double x2;
                int e2;
                sc = search(depth - 1, &x2, &e2, 1);
            } else {
                for (j = 0; j < 60 && n >= 0 && !PL.dead; j++) if (step(0)) break;
                sc = leaf(n) - (n >= MAXN ? 1e6 : 0);
            }
            if (write(p2[1], &sc, sizeof sc) != sizeof sc) _exit(4);
            _exit(0);
        }
        close(p2[1]);
        fd[k] = p2[0];
    }
    for (k = nc - npar < 0 ? 0 : nc - npar; k < nc; k++) {
        double sc;
        if (read(fd[k], &sc, sizeof sc) != sizeof sc) { fprintf(stderr, "child failed\n"); exit(3); }
        close(fd[k]);
        waitpid(pid[k], 0, 0);
        if (sc > best) { best = sc; *bx = cx[k]; *be = ce[k]; }
    }
    return best;
}

/* a macro: keys for HM frames, the pressed-edge keys (jump, rope, bomb, attack) released on the last */
static int HM = 8;
static void macro(uint16_t k)
{
    int j;
    for (j = 0; j < HM && !room_left && !PL.dead; j++)
        step(j == HM - 1 ? (uint16_t)(k & ~(K_JUMP | K_ROPE | K_BOMB | K_ATTACK)) : k);
}
static double score2(void)
{
    double x = pxv(), y = pyv(), sc;
    if (room_left) return 1e9;
    if (PL.dead) return -1e9;
    if (G.doorOpen && y <= 560) return 1e6 - fabs(x - 648) * 10 + PG.plife * 2000;
    sc = PG.plife * 2000 - fabs(y - 552) * 10 - fabs(x - 648) * (y <= 560 ? 1.0 : 0.05);
    return sc;
}

/* the room's cells (in a child process: collision queries flush the tree's dirty list, which the parent must not
   do outside the game's own order) */
static void dump(const char *why)
{
    pid_t pid;
    fflush(stderr);
    pid = fork();
    if (pid == 0) {
        int cx, cy, k;
        char g[55][43];
        for (cy = 0; cy < 55; cy++) {
            for (cx = 0; cx < 42; cx++) {
                double x = cx * 16 + 8, y = cy * 16 + 8;
                g[cy][cx] = collision_point_p(x, y, OBJ_oLava, 0, -1) != NOONE ? '~' :
                            collision_point_p(x, y, OBJ_oSolid, 0, -1) != NOONE ? '#' : '.';
            }
            g[cy][42] = 0;
        }
        for (k = 0; k < PW.n; k++) {
            int c;
            if (!PX(k).alive) continue;
            cx = (int)(PTOD(PX(k).x) / 16);
            cy = (int)(PTOD(PX(k).y) / 16);
            if (cx < 0 || cx >= 42 || cy < 0 || cy >= 55) continue;
            c = PX(k).obj == OBJ_oYellowBall ? 'y' : PX(k).obj == OBJ_oHawkman ? 'h' : PX(k).obj == OBJ_oRope ? '|' :
                PX(k).obj == OBJ_oXEnd ? 'X' : 0;
            if (PX(k).obj == OBJ_oOlmec) {
                int i, j;
                for (j = 0; j < 4; j++) for (i = 0; i < 4; i++) if (cy + j < 55 && cx + i < 42) g[cy + j][cx + i] = 'O';
            } else if (c) g[cy][cx] = (char)c;
        }
        if (PL.idx >= 0) {
            cx = (int)(pxv() / 16); cy = (int)(pyv() / 16);
            if (cx >= 0 && cx < 42 && cy >= 0 && cy < 55) g[cy][cx] = '@';
        }
        fprintf(stderr, "map (%s) at step %d: player %.1f,%.1f life %d; olmec %.1f,%.1f status %d\n", why, nlog, pxv(),
                pyv(), PG.plife, oxv(), oyv(), ost());
        for (cy = 25; cy < 55; cy++) fprintf(stderr, "%2d %4d %s\n", cy, cy * 16, g[cy]);
        _exit(0);
    }
    waitpid(pid, 0, 0);
}

int main(int argc, char **argv)
{
    long seed = atol(argv[1]);
    const char *out = argv[2];
    int k, cyc = 0;
    FILE *f;
    if (!freopen("/dev/null", "w", stdout)) return 2;
    scrClearGlobals();
    G.currLevel = 16;
    PG.plife = 4; PG.bombs = 4; PG.rope = 4; PG.money = 0;
    for (k = 3; k + 1 < argc; k++)
        if (!strcmp(argv[k], "--life")) PG.plife = atoi(argv[++k]);
        else if (!strcmp(argv[k], "--depth")) DEPTH = atoi(argv[++k]);
        else if (!strcmp(argv[k], "--maxn")) MAXN = atoi(argv[++k]);
        else if (!strcmp(argv[k], "--hm")) HM = atoi(argv[++k]);
        else if (!strcmp(argv[k], "--dstep")) DSTEP = atoi(argv[++k]);
        else if (!strcmp(argv[k], "--range")) DRANGE = atoi(argv[++k]);
    play_noenemy = 0;
    rng_seed(&g_rng, (uint32_t)seed);
    sndhost_init();
    gen_room_force = 3;
    play_level_start(110325);
    gen_room_force = -1;
    /* the intro: until Olmec is idle the first time */
    while (ost() != OL_IDLE && nlog < 2000) step(0);
    fprintf(stderr, "intro done at %d: olmec %.1f,%.1f player %.1f,%.1f\n", nlog, oxv(), oyv(), pxv(), pyv());
    while (!G.olmecDead && ost() != OL_DROWNING && cyc < 60 && !PL.dead) {
        double best, bx = 0;
        int be = 1;
        best = search(DEPTH, &bx, &be, 7);
        if (play_untranslated) fprintf(stderr, "UNTRANSLATED %d (%s) by step %d\n", play_untranslated,
                                       play_untr_obj >= 0 ? objdefs[play_untr_obj].name : "-", nlog);
        fprintf(stderr, "[%ld s] ", (long)time(0) % 100000);
        fprintf(stderr, "cycle %d at step %d: olmec %.1f,%.1f status %d, player %.1f,%.1f life %d -> X %.1f esc %d (score %.1f)\n",
                cyc, nlog, oxv(), oyv(), ost(), pxv(), pyv(), PG.plife, bx, be, best);
        if (best < -1e8) { fprintf(stderr, "no safe cycle\n"); break; }
        if (cycle(bx, be, MAXN) < 0) break;
        cyc++;
    }
    dump("olmec cycles done");
    fprintf(stderr, "olmec: dead %d status %d y %.1f at step %d, life %d, player dead %d\n", G.olmecDead, ost(), oyv(), nlog,
            PG.plife, PL.dead);
    /* out of the pit and to the door (640, 544), then up: best-first search over macros (HM frames of one key set),
       each node replayed from here in a child, its children's results from grandchildren; nodes deduplicated by the
       player's place (4 px), state and life */
    {
        static const uint16_t ACT[] = { 0, K_LEFT, K_RIGHT, K_UP, K_DOWN, K_JUMP, K_LEFT | K_JUMP, K_RIGHT | K_JUMP,
                                        K_LEFT | K_RUN, K_RIGHT | K_RUN, K_ROPE, K_UP | K_JUMP, K_LEFT | K_RUN | K_JUMP,
                                        K_RIGHT | K_RUN | K_JUMP, K_UP | K_LEFT, K_UP | K_RIGHT };
        enum { NACT = sizeof ACT / sizeof ACT[0], NMAX = 60000, PMAX = 120 };
        struct node { int parent; uint8_t act, done; int16_t len; double sc; };
        struct rr { double sc, x, y; int life, dead, left, st, ok, door; };
        static struct node nd[NMAX];
        static uint32_t seen[1 << 20];
        int nn = 1, goal = -1, iter = 0;
        nd[0].parent = -1; nd[0].len = 0; nd[0].done = 0; nd[0].sc = score2();
        while (goal < 0 && iter < 4000) {
            int pick[7], np = 0, k, fd[7];
            pid_t pid[7];
            /* the best unexpanded nodes */
            for (k = 0; k < 7; k++) {
                int i, bi = -1;
                for (i = 0; i < nn; i++)
                    if (!nd[i].done && nd[i].len < PMAX && (bi < 0 || nd[i].sc > nd[bi].sc)) bi = i;
                if (bi < 0) break;
                nd[bi].done = 1;
                pick[np++] = bi;
            }
            if (!np) { fprintf(stderr, "search exhausted at %d nodes\n", nn); break; }
            for (k = 0; k < np; k++) {
                int p2[2];
                if (pipe(p2)) exit(3);
                pid[k] = fork();
                if (pid[k] == 0) {
                    uint8_t path[PMAX];
                    int L = nd[pick[k]].len, i, j, a1;
                    struct rr out[NACT];
                    close(p2[0]);
                    for (i = pick[k], j = L - 1; i > 0; i = nd[i].parent) path[j--] = nd[i].act;
                    for (j = 0; j < L; j++) macro(ACT[path[j]]);
                    for (a1 = 0; a1 < NACT; a1++) {
                        int p3[2];
                        pid_t q;
                        if (pipe(p3)) _exit(5);
                        q = fork();
                        if (q == 0) {
                            struct rr r;
                            close(p3[0]);
                            macro(ACT[a1]);
                            r.sc = score2(); r.x = pxv(); r.y = pyv(); r.life = PG.plife; r.dead = PL.dead;
                            r.left = room_left != 0; r.st = PL.state; r.ok = 1; r.door = G.doorOpen;
                            if (write(p3[1], &r, sizeof r) != sizeof r) _exit(4);
                            _exit(0);
                        }
                        close(p3[1]);
                        if (read(p3[0], &out[a1], sizeof out[a1]) != sizeof out[a1]) out[a1].ok = 0;
                        close(p3[0]);
                        waitpid(q, 0, 0);
                    }
                    if (write(p2[1], out, sizeof out) != sizeof out) _exit(4);
                    _exit(0);
                }
                close(p2[1]);
                fd[k] = p2[0];
            }
            for (k = 0; k < np; k++) {
                struct rr out[NACT];
                int a1;
                if (read(fd[k], out, sizeof out) != sizeof out) { fprintf(stderr, "child failed\n"); exit(3); }
                close(fd[k]);
                waitpid(pid[k], 0, 0);
                for (a1 = 0; a1 < NACT; a1++) {
                    uint32_t h;
                    if (!out[a1].ok || out[a1].dead) continue;
                    h = (uint32_t)((int)(out[a1].x / 4) * 7919 + (int)(out[a1].y / 4) * 104729 + out[a1].st * 31 +
                                   out[a1].life * 1299709 + out[a1].door * 15485863) & ((1u << 25) - 1);
                    if (!out[a1].left && (seen[h >> 5] >> (h & 31) & 1)) continue;
                    seen[h >> 5] |= 1u << (h & 31);
                    if (nn >= NMAX) continue;
                    nd[nn].parent = pick[k]; nd[nn].act = (uint8_t)a1; nd[nn].len = (int16_t)(nd[pick[k]].len + 1);
                    nd[nn].done = 0; nd[nn].sc = out[a1].sc;
                    if (out[a1].left) goal = nn;
                    nn++;
                }
            }
            if (++iter % 50 == 0) {
                int i, bi = 0;
                for (i = 0; i < nn; i++) if (nd[i].sc > nd[bi].sc) bi = i;
                fprintf(stderr, "search iter %d: %d nodes, best score %.1f (len %d)\n", iter, nn, nd[bi].sc, nd[bi].len);
            }
        }
        if (goal < 0) {                           /* the best node, then up at the door */
            int i;
            goal = 0;
            for (i = 0; i < nn; i++) if (nd[i].sc > nd[goal].sc) goal = i;
            fprintf(stderr, "no goal: best node %d score %.1f\n", goal, nd[goal].sc);
        }
        if (goal >= 0) {
            uint8_t path[PMAX];
            int i, j, L = nd[goal].len;
            for (i = goal, j = L - 1; i > 0; i = nd[i].parent) path[j--] = nd[i].act;
            for (j = 0; j < L; j++) macro(ACT[path[j]]);
        }
        {
            int j;
            for (j = 0; j < 60 && !room_left; j++) {
                step(j % 4 < 2 ? K_UP : 0);
                if (j % 10 == 0) fprintf(stderr, "  up %d: player %.1f,%.1f state %d active %d xend %d\n", j, pxv(), pyv(),
                                         PL.state, PL.idx >= 0 ? (int)PX(PL.idx).visible : -1, instance_first_p(OBJ_oXEnd));
            }
        }
        fprintf(stderr, "room left %d at step %d; player %.1f,%.1f dead %d life %d untranslated %d\n", room_left, nlog,
                pxv(), pyv(), PL.dead, PG.plife, play_untranslated);
    }
    f = fopen(out, "w");
    fprintf(f, "# Olmec beaten (scripts/olmecbot.sh, seed %ld): he drowns, the player goes in at oXEnd (rEnd at step %d);\n"
               "# scripts/olmec_host.sh: the game program on the host against the runner's trace build/trace/end_olmec_s1\n"
               "# seed %ld\n# level 16\n# room rOlmec\n", seed, nlog, seed);
    for (k = 0; k < nlog;) {
        int j = k;
        char b[16];
        int o = 0;
        while (j < nlog && klog[j] == klog[k]) j++;
        if (!klog[k]) b[o++] = '-';
        if (klog[k] & K_RIGHT) b[o++] = 'R';
        if (klog[k] & K_LEFT) b[o++] = 'L';
        if (klog[k] & K_UP) b[o++] = 'U';
        if (klog[k] & K_DOWN) b[o++] = 'D';
        if (klog[k] & K_JUMP) b[o++] = 'J';
        if (klog[k] & K_RUN) b[o++] = 'N';
        if (klog[k] & K_ATTACK) b[o++] = 'A';
        if (klog[k] & K_ITEM) b[o++] = 'I';
        if (klog[k] & K_BOMB) b[o++] = 'B';
        if (klog[k] & K_ROPE) b[o++] = 'O';
        if (klog[k] & K_FLARE) b[o++] = 'F';
        if (klog[k] & K_PAY) b[o++] = 'P';
        if (klog[k] & K_START) b[o++] = 'S';
        b[o] = 0;
        fprintf(f, "%d %s\n", j - k, b);
        k = j;
    }
    fclose(f);
    return 0;
}
