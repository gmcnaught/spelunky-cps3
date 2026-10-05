# Arcade shell and HUD (PLAN.md P8)

Code: `src/shell/` (inputs, credits, high scores, main loop), `src/hud/` (HD's in-game HUD). Checks: `tests/shell`
(`scripts/shell_check.sh`), `tests/hud` (`scripts/hud_check.sh`, `scripts/hd_hudref.sh`).

## 1. Controls

HD reads every control through the `check*` scripts (`refs/hd/src/scripts/check*`), each one `oGamepad.<key>` (or
`<key>Pressed` / `<key>Released`) OR the keyboard key. `oGamepad`'s Step (`objects/oGamepad/Step_0.gml`) sets, once a
step and for each key: Released = held at the previous step and not now, Pressed = not held before and held now.
The port has one per-step struct with the same three masks (`struct shell_input`: `down`, `pressed`, `released`),
using `tools/tracer.py`'s route bits, so routes, the host runner (`play_step(keys)`) and the cabinet all give the
play code the same 16-bit mask.

| Bit | Route letter | HD check scripts | HD keyboard default | CPS3 (both panels) |
|---|---|---|---|---|
| `KEY_RIGHT` 0x0001 | R | `checkRight`, `checkRightPressed`, `checkRightReleased` | Right arrow | Stick right |
| `KEY_LEFT` 0x0002 | L | `checkLeft*` | Left arrow | Stick left |
| `KEY_UP` 0x0004 | U | `checkUp`, `checkUpPressed` | Up arrow | Stick up |
| `KEY_DOWN` 0x0008 | D | `checkDown`, `checkDownPressed` | Down arrow | Stick down |
| `KEY_JUMP` 0x0010 | J | `checkJump`, `checkJumpPressed`, `checkJumpReleased` | Z | B1 (LP) |
| `KEY_ATTACK` 0x0020 | A | `checkAttack`, `checkAttackPressed`, `checkAttackReleased` | X | B2 (MP) |
| `KEY_ITEM` 0x0040 | I | `checkItemPressed` | C | B3 (HP) |
| `KEY_RUN` 0x0080 | N | `checkRun`, `checkRunPressed` | Shift | B4 (LK) |
| `KEY_BOMB` 0x0100 | B | `checkBombPressed` | A | B5 (MK) |
| `KEY_ROPE` 0x0200 | O | `checkRopePressed` | S | B6 (HK) |
| `KEY_FLARE` 0x0400 | F | `checkFlarePressed` (its only use, `oPlayer1/Step_0.gml:415`, is commented out) | F | none |
| `KEY_PAY` 0x0800 | P | `checkPayPressed` (buy the held shop item, `oPlayer1/Step_0.gml:1327`) | P | Start, in play |
| `KEY_START` 0x1000 | S | `checkStartPressed` (intro / title / transition skip, restart after death, pause) | Escape | Start, outside play (`INPUT_START_MENU`) |

Not mapped: `checkLangPressed` (language, VARIANTS X3), the F-keys (`oDebug`, restart F6: X8), the key / pad configuration (X4).
The run toggle (X10) is the settings screen's RUN BUTTON (section 2; HOLD by default, so `checkRun` is the held
button). `global.downToRun` stays HD's default (true, `scrInit:50`).

**Start and pay.** HD needs seven play buttons (jump, whip, item, run, bomb, rope, pay); the CPS3 panel has six. Start
is pay during play (`INPUT_START_PAY`, `struct shell`'s `start_mode`; the game can switch it to `INPUT_START_MENU`),
so HD's pause on Start (`oScreen/Step_1.gml:32`) is not reachable on the cabinet (VARIANTS X9: the arcade has no
quit). Decided (user, 2026-10-03): Start stays pay; if pause is wanted it goes on a button combo. The places that take `checkAttackPressed() or checkStartPressed()` (transition skip, restart
after death, title) still work with B2.

**Panels.** The game is single-player. The Start button that begins a game picks the panel (P1 or P2) that controls
it; the other panel is ignored until the game ends.

**Timing.** 30 game steps a second on a 59.6 Hz display: one step every second frame (PLAN §1). Controls are sampled
every frame; a step sees the state on its own frame, plus any button that went down on the frame between and was
let go before the step (a tap shorter than a step is one step held, then released).

## 2. Credits and the main loop

