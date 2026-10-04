# Audio rules (P6): HD 1.2.2's sound calls and the CPS3 module

Source: every audio call in `refs/hd/src` GML (Scribble's own audio excluded: text only). Module: `src/snd/snd.{h,c}`.
Test: `tests/sndrules`, `scripts/sndrules_check.sh`, `tools/sndcheck.py rules`.

GameMaker 2024.14 behaviour below is from the runtime's documented audio semantics (Inferred: not yet checked
against the HD runner; that is P6's open "key-on log against the trace's sound calls").

## 1. HD's audio scripts and functions

| Script / call | GML | Behaviour in GameMaker 2024.14 terms | CPS3 (`snd.h`) |
|---|---|---|---|
| `playSound(s)` | `audio_play_sound(s, 2, 0)` | New sound instance at priority 2, once. A repeated call **stacks** (another instance; the first is not restarted or stopped), also within one step | `snd_play(s)` |
| `playMusic(s, loop)` | `if (global.music and not audio_is_playing(s)) audio_play_sound(s, 100, loop)` | Priority 100. A track already playing is not restarted. A **different** track is not stopped: two tracks would mix | `snd_music(s, loop)` |
| `audio_play_sound(mus, 10, ..)` direct | `oTitle` Alarm 3 (`musTitle`, loop, no `global.music` test, no playing test); `oEndCustom` Alarm 0, `oPDummy2` Alarm 2 (`musVictory`, once, if `global.music`); `oCamel` Step (`musCredits`, once, if `global.music`) | Priority 10. Each call a new instance | `snd_audio_play(s, 10, loop)` |
| `audio_is_playing(s)` | gates: `sndPush` (`moveTo` x2), `sndFlame` (`oOlmec`, `oLavaSpray` Step), `sndYetiYell` (`oYetiKing` Step), `sndBowPull` (`oPlayer1` Step 1438, `scrFireBow`), the tracks (`playMusic`, pause gains) | True while any instance of the asset plays. Assumed true while paused (Inferred) | `snd_is_playing(s)` |
| `audio_stop_sound(s)` | `sndBowPull` when the bow fires / is released; `musTitle` in `startMusic` (room `rTitle` branch) | Stops **every** instance of the asset | `snd_stop(s)` |
| `stopAllMusic()` | `audio_stop_sound` of the 8 tracks. Called at level exit (`oPlayer1` Step 754, 881, 1896), `oTransition` Create, `oEnd`, `oCredits1/2`, `oMoonRoom` Alarm 10, `oScreen` Game End, `onOffMusic` (music off) | Stops all music instances, effects keep playing | `snd_stop_music()` |
| `startMusic()` | `oLevel` Create (if `global.music`), `oSunRoom` / `oStarsRoom` Alarm 10, `oMoonRoom` Alarm 9 | Per room / `levelType` / `oLoadLevel.music`: `playMusic(track, true)` then `setSoundVol(track, level musicVol)`; boss track only if `oPlayer1.active`; `rSun/rMoon/rStars`: boss track; `rTitle`: **stops** `musTitle` | Game code: `snd_music` + `snd_gain` (the room logic stays in the game) |
| `onOffMusic()` / `toggleMusic()` | pause menu item 0: `global.music = !global.music`, then stop all music or start the room's track; then `audio_pause_all()` again while paused | | `snd_music_on`, `snd_stop_music`, `snd_music` |
| `setSoundVol(s, g)` | `audio_sound_gain(s, g / 10000, 0)` on the **asset** | Sets the asset gain: applies to new instances and to the playing ones at once. Replaces the asset's `.yy` volume | `snd_gain(s, reg)`; `reg = snd_level_gain[v]` for `2000 + 8000 * (v / 18)` |
| `scrInit` | `global.music = true`, `musicVol = soundVol = 15` (clamped 0-17); `setSoundVol` on all 8 tracks (musicVol) and 54 effects (soundVol) | Gain 0.8667 for those; the others keep their `.yy` volume | `snd_init(15, 15)` |
| `oScreen` Step 1 / `oScreenLast` (pause screen) | music gain 0 on pause, back to the level on unpause / die from pause | Effects are not affected | `snd_gain(track, 0)` / `snd_gain(track, snd_level_gain[v])` |
| `menuDie()` | `setSoundVol(track, 10000)` for 5 tracks | Gain 1 | `snd_gain(track, SND_GAIN_ONE)` |
| `audio_pause_all()` / `audio_resume_all()` | `oGame` Step: Start key pauses / resumes, menu "resume" (jump), after toggleMusic | Pauses every playing instance (position kept); sounds started afterwards play | `snd_pause_all()` / `snd_resume_all()` |
| `audio_stop_all()` / `sound_stop_all()` | `oGame` menu "restart" items (mobile / HTML5 builds), `oCredits1` Step | Stops everything | `snd_stop_all()` |

Calls not ported (not reachable in the arcade build): `oNAL` (HTML5 `sound_global_volume`, in no room), `oTemp`
(`sound_play(sndArrowTrap)`, in no room), `oSlider1/2` (option sliders, in no room), `oKeyConfig` / `oJoyConfig`
`audio_resume_all` (configuration screens removed, PLAN.md §1), `os_is_paused` pause (OS), `oPlayer1` Alarm 3
(`sndStep1/2`: undefined globals; `alarm[3]` is only set in commented-out lines). Sounds no GML plays:
`xbombready`, `xgspiderjump`, `xland`, `xletsexplore`, `xpause`.

`playSound` call sites: 331 (most frequent: `sndHit` 73, `sndPickup` 32, `sndHurt` 22, `sndCavemanDie` 15,
`sndThump` 13, `sndDamsel` 13, `sndArrowTrap` 12). Only the 4 effects above are guarded by `audio_is_playing`; every
other effect stacks. No game state depends on audio: `audio_is_playing` only gates sounds.

## 2. Voice allocation on 16 voices (`src/snd/snd.c`)

| Rule | Detail |
|---|---|
| Voice 0 | The 8 `m*` tracks. A track start replaces whatever voice 0 plays (stop then start) |
| Voices 1-15 | Effects. A new effect takes the lowest-numbered free voice (stacking: the same effect on several voices) |
| All 15 busy | Take the voice of lowest priority, the oldest start among equals, if its priority <= the new sound's; otherwise the call does nothing (GameMaker's priority rule) |
| End of a sound played once | `ceil(samples x 60 / 32000)` frames after its start (counted by `snd_frame()` each VBlank, never early at 59.6 Hz), the voice is keyed off and free; `snd_is_playing` false from then |
| Looped | Plays until stopped; the chip loops to the track's first sample (`tools/hdsound.py`) |
| Pause | Step 0 (position held) and volume 0; frames stop counting. Resume writes the step and the asset gain back |
| Gain | Volume register = `0x4000 x` gain; `.yy` volume at boot, replaced by `snd_gain` (playing voices updated, paused ones at resume) |
| Restart of a voice | Key-off then key-on in the same frame (the chip restarts a voice on an off -> on edge) |
| SH-2 | Voice bits from a table (no variable shifts), no divides; the one 32x32->64 multiply (`dmulu.l`) is in `snd_init`. `snd.o`: no libgcc symbols, 2.8 KB text, 608 B bss |

