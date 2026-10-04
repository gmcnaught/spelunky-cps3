#!/usr/bin/env python3
"""Runner probe of the instance collision tests (src/game/pworld.c point_hit / rect_hit / line_hit / overlap_at)
against the HD runner's own (CInstance::Collision_Point / Rectangle / Line / Instance, CSprite precise masks,
SeparatingAxisCollision): random cases of two oBrickSmooth instances (no events) with a chosen sprite, image_index,
image_angle (half of them rotated), image_xscale / yscale and fractional x / y, in an empty room.

    tools/colprobe.py <name> [--n N] [--seed S] [--whole]   runner + C; the runner's answers -> tests/colprobe/<name>.ref
    tools/colprobe.py <name> --no-run             C against tests/colprobe/<name>.ref (its n, seed: line 1)
    (build/colprobe/<name>.{case,c}: the cases and the C answers)

Per case the runner writes "k pair p1 r1 l1 p0 r0 l0": place_meeting(x, y, b) from a (Collision_Instance), then
collision_point / collision_rectangle / collision_line against instance a with prec true (1) and false (0).
build/host/colprobe replays the cases (test/host/colprobe.c) and the two files are compared.
"""
import os
import random
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..'))
sys.path.insert(0, HERE)
import tracer  # noqa: E402
import treeprobe  # noqa: E402,F401  (the same runner setup)

SPRITES = ['sArrowRight', 'sArrowLeft', 'sExplosion', 'sJar', 'sRock', 'sSkull', 'sBomb', 'sStandLeft', 'sBrick',
           'sRed', 'sWeb', 'sBlood', 'sGoldBar', 'sSnakeLeft']


def num(rnd, lo, hi, fracs=(0.0, 0.25, 0.5, 0.75, 0.375, 0.6, 0.1)):
    return round(rnd.randint(lo, hi) + rnd.choice(fracs), 6)


def make_cases(n, seed, whole=False):
    """whole: angle 0, scales +-1, whole x, y (pworld.c's integer path, precise_collision_int)"""
    rnd = random.Random(seed)
    cases = []
    for _ in range(n):
        def inst():
            if whole:
                return dict(spr=rnd.choice(SPRITES), img=float(rnd.randint(0, 5)), x=float(rnd.randint(200, 240)),
                            y=float(rnd.randint(200, 240)), ang=0.0, xs=rnd.choice([1.0, -1.0]),
                            ys=rnd.choice([1.0, -1.0]))
            ang = 0.0 if rnd.random() < 0.4 else rnd.choice([round(rnd.uniform(0, 360), 3), 90.0, 180.0, 270.0,
                                                               5.710593223571777, 45.0, 189.92623901367188])
            return dict(spr=rnd.choice(SPRITES), img=float(rnd.randint(0, 5)), x=num(rnd, 200, 240),
                        y=num(rnd, 200, 240), ang=ang, xs=rnd.choice([1.0, 1.0, 1.0, -1.0, 2.0, 0.5]),
                        ys=rnd.choice([1.0, 1.0, 1.0, -1.0, 0.5]))
        a, b = inst(), inst()
        if whole:
            b['x'] = a['x'] + rnd.randint(-16, 16)
            b['y'] = a['y'] + rnd.randint(-16, 16)
        else:
            b['x'] = round(a['x'] + rnd.uniform(-20, 20), 4)
            b['y'] = round(a['y'] + rnd.uniform(-20, 20), 4)
        p = (round(a['x'] + rnd.uniform(-12, 12), 3), round(a['y'] + rnd.uniform(-12, 12), 3))
        r = [round(a['x'] + rnd.uniform(-14, 14), 3), round(a['y'] + rnd.uniform(-14, 14), 3)]
        r += [round(r[0] + rnd.uniform(-8, 8), 3), round(r[1] + rnd.uniform(-8, 8), 3)]
        ln = [round(a['x'] + rnd.uniform(-16, 16), 3), round(a['y'] + rnd.uniform(-16, 16), 3)]
        ln += [round(a['x'] + rnd.uniform(-16, 16), 3), round(a['y'] + rnd.uniform(-16, 16), 3)]
        cases.append((a, b, p, r, ln))
    return cases


