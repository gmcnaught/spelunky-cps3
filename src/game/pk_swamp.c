/* P7 package B (swamp): docs/CONTENT.md §2. Water (the player's swimming, oWaterSwim, oBubble, oDrip, checkWater),
 * piranhas and fish bones, dead fish, the cemetery (oZombie, oGrave, oVampire), the ghost (oGame :45, oGhost) and the
 * cape (oCape; the pickup is package E's), the lake's mega mouth (oJaws). Translated statement for statement from refs/hd/src/objects/<obj>/<event>.gml
 * (line numbers in comments), overriding the weak defaults of pcontent.c.
 * GML keeps running after instance_destroy() inside an event: so does this code (RNG draws included). Function
 * arguments are evaluated last to first (instance_create(x + rand.., y + rand.., o) draws y's numbers first); the
 * operands of a binary operator left to right. A GML bool argument is true when > 0.5: collision_*(.., -1, -1) is
 * prec false, notme false.
 * Instance variables kept in struct pin_ext / pin_en fields of other names (the GML name in the macro): */
#include "pint.h"
#include "penemy.h"
#include "pcol.h"
#include "pmath.h"
#include "pcontent.h"
#include "../snd/sndgame.h"                     /* the GML sound calls (src/snd) */
#ifdef PLAY_STATS
#include <stdio.h>
#include <stdlib.h>
#endif

#define DIR(p)         (PE(p)->direction)       /* dir (oPiranha, oDeadFish, oVampire, oGhost, oJaws) */
#define BUBBLETIMER(p) (PEN(p)->turnTimer)      /* oPiranha / oDeadFish / oJaws bubbleTimer */
#define CANBITE(p)     (PE(p)->trigger)         /* oPiranha canBite */
#define CAPEOPEN(p)    (PE(p)->trigger)         /* oCape open */
#define VDEAD(p)       (PEN(p)->edead)          /* oVampire dead */

enum { E_LEFT = 0, E_RIGHT = 1, E_STUNNED = 98, E_DEAD = 99 };
/* oVampire's status values */
enum { V_IDLE, V_WALK, V_ATTACK, V_THROW, V_RECOVER, V_BOUNCE, V_HANG, V_FLY };

static double X(int i) { return PTOD(PX(i).x); }
static double Y(int i) { return PTOD(PX(i).y); }
static int CP(double x, double y, int obj) { return collision_point_any(x, y, obj, 0, NOONE); }
static int sprw(int i) { int s = PX(i).spr; return s >= 0 ? spr_dim(psprite[s].w, PX(i).xscale) : 0; }
static int sprh(int i) { int s = PX(i).spr; return s >= 0 ? spr_dim(psprite[s].h, PX(i).yscale) : 0; }
static double dabs(double a) { return a < 0 ? -a : a; }

/* the enemies' view test: x > xview - l and x < xview + 320 + r (same for y) */
static int eview(int i, int l, int r)
{
    int32_t ix, iy;
    view_read();
    if (pin_xy_int(i, &ix, &iy))          /* whole x, y: GML's compare of two ints is their order (as inview) */
        return ix > PW.xview - l && ix < PW.xview + 320 + r && iy > PW.yview - l && iy < PW.yview + 240 + r;
    return PGTI(PX(i).x, PW.xview - l) && PLTI(PX(i).x, PW.xview + 320 + r) && PGTI(PX(i).y, PW.yview - l) &&
           PLTI(PX(i).y, PW.yview + 240 + r);
}

/* distance_to_point(px, py) from i's bounding box */
static double distance_to_point_p(int i, double px, double py)
{
    double l, t, r, b, xd = 0, yd = 0;
    pcol_touch(i);
    if (!pin_bbox(i, &l, &t, &r, &b)) l = r = X(i), t = b = Y(i);
    if (px > r) xd = px - r;
    if (px < l) xd = px - l;
    if (py > b) yd = py - b;
    if (py < t) yd = py - t;
    return psqrt(xd * xd + yd * yd);
}

static void make_active(struct pin *p) { PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0; }

/* moveTo(c * cos(degtorad(d)), -c * sin(degtorad(d))) */
/* GML move toward direction d at speed c: moveTo by c * cos, -c * sin of degtorad_d(d), given as co, si (psincos_cr:
   pcos_cr's and psin_cr's bits; the callers' water test uses the same angle) */
static void move_cs(int i, double c, double co, double si)
{
    moveTo(i, ND(c * co), ND(-c * si), 0, 0);
}

static void kill_count(int i, int16_t *kind)
{
    if (!PEN(&PX(i))->countsAsKill) return;
    *kind += 1;                                                    /* (global.enemyKills: statistics) */
    PG.kills += 1;
}

/* repeat(3) instance_create(x + 8, y + 8, oBone); skull with yVel -rand(1,3), xVel rand(0,3) - rand(0,3) */
static void bones_and_skull(pos x, pos y, int skull_dy)
{
    int k, s;
    for (k = 0; k < 3; k++) pin_create(x, y, OBJ_oBone);
    s = pin_create(x, y + PI(skull_dy), OBJ_oSkull);
    PE(&PX(s))->yVel = NI(-RAND(1, 3));
    {
        int a = RAND(0, 3), b = RAND(0, 3);
        PE(&PX(s))->xVel = NI(a - b);
    }
}

/* ---- Create ---------------------------------------------------------------------------------------------- */
static int create(int i, int fromgen)
{
    struct pin *p = &PX(i);
    const struct inst *g = play_gen_inst;
    switch (p->obj) {
    case OBJ_oPiranha: case OBJ_oDeadFish:                     /* objects/oPiranha|oDeadFish/Create_0.gml */
        if (p->obj == OBJ_oPiranha) p->type = T_PIRANHA;       /* type = "Piranha" (oDeadFish keeps "NONE") */
        pin_setispd(p, (img_t)0.5);
        setCollisionBounds(i, 0, 0, 8, 8);
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        PE(p)->xAcc = N(0.2);
        PE(p)->yAcc = N(0.2);
        if (fromgen) DIR(p) = g ? g->dir : 0;                  /* :12 rand(1,2): the generator's */
        else {
            DIR(p) = 0;
            if (p->obj == OBJ_oPiranha && RAND(1, 2) == 1) DIR(p) = 180;
        }
        PE(p)->hp = 1;
        p->invincible = 0;
        BUBBLETIMER(p) = 0;
        PE(p)->status = 0;
        PE(p)->counter = 0;
        CANBITE(p) = 1;
        return 1;
    case OBJ_oZombie:                                          /* objects/oZombie/Create_0.gml */
        make_active(p);
        setCollisionBounds(i, 4, 4, 12, 16);
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        PE(p)->myGrav = N(0.2);
        PEN(p)->myGravNorm = N(0.2);
        pin_setispd(p, (img_t)0.4);
        PE(p)->hp = 1;
        p->invincible = 0;
        if (fromgen) PE(p)->facing = g ? g->facing : 0;        /* :17 rand(0,1): the generator's */
        else PE(p)->facing = (int16_t)RAND(0, 1);
        PE(p)->status = 0;
        PE(p)->counter = 0;
        if (fromgen) {
            if (g && (g->flags & IF_SWIMMING)) PEN(p)->swimming = 1;
        } else if (CP(X(i), Y(i), OBJ_oWater))
            PEN(p)->swimming = 1;                              /* :33 */
        return 1;
    case OBJ_oVampire:                                         /* objects/oVampire/Create_0.gml */
        make_active(p);
        setCollisionBounds(i, 2, 0, sprw(i) - 2, sprh(i));
        PE(p)->xVel = N(2.5);
        pin_setispd(p, (img_t)0.5);
        p->type = T_VAMPIRE;                                   /* type = "Vampire" */
        PE(p)->hp = 6;
        p->invincible = 0;
        PE(p)->myGrav = N(0.2);
        PEN(p)->myGravNorm = N(0.2);
        PE(p)->status = V_IDLE;
        PEN(p)->whipped = 0;
        PEN(p)->bounced = 0;
        VDEAD(p) = 0;
        PE(p)->counter = 0;
        PEN(p)->stunTime = 60;
        PEN(p)->sightCounter = 0;
        PE(p)->facing = E_RIGHT;
        PE(p)->colLeft = PE(p)->colRight = PE(p)->colBot = PE(p)->colTop = 0;
        return 1;
    case OBJ_oGhost:                                           /* objects/oGhost/Create_0.gml */
        p->type = T_NONE;
        pin_setispd(p, (img_t)0.5);
        setCollisionBounds(i, 4, 0, 12, 16);
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        PE(p)->xAcc = N(0.2);
        PE(p)->yAcc = N(0.2);
        PE(p)->hp = 1;
        p->invincible = 1;
        PE(p)->status = 1;                                     /* ATTACK */
        PE(p)->facing = E_RIGHT;
        snd_play(SND_xghost);                                  /* :28 */
        return 1;
    case OBJ_oBubble:                                          /* objects/oBubble/Create_0.gml */
        p->type = T_NONE;
        PE(p)->yVel = ND(-RAND(1, 3) * 0.1);
        PE(p)->yAcc = N(0.1);
        pin_setispd(p, (img_t)0.2);
        return 1;
    case OBJ_oCape:                                            /* objects/oCape/Create_0.gml */
        p->type = T_NONE;
        CAPEOPEN(p) = 0;
        return 1;
    case OBJ_oWater: case OBJ_oWaterSwim:                      /* objects/oWater/Create_0.gml: type, checked */
        p->type = T_NONE;
        return 1;
    case OBJ_oDrip:                                            /* oRubblePiece Create, then type = "Drip" */
        p->type = T_NONE;
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        PE(p)->yAcc = N(0.6);
        return 1;
    case OBJ_oJaws:                                            /* objects/oJaws/Create_0.gml (oEnemy's first) */
        pen_enemy_create(i);
        p->type = T_MEGAMOUTH;
        pin_setispd(p, (img_t)0.5);
        setCollisionBounds(i, 0, 0, 48, 32);
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        PE(p)->xAcc = N(0.2);
        PE(p)->yAcc = N(0.2);
        DIR(p) = 180;
        PE(p)->facing = 0;
        PE(p)->hp = 40;
        p->invincible = 0;
        BUBBLETIMER(p) = 0;
        PE(p)->canPickUp = 0;
        PE(p)->status = 0;
        PE(p)->counter = 0;
        return 1;
    case OBJ_oGrave:                                           /* objects/oGrave/Create_0.gml (oSolid's first) */
        if (!fromgen) {
            static const int16_t spr[5] = { GSPR_sGrave2, GSPR_sGrave3, GSPR_sGrave4, GSPR_sGrave5, GSPR_sGrave6 };
            p->invincible = 0;
            p->shopWall = 0;
            p->type = T_NONE;
            p->cleanDeath = 0;
            pin_set_sprite(i, spr[RAND(1, 5) - 1]);
        }
        return 1;
    }
    return 0;
}

