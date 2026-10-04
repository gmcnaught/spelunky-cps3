/* Events of the objects the player meets in P4 (no enemies): items, treasures, bombs, ropes, the whip, arrow
 * traps, push blocks, terrain destruction, particles, oGame and oLevel. refs/hd/src/objects/<obj>/<event>.gml,
 * line numbers in comments. `action_inherited()` is written out as a call of the parent's event.
 * GML argument order: last argument first, so instance_create(x + rand.., y + rand.., o) draws y's numbers first.
 * Objects or branches P4 does not reach set play_untranslated (codes 1xxx).
 */
#include "pint.h"
#include "../snd/sndgame.h"                     /* the GML sound calls (src/snd) */
#include "pmsg.h"                                /* the HUD messages (trMessages) */
#include "front.h"                                     /* P8: the front end's hooks (src/front/front.h) */
/* the hooks' defaults for builds without src/front (no front room ever runs) */
__attribute__((weak)) uint8_t front_on;
__attribute__((weak)) int16_t front_view_obj = -1, front_hborder = 160;
__attribute__((weak)) int front_ev(int ev, int i, int arg) { (void)ev; (void)i; (void)arg; return 0; }
__attribute__((weak)) int front_room(int room) { (void)room; return 0; }
#include "penemy.h"                  /* P5 hooks: enemies, damsel, shop (each marked "P5 hook") */
#include "pcontent.h"                            /* P7 content packages (docs/CONTENT.md) */

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
    "Yeti", "ManTrap", "Vampire", "Tomb Lord", "Magma Man", "Alien Boss", "UFO", "Alien", "Frog", "Fire Frog",
    "Monkey", "Piranha", "Mega Mouth", "Yeti King",
};

/* ---- Create events ------------------------------------------------------------------------------------- */
static void make_active(struct pin *p) { PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0; }

/* objects/oItem/Create_0.gml */
void create_item(struct pin *p)
{
    p->type = T_NONE;
    PE(p)->active = 1;
    PE(p)->New = 1;
    PE(p)->held = 0;
    PE(p)->myGrav = N(0.6);
    PE(p)->armed = 0;
    PE(p)->trigger = 0;
    PE(p)->safe = 0;
    PE(p)->heavy = 0;
    PE(p)->value = 0;
    PE(p)->colBot = 0;
    PE(p)->canPickUp = 1;
    PE(p)->bounceFactor = N(0.5);
    PE(p)->frictionFactor = N(0.3);
    PE(p)->breakPieces = 1;
    PE(p)->cost = 0;
    PE(p)->forSale = 0;
    PE(p)->inDiceHouse = 0;
    PE(p)->cimg = 0;
    PE(p)->stuck = 0;
    PE(p)->sticky = 0;
    PE(p)->enemyID = NOONE;
    pin_setdepth(p, G.hasSpectacles ? 51 : 101);
    PE(p)->colLeft = PE(p)->colRight = PE(p)->colBot = PE(p)->colTop = 0;
}

/* objects/oTreasure/Create_0.gml */
static void create_treasure(struct pin *p)
{
    p->type = T_NONE;
    PE(p)->held = 0;
    PE(p)->myGrav = N(0.6);
    PE(p)->trigger = 0;
    PE(p)->value = 0;
    PE(p)->canCollect = 0;
    PE(p)->yOff = 4;
    PE(p)->xVel = PE(p)->yVel = 0;
    PE(p)->state = 1;                                                              /* ACTIVE */
    PE(p)->colLeft = PE(p)->colRight = PE(p)->colBot = 0;
}

static void treasure(int i, int type, int l, int t, int r, int b, int value, int canCollect, int alarm0, int yOff)
{
    struct pin *p = &PX(i);
    create_treasure(p);
    p->type = (int16_t)type;
    make_active(p);
    setCollisionBounds(i, l, t, r, b);
    if (yOff) PE(p)->yOff = (int16_t)yOff;
    if (alarm0) PE(p)->alarm[0] = alarm0;
    PE(p)->value = value;
    if (canCollect >= 0) PE(p)->canCollect = (uint8_t)canCollect;
}

static void item(int i, int type, int l, int t, int r, int b, int32_t cost)
{
    struct pin *p = &PX(i);
    create_item(p);
    p->type = (int16_t)type;
    make_active(p);
    setCollisionBounds(i, l, t, r, b);
    if (cost >= 0) PE(p)->cost = cost;
}

