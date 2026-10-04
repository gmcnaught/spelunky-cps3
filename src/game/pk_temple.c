/* P7 package D (temple and Olmec): docs/CONTENT.md §2. Translated statement for statement from
 * refs/hd/src/objects/<obj>/<event>.gml (line numbers in comments), as penemy.c / pobj.c. Defines the pcontent.h
 * functions it translates (ptemple_ev / ptemple_player / ptemple_world), overriding the weak defaults of pcontent.c.
 *
 * D1 temple: oHawkman, oTombLord (oFly, oSceptre, oGoldDoor), oSmashTrap / oSmashTrapLit, oCeilingTrap, oDoor,
 *   oTemple / oTempleFake, lava (oLava, oLavaDrip, oMagma, oMagmaTrail, oMagmaMan; the player, items, ropes and
 *   enemies in lava).
 * D2 Olmec: oOlmec, oOlmecDebris, oOlmecSlam, oYellowBall, oYellowTrail, oPsychicCreate2, oFinalBoss,
 *   oCavemanWorship, oLavaSolid, oXEnd.
 *
 * GML variables without a struct pin_ext / pin_en field of their own name use one of the instance's unused fields
 * (the object never uses that field otherwise; playhost does not print it for these objects): see the per-object
 * #defines below.
 */
#include "pint.h"
#include "penemy.h"
#include "pcontent.h"
#include "inst.h"                                    /* play_gen_inst (struct inst, IF_*) */
#include "../snd/sndgame.h"
#include "pmath.h"

enum { E_IDLE = 0, E_WALK = 1, E_STUNNED = 98, E_DEAD = 99, E_LEFT = 0, E_RIGHT = 1 };

static double X(int i) { return PTOD(PX(i).x); }
static double Y(int i) { return PTOD(PX(i).y); }
static int CP(double x, double y, int obj) { return collision_point_p(x, y, obj, 0, NOONE) != NOONE; }
static int CPn(double x, double y, int obj, int self) { return collision_point_p(x, y, obj, 1, self) != NOONE; }
static int sprw(int i) { int s = PX(i).spr; return s >= 0 ? (int)(psprite[s].w * PX(i).xscale) : 0; }
static int sprh(int i) { int s = PX(i).spr; return s >= 0 ? (int)(psprite[s].h * PX(i).yscale) : 0; }
static int pl(void) { return PL.idx; }
static double dabs(double a) { return a < 0 ? -a : a; }

/* x > xview - l and x < xview + 320 + r (same for y with 240) */
static int vw(int i, int l, int r)
{
    double x = X(i), y = Y(i);
    view_read();
    return DGT(x, PW.xview - l) && DLT(x, PW.xview + 320 + r) && DGT(y, PW.yview - l) && DLT(y, PW.yview + 240 + r);
}

/* the event marks for sites that call one hook twice in one event (see lava_item / lava_enemy): the step and the
   instance of the last call */
struct mark { uint32_t step; int16_t i; int16_t room; };
static int marked(struct mark *m, int i)
{
    return m->i == i && m->step == PW.step && m->room == PW.room;
}
static void mark_set(struct mark *m, int i)
{
    m->i = (int16_t)i;
    m->step = PW.step;
    m->room = PW.room;
}

/* ==== oHawkman ================================================================================================== */
/* objects/oHawkman/Create_0.gml (after oEnemy's: penemy.c pen_create ran pen_enemy_create) */
static void hawkman_create(int i)
{
    struct pin *p = &PX(i);
    PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;           /* makeActive */
    setCollisionBounds(i, 2, 0, sprw(i) - 2, sprh(i));
    PE(p)->xVel = N(2.5);
    p->ispd = (img_t)0.5;
    PE(p)->myGrav = N(0.6);
    p->type = T_CAVEMAN;                       /* "Yeti": the type tests treat it as the caveman's (see penemy.c) */
    PE(p)->hp = 4;
    p->invincible = 0;
    PE(p)->status = E_IDLE;
    PEN(p)->whipped = 0;
    PEN(p)->burning = 0;
    PEN(p)->bounced = 0;
    PEN(p)->edead = 0;
    PE(p)->counter = 0;
    PEN(p)->sightCounter = 0;
    PE(p)->facing = E_RIGHT;
    PE(p)->colLeft = PE(p)->colRight = PE(p)->colBot = PE(p)->colTop = 0;
}

