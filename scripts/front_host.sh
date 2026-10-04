#!/bin/sh
# The front end (src/front: HD's intro / title / high-scores rooms as the attract mode) on the host, every step,
# against tools/drawmodel.py's model of a boot trace (tools/tracer.py TRACE_BOOT=1: scripts/front_traces.sh).
# The src/game snapshot of scripts/game_check.sh with tests/game/front_hooks.patch (the hooks src/game is to carry,
# src/front/front.h) applied in build/front/g.
#   scripts/front_host.sh <seed> <steps> <trace name> [room]   e.g. scripts/front_host.sh 7 1300 g_p8_boot_s7
#   room 5: the cycle starts in rHighscores (trace: TRACE_ROOM=rHighscores, g_p8_scores_s7)
set -e
cd "$(dirname "$0")/.."
S=$1; N=$2; T=$3
G=tests/game/build/g; F=build/front/g
[ -f "$G/stamp" ] || { echo "no src/game snapshot: run scripts/game_check.sh first"; exit 1; }
rm -rf "$F"; mkdir -p "$F"; cp "$G"/*.c "$G"/*.h "$F/"
grep -q "front_ev" "$F/pobj.c" || patch -s -d "$F" -p3 < tests/game/front_hooks.patch
SDK=../cps3-testgame/sdk/include
cc -std=gnu99 -O2 -ffp-contract=off -w -DDRAW_HOST -I$SDK -I$F -Ibuild/gen -Isrc/draw -Isrc/main -Isrc/hud -Isrc/shell \
  -Isrc/snd -Isrc/front -o build/front/host tests/game/host.c src/main/game.c src/draw/draw.c src/hud/hud.c \
  src/front/front.c build/gen/sprites.c build/gen/drawtab.c build/gen/fronttables.c src/snd/snd.c $F/*.c -lm
build/front/host attract $S $N build/gen - $4 2>build/front/host.log | \
  python3 tools/drawmodel.py hostcmp build/trace/$T.bin build/trace/$T.names build/gen - ${HUD:+--hud} ${SAVE:+--save $SAVE}
