/* Level generator: Spelunky Classic HD 1.2.2's level rooms (rLevel, rLevel2, rLevel3, rOlmec): the room's instances
 * in creation order with their Create events, oLevel Create (held item, Kali ball and chain), then oGame Create ->
 * scrInitLevel (scrLevelGen, scrRoomGen* per oRoom, scrEntityGen, scrTreasureGen, scrSetupWalls, scrGenerateItem,
 * scrShopItemsGen and the Create / Destroy events of every object they create or destroy), translated statement
 * for statement (GML file:line in comments). All areas: mines 1-4, lush 5-8 (black market, cemetery, lake), ice
 * 9-12 (yeti lair, alien craft, moai), temple 13-15 (sacrifice pit, city of gold), the Olmec level 16.
 *
 * GML evaluation rules used (HD's runtime, VM bytecode 17; checked in the bytecode, build/p2/disasm.txt):
 * `and` / `or` short-circuit; `or` binds looser than `and`; function arguments are evaluated right to left (last
 * argument first), operands of one expression left to right; `with` and instance searches as inst.h.
 */
#ifndef GEN_H
#define GEN_H
#include <stdint.h>
#include "inst.h"

/* global.pickupItem: the item carried into the level (scrHoldItem's types; PICK_OTHER: a type it does not
   create, e.g. a gold idol) */
enum pickup { PICK_NONE, PICK_ROCK, PICK_JAR, PICK_SKULL, PICK_FISHBONE, PICK_ARROW, PICK_MACHETE, PICK_MATTOCK,
              PICK_MATTOCKHEAD, PICK_PISTOL, PICK_WEBCANNON, PICK_TELEPORTER, PICK_SHOTGUN, PICK_BOW, PICK_FLARE,
              PICK_SCEPTRE, PICK_KEY, PICK_OTHER, PICK_COUNT };
extern const char *const pickup_names[PICK_COUNT];

/* the globals (global.*) the generator reads or writes */
struct gglobals {
    int16_t currLevel, levelType;
    uint8_t gameStart, customLevel, graphicsHigh, isDamsel, isTunnelMan, hasSpectacles;
    uint8_t hadDarkLevel, darkLevel, noDarkLevel, lake, cityOfGold, cemetary;
    uint8_t blackMarket, madeBlackMarket, genBlackMarket, sacrificePit, snakePit, alienCraft, yetiLair;
    uint8_t madeMoai, shop, madeUdjatEye, genUdjatEye, madeMarketEntrance, genMarketEntrance;
    uint8_t giantSpider, genGiantSpider, LockedChest, Key, cleanSolids, murderer, checkWater;
    uint8_t ashGrave, TombLord, genTombLord, genGoldEntrance, madeGoldEntrance, olmecDead, doorOpen;
    uint8_t pickupItem;         /* enum pickup */
    int16_t thiefLevel, kaliPunish, kaliGift;
    double favor;               /* global.favor (Kali: halves from sacrifices) */
    int16_t probDarkLevel, probSnakePit, probCemetary, probLake, probSacPit, probAlien, probYetiLair;
    int16_t lockedChestChance, marketChance, goldChance;
    int16_t startRoomX, startRoomY, endRoomX, endRoomY, exitX, exitY;
    int16_t roomW, roomH;       /* room_width, room_height */
    int8_t roomPath[4][5];      /* global.roomPath[x, y] */
    int8_t roomPoss[4][4];
};

/* oGame's instance variables used by generation */
struct ggame { uint8_t damsel, idol, altar, levelGen;
               uint8_t genClothingShop, genBombShop, genSupplyShop, genRareShop, genWeaponShop; };

/* background tiles added by tile_add / tile_add2 (not instances; kept for the display). bg: GSPR_bg* */
struct gtile { int16_t bg, left, top, w, h, x, y; int16_t depth; };
#define GTILES_MAX 1024

extern struct gglobals G;
extern struct ggame GAME;
extern struct gtile gtiles[GTILES_MAX];
extern int gntiles;

/* scrClearGlobals() plus the other new-game values (oGlobals / scrInit / oTitle Create) the generator reads */
void gen_new_game(void);

/* the room for global.currLevel / global.lake (oTransition's choice): 0 rLevel, 1 rLevel2, 2 rLevel3, 3 rOlmec */
int gen_room_for_level(void);
extern int gen_room_force;            /* >= 0: gen_room_for_level returns it (playhost: the tracer's room_goto(rLevel)) */

/* create the level room's instances and run their Create events (oGame's ending in scrInitLevel when
   global.gameStart). next_id: the runtime's instance id counter at room start. The RNG (g_rng) is used as it
   stands. Returns 0, or -1 when gen_untranslated was set (GML not translated was reached). */
int gen_level(int32_t next_id);

/* object Create / Destroy events (genobj.c); gen_not_level: the room is not a level (isLevel() false: no gems in
   bricks), for the transition rooms */
extern int gen_not_level;
int instance_create(int x, int y, int obj);
void gen_create_event(int i);   /* run instance i's Create event (room instances) */
void instance_destroy(int i);

/* scripts (gen.c, genent.c, genroom.c) */
void scrRoomGen(int room);
void scrRoomGen2(int room);
void scrRoomGen3(int room);
void scrRoomGen4(int room);
void scrRoomGen5(int room);
void scrRoomGenMarket(int room);
void scrRoomGenYeti(int room);
void scrEntityGen(void);
void scrSetupWalls(int bottom);
void scrCheckWaterTop(int self);
void scrGenerateItem(int self, int x, int y, int setType, int *obj);
void scrShopItemsGen(int room, int xpos, int ypos, int shopType, int *obj);
int scrGetRoomX(int x);
int scrGetRoomY(int y);
int isInShop(int x, int y);
void tile_add(int bg, int left, int top, int w, int h, int x, int y, int depth);

#endif
