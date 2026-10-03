# Spelunky Classic variants: HD 1.2.2 and SD compared with 1.1

## 1. Sources and method

| Source | Version | Path |
|---|---|---|
| Spelunky Classic 1.1 (GM8) | `spelunky_1_1_src.zip`, extracted | `build/gml/<object>/<event>.gml`, `build/gml/_scripts/`, rooms in `build/gmk/Rooms/` |
| Spelunky Classic HD (yancharkin, GameMaker 2.3+) | tag `1.2.2` = commit `e88247ab93a280134aadd8f95ef934080233e311` (2025-09-03). This is the port's base | `refs/hd/src/` |
| HD git history | full clone up to `b4a39995` (2026-08-25). The 12 commits after 1.2.2 change only translations, `.gitignore`, the highscore-room text drawing, `drawCredits` and `loadLocalizedSprites` (Observed: `git diff --stat 1.2.2 HEAD`) | scratchpad clone |
| Spelunky SD | v1.190, YYC build, so there is no code. Only `changelog.txt` is used | `refs/sd_linux/assets/changelog.txt` |

**Lineage (Observed: HD commit `2ae1fa53`).** HD's first commit is "Unmodified version from 'Play and Create with GameMaker' Humble Bundle". That is the 2015 GameMaker: Studio port of 1.1 by "DFK" (from the `//DFK` comments). HD then converted it to GMS 2.3. Each HD change below is tagged with its origin: **Humble** (already in `2ae1fa53`), **HD `<commit>`**, or **GMS semantics** (the same source text is evaluated differently by the GMS runtime).

**Method.**

1. Every 1.1 event file is mapped to its HD name: Create=`Create_0`, Step=`Step_0`, Begin Step=`Step_1`, End Step=`Step_2`, Alarm N=`Alarm_N`, Draw=`Draw_0`, Destroy=`Destroy_0`, Animation end=`Other_7`, Outside Room=`Other_0`, Room End=`Other_5`, Game End=`Other_3`, Collision with X=`Collision_X`, Key K pressed=`KeyPress_<keycode>`, Mouse events=`Mouse_N`.
2. Both sides are normalised. The diff tool removes comments, whitespace, `;` and braces, and lower-cases the text. It converts `and`/`or`/`not` and `true`/`false`, maps `__view_get(e__VW.X,0)` to `view_x[0]` and `instance_create_depth/layer` to `instance_create`, and drops `function f(...)` wrappers. 1.1 drag-and-drop actions (`action_inherited`, `action_kill_object`, `VARIABLE`, `execute_script`) become code. Tool: `scratchpad/vdiff/diff.py`.
3. Result: 1,095 object events or scripts exist on both sides. 867 are identical after normalisation and 228 differ. 45 exist only in 1.1 and 252 only in HD. Every differing file was read. Rooms were compared by instance list, size and creation order (`scratchpad/vdiff/rooms.py`).
4. The GM8 evaluation rules come from OpenGMK (`build/ext/OpenGMK`), the reference runner. `&&` / `||` / `^^` all have precedence 0 and are evaluated left to right (`gml-parser/src/ast.rs:776-778`). Both operands are always evaluated: `BinaryOperator::call(lhs, rhs)` evaluates both (`gm8emulator/src/gml/runtime.rs:834-837`), and the kernel's own comment says "gm8 doesn't short circuit" (`gm8emulator/src/gml/kernel.rs:5651`).

Line references: `build/gml/...` is 1.1 and `refs/hd/src/...` is HD 1.2.2. Unmarked statements are Observed in code. "Inferred" marks a conclusion drawn from code that was not run.

## 2. HD features to remove for the arcade build

Two things to know first.

- **No HD-only code calls the RNG on the gameplay path** (Observed: a grep for `random(`, `irandom`, `random_range`, `choose(` and `rand(` in HD-only scripts and objects). The one hit is Scribble `__scribble_gen_7_build_pages.gml:38` (`choose(-1,1)`). It runs only when Scribble text-animation randomisation is enabled. Inferred: plain messages do not run it, but if Scribble text is drawn during play with that option on, it consumes the global RNG. Removing Scribble removes that consumer.
- **Removing widescreen restores gameplay numbers.** HD sets the view width `w = round(240 * aspect)`, clamped to 320..560 (`refs/hd/src/objects/oScreen/Create_0.gml:24-31`), and 57 GML files read the view width. Among them are the bat wake check (`oBat/Step_0.gml:1`), the ghost spawn x (`oGame/Step.gml:42` in 1.1), the `oExplosion` solid bounds, and the `oLevel` activation region. HD also sets the camera's horizontal follow border to `w/2` for every play room (`oScreen/Create_0.gml:43-48`), which is 160 at 4:3, where 1.1 uses 128 (`build/gmk/Rooms/rLevel.xml:102`, `hBorder="128"`). It does the same for Olmec (`oOlmec/Alarm_5.gml:1`, `global.display_w/2`, against 1.1's `128`). HD's vertical border for `rOlmec` is 96 (`oScreen/Create_0.gml:48`) against 64 in 1.1's room. The replacement is the fixed 320x240 view with 1.1's borders, 128 / 96 (and 128 / 64 in `rOlmec`).

