#!/usr/bin/env python3
"""Reference runner for Spelunky Classic HD 1.2.2: build a traced copy of the game's data file, decode its dumps.

    tools/tracer.py build <game.unx> <route.txt> <out.unx> [--seed N]
    tools/tracer.py decode <trace.bin> <names.txt> [--steps a-b] [--no-inst]
    tools/tracer.py rng-probe <game.unx> <out.unx>      (scripts/hd_trace.sh --rng-probe; PROBE_GML below)
    tools/tracer.py build-gen <game.unx> <cases.txt> <out.unx>   (scripts/hd_trace.sh --gen; generator mode below)

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
  - TRACE_NOENEMY=1 (P4): at the first Begin Step in a level room (rLevel, rLevel2, rLevel3, rOlmec), before its
    phase-0 record, `with (o) instance_destroy(id, false)` for o in NOENEMY_OBJS (oEnemy and its children, which
    include the shopkeepers; oDamsel; oFakeBones): no Destroy events run. tests/routes/p4_*.txt and
    scripts/p4_trace.sh use it; the C play loop (src/game/prun.c enemies_out) removes the same objects.
  - TRACE_LEVEL=N (P5): the title flow sets global.currLevel = N before room_goto(rLevel) (as the generator mode
    does per case; oDebug's level keys do the same): the route starts on level N. playhost --level N.
    TRACE_MONEY=M (P5) likewise sets global.money = M (shop routes). playhost --money M.
    TRACE_GLOBALS=name=value,... (P7) likewise sets globals there: numbers as they are, anything else as a string
    ("~" for a space), e.g. pickupItem=Shotgun (oLevel Create gives it to the player: scrHoldItem) or hasJetpack=1.
    TRACE_NODARK=0 (P7) sets global.noDarkLevel = false there (scrClearGlobals leaves it true, so a route that starts
    on level N could never be a dark level).
  - TRACE_EVLOG=1 (probe runs only): every object event except Draw and oGamepad / oScreen / oIntro's appends
    id * 4096 + k to global.trc_evl (k: the names file's "C k <object> <event>" lines); the record writes the
    list since the last record. Used to determine the runner's event and instance order.
  - TRACE_SHOT=r1,r2,...: oGamepad Post-Draw (new) saves application_surface after record r (shot_gml).
  - TRACE_BOOT=1 (P8, the front end): no room flow (the intro and title run as on a cabinet, the route's keys from
    the first step in rIntro); the trace starts at the first Begin Step in rIntro. oIntro's randomize() is still
    random_set_seed(SEED); the trcSnd log starts there too.
  - TRACE_ROOM=<room> (P8): the title flow goes to that room instead of rLevel and the trace starts there (rOlmec:
    a level room like rLevel, global.gameStart true; with TRACE_LEVEL=16, P7)
    (rHighscores: the attract cycle's scores room, entered directly as src/front does), global.gameStart false
    (as the title's SCORES door leaves it: oGame's Create then generates no level).
  - TRACE_GUI=r1,r2,...: oGamepad Draw GUI End (new) saves gui_<r>.png at record r: application_surface with the
    GUI drawn over it as the runner draws it (oGame's scrDrawHUD, showMessages and showEndMessage run by oGame, whose instance
    variables they set as in its own Draw GUI; global.messageTimer put back after; oTransition's Draw GUI by event_perform, whose only side effect, global.noDarkLevel, it sets to the
    value it already has this frame; oTitle's, oHighscores' and oCredits2's (drawCredits) Draw GUI likewise, without
    side effects) - the frame TRACE_SHOT saves, plus the GUI.
  - TRACE_HUD=1: every record also carries the HUD globals the trace otherwise lacks (global.collect,
    messageTimer, message1 / message2 with their highlights, bloodLevel, drawHUD) in an SPT4 extension block.
  - TRACE_SND=1: every record also carries the sound calls made since the previous record, in call order (SPT4
    extension block): the scripts playSound, playMusic, startMusic, stopAllMusic, setSoundVol are replaced by
    their HD source with a log call first (snd_scripts), and the builtin audio_stop_sound / audio_pause_all /
    audio_resume_all / audio_stop_all / audio_play_sound calls in the code entries of SND_SITES go through logging
    wrappers (gml_GlobalScript_trcSnd). Logging starts at the title flow's room_goto(rLevel), so the level room's
    own start (oLevel Create: startMusic) is in record 0. Without either flag the records stay SPT3, byte for byte.
  - TRACE_TREE=r1,r2,... (probe runs only): after record r, tree_<r>.txt lists collision_rectangle_list over the
    whole room (unordered) for each object of TRACE_TREE_OBJS (default oSolid): the runner's collision-tree
    search order (src/game/pcol.c). The query flushes the tree's dirty list (UpdateTree): playhost --tree-probe.
  - oGamepad End Step (new): a phase-1 record each step. The buffer is saved every 50 records as
    trc_<k>.bin (buffer_save_ext; then rewound). After route steps + TAIL (default 30) or MAX_STEPS records:
    last chunk saved, then trc_done.txt (scripts/hd_trace.sh stops the runner once it exists: game_end() can
    hang in this runner), game_end().

oGamepad (created by scrInit in rScreenInit, persistent) is the second instance after oScreen, so its Begin Step
runs before every other instance's Begin Step except oScreen's (surfaces, pause), its Step before every
Step that reads the gamepad (as in the original).

Route file: lines "<steps> <keys>"; keys a string of letters or "-" for none; "#" starts a comment.
    R right  L left  U up  D down  J jump  A attack  I item  N run  B bomb  O rope  F flare  P pay  S start

Trace format (little-endian; the chunks concatenated in order, scripts/hd_trace.sh):
  record header
    u32  magic 0x33545053 ("SPT3")
    u8   phase           0 = first Begin Step in a room, 1 = End Step
    s32  rec             record number (0, 1, ...; both phases)
    s32  t               route steps done (the End Step of route step t writes t + 1)
    s32  input           key mask the last Step used (bits as KEYS below)
    s32  room            room index (names file R lines)
    f64  currLevel       global.currLevel
    f64  seed            random_get_seed()
    f64  plife, bombs, rope, money    global.plife / bombs / rope / money (-1e9 if not a number)
    u32  n               instances that follow (with (all), the runner's instance order)
    f64  view x, view y  camera_get_view_x / _y(view_camera[0])   (SPT3; view_xview[0] is not updated by the runner)
    f64  time            oGame.time (-1e9 without an oGame)       (SPT3)
  per instance
    s32  id
    s16  object_index    (names file O lines)
    f64  x, y
    s16  sprite_index    (-1 none; names file S lines)
    f64  image_index
    f64  image_xscale, image_yscale, image_angle
    u32  image_blend     (BGR colour)
    f64  image_alpha, depth
    u8   visible
    u16  alarm mask      bit k: alarm[k] != -1
    f64  alarm[k]        for each set bit, k ascending
    u8   vel flags       bit 0: the instance has xVel, bit 1: yVel (variable_instance_exists)
    f64  xVel, yVel      when present (-1e9 if not a number)
    f64  image_speed                                              (SPT3)
    u32  var mask        bit k: TRACE_VARS[k] exists and is numeric (SPT3)
    f64  value           for each set bit, k ascending            (SPT3)
  phase-0 records only, after the instances: the room's tile_add tiles (tiles_gml: legacy tile elements of
  every layer, layer_get_all() order then element order)
    u32  count
    per tile: string background name (NUL-terminated), f64 left, top, w, h, x, y, depth (the layer's depth)
  after the instances (and tiles), every record (SPT3):
    u32  count, s32 entries      TRACE_EVLOG's event log (count 0 without TRACE_EVLOG)
  SPT4 (TRACE_HUD / TRACE_SND): an SPT3 record, then
    u32  flags           bit 0 HUD block, bit 1 sound block
    HUD block (flags bit 0): f64 collect, messageTimer, bloodLevel, drawHUD; message1, message2 each as
      u8 type (0 string, 1 array of parts), then a string or u32 n + n strings; then the highlights:
      u32 n + n f64 (global.messageHighlights / message2Highlights when an array; n = 0 otherwise: only arrays
      highlight, scripts/drawHighlightedMessage)
    transition block (flags bit 2, with TRACE_HUD): f64 global.xtime, global.time, global.xmoney, and oTransition's
      drawLoot, moneyCount, isLoot, isKills, oDamselKiss.kissed, oTunnelMan.talk (-1e9 where absent)
    level block (flags bit 3, with TRACE_HUD): f64 global.darkLevel, oLevel.darkness,
      oPlayer1.distToNearestLightSource
    front block (flags bit 4, with TRACE_HUD): f64 oIntro.fadeLevel, oIntro.drawStatus, string str1, str2, str3
    end block (flags bit 5, with TRACE_HUD): f64 per END_KEYS: oGame.drawStatus, moneyCount (the game-over panel),
      oEnd3.drawStatus, moneyCount, fadeOut, fadeLevel, oCredits2.drawStatus, fadeIn, fadeOut, fadeLevel,
      global.kills, global.damsels (-1e9 where absent)
    sound block (flags bit 1): u32 count; per call u8 kind (SND_KINDS index), string asset (audio_get_name; "" none), f64 arg
      (playMusic: loop; setSoundVol: the volume argument; audio_play_sound: priority * 2 + loop; else 0)
  Magic "SPT3" (0x33545053, P4). "SPT2": no view, time, image_speed, vars or event log. "SPT1" (before
  2026-10-03 P3): also no image_xscale..visible fields and no tiles. decode reads all three.
Names file (<out.droid>.names): one line per resource, "O <index> <name>", "S ...", "R ...".
"""
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# UTMT=<path to UndertaleModCli.dll> overrides it (a worktree: the default is resolved from this file's place)
UTMT = os.environ.get('UTMT') or os.path.normpath(os.path.join(HERE, '..', '..', '_tools', 'UndertaleModTool', 'out-cli',
                                                                'UndertaleModCli.dll'))
