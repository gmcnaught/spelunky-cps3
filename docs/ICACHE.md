# ICACHE: keeping streaming code and data out of the SH-2's 4 KB cache

Status 2026-10-06, branch `claude/instruction-cache-optimization-kmmwck` on main d6a1155. Exploration: what keeps
the jtcps3 / MAME ratio of the play step high, and which strategies raise the cache hit rate. Measured with the
jtcost trace model (docs/PERF3.md section 1, docs/REVIEW-SH2.md section 3) on 7 traced steps, and with a real linked
build checked in MAME. Nothing in src/ is changed. **The hardware A/B (section 6) is still to run.**

## 1. Summary

| Strategy | Modelled jtcps3 change, 7 steps (fit / review constants) | Status |
|---|---|---|
| **A. Streaming functions run from SIMM 1's cache-through mirror** (0x26000000; 22 functions) | about −8 % / −4 % (model, functions alone) | linked, checksums equal |
| **B. Streaming arrays read through main RAM's mirror** (0x22000000; 28 `.bss` arrays, `inst_mem` the largest) | about −8 % / −6 % on top of A | linked, checksums equal |
| **A + B as built** (`tests/playsh2/nc_robust.txt`) | **−15.3 % / −8.8 %**, every step better (−7 % to −18 %); held-out steps −14 % to −18 % (fit) | **ready for jtcps3** |
| C. Hot fields of `struct pin` in one line (a layout "sublist") | −1.1 % / −0.9 % alone; −0.3 to −0.6 % on top of A + B | measured, not kept |
| D. Algorithmic sublists for the per-step sweeps (alarms, animate, Draw / Step snapshots) | `play_step` is 11.7 % of the step: the bound | sized, not built |
| E. Two-way mode + hot code in the 2 KB cache RAM | **+19.5 %** (two-way alone +28 %) | rejected |

The cause: **53 % of instruction-line fills and 27 % of data-line fills are dead**, evicted before any reuse.
In a 4 KB, 4-way, write-through cache, a dead fill costs a full line read and also evicts a line that would have
hit. On the SH7604 an access with A31-29 = 001 goes past the cache without filling a line. jtcps3 runs code
fetched that way at 2.5 clocks an instruction, the same as straight-line code that misses every line. So for code
that would miss anyway, the bypass costs nothing itself and leaves the cache to the code and data that reuse it.
It is a linker-only change: the code is the same and every value is the same.

## 2. Hardware facts used