## 3. Where it cannot match GameMaker

| Case | GameMaker | CPS3 | Reached in HD? |
|---|---|---|---|
| More than 15 effects at once | 128 channels (default `audio_channel_num`): nothing is stolen in practice | The 16th takes the oldest lowest-priority voice (cuts a sound short), or is dropped | Possible in big explosions / many hits in one step; not measured yet |
| Two different tracks at once | Both play mixed | The later replaces the earlier on voice 0 | Not found in the paths read (tracks are stopped before the next starts: level exit, `oTransition`, `oEnd`, `oMoonRoom`); not proved for every path |
| `oTitle` Alarm 3 while `musTitle` plays | Second instance (doubled) | Restart on voice 0 | Only if the title track already plays then |
| End of a sound | Ends at its last sample (audio thread) | Voice freed at a whole frame, up to 1 frame + 0.7 % late | Affects only `audio_is_playing` guards by at most a frame |
| Effect at `audio_is_playing` while paused | Assumed playing (Inferred) | Playing | |
| Volume | Float gain, 44.1 kHz mixing with clipping | 16-bit volume register, 8-bit samples at 32 kHz | |
| Steal tie-break | GameMaker's order among equal priorities is not known (Unknown) | Oldest start | Only on the CPS3 (GameMaker never steals here) |

## 4. Verification (2026-10-03)

