/* Create and Destroy events of the objects the mines generator creates (refs/hd/src/objects/<obj>/Create_0.gml,
 * Destroy_0.gml). `action_inherited()` / an absent event runs the parent's event; the chains are written out
 * per object. Only the variables of struct inst are kept; string variables (type, buyMessage, ...) are implied
 * by the object (type "Altar" = oSacAltarLeft / oSacAltarRight, "Gold Bars" = oGoldBars, "Exit" = oEntrance /
 * oExit). Instance-variable values from the GML: LEFT / RIGHT are 18 / 19 in oItem and oTreasure, 0 / 1 in oEnemy.
 */
#include "gen.h"
#include "rng.h"

#define XVEL_2_5 640                 /* xVel = 2.5, in 1/256 */

/* oSolid Create (objects/oSolid/Create_0.gml): invincible = false, shopWall = false, cleanDeath = false */
static void create_solid(struct inst *p)
{
    p->flags &= (uint8_t)~(IF_INVINCIBLE | IF_SHOPWALL | IF_CLEANDEATH);
}

/* oItem Create (objects/oItem/Create_0.gml) */
static void create_item(struct inst *p)
{
    p->value = 0;
    p->cost = 0;
    p->flags &= (uint8_t)~(IF_FORSALE | IF_INDICEHOUSE);
    p->depth = G.hasSpectacles ? 51 : 101;     /* :38-39 */
}

/* makeActive() (scripts/makeActive): xVel = yVel = xAcc = yAcc = 0 */
static void make_active(struct inst *p)
{
    p->xvel = p->yvel = 0;
}

/* oTreasure Create (objects/oTreasure/Create_0.gml) */
static void create_treasure(struct inst *p)
{
    p->value = 0;
    p->xvel = p->yvel = 0;
}

/* oEnemy Create (objects/oEnemy/Create_0.gml) */
static void create_enemy(struct inst *p)
{
    p->cost = 0;
    p->flags &= (uint8_t)~IF_FORSALE;
    p->facing = 0;
}

static void create_shop_item(struct inst *p, int32_t cost)
{
    create_item(p);
    make_active(p);
    p->cost = cost;
}

static void create_event(int i);

int instance_create(int x, int y, int obj)
{
    int i = inst_add(obj, x, y, W.next_id++);
    create_event(i);
    return i;
}

