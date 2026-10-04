/* Host harness for the play loop: plays a route from a generated level and prints a record at each point where
 * tools/tracer.py's runner writes one (phase 0: the room's first Begin Step; phase 1: oGamepad's End Step), for
 * tools/playcmp.py.
 *
 *   build/host/playhost <route.txt> <seed> [--nextid N] [--tail N] [--level N] [--money M] [--enemies] > out.txt
 *   --enemies: keep the enemies (P5 references, no TRACE_NOENEMY); --level: start on level N (TRACE_LEVEL)
 *
 * Output per record:
 *   R <rec> <phase> <t> <room> <level> <life> <bombs> <rope> <money> <xview> <yview> <time> <untranslated> <dops>
 *   I <id> <object> <x> <y> <sprite|-> <image_index> <xscale> <yscale> <angle> <alpha> <depth> <visible>
 *     <alarms k=v,...|-> <xVel> <yVel> <image_speed> [name=value ...]
 * Doubles as %.17g. oGamepad (the tracer's instance) is left out.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pint.h"
#include "pcol.h"

static int rec, t_done;

static void pd(double v) { printf(" %.17g", v); }

static void record(int phase)
{
    if (phase == 1) t_done++;
    int k;
    view_read();                                   /* the tracer reads camera_get_view_x / y */
    printf("R %d %d %d %d %d %d %d %d %d %d %d %ld %d %u %s\n", rec++, phase, t_done, PW.room, G.currLevel, PG.plife,
           PG.bombs, PG.rope, PG.money, PW.xview, PW.yview,
           instance_exists_p(OBJ_oGame) ? (long)play_time : -1000000000L, play_untranslated, play_dops,
           play_untr_obj >= 0 ? objdefs[play_untr_obj].name : "-");
    for (k = PW.n - 1; k >= 0; k--) {
        const struct pin *p = &PW.in[k];
        int a, any = 0;
        if (!p->alive || p->obj == OBJ_oGamepad) continue;
        printf("I %ld %s", (long)p->id, objdefs[p->obj].name);
        pd(PTOD(p->x));
        pd(PTOD(p->y));
        /* oYellHelp's sprite is sprite_add'ed at run time (global.sYellHelpNew): no name in the trace */
        printf(" %s", p->spr >= 0 && p->obj != OBJ_oYellHelp ? gsprname[p->spr] : "-");
        pd((double)p->img);
        pd(p->xscale);
        pd(p->yscale);
        pd(p->angle);
        pd(PE(p)->alpha);
        pd((double)p->depth);
        printf(" %d ", p->visible);
        for (a = 0; a < 12; a++)
            if (PE(p)->alarm[a] != -1) {
                printf("%s%d=%d", any ? "," : "", a, PE(p)->alarm[a]);
                any = 1;
            }
        if (!any) printf("-");
        pd(NTOD(PE(p)->xVel));
        pd(NTOD(PE(p)->yVel));
        pd((double)p->ispd);
        if (k == PL.idx) {
            printf(" state=%d xAcc=%.17g yAcc=%.17g fallTimer=%d stunTimer=%d dead=%d stunned=%d jumpTime=%d"
                   " whipping=%d hangCount=%d ladderTimer=%d pushTimer=%d runHeld=%d holdItem=%ld grav=%.17g"
                   " gravityIntensity=%.17g bounced=%d invincible=%d facing=%d myGrav=%.17g kJumped=%d"
                   " jumpButtonReleased=%d",
                   PL.state, NTOD(PE(p)->xAcc), NTOD(PE(p)->yAcc), PL.fallTimer, PL.stunTimer, PL.dead, PL.stunned,
                   PL.jumpTime, PL.whipping, PL.hangCount, PL.ladderTimer, PL.pushTimer, PL.runHeld,
                   PL.holdItem == NOONE ? 0L : (long)PX(PL.holdItem).id, NTOD(PE(p)->grav), NTOD(PL.gravityIntensity),
                   PL.bounced, PL.invincible, PL.facing, NTOD(PE(p)->myGrav), PL.kJumped, PL.jumpButtonReleased);
        } else if (obj_is(p->obj, OBJ_oItem)) {
            printf(" held=%d armed=%d safe=%d cost=%ld trigger=%d myGrav=%.17g", PE(p)->held, PE(p)->armed, PE(p)->safe,
                   (long)PE(p)->cost, PE(p)->trigger, NTOD(PE(p)->myGrav));
            if (p->obj == OBJ_oDamsel)                     /* P5 */
                printf(" status=%d counter=%d facing=%d bounced=%d dead=%d", PE(p)->status, PE(p)->counter, PE(p)->facing,
                       PEN(p)->bounced, PEN(p)->edead);
        } else if (obj_is(p->obj, OBJ_oEnemy)) {           /* P5: the variables the trace has (when they exist) */
            printf(" status=%d counter=%d facing=%d held=%d invincible=%d myGrav=%.17g bounced=%d dead=%d"
                   " xAcc=%.17g yAcc=%.17g cost=%ld", PE(p)->status, PE(p)->counter, PE(p)->facing, PE(p)->held, p->invincible,
                   NTOD(PE(p)->myGrav), PEN(p)->bounced, PEN(p)->edead, NTOD(PE(p)->xAcc), NTOD(PE(p)->yAcc), (long)PE(p)->cost);
        } else if (obj_is(p->obj, OBJ_oTreasure)) {
            printf(" held=%d state=%d value=%ld trigger=%d myGrav=%.17g", PE(p)->held, PE(p)->state, (long)PE(p)->value,
                   PE(p)->trigger, NTOD(PE(p)->myGrav));
        } else if (obj_is(p->obj, OBJ_oDetritus)) {
            printf(" life=%.17g grav=%.17g invincible=%d", NTOD(PE(p)->life), NTOD(PE(p)->grav), p->invincible);
        } else if (p->obj == OBJ_oWeb) {
            printf(" life=%.17g", NTOD(PE(p)->life));
        }
        printf("\n");
    }
}

