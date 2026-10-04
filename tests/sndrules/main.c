/* P6 audio rules test (docs/AUDIO.md): runs script.txt's calls (build/script.h, tests/sndrules/mkscript.py) on
   src/snd at their frames. Each frame: VBlank, the frame number written to sound register word 0x81 (no chip
   function; MAME logs and ignores it: the marker scripts/lua/sndlog.lua's log is cut into frames by), snd_frame(),
   the frame's calls. scripts/sndrules_check.sh compares the key-on / key-off log with tools/sndcheck.py's model. */
#include "cps3.h"
#include "snd.h"
#include "script.h"

#define TAIL 720                                /* frames after the last call: mVictory (11 s) runs out */
#define MARK (*(volatile uint32_t *)0x240e0204u)

static void run(const struct step *p)
{
    switch (p->op) {
    case OP_PLAY: snd_play(p->s); break;
    case OP_GUARD: if (!snd_is_playing(p->s)) snd_play(p->s); break;
    case OP_MUSIC: snd_music(p->s, p->a); break;
    case OP_APLAY: snd_audio_play(p->s, p->a, p->b); break;
    case OP_STOP: snd_stop(p->s); break;
    case OP_STOPMUSIC: snd_stop_music(); break;
    case OP_STOPALL: snd_stop_all(); break;
    case OP_PAUSE: snd_pause_all(); break;
    case OP_RESUME: snd_resume_all(); break;
    case OP_GAIN: snd_gain(p->s, (uint16_t)p->a); break;
    case OP_LEVEL: snd_gain(p->s, snd_level_gain[p->a]); break;
    case OP_MUSICFLAG: snd_music_on = (uint8_t)p->a; break;
    }
}

int main(void)
{
    cps3_init();
    snd_init(15, 15);
    cps3v_text(2, 1, "SPELUNKY CPS3 AUDIO RULES");
    int i = 0;
    for (uint32_t f = 0; f <= SCRIPT_END + TAIL; f++) {
        cps3v_wait_vblank();
        MARK = f;
        snd_frame();
        while (i < SCRIPT_N && script[i].frame == f)
            run(&script[i++]);
    }
    MARK = 0xffffffffu;
    cps3v_text(2, 3, "DONE");
    for (;;)
        cps3v_wait_vblank();
}
