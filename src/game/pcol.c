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
 * Marks: the play code writes x / y / sprite / mask / scales / angle only through play.h's pin_set* setters, which
 * mark the instance at each real change as SetPosition does (a write undone later, y += 1 then y -= 1, is two marks
 * as in the runner); make -C test/host constcheck makes any direct write a compile error.
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
#ifdef PLAY_STATS
#include <stdio.h>
#include <stdlib.h>
#endif
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
/* 448 nodes (124 bytes each): the most seen is 421 (olmec, level 16; 260-281 in the mines; playhost over 20 seeds x
   16 levels); running out stops the play loop with untranslated code 9101 */
#define RT_NODES 448
/* entries: play instance i is entry i; while a level is generated (gmode), generator instance w is entry w
   (ENT_MAX: above) */
/* rectangle sides in one of two forms (w): w 1, whole numbers below 2^14 in magnitude as ints (the floats' order
   and, below 2^24, their arithmetic are the ints'); w 0, order-mapped float bits (fkey: signed int order = float
   order; -0 as +0), whose area arithmetic takes the floats back (kf). Both forms compare with integer compares;
   two rectangles of different forms are compared as keys (rkey). w sits in the struct's padding */
typedef int32_t rk;
struct rbr { rk r[4]; int16_t id; int16_t w; }; /* r: min x, min y, max x, max y; id: child node or entry */
#define WLIM 16384                              /* whole form: |side| < 2^14 */
struct rnode { int16_t count, level; struct rbr b[RMAX]; };
static struct rnode rn[RT_NODES];
static int16_t rfreel[RT_NODES];
static int rnfree, rnused;
static int16_t rroot;
static uint8_t rlock;
/* links for the in-place update (cupdate_at): the node holding each branch's child (npar) and the leaf holding each
   entry (eleaf); set wherever a branch is put in a node (add_branch, gen_load's renaming) */
#define ENT_MAX (PIN_MAX > INST_MAX ? PIN_MAX : INST_MAX)
static int16_t npar[RT_NODES];
static int16_t eleaf[ENT_MAX];

static int nalloc(void)
{
    int n;
    if (rnfree == 0) {                          /* never in the references (RT_NODES is 1.06 x the largest use) */
        PUNTR(9101);
        return 0;
    }
    n = rfreel[--rnfree];
    rn[n].count = 0;
    rn[n].level = -1;
    ++rnused;
    PCST(rnused > (int)pcol_st.nodes_max ? (pcol_st.nodes_max = (uint32_t)rnused) : 0);
    return n;
}

static void rnode_free(int n)
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

/* float comparisons as integer ones (the SH-2 has no FPU): the bits mapped so that signed order is the float order,
   -0 as +0 (equal as floats); no NaN occurs here */
static int32_t fkey(float f)
{
    union { float f; int32_t i; } u;
    int32_t b;
    u.f = f;
    b = u.i;
    if (b == (int32_t)0x80000000) return 0;
    return b >= 0 ? b : (int32_t)(b ^ 0x7fffffff);
}
#define FLT(a, b) (fkey(a) < fkey(b))
#define FGT(a, b) (fkey(a) > fkey(b))
#define FEQ(a, b) (fkey(a) == fkey(b))

/* the float of a key */
static uint32_t kbits(rk k) { return k >= 0 ? (uint32_t)k : (uint32_t)(k ^ 0x7fffffff); }
static float kf(rk k)
{
    union { float f; uint32_t u; } v;
    v.u = kbits(k);
    return v.f;
}

static rk ikey(int32_t v);
static int remove_fast(int e);

/* side k of a rectangle as a key, and as a float */
static rk rkey(const struct rbr *a, int k) { return a->w ? ikey(a->r[k]) : a->r[k]; }
static float rflt(const struct rbr *a, int k) { return a->w ? (float)a->r[k] : kf(a->r[k]); }

/* a rectangle from whole-number sides: the whole form when they are below 2^14, else keys */
static __attribute__((noinline)) void rset_keys(struct rbr *o, int32_t l, int32_t t, int32_t r, int32_t b)
{
    o->r[0] = ikey(l); o->r[1] = ikey(t); o->r[2] = ikey(r); o->r[3] = ikey(b);
    o->w = 0;
}

/* inline: the whole form (the common case) is four compares and the stores; the keys out of line */
static inline void rset_i(struct rbr *o, int32_t l, int32_t t, int32_t r, int32_t b)
{
    if ((uint32_t)l + (WLIM - 1) < 2 * WLIM - 1 && (uint32_t)t + (WLIM - 1) < 2 * WLIM - 1 &&
        (uint32_t)r + (WLIM - 1) < 2 * WLIM - 1 && (uint32_t)b + (WLIM - 1) < 2 * WLIM - 1) {
        o->r[0] = l; o->r[1] = t; o->r[2] = r; o->r[3] = b;
        o->w = 1;
    } else
        rset_keys(o, l, t, r, b);
}

static int kint(rk k, int32_t *o);

/* a rectangle from float sides: the whole form when they are whole numbers below 2^14 (kint), else keys */
static void rset_f(struct rbr *o, const float *f)
{
    int32_t i[4];
    o->r[0] = fkey(f[0]); o->r[1] = fkey(f[1]); o->r[2] = fkey(f[2]); o->r[3] = fkey(f[3]);
    o->w = 0;
    if (kint(o->r[0], &i[0]) && kint(o->r[1], &i[1]) && kint(o->r[2], &i[2]) && kint(o->r[3], &i[3])) {
        o->r[0] = i[0]; o->r[1] = i[1]; o->r[2] = i[2]; o->r[3] = i[3];
        o->w = 1;
    }
}

static void rcopy(struct rbr *o, const struct rbr *a)
{
    o->r[0] = a->r[0]; o->r[1] = a->r[1]; o->r[2] = a->r[2]; o->r[3] = a->r[3];
    o->w = a->w;
}

/* ---- the area arithmetic (float, the runner's) with an exact integer path: a float that is a whole number below
   2^14 in magnitude gives its int without soft-float (fint), and float sums / products / fmaf of such values are the
   integer results while those stay below 2^24 in magnitude. struct xv holds a value as an int (k & 1) and / or a
   float (k & 2) ------------------------------------------------------------------------------------------------- */
struct xv { float f; int32_t i; uint8_t k; };

/* 2^(e - 127 + 9) for e = 127 .. 140: m * this has the integer part of m * 2^(e - 150) in its high word */
static const uint32_t fint_mul[14] = { 1u << 9, 1u << 10, 1u << 11, 1u << 12, 1u << 13, 1u << 14, 1u << 15, 1u << 16,
                                       1u << 17, 1u << 18, 1u << 19, 1u << 20, 1u << 21, 1u << 22 };

static int fint_bits(uint32_t u, int32_t *o)
{
    struct { uint32_t u; } v;
    uint32_t e, m, hi, lo;
    uint64_t p;
    v.u = u;
    if ((v.u & 0x7fffffffu) == 0) { *o = 0; return 1; }
    e = (v.u >> 23) & 0xffu;
    if (e < 127 || e > 140) return 0;
    m = (v.u & 0x7fffffu) | 0x800000u;
    p = (uint64_t)m * fint_mul[e - 127];
    hi = (uint32_t)(p >> 32);
    lo = (uint32_t)p;
    if (lo != 0) return 0;
    *o = (v.u & 0x80000000u) ? -(int32_t)hi : (int32_t)hi;
    return 1;
}

/* the int of a key's float (a whole number below 2^14) */
static int kint(rk k, int32_t *o) { return fint_bits(kbits(k), o); }

#define XV_LIM 16777216                       /* 2^24: below it, the float results are the integers */
static int xv_small(int32_t i) { return i > -XV_LIM && i < XV_LIM; }

static float xv_f(struct xv *v)
{
    if (!(v->k & 2)) { v->f = (float)v->i; v->k |= 2; }
    return v->f;
}

static void xv_setf(struct xv *v, float f) { v->f = f; v->k = 2; }
static void xv_seti(struct xv *v, int32_t i) { v->i = i; v->k = 1; }

/* a - b as the float subtraction gives it */
static void xv_sub(struct xv *o, struct xv *a, struct xv *b)
{
    if ((a->k & 1) && (b->k & 1) && xv_small(a->i - b->i)) xv_seti(o, a->i - b->i);
    else xv_setf(o, xv_f(a) - xv_f(b));
}

/* a > b, a == b as float comparisons */
static int xv_gt(struct xv *a, struct xv *b)
{
    if ((a->k & 1) && (b->k & 1)) return a->i > b->i;
    return FGT(xv_f(a), xv_f(b));
}

static int xv_eq(struct xv *a, struct xv *b)
{
    if ((a->k & 1) && (b->k & 1)) return a->i == b->i;
    return FEQ(xv_f(a), xv_f(b));
}

