/* Events of the objects the player meets in P4 (no enemies): items, treasures, bombs, ropes, the whip, arrow
 * traps, push blocks, terrain destruction, particles, oGame and oLevel. refs/hd/src/objects/<obj>/<event>.gml,
 * line numbers in comments. `action_inherited()` is written out as a call of the parent's event.
 * GML argument order: last argument first, so instance_create(x + rand.., y + rand.., o) draws y's numbers first.
 * Objects or branches P4 does not reach set play_untranslated (codes 1xxx).
 */
#include "pint.h"
#include "penemy.h"                  /* P5 hooks: enemies, damsel, shop (each marked "P5 hook") */

struct pgame PGAME;
struct plevel PLEV;
static int play_in_gen_init;              /* pobj_init_from_gen runs a Create event for its variables only */

const char *const ptype_names[T_COUNT] = {
    "", "Rock", "Jar", "Skull", "Fish Bone", "Arrow", "Bomb", "Rope", "Chest", "Crate", "Gold Idol", "Key",
    "Locked Chest", "Damsel", "Machete", "Mattock", "Mattock Head", "Pistol", "Web Cannon", "Teleporter", "Shotgun",
    "Bow", "Flare", "Sceptre", "Crystal Skull", "Lamp", "Bomb Bag", "Bomb Box", "Rope Pile", "Paste", "Parachute",
    "Spectacles", "Gloves", "Mitt", "Compass", "Spring Shoes", "Spike Shoes", "Jordans", "Cape", "Jetpack",
    "Udjat Eye", "Ankh", "Crown", "Kapala", "Flare Crate", "Dice", "Bones",
    "Gold Chunk", "Gold Nugget", "Gold Bar", "Gold Bars", "Emerald", "Big Emerald", "Sapphire", "Big Sapphire",
    "Ruby", "Big Ruby", "Diamond", "Whip", "Arrow Trap", "(other)",
    "NONE", "Snake", "Spider", "Giant Spider", "Caveman", "Skeleton", "Shopkeeper", "Scarab",
};

/* ---- Create events ------------------------------------------------------------------------------------- */
static void make_active(struct pin *p) { p->xVel = p->yVel = p->xAcc = p->yAcc = 0; }

/* objects/oItem/Create_0.gml */
void create_item(struct pin *p)
{
    p->type = T_NONE;
    p->active = 1;
    p->New = 1;
    p->held = 0;
    p->myGrav = N(0.6);
    p->armed = 0;
    p->trigger = 0;
    p->safe = 0;
    p->heavy = 0;
    p->value = 0;
    p->colBot = 0;
    p->canPickUp = 1;
    p->bounceFactor = N(0.5);
    p->frictionFactor = N(0.3);
    p->breakPieces = 1;
    p->cost = 0;
    p->forSale = 0;
    p->inDiceHouse = 0;
    p->cimg = 0;
    p->stuck = 0;
    p->sticky = 0;
    p->enemyID = NOONE;
    p->depth = G.hasSpectacles ? 51 : 101;
    p->colLeft = p->colRight = p->colBot = p->colTop = 0;
}

/* objects/oTreasure/Create_0.gml */
static void create_treasure(struct pin *p)
{
    p->type = T_NONE;
    p->held = 0;
    p->myGrav = N(0.6);
    p->trigger = 0;
    p->value = 0;
    p->canCollect = 0;
    p->yOff = 4;
    p->xVel = p->yVel = 0;
    p->state = 1;                                                              /* ACTIVE */
    p->colLeft = p->colRight = p->colBot = 0;
}

static void treasure(int i, int type, int l, int t, int r, int b, int value, int canCollect, int alarm0, int yOff)
{
    struct pin *p = &PX(i);
    create_treasure(p);
    p->type = (int16_t)type;
    make_active(p);
    setCollisionBounds(i, l, t, r, b);
    if (yOff) p->yOff = (int16_t)yOff;
    if (alarm0) p->alarm[0] = alarm0;
    p->value = value;
    if (canCollect >= 0) p->canCollect = (uint8_t)canCollect;
}

static void item(int i, int type, int l, int t, int r, int b, int32_t cost)
{
    struct pin *p = &PX(i);
    create_item(p);
    p->type = (int16_t)type;
    make_active(p);
    setCollisionBounds(i, l, t, r, b);
    if (cost >= 0) p->cost = cost;
}

/* objects/oDetritus/Create_0.gml (returns 0 if it destroyed itself) */
int create_detritus(int i)
{
    struct pin *p = &PX(i);
    p->type = T_NONE;
    make_active(p);
    setCollisionBounds(i, -4, -4, 4, 4);
    p->life = N(60);
    p->grav = N(0.6);
    p->bounce = 1;
    p->dying = 0;
    p->invincible = 0;
    if (instance_number_p(OBJ_oDetritus) > 32) {
        pin_destroy(i);
        return 0;
    }
    return 1;
}

static void create_solid(struct pin *p)
{
    p->invincible = 0;
    p->shopWall = 0;
    p->type = T_NONE;
    p->cleanDeath = 0;
}

