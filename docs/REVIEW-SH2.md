# External review: the play step against the jtcps3 SH-2 memory system

Status 2026-10-04, reviewer session, on main e264467 (grid build, SOFTFP=1, PIN_MAX 1792). Analysis and proposals
only: nothing in src/ was changed. Scratch material: `build/review_ext/` (git-ignored): `cachemodel.py`, the trimmed
playsh2 copy `spel/`, traces `spel/tests/playsh2/build/rx/trace/{a,b}.tr`.

## 1. Summary

- **The step is memory-bound on jtcps3, and MAME clocks do not see it.** A traced step (p4_exit559 step 301, 241,320
  instructions, about 230 K MAME clocks) run through a model of the SH7604 cache with jtcps3's measured latencies
  costs about **926 K jtcps3 clocks**. Of that:

  | Component | jtcps3 clocks | Share |
  |---|---|---|
  | Instruction execution (≈ MAME clocks) | 241 K | 26 % |
  | Instruction-fetch line misses (18,053 lines from SIMM 1) | 217 K | 23 % |
  | Data line misses: literal pools 4,695, main RAM 6,100 (stack 1,515, `inst_mem` 1,910), other SIMM 587 | 250 K | 27 % |
  | Stores: 27,943 at 6.06 each, **84 % of them to the stack** | 169 K | 18 % |
  | `mul.l` / `dmul` + `sts` stalls, taken branches | 48 K | 5 % |

  The step budget (PERF2.md "Target") is 0.525 M jtcps3 clocks. The traced step is 1.76x over.
- **The modelled ratio, 3.84 jtcps3 clocks per instruction, matches the measured grid ratio of 4.21**
  (EQUIV.md "jtcps3 timing", 18 routes, range 3.80-4.54). The model leaves out load-use stalls and fetch / data port
  conflicts, so it reads low. PERF2.md's target still converts with 3.41-3.6, and `tests/playsh2/report.py` with 2.94.
  Both overstate the headroom. At 4.2, the 0.525 M step budget is **125 K MAME clocks**, not 146-154 K.
- **The ratio rises as instruction-count cuts land** (exact 3.91 -> grid 4.21, EQUIV.md). The cuts so far remove
  instructions but leave misses and stores. The proposals below are ranked by modelled jtcps3 clocks, not MAME clocks.
- **The CPS3 has little to offload game logic to.** It has one SH-2 and no coprocessor. The on-chip DIVU has nothing
  to do in a step: 0 divides and 0 variable shifts in the traced step. The DMAC and the CPS3 DMAs move bytes, and a step
  moves few. The large gains are in data types, layout and calling patterns that suit a 4 KB write-through cache with
  6-clock stores. Section 4 lists where offload does pay: level start and the display list.

## 2. Hardware facts used (with sources)

| Fact | Value | Source |
|---|---|---|
| Cache | 4 KB, 4-way x 64 sets x 16 B lines, unified, pseudo-LRU, **write-through, write miss does not allocate, no write buffer** (CPU held until the write completes) | jtframe RTL `modules/jtframe/hdl/cpu/sh7604/CACHE.sv:164-185, 541-566, 660-661` |
| Store, any external area | 6.06 clocks | cps3-testgame `docs/CPS3.md` (ttest) |
| Cached load hit / uncached load main RAM / uncached SIMM 1 | 1.5 / 7.5 / 4.5 | same |
| Line fill | 4 separate 32-bit reads, CPU stalled. Sequential loads cost about 8 each from main RAM and about 5 each from SIMM 1, so a main-RAM miss (~30) costs more than a SIMM 1 miss (~20) | RTL `CACHE.sv:599-616`; maldita `docs/CPS3.md` mbench |
| Straight-line code from SIMM 1, every line a miss | 2.51 clocks an instruction (MAME 0.98) | maldita `docs/CPS3.md` |
| Two-way mode (CCR.TW) | 2 KB cache + 2 KB on-chip RAM at 0xC0000000: load and store 1.5 clocks. MAME maps only 1 KB there (`cps3.cpp:2170`). Code placed there must be stored encrypted | RTL `CACHE.sv:111-112, 289-321`; maldita `docs/CPS3.md` |
| MUL.L / DMULx.L | 4 cycles; `sts macl` right after waits: about 9 clocks a pair | RTL `MULT.sv:72-82`; maldita mbench |
| DIVU | 39 clocks, runs alongside the CPU | RTL `DIVU.sv:83-136` |
| DMAC | 2 channels; 16-byte units 0.77 clocks a byte (CPU stores: 1.5 a byte) | RTL `DMAC.sv`; cps3-testgame `docs/CPS3.md` |
| MAME | 1 clock per instruction, no cache, no wait states, DIVU instant | MAME `sh2.cpp:127-187, 327`, `sh7604.cpp:1139-1160` |
| SH-2 addressing | `mov.l @(disp,Rn)` reaches 60 bytes; `mov.w` 30 bytes and `mov.b` 15 bytes, **and only through R0**; `@(disp,GBR)` reaches 1020 / 510 / 255 bytes through R0; a global's address costs one PC-relative literal load (a data read from SIMM 1) | SH-2 ISA; verified in the GCC 13.3 output below |

