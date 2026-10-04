#!/usr/bin/env python3
"""P3 level viewer data and expected frames (PLAN.md P3) from HD reference traces (tools/tracer.py, SPT2 records).

    tools/viewlevel.py c <trace.bin> <names> <rec> <gen dir> <out.h>
    tools/viewlevel.py expect <trace.bin> <names> <rec> <gen dir> <out dir> <vx,vy> ...   (zoom 0x35, MAME's formula)
    tools/viewlevel.py shot <trace.bin> <names> <rec> <gen dir> <shot.png> <vx,vy> [--order old|new]
        compare the composed 320 x 240 view with the runner's own frame (its first 320 columns), in 5-bit colour

Drawing model: every drawable has a depth and is drawn deepest first.
  - The room background (bgCave tiled, layer depth 2147483500).
  - tile_add tiles (phase-0 record of the room; kept for the record asked): 16 x 16 cells of a background sprite.
  - Instances with a sprite, visible, at their traced depth, mirrored when image_xscale < 0.
On the CPS3, each tile depth becomes a tilemap when its cells fit (16-px cells, one tile per cell); terrain (oSolid
family at 16-px cells, one 16 x 16 piece) joins the tilemap of its depth. Up to 4 tilemaps (background first, then
the depths with most cells); everything else is a sprite. Equal depths: instances in creation order (--order old,
the default) or newest first (--order new), as the shot comparison decides.
"""
import json
import os
import re
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tracer  # noqa: E402

ZOOM = 0x35
CROP = 8
BG_DEPTH = 2147483500
MAX_MAPS = 4


def table(path, name):
    t = open(path).read()
    body = t[t.index(name):]
    body = body[body.index('{') + 1:body.index('\n};')]
    return [list(map(lambda v: int(v, 0), re.findall(r'-?0x[0-9a-f]+|-?\d+', ln.split('/*')[0])))
            for ln in body.splitlines() if '{' in ln]


