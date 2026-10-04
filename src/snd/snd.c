/* The game's audio rules (snd.h; docs/AUDIO.md) on the SDK's sound helpers (cps3s.h). Per-frame code has no
   variable shifts or divides (SH-2: those are libgcc calls): voice bits come from a table. */
#include "cps3s.h"
#include "snd.h"

#define NVOICE 16
#define FX_FIRST 1

uint8_t snd_music_on;
#ifdef SND_LOG
void (*snd_log)(int kind, int s, double arg);
#define LOG(k, s, a) do { if (snd_log) snd_log((k), (s), (a)); } while (0)
#else
#define LOG(k, s, a) ((void)0)                   /* CPS3 builds: no log, no cost */
#endif

/* round(0x4000 * (2000 + 8000 * (v / 18)) / 10000) */
const uint16_t snd_level_gain[18] = {
    3277, 4005, 4733, 5461, 6190, 6918, 7646, 8374, 9102, 9830, 10559, 11287, 12015, 12743, 13471, 14199, 14928, 15656
};

/* scrInit's setSoundVol(sndX, .. soundVol ..) list (initMusic's globals but xbombready, xgspiderjump, xland,
   xletsexplore, xpause, which no GML plays) */
static const uint8_t init_fx[54] = {
    SND_xignite, SND_xteleport, SND_xjetpack, SND_xwhip, SND_xjump, SND_xthrow, SND_xclimb1, SND_xclimb2,
    SND_xshotgun, SND_xbowpull, SND_xsteps, SND_xblink1, SND_xblink2, SND_xhit, SND_xhurt, SND_xdie, SND_xcoin,
    SND_xgem, SND_xpickup, SND_xchestopen, SND_xpush, SND_xmattockbreak, SND_xtrap, SND_xclick, SND_xbreak,
    SND_xthud, SND_xthump, SND_xcrunch, SND_xsplash, SND_xflame, SND_xexplosion, SND_xarrowtrap, SND_xboing,
    SND_xdamsel, SND_xkiss, SND_xghost, SND_xbat, SND_xgiantspider, SND_xspiderjump, SND_xfrog, SND_xzombie,
    SND_xmonkey, SND_xalert, SND_xcavemandie, SND_xalien, SND_xlaser, SND_xlasercharge, SND_xsmallexplode,
    SND_xpsychic, SND_xyetiyell, SND_xbigjump, SND_xslam, SND_xpfall, SND_xtfall
};

static const uint16_t vbit[NVOICE] = {
    1u << 0, 1u << 1, 1u << 2, 1u << 3, 1u << 4, 1u << 5, 1u << 6, 1u << 7,
    1u << 8, 1u << 9, 1u << 10, 1u << 11, 1u << 12, 1u << 13, 1u << 14, 1u << 15
};

/* a voice: free when left == 0; left < 0 loops until stopped; age orders starts (stealing takes the oldest) */
static struct voice { int16_t snd; uint8_t prio; uint8_t paused; int32_t left; uint32_t age; } voice[NVOICE];
static uint16_t keys;
static uint32_t starts;                         /* start counter for age */
static uint16_t gain[SND_COUNT];                /* asset gain as a volume register (audio_sound_gain on the asset) */
static int32_t frames[SND_COUNT];               /* frames a sound plays once: ceil(samples * 60 / SND_RATE) */

static void voice_off_mask(uint16_t m)
{
    for (int k = 0; k < NVOICE; k++)
        if (m & vbit[k]) {
            voice[k].left = 0;
            voice[k].snd = -1;
            voice[k].paused = 0;
        }
    keys &= (uint16_t)~m;
    cps3s_keys(keys);
}

static void voice_start(int k, int s, int prio, int loop)
{
    const struct snddef *d = &snddefs[s];
    uint32_t base = CPS3S_BASE + SND_AT;
    if (keys & vbit[k]) {                       /* a key-off -> key-on edge restarts the voice */
        keys &= (uint16_t)~vbit[k];
        cps3s_keys(keys);
    }
    cps3s_voice(k, base + d->start, base + d->end, base + (d->looped ? d->loop : d->start), loop ? 1 : 0, d->step,
                gain[s], gain[s]);
    keys |= vbit[k];
    cps3s_keys(keys);
    voice[k].snd = (int16_t)s;
    voice[k].prio = (uint8_t)prio;
    voice[k].paused = 0;
    voice[k].left = loop ? -1 : frames[s];
    voice[k].age = starts++;
}

void snd_init(int music_vol, int sound_vol)
{
    keys = 0;
    cps3s_keys(0);
    starts = 0;
    snd_music_on = 1;
    for (int k = 0; k < NVOICE; k++) {
        voice[k].snd = -1;
        voice[k].left = 0;
        voice[k].paused = 0;
    }
    for (int s = 0; s < SND_COUNT; s++) {
        const struct snddef *d = &snddefs[s];
        gain[s] = (uint16_t)d->vol;
        /* ceil(n * 3 / 1600) by reciprocal (exact for n < 2^24: tools/sndcheck.py rules checks the table) */
        uint32_t x = (d->end - d->start) * 3u + 1599u;
        frames[s] = (int32_t)(((uint64_t)x * 2748779070u) >> 42);
    }
    for (int s = SND_mBoss; s <= SND_mVictory; s++)
        gain[s] = snd_level_gain[music_vol];
    for (unsigned i = 0; i < sizeof init_fx; i++)
        gain[init_fx[i]] = snd_level_gain[sound_vol];
}

