/* P7 package C (ice): docs/CONTENT.md §2. Translated statement for statement from refs/hd/src/objects/<obj>/<event>.gml
 * (line numbers in comments): oYeti, oYetiKing (and oIceBlock's events), oUFO, oUFOCrash, oAlien, oAlienEject,
 * oAlienBoss, oLaser, oLaserExplode, oLaserTrail, oPsychicWave, oPsychicCreate, oSpringTrap, oThinIce, oIce,
 * oIceBottom, oDarkFall, oFrozenCaveman, oDark, oRubbleDark, oRubbleDarkSmall, oMoai*, oCrown, oBarrierEmitter,
 * oBarrier, oAlienShip / oAlienShipFloor; the player's laser and psychic wave hits (oPlayer1 Step :1537-1582).
 * Defines the pcontent.h functions it translates (pice_*), overriding the weak defaults of pcontent.c.
 * GML keeps running after instance_destroy() inside an event: so does this code (RNG draws included).
 *
 * Variables without a field of their own (struct pin_ext / pin_en are shared): oYeti / oYetiKing whipped ->
 * PEN whipped; oYeti sightCounter -> PEN sightCounter; oYetiKing attackTimer -> PEN hit; oUFO shift / shiftToggle /
 * alerted -> PEN turnTimer / throwCount / startled; oAlienBoss psychicRecover -> PEN firing; oAlienEject dir ->
 * PE facing; oThinIce thickness, oDarkFall timeFall -> PE counter; oPsychicCreate dir -> PE direction.
 * type: the GML strings as enum ptype (T_YETI, T_YETIKING, T_UFO, T_ALIEN, T_ALIENBOSS): penemy.c's oEnemy / oItem
 * Step branches test them (a thrown item or enemy hitting a UFO or the alien boss: pen_hit_common).
 * oDrip (oIce / oIceBlock Destroy, oThinIce, oIceBottom) is package B's object: created here, run there. */
#include "pint.h"
#include "penemy.h"
#include "penhelp.h"                           /* X, Y, CP, eview, isCollisionSolid, ... */
#include "pcontent.h"
#include "pmath.h"
#include "pcol.h"
#include "../snd/sndgame.h"

static double gabs(double a) { return a < 0 ? -a : a; }
static void solid_create(struct pin *p)                            /* objects/oSolid/Create_0.gml */
{
    p->invincible = 0;
    p->shopWall = 0;
    p->type = T_NONE;
    p->cleanDeath = 0;
}

/* x > xview - l and x < xview + wview + rx and y > yview - l and y < yview + hview + ry */
static int vin(int i, int l, int rx, int ry)
{
    view_read();
    return PGTI(PX(i).x, PW.xview - l) && PLTI(PX(i).x, PW.xview + 320 + rx) && PGTI(PX(i).y, PW.yview - l) &&
           PLTI(PX(i).y, PW.yview + 240 + ry);
}

static int16_t with_buf[PIN_MAX];

/* x + 8 + rand(0, k) - rand(0, k2), y likewise (y's numbers first: arguments right to left) */
static int rubble8(int i, int obj, int k, int k2)
{
    struct pin *p = &PX(i);
    int ya = RAND(0, k), yb = RAND(0, k2);
    int xa = RAND(0, k), xb = RAND(0, k2);
    return pin_create(p->x + PI(8 + xa - xb), p->y + PI(8 + ya - yb), obj);
}

static void gold_drop(int i, int obj)
{
    int g = rubble8(i, obj, 4, 4);
    PE(&PX(g))->xVel = NI(rand_diff(0, 3));
    PE(&PX(g))->yVel = NI(RAND(2, 4) * 1);
}

/* if (countsAsKill) { global.enemyKills[k] (statistics), global.<kind> += 1, global.kills += 1 } */
static void kill_count(int i)
{
    if (!PEN(&PX(i))->countsAsKill) return;
    switch (PX(i).obj) {
    case OBJ_oYeti: PG.yetis += 1; break;
    case OBJ_oYetiKing: PG.yetikings += 1; break;
    case OBJ_oUFO: PG.ufos += 1; break;
    case OBJ_oAlien: PG.aliens += 1; break;
    case OBJ_oAlienBoss: PG.alienbosses += 1; break;
    }
    PG.kills += 1;
}

/* ---- oYeti ------------------------------------------------------------------------------------------------- */
static void yeti_create(int i)        /* objects/oYeti/Create_0.gml (oEnemy's ran: penemy.c pen_create) */
{
    struct pin *p = &PX(i);
    en_make_active(p);
    setCollisionBounds(i, 2, 0, sprw(i) - 2, sprh(i));
    PE(p)->xVel = N(2.5);
    pin_setispd(p, (img_t)0.5);
    p->type = T_YETI;
    PE(p)->hp = 5;
    p->invincible = 0;
    PE(p)->status = 0;
    PEN(p)->whipped = 0;
    PEN(p)->bounced = 0;
    PEN(p)->edead = 0;
    PE(p)->counter = 0;
    PEN(p)->sightCounter = 0;
    PE(p)->facing = E_RIGHT;
    PE(p)->colLeft = PE(p)->colRight = PE(p)->colBot = PE(p)->colTop = 0;
}

static void yeti_sight(int i)                                      /* :65-74, :115-124 */
{
    struct pin *p = &PX(i);
    if (PEN(p)->sightCounter > 0) PEN(p)->sightCounter -= 1;
    else {
        int s = pin_create(p->x, p->y, OBJ_oEnemySight);
        double dir = PE(&PX(i))->facing == E_LEFT ? 180 : 0;
        double h, v;
        PE(&PX(s))->direction = dir;
        pen_sight_speed(dir, &h, &v);                                  /* speed = 10 */
        PEN(&PX(s))->hspeed = h;
        PEN(&PX(s))->vspeed = v;
        PEN(&PX(s))->owner = (int16_t)instance_place_p(i, X(i), Y(i), OBJ_oYeti);
        PEN(&PX(i))->sightCounter = 5;
    }
}

static void yeti_step(int i)                                       /* objects/oYeti/Step_0.gml */
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
    if (PE(p)->colBot && PE(p)->status != E_STUNNED) PE(p)->yVel = 0;  /* :19 */
    if (PE(p)->status != E_DEAD && PE(p)->status != E_STUNNED && PE(p)->hp < 1) PE(p)->status = E_DEAD;
    if (PE(p)->status >= E_STUNNED) {                                  /* :26 */
        if (CP(X(i) + 8, Y(i) + 12, OBJ_oSolid)) {
            scrCreateBlood(i, (pos)(X(i) + 8), (pos)(Y(i) + 8), 3);
            snd_play(SND_xcavemandie);                                         /* :31 */
            pin_destroy(i);
        }
    } else if (!PE(p)->held && CP(X(i) + 8, Y(i) + 8, OBJ_oSolid)) {
        scrCreateBlood(i, (pos)(X(i) + 8), (pos)(Y(i) + 8), 3);
        snd_play(SND_xcavemandie);                                             /* :38 */
        pin_destroy(i);
    }
    p = &PX(i);
    if (PE(p)->status == 0) {                                          /* IDLE :42 */
        PEN(p)->bounced = 0;
        if (isCollisionBottom(i, 1) && (CPn(X(i) - 1, Y(i), OBJ_oSolid, i) || CPn(X(i) + 16, Y(i), OBJ_oSolid, i))) {
            PE(p)->yVel = N(-6);
            PE(p)->xVel = PE(p)->facing == E_LEFT ? N(-1) : N(1);
            PE(p)->counter -= 10;
        }
        if (NLT(PE(p)->yVel, N(0)) && isCollisionTop(i, 1)) PE(p)->yVel = 0;
        if (isCollisionBottom(i, 1) && PE(p)->counter > 0) PE(p)->counter -= 1;
        if (PE(p)->counter < 1) {
            PE(p)->facing = (int16_t)RAND(0, 1);
            PE(p)->status = 1;
        }
        yeti_sight(i);
    } else if (PE(p)->status == 1) {                                   /* WALK :76 */
        if (isCollisionLeft(i, 1) || isCollisionRight(i, 1))
            PE(p)->facing = PE(p)->facing == E_LEFT ? E_RIGHT : E_LEFT;
        if (!isCollisionBottom(i, 1)) {
        } else if (RAND(1, 100) == 1) {
            PE(p)->status = 0;
            PE(p)->counter = (int16_t)RAND(20, 50);
            PE(p)->xVel = 0;
        } else if (PE(p)->facing == E_LEFT) {
            PE(p)->xVel = N(-1.5);
            if (!CPn(X(i) - 1, Y(i) + 16, OBJ_oSolid, i)) {
                PE(p)->status = 0;
                PE(p)->counter = (int16_t)RAND(20, 50);
                PE(p)->xVel = 0;
            }
        } else if (PE(p)->facing == E_RIGHT) {
            PE(p)->xVel = N(1.5);
            if (!CPn(X(i) + 16, Y(i) + 16, OBJ_oSolid, i)) {
                PE(p)->status = 0;
                PE(p)->counter = (int16_t)RAND(20, 50);
                PE(p)->xVel = 0;
            }
        }
        yeti_sight(i);
    } else if (PE(p)->status == 2) {                                   /* ATTACK :126 */
        pin_setispd(p, 1);
        if (isCollisionLeft(i, 1) || isCollisionRight(i, 1))
            PE(p)->facing = PE(p)->facing == E_LEFT ? E_RIGHT : E_LEFT;
        PE(p)->xVel = PE(p)->facing == E_LEFT ? N(-3) : N(3);
    } else if (PE(p)->status == E_STUNNED) {                           /* :137 */
        if (NEQ(PE(p)->xVel, N(0)) && PE(p)->hp > 0) pin_set_sprite(i, GSPR_sYetiStunL);
        else if (PEN(p)->bounced) pin_set_sprite(i, NLT(PE(p)->yVel, N(0)) ? GSPR_sYetiBounceL : GSPR_sYetiFallL);
        else pin_set_sprite(i, NGT(NABS(PE(p)->xVel), N(0)) ? GSPR_sYetiDieLL : GSPR_sYetiDieLR);
        if (collision_point_any_at(i, 0, 0, OBJ_oSpikes) && PEN(p)->edead && NNE(PE(p)->yVel, N(0))) {   /* :151 */
            if (RAND(1, 8) == 1) pin_create(p->x, p->y, OBJ_oBlood);         /* other: self here */
            p = &PX(i);
        }
        if (PE(p)->colBot && !PEN(p)->bounced) {
            PEN(p)->bounced = 1;
            scrCreateBlood(i, (pos)(X(i) + 8), (pos)(Y(i) + 8), 1);
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
    } else if (PE(p)->status == E_DEAD) {                              /* :176 */
        if (!PEN(p)->edead) {
            kill_count(i);
            snd_play(SND_xcavemandie);                                         /* :186 */
            PEN(p)->edead = 1;
        }
        pin_set_sprite(i, GSPR_sYetiDeadL);
        if (NGT(PE(p)->xVel, N(0)) || NGT(PE(p)->yVel, N(0))) PE(p)->status = E_STUNNED;
    }
    if (PE(p)->status >= E_STUNNED) {                                  /* :195 */
        scrCheckCollisions(i);
        if (NEQ(PE(p)->xVel, N(0)) && NEQ(PE(p)->yVel, N(0)) && PE(p)->hp < 1) PE(p)->status = E_DEAD;
    }
    if (NGT(PE(p)->xVel, N(0))) PE(p)->xVel -= N(0.1);                 /* :204 */
    if (NLT(PE(p)->xVel, N(0))) PE(p)->xVel += N(0.1);
    if (NLT(NABS(PE(p)->xVel), N(0.5))) PE(p)->xVel = 0;
    if (PE(p)->status < E_STUNNED && PE(p)->status != 3)
        pin_set_sprite(i, NGT(NABS(PE(p)->xVel), N(0)) ? GSPR_sYetiRunLeft : GSPR_sYetiLeft);
    if (PE(p)->held) pin_set_sprite(i, PE(p)->hp > 0 ? GSPR_sYetiHeldL : GSPR_sYetiDHeldL);
}