class Gen:
    def __init__(self, gen):
        c = os.path.join(gen, 'sprites.c')
        self.spr = table(c, 'sprdefs[')
        self.frm = table(c, 'framedefs[')
        self.pcs = table(c, 'piecedefs[')
        self.sprname = [ln.strip().strip('",') for ln in
                        open(c).read().split('sprnames[SPR_COUNT] = {')[1].split('};')[0].splitlines() if ln.strip()]
        self.sprid = {n: i for i, n in enumerate(self.sprname)}
        oc = open(os.path.join(gen, 'objects.c')).read()
        body = oc[oc.index('objdefs[OBJ_COUNT] = {'):]
        body = body[:body.index('\n};')]
        self.obj = {}
        for m in re.finditer(r'\{ "(\w+)", (OBJ_\w+), [^,]+, [^,]+, (\d), (\d), (\d), (-?\d+) \}', body):
            self.obj[m.group(1)] = dict(parent=m.group(2)[4:], visible=int(m.group(4)), depth=int(m.group(6)))
        self.meta = json.load(open(os.path.join(gen, 'gfx.json')))
        self.gfx = open(os.path.join(gen, 'gfx.bin'), 'rb').read()

    def is_a(self, o, anc):
        while o and o != 'NONE':
            if o == anc:
                return True
            o = self.obj.get(o, {}).get('parent')
        return False

    def frame(self, spr, img):
        s = self.spr[spr]
        return s[9] + int(img) % s[10]

    def cell_tile(self, spr, left, top):
        """tile number of the 16 x 16 cell at (left, top) of sprite spr's frame 0, None if not a whole piece cell"""
        s = self.spr[spr]
        p0, n = self.frm[s[9]]
        for dx, dy, pw, ph, tile in self.pcs[p0:p0 + n]:
            px, py = left - (dx + s[2]), top - (dy + s[3])
            if 0 <= px < 16 * pw and 0 <= py < 16 * ph and px % 16 == 0 and py % 16 == 0:
                return tile + (px // 16) * ph + py // 16
        return None


def load(trace, names_path, rec):
    names = tracer.load_names(names_path)
    tiles, state = [], None
    for hd, insts in tracer.records(open(trace, 'rb').read()):
        if hd['phase'] == 0 and hd['rec'] <= rec:
            tiles = hd['tiles']
        if hd['rec'] == rec:
            state = (hd, insts)
            break
    if state is None:
        raise SystemExit(f'record {rec} not in {trace}')
    return names, tiles, state[1]


def drawables(trace, names_path, rec, g, order='old'):
    """[(depth, seq, kind, ...)] deepest first. kind 'cell': (tile, cx, cy) on the 16-px grid; 'frame': (frame, x,
    y, flip); the background is not listed (BG_DEPTH, tilemap 0)"""
    names, tiles, insts = load(trace, names_path, rec)
    out = []
    for k, (bg, left, top, w, h, x, y, depth) in enumerate(tiles):
        sid = g.sprid.get(bg)
        if sid is None:
            continue
        for j in range(int(h) // 16):
            for i in range(int(w) // 16):
                t = g.cell_tile(sid, int(left) + 16 * i, int(top) + 16 * j)
                X, Y = int(x) + 16 * i, int(y) + 16 * j
                if t is not None and X % 16 == 0 and Y % 16 == 0:
                    out.append((int(depth), k, 'cell', t, X // 16, Y // 16))
                else:
                    raise SystemExit(f'tile {bg} ({left},{top}) at ({x},{y}) is not on the 16-px grid')
    seq = list(reversed(insts)) if order == 'old' else list(insts)      # the trace lists newest first
    base = len(tiles)
    for k, i in enumerate(seq):
        d = i.get('draw') or {}
        if i['spr'] < 0 or not d.get('visible', 1):
            continue
        sn = names['S'][i['spr']]
        if sn not in g.sprid:
            continue
        sid = g.sprid[sn]
        f = g.frame(sid, i['img'])
        x, y, depth = int(i['x']), int(i['y']), int(d.get('depth', g.obj[names['O'][i['obj']]]['depth']))
        flip = d.get('xscale', 1.0) < 0
        p0, npc = g.frm[f]
        s = g.spr[sid]
        on = names['O'][i['obj']]
        one = (npc == 1 and g.pcs[p0][2:4] == [1, 1] and g.pcs[p0][0] == -s[2] and g.pcs[p0][1] == -s[3]
               and not flip)
        if g.is_a(on, 'oSolid') and one and x % 16 == 0 and y % 16 == 0:
            out.append((depth, base + k, 'cell', g.pcs[p0][4], x // 16, y // 16))
        else:
            out.append((depth, base + k, 'frame', f, x, y, flip))
    out.sort(key=lambda d: (-d[0], d[1]))
    return out


def plan_maps(dr):
    """the depths drawn on tilemaps 1..3 (tilemap 0: the background): the depths with most cells whose cells do not
    overlap, at most MAX_MAPS - 1; their other drawables stay sprites"""
    cells = {}
    for d in dr:
        if d[2] == 'cell':
            cells.setdefault(d[0], []).append(d)
    ok = {}
    for depth, cs in cells.items():
        seen = {(c[4], c[5]) for c in cs}
        if len(seen) == len(cs):
            ok[depth] = len(cs)
    return sorted(sorted(ok, key=lambda k: -ok[k])[:MAX_MAPS - 1], reverse=True)


def write_c(out, dr, maps, g):
    bg = g.sprid['bgCave']
    bp0, bn = g.frm[g.spr[bg][9]]
    L = ['/* generated by tools/viewlevel.py: do not edit */', '#include <stdint.h>',
         f'#define VIEW_BG_PIECE {bp0}', f'#define VIEW_BG_NPIECES {bn}', f'#define VIEW_NMAPS {len(maps)}',
         '/* map cells: tilemap (1..3), column, row, tile */', 'static const uint16_t view_cells[][4] = {']
    for d in dr:
        if d[2] == 'cell' and d[0] in maps:
            L.append(f'    {{ {1 + maps.index(d[0])}, {d[4]}, {d[5]}, 0x{d[3]:x} }},')
    L += ['};', '#define VIEW_NCELLS (sizeof view_cells / sizeof view_cells[0])',
          '/* the drawing order, deepest first: kind 0 = tilemap band (a = tilemap), 1 = frame (a = frame, x, y, flip),',
          '   2 = single tile as a sprite (a = tile, x, y) */',
          'static const int32_t view_list[][5] = {']
    done = set()
    for d in dr:
        if d[2] == 'cell' and d[0] in maps:
            m = 1 + maps.index(d[0])
            if m not in done:
                done.add(m)
                L.append(f'    {{ 0, {m}, 0, 0, 0 }},')
        elif d[2] == 'cell':
            L.append(f'    {{ 2, 0x{d[3]:x}, {16 * d[4]}, {16 * d[5]}, 0 }},')
        else:
            L.append(f'    {{ 1, {d[3]}, {d[4]}, {d[5]}, {int(d[6])} }},')
    L += ['};', '#define VIEW_NLIST (sizeof view_list / sizeof view_list[0])', '']
    open(out, 'w').write('\n'.join(L))


def compose(dr, g, vx, vy, W=320, H=240):
    """the view as palette indices (0 = backdrop)"""
    img = [[0] * W for _ in range(H)]
    first = g.meta['first_tile']

    def blit_tile(tile, px, py, flip=False):
        t = tile - first
        tb = g.gfx[256 * t:256 * t + 256]
        for yy in range(16):
            Y = py + yy
            if 0 <= Y < H:
                row = img[Y]
                for xx in range(16):
                    X = px + xx
                    v = tb[16 * yy + (15 - xx if flip else xx)]
                    if v and 0 <= X < W:
                        row[X] = v

    def blit_frame(f, x, y, flip):
        p0, n = g.frm[f]
        for dx, dy, pw, ph, tile in g.pcs[p0:p0 + n]:
            for i in range(pw):
                for j in range(ph):
                    if flip:
                        blit_tile(tile + i * ph + j, x - dx - 16 * pw + 16 * (pw - 1 - i), y + dy + 16 * j, True)
                    else:
                        blit_tile(tile + i * ph + j, x + dx + 16 * i, y + dy + 16 * j)

    bg = g.sprid['bgCave']
    for ry in range((vy // 64) * 64, vy + H, 64):
        for rx in range((vx // 64) * 64, vx + W, 64):
            blit_frame(g.spr[bg][9], rx - vx, ry - vy, False)
    for d in dr:
        if d[2] == 'cell':
            blit_tile(d[3], 16 * d[4] - vx, 16 * d[5] - vy)
        else:
            blit_frame(d[3], d[4] - vx, d[5] - vy, d[6])
    return img


def screen(view, g):
    """MAME's 384x224 screen: view lines CROP.., X zoom ZOOM (cps3.cpp's copy with zoom; see tests/zoom)"""
    pal = g.meta['palette']
    fsz = (ZOOM << 16) // 0x40
    clip_r = ((384 * fsz + 0x8000) >> 16) - 1
    im = Image.new('RGB', (384, 224))
    px = im.load()
    for y in range(224):
        row = view[y + CROP]
        for x in range(384):
            sx = (x * fsz) >> 16
            v = row[sx] if sx <= clip_r and sx < 320 else 0
            w = pal[v] if v else 0
            px[x, y] = ((w & 31) << 3, (w >> 5 & 31) << 3, (w >> 10 & 31) << 3)
    return im


def shot_diff(view, g, shot, mask=None):
    """pixels of the 320 x 240 view whose 5-bit colour differs from the runner's frame"""
    import hdsprites
    pal = g.meta['palette']
    s = Image.open(shot).convert('RGB').load()
    n = 0
    m = Image.new('L', (320, 240)) if mask else None
    for y in range(240):
        for x in range(320):
            r, gg, b = s[x, y]
            want = hdsprites.bgr555(r, gg, b)
            v = view[y][x]
            if (pal[v] if v else 0) != want:
                n += 1
                if m:
                    m.putpixel((x, y), 255)
    if m:
        m.save(mask)
    return n


def main():
    a = sys.argv[1:]
    order = a[a.index('--order') + 1] if '--order' in a else 'old'
    g = Gen(a[4])
    dr = drawables(a[1], a[2], int(a[3]), g, order)
    if a[0] == 'c':
        maps = plan_maps(dr)
        write_c(a[5], dr, maps, g)
        print(f'{len(dr)} drawables; tilemaps 1..{len(maps)} at depths {maps}')
    elif a[0] == 'expect':
        os.makedirs(a[5], exist_ok=True)
        for k, cam in enumerate(x for x in a[6:] if ',' in x):
            vx, vy = map(int, cam.split(','))
            screen(compose(dr, g, vx, vy), g).save(os.path.join(a[5], f'expect_{k}.png'))
    elif a[0] == 'shot':
        vx, vy = map(int, a[6].split(','))
        n = shot_diff(compose(dr, g, vx, vy), g, a[5], a[5].replace('.png', '.diff.png'))
        print(f'{a[5]}: {n} of {320 * 240} pixels differ (order {order})')
        sys.exit(1 if n else 0)
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