static void create_event(int i)
{
    struct inst *p = &W.in[i];
    int obj = p->obj, x = p->x, y = p->y, n;
    switch (obj) {
    case OBJ_oBrick:
        /* objects/oBrick/Create_0.gml */
        create_solid(p);
        n = RAND(1, 10);                                                           /* :2 */
        if (n == 1) inst_set_sprite(i, GSPR_sBrick2);
        n = RAND(1, 100);                                                          /* :6 */
        if (n < 20) inst_set_sprite(i, GSPR_sBrickGold);
        else if (n < 30) inst_set_sprite(i, GSPR_sBrickGoldBig);
        else if (x > 1 && x < 672 - 16 && y > 1 && y < 544 - 16) {                /* :9, isLevel() true */
            int o;
            if (RAND(1, 100) == 1) instance_create(x + 8, y + 8, OBJ_oSapphireBig);
            else if (RAND(1, 120) == 1) instance_create(x + 8, y + 8, OBJ_oEmeraldBig);
            else if (RAND(1, 140) == 1) instance_create(x + 8, y + 8, OBJ_oRubyBig);
            else if (RAND(1, 1200) == 1) scrGenerateItem(i, x + 8, y + 8, 2, &o);
        }
        break;
    case OBJ_oBlock:
        /* objects/oBlock/Create_0.gml */
        create_solid(p);
        p->flags &= (uint8_t)~IF_CLEANDEATH;
        if (G.cityOfGold) inst_set_sprite(i, GSPR_sGoldBlock);
        break;
    case OBJ_oPushBlock:
        /* objects/oPushBlock/Create_0.gml; oMoveableSolid: xVel = yVel = 0 */
        create_solid(p);
        p->xvel = p->yvel = 0;
        p->flags &= (uint8_t)~IF_INVINCIBLE;
        if (G.cityOfGold) inst_set_sprite(i, GSPR_sGoldBlock);
        p->flags &= (uint8_t)~IF_CLEANDEATH;
        break;
    case OBJ_oHardBlock:
        create_solid(p);
        p->flags |= IF_INVINCIBLE;
        break;
    case OBJ_oSolid: case OBJ_oAltarLeft: case OBJ_oAltarRight: case OBJ_oSign: case OBJ_oBrickSmooth:
    case OBJ_oSacAltarLeft: case OBJ_oSacAltarRight:
        /* oSolid's event (the others have none, or add type = "Altar" / defile) */
        create_solid(p);
        break;
    case OBJ_oArrowTrapLeft: case OBJ_oArrowTrapLeftLit: case OBJ_oArrowTrapRight: case OBJ_oArrowTrapRightLit:
        /* objects/oArrowTrapLeft|Right/Create_0.gml (Lit: inherited) */
        create_solid(p);
        p->facing = (obj == OBJ_oArrowTrapLeft || obj == OBJ_oArrowTrapLeftLit) ? 0 : 1;
        p->flags &= (uint8_t)~IF_INVINCIBLE;
        p->alarm[1] = 1;
        break;
    case OBJ_oSpikes:
        p->flags &= (uint8_t)~IF_INVINCIBLE;
        break;
    case OBJ_oKaliHead:
        /* objects/oKaliHead/Create_0.gml */
        switch (RAND(1, 3)) {
        case 1: inst_set_sprite(i, GSPR_sKaliHead1); break;
        case 2: inst_set_sprite(i, GSPR_sKaliHead2); break;
        case 3: inst_set_sprite(i, GSPR_sKaliHead3); break;
        }
        break;
    /* oItem children */
    case OBJ_oChest: case OBJ_oCrate: case OBJ_oLockedChest: case OBJ_oRock: case OBJ_oJar: case OBJ_oSkull:
    case OBJ_oBall:
        create_item(p);
        make_active(p);
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
    case OBJ_oGoldIdol:
        create_item(p);
        make_active(p);
        p->value = 5000;
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
        p->flags &= (uint8_t)~IF_INVINCIBLE;
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
    /* oTreasure children */
    case OBJ_oRubyBig: create_treasure(p); make_active(p); p->alarm[0] = 20; p->value = 1600; break;
    case OBJ_oEmeraldBig: create_treasure(p); make_active(p); p->alarm[0] = 20; p->value = 800; break;
    case OBJ_oSapphireBig: create_treasure(p); make_active(p); p->alarm[0] = 20; p->value = 1200; break;
    case OBJ_oGoldBar: create_treasure(p); make_active(p); p->value = 500; break;
    case OBJ_oGoldBars: create_treasure(p); make_active(p); p->value = 1000; break;
    case OBJ_oGoldNugget: create_treasure(p); make_active(p); p->value = 500; break;
    case OBJ_oGoldChunk: create_treasure(p); make_active(p); p->value = 100; break;
    /* oEnemy children */
    case OBJ_oShopkeeper:
        create_enemy(p);
        make_active(p);
        p->xvel = 0;
        p->flags &= (uint8_t)~IF_INVINCIBLE;
        p->style = SHOP_GENERAL;
        p->status = 0;
        p->counter = 0;
        p->facing = 0;
        break;
    case OBJ_oSnake:
        create_enemy(p);
        make_active(p);
        p->xvel = XVEL_2_5;
        p->flags &= (uint8_t)~IF_INVINCIBLE;
        p->status = 0;
        p->counter = 0;
        p->facing = 1;
        break;
    case OBJ_oCaveman:
        create_enemy(p);
        make_active(p);
        p->xvel = XVEL_2_5;
        p->flags &= (uint8_t)~IF_INVINCIBLE;
        p->status = 0;
        p->counter = 0;
        p->facing = 1;
        break;
    case OBJ_oBat: case OBJ_oSpiderHang:
        create_enemy(p);
        if (obj == OBJ_oSpiderHang) make_active(p);
        p->xvel = p->yvel = 0;
        p->flags &= (uint8_t)~IF_INVINCIBLE;
        p->status = 0;
        break;
    case OBJ_oGiantSpiderHang:
        /* objects/oGiantSpiderHang/Create_0.gml */
        create_enemy(p);
        make_active(p);
        p->flags &= (uint8_t)~IF_INVINCIBLE;
        p->status = 0;
        instance_create(x, y + 16, OBJ_oWeb);                                      /* :25 */
        instance_create(x + 16, y + 16, OBJ_oWeb);                                 /* :26 */
        break;
    case OBJ_oScarab:
        /* objects/oScarab/Create_0.gml */
        create_enemy(p);
        p->xvel = p->yvel = 0;
        p->counter = (int16_t)RAND(10, 30);                                        /* :10 */
        if (G.levelType == 0) p->value = 4000;
        else if (G.levelType == 1) p->value = 8000;
        else if (G.levelType == 3) p->value = 12000;
        p->flags &= (uint8_t)~IF_INVINCIBLE;
        p->status = 0;
        break;
    /* oDrawnSprite children and others with nothing kept */
    case OBJ_oBones: case OBJ_oFakeBones:
        p->yvel = 0;
        break;
    case OBJ_oRubble: case OBJ_oRubbleSmall:
        p->xvel = p->yvel = 0;
        break;
    case OBJ_oPlayer1:
        /* objects/oPlayer1/Create_0.gml, scripts/characterCreateEvent */
        p->flags &= (uint8_t)~IF_INVINCIBLE;
        p->facing = 19;
        if (G.isDamsel) inst_set_sprite(i, GSPR_sDamselLeft);
        else if (G.isTunnelMan) inst_set_sprite(i, GSPR_sTunnelLeft);
        break;
    default:
        break;
    }
}