/* ---- oPiranha / oDeadFish Step ----------------------------------------------------------------------------- */
/* obj = instance_nearest(x, y, oCaveman), then oShopkeeper, oHawkman, oYeti: the first alive (hp > 0) one */
static int prey(int i)
{
    static const int16_t objs[4] = { OBJ_oCaveman, OBJ_oShopkeeper, OBJ_oHawkman, OBJ_oYeti };
    int k, obj = NOONE;
    for (k = 0; k < 4; k++) {
        if (obj == NOONE) obj = instance_nearest_p(X(i), Y(i), objs[k]);
        if (obj != NOONE && PE(&PX(obj))->hp <= 0) obj = NOONE;
    }
    return obj;
}

/* some instance of prey()'s four families is swimming. prey() has no side effect, and IDLE uses its answer only as
   obj != NOONE && obj's swimming && hp > 0: with no swimming instance in the families that test fails whatever
   prey() returns */
static int prey_swims(void)
{
    static const int16_t objs[4] = { OBJ_oCaveman, OBJ_oShopkeeper, OBJ_oHawkman, OBJ_oYeti };
    int16_t w[32];
    int k, j, n;
    for (k = 0; k < 4; k++) {
        n = pw_with(objs[k], w, 32);
        if (n >= 32) return 1;
        for (j = 0; j < n; j++)
            if (PEN(&PX(w[j]))->swimming) return 1;
    }
    return 0;
}

/* IDLE: swim along the water */
static void fish_idle_swim(int i)
{
    struct pin *p = &PX(i);
    if (DEQ(DIR(p), 0)) {
        if (CP(X(i) + 8 + 2, Y(i), OBJ_oWater) && !CP(X(i) + 10, Y(i), OBJ_oSolid)) moveTo(i, N(1), 0, 0, 0);
        else DIR(p) = 180;
    } else {
        if (CP(X(i) - 2, Y(i), OBJ_oWater) && !CP(X(i) - 2, Y(i), OBJ_oSolid)) moveTo(i, N(-1), 0, 0, 0);
        else DIR(p) = 0;
    }
}

static void fish_end(int i, int left, int right)
{
    struct pin *p = &PX(i);
    if (DGT(DIR(p), 90) && DLT(DIR(p), 270)) pin_set_sprite(i, left);
    else pin_set_sprite(i, right);
    if (!CP(X(i) + 4, Y(i) + 4, OBJ_oWater)) {
        pin_create(PX(i).x, PX(i).y, OBJ_oFishBone);
        pin_destroy(i);
    }
}

static void piranha_step(int i)                                /* objects/oPiranha/Step_0.gml */
{
    struct pin *p = &PX(i);
    int c, obj;
    double dist;                                               /* (the squared distance: pdist2) */
    if (!PE(p)->active) return;
    if (PE(p)->hp < 1) {                                       /* :3 */
        scrCreateBlood(i, p->x + PI(4), p->y + PI(4), 3);
        kill_count(i, &PG.piranhas);
        pin_destroy(i);
    }
    p = &PX(i);
    c = instance_first_p(OBJ_oCharacter);
    dist = pdist2(X(i) + 4, Y(i) + 4, X(c), Y(c));
    if (PE(p)->status == 0) {                                  /* IDLE :17 */
        fish_idle_swim(i);
        if (pdist2_lt(dist, 90) && PL.swimming && !PL.dead) PE(p)->status = 1;
        obj = prey_swims() ? prey(i) : NOONE;
#ifdef PLAY_STATS
        if (obj == NOONE) {                                    /* the host builds: the skipped prey() fails the test */
            int t = prey(i);
            if (t != NOONE && PEN(&PX(t))->swimming && PE(&PX(t))->hp > 0) { fprintf(stderr, "prey_swims\n"); abort(); }
        }
#endif
        if (obj != NOONE && PEN(&PX(obj))->swimming && PE(&PX(obj))->hp > 0) PE(p)->status = 3;
    } else if (PE(p)->status == 2) {                           /* PAUSE :53 */
        CANBITE(p) = 1;
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        else {
            PE(p)->status = 0;
            DIR(p) = RAND(0, 1) * 180;
        }
    } else if (PE(p)->status == 1 && c != NOONE) {             /* ATTACK :63 */
        if (pdist2_lt(dist, 90) && PL.swimming && !PL.dead) {
            double d = point_direction_d(X(i) + 4, Y(i) + 4, X(c), Y(c));
            int a = RAND(0, 1), b = RAND(0, 1);
            double r, si, co;
            DIR(p) = d + a - b;
            r = degtorad_d(DIR(p));
            psincos_cr(r, &si, &co);                           /* (move_dir's angle too: DIR unchanged) */
            if (CP(X(i) + co, Y(i) - si, OBJ_oWater)) move_cs(i, 1, co, si);
        } else {
            PE(p)->status = 2;
            PE(p)->counter = (int16_t)RAND(20, 40);
        }
    } else if (PE(p)->status == 3) {                           /* ATTACK_ENEMY :77 */
        obj = prey(i);
        if (obj != NOONE) {
            if (!PEN(&PX(obj))->swimming || PE(&PX(obj))->hp <= 0) PE(p)->status = 2;
        } else
            PE(p)->status = 2;
        if (PE(p)->status != 2) {
            double d = point_direction_d(X(i) + 4, Y(i) + 4, X(obj) + 8, Y(obj) + 8);
            int a = RAND(0, 1), b = RAND(0, 1);
            double r, si, co;
            DIR(p) = d + a - b;
            r = degtorad_d(DIR(p));
            psincos_cr(r, &si, &co);                           /* (move_dir's angle too: DIR unchanged) */
            if (CP(X(i) + co, Y(i) - si, OBJ_oWater)) move_cs(i, 1, co, si);
            else {
                PE(p)->status = 2;
                PE(p)->counter = (int16_t)RAND(20, 40);
            }
            dist = pdist2(X(i) + 4, Y(i) + 4, X(obj) + 8, Y(obj) + 8);
            if (pdist2_lt(dist, 4)) {
                PE(&PX(obj))->status = 2;
                if (CANBITE(p)) {
                    PE(&PX(obj))->hp -= 1;
                    CANBITE(p) = 0;
                    PE(p)->alarm[0] = 10;
                    scrCreateBlood(i, p->x + PI(4), p->y + PI(4), 1);
                }
            }
        } else
            PE(p)->counter = (int16_t)RAND(20, 40);
    }
    p = &PX(i);
    if (BUBBLETIMER(p) > 0) BUBBLETIMER(p) -= 1;               /* :123 */
    else {
        pin_create(p->x, p->y, OBJ_oBubble);
        BUBBLETIMER(&PX(i)) = (int16_t)RAND(40 - 10, 40 + 10);
    }
    fish_end(i, GSPR_sPiranhaLeft, GSPR_sPiranhaRight);
}

