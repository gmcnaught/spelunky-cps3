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
  p8_boot (attract) x7, p8_scores x5; c_swamp_drain rec 240 / 300 differ 53 / 172 px **in the baseline too**
  (same snapshots). p7_dark differs from the model by 45-79 K px with the HEAD code as well in this harness
  (driver without `scripts/game_check.sh`'s git snapshot; cause not investigated).
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
+47 K to +151 K on that one frame. If that matters, the queue can hold the off-screen cells and write them over
the following frames.

## Observed / Inferred / Unknown

- **Observed:** entries max 214, records max 9 over 27,544 frames; 97.8 % of frame draws one-piece; no
  multi-piece frame drawn mirrored; MAME's record flip mirrors pieces in place; vblank cell compare 9.5-16 K a
  frame -> 288 with the queue; frames identical (host 146/146, MAME snapshots byte-equal).
- **Inferred:** prebuilt sublists, a global-scroll camera and dropping the clip do not pay for this art and these
  scene sizes; the remaining draw cost is per-instance logic (scan 42 K, sort 18 K, Draw events in "list"), not
  video hardware use. On jtcps3 (~3.4x MAME for memory-bound code) the saving is ~45 K clocks a step pair.
- **Unknown:** jtcps3 behaviour for wholly off-screen sprites and record flip (RTL not available); the
  prototype on jtcps3; c_temple_olmec on the host stats build; p7_dark in this harness.
