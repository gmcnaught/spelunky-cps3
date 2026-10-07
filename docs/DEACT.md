# Off-view deactivation (branch deact2, 2026-10-06)

User decision (2026-10-06): instances outside the view plus a margin are deactivated to cut step cost, and the HD
reference build applies the same rule, so the exact gates compare like with like. A gameplay deviation from stock
HD 1.2.2 is accepted.

## 1. The rule

**Where.** One pass, at oGamepad's Begin Step (HD: tools/tracer.py `TRACE_DEACT=<margin>`, deact_gml, inline in the
Begin Step: a new global script beside TRACE_SND's gml_GlobalScript_trcSnd hung the runner; port: play_step right after
the Begin Step block, `deact_pass`). oGamepad's Begin Step runs before
every other instance's Begin Step except oScreen's, and no other gameplay object has a Begin Step, so the pass sits
after the animation and before the alarms, Step, collision and End Step events of the same frame.

**When.** Every step of rLevel, rLevel2 and rLevel3, except the room's first step (the Begin Step that writes the
phase-0 record: the view is not placed yet). Not in rOlmec, the transition rooms or the front end.

**Region.** The view the last draw left (camera_get_view_x / _y / _width / _height of view_camera[0]; port: PW.xview,
PW.yview, 320 x 240), grown by the margin M on every side: x0 = vx - M, x1 = vx + 320 + M, y0 = vy - M,
y1 = vy + 240 + M. An instance is outside when `x < x0 || x > x1 || y < y0 || y > y1` on its (x, y), not its box.

**Margin: M = 32.** Every candidate's sprite has its origin at (0, 0) and reaches at most 32 px right and down
(refs/hd/src/sprites; oJaws, 64 px, is exempt), so a deactivated instance is never visible. 32 is also at least
every view margin the candidates' own Steps use (oItem / oTreasure 16, oEnemy and its children 20 / 4, oRubblePiece
32): whatever HD runs for an instance inside its own view test still runs. 64 would only add instances that HD
already treats as off-view.

**Candidates.** Instances of oEnemy, oItem, oTreasure and their descendants (83 objects), except:
- objects: oShopkeeper, oShopkeeper2 (shop logic, the chase), oBomb (the fuse), oRopeThrow, oFlare, oFireFrogArmed,
  oFireFrogBomb (timers), oDamsel (exit / kiss), oDice (the dice shop counts them), oLampItem, oLampRedItem (light
  in dark levels), oJaws (64 px sprite);
- state: `held` true (the player's held item, a carried damsel or enemy), `forSale` true (the shopkeeper's
  `with oItem`).
Never candidates: oPlayer1 and the other oCharacter, the controllers (oGame, oLevel, oScreen, oGamepad, ...),
terrain and every other oSolid / oPlatform / oWater / oLadder object (so collisions of the active movers with the
level are unchanged), ropes, oGhost, oOlmec, boulders, explosions, rubble, blood and other detritus, bubbles.
No velocity test: oItem / oTreasure / oEnemy freeze themselves outside their own view test already (an item thrown
out of view stops mid-air in stock HD), so a deactivated mover keeps exactly the state HD would leave it in.

**The pass.**
1. Activation: every instance on the pass's list (the ones it deactivated) whose (x, y), read through its id, is
   inside the region is activated (`instance_activate_object(id)`), in list order; the rest stay listed.
2. Deactivation: every active candidate outside the region, in `with (all)` order, is deactivated
   (`instance_deactivate_object(id)`) and appended to the list.
The list is emptied at each room's first Begin Step.

**HD's legacy activation calls** (oLevel Step: `instance_activate_region` of the view + 96, `instance_activate_object`
of oCharacter, ropes, ...; oGame Step: `instance_activate_region` around draining water) are no-ops in stock 1.2.2,
which deactivates nothing during play (the matching `instance_deactivate_region` is commented out in oLevel). The
patch turns each call into `max(...)` (same arguments, no side effect), so the pass is the only activation in a level. The pause's
`instance_deactivate_all` / `instance_activate_all` stay (not on any route; the port does not model them).

