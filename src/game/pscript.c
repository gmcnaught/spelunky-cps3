/* GML scripts of the play loop (refs/hd/src/scripts/<name>/<name>.gml; line numbers in comments). */
#include "pint.h"

double prandom(double n)
{
    return (double)rng_next(&g_rng) * (1.0 / 4294967296.0) * n;
}

/* scripts/setCollisionBounds */
void setCollisionBounds(int i, int l, int t, int r, int b)
{
    struct pin *p = &PX(i);
    p->lbo = (int8_t)l;
    p->tbo = (int8_t)t;
    p->rbo = (int8_t)r;
    p->bbo = (int8_t)b;
}

/* scripts/calculateCollisionBounds */
void calcBounds(int i, double *lb, double *tb, double *rb, double *bb)
{
    const struct pin *p = &PX(i);
    *lb = PTOD(p->x) + p->lbo;
    *tb = PTOD(p->y) + p->tbo;
    *rb = PTOD(p->x) + p->rbo;
    *bb = PTOD(p->y) + p->bbo;
}

/* scripts/isCollisionLeft: collision_line(round(lb-d), round(tb), round(lb-d), round(bb-1), oSolid, 1, 1) > 0 */
int isCollisionLeft(int i, int d)
{
    double lb, tb, rb, bb;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(lb - d), dround(tb), dround(lb - d), dround(bb - 1), OBJ_oSolid, 1, i) != NOONE;
}

int isCollisionRight(int i, int d)
{
    double lb, tb, rb, bb;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(rb + d - 1), dround(tb), dround(rb + d - 1), dround(bb - 1), OBJ_oSolid, 1, i) != NOONE;
}

int isCollisionTop(int i, int d)
{
    double lb, tb, rb, bb;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(lb), dround(tb - d), dround(rb - 1), dround(tb - d), OBJ_oSolid, 1, i) != NOONE;
}

int isCollisionBottom(int i, int d)
{
    double lb, tb, rb, bb;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(lb), dround(bb + d - 1), dround(rb - 1), dround(bb + d - 1), OBJ_oSolid, 1, i) != NOONE;
}

/* scripts/isCollisionLadder */
int isCollisionLadder(int i)
{
    double lb, tb, rb, bb;
    calcBounds(i, &lb, &tb, &rb, &bb);
    if (collision_rect_p(lb + 8, tb + 8, rb - 8, bb - 8, OBJ_oLadderTop, 1, i) != NOONE)
        return 1;
    return collision_rect_p(lb + 8, tb + 8, rb - 8, bb - 8, OBJ_oLadder, 1, i) != NOONE;
}

int isCollisionPlatformBottom(int i, int d)
{
    double lb, tb, rb, bb;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(lb), dround(bb + d - 1), dround(rb - 1), dround(bb + d - 1), OBJ_oPlatform, 1, i) != NOONE;
}

int isCollisionPlatform(int i)
{
    double lb, tb, rb, bb;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_rect_p(lb, tb, rb - 1, bb - 1, OBJ_oPlatform, 1, i) != NOONE;
}

int isCollisionWaterTop(int i, int d)
{
    double lb, tb, rb, bb;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(lb), dround(tb - d), dround(rb - 1), dround(tb - d), OBJ_oWater, 1, i) != NOONE;
}

int isCollisionMoveableSolidLeft(int i, int d)
{
    double lb, tb, rb, bb;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(lb - d), dround(tb), dround(lb - d), dround(bb - 1), OBJ_oMoveableSolid, 1, i) != NOONE;
}

int isCollisionMoveableSolidRight(int i, int d)
{
    double lb, tb, rb, bb;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(rb + d - 1), dround(tb), dround(rb + d - 1), dround(bb - 1), OBJ_oMoveableSolid, 1, i) != NOONE;
}

/* scripts/getIdCollisionRight / Left: the line starts 5 px below the top */
int getIdCollisionRight(int i, int d)
{
    double lb, tb, rb, bb;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(rb + d - 1), dround(tb + 5), dround(rb + d - 1), dround(bb - 1), OBJ_oSolid, 1, i);
}

int getIdCollisionLeft(int i, int d)
{
    double lb, tb, rb, bb;
    calcBounds(i, &lb, &tb, &rb, &bb);
    return collision_line_p(dround(lb - d), dround(tb + 5), dround(lb - d), dround(bb - 1), OBJ_oSolid, 1, i);
}

