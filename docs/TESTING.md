# Testing: what to run when

Trace-based checks need the build the traces were made with: since 3dd5a2b PLAY_DEACT is 32 by default, so until the
traces are regenerated with TRACE_DEACT=32, run them on a DEACT=0 build (`make -C test/host DEACT=0`, `DEACT=0` for
playsh2_check / playsh2_jt). Timings measured 2026-10-06 on the development Mac (8 cores), branch testspeed (main 86b81a5). Host checks need
`make -C test/host`; trace checks need build/trace (the HD runner's references, scripts/hd_trace.sh); MAME checks
run headless in docker (scripts/mame.sh).

## Per commit (fast set, about 5 min)

| Check | What it proves | Time |
|---|---|---|
| `scripts/hostident.sh save <file>` before, `check <file>` after | playhost (exact) and playhost_grid output byte-identical on every playhost route: 184 runs (92 routes x 2 builds; the "182 host route runs" of LUSH.md / PERF3.md plus p4_bomb_throw s3's pair) | 24 s (P=8) |
| `scripts/ctall.sh` | every c_* route record-equal to its HD trace (CTALL 59/59) | 76 s (P=8; the old serial build/ctall.sh 249 s) |
| `scripts/equiv_check.sh` | the grid build equivalent to the exact build, the exact build record-equal to the trace (EQUIV 88/88 with tests/equiv_accept.txt) | 187 s (P=8, with other work running; serial 559 s) |

`P=<jobs>` sets the parallelism of the three (default 8). The output is the same as serial, line for line.

## Per batch (full set, before a merge)

| Check | Time |
|---|---|
| build/gates.sh (main checkout, untracked: P5 regress, P4 routes, gen 9 sets, colprobe, snd_check) | not timed here |
| `scripts/playsh2_check.sh` (MAME, 9,701 checksums), `SOFTFP=1` | 58 s, 53 s |
| `scripts/shell_check.sh` | 8 s |
| `scripts/game_check.sh p4_exit559 559 g_p4_exit559_s559 30,150,300` (and p5_shop / p5_spider: docs/DRAW.md) | 19 s (3 records) |
| `scripts/capture_check.sh` | 63-91 s |
| `scripts/gametime_check.sh` | 57 s |
| jtcps3 timing (`scripts/playsh2_jt.sh`, then `scripts/jt_time.sh` on the MiSTer: ask the lead) | see below |
| jtcps3 frames (`scripts/jt_frames.sh`, MiSTer screenshots against the model) | 5 min build, 6 min MiSTer |

## Side by side: RUNTAG

Two checks that build in the same directory must not run at once (a shared tests/game/build/g snapshot gave a
spurious 388 px frame difference on 2026-10-06). `RUNTAG=<tag>` gives a check its own directories:

| Script | Default | With RUNTAG=t |
|---|---|---|
| game_check.sh, smooth_check.sh, jt_frames.sh, game_host.sh, end_host.sh, front_host.sh | tests/game/build | tests/game/build/t_t (game_host / end_host / front_host read that snapshot; jt_frames: build/jtshot_t) |
| capture_check.sh | build/capture, tests/game/build/capture | build/capture_t, tests/game/build/t_t/capture |
| gametime_check.sh | tests/gametime/build | tests/gametime/build/t_t |
| playsh2_check.sh, playsh2_jt.sh, jtcost.sh, jt_time.sh | tests/playsh2/build | tests/playsh2/build/t_t |
| snd_check.sh, equiv_check.sh, ctall.sh, hostident.sh, replay.sh | build/snd_check, build/equiv, build/c, build/hostident, build/replay | the same with _t |

The Makefiles of tests/game, tests/gametime and tests/playsh2 take `W=<dir>` (the work directory: snapshot, route.h,
hold.h, jobs.h, flash.bin); the scripts pass it. Still shared: build/gen (tools/drawtables.py rewrites
drawtab.* with the same content) and the mdiff images game_check writes next to the trace's frames.

## jtcps3 timing runs

**Link: NC by default.** Since 2026-10-07 `scripts/playsh2_jt.sh` links with `NC=nc_robust.txt` unless told
otherwise (`NC=none`: the plain cached link; `LAYOUT=1` builds stay plain unless NC is given): the link the game
build and the release use, and the one every jtcps3 table in docs/ assumes. A/B on f800d8e (jtcps3, step means, K):

| Route | NC, default mode (.81) | NC, JTFAST (.62) | plain link, JTFAST (.62) |
|---|---|---|---|
| p5_lush_l5s11 | 378.8 | 377.3 | 415.5 |
| p5_lush_l6s23 | 353.7 | 352.7 | 387.7 |
| c_swamp_drain | 555.8 | 554.9 | 599.0 |
| c_swamp_swim | 437.2 | 436.2 | 471.9 |
| c_swamp_piranha | 347.6 | 346.8 | 379.1 |
| c_swamp_grave | 531.2 | 531.8 | 581.0 |

JTFAST against the default mode: within 0.4 % (two boards); the plain link: 8-10 % slower. The default-mode run
took 621 s on jtcps3, the JTFAST run 126 s (6 routes, JT_NOGEN).

Measured before the NC default (plain link), on jtcps3 (2026-10-06, main 86b81a5, `scripts/jt_time.sh`; times from load to the screenshot that read the
results, 30 s polls, so within 30 s above the true time):

| Set | Board | Result | Time |
|---|---|---|---|
| default build, 9 routes + 5 generation cases (ikernel / deact2 runs) | .62 / .81 | PASS | 20-26 min |
| JTFAST, 6 routes + 5 generation cases | .62 | PASS 11/11 | 128 s |
| JTFAST JT_NOGEN, the same 6 routes | .62 | PASS 6/6 | 124 s |
| p4_exit559 alone, default build | .81 | PASS 1/1, step mean 337,455 | 169 s |
| p4_exit559 alone, JTFAST | .62 | PASS 1/1, step mean 338,898 (+0.4 %; the same set on one board twice: 0.2 %) | 58 s |

`scripts/playsh2_jt.sh` times the jobs on the SH-2 (FRT clocks) and shows the results on screen. Measured in MAME
(6 routes + 5 generation cases, the routes of LUSH.md batch 24): of 10.6 G clocks, 0.8 G were the timed work; the
rest was the per-step state checksum (sum_play: FNV over every instance, ~2.9 M clocks a step). At jtcps3's ~4x
MAME's clocks that is the 20-26 min of a hardware run.

- `JTFAST=1`: no per-step checksum; one of the end state per route (host and SH-2 alike: PASS still means the same
  play). MAME 425 -> 34 emulated s, PASS 11/11, step means unchanged.
- `JT_NOGEN=1`: no generation cases: 26 emulated s.
- What a JTFAST PASS covers: every step's instance count and the route's end state (sum_play: every live instance's
  fields, the globals, the player, the RNG state), so a divergence that changes an instance count at any step or
  survives to the end fails it. A divergence that re-converges before the end with the same instance counts would
  pass; the per-record checks (`scripts/playsh2_check.sh` in MAME, 9,701 checksums; the default jtcps3 build) catch
  that. Use JTFAST for timing, not as the equivalence check.
- `scripts/jt_time.sh`: installs the newest built set on the MiSTer (default .62), loads it, screenshots every 30 s,
  reads the screen with `tools/jtresult.py` (the SDK font and bigtext glyphs matched cell by cell, no OCR), stops
  at the results, returns the MiSTer to the menu and prints the table with the jobs' names.

```
JTFAST=1 JT_NOGEN=1 JT_ROUTES=p4_exit559,p5_caveman,p5_snakes,p5_lush_l5s11,p5_lush_l5s37,p5_lush_l6s23 \
  scripts/playsh2_jt.sh && scripts/jt_time.sh
```