DOTNET = 'mcr.microsoft.com/dotnet/sdk:10.0'
# route letter -> (bit, oGamepad field)
KEYS = {'R': (1, 'right'), 'L': (2, 'left'), 'U': (4, 'up'), 'D': (8, 'down'), 'J': (16, 'jump'),
        'A': (32, 'attack'), 'I': (64, 'item'), 'N': (128, 'run'), 'B': (256, 'bomb'), 'O': (512, 'rope'),
        'F': (1024, 'flare'), 'P': (2048, 'pay'), 'S': (4096, 'start')}
MAGIC = 0x33545053   # "SPT3" (SPT2: without view / time / image_speed / vars / event log; SPT1: without the
MAGIC2 = 0x32545053  #  drawing fields and tiles; both still decoded)
MAGIC1 = 0x31545053
MAGIC4 = 0x34545053  # "SPT4": SPT3 + the TRACE_HUD / TRACE_SND extension block
TRACE_HUD = os.environ.get('TRACE_HUD') == '1'
BOOT = os.environ.get('TRACE_BOOT') == '1'   # P8: trace from the first Begin Step in rIntro, no room flow
ROOM = os.environ.get('TRACE_ROOM', 'rLevel')  # P8: the title flow's room (rHighscores: the attract's scores room)
TRACE_SND = os.environ.get('TRACE_SND') == '1'
# sound log kinds (SPT4 sound block)
SND_KINDS = ['', 'playSound', 'playMusic', 'startMusic', 'stopAllMusic', 'setSoundVol', 'audio_stop_sound',
             'audio_pause_all', 'audio_resume_all', 'audio_stop_all', 'audio_play_sound']
# code entries whose builtin audio calls go through the logging wrappers (refs/hd/src: every such call in the
# objects and scripts the play loop translates, tools/sndhooks.py; the direct audio_play_sound of the title /
# ending rooms)
SND_SITES = ['gml_Object_oPlayer1_Step_0', 'gml_GlobalScript_scrFireBow', 'gml_Object_oGame_Step_0',
             'gml_Object_oTitle_Alarm_3', 'gml_Object_oEndCustom_Alarm_0', 'gml_Object_oPDummy2_Alarm_2',
             'gml_Object_oCamel_Step_0']
# SPT3 per-instance variables (bit k of the var mask: TRACE_VARS[k] exists and converts to a number by real())
TRACE_VARS = ['state', 'xAcc', 'yAcc', 'held', 'armed', 'status', 'fallTimer', 'stunTimer', 'dead', 'stunned',
              'jumpTime', 'whipping', 'hangCount', 'ladderTimer', 'pushTimer', 'runHeld', 'life', 'holdItem',
              'timer', 'counter', 'grav', 'gravityIntensity', 'safe', 'bounced', 'invincible', 'facing', 'cost',
              'myGrav', 'kJumped', 'jumpButtonReleased', 'trigger', 'value']
# TRACE_NOENEMY=1: objects destroyed (instance_destroy(id, false): no Destroy event) at the first Begin Step in each
# level room, before the phase-0 record (P4: the player without enemies). oEnemy covers every enemy (shopkeepers
# included); oDamsel walks on her own; oFakeBones turns into a skeleton when the player comes near
NOENEMY_OBJS = ['oEnemy', 'oDamsel', 'oFakeBones']
# TRACE_DEACT=<margin> (docs/DEACT.md): off-view deactivation, the rule src/game/prun.c's deact pass applies
DEACT = os.environ.get('TRACE_DEACT')
DEACT_ROOTS = ['oEnemy', 'oItem', 'oTreasure']            # these objects and their descendants are candidates,
DEACT_EXEMPT = ['oShopkeeper', 'oShopkeeper2', 'oBomb', 'oRopeThrow', 'oFlare', 'oFireFrogArmed',   # except these
                'oFireFrogBomb', 'oDamsel', 'oDice', 'oLampItem', 'oLampRedItem', 'oJaws']
# HD's legacy activation calls (no-ops in stock 1.2.2, where nothing is deactivated during play) become no-ops here
# too: the deact pass is the only activation and deactivation in a level room
DEACT_SITES = {'gml_Object_oLevel_Step_0': ['instance_activate_region(', 'instance_activate_object('],
               'gml_Object_oGame_Step_0': ['instance_activate_region(']}
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


def gvar(name):
    """a global as a number (BAD when missing or not a number; booleans as 0 / 1)"""
    return (f'(variable_global_exists("{name}") ? (is_numeric(global.{name}) || is_bool(global.{name}) ? '
            f'real(global.{name}) : {BAD}) : {BAD})')


def ivar(obj, name):
    """obj.name of the object's first instance as a number (BAD when none or not a number; booleans as 0 / 1)"""
    e = f'{obj}.{name}'
    return (f'(instance_exists({obj}) ? (variable_instance_exists({obj}, "{name}") ? (is_numeric({e}) || is_bool({e}) ? '
            f'real({e}) : {BAD}) : {BAD}) : {BAD})')


# the end block (SPT4 flags bit 5, with TRACE_HUD): the game-over panel's (scripts/showEndMessage: oGame's) and the
# ending's (scripts/showFinalScore: oEnd3's; scripts/drawCredits: oCredits2's) state, then globals they draw
END_VARS = [('oGame', 'drawStatus'), ('oGame', 'moneyCount'), ('oEnd3', 'drawStatus'), ('oEnd3', 'moneyCount'),
            ('oEnd3', 'fadeOut'), ('oEnd3', 'fadeLevel'), ('oCredits2', 'drawStatus'), ('oCredits2', 'fadeIn'),
            ('oCredits2', 'fadeOut'), ('oCredits2', 'fadeLevel')]
END_GLOBALS = ['kills', 'damsels']
END_KEYS = [f'{o}.{v}' for o, v in END_VARS] + END_GLOBALS


