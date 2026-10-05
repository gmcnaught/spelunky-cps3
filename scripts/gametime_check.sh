#!/bin/sh
# tests/gametime in MAME: the game program's frame budget in three sections, the attract mode (src/front,
# ATTRACT_FRAMES frames from boot), game 1 and game 2 (each from a coin and Start): VBlank work, sound, step, draw,
# shell, per 2-frame step pair; a snapshot of its results screen and the numbers.
#   [ATTRACT_FRAMES=3600] scripts/gametime_check.sh [route seed level money enemies  route2 seed2 level2 money2 enemies2]
#   default p4_push_rope 365 1 0 0  p5_snakes 101 1 0 1
# -> tests/gametime/build/run/{gt.txt, snap/.., elf/mame (the set: the same program runs on jtcps3), elf/mister}
set -e
cd "$(dirname "$0")/.."
R=${1:-p4_push_rope}; S=${2:-365}; L=${3:-1}; M=${4:-0}; E=${5:-0}
R2=${6:-p5_snakes}; S2=${7:-101}; L2=${8:-1}; M2=${9:-0}; E2=${10:-1}
T=tests/gametime; B=$T/build; O=$B/run; rm -rf "$O"; mkdir -p "$O/w"
G=$B/g; rm -rf "$G"; mkdir -p "$G"
git archive "${GAME_REV:-HEAD}" src/game | tar -x -C "$G" --strip-components=2
cp build/gen/objects.[ch] build/gen/gentables.[ch] build/gen/playtables.[ch] "$G/"
sed -i '' "s/^#define PIN_MAX [0-9][0-9]*\$/#define PIN_MAX ${PIN:-1792}/" "$G/play.h"
grep -q "^#define PIN_MAX ${PIN:-1792}\$" "$G/play.h"
# EXACT=1: the exact build (src/game/pcol.c with PCOL_EXACT: the runner's R-tree order in play); default the
# shipping build's collision grid (src/game/pcolgrid.h). GRID_SHIFT=<n>: grid cells of 2^n px (default 4)
if [ "${EXACT:-0}" = 1 ]; then { echo "#define PCOL_EXACT 1"; cat "$G/pcol.c"; } > "$G/pcol.tmp" && mv "$G/pcol.tmp" "$G/pcol.c"; fi
if [ -n "${GRID_SHIFT:-}" ]; then { echo "#define PCOL_GRID_SHIFT $GRID_SHIFT"; cat "$G/pcol.c"; } > "$G/pcol.tmp" && mv "$G/pcol.tmp" "$G/pcol.c"; fi
scripts/unity.sh "$G"                                  # the hot play files as one TU (UNITY=0: separate)
touch "$G/stamp"
python3 tools/drawtables.py refs/hd/src build/gen >/dev/null
MK="OUT=build/run/elf ROUTE=$R SEED=$S LEVEL=$L MONEY=$M ENEMIES=$E ROUTE2=$R2 SEED2=$S2 LEVEL2=$L2 MONEY2=$M2 ENEMIES2=$E2 ATTRACT_FRAMES=${ATTRACT_FRAMES:-3600}"
scripts/dmake.sh $T $MK >"$O/make.log" 2>&1 || { tail -20 "$O/make.log"; exit 1; }
scripts/dmake.sh $T $MK mister >>"$O/make.log" 2>&1
GT_OUT="$O/gt.txt" scripts/mame.sh sfiii3na -rompath "$O/elf/mame" -skip_gameinfo -nothrottle -sound none -video none \
  -seconds_to_run ${SECONDS_TO_RUN:-6000} -cfg_directory "$O/w/cfg" -nvram_directory "$O/w/nvram" \
  -snapshot_directory "$O/snap" -diff_directory "$O/w/diff" -state_directory "$O/w/sta" -inipath "$O/w" \
  -autoboot_script scripts/lua/gametime.lua >"$O/mame.log" 2>&1 || true
rm -rf "$O/w"
python3 - "$O/gt.txt" <<'PY'
import sys
M = {}
for l in open(sys.argv[1]):
    f = l.split()
    M[(int(f[1]), f[2])] = [int(v) for v in f[3:]]
print('MAME clocks (a frame is 419,470, a step pair 838,940)')
names = ('attract', 'game 1', 'game 2')
for s in range(3):
    g = lambda k: M[(s, k)][0]
    P = sorted(M.get((s, 'pair'), []))
    n, fr = max(g('steps'), 1), max(g('frames'), 1)
    q = lambda x: P[min(len(P) - 1, int(x * len(P)))] if P else 0
    print('%s: frames %d, steps %d, pairs %d' % (names[s], g('frames'), g('steps'), g('pairs')))
    for k, d in (('vbl', fr), ('snd', fr), ('step', n), ('draw', n), ('shl', max(fr - n, 1))):
        print('  %-5s mean %9.0f  max %9d' % (k, g(k + '_sum') / d, g(k + '_max')))
    print('  pair  mean %9.0f  median %d  p90 %d  p99 %d  max %d (pair %d); over 2 frames %d' % (
        (g('pair_sum_lo') + 2 ** 32 * g('pair_sum_hi')) / max(g('pairs'), 1), q(.5), q(.9), q(.99), g('pair_max'),
        g('pair_max_at'), g('over')))
    U = M.get((s, 'pair'), [])
    if U: print('  largest pairs (pair: clocks; pair k opens at the game\'s step k + 1): %s' % ', '.join(
        '%d: %d' % (k, U[k]) for k in sorted(range(len(U)), key=lambda k: -U[k])[:5]))
    if s: print('  game start (frames before the first step, the largest: game_begin) %d' % g('start_clk'))
PY
ls "$O"/snap/*/*.png
