# pmath.c psincos_cr against psin_cr / pcos_cr for every float direction:  make -C tests/sincos && build/host/sincos
ROOT := $(abspath ../..)
OUT := $(ROOT)/build/host
$(OUT)/sincos: sincos.c $(ROOT)/src/game/pmath.c $(ROOT)/src/game/pmath.h
	mkdir -p $(OUT)
	$(CC) -std=c99 -O2 -Wall -Wextra -Wno-unused-function -ffp-contract=off -D_POSIX_C_SOURCE=200809L -o $@ sincos.c
