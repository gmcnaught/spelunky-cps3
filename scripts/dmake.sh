#!/bin/sh
# make inside the cps3-dev container (../cps3-testgame/docker), with the projects' parent directory mounted so the
# SDK at ../cps3-testgame/sdk is reachable:  scripts/dmake.sh <dir> [make args]
set -e
cd "$(dirname "$0")/.."
D=${1:-.}; shift || true
P=$(cd .. && pwd)
# ../cps3-testgame may be a symlink out of the mount (a git worktree): its target is mounted at its own path too
S=$(cd ../cps3-testgame && pwd -P)
exec docker run --rm -v "$P":/p -v "$S":"$S" -w "/p/$(basename "$PWD")/$D" cps3-dev:latest make "$@"
