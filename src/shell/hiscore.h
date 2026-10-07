/* High scores and operator settings in the CPS3's EEPROM, holding what HD 1.2.2 keeps in spelunky.ini
 * (VARIANTS X5; docs/ARCADE.md "EEPROM").
 *
 * HD's [highscore] section (objects/oGlobals/Create_0.gml, scripts/scrUpdateHighscores, scrResetHighscores,
 * highscore_add2; read by oTitle, oHighscores, oTransition, oTunnelMan):
 *   value1 MONEY  value2 TIME (s)  value3 KILLS  value4 SAVES  value5 PLAYS  value6 WINS  value7 DEATHS
 *   value8 TUNNEL1  value9 TUNNEL2 (the tunnel man's remaining prices)  value10 MINIGAMES (sun * 10000 + moon * 100
 *   + stars). The name<k> strings are the labels only and are not stored. All ten are kept: each is shown or used
 *   by an arcade-kept room (title shortcuts, the scores room, transitions, the sun / moon / stars rooms).
 * Not stored (not arcade): settings.json (locale, fullscreen, music, toggle run, touch), keys.json, gamepad.json.
 *
 * EEPROM words (93C46: 32 x 32 bits; words 0-15 left alone, Maldita's rule for the stand-in set):
 *   16 magic 0x53504b01 ("SPK", version 1)  17-26 value1..value10  27 settings  28 check (words 16-27 summed,
 *   xor 0x5a5a5a5a). A block with a wrong magic or check reads as HD's missing spelunky.ini. */
#ifndef SHELL_HISCORE_H
#define SHELL_HISCORE_H
#include <stdint.h>

#define HS_EE_AT     16
#define HS_EE_WORDS  13
#define HS_MAGIC     0x53504b01u

/* HD globals (oGlobals Create) */
#define TUNNEL1_MAX 100000
#define TUNNEL2_MAX 200000
#define TUNNEL3_MAX 300000

struct hiscores {
    int32_t value[11];          /* value[1..10] as HD's ini; value[0] unused */
};
#define HS_MONEY   1
#define HS_TIME    2
#define HS_KILLS   3
#define HS_SAVES   4
#define HS_PLAYS   5
#define HS_WINS    6
#define HS_DEATHS  7
#define HS_TUNNEL1 8
#define HS_TUNNEL2 9
#define HS_MINI    10

/* operator settings (word 27) */
struct settings {
    uint8_t free_play;          /* bit 0 */
    uint8_t coins_per_credit;   /* bits 8-11: 1..9 (0 or > 9 read as 1) */
    uint8_t toggle_run;         /* bit 1: the run button toggles running (HD's global.toggleRunEnabled, X10) */
    uint8_t smooth;             /* bit 2 clear: smooth motion (src/draw draw_smooth; on in a new or older block) */
    uint8_t invincible;         /* bit 3: developer option INVINCIBLE (src/game play_god); read only in SHELL_DEV
                                   builds (DEV=1), so a release build ignores a bit a dev build left */
};

/* the globals oGlobals Create sets from the ini */
struct hs_globals {
    int32_t tunnel1, tunnel2;   /* global.tunnel1 / 2 */
    uint8_t first_time;         /* global.firstTime (oTitle Create clears it again, VARIANTS X12) */
};

/* EEPROM access (src/shell/shell.c: the SDK's cps3_ee_read / write; a host test supplies its own) */
uint32_t shell_ee_read(int word);
void shell_ee_write(int word, uint32_t v);

/* boot: oGlobals Create. Reads the block (a bad block = no ini: every value 0); global.tunnel1 / 2 read value8 / 9
 * with HD's defaults 10001 / 20001 when there is no ini; then, when value1..9 are all <= 0, scrResetHighscores
 * writes the reset block (value8 = TUNNEL1_MAX + 1, value9 = TUNNEL2_MAX + 1) and firstTime is set. As in HD, the
 * tunnel globals of that session keep the values read before the reset. */
void hs_boot(struct hiscores *hs, struct settings *st, struct hs_globals *g);
void hs_write(const struct hiscores *hs, const struct settings *st);
/* the settings screen's CLEAR HIGH SCORES: the next boot is HD's first start (no ini) with st kept. Stores st with
 * value1..10 all 0, which hs_boot reads as no ini (no HD session stores it: scrResetHighscores leaves value8 / 9
 * above 0 and every game adds a play) */
void hs_clear(const struct settings *st);

/* the game's state at scrUpdateHighscores(type) */
enum hs_type { HS_DEATH = 0, HS_WIN = 1, HS_MINIGAME = 2 };
struct hs_run {
    int32_t money;              /* global.money */
    int32_t time;               /* global.time (oGame Step adds game_speed a step); scrUpdateHighscores divides it */
    int32_t kills, damsels;     /* global.kills, global.damsels */
    int32_t tunnel1, tunnel2;   /* global.tunnel1 / 2 now */
    int32_t mini1, mini2, mini3;
    uint8_t used_shortcut, keep_score;
};
#define HS_NEW_MONEY  0x01u     /* global.newMoney */
#define HS_NEW_TIME   0x02u
#define HS_NEW_KILLS  0x04u
#define HS_NEW_SAVES  0x08u
#define HS_NEW_MINI1  0x10u     /* oSunRoom.highscore */
#define HS_NEW_MINI2  0x20u     /* oMoonRoom.highscore */
#define HS_NEW_MINI3  0x40u     /* oStarsRoom.highscore */
/* scrUpdateHighscores(type): updates hs and the EEPROM; returns HS_NEW_* flags. r->time becomes floor(time / 1000)
 * as global.time does in HD. Nothing happens when keep_score is 0. */
uint32_t hs_update(struct hiscores *hs, const struct settings *st, struct hs_run *r, enum hs_type type);
#endif
