/* The game program's start-up and frame loop (game.h). Flash layout (tests/game/assets.py): samples at 0 (SND_AT),
   the game palette at GFX_PAL_AT, tiles at GFX_TILES_AT (a blank tile first), the HUD's at HUD_TILES_AT, the faded
   palettes at DARK_FADE_AT (tools/darkfade.py table). */
#include "cps3.h"
#include "sprites.h"
#include "hudart.h"
#include "shell.h"
#include "sndgame.h"
#include "draw.h"
#include "game.h"
#include "play.h"                            /* pw_gbr_init */

__attribute__((weak)) void main_inputs(uint32_t *pad0, uint32_t *pad1, uint32_t *lines)
{
    uint32_t s = cps3_system();
    *pad0 = cps3_pad(0);
    *pad1 = cps3_pad(1);
    *lines = (*pad0 & CPS3_COIN ? CR_COIN1 : 0) | (*pad1 & CPS3_COIN ? CR_COIN2 : 0) |
             (s & CPS3_SERVICE ? CR_SERVICE : 0) | (s & CPS3_TEST ? SHELL_TEST : 0);
}
__attribute__((weak)) void main_frame_done(void) {}
__attribute__((weak)) void main_draw_begin(void) {}
__attribute__((weak)) void main_draw_end(void) {}
__attribute__((weak)) void main_vblank_begin(void) {}
__attribute__((weak)) void main_vblank_end(void) {}

static void load_gfx(void)
{
    cps3dma_palette(GFX_PAL_AT, DRAW_PAL * 256, 256, 0);
    cps3dma_palette(GFX_PAL_AT, DRAW_PAL_LIT * 256, 256, 0);   /* the unfaded copy (dark levels: draw.c) */
    cps3dma_palette(HUD_PAL_AT, HUD_PAL * 256, 512, 0);
    cps3dma_palette(HUD_PAL_AT, DRAW_PAL_HUDDARK * 256, 256, 0);   /* faded by draw.c on dark levels */
    uint32_t src = GFX_TILES_AT - 256, first = GFX_FIRST_TILE - 1u, n = GFX_NTILES + 1;
    while (n) {                                   /* records of at most 1 MB */
        uint32_t k = n > 4096 ? 4096 : n;
        cps3dma_char_copy(src, first, k);
        src += 256 * k;
        first += k;
        n -= k;
    }
    cps3dma_char_copy(HUD_TILES_AT, HUD_FIRST_TILE, HUD_NTILES);
    cps3dma_char_run();
}

/* the C library calls GCC may emit (-fno-builtin, no libc) */
void *memcpy(void *d, const void *s, unsigned long n)
{
    char *a = d;
    const char *b = s;
    while (n--) *a++ = *b++;
    return d;
}
void *memset(void *d, int c, unsigned long n)
{
    char *a = d;
    while (n--) *a++ = (char)c;
    return d;
}

void main_boot(void);                             /* tests/game: RAM outside .bss (weak no-op) */
__attribute__((weak)) void main_boot(void) {}

int main(void)
{
    pw_gbr_init();                               /* PW through GBR (src/game/play.h) */
    cps3_init();
    main_boot();
    load_gfx();
    snd_init(15, 15);
    draw_boot();
    shell_init();
    for (;;) {
        uint32_t p0, p1, lines;
        cps3v_wait_vblank();
        main_vblank_begin();
        draw_vblank();
        main_vblank_end();
        cps3v_vblank();
        snd_frame();
        main_frame_done();
        main_inputs(&p0, &p1, &lines);
        shell_frame(p0, p1, lines);
    }
}
