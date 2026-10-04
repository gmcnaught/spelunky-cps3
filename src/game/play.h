/* The play loop: Spelunky Classic HD 1.2.2's rooms after generation, step by step as GameMaker 2024.14's runner
 * runs them, translated statement for statement from the GML (refs/hd/src at tag 1.2.2; file:line in comments).
 *
 * GameMaker semantics modelled (evidence in brackets; build/p4: event logs of the runner, tools/tracer.py
 * TRACE_EVLOG=1, and probes, build/p4/probe):
 *   - one step: xprevious / yprevious, Begin Step, alarms, Step, collision events, End Step, then the animation
 *     (image_index += image_speed x sprite speed, Animation End events), then the Draw events' side effects
 *     (characterDrawEvent sets image_xscale) [event logs: oBomb End Step, the trace record (oGamepad End Step),
 *     oPlayer1 End Step, Animation End, in that order].
 *   - every event type is dispatched object by object in the runtime's object index order (the data file's object
 *     order: build/gen/playtables.c pobj[].rt), each object's instances oldest first; an object without the event
 *     runs its parent's. The instances are taken when the dispatch starts: an instance created during it is not
 *     visited by it (an oWhipPre created in oPlayer1's Step runs its first Step the next step) [event logs].
 *   - alarms: for alarm 0..11 in turn, objects in index order, instances oldest first; alarm >= 0 counts down and
 *     fires on reaching 0, then reads 0 until the next count (-1) [oArrowTrap alarm[1] = 1: fires in step 1, 0 at
 *     its End Step, -1 after; oBlood alarms 0 / 1 / 2 interleaved with oFlame's alarm 0].
 *   - Animation End events: instances in creation order [oPlayer1 before newer oBloodTrail / oExplosion].
 *   - collision events: pairs from the runner's collision tree (HandleCollision, libyoyo.so): see pcol.c.
 *   - round() half to even; collision_point / collision_line: a point is in an instance when
 *     left <= px < right and top <= py < bottom, fractional coordinates as they are; collision_rectangle rounds
 *     its corners half up and needs a non-empty overlap of [x1, x2) with [left, right) (both axes) [probe].
 *   - bounding boxes (non-compatibility mode): left = x + xscale * (l - xorigin) for xscale > 0,
 *     x + xscale * (r + 1 - xorigin) for xscale < 0, right = left + |xscale| * (r - l + 1); fractional x kept
 *     [probe: bbox_left 197.3 for a rock at x 200.3, mirrored and doubled sStandLeft].
 *   - precise masks are stored cropped to the bounding box [probe: sRock's mask].
 *   - spriteless instances: image_index += image_speed without wrapping [trace: oGame 1, 2, 3, ...].
 */
#ifndef PLAY_H
#define PLAY_H
#include <stdint.h>
#include "pnum.h"
#include "objects.h"
#include "gentables.h"
#include "playtables.h"

/* play slots (PIN_DEAD the last): generated levels up to 1,404 instances (lake), 1,537 slots after 300 idle steps
   on a lake level, play adds up to 117 on the routes; tests/game: .data + .bss 450 KB of 512, 72 KB left for the
   stack, the cold arrays in sprite RAM (docs/DRAW.md section 6).
   The line stays bare: the test scripts set it by sed (PIN=n) */
#define PIN_MAX 1792
#define NOONE (-1)

