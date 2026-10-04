/* Play world: instances and GameMaker 2024.14's collision functions (rules and evidence: play.h).
 * Searches return the oldest matching instance (P2: collision_point, instance_place, instance_find, obj.var). */
#include "play.h"
#include "pint.h"                 /* PL (pw_release) */
#include "pcol.h"
#include "inst.h"                 /* GRID_W, GRID_H: the solid grid covers the generator's level grid */

/* the runner's float arithmetic as written (the precise collision code below follows its instruction order) */
#ifdef __clang__
#pragma STDC FP_CONTRACT OFF
#endif

/* the generator's instances, then the play instances in the same memory (play.h struct pworld) */
#define INST_MEM_N (PIN_MAX > INST_MAX ? PIN_MAX : INST_MAX)
typedef char pin_size_is_inst_size[sizeof(struct pin) == sizeof(struct inst) ? 1 : -1];
struct inst inst_mem[INST_MEM_N];
struct pworld PW = { .in = (struct pin *)inst_mem };
#ifdef PLAY_STATS
struct pw_stats pw_st;
#endif

static int spr_of(const struct pin *p) { return p->mask >= 0 ? p->mask : p->spr; }
static void grid_reset(void);
static void grid_unlink(int i);
static void grid_dirty(int i);
static void grid_flush(void);
static int16_t ghead[GRID_H][GRID_W];       /* the oSolid family (point queries) */
static int16_t thead[GRID_H][GRID_W];       /* the other terrain (the drawing only) */
static int tmaxw, tmaxh;
static int16_t gnext[PIN_MAX], gcell[PIN_MAX], gdnext[PIN_MAX];
static uint8_t gond[PIN_MAX];
static int16_t gdhead = NOONE;
static int gmaxw, gmaxh;

/* ---- per-object instance lists: the alive instances of each object in creation order (index order), and the
   alive count of each object with its descendants. Linked at pin_add, unlinked when alive goes to 0 ------------ */
int16_t pw_ohead[OBJ_COUNT], pw_inext[PIN_MAX];
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
static int nfree, nrmq;
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
static int16_t olive[OBJ_COUNT];
#define odesc0 obj_desc0
#define odesc obj_desc

static void olists_reset(void)
{
    int o;
    obj_desc_init();
    for (o = 0; o < OBJ_COUNT; o++) {
        pw_ohead[o] = otail[o] = NOONE;
        olive[o] = 0;
    }
    pw_ahead = pw_atail = NOONE;
    pw_nthead = pw_nttail = NOONE;
    grid_reset();
}

static void olive_add(int obj, int d)
{
    int a;
    for (a = obj; a >= 0; a = objdefs[a].parent) olive[a] = (int16_t)(olive[a] + d);
}

