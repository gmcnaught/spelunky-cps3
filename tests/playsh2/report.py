#!/usr/bin/env python3
"""tests/playsh2 report: the SH-2 records (scripts/lua/playsh2.lua) against the host's (host.c): checksums equal per
record; SH-2 clocks per generation (by area) and per route step (level start, the first step, mean, max); with a
.prof file, the PC samples summed per function and per source file, libgcc's soft-float helpers also per caller.
   report.py <host.txt> <sh2.txt> <nm.txt>"""
import sys, os, re, glob, bisect, collections

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
STEP_BUDGET = 2 * 419470            # a game step = 2 frames of 419,470 clocks (25 MHz / 59.6 Hz; PLAN.md section 1)
JT = 4.2                           # jtcps3 clocks per MAME clock: estimate; jtcost is the metric (scripts/jtcost.sh, docs/PERF3.md; 4.21 = EQUIV.md's grid build on jtcps3)
AREA = lambda lv: 'mines' if lv <= 4 else 'lush' if lv <= 8 else 'ice' if lv <= 12 else 'temple' if lv <= 15 else 'olmec'
KIND = {1: 'gen', 2: 'start', 3: 'step', 4: 'early'}


def load(path):
    recs, head = collections.OrderedDict(), None
    for line in open(path):
        f = line.split()
        if f[0] == 'T': head = f
        if f[0] != 'R': continue
        key = (int(f[1]), int(f[2]), int(f[3]))
        recs[key] = dict(sum=f[4], n=int(f[5]), extra=int(f[6]), clk=int(f[7]) if len(f) > 7 else None)
    return recs, head


def jobs():
    """the job table from build/jobs.h, or $PSH2_JOBS (scripts/playsh2_check.sh ROUTES=: build/<VARIANT>/jobs/jobs.h)"""
    t = open(os.environ.get('PSH2_JOBS') or os.path.join(os.path.dirname(os.path.abspath(__file__)), 'build', 'jobs.h')).read()
    names = re.findall(r'\{ "(\w+)", m_', t)
    js = [(int(s), int(l), int(r)) for s, l, r in re.findall(r'\{ (\d+)u, (-?\d+), (-?\d+) \}', t)]
    return js, names


def stats(v):
    return (sum(v) / len(v), max(v), min(v)) if v else (0, 0, 0)


def src_map():
    """function name -> source file (definitions in src/game, the test and build/gen)"""
    m = {}
    files = glob.glob(os.path.join(ROOT, 'src/game/*.c')) + glob.glob(os.path.join(ROOT, 'tests/playsh2/*.c'))
    for p in files:
        for mm in re.finditer(r'^[A-Za-z_][\w \*]*?\b(\w+)\s*\([^;{]*\)\s*\{', open(p).read(), re.M):
            m.setdefault(mm.group(1), os.path.basename(p))
    return m