## 2. What GameMaker does with a deactivated instance (Observed, HD runner, build/trace/dz_probe_cave, p5_caveman
seed 863, TRACE_DEACT=32 with TRACE_EVLOG / TRACE_TREE probes)

- No Step, alarm or animation: alarms and image_index are frozen (oRubyBig alarm[0] 19 at deactivation, 18 one step
  after activation; oSnake image_index 0.8 kept for 22 steps).
- Not in `with (all)` / `with (obj)` (the records leave it out), `instance_exists(id)` is false, collision functions
  do not find it.
- Its variables can still be read through its id (`id.x` gave 280, no error), so a reference held by an active
  instance does not stop the runner.
- Activation takes effect at once: the instance's alarms and Step run in the same frame.
- **Activation puts the instance last**, as a new creation: last in its object's Step order (oGoldBar Step order
  110976, 110977, 110991, 111003, 111004, 110998 after 110998's activation), first in `with (all)` order (the record
  order, newest first). Deactivating and activating changes the order the instance is dispatched in.
- **The collision tree**: the entry of a deactivated instance stays in the tree (searches pass over it) and is put in
  again at activation (the search order changes: 111003 111005 111004 after activation where it was 111003 111004
  111005). Checked with TRACE_TREE (oTreasure, oItem) at 13 records each (build/trace/dz_tree_*): equal with this
  model; taking the entry out at deactivation, or leaving it in place at activation, gives a different order. Marked
  dirty + put in (instance_create's), put in only, or marked dirty only: the same order on these probes (the port
  uses instance_create's).

## 3. The port

- `deact_pass` in play_step (prun.c), compiled when PLAY_DEACT (play.h) is not 0. One option in both: TRACE_DEACT on
  the HD side, PLAY_DEACT on the port side, both 32 by default (section 6).
- Deactivate (pworld.c `pw_deactivate`): the structural half of pin_kill without removing the slot: `alive = 0`
  (every event loop, query, `with`, count and the recorder skip it, as in GM), unlinked from the object lists
  (ounlink: olive counts, nearest caches, the grid, the animation list), out of pw_ord, its collision entry kept in
  the tree (pcol_deactivated: off the dirty / test lists and the object count; the grid build takes it out of the
  grid). The slot, pin_ext and pin_en stay. The list holds at most 320 ids (DL_MAX, 640 B; more: untranslated 9010);
  the candidate objects are a bit table (58 B).
- Activate (`pw_activate`): the structural half of pin_add on the kept record: a new creation number (pw_seq =
  PW.seq++, appended to pw_ord, the object lists and the animation list, so it is the newest), `alive = 1`, put in
  the collision tree as instance_create does (pcol_activated), marked for the drawing. A renumbering of the creation
  numbers (as pw_release's) runs first when PW.seq is near the limit.
- References kept across steps (PL.holdItem, trapID, enemyID, bombID) are not cleared: the slot is not released.
- The piranha batch hook (piranha4, `pswamp_piranha_run(order + k, n - k)` after `play_cur_obj = PX(i).obj;`): the
  pass runs before the Step snapshot, and a deactivated piranha is unlinked, so it is not in `order` and a batch run
  never spans it. No hook needed in the Step loop.

## 4. Verification plan

1. Exact build (playhost, PCOL_EXACT) with PLAY_DEACT=32 against TRACE_DEACT=32 traces of a few routes
   (dz_<route> names, build/trace untouched otherwise): record-equal (playcmp RESULT n/n, ROUTE equal).
2. The collision tree after activations: PCOL_TREE probes against TRACE_TREE (oTreasure, oItem).
3. Grid build: tools/equivcheck.py route check and state gate on the same traces.
4. Cost: MAME SOFTFP and jtcost on c_swamp_drain, c_swamp_swim, p5_lush_l5s11 / l6s23, p5_caveman.
5. The full retrace (build/retrace_cmds.sh with TRACE_DEACT=32) and all gates: a lead decision.

## 5. Results (branch deact2, 2026-10-06)

First on main 5e7974c, then rebased onto 1f1815a (swamp3, totem, spikes, sprdma, piranha4 merged): the exactness and
grid results below are the same on both bases; the costs are given for both (5.1 on 1f1815a).

**Exactness.** 15 routes regenerated with TRACE_DEACT=32 as build/trace/dz_<route>_s<seed> (build/retrace_cmds.sh's
commands; existing traces untouched). The exact build with PLAY_DEACT=32 (build/host_dz/playhost) is record-equal
on all 15: p5_caveman 291/291, p5_snakes 244/244, p5_spider 302/302, p5_shop 317/317, p5_buy 345/345, p5_l4 251/251,
p5_giant 201/201 (the trace continues into rHighscores, as without deact), p5_lush_l5s11 637/637, l6s23 631/631,
l5s37 641/641, c_swamp_drain 371/371, c_swamp_swim 419/419, c_swamp_piranha 413/413, c_jungle_mantrap 405/405,
c_jungle_firefrog 413/413. Sound calls (tools/sndcmp.py) 0 differ on drain, swim, mantrap. The default build
(PLAY_DEACT 0) stays record-equal to the existing traces (caveman, drain, l5s11 checked).

**Grid build** (playhost_grid vs playhost_eq, both PLAY_DEACT=32, tools/equivcheck.py): route and state PASS on 13.
p5_lush_l5s37: the accepted gameplay difference at gbag 419 (tests/equiv_accept.txt, unchanged). c_jungle_firefrog:
route PASS, state FAIL at gbag 115: which of two adjacent oSpikes (110853 / 110854) takes sSpikesBlood (collision
order: the grid's newest first against the tree's order after an activation's re-insert), then a rubble position
at 235; RNG equal throughout. Without deact the route passes both. Needs an accept line after the retrace:
`c_jungle_firefrog_s296 115 spike blood order (collision order after an activation's re-insert); RNG equal`.

**MAME SOFTFP, 5e7974c** (playsh2 ROUTES="c_swamp_drain c_swamp_swim p5_lush_l5s11 p5_lush_l6s23 p5_caveman", checksums
2349 / 2349 equal), route mean step:

| Route | PLAY_DEACT 0 | PLAY_DEACT 32 | change |
|---|---|---|---|
| c_swamp_drain | 167,537 | 151,023 | -9.9 % |
| c_swamp_swim | 131,150 | 115,510 | -11.9 % |
| p5_lush_l5s11 | 119,957 | 101,629 | -15.3 % |
| p5_lush_l6s23 | 111,455 | 97,395 | -12.6 % |
| p5_caveman | 99,638 | 96,815 | -2.8 % |
| all 2344 steps | 124,664 | 110,168 | -11.6 % |

**jtcost, 5e7974c** (fit constants; one call over the five routes, steps by record count as jtcost.sh numbers them):

| Step | instructions | model jtcps3 | change |
|---|---|---|---|
| c_swamp_drain 61 (walking) | 124,985 -> 96,434 | 537,445 -> 405,971 | -24.5 % |
| c_swamp_drain 161 (swimming) | 213,970 -> 190,553 | 878,286 -> 741,525 | -15.6 % |
| c_swamp_swim 571 (route step 200) | 177,944 -> 152,019 | 726,201 -> 592,205 | -18.5 % |
| c_swamp_swim 721 (route step 350) | 137,779 -> 100,288 | 593,262 -> 406,476 | -31.5 % |
| p5_lush_l5s11 991 (step 201) | 121,726 -> 104,278 | 478,074 -> 401,403 | -16.0 % |
| p5_lush_l6s23 1628 (step 201) | 130,699 -> 109,177 | 516,224 -> 425,960 | -17.5 % |
| p5_caveman 2196 (step 138) | 108,789 -> 101,771 | 425,119 -> 393,478 | -7.4 % |

The jtcps3 run on .62 was not made (the lead hands out the device).

### 5.1 Costs on 1f1815a (the speed branches merged)

The merged branches already cut the off-view enemies' cost (swamp3's off-view pre-check, piranha4's idle piranha
path), so deactivation saves less than on 5e7974c.

MAME SOFTFP (same routes, checksums 2349 / 2349 equal), route mean step with step 1:

| Route | PLAY_DEACT 0 | PLAY_DEACT 32 | change |
|---|---|---|---|
| c_swamp_drain | 147,776 | 138,944 | -6.0 % |
| c_swamp_swim | 114,598 | 108,690 | -5.2 % |
| p5_lush_l5s11 | 113,155 | 102,190 | -9.7 % |
| p5_lush_l6s23 | 100,826 | 93,084 | -7.7 % |
| p5_caveman | 98,375 | 96,276 | -2.1 % |
| all 2344 steps | 113,735 | 105,971 | -6.8 % |

jtcost fit, the same steps:

| Step | instructions | model jtcps3 | change |
|---|---|---|---|
| c_swamp_drain 61 | 105,200 -> 86,881 | 400,135 -> 343,852 | -14.1 % |
| c_swamp_drain 161 | 196,568 -> 179,731 | 779,390 -> 711,413 | -8.7 % |
| c_swamp_swim 571 | 161,856 -> 143,043 | 615,379 -> 566,444 | -8.0 % |
| c_swamp_swim 721 | 121,922 -> 98,781 | 472,610 -> 402,662 | -14.8 % |
| p5_lush_l5s11 991 | 117,460 -> 104,891 | 447,198 -> 410,466 | -8.2 % |
| p5_lush_l6s23 1628 | 120,243 -> 103,948 | 447,746 -> 403,287 | -9.9 % |
| p5_caveman 2196 | 103,713 -> 98,123 | 404,903 -> 394,660 | -2.5 % |

**Projected jtcps3 route means** (with step 1; not measured on 1f1815a). Base: the MAME mean times the route's last
measured jtcps3 / MAME ratio (drain 763.2 / 167.5 = 4.56, swim 611.3 / 131.2 = 4.66, l5s11 504.4 / 120.0 = 4.20,
l6s23 481.5 / 111.5 = 4.32: cc1b94f's jtcps3 run, LUSH.md 10.4, against 5e7974c's MAME, the same game code;
caveman 433.0 / 99.6 = 4.35, totem's run). A: the same ratio for the deact build (its MAME change). B: the base
scaled by the route's jtcost change (mean of its steps). Deactivation removes whole instances' code and data
from the step, so B, which counts the misses, is the likelier one.

| Route | base (est.) | A: MAME change | B: jtcost change | vs 0.525 M |
|---|---|---|---|---|
| c_swamp_drain | 674 K | 634 K | 597 K | over (+14-21 %) |
| c_swamp_swim | 534 K | 507 K | 473 K | met |
| p5_lush_l5s11 | 475 K | 429 K | 436 K | met |
| p5_lush_l6s23 | 436 K | 402 K | 393 K | met |
| p5_caveman | 428 K | 419 K | 417 K | met |

A jtcps3 run on .62 (playsh2_jt.sh with DEACT=32) would replace these.

## 6. Adoption (2026-10-06, on main 86b81a5)

- **Defaults.** play.h `#define PLAY_DEACT 32` (every build: the game, tests/game, playsh2, test/host). tools/tracer.py
  `TRACE_DEACT` defaults to 32, so scripts/hd_trace.sh and every caller (build/retrace_cmds.sh, p4_trace.sh,
  snd_traces.sh, ...) make deactivating traces without a new variable. Stock HD: `TRACE_DEACT=0`; the port without
  it: `make -C test/host DEACT=0 OUT=...`, `DEACT=0 scripts/playsh2_check.sh` / `playsh2_jt.sh` (they rewrite
  play.h's line). Only route traces change: generator mode (--gen) and the RNG probe have no Begin Step pass, and
  the pass runs only in rLevel / rLevel2 / rLevel3.
- **Memory.** tests/game's main RAM: the list was 320 x (id, x, y) = 3.2 KB and broke the link (.bss leaves under
  32 KB for the stack). Now ids only (the pass reads x, y through the id on both sides) and a bit table:
  stack room 32,892 B (the limit is 32,768).
- **tests/equiv_accept.txt:** `c_jungle_firefrog_s296 115`, the spike-blood order (section 5).
- **Checks on 86b81a5 + this branch:** PLAY_DEACT 0 build vs 86b81a5: every tests/routes/*.txt run (94) and fullreg
  (p5_caveman, p4_bomb_throw x levels 1-16 x seeds 1-20) byte-identical stdout. Default build: the 15 dz routes
  record-equal; the 15 traces made again with the id-read GML byte-identical to the earlier ones.
