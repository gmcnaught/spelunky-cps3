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
#include "front.h"
#include "../../src/snd/snd.h"
#include "cps3s.h"

struct shell SH;
volatile uint32_t vbl_count;
/* src/shell/hiscore.c's EEPROM (scrUpdateHighscores' store, src/main game.c): words in memory, HOST_EE=1 prints the
   writes on stderr */
static uint32_t ee[64];
uint32_t shell_ee_read(int word) { return ee[word & 63]; }
void shell_ee_write(int word, uint32_t v)
{
    ee[word & 63] = v;
    if (getenv("HOST_EE")) fprintf(stderr, "EE %d %08lx\n", word, (unsigned long)v);
}
void main_draw_begin(void) {}
/* src/snd/snd.c is linked (the play loop's and the front end's sounds): the SDK's voice registers as no-ops */
void cps3s_init(void) {}
void cps3s_voice(int v, uint32_t start, uint32_t end, uint32_t loop, int looped, uint32_t step, int vol_l, int vol_r)
{
    (void)v; (void)start; (void)end; (void)loop; (void)looped; (void)step; (void)vol_l; (void)vol_r;
}
void cps3s_volume(int v, int vol_l, int vol_r) { (void)v; (void)vol_l; (void)vol_r; }
void cps3s_step(int v, uint32_t step) { (void)v; (void)step; }
void cps3s_keys(uint16_t keys) { (void)keys; }
uint16_t cps3s_keys_now(void) { return 0; }

/* HOST_SND=<file>: src/snd's call log as tools/sndcmp.py's lines, "SND <kind> <asset> <arg>" and "R <rec> -" at each
   record point (src/main game.c game_rec_hook), into that file */
#include "sndnames.h"
static FILE *sndf;
extern void (*game_rec_hook)(int32_t rec);
static void snd_print(int k, int s, double arg)
{
    static const char *const kinds[] = { "", "playSound", "playMusic", "startMusic", "stopAllMusic", "setSoundVol",
                                         "audio_stop_sound", "audio_pause_all", "audio_resume_all", "audio_stop_all",
                                         "audio_play_sound" };
    fprintf(sndf, "SND %s %s %.17g\n", kinds[k], s >= 0 && s < SND_COUNT ? sndnames[s] : "-", arg);
}
static void rec_print(int32_t rec) { fprintf(sndf, "R %ld -\n", (long)rec); }

/* HOST_DUMP=<file>: the play state at each record point in test/host/playhost.c's format (its record(): the header
   and every instance's common fields, the oItem / oTreasure variables), for tools/playcmp.py against a trace */
