# tests/nc.mk: the shipping link of the game program (tests/game, tests/gametime; included after sdk.mk). The functions
# and .bss arrays named in NC (default tests/playsh2/nc_robust.txt) are linked at the SH-2's cache-through mirrors, so
# they are read past the cache (tests/playsh2/mknc.py; docs/ICACHE.md section 4.7: jtcps3 route steps -9.1 %).
# NC= (empty): the SDK's link_simm.ld as before. MAME's SH-2 recompiler does not run code at 0x26000000: an NC set
# carries <OUT>/mame/nodrc, and scripts/mame.sh then adds -nodrc.
NC     ?= $(ROOT)/tests/playsh2/nc_robust.txt
NCDIR  := $(ROOT)/tests/playsh2
ifneq ($(NC),)
CFLAGS += -ffunction-sections
$(OUT)/nc.ld: $(NC) $(NCDIR)/mknc.py
	@mkdir -p $(OUT)
	python3 $(NCDIR)/mknc.py $(NC) $@
$(OUT)/main.elf: $(OUT)/nc.ld
	@mkdir -p $(OUT)
	sh-elf-gcc $(CPS3_CFLAGS) $(CFLAGS) -I$(CPS3_SDK)/include -I$(OUT) -nostartfiles -T $(OUT)/nc.ld \
	  -Wl,-Map,$(OUT)/main.map -o $@ $(CPS3_SDK_SRCS) $(SRCS) -lgcc
	sh-elf-nm $@ > $(OUT)/nm.txt
	python3 $(NCDIR)/mknc.py --check $(NC) $(OUT)/nm.txt
endif
# SIMM 1 holds .nctext (loaded at 0x06000000, run at 0x26000000) before .text; objcopy skips it when absent
$(OUT)/mame/sfiii3na: $(OUT)/main.elf $(FLASH)
	sh-elf-objcopy -O binary -j .boot $< $(OUT)/main.bin
	sh-elf-objcopy -O binary -j .nctext -j .text -j .data $< $(OUT)/simm1.bin
	python3 $(CPS3_ROOT)/tools/mkcps3.py $(OUT)/main.bin $(OUT)/mame $(OUT)/simm1.bin $(FLASH)
	$(if $(NC),touch,rm -f) $(OUT)/mame/nodrc
