/* P3 level viewer (PLAN.md P3): one level from the HD reference runner (tools/viewlevel.py -> level.h) drawn as
   the port will draw it: bgCave on tilemap 0, terrain on tilemap 1, everything else as sprites in depth order, the
   320 x 240 view's lines 8-231 stretched to 384 by X zoom 0x35. All tiles by one character DMA at boot (PLAN §2).
   Cameras: the phases below (CAM_FRAMES each; scripts/view_check.sh snapshots them), then the stick. */
#include "cps3.h"
#include "sprites.h"
#include "level.h"

#define PAL        1u                           /* colour code 1: the game palette */
#define BLANK      (GFX_FIRST_TILE - 1u)        /* assets.py's blank tile */
#define UNIT_BG    CPS3V_MAP_UNIT(0)
#define UNIT_TER   CPS3V_MAP_UNIT(1)
#define ZOOM_X     (*(volatile uint16_t *)0x040c006eu)
#define CROP       8
#define VIEW_W     320
#define VIEW_H     240
#define ROOM_W     672
#define ROOM_H     544
#define CAM_FRAMES 120

static const int16_t cams[][2] = { { 0, 0 }, { 352, 0 }, { 176, 152 }, { 352, 304 }, { 0, 304 }, { 123, 77 } };
#define NCAMS (int)(sizeof cams / sizeof cams[0])

static void load_gfx(void)
{
    cps3dma_palette(GFX_PAL_AT, PAL * 256, 256, 0);
    uint32_t src = GFX_TILES_AT - 256, first = BLANK, n = GFX_NTILES + 1;
    while (n) {                                  /* records of at most 1 MB (dtest: one 1 MB record works) */
        uint32_t k = n > 4096 ? 4096 : n;
        cps3dma_char_copy(src, first, k);
        src += 256 * k;
        first += k;
        n -= k;
    }
    cps3dma_char_run();
}

static void build_maps(void)
{
    /* bgCave: one 64 x 64 frame, its pieces give the tile of each 16 x 16 cell; repeated over the 64 x 64 map */
    uint16_t bg[4][4];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            bg[i][j] = BLANK;
    for (int p = VIEW_BG_PIECE; p < VIEW_BG_PIECE + VIEW_BG_NPIECES; p++) {
        const struct piecedef *pc = &piecedefs[p];
        for (int i = 0; i < pc->w; i++)
            for (int j = 0; j < pc->h; j++)
                bg[(pc->dx >> 4) + i][(pc->dy >> 4) + j] = pc->tile + i * pc->h + j;
    }
    for (int r = 0; r < 64; r++)
        for (int c = 0; c < 64; c++) {
            cps3v_cell(UNIT_BG, c, r, bg[c & 3][r & 3], PAL, 0);
            cps3v_cell(UNIT_TER, c, r, BLANK, PAL, 0);
        }
    for (int k = 0; k < VIEW_NTERRAIN; k++)
        cps3v_cell(UNIT_TER, view_terrain[k][0], view_terrain[k][1], view_terrain[k][2], PAL, 0);
}

static void draw_frame(int f, int x, int y)
{
    const struct framedef *fd = &framedefs[f];
    for (int p = fd->piece; p < fd->piece + fd->npieces; p++) {
        const struct piecedef *pc = &piecedefs[p];
        int px = x + pc->dx, py = y + pc->dy;
        if (px > VIEW_W || py > 224 || px + 16 * pc->w <= 0 || py + 16 * pc->h <= 0)
            continue;
        cps3v_sprite(px, py, pc->w, pc->h, pc->tile, PAL, 0);
    }
}

static void draw_sprites(int vx, int vy, int deep)
{
    for (int k = 0; k < VIEW_NSPRITES; k++) {
        const int32_t *s = view_sprites[k];
        if ((s[0] > 100) != deep)
            continue;
        draw_frame(s[1], s[2] - vx, s[3] - vy);
    }
}

int main(void)
{
    cps3_init();
    load_gfx();
    build_maps();
    uint32_t frame = 0;
    int vx = 0, vy = 0;
    for (;;) {
        cps3v_wait_vblank();
        cps3v_vblank();
        int ph = frame / CAM_FRAMES;
        if (ph < NCAMS) {
            vx = cams[ph][0];
            vy = cams[ph][1];
        } else {
            uint32_t p1 = cps3_pad(0);
            vx += (p1 & CPS3_RIGHT ? 4 : 0) - (p1 & CPS3_LEFT ? 4 : 0);
            vy += (p1 & CPS3_DOWN ? 4 : 0) - (p1 & CPS3_UP ? 4 : 0);
            vx = vx < 0 ? 0 : vx > ROOM_W - VIEW_W ? ROOM_W - VIEW_W : vx;
            vy = vy < 0 ? 0 : vy > ROOM_H - VIEW_H ? ROOM_H - VIEW_H : vy;
        }
        cps3v_tilemap(0, vx, vy + CROP, UNIT_BG, 1);
        cps3v_tilemap(1, vx, vy + CROP, UNIT_TER, 1);
        ZOOM_X = 0x35;
        cps3v_begin();
        cps3v_band(0, 0, CPS3V_H);
        draw_sprites(vx, vy + CROP, 1);
        cps3v_group();
        cps3v_band(1, 0, CPS3V_H);
        draw_sprites(vx, vy + CROP, 0);
        cps3v_end();
        frame++;
    }
}
