/* Host harness for the level generator: runs the cases of a cases file (tools/tracer.py generator mode format:
 * "<seed> <level> <cont> <noDarkLevel>") and prints each generated level as text for tools/gencmp.py.
 *
 *   build/host/genhost <cases.txt> > out.txt
 *
 * Output per case:
 *   C <case> <seed> <level> <status>         status 0 = generated, -1 = not translated
 *   G <name> <value>                          globals after scrInitLevel (tools/tracer.py GEN_GLOBALS subset)
 *   R <16 x roomPath[i][j], i = 0..3, j = 0..3>
 *   I <id> <object> <x> <y> <sprite> <depth> <alarm k=v,...|-> <invincible> <status> <cost> <forSale>
 *     <shopWall> <value> <inDiceHouse> <cleanDeath> <xVel*256> <yVel*256> <facing> <counter> <style>
 *     (creation order; room instances with their room ids, generated ones from id 0: relative ids)
 *   D <4 raw RNG words after generation>
 */
#include <stdio.h>
#include <stdlib.h>
#include "gen.h"
#include "rng.h"

static const char *const style_names[] = { "General", "Bomb", "Weapon", "Rare", "Clothing", "Craps", "Kissing" };

static void dump(int k, long seed, int level, int st)
{
    int i, a;
    printf("C %d %ld %d %d\n", k, seed, level, st);
#define PG(n) printf("G %s %d\n", #n, (int)G.n)
    PG(darkLevel); PG(hadDarkLevel); PG(genUdjatEye); PG(madeUdjatEye); PG(snakePit); PG(shop);
    PG(startRoomX); PG(startRoomY); PG(endRoomX); PG(endRoomY); PG(exitX); PG(exitY); PG(giantSpider);
    PG(genGiantSpider); PG(LockedChest); PG(Key); PG(lockedChestChance); PG(levelType);
    printf("G oGame.damsel %d\nG oGame.idol %d\nG oGame.altar %d\n", GAME.damsel, GAME.idol, GAME.altar);
    printf("R");
    for (i = 0; i < 4; i++)
        for (a = 0; a < 4; a++)
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
        printf("%s %d %d %ld %d %d %ld %d %d %d %d %d %d %s\n", any ? "" : "-", !!(p->flags & IF_INVINCIBLE),
               p->status, (long)p->cost, !!(p->flags & IF_FORSALE), !!(p->flags & IF_SHOPWALL), (long)p->value,
               !!(p->flags & IF_INDICEHOUSE), !!(p->flags & IF_CLEANDEATH), p->xvel, p->yvel, p->facing,
               p->counter, style_names[p->style]);
    }
    printf("D %lu %lu %lu %lu\n", (unsigned long)rng_next(&g_rng), (unsigned long)rng_next(&g_rng),
           (unsigned long)rng_next(&g_rng), (unsigned long)rng_next(&g_rng));
}

int main(int argc, char **argv)
{
    FILE *f;
    char line[256];
    int k = 0, check = 0;
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
    while (fgets(line, sizeof line, f)) {
        long seed;
        int level, cont = 0, nodark = 1, n, st;
        char *h = line;
        while (*h && *h != '#') h++;
        *h = 0;
        n = sscanf(line, "%ld %d %d %d", &seed, &level, &cont, &nodark);
        if (n < 2)
            continue;
        if (!cont)
            gen_new_game();
        G.currLevel = (int16_t)level;
        G.noDarkLevel = (uint8_t)nodark;
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
