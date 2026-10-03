#!/usr/bin/env python3
"""Reference runner for Spelunky Classic HD 1.2.2: build a traced copy of the game's data file, decode its dumps.

    tools/tracer.py build <game.unx> <route.txt> <out.unx> [--seed N]
    tools/tracer.py decode <trace.bin> <names.txt> [--steps a-b] [--no-inst]

The data file is the official Linux release's (refs/hd/linux-arm64/assets/game.unx: VM bytecode 17, the same
game as the APK's game.droid except the desktop platform code: getPlatform sets global.mobileBuild = false,
characterStepEvent's run release, getWorkingDirPath, oDebug). `build` makes the copy with UndertaleModTool's CLI
(../_tools/UndertaleModTool/out-cli, run in the dotnet SDK container) and writes <out>.names (the object /
sprite / room names by index, for `decode`). scripts/hd_trace.sh runs the copy with the release's own runner
(GameMaker 2024.14 Linux arm64) in Docker (docker/hd-runner: Xvfb, Mesa, ALSA null) and joins the chunks.
Method from ../maldita.castilla-cps3/tools/tracer.py. Changes, all GML compiled into the copy:

  - oIntro Create: randomize() -> random_set_seed(SEED).
  - oGamepad Create (appended): the tracer's globals and the route as two arrays (segment end step, key mask).
  - oGamepad Step (replaced): the gamepad poll becomes the route. The fields the game reads through checkLeft(),
    checkJumpPressed() ... (global.gamepad.<key>, <key>Pressed, <key>Released) are set from the route's mask for
    route step t with oGamepad's own rules: Pressed on the first step a key is held, Released on the first step
    it is not. Keyboard reads stay (nothing presses a key under Xvfb). Before the trace starts every key is up.
    Room flow, once each: in rIntro -> room_goto(rTitle) (as the intro's skip); in rTitle -> what the title's
    START door does on a new game (global.usedShortcut = false, global.gameStart = true), then
    random_set_seed(SEED) again and room_goto(rLevel). TRACE_RESEED=0 leaves out the second seeding.
  - oGamepad Begin Step (new): the trace starts at the first Begin Step in rLevel (route step t = 0 is the Step
    that follows). A phase-0 record is written at the first Begin Step in each room (the room's state after
    every Create / Room Start: in rLevel, the generated level, oGame Create -> scrInitLevel -> scrLevelGen).
  - oGamepad End Step (new): a phase-1 record each step. The buffer is saved every 50 records as
    trc_<k>.bin (buffer_save_ext; then rewound). After route steps + TAIL (default 30) or MAX_STEPS records:
    last chunk saved, game_end().

oGamepad (created by scrInit in rScreenInit, persistent) is the second instance after oScreen, so its Begin Step
runs before every other instance's Begin Step except oScreen's (surfaces, pause), its Step before every
Step that reads the gamepad (as in the original).

Route file: lines "<steps> <keys>"; keys a string of letters or "-" for none; "#" starts a comment.
    R right  L left  U up  D down  J jump  A attack  I item  N run  B bomb  O rope  F flare  P pay  S start

Trace format (little-endian; the chunks concatenated in order, scripts/hd_trace.sh):
  record header
    u32  magic 0x31545053 ("SPT1")
    u8   phase           0 = first Begin Step in a room, 1 = End Step
    s32  rec             record number (0, 1, ...; both phases)
    s32  t               route steps done (the End Step of route step t writes t + 1)
    s32  input           key mask the last Step used (bits as KEYS below)
    s32  room            room index (names file R lines)
    f64  currLevel       global.currLevel
    f64  seed            random_get_seed()
    f64  plife, bombs, rope, money    global.plife / bombs / rope / money (-1e9 if not a number)
    u32  n               instances that follow (with (all), the runner's instance order)
  per instance
    s32  id
    s16  object_index    (names file O lines)
    f64  x, y
    s16  sprite_index    (-1 none; names file S lines)
    f64  image_index
    u16  alarm mask      bit k: alarm[k] != -1
    f64  alarm[k]        for each set bit, k ascending
    u8   vel flags       bit 0: the instance has xVel, bit 1: yVel (variable_instance_exists)
    f64  xVel, yVel      when present (-1e9 if not a number)
Names file (<out.droid>.names): one line per resource, "O <index> <name>", "S ...", "R ...".
"""
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
UTMT = os.path.normpath(os.path.join(HERE, '..', '..', '_tools', 'UndertaleModTool', 'out-cli', 'UndertaleModCli.dll'))
DOTNET = 'mcr.microsoft.com/dotnet/sdk:10.0'
# route letter -> (bit, oGamepad field)
KEYS = {'R': (1, 'right'), 'L': (2, 'left'), 'U': (4, 'up'), 'D': (8, 'down'), 'J': (16, 'jump'),
        'A': (32, 'attack'), 'I': (64, 'item'), 'N': (128, 'run'), 'B': (256, 'bomb'), 'O': (512, 'rope'),
        'F': (1024, 'flare'), 'P': (2048, 'pay'), 'S': (4096, 'start')}