def ext_gml():
    """GML appending the SPT4 extension block (TRACE_HUD / TRACE_SND) to buffer b"""
    if not (TRACE_HUD or TRACE_SND):
        return ''
    out = f'''
    buffer_write(b, buffer_u32, {(61 if TRACE_HUD else 0) | (2 if TRACE_SND else 0)});'''
    if TRACE_HUD:
        out += f'''
    buffer_write(b, buffer_f64, {gvar('collect')});
    buffer_write(b, buffer_f64, {gvar('messageTimer')});
    buffer_write(b, buffer_f64, {gvar('bloodLevel')});
    buffer_write(b, buffer_f64, {gvar('drawHUD')});'''
        for m, h in (('message1', 'messageHighlights'), ('message2', 'message2Highlights')):
            out += f'''
    {{
        var mm = variable_global_exists("{m}") ? global.{m} : "";
        if (is_array(mm))
        {{
            buffer_write(b, buffer_u8, 1);
            buffer_write(b, buffer_u32, array_length(mm));
            for (var k = 0; k < array_length(mm); k++) buffer_write(b, buffer_string, string(mm[k]));
        }}
        else
        {{
            buffer_write(b, buffer_u8, 0);
            buffer_write(b, buffer_string, string(mm));
        }}
        var hh = variable_global_exists("{h}") ? global.{h} : undefined;
        if (is_array(hh))
        {{
            buffer_write(b, buffer_u32, array_length(hh));
            for (var k = 0; k < array_length(hh); k++) buffer_write(b, buffer_f64, real(hh[k]));
        }}
        else buffer_write(b, buffer_u32, 0);
    }}'''
        out += f'''
    buffer_write(b, buffer_f64, {gvar('xtime')});
    buffer_write(b, buffer_f64, {gvar('time')});
    buffer_write(b, buffer_f64, {gvar('xmoney')});
    buffer_write(b, buffer_f64, instance_exists(oTransition) ? {num('oTransition.drawLoot')} : {BAD});
    buffer_write(b, buffer_f64, instance_exists(oTransition) ? {num('oTransition.moneyCount')} : {BAD});
    buffer_write(b, buffer_f64, instance_exists(oTransition) ? real(oTransition.isLoot) : {BAD});
    buffer_write(b, buffer_f64, instance_exists(oTransition) ? real(oTransition.isKills) : {BAD});
    buffer_write(b, buffer_f64, instance_exists(oDamselKiss) ? real(instance_nearest(176, 176, oDamselKiss).kissed) : {BAD});
    buffer_write(b, buffer_f64, instance_exists(oTunnelMan) ? real(instance_nearest(176, 176, oTunnelMan).talk) : {BAD});
    buffer_write(b, buffer_f64, {gvar('darkLevel')});
    buffer_write(b, buffer_f64, instance_exists(oLevel) ? {num('oLevel.darkness')} : {BAD});
    buffer_write(b, buffer_f64, instance_exists(oPlayer1) ? {num('oPlayer1.distToNearestLightSource')} : {BAD});
    buffer_write(b, buffer_f64, instance_exists(oIntro) ? {num('oIntro.fadeLevel')} : {BAD});
    buffer_write(b, buffer_f64, instance_exists(oIntro) ? {num('oIntro.drawStatus')} : {BAD});
    buffer_write(b, buffer_string, instance_exists(oIntro) ? string(oIntro.str1) : "");
    buffer_write(b, buffer_string, instance_exists(oIntro) ? string(oIntro.str2) : "");
    buffer_write(b, buffer_string, instance_exists(oIntro) ? string(oIntro.str3) : "");'''
        for o, v in END_VARS:
            out += f'''
    buffer_write(b, buffer_f64, {ivar(o, v)});'''
        for v in END_GLOBALS:
            out += f'''
    buffer_write(b, buffer_f64, {gvar(v)});'''
    if TRACE_SND:
        out += '''
    buffer_write(b, buffer_u32, global.trc_sndn);
    for (var k = 0; k < global.trc_sndn; k++)
    {
        var e = global.trc_snd[k];
        buffer_write(b, buffer_u8, e[0]);
        buffer_write(b, buffer_string, e[1]);
        buffer_write(b, buffer_f64, e[2]);
    }
    global.trc_snd = [];
    global.trc_sndn = 0;'''
    return out


# SPT4 per instance, after the variables: f64 cimg (oItem / oDamsel's price-tag frame counter; BAD if absent: the
# var mask is full)
SPT4_INST = f'''
        buffer_write(b, buffer_f64, variable_instance_exists(id, "cimg") ? {num('cimg')} : {BAD});'''


def record(phase):
    """GML writing one record into global.trc_buf"""
    return f'''
{{
    var b = global.trc_buf;
    buffer_write(b, buffer_u32, {MAGIC4 if TRACE_HUD or TRACE_SND else MAGIC});
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
    buffer_write(b, buffer_f64, camera_get_view_x(view_camera[0]));
    buffer_write(b, buffer_f64, camera_get_view_y(view_camera[0]));
    buffer_write(b, buffer_f64, instance_exists(oGame) ? {num('oGame.time')} : {BAD});
    var n = 0;
    var vn = [{', '.join('"' + v + '"' for v in TRACE_VARS)}];
    with (all)
    {{
        n += 1;
        buffer_write(b, buffer_s32, real(id));
        buffer_write(b, buffer_s16, real(object_index));
        buffer_write(b, buffer_f64, x);
        buffer_write(b, buffer_f64, y);
        buffer_write(b, buffer_s16, real(sprite_index));
        buffer_write(b, buffer_f64, image_index);
        buffer_write(b, buffer_f64, image_xscale);
        buffer_write(b, buffer_f64, image_yscale);
        buffer_write(b, buffer_f64, image_angle);
        buffer_write(b, buffer_u32, image_blend);
        buffer_write(b, buffer_f64, image_alpha);
        buffer_write(b, buffer_f64, depth);
        buffer_write(b, buffer_u8, visible ? 1 : 0);
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
        buffer_write(b, buffer_f64, image_speed);
        var vm = 0;
        var vv = [];
        for (var k = 0; k < array_length(vn); k++)
        {{
            vv[k] = 0;
            if (variable_instance_exists(id, vn[k]))
            {{
                var v = variable_instance_get(id, vn[k]);
                try {{ if (is_numeric(v) || is_bool(v) || typeof(v) == "ref") {{ vv[k] = real(v); vm |= (1 << k); }} }}
                catch (e) {{ }}
            }}
        }}
        buffer_write(b, buffer_u32, vm);
        for (var k = 0; k < array_length(vn); k++) if (vm & (1 << k)) buffer_write(b, buffer_f64, vv[k]);{SPT4_INST if TRACE_HUD or TRACE_SND else ''}
    }}
    buffer_poke(b, npos, buffer_u32, n);
    {tiles_gml() if phase == 0 else ''}
    buffer_write(b, buffer_u32, global.trc_evn);
    for (var k = 0; k < global.trc_evn; k++) buffer_write(b, buffer_s32, global.trc_evl[k]);
    global.trc_evn = 0;{ext_gml()}
    global.trc_rec += 1;
}}
'''


def tiles_gml():
    """GML appending the room's tile_add tiles (legacy tile elements: scripts/tile_add -> layer_tile_create) to
    buffer b: u32 count; per tile: string background name, f64 left, top, w, h, x, y, depth (layer depth).
    Order: layer_get_all(), then layer_get_all_elements() per layer"""
    return '''
    {
        var tpos = buffer_tell(b);
        buffer_write(b, buffer_u32, 0);
        var tn = 0;
        var lays = layer_get_all();
        for (var li = 0; li < array_length(lays); li++)
        {
            var els = layer_get_all_elements(lays[li]);
            for (var ei = 0; ei < array_length(els); ei++)
            {
                if (layer_get_element_type(els[ei]) == layerelementtype_tile)
                {
                    var el = els[ei];
                    var reg = layer_tile_get_region(el);
                    tn += 1;
                    buffer_write(b, buffer_string, sprite_get_name(layer_tile_get_sprite(el)));
                    buffer_write(b, buffer_f64, reg[0]);
                    buffer_write(b, buffer_f64, reg[1]);
                    buffer_write(b, buffer_f64, reg[2]);
                    buffer_write(b, buffer_f64, reg[3]);
                    buffer_write(b, buffer_f64, layer_tile_get_x(el));
                    buffer_write(b, buffer_f64, layer_tile_get_y(el));
                    buffer_write(b, buffer_f64, layer_get_depth(lays[li]));
                }
            }
        }
        buffer_poke(b, tpos, buffer_u32, tn);
    }
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
    if os.environ.get('TRACE_LEVEL'):
        reseed = f'global.currLevel = {int(os.environ["TRACE_LEVEL"])};\n    ' + reseed
    if os.environ.get('TRACE_NODARK') == '0':
        reseed = 'global.noDarkLevel = false;\n    ' + reseed
    if os.environ.get('TRACE_MONEY'):
        reseed = f'global.money = {int(os.environ["TRACE_MONEY"])};\n    ' + reseed
    for kv in filter(None, os.environ.get('TRACE_GLOBALS', '').split(',')):
        k, v = kv.split('=', 1)
        try:
            float(v)
        except ValueError:
            v = '"' + v.replace('~', ' ') + '"'
        reseed = f'global.{k} = {v};\n    ' + reseed
    noenemy = ''
    if os.environ.get('TRACE_NOENEMY') == '1':
        noenemy = ('if (room == rLevel || room == rLevel2 || room == rLevel3 || room == rOlmec)\n{\n' +
                   ''.join(f'    with ({o}) instance_destroy(id, false);\n' for o in NOENEMY_OBJS) + '}')
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
global.trc_evl = [];
global.trc_evn = 0;
{'global.trc_dl = [];' + chr(10) if DEACT else ''}{'global.trc_snd = [];' + chr(10) + 'global.trc_sndn = 0;' + chr(10) if TRACE_SND else ''}global.trc_ends = [{', '.join(ends) or '0'}];
global.trc_masks = [{', '.join(masks) or '0'}];
'''
    keys = ''.join(f'''
{f}Released = false;
{f}Pressed = false;
if ({f} && !(m & {bit})) {f}Released = true;
if (!{f} && (m & {bit})) {f}Pressed = true;
{f} = ((m & {bit}) != 0);
''' for bit, f in KEYS.values())
    flow = '' if BOOT else f'''
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
    global.gameStart = {'true' if ROOM in ('rLevel', 'rOlmec') else 'false'};
    {reseed}
    room_goto({ROOM});
}}'''
    step = f'''{flow}
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
if (!global.trc_on && {'room == rIntro' if BOOT else 'global.trc_phase == 2 && room == ' + ROOM})
{{
    global.trc_on = 1;
    global.trc_buf = buffer_create(1048576, buffer_grow, 1);
}}
if (!global.trc_on) exit;
if (room != global.trc_lastroom)
{{
    global.trc_lastroom = room;
    {noenemy}
    {record(0)}
    {tree_gml()}
    {'global.trc_dl = [];' if DEACT else ''}
}}
{f'else if (room == rLevel || room == rLevel2 || room == rLevel3) trcDeactPass({int(DEACT)});' if DEACT else ''}
'''
    end = f'''
if (global.trc_done || !global.trc_on) exit;
{record(1)}
{tree_gml()}
if (global.trc_t >= {total + TAIL} || global.trc_rec >= {MAX_STEPS})
{{
    {save_chunk(False)}
    var df = file_text_open_write("trc_done.txt");
    file_text_write_string(df, string(global.trc_chunk));
    file_text_close(df);
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


def tree_gml():
    """TRACE_TREE=r1,r2,...: after record r, tree_<r>.txt: per object of TRACE_TREE_OBJS a line
    "<object> <n> <id> ..." from collision_rectangle_list(-100000, -100000, 100000, 100000, obj, 0, 0, l, false)"""
    recs = [int(r) for r in os.environ.get('TRACE_TREE', '').split(',') if r]
    if not recs:
        return ''
    objs = os.environ.get('TRACE_TREE_OBJS', 'oSolid').split(',')
    cond = ' || '.join(f'trr == {r}' for r in recs)
    body = ''.join(f'''
    ds_list_clear(trl);
    trn = collision_rectangle_list(-100000, -100000, 100000, 100000, {o}, false, false, trl, false);
    file_text_write_string(trf, "{o} " + string(trn));
    for (var trk = 0; trk < ds_list_size(trl); trk++) file_text_write_string(trf, " " + string(real(ds_list_find_value(trl, trk))));
    file_text_writeln(trf);''' for o in objs)
    return f'''
{{
var trr = global.trc_rec - 1;
if ({cond})
{{
    var trf = file_text_open_write("tree_" + string(trr) + ".txt");
    var trl = ds_list_create();
    var trn = 0;{body}
    ds_list_destroy(trl);
    file_text_close(trf);
}}
}}
'''


def shot_gml():
    """TRACE_SHOT=r1,r2,...: oGamepad Post-Draw (new) saves application_surface as shot_<r>.png in the frame
    whose End Step wrote record r (phase 1), and shot_<r>.txt: view x, y, w, h and the surface's size"""
    recs = [int(r) for r in os.environ.get('TRACE_SHOT', '').split(',') if r]
    if not recs:
        return ''
    cond = ' || '.join(f'r == {r}' for r in recs)
    return f'''