static void deadfish_step(int i)                               /* objects/oDeadFish/Step_0.gml */
{
    struct pin *p = &PX(i);
    int c;
    double dist;                                               /* (the squared distance: pdist2) */
    if (!eview(i, 16, 0)) return;
    if (PE(p)->hp < 1) {                                       /* :4 */
        int k;
        for (k = 0; k < 3; k++) pin_create(PX(i).x + PI(4), PX(i).y + PI(4), OBJ_oBone);
        kill_count(i, &PG.deadfish);
        pin_destroy(i);
    }
    p = &PX(i);
    c = instance_first_p(OBJ_oCharacter);
    dist = pdist2(X(i), Y(i), X(c), Y(c));
    if (PE(p)->status == 0) {                                  /* IDLE :21 */
        fish_idle_swim(i);
        if (pdist2_lt(dist, 90) && PL.swimming) PE(p)->status = 1;
    } else if (PE(p)->status == 2) {                           /* PAUSE :41 */
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        else {
            PE(p)->status = 0;
            DIR(p) = RAND(0, 1) * 180;
        }
    } else if (c != NOONE) {                                   /* :50 */
        if (pdist2_lt(dist, 90) && PL.swimming && !PL.dead) {
            double d = point_direction_d(X(i), Y(i), X(c), Y(c));
            int a = RAND(0, 1), b = RAND(0, 1);
            double r, si, co;
            DIR(p) = d + a - b;
            r = degtorad_d(DIR(p));
            psincos_cr(r, &si, &co);                           /* (move_dir's angle too: DIR unchanged) */
            if (CP(X(i) + co, Y(i) - si, OBJ_oWater)) move_cs(i, 1, co, si);
        } else {
            PE(p)->status = 2;
            PE(p)->counter = (int16_t)RAND(20, 40);
        }
    }
    fish_end(i, GSPR_sDeadFishLeft, GSPR_sDeadFishRight);
}

/* objects/oFishBone/Step_0.gml (after oItem's): direction from the velocity, image_angle = direction */
static void fishbone_step(int i)
{
    struct pin *p;
    extern double patan_deg(double a);
    item_step(i);
    p = &PX(i);
    {
        num xv = PE(p)->xVel, yv = PE(p)->yVel;
        double x = NTOD(xv), y = NTOD(yv);
        if (NGT(xv, 0) && NLT(yv, 0)) DIR(p) = patan_deg(-y / x);
        else if (NLT(xv, 0) && NLT(yv, 0)) DIR(p) = 180 - patan_deg(-y / -x);
        else if (NGT(xv, 0) && NGT(yv, 0)) DIR(p) = patan_deg(y / x);
        else if (NLT(xv, 0) && NGT(yv, 0)) DIR(p) = 180 + patan_deg(y / -x);
        else if (NLT(xv, 0)) DIR(p) = 180;
        else DIR(p) = 0;
        pin_setangle(p, (float)DIR(p));                        /* image_angle: a float */
    }
}

/* ---- oJaws (the mega mouth): objects/oJaws/Step_0.gml ------------------------------------------------------ */
enum { J_IDLE, J_ATTACK, J_PAUSE, J_TURN };

static void jaws_dir_reset(struct pin *p)                      /* if (dir > 90 and dir < 270) dir = 180 else 0 */
{
    DIR(p) = DGT(DIR(p), 90) && DLT(DIR(p), 270) ? 180 : 0;
}

static void jaws_turn_left(int i)                              /* status = TURN; dir = 180; x -= 48; sJawsTurnL */
{
    struct pin *p = &PX(i);
    PE(p)->status = J_TURN;
    DIR(p) = 180;
    pin_setx(p, p->x - PI(48));
    pin_set_sprite(i, GSPR_sJawsTurnL);
    pin_setimg(&PX(i), 0);
}

static void jaws_turn_right(int i)                             /* status = TURN; dir = 0; sJawsTurnR */
{
    struct pin *p = &PX(i);
    PE(p)->status = J_TURN;
    DIR(p) = 0;
    pin_set_sprite(i, GSPR_sJawsTurnR);
    pin_setimg(&PX(i), 0);
}

static void jaws_step(int i)
{
    struct pin *p = &PX(i);
    int pl = PL.idx;
    if (!eview(i, 48, 48)) return;                             /* :1 the view +- 48 */
    if (!CP(X(i) + 8, Y(i) + 16, OBJ_oWater)) PE(p)->hp -= 1;  /* :5 */
    if (PE(p)->hp < 1) {                                       /* :10 */
        int k;
        kill_count(i, &PG.megamouths);
        {
            int yy = RAND(0, 4), xx = RAND(0, 4);
            scrCreateBlood(i, PX(i).x + PI(22 + xx), PX(i).y + PI(14 + yy), 4);
        }
        for (k = 0; k < 4; k++) {
            int yy = RAND(0, 6), xx = RAND(0, 4);
            pin_create(PX(i).x + PI(22 + xx), PX(i).y + PI(14 + yy), OBJ_oBone);
        }
        for (k = 0; k < 4; k++) {
            int obj = pin_create(PX(i).x + PI(16), PX(i).y + PI(16), OBJ_oCrate);
            int a = RAND(0, 3), b = RAND(0, 3);
            PE(&PX(obj))->xVel = NI(a - b);
            PE(&PX(obj))->yVel = NI(-RAND(1, 2));
        }
        pin_destroy(i);
    }
    p = &PX(i);
    /* :34 dist = point_distance(x, y, oPlayer1.x, oPlayer1.y): not read */
    if (PE(p)->status == J_IDLE) {                             /* :36 */
        if (DEQ(DIR(p), 0)) {
            if (CP(X(i) + 18, Y(i) + 16, OBJ_oWater) && !CP(X(i) + 18, Y(i) + 16, OBJ_oSolid)) moveTo(i, N(2), 0, 0, 0);
            else if (collision_rect_p(X(i) - 32, Y(i), X(i), Y(i) + 32, OBJ_oSolid, 0, NOONE) == NOONE) jaws_turn_left(i);
        } else {
            if (CP(X(i) - 2, Y(i) + 16, OBJ_oWater) && !CP(X(i) - 2, Y(i) + 16, OBJ_oSolid)) moveTo(i, N(-2), 0, 0, 0);
            else if (collision_rect_p(X(i) + 16, Y(i), X(i) + 48, Y(i) + 32, OBJ_oSolid, 0, NOONE) == NOONE)
                jaws_turn_right(i);
        }
        p = &PX(i);
        if (!isCollisionBottom(i, 2)) pin_sety(p, p->y + PI(1));   /* :66 */
        if (PL.swimming && !PL.dead) PE(p)->status = J_ATTACK;
    } else if (PE(p)->status == J_PAUSE) {                     /* :76 */
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        else {
            PE(p)->status = J_IDLE;
            jaws_dir_reset(p);
        }
    } else if (PE(p)->status == J_ATTACK && instance_exists_p(OBJ_oPlayer1)) {   /* :85 */
        if (PL.swimming && !PL.dead) {
            int turn = 0;
            if (p->spr == GSPR_sJawsLeft || p->spr == GSPR_sJawsRight)
                DIR(p) = point_direction_d(X(i) + 8, Y(i) + 16, X(pl), Y(pl) - 8);
            if (DLT(X(pl), X(i) + 8)) {
                if (p->spr == GSPR_sJawsRight &&
                    collision_rect_p(X(i) - 32, Y(i), X(i), Y(i) + 32, OBJ_oSolid, 0, NOONE) == NOONE) {
                    jaws_turn_left(i);
                    turn = 1;
                }
            } else {
                if (p->spr == GSPR_sJawsLeft && !CP(X(i) - 2, Y(i) + 16, OBJ_oSolid)) {
                    jaws_turn_right(i);
                    turn = 1;
                }
            }
            p = &PX(i);
            if (!turn) {                                       /* :117 */
                double r = degtorad_d(DIR(p)), si, co, cx, cy;
                psincos_cr(r, &si, &co);                       /* (move_dir's angle too: DIR unchanged) */
                cx = X(i) + co;
                cy = Y(i) - si;
                if (CP(cx, cy, OBJ_oWater) && !CP(cx, cy, OBJ_oSolid)) move_cs(i, 3, co, si);
            }
        } else {
            PE(p)->status = J_IDLE;
            jaws_dir_reset(p);
        }
    }
    p = &PX(i);
    if (BUBBLETIMER(p) > 0) BUBBLETIMER(p) -= 1;               /* :133 */
    else {
        pin_create(p->x, p->y + PI(16), OBJ_oBubble);
        BUBBLETIMER(&PX(i)) = 40;                              /* bubbleTimerMax */
    }
    p = &PX(i);
    if (p->spr == GSPR_sJawsLeft) setCollisionBounds(i, 0, 0, 64, 32);          /* :140 */
    else if (p->spr == GSPR_sJawsRight) setCollisionBounds(i, -48, 0, 16, 32);
}

