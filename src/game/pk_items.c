/* P7 package E (items): docs/CONTENT.md §2. Defines the pcontent.h functions it translates (pitems_*), overriding
 * the weak defaults of pcontent.c: the player's item branches (scrUseItem's weapons, scrFireBow, scrStealItem's
 * equipment, the jetpack, parachute, mitt, ankh, kapala, ball and chain), the weapons' objects (oSlash,
 * oMattockHit, oMachetePre / oMattockPre, oMattockHead), the bow's arrows, the flares and the flare crate, and the
 * mines leftovers (dice, Kali's altar, jars' spiders, sticky bombs, the locked chest).
 * refs/hd/src/objects/<obj>/<event>.gml and scripts/<name>/<name>.gml, line numbers in comments. GML argument order:
 * last argument first; binary operators left to right. */
#include "pint.h"
#include "pcontent.h"
#include "pmath.h"
#include "penemy.h"
#include "../snd/sndgame.h"                     /* the GML sound calls (src/snd) */
#include "pmsg.h"                                /* the HUD messages (trMessages) */

#define ME (PX(PL.idx))
static double X(int i) { return PTOD(PX(i).x); }
static double Y(int i) { return PTOD(PX(i).y); }
static int CP(double x, double y, int obj) { return collision_point_p(x, y, obj, 0, NOONE) != NOONE; }

/* oPlayer1 Create :51-53 */
enum { FIRING_PISTOL_MAX = 20, FIRING_SHOTGUN_MAX = 40 };


/* scripts/scrPlayerIsDucking */
static int scrPlayerIsDucking(int i)
{
    int s = PX(i).spr;
    return s == GSPR_sDuckLeft || s == GSPR_sCrawlLeft || s == GSPR_sDamselDuckL || s == GSPR_sDamselCrawlL ||
           s == GSPR_sTunnelDuckL || s == GSPR_sTunnelCrawlL;
}

static void attack_sprite(int i)
{
    if (G.isTunnelMan) pin_set_sprite(i, GSPR_sTunnelAttackL);
    else if (G.isDamsel) pin_set_sprite(i, GSPR_sDamselAttackL);
    else pin_set_sprite(i, GSPR_sAttackLeft);
}

/* ---- scrUseItem: the weapons (:122-593) ---------------------------------------------------------------------- */
/* "if (kDown) { if (scrPlayerIsDucking()) { drop the weapon } }" */
static void use_drop(int i)
{
    int h = PL.holdItem;
    struct pin *o = &PX(h), *p = &PX(i);
    PE(o)->held = 0;
    PE(o)->safe = 1;
    PE(o)->alarm[2] = 10;
    if (PL.facing == LEFT) {
        if (PE(o)->heavy) PE(o)->xVel = N(-4) + PE(p)->xVel;
        else PE(o)->xVel = N(-8) + PE(p)->xVel;
    } else if (PL.facing == RIGHT) {
        if (PE(o)->heavy) PE(o)->xVel = N(4) + PE(p)->xVel;
        else PE(o)->xVel = N(8) + PE(p)->xVel;
    }
    PE(o)->xVel = NMUL(PE(o)->xVel, N(0.4));
    PE(o)->yVel = N(0.5);
    PL.holdItem = NOONE;
}

/* the pistol (:246-275), the shotgun (:557-592), the web cannon (:362-397) */
static void use_gun(int i, int type)
{
    struct pin *p = &PX(i);
    int left = PL.facing == LEFT, k, n = type == T_SHOTGUN ? 6 : 1;
    int dx = type == T_WEBCANNON ? 18 : 12;
    if (!(PL.facing == LEFT || PL.facing == RIGHT) || PL.firing != 0) return;
    pin_create(p->x + PI(left ? -dx : dx), p->y + PI(type == T_WEBCANNON ? 0 : 1),
               left ? OBJ_oShotgunBlastLeft : OBJ_oShotgunBlastRight);
    for (k = 0; k < n; k++) {
        int obj = pin_create(PX(i).x + PI(left ? -dx : dx), PX(i).y - PI(2),
                             type == T_WEBCANNON ? OBJ_oWebBall : OBJ_oBullet);
        struct pin *b = &PX(obj);
        p = &PX(i);
        if (left) {
            PE(b)->xVel = NI(-1 * RAND(6, 8)) + PE(p)->xVel;
            if (NGE(PE(b)->xVel, N(-6))) PE(b)->xVel = N(-6);
        } else {
            PE(b)->xVel = NI(RAND(6, 8)) + PE(p)->xVel;
            if (NLT(PE(b)->xVel, N(6))) PE(b)->xVel = N(6);
        }
        if (type == T_SHOTGUN) {
            double a = prandom(1);
            double c = prandom(1);
            PE(b)->yVel = ND(a - c);
        } else
            PE(b)->yVel = 0;
    }
    p = &PX(i);
    if (PL.state != HANGING && PL.state != CLIMBING) {
        PE(p)->yVel -= N(1);
        if (type == T_SHOTGUN) {
            if (left) PE(p)->xVel += N(3);
            else PE(p)->xVel -= N(3);
        } else {
            if (left) PE(p)->xVel += N(1);
            else PE(p)->xVel -= N(1);
        }
    }
    snd_play(SND_xshotgun);                                                    /* :258 / :377 / :572 */
    PL.firing = type == T_SHOTGUN ? FIRING_SHOTGUN_MAX : FIRING_PISTOL_MAX;
}

/* the sceptre (:306-331) */
static void use_sceptre(int i)
{
    int left = PL.facing == LEFT, k, obj;
    if (!(PL.facing == LEFT || PL.facing == RIGHT) || PL.firing != 0) return;
    for (k = 0; k < 3; k++) {
        obj = pin_create(PX(i).x + PI(left ? -12 : 12), PX(i).y + PI(4), OBJ_oPsychicCreateP);
        PE(&PX(obj))->xVel = NI(left ? -RAND(1, 3) : RAND(1, 3));
        { double r = prandom(2); PE(&PX(obj))->yVel = ND(-r); }
    }
    obj = pin_create(PX(i).x + PI(left ? -12 : 12), PX(i).y - PI(2), OBJ_oPsychicWaveP);
    PE(&PX(obj))->xVel = N(left ? -6 : 6);
    snd_play(SND_xpsychic);                                                    /* :316 / :329 */
    PL.firing = FIRING_PISTOL_MAX;
}

/* the teleporter (:426-488); tx, ty: oPlayer1's instance variables */
static double tele_tx, tele_ty;
static void use_teleporter(int i)
{
    struct pin *p = &PX(i);
    int n, k, obj;
    if (PL.kUp) {
        tele_tx = X(i);
        tele_ty = Y(i) - (16 * RAND(4, 8));
        while (tele_ty < 16) tele_ty += 16;
    } else if (!scrPlayerIsDucking(i)) {
        if (PL.facing == LEFT && PL.firing == 0) {
            tele_tx = X(i) - (16 * RAND(4, 8));
            tele_ty = Y(i);
            if (tele_tx < 8) tele_tx = 8;
        } else if (PL.facing == RIGHT && PL.firing == 0) {
            tele_tx = X(i) + (16 * RAND(4, 8));
            tele_ty = Y(i);
            if (tele_tx > PW.room_w - 8) tele_tx = PW.room_w - 8;
        }
    }
    n = 0;
    while (collision_rect_p(tele_tx - 4, tele_ty - 4, tele_tx + 4, tele_ty + 4, OBJ_oSolid, 0, NOONE) != NOONE &&
           n < 3 && tele_ty > 16) {
        tele_ty -= 16;
        n += 1;
    }
    for (k = 0; k < 3; k++) {
        int ya = RAND(0, 8);
        int xa = RAND(0, 8);
        pin_create(P(X(i) - 4 + xa), P(Y(i) - 4 + ya), OBJ_oFlareSpark);
    }
    p = &PX(i);
    if (Y(i) < 8) pin_sety(p, PI(8));
    pin_setxy(p, P(tele_tx), P(tele_ty));
    {
        int16_t w[64];
        int m = pw_with(OBJ_oBall, w, 64);
        for (k = 0; k < m; k++) if (PX(w[k]).alive) pin_setxy(&PX(w[k]), PX(PL.idx).x, PX(PL.idx).y);
        m = pw_with(OBJ_oChain, w, 64);
        for (k = 0; k < m; k++) if (PX(w[k]).alive) pin_setxy(&PX(w[k]), PX(PL.idx).x, PX(PL.idx).y);
    }
    obj = instance_place_p(i, X(i), Y(i), OBJ_oEnemy);
    if (obj != NOONE) {
        scrCreateBlood(obj, PX(PL.idx).x, PX(PL.idx).y, 3);
        PE(&PX(obj))->hp -= 99;
        pin_destroy(obj);
    }
    snd_play(SND_xteleport);                                                   /* :483 */
    PL.state = 16;
}

