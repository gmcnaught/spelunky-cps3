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
__attribute__((weak)) void shell_video_stop(void) {}
__attribute__((weak)) uint32_t game_capture_size(void) { return 0; }
__attribute__((weak)) uint8_t game_capture_byte(uint32_t k) { (void)k; return 0; }

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
    while (k < width - n && k + n < 11)          /* (width > 11: the number keeps its digits, s its NUL) */
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

static void put_hex(int col, int row, uint32_t v, int digits)
{
    char s[9];
    for (int k = 0; k < digits && k < 8; k++)
        s[k] = "0123456789ABCDEF"[(v >> (4 * (digits - 1 - k))) & 15];
    s[digits < 8 ? digits : 8] = 0;
    cps3v_text(col, row, s);
}

static void clear_text(void)
{
    for (int r = 0; r < 28; r++)
        cps3v_text(0, r, "                                                ");
}

/* ---- the game capture (src/main game.h) as pages of code cells, for screenshots (docs/ARCADE.md section 7) ----
   A page is CAP_PAGE bytes: page index, page count, the capture's size (16 bits), CRC-16 (CCITT, 0xffff) of the
   page's other bytes, then CAP_PDATA bytes of the capture (zeros after its end). Its bits, first byte's high bit
   first, 7 to a cell, fill CAP_COLS x CAP_ROWS text cells row by row from cell (CAP_COL0, CAP_ROW0). A cell shows
   SS tile CAP_TILE + value: 4 x 2 pixel blocks in 2 columns and 4 rows, block b (row b / 2, column b % 2) white
   (colour 1) for value bit 6 - b set, black (colour 2) if not, block 7 red (colour 3: the decoder's alignment
   mark). tools/capture.py decodes the screenshots. */
#define CAP_ROW0  2
#define CAP_ROWS  24
#define CAP_COL0  1
#define CAP_COLS  46
#define CAP_CELLS (CAP_ROWS * CAP_COLS)
#define CAP_PAGE  (CAP_CELLS * 7 / 8)
#define CAP_PDATA (CAP_PAGE - 6)
#define CAP_TILE  128
#define CAP_AUTO  150                            /* frames a page shows before the next (2.5 s) */
#define SSW(n, a, b) (*(volatile uint32_t *)(0x05040000u + 4u * (n)) = ((uint32_t)(a) << 16) | (b))

static void cap_glyphs(void)
{
    static const uint16_t col[2] = { 0x0000, 0x001f };   /* colours 2 black, 3 red (BGR555) */
    for (int v = 0; v < 128; v++)
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x += 4) {     /* 4 pixels: 2 SS bytes, the left pixel in the low nibble */
                uint8_t px[4];
                int b = (y / 2) * 2 + x / 4;
                px[0] = px[1] = px[2] = px[3] = (uint8_t)(b == 7 ? 3 : (v >> (6 - b)) & 1 ? 1 : 2);
                SSW((0x4000 + (CAP_TILE + v) * 32 + y * 4 + x / 2) / 2, px[0] | px[1] << 4, px[2] | px[3] << 4);
            }
    cps3v_colours(0x1fe02, col, 2);
}

static uint16_t crc16(uint16_t c, const uint8_t *p, int n)
{
    while (n--) {
        c ^= (uint16_t)(*p++ << 8);
        for (int b = 0; b < 8; b++)
            c = (uint16_t)(c & 0x8000 ? c << 1 ^ 0x1021 : c << 1);
    }
    return c;
}

static void cap_make_page(uint8_t *pg, uint32_t size, int p, int np)
{
    pg[0] = (uint8_t)p;
    pg[1] = (uint8_t)np;
    pg[2] = (uint8_t)(size >> 8);
    pg[3] = (uint8_t)size;
    for (int k = 0; k < CAP_PDATA; k++) {
        uint32_t i = (uint32_t)p * CAP_PDATA + k;
        pg[6 + k] = i < size ? game_capture_byte(i) : 0;
    }
    uint16_t c = crc16(crc16(0xffff, pg, 4), pg + 6, CAP_PDATA);
    pg[4] = (uint8_t)(c >> 8);
    pg[5] = (uint8_t)c;
    pg[CAP_PAGE] = 0;
}

