#!/usr/bin/env python3
"""Host model of the game's frame (src/draw) from an HD runner trace record: tools/viewlevel.py's drawing model
(background, tile_add layers in their layer's element order, instances deepest first, equal depths newest first, x / y
truncated, mirroring by image_xscale) plus HD 1.2.2's Draw events (refs/hd/src/objects/<o>/Draw_0.gml, characterDrawEvent) and the view's
follow as the runner applies it before drawing. Independent of the C code: it reads the runner's trace only.

    tools/drawmodel.py screen <trace.bin> <names> <rec> <gen dir> <out.png> [--hud]
        MAME's 384 x 224 screen for record rec (view lines 8.., X zoom 0x35); --hud: HD's HUD (tools/hudcheck.py's
        model) drawn over it from the record's globals
    tools/drawmodel.py cmp <trace.bin> <names> <rec> <gen dir> <mame.png> [--hud] [--mask m.png] [--shot shot.png]
        pixels of the MAME snapshot whose 5-bit colour differs from the model; --shot also compares the model's
        320 x 240 view with the runner's own frame (TRACE_SHOT: application_surface, no GUI)
    tools/drawmodel.py hostcmp <trace.bin> <names> <gen dir> <host out dir> [--hud]
        every v_<rec>.bin of tests/game/host.c (the C display list composed on the host) against the model's view,
        view lines 8..231 (the screen), all 320 columns
Exit 0 when every comparison has 0 differing pixels.

Draw events modelled (as src/draw): oPlayer1 (characterDrawEvent: image_xscale from facing; jetpack / held arrow /
blinkToggle derived from invincible; jetpack / held arrow / red tint need fields the trace lacks: drawn as without them), oEnemy and its children (mirrored at x + 16 by
facing), oShopkeeper (+ the shotgun; hasGun assumed true: not traced), oDamsel (image_xscale from facing; the price
tag's sprite is sprite_add'ed: not drawn), oItem and children (unmirrored; price tag not drawn), oPDummy (image_xscale
from facing), oEntrance / oExit / oArrow / oTombLord / oYetiKing (draw_sprite, unmirrored), oLevel / oMsgSign (nothing).
"""
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tracer  # noqa: E402
import viewlevel  # noqa: E402
import drawtables  # noqa: E402

LEVEL_ROOMS = {'rLevel', 'rLevel2', 'rLevel3', 'rOlmec'}
RIGHT_PLAYER, RIGHT_ENEMY = 19, 1
HELD = {'oRock': 'ROCK', 'oJar': 'JAR', 'oSkull': 'SKULL', 'oFishBone': 'FISHBONE', 'oArrow': 'ARROW',
        'oMachete': 'MACHETE', 'oMattock': 'MATTOCK', 'oMattockHead': 'MATTOCKHEAD', 'oPistol': 'PISTOL',
        'oWebCannon': 'WEBCANNON', 'oTeleporter': 'TELEPORTER', 'oShotgun': 'SHOTGUN', 'oBow': 'BOW',
        'oSceptre': 'SCEPTRE', 'oFlare': 'FLARE', 'oKey': 'KEY'}


def kinds(g, src):
    """object name -> draw kind (DK_* without the prefix), as tools/drawtables.py assigns them"""
    import glob
    has = {os.path.basename(os.path.dirname(f)) for f in glob.glob(os.path.join(src, 'objects', '*', 'Draw_0.gml'))}
    out = {}
    for o in g.obj:
        a = o
        while a and a != 'NONE' and a not in has:
            a = g.obj.get(a, {}).get('parent')
        out[o] = 'SELF' if not a or a == 'NONE' else drawtables.KIND.get(a, 'DK_TODO')[3:]
    return out


def player_inv(names, insts):
    for i in insts:
        if names['O'][i['obj']] == 'oPlayer1':
            return int(i['vars'].get('invincible', 0))
    return 0


