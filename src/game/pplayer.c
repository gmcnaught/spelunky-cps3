/* oPlayer1: its events (refs/hd/src/objects/oPlayer1/<event>.gml) and the scripts they call (characterStepEvent,
 * characterSprite, characterDrawEvent; scripts/<name>/<name>.gml), translated statement for statement.
 * Line numbers: Step_0.gml unless named. GML read as GameMaker 2024.14 runs it: `and` / `or` short-circuit,
 * function arguments evaluated last to first, `other` outside a with / collision is self.
 * Untranslated GML that P4's routes do not reach (items and rooms of later milestones) sets play_untranslated.
 */
#include "pint.h"
#include "pcol.h"                                /* pcol_touch (the light search) */
#include "penemy.h"
#include "../snd/sndgame.h"                     /* the GML sound calls (src/snd) */
#include "pmsg.h"                                /* the HUD messages (trMessages) */
#include "pcontent.h"                            /* P7 content packages (docs/CONTENT.md) */

struct player PL;

#define GPd(k) ((GP.down & (k)) != 0)
#define GPp(k) ((GP.pressed & (k)) != 0)
#define GPr(k) ((GP.released & (k)) != 0)

static int spr_is_attack(int s) { return s == GSPR_sAttackLeft || s == GSPR_sDamselAttackL || s == GSPR_sTunnelAttackL; }
static int spr_is_exit(int s) { return s == GSPR_sPExit || s == GSPR_sDamselExit || s == GSPR_sTunnelExit; }

/* objects/oPlayer1/Create_0.gml with characterCreateEvent (for the room's oPlayer1) */
void pl_init_from_gen(int i)
{
    struct pin *p = &PX(i);
    PL.idx = i;
    PL.bet = 0;                                                                /* oPlayer1 Create :38 (P5) */
    PL.distToNearestLightSource = 999;                                         /* :31 */
    pmsg_player_reset();                                                       /* :103 */
    /* characterCreateEvent :7-117 */
    PL.hangCount = 0;
    PL.runHeld = 0;
    PL.blink = 0;
    PL.blinkToggle = -1;
    PL.invincible = 0;
    PL.swimming = 0;
    PL.kLeft = PL.kRight = PL.kUp = PL.kDown = PL.kJump = PL.kJumpPressed = PL.kRun = 0;
    PL.kAttack = PL.kAttackPressed = 0;
    PL.state = FALLING;
    PL.facing = RIGHT;
    PE(p)->grav = N(1);
    PL.gravNorm = N(1);
    PL.xVelLimit = N(16);
    PL.yVelLimit = N(10);
    PL.xAccLimit = N(9);
    PL.yAccLimit = N(6);
    PL.runAcc = N(3);
    PL.initialJumpAcc = N(-2);
    PL.jumpTimeTotal = 10;
    PL.climbAcc = N(0.6);
    PL.climbAnimSpeed = N(0.4);
    PL.departLadderXVel = N(4);
    PL.departLadderYVel = N(-4);
    PL.maxSlope = 4;
    PL.maxDownSlope = 5;
    PL.canRun = 1;
    PL.frictionRunningX = N(0.6);
    PL.frictionRunningFastX = N(0.98);
    PL.frictionClimbingX = N(0.6);
    PL.frictionClimbingY = N(0.6);
    PL.runAnimSpeed = N(0.1);
    setCollisionBounds(i, -5, -8, 5, 8);
    PL.statePrev = PL.state;
    PL.statePrevPrev = PL.statePrev;
    PL.gravityIntensity = PE(p)->grav;
    PL.jumpTime = PL.jumpTimeTotal;
    PL.jumpButtonReleased = 0;
    PL.ladderTimer = 0;
    PL.jumps = 0;
    PL.kLeftPushedSteps = PL.kRightPushedSteps = 0;
    PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;                                  /* makeActive */
    /* Create_0.gml :5-104 */
    PL.firstLevelSkip = 1;
    PL.levelSkip = 1;
    if (G.isDamsel) pin_set_sprite(i, GSPR_sDamselLeft);
    else if (G.isTunnelMan) pin_set_sprite(i, GSPR_sTunnelLeft);
    PL.active = 1;
    PL.dead = 0;
    PL.deadCounter = 100;
    PL.stunned = 0;
    PL.bounced = 0;
    PE(p)->myGrav = N(0.6);
    PL.fallTimer = 0;
    PL.stunTimer = 0;
    PL.wallHurt = 0;
    PL.pushTimer = 0;
    PL.whoaTimer = 0;
    PL.whoaTimerMax = 30;
    PL.bubbleTimer = 0;
    PL.bubbleTimerMax = 20;
    PL.climbSndToggle = PL.walkSndToggle = 0;
    PL.kAttack = 1;
    PL.kAttackPressed = 0;
    PL.whipping = 0;
    PL.cantJump = 0;
    PL.kJumped = 0;
    PL.burning = 0;
    PL.firing = 0;
    PL.bowArmed = 0;
    PL.bowStrength = 0;
    PL.jetpackFuel = 0;
    PL.bloodless = 0;
    PL.redColor = 0;
    PL.redToggle = 0;
    PL.kAttackReleased = 0;
    PL.holdItem = NOONE;
    PL.pickupItemType = T_NONE;
    PL.kItemPressed = 0;
    PL.holdArrow = 0;
    PL.holdArrowToggle = 0;
    PL.bombArrowCounter = 80;
    PL.viewCount = 0;
    PL.pExit = 0;
    if (isRoomIs(R_rOlmec)) PL.active = 0;
    PL.ladder = NOONE;
    PL.xFric = PL.yFric = N(1);
    PL.looking = 0;
}

/* scripts/characterSprite */
static void characterSprite(int i)
{
    struct pin *p = &PX(i);
    if (G.isTunnelMan || G.isDamsel) {
        PUNTR(2001);
        return;
    }
    if (!PL.stunned && !PL.whipping) {
        if (PL.state == STANDING) {
            if (!collision_point_any(PTOD(p->x) - 2, PTOD(p->y) + 9, OBJ_oSolid, 0, NOONE)) {
                pin_setispd(p, (img_t)0.6);
                pin_set_sprite(i, GSPR_sWhoaLeft);
            } else
                pin_set_sprite(i, GSPR_sStandLeft);
        }
        if (PL.state == RUNNING) {
            if (PL.kUp) pin_set_sprite(i, GSPR_sLookRunL);
            else pin_set_sprite(i, GSPR_sRunLeft);
        }
        if (PL.state == DUCKING) {
            if (NEQ(PE(p)->xVel, N(0))) pin_set_sprite(i, GSPR_sDuckLeft);
            else if (NLT(NABS(PE(p)->xVel), N(3))) pin_set_sprite(i, GSPR_sCrawlLeft);
            else pin_set_sprite(i, GSPR_sRunLeft);
        }
        if (PL.state == LOOKING_UP) {
            if (NGT(NABS(PE(p)->xVel), N(0))) pin_set_sprite(i, GSPR_sLookRunL);
            else pin_set_sprite(i, GSPR_sLookLeft);
        }
        if (PL.state == JUMPING)
            pin_set_sprite(i, GSPR_sJumpLeft);
        if (PL.state == FALLING && PL.statePrev == FALLING && PL.statePrevPrev == FALLING)
            pin_set_sprite(i, GSPR_sFallLeft);
        if (PL.state == HANGING)
            pin_set_sprite(i, GSPR_sHangLeft);
        if (PL.pushTimer > 20)
            pin_set_sprite(i, GSPR_sPushLeft);
        if (PL.state == CLIMBING) {
            if (collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oRope, 0, NOONE) != NOONE) {
                if (PL.kDown) pin_set_sprite(i, GSPR_sClimbUp3);
                else pin_set_sprite(i, GSPR_sClimbUp2);
            } else
                pin_set_sprite(i, GSPR_sClimbUp);
        }
        if (PL.state == DUCKTOHANG)
            pin_set_sprite(i, GSPR_sDuckToHangL);
    }
}

static void set_hang(struct pin *p, int i)
{
    PL.state = HANGING;
    move_snap(i, 1, 8);
    PE(p)->yVel = 0;
    PE(p)->yAcc = 0;
    PE(p)->grav = 0;
}

/* Not in HD: a push block (oMoveableSolid) moves 1 px per push (moveTo.gml :58 / :86), so one pushed off the grid
   under a solid shows a sliver of its top and the hang test's (x +- 9, y - 9) point misses the solid above. The
   ledge grab is refused when a solid covers the top of the push block at (px, py) or (px, py - 1). */
static int pushblock_covered(double px, double py)
{
    int b = collision_point_p(px, py, OBJ_oMoveableSolid, 0, NOONE);
    if (b == NOONE) b = collision_point_p(px, py - 1, OBJ_oMoveableSolid, 0, NOONE);
    if (b == NOONE) return 0;
    return collision_rect_any(PTOD(PX(b).x) + 1, PTOD(PX(b).y) - 3, PTOD(PX(b).x) + 14, PTOD(PX(b).y) - 1,
                              OBJ_oSolid, 0, NOONE);
}

