# Draw code against the CPS3 video hardware: cost, options, measurements

Status 2026-10-04, on 03f01c5. MAME 0.289 clocks (`scripts/mame.sh`); jtcps3 not run (needs the .81).

## Summary

- The display list is far inside the hardware limits: at most **214 entries** (attract) and **9 main-list
  records** in any frame of 74 routes plus 3,600 attract frames (27,544 frames), against jtcps3's 511 entries and
  510 records. Nothing in the list needs restructuring to fit.
- Writing sprite RAM is a small part of the draw. In the busiest gated frame (p4_exit559 rec 560: 165 drawables,
  137 entries) the draw is 159,872 clocks: instance scan 42 K, sort 18 K, list 74 K, HUD 26 K. The list part is
  ~540 clocks an entry, mostly Draw-event logic per instance; the entry writes are a few percent of it.
- **97.8 % of frame draws are one-piece frames** (2,211 of 2,326 frames have one piece). Prebuilt sublists save
  writes only for multi-piece frames: about one such instance per frame. Not worth building.
- The measurable waste was in VBlank: `draw_vblank` compared every on-screen cell of three tilemaps every frame
  (9.5-16 K clocks a frame). The prototype keeps a queue of the cells `cell_fix` changed: **288 clocks a frame**.
- Prototype (this branch): dirty-cell queue for the tilemaps, plus sprite entries written directly to draw.c's own
  sublist area (no SDK call per entry). Per 2-frame step pair: **about 14 K MAME clocks saved** (of the ~50 K
  draw + VBlank + sound budget in docs/PERF.md). Frames identical (below).

## 1. Current cost (MAME clocks, baseline 03f01c5)

`tests/gametime` (attract 3,600 frames; game 1 p4_push_rope; game 2 p5_snakes), per frame (vbl) / per step (draw):

| Section | vbl mean | draw mean | draw max |
|---|---|---|---|
| attract | 6,892 | 79,151 | 756,832 |
| game 1 p4_push_rope | 8,018 | 42,543 | 829,664 |
| game 2 p5_snakes | 8,678 | 35,225 | 55,136 |

`DRAW_PROFILE` parts (set-up, instance scan, sort, list, HUD) on busy frames:

| Frame | draw | scan | sort | list | HUD | entries | vblank |
|---|---|---|---|---|---|---|---|
| p4_exit559 rec 560 | 159,872 | 42,368 | 17,824 | 73,760 | 25,760 | 137 | 9,472 |
| c_swamp_drain rec 300 | 104,064 | 36,032 | 15,776 | 50,976 | 1,120 | 93 | 13,760 |
| p5_snakes rec 200 | 54,816 | 19,008 | 8,608 | 23,520 | 3,520 | 43 | 14,112 |

Max draw (~0.75-0.9 M) and max vbl (~0.5-0.75 M) are room starts (tilemap rebuild, 4,096 background cells).

## 2. Entries and records per frame (host count, `tests/game/host.c` with a stats hook)

All 73 routes that run on the host (c_temple_olmec crashes the host build: bus error, not investigated) and the
attract cycle (seed 7, 3,600 steps):

| | Value |
|---|---|
| Entries a frame (bands as 128-line entries): max / p99 / mean | 214 / 214 (attract) / 59.2; busiest routes p4_darkexit 200, p4_exit559 199, c_swamp_drain 174 |
| Main-list records a frame | max 9 (background + 3 bands, each band closes a group, the HUD) |
| Frames over 480 / 511 entries | 0 / 0 |
| `frame_out` calls a frame | mean 46, max 180 |
| Pieces: all of the frames drawn / on screen | mean 50.1 / 36.7 |
| Whole frames off screen (drawn instance, no piece visible) | mean 12.4, max 66 |
| Extra entries without the software clip | mean 13.4, max 68 (max frame would be ~282, still < 511) |
| Visible multi-piece instances a frame | mean 0.98, max 9 |
| Distinct frames drawn over all routes | 671 (21 multi-piece); 159 drawn mirrored, **no multi-piece frame drawn mirrored** |
| Frame draws | 929,245, of them multi-piece 20,866 (2.2 %) |

## 3. Options

