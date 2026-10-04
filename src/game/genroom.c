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
                tile_add(GSPR_bgKaliBody, 0, 0, 64, 64, xpos - 16, ypos - 48, 10001);
                instance_create(xpos + 16, ypos - 80 + 16, OBJ_oKaliHead);
            } else if (tile == 'a') {
                instance_create(xpos, ypos, OBJ_oChest);
            } else if (tile == 'I') {
                instance_create(xpos + 16, ypos + 12, OBJ_oGoldIdol);
            } else if (tile == 'B') {                                                        /* :379 */
                instance_create(xpos + 16, ypos + 12, OBJ_oGiantTikiHead);
                tile_add(GSPR_bgTiki, 0, 0, 32, 64, xpos, ypos + 32, 10001);
                n = 16 * RAND(0, 2);
                tile_add(GSPR_bgTikiArms, n, 0, 16, 16, xpos + 32, ypos + 32, 10001);
                n = 16 * RAND(0, 2);
                tile_add(GSPR_bgTikiArms, n, 16, 16, 16, xpos - 16, ypos + 32, 10001);
            } else if (tile == 'Q') {
                if (shopType == SHOP_CRAPS) tile_add(GSPR_bgDiceSign, 0, 0, 48, 32, xpos, ypos, 9004);
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
                    if (G.isDamsel) tile_add(GSPR_bgWanted, 32, 0, 32, 32, xpos, ypos, 9004);
                    else if (G.isTunnelMan) tile_add(GSPR_bgWanted, 64, 0, 32, 32, xpos, ypos, 9004);
                    else tile_add(GSPR_bgWanted, 0, 0, 32, 32, xpos, ypos, 9004);
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
                W.in[obj].flags &= (uint16_t)~IF_FORSALE;
            }
        }
    }
    (void)o;
}

/* ============================================================================================================ */
/* The other areas' room generators. Their templates come from tools/hdgentables.py by GML line: GT(gt_<script>,
 * line, k) is the k-th string literal on that line. strTemp is edited with GameMaker's string_delete /
 * string_insert (gs_del / gs_ins, the runner's rules: indexes from 1, clamped to the string), so obstacles that
 * run past the end behave as in the runner. */

struct gs { char c[200]; int16_t n; };

static void gs_set(struct gs *s, const char *t)
{
    int k = 0;
    while (t[k] && k < (int)sizeof s->c) { s->c[k] = t[k]; k++; }
    s->n = (int16_t)k;
}

/* string_delete(str, index, count) */
static void gs_del(struct gs *s, int idx, int cnt)
{
    int a, b, k;
    if (cnt <= 0 || idx <= 0)
        return;
    a = idx - 1;
    if (a > s->n) a = s->n;
    b = a + cnt;
    if (b > s->n) b = s->n;
    for (k = b; k < s->n; k++)
        s->c[a + k - b] = s->c[k];
    s->n = (int16_t)(s->n - (b - a));
}

/* string_insert(substr, str, index) */
static void gs_ins(struct gs *s, const char *sub, int idx)
{
    int a = idx - 1, m = 0, k;
    while (sub[m]) m++;
    if (a < 0) a = 0;
    if (a > s->n) a = s->n;
    if (s->n + m > (int)sizeof s->c) { UNTRANSLATED(6001); return; }
    for (k = s->n - 1; k >= a; k--)
        s->c[k + m] = s->c[k];
    for (k = 0; k < m; k++)
        s->c[a + k] = sub[k];
    s->n = (int16_t)(s->n + m);
}

/* string_char_at(str, i) ('\0' for "") */
static char gs_at(const struct gs *s, int i)
{
    i -= 1;
    if (s->n == 0 || i >= s->n) return 0;
    if (i < 0) i = 0;
    return s->c[i];
}

/* the obstacle overlay: strTemp = string_delete(strTemp, j, w); strTemp = string_insert(strObs1, strTemp, j);
   j += 10; ... for each row (rows 3 or 4) */
static void gs_obs(struct gs *s, int j, int w, const char *const *rows, int nrows)
{
    int r;
    for (r = 0; r < nrows; r++) {
        if (r) j += 10;
        gs_del(s, j, w);
        gs_ins(s, rows[r], j);
    }
}

#define NOTSOL(px, py) (collision_point((px), (py), OBJ_oSolid) == INST_NONE)

/* a template by switch: the k-th template literal of `case n:` of the switch on GML line `line` (tools/
   hdgentables.py); a missing one (a translation error) stops the level as untranslated */
static const char *sw_lit(const struct gswtab *t, int line, int n, int k)
{
    const char *p = gsw(t, line, n, k);
    if (!p) { UNTRANSLATED(6100 + line); return "00000000000000000000000000000000000000000000000000000000000000000000000000000000"; }
    return p;
}
#define S2(l, n, k) sw_lit(&gt_scrRoomGen2_cases, l, n, k)
#define S3(l, n, k) sw_lit(&gt_scrRoomGen3_cases, l, n, k)
#define S4(l, n, k) sw_lit(&gt_scrRoomGen4_cases, l, n, k)
#define S5(l, n, k) sw_lit(&gt_scrRoomGen5_cases, l, n, k)
#define SM(l, n, k) sw_lit(&gt_scrRoomGenMarket_cases, l, n, k)
#define SY(l, n, k) sw_lit(&gt_scrRoomGenYeti_cases, l, n, k)

/* strObs1..strObs4 = the case's literals (an obstacle switch) */
static void obs_set(const char **ob, const struct gswtab *t, int line, int n, int cnt)
{
    int k;
    for (k = 0; k < cnt; k++) ob[k] = sw_lit(t, line, n, k);
}

/* a tree, scrRoomGen2 :496-541 (scrRoomGen4 :514-559, scrRoomGenMarket :379-424 the same) */
static void gen_tree(int xpos, int ypos)
{
    int tx = xpos, ty = ypos - 16, b1 = 0, b2 = 0, m;
    instance_create(xpos, ypos, OBJ_oTree);
    for (m = 0; m < 5; m += 1) {
        if (RAND(0, m) > 2) break;
        if (NOTSOL(tx, ty - 16) && NOTSOL(tx - 16, ty - 16) && NOTSOL(tx + 16, ty - 16)) {
            instance_create(tx, ty, OBJ_oTree);
            if (m < 4) {
                if (RAND(1, 5) < 4 && !b1) {
                    instance_create(tx + 16, ty, OBJ_oTreeBranch);
                    b1 = 1;
                } else if (b1) b1 = 0;
                if (RAND(1, 5) < 4 && !b2) {
                    instance_create(tx - 16, ty, OBJ_oTreeBranch);
                    b2 = 1;
                } else if (b2) b2 = 0;
            }
        } else break;
        ty -= 16;
    }
    instance_create(tx - 16, ty + 16, OBJ_oLeaves);
    instance_create(tx + 16, ty + 16, OBJ_oLeaves);
}

/* shop tiles shared by the room generators (scrRoomGen2 :409-470 and the same blocks elsewhere) */
static void tile_q(int room, int xpos, int ypos, int *obj)                    /* "q": dice-house prize */
{
    RAND(1, 6);                                                                /* n = rand(1,6) */
    scrGenerateItem(room, xpos + 8, ypos + 8, 1, obj);
    W.in[*obj].flags |= IF_INDICEHOUSE;
}

static void tile_wanted(int xpos, int ypos)                                   /* "W" */
{
    if (G.murderer || G.thiefLevel > 0) {
        if (G.isDamsel) tile_add(GSPR_bgWanted, 32, 0, 32, 32, xpos, ypos, 9004);
        else if (G.isTunnelMan) tile_add(GSPR_bgWanted, 64, 0, 32, 32, xpos, ypos, 9004);
        else tile_add(GSPR_bgWanted, 0, 0, 32, 32, xpos, ypos, 9004);
    }
}

static void tile_icewall(int xpos, int ypos, int *obj)                         /* "+" */
{
    *obj = instance_create(xpos, ypos, OBJ_oSolid);
    inst_set_sprite(*obj, GSPR_sIceBlock);
    W.in[*obj].flags |= IF_SHOPWALL;
}