/* item / treasure / misc `type` strings (GML compares strings; here an enum) */
enum ptype {
    T_NONE, T_ROCK, T_JAR, T_SKULL, T_FISHBONE, T_ARROW, T_BOMB, T_ROPE, T_CHEST, T_CRATE, T_GOLDIDOL, T_KEY,
    T_LOCKEDCHEST, T_DAMSEL, T_MACHETE, T_MATTOCK, T_MATTOCKHEAD, T_PISTOL, T_WEBCANNON, T_TELEPORTER, T_SHOTGUN,
    T_BOW, T_FLARE, T_SCEPTRE, T_CRYSTALSKULL, T_LAMP, T_BOMBBAG, T_BOMBBOX, T_ROPEPILE, T_PASTE, T_PARACHUTE,
    T_SPECTACLES, T_GLOVES, T_MITT, T_COMPASS, T_SPRINGSHOES, T_SPIKESHOES, T_JORDANS, T_CAPE, T_JETPACK,
    T_UDJATEYE, T_ANKH, T_CROWN, T_KAPALA, T_FLARECRATE, T_DICE, T_BONES,
    /* treasures */
    T_GOLDCHUNK, T_GOLDNUGGET, T_GOLDBAR, T_GOLDBARS, T_EMERALD, T_BIGEMERALD, T_SAPPHIRE, T_BIGSAPPHIRE,
    T_RUBY, T_BIGRUBY, T_DIAMOND,
    T_WHIP, T_ARROWTRAP, T_OTHER,
    /* enemies' type (penemy.c; "NONE": oEnemy's default, the bat's) */
    T_ENONE, T_SNAKE, T_SPIDER, T_GIANTSPIDER, T_CAVEMAN, T_SKELETON, T_SHOPKEEPER, T_SCARAB,
    /* P7: the other areas' enemy types (each object's Create; oHawkman's is "Yeti" as oYeti's) */
    T_YETI, T_MANTRAP, T_VAMPIRE, T_TOMBLORD, T_MAGMAMAN, T_ALIENBOSS, T_UFO, T_ALIEN, T_FROG, T_FIREFROG, T_MONKEY,
    T_PIRANHA, T_MEGAMOUTH, T_YETIKING, T_COUNT
};
extern const char *const ptype_names[T_COUNT];

/* x, y, sprite_index, mask_index, image_xscale / yscale / angle: written only through the pin_set* setters below
   (a change marks the instance dirty in the collision tree, as the runner's SetPosition / SetSpriteIndex / ...
   do). make -C test/host constcheck compiles the play code with these fields const (PIN_CONST_CHECK): a direct
   write anywhere is an error there */
#ifdef PIN_CONST_CHECK
#define PIN_RO const
#else
#define PIN_RO
#endif
/* An instance: the fields every instance uses (struct pin, PIN_MAX of them) and the GML instance variables
   (struct pin_ext) in a pool, allocated by pin_add for the objects that can use them (pworld.c pin_needs_ext: any
   event besides Create / Destroy, an alarm or a collision event). The others (the terrain: oBrick, oLush, oTemple,
   ...; 70-80 % of a level's instances) share record 0, which holds pin_add's defaults and is never written (the
   PIN_EXT_CHECK build checks it after every event). PE(p) is p's record */
struct pin_ext {
    int16_t alarm[12];      /* -1 off; the routes set at most 210 */
    /* the GML instance variables the translated events use (named as in GML) */
    num xVel, yVel, xAcc, yAcc, myGrav, grav;
    num bounceFactor, frictionFactor, life;
    uint8_t held, armed, safe, heavy, trigger, stuck, sticky, canPickUp, canCollect, falling, bounce, dying;
    uint8_t collectible, forSale, New, breakPieces, active, inDiceHouse;
    uint8_t colLeft, colRight, colBot, colTop, rolling;
    int32_t value, cost;
    int16_t counter, fallCount, burnTimer, fired, facing, state, status, cimg, yOff, hp;
    int16_t trapID, enemyID;  /* instance indices (NOONE) */
    int16_t xAct;
    int16_t en;             /* its struct pin_en (0: the shared zeros) */
    num px, py;             /* oRopeThrow px, py */
    double direction;       /* oArrow */
    double alpha;           /* image_alpha (oSmokePuff: life / 12) */
    int8_t lbo, tbo, rbo, bbo; /* setCollisionBounds offsets */
    uint8_t etype, style;
    uint8_t hasGun;         /* oShopkeeper (the drawing reads it) */
    pos xprev;              /* xprevious, kept for oPlayer1 only (prun.c play_step 2; yprevious is never read); with en
                               moved up, the SH-2's 200 bytes are kept */
};

/* the enemies' variables (penemy.c, pdamsel.c, pshop.c: oEnemy / oDamsel Create's, oEnemySight's motion), in a
   third pool: the oEnemy and oDamsel families and oEnemySight get a record (pworld.c pin_needs_en), the others
   share record 0 (zeros, never written: PIN_EXT_CHECK). PEN(p) is p's record */
