/* The content packages' defaults (pcontent.h): weak, so a package's file overrides each function it translates */
#include "play.h"
#include "pcontent.h"
#include "pint.h"

#define WEAK __attribute__((weak))
WEAK int pjungle_ev(int ev, int i, int arg) { (void)ev; (void)i; (void)arg; return 0; }
WEAK int pswamp_ev(int ev, int i, int arg) { (void)ev; (void)i; (void)arg; return 0; }
WEAK int pice_ev(int ev, int i, int arg) { (void)ev; (void)i; (void)arg; return 0; }
WEAK int ptemple_ev(int ev, int i, int arg) { (void)ev; (void)i; (void)arg; return 0; }
WEAK int pitems_ev(int ev, int i, int arg) { (void)ev; (void)i; (void)arg; return 0; }

int pcontent_ev(int ev, int i, int arg)
{
    int r = pjungle_ev(ev, i, arg) || pswamp_ev(ev, i, arg) || pice_ev(ev, i, arg) || ptemple_ev(ev, i, arg) ||
            pitems_ev(ev, i, arg);
    if (r && ev == FEV_CREATE && play_gen_inst) play_gen_created = 1;      /* prun.c play_level_start */
    return r;
}

#define ENEMY(name) WEAK int name(int site, int e, int arg) { (void)site; (void)e; (void)arg; return 0; }
ENEMY(pjungle_enemy)
ENEMY(pswamp_enemy)
ENEMY(pice_enemy)
ENEMY(ptemple_enemy)
ENEMY(pitems_enemy)

int pcontent_enemy(int site, int e, int arg)
{
    if (pjungle_enemy(site, e, arg) || pswamp_enemy(site, e, arg) || pice_enemy(site, e, arg) ||
        ptemple_enemy(site, e, arg) || pitems_enemy(site, e, arg))
        return 1;
    PUNTR(site);
    return 0;
}

WEAK int pjungle_msolid(int s) { (void)s; return -1; }
WEAK int pswamp_msolid(int s) { (void)s; return -1; }
WEAK int pice_msolid(int s) { (void)s; return -1; }
WEAK int ptemple_msolid(int s) { (void)s; return -1; }
WEAK int pitems_msolid(int s) { (void)s; return -1; }

int pcontent_msolid(int s)
{
    int v = pjungle_msolid(s);
    if (v < 0) v = pswamp_msolid(s);
    if (v < 0) v = pice_msolid(s);
    if (v < 0) v = ptemple_msolid(s);
    if (v < 0) v = pitems_msolid(s);
    return v;
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
