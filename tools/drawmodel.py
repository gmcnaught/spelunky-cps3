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
        320 x 240 view with the runner's own frame (TRACE_SHOT: application_surface, no GUI); --gui gui.png (with
        --hud) the model's view with the HUD and oTransition's text against the runner's frame with its GUI
        (tools/tracer.py TRACE_GUI); --dark a8: drawn as a dark level at alpha byte a8 (tests/game DARK=a8).
        The port's own changes (not in HD) are drawn on the MAME screen only, HD's for --gui: the game-over panel's
        prompt (PRESS ATTACK, docs/GAMELOOP.md section 3; HD: the runner's attack key X), the compass's bottom row
        8 lines higher (docs/ARCADE.md section 6)
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
import titlelogo  # noqa: E402
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
    blink. A dead player's invincible is not counted down (:1903), so an unchanged 30 is no new hit
    (c_swamp_drain, c_swamp_swim: the blink ended 30 records after the death). A room start creates the player
    again (characterCreateEvent: -1)."""
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
            if inv in (30, 60) and inv > prev:      # a dead player's invincible stays (Step :1903): no new hit
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
    state[0]['olmecIntro'] = olmec_intro(names, seen)
    return names, tiles, state[0], state[1], nxt


def olmec_intro(names, recs, fired=False):
    """rOlmec: the view follows oOlmec (hborder 0) from its Create until its Alarm_5 (:1-4: oPlayer1, hborder
    display_w / 2), i.e. while no record so far has had oOlmec's alarm 5 at 0 (the step it fired)"""
    hd = recs[-1][0] if recs else None
    if fired or not hd or names['R'].get(hd['room'], '') != 'rOlmec':
        return False
    for h, insts in recs:
        if names['R'].get(h['room'], '') != 'rOlmec':
            continue
        for i in insts:
            if names['O'][i['obj']] == 'oOlmec' and i['alarms'].get(5) == 0:
                return False
    return True


def room_bg(hd, names):
    """the room background: scrInitLevel :140-143 sets bgTemple when global.levelType is 3 (levels 13-16; rOlmec's
    own layer is bgTemple too); bgCave otherwise; rIntro's backgroundNight"""
    rname = names['R'].get(hd['room'], '')
    if rname in ROOM_BG:
        return ROOM_BG[rname]
    if rname == 'rOlmec' or (rname in LEVEL_ROOMS and 13 <= int(hd.get('currLevel', 0)) <= 16):
        return 'bgTemple'
    return 'bgCave'


def room_size(rname):
    return {'rLevel': (672, 544), 'rLevel2': (672, 608), 'rLevel3': (672, 672), 'rOlmec': (672, 880),
            'rIntro': (960, 240), 'rTitle': (768, 240), 'rHighscores': (320, 256)}.get(rname, (320, 240))


# the view's target per room (the room's view 0, refs/hd/src/rooms/<room>.yy; the levels: oScreen's oPlayer1) and
# its vertical border; None: the room's code places the view (rTitle: oTitle Step)
VIEW_TARGET = {'rIntro': ('oPDummy3', 160), 'rTitle': (None, 0)}
ROOM_BG = {'rIntro': 'backgroundNight', 'rEnd3': None, 'rCredits2': None}   # None: no background (bgClouds layers)
# the ending's two bgClouds layers (480 x 200, not tiled horizontally): (layer depth, x, y); rEnd3's second layer is
# tiled vertically (y 0 and 200 cover the view), rCredits2's not (refs/hd/src/rooms/<room>/<room>.yy)
CLOUDS = {'rEnd3': [(2147483500, -160, 0), (2147483400, 160, 0), (2147483400, 160, 200)],
          'rCredits2': [(2147483500, -160, 0), (2147483400, 160, 0)]}
GUI_DEPTH = -1 << 60     # Draw GUI: after every Draw event
WHITE, YELLOW = (255, 255, 255), (255, 255, 0)


def view_after(hd, insts, names):
    """the view the frame after this record is drawn with: the record's view, then oPlayer1's follow (prun.c
    view_update: borders 160 / view_vborder, clamped to the room)"""
    rname = names['R'].get(hd['room'], '')
    vx, vy = int(hd['xview']), int(hd['yview'])
    W, H = room_size(rname)
    vb = 96 if rname in LEVEL_ROOMS else 0
    target, hb = 'oPlayer1', 160
    if rname in VIEW_TARGET:
        target, vb = VIEW_TARGET[rname]
    if hd.get('olmecIntro'):                   # oOlmec Create :37-40: view_object oOlmec, hborder 0, until Alarm_5
        target, hb = 'oOlmec', 0
    for i in insts:
        if names['O'][i['obj']] == target:
            import math
            x, y = math.floor(i['x']), math.floor(i['y'])
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


def pix(v):
    """the pixel a sprite at coordinate v starts on: ceil(v - 0.5) (round half down; tools/darkfade.py fit:
    build/trace/g_p7_dark_s18's oFlareSpark at y 112.8 is on row 113)"""
    import math
    return math.ceil(v - 0.5)


SAVE = None              # hostcmp --save dir
LOCSIGN = {o: 's' + o[1:] for o in ('oStartSign', 'oScoresSign', 'oQuitSign', 'oTutorialSign', 'oLevel5Sign',
                                    'oLevel9Sign', 'oLevel13Sign', 'oResetSign')}
DARK_FORCE = None        # cmp --dark a8: drawn as a dark level at that alpha byte (tests/game DARK=a8)


def alpha_byte(a):
    """draw_set_alpha(a)'s alpha byte for the front rooms' black rectangles (oIntro, oEnd3, oCredits2): a as a
    single-precision float, times 255, truncated (g_end_win_s7 record 1250: fadeLevel 0.7999999999999999 is drawn
    as a8 204, not 203; src/front front_fade the same)"""
    import struct
    f = struct.unpack('<f', struct.pack('<f', a))[0]
    return max(0, min(255, int(f * 255)))


def dark_a8(hd):
    """the alpha byte of oLevel's rectangle for this record, None when no dark level"""
    if DARK_FORCE is not None:
        return DARK_FORCE
    h = hd.get('hud') or {}
    if h.get('darkLevel', 0) > 0 and h.get('darkness', -1e9) > -1e8:
        return max(0, min(255, int(h['darkness'] * 255)))
    return None


def front_lpos():
    """room instance id -> its place in its layer, for the front rooms (build/gen/fronttables_rt.txt)"""
    if not hasattr(front_lpos, 'm'):
        front_lpos.m = {}
        p = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'gen', 'fronttables_rt.txt')
        if os.path.exists(p):
            for line in open(p):
                f = line.split()
                if f[0] == 'INST' and len(f) > 10:
                    front_lpos.m[int(f[1])] = int(f[10])
    return front_lpos.m


