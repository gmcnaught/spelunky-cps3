# PERF3: fitting the play step on jtcps3

Status 2026-10-05: **batch 24 on branch worktree-agent-acd14d6b30ea5482d** (on main a2fed51, not merged; section 5,
batch 24). Lush on jtcps3 (.62, route step means with step 1): p5_lush_l5s11 823.0 -> 675.4 K, l5s37 715.0 -> 628.8 K,
l6s23 735.7 -> 647.4 K: still 20-29 % over 0.525 M. The jtcps3 / MAME ratio on these routes rose from ~4.05 to ~4.5
as instructions went (MAME SOFTFP means 146.8 / 138.1 / 141.6 K): the 125 K MAME proxy understates what is left;
misses are now about three quarters of a lush step (jtcost). **Open:** lush (the per-instance passes' pin-line misses,
the player 16 % and the enemies 17 % of a step in jtcost, the remaining jungle / treasure Steps); p5_lush_l5s37's grid
build fails the EQUIV route check against the exact build (section 5, batch 24: collision event order, present since
before batch 23); p99 (non-lush routes); Phase 5 (level start, draw). Section 5 lists what was measured and not kept. Builds on docs/REVIEW-SH2.md (the cost model and findings P1-P7) and docs/HANDOFF.md (branch
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

**Literal pools: measured, not kept (2026-10-06, main 16ab24f).** Literal-pool line misses are 36-40 % of the data-line
misses and 13.7-15.6 % of the modelled step on every route (jtcost fit; 9 steps over 6 routes: p4_exit559, p5_snakes,
lush l5s11 / l6s23, c_jungle_monkey / mantrap, c_swamp_drain / swim). What they hold (p4_exit559 301): RAM addresses 582
lines, constants 541 (most: the float / double masks 0x7FFFFFFF, 0x007FFFFF, 0x80000000, 0x000FFFFF, 0x00800000,
0x7F800000 from pnum.h's inline helpers and soft-float, and the range check's 29999), function addresses 284. jtcost.py's
what-ifs (NOLIT_KIND / NOLIT_SYMS / NOLIT_FREE / NOLIT_VALS, 53c449c), fit:

| What-if | p4_exit559 301 (360.8 K) | p5_snakes 956 (338.3 K) |
|---|---|---|
| every literal load removed (bound) | −17.4 % | −16.2 % |
| RAM-address literals removed (bound of any GBR block) | −4.2 % | −3.6 % |
| constant literals removed | −5.0 % | −5.6 % |
| function-address literals removed | −2.5 % | −2.2 % |
| ~30 hot scalars into the GBR block (gmode, dhead, quiet_any, play_cur_obj, ...) | −0.40 % | −0.27 % |
| + hot arrays and PL / PG / PGAME as pointers in the block | −0.77 % | −0.78 % |
| the 8 masks made in registers (+3 instructions each) | +0.16 % | +0.26 % |
| all three | −0.66 % | −0.53 % |
| array pointers as block loads, scalars counted twice (first model) | +1.6 % | +1.4 % |

A missed pool line sits next to code that is itself cold, and it holds other literals that are still loaded: taking one
class of literal out removes few lines (the masks: 1,513 loads a step gone, 79 lines). The literal misses are a share
of the code footprint, reached by the NC link and by shorter paths, not by the literals. Not proposed:
-fsection-anchors (not supported on SH: gcc warns, output unchanged), -mrelax (bsr reaches +-4 KB only), grouping a
file's statics into one struct (gcc reloads the address from the pool after every call: pplayer.c holds 169 pool copies
of _PL in 13 functions).

### Phase 4: RAM structure (decision needed before work starts)

