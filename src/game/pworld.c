/* Play world: instances and GameMaker 2024.14's collision functions (rules and evidence: play.h).
 * Searches return the oldest matching instance (P2: collision_point, instance_place, instance_find, obj.var). */
#include <stddef.h>               /* offsetof: the reach checks below */
#include "play.h"
#include "pint.h"                 /* PL (pw_release) */
#ifdef PLAY_STATS
#include <stdio.h>
#include <stdlib.h>
#endif
#include "pcol.h"
#include "inst.h"                 /* GRID_W, GRID_H: the solid grid covers the generator's level grid */
#include "pmath.h"                /* psqrt, pdist2_lt */

/* the runner's float arithmetic as written (the precise collision code below follows its instruction order) */
#ifdef __clang__
#pragma STDC FP_CONTRACT OFF
#endif

/* the generator's instances, then the play instances in the same memory (play.h struct pworld) */
/* struct pin is 64 bytes (a shift indexes PW.in, not a mul.l) and struct inst 72: play slot i ends at byte 64 i + 64 <=
   72 (i + 1), inside the generator instances 0 .. i, which the loaders have read (play.h) */
typedef char pin_size_is_64[sizeof(struct pin) == 64 ? 1 : -1];
/* the reach of the SH-2's displacement loads (play.h): mov.w @(disp,Rn) to 30, mov.l to 60 */
typedef char pin_int16_reach[offsetof(struct pin, iy) <= 30 && offsetof(struct pin, bb) <= 30 && offsetof(struct pin, obj) <= 30 ? 1 : -1];
typedef char pin_int32_reach[offsetof(struct pin, angle) <= 60 ? 1 : -1];
typedef char pin_ext_size_8[sizeof(struct pin_ext) % 8 == 0 && EXT_MAX * (sizeof(struct pin_ext) / 8) < 32768 ? 1 : -1];   /* exto */
typedef char pin_size_le_inst_size[sizeof(struct pin) <= sizeof(struct inst) ? 1 : -1];
#define INST_MEM_PIN ((PIN_MAX * sizeof(struct pin) + sizeof(struct inst) - 1) / sizeof(struct inst))
#define INST_MEM_N (INST_MEM_PIN > INST_MAX ? INST_MEM_PIN : INST_MAX)
/* every generated instance gets a play slot (play_level_start, pcol.c gen_load: the grid's arrays are PIN_MAX long), with
   PIN_DEAD left over */
typedef char pin_max_covers_inst_max[PIN_MAX > INST_MAX ? 1 : -1];
struct inst inst_mem[INST_MEM_N];
struct pworld pw_mem = { .in = (struct pin *)inst_mem };   /* PW (play.h) */
#ifdef PLAY_STATS
struct pw_stats pw_st;
#endif

static int spr_of(const struct pin *p) { return p->mask >= 0 ? p->mask : p->spr; }
static void grid_reset(void);
static void grid_unlink(int i);
static void grid_dirty(int i);
static inline void grid_dirty_g(int i, int o, int g);
/* per object, one byte for pw_changed's two tests (one cache line read a move):
   - bits 0-2, gobj: computed at the object's first grid_dirty (after the first level start: xbits is set then): bit 1
     its instances go in the grid (obj_is(o, oSolid) || !pin_needs_ext(o): constant per object), bit 2 xbits[o] != 0
     (the static-family index; constant), bit 0 computed;
   - bits 4-7, nc_ofam (below) */
static uint8_t pwob[OBJ_COUNT];
#define GOBJ(o) (pwob[o] & 7)
static void grid_flush_run(void);
static int16_t ghead[GRID_H][GRID_W];       /* the oSolid family (point queries) */
static int16_t thead[GRID_H][GRID_W];       /* the other terrain (the drawing only) */
static int tmaxw, tmaxh;
static int16_t gnext[PIN_MAX], gcell[PIN_MAX], gdnext[PIN_MAX];
static uint8_t gond[PIN_MAX];
static int16_t gdhead = NOONE;
static inline void grid_flush(void) { if (gdhead >= 0) grid_flush_run(); }   /* the pending grid updates */
static int gmaxw, gmaxh;
#define GCELL_FAR (-2)                           /* gcell: an oSolid-family box too far out for the grid */
static int gfar;                                 /* how many (the grid's line query falls back to the tree then) */
/* the line queries' cell summary of the oSolid-family entries in ghead (solid_vline_any, solid_hline_any: line_any): a cell block is an
   entry whose integer box is exactly one in-grid cell ([16 cx, 16 cx + 16) x [16 cy, 16 cy + 16)); gfull counts them
   per cell (gfblk: one of them), gother counts the other entries per cell their box may reach (the cells of
   [l - 1, r] x [t - 1, b], clamped as the queries clamp: a superset of the cells where line_hit can hit them) */
static uint8_t gfull[GRID_H][GRID_W];
static int16_t gfblk[GRID_H][GRID_W];
static uint16_t gother[GRID_H][GRID_W];
static uint8_t gkind[PIN_MAX], gox0[PIN_MAX], goy0[PIN_MAX], gox1[PIN_MAX], goy1[PIN_MAX];   /* 1 block, 2 other */
/* the resting-object skip (pobj.c, the grid build): gclock counts the summary's changes, gver is a cell's last one
   (an oSolid-family entry put in or taken out of the cells it may reach), both 16-bit within an epoch: when gclock
   would wrap, gepoch advances and every gver restarts at 0, and a record of an earlier epoch is never still */
static uint16_t gclock = 1, gepoch, gver[GRID_H][GRID_W];
/* the static-family index (collision_point_p, collision_rect_p / _i): for each family of xf_obj (objects that do not
   move by moveTo's raw writes: liquids, ladders, spikes, webs, the jungle's trees and leaves), how many of its entries' integer boxes reach each
   cell ([l, r - 1] x [t, b - 1] in cells, clamped), kept like the solid grid (an entry whose box may have changed
   waits on xdhead until the next query); a cell with no count holds no point of the family. xfar counts the entries
   with a box that is not BB_INT, xsat a cell count past 255 (then every query of the family takes the search).
   A destroyed or killed entry goes on xdhead and leaves the counts at the next query (xflush_run: not alive) */
#ifndef PCOL_EXACT
#define XF_N 8
/* family 7 counts oTree's and oLeaves' instances together: for a query of either it is a superset count, and
   xpoint_any tests only the query's own family */
static const int16_t xf_obj[XF_N] = { OBJ_oLava, OBJ_oWater, OBJ_oWaterSwim, OBJ_oLadder, OBJ_oLadderTop, OBJ_oSpikes,
                                      OBJ_oWeb, OBJ_oTree };
static const uint8_t xf_bit[XF_N] = { 1, 2, 4, 8, 16, 32, 64, 128 };
static uint8_t xbits[OBJ_COUNT];                 /* bit f: the object is in family xf_obj[f] (7: or oLeaves') */
static int8_t xf_of[OBJ_COUNT];                  /* the family index of a query's obj, -1 none */
static int xbits_ok;
static uint8_t xcnt[XF_N][GRID_H][GRID_W];
static uint16_t xfar[XF_N];
static uint8_t xsat[XF_N];
static uint16_t xemp[XF_N];                      /* entries placed with an empty integer box (isfar 2): in no cell */
static uint8_t xmask[PIN_MAX], xond[PIN_MAX];
/* an entry's place in the index (xflush_run): isfar 1 a box not cached whole, 2 an empty one, else its cells x0 .. x1,
   y0 .. y1 (clamped); one record of 5 bytes (the five arrays' RAM), as a lookup (xplace_cell, xpoint_any) reads
   them together */
static struct xrec { int8_t isfar, x0, x1, y0, y1; } xr[PIN_MAX];
static int16_t xdnext[PIN_MAX], xdhead = NOONE;
/* per cell, the last entry placed (xflush_run) whose integer box covers the whole cell, any family, or NOONE: a hint
   only. collision_point_any's static path tests it with point_hit before walking the family (xhint_hit), so a stale
   hint (destroyed, moved, another object in a reused slot) costs a test and changes no answer */
static int16_t xhint[GRID_H][GRID_W];
/* bumped whenever an entry of an indexed object (xbits) is created, destroyed or may have changed its box: no
   collision_point answer for a static family can change while it holds still (pk_jungle.c's vegetation memo) */
static uint32_t xchg;
uint32_t pw_static_clock(void) { return xchg; }
static void xdirty(int i)
{
    xond[i] = 1;
    xdnext[i] = xdhead;
    xdhead = (int16_t)i;
}
#endif

#ifndef GCLOCK_WRAP
#define GCLOCK_WRAP 0xffff                       /* -DGCLOCK_WRAP=<small>: the host check of the wrap */
#endif
static void gtick(void)
{
    if (++gclock == GCLOCK_WRAP) {
        int x, y;
        gepoch++;
        gclock = 1;
        for (y = 0; y < GRID_H; y++)
            for (x = 0; x < GRID_W; x++) gver[y][x] = 0;
    }
}
static int16_t watch_i = NOONE;                  /* pw_changed calls on watch_i are counted in watch_n */
static uint32_t watch_n;

/* ---- per-object instance lists: the alive instances of each object in creation order (index order), and the
   alive count of each object with its descendants. Linked at pin_add, unlinked when alive goes to 0 ------------ */
int16_t pw_ohead[OBJ_COUNT], pw_inext[PIN_MAX];
/* bumped at a level start (prun.c: the event lists of objects with instances; an object's list going from empty
   to non-empty or back bumps the counts of its event keys, prun_onz) */
uint32_t pw_onz_gen = 1;
int16_t pw_seq[PIN_MAX];
int16_t pw_ord[PIN_MAX];
/* the free slots freel[0 .. nfree) (taken last in, first out), and the removed ones waiting for the step's end at
   the top, freel[PIN_MAX - 1 - k] for k < nrmq (a slot is in one part at most) */
static int16_t freel[PIN_MAX];
#ifndef PW_RELEASE_BATCH
#define PW_RELEASE_BATCH 64
#endif
#define PW_RELEASE_ROOM 256
#ifndef PW_SEQ_RENUM
#define PW_SEQ_RENUM (32766 - PIN_MAX)           /* at most PIN_MAX creations between two releases */
#endif
#define rmq(k) freel[PIN_MAX - 1 - (k)]
static int nfree, nrmq, nrmq_prev;            /* nrmq_prev: nrmq at the previous step's end (pw_release) */
static uint8_t relmark[PIN_MAX];
static uint8_t dead_ok;                          /* PIN_DEAD written in this room (after the loader: W.in) */
/* every alive instance in creation order: pw_ahead, then pw_anext[i] (an instance unlinked keeps its pw_anext, so a
   walk that saw it continues from it) */
int16_t pw_ahead, pw_anext[PIN_MAX];
static int16_t pw_atail, aprev[PIN_MAX];
/* the alive non-terrain instances (pin_needs_ext 1) in creation order: pw_nthead, then pw_ntnext[i] */
int16_t pw_nthead, pw_ntnext[PIN_MAX];
static int16_t pw_nttail, ntprev[PIN_MAX];
static int16_t otail[OBJ_COUNT], iprev[PIN_MAX];
/* the instances to animate (prun.c animate): alive instances in creation order, pw_tahead then pw_tanext[i], except
   those animate found doing nothing (anim_one left image_index's bits as they were and ran no Animation End event:
   it is a function of image_index, image_speed, the sprite and the object, so it does nothing again until one of
   them changes). One goes back on the list at a change of image_index's bits (pin_setimg) or of the sprite
   (pin_set_sprite), at a write of image_speed (pin_setispd; the three fields are PIN_RO) and when created. PLAY_STATS builds
   check every one off the list after animate. An instance taken off keeps its pw_tanext, so a walk at it
   continues */
int16_t pw_tahead, pw_tanext[PIN_MAX];
static int16_t tatail, taprev[PIN_MAX];
static uint8_t taon[PIN_MAX];
static int16_t olive[OBJ_COUNT];
#define odesc0 obj_desc0
#define odesc obj_desc

static uint8_t nc_any;                           /* instance_nearest_p's cache (nc) is in use */
/* bit e: obj_is(o, nc[e].obj), for each slot e that nc_get has filled (nc_slot keeps it at every change of the slot's
   obj): pw_changed calls nc_moved only for an instance of a kept family. A bit is set only after nc_get has set
   nc_any (never cleared), so nc_ofam[o] != 0 implies nc_any. Kept in pwob's high bits (NC_OFAM) */
#define NC_OFAM(o) (pwob[o] >> 4)                /* (pwob's bits 4-7) */
static void nc_inval(int obj);
static void nc_moved(int i);
static void nc_reset(void);
#ifdef PLAY_STATS
static void nc_ofam_check(int obj);
#endif

/* a family walk over the object tree (pcol.c's pcol_ochild / pcol_osib) in preorder: the object after o in root's
   family, o's subtree skipped when it holds no alive instance (olive counts a family) */
static int ofam_next(int root, int o)
{
    if (olive[o] && pcol_ochild[o] >= 0) return pcol_ochild[o];
    while (o != root) {
        if (pcol_osib[o] >= 0) return pcol_osib[o];
        o = objdefs[o].parent;
    }
    return -1;
}

/* instance_first_p's last answers (4 objects), each kept while olive_gen holds: olive_gen is bumped on every change of
   any object's alive count (olive_add: every link / unlink) and at every level start, and the oldest alive instance
   of a family depends only on its alive set (creation numbers keep their order when renumbered). Tags are gen + 1 */
static uint32_t olive_gen, ifc_tag[4];
static int16_t ifc_obj[4], ifc_val[4];
static uint8_t ifc_next;

static void pdist_warm(void);
static void olists_reset(void)
{
    pdist_warm();
    int o;
    olive_gen++;
    obj_desc_init();
    pcol_obj_tree();
    nc_reset();
    for (o = 0; o < OBJ_COUNT; o++) {
        pw_ohead[o] = otail[o] = NOONE;
        olive[o] = 0;
    }
    pw_onz_gen++;
    pw_ahead = pw_atail = NOONE;
    pw_nthead = pw_nttail = NOONE;
    pw_tahead = tatail = NOONE;
    for (o = 0; o < PIN_MAX; o++) taon[o] = 0;
    grid_reset();
}

static void olive_add(int obj, int d)
{
    int a;
#ifndef PCOL_EXACT
    if (xbits[obj]) xchg++;                       /* (xbits is set at the first level start, before any instance) */
#endif
    if (nc_any) nc_inval(obj);
    olive_gen++;
    for (a = obj; a >= 0; a = objdefs[a].parent) olive[a] = (int16_t)(olive[a] + d);
}

static void olink(int i)
{
    int o = PW.in[i].obj;
    pw_inext[i] = NOONE;
    iprev[i] = otail[o];
    if (otail[o] >= 0) pw_inext[otail[o]] = (int16_t)i; else { pw_ohead[o] = (int16_t)i; prun_onz(o); }
    otail[o] = (int16_t)i;
    olive_add(o, 1);
    pw_anext[i] = NOONE;
    aprev[i] = pw_atail;
    if (pw_atail >= 0) pw_anext[pw_atail] = (int16_t)i; else pw_ahead = (int16_t)i;
    pw_atail = (int16_t)i;
    if (pin_needs_ext(o)) {
        pw_ntnext[i] = NOONE;
        ntprev[i] = pw_nttail;
        if (pw_nttail >= 0) pw_ntnext[pw_nttail] = (int16_t)i; else pw_nthead = (int16_t)i;
        pw_nttail = (int16_t)i;
    }
}

/* ounlink without the alive count (olive_add(obj, -1)): pw_deactivate_n counts its instances by object */
static void ounlink_nc(int i)
{
    int o = PW.in[i].obj;
    if (iprev[i] >= 0) pw_inext[iprev[i]] = pw_inext[i]; else pw_ohead[o] = pw_inext[i];
    if (pw_inext[i] >= 0) iprev[pw_inext[i]] = iprev[i]; else otail[o] = iprev[i];
    if (pw_ohead[o] < 0) prun_onz(o);
    grid_unlink(i);
    pw_ta_off(i);
    if (aprev[i] >= 0) pw_anext[aprev[i]] = pw_anext[i]; else pw_ahead = pw_anext[i];
    if (pw_anext[i] >= 0) aprev[pw_anext[i]] = aprev[i]; else pw_atail = aprev[i];
    if (pin_needs_ext(o)) {
        if (ntprev[i] >= 0) pw_ntnext[ntprev[i]] = pw_ntnext[i]; else pw_nthead = pw_ntnext[i];
        if (pw_ntnext[i] >= 0) ntprev[pw_ntnext[i]] = ntprev[i]; else pw_nttail = ntprev[i];
    }
}

static void ounlink(int i)
{
    ounlink_nc(i);
    olive_add(PW.in[i].obj, -1);
}

/* the alive instances of obj (with its descendants) in creation order: a merge of the objects' lists. More than
   FAM_K non-empty lists: it->lin = 1, the caller scans every instance instead */
#define FAM_K 24
struct fam { int16_t cur[FAM_K]; int n, lin, k, obj; };

static void fam_begin(struct fam *it, int obj)
{
    int j;
    it->n = 0;
    it->lin = 0;
    it->k = -1;
    it->obj = obj;
    if (obj < 0) { it->lin = 1; return; }       /* -2: every instance */
    /* the family's objects in preorder (ofam_next), a subtree without alive instances skipped: the same non-empty lists
       as obj_desc's (in another order: fam_next takes the oldest head, creation numbers are unique) */
    for (j = obj; j >= 0; j = ofam_next(obj, j)) {
        int h;
        if (olive[j] == 0) continue;
        h = pw_ohead[j];
        if (h < 0) continue;
        if (it->n == FAM_K) { it->lin = 1; return; }
        it->cur[it->n++] = (int16_t)h;
    }
}

static int fam_next(struct fam *it)
{
    int j, b = 0, v;
    if (it->n == 0) return NOONE;
    for (j = 1; j < it->n; j++)
        if (PIN_OLDER(it->cur[j], it->cur[b])) b = j;
    v = it->cur[b];
    PWST(visit, 1);
    if (pw_inext[v] >= 0) it->cur[b] = pw_inext[v];
    else it->cur[b] = it->cur[--it->n];
    return v;
}

/* the next instance of the family (creation order), with the scan of every instance as the fallback */
static int fam_get(struct fam *it)
{
    if (!it->lin) return fam_next(it);
    while (++it->k < PW.nord) {
        int s = pw_ord[it->k];
        const struct pin *p = &PW.in[s];
        PWST(visit, 1);
        if (p->alive && (it->obj < 0 || obj_is(p->obj, it->obj))) return s;
    }
    return NOONE;
}

/* no alive instance of obj (with descendants) */
static int fam_none(int obj) { return obj >= 0 && olive[obj] == 0; }

/* ---- the drawing's dirty marks: instances whose x, y, sprite, mask, scale, angle, image_index, visible or depth
   changed, or that were created or destroyed, since the drawing last took the list (pw_draw_dirty) ------------ */
static int16_t ddlist[PIN_MAX];
static uint8_t ddmark[PIN_MAX];
static int nddlist;

static void ta_on(int i)
{
    int p;
    if (taon[i] || !PW.in[i].alive) return;
    for (p = tatail; p >= 0 && pw_seq[p] > pw_seq[i]; p = taprev[p]) {}
    taprev[i] = (int16_t)p;
    pw_tanext[i] = p >= 0 ? pw_tanext[p] : pw_tahead;
    if (p >= 0) pw_tanext[p] = (int16_t)i; else pw_tahead = (int16_t)i;
    if (pw_tanext[i] >= 0) taprev[pw_tanext[i]] = (int16_t)i; else tatail = (int16_t)i;
    taon[i] = 1;
}

void pw_ta_off(int i)
{
    if (!taon[i]) return;
    if (taprev[i] >= 0) pw_tanext[taprev[i]] = pw_tanext[i]; else pw_tahead = pw_tanext[i];
    if (pw_tanext[i] >= 0) taprev[pw_tanext[i]] = taprev[i]; else tatail = taprev[i];
    taon[i] = 0;
}

int pw_ta_is_on(int i) { return taon[i]; }
void pw_ta_on(int i) { ta_on(i); }

/* the last alive instance older than seq s0 with a sprite (prun.c animate) */
int pw_last_with_sprite(int16_t s0)
{
    int k;
    for (k = pw_atail; k >= 0; k = aprev[k])
        if (pw_seq[k] < s0 && PW.in[k].spr >= 0) return k;
    return NOONE;
}

#ifdef PLAY_STATS
uint32_t pw_muts;                                /* pw_draw_mark calls: every change of an instance's fields marks */
#endif
void pw_draw_mark(int i)
{
#ifdef PLAY_STATS
    pw_muts++;
#endif
    if (ddmark[i]) return;
    ddmark[i] = 1;
    ddlist[nddlist++] = (int16_t)i;
}

int pw_draw_dirty(const int16_t **list)
{
    *list = ddlist;
    return nddlist;
}

void pw_draw_dirty_clear(void)
{
    int k;
    for (k = 0; k < nddlist; k++) ddmark[ddlist[k]] = 0;
    nddlist = 0;
}

/* a setter changed x / y / sprite / mask / scale / angle (play.h pin_changed_) */
/* pw_changed past the watch count when nc_moved runs or the object's gobj byte is not computed yet */
static __attribute__((noinline)) void pw_changed_slow(int i)
{
    if (NC_OFAM(PW.in[i].obj)) nc_moved(i);            /* (nc_moved does nothing for an object of no kept family) */
    pw_draw_mark(i);
    PW.in[i].bbk = 0;
    grid_dirty(i);
    pcol_changed(i);
}

/* the same steps in the same order; the common case (no nc_moved, gobj known) has grid_dirty's body inline and ends in a
   tail call (no frame) */
void pw_changed(int i)
{
    int o, g;
    if (i == watch_i) watch_n++;
#ifdef PLAY_STATS
    nc_ofam_check(PW.in[i].obj);
#endif
    o = PW.in[i].obj;
    g = pwob[o];
    if (g >> 4 || !g) {                           /* nc_ofam, gobj not computed */
        pw_changed_slow(i);
        return;
    }
    pw_draw_mark(i);
    PW.in[i].bbk = 0;
    grid_dirty_g(i, o, g);
    pcol_changed(i);
}

/* pw_changed where no field changed (pobj.c rest_skip's replay of a fixed point's changes): the marks and the grid's
   re-placement as pw_changed. The box cache (bbk) and the nearest cache (nc) are kept: each is cleared or updated by
   every change of the fields it is computed from, so with none since, each still holds the current fields' values */
void pw_replayed(int i)
{
    if (i == watch_i) watch_n++;
    pw_draw_mark(i);
    grid_dirty(i);
    pcol_changed(i);
}

int pw_count(int obj) { return olive[obj]; }

/* ---- the struct pin_ext pool: record 0 holds the defaults (shared, never written); the others are allocated by
   pin_add for objects that need them and freed when the instance leaves (CRoom::RemoveMarked: pw_removed) ------ */
struct pin_ext pin_ext[EXT_MAX];
static int16_t extfree[EXT_MAX];
static int nextfree;
static uint8_t needs_ext[OBJ_COUNT];        /* 0 unknown, 1 no, 2 yes */
static int ext_used, ext_used_max;

/* pin_add's defaults: zero, alarms off, no trap / enemy */
static void ext_defaults(struct pin_ext *x)
{
    unsigned char *b = (unsigned char *)x;
    unsigned k;
    for (k = 0; k < sizeof *x; k++) b[k] = 0;
    for (k = 0; k < 12; k++) x->alarm[k] = -1;
    x->trapID = x->enemyID = NOONE;
    x->alpha = 1;
}

/* the terrain: static blocks, ladders, backgrounds and the transition rooms' decorations ("...Tile"), whose
   instances keep the defaults (no event besides Create / Destroy, no alarm, no collision event; and no other code
   writes their variables: the PIN_EXT_CHECK build checks that over the routes and generated levels). Every other
   object gets its own record */
static const char *const terrain_names[] = {
    "oBrick", "oBrickSmooth", "oBlock", "oHardBlock", "oLush", "oTemple", "oIce", "oDark", "oDesert", "oDesert2",
    "oLavaSolid", "oAlienShip", "oAlienShipFloor", "oXocBlock", "oAltarLeft", "oAltarRight", "oMoai", "oMoai2",
    "oMoai3", "oMoaiInside", "oLadder", "oLadderOrange", "oLadderTop", "oRoom", "oBlackBG", "oBlackFadeUp",
    "oCaveBG", "oCaveBG2", "oCaveBGEntrance", "oBackdrop", "oForeground",
    "oWater", "oWaterSwim"            /* Create / Destroy only; their type is in struct pin, checked is never read */
};

