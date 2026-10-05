/* High scores and settings in the EEPROM (hiscore.h): HD 1.2.2's spelunky.ini [highscore] rules */
#include "hiscore.h"

#define SET_FREE   0x1u
#define SET_TRUN   0x2u
#define SET_CPC_AT 8

static uint32_t check_of(const uint32_t *w)
{
    uint32_t s = 0;
    for (int k = 0; k < HS_EE_WORDS - 1; k++)
        s += w[k];
    return s ^ 0x5a5a5a5au;
}

static uint32_t settings_word(const struct settings *st)
{
    return (st->free_play ? SET_FREE : 0) | (st->toggle_run ? SET_TRUN : 0) |
           (uint32_t)(st->coins_per_credit & 15) << SET_CPC_AT;
}

void hs_write(const struct hiscores *hs, const struct settings *st)
{
    uint32_t w[HS_EE_WORDS];
    w[0] = HS_MAGIC;
    for (int k = 1; k <= 10; k++)
        w[k] = (uint32_t)hs->value[k];
    w[11] = settings_word(st);
    w[12] = check_of(w);
    for (int k = 0; k < HS_EE_WORDS; k++)
        if (shell_ee_read(HS_EE_AT + k) != w[k])   /* fewer EEPROM writes */
            shell_ee_write(HS_EE_AT + k, w[k]);
}

/* scrResetHighscores */
static void reset_block(struct hiscores *hs)
{
    for (int k = 1; k <= 10; k++)
        hs->value[k] = 0;
    hs->value[HS_TUNNEL1] = TUNNEL1_MAX + 1;
    hs->value[HS_TUNNEL2] = TUNNEL2_MAX + 1;
}

void hs_boot(struct hiscores *hs, struct settings *st, struct hs_globals *g)
{
    uint32_t w[HS_EE_WORDS];
    for (int k = 0; k < HS_EE_WORDS; k++)
        w[k] = shell_ee_read(HS_EE_AT + k);
    int ok = w[0] == HS_MAGIC && w[12] == check_of(w);
    st->free_play = 0;
    st->coins_per_credit = 1;
    st->toggle_run = 0;
    hs->value[0] = 0;
    for (int k = 1; k <= 10; k++)
        hs->value[k] = ok ? (int32_t)w[k] : 0;
    if (ok) {
        st->free_play = (w[11] & SET_FREE) != 0;
        st->toggle_run = (w[11] & SET_TRUN) != 0;
        st->coins_per_credit = (uint8_t)(w[11] >> SET_CPC_AT & 15);
        if (st->coins_per_credit == 0)
            st->coins_per_credit = 1;
    }
    /* oGlobals/Create_0.gml:38-39: ini_read_real("highscore", "value8", 10001), ("value9", 20001) */
    g->tunnel1 = ok ? hs->value[HS_TUNNEL1] : 10001;
    g->tunnel2 = ok ? hs->value[HS_TUNNEL2] : 20001;
    g->first_time = 0;
    /* :57-71 every value1..9 <= 0 (a missing key reads 0): scrResetHighscores, firstTime */
    int all = 1;
    for (int k = 1; k <= 9; k++)
        if (hs->value[k] > 0)
            all = 0;
    if (all) {
        reset_block(hs);
        hs_write(hs, st);
        g->first_time = 1;
    }
}

/* GML floor(a / b) for a >= 0 */
static int32_t fdiv(int32_t a, int32_t b) { return a / b; }

uint32_t hs_update(struct hiscores *hs, const struct settings *st, struct hs_run *r, enum hs_type type)
{
    uint32_t f = 0;
    if (!r->keep_score)
        return 0;
    int32_t *v = hs->value;
    int32_t tMoney = v[HS_MONEY], tTime = v[HS_TIME], tKills = v[HS_KILLS], tSaves = v[HS_SAVES];
    int32_t tPlays = v[HS_PLAYS], tWins = v[HS_WINS], tDeaths = v[HS_DEATHS];
    int32_t tMini1 = fdiv(v[HS_MINI], 10000);
    int32_t tMini2 = fdiv(v[HS_MINI] - tMini1 * 10000, 100);
    int32_t tMini3 = v[HS_MINI] - tMini1 * 10000 - tMini2 * 100;
    if (r->money > tMoney) {
        v[HS_MONEY] = r->money > 0 ? r->money : 0;
        f |= HS_NEW_MONEY;
    } else
        v[HS_MONEY] = tMoney > 0 ? tMoney : 0;
    r->time = r->time >= 0 ? r->time / 1000 : -((-r->time + 999) / 1000);   /* floor */
    if (type == HS_WIN && (r->time < tTime || tTime == 0) && !r->used_shortcut) {
        v[HS_TIME] = r->time;
        f |= HS_NEW_TIME;
    } else
        v[HS_TIME] = tTime;
    if (r->kills > tKills) {
        v[HS_KILLS] = r->kills;
        f |= HS_NEW_KILLS;
    } else
        v[HS_KILLS] = tKills;
    if (r->damsels > tSaves) {
        v[HS_SAVES] = r->damsels;
        f |= HS_NEW_SAVES;
    } else
        v[HS_SAVES] = tSaves;
    v[HS_PLAYS] = type == HS_MINIGAME ? tPlays : tPlays + 1;
    if (type == HS_WIN) {
        v[HS_WINS] = tWins + 1;
        v[HS_DEATHS] = tDeaths;
    } else if (type == HS_MINIGAME) {
        v[HS_WINS] = tWins;
        v[HS_DEATHS] = tDeaths;
    } else {
        v[HS_WINS] = tWins;
        v[HS_DEATHS] = tDeaths + 1;
    }
    v[HS_TUNNEL1] = r->tunnel1;
    v[HS_TUNNEL2] = r->tunnel2;
    if (r->mini1 > tMini1) {
        tMini1 = r->mini1;
        f |= HS_NEW_MINI1;
    }
    if (r->mini2 > tMini2) {
        tMini2 = r->mini2;
        f |= HS_NEW_MINI2;
    }
    if (r->mini3 > tMini3) {
        tMini3 = r->mini3;
        f |= HS_NEW_MINI3;
    }
    v[HS_MINI] = tMini1 * 10000 + tMini2 * 100 + tMini3;
    hs_write(hs, st);
    return f;
}