struct pin_en {
    uint8_t countsAsKill, swimming, edead, bounced, startled, angered, welcomed;
    int16_t bloodLeft, sacCount, burning, stunTime, sightCounter, squirtTimer, whipped, hit, stunMax;
    int16_t bombID, owner, firing, turnTimer, throwCount;
    num myGravNorm, myGravWater, yVelLimit;
    double hspeed, vspeed;  /* built-in motion (speed, direction): oEnemySight; applied after the Step events */
};

/* field order for the SH-2's displacement reach (mov.b @(disp,Rn) 0-15, mov.w 0-30, mov.l 0-60): bytes, then int16,
   then the 32-bit fields; 64 bytes (pworld.c checks it) */
struct pin {
    uint8_t alive, persistent;
    PIN_RO uint8_t visible;
    /* the bounding box cache (pworld.c pin_bbox): bbk 0 not computed since the last change of x / y / sprite /
       mask / scale (the setters clear it), BB_INT the box is bl, bt, br, bb exactly, BB_DBL computed in double
       each time, BB_NOSPR no sprite */
    uint8_t bbk;
    uint8_t invincible, cleanDeath, shopWall, treasure;
    int16_t obj;            /* OBJ_* */
    int16_t ext;            /* its struct pin_ext (0: the shared defaults) */
    PIN_RO int16_t spr;     /* sprite_index (GSPR_*, -1 none) */
    PIN_RO int16_t mask;    /* mask_index (-1: the sprite) */
    int16_t bl, bt, br, bb;
    int16_t type;           /* enum ptype */
    int16_t exto;           /* ext x sizeof(struct pin_ext) / 8 (pin_set_ext): PE(p) by a shift, not a multiply */
    int32_t id;
    PIN_RO pos x, y;
    PIN_RO float depth;     /* a float in the runner (-99999991 reads -99999992) */
    PIN_RO img_t img;       /* image_index */
    img_t ispd;             /* image_speed */
    PIN_RO float xscale, yscale, angle;  /* floats in the runner; the values set (+-1, whole numbers, (float) angle) */
};


/* PW.in and the generator's W.in are the same memory (pworld.c inst_mem): struct pin (64 bytes) is not larger than
   struct inst (72) and the loaders (play_level_start, play_transition_start) write play instance i only after reading generator instance k
   >= i. Nothing reads W during play */
struct pworld {
    struct pin *in;
    int16_t n;              /* slots used (the high-water mark: every slot below it was used in this room) */
    int16_t nord;           /* pw_ord's length */
    int16_t seq;            /* the next creation number (pw_seq; renumbered from 0 by pw_release near 32767) */
    int32_t next_id;
    int16_t room;           /* room index (names file R lines) of the current room */
    int16_t room_w, room_h;
    int32_t xview, yview;   /* view_xview[0], view_yview[0] (320 x 240) */
    int32_t vborder;        /* view_vborder[0] */
    uint8_t vdirty;         /* the view's position was set this frame (camera_set_view_pos) */
    uint32_t step;          /* steps run in this room */
    uint8_t room_new;       /* a room started since the last step (the tracer's phase-0 record is due) */
};
extern struct pworld PW;
/* creation order: pw_seq[i] is slot i's creation number in this room (older < newer); pw_ord[0 .. PW.nord) the
   slots in creation order (alive, or dead and not yet back on the free list) */
extern int16_t pw_seq[PIN_MAX];
extern int16_t pw_tahead, pw_tanext[PIN_MAX];     /* pworld.c: the terrain to animate (prun.c animate) */
void pw_ta_off(int i);
int pw_ta_is_on(int i);
int pw_last_with_sprite(int16_t s0);
extern int16_t pw_ord[PIN_MAX];
#define PIN_OLDER(a, b) (pw_seq[a] < pw_seq[b])
/* slot reuse: an instance's slot goes back on the free list at the end of the step whose RemoveMarked removed it
   (pw_release); the references kept across steps that pointed to it then point to PIN_DEAD, a slot never used
   (alive 0), as GameMaker's instance id of a destroyed instance names no instance */
