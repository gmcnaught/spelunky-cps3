#!/usr/bin/env python3
"""Repeated queries: the same collision / distance / nearest call with the same arguments (whitespace aside) two or
more times in one function (tools/sg/util/expensive-call.yml, ast-grep). A repeat is a candidate only where nothing
the query reads changes between the calls (position, sprite, the solid grid, the family's instances), and many are
in exclusive branches: read each one. PERF3 measured a blanket per-step memo of isCollision* as not worth it
(8-11 % repeats); single hot sites can be (docs/AST-GREP.md).
    tools/sgdup.py [paths...]      default: src/game src/front"""
import json, re, subprocess, sys, os, collections
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
paths = sys.argv[1:] or ['src/game', 'src/front']
def scan(rule):
    out = subprocess.run(['ast-grep', 'scan', '-r', os.path.join(root, 'tools/sg/util', rule), '--json=compact'] + paths,
                         cwd=root, capture_output=True, text=True, check=True).stdout
    return json.loads(out)
funcs = collections.defaultdict(list)                  # file -> [(start, end, name)]
for m in scan('function.yml'):
    n = re.search(r'(\w+)\s*\(', m['text'].split('{', 1)[0])
    funcs[m['file']].append((m['range']['start']['line'], m['range']['end']['line'], n.group(1) if n else '?'))
groups = collections.defaultdict(list)
for m in scan('expensive-call.yml'):
    ln = m['range']['start']['line']
    fn = next((n for s, e, n in funcs[m['file']] if s <= ln <= e), '?')
    groups[(m['file'], fn, re.sub(r'\s+', '', m['text']))].append(ln + 1)
rep = sorted(((k, v) for k, v in groups.items() if len(v) > 1), key=lambda kv: (-len(kv[1]), kv[0]))
print('%d repeated (function, query) groups, %d calls' % (len(rep), sum(len(v) for _, v in rep)))
for (f, fn, t), v in rep:
    print('%2dx %s:%s %s  lines %s' % (len(v), f, fn, t if len(t) < 90 else t[:87] + '...', ','.join(map(str, v))))
