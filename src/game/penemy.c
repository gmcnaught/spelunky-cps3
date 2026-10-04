/* P5: the Mines enemies, translated statement for statement from refs/hd/src/objects/<obj>/<event>.gml (line
 * numbers in comments): oEnemy (the parent), oSnake, oBat, oSpiderHang, oSpider, oGiantSpiderHang, oGiantSpider,
 * oCaveman (and its oEnemySight), oFakeBones, oSkeleton, oBone, oWebBall, oSplash, oYellHelp, the boulder trap
 * (oGiantTikiHead, oBoulder). The damsel is in pdamsel.c, the shopkeeper in pshop.c.
 * GML keeps running after instance_destroy() inside an event: so does this code (RNG draws included).
 * Branches the Mines cannot reach (lava, water creatures, other areas' enemy types, Kali's altar) set
 * play_untranslated (codes 5xxx).
 */
#include "pint.h"
#include "penemy.h"
#include "../snd/sndgame.h"                     /* the GML sound calls (src/snd) */
#include "pmath.h"
#include "pcontent.h"                            /* P7 content packages (docs/CONTENT.md) */
#include "pcol.h"                                /* pcol_query / pcol_touch (line_solid) */

enum { E_STUNNED = 98, E_DEAD = 99, E_LEFT = 0, E_RIGHT = 1 };

static double X(int i) { return PTOD(PX(i).x); }
static double Y(int i) { return PTOD(PX(i).y); }
static int CP(double x, double y, int obj) { return collision_point_p(x, y, obj, 0, NOONE) != NOONE; }
/* collision_point(x, y, obj, -1, -1): the runner reads a GML bool as value > 0.5, so -1 is false for both prec
   (bounding box only) and notme (self not excluded). Observed: c_ice_alienboss record 213, oYeti's ledge test at
   (528, 48) hits an oDarkFall at 528, 48 whose precise mask has pixel (0, 0) clear */
static int CPn(double x, double y, int obj, int self) { (void)self; return collision_point_p(x, y, obj, 0, NOONE) != NOONE; }
static int sprw(int i) { int s = PX(i).spr; return s >= 0 ? (int)(psprite[s].w * PX(i).xscale) : 0; }
static int sprh(int i) { int s = PX(i).spr; return s >= 0 ? (int)(psprite[s].h * PX(i).yscale) : 0; }
static int bloodless_of(int i) { return PX(i).obj == OBJ_oSkeleton; }
static void blood(int self, double x, double y, int n)            /* scrCreateBlood with self's bloodless */
{
    if (bloodless_of(self)) return;
    scrCreateBlood(self, (pos)x, (pos)y, n);
}
static int caveman_like(int t) { return t == T_CAVEMAN; }          /* "Caveman" / "ManTrap" / "Yeti" / "Hawkman" */

/* the enemies' view test: x > xview - l and x < xview + 320 + r (same for y) */
static int eview(int i, int l, int r)
{
    double x = X(i), y = Y(i);
    view_read();
    return DGT(x, PW.xview - l) && DLT(x, PW.xview + 320 + r) && DGT(y, PW.yview - l) && DLT(y, PW.yview + 240 + r);
}

static int isCollisionSolid(int i)
{
    double lb, tb, rb, bb;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_rect_p(lb, tb, rb - 1, bb - 1, OBJ_oSolid, 1, i) != NOONE;
}

static int pl(void) { return PL.idx; }

/* ---- Create --------------------------------------------------------------------------------------------- */
/* objects/oEnemy/Create_0.gml (after oDrawnSprite's: type = "") */
void pen_enemy_create(int i)
{
    struct pin *p = &PX(i);
    PE(p)->hp = 1;
    p->type = T_ENONE;
    PE(p)->active = 1;
    PEN(p)->bloodLeft = 4;
    PE(p)->heavy = 1;
    PE(p)->myGrav = N(0.6);
    PEN(p)->myGravNorm = N(0.6);
    PEN(p)->myGravWater = N(0.2);
    PEN(p)->yVelLimit = N(10);
    PE(p)->bounceFactor = N(0.5);
    PE(p)->frictionFactor = N(0.3);
    PE(p)->held = 0;
    PE(p)->armed = 0;
    PE(p)->trigger = 0;
    PE(p)->safe = 0;
    PE(p)->sticky = 0;
    PE(p)->canPickUp = 1;
    PE(p)->cost = 0;
    PE(p)->forSale = 0;
    PEN(p)->sacCount = 20;
    PEN(p)->countsAsKill = 1;
    PEN(p)->burning = 0;
    PEN(p)->swimming = 0;
    PEN(p)->stunTime = 200;
    PE(p)->facing = 0;
    PEN(p)->bombID = NOONE;
}

static void make_active(struct pin *p) { PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0; }

int pen_create(int i, int fromgen)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oSnake:                                                   /* objects/oSnake/Create_0.gml */
        pen_enemy_create(i);
        make_active(p);
        setCollisionBounds(i, 2, 0, 14, 16);
        PE(p)->xVel = N(2.5);
        p->ispd = (img_t)0.4;
        p->type = T_SNAKE;
        PE(p)->hp = 1;
        p->invincible = 0;
        PE(p)->status = 0;
        PEN(p)->bounced = 0;
        PEN(p)->edead = 0;
        PE(p)->counter = 0;
        PE(p)->facing = E_RIGHT;
        return 1;
    case OBJ_oBat:                                                     /* objects/oBat/Create_0.gml */
        pen_enemy_create(i);
        p->ispd = (img_t)0.5;
        setCollisionBounds(i, 2, 2, 14, 14);
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        PE(p)->xAcc = N(0.2);
        PE(p)->yAcc = N(0.2);
        PE(p)->hp = 1;
        p->invincible = 0;
        PE(p)->status = 0;
        return 1;
    case OBJ_oSpiderHang:                                              /* objects/oSpiderHang/Create_0.gml */
        pen_enemy_create(i);
        make_active(p);
        setCollisionBounds(i, 4, 0, 12, 12);
        p->ispd = (img_t)0.4;
        PE(p)->hp = 1;
        p->invincible = 0;
        PE(p)->status = 0;
        return 1;
    case OBJ_oSpider:                                                  /* objects/oSpider/Create_0.gml */
        pen_enemy_create(i);
        p->type = T_SPIDER;
        make_active(p);
        setCollisionBounds(i, 1, 5, 15, 16);
        PE(p)->myGrav = N(0.2);
        PEN(p)->myGravNorm = N(0.2);
        p->ispd = (img_t)0.4;
        PE(p)->hp = 1;
        p->invincible = 0;
        PE(p)->status = 0;
        return 1;
    case OBJ_oGiantSpiderHang:                                         /* objects/oGiantSpiderHang/Create_0.gml */
        pen_enemy_create(i);
        make_active(p);
        setCollisionBounds(i, 0, 0, 32, 16);
        p->ispd = (img_t)0.4;
        PE(p)->hp = 10;
        p->invincible = 0;
        PE(p)->status = 0;
        if (!fromgen) {                                                /* :25 (the generator made them) */
            pin_create(PX(i).x, PX(i).y + PI(16), OBJ_oWeb);
            pin_create(PX(i).x + PI(16), PX(i).y + PI(16), OBJ_oWeb);
        }
        return 1;
    case OBJ_oGiantSpider:                                             /* objects/oGiantSpider/Create_0.gml */
        pen_enemy_create(i);
        p->type = T_GIANTSPIDER;
        make_active(p);
        setCollisionBounds(i, 2, 16, 30, 32);
        PE(p)->myGrav = N(0.3);
        PEN(p)->myGravNorm = N(0.3);
        p->ispd = (img_t)0.8;
        PE(p)->hp = 1;
        p->invincible = 0;
        PEN(p)->whipped = 10;
        PEN(p)->squirtTimer = (int16_t)RAND(100, 1000);
        PE(p)->status = 0;
        snd_play(SND_xgiantspider);                                            /* :32 */
        return 1;
    case OBJ_oCaveman:                                                 /* objects/oCaveman/Create_0.gml */
        pen_enemy_create(i);
        make_active(p);
        setCollisionBounds(i, 2, 0, sprw(i) - 2, sprh(i));
        PE(p)->xVel = N(2.5);
        p->ispd = (img_t)0.5;
        PE(p)->myGrav = N(0.6);
        p->type = T_CAVEMAN;
        PE(p)->hp = 3;
        p->invincible = 0;
        PE(p)->status = 0;
        PEN(p)->bounced = 0;
        PEN(p)->edead = 0;
        PE(p)->counter = 0;
        PEN(p)->sightCounter = 0;
        PE(p)->facing = E_RIGHT;
        PE(p)->colLeft = PE(p)->colRight = PE(p)->colBot = PE(p)->colTop = 0;
        return 1;
    case OBJ_oSkeleton:                                                /* objects/oSkeleton/Create_0.gml */
        pen_enemy_create(i);
        make_active(p);
        setCollisionBounds(i, 2, 0, 14, 16);
        PE(p)->xVel = 0;
        p->ispd = (img_t)0.5;
        p->type = T_SKELETON;
        PE(p)->hp = 1;
        p->invincible = 0;
        PE(p)->status = 0;
        PEN(p)->bounced = 0;
        PEN(p)->edead = 0;
        PE(p)->counter = 20;
        PE(p)->facing = E_RIGHT;
        if (pl() != NOONE && DLT(X(pl()), X(i) + 8)) PE(p)->facing = E_LEFT;
        PE(p)->colLeft = PE(p)->colRight = 0;
        return 1;
    case OBJ_oFakeBones:                                               /* objects/oFakeBones/Create_0.gml */
        p->type = T_NONE;
        PE(p)->yVel = 0;
        PE(p)->yAcc = N(0.2);
        return 1;
    case OBJ_oEnemySight:                                              /* objects/oEnemySight/Create_0.gml */
        PEN(p)->owner = 0;
        return 1;
    case OBJ_oBone:                                                    /* objects/oBone/Create_0.gml */
        create_detritus(i);
        p = &PX(i);
        p->ispd = (img_t)0.3;
        make_active(p);
        setCollisionBounds(i, -4, -4, 4, 4);
        {
            double a = prandom(4);
            double b = prandom(4);
            PE(p)->xVel = ND(a - b);
        }
        PE(p)->yVel = ND(-1 - prandom(2));
        PE(p)->grav = NMUL(NI(RAND(1, 6)), N(0.1));
        return 1;
    case OBJ_oWebBall:                                                 /* objects/oWebBall/Create_0.gml */
        p->type = T_NONE;
        PE(p)->yVel = ND(-1 * (prandom(3) + 1));
        PE(p)->xVel = NI(RAND(1, 3));
        if (RAND(1, 2) == 1) PE(p)->xVel = NMUL(PE(p)->xVel, N(-1));
        PE(p)->life = NI(RAND(20, 100));
        p->invincible = 1;
        return 1;
    case OBJ_oSplash: p->type = T_NONE; p->ispd = (img_t)0.6; return 1;
    case OBJ_oYellHelp:                                                /* objects/oYellHelp/Create_0.gml */
        p->type = T_NONE;
        PE(p)->yVel = N(0.1);
        PE(p)->yAcc = N(0.1);
        PE(p)->alarm[0] = 40;
        pin_set_sprite(i, GSPR_sYellHelp);                             /* global.sYellHelpNew (English) */
        return 1;
    case OBJ_oBoulder:                                                 /* objects/oBoulder/Create_0.gml */
        p->invincible = 0;                                             /* oMovingSolid Create (after oSolid's) */
        p->shopWall = 0;
        p->type = T_NONE;
        p->cleanDeath = 0;
        make_active(p);
        setCollisionBounds(i, -14, -16, 14, 16);
        PE(p)->myGrav = N(0.6);
        p->invincible = 1;
        PEN(p)->bounced = 0;
        PE(p)->colLeft = PE(p)->colRight = 0;
        return 1;
    case OBJ_oScarab: case OBJ_oFrog: case OBJ_oFireFrog: case OBJ_oZombie: case OBJ_oVampire: case OBJ_oMonkey:
    case OBJ_oManTrap: case OBJ_oHawkman: case OBJ_oYeti: case OBJ_oYetiKing:
        if (!fromgen) PUNTR(5002);
        pen_enemy_create(i);
        return 1;
    }
    return 0;
}