/* scrUseItem :122-593 for a held weapon; then its tail (:679-684) */
static void use_weapon(int i)
{
    int h = PL.holdItem, t = PX(h).type;
    struct pin *p = &PX(i);
    switch (t) {
    case T_MACHETE: case T_MATTOCK:
        if (PL.kDown && !PL.whipping && scrPlayerIsDucking(i)) use_drop(i);
        if (!scrPlayerIsDucking(i) && !PL.whipping && (t == T_MACHETE || platformCharacterIs(ON_GROUND))) {
            p->ispd = (img_t)(t == T_MACHETE ? 1 : 0.2);
            attack_sprite(i);
            pin_setimg(p, 0);
            PL.whipping = 1;
            if (t == T_MATTOCK) PL.cantJump = 20;
            pin_setvisible(&PX(PL.holdItem), 0);
        }
        break;
    case T_PISTOL: case T_SHOTGUN: case T_WEBCANNON: case T_SCEPTRE:
        if (PL.kDown && scrPlayerIsDucking(i)) use_drop(i);
        if (!scrPlayerIsDucking(i)) {
            if (t == T_SCEPTRE) use_sceptre(i);
            else use_gun(i, t);
        }
        break;
    case T_TELEPORTER:
        if (PL.kDown) {
            if (scrPlayerIsDucking(i)) use_drop(i);
        } else
            use_teleporter(i);
        break;
    case T_BOW:
        if (PL.kDown) {
            if (scrPlayerIsDucking(i)) use_drop(i);
        } else if (!scrPlayerIsDucking(i) && PL.firing == 0 && !PL.bowArmed && PG.arrows > 0) {
            PL.bowArmed = 1;
            snd_play(SND_xbowpull);                                            /* :521 */
        } else if (PG.arrows <= 0)
            pmsg_str("I'M OUT OF ARROWS!", "", 80);                            /* :525 */
        break;
    }
    p = &PX(i);
    if (PL.kDown && PL.holdItem != NOONE) {                                    /* :679 */
        pin_setx(&PX(PL.holdItem), p->x);
        pin_sety(&PX(PL.holdItem), p->y);
    }
    if (PL.holdItem == NOONE) PL.pickupItemType = T_NONE;
}

/* oPlayer1 Step :417-479: the attack sprite past frame 4 with a machete / mattock held */
static void weapon_hit(int i)
{
    struct pin *p = &PX(i);
    int t = PX(PL.holdItem).type, left = PL.facing == LEFT, obj;
    if (t == T_MACHETE) {
        obj = pin_create(p->x + PI(left ? -16 : 16), p->y, OBJ_oSlash);
        pin_set_sprite(obj, left ? GSPR_sSlashLeft : GSPR_sSlashRight);
    } else {
        obj = pin_create(p->x + PI(left ? -16 : 16), p->y, OBJ_oMattockHit);
        pin_set_sprite(obj, left ? GSPR_sMattockHitL : GSPR_sMattockHitR);
    }
    snd_play(SND_xwhip);
}

/* oPlayer1 Step :484-512: the swing's start (frames 0-1) */
static void weapon_pre(int i)
{
    struct pin *p = &PX(i);
    int t = PX(PL.holdItem).type, obj;
    if (instance_number_p(OBJ_oMachetePre) != 0) return;
    if (PL.facing == LEFT) {
        obj = pin_create(p->x + PI(16), p->y, t == T_MACHETE ? OBJ_oMachetePre : OBJ_oMattockPre);
        pin_set_sprite(obj, t == T_MACHETE ? GSPR_sMachetePreL : GSPR_sMattockPreL);
    } else if (PL.facing == RIGHT) {
        obj = pin_create(p->x - PI(16), p->y, t == T_MACHETE ? OBJ_oMachetePre : OBJ_oMattockPre);
        pin_set_sprite(obj, t == T_MACHETE ? GSPR_sMachetePreR : GSPR_sMattockPreR);
    }
}

/* ---- the bow --------------------------------------------------------------------------------------------- */
/* scripts/scrFireBow (oPlayer1) */
static void fire_bow(int i)
{
    struct pin *p = &PX(i);
    int obj = NOONE;
    if (PL.holdItem == NOONE || PX(PL.holdItem).type != T_BOW) return;
    if (PL.facing == LEFT) {
        if (CP(X(i) - 14, Y(i), OBJ_oSolid)) obj = pin_create(p->x, p->y, OBJ_oArrow);
        else obj = pin_create(p->x - PI(14), p->y, OBJ_oArrow);
        p = &PX(i);
        PE(&PX(obj))->xVel = PE(p)->xVel - N(1) - PL.bowStrength;
        if (NGE(PE(&PX(obj))->xVel, N(-1))) PE(&PX(obj))->xVel = N(-1);
        PE(&PX(obj))->yVel = 0;
        PE(&PX(obj))->direction = 180;
        PE(&PX(obj))->safe = 1;
        PE(&PX(obj))->alarm[2] = 10;
        snd_play(SND_xarrowtrap);                                              /* :37 */
        PL.firing = 10;
    } else if (PL.facing == RIGHT) {
        if (CP(X(i) + 14, Y(i), OBJ_oSolid)) obj = pin_create(p->x, p->y, OBJ_oArrow);
        else obj = pin_create(p->x + PI(14), p->y, OBJ_oArrow);
        p = &PX(i);
        PE(&PX(obj))->xVel = PE(p)->xVel + N(1) + PL.bowStrength;
        if (NLT(PE(&PX(obj))->xVel, N(1))) PE(&PX(obj))->xVel = N(1);
        PE(&PX(obj))->yVel = 0;
        PE(&PX(obj))->direction = 0;
        PE(&PX(obj))->safe = 1;
        PE(&PX(obj))->alarm[2] = 10;
        snd_play(SND_xarrowtrap);                                              /* :51 */
        PL.firing = 10;
    }
    if (PL.holdArrow == ARROW_BOMB) {                                          /* :54 */
        pin_set_sprite(obj, GSPR_sBombArrowRight);
        PE(&PX(obj))->alarm[1] = (int16_t)PL.bombArrowCounter;
        PL.bombArrowCounter = 80;
    }
    PL.holdArrow = 0;
    PG.arrows -= 1;
    if (p->spr == GSPR_sDuckLeft || p->spr == GSPR_sDamselDuckL) pin_sety(&PX(obj), PX(obj).y + PI(4));
    PL.bowArmed = 0;
    PL.bowStrength = 0;
    if (snd_is_playing(SND_xbowpull)) snd_stop(SND_xbowpull);
}

/* oPlayer1 Step :1431-1442 */
static void bow_pull(int i)
{
    (void)i;
    PL.bowStrength += N(0.2);
    if (PL.holdItem == NOONE) {
        PL.bowArmed = 0;
        PL.bowStrength = 0;
        if (snd_is_playing(SND_xbowpull)) snd_stop(SND_xbowpull);
    }
    PL.holdArrow = ARROW_NORM;
}

/* oPlayer1 Step :1947-1960: arrows picked up with the bow held */
static void bow_arrows(int i)
{
    double x = X(i), y = Y(i);
    if (collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oArrow, 0, NOONE) != NOONE && !PL.dead && !PL.stunned) {
        int obj = instance_nearest_p(x, y, OBJ_oArrow);
        if (NLT(NABS(PE(&PX(obj))->xVel), N(1)) && NLT(NABS(PE(&PX(obj))->yVel), N(1)) && !PE(&PX(obj))->stuck) {
            PG.arrows += 1;
            snd_play(SND_xpickup);                                             /* :1954 */
            pin_destroy(obj);
        }
    }
}