### 3.1 Prebuilt sublists (PLAN.md :87)

Sprite RAM for them: CPS3V_PRE_A + PRE_B = 401,408 bytes (368,640 with the prototype's 32 KB run area). A sublist
starts on a 256-byte boundary (main-list word 0 bits 4-14 x 256), so "packing by size class" cannot put two
sublists in one 256-byte block.

| Option | Sprite RAM | Saves | Limits |
|---|---|---|---|
| All 2,326 frames x2 (plain, mirrored) | 1,189,376 B | - | does not fit |
| Multi-piece frames only (106), x2 | 57,344 B | n - 1 entry writes per multi-piece instance: ~1 instance a frame | one-piece frames: one entry either way (Maldita's rule, maldita docs/CPS3.md Item 2) |
| Frames used in play only | 21 multi-piece frames, ~12 KB | same | the routes do not cover all content |
| Drop mirrored copies (record flip) | halves the above | - | MAME: record word 2 bit 28 (`global_xflip`) XORs each sprite's flip and mirrors it about its own centre; piece positions are not mirrored, so a multi-piece frame still needs its mirrored sublist. jtcps3 RTL not available (`../jtcores/cores/cps3` is empty): Unknown there |
| Cache of recent frames written at VBlank | small | same | adds bookkeeping for a saving of ~1 instance a frame |

Entry count is unchanged by any of them: jtcps3 counts entries drawn, however they are grouped.
**Inferred: no prebuilt-sublist variant pays here.** Spelunky's art is 95 % one-piece frames (Maldita's was not).

### 3.2 Camera in a global scroll (question 2)

Hardware: main-list word 0 bits 28-30 pick one of 8 global scroll registers, added to every entry's position
(MAME `screen_update`; `cps3v_vblank` writes all 8 as 0). Positions are 10 bits and wrap (screen x/y = (pos +
scroll) mod 1024, >= 512 negative). The camera could go in scroll 1 with entries in room coordinates.

Today the camera costs 2 subtractions per instance (`spr_out` / `cached_out` pass `x - ox`), not per piece; the
per-piece work is the add of dx/dy, needed either way. A global scroll only pays if entries are kept across frames,
and they are not: depth order, frames and positions change every step, so the list is rebuilt anyway. With a scroll
the coarse cull must still keep every entry within ±512 px of the screen (rooms are up to 672 x 880 px). Not worth
changing.

### 3.3 Software clipping per piece (question 3)

- MAME clips every sprite to the render clip (`cps3_drawgfxzoom` with `m_renderbuffer_clip`); after the 10-bit
  wrap a position >= 512 is negative, so off-screen pieces within the wrap range are not drawn.
- jtcps3: sprites partly off every edge were drawn exactly (cps3-testgame vtest phases 0-4: 0 px; Maldita r14_a
  with a mirrored sprite at the left edge). Wholly off-screen sprites were not tested on jtcps3: Unknown.
- The test is 4 compares per piece (~10 clocks) and removes mean 13.4 / max 68 entries a frame from the 511-entry
  budget and from the line buffer's work. **Keep it.** The per-piece clip is cheaper than the entry it saves
  (~4 sprite-RAM stores, 6 clocks each on jtcps3).

### 3.4 Tilemaps (question 4)

- Scrolling is by register only: `draw_vblank` writes the 4 tilemaps' scroll registers each frame.
- Every room fits one 64 x 64 tilemap (largest rLevel 42 x 34 cells, rOlmec 42 x 55), so the whole room is
  resident: no edge streaming is needed.
- Waste found: `draw_vblank` compared mwant with mshown for every on-screen cell of every map each frame
  (about 21 x 15 x 3 = 945 compares) to find the few changed cells. **Prototype:** `cell_fix` (the one place
  mwant changes) queues the cell (512 entries, overflow -> full compare); `draw_vblank` writes the queued cells,
  or every room cell after a room build / tile-layer change. Rooms over 64 cells a side keep the on-screen compare.

### 3.5 Character DMA (lead's question)

Art is 1.28 MB (gfx.bin) + 35 KB (HUD), loaded once at boot by character DMA. At jtcps3's measured 2.2 clocks a
byte that is ~2.9 M clocks, about 7 frames (0.12 s), once. Everything is resident (1.3 MB of 8 MB character RAM),
so there is nothing to stream at run time. RLE (command 3, 8 bpp) would shrink the SIMM image and the boot copy;
neither is a constraint. Not worth building.

## 4. Prototype (this branch)

`src/draw/draw.c`:

1. Dirty-cell queue for the tilemaps (3.4).
2. Sprite entries written directly to sprite RAM: `ent_put` writes the 4 words of `cps3v_sprite` into draw.c's own
   sublist area (two 16 KB halves used in turn at 0x38000-0x3ffff, the end of CPS3V_PRE_A); a run is closed into
   one main-list record (`cps3v_object` at position 0, each entry's own colour code) before a band, the front end's
   text, the HUD, and at 511 entries. Word 2 comes from a 5 x 5 table.

`tests/game/host.c`: `host_sprram` and a `cps3v_object` that decodes the entries back into its list (checks
the word-2 format). `tests/game/sprbss.ld`: area A's test arrays must end below 0x38000 (they end at 0x14d60).

### Frames identical

- Host: composed frames of the HEAD build and the prototype, every recorded frame, byte-identical in 146 of 146
  runs (73 routes x HUD on/off, attract seeds 7 / 1 / 99 x 3,600 steps, rHighscores cycle).
- MAME: snapshots byte-identical to the baseline's on p4_exit559 (14 records), p5_snakes (5), c_swamp_drain (5),
  p7_dark (2). Against `tools/drawmodel.py`: 0 px on p4_exit559 x14, p5_snakes x5, p5_spider x5, p5_shop x5,
  p8_boot (attract) x7, p8_scores x5; c_swamp_drain rec 240 / 300 differed 53 / 172 px **in the baseline too**
  (same snapshots): a model error, fixed (section 5). p7_dark differs from the model by 45-79 K px at HEAD too:
  the program does not play the traced level (section 5).
- jtcps3: not run (the .81 is shared; ask the lead). The entry and record words are the SDK's, already 0 px on
  jtcps3 via `cps3v_object` in Maldita.

### Measured (MAME clocks)

`tests/gametime`, baseline -> prototype:

| Section | vbl mean | draw mean | vbl max |
|---|---|---|---|
| attract | 6,892 -> **1,015** | 79,151 -> 77,203 | 751,264 -> 797,632 |
| game 1 p4_push_rope | 8,018 -> **1,238** | 42,543 -> 41,546 | 490,880 -> 642,080 |
| game 2 p5_snakes | 8,678 -> **2,105** | 35,225 -> 34,667 | 685,344 -> 836,928 |

Step code unchanged (303,891 / 358,289 -> +1 / +2). Per step pair (2 VBlanks + 1 draw): game 1 -14.6 K,
game 2 -13.7 K. DRAW_PROFILE, p4_exit559 rec 560: draw 159,872 -> 154,272 (list 73,760 -> 69,120), vblank
9,472 -> 288.

Cost: the room-start VBlank (already over one frame) writes every room cell instead of the on-screen ones:
+47 K to +151 K on that one frame. Not spread over later frames: a room start's step is 2.0-2.8 M clocks (level
start) up to 30 M (generation) (docs/PERF.md), so the room start is a multi-frame stall either way and +0.15 M
(under 8 % of the smallest) changes no budget. Spreading would only pay once room starts fit their frames.

## 5. Frame differences that existed at HEAD

### c_swamp_drain rec 240 / 300 (53 / 172 px): the model was wrong (fixed)

The MAME frame shows the dead player lying on the floor; the model left it out. `tools/drawmodel.py`
`blink_toggles` rebuilds oPlayer1.blinkToggle (not traced) from `invincible`: a record with invincible 30 or 60
and `inv > prev - 1` counted as a new hit and re-armed 30 blink steps. oPlayer1 Step :1903 counts invincible
down only while not dead, so a dead player keeps 30 and every record re-armed the blink: the model blinked the
body forever, while the GML (and src/game) blink 30 steps after the death and then draw it (`else blinkToggle =
-1`, :1911). Rule now `inv > prev`. Records whose blinkToggle changes, over all traces: c_swamp_drain 71,
c_swamp_swim 76, p5_caveman 31. Host draw (every record, `tests/game/host.c`) against the model: c_swamp_drain
71 -> 0 of 370 frames differ, c_swamp_swim 76 -> 0 of 418. Limit of the rule: a second hit on an already dead
player (invincible set to 30 again, unchanged) would not be seen.

p5_caveman still differs on 115 of 290 host frames (from rec 176, an object ~50 px wide at view x 38-89), with
either rule: a separate difference, not investigated here.

### p7_dark (45-79 K px): the program plays a different level; draw and model are right

The model equals the runner's own frames (0 px on all 6 TRACE_SHOT frames). The MAME frames show another level:
at rec 100 a lit level with the snake-pit message ("I hear snakes"), life 3, against the runner's dark level
("I can't see a thing!"). The route has `# nodark 0` (TRACE_NODARK=0: global.noDarkLevel false), but
`tests/game` (mkroute.py, game_cfg) has no nodark / globals / room setting and src/game's clear of the globals
leaves noDarkLevel = 1 (gen.c:58), so level 2 is generated without darkness and the RNG goes another way. Only
playhost has `--nodark` (test/host/playhost.c:220). Second gap, for when it plays the right level: src/game
never sets `PLEV.darkness` (oLevel Create :16 darkness = 1 and the Step's darkness section :110-136 from the
flare / light-source distance are not translated), so draw.c would fade by 0. Both are outside src/draw: a
nodark / globals / room field in game_cfg and mkroute.py, and oLevel's darkness in src/game. The dark path
itself was gated with DARK=a8 (forced alpha).

### c_temple_olmec crashes the host build

`tests/game/host.c` (and the same tests/game snapshot in MAME) dies with SIGBUS in `play_level_start`
(prun.c:301, `PE(p)->alarm[a] = ...`) at game start. The route needs `# room rOlmec`, which tests/game cannot
set, so the host generates a level-16 rLevel instead, and that level has more instances than the snapshot's
`PIN_MAX 1000` (scripts/game_check.sh and gametime set it): `pin_add` returns PIN_DEAD and the copy loop writes
through that slot's ext. With PIN_MAX 4096 (src/game's own value) the same run completes, ASan clean. So: a
test-harness limit (PIN_MAX 1000, no room setting), plus a missing PIN_DEAD check in `play_level_start`
(src/game).

