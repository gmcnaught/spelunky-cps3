# Arcade shell test (src/shell, PLAN.md P8). Two parts:
#   make host        host checks of the input, credit and high-score logic (host.c)
#   scripts/dmake.sh tests/shell; scripts/shell_check.sh
#                    the shell's main loop in MAME: coins, starts and buttons pressed by scripts/lua/shellin.lua,
#                    the game hooks' step log (main.c) read back and checked by tools/shellcheck.py
PROG   := shell
TITLE  := Spelunky CPS3 shell test
SRCS   := main.c ../../src/shell/shell.c ../../src/shell/input.c ../../src/shell/credit.c ../../src/shell/hiscore.c
CFLAGS := -O2 -I../../src/shell
include ../../../cps3-testgame/sdk/sdk.mk

host:
	@mkdir -p ../../build/shell
	cc -std=c11 -Wall -Wextra -fsanitize=address,undefined -I../../src/shell host.c ../../src/shell/input.c \
	  ../../src/shell/credit.c ../../src/shell/hiscore.c -o ../../build/shell/host
	../../build/shell/host
.PHONY: host
