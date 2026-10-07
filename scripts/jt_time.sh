#!/bin/sh
# A jtcps3 timing run of the set scripts/playsh2_jt.sh built (tests/playsh2/build/jt/elf$JTV/mister: the newest MRA
# there and its zip), start to results without watching: install it on the MiSTer, load it, take a screenshot every
# POLL s and read it with tools/jtresult.py until the results screen shows, copy that screenshot back, return the
# MiSTer to the menu (also on failure or timeout), print the table with the jobs' names.
#   scripts/jt_time.sh            MISTER=root@host (default root@192.168.20.62: ask the lead first)
#   JTV=<variant>, RUNTAG=<tag> (as playsh2_jt.sh), POLL=30 s, FIRST=20 s (the first look), MAXWAIT=2400 s, MRA_DIR=_CPS3Test
#   -> build/jt_time/<set>_<date>/{results.png,results.txt}; exit 0 PASS, 1 FAIL / not read, 2 timeout
# e.g. JTFAST=1 JT_NOGEN=1 JT_ROUTES=p4_exit559,p5_caveman scripts/playsh2_jt.sh && scripts/jt_time.sh
# (the set is linked as playsh2_jt.sh built it: NC=nc_robust.txt by default since 2026-10-07, NC=none for the plain link)
set -e
cd "$(dirname "$0")/.."
DEV=${MISTER:-root@192.168.20.62}; MD=${MRA_DIR:-_CPS3Test}
J=tests/playsh2/build${RUNTAG:+/t_$RUNTAG}/jt; M=$J/elf${JTV:-}/mister
MRA=$(ls -t "$M"/*.mra 2>/dev/null | head -1)
[ -n "$MRA" ] || { echo "no MRA in $M: run scripts/playsh2_jt.sh first"; exit 1; }
SET=$(sed -n 's/.*<setname>\([^<]*\)<.*/\1/p' "$MRA" | head -1)
TITLE=$(basename "$MRA" .mra)
O=build/jt_time/${SET}_$(date +%Y%m%d_%H%M%S); mkdir -p "$O"
menu() { ssh "$DEV" 'timeout 20 sh -c "echo load_core /media/fat/menu.rbf > /dev/MiSTer_cmd"' || true; }
trap menu EXIT
ssh "$DEV" "mkdir -p /media/fat/_Arcade/$MD; rm -rf /media/fat/screenshots/$SET"
scp -q "$M/$SET.zip" "$DEV:/media/fat/games/mame/"
scp -q "$MRA" "$DEV:/media/fat/_Arcade/$MD/"
start=$(date +%s)
ssh "$DEV" "timeout 20 sh -c \"echo 'load_core /media/fat/_Arcade/$MD/$TITLE.mra' > /dev/MiSTer_cmd\""
echo "$TITLE: loaded at $(date +%T); polling every ${POLL:-30} s"
sleep "${FIRST:-20}"
rc=2
while [ $(( $(date +%s) - start )) -lt "${MAXWAIT:-2400}" ]; do
  f=$(ssh "$DEV" "timeout 5 sh -c 'echo screenshot > /dev/MiSTer_cmd'; sleep 2; ls -t /media/fat/screenshots/$SET/ 2>/dev/null | head -1")
  if [ -n "$f" ]; then
    scp -q "$DEV:/media/fat/screenshots/$SET/$f" "$O/last.png"
    set +e; python3 tools/jtresult.py "$O/last.png" > "$O/read.txt"; r=$?; set -e
    if [ $r = 0 ]; then mv "$O/last.png" "$O/results.png"; rc=0; break; fi
    echo "  $(( $(date +%s) - start )) s: $(head -1 "$O/read.txt")"
  fi
  sleep "${POLL:-30}"
done
wall=$(( $(date +%s) - start ))
menu; trap - EXIT
[ $rc = 2 ] && { echo "no results screen after $wall s (last: $O/last.png)"; exit 2; }
# the jobs' names in order (jobs.h: generation cases G<level>, routes by name)
JH=$J/elf${JTV:-}/jobs.h; [ -f "$JH" ] || JH=$J/jobs.h
python3 - "$JH" "$O/read.txt" "$wall" > "$O/results.txt" <<'PY'
import re, sys
h = open(sys.argv[1]).read()
names = re.findall(r'\{\s*"([^"]+)"', h)          # routes[] in job order
jobs = re.findall(r'\{\s*(\d+)u?,\s*(-?\d+),\s*(-?\d+)\s*\}', h[h.index('struct job jobs'):])   # jobs[]: seed, level, route
lines = open(sys.argv[2]).read().splitlines()
print('%s (results after %s s on jtcps3)' % (lines[0], sys.argv[3]))
rows = [l for l in lines[2:] if re.match(r'[RG]\d', l)]
print('%-4s %-3s %-18s %12s %10s %10s' % ('job', 'chk', 'name', 'total', 'step mean', 'step max'))
for k, l in enumerate(rows):
    f = l.split()
    seed, level, route = jobs[k] if k < len(jobs) else ('', '', '-1')
    nm = names[int(route)] if int(route) >= 0 else 'gen level %s' % level
    nums = [x for x in f[2:] if x.isdigit()]
    if f[0].startswith('R') and len(nums) >= 3:
        print('%-4s %-3s %-18s %12s %10s %10s' % (f[0], f[1], nm, nums[-3], nums[-2], nums[-1]))
    else:
        print('%-4s %-3s %-18s %12s' % (f[0], f[1], nm, nums[-1] if nums else ''))
PY
cat "$O/results.txt"; echo "$O"
head -1 "$O/read.txt" | grep -q '^PASS' || exit 1
