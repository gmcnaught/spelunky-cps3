#!/bin/sh
# The ending (src/front: rEnd, rEnd2, rEnd3, rCredits2) on the host, every record against the runner's trace: the game
# program (tests/game/host.c, the src/game snapshot of scripts/game_check.sh, UNITY=0 or 1) enters rEnd at its first
# step as rOlmec's oXEnd does (HOST_ROOM 23: the route's "# room rEnd", src/main game.c), the play state at each record point
# (HOST_DUMP) compared by tools/playcmp.py; the sound calls (HOST_SND) by tools/sndcmp.py.
#   scripts/end_host.sh            -> RESULT / ROUTE lines (playcmp), then sndcmp's summary
# The trace (build/trace/g_end_win_s7, 80 MB): TRACE_ROOM=rEnd TRACE_MONEY=12345
#   TRACE_GLOBALS=kaliPunish=2,kills=7,damsels=2,time=754321 TRACE_HUD=1 TRACE_SND=1 XVFB_SCREEN=1280x960x24
#   scripts/hd_trace.sh end_win 7 g_end_win_s7
# sndcmp: the runner's audio_is_playing(xflame) (oLavaSpray Step :20) turns false after ~20 steps, the host's voices
# never end (no sound clock in tests/game/host.c): its replays are missing on the host side.
set -e
cd "$(dirname "$0")/.."
G=tests/game/build/g; O=build/game/end
[ -f "$G/stamp" ] || { echo "no src/game snapshot: run scripts/game_check.sh first"; exit 1; }
mkdir -p $O
SDK=../cps3-testgame/sdk/include
cc -std=gnu99 -O2 -ffp-contract=off -w -DDRAW_HOST -DSND_LOG -Ibuild/snd -I$SDK -I$G -Ibuild/gen -Isrc/draw -Isrc/main -Isrc/hud \
  -Isrc/shell -Isrc/snd -Isrc/front -o $O/host tests/game/host.c src/main/game.c src/draw/draw.c src/hud/hud.c \
  build/gen/sprites.c src/front/front.c build/gen/fronttables.c src/snd/snd.c src/shell/hiscore.c build/gen/drawtab.c \
  $G/*.c -lm
HOST_SND=$O/snd.txt HOST_DUMP=$O/c.txt HOST_ROOM=23 HOST_GLOBALS=kaliPunish=2,kills=7,damsels=2,time=754321 \
  $O/host tests/routes/end_win.txt 7 1 12345 0 30 build/gen - none nohud > /dev/null 2> $O/stderr.txt
python3 tools/playcmp.py build/trace/g_end_win_s7.bin build/trace/g_end_win_s7.names $O/c.txt --max 3 | tail -4
python3 tools/sndcmp.py build/trace/g_end_win_s7.bin $O/snd.txt | tail -1
rm -f $O/c.txt