def blink_toggles(names, recs):
    """oPlayer1.blinkToggle at each record (not traced): oPlayer1 Step :1903-1911 toggles blinkToggle while blink > 0
    (then blink -= 1; -1 when 0). blink is set only by the hits, with invincible to the same value (oEnemy /
    oCaveman / ... Collision_oCharacter: 30; the restart :1856: 60), after the Step: a record whose invincible is 30
    or 60 and not the previous one less 1 had a hit in its step. invincible = 999 (level exit :738, :876) sets no
    blink. A room start creates the player again (characterCreateEvent: -1)."""
    out, bt, blink, prev = {}, -1, 0, 0
    for hd, insts in recs:
        inv = player_inv(names, insts)
        if hd['phase'] == 0:
            bt, blink = -1, 0
        else:
            if blink > 0:
                bt, blink = -bt, blink - 1
            else:
                bt = -1
            if inv in (30, 60) and inv > prev - 1:
                blink = inv
        prev = inv
        out[hd['rec']] = bt
    return out


def load(trace, names_path, rec, recs=None):
    names = tracer.load_names(names_path)
    tiles, state, nxt = [], None, None
    seen = []
    for hd, insts in (recs if recs is not None else tracer.records(open(trace, 'rb').read())):
        seen.append((hd, insts))
        if hd['phase'] == 0 and hd['rec'] <= rec:
            tiles = hd['tiles']
        if hd['rec'] == rec:
            state = (hd, insts)
        elif state is not None:
            nxt = hd
            break
    if state is None:
        raise SystemExit(f'record {rec} not in {trace}')
    state[0]['blinkToggle'] = blink_toggles(names, seen)[rec]
    return names, tiles, state[0], state[1], nxt


def room_size(rname):
    return {'rLevel': (672, 544), 'rLevel2': (672, 608), 'rLevel3': (672, 672), 'rOlmec': (672, 880)}.get(rname,
                                                                                                         (320, 240))


def view_after(hd, insts, names):
    """the view the frame after this record is drawn with: the record's view, then oPlayer1's follow (prun.c
    view_update: borders 160 / view_vborder, clamped to the room)"""
    rname = names['R'].get(hd['room'], '')
    vx, vy = int(hd['xview']), int(hd['yview'])
    W, H = room_size(rname)
    vb = 96 if rname in LEVEL_ROOMS else 0
    for i in insts:
        if names['O'][i['obj']] == 'oPlayer1':
            import math
            x, y, hb = math.floor(i['x']), math.floor(i['y']), 160
            if x - hb < vx:
                vx = x - hb
            elif x + hb > vx + 320:
                vx = x + hb - 320
            if y - vb < vy:
                vy = y - vb
            elif y + vb > vy + 240:
                vy = y + vb - 240
            vx = max(0, min(vx, W - 320))
            vy = max(0, min(vy, H - 240))
            break
    return vx, vy


