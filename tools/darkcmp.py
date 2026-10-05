#!/usr/bin/env python3
"""The dark level's oLevel.darkness and oPlayer1.distToNearestLightSource of build/host/playhost (PLAYHOST_DARK=1: its
K lines) against a TRACE_HUD trace's level block, on the records where global.darkLevel is set; exact compare.
   python3 tools/darkcmp.py <trace name> <playhost args...>      (from the repo root; PH=<binary>)
   e.g. python3 tools/darkcmp.py g_p7_dark_s18 tests/routes/p7_dark.txt 18 --enemies --level 2 --nodark 0
Traces with dark records: g_p7_dark_s18, c_items_flare_s69, c_jungle_scarab_s615."""
import sys, subprocess, os
sys.path.insert(0, 'tools')
import tracer
tr = sys.argv[1]
env = dict(os.environ, PLAYHOST_DARK='1')
r = subprocess.run([os.environ.get('PH', 'build/host/playhost')] + sys.argv[2:], capture_output=True, text=True, env=env)
K = {}
for l in r.stderr.splitlines():
    if l.startswith('K '):
        f = l.split(); K[int(f[1])] = (int(f[2]), float(f[3]), float(f[4]))
n = bad = badl = 0; first = None
for hd, insts in tracer.records(open('build/trace/%s.bin' % tr, 'rb').read()):
    h = hd.get('hud') or {}
    if 'darkLevel' not in h or hd['rec'] not in K: continue
    n += 1
    dl, dk, dist = K[hd['rec']]
    if h['darkLevel'] > 0 and dk != h['darkness']:
        bad += 1
        if first is None: first = (hd['rec'], dk, h['darkness'], dist, h.get('distLight'))
    if h['darkLevel'] > 0 and dist != h.get('distLight'):
        badl += 1
        if badl <= 4: print('distLight', hd['rec'], dist, h.get('distLight'), dk)
print(tr, 'records', n, 'darkness differ', bad, 'distLight differ', badl, 'first', first)