def drawables(names, tiles, insts, g, kind, blink=-1, a8=None, front=None, hud=None, room='', gui=False):
    out = []
    for k, (dep, X, Y) in enumerate(CLOUDS.get(room, [])):
        out.append((dep, -len(CLOUDS[room]) + k, 'frame', g.frame(g.sprid['bgClouds'], 0), X, Y, False))
    if gui and room == 'rCredits2' and hud and hud.get('oCredits2.drawStatus', -1e9) > -1e8:
        out += [(GUI_DEPTH, 0) + op for op in credits_ops(hud)]
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
    # oBloodTrail at depth 1) equal only so; the other 16 frames of the gate are equal either way.
    # In the front rooms (many room instances): a layer draws the instances made at run time first (newest first),
    # then its room instances in their order in the layer (build/trace/g_p8_boot_s7 record 820: rTitle's two logos
    # at depth 1 equal only so); tools/fronttables.py gives the layer positions
    lp = front_lpos()
    if any(i['id'] in lp for i in insts):
        seq = sorted(insts, key=lambda i: -(0x40000000 + i['id']) if i['id'] not in lp else -(0x3fffffff - lp[i['id']]))
    held = None                                   # oPlayer1 End Step (Step_2.gml), after the record: holdItem.depth
    for i in insts:                               # = 0 (51 climbing with jetpack / cape: not traced)
        if names['O'][i['obj']] == 'oPlayer1' and i['vars'].get('holdItem') is not None:
            held = int(i['vars']['holdItem'])
    for k, i in enumerate(seq):
        d = i.get('draw') or {}
        on = names['O'][i['obj']]
        if on == 'oLevel' and a8 is not None:   # the dark level's rectangle (oLevel Draw: draw_set_alpha)
            out.append((int(d.get('depth', -2)), base + k, 'dark', a8, 0, 0, False))
        if on == 'oIntro' and front is not None:  # objects/oIntro/Draw_0.gml: the fade, then the story's lines
            dep = int(d.get('depth', 0))
            fl = front['fadeLevel']
            out.append((dep, base + k, 'dark', alpha_byte(fl), 0, 0, False))
            for n, (txt, y) in enumerate(zip(front['str'], (100, 116, 132))):
                if front['drawStatus'] > n:
                    out.append((dep, base + k, 'text', txt, centred(txt), y, 'small', WHITE))
        if on == 'oEnd3' and hud and hud.get('oEnd3.drawStatus', -1e9) > -1e8:
            out += [(int(d.get('depth', 0)), base + k) + op for op in final_score_ops(hud)]
        if not d.get('visible', 1):
            continue
        kd = kind.get(on, 'SELF')
        depth = int(d.get('depth', g.obj[on]['depth']))
        if held is not None and i['id'] == held:
            depth = 0
        x, y = pix(i['x']), pix(i['y'])
        if on in titlelogo.MOVED:               # the port's title logo and its shadow, moved (tools/titlelogo.py)
            x += titlelogo.DX
        v = i['vars']
        ops = []

        def spr(name, img, X, Y, flip):
            if name in g.sprid:
                ops.append((g.frame(g.sprid[name], img), X, Y, flip))

        sn = names['S'].get(i['spr']) if i['spr'] >= 0 else None
        if sn is None and on in LOCSIGN:          # scripts/loadLocalizedSprites: sprite_add'ed English signs, the
            sn = LOCSIGN[on]                      # same pixels as the objects' own sprites (checked: 8 of 8)
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
        if kd in ('ITEM', 'DAMSEL') and v.get('cost', 0) > 0 and 'cimg' in v:
            # the price tag: draw_sprite_ext(global.sSmallCollectNew, cimg, x, y - 12, ..) (SPT4 traces: cimg)
            out.append((depth, base + k, 'tag', int(v['cimg']), x, y - 12, False))
    out.sort(key=lambda e: (-e[0], e[1]))
    return out