static FILE *dumpf;
static void dump_pd(double v) { fprintf(dumpf, " %.17g", v); }
static void dump_rec(int32_t rec)
{
    int k, phase = game_rec1 == rec;
    if (sndf) rec_print(rec);
    /* t: the route steps done (the runner's oGamepad Step ran in a frame whose Step event changed the room) */
    fprintf(dumpf, "R %ld %d %d %d %d %d %d %d %d %d %d %ld %d %u %s\n", (long)rec, phase, (int)game_steps + phase,
            PW.room,
            G.currLevel, PG.plife, PG.bombs, PG.rope, PG.money, PW.xview, PW.yview,
            instance_exists_p(OBJ_oGame) ? (long)play_time : -1000000000L, play_untranslated, 0u,
            play_untr_obj >= 0 ? objdefs[play_untr_obj].name : "-");
    for (k = PW.nord - 1; k >= 0; k--) {                /* newest first */
        const struct pin *p = &PW.in[pw_ord[k]];
        int a, any = 0;
        if (!p->alive || p->obj == OBJ_oGamepad) continue;
        fprintf(dumpf, "I %ld %s", (long)p->id, objdefs[p->obj].name);
        dump_pd(PTOD(p->x));
        dump_pd(PTOD(p->y));
        fprintf(dumpf, " %s", p->spr >= 0 && p->obj != OBJ_oYellHelp ? gsprname[p->spr] : "-");
        dump_pd((double)p->img);
        dump_pd(p->xscale);
        dump_pd(p->yscale);
        dump_pd(p->angle);
        dump_pd(PE(p)->alpha);
        dump_pd((double)p->depth);
        fprintf(dumpf, " %d ", p->visible);
        for (a = 0; a < 12; a++)
            if (PE(p)->alarm[a] != -1) {
                fprintf(dumpf, "%s%d=%d", any ? "," : "", a, PE(p)->alarm[a]);
                any = 1;
            }
        if (!any) fprintf(dumpf, "-");
        dump_pd(NTOD(PE(p)->xVel));
        dump_pd(NTOD(PE(p)->yVel));
        dump_pd((double)p->ispd);
        if (obj_is(p->obj, OBJ_oItem))
            fprintf(dumpf, " held=%d armed=%d safe=%d cost=%ld trigger=%d myGrav=%.17g", PE(p)->held, PE(p)->armed,
                    PE(p)->safe, (long)PE(p)->cost, PE(p)->trigger, NTOD(PE(p)->myGrav));
        else if (obj_is(p->obj, OBJ_oTreasure))
            fprintf(dumpf, " held=%d state=%d value=%ld trigger=%d myGrav=%.17g", PE(p)->held, PE(p)->state,
                    (long)PE(p)->value, PE(p)->trigger, NTOD(PE(p)->myGrav));
        fprintf(dumpf, "\n");
    }
}
static void snd_log_open(void)
{
    const char *f = getenv("HOST_SND");
    if (!f || !(sndf = fopen(f, "w"))) return;
    snd_log = snd_print;
    game_rec_hook = rec_print;
}
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

/* sprite RAM for src/draw's own sublists (draw.c writes the entries there); cps3v_object decodes a record's entries
   back into the list as MAME's screen_update reads them (position 0, each entry's colour code) */
