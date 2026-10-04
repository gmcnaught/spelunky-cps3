# Play-step performance on the SH-2: where the time goes and what would remove it

Status 2026-10-04, src/game at edb4171. Measured by tests/playsh2 in MAME (SOFTFP=1: src/sh2/softfp.c and
softfp_sh2.S), 70 jobs (48 generator cases, 22 routes); counts from test/host/playhost.c built with
-DPLAY_STATS. Analysis only. Changes to src/game belong to its owner and must stay bit-identical: same records on
the host for every route, the same SH-2 checksums (scripts/playsh2_check.sh), and the jtcps3 variant
(scripts/playsh2_jt.sh) equal.

## Budget

| | MAME clocks |
|---|---|
| One game step on jtcps3 (2 frames of 419,470, at the measured ×3.42–3.59 jtcps3/MAME ratio for steps) | **about 245 K** |
| Draw + VBlank + sound per step pair (tests/gametime, game sections) | about 50 K |
| Left for the step | **about 195 K** |
| Mean step now (all route steps) | 507 K (median 459 K, p99 1.53 M) |
| To remove | **about 310 K** (60 %) |

First steps of a level (2.0–2.8 M) and room changes with generation (up to 30 M) are outside this budget: they
need a loading frame or splitting over frames.

Code placement does not help: hot-first layout changed nothing on jtcps3 (+0.5 %, PLAN P4). A step runs about
39 KB of distinct code through a 4 KB cache. The lever is less work per step.

## Where a step goes (mean step 507 K MAME clocks)

PC samples (65,536 over all route steps, scripts/playsh2_check.sh PROF=512 PROF_SKIP=4), self time grouped:

| Part | Share | Clocks a step | Main functions |
|---|---|---|---|
| Collision tree, updates | 24.0 % | 122 K | insert_rec, rcomb_growth, rarea, remove_rec, rcover, add_branch |
| Collision tree, searches | 16.4 % | 83 K | search_rec (87 % of its calls through pcol_search_i: the collision functions), query_e |
| Collision tree, key conversions | 10.5 % | 53 K | fint_bits (from rcomb_growth and rarea), ikey (from pcol_search_i) |
| Terrain collision tests | 12.1 % | 61 K | collision_line_i, line_cb, line_run, ibounds, bbkind, pin_xy_int, point_hit |
| Step loop | 13.4 % | 68 K | play_step (animate over every instance), snapshot |
| Soft-float | about 6.5 % | about 33 K | adddf3, nedf2, addsf3, floatsidf, floatsisf, truncdfsf2, extendsfdf2, muldf3 (callers: moveTo, gameStepEvent, ebbox, dd_sincos, eview) |
| Event code (objects' own logic) | about 15 % | about 75 K | pen_step, pitem_step, pdam_step, jar_step, gameStepEvent, characterStepEvent, … |
| Event dispatch | 2.0 % | 10 K | ev_step's switch |

Exclusive time per event (ATTR=1; tree work done inside an event is charged to it): Step events 356 K a step
(75 % of everything outside the wrapped collision searches). Largest:

| Object | Clocks a step | Calls a step | Clocks a call |
|---|---|---|---|
| oGoldBar | 61.5 K | 8.3 | 7.4 K |
| oPlayer1 | 49.5 K | 1.0 | 50.4 K |
| oJar | 41.9 K | 4.3 | 9.7 K |
| oGoldBars | 35.7 K | 5.2 | 6.8 K |
| oBlood | 20.7 K | 0.6 | 36.9 K |
| oSapphireBig | 20.0 K | 4.4 | 4.6 K |

Most of the treasure and items' cost is resting on the floor: their Step runs moveTo and the isCollision* tests every
step.

Counts per step (playhost PLAY_STATS, p4_push_rope / p5_caveman):

| Count | p4_push_rope | p5_caveman |
|---|---|---|
| Tree searches | 105 | 130 |
| Tree inserts + removes | 43 | 70 |
| Tree node visits | 620 | 800 |
| Collision lines (isCollision*) | 86 | 106 |
| Collision points | 15 | 28 |
| Collision rects | 7 | 19 |
| Instances animated | 728 | 717 |
| Event snapshots | 15 (115 instances visited) | 15 (121 visited) |

Why the tree costs this much: collision_line / collision_rect / instance_place first flush the dirty list
(UpdateTree: every moved tree member is removed and inserted again, as the runner does) and then search the tree.
moveTo moves one pixel at a time and calls isCollision* at every pixel. So a falling item is reinserted and the tree
searched several times per step.

## Candidates, ranked by expected gain

Estimates come from the samples above; each needs its own measurement after the change.

| # | Change | Gain (MAME clocks a step) | Exactness argument |
|---|---|---|---|
| 1 | **isCollision* (collision_line / rect over oSolid, existence only) answered by the solid grid**, not the tree search. Keep the UpdateTree flush at the same points, so the tree's history is unchanged; skip only the search. getIdCollision* (moveTo's push blocks), which need the first instance in tree order, keep the tree. | **70–80 K** (most of the 83 K searches and part of ikey) | isCollision* uses only "!= NOONE". The grid holds every alive oSolid-family instance with a sprite, and the precise-mask test is the same function, so the set of hits is equal; only which one is returned would differ, and it is not used. |
| 2 | **Tree update in place when the entry stays in its leaf**: the runner's remove + insert of a moved entry, done as one step. The removal swaps in the node's last branch; the insert then appends the entry at the end, so do that swap and append in place. Then recompute the covers on the path. Fall back to the full remove + insert when the removal underflows the node, or when ChooseLeaf on the post-removal tree picks another leaf (computed as now). | **50–70 K** (half of the 122 K updates; ChooseLeaf's descent is still paid) | The same node contents in the same branch order and the same covers as remove + insert: Guttman's CondenseTree leaves tight covers, and insert unions them with the new rectangle. Check with tools/treeprobe.py and pcolxv-style random operation sequences against the full path. |
| 3 | **Integer tree keys**: keep the rectangle sides as int32 alongside the float bits (all of them are whole numbers below 2^24 in practice), so rarea / rcomb_growth / ikey need no fint_bits conversion; non-whole rectangles keep the float path | **35–45 K** (most of the 53 K conversions) | float arithmetic on integers below 2^24 is exact, and 18aca5f's integer area maths already showed equal results (pcolxv 53.5 M cases). |
| 4 | **Animate only instances whose image_index changes**: a list of instances with a multi-frame sprite, an Animation End event, or a non-trivial image_speed; terrain with a one-frame sprite at image_index 0 and image_speed × sprite speed = 1 stays at 0 | **30–40 K** (of the 68 K step loop; most of the 720 animated instances are terrain: not yet counted) | 0 + 1 = 1 ≥ 1 → 1 − 1 = 0 exactly in float. An instance leaves the fast case when its sprite, image_index or image_speed is written (the setters already mark changes). |
| 5 | **Resting treasure / items**: when xVel = yVel = 0, the item is on solid ground, nothing it collides with moved and the solid grid did not change, its Step has no effect; skip it | **50–100 K** (treasure and items' Step is about 200 K, mostly at rest) | Needs a per-object proof from the GML: no RNG draw, no timers, no collision with moving instances. The largest gain, but the hardest to make exact; do it after 1–4. |
| 6 | **Soft-float on the hot paths**: moveTo's frac / round(1 / frac) per call (a division), keyed on the velocity's bits; gameStepEvent's and eview's whole-number doubles through integer paths | 10–20 K | The same function of the same bits (memo), or integer results where the double is provably whole. |
| 7 | Event dispatch and snapshots: per-event lists kept incrementally (snapshot rebuilds 15 lists a step) | 5–10 K | The same order (object index, then creation). |

1 + 2 + 3 + 4 ≈ 185–235 K, which would bring the mean step from 507 K to about 270–320 K. The budget (about 195 K
left for the step) then needs 5 and 6 as well, or less work in the events themselves.

## How to check a change

- Host: test/host's playhost against every reference trace (scripts/p4_trace.sh, scripts/p5_trace.sh,
  scripts/p5_regress.sh), both host builds.
- SH-2: scripts/playsh2_check.sh (all jobs, SOFTFP=1: checksums equal, the timing report).
  GAME_REV=WORKTREE tests an uncommitted tree; SNAP_FILE / SNAP_SED try a one-line change on the snapshot only.
- Profile: scripts/playsh2_check.sh PROF=512 PROF_SKIP=4 [PROF_WRAP=1] (PC samples, soft-float charged to the
  caller); ATTR=1 for time by event and object.
- jtcps3: scripts/playsh2_jt.sh (23 jobs, main RAM only; MiSTer zip and expected screen).
