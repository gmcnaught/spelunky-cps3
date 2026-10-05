/* P5: the shopkeeper and the shops: refs/hd/src/objects/oShopkeeper/<event>.gml, scripts/scrShopkeeperAnger, the
 * player paying (objects/oPlayer1/Step_0.gml :1327-1427), items carried out of a shop (oItem Step :18-35), the
 * shotgun's pellets (oBullet) and blasts (oShotgunBlastLeft / Right). Messages (trMessages) are not traced and
 * left out. The dice house's roll (a bet placed) and the black market set play_untranslated (7xxx).
 */
#include "pint.h"
#include "penemy.h"
#include "../snd/sndgame.h"                     /* the GML sound calls (src/snd) */
#include "pmsg.h"                                /* the HUD messages (trMessages) */
#include "pcontent.h"                            /* P7 content packages (docs/CONTENT.md) */

enum { S_IDLE = 0, S_WALK = 1, S_ATTACK = 2, S_THROW = 3, S_PATROL = 4, S_FOLLOW = 5, S_STUNNED = 98, S_DEAD = 99 };
enum { E_LEFT = 0, E_RIGHT = 1 };

static double X(int i) { return PTOD(PX(i).x); }
static double Y(int i) { return PTOD(PX(i).y); }
static int CP(double x, double y, int obj) { return collision_point_p(x, y, obj, 0, NOONE) != NOONE; }
/* collision_point(..., -1, -1): -1 is false for prec and notme (GML bool: value > 0.5; penemy.c CPn) */
static int CPn(double x, double y, int obj, int self) { (void)self; return collision_point_p(x, y, obj, 0, NOONE) != NOONE; }
static double dabs(double a) { return a < 0 ? -a : a; }
static int sprw(int i) { int s = PX(i).spr; return s >= 0 ? spr_dim(psprite[s].w, PX(i).xscale) : 0; }
static int sprh(int i) { int s = PX(i).spr; return s >= 0 ? spr_dim(psprite[s].h, PX(i).yscale) : 0; }

/* objects/oShopkeeper/Create_0.gml (fromgen: the generator set style, status, facing) */
int pshop_create(int i, int fromgen)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oShopkeeper: {
        int style = PE(p)->style, status = PE(p)->status;
        pen_enemy_create(i);
        PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;
        setCollisionBounds(i, 2, 0, sprw(i) - 2, sprh(i));
        PE(p)->xVel = 0;
        p->ispd = (img_t)0.5;
        PE(p)->myGrav = N(0.6);
        p->type = T_SHOPKEEPER;
        PE(p)->style = SHOP_GENERAL;
        PE(p)->hp = 20;
        p->invincible = 0;
        PE(p)->status = S_IDLE;
        PEN(p)->whipped = 0;
        PEN(p)->bounced = 0;
        PEN(p)->edead = 0;
        PE(p)->counter = 0;
        PEN(p)->sightCounter = 0;
        PEN(p)->turnTimer = 0;
        PEN(p)->throwCount = 0;
        PEN(p)->stunTime = 5;
        PE(p)->facing = E_LEFT;
        PEN(p)->welcomed = 0;
        PEN(p)->angered = 0;
        PE(p)->hasGun = 1;
        PEN(p)->firing = 0;
        PE(p)->colLeft = PE(p)->colRight = PE(p)->colBot = PE(p)->colTop = 0;
        if (fromgen) {
            PE(p)->style = (uint8_t)style;
            PE(p)->status = (int16_t)status;
        }
        return 1;
    }
    case OBJ_oBullet:                                                  /* objects/oBullet/Create_0.gml */
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        PE(p)->safe = 0;
        return 1;
    case OBJ_oShotgunBlastLeft: case OBJ_oShotgunBlastRight:
        p->ispd = (img_t)0.8;
        return 1;
    }
    return 0;
}

/* scripts/scrShopkeeperAnger: the shopkeeper nearest to the caller */
void scrShopkeeperAnger(int self, int k)
{
    int shp = instance_nearest_p(X(self), Y(self), OBJ_oShopkeeper);
    if (shp != NOONE && !PEN(&PX(shp))->edead && !PEN(&PX(shp))->angered) {
        static const char *const m[5] = { "COME BACK HERE, THIEF!", "DIE, YOU VANDAL!", "TERRORIST!",
                                          "HEY, ONLY I CAN DO THAT!", "NOW I'M REALLY STEAMED!" };
        PE(&PX(shp))->status = S_ATTACK;
        if (G.thiefLevel > 0) G.thiefLevel += 3;
        else G.thiefLevel += 2;
        const char *s = G.murderer ? "YOU'LL PAY FOR YOUR CRIMES!" : m[k >= 0 && k < 4 ? k : 4];
        if (self == PL.idx) pmsg_player_str(s, "", 80);                        /* :22 (message1/2: the caller's) */
        else pmsg_str(s, "", 80);
    }
}

/* oItem Step :18-35: an item for sale carried out of the shop */
void pshop_item_left_shop(int item)
{
    scrShopkeeperAnger(item, 0);
    PE(&PX(item))->cost = 0;
}