/* scripts/characterStepEvent */
static void characterStepEvent(int i)
{
    struct pin *p = &PX(i);
    double x, y;
    int32_t xVelInteger = 0, yVelInteger = 0;
    PL.hangCountMax = 3;                                                       /* :7 */
    PL.kLeft = GPd(K_LEFT);                                                    /* :13 */
    if (PL.kLeft) PL.kLeftPushedSteps += 1;
    else PL.kLeftPushedSteps = 0;
    PL.kLeftPressed = GPp(K_LEFT);
    PL.kLeftReleased = GPr(K_LEFT);
    PL.kRight = GPd(K_RIGHT);
    if (PL.kRight) PL.kRightPushedSteps += 1;
    else PL.kRightPushedSteps = 0;
    PL.kRightPressed = GPp(K_RIGHT);
    PL.kRightReleased = GPr(K_RIGHT);
    PL.kUp = GPd(K_UP);
    PL.kDown = GPd(K_DOWN);
    PL.kRun = 0;                                                               /* :33 */
    PL.kJump = GPd(K_JUMP);
    PL.kJumpPressed = GPp(K_JUMP);
    PL.kJumpReleased = GPr(K_JUMP);
    if (PL.cantJump > 0) {                                                     /* :45 */
        PL.kJump = PL.kJumpPressed = PL.kJumpReleased = 0;
        PL.cantJump -= 1;
    } else if (G.isTunnelMan && p->spr == GSPR_sTunnelAttackL && PL.holdItem == NOONE) {
        PL.kJump = PL.kJumpPressed = PL.kJumpReleased = 0;
        PL.cantJump -= 1;
    }
    PL.kAttack = GPd(K_ATTACK);                                                /* :65 */
    PL.kAttackPressed = GPp(K_ATTACK);
    PL.kAttackReleased = GPr(K_ATTACK);
    PL.kItemPressed = GPp(K_ITEM);
    PL.xPrev = p->x;                                                           /* :71 */
    PL.yPrev = p->y;
    if (PL.stunned || PL.dead) {                                               /* :74 */
        PL.kLeft = PL.kLeftPressed = PL.kLeftReleased = 0;
        PL.kRight = PL.kRightPressed = PL.kRightReleased = 0;
        PL.kUp = PL.kDown = 0;
        PL.kJump = PL.kJumpPressed = PL.kJumpReleased = 0;
        PL.kAttack = PL.kAttackPressed = PL.kAttackReleased = 0;
        PL.kItemPressed = 0;
    }
    /* Collisions :97 */
    PL.colSolidLeft = PL.colSolidRight = PL.colLeft = PL.colRight = PL.colTop = PL.colBot = 0;
    PL.colLadder = PL.colPlatBot = PL.colPlat = PL.colWaterTop = PL.colIceBot = 0;
    PL.runKey = 0;
    if (isCollisionMoveableSolidLeft(i, 1)) PL.colSolidLeft = 1;
    if (isCollisionMoveableSolidRight(i, 1)) PL.colSolidRight = 1;
    if (isCollisionLeft(i, 1)) PL.colLeft = 1;
    if (isCollisionRight(i, 1)) PL.colRight = 1;
    if (isCollisionTop(i, 1)) PL.colTop = 1;
    if (isCollisionBottom(i, 1)) PL.colBot = 1;
    if (isCollisionLadder(i)) PL.colLadder = 1;
    if (isCollisionPlatformBottom(i, 1)) PL.colPlatBot = 1;
    if (isCollisionPlatform(i)) PL.colPlat = 1;
    if (isCollisionWaterTop(i, 1)) PL.colWaterTop = 1;
    if (collision_point_p(PTOD(p->x), PTOD(p->y) + 8, OBJ_oIce, 0, NOONE) != NOONE) PL.colIceBot = 1;
    if (play_toggle_run_on ? play_toggle_run : GPd(K_RUN)) {                  /* :120 checkRun() */
        PL.runHeld = 100;
        PL.runKey = 1;
    }
    if (GPd(K_ATTACK) && !PL.whipping) {                                       /* :126 */
        PL.runHeld += 1;
        PL.runKey = 1;
    }
    if (!PL.runKey || (!PL.kLeft && !PL.kRight)) PL.runHeld = 0;              /* Linux build's :132-139 */

    if (PL.state != CLIMBING && PL.state != HANGING) {                         /* :143 */
        if (PL.kLeftReleased && approximatelyZero(PE(p)->xVel)) PE(p)->xAcc -= N(0.5);
        if (PL.kRightReleased && approximatelyZero(PE(p)->xVel)) PE(p)->xAcc += N(0.5);
        if (PL.kLeft && !PL.kRight) {
            if (PL.colSolidLeft) {
                if (platformCharacterIs(ON_GROUND) && PL.state != DUCKING) {
                    PE(p)->xAcc -= N(1);
                    PL.pushTimer += 10;
                }
            } else if (PL.kLeftPushedSteps > 2 && (PL.facing == LEFT || approximatelyZero(PE(p)->xVel)))
                PE(p)->xAcc -= PL.runAcc;
            PL.facing = LEFT;
        }
        if (PL.kRight && !PL.kLeft) {
            if (PL.colSolidRight) {
                if (platformCharacterIs(ON_GROUND) && PL.state != DUCKING) {
                    PE(p)->xAcc += N(1);
                    PL.pushTimer += 10;
                }
            } else if ((PL.kRightPushedSteps > 2 || PL.colSolidLeft) && (PL.facing == RIGHT || approximatelyZero(PE(p)->xVel)))
                PE(p)->xAcc += PL.runAcc;
            PL.facing = RIGHT;
        }
        NOPS(4);
    }

    /* LADDERS :195 */
    if (PL.state == CLIMBING) {
        if (instance_exists_p(OBJ_oCape)) pswamp_player(2002, i, 0);
        PL.kJumped = 0;
        PL.ladderTimer = 10;
        PL.ladder = collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oLadder, 0, NOONE);
        if (PL.ladder != NOONE) pin_setx(p, PX(PL.ladder).x + PI(8));
        if (PL.kLeft) PL.facing = LEFT;
        else if (PL.kRight) PL.facing = RIGHT;
        if (PL.kUp) {
            if (collision_point_p(PTOD(p->x), PTOD(p->y) - 8, OBJ_oLadder, 0, NOONE) != NOONE ||
                collision_point_p(PTOD(p->x), PTOD(p->y) - 8, OBJ_oLadderTop, 0, NOONE) != NOONE) {
                PE(p)->yAcc -= PL.climbAcc;
                if (PE(p)->alarm[2] < 1) PE(p)->alarm[2] = 8;
            }
        } else if (PL.kDown) {
            if (collision_point_p(PTOD(p->x), PTOD(p->y) + 8, OBJ_oLadder, 0, NOONE) != NOONE ||
                collision_point_p(PTOD(p->x), PTOD(p->y) + 8, OBJ_oLadderTop, 0, NOONE) != NOONE) {
                PE(p)->yAcc += PL.climbAcc;
                if (PE(p)->alarm[2] < 1) PE(p)->alarm[2] = 8;
            } else
                PL.state = FALLING;
            if (PL.colBot) PL.state = STANDING;
        }
        if (PL.kJumpPressed && !PL.whipping) {
            if (PL.kLeft) PE(p)->xVel = -PL.departLadderXVel;
            else if (PL.kRight) PE(p)->xVel = PL.departLadderXVel;
            else PE(p)->xVel = 0;
            PE(p)->yAcc += PL.departLadderYVel;
            PL.state = JUMPING;
            PL.jumpButtonReleased = 0;
            PL.jumpTime = 0;
            PL.ladderTimer = 5;
        }
    } else if (PL.ladderTimer > 0)
        PL.ladderTimer -= 1;

    if (platformCharacterIs(IN_AIR) && PL.state != HANGING)                    /* :248 */
        PE(p)->yAcc += PL.gravityIntensity;

    if ((PL.colBot || PL.colPlatBot) && platformCharacterIs(IN_AIR) && NGE(PE(p)->yVel, N(0))) {   /* :254 */
        if (!PL.colPlat || PL.colBot) {
            PE(p)->yVel = 0;
            PE(p)->yAcc = 0;
            PL.state = RUNNING;
            PL.jumps = 0;
        }
    }
    if ((PL.colBot || PL.colPlatBot) && !PL.colPlat) PE(p)->yVel = 0;             /* :265 */

    if (PL.colBot == 0 && (!PL.colPlatBot || PL.colPlat) && platformCharacterIs(ON_GROUND)) {   /* :268 */
        PL.state = FALLING;
        PE(p)->yAcc += PE(p)->grav;
        PL.kJumped = 1;
        if (PG.hasGloves) PL.hangCount = 5;
    }

    if (PL.colTop) {                                                           /* :276 */
        if (PL.dead || PL.stunned) PE(p)->yVel = NMUL(-PE(p)->yVel, N(0.8));
        else if (PL.state == JUMPING) PE(p)->yVel = NABS(NMUL(PE(p)->yVel, N(0.3)));
    }

    if ((PL.colLeft && PL.facing == LEFT) || (PL.colRight && PL.facing == RIGHT)) {     /* :282 */
        if (PL.dead || PL.stunned) PE(p)->xVel = NMUL(-PE(p)->xVel, N(0.5));
        else PE(p)->xVel = 0;
    }

    /* JUMPING :294 */
    if (PL.kJumpReleased && platformCharacterIs(IN_AIR))
        PL.kJumped = 1;
    else if (platformCharacterIs(ON_GROUND)) {
        if (instance_exists_p(OBJ_oCape)) pswamp_player(2003, i, 0);
        PL.kJumped = 0;
    }

    if (PL.kJumpPressed && collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oWeb, 0, NOONE) != NOONE) {   /* :308 */
        int obj = instance_place_p(i, PTOD(p->x), PTOD(p->y), OBJ_oWeb);
        if (obj != NOONE) PE(&PX(obj))->life -= N(1);
        else PUNTR(2004);
        PE(p)->yAcc += NMULI(PL.initialJumpAcc, 2);
        PE(p)->yVel -= N(3);
        PE(p)->xAcc += NDIV(PE(p)->xVel, N(2));
        PL.state = JUMPING;
        PL.jumpButtonReleased = 0;
        PL.jumpTime = 0;
        PE(p)->grav = PL.gravNorm;
    } else if (PL.kJumpPressed && PL.colWaterTop) {
        PE(p)->yAcc += NMULI(PL.initialJumpAcc, 2);
        PE(p)->yVel -= N(3);
        PE(p)->xAcc += NDIV(PE(p)->xVel, N(2));
        PL.state = JUMPING;
        PL.jumpButtonReleased = 0;
        PL.jumpTime = 0;
        PE(p)->grav = PL.gravNorm;
    } else if (PG.hasCape && PL.kJumpPressed && PL.kJumped && platformCharacterIs(IN_AIR)) {
        pswamp_player(2005, i, 0);                                             /* oCape.open (package B) */
    } else if (PG.hasJetpack && PL.kJump && PL.kJumped && platformCharacterIs(IN_AIR) && PL.jetpackFuel > 0) {
        pitems_player(2006, i, 0);
    } else if (platformCharacterIs(ON_GROUND) && PL.kJumpPressed && PL.fallTimer == 0) {   /* :352 */
        if (NGT(PE(p)->xVel, N(3)) || NLT(PE(p)->xVel, N(-3))) {
            PE(p)->yAcc += NMULI(PL.initialJumpAcc, 2);
            PE(p)->xAcc += NMULI(PE(p)->xVel, 2);
        } else {
            PE(p)->yAcc += NMULI(PL.initialJumpAcc, 2);
            PE(p)->xAcc += NDIV(PE(p)->xVel, N(2));
        }
        if (PG.hasJordans) {
            PE(p)->yAcc = NMULI(PE(p)->yAcc, 3);
            PL.yAccLimit = N(12);
            PE(p)->grav = N(0.5);
        } else if (PG.hasSpringShoes)
            PE(p)->yAcc = NMUL(PE(p)->yAcc, N(1.5));
        else {
            PL.yAccLimit = N(6);
            PE(p)->grav = PL.gravNorm;
        }
        snd_play(SND_xjump);                                                   /* characterStepEvent :378 */
        PL.pushTimer = 0;
        PL.state = FALLING;
        PL.jumpButtonReleased = 0;
        PL.jumpTime = 0;
    }

    if (PL.jumpTime < PL.jumpTimeTotal) PL.jumpTime += 1;                       /* :389 */
    if (PL.kJump == 0) PL.jumpButtonReleased = 1;
    if (PL.jumpButtonReleased) PL.jumpTime = PL.jumpTimeTotal;
    /* gravityIntensity = (jumpTime / jumpTimeTotal) * grav :394 */
    PL.gravityIntensity = NMUL(NDIV(NI(PL.jumpTime), NI(PL.jumpTimeTotal)), PE(p)->grav);
    NOPS(2);

    if (PL.kUp && platformCharacterIs(ON_GROUND) && !PL.colLadder) {           /* :396 */
        PL.looking = UP;
        if (NEQ(PE(p)->xVel, N(0)) && NEQ(PE(p)->xAcc, N(0))) PL.state = LOOKING_UP;
    } else
        PL.looking = 0;
    if (!PL.kUp && PL.state == LOOKING_UP)
        PL.state = STANDING;

    /* HANGING :414 */
    x = PTOD(p->x);
    y = PTOD(p->y);
    if (!PL.colTop) {
        if (PG.hasGloves && NGT(PE(p)->yVel, N(0))) {
            if (PL.hangCount == 0 && y > 16 && !platformCharacterIs(ON_GROUND) && PL.kRight && PL.colRight &&
                (collision_point_any(x + 9, y - 5, OBJ_oSolid, 0, NOONE) ||
                 collision_point_any(x + 9, y - 6, OBJ_oSolid, 0, NOONE)))
                set_hang(p, i);
            else if (PL.hangCount == 0 && y > 16 && !platformCharacterIs(ON_GROUND) && PL.kLeft && PL.colLeft &&
                (collision_point_any(x - 9, y - 5, OBJ_oSolid, 0, NOONE) ||
                 collision_point_any(x - 9, y - 6, OBJ_oSolid, 0, NOONE)))
                set_hang(p, i);
        } else if (PL.hangCount == 0 && y > 16 && !platformCharacterIs(ON_GROUND) && PL.kRight && PL.colRight &&
                   (collision_point_p(x + 9, y - 5, OBJ_oTree, 0, NOONE) != NOONE ||
                    collision_point_p(x + 9, y - 6, OBJ_oTree, 0, NOONE) != NOONE))
            set_hang(p, i);
        else if (PL.hangCount == 0 && y > 16 && !platformCharacterIs(ON_GROUND) && PL.kLeft && PL.colLeft &&
                 (collision_point_p(x - 9, y - 5, OBJ_oTree, 0, NOONE) != NOONE ||
                  collision_point_p(x - 9, y - 6, OBJ_oTree, 0, NOONE) != NOONE))
            set_hang(p, i);
        else if (PL.hangCount == 0 && y > 16 && !platformCharacterIs(ON_GROUND) && PL.kRight && PL.colRight &&
                 (collision_point_any(x + 9, y - 5, OBJ_oSolid, 0, NOONE) ||
                  collision_point_any(x + 9, y - 6, OBJ_oSolid, 0, NOONE)) &&
                 !collision_point_any(x + 9, y - 9, OBJ_oSolid, 0, NOONE) &&
                 !collision_point_any(x, y + 9, OBJ_oSolid, 0, NOONE) && !pushblock_covered(x + 9, y - 5))
            set_hang(p, i);
        else if (PL.hangCount == 0 && y > 16 && !platformCharacterIs(ON_GROUND) && PL.kLeft && PL.colLeft &&
                 (collision_point_any(x - 9, y - 5, OBJ_oSolid, 0, NOONE) ||
                  collision_point_any(x - 9, y - 6, OBJ_oSolid, 0, NOONE)) &&
                 !collision_point_any(x - 9, y - 9, OBJ_oSolid, 0, NOONE) &&
                 !collision_point_any(x, y + 9, OBJ_oSolid, 0, NOONE) && !pushblock_covered(x - 9, y - 5))
            set_hang(p, i);
        x = PTOD(p->x);
        y = PTOD(p->y);
        if (PL.hangCount == 0 && y > 16 && !platformCharacterIs(ON_GROUND) && PL.state == FALLING &&   /* :476 */
            (collision_point_p(x, y - 5, OBJ_oArrow, 0, NOONE) != NOONE ||
             collision_point_p(x, y - 6, OBJ_oArrow, 0, NOONE) != NOONE) &&
            collision_point_p(x, y - 9, OBJ_oArrow, 0, NOONE) == NOONE &&
            collision_point_p(x, y + 9, OBJ_oArrow, 0, NOONE) == NOONE) {
            int obj = instance_nearest_p(x, y - 5, OBJ_oArrow);
            if (PE(&PX(obj))->stuck) {
                PL.state = HANGING;
                PE(p)->yVel = 0;
                PE(p)->yAcc = 0;
                PE(p)->grav = 0;
            }
        }
    }
    if (PL.hangCount > 0) PL.hangCount -= 1;                                    /* :506 */

    if (PL.state == HANGING) {                                                 /* :508 */
        if (instance_exists_p(OBJ_oCape)) pswamp_player(2007, i, 0);
        PL.kJumped = 0;
        if (PL.kDown && PL.kJumpPressed) {
            PE(p)->grav = PL.gravNorm;
            PL.state = FALLING;
            PE(p)->yAcc -= PE(p)->grav;
            PL.hangCount = 5;
            if (PG.hasGloves) PL.hangCount = 10;
        } else if (PL.kJumpPressed) {
            PE(p)->grav = PL.gravNorm;
            if ((PL.facing == RIGHT && PL.kLeft) || (PL.facing == LEFT && PL.kRight)) {
                PL.state = FALLING;
                PE(p)->yAcc -= PE(p)->grav;
            } else {
                PL.state = JUMPING;
                PE(p)->yAcc += NMULI(PL.initialJumpAcc, 2);
                if (PL.facing == RIGHT) pin_setx(p, p->x - (PI(2)));
                else pin_setx(p, p->x + (PI(2)));
            }
            PL.hangCount = PL.hangCountMax;
        }
        if ((PL.facing == LEFT && !isCollisionLeft(i, 2)) || (PL.facing == RIGHT && !isCollisionRight(i, 2))) {
            PE(p)->grav = PL.gravNorm;
            PL.state = FALLING;
            PE(p)->yAcc -= PE(p)->grav;
            PL.hangCount = 4;
        }
    } else
        PE(p)->grav = PL.gravNorm;

    /* pressing down while standing :554 */
    if (PL.kDown && platformCharacterIs(ON_GROUND) && !PL.whipping) {
        if (PL.colBot)
            PL.state = DUCKING;
        else if (PL.colPlatBot) {
            PL.fallTimer = 0;
            if (!PL.colBot) {
                int ladder = instance_place_p(i, PTOD(p->x), PTOD(p->y) + 16, OBJ_oLadder);
                PL.ladder = ladder;
                if (ladder != NOONE) {
                    if (NLT(NABS(NP(p->x - (PX(ladder).x + PI(8)))), N(4))) {
                        pin_setx(p, PX(ladder).x + PI(8));
                        PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;
                        PL.state = CLIMBING;
                    }
                } else {
                    pin_sety(p, p->y + (PI(1)));
                    PL.state = FALLING;
                    PE(p)->yAcc += PE(p)->grav;
                }
            } else
                PL.state = RUNNING;
        }
    }
    if (!PL.kDown && PL.state == DUCKING) {                                    /* :595 */
        PL.state = STANDING;
        PE(p)->xVel = 0;
        PE(p)->xAcc = 0;
    }
    if (NEQ(PE(p)->xVel, N(0)) && NEQ(PE(p)->xAcc, N(0)) && PL.state == RUNNING) PL.state = STANDING;
    if (NNE(PE(p)->xAcc, N(0)) && PL.state == STANDING) PL.state = RUNNING;
    if (NLT(PE(p)->yVel, N(0)) && platformCharacterIs(IN_AIR) && PL.state != HANGING) PL.state = JUMPING;
    if (NGT(PE(p)->yVel, N(0)) && platformCharacterIs(IN_AIR) && PL.state != HANGING) {   /* :613 */
        PL.state = FALLING;
        setCollisionBounds(i, -5, -6, 5, 8);
    } else
        setCollisionBounds(i, -5, -8, 5, 8);

    /* CLIMB LADDER :621 */
    x = PTOD(p->x);
    y = PTOD(p->y);
    PL.colPointLadder = collision_point_p(x, y, OBJ_oLadder, 0, NOONE) != NOONE ||
                        collision_point_p(x, y, OBJ_oLadderTop, 0, NOONE) != NOONE;
    if ((PL.kUp && platformCharacterIs(IN_AIR) && collision_point_p(x, y - 8, OBJ_oLadder, 0, NOONE) != NOONE &&
         PL.ladderTimer == 0) ||
        (PL.kUp && PL.colPointLadder && PL.ladderTimer == 0) ||
        (PL.kDown && PL.colPointLadder && PL.ladderTimer == 0 && platformCharacterIs(ON_GROUND) &&
         collision_point_p(x, y + 9, OBJ_oLadderTop, 0, NOONE) != NOONE && NEQ(PE(p)->xVel, N(0)))) {
        int ladder = instance_place_p(i, x, y - 8, OBJ_oLadder);
        PL.ladder = ladder;
        if (ladder != NOONE) {
            if (NLT(NABS(NP(p->x - (PX(ladder).x + PI(8)))), N(4))) {
                pin_setx(p, PX(ladder).x + PI(8));
                if (collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oLadder, 0, NOONE) == NOONE &&
                    collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oLadderTop, 0, NOONE) == NOONE)
                    pin_sety(p, PX(ladder).y + PI(14 - (gsprcol[p->spr].yo - 1)));      /* :637 sprite_yoffset */
                PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;
                PL.state = CLIMBING;
            }
        }
    }

    /* friction :692 */
    if (PL.state == CLIMBING) {
        PL.xFric = PL.frictionClimbingX;
        PL.yFric = PL.frictionClimbingY;
    } else {
        if (PL.runKey && platformCharacterIs(ON_GROUND) && PL.runHeld >= 10) {
            if (PL.kLeft) {
                PE(p)->xVel -= N(0.1);
                PL.xVelLimit = N(6);
                PL.xFric = PL.frictionRunningFastX;
            } else if (PL.kRight) {
                PE(p)->xVel += N(0.1);
                PL.xVelLimit = N(6);
                PL.xFric = PL.frictionRunningFastX;
            }
        } else if (PL.state == DUCKING) {
            if (NLT(PE(p)->xVel, N(2)) && NGT(PE(p)->xVel, N(-2))) {
                PL.xFric = N(0.2);
                PL.xVelLimit = N(3);
                pin_setispd(p, (img_t)0.8);
            } else if ((PL.kLeft || PL.kRight) && PG.downToRun) {
                pitems_player(2008, i, 0);
            } else {
                PE(p)->xVel = NMUL(PE(p)->xVel, N(0.8));
                if (NLT(PE(p)->xVel, N(0.5))) PE(p)->xVel = 0;
                PL.xFric = N(0.2);
                PL.xVelLimit = N(3);
                pin_setispd(p, (img_t)0.8);
            }
        } else {
            if (platformCharacterIs(IN_AIR)) {
                if (PL.dead || PL.stunned) PL.xFric = N(1.0);
                else PL.xFric = N(0.8);
            } else
                PL.xFric = PL.frictionRunningX;
        }
        if (collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oWeb, 0, NOONE) != NOONE) {   /* :758 */
            PL.xFric = N(0.2);
            PL.yFric = N(0.2);
            PL.fallTimer = 0;
        } else if (collision_point_any_at(i, 0, 0, OBJ_oWater)) {   /* -1, -1: false */
            pswamp_player(2009, i, 0);
        } else {
            PL.swimming = 0;
            PL.yFric = N(1);
        }
    }
    if (PL.colIceBot && PL.state != DUCKING && !PG.hasSpikeShoes) {
        PL.xFric = N(0.98);
        PL.yFric = N(1);
    }

    /* RUNNING :796 */
    if (platformCharacterIs(ON_GROUND)) {
        if (PL.state == RUNNING && PL.kLeft && PL.colLeft) PL.pushTimer += 1;
        else if (PL.state == RUNNING && PL.kRight && PL.colRight) PL.pushTimer += 1;
        else PL.pushTimer = 0;
        if (platformCharacterIs(ON_GROUND) && !PL.kJump && !PL.kDown && !PL.runKey)
            PL.xVelLimit = N(3);
        x = PTOD(p->x);
        y = PTOD(p->y);
        if (PL.state == DUCKING && NLT(NABS(PE(p)->xVel), N(3)) && PL.facing == LEFT &&                   /* :818 */
            collision_point_any(x, y + 9, OBJ_oSolid, 0, NOONE) &&
            !collision_point_any(x - 1, y + 9, OBJ_oSolid, 0, NOONE) && PL.kLeft) {
            PL.state = DUCKTOHANG;
            if (PL.holdItem != NOONE) {
                PE(&PX(PL.holdItem))->held = 0;
                if (PX(PL.holdItem).type == T_GOLDIDOL) pin_sety(&PX(PL.holdItem), PX(PL.holdItem).y - (PI(8)));
                scrDropItem(N(-1), N(-4));
            }
            if (instance_exists_p(OBJ_oMonkey)) pjungle_player(2010, i, 0);
        } else if (PL.state == DUCKING && NLT(NABS(PE(p)->xVel), N(3)) && PL.facing == RIGHT &&
                   collision_point_any(x, y + 9, OBJ_oSolid, 0, NOONE) &&
                   !collision_point_any(x + 1, y + 9, OBJ_oSolid, 0, NOONE) && PL.kRight) {
            PL.state = DUCKTOHANG;
            if (PL.holdItem != NOONE) {
                if (PX(PL.holdItem).type == T_GOLDIDOL) pin_sety(&PX(PL.holdItem), PX(PL.holdItem).y - (PI(8)));
                scrDropItem(N(1), N(-4));
            }
            if (instance_exists_p(OBJ_oMonkey)) pjungle_player(2010, i, 0);
        }
    }
    if (PL.state == DUCKTOHANG) {                                              /* :870 */
        pin_setx(p, PL.xPrev);
        pin_sety(p, PL.yPrev);
        PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;
        PE(p)->grav = 0;
    }
    if (instance_exists_p(OBJ_oParachute)) PL.yFric = N(0.5);                  /* :882 */
    if (instance_exists_p(OBJ_oCape)) pswamp_player(2011, i, 0);
    if (PL.pushTimer > 100) PL.pushTimer = 100;

    /* limits the acceleration :894 */
    if (NGT(PE(p)->xAcc, PL.xAccLimit)) PE(p)->xAcc = PL.xAccLimit;
    else if (NLT(PE(p)->xAcc, -PL.xAccLimit)) PE(p)->xAcc = -PL.xAccLimit;
    if (NGT(PE(p)->yAcc, PL.yAccLimit)) PE(p)->yAcc = PL.yAccLimit;
    else if (NLT(PE(p)->yAcc, -PL.yAccLimit)) PE(p)->yAcc = -PL.yAccLimit;
    PE(p)->xVel += PE(p)->xAcc;                                                        /* :900 */
    if (PL.dead || PL.stunned) PE(p)->yVel += N(0.6);
    else PE(p)->yVel += PE(p)->yAcc;
    PE(p)->xAcc = 0;
    PE(p)->yAcc = 0;
    PE(p)->xVel = NMUL(PE(p)->xVel, PL.xFric);                                         /* :909 */
    PE(p)->yVel = NMUL(PE(p)->yVel, PL.yFric);
    NOPS(10);
    if (instance_exists_p(OBJ_oBall)) pitems_player(2012, i, 0);                             /* :913 */
    if (!PL.dead && !PL.stunned) {                                             /* :943 */
        if (NGT(PE(p)->xVel, PL.xVelLimit)) PE(p)->xVel = PL.xVelLimit;
        else if (NLT(PE(p)->xVel, -PL.xVelLimit)) PE(p)->xVel = -PL.xVelLimit;
    }
    if (NGT(PE(p)->yVel, PL.yVelLimit)) PE(p)->yVel = PL.yVelLimit;
    else if (NLT(PE(p)->yVel, -PL.yVelLimit)) PE(p)->yVel = -PL.yVelLimit;
    if (approximatelyZero(PE(p)->xVel)) PE(p)->xVel = 0;                               /* :952 */
    if (approximatelyZero(PE(p)->yVel)) PE(p)->yVel = 0;
    if (approximatelyZero(PE(p)->xAcc)) PE(p)->xAcc = 0;
    if (approximatelyZero(PE(p)->yAcc)) PE(p)->yAcc = 0;
    NOPS(12);

    /* slopes :960: colTop is the value computed above, so the loop stops at once or runs maxSlope + 1 times */
    {
        pos slopeYPrev = 0;
        int32_t slopeChangeInY;
        if (PL.maxSlope > 0 && platformCharacterIs(ON_GROUND) && NNE(PE(p)->xVel, N(0))) {
            int ch = 0;
            slopeYPrev = p->y;
            /* the pixel steps write the field alone and pw_changed runs once after them if any changed y: nothing
               reads the collision state or the marks between them (the body reads PL.colTop, a flag), and
               pw_changed's marks are those of its last call (the draw mark, bbk, grid_dirty, mark_e's dirty / test
               list fronts, nc_moved's in-place entry; rest_end counts only whether a change happened) */
            for (; p->y >= slopeYPrev - PI(PL.maxSlope); ) {
                pos o = p->y;
                if (PL.colTop)
                    break;
                PIN_SETY_RAW(p, p->y - (PI(1)));
                if (POS_NE(o, p->y)) ch = 1;
            }
            if (ch) pin_changed_(p);
            slopeChangeInY = PFLOOR(slopeYPrev - p->y);
        } else
            slopeChangeInY = 0;
        if (NMULI_GT0(NABS(PE(p)->xVel), PL.maxSlope) && platformCharacterIs(ON_GROUND)) {   /* :973: NGT(NMULI(NABS(xVel), maxSlope), 0) */
            pos xPrev2 = p->x, yPrev2 = slopeYPrev, yPrevHigh = p->y;
            double dist;
            PL.xPrev = xPrev2;
            PL.yPrev = yPrev2;
            moveTo(i, PE(p)->xVel, PE(p)->yVel + NI(slopeChangeInY), &xVelInteger, &yVelInteger);
            {
                double dx = PTOD(p->x) - PTOD(xPrev2), dy = PTOD(p->y) - PTOD(yPrev2), d2 = dx * dx + dy * dy;
                double s = d2, prev = 0, a = xVelInteger < 0 ? -xVelInteger : xVelInteger;
                int it;
                /* dist is read only by the test below (and the ratio when it holds). d2 <= a * a (a a whole number,
                   exact): sqrt(d2) <= a, and the loop's result s (a fixed point of the rounded step, reached in
                   under 64 iterations for these d2) is within a few ulps of sqrt(d2), so s - a <= eps and DGT is
                   false: the loop is skipped (tests/slopedist) */
                if (d2 <= a * a) dist = 0;
                else {
                    for (it = 0; it < 64 && s != prev && d2 > 0; it++) { prev = s; s = 0.5 * (s + d2 / s); }
                    dist = d2 > 0 ? s : 0;                                     /* point_distance */
                }
            }
            if (DGT(dist, (xVelInteger < 0 ? -xVelInteger : xVelInteger))) {
                double ratio;
                int32_t axi = xVelInteger < 0 ? -xVelInteger : xVelInteger;
                pin_setx(p, xPrev2);
                pin_sety(p, yPrevHigh);
                ratio = axi / dist * 0.9;
                moveTo(i, NI(dround(xVelInteger * ratio)), NI(dround(yVelInteger * ratio + slopeChangeInY)),
                       &xVelInteger, &yVelInteger);
            }
        } else
            moveTo(i, PE(p)->xVel, PE(p)->yVel, &xVelInteger, &yVelInteger);           /* :1000 */
    }
    if (!PL.colBot && PL.maxDownSlope > 0 && xVelInteger != 0 && platformCharacterIs(ON_GROUND)) {   /* :1004 */
        pos upYPrev = p->y, o;
        int ch = 0;
        /* the field alone, one pw_changed at the end if any step or the final write changed y (as the slope loop
           above: the body reads PL.colBot only) */
        for (; p->y <= upYPrev + PI(PL.maxDownSlope); ) {
            if (PL.colBot) {
                upYPrev = p->y;
                break;
            }
            o = p->y;
            PIN_SETY_RAW(p, p->y + (PI(1)));
            if (POS_NE(o, p->y)) ch = 1;
        }
        o = p->y;
        PIN_SETY_RAW(p, upYPrev);
        if (POS_NE(o, upYPrev)) ch = 1;
        if (ch) pin_changed_(p);
    }
    characterSprite(i);                                                        /* :1018 */
    PL.statePrevPrev = PL.statePrev;
    PL.statePrev = PL.state;
    if (PL.state == RUNNING || PL.state == DUCKING || PL.state == LOOKING_UP) {   /* :1025 */
        if (PL.state == RUNNING || PL.state == LOOKING_UP)
            pin_setispd(p, (img_t)(NTOD(NABS(PE(p)->xVel)) * NTOD(PL.runAnimSpeed) + 0.1));
    }
    if (PL.state == CLIMBING) {                                                /* :1030 */
        double ax = NTOD(NABS(PE(p)->xVel)), ay = NTOD(NABS(PE(p)->yVel)), s2 = ax * ax + ay * ay, s = s2, prev = 0;
        int it;
        for (it = 0; it < 64 && s != prev && s2 > 0; it++) { prev = s; s = 0.5 * (s + s2 / s); }
        pin_setispd(p, (img_t)((s2 > 0 ? s : 0) * NTOD(PL.climbAnimSpeed)));
    }
    if (NGE(PE(p)->xVel, N(4)) || NLE(PE(p)->xVel, N(-4))) {
        pin_setispd(p, 1);
        if (platformCharacterIs(ON_GROUND)) setCollisionBounds(i, -8, -8, 8, 8);
        else setCollisionBounds(i, -5, -8, 5, 8);
    } else
        setCollisionBounds(i, -5, -8, 5, 8);
    if (PL.whipping) pin_setispd(p, 1);
    if (PL.state == DUCKTOHANG) {
        pin_setimg(p, 0);
        pin_setispd(p, (img_t)0.8);
    }
    if (DGT(p->ispd, 1)) pin_setispd(p, 1);
}