## 6. Instance capacity (PIN_MAX, EXT_MAX)

The shipping / SH-2 builds ran with PIN_MAX 1000 (the test scripts' sed of play.h's 4096) and EXT_MAX 400.

Generated instances (build/host/genhost over tests/gen, 8,080 cases; alive instances after generation):

| Set | Cases | Max | Cases >= 999 |
|---|---|---|---|
| mines (1-4) | 1,600 | 962 | 0 |
| lush (5-8) | 2,200 | 1,404 (lake) | 203: every lake case (about 570 oWaterSwim + 560 oLush) |
| ice (9-12) | 1,400 | 921 | 0 |
| temple (13-15) | 1,600 | 883 | 0 |
| olmec (16) | 400 | 1,290 | 400 |
| chains (natural lake on level 6) | 80 | 1,267 | 8 |

Play (playhost, every route with its header options): slots used up to 1,262 (c_temple_olmec), 996 on
c_swamp_vampkill; play adds up to 117 slots over a room's start (c_swamp_drain), 113 (p4_darkexit). The largest
generated levels played 300 steps idle (a scratch playhost with lake / cityOfGold / prob* globals and the level's
own room): lake up to 1,537 slots, Olmec 1,359. pin_ext records: lake about 790 with EXT_MAX raised (oWaterSwim
took one each), Olmec 381 (cityOfGold).

