# Host build of the level generator and its harness (tools/gencmp.py compares with the HD runner's dumps).
#   make -C test/host            -> build/host/genhost
#   make -C test/host sh2        -> compile check of src/game with sh-elf-gcc -m2 (if installed)
ROOT := $(abspath ../..)
GEN := $(ROOT)/build/gen
OUT := $(ROOT)/build/host
SRC := $(ROOT)/src/game/rng.c $(ROOT)/src/game/inst.c $(ROOT)/src/game/gen.c $(ROOT)/src/game/genroom.c \
       $(ROOT)/src/game/genobj.c $(GEN)/objects.c $(GEN)/gentables.c
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter
INC := -I$(ROOT)/src/game -I$(GEN)

all: $(OUT)/genhost

$(GEN)/objects.h $(GEN)/objects.c:
	cd $(ROOT) && python3 tools/hdobjects.py refs/hd/src build/gen/objects.h

$(GEN)/gentables.h $(GEN)/gentables.c:
	cd $(ROOT) && python3 tools/hdgentables.py refs/hd/src refs/hd/linux-arm64/assets/game.unx build/gen/gentables.h

$(OUT)/genhost: genhost.c $(SRC) $(ROOT)/src/game/*.h $(GEN)/objects.h $(GEN)/gentables.h
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) $(INC) -o $@ genhost.c $(SRC)

sh2: $(GEN)/objects.h $(GEN)/gentables.h
	mkdir -p $(OUT)/sh2
	for f in $(SRC); do sh-elf-gcc -m2 -std=c99 -O2 -Wall -ffreestanding $(INC) -c $$f -o $(OUT)/sh2/$$(basename $$f .c).o || exit 1; done

.PHONY: all sh2
