#!/usr/bin/env python3
"""The soft-float / libgcc helper call sites of an SH-2 build, by source line (docs/PERF3.md 1.3): reads
`sh-elf-objdump -dl` of an ELF built with -g (code generation is the same as without it). A call is a jsr @rN whose rN
was last loaded (in the same function) from a literal naming a helper (___adddf3, ___extendsfdf2, ...).
  tools/sfsites.py <objdump -dl output> [path part, default build/g/: the src/game snapshot]      -> per helper, per file, and the lines with the most sites"""
import sys, re, collections
dis = sys.argv[1]; only = sys.argv[2] if len(sys.argv) > 2 else 'build/g/'
HELP = re.compile(r'<(___(?:add|sub|mul|div|neg|eq|ne|lt|le|gt|ge|unord|extend|trunc|float|fix|fixuns)[a-z0-9]*)>')
fnre = re.compile(r'^[0-9a-f]+ <(_[^>]+)>:$')
linere = re.compile(r'^(/[^:]+|[^ \t:][^:]*):(\d+)')
ldre = re.compile(r'mov\.l\s+[0-9a-f]+ <[^>]*>,(r\d+)\s+!.*<(_[A-Za-z0-9_.]+)>')
mvre = re.compile(r'\smov\s+(r\d+),(r\d+)\s*$')
jsrre = re.compile(r'\sjsr\s+@(r\d+)')
regs = {}; cur = ('?', 0); func = '?'
fnl = {}; by_help = collections.Counter(); by_file = collections.Counter(); by_line = collections.Counter(); by_func = collections.Counter()
for l in open(dis):
    m = fnre.match(l)
    if m: func = m.group(1); regs = {}; continue
    m = linere.match(l)
    if m and not l.startswith(' '):
        cur = (m.group(1), int(m.group(2))); continue
    m = ldre.search(l)
    if m:
        if HELP.search(l): regs[m.group(1)] = HELP.search(l).group(1)
        else: regs.pop(m.group(1), None)
        continue
    m = mvre.search(l)
    if m:
        if m.group(1) in regs: regs[m.group(2)] = regs[m.group(1)]
        else: regs.pop(m.group(2), None)
        continue
    m = jsrre.search(l)
    if m and m.group(1) in regs:
        f = cur[0]
        if only not in f: continue
        f = f[f.index(only):]
        h = regs[m.group(1)]
        by_help[h] += 1; by_file[f] += 1; by_line[(f, cur[1], h)] += 1; by_func[(func, h)] += 1; fnl.setdefault(func, set()).add((f, cur[1], h))
print('soft-float call sites in %s: %d' % (only, sum(by_help.values())))
print('\nby helper'); [print('  %-18s %5d' % kv) for kv in by_help.most_common()]
print('\nby file'); [print('  %-26s %5d' % kv) for kv in by_file.most_common()]
import os
FN = os.getenv('SF_FUNC')
if FN:
    print('\n%s: sites by line' % FN)
    for (f, ln, h), v in sorted(by_line.items(), key=lambda x: (x[0][0], x[0][1])):
        if (f, ln, h) in fnl.get(FN, set()): print('  %s:%d %-16s %d' % (f, ln, h, v))
print('\nby function and helper (top 40)'); [print('  %-28s %-16s %4d' % (f, h, v)) for (f, h), v in by_func.most_common(40)]
