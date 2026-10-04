/* tests/playsh2 JT builds: large text on the CPS3 SS text layer, readable in a MiSTer screenshot. Each character
   is a 3x5 glyph whose pixels are 4x4 screen pixels: 16 quadrant tiles (tile 128 + k, k bit 0 top-left, 1
   top-right, 2 bottom-left, 3 bottom-right quarter lit) put 2x2 glyph pixels in one 8x8 cell, so a character is 2
   cells wide and 3 high (a column and a row of spacing included): 24 characters a row, 9 rows (cell rows 0-26). */
#include <stdint.h>

#define SSRAM 0x05040000u
#define SSW(n, a, b) (*(volatile uint32_t *)(SSRAM + 4 * (n)) = ((uint32_t)(a) << 16) | (uint32_t)(b))

/* 3x5 glyphs, one byte per row, bit 2 = left */
static const struct { char c; uint8_t r[5]; } glyph[] = {
    {'0', {7, 5, 5, 5, 7}}, {'1', {2, 6, 2, 2, 7}}, {'2', {7, 1, 7, 4, 7}}, {'3', {7, 1, 3, 1, 7}},
    {'4', {5, 5, 7, 1, 1}}, {'5', {7, 4, 7, 1, 7}}, {'6', {7, 4, 7, 5, 7}}, {'7', {7, 1, 1, 2, 2}},
    {'8', {7, 5, 7, 5, 7}}, {'9', {7, 5, 7, 1, 7}}, {'A', {2, 5, 7, 5, 5}}, {'B', {6, 5, 6, 5, 6}},
    {'D', {6, 5, 5, 5, 6}}, {'F', {7, 4, 6, 4, 4}}, {'G', {7, 4, 5, 5, 7}}, {'I', {7, 2, 2, 2, 7}},
    {'K', {5, 5, 6, 5, 5}}, {'L', {4, 4, 4, 4, 7}}, {'M', {5, 7, 7, 5, 5}}, {'O', {7, 5, 5, 5, 7}},
    {'P', {7, 5, 7, 4, 4}}, {'R', {6, 5, 6, 5, 5}}, {'S', {7, 4, 7, 1, 7}}, {'T', {7, 2, 2, 2, 2}},
    {'X', {5, 5, 2, 5, 5}}, {'V', {5, 5, 5, 5, 2}}, {'E', {7, 4, 6, 4, 7}}, {'N', {6, 5, 5, 5, 5}}, {'/', {1, 1, 2, 4, 4}}, {' ', {0, 0, 0, 0, 0}},
};

void big_init(void)
{
    int k, y;
    for (k = 0; k < 16; k++)                       /* tile 128 + k: 8 rows of 4 bytes (2 pixels a byte, low nibble left) */
        for (y = 0; y < 8; y++) {
            int l = (k >> (y < 4 ? 0 : 2)) & 1, r = (k >> (y < 4 ? 1 : 3)) & 1;
            uint32_t base = (0x4000 + (128 + k) * 32 + y * 4) / 2;
            SSW(base, 0x11 * l, 0x11 * l);
            SSW(base + 1, 0x11 * r, 0x11 * r);
        }
    for (k = 0; k < 64 * 28; k++)                  /* clear the screen */
        SSW(k, ' ', 0);
}

static const uint8_t *find(char c)
{
    unsigned k;
    for (k = 0; k < sizeof glyph / sizeof glyph[0]; k++)
        if (glyph[k].c == c) return glyph[k].r;
    return glyph[sizeof glyph / sizeof glyph[0] - 1].r;
}

/* the glyph pixel (x 0-3, y 0-5) of character c: 3x5 with a blank column 3 and row 5 */
static int px(const uint8_t *g, int x, int y)
{
    if (x > 2 || y > 4) return 0;
    return (g[y] >> (2 - x)) & 1;
}

void big_text(int row, const char *s)
{
    int col;
    for (col = 0; col < 24 && *s; col++, s++) {
        const uint8_t *g = find(*s);
        int cx, cy;
        for (cy = 0; cy < 3; cy++)
            for (cx = 0; cx < 2; cx++) {
                int k = px(g, 2 * cx, 2 * cy) | px(g, 2 * cx + 1, 2 * cy) << 1 | px(g, 2 * cx, 2 * cy + 1) << 2 |
                        px(g, 2 * cx + 1, 2 * cy + 1) << 3;
                SSW((row * 3 + cy) * 64 + col * 2 + cx, 128 + k, 0);
            }
    }
}