if (!global.trc_on) exit;
var r = global.trc_rec - 1;
if ({cond})
{{
    surface_save(application_surface, "shot_" + string(r) + ".png");
    var f = file_text_open_write("shot_" + string(r) + ".txt");
    file_text_write_string(f, string(view_xview[0]) + " " + string(view_yview[0]) + " " + string(view_wview[0]) + " "
        + string(view_hview[0]) + " " + string(surface_get_width(application_surface)) + " "
        + string(surface_get_height(application_surface)));
    file_text_close(f);
}}
'''


def gui_gml():
    """TRACE_GUI=r1,r2,...: oGamepad Draw GUI End (new) saves gui_<r>.png at record r (see the module doc)"""
    recs = [int(r) for r in os.environ.get('TRACE_GUI', '').split(',') if r]
    if not recs:
        return ''
    cond = ' || '.join(f'r == {r}' for r in recs)
    return f'''
if (!global.trc_on) exit;
var r = global.trc_rec - 1;
if ({cond})
{{
    var mt = global.messageTimer;
    var nd = global.noDarkLevel;
    var w = surface_get_width(application_surface), h = surface_get_height(application_surface);
    var s = surface_create(w, h);
    surface_set_target(s);
    draw_clear_alpha(c_black, 1);
    gpu_set_blendenable(false);
    draw_surface(application_surface, 0, 0);
    gpu_set_blendenable(true);
    with (oGame) {{ scrDrawHUD(); showMessages(); showEndMessage(); }}
    with (oTransition) event_perform(ev_draw, ev_gui);
    with (oTitle) event_perform(ev_draw, ev_gui);
    with (oHighscores) event_perform(ev_draw, ev_gui);
    with (oCredits2) event_perform(ev_draw, ev_gui);
    surface_reset_target();
    surface_save(s, "gui_" + string(r) + ".png");
    surface_free(s);
    global.messageTimer = mt;
    global.noDarkLevel = nd;
}}
'''


def csx(create, step, begin, end, seed, names):
    q = lambda s: '@"' + s.replace('"', '""') + '"'
    shot = shot_gml()
    shots = f'g.QueueReplace("gml_Object_oGamepad_Draw_77", {q(shot)});\n' if shot else ''
    gui = gui_gml()
    if gui:
        shots += f'g.QueueReplace("gml_Object_oGamepad_Draw_75", {q(gui)});\n'
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
{shots}{evlog_csx() if os.environ.get('TRACE_EVLOG') == '1' else ''}{genprobe_csx(q)}{treeat_csx(q)}{snd_csx(q)}{deact_csx(q)}
File.WriteAllText({q(names)}, sb.ToString());
g.Import();
'''


SND_WRAP = '''
function trcSnd(k, s, a) {
    if (variable_global_exists("trc_on") && (global.trc_phase == 2 || global.trc_on) && !global.trc_done) {
        var nm = "";
        if (!is_undefined(s)) { try { nm = audio_get_name(s); } catch (e) { nm = "?"; } }
        global.trc_snd[global.trc_sndn] = [k, nm, a];
        global.trc_sndn += 1;
    }
}
function trcStopSound(s) { trcSnd(6, s, 0); return audio_stop_sound(s); }
function trcPauseAll() { trcSnd(7, undefined, 0); return audio_pause_all(); }
function trcResumeAll() { trcSnd(8, undefined, 0); return audio_resume_all(); }
function trcStopAll() { trcSnd(9, undefined, 0); return audio_stop_all(); }
function trcPlaySound(s, p, l) { trcSnd(10, s, p * 2 + (l ? 1 : 0)); return audio_play_sound(s, p, l); }
'''
# script -> the log call put first in its function body (SND_KINDS index, asset argument, extra argument)
SND_SCRIPTS = {'playSound': 'trcSnd(1, argument0, 0);', 'playMusic': 'trcSnd(2, argument0, argument1 ? 1 : 0);',
               'startMusic': 'trcSnd(3, undefined, 0);', 'stopAllMusic': 'trcSnd(4, undefined, 0);',
               'setSoundVol': 'trcSnd(5, argument0, argument1);'}


def deact_gml():
    """TRACE_DEACT: gml_GlobalScript_trcDeact (docs/DEACT.md). trcDeactPass runs in oGamepad's Begin Step of every
    level room step but the room's first: (1) every instance it deactivated whose stored (x, y) is inside the region
    is activated, in the order they were deactivated; (2) every active candidate whose (x, y) is outside it is
    deactivated, in with (all) order. Region: the view the last draw left, grown by the margin on each side"""
    roots = ' || '.join(f'o == {r} || object_is_ancestor(o, {r})' for r in DEACT_ROOTS)
    ex = ' || '.join(f'o == {e}' for e in DEACT_EXEMPT)
    probe = '' if os.environ.get('TRACE_DEACT_PROBE') != '1' else '''
    if (nc > 0 && !variable_global_exists("trc_dprobe"))
    {
        global.trc_dprobe = 1;
        var s = "deact " + string(real(cand[0][0]));
        try { s += " obj " + object_get_name(cand[0][0].object_index) + " x " + string(cand[0][0].x); } catch (e) { s += " read error: " + string(e.message); }
        s += " exists " + string(instance_exists(cand[0][0]));
        var pf = file_text_open_write("deact_probe.txt"); file_text_write_string(pf, s); file_text_close(pf);
    }'''
    return f'''
function trcNoActivate() {{ }}
function trcDeactOk()
{{
    var o = object_index;
    if (!({roots})) return false;
    if ({ex}) return false;
    if (variable_instance_exists(id, "held") && held) return false;
    if (variable_instance_exists(id, "forSale") && forSale) return false;
    return true;
}}
function trcDeactPass(m)
{{
    var cam = view_camera[0];
    var x0 = camera_get_view_x(cam) - m, y0 = camera_get_view_y(cam) - m;
    var x1 = camera_get_view_x(cam) + camera_get_view_width(cam) + m, y1 = camera_get_view_y(cam) + camera_get_view_height(cam) + m;
    var dl = global.trc_dl, keep = [], nk = 0;
    for (var k = 0; k < array_length(dl); k++)
    {{
        var e = dl[k];
        if (e[1] < x0 || e[1] > x1 || e[2] < y0 || e[2] > y1) {{ keep[nk] = e; nk += 1; }}
        else instance_activate_object(e[0]);
    }}
    var cand = [], nc = 0;
    with (all)
    {{
        if ((x < x0 || x > x1 || y < y0 || y > y1) && trcDeactOk()) {{ cand[nc] = [id, x, y]; nc += 1; }}
    }}
    for (var k = 0; k < nc; k++) {{ instance_deactivate_object(cand[k][0]); keep[nk] = cand[k]; nk += 1; }}
    global.trc_dl = keep;{probe}
}}
'''