def main():
    a = sys.argv[1:]
    host, _ = load(a[0])
    sh2, head = load(a[1])
    nm = a[2] if len(a) > 2 else None
    js, rnames = jobs()
    # 1. checksums
    miss = [k for k in host if k not in sh2]
    extra = [k for k in sh2 if k not in host]
    diff = [k for k in host if k in sh2 and (host[k]['sum'] != sh2[k]['sum'] or host[k]['n'] != sh2[k]['n'])]
    print('records: host %d, SH-2 %d; checksums equal %d, differ %d, missing on SH-2 %d, extra %d'
          % (len(host), len(sh2), len(host) - len(diff) - len(miss), len(diff), len(miss), len(extra)))
    for k in diff[:5]: print('  first differences: job %d %s %d host %s SH-2 %s' % (k[0], KIND[k[1]], k[2], host[k]['sum'], sh2[k]['sum']))
    if head: print('MAME emulated time %.1f s; PROF period %s ticks' % (float(head[1]), head[4]))
    # 2. generation, per area
    print('\nLevel generation (gen_level), SH-2 clocks in MAME (x%.2f for jtcps3: estimate; jtcost is the metric):' % JT)
    by = collections.OrderedDict()
    for (j, kd, i), r in sh2.items():
        if kd == 1:
            by.setdefault(AREA(js[j][1]), []).append((r['clk'], js[j][1], js[j][0], r['n']))
    print('  %-7s %5s %12s %12s %12s %8s' % ('area', 'cases', 'mean', 'max', 'min', 'inst'))
    for ar, v in by.items():
        m, mx, mn = stats([x[0] for x in v])
        print('  %-7s %5d %12.0f %12d %12d %8d   (%.1f / %.1f ms at 25 MHz MAME / jtcps3, max)'
              % (ar, len(v), m, mx, mn, max(x[3] for x in v), mx / 25e3, mx * JT / 25e3))
    # 3. routes
    print('\nRoutes (play_level_start, then play_step per frame), SH-2 clocks in MAME; budget %d a step (30 Hz)' % STEP_BUDGET)
    print('  %-15s %6s %10s %10s %10s %10s %8s %7s %7s %6s' % ('route', 'steps', 'start', 'step1', 'mean', 'max',
                                                              'max@', 'over', 'overJT', 'inst'))
    allsteps = []
    for j, (s, lv, r) in enumerate(js):
        if r < 0: continue
        recs = [(k[2], k[1], v) for k, v in sh2.items() if k[0] == j]
        recs.sort()
        start = [v['clk'] for i, kd, v in recs if kd == 2]
        st = [(v['clk'], i, v) for i, kd, v in recs if kd in (3, 4)]
        if not st: continue
        c = [x[0] for x in st]
        allsteps += c
        m, mx, mn = stats(c)
        at = max(st)[1]
        # later level starts / transitions inside play_step: the steps over 2x the mean, listed
        print('  %-15s %6d %10d %10d %10.0f %10d %8d %7d %7d %6d' % (rnames[r], len(c), start[0] if start else -1,
              c[0], m, mx, at, sum(1 for x in c if x > STEP_BUDGET), sum(1 for x in c if x * JT > STEP_BUDGET),
              max(v['n'] for _, _, v in recs)))
        big = sorted(st, reverse=True)[:4]
        print('  %15s largest steps: %s' % ('', ', '.join('%d@%d' % (x[0], x[1]) for x in big)))
    if allsteps:
        s = sorted(allsteps)
        p = lambda q: s[min(len(s) - 1, int(q * len(s)))]
        print('  all route steps: %d, mean %.0f (%.1f%% of a step in MAME, %.1f%% x%.2f estimate; jtcost is the metric), median %d, p99 %d, max %d'
              % (len(s), sum(s) / len(s), 100 * sum(s) / len(s) / STEP_BUDGET, 100 * JT * sum(s) / len(s) / STEP_BUDGET,
                 JT, p(0.5), p(0.99), s[-1]))
    # 4. ATTR: time by category / event x object over the route steps
    attr = a[1] + '.attr'
    if os.path.exists(attr):
        attribution(attr, sh2, js)
    # 5. profile
    prof = a[1] + '.prof'
    if nm and os.path.exists(prof):
        profile(prof, nm, head, sys.argv[-1] == '--wrap')


