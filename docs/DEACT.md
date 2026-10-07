# Off-view deactivation (branch deact2, 2026-10-06)

User decision (2026-10-06): instances outside the view plus a margin are deactivated to cut step cost, and the HD
reference build applies the same rule, so the exact gates compare like with like. A gameplay deviation from stock
HD 1.2.2 is accepted.

## 1. The rule

**Where.** One pass, at oGamepad's Begin Step (HD: tools/tracer.py `TRACE_DEACT=<margin>`, gml_GlobalScript_trcDeact
`trcDeactPass`; port: play_step right after the Begin Step block, `deact_pass`). oGamepad's Begin Step runs before
every other instance's Begin Step except oScreen's, and no other gameplay object has a Begin Step, so the pass sits
after the animation and before the alarms, Step, collision and End Step events of the same frame.

**When.** Every step of rLevel, rLevel2 and rLevel3, except the room's first step (the Begin Step that writes the
phase-0 record: the view is not placed yet). Not in rOlmec, the transition rooms or the front end.

**Region.** The view the last draw left (camera_get_view_x / _y / _width / _height of view_camera[0]; port: PW.xview,
PW.yview, 320 x 240), grown by the margin M on every side: x0 = vx - M, x1 = vx + 320 + M, y0 = vy - M,
y1 = vy + 240 + M. An instance is outside when `x < x0 || x > x1 || y < y0 || y > y1` on its (x, y), not its box.

**Margin: M = 32.** Every candidate's sprite has its origin at (0, 0) and reaches at most 32 px right and down
(refs/hd/src/sprites; oJaws, 64 px, is exempt), so a deactivated instance is never visible. 32 is also at least
every view margin the candidates' own Steps use (oItem / oTreasure 16, oEnemy and its children 20 / 4, oRubblePiece
32): whatever HD runs for an instance inside its own view test still runs. 64 would only add instances that HD
already treats as off-view.

**Candidates.** Instances of oEnemy, oItem, oTreasure and their descendants (83 objects), except:
- objects: oShopkeeper, oShopkeeper2 (shop logic, the chase), oBomb (the fuse), oRopeThrow, oFlare, oFireFrogArmed,
  oFireFrogBomb (timers), oDamsel (exit / kiss), oDice (the dice shop counts them), oLampItem, oLampRedItem (light
  in dark levels), oJaws (64 px sprite);
- state: `held` true (the player's held item, a carried damsel or enemy), `forSale` true (the shopkeeper's
  `with oItem`).
Never candidates: oPlayer1 and the other oCharacter, the controllers (oGame, oLevel, oScreen, oGamepad, ...),
terrain and every other oSolid / oPlatform / oWater / oLadder object (so collisions of the active movers with the
level are unchanged), ropes, oGhost, oOlmec, boulders, explosions, rubble, blood and other detritus, bubbles.
No velocity test: oItem / oTreasure / oEnemy freeze themselves outside their own view test already (an item thrown
out of view stops mid-air in stock HD), so a deactivated mover keeps exactly the state HD would leave it in.

**The pass.**
1. Activation: every instance on the pass's list (the ones it deactivated, with the (x, y) they had then) whose
   (x, y) is inside the region is activated (`instance_activate_object(id)`), in list order; the rest stay listed.
2. Deactivation: every active candidate outside the region, in `with (all)` order, is deactivated
   (`instance_deactivate_object(id)`) and appended to the list with its (x, y).
The list is emptied at each room's first Begin Step.

**HD's legacy activation calls** (oLevel Step: `instance_activate_region` of the view + 96, `instance_activate_object`
of oCharacter, ropes, ...; oGame Step: `instance_activate_region` around draining water) are no-ops in stock 1.2.2,
which deactivates nothing during play (the matching `instance_deactivate_region` is commented out in oLevel). The
patch turns them into a no-op function (`trcNoActivate`), so the pass is the only activation in a level. The pause's
`instance_deactivate_all` / `instance_activate_all` stay (not on any route; the port does not model them).

## 2. What GameMaker does with a deactivated instance (Observed, HD runner, build/trace/dz_probe_cave, p5_caveman
seed 863, TRACE_DEACT=32 with TRACE_EVLOG / TRACE_TREE probes)

- No Step, alarm or animation: alarms and image_index are frozen (oRubyBig alarm[0] 19 at deactivation, 18 one step
  after activation; oSnake image_index 0.8 kept for 22 steps).
- Not in `with (all)` / `with (obj)` (the records leave it out), `instance_exists(id)` is false, collision functions
  do not find it.
- Its variables can still be read through its id (`id.x` gave 280, no error), so a reference held by an active
  instance does not stop the runner.
- Activation takes effect at once: the instance's alarms and Step run in the same frame.
- **Activation puts the instance last**, as a new creation: last in its object's Step order (oGoldBar Step order
  110976, 110977, 110991, 111003, 111004, 110998 after 110998's activation), first in `with (all)` order (the record
  order, newest first). Deactivating and activating changes the order the instance is dispatched in.
- **The collision tree**: activation re-inserts it (the tree's search order changes: 111003 111005 111004 after
  activation where they were 111003 111004 111005 before), so deactivation takes it out.

## 3. The port

- `deact_pass` in play_step (prun.c), compiled with `-DPLAY_DEACT=32` (the CPS3 game and every host / SH-2 test
  build of the option), off by default until the traces are regenerated with the patch. One option in both:
  `TRACE_DEACT=32` on the HD side, `PLAY_DEACT=32` on the port side.
- Deactivate (pworld.c `pw_deactivate`): the structural half of pin_kill without removing the slot: `alive = 0`
  (every event loop, query, `with`, count and the recorder skip it, as in GM), unlinked from the object lists
  (ounlink: olive counts, nearest caches, the grid, the animation list), out of pw_ord, the collision entry taken out
  (pcol_deactivated: tree remove, object count, dirty / test lists). The slot, pin_ext and pin_en stay.
- Activate (`pw_activate`): the structural half of pin_add on the kept record: a new creation number (pw_seq =
  PW.seq++, appended to pw_ord, the object lists and the animation list, so it is the newest), `alive = 1`, put in
  the collision tree as instance_create does (pcol_activated), marked for the drawing.
- References kept across steps (PL.holdItem, trapID, enemyID, bombID) are not cleared: the slot is not released.
- The piranha batch hook (piranha4, after `play_cur_obj = PX(i).obj;`): a deactivated piranha is unlinked, so it is
  not in the Step snapshot and breaks a batch run like a destroyed one.

## 4. Verification plan

1. Exact build (playhost, PCOL_EXACT) with PLAY_DEACT=32 against TRACE_DEACT=32 traces of a few routes
   (dz_<route> names, build/trace untouched otherwise): record-equal (playcmp RESULT n/n, ROUTE equal).
2. The collision tree after activations: PCOL_TREE probes against TRACE_TREE (oTreasure, oItem).
3. Grid build: tools/equivcheck.py route check and state gate on the same traces.
4. Cost: MAME SOFTFP and jtcost on c_swamp_drain, c_swamp_swim, p5_lush_l5s11 / l6s23, p5_caveman.
5. The full retrace (build/retrace_cmds.sh with TRACE_DEACT=32) and all gates: a lead decision.