/* PCOL_TREE=r1,r2,...: after record r, "TREE <r> <n> <id> ..." on stderr: the oSolid instances in the collision
   tree's search order, as tools/tracer.py TRACE_TREE writes them (the query flushes the dirty list as there) */
static void tree_probe(int r)
{
    const char *s = getenv("PCOL_TREE");
    static int32_t ids[PIN_MAX];
    int n, k;
    while (s && *s) {
        if (atoi(s) == r) {
            const char *on = getenv("PCOL_TREE_OBJ");
            int o = OBJ_oSolid, j;
            for (j = 0; on && j < OBJ_COUNT; j++)
                if (!strcmp(objdefs[j].name, on)) o = j;
            n = pcol_probe(o, ids, PIN_MAX);
            fprintf(stderr, "TREE %d %d", r, n);
            for (k = 0; k < n; k++) fprintf(stderr, " %ld", (long)ids[k]);
            fprintf(stderr, "\n");
            return;
        }
        while (*s && *s != ',') s++;
        if (*s == ',') s++;
    }
}

static void rec_cb(int phase)
{
    record(phase);
    tree_probe(rec - 1);
#ifdef NUM_IS_CLASS
    {   /* binary64 operations since the previous record (playhost_count) */
        static struct dcount last;
        fprintf(stderr, "D %d %llu %llu %llu %llu %llu\n", rec - 1, play_dcount.add - last.add, play_dcount.mul - last.mul,
                play_dcount.div - last.div, play_dcount.cmp - last.cmp, play_dcount.conv - last.conv);
        last = play_dcount;
    }
#endif
}

static uint32_t cmax[6], csteps;
#ifdef PLAY_STATS
#define NPW ((int)(sizeof(struct pw_stats) / 4))
static uint32_t pwmax[NPW];
static uint64_t pwsum[NPW];
static const char *const pwname[NPW] = { "bbox", "bbox_int", "visit", "point", "line", "rect", "place", "exists",
                                         "with", "dist", "nearest", "snap", "snapv", "anim" };
#endif

