# Game loop: one cabinet game from Start to the end

Status 2026-10-05 (updated), main a309f12: items 1-6 and darkness are done and gated; the release set is on MiSTer
.81 for a playthrough. See section 5.

Goal: a cabinet game runs HD's whole flow on one boot:
- levels 1-16 and Olmec through the transition rooms;
- death -> game-over panel -> scores stored -> rHighscores -> attract;
- win -> ending rooms -> credits -> scores stored -> rHighscores -> attract;
- a second game behaves as the first.

## 1. State on main (checked in the code, 2026-10-05)

| # | Step | HD | main | Effect on the cabinet |
|---|---|---|---|---|
| 1 | Transition before levels 5-8 | `oTransition/Step_0.gml:19-24`: lake roll `rand(1, global.probLake) == 1` -> `rLevel3`, else `rLevel` | `src/game/ptrans.c:229` `PUNTR(4013)`, **no room set** | **Blocker:** after level 4 the transition room can't be left (the only exit is that button press) unless the level rolled a black market |
| 2 | Death | `oPlayer1/Step_0.gml:1885-1886` `scrUpdateHighscores(0)` | `src/game/pplayer.c:1484-1491` sets `dead`, no call (no PUNTR either) | EEPROM scores, plays and deaths are never updated by play. `hs_update` (`src/shell/hiscore.c`) exists and is host-tested |
| 3 | Game-over panel | `oGame/Draw_64.gml:123` `showEndMessage`: GAME OVER, FINAL SCORE:, `$moneyCount`, PRESS <attack> FOR HIGH SCORES | `drawStatus` / `moneyCount` are counted (`src/game/pobj.c:1178-1186`, `pplayer.c:1241-1250`); nothing draws them | The dead player lies there with no panel |
| 4 | After the panel | `room_goto(rHighscores)` (`pplayer.c:1249`) | `prun.c:403-416` room_change returns it as unmodelled -> `game_step` returns 1 -> shell -> attract from **rIntro** (`front.c` `front_start`) | The scores screen is skipped |
| 5 | Win | Olmec -> oXEnd -> rEnd, rEnd2, rEnd3 (oEnd, oEnd2, oEnd2BG, oEnd3, oEndPlat, oEndWall) -> rCredits1 / 2 (oCredits1 / 2, `drawCredits`, `showFinalScore`) -> `scrUpdateHighscores(1)` (oCredits2) -> rHighscores | `pplayer.c:1757` `ptemple_player(2061)` untranslated; no end-room objects (about 470 GML lines in oEnd* / oCredits*) | Beating Olmec does nothing |
| 6 | Second game on one boot | `scrClearGlobals` | `game_begin` -> `scrClearGlobals` (`src/main/game.c`); PLAN P7: "a second game after game over: not covered by any route" | Unverified |
| 7 | Tunnel man in rTransition1x / 2x / 3x | `oTransition/Create_0.gml:57-71` (shortcut builder; EEPROM tunnel1 / 2) | `ptrans.c:48` `PUNTR(4005)`, `4011`, `4020` | Not created; stays so (section 3) |

Item 1 has a fix already: branch `worktree-agent-a9d8cd3f06c7d548a` (worktree directory gone, branch kept), 4 commits on e264467:
- 97504f7: the lake roll, and `G.probLake = 10` in `gen_new_game`;
- 1026aa3: playhost `--room rLevel3` / `--global`;
- 347969f: oJaws, route c_swamp_lakejaws;
- 219e129: route c_swamp_lake.

They were host-gated, but their full gates never finished (docs/HANDOFF.md section 2). Main has since raised EXT_MAX to 448 and made water terrain (docs/DRAW.md section 6), which those routes needed.

Also on the path, not blocking the loop:
- dark levels draw lit: `PLEV.darkness` is never set on main (docs/HANDOFF.md section 3; the edit scripts are in build/handoff/);
- the other `PUNTR` sites (`grep -rho "PUNTR([0-9]*)" src/game`). On the cabinet they record the code and play on.

## 2. Work order

1. **Lake roll (blocker).**
   - Rebase or cherry-pick the 4 lake commits onto main.
   - Run the full gates once: gates.sh, ctall, equiv_check, p5_regress, playsh2. Run game_check on c_swamp_lake with EXACT=1.
   - In MAME, run a PLAY-like route from level 4's exit through the transition into level 5.
