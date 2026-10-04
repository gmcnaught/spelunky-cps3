#!/bin/sh
# Modelled jtcps3 cost of single play steps (docs/PERF3.md section 1, the model of docs/REVIEW-SH2.md section 3):
# tests/playsh2 built as scripts/playsh2_check.sh builds it (GAME_REV / EXACT / SOFTFP / OPT / PIN as there; SOFTFP
# defaults to 1 here, as the game), cut to the given routes' jobs; MAME (scripts/mame.sh: docker, headless; -nodrc
# -debug) traces each step with scripts/lua/jtcost.lua (registers and PR per instruction, play_step's entry to its
# return); tools/jtcost.py reduces each trace; the traces are deleted (HANDOFF disk rule).
#   [GAME_REV=<rev>|WORKTREE] [EXACT=1] [SOFTFP=0] [VARIANT=jtcost] [JTCOST_CONSTS=review|fit] \
#     scripts/jtcost.sh [<route> <step>]...          default: p4_exit559 301 p5_snakes 956 (PERF3's two steps)
# <step>: the run's record count after the traced step. The jobs are the routes named, in the order first named,
# and the count runs on over them (one record for each level start and step): with the default list p4_exit559's
# 806 records come first, so p5_snakes 956 is its 150th record. `scripts/jtcost.sh p5_snakes 150` traces the same
# step (same instructions and stores; the jobs table's size moves the data, so misses differ: model +0.3 %). Compare
# commits with the same arguments only.
# Output: build/<VARIANT>/out/jtcost_<route>_<step>.txt (tables) under tests/playsh2; the JTCOST summary lines
# (one per constant set: review = REVIEW-SH2's hand-set costs, fit = the jtmodel fit) on stdout.
set -e
cd "$(dirname "$0")/.."
[ $# -eq 0 ] && set -- p4_exit559 301 p5_snakes 956
[ $(($# % 2)) -eq 0 ] || { echo "usage: scripts/jtcost.sh [<route> <step>]..." >&2; exit 2; }
V=${VARIANT:-jtcost}; E=tests/playsh2/build/$V; O=$E/out; TD=$PWD/$E/trace
ROUTES=; PAIRS=; last=0
while [ $# -gt 0 ]; do
  case " $ROUTES " in *" $1 "*) ;; *) ROUTES="${ROUTES:+$ROUTES }$1" ;; esac
  [ "$2" -gt "$last" ] || { echo "jtcost.sh: steps must ascend ($2 after $last)" >&2; exit 2; }
  last=$2; PAIRS="$PAIRS $1:$2"; shift 2
done
t0=$(date +%s)
ROUTES="$ROUTES" BUILD_ONLY=1 VARIANT=$V SOFTFP=${SOFTFP:-1} scripts/playsh2_check.sh
STEP=$(awk '$3 == "_play_step" { print $1 }' "$O/nm.txt")
[ -n "$STEP" ] || { echo "jtcost.sh: no _play_step in $O/nm.txt" >&2; exit 1; }
rm -rf "$TD"; mkdir -p "$TD"; trap 'rm -rf "$TD"' EXIT INT TERM
WIN=; k=0
for p in $PAIRS; do k=$((k + 1)); WIN="${WIN:+$WIN,}$((${p#*:} - 1)):$TD/t$k.tr"; done
JTC_STEP=$STEP JTC_WIN=$WIN scripts/mame.sh sfiii3na -rompath "$E/mame" -skip_gameinfo -nothrottle -sound none \
  -video none -nodrc -debug -debugger none -seconds_to_run ${SECONDS_TO_RUN:-40000} -cfg_directory "$O/w/cfg" \
  -nvram_directory "$O/w/nvram" -snapshot_directory "$O/w/snap" -diff_directory "$O/w/diff" \
  -state_directory "$O/w/sta" -inipath "$O/w" -autoboot_script scripts/lua/jtcost.lua >"$O/mame.log" 2>&1 || true
k=0; S=
for p in $PAIRS; do
  k=$((k + 1)); r=${p%:*}; s=${p#*:}; f=$O/jtcost_${r}_$s.txt
  if [ ! -s "$TD/t$k.tr" ]; then echo "jtcost.sh: no trace for $r $s (see $O/mame.log)" >&2; exit 1; fi
  python3 tools/jtcost.py --route "$r" --step "$s" "$TD/t$k.tr" "$O/nm.txt" > "$f"
  rm -f "$TD/t$k.tr"
  S="$S$(grep '^JTCOST ' "$f")
"
done
printf '%s' "$S"
echo "jtcost.sh: $(($(date +%s) - t0)) s; tables in $O/jtcost_*.txt"
