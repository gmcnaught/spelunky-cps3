# LUSH: where a lush step goes, and proposed fixes (2026-10-06, main 9968174)

Goal (PERF3 section 1): mean step <= 0.525 M jtcps3 clocks. Lush on jtcps3 (.62, PERF3 batch 24, cf7d06f):
l5s11 675 K (needs -22 %), l5s37 629 K (-17 %), l6s23 647 K (-19 %).

## 1. Method

`tools/jtcost.py` (fit constants) on one traced step per route, with a new option `JTC_BYOBJ=1`: each instruction's
cost is billed to `play_cur_obj` (the object whose event runs; the store's source register gives the value) and to
the play_step callee it runs under (the pass: ev_step = Step dispatch, pen_step, pl_step, ...). Rows under a pass
named after an event dispatcher are reliable; costs in play_step itself, pcol_handle, snapshot and the other passes
are billed to whichever object ran last and are reported here as "passes". `JTC_BYOBJ_N` sets how many objects get a
per-function table. Steps traced: 201 on all three routes, 401 on l5s11 and l6s23 (same ranking).

    JTC_BYOBJ=1 VARIANT=jtl_x scripts/jtcost.sh p5_lush_l6s23 201     # table in tests/playsh2/build/jtl_x/out/

Instance counts (playhost_grid at step 199): about 700 alive, 546-599 of them oLush terrain. The step is not long
because of instance count: it is a few dozen Steps whose code and data are cold every time (4 KB cache).

## 2. Attribution (modelled jtcps3 clocks of one step; share of the step)

| Group | l5s11 201 (602 K) | l5s37 201 (617 K) | l6s23 201 (659 K) |
|---|---|---|---|
| step-loop passes (animate, alarms, dispatch, pcol marks, collision pass, snapshots) | 137 K 22.8 % | 155 K 25 % | 123 K 19 % |
| oPlayer1 Step (pl_step) | 99 K 16.5 % | 127 K 20.6 % | 129 K 19.6 % |
| spear traps (6 + 6 / 4 + 4 instances) | 80 K 13.2 % | 39 K 6.3 % | 66 K 9.9 % |
| trees, branches, leaves | 50 K 8.3 % | **116 K 18.7 %** | 14 K 2.1 % |
| water life (oPiranha, oBubble) | 38 K 6.4 % | - | **176 K 26.7 %** |
| oManTrap (walking, in view) | 49 K 8.2 % | 17 K 2.8 % | 13 K 2.0 % |
| treasures (in view, mostly at rest) | 36 K 6.0 % | 35 K 5.6 % | 43 K 6.6 % |
| jars / skulls | 25 K 4.2 % | 21 K 3.4 % | 8 K 1.2 % |

The player costs about the same on non-lush p5_caveman 178 (140 K, 22.9 %): shared work, not lush-specific.

Per-function evidence behind each proposal (self cost, the step and route named):
- **Water life** (l6s23 201): 3 piranhas 118 K, 39 K each; their 9 `collision_point_any(.., oWaterSwim)` calls 25 K
  (2.8 K a call). 5 bubbles: 5 calls 19 K. The point is inside water, so `xpoint_none` (the static-family index's
  miss test) fails and `xpoint_any` walks the family's instance list (61 oWaterSwim) with the cell filter.
- **Vegetation** (l5s37 201): 10 oLeaves Steps 59 K (27 `collision_point_any` calls: `tree_or_leaves` twice before
  the view test, twice after; family 7 counts oTree and oLeaves together, so a leaves query that the count does not
  rule out walks both lists); 7 in-view oTree Steps 19 K; 4 oTreeBranch Steps 23 K; out-of-view trees in
  pjungle_idle 10 K. In steady play none of these Steps changes anything.
- **Spear traps** (l5s11 201, 12 traps, about 6.7 K each): the dispatch chain ev_step -> ptrans_step -> pcontent_ev
  -> pjungle_ev about 1.5 K; the trap body (inlined in pjungle_ev) 22.7 K for 12, with about 43 fetch-miss lines per
  trap (its code is evicted between traps); `instance_box_maybe` / `nc_get` / `pl_floor` 22 K; eview 4.8 K; oSpearTrapTop's
  animation (image_speed 0.5) in play_step about 4.6 K. Host counts (whole route): the oEnemy and oItem nearest caches
  are refilled once a step each: oEnemy by the piranha / man-trap moves (real changes), oItem by a resting oCrate's
  `pw_changed` (635 drops in 636 steps on l5s11).
- **Step-loop passes** (l5s11 201, play_step self 91 K by source line): animate's merged walk and anim_one
  (prun.c:172-262) about 36 K; the alarm passes (prun.c:481-490) about 11 K; the Step dispatch loop
  (prun.c:497-517) about 16 K; pcol_changed / pcol_event_done (pcol.c:1260, :1330) about 11 K.
- **Treasures at rest** (l5s11 201, 13 oSapphireBig, 4 of them in a Step): about 2.2 K per Step: ev_step ->
  treasure_step -> rest_skip (rest_get, rest_region, pw_rest_still) -> the replayed `pin_changed_`
  (pobj.c:622: nc_inval, pw_draw_mark, bbk = 0, grid_dirty, mark_e) -> the two lava tests.

## 3. Proposed fixes, in order

Estimates are the modelled cost now billed to the code a fix stops running, per traced step. The fix's own cost and
layout noise (1-1.5 % on jtcps3, PERF3 batch 24) come off them, so expect about half to two thirds of the figure.
Each fix needs the usual exactness argument and PLAY_STATS host check (PERF3 section 2); batch-gated.

| # | Fix | l5s11 | l5s37 | l6s23 | Exactness argument |
|---|---|---|---|---|---|
| 1 | **Full-cell answers in the static-family index.** A per-cell count of entries whose integer box covers the whole cell (water, lava, ladders: sWater is 16 x 16, build/gen/playtables.c:1842; unverified: that its box is the whole image, its instances sit on the 16-px grid, and how its collision kind interacts with prec). A whole point in a cell with such an entry (prec 0 or the entry not precise, and not only `notme`) is a hit with no list walk; the walk stays for partial cells and far boxes. Split family 7 into oTree and oLeaves planes so a leaves query is not answered by tree entries' counts | ~15 K | ~20 K | **~40 K** | same answer as point_hit over the family (a box covering the cell holds every point of it); host compares every answer with collision_point_p, as now |
| 2 | **Vegetation Steps memoized at the Step loop** (oTree, oTreeBranch, oLeaves; extends pjungle_idle to in-view instances). Keep per instance the state its last Step read: the solid grid's gver at the cell below (exists, rest_skip uses it), a version of the tree/leaves index at the side cells (new: bumped by xplace for family 7), and its sprite. Unchanged and the last Step changed nothing: skip the Step (in view or not) | ~40 K | **~90 K** | ~12 K | a Step that reads only those inputs gives the same result; host runs the real Step on every skip and aborts on any change (as PLAY_DCHECK) |
| 3 | **Spear-trap quiet path.** From the Step loop, without the dispatch chain: fired > 0 only decrements; fired == 0 runs the player's band test and the three box tests; the support test (`eview && !CP(x, y + 16, oSolid)`) from gver as in 2. Plus: nc entries updated in place on a member's move (no refill), and the rest-skip replay leaves nc and bbk alone when x, y are bit-equal (pure caches) | ~45 K | ~20 K | ~35 K | the trap's tests are unchanged; only the work that cannot change an answer is skipped; nc_get's host check already compares a kept entry with a fresh fill |
| 4 | **Direct Step handlers.** Extend stepk (pobj.c:1251) with the final handler per object, so a SK_OWN object reached through ptrans_step -> pcontent_ev -> pjungle_ev / pswamp_ev calls its Step directly | ~15 K | ~15 K | ~15 K | same claimant rule as stepk (PERF2 D): the switches depend on the object only; PLAY_DCHECK checks it |
| 5 | **Quiet non-terrain off the animation walk.** As pw_tahead does for terrain: an instance whose image_speed is 0 and whose image_index is in range is off the walk until a setter of img / ispd / sprite puts it back (PERF3 2.5, not done) | ~15 K | ~15 K | ~15 K | anim_one does nothing for it (anim_check's host test extended) |
| 6 | **Rest skip from the Step loop** for treasures and items (rest_skip before ev_step) | ~10 K | ~10 K | ~12 K | the existing rest_skip argument; only the call path moves |

Sum of the estimates: l5s11 ~140 K, l5s37 ~170 K, l6s23 ~130 K (23-28 %); at half to two thirds realized,
12-18 % against the 17-22 % needed. So fixes 1-3 are first (lush-specific, the largest), then 4-6 (every route
gains). If that is not enough, the next lever is the player's Step (20 % here, 23 % on p5_caveman: shared with every
level), which this note does not analyse.

Order: 1 and 2 first on their own (l6s23 and l5s37 each have one dominant group), measured on jtcps3 with
JT_ROUTES set to the three lush routes plus p4_exit559, p5_caveman, p5_snakes, as batch 24 did.

## 4. Not proposed (measured before, PERF3 section 3.3 and 5)

Code / data placement (fully associative bound -4 to -6 % on these steps: the misses are capacity), -Os, cache-RAM
stack, 2-way mode, alignment, unity build beyond 7 files. The ratio jtcps3 / MAME (4.4-4.5 on lush) means cutting
instructions without cutting the code and data touched gains little; every fix above removes whole calls.

## 5. Tool change (uncommitted)

tools/jtcost.py: `JTC_BYOBJ=1` (cost by object, by pass and object, per-object function tables; `JTC_BYOBJ_N`), and
`JTC_PCHIST` now prints the model cost per address too (map to lines with a `OPT="-O2 -g"` build and
`sh-elf-addr2line` in the cps3-dev container; -g leaves simm1.bin byte-identical).

## 6. Implemented (branch lush, 2026-10-06)

| Commit | Fix | jtcost fit, step 201: l5s11 / l5s37 / l6s23 |
|---|---|---|
| main 9968174 | baseline | 602.2 / 616.6 / 659.4 K |
| static-family hint (xhint_hit) | 1 | 612.4 / 611.8 / 632.4 K (l5s11: same object-code instructions, +10.5 K of misses: layout) |
| vegetation memo, + oLeaves out of view | 2 | 575.3 / 531.5 / 629.1 K |
| nc_moved, pw_replayed; spear-trap support memo | 3 | 538.0 / 515.1 / 610.0 K |
| SK_PKG direct package Steps | 4 | 518.3 / 504.3 / 583.5 K |
| animation list for every instance (pin_setispd), on only at image_index / image_speed / sprite changes | 5 | **487.6 / 469.1 / 549.8 K** (-19.0 / -23.9 / -16.6 %) |

Fix 6 (rest skip from the Step loop) was not done: after fix 4 a treasure's Step is already ev_step -> treasure_step,
and what remains is rest_skip's own checks and the replayed collision marks, which the event order needs (estimate
under 1 %).

