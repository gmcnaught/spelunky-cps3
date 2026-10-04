#!/usr/bin/env python3
"""tests/hud's flash: tools/hdsprites.py's tiles and palette (as tests/view: a blank tile first) and tools/hudart.py's
HUD tiles and its two palettes (colour codes 2 and 3).     assets.py <gen dir> <flash.bin>"""
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'cps3-testgame', 'sdk',
                                'tools'))
from cps3asset import Flash  # noqa: E402

gen, out = sys.argv[1], sys.argv[2]
meta = json.load(open(os.path.join(gen, 'gfx.json')))
hud = json.load(open(os.path.join(gen, 'hud.json')))
f = Flash()
f.tiles(meta['tiles_at'] - 256, bytes(256) + open(os.path.join(gen, 'gfx.bin'), 'rb').read())
f.colours(meta['pal_at'], meta['palette'])
f.tiles(hud['tiles_at'], open(os.path.join(gen, 'hud.bin'), 'rb').read())
f.colours(hud['pal_at'], hud['palette'] + hud['palette_yellow'])
f.write(out)
