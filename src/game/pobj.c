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
#include "pcol.h"                                /* pcol_quiet (the resting-object skip) */
#include "pmath.h"                               /* patan_deg */
#include "pcmpc.h"                               /* jar_step's compares against constants on the bits */

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
    p->type = (int8_t)type;
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
    p->type = (int8_t)type;
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
    switch (p->obj) {
    /* the debris made in numbers (an explosion's rubble and flames, blood, the trails, the drips): none of the P5 hooks
       has a case for them (each is a switch on the object, or pitem_create's oUdjatEye test, that returns 0 and does
       nothing for any other object), so their Create starts at the switch below */
    case OBJ_oBlood: case OBJ_oFlame: case OBJ_oBloodTrail: case OBJ_oFlameTrail: case OBJ_oSmokePuff: case OBJ_oBurn:
    case OBJ_oPoof: case OBJ_oRubble: case OBJ_oRubbleSmall: case OBJ_oRubbleDarkSmall: case OBJ_oExplosion:
    case OBJ_oDrip: case OBJ_oLavaDrip:
        break;
    default:
        if (pen_create(i, play_in_gen_init) || pdam_create(i, play_in_gen_init) || pshop_create(i, play_in_gen_init) ||
            pitem_create(i, play_in_gen_init))
            return;                                                            /* P5 hook */
        break;
    }
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
    case OBJ_oBow: item(i, T_BOW, -4, -4, 4, 4, 1000); pin_setispd(p, 0); break;
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
        pin_setispd(p, (img_t)0.8);
        snd_play(SND_xexplosion);                                              /* objects/oExplosion/Create_0.gml :3 */
        scrShake(5);
        break;
    case OBJ_oBlood:
        if (!create_detritus(i)) break;
        pin_setispd(p, (img_t)0.3);
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
        pin_setispd(p, (img_t)0.3);
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
    case OBJ_oBloodTrail: pin_setispd(p, (img_t)0.8); break;
    case OBJ_oFlameTrail: pin_setispd(p, (img_t)0.4); break;
    case OBJ_oSmokePuff: PE(p)->yVel = N(0.1); PE(p)->yAcc = N(0.1); pin_setispd(p, (img_t)0.4); break;
    case OBJ_oBurn: PE(p)->yVel = N(-0.1); PE(p)->yAcc = N(0.1); pin_setispd(p, (img_t)0.4); break;
    case OBJ_oPoof: PE(p)->xVel = 0; PE(p)->yVel = 0; pin_setispd(p, (img_t)0.4); break;
    case OBJ_oItemsGet: PE(p)->yVel = N(0.1); PE(p)->yAcc = N(0.1); pin_setispd(p, (img_t)0.8); PE(p)->alarm[0] = 40; break;
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
        pin_setispd(p, (img_t)0.5);
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
    PE(&PX(g))->xVel = NI(rand_diff(0, 3));
    PE(&PX(g))->yVel = NI(RAND(2, 4) * 1);
}

/* objects/oSolid/Destroy_0.gml */
void destroy_solid(int i)
{
    struct pin *p = &PX(i);
    int obj;
    if (p->shopWall) PUNTR(1010);
    if (collision_point_any_at(i, 8, -1, OBJ_oSpikes)) {
        obj = instance_place_at(i, 8, -1, OBJ_oSpikes);
        if (obj != NOONE) pin_destroy(obj);
    }
    if (collision_point_any_at(i, 8, -1, OBJ_oTikiTorch)) pjungle_world(1011, i, 0);
    if (collision_point_any_at(i, 8, -1, OBJ_oGrave)) pswamp_world(1012, i, 0);
    if (collision_point_any_at(i, 8, 18, OBJ_oLampRed)) PUNTR(1013);
    if (collision_point_any_at(i, 8, 18, OBJ_oLamp)) {
        obj = instance_place_at(i, 8, 16, OBJ_oLamp);
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
                PE(&PX(piece))->xVel = NI(rand_diff(1, 3));
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

/* oSolid's descendants with a Destroy event of their own, or inherited from an ancestor below oSolid (HD 1.2.2's
   objects/<name>/Destroy_0.gml; the Lit traps and oSacAltarRight inherit one) */
static int solid_own_destroy(int o)
{
    switch (o) {
    case OBJ_oAlienShip: case OBJ_oAlienShipFloor: case OBJ_oAltarLeft: case OBJ_oAltarRight:
    case OBJ_oArrowRepeaterL: case OBJ_oArrowRepeaterR: case OBJ_oArrowTrapLeft: case OBJ_oArrowTrapRight:
    case OBJ_oBlock: case OBJ_oBrick: case OBJ_oBrickSmooth: case OBJ_oCeilingTrap: case OBJ_oDark: case OBJ_oDoor:
    case OBJ_oGrave: case OBJ_oIce: case OBJ_oIceBlock: case OBJ_oLush: case OBJ_oPushBlock: case OBJ_oSacAltarLeft:
    case OBJ_oSign: case OBJ_oSmashTrap: case OBJ_oSmashTrapLit: case OBJ_oSpearTrapBottom: case OBJ_oSpearTrapTop:
    case OBJ_oTemple: case OBJ_oTrapBlock: case OBJ_oTree: case OBJ_oXocBlock: case OBJ_oArrowTrapLeftLit:
    case OBJ_oArrowTrapRightLit: case OBJ_oSacAltarRight: case OBJ_oSpearTrapLit:
        return 1;
    }
    return 0;
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
                /* the chain resolves to oSolid's own Destroy unless the object or an ancestor below oSolid has one
                   (refs/hd/src/objects/<o>/Destroy_0.gml); oOlmec / oMovingSolid ... run oSolid's */
                destroy_solid(i);
                if (solid_own_destroy(o)) PUNTR(1020);
            }
        } else if ((pobj[o].ev & EV_DESTROY) && !pcontent_ev(FEV_DESTROY, i, 0))           /* P7 hook */
            PUNTR(1021);
        break;
    }
}

