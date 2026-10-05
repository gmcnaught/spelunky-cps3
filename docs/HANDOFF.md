# Handoff

## 2026-10-05 14:05: v0.1.0 released; next session

v0.1.0 is on GitHub (tag build green, spelunky.zip + MRA) and on .81 (/media/fat/_Arcade/). Next, in order:

1. **Hardware bug capture.** The user's death on .81 is unexplained (no host repro in ~200 K runs). Add a way to get
   a cabinet session back to the host (input history from the start of the game, readable after the fact), so a
   .81 report replays in playhost. Then triage whatever the v0.1.0 playthrough turns up.
2. **Fresh untranslated survey on main.** docs/CONTENT.md's survey is from 2026-10-04 07:57, before the content
   packages; 85 PUNTR sites remain in src/game. Re-run the random-input survey (levels 1-16, 30 seeds) and fix the
   codes normal play reaches. Tunnel man (4005 / 4011 / 4020) stays untranslated (GAMELOOP section 3).
3. **p99 step spikes on jtcps3** (docs/PERF3.md "Open"): explosions, the boulder, temple traps over 0.84 M; dark
   levels at the top of the mean budget (+30-44 K). These show as slowdown in play.
4. **Done 2026-10-05 (docs/GAMELOOP.md section 5):** ~~Exact gates for the new rooms: showEndMessage in
   tools/tracer.py TRACE_GUI and tools/drawmodel.py; the ending's clouds and text in drawmodel; a route that beats
   Olmec.~~ game_check over_giant (with its enemies: `... 1 0 1`) and end_win (22 records) are 0 px against MAME and
   the runner; tests/routes/end_olmec.txt beats Olmec (scripts/olmec_host.sh). The traces g_over_giant_s253 and
   g_end_win_s7 were remade with the tracer's end block (a new SPT4 block: older decoders cannot read them), and
   build/gen/fronttables_rt.txt must be current (`make gen`) for the ending's layer order in drawmodel.
5. **Done 2026-10-05:** ~~compass arrows cropped (docs/ARCADE.md section 4, not in HD: 8 lines higher); ending text
   under fade rectangles hidden at half alpha~~ (faded HUD palettes, colour code 6).
6. **Done 2026-10-05 (branch devinv; docs/ARCADE.md section 2):** ~~Developer options in the service menu: INVINCIBLE (ON / OFF).~~ The settings screen is src/shell/shell.c
   (docs/ARCADE.md section 2); store the option in EEPROM word 27 next to the other settings bits. HD's
   `oPlayer1.invincible` (pint.h) is the post-hit blink timer, not a god mode: check which damage paths skip it
   (spikes, crush, lava, falls, explosions) and add one flag those paths test. Routes and gates keep it off.
   Decided (the user, 2026-10-05): testing only. An invincible game stores no high scores, and the option is
   compiled only into dev builds (e.g. tests/game DEV=1); scripts/release.sh builds without it, and a release
   build ignores the EEPROM bit if a dev build set it.
7. Housekeeping: about 40 local branches and the agent worktrees are merged or abandoned; prune them.

## 2026-10-05 12:20: game loop done on main a309f12

docs/GAMELOOP.md items 1-6 and darkness are merged and gated (section 5 there). The release set (scripts/release.sh)
is on MiSTer .81 for the user's playthrough. Next: the open items in docs/GAMELOOP.md section 5, of which the
user's is release-mode seeding (level generation must not be pinned).

## 2026-10-05: next is the game loop

Main is 2239a8e (smooth motion and VBlank pacing, docs/DRAW.md section 7). Next work: **docs/GAMELOOP.md** (death ->
scores -> rHighscores, the ending, the lake roll blocker after level 4). Of the 2026-10-04 items below:
- section 1 (ramcheck) is done on main (docs/DRAW.md section 6);
- perf3 is paused after batch 22 (docs/PERF3.md);
- the pk-lake branch (section 2) is **not merged**, and its lake roll is GAMELOOP item 1;
- section 3: darkness is not on main (`PLEV.darkness` is never set); the cimg fix was not checked.

MiSTer: .81 has the playable build (the user plays there).

# Handoff, 2026-10-04 16:45

