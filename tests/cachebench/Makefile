# tests/cachebench: jtcps3 cache costs for docs/ICACHE.md (main.c). From the repository root:
#   scripts/dmake.sh tests/cachebench            (or natively: make -C tests/cachebench, sh-elf-gcc in PATH)
#   scripts/cachebench.sh                        build, MAME run, results (MAME only checks that the rows run)
#   ../cps3-testgame/scripts/mister_run.sh tests/cachebench/build/mame cachebench "Cache bench" 3 5   (jtcps3)
PROG  := cachebench
TITLE := Cache bench
SRCS  := main.c kernels.S
include ../../../cps3-testgame/sdk/sdk.mk
