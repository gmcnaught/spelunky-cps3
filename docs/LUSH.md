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

## 13. Spear traps near instances (2026-10-06, branch totem on 041d709)

User report (MiSTer): jungle levels slow down whenever something is near a totem trap (oSpearTrapBottom / Top /
Lit).

### 13.1 What happens near a trap

Host survey (a temporary print in speartrap_step, not committed): the three box tests (instance_box_maybe) run only
on a step with fired == 0, so instance_nearest runs at most once per trap per 50 steps when something is in line.
An instance that stays in line (an item, a man trap) makes the trap fire every 50 steps, for the whole route.
Fires: p5_lush_l5s11 13 (trap 698, an item), l6s23 14, c_jungle_mantrap 18 (2 traps), c_jungle_monkey 9,
l5s37 0. Each oSpearsLeft lives 31 steps (sSpearsLeft has 31 frames; the animation end destroys it). So the trap
itself is not the cost. The cost is the spear, about 60 % of the steps while something stays in line.

MAME SOFTFP step means (playsh2 ROUTES="p5_lush_l5s11 p5_lush_l6s23 c_jungle_mantrap c_jungle_monkey", steps 2+,
grouped by step mod 50: 2 = the fire step, 3-32 = a spear alive, 33-49 and 0-1 = none):

| Route | 041d709 fire / alive / none | totem fire / alive / none | route mean |
|---|---|---|---|
| p5_lush_l5s11 | 144.2 / 121.6 / 114.4 K | 129.5 / 117.5 / 114.3 K | 119.4 -> 116.6 K (-2.3 %) |
| p5_lush_l6s23 | 122.7 / 113.6 / 110.2 K | 119.9 / 111.2 / 109.8 K | 112.5 -> 110.9 K (-1.4 %) |
| c_jungle_mantrap | 108.0 / 97.1 / 87.1 K | 101.7 / 91.4 / 86.6 K | 93.6 -> 89.8 K (-4.1 %) |
| c_jungle_monkey | 126.3 / 116.7 / 109.0 K | 121.9 / 114.6 / 108.7 K | 114.0 -> 112.5 K (-1.3 %) |

(The monkey route's alive / none gap is partly the monkey's own activity, which follows the same timing.)

jtcost fit, JTC_BYOBJ=1 (arguments are jtcost.sh's record counts: c_jungle_mantrap 203 / 212 / 242 = the fire step,
a spear alive, none):

| Step | 041d709 | totem (f217aaf) |
|---|---|---|
| c_jungle_mantrap 203 / 212 / 242 | 438.5 / 425.4 / 376.1 K | 429.5 / 407.5 / 380.4 K |
| p5_lush_l5s11 303 / 312 / 342 | 617.5 / 500.4 / 478.9 K | 595.2 / 491.5 / 485.6 K |

On 041d709, a step with a spear alive cost +49 K (mantrap 212, 2 spears) over one without. The spear-alive extra:
- **The spear's Step** (oSpearsLeft: collision_point(x +- 16, y) for oSpearTrapTop and oSpearTrapBottom). Both are oSolid
  children, so each test went through collision_point_p: grid_point, then touch_stale / stk_compact. About 10 K a
  spear a step (mantrap 212: collision_point_p from collision_point_any 15.7 K incl., touch_stale 4.0 K).
- **The rectangle tests against oSpearsLeft** once the family is non-empty: each enemy (pen_parent_step, Step :76),
  the player (hurt_logic, :1599) and the damsel run rq_init on the doubles, then rect_run's walk with touches. About
  3 K a call, 26 K on mantrap 212.

The fire step's extra (l5s11 303, +96 K): stk_compact 27.5 K (the stale stack held every mark since its last read)
and evnz_sync 13.8 K (the first oSpearsLeft rebuilt every key's event list; the step after the last spear dies does
the same), plus the creation and the first point tests.

Not the cost: instance_nearest_p. On l6s23 one trap has an item in its box every step (the tests then fail): one
instance_nearest_p 4.5 K, and instance_box_maybe 8.5 K for 21 calls (the idle traps' flat cost: jl6, step 242).

### 13.2 Fixes (branch totem)

| Commit | Change | Exactness |
|---|---|---|
| 131ce3c | the spear's point tests through the vegetation memo: noted with pw_rest_clock, skipped while pw_rest_still holds at the trap's cell and the spear's x, y and sprite are unchanged | as the trap support test (every oSolid-family entry holding the point covers its cell); veg_acts re-runs the tests on every skip |
| 09d2bd4, f217aaf | pcol.c flush_run drops the stale stack's entries that are no longer stale (when it holds more than 32) | stk_compact keeps only stale entries; an entry that becomes stale again is pushed again (membership does not end within a room) |
| e02c249 | evnz_sync per key: an object's list going empty <-> non-empty invalidates only the keys it has (prun_onz, evobj_init's test) | only an object in key k's evobj range changes k's list; the host builds compare every kept list with a rebuild |
| 874ebbb | collision_rect_p: a family of at most 4 instances whose integer boxes all lie off the query's floors gives NOONE without rq_init or the walk | rect_hit's integer and float tests both take corners in [floor(min), floor(max) + 1]; the host builds test every instance with rect_hit |