def deact_csx(q):
    if not DEACT:
        return ''
    out = f'g.QueueReplace("gml_GlobalScript_trcDeact", {q(deact_gml())});\n'
    for site, calls in DEACT_SITES.items():
        for c in calls:
            out += f'g.QueueFindReplace("{site}", {q(c)}, {q("trcNoActivate(")});\n'
    return out


def snd_csx(q):
    """TRACE_SND=1: the logging wrappers (gml_GlobalScript_trcSnd), the five audio scripts replaced by HD's source
    (refs/hd/src/scripts) with the log call first, the builtin calls of SND_SITES renamed to the wrappers"""
    if not TRACE_SND:
        return ''
    src = os.path.join(HERE, '..', 'refs', 'hd', 'src', 'scripts')
    out = f'g.QueueReplace("gml_GlobalScript_trcSnd", {q(SND_WRAP)});\n'
    for name, call in SND_SCRIPTS.items():
        t = open(os.path.join(src, name, name + '.gml'), encoding='utf-8').read()
        k = t.index('{', t.index('function ' + name)) + 1
        out += f'g.QueueReplace("gml_GlobalScript_{name}", {q(t[:k] + chr(10) + "    " + call + t[k:])});\n'
    for site in SND_SITES:
        for a, b in (('audio_stop_sound(', 'trcStopSound('), ('audio_pause_all(', 'trcPauseAll('),
                     ('audio_resume_all(', 'trcResumeAll('), ('audio_stop_all(', 'trcStopAll('),
                     ('audio_play_sound(', 'trcPlaySound(')):
            out += f'g.QueueFindReplace("{site}", {q(a)}, {q(b)});\n'
    return out


def genprobe_csx(q):
    """TRACE_GENPROBE=1 or "a1|a2|..." (probe runs only): in scrInitLevel, before the first occurrence of each anchor
    (decompiled text; 1 = "scrEntityGen();"), gprobe_<k>.txt: the oSolid instances in the collision tree's search
    order (collision_rectangle_list, which flushes the dirty list)"""
    v = os.environ.get('TRACE_GENPROBE')
    if not v:
        return ''
    anchors = ['scrEntityGen();'] if v == '1' else v.split('|')
    out = ''
    for k, a in enumerate(anchors):
        probe = ('{ var gl = ds_list_create(); var gn = collision_rectangle_list(-100000, -100000, 100000, 100000, oSolid, '
                 'false, false, gl, false); var gf = file_text_open_write("gprobe_' + str(k) + '.txt"); var gs = string(gn); '
                 'for (var gk = 0; gk < ds_list_size(gl); gk++) gs += " " + string(real(ds_list_find_value(gl, gk))); '
                 'file_text_write_string(gf, gs); file_text_close(gf); ds_list_destroy(gl); }\n')
        out += f'g.QueueFindReplace("gml_GlobalScript_scrInitLevel", {q(a)}, {q("{ " + probe + a + " }")});\n'
    return out

def treeat_csx(q):
    """TRACE_TREEAT=<code entry>:<prepend|append>,... with TRACE_TREE=r1,...: those events also write
    treeat_<r>_<k>.txt (the oSolid search order, as TRACE_TREE) while the step that writes record r runs (probe runs)"""
    v = os.environ.get('TRACE_TREEAT')
    recs = [int(r) for r in os.environ.get('TRACE_TREE', '').split(',') if r]
    if not v or not recs:
        return ''
    cond = ' || '.join(f'global.trc_rec == {r}' for r in recs)
    out = ''
    for k, e in enumerate(v.split(',')):
        code, where, *only = e.split(':')
        idc = f' && real(id) == {only[0]}' if only else ''
        probe = ('if (variable_global_exists("trc_on") && global.trc_on && (' + cond + ')' + idc + ') { var tal = ds_list_create(); '
                 'var tan = collision_rectangle_list(-100000, -100000, 100000, 100000, ' + os.environ.get('TRACE_TREE_OBJS', 'oSolid').split(',')[0] + ', false, false, tal, false); '
                 'var taf = file_text_open_write("treeat_" + string(global.trc_rec) + "_' + str(k) + '_" + string(real(id)) + ".txt"); '
                 'var tas = string(tan); for (var tak = 0; tak < ds_list_size(tal); tak++) tas += " " + '
                 'string(real(ds_list_find_value(tal, tak))); file_text_write_string(taf, tas); file_text_close(taf); '
                 'ds_list_destroy(tal); }\n')
        out += f'g.Queue{"Prepend" if where == "prepend" else "Append"}({q(code)}, {q(probe)});\n'
    return out

def evlog_csx():
    """TRACE_EVLOG=1 (probe runs only: every logged entry is decompiled and recompiled): every object event
    except Draw, oGamepad's, oScreen's and oIntro's gets a first statement appending id * 4096 + k to the record's
    event log while the trace is on; the names file gets "C <k> <code entry>" lines"""
    return '''
int evk = 0;
foreach (var c in Data.Code)
{
    if (c.ParentEntry != null) continue;
    var nm = c.Name.Content;
    var m = System.Text.RegularExpressions.Regex.Match(nm, "^gml_Object_(.+)_(Step|Alarm|Collision|Other|Create|Destroy)_");
    if (!m.Success) continue;
    var o = m.Groups[1].Value;
    if (o == "oGamepad" || o == "oScreen" || o == "oIntro") continue;
    sb.Append($"C {evk} {nm}\\n");
    g.QueuePrepend(nm, "if (variable_global_exists(\\"trc_on\\") && global.trc_on) { global.trc_evl[global.trc_evn] = real(id) * 4096 + " + evk + "; global.trc_evn += 1; }\\n");
    evk++;
}
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


PROBE_SEEDS = [0, 1, 2, 12345, -1, 2147483647]
PROBE_N = 2000
PROBE_NS = [3, 7, 13, 0.7, 100.5, 1000000.3]
PROBE_IS = [1, 10, 99, -5]
# per seed: f64 seed, f64 random_get_seed(), then three blocks, each right after random_set_seed(seed):
# A: PROBE_N x random(1); B: PROBE_N x random(4294967296); C: PROBE_N x irandom(2147483647);
# D: PROBE_N x random(PROBE_NS[k % 6]); E: PROBE_N x irandom(PROBE_IS[k % 4])
PROBE_GML = f'''
var seeds = {PROBE_SEEDS};
var ns = {PROBE_NS};
var is = {PROBE_IS};
var b = buffer_create(1048576, buffer_grow, 1);
for (var s = 0; s < array_length(seeds); s++)
{{
    random_set_seed(seeds[s]);
    buffer_write(b, buffer_f64, seeds[s]);
    buffer_write(b, buffer_f64, random_get_seed());
    for (var i = 0; i < {PROBE_N}; i++) buffer_write(b, buffer_f64, random(1));
    random_set_seed(seeds[s]);
    for (var i = 0; i < {PROBE_N}; i++) buffer_write(b, buffer_f64, random(4294967296));
    random_set_seed(seeds[s]);
    for (var i = 0; i < {PROBE_N}; i++) buffer_write(b, buffer_f64, irandom(2147483647));
    random_set_seed(seeds[s]);
    for (var i = 0; i < {PROBE_N}; i++) buffer_write(b, buffer_f64, random(ns[i mod 6]));
    random_set_seed(seeds[s]);
    for (var i = 0; i < {PROBE_N}; i++) buffer_write(b, buffer_f64, irandom(is[i mod 4]));
}}
buffer_save(b, "rng_probe.bin");
game_end();
'''


def rng_probe(droid, out):
    """build: a copy of the data file whose oGamepad Create writes rng_probe.bin (PROBE_GML) and ends"""
    droid, out = map(os.path.abspath, (droid, out))
    os.makedirs(os.path.dirname(out), exist_ok=True)
    root = os.path.commonpath([droid, UTMT, out])
    rel = lambda p: '/w/' + os.path.relpath(p, root)
    q = lambda s: '@"' + s.replace('"', '""') + '"'
    script = out + '.csx'
    open(script, 'w').write(f'''