/* instance_destroy(): the Destroy event, then gone */
void instance_destroy(int i)
{
    struct inst *p = &W.in[i];
    int x = p->x, y = p->y, k, r, ry, rx;
    switch (p->obj) {
    case OBJ_oBlock:
        /* objects/oBlock/Destroy_0.gml (no event_inherited: oSolid's Destroy does not run) */
        if (!(p->flags & IF_CLEANDEATH)) {
            if (!G.cityOfGold) {
                static const int16_t robj[3] = { OBJ_oRubble, OBJ_oRubbleSmall, OBJ_oRubbleSmall };
                static const int16_t rspr[3] = { GSPR_sRubbleLush, GSPR_sRubbleLushSmall, GSPR_sRubbleLushSmall };
                for (k = 0; k < 3; k++) {                                          /* :6-11 */
                    /* instance_create(x+8+rand(0,8)-rand(0,8), y+8+rand(0,8)-rand(0,8), ..): y argument first */
                    ry = y + 8 + RAND(0, 8);
                    ry -= RAND(0, 8);
                    rx = x + 8 + RAND(0, 8);
                    rx -= RAND(0, 8);
                    r = instance_create(rx, ry, robj[k]);
                    inst_set_sprite(r, rspr[k]);
                }
            } else {
                for (k = 0; k < 3; k++) {                                          /* :15-20 */
                    ry = y + 8 + RAND(0, 4);
                    ry -= RAND(0, 4);
                    rx = x + 8 + RAND(0, 4);
                    rx -= RAND(0, 4);
                    r = instance_create(rx, ry, OBJ_oGoldChunk);
                    rx = RAND(0, 3);
                    W.in[r].xvel = (int16_t)((rx - RAND(0, 3)) * 256);
                    W.in[r].yvel = (int16_t)(RAND(2, 4) * 256);
                }
                ry = y + 8 + RAND(0, 4);                                           /* :21-23 */
                ry -= RAND(0, 4);
                rx = x + 8 + RAND(0, 4);
                rx -= RAND(0, 4);
                r = instance_create(rx, ry, OBJ_oGoldNugget);
                rx = RAND(0, 3);
                W.in[r].xvel = (int16_t)((rx - RAND(0, 3)) * 256);
                W.in[r].yvel = (int16_t)(RAND(2, 4) * 256);
            }
        }
        break;
    default:
        /* the other objects the mines generator destroys (oTreasure children) have no Destroy event in their
           chain that does anything at generation time (oItem's needs `held`) */
        break;
    }
    inst_destroyed(i);
}