/* ---- the jetpack ------------------------------------------------------------------------------------------- */
/* characterStepEvent :339-351 */
static void jetpack_fly(int i)
{
    struct pin *p = &PX(i);
    PE(p)->yAcc += PL.initialJumpAcc;
    PE(p)->yVel = N(-1);
    PL.jetpackFuel -= 1;
    if (PE(p)->alarm[10] < 1) PE(p)->alarm[10] = 3;
    PL.state = JUMPING;
    PL.jumpButtonReleased = 0;
    PL.jumpTime = 0;
    PE(p)->grav = 0;
}

/* objects/oPlayer1/Alarm_10.gml */
static void jetpack_alarm(int i)
{
    int ya = RAND(0, 3), yb = RAND(0, 3);
    int xa = RAND(0, 3), xb = RAND(0, 3);
    int obj = pin_create(PX(i).x + PI(xa - xb), PX(i).y + PI(ya - yb), OBJ_oFlareSpark);
    PE(&PX(obj))->yVel = NI(RAND(1, 3));
    {
        int a = RAND(0, 3), b = RAND(0, 3);
        PE(&PX(obj))->xVel = NI(a - b);
    }
    snd_play(SND_xjetpack);                                                    /* :4 */
}

/* ---- equipment ------------------------------------------------------------------------------------------- */
/* scripts/scrStealItem :24-227: the equipment picked up (oPlayer1; holdItem one of these types) */
static void steal_equipment(int i)
{
    int h = PL.holdItem, t = PX(h).type, obj, dy = 14;
    const char *m1 = "", *m2 = "";
    switch (t) {
    case T_UDJATEYE: PG.hasUdjatEye = 1; m1 = "YOU GOT THE UDJAT EYE!"; m2 = "YOU FEEL AWAKENED."; break;
    case T_ANKH: PG.hasAnkh = 1; m1 = "YOU GOT THE ANKH!"; m2 = "YOU FEEL PROTECTED."; break;
    case T_CROWN: PG.hasCrown = 1; m1 = "YOU GOT THE HEDJET!"; m2 = "IT GLOWS A BRILLIANT WHITE."; break;
    case T_KAPALA: PG.hasKapala = 1; m1 = "YOU GOT THE KAPALA!"; m2 = "IT THIRSTS FOR BLOOD..."; break;
    case T_PASTE: PG.hasStickyBombs = 1; m1 = "YOU GOT STICKY BOMBS!"; break;
    case T_PARACHUTE: PG.hasParachute = 1; m1 = "YOU GOT A PARACHUTE!"; m2 = "IT WILL DEPLOY AUTOMATICALLY."; break;
    case T_SPECTACLES: G.hasSpectacles = 1; m1 = "YOU GOT SPECTACLES!"; m2 = "YOUR EYESIGHT SEEMS IMPROVED..."; break;
    case T_GLOVES:
        PG.hasGloves = 1;
        m1 = "YOU GOT CLIMBING GLOVES!";
        if (PL.pickupItemType == T_WEBCANNON) m2 = "YOUR SPIDER SENSE TINGLES!";
        break;
    case T_MITT: PG.hasMitt = 1; m1 = "YOU GOT A PITCHER'S MITT!"; break;
    case T_COMPASS: PG.hasCompass = 1; m1 = "YOU GOT A COMPASS!"; break;
    case T_SPRINGSHOES: PG.hasSpringShoes = 1; m1 = "YOU GOT SPRING SHOES!"; m2 = "YOU FEEL BOUNCY."; break;
    case T_SPIKESHOES: PG.hasSpikeShoes = 1; m1 = "YOU GOT SPIKE SHOES!"; break;
    case T_JORDANS: PG.hasJordans = 1; m1 = "YOU GOT JORDANS!"; m2 = "YOU FEEL LIGHT ON YOUR FEET."; break;
    case T_CAPE: PG.hasCape = 1; m1 = "YOU GOT A CAPE!"; break;
    case T_JETPACK: PG.hasJetpack = 1; m1 = "YOU GOT A JETPACK!"; break;
    default: PUNTR(3002); return;
    }
    pin_create(PX(h).x, PX(h).y - PI(dy), OBJ_oItemsGet);                     /* disp */
    pin_destroy(h);
    snd_play(SND_xpickup);
    PL.holdItem = NOONE;
    if (t == T_CAPE && PG.hasJetpack) {                                        /* :195 */
        obj = pin_create(PX(i).x, PX(i).y, OBJ_oJetpack);
        PE(&PX(obj))->cost = 0;
        PE(&PX(obj))->forSale = 0;
        PE(&PX(obj))->yVel = N(-1);
        PG.hasJetpack = 0;
    } else if (t == T_JETPACK && PG.hasCape) {                                 /* :214 */
        int16_t w[16];
        int n, k;
        obj = pin_create(PX(i).x, PX(i).y, OBJ_oCapePickup);
        PE(&PX(obj))->cost = 0;
        PE(&PX(obj))->forSale = 0;
        PE(&PX(obj))->yVel = N(-1);
        PG.hasCape = 0;
        n = pw_with(OBJ_oCape, w, 16);
        for (k = 0; k < n; k++) if (PX(w[k]).alive) pin_destroy(w[k]);
    }
    pmsg_player_str(m1, m2, 120);                                              /* :304 */
}

/* oPlayer1 Step :254-266: the parachute opens */
static void parachute_open(int i)
{
    if (!CP(X(i), Y(i) + 32, OBJ_oSolid)) {
        pin_create(PX(i).x - PI(8), PX(i).y - PI(16), OBJ_oParachute);
        PL.fallTimer = 0;
        PG.hasParachute = 0;
    }
}

/* oPlayer1 Step :298-302 / :1712-1716: the parachute is dropped */
static void parachute_drop(int i)
{
    int16_t w[16];
    int n, k;
    pin_create(PX(i).x - PI(8), PX(i).y - PI(16) - PI(8), OBJ_oParaUsed);
    n = pw_with(OBJ_oParachute, w, 16);
    for (k = 0; k < n; k++) if (PX(w[k]).alive) pin_destroy(w[k]);
}

/* oPlayer1 Step :1519-1524: a rock thrown at the player is caught with the mitt (holdItem = oRock: the object
   index, so holdItem.held sets every rock; the C keeps the first rock as holdItem) */
static void mitt_catch(int i)
{
    int16_t w[256];
    int n = pw_with(OBJ_oRock, w, 256), k, h;
    (void)i;
    for (k = 0; k < n; k++) if (PX(w[k]).alive) PE(&PX(w[k]))->held = 1;
    h = instance_first_p(OBJ_oRock);
    PL.holdItem = h;
    if (h != NOONE) PL.pickupItemType = PX(h).type;
}

/* scrUseItem :652-661: a throw with the mitt */
static void mitt_throw(int i)
{
    struct pin *o = &PX(PL.holdItem);
    (void)i;
    if (NLT(PE(o)->xVel, N(0))) PE(o)->xVel -= N(6);
    else PE(o)->xVel += N(6);
    if (!PL.kUp && !PL.kDown) PE(o)->yVel = N(-0.4);
    else if (PL.kDown) PE(o)->yVel = N(6);
    PE(o)->myGrav = N(0.1);
}

