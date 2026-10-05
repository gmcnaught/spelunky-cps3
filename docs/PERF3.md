# PERF3: fitting the play step on jtcps3

Status 2026-10-05: the mean-step goal holds on jtcps3 for the 18 hardware routes at batch 17 (caveman 517.4 K the
largest; p5_snakes 0 of 241 pairs over 0.84 M). **Not met:** p99 (bomb_drop, bomb_throw, buy, spider, idol, p1_walk have
more than 1 % of their steps over the pair in MAME SOFTFP x4.2), and the four p5_reg_* routes, which the jtcps3 build
does not run (mkjobs.py JT_ROUTES): p5_reg_l14s16's MAME SOFTFP step mean is 269.6 K, about 1.1-1.2 M on jtcps3. See
section 5. Builds on docs/REVIEW-SH2.md (the cost model and findings P1-P7) and docs/HANDOFF.md (branch
state). It runs alongside, not instead of, PERF2's remaining items.

## 1. Goal and how it is measured

- **Goal:**
  - mean play step ≤ **0.525 M jtcps3 clocks** on every playsh2 route;
  - p99 ≤ 0.84 M, the whole 2-frame pair, with draw and VBlank inside the pair;
  - level / room starts are handled separately (Phase 5).
- **Primary metric: modelled jtcps3 clocks.** `tools/jtcost.py` (Phase 0.2) on two fixed steps:
  - **p4_exit559 step 301** (enemies off);
  - **p5_snakes step 956** (enemies on).

  Reported per commit with the MAME clocks.
- **Ground truth per batch:** `scripts/playsh2_jt.sh` on the MiSTer (.62, through the lead). The model is recalibrated
  if it drifts more than 10 % from it.
- **The MAME proxy:** the target is ≈ **125 K MAME clocks** (0.525 M / 4.2), not PERF2's 146-154 K. The ratio rises as
  instruction cuts land (exact 3.91 -> grid 4.21), so MAME clocks may only be used to compare commits within this plan,
  never to declare the goal met.

### Baseline (modelled jtcps3 clocks; model in REVIEW-SH2.md section 3)

| Build | p4_exit559 301: instructions / model | p5_snakes 956: instructions / model | MAME step mean, p4 / p5 |
|---|---|---|---|
| main e264467 | 241 K / 926 K | 272 K / 969 K | 198.9 K / 271.8 K |
| + perf3 batch 1 (ab18e2d: A, D, E, F) | 171 K / **608 K** | 171 K / **563 K** | 143.4 K / 173.5 K |
| Goal | | | ≤ 0.525 M modelled and measured |

On batch 1, step 301's jtcps3 cost splits as:
- 28 % execution;
- 22 % fetch misses;
- 26 % data misses;
- 18 % stores, 85 % of them on the stack.

Collision code is 55 % of the step. Batch 1 removed work (A, D, E). It did not change how the remaining work uses
memory.

## 2. Rules for every commit in this plan

1. **Exactness argument in the commit message,** as PERF2 does: codegen-only, layout-only, or a value proof.
   Gating is per batch (memory: batch-gating). Each change is its own commit, the full gates run once per batch, and a
   failing batch is bisected.
2. **Gates (docs/HANDOFF.md):**
   - gates.sh;
   - ctall;
   - equiv_check;
   - fullreg;
   - playsh2 checksums in both builds;
   - SH-2 0 warnings and the same libgcc helper list;
   - the tests/game link (ramcheck: ≥ 32 KB of stack);
   - game_check p4_exit559.
3. **The report per commit:**
   - MAME step mean;
   - modelled jtcps3 on the two steps;
   - stack stores a step;
   - fetch misses a step;
   - .text size;
   - .bss and the stack margin.
