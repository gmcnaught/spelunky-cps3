# tests/softfp on the SH-2: src/sh2/softfp.c against libgcc's fp-bit (main.c); scripts/softfp_check.sh runs it in
# MAME and compares its hashes with the host test's (host.c, against the host's FPU).
#   scripts/dmake.sh tests/softfp [CASES=500000]
PROG   := softfp
TITLE  := Spelunky CPS3 soft-float test
CASES  ?= 500000
SRCS   := main.c build/softfp_sf.c build/softfp_sh2_sf.S
CFLAGS := -O2 -std=gnu99 -fno-bit-tests -I. -DSOFTFP_SFNAMES -DSOFTFP_ASM -DCASES=$(CASES) -Wno-unused-parameter
include ../../../cps3-testgame/sdk/sdk.mk

# softfp.c under its sf_* names (libgcc keeps the __* ones)
build/softfp_sf.c: ../../src/sh2/softfp.c
	@mkdir -p build
	cp $< $@
build/softfp_sh2_sf.S: ../../src/sh2/softfp_sh2.S
	@mkdir -p build
	cp $< $@
build/main.elf: cases.h check.h Makefile