/* oPlayer1 Step :1825-1866: revived by the ankh */
static void ankh_revive(int i)
{
    struct pin *p = &PX(i);
    int16_t w[64];
    int n, k;
    PG.plife = 4;
    if (instance_exists_p(OBJ_oMoai)) {
        int m;
        n = pw_with(OBJ_oMoaiInside, w, 64);
        for (k = 0; k < n; k++) if (PX(w[k]).alive) pin_destroy(w[k]);
        m = instance_first_p(OBJ_oMoai);
        pin_setxy(p, PX(m).x + PI(16 + 8), PX(m).y + PI(16 + 8));
    } else if (isRoomIs(R_rOlmec))
        pin_setxy(p, PI(16 + 8), PI(544 + 8));
    else {
        int e = instance_first_p(OBJ_oEntrance);
        if (e != NOONE) pin_setxy(p, PX(e).x + PI(8), PX(e).y + PI(8));
    }
    n = pw_with(OBJ_oBall, w, 64);
    for (k = 0; k < n; k++) if (PX(w[k]).alive) pin_setxy(&PX(w[k]), PX(i).x, PX(i).y);
    n = pw_with(OBJ_oChain, w, 64);
    for (k = 0; k < n; k++) if (PX(w[k]).alive) pin_setxy(&PX(w[k]), PX(i).x, PX(i).y);
    p = &PX(i);
    PE(p)->xVel = 0;
    PE(p)->yVel = 0;
    PL.blink = 60;
    PL.invincible = 60;
    PL.fallTimer = 0;
    pin_setvisible(p, 1);
    PL.active = 1;
    PL.dead = 0;
    PG.hasAnkh = 0;
    pmsg_player_str("THE ANKH SHATTERS!", "YOU HAVE BEEN REVIVED!", 150);
    snd_play(SND_xteleport);                                                   /* :1866 */
}

/* objects/oPlayer1/Collision_oBlood.gml (global.hasKapala and other.collectible) */
static void kapala_blood(int i, int other)
{
    PMSG.bloodLevel += 1;
    pin_create(PX(other).x, PX(other).y, OBJ_oBloodSpark);
    pin_destroy(other);
    if (PMSG.bloodLevel > 8) {
        PMSG.bloodLevel = 0;
        PG.plife += 1;
        pin_create(PX(i).x, PX(i).y - PI(8), OBJ_oHeart);
        snd_play(SND_xkiss);                                                   /* :13 */
    }
    if (PL.redColor < 55) PL.redColor += 5;
    PL.redToggle = 0;
}

/* characterStepEvent :913-940: the ball and chain */
static void ball_pull(int i)
{
    struct pin *p = &PX(i);
    int b = instance_first_p(OBJ_oBall);
    double bx, by, x = X(i), y = Y(i);
    if (b == NOONE || !DGE(distance_to_object_p(i, OBJ_oBall), 24)) return;
    bx = X(b);
    by = Y(b);
    if (NGT(PE(p)->xVel, N(0)) && bx < x && DGT(bx - x < 0 ? x - bx : bx - x, 24)) PE(p)->xVel = 0;
    if (NLT(PE(p)->xVel, N(0)) && bx > x && DGT(bx - x < 0 ? x - bx : bx - x, 24)) PE(p)->xVel = 0;
    if (NGT(PE(p)->yVel, N(0)) && by < y && DGT(by - y < 0 ? y - by : by - y, 24)) {
        if (DLT(bx - x < 0 ? x - bx : bx - x, 1))
            pin_setx(p, PX(b).x);
        else if (bx < x && !PL.kRight) {
            if (NGT(PE(p)->xVel, N(0))) PE(p)->xVel = NMUL(PE(p)->xVel, N(-0.25));
            else if (NEQ(PE(p)->xVel, N(0))) PE(p)->xVel -= N(1);
        } else if (bx > x && !PL.kLeft) {
            if (NLT(PE(p)->xVel, N(0))) PE(p)->xVel = NMUL(PE(p)->xVel, N(-0.25));
            else if (NEQ(PE(p)->xVel, N(0))) PE(p)->xVel += N(1);
        }
        PE(p)->yVel = 0;
        PL.fallTimer = 0;
    }
    if (NLT(PE(p)->yVel, N(0)) && by > y && DGT(by - y < 0 ? y - by : by - y, 24)) PE(p)->yVel = 0;
}

/* characterStepEvent :722-733: ducking and running with global.downToRun */
static void down_to_run(int i)
{
    struct pin *p = &PX(i);
    if (PL.kLeft) PE(p)->xVel -= N(0.1);
    else PE(p)->xVel += N(0.1);
    PL.xVelLimit = N(6);
    PL.xFric = PL.frictionRunningFastX;
}

/* oPlayer1 Step :28-45: the bomb arrow held too long */
static void bomb_arrow_held(int i)
{
    if (PL.bombArrowCounter > 0) PL.bombArrowCounter -= 1;
    else {
        pin_create(PX(i).x, PX(i).y, OBJ_oExplosion);
        if (G.graphicsHigh) scrCreateFlame(PX(i).x, PX(i).y, 3);
        PL.bombArrowCounter = 80;
        PL.holdArrow = 0;
    }
    if (isInShop(PFLOOR(PX(i).x), PFLOOR(PX(i).y))) scrShopkeeperAnger(i, 2);
}

/* oPlayer1 Step :653-670: up + attack at a flare crate opens it */
static void open_flare_crate(int i)
{
    int chest = instance_place_p(i, X(i), Y(i), OBJ_oFlareCrate), k;
    if (chest == NOONE) { PUNTR(2042); return; }
    for (k = 0; k < 3; k++) {
        int obj = pin_create(PX(chest).x, PX(chest).y, OBJ_oFlare);
        int a = RAND(0, 3), b = RAND(0, 3);
        PE(&PX(obj))->xVel = NI(a - b);
        PE(&PX(obj))->yVel = NI(RAND(1, 3) * -1);
    }
    snd_play(SND_xpickup);                                                     /* :662 */
    if (chest == PL.holdItem) {
        PL.holdItem = NOONE;
        PL.pickupItemType = T_NONE;
    }
    pin_create(PX(chest).x, PX(chest).y, OBJ_oPoof);                           /* with chest */
    pin_destroy(chest);
    PL.kAttackPressed = 0;
}

int pitems_player(int site, int i, int arg)
{
    (void)arg;
    switch (site) {
    case 2042: open_flare_crate(i); return 1;
    case 2008: down_to_run(i); return 1;
    case 2012: ball_pull(i); return 1;
    case 2017: mitt_catch(i); return 1;
    case 2031: bomb_arrow_held(i); return 1;
    case 2034: parachute_open(i); return 1;
    case 2035: case 2052: parachute_drop(i); return 1;
    case 2054: ankh_revive(i); return 1;
    case 2062: kapala_blood(i, arg); return 1;
    case 2072: mitt_throw(i); return 1;
    case 3002: steal_equipment(i); return 1;
    case 2006: jetpack_fly(i); return 1;
    case 2037: weapon_hit(i); return 1;
    case 2039: weapon_pre(i); return 1;
    case 2050: bow_pull(i); return 1;
    case 2055: bow_arrows(i); return 1;
    case 2044:                                                                 /* oPlayer1 Step :1136 */
        PL.holdArrow = ARROW_BOMB;
        PE(&PX(i))->alarm[11] = 1;
        return 1;
    case 2060: jetpack_alarm(i); return 1;
    case 2070: use_weapon(i); return 1;
    case 3001: fire_bow(i); return 1;
    }
    PUNTR(site);
    return 0;
}

/* ---- objects ---------------------------------------------------------------------------------------------- */
/* objects/oMattockHit/Other_7.gml */
static void mattock_hit_end(int i)
{
    struct pin *p = &PX(i);
    int hit = 0, obj = NOONE;
    if (CP(X(i), Y(i), OBJ_oSolid)) {
        obj = instance_place_p(i, X(i), Y(i), OBJ_oSolid);
        if (obj != NOONE && !PX(obj).invincible) hit = 1;
    } else if (CP(X(i), Y(i) + 9, OBJ_oSolid)) {
        obj = instance_place_p(i, X(i), Y(i) + 9, OBJ_oSolid);
        if (obj != NOONE && !PX(obj).invincible) hit = 1;
    }
    if (hit && !isRoomIs(R_rTitle) && !isRoomIs(R_rHighscores)) {
        int16_t w[512];
        int n, k;
        if (!PX(obj).invincible) pin_destroy(obj);                             /* with obj (the tiles: drawing) */
        n = pw_with(OBJ_oTreasure, w, 512);
        for (k = 0; k < n; k++) if (PX(w[k]).alive) PE(&PX(w[k]))->state = 1;
        n = pw_with(OBJ_oSpikes, w, 512);
        for (k = 0; k < n; k++) {
            int s = w[k];
            if (!PX(s).alive) continue;
            if (!CP(X(s), Y(s) + 16, OBJ_oSolid)) pin_destroy(s);
        }
        if (RAND(1, 20) == 1 && !G.isTunnelMan) {
            PL.holdItem = NOONE;
            PL.pickupItemType = T_NONE;
            G.pickupItem = PICK_NONE;
            p = &PX(i);
            obj = pin_create(p->x, p->y, OBJ_oMattockHead);
            PE(&PX(obj))->yVel = N(-2);
            snd_play(SND_xmattockbreak);                                       /* :43 */
            n = pw_with(OBJ_oMattock, w, 512);
            for (k = 0; k < n; k++) if (PX(w[k]).alive && !PX(w[k]).visible) pin_destroy(w[k]);
        } else
            snd_play(SND_xcrunch);                                             /* :50 */
    }
    pin_destroy(i);
}

