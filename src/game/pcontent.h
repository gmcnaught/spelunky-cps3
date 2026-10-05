/* P7 content packages (docs/CONTENT.md §2): A jungle, B swamp, C ice, D temple / Olmec, E items. Each package
 * implements its functions in its own file (pk_jungle.c, pk_swamp.c, pk_ice.c, pk_temple.c, pk_items.c); the weak
 * defaults in pcontent.c do what the code did before (the PUNTR of the site, or nothing), so a package overrides
 * exactly the functions it translates.
 *
 * pX_ev(ev, i, arg): an event of instance i that the generic dispatch of pobj.c / penemy.c does not translate
 *   (ev: FEV_* of src/front/front.h; arg: the alarm index for FEV_ALARM, the other instance for FEV_COLLISION, else
 *   0). Returns 1 when package X ran it (the dispatch then stops), 0 to leave it to the next package / the PUNTR.
 *   Asked in order A, B, C, D, E (pcontent_ev) at: Create (PUNTR 1000), Destroy (1021), Step (1061; enemies 5003),
 *   End Step (1063), Alarm (1073), Animation End (1080; enemies 5004), Collision (1096; enemies 5005), Outside
 *   (1097), Draw (before ptrans_draw; no PUNTR).
 * pX_player / pX_world(site, i, arg): one call per former PUNTR(site) in the player's code (pplayer.c, pscript.c)
 *   or the items / world code (pobj.c, pitem.c, penemy.c, ptrans.c, pdamsel.c), at the same place and under the
 *   same condition; `site` is that PUNTR code (grep "(site," to find the call). i: the instance in scope (PL.idx in
 *   scrUseItem / scrFireBow / scrStealItem); arg: a second instance or value where the site has one (obj, other,
 *   the alarm, skull), else 0. The default does PUNTR(site) and returns 0; the callers ignore the result for now
 *   (a package that needs the caller to stop there changes the call site in the same commit). */
#ifndef PCONTENT_H
#define PCONTENT_H
#include "front.h"                                   /* FEV_* */

int pjungle_ev(int ev, int i, int arg);
int pjungle_idle(int i);                         /* prun.c: the Step of oTree / oTreeBranch out of view */
int pswamp_ev(int ev, int i, int arg);
int pice_ev(int ev, int i, int arg);
int ptemple_ev(int ev, int i, int arg);
int pitems_ev(int ev, int i, int arg);
int pcontent_ev(int ev, int i, int arg);             /* the five in order: 1 when one ran the event */

/* pX_msolid(s): the oMovingSolid instance s in gameStepEvent's loop (penemy.c pen_moving_solids, which moves every
   oMovingSolid the same way) is package X's: its viscidTop (0 / 1); -1 when it is not X's. pcontent_msolid asks the
   five in order; -1 from all: the caller sets PUNTR(5050) */
int pjungle_msolid(int s);
int pswamp_msolid(int s);
int pice_msolid(int s);
int ptemple_msolid(int s);
int pitems_msolid(int s);
int pcontent_msolid(int s);

int pjungle_player(int site, int i, int arg);        /* 2010 / 2016 monkey on the player; 2020 spears */
int pjungle_world(int site, int i, int arg);         /* 1011 tiki torch on a destroyed block; 1017 snake from a jar */
int pswamp_player(int site, int i, int arg);         /* 2009 / 2032 / 2036 swimming; 2002 / 2003 / 2007 / 2011 / 2030 cape physics */
int pswamp_world(int site, int i, int arg);          /* 1041 / 8001 item in water; 1053 checkWater; 1012 grave; 1052 ghost */
int pice_player(int site, int i, int arg);           /* 2018 laser; 2019 psychic wave */
int ptemple_player(int site, int i, int arg);        /* 2021 smash trap; 2022 ceiling trap; 2033 lava; 2046 idol trap; 2061 exit past Olmec */
int ptemple_world(int site, int i, int arg);         /* 1032 / 1036 / 1039 / 1040 / 1056 lava; 1094 sceptre / gold door; 5012 enemies in lava */
int pitems_player(int site, int i, int arg);         /* jetpack, cape glide, parachute, mitt, weapons, flare crate, ankh, kapala, ball, downToRun; 3001 bow; 3002 equipment */
int pitems_world(int site, int i, int arg);
/* pX_enemy(site, e, arg): the enemy sites of penemy.c (5006 piranha / vampire blood on hurting the player, arg the
   character; 5010 a thrown item / stunned enemy hits e, arg kind 0 item / 1 enemy; 5011 fire frog in water;
   5013 oSpearsLeft; 5016 an arrow hits a vampire, arg the arrow; 5017 an explosion hits a magma man, arg the
   explosion: pobj.c oExplosion Collision_oEnemy :1-11). Asked in order A .. E (pcontent_enemy): 1 when
   one ran it; the defaults return 0 and pcontent_enemy does PUNTR(site) */
int pjungle_enemy(int site, int e, int arg);
int pswamp_enemy(int site, int e, int arg);
int pice_enemy(int site, int e, int arg);
int ptemple_enemy(int site, int e, int arg);
int pitems_enemy(int site, int e, int arg);
int pcontent_enemy(int site, int e, int arg);          /* bones, jars, sticky bombs, shop bombs / idol, udjat, dice, flare / chest / mattock / web cannon, bomb arrows, locked chest, Kali */
#endif