static void cap_show_page(const uint8_t *pg, int p, int np)
{
    for (int j = 0; j < CAP_CELLS; j++) {
        int b = 7 * j;
        int v = ((pg[b >> 3] << 8 | pg[(b >> 3) + 1]) >> (9 - (b & 7))) & 0x7f;
        SSW((CAP_ROW0 + j / CAP_COLS) * 64 + CAP_COL0 + j % CAP_COLS, CAP_TILE + v, 0);
    }
    put_num(30, 0, (uint32_t)p + 1, 2);
    put_num(36, 0, (uint32_t)np, 2);
}

/* the capture's pages until B2 or the test switch; B1 / right the next page, left the one before, and every
   CAP_AUTO frames the next by itself */
static void capture_view(void)
{
    uint8_t pg[CAP_PAGE + 1];                    /* on the stack: main RAM .bss is short (tests/ramcheck.ld) */
    uint32_t prev = 0xffffffffu, prev_sys = 0xffffffffu;
    uint32_t size = game_capture_size();
    int np = (int)((size + CAP_PDATA - 1) / CAP_PDATA), p = 0, shown = -1, t = 0;
    clear_text();
    if (size) {
        cap_glyphs();
        cps3v_text(2, 0, "SPELUNKY GAME CAPTURE  PAGE    OF");
        uint32_t w[6];                           /* header words 0-5 (src/main game.h CAP_W_*) for the text */
        for (int k = 0; k < 24; k++)
            w[k / 4] = w[k / 4] << 8 | game_capture_byte((uint32_t)k);
        cps3v_text(2, 26, "BUILD          STEPS        BYTES");
        put_hex(8, 26, w[3], 8);
        put_num(23, 26, w[5], 6);
        put_num(36, 26, size, 5);
        cps3v_text(2, 27, "B1 NEXT   LEFT BACK   B2 MENU");
    } else {
        cps3v_text(8, 11, "NO GAME CAPTURE");
        cps3v_text(8, 13, "PLAY A GAME FIRST");
        cps3v_text(8, 17, "B2 MENU");
    }
    for (;;) {
        if (size && p != shown) {                /* the page's bytes before the VBlank, its cells after */
            cap_make_page(pg, size, p, np);
            cps3v_wait_vblank();
            cap_show_page(pg, p, np);
            shown = p;
            t = 0;
        }
        cps3v_wait_vblank();
        cps3v_vblank();
        uint32_t q = cps3_pad(0) | cps3_pad(1), sys = cps3_system();
        uint32_t press = q & ~prev, spress = sys & ~prev_sys;
        prev = q;
        prev_sys = sys;
        if ((press & CPS3_B2) || (spress & CPS3_TEST))
            break;
        if (!size)
            continue;
        if ((press & (CPS3_B1 | CPS3_RIGHT)) || ++t >= CAP_AUTO)
            p = (p + 1) % np;
        else if (press & CPS3_LEFT)
            p = (p + np - 1) % np;
    }
    clear_text();
}

/* ---- settings screen (test switch) ---- */
/* DEV=1 builds (SHELL_DEV) add the developer option INVINCIBLE before GAME CAPTURE */
#ifdef SHELL_DEV
#define SEL_GOD  5
#define SEL_CAP  6
#define SEL_LAST 7
#define HELP_ROW 23
#else
#define SEL_CAP  5
#define SEL_LAST 6
#define HELP_ROW 22
#endif
static void line(int row, int sel, const char *label)
{
    cps3v_text(8, row, sel ? "+" : " ");          /* the SDK font has no ">" */
    cps3v_text(10, row, label);
}

static void settings_text(void)
{
    cps3v_text(16, 3, "SPELUNKY SETTINGS");
    cps3v_text(8, HELP_ROW, "UP / DOWN    CHOOSE");
    cps3v_text(8, HELP_ROW + 2, "B1 / RIGHT   CHANGE");
    cps3v_text(8, HELP_ROW + 4, "TEST         SAVE AND EXIT");
}