static void shoot(int i, int left)
{
    struct pin *p = &PX(i);
    int k;
    pin_create(p->x + PI(left ? 0 : 16), p->y + PI(9), left ? OBJ_oShotgunBlastLeft : OBJ_oShotgunBlastRight);
    for (k = 0; k < 6; k++) {
        int obj = pin_create(PX(i).x + PI(left ? 4 : 12), PX(i).y + PI(8), OBJ_oBullet);
        struct pin *b;
        p = &PX(i);
        b = &PX(obj);
        if (left) {
            PE(b)->xVel = NI(-1 * RAND(6, 8)) + PE(p)->xVel;
            if (NGE(PE(b)->xVel, N(-6))) PE(b)->xVel = N(-6);
        } else {
            PE(b)->xVel = NI(RAND(6, 8)) + PE(p)->xVel;
            if (NLT(PE(b)->xVel, N(6))) PE(b)->xVel = N(6);
        }
        {
            double a = prandom(1);
            double c = prandom(1);
            PE(b)->yVel = ND(a - c);
        }
        PE(b)->safe = 1;
        if (CP(PTOD(b->x), PTOD(b->y), OBJ_oSolid)) pin_destroy(obj);
    }
    p = &PX(i);
    PE(p)->yVel -= N(1);
    if (left) PE(p)->xVel += N(3);
    else PE(p)->xVel -= N(3);
    snd_play(SND_xshotgun);                                                    /* :416 / :433 */
    PEN(p)->firing = 30;                                                    /* firingMax */
}

static void drop_gun(int i)
{
    struct pin *p = &PX(i);
    if (PE(p)->hasGun) {
        int obj = pin_create(p->x + PI(8), p->y + PI(8), OBJ_oShotgun);
        PE(&PX(obj))->yVel = NI(RAND(4, 6));
        p = &PX(i);
        if (NLT(PE(p)->xVel, N(0))) PE(&PX(obj))->xVel = NI(-1 * RAND(4, 6));
        else PE(&PX(obj))->xVel = NI(RAND(4, 6));
        PE(&PX(obj))->cost = 0;
        PE(&PX(obj))->forSale = 0;
        PE(p)->hasGun = 0;
    }
}

/* ---- messages ---------------------------------------------------------------------------------------------- */
/* oShopkeeper.message2: an instance array that index assignments (message2[k] = ...) extend; the runner keeps the
   parts beyond the ones written (Observed: build/trace/g_p5_shop_s96 record 176, "PRESS P TO PURCHASE.P": part 3,
   the pay key, left from the kissing parlour's welcome). Per shopkeeper, by id; Create's message2 = "" (no parts) */
#define SKM_N 8
static struct skmsg { int32_t id; int8_t n; uint8_t hl; char part[5][28]; } skm[SKM_N];
static int skm_next;

static struct skmsg *sk_msg(int i)
{
    struct skmsg *m;
    int k;
    for (k = 0; k < SKM_N; k++)
        if (skm[k].id == PX(i).id && skm[k].n >= 0) return &skm[k];
    m = &skm[skm_next];
    skm_next = (skm_next + 1) % SKM_N;
    m->id = PX(i).id;
    m->n = 0;
    m->hl = 0;
    return m;
}

static void sk_part(struct skmsg *m, int k, const char *s)
{
    int j;
    for (j = 0; s[j] && j < 27; j++) m->part[k][j] = s[j];
    m->part[k][j] = 0;
    if (m->n < k + 1) m->n = (int8_t)(k + 1);
}

/* trMessages(message1, message2, messageHighlights ("": none), message2Highlights, 200) of shopkeeper i */
static void sk_show(struct skmsg *m, const char *const *m1, int n1)
{
    const char *p2[5];
    int k;
    for (k = 0; k < m->n; k++) p2[k] = m->part[k];
    if (m->n == 0) p2[0] = "";
    pmsg_tr(m1, n1, 0, p2, m->n, m->n ? 1u << m->hl : 0, 200);
}

/* the pay key's name (scrGetKey(global.keyPayVal): the runner's keyboard setting, P) */
#define PAY_KEY "P"

/* scripts/scrGetName: round(random_range(1, 32)) */
static const char *const sk_names[32] = {
    "AHKMED", "TERRY", "SMITHY", "LEON", "ALI", "ELBERT", "KAO", "DUKE", "TONY", "GUERT", "PANCHO", "EARL", "IVAN",
    "OLLIE", "SPOONY", "BOB", "RUDY", "JIMBO", "TOR", "WILLY", "HAMISH", "LAZLO", "WANG", "HERBIE", "ANDY", "DONG",
    "LEMMY", "BARNEY", "LOU", "TARN", "SLASH", "BROM"
};

/* the item's buyMessage as its Create set it (objects/<o>/Create_0.gml: [desc + " FOR $", string(cost), end]; past
   level 2 scrShopItemsGen :187 makes it [shopDesc, " FOR $", string(cost), "."], the same text: each shopDesc + " FOR
   $" is the Create's first part) and the cost then (the item's cost: unchanged since) */
