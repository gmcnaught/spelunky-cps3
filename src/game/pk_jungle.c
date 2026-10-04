/* P7 package A (jungle): docs/CONTENT.md §2. Translated statement for statement from refs/hd/src/objects/<obj>/<event>.gml
 * (line numbers in comments): oFrog, oFireFrog, oFireFrogBomb, oManTrap, oMonkey, oScarab, oTree, oTreeBranch,
 * oLeaves, oLeaf, oLush, oSpearTrapBottom / Top / Lit, oSpearsLeft, oTrapBlock, oTikiTorch; the player's monkey and
 * spear sites (oPlayer1 Step :894, characterStepEvent :830 / :855, oPlayer1 Step :1598) and oEnemy Step's fire frog
 * in water, spears and man-trap branches. Overrides the pcontent.h weak defaults.
 * GML keeps running after instance_destroy() inside an event: so does this code (RNG draws included). */
#include "pint.h"
#include "penemy.h"
#include "pcontent.h"
#include "../snd/sndgame.h"
#include "pmath.h"

enum { E_STUNNED = 98, E_DEAD = 99, E_LEFT = 0, E_RIGHT = 1 };
/* oManTrap statuses */
enum { MT_IDLE = 0, MT_WALK = 1, MT_SLEEPY = 96, MT_EATING = 97 };
/* oFrog / oFireFrog / oMonkey statuses */
enum { F_IDLE = 0, F_BOUNCE = 1, F_RECOVER = 2, F_WALK = 3, F_DROWNED = 4, M_HANG = 5, M_CLIMB = 6, M_GRAB = 7 };

/* the variables of these enemies that struct pin_en has no field for, by pin_en record */
static struct jx {
    double grabX, grabY;                 /* oMonkey */
    int16_t grabCounter, vineCounter, throwCounter, dir;
    uint8_t ateShopkeeper;               /* oManTrap */
} jx[EN_MAX];
static struct jx *JX(int i) { return &jx[pin_ext[PX(i).ext].en]; }
/* oFireFrogBomb.swimming, by pin_ext record */
static uint8_t ffb_swim[EXT_MAX];

static double X(int i) { return PTOD(PX(i).x); }
static double Y(int i) { return PTOD(PX(i).y); }
static int CP(double x, double y, int obj) { return collision_point_p(x, y, obj, 0, NOONE) != NOONE; }
static int CPn(double x, double y, int obj, int self) { return collision_point_p(x, y, obj, 1, self) != NOONE; }
static int sprw(int i) { int s = PX(i).spr; return s >= 0 ? (int)(psprite[s].w * PX(i).xscale) : 0; }
static int sprh(int i) { int s = PX(i).spr; return s >= 0 ? (int)(psprite[s].h * PX(i).yscale) : 0; }
static double dabs(double d) { return d < 0 ? -d : d; }
static int pl(void) { return PL.idx; }

/* x > xview - l and x < xview + 320 + r (y likewise) */
static int eview(int i, int l, int r)
{
    double x, y;
    int32_t ix, iy;
    view_read();
    if (pin_xy_int(i, &ix, &iy))          /* whole x, y: GML's compare of two ints is their order (as inview) */
        return ix > PW.xview - l && ix < PW.xview + 320 + r && iy > PW.yview - l && iy < PW.yview + 240 + r;
    x = X(i); y = Y(i);
    return DGT(x, PW.xview - l) && DLT(x, PW.xview + 320 + r) && DGT(y, PW.yview - l) && DLT(y, PW.yview + 240 + r);
}

static int isCollisionSolid(int i)
{
    double lb, tb, rb, bb;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_rect_p(lb, tb, rb - 1, bb - 1, OBJ_oSolid, 1, i) != NOONE;
}

static void make_active(struct pin *p) { PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0; }

/* instance_create(x + rand(0, 16), y - 8 + rand(0, 16), oLeaf): y's number first */
static void leaf_at(int i)
{
    int yy = RAND(0, 16), xx = RAND(0, 16);
    pin_create(PX(i).x + PI(xx), PX(i).y - PI(8) + PI(yy), OBJ_oLeaf);
}

/* instance_create(x+8+rand(0,k)-rand(0,k), y+8+rand(0,k)-rand(0,k), obj): y's numbers first */
static int rubble_at(int i, int obj, int k, int spr)
{
    struct pin *p = &PX(i);
    int ya = RAND(0, k), yb = RAND(0, k);
    int xa = RAND(0, k), xb = RAND(0, k);
    int r = pin_create(p->x + PI(8 + xa - xb), p->y + PI(8 + ya - yb), obj);
    if (spr >= 0) pin_set_sprite(r, spr);
    return r;
}

static void three_rubble(int i, int big, int small)
{
    rubble_at(i, OBJ_oRubble, 8, big);
    rubble_at(i, OBJ_oRubbleSmall, 8, small);
    rubble_at(i, OBJ_oRubbleSmall, 8, small);
}

/* gold = instance_create(...rand(0,4)...); gold.xVel = rand(0,3) - rand(0,3); gold.yVel = rand(2,4) * 1 */
static void gold_drop(int i, int obj)
{
    int g = rubble_at(i, obj, 4, -1);
    int a = RAND(0, 3), b = RAND(0, 3);
    PE(&PX(g))->xVel = NI(a - b);
    PE(&PX(g))->yVel = NI(RAND(2, 4) * 1);
}

/* ---- Create ------------------------------------------------------------------------------------------------ */
static int gen_facing(int fromgen) { return fromgen ? play_gen_inst->facing : RAND(0, 1); }
static int gen_swimming(int i, int fromgen)
{
    return fromgen ? (play_gen_inst->flags & IF_SWIMMING) != 0 : CP(X(i), Y(i), OBJ_oWater);
}

/* objects/oFrog/Create_0.gml, objects/oFireFrog/Create_0.gml (the same) */
static void frog_create(int i, int fromgen)
{
    struct pin *p = &PX(i);
    pen_enemy_create(i);
    make_active(p);
    setCollisionBounds(i, 4, 4, 12, 16);
    PE(p)->xVel = 0;
    PE(p)->yVel = 0;
    PE(p)->myGrav = N(0.2);
    PEN(p)->myGravNorm = N(0.2);
    p->ispd = (img_t)0.4;
    p->type = p->obj == OBJ_oFireFrog ? T_FIREFROG : T_FROG;         /* "Frog" / "Fire Frog" */
    PE(p)->hp = 1;
    p->invincible = 0;
    PE(p)->facing = (int16_t)gen_facing(fromgen);
    PE(p)->status = F_IDLE;
    PE(p)->counter = 0;
    if (gen_swimming(i, fromgen)) PEN(p)->swimming = 1;
}

static void mantrap_create(int i)                                    /* objects/oManTrap/Create_0.gml */
{
    struct pin *p = &PX(i);
    pen_enemy_create(i);
    make_active(p);
    setCollisionBounds(i, 2, 0, sprw(i) - 2, sprh(i));
    PE(p)->xVel = N(2.5);
    p->ispd = (img_t)0.5;
    p->type = T_MANTRAP;                                              /* "ManTrap" */
    PE(p)->hp = 3;
    p->invincible = 0;
    PE(p)->status = MT_IDLE;
    JX(i)->ateShopkeeper = 0;
    PEN(p)->bounced = 0;
    PEN(p)->edead = 0;                                                /* dead = false */
    PE(p)->counter = 0;
    PE(p)->facing = E_RIGHT;
    PE(p)->colLeft = PE(p)->colRight = PE(p)->colBot = PE(p)->colTop = 0;
}