`scripts/sndrules_check.sh`: `tests/sndrules` runs `script.txt` (stacking x3, the playing guard, 13 effects into 11
free voices stealing the 2 oldest, a gain change on the playing track, a stacked effect stopped by asset, pause with
a sound started while paused, gain while paused, effects running out, priority drop and priority steal, music
replace, loop past mLush's end, the music flag, `musVictory` once to its end). MAME 0.289 register log
(`scripts/lua/sndlog.lua`, cut into frames by the test's writes to sound word 0x81, which the chip ignores):

- 137 events (40 key-ons with sound / loop / volume, 40 key-offs, 57 step / volume writes) equal to the independent
  Python model of §2 (`tools/sndcheck.py rules`, `Rules`) frame by frame;
- 40 s of MAME's output equal to the chip model within 1 LSB at every sample;
- `snd_init`'s reciprocal frame count exact for every length below 2^24 samples.

A changed script (the priority-2 `xhit` at frame 246 made priority 10) fails the event check. `scripts/sound_check.sh`
(the jukebox) still passes after `tools/sndcheck.py`'s audio model moved into `synth()`.

## 5. Pending sites (src/game's sound calls and HUD messages, 2026-10-04)

src/game makes the GML's sound calls through `src/snd` (`src/snd/sndgame.h`; each call carries its GML file:line) and
keeps the HUD messages in `src/game/pmsg.{h,c}`. Gate: `scripts/snd_check.sh` (tools/sndcmp.py against the TRACE_SND
traces of `scripts/snd_traces.sh`, plus tools/playcmp.py), 19/19 routes equal. The sites below are not in the C yet:
add each one when the branch or object around it is translated, at the same place in the code.

Never run in HD (inside `/* */`): characterSprite :86, :148; oArrowTrapLeft / oArrowTrapRight Step :41.

Untranslated branches (PUNTR in src/game):

- oPlayer1 Step: :183 splash (oWaterSwim), :200 flame (lava), :425 :431 :440 :458 :464 :473 whips (machete,
  mattock, tunnel man), :1282 thump (the non-mines idol trap), :1438-1439 audio_is_playing / audio_stop_sound
  (xbowpull), :1530 :1558 :1578 hurt (mitt rock, laser, psychic wave), :1866 teleport (ankh), :1957 pickup (flare
  crate). Alarm_10 jetpack. KeyPress_8 stopAllMusic. Collision_oBlood :12 kiss (kapala).
- scrFireBow :39 :52 :72 (incl. the xbowpull stop); scrUseItem :258 :273 :316 :329 :377 :395 :483 :521 :572 :590
  (weapons, teleporter, bow).
- oEnemy Step :94 :102 (spears, smash trap), :141 sacrifice, :246 UFO; oItem Step :182 :311 (lava, UFO), :294 :305
  (tomb lord, alien boss); oJar Step :98 (lava); oDamsel Step :138 (spears), :321 (sacrifice).
- oDice Step (the dice house); oFlare / oFlareCrate Step splash; oPDummy Alarm_1 click (big chest).

Objects with no translated events: oAlienBoss, oUFO, oYeti, oYetiKing (its :114 audio_is_playing gate), oVampire,
oHawkman, oManTrap, oMagmaMan, oMonkey, oFrog, oFireFrog, oZombie, oTombLord, oScarab, oSpringTrap, oGame Alarm_2
(blink sounds), oGame Step's audio_pause_all / audio_resume_all / audio_stop_all (pause menu, game end). oSkull Step
:126 :136 go with oJar's hit code (penemy.c pen_jar_hit) if oSkull runs it.

HUD messages not yet set: genobj.c's generation-time scrShopkeeperAnger (a shop wall destroyed while generating),
oPlayer1 Step :1863 (ankh revive), oEnemy Step :143-160 and oDamsel Step :323-340 (sacrifice, scrGetFavorMsg),
oMsgSign (tutorial), oPlayer1 Other_7 :124 (rTutorial's messageTimer = 0). global.bloodLevel stays 0 until the
kapala is translated.

audio_is_playing: the runner's answer depends on its audio thread under the container's null sink, not on the steps
(p4_push_rope recorded three times: xpush at records 24 and 33 once, at 24 only twice). A trace that differs only
there is re-recorded (tools/sndcmp.py's docstring).
