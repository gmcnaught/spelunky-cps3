/* The arcade shell's main loop (shell.h) on the CPS3 SDK */
#include "cps3.h"
#include "shell.h"

struct shell SH;

/* the game's hooks: no-ops until the play runtime links its own */
__attribute__((weak)) void game_boot(void) {}
__attribute__((weak)) void game_attract_step(void) {}
__attribute__((weak)) void game_begin(void) {}
__attribute__((weak)) int game_step(const struct shell_input *in) { (void)in; return 1; }
__attribute__((weak)) void game_draw(void) {}

uint32_t shell_ee_read(int word) { return cps3_ee_read(word); }
void shell_ee_write(int word, uint32_t v) { cps3_ee_write(word, v); }

/* ---- text layer (attract credit line, settings screen) ---- */
static void put_num(int col, int row, uint32_t v, int width)
{
    char b[12];
    int n = 0;
    do {
        b[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < 11);
    char s[12];
    int k = 0;
    while (k < width - n && k < 11)
        s[k++] = ' ';
    while (n)
        s[k++] = b[--n];
    s[k] = 0;
    cps3v_text(col, row, s);
}

#define CREDIT_ROW 27
static uint8_t credit_shown = 0xff;             /* what the credit line shows (0xfe: free play, 0xfd: blank) */

static void credit_line(void)
{
    uint8_t want = SH.mode != SHELL_ATTRACT ? 0xfd : SH.st.free_play ? 0xfe : SH.cr.credits;
    if (want == credit_shown)
        return;
    credit_shown = want;
    cps3v_text(36, CREDIT_ROW, "          ");
    if (want == 0xfe)
        cps3v_text(38, CREDIT_ROW, "FREE PLAY");
    else if (want != 0xfd) {
        cps3v_text(37, CREDIT_ROW, "CREDIT");
        put_num(44, CREDIT_ROW, want, 2);
    }
}

/* ---- settings screen (test switch) ---- */
static void line(int row, int sel, const char *label)
{
    cps3v_text(8, row, sel ? "+" : " ");          /* the SDK font has no ">" */
    cps3v_text(10, row, label);
}

static void settings_run(void)
{
    cps3s_init();                                /* sound off */
    cps3v_begin();                               /* no sprites */
    cps3v_end();
    for (int t = 0; t < 4; t++)
        cps3v_tilemap(t, 0, 0, CPS3V_MAP_UNIT(0), 0);
    for (int r = 0; r < 28; r++)
        cps3v_text(0, r, "                                                ");
    struct settings st;
    st.free_play = SH.st.free_play;
    st.coins_per_credit = SH.st.coins_per_credit;
    st.toggle_run = SH.st.toggle_run;
    int sel = 0, clear = 0;
    uint32_t prev = 0xffffffffu, prev_sys = 0xffffffffu;
    cps3v_text(16, 3, "SPELUNKY SETTINGS");
    cps3v_text(8, 20, "UP / DOWN    CHOOSE");
    cps3v_text(8, 22, "B1 / RIGHT   CHANGE");
    cps3v_text(8, 24, "TEST         SAVE AND EXIT");
    for (;;) {
        line(7, sel == 0, "FREE PLAY");
        cps3v_text(32, 7, st.free_play ? "ON " : "OFF");
        line(9, sel == 1, "COINS PER CREDIT");
        put_num(32, 9, st.coins_per_credit, 1);
        line(11, sel == 2, "RUN BUTTON");
        cps3v_text(32, 11, st.toggle_run ? "TOGGLE" : "HOLD  ");
        line(13, sel == 3, "CLEAR HIGH SCORES");
        cps3v_text(32, 13, clear ? "YES" : "NO ");
        line(16, sel == 4, "SAVE AND EXIT");
        cps3v_wait_vblank();
        cps3v_vblank();
        uint32_t p = cps3_pad(0) | cps3_pad(1), sys = cps3_system();
        uint32_t press = p & ~prev, spress = sys & ~prev_sys;
        prev = p;
        prev_sys = sys;
        if ((press & CPS3_UP) && sel > 0)
            sel--;
        if ((press & CPS3_DOWN) && sel < 4)
            sel++;
        int leave = (spress & CPS3_TEST) != 0;
        int step = (press & (CPS3_B1 | CPS3_RIGHT)) ? 1 : (press & CPS3_LEFT) ? -1 : 0;
        if (step) {
            if (sel == 0)
                st.free_play = !st.free_play;
            else if (sel == 1)
                st.coins_per_credit = (uint8_t)((st.coins_per_credit - 1 + step + 9) % 9 + 1);
            else if (sel == 2)
                st.toggle_run = !st.toggle_run;
            else if (sel == 3)
                clear = !clear;
            else if (press & CPS3_B1)
                leave = 1;
        }
        if (leave)
            break;
    }
    if (clear) {                                 /* the next boot finds no block: HD's first start (hs_boot) */
        for (int k = 0; k < HS_EE_WORDS; k++)
            shell_ee_write(HS_EE_AT + k, 0);
        if (st.free_play || st.coins_per_credit != 1 || st.toggle_run) {   /* keep the settings: a reset block */
            struct hiscores hs;
            hs.value[0] = 0;
            for (int k = 1; k <= 10; k++)
                hs.value[k] = 0;
            hs.value[HS_TUNNEL1] = TUNNEL1_MAX + 1;
            hs.value[HS_TUNNEL2] = TUNNEL2_MAX + 1;
            hs_write(&hs, &st);
        }
    } else
        hs_write(&SH.hs, &st);
    __asm__ volatile("ldc %0, sr\n\tmov.l 1f, r0\n\tjmp @r0\n\tnop\n\t.align 2\n1:\t.long start" : : "r"(0xf0) : "r0");
}

/* ---- the loop ---- */
void shell_init(void)
{
    SH.mode = SHELL_ATTRACT;
    SH.start_mode = INPUT_START_PAY;
    SH.panel = 0;
    SH.frame = SH.steps = 0;
    hs_boot(&SH.hs, &SH.st, &SH.g);
    credit_reset(&SH.cr);
    input_reset(&SH.in, 0xffff);                 /* nothing held at power-on counts as a press */
    SH.last.down = SH.last.pressed = SH.last.released = 0;
    credit_shown = 0xff;
    game_boot();
}

static uint32_t start_prev = 0x3;                /* Start 1 / 2 at the previous frame (held at boot: no press) */
static uint32_t test_prev = SHELL_TEST;          /* test switch held at boot (or still held on leaving): no press */

/* the settings screen from a pad (MiSTer's stock jtcps3 has no OSD test switch; the test line is keyboard F2 only):
   Coin + B2 on either panel held together for SETTINGS_HOLD frames, as JTFRAME's Pocket combo for test. The Coin
   counts a credit as usual; leaving the screen restarts the program, which clears the credits. */
#define SETTINGS_COMBO (CPS3_COIN | CPS3_B2)
#define SETTINGS_HOLD  60
static uint8_t combo_armed;                      /* 0 until a frame without the combo: held at boot is no press */
static uint8_t combo_n;

int shell_frame(uint32_t pad0, uint32_t pad1, uint32_t lines)
{
    if (lines & ~test_prev & SHELL_TEST)
        settings_run();
    test_prev = lines & SHELL_TEST;
    if ((pad0 & SETTINGS_COMBO) != SETTINGS_COMBO && (pad1 & SETTINGS_COMBO) != SETTINGS_COMBO) {
        combo_armed = 1;
        combo_n = 0;
    } else if (combo_armed && ++combo_n == SETTINGS_HOLD)
        settings_run();
    credit_frame(&SH.cr, &SH.st, lines);
    uint32_t starts = (pad0 & CPS3_START ? 1u : 0) | (pad1 & CPS3_START ? 2u : 0);
    uint32_t spress = starts & ~start_prev;
    start_prev = starts;
    if (SH.mode == SHELL_ATTRACT && spress && credit_can_start(&SH.cr, &SH.st)) {
        credit_take(&SH.cr, &SH.st);
        SH.panel = (spress & 1) ? 0 : 1;
        SH.mode = SHELL_PLAY;
        SH.start_mode = INPUT_START_PAY;
        /* the Start that began the game is held: not a press for the game */
        input_reset(&SH.in, input_keys(SH.panel ? pad1 : pad0, &input_default_map, SH.start_mode));
        game_begin();
    }
    uint32_t pad = SH.panel ? pad1 : pad0;
    input_frame(&SH.in, SH.mode == SHELL_PLAY ? input_keys(pad, &input_default_map, SH.start_mode) : 0);
    int step = (SH.frame++ & 1) == 0;
    if (step) {
        input_step(&SH.in, &SH.last);
        SH.steps++;
        if (SH.mode == SHELL_PLAY) {
            if (game_step(&SH.last)) {
                SH.mode = SHELL_ATTRACT;
                input_reset(&SH.in, 0);
            }
        } else
            game_attract_step();
        cps3v_begin();
        game_draw();
        cps3v_end();
    }
    credit_line();
    return step;
}

static uint32_t sys_lines(uint32_t pad0, uint32_t pad1)
{
    uint32_t s = cps3_system();
    return (pad0 & CPS3_COIN ? CR_COIN1 : 0) | (pad1 & CPS3_COIN ? CR_COIN2 : 0) |
           (s & CPS3_SERVICE ? CR_SERVICE : 0) | (s & CPS3_TEST ? SHELL_TEST : 0);
}

void shell_run(void)
{
    shell_init();
    for (;;) {
        cps3v_wait_vblank();
        cps3v_vblank();
        uint32_t p0 = cps3_pad(0), p1 = cps3_pad(1);
        shell_frame(p0, p1, sys_lines(p0, p1));
    }
}