/* objects/oYeti/Collision_oCharacter.gml */
static void yeti_hit_player(int i, int c)
{
    struct pin *p = &PX(i), *o = &PX(c);
    if (DGT(gabs(PTOD(o->x) - (X(i) + 8)), 8)) {
    } else if (!PL.dead && !PL.stunned && (PL.state == JUMPING || PL.state == FALLING) && DLT(PTOD(o->y), Y(i) + 5) &&
               !PL.swimming) {
        if (PE(p)->status < E_STUNNED) {
            PE(o)->yVel = N(-6) - NMUL(N(0.2), PE(o)->yVel);
            if (PG.hasSpikeShoes) {
                PE(p)->hp -= (int16_t)(3 * dceil(PL.fallTimer / 16.0));
                pin_create(o->x, o->y + PI(8), OBJ_oBlood);
                p = &PX(i);
                o = &PX(c);
            } else
                PE(p)->hp -= (int16_t)(1 * dceil(PL.fallTimer / 16.0));
            PL.fallTimer = 0;
            PE(p)->status = E_STUNNED;
            PE(p)->counter = PEN(p)->stunTime;
            PE(p)->yVel = N(-6);
            if (DLT(PTOD(o->x), X(i) + 8)) PE(p)->xVel += N(1);
            else PE(p)->xVel -= N(1);
            pin_setispd(p, (img_t)0.5);
            snd_play(SND_xhit);                                                /* :20 */
        }
    } else if (PL.invincible == 0 && PE(p)->status < E_STUNNED) {
        if (CP(X(i) + 8, Y(i) - 4, OBJ_oSolid)) {                      /* :25 */
            PL.blink = 30;
            PL.invincible = 30;
            PE(o)->xVel = DLT(PTOD(o->x), X(i)) ? N(-6) : N(6);
            pin_create(o->x, o->y, OBJ_oBlood);
            if (PG.plife > 0) PG.plife -= 1;
            snd_play(SND_xhurt);                                               /* :40 */
        } else if (PE(p)->status != 3) {                               /* THROW :42 */
            PE(p)->status = 3;
            PE(p)->xVel = 0;
            if (DGT(PTOD(o->x), X(i) + 8)) {
                PE(p)->facing = E_RIGHT;
                pin_set_sprite(i, GSPR_sYetiThrowL);
                pin_setxy(o, p->x, p->y);
                PE(o)->yVel = N(-6);
                PE(o)->xVel = N(6);
            } else {
                PE(p)->facing = E_LEFT;
                pin_set_sprite(i, GSPR_sYetiThrowL);
                pin_setxy(o, p->x + PI(16), p->y);
                PE(o)->yVel = N(-6);
                PE(o)->xVel = N(-6);
            }
            PL.stunned = 1;
            PL.bounced = 0;
            PL.wallHurt = 1;
            if (PL.holdItem != NOONE) {                                /* with other :69 */
                struct pin *h = &PX(PL.holdItem);
                if (h->type == T_GOLDIDOL) pin_sety(h, h->y - PI(8));
                scrDropItem(PE(o)->xVel, PE(o)->yVel);
            }
        }
    }
}

/* objects/oYeti/Collision_oWhip.gml (oWhip, oSlash, oMattockHit: other.puncture for the last two) and
   Collision_oWhipPre.gml (oWhipPre, oMachetePre, oMattockPre: other.type == "Machete" for oMachetePre) */
static void yeti_whipped(int i, int w)
{
    struct pin *p = &PX(i);
    int pre = obj_is(PX(w).obj, OBJ_oWhipPre);
    if (PEN(p)->whipped) return;
    if (pre ? whip_machete(w) : PX(w).obj != OBJ_oWhip) {                     /* :3 */
        PE(p)->hp -= (int16_t)whip_damage(w);                                  /* other.damage: 2 */
        PEN(p)->countsAsKill = 1;
        if (PEN(p)->bloodLeft > 0) {
            scrCreateBlood(i, (pos)(X(i) + sprw(i) / 2.0), (pos)(Y(i) + sprh(i) / 2.0), 1);
            p = &PX(i);
            if (PE(p)->hp < 0) PEN(p)->bloodLeft -= 1;
        }
        PE(p)->status = E_STUNNED;
        PE(p)->counter = PEN(p)->stunTime;
        PE(p)->yVel = N(-3);
        if (pre) PE(p)->xVel = DLT(X(w), X(i) + 8) ? N(2) : N(-2);
        else PE(p)->xVel = DLT(X(pl()) - 8, X(i)) ? N(2) : N(-2);
        pin_setispd(p, (img_t)0.5);
    } else {
        PE(p)->yVel = N(-2);
        if (pre) PE(p)->xVel = DLT(X(w), X(i)) ? N(1) : N(-1);
        else PE(p)->xVel = DLT(X(pl()) - 8, X(i)) ? N(1) : N(-1);
    }
    snd_play(SND_xhit);                                                        /* :18, :27 */
    PEN(p)->whipped = 1;
    PE(p)->alarm[0] = 10;
}

/* ---- oYetiKing ----------------------------------------------------------------------------------------------- */
static void yetiking_create(int i)    /* objects/oYetiKing/Create_0.gml (oEnemy's: pen_create) */
{
    struct pin *p = &PX(i);
    en_make_active(p);
    setCollisionBounds(i, 6, 0, 26, 32);
    PE(p)->xVel = N(2.5);
    pin_setispd(p, (img_t)0.25);
    p->type = T_YETIKING;
    PE(p)->hp = 30;
    p->invincible = 0;
    PE(p)->heavy = 1;
    PE(p)->status = 0;
    PE(p)->canPickUp = 0;
    PEN(p)->bounced = 0;
    PEN(p)->edead = 0;
    PEN(p)->whipped = 0;
    PE(p)->counter = 0;
    PEN(p)->hit = 0;                                               /* attackTimer */
    PE(p)->facing = E_RIGHT;
}

