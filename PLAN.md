# Spelunky Classic on CPS3: a native ROM port plan

Goal: Spelunky v1.1 (Derek Yu, 2008-2009, GameMaker 8) re-implemented as a homebrew ROM set for the Capcom CPS3. The
set runs on MAME (`cps3` driver, stand-in set `sfiii3na`) and on the MiSTer's jtcps3 core. Gameplay matches 1.1. The
game is built natively on the SH-2 and the CPS3 PPU with everything learned in `../maldita.castilla-cps3`, and it uses
the SDK in `../cps3-testgame/sdk`. It is not a GameMaker runner.

**Licence.** The Spelunky User License (`refs/src_1_1/COPYING.txt`) allows redistribution and modification but not
sale. Game assets and the files converted from them are still never committed: converters build them from
`refs/` (git-ignored).

## 1. Decisions

| Decision | Date | Reason |
|---|---|---|
| Source of truth: the original v1.1 source, `spelunky_1_1_src.zip` (`spelunky.gmk`, GM8 format 800), with data files from the released `spelunky_1_1.zip` (`Spelunky.exe`, `sound/`) | 2026-10-03 | The only candidate that is the 1.1 code. Classic HD (yancharkin) is a GameMaker 2.3+ rewrite of the 2015 Humble GMS port ("some tweaks", different lighting); it is used only as readable GML for cross-checking. Spelunky SD has no public source and changes gameplay on purpose |
| Display: the 320-px view stretched to 384 horizontally; 224 of its 240 lines shown. All game logic keeps the 320x240 view (camera follow borders 128/96, `oLevel` deactivation region view ±96) | 2026-10-03 | User choice. The view size is part of the gameplay: `oLevel/Step.gml:75` deactivates every instance outside the view ±96 px, so shrinking the logical view would change which enemies run. Only the drawing crops 8 lines at the top and 8 at the bottom |
| Exactness: the GM8 RNG and the level generator (`scrLevelGen`, `scrRoomGen*`, `scrEntityGen`, `scrTreasureGen`, `scrShopItemsGen`) bit-exact, so a seed gives 1.1's level. Motion in fixed point using Maldita's F16 formats (positions s13.18, speeds / gravity / friction s7.24), gated by gameplay equivalence (§4) | 2026-10-03 | User choice. Maldita's soft-float runner was bit-exact but ran over the jtcps3 frame budget (float work was 31% of a mean step and 41% of the worst step). F16 made it fit, and its probe showed that 16.16 is not enough (gravity 0.15 -> 0.149994 changes landings), while 18 / 24 fraction bits matched every route |
| Native code, not a GML runner: GML translated by hand into typed C, statement for statement, with integer types wherever the GML values are whole numbers. There are no GML variants and no generic event dispatch beyond tables | 2026-10-03 | Maldita: the generic runner (binary64 GET / setters, depth-sorted float lists) is where the time went |
| Terrain as a grid, not instances: `oSolid` blocks (no Step event: `oBlock`, `oBrick`, `oSolid` have only Create / Destroy) live in a 42x34 cell array, and `collision_point(x, y, oSolid, ...)` becomes a cell lookup plus a short list of moving solids. GM8's activation rules are modelled (a deactivated block is invisible to collisions) | 2026-10-03 | 495 `collision_point` calls in 1.1, most of them against terrain; GM8 tests every instance's mask |
| Step at 30 Hz (`room_speed` 30) on a 59.6 Hz display: one game step every second frame, with the display list sent each frame | 2026-10-03 | Twice Maldita's budget per step: about 839,000 jtcps3 cycles a step |

## 2. Measured from 1.1 (2026-10-03, `tools/gmk2gml.py build/gmk build/gml`)

