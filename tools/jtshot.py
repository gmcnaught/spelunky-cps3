#!/usr/bin/env python3
"""MiSTer (jtcps3) screenshots of the game program against the model of the HD runner's trace record.

    tools/jtshot.py expect <trace.bin> <rec> <out.png> [--hud] [--sstext col,row,TEXT ...]
    tools/jtshot.py cmp <trace.bin> <rec> <shot.png> [--hud] [--sstext ...] [--mask diff.png] [--expect out.png]
    tools/jtshot.py ident <expected dir> <shot.png> ...   which held record each screenshot shows

ident compares each screenshot with every expect_<set>_<rec>.png in the directory (build/jtshot/expect) (scripts/jt_frames.sh's) as cmp
does. It prints the best match, its differing / jitter counts and the runner-up's, or "none" when even the best
differs in more than 2% of the pixels (a shot between holds). So shots can be taken at any rate through a whole run,
without relying on the hold schedule.

The model is tools/drawmodel.py's 320 x 240 view of the record (with HD's HUD for --hud), shown as jtcps3 shows the
program's screen: view lines 8..231 (the crop), and the full-screen X zoom 0x35 sampled as jtcps3 does it. Screen
column X shows source column ((X + 1) * fsz + 0x5800) >> 16, fsz = 0x35 << 16 / 0x40 (PLAN.md P3's jtcps3
recheck; MAME's own sampling is (X * fsz) >> 16, tools/hudcheck.py screen). --sstext: the shell's text layer (the
attract's credit line), drawn over the screen unzoomed, as drawmodel.py does for MAME.

cmp: in 5-bit colour (8-bit channels >> 3; jtcps3 expands v << 3 | v >> 2), columns 1..382 only:
- column 0 is the screenshot's edge;
- column 383 shows source column 318, which no MAME screen has, so it is unchecked.
A differing pixel that equals its expected right neighbour is counted apart as jitter: the known single pixels that
take their right neighbour's colour, at different places in each shot of the same frame. Exit 0 when no other
pixel differs.
"""
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drawmodel  # noqa: E402

ZOOM = 0x35
CROP = 8
GEN = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'gen')


def jt_screen(view):
    """jtcps3's 384 x 224 screen of a hudcheck View (320 x 240 bgr555 words, None = backdrop 0)"""
    fsz = (ZOOM << 16) // 0x40
    im = Image.new('RGB', (384, 224))
    px = im.load()
    for y in range(224):
        row = view.px[y + CROP]
        for x in range(384):
            sx = ((x + 1) * fsz + 0x5800) >> 16
            w = (row[sx] if sx < 320 else None) or 0
            px[x, y] = ((w & 31) << 3, (w >> 5 & 31) << 3, (w >> 10 & 31) << 3)
    return im


def expected(trace, rec, hud, sstexts):
    names = trace[:-4] + '.names'
    _, v, _, cam = drawmodel.model(trace, names, rec, GEN, hud)
    im = jt_screen(drawmodel.model.with_gui if hud else v)
    for t in sstexts:
        col, row, text = t.split(',', 2)
        drawmodel.sstext(im, int(col), int(row), text)
    return im, cam


def compare(exp, shot, mask=None):
    s = shot.convert('RGB')
    if s.size != (384, 224):
        raise SystemExit(f'screenshot is {s.size[0]} x {s.size[1]}: expected jtcps3\'s 384 x 224')
    e, p = exp.load(), s.load()
    c5 = lambda v: (v[0] >> 3, v[1] >> 3, v[2] >> 3)
    m = Image.new('L', (384, 224)) if mask else None
    bad = jit = 0
    for y in range(224):
        for x in range(1, 383):
            a = c5(p[x, y])
            if a == c5(e[x, y]):
                continue
            if a == c5(e[x + 1, y]):
                jit += 1
                if m: m.putpixel((x, y), 128)
            else:
                bad += 1
                if m: m.putpixel((x, y), 255)
    if m:
        m.save(mask)
    return bad, jit


def ident(d, shots):
    import glob
    exps = {os.path.basename(p)[7:-4]: Image.open(p).convert('RGB') for p in sorted(glob.glob(os.path.join(d, 'expect_*.png')))}
    if not exps:
        sys.exit(f'{d}: no expect_*.png')
    ok = True
    for s in shots:
        im = Image.open(s)
        r = sorted((compare(e, im) + (n,) for n, e in exps.items()), key=lambda t: t[0])
        (bad, jit, name), second = r[0], (r[1] if len(r) > 1 else None)
        if bad > 0.02 * 382 * 224:
            print(f'{s}: none (best {name}: {bad} px differ)')
            continue
        print(f'{s}: {name}: {bad} px differ, {jit} jitter' + (f' (next {second[2]}: {second[0]})' if second else ''))
        ok = ok and bad == 0
    return ok


def main():
    a = sys.argv[1:]
    if len(a) >= 3 and a[0] == 'ident':
        sys.exit(0 if ident(a[1], a[2:]) else 1)
    if len(a) < 4 or a[0] not in ('expect', 'cmp'):
        sys.exit(__doc__)
    hud = '--hud' in a
    sst = [a[k + 1] for k, w in enumerate(a) if w == '--sstext']
    opt = lambda k: a[a.index(k) + 1] if k in a else None
    exp, cam = expected(a[1], int(a[2]), hud, sst)
    if a[0] == 'expect':
        exp.save(a[3])
        print(f'{a[3]}: record {a[2]}, camera {cam[0]},{cam[1]}')
        return
    if opt('--expect'):
        exp.save(opt('--expect'))
    bad, jit = compare(exp, Image.open(a[3]), opt('--mask'))
    print(f'record {a[2]} camera {cam[0]},{cam[1]}: {bad} of {382 * 224} px differ (columns 1-382, 5-bit), '
          f'{jit} single-pixel jitter')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