/* ---- the parent Step: objects/oEnemy/Step_0.gml ------------------------------------------------------- */
int pen_hit_common(int e, int kind)
{
    /* the `with obj` block of oItem Step :244-310 (kind 0) and oEnemy Step :180-261 (kind 1) */
    struct pin *o = &PX(e);
    if (o->type == T_CAVEMAN) {
        if (kind == 0 ? PE(o)->status != E_STUNNED : PE(o)->status < E_STUNNED) {
            blood(e, X(e) + 8, Y(e) + 8, 1);
            o = &PX(e);
            PE(o)->hp -= 1;
            PE(o)->status = E_STUNNED;
            PE(o)->counter = PEN(o)->stunTime;
            PE(o)->yVel = N(-6);
            snd_play(SND_xhit);                                                /* oItem :263, oEnemy :198 */
        }
    } else if (o->type == T_SHOPKEEPER) {
        if (PE(o)->status < 98) {
            blood(e, X(e), Y(e), 1);
            o = &PX(e);
            PE(o)->hp -= 1;
            PE(o)->yVel = N(-6);
            PE(o)->status = 2;
            snd_play(SND_xhit);                                                /* :274, :209 */
        }
    } else if (o->type == T_GIANTSPIDER) {
        if (PEN(o)->whipped == 0) {
            blood(e, X(e) + 16, Y(e) + 24, 1);
            o = &PX(e);
            PE(o)->hp -= 1;
            PEN(o)->whipped = 10;
            snd_play(SND_xhit);                                                /* :284, :219 */
        }
    } else if (o->obj == OBJ_oManTrap || o->obj == OBJ_oVampire || o->obj == OBJ_oYeti || o->obj == OBJ_oHawkman ||
               o->obj == OBJ_oTombLord || o->obj == OBJ_oAlienBoss || o->obj == OBJ_oUFO) {
        PUNTR(5010);
    } else {
        blood(e, X(e) + 8, Y(e) + 8, 1);
        o = &PX(e);
        PE(o)->hp -= 1;
        snd_play(SND_xhit);                                                    /* :324, :258 */
    }
    return 0;
}

/* returns 0 if the instance is gone */
void pen_parent_step(int i)
{
    struct pin *p = &PX(i);
    if (!eview(i, 20, 4)) {
        PE(p)->active = 0;
        return;
    }
    PE(p)->active = 1;
    if (PE(p)->held) {                                                     /* :13 */
        struct pin *q = &PX(pl());
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        PE(p)->myGrav = 0;
        if (PL.facing == LEFT) { pin_setx(p, q->x - PI(12)); PE(p)->facing = 0; }
        if (PL.facing == RIGHT) { pin_setx(p, q->x - PI(4)); PE(p)->facing = 1; }
        if (PL.state == DUCKING && NLT(NABS(PE(q)->xVel), N(2))) pin_sety(p, q->y - PI(10));
        else pin_sety(p, q->y - PI(12));
        pin_setdepth(p, 1);
        if (PL.holdItem == NOONE || PE(p)->status < 98) PE(p)->held = 0;
    } else
        pin_setdepth(p, 60);
    if (CPn(X(i) + dfloor(sprw(i) / 2.0), Y(i) + dfloor(sprh(i) / 2.0), OBJ_oWaterSwim, i)) {   /* :34 */
        if (!PEN(p)->swimming) {
            pin_create((pos)(X(i) + dfloor(sprw(i) / 2.0)), p->y, OBJ_oSplash);
            p = &PX(i);
            PEN(p)->swimming = 1;
            snd_play(SND_xsplash);                                             /* :40 */
        }
        PE(p)->myGrav = PEN(p)->myGravWater;
        if (p->obj == OBJ_oFireFrog) PUNTR(5011);
    } else {
        PEN(p)->swimming = 0;
        PE(p)->myGrav = PEN(p)->myGravNorm;
    }
    if (PEN(p)->burning > 0) {                                              /* :57 */
        if (RAND(1, 5) == 1) {
            int yy = RAND(0, sprh(i)), xx = RAND(0, sprw(i));
            pin_create(p->x + PI(xx), p->y + PI(yy), OBJ_oBurn);
            p = &PX(i);
        }
        PEN(p)->burning -= 1;
    }
    if (CP(X(i) + dfloor(sprw(i) / 2.0), Y(i) - 1, OBJ_oLava)) ptemple_world(5012, i, 0);                   /* :63 */
    if (CP(X(i) + dfloor(sprw(i) / 2.0), Y(i) + sprh(i) - 2, OBJ_oLava)) ptemple_world(5012, i, 0);
    if (collision_rect_p(X(i) + 2, Y(i) + 2, X(i) + 14, Y(i) + 14, OBJ_oSpearsLeft, 0, NOONE) != NOONE) PUNTR(5013);
    if (CP(X(i) + 8, Y(i) + 16, OBJ_oSpikes) && NGT(PE(p)->yVel, N(2))) {  /* :108 */
        int spikes = instance_place_p(i, X(i) + 8, Y(i) + 14, OBJ_oSpikes);
        if (!bloodless_of(i) && spikes != NOONE) pin_set_sprite(spikes, GSPR_sSpikesBlood);
        if (PE(p)->hp > 0) {
            PE(p)->hp = 0;
            PEN(p)->countsAsKill = 0;
            if (!bloodless_of(i)) blood(i, X(i) + sprw(i) / 2.0, Y(i) + sprh(i) / 2.0, 3);
            p = &PX(i);
            if (caveman_like(p->type) || p->type == T_SHOPKEEPER) PE(p)->status = 99;
        }
        PE(p)->myGrav = 0;
        PE(p)->xVel = 0;
        PE(p)->yVel = N(0.2);
    }
    if (PE(p)->status >= 98) {                                             /* :131 sacrifice */
        if (!PE(p)->held && NEQ(PE(p)->xVel, N(0)) && NEQ(PE(p)->yVel, N(0))) {
            if (CP(X(i) + 8, Y(i) + 16, OBJ_oSacAltarLeft)) pitems_world(5014, i, 0);
        } else
            PEN(p)->sacCount = 20;
    }
    if (PE(p)->status == 98 && (NGT(NABS(PE(p)->xVel), N(2)) || NGT(NABS(PE(p)->yVel), N(2)))) {   /* :170 projectile */
        int obj = collision_rect_p(X(i), Y(i), X(i) + 16, Y(i) + 16, OBJ_oEnemy, 0, i);
        if (obj != NOONE) {
            if (!PX(obj).invincible && PX(obj).obj != OBJ_oMagmaMan) {
                if (PE(&PX(obj))->status < 98) PE(&PX(obj))->xVel = PE(p)->xVel;
                pen_hit_common(obj, 1);
                /* type "Arrow" / "Fish Bone": not enemies */
            }
        }
    }
}

/* ---- oSnake: objects/oSnake/Step_0.gml --------------------------------------------------------------- */
/* if (countsAsKill) { global.enemyKills[k] (statistics), global.<kind> += 1, global.kills += 1 } */
static void kill_count(int i)
{
    if (!PEN(&PX(i))->countsAsKill) return;
    switch (PX(i).obj) {
    case OBJ_oSnake: PG.snakes += 1; break;
    case OBJ_oBat: PG.bats += 1; break;
    case OBJ_oSpider: case OBJ_oSpiderHang: PG.spiders += 1; break;
    case OBJ_oGiantSpider: case OBJ_oGiantSpiderHang: PG.giantspiders += 1; break;
    case OBJ_oCaveman: PG.cavemen += 1; break;
    case OBJ_oSkeleton: PG.skeletons += 1; break;
    }
    PG.kills += 1;
}

