/* Instance model and GameMaker collision tests (see inst.h).
 *
 * Collision rules, GameMaker 2.3+ with option_collision_compatibility = false (refs/hd/src/options/main/
 * options_main.yy), as the GameMaker runner computes them (HTML5 runner: yyInstance.js Compute_BoundingBox,
 * Collision_Point, Collision_Rectangle; Function_Movement.js distance_to_object):
 *   bbox: left = x + (sprite bbox_left - xorigin), right = left + bbox width (exclusive); same in y.
 *   collision_point: left <= px < right and top <= py < bottom. Every generator call passes prec = 0, so the
 *     bounding box decides even for precise sprites.
 *   collision_rectangle(x1, y1, x2, y2): with l = max(min(x1, x2), left), r = min(max(x1, x2), right): a hit
 *     needs l < r (and the same in y): a rectangle that only touches the bbox edge misses.
 *   distance_to_object: per axis the gap between the boxes (0 if they overlap), sqrt(dx^2 + dy^2), the minimum
 *     over the object's instances. Kept squared here; the callers compare with squared limits.
 * Positions are whole numbers during generation, so all of this is integer arithmetic.
 */
#include "inst.h"

extern struct inst inst_mem[];
struct world W = { .in = inst_mem };
int gen_untranslated;
void (*inst_hook)(int op, int i, int a, int b, int c);

static int is_solid_family(int obj) { return obj_is(obj, OBJ_oSolid); }

/* ---- the objects that are obj or its descendants: obj_desc[obj_desc0[obj] .. obj_desc0[obj + 1]) (also used by
   the play loop, pworld.c) ---------------------------------------------------------------------------------- */
int16_t obj_desc0[OBJ_COUNT + 1];
int16_t obj_desc[2048];
static uint8_t desc_ready;

void obj_desc_init(void)
{
    static int16_t fill[OBJ_COUNT];
    int o, a, n = 0;
    if (desc_ready) return;
    for (a = 0; a <= OBJ_COUNT; a++) obj_desc0[a] = 0;
    for (o = 0; o < OBJ_COUNT; o++)                     /* each object counts for itself and its ancestors */
        for (a = o; a >= 0; a = objdefs[a].parent) obj_desc0[a + 1]++;
    for (a = 0; a < OBJ_COUNT; a++) obj_desc0[a + 1] = (int16_t)(obj_desc0[a + 1] + obj_desc0[a]);
    if (obj_desc0[OBJ_COUNT] > (int)(sizeof obj_desc / sizeof obj_desc[0])) { UNTRANSLATED(7003); return; }
    for (a = 0; a < OBJ_COUNT; a++) fill[a] = obj_desc0[a];
    for (o = 0; o < OBJ_COUNT; o++)                     /* ascending o: each family in object order */
        for (a = o; a >= 0; a = objdefs[a].parent) obj_desc[fill[a]++] = (int16_t)o;
    (void)n;
    desc_ready = 1;
}

/* ---- per-object lists of the alive generator instances in creation order, and the alive counts of each object
   with its descendants; searches walk the family's lists merged by index ---------------------------------- */
static int16_t gw_ohead[OBJ_COUNT], gw_otail[OBJ_COUNT], gw_inext[INST_MAX], gw_iprev[INST_MAX], gw_live[OBJ_COUNT];

static void gw_reset(void)
{
    int o;
    obj_desc_init();
    for (o = 0; o < OBJ_COUNT; o++) { gw_ohead[o] = gw_otail[o] = INST_NONE; gw_live[o] = 0; }
}

static void gw_link(int i)
{
    int o = W.in[i].obj, a;
    gw_inext[i] = INST_NONE;
    gw_iprev[i] = gw_otail[o];
    if (gw_otail[o] >= 0) gw_inext[gw_otail[o]] = (int16_t)i; else gw_ohead[o] = (int16_t)i;
    gw_otail[o] = (int16_t)i;
    for (a = o; a >= 0; a = objdefs[a].parent) gw_live[a]++;
}

static void gw_unlink(int i)
{
    int o = W.in[i].obj, a;
    if (gw_iprev[i] >= 0) gw_inext[gw_iprev[i]] = gw_inext[i]; else gw_ohead[o] = gw_inext[i];
    if (gw_inext[i] >= 0) gw_iprev[gw_inext[i]] = gw_iprev[i]; else gw_otail[o] = gw_iprev[i];
    for (a = o; a >= 0; a = objdefs[a].parent) gw_live[a]--;
}

/* the family's alive instances in creation order; more than WF_K non-empty lists: a scan of every instance */
#define WF_K 24
struct wfam { int16_t cur[WF_K]; int n, lin, k, obj; };