## 3. Method

1. `build/review_ext/spel`: a copy of HEAD with `tests/playsh2/mkjobs.py` cut to the jobs p4_exit559 (enemies off) and
   p5_snakes (enemies on). `playsh2_check.sh` with SOFTFP=1: 1050 / 1050 checksums equal. Step means: 198,851 and
   271,842 MAME clocks.
2. MAME debugger trace (`-nodrc -debug -debugger none`, `noloop`, r0-r15 and PR logged on every instruction). The
   window opens and closes on write taps on the record counter. `cachemodel.py` keeps only the instructions from
   `play_step`'s entry to its return. It rebuilds each load's and store's address from the registers and runs them
   through the cache above (LRU in place of pseudo-LRU). Costs per event:
   - instruction 1;
   - fetch miss +12 (fitted to 2.51 an instruction);
   - load hit +0.5;
   - load miss +27.5 from main RAM, +15.5 from SIMM 1;
   - store 6.06;
   - `mul` pair +7;
   - jump +2.
3. What-if switches in the model: WAYS=2 (two-way mode), CRAM_STACK=1 (stack accesses at cache-RAM cost), NOLIT=1
   (literal loads removed).

## 4. Findings and proposals, ranked by modelled jtcps3 gain

### P1. Measure in jtcps3 clocks (enabler, do first)

- **Observed:** MAME-clock targets misjudge both the gap and each cut's value. Two measured examples:
  - -Os was dropped for +6.0 % in MAME, but it cost only +2.4 % on jtcps3;
  - the grid saved 21.9 % in MAME but 15.9 % on jtcps3.
- **Action:**
  - Make a trace-plus-model run (the `cachemodel.py` method) a standard report for a few representative steps per
    batch, alongside the MAME clocks.
  - Calibrate it once against `scripts/playsh2_jt.sh` on the same build. Put the 4.2 ratio in PERF2.md's target and in
    report.py.
  - Rank every remaining PERF2 item by modelled jtcps3 clocks.

### P2. Cut stack traffic: calls, saved registers, out-parameters (about −8 to −12 %)

**Observed (step 301).**
- Stack stores and misses:
  - 23,353 stack stores (84 % of stores);
  - 15,146 of them are prologue register saves (`mov.l Rn,@-r15`, `sts.l pr,@-r15`) over 4,271 calls;
  - the rest are locals and out-parameters.
- Stack misses: 1,515 line misses on stack *loads*. A push does not allocate a line (write miss, no allocate), so the
  matching pop often misses: about 30 clocks.
- Stack traffic in total: about 160 K jtcps3 clocks, 17 % of the step.
- The worst offenders are small layered collision helpers, called 100-200 times a step:

| Function | Calls | Instructions a call | Pattern |
|---|---|---|---|
| `pos_int` | 205 | 34 | float -> int position decode |
| `pin_xy_int` | 143 | 54 | results through pointers |
| `ibounds` | 107 | 62 | 6 saved regs + PR; 4 results stored through pointers; 2 `mul.l` (x72, x200) |
| `bbkind` | 101 | 67 | |
| `collision_line_any_i` | 94 | 284 | **44 stack stores a call**: `struct qctx c` filled on the stack before `line_summary` answers from 1-3 cells |
| `query_e` / `flush` / `grid_flush` | 113 / 112 / 103 | 25-54 | |

**Proposals.**
1. **Return small results in registers.**
   - GCC 13.3 for SH returns an 8-byte struct through memory: 4 stack stores, then reloads.
   - The same data as a `uint64_t` (four int16, or two int32) comes back in r0:r1 with no memory traffic. Observed in
     `build/review_ext/t/ret.c`.
   - Apply this to ibounds / pin_xy_int / pos_int / ebbox_* / pin_ibox.
