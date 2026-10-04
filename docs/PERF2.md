# PERF2: halving the play step on the SH-2

Status 2026-10-04, plan only (perf2). Measured on main 28a4cf8 plus perf2's cut 1 (f23d9c0), with the grid
commit 63799b8 cherry-picked on top. Before any of the big restructurings below, this plan goes to the lead.

## Target

| | MAME clocks a step (mean, 71 jobs) |
|---|---|
| Exact build (R-tree), main 28a4cf8 | 371,626 |
| Grid build (newest-first grid, 16 px cells), same tree | **269,053** (median 235,264; p99 783,392) |
| perf1's measurement of main + grid (zero compares +84, noise; moveTo int loop -0.4 K) | 269,279 |
| **Target: half of the grid build** | **about 135 K (-134 K)** |

What jtcps3 needs:
- **Ratios used:** no grid-build jtcps3 run exists yet (EQUIV.md: JT_RESULT). The ratios are PLAN's:
  - steps 3.41x (tests/gametime);
  - steps 3.5-3.6x (playsh2_jt, e380bb6);
  - game pair 3.77x.
- **The 2-frame pair:** 0.84 M jtcps3 clocks, which must hold:
  - draw: 0.28 M;
  - VBlank: 2 x 17 K;
  - sound: 0.5 K;
  - **step: what is left, 0.525 M jtcps3 = 125 K MAME** (at 4.2x: the grid build measured 4.21 on jtcps3, EQUIV.md;
    the earlier 3.41-3.6x gave 146-154 K and overstated the headroom, docs/REVIEW-SH2.md).
- **The ratio rises as instruction cuts land** (exact 3.91 -> grid 4.21), so 125 K is an estimate:
  `scripts/jtcost.sh` (modelled jtcps3 clocks, docs/PERF3.md) is the metric.
- **Step alone:** under 0.84 M would only need 233-246 K MAME. That excludes draw, so it is not enough.

So the target is **≤ 125 K MAME** (0.525 M jtcps3 / 4.2), checked in modelled jtcps3 clocks (PERF3).

MAME does not model the cache or wait states. On jtcps3, extra cost comes from:
- **Cache misses:** about 2.5 clocks an instruction for straight-line code that misses.
- **Stores:** about 6 clocks each. The cache is write-through with no write buffer.
- **Uncached loads:** 7.5 clocks each.

So changes that run less code per step, or store less, gain more on jtcps3 than MAME shows. Skipping whole Step
events and direct dispatch both do this.

## Where a grid step goes

Two measurements:
- **PC samples:** 58,895 samples (PROF=512, PROF_SKIP=4, PROF_WRAP=1). Soft-float is charged to its caller. The
  samples include level starts (about 7 % of samples).
- **ATTR=1:** exclusive time per event and object. The wrappers add about 29 K, so ATTR's step is 297.9 K.

Calls per step come from playhost_grid's PLAY_STATS counters over the p4 / p5 routes.

| Subsystem | Share | MAME clocks a step | Calls a step | Main functions |
|---|---|---|---|---|
| isCollision* line queries (grid scan, line_hit, box cache, float -> int position decode) | 27.4 % | 74 K | about 80 lines, about 300 cell-list visits | collision_line_any_i 8.7 %, bbkind 6.6 % (1.8 % soft-float), line_hit 3.9 %, ibounds 1.7 %, pin_xy_int 1.6 %, pos_int / dwhole 1.5 %, grid_flush 0.8 % |
| Other collision queries | 6.5 % | 17 K | about 22 points, 15 rects, 2 places | point_hit, fam_get, instance_place_p, distance_to_instance_p |
| Collision upkeep: grid put / take out, dirty marks, collision-event pass | 15.0 % | 40 K | the pass 18 K (ATTR) | query_e, flush, mark_e, has_col, ebbox_int, cupdate_at; search_rec 3.5 % is mostly level start |
| Step loop | 12.8 % | 35 K | 15 snapshots (about 120 instances), about 75 animated | play_step 8.6 %: the xprevious copy over every alive instance (about 800), 12 alarm passes, animate inlined; snapshot 2.6 % |
| Event dispatch | 11.2 % | 30 K | 58.6 Step calls | ev_step 4.4 %; package hooks tried in turn: pen_step 1.5, pitem_step 1.1, pdam_step 0.8, pshop_step 0.5, ptrans_step 0.4, p*_ev 1.6 |
| Math | 6.6 % | 18 K | | psqrt 4.2 % (Newton with double divides), dd_sincos 1.4 % |
| Object code and soft-float | 20.4 % | 55 K | | pl_step, jar_step, gameStepEvent, characterStepEvent, eview, pen_jar_hit, moveTo; soft-float 11.4 % of all samples |

