/* scrEntityGen, scrTreasureGen, scrSetupWalls, scrCheckWaterTop (refs/hd/src/scripts/<name>/<name>.gml at 1.2.2;
 * line numbers in comments). Squared distances replace point_distance / distance_to_object (both sides are whole
 * numbers and sqrt is monotonic and correctly rounded, so d < k <=> d^2 < k^2). A `with` body that destroys its
 * own instance goes on with the instance's variables, as GameMaker does.
 */
#include "gen.h"
#include "rng.h"

#define NOTSOL(px, py) (collision_point((px), (py), OBJ_oSolid) == INST_NONE)
#define CP(px, py, o) (collision_point((px), (py), (o)) != INST_NONE)

static int32_t dist2(int x1, int y1, int x2, int y2)
{
    int32_t dx = x2 - x1, dy = y2 - y1;
    return dx * dx + dy * dy;
}

/* distance_to_object(obj) < k; with no instance of obj GameMaker returns a huge distance */
static int dto_lt(int self, int obj, int32_t k)
{
    int32_t d = distance2_to_object(self, obj);
    return d >= 0 && d < k * k;
}

/* distance_to_object(obj) > k */
static int dto_gt(int self, int obj, int32_t k)
{
    int32_t d = distance2_to_object(self, obj);
    return d < 0 || d > k * k;
}

/* point_distance(x, y, oEntrance.x, oEntrance.y) > k */
static int ent_far(int x, int y, int32_t k)
{
    int e = instance_first(OBJ_oEntrance);
    if (e == INST_NONE) { UNTRANSLATED(5001); return 0; }                     /* GML: noone.x is an error */
    return dist2(x, y, W.in[e].x, W.in[e].y) > k * k;
}

static int is_altar(int i) { return W.in[i].obj == OBJ_oSacAltarLeft || W.in[i].obj == OBJ_oSacAltarRight; }
static int is_tree(int i) { return W.in[i].obj == OBJ_oTree; }      /* type == "Tree" */
static int not_xpillar(int x) { return x != 160 && x != 176 && x != 320 && x != 336 && x != 480 && x != 496; }

/* ------------------------------------------------------------------------------------------------------------ */
/* scripts/scrTreasureGen, run by an oSolid (self) */
static void scrTreasureGen(int self, int bones)
{
    const struct inst *s = &W.in[self];
    int x = s->x, y = s->y, n, colStuff, obj;

    if (dto_lt(self, OBJ_oEntrance, 32)) return;                              /* :28 */
    if (dto_lt(self, OBJ_oExit, 32)) return;
    if (dto_lt(self, OBJ_oGoldIdol, 64)) return;

    colStuff = 1;                                                             /* :32 */
    if (NOTSOL(x, y - 16) && !CP(x, y - 1, OBJ_oTreasure) && !CP(x, y - 8, OBJ_oChest) &&
        !CP(x, y - 8, OBJ_oSpikes) && !CP(x, y - 8, OBJ_oEntrance) && !CP(x, y - 8, OBJ_oExit))
        colStuff = 0;

    if (!colStuff) {                                                          /* :43 */
        if (RAND(1, 100) == 1) { instance_create(x + 8, y - 4, OBJ_oRock); return; }
        else if (RAND(1, 40) == 1) { instance_create(x + 8, y - 6, OBJ_oJar); return; }
    }
    if (!colStuff && CP(x, y - 32, OBJ_oSolid) &&                             /* :49 alcove */
        (CP(x - 16, y - 16, OBJ_oSolid) || CP(x + 16, y - 16, OBJ_oSolid) || CP(x - 16, y - 16, OBJ_oBlock) ||
         CP(x + 16, y - 16, OBJ_oBlock))) {
        n = 60;
        if (dto_lt(self, OBJ_oGiantSpider, 100)) n = 5;                       /* :55 */
        if (G.levelType != 2 && RAND(1, n) == 1) instance_create(x, y - 16, OBJ_oWeb);
        else if (G.genUdjatEye && !G.LockedChest) {
            if (RAND(1, G.lockedChestChance) == 1) {
                instance_create(x + 8, y - 8, OBJ_oLockedChest);
                G.LockedChest = 1;
            } else G.lockedChestChance -= 1;
        } else if (RAND(1, 10) == 1) {
            instance_create(x + 8, y - 8, OBJ_oCrate);
        } else if (RAND(1, 15) == 1) {
            instance_create(x + 8, y - 8, OBJ_oChest);
        } else if (!GAME.damsel && RAND(1, 8) == 1 && !CP(x + 8, y - 8, OBJ_oWater)) {   /* :75 */
            obj = instance_create(x + 8, y - 8, OBJ_oDamsel);
            W.in[obj].cost = 0;
            W.in[obj].flags &= (uint16_t)~IF_FORSALE;
            GAME.damsel = 1;
        } else if (RAND(1, 40 - 2 * G.currLevel) <= 1 + bones) {              /* :82 */
            if (RAND(1, 8) == 1) instance_create(x, y - 16, OBJ_oFakeBones);
            else {
                instance_create(x, y - 16, OBJ_oBones);
                instance_create(x + 12, y - 4, OBJ_oSkull);
            }
        } else if (RAND(1, 3) == 1) instance_create(x + 8, y - 4, OBJ_oGoldBar);
        else if (RAND(1, 6) == 1) instance_create(x + 8, y - 8, OBJ_oGoldBars);
        else if (RAND(1, 6) == 1) instance_create(x + 8, y - 4, OBJ_oEmeraldBig);
        else if (RAND(1, 8) == 1) instance_create(x + 8, y - 4, OBJ_oSapphireBig);
        else if (RAND(1, 10) == 1) instance_create(x + 8, y - 4, OBJ_oRubyBig);
    } else if (!colStuff && (CP(x - 16, y - 16, OBJ_oSolid) && CP(x + 16, y - 16, OBJ_oSolid))) {   /* :97 tunnel */
        n = 60;
        if (dto_lt(self, OBJ_oGiantSpider, 100)) n = 10;
        if (G.levelType != 2 && RAND(1, n) == 1) instance_create(x, y - 16, OBJ_oWeb);
        else if (RAND(1, 4) == 1) instance_create(x + 8, y - 4, OBJ_oGoldBar);
        else if (RAND(1, 8) == 1) instance_create(x + 8, y - 8, OBJ_oGoldBars);
        else if (RAND(1, 80 - G.currLevel) <= 1 + bones) {
            if (RAND(1, 8) == 1) instance_create(x, y - 16, OBJ_oFakeBones);
            else {
                instance_create(x, y - 16, OBJ_oBones);
                instance_create(x + 12, y - 4, OBJ_oSkull);
            }
        } else if (RAND(1, 8) == 1) instance_create(x + 8, y - 4, OBJ_oEmeraldBig);
        else if (RAND(1, 9) == 1) instance_create(x + 8, y - 4, OBJ_oSapphireBig);
        else if (RAND(1, 10) == 1) instance_create(x + 8, y - 4, OBJ_oRubyBig);
    } else if (NOTSOL(x, y - 16) && !CP(x, y - 8, OBJ_oChest) && !CP(x, y - 8, OBJ_oSpikes) &&   /* :119 normal */
               !CP(x, y - 8, OBJ_oEntrance) && !CP(x, y - 8, OBJ_oExit)) {
        if (RAND(1, 40) == 1) instance_create(x + 8, y - 4, OBJ_oGoldBar);
        else if (RAND(1, 50) == 1) instance_create(x + 8, y - 8, OBJ_oGoldBars);
        else if (RAND(1, 140 - 2 * G.currLevel) <= 1 + bones) {
            if (RAND(1, 8) == 1) instance_create(x, y - 16, OBJ_oFakeBones);
            else {
                instance_create(x, y - 16, OBJ_oBones);
                instance_create(x + 12, y - 4, OBJ_oSkull);
            }
        }
    }
}