#define PIN_DEAD (PIN_MAX - 1)
void pw_release(void);

#define PX(i) (PW.in[i])
#ifndef EXT_MAX
#define EXT_MAX 448              /* struct pin_ext records (0: the shared defaults, EXT_SCRATCH): 381 at most in use
                                    (Olmec, cityOfGold, idle), lake levels 212 with oWater as terrain (DRAW.md 6) */
#endif
#define EXT_SCRATCH (EXT_MAX - 1)  /* never allocated: PIN_DEAD's record and a full ext_alloc's (pworld.c) */
#ifndef EN_MAX
#define EN_MAX 128               /* struct pin_en records (0: the shared zeros) */
#endif
extern struct pin_ext pin_ext[EXT_MAX];
extern struct pin_en pin_en[EN_MAX];
int pin_needs_ext(int obj);                       /* pworld.c: its instances get their own record */
void pw_removed(int i);                           /* RemoveMarked took instance i out: its record is free */
int pw_ext_used_max(void);
int pw_en_used_max(void);
#ifdef PIN_EXT_CHECK
struct pin_ext *pin_ext_checked(const struct pin *p);
struct pin_en *pin_en_checked(const struct pin *p);
#define PE(p) pin_ext_checked(p)
#define PEN(p) pin_en_checked(p)
#else
#define PE(p) ((struct pin_ext *)((char *)pin_ext + ((int32_t)(p)->exto << 3)))
#define PEN(p) (&pin_en[PE(p)->en])
#endif
/* p->ext = e, and the byte offset of its record / 8 (struct pin_ext's size is a multiple of 8: pworld.c checks it) */
static inline void pin_set_ext(struct pin *p, int e)
{
    p->ext = (int16_t)e;
    p->exto = (int16_t)(e * (int)(sizeof(struct pin_ext) / 8));
}
static inline int pin_is(int i, int obj) { return i >= 0 && PW.in[i].alive && obj_is(PW.in[i].obj, obj); }

/* the setters of the collision-relevant fields: store, and on a real change (!=; the scales and the angle as the
   runner's floats) tell the collision tree (pcol.c: CollisionMarkDirty) */
void pw_changed(int i);                           /* pworld.c: the box cache, the solid grid, pcol_changed */
#define PIN_WR(T, f) (*(T *)&(f))
/* p's slot index: its byte offset in PW.in / 64 as an unsigned shift (a signed pointer difference by 64 is a libgcc
   __ashiftrt_r4_6 call on the SH-2, which has no multi-bit arithmetic shift) */
#define PIN_IDX(p) ((int)((uint32_t)((const char *)(p) - (const char *)PW.in) / (uint32_t)sizeof(struct pin)))
static inline void pin_changed_(struct pin *p) { pw_changed(PIN_IDX(p)); }
static inline void pin_setx(struct pin *p, pos v) { pos o = p->x; PIN_WR(pos, p->x) = v; if (o != v) pin_changed_(p); }
static inline void pin_sety(struct pin *p, pos v) { pos o = p->y; PIN_WR(pos, p->y) = v; if (o != v) pin_changed_(p); }
static inline void pin_setxy(struct pin *p, pos x, pos y)
{
    pos ox = p->x, oy = p->y;
    PIN_WR(pos, p->x) = x;
    PIN_WR(pos, p->y) = y;
    if (ox != x || oy != y) pin_changed_(p);
}
static inline void pin_setspr(struct pin *p, int v)         /* sprite_index without pin_set_sprite's image rule */
{
    int o = p->spr;
    PIN_WR(int16_t, p->spr) = (int16_t)v;
    if (o != v) pin_changed_(p);
}
static inline void pin_setmask(struct pin *p, int v)
{
    int o = p->mask;
    PIN_WR(int16_t, p->mask) = (int16_t)v;
    if (o != v) pin_changed_(p);
}
/* image_index, visible, depth: no collision effect; the drawing's dirty mark (pw_draw_mark) on a change */
void pw_draw_mark(int i);
static inline void pin_setimg(struct pin *p, img_t v)
{
    img_t o = p->img;
    PIN_WR(img_t, p->img) = v;
    if (o != v) pw_draw_mark(PIN_IDX(p));
}
static inline void pin_setvisible(struct pin *p, int v)
{
    int o = p->visible;
    PIN_WR(uint8_t, p->visible) = (uint8_t)v;
    if (o != v) pw_draw_mark(PIN_IDX(p));
}
static inline void pin_setdepth(struct pin *p, float v)
{
    float o = p->depth;
    PIN_WR(float, p->depth) = v;
    if (o != v) pw_draw_mark(PIN_IDX(p));
}
static inline void pin_setxscale(struct pin *p, double v)
{
    float o = p->xscale;
    PIN_WR(float, p->xscale) = (float)v;
    if (o != (float)v) pin_changed_(p);
}
static inline void pin_setyscale(struct pin *p, double v)
{
    float o = p->yscale;
    PIN_WR(float, p->yscale) = (float)v;
    if (o != (float)v) pin_changed_(p);
}
static inline void pin_setangle(struct pin *p, double v)
{
    float o = p->angle;
    PIN_WR(float, p->angle) = (float)v;
    if (o != (float)v) pin_changed_(p);
}

