#!/bin/sh
# P7 content reference traces (docs/CONTENT.md): every tests/routes/c_<package>_<what>.txt run in the HD runner with
# the enemies kept, TRACE_HUD=1 TRACE_SND=1, the route's "# seed / level / money / globals / room / nodark" lines
# (TRACE_LEVEL, TRACE_MONEY, TRACE_GLOBALS, TRACE_ROOM, TRACE_NODARK: 0 lets the level be dark) -> build/trace/c_<package>_<what>_s<seed>.
#   scripts/content_traces.sh [route ...]     (default: every c_*.txt)
cd "$(dirname "$0")/.."
for f in tests/routes/c_*.txt; do
  r=$(basename "$f" .txt)
  [ $# -gt 0 ] && ! echo " $* " | grep -q " $r " && continue
  g() { sed -n "s/^# *$1 \([^ ]*\).*/\1/p" "$f" | head -1; }
  S=$(g seed); L=$(g level); M=$(g money); G=$(g globals); RM=$(g room); ND=$(g nodark)
  TRACE_HUD=1 TRACE_SND=1 TRACE_LEVEL=${L:-1} TRACE_MONEY=${M:-0} TRACE_GLOBALS=$G TRACE_ROOM=${RM:-rLevel} TRACE_NODARK=$ND XVFB_SCREEN=1280x960x24 \
    scripts/hd_trace.sh "$r" "${S:-1}" "${r}_s${S:-1}" | tail -1
done