/* oSlash / oMattockHit Step (oCharacter: the player) */
static void slash_step(int i, int right, int left, int pre)
{
    struct pin *p = &PX(i);
    if (pre ? PL.idx == NOONE || !PX(PL.idx).alive : instance_number_p(OBJ_oCharacter) == 0) {
        pin_destroy(i);
        if (pre) return;
    }
    if (PL.idx == NOONE) return;
    if (p->spr == right) pin_setxy(p, PX(PL.idx).x + PI(pre ? -16 : 16), PX(PL.idx).y);
    else if (p->spr == left) pin_setxy(p, PX(PL.idx).x + PI(pre ? 16 : -16), PX(PL.idx).y);
}

/* objects/oFlare/Step_0.gml (oItem's first) */
static void flare_step(int i)
{
    struct pin *p;
    item_step(i);
    p = &PX(i);
    /* distToPlayer = distance_to_object(oPlayer1): the dark level's light (drawing) */
    if (collision_point_p(X(i), Y(i), OBJ_oWater, 1, i) != NOONE) {
        pin_create(p->x, p->y, OBJ_oSplash);
        snd_play(SND_xsplash);                                                 /* :6 */
        p = &PX(i);
        if (PE(p)->held) {
            PL.holdItem = NOONE;
            PL.pickupItemType = T_NONE;
            PE(p)->held = 0;
        }
        pin_destroy(i);
    }
}

/* oFlare / oFlareCrate Alarm_0: a spark (y's numbers first) */
static void flare_spark(int i, int dy)
{
    int ya = RAND(0, 3), yb = RAND(0, 3);
    int xa = RAND(0, 3), xb = RAND(0, 3);
    pin_create(PX(i).x + PI(xa - xb), PX(i).y + PI(dy + ya - yb), OBJ_oFlareSpark);
    PE(&PX(i))->alarm[0] = 2;
}

/* objects/oBall/Step_0.gml (oItem's first) */
static void ball_step(int i)
{
    struct pin *p;
    int pl = PL.idx;
    item_step(i);
    p = &PX(i);
    if (pl == NOONE || !PX(pl).alive) return;
    if (DGE(distance_to_object_p(i, OBJ_oPlayer1), 24)) {
        double dx = X(pl) - X(i), ady = Y(pl) - Y(i);
        double adx = dx < 0 ? -dx : dx;
        if (ady < 0) ady = -ady;
        if (DGE(adx, 24) || !PE(p)->colBot) {
            if (DLT(adx, 1)) {
                pin_setx(p, PX(pl).x);
                PE(p)->xVel = 0;
            }
            if (PX(pl).x > p->x) {
                if (NGT(PE(&PX(pl))->xVel, N(0)) && p->y >= PX(pl).y) PE(p)->xVel = PE(&PX(pl))->xVel;
                else if (NLT(PE(p)->xVel, N(0))) PE(p)->xVel = NMUL(PE(p)->xVel, N(-0.5));
                else if (NEQ(PE(p)->xVel, N(0))) PE(p)->xVel = N(2);
            } else if (PX(pl).x < p->x) {
                if (NLT(PE(&PX(pl))->xVel, N(0)) && p->y >= PX(pl).y) PE(p)->xVel = PE(&PX(pl))->xVel;
                else if (NGT(PE(p)->xVel, N(0))) PE(p)->xVel = NMUL(PE(p)->xVel, N(-0.5));
                else if (NEQ(PE(p)->xVel, N(0))) PE(p)->xVel = N(-2);
            }
        } else {
            PE(p)->xVel = NMUL(PE(p)->xVel, N(0.5));
            if (NLT(NABS(PE(p)->xVel), N(0.5))) PE(p)->xVel = 0;
        }
        if (DGE(ady, 24)) {
            if (PX(pl).y < p->y) PE(p)->yVel = 0;
        }
    } else if (PE(p)->colBot)
        PE(p)->xVel = 0;
}

/* objects/oChain/Step_0.gml; linkVal in counter (the generator's chains, made by oLevel Create in creation order
   with linkVal 1-4, arrive with 0: their rank among the chains) */
static void chain_step(int i)
{
    struct pin *p = &PX(i);
    int b = instance_first_p(OBJ_oBall), pl = PL.idx;
    if (b == NOONE) {
        pin_destroy(i);
        return;
    }
    if (PE(p)->counter == 0) {
        int k, r = 1;
        for (k = pw_ohead[OBJ_oChain]; k != NOONE && k != i; k = pw_inext[k]) r++;
        PE(p)->counter = (int16_t)r;
    }
    if (pl == NOONE) return;
    pin_setx(p, P(X(b) + ((X(pl) - X(b)) / 4) * PE(p)->counter));
    pin_sety(p, P(Y(b) + ((Y(pl) - Y(b)) / 4) * PE(p)->counter));
}

/* objects/oGoldIdol/Step_0.gml for oCrystalSkull (pobj.c's goldidol_step is oGoldIdol's own case) */
static void idol_step(int i)
{
    struct pin *p;
    item_step(i);
    p = &PX(i);
    if (inview(i, 8)) {
        if (isLevel()) {
            if (!PE(p)->held && CP(X(i), Y(i) + 4, OBJ_oBrickSmooth) && instance_exists_p(OBJ_oShopkeeper) &&
                G.thiefLevel == 0 && !G.murderer)
                pitems_world(1043, i, 0);
        }
        p = &PX(i);
        if (!PE(p)->colBot && PE(p)->trigger) PE(p)->trigger = 0;
    }
}

/* ---- sticky bombs -------------------------------------------------------------------------------------------- */
/* oBomb's stickyXDiff / stickyYDiff (enemyID.x - x, in GML reals), by the bomb's instance id */
#define NSTICKY 16
static struct { int32_t id; double dx, dy; } sticky[NSTICKY];

static int sticky_slot(int32_t id, int make)
{
    int k, f = -1;
    for (k = 0; k < NSTICKY; k++) {
        if (sticky[k].id == id && id != 0) return k;
        if (f < 0 && (sticky[k].id == 0 || !make)) f = k;
    }
    if (!make) return -1;
    for (k = 0; k < NSTICKY; k++) {                  /* a slot whose bomb is gone */
        int j, used = 0;
        if (sticky[k].id == 0) return sticky[k].id = id, k;
        for (j = pw_ohead[OBJ_oBomb]; j != NOONE; j = pw_inext[j]) if (PX(j).id == sticky[k].id) used = 1;
        if (!used) return sticky[k].id = id, k;
    }
    (void)f;
    return 0;
}

static void sticky_attach(int i, int e)
{
    struct pin *p = &PX(i);
    int k = sticky_slot(p->id, 1);
    PE(p)->enemyID = (int16_t)e;
    PEN(&PX(e))->bombID = (int16_t)i;
    sticky[k].dx = X(e) - X(i);
    sticky[k].dy = Y(e) - Y(i);
}

/* oItem Step :217-232: a sticky bomb flying into an enemy or a damsel sticks to it */
static void sticky_fly(int i)
{
    struct pin *p = &PX(i);
    double x = X(i), y = Y(i);
    int e;
    if (!(NGT(NABS(PE(p)->xVel), N(2)) || NGT(NABS(PE(p)->yVel), N(2)))) return;
    if (collision_rect_p(x - 2, y - 2, x + 2, y + 2, OBJ_oEnemy, 0, NOONE) != NOONE) {
        e = instance_nearest_p(x, y, OBJ_oEnemy);
        if (e != NOONE) sticky_attach(i, e);
    } else if ((e = collision_rect_p(x - 2, y - 2, x + 2, y + 2, OBJ_oDamsel, 0, NOONE)) != NOONE)
        sticky_attach(i, e);
}