/* host cost counters (PLAY_STATS builds: playhost prints them per step): boxes computed in double, instances
   visited by linear scans, the calls of the collision and instance functions */
#ifdef PLAY_STATS
struct pw_stats { uint32_t bbox, bbox_int, visit, point, line, rect, place, exists, with, dist, nearest, snap, snapv,
                  anim; };
extern struct pw_stats pw_st;
#define PWST(f, n) (pw_st.f += (uint32_t)(n))
#else
#define PWST(f, n) ((void)0)
#endif

/* ---- pworld.c: instances, collision functions ------------------------------------------------------------ */
void pw_reset(void);
int pin_add(int obj, pos x, pos y, int32_t id);   /* no event */
int pin_create(pos x, pos y, int obj);            /* instance_create: Create event (nested) */
void pin_destroy(int i);                          /* instance_destroy: Destroy event, then gone */
void pin_kill(int i);                             /* gone without the Destroy event (instance_destroy(id, false)) */
void pin_set_sprite(int i, int spr);
int pin_bbox(int i, double *l, double *t, double *r, double *b);
int pin_ibox(int i, int32_t *b);                  /* the box l, t, r, b when whole numbers (cached); else 0 */
int pin_box_outside(int i, int w, int h);          /* its box is outside [0, w] x [0, h] (Outside Room) */
/* the alive instances of each object in creation order: pw_ohead[obj], then pw_inext[i] (NOONE ends) */
extern int16_t pw_ohead[OBJ_COUNT], pw_inext[PIN_MAX];
extern int16_t pw_ahead, pw_anext[PIN_MAX];        /* every alive instance in creation order */
int pw_count(int obj);                            /* alive instances of obj with its descendants */
/* the drawing's dirty list: the instances whose x, y, sprite, mask, scales, angle, image_index, visible or depth
   changed, or that were created or destroyed (alive 0 then), since pw_draw_dirty_clear; at most one entry each.
   A new room (pw_reset) empties it */
int pw_draw_dirty(const int16_t **list);
/* the terrain near the view (src/draw): pw_grid_sync, then per cell (cx < GRID_W, cy < GRID_H: inst.h)
   pw_grid_cell (the oSolid family) and pw_grid_tcell (the other terrain, pin_needs_ext 0), each list followed by
   pw_grid_next (NOONE ends): the instances with a box whose top-left is in the cell (clamped); a box reaches at
   most pw_grid_extent cells right / down. The other alive instances: pw_nthead, pw_ntnext (creation order) */
