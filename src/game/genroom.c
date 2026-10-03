/* scrRoomGen (refs/hd/src/scripts/scrRoomGen/scrRoomGen.gml): one mines room, run `with oRoom` by scrInitLevel.
 * The template strings are tools/hdgentables.py's tables (build/gen/gentables.c, by GML line); the choices are
 * translated here with the GML line of each block. The variables strTemp, n, obj, shopType belong to the oRoom.
 */
#include "gen.h"
#include "rng.h"

static void put(char *t, const char *s, int pos, int len)
{
    int k;
    for (k = 0; k < len; k++)
        t[pos + k] = s[k];
}

void scrRoomGen(int room)
{
    const struct inst *r = &W.in[room];
    int x = r->x, y = r->y, n, i, j, xpos, ypos, roomPath, roomPathAbove, shopType, o = INST_NONE, obj = INST_NONE;
    char strTemp[81];
    const char *tmpl;

    tmpl = "00000000000000000000000000000000000000000000000000000000000000000000000000000000";   /* :47 */
    roomPath = G.roomPath[scrGetRoomX(x)][scrGetRoomY(y)];                                     /* :49 */
    roomPathAbove = -1;
    shopType = SHOP_GENERAL;
    if (scrGetRoomY(y) != 0) roomPathAbove = G.roomPath[scrGetRoomX(x)][scrGetRoomY(y - 128)];  /* :52 */

    if (scrGetRoomX(x) == G.startRoomX && scrGetRoomY(y) == G.startRoomY) {                  /* :54 start room */
        if (roomPath == 2) n = RAND(5, 8);
        else n = RAND(1, 4);
        tmpl = mines_start[n - 1];                                                            /* :58-69 */
    } else if (scrGetRoomX(x) == G.endRoomX && scrGetRoomY(y) == G.endRoomY) {               /* :71 end room */
        if (roomPathAbove == 2) n = RAND(2, 4);
        else n = RAND(3, 6);
        tmpl = mines_end[n - 1];                                                              /* :75-84 */
    } else if (roomPath == 0) {                                                               /* :86 side room */
        if (G.currLevel > 1 && !GAME.altar && RAND(1, 16) == 1) {
            n = 11;
            GAME.altar = 1;
        } else if (GAME.idol || scrGetRoomY(y) == 3) {
            n = RAND(1, 9);
        } else {
            n = RAND(1, 10);
            if (n == 10) GAME.idol = 1;
        }
        tmpl = mines_side[n - 1];                                                             /* :104-121 */
    } else if (roomPath == 0 || roomPath == 1) {                                              /* :123 main room */
        n = RAND(1, 12);                                                                      /* :125 */
        if (n <= 7) tmpl = mines_main[n - 1];
        else if (n == 8) tmpl = RAND(1, 2) == 1 ? mines_main[7] : mines_main[8];             /* :139-140 */
        else if (n <= 11) tmpl = mines_main[n];
        else tmpl = RAND(1, 2) == 1 ? mines_main[12] : mines_main[13];                       /* :148-149 */
    } else if (roomPath == 3) {                                                               /* :153 */
        n = RAND(1, 8);
        if (n <= 7) tmpl = mines_main3[n - 1];
        else tmpl = RAND(1, 2) == 1 ? mines_main3[7] : mines_main3[8];                       /* :169-170 */
    } else if (roomPath == 4 || roomPath == 5) {                                              /* :174, :190 shops */
        int b = roomPath == 4 ? 0 : 3;
        tmpl = mines_shop[b];
        n = RAND(1, 7);
        switch (n) {
        case 1: shopType = SHOP_GENERAL; break;
        case 2: shopType = SHOP_BOMB; break;
        case 3: shopType = SHOP_WEAPON; break;
        case 4: shopType = SHOP_RARE; break;
        case 5: shopType = SHOP_CLOTHING; break;
        case 6: shopType = SHOP_CRAPS; tmpl = mines_shop[b + 1]; break;
        case 7: shopType = SHOP_KISSING; tmpl = mines_shop[b + 2]; GAME.damsel = 1; break;
        }
    } else if (roomPath == 8) {                                                               /* :206 snake pit */
        RAND(1, 1);
        tmpl = mines_snake[0];
    } else if (roomPath == 9) {                                                               /* :213 */
        RAND(1, 1);
        tmpl = mines_snake[1];
    } else {                                                                                  /* :220 drop */
        if (roomPath == 7) n = RAND(4, 12);
        else if (roomPathAbove != 2) n = RAND(1, 12);
        else n = RAND(1, 8);
        tmpl = mines_drop[n - 1];
    }
    for (i = 0; i < 80; i++)
        strTemp[i] = tmpl[i];
    strTemp[80] = 0;

    /* :243-316 obstacles */
    for (i = 1; i < 81; i++) {
        const char *o1 = "00000", *o2 = "00000", *o3 = "00000";
        char tile = strTemp[i - 1];
        j = i;
        if (tile == '8') {
            n = RAND(1, 8) - 1;
            o1 = mines_obs8[3 * n]; o2 = mines_obs8[3 * n + 1]; o3 = mines_obs8[3 * n + 2];
        } else if (tile == '5') {
            n = RAND(1, 16) - 1;
            o1 = mines_obs5[3 * n]; o2 = mines_obs5[3 * n + 1]; o3 = mines_obs5[3 * n + 2];
        } else if (tile == '6') {
            n = RAND(1, 10) - 1;
            o1 = mines_obs6[3 * n]; o2 = mines_obs6[3 * n + 1]; o3 = mines_obs6[3 * n + 2];
        }
        if (tile == '5' || tile == '6' || tile == '8') {
            /* string_delete(j, 5) + string_insert at j: overwrite 5 characters (1-based j) */
            put(strTemp, o1, j - 1, 5);
            j += 10;
            put(strTemp, o2, j - 1, 5);
            j += 10;
            put(strTemp, o3, j - 1, 5);
        }
    }

    /* :319-484 tiles */
    for (j = 0; j < 8; j++) {
        for (i = 1; i < 11; i++) {
            char tile = strTemp[i - 1 + j * 10];
            xpos = x + (i - 1) * 16;
            ypos = y + j * 16;
            if (tile == '1' && collision_point(xpos, ypos, OBJ_oSolid) == INST_NONE) {        /* :326 */
                if (RAND(1, 10) == 1) instance_create(xpos, ypos, OBJ_oBlock);
                else instance_create(xpos, ypos, OBJ_oBrick);
            } else if (tile == '2' && RAND(1, 2) == 1 && collision_point(xpos, ypos, OBJ_oSolid) == INST_NONE) {
                if (RAND(1, 10) == 1) instance_create(xpos, ypos, OBJ_oBlock);              /* :336 */
                else instance_create(xpos, ypos, OBJ_oBrick);
            } else if (tile == 'L') instance_create(xpos, ypos, OBJ_oLadderOrange);
            else if (tile == 'P') instance_create(xpos, ypos, OBJ_oLadderTop);
            else if (tile == '7' && RAND(1, 3) == 1) instance_create(xpos, ypos, OBJ_oSpikes);
            else if (tile == '4' && RAND(1, 4) == 1) instance_create(xpos, ypos, OBJ_oPushBlock);
            else if (tile == '9') {                                                          /* :346 */
                int block = instance_create(xpos, ypos + 16, OBJ_oBrick);
                if (scrGetRoomX(x) == G.startRoomX && scrGetRoomY(y) == G.startRoomY)
                    instance_create(xpos, ypos, OBJ_oEntrance);
                else {
                    instance_create(xpos, ypos, OBJ_oExit);
                    G.exitX = (int16_t)xpos;
                    G.exitY = (int16_t)ypos;
                    W.in[block].flags |= IF_INVINCIBLE;
                }
            } else if (tile == 'A') {                                                        /* :359 */
                instance_create(xpos, ypos, OBJ_oAltarLeft);
                instance_create(xpos + 16, ypos, OBJ_oAltarRight);
            } else if (tile == 'x') {                                                        /* :364 */
                instance_create(xpos, ypos, OBJ_oSacAltarLeft);
                instance_create(xpos + 16, ypos, OBJ_oSacAltarRight);
                tile_add(BG_bgKaliBody, 0, 0, 64, 64, xpos - 16, ypos - 48, 10001);
                instance_create(xpos + 16, ypos - 80 + 16, OBJ_oKaliHead);
            } else if (tile == 'a') {
                instance_create(xpos, ypos, OBJ_oChest);
            } else if (tile == 'I') {
                instance_create(xpos + 16, ypos + 12, OBJ_oGoldIdol);
            } else if (tile == 'B') {                                                        /* :379 */
                instance_create(xpos + 16, ypos + 12, OBJ_oGiantTikiHead);
                tile_add(BG_bgTiki, 0, 0, 32, 64, xpos, ypos + 32, 10001);
                n = 16 * RAND(0, 2);
                tile_add(BG_bgTikiArms, n, 0, 16, 16, xpos + 32, ypos + 32, 10001);
                n = 16 * RAND(0, 2);
                tile_add(BG_bgTikiArms, n, 16, 16, 16, xpos - 16, ypos + 32, 10001);
            } else if (tile == 'Q') {
                if (shopType == SHOP_CRAPS) tile_add(BG_bgDiceSign, 0, 0, 48, 32, xpos, ypos, 9004);
            } else if (tile == 'q') {                                                        /* :393 */
                n = RAND(1, 6);
                scrGenerateItem(room, xpos + 8, ypos + 8, 1, &obj);
                W.in[obj].flags |= IF_INDICEHOUSE;
            } else if (tile == '+') {                                                        /* :399 */
                obj = instance_create(xpos, ypos, OBJ_oSolid);
                inst_set_sprite(obj, GSPR_sIceBlock);
                W.in[obj].flags |= IF_SHOPWALL;
            } else if (tile == 'W') {                                                        /* :405 */
                if (G.murderer || G.thiefLevel > 0) {
                    if (G.isDamsel) tile_add(BG_bgWanted, 32, 0, 32, 32, xpos, ypos, 9004);
                    else if (G.isTunnelMan) tile_add(BG_bgWanted, 64, 0, 32, 32, xpos, ypos, 9004);
                    else tile_add(BG_bgWanted, 0, 0, 32, 32, xpos, ypos, 9004);
                }
            } else if (tile == '.' && collision_point(xpos, ypos, OBJ_oSolid) == INST_NONE) { /* :414 */
                if (RAND(1, 10) == 1) obj = instance_create(xpos, ypos, OBJ_oBlock);
                else obj = instance_create(xpos, ypos, OBJ_oBrick);
                W.in[obj].flags |= IF_SHOPWALL;
            } else if (tile == 'b') {
                obj = instance_create(xpos, ypos, OBJ_oBrickSmooth);
                W.in[obj].flags |= IF_SHOPWALL;
            } else if (tile == 'l') {
                if (GAME.damsel) instance_create(xpos, ypos, OBJ_oLampRed);
                else instance_create(xpos, ypos, OBJ_oLamp);
            } else if (tile == 'K') {                                                        /* :433 */
                obj = instance_create(xpos, ypos, OBJ_oShopkeeper);
                W.in[obj].style = (uint8_t)shopType;
            } else if (tile == 'k') {                                                        /* :438 */
                static const int16_t sign[7] = { GSPR_sSignGeneral, GSPR_sSignBomb, GSPR_sSignWeapon,
                                                 GSPR_sSignRare, GSPR_sSignClothing, GSPR_sSignCraps,
                                                 GSPR_sSignKissing };
                obj = instance_create(xpos, ypos, OBJ_oSign);
                inst_set_sprite(obj, sign[shopType]);
            } else if (tile == 'i') {
                scrShopItemsGen(room, xpos, ypos, shopType, &obj);
            } else if (tile == 'd') {
                instance_create(xpos + 8, ypos + 8, OBJ_oDice);
            } else if (tile == 'D') {                                                        /* :457 */
                obj = instance_create(xpos + 8, ypos + 8, OBJ_oDamsel);
                W.in[obj].flags |= IF_FORSALE;
                W.in[obj].status = 5;
            } else if (tile == 's') {                                                        /* :463 */
                if (RAND(1, 10) == 1) instance_create(xpos, ypos, OBJ_oSnake);
                else if (RAND(1, 2) == 1) instance_create(xpos, ypos, OBJ_oBrick);
            } else if (tile == 'S') {
                instance_create(xpos, ypos, OBJ_oSnake);
            } else if (tile == 'T') {
                instance_create(xpos + 8, ypos + 8, OBJ_oRubyBig);
            } else if (tile == 'M') {                                                        /* :476 */
                instance_create(xpos, ypos, OBJ_oBrick);
                obj = instance_create(xpos + 8, ypos + 8, OBJ_oMattock);
                W.in[obj].cost = 0;
                W.in[obj].flags &= (uint8_t)~IF_FORSALE;
            }
        }
    }
    (void)o;
}
