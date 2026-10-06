#!/usr/bin/env python3
"""Cache-bypass what-ifs on jtcost traces (docs/ICACHE.md). The SH7604's 4 KB cache is filled by every cached access;
code and data that are used once per step (a fill that is evicted before its next use: a "dead fill") cost a line
fill and also push out lines that would have hit. On the SH7604 any address with bits 31-29 = 001 (0x2xxxxxxx) is
read past the cache without filling a line (jtframe CACHE.sv NOCACHE_AREA), and code runs from SIMM 1's mirror at
0x26000000 on jtcps3 (cps3-testgame docs/CPS3.md: 2.5 clocks a NOP), so a function or a data object can be placed
there by the linker (a section linked at its address | 0x20000000) without touching the code that uses it.

  jtbypass.py parse <trace> <nm.txt> <out.pkl>       the trace (scripts/jtcost_native.sh JTC_KEEP=1) as events
  jtbypass.py dead <pkl> [<pkl>...]                  dead fills by function / data symbol (baseline cache)
  jtbypass.py sim <pkl> [--ncfile nc.txt] [--nc f,..] [--ncd s,..] [--cram f,..]   the model with those functions / data symbols
                                                     uncached; --cram: those functions in the two-way mode's cache
                                                     RAM (2 KB; the cache then 2 ways)
  jtbypass.py pcs <pkl> [<pkl>...]                   modelled clocks by instruction address (for addr2line)
  jtbypass.py greedy <pkl> [<pkl>...] [--max N]      a function set chosen greedily on the summed model of the traces
  jtbypass.py gready <pkl>.. (same with data symbols)

Cost model: tools/jtcost.py's fit constants (MAME cycles; store +5.06; load hit +0.5; fetch line miss +12.98; data
line miss +37.78; MUL.L +4.37; MULx.W +0.96; DIVU +37), plus for uncached accesses the measured jtcps3 costs
(cps3-testgame docs/CPS3.md ttest): a load from SIMM 1 4.5 clocks (+3.5), from main RAM 7.5 (+6.5); an instruction
fetched past the cache: each 32-bit word entered +3 (2.5 clocks a NOP: two instructions a word), each taken branch or
jump out of uncached code +3 (the SIMM empty loop: 10 clocks against 4; the prefetched word is thrown away). Stores are
write-through with no allocation, so they cost the same either way.

A "dead fill" is a line filled by a miss and evicted (or still resident at the end of the step: counted apart) with no
hit in between. With the step traced alone, lines still resident at its end are not dead (the next step may hit)."""
import sys, re, bisect, collections, pickle, array

C = dict(store=5.06, ld_hit=0.5, imiss=12.98, dmiss_ram=37.78, dmiss_simm=37.78, mull=4.37, mulw=0.96, divu=37.0,
         unc_simm=3.5, unc_ram=6.5, uword=3.0, ujump=3.0)
import os
for _k in C:                                       # JTB_<NAME>=<clocks>: override a constant (sensitivity runs)
    if os.getenv('JTB_' + _k.upper()): C[_k] = float(os.getenv('JTB_' + _k.upper()))
MAMEX = dict(BRA=1, BSR=1, BRAF=1, BSRF=1, JMP=1, JSR=1, RTS=1, RTE=3, TRAPA=7, **{'MUL.L': 1, 'DMULU.L': 1,
             'DMULS.L': 1, 'MAC.L': 2, 'MAC.W': 2, 'LDC.L': 2, 'STC.L': 1, 'TAS.B': 3})
memre = re.compile(r'@(\(\$?([0-9A-Fa-f]+),(R\d+|GBR|PC)\)|\(R0,(R\d+)\)|-(R\d+)|(R\d+)\+?|\(\$([0-9A-F]+)\))')
SIZE = {'B': 1, 'W': 2, 'L': 4}


def load_nm(nmf):
    syms, dsy = [], []
    for l in open(nmf):
        p = l.split()
        if len(p) != 3: continue
        a = int(p[0], 16)
        if p[1] in 'tTwW': syms.append((a & 0x1fffffff, p[2]))
        if p[1] in 'bBdDrRtT': dsy.append((a & 0x1fffffff, p[2]))
    syms.sort(); dsy.sort()
    return syms, dsy