static void settings_run(void)
{
    cps3s_init();                                /* sound off */
    shell_video_stop();                          /* the game's VBlank interrupt work */
    cps3v_begin();                               /* no sprites */
    cps3v_end();
    for (int t = 0; t < 4; t++)
        cps3v_tilemap(t, 0, 0, CPS3V_MAP_UNIT(0), 0);
    clear_text();
    struct settings st;                          /* by field: a struct copy is a memcpy call (-nostdlib) */
    st.free_play = SH.st.free_play;
    st.coins_per_credit = SH.st.coins_per_credit;
    st.toggle_run = SH.st.toggle_run;
    st.smooth = SH.st.smooth;
    st.invincible = SH.st.invincible;
    int sel = 0, clear = 0;
    uint32_t prev = 0xffffffffu, prev_sys = 0xffffffffu;
    settings_text();
    for (;;) {
        line(7, sel == 0, "FREE PLAY");
        cps3v_text(32, 7, st.free_play ? "ON " : "OFF");
        line(9, sel == 1, "COINS PER CREDIT");
        put_num(32, 9, st.coins_per_credit, 1);
        line(11, sel == 2, "RUN BUTTON");
        cps3v_text(32, 11, st.toggle_run ? "TOGGLE" : "HOLD  ");
        line(13, sel == 3, "SMOOTH MOTION");
        cps3v_text(32, 13, st.smooth ? "ON " : "OFF");
        line(15, sel == 4, "CLEAR HIGH SCORES");
        cps3v_text(32, 15, clear ? "YES" : "NO ");
#ifdef SHELL_DEV
        line(17, sel == SEL_GOD, "INVINCIBLE (DEV)");
        cps3v_text(32, 17, st.invincible ? "ON " : "OFF");
        line(19, sel == SEL_CAP, "GAME CAPTURE");
        line(21, sel == SEL_LAST, "SAVE AND EXIT");
#else
        line(17, sel == SEL_CAP, "GAME CAPTURE");
        line(19, sel == SEL_LAST, "SAVE AND EXIT");
#endif
        cps3v_wait_vblank();
        cps3v_vblank();
        uint32_t p = cps3_pad(0) | cps3_pad(1), sys = cps3_system();
        uint32_t press = p & ~prev, spress = sys & ~prev_sys;
        prev = p;
        prev_sys = sys;
        if ((press & CPS3_UP) && sel > 0)
            sel--;
        if ((press & CPS3_DOWN) && sel < SEL_LAST)
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
                st.smooth = !st.smooth;
            else if (sel == 4)
                clear = !clear;
#ifdef SHELL_DEV
            else if (sel == SEL_GOD)
                st.invincible = !st.invincible;
#endif
            else if (sel == SEL_CAP) {
                if (step > 0) {
                    capture_view();
                    settings_text();
                    prev = cps3_pad(0) | cps3_pad(1);   /* the buttons that left the view are no press */
                    prev_sys = cps3_system();
                }
            } else if (press & CPS3_B1)
                leave = 1;
        }
        if (leave)
            break;
    }
    if (clear)                                   /* the next boot: HD's first start, these settings kept */
        hs_clear(&st);
    else
        hs_write(&SH.hs, &st);
    /* restart as a reset does: interrupts masked, VBR 0 (the ROM's vector table: the game may have moved it to RAM,
       which start-up clears) */
    __asm__ volatile("ldc %0, sr\n\tmov #0, r0\n\tldc r0, vbr\n\tmov.l 1f, r0\n\tjmp @r0\n\tnop\n\t.align 2\n1:\t.long start"
                     : : "r"(0xf0) : "r0");
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
    /* a bijection of the previous value for each frame's inputs: a coin or Start one frame later gives another seed */
    SH.entropy = (SH.entropy ^ pad0 ^ (pad1 << 16 | pad1 >> 16) ^ lines) * 2654435761u + 1;
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