/* the hold-item hand-off used by the whoa, dead / stunned and hurt blocks */
static void drop_or_switch(void)
{
    if (PX(PL.holdItem).type == PL.pickupItemType) {
        PL.holdItem = NOONE;
        PL.pickupItemType = T_NONE;
    } else
        scrHoldItem(PL.pickupItemType);
}

static void open_chest(int i)
{
    struct pin *p = &PX(i);
    int chest = instance_place_p(i, PTOD(p->x), PTOD(p->y), OBJ_oChest), k, n, obj;
    if (chest == NOONE) { PUNTR(2013); return; }
    if (PX(chest).spr == GSPR_sChest) {                                         /* :567 */
        pin_set_sprite(chest, GSPR_sChestOpen);
        if (RAND(1, 12) == 1 && G.currLevel > 0) {
            obj = pin_create(PX(chest).x, PX(chest).y, OBJ_oBomb);
            {
                int a = RAND(0, 3);
                int b = RAND(0, 3);
                PE(&PX(obj))->xVel = NI(a - b);
            }
            PE(&PX(obj))->yVel = N(-2);
            pin_set_sprite(obj, GSPR_sBombArmed);
            PE(&PX(obj))->alarm[1] = 40;
            snd_play(SND_xtrap);                                               /* :580 */
        } else {
            int reps;
            snd_play(SND_xchestopen);                                          /* :583 */
            reps = RAND(3, 4);
            for (k = 0; k < reps; k++) {
                n = RAND(1, 3);
                obj = NOONE;
                switch (n) {
                case 1: obj = pin_create(PX(chest).x, PX(chest).y, OBJ_oEmerald); break;
                case 2: obj = pin_create(PX(chest).x, PX(chest).y, OBJ_oSapphire); break;
                case 3: obj = pin_create(PX(chest).x, PX(chest).y, OBJ_oRuby); break;
                }
                {
                    int a = RAND(0, 3);
                    int b = RAND(0, 3);
                    PE(&PX(obj))->xVel = NI(a - b);
                }
                PE(&PX(obj))->yVel = N(-2);
            }
            if (RAND(1, 4) == 1) {
                n = RAND(1, 3);
                obj = NOONE;
                switch (n) {
                case 1: obj = pin_create(PX(chest).x, PX(chest).y, OBJ_oEmeraldBig); break;
                case 2: obj = pin_create(PX(chest).x, PX(chest).y, OBJ_oSapphireBig); break;
                case 3: obj = pin_create(PX(chest).x, PX(chest).y, OBJ_oRubyBig); break;
                }
                {
                    int a = RAND(0, 3);
                    int b = RAND(0, 3);
                    PE(&PX(obj))->xVel = NI(a - b);
                }
                PE(&PX(obj))->yVel = N(-2);
            }
        }
        PL.kAttackPressed = 0;
    }
}

