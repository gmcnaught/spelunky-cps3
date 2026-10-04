#!/usr/bin/env python3
"""jtcps3 cost model of one traced play_step (docs/PERF3.md section 1; docs/REVIEW-SH2.md section 3). Input: a MAME
debugger trace with r0-r15 and pr logged on every instruction ("R0 .. R15 PR|PC: disassembly", scripts/jtcost.sh,
scripts/lua/jtcost.lua) and sh-elf-nm -n of the same main.elf. Only the instructions from play_step's entry to its
return are counted; the trace is read as a stream.

Cache (SH7604 per the jtframe RTL): 4 KB, 4 ways x 64 sets x 16 B lines, unified, LRU (RTL: pseudo-LRU),
write-through, a write miss does not allocate, 0x2xxxxxxx and above uncached. Each load's and store's address is
rebuilt from the registers.

Two constant sets, both computed on every run (one JTCOST line each); the tables use JTCOST_CONSTS / --consts:
  review (default): REVIEW-SH2.md's hand-set jtcps3 costs (cps3-testgame / maldita CPS3.md measurements):
    instruction 1; jump (BRA BSR BRAF BSRF JMP JSR RTS RTE) +2; fetch line miss +12; load hit +0.5;
    load miss: main RAM line +27.5, SIMM line +15.5; uncached load: SIMM 4.5, other 7.5; store 6.06;
    MUL.L / DMULx.L (with its STS) +7.
  fit: jtmodel's (build/handoff/jtmodel_new.tar, tools/jtmodel.py fit) least-squares fit of the two line-miss
    penalties to docs/EQUIV.md's measured jtcps3 step means of the grid build 899ce76 (18 routes; rms error 0.3 %),
    the other costs fixed: MAME interpreter cycles (1 + the opcode's extra cycles + taken conditional branches);
    store +5.06; load hit +0.5; fetch line miss +12.98; data line miss (main RAM or SIMM) +37.78;
    MUL.L / DMULx.L +4.37; MULx.W +0.96; DIVU (store to 0xFFFFFF04 / 0xFFFFFF14) +37; uncached loads +0.
What-ifs (both sets): WAYS=2 (two-way mode), CRAM_STACK=1 (stack accesses at cache-RAM cost), NOLIT=1 (literal-pool
loads removed).
JTCOST lines: ins instructions; mame_cyc MAME interpreter cycles by the opcode table (the fit's base; not the FRT
clocks playsh2 records); model jtcps3 clocks; stores (stack_st of them on the stack); imiss fetch line misses;
dmiss_ram / dmiss_simm data line misses (lit of them literal pools of the running function).
usage: jtcost.py [--route R] [--step N] [--consts review|fit] trace nm.txt"""
import sys, re, bisect, collections, os

args = sys.argv[1:]
opt = {'--route': '?', '--step': '?', '--consts': os.getenv('JTCOST_CONSTS', 'fit')}
while args and args[0] in opt:
    opt[args[0]] = args[1]; args = args[2:]
if len(args) != 2 or opt['--consts'] not in ('review', 'fit'): sys.exit(__doc__)
SEL = opt['--consts']
WAYS = int(os.getenv('WAYS', '4')); CRAM_STACK = os.getenv('CRAM_STACK') == '1'; NOLIT = os.getenv('NOLIT') == '1'

# extra cost over an instruction's base, per event: [review, fit]
CONSTS = dict(
    store=(5.06, 5.06), ld_hit=(0.5, 0.5), imiss=(12.0, 12.98), dmiss_ram=(27.5, 37.78), dmiss_simm=(15.5, 37.78),
    ld_unc_simm=(3.5, 0.0), ld_unc_other=(6.5, 0.0), cram=(0.5, 0.5), mull=(7.0, 4.37), mulw=(0.0, 0.96),
    divu=(0.0, 37.0), jump=(2.0, 0.0), ufetch=(9.0, 0.0))
SETS = ('review', 'fit'); SI = SETS.index(SEL)
# MAME's SH-2 interpreter: extra cycles over 1 by mnemonic (the opcode table of scripts/lua/jtmodel.lua); taken
# BT / BF +2, taken BT/S / BF/S +1
MAMEX = dict(BRA=1, BSR=1, BRAF=1, BSRF=1, JMP=1, JSR=1, RTS=1, RTE=3, TRAPA=7, **{'MUL.L': 1, 'DMULU.L': 1,
             'DMULS.L': 1, 'MAC.L': 2, 'MAC.W': 2, 'LDC.L': 2, 'STC.L': 1, 'TAS.B': 3})