static const struct { int16_t obj; const char *a, *b; } buymsg[] = {
    { OBJ_oAnkh, "AN ANKH FOR $", "." }, { OBJ_oBombBag, "A BAG OF 3 BOMBS FOR $", "." },
    { OBJ_oBombBox, "A BOX OF 12 BOMBS FOR $", "." }, { OBJ_oBow, "BOW AND ARROWS FOR $", "." },
    { OBJ_oCapePickup, "A CAPE FOR $", "." }, { OBJ_oCompass, "A COMPASS FOR $", "." },
    { OBJ_oGloves, "CLIMBING GLOVES FOR $", "." }, { OBJ_oJetpack, "JETPACK FOR $", "." },
    { OBJ_oMachete, "A MACHETE FOR $", "." }, { OBJ_oMattock, "A MATTOCK FOR $", "." },
    { OBJ_oMitt, "PITCHER'S MITT FOR $", "." }, { OBJ_oParaPickup, "A PARACHUTE FOR $", "." },
    { OBJ_oPaste, "BOMB PASTE FOR $", "." }, { OBJ_oPistol, "A PISTOL FOR $", "." },
    { OBJ_oRopePile, "EXTRA ROPE FOR $", "." }, { OBJ_oShotgun, "A SHOTGUN FOR $", "." },
    { OBJ_oSpectacles, "SPECTACLES FOR $", "." }, { OBJ_oSpikeShoes, "SPIKE SHOES FOR $", "." },
    { OBJ_oSpringShoes, "SPRINGY SHOES FOR $", "." }, { OBJ_oTeleporter, "A TELEPORTER FOR $", "." },
    { OBJ_oWebCannon, "A WEB CANNON FOR $", "." },
    { OBJ_oDamsel, "I'LL LET YOU HAVE HER FOR $", "!" },                       /* oDamsel Create :13 */
    { OBJ_oJordans, "JORDANS FOR $50000!", 0 },
    { OBJ_oCrown, "I SHOULDN'T BE SELLING THIS!", 0 }, { OBJ_oKapala, "I SHOULDN'T BE SELLING THIS!", 0 },
    { OBJ_oSceptre, "I SHOULDN'T BE SELLING THIS!", 0 }, { OBJ_oUdjatEye, "I SHOULDN'T BE SELLING THIS!", 0 },
};

static int buy_message(int it, const char **part, char *num)
{
    unsigned k;
    for (k = 0; k < sizeof buymsg / sizeof buymsg[0]; k++)
        if (buymsg[k].obj == PX(it).obj) {
            part[0] = buymsg[k].a;
            if (!buymsg[k].b) return 0;                                    /* a plain string */
            part[1] = pmsg_num(PE(&PX(it))->cost, num);
            part[2] = buymsg[k].b;
            return 3;
        }
    part[0] = "";                                                          /* oItem's default buyMessage: "" */
    return 0;
}