Main is e264467. The grid build ships (PLAN §1). Gates:
- exact build: build/gates.sh and build/ctall.sh, record-equal to the runner traces;
- grid build: scripts/equiv_check.sh;
- SH-2: checksums equal to the host build;
- tests/ramcheck.ld: at least 32 KB of stack.

Process:
- **MAME:** only through scripts/mame.sh (headless docker).
- **MiSTer:** .62 (root@192.168.20.62), one agent at a time, handed out by the lead.
- **Gating:** speed work is gated in batches: a short exactness argument per commit, the full suite once per batch, bisect on failure.
- **Gate scope:** every merge check also links tests/game (the ramcheck) and runs playsh2.

## 1. Merge blocker: main fails the ramcheck link

On e264467, tests/game (built like game_check, HUD=1 -O2, PIN 1792) ends .bss at 0x0207a4c4, about 23 KB of stack, against the 32 KB check. The playsh2 JT variant (main RAM only) overflows RAM by 22 KB. The largest main-RAM items are inst_mem 129,024 B (generator and play instances, `PW.in = inst_mem`, hot, so it stays), pin_ext 89,600, ents 12,288 and pin_en 9,728.

draw1's fix is uncommitted in worktree `.claude/worktrees/agent-ac1fab3d8977bc40f` (branch worktree-agent-ac1fab3d8977bc40f, on e264467). Modified files: docs/DRAW.md, src/draw/draw.c, src/game/play.h, tests/game/sprbss.ld, tests/playsh2/Makefile, tests/playsh2/main.c, tests/ramcheck.ld; new file tests/playsh2/jtcold.ld. The plan:

1. **Done and gated by draw1:** tests/game/sprbss.ld puts cold arrays in sprite RAM area A:
   - the generator-only lists in inst.c: with_pool, gw_inext, gw_iprev, ghome, gw_ohead, gw_otail, gw_live;
   - the attract-only lpos_id, lpos and w_alive.

   .bss is then 454,072 (54.7 KB of stack); playsh2 6,851/6,851; gametime unchanged.
2. **To finish:** draw.c's claim arrays (cnext, ccell, ctile, bnext, bpos; 17.9 KB) go to DRAW_CACHE_SECTION. They are touched only for instances whose draw state changed. Expected stack: 72.6 KB, or about 54.6 KB with perf3's batch.
3. **playsh2 JT variant:** tests/playsh2/jtcold.ld moves generator-only arrays (the R-tree nodes rn 55.5 KB, gtiles 16 KB, inst.c lists; 89 KB in all) to sprite RAM, and main.c's ram_init clears them. That leaves 68 KB of stack. Generation timings then include sprite-RAM accesses.
4. **Lead's addition:** the grid build doesn't use the R-tree (rn) at play time. If it is only used by generation or by the exact build, compile it out of the grid build (PCOL_EXACT only) or place it in sprite RAM in every build. Check against src/game/pcolgrid.h's hooks first.

Gate it with:
- gates.sh, ctall and equiv_check;
- the tests/game link plus MAME frames (scripts/game_check.sh p4_exit559);
- playsh2 checksums;
- gametime.

## 2. Waiting on the ramcheck fix (committed on branches, gated on the host)

- **pk-lake** (branch worktree-agent-a9d8cd3f06c7d548a, on e264467):
  - 97504f7: the oTransition lake roll and probLake;
  - 1026aa3: playhost --room rLevel3 and its globals;
  - 347969f: oJaws, c_swamp_lakejaws and a tests/equiv_accept.txt line (blood made in a different collision order; blood is not COSMETIC because the kapala and the vampire use it);
  - 219e129: c_swamp_lake (the natural level-6 lake, seed 121013091).

  Both lake routes are record-equal on playhost and playhost_nc without the EXT workaround. Its full gates, ctall and equiv rerun were in progress when it was stopped.