void pw_grid_sync(void);
int pw_grid_cell(int cx, int cy);
int pw_grid_tcell(int cx, int cy);
int pw_grid_next(int k);
void pw_grid_extent(int *w, int *h);
extern int16_t pw_nthead, pw_ntnext[PIN_MAX];
void pw_draw_dirty_clear(void);
int collision_point_p(double px, double py, int obj, int prec, int notme_self);
int collision_line_p(double x1, double y1, double x2, double y2, int obj, int prec, int notme_self);
int collision_rect_p(double x1, double y1, double x2, double y2, int obj, int prec, int notme_self);
/* no instance of obj at all, so these return NOONE with no side effect, without evaluating the coordinates (often
   doubles): collision_point when none is alive (it touches only alive ones); collision_line / rectangle when the
   collision tree counts none either (pcol_query gives -1 before any UpdateTree). The functions keep their names */
int pcol_count(int obj);
#define pw_noinst_point(obj) ((obj) >= 0 && pw_count(obj) == 0)
#define pw_noinst_tree(obj) ((obj) >= 0 && pcol_count(obj) == 0)
#define collision_point_p(px, py, obj, prec, notme) \
    (pw_noinst_point(obj) ? NOONE : (collision_point_p)((px), (py), (obj), (prec), (notme)))
#define collision_line_p(x1, y1, x2, y2, obj, prec, notme) \
    (pw_noinst_tree(obj) ? NOONE : (collision_line_p)((x1), (y1), (x2), (y2), (obj), (prec), (notme)))
#define collision_rect_p(x1, y1, x2, y2, obj, prec, notme) \
    (pw_noinst_tree(obj) ? NOONE : (collision_rect_p)((x1), (y1), (x2), (y2), (obj), (prec), (notme)))
/* the same with whole-number coordinates (|v| < 30000), without the double conversions */
int collision_line_i(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int obj, int prec, int notme_self);
int collision_rect_i(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int obj, int prec, int notme_self);
int solid_vline_any(int32_t x, int32_t y1, int32_t y2, int notme_self);   /* collision_line(x, y1, x, y2, oSolid, 1, notme) != NOONE */
int solid_hline_any(int32_t y, int32_t x1, int32_t x2, int notme_self);   /* collision_line(x1, y, x2, y, oSolid, 1, notme) != NOONE */
/* v as an int in (-30000, 30000) when it is a whole number; x and y as ints when both are (inline: the results stay
   in registers, no stack traffic in the collision helpers) */
static inline int pos_int(pos v, int32_t *o)
{
#ifdef PLAY_FIXED
    if ((v & ((1 << PFRAC_BITS) - 1)) != 0) return 0;
    *o = v >> PFRAC_BITS;
    return *o > -30000 && *o < 30000;
#else
    return fwhole(v, o) && *o > -30000 && *o < 30000;
#endif
}
static inline int pin_xy_int(int i, int32_t *x, int32_t *y)
{
    return pos_int(PW.in[i].x, x) && pos_int(PW.in[i].y, y);
}
/* the resting-object skip (pobj.c): the solid summary's change clock, whether a region's cells kept still since a
   clock value, and a count of pw_changed calls on one instance */
uint32_t pw_rest_clock(void);
int pw_rest_still(int32_t l, int32_t t, int32_t r, int32_t b, uint32_t since);
void pw_watch(int i);
uint32_t pw_watch_end(void);
int instance_place_p(int self, double px, double py, int obj);
#define place_meeting_p(self, px, py, obj) (instance_place_p((self), (px), (py), (obj)) != NOONE)
int instance_nearest_p(double px, double py, int obj);
int instance_exists_p(int obj);
int instance_first_p(int obj);
int instance_number_p(int obj);
double distance_to_object_p(int self, int obj);
double distance_to_instance_p(int self, int other);
int pin_overlap(int a, int b);
/* tools/colprobe.py: Collision_Point / Rectangle / Line of instance k, Collision_Instance of a, b */
int pw_test_point(int k, double px, double py, int prec);
int pw_test_rect(int k, double x1, double y1, double x2, double y2, int prec);
int pw_test_line(int k, double x1, double y1, double x2, double y2, int prec);
int pw_test_pair(int a, int b);                    /* Collision_Instance(a, b): bboxes and precise masks */
/* with (obj): the matching instances when it starts, newest first except exactly two: oldest first */
int pw_with(int obj, int16_t *out, int max);

