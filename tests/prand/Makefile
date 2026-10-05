# src/game/prand.h's prand_scale against (double)u * (1.0 / 4294967296.0) * n:  make -C tests/prand && build/host/prand
ROOT := $(abspath ../..)
OUT := $(ROOT)/build/host
$(OUT)/prand: prand.c $(ROOT)/src/game/prand.h
	mkdir -p $(OUT)
	$(CC) -std=c99 -O2 -Wall -Wextra -ffp-contract=off -I$(ROOT)/src/game -o $@ prand.c