/* ---- Step events ---------------------------------------------------------------------------------------- */
/* Resting objects (docs/PERF2.md A; the grid build only: -DPCOL_EXACT runs every Step in full, as the R-tree's
 * history depends on when the dirty list is flushed). The terrain part of an item's, a treasure's or a jar's Step
 * (isCollision*, moveTo, the velocity and position updates: rest_phys below) is a function of
 *   - the instance's x, y, xVel, yVel, myGrav, colLeft / Right / Bot / Top, stuck, status (compared by their bits),
 *   - its object and collision offsets (fixed for the instance),
 *   - and the oSolid-family entries of the solid grid in the cells around its box (the queries reach at most 2 px
 *     outside the box: x, y move at most 1 px in it; the grid's cells summary changes when such an entry is put in
 *     or taken out: pworld.c gver),
 * with xVel = yVel = +0 (moveTo then moves nothing: no play_time dependence). When a full run of it ended in the
 * state it started from (a fixed point) and none of those inputs changed since, the next run does the same: the
 * same query answers and the same writes, so it ends in the same state. The skip replays its observable effects:
 * the setters' change marks (one pw_changed when the run made any: the draw mark, the box cache, the collision
 * entry's dirty / test-list marks; the grid build's searches do not depend on when an entry is flushed) and the
 * outcome flags the caller reads (destroy, the branch taken). The checks: the grid host build with and without the
 * skip (-DPLAY_NOREST) gives the same records on every route. */
#if PLAY_SKIPS                                                 /* play.h */
#define PLAY_REST 1
#ifndef REST_MAX
#define REST_MAX 128             /* rst[] entries (a power of 2): instance p's is rst[p->ext & (REST_MAX - 1)] */
#endif
struct rest_st { float x, y; double xv, yv, mg; uint8_t cl, cr, cb, ct, stuck; int16_t status; };
/* a fixed point (rest_end): xVel = yVel = +0 there, so only the other fields are kept; myGrav by its bits. Two
   instances whose records share an entry take it in turn (the id tells them apart: the other one's full run) */
struct rest { int32_t id; uint32_t clk, mgh, mgl; float x, y; int16_t status; uint8_t cl, cr, cb, ct, stuck, ok, chg, out; };
static struct rest rst[REST_MAX];
static struct rest_st rest_s0;

static void rest_get(const struct pin *p, struct rest_st *o)
{
    const struct pin_ext *e = PE(p);
    o->x = p->x; o->y = p->y; o->xv = e->xVel; o->yv = e->yVel; o->mg = e->myGrav;
    o->cl = e->colLeft; o->cr = e->colRight; o->cb = e->colBot; o->ct = e->colTop; o->stuck = e->stuck;
    o->status = e->status;
}

/* the region of the terrain part's queries: the box +- 3 px, x, y included (the items' collision_point) */
static int rest_region(const struct pin *p, int32_t *b)
{
    const struct pin_ext *e = PE(p);
    int32_t x, y;
    if (!pin_xy_int_p(p, &x, &y) && (!fwhole(p->x, &x) || !fwhole(p->y, &y))) return 0;   /* (whole beyond +-30000: fwhole) */
    b[0] = x + (e->lbo < 0 ? e->lbo : 0) - 3; b[1] = y + (e->tbo < 0 ? e->tbo : 0) - 3;
    b[2] = x + (e->rbo > 0 ? e->rbo : 0) + 3; b[3] = y + (e->bbo > 0 ? e->bbo : 0) + 3;
    return 1;
}

static uint32_t fb(float f) { union { float f; uint32_t u; } v; v.f = f; return v.u; }
static uint64_t db(double d) { union { double d; uint64_t u; } v; v.d = d; return v.u; }
static int rest_ne(const struct rest_st *a, const struct rest_st *b)   /* by the bits */
{
    return fb(a->x) != fb(b->x) || fb(a->y) != fb(b->y) || db(a->xv) != db(b->xv) || db(a->yv) != db(b->yv) ||
           db(a->mg) != db(b->mg) || a->cl != b->cl || a->cr != b->cr || a->cb != b->cb || a->ct != b->ct ||
           a->stuck != b->stuck || a->status != b->status;
}

static int dbits0(double d) { union { double d; uint64_t u; } v; v.d = d; return v.u == 0; }

/* c is the fixed point r keeps (by the bits) */
static int rest_is(const struct rest_st *c, const struct rest *r)
{
    uint64_t m = db(c->mg);
    return fb(c->x) == fb(r->x) && fb(c->y) == fb(r->y) && dbits0(c->xv) && dbits0(c->yv) &&
           (uint32_t)(m >> 32) == r->mgh && (uint32_t)m == r->mgl && c->cl == r->cl && c->cr == r->cr &&
           c->cb == r->cb && c->ct == r->ct && c->stuck == r->stuck && c->status == r->status;
}

/* 1: the terrain part is skipped (*out: the outcome its last full run left); 0: run it between rest_begin and
   rest_end */
static int rest_skip(int i, uint8_t *out)
{
    struct pin *p = &PX(i);
    struct rest *r = &rst[p->ext & (REST_MAX - 1)];
    struct rest_st c;
    int32_t b[4];
    if (!r->ok || r->id != p->id || pcol_quiet()) return 0;
    rest_get(p, &c);
    if (!rest_is(&c, r) || !rest_region(p, b) || !pw_rest_still(b[0], b[1], b[2], b[3], r->clk))
        return 0;
    if (r->chg) pw_replayed(i);                       /* (the run's marks; no field changes here) */
    *out = r->out;
    return 1;
}

static void rest_begin(int i)
{
    rest_get(&PX(i), &rest_s0);
    pw_watch(i);
}

static void rest_end(int i, uint8_t out)
{
    struct pin *p = &PX(i);
    struct rest *r = &rst[p->ext & (REST_MAX - 1)];
    struct rest_st c;
    int32_t b[4];
    uint32_t n = pw_watch_end();
    uint64_t m;
    if (r->id == p->id) r->ok = 0;                    /* another instance's record stays */
    if (!p->alive || !p->ext || p->ext == EXT_SCRATCH) return;   /* the shared records: no fixed point */
    rest_get(p, &c);
    if (rest_ne(&c, &rest_s0) || !dbits0(c.xv) || !dbits0(c.yv) || !rest_region(p, b)) return;
    r->ok = 1;
    r->id = p->id;
    m = db(c.mg); r->mgh = (uint32_t)(m >> 32); r->mgl = (uint32_t)m;
    r->x = c.x; r->y = c.y; r->status = c.status;
    r->cl = c.cl; r->cr = c.cr; r->cb = c.cb; r->ct = c.ct; r->stuck = c.stuck;
    r->chg = n != 0;
    r->out = out;
    r->clk = pw_rest_clock();
}