Found on the way: pin_kill cleared `alive` after pw_draw_mark, so a killed instance went back on the animation list
and a reused slot kept its old place (harmless while the list held terrain only; fixed in the fix-5 commit).

Every commit: 182 host route runs (playhost exact and grid, every P1 / P4 / P5 / P7 / P8 / content route)
byte-identical to 9968174, with the PLAY_STATS checks (veg_quiet re-runs the Step's tests on every skip; anim_check
the list's order and every instance off it; nc_get a fresh fill).

## 7. Measured (2026-10-06, branch lush at the fix-5 refinement)

Gates: make check (host builds, constcheck, P5 regress all route-equal, EQUIV 88/88 with p5_lush_l5s37's accepted
line, snd 0 differ), ctall 59/59, playsh2 9,701/9,701 (grid) and 9,701/9,701 (SOFTFP), shell 26/26 + 49/49,
game_check p4_exit559 0 px at 30/150/300, capture_check 430/430 steps and 6/6 checkpoints, SH-2 0 warnings,
tests/game stack room 34,164 B (xhint and vm in sprite RAM).

MAME SOFTFP step means (steps 2+), batch 24 -> lush: l5s11 146.8 -> 122.1 K, l5s37 138.1 -> 104.3 K, l6s23 141.6 ->
118.4 K, caveman 103.8 -> 96.8 K, snakes 84.6 -> 78.0 K, exit559 91.4 -> 84.0 K.

jtcps3 (.62, JT_ROUTES as batch 24, PASS 11/11; route step means with step 1):

| Route | batch 24 (cf7d06f) | lush | change | vs 0.525 M |
|---|---|---|---|---|
| p5_lush_l5s11 | 675.4 K | 564.4 K | -16.4 % | +7.5 % |
| p5_lush_l5s37 | 628.8 K | 473.8 K | -24.6 % | met |
| p5_lush_l6s23 | 647.4 K | 552.8 K | -14.6 % | +5.3 % |
| p5_caveman | 498.9 K | 477.8 K | -4.2 % | met |
| p5_snakes | 395.6 K | 378.7 K | -4.3 % | met |
| p4_exit559 | 367.1 K | 344.5 K | -6.2 % | met |

What is left (jtcost fit, step 201 on this build): l5s11 488 K: player 127 K, man traps 53 K (walking enemies in
view: moveTo and isCollision*), the collision pass 36 K, piranha 30 K, spear traps 53 K (the Top's animation and the
box tests), bubble 25 K, jars 24 K. l6s23 550 K: player 156 K, 3 piranhas 105 K (spread: point tests 8 K, moveTo 7 K,
soft-float 10 K, psqrt 5 K), bubbles 47 K, spear traps 50 K. Next candidates: the player's Step (every level), the
walking enemies' moveTo / isCollision* chain (man traps, piranhas), the bubbles' point test per step.

## 8. Swimming (2026-10-06, branch swim)

The user saw slowdowns while swimming. None of the three lush timing routes swims; the routes that do are
c_swamp_swim (312 of 418 steps), c_swamp_drain (264 of 370) and c_swamp_lakejaws (167 of 294). MAME SOFTFP step
means (steps 2+, playsh2 ROUTES="c_swamp_swim c_swamp_drain"), not swimming / swimming: 130 K / 239 K (swim),
130 K / 334 K (drain); steps over ~165 K MAME (about the 0.74 M jtcps3 step budget): 98 and 188, all while
swimming. The cost was the piranhas chasing a swimming player: their ATTACK direction (point_direction + a - b) is
not a float direction, so psin_cr / pcos_cr ran the double-double series (dd_sincos) every call, four calls a
piranha step (c_swamp_swim 200: 1,599 K jtcost fit, 64 % in the piranhas).

- 57f19a3: psin_cr / pcos_cr through sincos_r's two Ziv tests (as psincos_cr), the series only when both fail.
  tests/sincos against the old function (cr_trig_dd): 2,000,000,000 random doubles and all 1,135,869,954 float
  directions 0 differ.
- 91c9ed9: psincos_cr at the sites that take cos and sin of one angle (ast-grep call_expression rule over src/game
  and src/front: 24 psin_cr / pcos_cr sites, 7 pairs; the piranha's and jaws' water test and move shared one angle).

c_swamp_swim 200: 1,599 -> 826 K (jtcost fit). MAME means while swimming: swim 239 -> 154 K, drain 334 -> 215 K;
steps over ~165 K: 98 -> 87, 188 -> 186 (the drain room's remaining load: 6 piranhas about 42 K each, spread over
point tests, moveTo, psqrt via point_distance_d, sincos_r; the player 124 K; frogs, bubbles, a bomb).
Gates on 91c9ed9: make check (EQUIV 88/88), ctall 59/59, playsh2 9,701/9,701 grid and SOFTFP, shell 26/26 + 49/49,
game_check 0 px, capture_check 430/430, SH-2 0 warnings.

## 9. jtcps3 on main (2026-10-06, main 2e1c921: lush, swim, sgapply, NC link)

`JTV=_nc NC=nc_robust.txt JT_ROUTES=p5_lush_l5s11,p5_lush_l5s37,p5_lush_l6s23,c_swamp_drain,c_swamp_swim,
c_swamp_piranha,p5_caveman,p5_snakes,p4_exit559 scripts/playsh2_jt.sh` (the NC link the game build uses since
f1d5950, docs/ICACHE.md 4.7 / 6; playsh2_jt.sh links NC only when `NC=` is given), run on .62 (jtcps3.rbf
2026-10-02) with `mister_run.sh`. **PASS 14/14, SPR OK** (5 generation cases, 9 routes; MAME `-nodrc` 14/14 too).
c_swamp_lakejaws is not in the set: it is a `# room` route, which mkjobs.py does not take. Before b16120b,
mkjobs.py `--jt` dropped c_* routes named in JT_ROUTES.

Step means with step 1 (as section 7); max is the largest step, mostly the level start. "Previous" is section 7
(jtcps3, branch lush, cached link); the swamp routes had no jtcps3 run before.

| Route | previous | now (mean) | change | max | vs 0.525 M |
|---|---|---|---|---|---|
| p5_lush_l5s11 | 564.4 K | 516.7 K | -8.4 % | 4.33 M | met (-1.6 %) |
| p5_lush_l5s37 | 473.8 K | 435.7 K | -8.0 % | 4.31 M | met (-17.0 %) |
| p5_lush_l6s23 | 552.8 K | 510.1 K | -7.7 % | 5.14 M | met (-2.8 %) |
| c_swamp_drain | - | 841.1 K | - | 6.85 M | +60.2 % |
| c_swamp_swim | - | 670.8 K | - | 6.85 M | +27.8 % |
| c_swamp_piranha | - | 422.4 K | - | 3.75 M | met (-19.5 %) |
| p5_caveman | 477.8 K | 431.9 K | -9.6 % | 2.83 M | met (-17.7 %) |
| p5_snakes | 378.7 K | 340.4 K | -10.1 % | 3.12 M | met (-35.2 %) |
| p4_exit559 | 344.5 K | 317.3 K | -7.9 % | 60.48 M | met (-39.6 %) |

All three lush routes now meet 0.525 M on the route mean. The routes that swim do not: c_swamp_drain (264 of 370
steps swimming, 6 piranhas in the room) and c_swamp_swim (312 of 418) are over on the mean; per section 8 (MAME),
their swimming steps cost more than the others (swim 154 vs 130 K, drain 215 vs 130 K).

## 10. Swimming, second pass (2026-10-06, branch swim2 on fddd5a7)

Goal: c_swamp_drain / c_swamp_swim (section 9: 841 / 671 K jtcps3, +60 / +28 % over 0.525 M).

### 10.1 Where the swamp routes' steps go

MAME SOFTFP per-step clocks (playsh2 ROUTES="c_swamp_drain c_swamp_swim", fddd5a7): route means (steps 2+)
drain 187.0 K, swim 144.2 K. Steps before the player swims (2-106, both routes) already average 126.6 K (about
550 K jtcps3 at the routes' ratio 4.4-4.5): the room is over budget before any swimming.

| Step range (drain) | MAME mean | What runs |
|---|---|---|
| 2-106 (walking) | 127 K | 6 piranhas active and in view, frogs, man traps, monkey, the player |
| 107-260 (swimming) | 160-210 K | + piranha attacks (psincos_cr), the player's swimming Step, bubbles, blood |
| 269-274 (the blast drains the pool) | 1.40-1.52 M | oGame checkWater over 88 waters each step |
| 275-296 (after) | 190-580 K | drips, rubble, fish bones (the drained piranhas) |

The drain event (269-296) is about 33 K of drain's 187 K mean; c_swamp_swim has no drain.

jtcost fit (JTC_BYOBJ=1, `JTC_BYOBJ_F` rows per object, new), traced steps (record count as jtcost.sh takes it):

| Step | model | by object |
|---|---|---|
| drain 61 (walking) | 576 K | oPiranha 186 K (6, all IDLE), oPlayer1 104 K, oFrog 53 K, oManTrap 44 K, oBubble 44 K |
| drain 161 (swimming) | 955 K | oPiranha 250 K, oPlayer1 187 K, collision pass 61 K, oBlood 64 K, oBubble 62 K, oMonkey 57 K |
| swim 522 (swimming) | 820 K | oPiranha 265 K, oPlayer1 149 K, oBubble 69 K, collision pass 69 K |
| drain 273 (draining) | 5.79 M | oGame 3.82 M (checkWater), oDrip 664 K |
| drain 279 (after) | 2.33 M | oDrip 790 K, oFishBone 248 K, collision pass 242 K, oBlood 191 K |

Per object and function (inclusive where marked):
- **Idle piranha, about 31 K a step each:** `moveTo(±1, 0)` 10.4 K incl. (pw_changed 2.4 K, solid_vline_any,
  bbkind_set, vel_parts); water point tests 8 K: fish_end's `collision_point_any_at(4, 4, oWater)` and the bubble's
  at a point inside the water missed the index (xpoint_none) and went back through collision_point_any's doubles
  (4.4 K a call); prey_swims' four pw_with 1.5 K; instance_first_p(oCharacter) 0.9 K; pdist2 on doubles about 5 K.
- **Attacking piranha:** + psincos_cr 48-50 K incl. (cr_reduce's 3 dd_add, sincos_r twice: about 140 soft-double
  calls) and point_direction_d 8 K. 95 psincos_cr calls on the drain route (370 steps), 97 on swim.
- **checkWater (drain 273):** for each water, instance_place(x -+ 16, y, oWater) twice (pgrid_search 592 K,
  instance_place_p 273 K, overlap_at 270 K) and up to ten collision_point_any.
- **Drips (drain 279):** their oWaterSwim test walked the family's list (3,192 loop iterations in
  collision_point_any, JTC_PCHIST): destroyed water kept its index counts, so drained cells failed xpoint_none.
- **The player while swimming:** 104 K walking, 150-187 K swimming (characterStepEvent 115 K incl., its moveTo 38 K,
  pgrid_search through collision_rect / line 27 K): shared code with every level, not analysed further here.

### 10.2 Fixes, ranked by expected saving (drain mean, MAME)

| # | Fix | Expected | Exactness |
|---|---|---|---|
| 1 | collision_point_any_at answers static-family tests on its own query (whole x, y; or the float position at dx = dy = 0) with the index (xstatic_any), no double re-entry; water / lava tests at the position moved to _at | 3-5 % | the query equals pq_init's on the doubles (PLAY_STATS compares query and answer) |
| 2 | a destroyed static-family entry leaves the index counts at the next query | 1-2 % (drain event) | counts stay a superset of the alive entries |
| 3 | instance_place_p on a static family from the index when no entry, or exactly one, can overlap (checkWater) | 2-3 % (drain event) | the search's result is fixed when at most one entry can pass place_cb |
| 4 | psincos_cr: integer fast path (fixed-point reduction and series, Ziv test) before sincos_r | 3-5 % (attack steps 5-8 %) | a correctly rounded result whenever the test passes; else the old path |
| 5 | instance_first_p memo per object (until the family's alive set changes) | ~1 % | the oldest alive instance changes only with the alive set |
| - | not proposed: idle moveTo / pw_changed (already the PLAY_WALK path: the cost is the move's bookkeeping), pdist2 in ints (the player's position is fractional while swimming) | | |

Even with all of them the route means stay over 0.525 M: the non-swimming swamp steps are about 550 K jtcps3.

### 10.3 Implemented (branch swim2)

MAME SOFTFP route means (steps 2+; "swimming": steps 107+), playsh2 ROUTES="c_swamp_drain c_swamp_swim":

| Commit | Fix | drain (swimming) | swim (swimming) |
|---|---|---|---|
| fddd5a7 | baseline | 187.0 K (211.1) | 144.2 K (150.1) |
| f384b3a | 1: static-family point tests on collision_point_any_at's own query | 180.1 K (203.9) | 137.4 K (143.2) |
| 3ac8e1a | 2: destroyed entries leave the index counts | 177.5 K (200.3) | 137.4 K (143.2) |
| 0b57648 | 3: instance_place_p from the index (checkWater; drain steps 270-274 1.37-1.49 M -> 1.02-1.10 M) | 173.4 K (194.5) | 137.4 K (143.2) |
| 2b12a68 | 4: psincos_cr / psin_cr / pcos_cr integer fast path | **169.1 K (188.6)** | **133.4 K (137.9)** |

-9.6 % (drain) and -7.5 % (swim) on the route means, -10.7 % / -8.1 % on the swimming steps. jtcost fit after fixes
1-3: drain 61 576 -> 556 K, drain 161 955 -> 926 K.

Measured and not kept: collision_point_any_at taking the solid summary on its integer query, with checkWater's ten
point tests and the fish's solid tests moved to it (MAME 173.4 -> 173.7 K: no gain); the instance_first_p memo (fix
5: its per-object arrays, 4.6 KB of .bss, broke the playsh2 link's 32 KB stack reserve, for about 0.6 % of a step).
Fix 3 first added a per-instance byte for empty boxes and broke the tests/game link the same way; empty boxes now
use xisfar's value 2 (tests/game stack room 33,868 B).

Gates on 2b12a68: make check (host builds, constcheck, P5 regress, EQUIV 88/88, snd), ctall 59/59, playsh2 9,701/9,701
grid and SOFTFP, shell 26/26 + 49/49, game_check p4_exit559 / p5_shop / p5_spider 0 px (13 frames), capture_check
428/428 steps and 6/6 checkpoints, SH-2 0 warnings (playsh2, tests/game, capture builds), tests/game link (stack
room 33,868 B). test/host's `make sh2` (PLAY_FIXED) stops at pworld.c:22's pin_ext size assert on fddd5a7 too. Every commit: 182 host
route runs byte-identical to fddd5a7. tests/sincos `fast`: 3,407,609,862 arguments, 0 differ.

What is left: the room itself. Steps before the player swims cost about 120 K MAME (about 530 K jtcps3): six active
piranhas (idle: moveTo, water tests, bubbles), frogs, man traps, a monkey and the player. The piranhas' idle moveTo
(about 10 K jtcps3 each) is the PLAY_WALK path already: its cost is the move's own bookkeeping (pw_changed's marks,
the box cache, one solid line). The player's swimming Step (characterStepEvent 115 K incl. on drain 161) is shared
code; it is the next candidate.

### 10.4 Idle piranhas, and jtcps3 (branch swim2 on main 041d709)

jtcost fit on 041d709, c_swamp_drain 61 (walking): 552 K, the six idle piranhas 176 K, of it 34.7 K instructions
and 3,742 fetch-miss lines (each piranha's Step touches about 10 KB of code, more than the 4 KB cache, so each of the
six starts cold). Per piranha: moveTo(+-1, 0) 10.6 K incl., point tests 6.5 K, pdist2 doubles 3.4 K, pw_with x4
1.5 K, instance_first_p 1 K.

| Commit | Change | MAME drain / swim |
|---|---|---|
| 041d709 | section 10.3's four fixes | 169.1 / 133.4 K |
| 4fc1e13 | moveTo_x1: the fish's one-pixel idle swim as moveTo's PLAY_WALK path does it, without the set-up | |
| ad5f648 | prey_swims: objects with no alive instance skipped before pw_with | 167.2 / 131.4 K |
| 9f9425f | struct pq nodbl: collision_point_any_at's whole query does not convert ix, iy to doubles | |
| b984b05 | instance_first_p: last four answers kept while no alive count changes (olive_gen; 36 B) | 165.1 / 129.2 K |
| cc1b94f | pdist_lt_at: piranha / dead fish point_distance < 90 on 2^-16 integers (doubles near T(c) only) | **162.8 / 126.9 K** |

From fddd5a7: drain -12.9 %, swim -12.0 % (MAME); steps before swimming 126.6 -> about 116 K.

jtcps3 (.62, jtcps3.rbf 2026-10-02, NC link: `JTV=_s3 NC=nc_robust.txt JT_ROUTES=c_swamp_drain,c_swamp_swim,
c_swamp_piranha,p5_lush_l5s11,p5_lush_l6s23 scripts/playsh2_jt.sh`; PASS 10/10, SPR OK; route step means with step 1):

| Route | section 9 (fddd5a7) | 041d709 | cc1b94f | change | vs 0.525 M |
|---|---|---|---|---|---|
| c_swamp_drain | 841.1 K | 786.1 K | 763.2 K | -9.3 % | +45 % |
| c_swamp_swim | 670.8 K | 635.0 K | 611.3 K | -8.9 % | +16 % |
| c_swamp_piranha | 422.4 K | 417.4 K | 410.1 K | -2.9 % | met |
| p5_lush_l5s11 | 516.7 K | 506.6 K | 504.4 K | -2.4 % | met |
| p5_lush_l6s23 | 510.1 K | 494.5 K | 481.5 K | -5.6 % | met |

The jtcps3 gain is about two thirds of MAME's: what is left is mostly fetch misses spread over the room's objects
(piranhas, player, frogs, man traps, the collision pass). Byte-identical cuts of this kind will not reach 0.525 M on
c_swamp_drain. Gameplay-visible options (not done; for the user): piranhas active only within a range of the player
tighter than the view (pen_parent_step's eview), or idle piranhas stepping on alternate frames (about 15 % of a
swamp step).

Gates on cc1b94f: make check (EQUIV 88/88), ctall 59/59, playsh2 9,701/9,701 grid and SOFTFP, shell 26/26 + 49/49,
game_check p4_exit559 / p5_shop / p5_spider 0 px (13 frames), capture_check 428/428 and 6/6, SH-2 0 warnings,
tests/game stack room 33828 B. Every commit: 182 host route runs byte-identical.


## 11. Why the swamp rooms cost more: cache capacity, not instructions (2026-10-06, main 041d709)

### 11.1 The gap in numbers

jtcost fit (JTC_BYOBJ=1), main 041d709. Steps by jtcost.sh's record count. The comparison steps are a
representative step of each route: caveman 138 is the route's median MAME step (96 K); l5s37 201 as in section 2.

| Step | model | instructions | fetch misses (cold / capacity) | data misses (cold / capacity; literal pools) | stores |
|---|---|---|---|---|---|
| c_swamp_drain 61 (walking) | **552 K** | 131.8 K | 9,391 (2,718 / 6,047) | 5,714 (1,967 / 3,224; 2,184) | 9,940 |
| p5_lush_l5s37 201 | 455 K | 125.0 K | 6,488 (2,637 / 2,888) | 4,562 (2,060 / 1,877; 1,593) | 8,290 |
| p5_caveman 138 | 420 K | 109.4 K | 6,332 (2,707 / 2,963) | 4,206 (1,876 / 1,942; 1,484) | 8,276 |

Drain 61 runs 5 % more instructions than l5s37 201 and touches about as much distinct code and data (the cold
misses are about equal). It is 97 K dearer, and 92 K of that is capacity misses: +3,159 fetch lines (41 K) and
+1,347 data lines (51 K). That is code and data evicted and fetched again within the step. The model's
fully associative bound is -2.4 %, so these are capacity misses, not conflicts: placement cannot fix them (section 4).

By object (the share of the step whose events run while play_cur_obj is that object):

| | drain 61 | caveman 138 | l5s37 201 |
|---|---|---|---|
| player | 104.5 K (18.9 %) | 165.5 K (39.4 %) | 143.8 K (31.6 %) |
| enemies and their detritus | **305 K (55 %)**: oPiranha 176.3 K (6), oFrog 53.5 K (7), oManTrap 44.4 K (3), oBubble 30.9 K (5) | ~118 K: oWebCannon 54.6 K, oCaveman 28.5 K, oSpiderHang 22.5 K, oSnake 11.6 K | ~129 K: oCaveman 60.3 K, oBat 28.7 K, oEnemySight 24.9 K, oManTrap 15.6 K |

Per instance (a segment per store to play_cur_obj, from a kept trace; code / data footprint = distinct 16-byte lines
touched):

| Instance (drain 61 unless named) | cost | instructions | fetch misses | data misses (literal) | stores | code / data touched |
|---|---|---|---|---|---|---|
| each of the 6 idle piranhas (consecutive in the Step loop) | 26.3-29.4 K, mean 28.5 K | 5.3 K | 623 (8.1 K) | 308 (126) (11.6 K) | 440 (2.2 K) | 9.0 KB / 4.5 KB |
| the frog in view | 38.5 K | 8.3 K | 824 | 371 | 637 | 12.1 KB / 4.8 KB |
| a frog out of view (5) | 1.5 K | 562 | 4-9 | 10 | 62 | 1.5 KB / 0.9 KB |
| the man trap in view | 35.5 K | 8.5 K | 694 | 320 | 753 | 10.8 KB / 4.8 KB |
| a man trap out of view (2) | 3.1-3.3 K | 546-569 | 64-82 | 33-35 | 61 | 1.5 KB / 0.9 KB |
| a bubble (5) | 4.8-7.4 K | 1.17 K | 77-160 | 52-93 | 83 | 2.6 KB / 1.6 KB |
| an animated instance, animation pass (about 25) | 0.65-1.0 K | 380-640 | 0-2 | 3-12 | 16-21 | ~1 KB / 0.5 KB |
| caveman 138: a snake / hanging spider out of view | 0.9 K | 430 | 0-5 | 3-5 | 52 | |
| caveman 138: the caveman in view | 27.7 K | 5.2 K | 627 | 296 | 382 | 9.8 KB / 4.4 KB |

So one idle piranha's Step is a long generic path: 9 KB of code and 4.5 KB of data, against a 4 KB unified cache.
The second piranha finds none of the first one's lines, so each of the six pays the full miss bill: 77 % of a
piranha's cost is misses and stores. Caveman's enemies are mostly out of view and leave at the view test (0.9 K).
HD's oPiranha has no view test and does not inherit oEnemy's Step (refs/hd/src/objects/oPiranha/Step_0.gml: `if
(active)`, and `active` stays as oEnemy's Create set it), so all six piranhas run every step wherever they are.

### 11.2 Per object: what HD's Step requires, and what the port runs

**oPiranha, IDLE** (6 a step on drain and swim). HD: `dist = point_distance(x+4, y+4, oCharacter.x, oCharacter.y)`;
a water point test and a solid point test ahead; `moveTo(±1, 0)`; `dist < 90 and oCharacter.swimming`; four
`instance_nearest` for prey; the bubble timer; `sprite_index` by `dir`; a water point test at (x+4, y+4). That is
three point tests, one 1-pixel move and a few compares. The port (JTC_CALLERS, inclusive, per piranha):

| Part | port path | cost | what is avoidable |
|---|---|---|---|
| Step dispatch | ev_step -> pen_step (switch, obj_is) -> pcontent_ev -> pcontent_step -> pswamp_ev (switch) -> piranha_step | ~1.3 K | everything but the call (stepk could hold the final handler) |
| moveTo(±1, 0) | vel_parts x2 (precip_parts), play_time %, is_character / obj_is x3, ibounds, solid_vline_any -> line_any (3.8 K), pin_setx -> pw_changed (nc_moved, pw_draw_mark, bbk, grid_dirty, pcol_changed -> mark_e, cupdate_at: 3.1 K), pin_sety (no change) | 10.6 K | vel_parts, the modulo, the obj_is tests, the y half: ~1.5-2 K. pw_changed and the line test are the move's own work |
| water ahead | collision_point_any_at -> xstatic_any -> xpoint_none (fails: the point is in water) -> xhint_hit -> point_hit | ~3.7 K | the wrapper chain's code lines |
| solid ahead | `CP(X(i) + 10, Y(i), oSolid)`: extendsfdf2 x2, adddf3, collision_point_any -> pq_init on doubles -> the solid summary | ~1.3 K | the doubles: the position is whole, so the summary can be read on ints |
| dist | instance_first_p (fam_begin over oCharacter's subtree) and pdist2 on doubles (4 extendsfdf2, 3 adddf3, 2 subdf3, 2 muldf3), computed every step | ~3.5 K | it is read only when the player is swimming (IDLE, ATTACK) |
| prey | prey_swims: 4 pw_with copying into a 32-entry stack array | 1.5 K | the copy and 4 calls: walk the lists |
| sprite | fish_end: `DGT(DIR, 90) && DLT(DIR, 270)`: gcmp_dd(a - b), a subdf3 each | ~0.7 K | a compare with a constant on the bits (pcmpc.h) |
| water at (x+4, y+4) | as water ahead | ~3 K | |
| outside the Step | the animation pass (image_speed 0.5: __mulsf3, __addsf3, the compares), the collision pass entry | ~1 K + pass share | |

**oPiranha, ATTACK** (swimming steps): + point_direction_d, RAND x2, psincos_cr (fast path since 2b12a68), a water
test at a fractional point on doubles, moveTo with fractional velocities. About 42 K a piranha on drain 161.

**oBubble** (about 5 alive). HD: `y += yVel; if (!collision_point(x, y, oWater)) instance_destroy()`. Port: ev_step ->
SK_PKG -> pswamp_ev -> PADDV (extendsfdf2, adddf3, truncdfsf2) -> pin_sety -> pw_changed (0.6 K) -> collision_point_any_at
on the float position (pfloor_int x2, xpoint_none fails, xhint_hit, point_hit). 4.8-7.4 K for one float add and one
point test, plus the animation pass (image_speed 0.2: not dyadic, the float add rounds).

**oFrog, oManTrap** (HD: `action_inherited()` first, then the view test). Out of view, HD runs oEnemy's view test
(active = false) and its own: two compares. The port runs ev_step -> pen_step -> pcontent_ev -> pcontent_step ->
pjungle_ev -> frog_step -> pen_parent_step -> eview, then eview again: 1.5 K a frog and 3.1 K a man trap (5 frogs and
2 man traps on drain 61). In view: moveTo with gravity, isCollision* x4-5, distance_to_object_p, the solid point
tests (35-38 K each, one of each in view at step 61).

**Generic per-instance overhead** paid by every active enemy whatever it does: the dispatch chain (1-1.3 K), the
animation pass (0.65-1.0 K: soft-float multiply and add, and the frame compares, even for image_speed 0.5 on a
1-speed sprite), the alarm pass walk (evnz lists: cheap), pcol_event_done after the Step, its collision-pass entry
(the pass is 21 K on drain 61), and pw_changed's marks on every move (nc_moved, the draw mark, the box cache reset,
grid_dirty, pcol_changed: 0.6-3.1 K by how cold it is).

### 11.3 Fixes, ranked (expected jtcps3 saving, jtcost fit)

Estimates are the cost billed now to the code each fix stops running. Because the per-instance footprint is the
problem, shortening the path should also cut misses in what remains; the estimates do not count that.

| # | Fix | walking (drain 61) | swimming (drain 161, swim 522) | byte-identical? |
|---|---|---|---|---|
| 1 | **Lean idle piranha path.** dist and c only when they are read (`PL.swimming && !PL.dead` first: pdist2 has no side effect); the solid test ahead on the whole position's ints (the solid summary, as collision_point_any's int path); fish_end's DIR compares on the bits (CGT / CLT with H(90), L(270)); prey_swims walking the families' lists without the copy | ~25-35 K | ~10 K (fish_end, prey) | yes: the same queries and compares, pure computations reordered or skipped when their results are not read |
| 2 | **moveTo(±1, 0) without the generic walk** for a non-character, non-solid, non-platform mover with ibounds: the one solid_vline_any, then pin_setx. That is what moveTo's PLAY_WALK branch does for these values (vel_parts(±1) = r 0, fl 1; y velocity 0: no y walk, pin_sety unchanged) | ~10 K | idle piranhas only | yes (PLAY_WALK builds only; the exact build keeps moveTo) |
| 3 | **Direct Step for P7 enemies claimed by pen_step** (oPiranha, oFrog, oManTrap, oMonkey, ...): pen_step's oEnemy fallback goes to pcontent_ev, and pcontent_step's claimant depends on the object only (as step_pkg, section 6 fix 4), so stepk can keep SK_PKG + claimant | ~10-18 K (about 18 instances) | the same | yes (PLAY_DCHECK keeps the whole path) |
| 4 | **Off-view enemy pre-check at the Step loop**, for objects whose Step is exactly `pen_parent_step(i); if (!eview(i, 20, 4)) return; ...` (frog, fire frog, man trap, ...): out of view, the Step is `active = 0`; pen_parent_step's first test and the caller's are the same eview, and nothing between them changes the view or the instance | ~10 K | ~5 K | yes |
| 5 | **Animation without soft-float in the common cases**: image_index + image_speed x speed with speed 1.0f (the product is image_speed exactly), and with both operands dyadic and small (0.5 steps) the float sum is exact in fixed point; the frame compares on the bits. Else the float path | ~10 K (about 25 animated instances) | ~10 K | yes, with an exactness argument per case (tests over all relevant floats, as tests/sincos) |
| 6 | **Bubble Step from the Step loop** with the float add on the bits when exact | ~5 K | ~10 K (more bubbles) | yes, by the same kind of argument as 5 |

Expected after 1-4 on drain 61: about 55-75 K (10-14 %), or 552 -> ~485 K. The walking steps would then be near the
0.525 M budget. The swimming steps would not (drain 161 ~886 K): the player's swimming Step (150-187 K) and the
attacking piranhas (42 K each) remain.

### 11.4 Structural options

- **A compact idle piranha path under 4 KB of code and data.** If one idle piranha's path fitted the cache, piranhas
  2-6 would cost about their instructions (~7 K) instead of 28.5 K: up to ~100 K on every walking swamp step. Fixes
  1-2 take soft-double and the generic moveTo off the path. The rest is the shared collision code (line_any, the
  static-family point test, pw_changed -> pcol_changed's tree marks), which a dedicated integer routine (water /
  solid counts read directly, the move's marks inlined) could replace for this case. Byte-identical in principle
  (the same values), but it duplicates collision code that the PLAY_STATS checks would have to cover. Only worth it
  if 1-4 leave the walking steps over budget.
- **Batching the six piranhas' Steps phase by phase** (all water tests, then all moves, ...), so each phase's code
  stays hot. The Steps are already consecutive (object order), so the batching that is free has already happened.
  Splitting phases across instances reorders side effects: RAND draws (the bubble timer), pin_create ids (bubbles),
  and collision marks. A piranha's tests read only static families and the prey lists, which other piranhas do not
  change, so an order-preserving split looks possible. The argument is long and fragile. Not proposed before the
  compact path.
- **Data layout.** Of a piranha's 308 data misses, 126 are literal pools. Most of the rest are the stack (88 % of the
  step's stores are stack stores), the index tables and other instances' records. The instance's own pin / pin_ext /
  pin_en lines are about 10. Packing piranha records together would gain little. Cutting literal-pool misses (RAM
  addresses through GBR, PERF3 1.4) is the data-side lever, and it is shared with every route.
- **Not byte-identical (gameplay-visible), not to implement:** a view test for piranhas (HD runs them everywhere:
  positions, bubbles and their RAND draws would differ off screen; on drain 61 all six are in view anyway, so
  little gain there); stepping idle piranhas every other frame (~85 K on walking steps, visibly different motion).

### 11.5 Implemented (branch swamp3 on main 16ab24f)

swim2's second batch (section 10.4: moveTo_x1, prey_swims' empty-object skip, struct pq nodbl, the instance_first_p
cache, pdist_lt_at) landed on main while this was in progress. It covers 11.3's fix 2 and part of fix 1, so those
are not repeated here. Six commits, each with 182 host route runs byte-identical:

| Commit | Change (11.3 fix) |
|---|---|
| 1d553a0 | fish_end's DIR compares on the bits (CGT 90 / CLT 270, pcmpc.h; tests/cmpc 445 M cases, 0 differ) (1) |
| 262c356 | piranha_step: c and the distance test only where IDLE / ATTACK read them (IDLE: only while the player swims) (1) |
| 0c37d52 | collision_point_any_at: oSolid at a whole position from the solid summary on the int query; the fish's solid tests (1) |
| 15b122c | prey_swims walks the families' object lists (no pw_with copy) (1) |
| 8f49444 | direct package Step for enemies that pen_step hands to pcontent_ev (pen_step returns 2 -> stepk SK_PKG) (3) |
| 6839211 | enemies out of eview(20, 4) whose Step starts with pen_parent_step: active = 0 on the SK_PEN / SK_PKG dispatch (4) |

jtcost fit, main 16ab24f -> swamp3 6839211:

| Step | model | instructions | fetch misses | data misses |
|---|---|---|---|---|
| c_swamp_drain 61 (walking) | 540.4 -> **471.7 K (-12.7 %)** | 125.0 -> 114.0 K | 9,197 -> 7,701 | 5,787 -> 5,000 |
| c_swamp_drain 161 (swimming) | 885.5 -> 831.3 K (-6.1 %) | 213.9 -> 207.9 K | 15,013 -> 13,846 | 9,194 -> 8,464 |
| c_swamp_swim 201 (swimming) | 750.6 -> 698.2 K (-7.0 %) | 182.8 -> 176.7 K | 12,782 -> 11,524 | 7,753 -> 7,105 |
| p5_caveman 138 | 431.8 -> 415.8 K (-3.7 %) | 108.8 -> 107.8 K | 6,552 -> 6,319 | 4,477 -> 4,197 |
| p5_lush_l5s37 201 | 468.4 -> 446.8 K (-4.6 %) | 124.7 -> 123.0 K | 6,725 -> 6,276 | 4,860 -> 4,533 |

MAME SOFTFP (steps 2+): c_swamp_drain 162.8 -> 154.8 K (-4.9 %; walking steps 2-106 113.6 -> 104.0 K, swimming
182.3 -> 175.0 K), c_swamp_swim 126.9 -> 118.5 K (-6.6 %; swimming 131.3 -> 123.4 K). playsh2 SOFTFP mean of all
route steps 90,423 -> 89,503 (-1.0 %): lush -1.4 to -5.4 %, other P5 -0.06 to -0.65 %, P4 / P1 +0.1 to +0.2 %,
p5_snakes +0.5 % (the table lookup on each enemy dispatch).

Then oPlayer1 (the swimming / running Step):

| Commit | Change | effect |
|---|---|---|
| 5436eb3 | characterStepEvent's slope branch: the point_distance Newton loop (about 7 rounds of __divdf3 / __adddf3 / __muldf3 / __nedf2) skipped when d2 <= a * a, where its DGT(dist, a) is false (tests/slopedist: 186.7 M cases) | jtcost caveman 138 415.8 -> 410.1 K (-1.4 %), drain 161 831.3 -> 825.7 K, swim 201 698.2 -> 694.1 K; MAME SOFTFP every P1 / P4 / P5 route -0.1 to -1.05 % (mean 89,503 -> 89,142) |

In swimming steps the loop's test usually holds (the player moves in x and y), so the loop still runs there. The
player's swimming Step at swim 201 (pl_step 149 K incl.): characterStepEvent 111 K (its moveTo 31 K; isCollisionLadder,
Platform, PlatformBottom, WaterTop, Left about 5 K each, through pcol_search_i -> pgrid_search; the Newton loop 8.8 K),
hurt_logic's collision_rect_p 9.3 K, pl_step's collision_rect_p 7.6 K.

