#!/bin/sh
# The game program's display list at every step, on the host (tests/game/host.c: src/main/game.c, src/draw, src/hud
# and the src/game snapshot of scripts/game_check.sh, the SDK's video calls recorded and composed as the CPS3 shows
# them), against tools/drawmodel.py's model of the HD runner's trace records; frames streamed (no files).
#   scripts/game_host.sh <route> <seed> <trace name> [level money enemies] [HUD=0|1]
#   e.g. scripts/game_host.sh p5_shop 96 g_p5_shop_s96 2 40000 1
# HUD=0 (default): no HUD on either side; HUD=1: HD's HUD on both (global.collect's "+N" line and the messages are
# not in the trace: those frames differ in that line). DARK=a8: both sides drawn as a dark level at that alpha byte
# (the fade path on a level that is not dark).
set -e
cd "$(dirname "$0")/.."
R=$1; S=$2; N=$3; L=${4:-1}; M=${5:-0}; E=${6:-0}
G=tests/game/build${RUNTAG:+/t_$RUNTAG}/g; HD=build/game/host${RUNTAG:+/t_$RUNTAG}   # RUNTAG: game_check.sh RUNTAG's snapshot, own output
[ -f "$G/stamp" ] || { echo "no src/game snapshot: run scripts/game_check.sh first"; exit 1; }
SDK=../cps3-testgame/sdk/include
mkdir -p $HD
cc -std=gnu99 -O2 -ffp-contract=off -w -DDRAW_HOST -DSND_LOG -Ibuild/snd -I$SDK -I$G -Ibuild/gen -Isrc/draw -Isrc/main -Isrc/hud -Isrc/shell \
  -Isrc/snd -Isrc/front -o $HD/host tests/game/host.c src/main/game.c src/draw/draw.c src/hud/hud.c build/gen/sprites.c \
  src/front/front.c build/gen/fronttables.c src/snd/snd.c src/shell/hiscore.c \
  build/gen/drawtab.c $G/*.c -lm
if [ "${HUD:-0}" = 1 ]; then H=hud; F=--hud; else H=nohud; F=; fi
$HD/host tests/routes/$R.txt $S $L $M $E 30 build/gen - all $H ${DARK:+$DARK} 2>$HD/stderr.txt |
  python3 tools/drawmodel.py hostcmp build/trace/$N.bin build/trace/$N.names build/gen - $F ${DARK:+--dark $DARK}
grep -h "^draw: " $HD/stderr.txt | sort | uniq -c | head -5
