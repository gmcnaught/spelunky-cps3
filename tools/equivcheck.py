#!/usr/bin/env python3
"""Gameplay-equivalence measurement (docs/EQUIV.md): a variant playhost output against the exact build's output
(build/host playhost, bit-equal to the HD runner's trace on every gated route). Both built with -DPLAY_RNGLOG
(Q lines: the RNG state at each record).

    tools/equivcheck.py <exact.txt> <variant.txt> [--show N]

Prints one line:
    EQ recs=<n> rng=<first record whose RNG state differs|-> disc=<first discrete difference|-> vdisc=<the same
       over the instances within 16 px of the exact build's view|-> bag=<first record whose header or multiset of
       instances without ids (object, sprite, frame, depth, visible, alarms, x, y) differs|-> gbag=<the same without
       the particles of COSMETIC|-> pos=<first x/y
       difference|-> set=<first instance-set difference|-> maxd_pre=<largest |dx|,|dy| before disc>
       route=<PASS|FAIL:<why>> state=<PASS when gbag is - or not before rng, else FAIL> rooms=<room@t ...>
Discrete state as tools/playfx.py (header, instance set, object, sprite, floor(image_index), depth, visible,
alarms, integer variables). Route check: the same rooms in the same order with entry steps within 1, the same
deaths, the same damage sequence (life after each loss), and equal last money / bombs / ropes, over the records
both have (the variant may stop early only if the exact one does).
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
            cur = dict(h=[int(x) for x in f[1:12]], q=None, insts={})
            recs.append(cur)
        elif f[0] == 'Q':
            cur['q'] = (f[1], f[2])
        elif f[0] == 'I':
            vs = dict(kv.split('=') for kv in f[17:])
            cur['insts'][f[1]] = dict(obj=f[2], x=float(f[3]), y=float(f[4]), spr=f[5], img=int(float(f[6]) // 1),
                                      depth=f[11], vis=f[12], al=f[13], vars=vs)
    return recs


def route(recs):
    # h: rec phase t room level life bombs rope money xview yview
    rooms, deaths, dmg, prev_room, prev_life = [], 0, [], None, None
    for r in recs:
        _, _, t, room, _, life, bombs, rope, money = r['h'][:9]
        if room != prev_room:
            rooms.append((room, t))
            prev_room = room
        if prev_life is not None and life > -1e8 and life < prev_life:
            dmg.append(life)
            if prev_life >= 1 > life:
                deaths += 1
        if life > -1e8:
            prev_life = life
    last = recs[-1]['h'] if recs else [0] * 11
    return dict(rooms=rooms, deaths=deaths, dmg=dmg, money=last[8], bombs=last[6], ropes=last[7])


# particles with no effect on play (no damage, no RNG draws after their Create; their Step only moves them and destroys
# them on terrain / water / lava / leaving the view): oRubblePiece's family (oRubble, oRubbleSmall, oRubbleDarkSmall,
# oLeaf, oDrip, oLavaDrip), oRubbleDark, and oDetritus's oBlood, oFlame, oBone (not oMagma)
COSMETIC = {'oRubblePiece', 'oRubble', 'oRubbleSmall', 'oRubbleDarkSmall', 'oLeaf', 'oDrip', 'oLavaDrip', 'oRubbleDark',
            'oBlood', 'oFlame', 'oBone'}


def bagof(r, skip=()):
    """the record's instances without their ids: the multiset of (object, sprite, frame, depth, visible, alarms, x, y
    to 1/16 px, the drawing grid)"""
    return sorted((v['obj'], v['spr'], v['img'], v['depth'], v['vis'], v['al'], round(v['x'] * 16), round(v['y'] * 16))
                  for v in r['insts'].values() if v['obj'] not in skip)


def in_view(h, v):
    xv, yv = h[9], h[10]
    return xv - 16 <= v['x'] <= xv + 336 and yv - 16 <= v['y'] <= yv + 256


def disc_diffs(ra, rb, view=False):
    """discrete differences; view: only the instances within 16 px of the exact build's view (either build)"""
    d = []
    ia, ib = ra['insts'], rb['insts']
    if view:
        ia2 = {i: v for i, v in ia.items() if in_view(ra['h'], v)}
        ib2 = {i: v for i, v in ib.items() if in_view(ra['h'], v)}
        keep = set(ia2) | set(ib2)
        ia = {i: v for i, v in ia.items() if i in keep}
        ib = {i: v for i, v in ib.items() if i in keep}
    if ra['h'] != rb['h']:
        d.append(f'header {ra["h"]} / {rb["h"]}')
    if set(ia) != set(ib):
        x = sorted(set(ia) ^ set(ib))[:6]
        d.append('set ' + ' '.join(f'{i}:{(ia.get(i) or ib.get(i))["obj"]}' for i in x))
    for i in set(ia) & set(ib):
        x, y = ia[i], ib[i]
        for f in ('obj', 'spr', 'img', 'depth', 'vis', 'al'):
            if x[f] != y[f]:
                d.append(f'{i} {x["obj"]} {f} {x[f]} / {y[f]}')
        for v in x['vars']:
            if v not in FRAC and v in y['vars'] and x['vars'][v] != y['vars'][v]:
                d.append(f'{i} {x["obj"]} {v} {x["vars"][v]} / {y["vars"][v]}')
    return d


