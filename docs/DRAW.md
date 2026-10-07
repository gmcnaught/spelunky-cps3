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
  32 KB below the stack top; PIN_MAX 3000 fails the link. Correction (after the merge, e264467): the "50 KB left"
  above was measured before EXT_MAX 448 and main's grid / perf2 arrays; e264467's tests/game ended .bss at
  0x0207a4c4 (23 KB of stack) and the playsh2 JT variant overflowed main RAM by 22 KB. inst_mem (129 KB) cannot
  leave main RAM: the play instances are that memory (pworld.c PW.in). Fix, cold arrays only:
  - tests/game/sprbss.ld (tests/game, gametime): the generator's lists (inst.c with_pool, gw_*, ghome), the front
    end's layer order (front.c lpos, lpos_id, w_alive);
  - draw.c's per-instance claim arrays (cnext, ccell, ctile, bnext, bpos: touched for changed instances only) in
    DRAW_CACHE_SECTION;
  - tests/playsh2/jtcold.ld (JT variant): the arrays used only while generating (pcol.c rn, the R-tree nodes:
    play-time collisions use the grid; gtiles; inst.c's lists), cleared by main.c. Its generation timings then
    include sprite-RAM accesses; step timings do not. tests/game already kept rn in sprite RAM.

  Result: tests/game .data 14.0 K + .bss 436.2 K, 72 KB left for the stack; playsh2 JT .bss to 0x0206efb4, 68 KB.
  Stack left (bytes, 0x02080000 - __bss_end): tests/game 74,000, gametime 66,240, playsh2 81,104, playsh2 JT 69,708.
  gametime's step / draw means unchanged (game 1 223,289 / 40,017; game 2 263,855 / 34,843 MAME clocks).
  The R-tree (rn) stays in every build: the generator uses it (gmode, bit-exact), and the grid build's play reads
  rn[rroot].level in query_e (ShouldUseFastCollision); it is already in sprite RAM in tests/game, gametime and both
  playsh2 variants.
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

c_temple_olmec in MAME against the model differed by 62-63 K px at rec 100-300: tools/drawmodel.py drew bgCave and
followed the player. Fixed in the model: the background is bgTemple in rOlmec and on levels 13-16 (scrInitLevel
:140-143, levelType 3; draw.c's rule), and in rOlmec the view follows oOlmec with hborder 0 until oOlmec's Alarm_5
has fired (Create :37-40, Alarm_5 :1-4; the trace shows alarm 5 at 0 on that step, rec 437). Now 0 px at rec 100,
200, 300. The content traces have no runner frames for rOlmec, so this rests on the GML and the trace's view.

tests/game builds the shipping collision grid since main's 58756ab (scripts/game_check.sh EXACT=1: the R-tree).
Routes the grid takes off the trace (c_swamp_drain from rec 139, docs/EQUIV.md) then differ from the model in
MAME with no draw error: c_swamp_drain rec 180 / 240 / 300 differ 268 / 976 / 56 K px on the grid build, 0 / 0 / 0
with EXACT=1.

## 7. Smooth motion (2026-10-05)

`draw_smooth` (settings SMOOTH MOTION; routes off): the Spelunky Classic Interpolated method (logic at 30 steps a
second, drawn positions blended between steps) on the CPS3's two frames a step.

- **Midpoint list.** `ent_put` writes each entry twice: at its place in the run area (0x38000-0x3ffff) and, moved by
  (mdx, mdy), at the same place 0x8000 lower (0x30000-0x37fff, the same records). Per instance, `inst_mid`: the
  offset is half the way back to the slot's last drawn place (`hist[]`: id, whole-pixel x / y, draw count; only the
  same id drawn at the previous draw, moved at most 32 px), less half the camera's step (`ocx`, `ocy`; no midpoint
  when the camera moved over 64 px or the room was built). Tile sprites move with the camera only; SDK entries (the
  bands, the HUD, the front end's text) are shared by both lists, the tilemaps take the midpoint camera through their
  scroll registers. A piece is kept when it is on screen in either list; the view tests are wider by the camera's
  half step.
- **Showing it.** At the step frame's VBlank, `draw_vblank` copies the main list (sprite RAM 0, at most 32 records)
  for later and points its run records 0x8000 lower: the midpoint list is sent. `draw_vblank_end` arms the VBlank
  interrupt (`src/main/main.c` vbl_irq, installed through a RAM copy of the vector table): at the next VBlank,
  `draw_vbl_irq` writes the kept records and the frame's scrolls and sends the list. The next draw may be building its
  main list at that moment, so the records written are saved and put back after the list DMA (the busy bit is waited
  for to come up, then down: on jtcps3 the character DMA's comes up 120-576 clocks late). Both lists' sublists stay
  untouched until the draw after next (run areas and the SDK's sublist areas alternate per draw).
- **Checked (MAME):** `scripts/smooth_check.sh` snapshots each held record twice: the record's own frame (sent by the
  interrupt) equals the model at 0 px on p4_exit559 30 / 150 / 300, p5_shop 162 / 242, p8_boot attract 300 / 1040;
  the midpoint frames are the scene half a step back (p4_exit559 rec 300: 52,111 px differ from the own frame, terrain
  tilemaps and sprites shifted together). Off (routes): game_check p4_exit559, p5_shop, p8_boot 0 px, the host check
  801 / 801 frames equal.
- **Cost (tests/gametime, MAME clocks, off -> on):** draw mean attract 77.7 -> 92.4 K, game 1 40.8 -> 49.9 K, game 2
  35.4 -> 42.5 K; pair mean game 1 133.3 -> 142.6 K, game 2 131.6 -> 138.9 K, game 2 max 170.0 -> 180.3 K; steps
  unchanged. The interrupt's own time is not in these (it came while the program waited for VBlank).
- **Inferred:** about 20-30 K jtcps3 clocks a pair (draw ~2.8x MAME); p5_snakes' jtcps3 pair max 710.4 K would be
  about 740 K, under 838.9 K. **Unknown:** the jtcps3 run (gametime SMOOTH=1 on the MiSTer), and whether jtcps3's
  list DMA reads sublists at the DMA or while it draws (either is safe here).

## 8. Sprite entries by DMAC, the list DMA not waited for (2026-10-06, branch sprdma)

PERF3 5.3: (a) `cps3v_vblank` spun on PPU status 0x0c bit 0 after starting the list DMA; (b) the entries were
written by ~4 CPU stores each (up to ~680 a draw: p4_exit559 max 137 entries, c_swamp_drain 169) into uncached
sprite RAM, and a main-RAM buffer sent by the SH-2 DMAC was proposed.

- **List DMA (on):** `draw_list_send` writes cps3v_vblank's registers (8 global scrolls 0, 8 / 9 four times to 0x82);
  `draw_list_sync` waits for bit 0 to clear and writes 0 to 0x82, at the next point that needs the copy done:
  src/main/main.c after `snd_frame` (before the shell writes a list), `draw_frame`, `draw_vblank`, `draw_vbl_irq`,
  `shell_video_stop`. tests/gametime counts both in "vbl".
- **Word 3 (on):** an entry's word 3 is always 0: `draw_boot` writes it in every slot of the run and midpoint areas
  (0x30000-0x3ffff), `ent_put` writes words 0-2. Smooth motion: the midpoint entry no longer reads word 0 back from
  sprite RAM (an uncached load).
- **DMAC (`DRAW_SPRDMA`, default 0; SPRDMA=1 in tests/game, tests/gametime and their scripts):** entries built in
  `sbuf` (main RAM, 2 halves x 8 entries x run + midpoint = 512 B: tests/game had ~1 KB above the 32 KB stack floor),
  a half sent when full and after the frame's last run by channel 0 (run area) / 1 (midpoint area), 16-byte units,
  cycle steal (CHCR 0x5e01), TCR in longwords; the next transfer on a channel waits for the previous one; the list
  send waits for both.

**TCR counts longwords.** In 16-byte mode the SH7604's TCR is decremented once per longword (jtframe
`sh7604/DMAC.sv`: TCR - 1 on every write beat, address + 4; MAME `sh7604.cpp`: `count &= ~3`, -4 per 16-byte unit).
cps3-testgame's ttest (`src/ttest.c:329`) set TCR = bytes / 16, so its "DMAC 16" row moved 1 KB, not 4 KB: the 0.77
clocks a byte quoted in PERF2 / PERF3 / REVIEW-SH2 was 4x low (corrected there).

**jtcps3 (.62, `tests/dmac`, 4 KB, FRT at 8-clock resolution, interrupts masked):**

| | main RAM -> main RAM | main RAM -> sprite RAM |
|---|---|---|
| DMAC 16-byte, cycle steal / burst | 13,200 / 12,840 | 12,736 / 12,736 |
| DMAC longword, cycle steal / burst | 13,584 / 13,360 | 13,360 / 13,392 |
| CPU 32-bit stores (1,024, unrolled by 8) | 6,984 | 6,928 |

- DMAC ~3.1 clocks a byte (~8.0 MB/s at 25 MHz), CPU stores ~1.7 (~14.8 MB/s).
- Readback into sprite RAM: 0 bad words of 1,024 in every mode, the word after the end unchanged (MAME the same).
- A cached DT / BF loop of 48,040 clocks loses -8..+56 clocks while any of the four transfers runs (burst too): code
  running from the cache is not slowed; only the CPU's own bus accesses contend.

**jtcps3 tests/gametime (.62; attract / game 1 p4_push_rope / game 2 p5_snakes):**

| | VB mean a frame | DR mean a step | pair mean |
|---|---|---|---|
| main before (16ab24f) | 3,118 / 3,178 / 4,430 | 164,415 / 104,803 / 93,854 | 304,876 / 415,203 / 442,817 |
| list not waited for + word 3 (default) | 2,755 / 2,933 / 4,228 | 166,841 / 106,116 / 95,024 | 304,461 / 414,780 / 444,464 |
| same + DMAC (SPRDMA=1) | 2,705 / 2,915 / 4,223 | 181,034 / 117,511 / 101,122 | 320,395 / 424,861 / 445,682 |

- The list send: VB -200..-360 clocks a frame.
- The DMAC: draw +7.3..+16.6 K a step (+8..+11 %): a DMAC byte costs ~2x a CPU-store byte, and the draw's own
  loads and stores compete with it for the bus. It stays off.
- Default vs main draw +1.2..+2.4 K with fewer stores; the step (unchanged code) moves by similar amounts between
  builds: inferred code-layout / cache placement, not measured further (one run each).
- MAME (tests/gametime game 2 draw mean): 41,306 / 41,179 / 41,695.

**Frames:** MAME snapshots of main (16ab24f) and the branch byte-identical (p4_exit559 8, p5_shop 5, p5_spider 5,
p8_boot attract 2, c_swamp_drain 4, smooth p4_exit559 own + midpoint 6; SPRDMA=0 18 of them too); host every step
(5 routes, 2,432 frames) identical for main, DMAC and CPU paths; capture_check PASS. jtcps3 (`scripts/jt_frames.sh`
on e8e67d3, .62): p4_exit559 30 / 300 / 520 / 800 and p5_shop 162 / 242 each with shots at 0 px against the model,
the other shots 0-8 px plus jitter (PLAN.md P3's transient), or taken as a hold began or ended (696 / 712 px).
Attract 1040: 153 px on jtcps3, where MAME vs model is 388 px on that build too (the regenerated g_p8_boot_s7 trace
or merged src changes: not the draw, whose MAME frames equal 16ab24f's).

## Observed / Inferred / Unknown

- **Observed:** entries max 214, records max 9 over 27,544 frames; 97.8 % of frame draws one-piece; no
  multi-piece frame drawn mirrored; MAME's record flip mirrors pieces in place; vblank cell compare 9.5-16 K a
  frame -> 288 with the queue; frames identical (host 146/146, MAME snapshots byte-equal).
- **Inferred:** prebuilt sublists, a global-scroll camera and dropping the clip do not pay for this art and these
  scene sizes; the remaining draw cost is per-instance logic (scan 42 K, sort 18 K, Draw events in "list"), not
  video hardware use. On jtcps3 (~3.4x MAME for memory-bound code) the saving is ~45 K clocks a step pair.
- **Unknown:** jtcps3 behaviour for wholly off-screen sprites and record flip (RTL not available); the
  prototype on jtcps3; p5_caveman's remaining host-vs-model difference.
