/* GameMaker 2024.14's collision tree and collision events (pcol.c): the hooks the play loop and the generator
 * call. Entries: play instance i is entry i; generator instance w is entry w while the level is generated (inst.c
 * calls inst_hook), renamed to its play index when the level is loaded (pw_reset). */
#ifndef PCOL_H
#define PCOL_H
#include <stdint.h>

/* pworld.c */
void pcol_after_reset(void);           /* pw_reset: a new room (StartRoom's RebuildTree(true)), or the level just
                                          generated (its RemoveMarked after the Create events, then renamed) */
void pcol_added(int i);                /* pin_add: a new instance (no tree action) */
void pcol_create(int i);               /* instance_create: CRoom::AddInstance + CollisionInsert, before Create */
void pcol_room_inst(int i);            /* a room instance at room start (StartRoom: CollisionMarkDirty) */
void pcol_changed(int i);              /* a setter changed a collision field (pworld.c pw_changed) */
void pcol_mark(int i);                 /* a change of position / sprite / mask / scale / angle (MarkDirty) */
void pcol_destroyed(int i);            /* instance_destroy: removed from the tree at the next RemoveMarked */
void pcol_touch(int i);                /* the runner computes i's bounding box (Compute_BoundingBox(true)) */
void pcol_load_done(void);              /* play_level_start loaded the level: no instance is quiet now */
int pcol_quiet(void);
void pcol_box(int i, float *o);
void pcol_sincosf(float a, float *s, float *c);  /* sinf, cosf as glibc's sincosf (the runner's) */         /* play instance i's bounding box l, t, r, b (floats; image_angle included) */                  /* some level-load instance is not yet looked at (pcol_touch_stale inexact) */
void pcol_touch_stale(int obj, int notme, int upto);  /* the touches of a creation-order scan up to `upto` */
/* ShouldUseFastCollision(obj) then, when it gives 1, UpdateTree: -1 no instance (no search at all), 1 search the
   tree (pcol_search), 2 test the object's instances in creation order (touching each, pcol_touch) */
int pcol_query(int obj);
/* RTree::Search over [l, r] x [t, b] (inclusive): cb(entry, ctx) for each leaf entry in the tree's order, until
   it returns 0. The tree is locked meanwhile (no CollisionUpdate) */
void pcol_search(float l, float t, float r, float b, int (*cb)(int e, void *ctx), void *ctx);
/* the same over whole-number sides (|v| < 2^24): the search rectangle (float)l .. (float)b */
void pcol_search_i(int32_t l, int32_t t, int32_t r, int32_t b, int (*cb)(int e, void *ctx), void *ctx);
void pcol_place_marks(int self);       /* instance_place / place_meeting moved self there and back (SetPosition) */
void pcol_touch_at(int self, double dx, double dy);   /* Compute_BoundingBox(true) of self moved by dx, dy */

/* prun.c */
void pcol_remove_marked(void);         /* CRoom::RemoveMarked (DoAStep_Draw, before the drawing) */
void pcol_handle(void);                /* HandleCollision: the collision events */
void pcol_event_done(int i);           /* after an event of instance i: its own changes are marked now (dispatch
                                          order; the C code writes x, y, ... directly, pcol.c compares) */

/* playhost --tree-probe: what tools/tracer.py TRACE_TREE writes (collision_rectangle_list over the room) */
int pcol_probe(int obj, int32_t *ids, int max);
/* statistics for the cost report */
struct pcol_stats { uint32_t inserts, removes, searches, visits, syncs, nodes_max, flushes, pairs_max; };
extern struct pcol_stats pcol_st;

/* generator hook (inst.h: inst_hook) */
void pcol_gen_hook(int op, int w, int a, int b, int c);

#endif