class Names:
    def __init__(s, syms):
        s.a = [x[0] for x in syms]; s.n = [x[1] for x in syms]
    def __call__(s, a):
        i = bisect.bisect_right(s.a, a & 0x1fffffff) - 1
        return s.n[i] if i >= 0 else '?'


def parse(tr, nmf, out):
    """Events per instruction: pc (as run: 0x26xxxxxx for code run past the cache), MAME cycles, flags (1 MUL.L, 2 MULx.W, 4 jump), and its data
    accesses (instruction index, address, 1 store / 2 literal / 4 stack / 8 DIVU)."""
    syms, dsy = load_nm(nmf)
    PLAY = [a for a, n in syms if n == '_play_step'][0]
    GBR = ([a for a, n in dsy if n == '_pw_mem'] or [0])[0]      # play.h pw_gbr_init: GBR = &pw_mem
    pcs = array.array('I'); mame = array.array('B'); flg = array.array('B')
    ai = array.array('I'); aa = array.array('I'); af = array.array('B')
    inside = False; ret = sp0 = None; cbr = None
    for line in open(tr):
        if '|' not in line: continue
        regs, ins = line.rstrip('\n').split('|', 1)
        r = regs.split()
        if len(r) != 17 or ':' not in ins: continue
        pcs_, dis = ins.split(':', 1)
        try: pc = int(pcs_, 16); R = [int(x, 16) for x in r]
        except ValueError: continue
        if not inside:
            if (pc & 0x1fffffff) == PLAY: inside = True; ret = R[16]; sp0 = R[15]
            else: continue
        elif pc == ret and R[15] == sp0:
            break
        dis = dis.strip(); op = dis.split()[0] if dis else ''
        args_ = dis[len(op):].strip()
        m = 1 + MAMEX.get(op, 0)
        if cbr is not None:
            k, bpc = cbr
            if k == 1:
                if pc != bpc + 2: m += 2
                cbr = None
            elif k == 2 and pc == bpc + 2: cbr = (3, bpc)
            else:
                if pc != bpc + 4: m += 1
                cbr = None
        if op in ('BT', 'BF'): cbr = (1, pc)
        elif op in ('BTS', 'BFS', 'BT/S', 'BF/S'): cbr = (2, pc)
        f = 0
        if op in ('MUL.L', 'DMULS.L', 'DMULU.L'): f |= 1
        if op in ('MULS.W', 'MULU.W', 'MULS', 'MULU'): f |= 2
        idx = len(pcs)
        pcs.append(pc); mame.append(min(m, 255)); flg.append(f)
        if (op.startswith('MOV') and op not in ('MOVA', 'MOVT')) or op.startswith('MAC') or op == 'TAS.B' or \
                op.startswith(('STS.L', 'STC.L', 'LDS.L', 'LDC.L', 'AND.B', 'OR.B', 'XOR.B', 'TST.B')):
            sz = SIZE.get(op[-1], 4) if '.' in op else 4
            parts = [p.strip() for p in re.split(r',(?![^()]*\))', args_.split(' [')[0])]
            for k, p in enumerate(parts):
                if not p.startswith('@'): continue
                mm = memre.match(p)
                if not mm: continue
                lit = False
                if mm.group(3):
                    d = int(mm.group(2), 16); b = mm.group(3)
                    if b == 'PC':
                        mb = re.search(r'\[([0-9A-F]+)\]', args_); a = int(mb.group(1), 16); lit = True
                    elif b == 'GBR': a = GBR + d
                    else: a = R[int(b[1:])] + d
                elif mm.group(4): a = R[0] + R[int(mm.group(4)[1:])]
                elif mm.group(5): a = R[int(mm.group(5)[1:])] - sz
                elif mm.group(6): a = R[int(mm.group(6)[1:])]
                elif mm.group(7): a = int(mm.group(7), 16)
                else: continue
                a &= 0xffffffff
                st = (k == len(parts) - 1) and not op.startswith(('MAC', 'LDS', 'LDC'))
                if op.startswith(('STS.L', 'STC.L')): st = True
                fl = (1 if st else 0) | (2 if lit else 0)
                if 0x02000000 <= (a & 0x1fffffff) < 0x02080000 and abs(a - R[15]) < 4096: fl |= 4
                if st and a in (0xffffff04, 0xffffff14): fl |= 8
                ai.append(idx); aa.append(a); af.append(fl)
    if not inside: sys.exit('jtbypass.py: play_step not entered')
    pickle.dump(dict(pcs=pcs, mame=mame, flg=flg, ai=ai, aa=aa, af=af, syms=syms, dsy=dsy), open(out, 'wb'), 2)
    print('%s: %d instructions, %d data accesses' % (out, len(pcs), len(ai)))