| Area | Value |
|---|---|
| Extraction | `gmksplit.jar` (Medo42 Gmk-Splitter v0.21, run in `eclipse-temurin:17-jre`) -> `build/gmk`; `tools/gmk2gml.py` -> one `.gml` per object event in `build/gml/<object>/` |
| Code | 35,259 GML lines (scripts and object events); 453 objects, 1,001 events, 154 scripts; drag-and-drop actions: 284 `action_inherited`, 31 `action_kill_object` and 34 others |
| Rooms | 32; play room `rLevel` 672x544 (42x34 cells of 16 px) with a 320x240 view following `oPlayer1` (borders 128 / 96), speed 30 |
| RNG | `rand(a, b)` = `floor(random(b - a + 1)) + a` (940 calls), `random(` 27. `randomize` and `random_set_seed` are never called: GM8 seeds at start-up |
| Collision | `collision_point` 495, `collision_rectangle` 87, `instance_place` 47, `collision_line` 36, `place_meeting` 4; built-in `hspeed` / `gravity` hardly used (1 / 4): motion is the game's own `xVel` / `yVel` code |
| Activation | `oLevel` Step: deactivate outside view ±96, then reactivate characters, ropes, explosions, ghost, boss and regions around the player, boulder and Olmec (`instance_activate_region(oPlayer1.x-16, oPlayer1.y-16, oPlayer1.x+16, oPlayer1.y+16)` passes coordinates as the width and height: a 1.1 quirk the port must keep) |
| Drawing | Surfaces (`oScreen`: the screen, `pSurf`, `darkSurf`), blend modes (multiply for dark levels, add for light), `image_alpha` 20, `image_angle` 22 |
| Assets | 2,299 sprite frame PNGs; sound 1.1 `sound/` (WAV effects, OGG music) via `supersound.dll` |

## 3. What carries over from Maldita and the SDK

| Learning (source) | Applied here |
|---|---|
| Budget against jtcps3, not MAME: 2.6-3x slower; about 2.5 cycles an instruction from SIMM 1, 4.5-8.5 a load or store off the CPU, 6 a sprite-RAM store (`cps3-testgame` `ttest`, Maldita C3) | Every CPU check on jtcps3 cycles; MAME cycles x 2.94 as a pre-check |
| Hot code placed together at the start of SIMM 1 (`.text.hot`); `struct` layout by 16-byte cache line; `-O3`; no libgcc variable shifts or 64-bit divides in hot paths; DIVU for division | From the first build |
| Status and inputs read through the uncached mirror (the cached sprite-list-DMA wait cost about 69,000 cycles a frame) | SDK `cps3v_vblank` / `cps3io` already do it |
| One prebuilt sublist per sprite frame (plain and mirrored); one 4-word main-list record per instance; the camera in a global scroll register; at most 511 list entries a frame on jtcps3; 8-bit tiles only; pieces up to 4x4 tiles | The sprite converter writes the sublists; terrain goes on tilemaps |
| Tiles reach character RAM only by character DMA from SIMMs 3-6 (CPU writes with the display on are lost on jtcps3; reloading by CPU does not work); wait on IRQ 10, not the busy bit | Per-area tile sets (mines, lush, ice, temple, boss) loaded by DMA at level start |
| Samples below 16 MB of sample flash (jtcps3 reads offsets mod 16 MB); music 16 kHz, effects 32 kHz, 8-bit | 1.1's audio is about 9 MB at 44.1 kHz and fits easily |
| Fixed-point motion formats and the route check / G-check gate (Maldita §10 F16) | §1 exactness decision; tools reused (`routecmp.py`, `tracecmp_eq.py` adapted) |
| Reference traces from the original game, the host runner as the CPS3 build's reference, MAME traces bit-exact to the host, jtcps3 hashes equal to MAME's | §4 |
| The flash cannot be rewritten on jtcps3: save data (high scores, tunnel-man progress) goes in the EEPROM | Later milestone |

## 4. Verification