static void open_crate(int i)
{
    struct pin *p = &PX(i);
    int chest = instance_place_p(i, PTOD(p->x), PTOD(p->y), OBJ_oCrate), obj;
    pos cx, cy;
    if (chest == NOONE) { PUNTR(2014); return; }
    cx = PX(chest).x;
    cy = PX(chest).y;
    if (RAND(1, 500) == 1) obj = pin_create(cx, cy, OBJ_oJetpack);              /* :619 */
    else if (RAND(1, 200) == 1) obj = pin_create(cx, cy, OBJ_oCapePickup);
    else if (RAND(1, 100) == 1) obj = pin_create(cx, cy, OBJ_oShotgun);
    else if (RAND(1, 100) == 1) obj = pin_create(cx, cy, OBJ_oMattock);
    else if (RAND(1, 100) == 1) obj = pin_create(cx, cy, OBJ_oTeleporter);
    else if (RAND(1, 90) == 1) obj = pin_create(cx, cy, OBJ_oGloves);
    else if (RAND(1, 90) == 1) obj = pin_create(cx, cy, OBJ_oSpectacles);
    else if (RAND(1, 80) == 1) obj = pin_create(cx, cy, OBJ_oWebCannon);
    else if (RAND(1, 80) == 1) obj = pin_create(cx, cy, OBJ_oPistol);
    else if (RAND(1, 80) == 1) obj = pin_create(cx, cy, OBJ_oMitt);
    else if (RAND(1, 60) == 1) obj = pin_create(cx, cy, OBJ_oPaste);
    else if (RAND(1, 60) == 1) obj = pin_create(cx, cy, OBJ_oSpringShoes);
    else if (RAND(1, 60) == 1) obj = pin_create(cx, cy, OBJ_oSpikeShoes);
    else if (RAND(1, 60) == 1) obj = pin_create(cx, cy, OBJ_oMachete);
    else if (RAND(1, 40) == 1) obj = pin_create(cx, cy, OBJ_oBombBox);
    else if (RAND(1, 40) == 1) obj = pin_create(cx, cy, OBJ_oBow);
    else if (RAND(1, 20) == 1) obj = pin_create(cx, cy, OBJ_oCompass);
    else if (RAND(1, 10) == 1) obj = pin_create(cx, cy, OBJ_oParaPickup);
    else if (RAND(1, 2) == 1) obj = pin_create(cx, cy, OBJ_oRopePile);
    else obj = pin_create(cx, cy, OBJ_oBombBag);
    PE(&PX(obj))->cost = 0;
    snd_play(SND_xpickup);                                                     /* :640 */
    if (chest == PL.holdItem) {
        PL.holdItem = NOONE;
        PL.pickupItemType = T_NONE;
    }
    pin_create(PX(chest).x, PX(chest).y, OBJ_oPoof);                           /* :646 with chest */
    pin_destroy(chest);
    PL.kAttackPressed = 0;
}

static void exit_level(int i)
{
    struct pin *p = &PX(i);
    int door;
    PL.holdArrow = 0;                                                          /* :771 */
    G.pickupItem = PICK_NONE;
    if (PL.holdItem != NOONE) {
        int h = PL.holdItem;
        if (PX(h).type == T_GOLDIDOL) {
            PG.money += PE(&PX(h))->value * (G.levelType + 1);
            if (PX(h).spr == GSPR_sCrystalSkull) PG.skulls += 1;
            else PG.idols += 1;
            snd_play(SND_xcoin);                                               /* :781 */
            pin_create(p->x, p->y - PI(8), OBJ_oBigCollect);
            pin_destroy(h);
            PL.holdItem = NOONE;
        } else if (PX(h).type == T_DAMSEL) {                                   /* :786 (P5) */
            if (PE(&PX(h))->hp > 0) {
                door = instance_place_p(i, PTOD(p->x), PTOD(p->y), OBJ_oExit);
                PG.damsels += 1;
                PG.xdamsels += 1;
                pin_setx(&PX(h), PX(door).x + PI(8));
                pin_sety(&PX(h), PX(door).y + PI(8));
                pin_set_sprite(h, GSPR_sDamselExit);
                PE(&PX(h))->status = 4;
                PE(&PX(h))->held = 0;
                PE(&PX(h))->xVel = 0;
                PE(&PX(h))->yVel = 0;
                snd_play(SND_xsteps);                                          /* :805 */
                pin_setdepth(&PX(h), 1000);
                PE(&PX(h))->active = 0;
                PL.holdItem = NOONE;
            } else {
                PE(&PX(h))->status = 2;
                PE(&PX(h))->held = 0;
                PL.holdItem = NOONE;
                PL.pickupItemType = T_NONE;
            }
        } else if (PE(&PX(h))->heavy) {
            PE(&PX(h))->held = 0;
            PL.holdItem = NOONE;
            PL.pickupItemType = T_NONE;
        } else if (PX(h).type == T_BOMB) {
            if (PE(&PX(h))->armed) PE(&PX(h))->held = 0;
            else {
                PG.bombs += 1;
                pin_destroy(h);
            }
            G.pickupItem = (uint8_t)pickup_of_ptype(PL.pickupItemType);
        } else if (PX(h).type == T_ROPE) {
            PG.rope += 1;
            pin_destroy(h);
            G.pickupItem = (uint8_t)pickup_of_ptype(PL.pickupItemType);
        } else {
            G.pickupItem = (uint8_t)pickup_of_ptype(PX(h).type);
            PE(&PX(h))->breakPieces = 0;
            pin_destroy(h);
        }
        PL.holdItem = NOONE;
        PL.pickupItemType = T_NONE;
    }
    door = instance_place_p(i, PTOD(p->x), PTOD(p->y), OBJ_oExit);              /* :859 */
    if (door != NOONE) {
        pin_setx(p, PX(door).x + PI(8));
        pin_sety(p, PX(door).y + PI(8));
    }
    PG.money += PG.collect;                                                    /* :867 */
    PG.xmoney += PG.collect;
    PG.collect = 0;
    pin_set_sprite(i, GSPR_sPExit);
    pin_setispd(p, (img_t)0.5);
    PL.active = 0;
    PL.invincible = 999;
    pin_setdepth(p, 999);
    if (G.thiefLevel > 0) G.thiefLevel -= 1;
    if (G.currLevel == 1) G.currLevel += PL.firstLevelSkip;
    else G.currLevel += PL.levelSkip;
    snd_stop_music();                                                          /* :881 */
    snd_play(SND_xsteps);
    if (collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oXMarket, 0, NOONE) != NOONE) G.genBlackMarket = 1;
    if (collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oXGold, 0, NOONE) != NOONE) G.cityOfGold = 1;
    if (instance_exists_p(OBJ_oMonkey)) pjungle_player(2016, i, 0);
}

/* objects/oPlayer1/Step_0.gml :676-760: up at a door of the oXStart family (the title / scores rooms' doors; in a
   game only rOlmec's oXEnd, the way to the ending). pExit: xSTART 0, xTUTORIAL 1, xSCORES 2, xTITLE 3, xEND 4,
   xSHORTCUT5 5, xSHORTCUT9 6, xSUN 7, xMOON 8, xSTARS 9, xCHANGE 10, xSHORTCUT13 11, xCHANGE2 12 (Create_0.gml :87) */
static void enter_door(int i)
{
    struct pin *p = &PX(i);
    double x, y;
    int door;
    if (isRoomIs(R_rOlmec) && PL.holdItem != NOONE) {                          /* :686 oXEnd is oXStart's child */
        int h = PL.holdItem;
        if (PE(&PX(h))->heavy) {
            PE(&PX(h))->held = 0;
            PL.holdItem = NOONE;
            PL.pickupItemType = T_NONE;
        } else if (PX(h).type == T_BOMB) {
            if (PE(&PX(h))->armed) PE(&PX(h))->held = 0;
            else {
                PG.bombs += 1;
                pin_destroy(h);
            }
            G.pickupItem = (uint8_t)pickup_of_ptype(PL.pickupItemType);
        } else if (PX(h).type == T_ROPE) {
            PG.rope += 1;
            pin_destroy(h);
            G.pickupItem = (uint8_t)pickup_of_ptype(PL.pickupItemType);
        } else {
            G.pickupItem = (uint8_t)pickup_of_ptype(PX(h).type);
            PE(&PX(h))->breakPieces = 0;
            pin_destroy(h);
        }
    } else if (isRoomIs(R_rOlmec)) G.pickupItem = PICK_NONE;                   /* :724 */
    else if (PL.holdItem != NOONE) PE(&PX(PL.holdItem))->held = 0;
    PL.holdItem = NOONE;
    PL.pickupItemType = T_NONE;
    x = PTOD(p->x);
    y = PTOD(p->y);
    door = instance_place_p(i, x, y, OBJ_oXStart);                             /* :730 */
    if (door != NOONE) pin_setx(p, PX(door).x + PI(8));
    pin_set_sprite(i, G.isDamsel ? GSPR_sDamselExit : G.isTunnelMan ? GSPR_sTunnelExit : GSPR_sPExit);
    pin_setispd(p, (img_t)0.5);
    PL.active = 0;
    pin_setdepth(p, 999);
    PL.invincible = 999;
    x = PTOD(p->x);
    PL.pExit = 0;                                                              /* :741 xSTART */
    if (collision_point_p(x, y, OBJ_oXScores, 0, NOONE) != NOONE) PL.pExit = 2;
    else if (collision_point_p(x, y, OBJ_oXTutorial, 0, NOONE) != NOONE) PL.pExit = 1;
    else if (collision_point_p(x, y, OBJ_oXTitle, 0, NOONE) != NOONE) PL.pExit = 3;
    else if (collision_point_p(x, y, OBJ_oXEnd, 0, NOONE) != NOONE) PL.pExit = 4;
    else if (collision_point_p(x, y, OBJ_oXShortcut5, 0, NOONE) != NOONE) PL.pExit = 5;
    else if (collision_point_p(x, y, OBJ_oXShortcut9, 0, NOONE) != NOONE) PL.pExit = 6;
    else if (collision_point_p(x, y, OBJ_oXShortcut13, 0, NOONE) != NOONE) PL.pExit = 11;
    else if (collision_point_p(x, y, OBJ_oXSun, 0, NOONE) != NOONE) PL.pExit = 7;
    else if (collision_point_p(x, y, OBJ_oXMoon, 0, NOONE) != NOONE) PL.pExit = 8;
    else if (collision_point_p(x, y, OBJ_oXStars, 0, NOONE) != NOONE) PL.pExit = 9;
    else if (collision_point_p(x, y, OBJ_oXChange, 0, NOONE) != NOONE) PL.pExit = 10;
    else if (collision_point_p(x, y, OBJ_oXChange2, 0, NOONE) != NOONE) PL.pExit = 12;
    if (PL.pExit != 12) snd_stop_music();                                      /* :757 */
    snd_play(SND_xsteps);
}

