/* The step loop of GameMaker 2024.14 as the HD runner runs it (rules and evidence: play.h), the level start, the
 * route inputs (tools/tracer.py's oGamepad replacement), the view and the room changes. */
#include "pint.h"
#include "penemy.h"
#include "pcol.h"
#include "../snd/sndgame.h"                     /* the GML sound calls (src/snd) */
#include "front.h"                                     /* P8: the front end's hooks (src/front/front.h) */
static void start_music(int levelType);

struct gamepad GP;
struct pglobals PG;
int play_untranslated;
int play_untr_obj = -1;
int play_cur_obj = -1;
uint32_t play_time;
int play_goto_room = -1;
int32_t play_rooms_entered;
int play_noenemy = 1;
const struct inst *play_gen_inst;
int play_gen_created;                    /* P4 references: TRACE_NOENEMY (playhost --enemies clears it) */
uint32_t play_dops;
#ifdef NUM_IS_CLASS
struct dcount play_dcount;
#endif

/* ---- dispatch order: object index order (pobj[].rt), then creation order -------------------------------- */
static int16_t order[PIN_MAX];

/* the tracer gives oGamepad a Begin Step and an End Step (tools/tracer.py) */
static uint16_t objev(int obj)
{
    return (uint16_t)(pobj[obj].ev | (obj == OBJ_oGamepad ? EV_BEGIN | EV_END : 0));
}

/* the objects with an event, in runtime object order: key 0-11 alarm k, 12 Step, 13 Outside Room, 14 End Step */
#define EVK_STEP 12
#define EVK_OUTSIDE 13
#define EVK_END 14
#define EVK_DRAW 15
static int16_t evobj[1024];
static int16_t evobj0[17];

static void evobj_init(void)
{
    int key, rt, n = 0;
    for (key = 0; key < 16; key++) {
        evobj0[key] = (int16_t)n;
        for (rt = 0; rt < RTOBJ_COUNT; rt++) {
            int o = obj_byrt[rt], has;
            static const uint16_t abit[12] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048 };
            if (key < 12) has = (pobj[o].alarms & abit[key]) != 0;
            else has = (objev(o) & (key == EVK_STEP ? EV_STEP : key == EVK_OUTSIDE ? EV_OUTSIDE :
                                    key == EVK_END ? EV_END : EV_DRAW)) != 0;
            if (!has) continue;
            if (n == (int)(sizeof evobj / sizeof evobj[0])) { PUNTR(9004); break; }
            evobj[n++] = (int16_t)o;
        }
    }
    evobj0[16] = (int16_t)n;
}

/* the alive instances whose object has the event `key`, in dispatch order: the objects in runtime order, each
   object's instances in creation order (pworld.c's lists) */
static int snapshot(int key)
{
    int j, n = 0;
    if (evobj0[16] == 0) evobj_init();
    PWST(snap, 1);
    for (j = evobj0[key]; j < evobj0[key + 1]; j++) {
        int i;
        for (i = pw_ohead[evobj[j]]; i >= 0; i = pw_inext[i]) {
            PWST(snapv, 1);
            order[n++] = (int16_t)i;
        }
    }
    return n;
}

/* ---- the view: oScreen Create sets the level rooms' view 0 to w x 240 following oPlayer1 with hborder w / 2,
   vborder 96, no speed limit (objects/oScreen/Create_0.gml :45; w = 320 for the 4:3 display of the CPS3 port and
   of the P4 references, scripts/p4_trace.sh). hborder 160 = half the width keeps the player centred ---------- */
static void view_update(void);

/* Observed (build/trace/p1_walk_s1, records 310-316, and p4_exit559_s559 record 474): the view a record reads
   is the one the previous frame's drawing computed (it trails the player by a step), except in steps where GML
   set the view's position (oLevel's screen shake, oPlayer1 looking up / down): then every later read in the step
   sees the target-following applied to the player's current position (the shake steps' records follow the player
   of the same step; once the shake ends they trail again) */
static uint8_t view_in_step;                 /* Begin Step .. collision events: reads see the last drawn view */
void view_read(void)
{
    if (PW.vdirty && !view_in_step)
        view_update();
}

void view_set_y(int32_t y)
{
    PW.yview = y;
    PW.vdirty = 1;
}

void view_set_x(int32_t x)
{
    PW.xview = x;
    PW.vdirty = 1;
}

/* view_object[0] and view_hborder[0] of the level rooms (oPlayer1, 160; oOlmec sets oOlmec, 0 until its Alarm_5) */
int16_t play_view_obj = OBJ_oPlayer1, play_hborder = 160;

