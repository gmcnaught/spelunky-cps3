#!/bin/sh
# Every tests/routes/c_*.txt against its HD trace (build/trace/c_<route>_s<seed>): playhost (PH=<binary>, default the
# exact build/host/playhost) with the route's seed / level / money / globals / room / nodark, tools/playcmp.py's
# RESULT and ROUTE lines, one line per route (the main checkout's build/ctall.sh, made a tracked script).
#   scripts/ctall.sh          -> "<route>: RESULT n/n ... ROUTE equal ...", then CTALL <equal>/<routes>
#   P=<jobs> (default 8): routes in parallel. RUNTAG=<tag>: the work files in build/c_<tag> (default build/c).
cd "$(dirname "$0")/.."
make -s -C test/host "$PWD/build/host/playhost" >/dev/null || exit 2
W=build/c${RUNTAG:+_$RUNTAG}; mkdir -p "$W"
export W PH
ls tests/routes/c_*.txt | xargs -P "${P:-8}" -n 1 sh -c '
  f=$0; r=$(basename $f .txt)
  g() { sed -n "s/^# *$1 \([^ ]*\).*/\1/p" "$f" | head -1; }
  S=$(g seed); L=$(g level); M=$(g money); G=$(g globals); RM=$(g room); ND=$(g nodark)
  [ -f build/trace/${r}_s${S:-1}.bin ] || { echo "$r: no trace"; exit 0; }
  set -- --enemies --level ${L:-1} --money ${M:-0}
  [ -n "$G" ] && set -- "$@" --global "$G"; [ -n "$RM" ] && set -- "$@" --room "$RM"; [ -n "$ND" ] && set -- "$@" --nodark "$ND"
  ${PH:-build/host/playhost} $f ${S:-1} "$@" > $W/$r.c.txt 2> $W/$r.c.err
  echo "$r: $(python3 tools/playcmp.py build/trace/${r}_s${S:-1}.bin build/trace/${r}_s${S:-1}.names $W/$r.c.txt --max 1 2>&1 | grep -E "^RESULT|^ROUTE (equal|DIFF)" | tr "\n" " ")"
  rm -f $W/$r.c.txt $W/$r.c.err' | sort > "$W/ctall.txt"
cat "$W/ctall.txt"
echo "CTALL $(grep -c 'ROUTE equal' "$W/ctall.txt")/$(wc -l < "$W/ctall.txt" | tr -d ' ')"
rm -rf "$W"
