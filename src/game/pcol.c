/* Collision events and the collision tree of GameMaker 2024.14 (libyoyo.so, x86_64 build of the same runtime:
 * HandleCollision, UpdateTree, CollisionInsert / Update / Remove / MarkDirty / MarkTest, RebuildTree,
 * ShouldUseFastCollision, RTree<CInstance*, float, float, 6, 2>; read from the disassembly, 2026-10-03).
 *
 * The tree. A Guttman R-tree, at most 6 and at least 2 branches a node, float rectangles [min, max] (overlap
 * inclusive), area = (max x - min x) * (max y - min y) in float: Search visits branches 0 .. count - 1 depth
 * first; InsertRectRec picks the branch needing the least area increase (ties: the smaller area, then the first);
 * a full node splits with the quadratic method (PickSeeds: the pair wasting the most area, ties the first;
 * ChoosePartition: the entry with the biggest preference, ties to the smaller group; fill to 2 when one group
 * must take the rest); RemoveRectRec removes by swapping in the node's last branch, reinserts the branches of
 * nodes left with fewer than 2 (the list newest first), and drops a root left with one child.
 *
 * Which instances are in it. An object has the flag 0x08 when it has collision events of its own or is the target
 * (or a descendant of a target) of one (CreateColPairs after ExpandCollisionEvents), 0x20 when an ancestor has
 * 0x08 or 0x20 (CRoom::AddInstance, StartRoom) or when a collision function asked for it
 * (ShouldUseFastCollision -> SetInCollisionRecursive, until the next room). An instance is in the tree when its
 * object has 0x08 or 0x20, or it is solid. Its rectangle is its bounding box when it was last put in:
 *   - CollisionInsert (instance_create, before the Create event): put in now;
 *   - a change of x / y / sprite / mask / scale / angle (only a real change: SetPosition compares) marks it dirty:
 *     first on the dirty list (members) and on the test list (0x08), and its box is stale;
 *   - Compute_BoundingBox(true) of a stale instance (a collision function testing it, bbox_*, the Outside Room
 *     check, ...) and UpdateTree (every entry of the dirty list, newest mark first, stale or not; at the start of
 *     HandleCollision and of collision_line / collision_rectangle / instance_place / place_meeting when they use
 *     the tree) remove it and put it in again (CollisionUpdate), unless a search holds the tree;
 *   - instance_destroy only marks it: it leaves the tree at the next RemoveMarked (DoAStep_Draw, before the
 *     drawing; StartRoom after the Create events), in creation order; 251 or more there rebuild the tree.
 *
 * Checked against the reference runner (the arm64 Linux build, which differs from the x86_64 library above only in
 * compiling the area increases fused: PickBranch's and ChoosePartition's growth and PickSeeds' first subtraction are
 * one fnmsub each, the bounding box fmadd; it shows once rubble gives fractional rectangles):
 *   - tools/treeprobe.py: 300 creations, 60 destroys, 50 moves in an empty room: the search order equal after each;
 *   - the instance constructor (SetObjectIndex, SetSpriteIndex) marks every new instance dirty before CollisionInsert
 *     puts it in; the room's own instances are put in at room start and marked dirty (tools/tracer.py
 *     TRACE_GENPROBE: the order during level generation equal at three points);
 *   - TRACE_TREE / TRACE_TREEAT (the search order of every oDrawnSprite at records and inside events) equal on p1_walk.
 * Probes kept: build/host/playhost with PCOL_TREE=r1,r2,... [PCOL_TREE_OBJ=<object>, default oSolid] prints
 * "TREE <r> <n> <ids>" on stderr after record r, the tree's search order as tools/tracer.py TRACE_TREE writes it
 * (pcol_probe); build/host/treeprobe with tools/treeprobe.py; playhost's "PCOL" cost lines (pcol_st). The C side
 * of TRACE_TREEAT / TRACE_GENPROBE and the PCOL_DEBUG logs (PCOL_LOG, PCOL_WATCH, PCOL_DIRTY, PCOL_TREEAT,
 * PCOL_GENPROBE, PCOL_V) were removed after 6e359da (git show 6e359da:src/game/pcol.c to restore them).
 * Not modelled: the C code writes x / y / sprite fields directly, so a change is seen when pcol.c next looks (an
 * event's end, a collision function, pin_set_sprite); a write undone before that (y += 1 then y -= 1) is not a
 * mark here although the runner marks it. The 8 routes do not depend on it; exact in general needs a sync after
 * every write (pcol_event_done(i) wrapped around each, tested: same results).
 *
 * HandleCollision: UpdateTree; each instance of the test list (newest mark first) searches the tree with its box;
 * each instance found that is not itself and has not searched yet in this pass forms a pair (searcher, found) when
 * one of the two objects has a collision event with the other's; the test list is emptied. Then pair by pair:
 * if both exist and overlap (Collision_Instance, precise masks), the found one goes on the test list for the next
 * pass (0x08), and the events run: the searcher's with the found one's object, then the found one's. A searcher with
 * a precise sprite of 2 or more frames stays on the test list (the explosion's growing mask, p1_walk record 312).
 */
#include "pint.h"
#include "pcol.h"
#include "inst.h"
#if defined(__has_include)
#if __has_include(<math.h>)
#include <math.h>                       /* the host: libm's fmaf (arm64 / x86-64 FMA: one rounding) */
#define PCOL_HAVE_MATH_H
#endif
#endif
#ifndef PCOL_HAVE_MATH_H
float fmaf(float x, float y, float z);  /* the SH-2 (no libm): src/sh2/fma.c, correctly rounded */
#endif

/* The runner's fused operations are fmaf calls; every other expression is rounded as written (no contraction:
   clang contracts a * b + c by default; test/host builds playhost_nc with -ffp-contract=off for every file to
   show the routes do not depend on it) */
