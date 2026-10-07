#!/usr/bin/env python3
"""Sound calls of the C play loop against the HD runner's, record by record (the gate for src/game's sound hooks,
docs/AUDIO.md; the runner's log: tools/tracer.py TRACE_SND=1, SPT4 records).

    tools/sndcmp.py <trace.bin> <c.log> [--max N] [--strict]   exit 0 when every record's calls are equal, apart from
                                                                the runner-audio-timed calls below (--strict: those too)
    tools/sndcmp.py emit <trace.bin> > c.log             the trace's calls in the C log format (a self-check)

C log format (e.g. test/host/playhost printing them; one line per call, in call order, and its record lines):
    SND <kind> <asset> <arg>
        kind   the GML call: playSound, playMusic, startMusic, stopAllMusic, setSoundVol, audio_stop_sound,
               audio_pause_all, audio_resume_all, audio_stop_all, audio_play_sound (tracer.SND_KINDS)
        asset  HD's asset name as tools/hdsound.py names it (xhit, mCave: enum snd without SND_), "-" for none
        arg    playMusic: loop (0 / 1); setSoundVol: the GML volume argument (2000 + 8000 * (v / 18), 10000, 0);
               audio_play_sound: priority * 2 + loop; else 0
    R <rec> ...                  playhost's record line (only the first two fields are read)
The SND lines before a record line belong to that record, as the runner logs the calls made since the previous
record. The calls are compared in order: kind and asset exactly, arg within 1e-6 (relative).

audio_is_playing gates a few calls (moveTo's xpush, the bow's xbowpull, the yeti king's xyetiyell). The runner's
answer depends on its audio thread under the container's null sink, not on the steps: Observed, p4_push_rope seed 365
recorded three times gave xpush at records 24 and 33 once and at 24 only twice (src/snd's voice model: 24 only, the
asset's 0.62 s); the batch-end traces (TRACE_DEACT 32, 2026-10-07) recorded twice gave 27 and 24 runner calls on the
same build (xpush at records 29, 35, 45, 51 in the first) with the play state ROUTE equal, and oLavaSpray's xflame
(scripts/end_host.sh) varies the same way. So a record whose calls differ only in playSound of AUDIO_TIMED is
counted apart ("runner-audio-timed"), not as a difference; every other call must still match. --strict counts them.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tracer  # noqa: E402


def trace_calls(path):
    out = {}
    for hd, _ in tracer.records(open(path, 'rb').read()):
        if 'snd' not in hd:
            raise SystemExit(f'{path}: record {hd["rec"]} has no sound log (record it with TRACE_SND=1)')
        out[hd['rec']] = [(k, a or '-', x) for k, a, x in hd['snd']]
    return out


def c_calls(path):
    out, cur = {}, []
    for line in open(path):
        w = line.split()
        if not w:
            continue
        if w[0] == 'SND':
            cur.append((w[1], w[2], float(w[3]) if len(w) > 3 else 0.0))
        elif w[0] == 'R' and len(w) > 1:
            out[int(w[1])] = cur
            cur = []
    return out


AUDIO_TIMED = ('xflame', 'xpush', 'xbowpull', 'xyetiyell')   # playSound gated by audio_is_playing in HD (above)


def untimed(calls):
    return [x for x in calls if not (x[0] == 'playSound' and x[1] in AUDIO_TIMED)]


def same(a, b):
    return a[0] == b[0] and a[1] == b[1] and abs(a[2] - b[2]) <= 1e-6 * max(1.0, abs(a[2]), abs(b[2]))


def main():
    a = sys.argv[1:]
    if len(a) >= 2 and a[0] == 'emit':
        for r, calls in sorted(trace_calls(a[1]).items()):
            for k, s, x in calls:
                print(f'SND {k} {s} {x!r}')
            print(f'R {r} -')
        return
    if len(a) < 2:
        sys.exit(__doc__)
    mx = int(a[a.index('--max') + 1]) if '--max' in a else 10
    strict = '--strict' in a
    t, c = trace_calls(a[0]), c_calls(a[1])
    recs = sorted(set(t) & set(c))
    bad, timed, n = [], {}, 0
    eq = lambda u, v: len(u) == len(v) and all(same(x, y) for x, y in zip(u, v))
    for r in recs:
        n += len(t[r])
        tr, cr = t[r], c[r]
        if not eq(tr, cr):
            if not strict and eq(untimed(tr), untimed(cr)):
                for x in tr + cr:
                    if x[0] == 'playSound' and x[1] in AUDIO_TIMED:
                        timed[x[1]] = timed.get(x[1], 0) + 1
                timed.setdefault('_recs', []).append(r)
                continue
            bad.append(r)
            if len(bad) <= mx:
                k = next((i for i, (x, y) in enumerate(zip(tr, cr)) if not same(x, y)), min(len(tr), len(cr)))
                print(f'rec {r}: call {k}: runner {tr[k] if k < len(tr) else "(none)"}, C {cr[k] if k < len(cr) else "(none)"}'
                      f' ({len(tr)} vs {len(cr)} calls)')
    only = sorted(set(t) ^ set(c))
    tr_ = timed.pop('_recs', [])
    print(f'{len(recs)} records compared ({n} runner calls), {len(bad)} differ'
          + (f'; {len(tr_)} differ only in runner-audio-timed calls ('
             + ', '.join(f'{k} {v}' for k, v in sorted(timed.items())) + f'; first rec {tr_[0]})' if tr_ else '')
          + (f'; records in one log only: {len(only)} (first {only[0]})' if only else ''))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
