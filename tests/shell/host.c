/* Host checks of src/shell's input, credit and high-score logic (no CPS3): cc -I src/shell tests/shell/host.c
   src/shell/{input,credit,hiscore}.c; tests/shell/Makefile's host target. Exit 0 when every check passes. */
#include <stdio.h>
#include <string.h>
#include "input.h"
#include "credit.h"
#include "hiscore.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static uint32_t ee[32];
static int ee_writes;
uint32_t shell_ee_read(int w) { return ee[w]; }
void shell_ee_write(int w, uint32_t v) { ee[w] = v; ee_writes++; }

/* CPS3 pad bits (cps3io.h) */
enum { UP = 1, DOWN = 2, LEFT = 4, RIGHT = 8, B1 = 0x10, B2 = 0x20, B3 = 0x40, B4 = 0x80, B5 = 0x100, B6 = 0x200,
       START = 0x400, COIN = 0x800 };

static void test_map(void)
{
    const struct input_map *m = &input_default_map;
    CHECK(input_keys(RIGHT, m, INPUT_START_MENU) == KEY_RIGHT);
    CHECK(input_keys(LEFT | UP, m, INPUT_START_MENU) == (KEY_LEFT | KEY_UP));
    CHECK(input_keys(DOWN, m, INPUT_START_MENU) == KEY_DOWN);
    CHECK(input_keys(B1, m, INPUT_START_MENU) == KEY_JUMP);
    CHECK(input_keys(B2, m, INPUT_START_MENU) == KEY_ATTACK);
    CHECK(input_keys(B3, m, INPUT_START_MENU) == KEY_ITEM);
    CHECK(input_keys(B4, m, INPUT_START_MENU) == KEY_RUN);
    CHECK(input_keys(B5, m, INPUT_START_MENU) == KEY_BOMB);
    CHECK(input_keys(B6, m, INPUT_START_MENU) == KEY_ROPE);
    CHECK(input_keys(START, m, INPUT_START_MENU) == KEY_START);
    CHECK(input_keys(START, m, INPUT_START_PAY) == KEY_PAY);
    CHECK(input_keys(COIN, m, INPUT_START_PAY) == 0);
    /* tools/tracer.py KEYS bits */
    CHECK(KEY_RIGHT == 1 && KEY_LEFT == 2 && KEY_UP == 4 && KEY_DOWN == 8 && KEY_JUMP == 16 && KEY_ATTACK == 32 &&
          KEY_ITEM == 64 && KEY_RUN == 128 && KEY_BOMB == 256 && KEY_ROPE == 512 && KEY_FLARE == 1024 &&
          KEY_PAY == 2048 && KEY_START == 4096);
}

/* oGamepad's rule per step, with frames sampled twice a step */
static void test_steps(void)
{
    struct input_state s;
    struct shell_input r;
    input_reset(&s, 0);
    /* held over a step pair: pressed once, down twice, released after */
    input_frame(&s, KEY_JUMP);                       /* step frame */
    input_step(&s, &r);
    CHECK(r.down == KEY_JUMP && r.pressed == KEY_JUMP && r.released == 0);
    input_frame(&s, KEY_JUMP);                       /* off frame */
    input_frame(&s, KEY_JUMP);
    input_step(&s, &r);
    CHECK(r.down == KEY_JUMP && r.pressed == 0 && r.released == 0);
    input_frame(&s, 0);
    input_frame(&s, 0);
    input_step(&s, &r);
    CHECK(r.down == 0 && r.pressed == 0 && r.released == KEY_JUMP);
    /* a one-frame tap on the off frame: held for one step, then released */
    input_frame(&s, KEY_BOMB);
    input_frame(&s, 0);
    input_step(&s, &r);
    CHECK(r.down == KEY_BOMB && r.pressed == KEY_BOMB);
    input_frame(&s, 0);
    input_frame(&s, 0);
    input_step(&s, &r);
    CHECK(r.down == 0 && r.released == KEY_BOMB);
    /* held at reset: no press */
    input_reset(&s, KEY_PAY);
    input_frame(&s, KEY_PAY);
    input_step(&s, &r);
    CHECK(r.down == KEY_PAY && r.pressed == 0);
}