static void snake_step(int i)
{
    struct pin *p;
    pen_parent_step(i);
    p = &PX(i);
    if (!eview(i, 20, 4)) return;
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    PE(p)->yVel += PE(p)->myGrav;
    if (NGT(PE(p)->yVel, PEN(p)->yVelLimit)) PE(p)->yVel = PEN(p)->yVelLimit;
    if (CP(X(i) + 8, Y(i) + 8, OBJ_oSolid)) PE(p)->hp = 0;
    if (PE(p)->hp < 1) {                                                   /* :15 */
        blood(i, X(i) + 8, Y(i) + 8, 3);
        kill_count(i);
        pin_destroy(i);
    }
    p = &PX(i);
    if (isCollisionBottom(i, 1) && PE(p)->status != E_STUNNED) PE(p)->yVel = 0;
    if (PE(p)->status == 0) {                                              /* IDLE */
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        else {
            PE(p)->facing = (int16_t)RAND(0, 1);
            PE(p)->status = 1;
        }
    } else if (PE(p)->status == 1) {                                       /* WALK */
        double x = X(i), y = Y(i);
        if (isCollisionLeft(i, 1) || isCollisionRight(i, 1))
            PE(p)->facing = PE(p)->facing == E_LEFT ? E_RIGHT : E_LEFT;
        if (PE(p)->facing == E_LEFT && !CPn(x - 1, y + 16, OBJ_oSolid, i)) PE(p)->facing = E_RIGHT;
        else if (PE(p)->facing == E_RIGHT && !CPn(x + 16, y + 16, OBJ_oSolid, i)) PE(p)->facing = E_LEFT;
        if ((!CPn(x - 1, y + 16, OBJ_oSolid, i) || CPn(x - 1, y, OBJ_oSolid, i)) &&
            (!CPn(x + 16, y + 16, OBJ_oSolid, i) || CPn(x + 16, y, OBJ_oSolid, i))) {
            if (CPn(x - 1, y, OBJ_oSolid, i)) PE(p)->facing = E_RIGHT;
            else PE(p)->facing = E_LEFT;
            PE(p)->xVel = 0;
        } else if (PE(p)->facing == E_LEFT) PE(p)->xVel = N(-1);
        else PE(p)->xVel = N(1);
        if (RAND(1, 100) == 1) {
            PE(p)->status = 0;
            PE(p)->counter = (int16_t)RAND(20, 50);
            PE(p)->xVel = 0;
        }
    }
    if (isCollisionSolid(i)) pin_sety(p, p->y - (PI(2)));                            /* :77 */
    if (PE(p)->status != E_STUNNED) {
        p->ispd = NEQ(PE(p)->xVel, N(0)) ? (img_t)0.2 : (img_t)0.4;
        pin_set_sprite(i, GSPR_sSnakeWalkL);
    }
}

/* ---- oBat: objects/oBat/Step_0.gml (no inherited Step) ---------------------------------------------- */
static void bat_fly(struct pin *p, double dir)
{
    PE(p)->xVel = ND(1 * pcos_cr(degtorad_d(dir)));
    PE(p)->yVel = ND(-1 * psin_cr(degtorad_d(dir)));
}

static void bat_step(int i)
{
    struct pin *p = &PX(i), *q;
    double dist, dir, x, y, qx, qy;
    if (!eview(i, 20, 4)) return;
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    if (CP(X(i) + 8, Y(i) + 8, OBJ_oSolid)) PE(p)->hp = -999;
    if (PE(p)->hp < 1) {
        blood(i, X(i) + 8, Y(i) + 8, 3);
        kill_count(i);
        pin_destroy(i);
    }
    p = &PX(i);
    q = &PX(pl());
    x = X(i);
    y = Y(i);
    qx = PTOD(q->x);
    qy = PTOD(q->y);
    dir = 0;
    dist = point_distance_d(x + 8, y + 8, qx, qy);
    if (PE(p)->status == 0) {                                              /* HANG */
        if (!PL.swimming && !PL.dead && ((DLT(dist, 90) && DGT(qy, y + 16)) || !CP(x + 8, y - 1, OBJ_oSolid))) {
            PE(p)->status = 1;
            snd_play(SND_xbat);                                                /* :29 */
        }
        pin_set_sprite(i, GSPR_sBatHang);
    } else if (!PL.swimming && !PL.dead) {
        if (DLT(dist, 160)) {
            dir = point_direction_d(x + 8, y + 8, qx, qy);
            if (isCollisionRight(i, 1) && DGT(qx, x + 8)) dir = DLT(qy, y + 8) ? 90 : 270;
            if (isCollisionLeft(i, 1) && DLT(qx, x + 8)) dir = DLT(qy, y + 8) ? 90 : 270;
            if (isCollisionTop(i, 1) && DLT(qy, y + 8) && DGT(qx - x < 0 ? x - qx : qx - x, 8))
                dir = DLT(qx, x + 8) ? 180 : 0;
            if (isCollisionBottom(i, 1) && DGT(qy, y + 8) && DGT(qx - x < 0 ? x - qx : qx - x, 8))
                dir = DLT(qx, x + 8) ? 180 : 0;
            if (CP(x + 8, y + 16, OBJ_oWater) && DGT(dir, 180) && DLT(dir, 360)) dir = 90;
            if (!CP(x, y + 12, OBJ_oWater) || DLT(qy, y)) bat_fly(p, dir);
        } else {
            if (CP(x + 8, y - 1, OBJ_oSolid)) PE(p)->status = 0;
            else bat_fly(p, 90);
        }
        pin_set_sprite(i, DLT(qx, x + 8) ? GSPR_sBatLeft : GSPR_sBatRight);
    } else {
        if (CP(x + 8, y - 1, OBJ_oSolid)) PE(p)->status = 0;
        else bat_fly(p, 90);
    }
}

/* ---- oSpiderHang / oGiantSpiderHang: objects/oSpiderHang/Step_0.gml, objects/oGiantSpiderHang/Step_0.gml --- */
static void spiderhang_step(int i, int giant)
{
    struct pin *p = &PX(i);
    int c = instance_first_p(OBJ_oCharacter);
    double dist;
    if (giant ? !eview(i, 32, 0) : !eview(i, 20, 4)) return;
    dist = distance_to_object_p(i, OBJ_oCharacter);
    if (CP(X(i) + (giant ? 16 : 8), Y(i) + (giant ? 8 : 4), OBJ_oSolid)) PE(p)->hp = 0;
    if (PE(p)->hp < 1) {
        if (giant) {                                                   /* :11 */
            int n = RAND(1, 3), k;
            for (k = 0; k < n; k++) {
                int r = RAND(1, 3), gem;
                gem = pin_create(PX(i).x + PI(16), PX(i).y + PI(24),
                                 r == 1 ? OBJ_oEmeraldBig : r == 2 ? OBJ_oSapphireBig : OBJ_oRubyBig);
                {
                    int a = RAND(0, 3), b = RAND(0, 3);
                    PE(&PX(gem))->xVel = NI(a - b);
                }
                PE(&PX(gem))->yVel = N(-2);
            }
            {
                int obj = pin_create(PX(i).x + PI(16), PX(i).y + PI(24), OBJ_oPaste);
                PE(&PX(obj))->cost = 0;
                PE(&PX(obj))->forSale = 0;
            }
            blood(i, X(i) + 16, Y(i) + 24, 4);
            kill_count(i);
        } else {
            blood(i, X(i) + 8, Y(i) + 8, 3);
            PG.spiders += 1;                                           /* no countsAsKill test here */
            PG.kills += 1;
        }
        pin_destroy(i);
    } else if ((giant && PE(&PX(i))->hp < 10) || !CP(X(i), Y(i) - 16, OBJ_oSolid) ||
               (DLT(dist, 90) && c != NOONE && DGT(Y(c), Y(i)) &&
                DLT(X(c) - (X(i) + (giant ? 16 : 8)) < 0 ? (X(i) + (giant ? 16 : 8)) - X(c) : X(c) - (X(i) + (giant ? 16 : 8)), 8))) {
        int hp = PE(&PX(i))->hp;
        int s = pin_create(PX(i).x, PX(i).y, giant ? OBJ_oGiantSpider : OBJ_oSpider);
        PE(&PX(s))->hp = (int16_t)hp;
        pin_destroy(i);
    }
}

/* ---- oSpider: objects/oSpider/Step_0.gml ------------------------------------------------------------- */
static void spider_step(int i)
{
    struct pin *p;
    int c;
    double dist;
    pen_parent_step(i);
    p = &PX(i);
    if (eview(i, 20, 4)) {
        moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
        PE(p)->yVel += PE(p)->myGrav;
        if (NGT(PE(p)->yVel, PEN(p)->yVelLimit)) PE(p)->yVel = PEN(p)->yVelLimit;
        if (CP(X(i) + 8, Y(i) + 8, OBJ_oSolid)) PE(p)->hp = 0;
        if (PE(p)->hp < 1) {
            blood(i, X(i) + 8, Y(i) + 8, 3);
            kill_count(i);
            pin_destroy(i);
        }
        p = &PX(i);
        if (isCollisionRight(i, 1)) PE(p)->xVel = N(1);
        if (isCollisionLeft(i, 1)) PE(p)->xVel = N(-1);
        dist = distance_to_object_p(i, OBJ_oCharacter);
        if (PE(p)->status == 0) {                                          /* IDLE */
            PE(p)->alarm[0] = RAND(5, 20);
            PE(p)->status = 2;
        } else if (PE(p)->status == 2) {                                   /* RECOVER */
            if (isCollisionBottom(i, 1)) PE(p)->xVel = 0;
        } else if (PE(p)->status == 1 && DLT(dist, 90)) {                  /* BOUNCE */
            if (isCollisionBottom(i, 1)) {
                PE(p)->yVel = NI(-1 * RAND(2, 5));
                c = instance_first_p(OBJ_oCharacter);
                PE(p)->xVel = (c != NOONE && DLT(X(c), X(i) + 8)) ? N(-2.5) : N(2.5);
                if (RAND(1, 4) == 1) { PE(p)->status = 0; PE(p)->xVel = 0; PE(p)->yVel = 0; }
            }
        } else if (PE(p)->status != 4)
            PE(p)->status = 0;
        if (isCollisionTop(i, 1)) PE(p)->yVel = N(1);
    }
    if (CP(X(i) + 8, Y(i) + 8, OBJ_oWater) && PE(p)->status != 4) {       /* :84 */
        PE(p)->status = 4;
        pin_set_sprite(i, GSPR_sSpiderDrowning);
        PE(p)->alarm[1] = 30;
        PE(p)->xVel = 0;
        PE(p)->yVel = N(0.2);
        pin_create(p->x + PI(8), p->y, OBJ_oSplash);
        snd_play(SND_xsplash);                                                 /* :92 */
    }
}

