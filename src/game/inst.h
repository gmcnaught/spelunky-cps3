/* Instance model for the level generator: GameMaker instances in creation order, with GameMaker's ids, object,
 * position, sprite and the per-object variables the generator and play need.
 *
 * GameMaker semantics modelled (evidence: tools/tracer.py generator dumps of the HD 1.2.2 runner, the runtime's
 * bytecode (UndertaleModTool disassembly) and the GameMaker HTML5 runner's collision code):
 *   - ids: room instances keep the ids stored in the room (rLevel: INST_RLEVEL_IDS); every instance_create takes
 *     the next id of one global counter (+1 each).
 *   - with (obj) visits the instances (children of obj included) that existed when it started, newest first,
 *     except exactly two instances: oldest first; destroyed ones are skipped. Instance searches (collision_*,
 *     instance_place, instance_nearest ties, obj.var) take the oldest match.
 *   - room start: all of the room's instances exist before the first Create event runs (rooms in gen.c).
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

#define INST_MAX 1536            /* the largest level seen: 1,404 instances (P2 references; 8,091 generator cases) */
#define INST_NONE (-1)

/* flags */
#define IF_INVINCIBLE  0x01
#define IF_SHOPWALL    0x02
#define IF_CLEANDEATH  0x04
#define IF_FORSALE     0x08
#define IF_INDICEHOUSE 0x10
#define IF_SPURT       0x20
#define IF_HELD        0x40
#define IF_SWIMMING    0x80
#define IF_NEW         0x100

/* oSolid.treasure / oXocBlock.treasure */
enum treasure { TR_NONE, TR_BIGRUBY, TR_DIAMOND, TR_SAPPHIRE, TR_EMERALD, TR_RUBY };
/* oExit-family type (oEntrance / oExit / oXMarket: "Exit", "Moai Exit", "Market Exit") */
enum exittype { EX_EXIT, EX_MOAI, EX_MARKET };

#define ALARMS 12

/* terrain grid: 16-px cells over x 0..767, y 0..639 */
#define GRID_W 48
#define GRID_H 40

/* shop styles (oShopkeeper.style, scrRoomGen shopType) */
enum shoptype { SHOP_GENERAL, SHOP_BOMB, SHOP_WEAPON, SHOP_RARE, SHOP_CLOTHING, SHOP_CRAPS, SHOP_KISSING,
                SHOP_ANKH, SHOP_EMPTY };

struct inst {
    int32_t id;
    int16_t obj;
    int16_t spr;            /* sprite_index (GSPR_*, -1 none) */
    int16_t x, y;
    int32_t depth;
    uint8_t alive;
    uint16_t flags;         /* IF_* */
    int8_t status;
    int8_t facing;
    uint8_t style;          /* enum shoptype */
    uint8_t ingrid;         /* on the grid (1) or the irregular list (2) */
    uint8_t treasure;       /* enum treasure */
    uint8_t etype;          /* enum exittype */
    int8_t linkval;         /* oChain.linkVal */
    int8_t shifttoggle;     /* oUFO.shiftToggle */
    int16_t counter;
    int16_t dir;            /* dir (oPiranha, oDeadFish, oJaws, oSmashTrap) */
    int16_t spurttime;      /* oLava.spurtTime */
    int16_t deathtimer;     /* oTrapBlock.deathTimer */
    int16_t xvel, yvel;     /* xVel / yVel in 1/256 */
    int32_t cost;
    int32_t value;
    int16_t alarm[ALARMS];  /* -1 off */
};

struct world {
    struct inst *in;        /* INST_MAX of them (inst_mem) */
    int16_t n;              /* instances created (alive or not), index = creation order */
    int32_t next_id;        /* id of the next instance_create */
    /* point-test grids (inst.c collision_point): cell, per cell the alive oSolid-family instances whose bbox is
       exactly that cell; lcell, per cell the other instances with a bbox by the cell of its top-left corner
       (clamped), their extent at most lext_x / lext_y cells; irr, the ones with a box over 64 px. Each instance
       is on one of them (ingrid 1, 3, 2); links in cnext. inst_selftest checks the answers against a scan */
    int16_t cell[GRID_H][GRID_W];
    int16_t lcell[GRID_H][GRID_W];
    int16_t cnext[INST_MAX];
    int16_t irr[INST_MAX];
    int16_t nirr;
    int16_t lext_x, lext_y;
};

