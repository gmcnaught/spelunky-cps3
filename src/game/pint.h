/* Play loop internals shared by pscript.c, pobj.c, pplayer.c, prun.c. */
#ifndef PINT_H
#define PINT_H
#include "play.h"
#include "gen.h"
#include "rng.h"

/* the oPlayer1 instance's variables (one player: kept outside struct pin) */
struct player {
    int idx;                                  /* its instance (NOONE if none) */
    uint8_t kLeft, kLeftPressed, kLeftReleased, kRight, kRightPressed, kRightReleased, kUp, kDown;
    uint8_t kRun, kJump, kJumpPressed, kJumpReleased, kAttack, kAttackPressed, kAttackReleased, kItemPressed;
    uint8_t kBombPressed, kRopePressed, kPayPressed;
    int32_t kLeftPushedSteps, kRightPushedSteps, cantJump;
    pos xPrev, yPrev;
    uint8_t colSolidLeft, colSolidRight, colLeft, colRight, colTop, colBot, colLadder, colPlatBot, colPlat;
    uint8_t colWaterTop, colIceBot, colPointLadder, colSpikes, runKey, canRun;
    int32_t runHeld;
    int state, statePrev, statePrevPrev, facing, looking;
    num xFric, yFric, xVelLimit, yVelLimit, xAccLimit, yAccLimit, runAcc, initialJumpAcc, climbAcc;
    num departLadderXVel, departLadderYVel, gravNorm, gravityIntensity;
    num frictionRunningX, frictionRunningFastX, frictionClimbingX, frictionClimbingY, runAnimSpeed, climbAnimSpeed;
    int32_t jumpTimeTotal, jumpTime, jumpButtonReleased, ladderTimer, jumps, hangCount, hangCountMax;
    int32_t maxSlope, maxDownSlope, pushTimer, fallTimer, stunTimer, wallHurt, whoaTimer, whoaTimerMax;
    int32_t bubbleTimer, bubbleTimerMax, firing, burning, redColor, viewCount, deadCounter, jetpackFuel;
    int32_t blink, blinkToggle, invincible, holdArrow, bombArrowCounter, pExit, firstLevelSkip, levelSkip;
    uint8_t kJumped, whipping, dead, stunned, active, bounced, swimming, redToggle, bowArmed, holdArrowToggle;
    uint8_t climbSndToggle, walkSndToggle, bloodless;
    num bowStrength;
    int ladder;                               /* instance index or NOONE */
    int holdItem;                             /* instance index or NOONE (GML 0) */
    int16_t pickupItemType;                   /* enum ptype (T_NONE = "") */
    int32_t money;
    int32_t bet;                              /* the dice house's bet (P5) */
};
extern struct player PL;

/* GML constants (characterCreateEvent) */
enum { STANDING = 10, RUNNING = 11, DUCKING = 12, LOOKING_UP = 13, CLIMBING = 14, JUMPING = 15, FALLING = 16,
       DYING = 17, LEFT = 18, RIGHT = 19, ON_GROUND = 20, IN_AIR = 21, ON_LADDER = 22, HANGING = 23,
       DUCKTOHANG = 24, UP = 101, DOWN = 102 };
enum { ARROW_NORM = 1, ARROW_BOMB = 2 };

/* scripts (pscript.c) */
double prandom(double n);                     /* random(n): u * 2^-32 * n */
void setCollisionBounds(int i, int l, int t, int r, int b);
void calcBounds(int i, double *lb, double *tb, double *rb, double *bb);
int isCollisionLeft(int i, int d);
int isCollisionRight(int i, int d);
int isCollisionTop(int i, int d);
int isCollisionBottom(int i, int d);
int isCollisionLadder(int i);
int isCollisionPlatformBottom(int i, int d);
int isCollisionPlatform(int i);
int isCollisionWaterTop(int i, int d);
int isCollisionMoveableSolidLeft(int i, int d);
int isCollisionMoveableSolidRight(int i, int d);
int getIdCollisionRight(int i, int d);
int getIdCollisionLeft(int i, int d);
int platformCharacterIs(int what);
int approximatelyZero(num a);
/* moveTo(xv, yv) for instance i; xVelInteger / yVelInteger are left in *xi, *yi (may be 0) */
void moveTo(int i, num xv, num yv, int32_t *xi, int32_t *yi);
void scrCreateBlood(int self, pos x, pos y, int n);
void scrCreateFlame(pos x, pos y, int n);
void scrShake(int d);
void scrHoldItem(int t);
void scrDropItem(num xv, num yv);
void scrStealItem(void);
void scrFireBow(void);
int isLevel(void);
int isRealLevel(void);
int isRoomIs(int r);
void move_snap(int i, int hs, int vs);
int ptype_of_pickup(int pickup);              /* gen's enum pickup -> enum ptype */
int pickup_of_ptype(int t);
int inview(int i, int m);
int create_detritus(int i);                   /* pobj.c: oDetritus Create / Step (oBone, P5) */
void detritus_step(int i);
void create_item(struct pin *p);              /* pobj.c: oItem Create / Step (oDamsel's inherited, P5) */
void item_step(int i);                     /* x, y inside the view +/- m (the GML's view checks) */

/* room indices (names file R lines) */
enum { R_rTitle = 4, R_rHighscores = 5, R_rSun = 6, R_rMoon = 7, R_rStars = 8, R_rTutorial = 9, R_rLevelEditor = 10,
       R_rLoadLevel = 11, R_rLevel = 12, R_rLevel2 = 13, R_rLevel3 = 14, R_rOlmec = 15, R_rTransition1 = 16,
       R_rTransition1x = 17, R_rTransition2 = 18, R_rTransition2x = 19, R_rTransition3 = 20, R_rTransition3x = 21,
       R_rTransition4 = 22 };
extern int play_goto_room;                    /* room_goto() target (-1 none), taken at the end of the step */

/* oGame / oLevel instance variables */
struct pgame { int32_t drawStatus, moneyCount; uint8_t paused; };
struct plevel { uint8_t musicFade; double darkness; };
extern struct pgame PGAME;
extern struct plevel PLEV;

/* the transition rooms (ptrans.c): 1 if the instance's event was theirs */
void play_transition_start(int room);
int ptrans_step(int i);
int ptrans_alarm(int i, int a);
int ptrans_animend(int i);
int ptrans_create(int i);
void ptrans_draw(int i);
void ptrans_draw_gui(void);                       /* oTransition Draw GUI: global.noDarkLevel */
int ptrans_gui(int32_t *v);                       /* drawLoot, moneyCount, isLoot, isKills (src/draw); 0: none */

/* events of the player (pplayer.c) */
void pl_step(int i);
void pl_end_step(int i);
void pl_alarm(int i, int a);
void pl_animend(int i);
void pl_collision(int i, int other);
void pl_draw(int i);

#endif
