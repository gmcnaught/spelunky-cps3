#!/usr/bin/env python3
"""P6 check (scripts/sound_check.sh): tests/sound's jukebox in MAME against the sound chip model.
    sndcheck.py <snd dir> <sndlog> <mame.wav>
1. Key-ons: every key-off -> on edge in the register log, identified by its start address (snd.h), plays the sounds
   in table order, each once, music on voice 0 and effects on voice 1.
2. Audio: the chip computed from the logged registers and snd.bin as MAME's cps3_a.cpp does (signed 8-bit sample x
   16-bit volume / 2^23 per voice; step in 1/4096 samples; loop / end) must equal MAME's WAV (written at the chip's
   rate, 37,286 Hz) within 1 LSB at lag 0. The model is ../maldita.castilla-cps3/tools/snd3check.py's (C4).
"""
import re
import sys
import wave

import numpy as np

CHIP = 42954545 // 3 // 384
BASE = 0x400000


def swap(v):
    return (v >> 16) | ((v << 16) & 0xffffffff)


def main():
    snd, logp, wav = sys.argv[1:4]
    rows = [tuple(int(m.group(k)) for k in range(1, 7)) + (m.group(7),) for m in re.finditer(
        r'\{ (\d+)u, (\d+)u, (\d+)u, (\d+), (-?\d+), (\d) \},\s*/\* (\w+) \*/', open(f'{snd}/snd.h').read())]
    by_start = {r[0]: k for k, r in enumerate(rows)}
    img = np.frombuffer(open(f'{snd}/snd.bin', 'rb').read(), np.int8).astype(np.int64)
    regs = [[0] * 8 for _ in range(16)]
    keys, events, got, bad_voice = 0, [], [], 0
    for line in open(logp):
        t, off, data = line.split()
        t, off, data = float(t), int(off), int(data, 16)
        if off < 0x80:
            regs[off // 8][off % 8] = data
            continue
        if off != 0x80:
            continue
        new = data >> 16
        for v in range(16):
            if new & (1 << v) and not keys & (1 << v):
                s = by_start.get(swap(regs[v][1]) - BASE, -1)
                got.append(s)
                if s < 0 or (v == 0) != bool(rows[s][5]):
                    bad_voice += 1
        events.append((t, new, [r[:] for r in regs]))
        keys = new
    ok1 = got == list(range(len(rows))) and bad_voice == 0
    print(f'key-ons: {len(got)} of {len(rows)} sounds, in table order: {got == list(range(len(rows)))}, '
          f'voices {"as assigned" if bad_voice == 0 else f"{bad_voice} wrong"}')

    w = wave.open(wav)
    rate, nch, n = w.getframerate(), w.getnchannels(), w.getnframes()
    pcm = np.frombuffer(w.readframes(n), np.int16).reshape(-1, nch)[:, 0].astype(np.float64) / 32768
    if rate != CHIP:
        sys.exit(f'WAV at {rate} Hz: record at {CHIP}')
    total = n
    out = np.zeros(total)
    st = [{'on': False, 'tot': 0, 'dead': False} for _ in range(16)]
    cur = [[0] * 8 for _ in range(16)]
    marks = [(int(np.ceil(t * CHIP)), k, r) for t, k, r in events] + [(total, None, None)]
    prev, keys = 0, 0
    for at, newkeys, newregs in marks:
        at = min(at, total)
        if at > prev:
            N = at - prev
            for v in range(16):
                s = st[v]
                if not s['on'] or s['dead']:
                    continue
                r = cur[v]
                start, end = swap(r[1]) - BASE, swap(r[5]) - BASE
                loop = ((r[3] & 0xffff) | ((r[4] << 16) & 0xffff0000)) - BASE
                step = r[3] >> 16
                vol = r[7] & 0xffff
                vol = vol - 0x10000 if vol >= 0x8000 else vol
                j = np.arange(N, dtype=np.int64)
                a = start + ((s['tot'] + j * step) >> 12)
                if r[2] & 1:
                    over = a >= end
                    a[over] = loop + (a[over] - end) % (end - loop)
                    keep = N
                else:
                    stop = np.nonzero(a >= end)[0]
                    keep = int(stop[0]) if len(stop) else N
                    if len(stop):
                        s['dead'] = True
                out[prev:prev + keep] += img[a[:keep]] * vol / 8388608
                s['tot'] += N * step
            prev = at
        if newkeys is None:
            break
        for v in range(16):
            if newkeys & (1 << v) and not keys & (1 << v):
                st[v] = {'on': True, 'tot': 0, 'dead': False}
            elif not newkeys & (1 << v):
                st[v]['on'] = False
        cur = newregs
        keys = newkeys
    err = np.abs(pcm - out) * 32768
    ok2 = err.max() <= 1.0 and np.dot(out, out) > 0
    print(f'audio: {n / rate:.1f} s, model peak {np.abs(out).max():.3f}, max error {err.max():.2f} LSB '
          f'({"PASS" if ok2 else "FAIL"}: every sample within 1 LSB at lag 0)')
    print('PASS' if ok1 and ok2 else 'FAIL')
    sys.exit(0 if ok1 and ok2 else 1)


if __name__ == '__main__':
    main()
