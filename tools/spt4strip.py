#!/usr/bin/env python3
"""SPT4 trace (tools/tracer.py TRACE_HUD / TRACE_SND) -> its SPT3 bytes (the per-instance cimg and the extension block
removed, magic SPT3), compared with a default trace: the flags must not change the game.
    tools/spt4strip.py <spt4.bin> <default spt3.bin>     prints identical / DIFFERENT"""
import struct, sys
M3, M4 = 0x33545053, 0x34545053
def strip(data):
    out = bytearray(); o = 0; H = struct.Struct('<IBiiii6dI')
    while o < len(data):
        h = H.unpack_from(data, o); assert h[0] == M4
        out += struct.pack('<I', M3) + data[o+4:o+H.size]; o += H.size
        out += data[o:o+24]; o += 24
        for _ in range(h[-1]):
            s = o
            o += 32 + 45
            (am,) = struct.unpack_from('<H', data, o); o += 2 + 8 * bin(am).count('1')
            vf = data[o]; o += 1 + 8 * bin(vf & 3).count('1')
            (_, vm) = struct.unpack_from('<dI', data, o); o += 12 + 8 * bin(vm).count('1')
            out += data[s:o]
            o += 8                                   # SPT4: cimg
        if h[1] == 0:
            s = o; (tn,) = struct.unpack_from('<I', data, o); o += 4
            for _ in range(tn): o = data.index(b'\0', o) + 1 + 56
            out += data[s:o]
        (ne,) = struct.unpack_from('<I', data, o); out += data[o:o+4+4*ne]; o += 4 + 4 * ne
        (fl,) = struct.unpack_from('<I', data, o); o += 4   # extension block: dropped
        if fl & 1:
            o += 32
            for _ in range(2):
                t = data[o]; o += 1
                if t:
                    (n,) = struct.unpack_from('<I', data, o); o += 4
                    for _ in range(n): o = data.index(b'\0', o) + 1
                else: o = data.index(b'\0', o) + 1
                (n,) = struct.unpack_from('<I', data, o); o += 4 + 8 * n
            if fl & 4:
                o += 72                              # the transition block
            if fl & 8:
                o += 24                              # the level block
            if fl & 16:
                o += 16                              # the front block
                for _ in range(3): o = data.index(b'\0', o) + 1
        if fl & 2:
            (n,) = struct.unpack_from('<I', data, o); o += 4
            for _ in range(n): o += 1; o = data.index(b'\0', o) + 1 + 8
    return bytes(out)
a = strip(open(sys.argv[1], 'rb').read()); b = open(sys.argv[2], 'rb').read()
print(sys.argv[1], 'stripped', len(a), 'default', len(b), 'identical' if a == b else 'DIFFERENT')
