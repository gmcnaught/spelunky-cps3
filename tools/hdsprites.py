#!/usr/bin/env python3
"""HD 1.2.2's sprites for the CPS3 (PLAN.md §2, P3): one 256-colour palette for all art, every frame cut into CPS3
sprite pieces of 1, 2 or 4 x 1, 2 or 4 tiles (16x16, 8 bits a pixel, tiles column by column within a piece), all
tiles resident in character RAM. Default flash offsets: palette at 16 MB, tiles at 16 MB + 64 KB (the first 16 MB
are the sound window, tools/hdsound.py).

    tools/hdsprites.py <hd src dir> <out dir> [--tiles-at N] [--pal-at N] [--first-tile N]

Writes <out dir>/sprites.h, sprites.c (enum SPR_*, sprite / frame / piece tables) and gfx.bin + gfx.json (tile and
palette bytes and their flash offsets, merged into the flash image by the build). Colour 0 is transparent; each
source colour gets the 5-bit value nearest under jtcps3's expansion (v << 3 | v >> 2), as Maldita's converter.
Pixels with alpha < 128 are transparent (39 partially transparent pixels in all of HD's art).

Piece position: (dx, dy) in pixels from the sprite's origin to the piece's top-left, unmirrored. Drawn mirrored
(image_xscale < 0), a piece goes to (-dx - 16 * w) with the flip flag set.
"""
import json
import os
import re
import sys

from PIL import Image

import titlelogo

SIZES = [(4, 4), (4, 2), (2, 4), (2, 2), (4, 1), (1, 4), (2, 1), (1, 2), (1, 1)]   # (w, h) in tiles, largest first


def load_yy(path):
    t = open(path, encoding='utf-8-sig').read()
    return json.loads(re.sub(r',(\s*[}\]])', r'\1', t))


def expand(v):
    return v << 3 | v >> 2


NEAR5 = [min(range(32), key=lambda v: abs(expand(v) - c)) for c in range(256)]


def bgr555(r, g, b):
    return NEAR5[r] | NEAR5[g] << 5 | NEAR5[b] << 10


def frames_of(src, name):
    y = load_yy(os.path.join(src, 'sprites', name, name + '.yy'))
    if name == titlelogo.NAME:      # the port's title logo, "SPELUNKY CLASSIC ARCADE" (tools/titlelogo.py)
        im = titlelogo.build(src)
        y = dict(y, width=im.width, height=im.height, bbox_left=0, bbox_top=0, bbox_right=im.width - 1,
                 bbox_bottom=im.height - 1)
        return y, [im]
    out = []
    for f in y['frames']:
        png = os.path.join(src, 'sprites', name, f['name'] + '.png')
        out.append(Image.open(png).convert('RGBA') if os.path.exists(png) else None)
    return y, out


def cut(pix, w, h):
    """pieces covering every opaque tile of a w x h frame of palette indices (list of rows):
    [(tx, ty, tw, th)] in tiles, greedy, largest sizes first, no overlaps, pieces inside the frame's tile grid"""
    tw, th = (w + 15) // 16, (h + 15) // 16

    def opaque(tx, ty):
        for y in range(16 * ty, min(h, 16 * ty + 16)):
            row = pix[y]
            for x in range(16 * tx, min(w, 16 * tx + 16)):
                if row[x]:
                    return True
        return False

    need = {(tx, ty) for ty in range(th) for tx in range(tw) if opaque(tx, ty)}
    pieces, covered = [], set()
    for ty in range(th):
        for tx in range(tw):
            if (tx, ty) not in need:
                continue
            for sw, sh in SIZES:
                if tx + sw > tw or ty + sh > th:
                    continue
                cells = [(tx + i, ty + j) for i in range(sw) for j in range(sh)]
                if any(c in covered for c in cells):
                    continue
                # a big piece pays off only when most of it is drawn
                if sw * sh > 1 and sum(c in need for c in cells) * 2 < sw * sh:
                    continue
                pieces.append((tx, ty, sw, sh))
                covered.update(cells)
                need -= set(cells)
                break
    return pieces


