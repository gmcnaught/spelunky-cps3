/* GML scripts of the play loop (refs/hd/src/scripts/<name>/<name>.gml; line numbers in comments). */
#include "pint.h"
#ifdef PLAY_STATS
#include <stdio.h>
#include <stdlib.h>
#endif
#include "../snd/sndgame.h"                     /* the GML sound calls (src/snd) */
#include "pmsg.h"                                /* the HUD messages (trMessages) */
#include "pcontent.h"
#include "prand.h"                              /* prandom: u * 2^-32 * n from the bits */
#include "precip.h"                             /* moveTo: round(1 / frac(|v|)) from the bits */                            /* P7 content packages (docs/CONTENT.md) */

/* (double)u * (1.0 / 4294967296.0) * n, from u's bits (prand.h: the same double) */
double prandom(double n)
{
    return prand_scale(rng_next(&g_rng), n);
}

/* scripts/setCollisionBounds */
void setCollisionBounds(int i, int l, int t, int r, int b)
{
    struct pin *p = &PX(i);
    PE(p)->lbo = (int8_t)l;
    PE(p)->tbo = (int8_t)t;
    PE(p)->rbo = (int8_t)r;
    PE(p)->bbo = (int8_t)b;
}

/* scripts/calculateCollisionBounds */
void calcBounds(int i, double *lb, double *tb, double *rb, double *bb)
{
    const struct pin *p = &PX(i);
    *lb = PTOD(p->x) + PE(p)->lbo;
    *tb = PTOD(p->y) + PE(p)->tbo;
    *rb = PTOD(p->x) + PE(p)->rbo;
    *bb = PTOD(p->y) + PE(p)->bbo;
}

/* calculateCollisionBounds as ints when x and y are whole numbers (then the rounding below is the identity) */
static inline __attribute__((always_inline)) int ibounds(int i, int32_t *lb, int32_t *tb, int32_t *rb, int32_t *bb)
{
    const struct pin *p = &PX(i);
    int32_t x, y;
    if (!pin_xy_int(i, &x, &y)) return 0;
    *lb = x + PE(p)->lbo;
    *tb = y + PE(p)->tbo;
    *rb = x + PE(p)->rbo;
    *bb = y + PE(p)->bbo;
    return 1;
}

/* scripts/isCollisionLeft: collision_line(round(lb-d), round(tb), round(lb-d), round(bb-1), oSolid, 1, 1) > 0 */
int isCollisionLeft(int i, int d)
{
    double lb, tb, rb, bb;
    int32_t il, it, ir, ib;
    if (ibounds(i, &il, &it, &ir, &ib))
        return solid_vline_any(il - d, it, ib - 1, i);
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(lb - d), dround(tb), dround(lb - d), dround(bb - 1), OBJ_oSolid, 1, i) != NOONE;
}

int isCollisionRight(int i, int d)
{
    double lb, tb, rb, bb;
    int32_t il, it, ir, ib;
    if (ibounds(i, &il, &it, &ir, &ib))
        return solid_vline_any(ir + d - 1, it, ib - 1, i);
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(rb + d - 1), dround(tb), dround(rb + d - 1), dround(bb - 1), OBJ_oSolid, 1, i) != NOONE;
}

int isCollisionTop(int i, int d)
{
    double lb, tb, rb, bb;
    int32_t il, it, ir, ib;
    if (ibounds(i, &il, &it, &ir, &ib))
        return solid_hline_any(it - d, il, ir - 1, i);
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(lb), dround(tb - d), dround(rb - 1), dround(tb - d), OBJ_oSolid, 1, i) != NOONE;
}

int isCollisionBottom(int i, int d)
{
    double lb, tb, rb, bb;
    int32_t il, it, ir, ib;
    if (ibounds(i, &il, &it, &ir, &ib))
        return solid_hline_any(ib + d - 1, il, ir - 1, i);
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(lb), dround(bb + d - 1), dround(rb - 1), dround(bb + d - 1), OBJ_oSolid, 1, i) != NOONE;
}