static void yetiking_step(int i)                                   /* objects/oYetiKing/Step_0.gml */
{
    struct pin *p, *q;
    int k;
    pen_parent_step(i);
    if (!vin(i, 36, 0, 0)) return;
    p = &PX(i);
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    PE(p)->yVel += PE(p)->myGrav;
    if (NGT(PE(p)->yVel, PEN(p)->yVelLimit)) PE(p)->yVel = PEN(p)->yVelLimit;
    if (CP(X(i) + 16, Y(i) + 16, OBJ_oSolid)) PE(p)->hp = 0;
    if (PE(p)->hp < 1) {                                               /* :15 */
        {
            int yr = RAND(0, 4), xr = RAND(0, 4);
            scrCreateBlood(i, (pos)(X(i) + 14 + xr), (pos)(Y(i) + 14 + yr), 3);
        }
        for (k = 0; k < 4; k++) {
            int yr = RAND(0, 6), xr = RAND(0, 4);
            pin_create(PX(i).x + PI(14 + xr), PX(i).y + PI(14 + yr), OBJ_oBone);
        }
        for (k = 0; k < 4; k++) {
            int obj;
            if (RAND(1, 12) == 1) {
                obj = pin_create(PX(i).x + PI(16), PX(i).y + PI(16), OBJ_oSpikeShoes);
                PE(&PX(obj))->cost = 0;
                PE(&PX(obj))->forSale = 0;
            } else if (RAND(1, 2) == 1)
                obj = pin_create(PX(i).x + PI(16), PX(i).y + PI(16), OBJ_oSapphireBig);
            else {
                obj = pin_create(PX(i).x + PI(16), PX(i).y + PI(16), OBJ_oRopePile);
                PE(&PX(obj))->cost = 0;
                PE(&PX(obj))->forSale = 0;
            }
            PE(&PX(obj))->xVel = NI(rand_diff(0, 3));
            PE(&PX(obj))->yVel = NI(-RAND(1, 2));
        }
        kill_count(i);
        pin_destroy(i);
    }
    p = &PX(i);
    if (isCollisionBottom(i, 1) && PE(p)->status != E_STUNNED) PE(p)->yVel = 0;   /* :49 */
    if (PEN(p)->hit > 0) PEN(p)->hit -= 1;
    if (PEN(p)->whipped > 0) PEN(p)->whipped -= 1;
    q = &PX(pl());
    if (PE(p)->status == 0) {                                          /* IDLE :54 */
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        if (PE(p)->counter <= 0) PE(p)->status = 1;
    } else if (PE(p)->status == 1) {                                   /* WALK :62 */
        double qx = PTOD(q->x), qy = PTOD(q->y);
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        if (PE(p)->facing == E_LEFT) {
            if (isCollisionLeft(i, 1) ||
                (DGT(qx, X(i) + 16) && DLT(gabs(qy - (Y(i) + 32)), 16) && PE(p)->counter == 0)) {
                pin_set_sprite(i, GSPR_sYetiKingTurnR);
                PE(p)->status = 2;
                PE(p)->counter = 30;
            } else if (DLT(qx, X(i) + 16) && DLT(gabs(qy - (Y(i) + 16)), 32) && PEN(p)->hit == 0) {
                PE(p)->status = 3;
                pin_set_sprite(i, GSPR_sYetiKingYellL);
                pin_setimg(p, 0);
                PE(p)->xVel = 0;
            } else
                PE(p)->xVel = N(-1);
        } else if (PE(p)->facing == E_RIGHT) {
            if (isCollisionRight(i, 1) ||
                (DLT(qx, X(i) + 16) && DLT(gabs(qy - (Y(i) + 32)), 16) && PE(p)->counter == 0)) {
                pin_set_sprite(i, GSPR_sYetiKingTurnL);
                PE(p)->status = 2;
                PE(p)->counter = 30;
            } else if (DGT(qx, X(i) + 16) && DLT(gabs(qy - (Y(i) + 16)), 32) && PEN(p)->hit == 0) {
                PE(p)->status = 3;
                pin_set_sprite(i, GSPR_sYetiKingYellR);
                pin_setimg(p, 0);
                PE(p)->xVel = 0;
            } else
                PE(p)->xVel = N(1);
        }
    } else if (PE(p)->status == 2) {                                   /* TURN :103 */
        PE(p)->xVel = 0;
    } else if (PE(p)->status == 3) {                                   /* ATTACK :107 */
        PE(p)->xVel = 0;
        pin_setispd(p, (img_t)0.5);
        PEN(p)->hit = 100;
        if (DGE(p->img, 7) && DLE(p->img, 12)) {
            int n;
            if (!snd_is_playing(SND_xyetiyell)) snd_play(SND_xyetiyell);          /* :114 */
            n = pw_with(OBJ_oIce, with_buf, PIN_MAX);                  /* with oIce :115 */
            for (k = 0; k < n; k++) {
                int c = with_buf[k], yk;
                if (!PX(c).alive) continue;
                yk = instance_first_p(OBJ_oYetiKing);
                if (yk != NOONE) {
                    if (RAND(1, 60) == 1 && DGT(gabs((X(yk) + 16) - (X(c) + 8)), 16) &&
                        pdist2_lt(pdist2(X(c), Y(c), X(yk), Y(yk)), 96)) {
                        pin_create(PX(c).x, PX(c).y, OBJ_oIceBlock);
                        pin_destroy(c);
                    }
                }
            }
            n = pw_with(OBJ_oThinIce, with_buf, PIN_MAX);              /* with oThinIce :128 */
            for (k = 0; k < n; k++)
                if (PX(with_buf[k]).alive) PE(&PX(with_buf[k]))->counter -= 2;
        }
    }
    p = &PX(i);
    if (isCollisionSolid(i)) pin_sety(p, p->y - PI(2));       /* :135 */
    if (PE(p)->facing == E_LEFT) {
        if (PE(p)->status == 1) pin_set_sprite(i, GSPR_sYetiKingWalkL);
        else if (PE(p)->status == 0) pin_set_sprite(i, GSPR_sYetiKingLeft);
    }
    if (PE(p)->facing == E_RIGHT) {
        if (PE(p)->status == 1) pin_set_sprite(i, GSPR_sYetiKingWalkR);
        else if (PE(p)->status == 0) pin_set_sprite(i, GSPR_sYetiKingRight);
    }
}

/* objects/oYetiKing/Collision_oCharacter.gml */
static void yetiking_hit_player(int i, int c)
{
    struct pin *p = &PX(i), *o = &PX(c);
    if (DGT(gabs(PTOD(o->x) - (X(i) + 16)), 16)) {
    } else if (!PL.dead && (PL.state == JUMPING || PL.state == FALLING) && DLT(PTOD(o->y), Y(i) + 8) && !PL.swimming) {
        PE(o)->yVel = N(-6) - NMUL(N(0.2), PE(o)->yVel);
        if (PG.hasSpikeShoes) {
            PE(p)->hp -= (int16_t)(3 * dceil(PL.fallTimer / 16.0));
            pin_create(o->x, o->y + PI(8), OBJ_oBlood);
        } else
            PE(p)->hp -= (int16_t)(1 * dceil(PL.fallTimer / 16.0));
        PL.fallTimer = 0;
        PEN(&PX(i))->countsAsKill = 1;
        pin_create(PX(i).x + PI(16), PX(i).y + PI(8), OBJ_oBlood);
        snd_play(SND_xhit);                                                    /* :14 */
    } else if (PL.invincible == 0) {
        PL.blink = 30;
        PL.invincible = 30;
        if (DLT(PTOD(o->y), Y(i))) PE(o)->yVel = N(-6);
        PE(o)->xVel = DLT(PTOD(o->x), X(i)) ? N(-6) : N(6);
        if (PG.plife > 0 && isRealLevel()) PG.plife -= 2;
        snd_play(SND_xhurt);                                                   /* :32 */
    }
}

/* ---- oUFO ---------------------------------------------------------------------------------------------------- */
static void ufo_create(int i, int shiftToggle)                     /* objects/oUFO/Create_0.gml */
{
    struct pin *p = &PX(i);
    pen_enemy_create(i);
    p->type = T_UFO;
    pin_setispd(p, (img_t)0.5);
    setCollisionBounds(i, 4, 2, 12, 14);
    PE(p)->xVel = 0;
    PE(p)->yVel = 0;
    PE(p)->xAcc = N(0.2);
    PE(p)->yAcc = N(0.2);
    PE(p)->hp = 1;
    p->invincible = 0;
    PE(p)->status = 0;
    PEN(p)->startled = 0;                                          /* alerted */
    PEN(p)->turnTimer = 0;                                         /* shift */
    PEN(p)->throwCount = (int16_t)shiftToggle;
}

