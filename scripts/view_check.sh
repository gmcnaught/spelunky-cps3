#!/bin/sh
# tests/view in MAME: one snapshot in the middle of each camera phase against tools/viewlevel.py's expected screens.
#   scripts/view_check.sh [trace] [names] [rec]     (default: build/trace/p3_shot record 46; TRACE_SHOT=1,46
#   scripts/hd_trace.sh p1_walk 1 p3_shot makes it)
set -e
cd "$(dirname "$0")/.."
T=${1:-build/trace/p3_shot.bin}; N=${2:-build/trace/p3_shot.names}; R=${3:-46}
CAMS="0,0 352,0 176,152 352,304 0,304 123,77"        # tests/view/main.c cams[]
python3 tools/hdobjects.py refs/hd/src build/gen/objects.h
python3 tools/hdsprites.py refs/hd/src build/gen >/dev/null
mkdir -p build/view
python3 tools/viewlevel.py c "$T" "$N" "$R" build/gen build/view/level.h
scripts/dmake.sh tests/view >/dev/null
B=tests/view/build; O=$B/run; rm -rf "$O"; mkdir -p "$O/w"
python3 tools/viewlevel.py expect "$T" "$N" "$R" build/gen "$B/expect" $CAMS
n=$(echo $CAMS | wc -w | tr -d ' ')
F=$(seq 0 $((n - 1)) | while read k; do printf '%d,' $((120 * k + 60)); done)
SNAP_FRAMES=$F mame sfiii3na -rompath "$B/mame" -skip_gameinfo -nothrottle -sound none -video none \
  -seconds_to_run 60 -cfg_directory "$O/w/cfg" -nvram_directory "$O/w/nvram" -snapshot_directory "$O/snap" \
  -diff_directory "$O/w/diff" -state_directory "$O/w/sta" -inipath "$O/w" -autoboot_script scripts/lua/snap.lua \
  >"$O/mame.log" 2>&1 || true
fail=0; k=0
for s in $(ls "$O"/snap/sfiii3na/*.png); do
  printf 'camera %s: ' "$(echo $CAMS | cut -d' ' -f$((k + 1)))"
  python3 ../cps3-testgame/tools/imgdiff.py "$B/expect/expect_$k.png" "$s" --mask "$O/diff_$k.png" || fail=1
  k=$((k + 1))
done
[ "$k" -eq "$n" ] || { echo "$k of $n snapshots"; fail=1; }
exit $fail