static void monkey_create(int i, int fromgen)                        /* objects/oMonkey/Create_0.gml */
{
    struct pin *p = &PX(i);
    struct jx *j;
    pen_enemy_create(i);
    p->type = T_MONKEY;                                               /* "Monkey" */
    make_active(p);
    setCollisionBounds(i, 4, 6, 12, 16);
    PE(p)->xVel = 0;
    PE(p)->yVel = 0;
    PE(p)->myGrav = N(0.2);
    p->ispd = (img_t)0.4;
    PE(p)->hp = 1;
    p->invincible = 0;
    PE(p)->facing = (int16_t)gen_facing(fromgen);
    PE(p)->status = M_HANG;
    j = JX(i);
    j->grabX = j->grabY = 0;
    j->grabCounter = 0;
    j->dir = 0;
    PE(p)->counter = 0;
    j->vineCounter = 0;
    j->throwCounter = 60;
    if (gen_swimming(i, fromgen)) PEN(p)->swimming = 1;
}

static void scarab_create(int i, int fromgen)                        /* objects/oScarab/Create_0.gml */
{
    struct pin *p = &PX(i);
    pen_enemy_create(i);
    p->type = T_ENONE;                                                /* no type: oEnemy's "NONE" */
    p->ispd = (img_t)0.5;
    setCollisionBounds(i, 4, 4, 12, 12);
    PE(p)->xVel = 0;
    PE(p)->yVel = 0;
    PE(p)->xAcc = N(0.2);
    PE(p)->yAcc = N(0.2);
    PE(p)->counter = (int16_t)(fromgen ? play_gen_inst->counter : RAND(10, 30));
    if (G.levelType == 0) PE(p)->value = 4000;
    else if (G.levelType == 1) PE(p)->value = 8000;
    else if (G.levelType == 3) PE(p)->value = 12000;
    PE(p)->hp = 1;
    p->invincible = 0;
    PE(p)->status = 0;
}

static int jungle_create(int i, int fromgen)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oFrog: case OBJ_oFireFrog: frog_create(i, fromgen); return 1;
    case OBJ_oManTrap: mantrap_create(i); return 1;
    case OBJ_oMonkey: monkey_create(i, fromgen); return 1;
    case OBJ_oScarab: scarab_create(i, fromgen); return 1;
    case OBJ_oFireFrogBomb:                                          /* objects/oFireFrogBomb/Create_0.gml */
        create_item(p);
        p->type = T_OTHER;
        make_active(p);
        setCollisionBounds(i, -6, -4, 6, 8);
        PE(p)->alarm[1] = 120;
        PE(p)->heavy = 1;
        ffb_swim[p->ext] = 0;
        return 1;
    case OBJ_oLeaf:                                                  /* oDrawnSprite, oRubblePiece, oLeaf Create */
        p->type = T_OTHER;
        PE(p)->xVel = 0;
        PE(p)->yVel = N(0.4);
        PE(p)->yAcc = N(0.01);
        p->ispd = (img_t)0.2;
        return 1;
    case OBJ_oTikiTorch: p->ispd = (img_t)0.5; return 1;
    case OBJ_oSpearsLeft: p->type = T_NONE; return 1;                /* oDrawnSprite's Create (type = "") */
    case OBJ_oSpearTrapTop: case OBJ_oSpearTrapLit: case OBJ_oSpearTrapBottom:
        if (!fromgen) return 0;                                      /* only generated */
        PE(p)->fired = 0;
        p->invincible = 0;
        if (p->obj != OBJ_oSpearTrapBottom) p->ispd = (img_t)0.5;
        return 1;
    case OBJ_oTrapBlock:
        if (!fromgen) return 0;
        PE(p)->dying = 0;
        PE(p)->counter = play_gen_inst->deathtimer;                  /* deathTimer (genroom.c 'B') */
        return 1;
    case OBJ_oTree: case OBJ_oLush: case OBJ_oLeaves: case OBJ_oTreeBranch:
        return fromgen;                                              /* the generator ran them */
    }
    return 0;
}

/* ---- oEnemy Step's package branches ---------------------------------------------------------------------- */
/* oEnemy Step :44 a fire frog in water becomes a frog */
static void firefrog_water(int i)
{
    int f = pin_create(PX(i).x, PX(i).y, OBJ_oFrog);
    PE(&PX(f))->status = PE(&PX(i))->status;
    pin_destroy(i);
}

/* oEnemy Step :76 the spears */
static void enemy_spears(int i)
{
    int trap = instance_nearest_p(X(i), Y(i), OBJ_oSpearsLeft);
    struct pin *p = &PX(i);
    if (DGE(PX(trap).img, 20) && DLT(PX(trap).img, 24)) {
        if (p->type == T_CAVEMAN || p->type == T_MANTRAP || p->type == T_YETI || p->type == T_SHOPKEEPER) {
            /* "Hawkman": no Create sets it (oHawkman's type is "Yeti") */
            if (PE(p)->hp > 0) {
                PE(p)->hp -= 2;
                PEN(p)->countsAsKill = 0;
                PE(p)->status = 98;
                PE(p)->counter = PEN(p)->stunTime;
                PE(p)->yVel = N(-6);
                if (DLT(X(trap) + 8, X(i) + 8)) PE(p)->xVel = N(4);
                else PE(p)->xVel = N(-4);
                p->ispd = (img_t)0.5;
                snd_play(SND_xhit);                                                    /* :94 */
                scrCreateBlood(i, P(X(i) + sprw(i) / 2.0), P(Y(i) + sprh(i) / 2.0), 2);
            }
        } else {
            PE(p)->hp -= 2;
            PEN(p)->countsAsKill = 0;
            snd_play(SND_xhit);                                                        /* :102 */
            scrCreateBlood(i, P(X(i) + sprw(i) / 2.0), P(Y(i) + sprh(i) / 2.0), 1);
        }
    }
}

/* ---- Step ---------------------------------------------------------------------------------------------- */
static void frog_step(int i)                                         /* objects/oFrog|oFireFrog/Step_0.gml */
{
    struct pin *p;
    int fire = PX(i).obj == OBJ_oFireFrog, c;
    double dist;
    pen_parent_step(i);
    p = &PX(i);
    if (!eview(i, 20, 4)) return;
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    PE(p)->yVel += PE(p)->myGrav;
    if (NGT(PE(p)->yVel, PEN(p)->yVelLimit)) PE(p)->yVel = PEN(p)->yVelLimit;
    if (CP(X(i) + 8, Y(i) + 8, OBJ_oSolid)) PE(p)->hp = -999;
    if (PE(p)->hp < 1) {                                                   /* :12 */
        if (!fire) scrCreateBlood(i, P(X(i) + 8), P(Y(i) + 8), 3);
        p = &PX(i);
        if (PEN(p)->countsAsKill) PG.kills += 1;                           /* global.frogs / firefrogs: statistics */
        if (fire) pin_create(p->x + PI(8), p->y + PI(8), OBJ_oFireFrogBomb);
        pin_destroy(i);
    }
    p = &PX(i);
    if (isCollisionRight(i, 1)) PE(p)->xVel = N(-1);
    if (isCollisionLeft(i, 1)) PE(p)->xVel = N(1);
    PE(p)->colBot = 0;
    if (isCollisionBottom(i, 1)) PE(p)->colBot = 1;
    dist = distance_to_object_p(i, OBJ_oCharacter);
    c = instance_first_p(OBJ_oCharacter);
    if (PE(p)->status == F_IDLE) {
        PE(p)->xVel = 0;
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        else if (DLT(dist, 64)) PE(p)->status = F_BOUNCE;
        if (PE(p)->status == F_BOUNCE) snd_play(SND_xfrog);                           /* :45 */
    } else if (PE(p)->status == F_RECOVER) {
        if (PE(p)->colBot) {
            PE(p)->status = F_IDLE;
            PE(p)->xVel = 0;
            PE(p)->yVel = 0;
            PE(p)->counter = (int16_t)RAND(10, 40);
        }
    } else if (PE(p)->status == F_BOUNCE) {
        if (PE(p)->colBot) {
            PE(p)->yVel = NI(-1 * RAND(2, 4));
            if (DLT(X(c), X(i))) {
                PE(p)->facing = E_LEFT;
                PE(p)->xVel = N(-3);
            } else {
                PE(p)->facing = E_RIGHT;
                PE(p)->xVel = N(3);
            }
        } else
            PE(p)->status = F_RECOVER;
    } else if (PE(p)->status != F_DROWNED) {
        PE(p)->status = F_IDLE;
        PE(p)->xVel = 0;
    }
    if (isCollisionTop(i, 1)) PE(p)->yVel = N(1);
    if (!PE(p)->colBot) pin_set_sprite(i, fire ? GSPR_sFireFrogJumpL : GSPR_sFrogJumpL);
    else pin_set_sprite(i, fire ? GSPR_sFireFrogLeft : GSPR_sFrogLeft);
}

