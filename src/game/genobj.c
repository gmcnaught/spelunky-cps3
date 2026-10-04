/* Create and Destroy events of the objects level generation creates or destroys
 * (refs/hd/src/objects/<obj>/Create_0.gml, Destroy_0.gml; line numbers in comments). `action_inherited()` or an
 * absent event runs the parent's event; the chains are written out (create_solid = oSolid's Create, ...).
 * Kept: the variables of struct inst. String variables are implied by the object, except oExit-family `type`
 * (etype) and `treasure`. LEFT / RIGHT are 18 / 19 in oItem, oTreasure and oCharacter, 0 / 1 in oEnemy.
 * GML argument order: a call's arguments are evaluated last to first, so instance_create(x+..rand.., y+..rand..)
 * draws the y expression's numbers first.
 */
#include "gen.h"
#include "rng.h"

#define XVEL_2_5 640                 /* xVel = 2.5, in 1/256 */

static void create_event(int i);
static void destroy_event(int i);

void gen_create_event(int i)
{
    create_event(i);
}

int instance_create(int x, int y, int obj)
{
    int i = inst_add(obj, x, y, W.next_id++);
    create_event(i);
    return i;
}

void instance_destroy(int i)
{
    if (!W.in[i].alive)
        return;
    destroy_event(i);
    inst_destroyed(i);
}

/* ---- parent Create events -------------------------------------------------------------------------------- */
/* oSolid (objects/oSolid/Create_0.gml): invincible = false, shopWall = false, type, treasure = "", cleanDeath */
static void create_solid(struct inst *p)
{
    p->flags &= (uint16_t)~(IF_INVINCIBLE | IF_SHOPWALL | IF_CLEANDEATH);
    p->treasure = TR_NONE;
}

/* oItem (objects/oItem/Create_0.gml) */
static void create_item(struct inst *p)
{
    p->value = 0;
    p->cost = 0;
    p->flags &= (uint16_t)~(IF_FORSALE | IF_INDICEHOUSE | IF_HELD);
    p->flags |= IF_NEW;
    p->depth = G.hasSpectacles ? 51 : 101;                                    /* :38-39 */
}

/* makeActive() (scripts/makeActive): xVel = yVel = xAcc = yAcc = 0 */
static void make_active(struct inst *p)
{
    p->xvel = p->yvel = 0;
}

/* oTreasure (objects/oTreasure/Create_0.gml) */
static void create_treasure(struct inst *p)
{
    p->value = 0;
    p->flags &= (uint16_t)~IF_HELD;
    p->xvel = p->yvel = 0;
}

/* oEnemy (objects/oEnemy/Create_0.gml) */
static void create_enemy(struct inst *p)
{
    p->cost = 0;
    p->flags &= (uint16_t)~(IF_FORSALE | IF_HELD | IF_SWIMMING);
    p->facing = 0;
}

static void create_shop_item(struct inst *p, int32_t cost)
{
    create_item(p);
    make_active(p);
    p->cost = cost;
}

/* the frog family's last line: if (collision_point(x, y, oWater, 0, 0)) swimming = true; */
static void swim_check(struct inst *p)
{
    if (collision_point(p->x, p->y, OBJ_oWater) != INST_NONE) p->flags |= IF_SWIMMING;
}

/* the terrain blocks' gem / item roll (oBrick :9-14, oLush :7-13, oDark :7-12, oTemple :8-14) */
static void block_gems(int i, int a, int b, int c)
{
    const struct inst *p = &W.in[i];
    int x = p->x, y = p->y, o;
    if (x > 1 && x < G.roomW - 16 && y > 1 && y < G.roomH - 16) {               /* isLevel() is true */
        if (RAND(1, a) == 1) instance_create(x + 8, y + 8, OBJ_oSapphireBig);
        else if (RAND(1, b) == 1) instance_create(x + 8, y + 8, OBJ_oEmeraldBig);
        else if (RAND(1, c) == 1) instance_create(x + 8, y + 8, OBJ_oRubyBig);
        else if (RAND(1, 1200) == 1) scrGenerateItem(i, x + 8, y + 8, 2, &o);
    }
}