static void ufo_step(int i)                                        /* objects/oUFO/Step_0.gml (no inherited) */
{
    struct pin *p = &PX(i);
    int c;
    double dist;
    if (!vin(i, 20, 4, 4)) return;
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    if (PE(p)->hp < 1) {                                               /* :6 */
        pin_create(p->x, p->y, OBJ_oUFOCrash);
        kill_count(i);
        pin_destroy(i);
    }
    p = &PX(i);
    c = instance_first_p(OBJ_oCharacter);
    dist = pdist2(X(i), Y(i), X(c), Y(c));                     /* (squared: pdist2_lt) */
    if (pdist2_lt(dist, 160) && !PEN(p)->startled) {
        PEN(p)->startled = 1;
        snd_play(SND_xalien);                                                  /* :23 */
    }
    if (PE(p)->status == 1) {                                          /* DESTROY :26 */
        PE(p)->yVel = 0;
        if (PEN(p)->throwCount == 0) {
            PE(p)->xVel = N(1);
            PEN(p)->turnTimer += 1;
            if (PEN(p)->turnTimer >= 64) PEN(p)->throwCount = 1;
        } else {
            PE(p)->xVel = N(-1);
            PEN(p)->turnTimer -= 1;
            if (PEN(p)->turnTimer <= -64) PEN(p)->throwCount = 0;
        }
        if (isCollisionLeft(i, 1)) {
            pin_setx(p, p->x + PI(1));
            PEN(p)->throwCount = 0;
            PEN(p)->turnTimer = -64;
        }
        if (isCollisionRight(i, 1)) {
            pin_setx(p, p->x - PI(1));
            PEN(p)->throwCount = 1;
            PEN(p)->turnTimer = 64;
        }
        if (DLT(gabs(X(c) - (X(i) + 8)), 8)) {
            PE(p)->status = 2;
            snd_play(SND_xlasercharge);                                        /* :65 */
        }
        if (DLT(Y(pl()), Y(i)) || DLT(Y(i), Y(pl()) - 96)) PE(p)->status = 0;
    } else if (PE(p)->status == 2) {                                   /* BLAST :70 */
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        pin_set_sprite(i, GSPR_sUFOBlast);
        if (DEQ(PX(i).img, 16)) {
            pin_create(PX(i).x + PI(8), PX(i).y + PI(16), OBJ_oLaser);
            snd_play(SND_xlaser);                                              /* :78 */
        }
    } else if (c != NOONE && !PL.swimming && !PL.dead) {               /* :81 */
        if (DGT(Y(i), Y(c) - 48)) {
            if (NEQ(PE(p)->yVel, N(2))) PE(p)->status = 1;
            PE(p)->yVel = N(-2);
        } else {
            if (NEQ(PE(p)->yVel, N(-2))) PE(p)->status = 1;
            PE(p)->yVel = N(2);
        }
        if (PEN(p)->throwCount == 0) {
            PE(p)->xVel = N(1);
            PEN(p)->turnTimer += 1;
            if (PEN(p)->turnTimer >= 32) {
                PEN(p)->throwCount = 1;
                if (DGT(X(c), X(i))) PEN(p)->turnTimer = 0;
            }
        } else {
            PE(p)->xVel = N(-1);
            PEN(p)->turnTimer -= 1;
            if (PEN(p)->turnTimer <= -32) {
                PEN(p)->throwCount = 0;
                if (DLT(X(c), X(i))) PEN(p)->turnTimer = 0;
            }
        }
        if (isCollisionLeft(i, 1)) {
            pin_setx(p, p->x + PI(1));
            PEN(p)->throwCount = 0;
            PEN(p)->turnTimer = -32;
        }
        if (isCollisionRight(i, 1)) {
            pin_setx(p, p->x - PI(1));
            PEN(p)->throwCount = 1;
            PEN(p)->turnTimer = 32;
        }
    }
}

/* ---- oUFOCrash ----------------------------------------------------------------------------------------------- */
static void ufocrash_step(int i)
{
    struct pin *p = &PX(i);
    pin_setx(p, PADDV(p->x, PE(p)->xVel));
    pin_sety(p, PADDV(p->y, PE(p)->yVel));
    if (NLT(PE(p)->yVel, N(6))) PE(p)->yVel += N(0.6);
}

static void ufocrash_alarm(int i)                                  /* objects/oUFOCrash/Alarm_0.gml */
{
    struct pin *p = &PX(i);
    int yr, xr;
    if (RAND(1, 2) == 1) {
        yr = RAND(0, 16); xr = RAND(0, 16);
        pin_create(p->x + PI(xr), p->y + PI(yr), OBJ_oFlameTrail);
    } else {
        yr = RAND(0, 16); xr = RAND(0, 16);
        pin_create(p->x + PI(xr), p->y + PI(yr), OBJ_oBurn);
    }
    PE(&PX(i))->alarm[0] = 3;
}

static void ufocrash_hit(int i)                                    /* Collision_oEnemy / Collision_oSolid */
{
    pin_create(PX(i).x + PI(8), PX(i).y + PI(8), OBJ_oExplosion);
    snd_play(SND_xexplosion);                                                  /* :2 */
    pin_destroy(i);
}

/* ---- oAlien -------------------------------------------------------------------------------------------------- */
static void alien_create(int i)                                    /* objects/oAlien/Create_0.gml */
{
    struct pin *p = &PX(i);
    pen_enemy_create(i);
    en_make_active(p);
    setCollisionBounds(i, 2, 6, 14, 16);
    PE(p)->xVel = N(2.5);
    pin_setispd(p, (img_t)0.5);
    p->type = T_ALIEN;
    PE(p)->hp = 1;
    p->invincible = 0;
    PE(p)->status = 0;
    PEN(p)->bounced = 0;
    PEN(p)->edead = 0;
    PE(p)->counter = 0;
    PE(p)->facing = E_RIGHT;
}

static void alien_step(int i)                                      /* objects/oAlien/Step_0.gml */
{
    struct pin *p;
    pen_parent_step(i);
    if (!vin(i, 20, 4, 4)) return;
    p = &PX(i);
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    if (CP(X(i) + 8, Y(i) + 8, OBJ_oSolid)) PE(p)->hp = 0;
    if (PE(p)->hp < 1) {
        scrCreateBlood(i, (pos)(X(i) + 8), (pos)(Y(i) + 8), 3);
        kill_count(i);
        pin_destroy(i);
    }
    p = &PX(i);
    PE(p)->yVel += N(0.6);
    if (isCollisionBottom(i, 1) && PE(p)->status != E_STUNNED) PE(p)->yVel = 0;
    if (PE(p)->status == 0) {
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        if (PE(p)->counter == 0) {
            PE(p)->facing = (int16_t)RAND(0, 1);
            PE(p)->status = 1;
        }
    } else if (PE(p)->status == 1) {
        double x = X(i), y = Y(i);
        if (isCollisionRight(i, 1)) PE(p)->facing = E_LEFT;
        if (isCollisionLeft(i, 1)) PE(p)->facing = E_RIGHT;
        if (PE(p)->facing == E_LEFT && !CPn(x - 1, y, OBJ_oSolid, i) && !CPn(x - 1, y + 16, OBJ_oSolid, i))
            PE(p)->facing = E_RIGHT;
        else if (PE(p)->facing == E_RIGHT && !CPn(x + 16, y, OBJ_oSolid, i) && !CPn(x + 16, y + 16, OBJ_oSolid, i))
            PE(p)->facing = E_LEFT;
        if ((!CPn(x - 1, y + 16, OBJ_oSolid, i) || CPn(x - 1, y, OBJ_oSolid, i)) &&
            (!CPn(x + 16, y + 16, OBJ_oSolid, i) || CPn(x + 16, y, OBJ_oSolid, i))) {
            PE(p)->facing = CPn(x - 1, y, OBJ_oSolid, i) ? E_RIGHT : E_LEFT;
            PE(p)->xVel = 0;
        } else if (PE(p)->facing == E_LEFT) PE(p)->xVel = N(-1);
        else PE(p)->xVel = N(1);
        if (RAND(1, 100) == 1) {
            PE(p)->status = 0;
            PE(p)->counter = (int16_t)RAND(20, 50);
            PE(p)->xVel = 0;
        }
    }
}

/* ---- oAlienEject (oDrawnSprite) ------------------------------------------------------------------------------ */
static void alieneject_create(int i)
{
    struct pin *p = &PX(i);
    p->type = T_NONE;
    en_make_active(p);
    setCollisionBounds(i, -4, -4, 4, 4);
    PE(p)->xVel = 0;
    PE(p)->yVel = N(-6);
    pin_setispd(p, (img_t)0.6);
    PE(p)->hp = 1;
    p->invincible = 0;
    PE(p)->status = 0;
    PE(p)->facing = (int16_t)RAND(0, 1);                           /* dir */
    PE(p)->counter = 0;
}

static void alieneject_step(int i)                                 /* objects/oAlienEject/Step_0.gml */
{
    struct pin *p = &PX(i);
    if (!vin(i, 20, 4, 16)) return;
    pin_setx(p, PADDV(p->x, PE(p)->xVel));
    pin_sety(p, PADDV(p->y, PE(p)->yVel));
    if (PE(p)->status == 0) {                                          /* EJECT */
        PE(p)->xVel = 0;
        PE(p)->yVel += N(0.5);
        if (NGE(PE(p)->yVel, N(0)) || isCollisionTop(i, 1)) {
            PE(p)->yVel = 0;
            PE(p)->status = 2;
            pin_set_sprite(i, GSPR_sAlienDeploy);
        }
        if (RAND(1, 5) == 1) {
            int ya = RAND(0, 3), yb = RAND(0, 3), xa = RAND(0, 3), xb = RAND(0, 3);
            pin_create(p->x + PI(xa - xb), p->y + PI(ya - yb), OBJ_oBurn);
        }
    } else if (PE(p)->status == 2) {                                   /* FLOAT */
        PE(p)->xVel = 0;
        PE(p)->yVel = N(2);
        if (CP(X(i), Y(i) + 6, OBJ_oSolid)) {
            pin_create(p->x - PI(8), p->y - PI(12), OBJ_oAlien);
            pin_destroy(i);
        } else if (PE(p)->facing == 0) {
            PE(p)->xVel = N(-1);
            if (CP(X(i) - 8, Y(i), OBJ_oSolid)) PE(p)->facing = 99;
        } else if (PE(p)->facing == 1) {
            PE(p)->xVel = N(1);
            if (CP(X(i) + 8, Y(i), OBJ_oSolid)) PE(p)->facing = 99;
        }
    }
}