def region(a):
    a &= 0x1fffffff
    if 0x06000000 <= a < 0x07000000: return 'simm'
    if 0x02000000 <= a < 0x02080000: return 'ram'
    return 'other'


class Sim:
    """4-way x 64-set LRU cache, write-through, no allocate on write. Tracks per-fill hits for dead-fill counts."""
    def __init__(s, ways=4):
        s.sets = [[] for _ in range(64)]; s.ways = ways
        s.fill_owner = {}                # line -> (owner key, hits)
        s.dead = collections.Counter(); s.fills = collections.Counter(); s.live = collections.Counter()
    def access(s, a, owner, alloc=True):
        ln = a >> 4; st = s.sets[ln & 63]
        if ln in st:
            st.remove(ln); st.append(ln)
            o = s.fill_owner.get(ln)
            if o is not None: o[1] += 1
            return True
        if alloc:
            if len(st) == s.ways:
                v = st.pop(0); o = s.fill_owner.pop(v, None)
                if o is not None and o[1] == 0: s.dead[o[0]] += 1
            st.append(ln); s.fill_owner[ln] = [owner, 0]; s.fills[owner] += 1
        return False
    def finish(s):
        for ln, o in s.fill_owner.items():
            if o[1] == 0: s.live[o[0]] += 1


BYFN = int(os.getenv('JTB_BYFN', '0'))   # 1: data fills by symbol@function; n > 1: and by offset % n in the symbol


