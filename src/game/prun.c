/* The step loop of GameMaker 2024.14 as the HD runner runs it (rules and evidence: play.h), the level start, the
 * route inputs (tools/tracer.py's oGamepad replacement), the view and the room changes. */
#include "pint.h"
#include "penemy.h"
#include "pcol.h"
#include "pcontent.h"
#include "../snd/sndgame.h"                     /* the GML sound calls (src/snd) */
#include "front.h"                                     /* P8: the front end's hooks (src/front/front.h) */
#ifdef PLAY_STATS
#include <stdio.h>
#include <stdlib.h>
#endif
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
uint8_t play_toggle_run_on, play_toggle_run;
uint8_t play_god;
int32_t play_god_life;
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
static int16_t evobj[640];                    /* 617 entries with build/gen of 2026-10 */
static int16_t evobj0[17];
/* per key, the objects of evobj[evobj0[key] ..] whose list was non-empty when it was built: evnz[evobj0[key] ..
   + evnzn[key]], valid while pw_onz_gen is evnzg[key] - 1 (no level start since) and evkn[key] is evnzk[key] (no
   list of an object with the event `key` went empty or non-empty since: the same objects of evobj's range have
   instances; prun_onz counts the keys an object has by evobj_init's test). A walk over it visits the same instances
   as one over evobj's: an object it leaves out had an empty list then and still has */
#define EVNZ_N 560                            /* keys 0-14 only (509 entries; the Draw objects are not walked) */
static int16_t evnz[EVNZ_N], evnzn[16];
static uint32_t evnzg[16], evnzk[16], evkn[16];

/* the objects with instances by runtime index: bit rt of onzb set when obj_byrt[rt]'s list is non-empty (prun_onz
   keeps it at each change, prun_onz_reset clears it with the lists at a level start); evkm[key]: bit rt set when
   obj_byrt[rt] is in evobj's range of key (evobj_init). evnz_sync's rebuild takes the bits of both, in runtime order:
   the objects a walk over evobj's range testing pw_ohead finds, in its order (evobj is in runtime order) */
#define RTW ((RTOBJ_COUNT + 31) / 32)
static uint32_t onzb[RTW], evkm[EVK_DRAW][RTW];
static const uint32_t bit32[32] = {
    1u << 0, 1u << 1, 1u << 2, 1u << 3, 1u << 4, 1u << 5, 1u << 6, 1u << 7, 1u << 8, 1u << 9, 1u << 10, 1u << 11,
    1u << 12, 1u << 13, 1u << 14, 1u << 15, 1u << 16, 1u << 17, 1u << 18, 1u << 19, 1u << 20, 1u << 21, 1u << 22,
    1u << 23, 1u << 24, 1u << 25, 1u << 26, 1u << 27, 1u << 28, 1u << 29, 1u << 30, 1u << 31
};

static const uint8_t debruijn[32] = {
    0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
};

void prun_onz_reset(void)
{
    int w;
    for (w = 0; w < RTW; w++) onzb[w] = 0;
}

void prun_onz(int obj)
{
    unsigned m = (unsigned)(pobj[obj].alarms & 0xfff), ev = objev(obj), key, rt = (unsigned)pobj[obj].rt;
    if (pw_ohead[obj] >= 0) onzb[rt >> 5] |= bit32[rt & 31];
    else onzb[rt >> 5] &= ~bit32[rt & 31];
    if (ev & EV_STEP) m |= 1u << EVK_STEP;
    if (ev & EV_OUTSIDE) m |= 1u << EVK_OUTSIDE;
    if (ev & EV_END) m |= 1u << EVK_END;
    for (key = 0; m; key++, m >>= 1)
        if (m & 1) evkn[key]++;
}

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
            if (key < EVK_DRAW) evkm[key][rt >> 5] |= bit32[rt & 31];
#ifdef PLAY_STATS
            if (pobj[o].rt != rt) { fprintf(stderr, "evobj_init: object %d's runtime index %d, not %d\n", o, pobj[o].rt, rt); abort(); }
