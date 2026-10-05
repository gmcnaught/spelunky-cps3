#!/usr/bin/env python3
"""Dark levels (PLAN P7): HD's darkness is one black rectangle at alpha oLevel.darkness over the view, drawn in
oLevel's Draw (depth -2; objects/oLevel/Draw_0.gml). oScreen's darkSurf (the light circles, objects/oScreen/Step_1.gml)
is built but never drawn, so lights do not cut holes: they only lower oLevel.darkness (oLevel Step :111-137:
dist / 160 from the nearest flare or oPlayer1.distToNearestLightSource, at most 0.9).

On the CPS3 the rectangle is a palette fade: colour code DRAW_PAL_DARK holds the game palette with every colour
faded, and everything drawn before oLevel uses it. A faded entry is computed from the art's 8-bit colour, as the
runner blends it; a 5-bit palette index whose sprites use more than one 8-bit colour can fade to different values
in the runner (listed by `check`).

    tools/darkfade.py sources <hd src> <gen dir>            build <gen dir>/rgbsrc.json (5-bit colour -> 8-bit sources)
    tools/darkfade.py fit <trace.bin> <names> <gen dir> <rec>,...   the blend rule against TRACE_SHOT frames
    tools/darkfade.py check <gen dir>                       5-bit colours whose 8-bit sources fade apart
    tools/darkfade.py table <gen dir>                       <gen dir>/fade.bin + fade.h: the game palette faded at
                                                            every alpha byte a8 = 0 .. 255 (512 bytes each), for
                                                            palette DMA (src/draw), flash offset DARK_FADE_AT; the
                                                            HUD palette (DARK_FADE_HUD_AT) and its c_yellow text
                                                            palette (DARK_FADE_HUDY_AT) the same way
"""
import json
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hdsprites  # noqa: E402

LEVELS = 64          # darkness steps the CPS3 build keeps a palette for (darkness 0 .. 0.9)


def fade8(c, a):
    """the runner's blend of one 8-bit channel under the black rectangle at alpha a: draw_set_alpha keeps
    a8 = floor(a * 255) (the vertex colour's alpha byte), the blend is dst * (255 - a8) / 255 rounded to 8 bits
    (`fit`: of the rules tried, the one the runner's frames of build/trace/g_p7_dark_s18 agree with)"""
    a8 = int(a * 255)
    return (c * (255 - a8) + 127) // 255


def fade555(rgb, a):
    return hdsprites.bgr555(*(fade8(c, a) for c in rgb))


def sources(src, gen):
    out = {}
    for n in sorted(os.listdir(os.path.join(src, 'sprites'))):
        if n.startswith('__') or 'scribble' in n.lower() or not os.path.exists(os.path.join(src, 'sprites', n, n + '.yy')):
            continue
        _, ims = hdsprites.frames_of(src, n)
        for im in ims:
            if im is None:
                continue
            for r, g, b, a in set(im.getdata()):
                if a >= 128:
                    out.setdefault(hdsprites.bgr555(r, g, b), set()).add((r, g, b))
    json.dump({str(k): sorted(v) for k, v in out.items()}, open(os.path.join(gen, 'rgbsrc.json'), 'w'))
    print(f'{len(out)} 5-bit colours, {sum(len(v) > 1 for v in out.values())} with more than one 8-bit source')


FADE_AT = 0x1160000     # after tools/hudart.py's tiles (0x1150000 + 139 tiles)


def palette_rgb(gen):
    """the game palette's 8-bit colour per index (tools/hdsprites.py gfx.json rgb: one entry per 8-bit colour where
    a 5-bit colour has several; None: unused)"""
    meta = json.load(open(os.path.join(gen, 'gfx.json')))
    return [tuple(meta['rgb'][k]) if k and meta['palette'][k] else None for k in range(256)]


