# PERF3: fitting the play step on jtcps3

Status 2026-10-04: plan. Builds on docs/REVIEW-SH2.md (the cost model and findings P1-P7) and docs/HANDOFF.md (branch
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
| 3.3 | **Not to repeat:** whole-program hot-first layout (+0.5 % on jtcps3), CCR.OD (worse), -Os everywhere (+2.4 %), the stack in cache RAM (−1 % net in the model, unverifiable in MAME) | measured already |

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
