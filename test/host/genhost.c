/* Host harness for the level generator: runs the cases of a cases file (tools/tracer.py generator mode format:
 * "<seed> <level> <cont> <noDarkLevel> [name=value ...]") and prints each generated level as text for
 * tools/gencmp.py.
 *
 *   build/host/genhost <cases.txt> [--selftest] > out.txt
 *
 * Output per case:
 *   C <case> <seed> <level> <status>         status 0 = generated, -1 = untranslated GML reached (code: U line)
 *   U <code>
 *   G <name> <value>                          globals after scrInitLevel
 *   R <20 x roomPath[i][j], i = 0..3, j = 0..4>
 *   I <id> <object> <x> <y> <sprite> <depth> <alarm k=v,...|-> <name>=<value> ...
 *     (creation order, alive instances; room instances with their room ids, generated ones from id 0)
 *   T <background> <left> <top> <w> <h> <x> <y> <depth>          tile_add tiles in order
 *   D <4 raw RNG words after generation>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gen.h"
#include "rng.h"

static const char *const style_names[] = { "General", "Bomb", "Weapon", "Rare", "Clothing", "Craps", "Kissing",
                                           "Ankh", "" };
/* string values: "~" for a space (tools/gencmp.py reads it back) */
static const char *const treasure_names[] = { "", "Big~Ruby", "Diamond", "Sapphire", "Emerald", "Ruby" };
static const char *const exit_names[] = { "Exit", "Moai~Exit", "Market~Exit" };

/* globals settable by a case override (name=value) */
struct gvar { const char *name; uint8_t *u8; int16_t *s16; };
static const struct gvar gvars[] = {
#define U8(n) { #n, &G.n, 0 }
#define S16(n) { #n, 0, &G.n }
    U8(hadDarkLevel), U8(darkLevel), U8(noDarkLevel), U8(lake), U8(cityOfGold), U8(cemetary), U8(blackMarket),
    U8(madeBlackMarket), U8(genBlackMarket), U8(madeMoai), U8(madeUdjatEye), U8(madeMarketEntrance),
    U8(murderer), U8(isDamsel), U8(isTunnelMan), U8(hasSpectacles), U8(graphicsHigh), U8(madeGoldEntrance),
    S16(thiefLevel), S16(kaliPunish), S16(probDarkLevel), S16(probSnakePit), S16(probCemetary), S16(probSacPit),
    S16(probAlien), S16(probYetiLair), S16(marketChance), S16(goldChance),
    { 0, 0, 0 }
};

static int set_global(const char *name, const char *val)
{
    const struct gvar *v;
    if (!strcmp(name, "pickupItem")) {
        int k;
        char s[64];
        int c;
        strncpy(s, !strncmp(val, "s:", 2) ? val + 2 : val, sizeof s - 1);
        s[sizeof s - 1] = 0;
        for (c = 0; s[c]; c++)
            if (s[c] == '~') s[c] = ' ';               /* the case file's space */
        for (k = 0; k < PICK_OTHER; k++)
            if (!strcmp(pickup_names[k], s)) { G.pickupItem = (uint8_t)k; return 0; }
        G.pickupItem = PICK_OTHER;
        return 0;
    }
    for (v = gvars; v->name; v++)
        if (!strcmp(v->name, name)) {
            if (v->u8) *v->u8 = (uint8_t)atoi(val);
            else *v->s16 = (int16_t)atoi(val);
            return 0;
        }
    fprintf(stderr, "genhost: unknown global %s\n", name);
    return -1;
}

