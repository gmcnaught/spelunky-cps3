/* The transition rooms between levels (rTransition1 .. 4: refs/hd/src/rooms/rTransition*), their objects
 * (oTransition, oPDummy, oSprite) and the room changes. GML: objects/<obj>/<event>.gml, line numbers in comments.
 *
 * The room's instances are built with the generator's instance code (as gen_level does): every room instance
 * exists before the first Create runs; oBricks' Create (scrSetupWalls(224), RNG for the cave-top tiles) and each
 * oBrick's Create (two rand() draws for its sprite; isLevel() is false here, so no gems: gen_not_level) draw the
 * RNG in room order; then the instances move to the play world.
 */
#include "pint.h"
#include "../snd/sndgame.h"                     /* the GML sound calls (src/snd) */
#include "pmsg.h"                                /* the HUD messages (trMessages) */
#include "pcontent.h"                            /* P7 content packages (docs/CONTENT.md) */

struct ptrans { int32_t drawLoot, drawPosX, drawPosY, moneyCount; uint8_t hurryup, isLoot, isKills; };
static struct ptrans TR;

static const struct groom *trans_room(int room)
{
    switch (room) {
    case R_rTransition1: return &groom_rTransition1;
    case R_rTransition1x: return &groom_rTransition1x;
    case R_rTransition2: return &groom_rTransition2;
    case R_rTransition2x: return &groom_rTransition2x;
    case R_rTransition3: return &groom_rTransition3;
    case R_rTransition3x: return &groom_rTransition3x;
    default: return &groom_rTransition4;
    }
}

/* objects/oTransition/Create_0.gml */
static void transition_create(int i)
{
    struct pin *p = &PX(i);
    TR.drawPosX = 100;                                                         /* :17 */
    TR.drawPosY = 83;
    TR.drawLoot = -2;
    TR.moneyCount = 0;
    TR.hurryup = 0;
    TR.isLoot = 0;
    TR.isKills = 0;
    pmsg_clear();                                                              /* :13 global.message1 / 2 = "" */
    if (PG.hasCape) pitems_world(4001, i, 0);
    if (G.currLevel - 1 < 1) PUNTR(4002);                                      /* scrClearGlobals: not reached */
    if (G.kaliPunish >= 2) pitems_world(4003, i, 0);
    PE(p)->alarm[0] = 10;                                                          /* :48 */
    PE(p)->alarm[1] = 30;
    if (PG.xdamsels > 0) pin_create(PI(176 + 8), PI(176 + 8), OBJ_oDamselKiss);   /* :52 (P5) */
    if (isRoomIs(R_rTransition1x) || isRoomIs(R_rTransition2x) || isRoomIs(R_rTransition3x)) PUNTR(4005);
    snd_stop_music();                                                          /* :76 */
}

void play_transition_start(int room)
{
    const struct groom *rm = trans_room(room);
    int k;
    inst_reset(PW.next_id);
    gntiles = 0;
    gen_untranslated = 0;
    G.roomW = rm->w;
    G.roomH = rm->h;
    gen_not_level = 1;
    for (k = 0; k < rm->n; k++)
        W.in[inst_add(rm->in[k].obj, rm->in[k].x, rm->in[k].y, rm->in[k].id)].depth = rm->in[k].depth;
    for (k = 0; k < rm->n; k++) {
        int o = rm->in[k].obj;
        if (o == OBJ_oBrick || o == OBJ_oHardBlock)
            gen_create_event(k);
        else if (o == OBJ_oBricks) {                                           /* objects/oBricks/Create_0.gml */
            scrSetupWalls(224);
            inst_destroyed(k);
        }
    }
    gen_not_level = 0;
    if (gen_untranslated) PUNTR(4006);
    pw_reset();
    PW.room = (int16_t)room;
    PW.room_w = rm->w;
    PW.room_h = rm->h;
    PW.next_id = W.next_id;
    PW.xview = PW.yview = 0;                                                   /* -room_offset = 0 at 4:3 */
    PW.vborder = 0;
    PW.vdirty = 0;
    PW.step = 0;
    PL.idx = NOONE;
    for (k = 0; k < W.n; k++) {
        const struct inst gk = W.in[k];                /* PX(i), i <= k, is the same memory (play.h) */
        const struct inst *g = &gk;
        int i;
        if (!g->alive) continue;
        i = pin_add(g->obj, PI(g->x), PI(g->y), g->id);
        pin_setspr(&PX(i), g->spr);
        pin_setdepth(&PX(i), g->depth);
        if (obj_is(g->obj, OBJ_oSolid)) PX(i).invincible = (g->flags & IF_INVINCIBLE) != 0;
    }
    pin_add(OBJ_oGamepad, 0, 0, 110219);
    {
    int n0 = PW.n;                                     /* instances the Creates make ran their Create already */
    for (k = 0; k < n0; k++) {                                                 /* the other Create events, in order */
        struct pin *p = &PX(k);
        switch (p->obj) {
        case OBJ_oTransition: transition_create(k); break;
        case OBJ_oPDummy:                                                      /* objects/oPDummy/Create_0.gml */
            PE(p)->status = 0;
            PE(p)->yVel = 0;
            if (G.isDamsel || G.isTunnelMan) PUNTR(4007);
            else pin_set_sprite(k, GSPR_sRunLeft);
            PE(p)->facing = 1;                                                     /* RIGHT = 1 here */
            break;
        case OBJ_oEntrance: case OBJ_oExit:
            PE(p)->etype = EX_EXIT;
            break;
        default:
            if ((pobj[p->obj].ev & EV_CREATE) && p->obj != OBJ_oBrick && p->obj != OBJ_oHardBlock &&
                p->obj != OBJ_oGamepad)
                PUNTR(4008);
            break;
        }
    }
    }
}