static void create_event(int i)
{
    struct inst *p = &W.in[i];
    int obj = p->obj, x = p->x, y = p->y, n;
    switch (obj) {
    /* ---- terrain ---- */
    case OBJ_oBrick:
        /* objects/oBrick/Create_0.gml */
        create_solid(p);
        n = RAND(1, 10);                                                           /* :2 */
        if (n == 1) inst_set_sprite(i, GSPR_sBrick2);
        n = RAND(1, 100);                                                          /* :6 */
        if (n < 20) inst_set_sprite(i, GSPR_sBrickGold);
        else if (n < 30) inst_set_sprite(i, GSPR_sBrickGoldBig);
        else block_gems(i, 100, 120, 140);
        break;
    case OBJ_oLush:
        /* objects/oLush/Create_0.gml */
        create_solid(p);
        n = RAND(1, 100);                                                          /* :4 */
        if (n < 20) inst_set_sprite(i, GSPR_sLushGold);
        else if (n < 30) inst_set_sprite(i, GSPR_sLushGoldBig);
        else block_gems(i, 80, 100, 120);
        break;
    case OBJ_oDark:
        /* objects/oDark/Create_0.gml (the room is never rIntro here) */
        create_solid(p);
        n = RAND(1, 100);                                                          /* :2 */
        if (n < 20) inst_set_sprite(i, GSPR_sDarkGold);
        else if (n < 30) inst_set_sprite(i, GSPR_sDarkGoldBig);
        else block_gems(i, 40, 60, 80);
        break;
    case OBJ_oTemple:
        /* objects/oTemple/Create_0.gml */
        create_solid(p);
        n = RAND(1, 100);                                                          /* :4 */
        if (G.cityOfGold) inst_set_sprite(i, GSPR_sGTemple);
        else if (n < 20) inst_set_sprite(i, GSPR_sTempleGold);
        else if (n < 30) inst_set_sprite(i, GSPR_sTempleGoldBig);
        else block_gems(i, 60, 80, 100);
        break;
    case OBJ_oIce:
        /* objects/oIce/Create_0.gml */
        create_solid(p);
        if (RAND(1, 80) == 1) instance_create(x, y, OBJ_oFrozenCaveman);
        break;
    case OBJ_oBlock:
        /* objects/oBlock/Create_0.gml */
        create_solid(p);
        if (G.cityOfGold) inst_set_sprite(i, GSPR_sGoldBlock);
        break;
    case OBJ_oPushBlock:
        /* objects/oPushBlock/Create_0.gml; oMoveableSolid: xVel = yVel = 0 */
        create_solid(p);
        p->xvel = p->yvel = 0;
        if (G.cityOfGold) inst_set_sprite(i, GSPR_sGoldBlock);
        break;
    case OBJ_oHardBlock: case OBJ_oLavaSolid: case OBJ_oMoai: case OBJ_oMoai2: case OBJ_oMoai3: case OBJ_oMoaiInside:
        create_solid(p);
        p->flags |= IF_INVINCIBLE;
        break;
    case OBJ_oGrave:
        /* objects/oGrave/Create_0.gml */
        create_solid(p);
        switch (RAND(1, 5)) {
        case 1: inst_set_sprite(i, GSPR_sGrave2); break;
        case 2: inst_set_sprite(i, GSPR_sGrave3); break;
        case 3: inst_set_sprite(i, GSPR_sGrave4); break;
        case 4: inst_set_sprite(i, GSPR_sGrave5); break;
        case 5: inst_set_sprite(i, GSPR_sGrave6); break;
        }
        break;
    case OBJ_oXocBlock: case OBJ_oTrapBlock: case OBJ_oSolid: case OBJ_oAltarLeft: case OBJ_oAltarRight:
    case OBJ_oSign: case OBJ_oBrickSmooth: case OBJ_oSacAltarLeft: case OBJ_oSacAltarRight: case OBJ_oTree:
    case OBJ_oThinIce: case OBJ_oAlienShip: case OBJ_oAlienShipFloor: case OBJ_oSpearTrapBottom: case OBJ_oSpearTrapTop:
    case OBJ_oSpearTrapLit:
        create_solid(p);                                                           /* and constants */
        break;
    case OBJ_oArrowTrapLeft: case OBJ_oArrowTrapLeftLit: case OBJ_oArrowTrapRight: case OBJ_oArrowTrapRightLit:
        /* objects/oArrowTrapLeft|Right/Create_0.gml (Lit: inherited) */
        create_solid(p);
        p->facing = (obj == OBJ_oArrowTrapLeft || obj == OBJ_oArrowTrapLeftLit) ? 0 : 1;
        p->alarm[1] = 1;
        break;
    case OBJ_oDarkFall:
        create_solid(p);
        make_active(p);
        break;
    case OBJ_oDoor: case OBJ_oThwompTrap:
        create_solid(p);
        make_active(p);
        p->counter = 0;
        p->status = 0;
        break;
    case OBJ_oCeilingTrap:
        create_solid(p);
        make_active(p);
        p->counter = 3;
        p->status = 0;
        break;
    case OBJ_oSmashTrap: case OBJ_oSmashTrapLit:
        /* objects/oSmashTrap/Create_0.gml */
        create_solid(p);
        make_active(p);
        p->status = 0;
        p->counter = 0;
        p->dir = (int16_t)RAND(0, 3);                                              /* :26 */
        break;
    case OBJ_oOlmec:
        /* objects/oOlmec/Create_0.gml */
        create_solid(p);
        make_active(p);
        p->flags |= IF_INVINCIBLE;
        p->facing = (int8_t)RAND(0, 1);                                            /* :19 */
        p->status = -1;
        p->counter = 0;
        break;
    case OBJ_oSpikes:
        p->flags &= (uint16_t)~IF_INVINCIBLE;
        break;
    case OBJ_oLava:
        /* objects/oLava/Create_0.gml */
        p->flags &= (uint16_t)~IF_SPURT;
        p->spurttime = (int16_t)RAND(100, 300);
        p->counter = 0;
        break;
    case OBJ_oKaliHead:
        /* objects/oKaliHead/Create_0.gml */
        switch (RAND(1, 3)) {
        case 1: inst_set_sprite(i, GSPR_sKaliHead1); break;
        case 2: inst_set_sprite(i, GSPR_sKaliHead2); break;
        case 3: inst_set_sprite(i, GSPR_sKaliHead3); break;
        }
        break;
    case OBJ_oIceBottom:
        p->alarm[0] = (int16_t)RAND(20, 400);
        break;
    case OBJ_oBarrierEmitter:
        /* objects/oBarrierEmitter/Create_0.gml */
        if (!instance_exists(OBJ_oBarrier)) instance_create(x, y + 16, OBJ_oBarrier);
        break;
    case OBJ_oSpringTrap:
        p->status = 0;
        p->counter = 0;
        break;
    case OBJ_oLeaves:
        /* objects/oLeaves/Create_0.gml */
        if (G.cemetary) inst_set_sprite(i, GSPR_sLeavesDead);
        if (collision_point(x - 16, y, OBJ_oTree) != INST_NONE || collision_point(x - 16, y, OBJ_oLeaves) != INST_NONE) {
            if (G.cemetary) inst_set_sprite(i, GSPR_sLeavesDeadR);
            else inst_set_sprite(i, GSPR_sLeavesRight);
        }
        break;
    case OBJ_oTreeBranch:
        if (G.cemetary) inst_set_sprite(i, GSPR_sTreeBranchDeadR);
        break;
    case OBJ_oFinalBoss:
        G.olmecDead = 0;
        G.doorOpen = 0;
        break;
    case OBJ_oExit: case OBJ_oEntrance:
        p->etype = EX_EXIT;
        break;
    case OBJ_oXMarket:
        p->etype = EX_MARKET;
        break;
    /* ---- oItem children ---- */
    case OBJ_oChest: case OBJ_oCrate: case OBJ_oLockedChest: case OBJ_oRock: case OBJ_oJar: case OBJ_oSkull:
    case OBJ_oBall: case OBJ_oArrow: case OBJ_oFishBone: case OBJ_oMattockHead: case OBJ_oSceptre:
        create_item(p);
        make_active(p);
        break;
    case OBJ_oFlare:
        create_item(p);
        make_active(p);
        p->flags |= IF_INVINCIBLE;
        p->alarm[0] = 1;
        break;
    case OBJ_oFlareCrate:
        create_item(p);
        make_active(p);
        p->alarm[0] = 1;
        break;
    case OBJ_oKey:
        create_item(p);
        make_active(p);
        p->cost = 0;
        break;
    case OBJ_oGoldIdol: case OBJ_oCrystalSkull:
        create_item(p);
        make_active(p);
        p->value = obj == OBJ_oCrystalSkull ? 15000 : 5000;
        break;
    case OBJ_oLampItem: case OBJ_oLampRedItem:
        create_item(p);
        make_active(p);
        p->value = 1000;
        break;
    case OBJ_oDice:
        /* objects/oDice/Create_0.gml */
        create_item(p);
        make_active(p);
        p->value = RAND(1, 6);                                                     /* :8 */
        break;
    case OBJ_oDamsel:
        /* objects/oDamsel/Create_0.gml */
        create_item(p);
        make_active(p);
        p->flags &= (uint16_t)~(IF_INVINCIBLE | IF_SWIMMING);
        p->cost = (10000 + 5000 * (G.currLevel - 2)) * 3;                         /* getKissValue() * 3 */
        p->status = 0;
        p->facing = 18;                                                            /* LEFT of oItem */
        p->counter = 200;
        break;
    case OBJ_oJetpack: create_shop_item(p, 20000); break;
    case OBJ_oCapePickup: create_shop_item(p, 12000); break;
    case OBJ_oShotgun: create_shop_item(p, 15000); break;
    case OBJ_oTeleporter: create_shop_item(p, 10000); break;
    case OBJ_oGloves: create_shop_item(p, 8000); break;
    case OBJ_oSpectacles: create_shop_item(p, 8000); break;
    case OBJ_oWebCannon: create_shop_item(p, 2000); break;
    case OBJ_oPistol: create_shop_item(p, 5000); break;
    case OBJ_oMitt: create_shop_item(p, 4000); break;
    case OBJ_oPaste: create_shop_item(p, 3000); break;
    case OBJ_oSpringShoes: create_shop_item(p, 5000); break;
    case OBJ_oSpikeShoes: create_shop_item(p, 4000); break;
    case OBJ_oMachete: create_shop_item(p, 7000); break;
    case OBJ_oMattock: create_shop_item(p, 8000); break;
    case OBJ_oBombBox: create_shop_item(p, 10000); break;
    case OBJ_oBow: create_shop_item(p, 1000); break;
    case OBJ_oCompass: create_shop_item(p, 3000); break;
    case OBJ_oParaPickup: create_shop_item(p, 2000); break;
    case OBJ_oRopePile: create_shop_item(p, 2500); break;
    case OBJ_oBombBag: create_shop_item(p, 2500); break;
    case OBJ_oAnkh: create_shop_item(p, 50000); break;
    case OBJ_oCrown: create_shop_item(p, 999999); break;
    /* ---- oTreasure children ---- */
    case OBJ_oRubyBig: create_treasure(p); make_active(p); p->alarm[0] = 20; p->value = 1600; break;
    case OBJ_oEmeraldBig: create_treasure(p); make_active(p); p->alarm[0] = 20; p->value = 800; break;
    case OBJ_oSapphireBig: create_treasure(p); make_active(p); p->alarm[0] = 20; p->value = 1200; break;
    case OBJ_oDiamond: create_treasure(p); make_active(p); p->alarm[0] = 20; p->value = 5000; break;
    case OBJ_oGoldBar: create_treasure(p); make_active(p); p->value = 500; break;
    case OBJ_oGoldBars: create_treasure(p); make_active(p); p->value = 1000; break;
    case OBJ_oGoldNugget: create_treasure(p); make_active(p); p->value = 500; break;
    case OBJ_oGoldChunk: create_treasure(p); make_active(p); p->value = 100; break;
    /* ---- oEnemy children ---- */
    case OBJ_oShopkeeper:
        create_enemy(p);
        make_active(p);
        p->xvel = 0;
        p->flags &= (uint16_t)~IF_INVINCIBLE;
        p->style = SHOP_GENERAL;
        p->status = 0;
        p->counter = 0;
        p->facing = 0;
        break;
    case OBJ_oSnake: case OBJ_oCaveman: case OBJ_oManTrap: case OBJ_oHawkman: case OBJ_oYeti: case OBJ_oYetiKing:
    case OBJ_oTombLord: case OBJ_oVampire:
        create_enemy(p);
        make_active(p);
        p->xvel = XVEL_2_5;
        p->flags &= (uint16_t)~IF_INVINCIBLE;
        p->status = 0;
        p->counter = 0;
        p->facing = 1;                                                             /* RIGHT */
        break;
    case OBJ_oAlienBoss:
        create_enemy(p);
        make_active(p);
        p->xvel = XVEL_2_5;
        p->status = 0;
        p->counter = 0;
        p->facing = 0;
        break;
    case OBJ_oBat: case OBJ_oSpiderHang: case OBJ_oSpider: case OBJ_oGiantSpiderHang:
        create_enemy(p);
        make_active(p);
        p->flags &= (uint16_t)~IF_INVINCIBLE;
        p->status = 0;
        if (obj == OBJ_oGiantSpiderHang) {
            /* objects/oGiantSpiderHang/Create_0.gml :25-26 */
            instance_create(x, y + 16, OBJ_oWeb);
            instance_create(x + 16, y + 16, OBJ_oWeb);
        }
        break;
    case OBJ_oScarab:
        /* objects/oScarab/Create_0.gml */
        create_enemy(p);
        p->xvel = p->yvel = 0;
        p->counter = (int16_t)RAND(10, 30);                                        /* :10 */
        if (G.levelType == 0) p->value = 4000;
        else if (G.levelType == 1) p->value = 8000;
        else if (G.levelType == 3) p->value = 12000;
        p->status = 0;
        break;
    case OBJ_oFrog: case OBJ_oFireFrog: case OBJ_oZombie: case OBJ_oMonkey:
        /* objects/oFrog|oFireFrog|oZombie|oMonkey/Create_0.gml: facing = rand(0,1) */
        create_enemy(p);
        make_active(p);
        p->facing = (int8_t)RAND(0, 1);
        p->status = obj == OBJ_oMonkey ? 5 : 0;                                    /* oMonkey: HANG */
        p->counter = 0;
        swim_check(p);
        break;
    case OBJ_oPiranha:
        /* objects/oPiranha/Create_0.gml */
        create_enemy(p);
        p->xvel = p->yvel = 0;
        p->dir = 0;
        if (RAND(1, 2) == 1) p->dir = 180;                                         /* :12 */
        p->status = 0;
        p->counter = 0;
        break;
    case OBJ_oDeadFish:
        create_enemy(p);
        p->dir = 0;
        p->status = 0;
        p->counter = 0;
        break;
    case OBJ_oJaws:
        create_enemy(p);
        p->dir = 180;
        p->facing = 0;
        p->status = 0;
        p->counter = 0;
        break;
    case OBJ_oUFO:
        /* objects/oUFO/Create_0.gml */
        create_enemy(p);
        p->status = 0;
        p->shifttoggle = (int8_t)RAND(0, 1);                                       /* :28 */
        break;
    /* ---- oDrawnSprite children and others ---- */
    case OBJ_oBones: case OBJ_oFakeBones:
        p->yvel = 0;
        break;
    case OBJ_oRubble: case OBJ_oRubbleSmall: case OBJ_oRubbleDarkSmall: case OBJ_oDrip: case OBJ_oLeaf:
    case OBJ_oLavaDrip:
        p->xvel = p->yvel = 0;
        break;
    case OBJ_oChain:
        p->linkval = 2;
        break;
    case OBJ_oPlayer1:
        /* objects/oPlayer1/Create_0.gml, scripts/characterCreateEvent */
        p->flags &= (uint16_t)~(IF_INVINCIBLE | IF_SWIMMING);
        p->facing = 19;
        if (G.isDamsel) inst_set_sprite(i, GSPR_sDamselLeft);
        else if (G.isTunnelMan) inst_set_sprite(i, GSPR_sTunnelLeft);
        break;
    /* no Create event, or nothing modelled: */
    case OBJ_oRoom: case OBJ_oGame: case OBJ_oLevel: case OBJ_oLadderOrange: case OBJ_oLadderTop: case OBJ_oVine:
    case OBJ_oVineTop: case OBJ_oWater: case OBJ_oWaterSwim: case OBJ_oLamp: case OBJ_oLampRed: case OBJ_oWeb:
    case OBJ_oGiantTikiHead: case OBJ_oTikiTorch: case OBJ_oGoldDoor: case OBJ_oFrozenCaveman: case OBJ_oBarrier:
    case OBJ_oTempleFake: case OBJ_oCavemanWorship: case OBJ_oHawkmanWorship: case OBJ_oBlackBG:
    case OBJ_oBlackFadeUp:
        break;
    default:
        UNTRANSLATED(1000 + obj);
        break;
    }
}

