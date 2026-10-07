/* The front end (front.h): HD 1.2.2's rIntro, rTitle and rHighscores on the play loop (src/game), with the events
 * of their own objects translated here, statement for statement (refs/hd/src/objects/<o>/<event>.gml: file:line in
 * comments). Objects src/game translates (oRopeThrow, oRope, oGame, the walls) run there.
 *
 * Gameplay state (what an attract room leaves behind, PLAN §1 exactness is per seed):
 *   - the RNG: oIntro's Create calls randomize() (objects/oIntro/Create_0.gml:6): HD seeds from the clock there, and
 *     every draw in the intro and title rooms follows from it (oDark Create's rand(1,100) x 203 come before it;
 *     oIntro's 3 random_range, scrSetupWalls' tiles, oBatIntro's random() and the title's draws after). A level's
 *     generation then continues the same generator. On the cabinet the shell starts a game with game_begin, which
 *     seeds the generator again (game_cfg.seed, or shell_seed(): src/main/game.c), as HD's randomize() leaves
 *     the seed arbitrary: the front end's draws never reach a level. front_seed is the intro's randomize() seed (the
 *     traces' random_set_seed(SEED): tools/tracer.py; 0 on the cabinet, PLAY=1: shell_seed() at each intro).
 *   - globals: oGlobals' Create (scrClearGlobals and the HD globals) runs in rIntro; a game start runs
 *     gen_new_game (src/game: scrClearGlobals' part) itself, so nothing the intro sets reaches a level either.
 *
 * Attract cycle (HD has none: its title waits for the player): rIntro (as HD: the story, oPDummy3's walk, then
 * room_goto(rTitle)), rTitle for FRONT_TITLE_STEPS, rHighscores for FRONT_SCORES_STEPS, then rIntro again.
 *
 * The ending (a game's last rooms, "the ending" below): rEnd .. rCredits2 with the game's globals; checked against the
 * runner: build/trace/g_end_win_s7 (tests/routes/end_win.txt), every record equal (tests/game/host.c HOST_DUMP). */
#include "pint.h"
#include "inst.h"
#include "gen.h"
#include "fronttables.h"
#include "front.h"
#include "pcol.h"
#include "hud.h"
#include "sndgame.h"
#include "shell.h"
#include "pcontent.h"                               /* pitems_world(4003): the ball and chain */

#define FRONT_TITLE_STEPS  900
#define FRONT_SCORES_STEPS 450
#ifndef R_rIntro
#define R_rIntro 1                                /* the names file's room index (pint.h lists the play rooms) */
#endif

uint8_t front_on;
int16_t front_view_obj = -1, front_hborder = 160;
uint32_t front_seed = 1;                          /* oIntro's randomize(); 0: shell_seed() (the cabinet) */
uint8_t front_new;
static int room_steps;
static int16_t lpos[PIN_MAX];                     /* the room instance's place in its layer, by slot */
static int32_t lpos_id[PIN_MAX];                  /* ... while the slot holds that instance */
void (*front_rec_cb)(int phase);

/* oIntro's instance variables (one instance) */
static struct {
    int i;                                        /* its instance (-1 none) */
    uint8_t fadeIn, fadeOut;
    double fadeLevel;
    int drawStatus;
    int8_t str1, str2, str3;                      /* which line of each set (0..7) */
} INTRO;

/* ---- the ending (docs/GAMELOOP.md item 6): rEnd, rEnd2, rEnd3, rCredits2 ---------------------------------------
   Entered from a game: rOlmec's oXEnd (oPlayer1 Other_7.gml :133 room_goto(rEnd), src/game prun.c room_change);
   oEnd's skip goes to rEnd3, oLavaSpray's Alarm_0 to rEnd2, oTreasureSil / oEnd2's skip to rEnd3, oEnd3's to
   rCredits2, and oCredits2's room_goto(rHighscores) ends the game (front_room: the play loop returns the room, src/main
   starts the attract cycle there). Skips: checkAttackPressed / checkStartPressed (the keyboard's Enter / Escape: no
   keyboard on the cabinet); oCredits2's: checkAttack (held). global.display_w 320, global.room_offset 0 (4:3) */
static uint8_t front_ending;                      /* the ending's rooms run in a game */
enum { PD_TRANSITION = 0, PD_END = 2, PD_LAVA = 3, PD_STOPPED = 99 };   /* oPDummy Create_0.gml :4 */
enum { PD2_DROP, PD2_STUNNED, PD2_GETUP, PD2_JUMPING };                 /* oPDummy2 Create_0.gml :4 */

/* the instance variables of oEnd, oEnd3, oCredits2 (one instance each) */
static struct { int32_t timer; uint8_t shakeToggle; } END1;
static struct { int i; int32_t drawStatus, moneyCount; uint8_t fadeOut, poop; double fadeLevel; } END3;
static struct { int i; int32_t drawStatus; uint8_t fadeIn, fadeOut, scrollStart, scrolling; double fadeLevel; } CRED;

static int ending_room(int r) { return r == R_rEnd || r == R_rEnd2 || r == R_rEnd3 || r == R_rCredits2; }
static int skip_pressed(void) { return (GP.pressed & (K_ATTACK | K_START)) != 0; }
static void skip_clear(void) { GP.pressed &= (uint16_t)~(K_ATTACK | K_START); }   /* gamepad.<key>Pressed = false */
/* the explorer's sprite, or the damsel's / the tunnel man's (global.isDamsel / isTunnelMan) */
static int chr_spr(int s, int damsel, int tunnel) { return G.isDamsel ? damsel : G.isTunnelMan ? tunnel : s; }
static int is_exit_spr(int s) { return s == GSPR_sPExit || s == GSPR_sDamselExit || s == GSPR_sTunnelExit; }
/* with (obj) visible = v / scroll = v (scroll: PE armed); obj.yVel = v (an assignment through an object index sets
   every instance's) */
static void with_visible(int obj, int v)
{
    int k;
    for (k = 0; k < PW.n; k++)
        if (PX(k).alive && obj_is(PX(k).obj, obj)) pin_setvisible(&PX(k), v);
}
static void with_scroll(int obj, int v)
{
    int k;
    for (k = 0; k < PW.n; k++)
        if (PX(k).alive && obj_is(PX(k).obj, obj)) PE(&PX(k))->armed = (uint8_t)v;
}
static void all_yvel(int obj, num v)
{
    int k;
    for (k = 0; k < PW.n; k++)
        if (PX(k).alive && obj_is(PX(k).obj, obj)) PE(&PX(k))->yVel = v;
}
static void all_status(int obj, int v)
{
    int k;
    for (k = 0; k < PW.n; k++)
        if (PX(k).alive && obj_is(PX(k).obj, obj)) PE(&PX(k))->status = (int16_t)v;
}
static void move_x(struct pin *p, double dx) { pin_setx(p, (pos)(PTOD(p->x) + dx)); }
static void move_y(struct pin *p, double dy) { pin_sety(p, (pos)(PTOD(p->y) + dy)); }
static double dabs(double v) { return v < 0 ? -v : v; }   /* abs() */

static const char *const intro_str1[8] = {
    "AS THE MOON BURNED BRIGHT ABOVE,", "WITH THE DESERT STRETCHING BEHIND ME,", "AFTER I DOUBLE-CHECKED MY MAP,",
    "MY LIPS CRACKED AND COVERED IN SAND,", "WITH FATE GUIDING MY EVERY MOVE,", "PUTTING THE FADED PHOTO IN MY POCKET,",
    "AS I RECALLED MY FATHER'S LAST WORDS,", "MY MEMORY SLIPPING AWAY FROM ME,",
};
static const char *const intro_str2[8] = {
    "I STRODE VALIANTLY TOWARD MY DESTINY,", "I SQUEEZED THE WHIP AT MY SIDE,", "I DRAINED THE REST OF MY CANTEEN,",
    "I SPOTTED THE CAVE'S ENTRANCE,", "I FURROWED MY BROW,", "I PAID MY BEDOUIN GUIDE,", "I DISMOUNTED MY CAMEL,",
    "I SQUINTED INTO THE DARKNESS,",
};
static const char *const intro_str3[8] = {
    "AND THOUGHT OF HER ONE LAST TIME.", "AND HELD MY HAT AGAINST THE WIND.", "AND A COLD CHILL TOOK HOLD OF ME.",
    "AND WONDERED HOW LONG I'D BE BELOW.", "AND VOWED TO RETURN VICTORIOUS.", "AND SWORE I HEARD VOICES UP AHEAD.",
    "AND FELT THE GODS SMILING UPON ME.", "AND THAT'S WHEN IT ALL STARTED.",
};

/* round(random_range(1, 8)) - 1: random_range(a, b) = a + random(b - a), one draw; round() half to even */
static int8_t intro_pick(void) { return (int8_t)(dround(1.0 + prandom(7.0)) - 1); }

/* objects/oTitle/Create_0.gml, the part that makes instances (run in the room's Create order, in the generator's
   world: the bricks' Create events draw from the RNG). global.firstLaunch / titleStart: a cabinet always starts at
   titleStart 0 (the boot path: the flare and oPDummy4); the high scores and the tunnel man's progress from the
   EEPROM (src/shell: SH.hs, SH.g) */
