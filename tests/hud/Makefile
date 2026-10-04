# HUD test (src/hud, PLAN.md P8): HD's in-game HUD for the states of cases.json drawn by the CPS3, one case every
# CASE_FRAMES frames, checked in MAME against tools/hudcheck.py's model (scripts/hud_check.sh).
#   python3 tools/hdsprites.py refs/hd/src build/gen; python3 tools/hudart.py refs/hd/src build/gen
#   python3 tools/hudcheck.py cases tests/hud/cases.json build/hud/cases.h
#   scripts/dmake.sh tests/hud; scripts/hud_check.sh
PROG   := hud
TITLE  := Spelunky CPS3 HUD test
GEN    := ../../build/gen
SRCS   := main.c ../../src/hud/hud.c $(GEN)/sprites.c
FLASH  := build/flash.bin
CFLAGS := -O2 -I$(GEN) -I../../build/hud -I../../src/hud
include ../../../cps3-testgame/sdk/sdk.mk

$(FLASH): assets.py $(GEN)/gfx.bin $(GEN)/gfx.json $(GEN)/hud.bin $(GEN)/hud.json
	@mkdir -p build
	python3 assets.py $(GEN) $(FLASH)