MAGIC = 0x31545053
CHUNK = 50
TAIL = int(os.environ.get('TRACE_TAIL', 30))
MAX_STEPS = int(os.environ.get('TRACE_MAX', 20000))
BAD = -1e9


def parse_route(path):
    segs = []
    for line in open(path):
        line = line.split('#')[0].strip()
        if not line:
            continue
        n, keys = (line.split() + ['-'])[:2]
        m = 0
        for k in keys:
            if k != '-':
                m |= KEYS[k][0]
        segs.append((int(n), m))
    return segs


def num(expr):
    return f'(is_numeric({expr}) ? {expr} : {BAD})'


def record(phase):
    """GML writing one record into global.trc_buf"""
    return f'''
{{
    var b = global.trc_buf;
    buffer_write(b, buffer_u32, {MAGIC});
    buffer_write(b, buffer_u8, {phase});
    buffer_write(b, buffer_s32, global.trc_rec);
    buffer_write(b, buffer_s32, global.trc_t);
    buffer_write(b, buffer_s32, global.trc_m);
    buffer_write(b, buffer_s32, real(room));
    buffer_write(b, buffer_f64, {num('global.currLevel')});
    buffer_write(b, buffer_f64, random_get_seed());
    buffer_write(b, buffer_f64, {num('global.plife')});
    buffer_write(b, buffer_f64, {num('global.bombs')});
    buffer_write(b, buffer_f64, {num('global.rope')});
    buffer_write(b, buffer_f64, {num('global.money')});
    var npos = buffer_tell(b);
    buffer_write(b, buffer_u32, 0);
    var n = 0;
    with (all)
    {{
        n += 1;
        buffer_write(b, buffer_s32, real(id));
        buffer_write(b, buffer_s16, real(object_index));
        buffer_write(b, buffer_f64, x);
        buffer_write(b, buffer_f64, y);
        buffer_write(b, buffer_s16, real(sprite_index));
        buffer_write(b, buffer_f64, image_index);
        var am = 0;
        for (var k = 0; k < 12; k++)
        {{
            if (alarm[k] != -1) am |= (1 << k);
        }}
        buffer_write(b, buffer_u16, am);
        for (var k = 0; k < 12; k++)
        {{
            if (am & (1 << k)) buffer_write(b, buffer_f64, alarm[k]);
        }}
        var vf = 0;
        if (variable_instance_exists(id, "xVel")) vf |= 1;
        if (variable_instance_exists(id, "yVel")) vf |= 2;
        buffer_write(b, buffer_u8, vf);
        if (vf & 1) buffer_write(b, buffer_f64, {num('xVel')});
        if (vf & 2) buffer_write(b, buffer_f64, {num('yVel')});
    }}
    buffer_poke(b, npos, buffer_u32, n);
    global.trc_rec += 1;
}}
'''


def save_chunk(rewind):
    s = ('buffer_save_ext(global.trc_buf, "trc_" + string(global.trc_chunk) + ".bin", 0, '
         'buffer_tell(global.trc_buf));')
    if rewind:
        s += '\nglobal.trc_chunk += 1;\nbuffer_seek(global.trc_buf, buffer_seek_start, 0);'
    return s