static void title_create_w(void)
{
    int32_t t1 = SH.g.tunnel1, t2 = SH.g.tunnel2;
    G.lake = 0;                                                                /* :1 */
    G.cemetary = 0;
    if (t1 == 0 || (t1 > 0 && t2 == 0)) {                                      /* :43 */
        instance_create(352, 96, OBJ_oLadderOrange);
        instance_create(352, 96 + 16, OBJ_oLadderTop);
        instance_create(352, 96 + 32, OBJ_oLadderOrange);
        instance_create(352, 96 + 48, OBJ_oLadderOrange);
        instance_create(352, 96 + 64, OBJ_oLadderOrange);
        instance_create(352, 96 + 80, OBJ_oLadderOrange);
        if (t2 == 0) {
            instance_create(128, 112, OBJ_oLevel9Sign);
            instance_create(144, 128, OBJ_oXShortcut9);
            if (t1 == 0) {
                instance_create(192, 112, OBJ_oLevel13Sign);
                instance_create(208, 128, OBJ_oXShortcut13);
            }
        }
    } else {                                                                   /* :65 */
        static const int16_t b[][2] = { { 304, 112 }, { 288, 112 }, { 272, 112 }, { 256, 128 }, { 256, 32 },
            { 256, 48 }, { 256, 64 }, { 256, 80 }, { 256, 96 }, { 256, 112 }, { 272, 48 }, { 272, 64 }, { 272, 80 },
            { 272, 96 }, { 288, 96 }, { 304, 96 }, { 320, 96 }, { 336, 96 } };
        int k, cx, cy, n = 0;
        int16_t w[64];
        for (k = 0; k < W.n && n < 64; k++)                                    /* with (oBrickSmooth): newest first, */
            if (W.in[k].alive && W.in[k].obj == OBJ_oBrickSmooth) w[n++] = (int16_t)k;   /* two: oldest first */
        for (k = 0; k < n; k++) instance_destroy(w[n == 2 ? k : n - 1 - k]);
        for (k = 0; k < (int)(sizeof b / sizeof b[0]); k++) instance_create(b[k][0], b[k][1], OBJ_oBrick);
        for (cx = 192; cx < 256; cx += 16)
            for (cy = 32; cy < 160; cy += 16) instance_create(cx, cy, OBJ_oBrick);
    }
    if (SH.hs.value[HS_MONEY] >= 200000 && SH.hs.value[HS_TIME] > 0 && SH.hs.value[HS_TIME] <= 600 &&
        SH.hs.value[HS_KILLS] >= 120 && SH.hs.value[HS_SAVES] >= 8) {                       /* :103 */
        instance_create(32, 112, OBJ_oMultiTrophy);
        instance_create(32, 128, OBJ_oXChange2);
    } else
        instance_create(32 + 8, 128 + 8, OBJ_oTunnelMan);
}

/* objects/oHighscores/Create_0.gml (two instances in rHighscores: each runs it): the globals, the trophies for the
   EEPROM's scores (src/shell: SH.hs), the "new" marks (front_new: after a game; none at a cabinet's boot),
   global.scoresStart 0 */
static void scores_create_w(void)
{
    int32_t tMoney = SH.hs.value[HS_MONEY], tTime = SH.hs.value[HS_TIME], tKills = SH.hs.value[HS_KILLS];
    int32_t tSaves = SH.hs.value[HS_SAVES];
    int t;
    G.currLevel = 1;                                                           /* :13 */
    G.darkLevel = 0;
    G.snakePit = 0;
    PG.arrows = 0;
    PG.hasJordans = 0;
    if (tMoney >= 50000) {                                                     /* :37 */
        t = instance_create(32, 32, OBJ_oTrophy);
        if (tMoney >= 200000) { inst_set_sprite(t, GSPR_sGoldTrophy); instance_create(32, 48, OBJ_oXSun); }
        else if (tMoney >= 100000) inst_set_sprite(t, GSPR_sSilverTrophy);
        else inst_set_sprite(t, GSPR_sBronzeTrophy);
    }
    if (tTime > 0 && tTime < 960) {                                            /* :54 */
        t = instance_create(64, 64, OBJ_oTrophy);
        inst_set_sprite(t, GSPR_sBronzeTrophy);
        if (tTime <= 720) inst_set_sprite(t, GSPR_sSilverTrophy);
        if (tTime <= 600) { inst_set_sprite(t, GSPR_sGoldTrophy); instance_create(64, 80, OBJ_oXMoon); }
    }
    if (tKills >= 80) {                                                        /* :72 */
        t = instance_create(32, 96, OBJ_oTrophy);
        if (tKills >= 120) { inst_set_sprite(t, GSPR_sGoldTrophy); instance_create(32, 112, OBJ_oXStars); }
        else if (tKills >= 100) inst_set_sprite(t, GSPR_sSilverTrophy);
        else inst_set_sprite(t, GSPR_sBronzeTrophy);
    }
    if (tSaves >= 4) {                                                         /* :89 */
        t = instance_create(64, 128, OBJ_oTrophy);
        if (tSaves >= 8) { inst_set_sprite(t, GSPR_sGoldTrophy); instance_create(64, 144, OBJ_oXChange); }
        else if (tSaves >= 6) inst_set_sprite(t, GSPR_sSilverTrophy);
        else inst_set_sprite(t, GSPR_sBronzeTrophy);
    }
    if (front_new & HS_NEW_MONEY) instance_create(272, 48, OBJ_oNew);         /* :135 */
    if (front_new & HS_NEW_KILLS) instance_create(272, 64, OBJ_oNew);
    if (front_new & HS_NEW_SAVES) instance_create(272, 80, OBJ_oNew);
    if (front_new & HS_NEW_TIME) instance_create(272, 96, OBJ_oNew);
}

/* ---- room loading (as play_transition_start: the instances into W, the Create events that need the
   generator's world (walls, RNG order), then PW and the other Create events) ---------------------------------- */
static const struct froom *room_table(int room)
{
    switch (room) {
    case R_rIntro: return &froom_rIntro;
    case R_rTitle: return &froom_rTitle;
    case R_rHighscores: return &froom_rHighscores;
    case R_rEnd: return &froom_rEnd;
    case R_rEnd2: return &froom_rEnd2;
    case R_rEnd3: return &froom_rEnd3;
    case R_rCredits2: return &froom_rCredits2;
    default: return 0;
    }
}

static uint8_t w_alive[PIN_MAX];                    /* the room instances alive after the generator-phase Creates */

