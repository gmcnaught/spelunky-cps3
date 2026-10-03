/* scrInitLevel and the scripts it calls, mines (levelType 0). GML: refs/hd/src/scripts/<name>/<name>.gml at tag
 * 1.2.2; line numbers in comments. Squared distances replace point_distance / distance_to_object (both sides
 * are whole numbers, sqrt is monotonic and correctly rounded, so `d < k` <=> `d^2 < k^2`).
 */
#include "gen.h"
#include "rng.h"

struct gglobals G;
struct ggame GAME;
struct gtile gtiles[GTILES_MAX];
int gntiles;

#define ROOM_W 672
#define ROOM_H 544
#define NOTSOL(px, py) (collision_point((px), (py), OBJ_oSolid) == INST_NONE)
#define CP(px, py, o) (collision_point((px), (py), (o)) != INST_NONE)

/* rLevel's room instance ids (Observed: reference dumps, every case; the ids stored in the room) */
static const int32_t rlevel_ids[19] = {
    104034,                                                          /* oPlayer1 */
    104017, 104018, 104019, 104020, 104021, 104022, 104023, 104024,  /* oRoom x 16 */
    104025, 104026, 104027, 104028, 104029, 104030, 104031, 104032,
    104016,                                                          /* oLevel */
    104033,                                                          /* oGame */
};

void tile_add(int bg, int left, int top, int w, int h, int x, int y, int depth)
{
    if (gntiles < GTILES_MAX) {
        struct gtile *t = &gtiles[gntiles++];
        t->bg = (int16_t)bg; t->left = (int16_t)left; t->top = (int16_t)top; t->w = (int16_t)w; t->h = (int16_t)h;
        t->x = (int16_t)x; t->y = (int16_t)y; t->depth = (int16_t)depth;
    }
}

/* scripts/scrGetRoomX, scrGetRoomY */
int scrGetRoomX(int tx)
{
    if (tx < 160 + 16) return 0;
    else if (tx >= 160 + 16 && tx < 320 + 16) return 1;
    else if (tx >= 320 + 16 && tx < 480 + 16) return 2;
    else if (tx >= 480 + 16) return 3;
    return -1;
}

int scrGetRoomY(int ty)
{
    if (ty < 128 + 16) return 0;
    else if (ty >= 128 + 16 && ty < 256 + 16) return 1;
    else if (ty >= 256 + 16 && ty < 384 + 16) return 2;
    else if (ty >= 384 + 16 && ty < 512 + 16) return 3;
    else if (ty >= 512 + 16) return 4;
    return -1;
}

/* scripts/isInShop */
int isInShop(int tx, int ty)
{
    int p = G.roomPath[scrGetRoomX(tx)][scrGetRoomY(ty)];
    return p == 4 || p == 5;
}

static int32_t dist2(int x1, int y1, int x2, int y2)
{
    int32_t dx = x2 - x1, dy = y2 - y1;
    return dx * dx + dy * dy;
}

/* distance_to_object(obj) < k */
static int dto_lt(int self, int obj, int32_t k)
{
    int32_t d = distance2_to_object(self, obj);
    return d >= 0 && d < k * k;
}

void gen_new_game(void)
{
    int i, j;
    /* scripts/scrClearGlobals (the generator's part) */
    G.gameStart = 0;
    G.noDarkLevel = 1;
    G.probDarkLevel = 12;
    G.probSnakePit = 8;
    G.probCemetary = 10;
    G.probYetiLair = 6;
    G.probAlien = 10;
    G.probSacPit = 8;
    G.thiefLevel = 0;
    G.murderer = 0;
    G.kaliPunish = 0;
    G.madeUdjatEye = 0;
    G.genUdjatEye = 0;
    G.madeMarketEntrance = 0;
    G.genMarketEntrance = 0;
    G.marketChance = 160;
    G.madeBlackMarket = 0;
    G.genBlackMarket = 0;
    G.madeMoai = 0;
    G.cityOfGold = 0;
    G.hasSpectacles = 0;
    G.exitX = 0;
    G.exitY = 0;
    G.currLevel = 1;
    G.levelType = 0;
    G.pickupItemNone = 1;
    /* oGlobals / scrInit / oTitle Create, and the values the tracer's generator mode sets with them */
    G.customLevel = 0;
    G.graphicsHigh = 1;
    G.isDamsel = 0;
    G.isTunnelMan = 0;
    G.hadDarkLevel = 0;
    G.lake = 0;
    G.cemetary = 0;
    G.shop = 0;
    G.darkLevel = 0;
    G.cleanSolids = 0;
    G.snakePit = 0;
    G.blackMarket = 0;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 5; j++)
            G.roomPath[i][j] = 0;
}

