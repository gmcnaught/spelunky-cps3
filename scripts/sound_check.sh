#!/bin/sh
# tests/sound (the jukebox) in MAME: register log (scripts/lua/sndlog.lua) and WAV at the chip's rate, checked by
# tools/sndcheck.py against the chip model.
set -e
cd "$(dirname "$0")/.."
docker run --rm -v "$PWD":/p -w /p cps3-dev:latest python3 tools/hdsound.py refs/hd/src build/snd >/dev/null
scripts/dmake.sh tests/sound >/dev/null
B=tests/sound/build; O=$B/run; rm -rf "$O"; mkdir -p "$O/w"
SNDLOG="$O/snd.log" mame sfiii3na -rompath "$B/mame" -skip_gameinfo -nothrottle -sound none -samplerate 37286 \
  -wavwrite "$O/out.wav" -video none -seconds_to_run ${SECONDS_TO_RUN:-80} -cfg_directory "$O/w/cfg" \
  -nvram_directory "$O/w/nvram" -snapshot_directory "$O/snap" -diff_directory "$O/w/diff" -state_directory "$O/w/sta" \
  -inipath "$O/w" -autoboot_script scripts/lua/sndlog.lua >"$O/mame.log" 2>&1 || true
docker run --rm -v "$PWD":/p -w /p cps3-dev:latest python3 tools/sndcheck.py build/snd "$O/snd.log" "$O/out.wav"
