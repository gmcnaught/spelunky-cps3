/* Play world: instances and GameMaker 2024.14's collision functions (rules and evidence: play.h).
 * Searches return the oldest matching instance (P2: collision_point, instance_place, instance_find, obj.var). */
#include "play.h"

struct pworld PW;

static int spr_of(const struct pin *p) { return p->mask >= 0 ? p->mask : p->spr; }

void pw_reset(void)
{
    PW.n = 0;
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
    p->spr = gobjspr[obj];
    p->mask = -1;
    p->alive = 1;
    p->visible = pobj[obj].visible;
    p->persistent = pobj[obj].persistent;
    p->x = x;
    p->y = y;
    p->xprev = x;
    p->yprev = y;
    p->depth = objdefs[obj].depth;
    p->img = 0;
    p->ispd = 1;
    p->xscale = p->yscale = 1;
    p->angle = 0;
    p->alpha = 1;
    for (k = 0; k < 12; k++)
        p->alarm[k] = -1;
    p->trapID = p->enemyID = NOONE;
    return i;
}

int pin_create(pos x, pos y, int obj)
{
    int i = pin_add(obj, x, y, PW.next_id++);
    ev_create(i);
    return i;
}

void pin_destroy(int i)
{
    if (i < 0 || !PW.in[i].alive)
        return;
    PW.in[i].alive = 0;          /* GameMaker marks it first: a search inside its Destroy event skips it */
    ev_destroy(i);
}

void pin_kill(int i)
{
    if (i >= 0)
        PW.in[i].alive = 0;
}

/* sprite_index = spr: image_index is kept unless it is past the new sprite's frames, then 0 (Observed in
   build/trace/p4_walk_s1: sRunLeft at image 4.668 -> sFallLeft (1 frame) 0.0 in record 60; sFallLeft at 0.4 ->
   sRunLeft (6 frames) 0.4 in record 66) */
void pin_set_sprite(int i, int spr)
{
    struct pin *p = &PW.in[i];
    if (p->spr != spr) {
        p->spr = (int16_t)spr;
        if (spr >= 0 && (p->img >= (img_t)psprite[spr].frames || p->img < 0))
            p->img = 0;
    }
}

/* ---- bounding boxes ------------------------------------------------------------------------------------- */
int pin_bbox(int i, double *l, double *t, double *r, double *b)
{
    const struct pin *p = &PW.in[i];
    int s = spr_of(p);
    const struct gsprcol *c;
    double xs, ys, x = PTOD(p->x), y = PTOD(p->y);
    if (s < 0)
        return 0;
    c = &gsprcol[s];
    xs = p->xscale;
    ys = p->yscale;
    if (xs >= 0) *l = x + xs * (c->l - c->xo);
    else *l = x + xs * (c->r + 1 - c->xo);
    *r = *l + (xs < 0 ? -xs : xs) * (c->r - c->l + 1);
    if (ys >= 0) *t = y + ys * (c->t - c->yo);
    else *t = y + ys * (c->b + 1 - c->yo);
    *b = *t + (ys < 0 ? -ys : ys) * (c->b - c->t + 1);
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
        return (m[cy * bpr + (cx >> 3)] >> (7 - (cx & 7))) & 1;
    }
}

static int precise(int i)
{
    int s = spr_of(&PW.in[i]);
    return s >= 0 && gsprcol[s].kind == 1;
}

static int match(int k, int obj, int notme_self)
{
    return PW.in[k].alive && k != notme_self && (obj == -2 ? 1 : obj_is(PW.in[k].obj, obj));
}

/* obj: an OBJ_* (with children), or -2 - k for "instance k only" via the _i helpers below */
static int point_hit(int k, double px, double py, int prec)
{
    double l, t, r, b;
    if (!pin_bbox(k, &l, &t, &r, &b))
        return 0;
    if (!(px >= l && px < r && py >= t && py < b))
        return 0;
    return !prec || !precise(k) || mask_at(k, px, py);
}

int collision_point_p(double px, double py, int obj, int prec, int notme_self)
{
    int k;
    for (k = 0; k < PW.n; k++)
        if (match(k, obj, notme_self) && point_hit(k, px, py, prec))
            return k;
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

static int line_hit(int k, double x1, double y1, double x2, double y2, int prec)
{
    double l, t, r, b, t0, t1;
    if (!pin_bbox(k, &l, &t, &r, &b))
        return 0;
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

int collision_line_p(double x1, double y1, double x2, double y2, int obj, int prec, int notme_self)
{
    int k;
    for (k = 0; k < PW.n; k++)
        if (match(k, obj, notme_self) && line_hit(k, x1, y1, x2, y2, prec))
            return k;
    return NOONE;
}

static int rect_hit(int k, double x1, double y1, double x2, double y2, int prec)
{
    double l, t, r, b, a0, a1, b0, b1;
    double lx = x1 < x2 ? x1 : x2, hx = x1 < x2 ? x2 : x1, ly = y1 < y2 ? y1 : y2, hy = y1 < y2 ? y2 : y1;
    if (!pin_bbox(k, &l, &t, &r, &b))
        return 0;
    lx = dfloor(lx + 0.5);
    hx = dfloor(hx + 0.5);
    ly = dfloor(ly + 0.5);
    hy = dfloor(hy + 0.5);
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

int collision_rect_p(double x1, double y1, double x2, double y2, int obj, int prec, int notme_self)
{
    int k;
    for (k = 0; k < PW.n; k++)
        if (match(k, obj, notme_self) && rect_hit(k, x1, y1, x2, y2, prec))
            return k;
    return NOONE;
}

/* instance a (its bbox moved by dx, dy) against instance b */
static int overlap_at(int a, double dx, double dy, int b)
{
    double l, t, r, bb, l2, t2, r2, b2;
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
        for (py = dfloor(y0); py < y1; py++)
            for (px = dfloor(x0); px < x1; px++)
                if (mask_at(a, px - dx, py - dy) && mask_at(b, px, py))
                    return 1;
        return 0;
    }
}

int pin_overlap(int a, int b)
{
    return overlap_at(a, 0, 0, b);
}

int instance_place_p(int self, double px, double py, int obj)
{
    int k;
    double dx = px - PTOD(PW.in[self].x), dy = py - PTOD(PW.in[self].y);
    for (k = 0; k < PW.n; k++)
        if (match(k, obj, self) && overlap_at(self, dx, dy, k))
            return k;
    return NOONE;
}

int instance_nearest_p(double px, double py, int obj)
{
    int k, best = NOONE;
    double bd = 0;
    for (k = 0; k < PW.n; k++) {
        double dx, dy, d;
        if (!match(k, obj, NOONE))
            continue;
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
    int k;
    for (k = 0; k < PW.n; k++)
        if (match(k, obj, NOONE))
            return k;
    return NOONE;
}

int instance_exists_p(int obj)
{
    return instance_first_p(obj) != NOONE;
}

int instance_number_p(int obj)
{
    int k, n = 0;
    for (k = 0; k < PW.n; k++)
        if (match(k, obj, NOONE))
            n++;
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
    for (k = 0; k < PW.n; k++)
        if (match(k, obj, NOONE)) {
            double d = distance_to_instance_p(self, k);
            if (d < best) best = d;
        }
    return best;
}

int pw_with(int obj, int16_t *out, int max)
{
    int k, n = 0;
    for (k = PW.n - 1; k >= 0; k--)
        if (match(k, obj, NOONE) && n < max)
            out[n++] = (int16_t)k;
    if (n == 2) {
        int16_t t = out[0];
        out[0] = out[1];
        out[1] = t;
    }
    return n;
}
