#!/usr/bin/env python3
"""Compare the host play loop (build/host/playhost) with a reference trace of the HD runner (tools/tracer.py SPT3).

    tools/playcmp.py <trace.bin> <names.txt> <playhost output> [--max N] [--quiet]

Per record: the header (room, level, life, bombs, ropes, money, view, oGame.time) and every instance by id
(object, x, y, sprite, image_index, image_xscale / yscale / angle, alpha, depth, visible, alarms, xVel / yVel when
the runner has them, image_speed, and the variables playhost prints that the trace also has). The tracer's own
instances (oGamepad, and the persistent oGlobals / oScreen) are left out. image_angle within 1e-9 (fdlibm atan
against the runner's libm); everything else exact.
Prints the first differences (with the record's context) and a summary line:
    RESULT <records equal>/<records compared> first_diff=<rec or -> untranslated=<code or 0>
and the route check, from the record headers of each side over all of its records:
    ROUTE ref|c rooms=<room@t ...> deaths=<n> money=<last> bombs=<last> ropes=<last> records=<n>
    ROUTE <equal|DIFFERENT>
(over the records both sides have; rooms: each room entered with the route step t of its first record; a death:
life going from >= 1 to < 1; the reference's records past the C output, e.g. an unmodelled room, are listed)
"""
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tracer  # noqa: E402

SKIP = {'oGamepad', 'oGlobals', 'oScreen'}


def fnum(s):
    return float(s)


def load_c(path):
    recs = []
    cur = None
    for line in open(path):
        f = line.split()
        if not f:
            continue
        if f[0] == 'R':
            cur = dict(rec=int(f[1]), phase=int(f[2]), t=int(f[3]), room=int(f[4]), level=int(f[5]),
                       life=int(f[6]), bombs=int(f[7]), rope=int(f[8]), money=int(f[9]), xview=int(f[10]),
                       yview=int(f[11]), time=int(f[12]), untr=int(f[13]), dops=int(f[14]), insts={})
            recs.append(cur)
        elif f[0] == 'I':
            al = {}
            if f[13] != '-':
                for kv in f[13].split(','):
                    k, v = kv.split('=')
                    al[int(k)] = float(v)
            vs = {}
            for kv in f[17:]:
                k, v = kv.split('=')
                vs[k] = float(v)
            cur['insts'][int(f[1])] = dict(obj=f[2], x=fnum(f[3]), y=fnum(f[4]), spr=f[5], img=fnum(f[6]),
                                           xscale=fnum(f[7]), yscale=fnum(f[8]), angle=fnum(f[9]),
                                           alpha=fnum(f[10]), depth=fnum(f[11]), visible=int(f[12]), alarms=al,
                                           xVel=fnum(f[14]), yVel=fnum(f[15]), ispd=fnum(f[16]), vars=vs)
    return recs


def compare_inst(r, c, out):
    def d(name, a, b, tol=0.0):
        if a is None:
            return
        if (abs(a - b) > tol) if tol else (a != b):
            out.append(f'{name} ref {a!r} c {b!r}')
    d('x', r['x'], c['x'])
    d('y', r['y'], c['y'])
    d('image_index', r['img'], c['img'])
    dr = r['draw']
    d('xscale', dr['xscale'], c['xscale'])
    d('yscale', dr['yscale'], c['yscale'])
    d('angle', dr['angle'], c['angle'], 1e-9)
    d('alpha', dr['alpha'], c['alpha'])
    d('depth', dr['depth'], c['depth'])
    d('visible', dr['visible'], c['visible'])
    if r['alarms'] != c['alarms']:
        out.append(f'alarms ref {r["alarms"]} c {c["alarms"]}')
    d('xVel', r['xVel'], c['xVel'])
    d('yVel', r['yVel'], c['yVel'])
    d('image_speed', r.get('ispd'), c['ispd'])
    for k, v in c['vars'].items():
        if k in r['vars']:
            d(k, r['vars'][k], v)