| Fact | Source |
|---|---|
| Areas by A31-29: 000 cached, **001 cache-through**, 010 purge, 011 address array, 110 data array | jtcores `modules/jtframe/hdl/cpu/sh7604/CACHE.sv:55-59` |
| A cache-through read goes out as one bus read (no burst, no allocate); a cached miss reads 4 longwords (`IBUS_READARRAY`) and fills the line; stores are write-through and never allocate | `CACHE.sv:585-622, 553-566` |
| CCR.ID / CCR.OD: a fetch / data miss still reads the whole line but does not keep it (`CACHE_UPDATE <= ~CCR.ID / ~CCR.OD`) | `CACHE.sv:536`. A cache-through area is cheaper for streaming code: it reads only the words it runs |
| Code run from SIMM 1 past the cache: **2.5 clocks a NOP**, empty loop 10 (cached 1 / 4) | cps3-testgame `docs/CPS3.md` ttest, jtcps3 column ("SIMM 1 past the cache") |
| Uncached loads: main RAM 7.5, SIMM 1 4.5 clocks; stores 6.06 anywhere | same table |
| Straight-line code from SIMM 1 that misses every line: 2.51 clocks an instruction | maldita `docs/CPS3.md` (REVIEW-SH2 section 2) |
| Code at 0x26xxxxxx is decrypted on jtcps3 (ttest's "SU" rows ran there) and in MAME (opcodes by the address & 0xc7ffffff); **not run on a real board yet** | cps3-testgame `docs/CPS3.md` ("The board: not run yet") |
| **MAME 0.264's SH-2 recompiler hangs on code at 0x26xxxxxx**; the interpreter (`-nodrc`) runs it. Its debugger disassembles such code wrongly (no loads / stores in the trace) | observed here; scripts below use `-nodrc` and a shadow link |

Cost model (`tools/jtbypass.py`): jtcost's constants for cached accesses, plus the measured uncached costs:
- each 32-bit word fetched past the cache +3;
- each taken branch out of uncached code +3 (the prefetched word is lost; fits the 10-clock loop);
- uncached loads +6.5 (main RAM) / +3.5 (SIMM).

Two sets are reported throughout:
- **fit**: jtmodel's least-squares fit to EQUIV's measured step means: fetch miss +12.98, data miss +37.78;
- **review**: the hand-set penalties: fetch miss +12, data miss +27.5 (RAM) / +15.5 (SIMM).

PERF3's batch-2 calibration found both sets read low against the hardware (fit by 2-21 %, review by 19-33 %).

## 3. Tools (this branch)

| Path | What |
|---|---|
| `scripts/jtcost_native.sh` | scripts/jtcost.sh without docker: native `sh-elf-gcc` (built as cps3-testgame/docker/Dockerfile does, in /opt/sh) and MAME (0.264 from apt: `sfiii3n` with the BIOS file renamed). `JTC_KEEP=1` keeps the trace; `CHECK=1 [CROUTES=..]` runs playsh2_check's checksum run instead; `GAME_SRC=`, `MAKEARGS=` (e.g. `NC=nc_robust.txt`), `BUILD_ONLY=1`, `OPT=` |
| `tools/jtbypass.py` | `parse` a trace into events; `dead`: dead fills by function, literal pool and data symbol (`JTB_BYFN=1` / `=64`: by accessing function, and by offset in the record); `sim` with `--nc` / `--ncd` / `--ncfile` / `--cram`; `greedy` / `gready`: function / data sets chosen on the summed model of several traces; `pcs`: clocks by instruction address (for addr2line on a `-g` build: same code); `JTB_<CONST>=` overrides a constant |
| `tests/playsh2/mknc.py` | the linker script: `f <function>` / `r <rodata>` at 0x26000000 + offset (`.nctext`, first in SIMM 1), `b <bss>` at 0x22000000 + offset (`.ncbss`, inside `__bss_start..__bss_end`, so it is cleared as before). `--shadow`: the same layout at the cached addresses, for MAME traces |
| `tests/playsh2/Makefile` `NC=<list>` [`NCSHADOW=1`] | the build with `-ffunction-sections` and mknc.py's script; SIMM 1's image includes `.nctext` |
| `scripts/playsh2_jt.sh` `NC=<list>` | the jtcps3 variant with that layout (MAME run with `-nodrc`) |
| `tests/playsh2/nc_robust.txt` | the list measured here; `nc_empty.txt` the baseline with the same `-ffunction-sections` link |
| `tests/cachebench`, `scripts/cachebench.sh` | the board's cache costs (section 6 step 2) |

## 4. Results

### 4.1 Traced steps (main d6a1155, SOFTFP=1, grid build)

| Step | Instructions | Modelled jtcps3, fit | review |
|---|---|---|---|
| p4_exit559 301 | 96,333 | 393.0 K | 322.6 K |
| p5_snakes 150 (PERF3's 956) | 94,061 | 367.1 K | 305.0 K |
| p5_caveman 178 | 146,836 | 611.8 K | 502.5 K |
| p5_giant 28 | 174,014 | 647.1 K | 540.4 K |
| p5_idol 109 | 139,236 | 530.9 K | 440.4 K |
| p4_bomb_drop 202 (the explosion) | 318,871 | 1,049.8 K | 889.6 K |
| p5_l4 151 | 103,083 | 394.9 K | 328.3 K |

On p4_exit559 301 (fit) the step splits as:
- data-line misses 40 % (2,419 main RAM, 1,765 SIMM, 1,419 of them literal pools);
- fetch misses 19 % (5,843 lines; 1,780 of them compulsory);
- stores 11 %.

### 4.2 Dead fills (7 steps; `jtbypass.py dead`)

Fetch fills 58,840, **31,340 dead (53.3 %)**; data fills 39,490, 10,795 dead (27.3 %).

| Owner | Fills | Dead | | Owner | Fills | Dead |
|---|---|---|---|---|---|---|
| d: inst_mem | 5,783 | 32 % | | f: isCollisionLeft | 655 | 100 % |
| f: moveTo | 2,183 | 82 % | | f: isCollisionRight | 615 | 99 % |
| f: characterStepEvent | 1,971 | 79 % | | f: isCollisionTop | 594 | 88 % |
| f: pl_step | 1,035 | 80 % | | f: item_step | 948 | 72 % |
| f: bbkind_set | 1,588 | 71 % | | f: treasure_step | 727 | 65 % |
| f: ev_step | 2,306 | 52 % | | f: pgrid_search | 1,910 | 28 % (reused: keep cached) |
| d: pin_ext | 1,372 | 29 % | | f: \_\_adddf3 | 2,109 | 25 % (keep cached) |

The pattern is the per-instance event code (pl_step, characterStepEvent, item_step, treasure_step, moveTo), run
once per instance per step. Between two runs it walks more code than the cache holds. The shared kernels
(pgrid_search, the soft-float adds, solid_vline_any) are reused and must stay cached.

`inst_mem`'s fills by accessing function (`JTB_BYFN=64`):
- 61 % come from `play_step`'s own sweeps, 42 % of them dead: `alive` / `obj` / `spr` at +0 / +8 / +12, `img` at
  +44, `exto` at +26;
- the rest are the events' own reads, mostly hits.

### 4.3 Choosing the sets

`jtbypass.py greedy` tries functions in order of dead fills and keeps each one that lowers the summed model of the
traces by more than 0.05 %. `gready` does the same for data objects (code symbols excluded).

**Cross-validation** (fit constants; sets chosen on p4_exit559 301, p5_snakes 150 and p5_caveman 178):

| | In sample | Held-out: giant 28 / idol 109 / l4 151 / bomb_drop 202 |
|---|---|---|
| functions only (49) | −8.5 to −12.0 % | −6.9 / −9.5 / −5.1 / −6.3 % |
| functions + data | −18.7 to −21.3 % | **−14.6 / −17.6 / −16.7 / −14.0 %** |

**Sensitivity** (sets chosen on all 7 steps, under each constant set):

| Constants | Functions | + data |
|---|---|---|
| fit | −8.5 % (44 functions) | −16.8 % |
| review | −3.7 % (26) | −9.5 % |
| pessimistic: review + uncached word +4.5, jump +6, loads +8.5 / +5 (worse than ttest measured) | −0.1 % (2) | −4.2 % |

- **Data:** the data choices are stable across all three sets (`inst_mem` alone −1.6 % to −3.9 %). An uncached
  load costs 7.5 clocks against a line fill of about 30-40. So bypass pays for any array where a line serves fewer
  than about 4 loads before it is evicted.
- **Functions:** the function gain depends on the real fetch-miss penalty against the measured 2.5 clocks an
  uncached instruction.
- **The kept list:** `nc_robust.txt` takes the 22 functions chosen under both the fit and the review constants,
  and the 28 `.bss` arrays chosen under review.

### 4.4 The real linked build (`NC=nc_robust.txt`)

- **Checksums:** `CHECK=1` (MAME `-nodrc`) on p4_exit559, p5_snakes, p5_caveman and p5_idol: **1,642 / 1,642
  equal to the host**. Also p4_spikes 184 / 184 with functions only and with data only.
- **What moved:** `.nctext` 0x13880 bytes at 0x26000000; `.ncbss` 0x41a80 bytes at 0x22003670.
- **How it was traced:** the shadow link (`NCSHADOW=1`, the same addresses with bit 29 clear) was traced. Its
  instruction counts equal the base build's (96,333 / 94,061 ...). It was modelled with the list's functions and
  arrays charged as uncached.
- **Baseline check:** the `nc_empty.txt` link is identical to the base build in every step (the
  `-ffunction-sections` link keeps the order), so the whole change below comes from the bypass.

| Step | fit: base -> NC | review: base -> NC | with C (field order) too, fit |
|---|---|---|---|
| p4_exit559 301 | 393.0 -> 322.7 K (−17.9 %) | 322.6 -> 288.8 K (−10.5 %) | −17.8 % |
| p5_snakes 150 | 367.1 -> 305.1 K (−16.9 %) | 305.0 -> 274.6 K (−10.0 %) | −16.7 % |
| p5_caveman 178 | 611.8 -> 528.0 K (−13.7 %) | 502.5 -> 466.7 K (−7.1 %) | −14.1 % |
| p5_giant 28 | 647.1 -> 562.9 K (−13.0 %) | 540.4 -> 499.7 K (−7.5 %) | −13.0 % |
| p5_idol 109 | 530.9 -> 452.0 K (−14.9 %) | 440.4 -> 404.9 K (−8.1 %) | −14.9 % |
| p4_bomb_drop 202 | 1,049.8 -> 888.0 K (−15.4 %) | 889.6 -> 809.3 K (−9.0 %) | −17.6 % |
| p5_l4 151 | 394.9 -> 325.6 K (−17.5 %) | 328.3 -> 293.2 K (−10.7 %) | −17.6 % |
| **sum** | **−15.3 %** | **−8.8 %** | −15.9 % |
| first step after the level start: p4_exit559 2 / p5_caveman 2 | −9.8 % / −8.8 % | −4.4 % / −3.8 % | |

### 4.5 Sublists and hot fields (C, D)

`play_step` (with `animate` and `anim_one` inlined) is **11.7 % of all modelled clocks** over the 7 steps. By
source line (`jtbypass.py pcs` + `sh-elf-addr2line` on an `-O2 -g` build, the same code):

| Part | Share of play_step | Lines |
|---|---|---|
| animate's walk and anim_one | about 36 % (prun.c:236 alone 16.7 %: `alive && spr >= 0`, one record line missed per instance for 2 fields) | prun.c:179-189, 233-236 |
| the 12 alarm passes | about 21 % | prun.c:454-459 |
| the Draw pass's snapshot loop | about 11 % | prun.c:148-152 |
| the Step / End Step dispatch loops | about 11 % | prun.c:472-484 |

**C, measured:** `struct pin` reordered so that animate's fields (alive, visible, bbk, obj, spr, img, ispd) fill
line 0 and exto / ext / the box fill line 1. The trade: `invincible` / `cleanDeath` / `shopWall` / `treasure` move
to offset 60, past `mov.b`'s reach. Result (instructions equal ±25, main-RAM misses −2 to −5 %):
- alone: **−1.1 %** (fit) / −0.9 % (review);
- with A + B: −15.9 % against −15.3 %. Once `inst_mem` is read uncached, its layout hardly matters.

Not kept (a src/game change for half a percent).

**D, not built.** Sublists that remove the walks, not their misses. Each is a src/game change that needs PERF2's
exactness argument:
- **armed-alarm lists** (PERF2 E's per-alarm counts). Every `alarm[a]` write already goes through about 100 GML
  translation sites; a setter would have to keep a per-alarm list of instances with `alarm[a] >= 0`, kept in
  object then creation order. Bound: the alarm passes' 21 % of play_step, about 2.5 % of the step;
- **animate: only instances whose image changes.** Most of the non-terrain walk is image_speed 0 or the one-frame
  fast path. A list kept by `pin_setimg` / the ispd / spr setters, in creation order. Bound: about 4 % of the step;
- **Draw / Step snapshots from incremental per-event lists** (PERF.md item 7). Bound: about 2.5 %.

These overlap with B: an `inst_mem` read uncached costs 7.5 clocks in place of a 30-40-clock miss. Measure them
after B.

### 4.6 Rejected

- **E, two-way mode with cache RAM:** two-way alone is +28.2 % (half the lines). With the 21 best functions per
  byte in the 2 KB of cache RAM: +19.5 %; with 1 KB (all MAME maps): +24.0 %. PERF3 3.3 found the same for the
  stack.
- **Already measured, not to repeat** (PERF3 3.3):
  - hot-first link order: +0.5 % on jtcps3;
  - cache-set-aware placement: the fully associative bound is −0.1 % to −6.5 %; the misses are capacity misses;
  - `-falign-functions=16`;
  - CCR.OD (a miss still reads the whole line: section 2).

## 5. Caveats

- **The constants decide the size of A.**
  - Functions alone range from −8.5 % (fit) to −0.1 % (pessimistic). Data (B) holds in every set.
  - The model's uncached costs are ttest's measurements: one instruction loop, and one load kind per area.
    Sequential uncached code, a branch's lost prefetch, and loads mixed with fetches on one bus are inferred.
  - The hardware run settles it.
- **Generation and level start are not traced.**
  - `inst_mem` is also the generator's `W.in` (play.h: the same memory), so generation reads it uncached too.
  - The JT run times the generation jobs (G1-G5) and the level starts.
  - If they slow down: leave `inst_mem` at its cached address and point play at the mirror instead. That is
    `PW.in = (struct pin *)((uintptr_t)inst_mem | 0x20000000)` where play_level_start sets PW.in: one line in
    src/game, and the generator stays cached.
- **The list follows the code.**
  - Function names (clones: `.isra.0`, `.part.0`) and inlining change with src/game. A function inlined into its
    callers leaves the list silently (the linker pattern matches nothing).
  - Re-run `greedy` after large changes; it takes minutes on kept pickles.
- **MAME:** checksum runs of NC builds need `-nodrc` (scripts above). MAME's per-step clock column read wrong in
  0.264's `-nodrc` runs here (wrapped values); MAME does not model the cache anyway.
- **Real board:** a CPS3 board's decryption of fetches at 0x26xxxxxx is untested (cps3-testgame ttest has not run
  on one). jtcps3 and MAME run it.
- **The game build:** `tests/game` / `scripts/release.sh` don't use this yet. They need the same link (mknc.py's
  sections next to tests/game/sprbss.ld's) and the `.nctext` objcopy.

## 6. Next steps

1. **Hardware A/B on jtcps3** (.62, through the lead). Same jobs, same code:
   ```
   JTV=_base scripts/playsh2_jt.sh
   JTV=_nc NC=nc_robust.txt scripts/playsh2_jt.sh
   ```
   - Expect PASS 23/23 on both, and the route step means lower by about 9-15 % if the model holds.
   - The per-route ratio against the model recalibrates the fetch- and data-miss penalties (PERF3 0.3).
   - Also compare the generation jobs and the routes' level starts.
2. **Pin the constants on the board: `tests/cachebench`.**
   - cps3-testgame's ttest (`.rbf` 2026-10-02, its newest jtcps3 data) gives the uncached costs but no cached
     line miss. tests/cachebench measures both on the same board, plus the two cases the strategy relies on.
   - Run it: `scripts/cachebench.sh` (MAME: checks only that every row runs), then
     `../cps3-testgame/scripts/mister_run.sh tests/cachebench/build/mame cachebench "Cache bench" 3 5` on the
     MiSTer, and read the screen (pass 2 or later).
   - Expected rows if the model's constants hold (clocks; the = rows are tools/jtbypass.py's constants x100):

   | Row | fit | review | What it pins |
   |---|---|---|---|
   | LIN C /INS (8 KB straight-line, cached: every line misses) | 2.62 | 2.50 | `=IMISS` = 8 x (LIN C - HOT C): 12.98 / 12 |
   | LIN U /INS (the same past the cache) | 2.50 | 2.50 | `=UWORD` = 2 x (LIN U - HOT C): 3.0 |
   | SPARSE C / U /LINE (2 instructions a line) | about 16 / 9 | 15 / 9 | the sparse-code case: U < C |
   | HOT+LIN C / U /IT (2 KB hot code + 8 KB stream) | about 13,800 / 11,300 | 13,300 / 11,300 | the strategy itself: U < C |
   | LD16 C / HIT / U /LD (one load a line, 64 KB of main RAM) | 39.8 / 2.5 / 8.5 | 29.5 / 2.5 / 8.5 | `=DMISS RAM`, `=UNC RAM` |
   | SIMM16 C / U /LD (8 KB of SIMM 1) | 39.8 / 5.5 | 17.5 / 5.5 | `=DMISS SIMM`, `=UNC SIMM` |
   | HOTD+ST C / U /IT (2 KB hot data + 8 KB streamed) | | | the data case: U < C |

   (The /LD rows include the loop: about 1 clock a load. MAME gives 1.00 / 3.01 / 2.00 and equal C and U values.)
   - With the board's values: `JTB_IMISS=.. JTB_DMISS_RAM=.. JTB_DMISS_SIMM=.. JTB_UWORD=.. JTB_UNC_RAM=..
     JTB_UNC_SIMM=.. python3 tools/jtbypass.py greedy ...` re-chooses the list (the pickles: section 7).
3. **If 1 holds:**
   - put the NC link into tests/game and the release (section 5);
   - consider `PW.in` through the mirror in place of the `inst_mem` symbol;
   - re-run `greedy` / `gready` with the calibrated constants on more steps (the four p5_reg_* routes and the p99
     steps: explosions, the boulder).
4. **Then D** (armed-alarm and animated-instance sublists), measured with this branch's tools on top of B.

## 7. Reproducing (Linux, no docker)

```
# toolchain as cps3-testgame/docker/Dockerfile (binutils 2.42, GCC 13.3 --target=sh-elf --with-cpu=m2) in /opt/sh;
# apt install mame (0.264); ../cps3-testgame at b78e1ec
JTC_KEEP=1 VARIANT=t scripts/jtcost_native.sh p4_exit559 301
python3 tools/jtbypass.py parse tests/playsh2/build/t/out/t1.tr tests/playsh2/build/t/out/nm.txt s.pkl
python3 tools/jtbypass.py dead s.pkl
python3 tools/jtbypass.py greedy a.pkl b.pkl c.pkl      # then gready with --nc from its last lines
JTC_KEEP=1 VARIANT=s MAKEARGS="NC=nc_robust.txt NCSHADOW=1" scripts/jtcost_native.sh p4_exit559 301
python3 tools/jtbypass.py sim s2.pkl --ncfile tests/playsh2/nc_robust.txt
CHECK=1 CROUTES="p4_exit559 p5_snakes" VARIANT=c MAKEARGS="NC=nc_robust.txt" scripts/jtcost_native.sh
```
A trace is tens to hundreds of MB: parse it and delete it (HANDOFF disk rule). A pickle is 1-18 MB.
