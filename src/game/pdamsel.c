/* P5: the damsel (refs/hd/src/objects/oDamsel/<event>.gml, line numbers in comments), the kissing damsel of the
 * transition rooms (oDamselKiss, oHeart) and the thrown items hitting a damsel (oItem Step :340-370).
 * global.isDamsel (playing as the damsel) is not modelled: the player is the explorer (gen G.isDamsel 0).
 */
#include "pint.h"
#include "penemy.h"
#include "../snd/sndgame.h"                     /* the GML sound calls (src/snd) */
#include "pcontent.h"                            /* P7 content packages (docs/CONTENT.md) */

enum { D_IDLE = 0, D_RUN = 1, D_THROWN = 2, D_YELL = 3, D_EXIT = 4, D_SLAVE = 5, D_KISS = 6, D_DEAD = 99 };

static double X(int i) { return PTOD(PX(i).x); }
static double Y(int i) { return PTOD(PX(i).y); }
static int CP(double x, double y, int obj) { return collision_point_p(x, y, obj, 0, NOONE) != NOONE; }

/* objects/oDamsel/Create_0.gml (after oItem's) */
int pdam_create(int i, int fromgen)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oDamsel: {
        int st = PE(p)->status;                                            /* the generator's (SLAVE in a parlour) */
        create_item(p);
        p->type = T_DAMSEL;
        PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;
        setCollisionBounds(i, -4, -4, 4, 8);
        p->ispd = (img_t)0.5;
        PE(p)->trigger = 1;
        PEN(p)->startled = 0;
        p->invincible = 0;
        PEN(p)->swimming = 0;
        PE(p)->heavy = 1;
        PE(p)->cost = (10000 + 5000 * (G.currLevel - 2)) * 3;                  /* getKissValue() * 3 */
        PE(p)->hp = 4;
        PEN(p)->bloodLeft = 4;
        PEN(p)->sacCount = 20;
        PEN(p)->edead = 0;
        PE(p)->status = D_IDLE;
        PEN(p)->hit = 0;
        PE(p)->facing = LEFT;
        PEN(p)->bounced = 0;
        PEN(p)->burning = 0;
        PE(p)->counter = 200;
        PEN(p)->stunMax = 120;
        PEN(p)->bombID = NOONE;
        PE(p)->cimg = 0;
        if (fromgen) PE(p)->status = (int16_t)st;
        return 1;
    }
    case OBJ_oDamselKiss:                                              /* objects/oDamselKiss/Create_0.gml */
        p->type = T_NONE;
        p->ispd = (img_t)0.5;
        PE(p)->trigger = 0;                                                /* kissed */
        PE(p)->status = D_IDLE;
        return 1;
    case OBJ_oHeart:                                                   /* objects/oHeart/Create_0.gml */
        p->type = T_NONE;
        PE(p)->alarm[0] = 30;
        return 1;
    }
    return 0;
}

