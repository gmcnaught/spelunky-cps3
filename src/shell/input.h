/* Arcade inputs -> HD 1.2.2's input checks (docs/ARCADE.md).
 *
 * HD reads every control through the check* scripts (refs/hd/src/scripts/check*), each of which ORs the keyboard
 * with oGamepad's fields: <key> (held), <key>Pressed (held now, not at the previous step), <key>Released (held at the
 * previous step, not now). oGamepad's Step (objects/oGamepad/Step_0.gml) computes them once per game step. The port
 * keeps one per-step struct with the same three masks; the bits are tools/tracer.py's route bits (KEYS), so a route
 * file, the host runner (src/game: play_step(keys)) and the cabinet give the play code the same 16-bit mask.
 *
 * Not mapped: checkLangPressed (language switch, X3), checkFlarePressed (oPlayer1/Step_0.gml:415: commented out in
 * HD), keyboard F-keys (oDebug, X8; restart F6). The run toggle (X10) is the settings screen's RUN BUTTON (game.c). */
#ifndef SHELL_INPUT_H
#define SHELL_INPUT_H
#include <stdint.h>

/* tools/tracer.py KEYS: route letter, oGamepad field */
#define KEY_RIGHT  0x0001u   /* R  right   checkRight*   */
#define KEY_LEFT   0x0002u   /* L  left    checkLeft*    */
#define KEY_UP     0x0004u   /* U  up      checkUp*      */
#define KEY_DOWN   0x0008u   /* D  down    checkDown*    */
#define KEY_JUMP   0x0010u   /* J  jump    checkJump*    */
#define KEY_ATTACK 0x0020u   /* A  attack  checkAttack*  (whip / use) */
#define KEY_ITEM   0x0040u   /* I  item    checkItemPressed (switch item) */
#define KEY_RUN    0x0080u   /* N  run     checkRun, checkRunPressed */
#define KEY_BOMB   0x0100u   /* B  bomb    checkBombPressed */
#define KEY_ROPE   0x0200u   /* O  rope    checkRopePressed */
#define KEY_FLARE  0x0400u   /* F  flare   checkFlarePressed (unused by HD's play code) */
#define KEY_PAY    0x0800u   /* P  pay     checkPayPressed (buy in a shop) */
#define KEY_START  0x1000u   /* S  start   checkStartPressed (skip, title, pause) */

/* one game step's controls, as oGamepad's fields after its Step */
struct shell_input {
    uint16_t down;       /* <key> */
    uint16_t pressed;    /* <key>Pressed */
    uint16_t released;   /* <key>Released */
};

/* where the cabinet's Start button goes (docs/ARCADE.md "Start and pay"):
 *   INPUT_START_MENU: KEY_START (title, transitions, skip, restart after death)
 *   INPUT_START_PAY:  KEY_PAY (in a level: Start buys; HD's pause is not on the arcade, VARIANTS X9) */
enum input_start_mode { INPUT_START_MENU, INPUT_START_PAY };

/* the button layout: CPS3 pad bit (cps3io.h CPS3_*) for each KEY_* bit, 0 = none */
struct input_map {
    uint16_t pad[13];    /* index k: KEY bit 1 << k */
};
extern const struct input_map input_default_map;

/* raw pad word (cps3_pad layout) -> held KEY_* mask under the map and start mode */
uint16_t input_keys(uint32_t pad, const struct input_map *m, enum input_start_mode sm);

/* per-frame sampling, per-step result. The display runs at 59.6 Hz and the game steps at 30 Hz (every second frame,
 * PLAN §1): input_frame() is called every frame with the held mask; input_step() is called once per game step and
 * fills that step's struct. A key's held state at a step is its state on that step's frame, plus any key that
 * was pressed on a frame in between and let go again before the step (a tap shorter than one step is seen as held
 * for one step, not lost). */
struct input_state {
    uint16_t prev;       /* held at the previous step */
    uint16_t taps;       /* went down on a frame since the previous step */
    uint16_t last;       /* held on the last frame */
};
void input_reset(struct input_state *s, uint16_t held_now);   /* held_now: keys held already are not presses */
void input_frame(struct input_state *s, uint16_t held);
void input_step(struct input_state *s, struct shell_input *r);
#endif