static void tile_sign(int xpos, int ypos, int shopType, int *obj)             /* "k" */
{
    static const int16_t sign[7] = { GSPR_sSignGeneral, GSPR_sSignBomb, GSPR_sSignWeapon, GSPR_sSignRare,
                                     GSPR_sSignClothing, GSPR_sSignCraps, GSPR_sSignKissing };
    *obj = instance_create(xpos, ypos, OBJ_oSign);
    if (shopType >= 0 && shopType < 7) inst_set_sprite(*obj, sign[shopType]);
}

static void tile_shopkeeper(int xpos, int ypos, int shopType, int *obj)       /* "K" */
{
    *obj = instance_create(xpos, ypos, OBJ_oShopkeeper);
    W.in[*obj].style = (uint8_t)shopType;
}

static void tile_damsel_for_sale(int xpos, int ypos, int *obj)                /* "D" ("!", "A") */
{
    *obj = instance_create(xpos + 8, ypos + 8, OBJ_oDamsel);
    W.in[*obj].flags |= IF_FORSALE;
    W.in[*obj].status = 5;
}

static void tile_smooth(int xpos, int ypos, int spr, int *obj)                /* "b" / ":" */
{
    *obj = instance_create(xpos, ypos, OBJ_oBrickSmooth);
    inst_set_sprite(*obj, spr);
    W.in[*obj].flags |= IF_SHOPWALL;
}

static void tile_exit(int room, int xpos, int ypos, int blockobj)              /* "9" */
{
    int x = W.in[room].x, y = W.in[room].y;
    int block = instance_create(xpos, ypos + 16, blockobj);
    if (scrGetRoomX(x) == G.startRoomX && scrGetRoomY(y) == G.startRoomY)
        instance_create(xpos, ypos, OBJ_oEntrance);
    else {
        instance_create(xpos, ypos, OBJ_oExit);
        G.exitX = (int16_t)xpos;
        G.exitY = (int16_t)ypos;
        W.in[block].flags |= IF_INVINCIBLE;
    }
}

static void tile_altar(int xpos, int ypos)                                    /* "x" */
{
    instance_create(xpos, ypos, OBJ_oSacAltarLeft);
    instance_create(xpos + 16, ypos, OBJ_oSacAltarRight);
    tile_add(GSPR_bgKaliBody, 0, 0, 64, 64, xpos - 16, ypos - 48, 10001);
    instance_create(xpos + 16, ypos - 80 + 16, OBJ_oKaliHead);
}

static int shop_pick7(void)                                                    /* switch(rand(1,7)) shop type */
{
    return RAND(1, 7) - 1;    /* 1 General .. 7 Kissing = enum order */
}

/* ------------------------------------------------------------------------------------------------------------ */
/* scripts/scrRoomGen2/scrRoomGen2.gml: lush */
#define T2(l, k) GT(gt_scrRoomGen2, l, k)
void scrRoomGen2(int room)
{
    int x = W.in[room].x, y = W.in[room].y, n, i, j, xpos, ypos, roomPath, roomPathAbove, obj = INST_NONE;
    int shopType = -1;          /* not set by this script outside shops */
    struct gs st;

    gs_set(&st, T2(47, 0));
    roomPath = G.roomPath[scrGetRoomX(x)][scrGetRoomY(y)];                    /* :49 */
    roomPathAbove = -1;
    if (scrGetRoomY(y) != 0) roomPathAbove = G.roomPath[scrGetRoomX(x)][scrGetRoomY(y - 128)];

    if (scrGetRoomX(x) == G.startRoomX && scrGetRoomY(y) == G.startRoomY) {  /* :54 start */
        if (roomPath == 2) n = RAND(3, 4);
        else n = RAND(1, 2);
        gs_set(&st, S2(57, n, 0));
    } else if (scrGetRoomX(x) == G.endRoomX && scrGetRoomY(y) == G.endRoomY) {   /* :66 end */
        if (G.lake) n = RAND(5, 5);
        else if (roomPathAbove == 2) n = RAND(1, 2);
        else n = RAND(3, 4);
        gs_set(&st, S2(71, n, 0));
    } else if (roomPath == 0 && RAND(1, 3) <= 2) {                            /* :80 side room */
        if (!GAME.altar && RAND(1, 12) == 1) {
            n = 10;
            GAME.altar = 1;
        } else if (GAME.idol) {
            n = RAND(1, 8);
        } else {
            n = RAND(1, 9);
            if (n == 9) GAME.idol = 1;
        }
        if (n == 9) gs_set(&st, S2(97, 9, G.cemetary ? 0 : 1));             /* :112-113 */
        else gs_set(&st, S2(97, n, 0));
    } else if (roomPath == 0 || roomPath == 1) {                              /* :120 */
        n = RAND(1, 10);                                                      /* :122 switch(rand(1,10)) */
        if (n == 6) gs_set(&st, S2(122, 6, RAND(1, 2) == 1 ? 0 : 1));      /* :131-132 */
        else gs_set(&st, S2(122, n, 0));
    } else if (roomPath == 3) {                                               /* :144 */
        n = RAND(1, 7);
        gs_set(&st, S2(146, n, 0));
    } else if (roomPath == 4) {                                               /* :161 shop */
        gs_set(&st, T2(163, 0));
        shopType = shop_pick7();
        if (shopType == SHOP_CRAPS) gs_set(&st, S2(165, 6, 0));
        if (shopType == SHOP_KISSING) { gs_set(&st, S2(165, 7, 0)); GAME.damsel = 1; }
    } else if (roomPath == 5) {                                               /* :176 shop */
        gs_set(&st, T2(178, 0));
        shopType = shop_pick7();
        if (shopType == SHOP_CRAPS) gs_set(&st, S2(180, 6, 0));
        if (shopType == SHOP_KISSING) { gs_set(&st, S2(180, 7, 0)); GAME.damsel = 1; }
    } else if (roomPath == 7) {                                               /* :191 lake bottom */
        n = RAND(1, 8);
        gs_set(&st, S2(193, n, 0));
    } else if (roomPath == 8) {                                               /* :205 lake top */
        if (G.roomPath[scrGetRoomX(x)][scrGetRoomY(y - 128)] == 2) n = RAND(1, 5);
        else n = RAND(1, 8);
        gs_set(&st, S2(211, n, 0));
    } else if (roomPath == 9) {                                               /* :223 mega mouth */
        RAND(1, 1);
        gs_set(&st, S2(225, 1, 0));
    } else {                                                                  /* :230 drop */
        if (roomPathAbove != 2) n = RAND(1, 6);
        else n = RAND(1, 5);
        gs_set(&st, S2(234, n, 0));
    }

    for (i = 1; i < 81; i += 1) {                                             /* :248 obstacles */
        const char *ob[4] = { "00000", "00000", "00000", "00000" };
        char tile = gs_at(&st, i);
        j = i;
        if (tile == '8') {
            RAND(1, 1);
            obs_set(ob, &gt_scrRoomGen2_cases, 261, 1, 3);
        } else if (tile == '5') {
            if (RAND(1, 8) == 1) n = RAND(100, 102);
            else n = RAND(1, 2);
            obs_set(ob, &gt_scrRoomGen2_cases, 270, n, 3);
        } else if (tile == '6') {
            n = RAND(1, 4);
            obs_set(ob, &gt_scrRoomGen2_cases, 282, n, 3);
        } else if (tile == 'V') {
            n = RAND(1, 3);
            obs_set(ob, &gt_scrRoomGen2_cases, 293, n, 4);
        }
        if (tile == '5' || tile == '6' || tile == '8' || tile == 'V')
            gs_obs(&st, j, 5, ob, tile == 'V' ? 4 : 3);                       /* :301-317 */
    }

    for (j = 0; j < 8; j += 1) {                                              /* :321 tiles */
        for (i = 1; i < 11; i += 1) {
            char tile = gs_at(&st, i + j * 10);
            xpos = x + (i - 1) * 16;
            ypos = y + j * 16;
            if (tile == '1' && NOTSOL(xpos, ypos)) instance_create(xpos, ypos, OBJ_oLush);
            else if (tile == '2' && RAND(1, 2) == 1 && NOTSOL(xpos, ypos)) instance_create(xpos, ypos, OBJ_oLush);
            if (tile == 't' && NOTSOL(xpos, ypos)) instance_create(xpos, ypos, OBJ_oTemple);   /* :336 */
            if (tile == 'r' && NOTSOL(xpos, ypos)) {                          /* :340 */
                if (RAND(1, 2) == 1) instance_create(xpos, ypos, OBJ_oTemple);
                else instance_create(xpos, ypos, OBJ_oLush);
            } else if (tile == '3' && NOTSOL(xpos, ypos)) {
                if (RAND(1, 2) == 1) instance_create(xpos, ypos, OBJ_oWaterSwim);
                else instance_create(xpos, ypos, OBJ_oLush);
            } else if (tile == 'L') instance_create(xpos, ypos, OBJ_oVine);
            else if (tile == 'P') instance_create(xpos, ypos, OBJ_oVineTop);
            else if (tile == '7' && RAND(1, 3) == 1) instance_create(xpos, ypos, OBJ_oSpikes);
            else if (tile == 's') instance_create(xpos, ypos, OBJ_oSpikes);
            else if (tile == '4') instance_create(xpos, ypos, OBJ_oPushBlock);
            else if (tile == '9') tile_exit(room, xpos, ypos, OBJ_oLush);
            else if (tile == 'c') instance_create(xpos, ypos, OBJ_oChest);
            else if (tile == 'd') {
                instance_create(xpos, ypos, OBJ_oWaterSwim);
                instance_create(xpos, ypos, OBJ_oChest);
            } else if (tile == 'w') instance_create(xpos, ypos, OBJ_oWaterSwim);
            else if (tile == 'v') {
                instance_create(xpos, ypos, OBJ_oWaterSwim);
                instance_create(xpos, ypos, OBJ_oLush);
            } else if (tile == ',') {
                instance_create(xpos, ypos, OBJ_oWaterSwim);
                if (RAND(1, 2) == 1) instance_create(xpos, ypos, OBJ_oLush);
            } else if (tile == 'J') {
                instance_create(xpos, ypos, OBJ_oWaterSwim);
                instance_create(xpos, ypos, OBJ_oJaws);
            } else if (tile == 'I') instance_create(xpos + 16, ypos + 12, OBJ_oGoldIdol);
            else if (tile == 'C') instance_create(xpos + 16, ypos + 12, OBJ_oCrystalSkull);
            else if (tile == '.' && NOTSOL(xpos, ypos)) {                     /* :404 */
                obj = instance_create(xpos, ypos, OBJ_oLush);
                W.in[obj].flags |= IF_SHOPWALL;
            } else if (tile == 'Q') {
                if (shopType == SHOP_CRAPS) tile_add(GSPR_bgDiceSign, 0, 0, 48, 32, xpos, ypos, 9004);
            } else if (tile == 'q') tile_q(room, xpos, ypos, &obj);
            else if (tile == '+') tile_icewall(xpos, ypos, &obj);
            else if (tile == 'W') tile_wanted(xpos, ypos);
            else if (tile == 'b') tile_smooth(xpos, ypos, GSPR_sLushSmooth, &obj);
            else if (tile == 'l') {
                if (GAME.damsel) instance_create(xpos, ypos, OBJ_oLampRed);
                else instance_create(xpos, ypos, OBJ_oLamp);
            } else if (tile == 'K') tile_shopkeeper(xpos, ypos, shopType, &obj);
            else if (tile == 'k') tile_sign(xpos, ypos, shopType, &obj);
            else if (tile == 'i') scrShopItemsGen(room, xpos, ypos, shopType, &obj);
            else if (tile == 'u') instance_create(xpos + 8, ypos + 8, OBJ_oDice);
            else if (tile == 'D') tile_damsel_for_sale(xpos, ypos, &obj);
            else if (tile == 'B') {                                           /* :478 */
                int idol = instance_first(OBJ_oGoldIdol), dt;
                obj = instance_create(xpos, ypos, OBJ_oTrapBlock);
                if (idol == INST_NONE) { UNTRANSLATED(6002); continue; }
                dt = W.in[obj].x - (W.in[idol].x - 8);
                if (dt < 0) dt = -dt;
                dt = 40 - dt;
                if (dt < 0) dt = 0;
                W.in[obj].deathtimer = (int16_t)dt;
            } else if (tile == 'x') tile_altar(xpos, ypos);
            else if (tile == 'p') {
                RAND(1, 2);                                                   /* if (rand(1,2)): always true */
                instance_create(xpos, ypos, OBJ_oFakeBones);
            } else if (tile == 'T') gen_tree(xpos, ypos);
        }
    }
}

