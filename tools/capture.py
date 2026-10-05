#!/usr/bin/env python3
"""The game capture (docs/ARCADE.md section 7): screenshots of the settings screen's GAME CAPTURE pages -> the
capture's text, which scripts/replay.sh replays on the host.

    tools/capture.py decode <shot.png> ... -o <capture.txt>
        each screenshot (384 x 224: a MiSTer screenshot of jtcps3, or a MAME snapshot) is one page; pages are
        matched by their index, a page whose CRC-16 fails is decoded again from the cell-wise majority of every shot
        of it; the capture's CRC-32 must hold. Writes the header as "# " lines and the controls in the route format
        (tests/routes: "<steps> <keys>")
    tools/capture.py steps <cabinet.log> <host.log>
        "T <steps> <life> <money> <level> <x> <y> <rng>" lines (scripts/lua/capture.lua in MAME, host.c
        HOST_STEPLOG): the first step whose line differs, or how many are equal
    tools/capture.py frames <gen dir> <out dir>
        tests/game/host.c's frame stream (out dir "-") on stdin -> <out dir>/f_<rec>.png (320 x 240 view)

Page layout (src/shell/shell.c capture_view): text cells (1 + c, 2 + r), c < 46, r < 24, at screen x 8 * col,
y 8 * row; each shows 7 bits as 4 x 2 pixel blocks (2 columns, 4 rows; block b white = bit 6 - b set, black = not),
block 7 red. Pixels are read 1 and 2 from the block's left edge (jtcps3 screenshots have single pixels that take
their right neighbour's colour: tools/jtshot.py). A page's grid is found by trying offsets of up to 4 pixels.
"""
import os
import struct
import sys
import zlib

COL0, ROW0, COLS, ROWS = 1, 2, 46, 24
CELLS = COLS * ROWS
PAGE = CELLS * 7 // 8
PDATA = PAGE - 6
HDR = ['magic', 'version', 'flags', 'rev', 'seed', 'steps', 'nent', 'nchk', 'level', 'plife', 'money', 'room',
       'dead', 'end_room']
MAGIC = 0x53504b43
CHK_EVERY = 64
FLAGS = [(0x01, 'toggle_run'), (0x02, 'smooth'), (0x04, 'god'), (0x08, 'dev'), (0x10, 'over'), (0x20, 'trunc'),
         (0x40, 'lossy'), (0x80, 'dirty')]
LETTERS = 'RLUDJAINBO'


def crc16(data, c=0xffff):
    for b in data:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xffff if c & 0x8000 else (c << 1) & 0xffff
    return c


def classify(p):
    r, g, b = p[:3]
    if r > 128 and g < 96 and b < 96:
        return 'R'
    return 'W' if r + g + b > 384 else 'K'