/* objects/oTransition/Alarm_0.gml: one loot (or kill) sprite every 3 steps, then drawLoot = 2 */
static void transition_alarm0(int i)
{
    int16_t *cnt = 0;
    int spr = -1, sp;
    if (TR.drawLoot == 0) {
        if (TR.drawPosX > 272) {
            TR.drawPosX = 100;
            TR.drawPosY += 2;
            if (TR.drawPosY > 83 + 4) TR.drawPosY = 83;
        }
    } else if (TR.drawPosX > 232) {
        TR.drawPosX = 96;
        TR.drawPosY += 2;
        if (TR.drawPosY > 91 + 4) TR.drawPosY = 91;
    }
    sp = pin_create(PI(TR.drawPosX), PI(TR.drawPosY), OBJ_oSprite);
    if (TR.drawLoot < 0) {
    } else {
        if (PG.gold > 0) { cnt = &PG.gold; spr = GSPR_sGoldChunk; }
        else if (PG.emeralds > 0) { cnt = &PG.emeralds; spr = GSPR_sEmerald; }
        else if (PG.sapphires > 0) { cnt = &PG.sapphires; spr = GSPR_sSapphire; }
        else if (PG.rubies > 0) { cnt = &PG.rubies; spr = GSPR_sRuby; }
        else if (PG.nuggets > 0) { cnt = &PG.nuggets; spr = GSPR_sGoldNugget; }
        else if (PG.goldbar > 0) { cnt = &PG.goldbar; spr = GSPR_sGoldBarDraw; }
        else if (PG.goldbars > 0) { cnt = &PG.goldbars; spr = GSPR_sGoldBarsDraw; }
        else if (PG.bigemeralds > 0) { cnt = &PG.bigemeralds; spr = GSPR_sEmeraldBig; }
        else if (PG.bigsapphires > 0) { cnt = &PG.bigsapphires; spr = GSPR_sSapphireBig; }
        else if (PG.bigrubies > 0) { cnt = &PG.bigrubies; spr = GSPR_sRubyBig; }
        else if (PG.diamonds > 0) { cnt = &PG.diamonds; spr = GSPR_sDiamond; }
        else if (PG.xdamsels > 0) { cnt = &PG.xdamsels; spr = GSPR_sDamselLeft; }               /* P5 */
        else if (PG.scarabs > 0) { cnt = &PG.scarabs; spr = GSPR_sScarabDisp; }
        else if (PG.idols > 0) { cnt = &PG.idols; spr = GSPR_sGoldIdolIco; }
        else if (PG.skulls > 0) { cnt = &PG.skulls; spr = GSPR_sCrystalSkullIco; }
        if (cnt) {
            pin_set_sprite(sp, spr);
            *cnt -= 1;
            TR.isLoot = 1;
        } else {
            if (TR.drawLoot == 0) {                                            /* :117 */
                TR.drawPosX = 96;
                TR.drawPosY = 91;
                pin_setx(&PX(sp), PI(96));
                pin_sety(&PX(sp), PI(91));
                TR.drawLoot = 1;
            }
            /* the kill counts (objects/oTransition/Alarm_0.gml :120-) */
            if (PG.bats > 0) { cnt = &PG.bats; spr = GSPR_sBatLeft; }
            else if (PG.snakes > 0) { cnt = &PG.snakes; spr = GSPR_sSnakeLeft; }
            else if (PG.spiders > 0) { cnt = &PG.spiders; spr = GSPR_sSpider; }
            else if (PG.deadfish > 0) { cnt = &PG.deadfish; spr = GSPR_sDeadFishLeftIco; }
            else if (PG.piranhas > 0) { cnt = &PG.piranhas; spr = GSPR_sPiranhaLeftIco; }
            else if (PG.skeletons > 0) { cnt = &PG.skeletons; spr = GSPR_sSkeletonLeft; }
            else if (PG.zombies > 0) { cnt = &PG.zombies; spr = GSPR_sZombieLeft; }
            else if (PG.vampires > 0) { cnt = &PG.vampires; spr = GSPR_sVampireLeft; }
            else if (PG.frogs > 0) { cnt = &PG.frogs; spr = GSPR_sFrogLeft; }
            else if (PG.firefrogs > 0) { cnt = &PG.firefrogs; spr = GSPR_sFireFrogLeft; }
            else if (PG.monkeys > 0) { cnt = &PG.monkeys; spr = GSPR_sMonkeyLeft; }
            else if (PG.mantraps > 0) { cnt = &PG.mantraps; spr = GSPR_sManTrapLeft; }
            else if (PG.yetis > 0) { cnt = &PG.yetis; spr = GSPR_sYetiLeft; }
            else if (PG.ufos > 0) { cnt = &PG.ufos; spr = GSPR_sUFO; }
            else if (PG.aliens > 0) { cnt = &PG.aliens; spr = GSPR_sAlien; }
            else if (PG.alienbosses > 0) { cnt = &PG.alienbosses; spr = GSPR_sAlienBossDisp; }
            else if (PG.cavemen > 0) { cnt = &PG.cavemen; spr = GSPR_sCavemanLeft; }
            else if (PG.hawkmen > 0) { cnt = &PG.hawkmen; spr = GSPR_sHawkLeft; }
            else if (PG.giantspiders > 0) { cnt = &PG.giantspiders; spr = GSPR_sGiantSpiderDisp; }
            else if (PG.megamouths > 0) { cnt = &PG.megamouths; spr = GSPR_sMegaMouth; }
            else if (PG.yetikings > 0) { cnt = &PG.yetikings; spr = GSPR_sYetiKingDisp; }
            else if (PG.tomblords > 0) { cnt = &PG.tomblords; spr = GSPR_sTombLordDisp; }
            else if (PG.damselsKilled > 0) { cnt = &PG.damselsKilled; spr = GSPR_sDamselLeftIco; }
            else if (PG.shopkeepers > 0) { cnt = &PG.shopkeepers; spr = GSPR_sShopLeftIco; }
            if (cnt) {
                pin_set_sprite(sp, spr);
                *cnt -= 1;
                TR.isKills = 1;
            } else {
                TR.drawLoot = 2;
                pin_destroy(sp);
            }
        }
    }
    if (TR.drawLoot < 0) {
    } else if (TR.drawLoot == 0)
        TR.drawPosX += 4;
    else
        TR.drawPosX += 8;
    if (TR.drawLoot == 2) {
    } else if (TR.hurryup)
        PE(&PX(i))->alarm[0] = 1;
    else
        PE(&PX(i))->alarm[0] = 3;
}