/* scripts/platformCharacterIs (the player's state) */
int platformCharacterIs(int what)
{
    int s = PL.state;
    if (what == ON_GROUND && (s == RUNNING || s == STANDING || s == DUCKING || s == LOOKING_UP)) return 1;
    if (what == IN_AIR && (s == JUMPING || s == FALLING)) return 1;
    if (what == ON_LADDER && s == CLIMBING) return 1;
    return 0;
}

/* scripts/approximatelyZero: -0.1 < a < 0.1 */
int approximatelyZero(num a)
{
    return NGT(a, N(-0.1)) && NLT(a, N(0.1));
}

static int is_character(int i)
{
    return obj_is(PX(i).obj, OBJ_oCharacter);
}

/* scripts/moveTo */
void moveTo(int i, num a0, num a1, int32_t *xio, int32_t *yio)
{
    struct pin *p = &PX(i);
    pos mtXPrev = p->x, mtYPrev = p->y;
    num xVelFrac = NFRAC(NABS(a0)), yVelFrac = NFRAC(NABS(a1));            /* :20-21 */
    int32_t xVelInteger = 0, yVelInteger = 0;
    if (NNE(xVelFrac, N(0))) {                                                       /* :24 */
        int32_t r = NRECIP_ROUND(xVelFrac);
        if (r != 0) xVelInteger = (int32_t)(play_time % (uint32_t)r) == 0;
    }
    if (NNE(yVelFrac, N(0))) {
        int32_t r = NRECIP_ROUND(yVelFrac);
        if (r != 0) yVelInteger = (int32_t)(play_time % (uint32_t)r) == 0;
    }
    xVelInteger += NFLOOR(NABS(a0));                                           /* :30 */
    yVelInteger += NFLOOR(NABS(a1));
    if (a0 < 0) xVelInteger = -xVelInteger;
    if (a1 < 0) yVelInteger = -yVelInteger;
    NOPS(10);
    if (xVelInteger > 0)                                                       /* :39 */
        for (; p->x < mtXPrev + PI(xVelInteger); p->x += PI(1)) {
            int solidId = getIdCollisionRight(i, 1);
            if (solidId != NOONE) {
                if (objdefs[PX(solidId).obj].parent == OBJ_oMoveableSolid && is_character(i)) {
                    /* with solidId: `break` leaves the with, not the for */
                    if (!place_meeting_p(solidId, PTOD(PX(solidId).x) + 1, PTOD(PX(solidId).y), OBJ_oSolid))
                        PX(solidId).x += PI(1);
                } else
                    break;
            }
        }
    if (xVelInteger < 0)                                                       /* :64 */
        for (; p->x > mtXPrev + PI(xVelInteger); p->x -= PI(1)) {
            int solidId = getIdCollisionLeft(i, 1);
            if (solidId != NOONE) {
                if (objdefs[PX(solidId).obj].parent == OBJ_oMoveableSolid && is_character(i)) {
                    if (!place_meeting_p(solidId, PTOD(PX(solidId).x) - 1, PTOD(PX(solidId).y), OBJ_oSolid))
                        PX(solidId).x -= PI(1);
                } else
                    break;
            }
        }
    if (yVelInteger > 0)                                                       /* :89 */
        for (; p->y < mtYPrev + PI(yVelInteger); p->y += PI(1)) {
            if (isCollisionBottom(i, 1))
                break;
            if (is_character(i))
                if (!isCollisionPlatform(i) && isCollisionPlatformBottom(i, 1) && !PL.kDown)
                    break;
        }
    if (yVelInteger < 0)                                                       /* :98 */
        for (; p->y > mtYPrev + PI(yVelInteger); p->y -= PI(1))
            if (isCollisionTop(i, 1))
                break;
    if (xio) *xio = xVelInteger;
    if (yio) *yio = yVelInteger;
}

/* scripts/scrCreateBlood */
void scrCreateBlood(int self, pos x, pos y, int n)
{
    if (self == PL.idx && PL.bloodless)
        return;
    while (n-- > 0)
        if (instance_number_p(OBJ_oDetritus) < 16)
            pin_create(x, y, OBJ_oBlood);
}

/* scripts/scrCreateFlame */
void scrCreateFlame(pos x, pos y, int n)
{
    while (n-- > 0)
        if (instance_number_p(OBJ_oDetritus) < 16)
            pin_create(x, y, OBJ_oFlame);
}

/* scripts/scrShake (the view part is for the shake in oLevel's Step) */
void scrShake(int d)
{
    if (PG.shake < d) PG.shake = d;
    if (PG.shake < 0) PG.shake = 0;
}

