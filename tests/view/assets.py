#!/usr/bin/env python3
"""tests/view's flash: tools/hdsprites.py's tiles at GFX_TILES_AT (a blank tile first, for empty tilemap cells:
tile GFX_FIRST_TILE - 1) and its palette at GFX_PAL_AT.     assets.py <gen dir> <flash.bin>"""
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'cps3-testgame', 'sdk',
                                'tools'))
from cps3asset import Flash  # noqa: E402

gen, out = sys.argv[1], sys.argv[2]
meta = json.load(open(os.path.join(gen, 'gfx.json')))
f = Flash()
f.tiles(meta['tiles_at'] - 256, bytes(256) + open(os.path.join(gen, 'gfx.bin'), 'rb').read())
f.colours(meta['pal_at'], meta['palette'])
f.write(out)