/* ---- Kali ------------------------------------------------------------------------------------------------- */
/* global.favor, global.kaliGift (scrClearGlobals: 0) */
static double kali_favor;
static int kali_gift;

/* scripts/scrGetFavorMsg; y: the caller's */
static const char *scrGetFavorMsg(double y)
{
    const char *m2 = "";
    int alt = instance_first_p(OBJ_oSacAltarRight), obj = NOONE;
    pos ax = alt != NOONE ? PX(alt).x : 0, ay = P(y - 8);
    if (DLE(kali_favor, -8)) m2 = "SHE SEEMS VERY ANGRY WITH YOU!";
    else if (DLT(kali_favor, 0)) m2 = "SHE SEEMS ANGRY WITH YOU.";
    else if (DEQ(kali_favor, 0)) m2 = "SHE HAS FORGIVEN YOU!";
    else if (DGE(kali_favor, 32)) {
        if (kali_gift >= 3 && DGE(kali_favor, 32 + (kali_gift - 2) * 16)) {
            m2 = "YOU FEEL INVIGORATED!";
            kali_gift += 1;
            PG.plife += RAND(4, 8);
        } else if (kali_gift >= 3)
            m2 = "SHE SEEMS ECSTATIC WITH YOU!";
        else if (PG.bombs < 80) {
            m2 = "YOUR SATCHEL FEELS VERY FULL NOW!";
            kali_gift = 3;
            PG.bombs = 99;
        } else {
            m2 = "YOU FEEL INVIGORATED!";
            kali_gift += 1;
            PG.plife += RAND(4, 8);
        }
    } else if (DGE(kali_favor, 16)) {
        if (kali_gift >= 2) m2 = "SHE SEEMS VERY HAPPY WITH YOU!";
        else {
            m2 = "SHE BESTOWS A GIFT UPON YOU!";
            kali_gift = 2;
            obj = pin_create(ax, ay, OBJ_oKapala);
            PE(&PX(obj))->cost = 0;
            PE(&PX(obj))->forSale = 0;
        }
    } else if (DGE(kali_favor, 8)) {
        if (kali_gift >= 1) m2 = "SHE SEEMS HAPPY WITH YOU.";
        else {
            m2 = "SHE BESTOWS A GIFT UPON YOU!";
            kali_gift = 1;
            if (alt != NOONE) {
                int n, m;
                obj = pin_create(ax, ay, OBJ_oPoof);
                PE(&PX(obj))->xVel = N(-1);
                PE(&PX(obj))->yVel = 0;
                obj = pin_create(ax, ay, OBJ_oPoof);
                PE(&PX(obj))->xVel = N(1);
                PE(&PX(obj))->yVel = 0;
                n = RAND(1, 8);
                m = n;
                for (;;) {
                    if (n == 1 && !PG.hasCape && !PG.hasJetpack) { obj = pin_create(ax, ay, OBJ_oCapePickup); break; }
                    else if (n == 2 && !PG.hasGloves) { obj = pin_create(ax, ay, OBJ_oGloves); break; }
                    else if (n == 3 && !G.hasSpectacles) { obj = pin_create(ax, ay, OBJ_oSpectacles); break; }
                    else if (n == 4 && !PG.hasMitt) { obj = pin_create(ax, ay, OBJ_oMitt); break; }
                    else if (n == 5 && !PG.hasSpringShoes) { obj = pin_create(ax, ay, OBJ_oSpringShoes); break; }
                    else if (n == 6 && !PG.hasSpikeShoes) { obj = pin_create(ax, ay, OBJ_oSpikeShoes); break; }
                    else if (n == 7 && !PG.hasStickyBombs) { obj = pin_create(ax, ay, OBJ_oPaste); break; }
                    else if (n == 8 && !PG.hasCompass) { obj = pin_create(ax, ay, OBJ_oCompass); break; }
                    n += 1;
                    if (n > 8) n = 1;
                    if (n == m) {
                        if (!PG.hasJetpack) obj = pin_create(ax, ay, OBJ_oJetpack);
                        else obj = pin_create(ax, ay, OBJ_oBombBox);
                        break;
                    }
                }
                PE(&PX(obj))->cost = 0;
                PE(&PX(obj))->forSale = 0;
            }
        }
    } else if (DGT(kali_favor, 0))
        m2 = "SHE SEEMS PLEASED WITH YOU.";
    return m2;
}

/* an enemy's favor (its Create's: oEnemy 1, oCaveman / oManTrap 2, oYeti 4, oHawkman 6, oShopkeeper 12) */
static double enemy_favor(int i)
{
    int o = PX(i).obj;
    if (obj_is(o, OBJ_oShopkeeper)) return 12;
    if (obj_is(o, OBJ_oHawkman)) return 6;
    if (obj_is(o, OBJ_oYeti)) return 4;
    if (obj_is(o, OBJ_oCaveman) || obj_is(o, OBJ_oManTrap)) return 2;
    return 1;
}

/* oEnemy Step :135-163 (damsel 0) and oDamsel Step :315-343 (damsel 1): a sacrifice on Kali's altar */
static void sacrifice(int i, int damsel)
{
    struct pin *p = &PX(i);
    const char *m1;
    double fav = damsel ? 8 : enemy_favor(i), x = damsel ? X(i) : X(i) + 8, y = damsel ? Y(i) : Y(i) + 8;
    if (PEN(p)->sacCount > 0) {
        PEN(p)->sacCount -= 1;
        return;
    }
    pin_create(P(x), P(y), OBJ_oFlame);
    snd_play(SND_xsmallexplode);
    scrCreateBlood(i, P(x), P(y), 3);
    p = &PX(i);
    m1 = damsel ? "KALI ACCEPTS YOUR SACRIFICE!" : "KALI ACCEPTS THE SACRIFICE!";
    if (DLE(kali_favor, -8))
        m1 = damsel ? "KALI DEVOURS YOUR SACRIFICE!" : "KALI DEVOURS THE SACRIFICE!";
    else if (PE(p)->status == 98)
        kali_favor += damsel ? fav * 1.5 : fav;
    else
        kali_favor += damsel ? fav : fav / 2;
    pmsg_str(m1, scrGetFavorMsg(Y(i)), 200);
    PG.shake = 10;
    pin_destroy(i);
}

/* objects/oSacAltarLeft/Destroy_0.gml (and oSacAltarRight's); defile: !PE->dying */
static void altar_destroy(int i)
{
    struct pin *p = &PX(i);
    if (!p->cleanDeath && !G.cleanSolids) {
        static const int16_t obj[3] = { OBJ_oRubble, OBJ_oRubbleSmall, OBJ_oRubbleSmall };
        int k;
        for (k = 0; k < 3; k++) {
            int ya = RAND(0, 8), yb = RAND(0, 8);
            int xa = RAND(0, 8), xb = RAND(0, 8);
            int r = pin_create(PX(i).x + PI(8 + xa - xb), PX(i).y + PI(8 + ya - yb), obj[k]);
            pin_set_sprite(r, k == 0 ? GSPR_sRubbleTan : GSPR_sRubbleTanSmall);
        }
    }
    p = &PX(i);
    if (!PE(p)->dying) {
        int16_t w[64];
        int n, k, pl = PL.idx;
        pmsg_str("YOU DARE DEFILE MY ALTAR?", "I WILL PUNISH YOU!", 200);
        scrShake(10);
        kali_favor -= 16;
        if (G.kaliPunish == 0) {
            n = pw_with(OBJ_oKaliHead, w, 64);
            for (k = 0; k < n; k++) if (PX(w[k]).alive) PE(&PX(w[k]))->alarm[0] = 1;
        } else if (G.kaliPunish == 1) {
            int c;
            pin_create(PX(pl).x, PX(pl).y, OBJ_oBall);
            for (c = 1; c <= 4; c++) {
                int o = pin_create(PX(pl).x, PX(pl).y, OBJ_oChain);
                PE(&PX(o))->counter = (int16_t)c;
            }
        } else if (G.darkLevel && PG.ghostExists) {
            n = pw_with(OBJ_oKaliHead, w, 64);
            for (k = 0; k < n; k++) if (PX(w[k]).alive) PE(&PX(w[k]))->alarm[0] = 1;
        } else {
            G.darkLevel = 1;
            if (!PG.ghostExists) {
                view_read();
                if (X(pl) > PW.room_w / 2.0) pin_create(PI(PW.xview + 320 + 8), PI(PW.yview + 240 / 2), OBJ_oGhost);
                else pin_create(PI(PW.xview - 32), PI(PW.yview + 240 / 2), OBJ_oGhost);
                PG.ghostExists = 1;
            }
        }
        G.kaliPunish += 1;
        n = pw_with(OBJ_oSacAltarLeft, w, 64);
        for (k = 0; k < n; k++) {
            if (!PX(w[k]).alive) continue;
            PE(&PX(w[k]))->dying = 1;
            pin_destroy(w[k]);
        }
    }
}

