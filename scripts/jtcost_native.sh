#!/bin/sh
# scripts/jtcost.sh without docker (Linux, e.g. a cloud container): the SH-2 toolchain native (sh-elf-gcc in PATH or
# /opt/sh/bin, built as ../cps3-testgame/docker/Dockerfile does), MAME native (mame or /usr/games/mame; 0.264 works),
# GNU sed. Builds tests/playsh2 as scripts/playsh2_check.sh BUILD_ONLY=1 does (SOFTFP=1, the unity TU, PIN 1792), cut
# to the routes named, traces each named step in MAME with scripts/lua/jtcost.lua, and reduces each trace with
# tools/jtcost.py; the trace is kept only with JTC_KEEP=1 (as <out>/t<k>.tr: tools/jtbypass.py reads it).
#   [GAME_SRC=<dir>] [VARIANT=<name>] [MAKEARGS="..."] [JTC_KEEP=1] scripts/jtcost_native.sh [<route> <step>]...
#   CHECK=1 [CROUTES="a b"] ... scripts/jtcost_native.sh: no trace; scripts/playsh2_check.sh's run instead (every job
#   of CROUTES, default all jobs: checksums against the host build, MAME clocks per route; tests/playsh2/report.py)
# GAME_SRC: the src/game to build (default src/game, the working tree). MAKEARGS: extra make variables for
# tests/playsh2/Makefile (e.g. "NC=nc_robust.txt NCSHADOW=1", docs/ICACHE.md). BUILD_ONLY=1: stop after the build.
# OPT: the optimisation flags (default -O2). Output: tests/playsh2/build/<VARIANT>/out.
# Trace NC builds through their shadow link (NCSHADOW=1) and charge the moved code with tools/jtbypass.py --ncfile:
# MAME's debugger disassembles code at 0x26xxxxxx wrongly, and tools/jtcost.py bills uncached fetches nothing.
set -e
cd "$(dirname "$0")/.."
export PATH=/opt/sh/bin:/usr/games:$PATH
[ $# -eq 0 ] && [ "${CHECK:-0}" != 1 ] && set -- p4_exit559 301 p5_snakes 956
T=tests/playsh2; V=${VARIANT:-jtnat}; E=$T/build/$V; O=$E/out; G=$T/build/g
ROUTES=; PAIRS=; last=0
while [ $# -gt 0 ]; do
  case " $ROUTES " in *" $1 "*) ;; *) ROUTES="${ROUTES:+$ROUTES }$1" ;; esac
  [ "$2" -gt "$last" ] || { echo "jtcost_native.sh: steps must ascend" >&2; exit 2; }
  last=$2; PAIRS="$PAIRS $1:$2"; shift 2