2. **Answer fast paths before building contexts.**
   - collision_line_any_i can run line_summary on the four ints in registers.
   - It then fills `struct qctx` only for the line_scan path, which is 2 % of queries (PERF2 B: 29 % hits, 71 % sure
     misses).
3. **Unity build of src/game (one translation unit).**
   - The cps3-dev GCC is built `--disable-lto` (observed: "LTO support has not been enabled"), so cross-file inlining
     and `-fipa-ra` cannot work today. One TU (`#include` the .c files) gives both.
   - With IPA-RA, a caller keeps values in r1-r7 across a call to a callee known not to clobber them, which removes
     saves.
   - Then mark the decode helpers `always_inline`.
   - Watch the code size: measure the I-misses with the model.
4. **Fewer soft-float calls in branchy code.**
   - The binary has 8,529 call sites to libgcc soft-float helpers (of 14,989 `jsr` sites). The binary is today's
     playsh2 jt build in `build/draww`. The most common: `__eqdf2` 1,287, `__extendsfdf2` 1,082, `__gedf2` 857,
     `__nesf2` 393.
   - Comparisons and float->double widening are integer bit operations: 4-10 inline instructions for non-NaN operands,
     with ±0 handled.
   - Every removed call frees the caller of the r0-r7 / PR clobber, so more functions become leaves: no PR save, fewer
     callee-saved registers.
   - Exactness: identical results whenever NaN cannot occur. A host assert build can check the operands.

**Not recommended: the stack in cache RAM (two-way mode).** Modelled on this step:
- the 4-way cache: 926 K;
- two-way with the stack in cache RAM: 918 K. Halving the cache adds 7 K I-misses and 2 K D-misses, which cancel the
  stack saving.

Also:
- the step's stack peaks at 1,684 bytes, which does not fit MAME's 1 KB mapping, so it cannot be verified in MAME;
- Maldita's attempt failed on jtcps3 for an unknown cause (maldita docs/CPS3.md).
- The stack saving is real (CRAM_STACK with 4 ways: 759 K, −18 %), so it has to come from software (1-4 above).

### P3. Hot globals through GBR; fewer literal-pool loads (about −4 to −6 %; upper bound −11 %)

**Observed.**
- Literal-pool loads (`mov.l/mov.w @(disp,PC)`) are about 9 % of the step's instructions (21 K in the first 300 K
  traced).
- Their lines miss 4,695 times a step, 89 % of SIMM data misses.
- Each global access is a literal load (the address) plus the access.
- With NOLIT=1 the step models at 826 K (−11 %).
- GBR is unused in the SDK and the game (grep).
- GCC 13.3 compiles `((struct hot *)__builtin_thread_pointer())->f` to a single `mov.l @(disp,GBR),r0`, with no
  literal (`build/review_ext/t/gbr.c`).

**Proposal.**
- Gather the per-step globals into one `struct hot` (≤ 1020 bytes, int32 fields first): PW's scalars, play_time, the
  grid / list heads, the `pcol_st` state, the counters.
- Set GBR once at boot. The SDK's ISRs must not change it: check crt0.S and isr.S.
- Large arrays stay where they are; their base addresses can sit in `struct hot` as pointers (one GBR load in place of
  one literal load).

### P4. Code working set (about −8 to −10 %, overlapping P2 and P3)

**Observed.**
- The step touches 2,148 distinct code lines (33.6 KB, 210 functions) but takes 18,053 fetch misses: 8.4x the
  compulsory count.
- 32 K of the 47 K line entries are reused within 256 lines (they would hit in a 4 KB fully associative cache). 13 K
  are reused at 8-16 KB distance: per instance, the event code calls the same collision / soft-float chain, and the
  chain plus the event code does not fit.
- Largest code footprints touched: `pl_step` 3.7 KB, `ev_step` 2.7 KB, `item_step` 1.6 KB, `play_step` 1.4 KB,
  `jar_step` 1.1 KB.
- ev_step calls pen_step, pdam_step, pshop_step and pitem_step on every one of its 58 calls (232 calls a step).

**Proposals.**
1. **A compact integer collision kernel.** One leaf routine for the axis-line / point / rect existence tests over
   int16 boxes, with no float decode, under about 1 KB. It replaces the chain collision_line_any_i -> line_summary /
   pgrid_search -> ibounds -> pin_xy_int -> pos_int -> bbkind. This needs P5's int positions.