def case_line(a, b, p, r, ln):
    f = lambda d: f"{d['spr']} {d['img']!r} {d['x']!r} {d['y']!r} {d['ang']!r} {d['xs']!r} {d['ys']!r}"
    return ' '.join([f(a), f(b)] + [repr(v) for v in (*p, *r, *ln)])


def gml(cases):
    body = []
    for k, (a, b, p, r, ln) in enumerate(cases):
        def mk(v, d):
            return (f"var {v} = instance_create_depth({d['x']!r}, {d['y']!r}, 0, oBrickSmooth); "
                    f"{v}.sprite_index = {d['spr']}; {v}.image_speed = 0; {v}.image_index = {d['img']!r}; "
                    f"{v}.image_angle = {d['ang']!r}; {v}.image_xscale = {d['xs']!r}; {v}.image_yscale = {d['ys']!r};")
        body.append(f'''
{{
    {mk('ia', a)}
    {mk('ib', b)}
    global.cp_b = ib;
    with (ia) global.cp_pair = place_meeting(x, y, global.cp_b);
    var s = "{k} " + string(global.cp_pair ? 1 : 0);
    s += " " + string(collision_point({p[0]!r}, {p[1]!r}, ia, true, false) != noone ? 1 : 0);
    s += " " + string(collision_rectangle({r[0]!r}, {r[1]!r}, {r[2]!r}, {r[3]!r}, ia, true, false) != noone ? 1 : 0);
    s += " " + string(collision_line({ln[0]!r}, {ln[1]!r}, {ln[2]!r}, {ln[3]!r}, ia, true, false) != noone ? 1 : 0);
    s += " " + string(collision_point({p[0]!r}, {p[1]!r}, ia, false, false) != noone ? 1 : 0);
    s += " " + string(collision_rectangle({r[0]!r}, {r[1]!r}, {r[2]!r}, {r[3]!r}, ia, false, false) != noone ? 1 : 0);
    s += " " + string(collision_line({ln[0]!r}, {ln[1]!r}, {ln[2]!r}, {ln[3]!r}, ia, false, false) != noone ? 1 : 0);
    file_text_write_string(global.cp_f, s);
    file_text_writeln(global.cp_f);
    instance_destroy(ia, false);
    instance_destroy(ib, false);
}}''')
    create = 'global.cp_phase = -1;\n'
    end = f'''
if (global.cp_phase == -1)
{{
    global.cp_phase = 0;
    global.cp_newroom = room_add();
    room_set_width(global.cp_newroom, 1024);
    room_set_height(global.cp_newroom, 1024);
    room_goto(global.cp_newroom);
}}
else if (global.cp_phase == 0 && room == global.cp_newroom)
{{
    global.cp_phase = 1;
    global.cp_f = file_text_open_write("colprobe.txt");
    {''.join(body)}
    file_text_close(global.cp_f);
    var df = file_text_open_write("colprobe_done.txt");
    file_text_write_string(df, "1");
    file_text_close(df);
    game_end();
}}
'''
    return create, end