/* objects/oDamsel/Step_0.gml */
static void damsel_step(int i)
{
    struct pin *p;
    item_step(i);                                                      /* action_inherited */
    p = &PX(i);
    if (PE(p)->active && CP(X(i), Y(i), OBJ_oExit) && PE(p)->hp > 0 && !PE(p)->held && PE(p)->status != D_THROWN) {   /* :2 */
        int door = instance_place_p(i, X(i), Y(i), OBJ_oExit);
        PG.damsels += 1;
        PG.xdamsels += 1;
        pin_setx(p, PX(door).x + PI(8));
        pin_sety(p, PX(door).y + PI(8));
        pin_set_sprite(i, GSPR_sDamselExit2);
        PE(p)->status = D_EXIT;
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        snd_play(SND_xsteps);                                                  /* :19 */
        pin_setdepth(p, 1000);
        PE(p)->active = 0;
        PE(p)->canPickUp = 0;
    }
    if (p->spr == GSPR_sDamselExit2 || p->spr == GSPR_sPExit) {        /* :25 */
        pin_setdepth(p, 1000);
        p->invincible = 1;
    }
    if (PEN(p)->hit > 0) PEN(p)->hit -= 1;
    if (collision_point_p(X(i), Y(i), OBJ_oWaterSwim, 1, i) != NOONE) {   /* :33 */
        if (!PEN(p)->swimming) {
            pin_create(p->x, p->y, OBJ_oSplash);
            p = &PX(i);
            PEN(p)->swimming = 1;
            snd_play(SND_xsplash);                                             /* :39 */
        }
        PE(p)->myGrav = N(0.2);
    } else {
        PEN(p)->swimming = 0;
        PE(p)->myGrav = N(0.6);
    }
    if (PE(p)->cost > 0 && PE(p)->hp <= 0) scrShopkeeperAnger(i, 3);              /* :50 */
    p = &PX(i);
    if (!p->invincible) {                                              /* :55 */
        if (CP(X(i), Y(i), OBJ_oSolid) && !PE(p)->held) {
            scrCreateBlood(i, p->x, p->y, 3);
            p = &PX(i);
            if (PE(p)->hp > 0) {
                PG.damselsKilled += 1;
                PG.kills += 1;
            }
            snd_play(SND_xdamsel);                                             /* :66 */
            pin_destroy(i);
        }
        if (PEN(p)->burning > 0) {
            if (RAND(1, 5) == 1) {
                int yy = RAND(4, 12), xx = RAND(4, 12);
                pin_create(p->x + PI(xx), p->y + PI(yy), OBJ_oBurn);
                p = &PX(i);
            }
            PEN(p)->burning -= 1;
        }
        if (CP(X(i), Y(i) + 6, OBJ_oLava)) PUNTR(6010);
        if (CP(X(i), Y(i) + 6, OBJ_oSpikes) && NGT(PE(p)->yVel, N(2))) {  /* :92 */
            int obj = instance_place_p(i, X(i), Y(i) + 6, OBJ_oSpikes);
            if (obj != NOONE) pin_set_sprite(obj, GSPR_sSpikesBlood);
            if (PE(p)->hp > 0) {
                scrCreateBlood(i, p->x, p->y, 3);
                p = &PX(i);
            }
            pin_set_sprite(i, GSPR_sDamselDieL);
            PE(p)->status = D_DEAD;
            PEN(p)->edead = 1;
            if (PE(p)->hp > 0) PE(p)->hp = 0;
            PE(p)->myGrav = 0;
            PE(p)->xVel = 0;
            PE(p)->yVel = N(0.2);
        }
        if (collision_rect_p(X(i) - 3, Y(i) - 3, X(i) + 3, Y(i) + 3, OBJ_oSpearsLeft, 0, NOONE) != NOONE &&
            (PE(p)->status != D_THROWN || isCollisionBottom(i, 1)))
            PUNTR(6011);
    }
    if (!PE(p)->held && NGT(PE(p)->yVel, N(2)) && PE(p)->status != D_THROWN) {    /* :144 */
        PE(p)->status = D_THROWN;
        snd_play(SND_xdamsel);                                                 /* :148 */
    }
    if (PE(p)->held)
        PE(p)->facing = (int16_t)PL.facing;                               /* oCharacter.facing */
    else if (PE(p)->status == D_SLAVE) {
        PE(p)->facing = DLT(PTOD(PX(PL.idx).x), X(i)) ? LEFT : RIGHT;
        pin_set_sprite(i, GSPR_sDamselLeft);
    } else if (PE(p)->status == D_KISS) {
        if (p->spr == GSPR_sDamselKissL && DEQ(p->img, 7)) {
            if (PE(p)->facing == LEFT) pin_create(p->x - PI(8), p->y - PI(8), OBJ_oHeart);
            else pin_create(p->x + PI(8), p->y - PI(8), OBJ_oHeart);
            snd_play(SND_xkiss);                                               /* :175 */
        }
    } else if (PE(p)->status == D_IDLE) {
        pin_set_sprite(i, GSPR_sDamselLeft);
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        else {
            PE(p)->status = D_YELL;
            pin_set_sprite(i, GSPR_sDamselYellL);
            snd_play(SND_xdamsel);                                             /* :203 */
        }
    } else if (PE(p)->status == D_YELL) {
        if (DEQ(p->img, 4)) pin_create(p->x, p->y - PI(16), OBJ_oYellHelp);
    } else if (PE(p)->status == D_RUN) {
        p->ispd = (img_t)0.8;
        pin_set_sprite(i, GSPR_sDamselRunL);
        if (PE(p)->facing == LEFT && isCollisionLeft(i, 2)) PE(p)->facing = RIGHT;
        if (PE(p)->facing == RIGHT && isCollisionRight(i, 2)) PE(p)->facing = LEFT;
        PE(p)->xVel = PE(p)->facing == LEFT ? N(-1.5) : N(1.5);
    } else if (PE(p)->status == D_THROWN) {                                /* :236 */
        PEN(p)->startled = 1;                                               /* global.damselsGrabbed: statistics */
        if (NEQ(PE(p)->xVel, N(0))) pin_set_sprite(i, GSPR_sDamselStunL);
        else if (PEN(p)->bounced) pin_set_sprite(i, NLT(PE(p)->yVel, N(0)) ? GSPR_sDamselBounceL : GSPR_sDamselFallL);
        else pin_set_sprite(i, NLT(PE(p)->xVel, N(0)) ? GSPR_sDamselDieLL : GSPR_sDamselDieLR);
        if (isCollisionBottom(i, 1) && !PEN(p)->bounced) PEN(p)->bounced = 1;
        if (isCollisionBottom(i, 2) ||
            collision_rect_p(X(i) - 4, Y(i) - 6, X(i) + 4, Y(i) + 8, OBJ_oWeb, 0, NOONE) != NOONE) {
            if (!PEN(p)->edead) {
                if (PE(p)->counter > 0) PE(p)->counter -= 1;
                else PE(p)->status = D_RUN;
            }
            if (PE(p)->hp <= 0) {
                pin_set_sprite(i, GSPR_sDamselDieL);
                PE(p)->status = D_DEAD;
                if (!PEN(p)->edead) {
                    PEN(p)->edead = 1;
                    PG.damselsKilled += 1;
                    PG.kills += 1;
                }
            }
        }
    }
    if (PE(p)->status == D_THROWN || PE(p)->status == D_DEAD) {                /* :311 sacrifice */
        if (!PE(p)->held && NEQ(PE(p)->xVel, N(0)) && NEQ(PE(p)->yVel, N(0))) {
            if (CP(X(i), Y(i) + 8, OBJ_oSacAltarLeft)) pitems_world(6012, i, 0);
        } else
            PEN(p)->sacCount = 20;
    }
}

