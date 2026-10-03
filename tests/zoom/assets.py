#!/usr/bin/env python3
"""tests/zoom's flash: 20 column tiles (tile c: a fill in pen 1 + c % 7, a 1-px white left edge, a 1-px black mark
at x = c % 16 on rows 4-11) and their colours. Also writes the expected 384x224 screens (expect_<zoom>.png)
from MAME's full-screen zoom formula (cps3.cpp: source x = (x * (z << 16) / 0x40) >> 16).
    assets.py <flash.bin> <assets.h>      (in the container)
    assets.py --expect <dir>              (on the host: needs Pillow)"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'cps3-testgame', 'sdk',
                                'tools'))
from cps3asset import Flash  # noqa: E402

TILES_AT, COLOURS_AT = 0x10000, 0x20000
NCOL, NROW = 20, 14
ZOOMS = (0x40, 0x35, 0x36)
# BGR555: 0 transparent, 1-7 fills, 8 white, 9 black
COLS = [0, 0x001f, 0x03e0, 0x7c00, 0x03ff, 0x7c1f, 0x7fe0, 0x4210, 0x7fff, 0x0000]


def tile(c):
    px = []
    for y in range(16):
        for x in range(16):
            p = 1 + c % 7
            if x == 0:
                p = 8
            elif x == c % 16 and 4 <= y < 12:
                p = 9
            px.append(p)
    return px


def expect(z):
    """RGB rows of the screen MAME shows: the 320x224 grid (tilemap from map pixel 0, 0) zoomed in x by z"""
    from PIL import Image
    src = [[tile(x // 16)[(y % 16) * 16 + x % 16] if x < 16 * NCOL else 0 for x in range(384)] for y in range(224)]
    fsz = (z << 16) // 0x40
    clip_r = ((384 * fsz + 0x8000) >> 16) - 1 if z != 0x40 else 383
    im = Image.new('RGB', (384, 224))
    for y in range(224):
        for x in range(384):
            sx = (x * fsz) >> 16
            p = src[y][sx] if sx <= clip_r else 0
            w = COLS[p]
            im.putpixel((x, y), ((w & 31) << 3, (w >> 5 & 31) << 3, (w >> 10 & 31) << 3))
    return im


def main():
    if sys.argv[1] == '--expect':
        for z in ZOOMS:
            expect(z).save(os.path.join(sys.argv[2], f'expect_{z:02x}.png'))
        return
    f = Flash()
    px = []
    for c in range(NCOL):
        px += tile(c)
    f.tiles(TILES_AT, px)
    f.colours(COLOURS_AT, COLS + [0] * (16 - len(COLS)))
    f.write(sys.argv[1])
    open(sys.argv[2], 'w').write(f'#define TILES_AT 0x{TILES_AT:x}u\n#define COLOURS_AT 0x{COLOURS_AT:x}u\n'
                                 f'#define NCOL {NCOL}\n#define NROW {NROW}\n')


if __name__ == '__main__':
    main()