int main(int argc, char **argv)
{
    FILE *f;
    char line[256];
    static uint16_t masks[100000];
    int nsteps = 0, k, tail = 30, r;
    long seed;
    int32_t nextid = 110325;
    int level = 1, money = 0;
    if (argc < 3) {
        fprintf(stderr, "usage: playhost <route.txt> <seed> [--nextid N] [--tail N]\n");
        return 2;
    }
    seed = atol(argv[2]);
    for (k = 3; k + 1 < argc; k++) {
        if (!strcmp(argv[k], "--nextid")) nextid = atol(argv[++k]);
        else if (!strcmp(argv[k], "--tail")) tail = atoi(argv[++k]);
        else if (!strcmp(argv[k], "--level")) level = atoi(argv[++k]);
        else if (!strcmp(argv[k], "--money")) money = atoi(argv[++k]);
    }
    for (k = 3; k < argc; k++)
        if (!strcmp(argv[k], "--enemies")) play_noenemy = 0;
    f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 2; }
    while (fgets(line, sizeof line, f)) {
        char *h = strchr(line, '#'), keys[64] = "-";
        int n;
        uint16_t m = 0;
        if (h) *h = 0;
        if (sscanf(line, "%d %63s", &n, keys) < 1) continue;
        for (h = keys; *h; h++)
            switch (*h) {
            case 'R': m |= K_RIGHT; break;   case 'L': m |= K_LEFT; break;    case 'U': m |= K_UP; break;
            case 'D': m |= K_DOWN; break;    case 'J': m |= K_JUMP; break;    case 'A': m |= K_ATTACK; break;
            case 'I': m |= K_ITEM; break;    case 'N': m |= K_RUN; break;     case 'B': m |= K_BOMB; break;
            case 'O': m |= K_ROPE; break;    case 'F': m |= K_FLARE; break;   case 'P': m |= K_PAY; break;
            case 'S': m |= K_START; break;
            }
        while (n-- > 0 && nsteps < 100000) masks[nsteps++] = m;
    }
    fclose(f);
    gen_new_game();
    G.currLevel = level;
    PG.plife = 4;
    PG.bombs = 4;
    PG.rope = 4;
    PG.money = money;
    rng_seed(&g_rng, (uint32_t)seed);
    play_level_start(nextid);
    for (k = 0; k < nsteps + tail; k++) {
        t_done = k;                                /* the phase-0 record comes before the step's input */
        {   /* collision tree cost (pcol.c): per-step totals and maxima, printed at the end */
            struct pcol_stats b = pcol_st;
#ifdef PLAY_STATS
            struct pw_stats w0 = pw_st;
#endif
            r = play_step(k < nsteps ? masks[k] : 0, rec_cb);
#ifdef PLAY_STATS
            {
                const uint32_t *a = (const uint32_t *)&w0, *c = (const uint32_t *)&pw_st;
                int j;
                for (j = 0; j < NPW; j++) {
                    uint32_t d = c[j] - a[j];
                    pwsum[j] += d;
                    if (d > pwmax[j]) pwmax[j] = d;
                }
            }
#endif
            {
                uint32_t ir = (pcol_st.inserts - b.inserts) + (pcol_st.removes - b.removes);
                uint32_t v = pcol_st.visits - b.visits, sy = pcol_st.syncs - b.syncs;
                uint32_t in = pcol_st.inserts - b.inserts, rm = pcol_st.removes - b.removes;
                uint32_t se = pcol_st.searches - b.searches;
                if (ir > cmax[0]) cmax[0] = ir;
                if (v > cmax[1]) cmax[1] = v;
                if (sy > cmax[2]) cmax[2] = sy;
                if (in > cmax[3]) cmax[3] = in;
                if (rm > cmax[4]) cmax[4] = rm;
                if (se > cmax[5]) cmax[5] = se;
                csteps++;
            }
        }
        if (r == PLAY_ROOM_EARLY) {                /* the frame ended before oGamepad's Step: same input again */
            k--;
            continue;
        }
        if (r != 0) {
            fprintf(stderr, "step %d: room_goto(%d): not modelled\n", k, r);
            break;
        }
    }
    fprintf(stderr, "PCOL steps %lu inserts %lu removes %lu searches %lu node_visits %lu full_syncs %lu flushes %lu "
            "nodes_max %lu | per-step max: inserts+removes %lu node_visits %lu full_syncs %lu\n", (unsigned long)csteps,
            (unsigned long)pcol_st.inserts, (unsigned long)pcol_st.removes, (unsigned long)pcol_st.searches,
            (unsigned long)pcol_st.visits, (unsigned long)pcol_st.syncs, (unsigned long)pcol_st.flushes,
            (unsigned long)pcol_st.nodes_max, (unsigned long)cmax[0], (unsigned long)cmax[1], (unsigned long)cmax[2]);
    fprintf(stderr, "PCOL per-step max: inserts %lu removes %lu searches %lu; pairs in a pass %lu; pin_ext records %d; pin_en %d; "
            "instances created %d\n", (unsigned long)cmax[3], (unsigned long)cmax[4], (unsigned long)cmax[5],
            (unsigned long)pcol_st.pairs_max, pw_ext_used_max(), pw_en_used_max(), PW.n);
#ifdef PLAY_STATS
    {   /* pworld / prun counters per step (the level start excluded): mean / max */
        int j;
        fprintf(stderr, "PW per step (mean/max):");
        for (j = 0; j < NPW; j++)
            fprintf(stderr, " %s %.1f/%lu", pwname[j], csteps ? (double)pwsum[j] / csteps : 0.0, (unsigned long)pwmax[j]);
        fprintf(stderr, "\n");
    }
#endif
    return 0;
}