static void view_update(void)
{
    int i = front_on ? (front_view_obj >= 0 ? instance_first_p(front_view_obj) : NOONE)   /* P8 hook */
                     : instance_first_p(play_view_obj);
    if (i != NOONE) {
        int32_t x = PFLOOR(PX(i).x), y = PFLOOR(PX(i).y), hb = front_on ? front_hborder : play_hborder, vb = PW.vborder;
        if (x - hb < PW.xview) PW.xview = x - hb;
        else if (x + hb > PW.xview + 320) PW.xview = x + hb - 320;
        if (y - vb < PW.yview) PW.yview = y - vb;
        else if (y + vb > PW.yview + 240) PW.yview = y + vb - 240;
        if (PW.xview + 320 > PW.room_w) PW.xview = PW.room_w - 320;
        if (PW.xview < 0) PW.xview = 0;
        if (PW.yview + 240 > PW.room_h) PW.yview = PW.room_h - 240;
        if (PW.yview < 0) PW.yview = 0;
    }
}

/* A frame, as the runner runs it (Observed in the reference traces):
 *   1. the animation: image_index += image_speed x sprite speed, Animation End events. A room_goto() made there
 *      (oPlayer1's exit animation) changes the room at once: the frame ends there, before oGamepad's Step reads
 *      the route (build/trace/p4_exit559_s559: the transition room's first record repeats the last route step);
 *   2. xprevious / yprevious, Begin Step (the tracer's phase-0 record in a new room), alarms, Step, Outside Room,
 *      collision events, End Step (the tracer's phase-1 record);
 *   3. a room_goto() made in 2 changes the room after the event phase that made it (Observed: the transition
 *      room's skip, in oTransition's Step, leaves no End Step record: the level's phase-0 record follows); no
 *      drawing in that frame;
 *   4. the Draw events' side effects (characterDrawEvent sets image_xscale) and the view's target following.
 * Evidence for the order of 1 and 4: build/trace/p4_hang_ladder_s1 record 64 (the duck-to-hang's Animation End
 * turns the player after the previous frame's drawing used the old facing); record 0 of every room shows one
 * animation (oGame's image_index 1) but no drawing (the player's image_xscale 1). */
static void draw_and_view(void)
{
    int k, j, m = 0, n0 = PW.nord;
    /* the instances with a Draw event in creation order (an object with an event is not terrain: the list of the
       alive non-terrain instances, pworld.c, in creation order), then any created meanwhile (pw_ord from n0 on,
       as the scan of every instance reached them) */
    for (k = pw_nthead; k >= 0; k = pw_ntnext[k])
        if (objev(PW.in[k].obj) & EV_DRAW) order[m++] = (int16_t)k;
    for (j = 0; j < m; j++) {
        k = order[j];
        if (PW.in[k].alive && PW.in[k].visible) { ev_draw(k); pcol_event_done(k); }
    }
    for (j = n0; j < PW.nord; j++) {
        k = pw_ord[j];
        if (PW.in[k].alive && PW.in[k].visible && (pobj[PW.in[k].obj].ev & EV_DRAW))
            { ev_draw(k); pcol_event_done(k); }
    }
    ptrans_draw_gui();                                                         /* Draw GUI (after Draw) */
    view_update();
    PW.vdirty = 0;
}

static uint32_t fbits(float f) { union { float f; uint32_t u; } v; v.f = f; return v.u; }

/* image_index += image_speed x the sprite's speed (pspr_anim: speed / 30.0f for type 0, the frame count, as the
   expressions computed them); a one-frame sprite at image_index +0 advancing exactly 1 (image_speed 1, speed 1)
   goes 0 -> 1 -> 0 with an Animation End: the same without the float arithmetic. 1: an Animation End event ran */
static int anim_one(int k)
{
    struct pin *p = &PW.in[k];
    PWST(anim, 1);
    if (!p->alive) return 0;
    if (p->spr < 0) {
        pin_setimg(p, p->img + p->ispd);
        return 0;
    }
    {
        const float *an = pspr_anim[p->spr];
        img_t sp = an[0], fr = an[1];
        play_cur_obj = p->obj;
        if (fbits(p->img) == 0 && fbits(p->ispd) == 0x3f800000u && fbits(sp) == 0x3f800000u &&
            fbits(fr) == 0x3f800000u) {
            if (pobj[p->obj].ev & EV_ANIMEND) { ev_animend(k); pcol_event_done(k); return 1; }
            if (pw_ta_is_on(k)) pw_ta_off(k);            /* terrain: nothing to do until a field changes */
            return 0;
        }
        pin_setimg(p, p->img + p->ispd * sp);
        if (p->img >= fr) {
            pin_setimg(p, p->img - fr);
            if (pobj[p->obj].ev & EV_ANIMEND) { ev_animend(k); pcol_event_done(k); return 1; }
        } else if (p->img < 0) {
            pin_setimg(p, p->img + fr);
            if (pobj[p->obj].ev & EV_ANIMEND) { ev_animend(k); pcol_event_done(k); return 1; }
        }
    }
    return 0;
}