#ifdef __clang__
#pragma STDC FP_CONTRACT OFF
#endif

struct pcol_stats pcol_st;

/* ---- the R-tree ----------------------------------------------------------------------------------------------- */
#define RMAX 6
#define RMIN 2
#define RT_NODES 1024            /* the routes use at most 260 */
#define ENT_MAX (PIN_MAX + INST_MAX)
struct rbr { float r[4]; int16_t id; };        /* r: min x, min y, max x, max y; id: child node or entry */
struct rnode { int16_t count, level; struct rbr b[RMAX]; };
static struct rnode rn[RT_NODES];
static int16_t rfreel[RT_NODES];
static int rnfree, rnused;
static int16_t rroot;
static uint8_t rlock;

static int nalloc(void)
{
    int n;
    if (rnfree == 0) {                          /* never in the references (RT_NODES is twice the largest use) */
        PUNTR(9101);
        return 0;
    }
    n = rfreel[--rnfree];
    rn[n].count = 0;
    rn[n].level = -1;
    if (++rnused > (int)pcol_st.nodes_max) pcol_st.nodes_max = (uint32_t)rnused;
    return n;
}

static void nfree(int n)
{
    rfreel[rnfree++] = (int16_t)n;
    rnused--;
}

static void rt_reset(void)
{
    int k;
    rnfree = 0;
    for (k = RT_NODES - 1; k >= 0; k--) rfreel[rnfree++] = (int16_t)k;
    rnused = 0;
    rroot = (int16_t)nalloc();
    rn[rroot].level = 0;
}

static float rarea(const float *r)
{
    float w = r[2] - r[0], h = r[3] - r[1];
    return w * h;
}

static void rcomb(float *o, const float *a, const float *b);

/* w * h - a with one rounding: the arm64 runner's fnmsub (s registers). fmaf, not (float)((double)w * h - a):
   the product is exact in double but the difference is rounded twice (to double, then to float) */
static float fms(float w, float h, float a)
{
    return fmaf(w, h, -a);
}

/* the area of the combined rectangle less a (fused) */
static float rcomb_growth(const float *a, const float *b, float area)
{
    float o[4];
    rcomb(o, a, b);
    return fms(o[2] - o[0], o[3] - o[1], area);
}

static void rcomb(float *o, const float *a, const float *b)
{
    o[0] = a[0] < b[0] ? a[0] : b[0];
    o[1] = a[1] < b[1] ? a[1] : b[1];
    o[2] = a[2] > b[2] ? a[2] : b[2];
    o[3] = a[3] > b[3] ? a[3] : b[3];
}

static int roverlap(const float *a, const float *b)
{
    return !(a[0] > b[2] || b[0] > a[2] || a[1] > b[3] || b[1] > a[3]);
}

static void rcover(int n, float *o)
{
    int k;
    const struct rnode *p = &rn[n];
    o[0] = p->b[0].r[0]; o[1] = p->b[0].r[1]; o[2] = p->b[0].r[2]; o[3] = p->b[0].r[3];
    for (k = 1; k < p->count; k++)
        rcomb(o, o, p->b[k].r);
}

/* PartitionVars */
static struct {
    int8_t part[RMAX + 1];
    int8_t count[2];
    float cover[2][4], area[2];
    struct rbr buf[RMAX + 1];
    float cover_split[4], cover_split_area;
} pv;

static void classify(int idx, int g)
{
    pv.part[idx] = (int8_t)g;
    if (pv.count[g] == 0) {
        int k;
        for (k = 0; k < 4; k++) pv.cover[g][k] = pv.buf[idx].r[k];
    } else
        rcomb(pv.cover[g], pv.buf[idx].r, pv.cover[g]);
    pv.area[g] = rarea(pv.cover[g]);
    pv.count[g]++;
}

static void pick_seeds(void)
{
    float area[RMAX + 1], worst, waste;
    int a, b, s0 = -1, s1 = -1, first = 1;
    for (a = 0; a <= RMAX; a++) area[a] = rarea(pv.buf[a].r);
    worst = -1.0f - pv.cover_split_area;
    for (a = 0; a < RMAX; a++)
        for (b = a + 1; b <= RMAX; b++) {
            waste = rcomb_growth(pv.buf[a].r, pv.buf[b].r, area[a]);
            waste = waste - area[b];
            if (waste > worst || (waste == worst && first)) {
                worst = waste;
                s0 = a;
                s1 = b;
                first = 0;
            }
        }
    classify(s0, 0);
    classify(s1, 1);
}

static void choose_partition(void)
{
    const int total = RMAX + 1, minfill = RMIN;
    int chosen = 0, better = 0, idx, g;
    pv.count[0] = pv.count[1] = 0;
    for (idx = 0; idx < total; idx++) pv.part[idx] = -1;
    pick_seeds();
    while (pv.count[0] + pv.count[1] < total && pv.count[0] < total - minfill && pv.count[1] < total - minfill) {
        float biggest = -1.0f;
        for (idx = 0; idx < total; idx++) {
            float g0, g1, diff;
            if (pv.part[idx] != -1) continue;
            g0 = rcomb_growth(pv.buf[idx].r, pv.cover[0], pv.area[0]);
            g1 = rcomb_growth(pv.buf[idx].r, pv.cover[1], pv.area[1]);
            diff = g1 - g0;
            if (diff >= 0) g = 0;
            else { g = 1; diff = -diff; }
            if (diff > biggest) {
                biggest = diff;
                chosen = idx;
                better = g;
            } else if (diff == biggest && pv.count[g] < pv.count[better]) {
                chosen = idx;
                better = g;
            }
        }
        classify(chosen, better);
    }
    if (pv.count[0] + pv.count[1] < total) {
        g = pv.count[0] >= total - minfill ? 1 : 0;
        for (idx = 0; idx < total; idx++)
            if (pv.part[idx] == -1) classify(idx, g);
    }
}