static void mantrap_new_shopkeeper(int i)                            /* oManTrap Step :31-36, :112-117 */
{
    int k, obj = pin_create(PX(i).x, PX(i).y, OBJ_oShopkeeper);
    PE(&PX(obj))->status = 2;
    PE(&PX(obj))->hasGun = 0;
    for (k = 0; k < 3; k++) leaf_at(i);
    scrCreateBlood(i, PX(i).x + PI(8), PX(i).y + PI(8), 1);
    snd_play(SND_xsmallexplode);
}

static void mantrap_step(int i)                                      /* objects/oManTrap/Step_0.gml */
{
    struct pin *p;
    pen_parent_step(i);
    p = &PX(i);
    if (!eview(i, 20, 4)) return;
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    if (!PE(p)->held) PE(p)->yVel += PE(p)->myGrav;
    if (NGT(PE(p)->yVel, PEN(p)->yVelLimit)) PE(p)->yVel = PEN(p)->yVelLimit;
    PE(p)->colLeft = PE(p)->colRight = PE(p)->colBot = PE(p)->colTop = 0;
    if (isCollisionLeft(i, 1)) PE(p)->colLeft = 1;
    if (isCollisionRight(i, 1)) PE(p)->colRight = 1;
    if (isCollisionBottom(i, 1)) PE(p)->colBot = 1;
    if (isCollisionTop(i, 1)) PE(p)->colTop = 1;
    if (PE(p)->hp < 1) {                                                   /* :19 */
        int k;
        if (PEN(p)->countsAsKill) PG.kills += 1;                           /* global.mantraps: statistics */
        for (k = 0; k < 3; k++) leaf_at(i);
        scrCreateBlood(i, PX(i).x + PI(8), PX(i).y + PI(8), 1);
        if (JX(i)->ateShopkeeper) mantrap_new_shopkeeper(i);
        pin_destroy(i);
    }
    p = &PX(i);
    if (isCollisionBottom(i, 1) && PE(p)->status != E_STUNNED) PE(p)->yVel = 0;
    if (PE(p)->status == MT_IDLE) {                                       /* :43 */
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        if (PE(p)->counter == 0) {
            PE(p)->facing = (int16_t)RAND(0, 1);
            PE(p)->status = MT_WALK;
        }
    } else if (PE(p)->status == MT_WALK) {                                /* :52 */
        double x = X(i), y = Y(i);
        if (isCollisionLeft(i, 1) || isCollisionRight(i, 1))
            PE(p)->facing = PE(p)->facing == E_LEFT ? E_RIGHT : E_LEFT;
        if (PE(p)->facing == E_LEFT && !CPn(x - 1, y, OBJ_oSolid, i) && !CPn(x - 1, y + 16, OBJ_oSolid, i))
            PE(p)->facing = E_RIGHT;
        else if (PE(p)->facing == E_RIGHT && !CPn(x + 16, y, OBJ_oSolid, i) && !CPn(x + 16, y + 16, OBJ_oSolid, i))
            PE(p)->facing = E_LEFT;
        if ((!CPn(x - 1, y + 16, OBJ_oSolid, i) || CPn(x - 1, y, OBJ_oSolid, i)) &&
            (!CPn(x + 16, y + 16, OBJ_oSolid, i) || CPn(x + 16, y, OBJ_oSolid, i))) {
            if (CPn(x - 1, y, OBJ_oSolid, i)) PE(p)->facing = E_RIGHT;
            else PE(p)->facing = E_LEFT;
            PE(p)->xVel = 0;
        } else if (PE(p)->facing == E_LEFT) PE(p)->xVel = N(-1);
        else PE(p)->xVel = N(1);
        if (RAND(1, 100) == 1) {
            PE(p)->status = MT_IDLE;
            PE(p)->counter = (int16_t)RAND(20, 50);
            PE(p)->xVel = 0;
        }
    } else if (PE(p)->status == E_STUNNED) {                              /* :86 */
        pin_set_sprite(i, GSPR_sManTrapStunL);
        if (PE(p)->colBot && !PEN(p)->bounced) {
            PEN(p)->bounced = 1;
            scrCreateBlood(i, p->x + PI(8), p->y + PI(8), 1);
            p = &PX(i);
        }
        if (PE(p)->held || PE(p)->colBot) {
            if (PE(p)->counter > 0) PE(p)->counter -= 1;
            else if (JX(i)->ateShopkeeper) {
                mantrap_new_shopkeeper(i);
                pin_destroy(i);
            } else if (PE(p)->hp > 0) {
                PE(p)->status = MT_IDLE;
                PE(p)->counter = (int16_t)RAND(20, 50);
                if (PE(p)->held) {
                    struct pin *q = &PX(pl());
                    PE(p)->held = 0;
                    if (CP(X(i) + 16, Y(i) + 8, OBJ_oSolid)) pin_setx(p, q->x - PI(12));
                    else if (CP(X(i), Y(i) + 8, OBJ_oSolid)) pin_setx(p, q->x - PI(4));
                    pin_sety(p, q->y - PI(8));
                    PL.holdItem = NOONE;
                    PL.pickupItemType = T_NONE;
                }
            }
        }
    }
    p = &PX(i);
    if (PE(p)->status >= E_STUNNED) scrCheckCollisions(i);               /* :138 */
    if (PE(p)->colBot) {                                                  /* :144 friction */
        if (NLT(NABS(PE(p)->xVel), N(0.1))) PE(p)->xVel = 0;
        else if (NNE(NABS(PE(p)->xVel), N(0))) PE(p)->xVel = NMUL(PE(p)->xVel, N(0.3));
    }
    if (isCollisionSolid(i)) pin_sety(p, p->y - PI(2));
    if (PE(p)->status == MT_EATING && DEQ(p->img, 8)) {                  /* :152 */
        scrCreateBlood(i, p->x + PI(8), p->y, 1);
        p = &PX(i);
    }
    if (PE(p)->status == MT_SLEEPY && DEQ(p->img, 6) && RAND(1, 8) == 1) {   /* :157 */
        int bone;
        if (PE(p)->facing == E_LEFT) {
            bone = pin_create(p->x + PI(2), p->y + PI(4), OBJ_oBone);
            PE(&PX(bone))->xVel = N(-2);
        } else {
            bone = pin_create(p->x + PI(14), p->y + PI(4), OBJ_oBone);
            PE(&PX(bone))->xVel = N(2);
        }
        p = &PX(i);
    }
    if (PE(p)->status < MT_SLEEPY) pin_set_sprite(i, GSPR_sManTrapLeft);
}

