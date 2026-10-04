/* The front end (front.h): HD 1.2.2's rIntro, rTitle and rHighscores on the play loop (src/game), with the events
 * of their own objects translated here, statement for statement (refs/hd/src/objects/<o>/<event>.gml: file:line in
 * comments). Objects src/game translates (oRopeThrow, oRope, oGame, the walls) run there.
 *
 * Gameplay state (what an attract room leaves behind, PLAN §1 exactness is per seed):
 *   - the RNG: oIntro's Create calls randomize() (objects/oIntro/Create_0.gml:6): HD seeds from the clock there, and
 *     every draw in the intro and title rooms follows from it (oDark Create's rand(1,100) x 203 come before it;
 *     oIntro's 3 random_range, scrSetupWalls' tiles, oBatIntro's random() and the title's draws after). A level's
 *     generation then continues the same generator. On the cabinet the shell starts a game with game_begin, which
 *     seeds the generator again (game_cfg.seed, or the frame counter: src/main/game.c), as HD's randomize() leaves
 *     the seed arbitrary: the front end's draws never reach a level. front_seed is the intro's randomize() seed (the
 *     traces' random_set_seed(SEED): tools/tracer.py).
 *   - globals: oGlobals' Create (scrClearGlobals and the HD globals) runs in rIntro; a game start runs
 *     gen_new_game (src/game: scrClearGlobals' part) itself, so nothing the intro sets reaches a level either.
 *
 * Attract cycle (HD has none: its title waits for the player): rIntro (as HD: the story, oPDummy3's walk, then
 * room_goto(rTitle)), rTitle for FRONT_TITLE_STEPS, rHighscores for FRONT_SCORES_STEPS, then rIntro again. */
#include "pint.h"
#include "inst.h"
#include "gen.h"
#include "fronttables.h"
#include "front.h"
#include "pcol.h"
#include "hud.h"
#include "snd.h"
#include "shell.h"

#define FRONT_TITLE_STEPS  900
#define FRONT_SCORES_STEPS 450
#ifndef R_rIntro
#define R_rIntro 1                                /* the names file's room index (pint.h lists the play rooms) */
#endif

uint8_t front_on;
int16_t front_view_obj = -1, front_hborder = 160;
uint32_t front_seed = 1;                          /* oIntro's randomize() */
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
        SH.hs.value[3] >= 120 && SH.hs.value[4] >= 8) {                       /* :103 */
        instance_create(32, 112, OBJ_oMultiTrophy);
        instance_create(32, 128, OBJ_oXChange2);
    } else
        instance_create(32 + 8, 128 + 8, OBJ_oTunnelMan);
}

/* objects/oHighscores/Create_0.gml (two instances in rHighscores: each runs it): the globals, the trophies for the
   EEPROM's scores (src/shell: SH.hs), the "new" marks (none at a cabinet's boot), global.scoresStart 0 */
static void scores_create_w(void)
{
    int32_t tMoney = SH.hs.value[HS_MONEY], tTime = SH.hs.value[HS_TIME], tKills = SH.hs.value[3];
    int32_t tSaves = SH.hs.value[4];
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
}

/* ---- room loading (as play_transition_start: the instances into W, the Create events that need the
   generator's world (walls, RNG order), then PW and the other Create events) ---------------------------------- */
static const struct froom *room_table(int room)
{
    switch (room) {
    case R_rIntro: return &froom_rIntro;
    case R_rTitle: return &froom_rTitle;
    case R_rHighscores: return &froom_rHighscores;
    default: return 0;
    }
}

static uint8_t w_alive[PIN_MAX];                    /* the room instances alive after the generator-phase Creates */

int front_room(int room)
{
    const struct froom *rm = room_table(room);
    int k, n0;
    if (!rm) return 0;
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
            rng_seed(&g_rng, front_seed);                                      /* :6 randomize() */
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
    }
    pin_add(OBJ_oGamepad, 0, 0, 110219);                                       /* the tracer's (persistent) */
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
    INTRO.i = -1;
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
        default:
            break;
        }
    }
    for (k = 0; k < rm->n; k++)                                                /* instance creation code */
        if (rm->in[k].cc == FRONT_CC_rIntro_0_Create)
            PE(&PX(k))->px = 400;                                              /* rIntro: xOff = 400 */
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
        p->ispd = (img_t)0.6;
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
        p->ispd = (img_t)0.3;
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
        p->ispd = (img_t)0.4;
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
        p->ispd = (img_t)0.6;
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

int front_ev(int ev, int i, int arg)
{
    int o = PX(i).obj;
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
    gen_new_game();
    PW.next_id = 110220;
    rng_seed(&g_rng, front_seed);                 /* the traces' random_set_seed(SEED) in the title flow */
    front_room(room);
}

void front_start(void)
{
    front_on = 1;
    gen_new_game();                                                            /* oGlobals: scrClearGlobals */
    PW.next_id = 110220 - 0;                                                   /* the runner's: oGamepad 110219 */
    front_room(R_rIntro);
    PW.next_id = 110220;
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
        PW.next_id = PW.next_id;
        front_room(R_rHighscores);
    } else if (PW.room == R_rHighscores && room_steps >= FRONT_SCORES_STEPS)
        front_start();
}