| # | Area | HD files (all under `refs/hd/src/`) | Replacement on the arcade build | RNG order |
|---|---|---|---|---|
| R1 | Widescreen and aspect | `objects/oScreen/Create_0.gml` (aspect, `room_set_view` for 30 rooms, `global.room_offset`, `global.display_w/h`, window size); `oScreen/Step_1.gml:3-5,44,96,183` (surfaces at display size); `room_offset` / `display_w` in `oTitle`, `oTransition`, `oHighscores`, `oIntro`, `oIntroBG`, `oCredits1/2`, `oMoon`, `oSunRoom`, `oMoonRoom`, `oStarsRoom`, `oDesert*Scroll`, `oPalmTreeScroll`, `oShrubScroll`, `oOlmec/Step_0.gml:52`, `oOlmec/Alarm_5.gml:1`, `oLevel/Draw_0.gml:6,17,23`; widened rooms: `rTitle` 640 to 768 px, `rCredits1` 320 to 464, `rCredits2` 320 to 430, `rScreenInit` 960x720, and padding blocks outside x 0..320 in `rTransition1..4`, `rTransition1x..3x`, `rEnd`, `rEnd2`, `rEnd3`, `rEndCustom`, `rHighscores`, `rSun`, `rMoon`, `rStars`; `oTitleBG_HD`, `oTitleLogo_HD` (title art in `rTitle`) | Fixed 320x240 view; 1.1 borders 128/96 (128/64 Olmec); 1.1 room sizes and instance lists (`build/gmk/Rooms/`); 1.1's title (`oTitle` at x=96, `oBricks` at 128,32) | None: room padding instances have no Create-event RNG (`oHardBlock`, `oBrick`, `oLush`, `oDark`, `oTemple` are static solids). Changing `w` changes `view_wview` in gameplay checks (bat wake, ghost spawn x, explosion bounds), not RNG order |
| R2 | Touch input | scripts `enableTouchInput`, `drawTouchControls`, `resizeTouchButtons`, `changeTouchOffset`, `menuTouchControls`; `global.touch*` and `vkeySize*` in `scripts/scrInit/scrInit.gml`; touch-visibility timer in `objects/oGame/Step_0.gml:22-33`; `enableTouchInput()` in `oGame/Create_0.gml`, `oTransition/Create_0.gml`; `global.touchControlsVisibility` branches in `oShopkeeper/Step_0.gml` (price prompt) | Arcade buttons | None |
| R3 | Localisation and Scribble | 117 `scripts/__scribble*` and `scribble*` scripts; 9 shaders `shaders/__shd_scribble*`; `fonts/fnt_7_12`, `fnt_7_12_large`, `scribble_fallback_font`; `datafiles/locale/`, `datafiles/fonts/`; `localization/`; scripts `tr`, `trMessages`, `showMessages`, `drawMessage`, `drawHighlightedMessage`, `drawText`, `drawTextUni`, `drawTextRtl`, `drawTextHCentered`, `drawTextHCenteredConf`, `draw_text_scribble*`, `string_*_scribble*`, `string_has_digits`, `string_remove_redundant_spaces`, `setLocale`, `getLocales`, `changeLocale`, `changeLocale2`, `menuLanguage`, `checkLangPressed`, `loadLocalizedSprites` (`global.s*New` sprites in `oBigCollect`, `oSmallCollect`, `oYellHelp`, `oNew`, `oDamsel/Draw_0`, `oItem/Draw_0`); `type = tr("None")` in `oEnemy`, `oItem`, `oSolid`, `oTreasure`, `oRubblePiece` Create; message arrays (`buyMessage = [..]`, `message1/message2`, `messageHighlights`) in every shop-item Create, `scrShopItemsGen.gml`, `scrStealItem.gml`, `scrShopkeeperAnger.gml`, `scrGetFavorMsg.gml`, `oShopkeeper/Step_0.gml`, `oMsgSign` (tutorial text rebuilt per sign position, `Collision_oCharacter.gml`, 168 lines) | English strings and 1.1's message system: `global.message` / `global.message2` / `global.messageTimer`, drawn by `build/gml/oLevel/Draw.gml:1-13` with the 8-px sprite fonts `sFont` / `sFontSmall` (`build/gml/_scripts/scrInit.gml:3-4`); 1.1 sign text (`build/gml/oMsgSign/`) | None in normal use (see the Scribble note above). `scrGetFavorMsg` keeps its item-gift branches; only the message variable changes |
| R4 | Keyboard and gamepad config screens | rooms `rKeyConfig`, `rJoyConfig`; objects `oKeyConfig`, `oKeyConfigButton`, `oJoyConfig`, `oJoyConfigButton`, `oJoyConfigClearBtn`, `oJoyConfigSkipBtn`, `oGamepad` (HD rewrite: `Step_0.gml` 302-line diff, `Other_75` async gamepad events); scripts `findGamepads`, `gamepadButtonCheck`, `gamepadInMenu`, `getJoyBtnOrAxisId`, `scrGetKey`, `scrGetJoy`, `keysSave`, `joySave`, `configLoad` (key and pad defaults); the `check*` input scripts (`checkJump`, `checkAttackPressed`, ...) now OR the keyboard and pad | One input module reading the CPS3 inputs: 8-way stick and buttons for jump, whip, item, rope, bomb, run (or the 1.1 "down to run" option), pay/purchase; start | None |
| R5 | Save files | `datafiles/spelunky.ini`; `ini_*` high scores in `objects/oGlobals/Create_0.gml` (tunnel-man progress `value8`/`value9`), `oTitle/Create_0.gml` (title shortcuts), `oHighscores`; scripts `highscore_add2`, `configSave`, `configLoad`, `json2dsmap`, `dsmap2json`, `json2struct`, `getWorkingDirPath`; `settings.json`, `keys.json`, `gamepad.json`; working-directory paths in `scrLoadLevel`, `scrTestLevel` | EEPROM (PLAN §3) for the high scores (money, time, kills, saves), the tunnel-man progress (`global.tunnel1/2`), and the "first time" flag | None |
| R6 | Window, fullscreen, OS options | `menuToggleFullscreen`, `game_end2`, `oNAL` (HTML5 browser resize; not referenced by any room or code, Observed by grep), `window_set_size` / `window_set_fullscreen` in `oScreen/Create_0.gml`, `options/` targets (android, ios, html5, operagx, tvos, mac, linux, windows), `options/extensions` (Electron, HTML5) | None needed. Arcade boots straight into the attract/title | None |
| R7 | Mobile and platform code | `getPlatform` (`global.mobileBuild`, `html5Build`, `html5Mobile`), `global.mobileBuild` branches in `oGame/Create_0.gml`, `oGame/Step_0.gml` menu, `characterStepEvent.gml:84-90` (Android run key) | None | None |
| R8 | Debug and developer tools | `oDebug` (F1-F5: level skip, money 200000 and health 99, item bundles; `localization/README_CHEATS.md`); `toggleCheats` (F8, `oGame/Step_0.gml:36`); instant restart (`oGame/Step_0.gml:38-43`, `global.keyRestartVal = vk_f6` in `configLoad.gml:20`); `oScreenFirst`, `oScreenLast` (surface debug, key `8`; not referenced by rooms or code); `oSprite` (unused); `oPressStart`, `oCaveLeft`, `oCaveRight` (no events or not referenced); `oPlayer1/KeyPress_114.gml` (comment only) | Nothing, or a service-mode test menu if wanted | None |
| R9 | HD pause menu | `oGame/Step_0.gml:154-395` (language switch, pause, menu: music toggle, key/pad config, toggle-run, touch, fullscreen, language, die, restart, quit); `oGame/Draw_64.gml`; scripts `menuDie`, `onOffMusic`, `toggleMusic`, `drawStats`, `showEndMessage`, `showFinalScore`, `drawCredits`, `draw_surface_pSurf` | 1.1 pause in `oScreen` Begin Step (`build/gml/oScreen/Begin Step.gml`), without the "rope key while paused = `game_end()`" (`:32`); the arcade has no quit. Keep "die" only if wanted | The pause uses `instance_deactivate_all`, which does not call the RNG |
| R10 | Toggle-run option | `checkRun.gml:2-9`, `checkRunPressed`, `oPlayer1/Step_0.gml:2093-2094`, `global.toggleRunEnabled` | 1.1 run button (plus 1.1's `global.downToRun` if kept) | None |
| R11 | GMS compatibility shims (no counterpart in a C port) | `__view_get`, `__view_set`, `__view_set_internal`, `__init_view`, `__background_*`, `__init_background`, `__init_global`, `__init_action`, `__global_object_depths`, `object_get_depth`, `action_inherited`, `action_kill_object`, `action_color`, `action_linear_step`, `action_move_to`, `action_path`, `action_reverse_xdir`, `instance_create`, `tile_add`, `tile_add2`, `tile_delete`, `tile_layer_find`, `room_set_view`, `sound_play`, `sound_stop_all`, `sound_volume`, `sound_global_volume`, `setSoundVol`, `initMusic`, `draw_background_stretched`, `gmitf`, `macros` | Native code per PLAN §1 | None |
| R12 | Splash and title extras | HD logo and contributor credits (`oTitleLogo_HD`, `drawCredits`); `global.firstLaunch` / `titleStart` logic in `oTitle/Create_0.gml:3-9`; `global.firstTime = false` at `oTitle/Create_0.gml:4` ("DFK adding this to override tutorial") | 1.1 title flow, attract mode, coin/start (PLAN P8). Whether to keep the forced first-play tutorial is open (§5) | None |

The 1.1 features that HD keeps but an arcade board does not need are the level editor and custom levels (`oLevelEditor`, `oLoadLevel`, `rLevelEditor`, `rLoadLevel`, `scrLoadLevel`, `scrTestLevel`, `levels/*.lvl`). They are not HD-specific. Removing them does not touch RNG order.

## 3. What HD changed from 1.1 (gameplay)

Categories: **bug fix**, **balance**, **feature**, **generator-RNG** (changes RNG call order or count during level generation, or the seed-to-level mapping), **semantics** (the same text evaluated differently by GMS). Message, localisation, sound-API, view-API and other mechanical changes are left out (§1).

| ID | Area | 1.1 -> HD | Refs (1.1 / HD) | Origin | Category | Arcade relevance |
|---|---|---|---|---|---|---|
| HD1 | Level generator: start room | Start room x = `rand(0,3)` -> fixed at column 0 ("DFK start room X is hard coded to 0 because I'm lazy"). One fewer `rand` call per level, and every non-market level starts in the top-left room | `_scripts/scrLevelGen.gml:25` / `scripts/scrLevelGen/scrLevelGen.gml:26-28` | Humble | generator-RNG, balance | Yes. It is the largest layout difference; restoring `rand(0,3)` is one line |
| HD2 | All `and` / `or` with side effects | GM8 evaluates both operands; GMS 2.3 short-circuits (Inferred: GameMaker 2.3+ always short-circuits; HD's options have no switch). Every `cond and rand(..)==1` skips the `rand` call when `cond` is false. 66 single-line occurrences in 16 1.1 files; in the generator: `scrLevelGen.gml:71,148,215-217,222,234`, `scrInitLevel.gml:148,169,181-182,189-190,232-236`, `scrRoomGen.gml:87,333,343,344` (per-tile, so it runs for every tile of every room), `scrRoomGen2..5`, `scrRoomGenMarket`, `scrRoomGenYeti`, `scrEntityGen` (8), `scrTreasureGen` (3), `scrSetupWalls.gml:378,417,466` | same lines both sides | GMS semantics | generator-RNG | Yes. HD's level for a seed cannot equal 1.1's. The port must pick one rule and document it |
| HD3 | Operator precedence | GM8: `a or b and c` = `(a or b) and c`. GMS: `a or (b and c)`. A scan for unparenthesised mixed `and`/`or` in 1.1 finds 3 sites: `scrEntityGen.gml:452-453` (temple-area enemy spawn: `startRoomX != .. or startRoomY != .. and not enemy-at`, so HD skips the "no enemy already here" test outside the start column) and `oItem/Step.gml:219,237` (sticky bombs, see HD17) | `_scripts/scrEntityGen.gml:452` / `scripts/scrEntityGen/scrEntityGen.gml:453` | GMS semantics | generator-RNG (enemy count and RNG calls in the temple) | Yes. Choose 1.1's grouping |
| HD4 | RNG algorithm and seeding | GM8 RNG, seeded at start-up -> GMS RNG; HD calls `randomize()` at intro (Inferred: the GMS PRNG differs from GM8's) | - / `objects/oIntro/Create_0.gml:6` | HD | generator-RNG | Yes. Decides what "same seed, same level" means |
| HD5 | Room creation order | `rLevel`, `rLevel2`, `rLevel3`, `rOlmec`: 1.1 creates `oPlayer1, oGame, oLevel`; HD creates `oPlayer1, oLevel, oGame` (commit "Fix lake not generated properly #29"). `oGame` Create runs `scrInitLevel` (generation), so in HD `oLevel` Create (`scrHoldItem(global.pickupItem)`, the Kali ball and chain, `startMusic`) now runs **before** generation | `build/gmk/Rooms/rLevel.xml` / `rooms/rLevel/rLevel.yy` (`instanceCreationOrder`) | HD `c7fb3754` | generator-RNG (Unknown: depends on whether the carried item's Create calls `rand`; e.g. `oDice` Create has `value = rand(1,6)`) | Yes. The lake bug it fixes is an HD-port regression (Inferred). 1.1's order is the reference |
| HD6 | Webs near giant spider | `distance_to_object(oGiantSpider < 100)` (measures the distance to object index 0 or 1) -> `distance_to_object(oGiantSpider) < 100`. Same `rand` count; changes the web chance from 1/60 to 1/5 or 1/10 near the spider | `_scripts/scrTreasureGen.gml:53,99` / `scripts/scrTreasureGen/scrTreasureGen.gml:55,102` | HD `de95c065` | bug fix, generator (output, not call count) | Yes, as an option: it is the intended rule |
| HD7 | Shopkeeper names | `rand(1,32)` -> `round(random_range(1,32))`: one RNG call either way; names 1 and 32 at half weight | `_scripts/scrGetName.gml:24` / `scripts/scrGetName/scrGetName.gml:25` | HD | balance (cosmetic) | No |
| HD8 | Rubble offsets | `rand(0-8)` (one argument, = `rand(-8,0)`) -> `rand(0,-8)`; offsets 0..+8 -> +1..+7; same call count | `oDark/Destroy.gml:4-6`, `oAlienShip/Destroy.gml`, `oAlienShipFloor/Destroy.gml` / `objects/oDark/Destroy_0.gml:4-6` etc. | Humble | bug fix (cosmetic) | No |
| HD9 | Instance deactivation | `oLevel` Step deactivates everything outside view ±96 every step -> the line is commented out: instances outside the view keep running (enemies, traps, items) | `oLevel/Step.gml:75` / `objects/oLevel/Step_0.gml:75` | Humble | balance (large behaviour change) | Yes. 1.1 behaviour (PLAN §1 relies on it) |
| HD10 | Explosions off-screen | `isLevel("rTutorial") or <in view±16>`: `isLevel` takes no argument and is true in every play room, so 1.1 explosions destroy blocks anywhere -> `isRoom("rTutorial") or <in view±16>`: blocks, tile covers and loose spikes are kept when the explosion is more than 16 px outside the view | `oExplosion/Collision with oSolid.gml:1` / `objects/oExplosion/Collision_oSolid.gml:1` | HD `de95c065` | bug fix (literal), balance | Yes. Keep 1.1 (off-screen bombs still dig) |
| HD11 | Olmec slam | `oOlmecSlam` destroys any `oSolid` child it touches -> only `oBlock`, `oPushBlock`, `oTemple` (new collision events; the `oSolid` event is commented out) | `oOlmecSlam/Collision with oSolid.gml` / `objects/oOlmecSlam/Collision_oSolid.gml:1-10`, `Collision_oBlock.gml`, `Collision_oPushBlock.gml`, `Collision_oTemple.gml` | Humble | balance | Yes, check `rOlmec` contents (§5) |
| HD12 | `rOlmec` lava | 160 extra `oLavaSolid` at y 880..928, at and below the 880-px room bottom; `oGame` moved to x=96 and last in creation order | `build/gmk/Rooms/rOlmec.xml` / `rooms/rOlmec/rOlmec.yy` | HD `66d5abc5` | Unknown purpose | Unknown |
| HD13 | Final-boss exit | `oFinalBoss` (no sprite or mask) does `instance_place(640,544,oEntrance)` (Inferred: returns `noone`, so 1.1 leaves the right-hand entrance and adds `oXEnd` on top) -> `with (oEntrance) instance_destroy()`, which also removes the start door at 16,544; `olmecDead` / `doorOpen` become globals | `oFinalBoss/Step.gml:5-6`, `oOlmec/Step.gml:39` / `objects/oFinalBoss/Step_0.gml:4` | HD `66d5abc5`, `fa42b51c` | bug fix | Yes, as an option |
| HD14 | Grabbing a ladder from the air | `y = ladder.y + 14` -> `y = ladder.y + 14 - (sprite_yoffset - 1)`; origin is y=8 in both (`sStandLeft`), so HD places the player 7 px higher ("Fix the rope crushing you glitch") | `_scripts/characterStepEvent.gml:623` / `scripts/characterStepEvent/characterStepEvent.gml:637` | HD `c86823e0` | bug fix | Yes, as an option |
| HD15 | Shotgun and pistol muzzle | Shot and blast spawn at x-9 / x+8 -> x-12 / x+12, so the player no longer shoots themselves ("self-hurting firearms") | `_scripts/scrUseItem.gml:247-262,560-578` / `scripts/scrUseItem/scrUseItem.gml:248-263,559-577` | HD `be810ab3`, `d2a8333e`, `de86aa91` | bug fix | Yes, as an option |
| HD16 | Web cannon muzzle | x±12 -> x±18 | `_scripts/scrUseItem.gml:363-384` / `scripts/scrUseItem/scrUseItem.gml:364-385` | HD `d779798c` | bug fix | Yes, as an option |
| HD17 | Sticky bombs | 1.1 (GM8 grouping): stick when `(fast) and touching enemy and enemyID == 0`; damsel branch the same. HD: rewritten with braces to undo HD3 for this site, but drops `enemyID == 0` (re-targets on every fast step) and the speed test for damsels | `oItem/Step.gml:219-249` / `objects/oItem/Step_0.gml:219-232` | HD `9ca3fa34` | bug fix (for the GMS regression), balance | Partly. Keep 1.1's condition with GM8 grouping |
| HD18 | UFO hit by thrown item | `instance_destroy()` after the UFO kill -> commented out ("crash when destroying UFO"). Inferred: the UFO dies from the `oExplosion` created in the same branch | `oItem/Step.gml:339` / `objects/oItem/Step_0.gml:316` | HD `d9a9de02` | bug fix (crash) | No (1.1 behaviour is fine without the GMS crash) |
| HD19 | Enemy Destroy cleanup | `oEnemy` Destroy clears `bombID.enemyID` and, if held, the player's `holdItem` and `pickupItem` -> commented out ("rare crash"). HD keeps a stale sticky-bomb target and a stale held item | `oEnemy/Destroy.gml:1-9` / `objects/oEnemy/Destroy_0.gml` | HD `1aedba48` | bug (introduced) | No: keep 1.1 |
| HD20 | Sceptre (psychic wave) | Homes on the nearest `oEnemy` -> the nearest of `oEnemy` / `oDamsel`; it no longer self-destroys on a damsel hit | `oPsychicWaveP/Step.gml:10`, `Collision with oDamsel.gml:14` / `objects/oPsychicWaveP/Step_0.gml:7-22`, `Collision_oDamsel.gml` | HD `700329f6` | feature | Yes, as an option |
| HD21 | Player and push block | New collision: if `point_distance(x,y,block+8) < 11 and y >= block.y` then `x = xprevious` | - / `objects/oPlayer1/Collision_oPushBlock.gml:1-4` | Humble | bug fix (Inferred: stops the player overlapping a pushed block) | Option. It changes collision |
| HD22 | Tunnel man | Attack key at talk 3..5 jumps to talk 6; the transition waits for talk 6 instead of talk < 3 | `oTunnelMan/Step.gml:1`, `oTransition/Step.gml:8` / `objects/oTunnelMan/Step_0.gml:1-5`, `oTransition/Step_0.gml:9` | HD `340deb13` | bug fix | Yes (shortcut flow) |
| HD23 | Title room | `room_restart()` removed after `titleStart = 2` ("player appearing at the wrong door"); the title builds extra brick walls when shortcuts are locked | `oTitle/Step.gml:13,21` / `objects/oTitle/Step_0.gml`, `Create_0.gml:66` onward | HD `efb5de27` | bug fix | Yes, the door fix |
| HD24 | Forced tutorial | 1.1 sends a first-time player to the tutorial -> `global.firstTime = false` | - / `objects/oTitle/Create_0.gml:3-4` | Humble | feature | Arcade decision (§5) |
| HD25 | Ghost warning music | `scrMusicFade` in `oLevel` Step -> commented out | `oLevel/Step.gml:1-5` / `objects/oLevel/Step_0.gml:1-5` | Humble | audio | Yes (restore) |
| HD26 | Olmec stats | `oOlmec` gains `event_inherited()` (oMovingSolid -> oSolid Create: `invincible=false`, `type="None"`, then its own `invincible=true`) and `type = "Olmec"`, so death-by-Olmec counts in `global.enemyDeaths[22]` | `oOlmec/Create.gml` / `objects/oOlmec/Create_0.gml:1-2` | HD `66d5abc5` | bug fix (stats) | Low |
| HD27 | Quit while paused | 1.1: rope key while paused calls `game_end()` -> removed (HD menu instead, R9) | `oScreen/Begin Step.gml:32` / - | HD | feature | Remove (R9) |

Counts (27 rows; some carry two tags): generator-RNG 5 (HD1-HD5; HD6 also changes generator output without changing the call count), bug fix 13, balance 6, feature 3, other 3 (HD12 unknown purpose, HD19 a bug HD introduced, HD25 audio).

## 4. SD changelog: single-player gameplay entries

The changelog has 240 entries. 105 are single-player gameplay entries, listed below in 103 rows (row SD73 covers three gem-ghosting entries). The other 135 are left out: networking, chat, commands (`/spawn`, `/replace`, ...), multiplayer-only (carrying or throwing players, per-player HUD, PVP, jack[s] mode, colours), UI and settings (volume keys, controls UI, Alt+F4, scale, captures, links), and the splash screen. SD moved to GameMaker: Studio in 1.093 and is YYC-compiled, so HD2 / HD3 / HD4 also apply to SD (Inferred). "HD has it" is checked against §3: HD's gameplay diff from 1.1 is complete for objects, scripts and rooms, so any fix not in §3 is absent from HD.

| ID | Ver | Entry | 1.1 location | Category | Arcade relevance | HD has it |
|---|---|---|---|---|---|---|
| SD1 | 1.190 | Screenshake and other camera effects are now slightly more correct | `oLevel/Step.gml:9-28` | bug fix | Maybe | No |
| SD2 | 1.111 | Fixed damsel exit sprite missing a start frame | sprite `sDamselExit` | bug fix (visual) | Yes, cheap | No |
| SD3 | 1.106 | Fixed a bug where you could die from trying to hang onto a ladder inside a solid block | `_scripts/characterStepEvent.gml:615-630` | bug fix | Yes | Partial (HD14 is the same snap code, Inferred) |
| SD4 | 1.106 | Enemies can no longer be crushed by blocks or suffocate while held by player | not located (enemy crush test vs `held`) | bug fix | Yes | No |
| SD5 | 1.106 | Shopkeepers can no longer rage about a pushable block generated in the shop | `_scripts/scrShopkeeperAnger.gml` callers | bug fix | Yes | No |
| SD6 | 1.106 | Arrow trap arrows now face the correct direction in their first frame | `oArrowTrap*` arrow creation | bug fix (visual) | Low | No |
| SD7 | 1.097 | Fixed crown/hedjet not working correctly for illuminating the dark levels | `oLevel/Step.gml:116`, `oScreen/Begin Step.gml:178` | bug fix | Yes | No |
| SD8 | 1.097 | Fixed a rare crash with `global.roomPath` on level generation | `_scripts/scrLevelGen.gml:148-178` (snake pit writes `roomPath[i,j+3]`; Inferred) | bug fix | Yes (crash) | No |
| SD9 | 1.097 | Fixed a rare bug where every solid would have doors in it | not located | bug fix | Unknown | No |
| SD10 | 1.093 | Fixed a glitch where cycling items (with item key) could cause items to disappear | `_scripts/scrHoldItem.gml`, item key in `oPlayer1/Step.gml` | bug fix | Yes | No |
| SD11 | 1.063 | Fixed camera switching position for a moment at the end of the Olmec fight | `oOlmec/Alarm 5.gml:1-4` | bug fix | Yes | No |
| SD12 | 1.057 | Items now properly knock back players standing on the ground | `oItem/Step.gml` (player hit) | balance | Maybe | No |
| SD13 | 1.057 | Fixed overgrown trees occasionally blocking the full room height | `oTree/Step.gml`, tree generation in `_scripts/scrEntityGen.gml` | bug fix | Yes; RNG impact Unknown | No |
| SD14 | 1.055 | Underlying logic in getting hit by an item is now fairer | `oItem/Step.gml` | balance | Maybe | No |
| SD15 | 1.047 | Compass now displays all possible directions (below the exit) | `_scripts/scrDrawHUD.gml:46` | bug fix | Yes | No |
| SD16 | 1.047 | Flare crates can now also be broken by some objects | `oFlareCrate` | feature | Maybe | No |
| SD17 | 1.042 | Chests and crates can now be broken by some objects | `oChest`, `oCrate` | feature | Maybe | No |
| SD18 | 1.037 | Fixed reset button closing challenge room doors that it should not | challenge rooms `rSun` / `rMoon` / `rStars` reset sign | bug fix | Yes | No |
| SD19 | 1.037 | Fixed players inside Moai being killable with an explosion | `oPlayer1/Step.gml:1855-1857` | bug fix | Yes | No |
| SD20 | 1.024 | Spring traps can no longer catapult objects into the walls | `oSpringTrap/Collision with oItem.gml:10`, `Collision with oEnemy.gml:9` | bug fix | Yes | No |
| SD21 | 1.019 | Fixed shop signs not being considered part of shops | `_scripts/isInShop.gml` | bug fix | Yes | No |
| SD22 | 1.019 | Fixed crash upon death to ghost in ice caves | `oGhost` | bug fix (crash) | Yes | No |
| SD23 | 1.015 | Fixed whips lagging behind the player by one frame | `oPlayer1/Step.gml:441,474` | bug fix | Yes | No |
| SD24 | 1.007 | Fixed Moon challenge exit door taking you to the caves instead of the menu | `oMoonRoom`, `rMoon` | bug fix | Yes | No |
| SD25 | 1.007 | Player no longer can run out of blood before dying | `_scripts/scrCreateBlood.gml` | bug fix | Low | No |
| SD26 | 1.007 | Fixed Moon challenge showing "finished" screen too early | `oMoonRoom` | bug fix | Yes | No |
| SD27 | 1.000 | Fixed block "cover" tiles being destroyed or not destroyed in wrong situations | `oExplosion/Collision with oSolid.gml:7-12` | bug fix (visual) | Low | No |
| SD28 | 1.000 | Transitions can be skipped faster with an extra attack key tap | `oTransition/Step.gml:1` | feature | Maybe | No |
| SD29 | 0.990 | Fixed player not dropping blood when sacrificed while having a kapala | `oPlayer1/Collision with oBlood.gml:1`, altar code | bug fix | Low | No |
| SD30 | 0.990 | Fixed regular walls mixing up on edges of city of gold | `_scripts/scrSetupWalls.gml` (calls `rand`) | bug fix; generator-RNG possible | Yes, after checking RNG | No |
| SD31 | 0.990 | More fixes to cases of skyrocketing out of water | `_scripts/characterStepEvent.gml` water code, `isCollisionWaterTop` | bug fix | Yes | No |
| SD32 | 0.990 | Fixed a rare crash related to frozen cavemen getting crushed | `oCaveman` | bug fix (crash) | Yes | No |
| SD33 | 0.990 | Fixed treasure friction to be the same as for items | `oTreasure/Step.gml:39` (`xVel *= 0.3`) | balance | Maybe | No |
| SD34 | 0.990 | Fixed bats "hanging" mid-air after killing players sometimes | `oBat/Step.gml` | bug fix | Yes | No |
| SD35 | 0.970 | Fixed player depth being incorrect while exiting levels | `oPlayer1` exit | bug fix (visual) | Low | No |
| SD36 | 0.970 | Creatures from broken jars don't get killed by whips/bullets instantly | `oJar` | bug fix | Yes | No |
| SD37 | 0.970 | Fixed dice physics being strange if pitcher's mitt is equipped | `oDice/Step.gml`, `global.hasMitt` throws | bug fix | Yes | No |
| SD38 | 0.970 | Fixed cavemen continuously bouncing on ground after death | `oCaveman/Step.gml:140-153` | bug fix | Yes | No |
| SD39 | 0.950 | Jumping upwards around the corners of blocks is now a little easier | `_scripts/characterStepEvent.gml` | balance | Maybe | No |
| SD40 | 0.950 | Fixed ability to get unlimited blood out of enemies | `_scripts/scrCreateBlood.gml`, kapala | bug fix (exploit) | Yes | No |
| SD41 | 0.950 | Fixed capes not being shown on level transitions | `oTransition` draw | bug fix (visual) | Low | No |
| SD42 | 0.950 | Fixed multiple enemies glitching out if stuck on a corner of a block | enemy Step movement | bug fix | Yes | No |
| SD43 | 0.950 | Fixed monkeys propelling at surprising speeds downwards | `oMonkey/Step.gml:10-11` | bug fix | Yes | No |
| SD44 | 0.950 | Fixed monkeys killing themselves with thrown items more often than they should | `oMonkey`, `oItem/Step.gml` | bug fix | Yes | No |
| SD45 | 0.924 | Fixed parachute rapidly descending after once landed | `oPlayer1/Step.gml:257` | bug fix | Yes | No |
| SD46 | 0.924 | Spring shoes now permit slightly longer falls without taking damage | `oPlayer1/Step.gml:267` (`fallTimer > 16`) | balance | Maybe | No |
| SD47 | 0.924 | Explosions can now push treasures around | `oExplosion/Collision with oSolid.gml:17-20` (`oTreasure.state=1`) | feature | Maybe | No |
| SD48 | 0.924 | Fixed gold bar bounding boxes a bit | gold-bar sprite masks | bug fix | Low | No |
| SD49 | 0.924 | Treasures now generally have at least some kind of physics | `oTreasure/Step.gml` | feature | Maybe | No |
| SD50 | 0.924 | Multiple mobs no longer move upwards through blocks when stuck | enemy Step movement | bug fix | Yes | No |
| SD51 | 0.924 | Flares now burn webs | `oFlare`, `oWeb` | feature | Maybe | No |
| SD52 | 0.910 | Fixed ghost movements not being centered | `oGhost/Step.gml:18` | bug fix | Yes | No |
| SD53 | 0.910 | Made ghost a little bit slower | `oGhost/Step.gml:18` | balance | Maybe | No |
| SD54 | 0.900 | Fixed caveman and hawkman "alert" sounds repeating a lot | `oCaveman`, `oHawkman` Step | bug fix (audio) | Yes | No |
| SD55 | 0.900 | Slightly lowered rock "safety time" when thrown | `oItem/Create.gml:13` (`safe`), rock throw | balance | Maybe | No |
| SD56 | 0.900 | Olmec ending no longer creates a block under exit | `oFinalBoss/Step.gml:8-17` | balance | Maybe | No (HD13 changes the same event, not this) |
| SD57 | 0.900 | Fixed held items not showing up on game ending | `oEnd*` | bug fix (visual) | Low | No |
| SD58 | 0.830 | Improved after-edge jumping with capes | `_scripts/characterStepEvent.gml` | balance | Maybe | No |
| SD59 | 0.830 | Fixed Olmec ending failing if falling while touching left level wall | `oFinalBoss/Step.gml`, `oOlmec/Step.gml` | bug fix | Yes | No |
| SD60 | 0.820 | Enemies now flash briefly at the end of stun time | enemy Draw | feature | Maybe | No |
| SD61 | 0.820 | Enemies now don't lag behind the player while being carried | held-enemy position update | bug fix | Yes | No |
| SD62 | 0.820 | Altars are now slightly less picky on conditions for enemy sacrifices | `oSacAltarLeft` / `oSacAltarRight` | balance | Maybe | No |
| SD63 | 0.800 | You can now jump in one-block-high hallways | `_scripts/characterStepEvent.gml` | feature | Maybe | No |
| SD64 | 0.800 | Some enemies (most notably yetis) are now slightly easier to stomp | `oEnemy/Collision with oCharacter.gml` | balance | Maybe | No |
| SD65 | 0.800 | Fixed Olmec level finishing by itself | `oFinalBoss/Step.gml`, `oOlmec/Step.gml:39` | bug fix | Yes | No |
| SD66 | 0.800 | Fixed jumping out of shallow water | `_scripts/characterStepEvent.gml` | bug fix | Yes | No |
| SD67 | 0.800 | Fixed dead damsels making sounds | `oDamsel` | bug fix | Yes | No |
| SD68 | 0.800 | Items may no longer hit damsels multiple times instead of one | `oItem/Step.gml` damsel hit | bug fix | Yes | No |
| SD69 | 0.800 | Items may no longer hit enemies multiple times instead of one | `oItem/Step.gml` enemy hit (around 250-340) | bug fix | Yes | No |
| SD70 | 0.770 | Partially rewrote item placement on map generation; small tweaks to random generation (item displacement) | `_scripts/scrTreasureGen.gml`, `_scripts/scrEntityGen.gml` | generator-RNG | No (changes every seed) | No |
| SD71 | 0.770 | Damsels should no longer be able to die to tiki traps on level start | `_scripts/scrEntityGen.gml:237-248` (tiki torch and spear traps) | bug fix | Yes | No |
| SD72 | 0.770 | Udjat eye no longer blinks when you're in front of the market door | `oGame/Step.gml:2-11` | bug fix | Yes | No |
| SD73 | 0.758, 0.735, 0.730 | Gem ghosting fixes (corner cases; oddities; gems can no longer be ghosted while inside blocks) | `oGem` / treasure Step | bug fix | Yes | No |
| SD74 | 0.758 | Fixed bomb arrows not consuming bombs | `_scripts/scrFireBow.gml:56-58`, `_scripts/scrUseItem.gml:34` (`//global.bombs -= 1`) | bug fix | Yes | No |
| SD75 | 0.758 | Shopkeepers can no longer throw you into a block, causing suffocation | `oShopkeeper/Step.gml:381,398` | bug fix | Yes | No |
| SD76 | 0.758 | You can now hold jump key in webs instead of tapping | `_scripts/characterStepEvent.gml:294` | feature | Maybe | No |
| SD77 | 0.758 | Crown no longer prevents ghost from appearing | `oGame/Step.gml:26,39` | balance | Maybe (1.1 rule is deliberate code) | No |
| SD78 | 0.754 | Fixed a bug with sticky bombs misbehaving if there's a damsel on the level | `oItem/Step.gml:237` | bug fix | Yes | Partial (HD17) |
| SD79 | 0.754 | Fixed item duplication if switching items after a usable item was obtained | `_scripts/scrUseItem.gml`, `_scripts/scrHoldItem.gml` | bug fix (exploit) | Yes | No |
| SD80 | 0.754 | Fixed certain items disappearing when bought | `oShopkeeper/Step.gml` purchase | bug fix | Yes | No |
| SD81 | 0.754 | Fake bones can now be jumped upon | bone objects | feature | Maybe | No |
| SD82 | 0.750 | You can now carry around duplicate usable items | `_scripts/scrStealItem.gml` | feature | Maybe | No |
| SD83 | 0.750 | Fixed some items not displaying on transition screens | `oTransition` | bug fix (visual) | Low | No |
| SD84 | 0.750 | Jetpack now recharges when hanging on a wall or climbing ropes | `oPlayer1/Step.gml:216` | balance | Maybe | No |
| SD85 | 0.750 | Jetpacks are now lighter and can be carried through doors | `oJetpack/Create.gml` (`heavy`) | balance | Maybe | No |
| SD86 | 0.740 | Fixed bombs sticking to things held in hands by players | `oItem/Step.gml:219` | bug fix | Yes | No (HD17 drops the `enemyID` guard but has no held test) |
| SD87 | 0.740 | Made pushable blocks slightly different from normal ones | not located | Unknown | Unknown | No |
| SD88 | 0.740 | Gold idols and crystal skulls now also trigger when the pedestal is destroyed | `oGoldIdol/Step.gml:28` | feature | Maybe | No |
| SD89 | 0.736 | Fixed mattock kicking gems upwards while breaking blocks | `oMattockHit/Animation end.gml:48` | bug fix | Yes | No |
| SD90 | 0.735 | Improved shopkeeper and giant spider AIs | `oShopkeeper/Step.gml`, `oGiantSpider/Step.gml` | balance | Maybe | No |
| SD91 | 0.735 | Whipping mechanic improvements | `oWhip` | balance | Maybe | No |
| SD92 | 0.735 | Some game objects now get pushed by moveable blocks | `_scripts/moveTo.gml` push | feature | Maybe | No |
| SD93 | 0.730 | Fixed vampires pursuing dead or invisible players | `oVampire/Step.gml` | bug fix | Yes | No |
| SD94 | 0.730 | You can now whip flying arrows under certain conditions | `oWhip`, `oArrow` | feature | Maybe | No |
| SD95 | 0.723 | You can now switch between walk and run mid-air | `_scripts/characterStepEvent.gml:84-87` | feature | Maybe | No |
| SD96 | 0.720 | Fixed player animations for cases of being eaten by a plant | `oManTrap` | bug fix (visual) | Yes | No |
| SD97 | 0.720 | Arrow traps can now be triggered by ropes thrown down | `oArrowTrap*` | feature | Maybe | No |
| SD98 | 0.710 | Further tweaked off-ground jumping | `_scripts/characterStepEvent.gml` | balance | Maybe | No |
| SD99 | 0.710 | Fixed arrows lying one pixel above ground level sometimes | `oArrow` | bug fix | Yes | No |
| SD100 | 0.709 | Most environment sounds now fade when outside the view | `playSound` callers | feature (audio) | Maybe | No |
| SD101 | 0.709 | Fixed some enemies harming player even after death | `oEnemy/Collision with oCharacter.gml` | bug fix | Yes | No |
| SD102 | 0.705 | Jumping out of one-block-high corridors is now a little easier | `_scripts/characterStepEvent.gml` | balance | Maybe | No |
| SD103 | 0.900 | Olmec is now slightly smarter when it comes to picking who to smash | `oOlmec/Step.gml` | balance (written for multiplayer) | Low | No |

Category counts (103 rows): bug fix 65 (8 visual, 3 crashes including SD8, 2 exploits, 1 audio), balance 19, feature 17, generator-RNG 1 (SD70; SD13 and SD30 may also be), unknown 1 (SD87). The 1.1 locations are file-level pointers taken from greps; "not located" means no matching 1.1 code was identified.

## 5. Overlaps (fixed in both HD and SD)

| HD | SD | Notes |
|---|---|---|
| HD17 sticky bombs | SD78 (0.754, damsel), SD86 (0.740, held things) | Same code (`oItem/Step.gml:219-249`). HD's fix undoes the GMS precedence change and drops `enemyID == 0`; SD's text describes damsel-specific and held-item cases. Partial overlap |
| HD14 ladder snap | SD3 (1.106, dying when hanging onto a ladder inside a solid) | Inferred overlap: both come from `characterStepEvent.gml:615-630`, where a mid-air ladder grab moves the player to `ladder.y+14` |
| HD13 final-boss exit | SD56 (0.900), SD59 (0.830), SD65 (0.800) | Same event (`oFinalBoss/Step.gml`), different fixes. No overlap in behaviour |

## 6. Unknowns

- **Generator rules: decided (PLAN.md §1, 2026-10-03).** The port follows HD's runtime: short-circuit `and` / `or` (as C), GameMaker 2.3 precedence, the GMS random generator, HD1 / HD3 / HD5 as HD has them. The reference is HD 1.2.2's own Linux runner with `random_set_seed` injected (P1), not GM8 rules applied to HD's text.
- **GMS evaluation order of function arguments** (e.g. the four `rand` calls in `instance_create(x+rand(..)-rand(..), y+rand(..)-rand(..), ..)`) relative to GM8: not checked. If it differs, the RNG order differs inside every such call.
- **The `and`/`or` scan** (HD2, HD3) found single-line conditions only. Conditions spread over lines outside parentheses, and side effects other than `rand` (e.g. `instance_create` in a condition), were not searched.
- **HD12** (`rOlmec` lava below the room): purpose not determined. **HD11**: which `oSolid` types 1.1's Olmec slam can actually reach in `rOlmec` was not listed.
- **HD5:** whether any item carried into a level has a Create event that calls `rand` (it would then shift every level's RNG in HD). Only `oDice` was seen with one.
- **HD18:** UFO death through the explosion is Inferred, not run.
- **Arcade decisions:** the forced first-play tutorial (HD24), keeping "die" in pause (R9), and keeping the level editor or custom levels.
- **SD locations:** most are file-level; SD4, SD9 and SD87 were not located. SD is closed (YYC), so its exact rules cannot be read.
