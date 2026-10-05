# Spelunky CPS3

Spelunky® Copyright (c) 2008, 2009 Derek Yu and Mossmouth, LLC. This is an unofficial, fan-made modification: a
port of [Spelunky Classic HD](https://github.com/yancharkin/SpelunkyClassicHD) (yancharkin), itself a modified
version of Derek Yu's Spelunky Classic. It is not an official or unmodified version of the game, is not affiliated
with or endorsed by Mossmouth or Capcom, and may not be sold. It is distributed under the Spelunky User License
v1.1b ([LICENSE](LICENSE)). No game assets or Capcom ROMs are included.

Spelunky Classic HD 1.2.2 re-implemented in C as a homebrew ROM set for the Capcom CPS3 (SH-2). It runs in MAME
(`cps3` driver, using the `sfiii3na` set name) and on MiSTer with the jtcps3 core. The design decisions and their
measurements are in [PLAN.md](PLAN.md). Its title (title screen and MRA) is **Spelunky Classic Arcade**: the
logo is built from HD's at build time (`tools/titlelogo.py`), with ARCADE in place of HD. The current state is in [docs/HANDOFF.md](docs/HANDOFF.md).

Game assets are never committed. Everything under `refs/`, `build/` and `rom/` is git-ignored and is rebuilt
locally (see [Build inputs](#build-inputs-refs-and-buildgen)).

## Where the code is

| Path | What it is |
|---|---|
| `src/main/main.c` | **Entrypoint.** Boot (palette and character DMA from flash), then the frame loop: inputs -> `shell` -> game step every 2nd frame -> `draw` -> VBlank. `game.c` holds the per-frame game glue |
| `src/shell/` | Coins, credits, Start, inputs, high scores in EEPROM |
| `src/front/` | Intro, title, high-score rooms, attract mode |
| `src/game/` | The game. `gen*.c`: level generator (bit-exact to HD). `p*.c`: play loop, player, enemies, items, shop, transitions. `pk_*.c`: content per area (jungle, swamp, ice, temple, items). `pcol.c`/`pcolgrid.h`: collision (R-tree build or the shipping grid) |
| `src/draw/`, `src/hud/` | Display list, tilemaps, HUD |
| `src/snd/` | HD's `playSound` / `playMusic` rules on the CPS3 sound chip |
| `src/sh2/` | Soft-float (C and SH-2 assembly) and `fma` |
| `tools/` | Python converters (HD source -> `build/gen` tables and art) and checkers |
| `scripts/` | Build, trace and gate scripts (shell) |
| `tests/` | CPS3 test programs (each a Makefile on the SDK) and route files (`tests/routes/*.txt`) |
| `test/host/` | Host (macOS/Linux) builds of the generator and play loop, compared against the HD runner's traces |
| `docker/` | `mame/` (headless MAME), `hd-runner/` (HD's own GameMaker Linux runner, for reference traces) |

The top-level `Makefile` wraps the steps below (`make` = `build/gen` + host builds; `make check`, `make
check-mame`, `make game-check`, `make game`/`mister`, `make images`; targets are listed at its top). There is no
release target yet. The game program (`src/main` + everything above) is built by
`tests/game/Makefile`, which feeds it a recorded route (`build/route.h`) and simulates coin/Start. With
`ATTRACT=1` it runs the attract mode instead.

## Prerequisites

The repo depends on sibling directories under the same parent (`MisterFPGA-Projects/`):

| Path | Needed for |
|---|---|
| `../cps3-testgame` | The CPS3 SDK (`sdk/sdk.mk`, `sdk/include`, crt0, link scripts), `tools/mkcps3.py` (MAME set), `tools/mkmra3.py` (MiSTer zip + MRA), and the `cps3-dev` Dockerfile |
| `../_tools/UndertaleModTool` | `out-cli/UndertaleModCli.dll`, used by `hdgentables.py`, `hdplaytables.py`, `fronttables.py` to read HD's `game.unx` (run in the `mcr.microsoft.com/dotnet/sdk:10.0` image) |

Host tools: Docker, Python 3 with Pillow and numpy, ffmpeg (for `tools/hdsound.py`), a C compiler (for `test/host`).

Docker images:

```sh
docker build -t cps3-dev:latest ../cps3-testgame/docker   # sh-elf binutils 2.42 + GCC 13.3 (SH-2, no libc)
docker build -t spelunky-mame:latest docker/mame           # built on first use by scripts/mame.sh
docker build -t spelunky-hd-runner docker/hd-runner        # built on first use by scripts/hd_trace.sh
```

`sh-elf-gcc` is not installed on the host; all SH-2 builds go through `scripts/dmake.sh <dir> [make args]`, which
runs `make` in `cps3-dev` with the parent directory mounted.

Do not run the host `mame` for project work: it opens a window and takes focus. Use `scripts/mame.sh`, which runs
MAME headless in Docker (`MAME_NATIVE=1` to run the host one on purpose). Its mount path is hard-coded to
`/Users/gmcnaught/MisterFPGA-Projects` (`scripts/mame.sh:8`).

## Build inputs: `refs/` and `build/gen`

### `refs/` (download once, git-ignored)

| Path | Source |
|---|---|
| `refs/hd/src` | Spelunky Classic HD source, git checkout of https://github.com/yancharkin/SpelunkyClassicHD at tag `1.2.2` |
| `refs/hd/linux-arm64` | `spelunky_classic_hd-linux-arm64.zip` from that repo's 1.2.2 release, unzipped. Provides `assets/game.unx` (the data file the table generators read) and the runner used for reference traces |
| `refs/hd/hd-1.2.2-android.apk` | Release APK (RNG research only) |
| `refs/src_1_1`, `refs/game_1_1` | Spelunky 1.1 source and game (history; `build/gml`) |

### `build/gen` and `build/snd` (generated, git-ignored)

Every SH-2 and host build reads these. `make gen` runs the commands below when an input is newer. By hand, after fetching `refs/` (order follows each tool's inputs;
`drawtables.py` reads the outputs of the others):

```sh
U=refs/hd/linux-arm64/assets/game.unx
python3 tools/hdobjects.py   refs/hd/src build/gen/objects.h           # objects.{h,c}
python3 tools/hdgentables.py refs/hd/src $U build/gen/gentables.h      # generator tables
python3 tools/hdplaytables.py $U build/gen/playtables.h                # play-loop tables
python3 tools/fronttables.py $U build/gen                              # front-end rooms
python3 tools/hdsprites.py   refs/hd/src build/gen                     # gfx.bin/json, sprites.{h,c}
python3 tools/hudart.py      refs/hd/src build/gen                     # hud.bin/json, hudart.h
python3 tools/darkfade.py sources refs/hd/src build/gen && python3 tools/darkfade.py table build/gen   # fade.bin/h
python3 tools/drawtables.py  refs/hd/src build/gen                     # drawtab.{h,c}
python3 tools/hdsound.py     refs/hd/src build/snd                     # snd.bin, snd.h
```

`scripts/game_check.sh` re-runs `drawtables.py` on every build.

### `build/trace` (reference traces, git-ignored)

Per-step dumps from HD's own runner, the reference every gate compares against:

```sh
scripts/hd_trace.sh <route> <seed> [name]     # tests/routes/<route>.txt -> build/trace/<name>.{bin,names,log}
scripts/hd_trace.sh --gen tests/gen/mines.txt # generator cases
```

About 20 s per route. `TRACE_SHOT=r1,r2` also saves the runner's frames at those records.

## Build and run the game

```sh
# Build tests/game for a route and compare MAME frames 30, 150, 300 to the runner's trace:
scripts/game_check.sh p4_exit559 559 g_p4_exit559_s559 30,150,300
```

This snapshots `src/game` at `HEAD` (`GAME_REV=<rev>` to change; `GAME_FILES="a.c b.c"` to overlay uncommitted
files) into `tests/game/build/g`, builds with `scripts/dmake.sh`, runs `scripts/mame.sh sfiii3na`, and compares
snapshots with `tools/drawmodel.py`. Output is in `tests/game/build/<route>/`. The 80 MB ROM set is deleted
afterwards. Options: `HUD=0`, `EXACT=1` (R-tree collision build), `ATTRACT=1`, `DARK=a8`, `HOLD=n` (also builds
the MiSTer zip + MRA).

To build a set and keep it, call the Makefile directly (needs `tests/game/build/g` from a previous `game_check.sh`):

```sh
scripts/dmake.sh tests/game ROUTE=p4_exit559 SEED=559 SNAPS=100,200   # -> tests/game/build/mame/sfiii3na/
scripts/dmake.sh tests/game ROUTE=p4_exit559 SEED=559 mister          # -> tests/game/build/mister/ (zip + MRA)
```

## Tests and gates

| Command | Checks |
|---|---|
| `make -C test/host` | Host builds: `genhost`, `playhost` (exact), `playhost_nc`, `playhost_grid` (shipping), `playhost_eq` |
| `make -C test/host sh2` | Compile check of `src/game` with `sh-elf-gcc -m2` |
| `scripts/p5_regress.sh` | All routes on the host play loop, record-equal to the runner traces |
| `scripts/equiv_check.sh` | Shipping (grid) build: gameplay equivalence (`tools/equivcheck.py`, docs/EQUIV.md) |
| `scripts/playsh2_check.sh` | SH-2 build in MAME: per-step checksums equal to the host build |
| `scripts/game_check.sh` | Game program frames in MAME against the runner (above) |
| `scripts/gametime_check.sh` | Frame budget (CPU clocks per step) |
| `scripts/snd_check.sh`, `scripts/sndrules_check.sh` | Sound calls and chip output |

`build/gates.sh` and `build/ctall.sh` run the full host suite. They live in `build/`, which is git-ignored, so a
fresh clone does not have them.

## Hardware

jtcps3 runs use a MiSTer on the LAN (`scripts/jt_frames.sh`, `scripts/playsh2_jt.sh`), one job at a time.
jtcps3 is 2.6-4x slower per step than MAME; the budget is about 0.84 M jtcps3 clocks per 30 Hz game step
(PLAN.md §3, docs/PERF3.md).

## Further reading

- [PLAN.md](PLAN.md): decisions, GameMaker runtime rules, phases P0-P8
- `docs/`: ARCADE (shell/inputs), AUDIO, CONTENT (areas), DRAW, EQUIV (grid vs exact), PERF*, VARIANTS (HD vs 1.1)

## Licence

The Spelunky User License (`refs/src_1_1/COPYING.txt`) allows redistribution and modification, not sale.
