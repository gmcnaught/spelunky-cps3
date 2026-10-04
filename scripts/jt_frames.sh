#!/bin/sh
# jtcps3 frame check: game program builds that play a route and hold chosen records on screen (tests/game HOLD,
# sound running; a coin sound when a hold starts, a click when it ends), for MiSTer screenshots compared with
# tools/jtshot.py (cmp, or ident: which held record a screenshot shows).
# Hold k lasts until frame T_k + H from the program's start (deterministic windows):
#   pass 1: a MAME run with minimal holds gives the route steps before each held record;
#   T_k: those steps at jtcps3's estimated step time (a game step 0.081 s: tests/gametime at b3348ac, 1.86-2.21 M
#        clocks a pair at 25 MHz; attract steps fit their 2 frames, MAME time) + 15% margin, after the previous
#        window's end; the game start 1.5 s boot + 3.7 s;
#   pass 2: the build with T_k (build/hold.h), checked in MAME (the held frames against the model) and packed for
#        the MiSTer.
#   scripts/jt_frames.sh   -> build/jtshot/{<set>.zip + .mra, expect/expect_<set>_<rec>.png, schedule.txt}
# HOLD=frames per window (default 1192: 20 s at 59.6 Hz)
cd "$(dirname "$0")/.."
# OUT=dir (default build/jtshot): never wiped; files are replaced one by one, the expected images in OUT/expect
H=${HOLD:-1192}; D=${OUT:-build/jtshot}; mkdir -p "$D/expect"; : > "$D/mame.txt"; : > "$D/schedule.txt"
FPS=59.6
check() {  # route seed trace recs level money enemies attract
  if [ -n "$8" ]; then ATTRACT=$8 HOLD=$H HUD=1 scripts/game_check.sh $1 $2 $3 $4 $5 $6 $7
  else HOLD=$H HUD=1 scripts/game_check.sh $1 $2 $3 $4 $5 $6 $7; fi | grep -v ": draw"
}
run() {   # name route seed trace recs level money enemies [attract]
  n=$1; r=$2; s=$3; t=$4; recs=$5; o=tests/game/build/${r}_hud1${9:+_attract$9}_hold
  k=$(echo $recs | tr ',' '\n' | grep -c .)
  python3 -c "import sys; k=int(sys.argv[1]); print('#define NHOLDS %d\nstatic const uint32_t hold_at[NHOLDS] = { %s };' % (k, ', '.join(['0'] * k)))" $k > tests/game/build/hold.h
  check $r $s $t $recs $6 $7 $8 $9 > /dev/null                      # pass 1
  grep '^S' "$o/out.txt" | awk '{ print $3, $17, $NF }' > "$D/pass1_$n.txt"
  python3 - "$D/pass1_$n.txt" $H $FPS ${9:+attract} > tests/game/build/hold.h 2>> "$D/schedule.txt" <<'PY'
import sys
rows = [l.split() for l in open(sys.argv[1])]
H, fps, attract = int(sys.argv[2]), float(sys.argv[3]), len(sys.argv) > 4
T, end, prev = [], 0.0, 0
for rec, steps, t in rows:
    steps, t = int(steps), float(t)
    if attract: arr = t * 1.15 * fps                   # minimal holds before: none (one hold per attract set)
    elif not T: arr = (1.5 + 3.7 + steps * 0.081) * 1.15 * fps
    else: arr = end + (steps - prev) * 0.081 * 1.15 * fps
    T.append(int(arr + 0.5)); end = T[-1] + H; prev = steps
    sys.stderr.write(f'{sys.argv[1].split("pass1_")[1][:-4]:10s} record {rec:>5s}: window {T[-1] / fps:6.1f} - {end / fps:6.1f} s from load\n')
print('#define NHOLDS %d\nstatic const uint32_t hold_at[NHOLDS] = { %s };' % (len(T), ', '.join(map(str, T))))
PY
  check $r $s $t $recs $6 $7 $8 $9 | tee -a "$D/mame.txt"           # pass 2
  cp "$o"/elf/mister/*.zip "$D/$n.zip"                              # the MRA names its set and zip: one per build
  sed -e "s/zip=\"game.zip\"/zip=\"$n.zip\"/" -e "s/<setname>game</<setname>$n</" "$o"/elf/mister/*.mra > "$D/$n.mra"
  rm -rf "$o/elf/mister"
  for rec in $(echo $recs | tr ',' ' '); do
    if [ -n "$9" ]; then python3 tools/jtshot.py expect build/trace/$t.bin $rec "$D/expect/expect_${n}_$rec.png" --hud \
                           --sstext 37,27,CREDIT --sstext 45,27,0
    else python3 tools/jtshot.py expect build/trace/$t.bin $rec "$D/expect/expect_${n}_$rec.png" --hud; fi
  done
}
run jt_p4 p4_exit559 559 g_p4_exit559_s559 30,300,520,800 1 0 0
run jt_shop p5_shop 96 g_p5_shop_s96 162,242 2 40000 1
run jt_attract p8_boot 7 g_p8_boot_s7 1040 1 0 0 1
cat "$D/schedule.txt"; ls "$D"