JUMPS = ('BRA', 'BSR', 'JMP', 'JSR', 'RTS', 'BRAF', 'BSRF', 'RTE')

tr, nmf = args
syms = []; dsy = []
for l in open(nmf):
    p = l.split()
    if len(p) != 3: continue
    if p[1] in 'tTwW': syms.append((int(p[0], 16), p[2]))
    if p[1] in 'bBdDrRtT': dsy.append((int(p[0], 16), p[2]))
syms.sort(); SA = [s[0] for s in syms]
dsy.sort(); DA = [s[0] for s in dsy]
def dname(a):
    a &= 0x1fffffff
    i = bisect.bisect_right(DA, a) - 1
    return dsy[i][1] if i >= 0 else '?'
def fn(pc):
    i = bisect.bisect_right(SA, pc) - 1
    return syms[i][1] if i >= 0 else '?'
PLAY = [a for a, n in syms if n == '_play_step'][0]
PCHIST = os.getenv('JTC_PCHIST'); pch = collections.Counter()   # JTC_PCHIST=<symbol>: its instructions by address
callers = collections.Counter()
STARTS = set(SA)                                  # a function's first instruction: one entry (call) of it
dm = collections.Counter(); dsto = collections.Counter()

class Cache:
    def __init__(s): s.sets = [[] for _ in range(64)]
    def access(s, a, alloc=True):
        st = s.sets[(a >> 4) & 63]; tag = a >> 10
        if tag in st:
            st.remove(tag); st.append(tag); return True
        if alloc:
            if len(st) == WAYS: st.pop(0)
            st.append(tag)
        return False
C = Cache()

def region(a):
    a &= 0x1fffffff
    if 0x06000000 <= a < 0x07000000: return 'simm'
    if 0x02000000 <= a < 0x02080000: return 'ram'
    if 0x04000000 <= a < 0x05000000: return 'video'
    if a >= 0x1fff0000 or (a & 0xf0000000) == 0xe0000000: return 'io'
    return 'other'