def model(ev, ncf=frozenset(), ncd=frozenset(), detail=False, ways=4, cram=frozenset()):
    """Modelled jtcps3 clocks of one traced step with the functions ncf run from the uncached mirror and the data
    symbols ncd read through it. Returns (total, counters, per-function costs, sim)."""
    fname = ev.get('_fn') or Names(ev['syms']); ev['_fn'] = fname
    dname = ev.get('_dn') or Names(ev['dsy']); ev['_dn'] = dname
    pcs, mame, flg = ev['pcs'], ev['mame'], ev['flg']
    ai, aa, af = ev['ai'], ev['aa'], ev['af']
    # function of each pc, cached per pc
    fcache = ev.setdefault('_fc', {})
    ev.setdefault('_base', {n: a for a, n in ev['dsy']})
    dcache = ev.setdefault('_dc', {})
    sim = Sim(ways)
    tot = 0.0; cnt = collections.Counter(); per = collections.Counter() if detail else None
    perpc = ev.get('_perpc')
    last_line = -1; last_word = -1; prev_pc = -1; prev_nc = False
    j = 0; na = len(ai)
    for i in range(len(pcs)):
        pc = pcs[i]
        f = fcache.get(pc)
        if f is None: f = fcache[pc] = fname(pc)
        nc = f in ncf or (pc >> 29) == 1
        cr = f in cram
        c = mame[i]
        fl = flg[i]
        if fl & 1: c += C['mull']
        if fl & 2: c += C['mulw']
        # fetch
        if cr:
            last_line = -1; last_word = -1
        elif nc:
            if prev_nc and pc != prev_pc + 2: c += C['ujump']; cnt['ujump'] += 1
            w = pc >> 2
            if w != last_word:
                c += C['uword']; cnt['uword'] += 1
            last_word = w; last_line = -1
        else:
            if prev_nc and pc != prev_pc + 2: c += C['ujump']; cnt['ujump'] += 1
            ln = pc >> 4
            if ln != last_line:
                last_line = ln
                if not sim.access(pc, 'f:' + f):
                    c += C['imiss']; cnt['imiss'] += 1
            last_word = -1
        prev_pc = pc; prev_nc = nc
        # data
        while j < na and ai[j] == i:
            a = aa[j]; d = af[j]; j += 1
            if d & 1:
                c += C['store']; cnt['store'] += 1
                if d & 8: c += C['divu']
                if (a >> 29) == 0: sim.access(a, None, alloc=False)
                continue
            rg = region(a)
            unc = (a >> 29) != 0 or rg == 'other'
            if not unc:
                if d & 2 and cr:
                    c += C['ld_hit']; cnt['cram'] += 1; continue   # cache RAM: 1.5 clocks
                if d & 2:
                    unc = nc                   # a literal of code run uncached is read through the mirror too
                    owner = 'l:' + f
                elif d & 4:
                    owner = 'stack'
                else:
                    owner = dcache.get(a >> 2)
                    if owner is None:
                        owner = dcache[a >> 2] = 'd:' + dname(a)
                    if owner[2:] in ncd: unc = True
                    if BYFN: owner = owner + '@' + f + ('+%d' % ((a - ev['_base'][owner[2:]]) % BYFN) if BYFN > 1 and owner[2:] in ev['_base'] else '')
            if unc:
                if rg == 'simm': c += C['unc_simm']; cnt['unc_simm'] += 1
                elif rg == 'ram': c += C['unc_ram']; cnt['unc_ram'] += 1
                continue
            if sim.access(a, owner):
                c += C['ld_hit']; cnt['ld_hit'] += 1
            else:
                c += C['dmiss_' + rg] if rg != 'other' else 0; cnt['dmiss_' + rg] += 1
        tot += c
        if detail: per[f] += c
        if perpc is not None: perpc[pc] += c
    sim.finish()
    return tot, cnt, per, sim


def load(p):
    return pickle.load(open(p, 'rb'))


def report(ev, ncf, ncd, label, ways=4, cram=frozenset()):
    t, cnt, per, sim = model(ev, ncf, ncd, ways=ways, cram=cram)
    print('%-28s model %9.0f  imiss %6d  dmiss ram %5d simm %5d  uword %6d ujump %5d  unc ram %5d simm %5d' % (
        label, t, cnt['imiss'], cnt['dmiss_ram'], cnt['dmiss_simm'], cnt['uword'], cnt['ujump'], cnt['unc_ram'],
        cnt['unc_simm']))
    return t