def table(gen):
    import struct
    hud = json.load(open(os.path.join(gen, 'hud.json')))
    if hud['tiles_at'] + 256 * hud['ntiles'] > FADE_AT:
        raise SystemExit('fade table overlaps the HUD tiles')
    rgb = palette_rgb(gen)
    meta = json.load(open(os.path.join(gen, 'gfx.json')))
    out = bytearray()
    for a8 in range(256):
        pal = [0 if c is None else hdsprites.bgr555(*((ch * (255 - a8) + 127) // 255 for ch in c)) for c in rgb]
        pal = [meta['palette'][k] if rgb[k] is None else pal[k] for k in range(256)]
        out += b''.join(struct.pack('<H', w) for w in pal)
    # the HUD palette too (tools/hudart.py: the price tag oItem / oDamsel draw in their Draw event, before oLevel's
    # rectangle): 256 more palettes at DARK_FADE_HUD_AT
    hrgb = hud['rgb']
    for a8 in range(256):
        hp = [0] * 256
        for k, c in enumerate(hrgb):
            hp[k + 1] = hdsprites.bgr555(*((ch * (255 - a8) + 127) // 255 for ch in c))
        out += b''.join(struct.pack('<H', w) for w in hp)
    # the HUD's c_yellow text palette (tools/hudart.py: each colour times (255, 255, 0), then faded): text drawn
    # before the ending's black rectangles (showFinalScore, drawCredits) fades with it, 256 more at DARK_FADE_HUDY_AT
    for a8 in range(256):
        hp = [0] * 256
        for k, c in enumerate(hrgb):
            y = (c[0], c[1], 0)
            hp[k + 1] = hdsprites.bgr555(*((ch * (255 - a8) + 127) // 255 for ch in y))
        out += b''.join(struct.pack('<H', w) for w in hp)
    open(os.path.join(gen, 'fade.bin'), 'wb').write(out)
    open(os.path.join(gen, 'fade.h'), 'w').write(
        '/* generated by tools/darkfade.py table: do not edit */\n#ifndef FADE_H\n#define FADE_H\n'
        f'#define DARK_FADE_AT 0x{FADE_AT:x}u   /* 256 palettes of 256 colours: alpha byte a8 at + 512 * a8 */\n'
        f'#define DARK_FADE_HUD_AT 0x{FADE_AT + 0x20000:x}u   /* the HUD palette (code HUD_PAL) the same way */\n'
        f'#define DARK_FADE_HUDY_AT 0x{FADE_AT + 0x40000:x}u   /* its c_yellow text palette (HUD_PAL_YELLOW) */\n'
        '#endif\n')
    print(f'fade.bin: 3 x 256 palettes at flash {FADE_AT:#x}')


def load_sources(gen):
    return {int(k): [tuple(c) for c in v] for k, v in json.load(open(os.path.join(gen, 'rgbsrc.json'))).items()}


def check(gen):
    s = load_sources(gen)
    bad = 0
    for k, v in sorted(s.items()):
        if len(v) > 1:
            apart = [a / 100 for a in range(0, 91) if len({fade555(c, a / 100) for c in v}) > 1]
            if apart:
                bad += 1
                print(f'{k:#06x} sources {v}: fade apart at {len(apart)} of 91 darkness values')
    print(f'{bad} colours fade apart')


def fit(trace, names, gen, recs):
    import drawmodel
    import viewlevel
    s = load_sources(gen)
    g = viewlevel.Gen(gen)
    kind = drawmodel.kinds(g, drawmodel.SRC)
    rules = {
        'a8 floor, round': fade8,
        'a8 floor, trunc': lambda c, a: c * (255 - int(a * 255)) // 255,
        'a8 round, round': lambda c, a: (c * (255 - int(a * 255 + 0.5)) + 127) // 255,
        'float trunc': lambda c, a: int(c * (1 - a)),
        'float round': lambda c, a: int(c * (1 - a) + 0.5),
    }
    for r in recs:
        nm, tiles, hd, insts, _ = drawmodel.load(trace, names, r)
        v, cam = drawmodel.view555(g, nm, tiles, hd, insts, kind)
        a = hd['hud']['darkness']
        shot = Image.open(trace.replace('.bin', f'.shot_{r}.png')).convert('RGB').load()
        res = {}
        for name, f in rules.items():
            n = tot = 0
            for y in range(240):
                for x in range(320):
                    c = v.px[y][x]
                    if not c or len(s.get(c, [])) != 1:
                        continue
                    tot += 1
                    want = hdsprites.bgr555(*shot[x, y])
                    if hdsprites.bgr555(*(f(ch, a) for ch in s[c][0])) != want:
                        n += 1
            res[name] = n
        print(f'rec {r} darkness {a}: pixels differing per rule (of {tot}): {res}')


def main():
    a = sys.argv[1:]
    if a and a[0] == 'sources':
        sources(a[1], a[2])
    elif a and a[0] == 'check':
        check(a[1])
    elif a and a[0] == 'table':
        table(a[1])
    elif a and a[0] == 'fit':
        fit(a[1], a[2], a[3], [int(r) for r in a[4].split(',')])
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
