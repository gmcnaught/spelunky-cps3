#!/usr/bin/env python3
"""Read tests/playsh2's jtcps3 results screen (JT builds, main.c jt_show) from a screenshot (MiSTer or MAME, 384 x 224).

    tools/jtresult.py <shot.png> [--json]

The screen is text only: the large summary (bigtext.c: 3x5 glyphs of 4x4 px, rows 0-2) and the table in the SDK's
8x8 font (../cps3-testgame/sdk/src/font.h; cps3v_text: cell rows 3-27). Each 8x8 cell is matched exactly against
the font (lit = any channel >= 128); the cell grid's offset is found from the screen itself (the one that matches
most non-blank cells), so a screenshot shifted by the core's crop reads the same.

Prints the summary line and one line per job: kind+number, CHK, name, total clocks, step mean, step max.
Exit 0: a results screen was read; 2: the program is still running (its RUNNING line); 1: neither.
"""
import os
import re
import sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_H = os.path.join(HERE, '..', '..', 'cps3-testgame', 'sdk', 'src', 'font.h')
BIG = {  # bigtext.c's 3x5 glyphs, one row each, bit 2 = left
    '0': (7, 5, 5, 5, 7), '1': (2, 6, 2, 2, 7), '2': (7, 1, 7, 4, 7), '3': (7, 1, 3, 1, 7), '4': (5, 5, 7, 1, 1),
    '5': (7, 4, 7, 1, 7), '6': (7, 4, 7, 5, 7), '7': (7, 1, 1, 2, 2), '8': (7, 5, 7, 5, 7), '9': (7, 5, 7, 1, 7),
    'A': (2, 5, 7, 5, 5), 'B': (6, 5, 6, 5, 6), 'D': (6, 5, 5, 5, 6), 'F': (7, 4, 6, 4, 4), 'G': (7, 4, 5, 5, 7),
    'I': (7, 2, 2, 2, 7), 'K': (5, 5, 6, 5, 5), 'L': (4, 4, 4, 4, 7), 'M': (5, 7, 7, 5, 5), 'P': (7, 5, 7, 4, 4),
    'R': (6, 5, 6, 5, 5), 'T': (7, 2, 2, 2, 2), 'X': (5, 5, 2, 5, 5), 'V': (5, 5, 5, 5, 2), 'E': (7, 4, 6, 4, 7),
    'N': (6, 5, 5, 5, 5), 'H': (5, 5, 7, 5, 5), 'U': (5, 5, 5, 5, 7), 'C': (7, 4, 4, 4, 7), 'W': (5, 5, 7, 7, 5),
    '/': (1, 1, 2, 4, 4), ' ': (0, 0, 0, 0, 0),
}
BIG['O'] = BIG['0']   # the same glyph: read by context below
BIG['S'] = BIG['5']


def font():
    """{8 row bytes: character} from font.h (ASCII 32-95, bit 7 = left)"""
    rows = re.findall(r'\{((?:\s*0x[0-9a-fA-F]{2}\s*,?){8})\}', open(FONT_H).read())
    out = {}
    for k, r in enumerate(rows[:64]):
        b = tuple(int(v, 16) for v in re.findall(r'0x[0-9a-fA-F]{2}', r))
        out.setdefault(b, chr(32 + k))
    return out


def lit(img):
    w, h = img.size
    p = img.convert('RGB').load()
    return [[1 if max(p[x, y]) >= 128 else 0 for x in range(w)] for y in range(h)], w, h


def cell(L, x0, y0):
    return tuple(sum(L[y0 + y][x0 + x] << (7 - x) for x in range(8)) for y in range(8))


def read(path):
    L, w, h = lit(Image.open(path))
    F = font()
    blank = (0,) * 8
    best = None
    for oy in range(8):
        for ox in range(8):
            n = 0
            for r in range(3, (h - oy) // 8):
                for c in range((w - ox) // 8):
                    g = cell(L, ox + 8 * c, oy + 8 * r)
                    if g != blank and g in F:
                        n += 1
            if best is None or n > best[0]:
                best = (n, ox, oy)
    _, ox, oy = best
    rows = []
    for r in range((h - oy) // 8):
        s = ''
        for c in range((w - ox) // 8):
            g = cell(L, ox + 8 * c, oy + 8 * r)
            s += F.get(g, '?' if g != blank else ' ')
        rows.append(s.rstrip())
    return L, ox, oy, rows


def big_row(L, ox, oy, row):
    """bigtext.c row `row` (cell rows 3 * row ..): 24 characters, 2 cells each"""
    s = ''
    for k in range(24):
        x0, y0 = ox + 16 * k, oy + 24 * row
        g = []
        for gy in range(5):
            v = 0
            for gx in range(3):
                y, x = y0 + 4 * gy + 1, x0 + 4 * gx + 1
                v = v << 1 | (L[y][x] if y < len(L) and x < len(L[0]) else 0)
            g.append(v)
        g = tuple(g)
        s += next((c for c, b in BIG.items() if b == g), '?')
    return s.rstrip()


def main():
    a = sys.argv[1:]
    if not a:
        sys.exit(__doc__)
    L, ox, oy, rows = read(a[0])
    if any('TIMING: RUNNING' in r for r in rows):
        print('RUNNING')
        sys.exit(2)
    hdr = next((i for i, r in enumerate(rows) if r.startswith('JOB CHK NAME')), None)
    if hdr is None:
        print('no results screen')
        sys.exit(1)
    top = big_row(L, ox, oy, 0)
    # 'O' / '0' and 'S' / '5' share glyphs: the summary is PASS|FAIL n/N SPR OK|SPR BAD
    m = re.match(r'(PA55|PASS|FAIL) *(\d+)/(\d+) *5PR *(0K|OK|BAD)', top.replace('O', '0').replace('S', '5')
                 .replace('PA55', 'PASS'))
    summary = top
    if m:
        summary = '%s %s/%s SPR %s' % (m.group(1).replace('PA55', 'PASS'), m.group(2), m.group(3),
                                       'BAD' if m.group(4) == 'BAD' else 'OK')
    print(summary)
    for r in rows[hdr:]:
        if r.strip():
            print(r)
    sys.exit(0)


if __name__ == '__main__':
    main()