Step events by object (ATTR, exclusive of the wrapped collision_point / rect / place calls, isCollision* included):

| Object | Clocks a step | Calls a step | Clocks a call |
|---|---|---|---|
| oJar | 27.7 K | 4.3 | 6.4 K |
| oPlayer1 | 26.4 K | 1.0 | 26.8 K |
| oGoldBar | 21.5 K | 8.4 | 2.6 K |
| oGoldBars | 11.1 K | 5.2 | 2.2 K |
| oLava | 10.0 K | 0.7 | 14.2 K |
| oBat | 7.9 K | 0.7 | 11.6 K |
| oSapphireBig | 7.8 K | 4.5 | 1.7 K |
| oGame | 7.5 K | 1.0 | 7.6 K |
| oSkull | 6.8 K | 1.1 | 6.1 K |
| oSnake | 6.5 K | 2.0 | 3.2 K |

Treasure and items (jar, skull, rock, chest, gold, gems) add up to about 90 K ATTR, which is about 80 K MAME. Most
of that is objects lying still: their Step reruns the same isCollision* lines, the lava rect and point, and a
y - 1 / y + 1 pair every step.

## The restructurings

Each item below gives the design, the expected saving in MAME clocks a step, and the exactness argument. All are
measured on the grid build.

| # | Change | Expected saving |
|---|---|---|
| A | Resting objects skip their physics | 45-60 K |
| B | Terrain bitmap for the oSolid queries | 35-50 K, overlaps A |
| C | Integer positions kept with the floats | 10-15 K, overlaps B |
| D | Direct per-object event functions | 15-20 K |
| E | Step loop: only the instances that need it | 15-20 K |
| F | sqrt from integers, exactly rounded | 10 K |
| G | Later | |

### A. Resting objects skip their physics (proved per object)

**Design.** Each item and treasure Step is split as its GML already orders it:
1. a terrain-physics part: isCollision* flags, moveTo, velocity update, the y - 1 / y + 1 probe, depth;
2. an interactions part: the lava rect and point, pen_jar_hit, pdam_jar_hit, being held. This part always runs.

The physics part is skipped when all of these hold:
- the instance is at a recorded fixed point. Its last full run started and ended in the same state: x, y, xVel,
  yVel, the col* flags, status and state, all compared by bits.
- no world input that part reads has changed since that run.

The world inputs are versions:
- a per-cell version of the oSolid family in the solid grid, raised when an instance enters, leaves or changes box
  in that cell. Only the cells under the query region (the box plus 1 px) are compared.
- a global version for anything else the part reads, such as G.hasSpectacles for the depth.

**Writes on a skip.** A skipped run performs the same writes as a full run. The full run's net effect at rest is
none, but its pin_sety pair marks the instance, so the skip calls the same mark. Depth is written with the same
value.

**Objects.** First the ones with the largest cost at rest: oGoldBar, oGoldBars, gems, oJar, oSkull, oRock, oChest.
Each gets its own proof from the GML: no RNG draw, no timers, no reads outside the listed inputs.

**Expected saving.** 45-60 K. Unknown: the fraction of steps each object is at rest. A host counter measures it
before implementation.

**Exactness.**
- **Grid build:** search order is creation order, so it does not depend on update history. Skipping a run whose
  effects equal its replayed writes gives identical records. Checked by:
  - grid build with the skip against grid build without it: record-equal on every route, and the SH-2 checksums;
  - a host shadow mode that runs the full step on a copy at every skip and compares state.
- **Exact build:** the R-tree's shape depends on when the dirty list is flushed. A skip moves the flush to a later
  query, so the exact build keeps the full Step (compile-time switch). Its output is unchanged by construction.

### B. Terrain bitmap for the oSolid queries

**Design.** Generated terrain is 16 x 16 blocks on the 16 px grid, with no scale and no rotation. A block whose box
is exactly its cell and whose mask is full (or which is not precise) sets that cell's bit in a 65 x 49 bitmap (400
bytes). Such blocks also go into a "static" side list, so the scanned cell lists hold only the other solids.

Moving a block, rescaling it, changing its sprite or destroying it clears its bit and puts it back in the scanned
lists. pin_changed already marks all of these.

How each query uses the bitmap:
- **isCollision* (collision_line_any_i, existence only):** test the bits of the 1-3 cells the axis-aligned line
  crosses (one AND each), then scan only the non-static lists.