static void hurt_logic(int i)
{
    struct pin *p = &PX(i);
    double x = PTOD(p->x), y = PTOD(p->y);
    int obj;
    if (PG.plife < -10000) PG.plife = -10000;                                  /* :1452 */
    if (PG.plife < -99 && p->visible && !play_god) {
        scrCreateBlood(i, p->x, p->y, 3);
        pin_setvisible(p, 0);
    }
    if (!(PG.plife >= -99 && p->visible && !spr_is_exit(p->spr)))
        return;
    if (!play_god && collision_point_any(x, y, OBJ_oSolid, 0, NOONE)) {   /* :1463 crushed */
        PG.plife -= 99;
        PL.active = 0;
        PE(p)->yVel = N(-3);
        snd_play(SND_xdie);                                                    /* :1476 */
        scrCreateBlood(i, p->x, p->y, 3);
        pin_setvisible(p, 0);
    }
    if (collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oArrow, 0, NOONE) != NOONE) {   /* :1483 */
        obj = instance_nearest_p(x, y, OBJ_oArrow);
        if (obj != NOONE && NGT(NABS(PE(&PX(obj))->xVel), N(3)) && !PE(&PX(obj))->safe) {
            if (PG.plife > 0) PG.plife -= 2;
            PE(p)->xVel = PE(&PX(obj))->xVel;
            PE(p)->yVel = N(-4);
            scrCreateBlood(i, p->x, p->y, 3);
            pin_destroy(obj);
            snd_play(SND_xhurt);                                               /* :1502 */
            PL.stunned = 1;
            PL.stunTimer = 20;
        }
    }
    if (collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oRock, 0, NOONE) != NOONE) {    /* :1508 */
        obj = instance_nearest_p(x, y, OBJ_oRock);
        if (obj != NOONE && NGT(NABS(PE(&PX(obj))->xVel), N(4)) && !PE(&PX(obj))->safe && !PL.stunned && !PL.dead) {
            if (PG.hasMitt && PL.holdItem == NOONE) pitems_player(2017, i, 0);
            else {
                if (PG.plife > 0) PG.plife -= 2;
                PE(p)->xVel = PE(&PX(obj))->xVel;
                PE(p)->yVel = N(-4);
                scrCreateBlood(i, p->x, p->y, 3);
                snd_play(SND_xhurt);                                           /* :1530 */
                PL.stunned = 1;
                PL.stunTimer = 20;
            }
        }
    }
    if (collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oLaser, 0, NOONE) != NOONE) pice_player(2018, i, 0);
    if (collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oPsychicWave, 0, NOONE) != NOONE && !PL.stunned && !PL.dead)
        pice_player(2019, i, 0);
    if (collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oExplosion, 0, NOONE) != NOONE) {   /* :1584 */
        int ex;
        PG.plife -= 10;
        ex = instance_nearest_p(x, y, OBJ_oExplosion);
        if (PX(ex).x < p->x) PE(p)->xVel = NI(RAND(4, 6));
        else PE(p)->xVel = NI(-RAND(4, 6));
        PE(p)->yVel = N(-6);
        PL.burning = 50;
        PL.stunned = 1;
        PL.stunTimer = 100;
        scrCreateBlood(i, p->x, p->y, 1);
    }
    x = PTOD(p->x);
    y = PTOD(p->y);
    obj = collision_rect_p(x - 6, y - 6, x + 6, y + 7, OBJ_oSpearsLeft, 0, NOONE);  /* :1599 */
    if (obj != NOONE) pjungle_player(2020, i, obj);
    if (collision_rect_p(x - 6, y - 6, x + 6, y + 7, OBJ_oSmashTrap, 0, NOONE) != NOONE) ptemple_player(2021, i, 0);
    obj = collision_rect_p(x - 2, y - 9, x + 2, y - 7, OBJ_oCeilingTrap, 0, NOONE);
    if (obj != NOONE && PE(&PX(obj))->status > 0) ptemple_player(2022, i, obj);
    PL.colSpikes = 0;                                                          /* :1652 */
    if (collision_rect_p(x - 4, y - 4, x + 4, y + 8, OBJ_oSpikes, 0, NOONE) != NOONE) PL.colSpikes = 1;
    if (PL.colSpikes && PL.dead) {
        if (!collision_point_any(x, y + 9, OBJ_oSolid, 0, NOONE)) pin_sety(p, PADDV(p->y, N(0.05)));
        else PE(p)->myGrav = N(0.6);
    } else
        PE(p)->myGrav = N(0.6);
    if (PL.colSpikes && NGT(PE(p)->yVel, N(0)) && (PL.fallTimer > 4 || PL.stunned) && !play_god) {   /* :1663 */
        if (!PL.dead) {
            scrCreateBlood(i, p->x, p->y, 3);
            PG.plife -= 99;
            PE(p)->xVel = 0;
            PE(p)->yVel = 0;
            PE(p)->myGrav = 0;
        }
        obj = instance_place_p(i, PTOD(p->x), PTOD(p->y), OBJ_oSpikes);
        if (obj != NOONE) pin_set_sprite(obj, GSPR_sSpikesBlood);
    }
}

/* oPlayer1 Step :74-147: distToNearestLightSource, the nearest light (oLevel Step's darkness on dark levels):
   distance_to_object(source) of the nearest instance of each kind (instance_nearest by origin) */
static double light_dist(int self, int obj)
{
    int s = instance_nearest_p(PTOD(PX(self).x), PTOD(PX(self).y), obj);
    pcol_touch(self);                                /* F_DistanceToObject computes both boxes */
    pcol_touch(s);
    return (float)distance_to_instance_p(self, s);  /* the runner's distance_to_object is a float */
}
static void pl_light(int i)
{
    static const int16_t near[] = { OBJ_oLava, OBJ_oLamp, OBJ_oLampItem, OBJ_oFlareCrate };            /* :82-105 */
    static const int16_t near48[] = { OBJ_oTikiTorch, OBJ_oArrowTrapLeftLit, OBJ_oArrowTrapRightLit,
                                      OBJ_oSpearTrapLit, OBJ_oSmashTrapLit };                           /* :106-135 */
    static const int16_t blast[] = { OBJ_oShotgunBlastLeft, OBJ_oShotgunBlastRight };                   /* :136-147 */
    double d = 999, e;
    unsigned k;
    if (instance_exists_p(OBJ_oExplosion)) {                                   /* :75-81 */
        int s = instance_nearest_p(PTOD(PX(i).x), PTOD(PX(i).y), OBJ_oExplosion);
        double img = (double)PX(s).img;
        d = light_dist(i, OBJ_oExplosion);
        if (img <= 3) d -= img * 16;
        else d += (img - 3) * 16;
    }
    for (k = 0; k < sizeof near / sizeof near[0]; k++)
        if (instance_exists_p(near[k]) && (e = light_dist(i, near[k])) < d) d = e;
    for (k = 0; k < sizeof near48 / sizeof near48[0]; k++)
        if (instance_exists_p(near48[k]) && (e = light_dist(i, near48[k])) + 48 < d) d = e + 48;
    for (k = 0; k < sizeof blast / sizeof blast[0]; k++)
        if (instance_exists_p(blast[k]) && (e = light_dist(i, blast[k])) < d) d = e;
    PL.distToNearestLightSource = d;
}