2. **Death -> scores.**
   - Add a play hook at `pplayer.c:1490` (play.h callback, set by src/main/game.c) carrying HD's arguments. Its `hs_run` comes from PG / G: money, time, kills, damsels, tunnel1 / 2, used_shortcut, keep_score.
   - It calls `hs_update(&SH.hs, &SH.st, &run, HS_DEATH)`.
   - Routes keep the EEPROM untouched (game_cfg.route), as with the run toggle.
3. **Game-over panel.**
   - `showEndMessage` in src/draw (as `transition_out`: `hud_text_centered`, large / small fonts, yellow / white).
   - Wording: "PRESS ATTACK FOR HIGH SCORES" (section 3).
4. **To rHighscores.**
   - When `game_step` ends on `R_rHighscores`, start the attract cycle there: `front_start_at(R_rHighscores)`, then the cycle as now.
   - The new-record marks (`HS_NEW_*`, `global.newMoney` ...) must reach oHighscores' draw.
5. **Second game.**
   - tests/game with a second route, as tests/gametime already plays two games on one boot.
   - Game 2's record hashes (or snapshots) must equal a fresh-boot run of the same route.
6. **Ending (largest).**
   - Translate oXEnd (2061), the rEnd* rooms and objects, rCredits1 / 2, `showFinalScore`, `scrUpdateHighscores(1)`.
   - These are front-end style rooms (scripted, no controls but the skip), so src/front's room machinery (`front_room`) is the likely home.
   - Check the 510-entry display budget on the credits.
   - Done (merged a309f12): rOlmec's oXEnd -> rEnd -> rEnd2 -> rEnd3 -> rCredits2 -> scores stored ->
     rHighscores (src/front, src/game pplayer.c / pk_temple.c / prun.c). `scripts/end_host.sh`: 3183 / 3183 records
     equal to the runner's `g_end_win_s7` (tests/routes/end_win.txt, `# room rEnd`). At most 343 display entries
     (the credits' testers page). rCredits1 (the title's credits door) is not reached on the cabinet.

## 3. Decisions (the user, 2026-10-05)

- **Game-over panel wording:** the attack button keeps its name: "PRESS ATTACK FOR HIGH SCORES" (HD's text with
  the button named as the attack button, not as a cabinet button such as B1).
- **Tunnel man and shortcuts:** planned, not implemented now (item 7 stays `PUNTR`). A later option is shortcuts as
  a start-level choice, as Tetris and Tempest let the player start at a higher level, rather than HD's tunnel man.
- **rHighscores after a game:** shown without controls, as in attract; controls (the minigame doors) can be added
  later.

## 4. Verification

- **Reference traces** (`scripts/hd_trace.sh`):
  - extend a death route (p5_giant, p5_caveman or p5_shop end in a death) with two attack presses, so the trace reaches rHighscores;
  - `TRACE_GUI` records give the runner's panel frames for `tools/drawmodel.py cmp --gui`;
  - the ending: a trace started in the end room (`TRACE_ROOM`, as `rHighscores` is for p8_scores), or a recorded Olmec win.
- **Host:** playhost and `scripts/game_host.sh` on the new routes; front rooms by `scripts/front_host.sh`.
- **MAME:** `scripts/game_check.sh` snapshots (panel, rHighscores with the new score); EEPROM word 16-28 read back after the run (as `scripts/shell_check.sh` reads word 27).
- **Hardware:** the user's playthrough on MiSTer .81 (`tests/game PLAY=1`, deployed as in docs/ARCADE.md).

## 5. Progress (2026-10-05)

Done on main (a2d661f), gated as one batch: gates.sh (p5_regress 19/19 and 18/18, P4, gen, colprobe, snd), ctall
50/50, EQUIV 74/74, playsh2 6851/6851 checksums (mean route step 137.4 K MAME clocks).
- **1 lake roll:** the 4 pk-lake commits cherry-picked (9346665 .. 58a5323); c_swamp_lake 190/190, c_swamp_lakejaws
  295/295.
- **2-4 death -> scores -> rHighscores** (4d20eda): `scrUpdateHighscores` in src/game/pscript.c calls `play_hs_hook`
  (src/main/game.c `hs_store` -> `hs_update`; routes leave the EEPROM unless `game_cfg.scores`), then
  `global.time = floor(time / 1000)`. The panel is `end_out` in src/draw/draw.c. `game_end_room` makes
  `game_attract_step` start the cycle in rHighscores; `front_new` places oNew on the new records until the intro.
  Route `over_giant` is 336/336 records equal to the HD trace `g_over_giant_s253`.
