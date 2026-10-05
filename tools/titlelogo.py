#!/usr/bin/env python3
"""The title logo, "SPELUNKY CLASSIC ARCADE" (sTitle_HD in place of HD's "SPELUNKY CLASSIC HD"), built from HD's own
sTitle_HD at build time (as all art, from refs/): rows 0..31, columns 0..127 (SPELUNKY over CLASSIC) unchanged,
at the same place (oTitleLogo's sTitle, drawn behind it 1 px left, is its shadow); HD's big "HD" (columns 131..192)
dropped; a third line, ARCADE (rows 32..47), in CLASSIC's cracked 15 x 16 letters 1 column apart, centred under it
(columns 16..110): A and C from CLASSIC, E from SPELUNKY, R from P's bowl over K's stem and leg, D from P and U with
the right corners cut.

    tools/titlelogo.py <hd src dir> <out.png>      the logo (preview)

hdsprites.frames_of gives it for sTitle_HD (and its width / height for the .yy's), so every tool sees the same art;
hdsprites registers original()'s colours first, so the palette stays HD's."""
import os
import sys

from PIL import Image

NAME = 'sTitle_HD'
# rTitle's oTitleLogo_HD (this logo) and oTitleLogo (sTitle, its shadow) moved right by DX (tools/fronttables.py): the
# 128-column logo (SPELUNKY's width) centred in the cavern's open columns at its rows (view x 30..288 at the attract camera, 320, 0)
MOVED, DX = ('oTitleLogo_HD', 'oTitleLogo'), 40
TOP_W = 128                                     # SPELUNKY / CLASSIC: columns 0..127
GLYPH = {'P': (16, 0), 'E': (32, 0), 'U': (64, 0), 'K': (96, 0), 'C': (14, 16), 'A': (46, 16)}   # 15 x 16 each
ARCADE_X = 16                                   # A R C A D E, 15 columns each, 1 apart: columns 16..110 (CLASSIC: 14..112)
# D: P's top and U's bottom, as a pixel map ('.': none, 0 dark, 1 light, 2 fill), the right corners cut by 3
D_ROWS = ['011111111111...',
          '1222222220221..',
          '22222222212221.',
          '222000000000221',
          '2220........022', '2220........022', '2220........022', '2220........022',
          '2220........022', '2220........022', '2220........022', '2220........022',
          '222111111111220',
          '22222222222220.',
          '0222222022220..',
          '.000000.0000...']


def _png(src):
    import hdsprites
    y = hdsprites.load_yy(os.path.join(src, 'sprites', NAME, NAME + '.yy'))
    return os.path.join(src, 'sprites', NAME, y['frames'][0]['name'] + '.png')


def original(src):
    return Image.open(_png(src)).convert('RGBA')


def _glyph(hd, ch):
    """15 x 16 letter as rows of RGBA tuples (alpha 0: none)"""
    px = lambda x, y: hd.getpixel((x, y))
    if ch == 'D':
        S = GLYPH['E']                          # E's top-left: its 0 / 1 / 2 colours
        col = {'0': px(S[0], S[1]), '1': px(S[0] + 1, S[1]), '2': px(S[0] + 1, S[1] + 1), '.': (0, 0, 0, 0)}
        return [[col[c] for c in r] for r in D_ROWS]
    if ch == 'R':                               # P rows 0..10 (_letter adds K's stem and leg)
        x0, y0 = GLYPH['P']
        return [[px(x0 + x, y0 + y) for x in range(15)] for y in range(11)]
    x0, y0 = GLYPH[ch]
    return [[px(x0 + x, y0 + y) for x in range(15)] for y in range(16)]


def _letter(hd, ch):
    """letter ch; R: P's bowl, K's rows 11..15 (the stem and the leg) below, a crack in the top bar as E's (row 1
    dark, row 2 light)"""
    g = _glyph(hd, ch)
    if ch == 'R':
        kx0, ky0 = GLYPH['K']
        g += [[hd.getpixel((kx0 + x, ky0 + y)) for x in range(15)] for y in range(11, 16)]
        ex, ey = GLYPH['E']
        g[1][7], g[2][7] = hd.getpixel((ex + 9, ey + 1)), hd.getpixel((ex + 9, ey + 2))
    return g


def build(src):
    hd = original(src)
    im = Image.new('RGBA', (TOP_W, 48), (0, 0, 0, 0))
    im.paste(hd.crop((0, 0, TOP_W, 32)), (0, 0))
    for i, ch in enumerate('ARCADE'):
        for y, row in enumerate(_letter(hd, ch)):
            for x, c in enumerate(row):
                if c[3]:
                    im.putpixel((ARCADE_X + 16 * i + x, 32 + y), c)
    return im


if __name__ == '__main__':
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    build(sys.argv[1]).save(sys.argv[2])