def attribution(path, sh2, js):
    hdr = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'build', 'g', 'objects.h')).read()
    names = re.findall(r'^    OBJ_(\w+),', hdr, re.M)
    steps = [(k, v) for k, v in sh2.items() if k[1] in (3, 4) and js[k[0]][2] >= 0]
    tot = sum(v['clk'] for _, v in steps)
    coll = sum(v['extra'] for _, v in steps)
    nonc = sorted(v['clk'] - v['extra'] for _, v in steps)
    first = [v['clk'] - v['extra'] for k, v in steps if k[2] == 1]
    n = len(nonc)
    if n == 0:
        print('\nATTR: no route steps'); return
    print('\nATTR: route steps %d, %.0f clocks a step (wrappers included); collision searches %.0f (%.1f%%)'
          % (n, tot / n, coll / n, 100.0 * coll / tot))
    print('  outside collision searches, a step: mean %.0f, median %d, p90 %d, max %d; first steps of levels: %s'
          % (sum(nonc) / n, nonc[n // 2], nonc[int(n * 0.9)], nonc[-1], ', '.join(str(x) for x in first)))
    cats, ev, kr = {}, [], []
    for line in open(path):
        f = line.split()
        if f[0] == 'C': cats[int(f[1])] = (float(f[2]), int(f[3]))
        if f[0] == 'E': ev.append((float(f[3]), int(f[1]), int(f[2]), int(f[4])))
        if f[0] == 'K': kr.append((float(f[2]), int(f[1]), int(f[3])))
    TYPES = ['Create', 'Destroy', 'Step', 'End Step', 'Alarm', 'Animation End', 'Collision', 'Draw']
    nc = tot - coll
    print('  per step (exclusive):  %-28s %10s %8s %9s' % ('', 'clocks', '% non-c', 'calls'))
    for c, nm in ((0, 'dispatch (play_step loops)'), (2, 'collision-event pass (pcol)'), (3, 'instance searches (exists/with)')):
        print('    %-36s %10.0f %7.1f%% %9.1f' % (nm, cats[c][0] / n, 100.0 * cats[c][0] / nc, cats[c][1] / n))
    print('    %-36s %10.0f %8s %9.1f' % ('collision searches', cats[1][0] / n, '', cats[1][1] / n))
    bytype = collections.Counter()
    for clk, t, o, calls in ev: bytype[t] += clk
    for t, clk in bytype.most_common():
        cn = sum(c for _, tt, _, c in ev if tt == t)
        print('    %-36s %10.0f %7.1f%% %9.1f' % ('events: ' + TYPES[t], clk / n, 100.0 * clk / nc, cn / n))
    print('  events by object (exclusive, collision searches excluded), largest:')
    for clk, t, o, calls in sorted(ev, reverse=True)[:20]:
        print('    %-16s %-14s %10.0f a step %6.1f%%  %8.1f calls a step  %7.0f a call'
              % (names[o] if o < len(names) else o, TYPES[t], clk / n, 100.0 * clk / nc, calls / n, clk / calls))
    if kr:
        CN = {0: 'dispatch (no event)', 2: 'collision-event pass', 3: 'instance searches'}
        print('  collision searches by calling object (the innermost enclosing event\'s object), largest:')
        for clk, r, calls in sorted(kr, reverse=True)[:20]:
            nm = CN.get(r, '?') if r < 4 else (names[r - 4] if r - 4 < len(names) else str(r - 4))
            print('    %-30s %10.0f a step %6.1f%%  %8.1f calls a step  %7.0f a call'
                  % (nm, clk / n, 100.0 * clk / max(cats[1][0], 1), calls / n, clk / calls))


COLL = re.compile(r'^(collision_\w+|instance_place_p|instance_nearest_p|pin_bbox|point_hit|line_hit|rect_hit|seg_box|'
                  r'mask_at|precise|match|pin_overlap|calcBounds|isCollision\w*|getIdCollision\w*|has_col|pcol_handle|'
                  r'distance_to_\w+|inview|approximatelyZero)$')


def group(f, smap):
    src = smap.get(f)
    if src is None: return 'libgcc'
    if src == 'inst.c': return 'generator instance searches (inst.c)'
    if src in ('gen.c', 'genroom.c', 'genobj.c', 'genent.c', 'rng.c'): return 'generator scripts'
    if COLL.match(f) or src in ('pworld.c', 'pcol.c'): return 'collision'
    if src == 'prun.c': return 'dispatch (prun.c)'
    return 'events (%s)' % src


def profile(prof, nm, head, wrap_arg):
    syms = []
    for line in open(nm):
        f = line.split()
        if len(f) == 3 and f[1] in 'tTwW': syms.append((int(f[0], 16), f[2]))
    syms.sort()
    addrs = [x[0] for x in syms]

    def fn(pc):
        k = bisect.bisect_right(addrs, pc) - 1
        n = syms[k][1] if k >= 0 else '?'
        n = n[1:] if n.startswith('_') else n           # sh-elf's leading underscore
        n = re.sub(r'\.(part|isra|constprop|cold)\.\d+.*$', '', n)  # GCC's clones
        return re.sub(r'^__(wrap|real)_', '', n)
    smap = src_map()
    kind = int(head[5]) if head and len(head) > 5 else 3
    wrap = (int(head[6]) if head and len(head) > 6 else 0) or wrap_arg
    S = []
    for line in open(prof):
        f = line.split()
        S.append((int(f[1], 16) + 8, int(f[2], 16) + 8))   # bucket middles
    if not S: return
    tot = len(S)
    pct = lambda n: 100.0 * n / tot
    self_ = collections.Counter(fn(pc) for pc, _ in S)
    print('\nPC samples, %s (%d samples; self time per function):' % ({3: 'route steps and level starts', 1: 'generation'}[kind], tot))
    for f, n in self_.most_common(22):
        print('  %6.2f%%  %-28s %s' % (pct(n), f, smap.get(f, 'libgcc')))
    g = collections.Counter()
    for f, n in self_.items(): g[group(f, smap)] += n
    print('  self time by group: ' + ', '.join('%s %.1f%%' % (c, pct(n)) for c, n in g.most_common()))
    if not wrap:
        print('  (PR-based callers are not meaningful for fp-bit: build with PROF_WRAP=1 for soft-float callers)')
        return
    # soft-float samples charged to the game function that called the helper (wrap.S)
    caller = collections.Counter()
    charged = collections.Counter()
    for pc, x in S:
        f = fn(pc)
        if smap.get(f) is None:
            c = fn(x)
            caller[c] += 1
            charged[group(c, smap)] += 1
        else:
            charged[group(f, smap)] += 1
    print('  soft-float (libgcc) time by the game function that called it:')
    for c, n in caller.most_common(18):
        print('    %6.2f%%  %-28s %s' % (pct(n), c, smap.get(c, '?')))
    print('  groups with their soft-float calls charged to them: ' +
          ', '.join('%s %.1f%%' % (c, pct(n)) for c, n in charged.most_common()))
    sf = sum(caller.values())
    print('  soft-float in all: %.1f%% of the samples' % pct(sf))


main()
