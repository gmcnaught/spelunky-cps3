# P6 audio rules test (docs/AUDIO.md): src/snd driven by script.txt; scripts/sndrules_check.sh checks it in MAME.
#   scripts/dmake.sh tests/sndrules
PROG   := sndrules
TITLE  := Spelunky CPS3 audio rules
SND    := ../../build/snd
SRCS   := main.c ../../src/snd/snd.c
FLASH  := build/flash.bin
CFLAGS := -O2 -I../../src/snd -fno-bit-tests
include ../../../cps3-testgame/sdk/sdk.mk

build/main.elf: build/script.h ../../src/snd/snd.h $(SND)/snd.h

build/script.h: mkscript.py script.txt
	@mkdir -p build
	python3 mkscript.py script.txt $@

$(FLASH): ../sound/assets.py $(SND)/snd.bin
	@mkdir -p build
	python3 ../sound/assets.py $(SND)/snd.bin $(FLASH)
