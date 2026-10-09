#!/bin/sh
# Host identity (the per-commit check of docs/LUSH.md / PERF3.md, "182 host route runs byte-identical"): the md5 of
# playhost's whole output (exact build, build/host/playhost) and of playhost_grid's (the shipping collision grid) on
# every route playhost runs (tests/routes/*.txt but p8_* and end_win, front rooms: the P4 / P1 routes and seeds of scripts/equiv_check.sh,
# enemies removed; every other route with its header's seed / level / money / globals / room / nodark, enemies kept).
# Needs no trace. Save a baseline before a change, check after it:
#   scripts/hostident.sh save [file]      default build/hostident_<HEAD>.txt
#   scripts/hostident.sh check <file>     -> one line per differing run, then HOSTIDENT <equal>/<runs>; exit 1 on any
#   P=<jobs> (default 8): runs in parallel. RUNTAG=<tag>: the work files in build/hostident_<tag>.
#   RUNS_ONLY=1: write the run list (<work dir>/runs.txt) and stop.
cd "$(dirname "$0")/.."
M=${1:-save}; F=${2:-build/hostident_$(git rev-parse --short HEAD).txt}
make -s -C test/host "$PWD/build/host/playhost" "$PWD/build/host/playhost_grid" >/dev/null || exit 2
W=build/hostident${RUNTAG:+_$RUNTAG}; rm -rf "$W"; mkdir -p "$W"
gv() { sed -n "s/^# *$1 \([^ ]*\).*/\1/p" "$2" | head -1; }
{
  for rs in "p1_walk 1" "p4_exit559 559" "p4_hang_ladder 1" "p4_items 1" "p4_spikes 58" "p4_push_rope 365" \
            "p4_bomb_drop 1" "p4_bomb_throw 7" "p4_bomb_throw 3" "p4_darkexit 559"; do
    echo "${rs% *}_s${rs#* } tests/routes/${rs% *}.txt ${rs#* }"
  done
  for f in tests/routes/*.txt; do
    r=$(basename "$f" .txt)
    case "$r" in p1_*|p4_*|p8_*|end_win) continue ;; esac   # p8_*, end_win: front rooms (tests/game host)
    s=$(gv seed "$f"); L=$(gv level "$f"); Mo=$(gv money "$f"); G=$(gv globals "$f"); RM=$(gv room "$f"); ND=$(gv nodark "$f")
    echo "${r}_s${s:-1} $f ${s:-1} --enemies --level ${L:-1} --money ${Mo:-0} ${G:+--global $G} ${RM:+--room $RM} ${ND:+--nodark $ND}"
  done
} | sed 's/ *$//' > "$W/runs.txt"     # (xargs -L joins a line ending in a blank with the next)
[ -n "${RUNS_ONLY:-}" ] && exit 0                           # (scripts/fcol_survey.sh: the run list alone)
# one line per run: <route>_s<seed> <build> <md5 of its stdout>
t0=$(date +%s)
xargs -P "${P:-8}" -L 1 sh -c 'n=$0; for b in playhost playhost_grid; do echo "$n $b $(build/host/$b "$@" 2>/dev/null | md5)"; done' \
  < "$W/runs.txt" | sort > "$W/now.txt"
echo "$(wc -l < "$W/now.txt" | tr -d ' ') runs in $(( $(date +%s) - t0 )) s" >&2
case "$M" in
save)  cp "$W/now.txt" "$F"; echo "saved $F" ;;
check) [ -f "$F" ] || { echo "no baseline $F"; exit 2; }
       awk '{print $1"/"$2, $3}' "$F" | sort > "$W/a.txt"; awk '{print $1"/"$2, $3}' "$W/now.txt" | sort > "$W/b.txt"
       join -a 1 -a 2 -e - -o 0,1.2,2.2 "$W/a.txt" "$W/b.txt" \
         | awk '$2 != $3 { print "DIFF", $1; d++ } { n++ } END { printf "HOSTIDENT %d/%d equal\n", n - d, n; exit d > 0 }' ;;
*)     echo "usage: scripts/hostident.sh save|check [file]"; exit 2 ;;
esac