/* objects/oPlayer1/Step_0.gml */
void pl_step(int i)
{
    struct pin *p = &PX(i);
    if (PG.plife > 99) PG.plife = 99;                                          /* :8 */
    if (PG.bombs > 99) PG.bombs = 99;
    if (PG.rope > 99) PG.rope = 99;
    if (PG.hasCape) pswamp_player(2030, i, 0);
    if (PL.redColor > 0) {                                                     /* :20 kapala */
        if (PL.redToggle) PL.redColor -= 5;
        else if (PL.redColor < 20) PL.redColor += 5;
        else PL.redToggle = 1;
    } else
        PL.redColor = 0;
    if (PL.holdArrow == ARROW_BOMB) pitems_player(2031, i, 0);                              /* :28 */
    if (PL.dead && !p->visible) {                                              /* :63 */
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        PE(p)->grav = 0;
        PE(p)->myGrav = 0;
        PL.bounced = 1;
    }
    if (LIGHT_ON()) pl_light(i);                                               /* :74-147 */
    /* WHOA :150 */
    if (p->spr == GSPR_sWhoaLeft || p->spr == GSPR_sDamselWhoaL || p->spr == GSPR_sTunnelWhoaL) {
        if (PL.whoaTimer > 0) PL.whoaTimer -= 1;
        else if (PL.holdItem != NOONE) {
            int h = PL.holdItem;
            PE(&PX(h))->held = 0;
            if (PL.facing == LEFT) PE(&PX(h))->xVel = N(-2);
            else PE(&PX(h))->xVel = N(2);
            if (PX(h).type == T_DAMSEL) snd_play(SND_xdamsel);                 /* :158 */
            if (PX(h).type == T_BOW && PL.bowArmed) scrFireBow();
            drop_or_switch();
        }
    } else
        PL.whoaTimer = PL.whoaTimerMax;
    if (PL.firing > 0) PL.firing -= 1;                                         /* :174 */
    if (collision_point_any_at(i, 0, 0, OBJ_oWaterSwim)) pswamp_player(2032, i, 0);   /* -1, -1: false */
    if (PL.burning > 0) {                                                      /* :188 */
        if (RAND(1, 5) == 1) {
            int yb = RAND(4, 12);                                              /* arguments: last first */
            int xb = RAND(4, 12);
            pin_create(p->x - PI(8) + PI(xb), p->y - PI(8) + PI(yb), OBJ_oBurn);
        }
        PL.burning -= 1;
    }
    if (!play_god && collision_point_any_at(i, 0, 6, OBJ_oLava)) ptemple_player(2033, i, 0);
    if (PG.hasJetpack && platformCharacterIs(ON_GROUND)) PL.jetpackFuel = 50;
    if (PTOD(p->y) > PW.room_h + 16 && !PL.dead && !play_god) {                /* :221 */
        PG.plife -= 99;
        PE(p)->xVel = 0;
        PE(p)->yVel = 0;
        PE(p)->grav = 0;
        PE(p)->myGrav = 0;
        PL.bounced = 1;
        if (PL.holdItem != NOONE) {
            pin_setvisible(&PX(PL.holdItem), 1);
            PE(&PX(PL.holdItem))->held = 0;
            PL.holdItem = NOONE;
            PL.pickupItemType = T_NONE;
        }
        snd_play(SND_xthud);                                                   /* :237 */
        snd_play(SND_xdie);
    }
    if (PL.active) {                                                           /* :241 */
        if (PL.stunTimer > 0 && (p->spr == GSPR_sStunL || p->spr == GSPR_sDamselStunL || p->spr == GSPR_sTunnelStunL)) {
            pin_setispd(p, (img_t)0.4);
            PL.stunTimer -= 1;
        }
        if (PL.stunTimer < 1 && (p->spr == GSPR_sStunL || p->spr == GSPR_sDamselStunL || p->spr == GSPR_sTunnelStunL))
            PL.stunned = 0;
        if (instance_exists_p(OBJ_oParachute)) PL.fallTimer = 0;
        if (NGT(PE(p)->yVel, N(0)) && PL.state != CLIMBING) {                             /* :254 */
            PL.fallTimer += 1;
            if (PL.fallTimer > 16) PL.wallHurt = 0;
            if (PG.hasParachute && !PL.stunned && PL.fallTimer > 14) pitems_player(2034, i, 0);
        } else if (platformCharacterIs(ON_GROUND) && PL.fallTimer > 16 &&
                   collision_rect_p(PTOD(p->x) - 8, PTOD(p->y) - 8, PTOD(p->x) + 8, PTOD(p->y) + 8, OBJ_oSpringTrap, 0, NOONE) == NOONE) {
            int obj;                                                           /* :269 LONG DROP */
            PL.stunned = 1;
            if (PL.fallTimer > 48) PG.plife -= 10;
            else if (PL.fallTimer > 32) PG.plife -= 2;
            else PG.plife -= 1;
            if (PG.plife < 1) scrCreateBlood(i, p->x, p->y, 3);
            PL.bounced = 1;
            PL.stunTimer += 60;
            PE(p)->yVel = N(-3);
            PL.fallTimer = 0;
            obj = pin_create(p->x - PI(4), p->y + PI(6), OBJ_oPoof);
            PE(&PX(obj))->xVel = N(-0.4);
            obj = pin_create(p->x + PI(4), p->y + PI(6), OBJ_oPoof);
            PE(&PX(obj))->xVel = N(0.4);
            snd_play(SND_xthud);                                               /* :289 */
        } else {
            PL.fallTimer = 0;
            if (instance_exists_p(OBJ_oParachute)) pitems_player(2035, i, 0);
        }
        if (PL.swimming && !collision_point_any_at(i, 0, 0, OBJ_oLava)) pswamp_player(2036, i, 0);
        else PL.bubbleTimer = PL.bubbleTimerMax;
        if (PL.state != DUCKTOHANG && !PL.stunned && !PL.dead && !spr_is_exit(p->spr)) {   /* :318 */
            PL.bounced = 0;
            characterStepEvent(i);
        } else if (PL.state != DUCKING && PL.state != DUCKTOHANG)
            PL.state = STANDING;
    }
    if (PL.dead || PL.stunned) {                                               /* :330 */
        if (PL.holdItem != NOONE) {
            int h = PL.holdItem;
            if (PX(h).type == T_BOW && PL.bowArmed) scrFireBow();
            pin_setvisible(&PX(h), 1);
            PE(&PX(h))->held = 0;
            drop_or_switch();
        }
        if (PL.bounced) PE(p)->yVel += N(1);
        else PE(p)->yVel += N(0.6);
        if (isCollisionTop(i, 1) && NLT(PE(p)->yVel, N(0))) PE(p)->yVel = NMUL(-PE(p)->yVel, N(0.8));
        if (isCollisionLeft(i, 1) || isCollisionRight(i, 1)) PE(p)->xVel = NMUL(-PE(p)->xVel, N(0.5));
        if (isCollisionBottom(i, 1) || isCollisionPlatformBottom(i, 1)) {
            if (NGT(PE(p)->yVel, N(1))) PE(p)->yVel = NMUL(-PE(p)->yVel, N(0.5));
            else PE(p)->yVel = 0;
            if (NLT(NABS(PE(p)->xVel), N(0.1))) PE(p)->xVel = 0;
            else if (NNE(NABS(PE(p)->xVel), N(0)) && collision_point_p(PTOD(p->x), PTOD(p->y) + 16, OBJ_oIce, 0, NOONE) != NOONE)
                PE(p)->xVel = NMUL(PE(p)->xVel, N(0.8));
            else if (NNE(NABS(PE(p)->xVel), N(0))) PE(p)->xVel = NMUL(PE(p)->xVel, N(0.3));
            PL.bounced = 1;
        }
        PL.xVelLimit = N(10);
        if (NGT(PE(p)->xVel, PL.xVelLimit)) PE(p)->xVel = PL.xVelLimit;
        else if (NLT(PE(p)->xVel, -PL.xVelLimit)) PE(p)->xVel = -PL.xVelLimit;
        if (NGT(PE(p)->yVel, PL.yVelLimit)) PE(p)->yVel = PL.yVelLimit;
        else if (NLT(PE(p)->yVel, -PL.yVelLimit)) PE(p)->yVel = -PL.yVelLimit;
        NOPS(10);
        moveTo(i, PE(p)->xVel, PE(p)->yVel, 0, 0);
    } else if (isLevel()) {                                                    /* :386 look up and down */
        if (PL.kDown && (platformCharacterIs(ON_GROUND) || PL.state == HANGING) && !PL.kRight && !PL.kLeft) {
            if (PL.viewCount <= 30) PL.viewCount += 1;
            else { view_read(); view_set_y(PW.yview + 4); }
        } else if (PL.kUp && (platformCharacterIs(ON_GROUND) || PL.state == HANGING) && !PL.kRight && !PL.kLeft) {
            if (PL.viewCount <= 30) PL.viewCount += 1;
            else { view_read(); view_set_y(PW.yview - 4); }
        } else
            PL.viewCount = 0;
    }
    if (PL.dead) PL.kAttackPressed = GPp(K_ATTACK);                            /* :403 */
    if (PG.plife > 0) PL.kBombPressed = GPp(K_BOMB);
    else PL.kBombPressed = 0;
    if (PG.plife > 0) PL.kRopePressed = GPp(K_ROPE);
    else PL.kRopePressed = 0;
    PL.kPayPressed = GPp(K_PAY);

    /* the whip :417 */
    if (spr_is_attack(p->spr) && PL.facing == LEFT && DGT(p->img, 4) && instance_number_p(OBJ_oWhip) == 0) {
        if (PL.holdItem != NOONE) {
            if (PX(PL.holdItem).type == T_MACHETE || PX(PL.holdItem).type == T_MATTOCK) pitems_player(2037, i, 0);
        } else if (G.isTunnelMan) PUNTR(2038);
        else {
            int obj = pin_create(p->x - PI(16), p->y, OBJ_oWhip);
            pin_set_sprite(obj, GSPR_sWhipLeft);
            snd_play(SND_xwhip);                                               /* :446 */
        }
    } else if (spr_is_attack(p->spr) && PL.facing == RIGHT && DGT(p->img, 4) && instance_number_p(OBJ_oWhip) == 0) {
        if (PL.holdItem != NOONE) {
            if (PX(PL.holdItem).type == T_MACHETE || PX(PL.holdItem).type == T_MATTOCK) pitems_player(2037, i, 0);
        } else if (G.isTunnelMan) PUNTR(2038);
        else {
            int obj = pin_create(p->x + PI(16), p->y, OBJ_oWhip);
            pin_set_sprite(obj, GSPR_sWhipRight);
            snd_play(SND_xwhip);                                               /* :479 */
        }
    }
    if (PL.holdItem != NOONE) {                                                /* :484 */
        if (PX(PL.holdItem).type == T_MACHETE || PX(PL.holdItem).type == T_MATTOCK) {
            if (spr_is_attack(p->spr) && DLT(p->img, 2)) pitems_player(2039, i, 0);
        }
    } else if (p->spr == GSPR_sTunnelAttackL && DLT(p->img, 2) && instance_number_p(OBJ_oMattockPre) == 0) {
        pitems_player(2040, i, 0);
    } else if (spr_is_attack(p->spr) && PL.facing == LEFT && DLT(p->img, 2) && instance_number_p(OBJ_oWhipPre) == 0) {
        int obj = pin_create(p->x + PI(16), p->y, OBJ_oWhipPre);
        pin_set_sprite(obj, GSPR_sWhipPreL);
    } else if (spr_is_attack(p->spr) && PL.facing == RIGHT && DLT(p->img, 2) && instance_number_p(OBJ_oWhipPre) == 0) {
        int obj = pin_create(p->x - PI(16), p->y, OBJ_oWhipPre);
        pin_set_sprite(obj, GSPR_sWhipPreR);
    }
    if (!PL.whipping) {                                                        /* :537 */
        int16_t w[64];
        int n = pw_with(OBJ_oWhip, w, 64), k;
        for (k = 0; k < n; k++) if (PX(w[k]).alive) pin_destroy(w[k]);
        n = pw_with(OBJ_oWhipPre, w, 64);
        for (k = 0; k < n; k++) if (PX(w[k]).alive) pin_destroy(w[k]);
    }
    if (PL.holdItem != NOONE) {                                                /* :543 */
        if (PE(&PX(PL.holdItem))->cost > 0 && isLevel()) {
            int rp = G.roomPath[scrGetRoomX(PFLOOR(p->x))][scrGetRoomY(PFLOOR(p->y))];
            if (rp != 4 && rp != 5) {                                          /* P5 */
                scrStealItem();
                if (instance_exists_p(OBJ_oShopkeeper)) scrShopkeeperAnger(i, 0);
            }
        } else if (PE(&PX(PL.holdItem))->cost > 0)
            scrStealItem();
    }
    {
        double x = PTOD(p->x), y = PTOD(p->y);
        if (PL.kUp && PL.kAttackPressed && collision_point_p(x, y, OBJ_oChest, 0, NOONE) != NOONE)   /* :563 */
            open_chest(i);
        if (PL.kUp && PL.kAttackPressed && collision_point_p(x, y, OBJ_oCrate, 0, NOONE) != NOONE)   /* :614 */
            open_crate(i);
        if (PL.kUp && PL.kAttackPressed && collision_point_p(x, y, OBJ_oFlareCrate, 0, NOONE) != NOONE)
            pitems_player(2042, i, 0);
        if (!PL.dead && !PL.stunned && !PL.whipping && collision_point_p(x, y, OBJ_oXStart, 0, NOONE) != NOONE &&
            PL.kUp && platformCharacterIs(ON_GROUND) && !spr_is_exit(p->spr))
            enter_door(i);                                                     /* :676 */
        if (!PL.dead && !PL.stunned && !PL.whipping && collision_point_p(x, y, OBJ_oExit, 0, NOONE) != NOONE &&   /* :762 */
            PL.kUp && platformCharacterIs(ON_GROUND) && !spr_is_exit(p->spr))
            exit_level(i);
    }
    if ((GPp(K_ATTACK) || GPp(K_START)) && PL.dead) {                          /* :911 game over */
        if (PGAME.moneyCount < PG.money || PGAME.drawStatus < 3) {
            PGAME.drawStatus = 3;
            PGAME.moneyCount = PG.money;
        } else {
            if (GP.pressed & K_ATTACK) GP.pressed &= (uint16_t)~K_ATTACK;
            if (GP.pressed & K_START) GP.pressed &= (uint16_t)~K_START;
            play_goto_room = R_rHighscores;
        }
    }
    /* items :954 (inGame = isLevel()) */
    if (PL.dead || PL.stunned || !PL.active) {
    } else if (isLevel() && PL.kItemPressed && !PL.whipping) {                 /* :958 switch items */
        if (PL.holdItem != NOONE) {
            int h = PL.holdItem;
            if (PX(h).spr == GSPR_sBombArmed) {
            } else if (PX(h).spr == GSPR_sBomb) {
                PG.bombs += 1;
                pin_destroy(h);
                if (PG.rope > 0) {
                    PL.holdItem = pin_create(p->x, p->y, OBJ_oRopeThrow);
                    PE(&PX(PL.holdItem))->held = 1;
                    PG.rope -= 1;
                    PL.whoaTimer = PL.whoaTimerMax;
                } else
                    scrHoldItem(PL.pickupItemType);
            } else if (PX(h).spr == GSPR_sRopeEnd) {
                PG.rope += 1;
                pin_destroy(h);
                scrHoldItem(PL.pickupItemType);
            } else if (!PE(&PX(h))->heavy && PE(&PX(h))->cost == 0) {
                if (PG.bombs > 0 || PG.rope > 0) {
                    PL.pickupItemType = PX(h).type;
                    if (PX(h).type == T_BOW && PL.bowArmed) scrFireBow();
                    PE(&PX(h))->breakPieces = 0;
                    pin_destroy(h);
                }
                if (PG.bombs > 0) {
                    PL.holdItem = pin_create(p->x, p->y, OBJ_oBomb);
                    if (PG.hasStickyBombs) PE(&PX(PL.holdItem))->sticky = 1;
                    PE(&PX(PL.holdItem))->held = 1;
                    PG.bombs -= 1;
                    PL.whoaTimer = PL.whoaTimerMax;
                } else if (PG.rope > 0) {
                    PL.holdItem = pin_create(p->x, p->y, OBJ_oRopeThrow);
                    PE(&PX(PL.holdItem))->held = 1;
                    PG.rope -= 1;
                    PL.whoaTimer = PL.whoaTimerMax;
                }
            }
        } else {
            if (PG.bombs > 0) {
                PL.holdItem = pin_create(p->x, p->y, OBJ_oBomb);
                if (PG.hasStickyBombs) PE(&PX(PL.holdItem))->sticky = 1;
                PE(&PX(PL.holdItem))->held = 1;
                PG.bombs -= 1;
                PL.whoaTimer = PL.whoaTimerMax;
            } else if (PG.rope > 0) {
                PL.holdItem = pin_create(p->x, p->y, OBJ_oRopeThrow);
                PE(&PX(PL.holdItem))->held = 1;
                PG.rope -= 1;
                PL.whoaTimer = PL.whoaTimerMax;
            }
        }
    } else if (isLevel() && PL.kRopePressed && PG.rope > 0 && !PL.whipping) {  /* :1049 */
        if (!PL.kDown && PL.colTop) {
        } else if (PL.kDown) {
            int obj, t = 1;
            if (PL.facing == LEFT) obj = pin_create(p->x - PI(16), p->y, OBJ_oRopeThrow);
            else obj = pin_create(p->x + PI(16), p->y, OBJ_oRopeThrow);
            {   /* with obj */
                struct pin *o = &PX(obj);
                double px1 = PTOD(p->x), py1 = PTOD(p->y);
                move_snap(obj, 16, 1);
                if (p->x < o->x) {
                    if (!collision_point_any(px1 + 8, py1, OBJ_oSolid, 0, NOONE)) {
                        double ox = PTOD(o->x), oy = PTOD(o->y);
                        if (!collision_rect_any(ox - 8, oy, ox - 7, oy + 16, OBJ_oSolid, 0, NOONE)) pin_setx(o, o->x - (PI(8)));
                        else if (!collision_rect_any(ox + 7, oy, ox + 8, oy + 16, OBJ_oSolid, 0, NOONE)) pin_setx(o, o->x + (PI(8)));
                        else t = 0;
                    } else t = 0;
                } else if (!collision_point_any(px1 - 8, py1, OBJ_oSolid, 0, NOONE)) {
                    double ox = PTOD(o->x), oy = PTOD(o->y);
                    if (!collision_rect_any(ox + 7, oy, ox + 8, oy + 16, OBJ_oSolid, 0, NOONE)) pin_setx(o, o->x + (PI(8)));
                    else if (!collision_rect_any(ox - 8, oy, ox - 7, oy + 16, OBJ_oSolid, 0, NOONE)) pin_setx(o, o->x - (PI(8)));
                    else t = 0;
                } else t = 0;
                if (!t)
                    pin_destroy(obj);
                else {
                    pin_create(o->x, o->y, OBJ_oRopeTop);
                    o = &PX(obj);
                    PE(o)->armed = 0;
                    PE(o)->falling = 1;
                    PE(o)->xVel = 0;
                    PE(o)->yVel = 0;
                    PG.rope -= 1;
                    snd_play(SND_xthrow);                                      /* :1119 */
                }
            }
        } else {
            int obj = pin_create(p->x, p->y, OBJ_oRopeThrow);
            PE(&PX(obj))->armed = 1;
            PE(&PX(obj))->px = NP(p->x);
            PE(&PX(obj))->py = NP(p->y);
            PE(&PX(obj))->xVel = 0;
            PE(&PX(obj))->yVel = N(-12);
            PG.rope -= 1;
            snd_play(SND_xthrow);                                              /* :1132 */
        }
    } else if (isLevel() && PL.kBombPressed && PG.bombs > 0 && !PL.whipping && PL.bowArmed) {
        pitems_player(2044, i, 0);
    } else if (isLevel() && PL.kBombPressed && PG.bombs > 0 && !PL.whipping) {  /* :1141 */
        int obj = pin_create(p->x, p->y, OBJ_oBomb);
        struct pin *o = &PX(obj);
        if (PG.hasStickyBombs) PE(o)->sticky = 1;
        pin_set_sprite(obj, GSPR_sBombArmed);
        PE(o)->armed = 1;
        PE(o)->alarm[0] = 80;
        pin_setispd(o, (img_t)0.2);
        PE(o)->safe = 1;
        PE(o)->alarm[2] = 10;
        if (PL.facing == LEFT) PE(o)->xVel = N(-8) + PE(p)->xVel;
        else if (PL.facing == RIGHT) PE(o)->xVel = N(8) + PE(p)->xVel;
        PE(o)->yVel = N(-3);
        if (PL.kUp) PE(o)->yVel = N(-9);
        if (PL.kDown) {
            if (platformCharacterIs(ON_GROUND)) PE(o)->xVel = NMUL(PE(o)->xVel, N(0.1));
            PE(o)->yVel = N(3);
        }
        PG.bombs -= 1;
        snd_play(SND_xthrow);                                                  /* :1178 */
    } else if (PL.holdItem == NOONE) {                                         /* :1180 */
        if (PL.kAttackPressed && PL.state != DUCKING && PL.state != DUCKTOHANG && !PL.whipping &&
            p->spr != GSPR_sPExit && p->spr != GSPR_sDamselExit) {
            pin_setispd(p, (img_t)0.6);
            if (G.isTunnelMan || G.isDamsel) PUNTR(2045);
            else {
                pin_set_sprite(i, GSPR_sAttackLeft);
                pin_setimg(p, 0);
                PL.whipping = 1;
            }
        } else if (PL.kAttackPressed && PL.kDown) {                            /* :1209 pick up */
            double x = PTOD(p->x), y = PTOD(p->y);
            if (collision_rect_p(x - 8, y, x + 8, y + 8, OBJ_oItem, 0, NOONE) != NOONE) {
                int obj = instance_nearest_p(x, y, OBJ_oItem);
                if (PE(&PX(obj))->canPickUp && collision_point_p(PTOD(PX(obj).x), PTOD(PX(obj).y), OBJ_oSolid, 0, NOONE) == NOONE) {
                    int h = obj;
                    PL.holdItem = h;
                    PE(&PX(h))->held = 1;
                    PL.whoaTimer = PL.whoaTimerMax;
                    PL.pickupItemType = PX(h).type;
                    if (PX(h).type == T_BOW && PE(&PX(h))->New) {
                        PE(&PX(h))->New = 0;
                        PG.arrows += 6;
                    }
                    if (PX(h).type == T_GOLDIDOL && PE(&PX(h))->trigger && !isRoomIs(R_rLoadLevel)) {
                        if (G.levelType == 0) {
                            int trap = instance_nearest_p(x, y - 64, OBJ_oGiantTikiHead);
                            if (trap != NOONE) PE(&PX(trap))->alarm[0] = 100;
                            scrShake(100);
                            PE(&PX(h))->trigger = 0;
                        } else
                            ptemple_player(2046, i, 0);
                    } else if (PX(h).type == T_DAMSEL) {                       /* :1290 (P5) */
                        if (PE(&PX(h))->status == 4) {
                            PL.holdItem = NOONE;                       /* holdItem = 0; holdItem.held = false */
                        } else
                            pin_set_sprite(h, GSPR_sDamselHoldL);
                    } else if (PE(&PX(h))->cost == 0)
                        scrStealItem();
                }
            } else if (collision_rect_p(x - 8, y, x + 8, y + 8, OBJ_oEnemy, 0, NOONE) != NOONE)
                pen_player_pickup_enemy(i);                                    /* P5 hook (:1306) */
        }
    } else if (PL.kAttackPressed) {                                            /* :1319 */
        if (PL.holdItem != NOONE) {
            extern void scrUseItem(void);
            scrUseItem();
        }
    }
    if (isLevel() && PL.active && PL.kPayPressed && !PL.dead && !PL.stunned) { /* :1327 */
        pshop_pay(i);                                                          /* P5 hook (:1329) */
    }
    if (PL.kAttack && PL.bowArmed && NLT(PL.bowStrength, N(12))) pitems_player(2050, i, 0);      /* :1431 */
    if (PL.kAttackReleased && PL.bowArmed) scrFireBow();
    hurt_logic(i);                                                             /* :1450 */
    if ((PL.dead || PL.stunned) && PL.holdItem != NOONE) {                     /* :1684 */
        int h = PL.holdItem;
        PE(&PX(h))->held = 0;
        PE(&PX(h))->xVel = PE(p)->xVel;
        PE(&PX(h))->yVel = N(-6);
        PE(&PX(h))->armed = 1;
        if (PX(h).type == T_DAMSEL) PE(&PX(h))->status = 2;                          /* :1691 (P5) */
        else if (PX(h).type == T_BOW) scrFireBow();
        drop_or_switch();
    }
    if (PL.dead || PL.stunned) {                                               /* :1708 */
        if (instance_exists_p(OBJ_oParachute)) pitems_player(2052, i, 0);
        if (PL.whipping) {
            int16_t w[64];
            int n, k;
            PL.whipping = 0;
            n = pw_with(OBJ_oWhip, w, 64);
            for (k = 0; k < n; k++) if (PX(w[k]).alive) pin_destroy(w[k]);
        }
        if (G.isDamsel || G.isTunnelMan) PUNTR(2053);
        else {
            if (NEQ(PE(p)->xVel, N(0))) {
                if (PL.dead) pin_set_sprite(i, GSPR_sDieL);
                else if (PL.stunned) pin_set_sprite(i, GSPR_sStunL);
            } else if (PL.bounced) {
                if (NLT(PE(p)->yVel, N(0))) pin_set_sprite(i, GSPR_sDieLBounce);
                else pin_set_sprite(i, GSPR_sDieLFall);
            } else {
                if (NLT(PE(p)->xVel, N(0))) pin_set_sprite(i, GSPR_sDieLL);
                else pin_set_sprite(i, GSPR_sDieLR);
            }
        }
        if (collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oSpikes, 0, NOONE) != NOONE && PL.dead && NNE(PE(p)->yVel, N(0))) {
            if (RAND(1, 8) == 1) scrCreateBlood(i, p->x, p->y, 1);
        }
        if (isCollisionRight(i, 1) || isCollisionLeft(i, 1) || isCollisionBottom(i, 1)) {   /* :1785 */
            if (PL.wallHurt > 0) {
                int k;
                for (k = 0; k < 3; k++) pin_create(p->x, p->y, OBJ_oBlood);
                PG.plife -= 1;
                PL.wallHurt -= 1;
                snd_play(SND_xhurt);                                           /* :1795 */
            }
        }
        if (isCollisionBottom(i, 1) && !PL.bounced) {                          /* :1799 */
            int k;
            PL.bounced = 1;
            for (k = 0; k < 3; k++) scrCreateBlood(i, p->x, p->y, 1);
            if (PL.wallHurt > 0) {
                PG.plife -= 1;
                PL.wallHurt -= 1;
            }
        }
    }
    if (PL.dead && PL.deadCounter > 0) PL.deadCounter -= 1;                    /* :1818 */
    if (isLevel()) {
        if (play_god) play_god_hold();
        if (!PL.dead && PG.plife < 1) {
            if (PG.hasAnkh) pitems_player(2054, i, 0);
            else {
                PG.plife = 0;
                PG.drawHUD = 0;
                scrUpdateHighscores(0);                                        /* :1886 (no minigame rooms) */
                PL.dead = 1;
                snd_play(SND_xdie);                                            /* :1890 */
            }
        }
        if (PL.dead) snd_stop_music();                                         /* :1896 */
    }
    if (!PL.dead && PL.invincible > 0) PL.invincible -= 1;                     /* :1903 */
    if (PL.blink > 0) {
        PL.blinkToggle *= -1;
        PL.blink -= 1;
    } else
        PL.blinkToggle = -1;
    PL.money = PG.money;                                                       /* :1927 */
    if (PG.collectCounter == 0) {
        if (PG.collect > 100) {
            PG.money += 100;
            PG.collect -= 100;
        } else {
            PG.money += PG.collect;
            PG.collect -= PG.collect;
        }
    } else
        PG.collectCounter -= 1;
    {
        double x = PTOD(p->x), y = PTOD(p->y);
        if (PL.holdItem != NOONE && PX(PL.holdItem).type == T_BOW) pitems_player(2055, i, 0);
        if (collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oTreasure, 0, NOONE) != NOONE && !PL.dead && !PL.stunned) {   /* :1964 */
            int gem = instance_nearest_p(x, y, OBJ_oTreasure);
            if (PE(&PX(gem))->canCollect) {
                int v = PE(&PX(gem))->value;
                PG.collect += v + dceil(v / 4.0) * G.levelType;
                PG.collectCounter += 20;
                if (PG.collectCounter > 100) PG.collectCounter = 100;
                switch (PX(gem).type) {                                        /* :1976 */
                case T_GOLDCHUNK: PG.gold += 1; break;
                case T_GOLDNUGGET: PG.nuggets += 1; break;
                case T_GOLDBAR: PG.goldbar += 1; break;
                case T_GOLDBARS: PG.goldbars += 1; break;
                case T_EMERALD: PG.emeralds += 1; break;
                case T_BIGEMERALD: PG.bigemeralds += 1; break;
                case T_SAPPHIRE: PG.sapphires += 1; break;
                case T_BIGSAPPHIRE: PG.bigsapphires += 1; break;
                case T_RUBY: PG.rubies += 1; break;
                case T_BIGRUBY: PG.bigrubies += 1; break;
                case T_DIAMOND: PG.diamonds += 1; break;
                }
                switch (PX(gem).type) {                                        /* :1987 coin */
                case T_GOLDCHUNK: case T_GOLDNUGGET: case T_GOLDBAR: case T_GOLDBARS: snd_play(SND_xcoin); break;
                default: snd_play(SND_xgem); break;
                }
                pin_destroy(gem);
            }
        }
        if (collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oBombBag, 0, NOONE) != NOONE && !PL.dead && !PL.stunned) {   /* :1994 */
            int obj = collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oBombBag, 0, NOONE);
            if (!PE(&PX(obj))->held && PE(&PX(obj))->cost == 0 && collision_point_p(PTOD(PX(obj).x), PTOD(PX(obj).y), OBJ_oSolid, 0, NOONE) == NOONE) {
                int d;
                PG.bombs += 3;
                d = pin_create(PX(obj).x, PX(obj).y - PI(14), OBJ_oItemsGet);
                pin_set_sprite(d, GSPR_sBombsGet);
                pin_destroy(obj);
                snd_play(SND_xpickup);                                         /* :2003 / :2019 */
                pmsg_player_str("YOU GOT 3 MORE BOMBS!", "", 120);
            }
        }
        if (collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oBombBox, 0, NOONE) != NOONE && !PL.dead && !PL.stunned) {
            int obj = collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oBombBox, 0, NOONE);
            if (!PE(&PX(obj))->held && PE(&PX(obj))->cost == 0 && collision_point_p(PTOD(PX(obj).x), PTOD(PX(obj).y), OBJ_oSolid, 0, NOONE) == NOONE) {
                int d;
                PG.bombs += 12;
                d = pin_create(PX(obj).x, PX(obj).y - PI(14), OBJ_oItemsGet);
                pin_set_sprite(d, GSPR_sBombsGet);
                pin_destroy(obj);
                snd_play(SND_xpickup);                                         /* :2003 / :2019 */
                pmsg_player_str("YOU GOT 12 MORE BOMBS!", "", 120);
            }
        }
        if (collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oRopePile, 0, NOONE) != NOONE && !PL.dead && !PL.stunned) {
            int obj = collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oRopePile, 0, NOONE);
            if (!PE(&PX(obj))->held && PE(&PX(obj))->cost == 0 && collision_point_p(PTOD(PX(obj).x), PTOD(PX(obj).y), OBJ_oSolid, 0, NOONE) == NOONE) {
                int d;
                PG.rope += 3;
                d = pin_create(PX(obj).x, PX(obj).y - PI(15), OBJ_oItemsGet);
                pin_set_sprite(d, GSPR_sRopeGet);
                pin_destroy(obj);
                snd_play(SND_xpickup);                                         /* :2035 */
                pmsg_player_str("YOU GOT 3 MORE ROPES!", "", 120);
            }
        }
        if (collision_point_p(x, y, OBJ_oExit, 0, NOONE) != NOONE) {           /* :2042 */
            if (PL.holdItem != NOONE) {
                int h = PL.holdItem;
                if (PX(h).type == T_GOLDIDOL) {
                    PG.collect += PE(&PX(h))->value * (G.levelType + 1);
                    PG.collectCounter += 20;
                    if (PG.collectCounter > 100) PG.collectCounter = 100;
                    if (PX(h).spr == GSPR_sCrystalSkull) PG.skulls += 1;
                    else PG.idols += 1;
                    snd_play(SND_xcoin);                                       /* :2055 */
                    pin_create(p->x, p->y - PI(8), OBJ_oBigCollect);
                    pin_destroy(h);
                    PL.holdItem = NOONE;
                } else if (PX(h).type == T_DAMSEL) {                           /* :2060 (P5) */
                    if (PE(&PX(h))->active && PE(&PX(h))->hp > 0) {
                        int door = instance_place_p(i, PTOD(p->x), PTOD(p->y), OBJ_oExit);
                        PG.damsels += 1;
                        PG.xdamsels += 1;
                        pin_setx(&PX(h), PX(door).x + PI(8));
                        pin_sety(&PX(h), PX(door).y + PI(8));
                        pin_set_sprite(h, GSPR_sDamselExit2);
                        PE(&PX(h))->status = 4;
                        PE(&PX(h))->held = 0;
                        PE(&PX(h))->xVel = 0;
                        PE(&PX(h))->yVel = 0;
                        snd_play(SND_xsteps);                                  /* :2078 */
                        pin_setdepth(&PX(h), 1000);
                        PE(&PX(h))->active = 0;
                        PE(&PX(h))->canPickUp = 0;
                        PL.holdItem = NOONE;
                    }
                }
            }
        }
    }
    PG.xmoney += PG.money - PL.money;
    if (play_toggle_run_on && GPp(K_RUN)) play_toggle_run = !play_toggle_run;  /* :2093 */
}

