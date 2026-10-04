# Remaining content (P7): survey, work packages, reference routes

State at 200e74d. The level generator covers every area: lush, ice, temple and Olmec levels generate equal to the
runner, with 8,091 instances checked. Play behaviour is translated for the mines (P4/P5): the player, bombs and ropes,
the mines enemies (snake, bat, spider, giant spider, caveman, skeleton), damsels, shops (shopkeeper, buying,
stealing), traps (arrow trap, spikes, boulder), items and treasure, and the transition rooms. Everything else
reaches `PUNTR(code)` (play_untranslated) the first time it runs. This file lists what remains, groups it into
packages that agents can take in parallel once slot reuse lands, and names the reference traces for each.

Data behind the numbers:
- **Generator:** 200 seeds × levels 1-16 through `build/host/genhost` (`noDarkLevel` 0). A percentage is the share of
  that area's levels holding at least one instance; M/L/I/T/O = mines/lush/ice/temple/Olmec. "spawned" means
  created at run time.
- **Untranslated survey:** each level played 1,500 steps with random walk / jump / whip / bomb inputs, every
  PUNTR code recorded and cleared (30 seeds per level).
- **GML lines:** non-blank, non-comment lines of the object's event files.
- **rand:** `rand` / `random` / `random_range` / `irandom` / `choose` calls in its events. Each one is an RNG draw
  whose order must match: GML evaluates function arguments right to left.

Most-reached codes in the untranslated survey (levels of 480 that reach them): oManTrap 100, oFireFrog 73, oYeti 73,
oHawkman 68, oCrown (Create) 63, oBomb sticky / lava 56, oSpringTrap 47, oFrog 33, oFinalBoss 30, oCeilingTrap 21,
oUFO 17, oTombLord 10, oDeadFish 7, oDice 6.

## 1. Before the packages: one hook commit (single owner, after slot reuse)

The packages must not edit the same files. The existing pattern is the P5 hooks (`pen_*`, `pdam_*`, `pshop_*`) and
P8's `front_ev` (`src/front/front.h`). Following it, one commit adds:

1. **`src/game/pcontent.h`:** per package X in {jungle, swamp, ice, temple, items}:
   `int pX_ev(int ev, int i, int arg);` (ev: FEV_CREATE / DESTROY / STEP / END_STEP / ALARM / ANIMEND / DRAW /
   OUTSIDE / COLLISION, as front.h). It returns 1 when package X ran the event.
2. **One weak default per function in `pobj.c`:** `return 0;`, so the build and every gate stay as they are. pobj.c's
   ev_* dispatchers call `pX_ev` (five lines) where they now fall through to the generic PUNTRs: 1000 Create, 1061
   Step, 1063 / 1073 Alarm, 1080 Other_7, 1096 Collision, 1097 Outside, 1021 Destroy.
3. **Player and item hooks:** every PUNTR in pplayer.c / pobj.c / pscript.c / pitem.c / penemy.c that the table below
   gives to a package becomes a call `pX_<what>(...)`. Its weak default does exactly the PUNTR it replaces, so the
   commit changes no behaviour (all routes equal, constcheck, sh2).
4. **New C files:** one per package (`src/game/pjungle.c`, `pswamp.c`, `pice.c`, `ptemple.c`, `pitems.c`), added to
   test/host/Makefile PSRC and the tests/game / tests/playsh2 snapshots in the same commit. After that each package
   edits only its own file, plus its rows in drawtables.py's KIND table and routes/traces.

Hooks per package (current PUNTR code: where):

