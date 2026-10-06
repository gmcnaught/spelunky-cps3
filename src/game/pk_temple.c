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
#include "pmsg.h"

enum { E_IDLE = 0, E_WALK = 1, E_STUNNED = 98, E_DEAD = 99, E_LEFT = 0, E_RIGHT = 1 };

static double X(int i) { return PTOD(PX(i).x); }
static double Y(int i) { return PTOD(PX(i).y); }
static int CP(double x, double y, int obj) { return collision_point_any(x, y, obj, 0, NOONE); }
/* collision_point(x, y, obj, -1, -1): the runner reads a bool argument as value > 0.5, so prec and notme are false */
static int CPm(double x, double y, int obj) { return collision_point_any(x, y, obj, 0, NOONE); }
static int sprw(int i) { int s = PX(i).spr; return s >= 0 ? spr_dim(psprite[s].w, PX(i).xscale) : 0; }
static int sprh(int i) { int s = PX(i).spr; return s >= 0 ? spr_dim(psprite[s].h, PX(i).yscale) : 0; }
static int pl(void) { return PL.idx; }
static double dabs(double a) { return a < 0 ? -a : a; }

/* x > xview - l and x < xview + 320 + r (same for y with 240) */
static int vw(int i, int l, int r)
{
    view_read();
    return PGTI(PX(i).x, PW.xview - l) && PLTI(PX(i).x, PW.xview + 320 + r) && PGTI(PX(i).y, PW.yview - l) &&
           PLTI(PX(i).y, PW.yview + 240 + r);
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
    pin_setispd(p, (img_t)0.5);
    PE(p)->myGrav = N(0.6);
    p->type = T_YETI;                                                      /* "Yeti" */
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
        if (isCollisionBottom(i, 1) && (CPm(X(i) - 1, Y(i), OBJ_oSolid) || CPm(X(i) + 16, Y(i), OBJ_oSolid))) {
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
            if (!CPm(X(i) - 1, Y(i) + 16, OBJ_oSolid)) {
                PE(p)->status = E_IDLE;
                PE(p)->counter = (int16_t)RAND(20, 50);
                PE(p)->xVel = 0;
            }
        } else {
            PE(p)->xVel = N(1.5);
            if (!CPm(X(i) + 16, Y(i) + 16, OBJ_oSolid)) {
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
        pin_setispd(p, 1);
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
            if (PEN(p)->countsAsKill) {
                PG.hawkmen += 1;
                PG.kills += 1;
            }
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
        pin_setispd(p, (img_t)0.5);
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
        pin_setispd(p, (img_t)0.5);
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
    pin_setispd(p, (img_t)0.4);
}

/* objects/oLava/Step_0.gml */
static void lava_step(int i)
{
    struct pin *p = &PX(i);
    double dist = 0;                             /* (squared: pdist2; 0 either way when not a level) */
    if (!PE(p)->spurt) return;                   /* (dist is read only with spurt: pdist2 has no side effect) */
    if (isLevel()) dist = pdist2(X(i), Y(i), X(pl()), Y(pl()));
    if (pdist2_lt(dist, 240)) {
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

/* oEnemy Step :63-74 (penemy.c pen_parent_step, site 5012): arg 1 lava above (:63), 2 lava below (:65) */
static void lava_enemy(int i, int arg)
{
    struct pin *p = &PX(i);
    if (arg == 1) {
        pin_destroy(i);
        return;
    }
    PE(p)->hp = 0;
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
    pin_setispd(p, (img_t)0.3);
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
    if (o->type == T_MAGMAMAN) return;
    PE(o)->yVel = N(-4);
    PE(o)->xVel = DLT(X(i), PTOD(o->x)) ? N(-3) : N(3);
    if (PE(o)->status != 98) snd_play(SND_xflame);                             /* :8 */
    PEN(o)->burning = 100;
    PE(o)->hp -= 2;
    if (!man || (o->type != T_TOMBLORD && o->type != T_YETIKING)) {
        PE(o)->status = 98;
        PE(o)->counter = 50;
    }
}

/* objects/oRubblePiece/Step_0.gml (oLavaDrip; type "NONE") */
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
    x = PTOD(p->x);
    y = PTOD(p->y);
    if (collision_point_any(x, y, OBJ_oWaterSwim, 0, NOONE)) pswamp_world(1041, i, 0);
    else if (collision_point_any(x, y, OBJ_oLava, 0, NOONE)) pin_destroy(i);
    if (collision_point_any(x, y, OBJ_oSolid, 0, NOONE)) pin_destroy(i);
    view_read();
    if (PLTI(px, PW.xview - 32) || PGTI(px, PW.xview + 320 + 32) || PLTI(py, PW.yview - 32) || PGTI(py, PW.yview + 240 + 32))
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
    pin_setispd(p, (img_t)0.5);
    p->type = T_MAGMAMAN;
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
    return collision_rect_any(lb, tb, rb - 1, bb - 1, OBJ_oSolid, 1, i);
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
            if (!CPm(X(i) - 1, Y(i) + 16, OBJ_oSolid)) {
                PE(p)->status = E_IDLE;
                PE(p)->counter = (int16_t)RAND(20, 50);
                PE(p)->xVel = 0;
            }
        } else {
            PE(p)->xVel = N(1.5);
            if (!CPm(X(i) + 16, Y(i) + 16, OBJ_oSolid)) {
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
        pin_setispd(o, 1);
        PE(o)->alarm[1] = RAND(8, 12);
    }
    if (DLT(PTOD(o->x), X(i))) PE(o)->xVel = NI(-RAND(2, 4));
    else PE(o)->xVel = NI(RAND(2, 4));
    if (DLT(PTOD(o->y), Y(i))) PE(o)->yVel = NI(-RAND(2, 4));
    if (PE(o)->held && instance_exists_p(OBJ_oCharacter)) PL.holdItem = NOONE;
}

/* ==== oTombLord, oFly, oSceptre, oGoldDoor ======================================================================= */
#define attackTimer firing                     /* oTombLord.attackTimer: PEN firing */
enum { TL_TURN = 2, TL_ATTACK = 3 };

static int bloodless_of(int i) { return PX(i).obj == OBJ_oSkeleton || PX(i).obj == OBJ_oTombLord; }

/* objects/oTombLord/Create_0.gml (oEnemy Create first) */
static void tomblord_create(int i)
{
    struct pin *p;
    pen_enemy_create(i);
    p = &PX(i);
    PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;
    setCollisionBounds(i, 6, 0, 26, 32);
    PE(p)->xVel = N(2.5);
    pin_setispd(p, (img_t)0.25);
    p->type = T_TOMBLORD;
    PE(p)->hp = 20;
    p->invincible = 0;
    PE(p)->heavy = 1;
    PE(p)->status = E_IDLE;
    PE(p)->canPickUp = 0;
    PEN(p)->bounced = 0;
    PEN(p)->edead = 0;
    PEN(p)->whipped = 0;
    PE(p)->counter = 0;
    PEN(p)->attackTimer = 0;
    PE(p)->facing = E_RIGHT;
}

/* objects/oTombLord/Step_0.gml */
static void tomblord_step(int i)
{
    struct pin *p;
    int c;
    pen_parent_step(i);
    p = &PX(i);
    if (!vw(i, 36, 0)) return;
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    PE(p)->yVel += PE(p)->myGrav;
    if (NGT(PE(p)->yVel, PEN(p)->yVelLimit)) PE(p)->yVel = PEN(p)->yVelLimit;
    if (CP(X(i) + 16, Y(i) + 16, OBJ_oSolid)) PE(p)->hp = 0;
    if (PE(p)->hp < 1) {                                                   /* :15 */
        int k;
        {   /* scrCreateBlood(x+14+rand(0,4), y+14+rand(0,4), 4): bloodless, but the arguments draw */
            int yy = RAND(0, 4), xx = RAND(0, 4);
            (void)yy; (void)xx;
        }
        for (k = 0; k < 4; k++) {
            int yy = RAND(0, 6), xx = RAND(0, 4);
            pin_create(PX(i).x + PI(14 + xx), PX(i).y + PI(12 + yy), OBJ_oBone);
        }
        if (G.currLevel == 13) pin_create(PX(i).x + PI(16), PX(i).y + PI(16), OBJ_oSceptre);
        if (PEN(&PX(i))->countsAsKill) {
            PG.tomblords += 1;
            PG.kills += 1;
        }
        pin_destroy(i);
    }
    p = &PX(i);
    if (isCollisionBottom(i, 1) && PE(p)->status != E_STUNNED) PE(p)->yVel = 0;
    if (PEN(p)->attackTimer > 0) PEN(p)->attackTimer -= 1;
    if (PEN(p)->whipped > 0) PEN(p)->whipped -= 1;
    c = pl();
    if (PE(p)->status == E_IDLE) {
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        if (PE(p)->counter <= 0) PE(p)->status = E_WALK;
    } else if (PE(p)->status == E_WALK) {                                  /* :46 */
        double px = X(c), py = Y(c), x = X(i), y = Y(i);
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        if (PE(p)->facing == E_LEFT) {
            if (isCollisionLeft(i, 1) || (DGT(px, x + 16) && DLT(dabs(py - (y + 32)), 16) && PE(p)->counter == 0)) {
                pin_set_sprite(i, GSPR_sTombLordTurnR);
                PE(p)->status = TL_TURN;
                PE(p)->counter = 30;
            } else if (DLT(px, x + 16) && DLT(dabs(py - (y + 16)), 32) && PEN(p)->attackTimer == 0) {
                PE(p)->status = TL_ATTACK;
                pin_set_sprite(i, GSPR_sTombLordAttackL);
                pin_setimg(p, 0);
                PE(p)->xVel = 0;
            } else
                PE(p)->xVel = N(-1);
        } else if (PE(p)->facing == E_RIGHT) {
            if (isCollisionRight(i, 1) || (DLT(px, x + 16) && DLT(dabs(py - (y + 32)), 16) && PE(p)->counter == 0)) {
                pin_set_sprite(i, GSPR_sTombLordTurnL);
                PE(p)->status = TL_TURN;
                PE(p)->counter = 30;
            } else if (DGT(px, x + 16) && DLT(dabs(py - (y + 16)), 32) && PEN(p)->attackTimer == 0) {
                PE(p)->status = TL_ATTACK;
                pin_set_sprite(i, GSPR_sTombLordAttackR);
                pin_setimg(p, 0);
                PE(p)->xVel = 0;
            } else
                PE(p)->xVel = N(1);
        }
    } else if (PE(p)->status == TL_TURN) {
        PE(p)->xVel = 0;
    } else if (PE(p)->status == TL_ATTACK) {                               /* :91 */
        PE(p)->xVel = 0;
        pin_setispd(p, (img_t)0.5);
        PEN(p)->attackTimer = 100;
        if (DGE(p->img, 7) && DLE(p->img, 12)) {
            int f;
            if (PE(p)->facing == E_LEFT) {
                f = pin_create(p->x + PI(8), p->y + PI(12 + RAND(0, 4)), OBJ_oFly);
                PE(&PX(f))->xVel = NI(-RAND(3, 5));
            } else {
                f = pin_create(p->x + PI(24), p->y + PI(12 + RAND(0, 4)), OBJ_oFly);
                PE(&PX(f))->xVel = NI(RAND(3, 5));
            }
            p = &PX(i);
        }
    } else if (PE(p)->status >= E_STUNNED)
        PE(p)->status = E_WALK;
    if (isCollisionSolid(i)) pin_sety(p, p->y - PI(2));
    if (PE(p)->facing == E_LEFT) {
        if (PE(p)->status == E_WALK) pin_set_sprite(i, GSPR_sTombLordWalkL);
        else if (PE(p)->status == E_IDLE) pin_set_sprite(i, GSPR_sTombLordLeft);
    }
    if (PE(p)->facing == E_RIGHT) {
        if (PE(p)->status == E_WALK) pin_set_sprite(i, GSPR_sTombLordWalkR);
        else if (PE(p)->status == E_IDLE) pin_set_sprite(i, GSPR_sTombLordRight);
    }
}

/* objects/oTombLord/Collision_oCharacter.gml */
static void tomblord_hit_player(int i, int c)
{
    struct pin *p = &PX(i), *o = &PX(c);
    double dx = PTOD(o->x) - (X(i) + 16);
    if (DGT(dabs(dx), 16)) {
    } else if (!PL.dead && (PL.state == JUMPING || PL.state == FALLING) && DLT(PTOD(o->y), Y(i) + 8) && !PL.swimming) {
        PE(o)->yVel = N(-6) - NMUL(N(0.2), PE(o)->yVel);
        if (PG.hasSpikeShoes) PE(p)->hp -= (int16_t)(3 * (PL.fallTimer / 16 + 1));
        else PE(p)->hp -= (int16_t)(1 * (PL.fallTimer / 16 + 1));
        PL.fallTimer = 0;
        PEN(p)->countsAsKill = 1;
        pin_create(p->x + PI(16), p->y, OBJ_oBone);
        snd_play(SND_xhit);                                                    /* :17 */
    } else if (PL.invincible == 0) {
        PL.blink = 30;
        PL.invincible = 30;
        if (DLT(PTOD(o->y), Y(i))) PE(o)->yVel = N(-6);
        PE(o)->xVel = DLT(PTOD(o->x), X(i)) ? N(-6) : N(6);
        if (PG.plife > 0) PG.plife -= 2;
        snd_play(SND_xhurt);                                                   /* :35 */
    }
}

/* objects/oTombLord/Collision_oWhip.gml; oWhipPre: oEnemy's (bloodless: no blood) */
static void tomblord_whipped(int i, int w)
{
    struct pin *p = &PX(i);
    if (PX(w).obj == OBJ_oWhipPre) {                                   /* objects/oEnemy/Collision_oWhipPre.gml */
        PE(p)->hp -= 1;
        PEN(p)->countsAsKill = 1;
        snd_play(SND_xhit);                                                    /* :8 */
        return;
    }
    if (PEN(p)->whipped == 0 && DLT(Y(w), Y(i) + 12)) {
        PE(p)->hp -= 1;
        PEN(p)->countsAsKill = 1;
        pin_create(p->x + PI(16), p->y + PI(24), OBJ_oBlood);
        snd_play(SND_xhit);                                                    /* :6 */
        PEN(&PX(i))->whipped = 10;
    }
}

static int tomblord_animend(int i)
{
    struct pin *p = &PX(i);                                            /* objects/oTombLord/Other_7.gml */
    if (p->spr == GSPR_sTombLordTurnR) {
        PE(p)->facing = E_RIGHT;
        PE(p)->status = E_WALK;
    }
    if (p->spr == GSPR_sTombLordTurnL) {
        PE(p)->facing = E_LEFT;
        PE(p)->status = E_WALK;
    }
    if (p->spr == GSPR_sTombLordAttackL || p->spr == GSPR_sTombLordAttackR) {
        PE(p)->status = E_IDLE;
        PE(p)->counter = 30;
        pin_setispd(p, (img_t)0.25);
    }
    return 1;
}

/* oFly collisions */
static void fly_hit(int i, int o)
{
    struct pin *f = &PX(i), *q = &PX(o);
    int oo = q->obj;
    if (obj_is(oo, OBJ_oCharacter)) {                                  /* objects/oFly/Collision_oCharacter.gml */
        if (q->spr == GSPR_sPExit || q->spr == GSPR_sDamselExit || q->spr == GSPR_sTunnelExit) return;
        if (PG.plife > 0) PG.plife -= 2;
        PE(q)->xVel = PE(f)->xVel;
        PE(q)->yVel = N(-4);
        pin_create(q->x, q->y, OBJ_oBlood);
        PL.stunned = 1;
        PL.stunTimer = 20;
        snd_play(SND_xhurt);                                                   /* :18 */
        pin_destroy(i);
    } else if (obj_is(oo, OBJ_oDamsel)) {                              /* objects/oFly/Collision_oDamsel.gml */
        if (q->invincible) return;
        if (PEN(q)->bloodLeft > 0) {
            scrCreateBlood(o, (pos)(X(o) + sprw(o) / 2.0), (pos)(Y(o) + sprh(o) / 2.0), 1);
            q = &PX(o);
            if (PE(q)->hp < 0) PEN(q)->bloodLeft -= 1;
        }
        if (PE(q)->held) {
            PE(q)->held = 0;
            PL.holdItem = NOONE;
        }
        PE(q)->hp -= 2;
        PE(q)->yVel = N(-6);
        PE(q)->status = 2;
        PE(q)->counter = 120;
        PE(q)->xVel = NMUL(PE(&PX(i))->xVel, N(0.3));
        snd_play(SND_xdamsel);                                                 /* :24 */
        pin_destroy(i);
    } else if (obj_is(oo, OBJ_oEnemy)) {                               /* objects/oFly/Collision_oEnemy.gml */
        if (oo == OBJ_oTombLord) return;
        if (PE(q)->heavy) {
            PE(q)->xVel = NMUL(PE(f)->xVel, N(0.5));
            PE(q)->yVel = N(-2);
        } else {
            PE(q)->xVel = PE(f)->xVel;
            PE(q)->yVel = N(-4);
        }
        PE(q)->xVel = PE(f)->xVel;
        PE(q)->yVel = N(-4);
        PE(q)->hp -= 2;
        if (PEN(q)->bloodLeft > 0) {
            if (!bloodless_of(o)) scrCreateBlood(o, (pos)(X(o) + sprw(o) / 2.0), (pos)(Y(o) + sprh(o) / 2.0), 1);
            q = &PX(o);
            if (PE(q)->hp < 0) PEN(q)->bloodLeft -= 1;
        }
        PE(q)->status = 98;
        PE(q)->counter = 20;
        snd_play(SND_xhit);                                                    /* :28 */
        pin_destroy(i);
    } else if (obj_is(oo, OBJ_oSolid)) {                               /* objects/oFly/Collision_oSolid.gml */
        pin_create(f->x, f->y, OBJ_oSmokePuff);
        snd_play(SND_xhit);                                                    /* :2 */
        pin_destroy(i);
    }
}

/* objects/oGoldDoor/Collision_oSceptre.gml */
static void golddoor_sceptre(int self, int s)
{
    if (!PE(&PX(s))->held) return;
    if (PG.hasCrown) {
        PE(&PX(s))->held = 0;
        PL.holdItem = NOONE;
        PL.pickupItemType = T_NONE;
        pin_destroy(s);
        snd_play(SND_xchestopen);                                              /* :13 */
        pin_create(PX(self).x, PX(self).y, OBJ_oXGold);
        pin_destroy(self);
    } else
        pmsg_str("THE SCEPTRE FITS...", "BUT NOTHING IS HAPPENING!", 100);
}

/* ==== oSmashTrap / oSmashTrapLit, oCeilingTrap, oDoor ============================================================= */
/* oSmashTrap's xv, yv, xa, ya, dir, hit */
#define xv px
#define yv py
#define xa bounceFactor
#define ya frictionFactor
#define sdir state
#define hit trigger
enum { SM_RIGHT = 0, SM_DOWN = 1, SM_LEFT = 2, SM_UP = 3 };

/* objects/oSmashTrap/Create_0.gml (oMovingSolid / oSolid Create: create_solid ran) */
static void smashtrap_create(int i, int fromgen)
{
    struct pin *p = &PX(i);
    PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;
    setCollisionBounds(i, 1, 1, 15, 15);
    p->invincible = 0;
    PE(p)->xv = PE(p)->yv = PE(p)->xa = PE(p)->ya = 0;
    PE(p)->status = 0;
    PE(p)->hit = 0;
    PE(p)->counter = 0;
    PE(p)->sdir = fromgen ? play_gen_inst->dir : (int16_t)RAND(0, 3);
    PE(p)->colLeft = PE(p)->colRight = PE(p)->colTop = PE(p)->colBot = 0;
}

/* objects/oSmashTrap/Step_0.gml */
static void smashtrap_step(int i)
{
    struct pin *p = &PX(i);
    if (!vw(i, 16, 16)) return;
    if (PE(p)->status == 0) {                                              /* IDLE */
        int c = instance_first_p(OBJ_oCharacter);
        double cx = X(c), cy = Y(c), x = X(i), y = Y(i);
        double dist = pdist2(x, y, cx, cy);              /* (squared: pdist2_lt) */
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        if (pdist2_lt(dist, 90) && PE(p)->counter < 1) {
            if (DLT(dabs(cy - (y + 8)), 8) && DGT(cx, x + 8) && !isCollisionRight(i, 2)) {
                PE(p)->status = 1;
                PE(p)->sdir = SM_RIGHT;
                PE(p)->xa = N(0.5);
            } else if (DLT(dabs(cx - (x + 8)), 8) && DGT(cy, y + 8) && !isCollisionBottom(i, 2)) {
                PE(p)->status = 1;
                PE(p)->sdir = SM_DOWN;
                PE(p)->ya = N(0.5);
            } else if (DLT(dabs(cy - (y + 8)), 8) && DLT(cx, x + 8) && !isCollisionLeft(i, 2)) {
                PE(p)->status = 1;
                PE(p)->sdir = SM_LEFT;
                PE(p)->xa = N(-0.5);
            } else if (DLT(dabs(cx - (x + 8)), 8) && DLT(cy, y + 8) && !isCollisionTop(i, 2)) {
                PE(p)->status = 1;
                PE(p)->sdir = SM_UP;
                PE(p)->ya = N(-0.5);
            }
        }
    } else if (PE(p)->status == 1) {                                       /* ATTACK :37 */
        PE(p)->colLeft = PE(p)->colRight = PE(p)->colTop = PE(p)->colBot = 0;
        if (isCollisionLeft(i, 1)) PE(p)->colLeft = 1;
        if (isCollisionRight(i, 1)) PE(p)->colRight = 1;
        if (isCollisionTop(i, 1)) PE(p)->colTop = 1;
        if (isCollisionBottom(i, 1)) PE(p)->colBot = 1;
        if (NLT(NABS(PE(p)->xv), N(4))) PE(p)->xv += PE(p)->xa;
        if (NLT(NABS(PE(p)->yv), N(4))) PE(p)->yv += PE(p)->ya;
        pin_setx(p, PADDV(p->x, PE(p)->xv));
        pin_sety(p, PADDV(p->y, PE(p)->yv));
        if (PE(p)->sdir == SM_RIGHT) {
            if (isCollisionRight(i, 2) && PE(p)->colRight) { pin_setx(p, p->x - PI(2)); PE(p)->hit = 1; }
            if (PE(p)->colRight) { pin_setx(p, p->x - PI(1)); PE(p)->hit = 1; }
        } else if (PE(p)->sdir == SM_DOWN) {
            if (isCollisionBottom(i, 2) && PE(p)->colBot) { pin_sety(p, p->y - PI(2)); PE(p)->hit = 1; }
            if (PE(p)->colBot) { pin_sety(p, p->y - PI(1)); PE(p)->hit = 1; }
        } else if (PE(p)->sdir == SM_LEFT) {
            if (isCollisionLeft(i, 2) && PE(p)->colLeft) { pin_setx(p, p->x + PI(2)); PE(p)->hit = 1; }
            if (PE(p)->colLeft) { pin_setx(p, p->x + PI(1)); PE(p)->hit = 1; }
        } else if (PE(p)->sdir == SM_UP) {
            if (isCollisionTop(i, 2) && PE(p)->colTop) { pin_sety(p, p->y + PI(2)); PE(p)->hit = 1; }
            if (PE(p)->colTop) { pin_sety(p, p->y + PI(1)); PE(p)->hit = 1; }
        }
        if (collision_rect_p(X(i) - 1, Y(i) - 1, X(i) + 17, Y(i) + 17, OBJ_oTombLord, 0, NOONE) != NOONE) PE(p)->hit = 1;
        if (PE(p)->hit) PE(p)->xv = PE(p)->yv = PE(p)->xa = PE(p)->ya = 0;
        if (PE(p)->hit && !PE(p)->colRight && !PE(p)->colLeft && !PE(p)->colTop && !PE(p)->colBot) {
            PE(p)->status = 0;
            PE(p)->hit = 0;
            PE(p)->counter = 50;
        }
    } else if (PE(p)->status == 99) {                                      /* :124 */
        PE(p)->xv = PE(p)->yv = PE(p)->xa = PE(p)->ya = 0;
        pin_sety(p, PADDV(p->y, N(0.05)));
        if (collision_point_any_at(i, 0, -1, OBJ_oLava)) pin_destroy(i);
    }
    if (collision_rect_any_at(i, 1, 1, 15, 15, OBJ_oLava)) PE(p)->status = 99;
}

/* the rubble of oSmashTrap / oCeilingTrap Destroy (sRubbleTan) and oDoor Destroy (k = 4, small only) */
static void rubble_tan(int i, int obj, int kx, int spr)
{
    struct pin *p = &PX(i);
    int ya = RAND(0, 8), yb = RAND(0, 8);
    int xa_ = RAND(0, kx), xb = RAND(0, kx);
    int r = pin_create(p->x + PI(8 + xa_ - xb), p->y + PI(8 + ya - yb), obj);
    pin_set_sprite(r, spr);
}

/* objects/oCeilingTrap/Create_0.gml, oDoor/Create_0.gml */
static void ceiling_create(int i, int door)
{
    struct pin *p = &PX(i);
    PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;
    if (door) setCollisionBounds(i, 1, 0, 15, 32);
    else setCollisionBounds(i, 0, 0, 16, 16);
    p->invincible = 0;
    if (!door) pin_setispd(p, (img_t)0.4);
    PE(p)->xVel = 0;
    PE(p)->yVel = 0;
    PE(p)->myGrav = N(1);
    PE(p)->counter = door ? 0 : 3;
    PE(p)->status = 0;
}

/* objects/oCeilingTrap/Step_0.gml */
static void ceiling_step(int i)
{
    struct pin *p = &PX(i);
    if (PE(p)->status == 1) {                                              /* DROP */
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        else {
            PE(p)->counter = 3;
            pin_sety(p, p->y + PI(1));
        }
        PE(p)->yVel = 0;
        if (CP(X(i) + 8, Y(i) + 17, OBJ_oSolid)) PE(p)->status = 2;
        if (p->spr == GSPR_sBlock) pin_set_sprite(i, GSPR_sCeilingTrapS);
    } else if (PE(p)->status == 2) {                                       /* WAIT */
        PE(p)->yVel = 0;
        if (isCollisionBottom(i, 1)) pin_sety(p, p->y - PI(1));
    }
}

/* objects/oDoor/Step_0.gml (dist = distance_to_object(oCharacter): unused) */
static void door_step(int i)
{
    struct pin *p = &PX(i);
    if (PE(p)->status == 1) {                                              /* DROP */
        PE(p)->yVel += PE(p)->myGrav;
        if (NGT(PE(p)->yVel, N(6))) PE(p)->yVel = N(6);
        if (isCollisionBottom(i, 1)) {
            PE(p)->status = 2;
            PE(p)->yVel = 0;
            PE(p)->counter = 100;
            pin_setdepth(p, 100);
        }
    } else if (PE(p)->status == 2) {
        if (isCollisionBottom(i, 1)) pin_sety(p, p->y - PI(1));
    }
}

/* ==== oTemple, oTempleFake ======================================================================================== */
/* scripts/scrGenerateItem's underground set (argument2 = 2) in play: the item and its cost / forSale */
static void gen_item_underground(pos x, pos y)
{
    static const int16_t objs[19] = { OBJ_oJetpack, OBJ_oCapePickup, OBJ_oShotgun, OBJ_oMattock, OBJ_oTeleporter,
        OBJ_oGloves, OBJ_oSpectacles, OBJ_oWebCannon, OBJ_oPistol, OBJ_oMitt, OBJ_oPaste, OBJ_oSpringShoes,
        OBJ_oSpikeShoes, OBJ_oMachete, OBJ_oBombBox, OBJ_oBow, OBJ_oCompass, OBJ_oParaPickup, OBJ_oRopePile };
    static const int8_t dx[19] = { 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    static const int8_t dy[19] = { -2, 0, 0, 0, 3, -1, 0, 0, 0, -1, 0, 0, 0, 0, -2, 0, 0, 0, 0 };
    int n = RAND(0, 18);
    int o = pin_create(x + PI(dx[n]), y + PI(dy[n]), objs[n]);
    PE(&PX(o))->cost = 0;
    PE(&PX(o))->forSale = 0;
}

/* objects/oTemple/Create_0.gml in play (oTempleFake, Olmec's room): the generator's block_gems in play */
static void temple_create(int i)
{
    struct pin *p = &PX(i);
    int n, x = PFLOOR(p->x), y = PFLOOR(p->y);
    p->cleanDeath = 0;
    n = RAND(1, 100);
    if (G.cityOfGold) pin_set_sprite(i, GSPR_sGTemple);
    else if (n < 20) pin_set_sprite(i, GSPR_sTempleGold);
    else if (n < 30) pin_set_sprite(i, GSPR_sTempleGoldBig);
    else if (isLevel() && x > 1 && x < PW.room_w - 16 && y > 1 && y < PW.room_h - 16) {
        if (RAND(1, 60) == 1) pin_create(p->x + PI(8), p->y + PI(8), OBJ_oSapphireBig);
        else if (RAND(1, 80) == 1) pin_create(p->x + PI(8), p->y + PI(8), OBJ_oEmeraldBig);
        else if (RAND(1, 100) == 1) pin_create(p->x + PI(8), p->y + PI(8), OBJ_oRubyBig);
        else if (RAND(1, 1200) == 1) gen_item_underground(p->x + PI(8), p->y + PI(8));
    }
}

/* objects/oTemple/Destroy_0.gml (oSolid's Destroy first: pobj.c) */
static void gold_piece(int i, int obj)
{
    struct pin *p = &PX(i);
    int ya = RAND(0, 4), yb = RAND(0, 4);
    int xa_ = RAND(0, 4), xb = RAND(0, 4);
    int g = pin_create(p->x + PI(8 + xa_ - xb), p->y + PI(8 + ya - yb), obj);
    int a = RAND(0, 3), b = RAND(0, 3);
    PE(&PX(g))->xVel = NI(a - b);
    PE(&PX(g))->yVel = NI(RAND(2, 4) * 1);
}

static void temple_destroy(int i)
{
    struct pin *p = &PX(i);
    int k;
    destroy_solid(i);                                                  /* action_inherited: oSolid's Destroy */
    p = &PX(i);
    if (p->cleanDeath || G.cleanSolids) return;
    rubble_tan(i, OBJ_oRubble, 8, GSPR_sRubbleLush);
    rubble_tan(i, OBJ_oRubbleSmall, 8, GSPR_sRubbleLushSmall);
    rubble_tan(i, OBJ_oRubbleSmall, 8, GSPR_sRubbleLushSmall);
    p = &PX(i);
    if (p->spr == GSPR_sTempleGold) {
        for (k = 0; k < 3; k++) gold_piece(i, OBJ_oGoldChunk);
    } else if (p->spr == GSPR_sTempleGoldBig || G.cityOfGold) {
        for (k = 0; k < 3; k++) gold_piece(i, OBJ_oGoldChunk);
        gold_piece(i, OBJ_oGoldNugget);
    }
    if (PX(i).treasure == TR_BIGRUBY) pin_create(PX(i).x + PI(8), PX(i).y + PI(8), OBJ_oRubyBig);
}

/* ==== the player's temple sites (oPlayer1 Step) =================================================================== */
static void player_smashtrap(int i)
{
    struct pin *p = &PX(i);                                            /* :1623 */
    int obj = instance_nearest_p(X(i), Y(i), OBJ_oSmashTrap);
    PG.plife -= 10;
    if (DLT(X(obj) + 8, X(i))) PE(p)->xVel = NI(-RAND(4, 6));
    else PE(p)->xVel = NI(RAND(4, 6));
    PE(p)->yVel = N(-6);
    if (obj != NOONE && PE(&PX(obj))->sdir == SM_DOWN) PE(p)->yVel = N(4);
    scrCreateBlood(i, p->x, p->y, 1);
    if (PL.holdItem != NOONE) {
        PE(&PX(PL.holdItem))->held = 0;
        PL.holdItem = NOONE;
    }
}

static void player_lava(int i)
{
    struct pin *p = &PX(i);                                            /* :195 */
    if (!PL.dead) snd_play(SND_xflame);                                        /* :200 */
    PG.plife -= 99;
    PE(p)->xVel = 0;
    PE(p)->yVel = N(0.1);
    PE(p)->grav = 0;
    PE(p)->myGrav = 0;
    PL.bounced = 1;
    PL.burning = 100;
    pin_setdepth(p, 999);
}

/* :1228 the gold idol's trap outside the mines (levelType 0 is pplayer.c's) */
static void player_idoltrap(int i)
{
    int h = PL.holdItem;
    int16_t w[256];
    int n, k;
    if (G.levelType == 1) {
        if (G.cemetary && !PG.ghostExists) {
            view_read();
            if (DGT(X(i), PW.room_w / 2.0)) pin_create(PI(PW.xview + 320 + 8), PI(PW.yview + 120), OBJ_oGhost);
            else pin_create(PI(PW.xview - 32), PI(PW.yview + 120), OBJ_oGhost);
            PG.ghostExists = 1;
        }
        n = pw_with(OBJ_oTrapBlock, w, 256);
        for (k = 0; k < n; k++)
            if (PX(w[k]).alive && DLT(distance_to_object_p(w[k], OBJ_oCharacter), 90)) PE(&PX(w[k]))->dying = 1;
    } else if (G.levelType == 3) {
        if (instance_exists_p(OBJ_oCeilingTrap)) {
            int t;
            n = pw_with(OBJ_oCeilingTrap, w, 256);
            for (k = 0; k < n; k++) {
                if (!PX(w[k]).alive) continue;
                PE(&PX(w[k]))->status = 1;
                PE(&PX(w[k]))->yVel = N(0.5);
            }
            scrShake(20);
            t = instance_nearest_p(X(i) - 64, Y(i) - 64, OBJ_oDoor);
            PE(&PX(t))->status = 1;
            PE(&PX(t))->yVel = N(1);
            t = instance_nearest_p(X(i) + 64, Y(i) - 64, OBJ_oDoor);
            PE(&PX(t))->status = 1;
            PE(&PX(t))->yVel = N(1);
        } else {
            n = pw_with(OBJ_oTrapBlock, w, 256);
            for (k = 0; k < n; k++) {
                if (!PX(w[k]).alive) continue;
                if (DLT(distance_to_object_p(w[k], OBJ_oCharacter), 90)) pin_destroy(w[k]);
                snd_play(SND_xthump);                                          /* :1282 */
                scrShake(10);
            }
        }
    }
    PE(&PX(h))->trigger = 0;
}

/* ==== items, ropes and detritus in lava =========================================================================== */
/* oItem Step :160-185 (sites 1032: the rectangle (:160) then the point (:173); see struct mark), oJar / oSkull
   Step :79-100 (1036), oTreasure Step :70-80 (1039) */
static struct mark mk_item;
static void lava_sink(struct pin *p)
{
    PE(p)->myGrav = 0;
    PE(p)->xVel = 0;
    PE(p)->yVel = 0;
    pin_sety(p, PADDV(p->y, N(0.05)));
}
static int lava_rect(int i) { return collision_rect_any_at(i, -3, -3, 3, 3, OBJ_oLava); }
static int lava_point(int i) { return collision_point_any_at(i, 0, -5, OBJ_oLava); }
static void lava_melt(int i)
{
    struct pin *p = &PX(i);
    if (p->type == T_BOMB) {
        int k;
        pin_create(p->x, p->y, OBJ_oExplosion);
        for (k = 0; k < 3; k++) pin_create(PX(i).x, PX(i).y, OBJ_oFlame);
        snd_play(SND_xexplosion);                                              /* oItem :181, oJar :98 */
    }
    pin_destroy(i);
}
static void lava_item(int i)
{
    if (!marked(&mk_item, i) && lava_rect(i)) {
        mark_set(&mk_item, i);
        lava_sink(&PX(i));
        return;
    }
    lava_melt(i);
}
static void lava_jar(int i)
{
    if (lava_rect(i)) lava_sink(&PX(i));
    if (lava_point(i)) lava_melt(i);
}
static void lava_treasure(int i)
{
    if (lava_rect(i)) lava_sink(&PX(i));
    if (lava_point(i)) pin_destroy(i);
}

/* objects/oRope/Step_0.gml (1056: lava next to it and burnTimer 0, :1; or burnTimer reached 1, :7) */
static void lava_rope(int i)
{
    struct pin *p = &PX(i);
    if (PE(p)->burnTimer == 1) {                                       /* :7 */
        if (PL.state == CLIMBING && collision_point_p(X(i) + 12, Y(i) + 4, OBJ_oPlayer1, 0, NOONE) != NOONE)
            PL.state = FALLING;
        pin_destroy(i);
        return;
    }
    pin_create(p->x + PI(8), p->y, OBJ_oRopeBurn);                     /* :3 */
}

/* objects/oRopeBurn/Step_0.gml */
static void ropeburn_step(int i)
{
    struct pin *p = &PX(i);
    pin_sety(p, PADDV(p->y, PE(p)->yVel));
    if (CP(X(i), Y(i), OBJ_oSolid) ||
        collision_rect_p(X(i) - 1, Y(i) - 8, X(i) + 1, Y(i) + 8, OBJ_oRope, 0, NOONE) == NOONE)
        pin_destroy(i);
    if (CP(X(i), Y(i), OBJ_oRope)) {
        int r = instance_nearest_p(X(i), Y(i), OBJ_oRope);
        if (r != NOONE) PE(&PX(r))->burnTimer = 1;
    }
}

/* ==== D2: oOlmec ================================================================================================== */
/* oOlmec's carryPlayer, slammed, toggle */
#define carryPlayer armed
#define slammed trigger
#define toggle sticky
enum { OL_START2 = -2, OL_START1 = -1, OL_IDLE = 0, OL_BOUNCE = 1, OL_RECOVER = 2, OL_DROWNING = 4, OL_PREPARE = 5,
       OL_SLAM = 6, OL_CREATE = 7 };

/* objects/oOlmec/Create_0.gml (fromgen: facing drawn by the generator) */
static void olmec_create(int i, int fromgen)
{
    struct pin *p = &PX(i);
    p->type = T_OTHER;                                                     /* "Olmec" */
    p->shopWall = 0;
    PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;
    setCollisionBounds(i, 2, 0, 62, 64);
    PE(p)->myGrav = N(0.2);
    p->invincible = 1;
    PE(p)->carryPlayer = 0;
    pin_setispd(p, (img_t)0.4);
    if (!fromgen) PE(p)->facing = (int16_t)RAND(0, 1);
    PE(p)->status = OL_START1;
    PE(p)->counter = 0;
    PE(p)->slammed = 0;
    play_hborder = 0;                                                      /* :37 */
    PW.vborder = 0;
    view_set_y(400);
    play_view_obj = OBJ_oOlmec;
}

static void olmec_debris(int i, int xr, int x0, int yr, int y0, int xvk)
{
    int yy = RAND(0, yr), xx = RAND(0, xr), d;
    d = pin_create(PX(i).x + PI(x0 + xx), PX(i).y + PI(y0 + yy), OBJ_oOlmecDebris);
    if (xvk == 1) PE(&PX(d))->xVel = NI(RAND(1, 4));
    else if (xvk == -1) PE(&PX(d))->xVel = NI(-RAND(1, 4));
    else {
        int a = RAND(1, 4), b = RAND(1, 4);
        PE(&PX(d))->xVel = NI(a - b);
    }
    PE(&PX(d))->yVel = NI(-RAND(1, 3));
}

/* objects/oOlmec/Step_0.gml */
static void olmec_step(int i)
{
    struct pin *p = &PX(i);
    int c = pl();
    if (PE(p)->carryPlayer ||
        collision_rect_p(X(i) - 1, Y(i), X(i) + 66, Y(i) + 62, OBJ_oPlayer1, 0, NOONE) != NOONE) {
        struct pin *q = &PX(c);
        pin_setx(q, PADDV(q->x, PE(p)->xVel));
        pin_sety(q, PADDV(q->y, PE(p)->yVel));
    }
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    if (NLT(PE(p)->yVel, N(6))) PE(p)->yVel += PE(p)->myGrav;
    if (isCollisionTop(i, 1)) {                                            /* :14 */
        pin_create(p->x, p->y - PI(16), OBJ_oOlmecSlam);
        p = &PX(i);
        pin_sety(p, p->y + PI(1));
        if (NLT(PE(p)->yVel, N(0))) PE(p)->yVel = NMUL(-PE(p)->yVel, N(0.8));
    }
    if (isCollisionLeft(i, 1)) {
        pin_setx(p, p->x + PI(1));
        PE(p)->xVel = 0;
    }
    if (isCollisionRight(i, 1)) {
        pin_setx(p, p->x - PI(1));
        PE(p)->xVel = 0;
    }
    if (collision_point_any_at(i, 0, 64, OBJ_oLava)) PE(p)->status = OL_DROWNING;
    if (collision_point_any_at(i, 0, -2, OBJ_oLava)) {                                  /* :36 */
        G.olmecDead = 1;
        PG.kills += 1;
        pin_destroy(i);
    }
    p = &PX(i);
    if (collision_rect_p(X(i), Y(i) - 2, X(i) + 64, Y(i) + 64, OBJ_oPlayer1, 0, NOONE) != NOONE) PE(p)->carryPlayer = 1;
    else PE(p)->carryPlayer = 0;
    switch (PE(p)->status) {
    case OL_START1:                                                        /* :50 */
        view_read();
        if (PW.xview < 176) view_set_x(PW.xview + 2);
        else {
            PE(p)->alarm[1] = 100;
            PE(p)->status = OL_START2;
        }
        if (isCollisionBottom(i, 1)) PE(p)->yVel = 0;
        break;
    case OL_START2:
        if (isCollisionBottom(i, 1)) PE(p)->yVel = 0;
        break;
    case OL_IDLE:                                                          /* :67 */
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        if (PE(p)->counter == 0) PE(p)->status = OL_BOUNCE;
        if (isCollisionBottom(i, 1)) PE(p)->yVel = 0;
        PE(p)->toggle = 1;
        break;
    case OL_CREATE: {                                                      /* :79 */
        int k;
        for (k = 0; k < 6; k++) {
            int ya = RAND(0, 32), yb = RAND(0, 32);
            int xa_ = RAND(0, 32), xb = RAND(0, 32);
            pin_create(PX(i).x + PI(32 + xa_ - xb), PX(i).y + PI(14 + ya - yb), OBJ_oPsychicCreate2);
        }
        for (k = 0; k < 3; k++) pin_create(PX(i).x + PI(32), PX(i).y + PI(16), OBJ_oYellowBall);
        snd_play(SND_xpsychic);                                                /* :88 */
        p = &PX(i);
        PE(p)->status = OL_IDLE;
        break;
    }
    case OL_RECOVER:                                                       /* :91 */
        if (isCollisionBottom(i, 1)) {
            snd_play(SND_xthump);                                              /* :95 */
            PE(p)->status = OL_IDLE;
            PE(p)->xVel = 0;
            PE(p)->yVel = 0;
            PE(p)->counter = (int16_t)RAND(40, 100);
        } else {
            if (PE(p)->counter > 1) PE(p)->counter -= 1;
            else if (PE(p)->counter == 1) {
                if (DLT(X(c), X(i))) PE(p)->xVel = N(-0.25);
                else if (DGT(X(c), X(i) + 64)) PE(p)->xVel = N(0.25);
                else PE(p)->xVel = 0;
                PE(p)->counter -= 1;
            } else {
                if (NLT(PE(p)->xVel, N(0)) && PE(p)->toggle) PE(p)->xVel -= N(0.25);
                else if (NLT(PE(p)->xVel, N(0)) && !PE(p)->toggle) PE(p)->xVel += N(0.25);
                if (NGT(PE(p)->xVel, N(0)) && PE(p)->toggle) PE(p)->xVel += N(0.25);
                else if (NGT(PE(p)->xVel, N(0)) && !PE(p)->toggle) PE(p)->xVel -= N(0.25);
                if (NLE(PE(p)->xVel, N(-2)) || NGE(PE(p)->xVel, N(2))) PE(p)->toggle = !PE(p)->toggle;
            }
            if ((!PL.active && NGE(PE(p)->yVel, N(0))) ||
                (DGT(Y(c), Y(i)) && DLT(dabs(X(c) - (X(i) + 32)), 32) && NGT(PE(p)->xVel, N(-1)))) {
                PE(p)->status = OL_PREPARE;
                PE(p)->yVel = 0;
                PE(p)->xVel = 0;
                PE(p)->myGrav = 0;
                PE(p)->counter = 20;
            }
        }
        break;
    case OL_BOUNCE:                                                        /* :131 */
        if (isCollisionBottom(i, 1)) PE(p)->yVel = N(-4);
        else {
            PE(p)->counter = 10;
            PE(p)->status = OL_RECOVER;
            snd_play(SND_xbigjump);                                            /* :141 */
        }
        break;
    case OL_PREPARE:
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        else {
            PE(p)->yVel = N(5);
            PE(p)->myGrav = N(0.2);
            PE(p)->status = OL_SLAM;
            PE(p)->slammed = 0;
        }
        break;
    case OL_SLAM:                                                          /* :155 */
        PE(p)->carryPlayer = 0;
        if (isCollisionBottom(i, 1)) {
            if (!PE(p)->slammed) {
                pin_create(p->x, p->y + PI(64), OBJ_oOlmecSlam);
                p = &PX(i);
                PE(p)->slammed = 1;
                scrShake(5);
            } else {
                if (RAND(1, 2) == 1 || !PL.active) PE(p)->status = OL_IDLE;
                else PE(p)->status = OL_CREATE;
                PE(p)->xVel = 0;
                PE(p)->yVel = 0;
                PE(p)->counter = 60;
                if (!PL.active) PE(p)->alarm[5] = 50;
            }
        }
        break;
    case OL_DROWNING:                                                      /* :177 */
        PE(p)->xVel = 0;
        PE(p)->yVel = N(0.1);
        PE(p)->myGrav = 0;
        scrShake(10);
        if (!snd_is_playing(SND_xflame)) snd_play(SND_xflame);                 /* :183 */
        break;
    }
    if (isCollisionTop(i, 1)) PE(p)->yVel = N(1);
    if (isCollisionLeft(i, 1) || isCollisionRight(i, 1)) PE(p)->xVel = -PE(p)->xVel;
    if (isCollisionSolid(i)) pin_sety(p, p->y - PI(2));
}

static void olmec_alarm(int i, int a)
{
    struct pin *p = &PX(i);
    int16_t w[64];
    int n, k;
    switch (a) {
    case 1:                                                            /* objects/oOlmec/Alarm_1.gml */
        pin_set_sprite(i, GSPR_sOlmecStart2);
        for (k = 0; k < 6; k++) olmec_debris(i, 32, 32, 32, 0, 1);
        snd_play(SND_xthump);                                                  /* :8 */
        n = pw_with(OBJ_oHawkmanWorship, w, 64);
        for (k = 0; k < n; k++) {
            int hw = w[k], h;
            if (!PX(hw).alive) continue;
            h = pin_create(PX(hw).x, PX(hw).y, OBJ_oHawkman);
            PE(&PX(h))->status = 98;
            PE(&PX(h))->hp = 1;
            PE(&PX(h))->xVel = N(-3);
            PE(&PX(h))->yVel = N(-5);
            PE(&PX(h))->counter = 300;
            pin_destroy(hw);
        }
        n = pw_with(OBJ_oCavemanWorship, w, 64);
        for (k = 0; k < n; k++) {
            if (!PX(w[k]).alive) continue;
            pin_setimg(&PX(w[k]), 0);
            pin_setispd(&PX(w[k]), 0);
        }
        PE(&PX(i))->alarm[2] = 50;
        break;
    case 2:                                                            /* Alarm_2 */
        pin_set_sprite(i, GSPR_sOlmecStart3);
        PE(p)->alarm[3] = 50;
        for (k = 0; k < 6; k++) olmec_debris(i, 32, 0, 32, 0, -1);
        snd_play(SND_xthump);                                                  /* :9 */
        break;
    case 3:                                                            /* Alarm_3 */
        pin_set_sprite(i, GSPR_sOlmec);
        for (k = 0; k < 12; k++) olmec_debris(i, 64, 0, 32, 32, 0);
        snd_play(SND_xthump);                                                  /* :8 */
        PE(&PX(i))->alarm[4] = 50;
        break;
    case 4:                                                            /* Alarm_4 */
        PE(p)->toggle = 1;
        PE(p)->status = OL_BOUNCE;
        snd_play(SND_xbigjump);                                                /* :3 */
        snd_play(SND_xalert);                                                  /* :4 */
        PE(p)->alarm[6] = 20;
        break;
    case 5:                                                            /* Alarm_5 */
        play_hborder = 160;                                                /* global.display_w / 2 */
        PW.vborder = 64;
        /* Observed (c_temple_olmec_s1 record 437): this step's later reads see xview 0 and the old yview, without
           the target following of prun.c's view_read (the view object changed with it): xview without vdirty */
        PW.xview = 0;
        play_view_obj = OBJ_oPlayer1;
        PL.active = 1;
        PE(p)->status = OL_IDLE;
        PE(p)->counter = 100;
        snd_music(SND_mBoss, 1);                                               /* :8 */
        break;
    case 6:                                                            /* Alarm_6 */
        n = pw_with(OBJ_oCavemanWorship, w, 64);
        for (k = 0; k < n; k++) {
            int cw = w[k], cm;
            if (!PX(cw).alive) continue;
            cm = pin_create(PX(cw).x, PX(cw).y, OBJ_oCaveman);
            PE(&PX(cm))->facing = 1;
            PE(&PX(cm))->status = 2;
            pin_destroy(cw);
        }
        break;
    }
}

/* ==== oOlmecDebris, oOlmecSlam, oYellowBall, oPsychicCreate2, oFinalBoss ============================================ */
/* objects/oOlmecDebris/Create_0.gml */
static void debris_create(int i)
{
    struct pin *p = &PX(i);
    int n;
    p->type = T_NONE;                                                      /* oDrawnSprite: "" */
    pin_setispd(p, (img_t)0.3);
    PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;
    setCollisionBounds(i, -4, -4, 4, 4);
    {
        double a = prandom(4);
        double b = prandom(4);
        PE(p)->xVel = ND(a - b);
    }
    PE(p)->yVel = ND(-1 - prandom(2));
    PE(p)->grav = N(0.6);
    p->invincible = 1;
    PE(p)->bounce = 1;
    n = RAND(1, 3);
    if (n == 1) pin_set_sprite(i, GSPR_sOlmecDebris2);
    else if (n == 2) pin_set_sprite(i, GSPR_sOlmecDebris3);
}

/* objects/oOlmecDebris/Step_0.gml */
static void debris_step(int i)
{
    struct pin *p = &PX(i);
    pin_setx(p, PADDV(p->x, PE(p)->xVel));
    pin_sety(p, PADDV(p->y, PE(p)->yVel));
    if (PE(p)->bounce) {
        if (NLT(PE(p)->yVel, N(6))) PE(p)->yVel += PE(p)->grav;
        if (CP(X(i), Y(i) + 4, OBJ_oTemple)) {
            if (NGT(PE(p)->yVel, N(1))) PE(p)->yVel = NMUL(-PE(p)->yVel, N(0.4));
            else {
                pin_create(p->x, p->y, OBJ_oSmokePuff);
                pin_destroy(i);
                p = &PX(i);
            }
            if (NLT(NABS(PE(p)->xVel), N(0.1))) PE(p)->xVel = 0;
            else if (NNE(NABS(PE(p)->xVel), N(0))) PE(p)->xVel = NMUL(PE(p)->xVel, N(0.3));
        }
    }
}

/* objects/oYellowBall/Create_0.gml */
static void yellowball_create(int i)
{
    struct pin *p = &PX(i);
    p->type = T_NONE;
    PE(p)->yVel = ND(-1 * (prandom(3) + 4));
    PE(p)->xVel = NI(RAND(2, 5));
    if (RAND(1, 2) == 1) PE(p)->xVel = NMUL(PE(p)->xVel, N(-1));
    PE(p)->alarm[1] = 1;
}

/* objects/oYellowBall/Step_0.gml */
static void yellowball_step(int i)
{
    struct pin *p = &PX(i);
    pin_setx(p, PADDV(p->x, PE(p)->xVel));
    pin_sety(p, PADDV(p->y, PE(p)->yVel));
    if (collision_rect_any(X(i) - 8, Y(i) - 8, X(i) + 8, Y(i) + 8, OBJ_oSolid, 0, NOONE) &&
        collision_rect_p(X(i) - 8, Y(i) - 8, X(i) + 8, Y(i) + 8, OBJ_oOlmec, 0, NOONE) == NOONE) {
        int n, obj = -1;
        pin_setx(p, PSUBV(p->x, PE(p)->xVel));
        pin_sety(p, PSUBV(p->y, PE(p)->yVel));
        if (RAND(1, 2) == 1) n = RAND(1, 4);
        else n = RAND(1, 5);
        switch (n) {
        case 1: obj = OBJ_oBat; break;
        case 2: obj = OBJ_oSpider; break;
        case 3: obj = OBJ_oSnake; break;
        case 4: obj = OBJ_oFrog; break;
        case 5: obj = OBJ_oFireFrog; break;
        }
        if (obj >= 0) pin_create(p->x - PI(8), p->y - PI(8), obj);
        pin_destroy(i);
        p = &PX(i);
    }
    if (NLT(PE(p)->yVel, N(6))) PE(p)->yVel += N(0.15);
}

/* objects/oFinalBoss/Step_0.gml */
static void finalboss_step(int i)
{
    (void)i;
    if (G.olmecDead && !G.doorOpen) {
        int16_t w[16];
        int n = pw_with(OBJ_oEntrance, w, 16), k, b;
        G.doorOpen = 1;
        for (k = 0; k < n; k++) if (PX(w[k]).alive) pin_destroy(w[k]);
        pin_create(PI(640), PI(544), OBJ_oXEnd);
        if (!CP(640, 560, OBJ_oSolid)) {
            b = pin_create(PI(640), PI(560), OBJ_oTemple);
            PX(b).invincible = 1;
        } else {
            b = collision_point_p(640, 560, OBJ_oSolid, 1, NOONE);           /* instance_position */
            if (b != NOONE) PX(b).invincible = 1;
        }
        snd_play(SND_xthump);                                                  /* :16 */
    }
}

/* ==== dispatch ==================================================================================================== */
static int create_ev(int i, int fromgen)
{
    switch (PX(i).obj) {
    case OBJ_oHawkman: hawkman_create(i); return 1;
    case OBJ_oLava: lava_create(i, fromgen); return 1;
    case OBJ_oMagma: magma_create(i); return 1;
    case OBJ_oMagmaTrail: pin_setispd(&PX(i), (img_t)0.4); return 1;
    case OBJ_oXEnd: PX(i).type = T_NONE; return 1;                     /* oDrawnSprite Create: type = "" */
    case OBJ_oLavaDrip:                                               /* oRubblePiece Create, then its own */
        PX(i).type = T_NONE;
        PE(&PX(i))->xVel = 0;
        PE(&PX(i))->yVel = 0;
        PE(&PX(i))->yAcc = N(0.6);
        pin_setispd(&PX(i), (img_t)0.4);
        return 1;
    case OBJ_oMagmaMan: magmaman_create(i); return 1;
    case OBJ_oTombLord: tomblord_create(i); return 1;
    case OBJ_oFly:                                                     /* objects/oFly/Create_0.gml */
        PE(&PX(i))->xVel = 0;
        PE(&PX(i))->yVel = ND(-prandom(3) + 0.5);
        return 1;
    case OBJ_oSmashTrap: case OBJ_oSmashTrapLit: smashtrap_create(i, fromgen); return 1;
    case OBJ_oCeilingTrap: ceiling_create(i, 0); return 1;
    case OBJ_oDoor: ceiling_create(i, 1); return 1;
    case OBJ_oTemple: if (!fromgen) temple_create(i); return 1;
    case OBJ_oOlmec: olmec_create(i, fromgen); return 1;
    case OBJ_oOlmecDebris: debris_create(i); return 1;
    case OBJ_oOlmecSlam:                                               /* objects/oOlmecSlam/Create_0.gml */
        PE(&PX(i))->alarm[0] = 1;
        snd_play(SND_xslam);                                                   /* :2 */
        return 1;
    case OBJ_oYellowBall: yellowball_create(i); return 1;
    case OBJ_oYellowTrail: PX(i).type = T_NONE; pin_setispd(&PX(i), 1); return 1;
    case OBJ_oPsychicCreate2: {                                        /* objects/oPsychicCreate2/Create_0.gml */
        struct pin *p = &PX(i);
        int o = instance_first_p(OBJ_oOlmec);
        p->type = T_NONE;
        PE(p)->yVel = 0;
        PE(p)->yAcc = N(0.6);
        pin_setispd(p, (img_t)0.4);
        PE(p)->direction = point_direction_d(X(i), Y(i), X(o) + 32, Y(o) + 16);
        return 1;
    }
    case OBJ_oFinalBoss:                                               /* objects/oFinalBoss/Create_0.gml */
        G.olmecDead = 0;
        G.doorOpen = 0;
        return 1;
    case OBJ_oCavemanWorship: pin_setispd(&PX(i), (img_t)0.25); return 1;
    case OBJ_oLavaSolid: PX(i).invincible = 1; return 1;
    case OBJ_oRopeBurn:                                                /* objects/oRopeBurn/Create_0.gml */
        PX(i).type = T_NONE;
        PE(&PX(i))->yVel = N(-1);
        pin_setispd(&PX(i), (img_t)0.8);
        return 1;
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
    case OBJ_oTombLord: tomblord_step(i); return 1;
    case OBJ_oFly: {                                                   /* objects/oFly/Step_0.gml */
        struct pin *p = &PX(i);
        pin_setx(p, PADDV(p->x, PE(p)->xVel));
        pin_sety(p, PADDV(p->y, PE(p)->yVel));
        pin_set_sprite(i, NLT(PE(p)->xVel, N(0)) ? GSPR_sFlyLeft : GSPR_sFlyRight);
        return 1;
    }
    case OBJ_oSceptre:                                                 /* objects/oSceptre/Step_0.gml */
        item_step(i);
        if (PE(&PX(i))->held) pin_set_sprite(i, PL.facing == LEFT ? GSPR_sSceptreLeft : GSPR_sSceptreRight);
        return 1;
    case OBJ_oSmashTrap: case OBJ_oSmashTrapLit: smashtrap_step(i); return 1;
    case OBJ_oCeilingTrap: ceiling_step(i); return 1;
    case OBJ_oDoor: door_step(i); return 1;
    case OBJ_oOlmec: olmec_step(i); return 1;
    case OBJ_oOlmecDebris: debris_step(i); return 1;
    case OBJ_oYellowBall: yellowball_step(i); return 1;
    case OBJ_oPsychicCreate2: {                                        /* objects/oPsychicCreate2/Step_0.gml */
        struct pin *p = &PX(i);
        double d = degtorad_d(PE(p)->direction), si, co;
        psincos_cr(d, &si, &co);                                       /* (pcos_cr's, psin_cr's bits) */
        pin_setx(p, (pos)(PTOD(p->x) + 2 * co));
        pin_sety(p, (pos)(PTOD(p->y) + -2 * si));
        return 1;
    }
    case OBJ_oFinalBoss: finalboss_step(i); return 1;
    case OBJ_oRopeBurn: ropeburn_step(i); return 1;
    case OBJ_oTempleFake:                                              /* objects/oTempleFake/Step_0.gml */
        if (!CP(X(i) + 8, Y(i) + 8, OBJ_oDoor)) {
            pin_create(PX(i).x, PX(i).y, OBJ_oTemple);
            pin_destroy(i);
        }
        return 1;
    }
    return 0;
}

static int alarm_ev(int i, int a)
{
    switch (PX(i).obj) {
    case OBJ_oHawkman: if (a == 0) PEN(&PX(i))->whipped = 0; return 1;    /* objects/oHawkman/Alarm_0.gml */
    case OBJ_oMagmaMan: if (a == 0) PEN(&PX(i))->whipped = 0; return 1;   /* objects/oMagmaMan/Alarm_0.gml */
    case OBJ_oOlmec: olmec_alarm(i, a); return 1;
    case OBJ_oOlmecSlam: if (a == 0) pin_destroy(i); return 1;        /* objects/oOlmecSlam/Alarm_0.gml */
    case OBJ_oYellowBall:
        if (a == 0) PX(i).invincible = 0;                              /* objects/oYellowBall/Alarm_0.gml */
        else if (a == 1) {                                             /* Alarm_1 */
            if (instance_number_p(OBJ_oYellowTrail) < 12) pin_create(PX(i).x, PX(i).y, OBJ_oYellowTrail);
            PE(&PX(i))->alarm[1] = 4;
        }
        return 1;
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
    case OBJ_oTombLord: return tomblord_animend(i);
    case OBJ_oYellowTrail: case OBJ_oPsychicCreate2: pin_destroy(i); return 1;
    case OBJ_oCeilingTrap:                                             /* objects/oCeilingTrap/Other_7.gml */
        if (p->spr == GSPR_sCeilingTrapS) pin_set_sprite(i, GSPR_sCeilingTrap);
        return 1;
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
    case OBJ_oTombLord:
        if (obj_is(oo, OBJ_oCharacter)) tomblord_hit_player(self, other);
        else tomblord_whipped(self, other);
        return 1;
    case OBJ_oFly: fly_hit(self, other); return 1;
    case OBJ_oOlmecSlam:                                               /* Collision_oBlock / oPushBlock / oTemple */
        if (oo == OBJ_oBlock || oo == OBJ_oPushBlock || oo == OBJ_oTemple) {
            PX(other).cleanDeath = 0;                                  /* tile_delete: drawing only */
            pin_destroy(other);
        }
        return 1;                                                      /* Collision_oSolid: commented out */
    case OBJ_oYellowBall: return 1;                                    /* Collision_oSolid: commented out */
    case OBJ_oGoldDoor: golddoor_sceptre(self, other); return 1;
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
    case OBJ_oTemple: temple_destroy(i); return 1;
    case OBJ_oSmashTrap: case OBJ_oSmashTrapLit: case OBJ_oCeilingTrap:   /* Destroy_0.gml (no inherit) */
        if (!PX(i).cleanDeath && !G.cleanSolids) {
            rubble_tan(i, OBJ_oRubble, 8, GSPR_sRubbleTan);
            rubble_tan(i, OBJ_oRubbleSmall, 8, GSPR_sRubbleTanSmall);
            rubble_tan(i, OBJ_oRubbleSmall, 8, GSPR_sRubbleTanSmall);
        }
        return 1;
    case OBJ_oDoor:                                                    /* objects/oDoor/Destroy_0.gml */
        rubble_tan(i, OBJ_oRubbleSmall, 4, GSPR_sRubbleTanSmall);
        rubble_tan(i, OBJ_oRubbleSmall, 4, GSPR_sRubbleTanSmall);
        return 1;
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
    case FEV_OUTSIDE:
        if (PX(i).obj == OBJ_oYellowBall) { pin_destroy(i); return 1; }   /* objects/oYellowBall/Other_0.gml */
        return 0;
    }
    return 0;
}

/* gameStepEvent's oMovingSolid loop (penemy.c pen_moving_solids): viscidTop of moving solid s, -1 if not ours */
int ptemple_msolid(int s)
{
    switch (PX(s).obj) {
    case OBJ_oSmashTrap: case OBJ_oSmashTrapLit: case OBJ_oCeilingTrap: case OBJ_oDoor: case OBJ_oOlmec:
        return 1;                                                      /* Create_0.gml: viscidTop = 1 */
    }
    return -1;
}

int ptemple_player(int site, int i, int arg)
{
    (void)arg;
    switch (site) {
    case 2021: player_smashtrap(i); return 0;
    case 2022:                                                         /* oPlayer1 Step :1643 the ceiling trap */
        PG.plife -= 10;
        scrCreateBlood(i, PX(i).x, PX(i).y, 1);
        return 0;
    case 2033: player_lava(i); return 0;
    case 2046: player_idoltrap(i); return 0;
    case 2061:                                                         /* oPlayer1 Other_7 :133 (rOlmec's oXEnd) */
        if (PL.pExit == 4) {                                           /* xEND (Step_0 :746) */
            G.gameStart = 0;
            play_goto_room = R_rEnd;                                   /* the ending: src/front */
            return 0;
        }
        break;                                                         /* the front rooms' doors */
    }
    PUNTR(site);
    return 0;
}

int ptemple_world(int site, int i, int arg)
{
    (void)arg;
    switch (site) {
    case 1032: lava_item(i); return 0;
    case 1036: lava_jar(i); return 0;
    case 1039: lava_treasure(i); return 0;
    case 1040: pin_destroy(i); return 0;                               /* oDetritus Step :14 */
    case 1056: lava_rope(i); return 0;
    case 5012: lava_enemy(i, arg); return 0;
    }
    PUNTR(site);
    return 0;
}
