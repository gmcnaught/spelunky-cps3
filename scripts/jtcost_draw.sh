#!/bin/sh
# Modelled jtcps3 cost of single draws (src/draw draw_frame) of the game program on a route: tests/game built as
# scripts/game_check.sh builds it (no snapshots held; SMOOTH=1 by default, as the cabinet), MAME (-nodrc -debug)
# traces draw_frame from its entry to its return (scripts/lua/jtcost.lua with JTC_TAP = the marker's route steps,
# 0x02000050: the window opens when the program has drawn step <step> - 1, so the traced draw is step <step>'s),
# tools/jtcost.py (JTC_FN=_draw_frame) reduces each trace; the traces are deleted.
#   [GAME_REV=<rev>] [SMOOTH=0] [RUNTAG=<tag>] scripts/jtcost_draw.sh <route> <seed> <level> <money> <enemies> <step>...
#   e.g. scripts/jtcost_draw.sh c_swamp_drain 113 5 0 1 150 271
# Output: tests/game/build[/t_<RUNTAG>]/jtd_<route>/jtcost_draw_<route>_<step>.txt; the JTCOST lines on stdout.
set -e
cd "$(dirname "$0")/.."
[ $# -ge 6 ] || { echo "usage: scripts/jtcost_draw.sh <route> <seed> <level> <money> <enemies> <step>..." >&2; exit 2; }
R=$1; S=$2; L=$3; M=$4; E=$5; shift 5
BD=build${RUNTAG:+/t_$RUNTAG}
T=tests/game; B=$T/$BD; O=$B/jtd_$R; TD=$PWD/$O/trace; rm -rf "$O"; mkdir -p "$O/w"
G=$B/g; rm -rf "$G"; mkdir -p "$G"
git archive "${GAME_REV:-HEAD}" src/game | tar -x -C "$G" --strip-components=2
cp build/gen/objects.[ch] build/gen/gentables.[ch] build/gen/playtables.[ch] "$G/"
sed -i '' "s/^#define PIN_MAX [0-9][0-9]*\$/#define PIN_MAX ${PIN:-1792}/" "$G/play.h"
grep -q "^#define PIN_MAX ${PIN:-1792}\$" "$G/play.h"
scripts/unity.sh "$G"
touch "$G/stamp"
python3 tools/drawtables.py refs/hd/src build/gen >/dev/null
scripts/dmake.sh $T W=$BD OUT=$BD/jtd_$R/elf HUD=1 ROUTE=$R SEED=$S SNAPS= LEVEL=$L MONEY=$M ENEMIES=$E OPT="${OPT:--O2}" \
  SMOOTH=${SMOOTH:-1} >"$O/make.log" 2>&1 || { tail -20 "$O/make.log"; exit 1; }
NM=$O/nm.txt
docker run --rm -v "$PWD":/p -w /p cps3-dev:latest sh-elf-nm -n "$O/elf/main.elf" > "$NM"
FN=$(awk '$3 == "_draw_frame" { print $1 }' "$NM")
[ -n "$FN" ] || { echo "jtcost_draw.sh: no _draw_frame in $NM" >&2; exit 1; }
rm -rf "$TD"; mkdir -p "$TD"; trap 'rm -rf "$TD"' EXIT INT TERM
WIN=; k=0; last=0
for s in "$@"; do
  [ "$s" -gt "$last" ] || { echo "jtcost_draw.sh: steps must ascend" >&2; exit 2; }
  last=$s; k=$((k + 1)); WIN="${WIN:+$WIN,}$((s - 1)):$TD/t$k.tr"
done
JTC_TAP=02000050 JTC_STEP=$FN JTC_WIN=$WIN scripts/mame.sh sfiii3na -rompath "$O/elf/mame" -skip_gameinfo -nothrottle \
  -sound none -video none -nodrc -debug -debugger none -seconds_to_run ${SECONDS_TO_RUN:-40000} -cfg_directory "$O/w/cfg" \
  -nvram_directory "$O/w/nvram" -snapshot_directory "$O/w/snap" -diff_directory "$O/w/diff" -state_directory "$O/w/sta" \
  -inipath "$O/w" -autoboot_script scripts/lua/jtcost.lua >"$O/mame.log" 2>&1 || true
k=0
for s in "$@"; do
  k=$((k + 1)); f=$O/jtcost_draw_${R}_$s.txt
  if [ ! -s "$TD/t$k.tr" ]; then echo "jtcost_draw.sh: no trace for $R $s (see $O/mame.log)" >&2; exit 1; fi
  JTC_FN=_draw_frame python3 tools/jtcost.py --route "$R" --step "$s" "$TD/t$k.tr" "$NM" > "$f"
  rm -f "$TD/t$k.tr"
  grep '^JTCOST ' "$f"
done
rm -rf "$O/elf/mame" "$O"/elf/*.bin "$O/w"