def run_runner(name, create, end):
    game = os.path.join(ROOT, 'refs', 'hd', 'linux-arm64')
    run = os.path.join(ROOT, 'build', 'run', 'colprobe_' + name)
    subprocess.run(['rm', '-rf', run], check=True)
    os.makedirs(os.path.join(run, 'config'))
    subprocess.run(['cp', '-R', game + '/.', os.path.join(run, 'game')], check=True)
    unx = os.path.join(run, 'game', 'assets', 'game.unx')
    src = os.path.join(game, 'assets', 'game.unx')
    root = os.path.commonpath([src, tracer.UTMT, unx])
    rel = lambda p: '/w/' + os.path.relpath(p, root)
    q = lambda s: '@"' + s.replace('"', '""') + '"'
    script = unx + '.csx'
    open(script, 'w').write(f'''
using UndertaleModLib.Compiler;
CodeImportGroup g = new(Data);
g.QueueAppend("gml_Object_oGamepad_Create_0", {q(create)});
g.QueueReplace("gml_Object_oGamepad_Step_2", {q(end)});
g.Import();
''')
    os.remove(unx)
    r = subprocess.run(['docker', 'run', '--rm', '-v', f'{root}:/w', tracer.DOTNET, 'dotnet', rel(tracer.UTMT), 'load',
                        rel(src), '-s', rel(script), '-o', rel(unx), '-f'], capture_output=True, text=True)
    if not os.path.exists(unx):
        sys.exit(r.stdout[-3000:] + r.stderr[-3000:])
    os.remove(script)
    sh = '''
Xvfb :99 -screen 0 1280x960x24 -nolisten tcp >/dev/null 2>&1 &
for i in 1 2 3 4 5 6 7 8 9 10; do [ -e /tmp/.X11-unix/X99 ] && break; sleep 0.5; done
mkdir -p /tmp/.config && ln -s /r/config /tmp/.config/SpelunkyClassicHD
./SpelunkyClassicHD > /r/run.log 2>&1 & p=$!; t=0
while [ ! -e colprobe_done.txt ] && [ ! -e gml_error.txt ] && [ $t -lt 300 ] && kill -0 $p 2>/dev/null; do sleep 1; t=$((t + 1)); done
sleep 1; kill -9 $p 2>/dev/null; true'''
    subprocess.run(['docker', 'run', '--rm', '--platform', 'linux/arm64', '-v', f'{run}:/r', '-w', '/r/game',
                    'spelunky-hd-runner', 'sh', '-c', sh], check=False)
    res = os.path.join(run, 'game', 'colprobe.txt')
    if not os.path.exists(res) or not os.path.exists(os.path.join(run, 'game', 'colprobe_done.txt')):
        sys.exit(f'no complete colprobe.txt; see {run}')
    return res, run


def main():
    name = sys.argv[1]
    n = int(sys.argv[sys.argv.index('--n') + 1]) if '--n' in sys.argv else 2000
    seed = int(sys.argv[sys.argv.index('--seed') + 1]) if '--seed' in sys.argv else 1
    whole = '--whole' in sys.argv
    out = os.path.join(ROOT, 'build', 'colprobe')
    os.makedirs(out, exist_ok=True)
    ref = os.path.join(ROOT, 'tests', 'colprobe', name + '.ref')
    if '--no-run' in sys.argv:
        head = open(ref).readline().split()            # "# n N seed S"
        n, seed, whole = int(head[2]), int(head[4]), 'whole' in head
    cases = make_cases(n, seed, whole)
    with open(os.path.join(out, name + '.case'), 'w') as f:
        for c in cases:
            f.write(case_line(*c) + '\n')
    if '--no-run' not in sys.argv:
        res, run = run_runner(name, *gml(cases))
        os.makedirs(os.path.dirname(ref), exist_ok=True)
        with open(ref, 'w') as f:
            f.write(f'# n {n} seed {seed}{" whole" if whole else ""} (tools/colprobe.py: the HD runner\'s answers)\n' + open(res).read())
        subprocess.run(['rm', '-rf', run], check=True)
    c = subprocess.run([os.path.join(ROOT, 'build', 'host', 'colprobe'), os.path.join(out, name + '.case')],
                       capture_output=True, text=True, check=True).stdout
    open(os.path.join(out, name + '.c'), 'w').write(c)
    rl, cl = open(ref).read().split('\n')[1:], c.split('\n')
    tests = ['pair', 'point1', 'rect1', 'line1', 'point0', 'rect0', 'line0']
    bad = {t: 0 for t in tests}
    first = {}
    for k in range(len(cases)):
        rr, cc = rl[k].split(), cl[k].split()
        for j, t in enumerate(tests):
            if rr[j + 1] != cc[j + 1]:
                bad[t] += 1
                first.setdefault(t, k)
    print(f'colprobe {name}: {len(cases)} cases; differences ' + ' '.join(f'{t} {bad[t]}' for t in tests))
    for t, k in first.items():
        print(f'  first {t}: case {k}: {case_line(*cases[k])}  ref {rl[k]}  c {cl[k]}')
    return 0 if not any(bad.values()) else 1


if __name__ == '__main__':
    sys.exit(main())