uint32_t host_sprram[0x80000 / 4];
static int wrap10(int v) { return ((v + 512) & 1023) - 512; }
void cps3v_object(uint32_t addr, uint32_t n, int x, int y, int pal)
{
    static const int size[4] = { 8, 1, 2, 4 };
    uint32_t k;
    if (x || y || pal >= 0 || (addr & 255)) { fprintf(stderr, "host: cps3v_object %x %d %d %d unsupported\n", addr, x, y, pal); exit(3); }
    for (k = 0; k < n && nlist < 4096; k++) {
        const uint32_t *w = &host_sprram[(addr >> 2) + 4 * k];
        struct lent *e = &list[nlist++];
        e->band = 0;
        e->w = size[w[2] & 3];
        e->h = size[(w[2] >> 2) & 3];
        if (w[2] != (((uint32_t)(16 * e->h - 1) << 24) | ((uint32_t)(16 * e->w - 1) << 16) | (w[2] & 15)) || w[3] ||
            !(w[2] & 3) || !(w[2] & 12)) { fprintf(stderr, "host: bad entry %08x %08x %08x %08x\n", w[0], w[1], w[2], w[3]); exit(3); }
        e->x = wrap10((int)((w[1] >> 16) & 0x3ff) - 8 * e->w + 1);
        e->y = wrap10(1006 - (int)(w[1] & 0x3ff) - 8 * e->h);
        e->tile = w[0] >> 17;
        e->pal = w[0] & 0x1ff;
        e->flags = w[0] & (CPS3V_FLIPX | CPS3V_FLIPY | CPS3V_BPP6);
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
    if (argc > 1 && !strcmp(argv[1], "attract")) {   /* host attract <seed> <steps> <gen dir> -: the front end */
        extern uint32_t front_seed;
        int k, steps = atoi(argv[3]);
        int32_t last = -1;
        front_seed = (uint32_t)strtoul(argv[2], 0, 10);
        SH.g.tunnel1 = 10001;                     /* a blank EEPROM (src/shell hs_boot: HD's ini defaults) */
        SH.g.tunnel2 = 20001;
        gfx = slurp(argv[4], "gfx.bin");
        hud = slurp(argv[4], "hud.bin");
        draw_boot();
        snd_init(15, 15);
        snd_log_open();
        if (argc > 6) {                           /* host attract <seed> <steps> <gen> - <room>: start there */
            front_rec_cb = 0;
            front_start_at(atoi(argv[6]));
        }
        for (k = 0; k < steps; k++) {
            game_attract_step();
            cps3v_begin();
            game_draw();
            cps3v_end();
            draw_vblank();
            if (game_rec1 >= 0 && game_rec1 != last) {
                int32_t r = game_rec1, a = host_a8;
                last = r;
                compose();
                fwrite(&r, 4, 1, stdout);
                fwrite(&a, 4, 1, stdout);
                fwrite(view, 2, 320 * 240, stdout);
                {
                    int q;
                    fprintf(stderr, "D %d %u %u room %d", game_rec1, draw_st.entries, draw_st.sprites, PW.room);
                    if (getenv("HOST_OBJ"))
                        for (q = 0; q < PW.n; q++)
                            if (PW.in[q].alive && PW.in[q].obj == atoi(getenv("HOST_OBJ")))
                                fprintf(stderr, " [%d %g %g spr %d img %g xs %g]", PW.in[q].id, PW.in[q].x, PW.in[q].y,
                                        PW.in[q].spr, PW.in[q].img, PW.in[q].xscale);
                    fprintf(stderr, "\n");
                }
            }
        }
        return 0;
    }
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
    game_cfg.scores = getenv("HOST_SCORES") != 0;   /* the route stores its scores (scrUpdateHighscores) */
    hs_boot(&SH.hs, &SH.st, &SH.g);               /* a blank EEPROM: HD's first start */
    draw_boot();
    snd_init(15, 15);
    snd_log_open();
    if (getenv("HOST_DUMP") && (dumpf = fopen(getenv("HOST_DUMP"), "w"))) game_rec_hook = dump_rec;
    game_cfg.globals = getenv("HOST_GLOBALS");    /* "name=value,..." (src/main game.c set_global's names) */
    if (getenv("HOST_ROOM")) game_cfg.room = atoi(getenv("HOST_ROOM"));   /* a route's "# room" (mkroute.py: 3 rOlmec,
                                                     23 rEnd) */
    game_begin();
    for (;;) {
        struct shell_input in = { 0, 0, 0 };
        int over;
        if (getenv("HOST_XEND") && game_steps == atoi(getenv("HOST_XEND")) && PL.idx != NOONE)
            pin_create(PX(PL.idx).x - PI(8), PX(PL.idx).y - PI(8), OBJ_oXEnd);   /* HOST_XEND=<step>: rOlmec's door
                                                     (oFinalBoss makes it once Olmec is in the lava) on the player */
        over = game_step(&in);
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
            fprintf(stderr, "D %d %u %u room %d list %d\n", game_rec1, draw_st.entries, draw_st.sprites, PW.room,
                    nlist);                       /* list: every entry of the frame (draw.c's runs, the bands, the
                                                     SDK's: the HUD and the front end's text) */
        }
        if (over) break;
    }
    if (play_untranslated) fprintf(stderr, "untranslated %d (object %d)\n", play_untranslated, play_untr_obj);
    if (getenv("HOST_AFTER") && strcmp(argv[8], "-")) {   /* HOST_AFTER=<steps>: the shell's attract after the game
                                                           (rHighscores after a game over); v_a<step>.bin each 30 */
        int k, steps = atoi(getenv("HOST_AFTER"));
        for (k = 0; k < steps; k++) {
            game_attract_step();
            cps3v_begin();
            game_draw();
            cps3v_end();
            draw_vblank();
            if (k % 30 == 0) {
                char path[512];
                FILE *o;
                compose();
                snprintf(path, sizeof path, "%s/v_a%d.bin", argv[8], k);
                if ((o = fopen(path, "wb"))) { fwrite(view, 2, 320 * 240, o); fclose(o); }
                fprintf(stderr, "A %d room %d entries %u\n", k, PW.room, draw_st.entries);
            }
        }
    }
    return 0;
}