void ev_create(int i)
{
    struct pin *p = &PX(i);
    if (pen_create(i, play_in_gen_init) || pdam_create(i, play_in_gen_init) || pshop_create(i, play_in_gen_init) ||
        pitem_create(i, play_in_gen_init))
        return;                                                                /* P5 hook */
    switch (p->obj) {
    /* items */
    case OBJ_oRock: item(i, T_ROCK, -4, -4, 4, 4, -1); break;
    case OBJ_oJar:
        item(i, T_JAR, -4, -6, 4, 6, -1);
        p->breakPieces = 1;
        break;
    case OBJ_oSkull: item(i, T_SKULL, -4, -4, 4, 4, -1); break;
    case OBJ_oChest: case OBJ_oCrate: case OBJ_oLockedChest:
        item(i, p->obj == OBJ_oChest ? T_CHEST : p->obj == OBJ_oCrate ? T_CRATE : T_LOCKEDCHEST,
             -6, p->obj == OBJ_oLockedChest ? -2 : 0, 6, 8, -1);
        p->heavy = 1;
        p->yVel = 0;
        p->yAcc = N(0.2);
        break;
    case OBJ_oGoldIdol:
        item(i, T_GOLDIDOL, -4, -4, 4, 4, -1);
        p->trigger = 1;
        p->heavy = 1;
        p->value = 5000;
        break;
    case OBJ_oKey: item(i, T_KEY, -4, -4, 4, 4, 0); break;
    case OBJ_oBomb: item(i, T_BOMB, -4, -4, 4, 4, -1); break;
    case OBJ_oRopeThrow:
        item(i, T_ROPE, -4, -4, 4, 4, -1);
        p->armed = 0;
        p->falling = 0;
        p->fallCount = 0;
        p->px = 0;
        p->py = 0;
        break;
    case OBJ_oArrow:
        item(i, T_ARROW, -4, -4, 4, 4, -1);
        p->myGrav = N(0.2);
        break;
    case OBJ_oFishBone:
        item(i, T_FISHBONE, -4, -4, 4, 4, -1);
        p->myGrav = N(0.2);
        p->safe = 0;
        break;
    case OBJ_oBombBag: item(i, T_BOMBBAG, -6, -2, 6, 6, 2500); break;
    case OBJ_oBombBox: item(i, T_BOMBBOX, -6, -2, 6, 8, 10000); p->heavy = 1; break;
    case OBJ_oRopePile: item(i, T_ROPEPILE, -6, -5, 6, 5, 2500); break;
    case OBJ_oGloves: item(i, T_GLOVES, -6, -6, 6, 8, 8000); break;
    case OBJ_oSpectacles: item(i, T_SPECTACLES, -6, -6, 6, 6, 8000); break;
    case OBJ_oMattock: item(i, T_MATTOCK, -4, -6, 4, 6, 8000); break;
    case OBJ_oMachete: item(i, T_MACHETE, -4, -4, 4, 4, 7000); break;
    case OBJ_oPistol: item(i, T_PISTOL, -4, -4, 4, 4, 5000); break;
    case OBJ_oShotgun: item(i, T_SHOTGUN, -4, -4, 4, 4, 15000); break;
    case OBJ_oBow: item(i, T_BOW, -4, -4, 4, 4, 1000); p->ispd = 0; break;
    case OBJ_oWebCannon: item(i, T_WEBCANNON, -4, -4, 4, 4, 2000); break;
    case OBJ_oTeleporter: item(i, T_TELEPORTER, -4, -4, 4, 4, 10000); break;
    case OBJ_oMitt: item(i, T_MITT, -6, -6, 6, 8, 4000); break;
    case OBJ_oPaste: item(i, T_PASTE, -6, -2, 6, 6, 3000); break;
    case OBJ_oSpringShoes: item(i, T_SPRINGSHOES, -6, -6, 6, 6, 5000); break;
    case OBJ_oSpikeShoes: item(i, T_SPIKESHOES, -6, -6, 6, 6, 4000); break;
    case OBJ_oCompass: item(i, T_COMPASS, -6, -6, 6, 6, 3000); break;
    case OBJ_oParaPickup: item(i, T_PARACHUTE, -6, -6, 6, 6, 2000); break;
    case OBJ_oCapePickup: item(i, T_CAPE, -6, -6, 6, 6, 12000); break;
    case OBJ_oJetpack: item(i, T_JETPACK, -5, -5, 5, 8, 20000); p->heavy = 1; break;
    case OBJ_oMattockHead: item(i, T_MATTOCKHEAD, -6, -4, 6, 4, -1); break;
    case OBJ_oSceptre: item(i, T_SCEPTRE, -4, -4, 4, 4, 0); break;
    case OBJ_oDice:                                                            /* objects/oDice/Create_0.gml */
        item(i, T_DICE, -6, 0, 6, 8, -1);
        p->heavy = 1;
        if (!play_in_gen_init) PUNTR(1002);                                    /* value = rand(1, 6): P5 (shops) */
        break;
    case OBJ_oLampItem:
        item(i, T_LAMP, -4, -4, 4, 4, -1);
        p->trigger = 1;
        p->heavy = 1;
        p->value = 1000;
        break;
    /* treasures */
    case OBJ_oGoldChunk: treasure(i, T_GOLDCHUNK, -2, -2, 2, 2, 100, 1, 0, 2); break;
    case OBJ_oGoldNugget: treasure(i, T_GOLDNUGGET, -4, -4, 4, 4, 500, 1, 0, 0); break;
    case OBJ_oGoldBar: treasure(i, T_GOLDBAR, -4, -4, 4, 4, 500, 1, 0, 0); break;
    case OBJ_oGoldBars: treasure(i, T_GOLDBARS, -7, -8, 7, 8, 1000, 1, 0, 8); break;
    case OBJ_oEmerald: treasure(i, T_EMERALD, -2, -2, 2, 2, 200, -1, 20, 2); break;
    case OBJ_oSapphire: treasure(i, T_SAPPHIRE, -2, -2, 2, 2, 400, -1, 20, 2); break;
    case OBJ_oRuby: treasure(i, T_RUBY, -2, -2, 2, 2, 400, -1, 20, 2); break;
    case OBJ_oEmeraldBig: treasure(i, T_BIGEMERALD, -4, -4, 4, 4, 800, 0, 20, 0); break;
    case OBJ_oSapphireBig: treasure(i, T_BIGSAPPHIRE, -4, -4, 4, 4, 1200, 0, 20, 0); break;
    case OBJ_oRubyBig: treasure(i, T_BIGRUBY, -4, -4, 4, 4, 1600, 0, 20, 0); break;
    case OBJ_oDiamond: treasure(i, T_DIAMOND, -4, -4, 4, 4, 5000, 0, 20, 0); break;
    /* ropes */
    case OBJ_oRope:
        p->type = T_ROPE;
        make_active(p);
        setCollisionBounds(i, -4, -4, 4, 4);
        p->burnTimer = 0;
        break;
    case OBJ_oRopeTop:
        p->type = T_ROPE;
        make_active(p);
        setCollisionBounds(i, -4, -4, 4, 4);
        break;
    /* the whip */
    case OBJ_oWhip: p->type = T_WHIP; break;
    case OBJ_oWhipPre: p->type = T_WHIP; p->alarm[0] = 3; break;
    /* solids created in play */
    case OBJ_oPushBlock:
        create_solid(p);
        p->xVel = p->yVel = 0;
        p->myGrav = N(0.6);
        setCollisionBounds(i, 0, 0, 16, 16);
        if (G.cityOfGold) pin_set_sprite(i, GSPR_sGoldBlock);
        break;
    case OBJ_oArrowTrapTest: p->trapID = NOONE; break;
    /* effects */
    case OBJ_oExplosion:
        p->ispd = (img_t)0.8;
        scrShake(5);
        break;
    case OBJ_oBlood:
        if (!create_detritus(i)) break;
        p->ispd = (img_t)0.3;
        {
            double a = prandom(4);
            double b = prandom(4);
            p->xVel = ND(a - b);
        }
        p->yVel = ND(-1 - prandom(2));
        p->grav = NMUL(NI(RAND(1, 6)), N(0.1));
        p->invincible = 1;
        p->bounce = 0;
        p->collectible = 0;
        p->alarm[0] = 1;
        p->alarm[1] = 1;
        p->alarm[2] = 5;
        break;
    case OBJ_oFlame:
        if (!create_detritus(i)) break;
        p->ispd = (img_t)0.3;
        {
            double a = prandom(4);
            double b = prandom(4);
            p->xVel = ND(a - b);
        }
        p->yVel = ND(-1 - prandom(2));
        p->grav = NMUL(NI(RAND(1, 6)), N(0.1));
        p->alarm[0] = 2;
        p->alarm[1] = 50;
        break;
    case OBJ_oBloodTrail: p->ispd = (img_t)0.8; break;
    case OBJ_oFlameTrail: p->ispd = (img_t)0.4; break;
    case OBJ_oSmokePuff: p->yVel = N(0.1); p->yAcc = N(0.1); p->ispd = (img_t)0.4; break;
    case OBJ_oBurn: p->yVel = N(-0.1); p->yAcc = N(0.1); p->ispd = (img_t)0.4; break;
    case OBJ_oPoof: p->xVel = 0; p->yVel = 0; p->ispd = (img_t)0.4; break;
    case OBJ_oItemsGet: p->yVel = N(0.1); p->yAcc = N(0.1); p->ispd = (img_t)0.8; p->alarm[0] = 40; break;
    case OBJ_oBigCollect: p->alarm[0] = 30; pin_set_sprite(i, GSPR_sBigCollect); break;
    case OBJ_oRubble: case OBJ_oRubbleSmall: case OBJ_oRubbleDarkSmall:
        p->type = T_NONE;
        p->xVel = 0;
        p->yVel = 0;
        p->yAcc = N(0.6);
        break;
    case OBJ_oBone: PUNTR(1001); break;
    default:
        if (ptrans_create(i)) break;
        if (pobj[p->obj].ev & EV_CREATE)
            PUNTR(1000);
        break;
    }
}

/* the generator made these instances and ran their Create events (gen*.c): the play variables of those events */
void pobj_init_from_gen(int i)
{
    struct pin *p = &PX(i);
    play_cur_obj = p->obj;
    const struct inst *g = 0;
    (void)g;
    if (pen_create(i, 1) || pdam_create(i, 1) || pshop_create(i, 1) || pitem_create(i, 1)) return;   /* P5 hook */
    switch (p->obj) {
    case OBJ_oArrowTrapLeft: case OBJ_oArrowTrapLeftLit: case OBJ_oArrowTrapRight: case OBJ_oArrowTrapRightLit:
        create_solid(p);
        p->type = T_ARROWTRAP;
        p->fired = 0;
        p->xAct = 0;
        break;
    case OBJ_oPushBlock:
        create_solid(p);
        p->xVel = p->yVel = 0;
        p->myGrav = N(0.6);
        setCollisionBounds(i, 0, 0, 16, 16);
        break;
    case OBJ_oWeb:
        p->life = N(12);
        p->dying = 0;
        break;
    case OBJ_oBones:
        p->yVel = 0;
        p->yAcc = N(0.2);
        break;
    case OBJ_oLamp: case OBJ_oLampRed:
        p->ispd = (img_t)0.5;
        break;
    case OBJ_oDamsel:
        break;                                                                 /* removed (TRACE_NOENEMY) */
    default:
        if (obj_is(p->obj, OBJ_oSolid))
            create_solid(p);
        else if (obj_is(p->obj, OBJ_oItem) || obj_is(p->obj, OBJ_oTreasure) || p->obj == OBJ_oRubble ||
                 p->obj == OBJ_oRubbleSmall) {
            int a0 = p->alarm[0];
            play_in_gen_init = 1;
            ev_create(i);                                                      /* the RNG parts ran in gen */
            play_in_gen_init = 0;
            p->alarm[0] = a0;
        }
        break;
    }
}

