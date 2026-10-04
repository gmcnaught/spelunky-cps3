#!/usr/bin/env python3
"""HD runner reference frames for the HUD (tests/hud/cases.json): a tools/tracer.py route build (tools/tracer.py is
used as it is, not changed) with one more event, oGamepad Draw GUI End (Draw_75), which at record R0 + k (k = case):
sets HD's globals to case k (global.plife, bombs, rope, money, collect, has*, udjatBlink, bloodLevel, arrows,
exitX / Y relative to the view as the case's, messageTimer, message1 / 2 and their highlights, drawHUD,
oPlayer1.pickupItemType; the drawing instance's image_index = anim), draws application_surface and then HD's own
scrDrawHUD() and showMessages() into a surface of the application surface's size, saves it as gui_<r>.png, and puts
the globals back. TRACE_SHOT (tracer.py) saves the same frame's application_surface as shot_<r>.png.
tools/hudcheck.py hd then checks: the model's HUD over shot_<r>.png == gui_<r>.png.

    tools/hudref.py <game.unx> <route.txt> <out.unx> <cases.json> <R0> [--seed N]
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tracer  # noqa: E402

TYPE = {'ROCK': 'Rock', 'JAR': 'Jar', 'SKULL': 'Skull', 'FISHBONE': 'Fish Bone', 'ARROW': 'Arrow',
        'MACHETE': 'Machete', 'MATTOCK': 'Mattock', 'MATTOCKHEAD': 'Mattock Head', 'PISTOL': 'Pistol',
        'WEBCANNON': 'Web Cannon', 'TELEPORTER': 'Teleporter', 'SHOTGUN': 'Shotgun', 'BOW': 'Bow',
        'SCEPTRE': 'Sceptre', 'FLARE': 'Flare', 'KEY': 'Key', 'NONE': '', 'OTHER': 'Bomb'}
HAS = {'UDJAT': 'hasUdjatEye', 'ANKH': 'hasAnkh', 'CROWN': 'hasCrown', 'KAPALA': 'hasKapala',
       'SPECTACLES': 'hasSpectacles', 'GLOVES': 'hasGloves', 'MITT': 'hasMitt', 'SPRINGSHOES': 'hasSpringShoes',
       'SPIKESHOES': 'hasSpikeShoes', 'CAPE': 'hasCape', 'JETPACK': 'hasJetpack', 'COMPASS': 'hasCompass',
       'PARACHUTE': 'hasParachute'}
GLOBALS = ['plife', 'bombs', 'rope', 'money', 'collect', 'hasStickyBombs', 'udjatBlink', 'bloodLevel', 'arrows',
           'exitX', 'exitY', 'messageTimer', 'message1', 'message2', 'messageHighlights', 'message2Highlights',
           'drawHUD'] + list(HAS.values())


def gml_str(s):
    return '"' + s.replace('"', '\\"') + '"'


def case_gml(c):
    L = [f'global.plife = {c.get("life", 0)};', f'global.bombs = {c.get("bombs", 0)};',
         f'global.rope = {c.get("ropes", 0)};', f'global.money = {c.get("money", 0)};',
         f'global.collect = {c.get("collect", 0)};', f'global.hasStickyBombs = {c.get("sticky_bombs", 0)};',
         f'global.udjatBlink = {c.get("udjat_blink", 0)};', f'global.bloodLevel = {c.get("blood_level", 0)};',
         f'global.arrows = {c.get("arrows", 0)};', f'global.drawHUD = {c.get("visible", 1)};',
         f'global.messageTimer = {c.get("message_timer", 0)};',
         f'global.exitX = view_xview[0] + {c.get("exit_x", 0) - c.get("view_x", 0)};',
         f'global.exitY = view_yview[0] + {c.get("exit_y", 0) - c.get("view_y", 0)};',
         f'oPlayer1.pickupItemType = {gml_str(TYPE[c.get("held", "NONE")])};',
         f'image_index = {c.get("anim", 0)};']
    items = set(c.get('items', []))
    L += [f'global.{v} = {int(k in items)};' for k, v in HAS.items()]
    for k in (1, 2):
        m = c.get(f'message{k}', '')
        hl = 'messageHighlights' if k == 1 else 'message2Highlights'
        if isinstance(m, str):
            L.append(f'global.message{k} = {gml_str(m)};')
            L.append(f'global.{hl} = -1;')
        else:
            L.append(f'global.message{k} = [{", ".join(gml_str(p) for p in m)}];')
            L.append(f'global.{hl} = [{", ".join(str(i) for i in c.get(f"message{k}_yellow", []))}];')
    return '\n        '.join(L)


def gui_end(cases, r0):
    save = '\n    '.join(f'global.hr_{g} = global.{g};' for g in GLOBALS)
    restore = '\n    '.join(f'global.{g} = global.hr_{g};' for g in GLOBALS)
    sw = '\n'.join(f'    case {r0 + k}:\n        {case_gml(c)}\n        break;' for k, c in enumerate(cases))
    return f'''
if (!global.trc_on) exit;
if (!instance_exists(oPlayer1)) exit;
var r = global.trc_rec - 1;
if (r >= {r0} && r < {r0 + len(cases)})
{{
    {save}
    var hr_type = oPlayer1.pickupItemType;
    var hr_img = image_index;
    switch (r)
    {{
{sw}
    }}
    var w = surface_get_width(application_surface), h = surface_get_height(application_surface);
    var s = surface_create(w, h);
    surface_set_target(s);
    draw_clear_alpha(c_black, 1);
    gpu_set_blendenable(false);
    draw_surface(application_surface, 0, 0);
    gpu_set_blendenable(true);
    scrDrawHUD();
    showMessages();
    surface_reset_target();
    surface_save(s, "gui_" + string(r) + ".png");
    surface_free(s);
    var f = file_text_open_write("gui_" + string(r) + ".txt");
    file_text_write_string(f, string(view_xview[0]) + " " + string(view_yview[0]) + " " + string(global.display_w)
        + " " + string(global.display_h) + " " + string(w) + " " + string(h) + " " + string(view_current));
    file_text_close(f);
    {restore}
    oPlayer1.pickupItemType = hr_type;
    image_index = hr_img;
}}
'''


def main():
    a = sys.argv[1:]
    seed = int(a[a.index('--seed') + 1]) if '--seed' in a else 1
    droid, route, out, cases_path, r0 = a[0], a[1], a[2], a[3], int(a[4])
    cases = json.load(open(cases_path))
    os.environ['TRACE_SHOT'] = ','.join(str(r0 + k) for k in range(len(cases)))
    code = gui_end(cases, r0)
    orig = tracer.csx

    def csx(*args):
        s = orig(*args)
        q = '@"' + code.replace('"', '""') + '"'
        return s.replace('g.Import();', f'g.QueueReplace("gml_Object_oGamepad_Draw_75", {q});\ng.Import();')

    tracer.csx = csx
    tracer.build(droid, route, out, seed)


if __name__ == '__main__':
    main()