`shell_run` (`src/shell/shell.c`): ATTRACT (the game's intro, title and scores rooms run without controls; the
credit line `CREDIT n` / `FREE PLAY` on the text layer) -> a Start with a credit (or free play) takes the credit
and calls `game_begin` -> PLAY (`game_step` every second frame until it returns nonzero) -> ATTRACT. Coins 1 and 2
count toward credits (coins per credit 1-9, at most 9 credits); the service button gives one credit. On a step
frame the game builds the display list (`game_draw`); on the frame between, the PPU sends the unchanged list again.
A step frame starts at a VBlank; the frame between starts at the next VBlank, or at once (with no VBlank work) when
that VBlank went by during the step, so a step and its draw have two frames (`src/main/main.c`; before 2026-10-05 a
step over one frame made the pair three frames). A step that overruns its two frames delays the next one: no step is
skipped.

**Smooth motion** (settings SMOOTH MOTION, on by default): each draw also builds a midpoint list, every instance and
the camera halfway between the previous step's positions and this step's (new instances, moves over 32 px and camera
moves over 64 px are not blended; nothing is across a room change). The step frame's VBlank shows the midpoint list,
the VBlank between shows the step's own list from the VBlank interrupt (`draw_vbl_irq`), so the screen has 60
positions a second from 30 steps, one frame (16.7 ms) later. Game logic and the routes are unchanged (routes run
with it off; `tests/game SMOOTH=1`, `scripts/smooth_check.sh` check it). MAME cost per step pair: +7 to +9 K
(p5_snakes mean 131.6 -> 138.9 K, max 170.0 -> 180.3 K; docs/DRAW.md section 7).

Game hooks (weak no-ops in `shell.c`, for the play runtime to define): `game_boot`, `game_attract_step`,
`game_begin`, `game_step(const struct shell_input *)`, `game_draw`.

### Attract cycle (our design; HD has none)

HD's title waits for the player. The cabinet's ATTRACT mode cycles HD's own rooms instead, run by `src/front` on the
play loop (`game_attract_step`, one step every second frame, like play):

| room | how long | what runs |
|---|---|---|
| rIntro | until oIntro's own `room_goto(rTitle)`: about 815 steps, 27 s (the boot trace's room change at record 815) | the story text, the dummy's walk |
| rTitle | 900 steps (30 s; `FRONT_TITLE_STEPS`) | the title's scripted flare and sparks, the dummy who climbs down |
| rHighscores | 450 steps (15 s; `FRONT_SCORES_STEPS`) | the scores box from the EEPROM's records |

After rHighscores the cycle starts again at rIntro. A room the front end does not model (e.g. a door the title's
player walks through) also restarts it at rIntro. A credit plus Start leaves the cycle at any step: `game_begin`
calls `front_stop`, then the normal game start.

Seeding rule:
- **The intro:** oIntro's Create calls `randomize()`, which in HD seeds from the clock. On the cabinet that call
  seeds with `front_seed` (`src/front/front.c`; the route's seed in tests, 1 by default). The cabinet (PLAY=1) sets
  it to 0: each pass through rIntro seeds from `shell_seed()`, so cycles differ (the first after power-on is the same
  each boot: no input has been mixed in yet).
- **A game start:** `game_begin` seeds the generator again, with `game_cfg.seed` (tests) or `shell_seed()`.
  `shell_seed()` (`src/shell/shell.h`) is `SH.entropy`, which `shell_frame` updates every frame from the pads and
  system lines (`(e ^ pad0 ^ rot16(pad1) ^ lines) * 2654435761 + 1`): the frame of the Coin and the Start, and any
  input before, select the seed. As GameMaker's `random_set_seed`, `rng_seed` keeps 16 bits of it (65,536 generator
  states). It also runs `gen_new_game`, scrClearGlobals' part.
- **Result:** no RNG draw and no global from the attract rooms reaches a level, as HD's clock seed makes the level
  seed arbitrary there too. The front end never changes a level's seed.

Checks:
- Host, every step, against HD's boot trace (`scripts/front_host.sh`): the display list equal on all records, and the
  sound calls (`HOST_SND`, tools/sndcmp.py) equal: boot 1,301 records with 24 runner calls; the high-scores room
  201 records with none in either log.
- MAME vs model: 0 px on the gated records (`scripts/game_check.sh` ATTRACT=1 / 5).