- **5 second game** (dc7f6b0): draw.c's price-tag counters (icid) were carried into the next game. tests/game/host.c
  HOST_GAME2=1: game 2's frame hash equals game 1's at every record on 16 routes.
- **Cabinet idle timeout** (a2d661f, not in HD): 900 steps on the game-over panel and the cabinet presses attack
  itself, so an unattended cabinet returns to attract.

Host test modes (tests/game/host.c): HOST_SCORES, HOST_EE (EEPROM writes), HOST_AFTER=<steps> (the attract after the
game, v_a<k>.bin), HOST_GAME2=1, HOST_CABINET=1 (route keys as the cabinet's controls).

- **Darkness** (5e2d097, branch darkness): oLevel.darkness, oPlayer1.distToNearestLightSource, oFlare.distToPlayer;
  0 differences in darkness / distLight on g_p7_dark_s18, c_items_flare_s69, c_jungle_scarab_s615,
  c_ice_darkfall_s202. The light search runs only on a dark level or with a Kali altar (the only GML readers).
  Step cost, mean MAME clocks: p7_dark 138.1 -> 150.3 K, c_items_flare 132.6 -> 148.5 K, c_jungle_scarab
  119.5 -> 132.7 K (soft-float in instance_nearest / distance); +30 to +44 on lit levels. Within the 146-154 K
  budget but at its top: the first speed target if dark levels run slow on jtcps3.
- **6 ending** (merged with main in 20c1ab3, then ci-ending a309f12). Gates on that tree: gates.sh 19/19 + 19/19,
  ctall 50/50, EQUIV 74/74, playsh2 6851/6851 (mean route step 140.0 K), end_host 3183/3183.
- **Release** (a309f12, scripts/release.sh): spelunky.zip + "Spelunky Classic Arcade.mra" on .81
  (/media/fat/_Arcade/, moved from _CPS3Test 2026-10-05; the core is _Arcade/cores/jtcps3.rbf); boots to the intro
  and title on the real core (screenshots 2026-10-05 12:19, 12:59).

Open:
- **A route that beats Olmec (2026-10-05):** tests/routes/end_olmec.txt (seed 1, level 16, rOlmec, enemies kept;
  3029 steps), written by tools/olmecbot.c (scripts/olmecbot.sh, about 3 minutes; the same keys again from the
  committed tool): 20 Olmec cycles chosen by fork() lookahead dig him down to the lava (he drowns from step 2690),
  then a best-first search over key macros takes the player up a rope out of the pit to oXEnd. The runner's trace
  (`TRACE_HUD=1 TRACE_SND=1 TRACE_LEVEL=16 TRACE_ROOM=rOlmec TRACE_TAIL=400 XVFB_SCREEN=1280x960x24
  scripts/hd_trace.sh end_olmec 1 end_olmec_s1`, 420 MB) goes to rEnd at record 3028 as the C does.
  `scripts/olmec_host.sh` (the game program on the host, exact build): ROUTE equal over 3431 records, rooms
  rOlmec@0, rEnd@3028, 3429 / 3431 records equal: records 2064 and 2720 differ in one oBat's yVel by 1 ulp
  (0.2579919185662628 / 0.25799191856626275), and nothing after. playhost (build/host): 3027 / 3029, the same two.
  `EXACT=0` (the shipping collision grid): ROUTE equal (rEnd at 3028, same money and life), records from 385 differ
  as on c_temple_olmec. Both builds report untranslated 1020 when Olmec is destroyed: src/game pobj.c flags every
  oSolid child's Destroy, but oOlmec (oMovingSolid, oSolid) has none of its own, so oSolid's (destroy_solid) is the
  translation; the flag is spurious there. The player ends with life 1 of 4.
- **Ending frames (done 2026-10-05):** tools/drawmodel.py models rEnd3 / rCredits2: the two bgClouds layers (no
  room background), showFinalScore in oEnd3's Draw, drawCredits in oCredits2's Draw GUI, their rectangles, from the
  trace's end block; the front rooms' layer order needs build/gen/fronttables_rt.txt from a current `make gen`.
  Trace remade with the GUI frames (`TRACE_ROOM=rEnd TRACE_MONEY=12345
  TRACE_GLOBALS=kaliPunish=2,kills=7,damsels=2,time=754321 TRACE_HUD=1 TRACE_SND=1 XVFB_SCREEN=1280x960x24
  TRACE_SHOT=<recs> TRACE_GUI=<recs> scripts/hd_trace.sh end_win 7 g_end_win_s7`). `scripts/game_check.sh end_win 7
  g_end_win_s7 300,450,520,560,700,850,1150,1244,1246,1248,1250,1252,1256,1260,1278,1280,1300,1500,1700,2300,2600,3000
  1 12345`: 0 px at all 22 records against MAME, the runner's frames and its GUI frames; on the host
  (`HOST_ROOM=23 HOST_GLOBALS=... HUD=1 scripts/game_host.sh end_win 7 g_end_win_s7 1 12345 0`) 3175 / 3175 frames
  equal. scripts/end_host.sh: 3183 / 3183 records; sndcmp 25 differ (6 with the old trace): every one an xflame
  replay, whose count follows the runner's audio timing (the host's voices never end).