static void wf_begin(struct wfam *it, int obj)
{
    int j;
    it->n = 0; it->lin = 0; it->k = -1; it->obj = obj;
    for (j = obj_desc0[obj]; j < obj_desc0[obj + 1]; j++) {
        int h = gw_ohead[obj_desc[j]];
        if (h < 0) continue;
        if (it->n == WF_K) { it->lin = 1; return; }
        it->cur[it->n++] = (int16_t)h;
    }
}

static int wf_next(struct wfam *it)
{
    int j, b = 0, v;
    if (it->lin) {
        while (++it->k < W.n)
            if (inst_is(it->k, it->obj)) return it->k;
        return INST_NONE;
    }
    if (it->n == 0) return INST_NONE;
    for (j = 1; j < it->n; j++)
        if (it->cur[j] < it->cur[b]) b = j;
    v = it->cur[b];
    if (gw_inext[v] >= 0) it->cur[b] = gw_inext[v];
    else it->cur[b] = it->cur[--it->n];
    return v;
}

int inst_bbox(int i, int32_t *l, int32_t *t, int32_t *r, int32_t *b)
{
    const struct inst *p = &W.in[i];
    const struct gsprcol *s;
    if (p->spr < 0)
        return 0;
    s = &gsprcol[p->spr];
    *l = p->x + s->l - s->xo;
    *r = *l + (s->r - s->l + 1);
    *t = p->y + s->t - s->yo;
    *b = *t + (s->b - s->t + 1);
    return 1;
}

static int16_t ghome[INST_MAX];               /* the lcell index of an instance with ingrid 3 */

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void grid_remove(int i)
{
    struct inst *p = &W.in[i];
    if (p->ingrid == 1) {
        int16_t *q = &W.cell[p->y >> 4][p->x >> 4];
        while (*q != i)
            q = &W.cnext[*q];
        *q = W.cnext[i];
    } else if (p->ingrid == 3) {
        int16_t *q = &W.lcell[ghome[i] / GRID_W][ghome[i] % GRID_W];
        while (*q != i)
            q = &W.cnext[*q];
        *q = W.cnext[i];
    } else if (p->ingrid == 2) {
        int k;
        for (k = 0; k < W.nirr; k++)
            if (W.irr[k] == i) {
                W.irr[k] = W.irr[--W.nirr];
                break;
            }
    }
    p->ingrid = 0;
}

static void grid_add(int i)
{
    struct inst *p = &W.in[i];
    int32_t l, t, r, b;
    if (!p->alive || !inst_bbox(i, &l, &t, &r, &b))
        return;
    if (is_solid_family(p->obj) && l >= 0 && t >= 0 && (l & 15) == 0 && (t & 15) == 0 && r - l == 16 &&
        b - t == 16 && (l >> 4) < GRID_W && (t >> 4) < GRID_H && l == p->x && t == p->y) {
        W.cnext[i] = W.cell[t >> 4][l >> 4];
        W.cell[t >> 4][l >> 4] = (int16_t)i;
        p->ingrid = 1;
    } else if (r - l > 64 || b - t > 64) {
        W.irr[W.nirr++] = (int16_t)i;
        p->ingrid = 2;
    } else {                                    /* home cell: the top-left corner's, clamped */
        int cx = clampi(l >> 4, 0, GRID_W - 1), cy = clampi(t >> 4, 0, GRID_H - 1);
        int ex = ((r - 1) >> 4) - cx, ey = ((b - 1) >> 4) - cy;
        if (ex > W.lext_x) W.lext_x = (int16_t)ex;
        if (ey > W.lext_y) W.lext_y = (int16_t)ey;
        ghome[i] = (int16_t)(cy * GRID_W + cx);
        W.cnext[i] = W.lcell[cy][cx];
        W.lcell[cy][cx] = (int16_t)i;
        p->ingrid = 3;
    }
}

/* the generator moved instance i (x, y written): its place in the grids */
void inst_moved(int i)
{
    grid_remove(i);
    grid_add(i);
}

void inst_reset(int32_t next_id)
{
    int y, x;
    W.n = 0;
    W.next_id = next_id;
    W.nirr = 0;
    gw_reset();
    if (inst_hook) inst_hook(IH_RESET, 0, next_id, 0, 0);
    for (y = 0; y < GRID_H; y++)
        for (x = 0; x < GRID_W; x++)
            W.cell[y][x] = W.lcell[y][x] = INST_NONE;
    W.lext_x = W.lext_y = 0;
}