/* ---- oZombie: objects/oZombie/Step_0.gml ------------------------------------------------------------------ */
static void zombie_step(int i)
{
    struct pin *p;
    int colBot, c;
    double dist;
    pen_parent_step(i);
    p = &PX(i);
    if (!eview(i, 20, 4)) return;
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    PE(p)->yVel += PE(p)->myGrav;
    if (NGT(PE(p)->yVel, PEN(p)->yVelLimit)) PE(p)->yVel = PEN(p)->yVelLimit;
    if (CP(X(i) + 8, Y(i) + 8, OBJ_oSolid)) PE(p)->hp = -999;
    if (PE(p)->hp < 1) {                                       /* :12 */
        scrCreateBlood(i, p->x + PI(8), p->y + PI(8), 1);
        bones_and_skull(PX(i).x + PI(8), PX(i).y + PI(8), 0);
        kill_count(i, &PG.zombies);
        pin_destroy(i);
    }
    p = &PX(i);
    if (isCollisionRight(i, 1)) PE(p)->xVel = N(-1);
    if (isCollisionLeft(i, 1)) PE(p)->xVel = N(1);
    colBot = isCollisionBottom(i, 1);
    PE(p)->colBot = (uint8_t)colBot;
    dist = distance_to_object_p(i, OBJ_oPlayer1);
    if (PE(p)->status == 0) {                                  /* IDLE :46 */
        PE(p)->xVel = 0;
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        else if (DLT(dist, 64)) PE(p)->status = 1;
        if (DLT(dist, 48)) PE(p)->status = 1;
        if (PL.swimming) PE(p)->status = 0;
        if (PE(p)->status == 1) snd_play(SND_xzombie);         /* :53 */
    } else if (PE(p)->status == 2) {                           /* RECOVER :55 */
        if (colBot) {
            PE(p)->status = 0;
            PE(p)->xVel = 0;
            PE(p)->yVel = 0;
            PE(p)->counter = (int16_t)RAND(40, 100);
        }
    } else if (PE(p)->status == 1) {                           /* BOUNCE :65 */
        if (colBot) {
            c = instance_first_p(OBJ_oCharacter);
            if (RAND(1, 4) == 1) {
                PE(p)->yVel = NI(-1 * RAND(2, 4));
                if (DLT(X(c), X(i))) { PE(p)->facing = E_LEFT; PE(p)->xVel = N(-3); }
                else { PE(p)->facing = E_RIGHT; PE(p)->xVel = N(3); }
            } else {
                PE(p)->yVel = NI(-1 * RAND(1, 2));
                if (DLT(X(c), X(i))) { PE(p)->facing = E_LEFT; PE(p)->xVel = N(-1); }
                else { PE(p)->facing = E_RIGHT; PE(p)->xVel = N(1); }
            }
        } else
            PE(p)->status = 2;
    } else if (PE(p)->status != 4) {                           /* DROWNED */
        PE(p)->status = 0;
        PE(p)->xVel = 0;
    }
    if (isCollisionTop(i, 1)) PE(p)->yVel = N(1);              /* :109 */
    if (!colBot) pin_set_sprite(i, GSPR_sZombieJumpL);
    else pin_set_sprite(i, GSPR_sZombieLeft);
}

/* ---- oVampire: objects/oVampire/Step_0.gml ----------------------------------------------------------------- */
/* d is a float's value in [0, 360] (point_direction_d's float, or 0 / 90 / 180 / 270): psincos_cr gives pcos_cr's
   and psin_cr's bits for every such d (tests/sincos, as bat_fly), without cr_trig_dd's double-double series. The host
   builds check the premise */
static void vampire_fly(struct pin *p, double c, double d)
{
    double s, co;
#ifdef PLAY_STATS
    if (!((double)(float)d == d && d >= 0 && d <= 360)) {
        fprintf(stderr, "vampire_fly: d %.17g is not a float in [0, 360]\n", d);
        abort();
    }
#endif
    psincos_cr(degtorad_d(d), &s, &co);
    PE(p)->xVel = ND(c * co);
    PE(p)->yVel = ND(-c * s);
}