/* objects/oAlienEject/Collision_oPlayer1.gml */
static void alieneject_hit_player(int i, int c)
{
    struct pin *o = &PX(c);
    int k;
    if (!PL.dead && (PL.state == JUMPING || PL.state == FALLING) && DLT(PTOD(o->y), Y(i) - 3) && !PL.swimming) {
        PE(o)->yVel = N(-6) - NMUL(N(0.2), PE(o)->yVel);
        PL.fallTimer = 0;
        snd_play(SND_xhit);                                                    /* :6 */
        for (k = 0; k < 3; k++) pin_create(PX(c).x + PI(8), PX(c).y + PI(8), OBJ_oBlood);
        PG.aliens += 1;                                                /* countsAsKill = true */
        PG.kills += 1;
        pin_destroy(i);
    } else if (PL.invincible == 0) {
        PL.blink = 30;
        PL.invincible = 30;
        if (DLT(PTOD(o->y), Y(i))) PE(o)->yVel = N(-6);
        PE(o)->xVel = DLT(PTOD(o->x), X(i)) ? N(-6) : N(6);
        if (PG.plife > 0) PG.plife -= 1;
        snd_play(SND_xhurt);                                                   /* :31 */
    }
}

/* ---- oAlienBoss ---------------------------------------------------------------------------------------------- */
static void alienboss_create(int i)                                /* objects/oAlienBoss/Create_0.gml */
{
    struct pin *p = &PX(i);
    pen_enemy_create(i);
    en_make_active(p);
    setCollisionBounds(i, 0, 0, 32, 32);
    PE(p)->xVel = N(2.5);
    pin_setispd(p, (img_t)0.25);
    p->type = T_ALIENBOSS;
    PE(p)->hp = 10;
    p->invincible = 0;
    PE(p)->status = 0;
    PE(p)->canPickUp = 0;
    PEN(p)->bounced = 0;
    PEN(p)->edead = 0;
    PE(p)->counter = 0;
    PE(p)->facing = E_LEFT;
    PEN(p)->firing = 100;                                          /* psychicRecover */
}

static void alienboss_step(int i)                                  /* objects/oAlienBoss/Step_0.gml */
{
    struct pin *p = &PX(i);
    int k;
    double dist;
    if (!vin(i, 36, 0, 0)) return;
    if (CP(X(i) + 8, Y(i) + 8, OBJ_oSolid)) PE(p)->hp = 0;
    if (PE(p)->hp < 1 && PE(p)->status != E_DEAD) {                    /* :9 */
        PE(p)->status = E_DEAD;
        pin_set_sprite(i, GSPR_sAlienBossDie);
        pin_setdepth(p, 101);
        for (k = 0; k < 4; k++) {
            int n = RAND(1, 3), gem;
            gem = pin_create(PX(i).x + PI(16), PX(i).y + PI(16),
                             n == 1 ? OBJ_oEmeraldBig : n == 2 ? OBJ_oSapphireBig : OBJ_oRubyBig);
            PE(&PX(gem))->xVel = NI(rand_diff(0, 3));
            PE(&PX(gem))->yVel = N(-2);
        }
        kill_count(i);
    }
    p = &PX(i);
    if (p->spr == GSPR_sAlienBossDie) {                                /* :34 */
        if (RAND(1, 2) == 1) {
            int r = RAND(14, 18);
            scrCreateBlood(i, (pos)(X(i) + 8), (pos)(Y(i) + r), 1);
            r = RAND(14, 18);
            pin_create(PX(i).x + PI(8), PX(i).y + PI(r), OBJ_oBlood);
        }
    }
    p = &PX(i);
    PE(p)->yVel += PE(p)->myGrav;
    if (NGT(PE(p)->yVel, PEN(p)->yVelLimit)) PE(p)->yVel = PEN(p)->yVelLimit;
    if (NGT(PE(p)->xVel, N(0))) PE(p)->xVel -= N(0.1);
    if (NLT(PE(p)->xVel, N(0))) PE(p)->xVel += N(0.1);
    if (NLT(NABS(PE(p)->xVel), N(0.5))) PE(p)->xVel = 0;
    if (isCollisionBottom(i, 1) && PE(p)->status != E_STUNNED) PE(p)->yVel = 0;
    if (PE(p)->status == 0) PE(p)->xVel = 0;
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    if (isCollisionSolid(i)) pin_sety(p, p->y - PI(2));
    dist = distance_to_object_p(i, OBJ_oPlayer1);
    if (PEN(p)->firing > 0) PEN(p)->firing -= 1;                       /* :65 */
    else if (DLT(dist, 96) && PE(p)->status != E_DEAD && !PL.dead && !PL.stunned && PL.invincible == 0) {
        for (k = 0; k < 6; k++) {
            int ya = RAND(0, 32), yb = RAND(0, 32), xa = RAND(0, 32), xb = RAND(0, 32);
            pin_create(PX(i).x + PI(16 + xa - xb), PX(i).y + PI(16 + ya - yb), OBJ_oPsychicCreate);
        }
        pin_create(PX(i).x + PI(16), PX(i).y + PI(16), OBJ_oPsychicWave);
        PEN(&PX(i))->firing = 100;
        snd_play(SND_xpsychic);                                                /* :74 */
    }
    p = &PX(i);
    if (p->spr != GSPR_sAlienBossHurt) pin_setispd(p, (img_t)0.25);
    if (PE(p)->status != E_DEAD && p->spr != GSPR_sAlienBossHurt) pin_set_sprite(i, GSPR_sAlienBoss);   /* :79-86 */
}

/* objects/oAlienBoss/Collision_oCharacter.gml */
static void alienboss_hit_player(int i, int c)
{
    struct pin *p = &PX(i), *o = &PX(c);
    if (!PL.dead && (PL.state == JUMPING || PL.state == FALLING) && DLT(PTOD(o->y), Y(i) + 8) && !PL.swimming) {
        PE(o)->yVel = N(-6) - NMUL(N(0.2), PE(o)->yVel);
        if (PG.hasSpikeShoes) {
            PE(p)->hp -= (int16_t)(3 * (PL.fallTimer / 16 + 1));
            pin_create(o->x, o->y + PI(8), OBJ_oBlood);            /* not bloodless */
        } else
            PE(p)->hp -= (int16_t)(1 * (PL.fallTimer / 16 + 1));
        PL.fallTimer = 0;
        PEN(&PX(i))->countsAsKill = 1;
        pin_create(PX(i).x + PI(16), PX(i).y + PI(8), OBJ_oBlood);
        snd_play(SND_xhit);                                                    /* :9 */
    } else if (PL.invincible == 0 && PE(p)->status != E_DEAD) {
        PL.blink = 30;
        PL.invincible = 30;
        if (DLT(PTOD(o->y), Y(i))) PE(o)->yVel = N(-6);
        PE(o)->xVel = DLT(PTOD(o->x), X(i)) ? N(-6) : N(6);
        if (PG.plife > 0) PG.plife -= 1;
        snd_play(SND_xhurt);                                                   /* :27 */
    }
}

/* ---- lasers, psychic waves -------------------------------------------------------------------------------------- */
static void laser_solid(int i, int s)                              /* objects/oLaser/Collision_oSolid.gml */
{
    if (!PX(s).invincible) {                                           /* with other */
        double x = X(s), y = Y(s);
        if (CP(x, y, OBJ_oGold)) {
            int g = instance_place_p(s, x, y, OBJ_oGold);
            if (g != NOONE) pin_destroy(g);
        }
        if (CP(x, y, OBJ_oGoldBig)) {
            int g = instance_place_p(s, x, y, OBJ_oGoldBig);
            if (g != NOONE) pin_destroy(g);
        }
        pin_destroy(s);
    }
    /* tile_layer_find / tile_delete: the background tiles (drawing only) */
    pin_create(PX(i).x, PX(i).y, OBJ_oLaserExplode);
    pin_destroy(i);
}

/* oLaser / oPsychicWave Collision_oEnemy and Collision_oDamsel: other.hp -= 3; other.xVel = rand(0,2) - rand(1,2);
   other.xVel = -1; other.yVel = -6 */
static void ray_hurt(int o)
{
    struct pin *q = &PX(o);
    int a = RAND(0, 2), b = RAND(1, 2);
    PE(q)->hp -= 3;
    PE(q)->xVel = NI(a - b);
    PE(q)->xVel = N(-1);
    PE(q)->yVel = N(-6);
}

static void psychic_dir_step(int i, double dir)
{
    struct pin *p = &PX(i);
    double si, co;                                             /* (psincos_cr: pcos_cr's, psin_cr's bits) */
    psincos_cr(degtorad_d(dir), &si, &co);
    pin_setx(p, (pos)((double)p->x + 2 * co));
    pin_sety(p, (pos)((double)p->y + -2 * si));
}

