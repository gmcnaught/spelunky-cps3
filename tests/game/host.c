/* tests/game host check: the game program's play loop and display list (src/main/game.c, src/draw, src/hud, the
 * same src/game snapshot) on the host, every step. The SDK's video calls are replaced by a recorder, and the list is
 * composed as the CPS3 shows it (tilemaps by their scroll, sprites with flips, colour code per entry) into the
 * 320 x 240 view (screen line s = view line s + 8). tools/drawmodel.py hostcmp compares those views with its model
 * of the runner's trace records (scripts/game_check.sh --host).
 *   host <route.txt> <seed> <level> <money> <enemies> <tail> <gen dir> <out dir> [rec,rec,... | all] [nohud|hud]
 *        [dark a8]
 * Output: <out dir>/v_<rec>.bin, 320 x 240 little-endian u16 per pixel: colour code << 8 | colour index (0: nothing
 * drawn); out dir "-": the frames on stdout, each a little-endian s32 record number, s32 alpha byte a8 of colour
 * code DRAW_PAL's fade (tools/darkfade.py; 0 but on dark levels), then the 320 x 240 words (for
 * tools/drawmodel.py hostcmp - : no files); stderr: "D <rec> <entries> <drawables>" per frame. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cps3.h"
#include "shell.h"
#include "sprites.h"
#include "hudart.h"
#include "pint.h"
#include "draw.h"
#include "game.h"
#include "fade.h"

struct shell SH;
volatile uint32_t vbl_count;
void main_draw_begin(void) {}
void main_draw_end(void) {}

/* ---- the recorder ---- */
static uint32_t maps[128][64 * 64];              /* tilemap units (4 KB each: 64 x 64 cells of 4 bytes over 4 units) */
static struct { int x, y; uint32_t unit; int on; } tm[4];
struct lent { int band, tm; int x, y, w, h; uint32_t tile, pal, flags; };
static struct lent list[4096];
static int nlist;
void cps3v_cell(uint32_t unit, int col, int row, uint32_t tile, uint32_t pal, uint32_t flags)
{
    maps[unit & 127][(row & 63) * 64 + (col & 63)] = tile << 16 | (pal & 0x1ff) | (flags & CPS3V_FLIPX);
}
void cps3v_tilemap(int t, int map_x, int map_y, uint32_t unit, int enable)
{
    tm[t].x = map_x;
    tm[t].y = map_y;
    tm[t].unit = unit;
    tm[t].on = enable;
}
static int host_a8;                              /* the faded palette code DRAW_PAL holds (draw.c's palette DMA) */
uint32_t cps3dma_palette(uint32_t src, uint32_t first, uint32_t n, uint32_t fade)
{
    (void)n; (void)fade;
    if (first == DRAW_PAL * 256 && src >= DARK_FADE_AT) host_a8 = (int)((src - DARK_FADE_AT) / 512);
    /* DRAW_PAL_HUDDARK follows the same a8 (draw.c writes both) */
    return 0;
}
void cps3v_begin(void) { nlist = 0; }
void cps3v_group(void) {}
void cps3v_end(void) {}
void cps3v_band(int t, int top, int lines)
{
    if (nlist < 4096) { list[nlist].band = 1; list[nlist].tm = t; list[nlist].y = top; list[nlist].h = lines; nlist++; }
}
void cps3v_sprite(int x, int y, int w, int h, uint32_t tile, uint32_t pal, uint32_t flags)
{
    if (nlist < 4096) {
        struct lent *e = &list[nlist++];
        e->band = 0; e->x = x; e->y = y; e->w = w; e->h = h; e->tile = tile; e->pal = pal; e->flags = flags;
    }
}

