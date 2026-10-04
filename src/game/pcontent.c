/* The content packages' defaults (pcontent.h): weak, so a package's file overrides each function it translates */
#include "play.h"
#include "pcontent.h"

#define WEAK __attribute__((weak))
WEAK int pjungle_ev(int ev, int i, int arg) { (void)ev; (void)i; (void)arg; return 0; }
WEAK int pswamp_ev(int ev, int i, int arg) { (void)ev; (void)i; (void)arg; return 0; }
WEAK int pice_ev(int ev, int i, int arg) { (void)ev; (void)i; (void)arg; return 0; }
WEAK int ptemple_ev(int ev, int i, int arg) { (void)ev; (void)i; (void)arg; return 0; }
WEAK int pitems_ev(int ev, int i, int arg) { (void)ev; (void)i; (void)arg; return 0; }

int pcontent_ev(int ev, int i, int arg)
{
    return pjungle_ev(ev, i, arg) || pswamp_ev(ev, i, arg) || pice_ev(ev, i, arg) || ptemple_ev(ev, i, arg) ||
           pitems_ev(ev, i, arg);
}

#define SITE(name) WEAK int name(int site, int i, int arg) { (void)i; (void)arg; PUNTR(site); return 0; }
SITE(pjungle_player)
SITE(pjungle_world)
SITE(pswamp_player)
SITE(pswamp_world)
SITE(pice_player)
SITE(ptemple_player)
SITE(ptemple_world)
SITE(pitems_player)
SITE(pitems_world)
