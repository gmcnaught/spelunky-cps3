/* Play world: instances and GameMaker 2024.14's collision functions (rules and evidence: play.h).
 * Searches return the oldest matching instance (P2: collision_point, instance_place, instance_find, obj.var). */
#include "play.h"
#include "pcol.h"
#include "inst.h"                 /* GRID_W, GRID_H: the solid grid covers the generator's level grid */

struct pworld PW;
#ifdef PLAY_STATS
struct pw_stats pw_st;
#endif

static int spr_of(const struct pin *p) { return p->mask >= 0 ? p->mask : p->spr; }
static void grid_reset(void);
static void grid_unlink(int i);
static void grid_dirty(int i);
static int16_t ghead[GRID_H][GRID_W];
static int16_t gnext[PIN_MAX], gcell[PIN_MAX], gdnext[PIN_MAX];
static uint8_t gond[PIN_MAX];
static int16_t gdhead = NOONE;
static int gmaxw, gmaxh;

/* ---- per-object instance lists: the alive instances of each object in creation order (index order), and the
   alive count of each object with its descendants. Linked at pin_add, unlinked when alive goes to 0 ------------ */
int16_t pw_ohead[OBJ_COUNT], pw_inext[PIN_MAX];
/* every alive instance in creation order: pw_ahead, then pw_anext[i] (an instance unlinked keeps its pw_anext, so a
   walk that saw it continues from it) */
int16_t pw_ahead, pw_anext[PIN_MAX];
static int16_t pw_atail, aprev[PIN_MAX];
static int16_t otail[OBJ_COUNT], iprev[PIN_MAX];
static int16_t olive[OBJ_COUNT];
/* the objects that are obj or its descendants: odesc[odesc0[obj] .. odesc0[obj + 1]) */
static int16_t odesc0[OBJ_COUNT + 1];
static int16_t *odesc;
static int16_t odesc_buf[2048];

static void odesc_init(void)
{
    int o, a, n = 0;
    for (a = 0; a < OBJ_COUNT; a++) {
        odesc0[a] = (int16_t)n;
        for (o = 0; o < OBJ_COUNT; o++)
            if (obj_is(o, a)) {
                if (n == (int)(sizeof odesc_buf / sizeof odesc_buf[0])) { PUNTR(9003); break; }
                odesc_buf[n++] = (int16_t)o;
            }
    }
    odesc0[OBJ_COUNT] = (int16_t)n;
    odesc = odesc_buf;
}

static void olists_reset(void)
{
    int o;
    if (!odesc) odesc_init();
    for (o = 0; o < OBJ_COUNT; o++) {
        pw_ohead[o] = otail[o] = NOONE;
        olive[o] = 0;
    }
    pw_ahead = pw_atail = NOONE;
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
        if (it->cur[j] < it->cur[b]) b = j;
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
    while (++it->k < PW.n) {
        const struct pin *p = &PW.in[it->k];
        PWST(visit, 1);
        if (p->alive && (it->obj < 0 || obj_is(p->obj, it->obj))) return it->k;
    }
    return NOONE;
}

/* no alive instance of obj (with descendants) */
static int fam_none(int obj) { return obj >= 0 && olive[obj] == 0; }

