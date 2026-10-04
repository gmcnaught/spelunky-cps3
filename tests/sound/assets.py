#!/usr/bin/env python3
"""tests/sound's flash: tools/hdsound.py's snd.bin at offset 0 (SND_AT), inside the 16 MB the chip addresses.
    assets.py <snd.bin> <flash.bin>"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'cps3-testgame', 'sdk',
                                'tools'))
from cps3asset import Flash, SAMPLE_MAX  # noqa: E402

data = open(sys.argv[1], 'rb').read()
if len(data) > SAMPLE_MAX:
    raise SystemExit('snd.bin past the 16 MB the sound chip addresses')
f = Flash()
f._put(0, data)                         # signed 8-bit samples, the chip reads byte a (cps3asset.Flash.sample's order)
f.write(sys.argv[2])
