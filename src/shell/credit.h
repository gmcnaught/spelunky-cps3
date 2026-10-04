/* Coins, credits and start (docs/ARCADE.md "Credits"). HD has no counterpart: the arcade shell adds it around the
 * title and the game (PLAN P8). */
#ifndef SHELL_CREDIT_H
#define SHELL_CREDIT_H
#include <stdint.h>
#include "hiscore.h"

#define CREDITS_MAX 9

struct credit_state {
    uint8_t credits;
    uint8_t coins;          /* coins toward the next credit */
    uint8_t prev;           /* coin / service lines at the previous frame (CR_*) */
};
#define CR_COIN1   0x1u
#define CR_COIN2   0x2u
#define CR_SERVICE 0x4u     /* the service button: one credit, as CPS3 games do */

void credit_reset(struct credit_state *c);
/* once a frame with the lines held (CR_*): a line's rising edge counts one coin (or one credit for service);
   credits stop at CREDITS_MAX (later coins are still counted toward nothing, as most boards lock out) */
void credit_frame(struct credit_state *c, const struct settings *st, uint32_t lines);
/* a game can start: free play or a credit */
int credit_can_start(const struct credit_state *c, const struct settings *st);
/* takes the game's credit (none in free play) */
void credit_take(struct credit_state *c, const struct settings *st);
#endif