/* scripts/isCollisionLadder */
int isCollisionLadder(int i)
{
    double lb, tb, rb, bb;
    int32_t il, it, ir, ib;
    if (ibounds(i, &il, &it, &ir, &ib)) {
        if (collision_rect_i(il + 8, it + 8, ir - 8, ib - 8, OBJ_oLadderTop, 1, i) != NOONE)
            return 1;
        return collision_rect_i(il + 8, it + 8, ir - 8, ib - 8, OBJ_oLadder, 1, i) != NOONE;
    }
    calcBounds(i, &lb, &tb, &rb, &bb);
    if (collision_rect_p(lb + 8, tb + 8, rb - 8, bb - 8, OBJ_oLadderTop, 1, i) != NOONE)
        return 1;
    return collision_rect_p(lb + 8, tb + 8, rb - 8, bb - 8, OBJ_oLadder, 1, i) != NOONE;
}

int isCollisionPlatformBottom(int i, int d)
{
    double lb, tb, rb, bb;
    int32_t il, it, ir, ib;
    if (ibounds(i, &il, &it, &ir, &ib))
        return collision_line_i(il, ib + d - 1, ir - 1, ib + d - 1, OBJ_oPlatform, 1, i) != NOONE;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(lb), dround(bb + d - 1), dround(rb - 1), dround(bb + d - 1), OBJ_oPlatform, 1, i) != NOONE;
}

int isCollisionPlatform(int i)
{
    double lb, tb, rb, bb;
    int32_t il, it, ir, ib;
    if (ibounds(i, &il, &it, &ir, &ib))
        return collision_rect_i(il, it, ir - 1, ib - 1, OBJ_oPlatform, 1, i) != NOONE;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_rect_p(lb, tb, rb - 1, bb - 1, OBJ_oPlatform, 1, i) != NOONE;
}

int isCollisionWaterTop(int i, int d)
{
    double lb, tb, rb, bb;
    int32_t il, it, ir, ib;
    if (ibounds(i, &il, &it, &ir, &ib))
        return collision_line_i(il, it - d, ir - 1, it - d, OBJ_oWater, 1, i) != NOONE;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(lb), dround(tb - d), dround(rb - 1), dround(tb - d), OBJ_oWater, 1, i) != NOONE;
}

int isCollisionMoveableSolidLeft(int i, int d)
{
    double lb, tb, rb, bb;
    int32_t il, it, ir, ib;
    if (ibounds(i, &il, &it, &ir, &ib))
        return collision_line_i(il - d, it, il - d, ib - 1, OBJ_oMoveableSolid, 1, i) != NOONE;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(lb - d), dround(tb), dround(lb - d), dround(bb - 1), OBJ_oMoveableSolid, 1, i) != NOONE;
}

int isCollisionMoveableSolidRight(int i, int d)
{
    double lb, tb, rb, bb;
    int32_t il, it, ir, ib;
    if (ibounds(i, &il, &it, &ir, &ib))
        return collision_line_i(ir + d - 1, it, ir + d - 1, ib - 1, OBJ_oMoveableSolid, 1, i) != NOONE;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(rb + d - 1), dround(tb), dround(rb + d - 1), dround(bb - 1), OBJ_oMoveableSolid, 1, i) != NOONE;
}

/* scripts/getIdCollisionRight / Left: the line starts 5 px below the top */
int getIdCollisionRight(int i, int d)
{
    double lb, tb, rb, bb;
    int32_t il, it, ir, ib;
    if (ibounds(i, &il, &it, &ir, &ib))
        return collision_line_i(ir + d - 1, it + 5, ir + d - 1, ib - 1, OBJ_oSolid, 1, i);
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(rb + d - 1), dround(tb + 5), dround(rb + d - 1), dround(bb - 1), OBJ_oSolid, 1, i);
}

int getIdCollisionLeft(int i, int d)
{
    double lb, tb, rb, bb;
    int32_t il, it, ir, ib;
    if (ibounds(i, &il, &it, &ir, &ib))
        return collision_line_i(il - d, it + 5, il - d, ib - 1, OBJ_oSolid, 1, i);
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(lb - d), dround(tb + 5), dround(lb - d), dround(bb - 1), OBJ_oSolid, 1, i);
}

/* getIdCollisionRight / Left(i, d) != noone: the line test alone (solid_vline_any, as isCollisionRight / Left take
   collision_line(...) != noone) */
