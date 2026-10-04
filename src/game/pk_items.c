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
        PE(&PX(obj))->yVel = ND(-prandom(2));
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

int pitems_world(int site, int i, int arg)
{
    (void)arg;
    switch (site) {
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