#endif
        }
    }
    evobj0[16] = (int16_t)n;
    if (evobj0[EVK_DRAW] > EVNZ_N) PUNTR(9004);
}

static void evnz_sync(int k0, int k1)
{
    int key, n;
#ifdef PLAY_STATS
    int j;
#endif
    for (key = k0; key < k1; key++) {
        if (evnzg[key] == pw_onz_gen + 1 && evnzk[key] == evkn[key]) {
#ifdef PLAY_STATS
            /* the host builds: the kept list is the one a rebuild gives */
            for (j = evobj0[key], n = 0; j < evobj0[key + 1]; j++)
                if (pw_ohead[evobj[j]] >= 0 && evobj0[key] + n < EVNZ_N) {
                    if (n >= evnzn[key] || evnz[evobj0[key] + n] != evobj[j]) {
                        fprintf(stderr, "evnz_sync: key %d kept a stale list (object %d)\n", key, evobj[j]);
                        abort();
                    }
                    n++;
                }
            if (n != evnzn[key]) { fprintf(stderr, "evnz_sync: key %d kept a stale list\n", key); abort(); }
#endif
            continue;
        }
        {
            int16_t *out = &evnz[evobj0[key]];
            int lim = EVNZ_N - evobj0[key], w;
            n = 0;
            const uint32_t *km = evkm[key];
            for (w = 0; w < RTW; w++) {
                uint32_t b = onzb[w] & km[w];
                while (b) {                       /* the lowest set bit's index by de Bruijn multiplication */
                    uint32_t lo = b & (0u - b);
                    if (n < lim) out[n++] = (int16_t)obj_byrt[w * 32 + debruijn[(lo * 0x077CB531u) >> 27]];
                    b ^= lo;
                }
            }
        }
#ifdef PLAY_STATS
        {   /* the host builds: the list is the walk over evobj's range */
            int m = 0;
            for (j = evobj0[key]; j < evobj0[key + 1]; j++)
                if (pw_ohead[evobj[j]] >= 0 && evobj0[key] + m < EVNZ_N) {
                    if (m >= n || evnz[evobj0[key] + m] != evobj[j]) { fprintf(stderr, "evnz_sync: key %d rebuilt wrong\n", key); abort(); }
                    m++;
                }
            if (m != n) { fprintf(stderr, "evnz_sync: key %d rebuilt %d objects, the walk %d\n", key, n, m); abort(); }
        }
#endif
        evnzn[key] = (int16_t)n;
        evnzg[key] = pw_onz_gen + 1;
        evnzk[key] = evkn[key];
    }
}

/* the alive instances whose object has the event `key`, in dispatch order: the objects in runtime order, each
   object's instances in creation order (pworld.c's lists) */
