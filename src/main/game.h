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

/* The capture of the last cabinet game (docs/ARCADE.md section 7): everything a host replay needs from game_begin
   (tests/game/host.c HOST_REPLAY, scripts/replay.sh). Written by game_begin / game_step for cabinet games only
   (game_cfg.route NULL); nothing in play reads it. The SH-2 builds keep it in sprite RAM outside the areas main_boot
   clears (CAPTURE_SECTION, tests/game/sprbss.ld), so it survives the settings screen's restart; the next game_begin
   replaces it. The settings screen's GAME CAPTURE shows it (src/shell, game_capture_size / game_capture_byte).
   Blob (big-endian): the CAP_HDR words, ent[0 .. nent), chk[0 .. nchk) (16 bits each), CRC-32 of the bytes before.
   ent: a run of steps with the same controls: bits 0-9 struct shell_input down's bits 0-9, bit 10 KEY_PAY, bits
   11-15 the run's steps - 1 (1..32). chk: every CAP_CHK_EVERY steps a 16-bit hash of the play state (capture_hash:
   the RNG, the player's position, life, money, room, view, instance count). */
#define CAP_MAGIC     0x53504b43u      /* "SPKC" */
#define CAP_VERSION   1u
#define CAP_ENT_MAX   7168
#define CAP_CHK_MAX   1024
#define CAP_CHK_EVERY 64
enum {                                 /* header words */
    CAP_W_MAGIC, CAP_W_VERSION, CAP_W_FLAGS, CAP_W_REV, CAP_W_SEED, CAP_W_STEPS, CAP_W_NENT, CAP_W_NCHK,
    CAP_W_LEVEL, CAP_W_PLIFE, CAP_W_MONEY, CAP_W_ROOM, CAP_W_DEAD, CAP_W_END_ROOM, CAP_HDR
};
#define CAP_F_TOGGLE_RUN 0x01u         /* settings RUN BUTTON TOGGLE (play_toggle_run_on) */
#define CAP_F_SMOOTH     0x02u         /* SMOOTH MOTION (drawing only) */
#define CAP_F_GOD        0x04u         /* INVINCIBLE (dev builds) */
#define CAP_F_DEV        0x08u         /* a SHELL_DEV build */
#define CAP_F_OVER       0x10u         /* the game ended (game_step returned nonzero) */
#define CAP_F_TRUNC      0x20u         /* ent or chk full: steps after CAP_W_STEPS are not recorded */
#define CAP_F_LOSSY      0x40u         /* controls outside bits 0-9 and KEY_PAY were held (not recorded) */
#define CAP_F_DIRTY      0x80u         /* the build's tree differed from CAP_W_REV's commit */
struct capture {
    uint32_t h[CAP_HDR];
    uint16_t ent[CAP_ENT_MAX];
    uint16_t chk[CAP_CHK_MAX];
};
extern struct capture capture;
uint16_t capture_hash(void);
void game_probe(uint32_t *o);           /* tests: 6 words of play state (game.c) */

/* platform hooks (weak defaults in main.c; tests/game overrides them) */
void main_inputs(uint32_t *pad0, uint32_t *pad1, uint32_t *lines);   /* the frame's pads and system lines */
void main_frame_done(void);      /* after each frame's VBlank work (list sent) */
void main_draw_begin(void);      /* around game_draw (cost measurement) */
void main_draw_end(void);
void main_vblank_begin(void);
void main_vblank_end(void);
#endif
