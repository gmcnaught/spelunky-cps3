#!/usr/bin/env python3
"""Print a generated level as a 16-px cell map (for writing routes), from build/host/genhost.

    tools/levelmap.py <seed> [level] [--find] [--enemies]

Cells: # terrain  P push block  H ladder  = ladder top  ^ spikes  < > arrow traps  E entrance  X exit
       $ treasure  i item  c chest/crate  w web  m enemy (removed with TRACE_NOENEMY)  d damsel  @ the player
Columns are numbered every 5 cells (x = 16 * column), rows (y = 16 * row).
--enemies (P5): enemies by kind instead of m: s snake  b bat  x spider  G giant spider  C caveman  K shopkeeper
       B fake bones  I gold idol  T giant tiki head (the boulder trap)  r scarab
--find: list seeds 1..400 with the exit in the entrance's room column and at most 2 rooms below.
"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
GENHOST = os.path.join(HERE, '..', 'build', 'host', 'genhost')


def gen(seed, level=1):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(f'{seed} {level} 0 1\n')
        name = f.name
    out = subprocess.run([GENHOST, name], capture_output=True, text=True).stdout
    os.unlink(name)
    return [l.split() for l in out.splitlines() if l.startswith('I ')]


SYM = [('oPushBlock', 'P'), ('oSolid', '#'), ('oLadderTop', '='), ('oLadder', 'H'), ('oSpikes', '^'),
       ('oArrowTrapLeft', '<'), ('oArrowTrapRight', '>'), ('oEntrance', 'E'), ('oExit', 'X'), ('oWeb', 'w'),
       ('oChest', 'c'), ('oCrate', 'c'), ('oDamsel', 'd'), ('oPlayer1', '@')]
TREAS = ('oGoldBar', 'oGoldBars', 'oEmeraldBig', 'oSapphireBig', 'oRubyBig', 'oGoldChunk', 'oGoldNugget')
ENEM = ('oSnake', 'oBat', 'oSpiderHang', 'oGiantSpiderHang', 'oCaveman', 'oScarab', 'oShopkeeper', 'oFakeBones')
SOLIDS = ('oBrick', 'oBlock', 'oBrickSmooth', 'oSolid', 'oAltarLeft', 'oAltarRight', 'oSign', 'oSacAltarLeft',
          'oSacAltarRight', 'oArrowTrapLeftLit', 'oArrowTrapRightLit')


EKIND = {'oSnake': 's', 'oBat': 'b', 'oSpiderHang': 'x', 'oSpider': 'x', 'oGiantSpiderHang': 'G', 'oCaveman': 'C',
         'oShopkeeper': 'K', 'oFakeBones': 'B', 'oGoldIdol': 'I', 'oGiantTikiHead': 'T', 'oScarab': 'r'}


def cellmap(insts, kinds=False):
    g = [['.'] * 42 for _ in range(34)]
    for f in insts:
        o, x, y = f[2], int(f[3]), int(f[4])
        c = None
        if kinds and o in EKIND:
            c = EKIND[o]
        elif o in SOLIDS:
            c = '#'
        elif o in TREAS:
            c = '$'
        elif o in ENEM:
            c = 'm'
        elif o in ('oRock', 'oJar', 'oSkull', 'oGoldIdol', 'oBombBag', 'oRopePile', 'oMattock', 'oKey'):
            c = 'i'
        else:
            for n, s in SYM:
                if o == n or (n == 'oLadder' and o == 'oLadderOrange'):
                    c = s
        if c is None:
            continue
        cx, cy = x // 16, y // 16
        if 0 <= cx < 42 and 0 <= cy < 34:
            if g[cy][cx] in '.$im' or c in '@EX' or (kinds and o in EKIND):
                g[cy][cx] = c
    return g


def main():
    a = sys.argv[1:]
    if '--find' in a:
        for s in range(1, 401):
            ins = gen(s)
            e = [f for f in ins if f[2] == 'oEntrance'][0]
            x = [f for f in ins if f[2] == 'oExit'][0]
            ex, ey, xx, xy = int(e[3]), int(e[4]), int(x[3]), int(x[4])
            if (ex - 16) // 160 == (xx - 16) // 160 and (xy - ey) <= 256:
                print(s, 'entrance', ex, ey, 'exit', xx, xy)
        return
    a = [x for x in a if x != '--enemies'] + (['--enemies'] if '--enemies' in a else [])
    seed = int(a[0])
    level = int(a[1]) if len(a) > 1 and a[1].isdigit() else 1
    g = cellmap(gen(seed, level), '--enemies' in a)
    print('    ' + ''.join(f'{c:<5d}' for c in range(0, 42, 5)))
    for r, row in enumerate(g):
        print(f'{r:3d} ' + ''.join(row))


if __name__ == '__main__':
    main()