4. **Data-type rules for new and changed code (the SH-2 ISA and GCC 13.3's output):**

| Use | Type | Why |
|---|---|---|
| Fields in RAM (instances, grids, tables) | the narrowest **signed** type that holds the range: int8_t / int16_t; flags as bits in one word | 16-byte lines: more fields a line, fewer misses. Signed because `mov.b` / `mov.w` sign-extend for free; unsigned adds `extu.b` / `extu.w` (observed) |
| Field order in hot structs | int32 first (`mov.l @(disp,Rn)` reaches 60 bytes), then int16 (reaches 30, through R0 only), then int8 (reaches 15, through R0 only) | A field past the reach costs an extra `add` or a literal (observed: offset 40 -> `add #32`) |
| Struct size of indexed arrays | a power of two (16, 32, 64) | struct pin is 72 bytes: every `PW.in[i]` is a `mul.l` + `sts` (≈ 9 clocks on jtcps3; 3,584 a step) |
| Locals, parameters, loop counters | int / int32_t | registers are 32-bit; narrow locals can add extends |
| Returning several small values | packed into a `uint32_t` (two int16) or `uint64_t` (four int16, returned in r0:r1) | struct returns and out-pointers go through the stack: 4 stores + reloads for an int16 box (observed) |
| Hot globals | fields of one `struct hot` reached through GBR | one `mov.l @(disp,GBR),r0` in place of a literal load + load (observed) |
| Floats and doubles | the source of truth stays binary64 / float32 (PLAN §1); integer shadows only where the flag proves them equal | exactness |

5. **No new stack contexts in hot paths.**
   - Don't fill a struct on the stack before a fast path that can answer from registers.
   - Don't pass a hot result back through a pointer.

## 3. Phases

### Phase 0: land what exists, make the metric real (enablers)

| # | Work | Done when |
|---|---|---|
| 0.1 | **Merge perf3 batch 1** (branch worktree-agent-a69ec85fed9f74a42: A, D, E, F, 16-bit gver, rst 128) onto main 90542f9 (ramcheck fixed). Finish its full gates and ramcheck | merged; gates in §2 pass |
| 0.2 | **`tools/jtcost.py` + `scripts/jtcost.sh`** from `build/review_ext/cachemodel.py`. The script: trace two fixed steps (write tap on the record count, breakpoint on `play_step`'s return to stop, `noloop`, registers + PR); reduce to the per-group / per-function table; delete the trace. Merge with jtmodel's uncommitted work (build/handoff/jtmodel.patch, jtmodel_new.tar): its constants fitted to EQUIV.md's 54 measured points replace the hand-set ones | one command gives the §1 table for any build; traces deleted after reduction (HANDOFF disk rule) |
| 0.3 | **Calibrate on hardware:** playsh2_jt.sh on .62 for main + batch 1. Record measured against modelled per route in this file | model within 10 % per route, or the constants are refitted |
| 0.4 | **Correct the targets:** PERF2.md's target to 125 K MAME (4.2); report.py's `JT = 2.94` to 4.2, labelled "estimate; jtcost is the metric" | edited |

### Phase 1: stack traffic and calls (codegen-only; expected −10 to −15 % modelled)

Every item here leaves every value unchanged. The exactness argument is "same computation, different calling
convention".

| # | Work | Evidence | Check |
|---|---|---|---|
| 1.1 | **Register returns** for `ibounds`, `pin_xy_int`, `pos_int`, `pin_ibox`, `ebbox_int`, `ebbox_rect`, `dwhole` / `dfloor_int` (and their callers): a `uint64_t` of four int16, or a `uint32_t` of two, in place of out-pointers. Inline the 1-call wrappers (`pcol_query`, 3 instructions a call) | ibounds: 6 saved registers + PR, 4 stores through pointers, called 107x a step | jtcost: stack stores fall; host gates unchanged |
| 1.2 | **Fast path before the context:** `collision_line_any_i` runs `line_summary` on the four ints in registers and builds `struct qctx` only for `line_scan`. Do the same in `collision_point_p` and the rect queries | 44 stack stores a call, 94 calls a step; line_summary answers 98 % | PLAY_STATS summary-vs-scan check (exists) |
| 1.3 | **Inline float / double compares and widening** in pnum.h: `deq`, `dne`, `dlt`, ... as integer tests on the bits (sign-magnitude order, ±0 equal). `fwiden` for float->double. Use them in the hot files first (pworld, pcol, pobj, pplayer, penemy). Host builds assert no NaN operand | 8,529 soft-float call sites; `__eqdf2` 1,287, `__extendsfdf2` 1,082 | host assert build over fullreg; identical records |
| 1.4 | **GBR hot block:** `struct hot` (≤ 1,020 bytes): PW's scalars, play_time, the grid / list heads and counters, pcol state, pointers to the big arrays. Set GBR in crt0 after .bss init. Check that no ISR (SDK isr / vblank) changes GBR. Access it through one macro, so the host build maps it to a plain global | literal-pool loads ≈ 9 % of instructions, 4,695 line misses a step; NOLIT bound −10 to −11 % | SH-2 0 warnings; checksums |

### Phase 2: data types and layout (layout-only or proven shadows; expected −15 to −25 %)

| # | Work | Exactness |
|---|---|---|
| 2.1 | **Integer shadows (PERF2 C):** the setters store int16 `ix`, `iy` and a whole flag next to the floats, plus the int16 box (`bl..bb`), updated only on a real change. The collision code and pos_int / pin_xy_int / bbkind / dwhole read the shadows when the flag is set | the int is read only when the flag proves it equals the float. Host assert mode compares the shadow with the float decode on every read |
| 2.2 | **struct pin split:** a hot record of 32 bytes (power of two):<br>- obj, alive / visible / flags bits, ext, en;<br>- ix, iy, the box, spr, mask;<br>- the creation number.<br>Cold parallel arrays hold the floats x / y / xprev / yprev / depth / img / ispd / scales / angle and id. A `_Static_assert` on every hot field's offset against the reach rules (§2.4). The PIN_RO setters keep working through accessors | layout-only; the PIN_CONST_CHECK build still compiles |
| 2.3 | **pin_ext reorder:** step-hot fields first (`held`, `col*`, `stuck`, `state`, `status`, `counter`, `facing`, the 12 alarms as int16), booleans packed into one int32 bitmask with accessor macros, then the binary64 velocities, then the cold fields. Same for pin_en | layout-only |
| 2.4 | **Compact integer collision kernel:** one leaf routine for the axis-line / point / rect existence tests over the int16 boxes and the grid summaries (B), with no float, under ~1 KB of code. It replaces the chain collision_line_any_i -> line_summary / pgrid_search -> ibounds -> pin_xy_int -> pos_int -> bbkind for whole-number queries. The float path stays for the rest | same answers as the scan: colprobe; a pcolxv-style random query check against the old functions |
| 2.5 | **Dense arrays for the per-step sweeps** that E did not cover: alarms (armed list), animation (non-terrain image_speed ≠ 0), snapshots | order kept (object order, then creation); the existing E argument |

### Phase 3: code footprint (expected −5 to −10 %, measured per item)

| # | Work | Note |
|---|---|---|
| 3.1 | **Unity build experiment:** one TU that `#include`s src/game's .c files (the toolchain has no LTO), compiled -O2. This enables cross-file inlining and `-fipa-ra`. Measure fetch misses and .text; keep it only if jtcost improves on both steps | static name clashes are renamed first, as a separate commit |
| 3.2 | **Hot / cold split of the big events:** pl_step (3.7 KB touched a step), characterStepEvent, item_step, jar_step, gameStepEvent. Move rare branches (damage, death, shop, pick-up, explosions) into `__attribute__((cold, noinline))` helpers; `__builtin_expect` on the common tests | codegen-only |
| 3.3 | **Not to repeat:** whole-program hot-first layout (+0.5 % on jtcps3), CCR.OD (worse), -Os everywhere (+2.4 %), the stack in cache RAM (−1 % net in the model, unverifiable in MAME), cache-set-aware code / data placement (jtcost's fully associative bound at 03b4638, fit: p4 301 511.2 -> 510.7 K, −0.1 %; p5 956 434.4 -> 406.1 K, −6.5 %: misses are mostly capacity), -falign-functions=16 (p4 +3.9 %, p5 +1.5 %; with -falign-loops / -falign-jumps=16 +2.4 / +1.5 %: fewer fetch misses, more literal-pool misses) | measured already |

### Phase 4: RAM structure (decision needed before work starts)

| # | Work | Decision |
|---|---|---|
| 4.1 | **Terrain out of `struct pin`:** a per-cell record (id, creation number, object: 8 bytes) for static terrain; a full slot is made only when GML code needs the instance (destroy, `with`, a returned id, a sprite / position change). Frees ~80-100 KB of main RAM and shortens every sweep | **User decision:** a large change to the instance model. It needs a written proof per query family that ids and with / collision order are unchanged (as A's proof). Start with a design note and a host prototype gated by gates.sh |
| 4.2 | R-tree (`rn`) compiled out of the grid build if only the generator and the exact build use it (HANDOFF §1.4) | the lead's item; check pcolgrid.h hooks |

### Phase 5: outside the mean step

| # | Work |
|---|---|
| 5.1 | **Level / room start** (2-21 M MAME clocks): profile with jtcost on the start step. Use DMAC 16-byte-unit copies and clears for inst_mem, the grids and the W -> PW load (0.77 clocks a byte, against 1.5 for CPU stores). Split the start over frames behind the transition |
| 5.2 | **p99 steps:** trace the worst non-start step per route (playsh2 "largest steps") and apply the same table |
| 5.3 | **Draw:**<br>- jtcost on `game_draw` (its 0.28 M allowance is unmeasured on jtcps3);<br>- draw1's queued item: the sprite-list DMA busy-wait in cps3v_vblank;<br>- entries built in main RAM and sent to sprite RAM by DMAC in place of ~550 CPU stores a frame. |

## 4. Order and batches

| Batch | Items | Expected modelled step (p4 301) |
|---|---|---|
| 0 | 0.1-0.4 | 608 K (batch 1, modelled; p5 956: 563 K) |
| 1 | 1.1, 1.2, 1.3 | ≈ 520-550 K |
| 2 | 1.4, 2.1 | ≈ 450-490 K |
| 3 | 2.2, 2.3, 2.5 | ≈ 400-440 K |
| 4 | 2.4, 3.1, 3.2 | ≈ 350-400 K |
| 5 | 4.x after the decision; 5.x | RAM and spikes |

These are estimates from the review's what-ifs. They overlap, so they are not additive. Each batch reports the real
figure in the table below, and the order changes if a batch underdelivers. **The goal is met** when the §1 goal holds
on the hardware run (0.3's method) for all playsh2 routes, not on the model alone.

## 5. Log

| Batch | Commit(s) | MAME p4 / p5 mean | jtcost p4 301 / p5 956 | jtcps3 measured (route mean of means) | Stack margin |
|---|---|---|---|---|---|
| main e264467 | | 198.9 K / 271.8 K | 926 K / 969 K | (grid 899ce76: 1.248 M) | 23 KB (failed) |
| batch 1 (merged, 63fe5c2..57dd34a) | A, D, E, F, gver 16-bit, rst 128 | 143.4 K / 173.5 K (SOFTFP=1; route mean of means 186.6 K) | 608 K / 563 K | JT run in MAME: PASS 23/23 | tests/game 64,560 B, JT 60,288 B |

Batch 1 gates on 57dd34a (2026-10-04): gates.sh all equal (19/19 both playhosts, P4 5/5, gen 9/9, colprobe 0, snd 19/19), ctall 48/48, EQUIV 72/72, FULLREG 640/640, playsh2 6,851/6,851 (grid, EXACT=1, SOFTFP=1), JT PASS 23/23, game_check p4_exit559 0 px at 30/150/300, SH-2 0 warnings.

| Batch | Commit(s) | MAME p4 / p5 mean (SOFTFP=1) | jtcost p4 301 / p5 956 (review) | jtcps3 measured | Stack margin |
|---|---|---|---|---|---|
| batch 2 (merged 4050e1b) | 1dd99c3 1.1, bcec614 1.2, 5fc9ae3 renames, 19859e4 pin 64 B, b90f6e9 pin field order, f7b5ebd 0.2 jtcost, 75a0852 0.4, df52042 3.1 unity (7 files), PCST counters + pg_overlap, 4e64196 bbkind, 14fb59a solid_[vh]line_any + query_e / flush inline | 130.2 K / 154.0 K (route mean of means 166.5 K, -10.8 %) | **527.2 K / 504.9 K** | (0.3 pending) | tests/game 78,892 B, JT 74,620 B |

Batch 2 gates on 14fb59a: gates.sh all equal, ctall 48/48, EQUIV 72/72 (output identical to batch 1), FULLREG 640/640,
playsh2 6,851/6,851 (grid, EXACT=1, SOFTFP=1), JT PASS 23/23, game_check p4_exit559 0 px at 30/150/300, SH-2 0
warnings. .text 1,007,504 (SOFTFP playsh2). Per commit (jtcost review, p4 / p5): batch 1 607.6 / 563.4; 1.1 + 1.2 +
pin 64 B + field order 579.9 / 546.3; unity 557.7 / 523.7 (13 files: 563.5 / 526.9, rejected); PCST + pg_overlap +
bbkind 543.6 / 512.1; solid lines + query_e / flush 527.2 / 504.9.

Next (batch 3, perf3-b2): c329716 PE(p) by a shift (exto in struct pin's pad): 510.6 / 491.0. Profile after it
(p4): play_step 48.8 K (alarm pass ~14 K, anim_one ~13.6 K inside it: 2.5), pgrid_search 46.7 K, bbkind_set 17.2 K,
solid_hline_any 14.4 K, mark_e + cupdate_at 24.7 K, pin_ibox 10.3 K (546 stack stores: 1.1).
The modelled goal (≤ 525 K) is met on both steps; the review constants read ~10 % under the measured ratio
(EQUIV 4.21 vs model 3.84), so the goal is not declared until 0.3's hardware run.

### 0.3 hardware calibration, batch 2 (14fb59a, JT build, MiSTer .62 jtcps3, 2026-10-04): goal NOT met

PASS 23/23. Step mean per route, jtcps3 clocks measured / MAME (SOFTFP playsh2 of the same code) / ratio:

| Route | jtcps3 | MAME | ratio | | Route | jtcps3 | MAME | ratio |
|---|---|---|---|---|---|---|---|---|
| p4_exit559 | 509,550 | 130,177 | 3.91 | | p5_buy | **734,759** | 207,738 | 3.54 |
| p4_hang_ladder | 522,737 | 136,982 | 3.82 | | p5_caveman | **915,828** | 212,777 | 4.30 |
| p4_items | 522,840 | 138,260 | 3.78 | | p5_cavestun | **697,784** | 169,169 | 4.12 |
| p4_spikes | 326,209 | 76,634 | 4.26 | | p5_giant | **798,295** | 197,011 | 4.05 |
| p4_push_rope | 497,343 | 123,238 | 4.04 | | p5_idol | **713,765** | 167,043 | 4.27 |
| p1_walk | **615,170** | 156,087 | 3.94 | | p5_l3spider | **574,670** | 133,444 | 4.31 |
| p4_bomb_drop | **696,976** | 177,513 | 3.93 | | p5_l4 | **717,564** | 176,234 | 4.07 |
| p4_bomb_throw | 487,024 | 113,724 | 4.28 | | p5_shop | **724,057** | 168,776 | 4.29 |
| | | | | | p5_snakes | **667,815** | 154,041 | 4.34 |
| | | | | | p5_spider | **598,286** | 159,235 | 3.76 |

Mean of means 628,926 jtcps3 (grid 899ce76: 1.248 M). 12 of 18 routes are over 0.525 M; p5_caveman needs -43 %.
Model check (model / MAME clocks on the traced step against the measured route ratio): review constants 3.16 (p4) and
2.89 (p5) against 3.91 and 4.34 measured: 19 % and 33 % low. jtmodel's fit constants 3.84 and 3.44: 2 % and 21 % low.
**From here jtcost's metric is the fit constant set** (still low on enemy routes: refit with this table pending), and
the MAME proxy is about 130 K MAME clocks per route mean (0.525 M / ~4.05).

### Batch 3 (merged 934269f, gated on 80d620b)

Commits: c329716 PE(p) by a shift (exto), ca3c4f3 pgrid_search locals, 2d1fa9e pin_ibox inline, 9d5f386 fzero / funit
and eview's integer path, c65180f gcmp_dd = gcmp_z(a - b), PIN_IDX (unsigned slot index; the 64-byte pin had made
p - PW.in a libgcc __ashiftrt_r4_6 call), distance_to_instance_p's one-axis integer path; tools: jtcost call counts,
JTC_PCHIST, libgcc callers, literal-miss classes, fit constants by default; tools/sfsites.py (8,132 soft-float call
sites in src/game, 2,807 of them through pnum.h).

| | p4_exit559 301 | p5_snakes 956 | p5_caveman 150 |
|---|---|---|---|
| jtcost fit, batch 2 (14fb59a) | 639.9 K | 601.4 K | 860.6 K |
| jtcost fit, batch 3 | 587.0 K | 547.3 K | 821.8 K |

MAME SOFTFP route mean of means 166.5 K -> 154.3 K (batch 1: 186.6 K). Gates: gates.sh all equal, ctall 48/48, EQUIV
72/72 (identical output), FULLREG 640/640, playsh2 6,851/6,851 (grid, EXACT=1, SOFTFP=1), JT PASS 23/23, game_check 0
px, stack tests/game 78,892 B / JT 74,620 B, 0 warnings.

Measured and not kept: PTOD / D* through inline fwiden everywhere (code growth: +0.5 to +1 %), a 13-file unity TU,
-mrelax (does not assemble the unity TU: 2-byte jump-table offsets), an image_speed 0 fast path in anim_one (+0.2 /
+0.9 / -0.4 %: under jtcost's layout noise of about 1 %), a per-step memo of isCollision* (8-11 % repeats).
Literal-pool line misses by what they hold (p4 / p5): RAM addresses 1,013 / 698, constants 703 / 724, function
addresses 414 / 301: a GBR block (1.4) can remove at most the first (38 K / 26 K fit, 6.5 % / 4.8 %).

Batch 3 on jtcps3 (.62, JT build of 80d620b, PASS 23/23): route mean of means **577,160** (batch 2: 628,926, -8.2 %);
over 0.525 M: 11 of 18 (p1_walk 576 K, bomb_drop 652 K, buy 673 K, caveman 829 K, cavestun 631 K, giant 721 K, idol
650 K, l4 659 K, shop 660 K, snakes 605 K, spider 549 K). Under: exit559 477 K, hang_ladder 491 K, items 491 K, spikes
302 K, push_rope 459 K, bomb_throw 449 K, l3spider 516 K.

### Batch 4 (merged 1d83951, gated on a64f299)

69cccbd Draw dispatch by a per-object claimant (PLAY_DCHECK: P5 19/19, c_ 48/48), c8cf3ca PW through GBR (1.4 step 1),
a64f299 the GBR rule (never &PW.field: pcol.c's search state behind GBR with s_r passed by address gave 237 SH-2
checksums off; its flags behind GBR measured slower; neither kept). jtcost fit p4 / p5 / caveman: 565.3 / 523.2 / 790.5 K
(batch 3: 587.0 / 547.3 / 821.8). MAME SOFTFP mean of means 154.3 -> 149.1 K. Gates as batch 3 plus gametime_check (the
game program with GBR set by src/main/main.c): pair mean 155.6 K / 163.0 K MAME (p4_push_rope / p5_snakes), draw mean
40.2 K / 34.9 K MAME. Stack 78,432 / 74,160 B. Measured and dropped: a has_col cache, pcinst_of's whole-box path with
an inline int -> float (i2f, exact for |v| < 2^24): +0.6 to +1.2 %.

Budget note (inferred, to measure): the 0.525 M step goal assumes 0.28 M for the draw in the 0.84 M pair (PERF2's
26daf63-era figure). The current draw is 35-40 K MAME, about 0.11-0.12 M jtcps3 at the 3.0x draw ratio measured then;
the pair on jtcps3 (tests/gametime on the MiSTer) decides.

Batch 4 on jtcps3 (.62, JT build of a64f299, PASS 23/23): route mean of means **565,532** (-2.0 %; modelled -4 %);
11 of 18 over 0.525 M (buy 657 K, caveman 819 K, giant 714 K, spider 539 K, snakes 593 K, ...).

**The frame budget measured on jtcps3** (tests/gametime, the game program with GBR, batch 4, .62; clocks per 2-frame
pair, budget 838,940):

| | attract | game 1 (p4_push_rope) | game 2 (p5_snakes) |
|---|---|---|---|
| step mean / max | 165 K / 25.9 M | 452.9 K / 5.26 M | 576.5 K / 827 K |
| draw mean / max | 181 K / 1.62 M | 111.8 K / 1.88 M | 99.0 K / 135 K |
| pair mean / max | 354 K / 28.3 M | 573.6 K / 8.17 M | 681.8 K / 942 K |
| pairs over budget | 5 of 1,799 (room starts) | 1 of 375 (the level start) | **18 of 241** |

So the draw is about 0.1 M, not the 0.28 M the 0.525 M step goal assumed: a step fits a pair up to about 0.72 M. The
binding constraint is the spike steps of the enemy routes (p5_snakes: 7.5 % of pairs drop a frame), not the mean.
Load-use stall (from spelunky-cps3-a6, low priority): jtcps3 stalls 1 cycle on a load used by the next instruction
(jtframe SH_core.sv:191); jtcost does not model it; 11,936 pairs on p4_exit559 301, 16,145 on p5_caveman 150 (~2 %).

### Batches 5 and 6

- Batch 5 (merged abebbe3): fne setters, flying-bat trig (psincos_cr: pcos_cr / psin_cr's bits for all 1,135,869,954
  float directions; p5_buy 150 857.5 -> 583.5 K fit). jtcps3 (.62): route mean of means **553.0 K** (-2.2 %), buy
  657 -> 550 K, spider 539 -> 496 K (now under 0.525 M); 10 of 18 over. Frame budget, game 2 (p5_snakes): pair mean
  674.7 K, **14 of 241 pairs over** (was 18), max 931 K.
- Batch 6 (merged 95c829e): pw_release's compaction from the oldest removed slot (p5_snakes record 203, the batch's
  spike step: pw_release 43.4 -> 24.0 K), rset_i inline, jar-search (a6: JAR_FAST before the enemy / damsel
  collision_rectangle in the grid build; caveman 150 -5.6 %, cavestun 150 -8.6 % fit), attr-caller (a6: ATTR's
  collision searches by calling object). MAME SOFTFP mean of means 145.8 -> 140.1 K (snakes -9.1 %, cavestun
  -7.5 %, caveman -6.2 %); gametime (MAME) p5_snakes pair max 226.5 -> 204.2 K. Gates as before, all equal.
  Frame budget on jtcps3 (.62), game 2 (p5_snakes): step mean / max 489.4 K / 754.2 K, draw 98.0 K / 132.8 K, pair
  mean 593.8 K (was 674.7 K), max 847.9 K, **2 of 241 pairs over** (was 14; the worst 9 K over). Game 1
  (p4_push_rope): step mean 444.7 K, pairs over 1 of 375 (the level start, as before).
  Route timing on jtcps3 (.62, JT build of 0cb7be6, PASS 23/23): route mean of means **517.1 K** (-6.5 %), 8 of 18
  over 0.525 M: caveman 739.3 K, giant 665.1 K, bomb_drop 623.4 K, idol 579.3 K, l4 569.4 K, shop 569.3 K, walk
  553.4 K, cavestun 547.5 K; snakes 497.7 K, spider 450.5 K.
- Batch 7 (perf3-b2 0a73215): view tests on the float bits (42906e2, 0a73215: PLTI / PGTI, gcmp_fi out of line; all
  2^32 floats x 20 v and 2^21 ulps around each v in [-4096, 8192) equal the double compare), game-search (a6,
  8b65712: a resting oMoveableSolid skips its place_meeting; giant 100 -7.4 %, l4 150 -9.7 % fit). jtcost (fit) at
  0a73215: p5_snakes 203 666.9 -> 648.7 K, p5_caveman 150 735.7 -> 662.5 K. gametime (MAME) p5_snakes pair max
  204.2 -> 199.8 K. Gates: gates.sh (p5_regress 19/19 both builds, colprobe, snd), ctall 48/48, EQUIV 72/72,
  FULLREG 640/640, playsh2 grid / EXACT / SOFTFP 6,851 equal, game_check, gametime, JT: all pass.
  On jtcps3 (.62, the gated batch-7 builds): frame budget game 2 (p5_snakes) step mean / max 483.8 K / 750.0 K, pair
  mean 588.2 K, max 844.8 K, **1 of 241 pairs over** (5.8 K over); game 1 pairs over 1 of 375 (the level start).
  JT PASS 23/23, route mean of means **486.9 K** (-5.8 %), 4 of 18 over 0.525 M: caveman 695.6 K, giant 613.7 K,
  bomb_drop 595.6 K, idol 550.6 K (walk 524.0 K, cavestun 524.8 K just under).
- Batch 8 (merged 54d3a26): BB_INTS, box-only instances at whole scales on overlap_at's integer path (oArrowTrapTest:
  443-598 float PreciseCollision runs a route on caveman / cavestun / giant). p5_caveman 150 662.5 -> 621.3 K fit;
  p4_exit559 301 +1.3 % (layout). Gates all pass (as batch 7).
- Batch 9 (perf3-b2): dist_newton_i (dcf6b02: the whole-box distance's Newton from isqrt + 0.5, every n <= 2^24
  checked; caveman 200 684.8 -> 665.3 K), spr_dim (2fa6c8f: sprite_width / height at whole scales, caveman 200 665.3
  -> 643.9 K, p5_snakes 956 -1.7 %), pw_release on a step that removed nothing (0e479f1: the 24 K batch release
  moves off p5_snakes record 203, 651.7 -> 635.8 K; record 208 with it 519.8 K), gametime's PR AT (f535e23: MAME
  puts p5_snakes' largest pairs at 200, 203, 202 - steps 201-204, the explosion - not the settling record 3).
  On jtcps3 (.62, batch-9 builds, main 630e420): frame budget game 2 (p5_snakes) step mean / max 474.8 K / 730.2 K,
  pair mean 581.1 K, max 831.3 K at pair 203 (the explosion), **0 of 241 pairs over** (7.7 K under the budget);
  game 1: 1 of 375 over, pair 1 (the level start).
- Batch 10 (perf3-b2): pcol_handle's searcher rectangle from its tree entry when not stale (83d07c9: caveman 200
  650.3 -> 635.6 K, p4 301 / p5 956 -0.9 %; PLAY_STATS checks it against ebbox_rect), jar3 (a6, 918e69e: jar_step's
  constant compares on the bits, src/game/pcmpc.h; p5_snakes record 3 711.8 -> 700.9 K).
  On jtcps3 (.62, batch-9 JT): PASS 23/23, route mean of means **465.6 K** (-4.4 %), 4 of 18 over 0.525 M: caveman
  638.5 K, giant 563.7 K, idol 547.5 K, bomb_drop 540.6 K.
- Batch 11 (merged 2c9237e): moveTo's non-characters test the line alone in place of getIdCollisionRight / Left
  (de8372c: caveman 200 638.9 -> 608.0 K), step1 (a6: evobj_init at play_level_start, query_dyn from the family's
  lists). On jtcps3 (.62): frame budget game 2 (p5_snakes) **0 of 241 over**, pair max 806.3 K at pair 203 (32.6 K
  under), step mean / max 463.9 / 704.0 K; JT PASS 23/23, route mean of means **452.8 K** (-2.7 %), 3 of 18 over
  0.525 M: caveman 614.2 K, giant 540.1 K, idol 536.8 K (bomb_drop 511.4 K now under).
- Batch 12 (merged 2b9c2c1): moveTo's pixel walks without the per-pixel setter in the grid build (b8b010f, PLAY_WALK:
  caveman 200 608.0 -> 559.0 K, p4 301 -2.4 %), pgrid_search with one key per hit (d3707b5: p5_l4 2, the level's first
  step, 4662.4 -> 4421.3 K), the solid grid built at the end of play_level_start (a6, b078556: p5_l4 2 -15 %).
  On jtcps3 (.62): JT PASS 23/23, route mean of means **435.5 K** (-3.8 %), **1 of 18 over 0.525 M: caveman 579.9 K**
  (giant 515.5 K, idol 522.6 K, bomb_drop 489.9 K under); frame budget game 2 (p5_snakes) 0 of 241 over, pair max
  801.6 K at pair 0 (the first after the level start), step mean / max 457.6 / 705.2 K.
- Batch 13 (perf3-b2): moveTo's other walks step the fields alone (46f37fa: characters and fractional movers, one
  setter per axis at the end; p4_exit559 301 487.4 -> 421.4 K, p5_snakes 203 571.1 -> 547.4 K), set_dyn /
  query_dyn_grid by family lists (a6, 7f268eb: p5_l4 2 -4.5 %).
  On jtcps3 (.62): JT PASS 23/23, route mean of means **421.7 K** (-3.2 %), 1 of 18 over: **caveman 552.1 K**; frame
  budget game 2 0 of 241 over, pair max 802.2 K, step mean 451.2 K; game 1 step mean 386.1 K.
- Batch 14 (merged 53f71dd): a character's fall skips the per-pixel platform tests when one prec-0 oPlatform box query
  over the fall finds nothing, and a character's sideways step tests the line before getIdCollision's search (e5e50be:
  p5_caveman 178 737.5 -> 680.0 K, p4 301 -3.1 %; PLAY_STATS checks the skipped tests), sincos_f's 8-entry cache
  (a6, a54ed43: p5_giant 28 745.1 -> 705.7 K).
  On jtcps3 (.62): JT PASS 23/23, route mean of means **419.4 K**, 1 of 18 over: caveman 545.5 K; frame budget game 2
  0 of 241 over, pair max 787.3 K, step mean 443.5 K; game 1 step mean 377.7 K.
- Batch 15 (merged 32195fe): pin_add / ext_alloc by words (c67b360: p5_giant 28 714.6 -> 700.2 K), item (a6: prandom
  from u's bits, e1c0966; item_step's colBot settle on the raw field, d4396ee: caveman 178 737.4 -> 718.2 K).
  On jtcps3 (.62): JT PASS 23/23, route mean of means 417.5 K, 1 of 18 over: caveman 542.3 K; frame budget game 2 0
  of 241 over, pair max 799.4 K. A whole-route sampled profile of p5_caveman (PROF=64 PROF_WRAP=1 SOFTFP=1): soft-float
  29.5 % of the samples (isCollisionSolid 3.6 %, pl_step 3.2 %, play_step 3.1 %, snake_step 2.1 %, pen_motion 1.8 %);
  self time play_step 12.6 %, pgrid_search 9.1 %, collision_point_p 5.0 %, snapshot 3.0 %.
- Batch 16 (merged a927404): oEnemySight's speed at dir 0 / 180 computed once (a6, 03d7199: p5_idol 109 650.2 ->
  544.6 K; MAME route means l4 -6.1 %, idol -1.3 %, caveman flat).