/* ---- Destroy events -------------------------------------------------------------------------------------- */
/* rubble pieces: instance_create(x+8+rand(0,r)-rand(0,s), y+8+rand(0,r)-rand(0,s), obj) for each of objs[],
   then rubble.sprite_index = sprs[k] when sprs */
static void rubble(int x, int y, int r, int s, const int16_t *objs, const int16_t *sprs, int n)
{
    int k, rx, ry, o;
    for (k = 0; k < n; k++) {
        ry = y + 8 + RAND(0, r);
        ry -= RAND(0, s);
        rx = x + 8 + RAND(0, r);
        rx -= RAND(0, s);
        o = instance_create(rx, ry, objs[k]);
        if (sprs) inst_set_sprite(o, sprs[k]);
    }
}

static const int16_t RUB3[3] = { OBJ_oRubble, OBJ_oRubbleSmall, OBJ_oRubbleSmall };
static const int16_t RUB_LUSH[3] = { GSPR_sRubbleLush, GSPR_sRubbleLushSmall, GSPR_sRubbleLushSmall };
static const int16_t RUB_TAN[3] = { GSPR_sRubbleTan, GSPR_sRubbleTanSmall, GSPR_sRubbleTanSmall };
static const int16_t RUB_DARK[3] = { OBJ_oRubbleDark, OBJ_oRubbleDarkSmall, OBJ_oRubbleDarkSmall };

