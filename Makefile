# Top-level entry points. Each target calls the existing per-directory Makefiles and scripts/; nothing here
# replaces them. Works with macOS make 3.81 (no grouped targets: each generator's extra outputs depend on its
# primary one).
#   make                 build/gen + build/snd, then the host builds (test/host)
#   make gen             build/gen and build/snd from refs/ (README: Build inputs)
#   make check           host gates: host builds, constcheck, routes (p5_regress), equivalence, sound calls
#   make check-mame      SH-2 gates in MAME (Docker): playsh2, softfp, shell, sndrules, sound, hud, view, zoom
#   make game-check      scripts/game_check.sh on ROUTE/SEED/TRACE/RECS (defaults below)
#   make game | mister   tests/game set for MAME / MiSTer (needs tests/game/build/g from a game-check)
#   make images          the Docker images (cps3-dev, spelunky-mame, spelunky-hd-runner)

PY      ?= python3
U       := refs/hd/linux-arm64/assets/game.unx
SRC     := refs/hd/src
GEN     := build/gen
SND     := build/snd
DMAKE   := scripts/dmake.sh

# game-check / game / mister
ROUTE   ?= p4_exit559
SEED    ?= 559
TRACE   ?= g_p4_exit559_s559
RECS    ?= 30,150,300
SNAPS   ?=

GEN_OUT := $(GEN)/objects.h $(GEN)/objects.c $(GEN)/gentables.h $(GEN)/gentables.c \
           $(GEN)/playtables.h $(GEN)/playtables.c $(GEN)/fronttables.h $(GEN)/fronttables.c \
           $(GEN)/sprites.h $(GEN)/sprites.c $(GEN)/gfx.bin $(GEN)/gfx.json \
           $(GEN)/hudart.h $(GEN)/hud.bin $(GEN)/hud.json $(GEN)/rgbsrc.json $(GEN)/fade.h $(GEN)/fade.bin \
           $(GEN)/drawtab.h $(GEN)/drawtab.c
SND_OUT := $(SND)/snd.h $(SND)/snd.bin

all: gen host

# ---- build/gen, build/snd (order follows each tool's inputs; README) ----
gen: $(GEN_OUT) $(SND_OUT)

$(GEN_OUT) $(SND_OUT): | refs-check

refs-check:
	@test -d $(SRC) || { echo "missing $(SRC): see README, Build inputs"; exit 1; }
	@test -f $(U) || { echo "missing $(U): see README, Build inputs"; exit 1; }
	@mkdir -p $(GEN) $(SND)

$(GEN)/objects.h: tools/hdobjects.py
	$(PY) tools/hdobjects.py $(SRC) $@
$(GEN)/objects.c: $(GEN)/objects.h ;

$(GEN)/gentables.h: tools/hdgentables.py $(U)
	$(PY) tools/hdgentables.py $(SRC) $(U) $@
$(GEN)/gentables.c: $(GEN)/gentables.h ;

$(GEN)/playtables.h: tools/hdplaytables.py $(U) $(GEN)/objects.h
	$(PY) tools/hdplaytables.py $(U) $@
$(GEN)/playtables.c: $(GEN)/playtables.h ;

$(GEN)/fronttables.h: tools/fronttables.py $(U)
	$(PY) tools/fronttables.py $(U) $(GEN)
$(GEN)/fronttables.c: $(GEN)/fronttables.h ;

$(GEN)/sprites.h: tools/hdsprites.py
	$(PY) tools/hdsprites.py $(SRC) $(GEN)
$(GEN)/sprites.c $(GEN)/gfx.bin $(GEN)/gfx.json: $(GEN)/sprites.h ;

$(GEN)/hudart.h: tools/hudart.py $(GEN)/gfx.json
	$(PY) tools/hudart.py $(SRC) $(GEN)
$(GEN)/hud.bin $(GEN)/hud.json: $(GEN)/hudart.h ;

$(GEN)/rgbsrc.json: tools/darkfade.py tools/hdsprites.py
	$(PY) tools/darkfade.py sources $(SRC) $(GEN)

$(GEN)/fade.h: tools/darkfade.py $(GEN)/gfx.json $(GEN)/hud.json $(GEN)/rgbsrc.json
	$(PY) tools/darkfade.py table $(GEN)
$(GEN)/fade.bin: $(GEN)/fade.h ;

$(GEN)/drawtab.h: tools/drawtables.py $(GEN)/gentables.h $(GEN)/sprites.h $(GEN)/objects.c
	$(PY) tools/drawtables.py $(SRC) $(GEN)
$(GEN)/drawtab.c: $(GEN)/drawtab.h ;

$(SND)/snd.h: tools/hdsound.py
	$(PY) tools/hdsound.py $(SRC) $(SND)
$(SND)/snd.bin: $(SND)/snd.h ;

# ---- builds ----
host: gen
	$(MAKE) -C test/host

# src/game compiled with sh-elf-gcc -m2 (in cps3-dev)
sh2: gen
	$(DMAKE) test/host sh2

constcheck: gen
	$(MAKE) -C test/host constcheck

game: gen
	$(DMAKE) tests/game ROUTE=$(ROUTE) SEED=$(SEED) SNAPS=$(SNAPS)

mister: gen
	$(DMAKE) tests/game ROUTE=$(ROUTE) SEED=$(SEED) SNAPS=$(SNAPS) mister

# ---- gates ----
check: host constcheck regress equiv snd

regress: host
	scripts/p5_regress.sh

equiv: host
	scripts/equiv_check.sh

snd: host
	scripts/snd_check.sh

game-check: gen
	scripts/game_check.sh $(ROUTE) $(SEED) $(TRACE) $(RECS)

MAME_GATES := playsh2 softfp shell sndrules sound hud view zoom gametime
check-mame: $(filter-out gametime,$(MAME_GATES))

$(MAME_GATES): gen
	scripts/$@_check.sh

# ---- Docker images ----
images:
	docker build -t cps3-dev:latest ../cps3-testgame/docker
	docker build -t spelunky-mame:latest docker/mame
	docker build -t spelunky-hd-runner docker/hd-runner

# ---- clean ----
# clean: host binaries and tests/*/build. distclean: build/gen and build/snd too (regenerating needs refs/ and the
# UndertaleModTool image). build/trace and the rest of build/ are never removed.
clean:
	rm -rf build/host tests/*/build

distclean: clean
	rm -rf $(GEN) $(SND)

.PHONY: all gen refs-check host sh2 constcheck game mister check regress equiv snd game-check check-mame \
        $(MAME_GATES) images clean distclean