/* ---- Destroy events -------------------------------------------------------------------------------------- */
/* x + 8 + rand(0, k) - rand(0, k), y likewise; y's numbers first */
static int rubble_at(int i, int obj, int k, int spr)
{
    struct pin *p = &PX(i);
    int ya = RAND(0, k), yb = RAND(0, k);
    int xa = RAND(0, k), xb = RAND(0, k);
    int r = pin_create(p->x + PI(8 + xa - xb), p->y + PI(8 + ya - yb), obj);
    if (spr >= 0) pin_set_sprite(r, spr);
    return r;
}

static void gold_drop(int i, int obj)
{
    int g = rubble_at(i, obj, 4, -1);
    int a = RAND(0, 3), b = RAND(0, 3);
    PX(g).xVel = NI(a - b);
    PX(g).yVel = NI(RAND(2, 4) * 1);
}

/* objects/oSolid/Destroy_0.gml */
static void destroy_solid(int i)
{
    struct pin *p = &PX(i);
    int obj;
    if (p->shopWall) PUNTR(1010);
    if (collision_point_p(PTOD(p->x) + 8, PTOD(p->y) - 1, OBJ_oSpikes, 0, NOONE) != NOONE) {
        obj = instance_place_p(i, PTOD(p->x) + 8, PTOD(p->y) - 1, OBJ_oSpikes);
        if (obj != NOONE) pin_destroy(obj);
    }
    if (collision_point_p(PTOD(p->x) + 8, PTOD(p->y) - 1, OBJ_oTikiTorch, 0, NOONE) != NOONE) PUNTR(1011);
    if (collision_point_p(PTOD(p->x) + 8, PTOD(p->y) - 1, OBJ_oGrave, 0, NOONE) != NOONE) PUNTR(1012);
    if (collision_point_p(PTOD(p->x) + 8, PTOD(p->y) + 18, OBJ_oLampRed, 0, NOONE) != NOONE) PUNTR(1013);
    if (collision_point_p(PTOD(p->x) + 8, PTOD(p->y) + 18, OBJ_oLamp, 0, NOONE) != NOONE) {
        obj = instance_place_p(i, PTOD(p->x) + 8, PTOD(p->y) + 16, OBJ_oLamp);
        if (obj != NOONE) {
            pin_create(PX(obj).x + PI(8), PX(obj).y + PI(12), OBJ_oLampItem);
            pin_destroy(obj);
        }
    }
    G.checkWater = 1;
}

static void three_rubble(int i, int big, int small)
{
    rubble_at(i, OBJ_oRubble, 8, big);
    rubble_at(i, OBJ_oRubbleSmall, 8, small);
    rubble_at(i, OBJ_oRubbleSmall, 8, small);
}

static void destroy_jar_like(int i, int skull)
{
    struct pin *p = &PX(i);
    if (!p->breakPieces) return;
    pin_create(p->x, p->y, OBJ_oSmokePuff);
    if (skull) {
        PUNTR(1015);                                                            /* oBone pieces */
        return;
    }
    {
        int k;
        for (k = 0; k < 3; k++) {
            int piece = pin_create(p->x - PI(2), p->y - PI(2), OBJ_oRubbleSmall);
            if (p->colLeft) PX(piece).xVel = NI(RAND(1, 3));
            else if (p->colRight) PX(piece).xVel = NI(-RAND(1, 3));
            else {
                int a = RAND(1, 3), b = RAND(1, 3);
                PX(piece).xVel = NI(a - b);
            }
            if (p->colTop) PX(piece).yVel = NI(RAND(0, 3));
            else PX(piece).yVel = NI(-RAND(0, 3));
        }
    }
    if (RAND(1, 3) == 1) pin_create(p->x, p->y, OBJ_oGoldChunk);
    else if (RAND(1, 6) == 1) pin_create(p->x, p->y, OBJ_oGoldNugget);
    else if (RAND(1, 12) == 1) pin_create(p->x, p->y, OBJ_oEmeraldBig);
    else if (RAND(1, 12) == 1) pin_create(p->x, p->y, OBJ_oSapphireBig);
    else if (RAND(1, 12) == 1) pin_create(p->x, p->y, OBJ_oRubyBig);
    else if (RAND(1, 6) == 1) PUNTR(1016);                                     /* a spider: P5 */
    else if (RAND(1, 12) == 1) PUNTR(1017);                                    /* a snake: P5 */
    if (p->held) {
        PL.holdItem = NOONE;
        G.pickupItem = PICK_NONE;                                              /* oPlayer1.pickupItem = "" */
    }
}

void ev_destroy(int i)
{
    struct pin *p = &PX(i);
    int o = p->obj;
    if (pen_destroy(i) || pdam_destroy(i)) return;                             /* P5 hook */
    if (obj_is(o, OBJ_oItem)) {
        if (o == OBJ_oJar || o == OBJ_oSkull) {                                /* oJar / oSkull Destroy */
            if (p->held) PL.holdItem = NOONE;                                  /* action_inherited: oItem */
            destroy_jar_like(i, o == OBJ_oSkull);
        } else if (o == OBJ_oBomb) {                                           /* oBomb Destroy (no inherit) */
            if (p->enemyID != NOONE) PUNTR(1018);
        } else if (o == OBJ_oDamsel || o == OBJ_oDice || o == OBJ_oFlare || o == OBJ_oFlareCrate) {
            PUNTR(1019);
        } else if (p->held)                                                    /* objects/oItem/Destroy_0.gml */
            PL.holdItem = NOONE;
        return;
    }
    switch (o) {
    case OBJ_oBrick:                                                           /* objects/oBrick/Destroy_0.gml */
        destroy_solid(i);
        if (!p->cleanDeath && !G.cleanSolids) {
            int k;
            rubble_at(i, OBJ_oRubble, 8, -1);
            rubble_at(i, OBJ_oRubbleSmall, 8, -1);
            rubble_at(i, OBJ_oRubbleSmall, 8, -1);
            if (p->spr == GSPR_sBrickGold)
                for (k = 0; k < 3; k++) gold_drop(i, OBJ_oGoldChunk);
            if (p->spr == GSPR_sBrickGoldBig) {
                for (k = 0; k < 3; k++) gold_drop(i, OBJ_oGoldChunk);
                gold_drop(i, OBJ_oGoldNugget);
            }
        }
        break;
    case OBJ_oBlock: case OBJ_oPushBlock:                                      /* oBlock / oPushBlock Destroy */
        if (!p->cleanDeath && (o == OBJ_oBlock || !G.cleanSolids)) {
            if (!G.cityOfGold)
                three_rubble(i, GSPR_sRubbleLush, GSPR_sRubbleLushSmall);
            else {
                int k;
                for (k = 0; k < 3; k++) gold_drop(i, OBJ_oGoldChunk);
                gold_drop(i, OBJ_oGoldNugget);
            }
        }
        break;
    case OBJ_oArrowTrapLeft: case OBJ_oArrowTrapLeftLit: case OBJ_oArrowTrapRight: case OBJ_oArrowTrapRightLit:
        if (!p->cleanDeath && !G.cleanSolids) {
            three_rubble(i, GSPR_sRubbleTan, GSPR_sRubbleTanSmall);
            if (p->fired == 0) pin_create(p->x + PI(8), p->y + PI(8), OBJ_oArrow);
        }
        break;
    case OBJ_oAltarLeft: case OBJ_oAltarRight: case OBJ_oSign:
        if (!p->cleanDeath && !G.cleanSolids)
            three_rubble(i, GSPR_sRubbleTan, GSPR_sRubbleTanSmall);
        break;
    case OBJ_oBrickSmooth:
        destroy_solid(i);
        if (!p->cleanDeath && !G.cleanSolids)
            three_rubble(i, GSPR_sRubbleTan, GSPR_sRubbleTanSmall);
        break;
    case OBJ_oFlame:                                                           /* objects/oFlame/Destroy_0.gml */
        pin_create(p->x, p->y, OBJ_oSmokePuff);
        break;
    default:
        if (obj_is(o, OBJ_oSolid)) {
            if (pobj[o].ev & EV_DESTROY) {
                /* the chain resolves to oSolid's own Destroy unless the object has one */
                int a, own = 0;
                for (a = o; a >= 0 && a != OBJ_oSolid; a = objdefs[a].parent) own = 1;
                (void)own;
                destroy_solid(i);
                if (o != OBJ_oSolid) PUNTR(1020);
            }
        } else if (pobj[o].ev & EV_DESTROY)
            PUNTR(1021);
        break;
    }
}

