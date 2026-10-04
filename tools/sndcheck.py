#!/usr/bin/env python3
"""P6 check (scripts/sound_check.sh): tests/sound's jukebox in MAME against the sound chip model.
    sndcheck.py <snd dir> <sndlog> <mame.wav>
1. Key-ons: every key-off -> on edge in the register log, identified by its start address (snd.h), plays the sounds
   in table order, each once, music on voice 0 and effects on voice 1.
2. Audio: the chip computed from the logged registers and snd.bin as MAME's cps3_a.cpp does (signed 8-bit sample x
   16-bit volume / 2^23 per voice; step in 1/4096 samples; loop / end) must equal MAME's WAV (written at the chip's
   rate, 37,286 Hz) within 1 LSB at lag 0. The model is ../maldita.castilla-cps3/tools/snd3check.py's (C4).

    sndcheck.py rules <snd dir> <script.txt> <sndlog> <mame.wav>
tests/sndrules (docs/AUDIO.md): the register log cut into frames by the test's frame marks (word 0x81) as events
(key-on: voice, sound, loop, volume; key-off; volume / step written to a keyed voice) must equal the events of this
file's model of the audio rules run on the script; and the audio check above on the whole run.
"""
import os
import re
import sys
import wave

import numpy as np

CHIP = 42954545 // 3 // 384
BASE = 0x400000


def swap(v):
    return (v >> 16) | ((v << 16) & 0xffffffff)


def synth(events, img, total):
    """the chip from (time, keys, regs) events (each a snapshot after a write) as cps3_a.cpp computes it"""
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
    return out


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
    out = synth(events, img, n)
    err = np.abs(pcm - out) * 32768
    ok2 = err.max() <= 1.0 and np.dot(out, out) > 0
    print(f'audio: {n / rate:.1f} s, model peak {np.abs(out).max():.3f}, max error {err.max():.2f} LSB '
          f'({"PASS" if ok2 else "FAIL"}: every sample within 1 LSB at lag 0)')
    print('PASS' if ok1 and ok2 else 'FAIL')
    sys.exit(0 if ok1 and ok2 else 1)


SCRIPT_TAIL = 720                       # tests/sndrules/main.c TAIL
LEVEL_GAIN = [round(0x4000 * (2000 + 8000 * (v / 18)) / 10000) for v in range(18)]
INIT_FX = ('xignite xteleport xjetpack xwhip xjump xthrow xclimb1 xclimb2 xshotgun xbowpull xsteps xblink1 xblink2 xhit '
           'xhurt xdie xcoin xgem xpickup xchestopen xpush xmattockbreak xtrap xclick xbreak xthud xthump xcrunch '
           'xsplash xflame xexplosion xarrowtrap xboing xdamsel xkiss xghost xbat xgiantspider xspiderjump xfrog '
           'xzombie xmonkey xalert xcavemandie xalien xlaser xlasercharge xsmallexplode xpsychic xyetiyell xbigjump '
           'xslam xpfall xtfall').split()


