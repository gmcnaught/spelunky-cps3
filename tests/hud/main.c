/* HUD test (PLAN.md P8): HD's in-game HUD (src/hud) for each state of tests/hud/cases.json (build/hud/cases.h), one
   case every CASE_FRAMES frames over a blank screen, the view's lines 8-231 stretched to 384 by X zoom 0x35 as the
   game's screen. All art by character DMA at boot. scripts/hud_check.sh snapshots the middle of each case. */
#include "cps3.h"
#include "sprites.h"
#include "hudart.h"
#include "hud.h"
#include "cases.h"

#define PAL        1u                           /* colour code 1: the game palette */
#define ZOOM_X     (*(volatile uint16_t *)0x040c006eu)
#define CASE_FRAMES 120

static void load_gfx(void)
{
    cps3dma_palette(GFX_PAL_AT, PAL * 256, 256, 0);
    cps3dma_palette(HUD_PAL_AT, HUD_PAL * 256, 512, 0);
    uint32_t src = GFX_TILES_AT - 256, first = GFX_FIRST_TILE - 1u, n = GFX_NTILES + 1;
    while (n) {
        uint32_t k = n > 4096 ? 4096 : n;
        cps3dma_char_copy(src, first, k);
        src += 256 * k;
        first += k;
        n -= k;
    }
    cps3dma_char_copy(HUD_TILES_AT, HUD_FIRST_TILE, HUD_NTILES);
    cps3dma_char_run();
}

int main(void)
{
    cps3_init();
    load_gfx();
    uint32_t frame = 0;
    for (;;) {
        cps3v_wait_vblank();
        cps3v_vblank();
        ZOOM_X = 0x35;
        cps3v_begin();
        hud_draw(&hud_cases[(frame / CASE_FRAMES) % NCASES], PAL);
        cps3v_end();
        frame++;
    }
}
