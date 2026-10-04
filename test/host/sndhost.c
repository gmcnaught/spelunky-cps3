/* src/snd on the host (the play loop's sound calls, src/snd/snd.c): the SDK's voice registers as no-ops, and the
   call log printed as tools/sndcmp.py's lines, "SND <kind> <asset> <arg>", in the play loop's output (playhost) */
#include <stdio.h>
#include "../../src/snd/snd.h"
#include "cps3s.h"

void cps3s_init(void) {}
void cps3s_voice(int v, uint32_t start, uint32_t end, uint32_t loop, int looped, uint32_t step, int vol_l, int vol_r)
{
    (void)v; (void)start; (void)end; (void)loop; (void)looped; (void)step; (void)vol_l; (void)vol_r;
}
void cps3s_volume(int v, int vol_l, int vol_r) { (void)v; (void)vol_l; (void)vol_r; }
void cps3s_step(int v, uint32_t step) { (void)v; (void)step; }
void cps3s_keys(uint16_t keys) { (void)keys; }
uint16_t cps3s_keys_now(void) { return 0; }

#include "sndnames.h"                            /* build/snd: sndnames[enum snd] (test/host/Makefile) */
static const char *const kinds[] = { "", "playSound", "playMusic", "startMusic", "stopAllMusic", "setSoundVol",
                                     "audio_stop_sound", "audio_pause_all", "audio_resume_all", "audio_stop_all",
                                     "audio_play_sound" };

static void print_call(int k, int s, double arg)
{
    printf("SND %s %s %.17g\n", kinds[k], s >= 0 && s < SND_COUNT ? sndnames[s] : "-", arg);
}

/* the sound state at a level start of the route (scrInit's volumes 15 / 15) and the log on stdout */
void sndhost_init(void)
{
    snd_init(15, 15);
    snd_log = print_call;
}