int front_room(int room)
{
    const struct froom *rm = room_table(room);
    int k, n0;
    if (!rm) return 0;
    if (front_ending && room == R_rHighscores) {                               /* oCredits2's room_goto: the game */
        front_ending = 0;                                                      /* ends there (src/main starts the */
        front_on = 0;                                                          /* attract cycle in rHighscores) */
        return 0;
    }
    if (!front_on) front_on = front_ending = 1;                                /* rEnd from the game (prun.c) */
    if (PW.room == R_rEnd) PG.shake = 0;                                       /* objects/oEnd/Other_5.gml (Room End) */
    inst_hook = pcol_gen_hook;                                                 /* the collision tree follows (as
                                                                                  play_level_start) */
    inst_reset(PW.next_id);
    gntiles = 0;
    for (k = 0; k < rm->nt; k++)                                               /* the room's own tile layers */
        tile_add(rm->t[k].bg, rm->t[k].left, rm->t[k].top, rm->t[k].w, rm->t[k].h, rm->t[k].x, rm->t[k].y,
                 rm->t[k].depth);
    gen_untranslated = 0;
    G.roomW = rm->w;
    G.roomH = rm->h;
    gen_not_level = 1;
    for (k = 0; k < rm->n; k++)
        W.in[inst_add(rm->in[k].obj, rm->in[k].x, rm->in[k].y, rm->in[k].id)].depth = rm->in[k].depth;
    /* the Create events in creation order, where they touch the RNG or the walls */
    for (k = 0; k < rm->n; k++) {
        int o = rm->in[k].obj;
        switch (o) {
        case OBJ_oBrick: case OBJ_oHardBlock: case OBJ_oBrickSmooth:
        case OBJ_oTemple: case OBJ_oLava: case OBJ_oLavaSolid: case OBJ_oEntrance:   /* (rEnd) */
            gen_create_event(k);
            break;
        case OBJ_oBricks:                                                      /* objects/oBricks/Create_0.gml */
            scrSetupWalls(224);
            inst_destroyed(k);
            break;
        case OBJ_oDark:                                                        /* objects/oDark/Create_0.gml */
            (void)RAND(1, 100);                                                /* :2 (rIntro: the value unused) */
            break;
        case OBJ_oDesert:                                                      /* objects/oDesert/Create_0.gml */
            if (room == R_rIntro) inst_set_sprite(k, GSPR_sDesertNight);
            break;
        case OBJ_oDesertTop:
            if (room == R_rIntro) inst_set_sprite(k, GSPR_sDesertTopNight);
            break;
        case OBJ_oDesert2:
            if (room == R_rIntro) inst_set_sprite(k, GSPR_sDesertNight2);
            break;
        case OBJ_oTitle:                                                       /* objects/oTitle/Create_0.gml */
            title_create_w();
            break;
        case OBJ_oHighscores:                                                  /* objects/oHighscores/Create_0.gml */
            scores_create_w();
            break;
        case OBJ_oIntro:                                                       /* objects/oIntro/Create_0.gml */
            rng_seed(&g_rng, front_seed ? front_seed : shell_seed());          /* :6 randomize() */
            INTRO.str1 = intro_pick();                                         /* :10 */
            INTRO.str2 = intro_pick();                                         /* :23 */
            INTRO.str3 = intro_pick();                                         /* :36 */
            break;
        default:
            break;
        }
    }
    gen_not_level = 0;
    pw_reset();
    PW.room = (int16_t)room;
    PW.room_w = rm->w;
    PW.room_h = rm->h;
    PW.next_id = W.next_id;
    PW.xview = PW.yview = 0;
    PW.vborder = 160;
    PW.vdirty = 0;
    PW.step = 0;
    PL.idx = NOONE;
    for (k = 0; k < rm->n; k++) w_alive[k] = W.in[k].alive;                  /* (PX overwrites W below) */
    for (k = 0; k < W.n; k++) {
        const struct inst gk = W.in[k];                /* PX(i), i <= k, is the same memory (play.h) */
        const struct inst *g = &gk;
        int i;
        if (!g->alive) continue;
        i = pin_add(g->obj, PI(g->x), PI(g->y), g->id);
        pin_setspr(&PX(i), g->spr);
        pin_setdepth(&PX(i), g->depth);
        if (k < rm->n && rm->in[k].xscale != 1.0f) pin_setxscale(&PX(i), rm->in[k].xscale);
        if (obj_is(g->obj, OBJ_oSolid)) PX(i).invincible = (g->flags & IF_INVINCIBLE) != 0;
        if (g->obj == OBJ_oLava) {                     /* rEnd: oLava's Create variables (the generator drew */
            play_gen_inst = g;                         /* spurtTime) */
            pobj_init_from_gen(i);
            play_gen_inst = 0;
        }
    }
    pin_add(OBJ_oGamepad, 0, 0, RUNNER_ID_GAMEPAD);                                       /* the tracer's (persistent) */
    n0 = PW.n;
    for (k = 0; k < PIN_MAX; k++) lpos_id[k] = -1;
    {
        int j = 0;
        for (k = 0; k < rm->n && j < n0; k++) {
            if (!w_alive[k]) continue;                                         /* (destroyed in a Create) */
            while (j < n0 && PX(j).id != rm->in[k].id) j++;
            if (j < n0) { lpos[j] = rm->in[k].lpos; lpos_id[j] = rm->in[k].id; }
        }
    }
    INTRO.i = END3.i = CRED.i = -1;
    for (k = 0; k < n0; k++) {                                                 /* the other Create events */
        struct pin *p = &PX(k);
        switch (p->obj) {
        case OBJ_oIntro:
            INTRO.i = k;
            INTRO.fadeIn = INTRO.fadeOut = 0;                                  /* :1 */
            INTRO.fadeLevel = 1;
            INTRO.drawStatus = 0;                                              /* :8 */
            PE(p)->alarm[11] = 20;
            break;
        case OBJ_oTitle:                                                       /* Create_0 :114 titleStart 0 */
            PE(p)->state = 0;
            PE(p)->alarm[0] = 50;
            PW.xview = 320;                                                    /* :25 __view_set(XView, 320) */
            break;
        case OBJ_oTunnelMan:                                                   /* objects/oTunnelMan/Create_0.gml */
            PE(p)->counter = 0;                                                /* talk */
            pin_set_sprite(k, GSPR_sTunnelManRight);                           /* :8 rTitle */
            break;
        case OBJ_oBatIntro:                                                    /* objects/oBatIntro/Create_0.gml */
            PE(p)->status = 0;
            PE(p)->xVel = PE(p)->yVel = PE(p)->yAcc = 0;
            break;
        case OBJ_oIntroBG: case OBJ_oMoon:                                     /* Create_0: xOff = 0 */
            PE(p)->px = 0;                                                     /* (xOff kept in px) */
            break;
        case OBJ_oPlayer1:                                                     /* a room's own player (rHighscores) */
            pl_init_from_gen(k);
            break;
        case OBJ_oPushBlock:
            pobj_init_from_gen(k);
            break;
        case OBJ_oButtonHighscore:                                             /* Create_0: pushed = false */
            PE(p)->armed = 0;
            PE(p)->counter = 0;
            break;
        case OBJ_oGame:                                                        /* objects/oGame/Create_0.gml */
            PGAME.drawStatus = 0;
            PGAME.moneyCount = 0;
            PGAME.paused = 0;
            play_time = 1;
            break;
        /* the ending's rooms */
        case OBJ_oPDummy:                                                      /* objects/oPDummy/Create_0.gml (rEnd) */
            p->type = T_NONE;                                                  /* oDrawnSprite */
            PE(p)->status = PD_TRANSITION;
            PE(p)->yVel = 0;
            pin_set_sprite(k, chr_spr(GSPR_sRunLeft, GSPR_sDamselRunL, GSPR_sTunnelRunL));
            PE(p)->facing = 1;                                                 /* RIGHT */
            break;
        case OBJ_oEnd: {                                                       /* objects/oEnd/Create_0.gml */
            int d = instance_first_p(OBJ_oPDummy);
            END1.shakeToggle = 0;
            if (d != NOONE) PE(&PX(d))->status = PD_END;                       /* :2 oPDummy.status = 2 */
            END1.timer = 0;
            if (G.kaliPunish >= 2) pitems_world(4003, k, 0);                   /* :6 the ball and chain (oBall2 at */
            snd_stop_music();                                                  /* oPDummy.y + 2, oChain2 x 4) */
            break;
        }
        case OBJ_oEndPlat:                                                     /* objects/oEndPlat/Create_0.gml */
            PE(p)->yVel = 0;
            break;
        case OBJ_oEntrance:                                                    /* (the generator's Create: etype) */
            PE(p)->etype = EX_EXIT;
            break;
        case OBJ_oEnd2:                                                        /* objects/oEnd2/Create_0.gml */
            PE(p)->alarm[0] = 50;
            PE(p)->alarm[2] = 10;
            break;
        case OBJ_oEnd3:                                                        /* objects/oEnd3/Create_0.gml */
            END3.i = k;
            END3.drawStatus = 0;
            END3.moneyCount = 0;
            END3.fadeOut = 0;
            END3.fadeLevel = 0;
            END3.poop = 0;
            PE(p)->alarm[0] = 50;
            with_visible(OBJ_oMenu, 0);                                        /* :10 */
            break;
        case OBJ_oBGEnd3:                                                      /* objects/oBGEnd3/Create_0.gml */
            if (room != R_rEnd3) {
                PE(p)->px = N(-48);                                            /* xOff (kept in px) */
                pin_setx(p, PI(-48));
            }
            break;
        case OBJ_oCredits2:                                                    /* objects/oCredits2/Create_0.gml */
            CRED.i = k;
            CRED.fadeIn = 1;
            CRED.fadeOut = 0;
            CRED.fadeLevel = 1;
            CRED.drawStatus = 0;
            CRED.scrollStart = 0;
            CRED.scrolling = 0;
            snd_stop_music();
            break;
        case OBJ_oDesertScroll: case OBJ_oDesertScroll2: case OBJ_oDesertTopScroll:   /* Create_0: scroll */
            p->type = T_NONE;
            PE(p)->armed = 0;
            break;
        default:
            break;
        }
    }
    for (k = 0; k < rm->n; k++)                                                /* instance creation code */
        if (rm->in[k].cc == FRONT_CC_rIntro_0_Create)
            PE(&PX(k))->px = 400;                                              /* rIntro: xOff = 400 */
        else if (rm->in[k].cc == FRONT_CC_rCredits2_0_Create) {                /* rCredits2's second oBGEnd3 */
            PE(&PX(k))->px = 320;                                              /* xOff = 320 */
            pin_setx(&PX(k), PI(320));                                         /* x = 320 */
        }
    front_view_obj = room == R_rIntro ? OBJ_oPDummy3 : -1;
    front_hborder = 160;
    room_steps = 0;
    play_rooms_entered++;
    PW.room_new = 1;
    pcol_load_done();                                                          /* the collision tree, as a level */
    return 1;
}

/* ---- events ------------------------------------------------------------------------------------------------ */
static double vx_read(void) { view_read(); return (double)PW.xview; }

/* objects/oIntro: Step_0, Alarm_8..11 */
static int intro_ev(int ev, int i, int a)
{
    if (ev == FEV_STEP) {
        /* :1-14 the skip (attract: no controls) */
        if (INTRO.fadeIn) {                                                    /* :16 */
            INTRO.drawStatus = -1;
            if (DGT(INTRO.fadeLevel, 0)) INTRO.fadeLevel -= 0.1;     /* GML compares within 1e-5 */
            else {
                INTRO.fadeIn = 0;
                if (!instance_exists_p(OBJ_oPDummy3)) pin_create(PI(-32), PI(184), OBJ_oPDummy3);
            }
        } else if (INTRO.fadeOut) {                                            /* :26 */
            if (DLT(INTRO.fadeLevel, 1)) INTRO.fadeLevel += 0.1;
            else {
                G.gameStart = 0;
                play_goto_room = R_rTitle;
            }
        }
        return 1;
    }
    if (ev == FEV_ALARM) {
        struct pin *p = &PX(i);
        if (INTRO.fadeIn) return 1;
        switch (a) {
        case 11: if (INTRO.drawStatus >= 0) INTRO.drawStatus = 1; PE(p)->alarm[10] = 80; break;
        case 10: if (INTRO.drawStatus >= 0) INTRO.drawStatus = 2; PE(p)->alarm[9] = 80; break;
        case 9: if (INTRO.drawStatus >= 0) INTRO.drawStatus = 3; PE(p)->alarm[8] = 80; break;
        case 8: INTRO.drawStatus = -1; INTRO.fadeIn = 1; break;
        }
        return 1;
    }
    return ev == FEV_CREATE || ev == FEV_DRAW;
}

/* objects/oPDummy3 (dummy actor for the intro): Create_0, Step_0, Alarm_0, Alarm_2, Draw_0 (image_xscale) */
enum { PD3_LEFT = 0, PD3_RIGHT = 1 };
static int pdummy3_ev(int ev, int i, int a)
{
    struct pin *p = &PX(i);
    switch (ev) {
    case FEV_CREATE:                                                           /* oDrawnSprite: type, blinkToggle */
        p->type = T_NONE;
        pin_setispd(p, (img_t)0.6);
        PE(p)->counter = 0;                                                    /* climbSndToggle */
        PE(p)->status = 0;                                                     /* TRANSITION */
        PE(p)->yVel = 0;
        PE(p)->facing = PD3_RIGHT;
        return 1;
    case FEV_STEP:
        pin_sety(p, (pos)(PTOD(p->y) + NTOD(PE(p)->yVel)));                    /* :1 */
        if (PE(p)->status == 0) {                                              /* :3 TRANSITION */
            if (DGE(PTOD(p->x), 904)) {
                pin_set_sprite(i, GSPR_sDuckLeft);
                PE(p)->status = 1;
            } else pin_setx(p, (pos)(PTOD(p->x) + 2));
        } else if (PE(p)->status == 1) {                                       /* :13 ROPEDROP */
            PE(p)->alarm[0] = 20;
            PE(p)->status += 1;
        } else if (PE(p)->status == 3) {                                       /* :18 */
            pin_set_sprite(i, GSPR_sRunLeft);
            if (DGE(PTOD(p->x), 920)) {
                pin_set_sprite(i, GSPR_sClimbUp3);
                PE(p)->status = 4;
            } else pin_setx(p, (pos)(PTOD(p->x) + 2));
        } else if (PE(p)->status == 4) {                                       /* :28 */
            if (DGE(PTOD(p->y), 256)) INTRO.fadeOut = 1;
            else pin_sety(p, (pos)(PTOD(p->y) + 2));
            if (PE(p)->alarm[2] < 1) PE(p)->alarm[2] = 8;
        }
        return 1;
    case FEV_ALARM:
        if (a == 0) {                                                          /* Alarm_0 */
            int r = pin_create((pos)(PTOD(p->x) + 16), p->y, OBJ_oRopeThrow);
            p = &PX(i);
            PE(&PX(r))->falling = 1;
            PE(&PX(r))->armed = 1;
            PE(p)->alarm[1] = 50;
            PE(p)->status = 3;
            snd_play(SND_xthrow);
        } else if (a == 2) {                                                   /* Alarm_2 */
            snd_play(PE(p)->counter ? SND_xclimb1 : SND_xclimb2);
            PE(p)->counter = !PE(p)->counter;
        }
        return 1;
    case FEV_DRAW:                                                             /* Draw_0 :1 */
        pin_setxscale(p, PE(p)->facing == PD3_RIGHT ? -1 : 1);
        return 1;
    default:
        return 0;
    }
}

