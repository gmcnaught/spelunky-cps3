#!/bin/sh
# Reference run of Spelunky Classic HD 1.2.2 (tools/tracer.py): build the traced game.unx for a route and a seed,
# run it with the release's own GameMaker Linux runner (refs/hd/linux-arm64, native linux/arm64 container,
# docker/hd-runner: Xvfb, Mesa, ALSA null) and join the trace chunks.
#
#   scripts/hd_trace.sh <route> <seed> [name]      route: tests/routes/<route>.txt
#   -> build/trace/<name>.bin (+ .names, .log); name defaults to <route>_s<seed>
#   TRACE_SHOT=r1,... also copies the frames shot_<r>.png / .txt to build/trace/<name>.shot_<r>.*
#   scripts/hd_trace.sh --rng-probe    -> build/trace/rng_probe.bin (tools/tracer.py PROBE_GML; tools/gmrand.py)
#
# Each run uses a fresh copy of the game directory and an empty save area (the runner's SavePrePend
# $HOME/.config/SpelunkyClassicHD/, mounted from build/run/<name>/config): no spelunky.ini / settings.json from
# earlier runs. The tracer's trc_<k>.bin chunks land in the working directory (the game copy, build/run/<name>/game).
# The runner crashes on exit after game_end() (exit 134 or 139); the chunks are complete by then.
# Get the release first: refs/hd/spelunky_classic_hd-linux-arm64.zip from
# https://github.com/yancharkin/SpelunkyClassicHD/releases/download/1.2.2/, unzipped into refs/hd/linux-arm64.
set -eu
cd "$(dirname "$0")/.."
if [ "$1" = --rng-probe ]; then PROBE=1; ROUTE=; SEED=; NAME=rng_probe; else
  PROBE=; ROUTE=$1; SEED=$2; NAME=${3:-${1}_s$2}; fi
T=${TIMEOUT:-1800}
GAME=refs/hd/linux-arm64
[ -f "$GAME/assets/game.unx" ] || { echo "missing $GAME/assets/game.unx"; exit 1; }
docker image inspect spelunky-hd-runner >/dev/null 2>&1 || docker build -q -t spelunky-hd-runner docker/hd-runner
RUN=build/run/$NAME
rm -rf "$RUN"; mkdir -p "$RUN/config" build/trace
cp -R "$GAME/." "$RUN/game"
if [ -n "$PROBE" ]; then
  python3 tools/tracer.py rng-probe "$GAME/assets/game.unx" "$RUN/game/assets/game.unx"
else
  python3 tools/tracer.py build "$GAME/assets/game.unx" "tests/routes/$ROUTE.txt" "$RUN/game/assets/game.unx" --seed "$SEED"
  mv "$RUN/game/assets/game.unx.names" "build/trace/$NAME.names"
fi
rm -f "$RUN/game/assets/game.unx.csx"
start=$(date +%s)
docker run --rm --platform linux/arm64 -v "$PWD/$RUN:/r" -w /r/game spelunky-hd-runner sh -c "
  Xvfb :99 -screen 0 1280x720x24 -nolisten tcp >/dev/null 2>&1 &
  for i in 1 2 3 4 5 6 7 8 9 10; do [ -e /tmp/.X11-unix/X99 ] && break; sleep 0.5; done
  mkdir -p /tmp/.config && ln -s /r/config /tmp/.config/SpelunkyClassicHD
  timeout $T ./SpelunkyClassicHD > /r/run.log 2>&1; echo \"runner exit \$?\" >> /r/run.log" || true
cp "$RUN/run.log" "build/trace/$NAME.log"
if [ -n "$PROBE" ]; then
  [ -f "$RUN/game/rng_probe.bin" ] || { echo "no rng_probe.bin"; tail -20 "build/trace/$NAME.log"; exit 1; }
  cp "$RUN/game/rng_probe.bin" build/trace/rng_probe.bin; echo "build/trace/rng_probe.bin"; exit 0
fi
for f in "$RUN"/game/shot_*; do [ -e "$f" ] && cp "$f" "build/trace/$NAME.$(basename "$f")"; done
n=$(ls "$RUN/game" | grep -c '^trc_.*\.bin$' || true)
[ "$n" -gt 0 ] || { echo "no trc_*.bin (log: build/trace/$NAME.log)"; tail -20 "build/trace/$NAME.log"; exit 1; }
k=0; : > "build/trace/$NAME.bin"
while [ $k -lt "$n" ]; do cat "$RUN/game/trc_$k.bin" >> "build/trace/$NAME.bin"; k=$((k + 1)); done
echo "build/trace/$NAME.bin ($n chunks, $(wc -c < "build/trace/$NAME.bin") bytes, $(( $(date +%s) - start )) s; $(tail -1 "build/trace/$NAME.log"))"