/* gold = instance_create(x+8+rand(0,4)-rand(0,4), y+8+rand(0,4)-rand(0,4), obj); gold.xVel = rand(0,3) -
   rand(0,3); gold.yVel = rand(2,4) * 1;  (chunks: 3 oGoldChunk, then one oGoldNugget when nugget) */
static void gold(int x, int y, int nugget)
{
    int k, g, rx, ry;
    for (k = 0; k < 3 + nugget; k++) {
        ry = y + 8 + RAND(0, 4);
        ry -= RAND(0, 4);
        rx = x + 8 + RAND(0, 4);
        rx -= RAND(0, 4);
        g = instance_create(rx, ry, k < 3 ? OBJ_oGoldChunk : OBJ_oGoldNugget);
        rx = RAND(0, 3);
        W.in[g].xvel = (int16_t)((rx - RAND(0, 3)) * 256);
        W.in[g].yvel = (int16_t)(RAND(2, 4) * 256);
    }
}

/* scripts/scrShopkeeperAnger(1), run by an oShopkeeper (the shopkeepers are never dead or angered yet) */
static void scrShopkeeperAnger(int self)
{
    int shp = instance_nearest(W.in[self].x, W.in[self].y, OBJ_oShopkeeper);
    if (shp != INST_NONE) {
        W.in[shp].status = 2;
        if (G.thiefLevel > 0) G.thiefLevel += 3;
        else G.thiefLevel += 2;
    }
}