/* ------------------------------------------------------------------------------------------------------------ */
/* scripts/scrLevelGen */
static void scrLevelGen(void)
{
    int roomX, roomY, prevX, prevY, n, d, i, j, k;

    G.startRoomX = 0;                                                         /* :28 */
    G.startRoomY = 0;
    roomX = G.startRoomX;
    roomY = 0;
    prevX = G.startRoomX;
    prevY = 0;
    G.roomPath[roomX][roomY] = 1;
    n = RAND(0, 3);                                                           /* :36 */

    G.sacrificePit = 0;
    G.snakePit = 0;
    G.alienCraft = 0;
    G.yetiLair = 0;
    G.blackMarket = 0;

    /* :47 black market, :74 sacrifice pit: levelType 1 / 3 only (the && stops before any rand) */

    while (roomY < 4) {                                                       /* :92 */
        d = 0;
        if (roomX == 0) n = RAND(3, 5);
        else if (roomX == 3) n = RAND(5, 7);
        else n = RAND(1, 5);

        if (n < 3 || n > 5) {                                                 /* :99 move left */
            if (roomX > 0) {
                if (G.roomPath[roomX - 1][roomY] == 0) roomX -= 1;
                else if (roomX < 3) {                                         /* GML dangling else */
                    if (G.roomPath[roomX + 1][roomY] == 0) roomX += 1;
                    else n = 5;
                }
            }
        } else if (n == 3 || n == 4) {                                        /* :107 move right */
            if (roomX < 3) {
                if (G.roomPath[roomX + 1][roomY] == 0) roomX += 1;
                else if (roomX > 0) {
                    if (G.roomPath[roomX - 1][roomY] == 0) roomX -= 1;
                    else n = 5;
                }
            }
        }

        if (n == 5) {                                                         /* :116 move down */
            roomY += 1;
            d = 1;
            if (roomY < 4) {
                G.roomPath[prevX][prevY] = 2;
                G.roomPath[roomX][roomY] = 3;
                d = 1;
            } else {
                G.endRoomX = (int16_t)roomX;
                G.endRoomY = (int16_t)(roomY - 1);
            }
        }

        if (!d) G.roomPath[roomX][roomY] = 1;
        prevX = roomX;
        prevY = roomY;
    }

    if (G.cityOfGold) G.roomPath[RAND(0, 3)][2] = 6;                         /* :139 */

    if (G.levelType == 0) {                                                   /* :145 snake pit */
        for (j = 0; j < 2; j += 1) {
            for (i = 0; i < 4; i += 1) {
                if (G.roomPath[i][j] == 0 && G.roomPath[i][j + 1] == 0 && G.roomPath[i][j + 2] == 0 &&
                    RAND(1, G.probSnakePit) == 1) {
                    G.roomPath[i][j] = 7;
                    if (G.roomPath[i][j + 2] == 0) {
                        G.roomPath[i][j + 1] = 8;
                        if (j == 0) {
                            if (G.roomPath[i][j + 3] == 0) {
                                G.roomPath[i][j + 2] = 8;
                                G.roomPath[i][j + 3] = 9;
                            } else {
                                G.roomPath[i][j + 2] = 9;
                            }
                        } else {
                            G.roomPath[i][j + 2] = 9;
                        }
                    }
                    G.snakePit = 1;
                    i = 99;
                    j = 99;
                    break;
                }
            }
        }
    }

    G.roomPath[0][4] = 0;                                                     /* :190 */
    G.roomPath[1][4] = 0;
    G.roomPath[2][4] = 0;
    G.roomPath[3][4] = 0;

    /* :197 lake, :216 moai / alien / yeti: other areas (no rand for levelType 0) */

    if (RAND(1, G.currLevel) <= 2 && G.currLevel > 1 && !G.madeBlackMarket) {  /* :243 shop */
        i = 0;
        for (k = 0; k < 4; k += 1) {
            for (j = 0; j < 4; j += 1) {
                G.roomPoss[j][k] = 0;
                if (G.roomPath[j][k] == 0) {
                    if (j < 3) {
                        if (G.roomPath[j + 1][k] == 1 || G.roomPath[j + 1][k] == 2) {
                            G.roomPoss[j][k] = 4;
                            i += 1;
                        }
                    } else if (j > 0) {
                        if (G.roomPath[j - 1][k] == 1 || G.roomPath[j - 1][k] == 2) {
                            G.roomPoss[j][k] = 5;
                            i += 1;
                        }
                    }
                }
            }
        }
        if (i > 0) {                                                          /* :280 */
            n = RAND(0, i - 1);
            for (k = 0; k < 4; k += 1) {
                for (j = 0; j < 4; j += 1) {
                    if (G.roomPoss[j][k] != 0) {
                        if (n == 0) {
                            G.roomPath[j][k] = G.roomPoss[j][k];
                            G.shop = 1;
                            j = 4;
                            k = 4;
                            break;
                        } else n -= 1;
                    }
                }
            }
        }
    }
}

