/* tests/playsh2's host build: the SDK's sound-voice calls (sdk/include/cps3s.h) as no-ops, for src/snd/snd.c */
#include <stdint.h>
#include "cps3s.h"

void cps3s_init(void) {}
void cps3s_voice(int v, uint32_t start, uint32_t end, uint32_t loop, int looped, uint32_t step, int vol_l, int vol_r)
{
    (void)v; (void)start; (void)end; (void)loop; (void)looped; (void)step; (void)vol_l; (void)vol_r;
}
void cps3s_volume(int v, int vol_l, int vol_r) { (void)v; (void)vol_l; (void)vol_r; }
void cps3s_step(int v, uint32_t step) { (void)v; (void)step; }
void cps3s_keys(uint16_t keys) { (void)keys; }
uint16_t cps3s_keys_now(void) { return 0; }
