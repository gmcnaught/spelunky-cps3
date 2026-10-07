/* The helpers the enemy and content files share (penemy.c, pdamsel.c, pshop.c, pk_*.c): one definition each, so the
 * copies cannot drift apart (pen_offview relies on eview being one test; REVIEW-C.md 4.1). Not in pint.h: X, Y, CP, pl
 * are short names for these files only. */
#ifndef PENHELP_H
#define PENHELP_H
#include "pint.h"

/* plain static, as the per-file copies were: `inline` changes GCC's inlining choices in the callers (measured on the
   SH-2 objects); unused: each file uses some of them */
#define PENHELP static __attribute__((unused))

PENHELP double X(int i) { return PTOD(PX(i).x); }
PENHELP double Y(int i) { return PTOD(PX(i).y); }
PENHELP int CP(double x, double y, int obj) { return collision_point_any(x, y, obj, 0, NOONE); }
/* collision_point(x, y, obj, -1, -1): the runner reads a GML bool as value > 0.5, so -1 is false for both prec
   (bounding box only) and notme (self not excluded). Observed: c_ice_alienboss record 213, oYeti's ledge test at
   (528, 48) hits an oDarkFall at 528, 48 whose precise mask has pixel (0, 0) clear */
PENHELP int CPn(double x, double y, int obj, int self) { (void)self; return collision_point_any(x, y, obj, 0, NOONE); }
PENHELP int sprw(int i) { int s = PX(i).spr; return s >= 0 ? spr_dim(psprite[s].w, PX(i).xscale) : 0; }
PENHELP int sprh(int i) { int s = PX(i).spr; return s >= 0 ? spr_dim(psprite[s].h, PX(i).yscale) : 0; }
PENHELP double dabs(double d) { return d < 0 ? -d : d; }
PENHELP int pl(void) { return PL.idx; }
/* oEnemy Create's active state: xVel = yVel = xAcc = yAcc = 0 (en_: pobj.c has its own make_active, and penemy.c
   shares the SH-2 build's unity TU with it, scripts/unity.sh) */
PENHELP void en_make_active(struct pin *p) { PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0; }

/* the enemies' view test: x > xview - l and x < xview + 320 + r (same for y) */
PENHELP int eview(int i, int l, int r)
{
    int32_t ix, iy;
    view_read();
    if (pin_xy_int(i, &ix, &iy))          /* whole x, y: GML's compare of two ints is their order (as inview) */
        return ix > PW.xview - l && ix < PW.xview + 320 + r && iy > PW.yview - l && iy < PW.yview + 240 + r;
    return PGTI(PX(i).x, PW.xview - l) && PLTI(PX(i).x, PW.xview + 320 + r) && PGTI(PX(i).y, PW.yview - l) &&
           PLTI(PX(i).y, PW.yview + 240 + r);
}

/* isCollisionSolid(i): collision_rectangle over i's bounds, oSolid, precise, notme */
PENHELP int isCollisionSolid(int i)
{
    double lb, tb, rb, bb;
    int32_t x, y;
    /* whole x, y: the corners are whole, and collision_rect_p takes rq_init's integer path, which is
       collision_rect_i's (iok, fok 0, the same bounds; no instance of oSolid: NOONE without a side effect in both);
       solid_rect_any (pworld.c) answers that query's != NOONE from the solid grid's cell summary, else searches */
    if (pin_xy_int(i, &x, &y) && x > -29000 && x < 29000 && y > -29000 && y < 29000) {
        const struct pin_ext *e = PE(&PX(i));
        return solid_rect_any(x + e->lbo, y + e->tbo, x + e->rbo - 1, y + e->bbo - 1, i);
    }
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_rect_any(lb, tb, rb - 1, bb - 1, OBJ_oSolid, 1, i);
}

/* the jar's speed test (oJar / oSkull Step :104 / :148's second operand) before its collision_rectangle in the grid
   build (penemy.c pen_jar_hit, pdamsel.c): the query has no result-visible effect there (searches go in creation
   order, whatever the flush history: pobj.c PLAY_REST), so a jar at |xVel|, |yVel| <= 2 skips it. The exact build
   keeps the query first (it moves the R-tree's flush) */
#ifdef PCOL_EXACT
#define JAR_FAST(j) 1
#else
#define JAR_FAST(j) (NGT(NABS(PE(j)->xVel), N(2)) || NGT(NABS(PE(j)->yVel), N(2)))
#endif
#undef PENHELP
#endif