/* objects/oBatIntro/Step_0.gml */
static void batintro_step(int i)
{
    struct pin *p = &PX(i);
    pin_setx(p, (pos)(PTOD(p->x) + NTOD(PE(p)->xVel)));
    pin_sety(p, (pos)(PTOD(p->y) + NTOD(PE(p)->yVel)));
    if (PE(p)->status == 0 && DLT(PTOD(p->x), vx_read() + 320 + 16)) {             /* :7 */
        PE(p)->status = 1;
        PE(p)->xVel = ND(-prandom(3) - 2);
        PE(p)->yVel = ND(-prandom(1));
        PE(p)->yAcc = ND(-prandom(1) * 0.2);
        snd_play(SND_xbat);
    }
}

/* objects/oIntroBG/Step_0.gml, objects/oMoon/Step_0.gml (rIntro; the rCredits1 branch: no oCredits1 here) */
static void parallax_step(int i, double rate, double at)
{
    struct pin *p = &PX(i);
    double vx = vx_read();
    if (!DEQ(vx, 0) && DLT(vx, 960 - 320)) PE(p)->px = NTOD(PE(p)->px) - rate;
    pin_setx(p, (pos)(vx + at + NTOD(PE(p)->px)));
}

/* objects/oTitle: Step_0 (no controls: only the view's scroll toward the player), Alarm_0..3 */
static int title_ev(int ev, int i, int a)
{
    struct pin *p = &PX(i);
    if (ev == FEV_STEP) {
        int pl = instance_first_p(OBJ_oPlayer1);
        if (pl != NOONE) {                                                     /* :21 */
            double px = PTOD(PX(pl).x);
            if (DLE(px, 320) && PW.xview > 0) { PW.xview -= 8; PW.vdirty = 1; }
            if (DGT(px, 320) && PW.xview < 320) { PW.xview += 8; PW.vdirty = 1; }
        }
        return 1;
    }
    if (ev == FEV_ALARM) {
        switch (a) {
        case 0: PE(p)->state = 1; PE(p)->alarm[1] = 100; break;                /* Alarm_0 */
        case 1: PE(p)->state = 2; PE(p)->alarm[2] = 70; break;                 /* Alarm_1 */
        case 2:                                                                /* Alarm_2 */
            PE(p)->state = 3;
            pin_create(PI(320 + 280), PI(-8), OBJ_oFlare);
            snd_play(SND_xignite);
            PE(&PX(i))->alarm[3] = 50;
            break;
        case 3:                                                                /* Alarm_3 */
            pin_create(PI(320 + 280), PI(-32), OBJ_oPDummy4);
            snd_audio_play(SND_mTitle, 10, 1);
            break;
        }
        return 1;
    }
    return ev == FEV_CREATE || ev == FEV_DRAW;   /* Draw_0: oTitle.darkness, which nothing draws; Draw_64: the
                                                    shortcut house's line, when the player stands left of 256 */
}

/* objects/oFlare: Create_0, Step_0, Alarm_0 (the oItem parts: src/game's create_item / item_step) */
static int flare_ev(int ev, int i, int a)
{
    struct pin *p = &PX(i);
    switch (ev) {
    case FEV_CREATE:
        create_item(p);                                                        /* action_inherited (oItem) */
        pin_setispd(p, (img_t)0.3);
        p->type = T_FLARE;
        PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;               /* makeActive */
        setCollisionBounds(i, -4, -4, 4, 4);
        PE(p)->xVel = PE(p)->yVel = 0;
        PE(p)->grav = N(0.6);
        p->invincible = 1;
        PE(p)->bounce = 1;
        PE(p)->px = 0;                                                         /* distToPlayer */
        PE(p)->alarm[0] = 1;
        return 1;
    case FEV_STEP:
        item_step(i);                                                          /* action_inherited (oItem) */
        if (PX(i).alive && instance_exists_p(OBJ_oPlayer1))
            PE(&PX(i))->px = ND(distance_to_object_p(i, OBJ_oPlayer1));
        /* :6 oWater: none in the front rooms */
        return 1;
    case FEV_ALARM:
        if (a == 0) {                                                          /* Alarm_0 */
            /* instance_create(x + rand(0,3) - rand(0,3), y + rand(0,3) - rand(0,3), oFlareSpark): arguments
               right to left, each expression's operands left to right */
            int32_t y1 = RAND(0, 3), y2 = RAND(0, 3), x1 = RAND(0, 3), x2 = RAND(0, 3);
            pin_create((pos)(PTOD(p->x) + x1 - x2), (pos)(PTOD(p->y) + y1 - y2), OBJ_oFlareSpark);
            PE(&PX(i))->alarm[0] = 2;
        }
        return 1;
    default:
        return 0;
    }
}

/* objects/oFlareSpark: Create_0, Step_0, Other_7 (Animation End) */
static int flarespark_ev(int ev, int i)
{
    struct pin *p = &PX(i);
    switch (ev) {
    case FEV_CREATE:
        p->type = T_NONE;                                                      /* oDrawnSprite */
        PE(p)->yVel = N(-0.1);
        PE(p)->yAcc = N(0.1);
        pin_setispd(p, (img_t)0.4);
        return 1;
    case FEV_STEP:
        pin_sety(p, (pos)(PTOD(p->y) + NTOD(PE(p)->yVel)));
        if (collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oSolid, 0, NOONE) != NOONE) pin_destroy(i);
        return 1;
    case FEV_ANIMEND:
        pin_destroy(i);
        return 1;
    default:
        return 0;
    }
}

/* objects/oPDummy4 (dummy actor for the title screen): Create_0, Step_0, Alarm_2 */
static int pdummy4_ev(int ev, int i, int a)
{
    struct pin *p = &PX(i);
    switch (ev) {
    case FEV_CREATE:
        p->type = T_NONE;                                                      /* oDrawnSprite */
        pin_setispd(p, (img_t)0.6);
        PE(p)->status = 0;
        PE(p)->counter = 0;                                                    /* climbSndToggle */
        PE(p)->xVel = PE(p)->yVel = 0;
        return 1;
    case FEV_STEP:
        pin_setxy(p, (pos)(PTOD(p->x) + NTOD(PE(p)->xVel)), (pos)(PTOD(p->y) + NTOD(PE(p)->yVel)));
        if (PE(p)->status == 0) {
            if (DGE(PTOD(p->y), 160)) {                                        /* :6 */
                pin_set_sprite(i, GSPR_sFallLeft);
                PE(p)->yVel = N(4);
                if (NLT(PE(p)->yVel, N(6))) PE(p)->yVel += N(0.2);
            } else {
                pin_set_sprite(i, GSPR_sClimbUp3);
                PE(p)->yVel = N(2);
                if (PE(p)->alarm[2] < 1) PE(p)->alarm[2] = 8;
            }
            if (DGE(PTOD(p->y), 176 + 8)) {                                    /* :19 */
                int pl;
                pin_sety(p, PI(176 + 8));
                pl = pin_create(p->x, p->y, OBJ_oPlayer1);
                PL.facing = 18;                                                /* LEFT */
                (void)pl;
                pin_destroy(i);
            }
        } else {
            pin_create(p->x, p->y, OBJ_oPlayer1);
            PL.facing = 18;
            pin_destroy(i);
        }
        return 1;
    case FEV_ALARM:
        if (a == 2) {
            snd_play(PE(p)->counter ? SND_xclimb1 : SND_xclimb2);
            PE(p)->counter = !PE(p)->counter;
        }
        return 1;
    default:
        return 0;
    }
}

/* objects/oHighscores/Step_0.gml: the view (global.shake: only the reset button sets it) */
static void scores_step(void)
{
    if (PG.shake > 0) {                                                        /* :3 */
        static uint8_t toggle;
        if (toggle) view_set_y(RAND(1, 8));
        else { PW.xview = 0; PW.vdirty = 1; }
        toggle = !toggle;
        PG.shake -= 1;
    } else {
        PW.xview = 0;
        PW.yview = 0;
        PW.vdirty = 1;
    }
}

/* objects/oButtonHighscore/Step_0.gml (attract: nothing reaches it; the push and the reset are translated for
   completeness up to scrResetHighscores, which a cabinet does in its settings screen instead) */
static void button_step(int i)
{
    struct pin *p = &PX(i);
    double x = PTOD(p->x), y = PTOD(p->y);
    PE(p)->armed = 0;                                                          /* :1 pushed = false */
    if (collision_rect_p(x + 2, y + 11, x + 13, y + 15, OBJ_oSolid, 0, NOONE) != NOONE) {
        if (!PE(p)->armed) { PE(p)->counter = 20; snd_play(SND_xclick); }
        PE(p)->armed = 1;
    } else PE(p)->armed = 0;
    if (PE(p)->armed) {
        if (PE(p)->counter > 0) PE(p)->counter -= 1;
        if (PE(p)->counter == 1) { PG.shake = 60; snd_play(SND_xthump); }
        pin_set_sprite(i, GSPR_sButtonPushed);
    } else pin_set_sprite(i, GSPR_sButton);
}


/* objects/oEnd/Step_0.gml */
static void end_step(void)
{
    END1.timer += 1;                                                           /* :1 */
    if (skip_pressed()) {                                                      /* :6 */
        if (END1.timer > 50) {
            skip_clear();
            G.gameStart = 0;
            play_goto_room = R_rEnd3;
        }
    }
    if (PG.shake > 0) {                                                        /* :17 shake the screen */
        if (END1.shakeToggle) { view_read(); view_set_y(PW.yview + RAND(1, 8)); }
        else view_set_y(0);
        END1.shakeToggle = !END1.shakeToggle;
        PG.shake -= 1;
    } else
        view_set_y(0);
    view_read();                                                               /* :29 */
    if (PW.xview < PW.room_w - 320) view_set_x(PW.xview + 1);
}