def gml(segs, seed):
    total = sum(n for n, _ in segs)
    ends, masks, t = [], [], 0
    for n, m in segs:
        t += n
        ends.append(str(t))
        masks.append(str(m))
    reseed = '' if os.environ.get('TRACE_RESEED') == '0' else f'random_set_seed({seed});'
    create = f'''
global.trc_phase = 0;
global.trc_on = 0;
global.trc_done = 0;
global.trc_t = 0;
global.trc_m = 0;
global.trc_rec = 0;
global.trc_chunk = 0;
global.trc_seg = 0;
global.trc_lastroom = -1;
global.trc_buf = -1;
global.trc_ends = [{', '.join(ends) or '0'}];
global.trc_masks = [{', '.join(masks) or '0'}];
'''
    keys = ''.join(f'''
{f}Released = false;
{f}Pressed = false;
if ({f} && !(m & {bit})) {f}Released = true;
if (!{f} && (m & {bit})) {f}Pressed = true;
{f} = ((m & {bit}) != 0);
''' for bit, f in KEYS.values())
    step = f'''
if (global.trc_phase == 0 && room == rIntro)
{{
    global.trc_phase = 1;
    global.gameStart = false;
    room_goto(rTitle);
}}
else if (global.trc_phase == 1 && room == rTitle)
{{
    global.trc_phase = 2;
    global.usedShortcut = false;
    global.gameStart = true;
    {reseed}
    room_goto(rLevel);
}}
var m = 0;
if (global.trc_on)
{{
    var t = global.trc_t;
    var nseg = array_length(global.trc_ends);
    while (global.trc_seg < nseg && t >= global.trc_ends[global.trc_seg]) global.trc_seg += 1;
    if (global.trc_seg < nseg) m = global.trc_masks[global.trc_seg];
    global.trc_t += 1;
}}
global.trc_m = m;
{keys}
'''
    begin = f'''
if (global.trc_done) exit;
if (!global.trc_on && global.trc_phase == 2 && room == rLevel)
{{
    global.trc_on = 1;
    global.trc_buf = buffer_create(1048576, buffer_grow, 1);
}}
if (!global.trc_on) exit;
if (room != global.trc_lastroom)
{{
    global.trc_lastroom = room;
    {record(0)}
}}
'''
    end = f'''
if (global.trc_done || !global.trc_on) exit;
{record(1)}
if (global.trc_t >= {total + TAIL} || global.trc_rec >= {MAX_STEPS})
{{
    {save_chunk(False)}
    global.trc_done = 1;
    global.trc_on = 0;
    game_end();
    exit;
}}
if (global.trc_rec mod {CHUNK} == 0)
{{
    {save_chunk(True)}
}}
'''
    return create, step, begin, end


def csx(create, step, begin, end, seed, names):
    q = lambda s: '@"' + s.replace('"', '""') + '"'
    return f'''
using System.IO;
using System.Text;
using UndertaleModLib.Compiler;
var sb = new StringBuilder();
for (int i = 0; i < Data.GameObjects.Count; i++) sb.Append($"O {{i}} {{Data.GameObjects[i].Name.Content}}\\n");
for (int i = 0; i < Data.Sprites.Count; i++) sb.Append($"S {{i}} {{Data.Sprites[i].Name.Content}}\\n");
for (int i = 0; i < Data.Rooms.Count; i++) sb.Append($"R {{i}} {{Data.Rooms[i].Name.Content}}\\n");
File.WriteAllText({q(names)}, sb.ToString());
CodeImportGroup g = new(Data);
g.QueueFindReplace("gml_Object_oIntro_Create_0", "randomize();", "random_set_seed({seed});");
g.QueueAppend("gml_Object_oGamepad_Create_0", {q(create)});
g.QueueReplace("gml_Object_oGamepad_Step_0", {q(step)});
g.QueueReplace("gml_Object_oGamepad_Step_1", {q(begin)});
g.QueueReplace("gml_Object_oGamepad_Step_2", {q(end)});
g.Import();
'''