/* AddBranch: 1 when the node split (*newn the new node) */
static int add_branch(const struct rbr *br, int n, int *newn)
{
    struct rnode *p = &rn[n];
    int k;
    if (p->count < RMAX) {
        p->b[p->count++] = *br;
        return 0;
    }
    for (k = 0; k < RMAX; k++) pv.buf[k] = p->b[k];
    pv.buf[RMAX] = *br;
    for (k = 0; k < 4; k++) pv.cover_split[k] = pv.buf[0].r[k];
    for (k = 1; k <= RMAX; k++) rcomb(pv.cover_split, pv.cover_split, pv.buf[k].r);
    pv.cover_split_area = rarea(pv.cover_split);
    choose_partition();
    *newn = nalloc();
    rn[*newn].level = p->level;
    p->count = 0;
    for (k = 0; k <= RMAX; k++) {
        struct rnode *t = &rn[pv.part[k] ? *newn : n];
        t->b[t->count++] = pv.buf[k];
    }
    return 1;
}

static int pick_branch(const float *r, int n)
{
    const struct rnode *p = &rn[n];
    float best_incr = -1.0f, best_area = 0;
    int k, best = 0, first = 1;
    for (k = 0; k < p->count; k++) {
        float area = rarea(p->b[k].r), incr = rcomb_growth(r, p->b[k].r, area);
        if (best_incr > incr || first || (incr == best_incr && best_area > area)) {
            best = k;
            best_area = area;
            best_incr = incr;
            first = 0;
        }
    }
    return best;
}

static int insert_rec(const struct rbr *br, int n, int *newn, int level)
{
    struct rnode *p = &rn[n];
    if (p->level > level) {
        int other, idx = pick_branch(br->r, n), child = p->b[idx].id;
        if (!insert_rec(br, child, &other, level)) {
            rcomb(rn[n].b[idx].r, br->r, rn[n].b[idx].r);
            return 0;
        } else {
            struct rbr nb;
            rcover(child, rn[n].b[idx].r);
            rcover(other, nb.r);
            nb.id = (int16_t)other;
            return add_branch(&nb, n, newn);
        }
    } else if (p->level == level)
        return add_branch(br, n, newn);
    return 0;
}

static void insert_rect(const struct rbr *br, int level)
{
    int newn, dummy;
    if (insert_rec(br, rroot, &newn, level)) {
        int nr = nalloc();
        struct rbr b;
        rn[nr].level = (int16_t)(rn[rroot].level + 1);
        rcover(rroot, b.r);
        b.id = rroot;
        add_branch(&b, nr, &dummy);
        rcover(newn, b.r);
        b.id = (int16_t)newn;
        add_branch(&b, nr, &dummy);
        rroot = (int16_t)nr;
    }
}

/* the reinsert list of RemoveRect (nodes, newest first) */
static int16_t relist[RT_NODES];
static int nrelist;

static void disconnect(int n, int k)
{
    struct rnode *p = &rn[n];
    p->b[k] = p->b[p->count - 1];
    p->count--;
}

/* RemoveRectRec: 0 when found and removed */
static int remove_rec(const float *r, int id, int n)
{
    struct rnode *p = &rn[n];
    int k;
    if (p->level > 0) {
        for (k = 0; k < rn[n].count; k++) {
            if (!roverlap(r, rn[n].b[k].r)) continue;
            if (!remove_rec(r, id, rn[n].b[k].id)) {
                int c = rn[n].b[k].id;
                if (rn[c].count >= RMIN)
                    rcover(c, rn[n].b[k].r);
                else {
                    int j;
                    for (j = nrelist; j > 0; j--) relist[j] = relist[j - 1];
                    relist[0] = (int16_t)c;
                    nrelist++;
                    disconnect(n, k);
                }
                return 0;
            }
        }
        return 1;
    }
    for (k = 0; k < p->count; k++)
        if (p->b[k].id == id) {
            disconnect(n, k);
            return 0;
        }
    return 1;
}

static int remove_rect(const float *r, int id)
{
    nrelist = 0;
    if (remove_rec(r, id, rroot)) return 1;
    while (nrelist > 0) {
        int t = relist[0], k, j;
        for (j = 0; j < nrelist - 1; j++) relist[j] = relist[j + 1];
        nrelist--;
        for (k = 0; k < rn[t].count; k++) {
            int save = nrelist;           /* insert_rect does not use the list */
            insert_rect(&rn[t].b[k], rn[t].level);
            nrelist = save;
        }
        nfree(t);
    }
    if (rn[rroot].count == 1 && rn[rroot].level > 0) {
        int t = rn[rroot].b[0].id;
        nfree(rroot);
        rroot = (int16_t)t;
    }
    return 0;
}

static int (*s_cb)(int e, void *ctx);
static void *s_ctx;
static float s_r[4];

static int search_rec(int n)
{
    int k;
    pcol_st.visits++;
    if (rn[n].level > 0) {
        for (k = 0; k < rn[n].count; k++)
            if (roverlap(s_r, rn[n].b[k].r) && !search_rec(rn[n].b[k].id))
                return 0;
    } else {
        for (k = 0; k < rn[n].count; k++)
            if (roverlap(s_r, rn[n].b[k].r) && s_cb && !s_cb(rn[n].b[k].id, s_ctx))
                return 0;
    }
    return 1;
}