/* objects/oPDummy (rEnd): Step_0, Alarm_0..5, Other_7, Draw_0's image_xscale (src/game's ptrans_draw) */
static int pdummy_end_ev(int ev, int i, int a)
{
    struct pin *p = &PX(i);
    int k;
    switch (ev) {
    case FEV_STEP:
        move_y(p, NTOD(PE(p)->yVel));                                          /* :1 */
        if (PE(p)->status != PD_STOPPED &&                                     /* :3 */
            collision_point_p(PTOD(p->x) + 8, PTOD(p->y), OBJ_oDamselKiss, 0, NOONE) != NOONE) {
            int person = instance_nearest_p(PTOD(p->x) + 8, PTOD(p->y), OBJ_oDamselKiss);
            if (!PE(&PX(person))->trigger) {                                   /* not kissed */
                PE(p)->status = PD_STOPPED;
                PE(p)->xVel = 0;
                PE(p)->yVel = 0;
                pin_set_sprite(i, chr_spr(GSPR_sStandLeft, GSPR_sDamselLeft, GSPR_sTunnelLeft));
                pin_set_sprite(person, G.isDamsel ? GSPR_sPKissL : GSPR_sDamselKissL);
                PE(p)->alarm[5] = 30;
            }
        }
        if (instance_exists_p(OBJ_oTunnelMan)) {                               /* :24 */
            int person = instance_nearest_p(PTOD(p->x) + 8, PTOD(p->y), OBJ_oTunnelMan);
            if (PE(p)->status != PD_STOPPED &&
                collision_point_p(PTOD(p->x) + 8, PTOD(p->y), OBJ_oTunnelMan, 0, NOONE) != NOONE &&
                PE(&PX(person))->counter == 0) {                               /* talk (front.c: counter) */
                PE(p)->status = PD_STOPPED;
                PE(p)->xVel = 0;
                PE(p)->yVel = 0;
                pin_set_sprite(i, chr_spr(GSPR_sStandLeft, GSPR_sDamselLeft, GSPR_sTunnelLeft));
                PE(&PX(person))->counter = 1;
            }
        }
        if (PE(p)->status == PD_TRANSITION) {                                  /* :43 */
            if (DGE(PTOD(p->x), 280)) {
                if (!is_exit_spr(p->spr)) {
                    snd_play(SND_xsteps);
                    pin_set_sprite(i, chr_spr(GSPR_sPExit, GSPR_sDamselExit, GSPR_sTunnelExit));
                }
            } else move_x(p, 2);
        } else if (PE(p)->status == PD_END && p->spr != GSPR_sStandLeft && p->spr != GSPR_sDamselLeft &&
                   p->spr != GSPR_sTunnelLeft) {                               /* :57 */
            if (DGE(PTOD(p->x), 448 + 8)) {
                pin_set_sprite(i, chr_spr(GSPR_sStandLeft, GSPR_sDamselLeft, GSPR_sTunnelLeft));   /* stop */
                PE(p)->alarm[0] = 20;
            } else move_x(p, 2);
        } else if (PE(p)->status == PD_LAVA) {                                 /* :69 */
            PE(p)->alarm[3] = 50;
            PE(p)->status += 1;
        } else if (PE(p)->status == PD_LAVA + 1) {                             /* :74 */
            int e = instance_first_p(OBJ_oEndPlat);
            pin_create((pos)(PTOD(PX(e).x) + RAND(0, 80)), PI(192 + 32), OBJ_oBurn);
        }
        return 1;
    case FEV_ALARM:
        switch (a) {
        case 0: PE(p)->alarm[1] = 100; break;                                  /* Alarm_0 */
        case 1: {                                                              /* Alarm_1 */
            int c = instance_first_p(OBJ_oBigChest), t;
            pin_set_sprite(c, GSPR_sBigChestOpen);
            t = pin_create(PX(c).x, PX(c).y, OBJ_oBigTreasure);
            PE(&PX(t))->yVel = N(-4);
            PE(&PX(t))->xVel = N(-3);
            snd_play(SND_xclick);
            PE(&PX(i))->alarm[2] = 20;
            break;
        }
        case 2: PE(p)->status = PD_LAVA; break;                                /* Alarm_2 */
        case 3: {                                                              /* Alarm_3 */
            int e = instance_first_p(OBJ_oEndPlat);
            pin_create(PX(e).x, (pos)(PTOD(PX(e).y) + 30), OBJ_oLavaSpray);
            PG.shake = 9999;
            PE(&PX(i))->alarm[4] = 10;
            break;
        }
        case 4:                                                                /* Alarm_4 (if (oLavaSpray): an */
            for (k = 0; k < PW.n; k++)                                         /* object index, always true) */
                if (PX(k).alive && PX(k).obj == OBJ_oLavaSpray) PE(&PX(k))->yAcc = N(-0.1);
            break;
        case 5:                                                                /* Alarm_5 */
            PE(p)->status = PD_TRANSITION;
            pin_set_sprite(i, chr_spr(GSPR_sRunLeft, GSPR_sDamselRunL, GSPR_sTunnelRunL));
            break;
        }
        return 1;
    case FEV_ANIMEND:                                                          /* Other_7 */
        if (is_exit_spr(p->spr)) pin_destroy(i);
        return 1;
    default:
        return 0;                                                              /* Draw_0: ptrans_draw */
    }
}

/* objects/oBigTreasure: Create_0, Step_0 */
static void bigtreasure_create(int i)
{
    struct pin *p = &PX(i);
    p->type = T_OTHER;                                                         /* "Big Treasure" */
    PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;                 /* makeActive */
    setCollisionBounds(i, 0, 0, 32, 32);
    PE(p)->xVel = 0;
    PE(p)->yVel = 0;
    PE(p)->myGrav = N(0.6);
    PE(p)->trigger = 1;
}
static void bigtreasure_step(int i)
{
    struct pin *p = &PX(i);
    struct pin_ext *e = PE(p);
    move_x(p, NTOD(e->xVel));                                                  /* :3 */
    move_y(p, NTOD(e->yVel));
    if (DLT(NTOD(e->yVel), 6)) e->yVel += e->myGrav;                           /* :6 */
    if (isCollisionTop(i, 1) && DLT(NTOD(e->yVel), 0)) e->yVel = ND(-NTOD(e->yVel) * 0.8);   /* :11 */
    if (isCollisionLeft(i, 1) || isCollisionRight(i, 1)) e->xVel = ND(-NTOD(e->xVel) * 0.5);
    if (isCollisionBottom(i, 1)) {                                             /* :21 */
        if (DGT(NTOD(e->yVel), 5)) {
            int pf = pin_create((pos)(PTOD(p->x) + 16 - 4), (pos)(PTOD(p->y) + 30), OBJ_oPoof);
            PE(&PX(pf))->xVel = N(-0.4);
            pf = pin_create((pos)(PTOD(PX(i).x) + 16 + 4), (pos)(PTOD(PX(i).y) + 30), OBJ_oPoof);
            PE(&PX(pf))->xVel = N(0.4);
            snd_play(SND_xthump);
            p = &PX(i);
            e = PE(p);
        }
        if (DGT(NTOD(e->yVel), 1)) e->yVel = ND(-NTOD(e->yVel) * 0.5);        /* :33 bounce */
        else e->yVel = 0;
        if (DLT(dabs(NTOD(e->xVel)), 0.1)) e->xVel = 0;                       /* :40 friction */
        else if (!DEQ(dabs(NTOD(e->xVel)), 0)) e->xVel = ND(NTOD(e->xVel) * 0.3);
    }
    if (isCollisionBottom(i, 0) && DLT(dabs(NTOD(e->yVel)), 1)) {              /* :44 */
        move_y(p, -1);
        e->yVel = 0;
    }
}

/* objects/oLavaSpray: Step_0 (Create_0: yVel, yAcc, status 0), Alarm_0 */
static void lavaspray_step(int i)
{
    struct pin *p = &PX(i);
    struct pin_ext *e = PE(p);
    move_y(p, NTOD(e->yVel));                                                  /* :1 */
    if (DGT(NTOD(e->yVel), -6)) e->yVel += e->yAcc;
    if (collision_point_p(PTOD(p->x), PTOD(p->y) - 1, OBJ_oEndPlat, 0, NOONE) != NOONE) {   /* :4 */
        all_yvel(OBJ_oEndPlat, e->yVel);
        all_yvel(OBJ_oPDummy, e->yVel);
        all_yvel(OBJ_oBigTreasure, e->yVel);
        {
            int k;
            for (k = 0; k < PW.n; k++)
                if (PX(k).alive && PX(k).obj == OBJ_oBigTreasure) PE(&PX(k))->myGrav = 0;
        }
    }
    if (DLT(PTOD(p->y), -16) && e->status == 0) {                              /* :12 */
        e->yVel = 0;
        e->yAcc = 0;
        e->alarm[0] = 40;
        e->status += 1;
    }
    if (!snd_is_playing(SND_xflame)) snd_play(SND_xflame);                     /* :20 */
}

/* objects/oPlayerSil, oTreasureSil: Step_0 (Create_0: xVel -6, yVel -8, myGrav 0.6) */
static void sil_step(int i, int treasure)
{
    struct pin *p = &PX(i);
    struct pin_ext *e = PE(p);
    move_x(p, NTOD(e->xVel));
    move_y(p, NTOD(e->yVel));
    if (DLT(NTOD(e->xVel), 0)) e->xVel += N(0.1);
    if (DLT(NTOD(e->yVel), 6)) e->yVel += e->myGrav;
    if (treasure && DGT(PTOD(p->y), 240)) play_goto_room = R_rEnd3;            /* oTreasureSil :13 */
}

/* objects/oEnd3/Step_0.gml */
static void end3_step(void)
{
    if (skip_pressed()) {                                                      /* :5 */
        if (END3.drawStatus == 7) {
            if (END3.moneyCount < PG.money) END3.moneyCount = PG.money;
            else END3.fadeOut = 1;
        } else if (END3.drawStatus == 8) {
            skip_clear();
            play_goto_room = R_rCredits2;
        }
    }
    if (END3.drawStatus > 2) {                                                 /* :25 */
        int32_t moneyDiff = PG.money - END3.moneyCount;
        if (moneyDiff > 1000) END3.moneyCount += 1000;
        else if (moneyDiff > 100) END3.moneyCount += 100;
        else END3.moneyCount += moneyDiff;
        if (END3.drawStatus == 3 && moneyDiff == 0 && !END3.poop) {
            PE(&PX(END3.i))->alarm[11] = 50;
            END3.poop = 1;
        }
    }
    if (END3.fadeOut) {                                                        /* :42 */
        if (DLT(END3.fadeLevel, 1)) END3.fadeLevel += 0.1;
        else END3.drawStatus = 8;
    }
}

/* objects/oEnd3/Alarm_*.gml */
static void end3_alarm(int i, int a)
{
    struct pin_ext *e = PE(&PX(i));
    switch (a) {
    case 0:                                                                    /* Alarm_0 */
        pin_create(PI(144), PI(-32), OBJ_oPDummy2);
        snd_play(SND_xpfall);
        break;
    case 1: END3.drawStatus = 2; e->alarm[2] = 50; break;
    case 2: END3.drawStatus = 3; break;
    case 11:
        END3.drawStatus = 4;
        snd_play(SND_xthump);
        PG.money += 50000;
        END3.moneyCount += 50000;
        e->alarm[3] = 50;
        break;
    case 3: END3.drawStatus = 5; e->alarm[4] = 10; break;
    case 4: END3.drawStatus = 6; e->alarm[5] = 10; break;
    case 5: END3.drawStatus = 7; break;
    }
}