- **Ending text under the fade rectangles (done 2026-10-05):** the lines showFinalScore (:62) and drawCredits (:64)
  draw before their black rectangle use the HUD palettes src/draw fades with it: colour code 5 (DRAW_PAL_HUDDARK,
  white) and the new code 6 (DRAW_PAL_HUDDARK_YELLOW, c_yellow; tools/darkfade.py's third table at
  DARK_FADE_HUDY_AT, 128 KB of flash; one more 512-byte palette DMA when the alpha changes); src/hud
  hud_text_faded. The rectangles' alpha byte is draw_set_alpha's: fadeLevel as a single-precision float times 255
  (g_end_win_s7 record 1250: 0.7999999999999999 is drawn at 204), src/front alpha_byte. rEnd3's fade (records
  1243-1252) and the credits' fades are 0 px against the runner (game_check end_win below).
- **Title flare (fixed 2026-10-05):** the intro's flare stopped mid-shaft in rTitle (host boot trace g_p8_boot_s7
  differed from record 1050). The play collision table (gentables.c, from game.unx) kept HD's manual sTitle_HD box
  (columns 14..192) while tools/fronttables.py moves the logo right by titlelogo.DX = 40: the solid logo reached
  x 608 across the shaft (x 592..608). tools/hdgentables.py now gives sTitle_HD the port logo's box (0..127 x
  0..47, as sprites.c). scripts/front_host.sh links again (src/shell/hiscore.c, the snapshot's .inc files):
  1298/1298 records equal.
- **Push-block ledge grab (not in HD, 2026-10-05):** a push block moves 1 px per push, so one under a solid can show
  a sliver of its top and HD's (x +- 9, y - 9) test grabs it. pplayer.c pushblock_covered refuses the non-glove
  solid grab when a solid covers the push block's top (columns 1..14); ledge flips are unchanged. No death was
  reproduced from these hangs (hermetic harness, about 200 K runs); the user's death on .81 is unexplained.
- **MAME frame check of the panel (done 2026-10-05):** tools/tracer.py's TRACE_GUI runs showEndMessage after
  scrDrawHUD / showMessages, and TRACE_HUD records carry the end block (flags bit 5: oGame.drawStatus / moneyCount,
  oEnd3's and oCredits2's state, global.kills / damsels); tools/drawmodel.py draws the panel (`end_text`; the port's
  prompt PRESS ATTACK on the MAME screen, HD's PRESS X against the runner's GUI frames). The route needs its enemies
  (p5_giant's spider): `scripts/game_check.sh over_giant 253 g_over_giant_s253 200,260,300,330 1 0 1` is 0 px at
  all four records against MAME, the runner's frames and its GUI frames (trace remade: `TRACE_HUD=1
  TRACE_SHOT=200,260,300,330 TRACE_GUI=200,260,300,330 XVFB_SCREEN=1280x960x24 scripts/hd_trace.sh over_giant 253
  g_over_giant_s253`); `HUD=1 scripts/game_host.sh over_giant 253 g_over_giant_s253 1 0 1`: 334 / 334 frames equal.
  Without the enemies (the default 0) the player does not die and every record differs.
- **Release-mode seeding (done 2026-10-05):** PLAY builds seed a game and each attract intro from `shell_seed()`, the
  input history mixed every frame (docs/ARCADE.md, seeding rule). Checked in MAME on the release set: Coin at frame
  200, Start at 400 twice gives the same RNG state and level 1; Start at 401 and 460 give two other levels.
