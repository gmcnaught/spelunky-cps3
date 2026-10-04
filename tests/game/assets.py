#!/usr/bin/env python3
"""tests/game's flash: tools/hdsound.py's samples at 0 (SND_AT), tools/hdsprites.py's tiles (a blank tile first, as
tests/view) and palette, tools/hudart.py's HUD tiles and palettes (as tests/hud).
    assets.py <gen dir> <snd.bin> <flash.bin>"""
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'cps3-testgame', 'sdk',
                                'tools'))
from cps3asset import Flash, SAMPLE_MAX  # noqa: E402

gen, snd, out = sys.argv[1:4]
meta = json.load(open(os.path.join(gen, 'gfx.json')))
hud = json.load(open(os.path.join(gen, 'hud.json')))
s = open(snd, 'rb').read()
if len(s) > SAMPLE_MAX or len(s) > meta['pal_at']:
    raise SystemExit('snd.bin overlaps the graphics')
f = Flash()
f._put(0, s)
f.tiles(meta['tiles_at'] - 256, bytes(256) + open(os.path.join(gen, 'gfx.bin'), 'rb').read())
f.colours(meta['pal_at'], meta['palette'])
f.tiles(hud['tiles_at'], open(os.path.join(gen, 'hud.bin'), 'rb').read())
f.colours(hud['pal_at'], hud['palette'] + hud['palette_yellow'])
f.write(out)