/* the spear trap of the lush and temple branches: scrEntityGen :243-252 / :431-440, run by oSolid s */
static void spear_trap(int s)
{
    int x = W.in[s].x, y = W.in[s].y;
    if (CP(x, y - 16, OBJ_oSolid)) {
        int sol = instance_nearest(x, y - 16, OBJ_oSolid);
        W.in[sol].flags |= IF_CLEANDEATH;
        instance_destroy(sol);
    }
    instance_create(x, y, OBJ_oSpearTrapBottom);
    if (G.darkLevel) instance_create(x, y - 16, OBJ_oSpearTrapLit);
    else instance_create(x, y - 16, OBJ_oSpearTrapTop);
    W.in[s].flags |= IF_CLEANDEATH;
    instance_destroy(s);
}

/* the arrow traps of the mines and temple branches (:146-172, :498-525), run by oBlock b */
static void arrow_trap(int b, int chance)
{
    int x = W.in[b].x, y = W.in[b].y, ent = instance_first(OBJ_oEntrance);
    if (!isInShop(x, y)) {
        int32_t d2;
        if (ent == INST_NONE) { UNTRANSLATED(5002); return; }
        d2 = dist2(x, y, W.in[ent].x, W.in[ent].y);                         /* n = point_distance(..) */
        if (!isInShop(x, y) && RAND(1, chance) == 1 && !(y == W.in[ent].y && d2 < 144 * 144) && d2 > 48 * 48) {
            if (CP(x + 16, y, OBJ_oSolid) && collision_rectangle(x - 32, y, x - 1, y + 15, OBJ_oSolid, b, 0) == INST_NONE) {
                if (G.darkLevel) instance_create(x, y, OBJ_oArrowTrapLeftLit);
                else instance_create(x, y, OBJ_oArrowTrapLeft);
                instance_destroy(b);
            } else if (CP(x - 16, y, OBJ_oSolid) &&
                       collision_rectangle(x + 16, y, x + 48, y + 15, OBJ_oSolid, b, 0) == INST_NONE) {
                if (G.darkLevel) instance_create(x, y, OBJ_oArrowTrapRightLit);
                else instance_create(x, y, OBJ_oArrowTrapRight);
                instance_destroy(b);
            }
        }
    }
}