def build(droid, route, out, seed):
    droid, route, out = map(os.path.abspath, (droid, route, out))
    segs = parse_route(route)
    work = os.path.dirname(out)
    os.makedirs(work, exist_ok=True)
    root = os.path.commonpath([droid, UTMT, out])
    rel = lambda p: '/w/' + os.path.relpath(p, root)
    names = out + '.names'
    script = out + '.csx'
    open(script, 'w').write(csx(*gml(segs, seed), seed, rel(names)))
    if os.path.exists(out):
        os.remove(out)
    cmd = ['docker', 'run', '--rm', '-v', f'{root}:/w', DOTNET, 'dotnet', rel(UTMT),
           'load', rel(droid), '-s', rel(script), '-o', rel(out), '-f']
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(out) or re.search(r'(?i)error|exception', r.stdout + r.stderr):
        sys.exit(r.stdout[-4000:] + r.stderr[-4000:])
    print(f'{out}: seed {seed}, {len(segs)} route segments, {sum(n for n, _ in segs)} steps (+{TAIL})')


def load_names(path):
    names = {'O': {}, 'S': {}, 'R': {}}
    for line in open(path):
        k, i, n = line.rstrip('\n').split(' ', 2)
        names[k][int(i)] = n
    return names


def records(data):
    """yield (header dict, [instance dict]) from trace bytes"""
    o = 0
    H = struct.Struct('<IBiiii6dI')
    while o < len(data):
        h = H.unpack_from(data, o)
        o += H.size
        if h[0] != MAGIC:
            raise ValueError(f'bad magic at {o - H.size}')
        hd = dict(zip(['magic', 'phase', 'rec', 't', 'input', 'room', 'currLevel', 'seed', 'plife', 'bombs',
                       'rope', 'money', 'n'], h))
        insts = []
        for _ in range(hd['n']):
            iid, obj, x, y, spr, img, am = struct.unpack_from('<ihddhdH', data, o)
            o += 34
            alarms = {}
            for k in range(12):
                if am & (1 << k):
                    alarms[k] = struct.unpack_from('<d', data, o)[0]
                    o += 8
            vf = data[o]
            o += 1
            vel = []
            for bit in (1, 2):
                if vf & bit:
                    vel.append(struct.unpack_from('<d', data, o)[0])
                    o += 8
                else:
                    vel.append(None)
            insts.append(dict(id=iid, obj=obj, x=x, y=y, spr=spr, img=img, alarms=alarms, xVel=vel[0], yVel=vel[1]))
        yield hd, insts


def g(v):
    return 'None' if v is None else repr(v)


def decode(path, names_path, steps=None, inst=True):
    names = load_names(names_path)
    data = open(path, 'rb').read()
    a, b = (map(int, steps.split('-')) if steps else (0, 1 << 60))
    for hd, insts in records(data):
        if not a <= hd['rec'] <= b:
            continue
        print(f"rec {hd['rec']} phase {hd['phase']} t {hd['t']} input {hd['input']} "
              f"room {names['R'].get(hd['room'], hd['room'])} level {g(hd['currLevel'])} seed {g(hd['seed'])} "
              f"life {g(hd['plife'])} bombs {g(hd['bombs'])} rope {g(hd['rope'])} money {g(hd['money'])} "
              f"n {hd['n']}")
        if not inst:
            continue
        for i in insts:
            al = ' '.join(f'a{k}={g(v)}' for k, v in i['alarms'].items())
            vel = '' if i['xVel'] is None and i['yVel'] is None else f" vel {g(i['xVel'])} {g(i['yVel'])}"
            print(f"  {i['id']} {names['O'].get(i['obj'], i['obj'])} {g(i['x'])} {g(i['y'])} "
                  f"{names['S'].get(i['spr'], i['spr'])} {g(i['img'])}{(' ' + al) if al else ''}{vel}")


def main():
    a = sys.argv[1:]
    if a and a[0] == 'build' and len(a) >= 4:
        seed = int(a[a.index('--seed') + 1]) if '--seed' in a else 1
        build(a[1], a[2], a[3], seed)
    elif a and a[0] == 'decode' and len(a) >= 3:
        decode(a[1], a[2], a[a.index('--steps') + 1] if '--steps' in a else None, '--no-inst' not in a)
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