| # | Work | Decision |
|---|---|---|
| 4.1 | **Terrain out of `struct pin`:** a per-cell record (id, creation number, object: 8 bytes) for static terrain; a full slot is made only when GML code needs the instance (destroy, `with`, a returned id, a sprite / position change). Frees ~80-100 KB of main RAM and shortens every sweep | **User decision:** a large change to the instance model. It needs a written proof per query family that ids and with / collision order are unchanged (as A's proof). Start with a design note and a host prototype gated by gates.sh |
| 4.2 | R-tree (`rn`) compiled out of the grid build if only the generator and the exact build use it (HANDOFF §1.4) | the lead's item; check pcolgrid.h hooks |

### Phase 5: outside the mean step

| # | Work |
|---|---|
| 5.1 | **Level / room start** (2-21 M MAME clocks): profile with jtcost on the start step. Use DMAC 16-byte-unit copies and clears for inst_mem, the grids and the W -> PW load (0.77 clocks a byte, against 1.5 for CPU stores). **Wrong rate:** (correction 2026-10-06: the 0.77 clocks a byte from cps3-testgame's ttest is 4x too low. ttest.c:329 sets TCR0 = bytes / 16 in 16-byte mode, but TCR counts longwords (jtframe sh7604 DMAC.sv: one count per longword write beat; MAME sh7604.cpp: count &= ~3, -4 a 16-byte unit), so its 4 KB row moved 1 KB: about 3.1 clocks a byte, against 1.5 for CPU stores; docs/DRAW.md section 8); this idea pays only for transfers that overlap CPU work from the cache. Split the start over frames behind the transition |
| 5.2 | **p99 steps:** trace the worst non-start step per route (playsh2 "largest steps") and apply the same table |
| 5.3 | **Draw:**<br>- jtcost on `game_draw` (its 0.28 M allowance is unmeasured on jtcps3);<br>- draw1's queued item: the sprite-list DMA busy-wait in cps3v_vblank;<br>- entries built in main RAM and sent to sprite RAM by DMAC in place of ~550 CPU stores a frame (branch sprdma, docs/DRAW.md section 8; the DMAC is slower per byte than CPU stores, see 5.1). |

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
- Batch 21 (merged 1fa4ffc): pen_moving_solids skips a solid that does not step (a86bc73: p5_reg_l14s16 -8.4 %);
  ebbox at scale +-1 without fmaf and s_overlap_slow on int bounds (a6, be71087 / 107c21a: bomb_drop 202 -5.8 %,
  snakes 203 / caveman 178 / exit559 301 -2.0 to -2.4 % fit); the lava rectangle tests from the index (7600fc0:
  l14s16 -3.9 %); the static-family index (1fa4ffc: oLadder / oLadderTop / oSpikes / oWeb join the liquids, and
  collision_point_p / collision_rect_p / collision_rect_i answer its misses: MAME SOFTFP snakes -4.1 %, p1_walk -3.2 %,
  bomb_drop -2.5 %, caveman / idol -2.3 %, spider / l2s10 -2.0 %). MAME SOFTFP steps over 838,940 at x4.2 (gates):
  l14s16 34, l2s10 / l3s10 23, bomb_drop 17, l4s10 16, bomb_throw 15, spider 13, buy 12, idol / p1_walk 8.
- Measured and not kept (a6): moveTo's final pin_setx + pin_sety as one pin_setxy (one pw_changed): l2s10 183 -0.3 %,
  snakes 203 -0.8 %, caveman 178 +0.15 %, bomb_drop 202 +1.0 % (fit). isCollisionLeft / Right / Top / Bottom's line
  ends for fractional positions in fixed point: the path almost never runs (debris is whole after moveTo's pixel
  steps). isCollisionTop's cost on detritus is the pcol flush in solid_hline_any (the movers' dirty entries), not the
  double path. Not kept (a6): treasure_step's colBot settle as raw writes (the path does not run on bomb_drop 202).
- Batch 22 (merged a6b3cef, the batches since 20 run as one on jtcps3): batch 21, the lava tests at an instance's
  whole x, y (b7d61b2: p5_reg_l14s16 -5.0 % MAME SOFTFP, 25 steps over at x4.2) and the index's arrays in sprite RAM
  in the JT build (a6b3cef, tests/playsh2/jtcold.ld). On jtcps3 (.62): JT PASS 23/23, route mean of means **391.4 K**
  (batch 20: 405.0 K, batch 19: 397.4 K), **caveman 503.5 K** (the largest; 4.1 % under 0.525 M); frame budget game 2
  (p5_snakes) 0 of 241 over, pair max 710.4 K (128.5 K under), step mean 398.3 K. Batch 21's code with the same
  placement (one JT run, no gates, 08:20): 390.2 K, caveman 501.3 K, so b7d61b2 is +0.3 % here (within run noise).
  The route maxima (the first step after the level start) rose 0.2-0.4 M: the index places every indexed entry at
  the first query (MAME step 1 +7 to +19 K x4.2), and the level start is Phase 5.1.
- Measured and not kept (a6): anim_one's step through a direct-mapped cache keyed by img / ispd / sp / fr bits (16 or 64
  entries; image_speed on p5_reg_l14s16 is 0.4 / 0.5 / 0.8 / 0.3 / 0.25, sp always 1, so a k / 2^n fixed-point path
  covers under 40 %): MAME SOFTFP l14s16 -1.5 %, jtcost fit record 22 +0.5 / +0.7 % (the table's D-misses). A
  hardware A / B would settle whether jtcost over-penalizes small hot tables.
- Measured: line_hit's double path (pin_bbox, the float casts) runs 11 times over p5_reg_l14s16 (one BB_DBL object);
  not worth a path.
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
- Batch 23 (branch worktree-agent-adf10525aca9ad59f, rebased on main 3249c51; 17 commits from 7976459): p99 spikes,
  then lush (the user's priority: slowdown on levels 5-6 at the cabinet).
  - Attribution (jtcost fit, JTC_CALLERS): p5_spider 202 / p5_buy 198 were one or two dd_sincos fallbacks of
    bat_fly (78 % / 67 % soft-float); explosion steps (bomb_drop 202, l2s10 183) are pcol_handle's grid searches for
    rubble (25 of 44 searches, 0 pairs), detritus moveTo / isCollision*, cupdate_at (flat beyond that);
    p5_reg_l14s16 (temple, dark) the light search's instance_nearest; p5_idol 252 detritus line tests against the
    rotated boulder (precise_line); lush: oSpearTrap's 6 instance_nearest a trap a step (p5_lush_l5s11 201: 72
    calls, 43 % of 1,185 K), oVampire's cr_trig double-double sin / cos (c_swamp_vampire 101: 315 K of 1,462 K),
    swamp fish collision_point(.., oWater) creation-order scans (c_swamp_drain 101: 354 K of 1,071 K), oLeaves'
    tree / leaves points (c_swamp_deadfish 101: 64 K). Frogs / monkeys / man traps (the user's suspects): 6-8 % each
    on c_jungle_monkey 101 (monkey_step 5 calls 58 K, frog_step 6 calls 50 K of 741 K), not the main cost.
  - Commits: psincos_cr's second Ziv test (sincos_r2; tests/sincos: all 1,135,869,954 float directions equal, the
    second test keeps every first-test failure); pcol_handle's can_pair skip and direct_pairs (a short-list
    searcher's pairs from the pairable instances); instance_nearest_p's integer bounds, list walk and per-family
    floor cache (nc); fam_begin's pruned family walk; speartrap_step's nearest once a step; vampire_fly via
    psincos_cr; collision_point_any's static-family existence path (xpoint_any, cell-filtered); oTree / oLeaves in
    the static-family index; RAM (short rv lists, family walks on pcol's object tree; tests/game 34,624 B stack).
  - MAME SOFTFP (main 3249c51 -> batch 23; route means steps 2+, steps over 838,940 at x4.2): p5_lush_l5s11 674.1 ->
    201.8 K (p99 695 -> 223 K), l6s23 485.3 -> 178.0 K (p99 516 -> 204 K), l5s37 374.0 -> 171.5 K (p99 392 -> 191 K);
    c_swamp_* / c_jungle_* at cd3bc01 -> before the RAM commits: vampire 470.8 -> 241.6 K, deadfish 429.3 -> 235.8 K,
    drain 423.6 -> 332.6 K, frog 271.7 -> 171.1 K, monkey 259.5 -> 170.0 K; non-lush routes all -0.3 to -7 % (spider
    -7.2 %, l14s16 -11.0 %, bomb_drop -4.4 %), over 197 -> 117 (spider 12 -> 0, buy 11 -> 0, bomb_throw 14 -> 3,
    bomb_drop 16 -> 8, p1_walk 7 -> 2, l14s16 39 -> 22); all-route mean 182.8 -> 116.2 K, p99 686 -> 219 K.
  - jtcost fit: p5_lush_l5s11 201 1,185 -> 748 K; c_swamp_deadfish 101 1,070 -> 879 K; c_swamp_drain 101 1,071 ->
    947 K (before the cell filter); p5_reg_l2s10 183 1,128 -> 1,029 K (can_pair).
  - Gates on 2682d0e: make check (host builds, constcheck, P5 21/21 + 3 lush routes without HD trace, EQUIV 85/85,
    snd 19/19 0 differ), ctall 59/59, playsh2 9,701/9,701 (grid) and 9,701/9,701 (SOFTFP), game_check p4_exit559 0 px
    at 30/150/300 (the tests/game link: 34,624 B of stack). The lush routes: playhost and playhost_grid output
    byte-identical to main's builds.
  - Not kept: none dropped after measurement; p5_reg_l9s5 (main's new route) rose 3.8 % with direct_pairs until its
    candidate count bound (2682d0e: 143.5 -> 142.6 K).
- Batch 24 (branch worktree-agent-acd14d6b30ea5482d on main a2fed51; gated on cf7d06f): lush, round 2.
  - The 359 K / 674 K question: not a regression and not a trajectory change. playhost and playhost_grid output for
    p5_lush_l5s11 is byte-identical at cd3bc01 and 3249c51 (records and PW / PCOL stats); MAME SOFTFP (the same
    harness, GAME_REV) gives 676,740 at cd3bc01, 676,742 at 3249c51 and 361,191 at 5213f99 (3249c51 plus batch 23's
    first five commits): the 359 K was a build that already had them.
  - tests/playsh2 ran every route with global.downToRun 0 (core.c called gen_new_game, not scrClearGlobals; 26ef68d
    set it there): 7aa53d3 starts a route from scrClearGlobals as playhost and game_begin do. Checksums re-based;
    records changed only on p5_reg_l9s5 / p5_reg_l12s13 (l9s5 MAME mean 142.6 -> 123.6 K).
  - HD traces for the three lush routes (build/trace/p5_lush_*, scripts/hd_trace.sh from the main checkout): the exact
    build is record-equal on all three (637 / 641 / 631), so P5 regress and EQUIV now check them. EQUIV: the grid
    build fails the route check on p5_lush_l5s37 (deaths + damage from record 418): an oEnemySight that meets the
    player and a solid in one step runs Collision_oCharacter first in the exact build (the cavemen within 100 px
    attack: 111013 status 2) and Collision_oSolid first (destroyed, no alert) in the grid build. The grid build's
    output is identical at cd3bc01, 3249c51, a2fed51 and cf7d06f: the collision-order design (EQUIV.md section 2),
    not this batch. Not accepted (tests/equiv_accept.txt needs route=PASS); make check stops at EQUIV 87/88.
  - Commits: speartrap_step's nearest only when an instance of the family lies in the box its tests need (8f3d48a,
    instance_box_maybe on nc's floors; MAME l5s11 201.8 -> 159.3 K), its y / x tests on the float bits (10a8418),
    the bottom trap's ceil setters skipped at whole x, y (5be0fe7), evnz: the alarm passes and snapshots walk only
    objects with instances (66ad79c; all routes -3 to -6 % MAME), oTree / oTreeBranch out of view without the
    dispatch chain (0a3b571), a treasure out of view without ev_step (57632e2), evobj / evnz sized for RAM (dbd8db1),
    piranha IDLE's prey() only when a prey family swims (cf7d06f); playsh2_jt.sh JT_ROUTES (f92fdb7).
  - MAME SOFTFP, full playsh2 (steps 2+; a2fed51 with 7aa53d3's harness -> cf7d06f): l5s11 201.8 -> 146.8 K (p99
    222.6 -> 170.4 K), l5s37 171.5 -> 138.1 K (190.8 -> 158.9 K), l6s23 178.0 -> 141.6 K (204.5 -> 171.3 K); all
    routes 111.7 -> 99.6 K, p99 218.9 -> 205.2 K, steps over 838,940 at x4.2 476 -> 119 (exit559 96.4 -> 91.4 K,
    caveman 108.2 -> 103.8 K, snakes 89.5 -> 84.6 K).
  - jtcost (fit): p5_lush_l5s11 201 744.8 -> 602.2 K (instructions 211.8 -> 150.7 K; I-miss 10.5 -> 9.0 K lines);
    p4_exit559 301 394.7 -> 387.9 K; p5_snakes 956 370.1 -> 360.1 K.
  - jtcps3 (.62, JT_ROUTES=p4_exit559,p5_caveman,p5_snakes and the three lush routes; PASS 11/11 each; route step
    means with step 1):

    | Build | l5s11 | l5s37 | l6s23 | caveman | snakes | exit559 |
    |---|---|---|---|---|---|---|
    | a2fed51 + 7aa53d3 harness | 823.0 K | 715.0 K | 735.7 K | 512.6 K | 415.0 K | 378.9 K |
    | 66ad79c (evnz) | 675.0 K | 623.4 K | 657.3 K | 500.9 K | 398.3 K | 368.8 K |
    | 0a3b571 (trees) | 672.4 K | 611.7 K | 647.7 K | 494.6 K | 390.9 K | 364.6 K |
    | 57632e2 (treasures) | 674.7 K | 614.3 K | 652.0 K | 494.6 K | 392.7 K | 364.8 K |
    | dbd8db1 (RAM sizes) | 683.6 K | 623.5 K | 657.0 K | 501.4 K | 398.5 K | 366.3 K |
    | cf7d06f (prey; gated) | 675.4 K | 628.8 K | 647.4 K | 498.9 K | 395.6 K | 367.1 K |

    The jtcps3 / MAME ratio on the lush routes went from 4.03-4.11 to 4.4-4.5. Layout moves the lush means by 1-1.5 %
    (dbd8db1 only resizes two arrays: +0.8 to +1.5 %), as much as the last four commits' own effects.
  - Gates on cf7d06f: make check's parts (host builds, constcheck, P5 regress 24/24 with the lush routes, EQUIV 87/88:
    p5_lush_l5s37 above, snd 19/19 0 differ), ctall 59/59, playsh2 9,701/9,701 (grid) and 9,701/9,701 (SOFTFP), shell
    26/26 + 49/49, game_check p4_exit559 0 px at 30/150/300 (tests/game stack room 34,168 B), capture_check 430/430
    steps and 6/6 checkpoints equal; SH-2 0 warnings.
  - Measured and not kept: the jungle's terrain points as collision_point_any_at's integer query (MAME 0, jtcost
    +1.9 %); a y-band mask before instance_box_maybe's scan (MAME +0.6 %); a creation-ordered list of the Draw
    instances for draw_and_view (MAME -1.7 to -2.1 %, jtcost +0.4 to +1.8 %, jtcps3 +0.5 to +1.4 %, and 7 KB of main
    RAM the tests/game link does not have).
- Spikes (branch spikes on main 53c449c, 2026-10-06): the steps over ~200 K MAME (about 0.84 M jtcps3), traced with
  jtcost fit and JTC_BYOBJ.
  - Where they go: c_temple_hawkman every 6th step (step 7: 1,003 K, oHawkman 535 K): hawkman_sight's pcos_cr /
    psin_cr(degtorad(0 or 180)) on the double-double path (sc_fast needs |a| >= 2^-6; sin(pi) fails Ziv's test).
    p5_idol 250 (1,013 K): oBlood 499 K, its line tests against the rotating boulder (precise_line / pl_axis per pixel
    in soft-float; pcinst_of is cached already). p4_bomb_drop 201 (913 K): spread (collision pass 229 K, rubble 17 x
    ~7 K, flames, gold chunks). p5_reg_l12s13 45 (1,285 K; c_temple_xroom3x the same steps): oGame 585 K, the player
    riding a falling oDarkFall (4 -> 11 px a step); each pixel moves the block +5, tests the player's bottom, moves the
    player 1 px, moves the block -5 and +1 (pen_moving_solids' viscid-top branch; cct true on all 71 pixels). The one
    exact cut, the bottom test with the block taken at y + 5 in place of the two moves (the grid build's searches do
    not depend on when an entry is flushed), is ~12-15 % of the step and leaves it over 0.84 M: not done.
    p5_reg_l14s16 21 (1,141 K): spread (oPlayer1 356 K, oTombLord 244 K). c_swamp_grave 186 / 351 (2.71 / 3.07 M):
    oGame's check_water 1.87 M (114 instance_place_p, 411 collision_point_any on doubles, 914 __adddf3).
  - 43d6d63: hawkman_sight through pen_sight_speed (the kept pair, as caveman's and pk_ice's sights). MAME SOFTFP:
    c_temple_hawkman mean 140.3 -> 128.4 K, max 284 -> 187 K, steps over 200 K 30 -> 0 (step 7 277.7 -> 146.7 K);
    c_temple_tomblord 154.3 -> 149.1 K (5 -> 1); c_temple_weblava 151.0 -> 146.0 K (5 -> 2).
  - 1a38d1f: psin_cr / pcos_cr / psincos_cr keep the slow path's results for +-0, pi / 2, pi, 3 pi / 2 (632 of the
    683 slow-path calls left over all routes' host runs). MAME SOFTFP route means: c_swamp_grave 165.3 -> 158.6 K,
    vampire 140.8 -> 136.8 K, p5_buy 83.2 -> 77.0 K, p5_spider 66.8 -> 63.8 K, webwater 135.2 -> 131.1 K (maxima
    unchanged: their causes are elsewhere).
  - Both: 182 host route runs byte-identical (PCOL stats equal); playsh2 grid and SOFTFP 9,701/9,701; shell; capture
    428/428; tests/game link 0 compiler warnings, stack room 33,740 B. Trace gates pending (the retrace).

### 2.1 integer shadows and 2.4 the integer collision kernel (branch ikernel, 2026-10-06)

Six commits on 1f1815a, rebased onto 3dd5a2b (off-view deactivation on). No new .bss: tests/game stack room 32,892 B on 3dd5a2b, the same as main.

- **2.1 (b76776d, 96a5f6a):** struct pin stays 64 bytes. `persistent` (written once, never read) is dropped,
  `type` becomes int8, and cleanDeath / shopWall / treasure become bitfields. That frees 4 bytes for int16 `ix`, `iy`:
  - the value when the float is whole in (-30000, 30000), PXY_NO when it is not, PXY_UNK when stale;
  - every write of x / y marks it stale: the setters on a POS_NE change, and the raw writes through PIN_SETX_RAW /
    PIN_SETY_RAW (moveTo's walk and restore, pobj.c's settle, characterStepEvent's slope loops, pin_add);
  - pin_xy_int reads the shadows, and pin_xy_fill decodes lazily;
  - the PLAY_STATS builds compare every read with pos_int on the floats (an injected off-by-one aborts).
  The box shadow already existed (bl..bb, bbk).
- **2.4 (a2d6acd .. 2602052):** one in-grid summary leaf, ik_cells(i, n, step, nm), with four register arguments.
  Its users:
  - ik_line (4 arguments) for solid_vline_any / solid_hline_any;
  - ik_side for isCollisionLeft / Right / Top / Bottom and anyCollisionLeft / Right (one routine, tail call);
  - solid_point_sum;
  - collision_point_any's whole oSolid point.
  ik_xpt answers the static-family whole points (xstatic_any's first two answers on ints). collision_point_any and
  collision_point_any_at no longer build struct pq when the summary or the index answers. The first version, a 2-D
  ik_sum with a stack argument, cost +3 % (about 140 instructions and 18 stores a call): measured, then replaced.
- **jtcost (fit, no NC), model K, drain 61 / l5s37 201 / caveman 138 / p4_exit559 301:**
  - 1f1815a: 400.6 / 441.6 / 407.9 / 354.9;
  - 2.1: 377.4 / 418.0 / 393.2 / 341.5;
  - 2.1 + 2.4: 379.6 / 420.6 / 394.8 / 335.9.
  2.4 lowers the fully associative bound by 0.8-2.9 % beyond 2.1, but set conflicts rose by 100-250 lines in this
  layout.
- **jtcps3 (.62 / .81, NC link, route step means with step 1, K; base 1f1815a from LUSH 12.5):**

  | route | 1f1815a | 2.1 (c81da62) | 2.1 + 2.4 (b069e58) |
  |---|---|---|---|
  | p5_lush_l5s11 | 467.7 | 446.3 (-4.6 %) | 434.4 (-7.1 %) |
  | p5_lush_l6s23 | 407.4 | 396.0 (-2.8 %) | 386.0 (-5.3 %) |
  | c_swamp_drain | 635.4 | 617.0 (-2.9 %) | 599.4 (-5.7 %) |
  | c_swamp_swim | 491.1 | 477.7 (-2.7 %) | 461.5 (-6.0 %) |
  | c_swamp_piranha | 390.5 | 374.1 (-4.2 %) | 363.5 (-6.9 %) |
  | c_swamp_grave | 626.1 | 596.9 (-4.7 %) | 574.2 (-8.3 %) |

- **Gates (b069e58):**
  - make check (EQUIV 88/88), ctall 59/59;
  - playsh2 9,701/9,701 grid and SOFTFP;
  - shell 26/26 + 49/49;
  - game_check p4_exit559 / p5_shop / p5_spider 0 px (13 frames);
  - capture 422/422 and 6/6;
  - SH-2 0 compiler warnings;
  - stack room: tests/game 33,600 B, playsh2 JT (NC) 62,900 B.
  Every commit: 182 host route runs byte-identical, stdout and stderr.
  On 3dd5a2b:
  - 182 runs byte-identical to 3dd5a2b's default build (PLAY_DEACT 32), with PIN_SHADOW_CHECK clean;
  - DEACT=0: ctall 59/59, EQUIV 88/88;
  - game_check p4_exit559 0 px.
  pw_activate and pw_deactivate write no x / y, so the shadows of a deactivated instance stay valid.
- **Not done:** the other-family queries (ladder / platform / water / moveable solid through pgrid_search) and
  collision_rect_p's fractional queries.

### check_water and the drain's water queries (branch water, 2026-10-08, on main 832c069)

Target: c_swamp_drain's frame drops (.62: 113 of 370 step pairs). jtcost (fit, modelled jtcps3 clocks) on the record
after the step: drain 272 (step 271) and c_jungle_firefrog 237; MAME: SOFTFP playsh2 on c_swamp_drain, c_swamp_grave,
c_jungle_firefrog (1217/1217 checksums on every run), steps after step 1: mean, max, steps over 150 K.

| Commit | What | drain 272 | firefrog 237 | MAME drain / grave / firefrog max (K) |
|---|---|---|---|---|
| 832c069 | base | 2,688 K | 1,655 K | 714 / 407 / 531 |
| 2eaa502 | pw_filled_xy: point_at_xy's two grid paths in one call | 2,479 K | 1,478 K | 666 / 374 / 493 |
| 98274bc | instance_place_ixy: xplace_one_i's one-cell case inline | (with c2ec8b8) | | |
| c2ec8b8 | pcol_place_marks_kept: in the grid a kept self's mark is its test-list move | 2,162 K | 1,244 K | 621 / 344 / 455 |
| 0f79d4f, acc9152 | check_water: room test hoisted; double path out of line | 2,128 K | | |
| 0cc0511 | x-index: isfar and cells in one 5-byte record | (with 15ad6eb) | | |
| bcc9124, 15ad6eb | collision_point_any_at: oSolid at a non-whole position on its floors; oDrip uses it | 2,145 K | 1,201 K | 603 / 344 / 455 |

MAME means (K): drain 121.9 -> 119.5, grave 123.0 -> 122.3, firefrog 104.7 -> 103.5. Steps over 150 K: drain 37 -> 37,
grave 49 -> 47, firefrog 29 -> 30 (step 245: 149,920 -> 150,048). Drain step 280 (record 281): 1,438 K -> 1,405 K.
Every commit: hostident 184/184; c2ec8b8 and HEAD: CTALL 59/59, EQUIV 88/88; HEAD links on the SH-2 (32 KB stack).

- **oGame on step 271:** 1,222 K -> 682 K. Of the base: the marks of instance_place_ixy's self (mark_e, flush_run,
  cupdate, dlist: about 250 K) went with c2ec8b8 (grid build only; the exact build keeps them: the R-tree's order
  depends on them); point_at_xy's wrappers about 210 K with 2eaa502.
- **What is left there:** instance_place_ixy 151 K (182 calls; per call: pcol_query, touch, tlist_front, the index's
  cell and hint, pcol_search_has_i, match, precise twice), pw_filled_xy 141 K (358 calls), check_water 66 K. Most is
  data-line misses: the per-water path (about 3 KB of code and 30 data lines) does not stay in the 4 KB cache.
- **Where the drain's other slow steps go (jtcost on HEAD acc9152):**
  - check_water runs only on steps ~270-274.
  - Step 271's collision pass (688 K, billed to oTreeBranch) is the bomb: explosion_solid 442 K inclusive through
    ev_collision (collision_point_p 36 calls, rubble pin_add).
  - Steps 275-295 (200-400 K MAME) run no check_water: step 280 is oDrip 409 K (43 drips, ~9.5 K each, about two
    thirds cache misses), oBlood 155 K, oRubbleSmall 145 K, oManTrap 132 K, the collision pass 173 K.
  - Step 165-type steps (150-220 K MAME) are general play (player, piranha, monkey).
- **Measured, not kept:** an 8-byte x-index record: +5.4 KB .bss, and the SH-2 link fails its 32 KB stack check; the
  5-byte record has the five arrays' RAM. Its gain on step 271 is small (oGame -3 K; the step +16 K from SIMM data
  misses of the moved layout, instructions -2.6 K).
- **Not done:**
  - a fused per-water path (the int body in one function: estimated oGame -200 to -400 K on steps 270-274);
  - oDrip's other costs: pw_changed / mark_e per move, 4 gcmp_fi view compares, x + 0.0 through soft-float when
    xVel is 0;
  - the debris objects' Steps run interleaved with others' (code always cold); a batch needs an order proof (the
    test list order is observable).


### Step 2 and explosion steps (branch deact3, on main 832c069, 2026-10-08)

The level's second play step deactivated every off-view candidate one by one (DEACT.md: the room's first step has no
pass), and explosion steps spent their collision time in precise_collision_int's pixel loop. Eight commits, each
with its exactness argument in its message and `scripts/hostident.sh check` 184/184 equal.

| Commit | Change | Exactness |
|---|---|---|
| df7cb13 | pw_deactivate_n: the pass's deactivations remove their slots from pw_ord in one compaction (was one shift of pw_ord's tail each: 4,720 stores) | value proof: nothing between reads pw_ord; removing a set keeps the rest's order |
| 9483b8d | precise_collision_int by mask rows: up to 25 columns of each mask row as one word, ANDed (flips by bit reversal) | value proof (both scales +-1, mask boxes start >= 0); old loop kept as pci_loop, PLAY_STATS compares every call; 2,000,000 random cases equal |
| ed35ea8 | dcand_init in the room's first step (its obj_anc scan was most of play_step's 1,948 data misses in step 2) | value proof: a function of the object tables |
| 69398d4 | pw_deactivate_n: olive_add once per object (count summed) | value proof: sums commute; nc_inval idempotent; xchg / olive_gen are clocks compared only for equality |
| dc9c821 | pci_of: scale signs from the float bits (no ___gtsf2) | value proof: f > 0 is 0 < bits <= 0x7f800000 |
| 2716d12 | pw_deactivate_n: the compaction as a copy loop between the ids | codegen-only |
| 1ff88a9 | deact_pass: doutside's integer case inline (no call, no out-parameter stack) | codegen-only; PLAY_STATS keeps the checked path |
| b43a919 | gout_fi / POUTI: PLTI(x, lo) or PGTI(x, hi) with one decode of x (rubble, leaf, detritus Steps, doutside_slow) | value proof (gcmp_fi's tests on the same h, l); 40,000,000 random cases equal |

**jtcost** (fit constants, modelled jtcps3 clocks; JTC_BYOBJ=1):

| Step | 832c069 | after | change |
|---|---|---|---|
| c_swamp_drain record 3 (step 2) | 719,612 | 618,706 (df7cb13), 556,685 (ed35ea8), 548,569 (69398d4), 543,029 (2716d12), 543,924 (b43a919) | -24.4 % |
| c_items_damselexpl record 121 (step 120) | 913,735 | 857,718 (ed35ea8), 859,243 (b43a919) | -6.0 % |
| p4_bomb_drop record 202 (step 201) | 859,913 | 868,620 (1ff88a9), 846,792 (b43a919) | -1.5 % |

Drain step 2 by function: pw_deactivate 114.3 K self (175 K with callees) -> pw_deactivate_n 24.6 K self (79.5 K
pass); olive_add 21.8 K -> 6.7 K; play_step self 110.1 K -> 61.7 K. precise_collision_int in damselexpl step 120:
73.0 K -> 12.8 K (9 calls). In bomb_drop step 201, gcmp_fi's 144 calls (18.4 K) became gout_fi's 56 (8.1 K). The
1ff88a9 bomb_drop figure (+1.0 % against the base with fewer instructions: more fetch conflicts) is layout; MAME
puts that step at -3.4 % there.

**MAME** (SOFTFP=1 playsh2_check; checksums 9,701 / 9,701 on the default routes and 9,960 / 9,960 on the 28 c_*
routes playsh2 takes, ROUTES="c_ice_darkfall ... c_temple_xroom3x": every c_* route without a globals / room
header). "Step 2" is the step after each level start (60 in the two sets):

| Set | step 2 mean (832c069 -> b43a919) | step 2 > 150 K | other steps 150-500 K | all-step mean |
|---|---|---|---|---|
| default 27 routes | 143.0 -> 104.3 K | 14 -> 3 | 194 -> 182 | 76,430 -> 74,635 (-2.3 %) |
| 28 c_* routes | 205.8 -> 166.4 K | 27 -> 16 | 229 -> 216 | 98,138 -> 95,959 (-2.2 %) |

Explosion clusters (MAME K a step, mean and max; steps > 150 K):

| Route, steps | mean | max | > 150 K |
|---|---|---|---|
| c_items_damselexpl 120-135 | 179.1 -> 170.0 (-5.0 %) | 209 -> 198 | 15 -> 15 |
| p4_bomb_drop 195-207 | 180.8 -> 170.8 (-5.6 %) | 217 -> 210 | 11 -> 10 |
| p4_bomb_throw 292-302 | 165.2 -> 161.4 (-2.3 %) | 187 -> 184 | 10 -> 10 |
| p5_reg_l2s10 / l3s10 170-190 | 189.1 -> 183.5 (-3.0 %) | 236 -> 229 | 20 -> 20 |
| p5_reg_l4s10 170-190 | 179.5 -> 174.0 (-3.0 %) | 231 -> 223 | 17 -> 16 |
| p5_reg_l9s5 335-345 | 223.5 -> 216.8 (-3.0 %) | 244 -> 240 | 11 -> 11 |
| c_swamp_vampkill 281-290 | 223.5 -> 219.1 (-2.0 %) | 305 -> 300 | 10 -> 10 |
| p5_reg_l14s16 19-43 | 168.4 -> 165.0 (-2.0 %) | 225 -> 223 | 21 -> 20 |

**Gates on b43a919:** hostident 184/184 (every commit); ctall 59/59; EQUIV 88/88; playsh2 SOFTFP as above;
game_check p4_exit559 0 px (records 30, 150, 300); no new .bss (brev8 is const, in ROM; pw_deactivate_n's object
counts are 32 B of stack): tests/game stack room 32,892 B.

**What is left, measured:**
- Step 2 after the deactivation pass: c_temple_weblava step 2 is 1,019.5 K modelled against 647.7 K for step 3; the
  pass is 61.6 K of the 372 K difference. The rest is the first full Step of the in-view instances before their rest
  memos exist (ptemple_ev +122 K, treasure_step +66 K, jar_step +37 K, item_step +26 K; rest_end, nc_get,
  collision_point_p only in step 2): game logic, not a one-off of the port.
- Explosion steps are spread over the new and moving instances: p4_bomb_drop step 201 has 25 rubble pieces at about
  7.5 K modelled each (pw_changed, mark_e, grid_dirty, collision_point_any: per-move bookkeeping and the misses of
  several small tables), oFlame Steps (ik_line, moveTo), the oWeb collision pass (pgrid_put, ebbox, addsf3).
  pin_add (about 3 K a call: the 208-byte pin_ext template copy and a dozen table misses) and evnz_sync (an event
  list rebuilt when an object's list goes empty / non-empty: 15 K in drain step 2, 9.7 K in damselexpl step 120)
  were not changed. Not tried: batching pcol_deactivated's obj_count (about 4 K in drain step 2).
- jtcost.sh with two windows on consecutive records (`c_temple_weblava 3 c_temple_weblava 4`) never opened the
  second window (stopped after 40 min); one call per step works.


### Detritus Steps: oDrip, rubble, oBlood, oFlame (branch debris, 2026-10-08, on water 0b06854)

Target: the per-instance Step cost of the pieces an explosion leaves (drain steps 275-295, firefrog 266-284,
damselexpl 120-140). Measured: jtcost (fit, plain cached link, modelled jtcps3 clocks; "fa" = the fully associative
bound) on drain records 281 / 290 (one run), firefrog 268, damselexpl 121; MAME SOFTFP playsh2 on drain, firefrog,
damselexpl, grave (1528/1528 checksums on every run): steps after step 1, mean / max / steps over 150 K.

| Commit | What (exactness) | drain 281 | drain 290 |
|---|---|---|---|
| 0b06854 | base (water) | 1,405 K (fa 1,403) | 1,142 K |
| 3570891 | rubble / ice / temple pieces: oSolid by collision_point_any_at (value, as 15ad6eb) | | |
| c05f527 | one rubblepiece_step for oRubble, oRubbleSmall, oDrip, oRubbleDarkSmall, oLavaDrip (codegen) | 1,372 K (1,385) | 1,131 K |
| 6544c17 | x += xVel skipped for xVel +-0 and a normal x (value: x + 0 is x, same bits, no mark) | 1,374 K (1,374) | 1,141 K |
| 1aa01ae | the view tests on the shadows / floors (pout_ab; value) | 1,303 K (1,319) | 1,108 K |
| 72ac93b | pw_piece_tests: the three point tests on one query (value; oSolid before site 1041, grid build) | 1,252 K (1,277) | 1,070 K |
| 840ee81 | oBlood / oFlame / oBone / oMagma: detritus_step's own isCollisionBottom answer (grid build) | | |
| 65602ff | detritus_step: yVel < 0 before isCollisionTop (grid build) | | |
| 09f25c5 | pw_piece_tests: whole by the shadows, no decode of the new y | 1,289 K (1,290) | 1,081 K |
| 9fc3770 | SK_RUBBLE: ev_step calls rubblepiece_step without the package dispatch (codegen) | 1,304 K (1,272) | 1,088 K |
| 6101e7f | pfloor_int by fwhole's multiply, no variable shifts (value; all 2^32 floats checked) | | |
| f5975ea | fwiden (TOD) for the drip's y and the query's floats (codegen) | 1,261 K (1,266) | 1,057 K |
| 7e8370c | detritus compares against 6, 1, 20 on the bits (pcmpc.h; tests/cmpc 515 M cases) | | |
| af85a93 | moveTo's walk: the stepped position by fint15, the shadows kept (value) | 1,226 K (1,245) | 1,044 K |
| c17d6a4 | pw_piece_tests: the query's floats only when xstatic_any reads them (codegen) | | |
| e6a29ea | moveTo_walk: moveTo's grid-build walk for the detritus as a small function (value) | 1,222 K (1,230) | 1,029 K |

Base -> e6a29ea: drain 281 -13.1 % (fa -12.3 %, instructions 367.8 K -> 321.0 K), drain 290 -9.9 %; firefrog 268
1,347 K -> 1,328 K (-1.4 %; fa 1,229 K -> 1,157 K, -5.8 %; instructions -5.1 %); damselexpl 121 920 K -> 910 K
(fa -1.7 %). The set-associative model moves +-2 % with the layout alone (conflict misses in unrelated objects:
firefrog oGame +27 K at 840ee81 with the same instructions), so the fa column and the instruction counts are the
steadier per-commit signal; the shipped link (nc_robust.txt) runs moveTo, detritus_step, pw_changed, mark_e,
grid_dirty and isCollision* uncached, which this model does not show.

By object (base -> e6a29ea, modelled K): drain 281 oDrip 362 -> 271 (43 drips: 8.4 -> 6.3 K each), oRubbleSmall 153
-> 96, oRubble 81 -> 52, oBlood 156 -> 131; drain 290 oDrip 237 -> 170, oBlood 130 -> 111; firefrog 268 oBlood
280 -> 239 (10), oFlame 158 -> 137 (6); damselexpl 121 oFlame 65 -> 54.

MAME (K; base -> 6101e7f -> af85a93 -> e6a29ea):

| Route | mean | max | steps > 150 K |
|---|---|---|---|
| c_swamp_drain | 119.5 -> 117.2 -> 116.5 -> 116.2 | 602.8 -> 561.3 -> 560.5 -> 557.7 | 37 -> 36 -> 35 -> 35 |
| c_jungle_firefrog | 103.5 -> 102.2 -> 101.2 -> 101.0 | 454.7 -> 444.9 -> 441.5 -> 440.3 | 30 -> 26 -> 24 -> 24 |
| c_items_damselexpl | 127.4 -> 127.4 -> 126.4 -> 126.2 | 266.7 -> 260.4 -> 256.9 -> 255.6 | 59 -> 58 -> 55 -> 55 |
| c_swamp_grave | 122.3 -> 121.2 -> 120.5 -> 120.4 | 344.2 -> 334.7 -> 330.9 -> 329.6 | 47 -> 43 -> 37 -> 37 |

Drain steps 275-295 at 6101e7f were 7-11 % below the base each (278: 393 -> 352 K, 281: 306 -> 276 K); they stay
over 150 K MAME: the drips are a fifth of such a step, and the rest is oTreeBranch's collision pass, oManTrap,
oPlayer1, oBloodTrail.

- **Gates:** hostident 184/184 after every commit; ctall 59/59 and EQUIV 88/88 at 09f25c5, af85a93, e6a29ea; playsh2
  SOFTFP 1528/1528 at 6101e7f, af85a93, e6a29ea; tests/game links with 0 compiler warnings, .bss unchanged (stack
  room 32,892 B); .text +2,376 B.
- **Measured, not kept apart:** 09f25c5 alone raised drain 281 (fa +12.6 K, instructions +3.2 K): without the y
  decode, pout_ab's x took pfloor_int, whose variable shifts were __lshrsi3 calls; 6101e7f removed them.
- **Not done:**
  - the drip's own soft-float (y += yVel, yVel += yAcc: __adddf3 x 2, __truncdfsf2) and pw_changed / mark_e /
    grid_dirty per move (about 1.4 K modelled a drip; uncached in the shipped link);
  - oBlood / oFlame: vel_parts twice a Step (xVel changes only at a wall), isCollisionLeft || Right as one query,
    the alarms' oBloodTrail / oFlameTrail creation, the animation pass's float image_index;
  - oBloodTrail (43 K at drain 281: the deactivation pass's doutside and the animation).

### Ice, temple and swamp steps (branch icetemple, 2026-10-08, on fpsbatch 7af3758)

Targets (MAME survey on fpsbatch, SOFTFP playsh2, steps over 175 K): p5_reg_l9s5 (level 9, ice) steps 335-361,
p5_reg_l14s16 (level 14, temple) 21-45, c_items_damselexpl (level 12, ice; the explosion aftermath is the debris
branch's), c_swamp_vampkill 283-298, c_temple_tomblord. Developed on 4a8284c, rebased onto 7af3758 (debris merged);
every commit is its own exactness argument (in its message).

**Attribution (jtcost, JTC_BYOBJ, JTC_CALLERS, JTC_PCHIST; 4a8284c, modelled K):**
- l9s5 step 359 (1,019): debris (oBlood, oRubbleDarkSmall, oFlame, oDrip, oRubbleSmall, oBurn) about 45 %; oYeti 231
  (its Steps 87, the collision pass 83 billed to it, play_step's Outside pass 29); oDarkFall 76 (11 blocks: each
  Step's isCollisionCharacterTop through calculateCollisionBounds doubles and four drounds, ~30 soft-float calls);
  oUFO 61 (mostly the flush its query triggers); the flushes (flush_run <- ik_line) 102 in all.
- vampkill step 288 (1,128): oBlood 379; oZombie 205 (the collision pass, precise float overlaps; isCollisionTop /
  Bottom at fractional y through doubles); oPlayer1 175.
- damselexpl step 193 (1,109): the collision pass 221 (explosion vs oIce, oPlayer1 vs oBlood: debris), oFlame / oBlood
  / oDrip 413, oPlayer1 110, oDarkFall 50.
- l14s16 step 21 (938): oPlayer1 331 (characterStepEvent 103, scrCreateBlood 51: six oBlood Creates), the collision
  pass 135 (six new oBlood searchers paired with oPlayer1), oSmashTrap 80 (attack: isCollision* at fractional x,
  isCollisionCharacter* through doubles). Steps 40-45 are oBlood (39 %) and the pass.

| Commit | Change |
|---|---|
| b8b45bd | pfr / pfr_k: isCollisionLeft / Right / Top / Bottom and anyCollisionLeft / Right at a fractional position on ints |
| c85d27e | line_solid_k: cct / ccr / ccl / ccb and oDarkFall's char_on_top on ints (pw_test_line_i) |
| b22b37d | pin_box_outside: a BB_DBL box at scales +-1 compared on pfr |
| 613e61d | pcolgrid.h pg_kcell: a float rectangle's cells from s_kfloor, not a binary search |
| b68ae89 | ebbox: the float box at scales +-1 with 4 soft-float calls, not 16 |
| c2b85f4 | prun.c DCAND: the candidate bit by a table (GCC made a __ashrsi3 call of it) |
| 682b441 | anim_one: the wrap compares on the float keys; ispd * sp skipped for sp exactly 1 |

Proofs: pfr_k against dround((double)x + k) for every float with |x| < 2^22 times 15 offsets (37.5 G cases); the
animation compares for all 2^32 floats against 11 operands both ways plus 400 M random pairs; temporary host checks
(removed) beside the old code on every call of the hostident runs: line_solid_k 59,236 calls, pin_box_outside 88,786,
ebbox 65,202 (bit-identical floats); pg_kcell keeps a PLAY_STATS compare (121,276 calls).

**jtcost** (fit, modelled jtcps3 K; the middle columns are the commits before the rebase, on 4a8284c; the last the
rebased branch against its base):

| Step | 4a8284c | c85d27e | 613e61d | b68ae89 | 682b441 (on 4a8284c) | 7af3758 -> HEAD |
|---|---|---|---|---|---|---|
| l9s5 349 (record 350) | 979.7 | 931.4 | | | 869.5 | 902.1 -> 790.3 (-12.4 %) |
| l9s5 359 | 1,019.2 | 965.8 | 944.7 | 934.3 | 919.1 | 960.4 -> 866.2 (-9.8 %) |
| l14s16 21 | 937.6 | 929.1 | | 921.0 | 911.6 | 930.0 -> 911.3 (-2.0 %) |
| l14s16 43 | 868.6 | 887.3 | | | 864.3 | 795.3 -> 784.3 (-1.4 %) |
| damselexpl 193 | 1,108.6 | 1,089.3 | 1,089.0 | 1,067.1 | 1,056.4 | 1,051.9 -> 1,001.4 (-4.8 %) |
| vampkill 288 | 1,127.7 | 992.1 | 985.6 | | 964.6 | 1,080.1 -> 930.0 (-13.9 %) |

l14s16 43 at c85d27e: +18.7 K with the same instruction count (layout). By object on l9s5 359 (4a8284c ->
682b441): oDarkFall 76 -> 20 K.

**MAME** (SOFTFP playsh2, steps after step 1; 7af3758 -> HEAD):

| Route | mean (K) | max (K) | > 150 K | > 175 K |
|---|---|---|---|---|
| p5_reg_l9s5 | 95.7 -> 74.4 | 235 -> 199 | 33 -> 26 | 26 -> 19 |
| p5_reg_l14s16 | 101.4 -> 97.9 | 223 -> 212 | 26 -> 18 | 5 -> 4 |
| c_items_damselexpl | 123.8 -> 108.4 | 251 -> 230 | 52 -> 29 | 25 -> 16 |
| c_swamp_vampkill | 125.6 -> 120.5 | 288 -> 232 | 26 -> 26 | 18 -> 15 |
| c_temple_tomblord | 127.0 -> 122.2 | 216 -> 210 | 4 -> 3 | 1 -> 1 |
| default 27 routes | 70.1 -> 67.0 (-4.5 %) | | 146 -> 112 | 62 -> 43 |

No step of the default set is more than 1 % slower. p5_reg_l12s13 (also ice): 77.9 -> 61.8, > 175 K 4 -> 0.

- **Gates (HEAD 682b441):** hostident 184/184 after every commit (on 4a8284c, and again on 7af3758 for each rebased
  commit); CTALL 59/59; EQUIV 88/88; playsh2 9,701 / 9,701 in the SOFTFP and default builds, 1,732 / 1,732 on the
  five routes; game_check p4_exit559 0 px (records 30, 150, 300); tests/game links with no compiler warning from the
  changed files, .bss end 0x02076a84 (38,268 B of stack: no .bss added; the new tables are const).
- **Measured, not kept:** collision_result keeping the last (searcher object, hit object) pair's has_col answer
  (5 bytes of .bss): instructions -200 a step, but every traced step +22 to +32 K modelled (the fully associative
  bound too: data layout); dropped.
- **What is left (measured):**
  - The l9s5 cluster (now 175-199 K MAME) is about half debris (oBlood, oRubbleDarkSmall, oFlame, oDrip, oBurn) and
    the per-move bookkeeping their moves cause (cupdate_at -> ebbox_rect -> pgrid_put in the flushes, pw_changed,
    mark_e, grid_dirty: about 3 K modelled a moved entry, mostly data misses across small tables).
  - The collision pass (82-157 K): pgrid_search 2.5-3.9 K a call, a fifth of it four missing loads (pg_head, pg_next,
    erw, pw_seq: separate arrays); has_col / tlist_* per searcher; ev_collision's dispatch chain.
  - vampkill's oZombie pairs take overlap_at's float precise path (pcinst_of, precise_collision: 104 __addsf3 calls
    a step); an integer form needs a proof for fractional positions.
  - pin_add (about 7 K a creation with its Create: scrCreateBlood's six bloods are 51 K of l14s16 step 21).

### Per-move bookkeeping and the collision pass (branch moves, 2026-10-08, on main 8b3d544)

Target: the debris steps after an explosion (c_swamp_drain 275-295, c_items_damselexpl 120-200): each moved
collision entry's marks (pw_changed, mark_e, grid_dirty, tlist_front), the flush that brings its grid rectangle up to
date (flush_run, cupdate, ebbox, pgrid_put) and the collision pass (pcol_handle, pgrid_search). Owned code only:
pworld.c pw_changed / grid_dirty / the nc_moved test, pcol.c's marks, flush and pass, pcolgrid.h untouched.

**Models.** jtcost fit (the plain link, as before) and, for drain records 281 / 290, the shipping layout:
`build/sc/jtnc.sh` (not committed) builds tests/playsh2 with `NC=nc_robust.txt NCSHADOW=1`, traces it with
scripts/lua/jtcost.lua and models the trace with tools/jtbypass.py, the list's functions and arrays charged as
uncached ("NC" below; "NC fa": the same with a fully associative 4 KB cache, the layout-free bound). On hardware
pw_changed, mark_e, grid_dirty, tlist_front, dlist_remove, ebbox and the arrays dn, dp, tn, tp, pg_*, pw_seq,
oinfo, ocnt, xbits, inst_mem run past the cache, so instruction counts there cost 2.5+ clocks each.

**Where drain 281 went (NC model, base 996 K):** marks 168 K (pw_changed 55 K: nc_moved's loop over 4 slots with
obj_is on every move; mark_e 39 K; grid_dirty 24 K with a pin_needs_ext call; tlist_front 16 K; pcol_changed 14 K);
flush 194 K (86 CollisionUpdates, 30 of them float boxes: ebbox's 4 __addsf3 and an __eqsf2, rset_f, pg_kcell, two
ebbox_int calls); pass 54 K. Host count (drain steps 275-290): 77-93 CollisionUpdates a step, 60-76 of them of
objects no query asks for and that cannot pair (oDrip, oRubble, oRubbleSmall, oBlood).

| Commit | Change | Exactness |
|---|---|---|
| 16f8777 | pw_changed: nc_moved only for an object of a kept nearest family (nc_ofam bits, kept by nc_slot) | value; PLAY_STATS compares every bit with obj_is |
| b1d3814 | grid_dirty: membership from a lazy per-object byte (gobj) | value (a constant memoized) |
| 71a5c91 | mark_e: OI_SOLID in oinfo, ef read / written once, unlink only when on the list | value |
| f913ee0 | pcol_handle: the test list's head off inline | codegen |
| 81a83e5 | flush_run: the dirty list's head off inline | codegen |
| f237637 | cupdate: play's grid path without cupdate_at (cupdate_grid) | codegen |
| c4f2933 | ebbox_rect: no repeated whole-box test (ebbox_f); ang == 0 on the bits | value |
| 79eeecd | ebbox_whole: at scale +-1 and a whole position the side pair by int sums | value; 1.68 G cases, PLAY_STATS compares |
| 0c5ff4e | **deferred marks**: a member of an object no query has asked for this room waits on fhead; ask_fam (first query of a family) and flush_pairable (the pass) bring it up to date | value (below) |
| dc6dcd7 | nc_robust.txt: ebbox_f for ebbox | layout |
| 1afd63a | mark_e: a deferred entry on a list keeps its place | value |
| b82ed80 | pcol_handle: can_pair once per run of one object | value; PLAY_STATS compares |
| e1bf167 | grid_dirty: gobj also holds xbits != 0, read before gond; no frame | value |
| 1291832 | tlist_front: an entry already on the list out of line (no frame) | codegen |
| 596d2d2 | mark_e: the dirty-list branch out of line (mark_dirty, in nc_robust.txt), all tail calls | codegen |
| 308118f | pw_changed: no frame on the common path | codegen |
| 62b7563 | pw_changed: nc_ofam alone (it implies nc_any) | value |
| fd70d06 | gobj and nc_ofam in one byte per object (pwob) | layout (.bss -460 B) |
| 8e39cda | pcol_changed: mark_e's body inline for play (mark_ef) | codegen |
| 01d5414 | pcol_handle: both has_col answers kept from collision_result (pf) | value |
| 34781a8 | EQUIV.md, pcolgrid.h: the deferred entries described | docs |

**The deferral's argument (0c5ff4e).** A grid rectangle reaches an answer only through: pgrid_search's callbacks
(line_cb / rect_cb / place_cb / probe_cb drop entries outside the query's family before any other effect, after
pcol_query of that family, which asks for it; collision_result keeps a hit only when has_col relates it to the
searcher, so both can_pair); pcol_search_has(k) && match(k, obj): k outside the family fails match; direct_pairs'
candidates (related: can_pair); touch_stale's scan (asks for its family first); pcol_handle's searcher rectangle (its
box either way). The grid orders hits by creation number (unique among the grid's entries), so when an entry went in
does not change the hits' order. The stale stack keeps every stale member of an asked object. PLAY_STATS builds
check after every query and pass that no deferred entry is asked for or can pair. The put-in times of the deferred
entries differ from the runner's tree (EQUIV.md section "The shipping build" notes it).

**jtcost** (fit, modelled jtcps3 K; plain link; per-commit traces of drain 281 / damselexpl 194 were cut after three
commits to free the heavy slot; the rest measured with the batch):

| Step | 8b3d544 | 16f8777 | b1d3814 | 71a5c91 | 79eeecd | 0c5ff4e | 308118f | 01d5414 |
|---|---|---|---|---|---|---|---|---|
| drain 281 | 1,172.9 | 1,117.6 | 1,084.7 | 1,100.8 | 1,067.5 | 891.0 | 823.8 | 783.4 (-33.2 %) |
| drain 281 instructions | 296.1 | 288.1 | 285.6 | 283.9 | 274.1 | 213.8 | 203.7 | 202.3 (-31.7 %) |
| drain 281 fa bound | 1,181.6 | 1,132.7 | 1,095.8 | 1,091.6 | 1,074.4 | 937.9 | 701.3 | 669.8 (-43.3 %) |
| drain 282 | 1,118.4 | | | | 1,016.0 | 905.1 | 838.8 | 802.1 (-28.3 %) |
| drain 290 | 988.5 | | | | 930.4 | 791.6 | 749.4 | 734.6 (-25.7 %) |
| damselexpl 121 | 802.7 | | | | 765.0 | 752.7 | 748.9 | 744.2 (-7.3 %) |
| damselexpl 194 | 1,001.4 | 982.8 | 983.8 | 983.4 | 964.5 | 856.8 | 830.5 | 822.5 (-17.9 %) |

NC model (shipping layout) on drain 281 / 290: base 996.5 / 847.5 -> 79eeecd 913.1 / 801.5 -> 0c5ff4e 709.8 / 653.4
-> 308118f 706.6 / 655.0 -> 01d5414 696.3 / 643.3 (-30.1 % / -24.1 %); NC fa 832.3 / 746.9 -> 621.5 / 598.4 -> 587.4
/ 580.4 -> 577.5 / 574.5. Between 0c5ff4e and 308118f the owned functions fell 50 K (mark_e -16 K, grid_dirty
-15 K, dlist_remove -8 K, tlist_front -4 K, can_pair -4 K) while unchanged cached functions rose 45 K (ik_cells,
rubblepiece_step, solid_point_sum, ik_xpt, ev_step: same instructions, set conflicts of the moved .text); the fa bound
fell 34 K. Drain 281 by part (NC): marks 168 -> 58 K, flush 194 -> 22 K, pass 54 -> 57 K.

**MAME** (SOFTFP playsh2, the six routes, 2328 / 2328 checksums at 79eeecd, 0c5ff4e, 308118f, 01d5414; steps after
step 1):

| Route | mean (K) | max (K) | > 150 K | > 175 K |
|---|---|---|---|---|
| c_swamp_drain | 110.0 -> 101.7 | 525 -> 463 | 33 -> 28 | 28 -> 15 |
| c_swamp_grave | 114.3 -> 110.3 | 320 -> 303 | 33 -> 26 | 24 -> 12 |
| c_jungle_firefrog | 95.7 -> 91.5 | 432 -> 416 | 20 -> 17 | 14 -> 9 |
| c_items_damselexpl | 108.4 -> 102.1 | 231 -> 207 | 29 -> 23 | 16 -> 7 |
| c_swamp_vampkill | 120.6 -> 117.6 | 232 -> 226 | 26 -> 25 | 15 -> 12 |
| p5_reg_l9s5 | 74.4 -> 70.4 | 199 -> 174 | 26 -> 22 | 19 -> 0 |

Steps more than 1 % slower than the base: 4 (drain 107, 112; vampkill 146, 203: +1.2 to +2.2 %, first asks of a
family and flush_pairable's walk).

**Gates (01d5414; 34781a8 adds comments and docs only):**
- hostident 184/184 after every commit;
- CTALL 59/59 and EQUIV 88/88 at 79eeecd, 0c5ff4e and 01d5414;
- playsh2 on the default routes: 9,701 / 9,701 checksums in both the SOFTFP and the default build;
- game_check p4_exit559: 0 px at records 30, 150 and 300;
- the tests/game link: no compiler warning; mknc --check finds every list entry at its mirror after dc6dcd7; .bss end 0x02076d50 (stack room 37,552 B; the base had 38,268 B: gobj / pwob 460 B, pf 256 B).
- On the default 27 routes (SOFTFP, steps after step 1), base -> 01d5414:
  - mean 67.0 -> 64.5 K (-3.6 %);
  - steps over 150 K: 112 -> 63; over 175 K: 43 -> 7;
  - one step more than 1 % slower (job 50 step 77: 55.3 -> 55.9 K).

**Measured, not kept:** pcol_changed past the cache (nc_robust.txt): NC model -2.5 % on drain 281 but the fa bound
+1.0 % (conflicts, not work); not listed.

**Not done (measured):**
- The test list still takes every debris move (tlist_front about 130 NC a move) and pcol_handle walks it (about 250 NC
  an entry that cannot pair: unlink, alive, keeps_testing's sprite reads). Skipping it for objects that cannot pair
  needs the searcher order kept for entries that become pairable before the pass (a mark counter and a merge).
- pgrid_search: about 1,000 instructions a search of 9 cells and 3-8 hits (damselexpl 194: 15 searches, 30 K; drain:
  8-10, 22 K NC); a third of it the stack spills of the query's sides and the per-cell loop. oEnemySight fails
  direct_pairs (more than 16 candidates); the RV_LONG objects always search.
- Set conflicts: drain 281's NC model is 119 K above its fa bound (cached .text and data placement).
||||||| parent of 91658b2 (PERF3.md: instance creation and the transition room's first step (branch spawn))

### Instance creation, the transition room's first step (branch spawn, 2026-10-08, on main 8b3d544)

Targets: pin_add / evnz_sync / Create on explosion steps (c_items_damselexpl step 120, c_swamp_drain steps 2 and
271), and the transition room's first step (c_temple_xroom3x step 443: 1.96 M modelled, a 4-frame stall at every
level end). Measured: jtcost (fit, JTC_BYOBJ; record = step + 1) on temple 444, damselexpl 121, drain 3 and 272;
MAME SOFTFP playsh2 on the three routes (1254 / 1254 checksums every run) and the default 27 routes. Every commit's
message carries its exactness argument; hostident 184 / 184 after each one.

| Commit | Change |
|---|---|
| 5e56d91 | rv_build: rv lists up to 13 objects (RV_SHORT 8 -> 13), equal lists stored once (95 entries in 17 lists, RV_MAX 128 holds). The oSolid family's list (oBoulder, oBullet, oDarkFall, oEnemySight, ... 13) was RV_LONG, so every block on the test list searched the grid; can_pair now answers 0 when none of the 13 is alive |
| 70a585c | evnz_sync: the rebuild from a bitmap (bit rt: obj_byrt[rt]'s list non-empty, kept by prun_onz) ANDed with each key's objects, not a walk over evobj with a pw_ohead load each (.bss +960 B) |
| 1c25975 | ext_defaults_fast: the pin_ext defaults by word stores, not a copy of a 200-byte template (13 data lines a creation) |
| e0ffcfe | evnz_sync: each set bit by de Bruijn multiplication (the shift loop kept the word on the stack) |
| be4b575 | pin_add: the created object's constants kept (sprite, visible, depth as a float: a __floatsisf call, the pin_ext / pin_en flags); creations come in runs of one object |
| 2176d56 | olive_add: the nearest-instance cache's entries dropped without nc_inval (four 390-byte-apart lines and obj_anc / obj_bit on SIMM per call). On 8b3d544: a compare of the parent walk with nc_ko[4] keys; rebased onto moves (50b170d): NC_OFAM(obj) & nc_okm (moves's family bits, an ok-bit byte) |
| aa29458 | play_transition_start: blocks the first animation pass would only take off its list (prun_anim_idle) are taken off at load |
| 5db8c48 | play_level_start: the same at the level load's end |
| d0c641c | play_transition_start ends with pcol_load_flush (pcol.c's flush() in the grid build; nothing in the exact build): the loaded blocks' stale boxes (gen_create_event's sprites) go into the grid in the load, not in the first step |
| 853cbfe | play_level_start: the same after pcol_load_done |
| be35c8f | ev_create: the debris objects (blood, flames, trails, smoke, rubble, explosion, drips) skip the four P5 Create hooks, which have no case for them |
| f35d765 | pcol_handle: direct_pairs only for the objects it served before 5e56d91 (on main: OI_DIRECT, targets + rv list <= 8; see below) |

**jtcost** (fit, modelled jtcps3 K; fa = the fully associative bound; ins = instructions K):

| Commit | temple 444 | damselexpl 121 | drain 3 | drain 272 |
|---|---|---|---|---|
| 8b3d544 | 1,964.9 (fa 1,804.0, ins 950.0) | 802.7 (777.3) | 540.1 (520.8) | 2,004.2 (1,793.6) |
| 5e56d91 | 945.8 (864.4, 375.7) | 799.2 | 532.6 | 1,998.1 |
| 70a585c | 905.2 (859.4) | 793.6 | 531.6 | 2,066.6 (fa 1,796.1) |
| 1c25975 | | 787.2 (766.1) | 534.6 | 2,060.2 (fa 1,791.1) |
| e0ffcfe | 935.2 (879.3) | 787.2 | 531.6 | 2,043.0 |
| be4b575 | 884.8 | 785.3 | | 1,983.3 |
| aa29458 | 767.1 (699.0, 321.1) | 775.0 | 525.4 | 1,922.5 |
| 853cbfe | 196.3 (197.3, 94.5) | 790.6 (764.3) | 531.0 | 1,954.0 (1,759.9) |
| be35c8f | | 766.7 (752.6) | 525.2 | 1,896.5 (1,735.0) |
| f35d765 | 195.1 (197.3) | 765.2 (749.5) | 524.0 (510.7) | 1,888.5 (1,733.1) |

Base -> f35d765: temple 444 -90.1 %; damselexpl 121 -4.7 % (fa -3.6 %, instructions -2.6 %); drain 3 -3.0 %; drain
272 -5.8 % (fa -3.4 %, instructions -1.6 %). The 70a585c / 853cbfe rises on drain 272 / damselexpl come with fewer
instructions and a flat fa bound (layout). By function (base -> f35d765):
- pin_add self 3.1 -> 2.2 K a call (damselexpl 49.5 -> 35.6 K for 16; drain 272 79.1 -> 55.5 K for 25);
- olive_add 1.06 -> 0.67 K a call (drain 272: 40.8 -> 26.1 K for 39);
- evnz_sync 15.1 -> 8.8 K (drain 3), 9.7 -> 6.4 K (damselexpl);
- ev_create 17.0 -> 15.0 K (drain 272, 25 calls).

Temple 444 at f35d765: pcol_handle self 122 K (412 searchers, can_pair 0 each), tlist_remove 31 K, play_step 18 K.

**MAME** (SOFTFP playsh2, K; steps after step 1):

| Route | mean | max | > 150 K | > 175 K | step 1 |
|---|---|---|---|---|---|
| c_temple_xroom3x (8b3d544 -> f35d765) | 50.5 -> 49.2 | 871.7 @443 -> 171.9 @45 | 5 -> 3 | 1 -> 0 | 763 -> 721 |
| c_items_damselexpl | 108.4 -> 108.5 | 230.5 -> 225.9 | 29 -> 29 | 16 -> 15 | 908 -> 865 |
| c_swamp_drain | 110.0 -> 109.5 | 525.4 -> 522.2 | 33 -> 33 | 28 -> 28 | 1,399 -> 650 |
| default 27 routes | 67.0 -> 66.9 | | 112 -> 107 | 43 -> 38 | mean 1,289 -> 719, max 1,680 -> 1,359 |

c_temple_xroom3x step 443 (the transition room's first step): 871.7 K -> 357.9 K (be4b575) -> 308.5 K (aa29458) ->
107.3 K (f35d765). The room-change step before it (kind 4, the transition room's load: 8.77 M) takes the moved
work: 8.95 M. The explosion routes' MAME means hardly move: these changes cut data and literal misses, which MAME does
not count; jtcost and jtcps3 do. Default set at f35d765: 281 of 9,480 steps more than 1 % slower than base, the
largest p5_giant's (+6.6 %, steps of about 60 K; its mean 73.3 -> 70.2 K).

**Rebased onto main 365247a (moves merged), HEAD 58edd68.** The commits keep their order; two changed in the rebase:
- af8252b (was 2176d56): moves's NC_OFAM(o) already holds "slot e's family contains o", so olive_add drops
  NC_OFAM(obj) & nc_okm (nc_okm: bit e = nc[e].ok != 0) instead of comparing the parent walk with keys; nc_moved
  loops over those bits with a moving mask (no variable shift: a first version with `x >> e` cost lush steps
  +1.4 K MAME each); nc_get keeps its own ok test (obj and ok share a line).
- 17d41aa (was f35d765): direct_pairs eligibility by oinfo's OI_DIRECT (targets + rv list <= 8, exactly the objects
  of before 5e56d91). `rv_n <= 8` alone let objects with 9-13 targets and a short rv list take it (their target
  family walks: p5_giant steps +6.6 %).

| On main | temple 444 (fa, ins) | damselexpl 121 (fa, ins) |
|---|---|---|
| 365247a | 1,815.2 K (1,711.2, 910.3) | 744.2 K (726.2, 166.5) |
| 58edd68 | 132.5 K (132.2, 42.3) | 724.8 K (710.0, 162.0) |

MAME SOFTFP, steps after step 1 (365247a -> 58edd68): c_temple_xroom3x step 443 826.0 -> 46.0 K, max 826.0 -> 157.7 K,
> 175 K 1 -> 0; c_items_damselexpl mean 102.1 -> 102.2, max 206.6 -> 196.7; c_swamp_drain 101.7 -> 101.1, max 463.1 ->
452.5; c_jungle_firefrog 91.5 -> 89.1 (> 150 K 17 -> 15); c_swamp_vampkill 117.6 -> 116.4; c_swamp_grave 110.3 ->
110.0; level first steps 0.9-1.5 M -> 0.53-0.84 M. Default 27 routes: mean 64.5 -> 64.5 K, > 150 K 63 -> 60, > 175 K
7 -> 5, step 1 mean 1,294 -> 645 K; 283 of 9,480 steps more than 1 % slower, none more than 1.7 % (p4_spikes' 23 K
steps +350). Gates at 58edd68: hostident 184 / 184 against a 365247a baseline (also at af8252b); CTALL 59 / 59; EQUIV
88 / 88; playsh2 9,701 / 9,701 in both builds, 2,399 / 2,399 on the six routes; game_check p4_exit559 0 px; tests/game
.bss ends at 0x02076b50 (38,064 B of stack).

- **Gates (f35d765):** hostident 184 / 184 after every commit; CTALL 59 / 59 and EQUIV 88 / 88 at 853cbfe and
  f35d765; playsh2 9,701 / 9,701 in the SOFTFP and default builds at 853cbfe and f35d765; game_check p4_exit559 0 px
  (records 30, 150, 300); tests/game links with .bss ending at 0x020768a0 (38,752 B of stack). .bss: evkm / onzb
  +960 B, pa_* +12 B, nc_ko 8 B (.data); the pin_ext template (200 B) is gone outside PLAY_STATS builds.
- **Measured, not kept:** 5e56d91 alone made the oSolid family eligible for direct_pairs: its scan of the pending
  destroyed entries (has_col both ways per pend entry, per block) made a level's first step slower (MAME p5_l4 step 1
  1.40 M -> 2.54 M, p5_caveman 1.16 M -> 1.36 M); f35d765 keeps direct_pairs to the old bound (p5_l4 1.36 M).
- **Measured, not kept:** can_pair's answer kept for the last object while ocnt does not change (a generation
  bumped by obj_count): temple 444 195.1 -> 137.3 K modelled, MAME step 443 107.3 -> 53.8 K, but the default set's
  steps got slower (MAME: 1,288 of 9,480 steps more than 1 % slower against base, 281 without it; damselexpl 121
  +2.7 K modelled): the check and the generation's store on every can_pair and count change. Step 443 is under the
  budget without it.
- **What is left (measured, drain 272 at 853cbfe):** a debris creation costs about 11.5 K modelled: pin_add 5.9 K
  inclusive (self 2.3 K: 44 + 16 zero stores and the literal pool; olink 1.4 K; pcol_added 0.9 K, entry_clear half
  of it; ta_on 0.45 K; grid_dirty 0.3 K), pcol_create 4.1 K (cupdate 3.0 K: the box and pgrid_put; mark_e 0.7 K),
  ev_create 2.1 K (0.8 K of it the hooks be35c8f skips for debris). The rest of pin_add is one data miss in each of
  about eight per-instance link arrays (pw_inext, iprev, pw_anext, aprev, pw_ntnext, ntprev, taprev, pw_tanext,
  pw_seq, pw_ord, gcell, gond): a per-instance link record would be one line, but every list walk would read it.
- **Not done:** the detritus Steps' soft-float (target 3: oDrip's y / yVel adds, oBlood's vel_parts twice and its
  left / right queries, oBloodTrail).