- Batch 17 (merged 353e2ae): the oSolid "any" queries from the solid grid's cell summary: solid_rect_any (f9cabd4) and
  collision_point_any (353e2ae, the GML's `collision_point(.., oSolid, ..) != noone` tests: pplayer.c 27, pobj.c 11,
  the CP wrappers); MAME SOFTFP route means (steps 2+): caveman -5.1 %, cavestun -4.7 %, giant -5.4 %, idol -7.0 %,
  l4 -4.1 %, snakes -7.0 %; gametime (MAME) p5_snakes pair mean 141.4 -> 134.1 K.
  On jtcps3 (.62): JT PASS 23/23, route mean of means **402.4 K** (-3.6 % from batch 15), **0 of 18 over 0.525 M**
  (the mean goal on the hardware routes; caveman 517.4 K, giant 472.6 K, idol 456.9 K, bomb_drop 451.1 K); frame budget game 2
  (p5_snakes) 0 of 241 over, pair max 748.6 K at pair 0 (90.3 K under), step mean / max 426.6 / 651.8 K; game 1
  (p4_push_rope) 1 of 375 over (pair 1, the level's first steps), step mean 376.5 K.
- Batch 18 (merged a50573a): collision_rect_any (989b93c: the rectangle tests; route means about -1 %).
  On jtcps3, batch 16 (sight): JT PASS 23/23, caveman 545.1 K (as batch 15), l4 450.5 K.
  On jtcps3, batch 18: JT PASS 23/23, route mean of means 400.7 K, caveman 511.8 K; frame budget game 2 0 of 241
  over, pair max 731.3 K.
- Batch 19 (merged b6faebf, gated on e68ee6a): isCollisionSolid's whole path by solid_rect_any (a6, e3e27e3: caveman
  178 619.0 -> 618.7 K), pen_motion's sight step without the double sums (a6, ee44e84: p5_l4 151 -0.65 %;
  tests/sightmv 2.3 G cases). On jtcps3: JT PASS 23/23, route mean of means **397.4 K**, caveman 509.0 K; frame
  budget game 2 0 of 241 over, pair max 727.5 K, step mean 415.7 K.