/* ------------------------------------------------------------------------------------------------------------ */
/* scripts/scrEntityGen */
void scrEntityGen(void)
{
    int n;
    G.LockedChest = 0;                                                        /* :27 */
    G.Key = 0;
    G.lockedChestChance = 8;

    if (G.levelType == 0) {                                                   /* :31 mines */
        G.giantSpider = 0;
        G.genGiantSpider = 0;
        if (RAND(1, 6) == 1) G.genGiantSpider = 1;
        WITH_BEGIN(s, OBJ_oSolid)                                             /* :36 */
            int x = W.in[s].x, y = W.in[s].y;
            if (!isInShop(x, y) && y > 16) {
                if (!is_altar(s)) scrTreasureGen(s, 0);
                if (scrGetRoomX(x) != G.startRoomX || scrGetRoomY(y - 16) != G.startRoomY) {   /* :46 */
                    if (y < G.roomH - 64 && NOTSOL(x, y + 16) && NOTSOL(x, y + 32) && !CP(x, y + 16, OBJ_oWater) &&
                        !CP(x, y + 32, OBJ_oWater) && !CP(x, y + 16, OBJ_oEnemy)) {
                        if (G.genGiantSpider && !G.giantSpider && NOTSOL(x + 16, y + 16) && NOTSOL(x + 16, y + 32) &&
                            RAND(1, 40) == 1) {
                            instance_create(x, y + 16, OBJ_oGiantSpiderHang);
                            G.giantSpider = 1;
                        } else if (G.darkLevel && RAND(1, 60) == 1) instance_create(x, y + 16, OBJ_oLamp);
                        else if (G.darkLevel && RAND(1, 40) == 1) instance_create(x, y + 16, OBJ_oScarab);
                        else if (RAND(1, 60) == 1) instance_create(x, y + 16, OBJ_oBat);
                        else if (RAND(1, 80) == 1) instance_create(x, y + 16, OBJ_oSpiderHang);
                    }
                    if (NOTSOL(x, y - 16)) {                                  /* :68 */
                        if (RAND(1, 60) == 1) instance_create(x, y - 16, OBJ_oSnake);
                        else if (RAND(1, 800) == 1) instance_create(x, y - 16, OBJ_oCaveman);
                    }
                }
            }
        WITH_END

        if (G.genUdjatEye && !G.LockedChest) {                                /* :78 force chest */
            WITH_BEGIN(e, OBJ_oExit)
                int x = W.in[e].x, y = W.in[e].y;
                if (NOTSOL(x - 8, y) && !CP(x - 8, y + 15, OBJ_oTreasure) && !CP(x - 8, y + 8, OBJ_oChest) &&
                    !CP(x - 8, y + 8, OBJ_oSpikes)) {
                    instance_create(x - 8, y + 8, OBJ_oLockedChest);
                    G.LockedChest = 1;
                    break;
                } else if (NOTSOL(x + 8, y) && !CP(x + 8, y + 15, OBJ_oTreasure) && !CP(x + 8, y + 8, OBJ_oChest) &&
                           !CP(x + 8, y + 8, OBJ_oSpikes)) {
                    instance_create(x + 16 + 8, y + 8, OBJ_oLockedChest);
                    G.LockedChest = 1;
                    break;
                } else {
                    instance_create(x + 8, y + 8, OBJ_oLockedChest);
                    G.LockedChest = 1;
                    break;
                }
            WITH_END
        }

        if (instance_exists(OBJ_oLockedChest)) {                              /* :110 key */
            n = 1;
            while (n < 8 && !G.Key) {
                WITH_BEGIN(t, OBJ_oTreasure)
                    int x = W.in[t].x, y = W.in[t].y;
                    if (RAND(1, 8) <= 1 && NOTSOL(x, y) && !G.Key) {
                        if (W.in[t].obj == OBJ_oGoldBars) instance_create(x, y + 4, OBJ_oKey);
                        else instance_create(x, y, OBJ_oKey);
                        G.Key = 1;
                        instance_destroy(t);
                        break;
                    }
                WITH_END
                n += 1;
            }
            if (!G.Key) {                                                     /* :128 */
                WITH_BEGIN(t, OBJ_oTreasure)
                    int x = W.in[t].x, y = W.in[t].y;
                    if (NOTSOL(x, y)) {
                        if (W.in[t].obj == OBJ_oGoldBars) instance_create(x, y + 4, OBJ_oKey);
                        else instance_create(x, y, OBJ_oKey);
                        G.Key = 1;
                        instance_destroy(t);
                        break;
                    }
                WITH_END
            }
        }

        if (G.Key) G.madeUdjatEye = 1;                                        /* :144 */

        WITH_BEGIN(b, OBJ_oBlock)                                             /* :146 arrow traps */
            arrow_trap(b, 4);
        WITH_END
    } else if (G.levelType == 1) {                                            /* :174 lush */
        G.ashGrave = 0;
        if (G.cemetary) {
            WITH_BEGIN(l, OBJ_oLush)                                          /* :179 graves */
                int x = W.in[l].x, y = W.in[l].y, obj;
                if (NOTSOL(x, y - 16) && !CP(x, y - 16, OBJ_oEntrance) && !CP(x, y - 16, OBJ_oExit) &&
                    RAND(1, 20) == 1 && not_xpillar(x)) {
                    obj = instance_create(x, y - 16, OBJ_oGrave);
                    if (!G.ashGrave && RAND(1, 40) == 1) {                    /* :189 */
                        inst_set_sprite(obj, GSPR_sGraveAsh);
                        obj = instance_create(x + 8, y + 8, OBJ_oShotgun);
                        W.in[obj].cost = 0;
                        W.in[obj].flags &= (uint16_t)~IF_FORSALE;
                        /* :195 ashGrave = true: the oLush's own variable, global.ashGrave stays false */
                    } else if (!CP(x + 8, y + 8, OBJ_oTreasure)) {
                        if (RAND(1, 2) == 1) instance_create(x + 8, y + 8, OBJ_oGoldNugget);
                        else if (RAND(1, 4) == 1) instance_create(x + 8, y + 8, OBJ_oSapphireBig);
                        else if (RAND(1, 6) == 1) instance_create(x + 8, y + 8, OBJ_oEmeraldBig);
                        else if (RAND(1, 8) == 1) instance_create(x + 8, y + 8, OBJ_oRubyBig);
                    }
                }
            WITH_END
        }

        WITH_BEGIN(s, OBJ_oSolid)                                             /* :208 */
            int x = W.in[s].x, y = W.in[s].y;
            if (RAND(1, 100) == 1 && NOTSOL(x, y - 16)) tile_add(GSPR_bgTrees, 0, 0, 16, 48, x, y - 32, 9005);

            if (!isInShop(x, y)) {                                            /* :213 */
                if (y > 32 && CP(x, y - 16, OBJ_oSolid) && G.genMarketEntrance && !G.madeMarketEntrance) {
                    int obj = instance_place(s, x, y - 16, OBJ_oSolid);       /* :218 */
                    if (obj == INST_NONE) UNTRANSLATED(5003);
                    else if (!is_tree(obj) && !is_altar(s) && !(W.in[obj].flags & IF_INVINCIBLE) &&
                             RAND(1, G.marketChance) <= 1) {
                        instance_create(x, y - 16, OBJ_oXMarket);
                        W.in[s].flags |= IF_INVINCIBLE;
                        G.madeMarketEntrance = 1;
                    } else G.marketChance -= 1;
                } else if (!is_tree(s) && !is_altar(s) && y != 0 &&                                  /* :227 */
                           collision_rectangle(x, y - 32, x + 15, y - 1, OBJ_oSolid, s, 1) == INST_NONE &&
                           collision_rectangle(x, y - 16, x + 15, y - 1, OBJ_oEnemy, s, 0) == INST_NONE &&
                           (NOTSOL(x - 16, y) || NOTSOL(x + 16, y)) &&
                           CP(x, y + 16, OBJ_oSolid) &&
                           !CP(x, y, OBJ_oXMarket) &&
                           !isInShop(x, y) &&
                           ent_far(x, y, 64)) {
                    if (G.darkLevel && !CP(x, y - 32, OBJ_oWater) && RAND(1, 20) == 1) {
                        instance_create(x, y - 32, OBJ_oTikiTorch);
                    } else if (RAND(1, 12) == 1 && not_xpillar(x)) {
                        spear_trap(s);                                        /* :243-252 */
                    }
                }

                if (!is_altar(s)) {                                           /* :256 */
                    if (G.cemetary) scrTreasureGen(s, 10);
                    else scrTreasureGen(s, 0);
                }

                if (scrGetRoomX(x) != G.startRoomX || scrGetRoomY(y - 16) != G.startRoomY) {   /* :263 */
                    if (y < G.roomH - 64 && NOTSOL(x, y + 16) && NOTSOL(x, y + 32) && !CP(x, y + 16, OBJ_oWater) &&
                        !CP(x, y + 32, OBJ_oWater)) {
                        if (G.cemetary) n = 60;
                        else n = 80;
                        if (G.darkLevel && RAND(1, 40) == 1) instance_create(x, y + 16, OBJ_oScarab);
                        else if (RAND(1, n) == 1) instance_create(x, y + 16, OBJ_oBat);
                    }
                    if (y > 16 && NOTSOL(x, y - 16) && !CP(x, y, OBJ_oEnemy) && !CP(x, y, OBJ_oSpikes)) {   /* :277 */
                        if (G.cemetary) {
                            if (RAND(1, 25) == 1) instance_create(x, y - 16, OBJ_oZombie);
                            else if (RAND(1, 160) == 1) instance_create(x, y - 16, OBJ_oVampire);
                        } else if (!CP(x, y - 16, OBJ_oWater)) {
                            if (G.blackMarket && (y % 128 == 0)) n = 0;
                            else n = 1;
                            if (RAND(1, 60) == n) instance_create(x, y - 16, OBJ_oManTrap);
                            else if (RAND(1, 60) == 1) instance_create(x, y - 16, OBJ_oCaveman);
                            else if (RAND(1, 120) == 1) instance_create(x, y - 16, OBJ_oFireFrog);
                            else if (RAND(1, 30) == 1) instance_create(x, y - 16, OBJ_oFrog);
                        } else if (RAND(1, 120) == 1) instance_create(x, y - 16, OBJ_oFireFrog);
                        else if (RAND(1, 30) == 1) instance_create(x, y - 16, OBJ_oFrog);
                    }
                }
            }
        WITH_END

        if (G.genMarketEntrance && !G.madeMarketEntrance) {                   /* :305 force market entrance */
            WITH_BEGIN(s, OBJ_oSolid)
                int x = W.in[s].x, y = W.in[s].y;
                if (y > 32 && CP(x, y - 16, OBJ_oSolid)) {
                    int obj = instance_place(s, x, y - 16, OBJ_oSolid);
                    if (obj == INST_NONE) UNTRANSLATED(5004);
                    else if (!is_tree(obj) && !is_altar(s) && !(W.in[obj].flags & IF_INVINCIBLE)) {
                        instance_create(x, y - 16, OBJ_oXMarket);
                        W.in[s].flags |= IF_INVINCIBLE;
                        G.madeMarketEntrance = 1;
                    }
                }
            WITH_END
        }

        WITH_BEGIN(v, OBJ_oVine)                                              /* :322 */
            if (RAND(1, 15) == 1) instance_create(W.in[v].x, W.in[v].y, OBJ_oMonkey);
        WITH_END

        WITH_BEGIN(w, OBJ_oWater)                                             /* :327 */
            int x = W.in[w].x, y = W.in[w].y;
            if (NOTSOL(x, y)) {
                if (RAND(1, 30) == 1) {
                    if (G.cemetary) instance_create(x + 4, y + 4, OBJ_oDeadFish);
                    else instance_create(x + 4, y + 4, OBJ_oPiranha);
                }
            }
        WITH_END
    } else if (G.levelType == 2) {                                            /* :339 ice */
        WITH_BEGIN(s, OBJ_oSolid)
            int x = W.in[s].x, y = W.in[s].y;
            if (!isInShop(x, y)) {
                n = 30;
                if (G.yetiLair) n = 90;
                if (scrGetRoomX(x) != G.startRoomX || scrGetRoomY(y - 16) != G.startRoomY) {   /* :349 */
                    if (y < G.roomH - 64 && NOTSOL(x, y + 16) && NOTSOL(x, y + 32) && !CP(x, y + 16, OBJ_oWater) &&
                        !CP(x, y + 32, OBJ_oWater)) {
                        if (G.darkLevel && RAND(1, 40) == 1) instance_create(x, y + 16, OBJ_oScarab);
                    } else if (y > 16 && y < 592 && NOTSOL(x, y - 16) && !isInShop(x, y)) {
                        if (RAND(1, n) == 1) instance_create(x, y - 16, OBJ_oUFO);
                    }
                }
                if (y > 16 && y < 592 && NOTSOL(x, y - 16) && !CP(x + 8, y - 8, OBJ_oEnemy) &&    /* :363 */
                    !CP(x + 8, y - 1, OBJ_oSpikes) && ent_far(x, y, 64) && !isInShop(x, y)) {
                    if (RAND(1, 10) == 1 && W.in[s].spr == GSPR_sDark &&
                        collision_rectangle(x, y - 64, x + 15, y - 1, OBJ_oSolid, s, 0) == INST_NONE &&
                        dto_gt(s, OBJ_oExit, 64))
                        instance_create(x, y - 16, OBJ_oSpringTrap);
                    else if (RAND(1, 20) == 1 && ent_far(x, y, 64)) instance_create(x, y - 16, OBJ_oYeti);
                }
                if (!is_altar(s)) scrTreasureGen(s, 0);                       /* :374 */
            }
        WITH_END
    } else if (G.levelType == 3) {                                            /* :381 temple */
        G.TombLord = 0;
        G.genTombLord = 0;
        if (G.currLevel == 13) G.genTombLord = 1;
        else if (RAND(1, 4) == 1) G.genTombLord = 1;

        G.genGoldEntrance = 0;
        if (G.currLevel == 14) G.genGoldEntrance = 1;
        G.madeGoldEntrance = 0;

        WITH_BEGIN(s, OBJ_oSolid)                                             /* :392 */
            int x = W.in[s].x, y = W.in[s].y;
            if (RAND(1, 100) == 1 && NOTSOL(x, y - 16)) tile_add(GSPR_bgStatues, 0, 0, 16, 48, x, y - 32, 9005);

            if (!isInShop(x, y)) {
                if (y > 32 && NOTSOL(x, y - 16) && G.genGoldEntrance && !G.madeGoldEntrance) {   /* :401 */
                    if (RAND(1, G.goldChance) == 1) {
                        instance_create(x, y - 16, OBJ_oGoldDoor);
                        W.in[s].flags |= IF_INVINCIBLE;
                        G.madeGoldEntrance = 1;
                    } else G.goldChance -= 1;
                } else if (!is_tree(s) && !is_altar(s) && y != 0 &&                                  /* :411 */
                           NOTSOL(x, y - 16) &&
                           !CP(x, y - 16, OBJ_oLava) &&
                           collision_rectangle(x, y - 16, x + 15, y - 1, OBJ_oEnemy, s, 0) == INST_NONE &&
                           NOTSOL(x, y - 32) &&
                           (NOTSOL(x - 16, y) || NOTSOL(x + 16, y)) &&
                           CP(x, y + 16, OBJ_oSolid) &&
                           !isInShop(x, y) &&
                           not_xpillar(x)) {
                    if (RAND(1, 12) == 1 && ent_far(x, y, 64)) {
                        if (CP(x - 16, y - 32, OBJ_oSolid) && CP(x + 16, y - 32, OBJ_oSolid) && NOTSOL(x, y - 32)) {
                            /* do nothing (:427) */
                        } else {
                            spear_trap(s);                                    /* :431-440 */
                        }
                    }
                }

                if (y < G.roomH - 64 && NOTSOL(x, y + 16) && NOTSOL(x, y + 32) && !CP(x, y + 16, OBJ_oWater) &&
                    !CP(x, y + 32, OBJ_oWater)) {                             /* :446 */
                    if (G.darkLevel && RAND(1, 40) == 1) instance_create(x, y + 16, OBJ_oScarab);
                }

                /* :453 GameMaker 2 precedence: A or (B and C) */
                if (scrGetRoomX(x) != G.startRoomX ||
                    (scrGetRoomY(y - 16) != G.startRoomY && !CP(x, y - 16, OBJ_oEnemy))) {
                    if (y > 16 && NOTSOL(x, y - 16)) {
                        if (G.genTombLord && !G.TombLord &&
                            collision_rectangle(x, y - 32, x + 32, y - 1, OBJ_oSolid, s, 0) == INST_NONE &&
                            RAND(1, 40) == 1) {
                            instance_create(x, y - 32, OBJ_oTombLord);
                            G.TombLord = 1;
                        } else if (RAND(1, 40) == 1) instance_create(x, y - 16, OBJ_oCaveman);
                        else if (RAND(1, 40) == 1) instance_create(x, y - 16, OBJ_oHawkman);
                        else if (RAND(1, 60) == 1) {
                            if (G.darkLevel) instance_create(x, y - 16, OBJ_oSmashTrapLit);
                            else instance_create(x, y - 16, OBJ_oSmashTrap);
                        }
                    }
                }

                if (!is_altar(s)) scrTreasureGen(s, 0);                       /* :476 */
            }
        WITH_END

        if (G.genGoldEntrance && !G.madeGoldEntrance) {                       /* :484 force gold door */
            WITH_BEGIN(s, OBJ_oSolid)
                int x = W.in[s].x, y = W.in[s].y;
                if (y > 32 && NOTSOL(x, y - 16)) {
                    instance_create(x, y - 16, OBJ_oGoldDoor);
                    W.in[s].flags |= IF_INVINCIBLE;
                    G.madeGoldEntrance = 1;
                    break;
                }
            WITH_END
        }

        WITH_BEGIN(b, OBJ_oBlock)                                             /* :498 */
            W.in[b].flags |= IF_CLEANDEATH;
            arrow_trap(b, 3);
        WITH_END
    }

    if (G.darkLevel) {                                                        /* :529 flares */
        WITH_BEGIN(e, OBJ_oEntrance)
            int x = W.in[e].x, y = W.in[e].y;
            if (NOTSOL(x - 16, y)) instance_create(x - 16 + 8, y + 8, OBJ_oFlareCrate);
            else if (NOTSOL(x + 16, y)) instance_create(x + 16 + 8, y + 8, OBJ_oFlareCrate);
            else instance_create(x + 8, y + 8, OBJ_oFlareCrate);
        WITH_END
    }
    G.cleanSolids = 0;                                                        /* :548 */
}

