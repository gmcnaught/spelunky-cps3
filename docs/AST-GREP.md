# AST-GREP: finding more sites for optimizations already proven here

Each rule in tools/sg/rules/ matches the source shape of an optimization this repo has already made and proved
exact, so the same change (and the same argument) can be applied at the sites it finds. ast-grep 0.44
(`brew install ast-grep`).

    ast-grep scan                      # every rule over src/ (sgconfig.yml), with the sites
    ast-grep scan --filter cp-at       # one rule
    tools/sgdup.py                     # repeated queries in one function (see below)

A match is a candidate, not a finding: each change still needs its exactness argument in the commit message and
the usual gates (PERF3 section 2; the host route comparison: every route through playhost and playhost_grid,
byte-identical output).

## Rules (counts after branch sgapply)

| Rule | Sites | Optimization it repeats | Argument |
|---|---|---|---|
| trig-pair (warning) | 2 | psincos_cr for cos and sin of one angle (91c9ed9) | psincos_cr gives psin_cr's and pcos_cr's bits (tests/sincos). The two left are hawkman_sight (a sight's creation) and pen_sight_speed (cached) |
| support-memo | 0 | oTree's `eview && !CP(x, y + 16, oSolid) -> destroy` memo (f27ea0f, 0eb9409) | the answer holds while x, y, sprite and the solid grid's cell clock do (pw_rest_still). oGrave done on sgapply |
| distance-compare | 3 | pdist2_lt: point_distance against a constant as the squared distance against a threshold (sgapply) | psqrt is correctly rounded (non-decreasing) and DLT is monotone in it, so the compare is d2 < T(c); T by bisection with psqrt, cached, warmed at the first level start. The 3 left are the spear trap's PLAY_STATS check (the reference there). Stored distances (`dist = point_distance_d(...); ... DLT(dist, c)`) are not matched: grep for them |
| cp-at | 3 | collision_point_any_at's index path at whole x, y for the static-index objects (PERF3 batches 21-22; 27 sites converted on sgapply) | the corners are a float's value plus ints, exact in a double, as the _at form computes them. The 3 left have fractional offsets (cos, sin) |
| float-compare | 49 | PLTI / PGTI on the float's bits (PERF3 batch 7) | **Measured and not kept (2026-10-06, branch floatcmp e81ba25):** gcmp_ffk (pfcmp.h: instance against instance plus an int, exact on the bits, tests/fcmp 28 G cases equal) at 43 sites changed playsh2's SOFTFP step-weighted mean by -0.024 % (no route over 0.2 %) and jtcost's lush / swamp steps by +0.5 to +0.9 % (layout): these compares run a few times a step. Only one site compares with an int (pk_ice.c DGT(Y(i), 576)). Revisit a site only when jtcost shows it hot |

tools/sgdup.py: the same collision / distance / nearest query, same arguments, two or more times in one function
(123 groups, 297 calls). Only a candidate where nothing the query reads changes between the calls; many repeats are
in exclusive branches or come after a move. PERF3 measured a blanket per-step memo of isCollision* as not worth it,
so take single hot sites, chosen with jtcost's JTC_BYOBJ tables (docs/LUSH.md section 1).

## Writing rules for this C code

- A bare call pattern such as `pcos_cr($A)` parses as a declaration and matches nothing. Give it context and a
  selector (`pattern: { context: "x = pcos_cr($A);", selector: call_expression }`), or match by kind and the
  callee's name (`kind: call_expression` + `has: { field: function, regex: ... }`, as tools/sg/util/). Patterns with
  operators in the arguments (`CP(X($I) + $DX, ...)`) parse as expressions and work as written.
- A metavariable used twice must match the same text: trig-pair's `$A` in pcos_cr and psin_cr inside one function.
- `files: ["src/**"]` in each rule keeps scans off build/ and refs/.
- Rules for scripts go in tools/sg/util/ (not in sgconfig.yml's ruleDirs, so `ast-grep scan` does not report them).
