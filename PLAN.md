# Spelunky Classic on CPS3: a native ROM port plan

Goal: Spelunky Classic HD 1.2.2 (yancharkin; Derek Yu's Spelunky 1.1 by way of the GameMaker: Studio port) re-implemented
as a homebrew ROM set for the Capcom CPS3, without HD's non-arcade features (§1). The set runs on MAME (`cps3` driver,
stand-in set `sfiii3na`) and on the MiSTer's jtcps3 core. Gameplay matches HD 1.2.2. The game is built natively on the SH-2 and the CPS3 PPU with everything learned in `../maldita.castilla-cps3`, and it uses
the SDK in `../cps3-testgame/sdk`. It is not a GameMaker runner.

**Licence.** The Spelunky User License (`refs/src_1_1/COPYING.txt`; HD keeps it: `refs/hd/src/LICENSE`) allows redistribution and modification but not
sale. Game assets and the files converted from them are still never committed: converters build them from
`refs/` (git-ignored).

## 1. Decisions

| Decision | Date | Reason |
|---|---|---|
| Source of truth: Spelunky Classic HD at tag 1.2.2 (e88247ab, 2025-09-03, GameMaker runtime 2024.1400.0.871): GML from `refs/hd/src`, reference runs from the release's Android build (`refs/hd/hd-1.2.2-android.apk`, `assets/game.droid`: VM bytecode 17, CODE 1.16 MB) | 2026-10-03 | User choice (replaces v1.1, set earlier the same day): HD carries the GMS port's and HD's fixes. 1.1 stays extracted (`build/gml`) as history. Spelunky SD is YYC in both builds (`game.unx` CODE / FUNC / VARI empty), so only its changelog is usable (`refs/sd_linux/assets/changelog.txt`) |
| Removed from HD (not arcade-beneficial): widescreen (the view stays 320x240, `oScreen` Create's aspect code fixed at 4:3), touch input, localisation (Scribble; English text only), keyboard / gamepad configuration screens, save files (EEPROM instead), window / fullscreen / OS options. The full list per object is set when `docs/VARIANTS.md` is in | 2026-10-03 | User choice |
| Display: the 320-px view stretched to 384 horizontally; 224 of its 240 lines shown. All game logic keeps the 320x240 view (camera follow borders 128/96; `oLevel` uses the view for water and activation) | 2026-10-03 | User choice. HD comments out 1.1's `instance_deactivate_region` (`refs/hd/src/objects/oLevel/Step_0.gml:75`): every instance runs every step, so the view no longer decides which enemies run; `oWater.checked` (line 70) still uses it. Only the drawing crops 8 lines at the top and 8 at the bottom |
| Exactness: the GameMaker runtime's RNG (2024.14; `randomize()` once, `oIntro` Create) and the level generator (`scrLevelGen`, `scrRoomGen*`, `scrEntityGen`, `scrTreasureGen`, `scrShopItemsGen`) bit-exact, so a seed gives HD's level. Motion in fixed point using Maldita's F16 formats (positions s13.18, speeds / gravity / friction s7.24), gated by gameplay equivalence (§4) | 2026-10-03 | User choice. Maldita's soft-float runner was bit-exact but ran over the jtcps3 frame budget (float work was 31% of a mean step and 41% of the worst step). F16 made it fit, and its probe showed that 16.16 is not enough (gravity 0.15 -> 0.149994 changes landings), while 18 / 24 fraction bits matched every route |
| Translation follows HD's runtime semantics, not GM8's: `and` / `or` short-circuit (as C does), GameMaker 2.3 operator precedence, the GMS random generator. These change the generator's RNG call sequence compared with 1.1 (`docs/VARIANTS.md` HD2, HD3); HD's own runner is the reference | 2026-10-03 | Follows from the HD base |
| Native code, not a GML runner: GML translated by hand into typed C, statement for statement, with integer types wherever the GML values are whole numbers. There are no GML variants and no generic event dispatch beyond tables | 2026-10-03 | Maldita: the generic runner (binary64 GET / setters, depth-sorted float lists) is where the time went |
| Terrain as a grid, not instances: `oSolid` blocks (no Step event: `oBlock`, `oBrick`, `oSolid` have only Create / Destroy) live in a 42x34 cell array, and `collision_point(x, y, oSolid, ...)` becomes a cell lookup plus a short list of moving solids. Activation rules modelled where HD still uses them | 2026-10-03 | 495 `collision_point` calls (HD about the same), most of them against terrain; GameMaker tests every instance's mask |
| Step at 30 Hz (`room_speed` 30) on a 59.6 Hz display: one game step every second frame, with the display list sent each frame | 2026-10-03 | Twice Maldita's budget per step: about 839,000 jtcps3 cycles a step |

## 1b. Performance revert candidates

HD behaviour kept for now that may be reverted to 1.1's if the jtcps3 budget needs it. Each entry is a gameplay change,
so reverting one needs the user's decision and its own route check.

| # | HD behaviour | 1.1 behaviour to revert to | Cost if kept | Tagged |
|---|---|---|---|---|
| R1 | No deactivation: every instance runs every step (`refs/hd/src/objects/oLevel/Step_0.gml:75` commented out) | `instance_deactivate_region` outside the view ±96 px, then the reactivation list (`build/gml/oLevel/Step.gml:64-100`) | Per-step cost scales with the whole level's instances instead of the view's; measured at P4 / P5 (CPU per route on jtcps3) | 2026-10-03, the user |

## 2. Measured (2026-10-03)

HD 1.2.2 (`refs/hd/src`, research agent's counts at HEAD b4a3999, 12 commits after the tag; re-measure at the tag in P2):
459 objects, 318 scripts (117 Scribble), 29 rooms, 841 sprites / 2,327 frames (530 of them 16x16), 67 sounds (8 OGG
music, 59 WAV effects, 9.2 MB); 38,773 GML lines without Scribble; `rand(` 933; game speed 30; `rLevel` 672x544 with a
320x240 view. Darkness: one black rectangle at alpha `darkness` over the whole view (`objects/oLevel/Draw_0.gml`): a
palette fade on CPS3, no per-pixel light needed.

HD art for the CPS3 (2026-10-03, every `refs/hd/src/sprites` PNG but Scribble's): 840 sprites, 2,326 frames; 6,816 tiles
of 16x16 without deduplication (1.7 MB, against 8 MB / 32,768 tiles of character RAM, `bg*` backgrounds included);
**204 distinct 5-bit colours across all of them** (at most 28 in one sprite); 39 partially transparent pixels in all;
largest frames 640x240, 480x200. So: one 256-colour palette for everything (8-bit tiles, one colour code), all art
resident in character RAM, loaded once by character DMA at boot (no per-area loading, unlike Maldita); fades and the
dark-level rectangle as colour-code copies of that palette, selected per main-list record or by palette DMA. Converter (`tools/hdsprites.py`, checked by `tools/sprcheck.py`: all 2,326 frames redrawn from pieces and tiles
equal the source after colour mapping): 2,682 pieces (at most 50 in one frame), 5,012 tiles after deduplication
(1.22 MB), 200 palette colours under jtcps3's expansion.

1.1, for reference (`tools/gmk2gml.py build/gmk build/gml`):

| Area | Value |
|---|---|
| Extraction | `gmksplit.jar` (Medo42 Gmk-Splitter v0.21, run in `eclipse-temurin:17-jre`) -> `build/gmk`; `tools/gmk2gml.py` -> one `.gml` per object event in `build/gml/<object>/` |
| Code | 35,259 GML lines (scripts and object events); 453 objects, 1,001 events, 154 scripts; drag-and-drop actions: 284 `action_inherited`, 31 `action_kill_object` and 34 others |
| Rooms | 32; play room `rLevel` 672x544 (42x34 cells of 16 px) with a 320x240 view following `oPlayer1` (borders 128 / 96), speed 30 |
| RNG | `rand(a, b)` = `floor(random(b - a + 1)) + a` (940 calls), `random(` 27. `randomize` and `random_set_seed` are never called: GM8 seeds at start-up |
| Collision | `collision_point` 495, `collision_rectangle` 87, `instance_place` 47, `collision_line` 36, `place_meeting` 4; built-in `hspeed` / `gravity` hardly used (1 / 4): motion is the game's own `xVel` / `yVel` code |
| Activation | `oLevel` Step: deactivate outside view ±96, then reactivate characters, ropes, explosions, ghost, boss and regions around the player, boulder and Olmec (HD removes the deactivation) |
| Drawing | Surfaces (`oScreen`: the screen, `pSurf`, `darkSurf`), blend modes (multiply for dark levels, add for light), `image_alpha` 20, `image_angle` 22 |
| Assets | 2,299 sprite frame PNGs; sound 1.1 `sound/` (WAV effects, OGG music) via `supersound.dll` |

## 2b. GameMaker 2024.14 rules established against the runner (P2, 2026-10-03)

| Rule | Evidence |
|---|---|
| Instance ids: room instances keep their stored ids (rLevel: oPlayer1 104034, oRoom 104017-104032, oLevel 104016, oGame 104033); every `instance_create` takes the next value of one global counter (+1, carried across restarts) | All 1,600 generator cases |
| `with (obj)` and instance searches visit newest first (reverse creation order) | `with oRoom` / `with oSolid` RNG order, all cases |
| Create runs inside `instance_create` (nested); Destroy runs at `instance_destroy` | Rubble from a block's Destroy listed right after the push block that replaced it |
| `and` / `or` short-circuit; function arguments evaluated right to left (the last first); operands of one expression left to right | Runtime bytecode (`build/p2/disasm.txt`); rubble positions |
| `option_collision_compatibility` false: bounding boxes exclusive at right / bottom; a `collision_rectangle` touching an edge misses; generator calls use prec = 0 (bbox only) | All cases; the HTML5 runner's code |
| `with`: newest first, except exactly two matching instances, which go oldest first (1-9 instances and after destroys checked); instances destroyed during the loop are skipped | Runner probe `build/p2/probe`, `TRACE_GEN_ORDER=all` dumps |
| `collision_point`, `instance_find(obj, 0)`, `obj.var` and `instance_nearest` ties return the oldest instance | Same |
| Room start: every room instance exists before the first Create runs; instances created by those Creates come after all room instances. Room instances take their layer's depth, not the object's | rOlmec cases |
| `layer_get_all_elements` lists tiles newest first (`tile_layer_find` uses it) | tile lists per depth, all cases |
| Not tested: whether `with` visits instances created inside its own loop | No generator loop does it |

## 3. What carries over from Maldita and the SDK

| Learning (source) | Applied here |
|---|---|
| Budget against jtcps3, not MAME: 2.6-3x slower; about 2.5 cycles an instruction from SIMM 1, 4.5-8.5 a load or store off the CPU, 6 a sprite-RAM store (`cps3-testgame` `ttest`, Maldita C3) | Every CPU check on jtcps3 cycles; MAME cycles x 2.94 as a pre-check |
| Hot code placed together at the start of SIMM 1 (`.text.hot`); `struct` layout by 16-byte cache line; `-O3`; no libgcc variable shifts or 64-bit divides in hot paths; DIVU for division | From the first build; `-fno-bit-tests` (P2: GCC 13 compiles large switches to `___ashlsi3` calls) |
| Status and inputs read through the uncached mirror (the cached sprite-list-DMA wait cost about 69,000 cycles a frame) | SDK `cps3v_vblank` / `cps3io` already do it |
| One prebuilt sublist per sprite frame (plain and mirrored); one 4-word main-list record per instance; the camera in a global scroll register; at most 511 list entries a frame on jtcps3; 8-bit tiles only; pieces up to 4x4 tiles | The sprite converter writes the sublists; terrain goes on tilemaps |
| Tiles reach character RAM only by character DMA from SIMMs 3-6 (CPU writes with the display on are lost on jtcps3; reloading by CPU does not work); wait on IRQ 10, not the busy bit | All art resident: one character DMA at boot (§2) |
| Samples below 16 MB of sample flash (jtcps3 reads offsets mod 16 MB); music 16 kHz, effects 32 kHz, 8-bit | HD's audio is 9.2 MB at 44.1 kHz and fits easily |
| Fixed-point motion formats and the route check / G-check gate (Maldita §10 F16) | §1 exactness decision; tools reused (`routecmp.py`, `tracecmp_eq.py` adapted) |
| Reference traces from the original game, the host runner as the CPS3 build's reference, MAME traces bit-exact to the host, jtcps3 hashes equal to MAME's | §4 |
| The flash cannot be rewritten on jtcps3: save data (high scores, tunnel-man progress) goes in the EEPROM | Later milestone |

## 4. Verification

1. **Reference:** HD 1.2.2's `game.droid` with tracer GML injected by UndertaleModTool (Maldita's `tools/tracer.py`
   method), run by the release's own GameMaker Linux runner (1.2.2 linux-arm64 build) in Docker on the Mac (P1; not gmloader on the MiSTer: the user, 2026-10-03, it is not a faithful runtime) with `random_set_seed` in place of `randomize` and recorded
   inputs, dumping the state at each step: the RNG seed, each instance's id, object, x, y, sprite, frame,
   alarms and the `xVel` / `yVel` variables, `global` score, life, bombs and ropes, the level number and the room.
2. **Generator (bit-exact):** for seeds x every area (1-4 mines, 5-8 lush, 9-12 ice, 13-16 temple, the boss level,
   shops, the moon / Yeti / city of gold variants), the instance list after level creation is equal to the reference
   (object, x, y, and the creation variables that change play). The RNG's state equals the reference after generation.
3. **Play (gameplay-equivalent, Maldita F16 rules):** a route check (same rooms in order, same deaths, damage,
   score, items collected and level-entry steps within 1), plus the G-check reported per route (discrete state equal;
   x / y within 1/16 px). The host fixed-point runner is the reference for the CPS3 build: MAME traces bit-exact to
   the host's, jtcps3 hashes equal to MAME's.
4. **Frames:** MAME snapshots against frames composed on the host from the trace state; jtcps3 screenshots.
5. **CPU:** on jtcps3, no step over its two-frame slot except at level start; worst step recorded per route.

## 4b. Milestones

| # | Result | Verified by |
|---|---|---|
| P0 | Sources and tooling: 1.1 extracted (done), HD 1.2.2 source and APK in `refs/hd` (done); repository scaffold on the SDK (`../cps3-testgame/sdk`, `sdk.mk`) with a `hello` that boots in MAME | MAME boot |
| P1 | **Reference runner.** HD 1.2.2's Linux build (official GameMaker runner, linux-arm64, native in Docker on Apple Silicon with Xvfb; x86_64 AppImage under emulation as fallback) with tracer GML injected into its data file by UndertaleModTool's CLI: fixed seed, inputs from a route file, a per-step dump (RNG state if readable, each instance's id, object, x, y, sprite, frame, alarms, `xVel` / `yVel`; globals: level, life, bombs, ropes, money). Not gmloader on the MiSTer (user, 2026-10-03) | Two runs with one seed and one input file give identical dumps; a dumped level matches the screen. **Done 2026-10-03:** `scripts/hd_trace.sh <route> <seed>`, `tools/tracer.py`, `docker/hd-runner`. UndertaleModTool's CLI re-saves `game.unx` and `game.droid` byte-identically (bytecode 17, 1,940 code entries). Inputs replace `oGamepad`'s Step (`checkX()` reads it). Records: one at each room start (the generated level) and one per End Step. Route `p1_walk`, seed 1: two runs byte-identical (406 records, md5 3e7bd4ef...); seed 2 a different level (762 vs 799 instances). About 20 s a run; the runner crashes after `game_end()` (exit 134 / 139) once the trace is written. The Linux `game.unx` differs from the APK's in 4 code entries (`getPlatform`, `characterStepEvent`'s run release, `getWorkingDirPath`, `oDebug`): the Linux data is the reference. Screen check (`TRACE_SHOT`, `tools/screencheck.py`): at records 1, 46 and 60 of `p1_walk` seed 1, all 169 visible terrain cells of the dump are terrain in the runner's frame and none of the other 236 cells is |
| P2 | **RNG and generator on the host (C):** the 2024.14 runtime's `random` (model from the reference: seed, draw sequence), `scrLevelGen` / `scrRoomGen*` / `scrEntityGen` / `scrTreasureGen` / `scrShopItemsGen` translated; level as cell grid + instance list | §4 item 2 on 200 seeds x every area **Done 2026-10-03, all areas** (`src/game/{rng,inst,gen,genroom,genobj,genent}.c`, `test/host/genhost`, `tools/gencmp.py`, `tools/hdgentables.py`, `tests/gen/*.txt` from `tests/gen/mkcases.py`; reference `scripts/hd_trace.sh --gen <cases>`): 8,080 of 8,080 cases equal, instances and the 4 RNG draws after generation: mines 1,600, lush 2,200, ice 1,400, temple 1,600, olmec 400, carried items 800, chains 1->16 80 (6.26 M instances, tile_add lists per depth, generator globals, roomPath, exit type, treasure). Variants reached: lake, black market, cemetery, yeti lair, alien craft, sacrifice pit, city of gold, dark level, Lady Xoc, moai, murderer, thief, Kali chain. About 3.5 ms a level on the host; ASan / UBSan clean; at most 1,404 instances in a level (capacity 2,048); SH-2 objects without floats or libgcc (`-fno-bit-tests`: GCC 13 otherwise turns large switches into `___ashlsi3`), bss 184 KB. Not modelled (no case reaches them): precise-mask `instance_place`, oDoor Destroy, the frozen caveman's `invincible = 20` |
| P3 | **Display bring-up:** full-screen X zoom (works on MAME and jtcps3, §5): X zoom 0x35, 318 of 320 px at x1.208. Then: all art by one DMA at boot (§2), the generated level on one 64x64 tilemap (672x544 fits: no streaming), the camera, the 8-line crop | MAME frames exact against host-composed frames; jtcps3 screenshots **In progress 2026-10-03:** `tests/view` + `tools/viewlevel.py` (level viewer from an HD-runner dump: bgCave on tilemap 0, terrain on tilemap 1, 193 sprites in object-depth order, zoom 0x35, 8-line crop; `scripts/view_check.sh`): 6 cameras pixel-exact in MAME. jtcps3 (.81, `.rbf` 2026-10-02): every camera phase equals the jtcps3 model (source column ((X + 1) * fsz + 0x5800) >> 16, colours v << 3 | v >> 2) except screen column 0 (170 of 224 rows, the screenshot edge). **Drawing model checked against the runner (2026-10-03):** `tools/viewlevel.py shot` composes a record's 320 x 240 view from the SPT2 trace (traced depth, mirroring by image_xscale, the tile_add layers, terrain) and compares it with the runner's own frame (`TRACE_SHOT`) in 5-bit colour: p1_walk seed 1 record 46: 0 of 76,800 pixels differ; record 1: 90, all on oPlayer1, whose Draw event (characterDrawEvent) draws it, not image_xscale (33 objects have Draw events: drawn by the runtime, P4/P5). The CPS3 viewer now draws that model (tilemaps for bgCave and the depths 10002, 100, 3; 1,029 drawables in depth order) and is exact in MAME at 6 cameras. **jtcps3 recheck of the current viewer (.81, 2026-10-03):** 13 screenshots of 4 camera phases against the jtcps3 model built from the MAME-exact expected screens, 5-bit colour: all pixels equal except screen column 0 (the screenshot edge), column 383 (source column 318, not in the MAME screens, so unchecked) and 0 to 13 single pixels per shot that take their right neighbour's colour, at different places in every shot of the same static frame (2 of 8 shots: none), so not from the display list. Open: Draw-event objects; GameMaker's order for equal depths (both orders gave the same frame here) |
| P4 | **Player and core physics:** `oPlayer1` (2,603 GML lines in events) and `characterStepEvent` (1,049), `moveTo` and the collision scripts, ladders, ropes, whip, hang, crouch, damage, bombs, items held / thrown. HD motion: positions stay whole pixels (`moveTo` steps 1 px at a time against solids); fractional `xVel` / `yVel` (binary64 in GameMaker) become integer steps by `oGame.time mod round(1 / frac(abs(v)))`. So the fixed-point question of §1 sits in the velocities: a velocity that drifts from GameMaker's double can flip `round(1/frac)` or a comparison. **First measurement:** the host runner with binary64 velocities (exact) against s7.24, on the reference routes; if s7.24 changes a route, keep binary64 for the velocity fields only and measure its SH-2 cost (Maldita: about 50 instructions an add) **Play runtime and player, 2026-10-03:** `src/game/p*.c` (GameMaker 2024.14 step order from `DoAStep_Update`, event dispatch by object order then oldest instance, alarms, GML real compare with epsilon 1e-5, float32 x/y/image fields, round half to even, camera from camera_get_view). Enemies removed (`TRACE_NOENEMY=1`, `scripts/p4_trace.sh`): p4_exit559 806/806, p4_hang_ladder 345/345, p4_items 278/278, p4_spikes 183/183, p4_push_rope 376/376 records equal; p1_walk (bomb) equal to record 309, differs at 310: the runner orders collision events through its R-tree (`RTree<CInstance*,float,float,6,2>`, built by the room's insert/remove history), `pcol.c` uses creation order. Not modelled: restart after death. **Arithmetic decided by the §1 gate:** s7.24 / s13.18 changes positions on 5 of 6 routes and the discrete state on 3 (`tools/playfx.py`), so velocities and positions stay binary64 / float32 as GameMaker (`-DPLAY_FIXED` kept for measurement). Cost: mean 328-498 double operations a step, max about 3,400 (the level's first step); SH-2 soft-float timing not yet measured. | Routes on the first mines levels: route check + G-check against P1 references |
| P5 | Mines content (enemies, traps, items, shopkeeper, damsel, idols, arrow traps), activation region model, level transitions (`rTransition*`) | Routes through levels 1-4 |
| P6 | Sound: HD's effects and music through the SDK's 16 voices. **Converter done 2026-10-03** (`tools/hdsound.py`, run in the cps3-dev image): 67 sounds, 440 s, all at 32 kHz 8-bit = 13.45 MB, inside the 16 MB window; music loops to its start; SNR 40-42 dB on the music; at most 46 samples (0.004%) clip. Playback checked (`tests/sound` jukebox, `scripts/sound_check.sh`, `tools/sndcheck.py`, `scripts/lua/sndlog.lua`): in MAME all 67 sounds key on in table order on their voices and 80 s of output equal the chip model within 1 LSB at every sample. Open: the game's audio rules (`playSound`, `playMusic`, loops, stops, voice allocation) driven by the runtime, the key-on check against the trace's sound calls, jtcps3 by ear | MAME key-on log equal to the trace's sound calls **Audio rules done 2026-10-03:** `src/snd` (HD playSound / playMusic / audio_* semantics, voice 0 music, voices 1-15 effects stacking, priority stealing; rules and the cases that cannot match GameMaker in docs/AUDIO.md); `scripts/sndrules_check.sh`: 137 of 137 key-on/off events equal to a Python model frame by frame, 40 s within 1 LSB of the chip model. Open: the game calls (startMusic / onOffMusic in the runtime), how often HD exceeds 15 effects, audio_is_playing while paused |
| P7 | Lush, ice, temple, the boss; dark levels (HD: a whole-view alpha rectangle, done as a palette fade) | Routes per area; hand playthrough |
| P8 | Title, attract, HUD, high scores in EEPROM, arcade coin and start **Shell and HUD done 2026-10-03:** `src/shell` (input as tracer KEYS bits, oGamepad pressed/released, credits, coin/start, free play, game step every 2nd frame, HD's spelunky.ini values in EEPROM words 16-28), `src/hud` (scrDrawHUD / showMessages; fonts by `tools/hudart.py`). HUD: 8 states exact in MAME and equal to the HD runner's own HUD (0 of 76,800 px each, `scripts/hd_hudref.sh`); shell: 47/47 host, 26/26 MAME (`scripts/shell_check.sh`); docs/ARCADE.md. Pay on Start (decided; pause, if wanted, on a button combo). Open: the crop cuts the lower compass arrows, jtcps3, attract content | MAME and jtcps3, the user's hand check |

## 5. Open questions

- **Resolved 2026-10-03: the runtime's RNG** (`tools/gmrand.py`, checked bit-exact on 60,000 draws of `random`, `irandom` over 6 seeds from `scripts/hd_trace.sh --rng-probe`; algorithm read from the APK's `libyoyo.so` symbols `InitRandom`, `YYRandom`, `fYYRandom`). WELL512a, 16 x u32 state: seeded by x0 = (u32)S, x(k+1) = ((x(k) * 0x343FD + 0x269EC3) mod 2^32) >> 16, state[k] = x(k+1); output the standard WELL512a word. `random(n)` = (double)u * 2^-32 * n; `irandom(n)` takes two draws: ((u2 & 0x7FFFFFFF) << 32 | u1) mod (n + 1). For integer n <= 2^21, `floor(random(n))` = (u * n) >> 32 exactly, so `rand(a, b)` = ((u * (b - a + 1)) >> 32) + a: the generator is integer-only (one `dmulu.l` on the SH-2).
- gmloader-next on the MiSTer (not used): three builds stop with SIGILL at libyoyo+0x1fc on the first `Function_Add` call (`gmloader-next-mill/gmloader/libyoyo.cpp:475`) for this 2024.14 runner.
- CPU: HD runs every instance every step (no deactivation): per-step cost grows with the whole level's enemies, not the view's.
- **Resolved 2026-10-03: full-screen X zoom works on jtcps3** (MiSTer .81, `jtcps3.rbf` 2026-10-02, `tests/zoom`). MAME 0.289: 0x40, 0x35 and 0x36 pixel-exact against MAME's formula. jtcps3: 0x40 exact (with the known 1-px screenshot offset); zoomed, screenshot column X shows source column `((X + 1) * fsz + o) >> 16` with o about 0x5600 (0 of the identifiable columns off for both zooms), i.e. MAME's stepping at a different sampling phase: 3,472-5,376 px differ from MAME's screen. Choice for P3: 0x35 (318 of 320 px, x1.208). Frame checks of zoomed screens compare MAME with MAME; jtcps3 screenshots are compared after the zoom is undone by the fitted rule.
- The 2024.14 runtime's `random` at the bit level (GM8's was an LCG; the GMS 2 generator is not documented here): from reference draws at P2.
- `image_alpha` effects on hardware without blending (P7).
