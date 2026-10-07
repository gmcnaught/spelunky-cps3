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
    cps3dma_palette(HUD_PAL_AT, DRAW_PAL_HUDDARK * 256, 512, 0);   /* and DRAW_PAL_HUDDARK_YELLOW: faded by draw.c */
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

/* The VBlank interrupt (IRL 12): the SDK's handler (crt0.S: count, acknowledge) plus src/draw's second list of a
   step (smooth motion, draw_vbl_irq). The vector table in use is copied to RAM with this handler in vectors 70-71
   and VBR pointed at it */
static void __attribute__((interrupt_handler)) vbl_irq(void)
{
    vbl_count++;
    *(volatile uint32_t *)0x05100000u = vbl_count;   /* acknowledges IRL 12 (any write) */
    draw_vbl_irq();
}
static uint32_t vectors[128];                     /* all of them: on-chip peripherals' vectors too (tests: FRT, 72) */
static void vbl_irq_install(void)
{
    const uint32_t *v;
    uint32_t sr;
    int k;
    __asm__ volatile("stc vbr, %0" : "=r"(v));
    for (k = 0; k < 128; k++) vectors[k] = v[k];
    vectors[70] = vectors[71] = (uint32_t)vbl_irq;
    __asm__ volatile("stc sr, %0" : "=r"(sr));
    __asm__ volatile("ldc %0, sr" : : "r"(sr | 0xf0));   /* no interrupts while VBR moves */
    __asm__ volatile("ldc %0, vbr" : : "r"(vectors) : "memory");
    __asm__ volatile("ldc %0, sr" : : "r"(sr));
}

/* the settings screen takes the display (src/shell/shell.c) */
void shell_video_stop(void) { draw_irq_off(); draw_list_sync(); }

int main(void)
{
    pw_gbr_init();                               /* PW through GBR (src/game/play.h) */
    cps3_init();
    main_boot();
    load_gfx();
    snd_init(15, 15);
    draw_boot();
    shell_init();
    if (!game_cfg.route) draw_smooth = SH.st.smooth;   /* the cabinet's setting; routes: off (tests may set it) */
    vbl_irq_install();
    /* Frames alternate step / between (shell_frame). A step frame starts at a VBlank. The frame between starts at
       the next VBlank, or at once when that VBlank went by during the step (late: no VBlank work then, the step's
       list is shown at the next step frame's VBlank), so a step and its draw have two frames. With smooth motion
       the VBlank work is the step frame's (the midpoint list); the VBlank between shows the step's own list from
       the interrupt (draw_vbl_irq) */
    uint32_t step_v = vbl_count;
    for (;;) {
        uint32_t p0, p1, lines;
        int step = !(SH.frame & 1), late = 0;
        if (step || vbl_count == step_v) cps3v_wait_vblank();
        else late = 1;
        if (step) step_v = vbl_count;
        if (!late && (step || !draw_smooth)) {
            main_vblank_begin();
            draw_vblank();
            main_vblank_end();
            draw_list_send();                     /* the list DMA, not waited for: */
            draw_vblank_end();
        }
        snd_frame();
        draw_list_sync();                         /* its end checked here, before the shell writes a list */
        if (!late) main_frame_done();
        main_inputs(&p0, &p1, &lines);
        shell_frame(p0, p1, lines);
    }
}