/* ---- Step events ---------------------------------------------------------------------------------------- */
/* objects/oItem/Step_0.gml */
void item_step(int i)
{
    struct pin *p = &PX(i);
    if (!(inview(i, 16) || p->type == T_ROPE))
        return;
    p->depth = G.hasSpectacles ? 51 : 101;                                      /* :5 */
    if ((!instance_exists_p(OBJ_oShopkeeper) || G.thiefLevel > 0 || G.murderer) && p->cost > 0) {   /* :8 */
        p->cost = 0;
        p->forSale = 0;
    }
    if (isRealLevel()) {                                                       /* :18 */
        if (p->cost > 0 && p->forSale && !isInShop(PFLOOR(p->x), PFLOOR(p->y))) pshop_item_left_shop(i);   /* P5 hook */
    } else if (isLevel()) {
        if (p->cost > 0 && p->forSale && !isInShop(PFLOOR(p->x), PFLOOR(p->y))) pshop_item_left_shop(i);   /* P5 hook */
    } else
        p->cost = 0;
    if (p->held) {                                                             /* :37 */
        struct pin *pl = &PX(PL.idx);
        p->xVel = 0;
        p->yVel = 0;
        if (PL.facing == LEFT) pin_setx(p, pl->x - PI(4));
        if (PL.facing == RIGHT) pin_setx(p, pl->x + PI(4));
        if (p->heavy) {
            if (p->type == T_GOLDIDOL || p->type == T_CRYSTALSKULL || p->type == T_LAMP || p->type == T_DAMSEL) {
                if (PL.state == DUCKING && NLT(NABS(pl->xVel), N(2))) pin_sety(p, pl->y + PI(2));
                else pin_sety(p, pl->y);
            } else {
                if (PL.state == DUCKING && NLT(NABS(pl->xVel), N(2))) pin_sety(p, pl->y - PI(2));
                else pin_sety(p, pl->y - PI(4));
            }
        } else {
            if (PL.state == DUCKING && NLT(NABS(pl->xVel), N(2))) pin_sety(p, pl->y + PI(4));
            else pin_sety(p, pl->y + PI(2));
        }
        p->depth = 1;
        if (PL.holdItem == NOONE) p->held = 0;
    } else if (collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oSolid, 0, NOONE) == NOONE) {   /* :69 */
        moveTo(i, p->xVel, p->yVel, 0, 0);
        p->colLeft = p->colRight = p->colBot = p->colTop = 0;
        if (isCollisionLeft(i, 1)) p->colLeft = 1;
        if (isCollisionRight(i, 1)) p->colRight = 1;
        if (isCollisionBottom(i, 1)) p->colBot = 1;
        if (isCollisionTop(i, 1)) p->colTop = 1;
        if (!p->colLeft && !p->colRight) p->stuck = 0;
        if (!p->colBot && !p->stuck) p->yVel += p->myGrav;
        if (NGT(p->yVel, N(8))) p->yVel = N(8);
        if (p->colLeft || p->colRight) {
            p->xVel = NMUL(-p->xVel, N(0.5));
            p->myGrav = N(0.6);
        }
        if (p->colBot) {
            p->myGrav = N(0.6);
            if (NGT(p->yVel, N(1))) p->yVel = NMUL(-p->yVel, p->bounceFactor);
            else p->yVel = 0;
            if (NLT(NABS(p->xVel), N(0.1))) p->xVel = 0;
            else if (NNE(NABS(p->xVel), N(0))) p->xVel = NMUL(p->xVel, p->frictionFactor);
            if (NLT(NABS(p->yVel), N(1))) {
                pin_sety(p, p->y - (PI(1)));
                if (!isCollisionBottom(i, 1)) pin_sety(p, p->y + (PI(1)));
                p->yVel = 0;
            }
        }
        NOPS(12);
        if (p->sticky && p->type == T_BOMB && p->spr == GSPR_sBombArmed) {     /* :113 */
            PUNTR(1031);
        } else if (p->type == T_ARROW && NGT(NABS(p->xVel), N(6))) {
            if (p->colLeft) {
                pin_setx(p, p->x - (PI(2)));
                p->xVel = 0;
                p->yVel = 0;
            } else if (p->colRight) {
                pin_setx(p, p->x + (PI(2)));
                p->xVel = 0;
                p->yVel = 0;
            }
            p->stuck = 1;
        } else if (p->colLeft && !p->stuck) {
            if (!p->colRight) pin_setx(p, p->x + (PI(1)));
        } else if (p->colRight && !p->stuck)
            pin_setx(p, p->x - (PI(1)));
        if (p->sticky && p->type == T_BOMB && p->spr == GSPR_sBombArmed) {
        } else if (isCollisionTop(i, 1)) {                                     /* :153 */
            if (NLT(p->yVel, N(0))) p->yVel = NMUL(-p->yVel, N(0.8));
            else pin_sety(p, p->y + (PI(1)));
            p->myGrav = N(0.6);
        }
        if (collision_rect_p(PTOD(p->x) - 3, PTOD(p->y) - 3, PTOD(p->x) + 3, PTOD(p->y) + 3, OBJ_oLava, 0, NOONE) != NOONE)
            PUNTR(1032);
        else
            p->myGrav = N(0.6);
        if (collision_point_p(PTOD(p->x), PTOD(p->y) - 5, OBJ_oLava, 0, NOONE) != NOONE && p->type != T_SCEPTRE)
            PUNTR(1032);
    } else {                                                                   /* :187 */
        p->colLeft = p->colRight = p->colBot = p->colTop = 0;
        if (isCollisionLeft(i, 1)) p->colLeft = 1;
        if (isCollisionRight(i, 1)) p->colRight = 1;
        if (isCollisionBottom(i, 1)) p->colBot = 1;
        if (isCollisionTop(i, 1)) p->colTop = 1;
        if (p->colTop && !p->colBot) pin_sety(p, p->y + (PI(1)));
        else if (p->colLeft && !p->colRight) pin_setx(p, p->x + (PI(1)));
        else if (p->colRight && !p->colLeft) pin_setx(p, p->x - (PI(1)));
        else {
            p->xVel = 0;
            p->yVel = 0;
        }
    }
    if (p->type == T_BOMB && p->sticky) {                                      /* :217 */
        PUNTR(1033);
    } else if (NGT(NABS(p->xVel), N(2)) || NGT(NABS(p->yVel), N(2))) {
        double x = PTOD(p->x), y = PTOD(p->y);
        pen_item_hit_enemy(i);                                                 /* P5 hook (:233) */
        if (PX(i).alive && collision_rect_p(x - 2, y - 2, x + 2, y + 2, OBJ_oDamsel, 0, i) != NOONE)
            pen_item_hit_damsel(i);                                            /* P5 hook (:340) */
    }
}

/* objects/oJar/Step_0.gml and objects/oSkull/Step_0.gml (no inherit) */
static void jar_step(int i, int skull)
{
    struct pin *p = &PX(i);
    int destroy = 0;
    p->colTop = p->colLeft = p->colRight = p->colBot = 0;
    if (p->held) {
        struct pin *pl = &PX(PL.idx);
        if (PL.facing == LEFT) pin_setx(p, pl->x - PI(4));
        else if (PL.facing == RIGHT) pin_setx(p, pl->x + PI(4));
        if (PL.state == DUCKING && NLT(NABS(pl->xVel), N(2))) pin_sety(p, pl->y + PI(4));
        else pin_sety(p, pl->y);
        p->depth = 1;
    } else {
        moveTo(i, p->xVel, p->yVel, 0, 0);
        if (NLT(p->yVel, N(6))) p->yVel += p->myGrav;
        if (isCollisionTop(i, 1)) p->colTop = 1;
        if (isCollisionLeft(i, 1)) p->colLeft = 1;
        if (isCollisionRight(i, 1)) p->colRight = 1;
        if (isCollisionBottom(i, 1)) p->colBot = 1;
        if (p->colTop && NLT(p->yVel, N(0))) {
            if (skull ? NLT(p->yVel, N(2)) : NLT(p->yVel, N(-3))) destroy = 1;
            p->yVel = NMUL(-p->yVel, N(0.8));
        }
        if (p->colLeft || p->colRight) {
            if (NABS(p->xVel) > (skull ? N(2) : N(3))) destroy = 1;
            p->xVel = NMUL(-p->xVel, N(0.5));
        }
        if (!skull && collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oSolid, 0, NOONE) != NOONE) destroy = 1;
        if (p->colBot) {
            if (NGT(p->yVel, N(3))) destroy = 1;
            if (NGT(p->yVel, N(1))) p->yVel = NMUL(-p->yVel, N(0.5));
            else p->yVel = 0;
            if (NLT(NABS(p->xVel), N(0.1))) p->xVel = 0;
            else if (NNE(NABS(p->xVel), N(0))) p->xVel = NMUL(p->xVel, N(0.3));
        }
        if (p->colLeft) {
            if (!p->colRight) pin_setx(p, p->x + (PI(1)));
            p->yVel = 0;
        } else if (p->colRight) {
            pin_setx(p, p->x - (PI(1)));
            p->yVel = 0;
        }
        if (isCollisionBottom(i, 0) && NLT(NABS(p->yVel), N(1))) {
            pin_sety(p, p->y - (PI(1)));
            p->yVel = 0;
        }
        p->depth = 100;
        if (collision_rect_p(PTOD(p->x) - 3, PTOD(p->y) - 3, PTOD(p->x) + 3, PTOD(p->y) + 3, OBJ_oLava, 0, NOONE) != NOONE ||
            collision_point_p(PTOD(p->x), PTOD(p->y) - 5, OBJ_oLava, 0, NOONE) != NOONE)
            PUNTR(1036);
        NOPS(12);
    }
    {
        double x = PTOD(p->x), y = PTOD(p->y);
        if (pen_jar_hit(i, skull)) destroy = 1;                                /* P5 hook (:104) */
        p = &PX(i);
        if (pdam_jar_hit(i)) destroy = 1;                                      /* P5 hook (:148) */
        (void)x; (void)y;
    }
    if (destroy) {
        if (p->held) {
            PL.holdItem = NOONE;
            PL.pickupItemType = T_NONE;
        }
        pin_destroy(i);
    }
}