#if defined(__GNUC__)
#define PCOL_NOINLINE __attribute__((noinline))
#define PCOL_INLINE static inline __attribute__((always_inline))
#else
#define PCOL_NOINLINE
#define PCOL_INLINE static inline
#endif

/* the ints of a key-form rectangle's sides when they are whole numbers below 2^14 */
static int rints_k(const struct rbr *x, int32_t *i)
{
    return kint(x->r[0], &i[0]) && kint(x->r[1], &i[1]) && kint(x->r[2], &i[2]) && kint(x->r[3], &i[3]);
}

/* the area of x: (r[2] - r[0]) * (r[3] - r[1]) in float (rarea_slow: not both whole, or a large product) */
static PCOL_NOINLINE void rarea_slow(struct xv *o, const struct rbr *x)
{
    int32_t i[4];
    if (!x->w && rints_k(x, i) && xv_small((i[2] - i[0]) * (i[3] - i[1]))) {
        xv_seti(o, (i[2] - i[0]) * (i[3] - i[1]));
        return;
    }
    {
        float w = rflt(x, 2) - rflt(x, 0), h = rflt(x, 3) - rflt(x, 1);
        xv_setf(o, w * h);
    }
}

PCOL_INLINE void rarea(struct xv *o, const struct rbr *x)
{
    if (x->w) {
        int32_t a = (x->r[2] - x->r[0]) * (x->r[3] - x->r[1]);
        if (xv_small(a)) { o->i = a; o->k = 1; return; }
    }
    rarea_slow(o, x);
}



/* w * h - a with one rounding: the arm64 runner's fnmsub (s registers). fmaf, not (float)((double)w * h - a):
   the product is exact in double but the difference is rounded twice (to double, then to float) */
static float fms(float w, float h, float a)
{
    return fmaf(w, h, -a);
}

/* the area of the combined rectangle less area (fused: one rounding) */
/* the union of two rectangles not both whole: as keys */
static PCOL_NOINLINE void rcomb_slow(struct rbr *o, const struct rbr *a, const struct rbr *b)
{
    rk p[4], q[4];
    int k;
    for (k = 0; k < 4; k++) { p[k] = rkey(a, k); q[k] = rkey(b, k); }
    o->r[0] = p[0] < q[0] ? p[0] : q[0];
    o->r[1] = p[1] < q[1] ? p[1] : q[1];
    o->r[2] = p[2] > q[2] ? p[2] : q[2];
    o->r[3] = p[3] > q[3] ? p[3] : q[3];
    o->w = 0;
    {   /* the sides taken from whole ones: back to the whole form */
        int32_t i[4];
        if (rints_k(o, i)) { o->r[0] = i[0]; o->r[1] = i[1]; o->r[2] = i[2]; o->r[3] = i[3]; o->w = 1; }
    }
}

/* the union (o may be a or b) */
PCOL_INLINE void rcomb(struct rbr *o, const struct rbr *a, const struct rbr *b)
{
    if (a->w & b->w) {
        rk x0 = a->r[0] < b->r[0] ? a->r[0] : b->r[0], y0 = a->r[1] < b->r[1] ? a->r[1] : b->r[1];
        rk x1 = a->r[2] > b->r[2] ? a->r[2] : b->r[2], y1 = a->r[3] > b->r[3] ? a->r[3] : b->r[3];
        o->r[0] = x0; o->r[1] = y0; o->r[2] = x1; o->r[3] = y1;
        o->w = 1;
    } else
        rcomb_slow(o, a, b);
}

static PCOL_NOINLINE void rcomb_growth_slow(struct xv *res, const struct rbr *a, const struct rbr *b,
                                            struct xv *area)
{
    struct rbr o;
    int32_t i[4];
    rcomb(&o, a, b);
    if ((area->k & 1) && (o.w ? (i[0] = o.r[0], i[1] = o.r[1], i[2] = o.r[2], i[3] = o.r[3], 1) : rints_k(&o, i))) {
        int32_t g = (i[2] - i[0]) * (i[3] - i[1]) - area->i;
        if (xv_small(g)) { xv_seti(res, g); return; }
    }
    xv_setf(res, fms(rflt(&o, 2) - rflt(&o, 0), rflt(&o, 3) - rflt(&o, 1), xv_f(area)));
}

/* the area of the union less area (fused: one rounding); both whole and area an int: the int result */
PCOL_INLINE void rcomb_growth(struct xv *res, const struct rbr *a, const struct rbr *b, struct xv *area)
{
    if (a->w & b->w & area->k & 1) {
        rk x0 = a->r[0] < b->r[0] ? a->r[0] : b->r[0], y0 = a->r[1] < b->r[1] ? a->r[1] : b->r[1];
        rk x1 = a->r[2] > b->r[2] ? a->r[2] : b->r[2], y1 = a->r[3] > b->r[3] ? a->r[3] : b->r[3];
        int32_t g = (x1 - x0) * (y1 - y0) - area->i;
        if (xv_small(g)) { res->i = g; res->k = 1; return; }
    }
    rcomb_growth_slow(res, a, b, area);
}

static PCOL_NOINLINE int roverlap_slow(const struct rbr *a, const struct rbr *b)
{
    return !(rkey(a, 0) > rkey(b, 2) || rkey(b, 0) > rkey(a, 2) || rkey(a, 1) > rkey(b, 3) || rkey(b, 1) > rkey(a, 3));
}

PCOL_INLINE int roverlap(const struct rbr *a, const struct rbr *b)
{
    if (a->w & b->w)
        return !(a->r[0] > b->r[2] || b->r[0] > a->r[2] || a->r[1] > b->r[3] || b->r[1] > a->r[3]);
    return roverlap_slow(a, b);
}

/* the cover of node n's branches into o's sides (o->id is kept) */
static void rcover(int n, struct rbr *o)
{
    int k;
    const struct rnode *p = &rn[n];
    rcopy(o, &p->b[0]);
    for (k = 1; k < p->count; k++)
        rcomb(o, o, &p->b[k]);
}

/* PartitionVars */
static struct {
    int8_t part[RMAX + 1];
    int8_t count[2];
    struct rbr cover[2];
    struct xv area[2];
    struct rbr buf[RMAX + 1];
    struct rbr cover_split;
    struct xv cover_split_area;
} pv;

static void classify(int idx, int g)
{
    pv.part[idx] = (int8_t)g;
    if (pv.count[g] == 0)
        rcopy(&pv.cover[g], &pv.buf[idx]);
    else
        rcomb(&pv.cover[g], &pv.buf[idx], &pv.cover[g]);
    rarea(&pv.area[g], &pv.cover[g]);
    pv.count[g]++;
}