static int anyCollisionRight(int i, int d)
{
    double lb, tb, rb, bb;
    int32_t il, it, ir, ib;
    if (ibounds(i, &il, &it, &ir, &ib))
        return solid_vline_any(ir + d - 1, it + 5, ib - 1, i);
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(rb + d - 1), dround(tb + 5), dround(rb + d - 1), dround(bb - 1), OBJ_oSolid, 1, i) != NOONE;
}

static int anyCollisionLeft(int i, int d)
{
    double lb, tb, rb, bb;
    int32_t il, it, ir, ib;
    if (ibounds(i, &il, &it, &ir, &ib))
        return solid_vline_any(il - d, it + 5, ib - 1, i);
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(lb - d), dround(tb + 5), dround(lb - d), dround(bb - 1), OBJ_oSolid, 1, i) != NOONE;
}

/* scripts/platformCharacterIs (the player's state) */
int platformCharacterIs(int what)
{
    int s = PL.state;
    if (what == ON_GROUND && (s == RUNNING || s == STANDING || s == DUCKING || s == LOOKING_UP)) return 1;
    if (what == IN_AIR && (s == JUMPING || s == FALLING)) return 1;
    if (what == ON_LADDER && s == CLIMBING) return 1;
    return 0;
}

/* scripts/approximatelyZero: -0.1 < a < 0.1 */
int approximatelyZero(num a)
{
    return NGT(a, N(-0.1)) && NLT(a, N(0.1));
}

static int is_character(int i)
{
    return obj_is(PX(i).obj, OBJ_oCharacter);
}

/* moveTo's whole steps of a velocity v: round(1 / frac(|v|)) (0 when frac(|v|) == 0 within GML_EPS), floor(|v|)
   and v < 0. The binary64 build takes them from v's bits (precip.h: integer operations and a threshold table, exact);
   outside its range, and in the other builds, the formula */
struct vparts { int32_t r, fl; int neg; };
static void vel_parts(num a, struct vparts *o)
{
#if !defined(PLAY_FIXED) && !defined(PLAY_COUNT)
    if (precip_parts(a, &o->r, &o->fl, &o->neg)) return;
#endif
    {
        num f = NFRAC(NABS(a));                                                /* :20-21 */
        o->r = NNE(f, N(0)) ? NRECIP_ROUND(f) : 0;                             /* :24 */
        o->fl = NFLOOR(NABS(a));                                               /* :30 */
        o->neg = a < 0;
    }
}

/* moveTo's pixel walks without the per-pixel setter (the grid build; PLAY_NOREST and the exact build keep them): a
   mover outside the oSolid family with whole x, y tests the same lines (the column or row ahead of its whole bounds,
   which a whole pixel step moves by one) from its start, then sets the position reached once. The walk's tests are
   oSolid searches that exclude the mover, so its own entry (flushed or not) does not change them, and the grid
   build's searches do not depend on when an entry is flushed (pobj.c PLAY_REST); the one setter call leaves the marks
   the last of the walk's would (the draw mark, the box cache, the dirty / test lists' fronts; rest_end reads only
   whether a change happened) */
#if !defined(PCOL_EXACT) && !defined(PLAY_FIXED) && !defined(NUM_IS_CLASS) && !defined(PLAY_NOREST)
#define PLAY_WALK 1
#else
#define PLAY_WALK 0
#endif

#if PLAY_WALK
/* no oPlatform instance's box meets the region a character's fall of n pixels tests (the bounds now, n rows down, one
   pixel of margin round): then every isCollisionPlatform / isCollisionPlatformBottom of the walk is false (a precise
   hit needs the box first; the tests are searches excluding the mover) */
static int platform_none(int i, int32_t n)
{
    double lb, tb, rb, bb;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_rect_p(lb - 1, tb - 1, rb + 1, bb + n + 1, OBJ_oPlatform, 0, i) == NOONE;
}
#endif

