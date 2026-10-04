#!/usr/bin/env python3
"""Sound calls of the C play loop against the HD runner's, record by record (the gate for src/game's sound hooks,
docs/AUDIO.md; the runner's log: tools/tracer.py TRACE_SND=1, SPT4 records).

    tools/sndcmp.py <trace.bin> <c.log> [--max N]       exit 0 when every record's calls are equal
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
    t, c = trace_calls(a[0]), c_calls(a[1])
    recs = sorted(set(t) & set(c))
    bad, n = [], 0
    for r in recs:
        n += len(t[r])
        tr, cr = t[r], c[r]
        if len(tr) != len(cr) or not all(same(x, y) for x, y in zip(tr, cr)):
            bad.append(r)
            if len(bad) <= mx:
                k = next((i for i, (x, y) in enumerate(zip(tr, cr)) if not same(x, y)), min(len(tr), len(cr)))
                print(f'rec {r}: call {k}: runner {tr[k] if k < len(tr) else "(none)"}, C {cr[k] if k < len(cr) else "(none)"}'
                      f' ({len(tr)} vs {len(cr)} calls)')
    only = sorted(set(t) ^ set(c))
    print(f'{len(recs)} records compared ({n} runner calls), {len(bad)} differ'
          + (f'; records in one log only: {len(only)} (first {only[0]})' if only else ''))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
