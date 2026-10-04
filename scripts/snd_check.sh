#!/bin/sh
# src/game's sound calls against the runner's (tools/sndcmp.py) on the TRACE_SND traces of scripts/snd_traces.sh
# (build/trace/s_<route>_s<seed>), the play state too (tools/playcmp.py: the sound calls must not change it).
#   scripts/snd_check.sh [route ...]      PLAYHOST=<binary> as scripts/p4_trace.sh; MAX=N differences shown
cd "$(dirname "$0")/.."
make -s -C test/host >/dev/null || exit 1
mkdir -p build/snd_check
for t in build/trace/s_*_s*.bin; do
  n=$(basename "$t" .bin); rs=${n#s_}; r=${rs%_s*}; s=${rs##*_s}
  [ $# -gt 0 ] && ! echo " $* " | grep -q " $r " && continue
  case "$r" in
  p5_*) L=$(sed -n 's/^# *level \([0-9][0-9]*\).*/\1/p' "tests/routes/$r.txt" | head -1)
        M=$(sed -n 's/^# *money \([0-9][0-9]*\).*/\1/p' "tests/routes/$r.txt" | head -1)
        A="--enemies --level ${L:-1} --money ${M:-0}" ;;
  *)    A= ;;
  esac
  ${PLAYHOST:-build/host/playhost} "tests/routes/$r.txt" "$s" $A > "build/snd_check/$n.c.txt" 2>/dev/null
  st=$(python3 tools/playcmp.py "$t" "build/trace/$n.names" "build/snd_check/$n.c.txt" --max 1 2>&1 | grep -E '^ROUTE (equal|DIFF)' | head -1)
  sn=$(python3 tools/sndcmp.py "$t" "build/snd_check/$n.c.txt" --max ${MAX:-3})
  printf '%-20s %s | sound: %s\n' "$r" "$st" "$(echo "$sn" | tail -1)"
  [ -n "$VERBOSE" ] && echo "$sn" | sed '$d'
done
rm -rf build/snd_check