static void dump(int k, long seed, int level, int st)
{
    int i, a;
    printf("C %d %ld %d %d\n", k, seed, level, st);
    if (st) printf("U %d\n", gen_untranslated);
#define PG(n) printf("G %s %d\n", #n, (int)G.n)
    PG(darkLevel); PG(hadDarkLevel); PG(genUdjatEye); PG(madeUdjatEye); PG(genMarketEntrance); PG(snakePit);
    PG(shop); PG(startRoomX); PG(startRoomY); PG(endRoomX); PG(endRoomY); PG(exitX); PG(exitY); PG(cemetary);
    PG(giantSpider); PG(genGiantSpider); PG(LockedChest); PG(Key); PG(lockedChestChance); PG(blackMarket);
    PG(sacrificePit); PG(alienCraft); PG(yetiLair); PG(levelType); PG(noDarkLevel); PG(lake); PG(marketChance);
    PG(madeMarketEntrance); PG(madeBlackMarket); PG(madeMoai); PG(ashGrave); PG(TombLord); PG(genTombLord);
    PG(genGoldEntrance); PG(madeGoldEntrance); PG(goldChance); PG(cityOfGold); PG(thiefLevel); PG(murderer);
    PG(checkWater); PG(cleanSolids);
    printf("G oGame.damsel %d\nG oGame.idol %d\nG oGame.altar %d\n", GAME.damsel, GAME.idol, GAME.altar);
    printf("G oGame.genSupplyShop %d\nG oGame.genBombShop %d\nG oGame.genWeaponShop %d\nG oGame.genRareShop %d\n"
           "G oGame.genClothingShop %d\n", GAME.genSupplyShop, GAME.genBombShop, GAME.genWeaponShop,
           GAME.genRareShop, GAME.genClothingShop);
    printf("R");
    for (i = 0; i < 4; i++)
        for (a = 0; a < 5; a++)
            printf(" %d", G.roomPath[i][a]);
    printf("\n");
    for (i = 0; i < W.n; i++) {
        const struct inst *p = &W.in[i];
        int any = 0;
        if (!p->alive)
            continue;
        printf("I %ld %s %d %d %s %d ", (long)p->id, objdefs[p->obj].name, p->x, p->y,
               p->spr >= 0 ? gsprname[p->spr] : "-", p->depth);
        for (a = 0; a < ALARMS; a++)
            if (p->alarm[a] != -1) {
                printf("%s%d=%d", any ? "," : "", a, p->alarm[a]);
                any = 1;
            }
        printf("%s invincible=%d status=%d cost=%ld forSale=%d shopWall=%d value=%ld inDiceHouse=%d cleanDeath=%d"
               " xVel=%d yVel=%d facing=%d counter=%d spurt=%d deathTimer=%d held=%d swimming=%d dir=%d"
               " spurtTime=%d shiftToggle=%d New=%d linkVal=%d style=%s treasure=%s type=%s\n",
               any ? "" : "-", !!(p->flags & IF_INVINCIBLE), p->status, (long)p->cost, !!(p->flags & IF_FORSALE),
               !!(p->flags & IF_SHOPWALL), (long)p->value, !!(p->flags & IF_INDICEHOUSE),
               !!(p->flags & IF_CLEANDEATH), p->xvel, p->yvel, p->facing, p->counter, !!(p->flags & IF_SPURT),
               p->deathtimer, !!(p->flags & IF_HELD), !!(p->flags & IF_SWIMMING), p->dir, p->spurttime,
               p->shifttoggle, !!(p->flags & IF_NEW), p->linkval, style_names[p->style],
               treasure_names[p->treasure], exit_names[p->etype]);
    }
    for (i = 0; i < gntiles; i++) {
        const struct gtile *t = &gtiles[i];
        printf("T %s %d %d %d %d %d %d %d\n", gsprname[t->bg], t->left, t->top, t->w, t->h, t->x, t->y, t->depth);
    }
    printf("D %lu %lu %lu %lu\n", (unsigned long)rng_next(&g_rng), (unsigned long)rng_next(&g_rng),
           (unsigned long)rng_next(&g_rng), (unsigned long)rng_next(&g_rng));
}

int main(int argc, char **argv)
{
    FILE *f;
    char line[512];
    int k = 0, check;
    if (argc < 2) {
        fprintf(stderr, "usage: genhost <cases.txt> [--selftest]\n");
        return 2;
    }
    check = argc > 2;
    f = fopen(argv[1], "r");
    if (!f) {
        perror(argv[1]);
        return 2;
    }
    gen_new_game();
    while (fgets(line, sizeof line, f)) {
        long seed;
        int level, cont = 0, nodark = 1, npos = 0, st;
        char *h = line, *tok;
        while (*h && *h != '#') h++;
        *h = 0;
        /* positional fields first, name=value overrides after scrClearGlobals */
        {
            char *toks[32];
            int nt = 0, t;
            for (tok = strtok(line, " \t\r\n"); tok && nt < 32; tok = strtok(0, " \t\r\n"))
                toks[nt++] = tok;
            if (nt < 2)
                continue;
            for (t = 0; t < nt; t++) {
                if (strchr(toks[t], '=')) continue;
                if (npos == 0) seed = atol(toks[t]);
                else if (npos == 1) level = atoi(toks[t]);
                else if (npos == 2) cont = atoi(toks[t]);
                else if (npos == 3) nodark = atoi(toks[t]);
                npos++;
            }
            if (!cont) {
                gen_new_game();
            }
            G.currLevel = (int16_t)level;
            G.noDarkLevel = (uint8_t)nodark;
            for (t = 0; t < nt; t++) {
                char *eq = strchr(toks[t], '=');
                if (!eq) continue;
                *eq = 0;
                set_global(toks[t], eq + 1);
            }
        }
        G.gameStart = 1;
        rng_seed(&g_rng, (uint32_t)seed);
        st = gen_level(0);
        if (check && inst_selftest())
            fprintf(stderr, "case %d: grid self-test failed\n", k);
        dump(k, seed, level, st);
        k++;
    }
    fclose(f);
    return 0;
}
