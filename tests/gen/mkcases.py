#!/usr/bin/env python3
"""Write the generator case files (tools/tracer.py generator mode format: "<seed> <level> <cont> <noDarkLevel>
[global=value ...]"), one per area, from one fixed seed list.

    python3 tests/gen/mkcases.py        -> tests/gen/{mines,lush,ice,temple,olmec,carry,chains}.txt

Every area: 200 seeds per level from new-game globals (noDarkLevel 1, as scrClearGlobals leaves it), the same
levels again with noDarkLevel 0 where dark levels exist, and the rarely drawn variants forced through their
globals. A case file never starts with a black-market case: global.roomPath's row 4 is created by an earlier
level's scrLevelGen (the runner stops with "index out of range" in isInShop otherwise, as the game would).
"""
import os
import random

HERE = os.path.dirname(os.path.abspath(__file__))
rnd = random.Random(20261003)
SEEDS = [rnd.randrange(0, 2 ** 31) for _ in range(200)]
PICKUPS = ['Rock', 'Jar', 'Skull', 'Fish Bone', 'Arrow', 'Machete', 'Mattock', 'Mattock Head', 'Pistol',
           'Web Cannon', 'Teleporter', 'Shotgun', 'Bow', 'Flare', 'Sceptre', 'Key', 'Gold Idol']


def write(name, lines, head):
    with open(os.path.join(HERE, name + '.txt'), 'w') as f:
        f.write(''.join('# ' + h + '\n' for h in head))
        f.write('\n'.join(lines) + '\n')
    print(name, len(lines))


def area(levels, dark):
    out = [f'{s} {L} 0 1' for s in SEEDS for L in levels]
    if dark:
        out += [f'{s} {L} 0 0' for s in SEEDS for L in levels if L not in (1, 16)]
    return out


def main():
    write('mines', area([1, 2, 3, 4], True) +
          [f'{(s + L * 1000003) % 2 ** 31} {L} {int(L > 1)} 0' for s in SEEDS[:50] for L in (1, 2, 3, 4)],
          ['P2 mines (levels 1-4): 200 seeds x levels 1-4, new game (noDarkLevel 1); levels 2-4 with noDarkLevel 0;',
           '50 chains 1->2->3->4 (cont 1)'])
    lush = area([5, 6, 7, 8], True)
    lush += [f'{s} {5 + k % 4} 0 1 lake=1' for k, s in enumerate(SEEDS)]
    lush += [f'{s} {5 + k % 4} 0 {k % 2} probCemetary=1' for k, s in enumerate(SEEDS)]
    lush += [f'{s} {5 + k % 4} 0 1 genBlackMarket=1' for k, s in enumerate(SEEDS)]
    write('lush', lush, ['P2 lush (levels 5-8): 200 seeds x levels 5-8, new game (noDarkLevel 1, then 0); lake,',
                         'cemetery (probCemetary=1), black market (genBlackMarket=1): 200 each, levels 5-8 in turn'])
    ice = area([9, 10, 11, 12], False)
    ice += [f'{s} {9 + k % 4} 0 1 probAlien=1 madeMoai=1' for k, s in enumerate(SEEDS)]
    ice += [f'{s} {9 + k % 4} 0 1 probAlien=1000000 probYetiLair=1 madeMoai=1' for k, s in enumerate(SEEDS)]
    ice += [f'{s} {9 + k % 4} 0 1 madeMoai=1' for k, s in enumerate(SEEDS)]
    write('ice', ice, ['P2 ice (levels 9-12): 200 seeds x levels 9-12, new game; alien craft (probAlien=1, moai made), yeti lair',
                       '(probYetiLair=1, no alien, moai made), no moai (madeMoai=1): 200 each, levels 9-12 in turn'])
    temple = area([13, 14, 15], True)
    temple += [f'{s} {13 + k % 3} 0 1 probSacPit=1' for k, s in enumerate(SEEDS)]
    temple += [f'{s} 15 0 {k % 2} cityOfGold=1' for k, s in enumerate(SEEDS)]
    write('temple', temple, ['P2 temple (levels 13-15): 200 seeds x levels 13-15, new game (noDarkLevel 1, then 0);',
                             'sacrifice pit (probSacPit=1, levels 13-15 in turn), city of gold (cityOfGold=1, level 15)'])
    olmec = [f'{s} 16 0 1' for s in SEEDS] + [f'{s} 16 0 1 cityOfGold=1' for s in SEEDS]
    write('olmec', olmec, ['P2 Olmec level 16: 200 seeds, new game; 200 with cityOfGold=1'])
    carry = []
    for k, s in enumerate(SEEDS):
        L = 1 + k % 16
        p = PICKUPS[k % len(PICKUPS)]
        carry.append(f'{s} {L} 0 1 pickupItem=s:{p.replace(" ", "~")}')
        carry.append(f'{s} {L} 0 1 kaliPunish={2 + k % 2}')
        carry.append(f'{s} {L} 0 1 murderer=1')
        carry.append(f'{s} {L} 0 1 thiefLevel={1 + k % 3}')
    write('carry', carry, ['P2 carried state: a held item (each scrHoldItem type and one it does not create), the Kali',
                           'ball and chain (kaliPunish 2, 3), murderer, thief (thiefLevel 1-3); levels 1-16 in turn',
                           '(pickupItem: "~" stands for a space)'])
    chains = []
    for c, s in enumerate(SEEDS[:5]):
        for L in range(1, 17):
            seed = (s + L * 7919) % 2 ** 31
            ov = ''
            if L == 6 and c % 2 == 0:
                ov = ' lake=1'
            elif L in (7, 9, 13, 16) or (L == 6 and c % 2):
                ov = ' lake=0'
            chains.append(f'{seed} {L} {int(L > 1)} 0{ov}')
    write('chains', chains, ['P2 chains 1->16 (cont 1: globals carried; noDarkLevel 0) on 5 seeds, re-seeded per level;',
                             'the lake on level 6 of chains 0, 2, 4 (oTransition draws it in the game)'])


if __name__ == '__main__':
    main()