void pcol_search(float l, float t, float r, float b, int (*cb)(int e, void *ctx), void *ctx)
{
    s_r[0] = l; s_r[1] = t; s_r[2] = r; s_r[3] = b;
    s_cb = cb;
    s_ctx = ctx;
    rlock = 1;
    pcol_st.searches++;
    search_rec(rroot);
    rlock = 0;
}

/* ---- objects: the flags 0x08 (F08) and 0x08 | 0x20 by ancestry (MEMBER), 0x20 asked (DYN) ------------------- */
#define OI_DONE 1
#define OI_F08 2
#define OI_MEMBER 4
#define OI_DYN 8
static uint8_t oinfo[OBJ_COUNT];
static int16_t ocnt[OBJ_COUNT];        /* instances of the object and its descendants (the runner's 0x78) */
static uint8_t otarget[OBJ_COUNT];     /* the target of a collision event */
static uint8_t oinit;

static int same_cols(int a, int b)
{
    int k, j;
    if (pobj[a].ncol != pobj[b].ncol) return 0;
    for (k = 0; k < pobj[a].ncol; k++) {
        for (j = 0; j < pobj[b].ncol; j++)
            if (pcol[pobj[a].col0 + k] == pcol[pobj[b].col0 + j]) break;
        if (j == pobj[b].ncol) return 0;
    }
    return 1;
}

static int f08_of(int o)
{
    int t, par = objdefs[o].parent;
    /* events of its own: pobj lists include the parent's (GetEventRecursive), so a list equal to the parent's is
       the parent's (Observed in the HD source: the 8 objects that repeat their parent's targets are oEnemy's
       children, which are targets anyway) */
    if (pobj[o].ncol > 0 && (par < 0 || !same_cols(o, par))) return 1;
    for (t = 0; t < OBJ_COUNT; t++)
        if (otarget[t] && obj_is(o, t)) return 1;
    return 0;
}

static void obj_init(void)
{
    int o, k;
    for (o = 0; o < OBJ_COUNT; o++)
        for (k = 0; k < pobj[o].ncol; k++) otarget[pcol[pobj[o].col0 + k]] = 1;
    for (o = 0; o < OBJ_COUNT; o++) oinfo[o] = (uint8_t)(OI_DONE | (f08_of(o) ? OI_F08 : 0));
    for (o = 0; o < OBJ_COUNT; o++) {
        int a;
        for (a = o; a >= 0; a = objdefs[a].parent)
            if (oinfo[a] & OI_F08) { oinfo[o] |= OI_MEMBER; break; }
    }
    oinit = 1;
}

static void obj_count(int obj, int d)
{
    int a;
    for (a = obj; a >= 0; a = objdefs[a].parent) ocnt[a] = (int16_t)(ocnt[a] + d);
}

/* ---- entries ------------------------------------------------------------------------------------------------- */
#define EF_TREE 1
#define EF_STALE 2
#define EF_PEND 4      /* destroyed: leaves at the next RemoveMarked */
#define EF_GEN 8       /* a generator entry renamed to this play index (pin_add keeps its state) */
#define EF_NOSNAP 16   /* no snapshot yet (taken at the next look without a mark) */
#define EF_USED 32
static uint8_t ef[ENT_MAX];
static int16_t dn[ENT_MAX], dp[ENT_MAX], tn[ENT_MAX], tp[ENT_MAX];
static uint8_t ond[ENT_MAX], ont[ENT_MAX];
static int16_t dhead = -1, thead = -1;
static float er[ENT_MAX][4];
static uint32_t epass[ENT_MAX];
static uint32_t pass_no;
static int16_t pend[ENT_MAX];
static int npend;
static int32_t gen_first_id;           /* the generator's first instance_create id (room instances are below) */
static uint8_t gen_pending;            /* a level was generated: the next pw_reset loads it */

struct snap { pos x, y; int16_t spr, mask; float xs, ys, ang; };
static struct snap sn[PIN_MAX];

static int eobj(int e) { return e < PIN_MAX ? PW.in[e].obj : W.in[e - PIN_MAX].obj; }
static int esolid(int e) { return objdefs[eobj(e)].solid; }
static int emember(int e) { return (oinfo[eobj(e)] & (OI_MEMBER | OI_DYN)) || esolid(e); }

static void dlist_remove(int e)
{
    if (!ond[e]) return;
    if (dp[e] >= 0) dn[dp[e]] = dn[e]; else dhead = dn[e];
    if (dn[e] >= 0) dp[dn[e]] = dp[e];
    ond[e] = 0;
}

static void dlist_front(int e)
{
    dlist_remove(e);
    dp[e] = -1;
    dn[e] = dhead;
    if (dhead >= 0) dp[dhead] = (int16_t)e;
    dhead = (int16_t)e;
    ond[e] = 1;
}

static void tlist_remove(int e)
{
    if (!ont[e]) return;
    if (tp[e] >= 0) tn[tp[e]] = tn[e]; else thead = tn[e];
    if (tn[e] >= 0) tp[tn[e]] = tp[e];
    ont[e] = 0;
}

static void tlist_front(int e)
{
    tlist_remove(e);
    tp[e] = -1;
    tn[e] = thead;
    if (thead >= 0) tp[thead] = (int16_t)e;
    thead = (int16_t)e;
    ont[e] = 1;
}

/* sin and cos of a float angle in radians (|a| <= 2 pi), rounded to float as glibc's sincosf: the polynomials in
   double are within 1e-16 of the true values */