#ifdef PLAY_STATS
#include <stdio.h>
#include <stdlib.h>
/* every terrain instance animate skips does nothing there */
static void anim_check(void)
{
    int k;
    for (k = pw_ahead; k >= 0; k = pw_anext[k]) {
        const struct pin *p = &PW.in[k];
        const float *an;
        if (pin_needs_ext(p->obj) || pw_ta_is_on(k)) continue;
        an = p->spr >= 0 ? pspr_anim[p->spr] : 0;
        if (!an || fbits(p->img) != 0 || fbits(p->ispd) != 0x3f800000u || fbits(an[0]) != 0x3f800000u ||
            fbits(an[1]) != 0x3f800000u || (pobj[p->obj].ev & EV_ANIMEND)) {
            fprintf(stderr, "animate: quiet terrain %d (%s) changed\n", k, objdefs[p->obj].name);
            abort();
        }
    }
}
#endif

/* the instances in creation order (GameMaker's animation pass), those an Animation End event creates (appended)
   included: they are animated in the same pass (Observed: c_ice_barrier_s111 record 102, the oSkeleton that
   oFakeBones' Animation End creates has image_index 0.5 in that step's record). The terrain off the list
   pw_tahead does nothing there, so the walk is the non-terrain list merged with the terrain list by creation
   number. Only an Animation End event creates instances or puts terrain back on the list, so the terrain position
   is found again after one ran. play_cur_obj ends as the full walk leaves it: the object of the last instance with
   a sprite it looked at */
static void animate(void)
{
    int k, a = pw_nthead, t = pw_tahead;
    int16_t s0 = PW.seq, lastseq = -1;
    for (;;) {
        if (a >= 0 && (t < 0 || pw_seq[a] < pw_seq[t])) { k = a; a = pw_ntnext[k]; }
        else if (t >= 0) { k = t; t = pw_tanext[k]; }
        else break;
        if (PW.in[k].alive && PW.in[k].spr >= 0) lastseq = pw_seq[k];
        if (anim_one(k))
            for (t = pw_tahead; t >= 0 && pw_seq[t] <= pw_seq[k]; t = pw_tanext[t]) {}
    }
    k = pw_last_with_sprite(s0);
    if (k >= 0 && pw_seq[k] > lastseq) play_cur_obj = PW.in[k].obj;
#ifdef PLAY_STATS
    anim_check();
#endif
}

/* ---- collision events (HandleCollision): see pcol.c ------------------------------------------------------ */
void pcol_handle(void);

/* ---- level start ----------------------------------------------------------------------------------------- */
static void enemies_out(void)
{
    /* tools/tracer.py TRACE_NOENEMY: instance_destroy(id, false) for oEnemy, oDamsel, oFakeBones */
    int k;
    for (k = 0; k < PW.n; k++)
        if (pin_is(k, OBJ_oEnemy) || pin_is(k, OBJ_oDamsel) || pin_is(k, OBJ_oFakeBones))
            pin_kill(k);
}

/* a generated instance whose struct pin_ext values differ from pin_add's defaults */
static int gen_not_default(const struct inst *g)
{
    int a;
    for (a = 0; a < 12; a++) if (g->alarm[a] != -1) return 1;
    return g->facing || g->status || g->cost || g->value || g->etype || g->style || (g->flags & (IF_FORSALE | IF_HELD)) ||
           g->xvel || g->yvel;
}

