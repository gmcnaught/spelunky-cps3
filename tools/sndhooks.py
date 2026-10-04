#!/usr/bin/env python3
"""The sound calls the play loop still lacks: every audio call (playSound, playMusic, startMusic, stopAllMusic,
audio_*) in the GML of the objects and scripts src/game translates, with the src/snd call that replaces it
(docs/AUDIO.md §1) and the asset's enum (build/snd/snd.h). src/game has no audio calls yet: this is the hook list
for it. Each line: <GML file>:<line>  <GML call>  ->  <C call>.

    tools/sndhooks.py <hd src> <src/game dir> [out.txt]
"""
import glob
import os
import re
import sys


def main():
    src, game = sys.argv[1], sys.argv[2]
    out = open(sys.argv[3], 'w') if len(sys.argv) > 3 else sys.stdout
    init = open(os.path.join(src, 'scripts', 'initMusic', 'initMusic.gml')).read()
    gl = dict(re.findall(r'global\.(\w+)\s*=\s*(\w+)', init))          # global.sndHit -> xhit
    code = ''.join(open(f).read() for f in glob.glob(os.path.join(game, 'p*.c')))
    objs = set(re.findall(r'OBJ_(\w+)', code))
    scripts = {os.path.basename(d) for d in glob.glob(os.path.join(src, 'scripts', '*'))
               if re.search(r'\b%s\b' % re.escape(os.path.basename(d)), code)}
    files = []
    for o in sorted(objs):
        files += sorted(glob.glob(os.path.join(src, 'objects', o, '*.gml')))
    for s in sorted(scripts):
        files += sorted(glob.glob(os.path.join(src, 'scripts', s, '*.gml')))
    pat = re.compile(r'\b(playSound|playMusic|startMusic|stopAllMusic|audio_play_sound|audio_stop_sound|'
                     r'audio_is_playing|audio_pause_all|audio_resume_all|audio_stop_all|setSoundVol)\s*\(([^;]*?)\)')

    def asset(a):
        a = a.strip().split(',')[0].strip()
        a = gl.get(a[7:], a[7:]) if a.startswith('global.') else a
        return 'SND_' + a if a else '?'
    C = {'playSound': 'snd_play({})', 'playMusic': 'snd_music({}, loop)', 'audio_play_sound': 'snd_audio_play({}, prio, loop)',
         'audio_stop_sound': 'snd_stop({})', 'audio_is_playing': 'snd_is_playing({})', 'setSoundVol': 'snd_gain({}, reg)',
         'stopAllMusic': 'snd_stop_music()', 'startMusic': 'game: startMusic (snd_music + snd_gain)',
         'audio_pause_all': 'snd_pause_all()', 'audio_resume_all': 'snd_resume_all()', 'audio_stop_all': 'snd_stop_all()'}
    n = {}
    for f in files:
        for k, line in enumerate(open(f, encoding='utf-8', errors='replace'), 1):
            s = line.split('//')[0]
            for m in pat.finditer(s):
                fn = m.group(1)
                c = C[fn].format(asset(m.group(2))) if '{}' in C[fn] else C[fn]
                out.write(f'{os.path.relpath(f, src)}:{k}  {m.group(0)[:60]}  ->  {c}\n')
                n[fn] = n.get(fn, 0) + 1
    sys.stderr.write(f'{len(objs)} objects and {len(scripts)} scripts translated; audio calls: '
                     + ', '.join(f'{k} {v}' for k, v in sorted(n.items(), key=lambda x: -x[1])) + '\n')


if __name__ == '__main__':
    main()