/* objects/oShopkeeper/Step_0.gml */
static void shopkeeper_step(int i)
{
    struct pin *p, *q;
    int c = PL.idx;
    double dist;
    pen_parent_step(i);
    p = &PX(i);
    {
        pos x = p->x, y = p->y;
        view_read();
        if (!(PGTI(x, PW.xview - 20) && PLTI(x, PW.xview + 320 + 4) && PGTI(y, PW.yview - 20) &&
              PLTI(y, PW.yview + 240 + 4)))
            return;
    }
    q = &PX(c);
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    if (!PE(p)->held) PE(p)->yVel += PE(p)->myGrav;
    if (NGT(PE(p)->yVel, N(8))) PE(p)->yVel = N(8);
    PE(p)->colLeft = PE(p)->colRight = PE(p)->colBot = PE(p)->colTop = 0;
    if (isCollisionLeft(i, 1)) PE(p)->colLeft = 1;
    if (isCollisionRight(i, 1)) PE(p)->colRight = 1;
    if (isCollisionBottom(i, 1)) PE(p)->colBot = 1;
    if (isCollisionTop(i, 1)) PE(p)->colTop = 1;
    if (PE(p)->colBot && PE(p)->status != S_STUNNED) PE(p)->yVel = 0;
    if (PEN(p)->throwCount > 0) PEN(p)->throwCount -= 1;
    if (PE(p)->status >= S_STUNNED) {                                      /* :26 crushed */
        if (CP(X(i) + 8, Y(i) + 12, OBJ_oSolid)) {
            scrCreateBlood(i, p->x + PI(8), p->y + PI(8), 3);
            snd_play(SND_xcavemandie);                                         /* :31 */
            p = &PX(i);
            if (PE(p)->hp > 0) {
                PG.shopkeepers += 1;
                PG.kills += 1;
            }
            G.murderer = 1;
            pin_destroy(i);
        }
    } else if (!PE(p)->held && CP(X(i) + 8, Y(i) + 8, OBJ_oSolid)) {
        scrCreateBlood(i, p->x + PI(8), p->y + PI(8), 3);
        snd_play(SND_xcavemandie);                                             /* :45 */
        p = &PX(i);
        if (PE(p)->hp > 0) {
            PG.shopkeepers += 1;
            PG.kills += 1;
        }
        G.murderer = 1;
        drop_gun(i);
        pin_destroy(i);
    }
    p = &PX(i);
    if (PE(p)->status != S_DEAD && PE(p)->status != S_STUNNED && PE(p)->hp < 1) PE(p)->status = S_DEAD;
    dist = distance_to_object_p(i, OBJ_oPlayer1);
    if ((PE(p)->status == S_IDLE || PE(p)->status == S_FOLLOW) && PL.holdItem != NOONE &&     /* :73 */
        PE(&PX(PL.holdItem))->cost > 0) {
        struct skmsg *m = sk_msg(i);
        const char *m1[3];
        char b[12];
        int n1 = buy_message(PL.holdItem, m1, b);
        sk_part(m, 0, "PRESS ");
        sk_part(m, 1, PAY_KEY);
        sk_part(m, 2, " TO PURCHASE.");
        m->hl = 1;
        sk_show(m, m1, n1);
        p = &PX(i);
    }
    if (PE(p)->status == S_PATROL || PE(p)->status == S_WALK) {                /* :96 */
        if (!PL.dead && DLT(distance_to_object_p(i, OBJ_oPlayer1), 64) && DLT(PTOD(q->y) - (Y(i) + 8), 16))
            PE(p)->status = S_ATTACK;
        else if (DLT(dabs(PTOD(q->x) - (X(i) + 8)), 4))
            PE(p)->status = S_ATTACK;
    }
    if (PE(p)->status == S_IDLE) {                                         /* :108 */
        PEN(p)->bounced = 0;
        if (PE(p)->colLeft) pin_setx(p, p->x + (PI(1)));
        if (PE(p)->colRight) pin_setx(p, p->x - (PI(1)));
        if (PE(p)->colLeft && PE(p)->colRight) PE(p)->status = S_ATTACK;
        PE(p)->facing = DLT(PTOD(q->x), X(i) + 8) ? E_LEFT : E_RIGHT;
        if (NLT(PE(p)->yVel, N(0)) && PE(p)->colTop) PE(p)->yVel = 0;
        if (G.murderer || G.thiefLevel > 0)
            PE(p)->status = S_PATROL;
        else if (!PEN(p)->welcomed && scrGetRoomX(PFLOOR(q->x)) == scrGetRoomX(PFLOOR(p->x)) &&
                 scrGetRoomY(PFLOOR(q->y)) == scrGetRoomY(PFLOOR(p->y)))
        {
            /* :136 the welcome message: scrGetName()'s random_range(1, 32) (one draw) except the Ankh shop's;
               style "Bomb" never equals tr("BOMB"): the bomb shop says SUPPLY SHOP */
            struct skmsg *m = sk_msg(i);
            const char *m1[3] = { "WELCOME TO ", 0, 0 };
            int n1 = 3;
            if (PE(p)->style != SHOP_ANKH) m1[1] = sk_names[dround(1 + prandom(31)) - 1];
            switch (PE(p)->style) {
            case SHOP_WEAPON: m1[2] = "'S ARMORY!"; break;
            case SHOP_CLOTHING: m1[2] = "'S CLOTHING SHOP!"; break;
            case SHOP_RARE: m1[2] = "'S SPECIALTY SHOP!"; break;
            case SHOP_CRAPS: m1[2] = "'S DICE HOUSE!"; break;
            case SHOP_KISSING: m1[2] = "'S KISSING PARLOR!"; break;
            case SHOP_ANKH: m1[0] = "I HAVE SOMETHING SPECIAL..."; n1 = 1; break;
            default: m1[2] = "'S SUPPLY SHOP!"; break;
            }
            if (PE(p)->style == SHOP_CRAPS) {                                  /* :144 */
                char b[12];
                sk_part(m, 0, "PRESS ");
                sk_part(m, 1, PAY_KEY);
                sk_part(m, 2, " TO BET $");
                sk_part(m, 3, pmsg_num(1000 + G.currLevel * 500, b));
                sk_part(m, 4, ".");
                m->hl = 1;
            } else if (PE(p)->style == SHOP_KISSING) {                         /* :158 */
                char b[12];
                sk_part(m, 0, "$");
                sk_part(m, 1, pmsg_num(10000 + 5000 * (G.currLevel - 2), b));   /* getKissValue() */
                sk_part(m, 2, " A KISS. PRESS ");
                sk_part(m, 3, PAY_KEY);
                m->hl = 3;
            } else
                m->n = 0;                                                      /* message2 = "" */
            sk_show(m, m1, n1);                                                /* :177 */
            PEN(p)->welcomed = 1;
        }
        if (PE(p)->style == SHOP_CRAPS) {                                  /* :185 */
            if (instance_number_p(OBJ_oDice) == 2 && PL.bet > 0) pitems_world(7010, i, 0);   /* P7 hook */
            /* else global.diceRolled = false */
        } else if (PL.holdItem != NOONE) {
            int obj = PL.holdItem;
            if (PE(&PX(obj))->cost > 0) {
                if (scrGetRoomX(PFLOOR(q->x)) == scrGetRoomX(PFLOOR(p->x)) &&
                    scrGetRoomY(PFLOOR(q->y)) == scrGetRoomY(PFLOOR(p->y)))
                    PE(p)->status = S_FOLLOW;
            }
        }
    } else if (PE(p)->status == S_FOLLOW) {                                /* :262 */
        double iv;
        p->ispd = (img_t)0.5;
        if (isCollisionLeft(i, 1) || isCollisionRight(i, 1)) PE(p)->facing = PE(p)->facing == E_LEFT ? E_RIGHT : E_LEFT;
        if (PEN(p)->turnTimer > 0) PEN(p)->turnTimer -= 1;
        else if (DLT(dabs(PTOD(q->y) - (Y(i) + 8)), 8) && isCollisionBottom(i, 1) && DGT(dist, 16)) {
            PE(p)->facing = DLT(PTOD(q->x), X(i)) ? E_LEFT : E_RIGHT;
            PEN(p)->turnTimer = 10;
        }
        iv = dist / 16 * 1.5;
        PE(p)->xVel = PE(p)->facing == E_LEFT ? ND(-iv) : ND(iv);
        if (NLT(PE(p)->xVel, N(-3))) PE(p)->xVel = N(-3);
        if (NGT(PE(p)->xVel, N(3))) PE(p)->xVel = N(3);
        if (DLT(dist, 12) || DLT(PTOD(q->y), Y(i))) PE(p)->xVel = 0;
        if (PL.holdItem != NOONE) {
            if (PE(&PX(PL.holdItem))->cost == 0) PE(p)->status = S_IDLE;
        } else
            PE(p)->status = S_IDLE;
    } else if (PE(p)->status == S_PATROL) {                                /* :295 */
        PEN(p)->bounced = 0;
        if (NLT(PE(p)->yVel, N(0)) && isCollisionTop(i, 1)) PE(p)->yVel = 0;
        if (PE(p)->colBot && PE(p)->counter > 0) PE(p)->counter -= 1;
        if (PE(p)->counter < 1) {
            PE(p)->facing = (int16_t)RAND(0, 1);
            PE(p)->status = S_WALK;
        }
    } else if (PE(p)->status == S_WALK) {                                  /* :307 */
        p->ispd = (img_t)0.5;
        if (isCollisionLeft(i, 1) || isCollisionRight(i, 1)) PE(p)->facing = PE(p)->facing == E_LEFT ? E_RIGHT : E_LEFT;
        if (PE(p)->facing == E_LEFT) {
            if (!CPn(X(i) - 1, Y(i), OBJ_oSolid, i)) {
                PE(p)->status = S_PATROL;
                PE(p)->counter = (int16_t)RAND(20, 50);
                PE(p)->xVel = 0;
            }
            PE(p)->xVel = N(-1.5);
        } else {
            if (!CPn(X(i) + 16, Y(i), OBJ_oSolid, i)) {
                PE(p)->status = S_PATROL;
                PE(p)->counter = (int16_t)RAND(20, 50);
                PE(p)->xVel = 0;
            }
            PE(p)->xVel = N(1.5);
        }
        if (RAND(1, 100) == 1) {
            PE(p)->status = S_PATROL;
            PE(p)->counter = (int16_t)RAND(20, 50);
            PE(p)->xVel = 0;
        }
    } else if (PE(p)->status == S_ATTACK) {                                /* :340 */
        p->ispd = 1;
        if (!PEN(p)->angered) {
            int16_t w[PIN_MAX];
            int n = pw_with(OBJ_oItem, w, PIN_MAX), k;
            for (k = 0; k < n; k++) {
                PE(&PX(w[k]))->cost = 0;
                PE(&PX(w[k]))->forSale = 0;
            }
            p = &PX(i);
            PEN(p)->angered = 1;
        }
        if (PEN(p)->turnTimer > 0) PEN(p)->turnTimer -= 1;
        else if (DLT(dabs(PTOD(q->y) - (Y(i) + 8)), 8) && isCollisionBottom(i, 1) && DGT(dist, 16)) {
            PE(p)->facing = DLT(PTOD(q->x), X(i)) ? E_LEFT : E_RIGHT;
            PEN(p)->turnTimer = 20;
        }
        if (isCollisionLeft(i, 1) || isCollisionRight(i, 1)) PE(p)->facing = PE(p)->facing == E_LEFT ? E_RIGHT : E_LEFT;
        PE(p)->xVel = PE(p)->facing == E_LEFT ? N(-3) : N(3);
        if (PE(p)->hasGun) {
            if (PEN(p)->firing > 0) PEN(p)->firing -= 1;
            else if (DLT(dabs(PTOD(q->y) - (Y(i) + 8)), 32)) {
                if (PE(p)->facing == E_LEFT && DLT(PTOD(q->x), X(i) + 8) && DLT(dist, 96)) shoot(i, 1);
                p = &PX(i);
                q = &PX(c);
                if (PE(p)->facing == E_RIGHT && DGT(PTOD(q->x), X(i) + 8) && DLT(dist, 96)) shoot(i, 0);
                p = &PX(i);
                q = &PX(c);
            }
        }
        if (DGT(PTOD(q->y), Y(i)) && DLT(dabs(PTOD(q->x) - (X(i) + 8)), 64)) {   /* :418 jump */
        } else if ((PE(p)->facing == E_LEFT && CPn(X(i) - 16, Y(i), OBJ_oSolid, i)) ||
                   (PE(p)->facing == E_RIGHT && CPn(X(i) + 32, Y(i), OBJ_oSolid, i))) {
            if (PE(p)->colBot && !isCollisionTop(i, 4)) PE(p)->yVel = NI(-1 * RAND(7, 8));
        } else if (DLE(PTOD(q->y), Y(i) + 16) &&
                   ((PE(p)->facing == E_LEFT && !CPn(X(i) - 16, Y(i) + 16, OBJ_oSolid, i)) ||
                    (PE(p)->facing == E_RIGHT && !CPn(X(i) + 32, Y(i) + 16, OBJ_oSolid, i)))) {
            if (PE(p)->colBot && !isCollisionTop(i, 4)) PE(p)->yVel = NI(-1 * RAND(7, 8));
        }
        if (!PE(p)->colBot && DGT(PTOD(q->y), Y(i) + 8)) PE(p)->xVel = PE(p)->facing == E_LEFT ? N(-1.5) : N(1.5);
        if (PL.dead) PE(p)->status = S_WALK;
    } else if (PE(p)->status == S_STUNNED) {                               /* :447 */
        if (PE(p)->colBot) pin_set_sprite(i, GSPR_sShopStunL);
        else if (PEN(p)->bounced) pin_set_sprite(i, NLT(PE(p)->yVel, N(0)) ? GSPR_sShopBounceL : GSPR_sShopFallL);
        else pin_set_sprite(i, NLT(PE(p)->xVel, N(0)) ? GSPR_sShopDieLL : GSPR_sShopDieLR);
        if (PE(p)->colBot && !PEN(p)->bounced) {
            PEN(p)->bounced = 1;
            scrCreateBlood(i, p->x + PI(8), p->y + PI(8), 3);
            p = &PX(i);
        }
        if (PE(p)->held || PE(p)->colBot) {
            if (PE(p)->counter > 0) PE(p)->counter -= 1;
            else if (PE(p)->hp > 0) {
                PE(p)->status = S_ATTACK;
                if (PE(p)->held) {
                    PE(p)->held = 0;
                    PL.holdItem = NOONE;
                    PL.pickupItemType = T_NONE;
                }
            }
        }
    } else if (PE(p)->status == S_DEAD) {                                  /* :479 */
        if (!PEN(p)->edead) {
            int n, k;
            PG.shopkeepers += 1;
            PG.kills += 1;
            G.murderer = 1;
            n = RAND(1, 4);
            for (k = 0; k < n; k++) {
                int obj = pin_create(PX(i).x + PI(8), PX(i).y + PI(8), OBJ_oGoldNugget);
                PE(&PX(obj))->yVel = N(-1);
                {
                    int a = RAND(1, 3), b = RAND(1, 3);
                    PE(&PX(obj))->xVel = NI(a - b);
                }
            }
            snd_play(SND_xcavemandie);                                         /* :529 */
            p = &PX(i);
            PEN(p)->edead = 1;
        }
        pin_set_sprite(i, GSPR_sShopDieL);
        if (NGT(NABS(PE(p)->xVel), N(0)) || NGT(NABS(PE(p)->yVel), N(0))) PE(p)->status = S_STUNNED;
    }
    if (PE(p)->status >= S_STUNNED) {                                      /* :532 */
        drop_gun(i);
        p = &PX(i);
        scrCheckCollisions(i);
        if (NEQ(PE(p)->xVel, N(0)) && NEQ(PE(p)->yVel, N(0)) && PE(p)->hp < 1) PE(p)->status = S_DEAD;
    }
    if (NGT(PE(p)->xVel, N(0))) PE(p)->xVel -= N(0.1);
    if (NLT(PE(p)->xVel, N(0))) PE(p)->xVel += N(0.1);
    if (NLT(NABS(PE(p)->xVel), N(0.5))) PE(p)->xVel = 0;
    if (PE(p)->status < S_STUNNED && PE(p)->status != S_THROW)
        pin_set_sprite(i, NGT(NABS(PE(p)->xVel), N(0)) ? GSPR_sShopRunLeft : GSPR_sShopLeft);
    if (PE(p)->held) pin_set_sprite(i, PE(p)->hp > 0 ? GSPR_sShopHeldL : GSPR_sShopDHeldL);
}