def read_cells(px, dx, dy):
    """the CELLS values (None where a block's samples disagree) and how many red marks are where they should be"""
    vals, marks = [], 0
    for j in range(CELLS):
        x0, y0 = 8 * (COL0 + j % COLS) + dx, 8 * (ROW0 + j // COLS) + dy
        v, bad = 0, False
        for blk in range(8):
            bx, by = x0 + 4 * (blk % 2), y0 + 2 * (blk // 2)
            s = [classify(px[bx + u, by + w]) for u in (1, 2) for w in (0, 1)]
            c = max('KWR', key=s.count)
            if s.count(c) < 3:
                bad = True
            if blk == 7:
                marks += c == 'R' and s.count(c) >= 3
            else:
                v = v << 1 | (c == 'W')
                bad = bad or c == 'R'
        vals.append(None if bad else v)
    return vals, marks


def cells_bytes(vals):
    bits = ''.join(format(v or 0, '07b') for v in vals)
    return bytes(int(bits[k:k + 8], 2) for k in range(0, 8 * PAGE, 8))


def page_ok(pg):
    return struct.unpack('>H', pg[4:6])[0] == crc16(pg[6:], crc16(pg[:4]))


def read_shot(path):
    from PIL import Image
    im = Image.open(path).convert('RGB')
    if im.size != (384, 224):
        im = im.resize((384, 224), Image.NEAREST)
    px = im.load()
    best = None
    for dy in range(-4, 5):
        for dx in range(-4, 5):
            if 8 * COL0 + dx < 0 or 8 * ROW0 + dy < 0 or 8 * (COL0 + COLS) + dx > 384 or 8 * (ROW0 + ROWS) + dy > 224:
                continue
            vals, marks = read_cells(px, dx, dy)
            unsure = sum(v is None for v in vals)
            if best is None or (marks, -unsure) > (best[1], -best[4]):
                best = (vals, marks, dx, dy, unsure)
    return best


def decode(shots, out):
    pages, npages, size = {}, None, None
    for s in shots:
        vals, marks, dx, dy, _ = read_shot(s)
        pg = cells_bytes(vals)
        ok = page_ok(pg)
        print(f'{os.path.basename(s)}: offset {dx},{dy}, marks {marks}/{CELLS}, unsure cells '
              f'{sum(v is None for v in vals)}, page {pg[0] + 1}/{pg[1]} CRC {"ok" if ok else "BAD"}')
        if marks < CELLS // 2:
            continue                              # not a capture page
        pages.setdefault(pg[0], []).append((vals, pg, ok))
        if ok:
            npages, size = pg[1], struct.unpack('>H', pg[2:4])[0]
    if npages is None:
        sys.exit('capture.py: no page with a good CRC')
    blob = b''
    for p in range(npages):
        got = pages.get(p, [])
        good = [pg for _, pg, ok in got if ok]
        if not good and len(got) > 1:            # every shot of the page failed: the cell-wise majority
            vals = []
            for j in range(CELLS):
                vs = [v[j] for v, _, _ in got if v[j] is not None]
                vals.append(max(set(vs), key=vs.count) if vs else 0)
            pg = cells_bytes(vals)
            if page_ok(pg):
                good = [pg]
                print(f'page {p + 1}: CRC ok from the majority of {len(got)} shots')
        if not good:
            sys.exit(f'capture.py: page {p + 1} of {npages} missing or bad ({len(got)} shots): take more screenshots')
        blob += good[0][6:]
    blob = blob[:size]
    if zlib.crc32(blob[:-4]) != struct.unpack('>I', blob[-4:])[0]:
        sys.exit('capture.py: the capture CRC-32 is wrong')
    h = dict(zip(HDR, struct.unpack('>14I', blob[:56])))
    for k in ('level', 'plife', 'money', 'room', 'dead', 'end_room'):
        if h[k] >= 1 << 31:
            h[k] -= 1 << 32
    if h['magic'] != MAGIC or h['version'] != 1:
        sys.exit(f'capture.py: magic {h["magic"]:#x} version {h["version"]}: not a version 1 capture')
    ne, nc = h['nent'], h['nchk']
    ent = struct.unpack(f'>{ne}H', blob[56:56 + 2 * ne])
    chk = struct.unpack(f'>{nc}H', blob[56 + 2 * ne:56 + 2 * ne + 2 * nc])
    flags = ' '.join(f'{n} {int(bool(h["flags"] & b))}' for b, n in FLAGS)
    with open(out, 'w') as f:
        f.write(f'# game capture: build {h["rev"]:08x} (tools/capture.py decode of {len(shots)} shots)\n')
        f.write(f'# seed {h["seed"]}\n# flags {h["flags"]:x} {flags}\n# steps {h["steps"]}\n')
        f.write(f'# end level {h["level"]} plife {h["plife"]} money {h["money"]} room {h["room"]} dead {h["dead"]} '
                f'end_room {h["end_room"]}\n')
        for k in range(0, nc, 32):
            f.write(f'# chk {CHK_EVERY} ' + ' '.join(f'{v:04x}' for v in chk[k:k + 32]) + '\n')
        steps = 0
        for e in ent:
            m = e & 0x7ff
            keys = ''.join(LETTERS[b] for b in range(10) if m >> b & 1) + ('P' if m & 0x400 else '')
            f.write(f'{(e >> 11) + 1} {keys or "-"}\n')
            steps += (e >> 11) + 1
    print(f'capture: build {h["rev"]:08x}, seed {h["seed"]}, {h["steps"]} steps ({steps} in {ne} runs), {nc} '
          f'checkpoints, flags {flags}; end level {h["level"]} plife {h["plife"]} money {h["money"]} room {h["room"]}'
          f' dead {h["dead"]} -> {out}')
    if steps != h['steps']:
        sys.exit('capture.py: the runs do not add up to the steps')


def steps_cmp(a, b):
    def load(p):
        return {int(ln.split()[1]): ln.split()[2:] for ln in open(p) if ln.startswith('T ')}
    ca, hb = load(a), load(b)
    common = sorted(set(ca) & set(hb))
    for s in common:
        if ca[s] != hb[s]:
            print(f'step {s}: cabinet {" ".join(ca[s])} host {" ".join(hb[s])}')
            print(f'steps: {common.index(s)} of {len(common)} equal before step {s}')
            return 1
    gaps = [s for s in range(1, max(ca) + 1) if s not in ca] if ca else []
    death = [s for s in common if int(ca[s][0]) <= 0]
    print(f'steps: {len(common)} of {len(common)} equal (cabinet {len(ca)} steps logged, gaps {len(gaps)}, host '
          f'{len(hb)}); life 0 from step {death[0] if death else "-"}; last: {" ".join(ca[common[-1]]) if common else "-"}')
    return 0


def frames(gen, d):
    import json
    from PIL import Image
    g = json.load(open(os.path.join(gen, 'gfx.json')))
    hm = json.load(open(os.path.join(gen, 'hud.json')))
    pals = {1: g['palette'], 2: hm['palette'], 3: hm['palette_yellow'], 4: g['palette']}
    fb = open(os.path.join(gen, 'fade.bin'), 'rb').read()
    os.makedirs(d, exist_ok=True)
    inp = sys.stdin.buffer
    n = 0
    while True:
        hd = inp.read(8)
        if len(hd) < 8:
            break
        rec, a8 = struct.unpack('<ii', hd)
        px = struct.unpack('<%dH' % (320 * 240), inp.read(320 * 240 * 2))
        fades = {1: struct.unpack_from('<256H', fb, 512 * a8), 5: struct.unpack_from('<256H', fb, 512 * (256 + a8))}
        im = Image.new('RGB', (320, 240))
        ip = im.load()
        for y in range(240):
            for x in range(320):
                w = px[y * 320 + x]
                c = (fades.get(w >> 8) or pals.get(w >> 8, [0] * 256))[w & 255] if w else 0
                ip[x, y] = ((c & 31) << 3, (c >> 5 & 31) << 3, (c >> 10 & 31) << 3)
        im.save(os.path.join(d, f'f_{rec}.png'))
        n += 1
    print(f'{n} frames -> {d}')


def main(a):
    if len(a) >= 4 and a[0] == 'decode' and '-o' in a:
        k = a.index('-o')
        return decode(a[1:k] + a[k + 2:], a[k + 1])
    if len(a) == 3 and a[0] == 'steps':
        return steps_cmp(a[1], a[2])
    if len(a) == 3 and a[0] == 'frames':
        return frames(a[1], a[2])
    sys.exit(__doc__)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