void front_stop(void) { front_on = 0; }

/* objects/oHighscores/Draw_64.gml (English: tr() through datafiles/locale/locales/en/text.json, whose labels carry
   one more space than the GML's; room_offset 0, the GUI's 320 x 240): the scores box, or the secret
   challenges when the player stands in the upper rooms; the reset warning */
static void scores_text(void)
{
    char b[40], *e;
    int pl = instance_first_p(OBJ_oPlayer1), blk = instance_nearest_p(160, 240, OBJ_oPushBlock), but;
    int32_t v1 = SH.hs.value[1], v3 = SH.hs.value[3], v4 = SH.hs.value[4];
    if (pl == NOONE) return;
    if (DLT(PTOD(PX(pl).y), 156)) {
        hud_text("SECRET CHALLENGES", HUD_FONT_SMALL, 1, 112 + (192 - 17 * 8) / 2, 32);
        /* (the challenge rooms' lines: the attract's player never stands there) */
    } else {
        hud_text("TOP DEFILERS", HUD_FONT_SMALL, 1, 112 + (192 - 12 * 8) / 2, 32);
        e = b; { const char *t = "MONEY:   "; while (*t) *e++ = *t++; } hud_itoa(v1, e); hud_text(b, HUD_FONT_SMALL, 0, 120, 48);
        e = b; { const char *t = "KILLS:   "; while (*t) *e++ = *t++; } hud_itoa(v3, e); hud_text(b, HUD_FONT_SMALL, 0, 120, 64);
        e = b; { const char *t = "SAVES:   "; while (*t) *e++ = *t++; } hud_itoa(v4, e); hud_text(b, HUD_FONT_SMALL, 0, 120, 80);
        if (SH.hs.value[6] > 0) {                                             /* only display time if won */
            int32_t sec = SH.hs.value[2], m = sec / 60;
            sec %= 60;
            e = b; { const char *t = "TIME:    "; while (*t) *e++ = *t++; }
            e += 0; hud_itoa(m, e); while (*e) e++; *e++ = ':'; if (sec < 10) *e++ = '0'; hud_itoa(sec, e);
            hud_text(b, HUD_FONT_SMALL, 0, 120, 96);
        }
        hud_text("STATISTICS", HUD_FONT_SMALL, 1, 112 + (192 - 10 * 8) / 2, 112);
        e = b; { const char *t = "PLAYS:   "; while (*t) *e++ = *t++; } hud_itoa(SH.hs.value[5], e); hud_text(b, HUD_FONT_SMALL, 0, 120, 128);
        e = b; { const char *t = "DEATHS:  "; while (*t) *e++ = *t++; } hud_itoa(SH.hs.value[7], e); hud_text(b, HUD_FONT_SMALL, 0, 120, 144);
        e = b; { const char *t = "WINS:    "; while (*t) *e++ = *t++; } hud_itoa(SH.hs.value[6], e); hud_text(b, HUD_FONT_SMALL, 0, 120, 160);
    }
    but = instance_first_p(OBJ_oButtonHighscore);
    if (blk != NOONE && but != NOONE && !PE(&PX(but))->armed && DGT(PTOD(PX(blk).x), 160))
        hud_text_centered("THIS WILL CLEAR EVERYTHING!", HUD_FONT_SMALL, 1, 0, 216);
}

void front_draw_gui(void)
{
    if (PW.room == R_rHighscores) scores_text();
}

int32_t front_drawkey(int i)
{
    if (lpos_id[i] == PX(i).id) return 0x3fffffff - lpos[i];               /* a room instance */
    return 0x40000000 + PX(i).id;                                              /* made at run time */
}

/* ---- drawing ------------------------------------------------------------------------------------------------- */
int front_fade(int *a8)
{
    if (PW.room == R_rIntro && INTRO.i >= 0 && PX(INTRO.i).alive) {           /* objects/oIntro/Draw_0.gml :1-4 */
        double f = INTRO.fadeLevel;
        *a8 = f <= 0 ? 0 : f >= 1 ? 255 : (int)(f * 255.0);
        return INTRO.i;
    }
    return -1;
}

void front_draw(int i, int ox, int oy)
{
    if (PX(i).obj == OBJ_oIntro) {                                             /* objects/oIntro/Draw_0.gml :6-17 */
        /* drawTextHCentered in a Draw event: room coordinates (x from the 320-px display, y as given) */
        int dx = -ox, dy = -(oy - HUD_CROP);
        if (INTRO.drawStatus > 0) hud_text_centered(intro_str1[INTRO.str1], HUD_FONT_SMALL, 0, dx, 116 - 16 + dy);
        if (INTRO.drawStatus > 1) hud_text_centered(intro_str2[INTRO.str2], HUD_FONT_SMALL, 0, dx, 116 + dy);
        if (INTRO.drawStatus > 2) hud_text_centered(intro_str3[INTRO.str3], HUD_FONT_SMALL, 0, dx, 116 + 16 + dy);
    }
}
