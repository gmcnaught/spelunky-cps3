#!/usr/bin/env python3
"""Runner probe of the collision tree alone (src/game/pcol.c): a known sequence of instance creations, destroys
and moves in an empty room of the HD runner, and the tree's search order after each phase, against the C model.

    tools/treeprobe.py <name> [--n N] [--seed S]     -> build/treeprobe/<name>.{case,ref,c}

Phase 1 (oGamepad's first End Step): N oBrickSmooth (no Create event of its own; 16 x 16 sCaveSmooth) by
instance_create_depth at distinct 16-px cells, in case order; dump A = collision_rectangle_list over the room
(unordered: the tree's search order); then instance_destroy(id, false) for the destroy list.
Phase 2 (next End Step: RemoveMarked ran in the Draw between): dump B; then x += 16 for the move list (each a
CollisionMarkDirty), dump C (its UpdateTree reinserts them, newest mark first); game_end.
The C side (build/host/treeprobe) replays the case through pin_create / pin_kill / pcol_remove_marked / pcol_mark.
"""
import os
import random
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..'))
sys.path.insert(0, HERE)
import tracer  # noqa: E402


def make_case(n, seed):
    if os.environ.get('TP_POS'):                  # explicit cells "x,y x,y ..." (no destroys / moves)
        cells = [tuple(map(int, c.split(','))) for c in os.environ['TP_POS'].split()]
        return [(16 * x, 16 * y) for x, y in cells], [], []
    rnd = random.Random(seed)
    cells = rnd.sample([(x, y) for x in range(1, 47) for y in range(1, 39)], n)
    pos = [(16 * x, 16 * y) for x, y in cells]
    dest = rnd.sample(range(n), n // 5)
    rest = [k for k in range(n) if k not in set(dest)]
    move = rnd.sample(rest, n // 6)
    return pos, dest, move


def gml(pos, dest, move):
    xs = ', '.join(str(x) for x, _ in pos)
    ys = ', '.join(str(y) for _, y in pos)
    ds = ', '.join(map(str, dest)) or ''
    ms = ', '.join(map(str, move)) or ''
    dump = '''
{
    var l = ds_list_create();
    var nn = collision_rectangle_list(-100000, -100000, 100000, 100000, oBrickSmooth, false, false, l, false);
    var s = "";
    for (var dk = 0; dk < ds_list_size(l); dk++) s += " " + string(real(ds_list_find_value(l, dk)));
    file_text_write_string(global.tp_f, string(nn) + s);
    file_text_writeln(global.tp_f);
    ds_list_destroy(l);
}
'''
    create = '''
global.tp_phase = -1;
'''
    stepdump = dump if os.environ.get('TP_STEPDUMP') else ''
    end = f'''
if (global.tp_phase == -1)
{{
    global.tp_phase = 0;
    global.tp_newroom = room_add();
    room_set_width(global.tp_newroom, 1024);
    room_set_height(global.tp_newroom, 1024);
    room_goto(global.tp_newroom);
}}
else if (global.tp_phase == 0 && room == global.tp_newroom)
{{
    global.tp_phase = 1;
    global.tp_room = room;
    global.tp_f = file_text_open_write("treeprobe.txt");
    {{
        var sr = "room " + room_get_name(room) + " " + string(instance_count);
        with (all) sr += " " + object_get_name(object_index) + "@" + string(x) + "," + string(y);
        file_text_write_string(global.tp_f, sr);
        file_text_writeln(global.tp_f);
    }}
    var xs = [{xs}];
    var ys = [{ys}];
    global.tp_ids = [];
    for (var k = 0; k < array_length(xs); k++)
    {{
        global.tp_ids[k] = instance_create_depth(xs[k], ys[k], 0, oBrickSmooth);
        {stepdump}
    }}
    var s0 = "";
    for (var k = 0; k < array_length(xs); k++) s0 += " " + string(real(global.tp_ids[k]));
    file_text_write_string(global.tp_f, "ids" + s0);
    file_text_writeln(global.tp_f);
    {dump}
    var ds = [{ds}];
    for (var k = 0; k < array_length(ds); k++) instance_destroy(global.tp_ids[ds[k]], false);
}}
else if (global.tp_phase == 1)
{{
    global.tp_phase = 2;
    if (room != global.tp_room) file_text_write_string(global.tp_f, "room changed");
    {dump}
    var ms = [{ms}];
    for (var k = 0; k < array_length(ms); k++) global.tp_ids[ms[k]].x += 16;
    {dump}
    file_text_close(global.tp_f);
    var df = file_text_open_write("treeprobe_done.txt");
    file_text_write_string(df, "1");
    file_text_close(df);
    game_end();
}}
'''
    if os.environ.get('TP_INCREATE', '0') == '1':
        k0 = end.index('if (global.tp_phase == 0)')
        k1 = end.index('else if (global.tp_phase == 1)')
        create = create + 'try {\n' + end[k0:k1] + '\n} catch (e) { var ef = file_text_open_write("treeprobe_err.txt"); file_text_write_string(ef, string(e)); file_text_close(ef); }\n'
        end = end[:k0] + end[k1 + len('else '):]
    return create, end


def build(name, n, seed):
    out = os.path.join(ROOT, 'build', 'treeprobe')
    os.makedirs(out, exist_ok=True)
    pos, dest, move = make_case(n, seed)
    with open(os.path.join(out, name + '.case'), 'w') as f:
        f.write(f'{len(pos)} {len(dest)} {len(move)}\n')
        f.write(' '.join(f'{x} {y}' for x, y in pos) + '\n')
        f.write(' '.join(map(str, dest)) + '\n')
        f.write(' '.join(map(str, move)) + '\n')
    create, end = gml(pos, dest, move)
    game = os.path.join(ROOT, 'refs', 'hd', 'linux-arm64')
    run = os.path.join(ROOT, 'build', 'run', 'treeprobe_' + name)
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
    sh = f'''
Xvfb :99 -screen 0 1280x960x24 -nolisten tcp >/dev/null 2>&1 &
for i in 1 2 3 4 5 6 7 8 9 10; do [ -e /tmp/.X11-unix/X99 ] && break; sleep 0.5; done
mkdir -p /tmp/.config && ln -s /r/config /tmp/.config/SpelunkyClassicHD
./SpelunkyClassicHD > /r/run.log 2>&1 & p=$!; t=0
while [ ! -e treeprobe_done.txt ] && [ ! -e treeprobe_err.txt ] && [ ! -e gml_error.txt ] && [ $t -lt 120 ] && kill -0 $p 2>/dev/null; do sleep 1; t=$((t + 1)); done
sleep 1; kill -9 $p 2>/dev/null; true'''
    subprocess.run(['docker', 'run', '--rm', '--platform', 'linux/arm64', '-v', f'{run}:/r', '-w', '/r/game',
                    'spelunky-hd-runner', 'sh', '-c', sh], check=False)
    res = os.path.join(run, 'game', 'treeprobe.txt')
    if not os.path.exists(res):
        sys.exit(f'no treeprobe.txt; see {run}')
    subprocess.run(['cp', res, os.path.join(out, name + '.ref')], check=True)
    print(os.path.join(out, name + '.ref'))


def main():
    name = sys.argv[1]
    n = int(sys.argv[sys.argv.index('--n') + 1]) if '--n' in sys.argv else 300
    seed = int(sys.argv[sys.argv.index('--seed') + 1]) if '--seed' in sys.argv else 1
    build(name, n, seed)


if __name__ == '__main__':
    main()