/* objects/oTreasure/Step_0.gml */
static void treasure_step(int i)
{
    struct pin *p = &PX(i);
    if (!(inview(i, 16) && p->state == 1))
        return;
    p->colLeft = p->colRight = p->colBot = 0;
    if (isCollisionLeft(i, 1)) p->colLeft = 1;
    if (isCollisionRight(i, 1)) p->colRight = 1;
    if (isCollisionBottom(i, 1)) p->colBot = 1;
    moveTo(i, p->xVel, p->yVel, 0, 0);
    if (!p->colBot) p->yVel += p->myGrav;
    if (NGT(p->yVel, N(8))) p->yVel = N(8);
    if (isCollisionTop(i, 1)) {
        if (NLT(p->yVel, N(0))) p->yVel = NMUL(-p->yVel, N(0.8));
        else pin_sety(p, p->y + (PI(1)));
    }
    if (p->colLeft || p->colRight) p->xVel = NMUL(-p->xVel, N(0.5));
    if (p->colBot) {
        if (NLT(NABS(p->xVel), N(0.1))) p->xVel = 0;
        else if (NNE(NABS(p->xVel), N(0))) p->xVel = NMUL(p->xVel, N(0.3));
        pin_sety(p, p->y - (PI(1)));
        if (!isCollisionBottom(i, 1)) {
            pin_sety(p, p->y + (PI(1)));
            p->status = 0;                                                     /* status = STATIC (not state) */
        }
        p->yVel = 0;
    }
    if (p->colLeft) {
        if (!p->colRight) pin_setx(p, p->x + (PI(1)));
    } else if (p->colRight)
        pin_setx(p, p->x - (PI(1)));
    if (G.hasSpectacles || PG.hasUdjatEye) p->depth = 0;
    else p->depth = 101;
    NOPS(8);
    if (collision_rect_p(PTOD(p->x) - 3, PTOD(p->y) - 3, PTOD(p->x) + 3, PTOD(p->y) + 3, OBJ_oLava, 0, NOONE) != NOONE ||
        collision_point_p(PTOD(p->x), PTOD(p->y) - 5, OBJ_oLava, 0, NOONE) != NOONE)
        PUNTR(1039);
}

/* objects/oDetritus/Step_0.gml (returns 0 if it destroyed itself) */
void detritus_step(int i)
{
    struct pin *p = &PX(i);
    double x = PTOD(p->x), y = PTOD(p->y);
    view_read();
    if (DLT(x, PW.xview - 4) || DGT(x, PW.xview + 320 + 4) || DLT(y, PW.yview - 4) || DGT(y, PW.yview + 240 + 4))
        pin_destroy(i);
    if (NGT(p->life, N(0))) p->life -= N(1);
    else pin_destroy(i);
    moveTo(i, p->xVel, p->yVel, 0, 0);
    if (collision_point_p(PTOD(p->x), PTOD(p->y) - 4, OBJ_oLava, 0, NOONE) != NOONE) PUNTR(1040);
    if (p->bounce) {
        if (NLT(p->yVel, N(6))) p->yVel += p->grav;
        if (isCollisionTop(i, 1) && NLT(p->yVel, N(0))) p->yVel = NMUL(-p->yVel, N(0.8));
        if (isCollisionLeft(i, 1) || isCollisionRight(i, 1)) p->xVel = NMUL(-p->xVel, N(0.5));
        if (isCollisionBottom(i, 1)) {
            if (NGT(p->yVel, N(1))) p->yVel = NMUL(-p->yVel, N(0.5));
            else p->yVel = 0;
        }
        NOPS(6);
    }
}

/* objects/oRubblePiece/Step_0.gml */
static void rubble_step(int i)
{
    struct pin *p = &PX(i);
    double x, y;
    pin_setx(p, PADDV(p->x, p->xVel));
    pin_sety(p, PADDV(p->y, p->yVel));
    p->yVel += p->yAcc;
    NOPS(3);
    x = PTOD(p->x);
    y = PTOD(p->y);
    if (collision_point_p(x, y, OBJ_oWaterSwim, 0, NOONE) != NOONE) PUNTR(1041);
    else if (collision_point_p(x, y, OBJ_oLava, 0, NOONE) != NOONE) pin_destroy(i);
    if (collision_point_p(x, y, OBJ_oSolid, 0, NOONE) != NOONE) pin_destroy(i);
    view_read();
    if (DLT(x, PW.xview - 32) || DGT(x, PW.xview + 320 + 32) || DLT(y, PW.yview - 32) || DGT(y, PW.yview + 240 + 32))
        pin_destroy(i);
}

/* objects/oBomb/Step_0.gml (after oItem's) */
static void bomb_step(int i)
{
    struct pin *p = &PX(i);
    item_step(i);
    if (p->spr == GSPR_sBombArmed) p->depth = 49;
    if (p->sticky) p->depth = 1;
    if (p->armed && instance_exists_p(OBJ_oShopkeeper)) PUNTR(1042);
}

/* objects/oRopeThrow/Step_0.gml (after oItem's) */
static void ropethrow_step(int i)
{
    struct pin *p = &PX(i);
    item_step(i);
    p = &PX(i);
    if (p->armed && NGE(p->yVel, N(0))) {
        move_snap(i, 16, 1);
        if (p->px < NP(p->x)) {
            if (collision_point_p(PTOD(p->x) - 8, PTOD(p->y), OBJ_oSolid, 0, NOONE) == NOONE) pin_setx(p, p->x - (PI(8)));
            else pin_setx(p, p->x + (PI(8)));
        } else {
            if (collision_point_p(PTOD(p->x) + 8, PTOD(p->y), OBJ_oSolid, 0, NOONE) == NOONE) pin_setx(p, p->x + (PI(8)));
            else pin_setx(p, p->x - (PI(8)));
        }
        pin_create(p->x, p->y, OBJ_oRopeTop);
        p = &PX(i);
        p->armed = 0;
        p->falling = 1;
        p->xVel = 0;
        p->yVel = 0;
    }
    if (p->falling) {
        p->xVel = 0;
        p->yVel = 0;
        pin_sety(p, p->y + (PI(8)));
        p->fallCount += 1;
        if (isCollisionBottom(i, 1) || p->fallCount > 16) {
            p->falling = 0;
            pin_sety(p, p->y - (PI(8)));
            pin_destroy(i);
        } else
            pin_create(p->x - PI(8), p->y, OBJ_oRope);
    }
}

/* objects/oArrow/Step_0.gml (after oItem's): direction = degrees of the velocity; image_angle = direction */
static void arrow_step(int i)
{
    struct pin *p = &PX(i);
    item_step(i);
    p = &PX(i);
    {
        double xv = NTOD(p->xVel), yv = NTOD(p->yVel);
        extern double patan_deg(double a);
        if (xv > 0 && yv < 0) p->direction = patan_deg(-yv / xv);
        else if (xv < 0 && yv < 0) p->direction = 180 - patan_deg(-yv / -xv);
        else if (xv > 0 && yv > 0) p->direction = patan_deg(yv / xv);
        else if (xv < 0 && yv > 0) p->direction = 180 + patan_deg(yv / -xv);
        else if (xv < 0) p->direction = 180;
        else if (!p->stuck) p->direction = 0;
        pin_setangle(p, (float)p->direction);                                     /* image_angle: a float */
    }
}