/* ------------------------------------------------------------------------------------------------------------ */
/* scripts/scrGenerateItem: obj is the calling instance's variable `obj` */
void scrGenerateItem(int self, int x, int y, int setType, int *obj)
{
    (void)self;
    if (setType == 0) {                                                       /* :24 crate set */
        if (RAND(1, 500) == 1) *obj = instance_create(x, y, OBJ_oJetpack);
        else if (RAND(1, 200) == 1) *obj = instance_create(x, y, OBJ_oCapePickup);
        else if (RAND(1, 100) == 1) *obj = instance_create(x, y, OBJ_oShotgun);
        else if (RAND(1, 100) == 1) *obj = instance_create(x, y, OBJ_oMattock);
        else if (RAND(1, 100) == 1) *obj = instance_create(x, y, OBJ_oTeleporter);
        else if (RAND(1, 90) == 1) *obj = instance_create(x, y, OBJ_oGloves);
        else if (RAND(1, 90) == 1) *obj = instance_create(x, y, OBJ_oSpectacles);
        else if (RAND(1, 80) == 1) *obj = instance_create(x, y, OBJ_oWebCannon);
        else if (RAND(1, 80) == 1) *obj = instance_create(x, y, OBJ_oPistol);
        else if (RAND(1, 80) == 1) *obj = instance_create(x, y, OBJ_oMitt);
        else if (RAND(1, 60) == 1) *obj = instance_create(x, y, OBJ_oPaste);
        else if (RAND(1, 60) == 1) *obj = instance_create(x, y, OBJ_oSpringShoes);
        else if (RAND(1, 60) == 1) *obj = instance_create(x, y, OBJ_oSpikeShoes);
        else if (RAND(1, 60) == 1) *obj = instance_create(x, y, OBJ_oMachete);
        else if (RAND(1, 40) == 1) *obj = instance_create(x, y, OBJ_oBombBox);
        else if (RAND(1, 40) == 1) *obj = instance_create(x, y, OBJ_oBow);
        else if (RAND(1, 20) == 1) *obj = instance_create(x, y, OBJ_oCompass);
        else if (RAND(1, 10) == 1) *obj = instance_create(x, y, OBJ_oParaPickup);
        else *obj = instance_create(x, y, OBJ_oRopePile);
        W.in[*obj].cost = 0;
        W.in[*obj].flags &= (uint8_t)~IF_FORSALE;
    } else if (setType == 1) {                                                /* :48 high end set */
        if (RAND(1, 40) == 1) *obj = instance_create(x, y, OBJ_oJetpack);
        else if (RAND(1, 25) == 1) *obj = instance_create(x, y, OBJ_oCapePickup);
        else if (RAND(1, 20) == 1) *obj = instance_create(x, y, OBJ_oShotgun);
        else if (RAND(1, 10) == 1) *obj = instance_create(x, y, OBJ_oGloves);
        else if (RAND(1, 10) == 1) *obj = instance_create(x, y, OBJ_oTeleporter);
        else if (RAND(1, 8) == 1) *obj = instance_create(x, y, OBJ_oMattock);
        else if (RAND(1, 8) == 1) *obj = instance_create(x, y, OBJ_oPaste);
        else if (RAND(1, 8) == 1) *obj = instance_create(x, y, OBJ_oSpringShoes);
        else if (RAND(1, 8) == 1) *obj = instance_create(x, y, OBJ_oSpikeShoes);
        else if (RAND(1, 8) == 1) *obj = instance_create(x, y, OBJ_oCompass);
        else if (RAND(1, 8) == 1) *obj = instance_create(x, y, OBJ_oPistol);
        else if (RAND(1, 8) == 1) *obj = instance_create(x, y, OBJ_oMachete);
        else *obj = instance_create(x, y, OBJ_oBombBox);
    } else if (setType == 2) {                                                /* :64 underground set */
        switch (RAND(0, 18)) {
        case 0: *obj = instance_create(x, y - 2, OBJ_oJetpack); break;
        case 1: *obj = instance_create(x, y, OBJ_oCapePickup); break;
        case 2: *obj = instance_create(x, y, OBJ_oShotgun); break;
        case 3: *obj = instance_create(x, y, OBJ_oMattock); break;
        case 4: *obj = instance_create(x, y + 3, OBJ_oTeleporter); break;
        case 5: *obj = instance_create(x, y - 1, OBJ_oGloves); break;
        case 6: *obj = instance_create(x, y, OBJ_oSpectacles); break;
        case 7: *obj = instance_create(x - 2, y, OBJ_oWebCannon); break;
        case 8: *obj = instance_create(x, y, OBJ_oPistol); break;
        case 9: *obj = instance_create(x, y - 1, OBJ_oMitt); break;
        case 10: *obj = instance_create(x, y, OBJ_oPaste); break;
        case 11: *obj = instance_create(x, y, OBJ_oSpringShoes); break;
        case 12: *obj = instance_create(x, y, OBJ_oSpikeShoes); break;
        case 13: *obj = instance_create(x, y, OBJ_oMachete); break;
        case 14: *obj = instance_create(x, y - 2, OBJ_oBombBox); break;
        case 15: *obj = instance_create(x, y, OBJ_oBow); break;
        case 16: *obj = instance_create(x, y, OBJ_oCompass); break;
        case 17: *obj = instance_create(x, y, OBJ_oParaPickup); break;
        case 18: *obj = instance_create(x, y, OBJ_oRopePile); break;
        }
        W.in[*obj].cost = 0;
        W.in[*obj].flags &= (uint8_t)~IF_FORSALE;
    }
}

