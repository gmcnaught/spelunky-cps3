#!/bin/sh
# Olmec beaten, then the ending, on the host: the game program (tests/game/host.c, src/main game.c, src/front) plays
# tests/routes/end_olmec.txt from rOlmec (HOST_ROOM 3: the route's "# room rOlmec", level 16, enemies kept): Olmec
# drowns, rOlmec's oFinalBoss opens the door (oXEnd at 640, 544), the player goes in (oPlayer1 Step :676-760,
# Other_7 :133) and the game goes on into rEnd without input. Every record is compared with the runner's trace by
# tools/playcmp.py (the door path into the ending without HOST_XEND). src/game is taken at GAME_REV (default HEAD)
# with PIN_MAX 1792; EXACT=0: the shipping build's collision grid (default EXACT=1: the runner's R-tree order).
#   scripts/olmec_host.sh            -> RESULT / ROUTE lines (playcmp)
# The trace (build/trace/end_olmec_s1, 420 MB): TRACE_HUD=1 TRACE_SND=1 TRACE_LEVEL=16 TRACE_ROOM=rOlmec
#   TRACE_TAIL=400 XVFB_SCREEN=1280x960x24 scripts/hd_trace.sh end_olmec 1 end_olmec_s1
set -e
cd "$(dirname "$0")/.."
O=build/game/olmec; G=$O/g
rm -rf "$G"; mkdir -p "$G"
git archive "${GAME_REV:-HEAD}" src/game | tar -x -C "$G" --strip-components=2
cp build/gen/objects.[ch] build/gen/gentables.[ch] build/gen/playtables.[ch] "$G/"
sed -i '' "s/^#define PIN_MAX [0-9][0-9]*\$/#define PIN_MAX ${PIN:-1792}/" "$G/play.h"
if [ "${EXACT:-1}" = 1 ]; then { echo "#define PCOL_EXACT 1"; cat "$G/pcol.c"; } > "$G/pcol.tmp" && mv "$G/pcol.tmp" "$G/pcol.c"; fi
SDK=../cps3-testgame/sdk/include
cc -std=gnu99 -O2 -ffp-contract=off -w -DDRAW_HOST -DSND_LOG -Ibuild/snd -I$SDK -I$G -Ibuild/gen -Isrc/draw -Isrc/main -Isrc/hud \
  -Isrc/shell -Isrc/snd -Isrc/front -o $O/host tests/game/host.c src/main/game.c src/draw/draw.c src/hud/hud.c \
  build/gen/sprites.c src/front/front.c build/gen/fronttables.c src/snd/snd.c src/shell/hiscore.c build/gen/drawtab.c \
  $G/*.c -lm
HOST_DUMP=$O/c.txt HOST_ROOM=3 $O/host tests/routes/end_olmec.txt 1 16 0 1 400 build/gen - none nohud > /dev/null 2> $O/stderr.txt
python3 tools/playcmp.py build/trace/end_olmec_s1.bin build/trace/end_olmec_s1.names $O/c.txt --max 3 | tail -4
rm -f $O/c.txt
