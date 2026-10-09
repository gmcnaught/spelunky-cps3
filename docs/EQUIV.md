# Gameplay equivalence: does a Maldita-style gate unlock speed?

## The shipping build (adopted, PLAN §1, 0ec4e16): the newest-first collision grid

**Code**
- **The grid:** `src/game/pcolgrid.h`, included once by pcol.c. pcol.c only carries the hooks: `PCOL_GRID_ON`,
  `pgrid_put` / `pgrid_out` / `pgrid_clear` / `pgrid_search` / `pgrid_load`.
- **How it works:** during play, the entries the runner's R-tree would hold (same set, same rectangles, same put-in
  / take-out points) sit in a grid of 16 px cells. A search returns them newest first.
- **Deferred entries (since branch moves):** a stale entry of an object no query has asked for this room waits on a
  deferred list (pcol.c `fhead`) and goes in when a query asks for its family (`ask_fam`) or it can make a pair
  (`flush_pairable`). Its rectangle reaches no answer before that (the callbacks drop other families; pairs need
  `can_pair`), and the hits' order is by creation number, so the answers and their order are the same. PERF3.md,
  the section of branch moves. Since branch aftermath a new entry of such an object that cannot pair at its creation
  waits there from the start (pcol.c `pcol_create`): the drain's drips and rubble never go in.
- **Generator:** keeps the tree (bit-exact).
- **Exact build:** `-DPCOL_EXACT` gives the tree in play too. It is the translation reference.

**Which build uses what**
- **Grid (default):** the SH-2 game program and tests/playsh2 / gametime / game.
- **Exact:** every test/host binary (`playhost`, `_nc`, `_fx`, `_count`, `colprobe`, `pcolxv`, `extchk`,
  constcheck), via `-DPCOL_EXACT` in test/host/Makefile's INC.
- **Equivalence pair:** `build/host/playhost_grid` (grid) and `build/host/playhost_eq` (exact). Both are built with
  `PLAY_RNGLOG` (Q lines: RNG state per record).
- **SH-2 scripts:** `EXACT=1` builds the tree and `GRID_SHIFT=<n>` sets the cell size. Both work with
  scripts/playsh2_check.sh, playsh2_jt.sh, gametime_check.sh and game_check.sh.

**Gate:** `scripts/equiv_check.sh [route-regex]` runs from any checkout (worktrees included).
- **Exact check:** the exact build must be record-equal to every P1 / P4 / P5 trace. A c_* route that is not
  (untranslated content) is skipped.
- **Grid check:** the grid build must pass tools/equivcheck.py's route check and state gate.
- **Accepted differences:** listed in tests/equiv_accept.txt by route and first gbag record.
- **Result:** EQUIV 35/35 (23 P routes plus 12 exact-equal c_items routes; 21 c_* skipped). p4_bomb_throw_s3 is
  accepted at gbag 172: explosion debris order, gold resting positions, money equal.
- **Exact build:** unchanged. scripts/p5_regress.sh gives 18/18 ROUTE equal, every route record-equal, with both
  playhost and playhost_nc.

**Cell size on the SH-2 (MAME, SOFTFP=1, 70 jobs, 6,635 / 6,635 checksums equal in every run)**

| Build | Mean step | Median | p99 |
|---|---|---|---|
| exact (tree) | 381,714 | 336,864 | 1,534,784 |
| grid 64 px | 328,142 | 301,344 | 775,904 |
| grid 32 px | 297,179 | 273,600 | 671,360 |
| **grid 16 px (default)** | **291,157 (-23.7 %)** | 269,888 | 637,120 |

8 px was not run: a 16 px block would span 3 cells and go to the big list.

The cleanup alone (32 px, same cells as the measurement build) took the grid from 307.6 K to 297.2 K. Its parts:
- integer cell coordinates for integer rectangles;
- no relink when an entry stays in its cell;
- creation numbers cached for the sort;
- the overlap test inlined for integer rectangles.

Per route, mean step in MAME clocks:

| Route | Exact | Grid 16 px | Change |
|---|---|---|---|
| p4_exit559 | 304,419 | 227,038 | -25.4 % |
| p4_hang_ladder | 319,513 | 261,787 | -18.1 % |
| p4_items | 313,325 | 260,811 | -16.8 % |
| p4_spikes | 218,009 | 187,682 | -13.9 % |
| p4_push_rope | 321,871 | 257,796 | -19.9 % |
| p1_walk | 382,699 | 293,964 | -23.2 % |
| p4_bomb_drop | 466,126 | 328,188 | -29.6 % |
| p4_bomb_throw | 353,400 | 246,105 | -30.4 % |
| p5_buy | 435,283 | 358,877 | -17.6 % |
| p5_caveman | 534,674 | 389,359 | -27.2 % |
| p5_cavestun | 401,671 | 311,044 | -22.6 % |
| p5_giant | 443,016 | 335,505 | -24.3 % |
| p5_idol | 398,035 | 328,216 | -17.5 % |
| p5_l3spider | 331,416 | 266,063 | -19.7 % |
| p5_l4 | 444,488 | 339,646 | -23.6 % |
| p5_reg_l2s10 / l3s10 | 486,280 | 344,420 | -29.2 % |
| p5_reg_l4s10 | 385,827 | 216,341 | -43.9 % |
| p5_shop | 410,905 | 321,962 | -21.6 % |
| p5_snakes | 412,835 | 341,729 | -17.2 % |
| p5_spider | 337,847 | 275,828 | -18.4 % |

Max step: about 21 M in both builds, the level's first step or a room change. jtcps3 below.

### jtcps3 timing (MiSTer .81, scripts/playsh2_jt.sh, 2026-10-04)

The builds were made before the grid's rebase onto 03f01c5: 899ce76 (grid and exact) and 01a2559 (-Os), both on main
0ec4e16, so without perf1's soft-float cuts of 64a7b11 / item 6 and without HandleCollision's inlined search (the
rebased hashes faabac9..6e96383 contain those and measure lower in MAME); main RAM only, PIN 1000:
- **exact:** `EXACT=1`;
- **grid:** the default (16 px cells);
- **-Os + O2 hot:** the grid built `OPT=-Os` with pcol.c, pworld.c, prun.c, pscript.c, pobj.c and pplayer.c kept at
  O2 by `#pragma GCC optimize("O2", "no-tree-loop-distribute-patterns")` (`O2FILES`).

All three: PASS 23 / 23 checksums on jtcps3 (5 generations + 18 routes), sprite RAM self-test OK. The PIN_MAX A / B
host check of playsh2_jt.sh differs on 15 jobs in every build: since slot reuse (286acc7) the checksum covers the slot
layout, which depends on PIN_MAX; it now reports instead of stopping (STRICT=1 stops).

Step mean per route, jtcps3 clocks (MAME clocks of the same build, jtcps3 / MAME ratio):