int pshop_step(int i)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oShopkeeper: shopkeeper_step(i); return 1;
    case OBJ_oBullet:                                                  /* objects/oBullet/Step_0.gml */
        pin_setx(p, PADDV(p->x, PE(p)->xVel));
        pin_sety(p, PADDV(p->y, PE(p)->yVel));
        return 1;
    }
    return 0;
}

int pshop_alarm(int i, int a)
{
    switch (PX(i).obj) {
    case OBJ_oShopkeeper: if (a == 0) PEN(&PX(i))->whipped = 0; return 1;
    case OBJ_oBullet: if (a == 0) PE(&PX(i))->safe = 0; return 1;
    }
    return 0;
}

int pshop_animend(int i)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oShopkeeper:
        if (p->spr == GSPR_sShopThrowL) {
            PE(p)->status = S_ATTACK;
            pin_set_sprite(i, GSPR_sShopLeft);
        }
        return 1;
    case OBJ_oShotgunBlastLeft: case OBJ_oShotgunBlastRight:
        pin_destroy(i);
        return 1;
    }
    return 0;
}

/* objects/oShopkeeper/Collision_oCharacter.gml */
static void shop_hit_player(int i, int c)
{
    struct pin *p = &PX(i), *o = &PX(c);
    if (PE(p)->status == S_IDLE || PE(p)->status == S_FOLLOW || PE(p)->status == S_STUNNED || PE(p)->status == S_DEAD || PE(p)->hp < 1 ||
        PL.dead || PL.stunned || DGT(dabs(PTOD(o->x) - (X(i) + 8)), 8)) {
    } else if (!PL.dead && !PL.stunned && (PL.state == JUMPING || PL.state == FALLING) && DLT(PTOD(o->y), Y(i) + 5) &&
               !PL.swimming) {
        if (PE(p)->status < S_STUNNED) {
            PE(o)->yVel = N(-6) - NMUL(N(0.2), PE(o)->yVel);
            if (PG.hasSpikeShoes) {
                PE(p)->hp -= (int16_t)(3 * dceil(PL.fallTimer / 16.0));
                pin_create(o->x, o->y + PI(8), OBJ_oBlood);
                p = &PX(i);
                o = &PX(c);
            } else
                PE(p)->hp -= (int16_t)(1 * dceil(PL.fallTimer / 16.0));
            PL.fallTimer = 0;
            PE(p)->status = S_STUNNED;
            PE(p)->counter = PEN(p)->stunTime;
            PE(p)->yVel = N(-6);
            if (DLT(PTOD(o->x), X(i) + 8)) PE(p)->xVel += N(1);
            else PE(p)->xVel -= N(1);
            p->ispd = (img_t)0.5;
            snd_play(SND_xhit);                                                /* :19 */
        }
    } else if (PL.invincible == 0 && PE(p)->status < S_STUNNED) {
        if (CP(X(i) + 8, Y(i) - 4, OBJ_oSolid)) {
            PL.blink = 30;
            PL.invincible = 30;
            PE(o)->xVel = DLT(PTOD(o->x), X(i)) ? N(-6) : N(6);
            pin_create(o->x, o->y, OBJ_oBlood);
            if (PG.plife > 0) PG.plife -= 1;
            snd_play(SND_xhurt);                                               /* :39 */
        } else if (PE(p)->status != S_THROW) {
            PE(p)->status = S_THROW;
            PE(p)->xVel = 0;
            if (DGT(PTOD(o->x), X(i) + 8)) {
                PE(p)->facing = E_RIGHT;
                pin_set_sprite(i, GSPR_sShopThrowL);
                pin_setx(o, p->x);
                pin_sety(o, p->y);
                PE(o)->yVel = N(-6);
                PE(o)->xVel = N(6);
            } else {
                PE(p)->facing = E_LEFT;
                pin_set_sprite(i, GSPR_sShopThrowL);
                pin_setx(o, p->x + PI(16));
                pin_sety(o, p->y);
                PE(o)->yVel = N(-6);
                PE(o)->xVel = N(-6);
            }
            PL.stunned = 1;
            PL.bounced = 0;
            PL.wallHurt = 1;
            if (PL.holdItem != NOONE) {
                if (PX(PL.holdItem).type == T_GOLDIDOL) pin_sety(&PX(PL.holdItem), PX(PL.holdItem).y - (PI(8)));
                scrDropItem(PE(o)->xVel, PE(o)->yVel);
            }
        }
    }
}