1. **Reference:** the real 1.1 (`Spelunky.exe`) run under a deterministic GM8 runner (P1) with a fixed seed and
   recorded inputs, dumping the state at each step: the RNG seed, each instance's id, object, x, y, sprite, frame,
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
| P0 | Sources and tooling: 1.1 extracted to text and PNGs (done: §2); repository scaffold on the SDK (`../cps3-testgame/sdk`, `sdk.mk`) with a `hello` that boots in MAME | MAME boot |
| P1 | **Reference runner.** OpenGMK `gm8emulator` (Rust, built in Docker) running `Spelunky.exe` with a set seed and an input file; a per-step state dump. Risk: `supersound.dll` (sound only; stub it if the runner cannot load it), GM8 version detection, Linux display in Docker. Fallback: HD's GML in GameMaker is not 1.1 and cannot serve as the reference; a minimal GM8 interpreter of the extracted GML is the last resort | Two runs with one seed and one input file give identical dumps; the dump's first level matches a screenshot of the real game |
| P2 | **RNG and generator on the host (C):** GM8's `random` (OpenGMK's model of the GM8 generator), `scrLevelGen` / `scrRoomGen*` / `scrEntityGen` / `scrTreasureGen` / `scrShopItemsGen` translated; level as cell grid + instance list | §4 item 2 on 200 seeds x every area |
| P3 | **Display bring-up:** full-screen X zoom (works on MAME and jtcps3, §5) (`m_ppu_crtc_zoom[3]`; 0x35 shows 318 of 320 px at x1.207, 0x36 324 px) checked on MAME and jtcps3 by a new `cps3-testgame` test. If jtcps3 does not zoom, the fallback is decided then: levels are generated at run time, so stretched terrain needs a tile for every (column phase, left block, right block) combination (5 source columns -> 6 CPS3 columns) built into the flash; or 1:1 pixels. Then: area tile sets by DMA, the generated level on one 64x64 tilemap (672x544 fits: no streaming), the camera, the 8-line crop | MAME frames exact against host-composed frames; jtcps3 screenshots |
| P4 | **Player and core physics:** `oCharacter` / `oPlayer1` movement, ladders, ropes, whip, hang, crouch, damage, bombs, items held / thrown; fixed point per §1 | Routes on the first mines levels: route check + G-check against P1 references |
| P5 | Mines content (enemies, traps, items, shopkeeper, damsel, idols, arrow traps), activation region model, level transitions (`rTransition*`) | Routes through levels 1-4 |
| P6 | Sound: 1.1's effects and music through the SDK's 16 voices | MAME key-on log equal to the trace's `playSound` calls |
| P7 | Lush, ice, temple, the boss; dark levels (no blending on CPS3: palette darkening per tilemap cell and per sprite, design decided at P7, visual only) | Routes per area; hand playthrough |
| P8 | Title, attract, HUD, high scores in EEPROM, arcade coin and start | MAME and jtcps3, the user's hand check |

## 5. Open questions

- P1: whether OpenGMK runs Spelunky 1.1 (GM8.0 exe, `supersound.dll`) on Linux in Docker. Unknown until tried.
- **Resolved 2026-10-03: full-screen X zoom works on jtcps3** (MiSTer .81, `jtcps3.rbf` 2026-10-02, `tests/zoom`). MAME 0.289: 0x40, 0x35 and 0x36 pixel-exact against MAME's formula. jtcps3: 0x40 exact (with the known 1-px screenshot offset); zoomed, screenshot column X shows source column `((X + 1) * fsz + o) >> 16` with o about 0x5600 (0 of the identifiable columns off for both zooms), i.e. MAME's stepping at a different sampling phase: 3,472-5,376 px differ from MAME's screen. Choice for P3: 0x35 (318 of 320 px, x1.208). Frame checks of zoomed screens compare MAME with MAME; jtcps3 screenshots are compared after the zoom is undone by the fitted rule.
- GM8's `random` at the bit level: OpenGMK documents the generator; confirm against the reference at P2.
- Dark levels and `image_alpha` effects on hardware without blending (P7).