static void vampire_step(int i)
{
    struct pin *p;
    int pl = PL.idx;
    double dist, plx, ply;
    pen_parent_step(i);
    p = &PX(i);
    if (!eview(i, 20, 4)) return;
    if (PE(p)->status == E_STUNNED) PE(p)->myGrav = N(0.6);    /* :5 */
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    PE(p)->colLeft = PE(p)->colRight = PE(p)->colBot = PE(p)->colTop = 0;
    if (isCollisionLeft(i, 1)) PE(p)->colLeft = 1;
    if (isCollisionRight(i, 1)) PE(p)->colRight = 1;
    if (isCollisionBottom(i, 1)) PE(p)->colBot = 1;
    if (isCollisionTop(i, 1)) PE(p)->colTop = 1;
    if (!PE(p)->held && PE(p)->status != V_HANG && PE(p)->status != V_FLY) PE(p)->yVel += PE(p)->myGrav;
    if (NGT(PE(p)->yVel, PEN(p)->yVelLimit)) PE(p)->yVel = PEN(p)->yVelLimit;
    if (PE(p)->status >= E_STUNNED) {                          /* :22 */
        if (CP(X(i) + 8, Y(i) + 12, OBJ_oSolid)) {
            pin_create(p->x + PI(8), p->y + PI(8), OBJ_oSmokePuff);
            snd_play(SND_xcavemandie);                         /* :27 */
            pin_destroy(i);
        }
    } else if (!PE(p)->held) {
        if (CP(X(i) + 8, Y(i) + 8, OBJ_oSolid)) {
            pin_create(p->x + PI(8), p->y + PI(8), OBJ_oSmokePuff);
            snd_play(SND_xcavemandie);                         /* :36 */
            pin_destroy(i);
        }
    }
    p = &PX(i);
    if (isCollisionBottom(i, 1) && PE(p)->status != E_STUNNED) PE(p)->yVel = 0;
    if (PE(p)->status != E_DEAD && PE(p)->status != E_STUNNED && PE(p)->hp < 1) PE(p)->status = E_DEAD;
    if (PEN(p)->burning > 0) {                                 /* :49 */
        if (RAND(1, 5) == 1) {
            int yy = RAND(4, 12), xx = RAND(4, 12);
            pin_create(p->x + PI(xx), p->y + PI(yy), OBJ_oBurn);
            p = &PX(i);
        }
        PEN(p)->burning -= 1;
    }
    plx = X(pl);
    ply = Y(pl);
    dist = distance_to_point_p(i, plx - 8, ply - 8);           /* :55 */
    switch (PE(p)->status) {
    case V_IDLE:                                               /* :57 */
        PEN(p)->bounced = 0;
        if (PE(p)->colBot && (CP(X(i) - 1, Y(i), OBJ_oSolid) || CP(X(i) + 16, Y(i), OBJ_oSolid))) {
            PE(p)->yVel = N(-6);
            PE(p)->xVel = PE(p)->facing == E_LEFT ? N(-1) : N(1);
            PE(p)->counter -= 10;
        }
        if (NLT(PE(p)->yVel, 0) && isCollisionTop(i, 1)) PE(p)->yVel = 0;
        if (isCollisionBottom(i, 1) && PE(p)->counter > 0) PE(p)->counter -= 1;
        if (PE(p)->counter < 1) {
            PE(p)->facing = (int16_t)RAND(0, 1);
            PE(p)->status = V_WALK;
        }
        if (DLT(dist, 96)) {
            if (DLT(plx, X(i) + 8)) PE(p)->facing = E_LEFT;
            else if (DGT(plx, X(i) + 8)) PE(p)->facing = E_RIGHT;
            PE(p)->status = V_ATTACK;
        }
        break;
    case V_WALK:                                               /* :88 */
        if (isCollisionLeft(i, 1) || isCollisionRight(i, 1)) PE(p)->facing = PE(p)->facing == E_LEFT ? E_RIGHT : E_LEFT;
        if (!PE(p)->colBot) {
        } else if (RAND(1, 100) == 1) {
            PE(p)->status = V_IDLE;
            PE(p)->counter = (int16_t)RAND(20, 50);
            PE(p)->xVel = 0;
        } else if (PE(p)->facing == E_LEFT) {
            PE(p)->xVel = N(-1.5);
            if (!CP(X(i) - 1, Y(i) + 16, OBJ_oSolid)) {
                PE(p)->status = V_IDLE;
                PE(p)->counter = (int16_t)RAND(20, 50);
                PE(p)->xVel = 0;
                PE(p)->yVel = 0;
            }
        } else if (PE(p)->facing == E_RIGHT) {
            PE(p)->xVel = N(1.5);
            if (!CP(X(i) + 16, Y(i) + 16, OBJ_oSolid)) {
                PE(p)->status = V_IDLE;
                PE(p)->counter = (int16_t)RAND(20, 50);
                PE(p)->xVel = 0;
                PE(p)->yVel = 0;
            }
        }
        if (DLT(dist, 96)) PE(p)->status = V_ATTACK;
        break;
    case V_ATTACK:                                             /* :131 */
        pin_setispd(p, 1);
        if (PE(p)->facing == E_LEFT && isCollisionLeft(i, 4)) {
            if (isCollisionTop(i, 1)) PE(p)->facing = E_RIGHT;
            else PE(p)->status = V_BOUNCE;
        } else if (PE(p)->facing == E_RIGHT && isCollisionRight(i, 4)) {
            if (isCollisionTop(i, 1)) PE(p)->facing = E_LEFT;
            else PE(p)->status = V_BOUNCE;
        } else if (PE(p)->facing == E_LEFT) {
            if (!CP(X(i) - 8, Y(i) + 16, OBJ_oSolid)) PE(p)->status = V_BOUNCE;
        } else if (PE(p)->facing == E_RIGHT) {
            if (!CP(X(i) + 8, Y(i) + 16, OBJ_oSolid)) PE(p)->status = V_BOUNCE;
        }
        if (DLT(dabs(plx - X(i)), 32) && DLT(ply, Y(i) + 8) && !CP(X(i) + 8, Y(i) + 8, OBJ_oWater))
            PE(p)->status = V_FLY;
        PE(p)->xVel = PE(p)->facing == E_LEFT ? N(-4) : N(4);
        break;
    case V_RECOVER:                                            /* :164 */
        if (PE(p)->facing == E_LEFT && isCollisionLeft(i, 1)) {
            PE(p)->facing = E_RIGHT;
            PE(p)->xVel = -PE(p)->xVel;
        } else if (PE(p)->facing == E_RIGHT && isCollisionRight(i, 1)) {
            PE(p)->facing = E_LEFT;
            PE(p)->xVel = -PE(p)->xVel;
        } else if (PE(p)->colBot) {
            PE(p)->status = V_IDLE;
            PE(p)->xVel = 0;
            PE(p)->yVel = 0;
            PE(p)->counter = (int16_t)RAND(40, 100);
        } else if (!CP(X(i) + 8, Y(i) + 8, OBJ_oWater)) {
            if (RAND(1, 100) == 1) PE(p)->status = V_FLY;
            else if (CP(X(i) + 8, Y(i) + 24, OBJ_oWater)) PE(p)->status = V_FLY;
        }
        break;
    case V_BOUNCE:                                             /* :189 */
        if (PE(p)->colBot) PE(p)->yVel = NI(-1 * RAND(3, 6));
        else PE(p)->status = V_RECOVER;
        break;
    case E_STUNNED:                                            /* :200 */
        if (PE(p)->colBot) {
        } else if (NEQ(PE(p)->xVel, 0) && PE(p)->hp > 0) pin_set_sprite(i, GSPR_sVampireStunL);
        else if (PEN(p)->bounced) pin_set_sprite(i, NLT(PE(p)->yVel, 0) ? GSPR_sVampireBounceL : GSPR_sVampireFallL);
        else pin_set_sprite(i, NGT(NABS(PE(p)->xVel), 0) ? GSPR_sVampireDieLL : GSPR_sVampireDieLR);
        if (PE(p)->colBot && !PEN(p)->bounced) {
            PEN(p)->bounced = 1;
            scrCreateBlood(i, p->x + PI(8), p->y + PI(8), 1);  /* bloodless: none */
        }
        if (PE(p)->held || PE(p)->colBot) {
            if (PE(p)->counter > 0) PE(p)->counter -= 1;
            else if (PE(p)->hp > 0) {
                PE(p)->status = V_IDLE;
                if (PE(p)->held) {
                    PE(p)->held = 0;
                    PL.holdItem = NOONE;
                    PL.pickupItemType = T_NONE;
                }
            }
        }
        break;
    case E_DEAD: {                                             /* :241 */
        int obj;
        bones_and_skull(PX(i).x + PI(8), PX(i).y + PI(8), 0);
        kill_count(i, &PG.vampires);
        snd_play(SND_xcavemandie);                             /* :256 */
        obj = pin_create(PX(i).x + PI(8), PX(i).y + PI(8), OBJ_oCapePickup);
        PE(&PX(obj))->cost = 0;
        PE(&PX(obj))->forSale = 0;
        pin_destroy(i);
        p = &PX(i);
        break;
    }
    case V_HANG:                                               /* :262 */
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        if (!PL.dead && !PL.swimming && ((DLT(dist, 90) && DGT(ply, Y(i) + 16)) || !CP(X(i) + 8, Y(i) - 1, OBJ_oSolid))) {
            PE(p)->status = V_FLY;
            snd_play(SND_xbat);                                /* :271 */
        }
        break;
    case V_FLY:
        if (pl != NOONE && !PL.swimming && !PL.dead) {         /* :274 */
            PE(p)->xVel = 0;
            PE(p)->yVel = 0;
            if (DLT(dist, 160)) {
                double x = X(i), y = Y(i);
                DIR(p) = point_direction_d(x + 8, y + 8, plx, ply);
                if (isCollisionRight(i, 1) && DGT(plx, x + 8)) DIR(p) = DLT(ply, y + 8) ? 90 : 270;
                if (isCollisionLeft(i, 1) && DLT(plx, x + 8)) DIR(p) = DLT(ply, y + 8) ? 90 : 270;
                if (isCollisionTop(i, 1) && DLT(ply, y + 8) && DGT(dabs(plx - x), 8)) DIR(p) = DLT(plx, x + 8) ? 180 : 0;
                if (isCollisionBottom(i, 1) && DGT(ply, y + 8) && DGT(dabs(plx - x), 8)) DIR(p) = DLT(plx, x + 8) ? 180 : 0;
                if (CP(x + 8, y + 16, OBJ_oWater) && DGT(DIR(p), 180) && DLT(DIR(p), 360)) DIR(p) = 90;
                if (!CP(x, y + 12, OBJ_oWater) || DLT(ply, y)) vampire_fly(p, 2, DIR(p));
            } else {
                if (CP(X(i) + 8, Y(i) - 1, OBJ_oSolid)) PE(p)->status = V_HANG;
                else {
                    DIR(p) = 90;
                    vampire_fly(p, 2, DIR(p));
                }
            }
            PE(p)->facing = DLT(plx, X(i) + 8) ? E_LEFT : E_RIGHT;
            if (PE(p)->colBot || CP(X(i), Y(i), OBJ_oWater)) PE(p)->status = V_IDLE;
        } else {                                               /* :333 */
            PE(p)->xVel = 0;
            PE(p)->yVel = 0;
            if (CP(X(i) + 8, Y(i) - 1, OBJ_oSolid)) PE(p)->status = V_HANG;
            else {
                DIR(p) = 90;
                vampire_fly(p, 1, DIR(p));
            }
        }
        break;
    }
    if (PE(p)->status >= E_STUNNED) {                          /* :347 */
        scrCheckCollisions(i);
        if (NEQ(PE(p)->xVel, 0) && NEQ(PE(p)->yVel, 0) && PE(p)->hp < 1) PE(p)->status = E_DEAD;
    }
    if (NGT(PE(p)->xVel, 0)) PE(p)->xVel -= N(0.1);            /* :356 */
    if (NLT(PE(p)->xVel, 0)) PE(p)->xVel += N(0.1);
    if (NLT(NABS(PE(p)->xVel), N(0.5))) PE(p)->xVel = 0;
    if (PE(p)->status == V_HANG) pin_set_sprite(i, GSPR_sVampireBatHang);
    else if (PE(p)->status < E_STUNNED && PE(p)->status != V_THROW) {
        if (PE(p)->status == V_FLY) pin_set_sprite(i, GSPR_sVampireBatLeft);
        else if (PE(p)->status == V_RECOVER) pin_set_sprite(i, GSPR_sVampireJumpL);
        else if (NGT(NABS(PE(p)->xVel), 0)) pin_set_sprite(i, GSPR_sVampireRunL);
        else pin_set_sprite(i, GSPR_sVampireLeft);
    }
    if (PE(p)->held) pin_set_sprite(i, PE(p)->hp > 0 ? GSPR_sVampireHeldL : GSPR_sVampireDHeldL);
}