/* oPlayer1 Step :1537 (the laser) and :1564 (the psychic wave) */
int pice_player(int site, int i, int arg)
{
    struct pin *p = &PX(i);
    double x = PTOD(p->x), y = PTOD(p->y);
    int obj;
    (void)arg;
    if (site == 2018) {
        obj = instance_nearest_p(x, y, OBJ_oLaser);
        if (obj != NOONE) {
            if (PG.plife > 0) PG.plife -= 3;
            PE(p)->xVel = DLT(X(obj), x) ? N(2) : N(-2);
            PE(p)->yVel = N(-4);
            scrCreateBlood(i, p->x, p->y, 3);
            pin_create(PX(obj).x, PX(obj).y, OBJ_oLaserExplode);       /* with obj */
            pin_destroy(obj);
            snd_play(SND_xhurt);                                               /* :1558 */
            PL.stunned = 1;
            PL.stunTimer = 20;
        }
        return 1;
    }
    if (site == 2019) {
        obj = instance_nearest_p(x, y, OBJ_oPsychicWave);
        if (obj != NOONE) {
            if (PG.plife > 0) PG.plife -= 1;
            PE(p)->xVel = DLT(X(obj), x) ? N(2) : N(-2);
            PE(p)->yVel = N(-4);
            snd_play(SND_xhurt);                                               /* :1578 */
            PL.stunned = 1;
            PL.stunTimer = 40;
        }
        return 1;
    }
    PUNTR(site);
    return 0;
}

/* ---- terrain: oThinIce, oIce, oIceBottom, oFrozenCaveman, oDark, oDarkFall, oRubbleDark ------------------------- */
static void thinice_step(int i)                                    /* objects/oThinIce/Step_0.gml */
{
    struct pin *p = &PX(i);
    int t;
    if (collision_rect_p(X(i), Y(i) - 1, X(i) + 16, Y(i) + 1, OBJ_oPlayer1, 0, NOONE) != NOONE) {
        PE(p)->counter -= 2;
        if (RAND(1, 100) == 1) {
            int xr = RAND(0, 16);
            pin_create(p->x + PI(xr), p->y + PI(9), OBJ_oDrip);
        }
    }
    p = &PX(i);
    t = PE(p)->counter;
    if (t > 50) pin_set_sprite(i, GSPR_sThinIce1);
    else if (t > 40) pin_set_sprite(i, GSPR_sThinIce2);
    else if (t > 30) pin_set_sprite(i, GSPR_sThinIce3);
    else if (t > 20) pin_set_sprite(i, GSPR_sThinIce4);
    else if (t > 10) pin_set_sprite(i, GSPR_sThinIce5);
    else if (t > 0) pin_set_sprite(i, GSPR_sThinIce6);
    else pin_destroy(i);
}

static void three_drips(int i)                                     /* oIce / oIceBlock Destroy: repeat(3) */
{
    int k;
    for (k = 0; k < 3; k++) {
        int yr = RAND(0, 16), xr = RAND(0, 16);
        pin_create(PX(i).x + PI(xr), PX(i).y + PI(yr), OBJ_oDrip);
    }
}

static void ice_destroy(int i)                                     /* objects/oIce/Destroy_0.gml (no inherited) */
{
    struct pin *p = &PX(i);
    if (!p->cleanDeath && !G.cleanSolids) {
        three_drips(i);
        if (CP(X(i) + 8, Y(i) + 8, OBJ_oFrozenCaveman)) {
            int f = instance_place_p(i, X(i) + 8, Y(i) + 8, OBJ_oFrozenCaveman);
            if (f != NOONE) pin_destroy(f);
        }
    }
}

static void dark_rubble3(int i)                                    /* oDark / oAlienShip(Floor) Destroy */
{
    rubble8(i, OBJ_oRubbleDark, 8, -8);
    rubble8(i, OBJ_oRubbleDarkSmall, 8, -8);
    rubble8(i, OBJ_oRubbleDarkSmall, 8, -8);
}

static void dark_destroy(int i)                                    /* objects/oDark/Destroy_0.gml */
{
    struct pin *p = &PX(i);
    int k;
    destroy_solid(i);
    if (!p->cleanDeath && !G.cleanSolids) {
        dark_rubble3(i);
        if (p->spr == GSPR_sDarkGold)
            for (k = 0; k < 3; k++) gold_drop(i, OBJ_oGoldChunk);
        if (p->spr == GSPR_sDarkGoldBig) {
            for (k = 0; k < 3; k++) gold_drop(i, OBJ_oGoldChunk);
            gold_drop(i, OBJ_oGoldNugget);
        }
    }
}

/* isCollisionCharacterTop(1) of the solid s: the character's bottom line against s itself */
static int char_on_top(int s)
{
    double lb, tb, rb, bb;
    int c = pl();
    if (c == NOONE) return 0;
    calcBounds(c, &lb, &tb, &rb, &bb);
    {   /* as penemy.c line_solid: the object's query, then s's own test */
        int q = pcol_query(PX(s).obj);
        if (q < 0) return 0;
        if (q == 2) pcol_touch(s);
    }
    return pw_test_line(s, dround(lb), dround(bb + 1 - 1), dround(rb - 1), dround(bb + 1 - 1), 1);
}

static void darkfall_step(int i)                                   /* objects/oDarkFall/Step_0.gml */
{
    struct pin *p = &PX(i);
    if (char_on_top(i)) {
        PE(p)->counter -= 1;                                           /* timeFall */
        if (PE(p)->counter <= 0) PE(p)->yAcc = PE(p)->grav;
    } else if (PE(p)->counter < 20)                                    /* timeFallMax */
        PE(p)->counter += 1;
    if (NGT(PE(p)->yVel, N(10))) PE(p)->yVel = N(10);
}

static void darkfall_solid(int i)                                  /* objects/oDarkFall/Collision_oSolid.gml */
{
    int k;
    snd_play(SND_xbreak);                                                      /* :1 */
    pin_create(PX(i).x + PI(8), PX(i).y + PI(8), OBJ_oSmokePuff);
    for (k = 0; k < 3; k++) {
        int yr = RAND(2, 14), xr = RAND(2, 14), obj;
        obj = pin_create(PX(i).x + PI(xr), PX(i).y + PI(yr), OBJ_oRubbleDark);
        PE(&PX(obj))->xVel = NI(rand_diff(1, 3));
        PE(&PX(obj))->yVel = NI(-RAND(0, 3));
    }
    pin_destroy(i);
}

/* objects/oRubblePiece/Step_0.gml (oRubbleDarkSmall) */
static void rubblepiece_step(int i)
{
    struct pin *p = &PX(i);
    pos px, py;
    pin_setx(p, PADDV(p->x, PE(p)->xVel));
    pin_sety(p, PADDV(p->y, PE(p)->yVel));
    PE(p)->yVel += PE(p)->yAcc;
    px = p->x;
    py = p->y;
    if (collision_point_any_at(i, 0, 0, OBJ_oWaterSwim)) pswamp_world(1041, i, 0);
    else if (collision_point_any_at(i, 0, 0, OBJ_oLava)) pin_destroy(i);
    if (collision_point_any_at(i, 0, 0, OBJ_oSolid)) pin_destroy(i);   /* (x, y: px, py; site 1041 sets yVel only) */
    view_read();
    if (PLTI(px, PW.xview - 32) || PGTI(px, PW.xview + 320 + 32) || PLTI(py, PW.yview - 32) || PGTI(py, PW.yview + 240 + 32))
        pin_destroy(i);
}

/* ---- oSpringTrap ------------------------------------------------------------------------------------------------ */
static void spring(int i)
{
    pin_set_sprite(i, GSPR_sSpringTrapSprung);
    snd_play(SND_xboing);                                                      /* Collision_* :4 */
    PE(&PX(i))->status = 1;
}

static void springtrap_collision(int i, int o)
{
    struct pin *p = &PX(i), *q = &PX(o);
    if (PE(p)->status != 0 || PE(p)->counter != 0) return;
    if (q->obj == OBJ_oPlayer1) {                                      /* Collision_oPlayer1 */
        if (DLT(gabs(X(o) - (X(i) + 8)), 6) && PL.state <= 13 && q->spr != GSPR_sPExit && q->spr != GSPR_sDamselExit) {
            spring(i);
            pin_sety(q, q->y - PI(16));
            PE(q)->yVel = N(-16);
            PE(&PX(i))->counter = 10;
        }
    } else if (obj_is(q->obj, OBJ_oEnemy)) {                           /* Collision_oEnemy */
        if (DLT(gabs(X(o) - X(i)), 6) && q->obj != OBJ_oUFO && q->obj != OBJ_oBat) {   /* not other.flying */
            spring(i);
            pin_sety(q, q->y - PI(16));
            PE(q)->yVel = N(-8);
            if (PE(q)->facing == 0) PE(q)->xVel -= N(1);
            else PE(q)->xVel += N(1);
            PE(&PX(i))->counter = 10;
        }
    } else if (obj_is(q->obj, OBJ_oItem)) {                            /* Collision_oItem */
        if (DLT(gabs(X(o) - (X(i) + 8)), 6) && !PE(q)->held && PE(q)->active) {
            spring(i);
            pin_sety(q, q->y - PI(24));
            PE(q)->yVel = N(-8);
            if (q->type == T_DAMSEL) {
                if (PE(q)->facing == LEFT) PE(q)->xVel -= N(1);
                else PE(q)->xVel += N(1);
            }
            PE(&PX(i))->counter = 10;
        }
    }
}

