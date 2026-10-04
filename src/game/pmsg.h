/* The HUD's message lines in the play state: scripts/trMessages (global.message1 / message2, their highlights and
 * global.messageTimer) and global.bloodLevel (the kapala), kept here rather than in play.h's struct pglobals.
 *
 * The text is stored as drawn: English (datafiles/locale/locales/en/text.json: tr() of every message the play code
 * shows is its own key), the parts of an array message joined, and yellow bit k set for character k of a part that
 * drawHighlightedMessage colours (message given as an array and its highlights as an array holding the part's
 * index). showMessages draws the lines while messageTimer > 0 and counts it down by one each Draw GUI: src/draw
 * does that once per drawn frame (pmsg_frame). */
#ifndef PMSG_H
#define PMSG_H
#include <stdint.h>

#define PMSG_MAX 48
struct pmsg { char text[PMSG_MAX + 1]; uint32_t yellow[2]; };
struct pmsgs {
    int16_t timer;               /* global.messageTimer */
    int16_t bloodLevel;          /* global.bloodLevel */
    struct pmsg m1, m2;          /* global.message1 / message2 as drawn */
    struct pmsg pm1, pm2;        /* oPlayer1.message1 / message2 (instance variables: oPlayer1 Alarm_0 / 1 show them
                                    again when no case matches); highlights never: trMessages(.., 0, 0, ..) */
};
extern struct pmsgs PMSG;

/* trMessages(message1, message2, messageHighlights, message2Highlights, timer): each message as n parts (n = 0 for
   a plain string, given as part[0]); hl1 / hl2: bit k = part k is highlighted (0: not an array, nothing is) */
void pmsg_tr(const char *const *m1, int n1, uint32_t hl1, const char *const *m2, int n2, uint32_t hl2, int timer);
/* trMessages(s1, s2, 0, 0, timer) with two plain strings */
void pmsg_str(const char *s1, const char *s2, int timer);
/* oPlayer1 code: message1 = m1; message2 = m2; trMessages(message1, message2, 0, 0, timer) */
void pmsg_player(const char *const *m1, int n1, const char *const *m2, int n2, int timer);
void pmsg_player_str(const char *s1, const char *s2, int timer);
/* oPlayer1: trMessages(message1, message2, 0, 0, timer) with the variables as they are */
void pmsg_player_again(int timer);
/* oPlayer1 Create :103: message1 = message2 = "" */
void pmsg_player_reset(void);
/* global.message1 = global.message2 = "" (oTransition Create), the timer kept */
void pmsg_clear(void);
/* string(v) of a whole number into b (12 bytes) */
const char *pmsg_num(int32_t v, char *b);
/* showMessages' countdown (src/draw: once per drawn frame, after drawing the lines) */
void pmsg_frame(void);
#endif