/* objects/oVampire/Collision_oCharacter.gml */
static void vampire_hit_player(int i, int c)
{
    struct pin *p = &PX(i), *o = &PX(c);
    if (DGT(dabs(PTOD(o->x) - (X(i) + 8)), 8)) {
    } else if (!PL.dead && (PL.state == JUMPING || PL.state == FALLING) && DLT(PTOD(o->y), Y(i) + 5) && !PL.swimming) {
        if (PE(p)->status < E_STUNNED) {
            PE(o)->yVel = N(-6) - NMUL(N(0.2), PE(o)->yVel);
            if (PG.hasSpikeShoes) PE(p)->hp -= (int16_t)(3 * (PL.fallTimer / 16 + 1));   /* bloodless: no oBlood */
            else PE(p)->hp -= (int16_t)(1 * (PL.fallTimer / 16 + 1));
            PL.fallTimer = 0;
            PEN(p)->countsAsKill = 1;
            PE(p)->status = E_STUNNED;
            PE(p)->counter = PEN(p)->stunTime;
            PE(p)->yVel = N(-6);
            if (DLT(PTOD(o->x), X(i) + 8)) PE(p)->xVel += N(1);
            else PE(p)->xVel -= N(1);
            pin_setispd(p, (img_t)0.5);
            snd_play(SND_xhit);                                /* :21 */
        }
    } else if (PL.invincible == 0) {
        if (PE(p)->status < E_STUNNED) {
            PL.blink = 30;
            PL.invincible = 30;
            if (DLT(PTOD(o->y), Y(i))) PE(o)->yVel = N(-6);
            PE(o)->xVel = DLT(PTOD(o->x), X(i)) ? N(-6) : N(6);
            pin_create(o->x, o->y, OBJ_oBlood);
            if (PG.plife > 0) PG.plife -= 1;
            snd_play(SND_xhurt);                               /* :39 */
        }
    }
}

/* objects/oVampire/Collision_oWhip.gml, Collision_oWhipPre.gml */
static void vampire_whipped(int i, int w)
{
    struct pin *p = &PX(i);
    if (PE(p)->status < E_STUNNED || PX(w).obj == OBJ_oSlash) {         /* other.type == "Machete" */
        PE(p)->hp -= (int16_t)(PX(w).obj == OBJ_oSlash ? 2 : 1);       /* other.damage */
        scrCreateBlood(i, (pos)(X(i) + sprw(i) / 2.0), (pos)(Y(i) + sprh(i) / 2.0), 1);   /* bloodless: none */
        PEN(p)->countsAsKill = 1;
        PE(p)->status = E_STUNNED;
        PE(p)->counter = PEN(p)->stunTime;
        PE(p)->yVel = N(-3);
        PE(p)->xVel = DLT(X(w), X(i) + 8) ? N(2) : N(-2);
        pin_setispd(p, (img_t)0.5);
        snd_play(SND_xhit);                                    /* :12 */
    }
}

/* ---- oGhost ------------------------------------------------------------------------------------------------ */
static void ghost_step(int i)                                  /* objects/oGhost/Step_0.gml */
{
    struct pin *p = &PX(i);
    int c;
    if (PE(p)->hp < 1) {
        PG.kills += 1;
        pin_destroy(i);
    }
    p = &PX(i);
    DIR(p) = 0;
    c = instance_first_p(OBJ_oCharacter);
    if (PE(p)->status == 1) {                                  /* ATTACK :15 */
        double r, si, co;
        DIR(p) = point_direction_d(X(i) + 8, Y(i) + 8, X(c), Y(c));
        r = degtorad_d(DIR(p));
        psincos_cr(r, &si, &co);                               /* (pcos_cr's, psin_cr's bits) */
        pin_setx(p, PADDV(p->x, ND(1 * co)));
        pin_sety(p, PADDV(p->y, ND(-1 * si)));
        if (DLT(X(c), X(i) + 8)) {
            if (p->spr == GSPR_sGhostRight) pin_set_sprite(i, GSPR_sGhostTurnLeft);
        } else {
            if (p->spr == GSPR_sGhostLeft) pin_set_sprite(i, GSPR_sGhostTurnRight);
        }
    }
}

static void ghost_hit_player(int i, int c)                     /* objects/oGhost/Collision_oCharacter.gml */
{
    struct pin *p = &PX(i);
    if (PL.invincible != 0 || play_god) return;
    bones_and_skull(PX(c).x, PX(c).y, -2);                     /* :6-12 (global.enemyDeaths: statistics) */
    pin_setvisible(&PX(c), 0);
    PL.invincible = 9999;
    PL.bounced = 1;
    PG.plife = -99;
    snd_play(SND_xdie);                                        /* :17 */
    PG.drawHUD = 0;
    if (PL.holdItem != NOONE) {                                /* :20 */
        struct pin *h = &PX(PL.holdItem);
        PE(h)->held = 0;
        PE(h)->xVel = PE(&PX(i))->facing == E_LEFT ? N(-2) : N(2);   /* the ghost's facing */
        PE(h)->yVel = N(-4);
        PL.holdItem = NOONE;
    }
    p = &PX(i);
    PE(p)->status = 0;                                         /* IDLE */
    pin_setispd(p, (img_t)0.2);
    pin_set_sprite(i, GSPR_sGhostDisappear);
    snd_play(SND_xghost);                                      /* :32 */
}