/* objects/oBullet/Collision_*.gml */
static void bullet_collision(int b, int other)
{
    struct pin *p = &PX(b), *o = &PX(other);
    int oo = o->obj;
    if (obj_is(oo, OBJ_oCharacter)) {
        if (o->spr != GSPR_sPExit && o->spr != GSPR_sDamselExit && o->spr != GSPR_sTunnelExit) {
            if (PG.plife > 0) PG.plife -= 4;
            PE(o)->xVel = PE(p)->xVel;
            PE(o)->yVel = N(-4);
            pin_create(o->x, o->y, OBJ_oBlood);
            PL.stunned = 1;
            PL.stunTimer = 20;
            snd_play(SND_xhurt);                                               /* Collision_oCharacter :19 */
            pin_destroy(b);
        }
    } else if (oo == OBJ_oDamsel) {
        if (!o->invincible) {
            if (PEN(o)->bloodLeft > 0) {
                scrCreateBlood(other, P(X(other) + sprw(other) / 2.0), P(Y(other) + sprh(other) / 2.0), 1);
                o = &PX(other);
                if (PE(o)->hp < 0) PEN(o)->bloodLeft -= 1;
            }
            if (PE(o)->held) {
                PE(o)->held = 0;
                PL.holdItem = NOONE;
            }
            PE(o)->hp -= 4;
            PE(o)->yVel = N(-6);
            PE(o)->status = 2;
            PE(o)->counter = 120;
            PE(o)->xVel = NMUL(PE(&PX(b))->xVel, N(0.3));
            snd_play(SND_xdamsel);                                             /* Collision_oDamsel :21 */
            pin_destroy(b);
        }
    } else if (obj_is(oo, OBJ_oEnemy)) {
        if (!PE(p)->safe) {
            if (oo == OBJ_oYetiKing || oo == OBJ_oTombLord) {
                PE(o)->xVel = NMUL(PE(p)->xVel, N(0.5));
                PE(o)->yVel = N(-2);
            } else {
                PE(o)->xVel = PE(p)->xVel;
                PE(o)->yVel = N(-4);
            }
            PE(o)->hp -= 4;
            if ((o->type == T_CAVEMAN || o->type == T_SHOPKEEPER || oo == OBJ_oYeti || oo == OBJ_oHawkman) &&
                PE(o)->status != 99) {
                PE(o)->status = 98;
                PE(o)->counter = 20;
            }
            if (PEN(o)->bloodLeft > 0) {
                if (o->obj != OBJ_oSkeleton)
                    scrCreateBlood(other, P(X(other) + sprw(other) / 2.0), P(Y(other) + sprh(other) / 2.0), 1);
                o = &PX(other);
                if (PE(o)->hp < 0) PEN(o)->bloodLeft -= 1;
            }
            snd_play(SND_xhit);                                                /* Collision_oEnemy :34 */
            pin_destroy(b);
        }
    } else if (obj_is(oo, OBJ_oSolid)) {
        pin_create(p->x, p->y, OBJ_oSmokePuff);
        snd_play(SND_xhit);                                                    /* Collision_oSolid :2 */
        pin_destroy(b);
    }
}