/* objects/oKaliHead/Alarm_0.gml */
static void kali_head_alarm(int i)
{
    int k;
    pin_set_sprite(i, GSPR_sGTHHole);
    for (k = 0; k < 6; k++) {
        int obj = pin_create(PX(i).x, PX(i).y, OBJ_oSpider);
        int a = RAND(0, 3), b = RAND(0, 3);
        PE(&PX(obj))->xVel = NI(a - b);
        PE(&PX(obj))->yVel = NI(-RAND(1, 3));
    }
    snd_play(SND_xthump);
}

/* objects/oSkull/Destroy_0.gml :2-19 (breakPieces; pobj.c played the sound and made the puff) */
static void skull_pieces(int i)
{
    struct pin *p = &PX(i);
    int k;
    for (k = 0; k < RAND(1, 2); k++) {
        int piece = pin_create(PX(i).x - PI(2), PX(i).y - PI(2), OBJ_oBone);
        p = &PX(i);
        if (PE(p)->colLeft) PE(&PX(piece))->xVel = NI(RAND(1, 3));
        else if (PE(p)->colRight) PE(&PX(piece))->xVel = NI(-RAND(1, 3));
        else {
            int a = RAND(1, 3), b = RAND(1, 3);
            PE(&PX(piece))->xVel = NI(a - b);
        }
        if (PE(p)->colTop) PE(&PX(piece))->yVel = NI(RAND(0, 3));
        else PE(&PX(piece))->yVel = NI(-RAND(0, 3));
    }
    if (PE(p)->held) {
        PL.holdItem = NOONE;
        G.pickupItem = PICK_NONE;                                              /* oPlayer1.pickupItem = "" */
    }
}

int pitems_world(int site, int i, int arg)
{
    (void)arg;
    switch (site) {
    case 1015: skull_pieces(i); return 1;
    case 1016: pin_create(PX(i).x - PI(8), PX(i).y - PI(8), OBJ_oSpider); return 1;   /* oJar Destroy :19 */
    case 1018:                                                                 /* objects/oBomb/Destroy_0.gml */
        if (PE(&PX(i))->enemyID != NOONE && PX(PE(&PX(i))->enemyID).alive)
            PEN(&PX(PE(&PX(i))->enemyID))->bombID = NOONE;
        return 1;
    case 1031: {                                                               /* oItem Step :113-120 */
        struct pin *p = &PX(i);
        if (PE(p)->colLeft || PE(p)->colRight || PE(p)->colTop || PE(p)->colBot) {
            PE(p)->xVel = 0;
            PE(p)->yVel = 0;
            if (PE(p)->colBot && NLT(NABS(PE(p)->yVel), N(1))) pin_sety(p, p->y + PI(1));
        }
        return 1;
    }
    case 1033: sticky_fly(i); return 1;
    case 1062: {                                                               /* objects/oBomb/Step_2.gml */
        int e = PE(&PX(i))->enemyID, k = sticky_slot(PX(i).id, 0);
        if (k >= 0) pin_setxy(&PX(i), P(X(e) - sticky[k].dx), P(Y(e) - sticky[k].dy));
        return 1;
    }
    case 1042: {                                                               /* objects/oBomb/Step_0.gml :5-15 */
        int r = G.roomPath[scrGetRoomX(PFLOOR(PX(i).x))][scrGetRoomY(PFLOOR(PX(i).y))];
        if ((r == 4 || r == 5) && DLT(distance_to_object_p(i, OBJ_oShopkeeper), 96)) {
            int16_t w[16];
            int n = pw_with(OBJ_oShopkeeper, w, 16), k;
            for (k = 0; k < n; k++) if (PX(w[k]).alive) scrShopkeeperAnger(w[k], 2);
        }
        return 1;
    }
    case 1043: {                                                               /* oGoldIdol Step :11-24 */
        int s = instance_first_p(OBJ_oShopkeeper);
        struct pin *p = &PX(i);
        double d = X(i) - X(s);
        if (PE(&PX(s))->status == 0 && DLT(d < 0 ? -d : d, 80)) {
            PG.collect += PE(p)->value * (G.levelType + 1);
            PG.collectCounter += 20;
            if (PG.collectCounter > 100) PG.collectCounter = 100;
            PG.idols += 1;
            snd_play(SND_xcoin);                                               /* :18 */
            pin_create(p->x, p->y - PI(8), OBJ_oBigCollect);
            pmsg_str("PLEASURE DOING BUSINESS!", "", 100);
            pin_destroy(i);
        }
        return 1;
    }
    case 1051: {                                                               /* oGame Step :3-11: the udjat eye */
        int pl = instance_first_p(OBJ_oPlayer1);
        if (pl != NOONE) {
            double dm = distance_to_object_p(pl, OBJ_oXMarket);
            int16_t *a2 = &PE(&PX(i))->alarm[2];
            if (DLT(dm, 4)) dm = 4;
            if (*a2 < 1 || DLT(dm, *a2)) *a2 = (int16_t)dm;
        }
        return 1;
    }
    case 4001: pin_create(0, 0, OBJ_oCape); return 1;                          /* oTransition Create :25 */
    case 4003: {                                                               /* oTransition Create :36-47 */
        int d = instance_first_p(OBJ_oPDummy), c;
        if (d == NOONE) { PUNTR(4003); return 0; }
        pin_create(PX(d).x, PX(d).y + PI(2), OBJ_oBall2);
        for (c = 1; c <= 4; c++) {
            int o = pin_create(PX(d).x, PX(d).y, OBJ_oChain2);
            PE(&PX(o))->counter = (int16_t)c;
        }
        return 1;
    }
    case 5014: sacrifice(i, 0); return 1;
    case 6012: sacrifice(i, 1); return 1;
    case 8002: pin_destroy(i); return 1;                                       /* oSacAltarLeft Step :4 */
    case 1019:                                                                 /* oItem Destroy (oFlare, oFlareCrate, oDice) */
        if (PE(&PX(i))->held) PL.holdItem = NOONE;
        return 1;
    case 1060:
        if (PX(i).obj == OBJ_oFlare) { flare_step(i); return 1; }
        break;
    }
    PUNTR(site);
    return 0;
}

