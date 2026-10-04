#!/bin/sh
# tests/game in MAME (the game program: shell -> play_step -> draw on a route): at the given records the MAME frame
# must equal tools/drawmodel.py's model of the HD runner's trace record (with the HUD), and the model's view the
# runner's own frame where a TRACE_SHOT frame exists (build/trace/<name>.shot_<rec>.png). 5-bit colour.
#   scripts/game_check.sh <route> <seed> <trace name> <rec,rec,...> [level money enemies]
#   e.g. scripts/game_check.sh p4_exit559 559 g_p4_exit559_s559 30,150,300
# Reference traces with frames: TRACE_SHOT=<recs> scripts/hd_trace.sh (build/game/traces.sh: the gate's three).
# src/game is taken at GAME_REV (default HEAD) with PIN_MAX 1000, as scripts/playsh2_check.sh does.
# HUD=0: the program draws no HUD (the runner's frames have none); HUD=1 (default): the model draws HD's HUD from the
# record's globals (TRACE_HUD traces: collect, messages, drawHUD, the transition's text); --gui frames (TRACE_GUI) too.
# ATTRACT=1 (or 5: from rHighscores): the attract mode (src/front), no coin / start; the route only gives the seed
# and the length. DPROF=1: the draw's parts timed. DARK=a8: the program and the model draw a dark level at that alpha byte (the fade
# path on a level that is not dark; the runner frames then differ, as expected). --host is done by scripts/game_host.sh (every step, on the host).
# Output: tests/game/build/<route>/ (snapshots, out.txt: per-snapshot draw stats, diff masks).
set -e
cd "$(dirname "$0")/.."
R=$1; S=$2; N=$3; RECS=$4; L=${5:-1}; M=${6:-0}; E=${7:-0}
T=tests/game; B=$T/build; O=$B/$R${HUD:+_hud$HUD}${DARK:+_dark$DARK}${ATTRACT:+_attract$ATTRACT}${HOLD:+_hold}; rm -rf "$O"; mkdir -p "$O/w"
G=$B/g; rm -rf "$G"; mkdir -p "$G"
git archive "${GAME_REV:-HEAD}" src/game | tar -x -C "$G" --strip-components=2
# GAME_FILES="pplayer.c penemy.c ...": those src/game files from the working tree over the snapshot (uncommitted
# work in them; the other files stay at GAME_REV)
for f in $GAME_FILES; do cp "src/game/$f" "$G/$f"; done
cp build/gen/objects.[ch] build/gen/gentables.[ch] build/gen/playtables.[ch] "$G/"
sed -i '' "s/^#define PIN_MAX 4096\$/#define PIN_MAX ${PIN:-1000}/" "$G/play.h"
grep -q "^#define PIN_MAX ${PIN:-1000}\$" "$G/play.h"
# EXACT=1: the exact build (src/game/pcol.c with PCOL_EXACT: the runner's R-tree order in play); default the
# shipping build's collision grid (src/game/pcolgrid.h). GRID_SHIFT=<n>: grid cells of 2^n px (default 4)
if [ "${EXACT:-0}" = 1 ]; then { echo "#define PCOL_EXACT 1"; cat "$G/pcol.c"; } > "$G/pcol.tmp" && mv "$G/pcol.tmp" "$G/pcol.c"; fi
if [ -n "${GRID_SHIFT:-}" ]; then { echo "#define PCOL_GRID_SHIFT $GRID_SHIFT"; cat "$G/pcol.c"; } > "$G/pcol.tmp" && mv "$G/pcol.tmp" "$G/pcol.c"; fi
touch "$G/stamp"
python3 tools/drawtables.py refs/hd/src build/gen >/dev/null
scripts/dmake.sh $T OUT=build/$R${HUD:+_hud$HUD}${DARK:+_dark$DARK}${ATTRACT:+_attract$ATTRACT}${HOLD:+_hold}/elf HUD=${HUD:-1} ROUTE=$R SEED=$S SNAPS=$RECS LEVEL=$L MONEY=$M ENEMIES=$E OPT="${OPT:--O2}" DPROF=${DPROF:-0} ${DARK:+DARK=$DARK} ${ATTRACT:+ATTRACT=$ATTRACT} ${HOLD:+HOLD=$HOLD} \
  >"$O/make.log" 2>&1 || { tail -20 "$O/make.log"; exit 1; }