/* the GRAB release (oMonkey Step :198-288) */
static void monkey_throw(int i)
{
    struct pin *p = &PX(i), *q = &PX(pl());
    struct jx *j = JX(i);
    int n = 500 + 125 * G.levelType, obj, c;                           /* 500 + ceil(500 / 4) * levelType */
    if (RAND(1, 4) == 1) {                                             /* trip the player */
        PE(q)->xVel = PL.facing == LEFT ? N(-3) : N(3);
        PE(q)->yVel = N(-3);
        PL.stunned = 1;
        PL.stunTimer = 40;
        /* status = 16: a variable oPlayer1 does not read */
        if (PL.holdItem != NOONE) scrDropItem(0, 0);
        snd_play(SND_xhit);                                                           /* :227 */
    } else if (PG.money >= n && RAND(1, 10) <= 8) {
        PG.money -= n;
        obj = pin_create(p->x, p->y, OBJ_oGoldNugget);
        PE(&PX(obj))->canCollect = 0;
        PE(&PX(obj))->alarm[0] = 20;
        {
            int a = RAND(1, 3), b = RAND(1, 3);
            PE(&PX(obj))->xVel = NI(a - b);
        }
        PE(&PX(obj))->yVel = NI(-RAND(3, 4));
        snd_play(SND_xthrow);                                                         /* :241 */
    } else if (RAND(1, 2) == 1 && PG.rope > 0) {
        PG.rope -= 1;
        obj = pin_create(p->x, p->y, OBJ_oRopeThrow);
        {
            int a = RAND(1, 3), b = RAND(1, 3);
            PE(&PX(obj))->xVel = NI(a - b);
        }
        PE(&PX(obj))->yVel = NI(-RAND(3, 4));
        snd_play(SND_xthrow);                                                         /* :253 */
    } else if (PG.bombs > 0) {
        PG.bombs -= 1;
        obj = pin_create(p->x, p->y, OBJ_oBomb);
        if (RAND(1, 10) == 1) {
            pin_set_sprite(obj, GSPR_sBombArmed);
            PE(&PX(obj))->armed = 1;
            PX(obj).ispd = 1;
            PE(&PX(obj))->alarm[1] = 40;
        }
        {
            int a = RAND(1, 3), b = RAND(1, 3);
            PE(&PX(obj))->xVel = NI(a - b);
        }
        PE(&PX(obj))->yVel = NI(-RAND(3, 4));
        snd_play(SND_xthrow);                                                         /* :272 */
    }
    p = &PX(i);
    PE(p)->status = F_BOUNCE;
    j->vineCounter = 20;
    PE(p)->yVel = NI(-1 * RAND(2, 4));
    c = instance_first_p(OBJ_oCharacter);
    if (DGT(X(c), X(i) + 8)) {
        PE(p)->facing = E_LEFT;
        PE(p)->xVel = N(-3);
    } else {
        PE(p)->facing = E_RIGHT;
        PE(p)->xVel = N(3);
    }
    j->grabCounter = 60;
}

static void monkey_step(int i)                                       /* objects/oMonkey/Step_0.gml */
{
    struct pin *p;
    struct jx *j;
    double dist;
    int c;
    pen_parent_step(i);
    p = &PX(i);
    if (!eview(i, 20, 4)) return;
    pin_setdepth(p, 40);
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    if (PE(p)->status != M_HANG && PE(p)->status != M_CLIMB && PE(p)->status != M_GRAB) PE(p)->yVel += PE(p)->myGrav;
    if (NGT(PE(p)->yVel, PEN(p)->yVelLimit)) PE(p)->yVel = PEN(p)->yVelLimit;
    if (CP(X(i) + 8, Y(i) + 8, OBJ_oSolid)) PE(p)->hp = -999;
    if (CPn(X(i) + 8, Y(i) + 8, OBJ_oWater, i)) {                         /* :15 */
        if (!PEN(p)->swimming) {
            pin_create(p->x + PI(8), p->y, OBJ_oSplash);
            p = &PX(i);
            PEN(p)->swimming = 1;
            snd_play(SND_xsplash);                                                    /* :21 */
        }
    } else
        PEN(p)->swimming = 0;
    if (PE(p)->hp < 1) {                                                   /* :29 */
        scrCreateBlood(i, p->x + PI(8), p->y + PI(8), 3);
        p = &PX(i);
        if (PEN(p)->countsAsKill) PG.kills += 1;                           /* global.monkeys: statistics */
        pin_destroy(i);
    }
    p = &PX(i);
    j = JX(i);
    if (isCollisionRight(i, 1)) PE(p)->xVel = N(-1);
    if (isCollisionLeft(i, 1)) PE(p)->xVel = N(1);
    PE(p)->colBot = 0;
    if (isCollisionBottom(i, 1)) PE(p)->colBot = 1;
    if (j->grabCounter > 0) j->grabCounter -= 1;
    if (j->vineCounter > 0) j->vineCounter -= 1;
    if (j->throwCounter > 0) j->throwCounter -= 1;
    dist = distance_to_object_p(i, OBJ_oCharacter);
    c = instance_first_p(OBJ_oCharacter);
    switch (PE(p)->status) {
    case F_IDLE:                                                          /* :60 */
        PE(p)->xVel = 0;
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        else PE(p)->status = F_WALK;
        if (DLT(dist, 64)) PE(p)->status = F_BOUNCE;
        break;
    case F_WALK:                                                          /* :68 */
        if (isCollisionLeft(i, 1) || isCollisionRight(i, 1))
            PE(p)->facing = PE(p)->facing == E_LEFT ? E_RIGHT : E_LEFT;
        PE(p)->xVel = PE(p)->facing == E_LEFT ? N(-2) : N(2);
        if (RAND(1, 100) == 1) {
            PE(p)->status = F_IDLE;
            PE(p)->counter = (int16_t)RAND(20, 50);
            PE(p)->xVel = 0;
        }
        break;
    case F_RECOVER:                                                       /* :92 */
        if (PE(p)->colBot) {
            PE(p)->status = F_IDLE;
            PE(p)->xVel = 0;
            PE(p)->yVel = 0;
            PE(p)->counter = (int16_t)RAND(10, 40);
        } else if (isCollisionLadder(i)) {
            if (j->vineCounter == 0) {
                PE(p)->status = M_HANG;
                PE(p)->xVel = 0;
                PE(p)->yVel = 0;
                PE(p)->counter = (int16_t)RAND(10, 40);
            }
        }
        break;
    case F_BOUNCE:                                                        /* :112 */
        if (PE(p)->colBot) {
            PE(p)->yVel = NI(-1 * RAND(4, 5));
            if (DLT(X(c), X(i) + 8)) {
                PE(p)->facing = E_LEFT;
                PE(p)->xVel = N(-2);
            } else {
                PE(p)->facing = E_RIGHT;
                PE(p)->xVel = N(2);
            }
        } else {
            PE(p)->status = F_RECOVER;
            snd_play(SND_xmonkey);                                                    /* :131 */
        }
        break;
    case M_HANG:                                                          /* :134 */
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        else PE(p)->status = M_CLIMB;
        break;
    case M_CLIMB:                                                         /* :145 */
        PE(p)->xVel = 0;
        if (j->dir == 0) {
            PE(p)->yVel = N(-1);
            if (!CP(X(i) + 8, Y(i), OBJ_oVine)) {
                j->dir = 1;
                PE(p)->status = M_HANG;
                PE(p)->counter = (int16_t)RAND(10, 40);
            }
        } else {
            PE(p)->yVel = N(1);
            if (!CP(X(i) + 8, Y(i) + 22, OBJ_oVine)) {
                j->dir = 0;
                PE(p)->status = M_HANG;
                PE(p)->counter = (int16_t)RAND(10, 40);
            }
        }
        if (DLT(dist, 64) && DGT(Y(c), Y(i))) {
            PE(p)->status = F_BOUNCE;
            j->vineCounter = 30;
            PE(p)->yVel = NI(-1 * RAND(2, 4));
            if (DLT(X(c), X(i))) {
                PE(p)->facing = E_LEFT;
                PE(p)->xVel = N(-3);
            } else {
                PE(p)->facing = E_RIGHT;
                PE(p)->xVel = N(3);
            }
        }
        break;
    case M_GRAB: {                                                        /* :186 */
        struct pin *q = &PX(pl());
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        pin_setdepth(p, 120);
        pin_setx(p, P(PTOD(q->x) + j->grabX));
        pin_sety(p, P(PTOD(q->y) + j->grabY));
        PE(p)->facing = PL.facing == LEFT ? E_LEFT : E_RIGHT;
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        else monkey_throw(i);
        break;
    }
    default:
        if (PE(p)->status != F_DROWNED) {
            PE(p)->status = F_IDLE;
            PE(p)->xVel = 0;
        }
        break;
    }
    p = &PX(i);
    if (PE(p)->status != M_GRAB && isCollisionTop(i, 1)) PE(p)->yVel = N(1);
    if (PE(p)->status == M_HANG) pin_set_sprite(i, GSPR_sMonkeyHangL);
    else if (PE(p)->status == M_CLIMB || PE(p)->status == M_GRAB) pin_set_sprite(i, GSPR_sMonkeyClimbL);
    else if (!PE(p)->colBot) pin_set_sprite(i, GSPR_sMonkeyJumpL);
    else if (PE(p)->status == F_WALK) pin_set_sprite(i, GSPR_sMonkeyWalkL);
    else pin_set_sprite(i, GSPR_sMonkeyLeft);
}

