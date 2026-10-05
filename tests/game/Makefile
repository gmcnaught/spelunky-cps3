# tests/game: the game program (src/main: shell -> play_step -> draw) in MAME, playing a route's keys
# (build/route.h, mkroute.py) and holding the frames listed in SNAPS on screen for scripts/lua/gamesnap.lua.
# scripts/game_check.sh snapshots src/game (git HEAD, or GAME_REV) and build/gen into build/g with PIN_MAX 1792
# (main RAM is 512 KB) and runs the gate.
#   scripts/dmake.sh tests/game ROUTE=p4_exit559 SEED=559 SNAPS=100,200 [LEVEL=1 MONEY=0 ENEMIES=0] [OPT=-O2]
#   [HOLD=n: the SNAPS frames held n frames each on their own, sound running (jtcps3 frame check, scripts/jt_frames.sh)]
#   [ATTRACT=1: no coin / start, the attract mode (src/front) from the intro; ATTRACT=5: from rHighscores]
#   [PLAY=1: playable: the cabinet's coin / Start and controls, a random seed, enemies on, level 1 (route unused);
#    the MiSTer set is spelunky.zip + "Spelunky Classic Arcade.mra"]
#   [DEV=1: developer options in the settings screen (INVINCIBLE; src/shell SHELL_DEV); scripts/release.sh leaves it off]
#   [SMOOTH=1: smooth motion on the route (scripts/smooth_check.sh: each held record's midpoint and own frames)]
#   [DPROF=1: src/draw's parts timed (draw_st.prof)] [DARK=a8: drawn as a dark level at alpha byte a8] [HUD=0: no HUD, as the runner's application_surface frames]
PROG   := $(if $(filter 1,$(PLAY)),spelunky,game)$(if $(filter 1,$(DEV)),dev)
TITLE  := $(if $(filter 1,$(PLAY)),Spelunky Classic Arcade,Spelunky CPS3)$(if $(filter 1,$(DEV)), (Dev))
ROOT   := ../..
GEN    := $(ROOT)/build/gen
OPT    ?= -O2
# REV: the build's commit, 8 hex digits (scripts/release.sh passes it: the container has no repository), DIRTY=1:
# the tree differed from it. Both go into the game capture's header (src/main game.h)
REV    ?= 0
DIRTY  ?= 0
ROUTE  ?= p4_exit559
SEED   ?= 559
SNAPS  ?=
LEVEL  ?= 1
MONEY  ?= 0
ENEMIES ?= 0
TAIL   ?= 30
GSRC   := $(notdir $(wildcard build/g/*.c))
SRCS   := main.c $(ROOT)/src/main/main.c $(ROOT)/src/main/game.c $(ROOT)/src/draw/draw.c $(ROOT)/src/hud/hud.c \
          $(ROOT)/src/shell/shell.c $(ROOT)/src/shell/input.c $(ROOT)/src/shell/credit.c $(ROOT)/src/shell/hiscore.c \
          $(ROOT)/src/snd/snd.c $(GEN)/sprites.c $(GEN)/drawtab.c $(addprefix build/g/,$(GSRC)) \
          $(ROOT)/src/front/front.c $(GEN)/fronttables.c \
          $(ROOT)/src/sh2/softfp.c $(ROOT)/src/sh2/softfp_sh2.S $(ROOT)/src/sh2/fma.c
CFLAGS := $(OPT) -std=gnu99 -fno-bit-tests -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers \
          -Ibuild/g -Ibuild -I. -I$(GEN) -I$(ROOT)/src/draw -I$(ROOT)/src/main -I$(ROOT)/src/hud -I$(ROOT)/src/shell \
          -I$(ROOT)/src/snd -I$(ROOT)/src/front -DSOFTFP_ASM -fdata-sections -Wl,-T,sprbss.ld -Wl,-T,../ramcheck.ld \
          '-DDRAW_MAPS_SECTION=__attribute__((section(".bss.drawmaps")))' \
          '-DDRAW_CACHE_SECTION=__attribute__((section(".bss.drawcache")))' '-DCAPTURE_SECTION=__attribute__((section(".bss.capture")))' -DCAPTURE_REV=0x$(REV)u -DCAPTURE_DIRTY=$(DIRTY) $(if $(filter 1,$(DPROF)),-DDRAW_PROFILE) $(if $(filter 0,$(HUD)),-DGAME_NOHUD) $(if $(DARK),-DGAME_DARK=$(DARK)) $(if $(ATTRACT),-DGAME_ATTRACT=$(ATTRACT)) $(if $(HOLD),-DGAME_HOLD=$(HOLD)) $(if $(filter 1,$(PLAY)),-DGAME_PLAY) $(if $(filter 1,$(SMOOTH)),-DGAME_SMOOTH) $(if $(filter 1,$(DEV)),-DSHELL_DEV)
FLASH  := build/flash.bin
include ../../../cps3-testgame/sdk/sdk.mk

build/g/stamp:
	@echo "build/g missing: scripts/game_check.sh snapshots src/game and build/gen there"; false
$(addprefix build/g/,$(GSRC)): build/g/stamp
.PHONY: build/route.h
build/route.h: mkroute.py
	@mkdir -p build
	python3 mkroute.py $@ $(ROOT)/tests/routes/$(ROUTE).txt $(SEED) $(LEVEL) $(MONEY) $(ENEMIES) $(TAIL) "$(SNAPS)"
$(OUT)/main.elf: build/g/stamp build/route.h sprbss.ld $(GEN)/drawtab.h

$(FLASH): assets.py $(GEN)/gfx.bin $(GEN)/gfx.json $(GEN)/hud.bin $(GEN)/hud.json $(ROOT)/build/snd/snd.bin $(GEN)/fade.bin
	@mkdir -p build
	python3 assets.py $(GEN) $(ROOT)/build/snd/snd.bin $(FLASH)