int pshop_collision(int self, int other)
{
    int so = PX(self).obj, oo = PX(other).obj;
    if (so == OBJ_oBullet) {
        bullet_collision(self, other);
        return 1;
    }
    if (so != OBJ_oShopkeeper) return 0;
    if (obj_is(oo, OBJ_oCharacter)) shop_hit_player(self, other);
    else if (oo == OBJ_oShotgun) {                                     /* Collision_oShotgun.gml */
        struct pin *p = &PX(self);
        if (PE(p)->hp > 0 && PE(p)->status == S_ATTACK && !PE(p)->hasGun) {
            if (PE(&PX(other))->held) {
                PL.holdItem = NOONE;
                PL.pickupItemType = T_NONE;
                PE(&PX(other))->held = 0;
            }
            pin_destroy(other);
            PE(&PX(self))->hasGun = 1;
        }
    } else {                                                           /* Collision_oWhip / oWhipPre (the whip) */
        struct pin *p = &PX(self);
        if (!PEN(p)->whipped) {
            if (whip_machete(other)) {                                         /* :3 other.type == "Machete" */
                PE(p)->hp -= (int16_t)whip_damage(other);
                if (PEN(p)->bloodLeft > 0) {
                    scrCreateBlood(self, P(X(self) + sprw(self) / 2.0), P(Y(self) + sprh(self) / 2.0), 1);
                    p = &PX(self);
                    if (PE(p)->hp < 0) PEN(p)->bloodLeft -= 1;
                }
            }
            PE(p)->yVel = N(-2);
            PE(p)->xVel = DLT(X(other), X(self)) ? N(1) : N(-1);
            snd_play(SND_xhit);                                                /* :15 */
            PEN(p)->whipped = 1;
            PE(p)->alarm[0] = 10;
            PE(p)->status = S_ATTACK;
        }
    }
    return 1;
}

