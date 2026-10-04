/* Arcade inputs -> HD's oGamepad fields (input.h, docs/ARCADE.md) */
#include "input.h"

/* CPS3 pad bits (sdk cps3io.h; repeated so this file builds on the host without the SDK) */
#define P_UP     0x0001u
#define P_DOWN   0x0002u
#define P_LEFT   0x0004u
#define P_RIGHT  0x0008u
#define P_B1     0x0010u
#define P_B2     0x0020u
#define P_B3     0x0040u
#define P_B4     0x0080u
#define P_B5     0x0100u
#define P_B6     0x0200u
#define P_START  0x0400u

/* HD's keyboard defaults (scripts/configLoad: Z jump, X attack, C item, Shift run, A bomb, S rope) laid on the
 * CPS3 panel: top row B1 B2 B3 = jump, whip, item (Z X C, in that order on the keyboard); bottom row B4 B5 B6 =
 * run, bomb, rope. Start: KEY_START or KEY_PAY by the start mode (input_keys). */
const struct input_map input_default_map = { {
    P_RIGHT,  /* KEY_RIGHT */
    P_LEFT,   /* KEY_LEFT */
    P_UP,     /* KEY_UP */
    P_DOWN,   /* KEY_DOWN */
    P_B1,     /* KEY_JUMP */
    P_B2,     /* KEY_ATTACK */
    P_B3,     /* KEY_ITEM */
    P_B4,     /* KEY_RUN */
    P_B5,     /* KEY_BOMB */
    P_B6,     /* KEY_ROPE */
    0,        /* KEY_FLARE */
    0,        /* KEY_PAY: Start in INPUT_START_PAY */
    0,        /* KEY_START: Start in INPUT_START_MENU */
} };

uint16_t input_keys(uint32_t pad, const struct input_map *m, enum input_start_mode sm)
{
    uint16_t k = 0;
    for (int i = 0; i < 13; i++)
        if (m->pad[i] && (pad & m->pad[i]))
            k |= (uint16_t)(1u << i);
    if (pad & P_START)
        k |= sm == INPUT_START_PAY ? KEY_PAY : KEY_START;
    return k;
}

void input_reset(struct input_state *s, uint16_t held_now)
{
    s->prev = held_now;
    s->last = held_now;
    s->taps = 0;
}

void input_frame(struct input_state *s, uint16_t held)
{
    s->taps |= held & (uint16_t)~s->last;
    s->last = held;
}

/* oGamepad/Step_0.gml for each key: Released = held before and not now; Pressed = not held before and held now */
void input_step(struct input_state *s, struct shell_input *r)
{
    uint16_t now = s->last | (s->taps & (uint16_t)~s->prev);
    r->down = now;
    r->pressed = now & (uint16_t)~s->prev;
    r->released = s->prev & (uint16_t)~now;
    s->prev = now;
    s->taps = 0;
}
