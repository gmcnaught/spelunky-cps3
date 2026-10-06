# tests/gametime: the game program's frame budget (main.c) on a route; scripts/gametime_check.sh builds and runs it.
# Reuses tests/game's route tool, flash assets and sprite-RAM layout (read-only), and src/game snapshotted into
# build/g by the script (PIN_MAX 1792).
#   scripts/dmake.sh tests/gametime [ROUTE=p4_push_rope SEED=365 LEVEL=1 MONEY=0 ENEMIES=0] [ROUTE2=p5_snakes SEED2=101
#   LEVEL2=1 MONEY2=0 ENEMIES2=1] [ATTRACT_FRAMES=3600] [NC=: the SDK's plain link; default tests/nc.mk's, as tests/game]
PROG   := gametime
TITLE  := Spelunky CPS3 frame budget
ROOT   := ../..
GEN    := $(ROOT)/build/gen
OPT    ?= -O2
REV    ?= 0
DIRTY  ?= 0
ROUTE  ?= p4_push_rope
SEED   ?= 365
LEVEL  ?= 1
MONEY  ?= 0
ENEMIES ?= 0
ROUTE2 ?= p5_snakes
SEED2  ?= 101
LEVEL2 ?= 1
MONEY2 ?= 0
ENEMIES2 ?= 1
ATTRACT_FRAMES ?= 3600
TAIL   ?= 30
GSRC   := $(notdir $(wildcard build/g/*.c))
SRCS   := main.c hooks.c frt.S ../playsh2/bigtext.c $(ROOT)/src/main/game.c $(ROOT)/src/draw/draw.c $(ROOT)/src/hud/hud.c \
          $(ROOT)/src/shell/shell.c $(ROOT)/src/shell/input.c $(ROOT)/src/shell/credit.c $(ROOT)/src/shell/hiscore.c \
          $(ROOT)/src/snd/snd.c $(GEN)/sprites.c $(GEN)/drawtab.c $(addprefix build/g/,$(GSRC)) \
          $(ROOT)/src/front/front.c $(GEN)/fronttables.c \
          $(ROOT)/src/sh2/softfp.c $(ROOT)/src/sh2/softfp_sh2.S $(ROOT)/src/sh2/fma.c
CFLAGS := $(OPT) -std=gnu99 -fno-bit-tests -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers \
          -Ibuild/g -Ibuild -I. -I$(GEN) -I$(ROOT)/src/draw -I$(ROOT)/src/main -I$(ROOT)/src/hud -I$(ROOT)/src/shell \
          -I$(ROOT)/src/snd -I$(ROOT)/src/front -DSOFTFP_ASM -fdata-sections -Wl,-T,../game/sprbss.ld -Wl,-T,marker.ld -Wl,-T,../ramcheck.ld \
          '-DDRAW_MAPS_SECTION=__attribute__((section(".bss.drawmaps")))' \
          '-DDRAW_CACHE_SECTION=__attribute__((section(".bss.drawcache")))' '-DCAPTURE_SECTION=__attribute__((section(".bss.capture")))' -DCAPTURE_REV=0x$(REV)u -DCAPTURE_DIRTY=$(DIRTY) '-DROUTE_NAME="$(shell echo $(ROUTE) | tr a-z A-Z)"' '-DROUTE2_NAME="$(shell echo $(ROUTE2) | tr a-z A-Z)"' -DATTRACT_FRAMES=$(ATTRACT_FRAMES) $(if $(filter 1,$(SMOOTH)),-DGAME_SMOOTH)
FLASH  := build/flash.bin
include ../../../cps3-testgame/sdk/sdk.mk
include ../nc.mk

build/g/stamp:
	@echo "build/g missing: scripts/gametime_check.sh snapshots src/game and build/gen there"; false
$(addprefix build/g/,$(GSRC)): build/g/stamp
.PHONY: build/route.h build/route2.h
build/route.h:
	@mkdir -p build
	python3 ../game/mkroute.py $@ $(ROOT)/tests/routes/$(ROUTE).txt $(SEED) $(LEVEL) $(MONEY) $(ENEMIES) $(TAIL) ""
build/route2.h:                                   # route 2: route.h's names with ROUTE2_ / route2_
	@mkdir -p build
	python3 ../game/mkroute.py $@.tmp $(ROOT)/tests/routes/$(ROUTE2).txt $(SEED2) $(LEVEL2) $(MONEY2) $(ENEMIES2) $(TAIL) ""
	sed -e 's/ROUTE_/ROUTE2_/g' -e 's/route_keys/route2_keys/g' -e 's/NSNAPS/NSNAPS2/g' -e 's/snaps\[/snaps2[/g' $@.tmp > $@
	rm -f $@.tmp
$(OUT)/main.elf: build/g/stamp build/route.h build/route2.h ../game/sprbss.ld marker.ld $(GEN)/drawtab.h $(ROOT)/src/main/main.c

$(FLASH): ../game/assets.py $(GEN)/gfx.bin $(GEN)/gfx.json $(GEN)/hud.bin $(GEN)/hud.json $(ROOT)/build/snd/snd.bin $(GEN)/fade.bin
	@mkdir -p build
	python3 ../game/assets.py $(GEN) $(ROOT)/build/snd/snd.bin $(FLASH)