int pdam_step(int i)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oDamsel: damsel_step(i); return 1;
    case OBJ_oDamselKiss:                                              /* objects/oDamselKiss/Step_0.gml */
        if (p->spr == GSPR_sDamselKissL && DEQ(p->img, 7)) {
            pin_create(p->x - PI(8), p->y - PI(8), OBJ_oHeart);
            snd_play(SND_xkiss);                                               /* :5 */
        }
        return 1;
    case OBJ_oHeart: pin_sety(p, p->y - (PI(1))); return 1;                          /* objects/oHeart/Step_0.gml */
    }
    return 0;
}

int pdam_alarm(int i, int a)
{
    if (PX(i).obj == OBJ_oHeart) {
        if (a == 0) pin_destroy(i);
        return 1;
    }
    return 0;
}

/* objects/oDamsel/Other_7.gml, oDamselKiss/Other_7.gml */
int pdam_animend(int i)
{
    struct pin *p = &PX(i);
    if (p->obj == OBJ_oDamsel) {
        if (PE(p)->status == D_YELL) {
            PE(p)->status = D_IDLE;
            PE(p)->counter = 200;
        }
        if (PE(p)->status == D_KISS) PE(p)->status = D_SLAVE;
        if (p->spr == GSPR_sDamselExit2 || p->spr == GSPR_sPExit) pin_destroy(i);
        return 1;
    }
    if (p->obj == OBJ_oDamselKiss) {
        if (p->spr == GSPR_sDamselKissL) {
            PE(p)->trigger = 1;                                            /* kissed */
            pin_set_sprite(i, GSPR_sDamselLeft);
        }
        return 1;
    }
    return 0;
}

/* objects/oDamsel/Collision_oWhip.gml (and oWhipPre; other.type "Machete": not the whip) */
int pdam_collision(int self, int other)
{
    struct pin *p = &PX(self);
    if (p->obj != OBJ_oDamsel) return 0;
    (void)other;
    if (PE(p)->status != D_THROWN && (PE(p)->status < D_EXIT || PE(p)->status == D_SLAVE || PE(p)->status == D_KISS) && PEN(p)->hit == 0) {
        PE(p)->yVel = N(-2);
        PEN(p)->hit = 10;
        snd_play(SND_xhit);                                                    /* :17 */
        snd_play(SND_xdamsel);
        if (PE(p)->forSale) scrShopkeeperAnger(self, 3);
    }
    return 1;
}

