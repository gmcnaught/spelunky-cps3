/* HD 1.2.2's HUD as CPS3 sprites (hud.h). Line references: refs/hd/src/scripts/scrDrawHUD/scrDrawHUD.gml. */
#include "cps3.h"
#include "sprites.h"
#include "hudart.h"
#include "hud.h"

static uint32_t pal_spr;

/* draw_sprite(spr, img, x, y) at GUI (view) coordinates; img < 0: the caller's frame already chosen as 0 */
static void spr(int s, int32_t img, int x, int y)
{
    const struct sprdef *sd = &sprdefs[s];
    int32_t n = sd->nframes;
    int32_t f = img % n;
    if (f < 0)
        f += n;
    const struct framedef *fd = &framedefs[sd->frame + f];
    y -= HUD_CROP;
    for (int p = fd->piece; p < fd->piece + fd->npieces; p++) {
        const struct piecedef *pc = &piecedefs[p];
        int px = x + pc->dx, py = y + pc->dy;
        if (px >= HUD_VIEW_W || py >= CPS3V_H || px + 16 * pc->w <= 0 || py + 16 * pc->h <= 0)
            continue;
        cps3v_sprite(px, py, pc->w, pc->h, pc->tile, pal_spr, 0);
    }
}

char *hud_itoa(int32_t n, char *buf)
{
    char t[12];
    int k = 0, o = 0;
    uint32_t u = n < 0 ? 0u - (uint32_t)n : (uint32_t)n;
    do {
        t[k++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    if (n < 0)
        buf[o++] = '-';
    while (k)
        buf[o++] = t[--k];
    buf[o] = 0;
    return buf;
}

static void glyphs(const char *text, enum hud_font f, const uint32_t *yellow, int all_yellow, int x, int y)
{
    int adv = f == HUD_FONT_LARGE ? 16 : 8;
    y -= HUD_CROP;
    for (int i = 0; text[i]; i++, x += adv) {
        unsigned c = (unsigned char)text[i];
        if (c <= ' ' || c > 'Z' || x >= HUD_VIEW_W || x + adv <= 0 || y >= CPS3V_H || y + adv <= 0)
            continue;
        int yl = all_yellow || (yellow && i < 64 && (yellow[i >> 5] >> (i & 31) & 1));
        cps3v_sprite(x, y, 1, 1, f == HUD_FONT_LARGE ? HUD_GLYPH_LARGE(c) : HUD_GLYPH_SMALL(c),
                     yl ? HUD_PAL_YELLOW : HUD_PAL, 0);
    }
}

static int len(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}

/* drawText: draw_text at (x, y + global.fontOffsetY); fontOffsetY is 0 for the English sprite font (scripts/
   setLocale: no charset/font.json) */
void hud_text(const char *text, enum hud_font f, int yellow, int x, int y)
{
    glyphs(text, f, 0, yellow, x, y);
}

/* drawTextHCentered: posX = ceil((display_w - length * width) / 2) + offsetX */
static int centre_x(int n, enum hud_font f)
{
    int w = HUD_VIEW_W - n * (f == HUD_FONT_LARGE ? 16 : 8);
    return w >= 0 ? (w + 1) / 2 : -((-w) / 2);
}

void hud_text_centered(const char *text, enum hud_font f, int yellow, int offset_x, int y)
{
    glyphs(text, f, 0, yellow, centre_x(len(text), f) + offset_x, y);
}

/* the hold box's item (scrDrawHUD :76-94), drawn at (8 + 8, 24 + 8) */
static const int16_t held_spr[HUD_HELD_COUNT] = {
    -1, SPR_sRock, SPR_sJar, SPR_sSkull, SPR_sFishBone, SPR_sArrowRight, SPR_sMacheteRight, SPR_sMattockRight,
    SPR_sMattockHead, SPR_sPistolRight, SPR_sWebCannonR, SPR_sTeleporter, SPR_sShotgunRight, SPR_sBowDisp,
    SPR_sSceptreRight, SPR_sFlare, SPR_sKeyRight, -1,
};

/* the item icons in scrDrawHUD's order (:97-171); kapala and udjat handled where they fall */
static const struct { uint16_t flag; int16_t spr; } icons[] = {
    { HUD_UDJAT, -1 }, { HUD_ANKH, SPR_sAnkhIcon }, { HUD_CROWN, SPR_sCrownIcon }, { HUD_KAPALA, -1 },
    { HUD_SPECTACLES, SPR_sSpectaclesIcon }, { HUD_GLOVES, SPR_sGlovesIcon }, { HUD_MITT, SPR_sMittIcon },
    { HUD_SPRINGSHOES, SPR_sSpringShoesIcon }, { HUD_SPIKESHOES, SPR_sSpikeShoesIcon }, { HUD_CAPE, SPR_sCapeIcon },
    { HUD_JETPACK, SPR_sJetpackIcon }, { HUD_COMPASS, SPR_sCompassIcon }, { HUD_PARACHUTE, SPR_sParachuteIcon },
};

static void compass(const struct hud_state *s)
{
    int small = s->message_timer > 0;
    int vx = s->view_x, vy = s->view_y;
    if (s->exit_y > vy + 240) {                                       /* :45-60 */
        if (s->exit_x < vx)
            spr(small ? SPR_sCompassSmallLL : SPR_sCompassLL, s->anim, 0, 224);
        else if (s->exit_x > vx + 320 - 16)
            spr(small ? SPR_sCompassSmallLR : SPR_sCompassLR, s->anim, 304, 224);
        else
            spr(small ? SPR_sCompassSmallDown : SPR_sCompassDown, s->anim, s->exit_x - vx, 224);
    } else if (s->exit_x < vx)                                        /* :61-65 */
        spr(small ? SPR_sCompassSmallLeft : SPR_sCompassLeft, s->anim, 0, s->exit_y - vy);
    else if (s->exit_x > vx + 320 - 16)                               /* :66-70 */
        spr(small ? SPR_sCompassSmallRight : SPR_sCompassRight, s->anim, 304, s->exit_y - vy);
}

static void message(const struct hud_msg *m, int y)
{
    /* showMessages -> drawMessage (no background: global.messageBackground is false for a sprite font) /
       drawHighlightedMessage: centred on display_w, small font, white with the highlighted parts yellow */
    glyphs(m->text, HUD_FONT_SMALL, m->yellow, 0, centre_x(len(m->text), HUD_FONT_SMALL), y);
}

void hud_draw(const struct hud_state *s, uint32_t pal_game)
{
    char b[12];
    pal_spr = pal_game;
    cps3v_group();
    if (s->visible) {
        const int lifeX = 8, bombX = 64, ropeX = 120, moneyX = 176;  /* :24-29 (vxv = vyv = 0) */
        spr(SPR_sHeart, s->anim, lifeX, 8);
        hud_text(hud_itoa(s->life < 0 ? 0 : s->life, b), HUD_FONT_LARGE, 0, lifeX + 16, 8);
        spr(s->sticky_bombs ? SPR_sStickyBombIcon : SPR_sBombIcon, s->anim, bombX, 8);
        hud_text(hud_itoa(s->bombs, b), HUD_FONT_LARGE, 0, bombX + 16, 8);
        spr(SPR_sRopeIcon, s->anim, ropeX, 8);
        hud_text(hud_itoa(s->ropes, b), HUD_FONT_LARGE, 0, ropeX + 16, 8);
        /* global.sMoneySignNew: money_sign.png, one 16 x 16 frame, origin 0, 0, at (moneyX, 6) */
        if (6 - HUD_CROP > -16)
            cps3v_sprite(moneyX, 6 - HUD_CROP, 1, 1, HUD_TILE_MONEYSIGN, HUD_PAL, 0);
        hud_text(hud_itoa(s->money, b), HUD_FONT_LARGE, 0, moneyX + 16, 8);
        if (s->items & HUD_COMPASS)
            compass(s);
        spr(SPR_sHoldItemIcon, s->anim, 8, 24);                    /* :74 */
        if (s->held < HUD_HELD_COUNT && held_spr[s->held] >= 0)
            spr(held_spr[s->held], s->anim, 16, 32);
        int n = 28;                                                   /* :96 */
        for (unsigned k = 0; k < sizeof icons / sizeof icons[0]; k++) {
            if (!(s->items & icons[k].flag))
                continue;
            if (icons[k].flag == HUD_UDJAT)
                spr(s->udjat_blink ? SPR_sUdjatEyeIcon2 : SPR_sUdjatEyeIcon, s->anim, n, 24);
            else if (icons[k].flag == HUD_KAPALA) {                   /* :115-122: no icon above blood level 8 */
                int bl = s->blood_level, f = bl == 0 ? 0 : bl <= 2 ? 1 : bl <= 4 ? 2 : bl <= 6 ? 3 : bl <= 8 ? 4 : -1;
                if (f >= 0)
                    spr(SPR_sKapalaIcon, f, n, 24);
            } else
                spr(icons[k].spr, s->anim, n, 24);
            n += 20;
        }
        if (s->held == HUD_HELD_BOW)                                 /* :172-181 */
            for (int m = s->arrows; m > 0; m--, n += 4)
                spr(SPR_sArrowIcon, s->anim, n, 24);
        if (s->collect > 0) {                                         /* :182-184 */
            b[0] = '+';
            hud_itoa(s->collect, b + 1);
            hud_text(b, HUD_FONT_SMALL, 1, moneyX, 24);
        }
    }
    if (s->message_timer > 0) {                                       /* showMessages: y1 = 216 + 8 - 8 */
        message(&s->message1, 216);
        message(&s->message2, 224);
    }
}
