# src/sh2/fma.c against the host libm's fmaf:  make -C tests/fma && build/host/fmatest [millions per set]
ROOT := $(abspath ../..)
OUT := $(ROOT)/build/host
$(OUT)/fmatest: fmatest.c $(ROOT)/src/sh2/fma.c
	mkdir -p $(OUT)
	$(CC) -std=c99 -O2 -Wall -Wextra -ffp-contract=off -DFMAF_NAME=sh2_fmaf -o $@ fmatest.c $(ROOT)/src/sh2/fma.c -lm
