/* Collision events: GameMaker 2024.14's HandleCollision (libyoyo.so, x86_64 build of the same runtime, symbols
 * HandleCollision, collisionResult, processCollision, CollisionMarkTest):
 *   1. for each instance of the "test list" (instances marked since the last pass), search the collision tree
 *      with its bounding box; every other instance found that has not been a searcher yet in this pass forms a
 *      pair (searcher, found) when either object has a collision event with the other's object;
 *   2. then, pair by pair in that order: if both still exist and their masks overlap (Collision_Instance), the
 *      found instance is marked for the next pass (when its object has collision events) and the events run:
 *      searcher's event with the found one's object, then the found one's with the searcher's.
 * This file keeps that pair rule; the order of the test list and of the tree search is the runner's RTree's
 * (not modelled yet: the searchers go in creation order and the found instances in creation order).
 */
#include "pint.h"

static int has_col(int a, int b)
{
    const struct pobj *o = &pobj[PX(a).obj];
    int k;
    for (k = 0; k < o->ncol; k++)
        if (obj_is(PX(b).obj, pcol[o->col0 + k]))
            return 1;
    return 0;
}

#define PAIRS_MAX 4096
static int16_t pa[PAIRS_MAX], pb[PAIRS_MAX];
static uint8_t searched[PIN_MAX];

void pcol_handle(void)
{
    int n = 0, k, j;
    for (k = 0; k < PW.n; k++) searched[k] = 0;
    for (k = 0; k < PW.n; k++) {
        double l, t, r, b;
        if (!PX(k).alive || !pobj[PX(k).obj].ncol || !pin_bbox(k, &l, &t, &r, &b))
            continue;
        for (j = 0; j < PW.n; j++) {
            double l2, t2, r2, b2;
            if (j == k || !PX(j).alive || searched[j]) continue;
            if (!has_col(k, j) && !has_col(j, k)) continue;
            if (!pin_bbox(j, &l2, &t2, &r2, &b2)) continue;
            if (!(l < r2 && l2 < r && t < b2 && t2 < b)) continue;
            if (n < PAIRS_MAX) {
                pa[n] = (int16_t)k;
                pb[n] = (int16_t)j;
                n++;
            }
        }
        searched[k] = 1;
    }
    for (k = 0; k < n; k++) {
        int a = pa[k], b = pb[k];
        if (!PX(a).alive || !PX(b).alive || !pin_overlap(a, b))
            continue;
        if (has_col(a, b)) ev_collision(a, b);                           /* Perform_Event(a, b) */
        if (has_col(b, a)) ev_collision(b, a);                           /* Perform_Event(b, a), unchecked */
    }
}