int inst_add(int obj, int x, int y, int32_t id)
{
    int i, k;
    struct inst *p;
    if (W.n >= INST_MAX) {                  /* never in the references; the level is then not comparable */
        UNTRANSLATED(7002);
        W.n = INST_MAX - 1;
    }
    i = W.n++;
    p = &W.in[i];
    p->id = id;
    p->obj = (int16_t)obj;
    p->spr = gobjspr[obj];
    p->x = (int16_t)x;
    p->y = (int16_t)y;
    p->depth = objdefs[obj].depth;
    p->alive = 1;
    p->flags = 0;
    p->status = 0;
    p->facing = 0;
    p->style = 0;
    p->ingrid = 0;
    p->counter = 0;
    p->treasure = TR_NONE;
    p->etype = EX_EXIT;
    p->linkval = 0;
    p->shifttoggle = 0;
    p->dir = 0;
    p->spurttime = 0;
    p->deathtimer = 0;
    p->xvel = p->yvel = 0;
    p->cost = 0;
    p->value = 0;
    for (k = 0; k < ALARMS; k++)
        p->alarm[k] = -1;
    gw_link(i);
    grid_add(i);
    if (inst_hook) inst_hook(IH_CREATE, i, 0, 0, 0);
    return i;
}

void inst_set_sprite(int i, int spr)
{
    if (inst_hook) inst_hook(IH_SPRITE, i, spr, 0, 0);
    grid_remove(i);
    W.in[i].spr = (int16_t)spr;
    grid_add(i);
}

void inst_destroyed(int i)
{
    grid_remove(i);
    if (W.in[i].alive) gw_unlink(i);
    W.in[i].alive = 0;
    if (inst_hook) inst_hook(IH_DESTROY, i, 0, 0, 0);
}

static int point_in(int i, int px, int py)
{
    int32_t l, t, r, b;
    return inst_bbox(i, &l, &t, &r, &b) && px >= l && px < r && py >= t && py < b;
}

static int collision_point_scan(int px, int py, int obj)
{
    int k;
    struct wfam it;
    if (gw_live[obj] == 0) return INST_NONE;
    wf_begin(&it, obj);
    while ((k = wf_next(&it)) != INST_NONE)
        if (point_in(k, px, py))
            return k;
    return INST_NONE;
}

static int collision_point_(int px, int py, int obj);
int collision_point(int px, int py, int obj)
{
    int r = collision_point_(px, py, obj);
    if (inst_hook) inst_hook(IH_POINT, r, obj, 0, 0);
    return r;
}

/* the oldest instance of obj whose bbox holds the point: the grids (a point on the grid), or the scan */
static int collision_point_(int px, int py, int obj)
{
    int best = INST_MAX, k, cx, cy, x, y;
    if (gw_live[obj] == 0) return INST_NONE;
    if (!(px >= 0 && py >= 0 && (px >> 4) < GRID_W && (py >> 4) < GRID_H))
        return collision_point_scan(px, py, obj);
    cx = px >> 4;
    cy = py >> 4;
    for (k = W.cell[cy][cx]; k != INST_NONE; k = W.cnext[k])        /* aligned solids: the box is the cell */
        if (k < best && obj_is(W.in[k].obj, obj))
            best = k;
    for (y = clampi(cy - W.lext_y, 0, GRID_H - 1); y <= cy; y++)
        for (x = clampi(cx - W.lext_x, 0, GRID_W - 1); x <= cx; x++)
            for (k = W.lcell[y][x]; k != INST_NONE; k = W.cnext[k])
                if (k < best && obj_is(W.in[k].obj, obj) && point_in(k, px, py))
                    best = k;
    for (k = 0; k < W.nirr; k++) {
        int j = W.irr[k];
        if (j < best && obj_is(W.in[j].obj, obj) && point_in(j, px, py))
            best = j;
    }
    return best == INST_MAX ? INST_NONE : best;
}

static int collision_rectangle_(int x1, int y1, int x2, int y2, int obj, int self, int notme);
int collision_rectangle(int x1, int y1, int x2, int y2, int obj, int self, int notme)
{
    int r = collision_rectangle_(x1, y1, x2, y2, obj, self, notme);
    if (inst_hook) inst_hook(IH_RECT, r, obj, 0, 0);
    return r;
}

static int collision_rectangle_(int x1, int y1, int x2, int y2, int obj, int self, int notme)
{
    int k;
    int32_t bl = x1 < x2 ? x1 : x2, br = x1 < x2 ? x2 : x1;
    int32_t bt = y1 < y2 ? y1 : y2, bb = y1 < y2 ? y2 : y1;
    struct wfam it;
    if (gw_live[obj] == 0) return INST_NONE;
    wf_begin(&it, obj);
    while ((k = wf_next(&it)) != INST_NONE) {
        int32_t l, t, r, b;
        if ((notme && k == self) || !inst_bbox(k, &l, &t, &r, &b))
            continue;
        if (bl >= r || br < l || bt >= b || bb < t)
            continue;
        if ((bl > l ? bl : l) == (br < r ? br : r) || (bt > t ? bt : t) == (bb < b ? bb : b))
            continue;
        return k;
    }
    return INST_NONE;
}

