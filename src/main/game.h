/* The game program (src/main): the arcade shell (src/shell) around the play loop (src/game) and its display
 * (src/draw), HUD (src/hud) and sound (src/snd).
 *
 *   shell_frame -> game_begin: a new game (gen_new_game, the level generated: play_level_start)
 *               -> game_step:  one play_step with the step's keys (the cabinet's, or a route's: game_cfg.route)
 *               -> game_draw:  draw_frame (display list, tilemaps prepared)
 *   VBlank      -> draw_vblank (tilemap scrolls / cells), cps3v_vblank (the list), snd_frame
 *
 * The play loop runs a step in about 10 M SH-2 clocks today (budget 0.84 M): a step takes many frames and the shell
 * runs the next one at the following step frame (no frame pacing beyond that). */
#ifndef GAME_H
#define GAME_H
#include <stdint.h>

struct game_cfg {
    const uint16_t *route;       /* key masks per step (tools/tracer.py KEYS bits); NULL: the cabinet's controls */
    int32_t nroute;              /* route steps; then `tail` steps without keys, then the game ends */
    int32_t tail;
    uint32_t seed;               /* RNG seed (route: the trace's; cabinet: 0 = shell_seed()) */
    int32_t level, money, enemies;   /* starting level, money, enemies kept (P4 routes: 0) */
    /* a route's header lines, as test/host/playhost.c's options (tools/tracer.py TRACE_NODARK / TRACE_GLOBALS /
       TRACE_ROOM): */
    int32_t nodark;              /* >= 0: global.noDarkLevel (-1: scrClearGlobals' value) */
    int32_t room;                /* the first room: -1 the level's own (the cabinet); routes 0 rLevel (default), 1 rLevel2,
                                    2 rLevel3 (a lake), 3 rOlmec (gen_room_for_level), 23 rEnd (the ending: the level
                                    generated, then rEnd at the first step) */
    const char *globals;         /* "name=value,..." ("~" a space), or NULL */
    int32_t scores;              /* a route: 1 stores scores in the EEPROM as the cabinet does (tests); 0 leaves it */
};
extern struct game_cfg game_cfg;

/* the record counter, as test/host/playhost.c and tools/tracer.py count them (phase 0 at a room's first Begin
   Step, phase 1 at oGamepad's End Step) */
extern int32_t game_rec;         /* records written so far */
extern int32_t game_rec1;        /* the last phase-1 record (-1 none): the frame drawn after the step shows it */
extern int32_t game_steps;       /* route steps used */
extern uint8_t game_over;        /* the route ended, or the play loop left the rooms it models */
extern int32_t game_end_room;    /* the room the game left for (R_rHighscores: the attract cycle starts there), -1 */

/* platform hooks (weak defaults in main.c; tests/game overrides them) */
void main_inputs(uint32_t *pad0, uint32_t *pad1, uint32_t *lines);   /* the frame's pads and system lines */
void main_frame_done(void);      /* after each frame's VBlank work (list sent) */
void main_draw_begin(void);      /* around game_draw (cost measurement) */
void main_draw_end(void);
void main_vblank_begin(void);
void main_vblank_end(void);
#endif
