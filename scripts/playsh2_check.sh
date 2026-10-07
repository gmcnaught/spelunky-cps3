#!/bin/sh
# tests/playsh2 in MAME: the level generator (48 cases, all areas), the P4 routes and the P5 routes on the SH-2 with the real
# src/game code; per-record checksums compared with the host build of the same code (tests/playsh2/host.c), SH-2
# clocks per generation / step summarised (tests/playsh2/report.py).
#   [PIN=1792] [OPT=-O2] [PROF=<ticks>] scripts/playsh2_check.sh
# ATTR=1: time per category / event / object and collision searches by calling object (tests/playsh2/attr.c;
# built with UNITY=0: the --wrap hooks see only calls between translation units).
# SOFTFP=1: link src/sh2/softfp.c in place of libgcc's fp-bit (scripts/softfp_check.sh tests it).
# VARIANT=<name>: build and output in tests/playsh2/build/<name> (default run, run_prof).
# PROF=<ticks> [PROF_SKIP=k] [PROF_KIND=1|3] [PROF_WRAP=1]: PC / PR sampled every <ticks> x 32 x k clocks during route steps (3) or generation (1) (timing then includes the sampler);
# report.py then sums the samples per function (and per caller for libgcc's soft-float helpers).
# ROUTES="a b ...": only those routes' jobs, in that order (tests/playsh2/mkjobs.py --routes; jobs.h in build/<VARIANT>/jobs).
# BUILD_ONLY=1: stop after the SH-2 build (build/<VARIANT>/main.elf, out/nm.txt): scripts/jtcost.sh.
set -e
cd "$(dirname "$0")/.."
T=tests/playsh2; B=$T/build; V=${VARIANT:-run${PROF:+_prof}}; E=$B/$V; O=$E/out; rm -rf "$O"; mkdir -p "$O/w"
# the code under test: src/game at GAME_REV (default HEAD: other work in the tree stays out; WORKTREE: as it is) and build/gen, PIN_MAX
# set to PIN (main RAM)
G=$B/g; rm -rf "$G"; mkdir -p "$G"
if [ "${GAME_REV:-HEAD}" = WORKTREE ]; then cp src/game/*.c src/game/*.h "$G/"      # the working tree's src/game
else git archive "${GAME_REV:-HEAD}" src/game | tar -x -C "$G" --strip-components=2; fi
cp build/gen/objects.[ch] build/gen/gentables.[ch] build/gen/playtables.[ch] "$G/"
sed -i '' "s/^#define PIN_MAX [0-9][0-9]*\$/#define PIN_MAX ${PIN:-1792}/" "$G/play.h"
grep -q "^#define PIN_MAX ${PIN:-1792}\$" "$G/play.h"; touch "$G/stamp"
# EXACT=1: the exact build (src/game/pcol.c with PCOL_EXACT: the runner's R-tree order in play); default the
# shipping build's collision grid (src/game/pcolgrid.h). GRID_SHIFT=<n>: grid cells of 2^n px (default 4)
# PCOL_EXACT in every file of the snapshot (pworld, pobj, penemy, pdamsel, pk_jungle, pscript test it too), as test/host's -DPCOL_EXACT
if [ "${EXACT:-0}" = 1 ]; then for f in "$G"/*.c; do { echo "#define PCOL_EXACT 1"; cat "$f"; } > "$f.tmp" && mv "$f.tmp" "$f"; done; fi
if [ -n "${GRID_SHIFT:-}" ]; then { echo "#define PCOL_GRID_SHIFT $GRID_SHIFT"; cat "$G/pcol.c"; } > "$G/pcol.tmp" && mv "$G/pcol.tmp" "$G/pcol.c"; fi
# SNAP_FILE / SNAP_SED: an experimental edit of the snapshot (measurements only; src/game is never touched)
if [ -n "${SNAP_SED:-}" ]; then sed -i '' "$SNAP_SED" "$G/$SNAP_FILE"; fi
# the hot play files as one translation unit (scripts/unity.sh; UNITY=0: separate)
[ "${ATTR:-0}" = 1 ] && export UNITY=0                                  # ATTR's --wrap hooks need the files separate
scripts/unity.sh "$G"
# snapcfg.h (tests/playsh2/core.c): where the snapshot keeps alpha
sed -n '/^struct pin_ext {/,/^};/p' "$G/play.h" | grep -q "alpha" && echo "#define ALPHA_IN_EXT 1" > "$G/snapcfg.h" || : > "$G/snapcfg.h"
JD=; JI=
if [ -n "${ROUTES:-}" ]; then
  mkdir -p "$E/jobs"; python3 $T/mkjobs.py "$E/jobs/jobs.h" tests/routes --routes "$(echo $ROUTES | tr ' ' ,)"
  JD="JOBSDIR=build/$V/jobs"; JI="-I$E/jobs"
fi
scripts/dmake.sh $T OUT=build/$V $JD OPT="${OPT:--O2}" PROF=${PROF:-0} PROF_KIND=${PROF_KIND:-3} PROF_SKIP=${PROF_SKIP:-1} PROF_WRAP=${PROF_WRAP:-0} SOFTFP=${SOFTFP:-0} ATTR=${ATTR:-0} FPCHECK=${FPCHECK:-0} >/dev/null
if [ "${BUILD_ONLY:-0}" = 1 ]; then
  docker run --rm -v "$PWD":/p -w /p cps3-dev:latest sh-elf-nm -n $E/main.elf > "$O/nm.txt"; exit 0
fi
# host reference (the same core.c and jobs.h; one process per job)
cc -std=c99 -O2 -ffp-contract=off -w -I$T $JI -I$B -I$G -o $B/host $T/host.c $T/core.c $T/sndstub.c src/snd/snd.c -Isrc/snd -Isrc/front -I../cps3-testgame/sdk/include $G/*.c -lm   # no contraction: as the SH-2 (pcol.c's fused operations are explicit fmaf)
N=$($B/host --count); j=0; : > "$O/host.txt"
while [ $j -lt $N ]; do $B/host $j >> "$O/host.txt"; j=$((j + 1)); done
[ "${ATTR:-0}" = 1 ] && export PSH2_ATTR=$(($(grep -c "^    OBJ_" $G/objects.h) - 1))   # OBJ_COUNT
PSH2_OUT="$O/sh2.txt" scripts/mame.sh sfiii3na -rompath "$E/mame" -skip_gameinfo -nothrottle -sound none -video none \
  -seconds_to_run ${SECONDS_TO_RUN:-40000} -cfg_directory "$O/w/cfg" -nvram_directory "$O/w/nvram" \
  -snapshot_directory "$O/w/snap" -diff_directory "$O/w/diff" -state_directory "$O/w/sta" -inipath "$O/w" \
  -autoboot_script scripts/lua/playsh2.lua >"$O/mame.log" 2>&1 || true
docker run --rm -v "$PWD":/p -w /p cps3-dev:latest sh-elf-nm -n $E/main.elf > "$O/nm.txt"
PSH2_JOBS=${ROUTES:+$E/jobs/jobs.h} python3 $T/report.py "$O/host.txt" "$O/sh2.txt" "$O/nm.txt"