/* a setter changed x / y / sprite / mask / scale / angle (play.h pin_changed_) */
void pw_changed(int i)
{
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

static void ext_reset(void)
{
    int k;
    ext_defaults(&pin_ext[0]);
    nextfree = 0;
    for (k = EXT_MAX - 1; k >= 1; k--) extfree[nextfree++] = (int16_t)k;
    ext_used = 0;
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

/* instance i left the room (RemoveMarked): its record is free */
void pw_removed(int i)
{
    struct pin *p = &PW.in[i];
    if (p->ext > 0) {
        extfree[nextfree++] = p->ext;
        ext_used--;
    }
#ifdef PIN_EXT_CHECK
    p->ext = -1;                                     /* a later PE(p) is an error */
#else
    p->ext = 0;
#endif
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
#endif

void pw_reset(void)
{
    PW.n = 0;
    ext_reset();
    olists_reset();
    pcol_after_reset();
}

int pin_add(int obj, pos x, pos y, int32_t id)
{
    int i, k;
    struct pin *p;
    if (PW.n >= PIN_MAX) {
        PUNTR(9001);
        return PIN_MAX - 1;
    }
    i = PW.n++;
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
    p->visible = pobj[obj].visible;
    p->persistent = pobj[obj].persistent;
    PIN_WR(pos, p->x) = x;
    PIN_WR(pos, p->y) = y;
    p->xprev = x;
    p->yprev = y;
    p->depth = objdefs[obj].depth;
    p->img = 0;
    p->ispd = 1;
    PIN_WR(double, p->xscale) = PIN_WR(double, p->yscale) = 1;
    PIN_WR(double, p->angle) = 0;
    p->alpha = 1;
    p->ext = (int16_t)(pin_needs_ext(obj) ? ext_alloc() : 0);   /* with pin_add's defaults (ext_defaults) */
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
    ev_destroy(i);
    pcol_destroyed(i);           /* in the collision tree until the next RemoveMarked */
}

void pin_kill(int i)
{
    if (i >= 0) {
        if (PW.in[i].alive) ounlink(i);
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
        grid_dirty(i);
        if (spr >= 0 && (p->img >= (img_t)psprite[spr].frames || p->img < 0))
            p->img = 0;
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
            if (xs && ys && pos_int(p->x, &x) && pos_int(p->y, &y)) {
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

/* the precise mask bit of instance i at room pixel (px, py) (inside its bbox) */
static int mask_at(int i, double px, double py)
{
    const struct pin *p = &PW.in[i];
    int s = spr_of(p), f, mw, cx, cy;
    const struct gsprcol *c = &gsprcol[s];
    const struct psprite *ps = &psprite[s];
    double sx, sy;
    if (c->kind != 1 || ps->nmasks == 0)
        return 1;
    f = 0;
    if (ps->nmasks > 1) {
        f = (int)p->img;
        if (f < 0) f = 0;
        f %= ps->nmasks;
    }
    sx = (px - PTOD(p->x)) / p->xscale + c->xo;
    sy = (py - PTOD(p->y)) / p->yscale + c->yo;
    cx = dfloor(sx) - c->l;
    cy = dfloor(sy) - c->t;
    mw = c->r - c->l + 1;
    if (cx < 0 || cy < 0 || cx >= mw || cy > c->b - c->t)
        return 0;
    {
        int bpr = (mw + 7) >> 3;
        const uint8_t *m = pmaskdata + ps->maskoff + f * bpr * (c->b - c->t + 1);
        static const uint8_t bit[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };  /* no variable shift */
        return (m[cy * bpr + (cx >> 3)] & bit[cx & 7]) != 0;
    }
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

/* a point query: px >= l && px < r with l, r whole is floor(px) >= l && floor(px) < r */
struct pq { double px, py; int32_t ix, iy; int iok; };

static void pq_init(struct pq *q, double px, double py)
{
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
    return !prec || !precise(k) || mask_at(k, q->px, q->py);
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
        for (x = 0; x < GRID_W; x++) ghead[y][x] = NOONE;
    gdhead = NOONE;
    gmaxw = gmaxh = 1;
}

static void grid_unlink(int i)
{
    int c = gcell[i];
    int16_t *pp;
    if (c < 0) return;
    for (pp = &ghead[c / GRID_W][c % GRID_W]; *pp != i; pp = &gnext[*pp]) {}
    *pp = gnext[i];
    gcell[i] = NOONE;
}

/* a solid's box may have changed (or it was added): placed again at the next query */
static void grid_dirty(int i)
{
    if (gond[i] || !obj_is(PW.in[i].obj, OBJ_oSolid)) return;
    gond[i] = 1;
    gdnext[i] = gdhead;
    gdhead = (int16_t)i;
}

static void grid_flush(void)
{
    while (gdhead >= 0) {
        int i = gdhead, cx, cy, w, h;
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
        if (w > gmaxw) gmaxw = w;
        if (h > gmaxh) gmaxh = h;
        cx = clampi(cx, 0, GRID_W - 1);
        cy = clampi(cy, 0, GRID_H - 1);
        gcell[i] = (int16_t)(cy * GRID_W + cx);
        gnext[i] = ghead[cy][cx];
        ghead[cy][cx] = (int16_t)i;
    }
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
                if ((best >= 0 && k > best) || k == notme || !obj_is(PW.in[k].obj, obj)) continue;
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

/* segment against the half-open box [l, r) x [t, b): Liang-Barsky with the open sides pulled in by 1e-9 */
static int seg_box(double x1, double y1, double x2, double y2, double l, double t, double r, double b,
                   double *t0o, double *t1o)
{
    double t0 = 0, t1 = 1, dx = x2 - x1, dy = y2 - y1;
    double pp[4], qq[4];
    int k;
    r -= 1e-9;
    b -= 1e-9;
    pp[0] = -dx; qq[0] = x1 - l;
    pp[1] = dx;  qq[1] = r - x1;
    pp[2] = -dy; qq[2] = y1 - t;
    pp[3] = dy;  qq[3] = b - y1;
    for (k = 0; k < 4; k++) {
        if (pp[k] == 0) {
            if (qq[k] < 0) return 0;
        } else {
            double u = qq[k] / pp[k];
            if (pp[k] < 0) { if (u > t1) return 0; if (u > t0) t0 = u; }
            else { if (u < t0) return 0; if (u < t1) t1 = u; }
        }
    }
    *t0o = t0;
    *t1o = t1;
    return 1;
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

static int line_hit(int k, struct qctx *c)
{
    double l, t, r, b, t0, t1, x1, y1, x2, y2;
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
    if (!seg_box(x1, y1, x2, y2, l, t, r, b, &t0, &t1))
        return 0;
    if (!prec || !precise(k))
        return 1;
    {   /* walk the clipped part a pixel at a time */
        double dx = x2 - x1, dy = y2 - y1, len = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
        int n = (int)((t1 - t0) * len) + 1, s;
        for (s = 0; s <= n; s++) {
            double u = t0 + (n ? (t1 - t0) * s / n : 0);
            double px = x1 + dx * u, py = y1 + dy * u;
            if (px >= l && px < r && py >= t && py < b && mask_at(k, px, py))
                return 1;
        }
        return 0;
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
struct rq { double lx, hx, ly, hy; int32_t ilx, ihx, ily, ihy; int iok; };

static void rq_init(struct rq *q, double x1, double y1, double x2, double y2)
{
    double lx = x1 < x2 ? x1 : x2, hx = x1 < x2 ? x2 : x1, ly = y1 < y2 ? y1 : y2, hy = y1 < y2 ? y2 : y1;
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

static int rect_hit(int k, const struct rq *q, int prec)
{
    double l, t, r, b, a0, a1, b0, b1;
    double lx = q->lx, hx = q->hx, ly = q->ly, hy = q->hy;
    int32_t ib[4];
    if (q->iok && pin_ibox(k, ib)) {
        int32_t i0 = q->ilx > ib[0] ? q->ilx : ib[0], i1 = q->ihx < ib[2] ? q->ihx : ib[2];
        int32_t j0 = q->ily > ib[1] ? q->ily : ib[1], j1 = q->ihy < ib[3] ? q->ihy : ib[3];
        if (!(i0 < i1 && j0 < j1))
            return 0;
        if (!prec || !precise(k))
            return 1;
    }
    if (!pin_bbox(k, &l, &t, &r, &b))
        return 0;
    a0 = lx > l ? lx : l;
    a1 = hx < r ? hx : r;
    b0 = ly > t ? ly : t;
    b1 = hy < b ? hy : b;
    if (!(a0 < a1 && b0 < b1))
        return 0;
    if (!prec || !precise(k))
        return 1;
    {
        int px, py;
        for (py = dfloor(b0); py < b1; py++)
            for (px = dfloor(a0); px < a1; px++)
                if (mask_at(k, px + 0.5 - 0.5, py))
                    return 1;
        return 0;
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
    }
    if (!pin_bbox(a, &l, &t, &r, &bb) || !pin_bbox(b, &l2, &t2, &r2, &b2))
        return 0;
    l += dx; r += dx; t += dy; bb += dy;
    if (!(l < r2 && l2 < r && t < b2 && t2 < bb))
        return 0;
    if (!precise(a) && !precise(b))
        return 1;
    {
        double x0 = l > l2 ? l : l2, x1 = r < r2 ? r : r2, y0 = t > t2 ? t : t2, y1 = bb < b2 ? bb : b2;
        int px, py;
        /* pixel centres: a pixel counts when its centre is inside both boxes (Observed: build/trace/p5_shop_s96
           record 211, a pellet whose box ends at y 256.4 does not hit the block whose top is y 256) (P5) */
        for (py = dfloor(y0); py < y1; py++) {
            if (!(py + 0.5 >= y0 && py + 0.5 < y1)) continue;
            for (px = dfloor(x0); px < x1; px++)
                if (px + 0.5 >= x0 && px + 0.5 < x1 && mask_at(a, px + 0.5 - dx, py + 0.5 - dy) &&
                    mask_at(b, px + 0.5, py + 0.5))
                    return 1;
        }
        return 0;
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