/* ---- oCape: objects/oCape/Step_0.gml ----------------------------------------------------------------------- */
static void cape_step(int i)
{
    struct pin *p = &PX(i);
    int pl = PL.idx;
    if (instance_exists_p(OBJ_oTransition) && !instance_exists_p(OBJ_oPDummy)) {
        pin_destroy(i);
    } else if (instance_exists_p(OBJ_oPDummy)) {
        int d = instance_first_p(OBJ_oPDummy);
        int s = PX(d).spr;
        if (s == GSPR_sPExit || s == GSPR_sDamselExit || s == GSPR_sTunnelExit) {
            pin_setxy(p, PX(d).x, PX(d).y + PI(4));
            pin_set_sprite(i, GSPR_sCapeBack);
            pin_setdepth(p, 0);
        } else {
            pin_setxy(p, PX(d).x - PI(4), PX(d).y - PI(2));
            if (s == GSPR_sRunLeft || s == GSPR_sDamselRunL || s == GSPR_sTunnelRunL) pin_set_sprite(i, GSPR_sCapeRight);
            else pin_set_sprite(i, GSPR_sCapeDR);
            pin_setdepth(p, 100);
        }
    } else if ((PL.state == CLIMBING || PX(pl).spr == GSPR_sPExit || PX(pl).spr == GSPR_sDamselExit ||
                PX(pl).spr == GSPR_sTunnelExit) && !PL.whipping) {
        pin_setxy(p, PX(pl).x, PX(pl).y + PI(4));
        pin_set_sprite(i, GSPR_sCapeBack);
        pin_setdepth(p, 0);
    } else {
        int cape = instance_first_p(OBJ_oCape);                /* oCape.open */
        int open = CAPEOPEN(&PX(cape));
        if (PL.facing == RIGHT) {
            pin_setxy(p, PX(pl).x - PI(4), PX(pl).y - PI(2));
            if (open) pin_set_sprite(i, GSPR_sCapeUR);
            else if (NGT(NABS(PE(&PX(pl))->xVel), 0)) pin_set_sprite(i, GSPR_sCapeRight);
            else pin_set_sprite(i, GSPR_sCapeDR);
        } else {
            pin_setxy(p, PX(pl).x + PI(4), PX(pl).y - PI(2));
            if (open) pin_set_sprite(i, GSPR_sCapeUL);
            else if (NGT(NABS(PE(&PX(pl))->xVel), 0)) pin_set_sprite(i, GSPR_sCapeLeft);
            else pin_set_sprite(i, GSPR_sCapeDL);
        }
        pin_setdepth(p, 100);
    }
    if (pl != NOONE && PX(pl).alive && !PX(pl).visible) pin_destroy(i);
}

/* oCape.open (the first oCape) */
static void cape_set_open(int v)
{
    int c = instance_first_p(OBJ_oCape);
    if (c != NOONE) CAPEOPEN(&PX(c)) = (uint8_t)v;
}
static int cape_open(void)
{
    int c = instance_first_p(OBJ_oCape);
    return c != NOONE && CAPEOPEN(&PX(c));
}

/* ---- oRubblePiece Step (oDrip): objects/oRubblePiece/Step_0.gml ----------------------------------------------- */
static void rubblepiece_step(int i)
{
    struct pin *p = &PX(i);
    double x, y;
    pos px, py;
    pin_setx(p, PADDV(p->x, PE(p)->xVel));
    pin_sety(p, PADDV(p->y, PE(p)->yVel));
    PE(p)->yVel += PE(p)->yAcc;
    px = p->x;
    py = p->y;
    x = X(i);
    y = Y(i);
    if (collision_point_any(x, y, OBJ_oWaterSwim, 0, NOONE)) pswamp_world(1041, i, 0);
    else if (collision_point_any(x, y, OBJ_oLava, 0, NOONE)) pin_destroy(i);
    if (collision_point_any(x, y, OBJ_oSolid, 0, NOONE)) pin_destroy(i);
    view_read();
    if (PLTI(px, PW.xview - 32) || PGTI(px, PW.xview + 320 + 32) || PLTI(py, PW.yview - 32) || PGTI(py, PW.yview + 240 + 32))
        pin_destroy(i);
}

/* ---- the events ------------------------------------------------------------------------------------------- */
int pswamp_ev(int ev, int i, int arg)
{
    struct pin *p = &PX(i);
    int o = p->obj;
    switch (ev) {
    case FEV_CREATE:
        return create(i, arg);
    case FEV_STEP:
        switch (o) {
        case OBJ_oPiranha: piranha_step(i); return 1;
        case OBJ_oDeadFish: deadfish_step(i); return 1;
        case OBJ_oZombie: zombie_step(i); return 1;
        case OBJ_oVampire: vampire_step(i); return 1;
        case OBJ_oFishBone: fishbone_step(i); return 1;
        case OBJ_oGhost: ghost_step(i); return 1;
        case OBJ_oCape: cape_step(i); return 1;
        case OBJ_oDrip: rubblepiece_step(i); return 1;
        case OBJ_oJaws: jaws_step(i); return 1;
        case OBJ_oBubble:                                      /* objects/oBubble/Step_0.gml */
            pin_sety(p, PADDV(p->y, PE(p)->yVel));
            if (!CP(X(i), Y(i), OBJ_oWater)) pin_destroy(i);
            return 1;
        case OBJ_oGrave:                                       /* objects/oGrave/Step_0.gml */
            if (eview(i, 20, 4) && !CP(X(i), Y(i) + 16, OBJ_oSolid)) pin_destroy(i);
            return 1;
        }
        return 0;
    case FEV_ALARM:
        if (o == OBJ_oPiranha && arg == 0) { CANBITE(p) = 1; return 1; }           /* objects/oPiranha/Alarm_0.gml */
        if (o == OBJ_oVampire && arg == 0) { PEN(p)->whipped = 0; return 1; }      /* objects/oVampire/Alarm_0.gml */
        return 0;
    case FEV_ANIMEND:
        if (o == OBJ_oBubble) { pin_destroy(i); return 1; }   /* objects/oBubble/Other_7.gml */
        if (o == OBJ_oJaws) {                                  /* objects/oJaws/Other_7.gml */
            if (p->spr == GSPR_sJawsTurnL) {
                pin_set_sprite(i, GSPR_sJawsLeft);
                PE(p)->status = J_PAUSE;
                PE(p)->counter = 40;
            } else if (p->spr == GSPR_sJawsTurnR) {
                pin_set_sprite(i, GSPR_sJawsRight);
                PE(p)->status = J_PAUSE;
                PE(p)->counter = 40;
                pin_setx(&PX(i), PX(i).x + PI(48));
            }
            return 1;
        }
        if (o == OBJ_oGhost) {                                 /* objects/oGhost/Other_7.gml */
            if (p->spr == GSPR_sGhostTurnRight) pin_set_sprite(i, GSPR_sGhostRight);
            else if (p->spr == GSPR_sGhostTurnLeft) pin_set_sprite(i, GSPR_sGhostLeft);
            else if (p->spr == GSPR_sGhostDisappear) pin_destroy(i);
            return 1;
        }
        return 0;
    case FEV_COLLISION: {
        int oo = PX(arg).obj;
        if (o == OBJ_oVampire) {
            if (obj_is(oo, OBJ_oCharacter)) vampire_hit_player(i, arg);
            else if (obj_is(oo, OBJ_oBlood)) {                 /* objects/oVampire/Collision_oBlood.gml */
                PE(p)->hp += 1;
                pin_destroy(arg);
            } else vampire_whipped(i, arg);
            return 1;
        }
        if (o == OBJ_oGhost) {
            if (obj_is(oo, OBJ_oCharacter)) ghost_hit_player(i, arg);
            return 1;                                          /* Collision_oWhip: empty */
        }
        return 0;
    }
    case FEV_DESTROY:
        if (o == OBJ_oGrave) {                                 /* objects/oGrave/Destroy_0.gml (no oSolid's) */
            static const int16_t objs[3] = { OBJ_oRubble, OBJ_oRubbleSmall, OBJ_oRubbleSmall };
            static const int16_t sprs[3] = { GSPR_sRubbleTan, GSPR_sRubbleTanSmall, GSPR_sRubbleTanSmall };
            int k;
            for (k = 0; k < 3; k++) {
                int ya = RAND(0, 8), yb = RAND(0, 8);
                int xa = RAND(0, 8), xb = RAND(0, 8);
                int r = pin_create(PX(i).x + PI(8 + xa - xb), PX(i).y + PI(8 + ya - yb), objs[k]);
                pin_set_sprite(r, sprs[k]);
            }
            return 1;
        }
        if (o == OBJ_oWaterSwim) {                             /* objects/oWaterSwim/Destroy_0.gml */
            int k;
            for (k = 0; k < 3; k++) {
                int yy = RAND(0, 16), xx = RAND(0, 16);
                pin_create(PX(i).x + PI(xx), PX(i).y + PI(yy), OBJ_oDrip);
            }
            return 1;
        }
        return 0;
    }
    return 0;
}

