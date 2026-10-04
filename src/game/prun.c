/* The step loop of GameMaker 2024.14 as the HD runner runs it (rules and evidence: play.h), the level start, the
 * route inputs (tools/tracer.py's oGamepad replacement), the view and the room changes. */
#include "pint.h"

struct gamepad GP;
struct pglobals PG;
int play_untranslated;
int play_untr_obj = -1;
int play_cur_obj = -1;
uint32_t play_time;
int play_goto_room = -1;
int32_t play_rooms_entered;
uint32_t play_dops;
#ifdef NUM_IS_CLASS
struct dcount play_dcount;
#endif

/* ---- dispatch order: object index order (pobj[].rt), then creation order -------------------------------- */
static int16_t order[PIN_MAX];
static int16_t bucket_n[RTOBJ_COUNT + 1];

/* the tracer gives oGamepad a Begin Step and an End Step (tools/tracer.py) */
static uint16_t objev(int obj)
{
    return (uint16_t)(pobj[obj].ev | (obj == OBJ_oGamepad ? EV_BEGIN | EV_END : 0));
}

/* the alive instances whose object has `ev` (EV_* bit; 0: every instance), in dispatch order */
static int snapshot(uint16_t ev, uint16_t alarm_bit)
{
    int k, n = 0, b;
    static int16_t start[RTOBJ_COUNT + 1];
    for (b = 0; b <= RTOBJ_COUNT; b++) bucket_n[b] = 0;
    for (k = 0; k < PW.n; k++) {
        const struct pin *p = &PW.in[k];
        if (!p->alive) continue;
        if (ev && !(objev(p->obj) & ev)) continue;
        if (alarm_bit && !(pobj[p->obj].alarms & alarm_bit)) continue;
        bucket_n[pobj[p->obj].rt]++;
        n++;
    }
    start[0] = 0;
    for (b = 0; b < RTOBJ_COUNT; b++) start[b + 1] = (int16_t)(start[b] + bucket_n[b]);
    for (k = 0; k < PW.n; k++) {
        const struct pin *p = &PW.in[k];
        if (!p->alive) continue;
        if (ev && !(objev(p->obj) & ev)) continue;
        if (alarm_bit && !(pobj[p->obj].alarms & alarm_bit)) continue;
        order[start[pobj[p->obj].rt]++] = (int16_t)k;
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
void view_read(void)
{
    if (PW.vdirty)
        view_update();
}

void view_set_y(int32_t y)
{
    PW.yview = y;
    PW.vdirty = 1;
}

static void view_update(void)
{
    int i = instance_first_p(OBJ_oPlayer1);
    if (i != NOONE) {
        int32_t x = PFLOOR(PX(i).x), y = PFLOOR(PX(i).y), hb = 160, vb = PW.vborder;
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
    int k;
    for (k = 0; k < PW.n; k++)
        if (PW.in[k].alive && PW.in[k].visible && (pobj[PW.in[k].obj].ev & EV_DRAW))
            ev_draw(k);
    view_update();
    PW.vdirty = 0;
}

static void animate(void)
{
    int k, n = PW.n;
    for (k = 0; k < n; k++) {
        struct pin *p = &PW.in[k];
        if (!p->alive) continue;
        if (p->spr < 0) {
            p->img = p->img + p->ispd;
            continue;
        }
        {
            const struct psprite *s = &psprite[p->spr];
            img_t sp = s->stype == 1 ? s->speed : s->speed / 30.0f;
            img_t fr = (img_t)s->frames;
            p->img = p->img + p->ispd * sp;
            play_cur_obj = p->obj;
            if (p->img >= fr) {
                p->img = p->img - fr;
                if (pobj[p->obj].ev & EV_ANIMEND) ev_animend(k);
            } else if (p->img < 0) {
                p->img = p->img + fr;
                if (pobj[p->obj].ev & EV_ANIMEND) ev_animend(k);
            }
        }
    }
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

void play_level_start(int32_t next_id)
{
    int k;
    static const int16_t rooms[4] = { R_rLevel, R_rLevel2, R_rLevel3, R_rOlmec };
    G.gameStart = 1;
    if (gen_level(next_id) != 0) PUNTR(9002);
    pw_reset();
    PW.room = rooms[gen_room_for_level()];
    PW.room_w = G.roomW;
    PW.room_h = G.roomH;
    PW.next_id = W.next_id;
    PW.xview = PW.yview = 0;
    PW.vborder = 96;
    PW.vdirty = 0;
    PW.step = 0;
    PL.idx = NOONE;
    for (k = 0; k < W.n; k++) {
        const struct inst *g = &W.in[k];
        int i;
        if (!g->alive) continue;
        i = pin_add(g->obj, PI(g->x), PI(g->y), g->id);
        {
            struct pin *p = &PX(i);
            int a;
            p->spr = g->spr;
            p->depth = g->depth;
            for (a = 0; a < 12; a++) p->alarm[a] = g->alarm[a];
            p->facing = g->facing;
            p->status = g->status;
            p->cost = g->cost;
            p->value = g->value;
            p->treasure = g->treasure;
            p->etype = g->etype;
            p->style = g->style;
        }
        if (g->obj == OBJ_oPlayer1) pl_init_from_gen(i);
        else pobj_init_from_gen(i);
        {
            struct pin *p = &PX(i);
            p->cost = g->cost;
            p->forSale = (g->flags & IF_FORSALE) != 0;
            p->invincible = (g->flags & IF_INVINCIBLE) != 0;
            p->shopWall = (g->flags & IF_SHOPWALL) != 0;
            p->cleanDeath = (g->flags & IF_CLEANDEATH) != 0;
            p->held = (g->flags & IF_HELD) != 0;
            p->xVel = NMUL(NI(g->xvel), N(1.0 / 256));
            p->yVel = NMUL(NI(g->yvel), N(1.0 / 256));
            if (obj_is(g->obj, OBJ_oItem) || obj_is(g->obj, OBJ_oTreasure)) p->value = g->value;
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
    for (k = 0; k < PW.n; k++)
        if (PX(k).alive && PX(k).held && PL.idx != NOONE) {                    /* the item scrHoldItem gave */
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
    if (r >= R_rTransition1 && r <= R_rTransition4)
        play_transition_start(r);
    else if (r == R_rLevel || r == R_rLevel2 || r == R_rLevel3 || r == R_rOlmec)
        play_level_start(PW.next_id);
    else
        return r;
    PW.room_new = 1;
    return 0;
}

int play_step(uint16_t keys, void (*record_cb)(int phase))
{
    int k, n, a;
    play_dops = 0;
    animate();                                                                 /* 1 */
    if (play_goto_room >= 0) {
        int r = room_change();
        return r ? r : PLAY_ROOM_EARLY;
    }
    for (k = 0; k < PW.n; k++) {                                               /* 2: xprevious, yprevious */
        PW.in[k].xprev = PW.in[k].x;
        PW.in[k].yprev = PW.in[k].y;
    }
    /* Begin Step: oScreen (drawing surfaces), oGamepad: in a new level room the tracer removes the enemies
       (TRACE_NOENEMY) and writes the phase-0 record */
    if (PW.room_new) {
        if (isRealLevel()) enemies_out();
        if (record_cb) record_cb(0);
        PW.room_new = 0;
    }
    for (a = 0; a < 12; a++) {                                                 /* alarms */
        n = snapshot(0, (uint16_t)(1u << a));
        for (k = 0; k < n; k++) {
            int i = order[k];
            struct pin *p = &PX(i);
            if (!p->alive) continue;
            if (p->alarm[a] >= 0) {
                p->alarm[a] -= 1;
                play_cur_obj = p->obj;
                if (p->alarm[a] == 0) ev_alarm(i, a);
            }
        }
    }
    if (play_goto_room >= 0) {
        int r = room_change();
        return r ? r : PLAY_ROOM_EARLY;
    }
    n = snapshot(EV_STEP, 0);                                                  /* Step */
    for (k = 0; k < n; k++) {
        int i = order[k];
        if (!PX(i).alive) continue;
        play_cur_obj = PX(i).obj;
        if (PX(i).obj == OBJ_oGamepad) gamepad_step(keys);
        else ev_step(i);
    }
    if (play_goto_room >= 0)
        return room_change();
    n = snapshot(EV_OUTSIDE, 0);                                               /* Outside Room */
    for (k = 0; k < n; k++) {
        int i = order[k];
        double l, t, r, b;
        if (!PX(i).alive || !pin_bbox(i, &l, &t, &r, &b)) continue;
        if (r < 0 || l > PW.room_w || b < 0 || t > PW.room_h) ev_outside(i);
    }
    pcol_handle();                                                             /* collision events */
    if (play_goto_room >= 0)
        return room_change();
    n = snapshot(EV_END, 0);                                                   /* End Step */
    for (k = 0; k < n; k++) {
        int i = order[k];
        if (!PX(i).alive) continue;
        play_cur_obj = PX(i).obj;
        if (PX(i).obj == OBJ_oGamepad) {
            if (record_cb) record_cb(1);
        } else
            ev_end_step(i);
    }
    PW.step++;
    if (play_goto_room >= 0)                                                   /* 3 */
        return room_change();
    draw_and_view();                                                           /* 4 */
    return 0;
}