- **collision_point over oSolid, and getIdCollision* (they need the oldest hit):** use the bitmap when no
  non-static candidate is older.

**Expected saving.** 35-50 K: a query goes from about 900 clocks to about 100. It overlaps A, since resting
objects issue fewer queries.

**Exactness.** For a whole-number axis-aligned line, line_hit against an integer box with a full mask is the
overlap test, which is what the bit encodes. Existence does not depend on order. Both builds use the solid grid
for these queries already (perf1), so both stay bit-identical. Checked by colprobe, by pcolxv-style random queries
against the scan, and by the gates.

### C. Integer positions kept with the floats

**Design.**
- pin_setx and pin_sety store int16 x and y, plus a whole-number flag, next to the floats. It is computed once per
  write from the float's bits, which fwhole already does.
- ibounds, pin_xy_int, pq_init, bbkind's integer box, moveTo's loop (cut 2, already drafted) and the line ends read
  the ints.
- The integer box becomes the int position plus a per-sprite offset, so a moved object no longer recomputes it
  through dunit, dzero and pos_int.

perf1's idea 1 also goes here. bbkind's dunit(xscale), dunit(yscale) and dzero(angle) become tests on the floats'
bits, which removes about 3 __extendsfdf2 per box recompute (4-6 K).

**Expected saving.** 10-15 K: pos_int, dwhole, pin_xy_int, ibounds and most of bbkind. It overlaps B.

**Exactness.** The int is the float's exact value when the flag is set; otherwise the float path runs as now.

### D. Direct per-object event functions

**Design.** Today ev_step tries front_ev, pen_step, pdam_step, pshop_step and pitem_step in turn, then a switch with
a default chain (ptrans_step, obj_is(oTreasure), obj_is(oItem), the package hooks).

The change is one table, step_fn[OBJ_COUNT], filled once at start. Each package exports which objects it claims,
in the same order as the hooks are tried now, so the first claimant is the same. ev_step becomes one indexed call.
The same goes for ev_draw, ev_end_step and ev_alarm.

**Expected saving.** 15-20 K of the 30 K. On jtcps3 the gain is larger: the hooks' code no longer runs for every
event.

**Exactness.** The same function runs for every (object, event) pair. Checked with a host assert mode that runs
the old chain's claim test next to the table.

### E. Step loop: only the instances that need it

**Design.**
- **xprevious / yprevious:** copied only for instances on the non-terrain list and for terrain marked as moved
  since the last copy (pw_changed). Terrain never moves otherwise.
  - Gain: about 1,600 loads and 1,600 stores a step, which costs about 10 K on jtcps3.
- **Alarms:** keep per-alarm counts of armed instances (alarm[a] >= 0). An alarm with a count of 0 skips its
  snapshot and its pass. Most steps have 0 or 1 armed alarms.
- **Snapshots:** the Step, Outside and End Step lists are kept incrementally on create and destroy (PERF.md item 7)
  instead of being rebuilt every step.

**Expected saving.** 15-20 K.

**Exactness.**
- xprev equals x for every instance whose x has not been written since the last copy.
- An unarmed alarm does nothing in the pass.
- The incremental lists keep the same order: object runtime order, then creation.

### F. sqrt from integers, exactly rounded

**Design.** psqrt currently runs Newton's method with double divides and then nudges the result. The replacement
computes IEEE sqrt from the mantissa with a 64-bit integer square root, which is exactly rounded. Inputs outside
[2^-60, 2^100] keep the current psqrt.

**Expected saving.** About 10 K (psqrt is 4.2 %).

**Exactness.**
- **Observed on the host:** psqrt equals sqrt() on 65.8 M inputs in the game's range: integer and quarter-pixel
  sums of squares, float positions, thirds. They differ only below 1e-30 or above 1e30, where the Newton loop's
  200-iteration cap ends early. Those inputs keep psqrt.
- The integer routine is checked against sqrt() on the same sets.

### G. Later, after A-F are measured

| Change | Notes |
|---|---|
| Soft-float in object code | eview, gameStepEvent, pen_jar_hit, characterStepEvent: whole-number fast paths as in perf1 (about 11 % of samples are soft-float called from events) |
| Collision-event pass | Only objects with collision events, from per-object lists |
| oLava Step | 14 K a call; it needs its own look |

## Hardware

Findings from the jtframe SH7604 RTL (modules/jtframe/hdl/cpu/sh7604) and the cps3-testgame / Maldita
measurements:

| Feature | Facts | Use here |
|---|---|---|
| Cache | 4 KB, 4-way, 16-byte lines, write-through, no write buffer; CCR two-way mode gives 2 KB of cache RAM at 0xC0000000 (RTL supports it; MAME maps only 1 KB) | **No.** A step walks about 39 KB of code, so halving the cache adds misses. Less code per step (A, D) is the lever. |
| DIVU | About 39 clocks on jtcps3; MAME divides at once | moveTo's `play_time % r` and the like only. Small, and MAME would hide its cost. Not planned. |
| DMAC | Works memory to memory. Only 16-byte units are faster than CPU stores on jtcps3 (0.77 clocks a byte). | No bulk clears or copies in a step. Level start only (out of scope). |
| MAC / dmuls | Present. dmulu + sts costs about 9 clocks on jtcps3 against 2 in MAME. | Positions and velocities stay binary64 / float32 (PLAN §1), so there are no fixed-point products to move. The soft-float multiply already uses dmulu. |
| CPS3 PPU / DMA (character, palette, sprite list) | | Draw-side only. It does not reduce the step. |

## Order and gates

Order: F (small, independent), E, D, C, B, A. Each one is measured on its own on the SH-2 grid build.

Every cut passes:
- build/gates.sh;
- every c_* content route;
- the 640-run full regression;
- SH-2 0 warnings and the same libgcc helper list;
- SH-2 checksums equal in both builds;
- equiv_check.sh on the grid build.

A also needs grid-with-skip against grid-without-skip record equality, and the shadow check.

Expected total, counting the overlaps: 269 K minus about 110-140 K gives **about 130-160 K**.
- **135 K target:** that range reaches it only at its low end.
- **125 K jtcps3 pair fit (at 4.2x):** not reached by this range.

So the G items (object soft-float, the collision-event pass, oLava) are probably needed too. On jtcps3, A, D and E
should gain more than MAME shows, because they remove code walked through the cache and the stores. A jtcps3 run
after A-F (on .81, through the lead) settles which figure applies.

Included from perf1's list (measured on main + grid):
- bbkind on bits: C.
- xprevious from a moved list: E.
- psqrt: F. It covers all callers, not only whole-number arguments.
- owner table for ev_step: D.

perf1's moveTo int-loop patch is equivalent to perf2's draft and goes into C.

## Handoff (perf2, 2026-10-04)

### Corrected baseline

The profile and the 269 K figure above were measured on a cherry-pick of the old grid commit. That build lacks
58756ab's fix: HandleCollision's search goes through search_run. So it ran no collision events, and its
equiv_check failed on most routes.

The real grid baseline is **main 58756ab: 281,254 MAME clocks mean step** (6851/6851 checksums). The exact build is
371.6 K.

The subsystem shares above are still roughly right, apart from the collision-event pass. The 50 % target is
therefore about 140 K. The jtcps3 pair fit needs a step of about 125 K MAME (see Target).

### Done: B (0fc7933 on this branch)

**What it does** (src/game/pworld.c):
- Each solid-grid cell keeps two counts (gfull / gfblk / gother, filled by gsum_in / gsum_out from grid_flush and
  grid_unlink):
  - exact 16 x 16 blocks;
  - other oSolid entries that may reach the cell.
- collision_line_any_i, through line_summary, answers from the line's own cells:
  - a usable block on an axis line is a hit;
  - no other entry and no usable block is a sure miss;
  - anything else goes to line_scan, the old scan.
- PLAY_STATS host builds compare every summary answer with the scan and abort on a difference. There were 0 across
  all routes.

**Mix.** About 29 % summary hits, 71 % sure misses. Only p5_idol scans (2 % of its line queries).

**SH-2.**
- Grid: 281,254 -> 261,257 (**-20.0 K**; estimate 35-50 K).
- Exact: 371.6 K -> 351.3 K.
- 6851/6851 checksums in both builds.

**Gates.**
- Exact build: routes 19/19 in both host builds, p4 5/5, gen 9/9, colprobe 0, snd 19/19, c_* 48/48, fullreg
  640/640.
- EQUIV 72/72.
- SH-2: 0 warnings; libgcc helpers unchanged.

**Why it saved less than estimated.** The line queries are now cheap, but their callers still pay:
- ibounds / pin_xy_int / pos_int decoding the float position;
- the bbkind box cache;
- the pcol_query flush.

Item C (int positions) removes the decode.

### At-rest measurements (grid host build, routes p5_* and p4_items)