/* ---- oGiantSpider: objects/oGiantSpider/Step_0.gml (no inherited Step) ------------------------------- */
static void giantspider_step(int i)
{
    struct pin *p = &PX(i);
    int c;
    double dist;
    if (!eview(i, 32, 0)) return;
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    PE(p)->yVel += PE(p)->myGrav;
    if (NGT(PE(p)->yVel, PEN(p)->yVelLimit)) PE(p)->yVel = PEN(p)->yVelLimit;
    if (PEN(p)->whipped > 0) PEN(p)->whipped -= 1;
    if (CP(X(i) + 16, Y(i) + 24, OBJ_oSolid)) PE(p)->hp = 0;
    if (PE(p)->hp < 1) {                                                   /* :16 */
        int n = RAND(1, 3), k;
        for (k = 0; k < n; k++) {
            int r = RAND(1, 3), gem;
            gem = pin_create(PX(i).x + PI(16), PX(i).y + PI(24),
                             r == 1 ? OBJ_oEmeraldBig : r == 2 ? OBJ_oSapphireBig : OBJ_oRubyBig);
            {
                int a = RAND(0, 3), b = RAND(0, 3);
                PE(&PX(gem))->xVel = NI(a - b);
            }
            PE(&PX(gem))->yVel = N(-2);
        }
        {
            int obj = pin_create(PX(i).x + PI(16), PX(i).y + PI(24), OBJ_oPaste);
            PE(&PX(obj))->cost = 0;
            PE(&PX(obj))->forSale = 0;
        }
        blood(i, X(i) + 16, Y(i) + 24, 4);
        kill_count(i);
        pin_destroy(i);
    }
    p = &PX(i);
    c = instance_first_p(OBJ_oCharacter);
    if (isCollisionRight(i, 1)) PE(p)->xVel = N(1);
    if (isCollisionLeft(i, 1)) PE(p)->xVel = N(-1);
    if (isCollisionTop(i, 1) && isCollisionBottom(i, 1) && PE(p)->status != 3) {   /* :53 */
        PE(p)->status = 3;
        PE(p)->xVel = (c != NOONE && DLT(X(c), X(i) + 16)) ? N(-1) : N(1);
    }
    dist = distance_to_object_p(i, OBJ_oCharacter);
    if (PEN(p)->squirtTimer > 0) PEN(p)->squirtTimer -= 1;
    if (PE(p)->status == 0) {                                              /* IDLE */
        if (p->spr != GSPR_sGiantSpiderFlip) pin_set_sprite(i, GSPR_sGiantSpider);
        PE(p)->alarm[0] = RAND(5, 20);
        if (PEN(p)->squirtTimer == 0) PE(p)->status = 5;
        else PE(p)->status = 2;
    } else if (PE(p)->status == 3) {                                       /* CRAWL */
        pin_set_sprite(i, GSPR_sGiantSpiderCrawl);
        if (!isCollisionTop(i, 1) || !isCollisionBottom(i, 1)) PE(p)->status = 0;
        else if (isCollisionRight(i, 1)) PE(p)->xVel = N(-1);
        else if (isCollisionLeft(i, 1)) PE(p)->xVel = N(1);
    } else if (PE(p)->status == 5) {                                       /* SQUIRT */
        pin_set_sprite(i, GSPR_sGiantSpiderSquirt);
        if (DGE(p->img, 5) && PEN(p)->squirtTimer == 0) {
            pin_create(p->x + PI(16), p->y + PI(16), OBJ_oWebBall);
            p = &PX(i);
            PEN(p)->squirtTimer = (int16_t)RAND(100, 1000);
        }
    } else if (PE(p)->status == 2) {                                       /* RECOVER */
        if (isCollisionBottom(i, 1)) PE(p)->xVel = 0;
    } else if (PE(p)->status == 1 && DLT(dist, 120)) {                     /* BOUNCE */
        pin_set_sprite(i, GSPR_sGiantSpiderJump);
        if (isCollisionBottom(i, 1)) {
            pin_set_sprite(i, GSPR_sGiantSpider);
            PE(p)->yVel = NI(-1 * RAND(3, 6));
            PE(p)->xVel = (c != NOONE && DLT(X(c), X(i) + 16)) ? N(-2.5) : N(2.5);
            snd_play(SND_xspiderjump);                                         /* :108 */
            if (RAND(1, 4) == 1) { PE(p)->status = 0; PE(p)->xVel = 0; PE(p)->yVel = 0; }
        }
    } else if (PE(p)->status != 4)
        PE(p)->status = 0;
    if (isCollisionTop(i, 1)) PE(p)->yVel = N(1);
}

/* ---- oCaveman: objects/oCaveman/Step_0.gml ----------------------------------------------------------- */
void scrCheckCollisions(int i)
{
    struct pin *p = &PX(i);
    setCollisionBounds(i, 2, 6, 14, 16);
    if (PE(p)->colLeft && !PE(p)->colRight) pin_setx(p, p->x + (PI(1)));
    else if (PE(p)->colRight) pin_setx(p, p->x - (PI(1)));
    if (PE(p)->colLeft || PE(p)->colRight) PE(p)->xVel = NMUL(-PE(p)->xVel, N(0.5));
    if (PE(p)->colTop && !PE(p)->colBot) pin_sety(p, p->y + (PI(1)));
    else if (PE(p)->colBot) {
        if (NGT(PE(p)->yVel, N(1))) PE(p)->yVel = NMUL(-PE(p)->yVel, N(0.5));
        else if (NLT(NABS(PE(p)->yVel), N(1))) PE(p)->yVel = 0;
        if (NLT(NABS(PE(p)->xVel), N(0.1))) PE(p)->xVel = 0;
        else if (NNE(NABS(PE(p)->xVel), N(0))) PE(p)->xVel = NMUL(PE(p)->xVel, N(0.3));
    }
}

static void caveman_sight(int i)
{
    struct pin *p = &PX(i);
    if (PEN(p)->sightCounter > 0) PEN(p)->sightCounter -= 1;
    else {
        int s = pin_create(p->x, p->y, OBJ_oEnemySight);
        double dir = PE(&PX(i))->facing == E_LEFT ? 180 : 0;
        PE(&PX(s))->direction = dir;
        PEN(&PX(s))->hspeed = 10 * pcos_cr(degtorad_d(dir));                  /* speed = 10 */
        PEN(&PX(s))->vspeed = -10 * psin_cr(degtorad_d(dir));
        PEN(&PX(s))->owner = (int16_t)instance_place_p(i, X(i), Y(i), OBJ_oCaveman);
        PEN(&PX(i))->sightCounter = 5;
    }
}