def tile_bytes(pix, w, h, tx, ty):
    b = bytearray(256)
    for y in range(16):
        sy = 16 * ty + y
        if sy >= h:
            break
        row = pix[sy]
        for x in range(16):
            sx = 16 * tx + x
            if sx < w:
                b[16 * y + x] = row[sx]
    return bytes(b)


def main():
    src, out = sys.argv[1], sys.argv[2]
    args = sys.argv[3:]
    opt = {a: int(args[i + 1], 0) for i, a in enumerate(args) if a.startswith('--')}
    tiles_at, pal_at, first_tile = opt.get('--tiles-at', 0x1010000), opt.get('--pal-at', 0x1000000), opt.get('--first-tile', 0x100)
    names = sorted(n for n in os.listdir(os.path.join(src, 'sprites'))
                   if not n.startswith('__') and 'scribble' not in n.lower()
                   and os.path.exists(os.path.join(src, 'sprites', n, n + '.yy')))
    # palette entries: one per 5-bit colour, except that a 5-bit colour drawn from more than one 8-bit colour gets one
    # entry per 8-bit colour (same 5-bit value): a dark level's fade (tools/darkfade.py) starts from the 8-bit
    # colour, and those can fade to different 5-bit values
    srcs = {}
    for n in names:
        for im in frames_of(src, n)[1]:
            if im is not None:
                for r, g, b, a in set(im.getdata()):
                    if a >= 128:
                        srcs.setdefault(bgr555(r, g, b), set()).add((r, g, b))
    split = {k for k, v in srcs.items() if len(v) > 1}
    colour = {}                     # bgr555 (or (r, g, b) for split colours) -> palette index (1..255)
    rgb_of = {}                     # palette index -> 8-bit colour
    tiles, tile_of = [], {}         # deduplicated tile bytes
    runs = {}                       # piece tile-run bytes -> first tile number
    sprites, frames, pieces = [], [], []

    def index_of(n, r, g, b):
        c = bgr555(r, g, b)
        if c in split:
            c = (r, g, b)
        if c not in colour:
            colour[c] = len(colour) + 1
            rgb_of[colour[c]] = (r, g, b)
            if colour[c] > 255:
                raise SystemExit(f'{n}: more than 255 colours in all')
        return colour[c]

    for n in names:
        y, ims = frames_of(src, n)
        w, h = y['width'], y['height']
        sq = y.get('sequence', {})
        xo, yo = int(sq.get('xorigin', 0)), int(sq.get('yorigin', 0))
        first_frame = len(frames)
        if n == titlelogo.NAME:     # HD's own logo's colours first, in its order: the palette stays HD's
            hd = titlelogo.original(src)
            for yy in range(hd.height):
                for xx in range(hd.width):
                    r, g, b, a = hd.getpixel((xx, yy))
                    if a >= 128:
                        index_of(n, r, g, b)
        for im in ims:
            pix = [[0] * w for _ in range(h)]
            if im is not None:
                px = im.load()
                for yy in range(min(h, im.height)):
                    for xx in range(min(w, im.width)):
                        r, g, b, a = px[xx, yy]
                        if a >= 128:
                            pix[yy][xx] = index_of(n, r, g, b)
            first_piece = len(pieces)
            for tx, ty, sw, sh in cut(pix, w, h):
                run = b''.join(tile_bytes(pix, w, h, tx + i, ty + j) for i in range(sw) for j in range(sh))
                if run not in runs:
                    runs[run] = first_tile + len(tiles)
                    for k in range(sw * sh):
                        tiles.append(run[256 * k:256 * k + 256])
                pieces.append((16 * tx - xo, 16 * ty - yo, sw, sh, runs[run]))
            frames.append((first_piece, len(pieces) - first_piece))
        sprites.append((n, w, h, xo, yo, y.get('bbox_left', 0), y.get('bbox_top', 0), y.get('bbox_right', w - 1),
                        y.get('bbox_bottom', h - 1), y.get('collisionKind', 1), first_frame, len(frames) - first_frame))

    os.makedirs(out, exist_ok=True)
    pal = [0] * 256
    for c, i in colour.items():
        pal[i] = bgr555(*c) if isinstance(c, tuple) else c
    gfx = bytearray()
    for t in tiles:
        gfx += t
    open(os.path.join(out, 'gfx.bin'), 'wb').write(bytes(gfx))
    json.dump({'tiles_at': tiles_at, 'pal_at': pal_at, 'first_tile': first_tile, 'ntiles': len(tiles),
               'palette': pal, 'rgb': [list(rgb_of.get(i, (0, 0, 0))) for i in range(256)],
               'split': len(split)}, open(os.path.join(out, 'gfx.json'), 'w'))

    hdr = '/* generated by tools/hdsprites.py from refs/hd/src (Spelunky Classic HD 1.2.2): do not edit */'
    h = [hdr, '#ifndef SPRITES_H', '#define SPRITES_H', '#include <stdint.h>', '', 'enum spr {']
    h += [f'    SPR_{s[0]},' for s in sprites] + ['    SPR_COUNT', '};', '',
          f'#define GFX_TILES_AT 0x{tiles_at:x}u', f'#define GFX_PAL_AT 0x{pal_at:x}u',
          f'#define GFX_FIRST_TILE 0x{first_tile:x}u', f'#define GFX_NTILES {len(tiles)}u', f'#define GFX_NCOLOURS {len(colour) + 1}u', '',
          'struct sprdef { int16_t w, h, xorig, yorig, bbl, bbt, bbr, bbb; uint8_t kind; uint16_t frame, nframes; };',
          'struct framedef { uint16_t piece; uint8_t npieces; };',
          'struct piecedef { int16_t dx, dy; uint8_t w, h; uint16_t tile; };',
          'extern const struct sprdef sprdefs[SPR_COUNT];', 'extern const char *const sprnames[SPR_COUNT];',
          f'extern const struct framedef framedefs[{len(frames)}];', f'extern const struct piecedef piecedefs[{len(pieces)}];',
          '', '#endif', '']
    c = [hdr, '#include "sprites.h"', '', 'const struct sprdef sprdefs[SPR_COUNT] = {']
    c += [f'    {{ {s[1]}, {s[2]}, {s[3]}, {s[4]}, {s[5]}, {s[6]}, {s[7]}, {s[8]}, {s[9]}, {s[10]}, {s[11]} }},  /* {s[0]} */'
          for s in sprites]
    c += ['};', '', 'const char *const sprnames[SPR_COUNT] = {'] + [f'    "{s[0]}",' for s in sprites] + ['};', '']
    c += [f'const struct framedef framedefs[{len(frames)}] = {{'] + [f'    {{ {a}, {b} }},' for a, b in frames] + ['};', '']
    c += [f'const struct piecedef piecedefs[{len(pieces)}] = {{'] + \
         [f'    {{ {p[0]}, {p[1]}, {p[2]}, {p[3]}, 0x{p[4]:x} }},' for p in pieces] + ['};', '']
    open(os.path.join(out, 'sprites.h'), 'w').write('\n'.join(h))
    open(os.path.join(out, 'sprites.c'), 'w').write('\n'.join(c))
    big = max(f[1] for f in frames)
    print(f'{len(sprites)} sprites, {len(frames)} frames, {len(pieces)} pieces (most in a frame: {big}), '
          f'{len(tiles)} tiles ({len(gfx) / 2**20:.2f} MB), {len(colour)} colours ({len(split)} 5-bit colours split by '
          f'their 8-bit sources)')


if __name__ == '__main__':
    main()
