/* Coins, credits and start (credit.h) */
#include "credit.h"

void credit_reset(struct credit_state *c)
{
    c->credits = 0;
    c->coins = 0;
    c->prev = 0xff;         /* lines held at power-on are not coins */
}

void credit_frame(struct credit_state *c, const struct settings *st, uint32_t lines)
{
    uint8_t now = (uint8_t)(lines & (CR_COIN1 | CR_COIN2 | CR_SERVICE));
    uint8_t up = now & (uint8_t)~c->prev;
    c->prev = now;
    uint8_t cpc = st->coins_per_credit ? st->coins_per_credit : 1;
    for (uint8_t b = CR_COIN1; b <= CR_COIN2; b <<= 1)
        if (up & b) {
            if (c->credits >= CREDITS_MAX)
                continue;
            if (++c->coins >= cpc) {
                c->coins = 0;
                c->credits++;
            }
        }
    if ((up & CR_SERVICE) && c->credits < CREDITS_MAX)
        c->credits++;
}

int credit_can_start(const struct credit_state *c, const struct settings *st)
{
    return st->free_play || c->credits > 0;
}

void credit_take(struct credit_state *c, const struct settings *st)
{
    if (!st->free_play && c->credits > 0)
        c->credits--;
}