/* objects/oPlayer1/Step_2.gml */
void pl_end_step(int i)
{
    struct pin *p = &PX(i);
    if (PL.holdItem != NOONE) {
        if (PL.state == CLIMBING && (PG.hasJetpack || PG.hasCape)) pin_setdepth(&PX(PL.holdItem), 51);
        else pin_setdepth(&PX(PL.holdItem), 0);
    }
    if (PL.state == DUCKTOHANG && p->spr != GSPR_sDuckToHangL && p->spr != GSPR_sDamselDtHL && p->spr != GSPR_sTunnelDtHL)
        PL.state = STANDING;
}

/* objects/oPlayer1/Alarm_*.gml: messages and sounds, except alarm 10 (jetpack) and 11 (bomb arrow) */
void pl_alarm(int i, int a)
{
    struct pin *p = &PX(i);
    switch (a) {
    case 0:                                                                    /* Alarm_0: messages */
        if (!isRoomIs(R_rTutorial)) {
            if (G.darkLevel) {
                if (PG.hasCrown) pmsg_player_str("THE HEDJET SHINES BRIGHTLY.", "", 200);
                else pmsg_player_str("I CAN'T SEE A THING!", "I'D BETTER USE THESE FLARES!", 200);
                PE(p)->alarm[1] = 210;
                break;
            }
            if (G.blackMarket) {
                pmsg_player_str("WELCOME TO THE BLACK MARKET!", "", 200);
                PE(p)->alarm[1] = 210;
                break;
            }
            if (G.snakePit) { pmsg_player_str("I HEAR SNAKES... I HATE SNAKES!", "", 200); break; }
            if (G.cemetary) {
                pmsg_player_str("THE DEAD ARE RESTLESS!", "", 200);
                if (G.lake) PE(p)->alarm[1] = 210;
                break;
            }
            if (G.lake) { pmsg_player_str("I CAN HEAR RUSHING WATER...", "", 200); break; }
            if (G.yetiLair) { pmsg_player_str("IT SMELLS LIKE WET FUR IN HERE!", "", 200); break; }
            if (G.alienCraft) { pmsg_player_str("THERE'S A PSYCHIC PRESENCE HERE!", "", 200); break; }
            if (G.cityOfGold) {
                pmsg_player_str("IT'S THE LEGENDARY CITY OF GOLD!", "", 200);
                if (G.sacrificePit) PE(p)->alarm[1] = 210;
                break;
            }
            if (G.sacrificePit) { pmsg_player_str("I CAN HEAR PRAYERS TO KALI!", "", 200); break; }
        }
        pmsg_player_again(200);                                                /* :56 message1 / 2 as they are */
        break;
    case 1:
        if (!isRoomIs(R_rTutorial)) {
            if (G.snakePit) { pmsg_player_str("I HEAR SNAKES... I HATE SNAKES!", "", 200); break; }
            if (G.cemetary && G.darkLevel) {
                pmsg_player_str("THE DEAD ARE RESTLESS!", "", 200);
                if (G.lake) PE(p)->alarm[4] = 210;
                break;
            }
            if (G.lake) { pmsg_player_str("I CAN HEAR RUSHING WATER...", "", 200); break; }
            if (G.yetiLair) { pmsg_player_str("THERE'S A PSYCHIC PRESENCE HERE!", "", 200); break; }   /* sic */
            if (G.alienCraft) { pmsg_player_str("IT'S THE LEGENDARY CITY OF GOLD!", "", 200); break; } /* sic */
            if (G.cityOfGold) {
                pmsg_player_str("IT'S THE LEGENDARY CITY OF GOLD!", "", 200);
                if (G.sacrificePit) PE(p)->alarm[4] = 210;
                break;
            }
            if (G.sacrificePit) { pmsg_player_str("I CAN HEAR PRAYERS TO KALI!", "", 200); break; }
        }
        pmsg_player_again(200);                                                /* :42 */
        break;
    case 2:
        snd_play(PL.climbSndToggle ? SND_xclimb1 : SND_xclimb2);
        PL.climbSndToggle = !PL.climbSndToggle;
        break;
    case 3:                                   /* global.sndStep1 / sndStep2: never assigned (initMusic): undefined */
        snd_play(-1);
        PL.walkSndToggle = !PL.walkSndToggle;
        break;
    case 4:                                                                    /* Alarm_4 */
        if (G.lake) pmsg_str("YOU HEAR RUSHING WATER...", "", 200);
        else if (G.sacrificePit) pmsg_str("I CAN HEAR PRAYERS TO KALI!", "", 200);
        break;
    case 10: pitems_player(2060, i, a); break;
    case 11:
        if (PL.holdArrow > 0) {
            PL.holdArrowToggle = !PL.holdArrowToggle;
            PE(p)->alarm[11] = 1;
        }
        break;
    }
}