static int instance_place_(int self, int px, int py, int obj);
int instance_place(int self, int px, int py, int obj)
{
    int r = instance_place_(self, px, py, obj);
    if (inst_hook)
        inst_hook(IH_PLACE, self, obj, r, (px - W.in[self].x + 2048) + 4096 * (py - W.in[self].y + 2048));
    return r;
}

static int instance_place_(int self, int px, int py, int obj)
{
    int k;
    int32_t l, t, r, b;
    if (!inst_bbox(self, &l, &t, &r, &b))
        return INST_NONE;
    l += px - W.in[self].x;
    r += px - W.in[self].x;
    t += py - W.in[self].y;
    b += py - W.in[self].y;
    {
    struct wfam it;
    if (gw_live[obj] == 0) return INST_NONE;
    wf_begin(&it, obj);
    while ((k = wf_next(&it)) != INST_NONE) {
        int32_t l2, t2, r2, b2;
        if (k == self || !inst_bbox(k, &l2, &t2, &r2, &b2))
            continue;
        /* rectangle masks only (the generator's callers are treasures and rectangular solids) */
        if (l < r2 && l2 < r && t < b2 && t2 < b)
            return k;
    }
    }
    return INST_NONE;
}

int32_t distance2_to_object(int self, int obj)
{
    int k;
    if (inst_hook) inst_hook(IH_DIST, self, obj, 0, 0);
    int32_t best = -1, sl, st, sr, sb;
    struct wfam it;
    if (!inst_bbox(self, &sl, &st, &sr, &sb))
        sl = sr = W.in[self].x, st = sb = W.in[self].y;
    if (gw_live[obj] == 0) return best;
    wf_begin(&it, obj);
    while ((k = wf_next(&it)) != INST_NONE) {
        int32_t l, t, r, b, xd = 0, yd = 0, d;
        if (!inst_bbox(k, &l, &t, &r, &b))
            l = r = W.in[k].x, t = b = W.in[k].y;
        if (l > sr) xd = l - sr;
        if (r < sl) xd = r - sl;
        if (t > sb) yd = t - sb;
        if (b < st) yd = b - st;
        d = xd * xd + yd * yd;
        if (best < 0 || d < best)
            best = d;
    }
    return best;
}

int instance_exists(int obj)
{
    return instance_first(obj) != INST_NONE;
}

int instance_first(int obj)
{
    struct wfam it;
    if (gw_live[obj] == 0) return INST_NONE;
    wf_begin(&it, obj);
    return wf_next(&it);
}

int16_t with_pool[WITH_POOL];
int with_top;

int with_collect(int obj)
{
    int k, n = 0, base = with_top, j;
    struct wfam it;
    if (gw_live[obj] > 0) {
        wf_begin(&it, obj);                       /* creation order, then reversed: newest first */
        while ((k = wf_next(&it)) != INST_NONE) {
            if (with_top >= WITH_POOL) { UNTRANSLATED(7001); break; }
            with_pool[with_top++] = (int16_t)k;
            n++;
        }
        for (j = 0; j < n / 2; j++) {
            int16_t t = with_pool[base + j];
            with_pool[base + j] = with_pool[base + n - 1 - j];
            with_pool[base + n - 1 - j] = t;
        }
    }
    if (n == 2) {                       /* the runner visits exactly two instances oldest first */
        int16_t t = with_pool[base];
        with_pool[base] = with_pool[base + 1];
        with_pool[base + 1] = t;
    }
    return n;
}

int instance_number(int obj)
{
    return gw_live[obj];
}

int instance_nearest(int px, int py, int obj)
{
    int k, best = INST_NONE;
    int32_t bd = 0;
    struct wfam it;
    if (gw_live[obj] == 0) return INST_NONE;
    wf_begin(&it, obj);
    while ((k = wf_next(&it)) != INST_NONE) {
        int32_t dx, dy, d;
        dx = px - W.in[k].x;
        dy = py - W.in[k].y;
        d = dx * dx + dy * dy;
        if (best == INST_NONE || d < bd) {
            best = k;
            bd = d;
        }
    }
    return best;
}

int inst_selftest(void)
{
    int x, y, bad = 0;
    for (y = -16; y < GRID_H * 16 + 16; y += 4)
        for (x = -16; x < GRID_W * 16 + 16; x += 4)
        {
            static const int16_t objs[] = { OBJ_oSolid, OBJ_oBrick, OBJ_oWater, OBJ_oLava, OBJ_oTreasure, OBJ_oLadder,
                                            OBJ_oSpikes, OBJ_oEnemy, OBJ_oItem };
            unsigned j;
            for (j = 0; j < sizeof objs / sizeof objs[0]; j++)
                if (collision_point(x, y, objs[j]) != collision_point_scan(x, y, objs[j]))
                    bad++;
        }
    return bad;
}