| Code | Share of Step calls that reach the terrain part | Share of terrain runs that end in their start state |
|---|---|---|
| Treasure (treasure_step) | about 20 % (the rest leave at `inview && state == 1`) | 99.3 % |
| Items (item_step) | | about 98 % |
| Jars | | 0 % as first measured |

**Jar state issue.** jar_step clears colTop / colLeft / colRight / colBot at its top, before `held`. A snapshot
taken after that clear never equals the end state (colBot 0 -> 1). Taking the start snapshot before the clear
fixes it: the flags are outputs only. A's jar code does this. rest_skip is called before the clear, and the skip
keeps the flags as they are.

### Implemented, not gated: A (branch perf2-A-wip, 102fa89, on top of 0fc7933)

**Measured.**
- SH-2 grid: 261,257 -> **214,280 (-47.0 K**; estimate 45-60 K). 6851/6851 checksums.
- The grid host build with the skip, against the same build with `-DPLAY_NOREST` (scratch script gridreg.sh): 74/74
  route files and 640/640 runs give identical stdout. The one exception is R-line column 15, play_dops, a cost
  counter.

**Code.**

pworld.c on perf2-A-wip:
- gclock and gver (line 46). They are bumped in gsum_in / gsum_out and in grid_reset.
- pw_rest_clock (line 1154).
- pw_rest_still (line 1158): grid_flush, then no gfar, then every cell of the region has gver ≤ the given clock.
- pw_watch / pw_watch_end: count the pw_changed calls on one instance.

pobj.c on perf2-A-wip:
- The rest block starts at line 543. It is compiled only without PCOL_EXACT, PLAY_FIXED, NUM_IS_CLASS and
  PLAY_NOREST; otherwise the stubs at line 623 run the full Step.
- rst[EXT_MAX] is indexed by the pin_ext record and checked against the instance id.
- rest_get / rest_ne compare x, y, xVel, yVel, myGrav, the four col flags, stuck and status by their bits.
- rest_region is the box ± 3 px, x and y included.
- rest_skip (line 582), rest_begin, rest_end. rest_end records the state only when it equals the start state,
  xVel = yVel = +0, and the position is whole.

The hooks:
- **item_step (line 629).**
  - The chain after `held` is skipped; T_BOMB and T_ARROW are excluded.
  - The lava part of branch :69 moved after the chain, as `if (br)`. Same order: it was the branch's last
    statement.
- **jar_step (line 757).** The skip jumps to `lava:` (line 812) with `destroy` restored from the record.
- **treasure_step (line 835).** The skip jumps to `terrain_done:` (line 870). Depth and lava still run.

**Proof per object.** The terrain part reads only:
- the instance fields listed in rest_get;
- the object and the collision offsets;
- oSolid-family queries within ± 2 px of the box.

And:
- **Velocity:** with xVel = yVel = +0, moveTo moves nothing and does not read play_time.
- **No other effects:** no RNG draws, no globals, no other instances' fields. The item :69 branch's sticky-bomb and
  arrow paths are excluded by type.
- **Writes the skip must replay:**
  - pin_setx / pin_sety marks, as one pin_changed_ when the full run made any;
  - `destroy` for jars;
  - the branch taken for items (lava only after :69).
- **Exact build:** keeps the full Step, because a skip moves the R-tree's flush.

**Still to do before A can be committed.**
1. Exact-build gates on perf2-A-wip. They should be unchanged: the code is compiled out under PCOL_EXACT.
2. scripts/equiv_check.sh on perf2-A-wip.
3. The SH-2 compile check: 0 warnings, same helpers. RAM: rst[400] at about 40 bytes is 16 KB. Check it fits
   (PIN 1000).
4. Squash and write a real commit message.

**Possible extensions.** Same pattern for oRock / oChest if they are not already covered (they go through
item_step). Enemies are not covered.

### Not started

D, E, F and C, in that order. The designs are above.

Notes for whoever continues:
- **SH-2 runs:**
  - rsync src/, tests/, test/, scripts/, docker/, build/gen and build/snd, plus cps3-testgame/sdk and tools, into
    a plain directory under the main checkout's build/. That is perf2's scratch psh2.sh.
  - Run scripts/playsh2_check.sh there with SOFTFP=1 GAME_REV=WORKTREE GAME_DIR=src/game.
  - MAME goes through scripts/mame.sh.
- **The SH-2 compile check:** run `make -C test/host sh2` in the cps3-dev container on that copy. Compare the
  warnings and the `nm -u` shift helpers with main.
