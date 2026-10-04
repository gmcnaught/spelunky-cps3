/* tests/playsh2 FPCHECK builds: softfp.c's C versions under sf_* names (the reference fpcheck.c compares with) */
#undef SOFTFP_ASM
#define SOFTFP_SFNAMES 1
#include "../../src/sh2/softfp.c"