memre = re.compile(r'@(\(\$?([0-9A-Fa-f]+),(R\d+|GBR|PC)\)|\(R0,(R\d+)\)|-(R\d+)|(R\d+)\+?|\(\$([0-9A-F]+)\))')
size = {'B': 1, 'W': 2, 'L': 4}
st = collections.Counter()
per = collections.defaultdict(collections.Counter)
tot = [0.0, 0.0]
inside = False; ret = None; sp0 = None; last_line = -1
cbr = None   # pending conditional branch: (kind, pc), kind 1 BT / BF, 2 BT/S / BF/S, 3 its delay slot seen
for line in open(tr):
    if '|' not in line: continue
    regs, ins = line.rstrip('\n').split('|', 1)
    r = regs.split()
    if len(r) != 17 or ':' not in ins: continue
    pcs, dis = ins.split(':', 1)
    try: pc = int(pcs, 16); R = [int(x, 16) for x in r]
    except ValueError: continue
    if not inside:
        if pc == PLAY: inside = True; ret = R[16]; sp0 = R[15]
        else: continue
    elif pc == ret and R[15] == sp0:
        break
    f = fn(pc); c = per[f]
    if pc in STARTS:
        c['entries'] += 1
        if f.startswith('___'): callers[(f, fn(R[16]))] += 1     # libgcc helper: who called it (PR)
    if f == PCHIST: pch[pc] += 1
    dis = dis.strip(); op = dis.split()[0] if dis else ''
    args_ = dis[len(op):].strip()
    ev = collections.Counter()          # this instruction's events (keys of CONSTS)
    mame = 1 + MAMEX.get(op, 0)
    if cbr is not None:                 # the previous conditional branch: taken?
        k, bpc = cbr
        if k == 1:
            if pc != bpc + 2: mame += 2; st['br_taken'] += 1
            cbr = None
        elif k == 2 and pc == bpc + 2: cbr = (3, bpc)
        else:
            if pc != bpc + 4: mame += 1; st['br_taken'] += 1
            cbr = None
    if op in ('BT', 'BF'): cbr = (1, pc)
    elif op in ('BTS', 'BFS', 'BT/S', 'BF/S'): cbr = (2, pc)
    st['ins'] += 1; c['ins'] += 1; st['mame'] += mame; c['mame'] += mame
    # instruction fetch (one access per 16-byte line entered)
    line_a = pc & ~15
    if line_a != last_line:
        last_line = line_a
        if (pc >> 29) == 0:
            if not C.access(pc):
                ev['imiss'] += 1; st['imiss'] += 1; c['imiss'] += 1
        else:
            ev['ufetch'] += 1  # uncached fetch (not expected)
    # data access
    if (op.startswith('MOV') and op not in ('MOVA', 'MOVT')) or op.startswith('MAC') or op in ('TAS.B',) or (op.startswith('STS.L') or op.startswith('STC.L') or op.startswith('LDS.L') or op.startswith('LDC.L')) or (op.startswith('AND.B') or op.startswith('OR.B') or op.startswith('XOR.B') or op.startswith('TST.B')):
        sz = size.get(op[-1], 4) if '.' in op else 4
        parts = [p.strip() for p in re.split(r',(?![^()]*\))', args_.split(' [')[0])]
        for k, p in enumerate(parts):
            if not p.startswith('@'): continue
            m = memre.match(p)
            if not m: continue
            if m.group(3):
                d = int(m.group(2), 16); b = m.group(3)
                if b == 'PC':
                    mb = re.search(r'\[([0-9A-F]+)\]', args_); a = int(mb.group(1), 16)
                elif b == 'GBR': a = d
                else: a = R[int(b[1:])] + d
            elif m.group(4): a = R[0] + R[int(m.group(4)[1:])]
            elif m.group(5): a = R[int(m.group(5)[1:])] - sz
            elif m.group(6): a = R[int(m.group(6)[1:])]
            elif m.group(7): a = int(m.group(7), 16)
            else: continue
            a &= 0xffffffff
            is_store = (k == len(parts) - 1) and not op.startswith('MAC') and not op.startswith('LDS') and not op.startswith('LDC')
            if op.startswith(('STS.L', 'STC.L')): is_store = True
            reg = region(a)
            stack = reg == 'ram' and abs(a - R[15]) < 4096
            key = ('st' if is_store else 'ld') + '_' + ('stack' if stack else reg)
            st[key] += 1; c[key] += 1
            if CRAM_STACK and stack: a = 0xc0000000
            if NOLIT and reg == 'simm' and m.group(3) == 'PC':
                continue
            if (a & 0xf0000000) == 0xc0000000:
                ev['cram'] += 1; continue
            if is_store:
                ev['store'] += 1
                if a in (0xffffff04, 0xffffff14): ev['divu'] += 1; st['divu'] += 1
                dsto['stack' if stack else dname(a)] += 1
                if (a >> 29) == 0: C.access(a, alloc=False)
            else:
                if (a >> 29) != 0 or reg in ('io', 'video'):
                    ev['ld_unc_simm' if reg == 'simm' else 'ld_unc_other'] += 1; st['ld_unc'] += 1
                elif C.access(a):
                    ev['ld_hit'] += 1
                else:
                    ev['dmiss_' + ('simm' if reg == 'simm' else 'ram')] += 1
                    st['dmiss_' + reg] += 1; c['dmiss'] += 1
                    dm[('lit:' + fn(a)) if (reg == 'simm' and fn(a) == f) else dname(a)] += 1
    if op in ('MUL.L', 'DMULS.L', 'DMULU.L'): ev['mull'] += 1; st['mul'] += 1
    if op in ('MULS.W', 'MULU.W', 'MULS', 'MULU'): ev['mulw'] += 1; st['mulw'] += 1
    if op in JUMPS: ev['jump'] += 1; st['jump'] += 1
    if op in ('JSR', 'BSR', 'BSRF'): st['call'] += 1; c['calls_out'] += 1
    for s in (0, 1):
        cost = (1.0 if s == 0 else mame) + sum(CONSTS[k][s] * n for k, n in ev.items())
        tot[s] += cost
        if s == SI: c['cost'] += cost
if not inside: sys.exit('jtcost.py: play_step (%08X) not entered in the trace' % PLAY)

