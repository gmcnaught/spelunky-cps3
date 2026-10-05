/* HD 1.2.2's in-game HUD (scripts/scrDrawHUD, called from oGame's Draw GUI, objects/oGame/Draw_64.gml:121) and
 * the message lines (scripts/showMessages, :122), drawn from a plain state struct as CPS3 sprites.
 *
 * Coordinates are HD's GUI coordinates (display_set_gui_size(display_w, display_h)): with the widescreen code
 * removed (VARIANTS X1) the GUI is the 320 x 240 view, so a HUD position is a view position. The port shows view
 * lines 8..231 (PLAN §1): everything is drawn 8 lines up (HUD_CROP); the X zoom (0x35) stretches it as the level.
 *
 * Art: HD's sprites (tools/hdsprites.py: colour code given by the caller, the game's palette) and the fonts and money
 * sign from HD's datafiles (tools/hudart.py: colour codes HUD_PAL / HUD_PAL_YELLOW).
 *
 * The draw has no side effects. HD's showMessages also counts global.messageTimer down by one each Draw GUI, and
 * scrDrawHUD sets global.exitX / exitY to 640 / 544 in rOlmec: the play code keeps those (state, not drawing). */
#ifndef HUD_H
#define HUD_H
#include <stdint.h>

#define HUD_CROP 8               /* view line shown on screen line 0 */
#define HUD_VIEW_W 320           /* global.display_w without widescreen */

/* oPlayer1.pickupItemType values that draw an item in the hold box (scrDrawHUD); any other type: the box only */
enum hud_held {
    HUD_HELD_NONE, HUD_HELD_ROCK, HUD_HELD_JAR, HUD_HELD_SKULL, HUD_HELD_FISHBONE, HUD_HELD_ARROW,
    HUD_HELD_MACHETE, HUD_HELD_MATTOCK, HUD_HELD_MATTOCKHEAD, HUD_HELD_PISTOL, HUD_HELD_WEBCANNON,
    HUD_HELD_TELEPORTER, HUD_HELD_SHOTGUN, HUD_HELD_BOW, HUD_HELD_SCEPTRE, HUD_HELD_FLARE, HUD_HELD_KEY,
    HUD_HELD_OTHER, HUD_HELD_COUNT
};

/* global.has* in scrDrawHUD's order */
#define HUD_UDJAT       0x0001u
#define HUD_ANKH        0x0002u
#define HUD_CROWN       0x0004u
#define HUD_KAPALA      0x0008u
#define HUD_SPECTACLES  0x0010u
#define HUD_GLOVES      0x0020u
#define HUD_MITT        0x0040u
#define HUD_SPRINGSHOES 0x0080u
#define HUD_SPIKESHOES  0x0100u
#define HUD_CAPE        0x0200u
#define HUD_JETPACK     0x0400u
#define HUD_COMPASS     0x0800u
#define HUD_PARACHUTE   0x1000u

#define HUD_MSG_MAX 48
struct hud_msg {
    char text[HUD_MSG_MAX + 1];  /* upper case ' '..'Z'; other characters are drawn as nothing */
    uint32_t yellow[2];          /* bit k: character k in c_yellow (drawHighlightedMessage's highlighted parts) */
};

struct hud_state {
    uint8_t visible;             /* global.drawHUD and instance_exists(oPlayer1) */
    int32_t life;                /* global.plife (drawn as 0 when below) */
    int32_t bombs, ropes, money; /* global.bombs, global.rope, global.money */
    int32_t collect;             /* global.collect: "+N" under the money when > 0 */
    uint8_t sticky_bombs;        /* global.hasStickyBombs */
    uint8_t held;                /* enum hud_held: oPlayer1.pickupItemType */
    uint16_t items;              /* HUD_* flags */
    uint8_t udjat_blink;         /* global.udjatBlink */
    int16_t blood_level;         /* global.bloodLevel (kapala) */
    int16_t arrows;              /* global.arrows (drawn while holding the bow) */
    int32_t anim;                /* oGame.image_index (whole part): the frame of sprites drawn with image -1 */
    /* compass (HUD_COMPASS): the exit and the view's top-left in room pixels */
    int32_t exit_x, exit_y, view_x, view_y;
    /* showMessages: global.messageTimer > 0 draws the two lines (and the small compass arrows) */
    int16_t message_timer;
    struct hud_msg message1, message2;
};

/* draws the HUD into the open display list (cps3v_sprite); pal_game: the colour code of tools/hdsprites.py's
   palette. Groups: one new main-list record (cps3v_group) first, so the HUD is drawn over everything before it */
void hud_draw(const struct hud_state *s, uint32_t pal_game);

/* drawText / drawTextHCentered with HD's English sprite fonts (16 x 16 large, 8 x 8 small; fixed advance) */
enum hud_font { HUD_FONT_SMALL, HUD_FONT_LARGE };
void hud_text(const char *text, enum hud_font f, int yellow, int x, int y);
void hud_text_centered(const char *text, enum hud_font f, int yellow, int offset_x, int y);
/* text drawn under a black rectangle (the ending's fades: showFinalScore, drawCredits) uses the HUD palettes src/draw
   fades with the frame's rectangle (draw.h DRAW_PAL_HUDDARK, DRAW_PAL_HUDDARK_YELLOW); on: hud_text and
   hud_text_centered draw with them until turned off */
#define HUD_PAL_FADED 5u
#define HUD_PAL_FADED_YELLOW 6u
void hud_text_faded(int on);
/* string(n) of a whole number into buf (at least 12 bytes); returns buf */
char *hud_itoa(int32_t n, char *buf);
#endif