static void scarab_step(int i)                                       /* objects/oScarab/Step_0.gml (no inherit) */
{
    struct pin *p = &PX(i);
    int c;
    double dist, dir;
    if (!eview(i, 20, 4)) return;
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    if (CP(X(i) + 8, Y(i) + 8, OBJ_oSolid)) PE(p)->hp = -999;
    if (PE(p)->hp < 1) {                                                   /* :8 */
        int k;
        for (k = 0; k < 3; k++) {
            int yy = RAND(0, 14), xx = RAND(0, 14), obj;
            obj = pin_create(PX(i).x + PI(2 + xx), PX(i).y + PI(2 + yy), OBJ_oFlareSpark);
            PE(&PX(obj))->yVel = NI(RAND(1, 3));
        }
        pin_destroy(i);
    }
    p = &PX(i);
    dir = 0;
    c = instance_first_p(OBJ_oCharacter);
    dist = point_distance_d(X(i) + 8, Y(i) + 8, X(c), Y(c));
    if (PE(p)->status == 0) {
        if (NGT(PE(p)->xVel, N(0))) PE(p)->xVel -= N(0.5);
        if (NGT(PE(p)->yVel, N(0))) PE(p)->yVel -= N(0.5);
        if (NLT(PE(p)->xVel, N(0))) PE(p)->xVel += N(0.5);
        if (NLT(PE(p)->yVel, N(0))) PE(p)->yVel += N(0.5);
        if (NLT(NABS(PE(p)->xVel), N(1))) PE(p)->xVel = 0;
        if (NLT(NABS(PE(p)->yVel), N(1))) PE(p)->yVel = 0;
        if (NEQ(PE(p)->xVel, N(0)) && NEQ(PE(p)->yVel, N(0)) && PE(p)->counter > 0) PE(p)->counter -= 1;
        if (PE(p)->counter == 0 && NLT(PE(p)->xVel, N(1)) && NLT(PE(p)->yVel, N(1))) {
            if (DLT(dist, 64)) dir = point_direction_d(X(i) + 8, Y(i) + 8, X(c), Y(c)) + 180;
            else dir = RAND(0, 360);
            PE(p)->xVel = ND(4 * pcos_cr(degtorad_d(dir)));
            PE(p)->yVel = ND(-4 * psin_cr(degtorad_d(dir)));
            PE(p)->counter = (int16_t)RAND(10, 30);
        }
        if (isCollisionRight(i, 1) && NGT(PE(p)->xVel, N(0))) PE(p)->xVel = -PE(p)->xVel;
        if (isCollisionLeft(i, 1) && NLT(PE(p)->xVel, N(0))) PE(p)->xVel = -PE(p)->xVel;
        if (isCollisionTop(i, 1) && NLT(PE(p)->yVel, N(0))) PE(p)->yVel = -PE(p)->yVel;
        if (isCollisionBottom(i, 1) && NGT(PE(p)->yVel, N(0))) PE(p)->yVel = -PE(p)->yVel;
    }
}

static void firefrogbomb_step(int i)                                 /* objects/oFireFrogBomb/Step_0.gml */
{
    struct pin *p;
    item_step(i);
    p = &PX(i);
    if (PE(p)->armed && instance_exists_p(OBJ_oShopkeeper)) pitems_world(1042, i, 0);   /* :2 as oBomb's */
    if (collision_point_p(X(i), Y(i), OBJ_oWaterSwim, 1, i) != NOONE) {
        if (!ffb_swim[p->ext]) {
            pin_create(p->x, p->y, OBJ_oSplash);
            p = &PX(i);
            ffb_swim[p->ext] = 1;
            snd_play(SND_xsplash);                                                    /* :21 */
        }
    } else
        ffb_swim[p->ext] = 0;
}

/* objects/oRubblePiece/Step_0.gml (oLeaf's, type "Leaf") */
static void leaf_step(int i)
{
    struct pin *p = &PX(i);
    double x, y;
    pin_setx(p, PADDV(p->x, PE(p)->xVel));
    pin_sety(p, PADDV(p->y, PE(p)->yVel));
    PE(p)->yVel += PE(p)->yAcc;
    NOPS(3);
    x = X(i);
    y = Y(i);
    if (CP(x, y, OBJ_oWaterSwim)) {
        PE(p)->yVel = 0;
        pin_set_sprite(i, GSPR_sLeafStill);
    } else if (CP(x, y, OBJ_oLava))
        pin_destroy(i);
    if (CP(x, y, OBJ_oSolid)) pin_destroy(i);
    view_read();
    if (DLT(x, PW.xview - 32) || DGT(x, PW.xview + 320 + 32) || DLT(y, PW.yview - 32) || DGT(y, PW.yview + 240 + 32))
        pin_destroy(i);
}

static int tree_or_leaves(double x, double y) { return CP(x, y, OBJ_oTree) || CP(x, y, OBJ_oLeaves); }

static void leaves_step(int i)                                       /* objects/oLeaves/Step_0.gml */
{
    struct pin *p = &PX(i);
    double x = X(i), y = Y(i);
    /* spriteSet stays false */
    if (tree_or_leaves(x - 16, y) && tree_or_leaves(x + 16, y)) pin_set_sprite(i, GSPR_sLeavesTop);
    if (!eview(i, 16, 16)) return;
    if (p->spr == GSPR_sLeavesTop) {
        if (!tree_or_leaves(x - 16, y) || !tree_or_leaves(x + 16, y)) pin_destroy(i);
    } else if (p->spr == GSPR_sLeaves || p->spr == GSPR_sLeavesDead) {
        if (!tree_or_leaves(x + 16, y)) pin_destroy(i);
    } else if (p->spr == GSPR_sLeavesRight || p->spr == GSPR_sLeavesDeadR) {
        if (!tree_or_leaves(x - 16, y)) pin_destroy(i);
    }
}

