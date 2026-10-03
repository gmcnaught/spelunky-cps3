#!/usr/bin/env python3
"""Flatten gmk-splitter output (build/gmk) into one readable GML file per object event (build/gml/<obj>/<event>.gml)
and report the non-code (drag-and-drop) actions, which the port must translate by hand.

usage: tools/gmk2gml.py build/gmk build/gml"""
import sys, pathlib, collections
import xml.etree.ElementTree as ET

src, dst = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
dnd = collections.Counter()
lines = 0
for ox in sorted(src.glob('Objects/**/*.xml')):
    if ox.name.startswith('_') or '.events' in str(ox.parent):
        continue
    obj = ox.stem
    root = ET.parse(ox).getroot()
    hdr = {k: (root.findtext(k) or '') for k in ('sprite', 'solid', 'visible', 'depth', 'persistent', 'parent', 'mask')}
    od = dst / obj
    od.mkdir(parents=True, exist_ok=True)
    (od / '_object.txt').write_text(''.join(f'{k}: {v}\n' for k, v in hdr.items()))
    evdir = ox.with_suffix('.events')
    for ev in sorted(evdir.glob('*.xml')) if evdir.exists() else []:
        er = ET.parse(ev).getroot()
        out = []
        for a in er.iter('action'):
            fn = a.findtext('functionName') or ''
            kind = a.findtext('kind')
            applies = a.findtext('appliesTo')
            args = [x.text or '' for x in a.iter('argument')]
            if kind == 'CODE':
                if applies != '.self':
                    out.append(f'// applies to: {applies}')
                out.append(args[0] if args else '')
            else:
                name = fn or kind
                dnd[name] += 1
                out.append(f'// DND {name}({", ".join(repr(x) for x in args)}) applies={applies} '
                           f'relative={a.findtext("relative")} not={a.findtext("not")}')
        text = '\n'.join(out) + '\n'
        lines += text.count('\n')
        (od / (ev.stem + '.gml')).write_text(text)
for sc in sorted(src.glob('Scripts/**/*.gml')):
    t = sc.read_text(errors='replace')
    lines += t.count('\n')
    (dst / '_scripts').mkdir(parents=True, exist_ok=True)
    (dst / '_scripts' / sc.name).write_text(t)
print(f'lines {lines}')
for k, v in dnd.most_common():
    print(f'DND {v:5d} {k}')