| Route | exact jt (MAME, ratio) | grid jt (MAME, ratio) | -Os+O2 hot jt (MAME, ratio) |
|---|---|---|---|
| p4_exit559 | 1,184,482 (304,424, 3.89) | 965,833 (227,036, 4.25) | 980,069 (238,639, 4.11) |
| p4_hang_ladder | 1,272,611 (319,511, 3.98) | 1,102,892 (261,793, 4.21) | 1,118,414 (275,115, 4.07) |
| p4_items | 1,242,224 (313,324, 3.96) | 1,088,794 (260,815, 4.17) | 1,105,040 (274,281, 4.03) |
| p4_spikes | 921,665 (218,010, 4.23) | 831,647 (187,682, 4.43) | 844,904 (194,951, 4.33) |
| p4_push_rope | 1,310,074 (321,864, 4.07) | 1,130,507 (257,793, 4.39) | 1,142,558 (268,697, 4.25) |
| p1_walk | 1,508,011 (382,698, 3.94) | 1,249,884 (293,963, 4.25) | 1,266,343 (309,121, 4.10) |
| p4_bomb_drop | 1,789,307 (466,126, 3.84) | 1,392,827 (328,193, 4.24) | 1,409,408 (344,921, 4.09) |
| p4_bomb_throw | 1,415,537 (353,396, 4.01) | 1,118,502 (246,104, 4.54) | 1,126,213 (256,114, 4.40) |
| p5_buy | 1,587,741 (435,286, 3.65) | 1,364,761 (358,876, 3.80) | 1,420,036 (384,421, 3.69) |
| p5_caveman | 2,076,744 (534,672, 3.88) | 1,673,349 (389,360, 4.30) | 1,726,056 (418,574, 4.12) |
| p5_cavestun | 1,553,743 (401,673, 3.87) | 1,311,003 (311,046, 4.21) | 1,357,106 (335,188, 4.05) |
| p5_giant | 1,697,096 (443,016, 3.83) | 1,398,571 (335,504, 4.17) | 1,434,490 (357,915, 4.01) |
| p5_idol | 1,552,314 (398,034, 3.90) | 1,356,086 (328,218, 4.13) | 1,396,013 (350,805, 3.98) |
| p5_l3spider | 1,352,869 (331,419, 4.08) | 1,160,485 (266,065, 4.36) | 1,194,243 (282,086, 4.23) |
| p5_l4 | 1,693,775 (444,488, 3.81) | 1,406,909 (339,647, 4.14) | 1,445,015 (360,108, 4.01) |
| p5_shop | 1,607,752 (410,904, 3.91) | 1,352,073 (321,960, 4.20) | 1,398,955 (343,099, 4.08) |
| p5_snakes | 1,659,737 (412,837, 4.02) | 1,453,039 (341,726, 4.25) | 1,499,256 (364,206, 4.12) |
| p5_spider | 1,284,445 (337,849, 3.80) | 1,107,134 (275,828, 4.01) | 1,134,450 (291,535, 3.89) |
| mean of route means | 1,483,896 (379,418, 3.91) | 1,248,016 (296,200, 4.21) | 1,277,698 (313,876, 4.07) |

- **Grid against exact on jtcps3:** -15.9 % mean (1.484 M -> 1.248 M), against -21.9 % in MAME. The ratio rises from
  3.91 to 4.21: the grid's work is cheaper in instructions but not in cache misses / memory waits.
- **-Os + O2 hot against O2 grid:** +2.4 % on jtcps3 (1.248 M -> 1.278 M), +6.0 % in MAME. The smaller code improves
  the ratio (4.21 -> 4.07) but not enough to win: O2 stays.
- **Budget:** 0.84 M jtcps3 clocks per 2-frame step. The grid build's route means are 0.83-1.67 M: only p4_spikes is
  under.

**Memory (PIN 1000):** 16 KB in all.
- cells: 65 x 49 int16;
- per entry: next / prev / cell, plus the sort's buffers, int16 x PIN_MAX each.

## Update: on top of perf1's exact cuts (main 219ed30)

This branch is rebased onto 219ed30, which includes fd02de6's bit-exact cuts: grid existence queries, in-place
leaf removal, integer R-tree keys and the terrain animate list. The comparison below is the newest-first grid
(`-DPCOL_CREATION=4`) against 219ed30's exact build.

Setup:
- **Host:** the same 23 routes. The exact build is still record-equal to the reference traces on all 23.
- **SH-2:** tests/playsh2, SOFTFP=1, PIN=1000, 70 jobs. Both runs had 6,635 of 6,635 checksums equal.

Host gates: the same as before the rebase.
- **Route check:** 23 of 23.
- **State gate:** 22 of 23. The failure is p4_bomb_throw s3: gold chunk resting positions from record 172, money
  equal.
- **RNG:** no divergence on any route.

**What the grid saves on top of the exact cuts: about 74 K MAME clocks a step (-19.4 %).**

| | Mean step | Median | p99 | Max (level start / room change) |
|---|---|---|---|---|
| 219ed30 exact | 381,714 | 336,864 | 1,534,784 | 20.8 M |
| 219ed30 + newest-first grid | 307,634 (-74.1 K, -19.4 %) | 284,608 | 671,936 | 21.3 M |

The budget is about 195 K for the step, so the grid leaves about 113 K still to remove.