/* gameStepEvent's oMoveableSolid fall (objects/oGame, scripts/gameStepEvent :248-262) for a solid at rest: with
   yVel +0 it adds myGrav (capped at 8), and the first place_meeting(x, y + 1, oSolid) hits, so yVel goes back to
   +0, y stays and no sound plays (yVel > myGrav is false). That run is a fixed point; the next one gives the same
   answer while the solid's x, y, sprite, image_index and myGrav keep their bits and no oSolid-family entry (the
   solid itself included: oMoveableSolid is one) went in or out of the cells around it (pw_rest_still, as
   rest_skip). The skip leaves yVel +0 and counts the run's NOPS */
struct mrest { int32_t id; uint32_t clk, mgh, mgl, x, y, img; int16_t spr; uint8_t ok; };
static struct mrest mrst[REST_MAX];

static int msolid_region(const struct pin *p, int32_t *b)
{
    if (!rest_region(p, b)) return 0;
    b[3] += 1;                                        /* the query's box is 1 px lower */
    return 1;
}

static int msolid_skip(int j)
{
    const struct pin *p = &PX(j);
    const struct mrest *r = &mrst[p->ext & (REST_MAX - 1)];
    uint64_t m = db(PE(p)->myGrav);
    int32_t b[4];
    return r->ok && r->id == p->id && !pcol_quiet() && dbits0(PE(p)->yVel) && fb(p->x) == r->x &&
           fb(p->y) == r->y && fb((float)p->img) == r->img && p->spr == r->spr && (uint32_t)(m >> 32) == r->mgh &&
           (uint32_t)m == r->mgl && msolid_region(p, b) && pw_rest_still(b[0], b[1], b[2], b[3], r->clk);
}

static int msolid_start(int j) { return dbits0(PE(&PX(j))->yVel); }

/* after a full run: rest 1 when it started at yVel +0, hit on the first query and left y and yVel +0 */
static void msolid_end(int j, int rest)
{
    const struct pin *p = &PX(j);
    struct mrest *r = &mrst[p->ext & (REST_MAX - 1)];
    uint64_t m;
    int32_t b[4];
    if (r->id == p->id) r->ok = 0;
    if (!rest || !p->alive || !p->ext || p->ext == EXT_SCRATCH || !msolid_region(p, b)) return;
    m = db(PE(p)->myGrav);
    r->ok = 1;
    r->id = p->id;
    r->x = fb(p->x); r->y = fb(p->y); r->img = fb((float)p->img); r->spr = p->spr;
    r->mgh = (uint32_t)(m >> 32); r->mgl = (uint32_t)m;
    r->clk = pw_rest_clock();
}
#else
#define PLAY_REST 0
static int rest_skip(int i, uint8_t *out) { (void)i; (void)out; return 0; }
static void rest_begin(int i) { (void)i; }
static void rest_end(int i, uint8_t out) { (void)i; (void)out; }
static int msolid_skip(int j) { (void)j; return 0; }
static int msolid_start(int j) { (void)j; return 0; }
static void msolid_end(int j, int rest) { (void)j; (void)rest; }
#endif

