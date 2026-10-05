# src/game/pcmpc.h's bit compares against pnum.h's NLT / NGT:  make -C tests/cmpc && build/host/cmpc [millions]
ROOT := $(abspath ../..)
OUT := $(ROOT)/build/host
$(OUT)/cmpc: cmpc.c $(ROOT)/src/game/pcmpc.h $(ROOT)/src/game/pnum.h
	mkdir -p $(OUT)
	$(CC) -std=c99 -O2 -Wall -Wextra -ffp-contract=off -I$(ROOT)/src/game -I$(ROOT)/build/gen -I$(ROOT)/build/snd \
	  -I$(ROOT)/../cps3-testgame/sdk/include -o $@ cmpc.c