/* oSolid Destroy (objects/oSolid/Destroy_0.gml) */
static void destroy_solid(int i)
{
    const struct inst *p = &W.in[i];
    int x = p->x, y = p->y, o;
    if (p->flags & IF_SHOPWALL) {                                              /* :1 */
        WITH_BEGIN(s, OBJ_oShopkeeper)
            scrShopkeeperAnger(s);
        WITH_END
    }
    if (collision_point(x + 8, y - 1, OBJ_oSpikes) != INST_NONE) {            /* :9 */
        o = instance_place(i, x + 8, y - 1, OBJ_oSpikes);
        if (o != INST_NONE) instance_destroy(o);
    }
    if (collision_point(x + 8, y - 1, OBJ_oTikiTorch) != INST_NONE) {
        o = instance_place(i, x + 8, y - 1, OBJ_oTikiTorch);
        if (o != INST_NONE) instance_destroy(o);
    }
    if (collision_point(x + 8, y - 1, OBJ_oGrave) != INST_NONE) {
        o = instance_place(i, x + 8, y - 1, OBJ_oGrave);
        if (o != INST_NONE) instance_destroy(o);
    }
    if (collision_point(x + 8, y + 18, OBJ_oLampRed) != INST_NONE) {         /* :24 */
        o = instance_place(i, x + 8, y + 16, OBJ_oLampRed);
        if (o != INST_NONE) {
            instance_create(W.in[o].x + 8, W.in[o].y + 12, OBJ_oLampRedItem);
            instance_destroy(o);
        }
    }
    if (collision_point(x + 8, y + 18, OBJ_oLamp) != INST_NONE) {
        o = instance_place(i, x + 8, y + 16, OBJ_oLamp);
        if (o != INST_NONE) {
            instance_create(W.in[o].x + 8, W.in[o].y + 12, OBJ_oLampItem);
            instance_destroy(o);
        }
    }
    G.checkWater = 1;
}

