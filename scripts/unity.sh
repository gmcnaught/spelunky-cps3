#!/bin/sh
# The SH-2 builds' unity translation unit (docs/PERF3.md 3.1): in a src/game snapshot directory, the hot play files are
# included by one file, punity.c, and renamed .inc, so that the build's build/g/*.c compiles them as one TU (cross-file
# inlining and IPA register allocation; the toolchain has no LTO). Order: pobj.c before pcol.c, so that a PCOL_EXACT
# put at the top of pcol.c (EXACT=1) reaches only pcol.c, as in the separate build. The host builds keep the files
# separate; the playsh2 checksums compare the two.
#   scripts/unity.sh <snapshot dir>        (UNITY=0: nothing; UNITY_FILES: the files, in order)
# jtcost (2026-10-04, perf3-b2): these 7 files -3.8 % / -4.2 % modelled jtcps3 on p4_exit559 301 / p5_snakes 956;
# 13 files (+ pmath rng pitem pcontent ptrans pmsg) less (more fetch misses)
set -e
G=$1
[ "${UNITY:-1}" = 1 ] || exit 0
: > "$G/punity.c"
for f in ${UNITY_FILES:-pworld.c pscript.c prun.c pobj.c pcol.c pplayer.c penemy.c}; do
  mv "$G/$f" "$G/${f%.c}.inc"; echo "#include \"${f%.c}.inc\"" >> "$G/punity.c"
done
