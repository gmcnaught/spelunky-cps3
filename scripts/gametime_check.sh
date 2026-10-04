#!/bin/sh
# tests/gametime in MAME: the game program's frame budget on a route (VBlank work, sound, step, draw, shell; per
# 2-frame step pair), a snapshot of its results screen, and the numbers.
#   scripts/gametime_check.sh [route seed [level money enemies]]     default p4_push_rope 365
# -> tests/gametime/build/<route>/{gt.txt, snap/.., elf/mame (the set: the same program runs on jtcps3)}
set -e
cd "$(dirname "$0")/.."
R=${1:-p4_push_rope}; S=${2:-365}; L=${3:-1}; M=${4:-0}; E=${5:-0}
T=tests/gametime; B=$T/build; O=$B/$R; rm -rf "$O"; mkdir -p "$O/w"
G=$B/g; rm -rf "$G"; mkdir -p "$G"
git archive "${GAME_REV:-HEAD}" src/game | tar -x -C "$G" --strip-components=2
cp build/gen/objects.[ch] build/gen/gentables.[ch] build/gen/playtables.[ch] "$G/"
sed -i '' "s/^#define PIN_MAX 4096\$/#define PIN_MAX ${PIN:-1000}/" "$G/play.h"
grep -q "^#define PIN_MAX ${PIN:-1000}\$" "$G/play.h"; touch "$G/stamp"
python3 tools/drawtables.py refs/hd/src build/gen >/dev/null
scripts/dmake.sh $T OUT=build/$R/elf ROUTE=$R SEED=$S LEVEL=$L MONEY=$M ENEMIES=$E >"$O/make.log" 2>&1 || { tail -20 "$O/make.log"; exit 1; }
GT_OUT="$O/gt.txt" mame sfiii3na -rompath "$O/elf/mame" -skip_gameinfo -nothrottle -sound none -video none \
  -seconds_to_run ${SECONDS_TO_RUN:-6000} -cfg_directory "$O/w/cfg" -nvram_directory "$O/w/nvram" \
  -snapshot_directory "$O/snap" -diff_directory "$O/w/diff" -state_directory "$O/w/sta" -inipath "$O/w" \
  -autoboot_script scripts/lua/gametime.lua >"$O/mame.log" 2>&1 || true
rm -rf "$O/w"
python3 - "$O/gt.txt" <<'PY'
import sys
M, P = {}, []
for l in open(sys.argv[1]):
    f = l.split()
    if f[0] == 'M': M[f[1]] = int(f[2])
    if f[0] == 'P': P.append(int(f[2]))
n, fr = max(M['steps'], 1), max(M['frames'], 1)
ps = sorted(P)
q = lambda x: ps[min(len(ps) - 1, int(x * len(ps)))] if ps else 0
print('frames %d, steps %d, pairs %d (MAME clocks; a frame is 419,470, a pair 838,940)' % (M['frames'], M['steps'], M['pairs']))
for k, d in (('vbl', fr), ('snd', fr), ('step', n), ('draw', n), ('shl', max(fr - n, 1))):
    print('  %-5s mean %9.0f  max %9d' % (k, M[k + '_sum'] / d, M[k + '_max']))
print('  pair  mean %9.0f  median %d  p90 %d  p99 %d  max %d (pair %d); over 2 frames %d' % (
    (M['pair_sum_lo'] + 2 ** 32 * M['pair_sum_hi']) / max(M['pairs'], 1), q(.5), q(.9), q(.99), M['pair_max'],
    M['pair_max_at'], M['over']))
print('  level start (game_begin) %d' % M['start_clk'])
PY
ls "$O"/snap/*/*.png
