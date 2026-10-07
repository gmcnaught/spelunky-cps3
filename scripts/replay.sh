#!/bin/sh
# A game capture (tools/capture.py decode: the settings screen's GAME CAPTURE pages) replayed on the host:
# tests/game/host.c HOST_REPLAY, built from the capture's own commit (its header's build; the working tree when
# that is 00000000 or not in this repository) with src/game as scripts/release.sh takes it (PIN_MAX 1792, unity TU).
# Prints the replay's end state and checkpoints against the capture's ("replay ..." lines; docs/ARCADE.md section 7).
#   scripts/replay.sh <capture.txt> [recs|all [frames dir]]
#     recs: record numbers ("120,121") or all: those frames as <frames dir>/f_<rec>.png (default build/replay/frames)
# Output in build/replay/<build>/: host, steps.log (HOST_STEPLOG: game_probe after each step), host.err.
set -e
cd "$(dirname "$0")/.."
C=$1; RECS=$2; FR=${3:-build/replay${RUNTAG:+_$RUNTAG}/frames}   # RUNTAG=<tag>: build/replay_<tag>
[ -f "$C" ] || { echo "usage: scripts/replay.sh <capture.txt> [recs|all [frames dir]]"; exit 2; }
REV=$(sed -n 's/^# game capture: build \([0-9a-f]*\).*/\1/p' "$C")
SEED=$(sed -n 's/^# seed \([0-9]*\)$/\1/p' "$C")
FLAGS=$(sed -n 's/^# flags \([0-9a-f]*\) .*/\1/p' "$C")
[ -n "$REV" ] && [ -n "$SEED" ] && [ -n "$FLAGS" ] || { echo "$C: not a capture (tools/capture.py decode)"; exit 2; }
D=build/replay${RUNTAG:+_$RUNTAG}/$REV; rm -rf "$D"; mkdir -p "$D/t"
if [ "$REV" != 00000000 ] && [ $((0x$FLAGS & 0x80)) = 0 ] && git cat-file -e "$REV^{commit}" 2>/dev/null; then
  git archive "$REV" src tests/game/host.c build/gen build/snd/snd.h scripts/unity.sh | tar -x -C "$D/t"
  echo "replay: build $REV"
else
  mkdir -p "$D/t/tests/game" "$D/t/build/snd" "$D/t/scripts"
  cp -R src "$D/t/"; cp tests/game/host.c "$D/t/tests/game/"; cp -R build/gen "$D/t/build/"
  cp build/snd/snd.h "$D/t/build/snd/"; cp scripts/unity.sh "$D/t/scripts/"
  echo "replay: build $REV with uncommitted changes (dirty) or not in this repository: the working tree"
fi
[ $((0x$FLAGS & 0x20)) = 0 ] || echo "replay: the capture is truncated (its buffer was full): steps after it are not replayed"
G=$D/t/g; mkdir -p "$G"; cp -R "$D/t/src/game/." "$G/"
cp "$D"/t/build/gen/objects.[ch] "$D"/t/build/gen/gentables.[ch] "$D"/t/build/gen/playtables.[ch] "$G/"
sed "s/^#define PIN_MAX [0-9][0-9]*\$/#define PIN_MAX 1792/" "$G/play.h" > "$G/play.tmp" && mv "$G/play.tmp" "$G/play.h"
grep -q "^#define PIN_MAX 1792\$" "$G/play.h"
sh "$D/t/scripts/unity.sh" "$G"
DEV=; [ $((0x$FLAGS & 0x08)) = 0 ] || DEV=-DSHELL_DEV          # a DEV=1 build (INVINCIBLE)
T=$D/t; SDK=../cps3-testgame/sdk/include
python3 -c "import re,sys; t=open(sys.argv[1]).read(); e=t[t.index('enum snd'):t.index('SND_COUNT')]; n=re.findall(r'SND_(\w+),', e); open(sys.argv[2],'w').write('static const char *const sndnames[%d] = {%s};\n' % (len(n), ', '.join('\"%s\"' % x for x in n)))" \
  $T/build/snd/snd.h $T/build/snd/sndnames.h              # as test/host/Makefile makes it
cc -std=gnu99 -O2 -ffp-contract=off -w -DDRAW_HOST -DSND_LOG $DEV -I$T/build/snd -I$SDK -I$G -I$T/build/gen \
  -I$T/src/draw -I$T/src/main -I$T/src/hud -I$T/src/shell -I$T/src/snd -I$T/src/front -o "$D/host" \
  $T/tests/game/host.c $T/src/main/game.c $T/src/draw/draw.c $T/src/hud/hud.c $T/build/gen/sprites.c \
  $T/src/front/front.c $T/build/gen/fronttables.c $T/src/snd/snd.c $T/src/shell/hiscore.c $T/build/gen/drawtab.c \
  $G/*.c -lm
# the cabinet's game: level 1, money 0, enemies on (src/main game_cfg's defaults)
if [ -n "$RECS" ]; then
  HOST_REPLAY=1 HOST_STEPLOG=${HOST_STEPLOG:-$D/steps.log} "$D/host" "$C" "$SEED" 1 0 1 0 $T/build/gen - "$RECS" 2> "$D/host.err" |
    python3 tools/capture.py frames $T/build/gen "$FR"
else
  HOST_REPLAY=1 HOST_STEPLOG=${HOST_STEPLOG:-$D/steps.log} "$D/host" "$C" "$SEED" 1 0 1 0 $T/build/gen "$D" 2> "$D/host.err"
fi
grep -E "^(end|replay|untranslated)" "$D/host.err"
