#!/usr/bin/env python3
"""tools/sgfix_at.py <rule-id> [--apply]: rewrite the matches of tools/sg/rules/cp-at.yml, rect-at.yml or place-at.yml
(and their -p forms: PTOD(p->x) with p = &PX(i) in the enclosing function, see resolve_p)
to the _at forms (collision_point_any_at, collision_rect_at / collision_rect_any_at, instance_place_at), the
instance's x / y read once by the callee on ints. Prints each site and its rewrite; --apply edits the files.
Every argument is checked again here: the coordinates must be X(i) / Y(i) or PTOD(PX(i).x / .y) of one instance
plus or minus an int offset of the rule's forms, the instance the same in each."""
import json, re, subprocess, sys

def split_args(s):
    out, d, cur = [], 0, ''
    for ch in s:
        if ch in '([': d += 1
        elif ch in ')]': d -= 1
        if ch == ',' and d == 0: out.append(cur.strip()); cur = ''
        else: cur += ch
    out.append(cur.strip())
    return out

OFF = r'(\d+|\(spr[wh]\(\w+\) >> 1\)|\(giant \? \d+ : \d+\))'
def coord(a, axis):
    m = re.fullmatch(r'(?:%s\((\w+)\)|PTOD\(PX\((\w+)\)\.%s\))(?:\s*([+-])\s*%s)?' % ('X' if axis == 'x' else 'Y', axis, OFF), a)
    if not m: return None
    i = m.group(1) or m.group(2)
    if not m.group(3): return i, '0'
    return i, ('-' + m.group(4)) if m.group(3) == '-' else m.group(4)

PIN = {}      # (file, P) at a match: the instance P points to (resolve_p)
def resolve_p(path, off, src):
    """the enclosing function (column-0 braces) of byte offset off: its one `struct pin *P = &PX(i);` for each P, when
    neither P nor i is assigned again in it (i a parameter or local); {P: i}"""
    b = src.rfind(b'\n{\n', 0, off); e = src.find(b'\n}\n', off)
    if b < 0 or e < 0: return {}
    body = src[b:e].decode()
    out = {}
    for m in re.finditer(r'struct pin \*(\w+) = &PX\((\w+)\);', body):
        P, i = m.group(1), m.group(2)
        if len(re.findall(r'(?<![\w>.])%s\s*=(?!=)' % P, body)) != 1: continue
        if re.search(r'(?<![\w>.])%s\s*([-+*/]?=(?!=)|\+\+|--)' % i, body): continue
        out[P] = i
    return out

def subst_p(t, pins):
    def f(m):
        P = m.group(1)
        if P not in pins: raise KeyError(P)
        return 'PTOD(PX(%s).%s)' % (pins[P], m.group(2))
    return re.sub(r'PTOD\((\w+)->([xy])\)', f, t)

def rewrite(t):
    t1 = ' '.join(t.split())
    m = re.fullmatch(r'(\w+)\((.*)\)(?:\s*([!=]=)\s*NOONE)?', t1)
    if not m: return None
    name, args, cmp = m.group(1), split_args(m.group(2)), m.group(3)
    neg = '!' if cmp == '==' else ''
    if name in ('CP', 'CPn', 'collision_point_any', 'collision_point_p'):
        if name == 'collision_point_p' and not cmp: return None
        if name in ('collision_point_any', 'collision_point_p') and args[3:] != ['0', 'NOONE']: return None
        a, b = coord(args[0], 'x'), coord(args[1], 'y')
        if not a or not b or a[0] != b[0]: return None
        return '%scollision_point_any_at(%s, %s, %s, %s)' % (neg, a[0], a[1], b[1], args[2])
    if name in ('collision_rect_p', 'collision_rect_any'):
        if args[5:] != ['0', 'NOONE']: return None
        c = [coord(args[0], 'x'), coord(args[1], 'y'), coord(args[2], 'x'), coord(args[3], 'y')]
        if None in c or len(set(v[0] for v in c)) != 1: return None
        if name == 'collision_rect_any':
            return '%scollision_rect_any_at(%s, %s, %s, %s, %s, %s)' % (neg, c[0][0], c[0][1], c[1][1], c[2][1], c[3][1], args[4])
        f = 'collision_rect_at(%s, %s, %s, %s, %s, %s)' % (c[0][0], c[0][1], c[1][1], c[2][1], c[3][1], args[4])
        return f + (' %s NOONE' % cmp if cmp else '')
    if name in ('instance_place_p', 'place_meeting_p'):
        a, b = coord(args[1], 'x'), coord(args[2], 'y')
        if not a or not b or a[0] != b[0] or a[0] != args[0]: return None
        f = 'instance_place_at(%s, %s, %s, %s)' % (args[0], a[1], b[1], args[3])
        if name == 'place_meeting_p': f = '(%s != NOONE)' % f
        return f + (' %s NOONE' % cmp if cmp else '')
    return None

rule = sys.argv[1]
js = subprocess.run(['ast-grep', 'scan', '--filter', '^%s$' % rule, '--json=stream'], capture_output=True, text=True).stdout
edits = {}
for l in js.splitlines():
    r = json.loads(l)
    text = r['text']
    if rule.endswith('-p'):
        src = open(r['file'], 'rb').read()
        try: text = subst_p(text, resolve_p(r['file'], r['range']['byteOffset']['start'], src))
        except KeyError: text = None
    new = rewrite(text) if text else None
    loc = '%s:%d' % (r['file'], r['range']['start']['line'] + 1)
    print('%-28s %s\n%-28s -> %s' % (loc, ' '.join(r['text'].split()), '', new or 'SKIPPED'))
    if new: edits.setdefault(r['file'], []).append((r['range']['byteOffset']['start'], r['range']['byteOffset']['end'], new))
if '--apply' in sys.argv:
    for f, es in edits.items():
        b = open(f, 'rb').read()
        for s, e, n in sorted(es, reverse=True):
            b = b[:s] + n.encode() + b[e:]
        open(f, 'wb').write(b)
    print('applied', sum(len(v) for v in edits.values()), 'in', len(edits), 'files')
