#!/usr/bin/env python3
"""The title logo, "SPELUNKY CLASSIC ARCADE" (sTitle_HD in place of HD's "SPELUNKY CLASSIC HD"), built from HD's own
sTitle_HD at build time so the repo carries no HD art: rows 0..31, columns 0..127 (SPELUNKY over CLASSIC) unchanged,
at the same place (oTitleLogo's sTitle, drawn behind it 1 px left, is its shadow); HD's big "HD" (columns 131..192)
dropped; a third line, ARCADE (rows 32..47), as wide as SPELUNKY (128 columns) in the same cracked letters: A and C
from CLASSIC, E from SPELUNKY, R from P's bowl over K's stem and leg, D from P and U with the right corners cut; each
letter widened to 20 / 21 columns by repeating columns that equal their left neighbour (so the cracks stay as they are).

    tools/titlelogo.py <hd src dir> <out.png>      the logo (preview)

hdsprites.frames_of gives it for sTitle_HD (and its width / height for the .yy's), so every tool sees the same art;
hdsprites registers original()'s colours first, so the palette stays HD's."""
import os
import sys

from PIL import Image

NAME = 'sTitle_HD'
TOP_W = 128                                     # SPELUNKY / CLASSIC: columns 0..127
GLYPH = {'P': (16, 0), 'E': (32, 0), 'U': (64, 0), 'K': (96, 0), 'C': (14, 16), 'A': (46, 16)}   # 15 x 16 each
WIDTHS = [21, 20, 21, 20, 21, 20]               # A R C A D E, 1 column apart: 128 in all
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


def _widen(g, w):
    """g widened to w columns: repeat the columns equal to their left neighbour (nearest the middle first)"""
    n = len(g[0])
    plain = sorted((c for c in range(1, n) if all(r[c] == r[c - 1] for r in g)), key=lambda c: abs(c - n // 2))
    if not plain:
        raise SystemExit('titlelogo: a letter with no column to repeat')
    reps = {c: 0 for c in plain}
    for k in range(w - n):
        reps[plain[k % len(plain)]] += 1
    return [sum(([r[c]] * (1 + reps.get(c, 0)) for c in range(n)), []) for r in g]


def _letter(hd, ch, w):
    """letter ch, w columns wide; R: P's bowl widened, K's rows 11..15 below with the leg moved right by w - 15 (the
    leg as K draws it), and a crack in the top bar as E's (row 1 dark, row 2 light)"""
    g = _widen(_glyph(hd, ch), w)
    if ch == 'R':
        kx0, ky0 = GLYPH['K']
        none = (0, 0, 0, 0)
        for y in range(11, 16):
            k = [hd.getpixel((kx0 + x, ky0 + y)) for x in range(15)]
            g.append(k[:4] + [none] * (w - 15) + k[4:])
        ex, ey = GLYPH['E']
        g[1][w - 8], g[2][w - 8] = hd.getpixel((ex + 9, ey + 1)), hd.getpixel((ex + 9, ey + 2))
    return g


def build(src):
    hd = original(src)
    im = Image.new('RGBA', (TOP_W, 48), (0, 0, 0, 0))
    im.paste(hd.crop((0, 0, TOP_W, 32)), (0, 0))
    x0 = 0
    for ch, w in zip('ARCADE', WIDTHS):
        for y, row in enumerate(_letter(hd, ch, w)):
            for x, c in enumerate(row):
                if c[3]:
                    im.putpixel((x0 + x, 32 + y), c)
        x0 += w + 1
    return im


if __name__ == '__main__':
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    build(sys.argv[1]).save(sys.argv[2])