def final_score_ops(h):
    """scripts/showFinalScore(drawStatus, fadeOut) as oEnd3's Draw runs it (room coordinates, English, room_offset
    0: lblX 64, valX 224): the lines, the black rectangle at alpha fadeLevel while fadeOut, then the last line"""
    ds = h['oEnd3.drawStatus']
    ops = []

    def text(t, x, y, size, col):
        ops.append(('text', t, x, y, size, col))
    if ds > 0:
        text(tr('YOU MADE IT!'), centred(tr('YOU MADE IT!'), 'large'), 32, 'large', YELLOW)
    if ds > 1:
        text(tr('FINAL SCORE:'), centred(tr('FINAL SCORE:')), 56, 'small', YELLOW)
    if ds > 2:
        t = tr('$') + str(int(h['oEnd3.moneyCount']))
        text(t, centred(t, 'large'), 72, 'large', WHITE)
    if ds > 4:
        sec = int(h['time'] // 1000)
        m = 0
        while sec > 59:
            sec -= 60
            m += 1
        text(tr('TIME:  '), 64, 96, 'small', YELLOW)
        text(f'{m}:{sec:02d}', 224, 96, 'small', WHITE)
    if ds > 5:
        text(tr('KILLS:  '), 64, 96 + 8, 'small', YELLOW)
        text(str(int(h['kills'])), 224, 96 + 8, 'small', WHITE)
    if ds > 6:
        text(tr('SAVES:  '), 64, 96 + 16, 'small', YELLOW)
        text(str(int(h['damsels'])), 224, 96 + 16, 'small', WHITE)
    if h['oEnd3.fadeOut']:
        ops.append(('dark', alpha_byte(h['oEnd3.fadeLevel']), 0, 0, False))
    if ds == 8:
        t = tr('YOU SHALL BE REMEMBERED AS A HERO.')
        text(t, centred(t), 116, 'small', WHITE)
    return ops


CREDITS = {
    1: [('SPELUNKY', 'large', 1, 16, 16)],
    2: [('A GAME BY', 'small', 1, 16, 16), ('DEREK YU', 'small', 2, 32, 32)],
    3: [('PLATFORM ENGINE', 'small', 1, 16, 16), ('MARTIN PIECYK', 'small', 2, 32, 32),
        ('SOUND EFFECTS MADE USING', 'small', 1, 16, 48), ("DR PETTER'S SFXR", 'small', 2, 32, 64),
        ('SCREEN SCALING CODE', 'small', 1, 16, 80), ('CHEVYRAY', 'small', 2, 32, 96)],
    4: [('MUSIC BY', 'small', 1, 16, 16), ('GEORGE BUZINKAI', 'small', 2, 32, 32),
        ('JONATHAN PERRY', 'small', 2, 32, 40)],
    5: [('BETA TESTING BY', 'small', 1, 16, 16)] + [
        (n, 'small', 2, 144 if i >= 11 else 32, 32 + 8 * i - (88 if i >= 11 else 0)) for i, n in enumerate([
            'ANNABELLE K.', 'BENZIDO', 'CHUTUP', 'CORPUS', 'GENERALVALTER', 'GUERT', 'GRAHAM GORING', 'HAOWAN',
            'HIDEOUS', 'INANE', 'INCREPARE', 'KAO', 'MARK JOHNS', 'MELLY', 'PAUL ERES', 'SUPER JOE', 'TANTAN',
            'TEAM QUIGGAN', 'TERRY', 'XION', 'ZAPHOS'])],
    6: [('SPELUNKY CLASSIC HD BY', 'small', 1, 16, 16), ('YANCHARKIN', 'small', 2, 32, 32),
        ('CONTRIBUTORS', 'small', 1, 16, 48)] + [
        (n, 'small', 2, 32, 64 + 8 * i) for i, n in enumerate([
            'NKRAPIVIN', 'GRHEAVY', 'SPENCJO', 'GABRIEL ALBUQUERQUE FERREIRA', 'BAKUSTARVER', 'LERETARDATN',
            'MASTERPHW', 'BRNBOT3K', 'V9TN'])],
    7: [('THANKS FOR PLAYING!', 'small', 1, 16, 16), ('SEE YOU NEXT ADVENTURE!', 'small', 2, 32, 32)],
}
# drawCredits' translated lines (tr(); the names are not translated)
CREDITS_TR = {'A GAME BY', 'PLATFORM ENGINE', 'SOUND EFFECTS MADE USING', 'SCREEN SCALING CODE', 'MUSIC BY',
              'BETA TESTING BY', 'SPELUNKY CLASSIC HD BY', 'CONTRIBUTORS', 'THANKS FOR PLAYING!',
              'SEE YOU NEXT ADVENTURE!'}


def credits_ops(h):
    """scripts/drawCredits(c_yellow, c_white) as oCredits2's Draw GUI runs it (English: X1 16, X2 32, X3 144): the
    page's lines, then the black rectangle at alpha fadeLevel while fadeIn or fadeOut"""
    ops = []
    for t, size, c, x, y in CREDITS.get(int(h['oCredits2.drawStatus']), []):
        ops.append(('text', tr(t) if t in CREDITS_TR else t, x, y, size, YELLOW if c == 1 else WHITE))
    if h['oCredits2.fadeIn'] or h['oCredits2.fadeOut']:
        ops.append(('dark', alpha_byte(h['oCredits2.fadeLevel']), 0, 0, False))
    return ops


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
    player = any(names['O'][i['obj']] == 'oPlayer1' for i in insts)
    h = hd.get('hud')
    if h is None:
        # without TRACE_HUD: global.drawHUD is false from the death (oPlayer1 Step :1459-1464) to the next level
        c['visible'] = int(any(names['O'][i['obj']] == 'oPlayer1' and not int(i['vars'].get('dead', 0)) for i in insts))
        return c
    # TRACE_HUD records: the globals as the runner has them at oGamepad's End Step (showMessages counts
    # messageTimer down in the Draw GUI after it: a timer > 0 there is a drawn message)
    c['visible'] = int(bool(h['drawHUD'] > 0) and player)
    c['collect'] = int(h['collect']) if h['collect'] > -1e8 else 0
    c['blood_level'] = int(h['bloodLevel']) if h['bloodLevel'] > -1e8 else 0
    c['message_timer'] = int(h['messageTimer']) if h['messageTimer'] > -1e8 else 0
    for m in ('message1', 'message2'):
        v = h[m]
        c[m] = v if isinstance(v, str) else list(v)
        if not isinstance(v, str):
            c[m + '_yellow'] = [int(x) for x in h[m + '_hl']]
    return c


SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'refs', 'hd', 'src')