Per route, mean step in MAME clocks:

| Route | Exact | Grid | Change |
|---|---|---|---|
| p4_exit559 | 304,419 | 242,256 | -20.4 % |
| p4_hang_ladder | 319,513 | 272,680 | -14.7 % |
| p4_items | 313,325 | 271,018 | -13.5 % |
| p4_spikes | 218,009 | 195,518 | -10.3 % |
| p4_push_rope | 321,871 | 271,087 | -15.8 % |
| p1_walk | 382,699 | 306,864 | -19.8 % |
| p4_bomb_drop | 466,126 | 341,711 | -26.7 % |
| p4_bomb_throw | 353,400 | 261,089 | -26.1 % |
| p5_buy | 435,283 | 375,592 | -13.7 % |
| p5_caveman | 534,674 | 417,294 | -22.0 % |
| p5_cavestun | 401,671 | 328,454 | -18.2 % |
| p5_giant | 443,016 | 353,829 | -20.1 % |
| p5_idol | 398,035 | 345,848 | -13.1 % |
| p5_l3spider | 331,416 | 284,698 | -14.1 % |
| p5_l4 | 444,488 | 364,089 | -18.1 % |
| p5_reg_l2s10 / l3s10 | 486,280 | 363,294 | -25.3 % |
| p5_reg_l4s10 | 385,827 | 231,188 | -40.1 % |
| p5_shop | 410,905 | 343,761 | -16.3 % |
| p5_snakes | 412,835 | 362,924 | -12.1 % |
| p5_spider | 337,847 | 292,754 | -13.3 % |

The max column is the level's first step in every route except p4_exit559, where it is the room change. Excluding
those, the largest step is 4.47 M exact and 4.78 M with the grid.

### Soft-float share

Measured with PROF=512, PROF_SKIP=4 and PROF_WRAP=1, which charges each soft-float call to its caller.

| | 219ed30 exact | + grid |
|---|---|---|
| Soft-float, share of all route samples | 21.5 % | 25.4 % |

The share rises with the grid only because the tree work is gone; the soft-float calls themselves are the same.

Where the soft-float time goes (exact build, share of all samples):
- **Collision geometry:** most of it. collision_point_p 3.7 %, rq_init 1.9 %, rcomb_growth_slow 1.2 %, ebbox
  1.2 %, overlap_at 1.0 %, collision_rect_p 0.6 %, pin_bbox 0.6 %.
- **Velocity code:** small. moveTo 1.9 %, gameStepEvent 1.1 %, characterStepEvent 0.6 %.

So fixed-point velocities would still save little; the remaining soft-float is in the bounding-box and rectangle
geometry. The earlier sections below were measured at 8479846, before perf1.

Status 2026-10-04. Measured on src/game at 8479846, plus this branch's measurement flags. Every option sits behind
a -D flag, and the default build is unchanged.

Host side: playhost variants compared with the exact build over 23 routes. These are every route with a reference
trace that the exact build matches record for record: p1_walk, the p4_* routes (both bomb_throw seeds, darkexit)
and the p5_* routes (the three reg routes included). The c_* routes are left out because none is translated yet:
the exact build matches only 0-93 of their records, with untranslated codes 1000-5004.

SH-2 side: tests/playsh2 in MAME with SOFTFP=1 and PIN=1000, 70 jobs, checked against the variant's own host
build. Every run had all 6,635 of 6,635 checksums equal.

Tool: `tools/equivcheck.py <exact.txt> <variant.txt>`. Both outputs come from builds with -DPLAY_RNGLOG.

## Answer

