/* The front end (PLAN P8): HD 1.2.2's intro, title and high-scores rooms as the cabinet's attract mode, run by the
 * play loop (src/game) with the room-specific objects translated here.
 *
 * The hook API (src/game implements the calls marked [game], src/front the rest):
 *
 *   front_on                 [front] nonzero while a front room runs: the play loop consults the hooks below only
 *                            then (no cost in a level)
 *   front_ev(ev, i, arg)     [front] an event of instance i that src/game does not translate (ev: FEV_*; arg: the
 *                            alarm number or the other instance of a collision). Returns 1 if src/front ran it.
 *                            [game] each ev_* entry point calls it first, while front_on:
 *                                if (front_on && front_ev(FEV_STEP, i, 0)) return;
 *   front_room(room)         [front] enter a front room (rIntro, rTitle, rHighscores: its instances, their Create
 *                            events, PW set up as play_transition_start does). Returns 1 if it is one.
 *                            [game] room_change() calls it for a room it does not model, while front_on.
 *   front_view_obj, front_hborder
 *                            [front] the view's target object (-1: none, the room's code moves the view) and its
 *                            horizontal border; [game] view_update() follows front_view_obj instead of oPlayer1
 *                            while front_on (the vertical border is PW.vborder as in the levels).
 *
 * The src/game side: pobj.c (the ev_* entry points, the weak defaults for builds without src/front), prun.c.
 *
 * Everything an attract room does to the gameplay state is listed in front.c's header (the RNG, globals). */
#ifndef FRONT_H
#define FRONT_H
#include <stdint.h>

enum { FEV_CREATE, FEV_DESTROY, FEV_STEP, FEV_END_STEP, FEV_ALARM, FEV_ANIMEND, FEV_DRAW, FEV_OUTSIDE,
       FEV_COLLISION };

extern uint8_t front_on;
extern int16_t front_view_obj, front_hborder;
int front_ev(int ev, int i, int arg);
int front_room(int room);

/* ---- the attract mode (src/main's game_attract_step / game_draw) ---- */
void front_start(void);                 /* (re)start the attract cycle: the intro */
void front_start_at(int room);          /* tests: start the cycle in that room (rHighscores: as TRACE_ROOM) */
void front_step(void);                  /* one attract step (no controls) */
extern void (*front_rec_cb)(int phase);  /* play_step's record callback for the attract steps (tests: records) */
void front_stop(void);                  /* a game begins (shell credit / start) */
/* drawing (src/draw): the black rectangle the room's code draws (oIntro's fade): its instance and alpha byte, -1
   for none; the Draw-event text of instance i (src/draw calls it at i's place in the depth order) */
int front_fade(int *a8);
/* the draw order key of instance i among equal depths (larger first): a front room draws a layer's run-time
   instances first, newest first, then its room instances in their layer order (src/draw uses the id elsewhere) */
int32_t front_drawkey(int i);
void front_draw(int i, int ox, int oy);
void front_draw_gui(void);              /* the attract rooms' Draw GUI (oHighscores' box), over everything */
#endif
