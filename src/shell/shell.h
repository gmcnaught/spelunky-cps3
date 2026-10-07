/* The arcade shell: the main loop around the game (docs/ARCADE.md).
 *
 * Timing (PLAN §1): HD runs at room_speed 30; the CPS3 displays 59.6 frames a second. One game step every second
 * frame: on a step frame the shell reads that step's controls (input_step), runs the game's step and has the game
 * build its display list; on the frame between it only samples the controls and coins, and the PPU's sprite list
 * (sprite RAM, unchanged) is sent again by cps3v_vblank. A step that overruns its two frames delays the next one
 * (the game slows down; no step is skipped, so play stays step-exact).
 *
 * Flow: ATTRACT (the game's intro / title / scores rooms run without controls; the cabinet's Start begins a game
 * only with a credit or in free play) -> PLAY (until the game reports its end) -> ATTRACT. The test switch, or Coin +
 * B2 held for a second, opens the settings screen at any time (free play, coins per credit, run button hold /
 * toggle, smooth motion, clear high scores; DEV=1 builds: INVINCIBLE; the last game's capture as pages of code
 * cells, docs/ARCADE.md section 7); leaving it stores the settings and restarts the program. */
#ifndef SHELL_H
#define SHELL_H
#include <stdint.h>
#include "input.h"
#include "credit.h"
#include "hiscore.h"

/* stops the game's VBlank interrupt work before the shell takes the video (src/main main.c; a weak no-op in shell.c
   for the shell's own tests) */
void shell_video_stop(void);

enum shell_mode { SHELL_ATTRACT, SHELL_PLAY };

struct shell {
    enum shell_mode mode;
    enum input_start_mode start_mode;   /* PLAY: Start as pay (default) or as start; the game may set it */
    uint8_t panel;              /* 0 / 1: the control panel of the player who pressed Start */
    uint32_t frame, steps;      /* frames run, game steps run */
    uint32_t entropy;           /* every frame's pads and lines mixed in (shell_seed) */
    struct input_state in;
    struct shell_input last;    /* the last step's controls */
    struct credit_state cr;
    struct settings st;
    struct hiscores hs;
    struct hs_globals g;
};
extern struct shell SH;

/* an RNG seed for a game or an attract intro (HD's randomize()): the input history's timing (coins, Start) */
static inline uint32_t shell_seed(void) { return SH.entropy ^ (SH.entropy >> 15); }

/* the game, implemented by the play runtime (weak no-op defaults in shell.c) */
void game_boot(void);                                   /* once, after hs_boot (SH.g, SH.hs read) */
void game_attract_step(void);                           /* intro / title / scores rooms, no controls */
void game_begin(void);                                  /* a credit was taken: start a game */
int game_step(const struct shell_input *in);            /* one play step; nonzero: the game is over */
void game_draw(void);                                   /* the step's display list, between cps3v_begin / end */
uint32_t game_capture_size(void);                       /* the last game's capture (src/main game.h): bytes, 0 none */
uint8_t game_capture_byte(uint32_t k);                  /* its byte k (after game_capture_size) */

void shell_init(void);
/* one frame after VBlank: pads (cps3_pad layout), system lines (CR_* coin / service, SHELL_TEST); 1 on a step
   frame. shell_run calls it; tests may drive it directly */
#define SHELL_TEST 0x100u
int shell_frame(uint32_t pad0, uint32_t pad1, uint32_t lines);
/* the main loop; never returns */
void shell_run(void);
#endif