/* ------------------------------------------------------------------------------------------------------------ */
/* scripts/scrRoomGen3/scrRoomGen3.gml: ice */
#define T3(l, k) GT(gt_scrRoomGen3, l, k)
static void alien_floor(int xpos, int ypos, int spr)
{
    int t = instance_create(xpos, ypos, OBJ_oAlienShip);
    inst_set_sprite(t, spr);
}

void scrRoomGen3(int room)
{
    int x = W.in[room].x, y = W.in[room].y, n, i, j, xpos, ypos, roomPath, obj = INST_NONE;
    int shopType = -1;
    struct gs st;

    gs_set(&st, T3(47, 0));
    roomPath = G.roomPath[scrGetRoomX(x)][scrGetRoomY(y)];                    /* :49 */
    if (scrGetRoomX(x) == G.startRoomX && scrGetRoomY(y) == G.startRoomY) {  /* :50 */
        if (roomPath == 2) n = RAND(2, 2);
        else n = RAND(1, 1);
        gs_set(&st, S3(54, n, 0));
    } else if (scrGetRoomX(x) == G.endRoomX && scrGetRoomY(y) == G.endRoomY) {   /* :61 */
        RAND(1, 1);
        gs_set(&st, S3(64, 1, 0));
    } else if (roomPath == 0 && RAND(1, 2) == 1) {                            /* :69 side room */
        if (!GAME.altar && RAND(1, 12) == 1) {
            n = 10;
            GAME.altar = 1;
        } else if (GAME.idol) {
            n = RAND(1, 8);
        } else {
            n = RAND(1, 9);
            if (n == 9) GAME.idol = 1;
        }
        gs_set(&st, S3(86, n, 0));
    } else if ((roomPath == 0 || roomPath == 1 || roomPath == 2) && (RAND(1, 10) < 10)) {   /* :105 */
        n = RAND(1, 9);
        gs_set(&st, S3(107, n, 0));
    } else if (roomPath == 4) {                                               /* :123 shop */
        gs_set(&st, T3(125, 0));
        shopType = shop_pick7();
        if (shopType == SHOP_CRAPS) gs_set(&st, S3(127, 6, 0));
        if (shopType == SHOP_KISSING) { gs_set(&st, S3(127, 7, 0)); GAME.damsel = 1; }
    } else if (roomPath == 5) {                                               /* :139 shop */
        gs_set(&st, T3(141, 0));
        shopType = shop_pick7();
        if (shopType == SHOP_CRAPS) gs_set(&st, S3(143, 6, 0));
        if (shopType == SHOP_KISSING) { gs_set(&st, S3(143, 7, 0)); GAME.damsel = 1; }
    } else if (roomPath == 6) {                                               /* :154 moai */
        n = RAND(1, 2);
        gs_set(&st, S3(156, n, 0));
    } else if (roomPath == 7) {                                               /* :162 alien craft */
        RAND(1, 1);
        gs_set(&st, S3(164, 1, 0));
    } else if (roomPath == 8) {
        RAND(1, 1);
        gs_set(&st, S3(171, 1, 0));
    } else if (roomPath == 9) {
        RAND(1, 1);
        gs_set(&st, S3(178, 1, 0));
    } else {                                                                  /* :183 */
        RAND(1, 1);
        gs_set(&st, S3(185, 1, 0));
    }

    for (i = 1; i < 81; i += 1) {                                             /* :193 obstacles */
        const char *ob[3] = { "00000", "00000", "00000" };
        char tile = gs_at(&st, i);
        j = i;
        if (tile == '8') {
            n = RAND(1, 6);
            obs_set(ob, &gt_scrRoomGen3_cases, 204, n, 3);
        } else if (tile == '5') {
            n = RAND(1, 15);
            obs_set(ob, &gt_scrRoomGen3_cases, 216, n, 3);
        } else if (tile == '6') {
            n = RAND(1, 4);
            obs_set(ob, &gt_scrRoomGen3_cases, 237, n, 3);
        } else if (tile == 'F') {
            n = RAND(1, 12);
            obs_set(ob, &gt_scrRoomGen3_cases, 247, n, 3);
        }
        if (tile == '5' || tile == '6' || tile == '8') gs_obs(&st, j, 5, ob, 3);   /* :264 */
        else if (tile == 'F') gs_obs(&st, j, 3, ob, 3);                           /* :275 */
    }

    for (j = 0; j < 8; j += 1) {                                              /* :289 tiles */
        for (i = 1; i < 11; i += 1) {
            char tile = gs_at(&st, i + j * 10);
            xpos = x + (i - 1) * 16;
            ypos = y + j * 16;
            if (tile == '1' && NOTSOL(xpos, ypos)) {
                if (RAND(1, 10) == 1) instance_create(xpos, ypos, OBJ_oIce);
                else instance_create(xpos, ypos, OBJ_oDark);
            } else if (tile == '2' && RAND(1, 2) == 1 && NOTSOL(xpos, ypos)) {
                if (RAND(1, 10) == 1) instance_create(xpos, ypos, OBJ_oIce);
                else instance_create(xpos, ypos, OBJ_oDark);
            } else if (tile == 'L') instance_create(xpos, ypos, OBJ_oVine);
            else if (tile == 'P') instance_create(xpos, ypos, OBJ_oVine);
            else if (tile == '7') instance_create(xpos, ypos, OBJ_oSpikes);
            else if (tile == '4' && RAND(1, 4) == 1) instance_create(xpos, ypos, OBJ_oPushBlock);
            else if (tile == '9') tile_exit(room, xpos, ypos, OBJ_oDark);
            else if (tile == 'a') {
                if (RAND(1, 1) == 1) instance_create(xpos, ypos, OBJ_oChest);
            } else if (tile == 'I') instance_create(xpos + 16, ypos + 12, OBJ_oGoldIdol);
            else if (tile == '.' && NOTSOL(xpos, ypos)) {                     /* :331 */
                obj = instance_create(xpos, ypos, OBJ_oDark);
                W.in[obj].flags |= IF_SHOPWALL;
            } else if (tile == 'Q') {
                if (shopType == SHOP_CRAPS) tile_add(GSPR_bgDiceSign, 0, 0, 48, 32, xpos, ypos, 9004);
            } else if (tile == 'q') tile_q(room, xpos, ypos, &obj);
            else if (tile == '+') tile_icewall(xpos, ypos, &obj);
            else if (tile == 'W') tile_wanted(xpos, ypos);
            else if (tile == ':') tile_smooth(xpos, ypos, GSPR_sDarkSmooth, &obj);
            else if (tile == 'l') {
                if (GAME.damsel) instance_create(xpos, ypos, OBJ_oLampRed);
                else instance_create(xpos, ypos, OBJ_oLamp);
            } else if (tile == 'K') tile_shopkeeper(xpos, ypos, shopType, &obj);
            else if (tile == 'k') tile_sign(xpos, ypos, shopType, &obj);
            else if (tile == '$') scrShopItemsGen(room, xpos, ypos, shopType, &obj);
            else if (tile == 'u') instance_create(xpos + 8, ypos + 8, OBJ_oDice);
            else if (tile == '!') tile_damsel_for_sale(xpos, ypos, &obj);
            else if (tile == 'Y') instance_create(xpos, ypos, OBJ_oYeti);
            else if (tile == 'A') {                                           /* :409 */
                instance_create(xpos, ypos, OBJ_oAlienShip);
                tile_add(GSPR_bgAlienShip3, 0, 0, 16, 16, xpos, ypos, 9005);
            } else if (tile == 'B') {
                alien_floor(xpos, ypos, GSPR_sAlienFloor);
                tile_add(GSPR_bgAlienShip3, 0, 0, 16, 16, xpos, ypos, 9005);
            } else if (tile == 'b') {                                         /* :420 */
                if (RAND(1, 2) == 1) {
                    if (RAND(1, 10) == 1) instance_create(xpos, ypos, OBJ_oIce);
                    else instance_create(xpos, ypos, OBJ_oDark);
                } else {
                    alien_floor(xpos, ypos, GSPR_sAlienFloor);
                    tile_add(GSPR_bgAlienShip3, 0, 0, 16, 16, xpos, ypos, 9005);
                }
            } else if (tile == 'C') {                                         /* :434 */
                alien_floor(xpos, ypos, GSPR_sAlienFront);
                alien_floor(xpos, ypos + 16, GSPR_sAlienFront);
                alien_floor(xpos, ypos + 32, GSPR_sAlienFront2);
                tile_add(GSPR_bgAlienShip3, 0, 0, 16, 16, xpos, ypos, 9005);
                tile_add(GSPR_bgAlienShip3, 0, 0, 16, 16, xpos, ypos + 16, 160);
                tile_add(GSPR_bgAlienShip3, 0, 0, 16, 16, xpos, ypos + 32, 160);
            } else if (tile == 'D') {
                alien_floor(xpos, ypos, GSPR_sAlienFront3);
                tile_add(GSPR_bgAlienShip3, 0, 0, 16, 16, xpos, ypos, 9005);
            } else if (tile == 'E') {
                if (RAND(1, 3) == 1) tile_add(GSPR_bgAlienShip, 32, 0, 32, 64, xpos, ypos, 9005);
                else tile_add(GSPR_bgAlienShip, 0, 0, 32, 64, xpos, ypos, 9005);
            } else if (tile == 'G') tile_add(GSPR_bgAlienShip2, 0, 0, 32, 48, xpos, ypos, 9005);
            else if (tile == 'X') instance_create(xpos, ypos, OBJ_oAlienBoss);
            else if (tile == 'x') tile_altar(xpos, ypos);
            else if (tile == 'T') {                                           /* :473 */
                instance_create(xpos, ypos, OBJ_oDark);
                obj = instance_create(xpos + 8, ypos + 8, OBJ_oJetpack);
                W.in[obj].cost = 0;
                W.in[obj].flags &= (uint16_t)~IF_FORSALE;
            } else if (tile == 't') instance_create(xpos, ypos, OBJ_oBarrierEmitter);
            else if (tile == 'c') instance_create(xpos, ypos, OBJ_oThinIce);
            else if (tile == 'M' && NOTSOL(xpos, ypos)) {                     /* :488 moai */
                instance_create(xpos, ypos, OBJ_oMoai);
                instance_create(xpos + 16, ypos, OBJ_oMoai2);
                instance_create(xpos + 32, ypos, OBJ_oMoai3);
                instance_create(xpos + 16, ypos + 16, OBJ_oMoaiInside);
                tile_add(GSPR_bgAlienShip3, 0, 0, 16, 16, xpos + 16, ypos + 16, 150);
                tile_add(GSPR_bgAlienShip3, 0, 0, 16, 16, xpos + 16, ypos + 32, 150);
                instance_create(xpos + 16, ypos + 32, OBJ_oThinIce);
                obj = instance_create(xpos + 16, ypos + 48, OBJ_oExit);
                W.in[obj].etype = EX_MOAI;
                obj = instance_create(xpos + 16 + 8, ypos + 48 + 8, OBJ_oCrown);
                W.in[obj].cost = 0;
                W.in[obj].flags &= (uint16_t)~IF_FORSALE;
            } else if (tile == 'm' && NOTSOL(xpos, ypos)) {
                obj = instance_create(xpos, ypos, OBJ_oDark);
                W.in[obj].flags |= IF_INVINCIBLE;
            } else if (tile == 'i') instance_create(xpos, ypos, OBJ_oIce);
            else if (tile == 'j' && RAND(1, 2) == 1) instance_create(xpos, ypos, OBJ_oIce);
            else if (tile == 'f') instance_create(xpos, ypos, OBJ_oDarkFall);
            else if (tile == 'w') instance_create(xpos, ypos, OBJ_oWaterSwim);
        }
    }
}