| Option | Route check (of 23) | State gate (of 23) | SH-2 mean step, MAME clocks (p99) |
|---|---|---|---|
| Exact (baseline, this tree) | 23 | 23 | **503.6 K** (1.55 M) |
| Fixed point s7.24 / s13.18 (Maldita F16) | 18 | 5 | 492.2 K, -2.3 % (1.49 M) |
| Fixed point s7.26, Q32.32, Q40.40, Q16.48 positions s.24 | 18-20 | 0-5 | not run (at most the s7.24 gain) |
| Velocities binary64, positions s13.18 | 23 | 23 | 505.1 K, +0.3 % (1.56 M) |
| Collision order: creation order, no R-tree (grid) | 23 | 21 | **383.7 K, -23.8 %** (0.79 M) |
| Collision order: newest first, no R-tree (grid) | 23 | 22 | **383.4 K, -23.9 %** (0.79 M) |
| 1.1's instance_deactivate_region (option 5) | 23 | 0 (in view: 20) | 503.9 K, +0.1 % (1.57 M) |
| Deactivation + newest-first grid | 23 | 0 (in view: 13) | 393.3 K, -21.9 % (0.84 M) |

The route check and the state gate are defined in §4.

**Fixed point does not unlock speed in this port.**
- Soft-float is now about 6.5 % of a step, after P4's own soft-float and integer fast paths.
- The velocity arithmetic is about 440 binary64 operations a step (playhost_count, steps 2 and on, 23 routes): 205
  compares, 216 conversions, 11 adds, 4.5 multiplies and 2.6 divides.
- Replacing all of it with s7.24 saves 11 K clocks a step.
- No format passes the gate at any precision (§1).

**The collision order is the lever.** Replace the R-tree emulation with a grid that returns hits in creation order,
or newest first. Either passes the route check on every route and takes 120 K clocks off the mean step. Newest
first is closer to the R-tree's order: the RNG state never diverges, and the id-free state differs on one route
only.

**Deactivation as 1.1 wrote it has no net effect.** Per-step scan cost and savings are equal (§3).

## 1. Fixed-point formats

Two flags:
- `-DPLAY_FIXED`: s7.24 / s13.18, as before.
- `-DPLAY_FX2` (src/game/pnum.h):
  - `FX_NFB` / `FX_PFB`: fraction bits.
  - `FX_T64`: int64 storage, with __int128 products.
  - `FX_NUMDBL`: binary64 velocities with fixed positions.
  - `FX_EPS168`: the compare epsilon as PLAY_FIXED had it. It changed no result.

### Harness bugs fixed first

Each of these had changed the earlier s7.24 result. The fixes leave the exact build unchanged: all 23 routes are
still record-equal to the reference traces.
- **Double evaluation:** N() / ND() / P() evaluated their argument twice. `ND(-1 - prandom(2))` (oFlame / oBlood
  Create) drew the RNG twice. Now the argument is evaluated once (fx_cvt).
- **Overflow in the level loader:** prun.c's `NMUL(NI(g->xvel), N(1.0 / 256))` overflows s7.24 once |xvel| is 128
  or more (0.5 px). A snake's 2.5 px start velocity came out as -0.5. Now `ND(g->xvel / 256.0)`, which is the same
  double in the exact build.
