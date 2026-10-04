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

Not mapped: `checkLangPressed` (language, VARIANTS X3), the run toggle (X10; `global.toggleRunEnabled` stays false,
so `checkRun` is the held button), the F-keys (`oDebug`, restart F6: X8), the key / pad configuration (X4).
`global.downToRun` stays HD's default (true, `scrInit:50`).

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
A step that overruns its two frames delays the next one: no step is skipped.

Game hooks (weak no-ops in `shell.c`, for the play runtime to define): `game_boot`, `game_attract_step`,
`game_begin`, `game_step(const struct shell_input *)`, `game_draw`.

The test switch (MAME: Service Mode; jtcps3: F2) opens the settings screen: FREE PLAY, COINS PER CREDIT, CLEAR HIGH
SCORES, SAVE AND EXIT (up / down choose, B1 or right change, left back, test or B1 on SAVE AND EXIT leave). Leaving
stores the settings and restarts the program.

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
| 27 | settings: bit 0 free play, bits 8-11 coins per credit |
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