| package | player (pplayer.c) | items, world (pobj.c, pscript.c, pitem.c) | enemies (penemy.c) |
|---|---|---|---|
| A jungle | 2010 / 2016 monkey on the player; 2020 spears hit the player | 1011 tiki torch on a destroyed block; 1017 snake from a jar (mines, kept in A with the snakes' code nearby) | 5003 / 5004 generic enemy dispatch for oFrog, oFireFrog, oManTrap, oMonkey, oScarab; 5005 collisions |
| B swamp | 2009 / 2032 / 2036 swimming (oWater / oWaterSwim); 2002 / 2003 / 2007 / 2011 / 2030 the cape's physics (vampire drop) | 1041 item in water; 1053 oGame checkWater; 1012 grave on a destroyed block; 1052 the ghost (oGame :45, 2:30); 8001 item in water (pitem.c) | 5003 / 5004 oPiranha, oDeadFish, oZombie, oVampire |
| C ice | 2018 laser; 2019 psychic wave | 1000 oCrown Create (crown pickup); 1080 oSpringTrap Other_7; the Moai exit | 5004 oYeti, oYetiKing, oUFO, oAlien, oAlienBoss |
| D temple | 2021 smash trap; 2022 ceiling trap; 2033 lava; 2046 the idol trap outside the mines; 2061 the exit animation's room past Olmec | 1032 / 1036 / 1039 / 1040 / 1056 items, jars, rope in lava; 1061 oFinalBoss; 1094 sceptre / gold door | 5004 oHawkman, oTombLord, oMagmaMan; 5012 enemies in lava |
| E items | 2005 / 2006 / 2060 jetpack, cape glide; 2034 / 2035 / 2052 parachute; 2017 / 2072 mitt; 2037-2040 / 2044 / 2050 / 2055 / 2070 weapons (scrUseItem); 2042 flare crate; 2054 ankh; 2062 kapala; 2012 ball and chain; 2008 downToRun | 3001 scrFireBow; 3002 equipment pickups (scrStealItem); 1001 / 1015 bones; 1016 spider from a jar; 1018 / 1019 / 1031 / 1033 / 1062 sticky bombs, damsel / flare Destroy; 1042 / 1043 bomb and idol in a shop; 1051 udjat blink (oXMarket); 1002 / 1064 / 1065 dice; 1060 flare / locked chest / mattock / web cannon Step; 1070 / 1071 bomb arrows; 1095 locked chest; 4001 / 4003 transition-room cape and Kali punishment; 8002 Kali head | 5014 / 6012 sacrifice on the Kali altar |

Left for later (no package): tunnel man / damsel as the player (2001, 2038, 2045, 2053), the pause menu (1054), the
dark-level light computation (`distToNearestLightSource`, PLEV.darkness: a drawing task with E's flares and lamps),
the end rooms and credits.

## 2. Packages

Size is the translated GML in lines (objects, plus player-side scripts for E). All packages draw through src/draw's
existing kinds: oEnemy's mirrored draw_self, oItem's, DK_PLAIN for oTombLord / oYetiKing. Drawing work is noted
where a package needs more.

### A. Jungle (lush, part 1): about 1,400 lines, 158 rand calls

Man traps, frogs and fire frogs, monkeys, scarabs, trees and leaves, spear traps, trap blocks, oLush destruction.
- **Dependencies:** oEnemy's step (pen_parent_step), moveTo, the collision scripts, scrCreateBlood; oLeaf / oRubble
  pieces (rubble exists); oFireFrogBomb explodes through oExplosion (translated).
- **RNG:** heavy. oLush Destroy 38 calls (the gem drops), monkeys 27, man traps 19.
- **Drawing:** none new. Leaves and branches are platforms (oPlatform): check src/draw's terrain cells for them.
- **Sound sites:** docs/AUDIO.md §5 (oManTrap, oMonkey, oFrog, oFireFrog, oScarab, oSpringTrap rows).

### B. Swamp (lush, part 2, and every level's water and ghost): about 1,030 lines, 55 rand calls

Water (swimming for the player, enemies and items; oWaterSwim, oBubble), piranhas and fish bones, dead fish, the
cemetery (zombies, graves, vampire with its cape drop), the ghost (any level after 2:30: oGame :45, oGhost), oCape.
- **Dependencies:**
  - the player's swim code (pplayer hooks 2009 / 2032 / 2036);
  - oItem / oEnemy water branches;
  - the cape (`hasCape`) physics in the player (shared with E's pickup: B owns the physics, E the pickup).
- **RNG:** piranha 10, vampire 14, zombie 8, grave 13.
- **Drawing:**
  - oFishBone sets image_angle: unsupported, drawn unrotated and counted. Precomputed rotations if needed.
  - Water surfaces are tiles (generator).
- **Note:** lake levels exist in every area; B's water code is needed wherever oWater generates (L94%).

### C. Ice: about 1,370 lines, 102 rand calls

Yetis, the yeti king (yeti lair), UFOs, aliens and the alien boss (alien craft; lasers, psychic waves), spring traps,
thin ice, ice and dark blocks (oIce / oDark destruction, oDrip, frozen cavemen), oDarkFall, the Moai and its exit,
the crown.
- **Dependencies:** oEnemySight (translated, for yetis); oRubbleDark; oIceBlock.
- **Generator gap:** the generator places no oYetiKing or oAlienBoss in 3,200 ice cases. The yeti lair and alien
  craft are special levels (`global.yetiLair`, `global.alienCraft`): their rooms need checking in the generator
  before the boss routes (gen*.c, owned by whoever holds the generator then).
- **RNG:** yeti 6, UFO 1, yeti king and alien boss low. oDark Destroy 38 (gem drops, as oLush).
- **Drawing:** oYetiKing image_yscale (DK_PLAIN reads it); the UFO's tractor laser (oLaser, oLaserTrail) is sprites.

### D. Temple and Olmec: about 1,850 lines, 158 rand calls

Split it in two if four agents are available for five packages:
- **D1 temple, about 1,450 lines:** hawkmen, tomb lords (the sceptre, the gold door, the city of gold, flies),
  smash traps, ceiling traps, doors, lava (the player, items, ropes and enemies in lava; magma, oMagmaMan),
  oTemple destruction.
- **D2 Olmec, about 400 lines:** oOlmec, debris and slams, yellow balls and trails, oFinalBoss's intro, the worship
  cavemen, oLavaSolid, oXEnd and the room after it (2061).

Notes:
- **RNG:** Olmec 20, smash traps 13 each, oTemple Destroy 38, hawkman 5.
- **Music:** Olmec's level calls startMusic's rOlmec branch (playMusic mBoss once oPlayer1.active), already in prun.c.
- **Drawing:** oTombLord image_yscale (DK_PLAIN). Olmec is a large multi-piece sprite (one display-list entry
  group); check the 510-entry budget there.

### E. Items, weapons, equipment, mines leftovers: about 830 object lines plus about 600 lines of player-script branches (scrUseItem weapons, scrFireBow, scrStealItem equipment)

- **Weapons (scrUseItem):** machete (oSlash), mattock (oMattockHit, oMattockHead), pistol and shotgun (oBullet,
  translated for the shopkeeper), web cannon (oWebBall), teleporter, bow (scrFireBow, oArrow, bomb arrows).
- **Equipment (scrStealItem 3002):** jetpack, parachute, cape pickup, gloves (climbing), mitt, spring and spike
  shoes, spectacles, compass, udjat eye, ankh (revive), kapala (oBlood collection, bloodLevel in src/game/pmsg.h),
  paste (sticky bombs).
- **Flares and lamps:** oFlare, oFlareCrate, oFlareSpark, oLamp, oLampRed; with the dark-level light computation if
  taken together.
- **Mines leftovers:**
  - the dice house (oDice, 223 lines);
  - the Kali altar (sacrifice, favour, oKaliHead; punishment: oBall and oChain);
  - the crystal skull;
  - spiders and snakes from jars;
  - sticky bombs; the locked chest and key.
- **Owns pplayer.c's item branches:** the other packages call into the player only through their hooks.
- **RNG:** low in the objects (33). The dice and Kali favour draw in their scripts.
- **Messages:** scrStealItem's equipment messages (texts in its GML :25-227); the ankh revive (oPlayer1 :1863);
  the sacrifice favour (scrGetFavorMsg). See docs/AUDIO.md §5's message list.

### Object tables

#### Package A objects

| object | parent | events (GML lines) | levels with it (generator, 200 seeds) | rand | creates |
|---|---|---|---|---|---|
| oManTrap | oEnemy | Col_oCaveman 9, Col_oCharacter 60, Col_oDamsel 17, Col_oShopkeeper 23, Col_oWhip 20, Col_oWhipPre 20, Create_0 30, Other_7 11, Step_0 160 | L84% | 19 | oBlood oBone oLeaf oShopkeeper oShotgun |
| oFrog | oEnemy | Create_0 26, Step_0 78 | L91% | 3 |  |
| oFireFrog | oEnemy | Create_0 26, Step_0 78 | L68% | 3 | oFireFrogBomb |
| oFireFrogBomb | oItem | Alarm_1 11, Create_0 8, Step_0 26 | spawned | 0 | oExplosion oSplash |
| oMonkey | oEnemy | Col_oCharacter 21, Col_oItem 26, Create_0 36, Step_0 283 | L60% | 27 | oBomb oGoldNugget oRopeThrow oSplash |
| oScarab | oEnemy | Col_oCharacter 6, Create_0 21, Destroy_0 4, Step_0 57 | M8% L9% T12% | 8 | oFlareSpark |
| oTree | oSolid | Create_0 3, Destroy_0 7, Step_0 8 | L93% T26% | 12 | oRubble oRubbleSmall |
| oTreeBranch | oPlatform | Create_0 2, Destroy_0 4, Step_0 8 | L91% T26% | 4 | oLeaf |
| oLeaves | oPlatform | Create_0 7, Destroy_0 5, Step_0 34 | L93% T26% | 8 | oLeaf |
| oLeaf | oRubblePiece | Create_0 5 | L71% T24% | 0 |  |
| oLush | oSolid | Create_0 12, Destroy_0 31 | L100% T97% | 38 | oEmeraldBig oGoldChunk oGoldNugget oRubble oRubbleSmall oRubyBig oSapphireBig |
| oSpearTrapBottom | oSolid | Alarm_0 2, Create_0 5, Destroy_0 10, Step_0 76 | L96% T69% | 12 | oArrow oRubble oRubbleSmall oSpearsLeft |
| oSpearTrapTop | oSolid | Alarm_0 2, Create_0 6, Destroy_0 10, Step_0 74 | L86% T61% | 12 | oArrow oRubble oRubbleSmall oSpearsLeft |
| oSpearsLeft | oDrawnSprite | Other_7 1, Step_0 9 | spawned | 0 |  |
| oTrapBlock | oSolid | Create_0 3, Destroy_0 14, Step_0 6 | L35% T10% | 12 | oRubble oRubbleSmall |
| oTikiTorch | - | Create_0 1 | L7% | 0 |  |

#### Package B objects

| object | parent | events (GML lines) | levels with it (generator, 200 seeds) | rand | creates |
|---|---|---|---|---|---|
| oPiranha | oEnemy | Alarm_0 1, Create_0 25, Step_0 131 | L64% | 10 | oBubble oFishBone |
| oDeadFish | oEnemy | Create_0 22, Step_0 70 | L5% | 4 | oBone oFishBone |
| oFishBone | oItem | Alarm_2 1, Create_0 6, Step_0 20 | spawned | 0 |  |
| oBubble | oDrawnSprite | Create_0 4, Other_7 1, Step_0 5 | spawned | 1 |  |
| oWaterSwim | oWater | Destroy_0 4 | L94% | 2 | oDrip |
| oWater | oDrawnSprite | Create_0 3 | spawned | 0 |  |
| oZombie | oEnemy | Create_0 25, Step_0 104 | L7% | 8 | oBone oSkull |
| oVampire | oEnemy | Alarm_0 1, Col_oBlood 2, Col_oCharacter 38, Col_oWhip 13, Col_oWhipPre 13, Create_0 37, Step_0 339 | L4% | 14 | oBlood oBone oBurn oCapePickup oSkull oSmokePuff |
| oGrave | oSolid | Create_0 10, Destroy_0 6, Step_0 5 | L7% | 13 | oRubble oRubbleSmall |
| oGhost | oDrawnSprite | Col_oCharacter 32, Col_oWhip 0, Create_0 20, Other_7 12, Step_0 24 | spawned | 3 | oBone oSkull |
| oCapePickup | oItem | Create_0 7 | M6% L3% I2% T1% O2% | 0 |  |
| oCape | oDrawnSprite | Create_0 2, Step_0 49 | spawned | 0 |  |

#### Package C objects

| object | parent | events (GML lines) | levels with it (generator, 200 seeds) | rand | creates |
|---|---|---|---|---|---|
| oYeti | oEnemy | Alarm_0 1, Col_oCharacter 73, Col_oWhip 31, Col_oWhipPre 31, Create_0 30, Other_7 5, Step_0 195 | I100% | 6 | oBlood oEnemySight |
| oYetiKing | oEnemy | Col_oCharacter 30, Col_oWhip 16, Create_0 27, Draw_0 1, Other_7 16, Step_0 139 | spawned | 10 | oBlood oBone oIceBlock oRopePile oSapphireBig oSpikeShoes |
| oIceBlock | oMoveableSolid | Create_0 5, Destroy_0 7, Step_0 8 | spawned | 2 | oDrip |
| oUFO | oEnemy | Create_0 22, Other_7 5, Step_0 119 | I90% | 1 | oLaser oUFOCrash |
| oUFOCrash | - | Alarm_0 3, Col_oEnemy 3, Col_oSolid 3, Create_0 3, Step_0 3 | spawned | 5 | oBurn oExplosion oFlameTrail |
| oAlien | oEnemy | Create_0 22, Step_0 69 | spawned | 3 |  |
| oAlienEject | oDrawnSprite | Col_oPlayer1 30, Col_oWeb 2, Create_0 16, Other_0 1, Other_7 1, Step_0 38 | spawned | 6 | oAlien oBlood oBurn |
| oAlienBoss | oEnemy | Col_oCharacter 27, Create_0 21, Other_7 9, Step_0 75 | spawned | 10 | oBlood oEmeraldBig oPsychicCreate oPsychicWave oRubyBig oSapphireBig |
| oLaser | oDrawnSprite | Alarm_0 2, Col_oDamsel 10, Col_oEnemy 9, Col_oSolid 21, Create_0 4, Other_0 1, Step_0 11 | spawned | 4 | oLaserExplode oLaserTrail |
| oLaserExplode | oDrawnSprite | Create_0 3, Other_7 1 | spawned | 0 |  |
| oLaserTrail | oDrawnSprite | Create_0 2, Other_7 1 | spawned | 0 |  |
| oPsychicWave | oDrawnSprite | Col_oDamsel 9, Col_oEnemy 7, Create_0 4, Other_0 1, Other_7 1, Step_0 3 | spawned | 4 |  |
| oPsychicCreate | oDrawnSprite | Create_0 6, Other_7 1, Step_0 2 | spawned | 0 |  |
| oSpringTrap | - | Col_oEnemy 14, Col_oItem 18, Col_oPlayer1 15, Create_0 4, Other_7 5, Step_0 6 | I93% | 0 |  |
| oThinIce | oSolid | Create_0 2, Step_0 12 | I65% | 2 | oDrip |
| oIce | oSolid | Create_0 2, Destroy_0 14 | I100% | 4 | oDrip oFrozenCaveman |
| oIceBottom | oDrawnSprite | Alarm_0 2, Create_0 2, Step_0 2 | I87% | 2 | oDrip |
| oDarkFall | oMovingSolid | Col_oSolid 9, Create_0 8, Step_0 8 | I91% | 5 | oRubbleDark oSmokePuff |
| oFrozenCaveman | oDrawnSprite | Destroy_0 7, Step_0 1 | I64% | 0 | oCaveman |
| oDrip | oRubblePiece | Create_0 2 | spawned | 0 |  |
| oMoai | oSolid | Create_0 2 | I51% | 0 |  |
| oMoai2 | oSolid | Create_0 2 | I51% | 0 |  |
| oMoai3 | oSolid | Create_0 2 | I51% | 0 |  |
| oMoaiInside | oSolid | Create_0 2 | I51% | 0 |  |
| oCrown | oItem | Create_0 6 | I51% | 0 |  |
| oDark | oSolid | Create_0 14, Destroy_0 28 | I100% | 38 | oEmeraldBig oGoldChunk oGoldNugget oRubbleDark oRubbleDarkSmall oRubyBig oSapphireBig |
| oRubbleDark | oDrawnSprite | Create_0 3, Other_0 1, Step_0 7 | spawned | 0 |  |

#### Package D objects

| object | parent | events (GML lines) | levels with it (generator, 200 seeds) | rand | creates |
|---|---|---|---|---|---|
| oHawkman | oEnemy | Alarm_0 1, Col_oCharacter 74, Col_oWhip 17, Col_oWhipPre 17, Create_0 32, Other_7 5, Step_0 195 | T99% | 5 | oBlood oEnemySight |
| oTombLord | oEnemy | Col_oCharacter 33, Col_oWhip 8, Create_0 28, Draw_0 1, Other_7 16, Step_0 116 | T47% | 8 | oBlood oBone oFly oSceptre |
| oFly | - | Col_oCharacter 18, Col_oDamsel 26, Col_oEnemy 28, Col_oSolid 3, Create_0 2, Step_0 4 | spawned | 1 | oBlood oSmokePuff |
| oSceptre | oItem | Create_0 6, Step_0 6 | spawned | 0 |  |
| oGoldDoor | oDrawnSprite | Col_oSceptre 20 | T33% | 0 | oXGold |
| oXGold | oExit | - | spawned | 0 |  |
| oSmashTrap | oMovingSolid | Create_0 27, Destroy_0 9, Step_0 132 | T85% | 13 | oRubble oRubbleSmall |
| oSmashTrapLit | oMovingSolid | Create_0 27, Destroy_0 9, Step_0 132 | T11% | 13 | oRubble oRubbleSmall |
| oCeilingTrap | oMovingSolid | Create_0 15, Destroy_0 9, Other_7 1, Step_0 20 | T27% | 12 | oRubble oRubbleSmall |
| oDoor | oMovingSolid | Create_0 14, Destroy_0 4, Step_0 20 | T27% | 8 | oRubbleSmall |
| oLava | oWater | Create_0 5, Destroy_0 9, Step_0 16 | T78% O100% | 6 | oFlame oLavaDrip oMagma |
| oLavaDrip | oRubblePiece | Create_0 2, Other_7 1 | spawned | 0 |  |
| oMagma | oDetritus | Alarm_0 2, Col_oCharacter 17, Col_oEnemy 16, Col_oWater 2, Create_0 10, Other_7 6, Step_0 8 | spawned | 4 | oBlood oMagmaMan oMagmaTrail oSmokePuff |
| oMagmaTrail | oDrawnSprite | Create_0 2, Other_7 1 | spawned | 0 |  |
| oMagmaMan | oEnemy | Alarm_0 1, Col_oBomb 16, Col_oCharacter 21, Col_oEnemy 19, Col_oWhip 6, Create_0 25, Other_7 4, Step_0 88 | spawned | 13 | oBlood oBurn oMagma |
| oTemple | oSolid | Create_0 13, Destroy_0 35 | L62% T100% O100% | 38 | oEmeraldBig oGoldChunk oGoldNugget oRubble oRubbleSmall oRubyBig oSapphireBig |
| oTempleFake | - | Step_0 5 | T27% | 0 | oTemple |
| oOlmec | oMovingSolid | Alarm_1 27, Alarm_2 9, Alarm_3 9, Alarm_4 5, Alarm_5 8, Alarm_6 7, Create_0 35, Step_0 177 | O100% | 20 | oCaveman oHawkman oOlmecDebris oOlmecSlam oPsychicCreate2 oYellowBall |
| oOlmecDebris | oDrawnSprite | Create_0 12, Step_0 21 | spawned | 4 | oSmokePuff |
| oOlmecSlam | - | Alarm_0 1, Col_oBlock 7, Col_oPushBlock 7, Col_oSolid 11, Col_oTemple 7, Create_0 2 | spawned | 0 |  |
| oYellowBall | oDrawnSprite | Alarm_0 1, Alarm_1 2, Col_oSolid 18, Create_0 5, Other_0 1, Step_0 28 | spawned | 9 | oBat oFireFrog oFrog oSnake oSpider oYellowTrail |
| oYellowTrail | oDrawnSprite | Create_0 2, Other_7 1 | spawned | 0 |  |
| oPsychicCreate2 | oDrawnSprite | Create_0 5, Other_7 1, Step_0 2 | spawned | 0 |  |
| oFinalBoss | - | Create_0 2, Step_0 17 | O100% | 0 | oTemple oXEnd |
| oCavemanWorship | - | Create_0 1 | O100% | 0 |  |
| oLavaSolid | oSolid | Create_0 2 | O100% | 0 |  |
| oXEnd | oXStart | - | spawned | 0 |  |

#### Package E objects

| object | parent | events (GML lines) | levels with it (generator, 200 seeds) | rand | creates |
|---|---|---|---|---|---|
| oMachete | oItem | Create_0 7, Step_0 6 | M6% L5% I1% T1% O0% | 0 |  |
| oSlash | oWhip | Create_0 5, Other_7 1, Step_0 14 | spawned | 0 |  |
| oMattock | oItem | Create_0 7, Step_0 6 | M14% L3% I1% T0% O0% | 0 |  |
| oMattockHit | oWhip | Create_0 5, Other_7 52, Step_0 14 | spawned | 1 | oMattockHead |
| oMattockHead | oItem | Create_0 4 | spawned | 0 |  |
| oPistol | oItem | Create_0 7, Step_0 6 | M3% L3% I1% T3% O0% | 0 |  |
| oShotgun | oItem | Create_0 7, Step_0 6 | M4% L5% I1% T3% O0% | 0 |  |
| oWebCannon | oItem | Create_0 7, Step_0 6 | M3% L3% I1% T2% O1% | 0 |  |
| oWebBall | oDrawnSprite | Alarm_0 1, Alarm_1 2, Col_oEnemy 6, Col_oItem 3, Col_oSolid 3, Col_oWater 3, Col_oWeb 3, Create_0 6, Other_0 1, Other_7 6, Step_0 16 | spawned | 4 | oWeb oYellowTrail |
| oTeleporter | oItem | Create_0 7 | M2% L2% I1% T1% O1% | 0 |  |
| oBow | oItem | Create_0 8, Step_0 20 | M5% L3% I2% T2% O2% | 0 |  |
| oArrow | oItem | Alarm_1 10, Alarm_2 1, Create_0 5, Draw_0 1, Step_0 33 | spawned | 0 | oExplosion |
| oJetpack | oItem | Create_0 8, Step_0 3 | M2% L2% I0% T2% O2% | 0 |  |
| oParaPickup | oItem | Create_0 7 | M4% L3% I1% T1% O1% | 0 |  |
| oParachute | - | Col_oItem 5, Other_7 1, Step_0 5 | spawned | 0 | oParaUsed |
| oParaUsed | oDrawnSprite | Create_0 3, Step_0 9 | spawned | 0 |  |
| oCapePickup | oItem | Create_0 7 | M6% L3% I2% T1% O2% | 0 |  |
| oFlare | oItem | Alarm_0 2, Create_0 12, Step_0 18 | spawned | 4 | oFlareSpark oSplash |
| oFlareCrate | oItem | Alarm_0 2, Create_0 9, Step_0 17 | M8% L10% T12% | 4 | oFlareSpark oPoof oSplash |
| oFlareSpark | oDrawnSprite | Create_0 4, Other_7 1, Step_0 5 | spawned | 0 |  |
| oLamp | - | Create_0 1 | M31% L19% I9% T6% | 0 |  |
| oLampRed | oLamp | Create_0 1 | M5% L3% I2% T0% | 0 |  |
| oDice | oItem | Create_0 12, Draw_0 5, Step_0 206 | M4% L3% I1% T1% | 4 | oBlood oLeaf |
| oSacAltarLeft | oSolid | Create_0 3, Destroy_0 60, Step_0 5 | M24% L26% I22% T26% | 12 | oBall oChain oGhost oRubble oRubbleSmall |
| oKaliHead | oDrawnSprite | Alarm_0 8, Create_0 6 | M24% L26% I22% T26% | 4 | oSpider |
| oBall | oItem | Create_0 6, Step_0 37 | spawned | 0 |  |
| oChain | oDrawnSprite | Create_0 2, Step_0 9 | spawned | 0 |  |
| oCrystalSkull | oGoldIdol | Create_0 3 | L4% | 0 |  |
| oUdjatEye | oItem | Create_0 6 | spawned | 0 |  |
| oAnkh | oItem | Create_0 7 | spawned | 0 |  |
| oKapala | oItem | Create_0 6 | spawned | 0 |  |
| oPaste | oItem | Create_0 7 | M2% L3% I2% T3% O0% | 0 |  |
| oGloves | oItem | Create_0 7 | M7% L4% I3% T4% O1% | 0 |  |
| oMitt | oItem | Create_0 7 | M5% L2% I1% T3% O1% | 0 |  |
| oCompass | oItem | Create_0 7 | M4% L3% I2% T3% O1% | 0 |  |
| oSpectacles | oItem | Create_0 7 | M4% L2% I0% T4% | 0 |  |
| oSpringShoes | oItem | Create_0 7 | M4% L4% I2% T2% O0% | 0 |  |
| oSpikeShoes | oItem | Create_0 7 | M5% L2% I2% T2% O1% | 0 |  |

## 3. Reference routes and traces

Every route is `tests/routes/c_<package>_<what>.txt`. Its header has `# seed`, `# level` and, for E, `# globals`.
`scripts/content_traces.sh` records it in the HD runner with the enemies kept, TRACE_HUD=1 and TRACE_SND=1 into
`build/trace/c_<package>_<what>_s<seed>`.

New tracer option: `TRACE_GLOBALS=name=value,...` sets globals before the level starts, as TRACE_LEVEL / TRACE_MONEY
do. Unset, the tracer output stays byte-identical.
- E's routes use it to start with an item: `pickupItem=Shotgun`; oLevel Create's scrHoldItem gives it.
- They also use it for equipment: `hasJetpack=1`.
- playhost has no matching option yet. Package E adds `--global` with its C side (G.pickupItem through gen.c's
  scrHoldItem, PG.has*).

How the routes were chosen:
- Seeds come from the generator: the start nearest an instance of the target object.
- Inputs were searched in the C play loop for the walk that gets closest without dying; movement is translated, so
  the runner follows the same path until the target object acts.
- The start rooms are closed, so most targets stay 25-60 px away, in view: enemies within the view's activity
  range act, and several come to the player.

| route | seed / level | target, as recorded (runner) |
|---|---|---|
| c_jungle_mantrap | 44 / 5 | 4 man traps active (move, status 0/1), closest 41 px; also frogs, fire frogs, a monkey in the level |
| c_jungle_frog | 71 / 5 | 9 frogs, statuses 0-2, closest 36 px; xfrog 4 |
| c_jungle_firefrog | 296 / 5 | 4 fire frogs reach the player (5 px): bombs explode, the player is hurt and dies; xfrog 7, xexplosion 2 |
| c_jungle_monkey | 83 / 7 | 5 monkeys moving (status 5/6), closest 57 px; frogs too |
| c_swamp_piranha | 537 / 5 | a piranha swimming (moving 363 records), 49 px; the player stays dry |
| c_swamp_deadfish | 696 / 5 | 3 dead fish moving, 44 px |
| c_swamp_vampire | 30 / 5 | 4 vampires, statuses 0/2/7, 24 px |
| c_swamp_zombie | 215 / 5 | 7 zombies, statuses 0-2, 40 px; xzombie 4 |
| c_ice_yeti | 143 / 12 | 7 yetis active, 40 px; aliens too |
| c_ice_ufo | 76 / 10 | 11 UFOs, lasers fire and hit the player (xlaser 4, xhurt 4), 22 px |
| c_ice_springtrap | 12 / 12 | the player stands on a spring trap (3 px, not triggered); UFO lasers hit (xlaser 6) and kill him |
| c_ice_darkfall | 202 / 9 | a dark-fall block 6 px away that does not fall (weak: redo with a walk across it) |
| c_temple_hawkman | 4 / 14 | 4 hawkmen fight the player (2 px; statuses 0-3, 98: stunned), xalert 30 |
| c_temple_tomblord | 530 / 13 | a tomb lord walking (statuses 0-2), 40 px |
| c_temple_smashtrap | 53 / 13 | 2 smash traps moving (status 0/1), 38 px |
| c_temple_olmec | 1 / 16, room rOlmec | TRACE_ROOM=rOlmec: the intro, then Olmec moves, jumps and slams (xbigjump 6, xslam 4, psychic, mBoss); the player dies; 931 records |
| c_items_shotgun | 559 / 1 | pickupItem=Shotgun: 3 shots (18 pellets, xshotgun 3, xhit 18) |
| c_items_mattock | 559 / 1 | pickupItem=Mattock: 6 mattock hits, 3 blocks dug (xcrunch 3) |
| c_items_bow | 559 / 1 | pickupItem=Bow, arrows=6: 4 arrows shot (xbowpull 8, xarrowtrap 4) |
| c_items_jetpack | 559 / 1 | hasJetpack=1 had no effect: no flight (the player only jumps; cause not checked); redo by picking a jetpack up |

Weak routes to redo: c_ice_darkfall, c_items_jetpack, c_ice_springtrap (trap not triggered). The yeti king, alien boss,
scarab and flare crate have no route yet: the generator places no oYetiKing / oAlienBoss (special levels), oScarab and
oFlareCrate appear only at run time or in rare rooms. playhost has no --room option yet (package D adds it with
rOlmec's start) and no --global (package E).

To check a trace reaches its object: the scratch scripts summarised each trace (records present, movement, closest
approach, status values). The C side of a package is gated with tools/playcmp.py against these traces, after the
package's playhost options exist, and with tools/sndcmp.py.