The test switch (MAME: Service Mode; jtcps3: F2), or Coin + B2 on either panel held together for 60 frames (the
stock jtcps3 has no OSD test switch), opens the settings screen: FREE PLAY, COINS PER CREDIT, RUN BUTTON (HOLD /
TOGGLE: HD's `global.toggleRunEnabled`, X10), SMOOTH MOTION (ON / OFF, section 2), CLEAR HIGH SCORES, GAME CAPTURE
(the last game's capture, section 7), SAVE AND EXIT (up / down choose, B1 or right change,
left back, test or B1 on SAVE AND EXIT leave). Leaving stores the settings and restarts the program (the combo's coin
credit is cleared with the rest). RUN BUTTON TOGGLE applies to cabinet games only (routes keep HD's default) and each
game starts walking (HD resets `toggleRun` once, in `scrInit`).

Developer options (testing only, not in HD): `tests/game DEV=1` builds (`-DSHELL_DEV`; set `spelunkydev`, "Spelunky
Classic Arcade (Dev)") add INVINCIBLE (DEV) before GAME CAPTURE. With it on, a cabinet game's player loses no life
(src/game `play_god_hold` puts back what a step took) and the branches that kill outright do not run (crushed, the
pit, spikes, lava, oGhost, oManTrap); hits still knock back and stun. Such a game stores no high scores. Routes never
set it. scripts/release.sh builds without DEV: no menu row, and the EEPROM bit is not read.

## 3. EEPROM

HD keeps its scores in `spelunky.ini` `[highscore]` (`objects/oGlobals/Create_0.gml`, `scripts/scrUpdateHighscores`,
`scrResetHighscores`, `highscore_add2`): value1 money, value2 time (s), value3 kills, value4 saves, value5 plays,
value6 wins, value7 deaths, value8 / value9 the tunnel man's prices (`global.tunnel1 / 2`), value10 the minigames
(sun x 10000 + moon x 100 + stars). All ten are arcade-relevant (title shortcuts, scores room, transitions, the
minigame rooms) and kept. Not kept: `settings.json`, `keys.json`, `gamepad.json` (not arcade).

| Word (32-bit) | Content |
|---|---|
| 0-15 | not used (left to the stand-in set, as Maldita) |
| 16 | magic 0x53504b01 |
| 17-26 | value1 .. value10 |
| 27 | settings: bit 0 free play, bit 1 run button toggle, bit 2 smooth motion off, bit 3 INVINCIBLE (DEV builds only), bits 8-11 coins per credit |
| 28 | check: words 16-27 summed, xor 0x5a5a5a5a |

A wrong magic or check reads as HD without `spelunky.ini`: every value 0, then as HD: `global.tunnel1 / 2` take the
defaults 10001 / 20001 (read before the reset), `scrResetHighscores` stores value8 = 100001 and value9 = 200001,
`firstTime` is set. `hs_update(type)` is `scrUpdateHighscores(type)` (0 death, 1 win, 2 minigame), including
`global.time = floor(global.time / 1000)` (note: `oGame` Step adds `game_speed` = 30 a step, so this "seconds" value
is 0.9 s of play; HD's behaviour, kept).

## 4. HUD

`hud_draw(const struct hud_state *, colour code)` (`src/hud/hud.c`) draws `scrDrawHUD` (called from `oGame`'s Draw
GUI) and `showMessages` from a plain struct: life, bombs (sticky icon), ropes, money, `+collect`, the hold box and
held item, the item icons (udjat with blink, ankh, crown, kapala by blood level, spectacles, gloves, mitt, spring /
spike shoes, cape, jetpack, compass, parachute), the bow's arrows, the compass arrows (large, or small while a
message shows) and the two message lines (white with yellow parts). Positions are HD's GUI coordinates, which are
the 320 x 240 view without widescreen; the port shows view lines 8-231, so the HUD is drawn 8 lines up and
stretched by the 0x35 X zoom with the level. Art: HD's sprites (`tools/hdsprites.py`), and the fonts and money sign
from HD's datafiles (`tools/hudart.py` -> `build/gen/hudart.h`, `hud.bin`, `hud.json`: colour codes 2 (as is) and 3
(`c_yellow` blend)). `hud_text` / `hud_text_centered` are `drawText` / `drawTextHCentered` for other screens.

HD's in-play HUD draws no time: the level and total times are shown only by the transition screen
(`oTransition/Draw_64.gml:78-100`) and the pause stats (`drawStats`).

State the play code keeps (drawing has no side effects): `showMessages` counts `global.messageTimer` down once per
Draw GUI; `scrDrawHUD` sets `global.exitX / Y` to 640 / 544 in rOlmec; `hud_state.anim` is `oGame.image_index`
(spriteless: +1 a step), the frame of the sprites HD draws with image -1 (`sTeleporter`, `sFlare`).

## 5. Verification (2026-10-03)

| Check | Result |
|---|---|
| `scripts/hud_check.sh`: tests/hud in MAME, 8 HUD states (`tests/hud/cases.json`) against `tools/hudcheck.py`'s model (HD's PNGs, drawn as GameMaker does: origins, sprite fonts, colour blend), 384 x 224 after the crop and zoom | 0 of 86,016 pixels differ in each of the 8 |
| `scripts/hd_hudref.sh`: the same 8 states drawn by HD 1.2.2's own runner (its `scrDrawHUD` / `showMessages`, globals set by an added Draw GUI End event; `tools/hudref.py` around `tools/tracer.py`), model drawn over the runner's frame without GUI | 0 of 76,800 pixels (first 320 columns) differ in each of the 8 (runner GUI width 427: messages centred on 427 there, on 320 on the CPS3) |
| `tests/shell` host (`make host`) | 47 of 47: mapping, oGamepad step rule, taps, credits, EEPROM block, HD's first-boot quirk, `scrUpdateHighscores` cases |
| `scripts/shell_check.sh`: `shell_run` in MAME, inputs scripted by `scripts/lua/shellin.lua` and `shellmenu.lua` | 26 of 26 + the settings screen check: steps every 2nd frame, no start without credit, coin / service credits, panel choice, Start as pay, tap, held, release, EEPROM read back on a second run, free play and coins per credit stored and used |

Not checked: jtcps3 (the HUD uses only the sprite path already exact in tests/view); the cabinet by hand.

## 6. Open

- The compass's bottom arrows (view y 224-239) lose their lower 8 lines to the crop (screen ends at view line 231).
- Attract mode content (HD's intro / title / scores rooms without controls, or a demo) is the play runtime's.

## 7. Game capture: a cabinet game replayed on the host

A bug seen on the cabinet is reported as the game's capture, read off the screen and replayed step for step by the
host build (`tests/game/host.c`).

**Why the screen.** jtcps3 writes no `.nvm` to the SD card: Maldita C6 goal 4 (jtcps3 .rbf 2026-09-24), where the
OSD save wrote nothing, and jtcps3's own MRAs have none. The 93C46 holds only 128 bytes, used by the scores. A MiSTer
screenshot of jtcps3 is a lossless 384 x 224 PNG (`tools/jtshot.py` compares them in 5-bit colour). So the capture
is shown as pages of code cells and decoded from screenshots.

**What is recorded** (src/main `game.c`, `game.h` `struct capture`), for cabinet games only (routes record nothing):
- At `game_begin`: the seed given to `rng_seed`; the settings that change play (RUN BUTTON TOGGLE, INVINCIBLE; SMOOTH
  MOTION only for the record); the build's commit and whether its tree was dirty (`REV` / `DIRTY` from
  `scripts/release.sh`).
- Every step: `struct shell_input` `down`, the only control `game_step` reads (the game over panel's own press
  comes from the state, and the replay makes it again). It is stored as runs of 16 bits each: 11 key bits and up to
  32 steps.
- Every 64 steps: a 16-bit hash of the state. It covers the RNG's words, the player's x / y bits and `dead`, life,
  bombs, ropes, money, level, room, view and the instance count.
- After the last step: level, life, money, room, `dead`, and the room the game left for.

The capture takes 16,440 bytes of sprite RAM after area B (`tests/game/sprbss.ld` `.capture`). Main RAM grows by 12
bytes (.bss ends at 0x02076318: 39.7 KB of stack). Capacity:
- 7,168 runs and 1,024 checkpoints (65,536 steps, 36 minutes). The MAME test games used 7-9 steps a run, which is
  about 30 minutes of play.
- When the buffer is full, the header is marked truncated and later steps are not stored.

Nothing in play reads the capture: routes are record-equal and the playsh2 checksums are unchanged.
- At boot the capture is not cleared. It survives the settings screen's restart (checked in MAME) and probably a
  JTFRAME reset (not checked).
- The next `game_begin` replaces it.

**On the cabinet:**
1. After the game to report, do not start another game. A capture can also be taken during a game; the settings
   screen ends that game.
2. Hold Coin + B2 for a second (or press the test switch, F2). Choose GAME CAPTURE (5 presses of down; 6 in a DEV
   build), then press B1.
3. Each page shows "SPELUNKY GAME CAPTURE PAGE n OF m" at the top, and the build, steps and size at the bottom.
   - Pages change by themselves every 2.5 s. B1 or right goes to the next page, left goes back, and B2 returns to
     the menu.
   - About 960 bytes fit on a page: the 12,062-step test game took 4 pages.
4. Take screenshots of the pages, either way:
   - on the MiSTer keyboard, Win + PrtScr on each page;
   - over ssh while the pages cycle: `for i in $(seq 1 40); do echo screenshot > /dev/MiSTer_cmd; sleep 1; done`.
   The files are in `/media/fat/screenshots/<set name>/`. Extra shots and shots of other screens are skipped.
5. On the host:
   - `tools/capture.py decode <shots> -o cap.txt` writes the header lines and the controls in the route format.
   - `scripts/replay.sh cap.txt [recs|all [dir]]` builds the host from the capture's commit, or from the working
     tree when the build was dirty. It prints the end state and the checkpoints against the capture's, and writes
     the listed records' frames as PNG.

**Page format** (src/shell `capture_view`, `tools/capture.py`). Each page holds 966 bytes:
- page index, page count, the capture's size, a CRC-16, then 960 bytes of the capture;
- the bits go 7 to a text cell, in cells (1-46, 2-25);
- a cell is SS tile 128 + value, with 4 x 2 pixel blocks: white for a 1 bit, black for a 0 bit, and a red block 7
  that serves as the alignment mark.

The decoder tries grid offsets of up to 4 pixels and reads each block 1-2 pixels from its left edge, because jtcps3
shots have single pixels with their right neighbour's colour. If a page fails its CRC, the cell-wise majority of all
shots of that page is tried. The whole capture has a CRC-32.

**Check:** `scripts/capture_check.sh` runs the PLAY=1 build in MAME. `scripts/lua/capture.lua` plays random controls
and logs `game_probe` after each step: life, money, level, the player's x / y bits and the RNG hash. The script then
snapshots the pages twice, before and after the settings restart, decodes both, replays them on the host and
compares the host's per-step log.

| Run (2026-10-05) | Capture | Result |
|---|---|---|
| CAP_PLAY=1500, monkey 1 (working tree) | 979 steps, 109 runs, 1 page (308 bytes), dead | 979 of 979 steps equal; life 0 at step 853 in both; 15 of 15 checkpoints; end state equal; same capture after the restart |
| monkey 5, Start 401; build 0885bc53 (replayed from its commit) | 513 steps, 61 runs, dead | 513 of 513 equal; life 0 at step 448 in both; 8 of 8 checkpoints; end state equal; same capture after the restart |
| CAP_GOD=1 CAP_TOGGLE=1 (DEV build: INVINCIBLE, RUN BUTTON TOGGLE), CAP_PLAY=24000, monkey 3; build 6548ef5d | 12,062 steps, 1,387 runs, 4 pages (3,036 bytes), taken mid-game | 12,062 of 12,062 equal; 188 of 188 checkpoints; end state equal; same capture after the restart |

The same 12,062-step capture replayed without its RUN BUTTON TOGGLE flag differs from checkpoint 1 (step 128) on: the
checkpoints catch a replay that is not the cabinet's game.

**Not checked:** jtcps3. To check it, the lead runs these steps on the MiSTer:
1. Install a release built from this code (`scripts/release.sh`). v0.1.0 has no capture.
2. Play a game and die. Open GAME CAPTURE and take screenshots: `echo screenshot > /dev/MiSTer_cmd` once a second
   for 2-3 cycles of the pages.
3. Copy the screenshots back and run `tools/capture.py decode`. Expect "marks 1104/1104" and "CRC ok" on each page,
   and the capture's CRC-32.
4. Run `scripts/replay.sh`. Expect the checkpoints equal and "replay end capture ... -> equal".
5. Leave the settings screen and open GAME CAPTURE again. Expect the same capture.
6. Optionally, reset (F3) and look again.
