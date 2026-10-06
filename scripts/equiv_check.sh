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
# build/trace. Outputs in build/equiv, deleted at the end (KEEP=1 keeps them).
#   scripts/equiv_check.sh [route-regex]
# One line per route, then: EQUIV <pass>/<checked> (skipped <n>), and exit 1 on any failure.
cd "$(dirname "$0")/.."
F=${1:-.}
make -s -C test/host "$PWD/build/host/playhost_eq" "$PWD/build/host/playhost_grid" >/dev/null || exit 2
O=build/equiv; mkdir -p $O
L=$O/list.txt; : > $L
for rs in "p1_walk 1" "p4_exit559 559" "p4_hang_ladder 1" "p4_items 1" "p4_spikes 58" "p4_push_rope 365" \
          "p4_bomb_drop 1" "p4_bomb_throw 7" "p4_bomb_throw 3" "p4_darkexit 559"; do echo "$rs P4" >> $L; done
for f in tests/routes/p5_*.txt; do
  r=$(basename $f .txt); s=$(sed -n 's/^# *seed \([0-9][0-9]*\).*/\1/p' $f | head -1); echo "$r ${s:-1} P5" >> $L
done
for f in tests/routes/c_*.txt; do
  r=$(basename $f .txt); s=$(sed -n 's/^# *seed \([0-9][0-9]*\).*/\1/p' $f | head -1); echo "$r ${s:-1} C" >> $L
done
gv() { sed -n "s/^# *$1 \([^ ]*\).*/\1/p" $2 | head -1; }
n=0; ok=0; skip=0; bad=0
while read r s k; do
  echo "$r" | grep -qE "$F" || continue
  N=${r}_s$s; f=tests/routes/$r.txt
  [ -f build/trace/$N.bin ] || continue
  if [ $k = P4 ]; then A="$f $s"
  else
    Lv=$(gv level $f); M=$(gv money $f); G=$(gv globals $f); RM=$(gv room $f); ND=$(gv nodark $f)
    A="$f $s --enemies --level ${Lv:-1} --money ${M:-0} ${G:+--global $G} ${RM:+--room $RM} ${ND:+--nodark $ND}"
  fi
  build/host/playhost_eq $A > $O/$N.exact.txt 2>/dev/null
  ex=$(python3 tools/playcmp.py build/trace/$N.bin build/trace/$N.names $O/$N.exact.txt --quiet 2>&1 | grep '^RESULT' | awk '{print $2}')
  if [ "${ex%/*}" != "${ex#*/}" ]; then
    if [ $k = C ]; then printf '%-24s skipped: exact %s\n' $N "$ex"; skip=$((skip + 1)); rm -f $O/$N.*; continue; fi
    printf '%-24s FAIL: exact build %s against the trace\n' $N "$ex"; n=$((n + 1)); bad=$((bad + 1)); continue
  fi
  build/host/playhost_grid $A > $O/$N.grid.txt 2>/dev/null
  res=$(python3 tools/equivcheck.py $O/$N.exact.txt $O/$N.grid.txt)
  n=$((n + 1))
  gb=$(echo "$res" | sed -n 's/.* gbag=\([^ ]*\) .*/\1/p')
  acc=$(awk -v n=$N -v g="$gb" '$1 == n && $2 == g' tests/equiv_accept.txt)
  if echo "$res" | grep -q 'route=PASS' && echo "$res" | grep -q 'state=PASS'; then ok=$((ok + 1)); v=pass
  elif echo "$res" | grep -q 'route=PASS' && [ -n "$acc" ]; then ok=$((ok + 1)); v=accepted
  elif echo "$acc" | awk '$3 == "gameplay:" { f = 1 } END { exit !f }'; then ok=$((ok + 1)); v=known
  else bad=$((bad + 1)); v=FAIL; fi
  printf '%-24s %s exact %s | %s\n' $N $v "$ex" "$(echo "$res" | sed 's/ rooms=.*//')"
  [ -n "$KEEP" ] || rm -f $O/$N.*
done < $L
[ -n "$KEEP" ] || rm -rf $O
echo "EQUIV $ok/$n (skipped $skip)"
[ $bad = 0 ]