/* ---- the player's sites (pplayer.c) ---------------------------------------------------------------------- */
int pswamp_player(int site, int i, int arg)
{
    struct pin *p = &PX(i);
    (void)arg;
    switch (site) {
    case 2002: case 2003: case 2007:                           /* characterStepEvent :197 :301 :510 */
        cape_set_open(0);
        return 1;
    case 2005:                                                 /* characterStepEvent :334: the cape opens / closes */
        cape_set_open(!cape_open());
        return 1;
    case 2009:                                                 /* characterStepEvent :764: under water */
        if (instance_exists_p(OBJ_oCape)) cape_set_open(0);
        if (PL.state == FALLING && NGT(PE(p)->yVel, 0)) PL.yFric = N(0.5);
        else if (!collision_point_any(X(i), Y(i) - 9, OBJ_oWater, 0, NOONE)) PL.yFric = N(1);
        else PL.yFric = N(0.9);
        return 1;
    case 2011:                                                 /* characterStepEvent :886 */
        if (cape_open()) PL.yFric = N(0.5);
        return 1;
    case 2030:                                                 /* oPlayer1 Step :12 */
        if (!instance_exists_p(OBJ_oCape)) pin_create(p->x, p->y, OBJ_oCape);
        if (cape_open()) PL.fallTimer = 0;
        return 1;
    case 2032:                                                 /* oPlayer1 Step :177 */
        if (!PL.swimming) {
            pin_create(p->x, p->y - PI(8), OBJ_oSplash);
            PL.swimming = 1;
            snd_play(SND_xsplash);                             /* :183 */
        }
        return 1;
    case 2036:                                                 /* oPlayer1 Step :303 */
        PL.fallTimer = 0;
        if (PL.bubbleTimer > 0) PL.bubbleTimer -= 1;
        else {
            pin_create(p->x, p->y - PI(4), OBJ_oBubble);
            PL.bubbleTimer = PL.bubbleTimerMax;
        }
        return 1;
    }
    PUNTR(site);
    return 0;
}

/* ---- the items' and world's sites (pobj.c, pitem.c) ------------------------------------------------------- */
/* oGame Step :71-126: with oWater, the surface sprites and the water that has lost its walls */
static int32_t waterLoopSafety;
static void check_water(void)
{
    int16_t w[PIN_MAX];
    int n = pw_with(OBJ_oWater, w, PIN_MAX), k, waterCounter = 0;
    for (k = 0; k < n; k++) {
        int j = w[k], obj, lava;
        double x, y;
        if (!PX(j).alive) continue;
        if (isRoomIs(R_rOlmec)) continue;
        x = X(j);
        y = Y(j);
        if (!(y < 512)) continue;
        lava = obj_is(PX(j).obj, OBJ_oLava);                   /* type == "Lava" */
        if (!CP(x, y - 16, OBJ_oSolid) && !CP(x, y - 16, OBJ_oWater))
            pin_set_sprite(j, lava ? GSPR_sLavaTop : GSPR_sWaterTop);
        obj = instance_place_p(j, x - 16, y, OBJ_oWater);
        if (obj != NOONE && (PX(obj).spr == GSPR_sWaterTop || PX(obj).spr == GSPR_sLavaTop))
            pin_set_sprite(j, lava ? GSPR_sLavaTop : GSPR_sWaterTop);
        obj = instance_place_p(j, x + 16, y, OBJ_oWater);
        if (obj != NOONE && (PX(obj).spr == GSPR_sWaterTop || PX(obj).spr == GSPR_sLavaTop))
            pin_set_sprite(j, lava ? GSPR_sLavaTop : GSPR_sWaterTop);
        if ((!CP(x - 16, y, OBJ_oSolid) && !CP(x - 16, y, OBJ_oWater)) ||
            (!CP(x + 16, y, OBJ_oSolid) && !CP(x + 16, y, OBJ_oWater)) ||
            (!CP(x, y + 16, OBJ_oSolid) && !CP(x, y + 16, OBJ_oWater))) {
            pin_destroy(j);
            waterCounter += 1;
        }
        waterLoopSafety += 1;
        if (waterLoopSafety > 100000) G.checkWater = 0;
    }
    if (waterCounter == 0) {
        G.checkWater = 0;
        waterLoopSafety = 0;                                   /* the next step's else branch */
    }
}

int pswamp_world(int site, int i, int arg)
{
    struct pin *p = &PX(i);
    (void)arg;
    switch (site) {
    case 1012: {                                               /* oSolid Destroy :19: the grave on top */
        int obj = instance_place_p(i, X(i) + 8, Y(i) - 1, OBJ_oGrave);
        if (obj != NOONE) pin_destroy(obj);
        return 1;
    }
    case 1041:                                                 /* oRubblePiece Step :5: in water */
        if (p->obj == OBJ_oDrip) pin_destroy(i);
        else if (p->obj == OBJ_oLeaf) {
            PE(p)->yVel = 0;
            pin_set_sprite(i, GSPR_sLeafStill);
        } else
            PE(p)->yVel = N(0.2);
        return 1;
    case 1052: {                                               /* oGame Step :59: the ghost */
        view_read();
        if (DGT(X(PL.idx), PW.room_w / 2.0)) pin_create(PI(PW.xview + 320 + 8), PI(PW.yview + 120), OBJ_oGhost);
        else pin_create(PI(PW.xview - 32), PI(PW.yview + 120), OBJ_oGhost);
        PG.ghostExists = 1;
        return 1;
    }
    case 1053:                                                 /* oGame Step :64 */
        check_water();
        return 1;
    case 8001:                                                 /* oFlareCrate Step :2: in water */
        pin_create(p->x, p->y, OBJ_oSplash);
        snd_play(SND_xsplash);                                 /* :5 */
        p = &PX(i);
        if (PE(p)->held) {
            PL.holdItem = NOONE;
            PL.pickupItemType = T_NONE;
            PE(p)->held = 0;
        }
        pin_create(p->x, p->y, OBJ_oPoof);
        pin_destroy(i);
        return 1;
    }
    PUNTR(site);
    return 0;
}

/* ---- the enemy sites (penemy.c) ---------------------------------------------------------------------------- */
int pswamp_enemy(int site, int e, int arg)
{
    struct pin *o = &PX(e);
    switch (site) {
    case 5006:                                                 /* oEnemy Collision_oCharacter :55 */
        if (o->obj == OBJ_oPiranha) {
            scrCreateBlood(e, o->x + PI(4), o->y + PI(4), 1);
            return 1;
        }
        return o->obj == OBJ_oVampire;                         /* bloodless (and its own collision event) */
    case 5016:                                                 /* oItem Step :243: vampires are weak to stakes */
        if (o->obj != OBJ_oVampire) return 0;
        if (PE(o)->status != 98) PE(o)->hp -= 3;
        return 1;
    }
    return 0;
}