/* ------------------------------------------------------------------------------------------------------------ */
/* scripts/scrRoomGen4/scrRoomGen4.gml: temple */
#define T4(l, k) GT(gt_scrRoomGen4, l, k)
void scrRoomGen4(int room)
{
    int x = W.in[room].x, y = W.in[room].y, n, i, j, k, l, xpos, ypos, roomPath, obj = INST_NONE;
    int shopType = -1;
    struct gs st;

    gs_set(&st, T4(47, 0));
    roomPath = G.roomPath[scrGetRoomX(x)][scrGetRoomY(y)];                    /* :49 */
    if (scrGetRoomX(x) == G.startRoomX && scrGetRoomY(y) == G.startRoomY) {  /* :50 */
        if (roomPath == 2) n = RAND(2, 2);
        else n = RAND(1, 1);
        gs_set(&st, S4(54, n, 0));
    } else if (scrGetRoomX(x) == G.endRoomX && scrGetRoomY(y) == G.endRoomY) {   /* :61 */
        RAND(1, 1);
        gs_set(&st, S4(64, 1, 0));
    } else if (roomPath == 0 && RAND(1, 4) > 1) {                             /* :69 side room */
        if (G.cityOfGold) {
            n = RAND(1, 14);
            if (n == 12) n = 15;
        } else if (!GAME.altar && RAND(1, 12) == 1) {
            n = 16;
            GAME.altar = 1;
        } else if (GAME.idol) {
            n = RAND(1, 11);
        } else {
            n = RAND(1, 12);
            if (n == 12) GAME.idol = 1;
        }
        if (n == 10) gs_set(&st, S4(91, 10, RAND(1, 2) == 1 ? 0 : 1));     /* :106-107 */
        else gs_set(&st, S4(91, n, 0));
    } else if (roomPath == 0 || roomPath == 1) {                              /* :121 */
        if (G.cityOfGold) n = RAND(1, 12);
        else n = RAND(1, 10);
        gs_set(&st, S4(125, n, 0));
    } else if (roomPath == 3) {                                               /* :144 */
        n = RAND(1, 4);
        gs_set(&st, S4(146, n, 0));
    } else if (roomPath == 4) {                                               /* :156 shop */
        gs_set(&st, T4(158, 0));
        if (GAME.damsel) n = RAND(1, 6);
        else n = RAND(1, 7);
        shopType = n - 1;
        if (shopType == SHOP_CRAPS) gs_set(&st, S4(162, 6, 0));
        if (shopType == SHOP_KISSING) { gs_set(&st, S4(162, 7, 0)); GAME.damsel = 1; }
    } else if (roomPath == 5) {                                               /* :173 shop */
        gs_set(&st, T4(175, 0));
        if (GAME.damsel) n = RAND(1, 6);
        else n = RAND(1, 7);
        shopType = n - 1;
        if (shopType == SHOP_CRAPS) gs_set(&st, S4(179, 6, 0));
        if (shopType == SHOP_KISSING) { gs_set(&st, S4(179, 7, 0)); GAME.damsel = 1; }
    } else if (roomPath == 6) gs_set(&st, T4(192, 0));                        /* :190 Lady Xoc */
    else if (roomPath == 7) gs_set(&st, T4(196, 0));                          /* pit top */
    else if (roomPath == 8) gs_set(&st, T4(200, 0));                          /* pit */
    else if (roomPath == 9) gs_set(&st, T4(204, 0));                          /* pit bottom */
    else {                                                                    /* :206 drop */
        n = RAND(1, 8);
        gs_set(&st, S4(208, n, 0));
    }

    for (i = 1; i < 81; i += 1) {                                             /* :223 obstacles */
        const char *ob[3] = { "00000", "00000", "00000" };
        char tile = gs_at(&st, i);
        j = i;
        if (tile == '8') {
            RAND(1, 1);
            obs_set(ob, &gt_scrRoomGen4_cases, 234, 1, 3);
        } else if (tile == '5') {
            n = RAND(1, 8);
            obs_set(ob, &gt_scrRoomGen4_cases, 241, n, 3);
        } else if (tile == '6') {
            n = RAND(1, 10);
            obs_set(ob, &gt_scrRoomGen4_cases, 255, n, 3);
        } else if (tile == 'r') {
            n = RAND(1, 10);
            obs_set(ob, &gt_scrRoomGen4_cases, 271, n, 3);
        }
        if (tile == '5' || tile == '6' || tile == '8') gs_obs(&st, j, 5, ob, 3);   /* :286 */
        else if (tile == 'r') gs_obs(&st, j, 4, ob, 3);                           /* :297 */
    }

    for (j = 0; j < 8; j += 1) {                                              /* :311 tiles */
        for (i = 1; i < 11; i += 1) {
            char tile = gs_at(&st, i + j * 10);
            xpos = x + (i - 1) * 16;
            ypos = y + j * 16;
            if (tile == '1' && NOTSOL(xpos, ypos)) {                          /* :318 */
                if (RAND(1, 100) == 1) instance_create(xpos, ypos, OBJ_oLush);
                else if (RAND(1, 10) == 1) instance_create(xpos, ypos, OBJ_oBlock);
                else instance_create(xpos, ypos, OBJ_oTemple);
            } else if (tile == '2' && RAND(1, 2) == 1 && NOTSOL(xpos, ypos)) {
                if (RAND(1, 10) == 1) instance_create(xpos, ypos, OBJ_oBlock);
                else instance_create(xpos, ypos, OBJ_oTemple);
            } else if (tile == '3' && NOTSOL(xpos, ypos)) {
                if (RAND(1, 2) == 1) instance_create(xpos, ypos, OBJ_oLava);
                else instance_create(xpos, ypos, OBJ_oTemple);
            } else if (tile == 'R' && NOTSOL(xpos, ypos)) {                   /* :334 */
                obj = instance_create(xpos, ypos, OBJ_oTemple);
                W.in[obj].treasure = TR_BIGRUBY;
            } else if (tile == 'L') instance_create(xpos, ypos, OBJ_oLadderOrange);
            else if (tile == 'P') instance_create(xpos, ypos, OBJ_oLadderTop);
            else if (tile == '7' && RAND(1, 3) == 1) instance_create(xpos, ypos, OBJ_oSpikes);
            else if (tile == '4' && RAND(1, 4) == 1) instance_create(xpos, ypos, OBJ_oPushBlock);
            else if (tile == '9') tile_exit(room, xpos, ypos, OBJ_oTemple);
            else if (tile == 'a') instance_create(xpos + 8, ypos + 8, OBJ_oChest);
            else if (tile == 'c') {
                if (RAND(1, 2) == 1) instance_create(xpos + 8, ypos + 8, OBJ_oChest);
                else instance_create(xpos + 8, ypos + 8, OBJ_oCrate);
            } else if (tile == 't') {                                         /* :365 */
                if (RAND(1, 120) == 1) instance_create(xpos + 8, ypos + 12, OBJ_oRubyBig);
                else if (RAND(1, 80) == 1) instance_create(xpos + 8, ypos + 12, OBJ_oSapphireBig);
                else if (RAND(1, 60) == 1) instance_create(xpos + 8, ypos + 12, OBJ_oEmeraldBig);
                else instance_create(xpos + 8, ypos + 8, OBJ_oGoldBars);
            } else if (tile == 'x') tile_altar(xpos, ypos);
            else if (tile == 'X') {                                           /* :379 Lady Xoc */
                for (l = 0; l < 6; l += 1) {
                    for (k = 0; k < 5; k += 1) {
                        obj = instance_create(xpos + k * 16, ypos + l * 16, OBJ_oXocBlock);
                        if (k == 2 && l == 1) W.in[obj].treasure = TR_DIAMOND;
                        if (k == 1 && l == 2) W.in[obj].treasure = TR_SAPPHIRE;
                        if (k == 3 && l == 2) W.in[obj].treasure = TR_SAPPHIRE;
                        if (k == 0 && l == 3) W.in[obj].treasure = TR_EMERALD;
                        if (k == 4 && l == 3) W.in[obj].treasure = TR_EMERALD;
                        if (k == 2 && l == 4) W.in[obj].treasure = TR_RUBY;
                        tile_add(GSPR_bgLadyXoc, k * 16, l * 16, 16, 16, xpos + k * 16, ypos + l * 16, 99);
                    }
                }
            } else if (tile == 'I') instance_create(xpos + 16, ypos + 12, OBJ_oGoldIdol);
            else if (tile == ';') {                                           /* :400 */
                obj = instance_create(xpos + 8, ypos + 8, OBJ_oDamsel);
                W.in[obj].cost = 0;
                W.in[obj].flags &= (uint16_t)~IF_FORSALE;
                instance_create(xpos + 16 + 8, ypos + 12, OBJ_oGoldIdol);
            } else if (tile == 'B') {
                if (RAND(1, 1) == 1) instance_create(xpos, ypos, OBJ_oTrapBlock);
            } else if (tile == 'C') instance_create(xpos, ypos, OBJ_oCeilingTrap);
            else if (tile == 'D') {
                instance_create(xpos, ypos, OBJ_oDoor);
                instance_create(xpos, ypos, OBJ_oTempleFake);
                instance_create(xpos, ypos + 16, OBJ_oTempleFake);
            } else if (tile == 'A') tile_damsel_for_sale(xpos, ypos, &obj);
            else if (tile == '?') instance_create(xpos, ypos, OBJ_oTombLord);
            else if (tile == '.' && NOTSOL(xpos, ypos)) {                     /* :431 */
                obj = instance_create(xpos, ypos, OBJ_oTemple);
                W.in[obj].flags |= IF_SHOPWALL;
            } else if (tile == 'Q') {
                if (shopType == SHOP_CRAPS) tile_add(GSPR_bgDiceSign, 0, 0, 48, 32, xpos, ypos, 9004);
            } else if (tile == 'q') tile_q(room, xpos, ypos, &obj);
            else if (tile == '+') tile_icewall(xpos, ypos, &obj);
            else if (tile == 'W') tile_wanted(xpos, ypos);
            else if (tile == 'b') {
                obj = instance_create(xpos, ypos, OBJ_oTemple);
                W.in[obj].flags |= IF_SHOPWALL;
            } else if (tile == 'l') instance_create(xpos, ypos, OBJ_oLamp);
            else if (tile == 'p') instance_create(xpos, ypos, OBJ_oLampRed);
            else if (tile == 'K') tile_shopkeeper(xpos, ypos, shopType, &obj);
            else if (tile == 'k') tile_sign(xpos, ypos, shopType, &obj);
            else if (tile == 'i') scrShopItemsGen(room, xpos, ypos, shopType, &obj);
            else if (tile == 'w') instance_create(xpos, ypos, OBJ_oLava);
            else if (tile == 'u') instance_create(xpos + 8, ypos + 8, OBJ_oDice);
            else if (tile == 'd') instance_create(xpos, ypos, OBJ_oLush);
            else if (tile == 'e') {
                if (RAND(1, 2) == 1) instance_create(xpos, ypos, OBJ_oLush);
            } else if (tile == 'T') gen_tree(xpos, ypos);
        }
    }
}