static void caveman_step(int i)
{
    struct pin *p;
    pen_parent_step(i);
    p = &PX(i);
    if (!PE(p)->active) return;
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    if (!PE(p)->held) PE(p)->yVel += PE(p)->myGrav;
    if (NGT(PE(p)->yVel, PEN(p)->yVelLimit)) PE(p)->yVel = PEN(p)->yVelLimit;
    PE(p)->colLeft = PE(p)->colRight = PE(p)->colBot = PE(p)->colTop = 0;
    if (isCollisionLeft(i, 1)) PE(p)->colLeft = 1;
    if (isCollisionRight(i, 1)) PE(p)->colRight = 1;
    if (isCollisionBottom(i, 1)) PE(p)->colBot = 1;
    if (isCollisionTop(i, 1)) PE(p)->colTop = 1;
    if (PE(p)->status >= E_STUNNED) {                                      /* :18 */
        if (CP(X(i) + 8, Y(i) + 12, OBJ_oSolid)) {
            blood(i, X(i) + 8, Y(i) + 8, 3);
            snd_play(SND_xcavemandie);                                         /* :23 */
            pin_destroy(i);
        }
    } else if (!PE(p)->held) {
        if (CP(X(i) + 8, Y(i) + 8, OBJ_oSolid)) {
            blood(i, X(i) + 8, Y(i) + 8, 3);
            snd_play(SND_xcavemandie);                                         /* :32 */
            pin_destroy(i);
        }
    }
    p = &PX(i);
    if (PE(p)->status != E_DEAD && PE(p)->status != E_STUNNED && PE(p)->hp < 1) PE(p)->status = E_DEAD;
    if (PE(p)->colBot && PE(p)->status != E_STUNNED) PE(p)->yVel = 0;
    if (PE(p)->status == 0) {                                              /* IDLE :45 */
        PEN(p)->bounced = 0;
        if (PE(p)->colBot && (CPn(X(i) - 1, Y(i), OBJ_oSolid, i) || CPn(X(i) + 16, Y(i), OBJ_oSolid, i))) {
            PE(p)->yVel = N(-6);
            PE(p)->xVel = PE(p)->facing == E_LEFT ? N(-1) : N(1);
            PE(p)->counter -= 10;
        }
        if (NLT(PE(p)->yVel, N(0)) && PE(p)->colTop) PE(p)->yVel = 0;
        if (PE(p)->colBot && PE(p)->counter > 0) PE(p)->counter -= 1;
        if (PE(p)->counter < 1) {
            PE(p)->facing = (int16_t)RAND(0, 1);
            PE(p)->status = 1;
        }
        caveman_sight(i);
    } else if (PE(p)->status == 1) {                                       /* WALK :79 */
        if (PE(p)->colLeft || PE(p)->colRight) PE(p)->facing = PE(p)->facing == E_LEFT ? E_RIGHT : E_LEFT;
        if (PE(p)->facing == E_LEFT) {
            if (!CPn(X(i) - 1, Y(i) + 16, OBJ_oSolid, i)) {
                PE(p)->status = 0;
                PE(p)->counter = (int16_t)RAND(20, 50);
                PE(p)->xVel = 0;
            }
            PE(p)->xVel = N(-1.5);
        } else {
            if (!CPn(X(i) + 16, Y(i) + 16, OBJ_oSolid, i)) {
                PE(p)->status = 0;
                PE(p)->counter = (int16_t)RAND(20, 50);
                PE(p)->xVel = 0;
            }
            PE(p)->xVel = N(1.5);
        }
        if (RAND(1, 100) == 1) {
            PE(p)->status = 0;
            PE(p)->counter = (int16_t)RAND(20, 50);
            PE(p)->xVel = 0;
        }
        caveman_sight(i);
    } else if (PE(p)->status == 2) {                                       /* ATTACK :126 */
        p->ispd = 1;
        if (PE(p)->colLeft || PE(p)->colRight) PE(p)->facing = PE(p)->facing == E_LEFT ? E_RIGHT : E_LEFT;
        PE(p)->xVel = PE(p)->facing == E_LEFT ? N(-3) : N(3);
    } else if (PE(p)->status == E_STUNNED) {                               /* :137 */
        if (NEQ(PE(p)->xVel, N(0)) && PE(p)->hp > 0) pin_set_sprite(i, GSPR_sCavemanStunL);
        else if (PEN(p)->bounced) pin_set_sprite(i, NLT(PE(p)->yVel, N(0)) ? GSPR_sCavemanBounceL : GSPR_sCavemanFallL);
        else pin_set_sprite(i, NGT(NABS(PE(p)->xVel), N(0)) ? GSPR_sCavemanDieLL : GSPR_sCavemanDieLR);
        if (PE(p)->colBot && !PEN(p)->bounced) {
            PEN(p)->bounced = 1;
            blood(i, X(i) + 8, Y(i) + 8, 1);
            p = &PX(i);
        }
        if (PE(p)->held || PE(p)->colBot) {
            if (PE(p)->counter > 0) PE(p)->counter -= 1;
            else if (PE(p)->hp > 0) {
                PE(p)->status = 0;
                if (PE(p)->held) {
                    PE(p)->held = 0;
                    PL.holdItem = NOONE;
                    PL.pickupItemType = T_NONE;
                }
            }
        }
    } else if (PE(p)->status == E_DEAD) {                                  /* :171 */
        if (!PEN(p)->edead) {
            kill_count(i);
            snd_play(SND_xcavemandie);                                         /* :181 */
            PEN(p)->edead = 1;
        }
        pin_set_sprite(i, GSPR_sCavemanDeadL);
        if (NGT(NABS(PE(p)->xVel), N(0)) || NGT(NABS(PE(p)->yVel), N(0))) PE(p)->status = E_STUNNED;
    }
    if (PE(p)->status >= E_STUNNED) {                                      /* :190 */
        scrCheckCollisions(i);
        if (NEQ(PE(p)->xVel, N(0)) && NEQ(PE(p)->yVel, N(0)) && PE(p)->hp < 1) PE(p)->status = E_DEAD;
    }
    if (NGT(PE(p)->xVel, N(0))) PE(p)->xVel -= N(0.1);
    if (NLT(PE(p)->xVel, N(0))) PE(p)->xVel += N(0.1);
    if (NLT(NABS(PE(p)->xVel), N(0.5))) PE(p)->xVel = 0;
    if (PE(p)->status < E_STUNNED) pin_set_sprite(i, NGT(NABS(PE(p)->xVel), N(0)) ? GSPR_sCavemanRunLeft : GSPR_sCavemanLeft);
    if (PE(p)->held) pin_set_sprite(i, PE(p)->hp > 0 ? GSPR_sCavemanHeldL : GSPR_sCavemanDHeldL);
}

/* ---- oSkeleton / oFakeBones ------------------------------------------------------------------------- */
static void skeleton_step(int i)
{
    struct pin *p;
    pen_parent_step(i);
    p = &PX(i);
    if (!eview(i, 20, 4)) return;
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    PE(p)->yVel += PE(p)->myGrav;
    if (NGT(PE(p)->yVel, PEN(p)->yVelLimit)) PE(p)->yVel = PEN(p)->yVelLimit;
    if (CP(X(i) + 8, Y(i) + 8, OBJ_oSolid)) PE(p)->hp = 0;
    if (PE(p)->hp < 1) {                                                   /* :15 */
        int k, skull;
        for (k = 0; k < 3; k++) pin_create(PX(i).x + PI(8), PX(i).y + PI(8), OBJ_oBone);
        skull = pin_create(PX(i).x + PI(8), PX(i).y + PI(8), OBJ_oSkull);
        PE(&PX(skull))->yVel = NI(-RAND(1, 3));
        {
            int a = RAND(0, 3), b = RAND(0, 3);
            PE(&PX(skull))->xVel = NI(a - b);
        }
        kill_count(i);
        pin_destroy(i);
    }
    p = &PX(i);
    if (isCollisionBottom(i, 1) && PE(p)->status != E_STUNNED) PE(p)->yVel = 0;
    if (PE(p)->status == 0) {
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        if (PE(p)->counter == 0) PE(p)->status = 1;
    } else if (PE(p)->status == 1) {
        PE(p)->colLeft = PE(p)->colRight = 0;
        if (isCollisionLeft(i, 1)) PE(p)->colLeft = 1;
        if (isCollisionRight(i, 1)) PE(p)->colRight = 1;
        if (isCollisionLeft(i, 4) && isCollisionRight(i, 4)) {
        } else if (PE(p)->colLeft || PE(p)->colRight)
            PE(p)->facing = PE(p)->facing == E_LEFT ? E_RIGHT : E_LEFT;
        PE(p)->xVel = PE(p)->facing == E_LEFT ? N(-1) : N(1);
    }
    if (isCollisionSolid(i)) pin_sety(p, p->y - (PI(2)));
    if (PE(p)->status != E_STUNNED) pin_set_sprite(i, PE(p)->status == 1 ? GSPR_sSkeletonWalkLeft : GSPR_sSkeletonLeft);
}

static void fakebones_step(int i)
{
    struct pin *p = &PX(i);
    struct pin *q;
    if (!eview(i, 16, 0)) return;
    if (!CP(X(i) + 8, Y(i) + 16, OBJ_oSolid)) {
        pin_sety(p, PADDV(p->y, PE(p)->yVel));
        PE(p)->yVel += PE(p)->yAcc;
    }
    if (CP(X(i) + 8, Y(i) + 15, OBJ_oSolid)) pin_sety(p, p->y - (PI(1)));
    q = &PX(pl());
    {
        double dy = PTOD(q->y) - (Y(i) + 8), dx = PTOD(q->x) - (X(i) + 8);
        if (DLT(dy < 0 ? -dy : dy, 8) && DLT(dx < 0 ? -dx : dx, 64)) pin_set_sprite(i, GSPR_sSkeletonCreateL);
    }
}

/* ---- oWebBall, oSplash, oYellHelp, oBone, oEnemySight ------------------------------------------------- */
static void webball_step(int i)
{
    struct pin *p = &PX(i);
    pin_setx(p, PADDV(p->x, PE(p)->xVel));
    pin_sety(p, PADDV(p->y, PE(p)->yVel));
    if (NLT(PE(p)->yVel, N(6))) PE(p)->yVel += N(0.2);
    if (NGT(PE(p)->life, N(0))) PE(p)->life -= N(1);
    else pin_set_sprite(i, GSPR_sWebCreate);
}

/* ---- the boulder: objects/oBoulder/Step_0.gml, Collision_oSolid.gml ------------------------------------- */
static void boulder_step(int i)
{
    struct pin *p = &PX(i);
    PE(p)->colLeft = PE(p)->colRight = 0;
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    if (NLT(PE(p)->yVel, N(8))) PE(p)->yVel += PE(p)->myGrav;
    if (DLE(X(i) - 17, 16) && NLT(PE(p)->xVel, N(0))) {
        pin_setx(p, p->x + (PI(1)));
        PE(p)->xVel = -PE(p)->xVel;
    }
    if (DGE(X(i) + 17, 656) && NGT(PE(p)->xVel, N(0))) {
        pin_setx(p, p->x - (PI(1)));
        PE(p)->xVel = -PE(p)->xVel;
    }
    if (isCollisionTop(i, 1) && NLT(PE(p)->yVel, N(0))) PE(p)->yVel = NMUL(-PE(p)->yVel, N(0.8));
    if (isCollisionBottom(i, 1)) {
        if (NGT(PE(p)->yVel, N(3))) PE(p)->yVel = NMUL(-PE(p)->yVel, N(0.3));
        else PE(p)->yVel = 0;
        if (NNE(NABS(PE(p)->xVel), N(0))) PE(p)->xVel = NMUL(PE(p)->xVel, N(0.99));
        if (!PEN(p)->bounced && NEQ(PE(p)->xVel, N(0))) {
            PE(p)->xVel = DLT(X(pl()), X(i)) ? N(-4.5) : N(4.5);
            PEN(p)->bounced = 1;
        }
        if (NLT(NABS(PE(p)->xVel), N(0.5))) PE(p)->xVel = 0;
    }
    if (!CP(X(i), Y(i) + 16, OBJ_oSolid)) {
        PE(p)->colLeft = collision_rect_p(X(i) - 16, Y(i) - 16, X(i) - 8, Y(i) + 16, OBJ_oSolid, 0, i) != NOONE;
        PE(p)->colRight = collision_rect_p(X(i) + 8, Y(i) - 16, X(i) + 16, Y(i) + 16, OBJ_oSolid, 0, i) != NOONE;
        if (PE(p)->colLeft && !PE(p)->colRight) pin_setx(p, p->x + (PI(1)));
        else if (PE(p)->colRight && !PE(p)->colLeft) pin_setx(p, p->x - (PI(1)));
    }
    p->ispd = (img_t)(NTOD(NABS(PE(p)->xVel)) / 5);
    if (NLT(PE(p)->xVel, N(0))) pin_set_sprite(i, GSPR_sBoulderRotateL);
    else if (NGT(PE(p)->xVel, N(0))) pin_set_sprite(i, GSPR_sBoulderRotateR);
    else pin_set_sprite(i, GSPR_sBoulder);
}

