#!/bin/sh
# tests/sndrules (the audio rules, docs/AUDIO.md) in MAME: register log (scripts/lua/sndlog.lua) and WAV at the
# chip's rate, checked by tools/sndcheck.py rules: logged events equal to the rules model's, audio equal to the chip
# model within 1 LSB.
set -e
cd "$(dirname "$0")/.."
[ -f build/snd/snd.bin ] || docker run --rm -v "$PWD":/p -w /p cps3-dev:latest python3 tools/hdsound.py refs/hd/src build/snd >/dev/null
scripts/dmake.sh tests/sndrules >/dev/null
B=tests/sndrules/build; O=$B/run; rm -rf "$O"; mkdir -p "$O/w"
SNDLOG="$O/snd.log" mame sfiii3na -rompath "$B/mame" -skip_gameinfo -nothrottle -sound none -samplerate 37286 \
  -wavwrite "$O/out.wav" -video none -seconds_to_run ${SECONDS_TO_RUN:-40} -cfg_directory "$O/w/cfg" \
  -nvram_directory "$O/w/nvram" -snapshot_directory "$O/snap" -diff_directory "$O/w/diff" -state_directory "$O/w/sta" \
  -inipath "$O/w" -autoboot_script scripts/lua/sndlog.lua >"$O/mame.log" 2>&1 || true
docker run --rm -v "$PWD":/p -w /p cps3-dev:latest python3 tools/sndcheck.py rules build/snd tests/sndrules/script.txt \
  "$O/snd.log" "$O/out.wav"