static void sincos_f(float a, float *s, float *c)
{
    double x = a, r, r2, ps, pc;
    int q = 0;
    static const double pio2_1 = 1.57079632673412561417e+00, pio2_2 = 6.07710050650619224932e-11;
    while (x > 0.78539816339744831) { x = (x - pio2_1) - pio2_2; q++; }
    while (x < -0.78539816339744831) { x = (x + pio2_1) + pio2_2; q--; }
    r = x;
    r2 = r * r;
    ps = r + r * r2 * (-1.0 / 6 + r2 * (1.0 / 120 + r2 * (-1.0 / 5040 + r2 * (1.0 / 362880 + r2 * (-1.0 / 39916800 +
         r2 * (1.0 / 6227020800.0 + r2 * (-1.0 / 1307674368000.0)))))));
    pc = 1 + r2 * (-0.5 + r2 * (1.0 / 24 + r2 * (-1.0 / 720 + r2 * (1.0 / 40320 + r2 * (-1.0 / 3628800 +
         r2 * (1.0 / 479001600 + r2 * (-1.0 / 87178291200.0 + r2 * (1.0 / 20922789888000.0))))))));
    switch (q & 3) {
    case 0: *s = (float)ps; *c = (float)pc; break;
    case 1: *s = (float)pc; *c = (float)-ps; break;
    case 2: *s = (float)-ps; *c = (float)-pc; break;
    default: *s = (float)-pc; *c = (float)ps; break;
    }
}

/* CInstance::Compute_BoundingBox (non-compatibility mode), normalized (CollisionUpdate): o = l, t, r, b */
static void ebbox(int e, float dx, float dy, float *o)
{
    float x, y, xs = 1, ys = 1, ang = 0, w, h, t0, t1;
    int s;
    const struct gsprcol *c;
    if (e < PIN_MAX) {
        const struct pin *p = &PW.in[e];
        s = p->mask >= 0 ? p->mask : p->spr;
        x = (float)PTOD(p->x);
        y = (float)PTOD(p->y);
        xs = (float)p->xscale;
        ys = (float)p->yscale;
        ang = (float)p->angle;
    } else {
        const struct inst *g = &W.in[e - PIN_MAX];
        s = g->spr;
        x = (float)g->x;
        y = (float)g->y;
    }
    x = x + dx;
    y = y + dy;
    if (s < 0) {
        o[0] = o[2] = x;
        o[1] = o[3] = y;
        return;
    }
    c = &gsprcol[s];
    if (ang == 0) {
        float l, r, t, b;
        /* fmadd in the runner (s registers): one rounding each */
        w = (float)(c->r - c->l) + 1.0f;
        h = (float)(c->b - c->t) + 1.0f;
        l = fmaf((float)(c->l - c->xo), xs, x);
        r = fmaf(w, xs, l);
        t = fmaf((float)(c->t - c->yo), ys, y);
        b = fmaf(h, ys, t);
        (void)t0; (void)t1;
        if (l > r) { float q = l; l = r; r = q; }
        if (t > b) { float q = t; t = b; b = q; }
        o[0] = l; o[1] = t; o[2] = r; o[3] = b;
    } else {
        /* rotated: the corners (A, C), (B, D) of the box around the origin, rotated by image_angle */
        float A, B, C, D, sn_, cs, a = ang * 3.14159274101257324f, ac, bc, cs_, ds, xmin, xmax, ymin, ymax;
        int lo = c->l < c->r ? c->l : c->r, hi = c->l < c->r ? c->r : c->l;
        int to = c->t < c->b ? c->t : c->b, bo = c->t < c->b ? c->b : c->t;
        a = a / 180.0f;
        A = (float)(lo - c->xo) * xs;
        t0 = (float)(hi - c->xo) + 1.0f;
        B = t0 * xs;
        C = (float)(to - c->yo) * ys;
        t0 = (float)(bo - c->yo) + 1.0f;
        D = t0 * ys;
        sincos_f(a, &sn_, &cs);
        ac = A * cs; bc = B * cs; cs_ = C * sn_; ds = D * sn_;
        xmin = ac < bc ? ac : bc; xmax = ac > bc ? ac : bc;
        ymin = cs_ < ds ? cs_ : ds; ymax = cs_ > ds ? cs_ : ds;
        t0 = xmin + x; o[0] = t0 + ymin;
        t0 = xmax + x; o[2] = t0 + ymax;
        ac = C * cs; bc = D * cs; cs_ = A * sn_; ds = B * sn_;
        xmin = ac < bc ? ac : bc; xmax = ac > bc ? ac : bc;          /* y extent from the cosines */
        ymin = cs_ < ds ? cs_ : ds; ymax = cs_ > ds ? cs_ : ds;      /* minus the x extent from the sines */
        t0 = xmin + y; o[1] = t0 - ymax;
        t0 = xmax + y; o[3] = t0 - ymin;
    }
}

static int edead(int e) { return (ef[e] & EF_PEND) != 0; }

/* CollisionUpdate: take the entry out (if in) and put it in with its current box */
static void cupdate_at(int e, float dx, float dy)
{
    struct rbr b;
    if (!(ef[e] & EF_TREE) && edead(e)) return;
    if (rlock) return;
    if (!emember(e)) return;
    ebbox(e, dx, dy, b.r);
    if (ef[e] & EF_TREE) {
        pcol_st.removes++;
        if (remove_rect(er[e], e)) {
            float cv[4];
            rcover(rroot, cv);
            remove_rect(cv, e);
        }
    }
    b.id = (int16_t)e;
    pcol_st.inserts++;
    insert_rect(&b, 0);
    er[e][0] = b.r[0]; er[e][1] = b.r[1]; er[e][2] = b.r[2]; er[e][3] = b.r[3];
    ef[e] |= EF_TREE;
}

static void cupdate(int e) { cupdate_at(e, 0, 0); }

/* CollisionMarkDirty (with the stale bounding box flag its callers set) */
static void mark_e(int e)
{
    int o = eobj(e);
    ef[e] |= EF_STALE;
    if ((oinfo[o] & (OI_MEMBER | OI_DYN)) || esolid(e)) dlist_front(e);
    if (oinfo[o] & OI_F08) tlist_front(e);
}