/* objects/oDetritus/Create_0.gml (returns 0 if it destroyed itself) */
int create_detritus(int i)
{
    struct pin *p = &PX(i);
    p->type = T_NONE;
    make_active(p);
    setCollisionBounds(i, -4, -4, 4, 4);
    PE(p)->life = N(60);
    PE(p)->grav = N(0.6);
    PE(p)->bounce = 1;
    PE(p)->dying = 0;
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
    if (front_on && front_ev(FEV_CREATE, i, 0)) return;                                 /* P8 hook */
    struct pin *p = &PX(i);
    if (pen_create(i, play_in_gen_init) || pdam_create(i, play_in_gen_init) || pshop_create(i, play_in_gen_init) ||
        pitem_create(i, play_in_gen_init))
        return;                                                                /* P5 hook */
    switch (p->obj) {
    /* items */
    case OBJ_oRock: item(i, T_ROCK, -4, -4, 4, 4, -1); break;
    case OBJ_oJar:
        item(i, T_JAR, -4, -6, 4, 6, -1);
        PE(p)->breakPieces = 1;
        break;
    case OBJ_oSkull: item(i, T_SKULL, -4, -4, 4, 4, -1); break;
    case OBJ_oChest: case OBJ_oCrate: case OBJ_oLockedChest:
        item(i, p->obj == OBJ_oChest ? T_CHEST : p->obj == OBJ_oCrate ? T_CRATE : T_LOCKEDCHEST,
             -6, p->obj == OBJ_oLockedChest ? -2 : 0, 6, 8, -1);
        PE(p)->heavy = 1;
        PE(p)->yVel = 0;
        PE(p)->yAcc = N(0.2);
        break;
    case OBJ_oGoldIdol:
        item(i, T_GOLDIDOL, -4, -4, 4, 4, -1);
        PE(p)->trigger = 1;
        PE(p)->heavy = 1;
        PE(p)->value = 5000;
        break;
    case OBJ_oKey: item(i, T_KEY, -4, -4, 4, 4, 0); break;
    case OBJ_oBomb: item(i, T_BOMB, -4, -4, 4, 4, -1); break;
    case OBJ_oRopeThrow:
        item(i, T_ROPE, -4, -4, 4, 4, -1);
        PE(p)->armed = 0;
        PE(p)->falling = 0;
        PE(p)->fallCount = 0;
        PE(p)->px = 0;
        PE(p)->py = 0;
        break;
    case OBJ_oArrow:
        item(i, T_ARROW, -4, -4, 4, 4, -1);
        PE(p)->myGrav = N(0.2);
        break;
    case OBJ_oFishBone:
        item(i, T_FISHBONE, -4, -4, 4, 4, -1);
        PE(p)->myGrav = N(0.2);
        PE(p)->safe = 0;
        break;
    case OBJ_oBombBag: item(i, T_BOMBBAG, -6, -2, 6, 6, 2500); break;
    case OBJ_oBombBox: item(i, T_BOMBBOX, -6, -2, 6, 8, 10000); PE(p)->heavy = 1; break;
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
    case OBJ_oJetpack: item(i, T_JETPACK, -5, -5, 5, 8, 20000); PE(p)->heavy = 1; break;
    case OBJ_oMattockHead: item(i, T_MATTOCKHEAD, -6, -4, 6, 4, -1); break;
    case OBJ_oSceptre: item(i, T_SCEPTRE, -4, -4, 4, 4, 0); break;
    case OBJ_oDice:                                                            /* objects/oDice/Create_0.gml */
        item(i, T_DICE, -6, 0, 6, 8, -1);
        PE(p)->heavy = 1;
        if (!play_in_gen_init) pitems_world(1002, i, 0);                                    /* value = rand(1, 6): P5 (shops) */
        break;
    case OBJ_oLampItem:
        item(i, T_LAMP, -4, -4, 4, 4, -1);
        PE(p)->trigger = 1;
        PE(p)->heavy = 1;
        PE(p)->value = 1000;
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
        PE(p)->burnTimer = 0;
        break;
    case OBJ_oRopeTop:
        p->type = T_ROPE;
        make_active(p);
        setCollisionBounds(i, -4, -4, 4, 4);
        break;
    /* the whip */
    case OBJ_oWhip: p->type = T_WHIP; break;
    case OBJ_oWhipPre: p->type = T_WHIP; PE(p)->alarm[0] = 3; break;
    /* solids created in play */
    case OBJ_oPushBlock:
        create_solid(p);
        PE(p)->xVel = PE(p)->yVel = 0;
        PE(p)->myGrav = N(0.6);
        setCollisionBounds(i, 0, 0, 16, 16);
        if (G.cityOfGold) pin_set_sprite(i, GSPR_sGoldBlock);
        break;
    case OBJ_oArrowTrapTest: PE(p)->trapID = NOONE; break;
    /* effects */
    case OBJ_oExplosion:
        p->ispd = (img_t)0.8;
        snd_play(SND_xexplosion);                                              /* objects/oExplosion/Create_0.gml :3 */
        scrShake(5);
        break;
    case OBJ_oBlood:
        if (!create_detritus(i)) break;
        p->ispd = (img_t)0.3;
        {
            double a = prandom(4);
            double b = prandom(4);
            PE(p)->xVel = ND(a - b);
        }
        PE(p)->yVel = ND(-1 - prandom(2));
        PE(p)->grav = NMUL(NI(RAND(1, 6)), N(0.1));
        p->invincible = 1;
        PE(p)->bounce = 0;
        PE(p)->collectible = 0;
        PE(p)->alarm[0] = 1;
        PE(p)->alarm[1] = 1;
        PE(p)->alarm[2] = 5;
        break;
    case OBJ_oFlame:
        if (!create_detritus(i)) break;
        p->ispd = (img_t)0.3;
        {
            double a = prandom(4);
            double b = prandom(4);
            PE(p)->xVel = ND(a - b);
        }
        PE(p)->yVel = ND(-1 - prandom(2));
        PE(p)->grav = NMUL(NI(RAND(1, 6)), N(0.1));
        PE(p)->alarm[0] = 2;
        PE(p)->alarm[1] = 50;
        break;
    case OBJ_oBloodTrail: p->ispd = (img_t)0.8; break;
    case OBJ_oFlameTrail: p->ispd = (img_t)0.4; break;
    case OBJ_oSmokePuff: PE(p)->yVel = N(0.1); PE(p)->yAcc = N(0.1); p->ispd = (img_t)0.4; break;
    case OBJ_oBurn: PE(p)->yVel = N(-0.1); PE(p)->yAcc = N(0.1); p->ispd = (img_t)0.4; break;
    case OBJ_oPoof: PE(p)->xVel = 0; PE(p)->yVel = 0; p->ispd = (img_t)0.4; break;
    case OBJ_oItemsGet: PE(p)->yVel = N(0.1); PE(p)->yAcc = N(0.1); p->ispd = (img_t)0.8; PE(p)->alarm[0] = 40; break;
    case OBJ_oBigCollect: PE(p)->alarm[0] = 30; pin_set_sprite(i, GSPR_sBigCollect); break;
    case OBJ_oRubble: case OBJ_oRubbleSmall: case OBJ_oRubbleDarkSmall:
        p->type = T_NONE;
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        PE(p)->yAcc = N(0.6);
        break;
    case OBJ_oBone: pitems_world(1001, i, 0); break;
    default:
        if (ptrans_create(i)) break;
        if ((pobj[p->obj].ev & EV_CREATE) && !pcontent_ev(FEV_CREATE, i, play_in_gen_init) &&   /* P7 hook */
            !play_in_gen_init)
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
        PE(p)->fired = 0;
        PE(p)->xAct = 0;
        break;
    case OBJ_oPushBlock:
        create_solid(p);
        PE(p)->xVel = PE(p)->yVel = 0;
        PE(p)->myGrav = N(0.6);
        setCollisionBounds(i, 0, 0, 16, 16);
        break;
    case OBJ_oWeb:
        PE(p)->life = N(12);
        PE(p)->dying = 0;
        break;
    case OBJ_oBones:
        PE(p)->yVel = 0;
        PE(p)->yAcc = N(0.2);
        break;
    case OBJ_oLamp: case OBJ_oLampRed:
        p->ispd = (img_t)0.5;
        break;
    case OBJ_oDamsel:
        break;                                                                 /* removed (TRACE_NOENEMY) */
    default:
        if (obj_is(p->obj, OBJ_oItem) || obj_is(p->obj, OBJ_oTreasure) || p->obj == OBJ_oRubble ||
            p->obj == OBJ_oRubbleSmall) {
            int a0 = PE(p)->alarm[0];
            play_in_gen_init = 1;
            ev_create(i);                                                      /* the RNG parts ran in gen */
            play_in_gen_init = 0;
            PE(p)->alarm[0] = a0;
        } else {
            if (obj_is(p->obj, OBJ_oSolid)) create_solid(p);
            if (pobj[p->obj].ev & EV_CREATE) pcontent_ev(FEV_CREATE, i, 1);   /* P7 hook (fromgen 1) */
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
    PE(&PX(g))->xVel = NI(a - b);
    PE(&PX(g))->yVel = NI(RAND(2, 4) * 1);
}

/* objects/oSolid/Destroy_0.gml */
void destroy_solid(int i)
{
    struct pin *p = &PX(i);
    int obj;
    if (p->shopWall) PUNTR(1010);
    if (collision_point_p(PTOD(p->x) + 8, PTOD(p->y) - 1, OBJ_oSpikes, 0, NOONE) != NOONE) {
        obj = instance_place_p(i, PTOD(p->x) + 8, PTOD(p->y) - 1, OBJ_oSpikes);
        if (obj != NOONE) pin_destroy(obj);
    }
    if (collision_point_p(PTOD(p->x) + 8, PTOD(p->y) - 1, OBJ_oTikiTorch, 0, NOONE) != NOONE) pjungle_world(1011, i, 0);
    if (collision_point_p(PTOD(p->x) + 8, PTOD(p->y) - 1, OBJ_oGrave, 0, NOONE) != NOONE) pswamp_world(1012, i, 0);
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
    if (!PE(p)->breakPieces) return;
    snd_play(SND_xbreak);                                                      /* oJar / oSkull Destroy :4 */
    pin_create(p->x, p->y, OBJ_oSmokePuff);
    if (skull) {
        pitems_world(1015, i, skull);                                                            /* oBone pieces */
        return;
    }
    {
        int k;
        for (k = 0; k < 3; k++) {
            int piece = pin_create(p->x - PI(2), p->y - PI(2), OBJ_oRubbleSmall);
            if (PE(p)->colLeft) PE(&PX(piece))->xVel = NI(RAND(1, 3));
            else if (PE(p)->colRight) PE(&PX(piece))->xVel = NI(-RAND(1, 3));
            else {
                int a = RAND(1, 3), b = RAND(1, 3);
                PE(&PX(piece))->xVel = NI(a - b);
            }
            if (PE(p)->colTop) PE(&PX(piece))->yVel = NI(RAND(0, 3));
            else PE(&PX(piece))->yVel = NI(-RAND(0, 3));
        }
    }
    if (RAND(1, 3) == 1) pin_create(p->x, p->y, OBJ_oGoldChunk);
    else if (RAND(1, 6) == 1) pin_create(p->x, p->y, OBJ_oGoldNugget);
    else if (RAND(1, 12) == 1) pin_create(p->x, p->y, OBJ_oEmeraldBig);
    else if (RAND(1, 12) == 1) pin_create(p->x, p->y, OBJ_oSapphireBig);
    else if (RAND(1, 12) == 1) pin_create(p->x, p->y, OBJ_oRubyBig);
    else if (RAND(1, 6) == 1) pitems_world(1016, i, skull);                                     /* a spider: P5 */
    else if (RAND(1, 12) == 1) pjungle_world(1017, i, skull);                                    /* a snake: P5 */
    if (PE(p)->held) {
        PL.holdItem = NOONE;
        G.pickupItem = PICK_NONE;                                              /* oPlayer1.pickupItem = "" */
    }
}

void ev_destroy(int i)
{
    if (front_on && front_ev(FEV_DESTROY, i, 0)) return;                                 /* P8 hook */
    struct pin *p = &PX(i);
    int o = p->obj;
    if (pen_destroy(i) || pdam_destroy(i)) return;                             /* P5 hook */
    if (obj_is(o, OBJ_oItem)) {
        if (o == OBJ_oJar || o == OBJ_oSkull) {                                /* oJar / oSkull Destroy */
            if (PE(p)->held) PL.holdItem = NOONE;                                  /* action_inherited: oItem */
            destroy_jar_like(i, o == OBJ_oSkull);
        } else if (o == OBJ_oBomb) {                                           /* oBomb Destroy (no inherit) */
            if (PE(p)->enemyID != NOONE) pitems_world(1018, i, 0);
        } else if (o == OBJ_oDamsel || o == OBJ_oDice || o == OBJ_oFlare || o == OBJ_oFlareCrate) {
            pitems_world(1019, i, 0);
        } else if (PE(p)->held)                                                    /* objects/oItem/Destroy_0.gml */
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
            if (PE(p)->fired == 0) pin_create(p->x + PI(8), p->y + PI(8), OBJ_oArrow);
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
            if (o != OBJ_oSolid && pcontent_ev(FEV_DESTROY, i, 0)) break;       /* P7 hook (calls destroy_solid
                                                                                  itself when it inherits) */
            if (pobj[o].ev & EV_DESTROY) {
                /* the chain resolves to oSolid's own Destroy unless the object has one */
                int a, own = 0;
                for (a = o; a >= 0 && a != OBJ_oSolid; a = objdefs[a].parent) own = 1;
                (void)own;
                destroy_solid(i);
                if (o != OBJ_oSolid) PUNTR(1020);
            }
        } else if ((pobj[o].ev & EV_DESTROY) && !pcontent_ev(FEV_DESTROY, i, 0))           /* P7 hook */
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
    pin_setdepth(p, G.hasSpectacles ? 51 : 101);                                      /* :5 */
    if ((!instance_exists_p(OBJ_oShopkeeper) || G.thiefLevel > 0 || G.murderer) && PE(p)->cost > 0) {   /* :8 */
        PE(p)->cost = 0;
        PE(p)->forSale = 0;
    }
    if (isRealLevel()) {                                                       /* :18 */
        if (PE(p)->cost > 0 && PE(p)->forSale && !isInShop(PFLOOR(p->x), PFLOOR(p->y))) pshop_item_left_shop(i);   /* P5 hook */
    } else if (isLevel()) {
        if (PE(p)->cost > 0 && PE(p)->forSale && !isInShop(PFLOOR(p->x), PFLOOR(p->y))) pshop_item_left_shop(i);   /* P5 hook */
    } else
        PE(p)->cost = 0;
    if (PE(p)->held) {                                                             /* :37 */
        struct pin *pl = &PX(PL.idx);
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        if (PL.facing == LEFT) pin_setx(p, pl->x - PI(4));
        if (PL.facing == RIGHT) pin_setx(p, pl->x + PI(4));
        if (PE(p)->heavy) {
            if (p->type == T_GOLDIDOL || p->type == T_CRYSTALSKULL || p->type == T_LAMP || p->type == T_DAMSEL) {
                if (PL.state == DUCKING && NLT(NABS(PE(pl)->xVel), N(2))) pin_sety(p, pl->y + PI(2));
                else pin_sety(p, pl->y);
            } else {
                if (PL.state == DUCKING && NLT(NABS(PE(pl)->xVel), N(2))) pin_sety(p, pl->y - PI(2));
                else pin_sety(p, pl->y - PI(4));
            }
        } else {
            if (PL.state == DUCKING && NLT(NABS(PE(pl)->xVel), N(2))) pin_sety(p, pl->y + PI(4));
            else pin_sety(p, pl->y + PI(2));
        }
        pin_setdepth(p, 1);
        if (PL.holdItem == NOONE) PE(p)->held = 0;
    } else if (collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oSolid, 0, NOONE) == NOONE) {   /* :69 */
        moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
        PE(p)->colLeft = PE(p)->colRight = PE(p)->colBot = PE(p)->colTop = 0;
        if (isCollisionLeft(i, 1)) PE(p)->colLeft = 1;
        if (isCollisionRight(i, 1)) PE(p)->colRight = 1;
        if (isCollisionBottom(i, 1)) PE(p)->colBot = 1;
        if (isCollisionTop(i, 1)) PE(p)->colTop = 1;
        if (!PE(p)->colLeft && !PE(p)->colRight) PE(p)->stuck = 0;
        if (!PE(p)->colBot && !PE(p)->stuck) PE(p)->yVel += PE(p)->myGrav;
        if (NGT(PE(p)->yVel, N(8))) PE(p)->yVel = N(8);
        if (PE(p)->colLeft || PE(p)->colRight) {
            PE(p)->xVel = NMUL(-PE(p)->xVel, N(0.5));
            PE(p)->myGrav = N(0.6);
        }
        if (PE(p)->colBot) {
            PE(p)->myGrav = N(0.6);
            if (NGT(PE(p)->yVel, N(1))) PE(p)->yVel = NMUL(-PE(p)->yVel, PE(p)->bounceFactor);
            else PE(p)->yVel = 0;
            if (NLT(NABS(PE(p)->xVel), N(0.1))) PE(p)->xVel = 0;
            else if (NNE(NABS(PE(p)->xVel), N(0))) PE(p)->xVel = NMUL(PE(p)->xVel, PE(p)->frictionFactor);
            if (NLT(NABS(PE(p)->yVel), N(1))) {
                pin_sety(p, p->y - (PI(1)));
                if (!isCollisionBottom(i, 1)) pin_sety(p, p->y + (PI(1)));
                PE(p)->yVel = 0;
            }
        }
        NOPS(12);
        if (PE(p)->sticky && p->type == T_BOMB && p->spr == GSPR_sBombArmed) {     /* :113 */
            pitems_world(1031, i, 0);
        } else if (p->type == T_ARROW && NGT(NABS(PE(p)->xVel), N(6))) {
            if (PE(p)->colLeft) {
                pin_setx(p, p->x - (PI(2)));
                PE(p)->xVel = 0;
                PE(p)->yVel = 0;
            } else if (PE(p)->colRight) {
                pin_setx(p, p->x + (PI(2)));
                PE(p)->xVel = 0;
                PE(p)->yVel = 0;
            }
            PE(p)->stuck = 1;
        } else if (PE(p)->colLeft && !PE(p)->stuck) {
            if (!PE(p)->colRight) pin_setx(p, p->x + (PI(1)));
        } else if (PE(p)->colRight && !PE(p)->stuck)
            pin_setx(p, p->x - (PI(1)));
        if (PE(p)->sticky && p->type == T_BOMB && p->spr == GSPR_sBombArmed) {
        } else if (isCollisionTop(i, 1)) {                                     /* :153 */
            if (NLT(PE(p)->yVel, N(0))) PE(p)->yVel = NMUL(-PE(p)->yVel, N(0.8));
            else pin_sety(p, p->y + (PI(1)));
            PE(p)->myGrav = N(0.6);
        }
        if (collision_rect_p(PTOD(p->x) - 3, PTOD(p->y) - 3, PTOD(p->x) + 3, PTOD(p->y) + 3, OBJ_oLava, 0, NOONE) != NOONE)
            ptemple_world(1032, i, 0);
        else
            PE(p)->myGrav = N(0.6);
        if (collision_point_p(PTOD(p->x), PTOD(p->y) - 5, OBJ_oLava, 0, NOONE) != NOONE && p->type != T_SCEPTRE)
            ptemple_world(1032, i, 0);
    } else {                                                                   /* :187 */
        PE(p)->colLeft = PE(p)->colRight = PE(p)->colBot = PE(p)->colTop = 0;
        if (isCollisionLeft(i, 1)) PE(p)->colLeft = 1;
        if (isCollisionRight(i, 1)) PE(p)->colRight = 1;
        if (isCollisionBottom(i, 1)) PE(p)->colBot = 1;
        if (isCollisionTop(i, 1)) PE(p)->colTop = 1;
        if (PE(p)->colTop && !PE(p)->colBot) pin_sety(p, p->y + (PI(1)));
        else if (PE(p)->colLeft && !PE(p)->colRight) pin_setx(p, p->x + (PI(1)));
        else if (PE(p)->colRight && !PE(p)->colLeft) pin_setx(p, p->x - (PI(1)));
        else {
            PE(p)->xVel = 0;
            PE(p)->yVel = 0;
        }
    }
    if (p->type == T_BOMB && PE(p)->sticky) {                                      /* :217 */
        pitems_world(1033, i, 0);
    } else if (NGT(NABS(PE(p)->xVel), N(2)) || NGT(NABS(PE(p)->yVel), N(2))) {
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
    PE(p)->colTop = PE(p)->colLeft = PE(p)->colRight = PE(p)->colBot = 0;
    if (PE(p)->held) {
        struct pin *pl = &PX(PL.idx);
        if (PL.facing == LEFT) pin_setx(p, pl->x - PI(4));
        else if (PL.facing == RIGHT) pin_setx(p, pl->x + PI(4));
        if (PL.state == DUCKING && NLT(NABS(PE(pl)->xVel), N(2))) pin_sety(p, pl->y + PI(4));
        else pin_sety(p, pl->y);
        pin_setdepth(p, 1);
    } else {
        moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
        if (NLT(PE(p)->yVel, N(6))) PE(p)->yVel += PE(p)->myGrav;
        if (isCollisionTop(i, 1)) PE(p)->colTop = 1;
        if (isCollisionLeft(i, 1)) PE(p)->colLeft = 1;
        if (isCollisionRight(i, 1)) PE(p)->colRight = 1;
        if (isCollisionBottom(i, 1)) PE(p)->colBot = 1;
        if (PE(p)->colTop && NLT(PE(p)->yVel, N(0))) {
            if (skull ? NLT(PE(p)->yVel, N(2)) : NLT(PE(p)->yVel, N(-3))) destroy = 1;
            PE(p)->yVel = NMUL(-PE(p)->yVel, N(0.8));
        }
        if (PE(p)->colLeft || PE(p)->colRight) {
            if (NABS(PE(p)->xVel) > (skull ? N(2) : N(3))) destroy = 1;
            PE(p)->xVel = NMUL(-PE(p)->xVel, N(0.5));
        }
        if (!skull && collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oSolid, 0, NOONE) != NOONE) destroy = 1;
        if (PE(p)->colBot) {
            if (NGT(PE(p)->yVel, N(3))) destroy = 1;
            if (NGT(PE(p)->yVel, N(1))) PE(p)->yVel = NMUL(-PE(p)->yVel, N(0.5));
            else PE(p)->yVel = 0;
            if (NLT(NABS(PE(p)->xVel), N(0.1))) PE(p)->xVel = 0;
            else if (NNE(NABS(PE(p)->xVel), N(0))) PE(p)->xVel = NMUL(PE(p)->xVel, N(0.3));
        }
        if (PE(p)->colLeft) {
            if (!PE(p)->colRight) pin_setx(p, p->x + (PI(1)));
            PE(p)->yVel = 0;
        } else if (PE(p)->colRight) {
            pin_setx(p, p->x - (PI(1)));
            PE(p)->yVel = 0;
        }
        if (isCollisionBottom(i, 0) && NLT(NABS(PE(p)->yVel), N(1))) {
            pin_sety(p, p->y - (PI(1)));
            PE(p)->yVel = 0;
        }
        pin_setdepth(p, 100);
        if (collision_rect_p(PTOD(p->x) - 3, PTOD(p->y) - 3, PTOD(p->x) + 3, PTOD(p->y) + 3, OBJ_oLava, 0, NOONE) != NOONE ||
            collision_point_p(PTOD(p->x), PTOD(p->y) - 5, OBJ_oLava, 0, NOONE) != NOONE)
            ptemple_world(1036, i, skull);
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
        if (PE(p)->held) {
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
    if (!(inview(i, 16) && PE(p)->state == 1))
        return;
    PE(p)->colLeft = PE(p)->colRight = PE(p)->colBot = 0;
    if (isCollisionLeft(i, 1)) PE(p)->colLeft = 1;
    if (isCollisionRight(i, 1)) PE(p)->colRight = 1;
    if (isCollisionBottom(i, 1)) PE(p)->colBot = 1;
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    if (!PE(p)->colBot) PE(p)->yVel += PE(p)->myGrav;
    if (NGT(PE(p)->yVel, N(8))) PE(p)->yVel = N(8);
    if (isCollisionTop(i, 1)) {
        if (NLT(PE(p)->yVel, N(0))) PE(p)->yVel = NMUL(-PE(p)->yVel, N(0.8));
        else pin_sety(p, p->y + (PI(1)));
    }
    if (PE(p)->colLeft || PE(p)->colRight) PE(p)->xVel = NMUL(-PE(p)->xVel, N(0.5));
    if (PE(p)->colBot) {
        if (NLT(NABS(PE(p)->xVel), N(0.1))) PE(p)->xVel = 0;
        else if (NNE(NABS(PE(p)->xVel), N(0))) PE(p)->xVel = NMUL(PE(p)->xVel, N(0.3));
        pin_sety(p, p->y - (PI(1)));
        if (!isCollisionBottom(i, 1)) {
            pin_sety(p, p->y + (PI(1)));
            PE(p)->status = 0;                                                     /* status = STATIC (not state) */
        }
        PE(p)->yVel = 0;
    }
    if (PE(p)->colLeft) {
        if (!PE(p)->colRight) pin_setx(p, p->x + (PI(1)));
    } else if (PE(p)->colRight)
        pin_setx(p, p->x - (PI(1)));
    if (G.hasSpectacles || PG.hasUdjatEye) pin_setdepth(p, 0);
    else pin_setdepth(p, 101);
    NOPS(8);
    if (collision_rect_p(PTOD(p->x) - 3, PTOD(p->y) - 3, PTOD(p->x) + 3, PTOD(p->y) + 3, OBJ_oLava, 0, NOONE) != NOONE ||
        collision_point_p(PTOD(p->x), PTOD(p->y) - 5, OBJ_oLava, 0, NOONE) != NOONE)
        ptemple_world(1039, i, 0);
}

/* objects/oDetritus/Step_0.gml (returns 0 if it destroyed itself) */
void detritus_step(int i)
{
    struct pin *p = &PX(i);
    double x = PTOD(p->x), y = PTOD(p->y);
    view_read();
    if (DLT(x, PW.xview - 4) || DGT(x, PW.xview + 320 + 4) || DLT(y, PW.yview - 4) || DGT(y, PW.yview + 240 + 4))
        pin_destroy(i);
    if (NGT(PE(p)->life, N(0))) PE(p)->life -= N(1);
    else pin_destroy(i);
    moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    if (collision_point_p(PTOD(p->x), PTOD(p->y) - 4, OBJ_oLava, 0, NOONE) != NOONE) ptemple_world(1040, i, 0);
    if (PE(p)->bounce) {
        if (NLT(PE(p)->yVel, N(6))) PE(p)->yVel += PE(p)->grav;
        if (isCollisionTop(i, 1) && NLT(PE(p)->yVel, N(0))) PE(p)->yVel = NMUL(-PE(p)->yVel, N(0.8));
        if (isCollisionLeft(i, 1) || isCollisionRight(i, 1)) PE(p)->xVel = NMUL(-PE(p)->xVel, N(0.5));
        if (isCollisionBottom(i, 1)) {
            if (NGT(PE(p)->yVel, N(1))) PE(p)->yVel = NMUL(-PE(p)->yVel, N(0.5));
            else PE(p)->yVel = 0;
        }
        NOPS(6);
    }
}

/* objects/oRubblePiece/Step_0.gml */
static void rubble_step(int i)
{
    struct pin *p = &PX(i);
    double x, y;
    pin_setx(p, PADDV(p->x, PE(p)->xVel));
    pin_sety(p, PADDV(p->y, PE(p)->yVel));
    PE(p)->yVel += PE(p)->yAcc;
    NOPS(3);
    x = PTOD(p->x);
    y = PTOD(p->y);
    if (collision_point_p(x, y, OBJ_oWaterSwim, 0, NOONE) != NOONE) pswamp_world(1041, i, 0);
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
    if (p->spr == GSPR_sBombArmed) pin_setdepth(p, 49);
    if (PE(p)->sticky) pin_setdepth(p, 1);
    if (PE(p)->armed && instance_exists_p(OBJ_oShopkeeper)) pitems_world(1042, i, 0);
}

/* objects/oRopeThrow/Step_0.gml (after oItem's) */
static void ropethrow_step(int i)
{
    struct pin *p = &PX(i);
    item_step(i);
    p = &PX(i);
    if (PE(p)->armed && NGE(PE(p)->yVel, N(0))) {
        move_snap(i, 16, 1);
        if (PE(p)->px < NP(p->x)) {
            if (collision_point_p(PTOD(p->x) - 8, PTOD(p->y), OBJ_oSolid, 0, NOONE) == NOONE) pin_setx(p, p->x - (PI(8)));
            else pin_setx(p, p->x + (PI(8)));
        } else {
            if (collision_point_p(PTOD(p->x) + 8, PTOD(p->y), OBJ_oSolid, 0, NOONE) == NOONE) pin_setx(p, p->x + (PI(8)));
            else pin_setx(p, p->x - (PI(8)));
        }
        pin_create(p->x, p->y, OBJ_oRopeTop);
        p = &PX(i);
        PE(p)->armed = 0;
        PE(p)->falling = 1;
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
    }
    if (PE(p)->falling) {
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        pin_sety(p, p->y + (PI(8)));
        PE(p)->fallCount += 1;
        if (isCollisionBottom(i, 1) || PE(p)->fallCount > 16) {
            PE(p)->falling = 0;
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
        double xv = NTOD(PE(p)->xVel), yv = NTOD(PE(p)->yVel);
        extern double patan_deg(double a);
        if (xv > 0 && yv < 0) PE(p)->direction = patan_deg(-yv / xv);
        else if (xv < 0 && yv < 0) PE(p)->direction = 180 - patan_deg(-yv / -xv);
        else if (xv > 0 && yv > 0) PE(p)->direction = patan_deg(yv / xv);
        else if (xv < 0 && yv > 0) PE(p)->direction = 180 + patan_deg(yv / -xv);
        else if (xv < 0) PE(p)->direction = 180;
        else if (!PE(p)->stuck) PE(p)->direction = 0;
        pin_setangle(p, (float)PE(p)->direction);                                     /* image_angle: a float */
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
            if (!PE(p)->held && collision_point_p(PTOD(p->x), PTOD(p->y) + 4, OBJ_oBrickSmooth, 0, NOONE) != NOONE &&
                instance_exists_p(OBJ_oShopkeeper) && G.thiefLevel == 0 && !G.murderer)
                pitems_world(1043, i, 0);
        }
        if (!PE(p)->colBot && PE(p)->trigger)
            PE(p)->trigger = 0;
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
                PE(p)->yVel += PE(p)->myGrav;
                if (NGT(PE(p)->yVel, N(8))) PE(p)->yVel = N(8);
                NOPS(2);
                for (; DLT(PTOD(p->y), PTOD(yMPrev) + NTOD(PE(p)->yVel)); pin_sety(p, p->y + (PI(1)))) {
                    if (place_meeting_p(j, PTOD(p->x), PTOD(p->y) + 1, OBJ_oSolid)) {
                        if (NGT(PE(p)->yVel, PE(p)->myGrav)) snd_play(SND_xthud);     /* :258 */
                        PE(p)->yVel = 0;
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
    else pitems_world(1051, i, 0);
    if (G.gameStart && instance_exists_p(OBJ_oCharacter) && isLevel()) {      /* :13 */
        if (!PL.dead) {
            PG.time += 30;
            PG.xtime += 30;
        }
    }
    if (instance_exists_p(OBJ_oPlayer1)) {                                     /* :45 ghost */
        if (isLevel() && !isRoomIs(R_rOlmec) && G.currLevel > 1 && !PG.hasCrown && PG.xtime > 120000 &&
            !spr_is_exit_g(PX(PL.idx).spr) && !PLEV.musicFade) {
            PLEV.musicFade = 1;
            pmsg_str("A CHILL RUNS UP YOUR SPINE...", "LET'S GET OUT OF HERE!", 200);   /* :56 */
        }
        if (isLevel() && !isRoomIs(R_rOlmec) && G.currLevel > 1 && !PG.hasCrown && PG.xtime > 150000 &&
            !PG.ghostExists && !spr_is_exit_g(PX(PL.idx).spr))
            pswamp_world(1052, i, 0);
    }
    if (G.checkWater) {                                                        /* :64 */
        if (instance_exists_p(OBJ_oWater)) pswamp_world(1053, i, 0);          /* clears it when waterCounter == 0 */
        else G.checkWater = 0;                                                 /* waterCounter == 0 */
    }
    if (instance_exists_p(OBJ_oPlayer1) && PL.dead) {                          /* :127 game over */
        if (PGAME.drawStatus == 0) {
            PE(g)->alarm[0] = 50;
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
    if (front_on && front_ev(FEV_STEP, i, 0)) return;                                 /* P8 hook */
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
        if (PE(&PX(i))->held) pin_set_sprite(i, PL.facing == 18 ? GSPR_sKeyLeft : GSPR_sKeyRight);
        break;
    case OBJ_oWhip: whip_step(i, 0); break;
    case OBJ_oWhipPre: whip_step(i, 1); break;
    case OBJ_oBlood:                                                           /* oBlood Step: inherited first */
    case OBJ_oFlame:
        detritus_step(i);
        p = &PX(i);
        if (NGT(PE(p)->yVel, N(6))) pin_destroy(i);
        if (isCollisionBottom(i, 1)) {
            if (NGT(PE(p)->life, N(20))) PE(p)->life = N(20);
        }
        break;
    case OBJ_oPoof:
        pin_setx(p, PADDV(p->x, PE(p)->xVel));
        pin_sety(p, PADDV(p->y, PE(p)->yVel));
        break;
    case OBJ_oSmokePuff: pin_sety(p, PSUBV(p->y, PE(p)->yVel)); break;
    case OBJ_oBurn:
        pin_sety(p, PADDV(p->y, PE(p)->yVel));
        if (collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oSolid, 0, NOONE) != NOONE) pin_destroy(i);
        break;
    case OBJ_oItemsGet:
        pin_sety(p, PSUBV(p->y, PE(p)->yVel));
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
        PE(p)->alpha = (float)(NTOD(PE(p)->life) / 12);                   /* image_alpha: a float */
        if (PE(p)->dying) PE(p)->life -= N(0.02);
        if (NLE(PE(p)->life, N(1))) pin_destroy(i);
        break;
    case OBJ_oRope:
        if (collision_point_p(PTOD(p->x) + 12, PTOD(p->y), OBJ_oLava, 0, NOONE) != NOONE && PE(p)->burnTimer == 0) ptemple_world(1056, i, 0);
        if (PE(p)->burnTimer > 1) PE(p)->burnTimer -= 1;
        else if (PE(p)->burnTimer == 1) ptemple_world(1056, i, 0);
        break;
    case OBJ_oArrowTrapLeft: case OBJ_oArrowTrapLeftLit: case OBJ_oArrowTrapRight: case OBJ_oArrowTrapRightLit:
        break;                                                                 /* firing = false; the rest commented */
    case OBJ_oBones:                                                           /* objects/oBones/Step_0.gml */
        if (collision_point_p(PTOD(p->x) + 8, PTOD(p->y) + 16, OBJ_oSolid, 0, NOONE) == NOONE) {
            pin_sety(p, PADDV(p->y, PE(p)->yVel));
            PE(p)->yVel += PE(p)->yAcc;
        }
        if (collision_point_p(PTOD(p->x) + 8, PTOD(p->y) + 15, OBJ_oSolid, 0, NOONE) != NOONE) pin_sety(p, p->y - (PI(1)));
        break;
    case OBJ_oGamepad: break;                                                  /* prun.c */
    default:
        if (ptrans_step(i)) break;
        if (obj_is(p->obj, OBJ_oTreasure)) treasure_step(i);
        else if (obj_is(p->obj, OBJ_oItem)) {
            if (p->obj == OBJ_oDice) {                                         /* objects/oDice/Step_0.gml */
                if (inview(i, 16)) pitems_world(1064, i, 0);                                /* :1-208: P5 (shops) */
                if (NGT(NABS(PE(p)->yVel), N(2)) || NGT(NABS(PE(p)->xVel), N(2))) {          /* :210 */
                    pin_set_sprite(i, GSPR_sDiceRoll);
                    PE(p)->value = RAND(1, 6);
                    if (PL.bet > 0) PE(p)->rolling = 1;                                  /* :214 */
                } else if (isCollisionBottom(i, 1)) {
                    static const int16_t dice[6] = { GSPR_sDice1, GSPR_sDice2, GSPR_sDice3, GSPR_sDice4,
                                                     GSPR_sDice5, GSPR_sDice6 };
                    if (PE(p)->rolling && NEQ(PE(p)->yVel, N(0))) pitems_world(1065, i, 0);
                    pin_set_sprite(i, dice[(PE(p)->value >= 1 && PE(p)->value <= 5) ? PE(p)->value - 1 : 5]);
                }
            } else if (p->obj == OBJ_oDamsel || p->obj == OBJ_oFlare || p->obj == OBJ_oFlareCrate ||
                p->obj == OBJ_oLockedChest || p->obj == OBJ_oMattock || p->obj == OBJ_oWebCannon)
                pitems_world(1060, i, 0);
            else if (!pcontent_ev(FEV_STEP, i, 0))                            /* P7 hook (calls item_step
                                                                                  itself when it inherits) */
                item_step(i);
        } else if (!pcontent_ev(FEV_STEP, i, 0))                                       /* P7 hook */
            PUNTR(1061);
        break;
    }
}

void ev_end_step(int i)
{
    if (front_on && front_ev(FEV_END_STEP, i, 0)) return;                                 /* P8 hook */
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oPlayer1: pl_end_step(i); break;
    case OBJ_oBomb:                                                            /* objects/oBomb/Step_2.gml */
        if (PE(p)->enemyID != NOONE && !PX(PE(p)->enemyID).alive) PE(p)->enemyID = NOONE;
        if (PE(p)->enemyID != NOONE) pitems_world(1062, i, 0);
        break;
    case OBJ_oGamepad: break;
    default: if (!pcontent_ev(FEV_END_STEP, i, 0)) PUNTR(1063); break;            /* P7 hook */
    }
}

/* ---- Alarms ---------------------------------------------------------------------------------------------- */
void ev_alarm(int i, int a)
{
    if (front_on && front_ev(FEV_ALARM, i, a)) return;                                 /* P8 hook */
    struct pin *p = &PX(i);
    int o = p->obj;
    if (o == OBJ_oPlayer1) { pl_alarm(i, a); return; }
    if (pen_alarm(i, a) || pshop_alarm(i, a) || pdam_alarm(i, a)) return;     /* P5 hook */
    if (obj_is(o, OBJ_oTreasure) && a == 0) { PE(p)->canCollect = 1; return; }     /* gems' Alarm_0 */
    switch (o) {
    case OBJ_oBomb:
        if (a == 0) {                                                          /* objects/oBomb/Alarm_0.gml */
            p->ispd = 1;
            PE(p)->alarm[1] = 40;
        } else if (a == 1) {                                                   /* Alarm_1 */
            pin_create(p->x, p->y, OBJ_oExplosion);
            if (G.graphicsHigh) scrCreateFlame(p->x, p->y, 3);
            p = &PX(i);
            if (PE(p)->held) PL.holdItem = NOONE;
            pin_destroy(i);
        } else if (a == 2)                                                     /* oItem Alarm_2 */
            PE(p)->safe = 0;
        break;
    case OBJ_oArrow:
        if (a == 1) pitems_world(1070, i, a);                                               /* bomb arrows */
        else if (a == 2) PE(p)->safe = 0;                                          /* objects/oArrow/Alarm_2.gml */
        break;
    case OBJ_oArrowTrapLeft: case OBJ_oArrowTrapLeftLit:
        if (a == 0) {
            int ar = pin_create(p->x + PI(16), p->y + PI(4), OBJ_oArrow);
            PE(&PX(ar))->xVel = N(5);
        } else if (a == 1 && !isRoomIs(R_rLevelEditor)) {                      /* objects/oArrowTrapLeft/Alarm_1.gml */
            int xAct = PFLOOR(p->x) - 1, obj;
            while (collision_point_p(xAct, PTOD(p->y) + 8, OBJ_oSolid, 0, NOONE) == NOONE) {
                if (PFLOOR(p->x) - xAct > 96) break;
                xAct -= 1;
            }
            if (xAct > PFLOOR(p->x) - 16) xAct = PFLOOR(p->x) - 16;
            PE(p)->xAct = (int16_t)xAct;
            obj = pin_create(PI(xAct), p->y, OBJ_oArrowTrapTest);
            pin_setxscale(&PX(obj), dceil(((PFLOOR(PX(i).x) - 1) - xAct) / 16.0));
            PE(&PX(obj))->trapID = (int16_t)i;
        }
        break;
    case OBJ_oArrowTrapRight: case OBJ_oArrowTrapRightLit:
        if (a == 0) pitems_world(1071, i, a);
        else if (a == 1 && !isRoomIs(R_rLevelEditor)) {                        /* objects/oArrowTrapRight/Alarm_1.gml */
            int x = PFLOOR(p->x), xAct = x + 16, n = 100, obj;
            while (collision_point_p(xAct, PTOD(p->y) + 8, OBJ_oSolid, 0, NOONE) == NOONE && n > 0) {
                if (xAct - x > 96) break;
                xAct += 1;
                n -= 1;
            }
            xAct -= x + 8;
            if (xAct < 32) xAct = 32;
            PE(p)->xAct = (int16_t)xAct;
            obj = pin_create(PI(x + 16), p->y, OBJ_oArrowTrapTest);
            pin_setxscale(&PX(obj), dceil((xAct - 16) / 16.0));
            PE(&PX(obj))->trapID = (int16_t)i;
        }
        break;
    case OBJ_oWhipPre: if (a == 0) pin_destroy(i); break;
    case OBJ_oBlood:
        if (a == 0) {                                                          /* objects/oBlood/Alarm_0.gml */
            if (G.graphicsHigh) {
                if (instance_number_p(OBJ_oBloodTrail) < 12) pin_create(p->x, p->y, OBJ_oBloodTrail);
                PE(&PX(i))->alarm[0] = 4;
            }
        } else if (a == 1) {
            p->invincible = 0;
            PE(p)->bounce = 1;
        } else if (a == 2)
            PE(p)->collectible = 1;
        break;
    case OBJ_oFlame:
        if (a == 0) {                                                          /* objects/oFlame/Alarm_0.gml */
            if (instance_number_p(OBJ_oFlameTrail) < 12) pin_create(p->x, p->y, OBJ_oFlameTrail);
            PE(&PX(i))->alarm[0] = 2;
        }
        break;
    case OBJ_oItemsGet: case OBJ_oBigCollect: if (a == 0) pin_destroy(i); break;
    case OBJ_oGame:
        if (a == 0) {                                                          /* objects/oGame/Alarm_0.gml */
            if (PGAME.drawStatus < 3) PGAME.drawStatus = 2;
            PE(p)->alarm[1] = 50;
        } else if (a == 1) {
            if (PGAME.drawStatus < 3) PGAME.drawStatus = 3;
        } else if (a == 2)
            PG.udjatBlink = !PG.udjatBlink;
        break;
    case OBJ_oGiantTikiHead: PUNTR(1072); break;                               /* the boulder: P5 */
    default:
        if (ptrans_alarm(i, a)) break;
        if (obj_is(o, OBJ_oItem) && a == 2) PE(p)->safe = 0;                       /* objects/oItem/Alarm_2.gml */
        else if (!pcontent_ev(FEV_ALARM, i, a)) PUNTR(1073);                          /* P7 hook */
        break;
    }
}

/* ---- Animation End --------------------------------------------------------------------------------------- */
void ev_animend(int i)
{
    if (front_on && front_ev(FEV_ANIMEND, i, 0)) return;                                 /* P8 hook */
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
        if (!ptrans_animend(i) && !pcontent_ev(FEV_ANIMEND, i, 0)) PUNTR(1080);       /* P7 hook */
        break;
    }
}

/* ---- Collision events ------------------------------------------------------------------------------------ */
static void trap_fire(int self, int other)
{
    int t = PE(&PX(self))->trapID;
    (void)other;
    if (t == NOONE) {                                                          /* trapID == 0 */
        pin_destroy(self);
        return;
    }
    if (PX(t).alive && PE(&PX(t))->fired == 0) {                                     /* with trapID */
        int ar;
        if (PE(&PX(t))->facing == 0) {
            ar = pin_create(PX(t).x - PI(2), PX(t).y + PI(4), OBJ_oArrow);
            PE(&PX(ar))->xVel = N(-8);
        } else {
            ar = pin_create(PX(t).x + PI(18), PX(t).y + PI(4), OBJ_oArrow);
            PE(&PX(ar))->xVel = N(8);
        }
        PE(&PX(t))->fired += 1;
        snd_play(SND_xarrowtrap);                                              /* oArrowTrapTest/Collision_* :22-25 */
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
        for (k = 0; k < n; k++) if (PX(w[k]).alive) PE(&PX(w[k]))->state = 1;
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
        PE(o)->alarm[1] = RAND(4, 8);
        PE(o)->enemyID = NOONE;
        if (o->y < e->y) PE(o)->yVel = NI(-RAND(2, 4));
        if (o->x < e->x) PE(o)->xVel = NI(-RAND(2, 4));
        else PE(o)->xVel = NI(RAND(2, 4));
    } else if (o->type == T_ROPE) {
        if (!PE(o)->falling) {
            if (o->y < e->y) PE(o)->yVel -= N(6);
            else PE(o)->yVel += N(6);
            if (e->x > o->x) PE(o)->xVel -= NI(RAND(4, 6));
            else PE(o)->xVel += NI(RAND(4, 6));
        }
    } else {
        if (o->y < e->y) PE(o)->yVel -= N(6);
        else PE(o)->yVel += N(6);
        if (e->x > o->x) PE(o)->xVel -= NI(RAND(4, 6));
        else PE(o)->xVel += NI(RAND(4, 6));
    }
    o = &PX(other);
    if (PE(o)->held) {
        PL.holdItem = NOONE;
        PL.pickupItemType = T_NONE;
        PE(o)->held = 0;
    }
}

void ev_collision(int self, int other)
{
    if (front_on && front_ev(FEV_COLLISION, self, other)) return;                                 /* P8 hook */
    int so = PX(self).obj, oo = PX(other).obj;
    if (pen_collision(self, other) || pdam_collision(self, other) || pshop_collision(self, other) ||
        pitem_collision(self, other))
        return;                                                                /* P5 hook */
    switch (so) {
    case OBJ_oPlayer1: pl_collision(self, other); break;
    case OBJ_oArrowTrapTest:
        if (obj_is(oo, OBJ_oCharacter)) {
            struct pin *pl = &PX(PL.idx);
            if (PE(&PX(self))->trapID == NOONE) pin_destroy(self);
            else if (NGT(NABS(PE(&PX(other))->xVel), N(0)) || NGT(NABS(PE(&PX(other))->yVel), N(0)) ||
                     (pl->spr == GSPR_sDuckToHangL && DGT(pl->img, 6)) || (pl->spr == GSPR_sDamselDtHL && DGT(pl->img, 6)) ||
                     (pl->spr == GSPR_sTunnelDtHL && DGT(pl->img, 6)))
                trap_fire(self, other);
        } else {                                                               /* P5: oBoulder as the others */
            if (PE(&PX(self))->trapID == NOONE) pin_destroy(self);
            else if (NGT(NABS(PE(&PX(other))->xVel), N(0)) || NGT(NABS(PE(&PX(other))->yVel), N(0)))
                trap_fire(self, other);
        }
        break;
    case OBJ_oFlame:                                                           /* objects/oFlame/Collision_oWater.gml */
        if (!obj_is(oo, OBJ_oWater)) { PUNTR(1096); break; }
        pin_create(PX(self).x, PX(self).y, OBJ_oSmokePuff);
        pin_destroy(self);
        break;
    case OBJ_oExplosion:
        if (obj_is(oo, OBJ_oSolid)) explosion_solid(self, other);
        else if (obj_is(oo, OBJ_oItem)) {
            if (oo == OBJ_oDamsel) PUNTR(1091);
            else explosion_item(self, other);
        } else if (obj_is(oo, OBJ_oWeb)) pin_destroy(other);
        else if (obj_is(oo, OBJ_oEnemy) && oo != OBJ_oMagmaMan) {         /* objects/oExplosion/Collision_oEnemy.gml */
            struct pin *o = &PX(other);
            if (!o->invincible) {
                PE(o)->hp -= 30;
                if (PX(self).x < o->x) PE(o)->xVel = NI(RAND(4, 6));
                else PE(o)->xVel = NI(-RAND(4, 6));
                PE(o)->yVel = N(-6);
                PEN(o)->burning = 50;
            }
        } else if (oo == OBJ_oMagmaMan) pcontent_enemy(5017, other, self);     /* Collision_oEnemy :1-11: P7 (D) */
        else if (!pcontent_ev(FEV_COLLISION, self, other))                     /* P7 hook (oBarrierEmitter: C) */
            PUNTR(1092);
        break;
    case OBJ_oWeb:                                                             /* objects/oWeb/Collision_*.gml */
        if (obj_is(oo, OBJ_oItem)) {
            if (!PE(&PX(other))->held && PX(other).type != T_ROPE) {
                PE(&PX(other))->xVel = 0;
                PE(&PX(other))->yVel = 0;
            }
        } else if (obj_is(oo, OBJ_oRubblePiece)) {
            PE(&PX(other))->xVel = 0;
            PE(&PX(other))->yVel = 0;
            PX(other).ispd = 0;
        } else if (obj_is(oo, OBJ_oTreasure)) {
            PE(&PX(other))->xVel = 0;
            PE(&PX(other))->yVel = 0;
        } else if (!pcontent_ev(FEV_COLLISION, self, other))                       /* P7 hook (Collision_oSlash) */
            PUNTR(1093);
        break;
    case OBJ_oJar:
        if (obj_is(oo, OBJ_oWhip)) {                                           /* objects/oJar/Collision_oWhip.gml */
            struct pin *p = &PX(self);
            int k;
            snd_play(SND_xbreak);                                              /* :1 */
            pin_create(p->x, p->y, OBJ_oSmokePuff);
            for (k = 0; k < 3; k++) {
                int piece = pin_create(PX(self).x - PI(2), PX(self).y - PI(2), OBJ_oRubbleSmall);
                int a = RAND(1, 3), b = RAND(1, 3);
                PE(&PX(piece))->xVel = NI(a - b);
            }
            p = &PX(self);
            if (RAND(1, 3) == 1) pin_create(p->x, p->y, OBJ_oGoldChunk);
            else if (RAND(1, 6) == 1) pin_create(p->x, p->y, OBJ_oGoldNugget);
            else if (RAND(1, 12) == 1) pin_create(p->x, p->y, OBJ_oEmeraldBig);
            else if (RAND(1, 12) == 1) pin_create(p->x, p->y, OBJ_oSapphireBig);
            else if (RAND(1, 12) == 1) pin_create(p->x, p->y, OBJ_oRubyBig);
            else if (RAND(1, 6) == 1) pin_create(p->x - PI(8), p->y - PI(8), OBJ_oSpider);   /* P5 */
            else if (RAND(1, 12) == 1) pin_create(p->x - PI(8), p->y - PI(8), OBJ_oSnake);
            if (PE(&PX(self))->held) {
                PL.holdItem = NOONE;
                PL.pickupItemType = T_NONE;
            }
            pin_destroy(self);
        } else
            ptemple_world(1094, self, other);
        break;
    case OBJ_oLockedChest: pitems_world(1095, self, other); break;
    default: if (!pcontent_ev(FEV_COLLISION, self, other)) PUNTR(1096); break;    /* P7 hook */
    }
}

void ev_draw(int i)
{
    if (front_on && front_ev(FEV_DRAW, i, 0)) return;                                 /* P8 hook */
    if (PX(i).obj == OBJ_oPlayer1) pl_draw(i);
    else if (pen_draw(i) || pdam_draw(i) || pshop_draw(i)) return;             /* P5 hook */
    else if (pcontent_ev(FEV_DRAW, i, 0)) return;                              /* P7 hook */
    else ptrans_draw(i);
}

void ev_outside(int i)
{
    if (front_on && front_ev(FEV_OUTSIDE, i, 0)) return;                                 /* P8 hook */
    if (PX(i).obj == OBJ_oPushBlock) pin_destroy(i);                           /* objects/oPushBlock/Other_0.gml */
    else if (pen_outside(i)) return;                                           /* P5 hook */
    else if (!pcontent_ev(FEV_OUTSIDE, i, 0)) PUNTR(1097);                     /* P7 hook */
}
