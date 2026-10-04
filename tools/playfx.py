#!/usr/bin/env python3
"""Compare two playhost outputs (the binary64 build and the fixed-point build, -DPLAY_FIXED): does the
arithmetic change any step's discrete state or any position?

    tools/playfx.py <double.txt> <fixed.txt>

Discrete state: the record header, the instance set, object, sprite, the frame shown (floor(image_index)),
alarms, visible, depth and the integer variables (state, timers, flags). image_index's fraction is a fractional
variable (it differs from the first animated step in s7.24) and is not compared. Positions: x, y exactly. Not compared: the fractional variables
themselves (xVel, yVel, xAcc, yAcc, grav, gravityIntensity, myGrav, life of particles), whose representation is
the thing that differs. Prints the first difference per kind and a summary line:
    FX <records> discrete_first=<rec|-> position_first=<rec|-> max_dxy=<largest |dx|, |dy| seen>
"""
import sys

FRAC = {'xAcc', 'yAcc', 'grav', 'gravityIntensity', 'myGrav', 'life'}


def load(path):
    recs, cur = [], None
    for line in open(path):
        f = line.split()
        if not f:
            continue
        if f[0] == 'R':
            cur = dict(h=f[1:13] + f[14:15] * 0, insts={})
            recs.append(cur)
        elif f[0] == 'I':
            vs = dict(kv.split('=') for kv in f[17:])
            cur['insts'][f[1]] = dict(obj=f[2], x=float(f[3]), y=float(f[4]), spr=f[5], img=int(float(f[6]) // 1), depth=f[11],
                                     vis=f[12], al=f[13], vars=vs)
    return recs


def main():
    a, b = load(sys.argv[1]), load(sys.argv[2])
    dfirst = pfirst = None
    maxd = 0.0
    shown = 0
    for k, (ra, rb) in enumerate(zip(a, b)):
        disc, pos = [], []
        if ra['h'][:11] != rb['h'][:11]:
            disc.append(f'header {ra["h"][:11]} / {rb["h"][:11]}')
        if set(ra['insts']) != set(rb['insts']):
            disc.append(f'instances differ: {sorted(set(ra["insts"]) ^ set(rb["insts"]))[:6]}')
        for i in set(ra['insts']) & set(rb['insts']):
            x, y = ra['insts'][i], rb['insts'][i]
            for f in ('obj', 'spr', 'img', 'depth', 'vis', 'al'):
                if x[f] != y[f]:
                    disc.append(f'{i} {x["obj"]} {f} {x[f]} / {y[f]}')
            for v in x['vars']:
                if v not in FRAC and v in y['vars'] and x['vars'][v] != y['vars'][v]:
                    disc.append(f'{i} {x["obj"]} {v} {x["vars"][v]} / {y["vars"][v]}')
            d = max(abs(x['x'] - y['x']), abs(x['y'] - y['y']))
            maxd = max(maxd, d)
            if d:
                pos.append(f'{i} {x["obj"]} x,y {x["x"]},{x["y"]} / {y["x"]},{y["y"]}')
        if disc and dfirst is None:
            dfirst = k
        if pos and pfirst is None:
            pfirst = k
        if (disc or pos) and shown < 2:
            shown += 1
            print(f'rec {k}:')
            for t in (disc + pos)[:12]:
                print('   ', t)
    print(f'FX {min(len(a), len(b))} discrete_first={dfirst if dfirst is not None else "-"} '
          f'position_first={pfirst if pfirst is not None else "-"} max_dxy={maxd:g}')


if __name__ == '__main__':
    main()