/* the spear traps' tests (oSpearTrapBottom / Top Step :5-77): fire when the instance is in line */
static void spears(int i, int right)
{
    int s = pin_create(PX(i).x + PI(right ? 16 : -16), PX(i).y, OBJ_oSpearsLeft);
    if (right) pin_set_sprite(s, GSPR_sSpearsRight);
    PE(&PX(i))->fired = 50;                                            /* firedMax */
}

static void speartrap_step(int i)                                    /* objects/oSpearTrapBottom|Top/Step_0.gml */
{
    struct pin *p = &PX(i);
    double x = X(i), y = Y(i), range = 64, prox = 4;
    int q = pl(), obj, side;
    if (PE(p)->fired > 0) PE(p)->fired -= 1;
    for (side = 0; side < 2; side++) {
        /* oPlayer1 */
        if (PE(&PX(i))->fired == 0 && DLT(dabs(Y(q) - y - 8), prox) && (side ? DGT(X(q), x + 8) : DLT(X(q), x)) &&
            DLT(point_distance_d(x + 8, y + 8, X(q), Y(q)), range))
            spears(i, side);
        obj = instance_nearest_p(x, y, OBJ_oEnemy);
        if (obj != NOONE && PE(&PX(i))->fired == 0 && DLT(dabs(Y(obj) - y), prox) &&
            (side ? DGT(X(obj), x) : DLT(X(obj), x)) && DLT(point_distance_d(x, y, X(obj), Y(obj)), range))
            spears(i, side);
        obj = instance_nearest_p(x, y, OBJ_oMoveableSolid);
        if (obj != NOONE && PE(&PX(i))->fired == 0 && DLT(dabs(Y(obj) - y), prox) &&
            (side ? DGT(X(obj), x) : DLT(X(obj), x)) && DLT(point_distance_d(x, y, X(obj), Y(obj)), range))
            spears(i, side);
        obj = instance_nearest_p(x, y, OBJ_oItem);
        if (obj != NOONE && PE(&PX(i))->fired == 0 && DLT(dabs(Y(obj) - y - 8), prox) &&
            (side ? DGT(X(obj), x + 8) : DLT(X(obj), x + 8)) &&
            DLT(point_distance_d(x + 8, y + 8, X(obj), Y(obj)), range))
            spears(i, side);
    }
    p = &PX(i);
    if (eview(i, 8, 8) && !CP(X(i), Y(i) + 16, OBJ_oSolid)) pin_destroy(i);   /* :79 */
    if (p->obj == OBJ_oSpearTrapBottom) {                                 /* :87 (Bottom only) */
        pin_setx(p, PI(PCEIL(p->x)));
        pin_sety(p, PI(PCEIL(p->y)));
    }
}

static int jungle_step(int i)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oFrog: case OBJ_oFireFrog: frog_step(i); return 1;
    case OBJ_oManTrap: mantrap_step(i); return 1;
    case OBJ_oMonkey: monkey_step(i); return 1;
    case OBJ_oScarab: scarab_step(i); return 1;
    case OBJ_oFireFrogBomb: firefrogbomb_step(i); return 1;
    case OBJ_oLeaf: leaf_step(i); return 1;
    case OBJ_oLeaves: leaves_step(i); return 1;
    case OBJ_oTree:                                                  /* objects/oTree/Step_0.gml */
        if (eview(i, 16, 16) && !CP(X(i), Y(i) + 16, OBJ_oSolid)) pin_destroy(i);
        return 1;
    case OBJ_oTreeBranch:                                            /* objects/oTreeBranch/Step_0.gml */
        if (eview(i, 16, 16) && !CP(X(i) - 16, Y(i), OBJ_oTree) && !CP(X(i) + 16, Y(i), OBJ_oTree)) pin_destroy(i);
        return 1;
    case OBJ_oSpearTrapBottom: case OBJ_oSpearTrapTop: case OBJ_oSpearTrapLit: speartrap_step(i); return 1;
    case OBJ_oSpearsLeft:                                            /* objects/oSpearsLeft/Step_0.gml */
        if (p->spr == GSPR_sSpearsLeft && !CP(X(i) + 16, Y(i), OBJ_oSpearTrapTop) &&
            !CP(X(i) + 16, Y(i), OBJ_oSpearTrapBottom))
            pin_destroy(i);
        p = &PX(i);
        if (p->spr == GSPR_sSpearsRight && !CP(X(i) - 16, Y(i), OBJ_oSpearTrapTop) &&
            !CP(X(i) - 16, Y(i), OBJ_oSpearTrapBottom))
            pin_destroy(i);
        pin_setdepth(&PX(i), 995);
        return 1;
    case OBJ_oTrapBlock:                                             /* objects/oTrapBlock/Step_0.gml */
        if (PE(p)->dying) {
            if (PE(p)->counter > 0) PE(p)->counter -= 1;
            else pin_destroy(i);
        }
        return 1;
    }
    return 0;
}

/* ---- Destroy ---------------------------------------------------------------------------------------------- */
static int jungle_destroy(int i)
{
    struct pin *p = &PX(i);
    int k;
    switch (p->obj) {
    case OBJ_oLush:                                                  /* objects/oLush/Destroy_0.gml */
        destroy_solid(i);
        if (!p->cleanDeath && !G.cleanSolids) {
            three_rubble(i, GSPR_sRubbleLush, GSPR_sRubbleLushSmall);
            if (p->spr == GSPR_sLushGold)
                for (k = 0; k < 3; k++) gold_drop(i, OBJ_oGoldChunk);
            if (p->spr == GSPR_sLushGoldBig) {
                for (k = 0; k < 3; k++) gold_drop(i, OBJ_oGoldChunk);
                gold_drop(i, OBJ_oGoldNugget);
            }
        }
        return 1;
    case OBJ_oTree:                                                  /* objects/oTree/Destroy_0.gml */
        destroy_solid(i);
        if (!p->cleanDeath && !G.cleanSolids) three_rubble(i, -1, -1);
        return 1;
    case OBJ_oSpearTrapBottom: case OBJ_oSpearTrapTop: case OBJ_oSpearTrapLit:   /* no inherit */
        if (!p->cleanDeath && !G.cleanSolids) three_rubble(i, GSPR_sRubbleTan, GSPR_sRubbleTanSmall);
        G.checkWater = 1;
        return 1;
    case OBJ_oTrapBlock:                                             /* objects/oTrapBlock/Destroy_0.gml */
        if (!p->cleanDeath && !G.cleanSolids) {
            three_rubble(i, GSPR_sRubbleTan, GSPR_sRubbleTanSmall);
            if (PE(&PX(i))->dying) {
                snd_play(SND_xthump);                                                 /* :11 */
                scrShake(10);
            }
        }
        return 1;
    case OBJ_oTreeBranch:                                            /* objects/oTreeBranch/Destroy_0.gml */
        if (p->spr != GSPR_sTreeBranchDeadL && p->spr != GSPR_sTreeBranchDeadR) rubble_at(i, OBJ_oLeaf, 8, -1);
        return 1;
    case OBJ_oLeaves:                                                /* objects/oLeaves/Destroy_0.gml */
        if (p->spr != GSPR_sLeavesDead && p->spr != GSPR_sLeavesDeadR) {
            rubble_at(i, OBJ_oLeaf, 8, -1);
            rubble_at(i, OBJ_oLeaf, 8, -1);
        }
        return 1;
    case OBJ_oScarab:                                                /* objects/oScarab/Destroy_0.gml */
        /* other.x / other.y: taken as the scarab itself (not verified by a route: none destroys a scarab; in the
           runner `other` would be the collision partner when the destroy comes from a collision event) */
        for (k = 0; k < 3; k++) {
            int yy = RAND(0, 4), xx = RAND(0, 4);
            pin_create(PX(i).x + PI(6 + xx), PX(i).y + PI(6 + yy), OBJ_oFlareSpark);
        }
        return 1;
    }
    return 0;
}