static void boulder_solid(int i, int o)
{
    struct pin *p = &PX(i);
    if (PX(o).invincible || NLT(NABS(PE(p)->xVel), N(1)))
        PE(p)->xVel = NMUL(-PE(p)->xVel, N(0.5));
    else {
        if (NLT(NABS(PE(p)->xVel), N(1)) || DGT(Y(o), Y(i) + 13))
            pin_sety(p, p->y - (PI(1)));
        else {
            /* with other: tile_layer_find / tile_delete at depth 3 (drawing only), then the spikes on it */
            int sp = collision_point_p(X(o) + 8, Y(o) - 1, OBJ_oSpikes, 1, NOONE);   /* instance_position */
            if (sp != NOONE) pin_destroy(sp);
            pin_destroy(o);
            snd_play(SND_xcrunch);                                             /* :32 */
        }
        p = &PX(i);
        if (NGT(PE(p)->xVel, N(0))) PE(p)->xVel -= N(0.1);
        else if (NLT(PE(p)->xVel, N(0))) PE(p)->xVel += N(0.1);
        if (NLT(NABS(PE(p)->xVel), N(1))) PE(p)->xVel = 0;
    }
}

int pen_step(int i)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oSnake: snake_step(i); return 1;
    case OBJ_oBat: bat_step(i); return 1;
    case OBJ_oSpiderHang: spiderhang_step(i, 0); return 1;
    case OBJ_oGiantSpiderHang: spiderhang_step(i, 1); return 1;
    case OBJ_oSpider: spider_step(i); return 1;
    case OBJ_oGiantSpider: giantspider_step(i); return 1;
    case OBJ_oCaveman: caveman_step(i); return 1;
    case OBJ_oSkeleton: skeleton_step(i); return 1;
    case OBJ_oFakeBones: fakebones_step(i); return 1;
    case OBJ_oWebBall: webball_step(i); return 1;
    case OBJ_oYellHelp: pin_sety(p, PSUBV(p->y, N(0.1))); return 1;
    case OBJ_oBone:                                                    /* objects/oBone/Step_0.gml */
        detritus_step(i);
        if (isCollisionBottom(i, 1)) {
            pin_set_sprite(i, GSPR_sSmokePuff);
            PE(&PX(i))->dying = 1;
        }
        return 1;
    case OBJ_oBoulder: boulder_step(i); return 1;
    case OBJ_oSplash: case OBJ_oEnemySight: return 1;                  /* no Step */
    }
    if (obj_is(p->obj, OBJ_oEnemy) && p->obj != OBJ_oShopkeeper) {
        if (!pcontent_ev(FEV_STEP, i, 0)) PUNTR(5003);                                       /* P7 hook */
        return 1;
    }
    return 0;
}

/* ---- built-in motion (GameMaker: after the Step events, x += hspeed, y += vspeed) ------------------------ */
void pen_motion(void)
{
    int k;
    /* only oEnemySight sets hspeed / vspeed (caveman_sight; pin_add zeroes them): its instances in creation order */
    for (k = pw_ohead[OBJ_oEnemySight]; k >= 0; k = pw_inext[k]) {
        struct pin *p = &PW.in[k];
        if (!p->alive || (dzero(PEN(p)->hspeed) && dzero(PEN(p)->vspeed))) continue;
        pin_setx(p, (pos)((double)p->x + PEN(p)->hspeed));
        pin_sety(p, (pos)((double)p->y + PEN(p)->vspeed));
    }
}

/* ---- Alarms ---------------------------------------------------------------------------------------------- */
int pen_alarm(int i, int a)
{
    struct pin *p = &PX(i);
    int c;
    switch (p->obj) {
    case OBJ_oSpider:
        if (a == 0) {                                                  /* objects/oSpider/Alarm_0.gml */
            PE(p)->status = 1;
            if (isCollisionBottom(i, 1)) {
                PE(p)->yVel = NI(-1 * RAND(2, 5));
                c = instance_first_p(OBJ_oCharacter);
                PE(p)->xVel = (c != NOONE && DLT(X(c), X(i))) ? N(-2.5) : N(2.5);
            }
        } else if (a == 1)
            pin_destroy(i);
        return 1;
    case OBJ_oGiantSpider:
        if (a == 0 && p->spr != GSPR_sGiantSpiderSquirt) {             /* objects/oGiantSpider/Alarm_0.gml */
            PE(p)->status = 1;
            pin_set_sprite(i, GSPR_sGiantSpiderJump);
            if (isCollisionBottom(i, 1)) {
                pin_set_sprite(i, GSPR_sGiantSpider);
                PE(p)->yVel = NI(-1 * RAND(2, 5));
                c = instance_first_p(OBJ_oCharacter);
                PE(p)->xVel = (c != NOONE && DLT(X(c), X(i) + 16)) ? N(-2.5) : N(2.5);
            }
        }
        return 1;
    case OBJ_oWebBall:
        if (a == 0) p->invincible = 0;
        else if (a == 1) PUNTR(5020);
        return 1;
    case OBJ_oYellHelp:
        if (a == 0) pin_destroy(i);
        return 1;
    case OBJ_oGiantTikiHead:                                           /* objects/oGiantTikiHead/Alarm_0.gml */
        if (a == 0) {
            pin_set_sprite(i, GSPR_sGTHHole);
            pin_create(p->x, p->y, OBJ_oBoulder);
            snd_play(SND_xthump);                                              /* :3 */
        }
        return 1;
    }
    return 0;
}

/* ---- Animation End --------------------------------------------------------------------------------------- */
int pen_animend(int i)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oSpider:
        if (p->spr == GSPR_sSpiderFlip) pin_set_sprite(i, GSPR_sSpider);
        return 1;
    case OBJ_oGiantSpider:
        if (p->spr == GSPR_sGiantSpiderFlip) {
            pin_set_sprite(i, GSPR_sGiantSpider);
            p->ispd = (img_t)0.4;
        } else if (p->spr == GSPR_sGiantSpiderSquirt) {
            PE(p)->status = 0;
            p->ispd = (img_t)0.4;
        }
        return 1;
    case OBJ_oFakeBones:
        if (p->spr == GSPR_sSkeletonCreateL) {
            pin_create(p->x, p->y, OBJ_oSkeleton);
            pin_destroy(i);
        }
        return 1;
    case OBJ_oBone:
        if (PE(p)->dying) pin_destroy(i);
        return 1;
    case OBJ_oSplash:
        pin_destroy(i);
        return 1;
    case OBJ_oWebBall:
        if (p->spr == GSPR_sWebCreate) {
            int w = pin_create(p->x - PI(8), p->y - PI(8), OBJ_oWeb);
            PE(&PX(w))->dying = 1;
            pin_destroy(i);
        }
        return 1;
    }
    if (obj_is(p->obj, OBJ_oEnemy) && pobj[p->obj].ev & EV_ANIMEND && p->obj != OBJ_oShopkeeper) {
        if (!pcontent_ev(FEV_ANIMEND, i, 0)) PUNTR(5004);                                    /* P7 hook */
        return 1;
    }
    return 0;
}

/* ---- Collision events ----------------------------------------------------------------------------------- */
/* objects/oEnemy/Collision_oCharacter.gml (stomp or hurt) */
static void enemy_hit_player(int i, int c)
{
    struct pin *p = &PX(i), *o = &PX(c);
    double dx = PTOD(o->x) - (X(i) + 8);
    if (DGT(dx < 0 ? -dx : dx, 12)) {
    } else if (!PL.dead && (PL.state == JUMPING || PL.state == FALLING) && DLT(PTOD(o->y), Y(i) + 8) && !PL.swimming) {
        PE(o)->yVel = N(-6) - NMUL(N(0.2), PE(o)->yVel);
        if (PG.hasSpikeShoes) {
            PE(p)->hp -= (int16_t)(3 * (PL.fallTimer / 16 + 1));
            scrCreateBlood(c, o->x, o->y + PI(8), 1);
        } else
            PE(p)->hp -= (int16_t)(1 * (PL.fallTimer / 16 + 1));
        PL.fallTimer = 0;
        snd_play(SND_xhit);                                                    /* :12 */
    } else if (PL.invincible == 0) {
        PL.blink = 30;
        PL.invincible = 30;
        PE(o)->xVel = DLT(PTOD(o->x), X(i)) ? N(-6) : N(6);
        if (PG.plife > 0) PG.plife -= 1;
        /* type == "Bat" / "Piranha" / "Vampire": oBat never sets its type (it stays "NONE"; Observed:
           build/trace/p5_buy_s28 record 257, no blood) and the others are not Mines enemies */
        if (p->obj == OBJ_oPiranha || p->obj == OBJ_oVampire) PUNTR(5006);
        snd_play(SND_xhurt);                                                   /* :57 */
    }
}

/* objects/oEnemy/Collision_oWhip.gml (and oWhipPre) */
static void enemy_whipped(int i, int w)
{
    struct pin *p = &PX(i);
    (void)w;
    PE(p)->hp -= 1;                                                        /* other.damage: 1 (oWhip, oWhipPre) */
    PEN(p)->countsAsKill = 1;
    if (PEN(p)->bloodLeft > 0) {
        blood(i, X(i) + sprw(i) / 2.0, Y(i) + sprh(i) / 2.0, 1);
        p = &PX(i);
        if (PE(p)->hp < 0) PEN(p)->bloodLeft -= 1;
    }
    snd_play(SND_xhit);                                                        /* :8 */
}

