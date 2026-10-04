/* The HUD's message lines (pmsg.h): scripts/trMessages, showMessages' countdown */
#include "pmsg.h"

struct pmsgs PMSG;

static void set(struct pmsg *m, const char *const *part, int n, uint32_t hl)
{
    int k, len = 0;
    m->yellow[0] = m->yellow[1] = 0;
    if (n == 0) n = 1, hl = 0;                    /* a plain string: drawMessage, white */
    for (k = 0; k < n; k++) {
        const char *s = part[k];
        for (; s && *s; s++) {
            if (len >= PMSG_MAX) break;
            if (hl >> k & 1) m->yellow[len >> 5] |= 1u << (len & 31);
            m->text[len++] = *s;
        }
    }
    m->text[len] = 0;
}

void pmsg_tr(const char *const *m1, int n1, uint32_t hl1, const char *const *m2, int n2, uint32_t hl2, int timer)
{
    set(&PMSG.m1, m1, n1, hl1);
    set(&PMSG.m2, m2, n2, hl2);
    PMSG.timer = (int16_t)timer;
}

void pmsg_str(const char *s1, const char *s2, int timer)
{
    pmsg_tr(&s1, 0, 0, &s2, 0, 0, timer);
}

void pmsg_player(const char *const *m1, int n1, const char *const *m2, int n2, int timer)
{
    set(&PMSG.pm1, m1, n1, 0);
    set(&PMSG.pm2, m2, n2, 0);
    pmsg_player_again(timer);
}

void pmsg_player_str(const char *s1, const char *s2, int timer)
{
    pmsg_player(&s1, 0, &s2, 0, timer);
}

void pmsg_player_again(int timer)
{
    PMSG.m1 = PMSG.pm1;
    PMSG.m2 = PMSG.pm2;
    PMSG.timer = (int16_t)timer;
}

void pmsg_player_reset(void)
{
    PMSG.pm1.text[0] = PMSG.pm2.text[0] = 0;
}

void pmsg_clear(void)
{
    PMSG.m1.text[0] = PMSG.m2.text[0] = 0;
    PMSG.m1.yellow[0] = PMSG.m1.yellow[1] = PMSG.m2.yellow[0] = PMSG.m2.yellow[1] = 0;
}

const char *pmsg_num(int32_t v, char *b)
{
    char t[12];
    int n = 0, k = 0;
    uint32_t u = v < 0 ? 0u - (uint32_t)v : (uint32_t)v;
    do t[n++] = (char)('0' + u % 10u); while ((u /= 10u) != 0);
    if (v < 0) b[k++] = '-';
    while (n) b[k++] = t[--n];
    b[k] = 0;
    return b;
}

void pmsg_frame(void)
{
    if (PMSG.timer > 0) PMSG.timer -= 1;
}