/* objects/oPDummy2 (the ending's actor, falling from the volcano): Create_0, Step_0, Alarm_0..2, Draw_0 */
static int pdummy2_ev(int ev, int i, int a)
{
    struct pin *p = &PX(i);
    struct pin_ext *e = PE(p);
    switch (ev) {
    case FEV_CREATE:
        p->type = T_NONE;                                                      /* oDrawnSprite */
        e->status = PD2_DROP;
        e->xVel = 0;
        e->yVel = 0;
        e->myGrav = N(0.6);
        e->facing = 0;                                                         /* LEFT */
        if (G.isDamsel) pin_set_sprite(i, GSPR_sDamselRunL);
        else if (G.isTunnelMan) pin_set_sprite(i, GSPR_sTunnelRunL);
        return 1;
    case FEV_STEP: {
        double x, y;
        move_x(p, NTOD(e->xVel));                                              /* :1 */
        move_y(p, NTOD(e->yVel));
        x = PTOD(p->x);
        y = PTOD(p->y);
        if (e->status == PD2_DROP) {                                           /* :4 */
            pin_set_sprite(i, chr_spr(GSPR_sDieLFall, GSPR_sDamselFallL, GSPR_sTunnelFallL));
            e->yVel = N(6);
            if (collision_point_p(x, y + 9, OBJ_oDesert2, 0, NOONE) != NOONE) {
                int pf;
                e->yVel = 0;
                e->status += 1;
                pin_set_sprite(i, chr_spr(GSPR_sStunL, GSPR_sDamselStunL, GSPR_sTunnelStunL));
                pf = pin_create((pos)(x - 4), (pos)(y + 6), OBJ_oPoof);
                PE(&PX(pf))->xVel = N(-0.4);
                pf = pin_create((pos)(x + 4), (pos)(y + 6), OBJ_oPoof);
                PE(&PX(pf))->xVel = N(0.4);
                snd_play(SND_xthud);
                p = &PX(i);
                e = PE(p);
            }
        } else if (e->status == PD2_STUNNED) {                                 /* :24 */
            e->alarm[0] = 70;
            e->alarm[1] = 50;
            e->status = PD2_GETUP;
        } else if (e->status == PD2_GETUP) {                                   /* :30 */
            int t = instance_nearest_p(x, y, OBJ_oBigTreasure);
            if (t != NOONE && DEQ(NTOD(PE(&PX(t))->yVel), 0)) {
                e->yVel = N(-4);
                e->status = PD2_JUMPING;
            }
        } else if (e->status == PD2_JUMPING) {                                 /* :42 */
            pin_set_sprite(i, chr_spr(GSPR_sJumpLeft, GSPR_sDamselDieLR, GSPR_sTunnelDieLR));
            if (DLT(NTOD(e->yVel), 4)) e->yVel += N(0.6);
            if (collision_point_p(x, y + 6, OBJ_oDesert2, 0, NOONE) != NOONE) {
                e->yVel = 0;
                e->status += 1;
                e->alarm[2] = 50;
                pin_set_sprite(i, chr_spr(GSPR_sStandLeft, GSPR_sDamselLeft, GSPR_sTunnelLeft));
                e->facing = 1;                                                 /* RIGHT */
                with_visible(OBJ_oMenu, 1);
            }
        }
        if (collision_point_p(PTOD(p->x), PTOD(p->y) + 6, OBJ_oDesert2, 0, NOONE) != NOONE) move_y(p, -2);   /* :64 */
        if (collision_point_p(PTOD(p->x), PTOD(p->y) + 7, OBJ_oDesert2, 0, NOONE) != NOONE) move_y(p, -1);
        return 1;
    }
    case FEV_ALARM:
        if (a == 0) {                                                          /* Alarm_0 */
            pin_create(PI(160), PI(-32), OBJ_oBigTreasure);
            snd_play(SND_xtfall);
        } else if (a == 1)                                                     /* Alarm_1 */
            pin_set_sprite(i, chr_spr(GSPR_sStandLeft, GSPR_sDamselLeft, GSPR_sTunnelLeft));
        else if (a == 2 && END3.i >= 0 && PX(END3.i).alive) {                 /* Alarm_2: with oEnd3 */
            END3.drawStatus = 1;
            PE(&PX(END3.i))->alarm[1] = 50;
            if (snd_music_on) snd_audio_play(SND_mVictory, 10, 0);
        }
        return 1;
    case FEV_DRAW:                                                             /* Draw_0 :1 */
        pin_setxscale(p, e->facing == 1 ? -1 : 1);
        return 1;
    default:
        return 0;
    }
}

/* objects/oCredits2/Step_0.gml */
static void credits_step(int i)
{
    int camel = instance_first_p(OBJ_oCamel);
    if (GP.down & K_ATTACK) {                                                  /* :4 checkAttack() */
        if (camel != NOONE) {
            if (CRED.fadeIn) CRED.fadeLevel = 0;
            else {
                skip_clear();
                CRED.fadeOut = 1;
            }
        }
    }
    if (camel != NOONE) {                                                      /* :20 */
        if (DLE(PTOD(PX(camel).x), 160) && !CRED.scrollStart) {
            with_scroll(OBJ_oDesertScroll, 1);
            PE(&PX(i))->alarm[1] = 1;
            PE(&PX(i))->alarm[2] = 20;
            CRED.scrollStart = 1;
            CRED.scrolling = 1;
        }
    }
    if (CRED.fadeIn) {                                                         /* :35 */
        if (DGT(CRED.fadeLevel, 0)) CRED.fadeLevel -= 0.1;
        else {
            CRED.fadeIn = 0;
            PE(&PX(i))->alarm[0] = 20;
        }
    } else if (CRED.fadeOut) {                                                 /* :44 */
        if (DLT(CRED.fadeLevel, 1)) CRED.fadeLevel += 0.1;
        else {
            snd_stop_music();
            scrUpdateHighscores(1);
            scrClearGlobals();
            PG.drawHUD = 0;
            G.gameStart = 0;
            play_goto_room = R_rHighscores;
        }
    }
}

/* objects/oCredits2/Alarm_*.gml: the scrolling desert, the camel and the caravan, the credit pages (drawStatus) */
static void credits_alarm(int i, int a)
{
    static const int16_t page_end[] = { 140, 140, 140, 140, 240, 240, 240 };   /* alarm[11] of Alarm_2 .. 8 */
    struct pin_ext *e = PE(&PX(i));
    if (a == 0) {                                                              /* Alarm_0 */
        pin_create(PI(320), PI(144), OBJ_oCamel);
        pin_create(PI(320 + 64), PI(144 - 16), OBJ_oCaravan);
    } else if (a == 1) {                                                       /* Alarm_1 */
        if (CRED.scrolling) {
            if (RAND(1, 8) == 1) pin_create(PI(-16), PI(176), OBJ_oShrubScroll);
            else if (RAND(1, 12) == 1) pin_create(PI(-32), PI(176 - 112), OBJ_oPalmTreeScroll);
            pin_create(PI(-16), PI(176), OBJ_oDesertTopScroll);
            pin_create(PI(-16), PI(192), OBJ_oDesertScroll2);
            pin_create(PI(-16), PI(208), OBJ_oDesertScroll);
            pin_create(PI(-16), PI(224), OBJ_oDesertScroll);
            with_scroll(OBJ_oDesertScroll, 1);
            with_scroll(OBJ_oDesertScroll2, 1);
            with_scroll(OBJ_oDesertTopScroll, 1);
            with_scroll(OBJ_oShrubScroll, 1);
            with_scroll(OBJ_oPalmTreeScroll, 1);
            PE(&PX(i))->alarm[1] = 16;
        }
    } else if (a >= 2 && a <= 8) {                                             /* Alarm_2 .. 8: the next page */
        CRED.drawStatus = a - 1;
        e->alarm[11] = page_end[a - 2];
        e->alarm[a + 1] = (int16_t)(page_end[a - 2] + 40);
    } else if (a == 9) {                                                       /* Alarm_9 */
        CRED.scrolling = 0;
        with_scroll(OBJ_oDesertScroll, 0);
        with_scroll(OBJ_oDesertScroll2, 0);
        with_scroll(OBJ_oDesertTopScroll, 0);
        with_scroll(OBJ_oPalmTreeScroll, 0);
        with_scroll(OBJ_oShrubScroll, 0);
        all_status(OBJ_oCamel, 2);
        all_status(OBJ_oCaravan, 2);
    } else if (a == 11)                                                        /* Alarm_11 */
        CRED.drawStatus = 0;
}

/* objects/oCamel, oCaravan: Step_0 (status 0 walking in, 1 standing, 2 leaving) */
static void camel_step(int i, int caravan)
{
    struct pin *p = &PX(i);
    if (PE(p)->status == 0) {
        move_x(p, -1);
        if (DLE(PTOD(p->x), caravan ? 160 + 64 : 160)) {
            if (!caravan && snd_music_on) snd_audio_play(SND_mCredits, 10, 0);   /* oCamel :6 */
            PE(p)->status = 1;
        }
    } else if (PE(p)->status == 1)
        move_x(p, -0.01);                                                      /* "or original": x -= 0.01 */
    else if (PE(p)->status == 2)
        move_x(p, -2);
    if (caravan && DLT(PTOD(p->x), -64)) CRED.fadeOut = 1;                     /* oCaravan :18 */
}