static void hawkman_sight(int i)
{
    struct pin *p = &PX(i);
    if (PEN(p)->sightCounter > 0) PEN(p)->sightCounter -= 1;
    else {
        int s = pin_create(p->x, p->y, OBJ_oEnemySight);
        double dir = PE(&PX(i))->facing == E_LEFT ? 180 : 0;
        PE(&PX(s))->direction = dir;
        PEN(&PX(s))->hspeed = 10 * pcos_cr(degtorad_d(dir));                  /* speed = 10 */
        PEN(&PX(s))->vspeed = -10 * psin_cr(degtorad_d(dir));
        PEN(&PX(s))->owner = (int16_t)instance_place_p(i, X(i), Y(i), OBJ_oHawkman);
        PEN(&PX(i))->sightCounter = 5;
    }
}

/* objects/oHawkman/Step_0.gml */
static void hawkman_step(int i)
{
    struct pin *p;
    pen_parent_step(i);
    p = &PX(i);
    if (!vw(i, 20, 4)) return;
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    if (!PE(p)->held) PE(p)->yVel += PE(p)->myGrav;
    if (NGT(PE(p)->yVel, PEN(p)->yVelLimit)) PE(p)->yVel = PEN(p)->yVelLimit;
    PE(p)->colLeft = PE(p)->colRight = PE(p)->colBot = PE(p)->colTop = 0;
    if (isCollisionLeft(i, 1)) PE(p)->colLeft = 1;
    if (isCollisionRight(i, 1)) PE(p)->colRight = 1;
    if (isCollisionBottom(i, 1)) PE(p)->colBot = 1;
    if (isCollisionTop(i, 1)) PE(p)->colTop = 1;
    if (PE(p)->status >= E_STUNNED) {                                      /* :19 */
        if (CP(X(i) + 8, Y(i) + 12, OBJ_oSolid)) {
            scrCreateBlood(i, p->x + PI(8), p->y + PI(8), 3);
            snd_play(SND_xcavemandie);                                         /* :24 */
            pin_destroy(i);
        }
    } else if (!PE(p)->held) {
        if (CP(X(i) + 8, Y(i) + 8, OBJ_oSolid)) {
            scrCreateBlood(i, p->x + PI(8), p->y + PI(8), 3);
            snd_play(SND_xcavemandie);                                         /* :33 */
            pin_destroy(i);
        }
    }
    p = &PX(i);
    if (PE(p)->status != E_DEAD && PE(p)->status != E_STUNNED && PE(p)->hp < 1) PE(p)->status = E_DEAD;
    if (isCollisionBottom(i, 1) && PE(p)->status != E_STUNNED) PE(p)->yVel = 0;   /* :43 */
    if (PE(p)->status == E_IDLE) {                                         /* :45 */
        if (isCollisionBottom(i, 1) && (CPn(X(i) - 1, Y(i), OBJ_oSolid, i) || CPn(X(i) + 16, Y(i), OBJ_oSolid, i))) {
            PE(p)->yVel = N(-6);
            PE(p)->xVel = PE(p)->facing == E_LEFT ? N(-1) : N(1);
            PE(p)->counter -= 10;
        }
        if (NLT(PE(p)->yVel, N(0)) && isCollisionTop(i, 1)) PE(p)->yVel = 0;
        if (isCollisionBottom(i, 1) && PE(p)->counter > 0) PE(p)->counter -= 1;
        if (PE(p)->counter < 1) {
            PE(p)->facing = (int16_t)RAND(0, 1);
            PE(p)->status = E_WALK;
        }
        hawkman_sight(i);
    } else if (PE(p)->status == E_WALK) {                                  /* :78 */
        if (isCollisionLeft(i, 1) || isCollisionRight(i, 1))
            PE(p)->facing = PE(p)->facing == E_LEFT ? E_RIGHT : E_LEFT;
        if (PE(p)->facing == E_LEFT) {
            PE(p)->xVel = N(-1.5);
            if (!CPn(X(i) - 1, Y(i) + 16, OBJ_oSolid, i)) {
                PE(p)->status = E_IDLE;
                PE(p)->counter = (int16_t)RAND(20, 50);
                PE(p)->xVel = 0;
            }
        } else {
            PE(p)->xVel = N(1.5);
            if (!CPn(X(i) + 16, Y(i) + 16, OBJ_oSolid, i)) {
                PE(p)->status = E_IDLE;
                PE(p)->counter = (int16_t)RAND(20, 50);
                PE(p)->xVel = 0;
            }
        }
        if (RAND(1, 100) == 1) {
            PE(p)->status = E_IDLE;
            PE(p)->counter = (int16_t)RAND(20, 50);
            PE(p)->xVel = 0;
        }
        hawkman_sight(i);
    } else if (PE(p)->status == 2) {                                       /* ATTACK :125 */
        p->ispd = 1;
        if (isCollisionLeft(i, 1) || isCollisionRight(i, 1))
            PE(p)->facing = PE(p)->facing == E_LEFT ? E_RIGHT : E_LEFT;
        PE(p)->xVel = PE(p)->facing == E_LEFT ? N(-3) : N(3);
        if (isCollisionBottom(i, 1) && !CP(X(i), Y(i) - 16, OBJ_oSolid)) PE(p)->yVel = N(-6);
    } else if (PE(p)->status == E_STUNNED) {                               /* :141 */
        if (NEQ(PE(p)->xVel, N(0)) && PE(p)->hp > 0) pin_set_sprite(i, GSPR_sHawkStunL);
        else if (PEN(p)->bounced) pin_set_sprite(i, NLT(PE(p)->yVel, N(0)) ? GSPR_sHawkBounceL : GSPR_sHawkFallL);
        else pin_set_sprite(i, NGT(NABS(PE(p)->xVel), N(0)) ? GSPR_sHawkDieLL : GSPR_sHawkDieLR);
        if (PE(p)->colBot && !PEN(p)->bounced) {
            PEN(p)->bounced = 1;
            scrCreateBlood(i, p->x + PI(8), p->y + PI(8), 1);
            p = &PX(i);
        }
        if (PE(p)->held || PE(p)->colBot) {
            if (PE(p)->counter > 0) PE(p)->counter -= 1;
            else if (PE(p)->hp > 0) {
                PE(p)->status = E_IDLE;
                if (PE(p)->held) {
                    PE(p)->held = 0;
                    PL.holdItem = NOONE;
                    PL.pickupItemType = T_NONE;
                }
            }
        }
    } else if (PE(p)->status == E_DEAD) {                                  /* :175 */
        if (!PEN(p)->edead) {
            if (PEN(p)->countsAsKill) PG.kills += 1;   /* global.hawkmen: no PG field yet (transition-room tally) */
            snd_play(SND_xcavemandie);                                         /* :185 */
            PEN(p)->edead = 1;
        }
        pin_set_sprite(i, GSPR_sHawkDeadL);
        if (NGT(NABS(PE(p)->xVel), N(0)) || NGT(NABS(PE(p)->yVel), N(0))) PE(p)->status = E_STUNNED;
    }
    if (PE(p)->status >= E_STUNNED) {                                      /* :194 */
        scrCheckCollisions(i);
        if (NEQ(PE(p)->xVel, N(0)) && NEQ(PE(p)->yVel, N(0)) && PE(p)->hp < 1) PE(p)->status = E_DEAD;
    }
    if (NGT(PE(p)->xVel, N(0))) PE(p)->xVel -= N(0.1);
    if (NLT(PE(p)->xVel, N(0))) PE(p)->xVel += N(0.1);
    if (NLT(NABS(PE(p)->xVel), N(0.5))) PE(p)->xVel = 0;
    if (PE(p)->status < E_STUNNED && PE(p)->status != 3)
        pin_set_sprite(i, NGT(NABS(PE(p)->xVel), N(0)) ? GSPR_sHawkRunLeft : GSPR_sHawkLeft);
    if (PE(p)->held) pin_set_sprite(i, PE(p)->hp > 0 ? GSPR_sHawkHeldL : GSPR_sHawkDHeldL);
}

