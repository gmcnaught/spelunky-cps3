#!/bin/sh
# MAME for the project scripts: headless in the spelunky-mame container (docker/mame, MAME 0.289 as the host's),
# so nothing opens a window or takes focus on the host. The projects directory is mounted at its own path, so
# absolute and relative paths (and worktree symlinks into the main checkout) resolve as on the host.
# MAME_NATIVE=1 runs the host's mame instead (to watch: with -video other than none).
# Environment passed through: the variables the Lua scripts read (scripts/lua/*.lua).
if [ "${MAME_NATIVE:-0}" = 1 ]; then exec mame "$@"; fi
P=/Users/gmcnaught/MisterFPGA-Projects
D=$(cd "$(dirname "$0")/.." && pwd)
docker image inspect spelunky-mame:latest >/dev/null 2>&1 || docker build -q -t spelunky-mame:latest "$D/docker/mame" >/dev/null
exec docker run --rm -v "$P":"$P" -w "$PWD" \
  -e CAP_ADDR -e CAP_GOD -e CAP_LOG -e CAP_MONKEY -e CAP_PLAY -e CAP_SHOTS -e CAP_START -e GAME_MIDSNAP -e GAME_NSNAPS -e GAME_OUT -e GT_OUT -e JTC_STEP -e JTC_WIN -e PSH2_ATTR -e PSH2_NJOBS -e PSH2_OUT -e SHLOG -e SNAP_FRAMES -e SNDLOG -e SOFTFP_OUT \
  spelunky-mame:latest /usr/games/mame "$@"