131ce3c alone made the fire steps worse (l5s11 144 -> 161 K MAME): without the spear's reads, the stale stack grew
until the next read. 09d2bd4 fixes that and bounds stk_compact everywhere. 4109f25 adds its host check: PLAY_STATS
builds keep the uncleaned stack beside it and compare the two compacted stacks at every read (checked to fire: a
stack that drops pushes aborts).

The stack fix and the per-key event lists also help routes without spears. Full playsh2 SOFTFP, 041d709 -> 4109f25
(9,598 route steps): 360 steps cheaper by more than 5 K MAME, 1 dearer (p4_exit559 595, the 21 M transition step, +7.7 K);
the all-route mean is 90.9 -> 90.3 K, the median 78.0 -> 77.5 K. The largest drops are the stack-full compactions (a
whole ENT_MAX stack sorted when mark_e found it full): p1_walk 198 160 -> 67 K MAME, where jtcost (p1_walk 199)
shows stk_compact at 176 K of 475 K, and the step at 298 K after. Other large drops: l5s37 158 151 -> 104 K, p5_l4 170
120 -> 77 K, p4_exit559 375 110 -> 74 K. Per route, the most steps improved are l5s37 (157), p5_reg_l14s16 (53), l5s11
(33) and p5_l4 (32). An object type appearing or disappearing no longer rebuilds every event list: jtcost l5s37 559
evnz_sync 13.5 K -> 0. p5_caveman: mean 96.5 -> 95.9 K, p99 124.4 K both (its large steps are not the stack).

Every commit: 91 routes through playhost, playhost_grid and a playhost_grid built without PLAY_STATS / PLAY_RNGLOG
(the SH-2 build's paths: the host checks re-run skipped queries, so the PLAY_STATS builds alone would not show a
skipped touch), 273 outputs byte-identical to 041d709. playsh2 (the four routes) 2,086 / 2,086 checksums on each.

Steps without a spear: instructions -0.4 % (mantrap 242: 95,357 -> 94,933) and the MAME means unchanged. jtcost
moves by +1.1 % (mantrap 242) and +1.4 % (l5s11 342), with the fully associative bound +0.1 / +0.3 %. That is
layout (131ce3c alone: same instructions, +4.9 K). PERF3's default steps: p4_exit559 301 +1.4 % (same instructions),
p5_snakes 956 +0.3 % (-554 instructions). A jtcps3 run decides.

jtcps3 (.62, jtcps3.rbf 2026-10-02, `JTV=_tt NC=nc_robust.txt JT_ROUTES=c_jungle_mantrap,p5_lush_l5s11,p5_lush_l5s37,
p5_lush_l6s23,p5_caveman,p4_exit559 scripts/playsh2_jt.sh`, the same with GAME_REV=041d709; PASS 11/11, SPR OK on
both). Route step means with step 1:

| Route | 041d709 | totem (1b06a5d) | change |
|---|---|---|---|
| c_jungle_mantrap | 404,628 | 394,731 | -2.4 % |
| p5_lush_l5s11 | 503,874 | 496,518 | -1.5 % |
| p5_lush_l6s23 | 492,144 | 485,155 | -1.4 % |
| p5_lush_l5s37 | 429,867 | 432,517 | +0.6 % |
| p5_caveman | 430,608 | 432,971 | +0.5 % |
| p4_exit559 | 312,837 | 314,291 | +0.5 % |

The routes where traps fire gain 1.4-2.4 % on the mean, which includes about 40 % of steps with no spear. Routes
with no fires lose 0.5-0.6 %, the code-placement cost jtcost showed (same or fewer instructions). The generation
totals are within +-0.3 %.

What is left on a spear-alive step (mantrap 212 vs 242, about 27 K): the rectangle tests' call-site double sums
(X(i) + 2, ...: about 280 instructions a call), veg_quiet, the spears' animation and dispatch. A trap that sees
something keeps firing every 50 steps, as in HD.

Gates on f217aaf: playsh2 9,701 / 9,701 grid and SOFTFP, shell 26/26 + 49/49, capture_check 428/428 steps and 6/6
checkpoints, host builds and constcheck, SH-2 0 compiler warnings (playsh2, tests/game, capture; the make recipe and
ld `.sprbss_a` notices are as on 041d709), tests/game stack room 33,740 B (041d709: 33,868; evnzk / evkn are
128 B), ctall 53 of 53 routes run equal. Held until the reference traces are regenerated (main's build/trace was lost
during the run): ctall's six c_temple routes, make check's P5 regress, EQUIV and snd, and game_check.