/* objects/oGoldIdol/Step_0.gml (after oItem's) */
static void goldidol_step(int i)
{
    struct pin *p = &PX(i);
    item_step(i);
    p = &PX(i);
    if (inview(i, 8)) {
        if (isLevel()) {
            if (!p->held && collision_point_p(PTOD(p->x), PTOD(p->y) + 4, OBJ_oBrickSmooth, 0, NOONE) != NOONE &&
                instance_exists_p(OBJ_oShopkeeper) && G.thiefLevel == 0 && !G.murderer)
                PUNTR(1043);
        }
        if (!p->colBot && p->trigger)
            p->trigger = 0;
    }
}

/* objects/oWhip/Step_0.gml, oWhipPre/Step_0.gml */
static void whip_step(int i, int pre)
{
    struct pin *p = &PX(i);
    if (PL.idx == NOONE || !PX(PL.idx).alive) {
        pin_destroy(i);
        return;
    }
    if (p->spr == (pre ? GSPR_sWhipPreR : GSPR_sWhipRight)) {
        pin_setx(p, PX(PL.idx).x + PI(pre ? -16 : 16));
        pin_sety(p, PX(PL.idx).y);
    } else if (p->spr == (pre ? GSPR_sWhipPreL : GSPR_sWhipLeft)) {
        pin_setx(p, PX(PL.idx).x + PI(pre ? 16 : -16));
        pin_sety(p, PX(PL.idx).y);
    }
}

/* scripts/gameStepEvent (oGame Step's first line) */
static void gameStepEvent(void)
{
    play_time += 1;                                                            /* oGame.time += 1 */
    if (play_time > 100000000) play_time = 0;
    pen_moving_solids();                                                       /* P5 hook (:37-206) */
    {   /* with oMoveableSolid: fall inside the view */
        int16_t w[256];
        int n = pw_with(OBJ_oMoveableSolid, w, 256), k;
        for (k = 0; k < n; k++) {
            int j = w[k];
            struct pin *p = &PX(j);
            double x, y;
            if (!p->alive) continue;
            x = PTOD(p->x);
            y = PTOD(p->y);
            view_read();
            if (DGT(x, PW.xview - 16) && DLT(x, PW.xview + 320) && DGT(y, PW.yview - 16) && DLT(y, PW.yview + 240)) {
                pos yMPrev = p->y;
                p->yVel += p->myGrav;
                if (NGT(p->yVel, N(8))) p->yVel = N(8);
                NOPS(2);
                for (; DLT(PTOD(p->y), PTOD(yMPrev) + NTOD(p->yVel)); pin_sety(p, p->y + (PI(1)))) {
                    if (place_meeting_p(j, PTOD(p->x), PTOD(p->y) + 1, OBJ_oSolid)) {
                        p->yVel = 0;
                        break;
                    }
                }
            }
        }
    }
}

static int spr_is_exit_g(int s) { return s == GSPR_sPExit || s == GSPR_sDamselExit; }

/* objects/oGame/Step_0.gml */
static void game_step(int i)
{
    struct pin *g = &PX(i);
    gameStepEvent();
    if (!instance_exists_p(OBJ_oXMarket)) PG.udjatBlink = 0;
    else PUNTR(1051);
    if (G.gameStart && instance_exists_p(OBJ_oCharacter) && isLevel()) {      /* :13 */
        if (!PL.dead) {
            PG.time += 30;
            PG.xtime += 30;
        }
    }
    if (instance_exists_p(OBJ_oPlayer1)) {                                     /* :45 ghost */
        if (isLevel() && !isRoomIs(R_rOlmec) && G.currLevel > 1 && !PG.hasCrown && PG.xtime > 120000 &&
            !spr_is_exit_g(PX(PL.idx).spr))
            PLEV.musicFade = 1;
        if (isLevel() && !isRoomIs(R_rOlmec) && G.currLevel > 1 && !PG.hasCrown && PG.xtime > 150000 &&
            !PG.ghostExists && !spr_is_exit_g(PX(PL.idx).spr))
            PUNTR(1052);
    }
    if (G.checkWater) {                                                        /* :64 */
        if (instance_exists_p(OBJ_oWater)) PUNTR(1053);
        G.checkWater = 0;                                                      /* waterCounter == 0 */
    }
    if (instance_exists_p(OBJ_oPlayer1) && PL.dead) {                          /* :127 game over */
        if (PGAME.drawStatus == 0) {
            g->alarm[0] = 50;
            PGAME.drawStatus += 1;
        }
        if (PGAME.drawStatus > 2) {
            int32_t moneyDiff = PG.money - PGAME.moneyCount;
            if (moneyDiff > 1000) PGAME.moneyCount += 1000;
            else if (moneyDiff > 100) PGAME.moneyCount += 100;
            else PGAME.moneyCount += moneyDiff;
        }
    }
    /* :152- pause menu and language keys: no keyboard in the routes, start never pressed */
    if (GP.pressed & K_START) PUNTR(1054);
}

/* objects/oLevel/Step_0.gml: the screen shake (the rest: water drawing flags and activation, no play state) */
static void level_step(int i)
{
    (void)i;
    if (PG.shake > 0) {
        if (PTOD(PX(PL.idx).y) < 96 || PTOD(PX(PL.idx).y) > PW.room_h - 96) PW.vborder = 0;
        else PW.vborder = 96;
        view_read();
        if (PG.shakeToggle || PW.yview <= 0) {
            view_set_y(PW.yview + 3);
            PG.shakeToggle = 0;
        } else if (!PG.shakeToggle || PW.yview >= PW.room_h - 240) {
            view_set_y(PW.yview - 3);
            PG.shakeToggle = 1;
        }
        PG.shake -= 1;
    } else
        PW.vborder = 96;
}

void ev_step(int i)
{
    struct pin *p = &PX(i);
    if (pen_step(i) || pdam_step(i) || pshop_step(i) || pitem_step(i)) return; /* P5 hook */
    switch (p->obj) {
    case OBJ_oPlayer1: pl_step(i); break;
    case OBJ_oGame: game_step(i); break;
    case OBJ_oLevel: level_step(i); break;
    case OBJ_oJar: jar_step(i, 0); break;
    case OBJ_oSkull: jar_step(i, 1); break;
    case OBJ_oBomb: bomb_step(i); break;
    case OBJ_oRopeThrow: ropethrow_step(i); break;
    case OBJ_oArrow: arrow_step(i); break;
    case OBJ_oGoldIdol: goldidol_step(i); break;
    case OBJ_oKey:
        item_step(i);
        if (PX(i).held) pin_set_sprite(i, PL.facing == 18 ? GSPR_sKeyLeft : GSPR_sKeyRight);
        break;
    case OBJ_oWhip: whip_step(i, 0); break;
    case OBJ_oWhipPre: whip_step(i, 1); break;
    case OBJ_oBlood:                                                           /* oBlood Step: inherited first */
    case OBJ_oFlame:
        detritus_step(i);
        p = &PX(i);
        if (NGT(p->yVel, N(6))) pin_destroy(i);
        if (isCollisionBottom(i, 1)) {
            if (NGT(p->life, N(20))) p->life = N(20);
        }
        break;
    case OBJ_oPoof:
        pin_setx(p, PADDV(p->x, p->xVel));
        pin_sety(p, PADDV(p->y, p->yVel));
        break;
    case OBJ_oSmokePuff: pin_sety(p, PSUBV(p->y, p->yVel)); break;
    case OBJ_oBurn:
        pin_sety(p, PADDV(p->y, p->yVel));
        if (collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oSolid, 0, NOONE) != NOONE) pin_destroy(i);
        break;
    case OBJ_oItemsGet:
        pin_sety(p, PSUBV(p->y, p->yVel));
        pin_setx(p, PI(PCEIL(p->x)));
        pin_sety(p, PI(PCEIL(p->y)));
        break;
    case OBJ_oBigCollect: pin_sety(p, p->y - (PI(1))); break;
    case OBJ_oRubble: case OBJ_oRubbleSmall: rubble_step(i); break;
    case OBJ_oPushBlock:                                                       /* inherited: no parent Step */
        if (collision_point_p(PTOD(p->x) + 8, PTOD(p->y) + 14, OBJ_oLava, 0, NOONE) != NOONE &&
            collision_point_p(PTOD(p->x) + 8, PTOD(p->y) + 17, OBJ_oSolid, 0, NOONE) == NOONE)
            PUNTR(1055);
        break;
    case OBJ_oWeb:                                                             /* objects/oWeb/Step_0.gml */
        p->alpha = NTOD(p->life) / 12;
        if (p->dying) p->life -= N(0.02);
        if (NLE(p->life, N(1))) pin_destroy(i);
        break;
    case OBJ_oRope:
        if (collision_point_p(PTOD(p->x) + 12, PTOD(p->y), OBJ_oLava, 0, NOONE) != NOONE && p->burnTimer == 0) PUNTR(1056);
        if (p->burnTimer > 1) p->burnTimer -= 1;
        else if (p->burnTimer == 1) PUNTR(1056);
        break;
    case OBJ_oArrowTrapLeft: case OBJ_oArrowTrapLeftLit: case OBJ_oArrowTrapRight: case OBJ_oArrowTrapRightLit:
        break;                                                                 /* firing = false; the rest commented */
    case OBJ_oBones:                                                           /* objects/oBones/Step_0.gml */
        if (collision_point_p(PTOD(p->x) + 8, PTOD(p->y) + 16, OBJ_oSolid, 0, NOONE) == NOONE) {
            pin_sety(p, PADDV(p->y, p->yVel));
            p->yVel += p->yAcc;
        }
        if (collision_point_p(PTOD(p->x) + 8, PTOD(p->y) + 15, OBJ_oSolid, 0, NOONE) != NOONE) pin_sety(p, p->y - (PI(1)));
        break;
    case OBJ_oGamepad: break;                                                  /* prun.c */
    default:
        if (ptrans_step(i)) break;
        if (obj_is(p->obj, OBJ_oTreasure)) treasure_step(i);
        else if (obj_is(p->obj, OBJ_oItem)) {
            if (p->obj == OBJ_oDice) {                                         /* objects/oDice/Step_0.gml */
                if (inview(i, 16)) PUNTR(1064);                                /* :1-208: P5 (shops) */
                if (NGT(NABS(p->yVel), N(2)) || NGT(NABS(p->xVel), N(2))) {          /* :210 */
                    pin_set_sprite(i, GSPR_sDiceRoll);
                    p->value = RAND(1, 6);
                } else if (isCollisionBottom(i, 1)) {
                    static const int16_t dice[6] = { GSPR_sDice1, GSPR_sDice2, GSPR_sDice3, GSPR_sDice4,
                                                     GSPR_sDice5, GSPR_sDice6 };
                    if (p->rolling && NEQ(p->yVel, N(0))) PUNTR(1065);
                    pin_set_sprite(i, dice[(p->value >= 1 && p->value <= 5) ? p->value - 1 : 5]);
                }
            } else if (p->obj == OBJ_oDamsel || p->obj == OBJ_oFlare || p->obj == OBJ_oFlareCrate ||
                p->obj == OBJ_oLockedChest || p->obj == OBJ_oMattock || p->obj == OBJ_oWebCannon)
                PUNTR(1060);
            else
                item_step(i);
        } else
            PUNTR(1061);
        break;
    }
}

