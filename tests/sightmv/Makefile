# penemy.c sight_fast's identities:  make -C tests/sightmv && build/host/sightmv
ROOT := $(abspath ../..)
OUT := $(ROOT)/build/host
$(OUT)/sightmv: sightmv.c
	mkdir -p $(OUT)
	$(CC) -std=c99 -O2 -Wall -Wextra -ffp-contract=off -o $@ sightmv.c