/* ------------------------------------------------------------------------------------------------------------ */
/* scripts/scrRoomGen5/scrRoomGen5.gml: the Olmec level */
#define T5(l, k) GT(gt_scrRoomGen5, l, k)
void scrRoomGen5(int room)
{
    int x = W.in[room].x, y = W.in[room].y, n, i, j, xpos, ypos;
    struct gs st;

    gs_set(&st, T5(47, 0));
    (void)G.roomPath[scrGetRoomX(x)][scrGetRoomY(y)];                         /* :49 roomPath (unused) */
    if (y < 480) {                                                            /* :50 */
        n = RAND(1, 6);
        gs_set(&st, S5(52, n, 0));
    } else {
        n = RAND(1, 6);
        gs_set(&st, S5(64, n, 0));
    }

    for (i = 1; i < 81; i += 1) {                                             /* :77 obstacles */
        const char *ob[3] = { "00000", "00000", "00000" };
        char tile = gs_at(&st, i);
        j = i;
        if (tile == '8') {
            RAND(1, 1);
            obs_set(ob, &gt_scrRoomGen5_cases, 88, 1, 3);
        } else if (tile == '5') {
            n = RAND(1, 8);
            obs_set(ob, &gt_scrRoomGen5_cases, 95, n, 3);
        } else if (tile == '6') {
            n = RAND(1, 8);                                                   /* cases 1-6 only */
            if (n <= 6) obs_set(ob, &gt_scrRoomGen5_cases, 109, n, 3);
        }
        if (tile == '5' || tile == '6' || tile == '8') gs_obs(&st, j, 5, ob, 3);
    }

    for (j = 0; j < 8; j += 1) {                                              /* :134 tiles */
        for (i = 1; i < 11; i += 1) {
            char tile = gs_at(&st, i + j * 10);
            xpos = x + (i - 1) * 16;
            ypos = y + j * 16;
            if (tile == '1' && NOTSOL(xpos, ypos)) {
                if (RAND(1, 10) == 1) instance_create(xpos, ypos, OBJ_oBlock);
                else instance_create(xpos, ypos, OBJ_oTemple);
            } else if (tile == '2' && RAND(1, 2) == 1 && NOTSOL(xpos, ypos)) {
                if (RAND(1, 10) == 1) instance_create(xpos, ypos, OBJ_oBlock);
                else instance_create(xpos, ypos, OBJ_oTemple);
            } else if (tile == 'L') instance_create(xpos, ypos, OBJ_oVine);
            else if (tile == 'P') instance_create(xpos, ypos, OBJ_oVineTop);
            else if (tile == '7' && RAND(1, 3) == 1) instance_create(xpos, ypos, OBJ_oSpikes);
            else if (tile == '4' && RAND(1, 4) == 1) instance_create(xpos, ypos, OBJ_oPushBlock);
            else if (tile == '9') {                                           /* :161 */
                int block;
                if (scrGetRoomX(x) == G.startRoomX && scrGetRoomY(y) == G.startRoomY)
                    instance_create(xpos, ypos, OBJ_oEntrance);
                else {
                    instance_create(xpos, ypos, OBJ_oExit);
                    G.exitX = (int16_t)xpos;
                    G.exitY = (int16_t)ypos;
                }
                block = instance_create(xpos, ypos + 16, OBJ_oTemple);
                W.in[block].flags |= IF_INVINCIBLE;
            } else if (tile == 'a') {
                if (RAND(1, 1) == 1) instance_create(xpos + 8, ypos + 8, OBJ_oChest);
            } else if (tile == 'T') {                                         /* :178 */
                if (RAND(1, 15) == 1) instance_create(xpos + 8, ypos + 8, OBJ_oChest);
                else if (RAND(1, 6) == 1) instance_create(xpos + 8, ypos + 8, OBJ_oGoldBars);
                else if (RAND(1, 6) == 1) instance_create(xpos + 8, ypos + 12, OBJ_oEmeraldBig);
                else if (RAND(1, 8) == 1) instance_create(xpos + 8, ypos + 12, OBJ_oSapphireBig);
                else if (RAND(1, 10) == 1) instance_create(xpos + 8, ypos + 12, OBJ_oRubyBig);
                else if (RAND(1, 10) == 1) instance_create(xpos + 8, ypos + 8, OBJ_oCrate);
                else if (RAND(1, 10) == 1) instance_create(xpos, ypos, OBJ_oBlock);
                else instance_create(xpos, ypos, OBJ_oTemple);
            } else if (tile == 't') instance_create(xpos, ypos, OBJ_oThwompTrap);
            else if (tile == 'I') {
                if (RAND(1, 1) == 1) instance_create(xpos + 16, ypos, OBJ_oGoldIdol);
            } else if (tile == 'C') instance_create(xpos, ypos, OBJ_oCeilingTrap);
            else if (tile == 'D') {
                instance_create(xpos, ypos, OBJ_oTempleFake);
                instance_create(xpos, ypos + 16, OBJ_oTempleFake);
                instance_create(xpos, ypos, OBJ_oDoor);
            } else if (tile == 'w') instance_create(xpos, ypos, OBJ_oWaterSwim);
        }
    }
}

