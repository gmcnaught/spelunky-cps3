#!/bin/sh
# HUD reference frames from HD 1.2.2's own runner (tools/hudref.py: a tools/tracer.py route build plus a Draw GUI
# End that draws HD's scrDrawHUD / showMessages for each tests/hud/cases.json state), then tools/hudcheck.py hd on
# each case. Runner set-up as scripts/hd_trace.sh (docker/hd-runner, refs/hd/linux-arm64).
#   scripts/hd_hudref.sh [route] [seed] [first record]       default: p1_walk 1 100
set -eu
cd "$(dirname "$0")/.."
ROUTE=${1:-p1_walk}; SEED=${2:-1}; R0=${3:-100}; NAME=hud_ref
GAME=refs/hd/linux-arm64
[ -f "$GAME/assets/game.unx" ] || { echo "missing $GAME/assets/game.unx"; exit 1; }
docker image inspect spelunky-hd-runner >/dev/null 2>&1 || { echo "no spelunky-hd-runner image (scripts/hd_trace.sh builds it)"; exit 1; }
RUN=build/run/$NAME; OUT=build/hud/ref
rm -rf "$RUN" "$OUT"; mkdir -p "$RUN/config" "$OUT"
cp -R "$GAME/." "$RUN/game"
python3 tools/hudref.py "$GAME/assets/game.unx" "tests/routes/$ROUTE.txt" "$RUN/game/assets/game.unx" \
  tests/hud/cases.json "$R0" --seed "$SEED"
rm -f "$RUN/game/assets/game.unx.csx" "$RUN/game/assets/game.unx.names"
docker run --rm --platform linux/arm64 -v "$PWD/$RUN:/r" -w /r/game spelunky-hd-runner sh -c "
  Xvfb :99 -screen 0 1280x720x24 -nolisten tcp >/dev/null 2>&1 &
  for i in 1 2 3 4 5 6 7 8 9 10; do [ -e /tmp/.X11-unix/X99 ] && break; sleep 0.5; done
  mkdir -p /tmp/.config && ln -s /r/config /tmp/.config/SpelunkyClassicHD
  ./SpelunkyClassicHD > /r/run.log 2>&1 & p=\$!; t=0
  while [ ! -e trc_done.txt ] && [ ! -e gml_error.txt ] && [ \$t -lt 600 ] && kill -0 \$p 2>/dev/null; do sleep 1; t=\$((t + 1)); done
  sleep 1; kill -9 \$p 2>/dev/null; echo \"runner stopped (\$t s)\" >> /r/run.log" || true
[ -f "$RUN/game/gml_error.txt" ] && { echo "GML error:"; cat "$RUN/game/gml_error.txt"; exit 1; }
for f in "$RUN"/game/shot_* "$RUN"/game/gui_*; do [ -e "$f" ] && cp "$f" "$OUT/"; done
n=$(python3 -c "import json; print(len(json.load(open('tests/hud/cases.json'))))")
fail=0; k=0
while [ $k -lt "$n" ]; do
  r=$((R0 + k))
  [ -f "$OUT/gui_$r.png" ] || { echo "case $k: no gui_$r.png"; fail=1; k=$((k + 1)); continue; }
  dw=$(cut -d' ' -f3 "$OUT/gui_$r.txt")
  python3 tools/hudcheck.py hd tests/hud/cases.json refs/hd/src $k "$OUT/shot_$r.png" "$OUT/gui_$r.png" "$dw" \
    "$OUT/diff_$k.png" || fail=1
  k=$((k + 1))
done
exit $fail
