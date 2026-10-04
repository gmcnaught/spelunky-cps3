# P6 jukebox (PLAN.md P6): every HD sound from tools/hdsound.py through the SDK's voices; scripts/sound_check.sh
# compares MAME's output with the chip model (tools/sndcheck.py).
#   docker run --rm -v "$PWD":/p -w /p cps3-dev:latest python3 tools/hdsound.py refs/hd/src build/snd
#   scripts/dmake.sh tests/sound
PROG   := sound
TITLE  := Spelunky CPS3 jukebox
SND    := ../../build/snd
SRCS   := main.c
FLASH  := build/flash.bin
CFLAGS := -O2 -I$(SND)
include ../../../cps3-testgame/sdk/sdk.mk

$(FLASH): assets.py $(SND)/snd.bin
	@mkdir -p build
	python3 assets.py $(SND)/snd.bin $(FLASH)
