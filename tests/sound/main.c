/* P6 jukebox (PLAN.md P6): plays every sound of tools/hdsound.py's table once, in order: an effect on voice 1 to its
   end, a music track on voice 0 for MUSIC_FRAMES, then key-off; GAP frames between sounds. The sound's number is
   shown on the text layer. scripts/sound_check.sh logs the register writes and checks MAME's output against the
   chip model (tools/sndcheck.py). */
#include "cps3.h"
#include "snd.h"

#define SND_AT       0u                 /* snd.bin's flash offset (assets.py) */
#define MUSIC_FRAMES 120
#define GAP          12

static void hex(char *b, uint32_t v, int n)
{
    for (int i = n - 1; i >= 0; i--, v >>= 4)
        b[i] = "0123456789ABCDEF"[v & 15];
    b[n] = 0;
}

int main(void)
{
    cps3_init();
    cps3v_text(2, 1, "SPELUNKY CPS3 JUKEBOX");
    for (int k = 0; k < SND_COUNT; k++) {
        const struct snddef *s = &snddefs[k];
        int v = s->looped ? 0 : 1;
        uint32_t base = CPS3S_BASE + SND_AT;
        cps3s_keys(0);
        cps3s_voice(v, base + s->start, base + s->end, base + s->loop, s->looped, s->step, s->vol, s->vol);
        cps3s_keys(1u << v);
        char b[3];
        hex(b, k, 2);
        cps3v_text(2, 3, "SOUND");
        cps3v_text(8, 3, b);
        /* frames to play: the whole effect (end - start samples at SND_RATE), or MUSIC_FRAMES */
        uint32_t frames = s->looped ? MUSIC_FRAMES : (s->end - s->start) * 60u / SND_RATE + 2;
        for (uint32_t f = 0; f < frames; f++)
            cps3v_wait_vblank();
        cps3s_keys(0);
        for (int f = 0; f < GAP; f++)
            cps3v_wait_vblank();
    }
    cps3v_text(2, 5, "DONE");
    for (;;)
        cps3v_wait_vblank();
}