int pshop_draw(int i)
{
    int o = PX(i).obj;
    return o == OBJ_oShopkeeper || o == OBJ_oBullet;                   /* draws only */
}

/* objects/oPlayer1/Step_0.gml :1327-1427: Pay in a shop (no bet / kiss parts beyond the kissing parlour) */
void pshop_pay(int i)
{
    int n = 0, shp;
    struct pin *p = &PX(i);
    if (!(isInShop(PFLOOR(p->x), PFLOOR(p->y)) && instance_exists_p(OBJ_oShopkeeper))) return;
    if (PL.holdItem != NOONE) {
        int h = PL.holdItem;
        if (PE(&PX(h))->cost <= 0) {
        } else if (PE(&PX(h))->cost > PG.money) {
            PE(&PX(h))->held = 0;
            PL.holdItem = NOONE;
            PL.pickupItemType = T_NONE;
            n = 1;
            pmsg_player_str("YOU HAVEN'T GOT ENOUGH MONEY!", "", 80);                 /* :1346 */
        } else {
            PG.money -= PE(&PX(h))->cost;
            scrStealItem();
            pmsg_player_str("THANK YOU!", "", 80);                                    /* :1355 */
        }
    }
    shp = instance_first_p(OBJ_oShopkeeper);                           /* oShopkeeper.style: the oldest */
    if (G.blackMarket) PUNTR(7011);
    else if (PE(&PX(shp))->style == SHOP_CRAPS) {
        if (G.thiefLevel > 0 || G.murderer) {
        } else if (PL.bet == 0 && PG.money >= (1000 + G.currLevel * 500)) {
            char b[12];
            const char *m1[3] = { " YOU BET $", 0, "!" };
            PL.bet = 1000 + G.currLevel * 500;
            PG.money -= 1000 + G.currLevel * 500;
            m1[1] = pmsg_num(1000 + G.currLevel * 500, b);
            pmsg_player(m1, 3, (const char *const[]){ "NOW ROLL THE DICE!" }, 0, 200);   /* :1374 */
        } else if (PL.bet > 0)
            pmsg_player_str("ONE BET AT A TIME!", "PLEASE ROLL THE DICE!", 200);       /* :1380 */
        else {
            char b[12];
            const char *m1[3] = { "YOU NEED $", 0, " TO BET!" };
            m1[1] = pmsg_num(1000 + G.currLevel * 500, b);
            pmsg_player(m1, 3, (const char *const[]){ "" }, 0, 200);       /* :1386 */
        }
    }
    if (PE(&PX(shp))->style == SHOP_KISSING && DLT(distance_to_object_p(i, OBJ_oDamsel), 16)) {
        int obj = instance_nearest_p(PTOD(p->x), PTOD(p->y), OBJ_oDamsel);
        int kiss = 10000 + 5000 * (G.currLevel - 2);                   /* getKissValue() */
        if (G.thiefLevel > 0 || G.murderer || !PE(&PX(obj))->forSale) {
        } else if (n == 0 && PG.money >= kiss) {
            if (PE(&PX(obj))->forSale && !PE(&PX(obj))->held) {
                PE(&PX(obj))->status = 6;                                    /* KISS */
                pin_set_sprite(obj, GSPR_sDamselKissL);
                PG.money -= kiss;
                PG.plife += 1;
                pmsg_player_str("NOW AIN'T SHE SWEET!", "", 200);                     /* :1416 */
            }
        } else {
            char b[12];
            const char *m1[3] = { "YOU NEED $", 0, "!" };
            m1[1] = pmsg_num(n == 0 ? kiss : PE(&PX(obj))->cost, b);
            pmsg_player(m1, 3, (const char *const[]){ "GET OUTTA HERE, DEADBEAT!" }, 0, 200);   /* :1424 */
        }
    }
}