/* ------------------------------------------------------------------------------------------------------------ */
/* scripts/scrShopItemsGen, run by an oRoom (xpos, ypos, shopType, obj, m, n are its variables) */
void scrShopItemsGen(int room, int xpos, int ypos, int shopType, int *obj)
{
    int m, n;
    (void)room;
    if (shopType == SHOP_BOMB) {                                              /* :25 */
        for (;;) {
            if (RAND(1, 5) == 1) {
                if (!instance_exists(OBJ_oPaste)) { *obj = instance_create(xpos + 8, ypos + 10, OBJ_oPaste); break; }
            } else if (RAND(1, 4) == 1) { *obj = instance_create(xpos + 8, ypos + 8, OBJ_oBombBox); break; }
            else { *obj = instance_create(xpos + 8, ypos + 10, OBJ_oBombBag); break; }
        }
    } else if (shopType == SHOP_WEAPON) {                                     /* :37 */
        m = 20;
        for (;;) {
            n = RAND(1, 4);
            if (m <= 0) { *obj = instance_create(xpos + 8, ypos + 10, OBJ_oBombBag); break; }
            else if (RAND(1, 12) == 1) {
                if (!instance_exists(OBJ_oWebCannon)) { *obj = instance_create(xpos + 8, ypos + 12, OBJ_oWebCannon); break; }
            } else if (RAND(1, 10) == 1) {
                if (!instance_exists(OBJ_oShotgun)) { *obj = instance_create(xpos + 8, ypos + 12, OBJ_oShotgun); break; }
            } else if (RAND(1, 6) == 1) { *obj = instance_create(xpos + 8, ypos + 10, OBJ_oBombBox); break; }
            else if (n == 1) {
                if (!instance_exists(OBJ_oPistol)) { *obj = instance_create(xpos + 8, ypos + 12, OBJ_oPistol); break; }
            } else if (n == 2) {
                if (!instance_exists(OBJ_oMachete)) { *obj = instance_create(xpos + 8, ypos + 12, OBJ_oMachete); break; }
            } else if (n == 3) { *obj = instance_create(xpos + 8, ypos + 10, OBJ_oBombBag); break; }
            else if (n == 4) {
                if (!instance_exists(OBJ_oBow)) { *obj = instance_create(xpos + 8, ypos + 12, OBJ_oBow); break; }
            }
            m -= 1;
        }
    } else if (shopType == SHOP_CLOTHING) {                                   /* :69 */
        m = 20;
        for (;;) {
            n = RAND(1, 6);
            if (RAND(1, m) == 1) { *obj = instance_create(xpos + 8, ypos + 11, OBJ_oRopePile); break; }
            else if (n == 1) {
                if (!instance_exists(OBJ_oSpringShoes)) { *obj = instance_create(xpos + 8, ypos + 10, OBJ_oSpringShoes); break; }
            } else if (n == 2) {
                if (!instance_exists(OBJ_oSpectacles)) { *obj = instance_create(xpos + 8, ypos + 10, OBJ_oSpectacles); break; }
            } else if (n == 3) {
                if (!instance_exists(OBJ_oGloves)) { *obj = instance_create(xpos + 8, ypos + 8, OBJ_oGloves); break; }
            } else if (n == 4) {
                if (!instance_exists(OBJ_oMitt)) { *obj = instance_create(xpos + 8, ypos + 8, OBJ_oMitt); break; }
            } else if (n == 5) {
                if (!instance_exists(OBJ_oCapePickup)) { *obj = instance_create(xpos + 8, ypos + 10, OBJ_oCapePickup); break; }
            } else if (n == 6) {
                if (!instance_exists(OBJ_oSpikeShoes)) { *obj = instance_create(xpos + 8, ypos + 10, OBJ_oSpikeShoes); break; }
            }
            m -= 1;
        }
    } else if (shopType == SHOP_RARE) {                                       /* :103 */
        m = 20;
        for (;;) {
            n = RAND(1, 11);
            if (RAND(1, m) == 1) { *obj = instance_create(xpos + 8, ypos + 8, OBJ_oBombBox); break; }
            else if (n == 1) {
                if (!instance_exists(OBJ_oSpringShoes)) { *obj = instance_create(xpos + 8, ypos + 10, OBJ_oSpringShoes); break; }
            } else if (n == 2) {
                if (!instance_exists(OBJ_oCompass)) { *obj = instance_create(xpos + 8, ypos + 10, OBJ_oCompass); break; }
            } else if (n == 3) {
                if (!instance_exists(OBJ_oMattock)) { *obj = instance_create(xpos + 8, ypos + 10, OBJ_oMattock); break; }
            } else if (n == 4) {
                if (!instance_exists(OBJ_oSpectacles)) { *obj = instance_create(xpos + 8, ypos + 10, OBJ_oSpectacles); break; }
            } else if (n == 5) {
                if (!instance_exists(OBJ_oJetpack)) { *obj = instance_create(xpos + 8, ypos + 8, OBJ_oJetpack); break; }
            } else if (n == 6) {
                if (!instance_exists(OBJ_oGloves)) { *obj = instance_create(xpos + 8, ypos + 8, OBJ_oGloves); break; }
            } else if (n == 7) {
                if (!instance_exists(OBJ_oMitt)) { *obj = instance_create(xpos + 8, ypos + 8, OBJ_oMitt); break; }
            } else if (n == 8) {
                if (!instance_exists(OBJ_oWebCannon)) { *obj = instance_create(xpos + 8, ypos + 12, OBJ_oWebCannon); break; }
            } else if (n == 9) {
                if (!instance_exists(OBJ_oCapePickup)) { *obj = instance_create(xpos + 8, ypos + 10, OBJ_oCapePickup); break; }
            } else if (n == 10) {
                if (!instance_exists(OBJ_oTeleporter)) { *obj = instance_create(xpos + 8, ypos + 12, OBJ_oTeleporter); break; }
            } else if (n == 11) {
                if (!instance_exists(OBJ_oSpikeShoes)) { *obj = instance_create(xpos + 8, ypos + 10, OBJ_oSpikeShoes); break; }
            }
            m -= 1;
        }
    } else {                                                                  /* :157 general (and others) */
        for (;;) {
            n = RAND(1, 3);
            if (RAND(1, 20) == 1) {
                if (!instance_exists(OBJ_oMattock)) { *obj = instance_create(xpos + 8, ypos + 10, OBJ_oMattock); break; }
            } else if (RAND(1, 10) == 1) {
                if (!instance_exists(OBJ_oGloves)) { *obj = instance_create(xpos + 8, ypos + 8, OBJ_oGloves); break; }
            } else if (RAND(1, 10) == 1) {
                if (!instance_exists(OBJ_oCompass)) { *obj = instance_create(xpos + 8, ypos + 10, OBJ_oCompass); break; }
            } else if (n == 1) { *obj = instance_create(xpos + 8, ypos + 10, OBJ_oBombBag); break; }
            else if (n == 2) { *obj = instance_create(xpos + 8, ypos + 11, OBJ_oRopePile); break; }
            else if (n == 3) { *obj = instance_create(xpos + 8, ypos + 10, OBJ_oParaPickup); break; }
        }
    }

    if (*obj != INST_NONE) {                                                  /* :180 */
        struct inst *p = &W.in[*obj];
        p->flags |= IF_FORSALE;
        if (G.currLevel > 2)
            p->cost += (p->cost / 100) * 10 * (G.currLevel - 2);              /* :187, costs are multiples of 100 */
    }
}

