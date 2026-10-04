#!/bin/sh
# The sound-call reference traces (tools/sndcmp.py gate for src/game's sound calls): every P4 route (enemies removed)
# and P5 route (tests/playsh2/mkjobs.py's 18) plus p4_darkexit, recorded with TRACE_SND=1 as build/trace/s_<route>_s<seed>
# (the existing traces stay). scripts/snd_check.sh compares the host play loop's SND lines against them.
#   scripts/snd_traces.sh [route ...]     (default: all 19)
cd "$(dirname "$0")/.."
P4="p4_exit559:559 p4_hang_ladder:1 p4_items:1 p4_spikes:58 p4_push_rope:365 p1_walk:1 p4_bomb_drop:1 p4_bomb_throw:7 p4_darkexit:559"
all="$P4"
for f in tests/routes/p5_*.txt; do
  r=$(basename "$f" .txt); s=$(sed -n 's/^# *seed \([0-9][0-9]*\).*/\1/p' "$f" | head -1); all="$all $r:${s:-1}"
done
for rs in $all; do
  r=${rs%:*}; s=${rs#*:}
  [ $# -gt 0 ] && ! echo " $* " | grep -q " $r " && continue
  case "$r" in
  p5_*) L=$(sed -n 's/^# *level \([0-9][0-9]*\).*/\1/p' "tests/routes/$r.txt" | head -1)
        M=$(sed -n 's/^# *money \([0-9][0-9]*\).*/\1/p' "tests/routes/$r.txt" | head -1)
        TRACE_SND=1 TRACE_LEVEL=${L:-1} TRACE_MONEY=${M:-0} XVFB_SCREEN=1280x960x24 \
          scripts/hd_trace.sh "$r" "$s" "s_${r}_s$s" | tail -1 ;;
  *)    TRACE_SND=1 TRACE_NOENEMY=1 XVFB_SCREEN=1280x960x24 scripts/hd_trace.sh "$r" "$s" "s_${r}_s$s" | tail -1 ;;
  esac
done