- Batch 20 (merged 7f5e32e): precise_line along an axis-parallel line (a6, 5c8b530: p5_idol 252, the boulder, 1,205.6
  -> 1,107.0 K), pcinst_of's 2-entry cache (a6, 4fd8eeb: idol 252 -> 1,061.2 K; MAME max step 321.3 -> 262.1 K), the
  liquid index (7f5e32e: oLava / oWater / oWaterSwim point misses from per-cell counts; p5_reg_l14s16's MAME SOFTFP
  step mean 269.6 -> 172.1 K, steps over at x4.2 215 -> 49; the other routes +0.15 to +0.34 %).
  On jtcps3: JT PASS 23/23, route mean of means **405.0 K (+1.9 % on batch 19; every route +0.9 to +2.9 %)**,
  caveman 518.3 K; the generation jobs G1-G5 -1.5 to -6.1 %, which run none of the changed code; frame budget game 2
  0 of 241 over, pair max 723.0 K, step mean 414.6 K (-0.3 %). Read as data layout: MAME instructions were within
  +0.34 %, and jtcost p5_caveman 178 with the liquid index had instructions +0.27 %, D-misses in RAM +3.0 % (the
  index's arrays, 11.5 KB of .bss, move the play state's arrays against the 4 KB cache's sets). Caveman's margin to
  0.525 M is 1.3 %.
- Measured and not kept (a6): PERF3 3.2's hot / cold split of characterStepEvent (4 cold helpers): caveman 178
  +0.01 %, snakes 203 +0.04 %, exit559 301 -0.6 % (its misses are code that runs every step).
- Measured and not kept (a6): a pw_onz bitmap of non-empty object lists for snapshot / the alarm pass: +0.3 / +0.8 %
  on caveman 178 (no variable shift on the SH-2; pw_ohead mostly cached).
- Measured and dropped: pcol_handle skipping a searcher no live object can pair with (can_pair): terrain blocks can
  always pair (live objects target oSolid), so it only added the test (p5_caveman 2 3.19 -> 3.32 M).
- Measured and dropped: a 4-entry cache of rotated boxes in bbox_dbl (p5_idol 252: pin_bbox <- line_hit 39.7 ->
  33.9 K only; the boulder's line tests are precise_line's float walk).
- Measured and dropped: distance_to_instance_p's double path from isqrt(floor(d)) + 1 (1.9 G sampled d equal): the
  measured steps never take it (whole boxes: dist_newton_i).
- Measured and dropped: moveTo's fractional walks on explicit double lines (detritus moveTo 64.1 -> 67.2 K: the
  setter calls, not the lines, were the cost; 46f37fa keeps the original calls).