class Rules:
    """docs/AUDIO.md's rules: voice 0 the tracks (a new one replaces), 1-15 effects stacking; when full, the lowest
    priority / oldest voice is taken if its priority <= the new one's, else the call is dropped; a sound played once
    ends after ceil(samples * 60 / 32000) frames; pause holds voices (step 0, volume 0) and their frames."""

    def __init__(self, rows, music_vol=15, sound_vol=15):
        self.rows = rows
        self.names = {r[6]: k for k, r in enumerate(rows)}
        self.gain = [r[4] for r in rows]
        for k, r in enumerate(rows):
            if r[6].startswith('m'):
                self.gain[k] = LEVEL_GAIN[music_vol]
        for n in INIT_FX:
            self.gain[self.names[n]] = LEVEL_GAIN[sound_vol]
        self.frames = [-((r[0] - r[1]) * 60 // 32000) for r in rows]          # ceil
        self.v = [None] * 16                    # (sound, prio, left, age, paused); left -1 = looping
        self.starts = 0
        self.music_on = True
        self.ev = []
        self.f = 0

    def off(self, vs):
        for k in vs:
            self.v[k] = None
            self.ev.append((self.f, 'off', k))

    def start(self, k, s, prio, loop):
        if self.v[k] is not None:
            self.off([k])
        self.v[k] = [s, prio, -1 if loop else self.frames[s], self.starts, False]
        self.starts += 1
        self.ev.append((self.f, 'on', k, self.rows[s][6], int(bool(loop)), self.gain[s]))

    def frame(self, f):
        self.f = f
        self.off([k for k in range(16) if self.v[k] and not self.v[k][4] and self.v[k][2] > 0
                  and self._dec(k) == 0])

    def _dec(self, k):
        self.v[k][2] -= 1
        return self.v[k][2]

    def playing(self, s):
        return any(x and x[0] == s for x in self.v)

    def aplay(self, s, prio, loop):
        if self.rows[s][6].startswith('m'):
            return self.start(0, s, prio, loop)
        free = [k for k in range(1, 16) if self.v[k] is None]
        if free:
            return self.start(free[0], s, prio, loop)
        k = min(range(1, 16), key=lambda k: (self.v[k][1], self.v[k][3]))
        if self.v[k][1] <= prio:
            self.start(k, s, prio, loop)

    def op(self, op, a):
        s = self.names[a[0]] if a and op != 'musicflag' else None
        if op == 'play':
            self.aplay(s, 2, 0)
        elif op == 'guard':
            if not self.playing(s):
                self.aplay(s, 2, 0)
        elif op == 'music':
            if self.music_on and not self.playing(s):
                self.aplay(s, 100, int(a[1]))
        elif op == 'aplay':
            self.aplay(s, int(a[1]), int(a[2]))
        elif op == 'stop':
            self.off([k for k in range(16) if self.v[k] and self.v[k][0] == s])
        elif op == 'stopmusic':
            self.off([0] if self.v[0] else [])
        elif op == 'stopall':
            self.off([k for k in range(16) if self.v[k]])
        elif op == 'pause':
            for k in range(16):
                if self.v[k] and not self.v[k][4]:
                    self.v[k][4] = True
                    self.ev += [(self.f, 'step', k, 0), (self.f, 'vol', k, 0)]
        elif op == 'resume':
            for k in range(16):
                if self.v[k] and self.v[k][4]:
                    self.v[k][4] = False
                    s = self.v[k][0]
                    self.ev += [(self.f, 'step', k, self.rows[s][3]), (self.f, 'vol', k, self.gain[s])]
        elif op in ('gain', 'level'):
            g = int(a[1]) if op == 'gain' else LEVEL_GAIN[int(a[1])]
            self.gain[s] = g
            self.ev += [(self.f, 'vol', k, g) for k in range(16) if self.v[k] and self.v[k][0] == s
                        and not self.v[k][4]]
        elif op == 'musicflag':
            self.music_on = bool(int(a[0]))


def log_events(logp, rows):
    """the register log as rules events, plus (time, keys, regs) snapshots after every write for synth()"""
    by_start = {r[0]: r[6] for r in rows}
    regs = [[0] * 8 for _ in range(16)]
    keys, f, ev, snaps = 0, -1, [], []
    for line in open(logp):
        t, off, data = line.split()
        t, off, data = float(t), int(off), int(data, 16)
        if off == 0x81:
            f = data
            continue
        if off < 0x80:
            v, r = off // 8, off % 8
            regs[v][r] = data
            if keys & (1 << v) and r in (3, 7):
                if r == 7:
                    lo, hi = data & 0xffff, data >> 16
                    ev.append((f, 'vol', v, lo if lo == hi else ('unequal', hi, lo)))
                else:
                    ev.append((f, 'step', v, data >> 16))
        elif off == 0x80:
            new = data >> 16
            for v in range(16):
                b = 1 << v
                if keys & b and not new & b:
                    ev.append((f, 'off', v))
            for v in range(16):
                b = 1 << v
                if new & b and not keys & b:
                    r = regs[v]
                    lo, hi = r[7] & 0xffff, r[7] >> 16
                    ev.append((f, 'on', v, by_start.get(swap(r[1]) - BASE, '?'), r[2] & 1,
                               lo if lo == hi else ('unequal', hi, lo)))
            keys = new
        else:
            continue
        snaps.append((t, keys, [x[:] for x in regs]))
    return ev, snaps


def rules_main():
    snd, script, logp, wav = sys.argv[2:6]
    rows = [tuple(int(m.group(k)) for k in range(1, 7)) + (m.group(7),) for m in re.finditer(
        r'\{ (\d+)u, (\d+)u, (\d+)u, (\d+), (-?\d+), (\d) \},\s*/\* (\w+) \*/', open(f'{snd}/snd.h').read())]
    steps = []
    for line in open(script):
        w = line.split('#')[0].split()
        if w:
            steps.append((int(w[0]), w[1], w[2:]))
    # src/snd/snd.c's frames by reciprocal equal ceil(n * 3 / 1600) for every length the flash can hold
    n = np.arange(1 << 24, dtype=np.uint64)
    recip = (((n * 3 + 1599) & 0xffffffff) * 2748779070) >> 42
    ok0 = bool(np.array_equal(recip, (n * 3 + 1599) // 1600))
    print(f'frames reciprocal: {"exact" if ok0 else "WRONG"} for lengths below 2^24')
    m = Rules(rows)
    i = 0
    for f in range(steps[-1][0] + SCRIPT_TAIL + 1):
        m.frame(f)
        while i < len(steps) and steps[i][0] == f:
            m.op(steps[i][1], steps[i][2])
            i += 1
    got, snaps = log_events(logp, rows)
    out = os.path.join(os.path.dirname(logp), '')
    open(out + 'expected.txt', 'w').write(''.join(' '.join(map(str, e)) + '\n' for e in m.ev))
    open(out + 'got.txt', 'w').write(''.join(' '.join(map(str, e)) + '\n' for e in got))
    ok1 = got == m.ev
    first = next((k for k, (a, b) in enumerate(zip(got, m.ev)) if a != b), min(len(got), len(m.ev)))
    print(f'events: {len(got)} logged, {len(m.ev)} expected ({sum(e[1] == "on" for e in m.ev)} key-ons, '
          f'{sum(e[1] == "off" for e in m.ev)} key-offs): {"equal" if ok1 else f"differ from event {first}"}')
    if not ok1:
        print('  got     ', got[first:first + 3], '\n  expected', m.ev[first:first + 3])
    w = wave.open(wav)
    rate, nch, nf = w.getframerate(), w.getnchannels(), w.getnframes()
    pcm = np.frombuffer(w.readframes(nf), np.int16).reshape(-1, nch)[:, 0].astype(np.float64) / 32768
    if rate != CHIP:
        sys.exit(f'WAV at {rate} Hz: record at {CHIP}')
    img = np.frombuffer(open(f'{snd}/snd.bin', 'rb').read(), np.int8).astype(np.int64)
    model = synth(snaps, img, nf)
    err = np.abs(pcm - model) * 32768
    ok2 = err.max() <= 1.0 and np.dot(model, model) > 0
    print(f'audio: {nf / rate:.1f} s, model peak {np.abs(model).max():.3f}, max error {err.max():.2f} LSB '
          f'({"PASS" if ok2 else "FAIL"}: every sample within 1 LSB at lag 0)')
    ok = ok0 and ok1 and ok2
    print('PASS' if ok else 'FAIL')
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'rules':
        rules_main()
    else:
        main()