/* ---- oBarrierEmitter / oBarrier ------------------------------------------------------------------------------- */
static void barrieremitter_destroy(int i)                          /* objects/oBarrierEmitter/Destroy_0.gml */
{
    int k, n;
    for (k = 0; k < 6; k++) {
        int yr = RAND(0, 14), xr = RAND(0, 14), obj;
        obj = pin_create(PX(i).x + PI(2 + xr), PX(i).y + PI(2 + yr), OBJ_oFlareSpark);
        PE(&PX(obj))->yVel = NI(RAND(1, 3));
    }
    scrShake(10);
    snd_play(SND_xsmallexplode);                                               /* :7 */
    n = pw_with(OBJ_oBarrier, with_buf, PIN_MAX);
    for (k = 0; k < n; k++)
        if (PX(with_buf[k]).alive) pin_destroy(with_buf[k]);
}

static void barrier_collision(int i, int o)
{
    struct pin *q = &PX(o);
    if (obj_is(q->obj, OBJ_oCharacter)) {                              /* Collision_oCharacter */
        if (PL.invincible == 0) {
            PL.blink = 30;
            PL.invincible = 30;
            PE(q)->yVel = N(-2);
            PE(q)->xVel = DLT(X(o), X(i)) ? N(-6) : N(6);
            if (PG.plife > 0) PG.plife -= 1;
            snd_play(SND_xhurt);                                               /* :12 */
        }
    } else if (q->obj == OBJ_oBullet) {                                /* Collision_oBullet */
        pin_create(q->x, q->y, OBJ_oSmokePuff);
        snd_play(SND_xhit);                                                    /* :4 */
        pin_destroy(o);
    } else if (q->obj == OBJ_oWebBall) {
        pin_destroy(o);
    } else if (obj_is(q->obj, OBJ_oItem)) {                            /* Collision_oItem */
        if (q->type == T_BOMB) {
            pin_set_sprite(o, GSPR_sBombArmed);
            pin_setispd(q, 1);
            PE(q)->alarm[1] = (int16_t)RAND(4, 8);
        }
        PE(q)->xVel = NI(-RAND(4, 6));
        PE(q)->yVel = N(-2);
        if (PE(q)->held && pl() != NOONE) PL.holdItem = NOONE;
    }
}

/* ---- the dispatch ------------------------------------------------------------------------------------------------ */
static int ev_create_ice(int i, int fromgen)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oYeti: yeti_create(i); return 1;
    case OBJ_oYetiKing: yetiking_create(i); return 1;
    case OBJ_oUFO:
        ufo_create(i, fromgen ? (play_gen_inst ? play_gen_inst->shifttoggle : 0) : RAND(0, 1));
        return 1;
    case OBJ_oAlien: alien_create(i); return 1;
    case OBJ_oAlienBoss: alienboss_create(i); return 1;
    case OBJ_oAlienEject: alieneject_create(i); return 1;
    case OBJ_oUFOCrash:
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        PE(p)->alarm[0] = 3;
        return 1;
    case OBJ_oLaser:
        p->type = T_NONE;
        PE(p)->yVel = 0;
        PE(p)->yAcc = N(0.6);
        PE(p)->alarm[0] = 1;
        return 1;
    case OBJ_oLaserExplode:
        p->type = T_NONE;
        pin_setispd(p, (img_t)0.8);
        snd_play(SND_xsmallexplode);                                           /* :3 */
        return 1;
    case OBJ_oLaserTrail: p->type = T_NONE; pin_setispd(p, (img_t)0.8); return 1;
    case OBJ_oPsychicWave:
        p->type = T_NONE;
        PE(p)->yVel = 0;
        PE(p)->yAcc = N(0.6);
        pin_setispd(p, (img_t)0.25);
        return 1;
    case OBJ_oPsychicCreate: {
        int b = instance_first_p(OBJ_oAlienBoss);
        p->type = T_NONE;
        PE(p)->yVel = 0;
        PE(p)->yAcc = N(0.6);
        pin_setispd(p, (img_t)0.4);
        PE(p)->grav = 0;
        PE(p)->direction = point_direction_d(X(i), Y(i), X(b) + 16, Y(b) + 16);
        return 1;
    }
    case OBJ_oSpringTrap:
        PE(p)->status = 0;
        PE(p)->counter = 0;
        return 1;
    case OBJ_oThinIce:
        if (!fromgen) solid_create(p);
        PE(p)->counter = 60;                                           /* thickness */
        return 1;
    case OBJ_oIceBlock:                                                /* objects/oIceBlock/Create_0.gml */
        if (!fromgen) solid_create(p);
        PE(p)->xVel = 0;                                               /* oMoveableSolid */
        PE(p)->yVel = 0;
        PE(p)->myGrav = N(0.6);
        p->invincible = 0;
        setCollisionBounds(i, 0, 0, 16, 16);
        if (G.cityOfGold) pin_set_sprite(i, GSPR_sGoldBlock);
        p->cleanDeath = 0;
        return 1;
    case OBJ_oIceBottom:
        if (!fromgen) PE(p)->alarm[0] = (int16_t)RAND(20, 400);
        return 1;
    case OBJ_oDarkFall:                                                /* objects/oDarkFall/Create_0.gml */
        if (!fromgen) solid_create(p);
        PE(p)->xVel = 0;                                               /* oMovingSolid */
        PE(p)->yVel = 0;
        PE(p)->myGrav = N(0.6);
        en_make_active(p);
        setCollisionBounds(i, 0, 0, 16, 8);
        p->invincible = 0;
        PE(p)->grav = NI(1);
        PE(p)->counter = 20;                                           /* timeFall */
        return 1;
    case OBJ_oRubbleDark:
        p->type = T_NONE;
        PE(p)->yVel = 0;
        PE(p)->yAcc = N(0.6);
        return 1;
    case OBJ_oMoai: case OBJ_oMoai2: case OBJ_oMoai3: case OBJ_oMoaiInside:
        if (!fromgen) solid_create(p);
        p->invincible = 1;
        return 1;
    case OBJ_oCrown:                                                   /* objects/oCrown/Create_0.gml */
        create_item(p);
        p->type = T_CROWN;
        en_make_active(p);
        setCollisionBounds(i, -6, -6, 6, 8);
        PE(p)->cost = 999999;
        return 1;
    case OBJ_oBarrierEmitter:
        pin_setispd(p, (img_t)0.5);
        if (!fromgen && !instance_exists_p(OBJ_oBarrier)) pin_create(p->x, p->y + PI(16), OBJ_oBarrier);
        return 1;
    case OBJ_oDark: case OBJ_oIce: case OBJ_oAlienShip: case OBJ_oAlienShipFloor:
        if (fromgen) return 1;                                         /* the generator ran them */
        return 0;
    }
    return 0;
}

static int ev_step_ice(int i)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oYeti: yeti_step(i); return 1;
    case OBJ_oYetiKing: yetiking_step(i); return 1;
    case OBJ_oUFO: ufo_step(i); return 1;
    case OBJ_oAlien: alien_step(i); return 1;
    case OBJ_oAlienBoss: alienboss_step(i); return 1;
    case OBJ_oAlienEject: alieneject_step(i); return 1;
    case OBJ_oUFOCrash: ufocrash_step(i); return 1;
    case OBJ_oLaser:                                                   /* objects/oLaser/Step_0.gml */
        pin_sety(p, PADDV(p->y, PE(p)->yVel));
        PE(p)->yVel += PE(p)->yAcc;
        if (NGT(PE(p)->yVel, N(4))) PE(p)->yVel = 0;
        return 1;
    case OBJ_oPsychicWave: {                                           /* objects/oPsychicWave/Step_0.gml */
        int c = instance_first_p(OBJ_oCharacter);
        psychic_dir_step(i, point_direction_d(X(i), Y(i), X(c), Y(c)));
        return 1;
    }
    case OBJ_oPsychicCreate: psychic_dir_step(i, PE(p)->direction); return 1;
    case OBJ_oSpringTrap:                                              /* objects/oSpringTrap/Step_0.gml */
        if (vin(i, 16, 0, 0)) {
            if (PE(p)->counter > 0) PE(p)->counter -= 1;
            if (!CP(X(i), Y(i) + 16, OBJ_oSolid)) pin_destroy(i);
        }
        return 1;
    case OBJ_oThinIce: thinice_step(i); return 1;
    case OBJ_oIceBlock:                                                /* objects/oIceBlock/Step_0.gml */
        if (collision_point_any_at(i, 8, 16, OBJ_oLava) && !CP(X(i) + 8, Y(i) + 17, OBJ_oSolid)) {
            PE(p)->yVel = 0;
            PE(p)->myGrav = 0;
            pin_sety(p, PADDV(p->y, N(0.05)));
        }
        if (DGT(Y(i), 576)) pin_destroy(i);
        return 1;
    case OBJ_oIceBottom:
        if (!CP(X(i) + 8, Y(i) - 1, OBJ_oSolid)) pin_destroy(i);
        return 1;
    case OBJ_oFrozenCaveman:
        if (!CP(X(i), Y(i), OBJ_oIce)) pin_destroy(i);
        return 1;
    case OBJ_oDarkFall: darkfall_step(i); return 1;
    case OBJ_oRubbleDark:                                              /* objects/oRubbleDark/Step_0.gml */
        pin_sety(p, PADDV(p->y, PE(p)->yVel));
        PE(p)->yVel += PE(p)->yAcc;
        if (CP(X(i), Y(i), OBJ_oBrick) || CP(X(i), Y(i), OBJ_oBlock)) pin_destroy(i);
        return 1;
    case OBJ_oRubbleDarkSmall: rubblepiece_step(i); return 1;
    case OBJ_oBarrierEmitter:
        if (vin(i, 8, 8, 8) && !CP(X(i), Y(i) - 16, OBJ_oSolid)) pin_destroy(i);
        return 1;
    }
    return 0;
}