void play_level_start(int32_t next_id)
{
    int k;
    static const int16_t rooms[4] = { R_rLevel, R_rLevel2, R_rLevel3, R_rOlmec };
    int levelType = G.levelType;               /* oLevel Create's startMusic runs before oGame Create's scrInitLevel */
    G.gameStart = 1;
    inst_hook = pcol_gen_hook;                                                 /* the collision tree follows */
    if (gen_level(next_id) != 0) PUNTR(9002);
    pw_reset();
    PW.room = rooms[gen_room_for_level()];
    PW.room_w = G.roomW;
    PW.room_h = G.roomH;
    PW.next_id = W.next_id;
    PW.xview = PW.yview = 0;
    PW.vborder = 96;
    PW.vdirty = 0;
    play_view_obj = OBJ_oPlayer1;
    play_hborder = 160;
    PW.step = 0;
    PL.idx = NOONE;
    for (k = 0; k < W.n; k++) {
        const struct inst gk = W.in[k];                /* PX(i), i <= k, is the same memory (play.h) */
        const struct inst *g = &gk;
        int i;
        if (!g->alive) continue;
        i = pin_add(g->obj, PI(g->x), PI(g->y), g->id);
        {
            struct pin *p = &PX(i);
            int a;
            pin_setspr(p, g->spr);
            pin_setdepth(p, g->depth);
            p->treasure = g->treasure;
            if (p->ext) {
                for (a = 0; a < 12; a++) PE(p)->alarm[a] = g->alarm[a];
                PE(p)->facing = g->facing;
                PE(p)->status = g->status;
                PE(p)->cost = g->cost;
                PE(p)->value = g->value;
                PE(p)->etype = g->etype;
                PE(p)->style = g->style;
            } else if (gen_not_default(g))
                PUNTR(9006);                       /* terrain shares the defaults (pworld.c pin_needs_ext) */
        }
        play_gen_inst = g;                         /* P7: the generator's values for FEV_CREATE (fromgen 1) */
        play_gen_created = 0;
        if (PX(i).ext) {                           /* xVel / yVel before the Create hook, which may set them */
            PE(&PX(i))->xVel = NMUL(NI(g->xvel), N(1.0 / 256));
            PE(&PX(i))->yVel = NMUL(NI(g->yvel), N(1.0 / 256));
        }
        if (g->obj == OBJ_oPlayer1) pl_init_from_gen(i);
        else pobj_init_from_gen(i);
        play_gen_inst = 0;
        if (PW.n > k + 1) PUNTR(9007);             /* an instance created here would overwrite W.in[k + 1 ..] */
        {
            struct pin *p = &PX(i);
            p->invincible = (g->flags & IF_INVINCIBLE) != 0;
            p->shopWall = (g->flags & IF_SHOPWALL) != 0;
            p->cleanDeath = (g->flags & IF_CLEANDEATH) != 0;
            if (p->ext) {
                PE(p)->cost = g->cost;
                PE(p)->forSale = (g->flags & IF_FORSALE) != 0;
                PE(p)->held = (g->flags & IF_HELD) != 0;
                if (!play_gen_created) {           /* (the P5 Creates zero them: the generator's again) */
                    PE(p)->xVel = NMUL(NI(g->xvel), N(1.0 / 256));
                    PE(p)->yVel = NMUL(NI(g->yvel), N(1.0 / 256));
                }
                if (obj_is(g->obj, OBJ_oItem) || obj_is(g->obj, OBJ_oTreasure)) PE(p)->value = g->value;
            }
        }
    }
    /* the persistent tracer instance (oGamepad: inputs in its Step, the record in its End Step) */
    pin_add(OBJ_oGamepad, 0, 0, 110219);
    /* oLevel Create: global.xmoney = global.xtime = 0, shake 0; scrHoldItem(global.pickupItem) for oPlayer1 */
    PG.xmoney = 0;
    PG.xtime = 0;
    PG.shake = 0;
    PG.ghostExists = 0;
    PG.drawHUD = 1;
    PLEV.musicFade = 0;
    if (snd_music_on) start_music(levelType);                                           /* :20 */
    for (k = 0; k < PW.n; k++)
        if (PX(k).alive && PE(&PX(k))->held && PL.idx != NOONE) {                    /* the item scrHoldItem gave */
            PL.holdItem = k;
            PL.pickupItemType = PX(k).type;
            PL.whoaTimer = PL.whoaTimerMax;
        }
    /* oGame Create */
    PGAME.drawStatus = 0;
    PGAME.moneyCount = 0;
    PGAME.paused = 0;
    play_time = 1;                                                             /* oGame.time = 1 */
    play_rooms_entered++;
    PW.room_new = 1;
    pcol_load_done();
}

/* scripts/startMusic for a level (global.music: snd_music_on; global.musicVol 15, scrInit's): the room is a level here,
   oLoadLevel never exists. levelType: global.levelType when oLevel Create runs it, before oGame Create's scrInitLevel
   sets this level's (Observed: c_jungle_mantrap_s44, level 5 from the start, plays mCave) */
static void start_music(int levelType)
{
    static const int16_t mus[4] = { SND_mCave, SND_mLush, SND_mIce, SND_mTemple };
    int s;
    snd_start_music();
    if (!snd_music_on) return;
    if (PW.room == R_rOlmec) {
        if (!(PL.idx != NOONE && PL.active)) return;
        s = SND_mBoss;
    } else
        s = mus[levelType >= 1 && levelType <= 3 ? levelType : 0];
    snd_music(s, 1);
    snd_volume(s, 2000 + 8000 * (15 / 18.0));
}

