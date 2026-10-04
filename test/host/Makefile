# Host build of the level generator and its harness (tools/gencmp.py compares with the HD runner's dumps).
#   make -C test/host            -> build/host/genhost
#   make -C test/host sh2        -> compile check of src/game (and src/sh2/fma.c, pcol.c's fmaf: the SH-2 has no
#                                   libm) with sh-elf-gcc -m2 (if installed); -fno-bit-tests:
#                                   GCC turns large switches into variable shifts (libgcc ___ashlsi3) otherwise
ROOT := $(abspath ../..)
GEN := $(ROOT)/build/gen
OUT := $(ROOT)/build/host
SRC := $(ROOT)/src/game/rng.c $(ROOT)/src/game/inst.c $(ROOT)/src/game/gen.c $(ROOT)/src/game/genroom.c \
       $(ROOT)/src/game/genobj.c $(ROOT)/src/game/genent.c $(GEN)/objects.c $(GEN)/gentables.c
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter
INC := -I$(ROOT)/src/game -I$(GEN)

PSRC := $(ROOT)/src/game/pworld.c $(ROOT)/src/game/pscript.c $(ROOT)/src/game/pobj.c $(ROOT)/src/game/pplayer.c \
        $(ROOT)/src/game/prun.c $(ROOT)/src/game/pcol.c $(ROOT)/src/game/ptrans.c $(GEN)/playtables.c \
        $(ROOT)/src/game/penemy.c $(ROOT)/src/game/pdamsel.c $(ROOT)/src/game/pshop.c $(ROOT)/src/game/pmath.c \
        $(ROOT)/src/game/pitem.c

all: $(OUT)/genhost $(OUT)/playhost $(OUT)/playhost_fx $(OUT)/playhost_nc

$(GEN)/objects.h $(GEN)/objects.c:
	cd $(ROOT) && python3 tools/hdobjects.py refs/hd/src build/gen/objects.h

$(GEN)/gentables.h $(GEN)/gentables.c:
	cd $(ROOT) && python3 tools/hdgentables.py refs/hd/src refs/hd/linux-arm64/assets/game.unx build/gen/gentables.h

$(OUT)/genhost: genhost.c $(SRC) $(ROOT)/src/game/*.h $(GEN)/objects.h $(GEN)/gentables.h
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) $(INC) -o $@ genhost.c $(SRC)

$(GEN)/playtables.h $(GEN)/playtables.c: $(GEN)/objects.h
	cd $(ROOT) && python3 tools/hdplaytables.py refs/hd/linux-arm64/assets/game.unx build/gen/playtables.h

# the play loop: binary64 (default) and s7.24 / s13.18 fixed point (-DPLAY_FIXED)
$(OUT)/playhost: playhost.c $(SRC) $(PSRC) $(ROOT)/src/game/*.h $(GEN)/playtables.h
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) -DPLAY_STATS $(INC) -o $@ playhost.c $(SRC) $(PSRC)

$(OUT)/playhost_fx: playhost.c $(SRC) $(PSRC) $(ROOT)/src/game/*.h $(GEN)/playtables.h
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) -DPLAY_FIXED $(INC) -o $@ playhost.c $(SRC) $(PSRC)

# binary64 with no floating-point contraction anywhere (clang on arm64 contracts a * b + c by default; the runner's
# fused operations are explicit fmaf calls in pcol.c): the routes must give the same records as playhost
#   PLAYHOST=build/host/playhost_nc scripts/p4_trace.sh <route> <seed> --no-run
$(OUT)/playhost_nc: playhost.c $(SRC) $(PSRC) $(ROOT)/src/game/*.h $(GEN)/playtables.h
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) -DPLAY_STATS -ffp-contract=off $(INC) -o $@ playhost.c $(SRC) $(PSRC)

# the play code with struct pin's collision fields const (play.h PIN_CONST_CHECK): a write that bypasses the
# pin_set* setters (and so the collision tree's dirty marks) is a compile error. Compile only (no binary)
constcheck: $(GEN)/objects.h $(GEN)/gentables.h $(GEN)/playtables.h
	for f in $(PSRC) playhost.c treeprobe.c; do $(CC) -std=c99 -fsyntax-only -Wall -Wno-unused-parameter -DPIN_CONST_CHECK $(INC) $$f || exit 1; done

# binary64 with its operations counted (the num class of pnum.h: -DPLAY_COUNT, everything compiled as C++)
$(OUT)/playhost_count: playhost.c $(SRC) $(PSRC) $(ROOT)/src/game/*.h $(GEN)/playtables.h
	mkdir -p $(OUT)
	$(CXX) -x c++ -std=c++17 -O2 -w -DPLAY_COUNT $(INC) -o $@ playhost.c $(SRC) $(PSRC)

# the play loop in both arithmetic builds (binary64: GCC's soft-float; PLAY_FIXED: integers only)
sh2: $(GEN)/objects.h $(GEN)/gentables.h $(GEN)/playtables.h
	mkdir -p $(OUT)/sh2 $(OUT)/sh2fx
	for f in $(SRC) $(PSRC); do sh-elf-gcc -m2 -std=c99 -O2 -Wall -ffreestanding -fno-bit-tests -DPLAY_FIXED $(INC) -c $$f -o $(OUT)/sh2fx/$$(basename $$f .c).o || exit 1; done
	for f in $(SRC) $(PSRC); do sh-elf-gcc -m2 -std=c99 -O2 -Wall -ffreestanding -fno-bit-tests $(INC) -c $$f -o $(OUT)/sh2/$$(basename $$f .c).o || exit 1; done
	sh-elf-gcc -m2 -std=c99 -O2 -Wall -Wextra -ffreestanding -fno-bit-tests -c $(ROOT)/src/sh2/fma.c -o $(OUT)/sh2/fma.o

.PHONY: all sh2 constcheck
