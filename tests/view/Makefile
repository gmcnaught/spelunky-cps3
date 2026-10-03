# P3 level viewer (PLAN.md P3): a level dumped by the HD reference runner drawn by the CPS3 (tools/viewlevel.py).
#   python3 tools/hdsprites.py refs/hd/src build/gen; python3 tools/hdobjects.py refs/hd/src build/gen/objects.h
#   python3 tools/viewlevel.py c <trace> <names> <rec> build/gen build/view/level.h
#   scripts/dmake.sh tests/view; scripts/view_check.sh
PROG   := view
TITLE  := Spelunky CPS3 level viewer
GEN    := ../../build/gen
SRCS   := main.c $(GEN)/sprites.c
FLASH  := build/flash.bin
CFLAGS := -O3 -I$(GEN) -I../../build/view
include ../../../cps3-testgame/sdk/sdk.mk

$(FLASH): assets.py $(GEN)/gfx.bin $(GEN)/gfx.json
	@mkdir -p build
	python3 assets.py $(GEN) $(FLASH)