2. **PERF2 D (direct `step_fn[obj]`).**
   - It removes the hook chain's code and the stack frames per event.
   - Under P1, D is worth more than PERF2's 15-20 K MAME estimate.
3. **Hot / cold splitting inside the large event functions.**
   - Move rare branches (damage, death, shop, item pick-up) into `__attribute__((cold, noinline))` helpers, so the
     common path of `pl_step` / `characterStepEvent` is contiguous lines.
   - `-freorder-blocks-and-partition` needs profile data, so do it by hand with `__builtin_expect` and cold helpers.
4. **Not recommended:** whole-program hot-first layout (measured +0.5 % on jtcps3), CCR.OD (measured worse in Maldita)
   and -Os everywhere (measured +2.4 %).

### P5. Data types and layout for 16-byte lines and SH-2 displacements (about −6 to −10 %, and main RAM)

**Observed.**
- `struct pin` is 72 bytes:
  - the stride costs a `mul.l` (3,584 multiplies in the step, about 25 K clocks);
  - `type` is at offset 56 and `bl..bbk` at 62-70, past `mov.w`'s 30-byte reach.
- `struct pin_ext` is 200 bytes:
  - `held` is at offset 96, the `col*` flags at 114, `state` / `status` at 138 / 140, `lbo..bbo` at 188;
  - each access is 3 instructions (literal offset, add, load). Seen in `ibounds`.
- `inst_mem` (1,792 x 72 = 129 KB) takes 1,910 line misses a step, about 53 K clocks. They come from play_step's sweeps
  over every slot.
- Main RAM is full:
  - .bss 430 KB of 512;
  - the tests/game build moves `W`, `rn`, `er`, `gtiles` and the draw maps into sprite RAM (`tests/game/sprbss.ld`;
    the comment says this is not checked on jtcps3);
  - the post-merge stack overflow blocker.
- Not observed: data "spilling" into the SIMMs. Constant tables in SIMM 1 cost less per miss (~20) than main RAM
  (~30).

**Proposals.**
1. **Hot record ≤ 32 bytes, power-of-two stride.** It holds:
   - obj, alive / flags;
   - x, y as int16 plus a whole-number bit (PERF2 C);
   - the int16 box;
   - ext / en indices.

   The float x / y, depth, image fields and scales go to a cold parallel array, read only by the setters, the float
   paths and drawing. Field order: int32 first (≤ 60 bytes), then int16 (≤ 30), then bytes (≤ 15).
2. **Reorder `pin_ext` by use.**
   - First: the flags read every step (`held`, `col*`, `stuck`, `state`, `status`, `counter`), as one uint32 bitmask
     for the booleans.
   - Then the binary64 velocities.
   - Last: the rarely used fields.
3. **Struct-of-arrays for the per-step sweeps.** Run the sweeps (xprevious, alarms, animation) over dense arrays of
   the non-terrain instances (≈120 of ~800). This is PERF2 E, done as layout rather than as skip lists.
4. **Terrain out of `struct pin`.**
   - PLAN §1 decided "terrain as a grid, not instances", but terrain still holds 70-80 % of the slots.
   - A per-cell record (id, creation number, object: 8 bytes x 2,048 cells = 16 KB) can replace up to ~1,400 x 72 B
     slots. A full slot is materialised only when GML code takes the instance (destroy, `with`, a returned id).
   - It shrinks every sweep and frees about 80-100 KB of main RAM: the stack blocker and the sprite-RAM spill.
   - Exactness hinge: ids and creation order (`with (oSolid)` newest first, oldest-first ties) must come from the cell
     records in the same order. Prove it per query family, as A's proof does.

### P6. Number types: keep binary64 results, change their representation (overlaps P2 and P5)

- Fixed point stays rejected (PLAN §1: .5 ties in round(1/frac)). The cost on this trace is not the arithmetic,
  though:
  - soft-float functions are 13 % of instructions and 7.5 % of the jtcps3 model (tight leaf code, ratio 2.2);
  - the cost is the decode at the interfaces: pos_int, pin_xy_int, bbkind, ibounds, ebbox_*, dwhole and
    `__extendsfdf2` (195 calls) together are about 125 K jtcps3 clocks.