using UndertaleModLib.Compiler;
CodeImportGroup g = new(Data);
g.QueueAppend("gml_Object_oGamepad_Create_0", {q(PROBE_GML)});
g.Import();
''')
    if os.path.exists(out):
        os.remove(out)
    r = subprocess.run(['docker', 'run', '--rm', '-v', f'{root}:/w', DOTNET, 'dotnet', rel(UTMT), 'load', rel(droid),
                        '-s', rel(script), '-o', rel(out), '-f'], capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(out) or re.search(r'(?i)error|exception', r.stdout + r.stderr):
        sys.exit(r.stdout[-4000:] + r.stderr[-4000:])
    print(f'{out}: RNG probe, seeds {PROBE_SEEDS}, {PROBE_N} draws x 5 blocks')


def load_names(path):
    names = {'O': {}, 'S': {}, 'R': {}, 'C': {}}
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
        if h[0] not in (MAGIC, MAGIC2, MAGIC1, MAGIC4):
            raise ValueError(f'bad magic at {o - H.size}')
        v2 = h[0] != MAGIC1
        v3 = h[0] in (MAGIC, MAGIC4)
        hd = dict(zip(['magic', 'phase', 'rec', 't', 'input', 'room', 'currLevel', 'seed', 'plife', 'bombs',
                       'rope', 'money', 'n'], h))
        if v3:
            hd['xview'], hd['yview'], hd['time'] = struct.unpack_from('<3d', data, o)
            o += 24
        insts = []
        for _ in range(hd['n']):
            iid, obj, x, y, spr, img = struct.unpack_from('<ihddhd', data, o)
            o += 32
            draw = None
            if v2:
                xs, ys, ang, blend, alpha, depth, vis = struct.unpack_from('<dddIddB', data, o)
                o += 45
                draw = dict(xscale=xs, yscale=ys, angle=ang, blend=blend, alpha=alpha, depth=depth, visible=vis)
            (am,) = struct.unpack_from('<H', data, o)
            o += 2
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
            ins = dict(id=iid, obj=obj, x=x, y=y, spr=spr, img=img, alarms=alarms, xVel=vel[0], yVel=vel[1],
                       draw=draw, vars={})
            if v3:
                ins['ispd'], vm = struct.unpack_from('<dI', data, o)
                o += 12
                for k, nm in enumerate(TRACE_VARS):
                    if vm & (1 << k):
                        ins['vars'][nm] = struct.unpack_from('<d', data, o)[0]
                        o += 8
                if h[0] == MAGIC4:
                    (cv,) = struct.unpack_from('<d', data, o)
                    o += 8
                    if cv > BAD:
                        ins['vars']['cimg'] = cv
            insts.append(ins)
        hd['tiles'] = []
        if v2 and hd['phase'] == 0:
            (tn,) = struct.unpack_from('<I', data, o)
            o += 4
            for _ in range(tn):
                e = data.index(b'\0', o)
                bg = data[o:e].decode('utf-8', 'replace')
                o = e + 1
                hd['tiles'].append((bg,) + struct.unpack_from('<7d', data, o))
                o += 56
        hd['ev'] = []
        if v3:
            (ne,) = struct.unpack_from('<I', data, o)
            o += 4
            hd['ev'] = list(struct.unpack_from(f'<{ne}i', data, o))
            o += 4 * ne
        if h[0] == MAGIC4:
            (fl,) = struct.unpack_from('<I', data, o)
            o += 4

            def sz():
                nonlocal o
                e = data.index(b'\0', o)
                v = data[o:e].decode('utf-8', 'replace')
                o = e + 1
                return v
            if fl & 1:
                hud = dict(zip(['collect', 'messageTimer', 'bloodLevel', 'drawHUD'], struct.unpack_from('<4d', data, o)))
                o += 32
                for m in ('message1', 'message2'):
                    t = data[o]
                    o += 1
                    if t:
                        (n,) = struct.unpack_from('<I', data, o)
                        o += 4
                        hud[m] = [sz() for _ in range(n)]
                    else:
                        hud[m] = sz()
                    (n,) = struct.unpack_from('<I', data, o)
                    o += 4
                    hud[m + '_hl'] = list(struct.unpack_from(f'<{n}d', data, o))
                    o += 8 * n
                if fl & 4:
                    hud.update(zip(['xtime', 'time', 'xmoney', 'drawLoot', 'moneyCount', 'isLoot', 'isKills',
                                    'kissed', 'talk'], struct.unpack_from('<9d', data, o)))
                    o += 72
                if fl & 8:
                    hud.update(zip(['darkLevel', 'darkness', 'distLight'], struct.unpack_from('<3d', data, o)))
                    o += 24
                if fl & 16:
                    hud['fadeLevel'], hud['drawStatus'] = struct.unpack_from('<2d', data, o)
                    o += 16
                    hud['str'] = [sz() for _ in range(3)]
                if fl & 32:
                    n = len(END_KEYS)
                    hud.update(zip(END_KEYS, struct.unpack_from(f'<{n}d', data, o)))
                    o += 8 * n
                hd['hud'] = hud
            if fl & 2:
                (n,) = struct.unpack_from('<I', data, o)
                o += 4
                calls = []
                for _ in range(n):
                    k = data[o]
                    o += 1
                    a = sz()
                    (x,) = struct.unpack_from('<d', data, o)
                    o += 8
                    calls.append((SND_KINDS[k] if k < len(SND_KINDS) else str(k), a, x))
                hd['snd'] = calls
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
              f"n {hd['n']}" + (f" view {g(hd['xview'])} {g(hd['yview'])} time {g(hd['time'])}" if 'xview' in hd else ''))
        for e in hd['ev']:
            print(f"  ev {e >> 12} {names['C'].get(e & 4095, e & 4095)}")
        if not inst:
            continue
        for i in insts:
            al = ' '.join(f'a{k}={g(v)}' for k, v in i['alarms'].items())
            vel = '' if i['xVel'] is None and i['yVel'] is None else f" vel {g(i['xVel'])} {g(i['yVel'])}"
            d = i['draw']
            dr = '' if d is None else (f" sc {g(d['xscale'])} {g(d['yscale'])} ang {g(d['angle'])} "
                                       f"blend {d['blend']:06x} alpha {g(d['alpha'])} depth {g(d['depth'])} "
                                       f"vis {d['visible']}")
            ext = ''
            if 'ispd' in i:
                ext = f" ispd {g(i['ispd'])}" + ''.join(f" {k}={g(v)}" for k, v in i['vars'].items())
            print(f"  {i['id']} {names['O'].get(i['obj'], i['obj'])} {g(i['x'])} {g(i['y'])} "
                  f"{names['S'].get(i['spr'], i['spr'])} {g(i['img'])}{dr}{(' ' + al) if al else ''}{vel}{ext}")
        for t in hd['tiles']:
            print(f"  tile {t[0]} {' '.join(g(v) for v in t[1:])}")


# ---------------------------------------------------------------------------------------------------------------
# Generator mode (P2): `build-gen <game.unx> <cases.txt> <out.unx>`, scripts/hd_trace.sh --gen <cases> [name].
# One runner process generates every case of the cases file in turn. oGame Create's `scrInitLevel()` call is
# wrapped (the decompiled text "scrInitLevel();" is replaced): for case k the starting globals are set (new game:
# scrClearGlobals() plus the variables it leaves alone; or carried over from case k-1; then the case's overrides)
# and random_set_seed(seed) is called at the start of the Create event of the room's first instance (oPlayer1 in
# rLevel / rLevel3, oBlackBG in rLevel2, oEntrance in rOlmec): the room's own instances (rOlmec's temple blocks and
# lava draw from the RNG before oGame) and oLevel Create (held item, Kali ball) see the case's state. Then
# scrInitLevel() runs (oGame Create) and the room's instances and tiles are dumped right after it returns (before
# any Step), followed by 4 x random(2^32) (= the next 4 raw WELL512a words) and the `with` iteration orders of a
# few objects. The case's room is the one the game uses for that level (gen_room: rLevel; rLevel2 for levels 9-12;
# rLevel3 when global.lake; rOlmec for level 16).
# oGamepad Begin Step then goes to case k + 1's room (room_restart if it is the same room) or ends the game. The
# rest of the run is the normal flow (rIntro -> rTitle -> the first case's room, no keys pressed).
#
# cases.txt: lines "<seed> <level> <cont> <noDarkLevel> [name=value ...]"; cont 0 = new-game globals, 1 = globals as
# left by the previous case (a level chain); name=value sets global.<name> (a number, or s:<text> for a string,
# "~" standing for a space).
# Output gen_<k>.bin per case (little-endian):
#   u32 magic 0x324e4753 ("SGN2"), s32 case, f64 seed, f64 level
#   f64 GEN_GLOBALS[...], then oGame.GEN_OGAME[...] (after scrInitLevel; -1e9 if not a number)
#   f64 roomPath[i, j] for i = 0..3 (room column), j = 0..4 (room row; row 4 is the lake row)
#   u32 n; per instance (with (all) order):
#     s32 id, s16 object_index, f64 x, f64 y, s16 sprite_index, f64 depth, f64 image_speed, u8 visible,
#     u8 persistent, u16 alarm mask, f64 alarm[k] for each set bit,
#     u32 var mask (bit v: GEN_VARS[v] exists and is numeric), f64 value for each set bit,
#     u8 string mask (bit s: GEN_SVARS[s] exists and is a string), string (buffer_string) for each set bit
#   tiles: u32 count, per tile: string background, f64 left, top, w, h, x, y, depth (tiles_gml)
#   f64 x 4: random(4294967296) x 4
#   TRACE_GEN_ORDER=all only: u32 0xFFFFFFFF, then for every object with an instance: s32 object, u32 count,
#   s32 id x count (with (obj) order); s32 -1
#   per object in GEN_ORDER: u32 count, s32 id x count (with (obj) order)
GEN_MAGIC = 0x324e4753
GEN_GLOBALS = ['darkLevel', 'hadDarkLevel', 'genUdjatEye', 'madeUdjatEye', 'genMarketEntrance', 'snakePit',
               'shop', 'startRoomX', 'startRoomY', 'endRoomX', 'endRoomY', 'exitX', 'exitY', 'cemetary',
               'giantSpider', 'genGiantSpider', 'LockedChest', 'Key', 'lockedChestChance', 'blackMarket',
               'sacrificePit', 'alienCraft', 'yetiLair', 'levelType', 'noDarkLevel', 'lake', 'marketChance',
               'madeMarketEntrance', 'madeBlackMarket', 'madeMoai', 'ashGrave', 'TombLord', 'genTombLord',
               'genGoldEntrance', 'madeGoldEntrance', 'goldChance', 'cityOfGold', 'thiefLevel', 'murderer',
               'checkWater', 'cleanSolids']
GEN_OGAME = ['damsel', 'idol', 'altar', 'genSupplyShop', 'genBombShop', 'genWeaponShop', 'genRareShop',
             'genClothingShop']
GEN_VARS = ['invincible', 'status', 'cost', 'forSale', 'shopWall', 'value', 'inDiceHouse', 'cleanDeath',
            'linkVal', 'trigger', 'xVel', 'yVel', 'facing', 'counter', 'spurt', 'deathTimer', 'held', 'swimming',
            'dir', 'spurtTime', 'shiftToggle', 'New']
GEN_SVARS = ['type', 'style', 'treasure']
GEN_ORDER = ['all', 'oSolid', 'oBlock', 'oBrick', 'oTreasure', 'oExit', 'oEntrance', 'oItem', 'oEnemy']


def parse_cases(path):
    """[(seed, level, cont, noDarkLevel, {name: value})], value an int or a str"""
    cs = []
    for line in open(path):
        f = line.split('#')[0].split()
        if not f:
            continue
        pos = [t for t in f if '=' not in t]
        seed, level, cont, nodark = (pos + ['0', '1'])[:4]
        ov = {}
        for t in f:
            if '=' in t:
                k, v = t.split('=', 1)
                ov[k] = v[2:].replace('~', ' ') if v.startswith('s:') else int(v)
        cs.append((int(seed), int(level), int(cont), int(nodark), ov))
    return cs


def case_room(level, ov, lake):
    if level == 16:
        return 'rOlmec'
    if 9 <= level <= 12:
        return 'rLevel2'
    if lake:
        return 'rLevel3'
    return 'rLevel'


def gml_value(v):
    return '"' + v.replace('"', '') + '"' if isinstance(v, str) else str(v)


def gen_dump_gml():
    gl = '\n'.join(f'    buffer_write(b, buffer_f64, (variable_global_exists("{g}") && is_numeric(global.{g})) ? '
                   f'global.{g} : {BAD});' for g in GEN_GLOBALS)
    og = '\n'.join(f'    buffer_write(b, buffer_f64, {num("oGame." + g)});' for g in GEN_OGAME)
    vars_ = ', '.join(f'"{v}"' for v in GEN_VARS)
    svars = ', '.join(f'"{v}"' for v in GEN_SVARS)
    if os.environ.get('TRACE_GEN_ORDER') == 'all':
        # every object with an instance (parents included): u32 0xFFFFFFFF, then per object: s32 object, u32 n, ids
        return_all = '''
    buffer_write(b, buffer_u32, 4294967295);
    for (var oi = 0; object_exists(oi); oi++)
    {
        if (!instance_exists(oi)) continue;
        buffer_write(b, buffer_s32, oi);
        npos = buffer_tell(b);
        buffer_write(b, buffer_u32, 0);
        n = 0;
        with (oi) { n += 1; buffer_write(b, buffer_s32, real(id)); }
        buffer_poke(b, npos, buffer_u32, n);
    }
    buffer_write(b, buffer_s32, -1);'''
    else:
        return_all = ''
    order = return_all + ''.join(f'''
    npos = buffer_tell(b);
    buffer_write(b, buffer_u32, 0);
    n = 0;
    with ({o}) {{ n += 1; buffer_write(b, buffer_s32, real(id)); }}
    buffer_poke(b, npos, buffer_u32, n);''' for o in GEN_ORDER)
    return f'''
{{
    var b = global.gen_buf;
    buffer_seek(b, buffer_seek_start, 0);
    buffer_write(b, buffer_u32, {GEN_MAGIC});
    buffer_write(b, buffer_s32, global.gen_k);
    buffer_write(b, buffer_f64, global.gen_seed[global.gen_k]);
    buffer_write(b, buffer_f64, global.currLevel);
{gl}
{og}
    for (var i = 0; i < 4; i++) for (var j = 0; j < 5; j++)
    {{
        var rp = -1000000000;
        if (i < array_length(global.roomPath) && j < array_length(global.roomPath[i])) rp = global.roomPath[i, j];
        buffer_write(b, buffer_f64, {num("rp")});
    }}
    var vn = [{vars_}];
    var sn = [{svars}];
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
        buffer_write(b, buffer_f64, depth);
        buffer_write(b, buffer_f64, image_speed);
        buffer_write(b, buffer_u8, visible ? 1 : 0);
        buffer_write(b, buffer_u8, persistent ? 1 : 0);
        var am = 0;
        for (var k = 0; k < 12; k++) if (alarm[k] != -1) am |= (1 << k);
        buffer_write(b, buffer_u16, am);
        for (var k = 0; k < 12; k++) if (am & (1 << k)) buffer_write(b, buffer_f64, alarm[k]);
        var vm = 0;
        for (var k = 0; k < array_length(vn); k++)
            if (variable_instance_exists(id, vn[k]) && is_numeric(variable_instance_get(id, vn[k]))) vm |= (1 << k);
        buffer_write(b, buffer_u32, vm);
        for (var k = 0; k < array_length(vn); k++) if (vm & (1 << k)) buffer_write(b, buffer_f64, real(variable_instance_get(id, vn[k])));
        var sm = 0;
        for (var k = 0; k < array_length(sn); k++)
            if (variable_instance_exists(id, sn[k]) && is_string(variable_instance_get(id, sn[k]))) sm |= (1 << k);
        buffer_write(b, buffer_u8, sm);
        for (var k = 0; k < array_length(sn); k++) if (sm & (1 << k)) buffer_write(b, buffer_string, variable_instance_get(id, sn[k]));
    }}
    buffer_poke(b, npos, buffer_u32, n);
{tiles_gml()}
    for (var k = 0; k < 4; k++) buffer_write(b, buffer_f64, random(4294967296));
{order}
    buffer_save_ext(b, "gen_" + string(global.gen_k) + ".bin", 0, buffer_tell(b));
}}
'''


def gml_gen(cases):
    arr = lambda k: '[' + ', '.join(str(c[k]) for c in cases) + ']'
    rooms = []
    lake = 0
    for c in cases:
        if not c[2]:
            lake = 0
        lake = c[4].get('lake', lake)
        rooms.append(case_room(c[1], c[4], lake))

    def ovs():
        return '[' + ', '.join('[' + ', '.join(f'["{k}", {gml_value(v)}]' for k, v in c[4].items()) + ']'
                               for c in cases) + ']'
    create = f'''