static void olink(int i)
{
    int o = PW.in[i].obj;
    pw_inext[i] = NOONE;
    iprev[i] = otail[o];
    if (otail[o] >= 0) pw_inext[otail[o]] = (int16_t)i; else pw_ohead[o] = (int16_t)i;
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

static void ounlink(int i)
{
    int o = PW.in[i].obj;
    if (iprev[i] >= 0) pw_inext[iprev[i]] = pw_inext[i]; else pw_ohead[o] = pw_inext[i];
    if (pw_inext[i] >= 0) iprev[pw_inext[i]] = iprev[i]; else otail[o] = iprev[i];
    olive_add(o, -1);
    grid_unlink(i);
    if (aprev[i] >= 0) pw_anext[aprev[i]] = pw_anext[i]; else pw_ahead = pw_anext[i];
    if (pw_anext[i] >= 0) aprev[pw_anext[i]] = aprev[i]; else pw_atail = aprev[i];
    if (pin_needs_ext(o)) {
        if (ntprev[i] >= 0) pw_ntnext[ntprev[i]] = pw_ntnext[i]; else pw_nthead = pw_ntnext[i];
        if (pw_ntnext[i] >= 0) ntprev[pw_ntnext[i]] = ntprev[i]; else pw_nttail = ntprev[i];
    }
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
    for (j = odesc0[obj]; j < odesc0[obj + 1]; j++) {
        int h = pw_ohead[odesc[j]];
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

void pw_draw_mark(int i)
{
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
void pw_changed(int i)
{
    pw_draw_mark(i);
    PW.in[i].bbk = 0;
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
    "oCaveBG", "oCaveBG2", "oCaveBGEntrance", "oBackdrop", "oForeground"
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
    for (k = EXT_MAX - 1; k >= 1; k--) extfree[nextfree++] = (int16_t)k;
    ext_used = 0;
    en_zero(&pin_en[0]);
    nenfree = 0;
    for (k = EN_MAX - 1; k >= 1; k--) enfree[nenfree++] = (int16_t)k;
    en_used = 0;
}

static int ext_alloc(void)
{
    int e;
    if (nextfree == 0) {
        PUNTR(9005);
        return 0;
    }
    e = extfree[--nextfree];
    ext_defaults(&pin_ext[e]);
    if (++ext_used > ext_used_max) ext_used_max = ext_used;
    return e;
}

int pw_ext_used_max(void) { return ext_used_max; }

static int en_alloc(void)
{
    int e;
    if (nenfree == 0) {
        PUNTR(9008);
        return 0;
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
    if (p->ext > 0) {
        if (pin_ext[p->ext].en > 0) {
            enfree[nenfree++] = pin_ext[p->ext].en;
            en_used--;
        }
        extfree[nextfree++] = p->ext;
        ext_used--;
    }
#ifdef PIN_EXT_CHECK
    p->ext = -1;                                     /* a later PE(p) is an error */
#else
    p->ext = 0;
#endif
}

/* a reference kept across steps: to PIN_DEAD when its slot goes back */
#define REL(r) do { if ((r) >= 0 && relmark[r]) (r) = PIN_DEAD; } while (0)

/* the end of a step: the slots of the instances RemoveMarked removed go back on the free list. Kept across steps
   (pint.h, play.h): oPlayer1's idx / ladder / holdItem, the instance variables trapID, enemyID, bombID of the
   alive instances; the per-slot state of pcol.c went in RemoveMarked, the grid's dirty list is flushed here */
void pw_release(void)
{
    int k, j, s, flush = 0;
    /* in batches (the compaction of pw_ord and the sweep cost about PW.nord): 64 removed, or the unused slots and
       the free ones close to running out (a step creates fewer than PW_RELEASE_ROOM) */
    if (nrmq < PW_RELEASE_BATCH && nfree + (PIN_DEAD - PW.n) >= PW_RELEASE_ROOM) return;
    if (nrmq == 0) return;
    if (!dead_ok) {
        struct pin *d = &PW.in[PIN_DEAD];
        unsigned char *b = (unsigned char *)d;
        unsigned k2;
        for (k2 = 0; k2 < sizeof *d; k2++) b[k2] = 0;
        PIN_WR(int16_t, d->spr) = -1;
        PIN_WR(int16_t, d->mask) = -1;
#ifdef PIN_EXT_CHECK
        d->ext = -1;                                 /* PE(PIN_DEAD) is an error */
#endif
        dead_ok = 1;
    }
    for (k = 0; k < nrmq; k++) {
        s = rmq(k);
        relmark[s] = 1;
        flush |= gond[s];
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
    for (k = j = 0; k < PW.nord; k++)
        if (!relmark[pw_ord[k]]) pw_ord[j++] = pw_ord[k];
    PW.nord = (int16_t)j;
    if (PW.seq > PW_SEQ_RENUM) {                     /* creation numbers from 0 again, in the same order */
        for (k = 0; k < PW.nord; k++) pw_seq[pw_ord[k]] = (int16_t)k;
        PW.seq = PW.nord;
    }
    for (k = 0; k < nrmq; k++) {
        s = rmq(k);
        relmark[s] = 0;
        freel[nfree++] = (int16_t)s;
#ifdef PIN_EXT_CHECK
        {   /* the check build: a free slot read through an index kept elsewhere shows in the output */
            struct pin *d = &PW.in[s];
            PIN_WR(pos, d->x) = PIN_WR(pos, d->y) = (pos)PI(8000);
            d->id = -7777;
        }
#endif
    }
    nrmq = 0;
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
    nfree = nrmq = 0;
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
        if (PW.n >= PIN_DEAD) {
            PUNTR(9001);
            return PIN_DEAD;
        }
        i = PW.n++;
    }
    pw_seq[i] = PW.seq++;
    pw_ord[PW.nord++] = (int16_t)i;
    p = &PW.in[i];
    {   /* zero every field */
        unsigned char *b = (unsigned char *)p;
        unsigned k2;
        for (k2 = 0; k2 < sizeof *p; k2++) b[k2] = 0;
    }
    p->id = id;
    p->obj = (int16_t)obj;
    PIN_WR(int16_t, p->spr) = gobjspr[obj];         /* a new instance: pcol_added takes it as it is */
    PIN_WR(int16_t, p->mask) = -1;
    p->alive = 1;
    PIN_WR(uint8_t, p->visible) = pobj[obj].visible;
    p->persistent = pobj[obj].persistent;
    PIN_WR(pos, p->x) = x;
    PIN_WR(pos, p->y) = y;
    p->xprev = x;
    p->yprev = y;
    PIN_WR(float, p->depth) = objdefs[obj].depth;
    PIN_WR(img_t, p->img) = 0;
    p->ispd = 1;
    PIN_WR(float, p->xscale) = PIN_WR(float, p->yscale) = 1;
    PIN_WR(float, p->angle) = 0;
    p->ext = (int16_t)(pin_needs_ext(obj) ? ext_alloc() : 0);   /* with pin_add's defaults (ext_defaults) */
    if (p->ext && pin_needs_en(obj)) pin_ext[p->ext].en = (int16_t)en_alloc();
    pw_draw_mark(i);                                 /* (a reused slot may still be on the list: marked once) */
    (void)k;
    olink(i);
    gcell[i] = NOONE;
    gond[i] = 0;
    grid_dirty(i);
    pcol_added(i);
    return i;
}

int pin_create(pos x, pos y, int obj)
{
    int i = pin_add(obj, x, y, PW.next_id++);
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
    pw_draw_mark(i);
    ev_destroy(i);
    pcol_destroyed(i);           /* in the collision tree until the next RemoveMarked */
}

void pin_kill(int i)
{
    if (i >= 0) {
        if (PW.in[i].alive) { ounlink(i); pw_draw_mark(i); }
        PW.in[i].alive = 0;
        pcol_destroyed(i);
    }
}

/* sprite_index = spr: image_index is kept unless it is past the new sprite's frames, then 0 (Observed in
   build/trace/p4_walk_s1: sRunLeft at image 4.668 -> sFallLeft (1 frame) 0.0 in record 60; sFallLeft at 0.4 ->
   sRunLeft (6 frames) 0.4 in record 66) */
void pin_set_sprite(int i, int spr)
{
    struct pin *p = &PW.in[i];
    if (p->spr != spr) {
        PIN_WR(int16_t, p->spr) = (int16_t)spr;
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

/* v as an int in (-30000, 30000) when it is a whole number */
static int pos_int(pos v, int32_t *o)
{
#ifdef PLAY_FIXED
    if ((v & ((1 << PFRAC_BITS) - 1)) != 0) return 0;
    *o = v >> PFRAC_BITS;
    return *o > -30000 && *o < 30000;
#else
    return fwhole(v, o) && *o > -30000 && *o < 30000;
#endif
}

static void bbox_dbl(const struct pin *p, const struct gsprcol *c, double *l, double *t, double *r, double *b)
{
    double xs = p->xscale, ys = p->yscale, x = PTOD(p->x), y = PTOD(p->y);
    PWST(bbox, 1);
    if (!dzero(p->angle)) {                       /* rotated: the box of the rotated sprite (pcol.c ebbox) */
        float o[4];
        pcol_box((int)(p - PW.in), o);
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

/* 1 / -1 when d is exactly 1.0 / -1.0, else 0 (bits) */
static int dunit(double d)
{
    union { double d; uint64_t u; } v;
    v.d = d;
    if (v.u == 0x3ff0000000000000ull) return 1;
    if (v.u == 0xbff0000000000000ull) return -1;
    return 0;
}

/* the cache kind of instance i's box (computed when a setter cleared it): with scales of exactly +-1 and whole x, y
   the double formula's results are the integers below */
static int bbkind(int i)
{
    struct pin *p = &PW.in[i];
    if (p->bbk == 0) {
        int s = spr_of(p);
        int32_t x, y;
        if (s < 0)
            p->bbk = BB_NOSPR;
        else {
            const struct gsprcol *c = &gsprcol[s];
            int xs = dunit(p->xscale), ys = dunit(p->yscale);
            if (xs && ys && dzero(p->angle) && pos_int(p->x, &x) && pos_int(p->y, &y)) {
                int32_t l = xs > 0 ? x + (c->l - c->xo) : x - (c->r + 1 - c->xo);
                int32_t t = ys > 0 ? y + (c->t - c->yo) : y - (c->b + 1 - c->yo);
                PWST(bbox_int, 1);
                p->bl = (int16_t)l;
                p->br = (int16_t)(l + (c->r - c->l + 1));
                p->bt = (int16_t)t;
                p->bb = (int16_t)(t + (c->b - c->t + 1));
                p->bbk = BB_INT;
            } else
                p->bbk = BB_DBL;
        }
    }
    return p->bbk;
}

int pin_bbox(int i, double *l, double *t, double *r, double *b)
{
    const struct pin *p = &PW.in[i];
    switch (bbkind(i)) {
    case BB_NOSPR:
        return 0;
    case BB_INT:
        *l = p->bl; *t = p->bt; *r = p->br; *b = p->bb;
        return 1;
    default:
        bbox_dbl(p, &gsprcol[spr_of(p)], l, t, r, b);
        return 1;
    }
}

/* the box as integers when it is cached so (BB_INT); 0 otherwise (no sprite, or not whole: use pin_bbox) */
int pin_ibox(int i, int32_t *b)
{
    const struct pin *p = &PW.in[i];
    if (bbkind(i) != BB_INT) return 0;
    b[0] = p->bl; b[1] = p->bt; b[2] = p->br; b[3] = p->bb;
    return 1;
}

/* x and y as ints when both are whole numbers (|v| < 30000) */
int pin_xy_int(int i, int32_t *x, int32_t *y)
{
    return pos_int(PW.in[i].x, x) && pos_int(PW.in[i].y, y);
}

int pin_box_outside(int i, int w, int h)
{
    int32_t q[4];
    double l, t, r, b;
    if (pin_ibox(i, q)) return q[2] < 0 || q[0] > w || q[3] < 0 || q[1] > h;
    if (!pin_bbox(i, &l, &t, &r, &b)) return 0;
    return r < 0 || l > w || b < 0 || t > h;
}

/* floor(v) as an int when |v| < 30000 */
static int dfloor_int(double v, int32_t *o)
{
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

static int pcinst_of(int i, double dx, double dy, struct pcinst *q)
{
    const struct pin *p = &PW.in[i];
    int s = spr_of(p);
    const struct gsprcol *c;
    const struct psprite *ps;
    double l, t, r, b;
    if (s < 0 || !pin_bbox(i, &l, &t, &r, &b)) return 0;
    c = &gsprcol[s];
    ps = &psprite[s];
    q->x = (float)(PTOD(p->x) + dx); q->y = (float)(PTOD(p->y) + dy);
    q->xs = p->xscale; q->ys = p->yscale; q->ang = p->angle;
    q->bl = (float)(l + dx); q->bt = (float)(t + dy); q->br = (float)(r + dx); q->bb = (float)(b + dy);
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
    ixA = 1.0f / A->xs; ixB = 1.0f / B->xs; iyA = 1.0f / A->ys; iyB = 1.0f / B->ys;
    rA = rotated_eps(A->ang);
    rB = rotated_eps(B->ang);
    if (!rA && !rB) {
        float lxA, lxB;
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
struct pq { double px, py; int32_t ix, iy; int iok; };

static void pq_init(struct pq *q, double px, double py)
{
    px = (float)px;                                  /* CInstance::Collision_Point takes floats */
    py = (float)py;
    q->px = px;
    q->py = py;
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
        if (!pin_bbox(k, &l, &t, &r, &b))
            return 0;
        if (!(q->px >= l && q->px < r && q->py >= t && q->py < b))
            return 0;
    }
    return !prec || !precise(k) || precise_point(k, (float)q->px, (float)q->py);
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
        for (x = 0; x < GRID_W; x++) ghead[y][x] = thead[y][x] = NOONE;
    gdhead = NOONE;
    gmaxw = gmaxh = tmaxw = tmaxh = 1;
}

static void grid_unlink(int i)
{
    int c = gcell[i];
    int16_t *pp;
    if (c < 0) return;
    pp = c >= GRID_W * GRID_H ? &thead[(c - GRID_W * GRID_H) / GRID_W][(c - GRID_W * GRID_H) % GRID_W]
                              : &ghead[c / GRID_W][c % GRID_W];
    for (; *pp != i; pp = &gnext[*pp]) {}
    *pp = gnext[i];
    gcell[i] = NOONE;
}

/* a solid's box may have changed (or it was added): placed again at the next query */
static void grid_dirty(int i)
{
    if (gond[i] || !(obj_is(PW.in[i].obj, OBJ_oSolid) || !pin_needs_ext(PW.in[i].obj))) return;
    gond[i] = 1;
    gdnext[i] = gdhead;
    gdhead = (int16_t)i;
}

static void grid_flush(void)
{
    while (gdhead >= 0) {
        int i = gdhead, cx, cy, w, h, solid;
        int32_t ib[4];
        double l, t, r, b;
        gdhead = gdnext[i];
        gond[i] = 0;
        grid_unlink(i);
        if (!PW.in[i].alive) continue;
        if (pin_ibox(i, ib)) {
            cx = ib[0] >> 4; cy = ib[1] >> 4;
            w = ((ib[2] - ib[0]) >> 4) + 1; h = ((ib[3] - ib[1]) >> 4) + 1;
        } else if (pin_bbox(i, &l, &t, &r, &b) && l > -30000 && l < 30000 && t > -30000 && t < 30000 &&
                   r - l < 30000 && b - t < 30000) {
            cx = dfloor(l) >> 4; cy = dfloor(t) >> 4;
            w = (dfloor(r - l) >> 4) + 2; h = (dfloor(b - t) >> 4) + 2;
        } else
            continue;                             /* no sprite: never hit */
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

/* Command_CollisionPoint tests the object's instances in creation order (Collision_Point computes each stale box:
   pcol_touch) */
int (collision_point_p)(double px, double py, int obj, int prec, int notme_self)
{
    int k;
    struct pq q;
    struct fam it;
    PWST(point, 1);
    if (fam_none(obj)) return NOONE;
    pq_init(&q, px, py);
    if (q.iok && obj >= 0 && obj_is(obj, OBJ_oSolid) && !pcol_quiet()) {
        k = grid_point(obj, notme_self, &q, prec);
        pcol_touch_stale(obj, notme_self, k);
        return k;
    }
    fam_begin(&it, obj);
    while ((k = fam_get(&it)) != NOONE) {
        if (k == notme_self) continue;
        pcol_touch(k);
        if (point_hit(k, &q, prec))
            return k;
    }
    return NOONE;
}

/* a line query with whole-number ends: their bounding box, and whether the line is axis-aligned */
struct lq { int iok, axis; int32_t lx, ly, hx, hy; };

static int whole(double v, int32_t *o)
{
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

static int line_hit(int k, struct qctx *c)
{
    double l, t, r, b, x1, y1, x2, y2;
    const struct lq *q = &c->lq;
    int prec = c->prec;
    int32_t ib[4];
    if (q->iok && pin_ibox(k, ib)) {
        if (q->hx < ib[0] || q->lx >= ib[2] || q->hy < ib[1] || q->ly >= ib[3])
            return 0;
        if (q->axis && (!prec || !precise(k)))
            return 1;
    }
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
    if (q == 1) qrect(x1, y1, x2, y2, r);
    return line_run(&c, q, r);
}

/* collision_line with whole-number ends (|v| < 30000): the same without the double conversions */
int collision_line_i(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int obj, int prec, int notme_self)
{
    int q = pcol_query(obj);
    struct qctx c;
    float r[4];
    PWST(line, 1);
    if (q < 0) return NOONE;
    c.lq.iok = 1;
    c.lq.lx = x1 < x2 ? x1 : x2; c.lq.hx = x1 < x2 ? x2 : x1;
    c.lq.ly = y1 < y2 ? y1 : y2; c.lq.hy = y1 < y2 ? y2 : y1;
    c.lq.axis = x1 == x2 || y1 == y2;
    c.obj = obj; c.notme = notme_self; c.prec = prec;
    c.ix1 = x1; c.iy1 = y1; c.ix2 = x2; c.iy2 = y2; c.dbl = 0;
    (void)r;
    return line_run(&c, q, 0);                  /* qrect: (float)min - 1.0f is these exactly */
}

/* a rectangle query: its sides rounded (floor(v + 0.5)) once */
/* f*: the corners as the runner's floats (CInstance::Collision_Rectangle takes floats) */
struct rq { double lx, hx, ly, hy; int32_t ilx, ihx, ily, ihy; int iok; float flx, fhx, fly, fhy; };

static void rq_init(struct rq *q, double x1, double y1, double x2, double y2)
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

static int rect_hit(int k, const struct rq *q, int prec)
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

int (collision_rect_p)(double x1, double y1, double x2, double y2, int obj, int prec, int notme_self)
{
    int q = pcol_query(obj);
    struct rq rq;
    float r[4];
    PWST(rect, 1);
    if (q < 0) return NOONE;
    rq_init(&rq, x1, y1, x2, y2);
    if (q == 1) qrect(x1, y1, x2, y2, r);
    return rect_run(&rq, q, r, obj, prec, notme_self);
}

/* collision_rectangle with whole-number corners (|v| < 30000): floor(v + 0.5) is v */
int collision_rect_i(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int obj, int prec, int notme_self)
{
    int q = pcol_query(obj);
    struct rq rq;
    float r[4];
    PWST(rect, 1);
    if (q < 0) return NOONE;
    rq.iok = 1;
    rq.ilx = x1 < x2 ? x1 : x2; rq.ihx = x1 < x2 ? x2 : x1;
    rq.ily = y1 < y2 ? y1 : y2; rq.ihy = y1 < y2 ? y2 : y1;
    rq.lx = rq.ilx; rq.hx = rq.ihx; rq.ly = rq.ily; rq.hy = rq.ihy;
    (void)r;
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

static void pci_of(int i, int32_t dx, int32_t dy, struct pci *q)
{
    const struct pin *p = &PW.in[i];
    int s = spr_of(p);
    const struct gsprcol *c = &gsprcol[s];
    const struct psprite *ps = &psprite[s];
    pos_int(p->x, &q->x);
    pos_int(p->y, &q->y);
    q->x += dx; q->y += dy;
    q->sx = p->xscale > 0 ? 1 : -1;
    q->sy = p->yscale > 0 ? 1 : -1;
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

static int precise_collision_int(int a, int32_t dx, int32_t dy, const int32_t *ia, int b, const int32_t *ib)
{
    struct pci A, B;
    int32_t x0, x1, y0, y1, c, r;
    pci_of(a, dx, dy, &A);
    pci_of(b, 0, 0, &B);
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

/* instance a (its bbox moved by dx, dy) against instance b */
static int overlap_at(int a, double dx, double dy, int b)
{
    double l, t, r, bb, l2, t2, r2, b2;
    int32_t ia[4], ib[4], idx, idy;
    if (pin_ibox(a, ia) && pin_ibox(b, ib) && whole(dx, &idx) && whole(dy, &idy)) {
        if (!(ia[0] + idx < ib[2] && ib[0] < ia[2] + idx && ia[1] + idy < ib[3] && ib[1] < ia[3] + idy))
            return 0;
        if (!precise(a) && !precise(b))
            return 1;
        return precise_collision_int(a, idx, idy, ia, b, ib);
    }
    if (!pin_bbox(a, &l, &t, &r, &bb) || !pin_bbox(b, &l2, &t2, &r2, &b2))
        return 0;
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
        if (!pcinst_of(a, dx, dy, &A) || !pcinst_of(b, 0, 0, &B)) return 0;
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

/* Command_InstancePlace: SetPosition(px, py) (a real move marks self dirty), the search, SetPosition back */
int instance_place_p(int self, double px, double py, int obj)
{
    PWST(place, 1);
    int k, q = pcol_query(obj);
    double dx = px - PTOD(PW.in[self].x), dy = py - PTOD(PW.in[self].y);
    int moved = (float)px != (float)PTOD(PW.in[self].x) || (float)py != (float)PTOD(PW.in[self].y);
    if (q < 0) return NOONE;
    if (q == 1) {
        struct qctx c;
        double l, t, r, b;
        pcol_touch(self);
        if (moved) pcol_place_marks(self);
        c.obj = obj; c.self = self; c.hit = NOONE; c.dx = dx; c.dy = dy;
        if (pin_bbox(self, &l, &t, &r, &b))
            pcol_search((float)(l + dx), (float)(t + dy), (float)(r + dx), (float)(b + dy), place_cb, &c);
        else
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

int instance_nearest_p(double px, double py, int obj)
{
    int k, best = NOONE;
    double bd = 0;
    struct fam it;
    PWST(nearest, 1);
    if (fam_none(obj)) return NOONE;
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

int instance_first_p(int obj)
{
    struct fam it;
    PWST(exists, 1);
    if (fam_none(obj)) return NOONE;
    fam_begin(&it, obj);
    return fam_get(&it);
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

double distance_to_instance_p(int self, int k)
{
    double sl, st, sr, sb, l, t, r, b, xd = 0, yd = 0, d;
    if (!pin_bbox(self, &sl, &st, &sr, &sb))
        sl = sr = PTOD(PW.in[self].x), st = sb = PTOD(PW.in[self].y);
    if (!pin_bbox(k, &l, &t, &r, &b))
        l = r = PTOD(PW.in[k].x), t = b = PTOD(PW.in[k].y);
    if (l > sr) xd = l - sr;
    if (r < sl) xd = r - sl;
    if (t > sb) yd = t - sb;
    if (b < st) yd = b - st;
    d = xd * xd + yd * yd;
    {   /* sqrt by Newton (no libm on the SH-2) */
        double s = d, prev = 0;
        int it;
        if (d <= 0) return 0;
        for (it = 0; it < 64 && s != prev; it++) { prev = s; s = 0.5 * (s + d / s); }
        return s;
    }
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

int pw_test_pair(int a, int b) { return overlap_at(a, 0, 0, b); }
