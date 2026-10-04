#!/bin/sh
# tests/hud in MAME: one snapshot in the middle of each case against tools/hudcheck.py's model screens.
#   scripts/hud_check.sh
set -e
cd "$(dirname "$0")/.."
python3 tools/hdsprites.py refs/hd/src build/gen >/dev/null
python3 tools/hudart.py refs/hd/src build/gen >/dev/null
mkdir -p build/hud
python3 tools/hudcheck.py cases tests/hud/cases.json build/hud/cases.h
scripts/dmake.sh tests/hud >/dev/null
B=tests/hud/build; O=$B/run; rm -rf "$O"; mkdir -p "$O/w"
python3 tools/hudcheck.py expect tests/hud/cases.json refs/hd/src "$B/expect"
n=$(python3 -c "import json; print(len(json.load(open('tests/hud/cases.json'))))")
F=$(seq 0 $((n - 1)) | while read k; do printf '%d,' $((120 * k + 60)); done)
SNAP_FRAMES=$F scripts/mame.sh sfiii3na -rompath "$B/mame" -skip_gameinfo -nothrottle -sound none -video none \
  -seconds_to_run 60 -cfg_directory "$O/w/cfg" -nvram_directory "$O/w/nvram" -snapshot_directory "$O/snap" \
  -diff_directory "$O/w/diff" -state_directory "$O/w/sta" -inipath "$O/w" -autoboot_script scripts/lua/snap.lua \
  >"$O/mame.log" 2>&1 || true
fail=0; k=0
for s in $(ls "$O"/snap/sfiii3na/*.png); do
  printf 'case %d: ' $k
  python3 ../cps3-testgame/tools/imgdiff.py "$B/expect/expect_$k.png" "$s" --mask "$O/diff_$k.png" || fail=1
  k=$((k + 1))
done
[ "$k" -eq "$n" ] || { echo "$k of $n snapshots"; fail=1; }
exit $fail
