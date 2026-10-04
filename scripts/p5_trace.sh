#!/bin/sh
# P5 reference runs: scripts/p4_trace.sh with the enemies kept (no TRACE_NOENEMY), the 4:3 view, and the route's
# starting level and money from "# level N" / "# money M" lines in the route file (TRACE_LEVEL,
# TRACE_MONEY; default 1 and 0). Run names start p5_.
#   scripts/p5_trace.sh <route> <seed> [--no-run]   route: tests/routes/<route>.txt
#   -> build/trace/<route>_s<seed>.bin, build/p5/<route>_s<seed>.c.txt, tools/playcmp.py's result
#   PLAYHOST=<binary>: another host build (as scripts/p4_trace.sh)
set -eu
cd "$(dirname "$0")/.."
R=$1; S=$2; N=${R}_s$S
case "$R" in p5_*) ;; *) echo "p5_trace.sh: route names start p5_"; exit 2;; esac
L=$(sed -n 's/^# *level \([0-9][0-9]*\).*/\1/p' "tests/routes/$R.txt" | head -1); L=${L:-1}
M=$(sed -n 's/^# *money \([0-9][0-9]*\).*/\1/p' "tests/routes/$R.txt" | head -1); M=${M:-0}
if [ "${3:-}" != --no-run ]; then
  TRACE_LEVEL=$L TRACE_MONEY=$M XVFB_SCREEN=1280x960x24 scripts/hd_trace.sh "$R" "$S" "$N" | tail -1
fi
make -s -C test/host build/host/playhost >/dev/null 2>&1 || make -s -C test/host >/dev/null
mkdir -p build/p5
${PLAYHOST:-build/host/playhost} "tests/routes/$R.txt" "$S" --enemies --level "$L" --money "$M" > "build/p5/$N.c.txt" 2> "build/p5/$N.c.err" || true
python3 tools/playcmp.py "build/trace/$N.bin" "build/trace/$N.names" "build/p5/$N.c.txt" --max ${MAX:-2}