/* objects/oDamsel/Draw_0.gml: image_xscale from facing, the price tag's frame */
int pdam_draw(int i)
{
    struct pin *p = &PX(i);
    if (p->obj != OBJ_oDamsel) return p->obj == OBJ_oDamselKiss || p->obj == OBJ_oHeart;
    pin_setxscale(p, PE(p)->facing == RIGHT ? -1 : 1);
    if (PE(p)->cost > 0) {
        PE(p)->cimg += 1;
        if (PE(p)->cimg > 9) PE(p)->cimg = 0;
    }
    return 1;
}

/* objects/oDamsel/Destroy_0.gml: oItem's (held: oPlayer1.holdItem = 0), then the sticky bomb's enemyID */
int pdam_destroy(int i)
{
    struct pin *p = &PX(i);
    if (p->obj != OBJ_oDamsel) return 0;
    if (PE(p)->held) PL.holdItem = NOONE;
    if (PEN(p)->bombID != NOONE) PE(&PX(PEN(p)->bombID))->enemyID = NOONE;
    return 1;
}

/* oDamselKiss's Room End (objects/oDamselKiss/Other_5.gml): global.plife += 1 */
void pdam_room_end(void)
{
    int k;
    for (k = 0; k < PW.n; k++)
        if (PX(k).alive && PX(k).obj == OBJ_oDamselKiss) PG.plife += 1;
}

/* oItem Step :340-370: a fast item hits a damsel */
void pen_item_hit_damsel(int it)
{
    double x = X(it), y = Y(it);
    int obj = collision_rect_p(x - 2, y - 2, x + 2, y + 2, OBJ_oDamsel, 0, it);
    struct pin *o;
    if (obj == NOONE) return;
    o = &PX(obj);
    if (!o->invincible && PE(o)->status != 99 && PEN(o)->hit == 0) {
        if (!(PE(o)->held && PE(&PX(it))->safe)) {
            scrCreateBlood(it, o->x, o->y, 1);
            o = &PX(obj);
            if (PE(o)->held) {
                PE(o)->held = 0;
                PL.holdItem = NOONE;
                PL.pickupItemType = T_NONE;
            }
            PE(o)->hp -= 1;
            PE(o)->yVel = N(-6);
            PE(o)->status = D_THROWN;
            PE(o)->counter = 120;
            PEN(o)->hit = 10;
            PE(o)->xVel = NMUL(PE(&PX(it))->xVel, N(0.3));
            if (PX(it).type == T_ARROW || PX(it).type == T_FISHBONE) pin_destroy(it);
            snd_play(SND_xhit);                                                /* oItem Step :367 */
            if (PE(o)->forSale) scrShopkeeperAnger(it, 3);
        }
    }
}

/* oJar / oSkull Step :148-170: a fast jar hits a damsel (1: the jar breaks) */
int pdam_jar_hit(int jar)
{
    double x = X(jar), y = Y(jar);
    struct pin *j = &PX(jar);
    if (collision_rect_p(x - 3, y - 3, x + 3, y + 3, OBJ_oDamsel, 0, NOONE) != NOONE &&
        (NGT(NABS(PE(j)->xVel), N(2)) || NGT(NABS(PE(j)->yVel), N(2)))) {
        int e = instance_nearest_p(x, y, OBJ_oDamsel);
        struct pin *o = &PX(e);
        if (!o->invincible && (NGT(NABS(PE(j)->xVel), N(1)) || NGT(NABS(PE(j)->yVel), N(1)))) {
            pin_create(j->x, j->y, OBJ_oBlood);
            o = &PX(e);
            j = &PX(jar);
        }
        if (PE(o)->held) PE(o)->held = 0;
        PL.holdItem = NOONE;
        PE(o)->hp -= 1;
        PE(o)->yVel = N(-6);
        PE(o)->status = D_THROWN;
        PE(o)->counter = 120;
        PE(o)->xVel = NMUL(PE(j)->xVel, N(0.3));
        return 1;
    }
    return 0;
}
