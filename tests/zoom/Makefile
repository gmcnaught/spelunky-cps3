# Full-screen X zoom (PLAN.md P3): a 320-px-wide test grid stretched to 384 by the CRTC zoom register.
#   scripts/dmake.sh tests/zoom; scripts/zoom_check.sh
PROG  := zoom
TITLE := Spelunky CPS3 zoom test
SRCS  := main.c
FLASH := build/flash.bin
include ../../../cps3-testgame/sdk/sdk.mk

$(FLASH) build/assets.h: assets.py
	@mkdir -p build
	python3 assets.py $(FLASH) build/assets.h

build/main.elf: build/assets.h