Measured and not kept:
- rq_static_none for prec 1 queries while no entry of the family is precise (isCollisionLadder's oLadderTop / oLadder
  queries skip the grid search): MAME -0.5 to -1.2 % on most routes, but jtcost +0.7 to +1.9 % on drain 61 / 161,
  swim 201, caveman 138, l5s37 201, p4_exit559 301 and p5_snakes 956 with the instructions unchanged. An unused
  8-byte .bss pad alone moved drain 61 and l5s37 201 by +1.1 %: this layout noise is the size of the gain.
- An integer path for anim_one (image_index + image_speed in 256ths when the sprite speed is 1.0f; exact,
  1.57 G cases checked against float): drain 61 492.5 -> 492.0 K. It removed 14 __mulsf3 calls but added 508
  instructions and 65 fetch misses to play_step: most swamp instances animate at 0.2 / 0.4, which are not
  multiples of 1/256.
- The off-view test in every ev_step (before the claimant switch): +1 to +2 % MAME on P4 / P5 routes (a switch
  on every Step). It now runs on the enemy dispatch only, from a const table.

Off-view share, for the deactivation decision (instrumented host build, eview(i, 20, 4) at each piranha Step):
c_swamp_drain 1,317 of 2,121 piranha Steps in view, c_swamp_swim 1,306 of 2,508. On walking steps 4 of the 6
piranhas are in view; later in the routes 2-3 are. An off-view idle piranha costs about 18-20 K jtcps3 now.

Gates on 6839211: playsh2 9,701 / 9,701 grid and SOFTFP, shell 26/26 + 49/49, capture_check 428/428 and 6/6,
SH-2 0 compiler warnings (playsh2, tests/game; tests/game's ld "dot moved backwards before .sprbss_a" is on
16ab24f too), tests/game stack room 33,796 B (16ab24f: 33,828 B). Not yet run, waiting for build/trace to be
regenerated: make check (EQUIV), ctall, game_check.