- **perf3 batch 1** (branch worktree-agent-a69ec85fed9f74a42, on e264467): ca44b8b A (resting skip, grid only), 1b0b230 D (step claimant table), 742980e (A guard for EXT_SCRATCH), e3ef748 E (xprevious for moved instances), fe9c74e F (integer sqrt), 16f21ca (gver/gclock 16-bit), ab18e2d (rst 128 entries).
  - SH-2 grid mean step 260,119 -> 180,294 MAME clocks (-30.7 %), 6851/6851 checksums.
  - Per commit: A -47.0 K, D -13.2 K, E -9.9 K, F -9.7 K.
  - Main-RAM stores per step: 27,940 base -> 21,139 with A -> 19,188 with D.
  - RAM cost: about 18 KB, less after 16f21ca / ab18e2d.
  - The full host gates of the batch, and its ramcheck, were not finished.

## 3. Uncommitted or lost work

- **draw1, darkness translation:**
  - Covers oLevel Create :16-17 and Step :110-136 (PLEV.darkness), oPlayer1 :74-147 distToNearestLightSource (initial 999), and oFlare distToPlayer, with float-rounded distances.
  - draw1 reported 0 differences against the traced darkness / distLight on g_p7_dark_s18, c_items_flare_s69 and c_jungle_scarab_s615.
  - The edits are not in its worktree now. The edit scripts are copied to build/handoff/ (from the lead's scratchpad) (`ed_dark.py`, `ed_dark2.py`, `darkcmp.py`, `darktr.py`); they target the old worktree path. The gate output is in `gates_dark.txt`.
  - Still to do after it: the p7_dark MAME run against the runner's 6 shots, which needs tests/game's nodark option (now on main).
- **draw1, cimg model fix:** tools/drawmodel.py rebuilds an oItem's price-tag counter (cimg) from the record history. It matched the traced cimg on all 612 records of g_p5_shop_s96 and c_items_dice_s191, and p5_caveman went from 115 to 0 differing frames. The scripts `ed_cimg.py`, `ed_cimg2.py` and `cimgchk.py` are in build/handoff/. Not in the worktree.
- **draw1, queued:** cps3v_vblank busy-waits for the sprite-list DMA (../cps3-testgame/sdk/src/cps3v.c:226). Measure the wait with a real list on .62; if it's material, start the DMA and check its status at the next VBlank. Do it in our code, not the SDK.
- **jtmodel** (worktree agent-a52ae5f4b5e11b230, uncommitted): scripts/jtmodel.sh, scripts/lua/jtmodel.lua and tools/jtmodel.py are new, and it changed scripts/mame.sh and scripts/playsh2_jt.sh.
  - Its rebuilds of equiv's jtcps3 builds (899ce76 grid/exact, 01a2559 -Os) reproduced all 54 MAME step means.
  - Counting runs (3 cache models, misses split code / literal pool / rodata / RAM, per-symbol loads and stores) were in progress.
  - Its task: a per-function jtcps3 cost model fitted to docs/EQUIV.md's 54 points; a ranking of arrays by loads and stores per step to place hot data in cached main RAM and cold data in sprite RAM; and evaluation of the stack in the 2 KB cache-RAM mode, -Os, and code placement.

## 4. Facts the next agents need

- jtcps3 versus MAME (cps3-testgame ttest):
  - 32-bit store 6.06 clocks;
  - main-RAM load 1.5 on a cache hit, 7.5 on a miss;
  - back-to-back MUL.L 6.37;
  - SIMM 1 code past the cache 2.5x;
  - cache-RAM mode 1.5;
  - one 4 KB unified cache.

  The step costs 3.9x (exact) to 4.2x (grid) MAME clocks on jtcps3. jtcps3 budget: step ≤ about 0.525 M per 2-frame pair, which is about 146-154 K MAME. The grid at e264467 is about 260 K, and perf3's batch brings it to 180 K.
- Character DMA: art is loaded once at boot; it isn't a CPU lever. Sprite RAM is uncached, so use it only for cold data. Character RAM can't take CPU writes while the display is on.
- Frame checks: on routes the grid takes off the trace, use EXACT=1.
- Disk: the 2026-10-04 incident filled the host disk and broke the colima VM, fixed by restart and fstrim. Never write full dumps; aggregate or delete outputs.

Saved copies (ignored dir build/handoff/): draw1_ramfix.patch (draw1's uncommitted diff on e264467) and jtcold.ld; jtmodel.patch and jtmodel_new.tar (jtmodel's uncommitted diff and new files); the darkness and cimg edit scripts.
