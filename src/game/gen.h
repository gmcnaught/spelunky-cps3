/* Level generator: Spelunky Classic HD 1.2.2's oGame Create -> scrInitLevel (scrLevelGen, scrRoomGen per oRoom,
 * scrEntityGen, scrTreasureGen, scrSetupWalls, scrGenerateItem, scrShopItemsGen and the Create / Destroy events
 * of every object they create), translated statement for statement (GML file:line in comments). Mines only
 * (levelType 0, levels 1-4) so far; other areas stop at gen_level() with an error code.
 *
 * GML evaluation rules used (HD's runtime, VM bytecode 17; checked in the bytecode, build/p2/disasm.txt):
 * `and` / `or` short-circuit; function arguments are evaluated right to left (last argument first), operands of
 * one expression left to right; `with` and instance searches as inst.h.
 */
#ifndef GEN_H
#define GEN_H
#include <stdint.h>
#include "inst.h"

/* the globals (global.*) the generator reads or writes */
struct gglobals {
    int16_t currLevel, levelType;
    uint8_t gameStart, customLevel, graphicsHigh, isDamsel, isTunnelMan, hasSpectacles;
    uint8_t hadDarkLevel, darkLevel, noDarkLevel, lake, cityOfGold, cemetary;
    uint8_t blackMarket, madeBlackMarket, genBlackMarket, sacrificePit, snakePit, alienCraft, yetiLair;
    uint8_t madeMoai, shop, madeUdjatEye, genUdjatEye, madeMarketEntrance, genMarketEntrance;
    uint8_t giantSpider, genGiantSpider, LockedChest, Key, cleanSolids, murderer;
    int16_t thiefLevel, kaliPunish;
    int16_t probDarkLevel, probSnakePit, probCemetary, probSacPit, probAlien, probYetiLair;
    int16_t lockedChestChance, marketChance;
    int16_t startRoomX, startRoomY, endRoomX, endRoomY, exitX, exitY;
    int8_t roomPath[4][5];      /* global.roomPath[x, y] */
    int8_t roomPoss[4][4];
    uint8_t pickupItemNone;     /* global.pickupItem == "" (no held item carried into the level) */
};

/* oGame's instance variables used by generation */
struct ggame { uint8_t damsel, idol, altar, levelGen; };

/* background tiles added by tile_add / tile_add2 (not instances; kept for the display) */
struct gtile { int16_t bg, left, top, w, h, x, y; int16_t depth; };
#define GTILES_MAX 1024

extern struct gglobals G;
extern struct ggame GAME;
extern struct gtile gtiles[GTILES_MAX];
extern int gntiles;

/* tile_add background ids (not sprites) */
enum gbg { BG_bgExtras, BG_bgCaveTop, BG_bgKaliBody, BG_bgTiki, BG_bgTikiArms, BG_bgDiceSign, BG_bgWanted };

/* scrClearGlobals() plus the other new-game values (oGlobals / scrInit / oTitle Create) the generator reads */
void gen_new_game(void);

/* rLevel: the room's instances in creation order (oPlayer1, 16 oRoom, oLevel, oGame: their Create events, oGame's
   ending in scrInitLevel when global.gameStart). next_id: the runtime's instance id counter at room start.
   The RNG (g_rng) is used as it stands. Returns 0, or -1 for an area not translated yet. */
int gen_level(int32_t next_id);

/* object Create / Destroy events (genobj.c) */
int instance_create(int x, int y, int obj);
void instance_destroy(int i);

/* scripts (gen.c, genroom.c) */
void scrRoomGen(int room);
void scrGenerateItem(int self, int x, int y, int setType, int *obj);
void scrShopItemsGen(int room, int xpos, int ypos, int shopType, int *obj);
int scrGetRoomX(int x);
int scrGetRoomY(int y);
int isInShop(int x, int y);
void tile_add(int bg, int left, int top, int w, int h, int x, int y, int depth);

#endif
