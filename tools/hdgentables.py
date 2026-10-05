#!/usr/bin/env python3
"""Tables for the level generator (src/game/gen*.c), from HD 1.2.2's data file and GML.

    tools/hdgentables.py <hd src dir> <game.unx> <out gentables.h>      (also writes gentables.c next to it)

1. Sprite collision data, from the runtime's data file (game.unx, read by UndertaleModTool's CLI in Docker, as
   tools/tracer.py does): per sprite in the data file's order (= the runtime's sprite_index), the origin, the
   bounding box (margins, inclusive) and the separation-mask kind (0 rectangle, 1 precise, 2 rotated). enum gspr:
   GSPR_<name> = the runtime sprite index. Precise masks are not exported: every collision function the generator
   calls passes prec = 0 (bounding boxes only), see src/game/inst.c.
2. Room templates of scrRoomGen (the mines): the string literals of each `switch` block, in source order, taken
   from the GML lines given in ROOMGEN below (the control flow that picks them is hand-translated in
   src/game/genroom.c with the same line references).
3. Every string literal of the other room generators (scrRoomGen2..5, scrRoomGenMarket, scrRoomGenYeti) by GML
   line: GT(<script>, <line>, <k>) is the k-th literal on that line (gentmpl()). The C code names the line it
   translates, so each template is checked against the GML by its line number.
4. Room instances (rLevel, rLevel2, rLevel3, rOlmec) in the data file's creation order with the runtime ids, and
   GROOM_MAXID, the largest room instance id of the game (ids above it are created at run time).
5. gobjspr[OBJ_COUNT]: each object's default sprite_index (GSPR_*, -1 none), in build/gen/objects.h's object
   order (tools/hdobjects.py: the project's objects sorted by name).
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import titlelogo  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
UTMT = os.path.normpath(os.path.join(HERE, '..', '..', '_tools', 'UndertaleModTool', 'out-cli', 'UndertaleModCli.dll'))
DOTNET = 'mcr.microsoft.com/dotnet/sdk:10.0'

# name, first and last GML line (scripts/scrRoomGen/scrRoomGen.gml), strings per entry
ROOMGEN = [
    ('mines_start', 60, 68, 1),      # start room, cases 1..8
    ('mines_end', 77, 83, 1),        # end room, cases 1..6
    ('mines_side', 107, 120, 1),     # side room, cases 1..11
    ('mines_main', 129, 150, 1),     # path 0/1 main room, cases 1..12 (8 and 12: two strings each)
    ('mines_main3', 159, 171, 1),    # path 3, cases 1..8 (8: two strings)
    ('mines_shop', 176, 203, 1),     # shop: 176 base, 186 craps, 187 kissing (path 4); 192, 202, 203 (path 5)
    ('mines_snake', 210, 217, 1),    # snake pit middle (210), bottom (217)
    ('mines_drop', 227, 238, 1),     # drop, cases 1..12
    ('mines_obs8', 256, 263, 3),     # obstacle "8", cases 1..8 (strObs1..3)
    ('mines_obs5', 270, 285, 3),     # obstacle "5", cases 1..16
    ('mines_obs6', 292, 301, 3),     # obstacle "6", cases 1..10
]

TMPL = ['scrRoomGen2', 'scrRoomGen3', 'scrRoomGen4', 'scrRoomGen5', 'scrRoomGenMarket', 'scrRoomGenYeti']

CSX = r'''
using System.IO;
using System.Text;
var sb = new StringBuilder();
foreach (var s in Data.Sprites)
    sb.Append($"{s.Name.Content} {s.OriginX} {s.OriginY} {s.MarginLeft} {s.MarginTop} {s.MarginRight} {s.MarginBottom} {(int)s.SepMasks} {s.CollisionMasks.Count}\n");
uint maxid = 0;
foreach (var r in Data.Rooms) foreach (var o in r.GameObjects) if (o.InstanceID > maxid) maxid = o.InstanceID;
sb.Append($"MAXID {maxid}\n");
foreach (var name in new[]{"rLevel","rLevel2","rLevel3","rOlmec"}) {
    var r = Data.Rooms.ByName(name);
    sb.Append($"ROOM {name} {r.Width} {r.Height}\n");
    var depth = new System.Collections.Generic.Dictionary<uint, int>();
    foreach (var L in r.Layers)
        if (L.InstancesData != null)
            foreach (var o in L.InstancesData.Instances) depth[o.InstanceID] = L.LayerDepth;
    foreach (var o in r.GameObjects)
        sb.Append($"INST {o.InstanceID} {o.ObjectDefinition.Name.Content} {o.X} {o.Y} {o.ScaleX} {o.ScaleY} {(o.CreationCode == null ? 0 : 1)} {(o.PreCreateCode == null ? 0 : 1)} {depth[o.InstanceID]}\n");
}
File.WriteAllText("OUT", sb.ToString());
'''


def sprites(unx, work):
    unx, work = os.path.abspath(unx), os.path.abspath(work)
    root = os.path.commonpath([unx, UTMT, work])
    rel = lambda p: '/w/' + os.path.relpath(p, root)
    txt = os.path.join(work, 'gentables_sprites.txt')
    csx = os.path.join(work, 'gentables_sprites.csx')
    open(csx, 'w').write(CSX.replace('OUT', rel(txt)))
    r = subprocess.run(['docker', 'run', '--rm', '-v', f'{root}:/w', DOTNET, 'dotnet', rel(UTMT), 'load', rel(unx),
                        '-s', rel(csx)], capture_output=True, text=True)
    if not os.path.exists(txt):
        sys.exit(r.stdout[-3000:] + r.stderr[-3000:])
    out, rooms, maxid, cur = [], {}, 0, None
    for line in open(txt):
        f = line.split()
        if f[0] == 'MAXID':
            maxid = int(f[1])
        elif f[0] == 'ROOM':
            cur = rooms[f[1]] = dict(w=int(f[2]), h=int(f[3]), insts=[])
        elif f[0] == 'INST':
            if f[6] != '1' or f[5] != '1' or f[7] != '0' or f[8] != '0':
                sys.exit(f'room instance with scale or creation code: {line}')
            cur['insts'].append((int(f[1]), f[2], int(f[3]), int(f[4]), int(f[9])))
        else:
            out.append((f[0], *map(int, f[1:])))
    return out, rooms, maxid


def roomgen(src):
    lines = open(os.path.join(src, 'scripts', 'scrRoomGen', 'scrRoomGen.gml')).read().split('\n')
    tabs = []
    for name, a, b, per in ROOMGEN:
        strs = []
        for ln in range(a, b + 1):
            for m in re.finditer(r'"([^"]*)"', lines[ln - 1]):
                s = m.group(1)
                if len(s) == (80 if per == 1 else 5):
                    strs.append((ln, s))
        tabs.append((name, per, strs))
    return tabs


SHOPNAMES = {'General', 'Bomb', 'Weapon', 'Rare', 'Clothing', 'Craps', 'Kissing', 'Ankh'}


def switch_cases(lines):
    """[((switch line, case value), [literals])] for every `switch` of a script (1-based lines); literals are the
    template strings in the case body (shop-type names left out)"""
    out = []
    for i, text in enumerate(lines):
        if not re.search(r'\bswitch\s*\(', text.split('//')[0]):
            continue
        depth, started, cur = 0, False, None
        for j in range(i, len(lines)):
            t = lines[j].split('//')[0]
            if started and depth == 1:
                m = re.match(r'\s*case\s+(-?\d+)\s*:', t)
                if m:
                    cur = ((i + 1, int(m.group(1))), [])
                    out.append(cur)
            if cur is not None and (depth >= 1 or not started):
                for lit in re.findall(r'"([^"]*)"', t):
                    if re.fullmatch(r'[0-9A-Za-z.,:;$!?+]{1,80}', lit) and lit not in SHOPNAMES:
                        cur[1].append(lit)
            for ch in t:
                if ch == '{':
                    depth += 1
                    started = True
                elif ch == '}':
                    depth -= 1
            if started and depth == 0:
                break
    return out


def main():
    src, unx, out = sys.argv[1:4]
    os.makedirs(os.path.dirname(out) or '.', exist_ok=True)
    spr, rooms, maxid = sprites(unx, os.path.dirname(out) or '.')
    # the port's title logo (tools/titlelogo.py) replaces sTitle_HD's art: its box is the new art's, as sprites.c's
    # (HD's manual box, columns 14..192, reaches the shaft's x 592..608 once tools/fronttables.py moves the logo by DX)
    im = titlelogo.build(src)
    spr = [(n, xo, yo, 0, 0, im.width - 1, im.height - 1, kind, nm) if n == titlelogo.NAME else
           (n, xo, yo, l, t, r, b, kind, nm) for n, xo, yo, l, t, r, b, kind, nm in spr]
    hdr = '/* generated by tools/hdgentables.py from HD 1.2.2 (game.unx, scrRoomGen.gml): do not edit */'
    h = [hdr, '#ifndef GENTABLES_H', '#define GENTABLES_H', '#include <stdint.h>', '', 'enum gspr {']
    h += [f'    GSPR_{s[0]},' for s in spr]
    h += ['    GSPR_COUNT', '};', '',
          '/* origin, bounding box (inclusive margins), mask kind: 0 rectangle, 1 precise, 2 rotated */',
          'struct gsprcol { int8_t xo, yo; int16_t l, t, r, b; uint8_t kind; };',
          'extern const struct gsprcol gsprcol[GSPR_COUNT];', 'extern const char *const gsprname[GSPR_COUNT];', '']
    c = [hdr, '#include "gentables.h"', '#include "objects.h"', '', 'const struct gsprcol gsprcol[GSPR_COUNT] = {']
    for s in spr:
        name, xo, yo, l, t, r, b, kind, nm = s
        c.append(f'    {{ {xo}, {yo}, {l}, {t}, {r}, {b}, {kind} }},  /* {name} */')
    c += ['};', '', 'const char *const gsprname[GSPR_COUNT] = {']
    c += [f'    "{s[0]}",' for s in spr]
    c += ['};', '']
    for name, per, strs in roomgen(src):
        n = len(strs)
        h.append(f'/* scrRoomGen.gml lines {strs[0][0]}-{strs[-1][0]}: {n} strings */')
        h.append(f'extern const char *const {name}[{n}];')
        c.append(f'const char *const {name}[{n}] = {{')
        c += [f'    "{s}",  /* :{ln} */' for ln, s in strs]
        c += ['};', '']
    # other room generators: string literals by line
    h.append('struct gtmpl { int16_t first, n; const char *const *const *lines; };')
    h.append('#define GT(t, line, k) ((t).lines[(line) - (t).first][k])')
    h.append('/* switch cases: GSW(t, line of the switch, case value, k): the k-th template literal of that case */')
    h.append('struct gswcase { int16_t line, value, n; const char *lits[4]; };')
    h.append('struct gswtab { int16_t n; const struct gswcase *c; };')
    h.append('const char *gsw(const struct gswtab *t, int line, int value, int k);')
    h.append('#define GSW(t, line, v, k) gsw(&(t##_cases), (line), (v), (k))')
    for sc in TMPL:
        lines = open(os.path.join(src, 'scripts', sc, sc + '.gml')).read().split('\n')
        name = 'gt_' + sc
        rows = []
        for ln, text in enumerate(lines, 1):
            lits = re.findall(r'"([^"]*)"', text.split('//')[0])
            lits = [x for x in lits if re.fullmatch(r'[0-9A-Za-z.,:;$!?+]{1,80}', x)]
            rows.append((ln, lits))
        c.append(f'/* scripts/{sc}/{sc}.gml */')
        for ln, lits in rows:
            if lits:
                c.append(f'static const char *const {name}_{ln}[] = {{ ' + ', '.join(f'"{x}"' for x in lits) + ' };')
        c.append(f'static const char *const *const {name}_lines[{len(rows)}] = {{')
        c += [f'    {name + "_" + str(ln) if lits else "0"},' for ln, lits in rows]
        c += ['};', f'const struct gtmpl {name} = {{ 1, {len(rows)}, {name}_lines }};', '']
        h.append(f'extern const struct gtmpl {name};')
        # switch cases: (switch line, case value) -> the template literals of that case's body, in order
        sw = switch_cases(lines)
        c.append(f'static const struct gswcase {name}_sw[{len(sw)}] = {{')
        for (ln, val), lits in sw:
            arr = '{ ' + ', '.join(f'"{x}"' for x in lits[:4]) + ' }'
            c.append(f'    {{ {ln}, {val}, {len(lits)}, {arr} }},')
        c += ['};', f'const struct gswtab {name}_cases = {{ {len(sw)}, {name}_sw }};', '']
        h.append(f'extern const struct gswtab {name}_cases;')
    # rooms
    h.append('')
    h.append(f'#define GROOM_MAXID {maxid}  /* largest room instance id in the data file */')
    h.append('struct groominst { int32_t id; int16_t obj; int16_t x, y; int32_t depth; };  /* depth: of its layer */')
    h.append('struct groom { int16_t w, h; int16_t n; const struct groominst *in; };')
    for rn, r in rooms.items():
        h.append(f'extern const struct groom groom_{rn};  /* {len(r["insts"])} instances, creation order */')
        c.append(f'static const struct groominst groom_{rn}_in[{len(r["insts"])}] = {{')
        c += [f'    {{ {i}, OBJ_{o}, {x}, {y}, {d} }},' for i, o, x, y, d in r['insts']]
        c += ['};', f'const struct groom groom_{rn} = {{ {r["w"]}, {r["h"]}, {len(r["insts"])}, groom_{rn}_in }};', '']
    objs = sorted(o for o in os.listdir(os.path.join(src, 'objects'))
                  if os.path.exists(os.path.join(src, 'objects', o, o + '.yy')))
    sidx = {n[0]: i for i, n in enumerate(spr)}
    h.append(f'#define GOBJ_COUNT {len(objs)}')
    h.append('extern const int16_t gobjspr[GOBJ_COUNT];  /* default sprite_index per object (objects.h order) */')
    c.append('const int16_t gobjspr[GOBJ_COUNT] = {')
    for o in objs:
        t = open(os.path.join(src, 'objects', o, o + '.yy'), encoding='utf-8-sig').read()
        m = re.search(r'"spriteId":\s*\{\s*"name":\s*"(\w+)"', t)
        c.append(f'    {sidx[m.group(1)] if m else -1},  /* {o}{" " + m.group(1) if m else ""} */')
    c += ['};', '']
    c += ['const char *gsw(const struct gswtab *t, int line, int value, int k)', '{', '    int i;',
          '    for (i = 0; i < t->n; i++)',
          '        if (t->c[i].line == line && t->c[i].value == value && k < t->c[i].n)',
          '            return t->c[i].lits[k];',
          '    return 0;', '}', '']
    h += ['', '#endif', '']
    open(out, 'w').write('\n'.join(h))
    open(os.path.splitext(out)[0] + '.c', 'w').write('\n'.join(c))


if __name__ == '__main__':
    main()