int isRoomIs(int r)
{
    return PW.room == r;
}

/* scripts/isLevel, isRealLevel */
int isLevel(void)
{
    return PW.room == R_rTutorial || PW.room == R_rLoadLevel || PW.room == R_rLevel || PW.room == R_rLevel2 ||
           PW.room == R_rLevel3 || PW.room == R_rOlmec;
}

int isRealLevel(void)
{
    return PW.room == R_rLevel || PW.room == R_rLevel2 || PW.room == R_rLevel3 || PW.room == R_rOlmec;
}

/* move_snap(hs, vs): x = round(x / hs) * hs */
void move_snap(int i, int hs, int vs)
{
    struct pin *p = &PX(i);
    if (hs > 0) p->x = PI(dround(PTOD(p->x) / hs) * hs);
    if (vs > 0) p->y = PI(dround(PTOD(p->y) / vs) * vs);
}

/* x > xview - m and x < xview + 320 + m and y > yview - m and y < yview + 240 + m */
int inview(int i, int m)
{
    double x = PTOD(PX(i).x), y = PTOD(PX(i).y);
    view_read();
    return DGT(x, PW.xview - m) && DLT(x, PW.xview + 320 + m) && DGT(y, PW.yview - m) && DLT(y, PW.yview + 240 + m);
}

static const int16_t pick2t[PICK_COUNT] = { T_NONE, T_ROCK, T_JAR, T_SKULL, T_FISHBONE, T_ARROW, T_MACHETE,
    T_MATTOCK, T_MATTOCKHEAD, T_PISTOL, T_WEBCANNON, T_TELEPORTER, T_SHOTGUN, T_BOW, T_FLARE, T_SCEPTRE, T_KEY,
    T_OTHER };

int ptype_of_pickup(int pickup)
{
    return pick2t[pickup];
}

int pickup_of_ptype(int t)
{
    int k;
    for (k = 0; k < PICK_OTHER; k++)
        if (pick2t[k] == t) return k;
    return PICK_OTHER;
}

/* scripts/scrHoldItem: called by oPlayer1 */
void scrHoldItem(int t)
{
    struct pin *pl = &PX(PL.idx);
    int obj = -1;
    if (t == T_NONE) {
        PL.holdItem = NOONE;
        PL.pickupItemType = T_NONE;
        return;
    }
    switch (t) {
    case T_ROCK: obj = OBJ_oRock; break;
    case T_JAR: obj = OBJ_oJar; break;
    case T_SKULL: obj = OBJ_oSkull; break;
    case T_FISHBONE: obj = OBJ_oFishBone; break;
    case T_ARROW: obj = OBJ_oArrow; break;
    case T_MACHETE: obj = OBJ_oMachete; break;
    case T_MATTOCK: obj = OBJ_oMattock; break;
    case T_MATTOCKHEAD: obj = OBJ_oMattockHead; break;
    case T_PISTOL: obj = OBJ_oPistol; break;
    case T_WEBCANNON: obj = OBJ_oWebCannon; break;
    case T_TELEPORTER: obj = OBJ_oTeleporter; break;
    case T_SHOTGUN: obj = OBJ_oShotgun; break;
    case T_BOW: obj = OBJ_oBow; break;
    case T_FLARE: obj = OBJ_oFlare; break;
    case T_SCEPTRE: obj = OBJ_oSceptre; break;
    case T_KEY: obj = OBJ_oKey; break;
    }
    if (obj >= 0) {
        int h = pin_create(pl->x, pl->y, obj);
        PL.holdItem = h;
        PX(h).held = 1;
        PX(h).cost = 0;
        PX(h).New = 0;
        PL.pickupItemType = (int16_t)t;
        PL.whoaTimer = PL.whoaTimerMax;
    } else {
        PL.holdItem = NOONE;
        PL.pickupItemType = T_NONE;
    }
}

/* scripts/scrDropItem */
void scrDropItem(num xv, num yv)
{
    int h = PL.holdItem;
    if (h == NOONE)
        return;
    PX(h).held = 0;
    PX(h).xVel = xv;
    PX(h).yVel = yv;
    if (PL.bowArmed)
        scrFireBow();
    if (PL.pickupItemType != PX(h).type)
        scrHoldItem(PL.pickupItemType);
    else {
        PL.holdItem = NOONE;
        PL.pickupItemType = T_NONE;
    }
}

void scrFireBow(void)
{
    PUNTR(3001);                                  /* the bow: not in P4's routes */
}