- **Raw casts:** `(pos)(double expr)` casts took a double as raw fixed-point units. Sites: penemy.c (blood, splash,
  oEnemySight's built-in motion) and pshop.c (blood). Now `P(...)`, identical in the exact build.

### Results

Each cell is the first record of each kind of difference against the exact build: rng (the RNG state differs) /
discrete (by id) / route check.

| Route | s7.24 | Q40.40 (int64) |
|---|---|---|
| p1_walk | - / 314 / pass | - / - / pass |
| p4_bomb_drop | 311 / 90 / pass | 311 / 90 / pass |
| p4_bomb_throw s3 | - / - / pass | - / - / pass |
| p4_bomb_throw s7 | - / 184 / pass | - / - / pass |
| p4_darkexit | 766 / 178 / FAIL (exit entered at 772, reference 765) | 766 / 688 / FAIL (same) |
| p4_exit559 | 467 / 389 / FAIL (exit entered at 466, reference 474) | 467 / 389 / FAIL (same) |
| p4_hang_ladder, p4_items, p4_push_rope | - / - / pass | - / - / pass |
| p4_spikes | 62 / 62 / pass | 62 / 62 / pass |
| p5_buy | 140 / 104 / pass | 140 / 104 / pass |
| p5_caveman, p5_cavestun | - / - / pass | - / - / pass |
| p5_giant | 27 / 15 / pass | 27 / 15 / pass |
| p5_idol | - / 24 / pass | - / 92 / pass |
| p5_l3spider | 204 / 68 / pass | 204 / 68 / pass |
| p5_l4, p5_shop, p5_snakes, p5_spider | - / - / pass | - / - / pass |
| p5_reg_l2s10, l3s10, l4s10 | 182 / 182 / FAIL (damage) | 182 / 182 / FAIL (damage) |

Route check, all formats:
- s7.24: 18 of 23.
- s7.26 (int32, so |v| < 32): 20.
- Q32.32: 18. Q40.40: 18. Q16.48 velocities with s.24 positions: 18.
- binary64 velocities with s13.18 or Q32.32 positions: 23. On these, no route has a discrete or RNG difference.
  Positions differ below 1/16 px only where GameMaker stores x / y as float32 (rubble, the yell sprite).

### Cause

Method: tools/equivcheck.py's first position difference, with the velocities from the record before, Q40.40
against binary64.

The same records fail at 24, 32, 40 and 48 fraction bits, so precision is not the cause.
- **The mechanism:** HD moves by `oGame.time mod round(1 / frac(abs(v)))`. The game's velocities are multiples of
  0.1-0.6 built up by repeated float operations, so 1 / frac lands on .5 ties.
- **Example, 1.08:** 1.8 x 0.6 = 1.08, and 1 / 0.08 = 12.5. On p5_giant record 12, binary64 has
  1.0799999999999998, which gives 12.500000000000023 and rounds to 13. Exact 1.08 gives 12.5, which rounds half to
  even to 12.
- **Example, the 2.5 tie:** 2.4, 5.4 and 1.4 give 1 / 0.4 = 2.5. First differences at this tie: p4_bomb_drop 86,
  p4_bomb_throw 64, p4_exit559 182 (an arrow), p4_spikes 51, p5_caveman 20, p5_idol 80, p5_l3spider 67,
  p4_push_rope 165.
- **Why no format can match:** binary64's rounding error decides which side of the tie a route takes, and no fixed
  format reproduces it.
- **What follows:** the one-pixel shift moves later collisions, events and RNG draws.
- **Comparisons:** the GML compare (epsilon 1e-5) was not the first cause on any route.

### SH-2

Mean over all 6,566 route steps:

| Build | Mean step | Change |
|---|---|---|
| exact | 503,611 | |
| s7.24 | 492,205 | -11.4 K |
| binary64 velocities, s13.18 positions | 505,053 | +1.4 K |

The binary64-velocity build gains nothing. Its int32 positions remove the float32 adds, but conversions appear
where a double velocity meets a fixed position.

## 2. Collision order

Four settings of `-DPCOL_CREATION`:
- **1 / 2:** searches visit the R-tree's entries in creation order (1) or newest first (2). The set and the
  rectangles are the same, and the tree is still maintained.
- **3 / 4:** the same two orders with no tree during play. The entries go in a 32 px grid:
  - each entry sits in the cell of its top-left corner when it spans at most two cells, otherwise on a big list;
  - a search reads the cells from one left / above its rectangle to its bottom-right corner;
  - it sorts the hits by creation number and calls back.

The level generator keeps the tree, because it must stay bit-exact.

Each cell is: rng / discrete (by id) / gbag (id-free state).

| Route | Creation order | Newest first |
|---|---|---|
| p1_walk | - / 310 / - | - / 310 / - |
| p4_bomb_drop | - / 195 / 199 | - / 195 / - |
| p4_bomb_throw s3 | - / 172 / 172 | - / 169 / 172 |
| p4_bomb_throw s7 | 168 / 168 / 168 | - / 168 / - |
| p4_darkexit | - / 179 / - | - / 190 / - |
| p5_reg_l2s10, l3s10, l4s10 | - / 188 / - | - / 188 / - |
| the other 15 routes | - / - / - | - / - / - |

The route check passes on 23 of 23 for all four settings. The grid gives the same results as the tree visited in
the same order (3 = 1, 4 = 2).

The two state comparisons:
- **Discrete:** compares instances by id.
- **gbag:** compares the header and the multiset of (object, sprite, frame, depth, visible, alarms, x / y to
  1/16 px), with no ids. It leaves out the particles that have no effect on play: oRubblePiece's family, oBlood
  and oFlame.

Every divergence comes from an explosion:
- **Destroy order:** the blocks it destroys are visited in another order.
- **Spawned objects:** the rubble, flames and gold chunks they spawn get other ids.
- **RNG draws:** those objects draw the same RNG values in a different order, so the same set of particles gets
  other sprites and velocities.
- **RNG state:** it stays equal, except on p4_bomb_throw s7 in creation order.
- **What remains without particles:** gold chunk / nugget resting positions a few pixels apart (p4_bomb_throw s3;
  p4_bomb_drop in creation order). Money totals are equal.

SH-2 results:

| Grid | Mean step | Change | Median | p99 |
|---|---|---|---|---|
| exact (tree) | 503,611 | | | 1,553,984 |
| creation order | 383,738 | -119.9 K, -23.8 % | 363,616 | 794,624 |
| newest first | 383,356 | -120.3 K, -23.9 % | 362,976 | 793,920 |

- The worst bomb steps drop the most: p4_bomb_drop step 202 goes from 2.07 M to 0.84 M.
- The largest step in every variant is a room change, about 30 M.
- The grid is not tuned yet: cell size, the insertion sort and the big list are untouched.

## 3. 1.1's instance_deactivate_region (option 5)

`-DPLAY_DEACT` runs 1.1's logic in oLevel's Step:
1. **Deactivate:** every instance except the oLevel itself whose bounding box does not touch the view +-96 px.
2. **Activate the view region.**
3. **Activate these objects:** oCharacter, oRope, oRopeThrow, oRopeTop, oGame, oGlobals, oScreen, oGamepad,
   oExplosion, oGhost, oFinalBoss, oBoulder, oOlmec.
4. **Activate the player / boulder / Olmec regions,** with the GML's arguments. The player's region is
   (x - 16, y - 16, width x + 16, height y + 16), as written in 1.1.

What happens to a deactivated instance:
- **State:** it is alive 0 but keeps its slot and its state.
- **Lists:** it is out of the object lists, the grid, the collision tree and the counts.
- **Reactivation:** it comes back in creation order.
- **Output:** playhost prints the deactivated instances too.

### Instance counts and equivalence

- **Instances per step** (record means over each route):
  - total: 720-840;
  - active: 250-420, mean about 330;
  - deactivated: 315-530, mean about 450.
- **Route check:** 23 of 23.
- **RNG:** equal on 22. The exception is p4_bomb_throw s7, from record 168.
- **Discrete state:** differs from record 1-2 on every route, by construction: deactivated rubble outside the view
  never times out.
- **Discrete state in view** (instances within 16 px of the view): equal on 20 routes. The three exceptions are
  p4_bomb_throw s7 (record 168), p5_caveman (181) and p5_l4 (79). In each, an enemy or item frozen outside the
  region comes back in a different state.

### SH-2

The scan runs in integers: cached integer boxes and a per-object table for the kept objects.

| Variant | Mean step | p99 |
|---|---|---|
| exact | 503,611 | 1,553,984 |
| the scan alone, nothing deactivated (`PLAY_DEACT_SCANONLY`) | 568,972 (+65.4 K) | 1,646,560 |
| deactivation | 503,916 (+0.3 K) | 1,566,016 |
| newest-first grid | 383,356 | 793,920 |
| deactivation + newest-first grid | 393,257 (+9.9 K over the grid) | 842,592 |

- **Savings from fewer active instances:** about 65 K a step with the tree (scan-only minus deactivation), and
  about 55 K with the grid (grid + scan minus both).
- **Scan cost:** the per-step scan over about 770 instances costs 65 K. The savings and the scan cost cancel, so
  1.1's version has no net gain.
- **First version:** pin_bbox in double plus obj_is chains cost 1.17 M a step.
- **Estimated cheaper scan:** looking at the non-terrain instances every step, and at terrain only when the regions
  move, should keep most of the 55-65 K. This is an estimate, not measured.
- **Largest steps:** room changes in every variant (about 30 M).

## 4. Proposed gate

Inputs:
- **Reference:** the exact host build, which is record-equal to the HD runner's trace on every gated route.
- **Variant:** the host build with the option.
- **Per route:** `tools/equivcheck.py <exact> <variant>`, both built with -DPLAY_RNGLOG.

The gate:
1. **Route check (gate, over all records), `route=PASS`.** All of these must hold:
   - the same rooms in the same order, with each entry step within 1;
   - the same number of deaths;
   - the same damage sequence (life after each loss);
   - the same last money, bombs and ropes;
   - the variant does not end before the exact build does.
2. **State gate (gate, up to the first RNG divergence), `state=PASS`.** Compared fields:
   - the header (room, level, life, bombs, ropes, money, view);
   - the id-free gameplay state (gbag).

   Both must be equal at every record before `rng`, the first record whose RNG state differs, or over the whole
   route when it never differs.
3. **Reported, not gated:**
   - rng;
   - disc (by id);
   - vdisc (by id, in view);
   - bag (with particles);
   - the first position difference.

   An option that fails gate 2 on a route can still be accepted per route by writing down why. The cases found so
   far:
   - explosion debris order (gold resting positions);
   - deactivation's frozen out-of-view instances. For deactivation, use vdisc in place of gbag.
4. **SH-2:**
   - MAME checksums equal to the variant's host build on every job (scripts/playsh2_check.sh, as now);
   - jtcps3 equal to MAME (scripts/playsh2_jt.sh).

### Results on this gate (gates 1 + 2, of 23)

| Option | Passes | Notes |
|---|---|---|
| binary64 velocities, fixed positions | 23 | |
| exact | 23 | |
| newest-first grid | 22 | fails on p4_bomb_throw s3 (gold positions from record 172; RNG equal) |
| creation-order grid | 21 | |
| s7.24 and Q32-Q48 | 2 | |
| deactivation | 0 | 20 when gate 2 is read over the view only |

### Known gameplay differences (accepted by the user)

tests/equiv_accept.txt lines whose reason starts with `gameplay:` are differences in play, not only in cosmetic
state, that the user accepted; scripts/equiv_check.sh reports them as `known`, still only at the listed first gbag
record.

- **p5_lush_l5s37 (2026-10-06):** one oEnemySight search overlaps oPlayer1 and an oTree in one step. HD's R-tree visits
  the player first (Collision_oCharacter alerts the caveman), the grid visits the tree first (Collision_oSolid destroys
  the sight; the alert is lost). HD's order follows its R-tree's state, which changes as instances are reinserted (the
  same pair comes in the opposite order 7 steps later); the grid's newest-first order matches it in 12 of the 14 such
  passes found in the routes. Matching it in every case means the exact build: about +48 % a lush step (MAME SOFTFP
  144.6 K -> 213.4 K) on levels already over budget.

## Files

- **src/game/pnum.h:** PLAY_FX2 (FX_NFB, FX_PFB, FX_T64, FX_NUMDBL, FX_EPS168). PLAY_FIXED's conversions now
  evaluate their argument once.
- **src/game/pcol.c:**
  - PCOL_CREATION 1-4: the search order; 3 / 4 are the grid with no tree.
  - pcol_deact / pcol_act.
- **src/game/pworld.c:**
  - PLAY_DEACT (pw_deactivate_step, PLAY_DEACT_SCANONLY), hooked in pobj.c's level_step.
  - FX_T64's separate struct pin memory.
- **src/game/rng.c and test/host/playhost.c:** PLAY_RNGLOG. It writes Q lines with the RNG index and a hash per
  record. With RNGTRACE=1 it also prints every draw, with the running object and callers.
- **tools/equivcheck.py:** the comparisons and both gates above.
- **Fixes that keep the exact build identical:** penemy.c / pshop.c P() casts, and prun.c's loader velocity.
- **Not committed:** scratch drivers (variant builds, route loops, the SH-2 runner with -D defines prepended to the
  snapshot's pnum.h).