def main():
    a = sys.argv[1:]
    if len(a) < 3:
        sys.exit(__doc__)
    names = tracer.load_names(a[1])
    crecs = load_c(a[2])
    mx = int(a[a.index('--max') + 1]) if '--max' in a else 3
    quiet = '--quiet' in a
    data = open(a[0], 'rb').read()
    rh = [(h['room'], h['t'], h['plife'], h['money'], h['bombs'], h['rope']) for h, _ in tracer.records(data)]
    ch = [(c['room'], c['t'], c['life'], c['money'], c['bombs'], c['rope']) for c in crecs]
    nmin = min(len(rh), len(ch))
    rsum = [summary(rh[:nmin], names), summary(ch[:nmin], names)]
    rest = summary(rh[nmin - 1:], names) if len(rh) > nmin else None
    neq = ncmp = 0
    first = None
    shown = 0
    untr = 0
    for (hd, insts), c in zip(tracer.records(data), crecs):
        ncmp += 1
        untr = untr or c['untr']
        diffs = []
        rn = names['R'].get(hd['room'], hd['room'])
        for k, rv, cv in (('room', hd['room'], c['room']), ('level', hd['currLevel'], c['level']),
                          ('life', hd['plife'], c['life']), ('bombs', hd['bombs'], c['bombs']),
                          ('rope', hd['rope'], c['rope']), ('money', hd['money'], c['money']),
                          ('xview', hd.get('xview'), c['xview']), ('yview', hd.get('yview'), c['yview']),
                          ('time', hd.get('time'), c['time']), ('t', hd['t'], c['t'])):
            if rv is not None and rv != cv:
                diffs.append(f'header {k}: ref {rv!r} c {cv!r}')
        ref = {}
        for i in insts:
            on = names['O'].get(i['obj'], str(i['obj']))
            if on in SKIP:
                continue
            ref[i['id']] = dict(i, objname=on, sprname=names['S'].get(i['spr'], '-') if i['spr'] >= 0 else '-')
        for iid in sorted(set(ref) - set(c['insts'])):
            r = ref[iid]
            diffs.append(f'missing in C: {iid} {r["objname"]} {r["x"]} {r["y"]} {r["sprname"]}')
        for iid in sorted(set(c['insts']) - set(ref)):
            ci = c['insts'][iid]
            diffs.append(f'extra in C: {iid} {ci["obj"]} {ci["x"]} {ci["y"]} {ci["spr"]}')
        for iid in sorted(set(ref) & set(c['insts'])):
            r, ci = ref[iid], c['insts'][iid]
            out = []
            if r['objname'] != ci['obj']:
                out.append(f'object ref {r["objname"]} c {ci["obj"]}')
            if r['sprname'] != ci['spr']:
                out.append(f'sprite ref {r["sprname"]} c {ci["spr"]}')
            compare_inst(r, ci, out)
            if out:
                diffs.append(f'{iid} {r["objname"]}: ' + '; '.join(out))
        if diffs:
            if first is None:
                first = hd['rec']
            if shown < mx and not quiet:
                shown += 1
                print(f'rec {hd["rec"]} (t {hd["t"]}, room {rn}, untranslated {c["untr"]}): {len(diffs)} differences')
                for x in diffs[:40]:
                    print('   ', x)
        else:
            neq += 1
    if len(crecs) != ncmp and not quiet:
        print(f'records: C {len(crecs)}, compared {ncmp}')
    print(f'RESULT {neq}/{ncmp} first_diff={first if first is not None else "-"} untranslated={untr}')
    for side, r in zip(('ref', 'c'), rsum):
        print(f'ROUTE {side} ' + ' '.join(f'{k}={v}' for k, v in r.items()))
    print('ROUTE ' + ('equal' if rsum[0] == rsum[1] else 'DIFFERENT') + f' over {nmin} records' +
          (f'; ref continues {len(rh) - nmin} records: rooms={rest["rooms"]}' if rest else ''))


def summary(hs, names):
    """the route check: rooms entered (room@t), deaths, last money / bombs / ropes, record count"""
    rooms, deaths, prev_room, prev_life = [], 0, None, None
    for room, t, life, money, bombs, rope in hs:
        if room != prev_room:
            rooms.append(f'{names["R"].get(room, room)}@{t}')
            prev_room = room
        if prev_life is not None and prev_life >= 1 > life and life > -1e8:
            deaths += 1
        if life > -1e8:
            prev_life = life
    last = hs[-1] if hs else (0,) * 6
    return dict(rooms=','.join(rooms), deaths=deaths, money=last[3], bombs=last[4], ropes=last[5], records=len(hs))


if __name__ == '__main__':
    main()