/* scripts/scrStealItem */
void scrStealItem(void)
{
    int h = PL.holdItem, d;
    switch (PX(h).type) {
    case T_BOMBBAG:
        PG.bombs += 3;
        d = pin_create(PX(h).x, PX(h).y - PI(14), OBJ_oItemsGet);
        pin_set_sprite(d, GSPR_sBombsGet);
        pin_destroy(h);
        PL.holdItem = NOONE;
        break;
    case T_BOMBBOX:
        PG.bombs += 12;
        d = pin_create(PX(h).x, PX(h).y - PI(14), OBJ_oItemsGet);
        pin_set_sprite(d, GSPR_sBombsGet);
        pin_destroy(h);
        PL.holdItem = NOONE;
        break;
    case T_ROPEPILE:
        PG.rope += 3;
        d = pin_create(PX(h).x, PX(h).y - PI(15), OBJ_oItemsGet);
        pin_set_sprite(d, GSPR_sRopeGet);
        pin_destroy(h);
        PL.holdItem = NOONE;
        break;
    case T_UDJATEYE: case T_ANKH: case T_CROWN: case T_KAPALA: case T_PASTE: case T_PARACHUTE: case T_SPECTACLES:
    case T_GLOVES: case T_MITT: case T_COMPASS: case T_SPRINGSHOES: case T_SPIKESHOES: case T_JORDANS: case T_CAPE:
    case T_JETPACK:
        PUNTR(3002);                              /* the equipment pickups: P5 */
        break;
    case T_MACHETE: case T_MATTOCK: case T_PISTOL: case T_WEBCANNON: case T_TELEPORTER: case T_SHOTGUN: case T_BOW:
        if (PX(h).cost > 0) {                     /* :229-293 */
            PX(h).cost = 0;
            PX(h).forSale = 0;
        }
        break;
    case T_DAMSEL:
        if (PX(h).cost > 0) PUNTR(3003);
        break;
    default:                                      /* :304 messages only */
        break;
    }
}

/* radtodeg(arctan(a)): fdlibm's s_atan.c (no libm on the SH-2); the runner's libm may differ in the last bit, so
   image_angle (drawing only) is compared with a tolerance by tools/playcmp.py */
static const double atanhi[] = { 4.63647609000806093515e-01, 7.85398163397448278999e-01,
                                 9.82793723247329054082e-01, 1.57079632679489655800e+00 };
static const double atanlo[] = { 2.26987774529616870924e-17, 3.06161699786838301793e-17,
                                 1.39033110312309984516e-17, 6.12323399573676603587e-17 };
static const double aT[] = { 3.33333333333329318027e-01, -1.99999999998764832476e-01, 1.42857142725034663711e-01,
                             -1.11111104054623557880e-01, 9.09088713343650656196e-02, -7.69187620504482999495e-02,
                             6.66107313738753120669e-02, -5.83357013379057348645e-02, 4.97687799461593236017e-02,
                             -3.65315727442169155270e-02, 1.62858201153657823623e-02 };

static double patan(double x)
{
    double w, s1, s2, z, ax = x < 0 ? -x : x;
    int id;
    if (ax >= 4.4028204e15) return x > 0 ? atanhi[3] + atanlo[3] : -atanhi[3] - atanlo[3];
    if (ax < 0.4375) {
        if (ax < 1e-29) return x;
        id = -1;
    } else if (ax < 1.1875) {
        if (ax < 0.6875) { id = 0; ax = (2.0 * ax - 1.0) / (2.0 + ax); }
        else { id = 1; ax = (ax - 1.0) / (ax + 1.0); }
    } else if (ax < 2.4375) { id = 2; ax = (ax - 1.5) / (1.0 + 1.5 * ax); }
    else { id = 3; ax = -1.0 / ax; }
    z = ax * ax;
    w = z * z;
    s1 = z * (aT[0] + w * (aT[2] + w * (aT[4] + w * (aT[6] + w * (aT[8] + w * aT[10])))));
    s2 = w * (aT[1] + w * (aT[3] + w * (aT[5] + w * (aT[7] + w * aT[9]))));
    if (id < 0) return (x < 0 ? -1 : 1) * (ax - ax * (s1 + s2));
    z = atanhi[id] - ((ax * (s1 + s2) - atanlo[id]) - ax);
    return x < 0 ? -z : z;
}

double patan_deg(double a)
{
    return patan(a) * 180.0 / 3.14159265358979323846;
}
