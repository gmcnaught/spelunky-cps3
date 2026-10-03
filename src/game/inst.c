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

struct world W;

static int is_solid_family(int obj) { return obj_is(obj, OBJ_oSolid); }

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

static void grid_remove(int i)
{
    struct inst *p = &W.in[i];
    if (p->ingrid == 1) {
        int16_t *q = &W.cell[p->y >> 4][p->x >> 4];
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
    if (!p->alive || !is_solid_family(p->obj) || !inst_bbox(i, &l, &t, &r, &b))
        return;
    if (l >= 0 && t >= 0 && (l & 15) == 0 && (t & 15) == 0 && r - l == 16 && b - t == 16 &&
        (l >> 4) < GRID_W && (t >> 4) < GRID_H && l == p->x && t == p->y) {
        W.cnext[i] = W.cell[t >> 4][l >> 4];
        W.cell[t >> 4][l >> 4] = (int16_t)i;
        p->ingrid = 1;
    } else {
        W.irr[W.nirr++] = (int16_t)i;
        p->ingrid = 2;
    }
}

void inst_reset(int32_t next_id)
{
    int y, x;
    W.n = 0;
    W.next_id = next_id;
    W.nirr = 0;
    for (y = 0; y < GRID_H; y++)
        for (x = 0; x < GRID_W; x++)
            W.cell[y][x] = INST_NONE;
}

int inst_add(int obj, int x, int y, int32_t id)
{
    int i = W.n++, k;
    struct inst *p = &W.in[i];
    p->id = id;
    p->obj = (int16_t)obj;
    p->spr = gobjspr[obj];
    p->x = (int16_t)x;
    p->y = (int16_t)y;
    p->depth = (int16_t)objdefs[obj].depth;
    p->alive = 1;
    p->flags = 0;
    p->status = 0;
    p->facing = 0;
    p->style = 0;
    p->ingrid = 0;
    p->counter = 0;
    p->xvel = p->yvel = 0;
    p->cost = 0;
    p->value = 0;
    for (k = 0; k < ALARMS; k++)
        p->alarm[k] = -1;
    grid_add(i);
    return i;
}

void inst_set_sprite(int i, int spr)
{
    grid_remove(i);
    W.in[i].spr = (int16_t)spr;
    grid_add(i);
}

void inst_destroyed(int i)
{
    grid_remove(i);
    W.in[i].alive = 0;
}

static int point_in(int i, int px, int py)
{
    int32_t l, t, r, b;
    return inst_bbox(i, &l, &t, &r, &b) && px >= l && px < r && py >= t && py < b;
}

static int collision_point_scan(int px, int py, int obj)
{
    int k;
    for (k = W.n - 1; k >= 0; k--)
        if (inst_is(k, obj) && point_in(k, px, py))
            return k;
    return INST_NONE;
}

int collision_point(int px, int py, int obj)
{
    int best = INST_NONE, k;
    if (!is_solid_family(obj))
        return collision_point_scan(px, py, obj);
    if (px >= 0 && py >= 0 && (px >> 4) < GRID_W && (py >> 4) < GRID_H)
        for (k = W.cell[py >> 4][px >> 4]; k != INST_NONE; k = W.cnext[k])
            if (k > best && obj_is(W.in[k].obj, obj))
                best = k;
    for (k = 0; k < W.nirr; k++) {
        int j = W.irr[k];
        if (j > best && obj_is(W.in[j].obj, obj) && point_in(j, px, py))
            best = j;
    }
    return best;
}

int collision_rectangle(int x1, int y1, int x2, int y2, int obj, int self, int notme)
{
    int k;
    int32_t bl = x1 < x2 ? x1 : x2, br = x1 < x2 ? x2 : x1;
    int32_t bt = y1 < y2 ? y1 : y2, bb = y1 < y2 ? y2 : y1;
    for (k = W.n - 1; k >= 0; k--) {
        int32_t l, t, r, b;
        if (!inst_is(k, obj) || (notme && k == self) || !inst_bbox(k, &l, &t, &r, &b))
            continue;
        if (bl >= r || br < l || bt >= b || bb < t)
            continue;
        if ((bl > l ? bl : l) == (br < r ? br : r) || (bt > t ? bt : t) == (bb < b ? bb : b))
            continue;
        return k;
    }
    return INST_NONE;
}

int instance_place(int self, int px, int py, int obj)
{
    int k;
    int32_t l, t, r, b;
    if (!inst_bbox(self, &l, &t, &r, &b))
        return INST_NONE;
    l += px - W.in[self].x;
    r += px - W.in[self].x;
    t += py - W.in[self].y;
    b += py - W.in[self].y;
    for (k = W.n - 1; k >= 0; k--) {
        int32_t l2, t2, r2, b2;
        if (k == self || !inst_is(k, obj) || !inst_bbox(k, &l2, &t2, &r2, &b2))
            continue;
        /* rectangle masks only (the generator's callers are treasures and rectangular solids) */
        if (l < r2 && l2 < r && t < b2 && t2 < b)
            return k;
    }
    return INST_NONE;
}

int32_t distance2_to_object(int self, int obj)
{
    int k;
    int32_t best = -1, sl, st, sr, sb;
    if (!inst_bbox(self, &sl, &st, &sr, &sb))
        sl = sr = W.in[self].x, st = sb = W.in[self].y;
    for (k = W.n - 1; k >= 0; k--) {
        int32_t l, t, r, b, xd = 0, yd = 0, d;
        if (!inst_is(k, obj))
            continue;
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
    int k;
    for (k = W.n - 1; k >= 0; k--)
        if (inst_is(k, obj))
            return k;
    return INST_NONE;
}

int inst_selftest(void)
{
    int x, y, bad = 0;
    for (y = -16; y < GRID_H * 16 + 16; y += 4)
        for (x = -16; x < GRID_W * 16 + 16; x += 4)
            if (collision_point(x, y, OBJ_oSolid) != collision_point_scan(x, y, OBJ_oSolid) ||
                collision_point(x, y, OBJ_oBrick) != collision_point_scan(x, y, OBJ_oBrick))
                bad++;
    return bad;
}
