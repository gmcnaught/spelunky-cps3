/* The play state on the CPS3 display (PLAN.md P3 / P4-P5): what GameMaker's runner draws for a room after a step,
 * in its order, as tests/view and tools/viewlevel.py model it.
 *
 *   - the room background (bgCave, bgTemple in rOlmec / temple levels) on tilemap 0;
 *   - three more tilemaps for the three depths with most 16-px cells at room start: the tile_add layers (gtiles)
 *     and terrain (the oSolid family drawn as one 16 x 16 tile on the 16-px grid). A cell already taken in its
 *     depth's tilemap (older first, tiles before instances) and every other drawable are sprites;
 *   - instances (visible, in view) deepest first, equal depths oldest first (instance id), through the Draw event
 *     that applies to the object (draw_kind: the translated Draw events, draw_self for objects without one);
 *   - the HUD (src/hud, oGame's Draw GUI) over everything.
 * The camera is the play state's view (PW.xview / yview, after the step's follow); the screen shows view lines
 * 8..231 (DRAW_CROP) stretched to 384 by X zoom 0x35.
 *
 * Reads src/game's state (play.h, pint.h, gen.h); writes nothing there. */
#ifndef DRAW_H
#define DRAW_H
#include <stdint.h>

#define DRAW_PAL     1u          /* colour code of tools/hdsprites.py's palette (faded on dark levels) */
#define DRAW_PAL_LIT 4u          /* the same palette, never faded: what is drawn after oLevel's rectangle, the HUD */
#define DRAW_PAL_HUDDARK 5u      /* the HUD palette faded as DRAW_PAL (the price tag before oLevel's rectangle) */
#define DRAW_CROP    8
#define DRAW_ZOOM_X  0x35

struct draw_stats {
    uint32_t frames;             /* draw_frame calls */
    uint32_t entries;            /* sublist entries (sprite pieces, bands) in the last frame */
    uint32_t entries_max;
    uint32_t records;            /* main-list records (groups) in the last frame */
    uint32_t sprites;            /* drawables sorted in the last frame (instances and tile sprites in view) */
    uint32_t cells;              /* tilemap cells written at the last draw_vblank */
    uint32_t dropped;            /* entries not drawn (over DRAW_ENTRIES_MAX) since boot */
    uint32_t todo;               /* instances of untranslated Draw events drawn as draw_self (last frame) */
    uint32_t unsup;              /* instances with a scale / angle / blend / alpha not drawn as GameMaker (last frame) */
    uint32_t noart;              /* drawables whose sprite has no art (last frame) */
    uint32_t room_builds;        /* tilemap rebuilds (room starts) */
    uint32_t prof[5];            /* DRAW_PROFILE builds: FRC ticks of the last frame's parts: set-up and tilemap
                                    copy, instance scan, sort, list, HUD */
};
extern struct draw_stats draw_st;
extern int16_t draw_dark_force;  /* tests: >= 0 draws as a dark level at that alpha byte (a8) */
extern uint8_t draw_hud_on;      /* 1: the HUD is drawn (tests clear it to compare with the runner's
                                    application_surface, which has no GUI) */

/* once at boot, after the art is in character RAM and the colours are loaded */
void draw_boot(void);
/* a new game started (game_begin): the next draw_frame rebuilds the tilemaps */
void draw_new_game(void);
/* the display list for the play state (between cps3v_begin / cps3v_end); tilemap cells and scrolls are prepared
   for draw_vblank */
void draw_frame(void);
/* at the next VBlank, before cps3v_vblank sends the list: tilemap scrolls and the changed cells */
void draw_vblank(void);
/* GML tile_layer_find(depth, x, y) + tile_delete (oExplosion / oBoulder / oMattockHit / oLaser / oOlmecSlam /
   oXocBlock): removes the tile_add tile at that point. src/game does not call it yet (its sites are marked
   "drawing only") */
void draw_tile_delete(int depth, int x, int y);
#endif
