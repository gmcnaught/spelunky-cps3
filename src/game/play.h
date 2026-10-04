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

#define PIN_MAX 4096
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
    T_WHIP, T_ARROWTRAP, T_OTHER, T_COUNT
};
extern const char *const ptype_names[T_COUNT];

struct pin {
    int32_t id;
    int16_t obj;            /* OBJ_* */
    int16_t spr;            /* sprite_index (GSPR_*, -1 none) */
    int16_t mask;           /* mask_index (-1: the sprite) */
    uint8_t alive, visible, persistent;
    uint8_t ingrid;
    pos x, y, xprev, yprev;
    float depth;            /* a float in the runner (-99999991 reads -99999992) */
    img_t img, ispd;        /* image_index, image_speed */
    double xscale, yscale, angle, alpha;
    int32_t alarm[12];
    /* the GML instance variables the translated events use (named as in GML) */
    num xVel, yVel, xAcc, yAcc, myGrav, grav;
    num bounceFactor, frictionFactor, life;
    int16_t type;           /* enum ptype */
    uint8_t held, armed, safe, heavy, trigger, stuck, sticky, canPickUp, canCollect, falling, bounce, dying;
    uint8_t invincible, collectible, cleanDeath, shopWall, forSale, New, breakPieces, active, inDiceHouse;
    uint8_t colLeft, colRight, colBot, colTop, rolled, rolling;
    int32_t value, cost;
    int16_t counter, fallCount, burnTimer, fired, facing, state, status, cimg, yOff, hp;
    int16_t trapID, enemyID;  /* instance indices (NOONE) */
    int16_t xAct;
    num px, py;             /* oRopeThrow px, py */
    double direction;       /* oArrow */
    int8_t lbo, tbo, rbo, bbo; /* setCollisionBounds offsets */
    uint8_t treasure, etype, style;
};

struct pworld {
    struct pin in[PIN_MAX];
    int16_t n;              /* instances created (alive or not); index = creation order */
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

#define PX(i) (PW.in[i])
static inline int pin_is(int i, int obj) { return i >= 0 && PW.in[i].alive && obj_is(PW.in[i].obj, obj); }

/* ---- pworld.c: instances, collision functions ------------------------------------------------------------ */
void pw_reset(void);
int pin_add(int obj, pos x, pos y, int32_t id);   /* no event */
int pin_create(pos x, pos y, int obj);            /* instance_create: Create event (nested) */
void pin_destroy(int i);                          /* instance_destroy: Destroy event, then gone */
void pin_kill(int i);                             /* gone without the Destroy event (instance_destroy(id, false)) */
void pin_set_sprite(int i, int spr);
int pin_bbox(int i, double *l, double *t, double *r, double *b);
int collision_point_p(double px, double py, int obj, int prec, int notme_self);
int collision_line_p(double x1, double y1, double x2, double y2, int obj, int prec, int notme_self);
int collision_rect_p(double x1, double y1, double x2, double y2, int obj, int prec, int notme_self);
int instance_place_p(int self, double px, double py, int obj);
#define place_meeting_p(self, px, py, obj) (instance_place_p((self), (px), (py), (obj)) != NOONE)
int instance_nearest_p(double px, double py, int obj);
int instance_exists_p(int obj);
int instance_first_p(int obj);
int instance_number_p(int obj);
double distance_to_object_p(int self, int obj);
double distance_to_instance_p(int self, int other);
int pin_overlap(int a, int b);                    /* Collision_Instance(a, b): bboxes and precise masks */
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
};
extern struct pglobals PG;

#endif