/* scripts/moveTo */
void moveTo(int i, num a0, num a1, int32_t *xio, int32_t *yio)
{
    struct pin *p = &PX(i);
    pos mtXPrev = p->x, mtYPrev = p->y;
    struct vparts vx, vy;
    int32_t xVelInteger = 0, yVelInteger = 0;
    int ch, xdone = 0, ydone = 0, raw = 0, noplat = 0;
#if PLAY_WALK
    int32_t il, it, ir, ib;
#endif
/* a pixel step: the setter, or (raw) the field alone, the setter called once with the position reached (below) */
#define MT_SETX(v) (raw ? (void)(PIN_WR(pos, p->x) = (v)) : pin_setx(p, (v)))
#define MT_SETY(v) (raw ? (void)(PIN_WR(pos, p->y) = (v)) : pin_sety(p, (v)))
    vel_parts(a0, &vx);
    vel_parts(a1, &vy);
    if (vx.r != 0) xVelInteger = (int32_t)(play_time % (uint32_t)vx.r) == 0;
    if (vy.r != 0) yVelInteger = (int32_t)(play_time % (uint32_t)vy.r) == 0;
    xVelInteger += vx.fl;
    yVelInteger += vy.fl;
    if (vx.neg) xVelInteger = -xVelInteger;
    if (vy.neg) yVelInteger = -yVelInteger;
    NOPS(10);
    /* the solid's id matters only to a character (pushing an oMoveableSolid); for the others any solid stops the walk:
       the line test alone (is_character is the object's, constant here) */
    ch = is_character(i);
#if PLAY_WALK
    /* the other walks (a character, a fractional position) step the fields alone: their tests are oSolid / oPlatform
       searches that exclude the mover (the same calls in the same order), a mover outside both families */
    raw = !obj_is(p->obj, OBJ_oSolid) && !obj_is(p->obj, OBJ_oPlatform);
    if (xVelInteger != 0 && !ch && !obj_is(p->obj, OBJ_oSolid) && ibounds(i, &il, &it, &ir, &ib)) {
        int32_t n = xVelInteger > 0 ? xVelInteger : -xVelInteger, k;
        for (k = 0; k < n; k++)
            if (solid_vline_any(xVelInteger > 0 ? ir + k : il - 1 - k, it + 5, ib - 1, i)) break;
        if (k) MT_SETX(mtXPrev + PI(xVelInteger > 0 ? k : -k));
        xdone = 1;
    }
#endif
    if (xVelInteger > 0 && !xdone)                                             /* :39 */
        for (; p->x < mtXPrev + PI(xVelInteger); MT_SETX(p->x + (PI(1)))) {
            int solidId;
            if (!ch) {
                if (anyCollisionRight(i, 1)) break;
                continue;
            }
            solidId = (!raw || anyCollisionRight(i, 1)) ? getIdCollisionRight(i, 1) : NOONE;
            if (solidId != NOONE) {
                if (objdefs[PX(solidId).obj].parent == OBJ_oMoveableSolid && is_character(i)) {
                    /* with solidId: `break` leaves the with, not the for */
                    if (!place_meeting_p(solidId, PTOD(PX(solidId).x) + 1, PTOD(PX(solidId).y), OBJ_oSolid)) {
                        pin_setx(&PX(solidId), PX(solidId).x + (PI(1)));
                        if (!snd_is_playing(SND_xpush)) snd_play(SND_xpush);          /* :59 */
                    }
                } else
                    break;
            }
        }
    if (xVelInteger < 0 && !xdone)                                             /* :64 */
        for (; p->x > mtXPrev + PI(xVelInteger); MT_SETX(p->x - (PI(1)))) {
            int solidId;
            if (!ch) {
                if (anyCollisionLeft(i, 1)) break;
                continue;
            }
            solidId = (!raw || anyCollisionLeft(i, 1)) ? getIdCollisionLeft(i, 1) : NOONE;
            if (solidId != NOONE) {
                if (objdefs[PX(solidId).obj].parent == OBJ_oMoveableSolid && is_character(i)) {
                    if (!place_meeting_p(solidId, PTOD(PX(solidId).x) - 1, PTOD(PX(solidId).y), OBJ_oSolid)) {
                        pin_setx(&PX(solidId), PX(solidId).x - (PI(1)));
                        if (!snd_is_playing(SND_xpush)) snd_play(SND_xpush);          /* :87 */
                    }
                } else
                    break;
            }
        }
#if PLAY_WALK
    if (yVelInteger != 0 && (yVelInteger < 0 || !ch) && !obj_is(p->obj, OBJ_oSolid) && ibounds(i, &il, &it, &ir, &ib)) {
        int32_t n = yVelInteger > 0 ? yVelInteger : -yVelInteger, k;
        for (k = 0; k < n; k++)
            if (solid_hline_any(yVelInteger > 0 ? ib + k : it - 1 - k, il, ir - 1, i)) break;
        if (k) MT_SETY(mtYPrev + PI(yVelInteger > 0 ? k : -k));
        ydone = 1;
    }
#endif
#if PLAY_WALK
    if (raw && ch && yVelInteger > 0 && !ydone) noplat = platform_none(i, yVelInteger);
#endif
    if (yVelInteger > 0 && !ydone)                                             /* :89 */
        for (; p->y < mtYPrev + PI(yVelInteger); MT_SETY(p->y + (PI(1)))) {
            if (isCollisionBottom(i, 1))
                break;
            if (is_character(i) && !noplat)
                if (!isCollisionPlatform(i) && isCollisionPlatformBottom(i, 1) && !PL.kDown)
                    break;
#ifdef PLAY_STATS
            if (noplat && (isCollisionPlatform(i) || isCollisionPlatformBottom(i, 1))) {   /* the host builds check it */
                fprintf(stderr, "moveTo: platform_none missed a platform (instance %d)\n", i);
                abort();
            }
#endif
        }
    if (yVelInteger < 0 && !ydone)                                             /* :98 */
        for (; p->y > mtYPrev + PI(yVelInteger); MT_SETY(p->y - (PI(1))))
            if (isCollisionTop(i, 1))
                break;
    if (raw) {                                     /* the setters, once: x then y, as the walks' last calls */
        pos fx = p->x, fy = p->y;
        PIN_WR(pos, p->x) = mtXPrev;
        PIN_WR(pos, p->y) = mtYPrev;
        pin_setx(p, fx);
        pin_sety(p, fy);
    }
#undef MT_SETX
#undef MT_SETY
    if (xio) *xio = xVelInteger;
    if (yio) *yio = yVelInteger;
}