static void destroy_event(int i)
{
    const struct inst *p = &W.in[i];
    int x = p->x, y = p->y, obj = p->obj, spr = p->spr, k;
    int clean = (p->flags & IF_CLEANDEATH) != 0;
    switch (obj) {
    case OBJ_oBrick:
        /* objects/oBrick/Destroy_0.gml */
        destroy_solid(i);
        if (!clean && !G.cleanSolids) {
            rubble(x, y, 8, 8, RUB3, 0, 3);
            if (spr == GSPR_sBrickGold) gold(x, y, 0);
            if (spr == GSPR_sBrickGoldBig) gold(x, y, 1);
        }
        break;
    case OBJ_oBlock:
        /* objects/oBlock/Destroy_0.gml (no event_inherited) */
        if (!clean) {
            if (!G.cityOfGold) rubble(x, y, 8, 8, RUB3, RUB_LUSH, 3);
            else gold(x, y, 1);
        }
        break;
    case OBJ_oPushBlock:
        /* objects/oPushBlock/Destroy_0.gml */
        if (!clean && !G.cleanSolids) {
            if (!G.cityOfGold) rubble(x, y, 8, 8, RUB3, RUB_LUSH, 3);
            else gold(x, y, 1);
        }
        break;
    case OBJ_oLush:
        /* objects/oLush/Destroy_0.gml */
        destroy_solid(i);
        if (!clean && !G.cleanSolids) {
            rubble(x, y, 8, 8, RUB3, RUB_LUSH, 3);
            if (spr == GSPR_sLushGold) gold(x, y, 0);
            if (spr == GSPR_sLushGoldBig) gold(x, y, 1);
        }
        break;
    case OBJ_oTemple:
        /* objects/oTemple/Destroy_0.gml */
        destroy_solid(i);
        if (!clean && !G.cleanSolids) {
            rubble(x, y, 8, 8, RUB3, RUB_LUSH, 3);
            if (spr == GSPR_sTempleGold) gold(x, y, 0);
            else if (spr == GSPR_sTempleGoldBig || G.cityOfGold) gold(x, y, 1);
            if (p->treasure == TR_BIGRUBY) instance_create(x + 8, y + 8, OBJ_oRubyBig);
        }
        break;
    case OBJ_oDark: case OBJ_oAlienShip:
        /* objects/oDark/Destroy_0.gml, oAlienShip (rand(0,-8)) */
        destroy_solid(i);
        if (obj == OBJ_oAlienShip || (!clean && !G.cleanSolids)) {
            rubble(x, y, 8, -8, RUB_DARK, 0, 3);
            if (obj == OBJ_oDark && spr == GSPR_sDarkGold) gold(x, y, 0);
            if (obj == OBJ_oDark && spr == GSPR_sDarkGoldBig) gold(x, y, 1);
        }
        break;
    case OBJ_oIce:
        /* objects/oIce/Destroy_0.gml (no event_inherited) */
        if (!clean && !G.cleanSolids) {
            int o, rx, ry;
            for (k = 0; k < 3; k++) {
                ry = y + RAND(0, 16);
                rx = x + RAND(0, 16);
                instance_create(rx, ry, OBJ_oDrip);
            }
            if (collision_point(x + 8, y + 8, OBJ_oFrozenCaveman) != INST_NONE) {
                o = instance_place(i, x + 8, y + 8, OBJ_oFrozenCaveman);
                if (o != INST_NONE) instance_destroy(o);
            }
        }
        break;
    case OBJ_oFrozenCaveman:
        if (!G.cleanSolids) {
            int e = instance_create(x, y, OBJ_oCaveman);
            W.in[e].flags |= IF_INVINCIBLE;      /* invincible = 20 */
            W.in[e].status = 98;
            W.in[e].counter = 200;               /* stunTime of oEnemy */
            UNTRANSLATED(2001);                  /* invincible = 20 is not a flag */
        }
        break;
    case OBJ_oTree:
        /* objects/oTree/Destroy_0.gml */
        destroy_solid(i);
        if (!clean && !G.cleanSolids) rubble(x, y, 8, 8, RUB3, 0, 3);
        break;
    case OBJ_oBrickSmooth:
        destroy_solid(i);
        if (!clean && !G.cleanSolids) rubble(x, y, 8, 8, RUB3, RUB_TAN, 3);
        break;
    case OBJ_oTrapBlock: case OBJ_oSpearTrapBottom: case OBJ_oSpearTrapTop: case OBJ_oSpearTrapLit: case OBJ_oSign:
    case OBJ_oAltarLeft: case OBJ_oAltarRight: case OBJ_oSmashTrap: case OBJ_oSmashTrapLit: case OBJ_oCeilingTrap:
    case OBJ_oGrave: case OBJ_oArrowTrapLeft: case OBJ_oArrowTrapLeftLit: case OBJ_oArrowTrapRight:
    case OBJ_oArrowTrapRightLit:
        /* objects/<obj>/Destroy_0.gml: no event_inherited; tan rubble (oGrave: always) */
        if (obj == OBJ_oGrave || (!clean && !G.cleanSolids)) {
            rubble(x, y, 8, 8, RUB3, RUB_TAN, 3);
            if (obj == OBJ_oArrowTrapLeft || obj == OBJ_oArrowTrapLeftLit || obj == OBJ_oArrowTrapRight ||
                obj == OBJ_oArrowTrapRightLit)
                instance_create(x + 8, y + 8, OBJ_oArrow);                  /* fired == 0 */
        }
        if (obj == OBJ_oSpearTrapBottom || obj == OBJ_oSpearTrapTop || obj == OBJ_oSpearTrapLit) G.checkWater = 1;
        break;
    case OBJ_oTreeBranch:
        /* objects/oTreeBranch/Destroy_0.gml */
        if (spr != GSPR_sTreeBranchDeadL && spr != GSPR_sTreeBranchDeadR) {
            static const int16_t LEAF[1] = { OBJ_oLeaf };
            rubble(x, y, 8, 8, LEAF, 0, 1);
        }
        break;
    case OBJ_oLeaves:
        if (spr != GSPR_sLeavesDead && spr != GSPR_sLeavesDeadR) {
            static const int16_t LEAF2[2] = { OBJ_oLeaf, OBJ_oLeaf };
            rubble(x, y, 8, 8, LEAF2, 0, 2);
        }
        break;
    case OBJ_oXocBlock:
        /* objects/oXocBlock/Destroy_0.gml (no event_inherited) */
        if (!clean) {
            if (!G.cityOfGold) rubble(x, y, 8, 8, RUB3, RUB_LUSH, 3);
            else {
                gold(x, y, 1);
                for (k = 0; k < 2; k++) {
                    if (p->treasure == TR_DIAMOND) instance_create(x + 8, y + 8, OBJ_oDiamond);
                    if (p->treasure == TR_RUBY) instance_create(x + 8, y + 8, OBJ_oRubyBig);
                    if (p->treasure == TR_SAPPHIRE) instance_create(x + 8, y + 8, OBJ_oSapphireBig);
                    if (p->treasure == TR_EMERALD) instance_create(x + 8, y + 8, OBJ_oEmeraldBig);
                }
            }
        }
        /* tile = tile_layer_find(99, x, y); if (tile) tile_delete(tile); (scripts/tile_layer_find: the
           layer's tiles newest first, the first whose rectangle holds the point) */
        for (k = gntiles - 1; k >= 0; k--) {
            const struct gtile *t = &gtiles[k];
            if (t->depth == 99 && x >= t->x && y >= t->y && x < t->x + t->w && y < t->y + t->h) {
                int m;
                for (m = k; m < gntiles - 1; m++) gtiles[m] = gtiles[m + 1];
                gntiles--;
                break;
            }
        }
        break;
    case OBJ_oSolid: case OBJ_oThinIce: case OBJ_oMoai: case OBJ_oMoai2: case OBJ_oMoai3: case OBJ_oMoaiInside:
    case OBJ_oLavaSolid: case OBJ_oHardBlock: case OBJ_oDarkFall:
        destroy_solid(i);
        break;
    /* no Destroy event in the chain, or nothing at generation time */
    case OBJ_oSpikes: case OBJ_oTikiTorch: case OBJ_oLamp: case OBJ_oLampRed: case OBJ_oWeb:
        break;
    default:
        if (obj_is(obj, OBJ_oTreasure)) break;                                  /* oTreasure: no Destroy event */
        if (obj_is(obj, OBJ_oItem)) break;                                      /* oItem: only when held */
        UNTRANSLATED(3000 + obj);
        break;
    }
}
