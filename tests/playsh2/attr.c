/* tests/playsh2 ATTR builds: exclusive CPU time of a route step by category, through linker wraps (-Wl,--wrap=)
 * of the play loop's entry points (src/game is not edited):
 *   collision   collision_point_p, collision_line_p, collision_rect_p, instance_place_p, instance_nearest_p,
 *               distance_to_object_p, distance_to_instance_p, pin_bbox, pin_overlap (the searches and boxes)
 *   pcol        pcol_handle (the collision-event pass) outside the calls above and the events it runs
 *   searches    instance_exists_p, instance_first_p, instance_number_p, pw_with (scans of the instance list)
 *   events      ev_create, ev_destroy, ev_step (and ev_outside), ev_end_step, ev_alarm, ev_animend, ev_collision,
 *               ev_draw: per event type and object (the instance's object at the call)
 *   dispatch    the rest of play_step: animation, xprevious, the alarm / event snapshots and loops, the view
 * Time is charged to the innermost wrapped call (a collision search inside a Step event is collision; a Create
 * inside a Step is that object's Create). Clocks from the FRC (main.c plat_now). Only route steps are measured.
 * While a collision call runs the PC sampler's tag is cleared, so a PROF build samples everything else.
 * Results in character RAM (scripts/lua/playsh2.lua), each a clocks low word, clocks high word, calls: 0x04180000
 * per category (dispatch, collision, pcol, instance searches); 0x04180100 per event type (8) x object
 * (OBJ_COUNT); 0x0418c000 the collision searches by caller: rows 0, 2, 3 the categories dispatch, pcol and
 * instance searches when no event encloses the call, row 4 + obj the innermost enclosing event's object (any
 * event type; a search inside a search counts once, with the outer call's caller). Each step's collision clocks
 * are the step record's extra word (main.c). The --wrap hooks see only calls between translation units:
 * scripts/playsh2_check.sh builds ATTR with UNITY=0. */
#include "pint.h"
#include "core.h"

uint32_t plat_now(void);                         /* main.c: FRC ticks (x 32 clocks) */
#define R32P(a) ((volatile uint32_t *)(a))
#define CAT ((volatile uint32_t *)0x04180000)    /* [cat * 3]: clocks low, high, calls */
#define EVT ((volatile uint32_t *)0x04180100)    /* [(type * OBJ_COUNT + obj) * 3]: clocks low, high, calls */
#define CLR ((volatile uint32_t *)0x0418c000)    /* [row * 3]: collision searches by caller (row 4 + obj) */
_Static_assert(0x04180100 + 8 * OBJ_COUNT * 12 <= 0x0418c000, "EVT runs into CLR");
_Static_assert(0x0418c000 + (4 + OBJ_COUNT) * 12 <= 0x04190000, "CLR past the area main.c clears");
#define P_TAG (*(volatile uint32_t *)0x04100014)

enum { C_DISPATCH, C_COLL, C_PCOL, C_SEARCH, C_EV };
enum { E_CREATE, E_DESTROY, E_STEP, E_END, E_ALARM, E_ANIMEND, E_COLLISION, E_DRAW, E_N };

static int on, depth;
static uint32_t last, step_coll;
static struct ctx { int16_t cat, type, obj, row; uint32_t tag; } st[64], cur;   /* row: C_COLL's caller */

static void add64(volatile uint32_t *a, uint32_t d)
{
    uint32_t lo = a[0] + d;
    if (lo < d) a[1] += 1;
    a[0] = lo;
}

static void charge(void)
{
    uint32_t t = plat_now(), d = (t - last) * 32;
#ifdef ATTR_DEBUG
    {   /* debug: readings going back, the largest interval */
        volatile uint32_t *dbg = (volatile uint32_t *)0x04180f00;
        if (t < last) { dbg[0]++; dbg[1] = t; dbg[2] = last; dbg[3] = (uint32_t)cur.cat; }
        if (d > dbg[4]) dbg[4] = d;
        dbg[5]++;
    }
#endif
    last = t;
    volatile uint32_t *a = cur.cat == C_EV ? &EVT[(cur.type * OBJ_COUNT + cur.obj) * 3] : &CAT[cur.cat * 3];
    add64(a, d);
    if (cur.cat == C_COLL) { step_coll += d; add64(&CLR[cur.row * 3], d); }
}

