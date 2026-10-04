#!/usr/bin/env python3
"""Tables for the play loop (src/game/p*.c), from HD 1.2.2's data file (game.unx, read by UndertaleModTool's CLI
in Docker as tools/hdgentables.py does).

    tools/hdplaytables.py <game.unx> <out playtables.h>      (also writes playtables.c next to it)

1. Per sprite (enum gspr order = the runtime's sprite_index): width, height, frame count, playback speed and
   type, separate masks flag and the collision masks. GameMaker 2024 stores each mask cropped to the sprite's
   bounding box: (r - l + 1) bits per row, rows t..b, MSB first, rows padded to bytes (Observed: sRock's mask
   against collision_point(prec = 1) in the runner, build/p4/probe).
2. Per object (objects.h order): the runtime object index (the data file's object order: GameMaker dispatches
   events object by object in this order, build/p4 event logs), default visible / persistent, the events it has
   with inheritance resolved (the runner's GetEventRecursive: an object without an event runs its parent's), the
   alarms it has, and its collision targets.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
UTMT = os.path.normpath(os.path.join(HERE, '..', '..', '_tools', 'UndertaleModTool', 'out-cli', 'UndertaleModCli.dll'))
DOTNET = 'mcr.microsoft.com/dotnet/sdk:10.0'

CSX = r'''
using System.IO;
using System.Text;
var sb = new StringBuilder();
foreach (var s in Data.Sprites) {
    sb.Append($"S {s.Name.Content} {s.Width} {s.Height} {s.Textures.Count} {s.GMS2PlaybackSpeed} {(int)s.GMS2PlaybackSpeedType} {(int)s.SepMasks} {s.CollisionMasks.Count} {s.MarginLeft} {s.MarginRight} {s.MarginTop} {s.MarginBottom} {s.OriginX} {s.OriginY}");
    foreach (var m in s.CollisionMasks) { sb.Append(" "); sb.Append(System.Convert.ToHexString(m.Data)); }
    sb.Append("\n");
}
foreach (var o in Data.GameObjects) {
    sb.Append($"O {o.Name.Content} {(o.ParentId == null ? "-" : o.ParentId.Name.Content)} {(o.Sprite == null ? "-" : o.Sprite.Name.Content)} {(o.TextureMaskId == null ? "-" : o.TextureMaskId.Name.Content)} {(o.Visible?1:0)} {(o.Persistent?1:0)}");
    for (int t = 0; t < o.Events.Count; t++) foreach (var e in o.Events[t]) sb.Append($" {t}:{e.EventSubtype}");
    sb.Append("\n");
}
foreach (var name in new[]{"rTransition1","rTransition1x","rTransition2","rTransition2x","rTransition3","rTransition3x","rTransition4"}) {
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

# event bits (struct pobj.ev)
EV = {(0, 0): 'EV_CREATE', (1, 0): 'EV_DESTROY', (3, 0): 'EV_STEP', (3, 1): 'EV_BEGIN', (3, 2): 'EV_END',
      (7, 7): 'EV_ANIMEND', (7, 0): 'EV_OUTSIDE', (8, 0): 'EV_DRAW', (7, 4): 'EV_ROOMSTART', (7, 5): 'EV_ROOMEND'}
EVBIT = ['EV_CREATE', 'EV_DESTROY', 'EV_STEP', 'EV_BEGIN', 'EV_END', 'EV_ANIMEND', 'EV_OUTSIDE', 'EV_DRAW',
         'EV_ROOMSTART', 'EV_ROOMEND']


def export(unx, work):
    unx, work = os.path.abspath(unx), os.path.abspath(work)
    root = os.path.commonpath([unx, UTMT, work])
    rel = lambda p: '/w/' + os.path.relpath(p, root)
    txt = os.path.join(work, 'playtables_rt.txt')
    csx = os.path.join(work, 'playtables_rt.csx')
    open(csx, 'w').write(CSX.replace('OUT', rel(txt)))
    if os.path.exists(txt):
        os.remove(txt)
    r = subprocess.run(['docker', 'run', '--rm', '-v', f'{root}:/w', DOTNET, 'dotnet', rel(UTMT), 'load', rel(unx),
                        '-s', rel(csx)], capture_output=True, text=True)
    if not os.path.exists(txt):
        sys.exit(r.stdout[-3000:] + r.stderr[-3000:])
    return txt


def main():
    unx, out = sys.argv[1:3]
    work = os.path.dirname(out) or '.'
    os.makedirs(work, exist_ok=True)
    txt = export(unx, work)
    sprs, objs, rooms, cur = [], [], {}, None
    for line in open(txt):
        f = line.split()
        if f[0] == 'ROOM':
            cur = rooms[f[1]] = dict(w=int(f[2]), h=int(f[3]), insts=[])
            continue
        if f[0] == 'INST':
            if f[6] != '1' or f[5] != '1' or f[7] != '0' or f[8] != '0':
                sys.exit(f'room instance with scale or creation code: {line}')
            cur['insts'].append((int(f[1]), f[2], int(f[3]), int(f[4]), int(f[9])))
            continue
        if f[0] == 'S':
            sprs.append(dict(name=f[1], w=int(f[2]), h=int(f[3]), frames=int(f[4]), speed=float(f[5]),
                             stype=int(f[6]), sep=int(f[7]), nm=int(f[8]), masks=f[15:15 + int(f[8])]))
        elif f[0] == 'O':
            evs = [tuple(map(int, e.split(':'))) for e in f[7:]]
            objs.append(dict(name=f[1], parent=None if f[2] == '-' else f[2], spr=f[3], mask=f[4], vis=int(f[5]),
                             pers=int(f[6]), evs=evs))
    byname = {o['name']: o for o in objs}
    rtidx = {o['name']: i for i, o in enumerate(objs)}
    # objects.h order (tools/hdobjects.py): read the enum
    oh = open(os.path.join(work, 'objects.h')).read()
    hobjs = []
    for m in re.finditer(r'\bOBJ_(\w+)\s*(?:=\s*\d+)?\s*,', oh):
        if m.group(1) not in ('COUNT', 'WORDS') and m.group(1) not in hobjs:
            hobjs.append(m.group(1))

    def chain(n):
        c = []
        while n:
            c.append(n)
            n = byname[n]['parent'] if n in byname else None
        return c

    def descendants_or_self(t):
        return {o['name'] for o in objs if t in chain(o['name'])}

    hdr = '/* generated by tools/hdplaytables.py from HD 1.2.2 (game.unx): do not edit */'
    h = [hdr, '#ifndef PLAYTABLES_H', '#define PLAYTABLES_H', '#include <stdint.h>', '#include "gentables.h"',
         '#include "objects.h"', '']
    h += [f'#define {b} 0x{1 << k:x}' for k, b in enumerate(EVBIT)]
    h += ['', '/* sprite data: size, frames, playback speed (type 0 frames per second, 1 frames per game frame),',
          '   masks cropped to the bounding box (gsprcol l..r x t..b), MSB first, rows padded to bytes */',
          'struct psprite { int16_t w, h; int16_t frames; uint8_t stype, sep; float speed; int16_t nmasks;',
          '                 int32_t maskoff; };',
          'extern const struct psprite psprite[GSPR_COUNT];', 'extern const uint8_t pmaskdata[];', '',
          '/* per object (objects.h order): runtime object index (event dispatch order), events with inheritance',
          '   resolved (EV_*), alarms (bit k: Alarm k), collision targets (objects.h indices) */',
          'struct pobj { int16_t rt; uint8_t visible, persistent; uint16_t ev; uint16_t alarms; int16_t ncol;',
          '              int16_t col0; };',
          'extern const struct pobj pobj[OBJ_COUNT];',
          'extern const int16_t pcol[];  /* collision target lists: pobj.col0 .. + ncol */',
          f'#define RTOBJ_COUNT {len(objs)}',
          'extern const int16_t obj_byrt[RTOBJ_COUNT];  /* OBJ_* of runtime object index */', '',
          '/* the transition rooms\' instances in creation order (struct groom: gentables.h) */']
    h += [f'extern const struct groom groom_{rn};  /* {len(r["insts"])} instances */' for rn, r in rooms.items()]
    h += ['', '#endif', '']
    c = [hdr, '#include "playtables.h"', '', 'const uint8_t pmaskdata[] = {']
    off = 0
    sprrows = []
    for s in sprs:
        mo = off
        for m in s['masks']:
            b = bytes.fromhex(m)
            c.append('    ' + ''.join(f'{x},' for x in b) + f'  /* {s["name"]} */' if b else f'    /* {s["name"]} empty */')
            off += len(b)
        sprrows.append(mo)
    c += ['    0', '};', '', 'const struct psprite psprite[GSPR_COUNT] = {']
    for s, mo in zip(sprs, sprrows):
        c.append(f'    {{ {s["w"]}, {s["h"]}, {s["frames"]}, {s["stype"]}, {s["sep"]}, {s["speed"]!r}f, {s["nm"]}, {mo} }},'
                 f'  /* {s["name"]} */')
    c += ['};', '']
    hidx = {n: i for i, n in enumerate(hobjs)}
    cols = []
    c.append('const struct pobj pobj[OBJ_COUNT] = {')
    for n in hobjs:
        if n not in byname:
            c.append(f'    {{ -1, 0, 0, 0, 0, 0, 0 }},  /* {n}: not in the data file */')
            continue
        ev, al, ct = 0, 0, []
        for a in chain(n):
            for t, s in byname[a]['evs']:
                if (t, s) in EV and not ev & (1 << EVBIT.index(EV[(t, s)])):
                    ev |= 1 << EVBIT.index(EV[(t, s)])
                if t == 2:
                    al |= 1 << s
                if t == 4:
                    tn = objs[s]['name']
                    if tn not in ct:
                        ct.append(tn)
        o = byname[n]
        c0 = len(cols)
        cols += [hidx[t] for t in ct if t in hidx]
        c.append(f'    {{ {rtidx[n]}, {o["vis"]}, {o["pers"]}, 0x{ev:x}, 0x{al:x}, {len(cols) - c0}, {c0} }},  /* {n}'
                 + (f' col: {" ".join(ct)}' if ct else '') + ' */')
    c += ['};', '', 'const int16_t pcol[] = {', '    ' + ', '.join(str(x) for x in cols) + (', ' if cols else '') + '0',
          '};', '', 'const int16_t obj_byrt[RTOBJ_COUNT] = {']
    c += [f'    {hidx.get(o["name"], -1)},  /* {o["name"]} */' for o in objs]
    c += ['};', '']
    for rn, r in rooms.items():
        c.append(f'static const struct groominst groom_{rn}_in[{len(r["insts"])}] = {{')
        c += [f'    {{ {i}, OBJ_{o}, {x}, {y}, {d} }},' for i, o, x, y, d in r['insts']]
        c += ['};', f'const struct groom groom_{rn} = {{ {r["w"]}, {r["h"]}, {len(r["insts"])}, groom_{rn}_in }};', '']
    open(out, 'w').write('\n'.join(h))
    open(os.path.splitext(out)[0] + '.c', 'w').write('\n'.join(c))
    print(f'{out}: {len(sprs)} sprites, {off} mask bytes, {len(objs)} objects')


if __name__ == '__main__':
    main()
