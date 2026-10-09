#!/bin/sh
# The double collision queries by call site (docs/AST-GREP.md): build/host/playhost_fcol (play.h FCOL_STATS, the grid
# build) on every playhost route of scripts/hostident.sh, one count file per run, then the table: per call site and
# query kind the calls that reached the function, those with a coordinate not whole, and the same on the routes named
# in HOT (default: the seven routes still dropping frames).
#   [P=3] [RUNTAG=<tag>] [HOT="route ..."] scripts/fcol_survey.sh [<rows>]      -> build/fcol<_tag>/table.txt
# Kinds: pt collision_point_p, ptany collision_point_any / CP / CPn, line collision_line_p, rect collision_rect_p,
# rany collision_rect_any, place instance_place_p / place_meeting_p, isrect penemy.c isCollisionRectangle, ovl
# overlap_at's float path (by the counted query it is under; "overlap_at(pass)": the collision pass)
cd "$(dirname "$0")/.."
W=build/fcol${RUNTAG:+_$RUNTAG}; rm -rf "$W"; mkdir -p "$W"
make -s -C test/host "$PWD/build/host/playhost_fcol" >/dev/null || exit 2
RUNS_ONLY=1 RUNTAG=fcol${RUNTAG:+_$RUNTAG} scripts/hostident.sh >/dev/null || exit 2
cp build/hostident_fcol${RUNTAG:+_$RUNTAG}/runs.txt "$W/runs.txt"
W="$W" xargs -P "${P:-3}" -L 1 sh -c 'n=$0; FCOL_OUT=$W/$n.cnt build/host/playhost_fcol "$@" >/dev/null 2>&1' < "$W/runs.txt"
HOT=${HOT:-"c_swamp_drain c_items_damselexpl c_swamp_vampkill c_swamp_grave c_jungle_firefrog p5_reg_l9s5 p5_reg_l14s16"} \
  python3 - "$W" "${1:-60}" <<'PY' | tee "$W/table.txt"
import glob, collections, os, sys
K = ['pt', 'ptany', 'line', 'rect', 'rany', 'place', 'isrect', 'ovl']
hot = tuple(os.environ['HOT'].split())
tot = collections.defaultdict(lambda: [0, 0, 0, 0])
for f in glob.glob(sys.argv[1] + '/*.cnt'):
    h = os.path.basename(f).rsplit('_s', 1)[0] in hot
    for l in open(f):
        a = l.split()
        v = tot[(a[0], int(a[1]), K[int(a[2])])]
        v[0] += int(a[3]); v[1] += int(a[4])
        if h: v[2] += int(a[3]); v[3] += int(a[4])
print('%-16s %5s %-6s %9s %8s %8s %8s' % ('file', 'line', 'kind', 'calls', 'notwhole', 'hot', 'hotnw'))
for k, v in sorted(tot.items(), key=lambda kv: (-kv[1][2], -kv[1][0]))[:int(sys.argv[2])]:
    print('%-16s %5d %-6s %9d %8d %8d %8d' % (k + tuple(v)))
print('total: %d calls, %d not whole, %d sites' % (sum(v[0] for v in tot.values()), sum(v[1] for v in tot.values()), len(tot)))
PY