/* the ending's objects (rooms rEnd .. rCredits2): -1 when o is not one of them */
static int ending_ev(int ev, int i, int a)
{
    struct pin *p = &PX(i);
    switch (p->obj) {
    case OBJ_oEnd:
        if (ev == FEV_STEP) end_step();
        return 1;
    case OBJ_oEndPlat:                                                         /* Step_0: y += yVel */
        if (ev == FEV_STEP) move_y(p, NTOD(PE(p)->yVel));
        return 1;
    case OBJ_oBigTreasure:
        if (ev == FEV_CREATE) bigtreasure_create(i);
        else if (ev == FEV_STEP) bigtreasure_step(i);
        return 1;
    case OBJ_oLavaSpray:
        if (ev == FEV_CREATE) { PE(p)->yVel = 0; PE(p)->yAcc = 0; PE(p)->status = 0; }
        else if (ev == FEV_STEP) lavaspray_step(i);
        else if (ev == FEV_ALARM && a == 0) play_goto_room = R_rEnd2;         /* Alarm_0 */
        return 1;
    case OBJ_oEnd2:
        if (ev == FEV_STEP && skip_pressed()) {                                /* Step_0 */
            skip_clear();
            G.gameStart = 0;
            play_goto_room = R_rEnd3;
        } else if (ev == FEV_ALARM) {
            if (a == 0) {                                                      /* Alarm_0 */
                pin_create(PI(240), PI(132), OBJ_oPlayerSil);
                PE(&PX(i))->alarm[1] = 30;
            } else if (a == 1)                                                 /* Alarm_1 */
                pin_create(PI(240), PI(132), OBJ_oTreasureSil);
            else if (a == 2) {                                                 /* Alarm_2: arguments right to */
                int32_t ry = RAND(0, 8), rx = RAND(0, 48);                     /* left */
                pin_create(PI(224 + rx), PI(144 + ry), OBJ_oVolcanoFlame);
                PE(&PX(i))->alarm[2] = (int16_t)RAND(10, 20);
            }
        }
        return 1;
    case OBJ_oPlayerSil: case OBJ_oTreasureSil:
        if (ev == FEV_CREATE) { PE(p)->xVel = N(-6); PE(p)->yVel = N(-8); PE(p)->myGrav = N(0.6); }
        else if (ev == FEV_STEP) sil_step(i, p->obj == OBJ_oTreasureSil);
        return 1;
    case OBJ_oVolcanoFlame:
        if (ev == FEV_CREATE) {                                                /* Create_0 */
            double r1, r2;
            pin_setispd(p, (img_t)0.3);
            r1 = prandom(4);
            r2 = prandom(4);
            PE(p)->xVel = ND(r1 - r2);
            PE(p)->yVel = ND(-1 - prandom(2));
            PE(p)->grav = ND(RAND(1, 6) * 0.1);
            PE(p)->alarm[0] = 2;
            PE(p)->alarm[1] = 50;
        } else if (ev == FEV_STEP) {                                           /* Step_0 */
            move_x(p, NTOD(PE(p)->xVel));
            move_y(p, NTOD(PE(p)->yVel));
            if (DLT(NTOD(PE(p)->yVel), 6)) PE(p)->yVel += PE(p)->grav;
        } else if (ev == FEV_OUTSIDE)                                          /* Other_0 */
            pin_destroy(i);
        return 1;                                                              /* Alarm_0: commented out */
    case OBJ_oEnd3:
        if (ev == FEV_STEP) end3_step();
        else if (ev == FEV_ALARM) end3_alarm(i, a);
        return 1;                                                              /* Draw_0: front_draw */
    case OBJ_oPDummy2:
        return pdummy2_ev(ev, i, a);
    case OBJ_oBGEnd3:                                                          /* Step_0 (rCredits2) */
        if (ev == FEV_STEP && PW.room != R_rEnd3) {
            int c = instance_first_p(OBJ_oCamel);
            if (c != NOONE && PE(&PX(c))->status != 2 && DLE(PTOD(PX(c).x), 160)) {
                view_read();
                pin_setx(p, (pos)(PW.xview + NTOD(PE(p)->px)));                /* xOff kept in px */
                PE(p)->px = PE(p)->px + N(0.02);
            }
        }
        return 1;
    case OBJ_oCredits2:
        if (ev == FEV_STEP) credits_step(i);
        else if (ev == FEV_ALARM) credits_alarm(i, a);
        return 1;
    case OBJ_oCamel:
        if (ev == FEV_CREATE) {                                                /* Create_0 */
            pin_setispd(p, (img_t)0.5);
            PE(p)->status = 0;
            if (G.isDamsel) pin_set_sprite(i, GSPR_sCamelDamsel);
            else if (G.isTunnelMan) pin_set_sprite(i, GSPR_sCamelTunnel);
        } else if (ev == FEV_STEP) camel_step(i, 0);
        return 1;
    case OBJ_oCaravan:
        if (ev == FEV_CREATE) {                                                /* Create_0 */
            if (PG.damsels > 0) pin_set_sprite(i, G.isDamsel ? GSPR_sCaravan3 : GSPR_sCaravan2);
            pin_setispd(p, (img_t)0.5);
            PE(p)->status = 0;
        } else if (ev == FEV_STEP) camel_step(i, 1);
        return 1;
    case OBJ_oDesertScroll: case OBJ_oDesertScroll2: case OBJ_oDesertTopScroll: case OBJ_oShrubScroll:
    case OBJ_oPalmTreeScroll:
        if (ev == FEV_CREATE) {                                                /* Create_0: scroll = false (the */
            p->type = T_NONE;                                                  /* oDrawnSprite ones: type "") */
            PE(p)->armed = 0;
        } else if (ev == FEV_STEP) {                                           /* Step_0 */
            if (PE(p)->armed) move_x(p, 1);
            if (DGT(PTOD(PX(i).x), 320)) pin_destroy(i);
        }
        return 1;
    case OBJ_oPDummy:
        return PW.room == R_rEnd ? pdummy_end_ev(ev, i, a) : 0;
    case OBJ_oEndWall: case OBJ_oEnd2BG: case OBJ_oBigChest: case OBJ_oShrub: case OBJ_oPalmTree:
        return ev == FEV_CREATE;
    default:
        return -1;
    }
}

int front_ev(int ev, int i, int arg)
{
    int o = PX(i).obj, r;
    if (ending_room(PW.room)) {
        if (ev == FEV_STEP && play_goto_room >= 0) return 1;  /* a Step called room_goto: the runner ends the */
        if ((r = ending_ev(ev, i, arg)) >= 0) return r;       /* Step dispatch there (prun.c) */
    }
    switch (o) {
    case OBJ_oHighscores:
        if (ev == FEV_STEP) scores_step();
        return ev == FEV_STEP || ev == FEV_CREATE || ev == FEV_DRAW;
    case OBJ_oButtonHighscore:
        if (ev == FEV_STEP) button_step(i);
        return ev == FEV_STEP || ev == FEV_CREATE;
    case OBJ_oTitle: return title_ev(ev, i, arg);
    case OBJ_oFlare: return flare_ev(ev, i, arg);
    case OBJ_oFlareSpark: return flarespark_ev(ev, i);
    case OBJ_oPDummy4: return pdummy4_ev(ev, i, arg);
    case OBJ_oTunnelMan:                         /* Step_0: talk / donations, all on controls: nothing in attract */
        return ev == FEV_CREATE || ev == FEV_STEP;
    case OBJ_oPlayer1:
        if (ev == FEV_CREATE) { pl_init_from_gen(i); return 1; }   /* oPlayer1 Create (characterCreateEvent) */
        return 0;
    case OBJ_oIntro: return intro_ev(ev, i, arg);
    case OBJ_oPDummy3: return pdummy3_ev(ev, i, arg);
    case OBJ_oBatIntro:
        if (ev == FEV_STEP) batintro_step(i);
        return ev == FEV_STEP || ev == FEV_CREATE;
    case OBJ_oIntroBG:
        if (ev == FEV_STEP) parallax_step(i, 0.02, 0);
        return ev == FEV_STEP || ev == FEV_CREATE;
    case OBJ_oMoon:
        if (ev == FEV_STEP) parallax_step(i, 0.01, 208);
        return ev == FEV_STEP || ev == FEV_CREATE;
    case OBJ_oNew:                               /* Create: global.sNewNew, the English new.png = sNew, its default */
    case OBJ_oDesert: case OBJ_oDesertTop: case OBJ_oDesert2: case OBJ_oGlobals:
        return ev == FEV_CREATE;
    default:
        return 0;
    }
}

/* ---- the attract cycle --------------------------------------------------------------------------------------- */
void front_start_at(int room)                     /* tests: the attract cycle from that room */
{
    front_on = 1;
    front_ending = 0;
    gen_new_game();
    PW.next_id = RUNNER_ID_FRONT;
    rng_seed(&g_rng, front_seed);                 /* the traces' random_set_seed(SEED) in the title flow */
    front_room(room);
}

void front_start(void)
{
    front_on = 1;
    front_ending = 0;
    front_new = 0;
    gen_new_game();                                                            /* oGlobals: scrClearGlobals */
    PW.next_id = RUNNER_ID_FRONT;                                             /* the runner's: after oGamepad */
    front_room(R_rIntro);
    PW.next_id = RUNNER_ID_FRONT;
}

void front_step(void)
{
    int r;
    if (!front_on) front_start();
    r = play_step(0, front_rec_cb);
    if (r == PLAY_ROOM_EARLY) return;                                          /* a frame without a step */
    if (r != 0) {                                      /* a room the front end has not */
        front_start();                                                         /* (the title's doors, ...) */
        return;
    }
    room_steps++;
    if (PW.room == R_rTitle && room_steps >= FRONT_TITLE_STEPS) {
        front_room(R_rHighscores);
    } else if (PW.room == R_rHighscores && room_steps >= FRONT_SCORES_STEPS)
        front_start();
}

void front_stop(void) { front_on = front_ending = 0; }

/* objects/oHighscores/Draw_64.gml (English: tr() through datafiles/locale/locales/en/text.json, whose labels carry
   one more space than the GML's; room_offset 0, the GUI's 320 x 240): the scores box, or the secret
   challenges when the player stands in the upper rooms; the reset warning */
