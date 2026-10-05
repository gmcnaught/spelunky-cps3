#!/bin/sh
# tools/olmecbot.c built against src/game (the exact build, as test/host's playhost) and run: a route that beats Olmec.
#   scripts/olmecbot.sh [seed] [out route] [options]     default: 1 tests/routes/end_olmec.txt --depth 2 --dstep 16
# Progress on stderr (Olmec's cycles, the map when he drowns, the search to the door); about 3 minutes on 8 cores.
set -e
cd "$(dirname "$0")/.."
G=build/gen; P=src/game
S=${1:-1}; OUT=${2:-tests/routes/end_olmec.txt}
[ $# -gt 2 ] && shift 2 || set -- --depth 2 --dstep 16
cc -std=gnu99 -O2 -w -I$P -Isrc/front -Isrc/snd -I$G -I../cps3-testgame/sdk/include -Ibuild/snd -DSND_LOG -DPCOL_EXACT \
  -o build/olmecbot tools/olmecbot.c $P/rng.c $P/inst.c $P/gen.c $P/genroom.c $P/genobj.c $P/genent.c $G/objects.c \
  $G/gentables.c $P/pworld.c $P/pscript.c $P/pobj.c $P/pplayer.c $P/prun.c $P/pcol.c $P/ptrans.c $G/playtables.c \
  $P/penemy.c $P/pdamsel.c $P/pshop.c $P/pmath.c $P/pitem.c $P/pmsg.c $P/pcontent.c $P/pk_jungle.c $P/pk_swamp.c \
  $P/pk_ice.c $P/pk_temple.c $P/pk_items.c src/snd/snd.c test/host/sndhost.c -lm
build/olmecbot "$S" "$OUT" "$@"