/* objects/oPlayer1/Other_7.gml */
void pl_animend(int i)
{
    struct pin *p = &PX(i);
    if (spr_is_attack(p->spr)) {
        PL.whipping = 0;
        if (PL.holdItem != NOONE) pin_setvisible(&PX(PL.holdItem), 1);
    } else if (p->spr == GSPR_sDuckToHangL || p->spr == GSPR_sDamselDtHL || p->spr == GSPR_sTunnelDtHL) {
        int obj;
        pin_sety(p, p->y + PI(16));
        move_snap(i, 1, 8);
        PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;
        PE(p)->grav = 0;
        if (PL.facing == LEFT) {
            obj = collision_point_p(PTOD(p->x) - 8, PTOD(p->y), OBJ_oLadder, 0, NOONE);
            if (obj == NOONE) obj = collision_point_p(PTOD(p->x) - 8, PTOD(p->y), OBJ_oLadderTop, 0, NOONE);
        } else {
            obj = collision_point_p(PTOD(p->x) + 8, PTOD(p->y), OBJ_oLadder, 0, NOONE);
            if (obj == NOONE) obj = collision_point_p(PTOD(p->x) + 8, PTOD(p->y), OBJ_oLadderTop, 0, NOONE);
        }
        if (obj != NOONE) {
            PL.state = CLIMBING;
            pin_setx(p, PX(obj).x + PI(8));
        } else if (PL.facing == LEFT) {
            PL.state = HANGING;
            PL.facing = RIGHT;
            pin_setx(p, p->x - PI(6));
            pin_setx(p, p->x + (PI(1)));
        } else {
            PL.state = HANGING;
            PL.facing = LEFT;
            pin_setx(p, p->x + PI(6));
        }
    } else if (spr_is_exit(p->spr)) {
        if (PG.collect > 0) {
            PG.money += PG.collect;
            PG.collect = 0;
        }
        if (PL.pExit == 0 && G.gameStart) {                                    /* xSTART */
            G.gameStart = 0;
            if (G.currLevel == 16) G.cityOfGold = 0;
            if (G.currLevel == 5) play_goto_room = R_rTransition1x;
            else if (G.currLevel == 9) play_goto_room = R_rTransition2x;
            else if (G.currLevel == 13) play_goto_room = R_rTransition3x;
            else if (G.levelType == 1) play_goto_room = R_rTransition2;
            else if (G.levelType == 2) play_goto_room = R_rTransition3;
            else if (G.levelType == 3 || G.levelType == 4) play_goto_room = R_rTransition4;
            else play_goto_room = R_rTransition1;
        } else
            ptemple_player(2061, i, 0);
        G.cleanSolids = 1;
    }
}

/* objects/oPlayer1/Collision_oBlood.gml, Collision_oPushBlock.gml */
void pl_collision(int i, int other)
{
    struct pin *p = &PX(i);
    if (obj_is(PX(other).obj, OBJ_oBlood)) {
        if (PG.hasKapala && PE(&PX(other))->collectible) pitems_player(2062, i, other);
    } else if (obj_is(PX(other).obj, OBJ_oPushBlock)) {
        double dx = PTOD(p->x) - (PTOD(PX(other).x) + 8), dy = PTOD(p->y) - (PTOD(PX(other).y) + 8);
        if (dx * dx + dy * dy < 121 && p->y >= PX(other).y)
            pin_setx(p, PE(p)->xprev);
    }
}

/* scripts/characterDrawEvent: the drawing sets image_xscale from facing */
void pl_draw(int i)
{
    pin_setxscale(&PX(i), PL.facing == RIGHT ? -1 : 1);
}

static int scrPlayerIsDucking(int i)
{
    int s = PX(i).spr;
    return s == GSPR_sDuckLeft || s == GSPR_sCrawlLeft || s == GSPR_sDamselDuckL || s == GSPR_sDamselCrawlL ||
           s == GSPR_sTunnelDuckL || s == GSPR_sTunnelCrawlL;
}

/* scripts/scrUseItem (called by oPlayer1 with holdItem set) */
void scrUseItem(void)
{
    int i = PL.idx, h = PL.holdItem;
    struct pin *p = &PX(i), *o = &PX(h);
    if (o->spr == GSPR_sBomb) {                                                /* :20 */
        pin_set_sprite(h, GSPR_sBombArmed);
        PE(o)->armed = 1;
        PE(o)->alarm[0] = 80;
        pin_setispd(o, (img_t)0.2);
    } else if (o->spr == GSPR_sRopeEnd) {                                      /* :31 */
        if (!PL.kDown && PL.colTop) {
        } else {
            PE(o)->held = 0;
            PE(o)->armed = 1;
            PE(o)->px = NP(p->x);
            PE(o)->py = NP(p->y);
            if (PL.kDown) {
                int obj, t = 1;
                if (PL.facing == LEFT) obj = pin_create(p->x - PI(16), p->y, OBJ_oRopeThrow);
                else obj = pin_create(p->x + PI(16), p->y, OBJ_oRopeThrow);
                {
                    struct pin *r = &PX(obj);
                    double ox, oy;
                    move_snap(obj, 16, 1);
                    ox = PTOD(r->x);
                    oy = PTOD(r->y);
                    if (p->x < r->x && !collision_point_any(PTOD(p->x) + 2, PTOD(p->y), OBJ_oSolid, 0, NOONE)) {
                        if (!collision_rect_any(ox - 8, oy, ox - 7, oy + 16, OBJ_oSolid, 0, NOONE)) pin_setx(r, r->x - (PI(8)));
                        else if (!collision_rect_any(ox + 7, oy, ox + 8, oy + 16, OBJ_oSolid, 0, NOONE)) pin_setx(r, r->x + (PI(8)));
                        else t = 0;
                    } else if (!collision_point_any(PTOD(p->x) - 2, PTOD(p->y), OBJ_oSolid, 0, NOONE)) {
                        if (!collision_rect_any(ox + 7, oy, ox + 8, oy + 16, OBJ_oSolid, 0, NOONE)) pin_setx(r, r->x + (PI(8)));
                        else if (!collision_rect_any(ox - 8, oy, ox - 7, oy + 16, OBJ_oSolid, 0, NOONE)) pin_setx(r, r->x - (PI(8)));
                        else t = 0;
                    }
                    if (!t) {
                        int o2 = pin_create(PX(PL.holdItem).x, PX(PL.holdItem).y, OBJ_oRopeThrow);
                        if (PL.facing == LEFT) PE(&PX(o2))->xVel = N(-3.2);
                        else PE(&PX(o2))->xVel = N(3.2);
                        PE(&PX(o2))->yVel = N(0.5);
                        pin_destroy(obj);
                    } else {
                        pin_create(PX(obj).x, PX(obj).y, OBJ_oRopeTop);
                        r = &PX(obj);
                        PE(r)->armed = 0;
                        PE(r)->falling = 1;
                        PE(r)->xVel = 0;
                        PE(r)->yVel = 0;
                    }
                }
                pin_destroy(PL.holdItem);
                PL.holdItem = NOONE;
            } else {
                pin_setx(o, p->x);
                PE(o)->xVel = 0;
                PE(o)->yVel = N(-12);
            }
            scrHoldItem(PL.pickupItemType);
            snd_play(SND_xthrow);                                              /* scrUseItem :119 */
        }
    } else if (o->type == T_MACHETE || o->type == T_MATTOCK || o->type == T_PISTOL || o->type == T_SCEPTRE ||
               o->type == T_WEBCANNON || o->type == T_TELEPORTER || o->type == T_BOW || o->type == T_SHOTGUN) {
        pitems_player(2070, PL.idx, 0);
        return;
    } else {                                                                   /* :594 throw */
        if (o->type == T_DAMSEL) {                                             /* scrUseItem :596 (P5) */
            PE(o)->status = 2;
            PE(o)->counter = PEN(o)->stunMax;
            pin_sety(o, o->y - (PI(4)));
            snd_play(SND_xdamsel);                                             /* :601 */
        }
        PE(o)->held = 0;
        PE(o)->safe = 1;
        PE(o)->alarm[2] = 10;
        if (PL.facing == LEFT) {
            if (PE(o)->heavy) PE(o)->xVel = N(-4) + PE(p)->xVel;
            else PE(o)->xVel = N(-8) + PE(p)->xVel;
            if (collision_point_any(PTOD(p->x) - 8, PTOD(p->y), OBJ_oSolid, 0, NOONE)) pin_setx(o, o->x + (PI(8)));
        } else if (PL.facing == RIGHT) {
            if (PE(o)->heavy) PE(o)->xVel = N(4) + PE(p)->xVel;
            else PE(o)->xVel = N(8) + PE(p)->xVel;
            if (collision_point_any(PTOD(p->x) + 8, PTOD(p->y), OBJ_oSolid, 0, NOONE)) pin_setx(o, o->x - (PI(8)));
        }
        if (PE(o)->heavy) PE(o)->yVel = N(-2);
        else PE(o)->yVel = N(-3);
        if (PL.kUp) {
            if (PE(o)->heavy) PE(o)->yVel = N(-4);
            else PE(o)->yVel = N(-9);
        }
        if (PL.kDown) {
            if (platformCharacterIs(ON_GROUND)) {
                pin_sety(o, o->y - (PI(2)));
                PE(o)->xVel = NMUL(PE(o)->xVel, N(0.6));
                PE(o)->yVel = N(0.5);
            } else
                PE(o)->yVel = N(3);
        } else if (!PG.hasMitt) {
            if (PL.facing == LEFT) {
                if (collision_point_any(PTOD(p->x) - 8, PTOD(p->y) - 10, OBJ_oSolid, 0, NOONE)) {
                    PE(o)->yVel = 0;
                    PE(o)->xVel -= N(1);
                }
            } else if (PL.facing == RIGHT) {
                if (collision_point_any(PTOD(p->x) + 8, PTOD(p->y) - 10, OBJ_oSolid, 0, NOONE)) {
                    PE(o)->yVel = 0;
                    PE(o)->xVel += N(1);
                }
            }
        }
        if (PG.hasMitt && !scrPlayerIsDucking(i)) pitems_player(2072, PL.idx, 0);
        if (o->spr == GSPR_sBombArmed) scrHoldItem(PL.pickupItemType);
        else PL.holdItem = NOONE;
        snd_play(SND_xthrow);                                                  /* :679 */
    }
    if (PL.kDown && PL.holdItem != NOONE) {                                    /* :679 */
        pin_setx(&PX(PL.holdItem), p->x);
        pin_sety(&PX(PL.holdItem), p->y);
    }
    if (PL.holdItem == NOONE) PL.pickupItemType = T_NONE;
}
