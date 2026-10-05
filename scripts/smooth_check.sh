#!/bin/sh
# Smooth motion in MAME (src/draw draw_smooth; tests/game SMOOTH=1 on a route): each held record is shown as the
# cabinet shows it, the midpoint list first (draw_vblank at the step frame's VBlank), then the record's own list
# (draw_vbl_irq from the next VBlank interrupt, with the main list being built put back after the list DMA). The
# own frame must equal tools/drawmodel.py's model of the trace record (as scripts/game_check.sh); the midpoint
# frame is reported against the own frame (pixels that differ). Needs the src/game snapshot of a game_check run.
#   scripts/smooth_check.sh <route> <seed> <trace name> <rec,rec,...> [level money enemies]
#   e.g. scripts/smooth_check.sh p4_exit559 559 g_p4_exit559_s559 30,150,300
# ATTRACT=1 (or 5): the attract mode, as scripts/game_check.sh.
# Output: tests/game/build/<route>_smooth/ (snap/: own frames sfiii3na/<k>.png, midpoint frames mid_<k>.png; out.txt).
set -e
cd "$(dirname "$0")/.."
R=$1; S=$2; N=$3; RECS=$4; L=${5:-1}; M=${6:-0}; E=${7:-0}
T=tests/game; B=$T/build; O=$B/${R}${ATTRACT:+_attract$ATTRACT}_smooth; rm -rf "$O"; mkdir -p "$O/w"
[ -f "$B/g/stamp" ] || { echo "no src/game snapshot: run scripts/game_check.sh first"; exit 1; }
scripts/dmake.sh $T OUT=build/${R}${ATTRACT:+_attract$ATTRACT}_smooth/elf HUD=1 SMOOTH=1 ${ATTRACT:+ATTRACT=$ATTRACT} ROUTE=$R SEED=$S SNAPS=$RECS LEVEL=$L MONEY=$M ENEMIES=$E \
  >"$O/make.log" 2>&1 || { tail -20 "$O/make.log"; exit 1; }
n=$(echo "$RECS" | tr ',' '\n' | grep -c .)
GAME_OUT="$O/out.txt" GAME_NSNAPS=$n GAME_MIDSNAP=1 scripts/mame.sh sfiii3na -rompath "$O/elf/mame" -skip_gameinfo \
  -nothrottle -sound none -video none -seconds_to_run 20000 -cfg_directory "$O/w/cfg" -nvram_directory "$O/w/nvram" \
  -snapshot_directory "$O/snap" -diff_directory "$O/w/diff" -state_directory "$O/w/sta" -inipath "$O/w" \
  -autoboot_script scripts/lua/gamesnap.lua >"$O/mame.log" 2>&1 || true
rm -rf "$O/elf/mame" "$O"/elf/*.bin "$O/w"
fail=0; k=0; H=--hud
[ -n "$ATTRACT" ] && H="$H --sstext 37,27,CREDIT --sstext 45,27,0"     # the shell's credit line: CREDIT  0
for rec in $(echo "$RECS" | tr ',' ' '); do
  mid=$(printf '%s/snap/mid_%04d.png' "$O" $k)
  own=$(printf '%s/snap/sfiii3na/%04d.png' "$O" $k)
  [ -f "$own" ] || { echo "rec $rec: no snapshot"; fail=1; k=$((k + 1)); continue; }
  printf 'rec %s own frame: ' "$rec"
  python3 tools/drawmodel.py cmp build/trace/$N.bin build/trace/$N.names $rec build/gen "$own" $H \
    --mask "$O/diff_$rec.png" || fail=1
  python3 - "$mid" "$own" "$rec" <<'PY'
import sys
from PIL import Image
a = Image.open(sys.argv[1]).convert('RGB'); b = Image.open(sys.argv[2]).convert('RGB')
w, h = a.size
pa, pb = a.load(), b.load()
d = sum(1 for y in range(h) for x in range(w) if pa[x, y] != pb[x, y])
print(f'rec {sys.argv[3]} midpoint frame: {d} of {a.width * a.height} px differ from the own frame')
PY
  k=$((k + 1))
done
exit $fail