/* ------------------------------------------------------------------------------------------------------------ */
/* scripts/scrTreasureGen, run by an oSolid (self); returns as the GML `return 0` */
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
            W.in[obj].flags &= (uint8_t)~IF_FORSALE;
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

/* ------------------------------------------------------------------------------------------------------------ */
/* scripts/scrEntityGen, levelType 0 */
static void scrEntityGen(void)
{
    int n;
    G.LockedChest = 0;                                                        /* :27 */
    G.Key = 0;
    G.lockedChestChance = 8;

    G.giantSpider = 0;                                                        /* :33 */
    G.genGiantSpider = 0;
    if (RAND(1, 6) == 1) G.genGiantSpider = 1;
    WITH_BEGIN(s, OBJ_oSolid)                                                 /* :36 */
        int x = W.in[s].x, y = W.in[s].y, o = W.in[s].obj;
        if (!isInShop(x, y) && y > 16) {
            if (o != OBJ_oSacAltarLeft && o != OBJ_oSacAltarRight)            /* type != "Altar" */
                scrTreasureGen(s, 0);
            if (scrGetRoomX(x) != G.startRoomX || scrGetRoomY(y - 16) != G.startRoomY) {   /* :46 */
                if (y < ROOM_H - 64 && NOTSOL(x, y + 16) && NOTSOL(x, y + 32) && !CP(x, y + 16, OBJ_oWater) &&
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
                if (NOTSOL(x, y - 16)) {                                      /* :68 */
                    if (RAND(1, 60) == 1) instance_create(x, y - 16, OBJ_oSnake);
                    else if (RAND(1, 800) == 1) instance_create(x, y - 16, OBJ_oCaveman);
                }
            }
        }
    WITH_END

    if (G.genUdjatEye && !G.LockedChest) {                                    /* :78 force chest */
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

    if (instance_exists(OBJ_oLockedChest)) {                                  /* :110 key */
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
        if (!G.Key) {                                                         /* :128 */
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

    if (G.Key) G.madeUdjatEye = 1;                                            /* :144 */

    WITH_BEGIN(b, OBJ_oBlock)                                                 /* :146 arrow traps */
        int x = W.in[b].x, y = W.in[b].y, ent = instance_first(OBJ_oEntrance);
        if (!isInShop(x, y)) {
            int32_t d2 = dist2(x, y, W.in[ent].x, W.in[ent].y);              /* n = point_distance(..) */
            if (!isInShop(x, y) && RAND(1, 4) == 1 && !(y == W.in[ent].y && d2 < 144 * 144) && d2 > 48 * 48) {
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
    WITH_END

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
/* scripts/scrSetupWalls(bottom), the parts with instances present in the mines (oBrick, oBlock; oLush, oDark,
   oIce, oTemple, oTempleFake, oWater, oLava, oVine, oTree, oTreeBranch loops find no instance there) */
static void scrSetupWalls(int bottom)
{
    WITH_BEGIN(b, OBJ_oBrick)                                                 /* :25 */
        int x = W.in[b].x, y = W.in[b].y, up = 0, down = 0;
        if (y == 0 || CP(x, y - 16, OBJ_oBrick) || CP(x, y - 16, OBJ_oHardBlock)) up = 1;
        if (y >= bottom || CP(x, y + 16, OBJ_oBrick) || CP(x, y + 16, OBJ_oHardBlock)) down = 1;
        /* left / right (:34-35) are computed but not used */
        if (!up) {
            inst_set_sprite(b, GSPR_sCaveUp);
            if (G.graphicsHigh) {
                if (RAND(1, 3) < 3) tile_add(BG_bgCaveTop, 0, 0, 16, 16, x, y - 16, 3);
                else tile_add(BG_bgCaveTop, 16, 0, 16, 16, x, y - 16, 3);
            }
        }
        if (!down) {
            if (!up) inst_set_sprite(b, GSPR_sCaveUp2);
            else inst_set_sprite(b, GSPR_sBrickDown);
        }
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
}

/* ------------------------------------------------------------------------------------------------------------ */
/* scripts/scrInitLevel */
static int scrInitLevel(void)
{
    int i, j, k, obj;

    G.levelType = 0;                                                          /* :25 */
    if (G.currLevel > 4 && G.currLevel < 9) G.levelType = 1;
    if (G.currLevel > 8 && G.currLevel < 13) G.levelType = 2;
    if (G.currLevel > 12 && G.currLevel < 16) G.levelType = 3;
    if (G.currLevel == 16) G.levelType = 4;
    if (G.levelType != 0 || G.lake)
        return -1;                                                            /* not translated yet */

    if (G.currLevel <= 1 || G.currLevel == 5 || G.currLevel == 9 || G.currLevel == 13)
        G.hadDarkLevel = 0;

    G.startRoomX = G.startRoomY = G.endRoomX = G.endRoomY = 0;                /* :52 */
    GAME.levelGen = 0;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            G.roomPath[i][j] = 0;

    /* :67-129 side walls (mines: k = 33) */
    k = 33;
    for (i = 0; i <= 42; i += 1) {
        for (j = 0; j <= k; j += 1) {
            if (i * 16 == 0 || i * 16 == 656 || j * 16 == 0 || j * 16 >= 528) {
                obj = instance_create(i * 16, j * 16, OBJ_oBrick);
                inst_set_sprite(obj, GSPR_sBrick);
                W.in[obj].flags |= IF_INVINCIBLE;
            }
        }
    }

    scrLevelGen();                                                            /* :146 */

    G.cemetary = 0;                                                           /* :148 (levelType 1 only) */

    WITH_BEGIN(r, OBJ_oRoom)                                                  /* :151 */
        scrRoomGen(r);
    WITH_END

    G.darkLevel = 0;                                                          /* :168 */
    if (!G.hadDarkLevel && !G.noDarkLevel && G.currLevel != 0 && G.currLevel != 1 && G.levelType != 2 &&
        G.currLevel != 16 && RAND(1, G.probDarkLevel) == 1) {
        G.darkLevel = 1;
        G.hadDarkLevel = 1;
    }
    if (G.blackMarket) G.darkLevel = 0;

    G.genUdjatEye = 0;                                                        /* :179 */
    if (!G.madeUdjatEye) {
        if (G.currLevel == 2 && RAND(1, 3) == 1) G.genUdjatEye = 1;
        else if (G.currLevel == 3 && RAND(1, 2) == 1) G.genUdjatEye = 1;
        else if (G.currLevel == 4) G.genUdjatEye = 1;
    }

    G.genMarketEntrance = 0;                                                  /* :187 (levels 5-7 only) */
    if (!G.madeMarketEntrance) {
        /* currLevel 5..7 draw; none in the mines */
    }

    scrEntityGen();                                                           /* :199 */

    if (instance_exists(OBJ_oEntrance) && !G.customLevel && instance_exists(OBJ_oPlayer1)) {   /* :201 */
        int p = instance_first(OBJ_oPlayer1), e = instance_first(OBJ_oEntrance);
        W.in[p].x = (int16_t)(W.in[e].x + 8);
        W.in[p].y = (int16_t)(W.in[e].y + 8);
    }

    if (G.darkLevel || G.blackMarket || G.snakePit || G.cemetary || G.lake || G.yetiLair || G.alienCraft ||
        G.sacrificePit || G.cityOfGold) {                                     /* :207 */
        WITH_BEGIN(p, OBJ_oPlayer1)
            W.in[p].alarm[0] = 10;
        WITH_END
    }

    scrSetupWalls(528);                                                       /* :225 */

    if (G.graphicsHigh) {                                                     /* :228 background details */
        for (k = 0; k < 20; k++) {
            /* tile_add(bgExtras, 32*rand(0,1), 0, 32, 32, 16*rand(1,42), 16*rand(1,33), 10002): last
               argument first */
            int ty = 16 * RAND(1, 33), tx = 16 * RAND(1, 42), tl = 32 * RAND(0, 1);
            tile_add(BG_bgExtras, tl, 0, 32, 32, tx, ty, 10002);
        }
    }

    GAME.levelGen = 1;                                                        /* :244 */

    if (G.murderer || G.thiefLevel > 0) {                                     /* :247 */
        WITH_BEGIN(e, OBJ_oExit)
            obj = instance_create(W.in[e].x, W.in[e].y, OBJ_oShopkeeper);
            W.in[obj].status = 4;
        WITH_END
    }

    WITH_BEGIN(t, OBJ_oTreasure)                                              /* :260 */
        if (CP(W.in[t].x, W.in[t].y, OBJ_oSolid)) {
            int s = instance_place(t, W.in[t].x, W.in[t].y, OBJ_oSolid);
            if (W.in[s].flags & IF_INVINCIBLE) instance_destroy(t);
        }
    WITH_END
    /* :269 with oWater: none in the mines */
    return 0;
}

/* ------------------------------------------------------------------------------------------------------------ */
int gen_level(int32_t next_id)
{
    int i;
    inst_reset(next_id);
    gntiles = 0;
    /* rLevel instance creation order (rooms/rLevel/rLevel.yy instanceCreationOrder) and Create events */
    inst_add(OBJ_oPlayer1, 24, 24, rlevel_ids[0]);
    {
        /* oPlayer1 Create (no RNG, no instances) */
        int p = W.n - 1;
        W.in[p].facing = 19;
        if (G.isDamsel) inst_set_sprite(p, GSPR_sDamselLeft);
        else if (G.isTunnelMan) inst_set_sprite(p, GSPR_sTunnelLeft);
    }
    for (i = 0; i < 16; i++)
        inst_add(OBJ_oRoom, 16 + 160 * (i & 3), 16 + 128 * (i >> 2), rlevel_ids[1 + i]);
    /* oLevel Create: global.gameStart = true; scrHoldItem(global.pickupItem) and the Kali ball and chain only
       when an item is carried / kaliPunish >= 2 (not modelled: they stop here) */
    inst_add(OBJ_oLevel, 32, 0, rlevel_ids[17]);
    G.gameStart = 1;
    if (!G.pickupItemNone || G.kaliPunish >= 2)
        return -1;
    /* oGame Create (objects/oGame/Create_0.gml) */
    inst_add(OBJ_oGame, 0, 0, rlevel_ids[18]);
    GAME.damsel = GAME.idol = GAME.altar = 0;                                 /* :9-12 */
    if (G.gameStart)
        return scrInitLevel();                                                /* :30 */
    return 0;
}