So PIN_MAX 1000 failed every Olmec level and every lake level: `play_level_start`'s copy wrote slot PIN_DEAD = 999,
which is generator instance 999's memory not read yet (the c_temple_olmec SIGBUS), and pin_add's callers wrote
through PIN_DEAD's record 0 (the shared defaults). EXT_MAX 400 failed every lake level (ext_alloc returned record 0).

Changes (src/game, harnesses):

- **PIN_MAX 1792** in play.h (the scripts' default PIN, their sed now matches any value). PIN_MAX > INST_MAX
  (1536) is a compile-time check: every generated instance gets a slot (the loader and pcol.c's gen_load).
- **oWater, oWaterSwim as terrain** (pworld.c terrain_names): Create / Destroy only, no alarms, no collision
  events; their only variables are `type` (struct pin) and `checked` (written by oLevel's commented-out
  deactivation, read nowhere). They stay in the collision grid (terrain is), so collision_point / instance_place on
  oWater see them as before. Lake levels then need at most 212 ext records.
- **EXT_MAX 448** (381 used at most, 66 left).
- **Guards:** PIN_DEAD's record is a reserved scratch record (EXT_SCRATCH = EXT_MAX - 1, never allocated; record 0
  stays the defaults); a full ext_alloc returns it too (PUNTR 9005); a full pin_add sets PIN_DEAD up and returns it
  (PUNTR 9001); pin_create then makes no collision entry and runs no Create; the loaders (prun.c, ptrans.c) skip
  a PIN_DEAD slot. Checked with an ASan playhost at PIN_MAX 1411 / INST_MAX 1410 / EXT_MAX 150: the lake level
  fills every slot, Olmec and c_swamp_vampkill run out of ext, all three run to the end with no ASan report.
- **RAM check:** tests/ramcheck.ld, linked into tests/game, gametime and playsh2 (both variants): .bss must end
  32 KB below the stack top. tests/game at PIN_MAX 1792: .data 14.0 K + .bss 458.5 K, 50 KB left for the stack;
  PIN_MAX 3000 fails the link.
- **Route options for the game program:** game_cfg nodark / room / globals (src/main/game.c), filled from the
  route's `# nodark`, `# room`, `# globals` lines by tests/game/mkroute.py (tests/game and gametime), as playhost's
  options; room also takes rLevel2 / rLevel3 (the cabinet keeps -1: the level's own room).

MAME (tests/game, new options): c_temple_olmec (room rOlmec) and `tests/routes/l_lake8.txt` (lush seed 479398616
level 8, lake=1, room rLevel3: the largest generated lake level) run to their end, no crash (draw max 1.17 M at the
room start, 69 entries).

For the oJaws work (the lake boss has no Step: untranslated 5003 on every lake level): the largest lake levels,
`lake=1`, noDarkLevel 1, room rLevel3 (tests/gen/lush.txt): seeds 479398616 (level 8, 1,404 instances),
1769241775 (5), 1305291916 (5), 243538780 (8), 587626975 (6), 235542814 (7), 908267969 (7), 738162012 (8);
`tests/routes/l_lake8.txt` runs the first in the game program. The host tracer can start only in rLevel / rOlmec,
so these levels have no runner trace yet.

c_temple_olmec in MAME against the model: 62-63 K px differ at rec 100-300. The model draws bgCave and follows
the player; rOlmec's background is bgTemple (refs/hd/src/rooms/rOlmec, as draw.c draws it) and oOlmec's Create
makes the view follow oOlmec (:37-40). Both are tools/drawmodel.py gaps (not fixed here); the content traces have
no runner frames for rOlmec to confirm the camera.

## Observed / Inferred / Unknown

- **Observed:** entries max 214, records max 9 over 27,544 frames; 97.8 % of frame draws one-piece; no
  multi-piece frame drawn mirrored; MAME's record flip mirrors pieces in place; vblank cell compare 9.5-16 K a
  frame -> 288 with the queue; frames identical (host 146/146, MAME snapshots byte-equal).
- **Inferred:** prebuilt sublists, a global-scroll camera and dropping the clip do not pay for this art and these
  scene sizes; the remaining draw cost is per-instance logic (scan 42 K, sort 18 K, Draw events in "list"), not
  video hardware use. On jtcps3 (~3.4x MAME for memory-bound code) the saving is ~45 K clocks a step pair.
- **Unknown:** jtcps3 behaviour for wholly off-screen sprites and record flip (RTL not available); the
  prototype on jtcps3; p5_caveman's remaining host-vs-model difference.