/* ---- Alarm, Animation End ------------------------------------------------------------------------------- */
static int jungle_alarm(int i, int a)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oFireFrogBomb:
        if (a == 1) {                                                /* objects/oFireFrogBomb/Alarm_1.gml */
            pin_create(p->x, p->y, OBJ_oExplosion);
            scrCreateBlood(i, PX(i).x, PX(i).y, 3);
            if (G.graphicsHigh) scrCreateFlame(PX(i).x, PX(i).y, 3);
            if (PE(&PX(i))->held) PL.holdItem = NOONE;
            pin_destroy(i);
            return 1;
        }
        return 0;
    case OBJ_oSpearTrapBottom: case OBJ_oSpearTrapTop: case OBJ_oSpearTrapLit:
        if (a == 0) {                                                /* Alarm_0 (never set) */
            int ar = pin_create(p->x - PI(16), p->y + PI(4), OBJ_oArrow);
            PE(&PX(ar))->xVel = N(-5);
            return 1;
        }
        return 0;
    }
    return 0;
}

static int jungle_animend(int i)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oManTrap:                                               /* objects/oManTrap/Other_7.gml */
        if (PE(p)->status == MT_EATING) {
            pin_set_sprite(i, GSPR_sManTrapSleepL);
            PE(p)->status = MT_SLEEPY;
        } else if (p->spr == GSPR_sManTrapSleepL) {
            pin_set_sprite(i, GSPR_sManTrapStunL);
            PE(p)->status = E_STUNNED;
            PE(p)->counter = (int16_t)(PEN(p)->stunTime * 2);
        }
        return 1;
    case OBJ_oSpearsLeft: pin_destroy(i); return 1;                 /* action_kill_object */
    }
    return 0;
}

/* ---- Collision events ----------------------------------------------------------------------------------- */
/* objects/oManTrap/Collision_oCharacter.gml */
static void mantrap_player(int i, int c)
{
    struct pin *p = &PX(i), *o = &PX(c);
    if (DGT(dabs(X(c) - (X(i) + 8)), 8)) {
    } else if ((PG.hasSpikeShoes || PE(p)->status == MT_EATING) && !PL.dead && !PL.stunned &&
               (PL.state == JUMPING || PL.state == FALLING) && DLT(Y(c), Y(i) + 5) && !PL.swimming) {
        PE(o)->yVel = N(-6) - NMUL(N(0.2), PE(o)->yVel);
        if (PG.hasSpikeShoes) {
            PE(p)->hp -= (int16_t)(3 * (PL.fallTimer / 16 + 1));
            pin_create(o->x, o->y + PI(8), OBJ_oBlood);                /* not bloodless */
            p = &PX(i);
            o = &PX(c);
        } else
            PE(p)->hp -= (int16_t)(1 * (PL.fallTimer / 16 + 1));
        PL.fallTimer = 0;
        PEN(p)->countsAsKill = 1;
        PE(p)->status = E_STUNNED;
        PE(p)->counter = PEN(p)->stunTime;
        PE(p)->yVel = N(-6);
        if (DLT(X(c), X(i) + 8)) PE(p)->xVel += N(1);
        else PE(p)->xVel -= N(1);
        p->ispd = (img_t)0.5;
        snd_play(SND_xhit);                                                           /* :18 */
    } else if (o->visible && PL.invincible == 0) {
        if (PE(p)->status != E_STUNNED && PE(p)->status != MT_EATING) {
            PE(p)->xVel = 0;
            PE(p)->status = MT_EATING;
            PE(p)->facing = DGT(X(c), X(i) + 8) ? E_RIGHT : E_LEFT;
            if (G.isDamsel) pin_set_sprite(i, GSPR_sManTrapEatDamselL);
            else if (G.isTunnelMan) pin_set_sprite(i, GSPR_sManTrapEatTunnelL);
            else pin_set_sprite(i, GSPR_sManTrapEatL);
            pin_setvisible(o, 0);
            PL.invincible = 9999;
            PL.bounced = 1;
            PG.plife = -99;
            snd_play(SND_xdie);                                                       /* :45 */
            PG.drawHUD = 0;
            if (PL.holdItem != NOONE) {
                if (PE(p)->held) PE(p)->held = 0;
                else {
                    struct pin *h = &PX(PL.holdItem);
                    PE(h)->held = 0;
                    PE(h)->xVel = PE(p)->facing == E_LEFT ? N(-2) : N(2);
                    PE(h)->yVel = N(-4);
                }
                PL.holdItem = NOONE;
                PL.pickupItemType = T_NONE;
            }
        }
    }
}

/* oManTrap Collision_oCaveman / oDamsel / oShopkeeper: eat it */
static void mantrap_eat(int i, int other, int spr)
{
    struct pin *p = &PX(i);
    if (PE(p)->status == E_STUNNED || PE(p)->status == MT_EATING) return;
    PE(p)->xVel = 0;
    PE(p)->status = MT_EATING;
    PE(p)->facing = DGT(X(other), X(i)) ? E_RIGHT : E_LEFT;
    pin_set_sprite(i, spr);
    if (PX(other).obj == OBJ_oShopkeeper) {
        struct pin *o = &PX(other);
        if (PE(o)->hp > 0) JX(i)->ateShopkeeper = 1;
        if (PE(o)->hasGun) {
            int obj = pin_create(o->x + PI(8), o->y + PI(8), OBJ_oShotgun);
            o = &PX(other);
            PE(&PX(obj))->yVel = NI(RAND(4, 6));
            if (NLT(PE(o)->xVel, N(0))) PE(&PX(obj))->xVel = NI(-1 * RAND(4, 6));
            else PE(&PX(obj))->xVel = NI(RAND(4, 6));
            PE(&PX(obj))->cost = 0;
            PE(&PX(obj))->forSale = 0;
            PE(o)->hasGun = 0;
        }
    } else if (PX(other).obj == OBJ_oDamsel && PE(&PX(other))->held) {
        PL.holdItem = NOONE;
        PL.pickupItemType = T_NONE;
    }
    pin_destroy(other);
}

/* objects/oManTrap/Collision_oWhip.gml, Collision_oWhipPre.gml */
static void mantrap_whipped(int i, int w)
{
    struct pin *p = &PX(i);
    if (PE(p)->status == E_STUNNED) return;
    if (PX(w).obj == OBJ_oSlash) PUNTR(5101);                          /* the machete: package E */
    PE(p)->hp -= 1;                                                     /* other.damage */
    PEN(p)->countsAsKill = 1;
    PE(p)->status = E_STUNNED;
    PE(p)->counter = PEN(p)->stunTime;
    PE(p)->yVel = N(-3);
    PE(p)->xVel = DLT(X(w), X(i) + 8) ? N(2) : N(-2);
    p->ispd = (img_t)0.5;
    leaf_at(i);
    snd_play(SND_xhit);                                                               /* :19 */
}