static void test_credits(void)
{
    struct credit_state c;
    struct settings st = { 0, 1, 0, 1 };
    credit_reset(&c);
    credit_frame(&c, &st, CR_COIN1);                 /* held at power-on: not a coin */
    CHECK(c.credits == 0);
    credit_frame(&c, &st, 0);
    credit_frame(&c, &st, CR_COIN1);
    CHECK(c.credits == 1);
    credit_frame(&c, &st, CR_COIN1);                 /* still held */
    CHECK(c.credits == 1);
    credit_frame(&c, &st, CR_COIN2 | CR_SERVICE);
    CHECK(c.credits == 3);
    CHECK(credit_can_start(&c, &st));
    credit_take(&c, &st);
    CHECK(c.credits == 2);
    st.coins_per_credit = 2;
    credit_frame(&c, &st, 0);
    credit_frame(&c, &st, CR_COIN1);
    CHECK(c.credits == 2 && c.coins == 1);
    credit_frame(&c, &st, 0);
    credit_frame(&c, &st, CR_COIN1);
    CHECK(c.credits == 3 && c.coins == 0);
    for (int k = 0; k < 40; k++)
        credit_frame(&c, &st, k & 1 ? CR_COIN1 : 0);
    CHECK(c.credits == CREDITS_MAX);
    struct credit_state z;
    credit_reset(&z);
    struct settings fp = { 1, 1, 0, 1 };
    CHECK(credit_can_start(&z, &fp));
    credit_take(&z, &fp);
    CHECK(z.credits == 0);
    CHECK(!credit_can_start(&z, &st));
}

