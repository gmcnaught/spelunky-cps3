/* Instance model for the level generator: GameMaker instances in creation order, with GameMaker's ids, object,
 * position, sprite and the per-object variables the generator and play need.
 *
 * GameMaker semantics modelled (evidence: tools/tracer.py generator dumps of the HD 1.2.2 runner, the runtime's
 * bytecode (UndertaleModTool disassembly) and the GameMaker HTML5 runner's collision code):
 *   - ids: room instances keep the ids stored in the room (rLevel: INST_RLEVEL_IDS); every instance_create takes
 *     the next id of one global counter (+1 each).
 *   - with (obj) and every instance search (collision_*, instance_place, obj.var) visit the instances newest
 *     first (reverse creation order), children of obj included, destroyed ones skipped. `with` visits the
 *     instances that existed when it started.
 *   - instance_create runs the Create event (with its event_inherited chain) before returning.
 *   - instance_destroy runs the Destroy event at once; the instance is gone for every later test.
 *   - collisions use GameMaker 2's non-compatibility bounding boxes (option_collision_compatibility false):
 *     [x - xorigin + bbox_left, that + bbox width) half-open; see inst.c.
 */
#ifndef INST_H
#define INST_H
#include <stdint.h>
#include "objects.h"
#include "gentables.h"

#define INST_MAX 4096
#define INST_NONE (-1)

/* flags */
#define IF_INVINCIBLE  0x01
#define IF_SHOPWALL    0x02
#define IF_CLEANDEATH  0x04
#define IF_FORSALE     0x08
#define IF_INDICEHOUSE 0x10

#define ALARMS 12

/* terrain grid: 16-px cells over x 0..767, y 0..639 */
#define GRID_W 48
#define GRID_H 40

/* shop styles (oShopkeeper.style, scrRoomGen shopType) */
enum shoptype { SHOP_GENERAL, SHOP_BOMB, SHOP_WEAPON, SHOP_RARE, SHOP_CLOTHING, SHOP_CRAPS, SHOP_KISSING };

struct inst {
    int32_t id;
    int16_t obj;
    int16_t spr;            /* sprite_index (GSPR_*, -1 none) */
    int16_t x, y;
    int16_t depth;
    uint8_t alive;
    uint8_t flags;          /* IF_* */
    int8_t status;
    int8_t facing;
    uint8_t style;          /* enum shoptype */
    uint8_t ingrid;         /* on the grid (1) or the irregular list (2) */
    int16_t counter;
    int16_t xvel, yvel;     /* xVel / yVel in 1/256 */
    int32_t cost;
    int32_t value;
    int16_t alarm[ALARMS];  /* -1 off */
};

struct world {
    struct inst in[INST_MAX];
    int16_t n;              /* instances created (alive or not), index = creation order */
    int32_t next_id;        /* id of the next instance_create */
    /* terrain grid: per cell, a list of the alive oSolid-family instances whose bbox is exactly that cell; the
       other oSolid-family instances are on the irregular list. A point test against oSolid (or a child) is a
       cell lookup plus that list and gives the instance test's answer (inst_selftest checks it) */
    int16_t cell[GRID_H][GRID_W];
    int16_t cnext[INST_MAX];
    int16_t irr[INST_MAX];
    int16_t nirr;
};

extern struct world W;

void inst_reset(int32_t next_id);
/* a new instance without running any event (room instances use inst_add with their room id) */
int inst_add(int obj, int x, int y, int32_t id);
void inst_set_sprite(int i, int spr);
void inst_destroyed(int i);    /* mark destroyed (the caller runs the Destroy event first) */

static inline int inst_is(int i, int obj) { return W.in[i].alive && obj_is(W.in[i].obj, obj); }

/* bounding box, half-open [l, r) x [t, b); returns 0 if the instance has no sprite (no collisions) */
int inst_bbox(int i, int32_t *l, int32_t *t, int32_t *r, int32_t *b);

/* collision_point(px, py, obj, prec = 0, notme = 0): the newest matching instance or INST_NONE */
int collision_point(int px, int py, int obj);
/* collision_rectangle(x1, y1, x2, y2, obj, prec = 0, notme): notme excludes self */
int collision_rectangle(int x1, int y1, int x2, int y2, int obj, int self, int notme);
/* instance_place(px, py, obj) for instance self (its bbox moved to px, py; self excluded) */
int instance_place(int self, int px, int py, int obj);
/* distance_to_object(obj) from self, squared (exact in integers); -1 if there is no instance */
int32_t distance2_to_object(int self, int obj);
int instance_exists(int obj);
int instance_first(int obj);           /* obj.var: the instance GameMaker reads (newest first) */

/* iteration as `with (obj)`: snapshot the count, then for (k = n0 - 1; k >= 0; k--) if (inst_is(k, obj)) */
#define WITH_BEGIN(var, obj) { int with_n0_ = W.n, var; \
    for (var = with_n0_ - 1; var >= 0; var--) { if (!inst_is(var, (obj))) continue;
#define WITH_END } }

int inst_selftest(void);       /* grid answers == instance answers for every cell; 0 = ok */

#endif
