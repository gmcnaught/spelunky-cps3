/* The game's audio rules (PLAN.md P6; docs/AUDIO.md): HD 1.2.2's sound calls on the CPS3's 16 PCM voices.
   Independent of the play runtime: the game calls these where the GML calls the matching script or function.

     GML                                        here
     playSound(s)                               snd_play(s)                 audio_play_sound(s, 2, false)
     playMusic(s, loop)                         snd_music(s, loop)          if global.music and not playing(s):
                                                                            audio_play_sound(s, 100, loop)
     audio_play_sound(s, prio, loop)            snd_audio_play(s, prio, loop)
     audio_is_playing(s)                        snd_is_playing(s)
     audio_stop_sound(s)                        snd_stop(s)
     stopAllMusic()                             snd_stop_music()            audio_stop_sound of the 8 tracks
     audio_stop_all() / sound_stop_all()        snd_stop_all()
     audio_pause_all() / audio_resume_all()     snd_pause_all() / snd_resume_all()
     setSoundVol(s, 2000 + 8000*(v/18))         snd_gain(s, snd_level_gain[v])
     setSoundVol(s, 0) / (s, 10000)             snd_gain(s, 0) / snd_gain(s, SND_GAIN_ONE)
     global.music                               snd_music_on

   Voice 0 plays the m* tracks; voices 1-15 the effects. A repeated effect stacks (a new voice; GameMaker makes a new
   sound instance each call). With 15 effect voices busy, a new sound takes the voice of lowest priority, the oldest
   of those, if that priority is at most the new sound's; otherwise the call does nothing. snd_frame() once a frame
   (each vblank) ends voices whose sound has run out. Nothing here feeds back into game state except
   snd_is_playing, which the GML only uses to gate sounds. */
#ifndef SND_RULES_H
#define SND_RULES_H
#include <stdint.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#include "../../build/snd/snd.h"                /* tools/hdsound.py: enum snd, snddefs */
#pragma GCC diagnostic pop

#define SND_MUSIC_VOICE 0
#define SND_GAIN_ONE    0x4000                  /* volume register at GameMaker gain 1 (tools/hdsound.py VOICE) */
#define SND_PRIO_EFFECT 2                       /* playSound */
#define SND_PRIO_MUSIC  100                     /* playMusic */

#ifndef SND_AT
#define SND_AT 0u                               /* snd.bin's offset in the sample flash */
#endif

extern uint8_t snd_music_on;                    /* global.music (scrInit: true) */
/* volume register for setSoundVol(s, 2000 + 8000 * (v / 18)), v = global.musicVol / soundVol 0-17 */
extern const uint16_t snd_level_gain[18];

/* scrInit's audio part: all voices off, global.music = 1, every asset's gain its .yy volume, then setSoundVol at
   musicVol for the 8 tracks and at soundVol for the 54 effects scrInit lists (HD: both 15; 0-17) */
void snd_init(int music_vol, int sound_vol);
void snd_frame(void);
int snd_audio_play(int s, int prio, int loop); /* voice used, or -1 if the call was dropped */
void snd_play(int s);
void snd_music(int s, int loop);
int snd_is_playing(int s);
void snd_stop(int s);
void snd_stop_music(void);
void snd_stop_all(void);
void snd_pause_all(void);
void snd_resume_all(void);
void snd_gain(int s, uint16_t reg);
/* setSoundVol(s, v) (v the GML volume argument, 0 .. 10000) and the startMusic() call itself (the game runs its
   body: playMusic / setSoundVol through these functions) */
void snd_volume(int s, double v);
void snd_start_music(void);

/* the call log (tests: test/host/playhost prints it as tools/sndcmp.py's SND lines): each function above, as the
   game calls it, reports the GML call it stands for first (SNDK_*: tools/tracer.py SND_KINDS; calls made inside
   these functions are not reported, as the runner's trace logs only the scripts and the builtins the GML calls) */
enum { SNDK_PLAY_SOUND = 1, SNDK_PLAY_MUSIC, SNDK_START_MUSIC, SNDK_STOP_ALL_MUSIC, SNDK_SET_VOL, SNDK_STOP_SOUND,
       SNDK_PAUSE_ALL, SNDK_RESUME_ALL, SNDK_STOP_ALL, SNDK_AUDIO_PLAY };
#ifdef SND_LOG                                  /* host builds only (test/host, tests/game/host.c) */
extern void (*snd_log)(int kind, int s, double arg);   /* s: enum snd or -1; arg as sndcmp's (0 if none) */
#endif
#endif
