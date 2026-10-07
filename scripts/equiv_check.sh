#!/bin/sh
# The gameplay-equivalence gate (docs/EQUIV.md §4): the shipping build's collision grid (build/host/playhost_grid)
# against the exact build (build/host/playhost_eq, PCOL_EXACT) on every route with a reference trace:
#   - the exact build must be record-equal to the trace (tools/playcmp.py RESULT n/n). The P4 / P1 / bomb /
#     darkexit and P5 routes are required to be; a c_* route that is not (untranslated content) is skipped;
#   - the grid build must pass tools/equivcheck.py's route check and state gate against the exact build; a state-gate
#     difference listed in tests/equiv_accept.txt (route, its first gbag record, the reason) is accepted; a line whose
#     reason starts with "gameplay:" is a known gameplay difference the user accepted, accepted also when the route
#     check fails (docs/EQUIV.md §4).
# Works in any checkout (worktrees too): the binaries are built in this checkout's build/host, traces read from its
# build/trace. Outputs in build/equiv[_<RUNTAG>], deleted at the end (KEEP=1 keeps them).
#   scripts/equiv_check.sh [route-regex]
# One line per route, then: EQUIV <pass>/<checked> (skipped <n>), and exit 1 on any failure.
cd "$(dirname "$0")/.."
gv() { sed -n "s/^# *$1 \([^ ]*\).*/\1/p" $2 | head -1; }
# one route (the routes run P at a time, default 8, through this script with EQ_ONE=1; output in list order)
if [ "${EQ_ONE:-}" = 1 ]; then
  r=$1; s=$2; k=$3
  N=${r}_s$s; f=tests/routes/$r.txt
  [ -f build/trace/$N.bin ] || exit 0
  if [ $k = P4 ]; then A="$f $s"
  else
    Lv=$(gv level $f); M=$(gv money $f); G=$(gv globals $f); RM=$(gv room $f); ND=$(gv nodark $f)
    A="$f $s --enemies --level ${Lv:-1} --money ${M:-0} ${G:+--global $G} ${RM:+--room $RM} ${ND:+--nodark $ND}"
  fi
  build/host/playhost_eq $A > $O/$N.exact.txt 2>/dev/null
  ex=$(python3 tools/playcmp.py build/trace/$N.bin build/trace/$N.names $O/$N.exact.txt --quiet 2>&1 | grep '^RESULT' | awk '{print $2}')
  if [ "${ex%/*}" != "${ex#*/}" ]; then
    if [ $k = C ]; then printf '%-24s skipped: exact %s\n' $N "$ex"; rm -f $O/$N.*; exit 0; fi
    printf '%-24s FAIL: exact build %s against the trace\n' $N "$ex"; exit 0
  fi
  build/host/playhost_grid $A > $O/$N.grid.txt 2>/dev/null
  res=$(python3 tools/equivcheck.py $O/$N.exact.txt $O/$N.grid.txt)
  gb=$(echo "$res" | sed -n 's/.* gbag=\([^ ]*\) .*/\1/p')
  acc=$(awk -v n=$N -v g="$gb" '$1 == n && $2 == g' tests/equiv_accept.txt)
  if echo "$res" | grep -q 'route=PASS' && echo "$res" | grep -q 'state=PASS'; then v=pass
  elif echo "$res" | grep -q 'route=PASS' && [ -n "$acc" ]; then v=accepted
  elif echo "$acc" | awk '$3 == "gameplay:" { f = 1 } END { exit !f }'; then v=known
  else v=FAIL; fi
  printf '%-24s %s exact %s | %s\n' $N $v "$ex" "$(echo "$res" | sed 's/ rooms=.*//')"
  [ -n "$KEEP" ] || rm -f $O/$N.*
  exit 0
fi
F=${1:-.}
make -s -C test/host "$PWD/build/host/playhost_eq" "$PWD/build/host/playhost_grid" >/dev/null || exit 2
O=build/equiv${RUNTAG:+_$RUNTAG}; mkdir -p $O     # RUNTAG=<tag>: build/equiv_<tag> (two checks side by side)
L=$O/list.txt; : > $L
for rs in "p1_walk 1" "p4_exit559 559" "p4_hang_ladder 1" "p4_items 1" "p4_spikes 58" "p4_push_rope 365" \
          "p4_bomb_drop 1" "p4_bomb_throw 7" "p4_bomb_throw 3" "p4_darkexit 559"; do echo "$rs P4" >> $L; done
for f in tests/routes/p5_*.txt; do
  r=$(basename $f .txt); s=$(sed -n 's/^# *seed \([0-9][0-9]*\).*/\1/p' $f | head -1); echo "$r ${s:-1} P5" >> $L
done
for f in tests/routes/c_*.txt; do
  r=$(basename $f .txt); s=$(sed -n 's/^# *seed \([0-9][0-9]*\).*/\1/p' $f | head -1); echo "$r ${s:-1} C" >> $L
done
grep -n . $L | while IFS=: read i line; do set -- $line; echo "$1" | grep -qE "$F" && echo "$i $line"; done > $O/todo.txt
EQ_ONE=1 O=$O xargs -P "${P:-8}" -L 1 sh -c 'i=$0; sh scripts/equiv_check.sh "$@" | sed "s/^/$i /"' < $O/todo.txt \
  | sort -n | cut -d" " -f2- > $O/res.txt
cat $O/res.txt
ok=$(grep -cE "^[^ ]+ +(pass|accepted|known) " $O/res.txt); bad=$(grep -cE "^[^ ]+ +FAIL" $O/res.txt)
skip=$(grep -cE "^[^ ]+ +skipped:" $O/res.txt); n=$((ok + bad))
[ -n "$KEEP" ] || rm -rf $O
echo "EQUIV $ok/$n (skipped $skip)"
[ $bad = 0 ]
