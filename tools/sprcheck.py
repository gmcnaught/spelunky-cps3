#!/usr/bin/env python3
"""Check tools/hdsprites.py's output: every frame redrawn from its pieces and tiles (gfx.bin) equals the source PNG
after the converter's colour mapping (alpha < 128 transparent).   tools/sprcheck.py <hd src dir> <gen dir>"""
import json
import os
import re
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hdsprites  # noqa: E402


def table(path, name):
    t = open(path).read()
    body = t[t.index(name):]
    body = body[body.index('{') + 1:body.index('\n};')]
    return [list(map(lambda v: int(v, 0), re.findall(r'-?0x[0-9a-f]+|-?\d+', ln.split('/*')[0])))
            for ln in body.splitlines() if '{' in ln]


def main():
    src, gen = sys.argv[1], sys.argv[2]
    meta = json.load(open(os.path.join(gen, 'gfx.json')))
    gfx = open(os.path.join(gen, 'gfx.bin'), 'rb').read()
    pal = meta['palette']
    inv = {}
    for i in range(1, 256):                      # first index of each colour (unused entries are 0, as black is)
        inv.setdefault(pal[i], i)
    c = os.path.join(gen, 'sprites.c')
    spr, frm, pcs = table(c, 'sprdefs['), table(c, 'framedefs['), table(c, 'piecedefs[')
    names = [ln.strip().strip('",') for ln in open(c).read().split('sprnames[SPR_COUNT] = {')[1].split('};')[0].splitlines() if ln.strip()]
    bad = 0
    for k, n in enumerate(names):
        w, h, xo, yo, *_rest = spr[k]
        f0, nf = spr[k][9], spr[k][10]
        y, ims = hdsprites.frames_of(src, n)
        for fi in range(nf):
            want = [[0] * w for _ in range(h)]
            im = ims[fi]
            if im is not None:
                px = im.load()
                for yy in range(min(h, im.height)):
                    for xx in range(min(w, im.width)):
                        r, g, b, a = px[xx, yy]
                        if a >= 128:
                            want[yy][xx] = inv[hdsprites.bgr555(r, g, b)]
            got = [[0] * w for _ in range(h)]
            p0, npc = frm[f0 + fi]
            for dx, dy, pw, ph, tile in pcs[p0:p0 + npc]:
                t = tile - meta['first_tile']
                for i in range(pw):
                    for j in range(ph):
                        tb = gfx[256 * (t + i * ph + j):256 * (t + i * ph + j + 1)]
                        for yy in range(16):
                            for xx in range(16):
                                X, Y = dx + xo + 16 * i + xx, dy + yo + 16 * j + yy
                                if 0 <= X < w and 0 <= Y < h and tb[16 * yy + xx]:
                                    got[Y][X] = tb[16 * yy + xx]
            if got != want:
                bad += 1
                print(f'{n} frame {fi}: differs')
    print(f'{sum(s[10] for s in spr)} frames checked, {bad} differ')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