/* scripts/scrCreateBlood */
void scrCreateBlood(int self, pos x, pos y, int n)
{
    if (self == PL.idx && PL.bloodless)
        return;
    while (n-- > 0)
        if (instance_number_p(OBJ_oDetritus) < 16)
            pin_create(x, y, OBJ_oBlood);
}

/* scripts/scrCreateFlame */
void scrCreateFlame(pos x, pos y, int n)
{
    while (n-- > 0)
        if (instance_number_p(OBJ_oDetritus) < 16)
            pin_create(x, y, OBJ_oFlame);
}

/* scripts/scrShake (the view part is for the shake in oLevel's Step) */
void scrShake(int d)
{
    if (PG.shake < d) PG.shake = d;
    if (PG.shake < 0) PG.shake = 0;
}

int isRoomIs(int r)
{
    return PW.room == r;
}

/* scripts/isLevel, isRealLevel */
int isLevel(void)
{
    return PW.room == R_rTutorial || PW.room == R_rLoadLevel || PW.room == R_rLevel || PW.room == R_rLevel2 ||
           PW.room == R_rLevel3 || PW.room == R_rOlmec;
}

int isRealLevel(void)
{
    return PW.room == R_rLevel || PW.room == R_rLevel2 || PW.room == R_rLevel3 || PW.room == R_rOlmec;
}

/* move_snap(hs, vs): x = round(x / hs) * hs */
void move_snap(int i, int hs, int vs)
{
    struct pin *p = &PX(i);
    if (hs > 0) pin_setx(p, PI(dround(PTOD(p->x) / hs) * hs));
    if (vs > 0) pin_sety(p, PI(dround(PTOD(p->y) / vs) * vs));
}

