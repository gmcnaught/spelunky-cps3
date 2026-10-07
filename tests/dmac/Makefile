# SH-2 DMAC on the CPS3 (docs/DRAW.md section 8): readback and transfer times main RAM -> main RAM / sprite RAM in
# 16-byte and longword units, cycle steal and burst; CPU store loops; a cached CPU loop's slowdown while a transfer
# runs. Results on the text layer (and at main RAM dm_res for MAME).
#   scripts/dmake.sh tests/dmac [mister]; MAME: scripts/mame.sh ...; jtcps3: ../cps3-testgame/scripts/mister_run.sh
PROG  := dmactest
TITLE := Spelunky CPS3 DMAC test
SRCS  := main.c
include ../../../cps3-testgame/sdk/sdk.mk
