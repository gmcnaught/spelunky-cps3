#!/bin/sh
# P4 reference runs: scripts/hd_trace.sh with the enemies removed (TRACE_NOENEMY=1) and the 4:3 view (320 x 240,
# XVFB_SCREEN=1280x960x24), then the host play loop and the comparison.
#   scripts/p4_trace.sh <route> <seed> [--no-run]   route: tests/routes/<route>.txt
#   -> build/trace/<route>_s<seed>.bin, build/p4/<route>_s<seed>.c.txt, tools/playcmp.py's result
set -eu
cd "$(dirname "$0")/.."
R=$1; S=$2; N=${R}_s$S
if [ "${3:-}" != --no-run ]; then
  TRACE_NOENEMY=1 XVFB_SCREEN=1280x960x24 scripts/hd_trace.sh "$R" "$S" "$N" | tail -1
fi
make -s -C test/host build/host/playhost >/dev/null 2>&1 || make -s -C test/host >/dev/null
mkdir -p build/p4
build/host/playhost "tests/routes/$R.txt" "$S" > "build/p4/$N.c.txt" 2> "build/p4/$N.c.err" || true
python3 tools/playcmp.py "build/trace/$N.bin" "build/trace/$N.names" "build/p4/$N.c.txt" --max ${MAX:-2}
