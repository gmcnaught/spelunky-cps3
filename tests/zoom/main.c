/* Full-screen X zoom (PLAN.md P3): a 320 x 224 grid of 20 column tiles on tilemap 0, then the CRTC's X zoom
   (0x040c006e low byte; 0x40 = 1:1, smaller stretches) set in phases of 300 frames: 0x40, 0x35, 0x36, repeating.
   Tiles and colours by DMA from the flash (assets.py). scripts/zoom_check.sh compares MAME snapshots taken in the
   middle of each phase with assets.py's expected screens. */
#include "cps3.h"
#include "assets.h"

#define TILE0  0x100u
#define PAL    1u
#define UNIT   CPS3V_MAP_UNIT(0)
#define ZOOM_X (*(volatile uint16_t *)0x040c006eu)

static const uint16_t zooms[] = { 0x40, 0x35, 0x36 };

int main(void)
{
    cps3_init();
    cps3dma_palette(COLOURS_AT, PAL * 256, 16, 0);
    cps3dma_char_copy(TILES_AT, TILE0, NCOL);
    cps3dma_char_run();
    for (int r = 0; r < 64; r++)
        for (int c = 0; c < 64; c++)
            cps3v_cell(UNIT, c, r, c < NCOL && r < NROW ? TILE0 + c : 0, PAL, 0);

    uint32_t frame = 0;
    for (;;) {
        cps3v_wait_vblank();
        cps3v_vblank();
        cps3v_tilemap(0, 0, 0, UNIT, 1);
        ZOOM_X = zooms[(frame / 300) % 3];
        cps3v_begin();
        cps3v_band(0, 0, CPS3V_H);
        cps3v_end();
        frame++;
    }
}