/* ---- change detection for play instances (the C code writes x, y, ... directly) ------------------------------ */
static void snap_take(int i)
{
    const struct pin *p = &PW.in[i];
    struct snap *s = &sn[i];
    s->x = p->x; s->y = p->y; s->spr = p->spr; s->mask = p->mask;
    s->xs = (float)p->xscale; s->ys = (float)p->yscale; s->ang = (float)p->angle;
    ef[i] &= (uint8_t)~EF_NOSNAP;
}

static int snap_changed(int i)
{
    const struct pin *p = &PW.in[i];
    const struct snap *s = &sn[i];
    return s->x != p->x || s->y != p->y || s->spr != p->spr || s->mask != p->mask || s->xs != (float)p->xscale ||
           s->ys != (float)p->yscale || s->ang != (float)p->angle;
}

static void sync1(int i)
{
    if (ef[i] & EF_NOSNAP) { snap_take(i); return; }
    if (snap_changed(i)) {
        snap_take(i);
        mark_e(i);
    }
}

static void sync_all(void)
{
    int i;
    pcol_st.syncs++;
    for (i = 0; i < PW.n; i++)
        if (PW.in[i].alive) sync1(i);
}

/* UpdateTree */
static void flush(void)
{
    sync_all();
    pcol_st.flushes++;
    while (dhead >= 0) {
        int e = dhead;
        dlist_remove(e);
        if (!edead(e)) {
            ef[e] &= (uint8_t)~EF_STALE;
            cupdate(e);
        }
    }
}

static void touch_e(int e)
{
    if (e < PIN_MAX) sync1(e);
    if (ef[e] & EF_STALE) {
        ef[e] &= (uint8_t)~EF_STALE;
        cupdate(e);
    }
}

void pcol_touch(int i) { touch_e(i); }

void pcol_event_done(int i) { if (i >= 0 && PW.in[i].alive) sync1(i); }

void pcol_touch_at(int self, double dx, double dy)
{
    sync1(self);
    if (ef[self] & EF_STALE) {
        ef[self] &= (uint8_t)~EF_STALE;
        cupdate_at(self, (float)dx, (float)dy);
    }
}

void pcol_place_marks(int self)
{
    sync1(self);
    mark_e(self);
}

void pcol_mark(int i)
{
    if (ef[i] & EF_NOSNAP) { snap_take(i); }
    else snap_take(i);
    mark_e(i);
}

static void entry_clear(int e)
{
    dlist_remove(e);
    tlist_remove(e);
    ef[e] = 0;
    epass[e] = 0xFFFFFFFFu;
}

/* ---- rooms --------------------------------------------------------------------------------------------------- */
static void room_reset(void)
{
    int e, o;
    if (!oinit) obj_init();
    rt_reset();
    for (e = 0; e < ENT_MAX; e++) {
        ef[e] = 0;
        ond[e] = ont[e] = 0;
        epass[e] = 0xFFFFFFFFu;
    }
    dhead = thead = -1;
    npend = 0;
    for (o = 0; o < OBJ_COUNT; o++) {
        oinfo[o] &= (uint8_t)~OI_DYN;
        ocnt[o] = 0;
    }
    rlock = 0;
}

static void rebuild_all(void);

/* CRoom::RemoveMarked */
static void remove_marked(void)
{
    int k, j, many = npend >= 251;
    for (k = 1; k < npend; k++) {                     /* creation order (the active list) */
        int16_t v = pend[k];
        for (j = k; j > 0 && pend[j - 1] > v; j--) pend[j] = pend[j - 1];
        pend[j] = v;
    }
    for (k = 0; k < npend; k++) {
        int e = pend[k];
        if ((ef[e] & EF_TREE) && !many) {
            pcol_st.removes++;
            if (remove_rect(er[e], e)) {
                float cv[4];
                rcover(rroot, cv);
                remove_rect(cv, e);
            }
        }
        obj_count(eobj(e), -1);
        entry_clear(e);
    }
    npend = 0;
    if (many) rebuild_all();
}

/* RebuildTree(false): every instance in creation order: marked dirty, put in */
static void rebuild_all(void)
{
    int e;
    rt_reset();
    for (e = 0; e < ENT_MAX; e++) ef[e] &= (uint8_t)~EF_TREE;
    for (e = 0; e < PW.n; e++)
        if (PW.in[e].alive && !edead(e)) {
            mark_e(e);
            ef[e] &= (uint8_t)~EF_STALE;
            cupdate(e);
        }
}

void pcol_remove_marked(void) { remove_marked(); }

void pcol_destroyed(int i)
{
    if (i < 0 || (ef[i] & EF_PEND)) return;
    ef[i] |= EF_PEND;
    pend[npend++] = (int16_t)i;
}

/* the generated level (entries PIN_MAX + w): RemoveMarked after the room's Create events, then the alive ones are
   renamed to their play index (play_level_start adds them in creation order) */