static int snapshot(int key)
{
    int j, n = 0;
    if (evobj0[16] == 0) evobj_init();
    PWST(snap, 1);
    evnz_sync(key, key + 1);
    for (j = evobj0[key]; j < evobj0[key] + evnzn[key]; j++) {
        int i;
        for (i = pw_ohead[evnz[j]]; i >= 0; i = pw_inext[i]) {
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
    for (k = pw_nthead; k >= 0; k = pw_ntnext[k]) {
#ifdef PLAY_STATS
        if (!(pw_ntfl[k] & NTF_DRAW) != !(objev(PW.in[k].obj) & EV_DRAW)) { fprintf(stderr, "pw_ntfl %d\n", k); abort(); }
#endif
        if (pw_ntfl[k] & NTF_DRAW) order[m++] = (int16_t)k;   /* (objev(obj) & EV_DRAW: pin_add's byte) */
    }
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
/* anim_one's float tests without soft-float: f a normal number or +-0 (then f * 1.0f is f), and the compares
   a >= b, a < 0 on the bits (the signed-magnitude order mapped to two's complement, -0 as +0) when neither is a NaN */
static int fnormal0(float f) { uint32_t e = fbits(f) & 0x7f800000u; return (e != 0 && e != 0x7f800000u) || (fbits(f) << 1) == 0; }
static int32_t fkey_of(float f) { int32_t b = (int32_t)fbits(f); return b >= 0 ? b : (int32_t)((uint32_t)b ^ 0x7fffffffu) + 1; }
static int fnan(float f) { return (fbits(f) & 0x7fffffffu) > 0x7f800000u; }
static int fkey_ge(float a, float b) { return fnan(a) || fnan(b) ? a >= b : fkey_of(a) >= fkey_of(b); }
static int fkey_lt0(float a) { return fnan(a) ? a < 0 : fkey_of(a) < 0; }

/* image_index += image_speed x the sprite's speed (pspr_anim: speed / 30.0f for type 0, the frame count, as the
   expressions computed them); a one-frame sprite at image_index +0 advancing exactly 1 (image_speed 1, speed 1)
   goes 0 -> 1 -> 0 with an Animation End: the same without the float arithmetic. 1: an Animation End event ran.
   An instance left with image_index's bits as they were and no event goes off the animation list (pworld.c
   pw_tahead): the same run gives the same result until image_index, image_speed or the sprite changes, and each of
   those puts it back */
static int anim_one(int k)
{
    struct pin *p = &PW.in[k];
    uint32_t b0;
    PWST(anim, 1);
    if (!p->alive) return 0;
    b0 = fbits(p->img);
    if (p->spr < 0) {
        pin_setimg(p, p->img + p->ispd);
        if (fbits(p->img) == b0) pw_ta_off(k);
        return 0;
    }
    {
        const float *an = pspr_anim[p->spr];
        img_t sp = an[0], fr = an[1];
        play_cur_obj = p->obj;
        if (b0 == 0 && fbits(p->ispd) == 0x3f800000u && fbits(sp) == 0x3f800000u && fbits(fr) == 0x3f800000u) {
            if (pobj[p->obj].ev & EV_ANIMEND) { ev_animend(k); pcol_event_done(k); return 1; }
            pw_ta_off(k);                                /* nothing to do until a field changes */
            return 0;
        }
        pin_setimg(p, p->img + (fbits(sp) == 0x3f800000u && fnormal0(p->ispd) ? p->ispd : p->ispd * sp));
        if (fkey_ge(p->img, fr)) {
            pin_setimg(p, p->img - fr);
            if (pobj[p->obj].ev & EV_ANIMEND) { ev_animend(k); pcol_event_done(k); return 1; }
        } else if (fkey_lt0(p->img)) {
            pin_setimg(p, p->img + fr);
            if (pobj[p->obj].ev & EV_ANIMEND) { ev_animend(k); pcol_event_done(k); return 1; }
        }
        if (fbits(p->img) == b0) pw_ta_off(k);
    }
    return 0;
}

/* instance k would leave anim_one with nothing done but pw_ta_off (its first branch: image_index 0, image_speed and
   the sprite's advance and frame count 1, no Animation End event); then taking it off the list now is what the next
   animation pass does to it (ptrans.c: the transition room's blocks, off before its first step) */
int prun_anim_idle(int k)
{
    const struct pin *p = &PW.in[k];
    const float *an;
    if (!p->alive || p->spr < 0) return 0;
    an = pspr_anim[p->spr];
    return fbits(p->img) == 0 && fbits(p->ispd) == 0x3f800000u && fbits(an[0]) == 0x3f800000u &&
           fbits(an[1]) == 0x3f800000u && !(pobj[p->obj].ev & EV_ANIMEND);
}

#ifdef PLAY_STATS
#include <stdio.h>
#include <stdlib.h>
/* every alive instance off the animation list: anim_one's arithmetic on it would leave image_index's bits and run
   no event */
static void anim_check(void)
{
    int k, last = -1;
    for (k = pw_tahead; k >= 0; k = pw_tanext[k]) {          /* the list: alive, in creation order */
        if (!PW.in[k].alive || pw_seq[k] <= last) {
            fprintf(stderr, "animate: list entry %d (%s) dead or out of order\n", k, objdefs[PW.in[k].obj].name);
            abort();
        }
        last = pw_seq[k];
    }
    for (k = pw_ahead; k >= 0; k = pw_anext[k]) {
        const struct pin *p = &PW.in[k];
        int act;
        if (!p->alive || pw_ta_is_on(k)) continue;
        if (p->spr < 0) act = fbits(p->img + p->ispd) != fbits(p->img);
        else {
            const float *an = pspr_anim[p->spr];
            img_t sp = an[0], fr = an[1], v;
            int ev = (pobj[p->obj].ev & EV_ANIMEND) != 0;
            if (fbits(p->img) == 0 && fbits(p->ispd) == 0x3f800000u && fbits(sp) == 0x3f800000u &&
                fbits(fr) == 0x3f800000u)
                act = ev;
            else {
                v = p->img + p->ispd * sp;
                act = 0;
                if (v >= fr) { v = v - fr; act = ev; }
                else if (v < 0) { v = v + fr; act = ev; }
                act = act || fbits(v) != fbits(p->img);
            }
        }
        if (act) {
            fprintf(stderr, "animate: instance %d (%s) off the list would change\n", k, objdefs[p->obj].name);
            abort();
        }
    }
}
#endif

/* the instances in creation order (GameMaker's animation pass), those an Animation End event creates (appended)
   included: they are animated in the same pass (Observed: c_ice_barrier_s111 record 102, the oSkeleton that
   oFakeBones' Animation End creates has image_index 0.5 in that step's record). The instances off the list
   pw_tahead do nothing there, so the walk is that list. Only an Animation End event creates instances or puts one
   back on the list, so the position is found again after one ran. play_cur_obj ends as the full walk leaves it: the
   object of the last instance with a sprite (pw_last_with_sprite for those older than the walk, which the walk
   skips when they are off the list; the ones created during it are on it) */
static void animate(void)
{
    int k, t = pw_tahead;
    int16_t s0 = PW.seq, lastseq = -1;
    while ((k = t) >= 0) {
        t = pw_tanext[k];
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
    int darkPrev = G.darkLevel;                /* and its darkness (:16-17): the darkLevel before scrInitLevel's */
    /* the event-object table (a function of the object tables only) is built here, out of the first play_step
       (393 K jtcps3 clocks there on p5_l4); the step's own check stays for callers that start without a level */
    if (evobj0[16] == 0) evobj_init();
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
        struct inst gk;                                /* PX(i), i <= k, is the same memory (play.h) */
        const struct inst *g = &gk;
        int i;
        PIN_INST_BARRIER();
        gk = W.in[k];
        PIN_INST_BARRIER();
        if (!g->alive) continue;
        i = pin_add(g->obj, PI(g->x), PI(g->y), g->id);
        if (i == PIN_DEAD) continue;               /* full (PUNTR 9001): the rest of the level is not loaded */
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
                    PE(p)->xVel = ND(g->xvel / 256.0);
                    PE(p)->yVel = ND(g->yvel / 256.0);
                }
                if (obj_is(g->obj, OBJ_oItem) || obj_is(g->obj, OBJ_oTreasure)) PE(p)->value = g->value;
            }
        }
    }
    /* the persistent tracer instance (oGamepad: inputs in its Step, the record in its End Step) */
    pin_add(OBJ_oGamepad, 0, 0, RUNNER_ID_GAMEPAD);
    /* oLevel Create: global.xmoney = global.xtime = 0, shake 0; scrHoldItem(global.pickupItem) for oPlayer1 */
    PG.xmoney = 0;
    PG.xtime = 0;
    PG.shake = 0;
    PG.ghostExists = 0;
    PG.drawHUD = 1;
    PLEV.musicFade = 0;
    PLEV.darkness = darkPrev ? 1 : 0;                                          /* oLevel Create :16-17 */
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
    pcol_load_flush();                         /* the grid build's pending collision updates (ptrans.c's reason) */
    /* the solid grid's first build (every loaded oSolid-family entry: grid_flush_run, bbkind_set) here instead of at
       the first play_step's first query (about 0.6 M jtcps3 clocks there on p5_l4). The grid's answers do not depend
       on when its pending updates are applied: grid_point keeps the oldest hit (PIN_OLDER), the line scans and the
       cell summary test for any hit (the summary's representative block only decides when the scan runs), and an
       entry that changes before the first query is marked again and re-put then. pw_rest_clock values move earlier;
       rest records compare only clocks taken after them */
    pw_grid_sync();
    /* the instances the first step's animation pass would only take off its list (prun_anim_idle: the blocks, most
       of the level's ~600 instances) are taken off here, after every Create: as at the transition room's load
       (ptrans.c), nothing between here and that pass changes a sprite, image_index or image_speed without putting
       the instance back (pin_setspr, the loaders' raw sprite write, is not used after a load) */
    {
        int nx;
        for (k = pw_tahead; k >= 0; k = nx) {
            nx = pw_tanext[k];
            if (prun_anim_idle(k)) pw_ta_off(k);
        }
    }
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

#if PLAY_DEACT
/* ---- off-view deactivation (docs/DEACT.md; tools/tracer.py TRACE_DEACT, gml_GlobalScript_trcDeact trcDeactPass) --
   oGamepad's Begin Step, every step of a level room but its first: (1) the instances the pass deactivated whose
   (x, y) then is inside the view grown by PLAY_DEACT are activated, in the order they were deactivated; (2) the alive
   candidates outside it are deactivated, in `with (all)` order (newest first), and listed with their (x, y) */
#define DL_MAX 320                                /* deactivated at once; more stops the play loop (untranslated 9010) */
static int16_t dl_i[DL_MAX];                      /* the instances the pass deactivated, in that order */
static int dl_n;
static uint8_t dbits[(OBJ_COUNT + 7) / 8], dbits_ok;   /* the candidate objects */
/* the bit by a table: GCC compiled 1 << (o & 7) as a variable right shift of the byte (libgcc __ashrsi3 a call, one
   per instance of the pass) */
static const uint8_t dbit8[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };
#define DCAND(o) (dbits[(o) >> 3] & dbit8[(o) & 7])

static void dcand_init(void)
{
    static const int16_t ex[] = { OBJ_oShopkeeper, OBJ_oShopkeeper2, OBJ_oBomb, OBJ_oRopeThrow, OBJ_oFlare,
                                  OBJ_oFireFrogArmed, OBJ_oFireFrogBomb, OBJ_oDamsel, OBJ_oDice, OBJ_oLampItem,
                                  OBJ_oLampRedItem, OBJ_oJaws };
    unsigned k;
    int o;
    for (o = 0; o < OBJ_COUNT; o++)
        if (obj_is(o, OBJ_oEnemy) || obj_is(o, OBJ_oItem) || obj_is(o, OBJ_oTreasure)) dbits[o >> 3] |= (uint8_t)(1 << (o & 7));
    for (k = 0; k < sizeof ex / sizeof ex[0]; k++) dbits[ex[k] >> 3] &= (uint8_t)~(1 << (ex[k] & 7));
    dbits_ok = 1;
}

/* the view tests on the bits of x, y (PLTI / PGTI: gcmp_fi). At a whole position (the integer shadows, play.h
   pin_xy_int_p) x - v is a whole number for an int v, beyond eps unless 0: PLTI(x, v) is x < v and PGTI(x, v) x > v */
static __attribute__((noinline)) int doutside_slow(const struct pin *p, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    int32_t x, y;
    if (pin_xy_int_p(p, &x, &y)) {
        int r = x < x0 || x > x1 || y < y0 || y > y1;
#ifdef PLAY_STATS
        if (r != (PLTI(p->x, x0) || PGTI(p->x, x1) || PLTI(p->y, y0) || PGTI(p->y, y1))) {
            fprintf(stderr, "doutside: integer answer %d differs\n", r);
            abort();
        }
#endif
        return r;
    }
    return POUTI(p->x, x0, x1) || POUTI(p->y, y0, y1);
}

/* doutside_slow's first case (both shadows known and whole: pin_xy_int_p's first branch) inline in the pass's loops,
   without the call and the stack of the out-parameters; the rest (and every case in the PLAY_STATS builds, with their
   checks) out of line */
static inline int doutside(const struct pin *p, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
#ifndef PLAY_STATS
    int32_t x = p->ix, y = p->iy;
    if (x > -30000 && y > -30000) return x < x0 || x > x1 || y < y0 || y > y1;
#endif
    return doutside_slow(p, x0, y0, x1, y1);
}

/* (1) reads a listed instance's x, y now, as the GML reads them through its id (a deactivated instance's variables
   stay readable: docs/DEACT.md 2) */
static void deact_pass(void)
{
    int32_t x0 = PW.xview - PLAY_DEACT, y0 = PW.yview - PLAY_DEACT;
    int32_t x1 = PW.xview + 320 + PLAY_DEACT, y1 = PW.yview + 240 + PLAY_DEACT;
    int16_t *cand = order;                        /* (the snapshot's array: free before the alarm passes) */
    int k, n = 0, nc = 0, i;
    if (!dbits_ok) dcand_init();                  /* (a room entered without its first step: not on any route) */
    for (k = 0; k < dl_n; k++) {
        i = dl_i[k];
        if (doutside(&PX(i), x0, y0, x1, y1)) dl_i[n++] = (int16_t)i;
        else pw_activate(i);
    }
    dl_n = n;
    for (i = pw_nthead; i >= 0; i = pw_ntnext[i]) {   /* creation order; the list below takes them newest first */
        const struct pin *p = &PX(i);
#ifdef PLAY_STATS
        if (!(pw_ntfl[i] & NTF_DCAND) != !DCAND(p->obj)) { fprintf(stderr, "pw_ntfl %d\n", i); abort(); }
#endif
        if (!(pw_ntfl[i] & NTF_DCAND) || !doutside(p, x0, y0, x1, y1)) continue;   /* (DCAND(p->obj)) */
        if (p->ext && (PE(p)->held || PE(p)->forSale)) continue;
        cand[nc++] = (int16_t)i;
    }
    if (dl_n + nc > DL_MAX) { PUNTR(9010); return; }
    for (k = 0; k < nc; k++) dl_i[dl_n + k] = cand[nc - 1 - k];
    pw_deactivate_n(dl_i + dl_n, nc);
    dl_n += nc;
}
#endif

/* pw_ntfl's bits of an object (play.h; pin_add keeps them per slot): its Draw event (objev's EV_DRAW bit: oGamepad's
   extra bits are others) and deact_pass's candidacy (DCAND: dbits is made once and never changes) */
int prun_ntfl(int obj)
{
    int f = (pobj[obj].ev & EV_DRAW) ? NTF_DRAW : 0;
#if PLAY_DEACT
    if (!dbits_ok) dcand_init();
    if (DCAND(obj)) f |= NTF_DCAND;
#endif
    return f;
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
    else if (!((front_on || r == R_rEnd) && front_room(r)))                    /* P8 hook (rEnd: the ending, */
        return r;                                                              /* src/front, from rOlmec) */
    PW.room_new = 1;
    return 0;
}

int play_step(uint16_t keys, void (*record_cb)(int phase))
{
    int k, n, a, seq0, gp_skip = 0;
    play_dops = 0;
    view_in_step = 0;
    animate();                                                                 /* 1 */
    if (play_goto_room >= 0) {
        int r = room_change();
        return r ? r : PLAY_ROOM_EARLY;
    }
    /* 2: xprevious, yprevious. Only oPlayer1's are read (objects/oPlayer1/Collision_oPushBlock.gml; no other
       HD object reads xprevious / yprevious), so only its instances keep them (docs/PERF2.md E) */
    for (k = pw_ohead[OBJ_oPlayer1]; k >= 0; k = pw_inext[k]) {
        if (PW.in[k].ext) PE(&PW.in[k])->xprev = PW.in[k].x;
    }
    /* Begin Step: oScreen (drawing surfaces), oGamepad: in a new level room the tracer removes the enemies
       (TRACE_NOENEMY) and writes the phase-0 record */
    if (PW.room_new) {
        if (play_noenemy && isRealLevel()) enemies_out();
        if (record_cb) record_cb(0);
        PW.room_new = 0;
#if PLAY_DEACT
        dl_n = 0;
        if (!dbits_ok) dcand_init();              /* (here, not in the room's second step: a read of obj_anc's rows) */
    } else if (PW.room == R_rLevel || PW.room == R_rLevel2 || PW.room == R_rLevel3) {
        deact_pass();
#endif
    }
    view_in_step = 1;
    /* an instance created during the alarm phase gets no alarm pass in it, also for the alarms after the one that
       created it (Observed: c_jungle_firefrog_s296 record 246, oFireFrogBomb Alarm_1's oBlood keep alarm[2] 5) */
    /* the alarm passes walk the objects' lists instead of a snapshot (docs/PERF2.md E): the same instances in the
       same order. Destroying an instance unlinks it but keeps its pw_inext (its successor then, or a later one),
       and its slot is not reused before pw_release, so the walk goes on to the instances that follow; the ones
       created during the passes are appended to the lists and skipped by their pw_seq, as the snapshot leaves
       them out */
    seq0 = PW.seq;
    if (evobj0[16] == 0) evobj_init();
    /* the objects with instances when the alarm passes start: one empty then gets only instances created during
       the passes (pw_seq >= seq0, skipped below), so leaving it out changes nothing */
    evnz_sync(0, 12);
    for (a = 0; a < 12; a++) {                                                 /* alarms */
        int j;
        for (j = evobj0[a]; j < evobj0[a] + evnzn[a]; j++) {
            int i;
            for (i = pw_ohead[evnz[j]]; i >= 0; i = pw_inext[i]) {
                struct pin *p = &PX(i);
                if (!p->alive || pw_seq[i] >= seq0) continue;
                if (PE(p)->alarm[a] >= 0) {
                    PE(p)->alarm[a] -= 1;
                    play_cur_obj = p->obj;
                    if (PE(p)->alarm[a] == 0) { ev_alarm(i, a); pcol_event_done(i); }
                }
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
        if (PX(i).obj == OBJ_oPiranha && (a = pswamp_piranha_run(order + k, n - k)) > 0) {
            k += a - 1;                                                        /* idle piranhas in a row (pk_swamp.c) */
            continue;
        }
        if (PX(i).obj == OBJ_oGamepad) {
            /* the ending's rooms (src/front): the runner changes the room once the event that called room_goto
               ends, the rest of the Step dispatch does not run (build/trace/g_end_win_s7 record 1274: oEnd3, before
               oGamepad, leaves rEnd3 with the route step unused); src/front skips the other instances' Steps */
            if (play_goto_room >= 0 && (PW.room == R_rEnd || PW.room == R_rEnd2 || PW.room == R_rEnd3 ||
                                        PW.room == R_rCredits2)) gp_skip = 1;
            else gamepad_step(keys);
        } else if (!front_on && (((PX(i).obj == OBJ_oTree || PX(i).obj == OBJ_oTreeBranch || PX(i).obj == OBJ_oLeaves ||
                                   PX(i).obj == OBJ_oGrave) &&
                                   pjungle_idle(i)) ||
                                 ev_step_idle(i))) {
            /* ev_step would find nothing to do: oTree / oTreeBranch out of view (it reaches pjungle_ev's Step through
               stepk SK_OWN, ptrans_step, the oTreasure / oItem tests and pcontent_step's claimant 1: caches and tests
               only), or a treasure out of view (stepk SK_TREASURE: treasure_step's first test) */
            pcol_event_done(i);
        } else {
            ev_step(i);
            pcol_event_done(i);
        }
    }
    if (play_goto_room >= 0) {
        int r = room_change();
        return r ? r : gp_skip ? PLAY_ROOM_EARLY : 0;
    }
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