/* ------------------------------------------------------------------------------------------------------------ */
/* scripts/scrCheckWaterTop, run by an oWater (recursive) */
void scrCheckWaterTop(int self)
{
    int k;
    for (k = 0; k < 2; k++) {
        int obj = instance_place(self, W.in[self].x + (k ? 16 : -16), W.in[self].y, OBJ_oWater);
        if (obj != INST_NONE && W.in[obj].spr != GSPR_sWaterTop && W.in[obj].spr != GSPR_sLavaTop) {
            inst_set_sprite(obj, W.in[obj].obj == OBJ_oLava ? GSPR_sLavaTop : GSPR_sWaterTop);
            scrCheckWaterTop(obj);
        }
    }
}

/* ------------------------------------------------------------------------------------------------------------ */
/* scripts/scrSetupWalls(bottom) */
void scrSetupWalls(int bottom)
{
    WITH_BEGIN(b, OBJ_oBrick)                                                 /* :25 */
        int x = W.in[b].x, y = W.in[b].y, up = 0, down = 0;
        if (y == 0 || CP(x, y - 16, OBJ_oBrick) || CP(x, y - 16, OBJ_oHardBlock)) up = 1;
        if (y >= bottom || CP(x, y + 16, OBJ_oBrick) || CP(x, y + 16, OBJ_oHardBlock)) down = 1;
        /* left / right (:34-35) are computed but not used: collision tests have no side effect */
        if (!up) {
            inst_set_sprite(b, GSPR_sCaveUp);
            if (G.graphicsHigh) {
                if (RAND(1, 3) < 3) tile_add(GSPR_bgCaveTop, 0, 0, 16, 16, x, y - 16, 3);
                else tile_add(GSPR_bgCaveTop, 16, 0, 16, 16, x, y - 16, 3);
            }
        }
        if (!down) {
            if (!up) inst_set_sprite(b, GSPR_sCaveUp2);
            else inst_set_sprite(b, GSPR_sBrickDown);
        }
    WITH_END

    WITH_BEGIN(b, OBJ_oLush)                                                  /* :70 */
        int x = W.in[b].x, y = W.in[b].y, up = 0, down = 0;
        if (y == 0 || CP(x, y - 16, OBJ_oLush)) up = 1;
        if (y >= bottom || CP(x, y + 16, OBJ_oLush)) down = 1;
        if (!up) {
            inst_set_sprite(b, GSPR_sLushUp);
            if (G.graphicsHigh) {
                if (RAND(1, 8) == 1) tile_add(GSPR_bgCaveTop2, 32, 0, 16, 16, x, y - 16, 3);
                else if (RAND(1, 3) < 3) tile_add(GSPR_bgCaveTop2, 0, 0, 16, 16, x, y - 16, 3);
                else tile_add(GSPR_bgCaveTop2, 16, 0, 16, 16, x, y - 16, 3);
            }
        }
        if (!down) {
            if (!up) inst_set_sprite(b, GSPR_sLushUp2);
            else inst_set_sprite(b, GSPR_sLushDown);
            if (NOTSOL(x, y + 16) && G.graphicsHigh) {                        /* :99 */
                if (RAND(1, 12) == 1) tile_add(GSPR_bgCaveTop2, 48, 0, 16, 16, x, y + 16, 3);
                else if (RAND(1, 12) == 1) tile_add(GSPR_bgCaveTop2, 64, 0, 16, 16, x, y + 16, 3);
            }
        }
    WITH_END

    WITH_BEGIN(b, OBJ_oDark)                                                  /* :109 */
        int x = W.in[b].x, y = W.in[b].y, up = 0, down = 0;
        if (y == 0 || CP(x, y - 16, OBJ_oDark)) up = 1;
        if (y >= bottom || CP(x, y + 16, OBJ_oDark)) down = 1;
        if (!up) {
            inst_set_sprite(b, GSPR_sDarkUp);
            if (G.graphicsHigh) {
                if (RAND(1, 3) < 3) tile_add(GSPR_bgCaveTop3, 0, 0, 16, 16, x, y - 16, 3);
                else tile_add(GSPR_bgCaveTop3, 16, 0, 16, 16, x, y - 16, 3);
            }
        }
        if (!down) {
            if (!up) inst_set_sprite(b, GSPR_sDarkUp2);
            else inst_set_sprite(b, GSPR_sDarkDown);
        }
    WITH_END

    WITH_BEGIN(b, OBJ_oIce)                                                   /* :151 */
        int x = W.in[b].x, y = W.in[b].y, up = 0, down = 0, left = 0, right = 0;
        if (CP(x, y - 16, OBJ_oIce)) up = 1;
        if (CP(x, y + 16, OBJ_oIce)) down = 1;
        if (CP(x - 16, y, OBJ_oIce)) left = 1;
        if (CP(x + 16, y, OBJ_oIce)) right = 1;
        if (!up) inst_set_sprite(b, GSPR_sIceUp);
        if (!down) {
            if (!up) inst_set_sprite(b, GSPR_sIceUp2);
            else inst_set_sprite(b, GSPR_sIceDown);
            if (RAND(1, 20) == 1 && NOTSOL(x, y + 16)) instance_create(x, y + 16, OBJ_oIceBottom);   /* :171 */
        }
        if (!left) {
            if (!up && !down) inst_set_sprite(b, GSPR_sIceUDL);
            else if (!up) inst_set_sprite(b, GSPR_sIceUL);
            else if (!down) inst_set_sprite(b, GSPR_sIceDL);
            else inst_set_sprite(b, GSPR_sIceLeft);
        }
        if (!right) {
            if (!up && !down) inst_set_sprite(b, GSPR_sIceUDR);
            else if (!up) inst_set_sprite(b, GSPR_sIceUR);
            else if (!down) inst_set_sprite(b, GSPR_sIceDR);
            else inst_set_sprite(b, GSPR_sIceRight);
        }
        if (!up && !left && !right && down) inst_set_sprite(b, GSPR_sIceULR);
        if (!down && !left && !right && up) inst_set_sprite(b, GSPR_sIceDLR);
        if (up && down && !left && !right) inst_set_sprite(b, GSPR_sIceLR);
        if (!up && !down && !left && !right) inst_set_sprite(b, GSPR_sIceBlock);
    WITH_END

    {
        /* :196 oTemple and :280 oTempleFake: the same rules but for `up` (oTemple: y+16 for oTempleFake, as the
           GML has it) and the tiles (oTempleFake: only with cityOfGold) */
        int pass;
        for (pass = 0; pass < 2; pass++) {
            int tobj = pass ? OBJ_oTempleFake : OBJ_oTemple;
            WITH_BEGIN(b, tobj)
                int x = W.in[b].x, y = W.in[b].y, up = 0, down = 0, left = 0, right = 0;
                if (!pass) {
                    if (y == 0 || CP(x, y - 16, OBJ_oTemple) || CP(x, y + 16, OBJ_oTempleFake)) up = 1;
                    if (y >= bottom || CP(x, y + 16, OBJ_oTemple) || CP(x, y + 16, OBJ_oTempleFake)) down = 1;
                } else {
                    if (y == 0 || CP(x, y - 16, OBJ_oTemple) || CP(x, y - 16, OBJ_oTempleFake)) up = 1;
                    if (CP(x, y + 16, OBJ_oTemple) || CP(x, y + 16, OBJ_oTempleFake)) down = 1;
                }
                if (CP(x - 16, y, OBJ_oTemple) || CP(x - 16, y, OBJ_oTempleFake)) left = 1;
                if (CP(x + 16, y, OBJ_oTemple) || CP(x + 16, y, OBJ_oTempleFake)) right = 1;
                if (G.cityOfGold) {
                    if (!up) {
                        inst_set_sprite(b, GSPR_sGTempleUp);
                        if (G.graphicsHigh) {
                            if (RAND(1, 4) == 1) tile_add(GSPR_bgCaveTop4, 0, 0, 16, 16, x, y - 16, 3);
                            else if (RAND(1, 4) == 1) tile_add(GSPR_bgCaveTop4, 16, 0, 16, 16, x, y - 16, 3);
                        }
                        if (!left && !right) inst_set_sprite(b, !down ? GSPR_sGTempleUp6 : GSPR_sGTempleUp5);
                        else if (!left) inst_set_sprite(b, !down ? GSPR_sGTempleUp7 : GSPR_sGTempleUp3);
                        else if (!right) inst_set_sprite(b, !down ? GSPR_sGTempleUp8 : GSPR_sGTempleUp4);
                        else if (left && right && !down) inst_set_sprite(b, GSPR_sGTempleUp2);
                    } else if (!down) inst_set_sprite(b, GSPR_sGTempleDown);
                } else {
                    if (!up) {
                        inst_set_sprite(b, GSPR_sTempleUp);
                        if (!pass && G.graphicsHigh) {
                            if (RAND(1, 4) == 1) tile_add(GSPR_bgCaveTop4, 0, 0, 16, 16, x, y - 16, 3);
                            else if (RAND(1, 4) == 1) tile_add(GSPR_bgCaveTop4, 16, 0, 16, 16, x, y - 16, 3);
                        }
                        if (!left && !right) inst_set_sprite(b, !down ? GSPR_sTempleUp6 : GSPR_sTempleUp5);
                        else if (!left) inst_set_sprite(b, !down ? GSPR_sTempleUp7 : GSPR_sTempleUp3);
                        else if (!right) inst_set_sprite(b, !down ? GSPR_sTempleUp8 : GSPR_sTempleUp4);
                        else if (left && right && !down) inst_set_sprite(b, GSPR_sTempleUp2);
                    } else if (!down) inst_set_sprite(b, GSPR_sTempleDown);
                }
            WITH_END
        }
    }

    WITH_BEGIN(w, OBJ_oWater)                                                 /* :360 */
        if (W.in[w].obj != OBJ_oLava) {                                       /* type == "Water" */
            int x = W.in[w].x, y = W.in[w].y, up = 0, upWater = 0, down = 0;
            if (CP(x, y - 16, OBJ_oWater)) upWater = 1;
            if (CP(x, y - 16, OBJ_oSolid)) up = 1;
            if (CP(x, y + 16, OBJ_oSolid) && !CP(x, y + 16, OBJ_oWater)) down = 1;
            if (!up && !upWater) inst_set_sprite(w, GSPR_sWaterTop);
            if (upWater && CP(x, y - 32, OBJ_oWater) && down && RAND(1, 4) == 1) {   /* :379 */
                int water;
                inst_set_sprite(w, GSPR_sWaterBottomTall2);
                water = instance_place(w, x, y - 16, OBJ_oWater);
                if (water != INST_NONE) inst_set_sprite(water, GSPR_sWaterBottomTall1);
            } else if ((up || upWater) && down) {
                switch (RAND(1, 4)) {
                case 1: inst_set_sprite(w, GSPR_sWaterBottom); break;
                case 2: inst_set_sprite(w, GSPR_sWaterBottom2); break;
                case 3: inst_set_sprite(w, GSPR_sWaterBottom3); break;
                case 4: inst_set_sprite(w, GSPR_sWaterBottom4); break;
                }
            }
        }
    WITH_END

    WITH_BEGIN(w, OBJ_oLava)                                                  /* :399 */
        int x = W.in[w].x, y = W.in[w].y, up = 0, upWater = 0;
        if (CP(x, y - 16, OBJ_oWater)) upWater = 1;
        if (CP(x, y - 16, OBJ_oSolid)) up = 1;
        if (!up && !upWater) {
            inst_set_sprite(w, GSPR_sLavaTop);
            if (RAND(1, 4) == 1) W.in[w].flags |= IF_SPURT;
        }
    WITH_END

    WITH_BEGIN(v, OBJ_oVine)                                                  /* :438 */
        int x = W.in[v].x, y = W.in[v].y, up = 0, down = 0;
        if (CP(x + 8, y - 8, OBJ_oLadder)) up = 1;
        if (CP(x + 8, y + 16, OBJ_oLadder)) down = 1;
        if (!up) inst_set_sprite(v, GSPR_sVineSource);
        else if (!down) inst_set_sprite(v, GSPR_sVineBottom);
    WITH_END

    WITH_BEGIN(b, OBJ_oBlock)                                                 /* :458 */
        int x = W.in[b].x, y = W.in[b].y, down = 0;
        if (CP(x, y + 16, OBJ_oBrick) || CP(x, y + 16, OBJ_oTemple) || CP(x, y + 16, OBJ_oHardBlock)) down = 1;
        if (CP(x - 16, y, OBJ_oLava) || CP(x + 16, y, OBJ_oLava)) down = 0;
        if (down && RAND(1, 4) == 1) {
            instance_create(x, y, OBJ_oPushBlock);
            instance_destroy(b);
        }
    WITH_END

    WITH_BEGIN(t, OBJ_oTree)                                                  /* :474 */
        int x = W.in[t].x, y = W.in[t].y;
        if (!CP(x, y - 16, OBJ_oTree)) {
            inst_set_sprite(t, G.cemetary ? GSPR_sTreeTopDead : GSPR_sTreeTop);
            W.in[t].depth = 1;
        }
    WITH_END

    WITH_BEGIN(t, OBJ_oTreeBranch)                                            /* :494 */
        int x = W.in[t].x, y = W.in[t].y, up = 0, right = 0;
        if (CP(x, y - 16, OBJ_oLeaves)) up = 1;
        if (CP(x + 16, y, OBJ_oTree)) right = 1;
        if (up) instance_destroy(t);
        if (right) inst_set_sprite(t, G.cemetary ? GSPR_sTreeBranchDeadL : GSPR_sTreeBranchLeft);
    WITH_END
}