static void gen_load(void)
{
    static int16_t map[INST_MAX];
    int w, n = 0, k;
    remove_marked();
    for (w = 0; w < W.n; w++) map[w] = (int16_t)(W.in[w].alive ? n++ : -1);
    /* tree leaves */
    for (k = 0; k < RT_NODES; k++) {
        int j;
        if (rn[k].level != 0) continue;
        for (j = 0; j < rn[k].count; j++)
            if (rn[k].b[j].id >= PIN_MAX) rn[k].b[j].id = map[rn[k].b[j].id - PIN_MAX];
    }
    /* entries: copy (play indices are below their generator index + PIN_MAX, so ascending order is safe) */
    {
        int16_t dl[ENT_MAX], tl[ENT_MAX];
        int nd = 0, nt = 0, e;
        for (e = dhead; e >= 0; e = dn[e]) dl[nd++] = (int16_t)(e >= PIN_MAX ? map[e - PIN_MAX] : e);
        for (e = thead; e >= 0; e = tn[e]) tl[nt++] = (int16_t)(e >= PIN_MAX ? map[e - PIN_MAX] : e);
        for (e = 0; e < ENT_MAX; e++) ond[e] = ont[e] = 0;
        dhead = thead = -1;
        for (w = 0; w < W.n; w++) {
            int i = map[w], g = PIN_MAX + w;
            if (i < 0) continue;
            ef[i] = (uint8_t)((ef[g] & (EF_TREE | EF_STALE)) | EF_GEN | EF_NOSNAP | EF_USED);
            er[i][0] = er[g][0]; er[i][1] = er[g][1]; er[i][2] = er[g][2]; er[i][3] = er[g][3];
            epass[i] = epass[g];
        }
        for (e = PIN_MAX; e < ENT_MAX; e++) ef[e] = 0;
        for (k = nd - 1; k >= 0; k--) if (dl[k] >= 0) dlist_front(dl[k]);
        for (k = nt - 1; k >= 0; k--) if (tl[k] >= 0) tlist_front(tl[k]);
    }
}

void pcol_after_reset(void)
{
    if (gen_pending) {
        gen_pending = 0;
        gen_load();
    } else
        room_reset();
}

void pcol_added(int i)
{
    if (ef[i] & EF_GEN) {                         /* a generated instance: its state came with it */
        ef[i] &= (uint8_t)~EF_GEN;
        return;
    }
    entry_clear(i);
    ef[i] = EF_USED;
    snap_take(i);
    obj_count(PW.in[i].obj, 1);
}

/* instance_create: the CInstance constructor marks it dirty (SetObjectIndex, SetSpriteIndex: CollisionMarkDirty;
   Observed: tools/treeprobe.py, 3 creations in an empty room leave the leaf [1, 2, 0]: put in at creation, put in
   again newest first by the next UpdateTree), then CRoom::AddInstance's CollisionInsert puts it in */
void pcol_create(int i)
{
    snap_take(i);
    mark_e(i);
    ef[i] &= (uint8_t)~EF_STALE;
    cupdate(i);                                   /* Compute_BoundingBox(true) */
    if (oinfo[PW.in[i].obj] & OI_F08) tlist_front(i);
}

void pcol_room_inst(int i)
{
    ef[i] |= EF_NOSNAP;                           /* the loader sets its fields after pin_add */
    mark_e(i);
}

/* ---- collision functions ------------------------------------------------------------------------------------- */
static void set_dyn(int obj)
{
    int o;
    for (o = 0; o < OBJ_COUNT; o++)
        if (obj_is(o, obj) && !(oinfo[o] & (OI_MEMBER | OI_DYN))) oinfo[o] |= OI_DYN;
}

/* ShouldUseFastCollision, and UpdateTree when it says 1 */
static int query_e(int obj, int gen)
{
    int cnt = ocnt[obj];
    if (cnt == 0) return -1;
    if (cnt < rn[rroot].level) return 2;
    if (!(oinfo[obj] & (OI_MEMBER | OI_DYN))) {
        int e, n = gen ? W.n : PW.n;
        set_dyn(obj);
        for (e = 0; e < n; e++) {
            int ent = gen ? PIN_MAX + e : e;
            int alive = gen ? W.in[e].alive : PW.in[e].alive;
            struct rbr b;
            if (!alive || edead(ent) || !obj_is(eobj(ent), obj) || (ef[ent] & EF_TREE)) continue;
            if (!gen) sync1(ent);
            ef[ent] &= (uint8_t)~EF_STALE;        /* Compute_BoundingBox(false) */
            ebbox(ent, 0, 0, b.r);
            b.id = (int16_t)ent;
            pcol_st.inserts++;
            insert_rect(&b, 0);
            er[ent][0] = b.r[0]; er[ent][1] = b.r[1]; er[ent][2] = b.r[2]; er[ent][3] = b.r[3];
            ef[ent] |= EF_TREE;
        }
    }
    if (gen) {
        while (dhead >= 0) {
            int e = dhead;
            dlist_remove(e);
            if (!edead(e)) {
                ef[e] &= (uint8_t)~EF_STALE;
                cupdate(e);
            }
        }
    } else
        flush();
    return 1;
}

int pcol_query(int obj) { return query_e(obj, 0); }

/* HandleCollision */
#define PAIRS_MAX 4096
static int16_t pa[PAIRS_MAX], pb[PAIRS_MAX];
static int npairs;
static int hc_self;

static int has_col(int a, int b)
{
    const struct pobj *o = &pobj[PX(a).obj];
    int k;
    for (k = 0; k < o->ncol; k++)
        if (obj_is(PX(b).obj, pcol[o->col0 + k]))
            return 1;
    return 0;
}

static int collision_result(int e, void *ctx)
{
    (void)ctx;
    if (e == hc_self || epass[e] == pass_no) return 1;
    if ((has_col(hc_self, e) || has_col(e, hc_self)) && npairs < PAIRS_MAX) {
        pa[npairs] = (int16_t)hc_self;
        pb[npairs] = (int16_t)e;
        npairs++;
    }
    return 1;
}

/* a precise sprite of 2 or more frames (Compute_BoundingBox's flag 0x200: the sprite's collision kind 1; HandleCollision
   keeps such a searcher on the test list for the next pass. Observed: p1_walk record 312, the explosion's growing
   mask reaches a block two steps after its creation) */
static int keeps_testing(int i)
{
    const struct pin *p = &PW.in[i];
    int s = p->mask >= 0 ? p->mask : p->spr;
    return s >= 0 && gsprcol[s].kind == 1 && psprite[s].frames >= 2;
}

