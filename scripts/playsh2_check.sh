#!/bin/sh
# tests/playsh2 in MAME: the level generator (48 cases, all areas), the P4 routes and the P5 routes on the SH-2 with the real
# src/game code; per-record checksums compared with the host build of the same code (tests/playsh2/host.c), SH-2
# clocks per generation / step summarised (tests/playsh2/report.py).
#   [PIN=1000] [OPT=-O2] [PROF=<ticks>] scripts/playsh2_check.sh
# ATTR=1: time per category / event / object (tests/playsh2/attr.c).
# SOFTFP=1: link src/sh2/softfp.c in place of libgcc's fp-bit (scripts/softfp_check.sh tests it).
# VARIANT=<name>: build and output in tests/playsh2/build/<name> (default run, run_prof).
# PROF=<ticks> [PROF_SKIP=k] [PROF_KIND=1|3] [PROF_WRAP=1]: PC / PR sampled every <ticks> x 32 x k clocks during route steps (3) or generation (1) (timing then includes the sampler);
# report.py then sums the samples per function (and per caller for libgcc's soft-float helpers).
set -e
cd "$(dirname "$0")/.."
T=tests/playsh2; B=$T/build; V=${VARIANT:-run${PROF:+_prof}}; E=$B/$V; O=$E/out; rm -rf "$O"; mkdir -p "$O/w"
# the code under test: src/game at GAME_REV (default HEAD: other work in the tree stays out; WORKTREE: as it is) and build/gen, PIN_MAX
# set to PIN (main RAM)
G=$B/g; rm -rf "$G"; mkdir -p "$G"
if [ "${GAME_REV:-HEAD}" = WORKTREE ]; then cp src/game/*.c src/game/*.h "$G/"      # the working tree's src/game
else git archive "${GAME_REV:-HEAD}" src/game | tar -x -C "$G" --strip-components=2; fi
cp build/gen/objects.[ch] build/gen/gentables.[ch] build/gen/playtables.[ch] "$G/"
sed -i '' "s/^#define PIN_MAX 4096\$/#define PIN_MAX ${PIN:-1000}/" "$G/play.h"
grep -q "^#define PIN_MAX ${PIN:-1000}\$" "$G/play.h"; touch "$G/stamp"
# SNAP_FILE / SNAP_SED: an experimental edit of the snapshot (measurements only; src/game is never touched)
if [ -n "${SNAP_SED:-}" ]; then sed -i '' "$SNAP_SED" "$G/$SNAP_FILE"; fi
# snapcfg.h (tests/playsh2/core.c): where the snapshot keeps alpha
sed -n '/^struct pin_ext {/,/^};/p' "$G/play.h" | grep -q "alpha" && echo "#define ALPHA_IN_EXT 1" > "$G/snapcfg.h" || : > "$G/snapcfg.h"
scripts/dmake.sh $T OUT=build/$V OPT="${OPT:--O2}" PROF=${PROF:-0} PROF_KIND=${PROF_KIND:-3} PROF_SKIP=${PROF_SKIP:-1} PROF_WRAP=${PROF_WRAP:-0} SOFTFP=${SOFTFP:-0} ATTR=${ATTR:-0} FPCHECK=${FPCHECK:-0} >/dev/null
# host reference (the same core.c and jobs.h; one process per job)
cc -std=c99 -O2 -ffp-contract=off -w -I$T -I$B -I$G -o $B/host $T/host.c $T/core.c $T/sndstub.c src/snd/snd.c -Isrc/snd -Isrc/front -I../cps3-testgame/sdk/include $G/*.c -lm   # no contraction: as the SH-2 (pcol.c's fused operations are explicit fmaf)
N=$($B/host --count); j=0; : > "$O/host.txt"
while [ $j -lt $N ]; do $B/host $j >> "$O/host.txt"; j=$((j + 1)); done
[ "${ATTR:-0}" = 1 ] && export PSH2_ATTR=$(($(grep -c "^    OBJ_" $G/objects.h) - 1))   # OBJ_COUNT
PSH2_OUT="$O/sh2.txt" scripts/mame.sh sfiii3na -rompath "$E/mame" -skip_gameinfo -nothrottle -sound none -video none \
  -seconds_to_run ${SECONDS_TO_RUN:-40000} -cfg_directory "$O/w/cfg" -nvram_directory "$O/w/nvram" \
  -snapshot_directory "$O/w/snap" -diff_directory "$O/w/diff" -state_directory "$O/w/sta" -inipath "$O/w" \
  -autoboot_script scripts/lua/playsh2.lua >"$O/mame.log" 2>&1 || true
docker run --rm -v "$PWD":/p -w /p cps3-dev:latest sh-elf-nm -n $E/main.elf > "$O/nm.txt"
python3 $T/report.py "$O/host.txt" "$O/sh2.txt" "$O/nm.txt"