static void pick_seeds(void)
{
    struct xv area[RMAX + 1], worst, waste, g, m1;
    int a, b, s0 = -1, s1 = -1, first = 1;
    for (a = 0; a <= RMAX; a++) rarea(&area[a], &pv.buf[a]);
    xv_seti(&m1, -1);
    xv_sub(&worst, &m1, &pv.cover_split_area);
    for (a = 0; a < RMAX; a++)
        for (b = a + 1; b <= RMAX; b++) {
            rcomb_growth(&g, &pv.buf[a], &pv.buf[b], &area[a]);
            xv_sub(&waste, &g, &area[b]);
            if (xv_gt(&waste, &worst) || (xv_eq(&waste, &worst) && first)) {
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
        struct xv biggest;
        xv_seti(&biggest, -1);
        for (idx = 0; idx < total; idx++) {
            struct xv g0, g1, diff;
            if (pv.part[idx] != -1) continue;
            rcomb_growth(&g0, &pv.buf[idx], &pv.cover[0], &pv.area[0]);
            rcomb_growth(&g1, &pv.buf[idx], &pv.cover[1], &pv.area[1]);
            xv_sub(&diff, &g1, &g0);
            if ((diff.k & 1) ? diff.i >= 0 : fkey(diff.f) >= 0) g = 0;
            else {
                g = 1;
                if (diff.k & 1) diff.i = -diff.i;
                if (diff.k & 2) diff.f = -diff.f;
            }
            if (xv_gt(&diff, &biggest)) {
                biggest = diff;
                chosen = idx;
                better = g;
            } else if (xv_eq(&diff, &biggest) && pv.count[g] < pv.count[better]) {
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
static void link_in(int n, const struct rbr *br)
{
    if (rn[n].level == 0) eleaf[br->id] = (int16_t)n;
    else npar[br->id] = (int16_t)n;
}

static int add_branch(const struct rbr *br, int n, int *newn)
{
    struct rnode *p = &rn[n];
    int k;
    if (p->count < RMAX) {
        p->b[p->count++] = *br;
        link_in(n, br);
        return 0;
    }
    for (k = 0; k < RMAX; k++) pv.buf[k] = p->b[k];
    pv.buf[RMAX] = *br;
    rcopy(&pv.cover_split, &pv.buf[0]);
    for (k = 1; k <= RMAX; k++) rcomb(&pv.cover_split, &pv.cover_split, &pv.buf[k]);
    rarea(&pv.cover_split_area, &pv.cover_split);
    choose_partition();
    *newn = nalloc();
    rn[*newn].level = p->level;
    p->count = 0;
    for (k = 0; k <= RMAX; k++) {
        int m = pv.part[k] ? *newn : n;
        struct rnode *t = &rn[m];
        t->b[t->count++] = pv.buf[k];
        link_in(m, &pv.buf[k]);
    }
    return 1;
}

static int pick_branch(const struct rbr *r, int n)
{
    const struct rnode *p = &rn[n];
    struct xv best_incr, best_area, area, incr;
    int k, best = 0, first = 1;
    xv_seti(&best_incr, -1);
    xv_seti(&best_area, 0);
    for (k = 0; k < p->count; k++) {
        rarea(&area, &p->b[k]);
        rcomb_growth(&incr, r, &p->b[k], &area);
        if (first || xv_gt(&best_incr, &incr) || (xv_eq(&incr, &best_incr) && xv_gt(&best_area, &area))) {
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
        int other, idx = pick_branch(br, n), child = p->b[idx].id;
        if (!insert_rec(br, child, &other, level)) {
            rcomb(&rn[n].b[idx], br, &rn[n].b[idx]);
            return 0;
        } else {
            struct rbr nb;
            rcover(child, &rn[n].b[idx]);
            rcover(other, &nb);
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
        rcover(rroot, &b);
        b.id = rroot;
        add_branch(&b, nr, &dummy);
        rcover(newn, &b);
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
static int remove_rec(const struct rbr *r, int id, int n)
{
    struct rnode *p = &rn[n];
    int k;
    if (p->level > 0) {
        for (k = 0; k < rn[n].count; k++) {
            if (!roverlap(r, &rn[n].b[k])) continue;
            if (!remove_rec(r, id, rn[n].b[k].id)) {
                int c = rn[n].b[k].id;
                if (rn[c].count >= RMIN)
                    rcover(c, &rn[n].b[k]);
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

static int remove_rect(const struct rbr *r, int id)
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
        rnode_free(t);
    }
    if (rn[rroot].count == 1 && rn[rroot].level > 0) {
        int t = rn[rroot].b[0].id;
        rnode_free(rroot);
        rroot = (int16_t)t;
    }
    return 0;
}

static int (*s_cb)(int e, void *ctx);
static void *s_ctx;
static struct rbr s_r;
static rk s_k[4];                      /* s_r's sides as keys (s_kv: computed) */
static uint8_t s_kv;
static void s_keys(void);

static PCOL_NOINLINE int s_overlap_slow(const struct rbr *b);

/* roverlap(&s_r, b) with s_r's keys at hand */
PCOL_INLINE int s_overlap(const struct rbr *b)
{
    if (b->w & s_r.w)
        return !(s_r.r[0] > b->r[2] || b->r[0] > s_r.r[2] || s_r.r[1] > b->r[3] || b->r[1] > s_r.r[3]);
    return s_overlap_slow(b);
}

/* a whole entry against a float search rectangle (s_r not whole): s_k[0] > ikey(r2) is s0 > r2 (the keys order as
   the floats, and ikey(v) is (float)v's for the entry's |v| < 2^14), that is r2 <= ceil(s0) - 1; ikey(r0) > s_k[2]
   is r0 >= floor(s2) + 1; the same in y. The four int bounds (s_ib: ceil(s0) - 1, ceil(s1) - 1, floor(s2) + 1,
   floor(s3) + 1) are computed once a search (s_keys) when |s| < 2^15; PLAY_STATS builds compare with the keys */
static int32_t s_ib[4];
static uint8_t s_iv;

static int s_kfloor(rk k, int neg, int32_t *o)              /* floor((neg ? -1 : 1) * the float of key k) */
{
    union { float f; uint32_t u; } v;
    uint32_t e;
    uint64_t pr;
    int32_t ip;
    v.u = kbits(k);
    if (neg) v.u ^= 0x80000000u;
    if ((v.u & 0x7fffffffu) == 0) { *o = 0; return 1; }
    e = (v.u >> 23) & 0xffu;
    if (e < 127) { *o = (v.u >> 31) ? -1 : 0; return 1; }
    if (e > 141) return 0;
    pr = (uint64_t)((v.u & 0x7fffffu) | 0x800000u) * fwhole_mul[e - 127];
    ip = (int32_t)(pr >> 32);
    *o = (v.u >> 31) ? ((uint32_t)pr ? -ip - 1 : -ip) : ip;
    return 1;
}

static void s_ibounds(void)
{
    int32_t f0, f1, f2, f3;
    s_iv = !s_r.w && s_kfloor(s_k[0], 1, &f0) && s_kfloor(s_k[1], 1, &f1) && s_kfloor(s_k[2], 0, &f2) &&
           s_kfloor(s_k[3], 0, &f3);
    if (s_iv) { s_ib[0] = -f0 - 1; s_ib[1] = -f1 - 1; s_ib[2] = f2 + 1; s_ib[3] = f3 + 1; }
}

static PCOL_NOINLINE int s_overlap_slow(const struct rbr *b)
{
    if (b->w) {
        if (!s_kv) s_keys();
        if (s_iv) {
            int r = !(b->r[2] <= s_ib[0] || b->r[0] >= s_ib[2] || b->r[3] <= s_ib[1] || b->r[1] >= s_ib[3]);
#ifdef PLAY_STATS
            if (r != !(s_k[0] > ikey(b->r[2]) || ikey(b->r[0]) > s_k[2] || s_k[1] > ikey(b->r[3]) || ikey(b->r[1]) > s_k[3])) {
                fprintf(stderr, "s_overlap_slow: the int bounds differ from the keys\n");
                abort();
            }
#endif
            return r;
        }
        return !(s_k[0] > ikey(b->r[2]) || ikey(b->r[0]) > s_k[2] || s_k[1] > ikey(b->r[3]) || ikey(b->r[1]) > s_k[3]);
    }
    if (!s_kv) s_keys();
    return !(s_k[0] > b->r[2] || b->r[0] > s_k[2] || s_k[1] > b->r[3] || b->r[1] > s_k[3]);
}

static void s_keys(void)
{
    int k;
    for (k = 0; k < 4; k++) s_k[k] = rkey(&s_r, k);
    s_kv = 1;
    s_ibounds();
}

static void search_run(void);           /* the tree, or pcolgrid.h in play */

static int search_rec(int n)
{
    int k;
    PCST(pcol_st.visits++);
    if (rn[n].level > 0) {
        for (k = 0; k < rn[n].count; k++)
            if (s_overlap(&rn[n].b[k]) && !search_rec(rn[n].b[k].id))
                return 0;
    } else {
        for (k = 0; k < rn[n].count; k++)
            if (s_overlap(&rn[n].b[k]) && s_cb && !s_cb(rn[n].b[k].id, s_ctx))
                return 0;
    }
    return 1;
}

/* the key of (float)v for a whole number |v| < 2^24 (exact), without soft-float: the float's bits built from the
   highest set bit (constant-shift tests) and a table multiply */
static const uint32_t ikey_mul[24] = { 1u << 23, 1u << 22, 1u << 21, 1u << 20, 1u << 19, 1u << 18, 1u << 17, 1u << 16,
                                       1u << 15, 1u << 14, 1u << 13, 1u << 12, 1u << 11, 1u << 10, 1u << 9, 1u << 8,
                                       1u << 7, 1u << 6, 1u << 5, 1u << 4, 1u << 3, 1u << 2, 1u << 1, 1u };
static rk ikey(int32_t v)
{
    uint32_t a = v < 0 ? (uint32_t)-v : (uint32_t)v, t = a, bits;
    int p = 0;
    if (a == 0) return 0;
    if (t >= 0x10000u) { t >>= 16; p += 16; }
    if (t >= 0x100u) { t >>= 8; p += 8; }
    if (t >= 0x10u) { t >>= 4; p += 4; }
    if (t >= 0x4u) { t >>= 2; p += 2; }
    if (t >= 0x2u) { p += 1; }
    bits = ((uint32_t)(p + 127) << 23) | ((a * ikey_mul[p]) & 0x7fffffu);
    if (v < 0) bits |= 0x80000000u;
    return (int32_t)bits >= 0 ? (int32_t)bits : (int32_t)(bits ^ 0x7fffffff);
}

void pcol_search_i(int32_t l, int32_t t, int32_t r, int32_t b, int (*cb)(int e, void *ctx), void *ctx)
{
    rset_i(&s_r, l, t, r, b);
    s_kv = 0;
    s_cb = cb;
    s_ctx = ctx;
    rlock = 1;
    PCST(pcol_st.searches++);
    search_run();
    rlock = 0;
}

void pcol_search(float l, float t, float r, float b, int (*cb)(int e, void *ctx), void *ctx)
{
    {
        float f[4];
        f[0] = l; f[1] = t; f[2] = r; f[3] = b;
        rset_f(&s_r, f);
        s_kv = 0;
    }
    s_cb = cb;
    s_ctx = ctx;
    rlock = 1;
    PCST(pcol_st.searches++);
    search_run();
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
/* the object tree from objdefs' parents (obj_init): an object's first child and next sibling, so a family (obj_is(o,
   b): b and its descendants) is walked by fam_obj_next in place of a test of every object (obj_anc's rows: a cache
   line each) */
int16_t pcol_ochild[OBJ_COUNT], pcol_osib[OBJ_COUNT];   /* also pworld.c's family walks (pcol_obj_tree) */

/* the object after o in a walk of root's family (preorder; -1 at the end) */
static int fam_obj_next(int root, int o)
{
    if (pcol_ochild[o] >= 0) return pcol_ochild[o];
    while (o != root) {
        if (pcol_osib[o] >= 0) return pcol_osib[o];
        o = objdefs[o].parent;
    }
    return -1;
}

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

/* pcol_handle's searcher test (can_pair, direct_pairs): for each object d, the objects whose collision events (pobj's
   lists, with the inherited ones) have a target that d is or descends from (has_col(o, d) for an instance of o and
   one of d), as lists rv_obj[rv_beg[d] .. + rv_n[d]); built by obj_init. Kept only when d's targets and list hold at
   most RV_SHORT objects (the HD tables: 311 objects, 55 entries); rv_n 255: longer (the search, as before) */
#define RV_SHORT 8
#define RV_MAX 128
#define RV_LONG 255
static uint8_t rv_beg[OBJ_COUNT], rv_n[OBJ_COUNT];
static int16_t rv_obj[RV_MAX];

static void rv_build(void)
{
    int d, o, k, n = 0;
    for (d = 0; d < OBJ_COUNT; d++) {
        int m = 0;
        rv_beg[d] = (uint8_t)n;
        for (o = 0; o < OBJ_COUNT; o++)
            for (k = 0; k < pobj[o].ncol; k++)
                if (obj_is(d, pcol[pobj[o].col0 + k])) {
                    if (m == RV_LONG || pobj[d].ncol + m + 1 > RV_SHORT || n + m == RV_MAX) m = RV_LONG;
                    else rv_obj[n + m++] = (int16_t)o;
                    break;
                }
        /* (m is RV_LONG once the list is too long: the loop above stops adding) */
        rv_n[d] = (uint8_t)(m == RV_LONG || pobj[d].ncol > RV_SHORT ? RV_LONG : m);
        if (rv_n[d] != RV_LONG) n += m;
    }
}

static void obj_init(void)
{
    int o, k;
    for (o = 0; o < OBJ_COUNT; o++) pcol_ochild[o] = pcol_osib[o] = -1;
    for (o = OBJ_COUNT - 1; o >= 0; o--) {
        int p = objdefs[o].parent;
        if (p >= 0) { pcol_osib[o] = pcol_ochild[p]; pcol_ochild[p] = (int16_t)o; }
    }
#ifdef PLAY_STATS
    {   /* the host builds check the walk against obj_is for every pair */
        int b, n, m;
        for (b = 0; b < OBJ_COUNT; b++) {
            for (n = 0, o = b; o >= 0; o = fam_obj_next(b, o)) { n++; if (!obj_is(o, b)) abort(); }
            for (m = 0, o = 0; o < OBJ_COUNT; o++) m += obj_is(o, b);
            if (n != m) { fprintf(stderr, "obj_init: family %d walks %d objects, obj_is %d\n", b, n, m); abort(); }
        }
    }
#endif
    for (o = 0; o < OBJ_COUNT; o++)
        for (k = 0; k < pobj[o].ncol; k++) otarget[pcol[pobj[o].col0 + k]] = 1;
    for (o = 0; o < OBJ_COUNT; o++) oinfo[o] = (uint8_t)(OI_DONE | (f08_of(o) ? OI_F08 : 0));
    rv_build();
    for (o = 0; o < OBJ_COUNT; o++) {
        int a;
        for (a = o; a >= 0; a = objdefs[a].parent)
            if (oinfo[a] & OI_F08) { oinfo[o] |= OI_MEMBER; break; }
    }
    oinit = 1;
}

void pcol_obj_tree(void) { if (!oinit) obj_init(); }

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
#define EF_NOSNAP 16   /* loaded with the level: its field writes are not marks until pcol.c first looks at it */
#define EF_USED 32
#define EF_OND 64      /* on the dirty list */
#define EF_ONT 128     /* on the test list */
#define EPASS_NONE 0xFF
static uint8_t gmode;                  /* a level is being generated: entries are generator instances */
static uint8_t ef[ENT_MAX];
static int16_t dn[ENT_MAX], dp[ENT_MAX], tn[ENT_MAX], tp[ENT_MAX];
static int16_t dhead = -1, tchead = -1;
static rk er[ENT_MAX][4];           /* the rectangle the entry was put in with (RemoveRect's search key) */
static uint8_t erw[ENT_MAX];           /* its form (struct rbr w) */
#include "pcolgrid.h"                   /* the shipping build's play-time grid (PCOL_EXACT: the tree) */

#ifndef PCOL_EXACT
/* pcol_search(l, t, r, b, ..) in play calls back entry e: e is in the grid and its rectangle meets the query's
   (pgrid_search's compare; the search reads every cell an entry in the grid can meet the query from). 0 in the
   generator (the tree) */
int pcol_search_has(int e, float l, float t, float r, float b)
{
    float f[4];
    if (!PCOL_GRID_ON || e < 0 || e >= PIN_MAX || pg_cell[e] == PGRID_NONE) return 0;
    f[0] = l; f[1] = t; f[2] = r; f[3] = b;
    rset_f(&s_r, f);
    s_kv = 0;
    return pg_overlap(e);
}
#endif

static void search_run(void) { if (PCOL_GRID_ON) pgrid_search(); else search_rec(rroot); }
static uint8_t epass[ENT_MAX];         /* the HandleCollision pass that entry searched in (EPASS_NONE: none since the
                                          last wrap; pass_no runs 0 .. 254, then every epass is reset) */
static uint8_t pass_no;
static int16_t pend[ENT_MAX];
static int npend;
static int32_t gen_first_id;           /* the generator's first instance_create id (room instances are below) */
static uint8_t gen_pending;            /* a level was generated: the next pw_reset loads it */

static uint8_t quiet_any;              /* some entry has EF_NOSNAP */

static int eobj(int e) { return gmode ? W.in[e].obj : PW.in[e].obj; }
/* creation order of entries: the index in the generator, the creation number in play (slots are reused) */
static int32_t ekey(int e) { return gmode ? e : pw_seq[e]; }
static int esolid(int e) { return objdefs[eobj(e)].solid; }
static int emember(int e) { return (oinfo[eobj(e)] & (OI_MEMBER | OI_DYN)) || esolid(e); }

static void dlist_remove(int e)
{
    if (!(ef[e] & EF_OND)) return;
    if (dp[e] >= 0) dn[dp[e]] = dn[e]; else dhead = dn[e];
    if (dn[e] >= 0) dp[dn[e]] = dp[e];
    ef[e] &= (uint8_t)~EF_OND;
}

static void dlist_front(int e)
{
    dlist_remove(e);
    dp[e] = -1;
    dn[e] = dhead;
    if (dhead >= 0) dp[dhead] = (int16_t)e;
    dhead = (int16_t)e;
    ef[e] |= EF_OND;
}

static void tlist_remove(int e)
{
    if (!(ef[e] & EF_ONT)) return;
    if (tp[e] >= 0) tn[tp[e]] = tn[e]; else tchead = tn[e];
    if (tn[e] >= 0) tp[tn[e]] = tp[e];
    ef[e] &= (uint8_t)~EF_ONT;
}

static void tlist_front(int e)
{
    tlist_remove(e);
    tp[e] = -1;
    tn[e] = tchead;
    if (tchead >= 0) tp[tchead] = (int16_t)e;
    tchead = (int16_t)e;
    ef[e] |= EF_ONT;
}

/* sin and cos of a float angle in radians (|a| <= 2 pi), rounded to float as glibc's sincosf: the polynomials in
   double are within 1e-16 of the true values */
static void sincos_poly(float a, float *s, float *c);

/* sincos_poly through a small cache keyed by the angle's bits: the result is a function of a alone, so a hit gives
   the same floats (a rotated instance's box is recomputed several times a step, at an angle that rarely changes;
   each evaluation is about 10 K jtcps3 clocks in soft-float double). The entry for a's bits keeps the last result */
#define SC_CACHE 8
static uint32_t sc_key[SC_CACHE];
static uint8_t sc_ok[SC_CACHE];
static float sc_s[SC_CACHE], sc_c[SC_CACHE];

static void sincos_f(float a, float *s, float *c)
{
    union { float f; uint32_t u; } k;
    unsigned h;
    k.f = a;
    h = (k.u ^ (k.u >> 11) ^ (k.u >> 19)) & (SC_CACHE - 1);
    if (!sc_ok[h] || sc_key[h] != k.u) {
        sincos_poly(a, &sc_s[h], &sc_c[h]);
        sc_key[h] = k.u;
        sc_ok[h] = 1;
    }
    *s = sc_s[h];
    *c = sc_c[h];
}

static void sincos_poly(float a, float *s, float *c)
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

/* the box as integers when the fmadds of ebbox give whole numbers: scales +-1, angle 0, whole x, y (play: pworld.c's
   cached box; generator instances: int16 x, y at scale 1) */
static int ebbox_int(int e, int32_t *ib)
{
    if (!gmode) return fzero(PW.in[e].angle) && pin_ibox(e, ib);
    if (W.in[e].spr < 0) return 0;
    {
        const struct gsprcol *g = &gsprcol[W.in[e].spr];
        ib[0] = W.in[e].x + (g->l - g->xo);
        ib[1] = W.in[e].y + (g->t - g->yo);
        ib[2] = ib[0] + (g->r - g->l + 1);
        ib[3] = ib[1] + (g->b - g->t + 1);
        return 1;
    }
}

static int fbits_pm1(float f) { union { float f; uint32_t u; } v; v.f = f; return (v.u & 0x7fffffffu) == 0x3f800000u; }

/* CInstance::Compute_BoundingBox (non-compatibility mode), normalized (CollisionUpdate): o = l, t, r, b */
static void ebbox(int e, float dx, float dy, float *o);

/* the same as a tree rectangle */
static void ebbox_rect(int e, float dx, float dy, struct rbr *o)
{
    int32_t ib[4];
    float fr[4];
    if (fkey(dx) == 0 && fkey(dy) == 0 && ebbox_int(e, ib)) {
        rset_i(o, ib[0], ib[1], ib[2], ib[3]);
        return;
    }
    ebbox(e, dx, dy, fr);
    rset_f(o, fr);
}

static void er_set(int e, const struct rbr *b)
{
    er[e][0] = b->r[0]; er[e][1] = b->r[1]; er[e][2] = b->r[2]; er[e][3] = b->r[3];
    erw[e] = (uint8_t)b->w;
}

/* RemoveRect of entry e with the rectangle it was put in with; failing that (not found), with the root's cover */
static void remove_entry(int e)
{
    struct rbr r;
    if (remove_fast(e)) return;
    r.r[0] = er[e][0]; r.r[1] = er[e][1]; r.r[2] = er[e][2]; r.r[3] = er[e][3];
    r.w = erw[e];
    if (remove_rect(&r, e)) {
        rcover(rroot, &r);
        remove_rect(&r, e);
    }
}

/* Compute_BoundingBox of play instance i as the runner keeps it (floats; rotated by image_angle): pworld.c's boxes
   of rotated instances */
void pcol_box(int i, float *o);

static void ebbox(int e, float dx, float dy, float *o)
{
    float x, y, xs = 1, ys = 1, ang = 0, w, h, t0, t1;
    int s;
    const struct gsprcol *c;
    if (fkey(dx) == 0 && fkey(dy) == 0) {
        int32_t ib[4];
        if (ebbox_int(e, ib)) {
            o[0] = (float)ib[0]; o[1] = (float)ib[1]; o[2] = (float)ib[2]; o[3] = (float)ib[3];
            return;
        }
    }
    if (!gmode) {
        const struct pin *p = &PW.in[e];
        s = p->mask >= 0 ? p->mask : p->spr;
        x = (float)PTOD(p->x);
        y = (float)PTOD(p->y);
        xs = (float)p->xscale;
        ys = (float)p->yscale;
        ang = (float)p->angle;
    } else {
        const struct inst *g = &W.in[e];
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
        /* a scale of +-1 (its bits): fmaf(a, +-1, c) is one rounding of the exact +-a + c, the float sum c + (+-a)
           (the software fmaf is about 700 jtcps3 clocks; debris and most sprites have scale 1) */
        if (fbits_pm1(xs)) {
            float a0 = (float)(c->l - c->xo);
            l = x + (xs > 0 ? a0 : -a0);
            r = l + (xs > 0 ? w : -w);
        } else {
            l = fmaf((float)(c->l - c->xo), xs, x);
            r = fmaf(w, xs, l);
        }
        if (fbits_pm1(ys)) {
            float a1 = (float)(c->t - c->yo);
            t = y + (ys > 0 ? a1 : -a1);
            b = t + (ys > 0 ? h : -h);
        } else {
            t = fmaf((float)(c->t - c->yo), ys, y);
            b = fmaf(h, ys, t);
        }
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

void pcol_box(int i, float *o) { ebbox(i, 0, 0, o); }
void pcol_sincosf(float a, float *s, float *c) { sincos_f(a, s, c); }

/* RemoveRect of entry e when its leaf keeps RMIN or more branches: RemoveRectRec finds e in its leaf (the one
   holding it: the covers on its path overlap its rectangle) and takes it out by swapping in the leaf's last branch;
   no node underflows, so nothing is reinserted, and on the way back each node of the path gets its child's cover
   (rcover). The same here from the leaf up, without the search. 0: not done (the leaf would underflow) */
static int remove_fast(int e)
{
    int n = eleaf[e], k;
    struct rnode *p = &rn[n];
    if (p->count <= RMIN) return 0;
    for (k = 0; k < p->count && p->b[k].id != e; k++) {}
    if (k == p->count) return 0;
    disconnect(n, k);
    while (n != rroot) {
        int up = npar[n];
        struct rnode *q = &rn[up];
        for (k = 0; q->b[k].id != n; k++) {}
        rcover(n, &q->b[k]);
        n = up;
    }
    return 1;
}

/* CollisionUpdate: take the entry out (if in) and put it in with its current box */
static void cupdate_at(int e, float dx, float dy)
{
    struct rbr b;
    if (!(ef[e] & EF_TREE) && edead(e)) return;
    if (rlock) return;
    if (!emember(e)) return;
    ebbox_rect(e, dx, dy, &b);
    if (PCOL_GRID_ON) {                           /* pcolgrid.h */
        PCST(pcol_st.inserts++);
        er_set(e, &b);
        ef[e] |= EF_TREE;
        pgrid_put(e);
        return;
    }
    if (ef[e] & EF_TREE) {
        PCST(pcol_st.removes++);
        remove_entry(e);
    }
    b.id = (int16_t)e;
    PCST(pcol_st.inserts++);
    insert_rect(&b, 0);
    er_set(e, &b);
    ef[e] |= EF_TREE;
}

static void cupdate(int e) { cupdate_at(e, 0, 0); }

/* CollisionMarkDirty (with the stale bounding box flag its callers set) */
/* the stale tree members (EF_STALE, on the dirty list): pushed when they become stale; entries no longer stale and
   repeats are dropped when the stack is read (stk_compact). A scan that computes the boxes of an object's instances
   in creation order (collision_point and the like) only changes the tree through these */
static int16_t stk[ENT_MAX];
static int nstk;
#ifdef PLAY_STATS
/* the host builds: the stack as it was before stk_clean (pushed as stk, cut back only when read or full); every read
   compares the two compacted stacks */
static int16_t ostk[ENT_MAX];
static int onstk;
#endif

static int stk_compact_a(int16_t *s, int m)
{
    int k, j, n = 0;
    for (k = 0; k < m; k++) {
        int16_t e = s[k];
        if (!(ef[e] & EF_STALE)) continue;
        for (j = n; j > 0 && ekey(s[j - 1]) > ekey(e); j--) s[j] = s[j - 1];
        if (j > 0 && s[j - 1] == e) {                   /* a repeat: undo the shift */
            for (; j < n; j++) s[j] = s[j + 1];
            continue;
        }
        s[j] = e;
        n++;
    }
    return n;
}

static void stk_compact(void)
{
    nstk = stk_compact_a(stk, nstk);
#ifdef PLAY_STATS
    {
        int k;
        onstk = stk_compact_a(ostk, onstk);
        for (k = 0; k < nstk && k < onstk && stk[k] == ostk[k]; k++) {}
        if (k != nstk || k != onstk) {
            fprintf(stderr, "stk_compact: the cleaned stack differs (%d of %d / %d)\n", k, nstk, onstk);
            abort();
        }
    }
#endif
}

/* after a flush: the stack keeps only the entries still stale, in order (dead ones waiting for remove_marked). The
   reads (stk_compact) are unchanged: they keep only the stale entries, and one dropped here that becomes stale again
   is pushed again by mark_e (it pushes whenever it sets EF_STALE on a member; membership does not end within a room:
   OI_DYN is only set, solid is the object's, and a reused slot starts at ef 0). Without this the stack held every
   mark since its last read, up to ENT_MAX, and the next read (a creation-order scan of an oSolid child: a spear
   trap's point test) sorted through them all */
static void stk_clean(void)
{
    int k, n = 0;
    for (k = 0; k < nstk; k++)
        if (ef[stk[k]] & EF_STALE) stk[n++] = stk[k];
    nstk = n;
}

static void mark_e(int e)
{
    int o = eobj(e);
    if ((oinfo[o] & (OI_MEMBER | OI_DYN)) || esolid(e)) {
        if (!(ef[e] & EF_STALE)) {
            if (nstk == ENT_MAX) stk_compact();
            stk[nstk++] = (int16_t)e;
#ifdef PLAY_STATS
            if (onstk == ENT_MAX) onstk = stk_compact_a(ostk, onstk);
            ostk[onstk++] = (int16_t)e;
#endif
        }
        ef[e] |= EF_STALE;
        dlist_front(e);
    } else
        ef[e] |= EF_STALE;
    if (oinfo[o] & OI_F08) tlist_front(e);
}

/* ---- changes of play instances: the pin_set* setters (play.h) call pcol_changed on a real change of x, y,
   sprite_index, mask_index, image_xscale / yscale / angle (SetPosition, SetSpriteIndex, ...: CollisionMarkDirty
   then). An instance loaded with the level (EF_NOSNAP) takes its loader's writes without a mark until pcol.c
   first looks at it (sync1) or any UpdateTree (sync_all) */
void pcol_changed(int i)
{
    if (!PW.in[i].alive || (ef[i] & EF_NOSNAP)) return;
    mark_e(i);
}

static void sync1(int i)
{
    ef[i] &= (uint8_t)~EF_NOSNAP;
}

static void sync_all(void)
{
    int i;
    if (!quiet_any) return;
    PCST(pcol_st.syncs++);
    for (i = 0; i < PW.n; i++)
        if (PW.in[i].alive) sync1(i);
    quiet_any = 0;
}

/* UpdateTree */
static __attribute__((noinline)) void flush_run(void)
{
    sync_all();
    PCST(pcol_st.flushes++);
    while (dhead >= 0) {
        int e = dhead;
        dlist_remove(e);
        if (!edead(e)) {
            ef[e] &= (uint8_t)~EF_STALE;
            cupdate(e);
        }
    }
    if (nstk > 32) stk_clean();                   /* (a short stack is left for stk_compact) */
}

static inline void flush(void)
{
    if (quiet_any || dhead >= 0) flush_run();
    else PCST(pcol_st.flushes++);                  /* (flush_run counts it otherwise) */
}

static void touch_e(int e)
{
    if (!gmode) sync1(e);
    if (ef[e] & EF_STALE) {
        ef[e] &= (uint8_t)~EF_STALE;
        cupdate(e);
    }
}

void pcol_touch(int i) { touch_e(i); }

/* the touches of a creation-order scan of obj's instances (but notme) up to and including `upto` (NOONE: all) that
   do something: those of stale tree members, which all are on the dirty list; in creation order (the scans of
   pworld.c that find their hit another way). Not exact while some entry is quiet (quiet_any: sync1 there clears
   EF_NOSNAP too): the caller scans then */
int pcol_quiet(void) { return quiet_any; }

/* the level is loaded (play_level_start): the loader's writes are done; from here every change is a mark */
void pcol_load_done(void) { sync_all(); }

/* the touches of a creation-order scan of obj's instances (but notme) up to and including `upto` (NOONE: all) that
   change the tree: those of the stale members, in creation order */
static void touch_stale(int obj, int notme, int upto)
{
    int k;
    stk_compact();
    for (k = 0; k < nstk; k++) {
        int e = stk[k], alive = gmode ? W.in[e].alive : PW.in[e].alive;
        if (upto >= 0 && ekey(e) > ekey(upto)) break;
        if (e == notme || !alive || !obj_is(eobj(e), obj) || !(ef[e] & EF_STALE)) continue;
        touch_e(e);
    }
}

void pcol_touch_stale(int obj, int notme, int upto) { touch_stale(obj, notme, upto); }

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
    sync1(i);
    mark_e(i);
}

static void entry_clear(int e)
{
    dlist_remove(e);
    tlist_remove(e);
    if (PCOL_GRID_ON) pgrid_out(e);
    ef[e] = 0;
    epass[e] = EPASS_NONE;
}

/* ---- rooms --------------------------------------------------------------------------------------------------- */
static void room_reset(void)
{
    int e, o;
    if (!oinit) obj_init();
    rt_reset();
    for (e = 0; e < ENT_MAX; e++) {
        ef[e] = 0;
        epass[e] = EPASS_NONE;
    }
    dhead = tchead = -1;
    npend = 0;
    nstk = 0;
#ifdef PLAY_STATS
    onstk = 0;
#endif
    for (o = 0; o < OBJ_COUNT; o++) {
        oinfo[o] &= (uint8_t)~OI_DYN;
        ocnt[o] = 0;
    }
    rlock = 0;
    pgrid_clear();
}

static void rebuild_all(void);

/* CRoom::RemoveMarked */
static void remove_marked(void)
{
    int k, j, many = npend >= 251;
    for (k = 1; k < npend; k++) {                     /* creation order (the active list) */
        int16_t v = pend[k];
        for (j = k; j > 0 && ekey(pend[j - 1]) > ekey(v); j--) pend[j] = pend[j - 1];
        pend[j] = v;
    }
    for (k = 0; k < npend; k++) {
        int e = pend[k];
        if ((ef[e] & EF_TREE) && !many && !PCOL_GRID_ON) {
            PCST(pcol_st.removes++);
            remove_entry(e);
        }
        obj_count(eobj(e), -1);
        entry_clear(e);
        if (!gmode) pw_removed(e);                /* the instance's memory goes (struct pin_ext) */
    }
    npend = 0;
    if (many) rebuild_all();
}

/* RebuildTree(false): every instance in creation order: marked dirty, put in */
static void rebuild_all(void)
{
    int e, j;
    rt_reset();
    if (PCOL_GRID_ON) pgrid_clear();
    for (e = 0; e < ENT_MAX; e++) ef[e] &= (uint8_t)~EF_TREE;
    for (j = 0; j < (gmode ? W.n : PW.nord); j++)
        if (e = gmode ? j : pw_ord[j], (gmode ? W.in[e].alive : PW.in[e].alive) && !edead(e)) {
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

/* the generated level (entries w): RemoveMarked after the room's Create events, then the alive ones are renamed to
   their play index (play_level_start adds them in creation order), in place: map[w] <= w, so ascending w reads each
   entry before anything is written over it */
static void gen_load(void)
{
    int16_t *map = pend;                          /* free: remove_marked empties it */
    int w, n = 0, k, e;
    remove_marked();
    for (w = 0; w < W.n; w++) map[w] = (int16_t)(W.in[w].alive ? n++ : -1);
#define GMAP(x) ((x) < 0 ? (int16_t)-1 : map[x])
    for (k = 0; k < RT_NODES; k++) {              /* tree leaves */
        int j;
        if (rn[k].level != 0) continue;
        for (j = 0; j < rn[k].count; j++) rn[k].b[j].id = GMAP(rn[k].b[j].id);
    }
    for (w = 0; w < W.n; w++) {                   /* entries, with their dirty / test list links */
        int i = map[w];
        if (i < 0) continue;
        ef[i] = (uint8_t)((ef[w] & (EF_TREE | EF_STALE | EF_OND | EF_ONT)) | EF_GEN | EF_NOSNAP | EF_USED);
        er[i][0] = er[w][0]; er[i][1] = er[w][1]; er[i][2] = er[w][2]; er[i][3] = er[w][3];
        erw[i] = erw[w];
        eleaf[i] = eleaf[w];
        epass[i] = epass[w];
        dn[i] = GMAP(dn[w]); dp[i] = GMAP(dp[w]); tn[i] = GMAP(tn[w]); tp[i] = GMAP(tp[w]);
    }
    dhead = GMAP(dhead);
    tchead = GMAP(tchead);
#undef GMAP
    for (e = n; e < ENT_MAX; e++) { ef[e] = 0; epass[e] = EPASS_NONE; }
    nstk = 0;                                     /* the stale stack, renamed: the stale ones in order */
    for (e = 0; e < n; e++) if ((ef[e] & (EF_STALE | EF_OND)) == (EF_STALE | EF_OND)) stk[nstk++] = (int16_t)e;
#ifdef PLAY_STATS
    for (onstk = 0; onstk < nstk; onstk++) ostk[onstk] = stk[onstk];
#endif
    quiet_any = 1;
    gmode = 0;
    if (PCOL_GRID_ON) pgrid_load(n);
}

void pcol_after_reset(void)
{
    if (gen_pending) {
        gen_pending = 0;
        gen_load();
    } else {
        room_reset();
        gmode = 0;
    }
}

void pcol_added(int i)
{
    if (ef[i] & EF_GEN) {                         /* a generated instance: its state came with it */
        ef[i] &= (uint8_t)~EF_GEN;
        return;
    }
    entry_clear(i);
    ef[i] = EF_USED;
    obj_count(PW.in[i].obj, 1);
}

/* instance_create: the CInstance constructor marks it dirty (SetObjectIndex, SetSpriteIndex: CollisionMarkDirty;
   Observed: tools/treeprobe.py, 3 creations in an empty room leave the leaf [1, 2, 0]: put in at creation, put in
   again newest first by the next UpdateTree), then CRoom::AddInstance's CollisionInsert puts it in */
void pcol_create(int i)
{
    mark_e(i);
    ef[i] &= (uint8_t)~EF_STALE;
    cupdate(i);                                   /* Compute_BoundingBox(true) */
    if (oinfo[PW.in[i].obj] & OI_F08) tlist_front(i);
}

/* off-view deactivation (pworld.c pw_deactivate). The runner leaves a deactivated instance's entry in the tree, where
   searches pass over it (its instance is not alive), and puts it in again when it is activated (Observed:
   build/trace/dz_tree_oTreasure / dz_tree_oItem, p5_caveman seed 863 TRACE_DEACT=32 with TRACE_TREE at 13 records:
   the search order equal with this, different when the entry leaves the tree at deactivation or stays where it was
   at activation; docs/DEACT.md 2). Off the dirty and test lists and out of the object's count meanwhile; the grid
   (pcolgrid.h) takes it out, as its search has no alive test */
void pcol_deactivated(int i)
{
    uint8_t keep = ef[i] & EF_TREE;
    obj_count(PW.in[i].obj, -1);
    entry_clear(i);
    if (!PCOL_GRID_ON) ef[i] = keep;
}

/* pw_activate: put in again as instance_create puts a new instance in (pcol_added's count, pcol_create: marked dirty,
   CollisionUpdate now). Marked dirty only, or put in only, gave the same search order on the probes above */
void pcol_activated(int i)
{
    uint8_t keep = ef[i] & EF_TREE;
    entry_clear(i);
    ef[i] = (uint8_t)(EF_USED | keep);
    obj_count(PW.in[i].obj, 1);
    pcol_create(i);
}

void pcol_room_inst(int i)
{
    ef[i] |= EF_NOSNAP;                           /* the loader sets its fields after pin_add */
    quiet_any = 1;
    mark_e(i);
}

/* ---- collision functions ------------------------------------------------------------------------------------- */
static void set_dyn(int obj)
{
    int o;
    for (o = obj; o >= 0; o = fam_obj_next(obj, o))           /* the objects o with obj_is(o, obj) */
        if (!(oinfo[o] & (OI_MEMBER | OI_DYN))) oinfo[o] |= OI_DYN;
}

/* the grid build's query_dyn: the instances of obj's family from each member object's instance list (pworld.c
   pw_ohead / pw_inext: every alive instance of the object) instead of a scan of every entry in creation order. The
   same entries go in (alive, not dead, of the family, not yet in the tree) with the same writes; only the order of
   pgrid_put differs, and pgrid_search orders what it collects by pw_seq. PLAY_STATS builds count the scan's entries
   first and abort on a different number */
static void query_dyn_grid(int obj)
{
    int o, i, put = 0;
#ifdef PLAY_STATS
    int e, want = 0;
    for (e = 0; e < PW.nord; e++) {
        int ent = pw_ord[e];
        if (PW.in[ent].alive && !edead(ent) && obj_is(eobj(ent), obj) && !(ef[ent] & EF_TREE)) want++;
    }
#endif
    for (o = obj; o >= 0; o = fam_obj_next(obj, o)) {        /* the objects o with obj_is(o, obj) */
        for (i = pw_ohead[o]; i >= 0; i = pw_inext[i]) {
            struct rbr b;
            if (!PW.in[i].alive || edead(i) || (ef[i] & EF_TREE)) continue;
            sync1(i);
            ef[i] &= (uint8_t)~EF_STALE;          /* Compute_BoundingBox(false) */
            ebbox_rect(i, 0, 0, &b);
            b.id = (int16_t)i;
            PCST(pcol_st.inserts++);
            er_set(i, &b);
            ef[i] |= EF_TREE;
            pgrid_put(i);
            put++;
        }
    }
#ifdef PLAY_STATS
    if (put != want) { fprintf(stderr, "query_dyn_grid: %d entries, the scan %d (obj %d)\n", put, want, obj); abort(); }
#endif
    (void)put;
}

/* query_e's first query of an object that is not yet in the tree: its instances go in */
static __attribute__((noinline)) void query_dyn(int obj, int gen)
{
    if (!gen && PCOL_GRID_ON) {
        set_dyn(obj);
        query_dyn_grid(obj);
        return;
    }
    {
        int e, n = gen ? W.n : PW.nord;
        set_dyn(obj);
        for (e = 0; e < n; e++) {
            int ent = gen ? e : pw_ord[e];
            int alive = gen ? W.in[ent].alive : PW.in[ent].alive;
            struct rbr b;
            if (!alive || edead(ent) || !obj_is(eobj(ent), obj) || (ef[ent] & EF_TREE)) continue;
            if (!gen) sync1(ent);
            ef[ent] &= (uint8_t)~EF_STALE;        /* Compute_BoundingBox(false) */
            ebbox_rect(ent, 0, 0, &b);
            b.id = (int16_t)ent;
            PCST(pcol_st.inserts++);
            if (!gen && PCOL_GRID_ON) {
                er_set(ent, &b);
                ef[ent] |= EF_TREE;
                pgrid_put(ent);
                continue;
            }
            insert_rect(&b, 0);
            er_set(ent, &b);
            ef[ent] |= EF_TREE;
        }
    }
}

static __attribute__((noinline)) void gen_flush(void)
{
    while (dhead >= 0) {
        int e = dhead;
        dlist_remove(e);
        if (!edead(e)) {
            ef[e] &= (uint8_t)~EF_STALE;
            cupdate(e);
        }
    }
}

/* ShouldUseFastCollision, and UpdateTree when it says 1 (inline: the common answer needs no frame) */
static inline int query_e(int obj, int gen)
{
    int cnt = ocnt[obj];
    if (cnt == 0) return -1;
    if (cnt < rn[rroot].level) return 2;
    if (!(oinfo[obj] & (OI_MEMBER | OI_DYN))) query_dyn(obj, gen);
    if (gen) gen_flush();
    else flush();
    return 1;
}

int pcol_query(int obj) { return query_e(obj, 0); }

/* pcol_query(obj)'s answer without the flush (pk_swamp.c's idle piranha batch decides before any write which path
   line_any takes): 0 when pcol_query would first make obj dynamic (query_dyn) */
int pcol_query_kind(int obj)
{
    int cnt = ocnt[obj];
    if (cnt == 0) return -1;
    if (cnt < rn[rroot].level) return 2;
    return (oinfo[obj] & (OI_MEMBER | OI_DYN)) ? 1 : 0;
}

int pcol_count(int obj) { return ocnt[obj]; }

/* HandleCollision */
/* collision pairs in one pass: at most 28 seen (the routes, and 300 idle steps with enemies over 20 seeds x 16
   levels); more stops the play loop with untranslated code 9102 */
#define PAIRS_MAX 256
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
    if (has_col(hc_self, e) || has_col(e, hc_self)) {
        if (npairs == PAIRS_MAX) {
            PUNTR(9102);
            return 0;
        }
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

/* whether searcher s can make a pair: collision_result keeps a hit e only when has_col(s, e) (e's object is or
   descends from a target t of s's: ocnt[t] counts e) or has_col(e, s) (e's object is in s's rv list: its ocnt counts
   e). ocnt counts an entry from pcol_added to remove_marked, so every entry a search can meet. With all those counts
   0 the search records nothing and has no other effect, so pcol_handle skips it (the explosion's rubble: no target,
   oWeb the only object with an event on it) */
static int can_pair(int s)
{
    int os = PW.in[s].obj, k, n;
    const struct pobj *q = &pobj[os];
    if (rv_n[os] == RV_LONG) return 1;            /* long lists (oPlayer1's 42): search */
    for (k = 0; k < q->ncol; k++) if (ocnt[pcol[q->col0 + k]]) return 1;
    for (k = rv_beg[os], n = k + rv_n[os]; k < n; k++) if (ocnt[rv_obj[k]]) return 1;
    return 0;
}

#ifndef PCOL_EXACT
/* pcol_handle's pairs of searcher s (s_r set, s_kv 0) without the grid search, when its lists are short (can_pair's
   bound): the grid search calls collision_result for every grid entry whose rectangle overlaps s_r (pg_overlap: the
   predicate it applies; the grid finds every such entry), newest first (pw_seq descending), and collision_result
   keeps only entries that can pair with s: alive instances of a target's family or of an object on s's rv list
   (pworld.c's instance lists pw_ohead / pw_inext), or destroyed entries still in the grid (pend). Here those
   candidates, the grid's members among them that overlap s_r, sorted newest first, go to collision_result in that
   order: the pairs it keeps, and their order, are the search's. 0 (nothing kept, the caller searches): more than
   DP_MAX candidates, or two of them with one creation number. PLAY_STATS builds also search and compare */
#define DP_MAX 16
static int direct_pairs(int s)
{
    int os = PW.in[s].obj, k, j, n = 0, o, i, t;
    const struct pobj *q = &pobj[os];
    int16_t c[DP_MAX];
    /* ocnt bounds each list's length (it counts the alive instances and the destroyed entries still in the grid):
       a sum over DP_MAX goes to the search before any list is read (p5_reg_l9s5: oEnemySight's hundreds) */
    for (k = n = 0; k < q->ncol; k++) n += ocnt[pcol[q->col0 + k]];
    for (k = rv_beg[os]; k < rv_beg[os] + rv_n[os]; k++) n += ocnt[rv_obj[k]];
    if (n > DP_MAX) return 0;
    n = 0;
    for (k = 0; k < q->ncol; k++) {
        t = pcol[q->col0 + k];
        if (!ocnt[t]) continue;
        for (o = t; o >= 0; o = fam_obj_next(t, o))
            for (i = pw_ohead[o]; i >= 0; i = pw_inext[i]) {
                if (n == DP_MAX) return 0;
                c[n++] = (int16_t)i;
            }
    }
    for (k = rv_beg[os]; k < rv_beg[os] + rv_n[os]; k++) {
        o = rv_obj[k];
        if (!ocnt[o]) continue;
        for (i = pw_ohead[o]; i >= 0; i = pw_inext[i]) {
            if (n == DP_MAX) return 0;
            c[n++] = (int16_t)i;
        }
    }
    for (k = 0; k < npend; k++) {
        int e = pend[k];
        if (!(ef[e] & EF_TREE) || !(has_col(s, e) || has_col(e, s))) continue;
        if (n == DP_MAX) return 0;
        c[n++] = (int16_t)e;
    }
    for (k = j = 0; k < n; k++) {                 /* the grid's members that overlap s_r */
        int e = c[k];
        if ((ef[e] & EF_TREE) && pg_overlap(e)) c[j++] = (int16_t)e;
    }
    n = j;
    for (k = 1; k < n; k++) {                     /* newest first */
        int16_t v = c[k];
        for (j = k; j > 0 && pw_seq[c[j - 1]] < pw_seq[v]; j--) c[j] = c[j - 1];
        c[j] = v;
    }
    for (k = j = 0; k < n; k++) {                 /* repeats (an instance in two lists) out */
        if (j > 0 && c[j - 1] == c[k]) continue;
        if (j > 0 && pw_seq[c[j - 1]] == pw_seq[c[k]]) return 0;
        c[j++] = c[k];
    }
    for (k = 0; k < j; k++)
        if (!collision_result(c[k], 0)) break;
    return 1;
}
#endif

void pcol_handle(void)
{
    int k, nkeep = 0;
    static int16_t keep[PIN_MAX];
    npairs = 0;
    flush();
    while (tchead >= 0) {
        int s = tchead;
        tlist_remove(s);
        if (edead(s) || !PW.in[s].alive) continue;
#ifndef PLAY_STATS
        if (!can_pair(s)) goto searched;
#else
        int skip = !can_pair(s), np0 = npairs;    /* the host builds search anyway and check that nothing was kept */
#endif
        /* the search rectangle of its box: as pcol_search with ebbox's floats (rset_f takes whole ones as ints). A
           tree member that is not stale has it as its tree rectangle (cupdate_at put ebbox_rect(s, 0, 0) there, and
           every change of its box since would have marked it stale; flush ran above, and nothing moves in this loop) */
        if ((ef[s] & (EF_TREE | EF_STALE)) == EF_TREE) {
            s_r.r[0] = er[s][0]; s_r.r[1] = er[s][1]; s_r.r[2] = er[s][2]; s_r.r[3] = er[s][3];
            s_r.w = erw[s];
#ifdef PLAY_STATS
            {   /* the host builds check the premise */
                struct rbr c;
                ebbox_rect(s, 0, 0, &c);
                if (c.w != s_r.w || c.r[0] != s_r.r[0] || c.r[1] != s_r.r[1] || c.r[2] != s_r.r[2] || c.r[3] != s_r.r[3]) {
                    fprintf(stderr, "pcol_handle: entry %d's tree rectangle is not its box\n", s);
                    abort();
                }
            }
#endif
        } else
            ebbox_rect(s, 0, 0, &s_r);
        s_kv = 0;
        hc_self = s;
        s_cb = collision_result;
        s_ctx = 0;
        rlock = 1;
        PCST(pcol_st.searches++);
#ifndef PCOL_EXACT
        if (PCOL_GRID_ON && rv_n[PW.in[s].obj] != RV_LONG) {
            int np1 = npairs;
            if (!direct_pairs(s)) search_run();
#ifdef PLAY_STATS
            else {                                /* the host builds search too and compare the pairs */
                int16_t qa[PAIRS_MAX], qb[PAIRS_MAX];
                int nd = npairs, m;
                for (m = np1; m < nd; m++) { qa[m] = pa[m]; qb[m] = pb[m]; }
                npairs = np1;
                search_run();
                if (npairs != nd) { fprintf(stderr, "pcol_handle: direct_pairs %d pairs, the search %d (entry %d)\n", nd - np1, npairs - np1, s); abort(); }
                for (m = np1; m < nd; m++)
                    if (qa[m] != pa[m] || qb[m] != pb[m]) { fprintf(stderr, "pcol_handle: direct_pairs' pair %d differs (entry %d)\n", m, s); abort(); }
            }
#endif
            (void)np1;
        } else
#endif
        search_run();                             /* the tree, or pcolgrid.h in play */
        rlock = 0;
#ifdef PLAY_STATS
        if (skip && npairs != np0) {
            fprintf(stderr, "pcol_handle: entry %d (object %d) paired though can_pair said no\n", s, PW.in[s].obj);
            abort();
        }
#else
    searched:
#endif
        if (keeps_testing(s)) {                   /* pushed on the front of a local list */
            for (k = nkeep; k > 0; k--) keep[k] = keep[k - 1];
            keep[0] = (int16_t)s;
            nkeep++;
        }
        epass[s] = pass_no;
    }
    PCST((uint32_t)npairs > pcol_st.pairs_max ? (pcol_st.pairs_max = (uint32_t)npairs) : 0);
    if (++pass_no == EPASS_NONE) {                /* wrap: no entry has searched in the passes to come */
        int e;
        for (e = 0; e < ENT_MAX; e++) epass[e] = EPASS_NONE;
        pass_no = 0;
    }
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
    if (!gmode && PW.in[e].alive && !edead(e) && obj_is(PW.in[e].obj, pr_obj) && pr_n < pr_max)
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
    int e = w, k;
    switch (op) {
    case IH_RESET:                                /* StartRoom: RebuildTree(true) */
        room_reset();
        gmode = 1;
        gen_first_id = a;
        gen_pending = 1;
        break;
    case IH_CREATE:
        ef[e] = EF_USED;
        epass[e] = EPASS_NONE;
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
        touch_stale(a, NOONE, op == IH_POINT ? w : NOONE);
        break;
    case IH_RECT:                                 /* collision_rectangle(.., a = obj) = w */
        if (query_e(a, 1) == 2) touch_stale(a, NOONE, w);
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
                touch_e(k);
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