/* objects/oItem/Step_0.gml */
void item_step(int i)
{
    struct pin *p = &PX(i);
    uint8_t br = 0;
    int rest = 0;
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
    } else if (p->type != T_BOMB && p->type != T_ARROW && rest_skip(i, &br)) {
        /* the terrain part as its last full run (rest_skip); the lava tests below still run */
    } else if (rest = p->type != T_BOMB && p->type != T_ARROW, rest ? rest_begin(i) : (void)0,   /* (comma: only on */
               !collision_point_any_at(i, 0, 0, OBJ_oSolid)) {   /* :69   reaching here) */
        br = 1;
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
#if PLAY_REST
                /* the grid build: the test at y - 1 on the field alone, then the final y and one mark. The query
                   reads the position fields (ibounds / calcBounds) and searches oSolid without the item, and the
                   grid build's answers do not depend on when the item's entry is flushed; the setters' marks
                   (draw, box cache, dirty / test list fronts; rest_end reads only whether one happened) are those
                   one pin_changed_ leaves: the flush between the two marks does not touch the test list
                   (cupdate_at), and the stale stack is read in creation order. The final y is the setters' */
                if (!obj_is(p->obj, OBJ_oSolid)) {
                    pos y1 = p->y - (PI(1)), y2 = y1 + (PI(1));
                    PIN_SETY_RAW(p, y1);
                    if (!isCollisionBottom(i, 1)) PIN_SETY_RAW(p, y2);
                    pin_changed_(p);
                } else
#endif
                {
                    pin_sety(p, p->y - (PI(1)));
                    if (!isCollisionBottom(i, 1)) pin_sety(p, p->y + (PI(1)));
                }
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
        if (rest) rest_end(i, 1);
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
        if (rest) rest_end(i, 0);
    }
    if (br) {                                                                  /* :171-185 (branch :69) */
        p = &PX(i);
        if (collision_rect_any_at(i, -3, -3, 3, 3, OBJ_oLava))
            ptemple_world(1032, i, 0);
        else
            PE(p)->myGrav = N(0.6);
        if (collision_point_any_at(i, 0, -5, OBJ_oLava) && p->type != T_SCEPTRE)
            ptemple_world(1032, i, 0);
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

/* objects/oJar/Step_0.gml and objects/oSkull/Step_0.gml (no inherit). The compares against constants on the bits
   (pcmpc.h: the same answers as NLT / NGT, no soft-float call) */
static void jar_step(int i, int skull)
{
    struct pin *p = &PX(i);
    int destroy = 0;
    uint8_t out;
    if (!PE(p)->held && rest_skip(i, &out)) {       /* the terrain part as its last full run (rest_skip) */
        destroy = out;
        pin_setdepth(p, 100);
        goto lava;
    }
    if (!PE(p)->held) rest_begin(i);
    PE(p)->colTop = PE(p)->colLeft = PE(p)->colRight = PE(p)->colBot = 0;
    if (PE(p)->held) {
        struct pin *pl = &PX(PL.idx);
        if (PL.facing == LEFT) pin_setx(p, pl->x - PI(4));
        else if (PL.facing == RIGHT) pin_setx(p, pl->x + PI(4));
        if (PL.state == DUCKING && CLT(NABS(PE(pl)->xVel), 2, CMPC_L_2)) pin_sety(p, pl->y + PI(4));
        else pin_sety(p, pl->y);
        pin_setdepth(p, 1);
    } else {
        moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
        if (CLT(PE(p)->yVel, 6, CMPC_L_6)) PE(p)->yVel += PE(p)->myGrav;
        if (isCollisionTop(i, 1)) PE(p)->colTop = 1;
        if (isCollisionLeft(i, 1)) PE(p)->colLeft = 1;
        if (isCollisionRight(i, 1)) PE(p)->colRight = 1;
        if (isCollisionBottom(i, 1)) PE(p)->colBot = 1;
        if (PE(p)->colTop && NLT(PE(p)->yVel, N(0))) {
            if (skull ? CLT(PE(p)->yVel, 2, CMPC_L_2) : CLT(PE(p)->yVel, -3, CMPC_L_M3)) destroy = 1;
            PE(p)->yVel = NMUL(-PE(p)->yVel, N(0.8));
        }
        if (PE(p)->colLeft || PE(p)->colRight) {
            if (NGT_COLD(NABS(PE(p)->xVel), skull ? N(2) : N(3))) destroy = 1;
            PE(p)->xVel = NMUL(-PE(p)->xVel, N(0.5));
        }
        if (!skull && collision_point_any_at(i, 0, 0, OBJ_oSolid)) destroy = 1;
        if (PE(p)->colBot) {
            if (CGT(PE(p)->yVel, 3, CMPC_H_3)) destroy = 1;
            if (CGT(PE(p)->yVel, 1, CMPC_H_1)) PE(p)->yVel = NMUL(-PE(p)->yVel, N(0.5));
            else PE(p)->yVel = 0;
            if (CLT(NABS(PE(p)->xVel), 0.1, CMPC_L_0_1)) PE(p)->xVel = 0;
            else if (NNE(NABS(PE(p)->xVel), N(0))) PE(p)->xVel = NMUL(PE(p)->xVel, N(0.3));
        }
        if (PE(p)->colLeft) {
            if (!PE(p)->colRight) pin_setx(p, p->x + (PI(1)));
            PE(p)->yVel = 0;
        } else if (PE(p)->colRight) {
            pin_setx(p, p->x - (PI(1)));
            PE(p)->yVel = 0;
        }
        if (isCollisionBottom(i, 0) && CLT(NABS(PE(p)->yVel), 1, CMPC_L_1)) {
            pin_sety(p, p->y - (PI(1)));
            PE(p)->yVel = 0;
        }
        rest_end(i, (uint8_t)destroy);
        pin_setdepth(p, 100);
    lava:
        if (collision_rect_any_at(i, -3, -3, 3, 3, OBJ_oLava) ||
            collision_point_any_at(i, 0, -5, OBJ_oLava))
            ptemple_world(1036, i, skull);
        NOPS(12);
    }
    if (pen_jar_hit(i, skull)) destroy = 1;                                    /* P5 hook (:104) */
    p = &PX(i);
    if (pdam_jar_hit(i)) destroy = 1;                                          /* P5 hook (:148) */
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
    uint8_t out;
    if (!(inview(i, 16) && PE(p)->state == 1))
        return;
    if (rest_skip(i, &out)) goto terrain_done;
    rest_begin(i);
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
    rest_end(i, 0);
terrain_done:
    if (G.hasSpectacles || PG.hasUdjatEye) pin_setdepth(p, 0);
    else pin_setdepth(p, 101);
    NOPS(8);
    if (collision_rect_any_at(i, -3, -3, 3, 3, OBJ_oLava) ||
        collision_point_any_at(i, 0, -5, OBJ_oLava))
        ptemple_world(1039, i, 0);
}

/* PLTI(v, a) || PGTI(v, b) (a < b) for a coordinate v of an instance and s its pin_xy_int shadow (play.h): a whole v
   is s (> -30000), and gcmp_fi(v, c) is then the sign of s - c (an integer: 0 or at least 1 away). Another v within
   pfloor_int's range by its floor f: f <= a - 2 gives v < a - 1, f >= b + 1 gives v > b + eps, a <= f < b gives a <= v
   < b (neither); only v in [a - 1, a) or [b, b + 1), where the compare's eps decides, takes gcmp_fi */
#ifdef PIN_SHADOW_CHECK
#include <stdio.h>
#include <stdlib.h>
#endif
static inline int pout_ab(pos v, int32_t s, int32_t a, int32_t b)
{
    int32_t f;
    if (s > -30000) {
#ifdef PIN_SHADOW_CHECK
        if (!pos_int(v, &f) || f != s) {
            fprintf(stderr, "pout_ab: shadow %d differs from %.9g\n", (int)s, (double)v);
            abort();
        }
#endif
        return s < a || s > b;
    }
    if (pfloor_int(v, &f)) {
        if (f <= a - 2 || f > b) return 1;
        if (f >= a && f < b) return 0;
    }
    return PLTI(v, a) || PGTI(v, b);
}

/* objects/oDetritus/Step_0.gml; returns its isCollisionBottom(i, 1) answer when bounce asked it, else -1 (pint.h
   detritus_bottom) */
int detritus_step(int i)
{
    struct pin *p = &PX(i);
    pos x = p->x, y = p->y;
    int bot = -1;
    view_read();
    if (pout_ab(x, p->ix, PW.xview - 4, PW.xview + 320 + 4) || pout_ab(y, p->iy, PW.yview - 4, PW.yview + 240 + 4))
        pin_destroy(i);
    if (NGT(PE(p)->life, N(0))) PE(p)->life -= N(1);
    else pin_destroy(i);
    moveTo_walk(i, PE(p)->xVel, PE(p)->yVel);
    if (collision_point_any_at(i, 0, -4, OBJ_oLava)) ptemple_world(1040, i, 0);
    if (PE(p)->bounce) {
        if (CLT(PE(p)->yVel, 6, CMPC_L_6)) PE(p)->yVel += PE(p)->grav;
#if !defined(PCOL_EXACT) && !defined(NUM_IS_CLASS)
        /* yVel < 0 first: the query writes nothing the compare reads, and skipping it skips only flushes, which the
           grid build's searches do not depend on (the count build keeps the order: its compare count) */
        if (NLT(PE(p)->yVel, N(0)) && isCollisionTop(i, 1)) PE(p)->yVel = NMUL(-PE(p)->yVel, N(0.8));
#else
        if (isCollisionTop(i, 1) && NLT(PE(p)->yVel, N(0))) PE(p)->yVel = NMUL(-PE(p)->yVel, N(0.8));
#endif
        if (isCollisionLeft(i, 1) || isCollisionRight(i, 1)) PE(p)->xVel = NMUL(-PE(p)->xVel, N(0.5));
        if ((bot = isCollisionBottom(i, 1)) != 0) {
            if (CGT(PE(p)->yVel, 1, CMPC_H_1)) PE(p)->yVel = NMUL(-PE(p)->yVel, N(0.5));
            else PE(p)->yVel = 0;
        }
        NOPS(6);
    }
    return bot;
}

/* a normal float f (not zero, subnormal, infinite or NaN): (float)((double)f + d) for d = +-0 is f, the same bits */
static inline int fnormal(float f)
{
    union { float f; uint32_t u; } v;
    uint32_t e;
    v.f = f;
    e = (v.u >> 23) & 0xffu;
    return e != 0 && e != 0xffu;
}

/* objects/oRubblePiece/Step_0.gml: oRubble, oRubbleSmall (nops 3), oDrip, oRubbleDarkSmall, oLavaDrip (0) */
__attribute__((noinline)) void rubblepiece_step(int i, int nops)
{
    struct pin *p = &PX(i);
    pos px, py;
#if !defined(NUM_IS_CLASS)
    /* x += xVel with xVel +-0 (the drips, the rubble): x + 0 is x for a normal x, so the setter would store the same
       bits and mark nothing */
    if (!dzero(PE(p)->xVel) || !fnormal(p->x))
#endif
        pin_setx(p, PADDV(p->x, PE(p)->xVel));
    pin_sety(p, (pos)(TOD(p->y) + NTOD(PE(p)->yVel)));                /* PADDV, y widened by fwiden */
    PE(p)->yVel += PE(p)->yAcc;
    NOPS(nops);
    px = p->x;
    py = p->y;
#ifndef PCOL_EXACT
    {
        /* the three tests at once (pworld.c), the oSolid one before site 1041 / the lava's pin_destroy: these write
           no position and no oSolid-family entry (1041 sets yVel or destroys the drip), and the grid build's
           searches do not depend on when an entry is flushed */
        int t = pw_piece_fast(i);                                       /* (its cells' answer, else -1) */
        if (t < 0) t = pw_piece_tests(i);
        if (t & 1) pswamp_world(1041, i, 0);
        else if (t & 2) pin_destroy(i);
        if (t & 4) pin_destroy(i);
    }
#else
    if (collision_point_any_at(i, 0, 0, OBJ_oWaterSwim)) pswamp_world(1041, i, 0);
    else if (collision_point_any_at(i, 0, 0, OBJ_oLava)) pin_destroy(i);
    if (collision_point_any_at(i, 0, 0, OBJ_oSolid)) pin_destroy(i);   /* (x, y: px, py; site 1041 moves nothing) */
#endif
    view_read();
    if (pout_ab(px, p->ix, PW.xview - 32, PW.xview + 320 + 32) || pout_ab(py, p->iy, PW.yview - 32, PW.yview + 240 + 32))
        pin_destroy(i);                                                 /* (ix, iy: the shadows of px, py) */
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
            if (!collision_point_any_at(i, -8, 0, OBJ_oSolid)) pin_setx(p, p->x - (PI(8)));
            else pin_setx(p, p->x + (PI(8)));
        } else {
            if (!collision_point_any_at(i, 8, 0, OBJ_oSolid)) pin_setx(p, p->x + (PI(8)));
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
            if (!PE(p)->held && collision_point_any_at(i, 0, 4, OBJ_oBrickSmooth) &&
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
            pos x, y;
            if (!p->alive) continue;
            x = p->x;
            y = p->y;
            view_read();
            if (PGTI(x, PW.xview - 16) && PLTI(x, PW.xview + 320) && PGTI(y, PW.yview - 16) && PLTI(y, PW.yview + 240)) {
                pos yMPrev = p->y;
                int rest0, first = 1, rest = 0;
                if (msolid_skip(j)) { NOPS(2); continue; }
                rest0 = msolid_start(j);
                PE(p)->yVel += PE(p)->myGrav;
                if (NGT(PE(p)->yVel, N(8))) PE(p)->yVel = N(8);
                NOPS(2);
                for (; DLT(PTOD(p->y), PTOD(yMPrev) + NTOD(PE(p)->yVel)); pin_sety(p, p->y + (PI(1)))) {
                    if (place_meeting_p(j, PTOD(p->x), PTOD(p->y) + 1, OBJ_oSolid)) {
                        if (NGT(PE(p)->yVel, PE(p)->myGrav)) snd_play(SND_xthud);     /* :258 */
                        PE(p)->yVel = 0;
                        rest = rest0 && first;
                        break;
                    }
                    first = 0;
                }
                msolid_end(j, rest);
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

/* objects/oLevel/Step_0.gml :110-136: darkness (0 lightest, 1 darkest; drawn as the dark level's palette fade,
   src/draw). global.darknessLerp is never set above 0 (oGlobals / oLevel Create only) */
static void level_darkness(void)
{
    double dist = 160;
    if (!G.darkLevel || PL.idx == NOONE) return;
    if (PG.hasCrown) dist = 0;
    else if (instance_exists_p(OBJ_oFlare))
        dist = FLARE_DIST(&PX(instance_nearest_p(PTOD(PX(PL.idx).x), PTOD(PX(PL.idx).y), OBJ_oFlare)));
    if (PL.distToNearestLightSource < 200 && PL.distToNearestLightSource < dist) dist = PL.distToNearestLightSource;
    PLEV.darkness = dist == 0 ? 0 : dist / 160;
    if (PLEV.darkness > 0.9) PLEV.darkness = 0.9;
}

/* objects/oLevel/Step_0.gml: the screen shake, the darkness (the rest: water drawing flags and activation, no play
   state) */
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
    level_darkness();                                                          /* :110 */
}

/* The Step's claimant per object (docs/PERF2.md D). Which of the P5 hooks (pen_step, pdam_step, pshop_step,
 * pitem_step, tried in that order) runs an instance's Step, or none (ev_step's own switch), depends on its object
 * only: each hook returns 1 or 0 by a switch on the object or obj_is, and returns 0 without any effect. So the
 * first Step of an object tries the hooks in order and keeps the claimant; later ones call it directly. The same
 * holds one level down for treasure (ptrans_step's switch, then obj_is(oTreasure)). -DPLAY_DCHECK (host check):
 * every Step tries the chain and aborts if the claimant differs from the kept one. */
/* SK_PKG + k (k 1-5): ev_step's own path for the object is ptrans_step's 0, then the oTreasure / oItem tests, then
   pcontent_ev's Step with claimant k (all decided by the object alone): ev_step calls package k's ev directly */
enum { SK_NONE, SK_PEN, SK_PDAM, SK_PSHOP, SK_PITEM, SK_OWN, SK_TREASURE, SK_PKG };
/* SK_RUBBLE: an SK_PKG object whose package Step is rubblepiece_step(i, 0) and nothing else (oDrip: pswamp_ev,
   oRubbleDarkSmall: pice_ev, oLavaDrip: ptemple_ev; each switches on the object first), with no off-view test */
#define SK_RUBBLE (SK_PKG + 6)
/* SK_RUBBLE3: oRubble / oRubbleSmall once their Step is known to be ev_step's own (step_hooks SK_OWN): its switch
   runs rubblepiece_step(i, 3) and nothing else. SK_RUBBLE and SK_RUBBLE3 are the largest kinds (ev_step_run) */
#define SK_RUBBLE3 (SK_PKG + 7)
static uint8_t stepk[OBJ_COUNT];

static int step_hooks(int i)
{
    if (pen_step(i)) return SK_PEN;
    if (pdam_step(i)) return SK_PDAM;
    if (pshop_step(i)) return SK_PSHOP;
    if (pitem_step(i)) return SK_PITEM;
    return SK_OWN;
}

#ifdef PLAY_DCHECK
#include <stdio.h>
#include <stdlib.h>
static void stepk_check(int o, int k)
{
    if (stepk[o] != SK_NONE && stepk[o] != k) {
        fprintf(stderr, "PLAY_DCHECK: object %d Step claimant %d, kept %d\n", o, k, stepk[o]);
        abort();
    }
    stepk[o] = (uint8_t)k;
}
#endif

/* 1 when ev_step(i) would do nothing (front_on 0): an object whose Step is treasure_step (stepk SK_TREASURE, kept
   from its first Step) out of view, treasure_step's first test (inview reads the view and the instance only) */
/* 1 when ev_step(i) of an instance of o is exactly pkg_ev(FEV_STEP, i, 0): front_on 0, the Step kept as package
   pkg_ev's direct call (SK_PKG + k), and no off-view test before it (pen_offview_obj 0). PLAY_DCHECK: 0 */
int ev_step_is_pkg(int o, int (*pkg_ev)(int ev, int i, int arg))
{
#ifdef PLAY_DCHECK
    (void)o; (void)pkg_ev;
    return 0;
#else
    return !front_on && stepk[o] > SK_PKG && stepk[o] <= SK_PKG + 5 && !pen_offview_obj[o] &&
           pcontent_pkg_ev[stepk[o] - SK_PKG] == pkg_ev;
#endif
}

/* prun.c's Step loop with front_on 0, at ord[0] (alive, play_cur_obj set; n >= 1 entries left): what ev_step(i);
   pcol_event_done(i) would do for the instances it can take, and how many it took (0: none, the caller runs
   ev_step). A leaf for every other object (the two cases out of line):
   - a treasure out of view (stepk SK_TREASURE: treasure_step's first test, inview, reads the view and the instance
     only): pcol_event_done alone, 1;
   - a debris piece (stepk SK_RUBBLE: oDrip, oRubbleDarkSmall, oLavaDrip, ev_step's rubblepiece_step(i, 0);
     SK_RUBBLE3: oRubble / oRubbleSmall, rubblepiece_step(i, 3)): it and the pieces that follow it in ord, each as the
     loop would take it (play_cur_obj, rubblepiece_step, pcol_event_done); the run ends at the first entry that is
     not an alive piece (a dead one the loop skips, any other object the loop's own tests), so every entry is taken
     as the loop would take it. The loop body's tests before ev_step (oPiranha, oGamepad, the jungle idle objects)
     are of other objects */
static __attribute__((noinline)) int treasure_idle(int i)
{
    if (inview(i, 16)) return 0;
    pcol_event_done(i);
    return 1;
}

static __attribute__((noinline)) int piece_run(const int16_t *ord, int n, int k)
{
    int i = ord[0], j;
    for (j = 0;;) {
        play_cur_obj = PX(i).obj;
        rubblepiece_step(i, k == SK_RUBBLE ? 0 : 3);
        pcol_event_done(i);
        if (++j == n) break;
        i = ord[j];
        if (!PX(i).alive || (k = stepk[PX(i).obj]) < SK_RUBBLE) break;
    }
    return j;
}

int ev_step_run(const int16_t *ord, int n)
{
    int k = stepk[PX(ord[0]).obj];
    if (k >= SK_RUBBLE) return piece_run(ord, n, k);
    if (k == SK_TREASURE) return treasure_idle(ord[0]);
    return 0;
}

/* a package ran the object's Step at the end of ev_step's own path (whose every test depends on the object only):
   later Steps call the package directly (SK_PKG). PLAY_DCHECK keeps the whole path */
static void step_pkg(int o)
{
#ifndef PLAY_DCHECK
    int k = pcontent_step_claimant(o);
    if (k >= 1 && k <= 5)
        stepk[o] = (uint8_t)((o == OBJ_oDrip || o == OBJ_oRubbleDarkSmall || o == OBJ_oLavaDrip) && !pen_offview_obj[o] ?
                             SK_RUBBLE : SK_PKG + k);
#else
    (void)o;
#endif
}

void ev_step(int i)
{
    if (front_on && front_ev(FEV_STEP, i, 0)) return;                                 /* P8 hook */
    struct pin *p = &PX(i);
#ifdef PLAY_DCHECK
    {
        int k = step_hooks(i);
        if (k != SK_OWN) { stepk_check(p->obj, k); return; }
        if (stepk[p->obj] != SK_NONE && stepk[p->obj] < SK_OWN) stepk_check(p->obj, SK_OWN);   /* aborts */
    }
#else
    switch (stepk[p->obj]) {                                                   /* P5 hook */
    case SK_PEN: {
        int o = p->obj;
        if (pen_offview_obj[o] && pen_offview(i)) return;      /* (penemy.c) */
        if (pen_step(i) == 2) step_pkg(o);       /* pen_step's path for o ends in pcontent_ev: call the package */
        return;
    }
    case SK_PDAM: pdam_step(i); return;
    case SK_PSHOP: pshop_step(i); return;
    case SK_PITEM: pitem_step(i); return;
    case SK_TREASURE: treasure_step(i); return;
    case SK_RUBBLE: rubblepiece_step(i, 0); return;                        /* (the package's Step, directly) */
    case SK_RUBBLE3: rubblepiece_step(i, 3); return;                       /* (the switch's case below) */
    case SK_OWN: break;
    case SK_NONE:
        if ((stepk[p->obj] = (uint8_t)step_hooks(i)) != SK_OWN) return;
        if (p->obj == OBJ_oRubble || p->obj == OBJ_oRubbleSmall) stepk[p->obj] = SK_RUBBLE3;
        break;
    default:                                                                   /* SK_PKG + 1-5 */
        if (pen_offview_obj[p->obj] && pen_offview(i)) return;
        pcontent_pkg_ev[stepk[p->obj] - SK_PKG](FEV_STEP, i, 0);
        return;
    }
#endif
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
        if (PE(&PX(i))->held) pin_set_sprite(i, PL.facing == LEFT ? GSPR_sKeyLeft : GSPR_sKeyRight);
        break;
    case OBJ_oWhip: whip_step(i, 0); break;
    case OBJ_oWhipPre: whip_step(i, 1); break;
    case OBJ_oBlood:                                                           /* oBlood Step: inherited first */
    case OBJ_oFlame: {
        int b = detritus_step(i);
        p = &PX(i);
        if (CGT(PE(p)->yVel, 6, CMPC_H_6)) pin_destroy(i);
        if (detritus_bottom(i, b)) {
            if (CGT(PE(p)->life, 20, CMPC_H_20)) PE(p)->life = N(20);
        }
        break;
    }
    case OBJ_oPoof:
        pin_setx(p, PADDV(p->x, PE(p)->xVel));
        pin_sety(p, PADDV(p->y, PE(p)->yVel));
        break;
    case OBJ_oSmokePuff: pin_sety(p, PSUBV(p->y, PE(p)->yVel)); break;
    case OBJ_oBurn:
        pin_sety(p, PADDV(p->y, PE(p)->yVel));
        if (collision_point_any_at(i, 0, 0, OBJ_oSolid)) pin_destroy(i);
        break;
    case OBJ_oItemsGet:
        pin_sety(p, PSUBV(p->y, PE(p)->yVel));
        pin_setx(p, PI(PCEIL(p->x)));
        pin_sety(p, PI(PCEIL(p->y)));
        break;
    case OBJ_oBigCollect: pin_sety(p, p->y - (PI(1))); break;
    case OBJ_oRubble: case OBJ_oRubbleSmall: rubblepiece_step(i, 3); break;
    case OBJ_oPushBlock:                                                       /* inherited: no parent Step */
        if (collision_point_any_at(i, 8, 14, OBJ_oLava) &&
            !collision_point_any_at(i, 8, 17, OBJ_oSolid))
            PUNTR(1055);
        break;
    case OBJ_oWeb:                                                             /* objects/oWeb/Step_0.gml */
        PE(p)->alpha = (float)(NTOD(PE(p)->life) / 12);                   /* image_alpha: a float */
        if (PE(p)->dying) PE(p)->life -= N(0.02);
        if (NLE(PE(p)->life, N(1))) pin_destroy(i);
        break;
    case OBJ_oRope:
        if (collision_point_any_at(i, 12, 0, OBJ_oLava) && PE(p)->burnTimer == 0) ptemple_world(1056, i, 0);
        if (PE(p)->burnTimer > 1) PE(p)->burnTimer -= 1;
        else if (PE(p)->burnTimer == 1) ptemple_world(1056, i, 0);
        break;
    case OBJ_oArrowTrapLeft: case OBJ_oArrowTrapLeftLit: case OBJ_oArrowTrapRight: case OBJ_oArrowTrapRightLit:
        break;                                                                 /* firing = false; the rest commented */
    case OBJ_oBones:                                                           /* objects/oBones/Step_0.gml */
        if (!collision_point_any_at(i, 8, 16, OBJ_oSolid)) {
            pin_sety(p, PADDV(p->y, PE(p)->yVel));
            PE(p)->yVel += PE(p)->yAcc;
        }
        if (collision_point_any_at(i, 8, 15, OBJ_oSolid)) pin_sety(p, p->y - (PI(1)));
        break;
    case OBJ_oGamepad: break;                                                  /* prun.c */
    default:
        if (ptrans_step(i)) {
#ifdef PLAY_DCHECK
            stepk_check(p->obj, SK_OWN);
#endif
            break;
        }
        if (obj_is(p->obj, OBJ_oTreasure)) {
#ifdef PLAY_DCHECK
            stepk_check(p->obj, SK_TREASURE);
#else
            stepk[p->obj] = SK_TREASURE;
#endif
            treasure_step(i);
        }
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
            else if (pcontent_ev(FEV_STEP, i, 0))                             /* P7 hook (calls item_step
                                                                                  itself when it inherits) */
                step_pkg(p->obj);
            else
                item_step(i);
        } else if (pcontent_ev(FEV_STEP, i, 0))                                        /* P7 hook */
            step_pkg(p->obj);
        else
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
            pin_setispd(p, 1);
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
            while (!collision_point_any(xAct, PTOD(p->y) + 8, OBJ_oSolid, 0, NOONE)) {
                if (PFLOOR(p->x) - xAct > 96) break;
                xAct -= 1;
            }
            if (xAct > PFLOOR(p->x) - 16) xAct = PFLOOR(p->x) - 16;
            PE(p)->xAct = (int16_t)xAct;
            obj = pin_create(PI(xAct), p->y, OBJ_oArrowTrapTest);
            pin_setxscale(&PX(obj), dceil(((PFLOOR(PX(i).x) - 1) - xAct) / 16.0));
            PE(&PX(obj))->trapID = (int16_t)i;
            pw_ref(obj);
        }
        break;
    case OBJ_oArrowTrapRight: case OBJ_oArrowTrapRightLit:
        if (a == 0) pitems_world(1071, i, a);
        else if (a == 1 && !isRoomIs(R_rLevelEditor)) {                        /* objects/oArrowTrapRight/Alarm_1.gml */
            int x = PFLOOR(p->x), xAct = x + 16, n = 100, obj;
            while (!collision_point_any(xAct, PTOD(p->y) + 8, OBJ_oSolid, 0, NOONE) && n > 0) {
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
            pw_ref(obj);
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
    pos x = p->x, y = p->y;
    view_read();
    if (isRoomIs(R_rTutorial) || (PGTI(x, PW.xview - 16) && PLTI(x, PW.xview + 320 + 16) && PGTI(y, PW.yview - 16) && PLTI(y, PW.yview + 240 + 16))) {
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
            if (!collision_point_any_at(s, 0, 16, OBJ_oSolid)) pin_destroy(s);
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
        pin_setispd(o, 1);
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
            if (oo == OBJ_oDamsel) {                                   /* objects/oExplosion/Collision_oDamsel.gml */
                struct pin *o = &PX(other);
                if (!o->invincible) {
                    PE(o)->hp -= 100;
                    if (PX(self).x < o->x) PE(o)->xVel = NI(RAND(4, 6));
                    else PE(o)->xVel = NI(-RAND(4, 6));
                    PE(o)->yVel = N(-6);
                    PEN(o)->burning = 50;
                    PE(o)->status = 2;                                     /* pdamsel.c D_THROWN */
                }
            } else explosion_item(self, other);
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
            pin_setispd(&PX(other), 0);
        } else if (obj_is(oo, OBJ_oTreasure)) {
            PE(&PX(other))->xVel = 0;
            PE(&PX(other))->yVel = 0;
        } else if (obj_is(oo, OBJ_oWater) || obj_is(oo, OBJ_oLaser))            /* Collision_oWater (oLava too), */
            pin_destroy(self);                                                 /* Collision_oLaser: instance_destroy() */
        else if (!pcontent_ev(FEV_COLLISION, self, other))                     /* P7 hook (Collision_oSlash) */
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
                PE(&PX(piece))->xVel = NI(rand_diff(1, 3));
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
        } else if (obj_is(oo, OBJ_oBullet))                                    /* objects/oJar/Collision_oBullet.gml: */
            pin_destroy(self);                                                 /* its Destroy breaks it (destroy_jar_like) */
        else
            ptemple_world(1094, self, other);
        break;
    case OBJ_oSkull:                                                           /* objects/oSkull/Collision_oBullet.gml */
        if (obj_is(oo, OBJ_oBullet)) pin_destroy(self);
        else if (!pcontent_ev(FEV_COLLISION, self, other)) PUNTR(1096);
        break;
    case OBJ_oLockedChest: pitems_world(1095, self, other); break;
    default: if (!pcontent_ev(FEV_COLLISION, self, other)) PUNTR(1096); break;    /* P7 hook */
    }
}

/* The Draw's claimant per object, as the Step's (docs/PERF2.md D): pl_draw for oPlayer1, then pen_draw, pdam_draw,
 * pshop_draw (each claims by the object alone and has no effect when it does not), pcontent_ev (no content package
 * has a Draw case: 0, no effect), ptrans_draw (acts on oPDummy only). The first Draw of an object runs the chain
 * and keeps the claimant; later ones run it directly, or nothing for pen_draw's and pshop_draw's objects (they only
 * draw) and ptrans_draw's others. -DPLAY_DCHECK: every Draw runs the chain and aborts on a different claimant */
enum { DK_NONE, DK_PL, DK_PDAM, DK_PCONTENT, DK_PDUMMY, DK_NOTHING };
static uint8_t drawk[OBJ_COUNT];

static int draw_hooks(int i)
{
    if (PX(i).obj == OBJ_oPlayer1) { pl_draw(i); return DK_PL; }
    if (pen_draw(i)) return DK_NOTHING;                                        /* P5 hook */
    if (pdam_draw(i)) return DK_PDAM;
    if (pshop_draw(i)) return DK_NOTHING;
    if (pcontent_ev(FEV_DRAW, i, 0)) return DK_PCONTENT;                       /* P7 hook */
    ptrans_draw(i);
    return PX(i).obj == OBJ_oPDummy ? DK_PDUMMY : DK_NOTHING;
}

void ev_draw(int i)
{
    int o;
    if (front_on && front_ev(FEV_DRAW, i, 0)) return;                                 /* P8 hook */
    o = PX(i).obj;
#ifdef PLAY_DCHECK
    {
        int k = draw_hooks(i);
        if (drawk[o] != DK_NONE && drawk[o] != k) {
            fprintf(stderr, "PLAY_DCHECK: object %d Draw claimant %d, kept %d\n", o, k, drawk[o]);
            abort();
        }
        drawk[o] = (uint8_t)k;
    }
#else
    switch (drawk[o]) {
    case DK_PL: pl_draw(i); return;
    case DK_PDAM: pdam_draw(i); return;
    case DK_PCONTENT: pcontent_ev(FEV_DRAW, i, 0); return;
    case DK_PDUMMY: ptrans_draw(i); return;
    case DK_NOTHING: return;
    }
    drawk[o] = (uint8_t)draw_hooks(i);
#endif
}

void ev_outside(int i)
{
    if (front_on && front_ev(FEV_OUTSIDE, i, 0)) return;                                 /* P8 hook */
    if (PX(i).obj == OBJ_oPushBlock) pin_destroy(i);                           /* objects/oPushBlock/Other_0.gml */
    else if (pen_outside(i)) return;                                           /* P5 hook */
    else if (!pcontent_ev(FEV_OUTSIDE, i, 0)) PUNTR(1097);                     /* P7 hook */
}