static int str_eq(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static int is_terrain(int obj)
{
    const char *n = objdefs[obj].name;
    const struct pobj *o = &pobj[obj];
    unsigned k, len = 0;
    if ((o->ev & ~(EV_CREATE | EV_DESTROY)) || o->alarms || o->ncol) return 0;
    for (k = 0; k < sizeof terrain_names / sizeof terrain_names[0]; k++)
        if (str_eq(n, terrain_names[k])) return 1;
    while (n[len]) len++;
    return len > 4 && str_eq(n + len - 4, "Tile");
}

int pin_needs_ext(int obj)
{
    if (!needs_ext[obj]) needs_ext[obj] = (uint8_t)(is_terrain(obj) ? 1 : 2);
    return needs_ext[obj] == 2;
}

/* the enemies' pool (struct pin_en) */
struct pin_en pin_en[EN_MAX];
static int16_t enfree[EN_MAX];
static int nenfree;
static int en_used, en_used_max;

static void en_zero(struct pin_en *x)
{
    unsigned char *b = (unsigned char *)x;
    unsigned k;
    for (k = 0; k < sizeof *x; k++) b[k] = 0;
}

/* the objects that use struct pin_en: the oEnemy and oDamsel families and the other objects whose events penemy.c,
   pdamsel.c and pshop.c run (their switch cases) */
static int pin_needs_en(int obj)
{
    switch (obj) {
    case OBJ_oEnemySight: case OBJ_oBoulder: case OBJ_oGiantTikiHead: case OBJ_oDamselKiss: case OBJ_oSpiderHang:
    case OBJ_oGiantSpiderHang: case OBJ_oFakeBones: case OBJ_oYellHelp: case OBJ_oHeart: case OBJ_oSplash:
    case OBJ_oWeb: case OBJ_oWebBall: case OBJ_oBullet: case OBJ_oBone: case OBJ_oShotgunBlastLeft:
    case OBJ_oShotgunBlastRight:
        return 1;
    }
    return obj_is(obj, OBJ_oEnemy) || obj_is(obj, OBJ_oDamsel);
}

int pw_en_used_max(void) { return en_used_max; }

static void ext_reset(void)
{
    int k;
    ext_defaults(&pin_ext[0]);
    nextfree = 0;
    for (k = EXT_SCRATCH - 1; k >= 1; k--) extfree[nextfree++] = (int16_t)k;   /* EXT_SCRATCH kept out */
    ext_defaults(&pin_ext[EXT_SCRATCH]);
    ext_used = 0;
    en_zero(&pin_en[0]);
    nenfree = 0;
    for (k = EN_SCRATCH - 1; k >= 1; k--) enfree[nenfree++] = (int16_t)k;     /* EN_SCRATCH kept out */
    en_zero(&pin_en[EN_SCRATCH]);
    en_used = 0;
}

typedef uint32_t __attribute__((may_alias)) u32a;
typedef char pin_ext_words[sizeof(struct pin_ext) % 4 == 0 ? 1 : -1];
typedef char pin_words[sizeof(struct pin) % 4 == 0 ? 1 : -1];
static void ext_defaults_fast(struct pin_ext *x)
{
    static struct pin_ext t;
    static uint8_t made;
    const u32a *a = (const u32a *)&t;
    u32a *b = (u32a *)x;
    unsigned k;
    if (!made) { ext_defaults(&t); made = 1; }
    for (k = 0; k < sizeof t / 4; k++) b[k] = a[k];
}

static int ext_alloc(void)
{
    int e;
    if (nextfree == 0) {                             /* full: the scratch record (never record 0, the defaults) */
        PUNTR(9005);
        ext_defaults(&pin_ext[EXT_SCRATCH]);
        return EXT_SCRATCH;
    }
    e = extfree[--nextfree];
    ext_defaults_fast(&pin_ext[e]);
    if (++ext_used > ext_used_max) ext_used_max = ext_used;
    return e;
}

int pw_ext_used_max(void) { return ext_used_max; }

/* (out of line, and pin_add calls en_alloc alone: pin_add is hot and cached; a scratch-ext test there with en_zero
   inlined grew it by 472 bytes, which moved the cached code after it: jtcps3 route steps +1 to +4 %) */
static __attribute__((noinline)) int en_scratch(void)
{
    en_zero(&pin_en[EN_SCRATCH]);
    return EN_SCRATCH;
}

static int en_alloc(void)
{
    int e;
    if (nenfree == 0) {                              /* full: the scratch record (never record 0, the zeros) */
        PUNTR(9008);
        return en_scratch();
    }
    e = enfree[--nenfree];
    en_zero(&pin_en[e]);
    if (++en_used > en_used_max) en_used_max = en_used;
    return e;
}

/* instance i left the room (RemoveMarked): its record is free */
void pw_removed(int i)
{
    struct pin *p = &PW.in[i];
    rmq(nrmq) = (int16_t)i;                          /* its slot goes back at the step's end (pw_release) */
    nrmq++;
    if (p->ext > 0 && p->ext != EXT_SCRATCH) {       /* the scratch records are shared: never on a free list */
        if (pin_ext[p->ext].en > 0 && pin_ext[p->ext].en != EN_SCRATCH) {
            enfree[nenfree++] = pin_ext[p->ext].en;
            en_used--;
        }
        extfree[nextfree++] = p->ext;
        ext_used--;
    }
#ifdef PIN_EXT_CHECK
    p->ext = -1;                                     /* a later PE(p) is an error */
#else
    pin_set_ext(p, 0);
#endif
}

/* the slot PIN_DEAD: never alive; what the references to removed instances (REL) and a full pin_add point to. Its
   record is the scratch one (writes through it touch no live instance and not the defaults); initialised once a
   room, after the loader (until then it is W.in's memory) */
static void dead_init(void)
{
    struct pin *d;
    unsigned char *b;
    unsigned k2;
    if (dead_ok) return;
    d = &PW.in[PIN_DEAD];
    b = (unsigned char *)d;
    for (k2 = 0; k2 < sizeof *d; k2++) b[k2] = 0;
    PIN_WR(int16_t, d->spr) = -1;
    PIN_WR(int16_t, d->mask) = -1;
#ifdef PIN_EXT_CHECK
    d->ext = -1;                                     /* PE(PIN_DEAD) is an error */
#else
    pin_set_ext(d, EXT_SCRATCH);
#endif
    dead_ok = 1;
}

/* a reference kept across steps: to PIN_DEAD when its slot goes back */
#define REL(r) do { if ((r) >= 0 && relmark[r]) (r) = PIN_DEAD; } while (0)

/* the end of a step: the slots of the instances RemoveMarked removed go back on the free list. Kept across steps
   (pint.h, play.h): oPlayer1's idx / ladder / holdItem, the instance variables trapID, enemyID, bombID of the
   alive instances; the per-slot state of pcol.c went in RemoveMarked, the grid's dirty list is flushed here */
void pw_release(void)
{
    int k, j, s, n, lo, hi, smin = 32767, flush = 0;
    /* in batches (the compaction of pw_ord and the sweep cost about PW.nord): 64 removed, at the end of a step that
       removed none (not the step whose removals filled the batch: an explosion's, the frame budget's spike, p5_snakes
       record 203), or the unused slots and the free ones close to running out (a step creates fewer than
       PW_RELEASE_ROOM). Which slots go back when is not seen by the game (instances are reached through pw_ord, the
       per-object lists and creation numbers) */
    if (nfree + (PIN_DEAD - PW.n) >= PW_RELEASE_ROOM && (nrmq < PW_RELEASE_BATCH || nrmq != nrmq_prev)) {
        nrmq_prev = nrmq;
        return;
    }
    if (nrmq == 0) return;
    dead_init();
    for (k = 0; k < nrmq; k++) {
        s = rmq(k);
        relmark[s] = 1;
        flush |= gond[s];
        if (pw_seq[s] < smin) smin = pw_seq[s];
    }
    if (flush) grid_flush();
    REL(PL.idx);
    REL(PL.ladder);
    REL(PL.holdItem);
    for (s = pw_nthead; s >= 0; s = pw_ntnext[s]) {
        struct pin_ext *x;
        if (PW.in[s].ext <= 0) continue;
        x = &pin_ext[PW.in[s].ext];
        REL(x->trapID);
        REL(x->enemyID);
        if (x->en > 0) REL(pin_en[x->en].bombID);
    }
    /* pw_ord is in creation order (pw_seq ascends along it): the entries before the oldest removed one stay where they
       are, so the compaction starts there (found by a binary search) */
    n = PW.nord;
    lo = 0;
    hi = n;
    while (lo < hi) {
        int m = (lo + hi) >> 1;
        if (pw_seq[pw_ord[m]] < smin) lo = m + 1;
        else hi = m;
    }
#ifdef PLAY_STATS
    for (k = 0; k < lo; k++)                         /* the host builds check the premise */
        if (relmark[pw_ord[k]]) { fprintf(stderr, "pw_release: removed slot %d before %d\n", pw_ord[k], lo); abort(); }
#endif
    for (k = j = lo; k < n; k++)
        if (!relmark[pw_ord[k]]) pw_ord[j++] = pw_ord[k];
    PW.nord = (int16_t)j;
    if (PW.seq > PW_SEQ_RENUM) {                     /* creation numbers from 0 again, in the same order */
        for (k = 0; k < PW.nord; k++) pw_seq[pw_ord[k]] = (int16_t)k;
        PW.seq = PW.nord;
    }
    /* rmq(k) goes to freel[nfree + k]. When nfree + 2 * nrmq > PIN_MAX the two parts meet in that copy and it
       overwrites queue entries not yet read: a slot twice on the free list, then two alive instances in one slot
       and a cycle in an object list (scripts/untr_survey.sh, level 9 seed 5: about 900 oEnemySight removed while
       the batch waited; pen_motion looped at step 769). Then the queue is reversed in place first (rmq(k) at
       freel[PIN_MAX - nrmq + k]) and read upwards: the same free list, every write below the entries still unread */
    j = 0;
    if (nfree + 2 * nrmq > PIN_MAX) {
        int a, b;
        for (a = PIN_MAX - nrmq, b = PIN_MAX - 1; a < b; a++, b--) { s = freel[a]; freel[a] = freel[b]; freel[b] = (int16_t)s; }
        j = 1;
    }
    for (k = 0; k < nrmq; k++) {
        s = j ? freel[PIN_MAX - nrmq + k] : rmq(k);
        relmark[s] = 0;
        freel[nfree++] = (int16_t)s;
#ifdef PIN_EXT_CHECK
        {   /* the check build: a free slot read through an index kept elsewhere shows in the output */
            struct pin *d = &PW.in[s];
            PIN_SETX_RAW(d, (pos)PI(8000));
            PIN_SETY_RAW(d, (pos)PI(8000));
            d->id = -7777;
        }
#endif
    }
    nrmq = nrmq_prev = 0;
}

#ifdef PIN_EXT_CHECK
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* the check build: no access to a removed instance's record, and record 0 still the defaults */
struct pin_ext *pin_ext_checked(const struct pin *p)
{
    static struct pin_ext def;
    static int init;
    if (!init) { ext_defaults(&def); init = 1; }
    if (memcmp(&pin_ext[0], &def, sizeof def)) {
        unsigned k;
        for (k = 0; k < sizeof def && ((unsigned char *)&pin_ext[0])[k] == ((unsigned char *)&def)[k]; k++) {}
        fprintf(stderr, "PIN_EXT_CHECK: record 0 written at byte %u (step %u, current object %s)\n", k,
                (unsigned)PW.step, play_cur_obj >= 0 ? objdefs[play_cur_obj].name : "-");
        exit(3);
    }
    if (p->ext < 0) {
        fprintf(stderr, "PIN_EXT_CHECK: removed instance %ld (%s) read (step %u)\n", (long)p->id,
                objdefs[p->obj].name, (unsigned)PW.step);
        exit(3);
    }
    return &pin_ext[p->ext];
}

struct pin_en *pin_en_checked(const struct pin *p)
{
    static struct pin_en zero;
    struct pin_ext *x = pin_ext_checked(p);
    if (memcmp(&pin_en[0], &zero, sizeof zero)) {
        unsigned k;
        for (k = 0; k < sizeof zero && ((unsigned char *)&pin_en[0])[k] == 0; k++) {}
        fprintf(stderr, "PIN_EXT_CHECK: pin_en record 0 written at byte %u (step %u, current object %s, accessed %s)\n",
                k, (unsigned)PW.step, play_cur_obj >= 0 ? objdefs[play_cur_obj].name : "-", objdefs[p->obj].name);
        fprintf(stderr, "  ext %d en %d en_used %d id %ld alive %d\n", p->ext, x->en, en_used, (long)p->id, p->alive);
        exit(3);
    }
    if (x->en == 0 && getenv("PIN_EN_LOG")) {
        static uint8_t seen[OBJ_COUNT];
        if (!seen[p->obj]) {
            seen[p->obj] = 1;
            fprintf(stderr, "PIN_EN_LOG: %s reads / writes the shared record (step %u, current %s)\n",
                    objdefs[p->obj].name, (unsigned)PW.step, play_cur_obj >= 0 ? objdefs[play_cur_obj].name : "-");
        }
    }
    return &pin_en[x->en];
}
#endif

void pw_reset(void)
{
    int k;
    PW.n = 0;
    PW.nord = 0;
    PW.seq = 0;
    nfree = nrmq = nrmq_prev = 0;
    dead_ok = 0;
    for (k = 0; k < nddlist; k++) ddmark[ddlist[k]] = 0;
    nddlist = 0;                                     /* a new room: the drawing starts from scratch */
    ext_reset();
    olists_reset();
    pcol_after_reset();
}

int pin_add(int obj, pos x, pos y, int32_t id)
{
    int i, k;
    struct pin *p;
    if (nfree > 0)
        i = freel[--nfree];
    else {
        if (PW.n >= PIN_DEAD) {                      /* full: the caller gets the scratch slot (dead_init), not */
            PUNTR(9001);                             /* alive, its writes go to the scratch record */
            dead_init();
            return PIN_DEAD;
        }
        i = PW.n++;
    }
    pw_seq[i] = PW.seq++;
    pw_ord[PW.nord++] = (int16_t)i;
    p = &PW.in[i];
    {   /* zero every field */
        u32a *b = (u32a *)p;
        unsigned k2;
        for (k2 = 0; k2 < sizeof *p / 4; k2++) b[k2] = 0;
    }
    p->id = id;
    p->obj = (int16_t)obj;
    PIN_WR(int16_t, p->spr) = gobjspr[obj];         /* a new instance: pcol_added takes it as it is */
    PIN_WR(int16_t, p->mask) = -1;
    p->alive = 1;
    PIN_WR(uint8_t, p->visible) = pobj[obj].visible;
    PIN_SETX_RAW(p, x);
    PIN_SETY_RAW(p, y);
    PIN_WR(float, p->depth) = objdefs[obj].depth;
    PIN_WR(img_t, p->img) = 0;
    PIN_WR(img_t, p->ispd) = 1;
    PIN_WR(float, p->xscale) = PIN_WR(float, p->yscale) = 1;
    PIN_WR(float, p->angle) = 0;
    pin_set_ext(p, pin_needs_ext(obj) ? ext_alloc() : 0);         /* with pin_add's defaults (ext_defaults) */
    if (obj == OBJ_oPlayer1 && p->ext) PE(p)->xprev = x;
    if (p->ext && pin_needs_en(obj)) pin_ext[p->ext].en = (int16_t)en_alloc();   /* (a scratch ext holder's: never freed) */
    pw_ta_off(i);                                    /* (a reused slot is placed again by its creation number) */
    ta_on(i);
    pw_draw_mark(i);                                 /* (a reused slot may still be on the list: marked once) */
    (void)k;
    olink(i);
    gcell[i] = NOONE;
    gond[i] = 0;
#ifndef PCOL_EXACT
    if (xmask[i] && !xond[i]) xdirty(i);            /* a reused slot still counted in the liquid index */
#endif
    grid_dirty(i);
    pcol_added(i);
    return i;
}

int pin_create(pos x, pos y, int obj)
{
    int i = pin_add(obj, x, y, PW.next_id++);
    if (i == PIN_DEAD) return i;                     /* full: no collision entry, no Create */
    pcol_create(i);                                  /* CollisionInsert, before the Create event */
    ev_create(i);
    return i;
}

void pin_destroy(int i)
{
    if (i < 0 || !PW.in[i].alive)
        return;
    PW.in[i].alive = 0;          /* GameMaker marks it first: a search inside its Destroy event skips it */
    ounlink(i);
#ifndef PCOL_EXACT
    if (xmask[i] && !xond[i]) xdirty(i);            /* its counts leave the static-family index at the next query */
#endif
    pw_draw_mark(i);
    ev_destroy(i);
    pcol_destroyed(i);           /* in the collision tree until the next RemoveMarked */
}

void pin_kill(int i)
{
    if (i >= 0) {
        if (PW.in[i].alive) { ounlink(i); PW.in[i].alive = 0; pw_draw_mark(i); }   /* (dead first: not back on the
                                                                                     animation list) */
#ifndef PCOL_EXACT
        if (xmask[i] && !xond[i]) xdirty(i);
#endif
        PW.in[i].alive = 0;
        pcol_destroyed(i);
    }
}

/* off-view deactivation (docs/DEACT.md, prun.c deact_pass): instance_deactivate_object of ids[0 .. n), in that order
   (newest first: deact_pass takes them from the creation-ordered pw_nthead list backwards). Each is not alive for
   every event loop, query, `with`, count and the recorder (GameMaker skips a deactivated instance in all of them), out
   of the object lists and pw_ord, its collision entry taken out (pcol_deactivated); the slot and its records stay (its
   variables stay readable through references, as GameMaker's). Every id is alive (pw_nthead holds linked instances
   only) and listed once */
void pw_deactivate_n(const int16_t *ids, int n)
{
    int k, j, m, ng = 0;
    int16_t go[8], gn[8];                            /* alive counts to take off: up to 8 objects at a time */
    if (n <= 0) return;
    for (k = 0; k < n; k++) {
        int i = ids[k], o = PW.in[i].obj;
        ounlink_nc(i);
        for (j = 0; j < ng && go[j] != o; j++) {}
        if (j == ng) {
            if (ng == 8) {
                for (j = 0; j < 8; j++) olive_add(go[j], -gn[j]);
                ng = j = 0;
            }
            go[j] = (int16_t)o;
            gn[j] = 0;
            ng++;
        }
        gn[j]++;
        PW.in[i].alive = 0;
        pw_draw_mark(i);
#ifndef PCOL_EXACT
        if (xmask[i] && !xond[i]) xdirty(i);
#endif
        pcol_deactivated(i);
    }
    for (j = 0; j < ng; j++) olive_add(go[j], -gn[j]);
    /* pw_ord without the ids, in one pass (nothing above reads pw_ord: the order left is the one removing them one by
       one gives). pw_ord holds distinct slots in creation order, so pw_seq rises along it (pw_activate appends with the
       next number; the renumbering keeps the order); the ids, oldest (ids[n - 1]) first, are met in that order: the
       compaction starts at the oldest one's place (a binary search on pw_seq) and drops each id where the walk meets
       it. Any id the walk did not meet (not in that order) is filtered out after, one by one */
    {
        int lo = 0, hi = PW.nord - 1, s0 = pw_seq[ids[n - 1]];
        while (lo < hi) {
            int mi = (lo + hi) >> 1;
            if (pw_seq[pw_ord[mi]] < s0) lo = mi + 1; else hi = mi;
        }
        int16_t *src = &pw_ord[lo], *dst = src, *end = &pw_ord[PW.nord];
        for (m = n - 1; m >= 0; m--) {
            int16_t want = ids[m];
            while (src < end && *src != want) *dst++ = *src++;
            if (src == end) break;
            src++;
        }
        while (src < end) *dst++ = *src++;
        PW.nord = (int16_t)(dst - pw_ord);
    }
    if (m >= 0) {
#ifdef PLAY_STATS
        fprintf(stderr, "pw_deactivate_n: %d not found in creation order\n", ids[m]);
        abort();
#endif
        for (; m >= 0; m--) {
            for (k = j = 0; k < PW.nord; k++)
                if (pw_ord[k] != ids[m]) pw_ord[j++] = pw_ord[k];
            PW.nord = (int16_t)j;
        }
    }
}

/* instance_activate_object(i) of a deactivated instance: it comes back as the newest instance (Observed: last in its
   object's Step order, first in `with (all)`, docs/DEACT.md 2), with a new creation number, and is put in the
   collision tree as instance_create puts a new one in (pcol_activated) */
void pw_activate(int i)
{
    int k;
    if (PW.in[i].alive) return;
    if (PW.seq > PW_SEQ_RENUM) {                      /* creation numbers from 0 again, in the same order */
        for (k = 0; k < PW.nord; k++) pw_seq[pw_ord[k]] = (int16_t)k;
        PW.seq = PW.nord;
    }
    pw_seq[i] = PW.seq++;
    pw_ord[PW.nord++] = (int16_t)i;
    PW.in[i].alive = 1;
    PW.in[i].bbk = 0;
    pw_ta_off(i);
    ta_on(i);
    pw_draw_mark(i);
    olink(i);
    gcell[i] = NOONE;
    gond[i] = 0;
#ifndef PCOL_EXACT
    if (xmask[i] && !xond[i]) xdirty(i);
#endif
    grid_dirty(i);
    pcol_activated(i);
}

/* sprite_index = spr: image_index is kept unless it is past the new sprite's frames, then 0 (Observed in
   build/trace/p4_walk_s1: sRunLeft at image 4.668 -> sFallLeft (1 frame) 0.0 in record 60; sFallLeft at 0.4 ->
   sRunLeft (6 frames) 0.4 in record 66) */
void pin_set_sprite(int i, int spr)
{
    struct pin *p = &PW.in[i];
    if (p->spr != spr) {
        PIN_WR(int16_t, p->spr) = (int16_t)spr;
        ta_on(i);
        p->bbk = 0;
        pw_draw_mark(i);
        grid_dirty(i);
        if (spr >= 0 && (p->img >= (img_t)psprite[spr].frames || p->img < 0))
            PIN_WR(img_t, p->img) = 0;
        pcol_mark(i);                                /* SetSpriteIndex: CollisionMarkDirty */
    }
}

/* ---- bounding boxes ------------------------------------------------------------------------------------- */
#define BB_INT 1
#define BB_DBL 2
#define BB_NOSPR 3
#define BB_INTS 4    /* the box is bl, bt, br, bb exactly, a box-only sprite at whole scales (bbox_ints); pin_ibox: 0 */

/* pos_int: play.h */

/* pin_xy_int's slow path (play.h): one shadow not known; both decoded from the floats and stored (the shadows are a
   cache of x, y: written through a const pointer's object, which is PW.in's) */
__attribute__((noinline)) int pin_xy_fill(const struct pin *p, int32_t *x, int32_t *y)
{
    struct pin *w = (struct pin *)p;
    int32_t a, b;
    w->ix = pos_int(p->x, &a) ? (int16_t)a : PXY_NO;
    w->iy = pos_int(p->y, &b) ? (int16_t)b : PXY_NO;
    if (w->ix == PXY_NO || w->iy == PXY_NO) return 0;
    *x = a; *y = b;
    return 1;
}

#ifdef PIN_SHADOW_CHECK
/* the check build: the shadows' answer r (and x, y when 1) against pos_int on the floats */
int pin_xy_check(const struct pin *p, int r, int32_t x, int32_t y)
{
    int32_t a = 0, b = 0;
    int d = pos_int(p->x, &a) && pos_int(p->y, &b);
    if (d != r || (r && (a != x || b != y))) {
        fprintf(stderr, "pin_xy_int: shadows %d (%d %d) differ from the floats %d (%.9g %.9g), instance %d\n", r, (int)x,
                (int)y, d, (double)p->x, (double)p->y, PIN_IDX(p));
        abort();
    }
    return r;
}
#endif

static void bbox_dbl(const struct pin *p, const struct gsprcol *c, double *l, double *t, double *r, double *b)
{
    double xs = p->xscale, ys = p->yscale, x = PTOD(p->x), y = PTOD(p->y);
    PWST(bbox, 1);
    if (!fzero(p->angle)) {                       /* rotated: the box of the rotated sprite (pcol.c ebbox) */
        float o[4];
        pcol_box(PIN_IDX(p), o);
        *l = o[0]; *t = o[1]; *r = o[2]; *b = o[3];
        return;
    }
    if (xs >= 0) *l = x + xs * (c->l - c->xo);
    else *l = x + xs * (c->r + 1 - c->xo);
    *r = *l + (xs < 0 ? -xs : xs) * (c->r - c->l + 1);
    if (ys >= 0) *t = y + ys * (c->t - c->yo);
    else *t = y + ys * (c->b + 1 - c->yo);
    *b = *t + (ys < 0 ? -ys : ys) * (c->b - c->t + 1);
}

/* pnum.h's PLTI / PGTI (its comment) */
static const uint32_t ffix32_mul[32] = {
    1u << 0,  1u << 1,  1u << 2,  1u << 3,  1u << 4,  1u << 5,  1u << 6,  1u << 7,  1u << 8,  1u << 9,  1u << 10,
    1u << 11, 1u << 12, 1u << 13, 1u << 14, 1u << 15, 1u << 16, 1u << 17, 1u << 18, 1u << 19, 1u << 20, 1u << 21,
    1u << 22, 1u << 23, 1u << 24, 1u << 25, 1u << 26, 1u << 27, 1u << 28, 1u << 29, 1u << 30, 1u << 31 };
__attribute__((noinline, cold)) int gcmp_cold(double a, double b) { return gcmp_dd(a, b); }

int gcmp_fi(float x, int32_t v)
{
    union { float f; uint32_t u; } b;
    uint32_t e, l;
    uint64_t p;
    int32_t k;
    int r;
    b.f = x;
    e = ((b.u >> 23) & 0xffu) - 118u;
    if (e > 31u || (uint32_t)v + (1u << 30) >= (1u << 31)) return gcmp_dd((double)x, (double)v);
    p = (uint64_t)((b.u & 0x7fffffu) | 0x800000u) * ffix32_mul[e];
    l = (uint32_t)p;
    if (b.u >> 31) v = -v;
    k = (int32_t)(p >> 32) - v;
    if (k >= 0) r = k > 0 || l >= 42950u;
    else r = k < -1 || l <= 0u - 42950u ? -1 : 0;
    return (b.u >> 31) ? -r : r;
}

/* pnum.h pfr: gcmp_fi's scaling (the mantissa times 2^(e - 118) has floor(|x|) in its high word and the fraction
   times 2^32 in its low word, for biased exponents 126 .. 148); a negative x with a fraction is -floor(|x|) - 1 with
   the fraction 1 - l / 2^32 */
int pfr(float x, int32_t *o)
{
    union { float f; uint32_t u; } b;
    uint32_t e, l;
    uint64_t p;
    int32_t f;
    int c;
    b.f = x;
    e = (b.u >> 23) & 0xffu;
    if (e < 126) { *o = 0; return 1; }
    if (e > 148) return 0;
    p = (uint64_t)((b.u & 0x7fffffu) | 0x800000u) * ffix32_mul[e - 118];
    f = (int32_t)(uint32_t)(p >> 32);
    l = (uint32_t)p;
    if (!(b.u >> 31)) c = l == 0 ? 0 : l < 0x80000000u ? 1 : l == 0x80000000u ? 2 : 3;
    else if (l == 0) { f = -f; c = 0; }
    else { f = -f - 1; c = l > 0x80000000u ? 1 : l == 0x80000000u ? 2 : 3; }
    *o = f * 4 + c;
    return 1;
}

/* PLTI(x, lo) || PGTI(x, hi) with x decoded once: gcmp_fi's steps for both bounds (a negative x compares |x| with
   -lo and -hi, the signs swapped); either bound outside gcmp_fi's integer range, or x outside its exponents: the two
   gcmp_fi calls */
int gout_fi(float x, int32_t lo, int32_t hi)
{
    union { float f; uint32_t u; } b;
    uint32_t e, l;
    uint64_t p;
    int32_t h, k;
    b.f = x;
    e = ((b.u >> 23) & 0xffu) - 118u;
    if (e > 31u || (uint32_t)lo + (1u << 30) >= (1u << 31) || (uint32_t)hi + (1u << 30) >= (1u << 31))
        return gcmp_fi(x, lo) < 0 || gcmp_fi(x, hi) > 0;
    p = (uint64_t)((b.u & 0x7fffffu) | 0x800000u) * ffix32_mul[e];
    l = (uint32_t)p;
    h = (int32_t)(p >> 32);
    if (b.u >> 31) {
        k = h + lo;                                   /* x < lo: |x| > -lo */
        if (k > 0 || (k == 0 && l >= 42950u)) return 1;
        k = h + hi;                                   /* x > hi: |x| < -hi */
        return k < -1 || (k == -1 && l <= 0u - 42950u);
    }
    k = h - lo;                                       /* x < lo */
    if (k < -1 || (k == -1 && l <= 0u - 42950u)) return 1;
    k = h - hi;                                       /* x > hi */
    return k > 0 || (k == 0 && l >= 42950u);
}

/* 1 / -1 when f is exactly 1.0f / -1.0f, else 0 (bits: the same answer as for (double)f) */
static int funit(float f)
{
    union { float f; uint32_t u; } v;
    v.f = f;
    if (v.u == 0x3f800000u) return 1;
    if (v.u == 0xbf800000u) return -1;
    return 0;
}

/* a sprite whose one mask has every bit of its mask box set (l, t >= 0): its precise test is its box's */
static int mask_full(const struct gsprcol *c, const struct psprite *ps)
{
    int32_t w = c->r - c->l + 1, h = c->b - c->t + 1, bpr = (w + 7) >> 3, y, k;
    const uint8_t *m = pmaskdata + ps->maskoff;
    uint8_t last = (uint8_t)(0xff00u >> (((w - 1) & 7) + 1));
    if (ps->nmasks != 1 || c->l < 0 || c->t < 0 || w <= 0 || h <= 0) return 0;
    for (y = 0; y < h; y++, m += bpr) {
        for (k = 0; k < bpr - 1; k++)
            if (m[k] != 0xff) return 0;
        if ((m[bpr - 1] & last) != last) return 0;
    }
    return 1;
}

/* BB_INTS: whole scales sx, sy (1 <= |s| <= 8, not both +-1), whole x, y, angle 0, a sprite with no mask or one full
   mask (mask_full), whose mask box and origin keep |l|, |r + 1|, |l - xo|, |r + 1 - xo| (rows the same) <= 127:
   bbox_dbl's products and sums are of integers (exact), so the box is the integers below. overlap_at takes such an
   instance as its box alone (pci_of): the runner's float loop forms its sprite column lx = (c + 0.5 - x) * fl(1 / s)
   + xo and steps it by fl(1 / s) over the box overlap, at most |s| (r - l + 1) <= 1024 steps; the exact values lie in
   [l + 0.5 / |s|, r + 1 - 0.5 / |s|] (the overlap is inside the box), and with |lx| < 129 each rounding is at most
   2^-17, so lx stays within 0.009 of them, inside [l, r + 1) by the margin 0.5 / 8: the range test passes for every
   column, and trunc(lx) is in [l, r] (l >= 0) where a full mask's bit is set (rows: ly is formed afresh each row, one
   product and one sum). Checked (host, temporary): every such overlap_at against the float path, the P5 routes and
   ctall: 0 differ */
static __attribute__((noinline)) int bbox_ints(struct pin *p, const struct gsprcol *c)
{
    int32_t sx, sy, ax, ay, l, t, x, y;
    int k = spr_of(p);
    if (!fzero(p->angle) || !pin_xy_int_p(p, &x, &y)) return 0;
    if (c->kind == 1 && psprite[k].nmasks > 0 && !mask_full(c, &psprite[k])) return 0;
    if (!fwhole(p->xscale, &sx) || !fwhole(p->yscale, &sy)) return 0;
    ax = sx < 0 ? -sx : sx; ay = sy < 0 ? -sy : sy;
    if (ax < 1 || ax > 8 || ay < 1 || ay > 8) return 0;
    if (c->l < -127 || c->r > 126 || c->l - c->xo < -127 || c->l - c->xo > 127 || c->r + 1 - c->xo < -127 ||
        c->r + 1 - c->xo > 127 || c->t < -127 || c->b > 126 || c->t - c->yo < -127 || c->t - c->yo > 127 ||
        c->b + 1 - c->yo < -127 || c->b + 1 - c->yo > 127 || x < -16000 || x > 16000 || y < -16000 || y > 16000)
        return 0;
    l = sx >= 0 ? x + sx * (c->l - c->xo) : x + sx * (c->r + 1 - c->xo);
    t = sy >= 0 ? y + sy * (c->t - c->yo) : y + sy * (c->b + 1 - c->yo);
    p->bl = (int16_t)l; p->bt = (int16_t)t;
    p->br = (int16_t)(l + ax * (c->r - c->l + 1)); p->bb = (int16_t)(t + ay * (c->b - c->t + 1));
    return 1;
}

/* the cache kind of instance i's box (computed when a setter cleared it): with scales of exactly +-1 and whole x, y
   the double formula's results are the integers below */
static __attribute__((noinline)) int bbkind_set(int i)
{
    struct pin *p = &PW.in[i];
    {
        int s = spr_of(p);
        int32_t x, y;
        if (s < 0)
            p->bbk = BB_NOSPR;
        else {
            const struct gsprcol *c = &gsprcol[s];
            int xs = funit(p->xscale), ys = funit(p->yscale);
            if (xs && ys && fzero(p->angle) && pin_xy_int_p(p, &x, &y)) {
                int32_t l = xs > 0 ? x + (c->l - c->xo) : x - (c->r + 1 - c->xo);
                int32_t t = ys > 0 ? y + (c->t - c->yo) : y - (c->b + 1 - c->yo);
                PWST(bbox_int, 1);
                p->bl = (int16_t)l;
                p->br = (int16_t)(l + (c->r - c->l + 1));
                p->bt = (int16_t)t;
                p->bb = (int16_t)(t + (c->b - c->t + 1));
                p->bbk = BB_INT;
            } else
                p->bbk = !(xs && ys) && bbox_ints(p, c) ? BB_INTS : BB_DBL;
        }
    }
    return p->bbk;
}
static inline int bbkind(int i)
{
    int k = PW.in[i].bbk;
    return k ? k : bbkind_set(i);
}

int pin_bbox(int i, double *l, double *t, double *r, double *b)
{
    const struct pin *p = &PW.in[i];
    switch (bbkind(i)) {
    case BB_NOSPR:
        return 0;
    case BB_INT: case BB_INTS:
        *l = p->bl; *t = p->bt; *r = p->br; *b = p->bb;
        return 1;
    default:
        bbox_dbl(p, &gsprcol[spr_of(p)], l, t, r, b);
        return 1;
    }
}

/* the box as integers when it is cached so (BB_INT); 0 otherwise (no sprite, or not whole: use pin_bbox) */
__attribute__((always_inline)) inline int pin_ibox(int i, int32_t *b)
{
    const struct pin *p = &PW.in[i];
    if (bbkind(i) != BB_INT) return 0;
    b[0] = p->bl; b[1] = p->bt; b[2] = p->br; b[3] = p->bb;
    return 1;
}

/* pin_ibox, BB_INTS too (overlap_at) */
static inline int pin_ibox_s(int i, int32_t *b)
{
    const struct pin *p = &PW.in[i];
    int k = bbkind(i);
    if (k != BB_INT && k != BB_INTS) return 0;
    b[0] = p->bl; b[1] = p->bt; b[2] = p->br; b[3] = p->bb;
    return 1;
}

int pin_box_outside(int i, int w, int h)
{
    int32_t q[4];
    double l, t, r, b;
    if (pin_ibox(i, q)) return q[2] < 0 || q[0] > w || q[3] < 0 || q[1] > h;
    {   /* BB_DBL at scales +-1, angle 0 (debris at a fractional position): bbox_dbl's sides are x + ax, x + ax + cw and
           y + ay, y + ay + ch, exact doubles (|x| < 2^22, small ints), so each compare is x against an int: on pfr's
           v = 4 floor(x) + class, x < K is v < 4K and x > K is v > 4K */
        const struct pin *p = &PW.in[i];
        int32_t vx, vy, ax, ay;
        int xs, ys;
        if (p->bbk == BB_DBL && fzero(p->angle) && (xs = funit(p->xscale)) != 0 && (ys = funit(p->yscale)) != 0 &&
            pfr(p->x, &vx) && pfr(p->y, &vy)) {
            const struct gsprcol *c = &gsprcol[spr_of(p)];
            ax = xs > 0 ? c->l - c->xo : -(c->r + 1 - c->xo);
            ay = ys > 0 ? c->t - c->yo : -(c->b + 1 - c->yo);
            return vx < 4 * -(ax + (c->r - c->l + 1)) || vx > 4 * (w - ax) || vy < 4 * -(ay + (c->b - c->t + 1)) ||
                   vy > 4 * (h - ay);
        }
    }
    if (!pin_bbox(i, &l, &t, &r, &b)) return 0;
    return r < 0 || l > w || b < 0 || t > h;
}

/* v as an int when it is a whole number with |v| < 30000, from the double's bits (no soft-float): exponent p =
   e - 1023 in 0 .. 14 puts the integer part in the high word's mantissa bits (21 with the hidden one); their product
   with 2^(p + 12) has the integer part in its high word and the fraction bits in its low word */
static const uint32_t dw_mul[15] = { 1u << 12, 1u << 13, 1u << 14, 1u << 15, 1u << 16, 1u << 17, 1u << 18, 1u << 19,
                                     1u << 20, 1u << 21, 1u << 22, 1u << 23, 1u << 24, 1u << 25, 1u << 26 };
static int dwhole(double v, int32_t *o)
{
    union { double d; uint64_t u; } c;
    uint32_t hi, lo, e, m;
    uint64_t pr;
    c.d = v;
    hi = (uint32_t)(c.u >> 32);
    lo = (uint32_t)c.u;
    e = (hi >> 20) & 0x7ffu;
    if (e < 1023) {
        if ((hi & 0x7fffffffu) == 0 && lo == 0) { *o = 0; return 1; }     /* +-0 */
        return 0;
    }
    if (e > 1023 + 14 || lo != 0) return 0;
    m = (hi & 0xfffffu) | 0x100000u;
    pr = (uint64_t)m * dw_mul[e - 1023];
    if ((uint32_t)pr != 0 || (uint32_t)(pr >> 32) >= 30000u) return 0;
    *o = (hi & 0x80000000u) ? -(int32_t)(uint32_t)(pr >> 32) : (int32_t)(uint32_t)(pr >> 32);
    return 1;
}

/* floor(v) as an int when |v| < 30000 */
static int dfloor_int(double v, int32_t *o)
{
    if (dwhole(v, o)) return 1;
    if (!(v > -30000 && v < 30000)) return 0;
    *o = dfloor(v);
    return 1;
}

static int precise(int i)
{
    int s = spr_of(&PW.in[i]);
    return s >= 0 && gsprcol[s].kind == 1;
}

static int match(int k, int obj, int notme_self)
{
    PWST(visit, 1);
    return PW.in[k].alive && k != notme_self && (obj == -2 ? 1 : obj_is(PW.in[k].obj, obj));
}

/* sqrtf correctly rounded (the runner's sqrtss), without libm: an integer square root of the mantissa scaled to an
   even exponent, rounded to nearest (no tie can occur for a square root of a float). x >= 0 and finite here */
static float sqrtf_exact(float x)
{
    union { float f; uint32_t u; } v;
    uint32_t e, m;
    uint64_t n, r, b, q;
    int ex;
    v.f = x;
    if ((v.u & 0x7fffffffu) == 0) return x;
    e = (v.u >> 23) & 0xff;
    m = v.u & 0x7fffff;
    if (e == 0) { ex = -149; while (!(m & 0x800000)) { m <<= 1; ex--; } } else { m |= 0x800000; ex = (int)e - 150; }
    /* x = m * 2^ex, m in [2^23, 2^24): n = m * 2^24 (2^25 for an odd ex), x = n * 2^ex with ex even */
    n = (uint64_t)m << 24;
    ex -= 24;
    if (ex & 1) { n <<= 1; ex--; }
    r = 0;
    for (b = (uint64_t)1 << 30; b; b >>= 1)        /* r = floor(sqrt(n)), n < 2^49: 24 or 25 bits */
        if ((r + b) * (r + b) <= n) r += b;
    if (r >= ((uint64_t)1 << 24)) {                 /* 25 bits: drop one, round on it and the remainder */
        q = r >> 1;
        if ((r & 1) && (r * r < n || (q & 1))) q++;
        ex = ex / 2 + 1;
    } else {                                        /* 24 bits: round up when sqrt(n) > r + 0.5 */
        q = r;
        if (n - r * r > r) q++;
        ex = ex / 2;
    }
    v.f = (float)q;                                 /* q <= 2^24: exact */
    while (ex > 0) { v.f *= 2.0f; ex--; }
    while (ex < 0) { v.f *= 0.5f; ex++; }
    return v.f;
}

/* ---- precise collision of two instances, as the runner computes it (libyoyo, CInstance::Collision_Instance:
   SeparatingAxisCollision when one is rotated, then CSprite::PreciseCollision; all in float) ------------------- */
struct pcinst {
    float x, y, xs, ys, ang, bl, bt, br, bb;   /* position, scales, image_angle, the instance's bounding box */
    float xo, yo, ml, mt, mr, mb;               /* the sprite's origin and mask box */
    int bpr;
    const uint8_t *mask;                        /* NULL: the sprite has no mask (its box counts) */
};

static uint32_t pcf(float f) { union { float f; uint32_t u; } v; v.f = f; return v.u; }
static uint64_t pcd(double d) { union { double d; uint64_t u; } v; v.d = d; return v.u; }

/* (float)(v + d): v's own float when d is +0 (v + +0 is v but for -0, which gives +0), else the double sum */
static float pc_addf(double v, double d)
{
    if (pcd(d) == 0) return v == 0 ? 0.0f : (float)v;
    return (float)(v + d);
}

/* box: i's pin_bbox (l, t, r, b) when the caller has it (overlap_at: nothing changes between), else NULL */
static int pcinst_of_raw(int i, double dx, double dy, struct pcinst *q, const double *box)
{
    const struct pin *p = &PW.in[i];
    int s = spr_of(p);
    const struct gsprcol *c;
    const struct psprite *ps;
    double l, t, r, b;
    if (s < 0) return 0;
    if (box) { l = box[0]; t = box[1]; r = box[2]; b = box[3]; }
    else if (!pin_bbox(i, &l, &t, &r, &b)) return 0;
    c = &gsprcol[s];
    ps = &psprite[s];
    q->x = pcd(dx) == 0 ? (fzero(p->x) ? 0.0f : p->x) : (float)(PTOD(p->x) + dx);
    q->y = pcd(dy) == 0 ? (fzero(p->y) ? 0.0f : p->y) : (float)(PTOD(p->y) + dy);
    q->xs = p->xscale; q->ys = p->yscale; q->ang = p->angle;
    q->bl = pc_addf(l, dx); q->bt = pc_addf(t, dy); q->br = pc_addf(r, dx); q->bb = pc_addf(b, dy);
#ifdef PLAY_STATS
    {   /* the box given is pin_bbox's; the floats are the double sums' */
        double m[4];
        if ((box && (!pin_bbox(i, &m[0], &m[1], &m[2], &m[3]) || pcd(m[0]) != pcd(l) || pcd(m[1]) != pcd(t) ||
                     pcd(m[2]) != pcd(r) || pcd(m[3]) != pcd(b))) ||
            pcf(q->x) != pcf((float)(PTOD(p->x) + dx)) || pcf(q->y) != pcf((float)(PTOD(p->y) + dy)) ||
            pcf(q->bl) != pcf((float)(l + dx)) || pcf(q->bt) != pcf((float)(t + dy)) || pcf(q->br) != pcf((float)(r + dx)) ||
            pcf(q->bb) != pcf((float)(b + dy))) {
            fprintf(stderr, "pcinst_of_raw: box or floats differ (%d)\n", i);
            abort();
        }
    }
#endif
    q->xo = (float)c->xo; q->yo = (float)c->yo;
    q->ml = (float)c->l; q->mt = (float)c->t; q->mr = (float)c->r; q->mb = (float)c->b;
    q->bpr = ((c->r - c->l + 1) + 7) >> 3;
    q->mask = 0;
    if (c->kind == 1 && ps->nmasks > 0) {
        int f = 0;
        if (ps->nmasks > 1) {
            int fi = (int)p->img;                   /* cvttss2si, then the positive modulo */
            f = fi % ps->nmasks;
            if (f < 0) f += ps->nmasks;
        }
        q->mask = pmaskdata + ps->maskoff + f * q->bpr * (c->b - c->t + 1);
    }
    return 1;
}

/* pcinst_of through a 2-entry cache keyed by what it reads: x, y, the scales, the angle, the image index (the mask
   frame), the sprite or mask, and dx, dy (bits). Its result (the box through pin_bbox, the transform's floats and
   the mask frame's address) is a function of those and the static sprite tables, so a hit is the struct a call
   fills. The precise line and point tests rebuild it for every query against the same instance (the idol's
   rotating boulder: 27 a step, about 3.8 K jtcps3 clocks each in soft-float). PLAY_STATS builds compare every hit
   with a fresh pcinst_of_raw */
struct pcc { uint32_t x, y, xs, ys, ang, img; uint64_t dx, dy; int16_t s; uint8_t ok, ret; struct pcinst q; };
static struct pcc pccache[2];
static unsigned pcnext;


static int pcinst_of_b(int i, double dx, double dy, struct pcinst *q, const double *box)
{
    const struct pin *p = &PW.in[i];
    uint32_t x = pcf(p->x), y = pcf(p->y), xs = pcf(p->xscale), ys = pcf(p->yscale), ang = pcf(p->angle),
             img = pcf((float)p->img);
    uint64_t bx = pcd(dx), by = pcd(dy);
    int s = spr_of(p), k, r;
    for (k = 0; k < 2; k++) {
        const struct pcc *c = &pccache[k];
        if (c->ok && c->x == x && c->y == y && c->xs == xs && c->ys == ys && c->ang == ang && c->img == img &&
            c->s == s && c->dx == bx && c->dy == by) {
#ifdef PLAY_STATS
            struct pcinst f;
            int fr = pcinst_of_raw(i, dx, dy, &f, NULL);
            if (fr != c->ret || (fr && (pcf(f.x) != pcf(c->q.x) || pcf(f.y) != pcf(c->q.y) || pcf(f.xs) != pcf(c->q.xs) ||
                pcf(f.ys) != pcf(c->q.ys) || pcf(f.ang) != pcf(c->q.ang) || pcf(f.bl) != pcf(c->q.bl) ||
                pcf(f.bt) != pcf(c->q.bt) || pcf(f.br) != pcf(c->q.br) || pcf(f.bb) != pcf(c->q.bb) ||
                pcf(f.xo) != pcf(c->q.xo) || pcf(f.yo) != pcf(c->q.yo) || pcf(f.ml) != pcf(c->q.ml) ||
                pcf(f.mt) != pcf(c->q.mt) || pcf(f.mr) != pcf(c->q.mr) || pcf(f.mb) != pcf(c->q.mb) ||
                f.bpr != c->q.bpr || f.mask != c->q.mask))) {
                fprintf(stderr, "pcinst_of: cached transform differs (%d)\n", i);
                abort();
            }
#endif
            if (c->ret) *q = c->q;
            return c->ret;
        }
    }
    r = pcinst_of_raw(i, dx, dy, q, box);
    {
        struct pcc *c = &pccache[pcnext++ & 1];
        c->x = x; c->y = y; c->xs = xs; c->ys = ys; c->ang = ang; c->img = img; c->s = (int16_t)s;
        c->dx = bx; c->dy = by; c->ok = 1; c->ret = (uint8_t)r;
        if (r) c->q = *q;
    }
    return r;
}

static int pcinst_of(int i, double dx, double dy, struct pcinst *q) { return pcinst_of_b(i, dx, dy, q, NULL); }

static int pc_bit(const struct pcinst *q, float lx, float ly)
{
    int cx = (int)(lx - q->ml), cy = (int)(ly - q->mt);
    static const uint8_t bit[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
    return (q->mask[cy * q->bpr + (cx >> 3)] & bit[cx & 7]) != 0;
}

/* getPoints: the corners of the rotated mask box */
static void sa_points(const struct pcinst *q, float *pt)
{
    float L = (q->ml < q->mr ? q->ml : q->mr) - q->xo, R = (q->ml > q->mr ? q->ml : q->mr) - q->xo + 1.0f;
    float T, B, a, sn, cs;
    L = L * q->xs;
    R = R * q->xs;
    if (q->mb > q->mt) { T = (q->mt - q->yo) * q->ys; B = (q->mb - q->yo + 1.0f) * q->ys; }
    else { T = (q->mb - q->yo) * q->ys; B = (q->mt - q->yo + 1.0f) * q->ys; }
    a = q->ang * -3.14159274101257324f;
    a = a / 180.0f;
    pcol_sincosf(a, &sn, &cs);
    pt[0] = (L * cs + q->x) - T * sn;  pt[1] = L * sn + (T * cs + q->y);
    pt[2] = (R * cs + q->x) - T * sn;  pt[3] = (T * cs + q->y) + R * sn;
    pt[4] = (R * cs + q->x) - B * sn;  pt[5] = R * sn + (B * cs + q->y);
    pt[6] = (L * cs + q->x) - B * sn;  pt[7] = (B * cs + q->y) + L * sn;
}

/* one axis of sa_checkCollision: the normal of edge e0 -> e1; 0 when it separates */
static int sa_axis(const float *e0, const float *e1, const float *pa, const float *pb)
{
    float dx = e1[0] - e0[0], dy = e1[1] - e0[1], len, ax, ay, mina, maxa, minb, maxb, v;
    int k;
    len = sqrtf_exact(dx * dx + dy * dy);
    ay = dx / len;
    ax = -dy / len;
    mina = maxa = pa[0] * ax + pa[1] * ay;
    for (k = 1; k < 4; k++) {
        v = pa[2 * k] * ax + pa[2 * k + 1] * ay;
        if (mina > v) mina = v; else if (v > maxa) maxa = v;
    }
    minb = maxb = pb[0] * ax + pb[1] * ay;
    for (k = 1; k < 4; k++) {
        v = pb[2 * k] * ax + pb[2 * k + 1] * ay;
        if (minb > v) minb = v; else if (v > maxb) maxb = v;
    }
    return !(minb > maxa) && mina <= maxb;
}

static int sa_collision(const struct pcinst *a, const struct pcinst *b)
{
    float pa[8], pb[8];
    sa_points(a, pa);
    sa_points(b, pb);
    return sa_axis(pa, pa + 2, pa, pb) && sa_axis(pa + 2, pa + 4, pa, pb) &&
           sa_axis(pb, pb + 2, pa, pb) && sa_axis(pb + 2, pb + 4, pa, pb);
}

static int rotated_eps(float ang) { double d = ang; return d > GML_EPS || d < -GML_EPS; }

/* 1 / s for a scale; s = +-1: itself (1 / 1 and 1 / -1 are exact), without __divsf3 */
static float pc_inv(float s)
{
    union { float f; uint32_t u; } v;
    v.f = s;
    if ((v.u & 0x7fffffffu) == 0x3f800000u) return s;
    return 1.0f / s;
}

/* v * s; s = +-1: v or -v (exact for a finite v; the operands here are positions and box sides), without __mulsf3 */
static float pc_mul(float v, float s)
{
    union { float f; uint32_t u; } a, b;
    b.f = s;
    if (b.u == 0x3f800000u) return v;
    if (b.u == 0xbf800000u) { a.f = v; a.u ^= 0x80000000u; return a.f; }
    return v * s;
}

/* pc_bit at the mask column and row cx = (int)(lx - ml), cy = (int)(ly - mt) */
static int pc_bitc(const struct pcinst *q, int cx, int cy)
{
    static const uint8_t bit[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
    return (q->mask[cy * q->bpr + (cx >> 3)] & bit[cx & 7]) != 0;
}

/* precise_collision's unrotated loop as the runner has it (A: this sprite; x0 .. iyB as there) */
static int pc_loop(const struct pcinst *A, const struct pcinst *B, float x0, float y0, float x1, float y1, float ixA,
                   float ixB, float iyA, float iyB)
{
    float lxA, lxB, xc, yc;
    float arA = A->mr + 1.0f, abA = A->mb + 1.0f, arB = B->mr + 1.0f, abB = B->mb + 1.0f;
    if (!(x1 > x0)) return 0;
    lxB = (x0 - B->x) * ixB + B->xo;
    lxA = (x0 - A->x) * ixA + A->xo;
    for (xc = x0; x1 > xc; xc = xc + 1.0f, lxB = lxB + ixB, lxA = lxA + ixA) {
        int okA, okB;
        float tA, tB;
        if (A->ml > lxA || lxA >= arA || B->ml > lxB || lxB >= arB || !(y1 > y0)) continue;
        tA = (float)(int)lxA; tB = (float)(int)lxB;
        okA = !(A->ml > tA) && !(tA > A->mr);
        okB = !(B->ml > tB) && !(tB > B->mr);
        for (yc = y0; y1 > yc; yc = yc + 1.0f) {
            float lyA = (yc - A->y) * iyA + A->yo, lyB, ty;
            if (A->mt > lyA || lyA >= abA) continue;
            if (A->mask) {
                if (!okA) continue;
                ty = (float)(int)lyA;
                if (A->mt > ty || ty > A->mb || !pc_bit(A, tA, ty)) continue;
            }
            lyB = (yc - B->y) * iyB + B->yo;
            if (B->mt > lyB || lyB >= abB) continue;
            if (!B->mask) return 1;
            if (!okB) continue;
            ty = (float)(int)lyB;
            if (B->mt > ty || ty > B->mb) continue;
            if (pc_bit(B, tB, ty)) return 1;
        }
    }
    return 0;
}

#define PC_ROWS 64

/* precise_collision's unrotated loop (A: this sprite; x0 .. iyB as there). A row's tests depend on yc alone, and yc
   runs from y0 by + 1.0f in every column: each row's floats are computed once, when a column first needs them (the
   same operations on the same values, never more of them than pc_loop makes), kept in rw (bit 0 A's row in its
   box, 1 A's mask row, 2 B's row in its box, 3 B's mask row, 4 / 5 A's / B's computed; cy the mask rows), and the
   columns run on them in pc_loop's order. The rows' yc when the first column passes its tests (pc_loop adds them in
   each such column). More rows than PC_ROWS: pc_loop */
static int pc_rows(const struct pcinst *A, const struct pcinst *B, float x0, float y0, float x1, float y1, float ixA,
                   float ixB, float iyA, float iyB)
{
    float lxA, lxB, xc, yc;
    float arA = A->mr + 1.0f, abA = A->mb + 1.0f, arB = B->mr + 1.0f, abB = B->mb + 1.0f;
    float ycs[PC_ROWS];
    uint8_t rw[PC_ROWS];
    int16_t cyA[PC_ROWS], cyB[PC_ROWS];
    int n = -1, k;
    if (!(x1 > x0)) return 0;
    lxB = pc_mul(x0 - B->x, ixB) + B->xo;
    lxA = pc_mul(x0 - A->x, ixA) + A->xo;
    for (xc = x0; x1 > xc; xc = xc + 1.0f, lxB = lxB + ixB, lxA = lxA + ixA) {
        int okA, okB, cxA = 0, cxB = 0;
        float tA, tB;
        if (A->ml > lxA || lxA >= arA || B->ml > lxB || lxB >= arB || !(y1 > y0)) continue;
        if (n < 0) {
            for (n = 0, yc = y0; y1 > yc; yc = yc + 1.0f) {
                if (n == PC_ROWS) return pc_loop(A, B, x0, y0, x1, y1, ixA, ixB, iyA, iyB);
                rw[n] = 0;
                ycs[n++] = yc;
            }
        }
        tA = (float)(int)lxA; tB = (float)(int)lxB;
        okA = !(A->ml > tA) && !(tA > A->mr);
        okB = !(B->ml > tB) && !(tB > B->mr);
        if (A->mask && okA) cxA = (int)(tA - A->ml);
        if (B->mask && okB) cxB = (int)(tB - B->ml);
        for (k = 0; k < n; k++) {
            int f = rw[k];
            if (!(f & 16)) {
                float ly = pc_mul(ycs[k] - A->y, iyA) + A->yo, ty;
                f |= 16;
                if (!(A->mt > ly || ly >= abA)) {
                    f |= 1;
                    if (A->mask) {
                        ty = (float)(int)ly;
                        if (!(A->mt > ty || ty > A->mb)) { f |= 2; cyA[k] = (int16_t)(int)(ty - A->mt); }
                    }
                }
                rw[k] = (uint8_t)f;
            }
            if (!(f & 1)) continue;
            if (A->mask && (!okA || !(f & 2) || !pc_bitc(A, cxA, cyA[k]))) continue;
            if (!(f & 32)) {
                float ly = pc_mul(ycs[k] - B->y, iyB) + B->yo, ty;
                f |= 32;
                if (!(B->mt > ly || ly >= abB)) {
                    f |= 4;
                    if (B->mask) {
                        ty = (float)(int)ly;
                        if (!(B->mt > ty || ty > B->mb)) { f |= 8; cyB[k] = (int16_t)(int)(ty - B->mt); }
                    }
                }
                rw[k] = (uint8_t)f;
            }
            if (!(f & 4)) continue;
            if (!B->mask) return 1;
            if (!okB || !(f & 8)) continue;
            if (pc_bitc(B, cxB, cyB[k])) return 1;
        }
    }
    return 0;
}

/* CSprite::PreciseCollision (A: this sprite) */
static int precise_collision(const struct pcinst *A, const struct pcinst *B)
{
    float bl = A->bl > B->bl ? A->bl : B->bl, bt = A->bt > B->bt ? A->bt : B->bt;
    float br = A->br < B->br ? A->br : B->br, bb = A->bb < B->bb ? A->bb : B->bb;
    float x0, y0, x1, y1, xc, yc, ixA, ixB, iyA, iyB;
    float arA = A->mr + 1.0f, abA = A->mb + 1.0f, arB = B->mr + 1.0f, abB = B->mb + 1.0f;
    int rA, rB;
    if (A->xs == 0 || A->ys == 0 || B->xs == 0 || B->ys == 0) return 0;
    x0 = (float)((int)(bl + 32768.0f) - 32768) + 0.5f;
    y0 = (float)((int)(bt + 32768.0f) - 32768) + 0.5f;
    x1 = (float)(32768 - (int)(32768.0f - br));
    y1 = (float)(32768 - (int)(32768.0f - bb));
    ixA = pc_inv(A->xs); ixB = pc_inv(B->xs); iyA = pc_inv(A->ys); iyB = pc_inv(B->ys);
    rA = rotated_eps(A->ang);
    rB = rotated_eps(B->ang);
    if (!rA && !rB) {
        int r = pc_rows(A, B, x0, y0, x1, y1, ixA, ixB, iyA, iyB);
#ifdef PLAY_STATS
        if (r != pc_loop(A, B, x0, y0, x1, y1, ixA, ixB, iyA, iyB)) {
            fprintf(stderr, "precise_collision: row answer %d differs\n", r);
            abort();
        }
#endif
        return r;
    }
    {
        float sA = 0, cA = 0, sB = 0, cB = 0;
        if (rA) { float a = A->ang * -3.14159274101257324f; a = a / 180.0f; pcol_sincosf(a, &sA, &cA); }
        if (rB) { float a = B->ang * -3.14159274101257324f; a = a / 180.0f; pcol_sincosf(a, &sB, &cB); }
        for (xc = x0; x1 > xc; xc = xc + 1.0f) {
            float dxA, dxB, nA, nB;
            if (!rA) {
                float l = (xc - A->x) * ixA + A->xo;
                if (A->ml > l || l >= arA) continue;
            }
            if (!rB) {
                float l = (xc - B->x) * ixB + B->xo;
                if (B->ml > l || l >= arB) continue;
            }
            if (!(y1 > y0)) continue;
            dxA = xc - A->x; nA = dxA * -sA;
            dxB = xc - B->x; nB = dxB * -sB;
            for (yc = y0; y1 > yc; yc = yc + 1.0f) {
                float dy = yc - A->y, lxA, lyA, lxB, lyB, t;
                if (rA) {
                    lxA = (cA * dxA + sA * dy) * ixA + A->xo;
                    if (A->ml > lxA || lxA >= arA) continue;
                    dy = dy * cA + nA;
                } else
                    lxA = (xc - A->x) * ixA + A->xo;
                lyA = dy * iyA + A->yo;
                if (A->mt > lyA || lyA >= abA) continue;
                if (A->mask) {
                    float tx = (float)(int)lxA;
                    if (A->ml > tx || tx > A->mr) continue;
                    t = (float)(int)lyA;
                    if (A->mt > t || t > A->mb || !pc_bit(A, tx, t)) continue;
                }
                dy = yc - B->y;
                if (rB) {
                    lxB = (cB * dxB + sB * dy) * ixB + B->xo;
                    if (B->ml > lxB || lxB >= arB) continue;
                    dy = dy * cB + nB;
                } else
                    lxB = (xc - B->x) * ixB + B->xo;
                lyB = dy * iyB + B->yo;
                if (B->mt > lyB || lyB >= abB) continue;
                if (!B->mask) return 1;
                {
                    float tx = (float)(int)lxB;
                    if (B->ml > tx || tx > B->mr) continue;
                    t = (float)(int)lyB;
                    if (B->mt > t || t > B->mb) continue;
                    if (pc_bit(B, tx, t)) return 1;
                }
            }
        }
        return 0;
    }
}

/* CSprite::PreciseCollisionPoint (non-compatibility mode): the point in the sprite's frame (the instance at its
   truncated position, rotated back by image_angle unless |angle| < 1e-4), floor, inside the mask box, the bit */
static int precise_point(int i, float px, float py)
{
    const struct pin *p = &PW.in[i];
    int s = spr_of(p), f = 0;
    const struct gsprcol *c = &gsprcol[s];
    const struct psprite *ps = &psprite[s];
    float X = (float)(int32_t)PTOD(p->x), Y = (float)(int32_t)PTOD(p->y), xs = p->xscale, ys = p->yscale;
    float ang = p->angle, lx, dyv, ly;
    int bpr, cx, cy;
    static const uint8_t bit[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
    if (c->kind != 1 || ps->nmasks == 0) return 1;
    if ((ang < 0 ? -ang : ang) < 9.99999974737875e-05f) {
        lx = (float)dfloor((px - X) / xs + (float)c->xo);
        dyv = py - Y;
    } else {
        float a = ang * -3.14159274101257324f, sn, cs, dx = px - X, dy = py - Y;
        a = a / 180.0f;
        pcol_sincosf(a, &sn, &cs);
        lx = (float)dfloor((cs * dx + sn * dy) / xs + (float)c->xo);
        dyv = dy * cs - sn * dx;
    }
    if ((float)c->l > lx || lx > (float)c->r) return 0;
    ly = (float)dfloor(dyv / ys + (float)c->yo);
    if ((float)c->t > ly || ly > (float)c->b) return 0;
    if (ps->nmasks > 1) {
        f = (int)p->img % ps->nmasks;
        if (f < 0) f += ps->nmasks;
    }
    bpr = ((c->r - c->l + 1) + 7) >> 3;
    cx = (int)(lx - (float)c->l);
    cy = (int)(ly - (float)c->t);
    return (pmaskdata[ps->maskoff + f * bpr * (c->b - c->t + 1) + cy * bpr + (cx >> 3)] & bit[cx & 7]) != 0;
}

/* a point query: px >= l && px < r with l, r whole is floor(px) >= l && floor(px) < r */
struct pq { double px, py; int32_t ix, iy; int iok, nodbl; };   /* nodbl: px, py not set; they are ix, iy (whole) */

static void pq_init(struct pq *q, double px, double py)
{
    if (dwhole(px, &q->ix) && dwhole(py, &q->iy)) {   /* (float) of a whole number below 2^24 is itself */
        q->px = px;
        q->py = py;
        q->iok = 1;
        q->nodbl = 0;
        return;
    }
    px = (float)px;                                  /* CInstance::Collision_Point takes floats */
    py = (float)py;
    q->px = px;
    q->py = py;
    q->nodbl = 0;
    q->iok = dfloor_int(px, &q->ix) && dfloor_int(py, &q->iy);
}

static int point_hit(int k, const struct pq *q, int prec)
{
    double l, t, r, b;
    int32_t ib[4];
    if (q->iok && pin_ibox(k, ib)) {
        if (!(q->ix >= ib[0] && q->ix < ib[2] && q->iy >= ib[1] && q->iy < ib[3]))
            return 0;
    } else {
        double px = q->nodbl ? (double)q->ix : q->px, py = q->nodbl ? (double)q->iy : q->py;
        if (!pin_bbox(k, &l, &t, &r, &b))
            return 0;
        if (!(px >= l && px < r && py >= t && py < b))
            return 0;
    }
    if (!prec || !precise(k)) return 1;
    return q->nodbl ? precise_point(k, (float)q->ix, (float)q->iy) : precise_point(k, (float)q->px, (float)q->py);
}

/* ---- the solid grid: every alive instance of the oSolid family with a sprite, in the 16 px cell of its box's
   top-left corner (clamped to GRID_W x GRID_H); gmax* the widest / tallest box in cells. A point is in a box whose
   cell is within gmax cells up-left of the point's. Instances whose box changed wait on gdirty until the next
   grid query ---------------------------------------------------------------------------------------------- */

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void grid_reset(void)
{
    int x, y;
    for (y = 0; y < GRID_H; y++)
        for (x = 0; x < GRID_W; x++) { ghead[y][x] = thead[y][x] = NOONE; gfull[y][x] = 0; gother[y][x] = 0; }
    for (x = 0; x < PIN_MAX; x++) gkind[x] = 0;
    gdhead = NOONE;
    gtick();                                      /* every cell changed (the rest records of pobj.c) */
    for (y = 0; y < GRID_H; y++)
        for (x = 0; x < GRID_W; x++) gver[y][x] = gclock;
    gmaxw = gmaxh = tmaxw = tmaxh = 1;
    gfar = 0;
#ifndef PCOL_EXACT
    if (!xbits_ok) {                             /* (the object tree is fixed: once) */
        for (x = 0; x < OBJ_COUNT; x++) {
            int f, b = 0;
            xf_of[x] = -1;
            for (f = 0; f < XF_N; f++) {
                if (obj_is(x, xf_obj[f])) b |= 1 << f;
                if (x == xf_obj[f]) xf_of[x] = (int8_t)f;
            }
            if (obj_is(x, OBJ_oLeaves)) b |= 1 << 7;
            if (x == OBJ_oLeaves) xf_of[x] = 7;
            xbits[x] = (uint8_t)b;
        }
        xbits_ok = 1;
    }
    for (x = 0; x < PIN_MAX; x++) { xmask[x] = xond[x] = 0; xr[x].isfar = 0; }
    for (x = 0; x < XF_N; x++) {
        xfar[x] = 0;
        xemp[x] = 0;
        xsat[x] = 0;
        for (y = 0; y < GRID_H; y++) { int c; for (c = 0; c < GRID_W; c++) xcnt[x][y][c] = 0; }
    }
    for (y = 0; y < GRID_H; y++)
        for (x = 0; x < GRID_W; x++) xhint[y][x] = NOONE;
    xdhead = NOONE;
#endif
}

static void gsum_out(int i)
{
    int x, y;
    gtick();
    if (gkind[i] == 1) {
        x = gox0[i]; y = goy0[i];
        gver[y][x] = gclock;
        if (--gfull[y][x] && gfblk[y][x] == i) {        /* another block of the cell stands for it (in its list) */
            int k;
            for (k = ghead[y][x]; k >= 0 && (k == i || gkind[k] != 1); k = gnext[k]) {}
            gfblk[y][x] = (int16_t)k;
        }
    } else
        for (y = goy0[i]; y <= goy1[i]; y++)
            for (x = gox0[i]; x <= gox1[i]; x++) { gother[y][x]--; gver[y][x] = gclock; }
    gkind[i] = 0;
}

static void gsum_in(int i, int x0, int y0, int x1, int y1, int block)
{
    int x, y;
    gox0[i] = (uint8_t)x0; goy0[i] = (uint8_t)y0; gox1[i] = (uint8_t)x1; goy1[i] = (uint8_t)y1;
    gtick();
    if (block) {
        gver[y0][x0] = gclock;
        gkind[i] = 1;
        if (gfull[y0][x0]++ == 0) gfblk[y0][x0] = (int16_t)i;
        return;
    }
    gkind[i] = 2;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) { gother[y][x]++; gver[y][x] = gclock; }
}

/* the summary's change clock (solid changes still pending get a later one when they are applied) */
uint32_t pw_rest_clock(void) { return (uint32_t)gepoch << 16 | gclock; }

/* no oSolid-family entry was put in or taken out of the cells of [l, r] x [t, b] (clamped as the queries clamp)
   after clock `since`, and none is far out (the line queries' tree fallback) */
int pw_rest_still(int32_t l, int32_t t, int32_t r, int32_t b, uint32_t since)
{
    int x, y, x0, x1, y0, y1;
    grid_flush();
    if (gfar || (uint16_t)(since >> 16) != gepoch) return 0;
    x0 = clampi(l >> 4, 0, GRID_W - 1); x1 = clampi(r >> 4, 0, GRID_W - 1);
    y0 = clampi(t >> 4, 0, GRID_H - 1); y1 = clampi(b >> 4, 0, GRID_H - 1);
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            if (gver[y][x] > (uint16_t)since) return 0;
    return 1;
}

void pw_watch(int i) { watch_i = (int16_t)i; watch_n = 0; }
uint32_t pw_watch_end(void) { watch_i = NOONE; return watch_n; }

static void grid_unlink(int i)
{
    int c = gcell[i];
    int16_t *pp;
    if (gkind[i]) gsum_out(i);
    if (c < 0) {
        if (c == GCELL_FAR) { gfar--; gcell[i] = NOONE; }
        return;
    }
    pp = c >= GRID_W * GRID_H ? &thead[(c - GRID_W * GRID_H) / GRID_W][(c - GRID_W * GRID_H) % GRID_W]
                              : &ghead[c / GRID_W][c % GRID_W];
    for (; *pp != i; pp = &gnext[*pp]) {}
    *pp = gnext[i];
    gcell[i] = NOONE;
}


static __attribute__((noinline)) void grid_dirty_new(int i, int o)
{
    int g = 1 | (obj_is(o, OBJ_oSolid) || !pin_needs_ext(o) ? 2 : 0);
#ifndef PCOL_EXACT
    if (xbits[o]) g |= 4;
#endif
    pwob[o] = (uint8_t)((pwob[o] & 0xf0) | g);
    grid_dirty(i);
}

/* a solid's box may have changed (or it was added): placed again at the next query (a leaf but for the first call per
   object: no frame) */
static inline void grid_dirty_g(int i, int o, int g)
{
#ifdef PLAY_STATS
    if (!(g & 2) != !(obj_is(o, OBJ_oSolid) || !pin_needs_ext(o))) { fprintf(stderr, "grid_dirty: gobj[%d]\n", o); abort(); }
#ifndef PCOL_EXACT
    if (!(g & 4) != !xbits[o]) { fprintf(stderr, "grid_dirty: gobj[%d] xbits\n", o); abort(); }
#endif
#endif
#ifndef PCOL_EXACT
    if (g & 4) {
        xchg++;
        if (!xond[i]) xdirty(i);
    }
#endif
    if (!(g & 2) || gond[i]) return;
    gond[i] = 1;
    gdnext[i] = gdhead;
    gdhead = (int16_t)i;
}

static void grid_dirty(int i)
{
    int o = PW.in[i].obj, g = GOBJ(o);
    if (!g) {
        grid_dirty_new(i, o);
        return;
    }
    grid_dirty_g(i, o, g);
}

static __attribute__((noinline)) void grid_flush_run(void)
{
    while (gdhead >= 0) {
        int i = gdhead, cx, cy, w, h, solid, block = 0;
        int32_t ib[4], sl, st, sr, sb;
        double l, t, r, b;
        gdhead = gdnext[i];
        gond[i] = 0;
        grid_unlink(i);
        if (!PW.in[i].alive) continue;
        if (pin_ibox(i, ib)) {
            cx = ib[0] >> 4; cy = ib[1] >> 4;
            w = ((ib[2] - ib[0]) >> 4) + 1; h = ((ib[3] - ib[1]) >> 4) + 1;
            sl = ib[0]; st = ib[1]; sr = ib[2]; sb = ib[3];
            block = (ib[0] & 15) == 0 && (ib[1] & 15) == 0 && ib[2] == ib[0] + 16 && ib[3] == ib[1] + 16 &&
                    cx >= 0 && cx < GRID_W && cy >= 0 && cy < GRID_H;
        } else if (pin_bbox(i, &l, &t, &r, &b) && l > -30000 && l < 30000 && t > -30000 && t < 30000 &&
                   r - l < 30000 && b - t < 30000) {
            cx = dfloor(l) >> 4; cy = dfloor(t) >> 4;
            w = (dfloor(r - l) >> 4) + 2; h = (dfloor(b - t) >> 4) + 2;
            sl = dfloor(l); st = dfloor(t); sr = dfloor(r) + 1; sb = dfloor(b) + 1;
            if (sr < sl + 1) sr = sl + 1;                 /* (a box with r < l: its cells still counted) */
            if (sb < st + 1) sb = st + 1;
        } else {
            if (bbkind(i) != BB_NOSPR && obj_is(PW.in[i].obj, OBJ_oSolid)) {
                gcell[i] = GCELL_FAR;             /* a box far out (|side| >= 30000): not in a cell */
                gfar++;
            }
            continue;                             /* no sprite: never hit */
        }
        solid = obj_is(PW.in[i].obj, OBJ_oSolid);
        if (solid) {
            if (w > gmaxw) gmaxw = w;
            if (h > gmaxh) gmaxh = h;
        } else {
            if (w > tmaxw) tmaxw = w;
            if (h > tmaxh) tmaxh = h;
        }
        cx = clampi(cx, 0, GRID_W - 1);
        cy = clampi(cy, 0, GRID_H - 1);
        if (solid) {
            if (block) gsum_in(i, cx, cy, cx, cy, 1);     /* (cx, cy in the grid: not clamped) */
            else gsum_in(i, clampi((sl - 1) >> 4, 0, GRID_W - 1), clampi((st - 1) >> 4, 0, GRID_H - 1),
                         clampi(sr >> 4, 0, GRID_W - 1), clampi(sb >> 4, 0, GRID_H - 1), 0);
            gcell[i] = (int16_t)(cy * GRID_W + cx);
            gnext[i] = ghead[cy][cx];
            ghead[cy][cx] = (int16_t)i;
        } else {                                  /* cell index + GRID_W * GRID_H: the terrain list */
            gcell[i] = (int16_t)(GRID_W * GRID_H + cy * GRID_W + cx);
            gnext[i] = thead[cy][cx];
            thead[cy][cx] = (int16_t)i;
        }
    }
}

/* the drawing's walk of the terrain near the view (src/draw): the grid's cells, each holding the oSolid-family and
   terrain instances (pin_needs_ext 0) whose box's top-left corner is in that cell (clamped to GRID_W x GRID_H);
   a box reaches at most pw_grid_extent cells right / down of its cell */
void pw_grid_sync(void) { grid_flush(); }
int pw_grid_cell(int cx, int cy) { return ghead[cy][cx]; }
int pw_grid_tcell(int cx, int cy) { return thead[cy][cx]; }
int pw_grid_next(int k) { return gnext[k]; }
void pw_grid_extent(int *w, int *h)
{
    *w = gmaxw > tmaxw ? gmaxw : tmaxw;
    *h = gmaxh > tmaxh ? gmaxh : tmaxh;
}

/* the oldest instance of obj (oSolid or a descendant; but notme) whose box (and mask) holds the point */
static int grid_point(int obj, int notme, const struct pq *q, int prec)
{
    int x0, x1, y0, y1, x, y, best = NOONE;
    grid_flush();
    x1 = clampi(q->ix >> 4, 0, GRID_W - 1);
    y1 = clampi(q->iy >> 4, 0, GRID_H - 1);
    x0 = clampi((q->ix >> 4) - gmaxw, 0, GRID_W - 1);
    y0 = clampi((q->iy >> 4) - gmaxh, 0, GRID_H - 1);
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            int k;
            for (k = ghead[y][x]; k >= 0; k = gnext[k]) {
                PWST(visit, 1);
                if ((best >= 0 && PIN_OLDER(best, k)) || k == notme || !obj_is(PW.in[k].obj, obj)) continue;
                if (point_hit(k, q, prec)) best = k;
            }
        }
    return best;
}

#ifndef PCOL_EXACT
static void xplace(int i, int d)
{
    int f, x, y;
    for (f = 0; f < XF_N; f++) {
        if (!(xmask[i] >> f & 1)) continue;
        if (xr[i].isfar == 1) { xfar[f] += d; continue; }
        if (xr[i].isfar == 2) { xemp[f] += d; continue; }
        for (y = xr[i].y0; y <= xr[i].y1; y++)
            for (x = xr[i].x0; x <= xr[i].x1; x++) {
                if (d > 0 && xcnt[f][y][x] == 255) xsat[f] = 1;   /* (then the counts are not read until the reset) */
                xcnt[f][y][x] += d;
            }
    }
}

static __attribute__((noinline)) void xflush_run(void)
{
    while (xdhead >= 0) {
        int i = xdhead, b;
        int32_t ib[4];
        xdhead = xdnext[i];
        xond[i] = 0;
        xplace(i, -1);
        xmask[i] = 0;
        xr[i].isfar = 0;
        if (!PW.in[i].alive || !(b = xbits[PW.in[i].obj]) || bbkind(i) == BB_NOSPR) continue;
        xmask[i] = (uint8_t)b;
        if (pin_ibox(i, ib)) {
            if (ib[2] <= ib[0] || ib[3] <= ib[1]) {                  /* empty: never hit by a point; counted in xemp */
                xr[i].isfar = 2;                                     /* (xpoint_any tests it with point_hit: a miss) */
                xplace(i, 1);
                continue;
            }
            xr[i].x0 = (int8_t)clampi(ib[0] >> 4, 0, GRID_W - 1); xr[i].x1 = (int8_t)clampi((ib[2] - 1) >> 4, 0, GRID_W - 1);
            xr[i].y0 = (int8_t)clampi(ib[1] >> 4, 0, GRID_H - 1); xr[i].y1 = (int8_t)clampi((ib[3] - 1) >> 4, 0, GRID_H - 1);
            {
                int x, y, fx0 = clampi((ib[0] + 15) >> 4, 0, GRID_W), fx1 = clampi(ib[2] >> 4, 0, GRID_W);
                int fy0 = clampi((ib[1] + 15) >> 4, 0, GRID_H), fy1 = clampi(ib[3] >> 4, 0, GRID_H);
                for (y = fy0; y < fy1; y++)
                    for (x = fx0; x < fx1; x++) xhint[y][x] = (int16_t)i;
            }
        } else
            xr[i].isfar = 1;
        xplace(i, 1);
    }
}

/* the cell's hint is an alive instance of obj's family, not notme, holding the whole point q: some instance of the
   family but notme holds it (what xpoint_any finds; point_hit's box cache is its only side effect, as there) */
static int xhint_hit(int obj, int notme, const struct pq *q, int prec)
{
    int k;
    if (!q->iok || q->ix < 0 || q->iy < 0 || (q->ix >> 4) >= GRID_W || (q->iy >> 4) >= GRID_H) return 0;
    k = xhint[q->iy >> 4][q->ix >> 4];
    return k >= 0 && k != notme && PW.in[k].alive && obj_is(PW.in[k].obj, obj) && point_hit(k, q, prec);
}

/* no entry of family f has a box holding the whole point q (a point is in a box's cell) */
static int xpoint_none(int f, const struct pq *q)
{
    int cx, cy;
    if (!q->iok || q->ix < 0 || q->iy < 0 || (cx = q->ix >> 4) >= GRID_W || (cy = q->iy >> 4) >= GRID_H) return 0;
    if (xdhead >= 0) xflush_run();
    return xfar[f] == 0 && !xsat[f] && xcnt[f][cy][cx] == 0;
}
#endif

/* ---- the integer kernel (PERF3 2.4): the oSolid summary of whole-number queries, one routine for the axis lines,
   the rectangles and the points (ik_line, rect_any_i, solid_point_sum: their code and the grid's lines stay in the cache
   across a Step's queries) ---------------------------------------------------------------------------------------- */

/* the oSolid cell summary (after grid_flush) of the query [l, r) x [t, b) (half-open, whole): its cells l >> 4 ..
   (r - c) >> 4 and t >> 4 .. (b - c) >> 4, clamped, c = nmf & 1. 1 when a cell block (an entry whose integer box is
   exactly its cell) meets the query and is not notme, nor precise under prec; 0 when no block meets it and no other
   entry reaches these cells (gother); -1 otherwise. nmf = notme * 4 + prec * 2 + c (no stack argument).
   - an axis line [lx, hx] x [ly, hy] (line_hit's integer path, closed) is [lx, hx + 1) x [ly, hy + 1) with c 1: the
     block cell [cl, cl + 16) meets [lx, hx] when lx <= cl + 15 and hx >= cl, which is max(lx, cl) < min(hx + 1, cl + 16),
     and its cells lx >> 4 .. hx >> 4 are line_summary's (until 2026-10-06 its own loop);
   - a rectangle (rect_hit on whole boxes) is itself with c 0: rect_any_i's loop, cells to r >> 4;
   - a point (x, y) in the grid is [x, x + 1) x [y, y + 1) with c 1: its one cell, whose block always meets it:
     solid_point_sum's tests (1: a block, not notme, not precise under prec; 0: no other entry and no block but notme) */
static __attribute__((noinline)) int ik_sum(int32_t l, int32_t t, int32_t r, int32_t b, int nmf)
{
    int notme = nmf >> 2, prec = nmf & 2, c = nmf & 1, sure = 1, x, y, k;
    int x0 = clampi(l >> 4, 0, GRID_W - 1), xe = clampi((r - c) >> 4, 0, GRID_W - 1);
    int y0 = clampi(t >> 4, 0, GRID_H - 1), ye = clampi((b - c) >> 4, 0, GRID_H - 1);
    for (y = y0; y <= ye; y++) {
        int32_t ct = y * 16;
        int yin = (t > ct ? t : ct) < (b < ct + 16 ? b : ct + 16);
        for (x = x0; x <= xe; x++) {
            int n = gfull[y][x];
            int32_t cl = x * 16;
            if (gother[y][x]) sure = 0;
            if (n == 0) continue;
            k = gfblk[y][x];
            if (k == notme || !yin || !((l > cl ? l : cl) < (r < cl + 16 ? r : cl + 16))) {
                if (n > 1) sure = 0;                      /* k is a miss; another block may not be */
                continue;
            }
            if (!prec || !precise(k)) return 1;
            sure = 0;
        }
    }
    return sure ? 0 : -1;
}
#define IK_NMF(notme, prec, c) ((notme) * 4 + (prec) * 2 + (c))

/* ik_sum on a query inside the grid that is one cell wide or tall (an axis line, a point): the n cells from cell
   index i (y * GRID_W + x) by step (1: a row, GRID_W: a column), nm = notme * 2 + prec. Every one of these cells
   meets the query (no clamping; the line's cells are its own), so a block misses only as notme: per cell, gother
   makes the answer unsure, a block not notme is a hit unless precise under prec (then unsure), notme's block with
   another block there is unsure. ik_sum's answers, without the clamps, the overlap tests and the spills of its 2-D
   loop (four register arguments, a leaf) */
static __attribute__((noinline)) int ik_cells(int i, int n, int step, int nm)
{
    const uint8_t *f = &gfull[0][0] + i;
    const uint16_t *o = &gother[0][0] + i;
    const int16_t *b = &gfblk[0][0] + i;
    int notme = nm >> 1, sure = 1;
    for (;;) {
        int c = *f;
        if (*o) sure = 0;
        if (c) {
            int k = *b;
            if (k != notme) {
                if (!(nm & 1) || !precise(k)) return 1;
                sure = 0;
            } else if (c > 1)
                sure = 0;                                 /* notme's block; another block may hit */
        }
        if (--n == 0) break;
        f += step; o += step; b += step;
    }
    return sure ? 0 : -1;
}

/* Command_CollisionPoint tests the object's instances in creation order (Collision_Point computes each stale box:
   pcol_touch) */
/* collision_point_p's search after pq_init (pq_q: a query made of whole ints, nodbl, gives what pq_init gives for
   those values as doubles) */
static int point_q(const struct pq *q, int obj, int prec, int notme_self)
{
    int k;
    struct fam it;
#ifndef PCOL_EXACT
    if (obj >= 0 && xf_of[obj] >= 0 && !pcol_quiet() && xpoint_none(xf_of[obj], q)) {
#ifdef PLAY_STATS
        fam_begin(&it, obj);
        while ((k = fam_get(&it)) != NOONE)
            if (k != notme_self && point_hit(k, q, prec)) {
                fprintf(stderr, "collision_point_p: static-family index miss differs (%d %d %d)\n", obj, q->ix, q->iy);
                abort();
            }
#endif
        return NOONE;
    }
#endif
    if (q->iok && obj >= 0 && obj_is(obj, OBJ_oSolid) && !pcol_quiet()) {
        k = grid_point(obj, notme_self, q, prec);
        pcol_touch_stale(obj, notme_self, k);
        return k;
    }
    fam_begin(&it, obj);
    while ((k = fam_get(&it)) != NOONE) {
        if (k == notme_self) continue;
        pcol_touch(k);
        if (point_hit(k, q, prec))
            return k;
    }
    return NOONE;
}

int (collision_point_p)(double px, double py, int obj, int prec, int notme_self)
{
    struct pq q;
    PWST(point, 1);
    if (fam_none(obj)) return NOONE;
    pq_init(&q, px, py);
    return point_q(&q, obj, prec, notme_self);
}

/* collision_point(px, py, obj, prec, notme) != noone. For oSolid in the grid build (PCOL_EXACT keeps collision_point_p),
   whole point cells from the solid grid's summary: the point's cell holds a block (a box of exactly that cell) that
   is not the caller and not precise: a hit; no block there and no other oSolid-family entry reaching the cell
   (gother): a miss (a block's box is its own cell); otherwise collision_point_p. Skipped: the stale touches of the
   scan (pcol_touch_stale), which the grid build's searches do not depend on (pobj.c PLAY_REST). The host builds
   compare every summary answer */

#ifndef PCOL_EXACT
/* some instance of static family obj (xf_of) but notme holds the whole point q: point_hit over the family's object
   lists (ofam_next's preorder, subtrees with olive 0 skipped), in any order and without the touches */
static int xpoint_any(int obj, int notme, const struct pq *q, int prec)
{
    int j, k, f = xf_of[obj], cx = -1, cy = -1;
    /* the index's cells (xflush_run brings them up to date): an instance whose integer box holds the whole point
       reaches the point's cell (xplace's [l, r - 1] x [t, b - 1] in cells, clamped as the point is in the grid);
       one not placed (xmask 0: no sprite, an empty box) is never hit; far ones (box not BB_INT) take point_hit */
    if (xdhead >= 0) xflush_run();
    if (q->iok && q->ix >= 0 && q->iy >= 0 && (q->ix >> 4) < GRID_W && (q->iy >> 4) < GRID_H) {
        cx = q->ix >> 4;
        cy = q->iy >> 4;
    }
    for (j = obj; j >= 0; j = ofam_next(obj, j)) {
        int o = j;
        if (olive[o] == 0) continue;
        for (k = pw_ohead[o]; k >= 0; k = pw_inext[k]) {
            if (k == notme) continue;
            if (cx >= 0 && !xr[k].isfar && (!(xmask[k] & xf_bit[f]) || cx < xr[k].x0 || cx > xr[k].x1 || cy < xr[k].y0 ||
                                          cy > xr[k].y1))
                continue;
            if (point_hit(k, q, prec)) return 1;
        }
    }
    return 0;
}
#endif

#ifndef PCOL_EXACT
/* collision_point_any's answer for a static family (xf_of[obj] >= 0, pcol_quiet() 0, obj alive) at the query q */
static int xstatic_any(int obj, int notme, const struct pq *q, int prec)
{
    if (xpoint_none(xf_of[obj], q)) return 0;
    return xhint_hit(obj, notme, q, prec) || xpoint_any(obj, notme, q, prec);
}
#endif

#ifndef PCOL_EXACT
/* the oSolid summary's answer at query q (gfar 0, pcol_quiet() 0, oSolid alive): 1 a hit, 0 a miss, -1 unknown; reads
   iok, ix, iy only */
static int solid_point_sum(const struct pq *q, int prec, int notme_self)
{
    if (q->iok && q->ix >= 0 && q->iy >= 0 && (q->ix >> 4) < GRID_W && (q->iy >> 4) < GRID_H) {
        grid_flush();
        return ik_cells((q->iy >> 4) * GRID_W + (q->ix >> 4), 1, 1, notme_self * 2 + (prec != 0));
    }
    return -1;
}

/* collision_point_any(x, y, obj, prec, notme) != noone for static family obj (xf_of) at a whole point (x, y) (or the
   floors of a point, as xstatic_any reads them): xstatic_any's first answers on the ints - 0 when no entry of the
   family reaches the point's cell (xpoint_none, after the same xflush_run), 1 when the cell's hint is alive, of obj,
   not notme, has a cached whole box (BB_INT) holding the point and is not precise under prec (xhint_hit: point_hit's
   integer test); -1 otherwise (out of the grid, no hint, a hint without a cached whole box or not holding the
   point), and xstatic_any then repeats these tests, which change nothing more */
static int ik_xpt(int obj, int notme, int prec, int32_t x, int32_t y)
{
    int f = xf_of[obj], cx, cy, k;
    const struct pin *h;
    if (x < 0 || y < 0 || (cx = x >> 4) >= GRID_W || (cy = y >> 4) >= GRID_H) return -1;
    if (xdhead >= 0) xflush_run();
    if (xfar[f] == 0 && !xsat[f] && xcnt[f][cy][cx] == 0) return 0;
    k = xhint[cy][cx];
    if (k < 0 || k == notme) return -1;
    h = &PW.in[k];
    if (!h->alive || h->bbk != BB_INT || !obj_is(h->obj, obj)) return -1;
    if (!(x >= h->bl && x < h->br && y >= h->bt && y < h->bb) || (prec && precise(k))) return -1;
    return 1;
}
#endif

int (collision_point_any)(double px, double py, int obj, int prec, int notme_self)
{
#ifndef PCOL_EXACT
    struct pq q;
    /* a static family (liquids, ladders, spikes, webs): the index's miss as collision_point_p's, else whether any
       instance is hit (xpoint_any). collision_point_p returns the oldest instance hit and touches (pcol_touch) the
       ones before it in creation order: whether one is hit does not depend on the order, and the touches only update
       stale collision entries, which the grid build's searches do not depend on (as the oSolid summary below; with
       pcol_quiet 0 every instance is synced). The host builds compare every answer with collision_point_p */
    if (obj >= 0 && obj != OBJ_oSolid && xf_of[obj] >= 0 && !pcol_quiet()) {
        int r;
        int32_t a, b;
        if (fam_none(obj)) return 0;
        /* whole px, py: pq_init's ix, iy (dwhole), and the kernel's answer without the query struct */
        r = dwhole(px, &a) && dwhole(py, &b) ? ik_xpt(obj, notme_self, prec, a, b) : -1;
        if (r < 0) {
            pq_init(&q, px, py);
            r = xstatic_any(obj, notme_self, &q, prec);
        }
#ifdef PLAY_STATS
        if (r != (collision_point_p(px, py, obj, prec, notme_self) != NOONE)) {
            fprintf(stderr, "collision_point_any: static family %d answer %d differs (%.17g %.17g)\n", obj, r, px, py);
            abort();
        }
#endif
        return r;
    }
    if (obj == OBJ_oSolid && !gfar && !pcol_quiet()) {
        int r;
        int32_t a, b;
        if (fam_none(obj)) return 0;
        if (dwhole(px, &a) && dwhole(py, &b)) {               /* pq_init's ix, iy: solid_point_sum on them */
            r = -1;
            if (a >= 0 && b >= 0 && (a >> 4) < GRID_W && (b >> 4) < GRID_H) {
                grid_flush();
                r = ik_cells((b >> 4) * GRID_W + (a >> 4), 1, 1, notme_self * 2 + (prec != 0));
            }
        } else {
            pq_init(&q, px, py);
            r = solid_point_sum(&q, prec, notme_self);
        }
        if (r >= 0) {
#ifdef PLAY_STATS
            if (r != (collision_point_p(px, py, obj, prec, notme_self) != NOONE)) {
                fprintf(stderr, "collision_point_any: summary %d differs (%.17g %.17g)\n", r, px, py);
                abort();
            }
#endif
            return r;
        }
    }
#endif
    return collision_point_p(px, py, obj, prec, notme_self) != NOONE;
}

/* collision_rectangle(x + l, y + t, x + r, y + b, obj, 0, noone) != noone and collision_point(x + dx, y + dy, obj, 0,
   noone) != noone at instance i's x, y (|l|, ... <= 16): with x, y whole and |x|, |y| < 29900 the corners are whole
   doubles within dwhole's range, which rq_init / pq_init take as these ints (collision_rect_i's query; the point's
   cell for the static-family index), without the double sums; otherwise collision_rect_any / collision_point_any on
   the doubles */
static int xy_int_near(int i, int32_t *x, int32_t *y)
{
    return pin_xy_int(i, x, y) && *x > -29900 && *x < 29900 && *y > -29900 && *y < 29900;
}

static int rect_any_i(int32_t l, int32_t t, int32_t r, int32_t b, int prec, int notme_self);

int (collision_rect_any_at)(int i, int32_t l, int32_t t, int32_t r, int32_t b, int obj)
{
    int32_t x, y;
    if (xy_int_near(i, &x, &y)) {
        /* oSolid: collision_rect_any's whole-corner path (rect_any_i on the same ints; l <= r, t <= b as given) */
        if (obj == OBJ_oSolid && l <= r && t <= b) return rect_any_i(x + l, y + t, x + r, y + b, 0, NOONE);
        return collision_rect_i(x + l, y + t, x + r, y + b, obj, 0, NOONE) != NOONE;
    }
    return (collision_rect_any)(PTOD(PW.in[i].x) + l, PTOD(PW.in[i].y) + t, PTOD(PW.in[i].x) + r, PTOD(PW.in[i].y) + b,
                                obj, 0, NOONE);
}

/* collision_point_any_at(i, dx, dy, obj) (the play.h macro with it) when i's x, y are whole with |.| < 29900
   (xy_int_near) and px = x + dx, py = y + dy: its branches on the ints given, without reading the position again
   (check_water's tests of a water's neighbours: pw_filled_xy) */
static int point_at_xy(int obj, int32_t px, int32_t py)
{
    if (pw_noinst_point(obj)) return 0;
#ifndef PCOL_EXACT
    {
        struct pq q;
        int r;
        q.iok = 1; q.ix = px; q.iy = py; q.nodbl = 1;
        if (obj == OBJ_oSolid && !gfar && !pcol_quiet()) {
            r = fam_none(obj) ? 0 : solid_point_sum(&q, 0, NOONE);
            if (r < 0) r = collision_point_p(px, py, obj, 0, NOONE) != NOONE;
        } else if (obj >= 0 && xf_of[obj] >= 0 && !pcol_quiet()) {
            r = fam_none(obj) ? 0 : ik_xpt(obj, NOONE, 0, px, py);
            if (r < 0) r = xstatic_any(obj, NOONE, &q, 0);
        } else
            return (collision_point_any)(px, py, obj, 0, NOONE);
#ifdef PLAY_STATS
        if (r != (collision_point_p((double)px, (double)py, obj, 0, NOONE) != NOONE)) {
            fprintf(stderr, "point_at_xy: answer %d differs (%d %d %d)\n", r, obj, (int)px, (int)py);
            abort();
        }
#endif
        return r;
    }
#else
    return (collision_point_any)(px, py, obj, 0, NOONE);
#endif
}

/* collision_point_any_at(i, dx, dy, oSolid) || collision_point_any_at(i, dx, dy, obj) for i at whole x, y (|.| < 29900)
   and px = x + dx, py = y + dy (|dx|, |dy| <= 16) */
int pw_filled_xy(int obj, int32_t px, int32_t py)
{
#ifndef PCOL_EXACT
    /* point_at_xy's two calls in one when both take the grid paths (the point in the grid, gfar 0, pcol_quiet() 0,
       obj a static family but oSolid), with the same flushes and fallbacks in the same order:
       - oSolid: none alive -> 0; else grid_flush and ik_cells' one cell with notme NOONE, prec 0 (a block whose
         gfblk is not NOONE: 1; gother, or a second block: -1; else 0); -1 -> collision_point_p;
       - obj (only after a solid miss): none alive -> 0; else ik_xpt's tests inline (xflush_run, the index's miss, the
         hint holding the point); -1 -> xstatic_any on point_at_xy's query.
       oSolid's -1 goes back to point_at_xy for both (its grid_flush and ik_cells then change nothing) */
    int f = obj >= 0 && obj != OBJ_oSolid ? xf_of[obj] : -1, cx = px >> 4, cy = py >> 4;
    if (f >= 0 && px >= 0 && py >= 0 && cx < GRID_W && cy < GRID_H && !gfar && !pcol_quiet()) {
        int r, k, n;
        const struct pin *h;
        if (olive[OBJ_oSolid]) {
            grid_flush();
            n = gfull[cy][cx];
            r = n && gfblk[cy][cx] != NOONE ? 1 : gother[cy][cx] || n > 1 ? -1 : 0;
            if (r < 0) return point_at_xy(OBJ_oSolid, px, py) || point_at_xy(obj, px, py);
#ifdef PLAY_STATS
            if (r != (collision_point_p((double)px, (double)py, OBJ_oSolid, 0, NOONE) != NOONE)) {
                fprintf(stderr, "point_at_xy: answer %d differs (%d %d %d)\n", r, OBJ_oSolid, (int)px, (int)py);
                abort();
            }
#endif
            if (r) return 1;
        }
        if (!olive[obj]) return 0;
        if (xdhead >= 0) xflush_run();
        if (xfar[f] == 0 && !xsat[f] && xcnt[f][cy][cx] == 0)
            r = 0;
        else if ((k = xhint[cy][cx]) >= 0 && (h = &PW.in[k])->alive && h->bbk == BB_INT && obj_is(h->obj, obj) &&
                 px >= h->bl && px < h->br && py >= h->bt && py < h->bb)
            r = 1;
        else {
            struct pq q;
            q.iok = 1; q.ix = px; q.iy = py; q.nodbl = 1;
            r = xstatic_any(obj, NOONE, &q, 0);
        }
#ifdef PLAY_STATS
        if (r != (collision_point_p((double)px, (double)py, obj, 0, NOONE) != NOONE)) {
            fprintf(stderr, "point_at_xy: answer %d differs (%d %d %d)\n", r, obj, (int)px, (int)py);
            abort();
        }
#endif
        return r;
    }
#endif
    return point_at_xy(OBJ_oSolid, px, py) || point_at_xy(obj, px, py);
}

int (collision_point_any_at)(int i, int32_t dx, int32_t dy, int obj)
{
#ifndef PCOL_EXACT
    {
        int32_t x, y;
        struct pq q;
        int ok = 0;
        /* the query collision_point_any's pq_init makes of PTOD(x) + dx, PTOD(y) + dy, without the doubles: whole x, y
           (|.| < 29900) give whole sums, which pq_init takes as these ints (px, py the same values); at dx = dy = 0 the
           point is the position itself, a float: pq_init keeps a whole one, and rounds a fractional one to float,
           which leaves it unchanged, so px, py are x, y and ix, iy their floors (iok while |v| < 30000, dfloor_int's
           range) */
        if (obj >= 0 && xf_of[obj] >= 0 && !pcol_quiet()) {
            if (xy_int_near(i, &x, &y)) {
                q.iok = 1; q.ix = x + dx; q.iy = y + dy;
                q.nodbl = 1;                                  /* (px, py: the whole ix, iy, converted only if read) */
                ok = 1;
            }
            else if (dx == 0 && dy == 0) {
                q.px = PW.in[i].x; q.py = PW.in[i].y;
                q.nodbl = 0;
                q.iok = pfloor_int(PW.in[i].x, &q.ix) && pfloor_int(PW.in[i].y, &q.iy);
                ok = 1;
            }
        }
        /* oSolid at whole x, y: the summary on the same int query (collision_point_any's, without the doubles: the
           point PTOD(x) + dx is the whole double x + dx, which pq_init takes as these ints); not known: the search
           collision_point_any falls back to, at the same point */
        if (obj == OBJ_oSolid && !gfar && !pcol_quiet() && xy_int_near(i, &x, &y)) {
            int r;
            q.iok = 1; q.ix = x + dx; q.iy = y + dy;                 /* (solid_point_sum reads iok, ix, iy) */
            r = fam_none(obj) ? 0 : solid_point_sum(&q, 0, NOONE);
            if (r < 0) r = collision_point_p(q.ix, q.iy, obj, 0, NOONE) != NOONE;
#ifdef PLAY_STATS
            if (r != (collision_point_p(PTOD(PW.in[i].x) + dx, PTOD(PW.in[i].y) + dy, obj, 0, NOONE) != NOONE)) {
                fprintf(stderr, "collision_point_any_at: solid answer %d differs (%d %d)\n", r, q.ix, q.iy);
                abort();
            }
#endif
            return r;
        }
        /* oSolid at the position itself (dx = dy = 0) not whole or not near: collision_point_any's summary path on the
           point PTOD(x), PTOD(y) (its dwhole test, else pq_init, which keeps a float's value): the ints it reads,
           iok, ix, iy, are the floors pfloor_int gives (dfloor_int's range), so solid_point_sum answers as there;
           not known: the same search at the same point, as collision_point_any falls back to */
        if (obj == OBJ_oSolid && dx == 0 && dy == 0 && !gfar && !pcol_quiet()) {
            int r;
            if (fam_none(obj)) return 0;
            q.iok = pfloor_int(PW.in[i].x, &q.ix) && pfloor_int(PW.in[i].y, &q.iy);
            r = solid_point_sum(&q, 0, NOONE);
            if (r >= 0) {
#ifdef PLAY_STATS
                if (r != (collision_point_p(PTOD(PW.in[i].x) + dx, PTOD(PW.in[i].y) + dy, obj, 0, NOONE) != NOONE)) {
                    fprintf(stderr, "collision_point_any: summary %d differs (%.17g %.17g)\n", r,
                            PTOD(PW.in[i].x) + dx, PTOD(PW.in[i].y) + dy);
                    abort();
                }
#endif
                return r;
            }
            return collision_point_p(PTOD(PW.in[i].x) + dx, PTOD(PW.in[i].y) + dy, obj, 0, NOONE) != NOONE;
        }
        if (ok) {
            int r = fam_none(obj) ? 0 : q.iok ? ik_xpt(obj, NOONE, 0, q.ix, q.iy) : -1;
            if (r < 0) r = xstatic_any(obj, NOONE, &q, 0);
#ifdef PLAY_STATS
            {
                struct pq c;
                pq_init(&c, PTOD(PW.in[i].x) + dx, PTOD(PW.in[i].y) + dy);
                if (c.iok != q.iok || (c.iok && (c.ix != q.ix || c.iy != q.iy)) || c.px != (q.nodbl ? (double)q.ix : q.px) ||
                    c.py != (q.nodbl ? (double)q.iy : q.py)) {
                    fprintf(stderr, "collision_point_any_at: query differs (%d %d %d)\n", i, (int)dx, (int)dy);
                    abort();
                }
                if (r != (collision_point_p(PTOD(PW.in[i].x) + dx, PTOD(PW.in[i].y) + dy, obj, 0, NOONE) != NOONE)) {
                    fprintf(stderr, "collision_point_any_at: answer %d differs (%d %d %d)\n", r, obj, q.ix, q.iy);
                    abort();
                }
            }
#endif
            return r;
        }
    }
#endif
    /* the other objects (and the branches above off their grid paths) at whole x, y (|.| < 29900): the point
       PTOD(x) + dx, PTOD(y) + dy is whole, so collision_point_any is collision_point_p there (its static-family branch
       needs pcol_quiet() 0 and obj's index, its oSolid branch gfar 0 as well, which return above at a near whole
       x, y), and pq_init gives iok, these ints and their values (nodbl's meaning): point_q on them */
    {
        int32_t x, y;
        if (xy_int_near(i, &x, &y)) {
            struct pq q;
            PWST(point, 1);
            if (fam_none(obj)) return 0;
            q.iok = 1; q.ix = x + dx; q.iy = y + dy; q.nodbl = 1;
#ifdef PLAY_STATS
            {
                struct pq c;
                pq_init(&c, PTOD(PW.in[i].x) + dx, PTOD(PW.in[i].y) + dy);
                if (!c.iok || c.ix != q.ix || c.iy != q.iy || c.px != (double)q.ix || c.py != (double)q.iy) {
                    fprintf(stderr, "collision_point_any_at: int query differs (%d %d %d)\n", i, (int)dx, (int)dy);
                    abort();
                }
            }
#endif
            return point_q(&q, obj, 0, NOONE) != NOONE;
        }
    }
    return (collision_point_any)(PTOD(PW.in[i].x) + dx, PTOD(PW.in[i].y) + dy, obj, 0, NOONE);
}

#ifndef PCOL_EXACT
/* collision_point_any_at(i, 0, 0, obj) for a static family (xf_of[obj] >= 0) on its query q of i's position: its
   static-family branch (the macro's pw_noinst_point test first; pcol_quiet: the function itself) */
static int piece_static(int i, int obj, struct pq *q)
{
    int r;
    if (pw_noinst_point(obj)) return 0;
    if (xf_of[obj] < 0 || pcol_quiet()) return (collision_point_any_at)(i, 0, 0, obj);
    r = fam_none(obj) ? 0 : q->iok ? ik_xpt(obj, NOONE, 0, q->ix, q->iy) : -1;
    if (r < 0) {
        if (q->nodbl == 2) {                     /* pw_piece_tests' floats, made when first read */
            q->px = TOD(PW.in[i].x); q->py = TOD(PW.in[i].y);
            q->nodbl = 0;
        }
        r = xstatic_any(obj, NOONE, q, 0);
    }
    return r;
}

/* rubblepiece_step's point tests (pobj.c) at i's position, its query made once: 1 collision_point_any_at(i, 0, 0,
   oWaterSwim), 2 the same for oLava (asked only without 1), 4 for oSolid. collision_point_any_at's query of the
   position (dx = dy = 0) is q below: the ints of a whole near position (nodbl), else the floats and their floors.
   The liquids take its static-family branch on q; oSolid its oSolid branches, which read only iok, ix, iy (the
   whole branch's ints are the floors of the not-whole branch's, which also takes a whole position past 29900), and
   fall back to collision_point_p at PTOD(x) + 0, PTOD(y) + 0 (the whole branch's ints as doubles: +0 for -0) */
int pw_piece_tests(int i)
{
    const struct pin *p = &PW.in[i];
    struct pq q;
    int32_t x = p->ix, y = p->iy;
    int r = 0, s, whole;
    /* whole near by the shadows (PXY_UNK, PXY_NO are below -29900) without pin_xy_fill's decode of a changed y (the
       drips'); a whole position with a stale shadow takes the floats: the same query (its floors are its ints, px, py
       their values: nodbl's meaning), except at a +-0 coordinate (px -0.0 where nodbl reads +0): the decode then */
    if (x > -29900 && x < 29900 && y > -29900 && y < 29900) {
#ifdef PIN_SHADOW_CHECK
        pin_xy_check(p, 1, x, y);
#endif
        whole = 1;
    } else if (fzero(p->x) || fzero(p->y))
        whole = xy_int_near(i, &x, &y);
    else
        whole = 0;
    if (whole) {
        q.iok = 1; q.ix = x; q.iy = y;
        q.nodbl = 1;
    } else {
        q.nodbl = 2;              /* px, py: the floats (fwiden), set by piece_static before xstatic_any reads them */
        q.iok = pfloor_int(PW.in[i].x, &q.ix) && pfloor_int(PW.in[i].y, &q.iy);
    }
    if (piece_static(i, OBJ_oWaterSwim, &q)) r = 1;
    else if (piece_static(i, OBJ_oLava, &q)) r = 2;
    if (pw_noinst_point(OBJ_oSolid)) s = 0;
    else if (!gfar && !pcol_quiet()) {
        s = fam_none(OBJ_oSolid) ? 0 : solid_point_sum(&q, 0, NOONE);
        if (s < 0) {
            /* collision_point_p at PTOD(x) + 0, PTOD(y) + 0 (fam_none 0 here): pq_init makes q of that point - a whole
               near one: iok, the ints, their values (nodbl 1); otherwise the floats themselves ((float) of x + 0 is x
               but for -0, which gives +0) and their floors, which q's ints are (pfloor_int: dfloor_int's range) */
            PWST(point, 1);
            if (q.nodbl == 2) {
                q.px = fzero(PW.in[i].x) ? 0.0 : TOD(PW.in[i].x);
                q.py = fzero(PW.in[i].y) ? 0.0 : TOD(PW.in[i].y);
                q.nodbl = 0;
            }
#ifdef PLAY_STATS
            {
                struct pq c;
                pq_init(&c, PTOD(PW.in[i].x) + 0, PTOD(PW.in[i].y) + 0);
                if (c.iok != q.iok || (c.iok && (c.ix != q.ix || c.iy != q.iy)) ||
                    pcd(c.px) != pcd(q.nodbl ? (double)q.ix : q.px) || pcd(c.py) != pcd(q.nodbl ? (double)q.iy : q.py)) {
                    fprintf(stderr, "pw_piece_tests: query differs (%d)\n", i);
                    abort();
                }
            }
#endif
            s = point_q(&q, OBJ_oSolid, 0, NOONE) != NOONE;
        }
    } else
        s = (collision_point_any_at)(i, 0, 0, OBJ_oSolid);
#ifdef PLAY_STATS
    {
        double px = PTOD(PW.in[i].x) + 0, py = PTOD(PW.in[i].y) + 0;
        int w = collision_point_p(px, py, OBJ_oWaterSwim, 0, NOONE) != NOONE;
        int c = w ? 1 : (collision_point_p(px, py, OBJ_oLava, 0, NOONE) != NOONE) ? 2 : 0;
        if (c != r || s != (collision_point_p(px, py, OBJ_oSolid, 0, NOONE) != NOONE)) {
            fprintf(stderr, "pw_piece_tests: %d %d differ (instance %d)\n", r, s, i);
            abort();
        }
    }
#endif
    return r | (s ? 4 : 0);
}
#endif

/* ---- the idle fish's tests (pk_swamp.c piranha_idle): the common answers of three queries in a few lines, -1 where
   the caller must ask the general function. Each reads what the general one reads first and changes nothing it would
   not (grid_flush only where collision_point_any would run it) ---------------------------------------------- */

/* collision_point_any_at(i, dx, dy, obj) for a static family, at the query x, y it makes for whole x(i), y(i) (|.| <
   29900) and |dx|, |dy| <= 16: x(i) + dx, y(i) + dy. 0 when no entry of the family reaches the point's cell
   (xpoint_none), 1 when the cell's hint has a cached whole box holding the point (xhint_hit, prec 0); -1 otherwise
   (pending index updates, out of the grid, a hint without a whole box) */
int pw_static_xy(int obj, int32_t x, int32_t y)
{
#ifndef PCOL_EXACT
    int f, cx, cy, k;
    const struct pin *h;
    if (obj < 0 || (f = xf_of[obj]) < 0 || xdhead >= 0 || pcol_quiet()) return -1;
    if (fam_none(obj)) return 0;
    if (x < 0 || y < 0 || (cx = x >> 4) >= GRID_W || (cy = y >> 4) >= GRID_H) return -1;
    if (xfar[f] == 0 && !xsat[f] && xcnt[f][cy][cx] == 0) return 0;
    k = xhint[cy][cx];
    if (k < 0) return -1;
    h = &PW.in[k];
    if (!h->alive || h->bbk != BB_INT || !obj_is(h->obj, obj)) return -1;
    return x >= h->bl && x < h->br && y >= h->bt && y < h->bb ? 1 : -1;
#else
    (void)obj; (void)x; (void)y;
    return -1;
#endif
}

/* collision_point_any(x, y, oSolid, 0, noone) for whole x, y (|.| < 30000): the solid summary's answers (a block in
   the cell: 1; no block and no other entry reaching it: 0), -1 otherwise */
int pw_solid_pt(int32_t x, int32_t y)
{
#ifndef PCOL_EXACT
    struct pq q;
    if (gfar || pcol_quiet()) return -1;
    if (fam_none(OBJ_oSolid)) return 0;
    q.iok = 1; q.ix = x; q.iy = y;                             /* (solid_point_sum reads iok, ix, iy) */
    return solid_point_sum(&q, 0, NOONE);
#else
    (void)x; (void)y;
    return -1;
#endif
}

/* pin_setx(p, PI(x + d)) for i's whole x (|x| < 29900) and d = +-1. A box cached whole (BB_INT) before stays cached:
   bbkind_set's box at x + d is the old one moved by d (the sprite, the scales and the angle are unchanged, x + d is
   whole), and the setter's marks (pw_changed) do not read the box */
#define XSTEP_POS(v) fint15(v)
void pw_xstep(int i, int32_t x, int d)
{
    struct pin *p = &PW.in[i];
    int k = p->bbk == BB_INT;
#ifdef PLAY_STATS
    if (POS_NE(XSTEP_POS(x + d), PI(x + d))) { fprintf(stderr, "pw_xstep: fint15 %d differs\n", (int)(x + d)); abort(); }
#endif
    pin_setx(p, XSTEP_POS(x + d));
    if (k) {
        p->bl = (int16_t)(p->bl + d);
        p->br = (int16_t)(p->br + d);
        p->bbk = BB_INT;
#ifdef PLAY_STATS
        {
            int16_t b[4] = { p->bl, p->bt, p->br, p->bb };
            p->bbk = 0;
            if (bbkind_set(i) != BB_INT || b[0] != p->bl || b[1] != p->bt || b[2] != p->br || b[3] != p->bb) {
                fprintf(stderr, "pw_xstep: box differs (%d)\n", i);
                abort();
            }
        }
#endif
    }
}

/* some alive instance of one of the n families objs[] has its enemy record's swimming set */
static int fam_swims_walk(const int16_t *objs, int n)
{
    int a, j, k;
    for (a = 0; a < n; a++) {
        int obj = objs[a];
        if (olive[obj] == 0) continue;
        for (j = obj; j >= 0; j = ofam_next(obj, j)) {
            if (olive[j] == 0) continue;
            for (k = pw_ohead[j]; k >= 0; k = pw_inext[k])
                if (PEN(&PW.in[k])->swimming) return 1;
        }
    }
    return 0;
}

/* the walk's answer kept while PW.step, olive_gen and objs hold. Its callers (pk_swamp.c: piranha_step, piranha_idle,
   pswamp_piranha_run) call it only in oPiranha's Step dispatch, so within a step nothing but piranha Steps runs between two calls: they write no
   enemy's swimming (the writes: the enemies' Create events and their own Steps, pdamsel.c, pk_jungle.c, penemy.c,
   pk_swamp.c create), and every instance they create or destroy bumps olive_gen (olive_add), as a room start does
   (olists_reset). The host builds compare every kept answer with the walk */
static uint32_t fsw_step, fsw_gen;
static const int16_t *fsw_objs;
static int8_t fsw_val;
int pw_fam_swims(const int16_t *objs, int n)
{
    if (fsw_objs == objs && fsw_step == PW.step && fsw_gen == olive_gen + 1) {
#ifdef PLAY_STATS
        if (fsw_val != fam_swims_walk(objs, n)) { fprintf(stderr, "pw_fam_swims: kept answer differs\n"); abort(); }
#endif
        return fsw_val;
    }
    fsw_val = (int8_t)fam_swims_walk(objs, n);
    fsw_objs = objs;
    fsw_step = PW.step;
    fsw_gen = olive_gen + 1;
    return fsw_val;
}

/* a line query with whole-number ends: their bounding box, and whether the line is axis-aligned */
struct lq { int iok, axis; int32_t lx, ly, hx, hy; };

static int whole(double v, int32_t *o)
{
    if (dwhole(v, o)) return 1;
    if (!dfloor_int(v, o)) return 0;
    return (double)*o == v;
}

static void lq_init(struct lq *q, double x1, double y1, double x2, double y2)
{
    int32_t a, b, c, d;
    q->iok = whole(x1, &a) && whole(y1, &b) && whole(x2, &c) && whole(y2, &d);
    if (!q->iok) return;
    q->lx = a < c ? a : c; q->hx = a < c ? c : a;
    q->ly = b < d ? b : d; q->hy = b < d ? d : b;
    q->axis = a == c || b == d;
}

/* seg_box clips to the closed box [l, r - 1e-9] x [t, b - 1e-9]: with whole numbers, a segment whose bounding box
   misses [l, r - 1] x [t, b - 1] misses it, and an axis-aligned one that meets it hits it */
struct rq;
/* a query: the line's ends as doubles (x1 .. y2; with dbl 0 they are ix1 .. iy2 and converted when needed) */
struct qctx { int obj, notme, prec, self, hit; double x1, y1, x2, y2, dx, dy; struct lq lq; struct rq *rq;
              int32_t ix1, iy1, ix2, iy2; uint8_t dbl; };

/* CInstance::Collision_Line against the bounding box (non-compatibility mode, floats): outside the segment's own
   box; else the segment, its ends ordered in x, clipped to [left, right - 1e-5] and missing when both clipped ends
   are above top or both below bottom */
static int line_box_f(float x1, float y1, float x2, float y2, float l, float t, float r, float b, float *o)
{
    float xa, ya, xb, yb;
    if ((x1 < x2 ? x1 : x2) >= r || l > (x1 > x2 ? x1 : x2) || (y1 < y2 ? y1 : y2) >= b || t > (y1 > y2 ? y1 : y2))
        return 0;
    if (x1 > x2) { xa = x2; ya = y2; xb = x1; yb = y1; }
    else { xa = x1; ya = y1; xb = x2; yb = y2; }
    if (l > xa) {
        ya = ya + ((yb - ya) * (l - xa)) / (xb - xa);
        xa = l;
    }
    r = r + -1.0e-5f;
    if (xb > r) {
        yb = yb + ((yb - ya) * (r - xb)) / (xb - xa);
        xb = r;
    }
    if (t > ya && t > yb) return 0;
    if (ya > b && yb > b) return 0;
    if (o) { o[0] = xa; o[1] = ya; o[2] = xb; o[3] = yb; }
    return 1;
}

/* precise_line's walk along an axis-parallel line (solid_hline_any / solid_vline_any against a rotated or precise
   solid: the idol's boulder). slope is +-0 there and v - lo >= +0 (v starts at max(side, lo) and grows), so the
   interpolated coordinate's (v - lo) * slope + lo, and its difference d from the instance's position, are the
   same float on every pixel: the per-pixel products with d (c0, c1) are computed once by the caller, the rest
   keeps its operands and order (no contraction: -ffp-contract=off). x-major: tx from (cs * dx + c1), ty from
   (c0 + dx * ns), c0 = cs * dy, c1 = sn * dy; y-major: tx from (c0 + sn * dy), ty from (cs * dy + c1), c0 = cs * dx,
   c1 = dx * ns. A division by a scale of 1.0f (its bits) is left out (q / 1.0f is q), and (float)(int32_t)
   dfloor(q) is q's floor on the bits (pl_floor) compared as an int with the mask box's whole floats; pc_bit's
   (int)(t - ml) is then the ints' difference. -1: outside the integer range (|q| >= 2^15, a mask box that is not
   whole): the caller's walk. PLAY_STATS builds run precise_line_ref beside every answer */
static int pl_one(float f) { union { float f; uint32_t u; } v; v.f = f; return v.u == 0x3f800000u; }
static int pl_zero(float f) { union { float f; uint32_t u; } v; v.f = f; return (v.u << 1) == 0; }

static int pl_floor(float q, int32_t *o)                  /* floor(q) for |q| < 2^15 (fwhole's product) */
{
    union { float f; uint32_t u; } v;
    uint32_t e;
    uint64_t pr;
    int32_t ip;
    v.f = q;
    if ((v.u & 0x7fffffffu) == 0) { *o = 0; return 1; }
    e = (v.u >> 23) & 0xffu;
    if (e < 127) { *o = (v.u >> 31) ? -1 : 0; return 1; }
    if (e > 141) return 0;
    pr = (uint64_t)((v.u & 0x7fffffu) | 0x800000u) * fwhole_mul[e - 127];
    ip = (int32_t)(pr >> 32);
    *o = (v.u >> 31) ? ((uint32_t)pr ? -ip - 1 : -ip) : ip;
    return 1;
}

static int pl_axis(const struct pcinst *A, float v, float end, int xmajor, float c0, float c1, float sn, float cs,
                   float ns)
{
    int32_t ml, mt, mr, mb, tx, ty;
    int xs1 = pl_one(A->xs), ys1 = pl_one(A->ys);
    static const uint8_t bit[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
    if (!fwhole(A->ml, &ml) || !fwhole(A->mt, &mt) || !fwhole(A->mr, &mr) || !fwhole(A->mb, &mb)) return -1;
    for (; !(end < v); v = v + 1.0f) {
        float qx, qy;
        if (xmajor) {
            float dx = v - A->x;
            qx = cs * dx + c1;
            qy = c0 + dx * ns;
        } else {
            float dy = v - A->y;
            qx = c0 + sn * dy;
            qy = cs * dy + c1;
        }
        if (!xs1) qx = qx / A->xs;
        qx = qx + A->xo;
        if (!pl_floor(qx, &tx)) return -1;
        if (ml > tx || tx > mr) continue;
        if (!ys1) qy = qy / A->ys;
        qy = qy + A->yo;
        if (!pl_floor(qy, &ty)) return -1;
        if (mt > ty || ty > mb) continue;
        {
            int cx = tx - ml, cy = ty - mt;
            if (A->mask[cy * A->bpr + (cx >> 3)] & bit[cx & 7]) return 1;
        }
    }
    return 0;
}

#ifdef PLAY_STATS
/* CSprite::PreciseCollisionLine: the clipped segment (o: x1 y1 x2 y2) a pixel at a time along its longer axis
   (whole steps from the bbox side or the end, the other coordinate interpolated), each point rotated back and
   divided by the scale, floored, inside the mask box, the bit; a single point is PreciseCollisionPoint */
static int precise_line_ref(int k, const float *o)
{
    struct pcinst A;
    float sn, cs, a, x1 = o[0], y1 = o[1], x2 = o[2], y2 = o[3], lo_x, lo_y, hi_x, hi_y, slope, v, end, ns;
    double l, t, r, b;
    if (!pcinst_of(k, 0, 0, &A)) return 0;
    if (!A.mask) return 1;
    if (x1 == x2 && y1 == y2) return precise_point(k, x1, y1);
    pin_bbox(k, &l, &t, &r, &b);
    a = A.ang * -3.14159274101257324f;
    a = a / 180.0f;
    pcol_sincosf(a, &sn, &cs);
    ns = -sn;
    if (!((x2 - x1 < 0 ? x1 - x2 : x2 - x1) >= (y2 - y1 < 0 ? y1 - y2 : y2 - y1))) {
        if (y1 > y2) { hi_x = x1; hi_y = y1; lo_x = x2; lo_y = y2; }
        else { hi_x = x2; hi_y = y2; lo_x = x1; lo_y = y1; }
        v = (float)t > lo_y ? (float)t : lo_y;
        end = (float)b < hi_y ? (float)b : hi_y;
        if (end < v) return 0;
        slope = (hi_x - lo_x) / (hi_y - lo_y);
        for (; !(end < v); v = v + 1.0f) {
            float dx = ((v - lo_y) * slope + lo_x) - A.x, dy = v - A.y, tx, ty;
            tx = (float)(int32_t)dfloor((cs * dx + sn * dy) / A.xs + A.xo);
            if (A.ml > tx || tx > A.mr) continue;
            ty = (float)(int32_t)dfloor((cs * dy + dx * ns) / A.ys + A.yo);
            if (A.mt > ty || ty > A.mb) continue;
            if (pc_bit(&A, tx, ty)) return 1;
        }
        return 0;
    }
    if (x1 > x2) { hi_x = x1; hi_y = y1; lo_x = x2; lo_y = y2; }
    else { hi_x = x2; hi_y = y2; lo_x = x1; lo_y = y1; }
    v = (float)l > lo_x ? (float)l : lo_x;
    end = (float)r < hi_x ? (float)r : hi_x;
    if (!(end >= v)) return 0;
    slope = (hi_y - lo_y) / (hi_x - lo_x);
    for (; !(end < v); v = v + 1.0f) {
        float dx = v - A.x, dy = ((v - lo_x) * slope + lo_y) - A.y, tx, ty;
        tx = (float)(int32_t)dfloor((cs * dx + sn * dy) / A.xs + A.xo);
        if (A.ml > tx || tx > A.mr) continue;
        ty = (float)(int32_t)dfloor((cs * dy + dx * ns) / A.ys + A.yo);
        if (A.mt > ty || ty > A.mb) continue;
        if (pc_bit(&A, tx, ty)) return 1;
    }
    return 0;
}
#define PL_CHECK(r) do { if ((r) != precise_line_ref(k, o)) { fprintf(stderr, "precise_line: pl_axis %d differs (%d)\n", (r), k); abort(); } } while (0)
#else
#define PL_CHECK(r) ((void)0)
#endif

/* CSprite::PreciseCollisionLine: the clipped segment (o: x1 y1 x2 y2) a pixel at a time along its longer axis
   (whole steps from the bbox side or the end, the other coordinate interpolated), each point rotated back and
   divided by the scale, floored, inside the mask box, the bit; a single point is PreciseCollisionPoint */
static int precise_line(int k, const float *o)
{
    struct pcinst A;
    float sn, cs, a, x1 = o[0], y1 = o[1], x2 = o[2], y2 = o[3], lo_x, lo_y, hi_x, hi_y, slope, v, end, ns;
    double l, t, r, b;
    if (!pcinst_of(k, 0, 0, &A)) return 0;
    if (!A.mask) return 1;
    if (x1 == x2 && y1 == y2) return precise_point(k, x1, y1);
    pin_bbox(k, &l, &t, &r, &b);
    a = A.ang * -3.14159274101257324f;
    a = a / 180.0f;
    pcol_sincosf(a, &sn, &cs);
    ns = -sn;
    if (!((x2 - x1 < 0 ? x1 - x2 : x2 - x1) >= (y2 - y1 < 0 ? y1 - y2 : y2 - y1))) {
        if (y1 > y2) { hi_x = x1; hi_y = y1; lo_x = x2; lo_y = y2; }
        else { hi_x = x2; hi_y = y2; lo_x = x1; lo_y = y1; }
        v = (float)t > lo_y ? (float)t : lo_y;
        end = (float)b < hi_y ? (float)b : hi_y;
        if (end < v) return 0;
        slope = (hi_x - lo_x) / (hi_y - lo_y);
        if (pl_zero(slope)) {                 /* a vertical line: dx the same every pixel (pl_axis) */
            float dx = ((v - lo_y) * slope + lo_x) - A.x;
            int r = pl_axis(&A, v, end, 0, cs * dx, dx * ns, sn, cs, ns);
            if (r >= 0) { PL_CHECK(r); return r; }
        }
        for (; !(end < v); v = v + 1.0f) {
            float dx = ((v - lo_y) * slope + lo_x) - A.x, dy = v - A.y, tx, ty;
            tx = (float)(int32_t)dfloor((cs * dx + sn * dy) / A.xs + A.xo);
            if (A.ml > tx || tx > A.mr) continue;
            ty = (float)(int32_t)dfloor((cs * dy + dx * ns) / A.ys + A.yo);
            if (A.mt > ty || ty > A.mb) continue;
            if (pc_bit(&A, tx, ty)) return 1;
        }
        return 0;
    }
    if (x1 > x2) { hi_x = x1; hi_y = y1; lo_x = x2; lo_y = y2; }
    else { hi_x = x2; hi_y = y2; lo_x = x1; lo_y = y1; }
    v = (float)l > lo_x ? (float)l : lo_x;
    end = (float)r < hi_x ? (float)r : hi_x;
    if (!(end >= v)) return 0;
    slope = (hi_y - lo_y) / (hi_x - lo_x);
    if (pl_zero(slope)) {                     /* a horizontal line: dy the same every pixel (pl_axis) */
        float dy = ((v - lo_x) * slope + lo_y) - A.y;
        int r = pl_axis(&A, v, end, 1, cs * dy, sn * dy, sn, cs, ns);
        if (r >= 0) { PL_CHECK(r); return r; }
    }
    for (; !(end < v); v = v + 1.0f) {
        float dx = v - A.x, dy = ((v - lo_x) * slope + lo_y) - A.y, tx, ty;
        tx = (float)(int32_t)dfloor((cs * dx + sn * dy) / A.xs + A.xo);
        if (A.ml > tx || tx > A.mr) continue;
        ty = (float)(int32_t)dfloor((cs * dy + dx * ns) / A.ys + A.yo);
        if (A.mt > ty || ty > A.mb) continue;
        if (pc_bit(&A, tx, ty)) return 1;
    }
    return 0;
}

static int line_hit_vi(int k, int32_t X, int32_t lo, int32_t hi);
static int line_hit_hi(int k, int32_t Y, int32_t lo, int32_t hi);
static int line_hit_f(int k, struct qctx *c);

static int line_hit(int k, struct qctx *c)
{
    const struct lq *q = &c->lq;
    int prec = c->prec;
    int32_t ib[4];
    if (q->iok && pin_ibox(k, ib)) {
        if (q->hx < ib[0] || q->lx >= ib[2] || q->hy < ib[1] || q->ly >= ib[3])
            return 0;
        if (q->axis && (!prec || !precise(k)))
            return 1;
        if (q->axis) {                            /* an axis-aligned line against a precise entry: line_hit_vi */
            int r = q->lx == q->hx ? line_hit_vi(k, q->lx, q->ly, q->hy) : line_hit_hi(k, q->ly, q->lx, q->hx);
            if (r >= 0) {
#ifdef PLAY_STATS
                if (r != line_hit_f(k, c)) {
                    fprintf(stderr, "line_hit_vi: %d differs (%d: %d %d %d)\n", r, k, (int)q->lx, (int)q->ly, (int)q->hy);
                    abort();
                }
#endif
                return r;
            }
        }
    }
    return line_hit_f(k, c);
}

/* line_hit past its integer tests: the doubles and floats of CInstance::Collision_Line */
static int line_hit_f(int k, struct qctx *c)
{
    double l, t, r, b, x1, y1, x2, y2;
    int prec = c->prec;
    if (!pin_bbox(k, &l, &t, &r, &b))
        return 0;
    if (!c->dbl) {
        c->x1 = c->ix1; c->y1 = c->iy1; c->x2 = c->ix2; c->y2 = c->iy2;
        c->dbl = 1;
    }
    x1 = c->x1; y1 = c->y1; x2 = c->x2; y2 = c->y2;
    if (!prec || !precise(k))
        return line_box_f((float)x1, (float)y1, (float)x2, (float)y2, (float)l, (float)t, (float)r, (float)b, 0);
    {
        float o[4];
        if (!line_box_f((float)x1, (float)y1, (float)x2, (float)y2, (float)l, (float)t, (float)r, (float)b, o))
            return 0;
        return precise_line(k, o);
    }
}

/* collision_line / collision_rectangle / instance_place: ShouldUseFastCollision; with the tree (pcol_query 1),
   the first hit in the tree's search order (the search callbacks return false at the first hit); otherwise the
   object's instances in creation order */

static int line_cb(int k, void *v)
{
    struct qctx *q = (struct qctx *)v;
    if (k >= PIN_MAX || !match(k, q->obj, q->notme) || !line_hit(k, q)) return 1;
    q->hit = k;
    return 0;
}

static void qrect(double x1, double y1, double x2, double y2, float *r)
{
    r[0] = (float)(x1 < x2 ? x1 : x2) - 1.0f;
    r[1] = (float)(y1 < y2 ? y1 : y2) - 1.0f;
    r[2] = (float)(x1 < x2 ? x2 : x1) + 1.0f;
    r[3] = (float)(y1 < y2 ? y2 : y1) + 1.0f;
}

/* the search of collision_line (c: the query, q: pcol_query's answer, r: the tree search rectangle; NULL: the
   whole-number one from c->lq) */
static int line_run(struct qctx *c, int q, const float *r)
{
    int k;
    if (q == 1) {
        c->hit = NOONE;
        if (r) pcol_search(r[0], r[1], r[2], r[3], line_cb, c);
        else pcol_search_i(c->lq.lx - 1, c->lq.ly - 1, c->lq.hx + 1, c->lq.hy + 1, line_cb, c);
        return c->hit;
    }
    {
        struct fam it;
        fam_begin(&it, c->obj);
        while ((k = fam_get(&it)) != NOONE) {
            if (k == c->notme) continue;
            pcol_touch(k);
            if (line_hit(k, c))
                return k;
        }
    }
    return NOONE;
}

int (collision_line_p)(double x1, double y1, double x2, double y2, int obj, int prec, int notme_self)
{
    int q = pcol_query(obj);
    struct qctx c;
    float r[4];
    PWST(line, 1);
    if (q < 0) return NOONE;
    lq_init(&c.lq, x1, y1, x2, y2);
    c.obj = obj; c.notme = notme_self; c.prec = prec;
    c.x1 = x1; c.y1 = y1; c.x2 = x2; c.y2 = y2; c.dbl = 1;
    if (c.lq.iok) return line_run(&c, q, 0);   /* whole ends: qrect's are the ints +-1 (pcol_search_i) */
    if (q == 1) qrect(x1, y1, x2, y2, r);
    return line_run(&c, q, r);
}

/* collision_line with whole-number ends (|v| < 30000): the same without the double conversions */
int collision_line_i(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int obj, int prec, int notme_self)
{
    int q = pcol_query(obj);
    struct qctx c;
    PWST(line, 1);
    if (q < 0) return NOONE;
    c.lq.iok = 1;
    c.lq.lx = x1 < x2 ? x1 : x2; c.lq.hx = x1 < x2 ? x2 : x1;
    c.lq.ly = y1 < y2 ? y1 : y2; c.lq.hy = y1 < y2 ? y2 : y1;
    c.lq.axis = x1 == x2 || y1 == y2;
    c.obj = obj; c.notme = notme_self; c.prec = prec;
    c.ix1 = x1; c.iy1 = y1; c.ix2 = x2; c.iy2 = y2; c.dbl = 0;
    return line_run(&c, q, 0);                  /* qrect: (float)min - 1.0f is these exactly */
}

/* the grid scan (whether some instance of obj is hit, as collision_line_i(...) != NOONE: the oSolid family's grid
   holds its alive instances with a sprite, those without one never hit): a box of [l, r] x [t, b] in cell (cx, cy) reaches cell cx + gmaxw at most; line_hit needs l <= hx
   and r > lx (and in y), so the hits are in the cells (lx >> 4) - gmaxw .. hx >> 4 (clamped as the cells are) */
static int line_scan(struct qctx *c, int obj, int notme_self)
{
    int x0, xe, y0, ye, x, y, k;
    x0 = clampi((c->lq.lx >> 4) - gmaxw, 0, GRID_W - 1);
    xe = clampi(c->lq.hx >> 4, 0, GRID_W - 1);
    y0 = clampi((c->lq.ly >> 4) - gmaxh, 0, GRID_H - 1);
    ye = clampi(c->lq.hy >> 4, 0, GRID_H - 1);
    /* the line's own cells first (a hit there ends the scan); every grid instance is alive (unlinked when it dies) */
    for (y = ye; y >= y0; y--)
        for (x = xe; x >= x0; x--)
            for (k = ghead[y][x]; k >= 0; k = gnext[k]) {
                PWST(visit, 1);
                if (k != notme_self && line_hit(k, c) && (obj == OBJ_oSolid || obj_is(PW.in[k].obj, obj))) return 1;
            }
    return 0;
}

/* the paths of ik_line that need the whole query context (kept out of line: the summary's answer, the
   common case, then builds no struct qctx on the stack) */
static void any_ctx(struct qctx *c, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int obj, int prec, int notme_self)
{
    c->lq.iok = 1;
    c->lq.lx = x1 < x2 ? x1 : x2; c->lq.hx = x1 < x2 ? x2 : x1;
    c->lq.ly = y1 < y2 ? y1 : y2; c->lq.hy = y1 < y2 ? y2 : y1;
    c->lq.axis = x1 == x2 || y1 == y2;
    c->obj = obj; c->notme = notme_self; c->prec = prec;
    c->ix1 = x1; c->iy1 = y1; c->ix2 = x2; c->iy2 = y2; c->dbl = 0;
}

static __attribute__((noinline)) int any_run(int q, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int obj, int prec,
                                             int notme_self)
{
    struct qctx c;
    any_ctx(&c, x1, y1, x2, y2, obj, prec, notme_self);
    return line_run(&c, q, 0) != NOONE;
}

static __attribute__((noinline)) int any_scan(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int obj, int prec,
                                              int notme_self)
{
    struct qctx c;
    any_ctx(&c, x1, y1, x2, y2, obj, prec, notme_self);
    return line_scan(&c, obj, notme_self);
}

/* collision_line(a, lo, a, hi, oSolid, 1, notme) != noone (nm = notme * 2: solid_vline_any) or collision_line(lo, a,
   hi, a, ...) (nm = notme * 2 + 1: solid_hline_any), four register arguments: pcol_query (the flush), then the summary
   (ik_cells inside the grid, ik_sum else), else the scan; the tree's other answers take the search (any_run). The
   ends keep the callers' order for any_run / any_scan */
static __attribute__((noinline)) int ik_line(int32_t a, int32_t lo, int32_t hi, int nm)
{
    int q = pcol_query(OBJ_oSolid), notme = nm >> 1, r;
    int32_t l = lo < hi ? lo : hi, h = lo < hi ? hi : lo, x1, y1, x2, y2;
    PWST(line, 1);
    if (nm & 1) { x1 = lo; y1 = a; x2 = hi; y2 = a; }
    else { x1 = a; y1 = lo; x2 = a; y2 = hi; }
    if (q < 0) return 0;
    if (q != 1) return any_run(q, x1, y1, x2, y2, OBJ_oSolid, 1, notme);
    grid_flush();
    if (gfar) return any_run(q, x1, y1, x2, y2, OBJ_oSolid, 1, notme);
    if (a < 0 || l < 0 || (nm & 1 ? a >= GRID_H * 16 || h >= GRID_W * 16 : a >= GRID_W * 16 || h >= GRID_H * 16))
        r = ik_sum(x1 < x2 ? x1 : x2, y1 < y2 ? y1 : y2, (x1 < x2 ? x2 : x1) + 1, (y1 < y2 ? y2 : y1) + 1,
                   IK_NMF(notme, 1, 1));                  /* (partly outside the grid: the clamped cells) */
    else if (nm & 1)
        r = ik_cells((a >> 4) * GRID_W + (l >> 4), (h >> 4) - (l >> 4) + 1, 1, notme * 2 + 1);
    else
        r = ik_cells((l >> 4) * GRID_W + (a >> 4), (h >> 4) - (l >> 4) + 1, GRID_W, notme * 2 + 1);
#ifdef PLAY_STATS
    if (r >= 0 && r != any_scan(x1, y1, x2, y2, OBJ_oSolid, 1, notme)) {   /* the host builds check every summary answer */
        fprintf(stderr, "ik_line %d differs from the scan: %d %d %d %d notme %d\n", r, (int)x1, (int)y1, (int)x2, (int)y2,
                notme);
        abort();
    }
#endif
    if (r >= 0) return r;
    return any_scan(x1, y1, x2, y2, OBJ_oSolid, 1, notme);
}

/* collision_rectangle(l, t, r, b, oSolid, 1, notme) != noone for whole l <= r, t <= b (isCollisionSolid): the
   summary of the cells the rectangle covers (rect_hit on whole boxes: max(l, bl) < min(r, br) and in y) - a block
   that meets it and is not precise is a hit; no block meeting it and no other oSolid-family entry reaching those
   cells (gother: a superset of the cells an entry's box meets) is a miss; otherwise collision_rect_i's search. The
   host builds compare every summary answer with the search */
static int rect_any_i(int32_t l, int32_t t, int32_t r, int32_t b, int prec, int notme_self)
{
    int q = pcol_query(OBJ_oSolid), sure = 1, x, y, k;
    int x0, xe, y0, ye;
    PWST(rect, 1);
    if (q < 0) return 0;
    if (q != 1 || l > r || t > b) return collision_rect_i(l, t, r, b, OBJ_oSolid, prec, notme_self) != NOONE;
    grid_flush();
    if (gfar) return collision_rect_i(l, t, r, b, OBJ_oSolid, prec, notme_self) != NOONE;
    x0 = clampi(l >> 4, 0, GRID_W - 1); xe = clampi(r >> 4, 0, GRID_W - 1);
    y0 = clampi(t >> 4, 0, GRID_H - 1); ye = clampi(b >> 4, 0, GRID_H - 1);
    for (y = y0; y <= ye; y++)
        for (x = x0; x <= xe; x++) {
            int n = gfull[y][x];
            int32_t cl = x * 16, ct = y * 16;
            if (gother[y][x]) sure = 0;
            if (n == 0) continue;
            k = gfblk[y][x];
            if (k == notme_self || !((l > cl ? l : cl) < (r < cl + 16 ? r : cl + 16)) ||
                !((t > ct ? t : ct) < (b < ct + 16 ? b : ct + 16))) {
                if (n > 1) sure = 0;                      /* k is a miss; another block may not be */
                continue;
            }
            if (!prec || !precise(k)) {
#ifdef PLAY_STATS
                if (collision_rect_i(l, t, r, b, OBJ_oSolid, prec, notme_self) == NOONE) {
                    fprintf(stderr, "solid_rect_any: summary hit, search none (%d %d %d %d)\n", (int)l, (int)t, (int)r, (int)b);
                    abort();
                }
#endif
                return 1;
            }
            sure = 0;
        }
#ifdef PLAY_STATS
    if (sure && collision_rect_i(l, t, r, b, OBJ_oSolid, prec, notme_self) != NOONE) {
        fprintf(stderr, "solid_rect_any: summary miss, search hit (%d %d %d %d)\n", (int)l, (int)t, (int)r, (int)b);
        abort();
    }
#endif
    if (sure) return 0;
    return collision_rect_i(l, t, r, b, OBJ_oSolid, prec, notme_self) != NOONE;
}

int solid_rect_any(int32_t l, int32_t t, int32_t r, int32_t b, int notme_self)
{
    return rect_any_i(l, t, r, b, 1, notme_self);
}

#ifndef PCOL_EXACT
/* no entry of liquid family f reaches the cells of [l, r] x [t, b] (whole, l <= r, t <= b) */
static int xrect_none(int f, int32_t l, int32_t t, int32_t r, int32_t b)
{
    int x, y, x0 = clampi(l >> 4, 0, GRID_W - 1), xe = clampi(r >> 4, 0, GRID_W - 1);
    int y0 = clampi(t >> 4, 0, GRID_H - 1), ye = clampi(b >> 4, 0, GRID_H - 1);
    if (xdhead >= 0) xflush_run();
    if (xfar[f] || xsat[f]) return 0;
    for (y = y0; y <= ye; y++)
        for (x = x0; x <= xe; x++)
            if (xcnt[f][y][x]) return 0;
    return 1;
}
#endif

/* collision_rectangle(x1, y1, x2, y2, obj, prec, notme) != noone: whole corners of an oSolid query by rect_any_i (the
   corners rq_init would take as ints: dwhole), else collision_rect_p */
int (collision_rect_any)(double x1, double y1, double x2, double y2, int obj, int prec, int notme_self)
{
    int32_t a, b, c, d;
    if (obj == OBJ_oSolid && dwhole(x1, &a) && dwhole(y1, &b) && dwhole(x2, &c) && dwhole(y2, &d))
        return rect_any_i(a < c ? a : c, b < d ? b : d, a < c ? c : a, b < d ? d : b, prec, notme_self);
    return collision_rect_p(x1, y1, x2, y2, obj, prec, notme_self) != NOONE;
}

/* isCollisionLeft / Right / Top / Bottom (i, d) and getIdCollisionLeft / Right's line test (pscript.c anyCollision*)
   on whole x, y (the shadows: pin_xy_int_p) and the setCollisionBounds offsets: calculateCollisionBounds' sides
   lb = x + lbo, tb = y + tbo, rb = x + rbo, bb = y + bbo (whole: the rounding is the identity, as pscript.c ibounds
   took them), the scripts' line from them, and ik_line. -1 (nothing done) when x or y is not whole: the scripts'
   doubles then. side: bit 0 the right / bottom edge, bit 1 a horizontal line, bit 2 the line starts 5 px below
   the top (IK_* in play.h) */
__attribute__((noinline)) int ik_side(int i, int side, int d)
{
    const struct pin *p = &PW.in[i];
    const struct pin_ext *e;
    int32_t x, y, a;
    if (!pin_xy_int_p(p, &x, &y)) return -1;
    e = PE(p);
    if (side & 2) {                               /* collision_line(lb, a, rb - 1, a): Top a = tb - d, Bottom bb + d - 1 */
        a = side & 1 ? y + e->bbo + d - 1 : y + e->tbo - d;
        return ik_line(a, x + e->lbo, x + e->rbo - 1, i * 2 + 1);
    }
    a = side & 1 ? x + e->rbo + d - 1 : x + e->lbo - d;   /* collision_line(a, tb (+ 5), a, bb - 1): Left lb - d, Right rb + d - 1 */
    return ik_line(a, y + e->tbo + (side & 4 ? 5 : 0), y + e->bbo - 1, i * 2);
}

/* collision_line(x, y1, x, y2, oSolid, 1, notme) != noone and collision_line(x1, y, x2, y, ...): isCollisionLeft /
   Right / Top / Bottom with whole-number bounds (pscript.c); obj and prec constant, four arguments in registers */
int solid_vline_any(int32_t x, int32_t y1, int32_t y2, int notme_self)
{
    return ik_line(x, y1, y2, notme_self * 2);
}

/* solid_vline_any(x, y1, y2, notme_self)'s answer as line_any finds it on the grid (pcol_query 1, gfar 0), without
   pcol_query's flush (pk_swamp.c's idle piranha batch makes it, pcol_query, at the Step's place in its phase M; the
   grid build's searches do not depend on when entries are flushed): -1 on line_any's other paths */
int pw_solid_vline_q(int32_t x, int32_t y1, int32_t y2, int notme_self)
{
    int q = pcol_query_kind(OBJ_oSolid), r;
    if (q < 0) return 0;
    if (q != 1) return -1;
    grid_flush();
    if (gfar) return -1;
    {
        int32_t l = y1 < y2 ? y1 : y2, h = y1 < y2 ? y2 : y1;
        if (x < 0 || l < 0 || x >= GRID_W * 16 || h >= GRID_H * 16)
            r = ik_sum(x, l, x + 1, h + 1, IK_NMF(notme_self, 1, 1));   /* (line_summary's, clamped) */
        else
            r = ik_cells((l >> 4) * GRID_W + (x >> 4), (h >> 4) - (l >> 4) + 1, GRID_W, notme_self * 2 + 1);
    }
    if (r >= 0) return r;
    return any_scan(x, y1, x, y2, OBJ_oSolid, 1, notme_self);
}

int solid_hline_any(int32_t y, int32_t x1, int32_t x2, int notme_self)
{
    return ik_line(y, x1, x2, notme_self * 2 + 1);
}

/* a rectangle query: its sides rounded (floor(v + 0.5)) once */
/* f*: the corners as the runner's floats (CInstance::Collision_Rectangle takes floats) */
/* with whole-number corners (fok 0) the float corners are the ints', made when first needed (rq_floats) */
struct rq { double lx, hx, ly, hy; int32_t ilx, ihx, ily, ihy; int iok; float flx, fhx, fly, fhy; int fok; };

static void rq_floats(struct rq *q)
{
    q->flx = (float)q->ilx; q->fhx = (float)q->ihx; q->fly = (float)q->ily; q->fhy = (float)q->ihy;
    q->fok = 1;
}

static void rq_init(struct rq *q, double x1, double y1, double x2, double y2)
{
    int32_t a, b, c, d;
    if (dwhole(x1, &a) && dwhole(y1, &b) && dwhole(x2, &c) && dwhole(y2, &d)) {
        /* floor(v + 0.5) of a whole v is v; (float)v is v */
        q->ilx = a < c ? a : c; q->ihx = a < c ? c : a;
        q->ily = b < d ? b : d; q->ihy = b < d ? d : b;
        q->iok = 1;
        q->fok = 0;
        return;
    }
    q->fok = 1;
    {
    double lx = x1 < x2 ? x1 : x2, hx = x1 < x2 ? x2 : x1, ly = y1 < y2 ? y1 : y2, hy = y1 < y2 ? y2 : y1;
    float f1 = (float)x1, f2 = (float)x2, g1 = (float)y1, g2 = (float)y2;
    q->flx = f1 < f2 ? f1 : f2; q->fhx = f1 < f2 ? f2 : f1;
    q->fly = g1 < g2 ? g1 : g2; q->fhy = g1 < g2 ? g2 : g1;
    q->iok = dfloor_int(lx + 0.5, &q->ilx) && dfloor_int(hx + 0.5, &q->ihx) && dfloor_int(ly + 0.5, &q->ily) &&
             dfloor_int(hy + 0.5, &q->ihy);
    if (q->iok) {
        q->lx = q->ilx; q->hx = q->ihx; q->ly = q->ily; q->hy = q->ihy;
    } else {
        q->lx = dfloor(lx + 0.5);
        q->hx = dfloor(hx + 0.5);
        q->ly = dfloor(ly + 0.5);
        q->hy = dfloor(hy + 0.5);
    }
    }
}

static int precise_rect(int k, float ql, float qt, float qr, float qb)
{
    struct pcinst A;
    float x0, y0, x1, y1, xc, yc, ixs, iys, arA, abA;
    if (!pcinst_of(k, 0, 0, &A)) return 0;
    if (!A.mask) return 1;
    x0 = (float)((int)((A.bl > ql ? A.bl : ql) + 32768.0f) - 32768) + 0.5f;
    y0 = (float)((int)((A.bt > qt ? A.bt : qt) + 32768.0f) - 32768) + 0.5f;
    x1 = (float)(32768 - (int)(32768.0f - (A.br < qr ? A.br : qr)));
    y1 = (float)(32768 - (int)(32768.0f - (A.bb < qb ? A.bb : qb)));
    ixs = 1.0f / A.xs; iys = 1.0f / A.ys;
    arA = A.mr + 1.0f; abA = A.mb + 1.0f;
    if (!(x1 > x0)) return 0;
    if (!rotated_eps(A.ang)) {
        float lx = (x0 - A.x) * ixs + A.xo;
        for (xc = x0; x1 > xc; xc = xc + 1.0f, lx = lx + ixs) {
            float tx;
            if (A.ml > lx || lx >= arA || ql > xc || xc > qr || !(y1 > y0)) continue;
            tx = (float)(int)lx;
            for (yc = y0; y1 > yc; yc = yc + 1.0f) {
                float ly = (yc - A.y) * iys + A.yo, ty;
                if (A.mt > ly || ly >= abA || A.ml > tx || tx > A.mr) continue;
                ty = (float)(int)ly;
                if (A.mt > ty || ty > A.mb || !pc_bit(&A, tx, ty)) continue;
                if (qt > yc || yc > qb) continue;
                return 1;
            }
        }
        return 0;
    }
    {
        float a = A.ang * -3.14159274101257324f, sn, cs;
        a = a / 180.0f;
        pcol_sincosf(a, &sn, &cs);
        for (xc = x0; x1 > xc; xc = xc + 1.0f) {
            float dx, cdx, ndx;
            if (ql > xc || xc > qr || !(y1 > y0)) continue;
            dx = xc - A.x; cdx = cs * dx; ndx = dx * -sn;
            for (yc = y0; y1 > yc; yc = yc + 1.0f) {
                float dy = yc - A.y, lx, ly, tx, ty;
                lx = (sn * dy + cdx) * ixs + A.xo;
                if (A.ml > lx || lx >= arA) continue;
                ly = (dy * cs + ndx) * iys + A.yo;
                if (A.mt > ly || ly >= abA) continue;
                tx = (float)(int)lx;
                if (A.ml > tx || tx > A.mr) continue;
                ty = (float)(int)ly;
                if (A.mt > ty || ty > A.mb || !pc_bit(&A, tx, ty)) continue;
                if (qt > yc || yc > qb) continue;
                return 1;
            }
        }
        return 0;
    }
}

static int rect_hit(int k, struct rq *q, int prec)
{
    double l, t, r, b;
    int32_t ib[4];
    if (q->iok && pin_ibox(k, ib)) {
        int32_t i0 = q->ilx > ib[0] ? q->ilx : ib[0], i1 = q->ihx < ib[2] ? q->ihx : ib[2];
        int32_t j0 = q->ily > ib[1] ? q->ily : ib[1], j1 = q->ihy < ib[3] ? q->ihy : ib[3];
        if (!prec || !precise(k))
            return i0 < i1 && j0 < j1;
    }
    if (!pin_bbox(k, &l, &t, &r, &b))
        return 0;
    if (!q->fok) rq_floats(q);
    if (!prec || !precise(k)) {
        /* CInstance::Collision_Rectangle, the bounding box (floats): outside when xmin >= right, left > xmax (and
           in y); else a miss when the overlap's ends round (floor(v + 0.5)) to the same column, or row */
        float fl = (float)l, ft = (float)t, fr = (float)r, fb = (float)b, c0, c1;
        if (q->flx >= fr || fl > q->fhx || q->fly >= fb || ft > q->fhy)
            return 0;
        c0 = (q->flx > fl ? q->flx : fl) + 0.5f;
        c1 = (q->fhx < fr ? q->fhx : fr) + 0.5f;
        if (dfloor(c0) == dfloor(c1)) return 0;
        c0 = (q->fly > ft ? q->fly : ft) + 0.5f;
        c1 = (q->fhy < fb ? q->fhy : fb) + 0.5f;
        if (dfloor(c0) == dfloor(c1)) return 0;
        return 1;
    }
    {   /* CSprite::PreciseCollisionRectangle: pixel centres of the box overlap that lie in the query (closed) */
        float fl = (float)l, ft = (float)t, fr = (float)r, fb = (float)b;
        if (q->flx >= fr || fl > q->fhx || q->fly >= fb || ft > q->fhy)
            return 0;
        return precise_rect(k, q->flx, q->fly, q->fhx, q->fhy);
    }
}

static int rect_cb(int k, void *v)
{
    struct qctx *q = (struct qctx *)v;
    if (k >= PIN_MAX || !match(k, q->obj, q->notme) || !rect_hit(k, q->rq, q->prec)) return 1;
    q->hit = k;
    return 0;
}

static int rect_run(struct rq *rq, int q, const float *r, int obj, int prec, int notme_self);

/* a prec 0 query of a static family (the index) with integer corners (iok: rect_hit's integer test on BB_INT boxes,
   max(l, bl) < min(r, br) and in y, so a hit has a pixel of the box inside the corners; an empty box never hits; a
   box of another kind is in xfar) and no entry of the family in the cells they cover: NOONE. The host builds test
   every entry of the family beside every answer */
static int rq_static_none(struct rq *rq, int obj, int prec, int notme_self)
{
#ifndef PCOL_EXACT
    if (prec || obj < 0 || xf_of[obj] < 0 || !rq->iok || pcol_quiet() ||
        !xrect_none(xf_of[obj], rq->ilx, rq->ily, rq->ihx, rq->ihy))
        return 0;
#ifdef PLAY_STATS
    {   /* every entry of the family, by rect_hit */
        struct rq c = *rq;
        struct fam it;
        int k;
        fam_begin(&it, obj);
        while ((k = fam_get(&it)) != NOONE)
            if (!match(k, obj, notme_self) || !rect_hit(k, &c, prec)) continue;
            else break;
        if (k != NOONE) {
            fprintf(stderr, "collision_rect: static-family index miss differs (%d %d %d %d %d)\n", obj, rq->ilx,
                    rq->ily, rq->ihx, rq->ihy);
            abort();
        }
    }
#endif
    return 1;
#else
    (void)rq; (void)obj; (void)prec; (void)notme_self;
    return 0;
#endif
}

#if !defined(PCOL_EXACT)
static int dfloor14(double v, int32_t *o);
/* a family of at most 4 alive instances whose integer boxes (pin_ibox) all lie off the query's floors: X0 = floor of
   the lesser x corner, X1 of the greater (floor is monotonic), box right b[2] < X0 or left b[0] > X1 + 1, or the same
   in y. Then rect_hit fails for each: its integer test takes corners floor(v + 0.5) in [X0, X1 + 1] (or v itself
   when whole), so max(lx, l) < min(hx, r) cannot hold; its float test takes (float)v in [X0, X1 + 1] (rounding is
   monotonic and X0, X1 + 1 are floats), so x0 >= r or l > x1 holds, and the precise test needs that overlap too.
   So no instance is hit, whatever the search order. The skipped walk's only other effects are the stale touches
   (pcol_touch), which the grid build's searches do not depend on (pobj.c PLAY_REST), and the tree search's caches
   (pcol_query, which runs before, did the flush). The host builds test every instance with rect_hit */
static int rect_far_none_i(int32_t a, int32_t b, int32_t c, int32_t d, int obj, int prec, int notme_self);
static int rect_far_none(double x1, double y1, double x2, double y2, int obj, int prec, int notme_self)
{
    int32_t a, b, c, d;
    if (obj < 0 || olive[obj] > 4 || !dfloor14(x1, &a) || !dfloor14(x2, &c) || !dfloor14(y1, &b) || !dfloor14(y2, &d))
        return 0;
    if (!rect_far_none_i(a, b, c, d, obj, prec, notme_self)) return 0;
#ifdef PLAY_STATS
    {
        struct rq rq;
        struct fam it;
        int k;
        rq_init(&rq, x1, y1, x2, y2);
        fam_begin(&it, obj);
        while ((k = fam_get(&it)) != NOONE)
            if (match(k, obj, notme_self) && rect_hit(k, &rq, prec)) {
                fprintf(stderr, "collision_rect_p: far answer misses %d (%.17g %.17g %.17g %.17g)\n", k, x1, y1, x2, y2);
                abort();
            }
    }
#endif
    return 1;
}

/* rect_far_none on the corners' floors a = floor(x1), b = floor(y1), c, d (|.| < 2^14: dfloor14's range; whole corners
   are their own floors) */
static int rect_far_none_i(int32_t a, int32_t b, int32_t c, int32_t d, int obj, int prec, int notme_self)
{
    int32_t X0, X1, Y0, Y1, ib[4];
    int k;
    struct fam it;
    if (obj < 0 || olive[obj] > 4 || a <= -16384 || a >= 16384 || b <= -16384 || b >= 16384 || c <= -16384 ||
        c >= 16384 || d <= -16384 || d >= 16384)
        return 0;
    X0 = a < c ? a : c; X1 = a < c ? c : a;
    Y0 = b < d ? b : d; Y1 = b < d ? d : b;
    fam_begin(&it, obj);
    while ((k = fam_get(&it)) != NOONE)
        if (!pin_ibox(k, ib) || !(ib[2] < X0 || ib[0] > X1 + 1 || ib[3] < Y0 || ib[1] > Y1 + 1)) return 0;
#ifdef PLAY_STATS
    {   /* (the floors' query: the far answer holds for every rectangle with these floors) */
        struct rq rq;
        rq_init(&rq, a, b, c, d);
        fam_begin(&it, obj);
        while ((k = fam_get(&it)) != NOONE)
            if (match(k, obj, notme_self) && rect_hit(k, &rq, prec)) {
                fprintf(stderr, "collision_rect_p: far answer misses %d (%d %d %d %d)\n", k, a, b, c, d);
                abort();
            }
    }
#else
    (void)prec; (void)notme_self;
#endif
    return 1;
}
#endif

int (collision_rect_p)(double x1, double y1, double x2, double y2, int obj, int prec, int notme_self)
{
    int q = pcol_query(obj);
    struct rq rq;
    float r[4];
    PWST(rect, 1);
    if (q < 0) return NOONE;
#if !defined(PCOL_EXACT)
    if (rect_far_none(x1, y1, x2, y2, obj, prec, notme_self)) return NOONE;
#endif
    rq_init(&rq, x1, y1, x2, y2);
    if (rq_static_none(&rq, obj, prec, notme_self)) return NOONE;
    if (!rq.fok) return rect_run(&rq, q, 0, obj, prec, notme_self);   /* whole corners: qrect's are the ints +-1 */
    if (q == 1) qrect(x1, y1, x2, y2, r);
    return rect_run(&rq, q, r, obj, prec, notme_self);
}

static int rect_q_i(int q, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int obj, int prec, int notme_self);

/* collision_rectangle with whole-number corners (|v| < 30000): floor(v + 0.5) is v */
int collision_rect_i(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int obj, int prec, int notme_self)
{
    int q = pcol_query(obj);
    PWST(rect, 1);
    if (q < 0) return NOONE;
    return rect_q_i(q, x1, y1, x2, y2, obj, prec, notme_self);
}

/* collision_rect_p(x + l, y + t, x + r, y + b, obj, 0, noone) at instance i's x, y (|l|, ... <= 16): at a whole x, y
   (|.| < 29900: xy_int_near) the corners are whole doubles below 2^14 in magnitude or not, and collision_rect_p's
   steps are taken on the ints: pcol_query, the far test (its dfloor14 of a whole corner is the corner; the range test
   as dfloor14's), rq_init's whole path (collision_rect_i's query), the search */
int (collision_rect_at)(int i, int32_t l, int32_t t, int32_t r, int32_t b, int obj)
{
    int32_t x, y;
    int q;
    if (!xy_int_near(i, &x, &y))
        return (collision_rect_p)(PTOD(PW.in[i].x) + l, PTOD(PW.in[i].y) + t, PTOD(PW.in[i].x) + r, PTOD(PW.in[i].y) + b,
                                  obj, 0, NOONE);
    q = pcol_query(obj);
    PWST(rect, 1);
    if (q < 0) return NOONE;
#if !defined(PCOL_EXACT)
    if (obj >= 0 && olive[obj] <= 4 && rect_far_none_i(x + l, y + t, x + r, y + b, obj, 0, NOONE)) return NOONE;
#endif
    return rect_q_i(q, x + l, y + t, x + r, y + b, obj, 0, NOONE);
}

static int rect_q_i(int q, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int obj, int prec, int notme_self)
{
    struct rq rq;
    rq.iok = 1;
    rq.ilx = x1 < x2 ? x1 : x2; rq.ihx = x1 < x2 ? x2 : x1;
    rq.ily = y1 < y2 ? y1 : y2; rq.ihy = y1 < y2 ? y2 : y1;
    rq.fok = 0;                                 /* (the float corners were left unset before 2026-10-04) */
    if (rq_static_none(&rq, obj, prec, notme_self)) return NOONE;
    return rect_run(&rq, q, 0, obj, prec, notme_self);
}

static int rect_run(struct rq *rq, int q, const float *r, int obj, int prec, int notme_self)
{
    int k;
    if (q == 1) {
        struct qctx c;
        c.obj = obj; c.notme = notme_self; c.prec = prec; c.hit = NOONE; c.rq = rq;
        if (r) pcol_search(r[0], r[1], r[2], r[3], rect_cb, &c);
        else pcol_search_i(rq->ilx - 1, rq->ily - 1, rq->ihx + 1, rq->ihy + 1, rect_cb, &c);
        return c.hit;
    }
    {
        struct fam it;
        fam_begin(&it, obj);
        while ((k = fam_get(&it)) != NOONE) {
            if (k == notme_self) continue;
            pcol_touch(k);
            if (rect_hit(k, rq, prec))
                return k;
        }
    }
    return NOONE;
}

/* CSprite::PreciseCollision's unrotated loop for two instances with integer boxes (BB_INT: scale +-1, angle 0,
   whole x, y; a moved by whole dx, dy): every value the runner's float loop forms is a half-integer it holds
   exactly, so the loop runs on doubled integers: pixel centre c + 0.5 of the box overlap, sprite column
   2 lx = (2c + 1 - 2x) * xscale + 2 xorigin (odd), in the mask box when 2 l <= 2 lx < 2 r + 2, its column
   trunc(lx) = (2 lx) / 2 (C division truncates as cvttss2si); rows the same */
struct pci { int32_t x, y; int sx, sy, xo, yo, ml, mt, mr, mb, bpr; const uint8_t *mask; };

/* f > 0 on the bits (no soft-float compare): above +0 and not above +inf (a positive NaN is not > 0) */
static int pci_fpos(float f)
{
    union { float f; int32_t b; } v;
    v.f = f;
    return v.b > 0 && v.b <= 0x7f800000;
}

static void pci_of(int i, int32_t dx, int32_t dy, struct pci *q)
{
    const struct pin *p = &PW.in[i];
    int s = spr_of(p);
    const struct gsprcol *c = &gsprcol[s];
    const struct psprite *ps = &psprite[s];
    if (p->bbk == BB_INTS) {                        /* its box alone (bbox_ints): every column and row passes */
        q->x = q->y = 0; q->sx = q->sy = 1; q->xo = q->yo = 0;
        q->ml = q->mt = -(1 << 28); q->mr = q->mb = 1 << 28;
        q->bpr = 0; q->mask = 0;
        return;
    }
    if (!pin_xy_int_p(p, &q->x, &q->y)) {         /* (a BB_INT box: whole x, y; else as pos_int leaves them) */
        pos_int(p->x, &q->x);
        pos_int(p->y, &q->y);
    }
    q->x += dx; q->y += dy;
    q->sx = pci_fpos(p->xscale) ? 1 : -1;
    q->sy = pci_fpos(p->yscale) ? 1 : -1;
    q->xo = c->xo; q->yo = c->yo;
    q->ml = c->l; q->mt = c->t; q->mr = c->r; q->mb = c->b;
    q->bpr = ((c->r - c->l + 1) + 7) >> 3;
    q->mask = 0;
    if (c->kind == 1 && ps->nmasks > 0) {
        int f = 0;
        if (ps->nmasks > 1) {
            f = (int)p->img % ps->nmasks;
            if (f < 0) f += ps->nmasks;
        }
        q->mask = pmaskdata + ps->maskoff + f * q->bpr * (c->b - c->t + 1);
    }
}

static int pci_bit(const struct pci *q, int cx, int cy)
{
    static const uint8_t bit[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
    cx -= q->ml;
    cy -= q->mt;
    return (q->mask[cy * q->bpr + (cx >> 3)] & bit[cx & 7]) != 0;
}

/* line_hit's precise test of the vertical whole line x = X, y = lo .. hi (lo < hi) against entry k with an integer box
   (BB_INT: angle 0, scales +-1, whole x, y) and a mask, on ints. line_box_f: a miss when X is outside [l, r) or the
   segment outside [t, b] in y; otherwise it keeps the segment (X <= r - 1 < r - 1e-5: no clip). precise_line's
   vertical branch (slope 0): v = max(t, lo) .. min(b, hi) in steps of 1 (floats of ints: exact), the mask column
   floor((X - x) / xs + xo) = xo + (X - x) xs and row floor((v - y) / ys + yo) = yo + (v - y) ys (angle 0: cs 1, sn
   and ns signed zeros), inside the mask box, the bit (pci_bit as pc_bit). -1: another case (the floats) */
static int line_hit_vi(int k, int32_t X, int32_t lo, int32_t hi)
{
    struct pci A;
    const struct pin *p = &PW.in[k];
    int32_t v, v1, tx;
    if (p->bbk != BB_INT || lo >= hi) return -1;
    pci_of(k, 0, 0, &A);
    if (!A.mask) return -1;
    if (X < p->bl || X >= p->br || lo >= p->bb || hi < p->bt) return 0;
    tx = A.xo + (X - A.x) * A.sx;
    if (tx < A.ml || tx > A.mr) return 0;
    v = lo > p->bt ? lo : p->bt;
    v1 = hi < p->bb ? hi : p->bb;
    for (; v <= v1; v++) {
        int32_t ty = A.yo + (v - A.y) * A.sy;
        if (ty >= A.mt && ty <= A.mb && pci_bit(&A, tx, ty)) return 1;
    }
    return 0;
}

/* the same for the horizontal whole line y = Y, x = lo .. hi (lo < hi). line_box_f: a miss when Y is outside [t, b) or
   the segment outside [l, r] in x; otherwise the segment from xa = max(l, lo) (the clip at l: y unchanged, slope 0)
   to xb = hi, or to r' = (float)r + -1e-5f when hi > r'. For 0 < r < 2^14, r' < r when r <= 256 (the float spacing
   just below r is at most 2^-16 < 2 x 1e-5 there; at 256 it is 2^-16 too) and r' = r above 256 (spacing below r at
   least 2^-15 > 2 x 1e-5), so the last whole x <= xb is min(hi, r <= 256 ? r - 1 : r). precise_line's horizontal
   branch (slope 0) walks v = xa .. that x, column xo + (v - x) xs, row yo + (Y - y) ys; where the clipped segment is
   one point (xa = xb) it takes precise_point, whose column and row at whole x, y and angle 0 are the same
   (floor((px - x) / xs + xo), ...) with the same frame and mask box: one step of the walk. -1: r outside (0, 2^14),
   not BB_INT, no mask */
static int line_hit_hi(int k, int32_t Y, int32_t lo, int32_t hi)
{
    struct pci A;
    const struct pin *p = &PW.in[k];
    int32_t v, v1, ty, r = p->br;
    if (p->bbk != BB_INT || lo >= hi || r <= 0 || r >= 16384) return -1;
    pci_of(k, 0, 0, &A);
    if (!A.mask) return -1;
    if (Y < p->bt || Y >= p->bb || lo >= r || hi < p->bl) return 0;
    v = lo > p->bl ? lo : p->bl;
    v1 = r <= 256 ? r - 1 : r;
    if (hi < v1) v1 = hi;
    if (v > v1) return 0;
    ty = A.yo + (Y - A.y) * A.sy;
    if (ty < A.mt || ty > A.mb) return 0;
    for (; v <= v1; v++) {
        int32_t tx = A.xo + (v - A.x) * A.sx;
        if (tx >= A.ml && tx <= A.mr && pci_bit(&A, tx, ty)) return 1;
    }
    return 0;
}

/* the pixel loop of precise_collision_int as the runner runs it (pixel by pixel) */
static int pci_loop(const struct pci *Ap, const struct pci *Bp, int32_t dx, int32_t dy, const int32_t *ia, const int32_t *ib)
{
    const struct pci A = *Ap, B = *Bp;
    int32_t x0, x1, y0, y1, c, r;
    x0 = ia[0] + dx > ib[0] ? ia[0] + dx : ib[0];
    x1 = ia[2] + dx < ib[2] ? ia[2] + dx : ib[2];
    y0 = ia[1] + dy > ib[1] ? ia[1] + dy : ib[1];
    y1 = ia[3] + dy < ib[3] ? ia[3] + dy : ib[3];
    for (c = x0; c < x1; c++) {
        int32_t lA = (2 * c + 1 - 2 * A.x) * A.sx + 2 * A.xo, lB = (2 * c + 1 - 2 * B.x) * B.sx + 2 * B.xo;
        int32_t tA = lA / 2, tB = lB / 2;
        int okA, okB;
        if (lA < 2 * A.ml || lA >= 2 * A.mr + 2 || lB < 2 * B.ml || lB >= 2 * B.mr + 2) continue;
        okA = tA >= A.ml && tA <= A.mr;
        okB = tB >= B.ml && tB <= B.mr;
        if ((A.mask && !okA) || (B.mask && !okB)) continue;
        for (r = y0; r < y1; r++) {
            int32_t mA = (2 * r + 1 - 2 * A.y) * A.sy + 2 * A.yo, mB, u;
            if (mA < 2 * A.mt || mA >= 2 * A.mb + 2) continue;
            if (A.mask) {
                u = mA / 2;
                if (u < A.mt || u > A.mb || !pci_bit(&A, tA, u)) continue;
            }
            mB = (2 * r + 1 - 2 * B.y) * B.sy + 2 * B.yo;
            if (mB < 2 * B.mt || mB >= 2 * B.mb + 2) continue;
            if (!B.mask) return 1;
            u = mB / 2;
            if (u < B.mt || u > B.mb) continue;
            if (pci_bit(&B, tB, u)) return 1;
        }
    }
    return 0;
}

/* n (1 .. 25) bits of a mask row from bit off on, the first in bit 31, the rest 0 (the bytes past the last one needed
   are not read) */
static uint32_t pci_row(const uint8_t *row, int32_t off, int n)
{
    const uint8_t *p = row + (off >> 3);
    int sh = off & 7, last = sh + n - 1;
    uint32_t v = (uint32_t)p[0] << 24;
    if (last >= 8) v |= (uint32_t)p[1] << 16;
    if (last >= 16) v |= (uint32_t)p[2] << 8;
    if (last >= 24) v |= p[3];
    return (v << sh) & (0xffffffffu << (32 - n));
}

#define BR2(n) n, n + 128, n + 64, n + 192
#define BR4(n) BR2(n), BR2(n + 32), BR2(n + 16), BR2(n + 48)
#define BR6(n) BR4(n), BR4(n + 8), BR4(n + 4), BR4(n + 12)
static const uint8_t brev8[256] = { BR6(0), BR6(2), BR6(1), BR6(3) };
#undef BR2
#undef BR4
#undef BR6
static uint32_t rev32(uint32_t v)
{
    return (uint32_t)brev8[v >> 24] | (uint32_t)brev8[(v >> 16) & 255] << 8 | (uint32_t)brev8[(v >> 8) & 255] << 16 |
           (uint32_t)brev8[v & 255] << 24;
}

/* the columns c (rows the same) whose mask column k lies in q's mask box [ml, mr]: k = c - x + xo at scale 1,
   x + xo - 1 - c at -1 (pci_loop: 2k + 1 = (2c + 1 - 2x) sx + 2 xo, in [2 ml, 2 mr + 2) exactly when ml <= k <= mr) */
static void pci_span(int32_t x, int s, int o, int lo, int hi, int32_t *c0, int32_t *c1)
{
    int32_t a = s > 0 ? x - o + lo : x + o - 1 - hi, b = s > 0 ? x - o + hi : x + o - 1 - lo;
    if (a > *c0) *c0 = a;
    if (b < *c1) *c1 = b;
}

/* CSprite::PreciseCollision for two BB_INT / BB_INTS instances (pci_loop) by mask rows. Both scales are +-1, so the
   loop's column test is k in [ml, mr] for k above (an interval of c: pci_span), and where a mask's box starts at
   column and row >= 0 its column trunc((2k + 1) / 2) is k itself (2k + 1 > 0) and okA always holds: the loop finds a
   hit exactly when some pixel (c, r) of the overlap, both column spans and both row spans has both mask bits set (an
   instance without a mask: set). Up to 25 columns at a time: each mask's bits of the row as one word (a flipped one
   read forwards from its lowest column, then reversed when the other is not flipped: only whether the AND is 0
   matters). A box starting below 0 takes pci_loop */
static int precise_collision_int(int a, int32_t dx, int32_t dy, const int32_t *ia, int b, const int32_t *ib)
{
    struct pci A, B;
    int32_t c0, c1, r0, r1, c, r;
    pci_of(a, dx, dy, &A);
    pci_of(b, 0, 0, &B);
    if ((A.mask && (A.ml < 0 || A.mt < 0)) || (B.mask && (B.ml < 0 || B.mt < 0)))
        return pci_loop(&A, &B, dx, dy, ia, ib);
    c0 = ia[0] + dx > ib[0] ? ia[0] + dx : ib[0];
    c1 = (ia[2] + dx < ib[2] ? ia[2] + dx : ib[2]) - 1;
    r0 = ia[1] + dy > ib[1] ? ia[1] + dy : ib[1];
    r1 = (ia[3] + dy < ib[3] ? ia[3] + dy : ib[3]) - 1;
    pci_span(A.x, A.sx, A.xo, A.ml, A.mr, &c0, &c1);
    pci_span(B.x, B.sx, B.xo, B.ml, B.mr, &c0, &c1);
    pci_span(A.y, A.sy, A.yo, A.mt, A.mb, &r0, &r1);
    pci_span(B.y, B.sy, B.yo, B.mt, B.mb, &r0, &r1);
    {
        int res = 0;
        if (c0 > c1 || r0 > r1) goto done;
        if (!A.mask && !B.mask) { res = 1; goto done; }
        for (c = c0; c <= c1; c += 25) {
            int n = c1 - c + 1 < 25 ? c1 - c + 1 : 25;
            /* the lowest mask column of the n, its offset in the row, and whether the word is reversed */
            int32_t ka = (A.sx > 0 ? c - A.x + A.xo : A.x + A.xo - 1 - (c + n - 1)) - A.ml;
            int32_t kb = (B.sx > 0 ? c - B.x + B.xo : B.x + B.xo - 1 - (c + n - 1)) - B.ml;
            int flip = A.sx != B.sx;
            for (r = r0; r <= r1; r++) {
                uint32_t v = 0xffffffffu, w;
                if (A.mask) {
                    int32_t j = (A.sy > 0 ? r - A.y + A.yo : A.y + A.yo - 1 - r) - A.mt;
                    v = pci_row(A.mask + j * A.bpr, ka, n);
                    if (!v) continue;
                    if (flip && B.mask) v = rev32(v) << (32 - n);
                }
                if (B.mask) {
                    int32_t j = (B.sy > 0 ? r - B.y + B.yo : B.y + B.yo - 1 - r) - B.mt;
                    w = pci_row(B.mask + j * B.bpr, kb, n);
                    v &= w;
                }
                if (v) { res = 1; goto done; }
            }
        }
    done:
#ifdef PLAY_STATS
        if (res != pci_loop(&A, &B, dx, dy, ia, ib)) {
            fprintf(stderr, "precise_collision_int: row answer %d differs (%d, %d)\n", res, a, b);
            abort();
        }
#endif
        return res;
    }
}

#ifdef FCOL_STATS
/* play.h FCOL_STATS: the per-call-site counts (playhost_fcol) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct fcol_e { const char *file; int line, kind; unsigned long calls, frac; };
static struct fcol_e fcol_t[8192];
static const char *fcol_pf, *fcol_of;
static int fcol_pl, fcol_ol, fcol_depth, fcol_reg;
static void fcol_dump(void)
{
    const char *o = getenv("FCOL_OUT");
    FILE *f;
    int k;
    if (!o || !(f = fopen(o, "a"))) return;
    for (k = 0; k < 8192; k++)
        if (fcol_t[k].file) {
            const char *b = strrchr(fcol_t[k].file, '/');
            fprintf(f, "%s %d %d %lu %lu\n", b ? b + 1 : fcol_t[k].file, fcol_t[k].line, fcol_t[k].kind, fcol_t[k].calls,
                    fcol_t[k].frac);
        }
    fclose(f);
}
static void fcol_add(const char *file, int line, int kind, int frac)
{
    unsigned h = ((unsigned)(size_t)file * 31u + (unsigned)line * 7u + (unsigned)kind) & 8191u;
    if (!fcol_reg) { fcol_reg = 1; atexit(fcol_dump); }
    while (fcol_t[h].file && !(fcol_t[h].file == file && fcol_t[h].line == line && fcol_t[h].kind == kind))
        h = (h + 1) & 8191u;
    fcol_t[h].file = file; fcol_t[h].line = line; fcol_t[h].kind = kind;
    fcol_t[h].calls++;
    fcol_t[h].frac += frac != 0;
}
void fcol_site(const char *file, int line) { if (!fcol_pf && !fcol_depth) { fcol_pf = file; fcol_pl = line; } }
void fcol_clear(void) { if (!fcol_depth) fcol_pf = NULL; }
static int fcol_int(double v) { return v > -30000 && v < 30000 && v == (double)(int32_t)v; }
int fcol_note(int kind, int n, double a, double b, double c, double d)
{
    int frac;
    if (fcol_depth++) return 0;
    frac = !fcol_int(a) || !fcol_int(b) || (n == 4 && (!fcol_int(c) || !fcol_int(d)));
    fcol_add(fcol_pf, fcol_pl, kind, frac);
    fcol_of = fcol_pf; fcol_ol = fcol_pl;
    fcol_pf = NULL;
    return 1;
}
void fcol_done(int counted) { fcol_depth--; if (counted) fcol_of = NULL; }
#endif

/* instance a (its bbox moved by dx, dy) against instance b */
static int overlap_at(int a, double dx, double dy, int b)
{
    double l, t, r, bb, l2, t2, r2, b2, ba[4];
    int32_t ia[4], ib[4], idx, idy;
    if (pin_ibox_s(a, ia) && pin_ibox_s(b, ib) && whole(dx, &idx) && whole(dy, &idy)) {
        if (!(ia[0] + idx < ib[2] && ib[0] < ia[2] + idx && ia[1] + idy < ib[3] && ib[1] < ia[3] + idy))
            return 0;
        if (!precise(a) && !precise(b))
            return 1;
        return precise_collision_int(a, idx, idy, ia, b, ib);
    }
#ifdef FCOL_STATS
    fcol_add(fcol_of ? fcol_of : "overlap_at(pass)", fcol_of ? fcol_ol : 0, FK_OVL, 1);
#endif
    if (!pin_bbox(a, &l, &t, &r, &bb) || !pin_bbox(b, &l2, &t2, &r2, &b2))
        return 0;
    ba[0] = l; ba[1] = t; ba[2] = r; ba[3] = bb;
    l += dx; r += dx; t += dy; bb += dy;
    if (!(l < r2 && l2 < r && t < b2 && t2 < bb))
        return 0;
    if (!precise(a) && !precise(b)) {
        /* CInstance::Collision_Instance, neither precise (floats): a miss when the overlap's ends round to the same
           column (floor(max left + 0.49999), floor(min right + 0.5)), or row */
        float fl = (float)l, ft = (float)t, fr = (float)r, fb = (float)bb;
        float gl = (float)l2, gt = (float)t2, gr = (float)r2, gb = (float)b2;
        if (dfloor((fl > gl ? fl : gl) + 0.49998999f) == dfloor((fr < gr ? fr : gr) + 0.5f)) return 0;
        if (dfloor((ft > gt ? ft : gt) + 0.49998999f) == dfloor((fb < gb ? fb : gb) + 0.5f)) return 0;
        return 1;
    }
    {   /* one precise: SeparatingAxisCollision when either is rotated, then CSprite::PreciseCollision */
        struct pcinst A, B;
        double bbx[4];
        bbx[0] = l2; bbx[1] = t2; bbx[2] = r2; bbx[3] = b2;
        if (!pcinst_of_b(a, dx, dy, &A, ba) || !pcinst_of_b(b, 0, 0, &B, bbx)) return 0;
        if ((A.ang != 0 || B.ang != 0) && !sa_collision(&A, &B)) return 0;
        return precise_collision(&A, &B);
    }
}

int pin_overlap(int a, int b)
{
    return overlap_at(a, 0, 0, b);
}

static int place_cb(int k, void *v)
{
    struct qctx *q = (struct qctx *)v;
    if (k >= PIN_MAX || !match(k, q->obj, q->self) || !overlap_at(q->self, q->dx, q->dy, k)) return 1;
    q->hit = k;
    return 0;
}

#ifndef PCOL_EXACT
/* instance_place's candidates from the static-family index: self's integer box moved by whole dx, dy ([l, r) x [t, b),
   not empty, inside the index's cells). An entry that overlap_at finds overlapping it has a non-empty integer box
   (xfar and xemp 0: no other kind is placed) sharing a pixel with it, so it reaches one of its cells. -2: the index
   cannot tell; NOONE: no entry of obj's family reaches them; else the only entry that does (the counts are 1 in all,
   and the cell's hint is a placed entry of the family in that cell, so it is the counted one) */
static int xplace_one_i(int self, int32_t idx, int32_t idy, int obj)
{
    int32_t ia[4], l, t, r, b;
    int f, x, y, n = 0, cx = 0, cy = 0, k;
    if (obj < 0 || (f = xf_of[obj]) < 0 || pcol_quiet()) return -2;
    if (!pin_ibox_s(self, ia)) return -2;
    l = ia[0] + idx; t = ia[1] + idy; r = ia[2] + idx; b = ia[3] + idy;
    if (r <= l || b <= t || l < 0 || t < 0 || ((r - 1) >> 4) >= GRID_W || ((b - 1) >> 4) >= GRID_H) return -2;
    if (xdhead >= 0) xflush_run();
    if (xfar[f] || xsat[f] || xemp[f]) return -2;
    for (y = t >> 4; y <= (b - 1) >> 4; y++)
        for (x = l >> 4; x <= (r - 1) >> 4; x++)
            if (xcnt[f][y][x]) {
                n += xcnt[f][y][x];
                if (n > 1) return -2;
                cx = x;
                cy = y;
            }
    if (n == 0) return NOONE;
    k = xhint[cy][cx];
    if (k < 0 || !PW.in[k].alive || !(xmask[k] & xf_bit[f]) || xr[k].isfar || cx < xr[k].x0 || cx > xr[k].x1 ||
        cy < xr[k].y0 || cy > xr[k].y1)
        return -2;
    return k;
}

/* xplace_one_i(self, idx, idy, obj) given self's box ia (pin_ibox_s): when the moved box is one 16 x 16 cell of the
   index (check_water's neighbours), its tests on that cell inline (the loop's one count: 0 NOONE, over 1 -2; the hint's
   checks), in the same order after the same checks; otherwise xplace_one_i */
static inline int xplace_cell(int self, const int32_t *ia, int32_t idx, int32_t idy, int obj)
{
    int32_t l = ia[0] + idx, t = ia[1] + idy;
    int f, cx, cy, n, k;
    if (ia[2] + idx != l + 16 || ia[3] + idy != t + 16 || (l & 15) || (t & 15) || l < 0 || t < 0 ||
        (cx = l >> 4) >= GRID_W || (cy = t >> 4) >= GRID_H || obj < 0 || (f = xf_of[obj]) < 0)
        return xplace_one_i(self, idx, idy, obj);
    if (pcol_quiet()) return -2;
    if (xdhead >= 0) xflush_run();
    if (xfar[f] || xsat[f] || xemp[f]) return -2;
    n = xcnt[f][cy][cx];
    if (n == 0) return NOONE;
    if (n > 1) return -2;
    k = xhint[cy][cx];
    if (k < 0 || !PW.in[k].alive || !(xmask[k] & xf_bit[f]) || xr[k].isfar || cx < xr[k].x0 || cx > xr[k].x1 ||
        cy < xr[k].y0 || cy > xr[k].y1)
        return -2;
    return k;
}

static int xplace_one(int self, double dx, double dy, int obj)
{
    int32_t idx, idy;
    if (!whole(dx, &idx) || !whole(dy, &idy)) return -2;
    return xplace_one_i(self, idx, idy, obj);
}
#endif

/* Command_InstancePlace: SetPosition(px, py) (a real move marks self dirty), the search, SetPosition back */
static int place_after_query(int self, int q, double px, double py, double dx, double dy, int moved, int obj)
{
    int k;
    if (q < 0) return NOONE;
    if (q == 1) {
        struct qctx c;
        double l, t, r, b;
        pcol_touch(self);
        if (moved) pcol_place_marks(self);
        c.obj = obj; c.self = self; c.hit = NOONE; c.dx = dx; c.dy = dy;
        if (pin_bbox(self, &l, &t, &r, &b)) {
            float fl = (float)(l + dx), ft = (float)(t + dy), fr = (float)(r + dx), fb = (float)(b + dy);
#ifndef PCOL_EXACT
            /* a static family: the search's hits are its entries whose rectangles meet the query and that pass
               place_cb (match, overlap_at); with at most one entry able to pass overlap_at (xplace_one), the search
               returns it when it is a hit, else NOONE (the other entries' callbacks only fill box caches) */
            k = xplace_one(self, dx, dy, obj);
            if (k != -2) {
                if (k >= 0 && !(pcol_search_has(k, fl, ft, fr, fb) && match(k, obj, self) && overlap_at(self, dx, dy, k)))
                    k = NOONE;
#ifdef PLAY_STATS
                pcol_search(fl, ft, fr, fb, place_cb, &c);
                if (c.hit != k) {
                    fprintf(stderr, "instance_place_p: static-family answer %d differs from %d (%d %d)\n", k, c.hit, self,
                            obj);
                    abort();
                }
#endif
                return k;
            }
#endif
            pcol_search(fl, ft, fr, fb, place_cb, &c);
        } else
            pcol_search((float)px, (float)py, (float)px, (float)py, place_cb, &c);
        return c.hit;
    }
    if (moved) pcol_place_marks(self);
    {
        struct fam it;
        fam_begin(&it, obj);
        while ((k = fam_get(&it)) != NOONE) {
            if (k == self) continue;
            pcol_touch(k);
            pcol_touch_at(self, dx, dy);
            if (overlap_at(self, dx, dy, k)) {
                if (moved) pcol_place_marks(self);
                return k;
            }
        }
    }
    if (moved) pcol_place_marks(self);
    return NOONE;
}

int (instance_place_p)(int self, double px, double py, int obj)
{
    PWST(place, 1);
    int q = pcol_query(obj);
    double dx = px - PTOD(PW.in[self].x), dy = py - PTOD(PW.in[self].y);
    int moved = (float)px != (float)PTOD(PW.in[self].x) || (float)py != (float)PTOD(PW.in[self].y);
    return place_after_query(self, q, px, py, dx, dy, moved, obj);
}

/* (float)v of a box side: fint15's bits below 2^15 in magnitude (no __floatsisf call) */
#define PLACE_F(v) ((v) > -32768 && (v) < 32768 ? fint15(v) : (float)(v))

/* instance_place_p(self, PTOD(x) + idx, PTOD(y) + idy, obj) at self's x, y: instance_place_ixy at a whole near x, y
   (xy_int_near; |idx|, |idy| <= 16), else the doubles */
int instance_place_at(int self, int32_t idx, int32_t idy, int obj)
{
    int32_t x, y;
    if (xy_int_near(self, &x, &y)) return instance_place_ixy(self, x, y, idx, idy, obj);
    return (instance_place_p)(self, PTOD(PW.in[self].x) + idx, PTOD(PW.in[self].y) + idy, obj);
}

/* instance_place_p(self, x + idx, y + idy, obj) for self at whole x, y (|.| < 29900) and |idx|, |idy| <= 16, without
   the doubles where self's box is cached whole (BB_INT / BB_INTS): px, py are whole, so dx, dy are idx, idy exactly,
   moved is idx || idy (whole values below 2^24 are their floats), pin_bbox's box is the ints and the query's floats
   (l + dx, ...) are (float)(int); overlap_at's integer path is taken as it would be (whole dx, dy). Otherwise, and
   off the grid path, instance_place_p's own code on the same values (place_after_query) */
int instance_place_ixy(int self, int32_t x, int32_t y, int32_t idx, int32_t idy, int obj)
{
    PWST(place, 1);
    int q = pcol_query(obj), moved = idx != 0 || idy != 0;
    int32_t ia[4];
    if (q == 1 && pin_ibox_s(self, ia)) {
        struct qctx c;
        pcol_touch(self);
        if (moved) pcol_place_marks_kept(self);              /* (self's tree rectangle is its box: pcol.c pm_e) */
        c.obj = obj; c.self = self; c.hit = NOONE;            /* (c.dx, c.dy: set where a search reads them) */
#ifndef PCOL_EXACT
        {
            int k = xplace_cell(self, ia, idx, idy, obj);
            if (k != -2) {
                if (k >= 0) {
                    int32_t ib[4];
                    int ov;
                    if (pin_ibox_s(k, ib)) {                   /* overlap_at's integer path */
                        ov = ia[0] + idx < ib[2] && ib[0] < ia[2] + idx && ia[1] + idy < ib[3] && ib[1] < ia[3] + idy;
                        if (ov && (precise(self) || precise(k))) ov = precise_collision_int(self, idx, idy, ia, k, ib);
                    } else
                        ov = -1;
                    if (!(pcol_search_has_i(k, ia[0] + idx, ia[1] + idy, ia[2] + idx, ia[3] + idy) &&
                          match(k, obj, self) && (ov >= 0 ? ov : overlap_at(self, idx, idy, k))))
                        k = NOONE;
                }
#ifdef PLAY_STATS
                c.dx = idx; c.dy = idy;
                pcol_search(PLACE_F(ia[0] + idx), PLACE_F(ia[1] + idy), PLACE_F(ia[2] + idx), PLACE_F(ia[3] + idy), place_cb,
                            &c);
                if (c.hit != k) {
                    fprintf(stderr, "instance_place_ixy: static-family answer %d differs from %d (%d %d)\n", k, c.hit,
                            self, obj);
                    abort();
                }
#endif
                return k;
            }
        }
#endif
        c.dx = idx; c.dy = idy;
        pcol_search(PLACE_F(ia[0] + idx), PLACE_F(ia[1] + idy), PLACE_F(ia[2] + idx), PLACE_F(ia[3] + idy), place_cb, &c);
        return c.hit;
    }
    return place_after_query(self, q, (double)(x + idx), (double)(y + idy), idx, idy, moved, obj);
}

/* floor(v) as an int when |v| < 2^14, from the double's bits (dwhole's product: the integer part in the high word,
   the fraction in the low word and in lo) */
static int dfloor14(double v, int32_t *o)
{
    union { double d; uint64_t u; } c;
    uint32_t hi, lo, e, ip, fr;
    uint64_t pr;
    c.d = v;
    hi = (uint32_t)(c.u >> 32);
    lo = (uint32_t)c.u;
    e = (hi >> 20) & 0x7ffu;
    if (e < 1023) {                                   /* |v| < 1 */
        *o = ((hi & 0x80000000u) && ((hi & 0x7fffffffu) | lo)) ? -1 : 0;
        return 1;
    }
    if (e > 1023 + 13) return 0;                      /* |v| >= 2^14, Inf, NaN */
    pr = (uint64_t)((hi & 0xfffffu) | 0x100000u) * dw_mul[e - 1023];
    ip = (uint32_t)(pr >> 32);
    fr = (uint32_t)pr | lo;
    *o = (hi & 0x80000000u) ? -(int32_t)ip - (fr != 0) : (int32_t)ip;
    return 1;
}

/* instance_nearest_p's integer pass reads, per family, the alive instances and floor(x), floor(y) (|.| < 2^14). They
   are kept for up to NC_N families between calls (a lush level's spear traps ask for the nearest oEnemy,
   oMoveableSolid and oItem one after another): an entry is dropped when an instance of its family is linked or
   unlinked (olive_add) and at a level start (olists_reset); when one changes position, sprite, mask, scale or angle
   (pw_changed: every x / y setter with a change; moveTo's raw pixel walks and the rest replay end in one) its floors
   are updated in place (nc_moved).
   ok 2: the family does not fit (more than NEAR_MAX, or a coordinate out of range): the double loop */
#define NEAR_MAX 64
#define NC_N 4
struct ncache { int16_t obj, n; uint8_t ok; int16_t k[NEAR_MAX], x[NEAR_MAX], y[NEAR_MAX]; };
static struct ncache nc[NC_N];
static uint8_t nc_next;
static uint8_t nc_asg;                           /* bit e: slot e has been filled (its obj's family is in nc_ofam) */

/* bit b of nc_ofam over root's family (root and its descendants: pcol.c's fam_obj_next walk), set or cleared */
static void nc_fam_bit(int root, uint8_t b, int set)
{
    int o = root;
    pcol_obj_tree();
    for (;;) {
        pwob[o] = (uint8_t)(set ? pwob[o] | b << 4 : pwob[o] & ~(b << 4));
        if (pcol_ochild[o] >= 0) { o = pcol_ochild[o]; continue; }
        while (o != root && pcol_osib[o] < 0) o = objdefs[o].parent;
        if (o == root) return;
        o = pcol_osib[o];
    }
}

#ifdef PLAY_STATS
static void nc_ofam_check(int obj)
{
    int e;
    for (e = 0; e < NC_N; e++)
        if (!(NC_OFAM(obj) >> e & 1) != !((nc_asg >> e & 1) && obj_is(obj, nc[e].obj))) {
            fprintf(stderr, "nc_ofam[%d] bit %d differs from obj_is\n", obj, e);
            abort();
        }
}
#endif

/* slot e is about to hold obj's family (nc_fill writes nc[e].obj): nc_ofam's bit e moves from the old family */
static void nc_slot(int e, int obj)
{
    uint8_t b = (uint8_t)(1u << e);
    if ((nc_asg & b) && nc[e].obj == obj) return;
    if (nc_asg & b) nc_fam_bit(nc[e].obj, b, 0);
    nc_fam_bit(obj, b, 1);
    nc_asg |= b;
}

static void nc_inval(int obj)
{
    int e;
    for (e = 0; e < NC_N; e++)
        if (nc[e].ok && obj_is(obj, nc[e].obj)) nc[e].ok = 0;
}

/* instance i (alive and linked, or not) may have changed position: a kept entry of a family holding it takes its
   new floors in place (k[]'s order is the lists' walk, which a move does not change); one where it is not found,
   whose floors do not fit, or that did not fit (ok 2) is dropped as nc_inval drops it */
static __attribute__((noinline)) void nc_moved(int i)
{
    int e, j, obj = PW.in[i].obj;
    for (e = 0; e < NC_N; e++) {
        struct ncache *c = &nc[e];
        int32_t xk, yk;
        if (!c->ok || !(NC_OFAM(obj) >> e & 1)) continue;       /* (bit e: obj_is(obj, c->obj)) */
        if (c->ok != 1) { c->ok = 0; continue; }
        for (j = 0; j < c->n && c->k[j] != i; j++) {}
        if (j == c->n || !pl_floor(PW.in[i].x, &xk) || !pl_floor(PW.in[i].y, &yk) ||
            xk < -16384 || xk >= 16384 || yk < -16384 || yk >= 16384) {
            c->ok = 0;
            continue;
        }
        c->x[j] = (int16_t)xk;
        c->y[j] = (int16_t)yk;
    }
}

static void nc_reset(void)
{
    int e;
    for (e = 0; e < NC_N; e++) nc[e].ok = 0;
}

static void nc_fill(struct ncache *c, int obj)
{
    int j, k, n = 0;
    c->obj = (int16_t)obj;
    c->ok = 2;
    for (j = obj; j >= 0; j = ofam_next(obj, j)) {
        int o = j;
        if (olive[o] == 0) continue;
        for (k = pw_ohead[o]; k >= 0; k = pw_inext[k]) {
            int32_t xk, yk;
            if (n == NEAR_MAX || !pl_floor(PW.in[k].x, &xk) || !pl_floor(PW.in[k].y, &yk) ||
                xk < -16384 || xk >= 16384 || yk < -16384 || yk >= 16384)
                return;
            c->k[n] = (int16_t)k; c->x[n] = (int16_t)xk; c->y[n] = (int16_t)yk;
            n++;
        }
    }
    c->n = (int16_t)n;
    c->ok = 1;
}

static struct ncache *nc_get(int obj)
{
    int e;
    for (e = 0; e < NC_N; e++)
        if (nc[e].ok && nc[e].obj == obj) {
#ifdef PLAY_STATS
            {   /* the host builds check a kept entry against a fresh one */
                static struct ncache f;
                int m;
                nc_fill(&f, obj);
                if (f.ok != nc[e].ok || (f.ok == 1 && f.n != nc[e].n)) { fprintf(stderr, "nc_get: family %d changed\n", obj); abort(); }
                for (m = 0; f.ok == 1 && m < f.n; m++)
                    if (f.k[m] != nc[e].k[m] || f.x[m] != nc[e].x[m] || f.y[m] != nc[e].y[m]) {
                        fprintf(stderr, "nc_get: family %d instance %d changed\n", obj, f.k[m]);
                        abort();
                    }
            }
#endif
            return &nc[e];
        }
    e = nc_next;
    nc_next = (uint8_t)((nc_next + 1) & (NC_N - 1));
    nc_any = 1;
    nc_slot(e, obj);
    nc_fill(&nc[e], obj);
    return &nc[e];
}

/* instance_nearest: the instance of least d = dx * dx + dy * dy (doubles), the first of them in the family's order.
   The integer pass: with X = floor(px), Xk = floor(x_k) (all |.| < 2^14, else the double loop) and
   D = X - Xk, dx = px - x_k lies in (D - 1, D + 1), so |dx| is in [lo, hi] = [max(|D| - 1, 0), |D| + 1], and the same
   in y; L = lox^2 + loy^2 and U = hix^2 + hiy^2 are whole numbers below 2^31 (doubles exactly) and rounding is
   monotonic, so the double d of instance k is in [L_k, U_k]. The first instance of least d has L <= its d <= every
   U; the double loop over the instances with L <= min U, in the same order, picks it. NaN cannot occur (every
   operand finite). The integer pass walks the family's object lists one after another (ofam_next, as fam_begin), not in
   creation order; the candidates (L <= min U, a few) are then sorted by creation number (pw_seq: the family's
   order, fam_next's) before the double pass. The family's floors come from nc_get. Up to NEAR_MAX instances, else
   the double loop over all */
int instance_nearest_p(double px, double py, int obj)
{
    int k, best = NOONE;
    double bd = 0;
    struct fam it;
    PWST(nearest, 1);
    if (fam_none(obj)) return NOONE;
    {
        int16_t ck[NEAR_MAX];
        uint32_t cl[NEAR_MAX], mu = 0xffffffffu;
        int32_t X, Y;
        int n = 0, j, m, ok = obj >= 0 && dfloor14(px, &X) && dfloor14(py, &Y);
        struct ncache *c = ok ? nc_get(obj) : 0;
        if (c && c->ok == 1) {
            n = c->n;
            for (j = 0; j < n; j++) {
                uint32_t ax, ay, u;
                int32_t xk = c->x[j], yk = c->y[j];
                ax = (uint32_t)(X >= xk ? X - xk : xk - X);
                ay = (uint32_t)(Y >= yk ? Y - yk : yk - Y);
                u = (ax + 1) * (ax + 1) + (ay + 1) * (ay + 1);
                if (u < mu) mu = u;
                ax = ax ? ax - 1 : 0;
                ay = ay ? ay - 1 : 0;
                ck[j] = c->k[j];
                cl[j] = ax * ax + ay * ay;
            }
        } else
            ok = 0;
        if (ok) {
            for (j = m = 0; j < n; j++) {         /* the candidates, in creation order */
                int16_t v = ck[j];
                if (cl[j] > mu) continue;
                for (k = m; k > 0 && pw_seq[ck[k - 1]] > pw_seq[v]; k--) ck[k] = ck[k - 1];
                ck[k] = v;
                m++;
            }
            for (j = 0; j < m; j++) {
                double dx, dy, d;
                k = ck[j];
                dx = px - PTOD(PW.in[k].x);
                dy = py - PTOD(PW.in[k].y);
                d = dx * dx + dy * dy;
                if (best == NOONE || d < bd) {
                    best = k;
                    bd = d;
                }
            }
#ifdef PLAY_STATS
            {   /* the host builds compare with the double loop */
                int b2 = NOONE;
                double bd2 = 0;
                fam_begin(&it, obj);
                while ((k = fam_get(&it)) != NOONE) {
                    double dx = px - PTOD(PW.in[k].x), dy = py - PTOD(PW.in[k].y), d = dx * dx + dy * dy;
                    if (b2 == NOONE || d < bd2) { b2 = k; bd2 = d; }
                }
                if (b2 != best) { fprintf(stderr, "instance_nearest_p: %d, the double loop %d\n", best, b2); abort(); }
            }
#endif
            return best;
        }
    }
    fam_begin(&it, obj);
    while ((k = fam_get(&it)) != NOONE) {
        double dx, dy, d;
        dx = px - PTOD(PW.in[k].x);
        dy = py - PTOD(PW.in[k].y);
        d = dx * dx + dy * dy;
        if (best == NOONE || d < bd) {
            best = k;
            bd = d;
        }
    }
    return best;
}

/* 0 when no alive instance of the family has floor(x) in [x0, x1] and floor(y) in [y0, y1] (nc_get's floors); 1 when
   one has, or may (the family does not fit the cache). No side effect besides the cache (the speartrap
   step: an instance_nearest whose answer cannot pass the tests that follow is not computed) */
int instance_box_maybe(int obj, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    const struct ncache *c;
    int j;
    if (fam_none(obj)) return 0;
    if (obj < 0) return 1;
    c = nc_get(obj);
    if (c->ok != 1) return 1;
    for (j = 0; j < c->n; j++)
        if (c->x[j] >= x0 && c->x[j] <= x1 && c->y[j] >= y0 && c->y[j] <= y1) return 1;
    return 0;
}

int instance_first_p(int obj)
{
    struct fam it;
    int e, k;
    PWST(exists, 1);
    if (fam_none(obj)) return NOONE;
    for (e = 0; e < 4; e++)
        if (ifc_obj[e] == obj && ifc_tag[e] == olive_gen + 1) {
#ifdef PLAY_STATS
            fam_begin(&it, obj);
            if (fam_get(&it) != ifc_val[e]) {
                fprintf(stderr, "instance_first_p: cached %d differs (%d)\n", ifc_val[e], obj);
                abort();
            }
#endif
            return ifc_val[e];
        }
    fam_begin(&it, obj);
    k = fam_get(&it);
    e = ifc_next++ & 3;
    ifc_obj[e] = (int16_t)obj;
    ifc_val[e] = (int16_t)k;
    ifc_tag[e] = olive_gen + 1;
    return k;
}

int instance_exists_p(int obj)
{
    PWST(exists, 1);
    if (obj < 0) return instance_first_p(obj) != NOONE;
    return olive[obj] > 0;
}

int instance_number_p(int obj)
{
    struct fam it;
    int n = 0;
    PWST(exists, 1);
    if (obj >= 0) return olive[obj];
    fam_begin(&it, obj);
    while (fam_get(&it) != NOONE) n++;
    return n;
}

/* pdist_newton(n) for a whole n <= 2^24: n = r^2 gives r (r = isqrt(n)), else the same loop from r + 0.5 ends on the
   same bits (checked for every n in 0 .. 2^24); it runs 3.2 times on average, from d 13.1 (n 1 .. 70580) */
static double dist_newton_i(uint32_t n)
{
    uint32_t r = 0, b = 1u << 24, m = n;
    double d = n, s, prev = 0;
    int it;
    while (b > m) b >>= 2;
    while (b) {
        if (m >= r + b) { m -= r + b; r = (r >> 1) + b; }
        else r >>= 1;
        b >>= 2;
    }
    if (r * r == n) return r;
    s = (double)r + 0.5;
    for (it = 0; it < 64 && s != prev; it++) { prev = s; s = 0.5 * (s + d / s); }
    return s;
}

/* DLT(psqrt(d2), c) for d2 = dx * dx + dy * dy >= 0 (point_distance_d's sum: +0 or above, never NaN for finite
   positions). psqrt is correctly rounded, so non-decreasing in d2, and gcmp_dd(s, c) < 0 (s - c rounded, below
   -eps) is non-decreasing in s's falseness: the compare is true exactly for d2 below a threshold T(c). T is the
   least d2 (as bits: a non-negative double's bits order as its value) where the compare is false, found once per c
   by bisection with psqrt itself (between 1 and 4 (c + 1)^2 when the compare is true and false there), and kept
   (dthr). The host builds compare every answer with
   DLT(psqrt(d2), c) */
#define DTHR_N 16
static struct { uint64_t cb, t; } dthr[DTHR_N];
static uint8_t ndthr, dthr_next;
static uint64_t dthr_bits(double d) { union { double d; uint64_t u; } v; v.d = d; return v.u; }
static double dthr_dbl(uint64_t u) { union { double d; uint64_t u; } v; v.u = u; return v.d; }
static uint64_t dthr_get(double c)                /* T(c)'s bits (found and kept at the first use of c) */
{
    uint64_t cb = dthr_bits(c), lo, hi;
    int k;
    for (k = 0; k < ndthr && dthr[k].cb != cb; k++) {}
    if (k == ndthr) {
        /* bounds that keep psqrt on its fast range: below lo every d2 compares true (lo's does), at hi false */
        double h = 4 * (c + 1) * (c + 1);
        lo = DLT(psqrt(1.0), c) ? dthr_bits(1.0) : 0;
        hi = !DLT(psqrt(h), c) ? dthr_bits(h) : 0x7ff0000000000000ull;   /* (+inf: false) */
        if (lo == 0 && !DLT(psqrt(0), c)) hi = 0;   /* DLT(0, c) false: c <= eps */
        while (lo < hi) {                        /* the least u in [lo, hi] with the compare false (hi is one) */
            uint64_t mid = lo + (hi - lo) / 2;
            if (DLT(psqrt(dthr_dbl(mid)), c)) lo = mid + 1;
            else hi = mid;
        }
        if (ndthr < DTHR_N) k = ndthr++;
        else { k = dthr_next; dthr_next = (uint8_t)((dthr_next + 1) & (DTHR_N - 1)); }
        dthr[k].cb = cb;
        dthr[k].t = lo;
    }
    return dthr[k].t;
}

int pdist2_lt(double d2, double c)
{
    int r = dthr_bits(d2) < dthr_get(c);
#ifdef PLAY_STATS
    if (dthr_bits(d2) >> 63 || d2 != d2 || r != DLT(psqrt(d2), c)) {
        fprintf(stderr, "pdist2_lt: %.17g against %.17g: %d\n", d2, c, r);
        abort();
    }
#endif
    return r;
}

/* a float as an int in units of 2^-16 when it is one exactly and |f| < 2^14 */
static int pfix16(float f, int64_t *o)
{
    union { float f; uint32_t u; } v;
    uint32_t e, m, sh;
    int32_t a;
    v.f = f;
    if ((v.u & 0x7fffffffu) == 0) { *o = 0; return 1; }
    e = (v.u >> 23) & 0xffu;
    if (e == 0 || e > 140) return 0;
    m = (v.u & 0x7fffffu) | 0x800000u;
    if (e >= 134) a = (int32_t)(m << (e - 134));
    else {
        sh = 134 - e;
        if (sh > 23 || (m & ((1u << sh) - 1))) return 0;
        a = (int32_t)(m >> sh);
    }
    *o = (v.u >> 31) ? -a : a;
    return 1;
}

/* pdist2_lt(pdist2(PTOD(x1) + ox, PTOD(y1) + oy, PTOD(x2), PTOD(y2)), c) (point_distance from a position plus whole
   offsets to a position, against c). With the four floats exact in 2^-16 units (|v| < 2^14), dx, dy are exact ints
   (|.| < 2^31) and S = dx^2 + dy^2 exact (units 2^-32); pdist2's d2 = fl(fl(dx^2) + fl(dy^2)) is within a relative
   2^-51.9 of S, so below m = (S >> 50) + 2 units of it. d2 < T(c) (pdist2_lt's threshold, Tf = floor(T 2^32)) is then
   true when S + m <= Tf, false when S >= Tf + 1 + m; between them, and in every other case, the doubles. The host
   builds compare every integer answer with the double one */
int pdist_lt_at(pos x1, pos y1, int32_t ox, int32_t oy, pos x2, pos y2, double c)
{
    int64_t a, b, u, w;
    if (pfix16(x1, &a) && pfix16(y1, &b) && pfix16(x2, &u) && pfix16(y2, &w)) {
        int64_t dx = u - (a + (int64_t)ox * 65536), dy = w - (b + (int64_t)oy * 65536);   /* (ox, oy < 0: no shift) */
        uint64_t t = dthr_get(c), e = t >> 52, tf, S, m;
        int r = -1;
        if (dx > -0x7fffffffLL && dx < 0x7fffffffLL && dy > -0x7fffffffLL && dy < 0x7fffffffLL && e >= 1 &&
            e <= 1052) {
            uint64_t mt = (t & 0xfffffffffffffull) | (1ull << 52);
            tf = e >= 1043 ? mt << (e - 1043) : (1043 - e >= 64 ? 0 : mt >> (1043 - e));
            S = (uint64_t)((int64_t)(int32_t)dx * (int32_t)dx) + (uint64_t)((int64_t)(int32_t)dy * (int32_t)dy);
            m = (S >> 50) + 2;
            if (S + m <= tf) r = 1;
            else if (S >= tf + 1 + m) r = 0;
        }
        if (r >= 0) {
#ifdef PLAY_STATS
            if (r != pdist2_lt(pdist2(PTOD(x1) + ox, PTOD(y1) + oy, PTOD(x2), PTOD(y2)), c)) {
                fprintf(stderr, "pdist_lt_at: %d differs (%.9g %.9g %d %d %.9g %.9g %g)\n", r, x1, y1, (int)ox, (int)oy,
                        x2, y2, c);
                abort();
            }
#endif
            return r;
        }
    }
    return pdist2_lt(pdist2(PTOD(x1) + ox, PTOD(y1) + oy, PTOD(x2), PTOD(y2)), c);
}

/* the thresholds of the constants the Steps compare with (a first use mid-step would bisect there: about 63 psqrt
   calls): taken at the first level start */
static void pdist_warm(void)
{
    static const double c[] = { 4, 64, 90, 96, 160, 240 };
    static uint8_t done;
    unsigned k;
    if (done) return;
    done = 1;
    for (k = 0; k < sizeof c / sizeof c[0]; k++) (void)pdist2_lt(0, c[k]);
}

double distance_to_instance_p(int self, int k)
{
    double sl, st, sr, sb, l, t, r, b, xd = 0, yd = 0, d;
    int32_t ia[4], ic[4];
    if (pin_ibox(self, ia) && pin_ibox(k, ic)) {   /* whole boxes: separated on one axis, d = n^2 and the result n */
        int32_t ixd = 0, iyd = 0;
        if (ic[0] > ia[2]) ixd = ic[0] - ia[2];
        if (ic[2] < ia[0]) ixd = ic[2] - ia[0];
        if (ic[1] > ia[3]) iyd = ic[1] - ia[3];
        if (ic[3] < ia[1]) iyd = ic[3] - ia[1];
        if (ixd == 0) return iyd < 0 ? -iyd : iyd;
        if (iyd == 0) return ixd < 0 ? -ixd : ixd;
        if (ixd < 0) ixd = -ixd;
        if (iyd < 0) iyd = -iyd;
        if (ixd <= 2895 && iyd <= 2895) return dist_newton_i((uint32_t)(ixd * ixd + iyd * iyd));
        xd = ixd; yd = iyd;
        return pdist_newton(xd * xd + yd * yd);
    }
    if (!pin_bbox(self, &sl, &st, &sr, &sb))
        sl = sr = PTOD(PW.in[self].x), st = sb = PTOD(PW.in[self].y);
    if (!pin_bbox(k, &l, &t, &r, &b))
        l = r = PTOD(PW.in[k].x), t = b = PTOD(PW.in[k].y);
    if (l > sr) xd = l - sr;
    if (r < sl) xd = r - sl;
    if (t > sb) yd = t - sb;
    if (b < st) yd = b - st;
    d = xd * xd + yd * yd;
    return pdist_newton(d);                         /* sqrt by Newton (no libm on the SH-2) */
}

double distance_to_object_p(int self, int obj)
{
    int k;
    double best = 1000000;
    struct fam it;
    PWST(dist, 1);
    pcol_touch(self);                                /* F_DistanceToObject computes the boxes */
    if (fam_none(obj)) return best;
    fam_begin(&it, obj);
    while ((k = fam_get(&it)) != NOONE) {
        double d;
        pcol_touch(k);
        d = distance_to_instance_p(self, k);
        if (d < best) best = d;
    }
    return best;
}

/* distance_to_object_p(self, obj)'s writes without the distance: the box touches (pcol_touch of self, then of each
   instance of obj's family in its creation order) and the counter; for a caller that does not read the distance */
void pw_touch_object(int self, int obj)
{
    int k;
    struct fam it;
    PWST(dist, 1);
    pcol_touch(self);
    if (fam_none(obj)) return;
    fam_begin(&it, obj);
    while ((k = fam_get(&it)) != NOONE) pcol_touch(k);
}

int pw_with(int obj, int16_t *out, int max)
{
    int k, n = 0, m = 0, j;
    struct fam it;
    static int16_t all[PIN_MAX];
    PWST(with, 1);
    if (fam_none(obj)) return 0;
    fam_begin(&it, obj);                             /* newest first: the last max of the creation order */
    while ((k = fam_get(&it)) != NOONE) all[m++] = (int16_t)k;
    for (j = m - 1; j >= 0 && n < max; j--) out[n++] = all[j];
    if (n == 2) {
        int16_t t = out[0];
        out[0] = out[1];
        out[1] = t;
    }
    return n;
}

/* tools/colprobe.py (test/host/colprobe.c): the instance-level tests against one instance k (Collision_Point /
   Rectangle / Line of instance k, Collision_Instance of a and b) */
int pw_test_point(int k, double px, double py, int prec)
{
    struct pq q;
    pq_init(&q, px, py);
    return point_hit(k, &q, prec);
}

int pw_test_rect(int k, double x1, double y1, double x2, double y2, int prec)
{
    struct rq q;
    rq_init(&q, x1, y1, x2, y2);
    return rect_hit(k, &q, prec);
}

int pw_test_line(int k, double x1, double y1, double x2, double y2, int prec)
{
    struct qctx c;
    lq_init(&c.lq, x1, y1, x2, y2);
    c.prec = prec;
    c.x1 = x1; c.y1 = y1; c.x2 = x2; c.y2 = y2; c.dbl = 1;
    return line_hit(k, &c);
}

/* pw_test_line with whole ends, |v| < 30000 (lq_init's whole numbers: the same integer query; line_hit_f makes the
   doubles from the ints when it needs them) */
int pw_test_line_i(int k, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int prec)
{
    struct qctx c;
    c.lq.iok = 1;
    c.lq.lx = x1 < x2 ? x1 : x2; c.lq.hx = x1 < x2 ? x2 : x1;
    c.lq.ly = y1 < y2 ? y1 : y2; c.lq.hy = y1 < y2 ? y2 : y1;
    c.lq.axis = x1 == x2 || y1 == y2;
    c.prec = prec;
    c.ix1 = x1; c.iy1 = y1; c.ix2 = x2; c.iy2 = y2; c.dbl = 0;
    return line_hit(k, &c);
}

int pw_test_pair(int a, int b) { return overlap_at(a, 0, 0, b); }