int pitems_ev(int ev, int i, int arg)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oFlare:
        if (ev == FEV_CREATE) {                                                /* objects/oFlare/Create_0.gml */
            create_item(p);
            p->ispd = (img_t)0.3;
            p->type = T_FLARE;
            PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;
            setCollisionBounds(i, -4, -4, 4, 4);
            PE(p)->xVel = 0;
            PE(p)->yVel = 0;
            PE(p)->grav = N(0.6);
            p->invincible = 1;
            PE(p)->bounce = 1;
            PE(p)->alarm[0] = 1;
            return 1;
        }
        if (ev == FEV_STEP) { flare_step(i); return 1; }
        if (ev == FEV_ALARM && arg == 0) { flare_spark(i, 0); return 1; }
        break;
    case OBJ_oFlareCrate:
        if (ev == FEV_CREATE) {                                                /* objects/oFlareCrate/Create_0.gml */
            create_item(p);
            p->type = T_FLARECRATE;
            PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;
            setCollisionBounds(i, -6, 0, 6, 8);
            p->ispd = (img_t)0.2;
            PE(p)->alarm[0] = 1;
            PE(p)->heavy = 1;
            PE(p)->yVel = 0;
            PE(p)->yAcc = N(0.2);
            return 1;
        }
        if (ev == FEV_ALARM && arg == 0) { flare_spark(i, -4); return 1; }
        break;
    case OBJ_oSlash:
        if (ev == FEV_CREATE) { p->type = T_MACHETE; p->ispd = 1; return 1; }
        if (ev == FEV_STEP) { slash_step(i, GSPR_sSlashRight, GSPR_sSlashLeft, 0); return 1; }
        if (ev == FEV_ANIMEND) { pin_destroy(i); return 1; }
        break;
    case OBJ_oMattockHit:
        if (ev == FEV_CREATE) { p->type = T_MATTOCK; p->ispd = (img_t)0.5; return 1; }
        if (ev == FEV_STEP) { slash_step(i, GSPR_sMattockHitR, GSPR_sMattockHitL, 0); return 1; }
        if (ev == FEV_ANIMEND) { mattock_hit_end(i); return 1; }
        break;
    case OBJ_oMachetePre: case OBJ_oMattockPre:
        if (ev == FEV_CREATE) {
            p->type = p->obj == OBJ_oMachetePre ? T_MACHETE : T_MATTOCK;
            PE(p)->alarm[0] = 3;
            return 1;
        }
        if (ev == FEV_STEP) {
            if (p->obj == OBJ_oMachetePre) slash_step(i, GSPR_sMachetePreR, GSPR_sMachetePreL, 1);
            else slash_step(i, GSPR_sMattockPreR, GSPR_sMattockPreL, 1);
            return 1;
        }
        if (ev == FEV_ALARM && arg == 0) { pin_destroy(i); return 1; }
        break;
    case OBJ_oBall:
        if (ev == FEV_CREATE) {                                                /* objects/oBall/Create_0.gml */
            create_item(p);
            p->type = T_OTHER;                                                 /* "Ball" */
            PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;
            setCollisionBounds(i, -5, -5, 5, 5);
            PE(p)->heavy = 1;
            PE(p)->myGrav = N(1);
            return 1;
        }
        if (ev == FEV_STEP) { ball_step(i); return 1; }
        break;
    case OBJ_oChain:
        if (ev == FEV_CREATE) { p->type = T_OTHER; PE(p)->counter = 2; return 1; }   /* "Chain"; linkVal = 2 */
        if (ev == FEV_STEP) { chain_step(i); return 1; }
        break;
    case OBJ_oCrystalSkull:
        if (ev == FEV_CREATE) {                                                /* oGoldIdol's Create, then its own */
            create_item(p);
            p->type = T_GOLDIDOL;
            PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;
            setCollisionBounds(i, -4, -4, 4, 4);
            PE(p)->trigger = 1;
            PE(p)->heavy = 1;
            PE(p)->value = 15000;
            return 1;
        }
        if (ev == FEV_STEP) { idol_step(i); return 1; }
        break;
    case OBJ_oKapala: case OBJ_oAnkh:
        if (ev == FEV_CREATE) {
            create_item(p);
            p->type = p->obj == OBJ_oKapala ? T_KAPALA : T_ANKH;
            PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;
            if (p->obj == OBJ_oKapala) setCollisionBounds(i, -6, -6, 6, 8);
            else setCollisionBounds(i, -4, -6, 4, 8);
            PE(p)->cost = p->obj == OBJ_oKapala ? 999999 : 50000;
            return 1;
        }
        break;
    case OBJ_oJetpack:
        if (ev == FEV_STEP) {                                                  /* objects/oJetpack/Step_0.gml */
            item_step(i);
            if (PL.idx != NOONE && PX(PL.idx).alive && !PX(PL.idx).visible) pin_destroy(i);
            return 1;
        }
        break;
    case OBJ_oParachute:
        if (ev == FEV_STEP) {                                                  /* objects/oParachute/Step_0.gml */
            if (PL.idx != NOONE && PX(PL.idx).alive) pin_setxy(p, PX(PL.idx).x - PI(8), PX(PL.idx).y - PI(16));
            return 1;
        }
        if (ev == FEV_ANIMEND) {
            if (p->spr == GSPR_sParaOpen) pin_set_sprite(i, GSPR_sParachute);
            return 1;
        }
        if (ev == FEV_COLLISION) {                                             /* Collision_oItem.gml */
            if (obj_is(PX(arg).obj, OBJ_oItem) && (NGT(PE(&PX(arg))->xVel, N(0)) || NGT(PE(&PX(arg))->yVel, N(0)))) {
                pin_create(p->x, p->y, OBJ_oParaUsed);
                pin_destroy(i);
            }
            return 1;
        }
        break;
    case OBJ_oParaUsed:
        if (ev == FEV_CREATE) { p->type = T_NONE; PE(p)->yVel = 0; PE(p)->yAcc = N(0.2); return 1; }
        if (ev == FEV_STEP) {                                                  /* objects/oParaUsed/Step_0.gml */
            if (!CP(X(i) + 8, Y(i) + 16, OBJ_oSolid)) {
                pin_sety(p, PADDV(p->y, PE(p)->yVel));
                PE(p)->yVel += PE(p)->yAcc;
            }
            if (CP(X(i) + 8, Y(i) + 15, OBJ_oSolid)) pin_sety(p, p->y - PI(1));
            return 1;
        }
        break;
    case OBJ_oBloodSpark:
        if (ev == FEV_CREATE) {
            p->type = T_NONE;
            { int r = RAND(1, 3); PE(p)->yVel = ND(-r * 0.2); }
            PE(p)->yAcc = N(0.1);
            p->ispd = (img_t)0.5;
            return 1;
        }
        if (ev == FEV_STEP) { pin_sety(p, PADDV(p->y, PE(p)->yVel)); return 1; }
        if (ev == FEV_ANIMEND) { pin_destroy(i); return 1; }
        break;
    case OBJ_oSacAltarLeft: case OBJ_oSacAltarRight:
        if (ev == FEV_DESTROY) { altar_destroy(i); return 1; }
        break;
    case OBJ_oKaliHead:
        if (ev == FEV_ALARM && arg == 0) { kali_head_alarm(i); return 1; }
        break;
    case OBJ_oBall2:
        if (ev == FEV_CREATE) {                                                /* objects/oBall2/Create_0.gml */
            create_item(p);
            p->type = T_OTHER;                                                 /* "Ball" */
            PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;
            setCollisionBounds(i, -5, -5, 5, 5);
            PE(p)->heavy = 1;
            PE(p)->myGrav = N(1);
            return 1;
        }
        if (ev == FEV_STEP) {                                                  /* objects/oBall2/Step_0.gml */
            int d;
            item_step(i);
            d = instance_first_p(OBJ_oPDummy);
            if (d != NOONE && DGE(distance_to_object_p(i, OBJ_oPDummy), 24)) pin_setx(&PX(i), PX(d).x - PI(24));
            return 1;
        }
        break;
    case OBJ_oChain2:
        if (ev == FEV_CREATE) { p->type = T_OTHER; PE(p)->counter = 2; return 1; }
        if (ev == FEV_STEP) {                                                  /* objects/oChain2/Step_0.gml */
            int d = instance_first_p(OBJ_oPDummy), b = instance_first_p(OBJ_oBall2);
            double tx = d != NOONE ? X(d) : 280, ty = d != NOONE ? Y(d) : 184;
            if (b == NOONE) { PUNTR(1061); return 1; }
            pin_setx(p, P(X(b) + ((tx - X(b)) / 4) * PE(p)->counter));
            pin_sety(p, P(Y(b) + ((ty - Y(b)) / 4) * PE(p)->counter));
            return 1;
        }
        break;
    case OBJ_oFlareSpark:
        if (ev == FEV_CREATE) {
            p->type = T_NONE;
            PE(p)->yVel = N(-0.1);
            PE(p)->yAcc = N(0.1);
            p->ispd = (img_t)0.4;
            return 1;
        }
        if (ev == FEV_STEP) {
            pin_sety(p, PADDV(p->y, PE(p)->yVel));
            if (CP(X(i), Y(i), OBJ_oSolid)) pin_destroy(i);
            return 1;
        }
        if (ev == FEV_ANIMEND) { pin_destroy(i); return 1; }
        break;
    }
    return 0;
}
