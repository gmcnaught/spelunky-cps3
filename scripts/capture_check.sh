#!/bin/sh
# The game capture end to end in MAME (docs/ARCADE.md section 7): tests/game PLAY=1 built as scripts/release.sh
# builds it; scripts/lua/capture.lua plays a cabinet game (coin, Start, random controls, bombs until life 0, the
# game over panel) logging game_probe after every step, then opens the settings screen's GAME CAPTURE and snapshots
# its pages, twice (before and after the settings screen's restart). tools/capture.py decodes both sets (they must be
# equal), scripts/replay.sh replays the capture on the host, and tools/capture.py steps compares the host's step log
# with MAME's. Exit 0 when every step is equal and the replay's end state and checkpoints equal the capture's.
#   scripts/capture_check.sh       [CAP_MONKEY=n (the controls) CAP_START=frame (the Start: the seed) CAP_PLAY=frames]
#                                  [DEV=1: a dev build; CAP_GOD=1: a dev build, INVINCIBLE on, the capture taken mid-game]
# Output: build/capture/ (cabinet.log, capture.txt, capture2.txt, host steps.log, snap/, the result lines)
set -e
cd "$(dirname "$0")/.."
O=build/capture; T=tests/game; G=$T/build/g
rm -rf "$O" "$T/build/capture"; mkdir -p "$O/w"
rm -rf "$G"; mkdir -p "$G"
git archive HEAD src/game | tar -x -C "$G" --strip-components=2
for f in $GAME_FILES; do cp "src/game/$f" "$G/$f"; done
cp build/gen/objects.[ch] build/gen/gentables.[ch] build/gen/playtables.[ch] "$G/"
sed "s/^#define PIN_MAX [0-9][0-9]*\$/#define PIN_MAX 1792/" "$G/play.h" > "$G/play.tmp" && mv "$G/play.tmp" "$G/play.h"
grep -q "^#define PIN_MAX 1792\$" "$G/play.h"
scripts/unity.sh "$G"
touch "$G/stamp"
REV=$(git rev-parse --short=8 HEAD); DIRTY=0; git diff --quiet HEAD -- src tests/game build/gen || DIRTY=1
[ "${CAP_GOD:-0}" = 1 ] && DEV=1                     # INVINCIBLE is in dev builds only
scripts/dmake.sh $T OUT=build/capture PLAY=1 REV=$REV DIRTY=$DIRTY ${DEV:+DEV=$DEV} > "$O/make.log" 2>&1 ||
  { tail -20 "$O/make.log"; exit 1; }
CAP=$(awk '$2 == "__capture_start" { print $1 }' $T/build/capture/main.map)
[ -n "$CAP" ] || { echo "no capture symbol in main.map"; exit 1; }
CAP_LOG=$O/cabinet.log CAP_ADDR=$CAP CAP_MONKEY=${CAP_MONKEY:-1} CAP_START=${CAP_START:-200} CAP_PLAY=${CAP_PLAY:-6000} \
CAP_SHOTS=${CAP_SHOTS:-24} scripts/mame.sh sfiii3na -rompath $T/build/capture/mame -skip_gameinfo -nothrottle \
  -sound none -video none -seconds_to_run ${SECONDS_TO_RUN:-3000} -cfg_directory "$O/w/cfg" -nvram_directory "$O/w/nvram" \
  -snapshot_directory "$O/snap" -diff_directory "$O/w/diff" -state_directory "$O/w/sta" -inipath "$O/w" \
  -autoboot_script scripts/lua/capture.lua > "$O/mame.log" 2>&1 || true
rm -rf $T/build/capture/mame $T/build/capture/*.bin
n1=$(sed -n 's/^SHOTS 1 \([0-9]*\)/\1/p' "$O/cabinet.log"); n2=$(sed -n 's/^SHOTS 2 \([0-9]*\)/\1/p' "$O/cabinet.log")
[ -n "$n1" ] && [ -n "$n2" ] || { echo "MAME did not finish both snapshot sets"; tail -5 "$O/mame.log"; exit 1; }
S=$(ls "$O"/snap/sfiii3na/*.png | sort)
python3 tools/capture.py decode $(echo "$S" | head -n "$n1") -o "$O/capture.txt" > "$O/decode1.txt"
python3 tools/capture.py decode $(echo "$S" | tail -n "$n2") -o "$O/capture2.txt" > "$O/decode2.txt"
tail -1 "$O/decode1.txt"
fail=0
if cmp -s "$O/capture.txt" "$O/capture2.txt"; then echo "after the restart: the same capture"
else echo "after the restart: a DIFFERENT capture"; fail=1; fi
HOST_STEPLOG=$O/steps.log scripts/replay.sh "$O/capture.txt" | tee "$O/replay.txt"
grep -q "^replay end capture.*-> equal" "$O/replay.txt" || fail=1
grep -q "^replay steps .*checkpoints \([0-9]*\) of \1 equal$" "$O/replay.txt" || fail=1
python3 tools/capture.py steps "$O/cabinet.log" "$O/steps.log" || fail=1
exit $fail