exception_unhandled_handler(function(e) {{
    var ef = file_text_open_write("gml_error.txt");
    file_text_write_string(ef, e.longMessage);
    file_text_close(ef);
    game_end();
    return 0;
}});
global.trc_phase = 0;
global.gen_on = 0;
global.gen_k = 0;
global.gen_n = {len(cases)};
global.gen_seed = {arr(0)};
global.gen_level = {arr(1)};
global.gen_cont = {arr(2)};
global.gen_nodark = {arr(3)};
global.gen_room = [{', '.join(rooms)}];
global.gen_ov = {ovs()};
global.gen_seeded = 0;
global.gen_buf = buffer_create(262144, buffer_grow, 1);
'''
    keys = ''.join(f'{f}Released = false;\n{f}Pressed = false;\n{f} = false;\n' for _, f in KEYS.values())
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
    global.gen_on = 1;
    room_goto(global.gen_room[0]);
}}
{keys}
'''
    begin = f'''
if (global.gen_on && (room == rLevel || room == rLevel2 || room == rLevel3 || room == rOlmec))
{{
    global.gen_k += 1;
    if (global.gen_k >= global.gen_n)
    {{
        global.gen_on = 0;
        game_end();
        exit;
    }}
    global.gen_seeded = 0;
    if (global.gen_room[global.gen_k] == room) room_restart();
    else room_goto(global.gen_room[global.gen_k]);
}}
'''
    setup = '''
if (global.gen_on && !global.gen_seeded && room == global.gen_room[global.gen_k])
{
    global.gen_seeded = 1;
    var k = global.gen_k;
    if (!global.gen_cont[k])
    {
        scrClearGlobals();
        global.hadDarkLevel = false;
        global.lake = false;
        global.cemetary = false;
        global.shop = false;
        global.darkLevel = false;
        global.customLevel = false;
    }
    global.currLevel = global.gen_level[k];
    global.noDarkLevel = global.gen_nodark[k];
    var ov = global.gen_ov[k];
    for (var q = 0; q < array_length(ov); q++) variable_global_set(ov[q][0], ov[q][1]);
    global.gameStart = true;
    random_set_seed(global.gen_seed[k]);
}
'''
    initlevel = '''
scrInitLevel();
if (global.gen_on)
''' + gen_dump_gml()
    return create, step, begin, setup, initlevel