/* objects/oHawkman/Collision_oCharacter.gml */
static void hawkman_hit_player(int i, int c)
{
    struct pin *p = &PX(i), *o = &PX(c);
    double dx;
    if (PE(p)->status == E_DEAD || PE(p)->status == E_STUNNED) return;
    dx = PTOD(o->x) - (X(i) + 8);
    if (DGT(dabs(dx), 8)) {
    } else if (!PL.dead && !PL.stunned && (PL.state == JUMPING || PL.state == FALLING) && DLT(PTOD(o->y), Y(i) + 5) &&
               !PL.swimming) {
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
        snd_play(SND_xhit);                                                    /* :21 */
    } else if (PL.invincible == 0 && PE(p)->status < E_STUNNED) {
        if (CP(X(i) + 8, Y(i) - 4, OBJ_oSolid)) {                          /* :25 */
            PL.blink = 30;
            PL.invincible = 30;
            PE(o)->xVel = DLT(PTOD(o->x), X(i)) ? N(-6) : N(6);
            pin_create(o->x, o->y, OBJ_oBlood);
            if (PG.plife > 0) PG.plife -= 1;
            snd_play(SND_xhurt);                                               /* :40 */
        } else if (PE(p)->status != 3) {                                   /* THROW :42 */
            PE(p)->status = 3;
            PE(p)->xVel = 0;
            if (DGT(PTOD(o->x), X(i) + 8)) {
                PE(p)->facing = E_RIGHT;
                pin_set_sprite(i, GSPR_sHawkThrowL);
                pin_setxy(o, p->x, p->y);
                PE(o)->yVel = N(-6);
                PE(o)->xVel = N(6);
            } else {
                PE(p)->facing = E_LEFT;
                pin_set_sprite(i, GSPR_sHawkThrowL);
                pin_setxy(o, p->x + PI(16), p->y);
                PE(o)->yVel = N(-6);
                PE(o)->xVel = N(-6);
            }
            PL.stunned = 1;
            PL.bounced = 0;
            PL.wallHurt = 1;
            if (PL.holdItem != NOONE) {                                    /* with other :69 */
                if (PX(PL.holdItem).type == T_GOLDIDOL) pin_sety(&PX(PL.holdItem), PX(PL.holdItem).y - PI(8));
                scrDropItem(PE(o)->xVel, PE(o)->yVel);
            }
        }
    }
}