/* x > xview - m and x < xview + 320 + m and y > yview - m and y < yview + 240 + m */
int inview(int i, int m)
{
    int32_t ix, iy;
    view_read();
    if (pin_xy_int(i, &ix, &iy))                  /* whole numbers: the epsilon compares are the integer ones */
        return ix > PW.xview - m && ix < PW.xview + 320 + m && iy > PW.yview - m && iy < PW.yview + 240 + m;
    return PGTI(PX(i).x, PW.xview - m) && PLTI(PX(i).x, PW.xview + 320 + m) && PGTI(PX(i).y, PW.yview - m) &&
           PLTI(PX(i).y, PW.yview + 240 + m);
}

static const int16_t pick2t[PICK_COUNT] = { T_NONE, T_ROCK, T_JAR, T_SKULL, T_FISHBONE, T_ARROW, T_MACHETE,
    T_MATTOCK, T_MATTOCKHEAD, T_PISTOL, T_WEBCANNON, T_TELEPORTER, T_SHOTGUN, T_BOW, T_FLARE, T_SCEPTRE, T_KEY,
    T_OTHER };

int ptype_of_pickup(int pickup)
{
    return pick2t[pickup];
}

int pickup_of_ptype(int t)
{
    int k;
    for (k = 0; k < PICK_OTHER; k++)
        if (pick2t[k] == t) return k;
    return PICK_OTHER;
}

/* scripts/scrHoldItem: called by oPlayer1 */
void scrHoldItem(int t)
{
    struct pin *pl = &PX(PL.idx);
    int obj = -1;
    if (t == T_NONE) {
        PL.holdItem = NOONE;
        PL.pickupItemType = T_NONE;
        return;
    }
    switch (t) {
    case T_ROCK: obj = OBJ_oRock; break;
    case T_JAR: obj = OBJ_oJar; break;
    case T_SKULL: obj = OBJ_oSkull; break;
    case T_FISHBONE: obj = OBJ_oFishBone; break;
    case T_ARROW: obj = OBJ_oArrow; break;
    case T_MACHETE: obj = OBJ_oMachete; break;
    case T_MATTOCK: obj = OBJ_oMattock; break;
    case T_MATTOCKHEAD: obj = OBJ_oMattockHead; break;
    case T_PISTOL: obj = OBJ_oPistol; break;
    case T_WEBCANNON: obj = OBJ_oWebCannon; break;
    case T_TELEPORTER: obj = OBJ_oTeleporter; break;
    case T_SHOTGUN: obj = OBJ_oShotgun; break;
    case T_BOW: obj = OBJ_oBow; break;
    case T_FLARE: obj = OBJ_oFlare; break;
    case T_SCEPTRE: obj = OBJ_oSceptre; break;
    case T_KEY: obj = OBJ_oKey; break;
    }
    if (obj >= 0) {
        int h = pin_create(pl->x, pl->y, obj);
        PL.holdItem = h;
        PE(&PX(h))->held = 1;
        PE(&PX(h))->cost = 0;
        PE(&PX(h))->New = 0;
        PL.pickupItemType = (int16_t)t;
        PL.whoaTimer = PL.whoaTimerMax;
    } else {
        PL.holdItem = NOONE;
        PL.pickupItemType = T_NONE;
    }
}

/* scripts/scrDropItem */
void scrDropItem(num xv, num yv)
{
    int h = PL.holdItem;
    if (h == NOONE)
        return;
    PE(&PX(h))->held = 0;
    PE(&PX(h))->xVel = xv;
    PE(&PX(h))->yVel = yv;
    if (PL.bowArmed)
        scrFireBow();
    if (PL.pickupItemType != PX(h).type)
        scrHoldItem(PL.pickupItemType);
    else {
        PL.holdItem = NOONE;
        PL.pickupItemType = T_NONE;
    }
}

void scrFireBow(void)
{
    pitems_player(3001, PL.idx, 0);                                  /* the bow: not in P4's routes */
}