/* ---- prun.c: the step loop -------------------------------------------------------------------------------- */
/* inputs: the route's key mask for the step (tools/tracer.py KEYS bits) */
enum { K_RIGHT = 1, K_LEFT = 2, K_UP = 4, K_DOWN = 8, K_JUMP = 16, K_ATTACK = 32, K_ITEM = 64, K_RUN = 128,
       K_BOMB = 256, K_ROPE = 512, K_FLARE = 1024, K_PAY = 2048, K_START = 4096 };
struct gamepad { uint16_t down, pressed, released; };
extern struct gamepad GP;
void play_level_start(int32_t next_id);           /* generate the level (gen_level) and load it into PW */
/* a read of the view (__view_get, camera_get_view_x / y): after the position was set this frame, the camera
   follows its target at once (see prun.c) */
void view_read(void);
void view_set_y(int32_t y);
void view_set_x(int32_t x);
extern int16_t play_view_obj, play_hborder;      /* view_object[0], view_hborder[0] (oOlmec changes them) */
/* one step with the key mask; record_cb is called at the trace's record point (oGamepad's End Step) */
/* one frame with the route's key mask; record_cb(phase) is called where the tracer writes its records (phase 0:
   the first Begin Step in a room; phase 1: oGamepad's End Step). Returns 0; PLAY_ROOM_EARLY when an Animation
   End event changed the room before the frame's Step (the route step was not used: give it again); or a room
   the play loop does not enter (the caller stops) */
#define PLAY_ROOM_EARLY (-2)
int play_step(uint16_t keys, void (*record_cb)(int phase));
extern int play_untranslated;                     /* set when GML that is not translated is reached */
extern int play_untr_obj;                         /* the object of the instance whose event reached it (-1) */
extern int play_cur_obj;                          /* the object whose event runs (prun.c dispatch) */
#define PUNTR(code) do { if (!play_untranslated) { play_untranslated = (code); play_untr_obj = play_cur_obj; } } while (0)
extern uint32_t play_time;                        /* oGame.time */
extern int32_t play_rooms_entered;
extern int play_noenemy;                          /* 1: remove the enemies at level start (TRACE_NOENEMY) */

/* event entry points (pobj.c, pplayer.c) */
void ev_create(int i);
void ev_destroy(int i);
void ev_step(int i);
void ev_end_step(int i);
void ev_alarm(int i, int a);
void ev_animend(int i);
void ev_collision(int self, int other);
void ev_draw(int i);
void ev_outside(int i);
void pl_init_from_gen(int i);                     /* oPlayer1 Create's variables */
void pobj_init_from_gen(int i);                   /* the Create variables of a generated instance */

/* globals the play loop uses (global.*) beyond the generator's */
struct pglobals {
    int32_t plife, bombs, rope, money, collect, collectCounter, xmoney;
    int32_t shake, kills;
    uint8_t hasCape, hasJetpack, hasGloves, hasSpringShoes, hasJordans, hasSpikeShoes, hasParachute, hasMitt;
    uint8_t hasKapala, hasCrown, hasUdjatEye, hasAnkh, hasStickyBombs, hasCompass, ghostExists, downToRun;
    uint8_t udjatBlink, shakeToggle, drawHUD;
    int32_t time, xtime;
    int16_t arrows;
    /* loot of the level (the transition room shows and counts it down): global.gold ... skulls */
    int16_t gold, nuggets, goldbar, goldbars, emeralds, bigemeralds, sapphires, bigsapphires, rubies, bigrubies;
    int16_t diamonds, xdamsels, scarabs, idols, skulls;
    /* P5: the level's kills the transition room shows (global.bats ... shopkeepers), damsels saved / killed */
    int16_t bats, snakes, spiders, giantspiders, cavemen, skeletons, damselsKilled, shopkeepers, damsels;
    /* P7: the other areas' kinds (global.deadfish ... tomblords) */
    int16_t deadfish, piranhas, zombies, vampires, frogs, firefrogs, monkeys, mantraps, yetis, ufos, aliens;
    int16_t alienbosses, hawkmen, megamouths, yetikings, tomblords;
};
extern struct pglobals PG;

#endif