# HOLD=n (jtcps3 frame check, scripts/jt_frames.sh): the SNAPS frames are held n frames each by the program itself;
# the MiSTer set (zip + MRA) is built and kept in $O/elf/mister
[ -n "$HOLD" ] && { scripts/dmake.sh $T OUT=build/$R${HUD:+_hud$HUD}${DARK:+_dark$DARK}${ATTRACT:+_attract$ATTRACT}_hold/elf HUD=${HUD:-1} ROUTE=$R SEED=$S SNAPS=$RECS LEVEL=$L MONEY=$M ENEMIES=$E OPT="${OPT:--O2}" DPROF=${DPROF:-0} ${ATTRACT:+ATTRACT=$ATTRACT} HOLD=$HOLD TITLE="Spelunky frame check $R" mister >>"$O/make.log" 2>&1 || { tail -20 "$O/make.log"; exit 1; }; }
n=$(echo "$RECS" | tr ',' '\n' | grep -c .)
GAME_OUT="$O/out.txt" GAME_NSNAPS=$n scripts/mame.sh sfiii3na -rompath "$O/elf/mame" -skip_gameinfo -nothrottle -sound none \
  -video none -seconds_to_run ${SECONDS_TO_RUN:-20000} -cfg_directory "$O/w/cfg" -nvram_directory "$O/w/nvram" \
  -snapshot_directory "$O/snap" -diff_directory "$O/w/diff" -state_directory "$O/w/sta" -inipath "$O/w" \
  -autoboot_script scripts/lua/gamesnap.lua >"$O/mame.log" 2>&1 || true
rm -rf "$O/elf/mame" "$O"/elf/*.bin "$O/w"         # the ROM set (80 MB) is not kept: the disk is small
fail=0; k=0; H=--hud; [ "${HUD:-1}" = 0 ] && H=
[ -n "$ATTRACT" ] && H="$H --sstext 37,27,CREDIT --sstext 45,27,0"     # the shell's credit line: CREDIT  0
[ -n "$DARK" ] && H="$H --dark $DARK"
for rec in $(echo "$RECS" | tr ',' ' '); do
  s=$(printf '%s/snap/sfiii3na/%04d.png' "$O" $k)
  [ -f "$s" ] || { echo "rec $rec: no snapshot"; fail=1; k=$((k + 1)); continue; }
  shot=build/trace/$N.shot_$rec.png; gui=build/trace/$N.gui_$rec.png
  G2=; [ -f "$gui" ] && [ -n "$H" ] && G2="--gui $gui"
  if [ -f "$shot" ]; then
    python3 tools/drawmodel.py cmp build/trace/$N.bin build/trace/$N.names $rec build/gen "$s" $H \
      --mask "$O/diff_$rec.png" --shot "$shot" $G2 || fail=1
  else
    python3 tools/drawmodel.py cmp build/trace/$N.bin build/trace/$N.names $rec build/gen "$s" $H \
      --mask "$O/diff_$rec.png" || fail=1
  fi
  k=$((k + 1))
done
grep '^[SE]' "$O/out.txt" | awk '$1 == "S" { printf "rec %s: draw %d clocks (parts %d %d %d %d %d), vblank %d, entries %d, drawables %d, cells %d, unsup %d todo %d noart %d\n", $3, $5, $21, $22, $23, $24, $25, $7, $9, $11, $12, $13, $14, $15 }
  $1 == "E" { printf "draws %d, mean draw %.0f clocks, max draw %d, max vblank %d, max entries %d, dropped %d\n", $4, $3 / ($4 ? $4 : 1), $6, $8, $10, $16 }'
exit $fail