void snd_frame(void)
{
    uint16_t m = 0;
    for (int k = 0; k < NVOICE; k++)
        if (voice[k].left > 0 && !voice[k].paused && --voice[k].left == 0)
            m |= vbit[k];
    if (m)
        voice_off_mask(m);
}

static int audio_play(int s, int prio, int loop)
{
    int k;
    if ((unsigned)s >= SND_COUNT)
        return -1;
    if (s <= SND_mVictory)
        k = SND_MUSIC_VOICE;                    /* a track replaces the one playing (GameMaker would mix both) */
    else {
        for (k = FX_FIRST; k < NVOICE; k++)
            if (voice[k].left == 0)
                break;
        if (k == NVOICE) {                      /* all busy: lowest priority, oldest first */
            int best = FX_FIRST;
            for (k = FX_FIRST + 1; k < NVOICE; k++)
                if (voice[k].prio < voice[best].prio ||
                    (voice[k].prio == voice[best].prio && starts - voice[k].age > starts - voice[best].age))
                    best = k;
            if (voice[best].prio > prio)
                return -1;
            k = best;
        }
    }
    voice_start(k, s, prio, loop);
    return k;
}

int snd_audio_play(int s, int prio, int loop)
{
    LOG(SNDK_AUDIO_PLAY, s, prio * 2 + (loop ? 1 : 0));
    return audio_play(s, prio, loop);
}

void snd_play(int s)
{
    LOG(SNDK_PLAY_SOUND, s, 0);
    audio_play(s, SND_PRIO_EFFECT, 0);
}

static int is_playing(int s);

void snd_music(int s, int loop)
{
    LOG(SNDK_PLAY_MUSIC, s, loop ? 1 : 0);
    if (snd_music_on && !is_playing(s))
        audio_play(s, SND_PRIO_MUSIC, loop);
}

int snd_is_playing(int s) { return is_playing(s); }

static int is_playing(int s)
{
    for (int k = 0; k < NVOICE; k++)
        if (voice[k].left != 0 && voice[k].snd == s)
            return 1;
    return 0;
}

static void stop(int s);

void snd_stop(int s)
{
    LOG(SNDK_STOP_SOUND, s, 0);
    stop(s);
}

static void stop(int s)
{
    uint16_t m = 0;
    for (int k = 0; k < NVOICE; k++)
        if (voice[k].left != 0 && voice[k].snd == s)
            m |= vbit[k];
    if (m)
        voice_off_mask(m);
}

void snd_stop_music(void)
{
    LOG(SNDK_STOP_ALL_MUSIC, -1, 0);
    if (voice[SND_MUSIC_VOICE].left != 0)
        voice_off_mask(vbit[SND_MUSIC_VOICE]);
}

void snd_stop_all(void)
{
    LOG(SNDK_STOP_ALL, -1, 0);
    voice_off_mask(0xffff);
}

/* paused: step 0 holds the position, volume 0 silences the held sample; frames stop counting */
void snd_pause_all(void)
{
    LOG(SNDK_PAUSE_ALL, -1, 0);
    for (int k = 0; k < NVOICE; k++)
        if (voice[k].left != 0 && !voice[k].paused) {
            voice[k].paused = 1;
            cps3s_step(k, 0);
            cps3s_volume(k, 0, 0);
        }
}

void snd_resume_all(void)
{
    LOG(SNDK_RESUME_ALL, -1, 0);
    for (int k = 0; k < NVOICE; k++)
        if (voice[k].paused) {
            int s = voice[k].snd;
            voice[k].paused = 0;
            cps3s_step(k, snddefs[s].step);
            cps3s_volume(k, gain[s], gain[s]);
        }
}

/* audio_sound_gain(asset, g, 0): new and playing instances (paused ones take it at resume) */
static void gain_set(int s, uint16_t reg);

void snd_gain(int s, uint16_t reg) { gain_set(s, reg); }

/* setSoundVol: sound_volume(s, v / 10000) = audio_sound_gain at v / 10000 */
void snd_volume(int s, double v)
{
    LOG(SNDK_SET_VOL, s, v);
    gain_set(s, (uint16_t)(v * (SND_GAIN_ONE / 10000.0) + 0.5));
}

void snd_start_music(void) { LOG(SNDK_START_MUSIC, -1, 0); }

static void gain_set(int s, uint16_t reg)
{
    if ((unsigned)s >= SND_COUNT)
        return;
    gain[s] = reg;
    for (int k = 0; k < NVOICE; k++)
        if (voice[k].left != 0 && voice[k].snd == s && !voice[k].paused)
            cps3s_volume(k, reg, reg);
}
