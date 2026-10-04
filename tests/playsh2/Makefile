# tests/playsh2: the level generator and the play loop on the SH-2 (main.c), checked against the host (host.c) by
# scripts/playsh2_check.sh, which snapshots src/game (git HEAD, or GAME_REV) and build/gen into build/g with play.h's
# PIN_MAX set to its PIN (main RAM is 512 KB; the host's 4,096 instances of 372 bytes are 1.5 MB). PROF=<ticks>: PC / PR sampling every <ticks> x 8 clocks
# during the work of kind PROF_KIND (3 route steps and level starts, 1 generation),
# one interrupt in PROF_SKIP logged (49,152 samples at most).
#   scripts/dmake.sh tests/playsh2 [OUT=build/run] [OPT=-O2] [PROF=0]
PROG   := playsh2
TITLE  := Spelunky CPS3 SH-2 play timing
ROOT   := ../..
OPT    ?= -O2
PROF   ?= 0
PROF_KIND ?= 3
PROF_SKIP ?= 1
GSRC   := rng.c inst.c gen.c genroom.c genobj.c genent.c pworld.c pscript.c pobj.c pplayer.c prun.c pcol.c ptrans.c
SRCS   := main.c core.c isr.S $(addprefix build/g/,$(GSRC)) build/g/objects.c build/g/gentables.c build/g/playtables.c
CFLAGS := $(OPT) -std=gnu99 -fno-bit-tests -Wno-unused-parameter -Wno-sign-compare -Ibuild/g -Ibuild -I. \
          -DPROF_PERIOD=$(PROF) -DPROF_KIND=$(PROF_KIND) -DPROF_SKIP=$(PROF_SKIP) -fdata-sections -Wl,-T,sprbss.ld
# PROF_WRAP=1: soft-float calls through wrap.S, so the samples inside them name their caller
PROF_WRAP ?= 0
SFWRAP := adddf3 subdf3 muldf3 divdf3 eqdf2 nedf2 gtdf2 gedf2 ltdf2 ledf2 extendsfdf2 truncdfsf2 fixdfsi floatsidf floatunsidf addsf3 subsf3 mulsf3 divsf3 gtsf2 gesf2 ltsf2 lesf2 fixsfsi floatsisf
comma  := ,
ifeq ($(PROF_WRAP),1)
SRCS   += wrap.S
CFLAGS += -DPROF_WRAP=1 $(addprefix -Wl$(comma)--wrap=__,$(SFWRAP))
endif
# BENCH=1: only the soft-float microbenchmark (bench.c)
ifeq ($(BENCH),1)
SRCS   += bench.c
CFLAGS += -DBENCH=1
endif
include ../../../cps3-testgame/sdk/sdk.mk

build/g/stamp:
	@echo "build/g missing: scripts/playsh2_check.sh snapshots src/game and build/gen there"; false
$(addprefix build/g/,$(GSRC)): build/g/stamp
build/jobs.h: mkjobs.py $(wildcard ../routes/p4_*.txt)
	@mkdir -p build
	python3 mkjobs.py $@ ../routes
$(OUT)/main.elf: build/g/stamp build/jobs.h core.h sprbss.ld wrap.S