/* objects/oTransition/Step_0.gml */
static void transition_step(int i)
{
    if ((GP.pressed & K_ATTACK) || (GP.pressed & K_START)) {
        int n = 0;
        if (instance_exists_p(OBJ_oTunnelMan)) PUNTR(4011);
        if (TR.drawLoot == 2 && TR.moneyCount == PG.xmoney && n == 0) {
            GP.pressed &= (uint16_t)~(K_ATTACK | K_START);
            if (PE(&PX(i))->alarm[0] > 1) PE(&PX(i))->alarm[0] = 1;
            if (PE(&PX(i))->alarm[1] > 1) PE(&PX(i))->alarm[1] = 1;
            G.gameStart = 1;
            G.lake = 0;
            if (G.customLevel) PUNTR(4012);
            else if (G.currLevel >= 5 && G.currLevel <= 8 && !G.genBlackMarket) PUNTR(4013);   /* lake roll: P7 */
            else if (G.currLevel >= 9 && G.currLevel <= 12) play_goto_room = R_rLevel2;
            else if (G.currLevel == 16) play_goto_room = R_rOlmec;
            else play_goto_room = R_rLevel;
        } else
            TR.hurryup = 1;
    }
    /* :40-67 up / down change a tunnel man's donation: no tunnel man in rTransition1 */
    if (TR.drawLoot == 2) {                                                    /* :69 */
        if (TR.moneyCount < PG.xmoney) {
            if (TR.hurryup) TR.moneyCount = PG.xmoney;
            else {
                int32_t moneyDiff = PG.xmoney - TR.moneyCount;
                if (moneyDiff > 100) TR.moneyCount += 100;
                else TR.moneyCount += moneyDiff;
            }
        }
    }
}