/* scripts/scrStealItem */
void scrStealItem(void)
{
    int h = PL.holdItem, d;
    const char *m1 = "", *m2 = "";                /* :304 trMessages(message1, message2, 0, 0, 120) */
    switch (PX(h).type) {
    case T_BOMBBAG:
        PG.bombs += 3;
        d = pin_create(PX(h).x, PX(h).y - PI(14), OBJ_oItemsGet);
        pin_set_sprite(d, GSPR_sBombsGet);
        pin_destroy(h);
        snd_play(SND_xpickup);                                                 /* :71 / :82 */
        PL.holdItem = NOONE;
        m1 = "YOU GOT 3 MORE BOMBS!";
        break;
    case T_BOMBBOX:
        PG.bombs += 12;
        d = pin_create(PX(h).x, PX(h).y - PI(14), OBJ_oItemsGet);
        pin_set_sprite(d, GSPR_sBombsGet);
        pin_destroy(h);
        snd_play(SND_xpickup);                                                 /* :71 / :82 */
        PL.holdItem = NOONE;
        m1 = "YOU GOT 12 MORE BOMBS!";
        break;
    case T_ROPEPILE:
        PG.rope += 3;
        d = pin_create(PX(h).x, PX(h).y - PI(15), OBJ_oItemsGet);
        pin_set_sprite(d, GSPR_sRopeGet);
        pin_destroy(h);
        snd_play(SND_xpickup);                                                 /* :103 */
        PL.holdItem = NOONE;
        m1 = "YOU GOT 3 MORE ROPES!";
        break;
    case T_UDJATEYE: case T_ANKH: case T_CROWN: case T_KAPALA: case T_PASTE: case T_PARACHUTE: case T_SPECTACLES:
    case T_GLOVES: case T_MITT: case T_COMPASS: case T_SPRINGSHOES: case T_SPIKESHOES: case T_JORDANS: case T_CAPE:
    case T_JETPACK:
        pitems_player(3002, PL.idx, 0);                              /* the equipment pickups: P5 */
        return;
    case T_MACHETE: case T_MATTOCK: case T_PISTOL: case T_WEBCANNON: case T_TELEPORTER: case T_SHOTGUN: case T_BOW:
        if (PE(&PX(h))->cost > 0) {                     /* :229-293 */
            static const struct { int16_t t; const char *a, *b; } wm[7] = {
                { T_MACHETE, "YOU GOT A MACHETE!", "" }, { T_MATTOCK, "YOU GOT A MATTOCK!", "IT SEEMS A BIT RUSTY." },
                { T_PISTOL, "YOU GOT A PISTOL!", "" }, { T_WEBCANNON, "YOU GOT A WEB CANNON!", "" },
                { T_TELEPORTER, "YOU GOT A TELEPORTER!", "" }, { T_SHOTGUN, "YOU GOT A SHOTGUN!", "" },
                { T_BOW, "YOU GOT THE BOW AND ARROWS!", "" } };
            int k;
            PE(&PX(h))->cost = 0;
            PE(&PX(h))->forSale = 0;
            snd_play(SND_xpickup);
            for (k = 0; k < 7; k++)
                if (wm[k].t == PX(h).type) { m1 = wm[k].a; m2 = wm[k].b; }
        }
        break;
    case T_DAMSEL:                                /* P5: bought (global.damselsBought: statistics) */
        if (PE(&PX(h))->cost > 0) {
            PE(&PX(h))->cost = 0;
            PE(&PX(h))->forSale = 0;
            snd_play(SND_xpickup);                                             /* :301 */
            m1 = "YOU MUST BE IN LOVE!";
        }
        break;
    default:
        break;
    }
    pmsg_player_str(m1, m2, 120);                                              /* :304 (oPlayer1 runs it) */
}

/* radtodeg(arctan(a)): fdlibm's s_atan.c (no libm on the SH-2); the runner's libm may differ in the last bit, so
   image_angle (drawing only) is compared with a tolerance by tools/playcmp.py */
static const double atanhi[] = { 4.63647609000806093515e-01, 7.85398163397448278999e-01,
                                 9.82793723247329054082e-01, 1.57079632679489655800e+00 };
static const double atanlo[] = { 2.26987774529616870924e-17, 3.06161699786838301793e-17,
                                 1.39033110312309984516e-17, 6.12323399573676603587e-17 };
static const double aT[] = { 3.33333333333329318027e-01, -1.99999999998764832476e-01, 1.42857142725034663711e-01,
                             -1.11111104054623557880e-01, 9.09088713343650656196e-02, -7.69187620504482999495e-02,
                             6.66107313738753120669e-02, -5.83357013379057348645e-02, 4.97687799461593236017e-02,
                             -3.65315727442169155270e-02, 1.62858201153657823623e-02 };