/* objects/oHawkman/Collision_oWhip.gml (and oWhipPre, the same; other.type "Machete": the machete, package E) */
static void hawkman_whipped(int i, int w)
{
    struct pin *p = &PX(i);
    if (PE(p)->status < E_STUNNED) {
        PE(p)->hp -= 1;                                                    /* other.damage: 1 */
        if (PEN(p)->bloodLeft > 0) {
            scrCreateBlood(i, (pos)(X(i) + sprw(i) / 2.0), (pos)(Y(i) + sprh(i) / 2.0), 1);
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

/* ==== lava ======================================================================================================= */
/* oLava's spurt, spurtTime, spurtCounter */
#define spurt armed
#define spurtTime fired
#define spurtCounter counter

/* objects/oLava/Create_0.gml (fromgen: the generator drew spurtTime and set spurt) */
static void lava_create(int i, int fromgen)
{
    struct pin *p = &PX(i);
    p->type = T_OTHER;                                                     /* "Lava" */
    if (fromgen) {
        PE(p)->spurt = (play_gen_inst->flags & IF_SPURT) != 0;
        PE(p)->spurtTime = play_gen_inst->spurttime;
    } else {
        PE(p)->spurt = 0;
        PE(p)->spurtTime = (int16_t)RAND(100, 300);
    }
    PE(p)->spurtCounter = PE(p)->spurtTime;
    p->ispd = (img_t)0.4;
}

/* objects/oLava/Step_0.gml */
static void lava_step(int i)
{
    struct pin *p = &PX(i);
    double dist = 0;
    if (isLevel()) dist = point_distance_d(X(i), Y(i), X(pl()), Y(pl()));
    if (PE(p)->spurt && DLT(dist, 240)) {
        if (PE(p)->spurtCounter > 0) PE(p)->spurtCounter -= 1;
        else {
            int f;
            PE(p)->spurtCounter = PE(p)->spurtTime;
            if (RAND(1, 8) == 1) f = pin_create(p->x + PI(8), p->y - PI(4), OBJ_oMagma);
            else f = pin_create(p->x + PI(8), p->y - PI(4), OBJ_oFlame);
            PE(&PX(f))->yVel = NI(-RAND(1, 4));
        }
    }
}

/* objects/oLava/Destroy_0.gml */
static void lava_destroy(int i)
{
    int k;
    for (k = 0; k < 3; k++) {
        int yy = RAND(0, 16), xx = RAND(0, 16);                            /* arguments: last first */
        pin_create(PX(i).x + PI(xx), PX(i).y + PI(yy), OBJ_oLavaDrip);
    }
    if (RAND(1, 6) == 1) {
        int f = pin_create(PX(i).x + PI(8), PX(i).y + PI(8), OBJ_oFlame);
        PE(&PX(f))->yVel = N(4);
    }
}

/* oEnemy Step :63-74 (penemy.c pen_parent_step calls site 5012 for :63 and for :65): the first call of a step is
   :63 when lava is above, else :65 */
static struct mark mk_enemy;
static void lava_enemy(int i)
{
    struct pin *p = &PX(i);
    if (!marked(&mk_enemy, i) && CP(X(i) + dfloor(sprw(i) / 2.0), Y(i) - 1, OBJ_oLava)) {
        mark_set(&mk_enemy, i);
        pin_destroy(i);                                                    /* :63 */
        return;
    }
    PE(p)->hp = 0;                                                         /* :65 */
    PEN(p)->countsAsKill = 0;
    PEN(p)->burning = 1;
    PE(p)->myGrav = 0;
    PE(p)->xVel = 0;
    PE(p)->yVel = N(0.1);
    pin_setdepth(p, 999);
}

/* ==== oMagma, oMagmaTrail, oLavaDrip ============================================================================= */
/* objects/oMagma/Create_0.gml (oDetritus Create first; GML goes on after its instance_destroy) */
static void magma_create(int i)
{
    struct pin *p;
    create_detritus(i);
    p = &PX(i);
    p->ispd = (img_t)0.3;
    PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;           /* makeActive */
    setCollisionBounds(i, -8, -8, 8, 8);
    {
        double a = prandom(4);
        double b = prandom(4);
        PE(p)->xVel = ND(a - b);
    }
    PE(p)->yVel = ND(-1 - prandom(2));
    PE(p)->grav = NMUL(NI(RAND(1, 6)), N(0.1));
    PE(p)->hp = 200;
    PE(p)->alarm[0] = 2;
    PE(p)->alarm[1] = 50;
}

/* objects/oMagma/Step_0.gml */
static void magma_step(int i)
{
    detritus_step(i);
    if (isCollisionBottom(i, 1)) {
        struct pin *p = &PX(i);
        pin_set_sprite(i, GSPR_sMagmaManCreate);
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        PE(p)->dying = 1;
    }
}

/* objects/oMagma/Collision_oCharacter.gml, oMagmaMan/Collision_oCharacter.gml */
static void magma_hit_player(int i, int c)
{
    struct pin *o = &PX(c);
    if (PL.invincible != 0) return;
    PL.blink = 30;
    PL.invincible = 30;
    PL.stunned = 1;
    PL.stunTimer = 20;
    PL.burning = 100;
    PE(o)->yVel = N(-4);
    PE(o)->xVel = DLT(PTOD(o->x), X(i)) ? N(-6) : N(6);
    pin_create(o->x, o->y, OBJ_oBlood);
    if (PG.plife > 0) PG.plife -= 2;
    snd_play(SND_xhurt);                                                       /* oMagma :16, oMagmaMan :20 */
    snd_play(SND_xflame);                                                      /* oMagma :17, oMagmaMan :21 */
}

/* objects/oMagma/Collision_oEnemy.gml, oMagmaMan/Collision_oEnemy.gml (man: tomb lord / yeti king not stunned) */
static void magma_hit_enemy(int i, int e, int man)
{
    struct pin *o = &PX(e);
    if (o->obj == OBJ_oMagmaMan) return;                               /* type "Magma Man" */
    PE(o)->yVel = N(-4);
    PE(o)->xVel = DLT(X(i), PTOD(o->x)) ? N(-3) : N(3);
    if (PE(o)->status != 98) snd_play(SND_xflame);                             /* :8 */
    PEN(o)->burning = 100;
    PE(o)->hp -= 2;
    if (!man || (o->obj != OBJ_oTombLord && o->obj != OBJ_oYetiKing)) {
        PE(o)->status = 98;
        PE(o)->counter = 50;
    }
}

/* objects/oRubblePiece/Step_0.gml (oLavaDrip; type "NONE") */
static void rubblepiece_step(int i)
{
    struct pin *p = &PX(i);
    double x, y;
    pin_setx(p, PADDV(p->x, PE(p)->xVel));
    pin_sety(p, PADDV(p->y, PE(p)->yVel));
    PE(p)->yVel += PE(p)->yAcc;
    x = PTOD(p->x);
    y = PTOD(p->y);
    if (collision_point_p(x, y, OBJ_oWaterSwim, 0, NOONE) != NOONE) pswamp_world(1041, i, 0);
    else if (collision_point_p(x, y, OBJ_oLava, 0, NOONE) != NOONE) pin_destroy(i);
    if (collision_point_p(x, y, OBJ_oSolid, 0, NOONE) != NOONE) pin_destroy(i);
    view_read();
    if (DLT(x, PW.xview - 32) || DGT(x, PW.xview + 320 + 32) || DLT(y, PW.yview - 32) || DGT(y, PW.yview + 240 + 32))
        pin_destroy(i);
}

/* ==== oMagmaMan ================================================================================================== */
/* objects/oMagmaMan/Create_0.gml (oEnemy Create first) */
static void magmaman_create(int i)
{
    struct pin *p;
    pen_enemy_create(i);
    p = &PX(i);
    PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;
    setCollisionBounds(i, 2, 0, sprw(i) - 2, sprh(i));
    PE(p)->xVel = N(2.5);
    p->ispd = (img_t)0.5;
    p->type = T_OTHER;                                                     /* "Magma Man" */
    PE(p)->hp = 200;
    p->invincible = 0;
    PE(p)->status = E_IDLE;
    PEN(p)->whipped = 0;
    PEN(p)->bounced = 0;
    PEN(p)->edead = 0;
    PE(p)->counter = 0;
    PEN(p)->sightCounter = 0;
    PE(p)->facing = E_RIGHT;
}

static int isCollisionSolid(int i)
{
    double lb, tb, rb, bb;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_rect_p(lb, tb, rb - 1, bb - 1, OBJ_oSolid, 1, i) != NOONE;
}

/* objects/oMagmaMan/Step_0.gml (no inherited Step) */
static void magmaman_step(int i)
{
    struct pin *p = &PX(i);
    if (PE(p)->hp > 0) PE(p)->hp -= 1;
    if (CP(X(i) + 8, Y(i) + 8, OBJ_oSolid) || PE(p)->hp < 1) {
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        PE(p)->status = E_DEAD;
        pin_set_sprite(i, GSPR_sMagmaManDie);
    }
    PE(p)->yVel += PE(p)->myGrav;
    if (NGT(PE(p)->yVel, PEN(p)->yVelLimit)) PE(p)->yVel = PEN(p)->yVelLimit;
    if (isCollisionBottom(i, 1)) PE(p)->yVel = 0;
    else {
        pin_create(p->x + PI(8), p->y + PI(8), OBJ_oMagma);
        pin_destroy(i);
    }
    p = &PX(i);
    if (RAND(1, 20) == 1) {                                                /* :27 */
        int yy = RAND(4, 12), xx = RAND(4, 12);
        pin_create(p->x + PI(xx), p->y + PI(yy), OBJ_oBurn);
        p = &PX(i);
    }
    PEN(p)->burning -= 1;
    if (PE(p)->status == E_IDLE) {
        if (NLT(PE(p)->yVel, N(0)) && isCollisionTop(i, 1)) PE(p)->yVel = 0;
        if (isCollisionBottom(i, 1) && PE(p)->counter > 0) PE(p)->counter -= 1;
        if (PE(p)->counter < 1) {
            PE(p)->facing = (int16_t)RAND(0, 1);
            PE(p)->status = E_WALK;
            if (RAND(1, 6) == 1) {
                int m = pin_create(PX(i).x + PI(8), PX(i).y + PI(8), OBJ_oMagma);
                PE(&PX(m))->hp = PE(&PX(i))->hp;
                pin_destroy(i);
            }
        }
    } else if (PE(p)->status == E_WALK) {
        if (isCollisionLeft(i, 1) || isCollisionRight(i, 1))
            PE(p)->facing = PE(p)->facing == E_LEFT ? E_RIGHT : E_LEFT;
        if (PE(p)->facing == E_LEFT) {
            PE(p)->xVel = N(-1.5);
            if (!CPn(X(i) - 1, Y(i) + 16, OBJ_oSolid, i)) {
                PE(p)->status = E_IDLE;
                PE(p)->counter = (int16_t)RAND(20, 50);
                PE(p)->xVel = 0;
            }
        } else {
            PE(p)->xVel = N(1.5);
            if (!CPn(X(i) + 16, Y(i) + 16, OBJ_oSolid, i)) {
                PE(p)->status = E_IDLE;
                PE(p)->counter = (int16_t)RAND(20, 50);
                PE(p)->xVel = 0;
            }
        }
        if (RAND(1, 100) == 1) {
            PE(p)->status = E_IDLE;
            PE(p)->counter = (int16_t)RAND(20, 50);
            PE(p)->xVel = 0;
        }
    }
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    if (isCollisionSolid(i)) pin_sety(p, p->y - PI(2));
    if (PE(p)->status < E_STUNNED && PE(p)->status != 3)
        pin_set_sprite(i, NGT(NABS(PE(p)->xVel), N(0)) ? GSPR_sMagmaManWalkL : GSPR_sMagmaManLeft);
}

/* objects/oMagmaMan/Collision_oBomb.gml */
static void magmaman_bomb(int i, int b)
{
    struct pin *o = &PX(b);
    if (o->spr != GSPR_sBombArmed) {
        pin_set_sprite(b, GSPR_sBombArmed);
        o->ispd = 1;
        PE(o)->alarm[1] = RAND(8, 12);
    }
    if (DLT(PTOD(o->x), X(i))) PE(o)->xVel = NI(-RAND(2, 4));
    else PE(o)->xVel = NI(RAND(2, 4));
    if (DLT(PTOD(o->y), Y(i))) PE(o)->yVel = NI(-RAND(2, 4));
    if (PE(o)->held && instance_exists_p(OBJ_oCharacter)) PL.holdItem = NOONE;
}

/* ==== dispatch ==================================================================================================== */
static int create_ev(int i, int fromgen)
{
    switch (PX(i).obj) {
    case OBJ_oHawkman: hawkman_create(i); return 1;
    case OBJ_oLava: lava_create(i, fromgen); return 1;
    case OBJ_oMagma: magma_create(i); return 1;
    case OBJ_oMagmaTrail: PX(i).ispd = (img_t)0.4; return 1;
    case OBJ_oLavaDrip:                                                /* oRubblePiece Create, then its own */
        PX(i).type = T_NONE;
        PE(&PX(i))->xVel = 0;
        PE(&PX(i))->yVel = 0;
        PE(&PX(i))->yAcc = N(0.6);
        PX(i).ispd = (img_t)0.4;
        return 1;
    case OBJ_oMagmaMan: magmaman_create(i); return 1;
    }
    return 0;
}

static int step_ev(int i)
{
    switch (PX(i).obj) {
    case OBJ_oHawkman: hawkman_step(i); return 1;
    case OBJ_oLava: lava_step(i); return 1;
    case OBJ_oMagma: magma_step(i); return 1;
    case OBJ_oLavaDrip: rubblepiece_step(i); return 1;
    case OBJ_oMagmaMan: magmaman_step(i); return 1;
    }
    return 0;
}

static int alarm_ev(int i, int a)
{
    switch (PX(i).obj) {
    case OBJ_oHawkman: if (a == 0) PEN(&PX(i))->whipped = 0; return 1;    /* objects/oHawkman/Alarm_0.gml */
    case OBJ_oMagmaMan: if (a == 0) PEN(&PX(i))->whipped = 0; return 1;   /* objects/oMagmaMan/Alarm_0.gml */
    case OBJ_oMagma:                                                   /* objects/oMagma/Alarm_0.gml (1: none) */
        if (a == 0) {
            pin_create(PX(i).x, PX(i).y, OBJ_oMagmaTrail);
            PE(&PX(i))->alarm[0] = 2;
        }
        return 1;
    }
    return 0;
}

static int animend_ev(int i)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oHawkman:                                                 /* objects/oHawkman/Other_7.gml */
        if (p->spr == GSPR_sHawkThrowL) {
            PE(p)->status = E_IDLE;
            pin_set_sprite(i, GSPR_sHawkLeft);
        }
        return 1;
    case OBJ_oMagma:                                                   /* objects/oMagma/Other_7.gml */
        if (PE(p)->dying) {
            int m = pin_create(p->x - PI(8), p->y - PI(8), OBJ_oMagmaMan);
            PE(&PX(m))->hp = PE(&PX(i))->hp;
            pin_destroy(i);
        }
        return 1;
    case OBJ_oMagmaTrail: case OBJ_oLavaDrip: pin_destroy(i); return 1;
    case OBJ_oMagmaMan:                                                /* objects/oMagmaMan/Other_7.gml */
        if (p->spr == GSPR_sMagmaManDie) pin_destroy(i);
        return 1;
    }
    return 0;
}

static int collision_ev(int self, int other)
{
    int oo = PX(other).obj;
    switch (PX(self).obj) {
    case OBJ_oHawkman:
        if (obj_is(oo, OBJ_oCharacter)) hawkman_hit_player(self, other);
        else hawkman_whipped(self, other);
        return 1;
    case OBJ_oMagma:
        if (obj_is(oo, OBJ_oCharacter)) magma_hit_player(self, other);
        else if (obj_is(oo, OBJ_oEnemy)) magma_hit_enemy(self, other, 0);
        else if (obj_is(oo, OBJ_oWater)) {                             /* objects/oMagma/Collision_oWater.gml */
            pin_create(PX(self).x, PX(self).y, OBJ_oSmokePuff);
            pin_destroy(self);
        } else return 0;
        return 1;
    case OBJ_oMagmaMan:
        if (obj_is(oo, OBJ_oCharacter)) magma_hit_player(self, other);
        else if (obj_is(oo, OBJ_oEnemy)) magma_hit_enemy(self, other, 1);
        else if (obj_is(oo, OBJ_oBomb)) magmaman_bomb(self, other);
        else if (!PEN(&PX(self))->whipped) {                           /* objects/oMagmaMan/Collision_oWhip.gml */
            snd_play(SND_xhit);                                                /* :3 */
            PEN(&PX(self))->whipped = 1;
            PE(&PX(self))->alarm[0] = 10;
        }
        return 1;
    case OBJ_oFlame:                                                   /* objects/oFlame/Collision_oWater.gml */
        if (!obj_is(oo, OBJ_oLava)) return 0;                          /* water: package B */
        pin_create(PX(self).x, PX(self).y, OBJ_oSmokePuff);
        pin_destroy(self);
        return 1;
    }
    return 0;
}

static int destroy_ev(int i)
{
    switch (PX(i).obj) {
    case OBJ_oLava: lava_destroy(i); return 1;
    }
    return 0;
}

int ptemple_ev(int ev, int i, int arg)
{
    switch (ev) {
    case FEV_CREATE: return create_ev(i, arg);
    case FEV_STEP: return step_ev(i);
    case FEV_ALARM: return alarm_ev(i, arg);
    case FEV_ANIMEND: return animend_ev(i);
    case FEV_COLLISION: return collision_ev(i, arg);
    case FEV_DESTROY: return destroy_ev(i);
    }
    return 0;
}

int ptemple_player(int site, int i, int arg)
{
    (void)arg;
    switch (site) {
    }
    PUNTR(site);
    (void)i;
    return 0;
}

int ptemple_world(int site, int i, int arg)
{
    (void)arg;
    switch (site) {
    case 5012: lava_enemy(i); return 0;
    case 5030: {                                    /* oEnemySight/Collision_oCharacter.gml :10 with oHawkman */
        int16_t w[256];
        int n = pw_with(OBJ_oHawkman, w, 256), k;
        for (k = 0; k < n; k++) {
            int c = w[k];
            if (!PX(c).alive) continue;
            if (DLT(distance_to_object_p(c, OBJ_oPlayer1), 100) && PE(&PX(c))->status < 98) {
                PE(&PX(c))->status = 2;
                snd_play(SND_xalert);                                          /* :15 */
            }
        }
        return 0;
    }
    }
    PUNTR(site);
    return 0;
}