static void test_hiscores(void)
{
    struct hiscores hs;
    struct settings st;
    struct hs_globals g;
    memset(ee, 0, sizeof ee);
    /* blank EEPROM = no spelunky.ini: tunnel globals 10001 / 20001 (read before the reset), reset block stored */
    hs_boot(&hs, &st, &g);
    CHECK(g.tunnel1 == 10001 && g.tunnel2 == 20001 && g.first_time == 1);
    CHECK(hs.value[HS_TUNNEL1] == TUNNEL1_MAX + 1 && hs.value[HS_TUNNEL2] == TUNNEL2_MAX + 1);
    CHECK(ee[HS_EE_AT] == HS_MAGIC && ee[HS_EE_AT + 8] == TUNNEL1_MAX + 1 && st.free_play == 0 &&
          st.coins_per_credit == 1);
    /* second boot: the stored block */
    hs_boot(&hs, &st, &g);
    CHECK(g.tunnel1 == TUNNEL1_MAX + 1 && g.tunnel2 == TUNNEL2_MAX + 1 && g.first_time == 0);
    /* a death with money 1500, 90000 time units, 3 kills, 1 save */
    struct hs_run r = { .money = 1500, .time = 90000, .kills = 3, .damsels = 1, .tunnel1 = 10001, .tunnel2 = 20001,
                        .keep_score = 1 };
    uint32_t f = hs_update(&hs, &st, &r, HS_DEATH);
    CHECK(f == (HS_NEW_MONEY | HS_NEW_KILLS | HS_NEW_SAVES));
    CHECK(r.time == 90);
    CHECK(hs.value[HS_MONEY] == 1500 && hs.value[HS_TIME] == 0 && hs.value[HS_PLAYS] == 1 &&
          hs.value[HS_DEATHS] == 1 && hs.value[HS_WINS] == 0 && hs.value[HS_TUNNEL1] == 10001);
    /* a win: time recorded, wins + 1, deaths kept */
    struct hs_run w = { .money = 900, .time = 600999, .kills = 1, .damsels = 0, .tunnel1 = 5, .tunnel2 = 6,
                        .keep_score = 1 };
    f = hs_update(&hs, &st, &w, HS_WIN);
    CHECK(f == HS_NEW_TIME && hs.value[HS_TIME] == 600 && hs.value[HS_WINS] == 1 && hs.value[HS_DEATHS] == 1 &&
          hs.value[HS_PLAYS] == 2 && hs.value[HS_MONEY] == 1500);
    /* a slower win does not replace the time; a shortcut win never records one */
    struct hs_run w2 = { .time = 700000, .keep_score = 1, .tunnel1 = 5, .tunnel2 = 6 };
    hs_update(&hs, &st, &w2, HS_WIN);
    CHECK(hs.value[HS_TIME] == 600 && hs.value[HS_WINS] == 2);
    struct hs_run w3 = { .time = 1000, .keep_score = 1, .used_shortcut = 1, .tunnel1 = 5, .tunnel2 = 6 };
    CHECK(!(hs_update(&hs, &st, &w3, HS_WIN) & HS_NEW_TIME));
    /* minigames: plays unchanged, packed scores */
    struct hs_run m = { .tunnel1 = 5, .tunnel2 = 6, .mini1 = 12, .mini2 = 3, .mini3 = 45, .keep_score = 1 };
    f = hs_update(&hs, &st, &m, HS_MINIGAME);
    CHECK(f == (HS_NEW_MINI1 | HS_NEW_MINI2 | HS_NEW_MINI3) && hs.value[HS_MINI] == 120345 &&
          hs.value[HS_PLAYS] == 4);
    struct hs_run m2 = { .tunnel1 = 5, .tunnel2 = 6, .mini1 = 11, .mini2 = 4, .mini3 = 0, .keep_score = 1 };
    f = hs_update(&hs, &st, &m2, HS_MINIGAME);
    CHECK(f == HS_NEW_MINI2 && hs.value[HS_MINI] == 120445);
    /* keepScore false: nothing */
    struct hs_run k0 = { .money = 99999, .keep_score = 0 };
    CHECK(hs_update(&hs, &st, &k0, HS_DEATH) == 0 && hs.value[HS_MONEY] == 1500);
    /* settings survive and are read back; a corrupted block reads as no ini */
    st.free_play = 1;
    st.coins_per_credit = 3;
    st.toggle_run = 1;
    hs_write(&hs, &st);
    struct hiscores h2;
    struct settings s2;
    hs_boot(&h2, &s2, &g);
    CHECK(s2.free_play == 1 && s2.coins_per_credit == 3 && s2.toggle_run == 1 && h2.value[HS_MINI] == 120445 &&
          g.tunnel1 == 5);
    CHECK(s2.smooth == 1 && ee[HS_EE_AT + 11] == (0x1u | 0x2u | 3u << 8));   /* smooth on: bit 2 clear */
    st.smooth = 0;
    hs_write(&hs, &st);
    hs_boot(&h2, &s2, &g);
    CHECK(s2.smooth == 0 && s2.toggle_run == 1 && ee[HS_EE_AT + 11] == (0x1u | 0x2u | 0x4u | 3u << 8));
    ee[HS_EE_AT + 3] ^= 1;
    hs_boot(&h2, &s2, &g);
    CHECK(s2.free_play == 0 && s2.toggle_run == 0 && s2.smooth == 1 && g.tunnel1 == 10001 && g.first_time == 1 && h2.value[HS_MONEY] == 0);
    /* an unchanged block is not rewritten */
    ee_writes = 0;
    hs_write(&h2, &s2);
    CHECK(ee_writes == 0);
}

int main(void)
{
    test_map();
    test_steps();
    test_credits();
    test_hiscores();
    printf("%d of %d checks passed\n", checks - fails, checks);
    return fails != 0;
}