double patan(double x)
{
    double w, s1, s2, z, ax = x < 0 ? -x : x;
    int id;
    if (ax >= 4.4028204e15) return x > 0 ? atanhi[3] + atanlo[3] : -atanhi[3] - atanlo[3];
    if (ax < 0.4375) {
        if (ax < 1e-29) return x;
        id = -1;
    } else if (ax < 1.1875) {
        if (ax < 0.6875) { id = 0; ax = (2.0 * ax - 1.0) / (2.0 + ax); }
        else { id = 1; ax = (ax - 1.0) / (ax + 1.0); }
    } else if (ax < 2.4375) { id = 2; ax = (ax - 1.5) / (1.0 + 1.5 * ax); }
    else { id = 3; ax = -1.0 / ax; }
    z = ax * ax;
    w = z * z;
    s1 = z * (aT[0] + w * (aT[2] + w * (aT[4] + w * (aT[6] + w * (aT[8] + w * aT[10])))));
    s2 = w * (aT[1] + w * (aT[3] + w * (aT[5] + w * (aT[7] + w * aT[9]))));
    if (id < 0) return (x < 0 ? -1 : 1) * (ax - ax * (s1 + s2));
    z = atanhi[id] - ((ax * (s1 + s2) - atanlo[id]) - ax);
    return x < 0 ? -z : z;
}

double patan_deg(double a)
{
    return patan(a) * 180.0 / 3.14159265358979323846;
}

/* scripts/scrClearGlobals: a new game. The generator's globals (struct gglobals: gen_new_game, which also sets the
   values oGlobals / scrInit / oTitle Create give them), then the play loop's in the script's order; the globals the
   port does not model (titleStart, entityGen, yviewPrev, xview / yview, waterCounter, crapsPoint, probLake,
   marketEntrance, goldEntrance, hasNinjaSuit, flares) are left out */
void scrClearGlobals(void)
{
    gen_new_game();
    PG.ghostExists = 0;                                                        /* :20 */
    PG.drawHUD = 0;
    PG.collect = 0;
    PG.collectCounter = 0;
    PG.shake = 0;
    PG.shakeToggle = 0;
    PMSG.bloodLevel = 0;                                                       /* :35 */
    PG.hasUdjatEye = 0;                                                        /* :68 */
    PG.hasAnkh = 0;
    PG.hasCrown = 0;
    PG.hasKapala = 0;
    PG.hasStickyBombs = 0;
    PG.hasCompass = 0;
    PG.hasParachute = 0;
    PG.hasSpringShoes = 0;
    PG.hasSpikeShoes = 0;
    PG.hasJordans = 0;
    PG.hasCape = 0;
    PG.hasJetpack = 0;
    PG.hasGloves = 0;
    PG.hasMitt = 0;
    if (G.isTunnelMan) {                                                       /* :86 */
        PG.plife = 2;
        PG.bombs = 0;
        PG.rope = 0;
    } else {
        PG.plife = 4;
        PG.bombs = 4;
        PG.rope = 4;
    }
    PG.arrows = 0;                                                             /* :99 */
    PG.money = 0;
    PG.time = 0;
    PG.kills = 0;
    PG.damsels = 0;
    PG.gold = PG.goldbar = PG.goldbars = PG.nuggets = 0;
    PG.rubies = PG.bigrubies = PG.sapphires = PG.bigsapphires = PG.emeralds = PG.bigemeralds = PG.diamonds = 0;
    PG.scarabs = PG.idols = PG.skulls = 0;
    PG.xdamsels = 0;
    PG.xmoney = 0;
    PG.xtime = 0;
    PG.bats = PG.snakes = PG.spiders = PG.skeletons = PG.frogs = PG.firefrogs = PG.piranhas = PG.mantraps = 0;
    PG.yetis = PG.aliens = PG.ufos = PG.cavemen = PG.hawkmen = PG.monkeys = PG.zombies = PG.vampires = 0;
    PG.deadfish = PG.alienbosses = PG.giantspiders = PG.yetikings = PG.megamouths = PG.tomblords = 0;
    PG.shopkeepers = 0;
    PG.damselsKilled = 0;
}