void ev_end_step(int i)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oPlayer1: pl_end_step(i); break;
    case OBJ_oBomb:                                                            /* objects/oBomb/Step_2.gml */
        if (p->enemyID != NOONE && !PX(p->enemyID).alive) p->enemyID = NOONE;
        if (p->enemyID != NOONE) PUNTR(1062);
        break;
    case OBJ_oGamepad: break;
    default: PUNTR(1063); break;
    }
}

/* ---- Alarms ---------------------------------------------------------------------------------------------- */
void ev_alarm(int i, int a)
{
    struct pin *p = &PX(i);
    int o = p->obj;
    if (o == OBJ_oPlayer1) { pl_alarm(i, a); return; }
    if (pen_alarm(i, a) || pshop_alarm(i, a) || pdam_alarm(i, a)) return;     /* P5 hook */
    if (obj_is(o, OBJ_oTreasure) && a == 0) { p->canCollect = 1; return; }     /* gems' Alarm_0 */
    switch (o) {
    case OBJ_oBomb:
        if (a == 0) {                                                          /* objects/oBomb/Alarm_0.gml */
            p->ispd = 1;
            p->alarm[1] = 40;
        } else if (a == 1) {                                                   /* Alarm_1 */
            pin_create(p->x, p->y, OBJ_oExplosion);
            if (G.graphicsHigh) scrCreateFlame(p->x, p->y, 3);
            p = &PX(i);
            if (p->held) PL.holdItem = NOONE;
            pin_destroy(i);
        } else if (a == 2)                                                     /* oItem Alarm_2 */
            p->safe = 0;
        break;
    case OBJ_oArrow:
        if (a == 1) PUNTR(1070);                                               /* bomb arrows */
        else if (a == 2) p->safe = 0;                                          /* objects/oArrow/Alarm_2.gml */
        break;
    case OBJ_oArrowTrapLeft: case OBJ_oArrowTrapLeftLit:
        if (a == 0) {
            int ar = pin_create(p->x + PI(16), p->y + PI(4), OBJ_oArrow);
            PX(ar).xVel = N(5);
        } else if (a == 1 && !isRoomIs(R_rLevelEditor)) {                      /* objects/oArrowTrapLeft/Alarm_1.gml */
            int xAct = PFLOOR(p->x) - 1, obj;
            while (collision_point_p(xAct, PTOD(p->y) + 8, OBJ_oSolid, 0, NOONE) == NOONE) {
                if (PFLOOR(p->x) - xAct > 96) break;
                xAct -= 1;
            }
            if (xAct > PFLOOR(p->x) - 16) xAct = PFLOOR(p->x) - 16;
            p->xAct = (int16_t)xAct;
            obj = pin_create(PI(xAct), p->y, OBJ_oArrowTrapTest);
            pin_setxscale(&PX(obj), dceil(((PFLOOR(PX(i).x) - 1) - xAct) / 16.0));
            PX(obj).trapID = (int16_t)i;
        }
        break;
    case OBJ_oArrowTrapRight: case OBJ_oArrowTrapRightLit:
        if (a == 0) PUNTR(1071);
        else if (a == 1 && !isRoomIs(R_rLevelEditor)) {                        /* objects/oArrowTrapRight/Alarm_1.gml */
            int x = PFLOOR(p->x), xAct = x + 16, n = 100, obj;
            while (collision_point_p(xAct, PTOD(p->y) + 8, OBJ_oSolid, 0, NOONE) == NOONE && n > 0) {
                if (xAct - x > 96) break;
                xAct += 1;
                n -= 1;
            }
            xAct -= x + 8;
            if (xAct < 32) xAct = 32;
            p->xAct = (int16_t)xAct;
            obj = pin_create(PI(x + 16), p->y, OBJ_oArrowTrapTest);
            pin_setxscale(&PX(obj), dceil((xAct - 16) / 16.0));
            PX(obj).trapID = (int16_t)i;
        }
        break;
    case OBJ_oWhipPre: if (a == 0) pin_destroy(i); break;
    case OBJ_oBlood:
        if (a == 0) {                                                          /* objects/oBlood/Alarm_0.gml */
            if (G.graphicsHigh) {
                if (instance_number_p(OBJ_oBloodTrail) < 12) pin_create(p->x, p->y, OBJ_oBloodTrail);
                PX(i).alarm[0] = 4;
            }
        } else if (a == 1) {
            p->invincible = 0;
            p->bounce = 1;
        } else if (a == 2)
            p->collectible = 1;
        break;
    case OBJ_oFlame:
        if (a == 0) {                                                          /* objects/oFlame/Alarm_0.gml */
            if (instance_number_p(OBJ_oFlameTrail) < 12) pin_create(p->x, p->y, OBJ_oFlameTrail);
            PX(i).alarm[0] = 2;
        }
        break;
    case OBJ_oItemsGet: case OBJ_oBigCollect: if (a == 0) pin_destroy(i); break;
    case OBJ_oGame:
        if (a == 0) {                                                          /* objects/oGame/Alarm_0.gml */
            if (PGAME.drawStatus < 3) PGAME.drawStatus = 2;
            p->alarm[1] = 50;
        } else if (a == 1) {
            if (PGAME.drawStatus < 3) PGAME.drawStatus = 3;
        } else if (a == 2)
            PG.udjatBlink = !PG.udjatBlink;
        break;
    case OBJ_oGiantTikiHead: PUNTR(1072); break;                               /* the boulder: P5 */
    default:
        if (ptrans_alarm(i, a)) break;
        if (obj_is(o, OBJ_oItem) && a == 2) p->safe = 0;                       /* objects/oItem/Alarm_2.gml */
        else PUNTR(1073);
        break;
    }
}