def drawables(names, tiles, insts, g, kind, blink=-1):
    out = []
    for k, (bg, left, top, w, h, x, y, depth) in enumerate(tiles):
        sid = g.sprid.get(bg)
        if sid is None:
            continue
        for j in range(int(h) // 16):
            for i in range(int(w) // 16):
                t = g.cell_tile(sid, int(left) + 16 * i, int(top) + 16 * j)
                X, Y = int(x) + 16 * i, int(y) + 16 * j
                if t is not None:
                    out.append((int(depth), k, 'cell', t, X // 16, Y // 16))
    base = len(tiles)
    seq = list(insts)                             # equal depths: newest first (the trace's order). Checked on the
    # runner's frames: build/trace/g_p4_exit559_s559 record 300 and g_p5_shop_s96 record 242 (overlapping oBlood /
    # oBloodTrail at depth 1) equal only so; the other 16 frames of the gate are equal either way
    held = None                                   # oPlayer1 End Step (Step_2.gml), after the record: holdItem.depth
    for i in insts:                               # = 0 (51 climbing with jetpack / cape: not traced)
        if names['O'][i['obj']] == 'oPlayer1' and i['vars'].get('holdItem') is not None:
            held = int(i['vars']['holdItem'])
    for k, i in enumerate(seq):
        d = i.get('draw') or {}
        if not d.get('visible', 1):
            continue
        on = names['O'][i['obj']]
        kd = kind.get(on, 'SELF')
        depth = int(d.get('depth', g.obj[on]['depth']))
        if held is not None and i['id'] == held:
            depth = 0
        x, y = int(i['x']), int(i['y'])
        v = i['vars']
        ops = []

        def spr(name, img, X, Y, flip):
            if name in g.sprid:
                ops.append((g.frame(g.sprid[name], img), X, Y, flip))

        sn = names['S'].get(i['spr']) if i['spr'] >= 0 else None
        img = int(i['img'])
        facing = int(v.get('facing', 0))
        if kd == 'NONE' or sn is None and kd not in ('PLAYER',):
            continue
        if kd in ('SELF', 'TODO', 'DICE'):
            spr(sn, img, x, y, d.get('xscale', 1.0) < 0)
        elif kd == 'PLAYER':
            if blink != 1:
                spr(sn, img, x, y, facing == RIGHT_PLAYER)
        elif kd == 'DAMSEL':
            spr(sn, img, x, y, facing == RIGHT_PLAYER)
        elif kd == 'PDUMMY':
            spr(sn, img, x, y, facing == 1)
        elif kd in ('ITEM', 'PLAIN', 'JAWS'):
            spr(sn, img, x, y, False)
        elif kd in ('ENEMY', 'SHOP'):
            if facing == RIGHT_ENEMY:
                spr(sn, img, x + 16, y, True)
            else:
                spr(sn, img, x, y, False)
            if kd == 'SHOP' and int(v.get('status', 0)) not in (0, 5):
                if facing == 0:
                    spr('sShotgunLeft', 0, x + 6, y + 10, False)
                else:
                    spr('sShotgunRight', 0, x + 10, y + 10, False)
        for f, X, Y, flip in ops:
            out.append((depth, base + k, 'frame', f, X, Y, flip))
    out.sort(key=lambda e: (-e[0], e[1]))
    return out


def hud_case(hd, insts, names, vx, vy):
    c = dict(life=int(hd['plife']), bombs=int(hd['bombs']), ropes=int(hd['rope']), money=int(hd['money']), view_x=vx,
             view_y=vy)
    ids = {i['id']: i for i in insts}
    for i in insts:
        on = names['O'][i['obj']]
        if on == 'oGame':
            c['anim'] = int(i['img'])
        if on == 'oPlayer1':
            h = i['vars'].get('holdItem')
            if h is not None and int(h) in ids:
                c['held'] = HELD.get(names['O'][ids[int(h)]['obj']], 'OTHER')
    # global.drawHUD (not traced): false from the death (oPlayer1 Step :1459-1464, plife < 1 -> dead) to the next level
    c['visible'] = int(any(names['O'][i['obj']] == 'oPlayer1' and not int(i['vars'].get('dead', 0)) for i in insts))
    return c


SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'refs', 'hd', 'src')


def view555(g, names, tiles, hd, insts, kind, art=None):
    """the model's 320 x 240 view as bgr555 words (0: nothing), with the HUD when art (hudcheck.Art) is given"""
    import hudcheck
    vx, vy = view_after(hd, insts, names)
    view = viewlevel.compose(drawables(names, tiles, insts, g, kind, hd.get('blinkToggle', -1)), g, vx, vy)
    pal = g.meta['palette']
    v = hudcheck.View()
    for y in range(240):
        for x in range(320):
            v.px[y][x] = pal[view[y][x]] if view[y][x] else None
    if art:
        hudcheck.model(hud_case(hd, insts, names, vx, vy), art, 320, v)
    return v, (vx, vy)


def hostcmp(trace, names_path, gen, d, hud):
    import glob
    import json
    import struct
    import hudcheck
    g = viewlevel.Gen(gen)
    kind = kinds(g, SRC)
    art = hudcheck.Art(SRC) if hud else None
    hm = json.load(open(os.path.join(gen, 'hud.json')))
    pals = {1: g.meta['palette'], 2: hm['palette'], 3: hm['palette_yellow']}
    want = {}
    if d == '-':                                  # a stream from tests/game/host.c (out dir "-"): s32 rec + view
        inp = sys.stdin.buffer

        def frames():
            while True:
                h = inp.read(4)
                if len(h) < 4:
                    return
                yield struct.unpack('<i', h)[0], inp.read(320 * 240 * 2)
        stream = frames()
        nxt = next(stream, None)
    else:
        for f in glob.glob(os.path.join(d, 'v_*.bin')):
            want[int(os.path.basename(f)[2:-4])] = f
        stream = nxt = None
    names = tracer.load_names(names_path)
    tiles, total, bad, skip = [], 0, 0, []
    recs = list(tracer.records(open(trace, 'rb').read()))
    bts = blink_toggles(names, recs)
    for k, (hd, insts) in enumerate(recs):
        hd['blinkToggle'] = bts[hd['rec']]
        if hd['phase'] == 0:
            tiles = hd['tiles']
        r = hd['rec']
        raw = None
        if stream is not None:
            while nxt is not None and nxt[0] < r:
                nxt = next(stream, None)
            if nxt is None or nxt[0] != r:
                continue
            raw = nxt[1]
        elif r not in want:
            continue
        if hd['phase'] != 1:
            continue
        if k + 1 < len(recs) and recs[k + 1][0]['phase'] == 0:
            skip.append(r)                        # a room_goto in this step: the runner draws no frame
            continue
        v, cam = view555(g, names, tiles, hd, insts, kind, art)
        if raw is None:
            raw = open(want[r], 'rb').read()
        px = struct.unpack('<%dH' % (320 * 240), raw)
        n, bb = 0, [999, 999, -1, -1]
        for y in range(8, 232):
            row = v.px[y]
            for x in range(320):
                w = px[y * 320 + x]
                c = pals.get(w >> 8, [0] * 256)[w & 255] if w else 0
                if c != (row[x] or 0):
                    n += 1
                    bb = [min(bb[0], x), min(bb[1], y), max(bb[2], x), max(bb[3], y)]
        total += 1
        if n:
            bad += 1
            print(f'rec {r} camera {cam[0]},{cam[1]}: host list vs model {n} of {320 * 224} px differ, view x {bb[0]}-{bb[2]} '
                  f'y {bb[1]}-{bb[3]}')
    print(f'{total} frames compared, {bad} differ; not drawn by the runner (room change in the step): {skip}')
    return bad == 0


def model(trace, names_path, rec, gen, hud=False):
    g = viewlevel.Gen(gen)
    src = SRC
    names, tiles, hd, insts, _ = load(trace, names_path, rec)
    vx, vy = view_after(hd, insts, names)
    dr = drawables(names, tiles, insts, g, kinds(g, src), hd.get('blinkToggle', -1))
    view = viewlevel.compose(dr, g, vx, vy)
    scr = viewlevel.screen(view, g)
    if hud:
        import hudcheck
        pal = g.meta['palette']
        v = hudcheck.View()
        for y in range(240):
            for x in range(320):
                v.px[y][x] = pal[view[y][x]] if view[y][x] else None
        hudcheck.model(hud_case(hd, insts, names, vx, vy), hudcheck.Art(src), 320, v)
        scr = hudcheck.screen(v)
    return g, view, scr, (vx, vy)


def diff5(a, b, mask=None):
    a, b = a.convert('RGB').load(), b.convert('RGB').load()
    n = 0
    m = Image.new('L', (384, 224)) if mask else None
    for y in range(224):
        for x in range(384):
            if tuple(c >> 3 for c in a[x, y]) != tuple(c >> 3 for c in b[x, y]):
                n += 1
                if m:
                    m.putpixel((x, y), 255)
    if m:
        m.save(mask)
    return n


def main():
    a = sys.argv[1:]
    hud = '--hud' in a
    if a and a[0] == 'hostcmp':
        sys.exit(0 if hostcmp(a[1], a[2], a[3], a[4], hud) else 1)
    if len(a) < 6:
        sys.exit(__doc__)
    opt = lambda k: a[a.index(k) + 1] if k in a else None
    g, view, scr, cam = model(a[1], a[2], int(a[3]), a[4], hud)
    if a[0] == 'screen':
        scr.save(a[5])
        print(f'{a[5]}: camera {cam}')
        return
    n = diff5(scr, Image.open(a[5]), opt('--mask'))
    msg = f'rec {a[3]} camera {cam[0]},{cam[1]}: MAME vs model {n} of {384 * 224} px differ'
    fail = n != 0
    if opt('--shot'):
        s = opt('--shot')
        k = viewlevel.shot_diff(view, g, s, s.replace('.png', '.mdiff.png'))
        msg += f'; runner frame vs model {k} of {320 * 240} px'
        fail = fail or k != 0
    print(msg)
    sys.exit(1 if fail else 0)


if __name__ == '__main__':
    main()