extern struct world W;
extern struct inst inst_mem[];  /* W.in's memory (pworld.c; test/host/genhost.c), also PW.in (play.h) */

void inst_reset(int32_t next_id);
/* the play loop's collision tree follows the generator (src/game/pcol.c pcol_gen_hook; NULL in genhost):
   IH_RESET (a = first id), IH_CREATE, IH_SPRITE (before; a = new sprite), IH_MOVE, IH_DESTROY, and the searches
   that compute bounding boxes or use the tree: IH_POINT (i = hit, a = obj), IH_RECT (i = hit, a = obj),
   IH_PLACE (i = self, a = obj, b = hit, c = dx + 2048 + 4096 * (dy + 2048)), IH_DIST (i = self, a = obj) */
enum { IH_RESET, IH_CREATE, IH_SPRITE, IH_MOVE, IH_DESTROY, IH_POINT, IH_RECT, IH_PLACE, IH_DIST };
extern void (*inst_hook)(int op, int i, int a, int b, int c);
/* a new instance without running any event (room instances use inst_add with their room id) */
int inst_add(int obj, int x, int y, int32_t id);
void inst_set_sprite(int i, int spr);
void inst_destroyed(int i);    /* mark destroyed (the caller runs the Destroy event first) */

static inline int inst_is(int i, int obj) { return W.in[i].alive && obj_is(W.in[i].obj, obj); }

/* bounding box, half-open [l, r) x [t, b); returns 0 if the instance has no sprite (no collisions) */
int inst_bbox(int i, int32_t *l, int32_t *t, int32_t *r, int32_t *b);
/* the objects that are obj or its descendants: obj_desc[obj_desc0[obj] .. obj_desc0[obj + 1]) (obj_desc_init) */
extern int16_t obj_desc0[], obj_desc[];
void obj_desc_init(void);
void inst_moved(int i);                  /* x / y of instance i written: the point-test grids follow */

/* instance searches (collision_point, collision_rectangle, instance_place, instance_nearest ties, instance_find(obj,
   0) = obj.var) return the oldest matching instance (Observed: build/p2/probe, overlapping instances) */
/* collision_point(px, py, obj, prec = 0, notme = 0): the matching instance or INST_NONE */
int collision_point(int px, int py, int obj);
/* collision_rectangle(x1, y1, x2, y2, obj, prec = 0, notme): notme excludes self */
int collision_rectangle(int x1, int y1, int x2, int y2, int obj, int self, int notme);
/* instance_place(px, py, obj) for instance self (its bbox moved to px, py; self excluded) */
int instance_place(int self, int px, int py, int obj);
/* distance_to_object(obj) from self, squared (exact in integers); -1 if there is no instance */
int32_t distance2_to_object(int self, int obj);
int instance_exists(int obj);
int instance_first(int obj);           /* obj.var / instance_find(obj, 0): the oldest instance */
int instance_number(int obj);
/* instance_nearest(px, py, obj): smallest point distance to (x, y); ties: the first in search order */
int instance_nearest(int px, int py, int obj);
/* set when the generator reaches GML it does not translate (the level is then not comparable) */
extern int gen_untranslated;
#define UNTRANSLATED(code) (gen_untranslated = (code))

/* `with (obj)`: the instances that match when it starts (instances created in the loop are not visited,
   destroyed ones are skipped), newest first, except that exactly two instances are visited oldest first
   (Observed in the runner: build/p2/probe, n = 1..9 instances, with destroys). with_collect pushes the visit
   order on with_pool and returns the count; WITH_END pops it. */
#define WITH_POOL 4096
extern int16_t with_pool[WITH_POOL];
extern int with_top;
int with_collect(int obj);
#define WITH_BEGIN(var, obj) { int with_b_ = with_top, with_n_ = with_collect(obj), with_k_, var; \
    for (with_k_ = 0; with_k_ < with_n_; with_k_++) { var = with_pool[with_b_ + with_k_]; \
        if (!W.in[var].alive) continue;
#define WITH_END } with_top = with_b_; }

int inst_selftest(void);       /* grid answers == instance answers for every cell; 0 = ok */

#endif