def main():
    a = sys.argv[1:]
    if not a: sys.exit(__doc__)
    cmd = a.pop(0)
    opts = {}
    pos = []
    while a:
        x = a.pop(0)
        if x.startswith('--'): opts[x] = a.pop(0)
        else: pos.append(x)
    ncf = set(x for x in opts.get('--nc', '').split(',') if x)
    ncd = set(x for x in opts.get('--ncd', '').split(',') if x)
    if '--ncfile' in opts:                       # tests/playsh2/mknc.py's list: f -> --nc, b / r -> --ncd
        for l in open(opts['--ncfile']):
            l = l.split('#')[0].split()
            if len(l) == 2: (ncf if l[0] == 'f' else ncd).add('_' + l[1])
    ncf = frozenset(ncf); ncd = frozenset(ncd)
    cram = frozenset(x for x in opts.get('--cram', '').split(',') if x)
    ways = 2 if cram else 4
    if cmd == 'parse':
        parse(*pos); return
    evs = [load(p) for p in pos]
    if cmd == 'sim':
        for p, ev in zip(pos, evs):
            b = report(ev, frozenset(), frozenset(), p.split('/')[-1] + ' base')
            t = report(ev, ncf, ncd, p.split('/')[-1] + ' policy', ways, cram)
            print('  change %+.2f %%' % (100 * (t / b - 1)))
        return
    if cmd == 'pcs':                                 # modelled clocks by instruction address: "pc clocks" lines
        agg = collections.Counter()
        for ev in evs:
            ev['_perpc'] = collections.Counter(); model(ev, ncf, ncd)
            agg.update(ev['_perpc'])
        for pc, v in sorted(agg.items()): print('%08x %.0f' % (pc, v))
        return
    if cmd == 'dead':
        agg_f = collections.Counter(); agg_d = collections.Counter(); agg_l = collections.Counter()
        cost = collections.Counter(); tot = 0
        for ev in evs:
            t, cnt, per, sim = model(ev, detail=True)
            tot += t
            for k, v in per.items(): cost[k] += v
            for k, v in sim.fills.items(): agg_f[k] += v
            for k, v in sim.dead.items(): agg_d[k] += v
            for k, v in sim.live.items(): agg_l[k] += v
        print('total model %.0f over %d traces' % (tot, len(evs)))
        print('\nfills by owner (f: fetch of a function, l: its literal pool, d: data symbol): fills, dead, dead %')
        for k, v in sorted(agg_f.items(), key=lambda kv: -kv[1])[:int(opts.get('--top', 60))]:
            print('  %-34s %6d %6d %5.1f%%   (resident unused at end %d)' % (k, v, agg_d[k], 100 * agg_d[k] / v, agg_l[k]))
        F = sum(v for k, v in agg_f.items() if k and k.startswith('f:')); D = sum(v for k, v in agg_d.items() if k and k.startswith('f:'))
        print('\nfetch fills %d, dead %d (%.1f%%)' % (F, D, 100 * D / max(F, 1)))
        F = sum(v for k, v in agg_f.items() if k and not k.startswith('f:')); D = sum(v for k, v in agg_d.items() if k and not k.startswith('f:'))
        print('data fills %d, dead %d (%.1f%%)' % (F, D, 100 * D / max(F, 1)))
        return
    if cmd in ('greedy', 'gready'):
        data = cmd == 'gready'
        base = [model(ev)[0] for ev in evs]
        B = sum(base)
        # candidates: owners with the most dead fills
        dead = collections.Counter(); fills = collections.Counter()
        for ev in evs:
            sim = model(ev)[3]
            for k, v in sim.dead.items(): dead[k] += v
            for k, v in sim.fills.items(): fills[k] += v
        pre = 'd:' if data else 'f:'
        funcs = set(n for ev in evs for a, n in ev['syms'])     # data candidates: data objects only (not code)
        cands = [k[2:] for k, v in dead.most_common() if k and k.startswith(pre) and v >= int(opts.get('--min', 8))
                 and not (data and k[2:] in funcs)]
        cands = cands[:int(opts.get('--max', 60))]
        cur_f, cur_d = set(ncf), set(ncd)
        cur = sum(model(ev, frozenset(cur_f), frozenset(cur_d))[0] for ev in evs)
        print('base %.0f; start %.0f; %d candidates' % (B, cur, len(cands)))
        for c in cands:
            tf = cur_f | ({c} if not data else set()); td = cur_d | ({c} if data else set())
            t = sum(model(ev, frozenset(tf), frozenset(td))[0] for ev in evs)
            keep = t < cur - 0.0005 * B
            print('  %-30s dead %5d / %5d fills  %9.0f  %+6.2f %%  %s' % (c, dead[pre + c], fills[pre + c], t,
                  100 * (t / cur - 1), 'keep' if keep else ''))
            if keep: cur, cur_f, cur_d = t, tf, td
        print('result %.0f (%+.2f %% on base); per trace:' % (cur, 100 * (cur / B - 1)))
        for p, ev, b in zip(pos, evs, base):
            t = model(ev, frozenset(cur_f), frozenset(cur_d))[0]
            print('  %-40s %9.0f -> %9.0f  %+6.2f %%' % (p.split('/')[-1], b, t, 100 * (t / b - 1)))
        print('--nc ' + ','.join(sorted(cur_f)))
        print('--ncd ' + ','.join(sorted(cur_d)))
        return
    sys.exit(__doc__)


if __name__ == '__main__':
    main()
