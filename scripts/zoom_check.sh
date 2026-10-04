#!/bin/sh
# tests/zoom in MAME: a snapshot in the middle of each 300-frame phase (zoom 0x40, 0x35, 0x36) against
# tests/zoom/assets.py's expected screens.
set -e
cd "$(dirname "$0")/.."
scripts/dmake.sh tests/zoom >/dev/null
B=tests/zoom/build; O=$B/run; rm -rf "$O"; mkdir -p "$O/w"
python3 tests/zoom/assets.py --expect "$B"
SNAP_FRAMES=150,450,750 scripts/mame.sh sfiii3na -rompath "$B/mame" -skip_gameinfo -nothrottle -sound none -video none \
  -seconds_to_run 60 -cfg_directory "$O/w/cfg" -nvram_directory "$O/w/nvram" -snapshot_directory "$O/snap" \
  -diff_directory "$O/w/diff" -state_directory "$O/w/sta" -inipath "$O/w" -autoboot_script scripts/lua/snap.lua \
  >"$O/mame.log" 2>&1 || true
fail=0; k=0
for z in 40 35 36; do
  s=$(ls "$O"/snap/sfiii3na/*.png | sed -n "$((k + 1))p")
  printf 'zoom 0x%s: ' "$z"
  python3 ../cps3-testgame/tools/imgdiff.py "$B/expect_$z.png" "$s" --mask "$O/diff_$z.png" || fail=1
  k=$((k + 1))
done
exit $fail
