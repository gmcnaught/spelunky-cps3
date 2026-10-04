#!/usr/bin/env python3
"""Checks tests/shell's step log from MAME (scripts/shell_check.sh; inputs: scripts/lua/shellin.lua).
    tools/shellcheck.py <mame run 1 log> [<run 2 log>]        exit 0 when every check passes

Log entry: program frame, kind (0 attract step, 1 game begin, 2 play step), mode, panel, credits, down, pressed,
released (src/shell/input.h KEY_* bits). Screen frame f of shellin.lua is seen near program frame f (a few frames of
boot apart), so the checks use windows around the scripted frames."""
import sys

RIGHT, JUMP, RUN, ROPE, PAY, START = 1, 16, 128, 512, 2048, 4096


def main():
    log = [tuple(map(int, ln.split()[1:])) for ln in open(sys.argv[1]) if ln.startswith('LOG ')]
    ok = [0, 0]

    def check(c, what):
        ok[c] += 1
        print(('ok   ' if c else 'FAIL ') + what)

    steps = [e for e in log if e[1] != 1]
    begins = [e for e in log if e[1] == 1]
    check(len(steps) > 200, f'{len(steps)} steps logged')
    check(all(b[0] - a[0] == 2 for a, b in zip(steps, steps[1:])), 'one game step every second frame')
    check(len(begins) == 2, f'{len(begins)} games begun (two credits / starts scripted)')
    if len(begins) != 2:
        return 1
    b1, b2 = begins
    check(b1[0] > 150, 'Start without a credit (frame 100) began nothing')
    check(b1[3] == 0 and b1[4] == 1, f'game 1 on panel 1 with 1 credit left (panel {b1[3] + 1}, {b1[4]})')
    check(b2[3] == 1 and b2[4] == 0, f'game 2 on panel 2 with 0 credits left (panel {b2[3] + 1}, {b2[4]})')
    att = [e for e in log if e[1] == 0]
    cr_at = lambda lo, hi: sorted({e[4] for e in att if lo <= e[0] < hi})
    check(cr_at(0, 128) == [0], f'no credit before the first coin {cr_at(0, 128)}')
    check(cr_at(136, 148) == [1], f'coin 1: one credit {cr_at(136, 148)}')
    check(cr_at(156, 166) == [2], f'coin 2: two credits {cr_at(156, 166)}')
    check(cr_at(384, 398) == [1], f'service: one credit {cr_at(384, 398)}')
    check(cr_at(410, 9999) == [3], f'two more coins: three credits {cr_at(410, 9999)}')
    g1 = [e for e in log if e[1] == 2 and b1[0] < e[0] < b2[0]]
    g2 = [e for e in log if e[1] == 2 and e[0] > b2[0]]
    for name, g in (('game 1', g1), ('game 2', g2)):
        check(all(not (e[5] | e[6] | e[7]) & START for e in g), f'{name}: Start is pay, never start')
        check(not g[0][6], f'{name}: the Start that began it is not a press at the first step')
        check(g[-1][6] & ROPE and all(not e[6] & ROPE for e in g[:-1]), f'{name}: ends at its rope press')
    pj = [e for e in g1 if e[6] & JUMP]
    check(len(pj) == 1 and len([e for e in g1 if e[5] & JUMP]) == 1, 'a one-frame jump tap: pressed and held for one step')
    rj = [e for e in g1 if e[7] & JUMP]
    check(len(rj) == 1 and rj[0][0] == pj[0][0] + 2, 'the tap released at the next step')
    rd = [e for e in g1 if e[5] & RIGHT and e[0] < 250]
    check(len(rd) == 10 and len([e for e in g1 if e[6] & RIGHT]) == 1 and len([e for e in g1 if e[7] & RIGHT]) == 1,
          f'right held 20 frames: pressed once, held {len(rd)} steps, released once')
    check(not any(e[5] & RIGHT for e in g1 if e[0] > 250), 'panel 2 right ignored in a panel-1 game')
    pp = [e for e in g1 if e[6] & PAY]
    check(len(pp) == 1 and pp[0][0] > 235, f'one pay press from Start at frame 240 ({len(pp)})')
    rn = [e for e in g2 if e[5] & RUN]
    check(len(rn) == 3 and len([e for e in g2 if e[6] & RUN]) == 1, f'panel 2 run (B4) held 6 frames: {len(rn)} steps')
    check(not any(e[5] & RIGHT for e in g2), 'panel 1 right ignored in a panel-2 game')
    # EEPROM (src/shell/hiscore.c): run 1 from a blank EEPROM (HD without spelunky.ini: tunnel 10001 / 20001,
    # firstTime); each game stores a death with money 1234 (main.c); run 2 boots from what run 1 stored
    boot = lambda p: [tuple(map(int, ln.split()[1:])) for ln in open(p) if ln.startswith('BOOT ')]
    b = boot(sys.argv[1])
    check(b == [(10001, 20001, 1, 0)], f'run 1 boot: tunnel 10001 / 20001, first time, money 0 {b}')
    if len(sys.argv) > 2:
        b = boot(sys.argv[2])
        check(b == [(10001, 20001, 0, 1234)], f'run 2 boot: the stored block (tunnel 10001 / 20001, money 1234) {b}')
    print(f'{ok[1]} of {sum(ok)} checks passed')
    return 1 if ok[0] else 0


if __name__ == '__main__':
    sys.exit(main())