static int ev_alarm_ice(int i, int a)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oYeti: if (a == 0) PEN(p)->whipped = 0; return 1;
    case OBJ_oUFOCrash: if (a == 0) ufocrash_alarm(i); return 1;
    case OBJ_oLaser:
        if (a == 0) {
            pin_create(p->x, p->y, OBJ_oLaserTrail);
            PE(&PX(i))->alarm[0] = 1;
        }
        return 1;
    case OBJ_oIceBottom:
        if (a == 0) {
            pin_create(p->x + PI(8), p->y + PI(4), OBJ_oDrip);
            PE(&PX(i))->alarm[0] = (int16_t)RAND(20, 400);
        }
        return 1;
    }
    return 0;
}

static int ev_animend_ice(int i)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oYeti:
        if (p->spr == GSPR_sYetiThrowL) {
            PE(p)->status = 0;
            pin_set_sprite(i, GSPR_sYetiLeft);
        }
        return 1;
    case OBJ_oYetiKing:
        if (p->spr == GSPR_sYetiKingTurnR) { PE(p)->facing = E_RIGHT; PE(p)->status = 1; }
        if (p->spr == GSPR_sYetiKingTurnL) { PE(p)->facing = E_LEFT; PE(p)->status = 1; }
        if (p->spr == GSPR_sYetiKingYellL || p->spr == GSPR_sYetiKingYellR) {
            PE(p)->status = 0;
            PE(p)->counter = 30;
            pin_setispd(p, (img_t)0.25);
        }
        return 1;
    case OBJ_oUFO:
        if (p->spr == GSPR_sUFOBlast) {
            PE(p)->status = 1;
            pin_set_sprite(i, GSPR_sUFO);
        }
        return 1;
    case OBJ_oAlienBoss:
        if (p->spr == GSPR_sAlienBossDie) {
            int n, k;
            pin_set_sprite(i, GSPR_sAlienBossDead);
            n = pw_with(OBJ_oBarrierEmitter, with_buf, PIN_MAX);
            for (k = 0; k < n; k++)
                if (PX(with_buf[k]).alive) pin_destroy(with_buf[k]);
        }
        if (PX(i).spr == GSPR_sAlienBossHurt) pin_set_sprite(i, GSPR_sAlienBoss);
        return 1;
    case OBJ_oAlienEject:
        if (p->spr == GSPR_sAlienDeploy) pin_set_sprite(i, GSPR_sAlienParachute);
        return 1;
    case OBJ_oSpringTrap:
        if (PE(p)->status == 1) {
            PE(p)->status = 0;
            pin_set_sprite(i, GSPR_sSpringTrap);
        }
        return 1;
    case OBJ_oLaserExplode: case OBJ_oLaserTrail: case OBJ_oPsychicWave: case OBJ_oPsychicCreate:
        pin_destroy(i);
        return 1;
    }
    return 0;
}

static int ev_collision_ice(int i, int o)
{
    struct pin *p = &PX(i);
    int oo = PX(o).obj;
    switch (p->obj) {
    case OBJ_oYeti:
        if (obj_is(oo, OBJ_oCharacter)) yeti_hit_player(i, o);
        else yeti_whipped(i, o);
        return 1;
    case OBJ_oYetiKing:
        if (obj_is(oo, OBJ_oCharacter)) yetiking_hit_player(i, o);
        else if (oo == OBJ_oWhip) {                                    /* Collision_oWhip (puncture false) */
            if (PEN(p)->whipped == 0 && DLT(Y(o), Y(i) + 12)) {
                snd_play(SND_xhit);                                            /* :13 */
                PEN(p)->whipped = 10;
            }
        } else if (oo == OBJ_oWhipPre) enemy_whipped(i, o);
        else PUNTR(7001);
        return 1;
    case OBJ_oUFO: case OBJ_oAlien:
        if (obj_is(oo, OBJ_oCharacter)) enemy_hit_player(i, o);
        else enemy_whipped(i, o);
        return 1;
    case OBJ_oAlienBoss:
        if (obj_is(oo, OBJ_oCharacter)) alienboss_hit_player(i, o);
        else enemy_whipped(i, o);
        return 1;
    case OBJ_oAlienEject:
        if (obj_is(oo, OBJ_oPlayer1)) alieneject_hit_player(i, o);
        else {                                                         /* Collision_oWeb */
            pin_create(p->x - PI(8), p->y - PI(12), OBJ_oAlien);
            pin_destroy(i);
        }
        return 1;
    case OBJ_oUFOCrash: ufocrash_hit(i); return 1;
    case OBJ_oLaser:
        if (obj_is(oo, OBJ_oSolid)) laser_solid(i, o);
        else if (obj_is(oo, OBJ_oEnemy)) {
            if (PX(o).type != T_UFO && PX(o).invincible == 0) {
                ray_hurt(o);
                pin_create(PX(i).x, PX(i).y, OBJ_oLaserExplode);
                pin_destroy(i);
            }
        } else if (!PX(o).invincible) {                                /* Collision_oDamsel */
            ray_hurt(o);
            PE(&PX(i))->status = 2;
            pin_create(PX(i).x, PX(i).y, OBJ_oLaserExplode);
            pin_destroy(i);
        }
        return 1;
    case OBJ_oPsychicWave:
        if (obj_is(oo, OBJ_oEnemy)) {
            if (PX(o).type != T_ALIENBOSS && PX(o).invincible == 0) ray_hurt(o);
        } else {                                                       /* Collision_oDamsel */
            if (!PX(o).invincible) {
                ray_hurt(o);
                PE(&PX(i))->status = 2;
            }
            pin_destroy(i);
        }
        return 1;
    case OBJ_oSpringTrap: springtrap_collision(i, o); return 1;
    case OBJ_oDarkFall: darkfall_solid(i); return 1;
    case OBJ_oBarrierEmitter:
        if (oo == OBJ_oBullet) pin_destroy(i);
        else if (NGT(NABS(PE(&PX(o))->xVel), N(2)) || NGT(NABS(PE(&PX(o))->yVel), N(2))) pin_destroy(i);
        return 1;
    case OBJ_oBarrier: barrier_collision(i, o); return 1;
    case OBJ_oExplosion:                                               /* oExplosion/Collision_oBarrierEmitter.gml */
        if (oo != OBJ_oBarrierEmitter) return 0;
        pin_destroy(o);
        return 1;
    }
    return 0;
}

static int ev_destroy_ice(int i)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oIce: ice_destroy(i); return 1;
    case OBJ_oIceBlock:
        if (!p->cleanDeath && !G.cleanSolids) three_drips(i);
        return 1;
    case OBJ_oDark: dark_destroy(i); return 1;
    case OBJ_oAlienShip: case OBJ_oAlienShipFloor:
        destroy_solid(i);
        dark_rubble3(i);
        return 1;
    case OBJ_oThinIce: case OBJ_oDarkFall: case OBJ_oMoai: case OBJ_oMoai2: case OBJ_oMoai3: case OBJ_oMoaiInside:
        destroy_solid(i);                                              /* oSolid's */
        return 1;
    case OBJ_oFrozenCaveman:                                           /* objects/oFrozenCaveman/Destroy_0.gml */
        if (!G.cleanSolids) {
            int e = pin_create(p->x, p->y, OBJ_oCaveman);
            struct pin *q = &PX(e);
            q->invincible = 20;
            PE(q)->status = E_STUNNED;
            PE(q)->counter = PEN(q)->stunTime;
        }
        return 1;
    case OBJ_oBarrierEmitter: barrieremitter_destroy(i); return 1;
    }
    return 0;
}

/* gameStepEvent's moving solids (penemy.c pen_moving_solids): oDarkFall's viscidTop (Create :2) */
int pice_msolid(int s) { return PX(s).obj == OBJ_oDarkFall ? 1 : -1; }

int pice_ev(int ev, int i, int arg)
{
    switch (ev) {
    case FEV_CREATE: return ev_create_ice(i, arg);
    case FEV_STEP: return ev_step_ice(i);
    case FEV_ALARM: return ev_alarm_ice(i, arg);
    case FEV_ANIMEND: return ev_animend_ice(i);
    case FEV_COLLISION: return ev_collision_ice(i, arg);
    case FEV_DESTROY: return ev_destroy_ice(i);
    case FEV_OUTSIDE:                                                  /* Other_0: action_kill_object */
        switch (PX(i).obj) {
        case OBJ_oLaser: case OBJ_oPsychicWave: case OBJ_oRubbleDark: case OBJ_oRubbleDarkSmall: case OBJ_oAlienEject:
            pin_destroy(i);
            return 1;
        }
        return 0;
    }
    return 0;
}