def build_gen(droid, cases_path, out):
    droid, cases_path, out = map(os.path.abspath, (droid, cases_path, out))
    cases = parse_cases(cases_path)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    root = os.path.commonpath([droid, UTMT, out])
    rel = lambda p: '/w/' + os.path.relpath(p, root)
    q = lambda s: '@"' + s.replace('"', '""') + '"'
    create, step, begin, setup, initlevel = gml_gen(cases)
    names = out + '.names'
    script = out + '.csx'
    open(script, 'w').write(f'''
using System.IO;
using System.Text;
using UndertaleModLib.Compiler;
var sb = new StringBuilder();
for (int i = 0; i < Data.GameObjects.Count; i++) sb.Append($"O {{i}} {{Data.GameObjects[i].Name.Content}}\\n");
for (int i = 0; i < Data.Sprites.Count; i++) sb.Append($"S {{i}} {{Data.Sprites[i].Name.Content}}\\n");
for (int i = 0; i < Data.Rooms.Count; i++) sb.Append($"R {{i}} {{Data.Rooms[i].Name.Content}}\\n");
File.WriteAllText({q(rel(names))}, sb.ToString());
CodeImportGroup g = new(Data);
g.QueueAppend("gml_Object_oGamepad_Create_0", {q(create)});
g.QueueReplace("gml_Object_oGamepad_Step_0", {q(step)});
g.QueueReplace("gml_Object_oGamepad_Step_1", {q(begin)});
g.QueueFindReplace("gml_Object_oGame_Create_0", "scrInitLevel();", {q("{" + initlevel + "}")});
g.QueuePrepend("gml_Object_oPlayer1_Create_0", {q(setup)});
g.QueuePrepend("gml_Object_oEntrance_Create_0", {q(setup)});
g.QueueReplace("gml_Object_oBlackBG_Create_0", {q(setup)});
g.Import();
''')
    if os.path.exists(out):
        os.remove(out)
    r = subprocess.run(['docker', 'run', '--rm', '-v', f'{root}:/w', DOTNET, 'dotnet', rel(UTMT), 'load', rel(droid),
                        '-s', rel(script), '-o', rel(out), '-f'], capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(out) or re.search(r'(?i)error|exception', r.stdout + r.stderr):
        sys.exit(r.stdout[-4000:] + r.stderr[-4000:])
    print(f'{out}: generator mode, {len(cases)} cases')


def gen_records(data):
    """yield one dict per gen_<k>.bin record (see build_gen)"""
    o = 0
    while o < len(data):
        magic, k, seed, level = struct.unpack_from('<Iidd', data, o)
        if magic != GEN_MAGIC:
            raise ValueError(f'bad gen magic at {o}')
        o += 24
        r = dict(case=k, seed=int(seed), level=int(level))
        ng = len(GEN_GLOBALS) + len(GEN_OGAME)
        r['globals'] = dict(zip(GEN_GLOBALS + ['oGame.' + g for g in GEN_OGAME], struct.unpack_from(f'<{ng}d', data, o)))
        o += 8 * ng
        r['roomPath'] = list(struct.unpack_from('<20d', data, o))
        o += 160
        (n,) = struct.unpack_from('<I', data, o)
        o += 4
        insts = []
        H = struct.Struct('<ihddhddBBH')
        for _ in range(n):
            iid, obj, x, y, spr, depth, ispd, vis, pers, am = H.unpack_from(data, o)
            o += H.size
            al = {}
            for b in range(12):
                if am & (1 << b):
                    al[b] = struct.unpack_from('<d', data, o)[0]
                    o += 8
            (vm,) = struct.unpack_from('<I', data, o)
            o += 4
            vs = {}
            for b, v in enumerate(GEN_VARS):
                if vm & (1 << b):
                    vs[v] = struct.unpack_from('<d', data, o)[0]
                    o += 8
            sm = data[o]
            o += 1
            for b, v in enumerate(GEN_SVARS):
                if sm & (1 << b):
                    e = data.index(b'\0', o)
                    vs[v] = data[o:e].decode('utf-8', 'replace')
                    o = e + 1
            insts.append(dict(id=iid, obj=obj, x=x, y=y, spr=spr, depth=depth, image_speed=ispd, visible=vis,
                              persistent=pers, alarms=al, vars=vs))
        r['insts'] = insts
        (tn,) = struct.unpack_from('<I', data, o)
        o += 4
        r['tiles'] = []
        for _ in range(tn):
            e = data.index(b'\0', o)
            bg = data[o:e].decode('utf-8', 'replace')
            o = e + 1
            r['tiles'].append((bg,) + tuple(int(v) for v in struct.unpack_from('<7d', data, o)))
            o += 56
        r['draws'] = [int(v) for v in struct.unpack_from('<4d', data, o)]
        o += 32
        r['order'] = {}
        if struct.unpack_from('<I', data, o)[0] == 0xFFFFFFFF:
            o += 4
            r['order_all'] = {}
            while True:
                (oi,) = struct.unpack_from('<i', data, o)
                o += 4
                if oi < 0:
                    break
                (c,) = struct.unpack_from('<I', data, o)
                o += 4
                r['order_all'][oi] = list(struct.unpack_from(f'<{c}i', data, o))
                o += 4 * c
        for name in GEN_ORDER:
            (c,) = struct.unpack_from('<I', data, o)
            o += 4
            r['order'][name] = list(struct.unpack_from(f'<{c}i', data, o))
            o += 4 * c
        yield r


def main():
    a = sys.argv[1:]
    if a and a[0] == 'build' and len(a) >= 4:
        seed = int(a[a.index('--seed') + 1]) if '--seed' in a else 1
        build(a[1], a[2], a[3], seed)
    elif a and a[0] == 'build-gen' and len(a) >= 4:
        build_gen(a[1], a[2], a[3])
    elif a and a[0] == 'rng-probe' and len(a) >= 3:
        rng_probe(a[1], a[2])
    elif a and a[0] == 'decode' and len(a) >= 3:
        decode(a[1], a[2], a[a.index('--steps') + 1] if '--steps' in a else None, '--no-inst' not in a)
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
