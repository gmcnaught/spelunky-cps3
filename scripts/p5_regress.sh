#!/bin/sh
# P5 regression: the P4 routes (enemies removed) and every P5 route (tests/routes/p5_*.txt, seed from its
# "# seed N" line) against their existing reference traces (--no-run). One RESULT / ROUTE line each.
cd "$(dirname "$0")/.."
for rs in "p4_exit559 559" "p4_hang_ladder 1" "p4_items 1" "p4_spikes 58" "p4_push_rope 365"; do
  r=${rs% *}; s=${rs#* }
  printf '%-22s ' "$r"; scripts/p4_trace.sh "$r" "$s" --no-run 2>&1 | grep -E '^RESULT' | tr '\n' ' '
  scripts/p4_trace.sh "$r" "$s" --no-run 2>&1 | grep -E '^ROUTE (equal|DIFF)'
done
for f in tests/routes/p5_*.txt; do
  r=$(basename "$f" .txt)
  s=$(sed -n 's/^# *seed \([0-9][0-9]*\).*/\1/p' "$f" | head -1); s=${s:-1}
  [ -f "build/trace/${r}_s$s.bin" ] || { printf '%-22s no reference trace\n' "$r"; continue; }
  printf '%-22s ' "$r"; out=$(scripts/p5_trace.sh "$r" "$s" --no-run 2>&1)
  echo "$out" | grep -E '^RESULT' | tr '\n' ' '; echo "$out" | grep -E '^ROUTE (equal|DIFF)'
done