- Measured and dropped: pq_init's (double)(float)v and floor on the bits (dfl_floor, exact over 16.6 G doubles):
  pq_init 12.6 -> 6.9 K on caveman 200 but the step 556.7 -> 565.2 K, instructions flat (layout).
- Measured and dropped: pgrid_search with a float search rectangle comparing whole entries by the rectangle's integer
  bounds (pg_kside: exact for every float below 2^14) and float entries by keys inline. p4_bomb_drop record 202 (the
  bomb's explosion, 1.50 M fit: debris searching in pcol_handle) 1497.7 -> 1447.7 K with the compares inline in the
  scan, but p4 301 / p5 956 +1.1 / +1.9 % (I-misses: the scan grew); out of line, 1482.2 K and P4 / P5 +1.3 / +0.9 %
  with fewer instructions (layout). The explosion's steps stay beyond a frame pair either way.
- Measured and not kept (a6, jar-fp f8916d8): the jars' speed test on the bits and the hit rectangle on whole
  corners: 80 soft-float calls gone on p5_snakes 203 but jar_step's fetch misses up, 666.9 -> 667.3 K.
- Measured and dropped: anim_one's image compares as integer bit tests (img_ge / img_lt0, equal to the float
  compares over all 2^32 / 300 M pairs): p4 301 554.0 -> 552.4 K, p5 956 445.9 -> 448.0 K fit, stack stores +70.
- Measured and dropped: a call-free pg_collect for pgrid_search (-1.4 / -1.0 / +0.6 / +0.2 %: noise, +3.5 KB).
- jtcost hangs when asked for two consecutive steps of one route: trace one step per run.