/* ------------------------------------------------------------------------------------------------------------ */
/* scripts/scrRoomGenMarket/scrRoomGenMarket.gml: the black market */
#define TM(l, k) GT(gt_scrRoomGenMarket, l, k)
void scrRoomGenMarket(int room)
{
    int x = W.in[room].x, y = W.in[room].y, n, m, i, j, xpos, ypos, roomPath, roomPathAbove, obj = INST_NONE;
    int shopType = -1;
    struct gs st;

    gs_set(&st, TM(47, 0));
    roomPath = G.roomPath[scrGetRoomX(x)][scrGetRoomY(y)];                    /* :49 */
    roomPathAbove = -1;
    if (scrGetRoomY(y) != 0) roomPathAbove = G.roomPath[scrGetRoomX(x)][scrGetRoomY(y - 128)];

    if (scrGetRoomX(x) == G.startRoomX && scrGetRoomY(y) == G.startRoomY) {  /* :53 */
        if (roomPath == 2) n = RAND(3, 4);
        else n = RAND(1, 2);
        gs_set(&st, SM(57, n, 0));
    } else if (scrGetRoomX(x) == G.endRoomX && scrGetRoomY(y) == G.endRoomY) {   /* :66 */
        if (roomPathAbove == 2) n = RAND(1, 2);
        else n = RAND(3, 4);
        gs_set(&st, SM(70, n, 0));
    } else if (roomPath == 1) {                                               /* :78 */
        n = RAND(1, 8);
        gs_set(&st, SM(80, n, 0));
    } else if (roomPath == 3) {                                               /* :96 */
        n = RAND(1, 7);
        gs_set(&st, SM(98, n, 0));
    } else if (roomPath == 4 && x == 496) {                                   /* :113 ankh shop */
        gs_set(&st, TM(115, 0));
        shopType = SHOP_ANKH;
    } else if (roomPath == 4) {                                               /* :118 */
        gs_set(&st, TM(120, 0));
        shopType = SHOP_EMPTY;
        n = RAND(1, 5);
        m = n;
        while (shopType == SHOP_EMPTY) {                                      /* :127 */
            if (n == 1) { if (!GAME.genSupplyShop) { shopType = SHOP_GENERAL; GAME.genSupplyShop = 1; } }
            else if (n == 2) { if (!GAME.genBombShop) { shopType = SHOP_BOMB; GAME.genBombShop = 1; } }
            else if (n == 3) { if (!GAME.genWeaponShop) { shopType = SHOP_WEAPON; GAME.genWeaponShop = 1; } }
            else if (n == 4) { if (!GAME.genRareShop) { shopType = SHOP_RARE; GAME.genRareShop = 1; } }
            else if (n == 5) { if (!GAME.genClothingShop) { shopType = SHOP_CLOTHING; GAME.genClothingShop = 1; } }
            n += 1;
            if (n > 5) n = 1;
            if (n == m) {
                shopType = SHOP_GENERAL;
                break;
            }
        }
    } else if (roomPath == 5) {                                               /* :143 casino */
        gs_set(&st, TM(145, 0));
        shopType = SHOP_CRAPS;
    } else {                                                                  /* :148 drop */
        if (roomPathAbove != 2) n = RAND(1, 6);
        else n = RAND(1, 5);
        gs_set(&st, SM(152, n, 0));
    }

    for (i = 1; i < 81; i += 1) {                                             /* :166 obstacles */
        const char *ob[4] = { "00000", "00000", "00000", "00000" };
        char tile = gs_at(&st, i);
        j = i;
        if (tile == '8') {
            RAND(1, 1);
            obs_set(ob, &gt_scrRoomGenMarket_cases, 179, 1, 3);
        } else if (tile == '5') {
            if (RAND(1, 8) == 1) n = RAND(100, 102);
            else n = RAND(1, 2);
            obs_set(ob, &gt_scrRoomGenMarket_cases, 188, n, 3);
        } else if (tile == '6') {
            n = RAND(1, 4);
            obs_set(ob, &gt_scrRoomGenMarket_cases, 200, n, 3);
        } else if (tile == 'V') {
            n = RAND(1, 3);
            obs_set(ob, &gt_scrRoomGenMarket_cases, 211, n, 4);
        }
        if (tile == '5' || tile == '6' || tile == '8' || tile == 'V')
            gs_obs(&st, j, 5, ob, tile == 'V' ? 4 : 3);
    }

    for (j = 0; j < 8; j += 1) {                                              /* :239 tiles */
        for (i = 1; i < 11; i += 1) {
            char tile = gs_at(&st, i + j * 10);
            xpos = x + (i - 1) * 16;
            ypos = y + j * 16;
            if (tile == '1' && NOTSOL(xpos, ypos)) instance_create(xpos, ypos, OBJ_oLush);
            else if (tile == '2' && RAND(1, 2) == 1 && NOTSOL(xpos, ypos)) instance_create(xpos, ypos, OBJ_oLush);
            if (tile == 't' && NOTSOL(xpos, ypos)) instance_create(xpos, ypos, OBJ_oTemple);   /* :254 */
            else if (tile == '3' && NOTSOL(xpos, ypos)) {
                if (RAND(1, 2) == 1) instance_create(xpos, ypos, OBJ_oWaterSwim);
                else instance_create(xpos, ypos, OBJ_oLush);
            } else if (tile == 'L') instance_create(xpos, ypos, OBJ_oVine);
            else if (tile == 'P') instance_create(xpos, ypos, OBJ_oVineTop);
            else if (tile == 'G') instance_create(xpos, ypos, OBJ_oLadderOrange);
            else if (tile == 'H') instance_create(xpos, ypos, OBJ_oLadderTop);
            else if (tile == '7' && RAND(1, 3) == 1) instance_create(xpos, ypos, OBJ_oSpikes);
            else if (tile == 's') instance_create(xpos, ypos, OBJ_oSpikes);
            else if (tile == '4') instance_create(xpos, ypos, OBJ_oPushBlock);
            else if (tile == '9') tile_exit(room, xpos, ypos, OBJ_oLush);
            else if (tile == 'c') instance_create(xpos, ypos, OBJ_oChest);
            else if (tile == 'd') {
                instance_create(xpos, ypos, OBJ_oWaterSwim);
                instance_create(xpos, ypos, OBJ_oChest);
            } else if (tile == 'w') instance_create(xpos, ypos, OBJ_oWaterSwim);
            else if (tile == 'I') instance_create(xpos + 16, ypos + 8, OBJ_oGoldIdol);
            else if (tile == '.' && NOTSOL(xpos, ypos)) {
                obj = instance_create(xpos, ypos, OBJ_oLush);
                W.in[obj].flags |= IF_SHOPWALL;
            } else if (tile == '+') tile_icewall(xpos, ypos, &obj);
            else if (tile == 'q') tile_q(room, xpos, ypos, &obj);
            else if (tile == 'Q') {
                if (shopType == SHOP_CRAPS) tile_add(GSPR_bgDiceSign, 0, 0, 48, 32, xpos, ypos, 9004);
            } else if (tile == 'W') tile_wanted(xpos, ypos);
            else if (tile == 'b') tile_smooth(xpos, ypos, GSPR_sLushSmooth, &obj);
            else if (tile == 'l') obj = instance_create(xpos, ypos, OBJ_oLamp);
            else if (tile == 'K') tile_shopkeeper(xpos, ypos, shopType, &obj);
            else if (tile == 'k') {
                /* :348: the sign's sprite for General .. Craps (no Kissing branch here) */
                static const int16_t msign[6] = { GSPR_sSignGeneral, GSPR_sSignBomb, GSPR_sSignWeapon,
                                                  GSPR_sSignRare, GSPR_sSignClothing, GSPR_sSignCraps };
                obj = instance_create(xpos, ypos, OBJ_oSign);
                if (shopType >= SHOP_GENERAL && shopType <= SHOP_CRAPS) inst_set_sprite(obj, msign[shopType]);
            } else if (tile == 'i') scrShopItemsGen(room, xpos, ypos, shopType, &obj);
            else if (tile == 'A') obj = instance_create(xpos + 8, ypos + 8, OBJ_oAnkh);
            else if (tile == 'z') instance_create(xpos + 8, ypos + 8, OBJ_oDice);
            else if (tile == 'B') instance_create(xpos, ypos, OBJ_oTrapBlock);
            else if (tile == 'p') {
                RAND(1, 2);                                                   /* if (rand(1,2)): always true */
                instance_create(xpos, ypos, OBJ_oFakeBones);
            } else if (tile == 'T') gen_tree(xpos, ypos);
        }
    }
}

