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
