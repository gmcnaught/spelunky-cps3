"""P1 screen check: the dumped solids / player / ladders against a frame the tracer saved (TRACE_SHOT).

    TRACE_SHOT=1 scripts/hd_trace.sh p1_walk 1 p1_walk_s1shot
    python3 tools/screencheck.py build/trace/p1_walk_s1shot 1     -> counts, build/trace/<name>.overlay_<rec>.png

Each 16x16 cell of application_surface (1 px = 1 room px; the view at 0,0) is called terrain when at least 40 %
of its pixels are brown (r > b + 25); a cell is solid when the record has an oBrick / oBlock / oPushBlock /
oArrowTrapLeft / oArrowTrapRight instance there. Overlay (x3): green solids, yellow ladders, magenta the player
(16x16 around x, y), cyan entrance / exit.
"""
import sys
from PIL import Image, ImageDraw
sys.path.insert(0, 'tools')
import tracer
T = sys.argv[1]
REC = int(sys.argv[2]) if len(sys.argv) > 2 else 1
names = tracer.load_names(T + '.names')
for hd, insts in tracer.records(open(T + '.bin', 'rb').read()):
    if hd['rec'] == REC: break
vx, vy, vw, vh, sw, sh = map(float, open(f'{T}.shot_{REC}.txt').read().split())
im = Image.open(f'{T}.shot_{REC}.png').convert('RGB'); W, H = im.size
px = im.load()
SOLID = ('oBrick', 'oBlock', 'oPushBlock', 'oArrowTrapLeft', 'oArrowTrapRight')
solid = set(); objs = []
for i in insts:
    n = names['O'][i['obj']]
    objs.append((n, i['x'], i['y']))
    if n in SOLID:
        assert i['x'] % 16 == 0 and i['y'] % 16 == 0, (n, i['x'], i['y'])
        solid.add((int(i['x']) // 16, int(i['y']) // 16))
# per 16x16 cell in the picture: share of terrain-coloured pixels (brown/orange: r > b + 25)
def frac(cx, cy):
    n = t = 0
    for y in range(cy * 16, cy * 16 + 16):
        for x in range(cx * 16, cx * 16 + 16):
            if x < W and y < H:
                r, g, b = px[x, y]; n += 1; t += (r > b + 25)
    return t / n if n else 0
cols, rows = (W + 15) // 16, (H + 15) // 16
TH = 0.40
sf = []; nf = []
tp = fn = fp = tn = 0; fps = []; fns = []
for cy in range(rows):
    for cx in range(cols):
        f = frac(cx, cy); s = (cx, cy) in solid
        (sf if s else nf).append(f)
        if s and f >= TH: tp += 1
        elif s: fn += 1; fns.append((cx, cy, round(f, 2)))
        elif f >= TH: fp += 1; fps.append((cx, cy, round(f, 2)))
        else: tn += 1
print(f' terrain share: solid cells min {min(sf):.2f}, other cells max {max(nf):.2f} (threshold {TH})')
print(f'rec {REC}: view ({vx},{vy}) surface {W}x{H}; cells {cols}x{rows}: solid&terrain {tp}, solid&not {fn}, '
      f'notsolid&terrain {fp}, neither {tn}')
print(' solid but not terrain-coloured:', fns)
print(' terrain-coloured but no solid instance:', [(c, [n for n, x, y in objs if int(x)//16 == c[0] and int(y)//16 == c[1]]) for c in fps])
for n, x, y in objs:
    if n in ('oPlayer1', 'oEntrance', 'oExit'):
        print(f' {n} at ({x}, {y})')
d = ImageDraw.Draw(im)
for cx, cy in solid:
    if cx < cols and cy < rows:
        d.rectangle([cx * 16, cy * 16, cx * 16 + 15, cy * 16 + 15], outline=(0, 255, 0))
for n, x, y in objs:
    c = {'oPlayer1': (255, 0, 255), 'oEntrance': (0, 255, 255), 'oExit': (0, 255, 255)}.get(n)
    if n.startswith('oLadder'): c = (255, 255, 0)
    if c:
        d.rectangle([x, y, x + 15, y + 15] if n != 'oPlayer1' else [x - 8, y - 8, x + 7, y + 7], outline=c)
im = im.resize((W * 3, H * 3), Image.NEAREST)
im.save(f'{T}.overlay_{REC}.png')
print(f' overlay: {T}.overlay_{REC}.png')