static void scores_text(void)
{
    char b[40], *e;
    int pl = instance_first_p(OBJ_oPlayer1), blk = instance_nearest_p(160, 240, OBJ_oPushBlock), but;
    int32_t v1 = SH.hs.value[HS_MONEY], v3 = SH.hs.value[HS_KILLS], v4 = SH.hs.value[HS_SAVES];
    if (pl == NOONE) return;
    if (DLT(PTOD(PX(pl).y), 156)) {
        hud_text("SECRET CHALLENGES", HUD_FONT_SMALL, 1, 112 + (192 - 17 * 8) / 2, 32);
        /* (the challenge rooms' lines: the attract's player never stands there) */
    } else {
        hud_text("TOP DEFILERS", HUD_FONT_SMALL, 1, 112 + (192 - 12 * 8) / 2, 32);
        e = b; { const char *t = "MONEY:   "; while (*t) *e++ = *t++; } hud_itoa(v1, e); hud_text(b, HUD_FONT_SMALL, 0, 120, 48);
        e = b; { const char *t = "KILLS:   "; while (*t) *e++ = *t++; } hud_itoa(v3, e); hud_text(b, HUD_FONT_SMALL, 0, 120, 64);
        e = b; { const char *t = "SAVES:   "; while (*t) *e++ = *t++; } hud_itoa(v4, e); hud_text(b, HUD_FONT_SMALL, 0, 120, 80);
        if (SH.hs.value[HS_WINS] > 0) {                                             /* only display time if won */
            e = b; { const char *t = "TIME:    "; while (*t) *e++ = *t++; }
            hud_mss(e, SH.hs.value[HS_TIME]);
            hud_text(b, HUD_FONT_SMALL, 0, 120, 96);
        }
        hud_text("STATISTICS", HUD_FONT_SMALL, 1, 112 + (192 - 10 * 8) / 2, 112);
        e = b; { const char *t = "PLAYS:   "; while (*t) *e++ = *t++; } hud_itoa(SH.hs.value[HS_PLAYS], e); hud_text(b, HUD_FONT_SMALL, 0, 120, 128);
        e = b; { const char *t = "DEATHS:  "; while (*t) *e++ = *t++; } hud_itoa(SH.hs.value[HS_DEATHS], e); hud_text(b, HUD_FONT_SMALL, 0, 120, 144);
        e = b; { const char *t = "WINS:    "; while (*t) *e++ = *t++; } hud_itoa(SH.hs.value[HS_WINS], e); hud_text(b, HUD_FONT_SMALL, 0, 120, 160);
    }
    but = instance_first_p(OBJ_oButtonHighscore);
    if (blk != NOONE && but != NOONE && !PE(&PX(but))->armed && DGT(PTOD(PX(blk).x), 160))
        hud_text_centered("THIS WILL CLEAR EVERYTHING!", HUD_FONT_SMALL, 1, 0, 216);
}

/* scripts/drawCredits (oCredits2's Draw GUI: color1 c_yellow, color2 c_white; English, X1 16, X2 32, X3 144). Its
   black rectangle (fadeIn / fadeOut, alpha fadeLevel) is front_fade's, drawn after the text: the text then uses the
   faded HUD palettes (hud_text_faded) */
static void credits_text(void)
{
    static const char *const testers[] = {
        "ANNABELLE K.", "BENZIDO", "CHUTUP", "CORPUS", "GENERALVALTER", "GUERT", "GRAHAM GORING",
        "HAOWAN", "HIDEOUS", "INANE", "INCREPARE", "KAO", "MARK JOHNS", "MELLY", "PAUL ERES",
        "SUPER JOE", "TANTAN", "TEAM QUIGGAN", "TERRY", "XION", "ZAPHOS"
    };
    static const char *const hd_contributors[] = {
        "NKRAPIVIN", "GRHEAVY", "SPENCJO", "GABRIEL ALBUQUERQUE FERREIRA", "BAKUSTARVER",
        "LERETARDATN", "MASTERPHW", "BRNBOT3K", "V9TN"
    };
    int k;
    hud_text_faded(CRED.fadeIn || CRED.fadeOut);
    switch (CRED.drawStatus) {
    case 1: hud_text("SPELUNKY", HUD_FONT_LARGE, 1, 16, 16); break;
    case 2:
        hud_text("A GAME BY", HUD_FONT_SMALL, 1, 16, 16);
        hud_text("DEREK YU", HUD_FONT_SMALL, 0, 32, 32);
        break;
    case 3:
        hud_text("PLATFORM ENGINE", HUD_FONT_SMALL, 1, 16, 16);
        hud_text("MARTIN PIECYK", HUD_FONT_SMALL, 0, 32, 32);
        hud_text("SOUND EFFECTS MADE USING", HUD_FONT_SMALL, 1, 16, 48);
        hud_text("DR PETTER'S SFXR", HUD_FONT_SMALL, 0, 32, 64);
        hud_text("SCREEN SCALING CODE", HUD_FONT_SMALL, 1, 16, 80);
        hud_text("CHEVYRAY", HUD_FONT_SMALL, 0, 32, 96);
        break;
    case 4:
        hud_text("MUSIC BY", HUD_FONT_SMALL, 1, 16, 16);
        hud_text("GEORGE BUZINKAI", HUD_FONT_SMALL, 0, 32, 32);
        hud_text("JONATHAN PERRY", HUD_FONT_SMALL, 0, 32, 40);
        break;
    case 5:
        hud_text("BETA TESTING BY", HUD_FONT_SMALL, 1, 16, 16);
        for (k = 0; k < (int)(sizeof testers / sizeof testers[0]); k++)
            hud_text(testers[k], HUD_FONT_SMALL, 0, k >= 11 ? 144 : 32, k >= 11 ? 32 + 8 * k - 88 : 32 + 8 * k);
        break;
    case 6:
        hud_text("SPELUNKY CLASSIC HD BY", HUD_FONT_SMALL, 1, 16, 16);
        hud_text("YANCHARKIN", HUD_FONT_SMALL, 0, 32, 32);
        hud_text("CONTRIBUTORS", HUD_FONT_SMALL, 1, 16, 48);
        for (k = 0; k < (int)(sizeof hd_contributors / sizeof hd_contributors[0]); k++)
            hud_text(hd_contributors[k], HUD_FONT_SMALL, 0, 32, 64 + 8 * k);
        break;
    case 7:
        hud_text("THANKS FOR PLAYING!", HUD_FONT_SMALL, 1, 16, 16);
        hud_text("SEE YOU NEXT ADVENTURE!", HUD_FONT_SMALL, 0, 32, 32);
        break;
    }
    hud_text_faded(0);
}

void front_draw_gui(void)
{
    if (PW.room == R_rHighscores) scores_text();
    else if (PW.room == R_rCredits2 && CRED.i >= 0) credits_text();
}

int32_t front_drawkey(int i)
{
    if (lpos_id[i] == PX(i).id) return 0x3fffffff - lpos[i];               /* a room instance */
    return 0x40000000 + PX(i).id;                                              /* made at run time */
}

/* ---- drawing ------------------------------------------------------------------------------------------------- */
/* draw_set_alpha(f)'s alpha byte for the rooms' black rectangles: f as a single-precision float times 255, truncated
   (build/trace/g_end_win_s7 record 1250: oEnd3.fadeLevel 0.7999999999999999 is drawn at 204; tools/drawmodel.py
   alpha_byte) */
static int alpha_byte(double f) { return f <= 0 ? 0 : f >= 1 ? 255 : (int)((double)(float)f * 255.0); }

int front_fade(int *a8)
{
    if (PW.room == R_rIntro && INTRO.i >= 0 && PX(INTRO.i).alive) {           /* objects/oIntro/Draw_0.gml :1-4 */
        *a8 = alpha_byte(INTRO.fadeLevel);
        return INTRO.i;
    }
    if (PW.room == R_rEnd3 && END3.i >= 0 && END3.fadeOut) {                  /* showFinalScore :62 */
        *a8 = alpha_byte(END3.fadeLevel);
        return END3.i;
    }
    if (PW.room == R_rCredits2 && CRED.i >= 0 && (CRED.fadeIn || CRED.fadeOut)) {   /* drawCredits :64 (Draw GUI: */
        *a8 = alpha_byte(CRED.fadeLevel);                                      /* over all; oCredits2 is the */
        return CRED.i;                                                         /* shallowest instance) */
    }
    return -1;
}

/* scripts/showFinalScore(drawStatus, fadeOut) (oEnd3's Draw: room coordinates, the view at 0, 0; English; lblX 64,
   valX 224 at room_offset 0). The lines before the black rectangle (fadeOut) use the faded HUD palettes
   (hud_text_faded): they fade with it */
static void final_score(int dx, int dy)
{
    char b[24];
    hud_text_faded(END3.fadeOut);
    {
        if (END3.drawStatus > 0) hud_text_centered("YOU MADE IT!", HUD_FONT_LARGE, 1, dx, 32 + dy);
        if (END3.drawStatus > 1) hud_text_centered("FINAL SCORE:", HUD_FONT_SMALL, 1, dx, 56 + dy);
        if (END3.drawStatus > 2) {
            b[0] = '$';
            hud_itoa(END3.moneyCount, b + 1);
            hud_text_centered(b, HUD_FONT_LARGE, 0, dx, 72 + dy);
        }
        if (END3.drawStatus > 4) {                                             /* :30 */
            hud_text("TIME:    ", HUD_FONT_SMALL, 1, 64 + dx, 96 + dy);        /* tr("TIME:  ") */
            hud_mss(b, PG.time / 1000);                                        /* floor(global.time / 1000) */
            hud_text(b, HUD_FONT_SMALL, 0, 224 + dx, 96 + dy);
        }
        if (END3.drawStatus > 5) {
            hud_text("KILLS:   ", HUD_FONT_SMALL, 1, 64 + dx, 96 + 8 + dy);
            hud_text(hud_itoa(PG.kills, b), HUD_FONT_SMALL, 0, 224 + dx, 96 + 8 + dy);
        }
        if (END3.drawStatus > 6) {
            hud_text("SAVES:   ", HUD_FONT_SMALL, 1, 64 + dx, 96 + 16 + dy);
            hud_text(hud_itoa(PG.damsels, b), HUD_FONT_SMALL, 0, 224 + dx, 96 + 16 + dy);
        }
    }
    hud_text_faded(0);
    if (END3.drawStatus == 8)                                                  /* :70 */
        hud_text_centered("YOU SHALL BE REMEMBERED AS A HERO.", HUD_FONT_SMALL, 0, dx, 116 + dy);
}

void front_draw(int i, int ox, int oy)
{
    if (PX(i).obj == OBJ_oIntro) {                                             /* objects/oIntro/Draw_0.gml :6-17 */
        /* drawTextHCentered in a Draw event: room coordinates (x from the 320-px display, y as given) */
        int dx = -ox, dy = -(oy - HUD_CROP);
        if (INTRO.drawStatus > 0) hud_text_centered(intro_str1[INTRO.str1], HUD_FONT_SMALL, 0, dx, 116 - 16 + dy);
        if (INTRO.drawStatus > 1) hud_text_centered(intro_str2[INTRO.str2], HUD_FONT_SMALL, 0, dx, 116 + dy);
        if (INTRO.drawStatus > 2) hud_text_centered(intro_str3[INTRO.str3], HUD_FONT_SMALL, 0, dx, 116 + 16 + dy);
    } else if (PX(i).obj == OBJ_oEnd3)                                         /* objects/oEnd3/Draw_0.gml */
        final_score(-ox, -(oy - HUD_CROP));
}