ins = st['ins']; cost = tot[SI]
stores = sum(v for k, v in st.items() if k.startswith('st_'))
lit = sum(v for k, v in dm.items() if k.startswith('lit:'))
print('route %s step %s, constants %s; instructions %d, MAME cycles %d, modelled jtcps3 %.0f, ratio %.2f '
      '(the other set, %s: %.0f)' % (opt['--route'], opt['--step'], SEL, ins, st['mame'], cost, cost / max(ins, 1),
                                    SETS[1 - SI], tot[1 - SI]))
for k in sorted(st):
    if k not in ('ins', 'mame'): print('  %-14s %8d' % (k, st[k]))
K = {k: v[SI] for k, v in CONSTS.items()}
print('stores %d (%.0f%% stack) -> %.0f jtcps3 clocks; I-miss %d lines -> %.0f; D-miss ram %d / simm %d -> %.0f' % (
    stores, 100 * st['st_stack'] / max(stores, 1), stores * (1 + K['store']), st['imiss'], st['imiss'] * K['imiss'],
    st['dmiss_ram'], st['dmiss_simm'], st['dmiss_ram'] * K['dmiss_ram'] + st['dmiss_simm'] * K['dmiss_simm']))
print('\nfunction           instr   model  ratio  imiss  dmiss  stores(stack)  calls')
for f, c in sorted(per.items(), key=lambda x: -x[1]['cost'])[:45]:
    s = sum(v for k, v in c.items() if k.startswith('st_'))
    print('%-22s %7d %7.0f %5.2f %6d %6d %6d(%d) %6d' % (f[:22], c['ins'], c['cost'], c['cost'] / c['ins'], c['imiss'], c['dmiss'], s, c['st_stack'], c['entries']))

print('\nD-miss lines by symbol (lit: = literal pool of the running function)')
print('  literal pools total', lit)
for k, v in dm.most_common(25): print('  %-30s %6d' % (k, v))
print('\nstores by symbol')
for k, v in dsto.most_common(12): print('  %-30s %6d' % (k, v))

def group(f):
    if f.startswith('___'): return 'soft-float/libgcc'
    if any(k in f for k in ('collision', 'pgrid', 'pg_', 'ibounds', 'bbkind', 'pin_xy_int', 'pos_int', 'query_e', 'flush',
                             'mark_e', 'cupdate', 'ebbox', 'line_', 'point_', 'rect_', 'overlap', 'instance_place', 'isCollision',
                             'pcol', 'gsum', 'grid', 'search', 'rset', 'fam_', 'distance', 'precise', 'has_col', 'pw_rest', 'dwhole')): return 'collision'
    if f in ('_play_step', '_snapshot', '_animate', '_pw_draw_mark', '_view_update', '_ev_alarm', '_remove_marked', '_pw_release'): return 'step loop'
    if f in ('_ev_step', '_ev_end_step', '_ev_draw', '_ev_collision'): return 'dispatch'
    return 'object code / other'
G = collections.defaultdict(collections.Counter)
for f, c in per.items():
    g = G[group(f)]
    for k, v in c.items(): g[k] += v
print('\ngroup                    instr  share   model  share  ratio  imiss  dmiss  stack-st')
for g, c in sorted(G.items(), key=lambda x: -x[1]['cost']):
    print('%-22s %7d %5.1f%% %7.0f %5.1f%% %5.2f %6d %6d %6d' % (g, c['ins'], 100 * c['ins'] / ins, c['cost'], 100 * c['cost'] / cost,
          c['cost'] / c['ins'], c['imiss'], c['dmiss'], c['st_stack']))
print()
for s in (0, 1):
    print('JTCOST route=%s step=%s consts=%s ins=%d mame_cyc=%d model=%.0f ratio=%.2f stores=%d stack_st=%d imiss=%d '
          'dmiss_ram=%d dmiss_simm=%d lit=%d' % (opt['--route'], opt['--step'], SETS[s], ins, st['mame'], tot[s],
                                                tot[s] / max(ins, 1), stores, st['st_stack'], st['imiss'],
                                                st['dmiss_ram'], st['dmiss_simm'], lit))

print('\nsoft-float / libgcc calls by caller')
for (h, cf), v in callers.most_common(30): print('  %-16s <- %-24s %6d' % (h, cf, v))

if PCHIST:
    print('\n%s: executions by address (JTC_PCHIST)' % PCHIST)
    for a in sorted(pch): print('  %08x %7d' % (a, pch[a]))