- **Proposal:** int16 x / y and a whole flag written by the setters (PERF2 C), plus an integer box cached per
  instance, updated only on a real change. Collision and drawing then read ints only.
- Floats stay the source of truth, so every result is unchanged by construction: the int is used only when the flag
  says the float is that int.

### P7. Offload where it pays (outside the mean step)

- **Level start / room change** (2.0-2.8 M, up to 21 M clocks): clears and bulk copies (inst_mem, grids, the
  generator's W into the play slots) via the DMAC in 16-byte units. That is 0.77 clocks a byte and runs alongside the
  CPU, against 1.5 a byte for stores. Combine this with splitting the room start over frames.
- **Display list:** build the entries in main RAM, then one DMAC 16-byte-unit transfer to sprite RAM in place of about
  550 CPU stores (6 clocks each). Small (about 3 K clocks a frame), but it also frees the CPU while the transfer runs.
- **DIVU / MAC:** nothing to offload in the step (0 divides traced).
  - `mul.l` + `sts` pairs (3,584) lose about 7 clocks each.
  - Keep strides power-of-two (P5).
  - Where a multiply stays, put independent instructions between `mul.l` and `sts macl`. The jtcps3 MAC overlaps
    (RTL `MULT.sv:167`).

## 5. Expected effect and order

Modelled on step 301. The estimates overlap and are not additive:

| Order | Item | Modelled jtcps3 change | Exactness argument |
|---|---|---|---|
| 0 | P1 cost model in the gate report; ratio 4.2 in the targets | none | |
| 1 | PERF2 A (rest skip, pending merge) | −47 K MAME ≈ −180 K | proved per object (PERF2) |
| 2 | P6 + PERF2 C: int positions and int box cache | −90 to −110 K | int used only when equal to the float |
| 3 | P2 1-2 (register returns, no context struct on fast paths) + P4 1 (kernel) | −80 to −120 K | same results; colprobe / pcolxv |
| 4 | P3 GBR globals | −40 to −55 K | code generation only |
| 5 | P2 3-4 (unity build, inline compares) | −40 to −80 K, code size to watch | identical results with no NaN (host assert) |
| 6 | P5 1-3 (hot 32 B record, pin_ext order, SoA sweeps) + PERF2 D / E | −60 to −90 K | layout only |
| 7 | P5 4 (terrain out of pin) | RAM −80-100 KB; sweeps | ids and order proof per query family |

From 926 K, items 1-6 project to about 0.45-0.55 M against the 0.525 M budget. So the step fits only with most of them
in. Item 7 is needed for RAM regardless.

**Unknown:**
- other routes and areas (two steps traced, section 6);
- level-start steps (not traced);
- the draw's own jtcps3 profile (0.28 M is its PERF2 allowance, unmeasured by this model);
- the model's error against jtcps3 for this build. A `playsh2_jt.sh` run of e264467 settles the calibration.

## 6. Second trace: p5_snakes step 956 (enemies on)

The window was closed by a breakpoint on play_step's return (0x06001DA6). The picture is the same as p4_exit559's.

| | p4_exit559 step 301 | p5_snakes step 956 |
|---|---|---|
| Instructions (≈ MAME clocks) | 241,320 | 272,435 |
| Modelled jtcps3 clocks | 926 K (ratio 3.84) | 969 K (ratio 3.56) |
| Fetch line misses | 18,053 | 17,843 |
| Data line misses: main RAM / SIMM (of which literal pools) | 6,100 / 5,282 (4,695) | 6,056 / 5,119 (4,549) |
| Stack-load misses / `inst_mem` misses | 1,515 / 1,910 | 1,465 / 2,050 |
| Stores (stack share) | 27,943 (84 %) | 29,603 (87 %) |
| Collision group: share of the model | 53.8 % | 50.4 % |
| Soft-float group: share of the model (instructions) | 7.5 % (13.0 %) | 10.8 % (20.6 %) |
| What-if: two-way mode, stack in cache RAM | −0.9 % | −0.8 % |
| What-if: stack free, 4-way cache kept | −18 % | −18 % |
| What-if: no literal loads | −11 % | −10 % |

Soft-float is larger on the enemy step: `__adddf3` runs 21 K instructions, against 9 K on p4. Its jtcps3 ratio is the
lowest of any group (1.7-1.9): tight leaf code. P6's point stands. Make the float interfaces cheaper (decode,
compares, calls); do not replace the float arithmetic.