/* ---- one step -------------------------------------------------------------------------------------------- */
static void gamepad_step(uint16_t m)
{
    GP.released = (uint16_t)(GP.down & ~m);
    GP.pressed = (uint16_t)(~GP.down & m);
    GP.down = m;
}

/* enter the room of a room_goto(); 0, or the room if the play loop does not model it */
static int room_change(void)
{
    int r = play_goto_room;
    play_goto_room = -1;
    pdam_room_end();                                                           /* P5 hook: Room End events */
    if (r >= R_rTransition1 && r <= R_rTransition4)
        play_transition_start(r);
    else if (r == R_rLevel || r == R_rLevel2 || r == R_rLevel3 || r == R_rOlmec)
        play_level_start(PW.next_id);
    else if (!(front_on && front_room(r)))                                     /* P8 hook */
        return r;
    PW.room_new = 1;
    return 0;
}

int play_step(uint16_t keys, void (*record_cb)(int phase))
{
    int k, n, a, seq0;
    play_dops = 0;
    view_in_step = 0;
    animate();                                                                 /* 1 */
    if (play_goto_room >= 0) {
        int r = room_change();
        return r ? r : PLAY_ROOM_EARLY;
    }
    for (k = pw_ahead; k >= 0; k = pw_anext[k]) {                              /* 2: xprevious, yprevious */
        PW.in[k].xprev = PW.in[k].x;
        PW.in[k].yprev = PW.in[k].y;
    }
    /* Begin Step: oScreen (drawing surfaces), oGamepad: in a new level room the tracer removes the enemies
       (TRACE_NOENEMY) and writes the phase-0 record */
    if (PW.room_new) {
        if (play_noenemy && isRealLevel()) enemies_out();
        if (record_cb) record_cb(0);
        PW.room_new = 0;
    }
    view_in_step = 1;
    /* an instance created during the alarm phase gets no alarm pass in it, also for the alarms after the one that
       created it (Observed: c_jungle_firefrog_s296 record 246, oFireFrogBomb Alarm_1's oBlood keep alarm[2] 5) */
    seq0 = PW.seq;
    for (a = 0; a < 12; a++) {                                                 /* alarms */
        n = snapshot(a);
        for (k = 0; k < n; k++) {
            int i = order[k];
            struct pin *p = &PX(i);
            if (!p->alive || pw_seq[i] >= seq0) continue;
            if (PE(p)->alarm[a] >= 0) {
                PE(p)->alarm[a] -= 1;
                play_cur_obj = p->obj;
                if (PE(p)->alarm[a] == 0) { ev_alarm(i, a); pcol_event_done(i); }
            }
        }
    }
    if (play_goto_room >= 0) {
        int r = room_change();
        return r ? r : PLAY_ROOM_EARLY;
    }
    n = snapshot(EVK_STEP);                                                  /* Step */
    for (k = 0; k < n; k++) {
        int i = order[k];
        if (!PX(i).alive) continue;
        play_cur_obj = PX(i).obj;
        if (PX(i).obj == OBJ_oGamepad) gamepad_step(keys);
        else {
            ev_step(i);
            pcol_event_done(i);
        }
    }
    if (play_goto_room >= 0)
        return room_change();
    pen_motion();                                                              /* P5 hook: speed / direction */
    n = snapshot(EVK_OUTSIDE);                                               /* Outside Room */
    for (k = 0; k < n; k++) {
        int i = order[k];
        if (!PX(i).alive) continue;
        pcol_touch(i);                                                         /* HandleOther computes the box */
        if (pin_box_outside(i, PW.room_w, PW.room_h)) { ev_outside(i); pcol_event_done(i); }
    }
    pcol_handle();                                                             /* collision events */
    view_in_step = 0;
    if (play_goto_room >= 0)
        return room_change();
    n = snapshot(EVK_END);                                                   /* End Step */
    for (k = 0; k < n; k++) {
        int i = order[k];
        if (!PX(i).alive) continue;
        play_cur_obj = PX(i).obj;
        if (PX(i).obj == OBJ_oGamepad) {
            if (record_cb) record_cb(1);
        } else
            { ev_end_step(i); pcol_event_done(i); }
    }
    PW.step++;
    if (play_goto_room >= 0)                                                   /* 3 */
        return room_change();
    pcol_remove_marked();                                                      /* DoAStep_Draw: RemoveMarked */
    draw_and_view();                                                           /* 4 */
    pw_release();                                                              /* the removed slots go back */
    return 0;
}