/* objects/oCaveman/Collision_oCharacter.gml */
static void caveman_hit_player(int i, int c)
{
    struct pin *p = &PX(i), *o = &PX(c);
    double dx = PTOD(o->x) - (X(i) + 8);
    if (DGT(dx < 0 ? -dx : dx, 8)) {
    } else if (!PL.dead && (PL.state == JUMPING || PL.state == FALLING) && DLT(PTOD(o->y), Y(i) + 5) && !PL.swimming) {
        if (PE(p)->status < E_STUNNED) {
            PE(o)->yVel = N(-6) - NMUL(N(0.2), PE(o)->yVel);
            if (PG.hasSpikeShoes) {
                PE(p)->hp -= (int16_t)(3 * (PL.fallTimer / 16 + 1));
                pin_create(o->x, o->y + PI(8), OBJ_oBlood);
                p = &PX(i);
                o = &PX(c);
            } else
                PE(p)->hp -= (int16_t)(1 * (PL.fallTimer / 16 + 1));
            PL.fallTimer = 0;
            PEN(p)->countsAsKill = 1;
            PE(p)->status = E_STUNNED;
            PE(p)->counter = PEN(p)->stunTime;
            PE(p)->yVel = N(-6);
            if (DLT(PTOD(o->x), X(i) + 8)) PE(p)->xVel += N(1);
            else PE(p)->xVel -= N(1);
            p->ispd = (img_t)0.5;
            snd_play(SND_xhit);                                                /* :21 */
        }
    } else if (PL.invincible == 0) {
        if (PE(p)->status < E_STUNNED) {
            PL.blink = 30;
            PL.invincible = 30;
            if (DLT(PTOD(o->y), Y(i))) PE(o)->yVel = N(-6);
            PE(o)->xVel = DLT(PTOD(o->x), X(i)) ? N(-6) : N(6);
            pin_create(o->x, o->y, OBJ_oBlood);
            if (PG.plife > 0) PG.plife -= 1;
            snd_play(SND_xhurt);                                               /* :43 */
        }
    }
}

/* objects/oCaveman/Collision_oWhip.gml (and oWhipPre; other.type "Machete": not the whip) */
static void caveman_whipped(int i, int w)
{
    struct pin *p = &PX(i);
    if (PE(p)->status < E_STUNNED) {
        PE(p)->hp -= 1;
        if (PEN(p)->bloodLeft > 0) {
            blood(i, X(i) + sprw(i) / 2.0, Y(i) + sprh(i) / 2.0, 1);
            p = &PX(i);
            if (PE(p)->hp < 0) PEN(p)->bloodLeft -= 1;
        }
        PEN(p)->countsAsKill = 1;
        PE(p)->status = E_STUNNED;
        PE(p)->counter = PEN(p)->stunTime;
        PE(p)->yVel = N(-3);
        PE(p)->xVel = DLT(X(w), X(i) + 8) ? N(2) : N(-2);
        p->ispd = (img_t)0.5;
        snd_play(SND_xhit);                                                    /* :16 */
    }
}

/* objects/oGiantSpider/Collision_oCharacter.gml */
static void giant_hit_player(int i, int c)
{
    struct pin *p = &PX(i), *o = &PX(c);
    double dx = PTOD(o->x) - (X(i) + 16);
    if (DGT(dx < 0 ? -dx : dx, 16)) {
    } else if (!PL.dead && (PL.state == JUMPING || PL.state == FALLING) && DLT(PTOD(o->y), Y(i) + 16) && !PL.swimming) {
        PE(o)->yVel = N(-6) - NMUL(N(0.2), PE(o)->yVel);
        if (PG.hasSpikeShoes) {
            PE(p)->hp -= (int16_t)(3 * (PL.fallTimer / 16 + 1));
            pin_create(o->x, o->y + PI(8), OBJ_oBlood);
            p = &PX(i);
        } else
            PE(p)->hp -= (int16_t)(1 * (PL.fallTimer / 16 + 1));
        PL.fallTimer = 0;
        pin_create(p->x + PI(16), p->y + PI(24), OBJ_oBlood);
        snd_play(SND_xhit);                                                    /* :13 */
    } else if (PL.invincible == 0) {
        PL.blink = 30;
        PL.invincible = 30;
        if (DLT(PTOD(o->y), Y(i))) PE(o)->yVel = N(-6);
        PE(o)->xVel = DLT(PTOD(o->x), X(i)) ? N(-6) : N(6);
        if (PG.plife > 0) PG.plife -= 2;
        snd_play(SND_xhurt);                                                   /* :33 */
    }
}

int pen_collision(int self, int other)
{
    int so = PX(self).obj, oo = PX(other).obj;
    switch (so) {
    case OBJ_oCaveman:
        if (obj_is(oo, OBJ_oCharacter)) caveman_hit_player(self, other);
        else caveman_whipped(self, other);
        return 1;
    case OBJ_oGiantSpider:
        if (obj_is(oo, OBJ_oCharacter)) giant_hit_player(self, other);
        else if (PEN(&PX(self))->whipped == 0) {                              /* Collision_oWhip (oWhipPre: oEnemy's) */
            if (oo == OBJ_oWhip) {
                PE(&PX(self))->hp -= 1;
                pin_create(PX(self).x + PI(16), PX(self).y + PI(24), OBJ_oBlood);
                snd_play(SND_xhit);                                            /* Collision_oWhip :5 */
                PEN(&PX(self))->whipped = 10;
            } else
                enemy_whipped(self, other);
        } else if (oo == OBJ_oWhipPre)
            enemy_whipped(self, other);
        return 1;
    case OBJ_oEnemySight:
        if (obj_is(oo, OBJ_oSolid)) pin_destroy(self);
        else {                                                         /* Collision_oCharacter: with oCaveman */
            int16_t w[256];
            int n = pw_with(OBJ_oCaveman, w, 256), k;
            for (k = 0; k < n; k++) {
                int c = w[k];
                if (!PX(c).alive) continue;
                if (DLT(distance_to_object_p(c, OBJ_oPlayer1), 100) && PE(&PX(c))->status < 98) {
                    PE(&PX(c))->status = 2;
                    snd_play(SND_xalert);                                      /* oEnemySight/Collision_oCharacter :6 */
                }
            }
            if (instance_exists_p(OBJ_oHawkman)) PUNTR(5030);
        }
        return 1;
    case OBJ_oWebBall:
        if (obj_is(oo, OBJ_oEnemy) && PX(other).type == T_GIANTSPIDER) return 1;
        pin_set_sprite(self, GSPR_sWebCreate);
        PE(&PX(self))->xVel = 0;
        PE(&PX(self))->yVel = 0;
        return 1;
    case OBJ_oBoulder: boulder_solid(self, other); return 1;
    case OBJ_oWeb:                                                     /* objects/oWeb/Collision_oEnemy.gml */
        if (!obj_is(oo, OBJ_oEnemy)) return 0;
        if (PX(other).type != T_SPIDER && PX(other).type != T_GIANTSPIDER) {
            PE(&PX(other))->xVel = 0;
            PE(&PX(other))->yVel = 0;
        }
        if (oo == OBJ_oMagmaMan) pin_destroy(self);
        if (PX(other).type == T_SHOPKEEPER && PE(&PX(other))->hp > 0) PE(&PX(other))->status = 2;
        return 1;
    }
    if (obj_is(so, OBJ_oEnemy) && so != OBJ_oShopkeeper) {
        if (so == OBJ_oSnake || so == OBJ_oBat || so == OBJ_oSpider || so == OBJ_oSpiderHang ||
            so == OBJ_oGiantSpiderHang || so == OBJ_oSkeleton) {
            if (obj_is(oo, OBJ_oCharacter)) enemy_hit_player(self, other);
            else enemy_whipped(self, other);
        } else if (!pcontent_ev(FEV_COLLISION, self, other))                               /* P7 hook */
            PUNTR(5005);
        return 1;
    }
    return 0;
}

int pen_draw(int i)
{
    /* oEnemy's Draw only draws (mirrored by facing); no side effects */
    return obj_is(PX(i).obj, OBJ_oEnemy) && PX(i).obj != OBJ_oShopkeeper;
}

int pen_outside(int i)
{
    int o = PX(i).obj;
    if (obj_is(o, OBJ_oEnemy) || o == OBJ_oWebBall) {                  /* oEnemy / oWebBall Other_0 */
        pin_destroy(i);
        return 1;
    }
    return 0;
}

int pen_destroy(int i)
{
    /* oEnemy's Destroy is commented out; oEnemySight, oWebBall, ... have none */
    return obj_is(PX(i).obj, OBJ_oEnemy) && PX(i).obj != OBJ_oShopkeeper;
}

/* ---- thrown items (oItem Step :233-326) ------------------------------------------------------------------ */
void pen_item_hit_enemy(int it)
{
    double x = X(it), y = Y(it);
    if (collision_rect_p(x - 2, y - 2, x + 2, y + 2, OBJ_oEnemy, 0, NOONE) != NOONE) {
        int obj = instance_nearest_p(x, y, OBJ_oEnemy);
        if (!PX(obj).invincible && PX(obj).obj != OBJ_oMagmaMan) {
            PE(&PX(obj))->xVel = PE(&PX(it))->xVel;
            pen_hit_common(obj, 0);
            PE(&PX(obj))->xVel = NMUL(PE(&PX(it))->xVel, N(0.3));
            if (PX(it).type == T_ARROW && PX(it).spr == GSPR_sBombArrowRight) PUNTR(5040);
            if (PX(it).type == T_ARROW || PX(it).type == T_FISHBONE) pin_destroy(it);
        }
    }
}