/* ---- Animation End --------------------------------------------------------------------------------------- */
void ev_animend(int i)
{
    /* the room's first frame animates the enemies before TRACE_NOENEMY removes them at its Begin Step: their
       Animation End events only change their own status / sprite (oShopkeeper, oDamsel, oFakeBones, ...) */
    if (play_noenemy && PW.room_new &&
        (obj_is(PX(i).obj, OBJ_oEnemy) || PX(i).obj == OBJ_oDamsel || PX(i).obj == OBJ_oFakeBones))
        return;
    if (pen_animend(i) || pdam_animend(i) || pshop_animend(i)) return;         /* P5 hook */
    switch (PX(i).obj) {
    case OBJ_oPlayer1: pl_animend(i); break;
    case OBJ_oExplosion: case OBJ_oPoof: case OBJ_oSmokePuff: case OBJ_oBurn: case OBJ_oFlameTrail:
    case OBJ_oBloodTrail:
        pin_destroy(i);
        break;
    default:
        if (!ptrans_animend(i)) PUNTR(1080);
        break;
    }
}

/* ---- Collision events ------------------------------------------------------------------------------------ */
static void trap_fire(int self, int other)
{
    int t = PX(self).trapID;
    (void)other;
    if (t == NOONE) {                                                          /* trapID == 0 */
        pin_destroy(self);
        return;
    }
    if (PX(t).alive && PX(t).fired == 0) {                                     /* with trapID */
        int ar;
        if (PX(t).facing == 0) {
            ar = pin_create(PX(t).x - PI(2), PX(t).y + PI(4), OBJ_oArrow);
            PX(ar).xVel = N(-8);
        } else {
            ar = pin_create(PX(t).x + PI(18), PX(t).y + PI(4), OBJ_oArrow);
            PX(ar).xVel = N(8);
        }
        PX(t).fired += 1;
    }
    pin_destroy(self);
}

/* objects/oExplosion/Collision_oSolid.gml */
static void explosion_solid(int self, int other)
{
    struct pin *p = &PX(self);
    double x = PTOD(p->x), y = PTOD(p->y);
    view_read();
    if (isRoomIs(R_rTutorial) || (DGT(x, PW.xview - 16) && DLT(x, PW.xview + 320 + 16) && DGT(y, PW.yview - 16) && DLT(y, PW.yview + 240 + 16))) {
        int16_t w[512];
        int n, k;
        /* tile_layer_find / tile_delete: the background tiles (drawing only) */
        if (!PX(other).invincible) pin_destroy(other);
        n = pw_with(OBJ_oTreasure, w, 512);
        for (k = 0; k < n; k++) if (PX(w[k]).alive) PX(w[k]).state = 1;
        n = pw_with(OBJ_oSpikes, w, 512);
        for (k = 0; k < n; k++) {
            int s = w[k];
            if (!PX(s).alive) continue;
            if (collision_point_p(PTOD(PX(s).x), PTOD(PX(s).y) + 16, OBJ_oSolid, 0, NOONE) == NOONE) pin_destroy(s);
        }
    }
}

/* objects/oExplosion/Collision_oItem.gml */
static void explosion_item(int self, int other)
{
    struct pin *e = &PX(self), *o = &PX(other);
    if (o->type == T_ARROW || o->type == T_FISHBONE || o->type == T_JAR || o->type == T_SKULL) {
        pin_destroy(other);
    } else if (o->type == T_BOMB) {
        pin_set_sprite(other, GSPR_sBombArmed);
        o->ispd = 1;
        o->alarm[1] = RAND(4, 8);
        o->enemyID = NOONE;
        if (o->y < e->y) o->yVel = NI(-RAND(2, 4));
        if (o->x < e->x) o->xVel = NI(-RAND(2, 4));
        else o->xVel = NI(RAND(2, 4));
    } else if (o->type == T_ROPE) {
        if (!o->falling) {
            if (o->y < e->y) o->yVel -= N(6);
            else o->yVel += N(6);
            if (e->x > o->x) o->xVel -= NI(RAND(4, 6));
            else o->xVel += NI(RAND(4, 6));
        }
    } else {
        if (o->y < e->y) o->yVel -= N(6);
        else o->yVel += N(6);
        if (e->x > o->x) o->xVel -= NI(RAND(4, 6));
        else o->xVel += NI(RAND(4, 6));
    }
    o = &PX(other);
    if (o->held) {
        PL.holdItem = NOONE;
        PL.pickupItemType = T_NONE;
        o->held = 0;
    }
}

void ev_collision(int self, int other)
{
    int so = PX(self).obj, oo = PX(other).obj;
    if (pen_collision(self, other) || pdam_collision(self, other) || pshop_collision(self, other) ||
        pitem_collision(self, other))
        return;                                                                /* P5 hook */
    switch (so) {
    case OBJ_oPlayer1: pl_collision(self, other); break;
    case OBJ_oArrowTrapTest:
        if (obj_is(oo, OBJ_oCharacter)) {
            struct pin *pl = &PX(PL.idx);
            if (PX(self).trapID == NOONE) pin_destroy(self);
            else if (NGT(NABS(PX(other).xVel), N(0)) || NGT(NABS(PX(other).yVel), N(0)) ||
                     (pl->spr == GSPR_sDuckToHangL && DGT(pl->img, 6)) || (pl->spr == GSPR_sDamselDtHL && DGT(pl->img, 6)) ||
                     (pl->spr == GSPR_sTunnelDtHL && DGT(pl->img, 6)))
                trap_fire(self, other);
        } else {                                                               /* P5: oBoulder as the others */
            if (PX(self).trapID == NOONE) pin_destroy(self);
            else if (NGT(NABS(PX(other).xVel), N(0)) || NGT(NABS(PX(other).yVel), N(0)))
                trap_fire(self, other);
        }
        break;
    case OBJ_oExplosion:
        if (obj_is(oo, OBJ_oSolid)) explosion_solid(self, other);
        else if (obj_is(oo, OBJ_oItem)) {
            if (oo == OBJ_oDamsel) PUNTR(1091);
            else explosion_item(self, other);
        } else if (obj_is(oo, OBJ_oWeb)) pin_destroy(other);
        else PUNTR(1092);
        break;
    case OBJ_oWeb:                                                             /* objects/oWeb/Collision_*.gml */
        if (obj_is(oo, OBJ_oItem)) {
            if (!PX(other).held && PX(other).type != T_ROPE) {
                PX(other).xVel = 0;
                PX(other).yVel = 0;
            }
        } else if (obj_is(oo, OBJ_oRubblePiece)) {
            PX(other).xVel = 0;
            PX(other).yVel = 0;
            PX(other).ispd = 0;
        } else if (obj_is(oo, OBJ_oTreasure)) {
            PX(other).xVel = 0;
            PX(other).yVel = 0;
        } else
            PUNTR(1093);
        break;
    case OBJ_oJar:
        if (obj_is(oo, OBJ_oWhip)) {                                           /* objects/oJar/Collision_oWhip.gml */
            struct pin *p = &PX(self);
            int k;
            pin_create(p->x, p->y, OBJ_oSmokePuff);
            for (k = 0; k < 3; k++) {
                int piece = pin_create(PX(self).x - PI(2), PX(self).y - PI(2), OBJ_oRubbleSmall);
                int a = RAND(1, 3), b = RAND(1, 3);
                PX(piece).xVel = NI(a - b);
            }
            p = &PX(self);
            if (RAND(1, 3) == 1) pin_create(p->x, p->y, OBJ_oGoldChunk);
            else if (RAND(1, 6) == 1) pin_create(p->x, p->y, OBJ_oGoldNugget);
            else if (RAND(1, 12) == 1) pin_create(p->x, p->y, OBJ_oEmeraldBig);
            else if (RAND(1, 12) == 1) pin_create(p->x, p->y, OBJ_oSapphireBig);
            else if (RAND(1, 12) == 1) pin_create(p->x, p->y, OBJ_oRubyBig);
            else if (RAND(1, 6) == 1) pin_create(p->x - PI(8), p->y - PI(8), OBJ_oSpider);   /* P5 */
            else if (RAND(1, 12) == 1) pin_create(p->x - PI(8), p->y - PI(8), OBJ_oSnake);
            if (PX(self).held) {
                PL.holdItem = NOONE;
                PL.pickupItemType = T_NONE;
            }
            pin_destroy(self);
        } else
            PUNTR(1094);
        break;
    case OBJ_oLockedChest: PUNTR(1095); break;
    default: PUNTR(1096); break;
    }
}

void ev_draw(int i)
{
    if (PX(i).obj == OBJ_oPlayer1) pl_draw(i);
    else if (pen_draw(i) || pdam_draw(i) || pshop_draw(i)) return;             /* P5 hook */
    else ptrans_draw(i);
}

void ev_outside(int i)
{
    if (PX(i).obj == OBJ_oPushBlock) pin_destroy(i);                           /* objects/oPushBlock/Other_0.gml */
    else if (pen_outside(i)) return;                                           /* P5 hook */
    else PUNTR(1097);
}