def main():
    a, b = load(sys.argv[1]), load(sys.argv[2])
    show = int(sys.argv[sys.argv.index('--show') + 1]) if '--show' in sys.argv else 0
    n = min(len(a), len(b))
    rng = disc = posf = setf = vdisc = bag = gbag = None
    maxd = 0.0
    for k in range(n):
        ra, rb = a[k], b[k]
        if bag is None and (ra['h'] != rb['h'] or bagof(ra) != bagof(rb)):
            bag = k
        if gbag is None and (ra['h'] != rb['h'] or bagof(ra, COSMETIC) != bagof(rb, COSMETIC)):
            gbag = k
        if vdisc is None:
            dv = disc_diffs(ra, rb, True)
            if dv:
                vdisc = k
                if show:
                    print(f'view disc rec {k}:')
                    for t in dv[:show]:
                        print('   ', t)
        if rng is None and ra['q'] != rb['q']:
            rng = k
        if setf is None and set(ra['insts']) != set(rb['insts']):
            setf = k
        if disc is None:
            dd = disc_diffs(ra, rb)
            if dd:
                disc = k
                if show:
                    print(f'disc rec {k} (t {ra["h"][2]}):')
                    for t in dd[:show]:
                        print('   ', t)
        for i in set(ra['insts']) & set(rb['insts']):
            x, y = ra['insts'][i], rb['insts'][i]
            d = max(abs(x['x'] - y['x']), abs(x['y'] - y['y']))
            if d and posf is None:
                posf = k
                if show:
                    print(f'pos rec {k}: {i} {x["obj"]} {x["x"]},{x["y"]} / {y["x"]},{y["y"]}')
            if disc is None:
                maxd = max(maxd, d)
    ref, var = route(a[:n]), route(b[:n])
    why = []
    if [r for r, _ in ref['rooms']] != [r for r, _ in var['rooms']]:
        why.append('rooms')
    elif any(abs(t1 - t2) > 1 for (_, t1), (_, t2) in zip(ref['rooms'], var['rooms'])):
        why.append('entry')
    for k in ('deaths', 'dmg', 'money', 'bombs', 'ropes'):
        if ref[k] != var[k]:
            why.append(k)
    if len(b) < len(a):
        why.append(f'short{len(b)}/{len(a)}')
    f = lambda v: '-' if v is None else str(v)
    rs = ','.join(f'{r}@{t1}' + ('' if t1 == t2 else f'/{t2}') for (r, t1), (_, t2) in zip(ref['rooms'], var['rooms']))
    print(f'EQ recs={n} rng={f(rng)} disc={f(disc)} vdisc={f(vdisc)} bag={f(bag)} gbag={f(gbag)} pos={f(posf)} set={f(setf)} maxd_pre={maxd:g} '
          f'route={"PASS" if not why else "FAIL:" + "+".join(why)} '
          f'state={"PASS" if gbag is None or (rng is not None and gbag >= rng) else "FAIL"} rooms={rs}')


if __name__ == '__main__':
    main()