void pcol_handle(void)
{
    int k, nkeep = 0;
    static int16_t keep[PIN_MAX];
    npairs = 0;
    flush();
    while (thead >= 0) {
        int s = thead;
        tlist_remove(s);
        if (edead(s) || !PW.in[s].alive) continue;
        {
            float r[4];
            ebbox(s, 0, 0, r);
            hc_self = s;
            pcol_search(r[0], r[1], r[2], r[3], collision_result, 0);
        }
        if (keeps_testing(s)) {                   /* pushed on the front of a local list */
            for (k = nkeep; k > 0; k--) keep[k] = keep[k - 1];
            keep[0] = (int16_t)s;
            nkeep++;
        }
        epass[s] = pass_no;
    }
    pass_no++;
    for (k = 0; k < nkeep; k++)                   /* each pushed on the front of the test list */
        tlist_front(keep[k]);
    for (k = 0; k < npairs; k++) {
        int a = pa[k], b = pb[k];
        if (!PX(a).alive || !PX(b).alive || !pin_overlap(a, b))
            continue;
        if (oinfo[PX(b).obj] & OI_F08) tlist_front(b);                   /* CollisionMarkTest */
        if (has_col(a, b)) { ev_collision(a, b); pcol_event_done(a); }   /* Perform_Event(a, b) */
        if (has_col(b, a)) { ev_collision(b, a); pcol_event_done(b); }   /* Perform_Event(b, a), unchecked */
    }
}

/* tools/tracer.py TRACE_TREE: collision_rectangle_list(-100000, -100000, 100000, 100000, obj, 0, 0, l, false) */
static int32_t *pr_ids;
static int pr_n, pr_max, pr_obj;

static int probe_cb(int e, void *ctx)
{
    (void)ctx;
    if (e < PIN_MAX && PW.in[e].alive && !edead(e) && obj_is(PW.in[e].obj, pr_obj) && pr_n < pr_max)
        pr_ids[pr_n++] = PW.in[e].id;
    return 1;
}

int pcol_probe(int obj, int32_t *ids, int max)
{
    pr_ids = ids;
    pr_n = 0;
    pr_max = max;
    pr_obj = obj;
    if (pcol_query(obj) == 1)
        pcol_search(-100000.0f, -100000.0f, 100000.0f, 100000.0f, probe_cb, 0);
    return pr_n;
}



/* ---- the generator (inst.c's hook) --------------------------------------------------------------------------- */
void pcol_gen_hook(int op, int w, int a, int b, int c)
{
    int e = PIN_MAX + w, k;
    switch (op) {
    case IH_RESET:                                /* StartRoom: RebuildTree(true) */
        room_reset();
        gen_first_id = a;
        gen_pending = 1;
        break;
    case IH_CREATE:
        ef[e] = EF_USED;
        epass[e] = 0xFFFFFFFFu;
        obj_count(W.in[w].obj, 1);
        /* a room instance (StartRoom: put in by RebuildTree(true), marked dirty; Observed: tools/tracer.py
           TRACE_GENPROBE before scrLevelGen equals this only with oPlayer1 in the tree) or instance_create (the
           constructor's mark, then CollisionInsert): the same here */
        mark_e(e);
        ef[e] &= (uint8_t)~EF_STALE;
        cupdate(e);
        if (oinfo[W.in[w].obj] & OI_F08) tlist_front(e);
        break;
    case IH_SPRITE:                               /* before the change; a: the new sprite */
        if (W.in[w].spr != a) mark_e(e);
        break;
    case IH_MOVE:
        mark_e(e);
        break;
    case IH_DESTROY:
        if (!(ef[e] & EF_PEND)) {
            ef[e] |= EF_PEND;
            pend[npend++] = (int16_t)e;
        }
        break;
    case IH_POINT:                                /* collision_point(.., a = obj) = w: tests the instances in
                                                     creation order up to the hit (Collision_Point) */
    case IH_DIST:                                 /* distance_to_object(a = obj) from w: self, then every instance */
        if (op == IH_DIST) touch_e(e);
        for (k = 0; k < W.n; k++) {
            if (op == IH_POINT && w >= 0 && k > w) break;
            if (W.in[k].alive && obj_is(W.in[k].obj, a) && (ef[PIN_MAX + k] & EF_STALE)) touch_e(PIN_MAX + k);
        }
        break;
    case IH_RECT:                                 /* collision_rectangle(.., a = obj) = w */
        if (query_e(a, 1) == 2)
            for (k = 0; k < W.n; k++) {
                if (w >= 0 && k > w) break;
                if (W.in[k].alive && obj_is(W.in[k].obj, a) && (ef[PIN_MAX + k] & EF_STALE)) touch_e(PIN_MAX + k);
            }
        break;
    case IH_PLACE: {                              /* instance_place by w (a = obj) = b; c = dx + 4096 * dy + offset */
        int q = query_e(a, 1), dx = (c & 4095) - 2048, dy = ((c >> 12) & 4095) - 2048, moved = dx || dy;
        if (q == -1) break;
        if (q == 1) {
            touch_e(e);
            if (moved) mark_e(e);
        } else {
            if (moved) mark_e(e);
            for (k = 0; k < W.n; k++) {
                if (b >= 0 && k > b) break;
                if (!W.in[k].alive || k == w || !obj_is(W.in[k].obj, a)) continue;
                touch_e(PIN_MAX + k);
                if (ef[e] & EF_STALE) {
                    ef[e] &= (uint8_t)~EF_STALE;
                    cupdate_at(e, (float)dx, (float)dy);
                }
            }
            if (moved) mark_e(e);
        }
        break;
    }
    default:
        break;
    }
}