/* ---- composition ---- */
static uint8_t *gfx, *hud;
static uint16_t view[240][320];
static const uint8_t *tile_px(uint32_t t)
{
    if (t >= GFX_FIRST_TILE && t < GFX_FIRST_TILE + GFX_NTILES) return gfx + 256 * (t - GFX_FIRST_TILE);
    if (t >= HUD_FIRST_TILE && t < HUD_FIRST_TILE + HUD_NTILES) return hud + 256 * (t - HUD_FIRST_TILE);
    return NULL;                                  /* the blank tile */
}
static void put(int X, int Y, uint32_t pal, uint8_t v)
{
    if (v && X >= 0 && X < 320 && Y >= 8 && Y < 232) view[Y][X] = (uint16_t)(pal << 8 | v);
}
static void compose(void)
{
    int k, X, Y;
    memset(view, 0, sizeof view);
    for (k = 0; k < nlist; k++) {
        const struct lent *e = &list[k];
        if (e->band) {
            if (!tm[e->tm].on) continue;
            for (Y = 8 + e->y; Y < 8 + e->y + e->h && Y < 232; Y++)
                for (X = 0; X < 320; X++) {
                    int mx = (X + tm[e->tm].x) & 1023, my = (Y - 8 + tm[e->tm].y) & 1023;
                    uint32_t c = maps[tm[e->tm].unit & 127][(my >> 4) * 64 + (mx >> 4)];
                    const uint8_t *p = tile_px(c >> 16);
                    int u = mx & 15, v = my & 15;
                    if (!p) continue;
                    if (c & CPS3V_FLIPX) u = 15 - u;
                    put(X, Y, c & 0x1ff, p[16 * v + u]);
                }
        } else {
            int i, j, u, v;
            for (i = 0; i < e->w; i++)
                for (j = 0; j < e->h; j++) {
                    const uint8_t *p = tile_px(e->tile + i * e->h + j);
                    int tx = (e->flags & CPS3V_FLIPX) ? e->w - 1 - i : i;
                    if (!p) continue;
                    for (v = 0; v < 16; v++)
                        for (u = 0; u < 16; u++)
                            put(e->x + 16 * tx + u, e->y + 8 + 16 * j + v, e->pal,
                                p[16 * v + ((e->flags & CPS3V_FLIPX) ? 15 - u : u)]);
                }
        }
    }
}

static uint8_t *slurp(const char *dir, const char *name)
{
    char path[512];
    FILE *f;
    long n;
    uint8_t *b;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    f = fopen(path, "rb");
    if (!f) { perror(path); exit(2); }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc((size_t)n);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) exit(2);
    fclose(f);
    return b;
}

int main(int argc, char **argv)
{
    static uint16_t masks[100000];
    static int want[100000];
    char line[256];
    int n = 0, all = 0;
    FILE *f;
    if (argc < 9) {
        fprintf(stderr, "usage: host <route.txt> <seed> <level> <money> <enemies> <tail> <gen dir> <out dir> [recs|all]\n");
        return 2;
    }
    f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 2; }
    while (fgets(line, sizeof line, f)) {
        char *h = strchr(line, '#'), keys[64] = "-";
        int c;
        uint16_t m = 0;
        if (h) *h = 0;
        if (sscanf(line, "%d %63s", &c, keys) < 1) continue;
        for (h = keys; *h; h++) {
            static const char L[] = "RLUDJAINBOFPS";
            const char *q = strchr(L, *h);
            if (q) m |= (uint16_t)(1u << (q - L));
        }
        while (c-- > 0 && n < 100000) masks[n++] = m;
    }
    fclose(f);
    if (argc > 10 && !strcmp(argv[10], "nohud")) draw_hud_on = 0;
    if (argc > 11) draw_dark_force = (int16_t)atoi(argv[11]);   /* a8: the fade path on any level */
    if (argc > 9 && !strcmp(argv[9], "all")) all = 1;
    else if (argc > 9) {
        char *s = argv[9];
        while (*s) {
            int r = (int)strtol(s, &s, 10);
            if (r >= 0 && r < 100000) want[r] = 1;
            if (*s) s++;
        }
    }
    gfx = slurp(argv[7], "gfx.bin");
    hud = slurp(argv[7], "hud.bin");
    game_cfg.route = masks;
    game_cfg.nroute = n;
    game_cfg.seed = (uint32_t)strtoul(argv[2], 0, 10);
    game_cfg.level = atoi(argv[3]);
    game_cfg.money = atoi(argv[4]);
    game_cfg.enemies = atoi(argv[5]);
    game_cfg.tail = atoi(argv[6]);
    draw_boot();
    game_begin();
    for (;;) {
        struct shell_input in = { 0, 0, 0 };
        int over = game_step(&in);
        cps3v_begin();
        game_draw();
        cps3v_end();
        draw_vblank();
        if (game_rec1 >= 0 && (all || want[game_rec1])) {
            char path[512];
            FILE *o;
            compose();
            if (!strcmp(argv[8], "-")) {
                int32_t r = game_rec1, a = host_a8;
                fwrite(&r, 4, 1, stdout);         /* host byte order: little-endian */
                fwrite(&a, 4, 1, stdout);         /* the fade of colour code DRAW_PAL this frame (dark levels) */
                fwrite(view, 2, 320 * 240, stdout);
            } else {
                snprintf(path, sizeof path, "%s/v_%d.bin", argv[8], game_rec1);
                o = fopen(path, "wb");
                if (!o) { perror(path); return 2; }
                fwrite(view, 2, 320 * 240, o);
                fclose(o);
            }
            fprintf(stderr, "D %d %u %u\n", game_rec1, draw_st.entries, draw_st.sprites);
        }
        if (over) break;
    }
    return 0;
}