done
rm -rf "$O" "$G"; mkdir -p "$O/w" "$G" "$E/jobs"
S=${GAME_SRC:-src/game}
cp "$S"/*.c "$S"/*.h "$G/"
cp build/gen/objects.[ch] build/gen/gentables.[ch] build/gen/playtables.[ch] "$G/"
sed -i "s/^#define PIN_MAX [0-9][0-9]*\$/#define PIN_MAX ${PIN:-1792}/" "$G/play.h"
touch "$G/stamp"
scripts/unity.sh "$G"
sed -n '/^struct pin_ext {/,/^};/p' "$G/play.h" | grep -q "alpha" && echo "#define ALPHA_IN_EXT 1" > "$G/snapcfg.h" || : > "$G/snapcfg.h"
[ "${CHECK:-0}" = 1 ] && ROUTES=${CROUTES:-}
if [ -n "$ROUTES" ]; then python3 $T/mkjobs.py "$E/jobs/jobs.h" tests/routes --routes "$(echo $ROUTES | tr ' ' ,)"
else python3 $T/mkjobs.py "$E/jobs/jobs.h" tests/routes; fi
make -s -C $T OUT=build/$V JOBSDIR=build/$V/jobs OPT="${OPT:--O2}" PROF=0 SOFTFP=${SOFTFP:-1} $MAKEARGS >"$O/make.log" 2>&1 || { cat "$O/make.log"; exit 1; }
sh-elf-nm -n $E/main.elf > "$O/nm.txt"
sh-elf-size -A $E/main.elf | awk '$1 ~ /^\.(nctext|text|data|ncbss|bss)$/ { printf "%s %s  ", $1, $2 } END { print "" }' > "$O/size.txt"
[ "${BUILD_ONLY:-0}" = 1 ] && { cat "$O/size.txt"; exit 0; }
# MAME before 0.27x has no sfiii3na: sfiii3n (the same game keys), the BIOS file under its Japan name
SET=sfiii3na; mame -listroms sfiii3na >/dev/null 2>&1 || { SET=sfiii3n; mkdir -p "$E/mame/sfiii3n"
  for f in "$E"/mame/sfiii3na/*; do ln -sf "$PWD/$f" "$E/mame/sfiii3n/$(basename "$f" | sed 's/asia/japan/')"; done; }
if [ "${CHECK:-0}" = 1 ]; then
  B=$T/build
  cc -std=c99 -O2 -ffp-contract=off -w -I$T -I$E/jobs -I$B -I$G -o $B/host_$V $T/host.c $T/core.c $T/sndstub.c src/snd/snd.c -Isrc/snd -Isrc/front -I../cps3-testgame/sdk/include $G/*.c -lm
  N=$($B/host_$V --count); j=0; : > "$O/host.txt"
  while [ $j -lt $N ]; do $B/host_$V $j >> "$O/host.txt"; j=$((j + 1)); done
  # MAME 0.264's SH-2 recompiler does not run code from SIMM 1's cache-through mirror (NC builds hang): -nodrc
  case "$MAKEARGS" in *NC=*) ND=-nodrc ;; *) ND= ;; esac
  PSH2_OUT="$O/sh2.txt" mame $SET $ND -rompath "$E/mame" -skip_gameinfo -nothrottle -sound none -video none \
    -seconds_to_run ${SECONDS_TO_RUN:-40000} -cfg_directory "$O/w/cfg" -nvram_directory "$O/w/nvram" \
    -snapshot_directory "$O/w/snap" -diff_directory "$O/w/diff" -state_directory "$O/w/sta" -inipath "$O/w" \
    -autoboot_script scripts/lua/playsh2.lua >"$O/mame.log" 2>&1 || true
  PSH2_JOBS=$E/jobs/jobs.h python3 $T/report.py "$O/host.txt" "$O/sh2.txt" "$O/nm.txt"
  cat "$O/size.txt"; exit 0
fi
STEP=$(awk '$3 == "_play_step" { print $1 }' "$O/nm.txt")
TD=$PWD/$O; WIN=; k=0
for p in $PAIRS; do k=$((k + 1)); WIN="${WIN:+$WIN,}$((${p#*:} - 1)):$TD/t$k.tr"; done
JTC_STEP=$STEP JTC_WIN=$WIN mame $SET -rompath "$E/mame" -skip_gameinfo -nothrottle -sound none \
  -video none -nodrc -debug -debugger none -seconds_to_run ${SECONDS_TO_RUN:-40000} -cfg_directory "$O/w/cfg" \
  -nvram_directory "$O/w/nvram" -snapshot_directory "$O/w/snap" -diff_directory "$O/w/diff" \
  -state_directory "$O/w/sta" -inipath "$O/w" -autoboot_script scripts/lua/jtcost.lua >"$O/mame.log" 2>&1 || true
k=0
for p in $PAIRS; do
  k=$((k + 1)); r=${p%:*}; s=${p#*:}; f=$O/jtcost_${r}_$s.txt
  [ -s "$TD/t$k.tr" ] || { echo "jtcost_native.sh: no trace for $r $s (see $O/mame.log)" >&2; exit 1; }
  JTC_SIMM1=$E/simm1.bin python3 tools/jtcost.py --route "$r" --step "$s" "$TD/t$k.tr" "$O/nm.txt" > "$f"
  grep '^JTCOST .*consts=fit' "$f"
  [ "${JTC_KEEP:-0}" = 1 ] || rm -f "$TD/t$k.tr"
done
cat "$O/size.txt"