TAG = os.path.join(SRC, 'datafiles', 'locale', 'locales', 'en', 'images', 'small_collect.png')


def compose555(dr, g, vx, vy, W=320, H=240, bgname='bgCave'):
    """tools/viewlevel.py's compose as bgr555 words (None: nothing), plus the price tag ('tag': HD's
    small_collect.png, 20 frames side by side, origin 4, 4) and a dark level's rectangle ('dark': alpha byte a8 over
    everything drawn before it, faded from each pixel's 8-bit colour: gfx.json rgb, tools/darkfade.py)"""
    import hdsprites
    pal = g.meta['palette']
    rgbs = g.meta.get('rgb')
    first = g.meta['first_tile']
    img = [[None] * W for _ in range(H)]
    src = [[None] * W for _ in range(H)]        # the pixel's 8-bit colour (palette entry or the tag's)

    def blit_tile(tile, px, py, flip=False):
        t = tile - first
        tb = g.gfx[256 * t:256 * t + 256]
        for yy in range(16):
            Y = py + yy
            if 0 <= Y < H:
                row, srow = img[Y], src[Y]
                for xx in range(16):
                    X = px + xx
                    c = tb[16 * yy + (15 - xx if flip else xx)]
                    if c and 0 <= X < W:
                        row[X] = pal[c]
                        srow[X] = c

    def blit_frame(f, x, y, flip):
        p0, n = g.frm[f]
        for dx, dy, pw, ph, tile in g.pcs[p0:p0 + n]:
            for i in range(pw):
                for j in range(ph):
                    if flip:
                        blit_tile(tile + i * ph + j, x - dx - 16 * pw + 16 * (pw - 1 - i), y + dy + 16 * j, True)
                    else:
                        blit_tile(tile + i * ph + j, x + dx + 16 * i, y + dy + 16 * j)
    def blit_text(text, size, colour, x, y):
        """draw_text with HD's English sprite fonts (as tools/hudcheck.py draw_text), the colour blended in; the
        pixels keep their 8-bit colour for a later rectangle"""
        import hudcheck
        if not hasattr(compose555, 'art'):
            compose555.art = hudcheck.Art(SRC)
        im, w = (compose555.art.large, 16) if size == 'large' else (compose555.art.small, 8)
        p = im.load()
        for k, ch in enumerate(str(text)):
            i = ord(ch) - 32
            if not 0 <= i < 59:
                continue
            for j in range(w):
                Y = y + j
                if not 0 <= Y < H:
                    continue
                for ii in range(w):
                    X = x + k * w + ii
                    r, gg, b, a = p[i * w + ii, j]
                    if a >= 128 and 0 <= X < W:
                        rgb = (r * colour[0] // 255, gg * colour[1] // 255, b * colour[2] // 255)
                        img[Y][X] = hdsprites.bgr555(*rgb)
                        src[Y][X] = rgb
    tag = None
    if bgname:
        bg = g.sprid[bgname]
        bw, bh = g.spr[bg][0], g.spr[bg][1]
        for ry in range((vy // bh) * bh, vy + H, bh):
            for rx in range((vx // bw) * bw, vx + W, bw):
                blit_frame(g.spr[bg][9], rx - vx, ry - vy, False)
    for d in dr:
        if d[2] == 'cell':
            blit_tile(d[3], 16 * d[4] - vx, 16 * d[5] - vy)
        elif d[2] == 'frame':
            blit_frame(d[3], d[4] - vx, d[5] - vy, d[6])
        elif d[2] == 'text':                      # a Draw-event text (src/front): (text, x, y, size, colour) in room
            if d[0] == GUI_DEPTH:                 # coordinates; Draw GUI text in view coordinates
                blit_text(d[3], d[6], d[7], d[4], d[5])
            else:
                blit_text(d[3], d[6], d[7], d[4] - vx, d[5] - vy)
        elif d[2] == 'dark':
            a8 = d[3]
            for Y in range(H):
                row, srow = img[Y], src[Y]
                for X in range(W):
                    c = srow[X]
                    if c is not None:
                        rgb = tuple(rgbs[c]) if isinstance(c, int) else c
                        row[X] = hdsprites.bgr555(*((ch * (255 - a8) + 127) // 255 for ch in rgb))
                        srow[X] = None                    # one rectangle a frame
        else:
            if tag is None:
                tag = Image.open(TAG).convert('RGBA')
            tw = tag.width // 20
            k = d[3] % 20
            tp = tag.load()
            for yy in range(tag.height):
                for xx in range(tw):
                    r, gg, b, a = tp[tw * k + xx, yy]
                    X, Y = d[4] - 4 - vx + xx, d[5] - 4 - vy + yy
                    if a >= 128 and 0 <= X < W and 0 <= Y < H:
                        img[Y][X] = hdsprites.bgr555(r, gg, b)
                        src[Y][X] = (r, gg, b)
    return img


def trans_text(hd, insts, names):
    """oTransition's Draw GUI (objects/oTransition/Draw_64.gml, English, room_offset 0) from a TRACE_HUD record:
    [(text, x or None for centred, y, yellow)]"""
    h = hd.get('hud') or {}
    if h.get('drawLoot', -1e9) < -1e8:
        return []
    out = []
    if h.get('kissed', -1e9) > 0:
        out.append(('MY HERO!', None, 216, False))
    lv = int(hd['currLevel']) - 1
    out.append(('TUTORIAL CAVE COMPLETED!' if lv < 1 else f'LEVEL {lv} COMPLETED!', 32, 48, True))
    for k, t in enumerate(['TIME  = ', 'LOOT  = ', 'KILLS = ', 'MONEY = ']):
        out.append((t, 32, 64 + 16 * k, False))
    dl = int(h['drawLoot'])
    if dl >= 1 and not h['isLoot']:
        out.append(('NONE', 96, 80, False))
    if dl > -2:
        s, s2 = int(h['xtime'] // 1000), int(h['time'] // 1000)
        m, m2 = s // 60, s2 // 60
        s, s2 = s % 60, s2 % 60
        out.append((f'{m}:{s:02d} / {m2}:{s2:02d}', 96, 64, False))
    if dl == 2:
        if not h['isKills']:
            out.append(('NONE', 96, 96, False))
        out.append((f'${int(h["moneyCount"])} / ${int(hd["money"])}', 96, 112, False))
    return out


def tr(s):
    """scripts/tr: HD's English text (datafiles/locale/locales/en/text.json), upper case; the key when absent"""
    if not hasattr(tr, 'm'):
        import json
        tr.m = json.load(open(os.path.join(SRC, 'datafiles', 'locale', 'locales', 'en', 'text.json')))
    v = tr.m.get(s, '')
    return v.upper() if v else s


def centred(text, size='small'):
    """drawTextHCentered's x on the 320-px display: ceil((display_w - length * width) / 2)"""
    return -((len(text) * (16 if size == 'large' else 8) - 320) // 2)


def end_text(hd, insts, names, port=True):
    """scripts/showEndMessage, a level's part (oGame's Draw GUI after showMessages; English) from a record with the
    end block (TRACE_HUD): [(text, x, y, size, yellow)]. The prompt names the attack button on the port
    (docs/GAMELOOP.md section 3: src/draw end_out); HD names the key (scrGetKey(global.keyAttackVal): X)"""
    h = hd.get('hud') or {}
    ds = h.get('oGame.drawStatus', -1e9)
    if ds < -1e8 or names['R'].get(hd['room'], '') not in LEVEL_ROOMS:
        return []
    pl = next((i for i in insts if names['O'][i['obj']] == 'oPlayer1'), None)
    if pl is None or not int(pl['vars'].get('dead', 0)):
        return []
    out = []
    if ds > 0:
        out.append(('GAME OVER', 32 + 16, 'large', True))
    if ds > 1:
        out.append(('FINAL SCORE:', 64 + 16, 'small', True))
    if ds > 2:
        out.append((tr('$') + str(int(h['oGame.moneyCount'])), 72 + 16, 'large', False))
        prompt = 'PRESS ATTACK FOR HIGH SCORES.' if port else tr('PRESS ') + 'X' + tr(' FOR HIGH SCORES.')
        out.append((prompt, 120, 'small', True))
    return [(t, centred(t, size), y, size, yel) for t, y, size, yel in out]


def scores_text(hd, insts, names):
    """oHighscores' Draw GUI (objects/oHighscores/Draw_64.gml, English) for a cabinet's blank EEPROM (every score 0):
    [(text, x or None for centred, y, yellow)]"""
    if names['R'].get(hd['room']) != 'rHighscores':
        return []
    pl = next((i for i in insts if names['O'][i['obj']] == 'oPlayer1'), None)
    if pl is None:
        return []
    out = []
    if pl['y'] < 156:
        out.append(('SECRET CHALLENGES', 112 + (192 - 17 * 8) // 2, 32, True))
    else:
        # tr(): datafiles/locale/locales/en/text.json ("MONEY:  " -> "MONEY:   ", ...)
        out += [('TOP DEFILERS', 112 + (192 - 12 * 8) // 2, 32, True), ('MONEY:   0', 120, 48, False),
                ('KILLS:   0', 120, 64, False), ('SAVES:   0', 120, 80, False),
                ('STATISTICS', 112 + (192 - 10 * 8) // 2, 112, True), ('PLAYS:   0', 120, 128, False),
                ('DEATHS:  0', 120, 144, False), ('WINS:    0', 120, 160, False)]
    blk = [i for i in insts if names['O'][i['obj']] == 'oPushBlock']
    if blk and min(blk, key=lambda i: (i['x'] - 160) ** 2 + (i['y'] - 240) ** 2)['x'] > 160:
        out.append(('THIS WILL CLEAR EVERYTHING!', None, 216, True))
    return out


def view555(g, names, tiles, hd, insts, kind, art=None, port=True):
    """the model's 320 x 240 view as bgr555 words (None: nothing), with the HUD when art (hudcheck.Art) is given;
    port: the port's own changes (the MAME screen; False: HD's, the runner's GUI frames)"""
    import hudcheck
    vx, vy = view_after(hd, insts, names)
    v = hudcheck.View()
    rname = names['R'].get(hd['room'], '')
    h = hd.get('hud') or {}
    front = h if h.get('fadeLevel', -1e9) > -1e8 else None
    v.px = compose555(drawables(names, tiles, insts, g, kind, hd.get('blinkToggle', -1), dark_a8(hd), front, h, rname,
                                art is not None),
                      g, vx, vy, bgname=room_bg(hd, names))
    if art:
        hudcheck.model(hud_case(hd, insts, names, vx, vy), art, 320, v, port)
        for text, x, y, yel in trans_text(hd, insts, names) + scores_text(hd, insts, names):
            if x is None:
                x = -((len(text) * 8 - 320) // 2)
            hudcheck.draw_text(v, art, text, 'small', x, y, hudcheck.YELLOW if yel else hudcheck.WHITE)
        for text, x, y, size, yel in end_text(hd, insts, names, port):
            hudcheck.draw_text(v, art, text, size, x, y, hudcheck.YELLOW if yel else hudcheck.WHITE)
    return v, (vx, vy)


def shot555(v, shot, mask=None):
    """pixels of the view whose 5-bit colour differs from the runner's frame (first 320 columns)"""
    import hdsprites
    s = Image.open(shot).convert('RGB').load()
    n = 0
    m = Image.new('L', (320, 240)) if mask else None
    for y in range(240):
        for x in range(320):
            if (v.px[y][x] or 0) != hdsprites.bgr555(*s[x, y]):
                n += 1
                if m:
                    m.putpixel((x, y), 255)
    if m:
        m.save(mask)
    return n


def hostcmp(trace, names_path, gen, d, hud):
    import glob
    import json
    import struct
    import hudcheck
    g = viewlevel.Gen(gen)
    kind = kinds(g, SRC)
    art = hudcheck.Art(SRC) if hud else None
    hm = json.load(open(os.path.join(gen, 'hud.json')))
    pals = {1: g.meta['palette'], 2: hm['palette'], 3: hm['palette_yellow'], 4: g.meta['palette']}
    fb = open(os.path.join(gen, 'fade.bin'), 'rb').read()      # code 1 at fade a8 (tools/darkfade.py table)
    fades = [struct.unpack_from('<256H', fb, 512 * k) for k in range(256)]
    hfades = [struct.unpack_from('<256H', fb, 512 * (256 + k)) for k in range(256)]
    hyfades = [struct.unpack_from('<256H', fb, 512 * (512 + k)) for k in range(256)]   # code 6: c_yellow text faded

    def colour(w, fa8):
        code = w >> 8
        return (fades[fa8] if code == 1 else hfades[fa8] if code == 5 else hyfades[fa8] if code == 6 else
                pals.get(code, [0] * 256))[w & 255] if w else 0
    want = {}
    if d == '-':                                  # a stream from tests/game/host.c (out dir "-"): s32 rec + view
        inp = sys.stdin.buffer

        def frames():
            while True:
                h = inp.read(4)
                if len(h) < 4:
                    return
                a8 = struct.unpack('<i', inp.read(4))[0]
                yield struct.unpack('<i', h)[0], inp.read(320 * 240 * 2), a8
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
    olm_fired = False
    for k, (hd, insts) in enumerate(recs):
        hd['blinkToggle'] = bts[hd['rec']]
        hd['olmecIntro'] = olmec_intro(names, [(hd, insts)], olm_fired)   # olm_fired: alarm 5 seen at 0 before
        olm_fired = olm_fired or (names['R'].get(hd['room'], '') == 'rOlmec' and not hd['olmecIntro'])
        if hd['phase'] == 0:
            tiles = hd['tiles']
        r = hd['rec']
        raw = None
        fa8 = 0
        if stream is not None:
            while nxt is not None and nxt[0] < r:
                nxt = next(stream, None)
            if nxt is None or nxt[0] != r:
                continue
            raw = nxt[1]
            fa8 = nxt[2]
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
                c = colour(w, fa8)
                if c != (row[x] or 0):
                    n += 1
                    bb = [min(bb[0], x), min(bb[1], y), max(bb[2], x), max(bb[3], y)]
        total += 1
        if n and SAVE and bad < 3:                # --save dir: model / host views of the first differing frames
            from PIL import Image
            for nm, get in (('model', lambda x, y: v.px[y][x] or 0),
                            ('host', lambda x, y: colour(px[y * 320 + x], fa8))):
                im = Image.new('RGB', (320, 240))
                ip = im.load()
                for yy in range(240):
                    for xx in range(320):
                        w = get(xx, yy)
                        ip[xx, yy] = ((w & 31) << 3, (w >> 5 & 31) << 3, (w >> 10 & 31) << 3)
                im.save(os.path.join(SAVE, f'{nm}_{r}.png'))
        if n:
            bad += 1
            print(f'rec {r} camera {cam[0]},{cam[1]}: host list vs model {n} of {320 * 224} px differ, view x {bb[0]}-{bb[2]} '
                  f'y {bb[1]}-{bb[3]}')
    print(f'{total} frames compared, {bad} differ; not drawn by the runner (room change in the step): {skip}')
    return bad == 0


def model(trace, names_path, rec, gen, hud=False):
    """(the 320 x 240 view without the HUD, MAME's screen with the HUD if asked, the camera)"""
    import hudcheck
    g = viewlevel.Gen(gen)
    names, tiles, hd, insts, _ = load(trace, names_path, rec)
    kind = kinds(g, SRC)
    v, cam = view555(g, names, tiles, hd, insts, kind)
    vh = view555(g, names, tiles, hd, insts, kind, hudcheck.Art(SRC))[0] if hud else None
    scr = hudcheck.screen(vh if hud else v)
    model.with_gui = vh                           # the port's screen (MAME, jtcps3)
    model.hd_gui = lambda: view555(g, names, tiles, hd, insts, kind, hudcheck.Art(SRC), port=False)[0]
    return g, v, scr, cam


def sstext(im, col, row, text):
    """the SDK's 8 x 8 SS-layer font (cps3-testgame/sdk/src/font.h, white) at text cell (col, row) of the screen"""
    import re
    if not hasattr(sstext, 'font'):
        f = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'cps3-testgame', 'sdk', 'src',
                              'font.h')).read()
        sstext.font = [[int(v, 16) for v in re.findall(r'0x([0-9a-f]{2})', ln)] for ln in f.splitlines()
                       if ln.strip().startswith('{0x')]
    p = im.load()
    for k, ch in enumerate(text):
        g = sstext.font[ord(ch) - 32] if 32 <= ord(ch) < 96 else [0] * 8
        for y in range(8):
            for x in range(8):
                if g[y] >> (7 - x) & 1:
                    X, Y = 8 * (col + k) + x, 8 * row + y
                    if X < 384 and Y < 224:
                        p[X, Y] = (255, 255, 255)


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
    global DARK_FORCE, SAVE
    if '--save' in a:
        SAVE = a[a.index('--save') + 1]
    if '--dark' in a:
        DARK_FORCE = int(a[a.index('--dark') + 1])
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
    for k, w in enumerate(a):                     # --sstext col,row,TEXT: the shell's SS text layer (src/shell: the
        if w == '--sstext':                       # attract's credit line), over everything, not zoomed
            col, row, text = a[k + 1].split(',', 2)
            sstext(scr, int(col), int(row), text)
    n = diff5(scr, Image.open(a[5]), opt('--mask'))
    msg = f'rec {a[3]} camera {cam[0]},{cam[1]}: MAME vs model {n} of {384 * 224} px differ'
    fail = n != 0
    if opt('--shot'):
        s = opt('--shot')
        k = shot555(view, s, s.replace('.png', '.mdiff.png'))
        msg += f'; runner frame vs model {k} of {320 * 240} px'
        fail = fail or k != 0
    if opt('--gui') and hud:
        s = opt('--gui')
        k = shot555(model.hd_gui(), s, s.replace('.png', '.mdiff.png'))
        msg += f'; runner GUI frame vs model {k} px'
        fail = fail or k != 0
    print(msg)
    sys.exit(1 if fail else 0)


if __name__ == '__main__':
    main()