static void enter(int cat, int type, int obj)
{
    if (!on) return;
    int16_t row = cur.cat == C_EV ? (int16_t)(4 + cur.obj) : cur.cat == C_COLL ? cur.row : cur.cat;
    charge();
    if (cat == C_COLL && cur.cat != C_COLL) CLR[row * 3 + 2] += 1;
    st[depth++] = cur;
    cur.row = row;
    cur.cat = (int16_t)cat;
    cur.type = (int16_t)type;
    cur.obj = (int16_t)obj;
    if (cat == C_EV) EVT[(type * OBJ_COUNT + obj) * 3 + 2] += 1;
    else CAT[cat * 3 + 2] += 1;
    cur.tag = P_TAG;
    if (cat == C_COLL) P_TAG = 0;                /* the sampler skips collision code */
}

static void leave(void)
{
    if (!on) return;
    charge();
    P_TAG = cur.tag;
    cur = st[--depth];
}

void attr_step_begin(void)
{
    on = 1;
    depth = 0;
    step_coll = 0;
    cur.cat = C_DISPATCH;
    cur.tag = P_TAG;
    last = plat_now();
}

uint32_t attr_step_end(void)                     /* the step's collision clocks */
{
    if (on) charge();
    on = 0;
    return step_coll;
}

/* ---- the wraps ------------------------------------------------------------------------------------------ */
#define W_COLL(rt, name, params, args)                                                                        \
    rt __real_##name params;                                                                                  \
    rt __wrap_##name params                                                                                   \
    {                                                                                                         \
        rt rv_;                                                                                               \
        enter(C_COLL, 0, 0);                                                                                  \
        rv_ = __real_##name args;                                                                             \
        leave();                                                                                              \
        return rv_;                                                                                           \
    }
W_COLL(int, collision_point_p, (double px, double py, int obj, int prec, int ns), (px, py, obj, prec, ns))
W_COLL(int, collision_line_p, (double x1, double y1, double x2, double y2, int obj, int prec, int ns),
       (x1, y1, x2, y2, obj, prec, ns))
W_COLL(int, collision_rect_p, (double x1, double y1, double x2, double y2, int obj, int prec, int ns),
       (x1, y1, x2, y2, obj, prec, ns))
W_COLL(int, instance_place_p, (int self, double px, double py, int obj), (self, px, py, obj))
W_COLL(int, instance_nearest_p, (double px, double py, int obj), (px, py, obj))
W_COLL(double, distance_to_object_p, (int self, int obj), (self, obj))
W_COLL(double, distance_to_instance_p, (int self, int other), (self, other))
W_COLL(int, pin_bbox, (int i, double *l, double *t, double *r, double *b), (i, l, t, r, b))
W_COLL(int, pin_overlap, (int a, int b), (a, b))

#define W_SEARCH(rt, name, params, args)                                                                      \
    rt __real_##name params;                                                                                  \
    rt __wrap_##name params                                                                                   \
    {                                                                                                         \
        rt rv_;                                                                                               \
        enter(C_SEARCH, 0, 0);                                                                                \
        rv_ = __real_##name args;                                                                             \
        leave();                                                                                              \
        return rv_;                                                                                           \
    }
W_SEARCH(int, instance_exists_p, (int obj), (obj))
W_SEARCH(int, instance_first_p, (int obj), (obj))
W_SEARCH(int, instance_number_p, (int obj), (obj))
W_SEARCH(int, pw_with, (int obj, int16_t *out, int max), (obj, out, max))

void __real_pcol_handle(void);
void __wrap_pcol_handle(void) { enter(C_PCOL, 0, 0); __real_pcol_handle(); leave(); }

#define W_EV1(name, type)                                                                                     \
    void __real_##name(int i);                                                                                \
    void __wrap_##name(int i) { enter(C_EV, type, PW.in[i].obj); __real_##name(i); leave(); }
W_EV1(ev_create, E_CREATE)
W_EV1(ev_destroy, E_DESTROY)
W_EV1(ev_step, E_STEP)
W_EV1(ev_outside, E_STEP)
W_EV1(ev_end_step, E_END)
W_EV1(ev_animend, E_ANIMEND)
W_EV1(ev_draw, E_DRAW)
void __real_ev_alarm(int i, int a);
void __wrap_ev_alarm(int i, int a) { enter(C_EV, E_ALARM, PW.in[i].obj); __real_ev_alarm(i, a); leave(); }
void __real_ev_collision(int s, int o);
void __wrap_ev_collision(int s, int o) { enter(C_EV, E_COLLISION, PW.in[s].obj); __real_ev_collision(s, o); leave(); }
