# tests/playsh2: the level generator and the play loop on the SH-2 (main.c), checked against the host (host.c) by
# scripts/playsh2_check.sh, which snapshots src/game (git HEAD, or GAME_REV) and build/gen into build/g with play.h's
# PIN_MAX set to its PIN (main RAM is 512 KB; the host's 4,096 instances of 372 bytes are 1.5 MB). PROF=<ticks>: PC / PR sampling every <ticks> x 32 clocks
# during the work of kind PROF_KIND (3 route steps and level starts, 1 generation),
# one interrupt in PROF_SKIP logged (65,536 samples at most).
#   scripts/dmake.sh tests/playsh2 [OUT=build/run] [OPT=-O2] [PROF=0]
PROG   := playsh2
comma  := ,
TITLE  := Spelunky CPS3 SH-2 play timing
ROOT   := ../..
OPT    ?= -O2
PROF   ?= 0
PROF_KIND ?= 3
PROF_SKIP ?= 1
# every C file of the snapshot (src/game's and build/gen's tables)
GSRC   := $(notdir $(wildcard build/g/*.c))
SRCS   := main.c core.c isr.S $(addprefix build/g/,$(GSRC)) $(ROOT)/src/sh2/fma.c $(ROOT)/src/snd/snd.c   # fma.c: pcol.c's fmaf (no libm)
CFLAGS := $(OPT) -std=gnu99 -fno-bit-tests -Wno-unused-parameter -Wno-sign-compare -Ibuild/g -Ibuild -I. -I$(ROOT)/src/snd \
          -DPROF_PERIOD=$(PROF) -DPROF_KIND=$(PROF_KIND) -DPROF_SKIP=$(PROF_SKIP) -fdata-sections -Wl,-T,sprbss.ld
# SOFTFP=1: src/sh2/softfp.c in place of libgcc's fp-bit soft-float
ifeq ($(SOFTFP),1)
SRCS   += $(ROOT)/src/sh2/softfp.c $(ROOT)/src/sh2/softfp_sh2.S
CFLAGS += -DSOFTFP_ASM
endif
# ATTR=1: exclusive time per category / event / object (attr.c, linker wraps of the play loop's entry points)
ATTR   ?= 0
AWRAP  := collision_point_p collision_line_p collision_rect_p instance_place_p instance_nearest_p \
          distance_to_object_p distance_to_instance_p pin_bbox pin_overlap pcol_handle ev_create ev_destroy ev_step \
          ev_outside ev_end_step ev_animend ev_draw ev_alarm ev_collision instance_exists_p instance_first_p \
          instance_number_p pw_with
ifeq ($(ATTR),1)
SRCS   += attr.c
CFLAGS += -DATTR=1 $(addprefix -Wl$(comma)--wrap=,$(AWRAP))
endif
# FPCHECK=1 (with SOFTFP=1): every assembly soft-float call checked against softfp.c's C (fpcheck.c, fpref.c)
FPCHECK ?= 0
ifeq ($(FPCHECK),1)
SRCS   += fpcheck.c fpref.c
CFLAGS += $(addprefix -Wl$(comma)--wrap=__,adddf3 subdf3 addsf3 subsf3 eqdf2 nedf2 ltdf2 ledf2 gtdf2 gedf2)
endif
# PROF_WRAP=1: soft-float calls through wrap.S, so the samples inside them name their caller
PROF_WRAP ?= 0
SFWRAP := adddf3 subdf3 muldf3 divdf3 eqdf2 nedf2 gtdf2 gedf2 ltdf2 ledf2 extendsfdf2 truncdfsf2 fixdfsi floatsidf floatunsidf addsf3 subsf3 mulsf3 divsf3 gtsf2 gesf2 ltsf2 lesf2 fixsfsi floatsisf
ifeq ($(PROF_WRAP),1)
SRCS   += wrap.S
CFLAGS += -DPROF_WRAP=1 $(addprefix -Wl$(comma)--wrap=__,$(SFWRAP))
endif
# BENCH=1: only the soft-float microbenchmark (bench.c)
ifeq ($(BENCH),1)
SRCS   += bench.c
CFLAGS += -DBENCH=1
endif
# JT=1: the jtcps3 variant (scripts/playsh2_jt.sh): main RAM only (no sprbss.ld), results on screen (bigtext.c)
ifeq ($(JT),1)
SRCS   += bigtext.c
CFLAGS := -Ibuild/jt $(filter-out -Wl$(comma)-T$(comma)sprbss.ld,$(CFLAGS)) -DJT=1   # build/jt: jobs.h, jt_expect.h
endif
include ../../../cps3-testgame/sdk/sdk.mk

build/g/stamp:
	@echo "build/g missing: scripts/playsh2_check.sh snapshots src/game and build/gen there"; false
$(addprefix build/g/,$(GSRC)): build/g/stamp
build/jobs.h: mkjobs.py $(wildcard ../routes/p*.txt)
	@mkdir -p build
	python3 mkjobs.py $@ ../routes
$(OUT)/main.elf: build/g/stamp build/jobs.h core.h sprbss.ld wrap.S attr.c $(ROOT)/src/sh2/softfp.c $(ROOT)/src/sh2/softfp_sh2.S