/* objects/oPDummy/Step_0.gml (transition part) */
static void pdummy_step(int i)
{
    struct pin *p = &PX(i);
    pin_sety(p, PADDV(p->y, PE(p)->yVel));
    if (PE(p)->status != 99 && collision_point_p(PTOD(p->x) + 8, PTOD(p->y), OBJ_oDamselKiss, 0, NOONE) != NOONE) {   /* P5 */
        int person = instance_nearest_p(PTOD(p->x) + 8, PTOD(p->y), OBJ_oDamselKiss);
        if (!PE(&PX(person))->trigger) {                                             /* not kissed */
            PE(p)->status = 99;                                                    /* STOPPED */
            PE(p)->xVel = 0;
            PE(p)->yVel = 0;
            pin_set_sprite(i, GSPR_sStandLeft);
            pin_set_sprite(person, GSPR_sDamselKissL);
            PE(p)->alarm[5] = 30;
        }
    }
    if (instance_exists_p(OBJ_oTunnelMan)) PUNTR(4020);
    if (PE(p)->status == 0) {                                                      /* TRANSITION */
        if (PTOD(p->x) >= 280) {
            if (p->spr != GSPR_sPExit && p->spr != GSPR_sDamselExit && p->spr != GSPR_sTunnelExit) {
                snd_play(SND_xsteps);                                          /* :49 */
                pin_set_sprite(i, GSPR_sPExit);
            }
        } else
            pin_setx(p, p->x + (PI(2)));
    } else if (PE(p)->status != 99)                                                /* STOPPED: nothing */
        PUNTR(4021);
}

int ptrans_step(int i)
{
    switch (PX(i).obj) {
    case OBJ_oTransition: transition_step(i); return 1;
    case OBJ_oPDummy: pdummy_step(i); return 1;
    }
    return 0;
}

int ptrans_alarm(int i, int a)
{
    switch (PX(i).obj) {
    case OBJ_oTransition:
        if (a == 0) transition_alarm0(i);
        else if (a == 1) {                                                     /* objects/oTransition/Alarm_1.gml */
            TR.drawLoot += 1;
            if (TR.drawLoot < 0) PE(&PX(i))->alarm[1] = TR.hurryup ? 1 : 30;
        }
        return 1;
    case OBJ_oPDummy:
        if (a == 5) {                                                          /* objects/oPDummy/Alarm_5.gml */
            PE(&PX(i))->status = 0;
            pin_set_sprite(i, GSPR_sRunLeft);
        } else
            PUNTR(4022);
        return 1;
    }
    return 0;
}

int ptrans_animend(int i)
{
    if (PX(i).obj == OBJ_oPDummy) {                                            /* objects/oPDummy/Other_7.gml */
        int s = PX(i).spr;
        if (s == GSPR_sPExit || s == GSPR_sDamselExit || s == GSPR_sTunnelExit) pin_destroy(i);
        return 1;
    }
    return 0;
}

/* objects/oTransition/Draw_64.gml (Draw GUI, every drawn frame, visible instances): its one side effect,
   :86-89 global.noDarkLevel = (the seconds part of xtime <= 20; the minutes are taken off first), while
   drawLoot > -2. xtime does not change in the transition room, so once per step is the same */
void ptrans_draw_gui(void)
{
    int i;
    for (i = pw_ohead[OBJ_oTransition]; i >= 0; i = pw_inext[i]) {
        int32_t s;
        if (!PX(i).alive || !PX(i).visible || TR.drawLoot <= -2) continue;
        s = PG.xtime / 1000;                                                   /* floor: xtime >= 0 */
        while (s > 59) s -= 60;
        G.noDarkLevel = s <= 20;
    }
}

/* the transition's GUI state for the drawing (src/draw): drawLoot, moneyCount, isLoot, isKills; 0 when there is
   no oTransition */
int ptrans_gui(int32_t *v)
{
    if (pw_count(OBJ_oTransition) == 0) return 0;
    v[0] = TR.drawLoot;
    v[1] = TR.moneyCount;
    v[2] = TR.isLoot;
    v[3] = TR.isKills;
    return 1;
}

int ptrans_create(int i)
{
    if (PX(i).obj == OBJ_oSprite) {                                            /* objects/oSprite/Create_0.gml */
        PX(i).ispd = 0;
        return 1;
    }
    return 0;
}

void ptrans_draw(int i)
{
    if (PX(i).obj == OBJ_oPDummy) pin_setxscale(&PX(i), PE(&PX(i))->facing == 1 ? -1 : 1);  /* objects/oPDummy/Draw_0.gml */
}
