/* The level room's creation, oLevel / oGame Create, scrInitLevel, scrLevelGen, scrGenerateItem, scrShopItemsGen.
 * GML: refs/hd/src/scripts/<name>/<name>.gml and objects/<obj>/<event>.gml at tag 1.2.2; line numbers in comments.
 */
#include "gen.h"
#include "rng.h"

struct gglobals G;
struct ggame GAME;
struct gtile gtiles[GTILES_MAX];
int gntiles;

const char *const pickup_names[PICK_COUNT] = {
    "", "Rock", "Jar", "Skull", "Fish Bone", "Arrow", "Machete", "Mattock", "Mattock Head", "Pistol",
    "Web Cannon", "Teleporter", "Shotgun", "Bow", "Flare", "Sceptre", "Key", "(other)",
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

void gen_new_game(void)
{
    /* scripts/scrClearGlobals (the generator's part) */
    G.gameStart = 0;
    G.pickupItem = PICK_NONE;
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
    G.favor = 0;                                                              /* :66 */
    G.kaliGift = 0;
    G.madeUdjatEye = 0;
    G.genUdjatEye = 0;
    G.madeMarketEntrance = 0;
    G.genMarketEntrance = 0;
    G.marketChance = 160;
    G.madeBlackMarket = 0;
    G.genBlackMarket = 0;
    G.madeMoai = 0;
    G.madeGoldEntrance = 0;
    G.genGoldEntrance = 0;
    G.goldChance = 160;
    G.cityOfGold = 0;
    G.hasSpectacles = 0;
    G.checkWater = 0;
    G.exitX = 0;
    G.exitY = 0;
    G.currLevel = 1;
    G.levelType = 0;
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
}

int gen_room_force = -1;

int gen_room_for_level(void)
{
    if (gen_room_force >= 0) return gen_room_force;
    /* objects/oTransition/Step_0.gml :14-27 (the lake is drawn there: here it is global.lake) */
    if (G.currLevel == 16) return 3;
    if (G.currLevel >= 9 && G.currLevel <= 12) return 1;
    if (G.lake) return 2;
    return 0;
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

    if (G.levelType == 1 && !G.madeBlackMarket && G.genBlackMarket) {        /* :47 black market */
        static const int8_t bm[4][4] = { { 2, 2, 2, 3 }, { 4, 4, 4, 1 }, { 4, 4, 5, 1 }, { 2, 2, 4, 3 } };
        G.blackMarket = 1;
        G.startRoomX = 0;
        G.startRoomY = 0;
        G.endRoomX = 3;
        G.endRoomY = 3;
        for (i = 0; i < 4; i++)
            for (j = 0; j < 4; j++)
                G.roomPath[i][j] = bm[i][j];
        G.madeBlackMarket = 1;
        return;
    }

    if (G.levelType == 3 && RAND(1, G.probSacPit) == 1) {                     /* :74 sacrifice pit */
        while (n == roomX) n = RAND(0, 3);
        for (i = 0; i < 4; i += 1) {
            if (i == 0) G.roomPath[n][i] = 7;
            else if (i == 3) G.roomPath[n][i] = 9;
            else G.roomPath[n][i] = 8;
        }
        G.sacrificePit = 1;
        GAME.idol = 1;
        GAME.damsel = 1;
    }

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

    if (G.lake) {                                                             /* :197 lake */
        for (i = 0; i < 4; i++) {
            G.roomPath[i][3] = 8;
            G.roomPath[i][4] = 7;
        }
        n = RAND(0, 3);
        while (n == G.endRoomX) n = RAND(0, 3);
        G.roomPath[n][4] = 9;
    }

    if (!G.madeMoai && G.levelType == 2) {                                    /* :216 moai */
        if (G.currLevel == 9 && RAND(1, 4) == 1) G.madeMoai = 1;
        else if (G.currLevel == 10 && RAND(1, 3) == 1) G.madeMoai = 1;
        else if (G.currLevel == 11 && RAND(1, 2) == 1) G.madeMoai = 1;
        else if (G.currLevel == 12) G.madeMoai = 1;
        if (G.madeMoai) {
            int a = RAND(0, 3);                                               /* first index first (bytecode) */
            int b = RAND(1, 2);
            G.roomPath[a][b] = 6;
        }
    } else if (G.levelType == 2 && RAND(1, G.probAlien) == 1) {               /* :225 alien craft */
        k = RAND(0, 2);
        j = RAND(1, 2);
        for (i = k; i < 4; i += 1) {
            if (i == k) G.roomPath[i][j] = 7;
            else if (i == 3) G.roomPath[i][j] = 9;
            else G.roomPath[i][j] = 8;
        }
        G.alienCraft = 1;
    } else if (G.levelType == 2 && !G.alienCraft && RAND(1, G.probYetiLair) == 1) {   /* :237 yeti */
        G.yetiLair = 1;
    }

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
        W.in[*obj].flags &= (uint16_t)~IF_FORSALE;
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
        W.in[*obj].flags &= (uint16_t)~IF_FORSALE;
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
/* scripts/scrInitLevel */
static int scrInitLevel(void)
{
    int i, j, k, obj, bottom;

    G.levelType = 0;                                                          /* :25 */
    if (G.currLevel > 4 && G.currLevel < 9) G.levelType = 1;
    if (G.currLevel > 8 && G.currLevel < 13) G.levelType = 2;
    if (G.currLevel > 12 && G.currLevel < 16) G.levelType = 3;
    if (G.currLevel == 16) G.levelType = 4;

    if (G.currLevel <= 1 || G.currLevel == 5 || G.currLevel == 9 || G.currLevel == 13)
        G.hadDarkLevel = 0;

    G.startRoomX = G.startRoomY = G.endRoomX = G.endRoomY = 0;                /* :52 */
    GAME.levelGen = 0;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            G.roomPath[i][j] = 0;

    /* :67-129 side walls */
    if (G.levelType == 4) k = 54;
    else if (G.levelType == 2) k = 38;
    else if (G.lake) k = 41;
    else k = 33;
    for (i = 0; i <= 42; i += 1) {
        for (j = 0; j <= k; j += 1) {
            if (G.levelType == 2) {                                           /* :85 */
                if (i * 16 == 0 || i * 16 == 656 || j * 16 == 0) {
                    obj = instance_create(i * 16, j * 16, OBJ_oDark);
                    W.in[obj].flags |= IF_INVINCIBLE;
                    inst_set_sprite(obj, GSPR_sDark);
                }
            } else if (G.levelType == 4) {                                    /* :96 */
                if (i * 16 == 0 || i * 16 == 656 || j * 16 == 0) {
                    obj = instance_create(i * 16, j * 16, OBJ_oTemple);
                    W.in[obj].flags |= IF_INVINCIBLE;
                    if (!G.cityOfGold) inst_set_sprite(obj, GSPR_sTemple);
                }
            } else if (G.lake) {                                              /* :107 */
                if (i * 16 == 0 || i * 16 == 656 || j * 16 == 0 || j * 16 >= 656) {
                    obj = instance_create(i * 16, j * 16, OBJ_oLush);
                    inst_set_sprite(obj, GSPR_sLush);
                    W.in[obj].flags |= IF_INVINCIBLE;
                }
            } else if (i * 16 == 0 || i * 16 == 656 || j * 16 == 0 || j * 16 >= 528) {   /* :118 */
                if (G.levelType == 0) { obj = instance_create(i * 16, j * 16, OBJ_oBrick); inst_set_sprite(obj, GSPR_sBrick); }
                else if (G.levelType == 1) { obj = instance_create(i * 16, j * 16, OBJ_oLush); inst_set_sprite(obj, GSPR_sLush); }
                else { obj = instance_create(i * 16, j * 16, OBJ_oTemple); if (!G.cityOfGold) inst_set_sprite(obj, GSPR_sTemple); }
                W.in[obj].flags |= IF_INVINCIBLE;
            }
        }
    }

    if (G.levelType == 2) {                                                   /* :131 */
        for (i = 0; i <= 42; i += 1)
            instance_create(i * 16, 40 * 16, OBJ_oDark);
    }

    scrLevelGen();                                                            /* :146 */

    G.cemetary = 0;                                                           /* :148 */
    if (G.levelType == 1 && RAND(1, G.probCemetary) == 1) G.cemetary = 1;

    WITH_BEGIN(r, OBJ_oRoom)                                                  /* :151 */
        if (G.levelType == 0) scrRoomGen(r);
        else if (G.levelType == 1) {
            if (G.blackMarket) scrRoomGenMarket(r);
            else scrRoomGen2(r);
        } else if (G.levelType == 2) {
            if (G.yetiLair) scrRoomGenYeti(r);
            else scrRoomGen3(r);
        } else if (G.levelType == 3) scrRoomGen4(r);
        else scrRoomGen5(r);
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

    G.genMarketEntrance = 0;                                                  /* :187 */
    if (!G.madeMarketEntrance) {
        if (G.currLevel == 5 && RAND(1, 3) == 1) G.genMarketEntrance = 1;
        else if (G.currLevel == 6 && RAND(1, 2) == 1) G.genMarketEntrance = 1;
        else if (G.currLevel == 7) G.genMarketEntrance = 1;
    }

    scrEntityGen();                                                           /* :199 */

    if (instance_exists(OBJ_oEntrance) && !G.customLevel && instance_exists(OBJ_oPlayer1)) {   /* :201 */
        int p = instance_first(OBJ_oPlayer1), e = instance_first(OBJ_oEntrance);
        W.in[p].x = (int16_t)(W.in[e].x + 8);
        W.in[p].y = (int16_t)(W.in[e].y + 8);
        inst_moved(p);
        if (inst_hook) inst_hook(IH_MOVE, p, 0, 0, 0);                        /* the collision tree (pcol.c) */
    }

    if (G.darkLevel || G.blackMarket || G.snakePit || G.cemetary || G.lake || G.yetiLair || G.alienCraft ||
        G.sacrificePit || G.cityOfGold) {                                     /* :207 */
        WITH_BEGIN(p, OBJ_oPlayer1)
            W.in[p].alarm[0] = 10;
        WITH_END
    }

    if (G.levelType == 4) bottom = 864;                                       /* :223 */
    else if (G.lake) bottom = 656;
    else bottom = 528;
    scrSetupWalls(bottom);

    if (G.graphicsHigh) {                                                     /* :228 background details */
        for (k = 0; k < 20; k++) {
            /* tile_add(bg, 32*rand(0,1), 0, 32, 32, 16*rand(1,42), 16*rand(1,33), 10002): last argument first */
            int bg = GSPR_bgExtras, ty, tx, tl;
            if (G.levelType == 1 && RAND(1, 3) < 3) bg = GSPR_bgExtrasLush;
            else if (G.levelType == 2 && RAND(1, 3) < 3) bg = GSPR_bgExtrasIce;
            else if (G.levelType == 3 && RAND(1, 3) < 3) bg = GSPR_bgExtrasTemple;
            ty = 16 * RAND(1, 33);
            tx = 16 * RAND(1, 42);
            tl = 32 * RAND(0, 1);
            tile_add(bg, tl, 0, 32, 32, tx, ty, 10002);
        }
    }

    GAME.levelGen = 1;                                                        /* :244 */

    if (G.murderer || G.thiefLevel > 0) {                                     /* :247 */
        WITH_BEGIN(e, OBJ_oExit)
            if (W.in[e].etype == EX_EXIT) {
                obj = instance_create(W.in[e].x, W.in[e].y, OBJ_oShopkeeper);
                W.in[obj].status = 4;
            }
        WITH_END
    }

    WITH_BEGIN(t, OBJ_oTreasure)                                              /* :260 */
        if (collision_point(W.in[t].x, W.in[t].y, OBJ_oSolid) != INST_NONE) {
            int s = instance_place(t, W.in[t].x, W.in[t].y, OBJ_oSolid);
            if (s == INST_NONE) UNTRANSLATED(4001);                           /* GML: noone.invincible, an error */
            else if (W.in[s].flags & IF_INVINCIBLE) instance_destroy(t);
        }
    WITH_END

    WITH_BEGIN(w, OBJ_oWater)                                                 /* :269 */
        if (W.in[w].spr == GSPR_sWaterTop || W.in[w].spr == GSPR_sLavaTop)
            scrCheckWaterTop(w);
    WITH_END
    return 0;
}

/* ------------------------------------------------------------------------------------------------------------ */
/* scripts/scrHoldItem(global.pickupItem), run by oPlayer1 from oLevel Create */
static void scrHoldItem(int self)
{
    static const int16_t objs[PICK_COUNT] = {
        -1, OBJ_oRock, OBJ_oJar, OBJ_oSkull, OBJ_oFishBone, OBJ_oArrow, OBJ_oMachete, OBJ_oMattock,
        OBJ_oMattockHead, OBJ_oPistol, OBJ_oWebCannon, OBJ_oTeleporter, OBJ_oShotgun, OBJ_oBow, OBJ_oFlare,
        OBJ_oSceptre, OBJ_oKey, -1,
    };
    int it = G.pickupItem, h;
    if (it == PICK_NONE || objs[it] < 0)
        return;                                                               /* holdItem = 0 */
    h = instance_create(W.in[self].x, W.in[self].y, objs[it]);
    W.in[h].flags |= IF_HELD;                                                 /* holdItem.held = true */
    W.in[h].cost = 0;
    W.in[h].flags &= (uint16_t)~IF_NEW;
    /* oLevel Create :32: holdItem.cost = 0 again */
}

/* ------------------------------------------------------------------------------------------------------------ */
int gen_level(int32_t next_id)
{
    static const struct groom *const rooms[4] = { &groom_rLevel, &groom_rLevel2, &groom_rLevel3, &groom_rOlmec };
    const struct groom *rm = rooms[gen_room_for_level()];
    int k;
    inst_reset(next_id);
    gntiles = 0;
    gen_untranslated = 0;
    G.roomW = rm->w;
    G.roomH = rm->h;
    /* the room's instances in creation order (rooms/<room>/<room>.yy instanceCreationOrder): the runner adds
       them all, then runs their Create events in that order (Observed: instances created by a room instance's
       Create come after every room instance in the runner's list) */
    for (k = 0; k < rm->n; k++)                                               /* all exist before any Create */
        W.in[inst_add(rm->in[k].obj, rm->in[k].x, rm->in[k].y, rm->in[k].id)].depth = rm->in[k].depth;
    for (k = 0; k < rm->n; k++) {
        const struct groominst *ri = &rm->in[k];
        if (ri->obj == OBJ_oLevel) {
            /* objects/oLevel/Create_0.gml */
            G.gameStart = 1;                                                  /* :3 */
            WITH_BEGIN(p, OBJ_oPlayer1)                                       /* :26 */
                scrHoldItem(p);
                if (G.kaliPunish >= 2) {
                    int px = W.in[p].x, py = W.in[p].y, c;
                    instance_create(px, py, OBJ_oBall);
                    for (c = 1; c <= 4; c++) {
                        int o = instance_create(px, py, OBJ_oChain);
                        W.in[o].linkval = (int8_t)c;
                    }
                }
            WITH_END
        } else if (ri->obj == OBJ_oGame) {
            /* objects/oGame/Create_0.gml */
            GAME.damsel = GAME.idol = GAME.altar = 0;                         /* :9-12 */
            GAME.genClothingShop = GAME.genBombShop = GAME.genSupplyShop = GAME.genRareShop = GAME.genWeaponShop = 0;
            if (instance_number(OBJ_oGame) > 1) UNTRANSLATED(4002);           /* :24 */
            if (G.gameStart && scrInitLevel() != 0)                           /* :30 */
                return -1;
        } else {
            gen_create_event(k);
        }
    }
    return gen_untranslated ? -1 : 0;
}