/* objects/oMonkey/Collision_oCharacter.gml */
static void monkey_player(int i, int c)
{
    struct pin *p = &PX(i), *o = &PX(c), *q;
    struct jx *j = JX(i);
    if (DGT(dabs(X(c) - (X(i) + 8)), 4) || PE(p)->status == M_GRAB) {
    } else if (!PL.dead && (PL.state == JUMPING || PL.state == FALLING) && DLT(Y(c), Y(i) + 2) && !PL.swimming) {
        PE(o)->yVel = N(-6) - NMUL(N(0.2), PE(o)->yVel);
        PL.fallTimer = 0;
        PE(p)->hp -= 1;
        snd_play(SND_xhit);                                                           /* :11 */
    } else if (!PL.invincible && j->grabCounter == 0) {
        q = &PX(pl());
        if (DGT(Y(i) + 8, PTOD(q->y) + 2)) pin_sety(p, P(PTOD(q->y) + 2 - 8));
        if (DLT(Y(i) + 8, PTOD(q->y) - 2)) pin_sety(p, P(PTOD(q->y) - 2 - 8));
        PE(p)->status = M_GRAB;
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        j->grabX = X(i) - PTOD(q->x);
        j->grabY = Y(i) - PTOD(q->y);
        PE(p)->counter = (int16_t)RAND(40, 80);
    }
}

/* objects/oMonkey/Collision_oItem.gml */
static void monkey_item(int i, int it)
{
    struct pin *p = &PX(i), *o = &PX(it);
    struct jx *j = JX(i);
    if (!(j->throwCounter == 0 && PE(p)->status != M_GRAB && PE(o)->active && !PE(o)->held)) return;
    if (o->type == T_ROPE) {
        if (PE(o)->falling) return;
        PE(o)->xVel = PE(p)->facing == E_RIGHT ? N(5) : N(-5);
        PE(o)->yVel = N(-4);
        if (!CP(X(it), Y(it), OBJ_oSolid)) pin_sety(o, P(Y(i) - 2));
    } else {
        PE(o)->xVel = PE(p)->facing == E_RIGHT ? N(5) : N(-5);
        PE(o)->yVel = N(-4);
        if (!CP(X(it), Y(it) - 2, OBJ_oSolid)) pin_sety(o, o->y - PI(2));
    }
    j->throwCounter = 60;
    PE(p)->status = F_IDLE;
    PE(p)->counter = (int16_t)RAND(20, 60);
}

/* objects/oScarab/Collision_oCharacter.gml */
static void scarab_player(int i)
{
    PG.collect += PE(&PX(i))->value;
    PG.collectCounter += 20;
    if (PG.collectCounter > 100) PG.collectCounter = 100;
    PG.scarabs += 1;
    snd_play(SND_xcoin);                                                              /* :5 */
    pin_destroy(i);
}

static int jungle_collision(int self, int other)
{
    int so = PX(self).obj, oo = PX(other).obj, ch = obj_is(oo, OBJ_oCharacter);
    switch (so) {
    case OBJ_oFrog: case OBJ_oFireFrog:                              /* oEnemy's */
        if (ch) enemy_hit_player(self, other);
        else enemy_whipped(self, other);
        return 1;
    case OBJ_oMonkey:
        if (ch) monkey_player(self, other);
        else if (obj_is(oo, OBJ_oItem)) monkey_item(self, other);
        else enemy_whipped(self, other);
        return 1;
    case OBJ_oScarab:
        if (ch) scarab_player(self);
        else enemy_whipped(self, other);
        return 1;
    case OBJ_oManTrap:
        if (ch) mantrap_player(self, other);
        else if (oo == OBJ_oCaveman) mantrap_eat(self, other, GSPR_sManTrapEatCavemanL);
        else if (oo == OBJ_oDamsel) mantrap_eat(self, other, GSPR_sManTrapEatDamselL);
        else if (oo == OBJ_oShopkeeper) mantrap_eat(self, other, GSPR_sManTrapEatShopkeeperL);
        else mantrap_whipped(self, other);
        return 1;
    }
    return 0;
}

/* ---- the pcontent.h entries ---------------------------------------------------------------------------- */
int pjungle_ev(int ev, int i, int arg)
{
    switch (ev) {
    case FEV_CREATE: return jungle_create(i, arg);
    case FEV_STEP: return jungle_step(i);
    case FEV_DESTROY: return jungle_destroy(i);
    case FEV_ALARM: return jungle_alarm(i, arg);
    case FEV_ANIMEND: return jungle_animend(i);
    case FEV_COLLISION: return jungle_collision(i, arg);
    }
    return 0;
}

/* knock off monkeys that grabbed you: with oMonkey (characterStepEvent :830 / :855, oPlayer1 Step :894) */
static void monkeys_off(int xv_kind)
{
    int16_t w[256];
    int n = pw_with(OBJ_oMonkey, w, 256), k;
    for (k = 0; k < n; k++) {
        struct pin *m = &PX(w[k]);
        if (!m->alive || PE(m)->status != M_GRAB) continue;
        if (xv_kind == 2) {
            int a = RAND(0, 1), b = RAND(0, 1);
            PE(m)->xVel = NI(a - b);
        } else
            PE(m)->xVel = xv_kind ? N(1) : N(-1);
        PE(m)->yVel = N(-4);
        PE(m)->status = F_BOUNCE;
        JX(w[k])->vineCounter = 20;
        JX(w[k])->grabCounter = 60;
    }
}

int pjungle_player(int site, int i, int arg)
{
    struct pin *p = &PX(i);
    switch (site) {
    case 2010: monkeys_off(PL.facing == RIGHT ? 1 : 0); return 1;   /* the ledge flip, left or right */
    case 2016: monkeys_off(2); return 1;                            /* the exit */
    case 2020: {                                                     /* oPlayer1 Step :1598 the spears */
        struct pin *o = &PX(arg);
        if (DGE(o->img, 20) && DLT(o->img, 24)) {
            PG.plife -= 4;
            if (o->spr == GSPR_sSpearsLeft) PE(p)->xVel = NI(-RAND(4, 6));
            else PE(p)->xVel = NI(RAND(4, 6));
            PE(p)->yVel = N(-6);
            pin_sety(p, p->y - PI(1));
            scrCreateBlood(i, p->x, p->y, 1);
        }
        return 1;
    }
    }
    PUNTR(site);
    return 0;
}

int pjungle_world(int site, int i, int arg)
{
    struct pin *p = &PX(i);
    switch (site) {
    case 1011: {                                                     /* oSolid Destroy :14 the tiki torch */
        int obj = instance_place_p(i, X(i) + 8, Y(i) - 1, OBJ_oTikiTorch);
        if (obj != NOONE) pin_destroy(obj);
        return 1;
    }
    case 1017:                                                       /* oJar Destroy :22 a snake */
        if (PE(p)->colLeft) pin_create(p->x, p->y - PI(8), OBJ_oSnake);
        else if (PE(p)->colRight) pin_create(p->x - PI(16), p->y - PI(8), OBJ_oSnake);
        else pin_create(p->x - PI(8), p->y - PI(8), OBJ_oSnake);
        return 1;
    }
    PUNTR(site);
    return 0;
}

/* penemy.c's enemy sites (pcontent.h pX_enemy): 0 leaves the site to the next package */
int pjungle_enemy(int site, int e, int arg)
{
    switch (site) {
    case 5011: firefrog_water(e); return 1;
    case 5013: enemy_spears(e); return 1;
    }
    return 0;
}
