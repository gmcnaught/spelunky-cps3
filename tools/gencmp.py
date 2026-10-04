#!/usr/bin/env python3
"""Compare generated levels: the HD runner's generator dumps (scripts/hd_trace.sh --gen) with the C generator's
(build/host/genhost, test/host/genhost.c).

    tools/gencmp.py <ref.gen> <ref.names> <c.txt> [--max N] [--show K]
    tools/gencmp.py --dump <ref.gen> <ref.names> [--case K]

Per case: the level's instances in creation order (the runner's `with (all)` list reversed, persistent instances
left out), room instances by their room ids (<= GROOM_MAXID, build/gen/gentables.h), generated ones by relative id
(id - the first generated id). Compared: id, object, x, y, sprite, depth, alarms, and the variables CVARS the
instance has in the runner (missing there = not compared; `type` only for the exit objects, `style` only for
oShopkeeper); the globals the runner has set; global.roomPath; the tile_add tiles per depth in order (the runner
lists a layer's tiles newest first: reversed here); the 4 RNG words after generation. The first difference of
each failing case is reported with the instances around it; a C case that reached untranslated GML says so.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tracer  # noqa: E402

def maxid():
    """GROOM_MAXID from build/gen/gentables.h (tools/hdgentables.py): ids above it are created at run time"""
    h = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'gen', 'gentables.h')
    for line in open(h):
        if line.startswith('#define GROOM_MAXID'):
            return int(line.split()[2])
    raise SystemExit('GROOM_MAXID not in ' + h)


CVARS = ['invincible', 'status', 'cost', 'forSale', 'shopWall', 'value', 'inDiceHouse', 'cleanDeath', 'facing',
         'counter', 'spurt', 'deathTimer', 'held', 'swimming', 'dir', 'spurtTime', 'shiftToggle', 'New', 'linkVal',
         'style', 'treasure', 'type']
TYPE_OBJS = ('oExit', 'oEntrance', 'oXMarket')      # `type` compared only here (elsewhere a per-object constant)


def tiles_by_depth(tiles):
    """tile lists compared per depth (layer), in order within a depth"""
    d = {}
    for t in tiles:
        d.setdefault(t[-1], []).append(tuple(t))
    return d


def ref_cases(path, names_path):
    names = tracer.load_names(names_path)
    top = maxid()
    for r in tracer.gen_records(open(path, 'rb').read()):
        lvl = [x for x in reversed(r['insts']) if not x['persistent']]
        gen = [x['id'] for x in lvl if x['id'] > top]
        base = min(gen) if gen else 0
        insts = []
        for x in lvl:
            iid = x['id'] if x['id'] <= top else x['id'] - base
            obj = names['O'][x['obj']]
            v = {k: x['vars'][k] for k in CVARS if k in x['vars']}
            if 'style' in v and obj != 'oShopkeeper':
                del v['style']
            if 'type' in v and obj not in TYPE_OBJS:
                del v['type']
            insts.append(dict(id=iid, obj=obj, x=x['x'], y=x['y'], spr=names['S'].get(x['spr'], '-'),
                              depth=x['depth'], alarms={k2: int(a) for k2, a in x['alarms'].items()}, vars=v))
        yield dict(case=r['case'], seed=r['seed'], level=r['level'], globals=r['globals'], roomPath=r['roomPath'],
                   insts=insts, draws=r['draws'], tiles=tiles_by_depth(list(reversed(r['tiles']))))


def num_or_str(t):
    try:
        return float(t)
    except ValueError:
        return t


def c_cases(path):
    cur = None
    for line in open(path):
        f = line.rstrip('\n').split(' ')
        if not f or not f[0]:
            continue
        if f[0] == 'C':
            if cur:
                cur['tiles'] = tiles_by_depth(cur['tiles'])
                yield cur
            cur = dict(case=int(f[1]), seed=int(f[2]), level=int(f[3]), status=int(f[4]), globals={}, insts=[],
                       tiles=[], untranslated=0)
        elif f[0] == 'U':
            cur['untranslated'] = int(f[1])
        elif f[0] == 'G':
            cur['globals'][f[1]] = float(f[2])
        elif f[0] == 'R':
            cur['roomPath'] = [float(v) for v in f[1:]]
        elif f[0] == 'I':
            al = {}
            if f[7] != '-':
                for kv in f[7].split(','):
                    a, b = kv.split('=')
                    al[int(a)] = int(b)
            v = {}
            for kv in f[8:]:
                k, val = kv.split('=', 1)
                v[k] = num_or_str(val.replace('~', ' '))
            cur['insts'].append(dict(id=int(f[1]), obj=f[2], x=float(f[3]), y=float(f[4]), spr=f[5],
                                     depth=float(f[6]), alarms=al, vars=v))
        elif f[0] == 'T':
            cur['tiles'].append((f[1],) + tuple(int(t) for t in f[2:9]))
        elif f[0] == 'D':
            cur['draws'] = [int(t) for t in f[1:5]]
    if cur:
        cur['tiles'] = tiles_by_depth(cur['tiles'])
        yield cur
    return


def fmt(i, keys=None):
    if i is None:
        return '(none)'
    v = ' '.join(f'{k}={g}' for k, g in sorted(i['vars'].items()) if k in CVARS and (keys is None or k in keys))
    return f"{i['id']} {i['obj']} {i['x']:g},{i['y']:g} {i['spr']} d{i['depth']:g} al{i['alarms']} {v}"


def inst_diff(r, c):
    for k in ('id', 'obj', 'x', 'y', 'spr', 'depth', 'alarms'):
        if r[k] != c[k]:
            return k
    for k, v in r['vars'].items():
        if k in CVARS and k in c['vars'] and c['vars'][k] != v:
            return 'var ' + k
    return None


def compare(rc, cc, show):
    out = []
    for k in rc['globals']:
        if rc['globals'][k] == tracer.BAD:
            continue                                     # not set in the runner (yet)
        if k in cc['globals'] and cc['globals'][k] != rc['globals'][k]:
            out.append(f"global {k}: ref {rc['globals'][k]:g} C {cc['globals'][k]:g}")
    if cc.get('status'):
        out.append(f"C reached untranslated GML (code {cc.get('untranslated')})")
    if rc['roomPath'] != cc.get('roomPath'):
        out.append(f"roomPath: ref {[int(v) for v in rc['roomPath']]} C {[int(v) for v in cc.get('roomPath', [])]}")
    ri, ci = rc['insts'], cc['insts']
    for n in range(max(len(ri), len(ci))):
        r = ri[n] if n < len(ri) else None
        c = ci[n] if n < len(ci) else None
        what = 'count' if r is None or c is None else inst_diff(r, c)
        if what:
            out.append(f'instance #{n} ({what}) of ref {len(ri)} / C {len(ci)}:')
            for m in range(max(0, n - show), n):
                out.append(f'    same {fmt(ri[m])}')
            keys = set(r['vars']) if r else None
            out.append(f'    ref  {fmt(r)}')
            out.append(f'    C    {fmt(c, keys)}')
            for m in range(n + 1, min(n + 1 + show, max(len(ri), len(ci)))):
                out.append(f"    ref  {fmt(ri[m]) if m < len(ri) else '-'}")
                out.append(f"    C    {fmt(ci[m]) if m < len(ci) else '-'}")
            break
    for dep in sorted(set(rc['tiles']) | set(cc['tiles'])):
        a, b = rc['tiles'].get(dep, []), cc['tiles'].get(dep, [])
        if a != b:
            k = next((n for n, (p, q) in enumerate(zip(a, b)) if p != q), min(len(a), len(b)))
            out.append(f"tiles at depth {dep}: ref {len(a)} C {len(b)}, first difference #{k}: "
                       f"ref {a[k] if k < len(a) else '-'} C {b[k] if k < len(b) else '-'}")
            break
    if rc['draws'] != cc.get('draws'):
        out.append(f"RNG after generation: ref {rc['draws']} C {cc.get('draws')}")
    return out


def main():
    a = sys.argv[1:]
    opt = lambda k, d: int(a[a.index(k) + 1]) if k in a else d
    if a and a[0] == '--dump':
        want = opt('--case', None)
        for rc in ref_cases(a[1], a[2]):
            if want is not None and rc['case'] != want:
                continue
            print(f"case {rc['case']} seed {rc['seed']} level {rc['level']} n {len(rc['insts'])} draws {rc['draws']}")
            print('  ' + ' '.join(f'{k}={v:g}' for k, v in rc['globals'].items()))
            for i in rc['insts']:
                print('  ' + fmt(i))
        return 0
    if len(a) < 3:
        sys.exit(__doc__)
    refs = list(ref_cases(a[0], a[1]))
    cs = {c['case']: c for c in c_cases(a[2])}
    bad = 0
    shown = 0
    for rc in refs:
        cc = cs.get(rc['case'])
        if cc is None:
            print(f"case {rc['case']}: missing in C output")
            bad += 1
            continue
        d = compare(rc, cc, opt('--show', 4))
        if d:
            bad += 1
            if shown < opt('--max', 3):
                shown += 1
                print(f"case {rc['case']} (seed {rc['seed']}, level {rc['level']}, C status {cc['status']}):")
                print('\n'.join('  ' + x for x in d))
    print(f"{len(refs) - bad} / {len(refs)} cases equal ({sum(len(r['insts']) for r in refs)} instances)")
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
