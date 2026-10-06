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

## Rules (counts on main e4a7b08)

| Rule | Sites | Optimization it repeats | Argument |
|---|---|---|---|
| trig-pair (warning) | 2 | psincos_cr for cos and sin of one angle (91c9ed9) | psincos_cr gives psin_cr's and pcos_cr's bits (tests/sincos). The two left are hawkman_sight (a sight's creation) and pen_sight_speed (cached) |
| support-memo | 1 | oTree's `eview && !CP(x, y + 16, oSolid) -> destroy` memo (f27ea0f, 0eb9409) | the answer holds while x, y, sprite and the solid grid's cell clock do (pw_rest_still). Site: oGrave (pk_swamp.c) |
| distance-compare | 8 | not done yet: point_distance_d compared with a constant | sqrt rounds monotonically, so sqrt_rn(d2) < c exactly when d2 < T(c), T(c) the least double whose rounded root is >= c (computed once per constant). Saves psqrt (about 1.6 K jtcps3 clocks a call). Several sites are PLAY_STATS checks; distances kept in a variable (piranha, jaws) need a by-hand look |
| cp-at | 90 | collision_point_any_at's integer path at whole x, y (PERF3 batches 21-22) | the corners are whole doubles the query takes as ints (dwhole); the doubles path stays for fractional positions |
| float-compare | 49 | PLTI / PGTI on the float's bits (PERF3 batch 7) | all 2^32 floats checked there against the double compare |

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