/* oJar / oSkull Step :104-145: 1 if the jar hit an enemy (it breaks) */
int pen_jar_hit(int jar, int skull)
{
    double x = X(jar), y = Y(jar);
    struct pin *j = &PX(jar);
    (void)skull;
    if (collision_rect_p(x - 3, y - 3, x + 3, y + 3, OBJ_oEnemy, 0, NOONE) != NOONE &&
        (NGT(NABS(PE(j)->xVel), N(2)) || NGT(NABS(PE(j)->yVel), N(2)))) {
        int e = instance_nearest_p(x, y, OBJ_oEnemy);
        if (!PX(e).invincible) {
            struct pin *o = &PX(e);
            PE(o)->xVel = PE(j)->xVel;
            if (o->type == T_CAVEMAN || o->type == T_SHOPKEEPER || o->obj == OBJ_oManTrap || o->obj == OBJ_oYeti ||
                o->obj == OBJ_oHawkman) {
                if (PE(o)->status != 98) {
                    if (o->obj != OBJ_oManTrap) pin_create(o->x, o->y, OBJ_oBlood);
                    o = &PX(e);
                    PE(o)->status = E_STUNNED;
                    PE(o)->counter = PEN(o)->stunTime;
                    PE(o)->yVel = N(-6);
                    snd_play(SND_xhit);                                        /* :126 */
                }
            } else {
                pin_create(o->x + PI(8), o->y + PI(8), OBJ_oBlood);
                o = &PX(e);
                PE(o)->hp -= 1;
                snd_play(SND_xhit);                                            /* :136 */
            }
            PE(&PX(e))->xVel = NMUL(PE(&PX(jar))->xVel, N(0.3));
        }
        return 1;
    }
    return 0;
}

/* oPlayer1 Step :1306: pick up a stunned enemy */
void pen_player_pickup_enemy(int i)
{
    int obj = instance_nearest_p(X(i), Y(i), OBJ_oEnemy);
    if (PE(&PX(obj))->status >= 98 && PE(&PX(obj))->canPickUp) {
        PL.holdItem = obj;
        PE(&PX(obj))->held = 1;
        PL.whoaTimer = PL.whoaTimerMax;
        PL.pickupItemType = PX(obj).type;
    }
}

/* ---- scripts/gameStepEvent :37-206: the oMovingSolid instances (the boulder) move by xVel / yVel and push
   or carry the player. oCharacter's viscidMovementOk: one player, kept here ------------------------------ */
static int viscidOk;

static int isCollisionRectangle(double a0, double a1, double a2, double a3, double a4, double a5, double a6,
                                double a7)
{
    double w1 = a2 - a0, h1 = a3 - a1, w2 = a6 - a4, h2 = a7 - a5;
    if (DLE(w2, 0) || DLE(h2, 0) || DLE(w1, 0) || DLE(h1, 0)) return 0;
    w2 += a4;
    h2 += a5;
    w1 += a0;
    h1 += a1;
    return (DLT(w2, a4) || DGT(w2, a0)) && (DLT(h2, a5) || DGT(h2, a1)) && (DLT(w1, a0) || DGT(w1, a4)) &&
           (DLT(h1, a1) || DGT(h1, a5));
}

/* isCollisionCharacterTop / Right / Left / Bottom (s): collision_line from the character's bounds against the
   solid s (an instance id): the object's query as collision_line_p makes it (UpdateTree; the creation-order scan
   touches s), then s's own line test (several instances of the object: smash traps, dark-fall blocks) */
static int line_solid(int s, double x1, double y1, double x2, double y2)
{
    int q = pcol_query(PX(s).obj);
    if (q < 0) return 0;
    if (q == 2) pcol_touch(s);
    return pw_test_line(s, x1, y1, x2, y2, 1);
}
static int cct(int s, int d)
{
    double lb, tb, rb, bb;
    calcBounds(pl(), &lb, &tb, &rb, &bb);
    return line_solid(s, dround(lb), dround(bb + d - 1), dround(rb - 1), dround(bb + d - 1));
}
static int ccr(int s, int d)
{
    double lb, tb, rb, bb;
    calcBounds(pl(), &lb, &tb, &rb, &bb);
    return line_solid(s, dround(lb - d), dround(tb), dround(lb - d), dround(bb - 1));
}
static int ccl(int s, int d)
{
    double lb, tb, rb, bb;
    calcBounds(pl(), &lb, &tb, &rb, &bb);
    return line_solid(s, dround(rb + d - 1), dround(tb), dround(rb + d - 1), dround(bb - 1));
}
static int ccb(int s, int d)
{
    double lb, tb, rb, bb;
    calcBounds(pl(), &lb, &tb, &rb, &bb);
    return line_solid(s, dround(lb), dround(tb - d), dround(rb - 1), dround(tb - d));
}

/* the moving solid's viscidTop (the boulder's 1); another object's from its package (pcontent_msolid), -1 when
   none translates it */
static int viscidTop_of(int s) { return PX(s).obj == OBJ_oBoulder ? 1 : pcontent_msolid(s); }

void pen_moving_solids(void)
{
    int16_t w[64];
    int n, k, c = pl();
    viscidOk = 1;                                                      /* with oCharacter */
    n = pw_with(OBJ_oMovingSolid, w, 64);
    for (k = 0; k < n; k++) {
        int s = w[k];
        struct pin *p = &PX(s);
        pos mstXPrev, mstYPrev;
        num xVelFrac, yVelFrac;
        int32_t xi = 0, yi = 0;
        if (!p->alive) continue;
        if (viscidTop_of(s) < 0) PUNTR(5050);                                 /* P7 hook (pcontent_msolid) */
        PE(p)->xVel += PE(p)->xAcc;
        PE(p)->yVel += PE(p)->yAcc;
        if (approximatelyZero(PE(p)->xVel)) PE(p)->xVel = 0;
        if (approximatelyZero(PE(p)->yVel)) PE(p)->yVel = 0;
        if (approximatelyZero(PE(p)->xAcc)) PE(p)->xAcc = 0;
        if (approximatelyZero(PE(p)->yAcc)) PE(p)->yAcc = 0;
        mstXPrev = p->x;
        mstYPrev = p->y;
        xVelFrac = NFRAC(NABS(PE(p)->xVel));
        yVelFrac = NFRAC(NABS(PE(p)->yVel));
        if (NNE(xVelFrac, N(0))) {
            int32_t r = NRECIP_ROUND(xVelFrac);
            if (r != 0) xi = (int32_t)(play_time % (uint32_t)r) == 0;
        }
        if (NNE(yVelFrac, N(0))) {
            int32_t r = NRECIP_ROUND(yVelFrac);
            if (r != 0) yi = (int32_t)(play_time % (uint32_t)r) == 0;
        }
        xi += NFLOOR(NABS(PE(p)->xVel));
        yi += NFLOOR(NABS(PE(p)->yVel));
        if (PE(p)->xVel < 0) xi = -xi;
        if (PE(p)->yVel < 0) yi = -yi;
        {
            double lb, tb, rb, bb, x = X(s), y = Y(s);
            int sp = p->spr, xo = gsprcol[sp].xo, yo = gsprcol[sp].yo, sw = sprw(s), sh = sprh(s);
            int axi = xi < 0 ? -xi : xi, ayi = yi < 0 ? -yi : yi, near = 0;
            if (c != NOONE) {
                calcBounds(c, &lb, &tb, &rb, &bb);
                near = isCollisionRectangle(x - axi - xo - 2, y - ayi - yo - 2, x + sw + axi - xo + 2,
                                            y + sh + ayi - yo + 2, lb, tb, rb, bb);
            }
            if (near) {
                struct pin *q = &PX(c);
                int brk;
                if (xi > 0) {
                    brk = 0;
                    for (; p->x < mstXPrev + PI(xi); pin_setx(p, p->x + (PI(1)))) {
                        if (viscidTop_of(s) > 0 && cct(s, 1) && (viscidOk == 1 || viscidOk == 2)) {
                            if (!isCollisionRight(c, 1)) { pin_setx(q, q->x + (PI(1))); viscidOk = 2; }
                        } else if (ccr(s, 1)) {
                            if (isCollisionRight(c, 1)) { brk = 1; break; }
                            pin_setx(q, q->x + (PI(1)));
                        }
                        if (brk) break;
                    }
                }
                if (xi < 0) {
                    brk = 0;
                    for (; p->x > mstXPrev + PI(xi); pin_setx(p, p->x - (PI(1)))) {
                        if (viscidTop_of(s) > 0 && cct(s, 1) && (viscidOk == 1 || viscidOk == 2)) {
                            if (!isCollisionLeft(c, 1)) { pin_setx(q, q->x - (PI(1))); viscidOk = 2; }
                        } else if (ccl(s, 1)) {
                            if (isCollisionLeft(c, 1)) { brk = 1; break; }
                            pin_setx(q, q->x - (PI(1)));
                        }
                        if (brk) break;
                    }
                }
                if (yi > 0) {
                    for (; p->y < mstYPrev + PI(yi); pin_sety(p, p->y + (PI(1)))) {
                        if (viscidTop_of(s) > 0 && cct(s, 2)) {
                            pin_sety(p, p->y + (PI(5)));
                            if (!isCollisionBottom(c, 1)) pin_sety(q, q->y + (PI(1)));
                            pin_sety(p, p->y - (PI(5)));
                        } else if (ccb(s, 1)) {
                            if (isCollisionBottom(c, 1)) break;
                            pin_sety(q, q->y + (PI(1)));
                        }
                    }
                }
                if (yi < 0) {
                    for (; p->y > mstYPrev + PI(yi); pin_sety(p, p->y - (PI(1)))) {
                        if (cct(s, 1)) {
                            if (isCollisionTop(c, 1)) break;
                            pin_sety(q, q->y - (PI(1)));
                        }
                        if (ccb(s, 1)) {
                            if (PL.jumpTime < PL.jumpTimeTotal) {
                                PE(q)->yVel = N(-2);
                                PL.jumpTime = PL.jumpTimeTotal;
                            }
                        }
                    }
                }
                if (viscidOk == 2) viscidOk = 0;
            } else {
                pin_setx(p, p->x + (PI(xi)));
                pin_sety(p, p->y + (PI(yi)));
            }
        }
    }
}
