# characterStepEvent's slope-distance skip (src/game/pplayer.c):  make -C tests/slopedist && build/host/slopedist [millions]
ROOT := $(abspath ../..)
OUT := $(ROOT)/build/host
$(OUT)/slopedist: slopedist.c $(ROOT)/src/game/pnum.h
	mkdir -p $(OUT)
	$(CC) -std=c99 -O2 -Wall -Wextra -ffp-contract=off -I$(ROOT)/src/game -I$(ROOT)/build/gen -I$(ROOT)/build/snd \
	  -I$(ROOT)/../cps3-testgame/sdk/include -o $@ slopedist.c
