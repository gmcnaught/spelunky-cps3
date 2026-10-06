#!/bin/sh
# tests/cachebench in MAME: build, run two passes (scripts/lua/cachebench.lua), print the rows. MAME has no cache and no
# wait states, so this only checks that every row runs (with -nodrc: MAME 0.264's recompiler does not run code from
# SIMM 1's cache-through mirror); the values that matter come from jtcps3 (docs/ICACHE.md section 6).
#   scripts/cachebench.sh        NATIVE=1: sh-elf-gcc and mame from PATH (/opt/sh/bin, /usr/games) instead of docker
set -e
cd "$(dirname "$0")/.."
T=tests/cachebench; O=$T/build/run; rm -rf "$O"; mkdir -p "$O/w"
if [ "${NATIVE:-0}" = 1 ]; then
  export PATH=/opt/sh/bin:/usr/games:$PATH; make -s -C $T >/dev/null; M=mame
else
  scripts/dmake.sh $T >/dev/null; M=scripts/mame.sh
fi
SET=sfiii3na; R=$T/build/mame
if [ "${NATIVE:-0}" = 1 ] && ! mame -listroms sfiii3na >/dev/null 2>&1; then   # MAME before 0.27x: sfiii3n
  SET=sfiii3n; mkdir -p $R/sfiii3n
  for f in $R/sfiii3na/*; do ln -sf "$PWD/$f" "$R/sfiii3n/$(basename "$f" | sed 's/asia/japan/')"; done
fi
CB_OUT="$O/mame.txt" $M $SET -rompath $R -nodrc -skip_gameinfo -nothrottle -sound none -video none \
  -seconds_to_run 120 -cfg_directory "$O/w/cfg" -nvram_directory "$O/w/nvram" -snapshot_directory "$O/snap" \
  -diff_directory "$O/w/diff" -state_directory "$O/w/sta" -inipath "$O/w" -autoboot_script scripts/lua/cachebench.lua \
  >"$O/mame.log" 2>&1 || true
test -s "$O/mame.txt" || { echo "no results (see $O/mame.log)"; exit 1; }
N="LIN_C LIN_U SPARSE_C SPARSE_U HOT_C HOTLIN_C HOTLIN_U LD16_C LD16_HIT LD16_U LD4_C LD4_U SIMM16_C SIMM16_U HOTD_C HOTD_U =IMISS =UWORD =DMISS_RAM =DMISS_SIMM =UNC_RAM =UNC_SIMM"
set -- $N
while read -r i v; do printf '%-12s %8s\n' "$1" "$v"; shift; done < "$O/mame.txt"