/* ------------------------------------------------------------------------------------------------------------ */
/* scripts/scrRoomGenYeti/scrRoomGenYeti.gml: the yeti lair */
#define TY(l, k) GT(gt_scrRoomGenYeti, l, k)
void scrRoomGenYeti(int room)
{
    int x = W.in[room].x, y = W.in[room].y, n, i, j, xpos, ypos, roomPath, roomPathAbove, obj = INST_NONE;
    int shopType = -1;
    struct gs st;

    gs_set(&st, TY(47, 0));
    roomPath = G.roomPath[scrGetRoomX(x)][scrGetRoomY(y)];                    /* :49 */
    roomPathAbove = -1;
    if (scrGetRoomY(y) != 0) roomPathAbove = G.roomPath[scrGetRoomX(x)][scrGetRoomY(y - 128)];

    if (scrGetRoomX(x) == G.startRoomX && scrGetRoomY(y) == G.startRoomY) {  /* :53 */
        if (roomPath == 2) n = RAND(3, 4);
        else n = RAND(1, 2);
        gs_set(&st, SY(57, n, 0));
    } else if (scrGetRoomX(x) == G.endRoomX && scrGetRoomY(y) == G.endRoomY) {   /* :66 */
        if (roomPathAbove == 2) n = RAND(1, 4);
        else n = RAND(3, 6);
        gs_set(&st, SY(70, n, 0));
    } else if (roomPath == 0 && RAND(1, 4) < 4) {                             /* :81 side room */
        if (GAME.idol || scrGetRoomY(y) == 3) n = RAND(1, 2);
        else {
            n = RAND(1, 1);
            if (n == 10) GAME.idol = 1;
        }
        gs_set(&st, SY(92, n, 0));
    } else if (roomPath == 0 || roomPath == 1) {                              /* :102 */
        if (!instance_exists(OBJ_oYetiKing)) {
            n = RAND(1, 10);
            if (y > 384) n = 10;
            else if (y > 256 && RAND(1, 2) == 1) n = 10;
            else if (y > 128 && RAND(1, 3) == 1) n = 10;
        } else n = RAND(1, 9);
        gs_set(&st, SY(113, n, 0));
    } else if (roomPath == 3) {                                               /* :131 */
        n = RAND(1, 9);
        gs_set(&st, SY(133, n, 0));
    } else if (roomPath == 4) {                                               /* :150 shop */
        gs_set(&st, TY(153, 0));
        shopType = shop_pick7();
        if (shopType == SHOP_CRAPS) gs_set(&st, SY(155, 6, 0));
        if (shopType == SHOP_KISSING) { gs_set(&st, SY(155, 7, 0)); GAME.damsel = 1; }
    } else if (roomPath == 5) {                                               /* :167 shop */
        gs_set(&st, TY(170, 0));
        shopType = shop_pick7();
        if (shopType == SHOP_CRAPS) gs_set(&st, SY(172, 6, 0));
        if (shopType == SHOP_KISSING) { gs_set(&st, SY(172, 7, 0)); GAME.damsel = 1; }
    } else if (roomPath == 8) {                                               /* :183 */
        RAND(1, 1);
        gs_set(&st, SY(185, 1, 0));
    } else if (roomPath == 9) {
        RAND(1, 1);
        gs_set(&st, SY(192, 1, 0));
    } else {                                                                  /* :197 drop */
        if (roomPath == 7) n = RAND(4, 12);
        else if (roomPathAbove != 2) n = RAND(1, 12);
        else n = RAND(1, 8);
        gs_set(&st, SY(202, n, 0));
    }

    for (i = 1; i < 81; i += 1) {                                             /* :220 obstacles */
        const char *ob[3] = { "00000", "00000", "00000" };
        char tile = gs_at(&st, i);
        j = i;
        if (tile == '8') {
            n = RAND(1, 7);
            obs_set(ob, &gt_scrRoomGenYeti_cases, 231, n, 3);
        } else if (tile == '5') {
            n = RAND(1, 16);
            obs_set(ob, &gt_scrRoomGenYeti_cases, 244, n, 3);
        } else if (tile == '6') {
            n = RAND(1, 10);
            obs_set(ob, &gt_scrRoomGenYeti_cases, 266, n, 3);
        }
        if (tile == '5' || tile == '6' || tile == '8') gs_obs(&st, j, 5, ob, 3);
    }

    for (j = 0; j < 8; j += 1) {                                              /* :295 tiles */
        for (i = 1; i < 11; i += 1) {
            char tile = gs_at(&st, i + j * 10);
            xpos = x + (i - 1) * 16;
            ypos = y + j * 16;
            if (tile == '1' && NOTSOL(xpos, ypos)) {                          /* :302 */
                if (RAND(1, 6) == 1) instance_create(xpos, ypos, OBJ_oDark);
                else instance_create(xpos, ypos, OBJ_oIce);
            }
            if (tile == '2' && RAND(1, 2) == 1 && NOTSOL(xpos, ypos)) {       /* :307 */
                if (RAND(1, 6) == 1) instance_create(xpos, ypos, OBJ_oDark);
                else instance_create(xpos, ypos, OBJ_oIce);
            }
            if (tile == 't' && NOTSOL(xpos, ypos)) instance_create(xpos, ypos, OBJ_oThinIce);   /* :312 */
            else if (tile == 'L') instance_create(xpos, ypos, OBJ_oLadderOrange);
            else if (tile == 'P') instance_create(xpos, ypos, OBJ_oLadderTop);
            else if (tile == '7' && RAND(1, 3) == 1) instance_create(xpos, ypos, OBJ_oSpikes);
            else if (tile == '4' && RAND(1, 4) == 1) instance_create(xpos, ypos, OBJ_oPushBlock);
            else if (tile == '9') tile_exit(room, xpos, ypos, OBJ_oDark);
            else if (tile == 'A') {
                instance_create(xpos, ypos, OBJ_oAltarLeft);
                instance_create(xpos + 16, ypos, OBJ_oAltarRight);
            } else if (tile == 'a') instance_create(xpos, ypos, OBJ_oChest);
            else if (tile == 'I') instance_create(xpos + 16, ypos + 8, OBJ_oGoldIdol);
            else if (tile == 'B') instance_create(xpos + 16, ypos + 12, OBJ_oGiantTikiHead);
            else if (tile == '.' && NOTSOL(xpos, ypos)) {
                obj = instance_create(xpos, ypos, OBJ_oDark);
                W.in[obj].flags |= IF_SHOPWALL;
            } else if (tile == 'Q') {
                if (shopType == SHOP_CRAPS) tile_add(GSPR_bgDiceSign, 0, 0, 48, 32, xpos, ypos, 9004);
            } else if (tile == 'q') tile_q(room, xpos, ypos, &obj);
            else if (tile == '+') tile_icewall(xpos, ypos, &obj);
            else if (tile == 'W') tile_wanted(xpos, ypos);
            else if (tile == 'b') tile_smooth(xpos, ypos, GSPR_sDarkSmooth, &obj);
            else if (tile == 'l') {
                if (GAME.damsel) instance_create(xpos, ypos, OBJ_oLampRed);
                else instance_create(xpos, ypos, OBJ_oLamp);
            } else if (tile == 'K') tile_shopkeeper(xpos, ypos, shopType, &obj);
            else if (tile == 'k') tile_sign(xpos, ypos, shopType, &obj);
            else if (tile == '$') scrShopItemsGen(room, xpos, ypos, shopType, &obj);
            else if (tile == 'd') instance_create(xpos + 8, ypos + 8, OBJ_oDice);
            else if (tile == 'D') tile_damsel_for_sale(xpos, ypos, &obj);
            else if (tile == 's') instance_create(xpos, ypos, OBJ_oSpikes);
            else if (tile == 'S') instance_create(xpos, ypos, OBJ_oSnake);
            else if (tile == 'T') instance_create(xpos + 8, ypos + 8, OBJ_oRubyBig);
            else if (tile == 'i') instance_create(xpos, ypos, OBJ_oIce);
            else if (tile == 'j' && RAND(1, 2) == 1) instance_create(xpos, ypos, OBJ_oIce);
            else if (tile == 'Y') instance_create(xpos, ypos, OBJ_oYetiKing);
            else if (tile == 'y') instance_create(xpos, ypos, OBJ_oYeti);
        }
    }
}
