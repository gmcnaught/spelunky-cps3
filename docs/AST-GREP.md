# AST-GREP: finding more sites for optimizations already proven here

Each rule in tools/sg/rules/ matches the source shape of an optimization this repo has already made and proved
exact, so the same change (and the same argument) can be applied at the sites it finds. ast-grep 0.44
(`brew install ast-grep`).

    ast-grep scan                      # every rule over src/ (sgconfig.yml), with the sites
    ast-grep scan --filter cp-at       # one rule
    tools/sgdup.py                     # repeated queries in one function (see below)
    tools/sgfix_at.py <rule> [--apply] # cp-at / rect-at / place-at and their -p forms: list or apply the rewrites
    scripts/fcol_survey.sh             # the double collision queries by call site at run time (below)

A match is a candidate, not a finding: each change still needs its exactness argument in the commit message and
the usual gates (PERF3 section 2; the host route comparison: every route through playhost and playhost_grid,
byte-identical output).

## Rules (counts after branch floatcol: matches left in src/)

| Rule | Sites | Optimization it repeats | Argument |
|---|---|---|---|
| trig-pair (warning) | 2 | psincos_cr for cos and sin of one angle (91c9ed9) | psincos_cr gives psin_cr's and pcos_cr's bits (tests/sincos). The two left are hawkman_sight (a sight's creation) and pen_sight_speed (cached) |
| support-memo | 0 | oTree's `eview && !CP(x, y + 16, oSolid) -> destroy` memo (f27ea0f, 0eb9409) | the answer holds while x, y, sprite and the solid grid's cell clock do (pw_rest_still). oGrave done on sgapply |
| distance-compare | 3 | pdist2_lt: point_distance against a constant as the squared distance against a threshold (sgapply) | psqrt is correctly rounded (non-decreasing) and DLT is monotone in it, so the compare is d2 < T(c); T by bisection with psqrt, cached, warmed at the first level start. The 3 left are the spear trap's PLAY_STATS check (the reference there). Stored distances (`dist = point_distance_d(...); ... DLT(dist, c)`) are not matched: grep for them |
| cp-at | 0 | collision_point_any_at for every object at X(i) / PTOD(PX(i).x) plus int offsets (PERF3 batches 21-22: static-index objects, 27 sites on sgapply; branch floatcol: every object, 97 sites, 5d054a0) | at a whole x, y (\|.\| < 29900) the point is whole (\|dx\| < 100) and the _at form makes pq_init's query of it on ints (iok, the ints, their values); at a fractional one (pfrac_ok: +-0 or 1/2 <= \|x\| < 2^13; ef8fa7a) of the float sums x + (float)dx and their floors; otherwise collision_point_any on the same doubles. CP / CPn / collision_point_p != NOONE are collision_point_any. The 3 with cos / sin offsets are not matched (the constraint lists int forms) |
| cp-at-p | 8 | the same at PTOD(p->x) with p = &PX(i) (floatcol db5c623: 31 sites with rect-at-p / place-at-p; d4230bf: 5 more in pobj.c) | tools/sgfix_at.py resolves p from the enclosing function's `struct pin *p = &PX(i);` (p assigned nothing but &PX(i) again, i not assigned). Left: scrUseItem's 6 (p of a local index), pshop.c's b, ptrans.c (the transition room: another agent's) |
| rect-at | 0 | collision_rect_at (instance) / collision_rect_any_at (!= noone) at X(i) plus int offsets, prec 0, noone (floatcol 0d4f720, 14 sites) | collision_rect_p's steps on the ints at a whole near x, y: pcol_query, the far test (rect_far_none_i: dfloor14 of a whole corner is itself), rq_init's whole path, the search; at a fractional x, y (pfrac_ok, l <= r, t <= b) rq_init's float path from the float sums and pfr's roundings (rect_at_frac); oSolid any: rect_any_i (collision_rect_any's whole-corner path) |
| rect-at-p | 0 | the same at PTOD(p->x) (floatcol db5c623) | as cp-at-p |
| place-at | 0 | instance_place_at at the instance's own X(i) plus offsets \|d\| <= 16 (floatcol 02aa7e4, 15 sites) | instance_place_ixy at a whole near x, y (its contract), else instance_place_p on the same doubles |
| place-at-p | 1 | the same at PTOD(p->x) (floatcol db5c623) | as cp-at-p; the one left queries for another instance (pobj.c:1201, self j) |
| (AT_XY, by hand) | 28 | pplayer.c's queries on double copies x, y of the player's position (floatcol 141debc) | play.h AT_XY: the _at form while the instance's x, y have the bits of x0, y0 kept beside the copies, else the query as written (a hook can move the player between the read and the query: pjungle_player 2020) |
| float-compare | 49 | PLTI / PGTI on the float's bits (PERF3 batch 7) | **Measured and not kept (2026-10-06, branch floatcmp e81ba25):** gcmp_ffk (pfcmp.h: instance against instance plus an int, exact on the bits, tests/fcmp 28 G cases equal) at 43 sites changed playsh2's SOFTFP step-weighted mean by -0.024 % (no route over 0.2 %) and jtcost's lush / swamp steps by +0.5 to +0.9 % (layout): these compares run a few times a step. Only one site compares with an int (pk_ice.c DGT(Y(i), 576)). Revisit a site only when jtcost shows it hot |

tools/sgdup.py: the same collision / distance / nearest query, same arguments, two or more times in one function
(123 groups, 297 calls). Only a candidate where nothing the query reads changes between the calls; many repeats are
in exclusive branches or come after a move. PERF3 measured a blanket per-step memo of isCollision* as not worth it,
so take single hot sites, chosen with jtcost's JTC_BYOBJ tables (docs/LUSH.md section 1).

## The double collision queries at run time (scripts/fcol_survey.sh)

Rank sites by calls, not by count in the source. test/host playhost_fcol (play.h FCOL_STATS, the grid build without
PLAY_STATS, whose checks call the double queries) counts every call of collision_point_p / _any / line / rect /
rect_any, instance_place_p, CP / CPn that reaches the function, by call site, with the calls whose coordinates were
not all whole, plus penemy.c isCollisionRectangle and overlap_at's float path (by the counted query above it;
"overlap_at(pass)": the collision pass). The script runs it on every hostident route (P=3) and prints the sites by
calls on the seven routes still dropping frames (HOT=...). On 8b3d544: 574,488 calls in the 92 runs, 33,046 with a
coordinate not whole; on the seven routes 5-7.6 K a route (about 19 a step), almost all whole: the cost there was
the double sums and conversions at the call site (PTOD, __adddf3, __floatsidf: about 0.4-0.8 K modelled jtcps3 a
point query, 1-1.5 K a rectangle) and pq_init / rq_init's decode, not the searches' float paths. The float paths
that do run: players and items at fractional positions (knock-back, falling treasure), the debris' fallback (pw_piece_tests), and
overlap_at's precise float test (vampkill's zombies, olmec, the UFOs: 35 K modelled a call). Confirm with jtcost's
JTC_CALLERS before converting a site.
After branch floatcol (ef8fa7a): 125,692 calls, 10,988 not whole, 95 sites (the generator, scripts' bounds,
pplayer.c's rare tests on copies, the _at forms' fallbacks at |x| < 1/2 or far positions).

## Writing rules for this C code

- A bare call pattern such as `pcos_cr($A)` parses as a declaration and matches nothing. Give it context and a
  selector (`pattern: { context: "x = pcos_cr($A);", selector: call_expression }`), or match by kind and the
  callee's name (`kind: call_expression` + `has: { field: function, regex: ... }`, as tools/sg/util/). Patterns with
  operators in the arguments (`CP(X($I) + $DX, ...)`) parse as expressions and work as written.
- A metavariable used twice must match the same text: trig-pair's `$A` in pcos_cr and psin_cr inside one function.
- `files: ["src/**"]` in each rule keeps scans off build/ and refs/.
- Rules for scripts go in tools/sg/util/ (not in sgconfig.yml's ruleDirs, so `ast-grep scan` does not report them).
