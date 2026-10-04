/* oPlayer1: its events (refs/hd/src/objects/oPlayer1/<event>.gml) and the scripts they call (characterStepEvent,
 * characterSprite, characterDrawEvent; scripts/<name>/<name>.gml), translated statement for statement.
 * Line numbers: Step_0.gml unless named. GML read as GameMaker 2024.14 runs it: `and` / `or` short-circuit,
 * function arguments evaluated last to first, `other` outside a with / collision is self.
 * Untranslated GML that P4's routes do not reach (items and rooms of later milestones) sets play_untranslated.
 */
#include "pint.h"

struct player PL;

#define ME (PX(PL.idx))
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
    p->grav = N(1);
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
    PL.gravityIntensity = p->grav;
    PL.jumpTime = PL.jumpTimeTotal;
    PL.jumpButtonReleased = 0;
    PL.ladderTimer = 0;
    PL.jumps = 0;
    PL.kLeftPushedSteps = PL.kRightPushedSteps = 0;
    p->xVel = p->yVel = p->xAcc = p->yAcc = 0;                                  /* makeActive */
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
    p->myGrav = N(0.6);
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
            if (collision_point_p(PTOD(p->x) - 2, PTOD(p->y) + 9, OBJ_oSolid, 0, NOONE) == NOONE) {
                p->ispd = (img_t)0.6;
                pin_set_sprite(i, GSPR_sWhoaLeft);
            } else
                pin_set_sprite(i, GSPR_sStandLeft);
        }
        if (PL.state == RUNNING) {
            if (PL.kUp) pin_set_sprite(i, GSPR_sLookRunL);
            else pin_set_sprite(i, GSPR_sRunLeft);
        }
        if (PL.state == DUCKING) {
            if (NEQ(p->xVel, N(0))) pin_set_sprite(i, GSPR_sDuckLeft);
            else if (NLT(NABS(p->xVel), N(3))) pin_set_sprite(i, GSPR_sCrawlLeft);
            else pin_set_sprite(i, GSPR_sRunLeft);
        }
        if (PL.state == LOOKING_UP) {
            if (NGT(NABS(p->xVel), N(0))) pin_set_sprite(i, GSPR_sLookRunL);
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
    p->yVel = 0;
    p->yAcc = 0;
    p->grav = 0;
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
    if (GPd(K_RUN)) {                                                          /* :120 checkRun() */
        PL.runHeld = 100;
        PL.runKey = 1;
    }
    if (GPd(K_ATTACK) && !PL.whipping) {                                       /* :126 */
        PL.runHeld += 1;
        PL.runKey = 1;
    }
    if (!PL.runKey || (!PL.kLeft && !PL.kRight)) PL.runHeld = 0;              /* Linux build's :132-139 */

    if (PL.state != CLIMBING && PL.state != HANGING) {                         /* :143 */
        if (PL.kLeftReleased && approximatelyZero(p->xVel)) p->xAcc -= N(0.5);
        if (PL.kRightReleased && approximatelyZero(p->xVel)) p->xAcc += N(0.5);
        if (PL.kLeft && !PL.kRight) {
            if (PL.colSolidLeft) {
                if (platformCharacterIs(ON_GROUND) && PL.state != DUCKING) {
                    p->xAcc -= N(1);
                    PL.pushTimer += 10;
                }
            } else if (PL.kLeftPushedSteps > 2 && (PL.facing == LEFT || approximatelyZero(p->xVel)))
                p->xAcc -= PL.runAcc;
            PL.facing = LEFT;
        }
        if (PL.kRight && !PL.kLeft) {
            if (PL.colSolidRight) {
                if (platformCharacterIs(ON_GROUND) && PL.state != DUCKING) {
                    p->xAcc += N(1);
                    PL.pushTimer += 10;
                }
            } else if ((PL.kRightPushedSteps > 2 || PL.colSolidLeft) && (PL.facing == RIGHT || approximatelyZero(p->xVel)))
                p->xAcc += PL.runAcc;
            PL.facing = RIGHT;
        }
        NOPS(4);
    }

    /* LADDERS :195 */
    if (PL.state == CLIMBING) {
        if (instance_exists_p(OBJ_oCape)) PUNTR(2002);
        PL.kJumped = 0;
        PL.ladderTimer = 10;
        PL.ladder = collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oLadder, 0, NOONE);
        if (PL.ladder != NOONE) p->x = PX(PL.ladder).x + PI(8);
        if (PL.kLeft) PL.facing = LEFT;
        else if (PL.kRight) PL.facing = RIGHT;
        if (PL.kUp) {
            if (collision_point_p(PTOD(p->x), PTOD(p->y) - 8, OBJ_oLadder, 0, NOONE) != NOONE ||
                collision_point_p(PTOD(p->x), PTOD(p->y) - 8, OBJ_oLadderTop, 0, NOONE) != NOONE) {
                p->yAcc -= PL.climbAcc;
                if (p->alarm[2] < 1) p->alarm[2] = 8;
            }
        } else if (PL.kDown) {
            if (collision_point_p(PTOD(p->x), PTOD(p->y) + 8, OBJ_oLadder, 0, NOONE) != NOONE ||
                collision_point_p(PTOD(p->x), PTOD(p->y) + 8, OBJ_oLadderTop, 0, NOONE) != NOONE) {
                p->yAcc += PL.climbAcc;
                if (p->alarm[2] < 1) p->alarm[2] = 8;
            } else
                PL.state = FALLING;
            if (PL.colBot) PL.state = STANDING;
        }
        if (PL.kJumpPressed && !PL.whipping) {
            if (PL.kLeft) p->xVel = -PL.departLadderXVel;
            else if (PL.kRight) p->xVel = PL.departLadderXVel;
            else p->xVel = 0;
            p->yAcc += PL.departLadderYVel;
            PL.state = JUMPING;
            PL.jumpButtonReleased = 0;
            PL.jumpTime = 0;
            PL.ladderTimer = 5;
        }
    } else if (PL.ladderTimer > 0)
        PL.ladderTimer -= 1;

    if (platformCharacterIs(IN_AIR) && PL.state != HANGING)                    /* :248 */
        p->yAcc += PL.gravityIntensity;

    if ((PL.colBot || PL.colPlatBot) && platformCharacterIs(IN_AIR) && NGE(p->yVel, N(0))) {   /* :254 */
        if (!PL.colPlat || PL.colBot) {
            p->yVel = 0;
            p->yAcc = 0;
            PL.state = RUNNING;
            PL.jumps = 0;
        }
    }
    if ((PL.colBot || PL.colPlatBot) && !PL.colPlat) p->yVel = 0;             /* :265 */

    if (PL.colBot == 0 && (!PL.colPlatBot || PL.colPlat) && platformCharacterIs(ON_GROUND)) {   /* :268 */
        PL.state = FALLING;
        p->yAcc += p->grav;
        PL.kJumped = 1;
        if (PG.hasGloves) PL.hangCount = 5;
    }

    if (PL.colTop) {                                                           /* :276 */
        if (PL.dead || PL.stunned) p->yVel = NMUL(-p->yVel, N(0.8));
        else if (PL.state == JUMPING) p->yVel = NABS(NMUL(p->yVel, N(0.3)));
    }

    if ((PL.colLeft && PL.facing == LEFT) || (PL.colRight && PL.facing == RIGHT)) {     /* :282 */
        if (PL.dead || PL.stunned) p->xVel = NMUL(-p->xVel, N(0.5));
        else p->xVel = 0;
    }

    /* JUMPING :294 */
    if (PL.kJumpReleased && platformCharacterIs(IN_AIR))
        PL.kJumped = 1;
    else if (platformCharacterIs(ON_GROUND)) {
        if (instance_exists_p(OBJ_oCape)) PUNTR(2003);
        PL.kJumped = 0;
    }

    if (PL.kJumpPressed && collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oWeb, 0, NOONE) != NOONE) {   /* :308 */
        int obj = instance_place_p(i, PTOD(p->x), PTOD(p->y), OBJ_oWeb);
        if (obj != NOONE) PX(obj).life -= N(1);
        else PUNTR(2004);
        p->yAcc += NMULI(PL.initialJumpAcc, 2);
        p->yVel -= N(3);
        p->xAcc += NDIV(p->xVel, N(2));
        PL.state = JUMPING;
        PL.jumpButtonReleased = 0;
        PL.jumpTime = 0;
        p->grav = PL.gravNorm;
    } else if (PL.kJumpPressed && PL.colWaterTop) {
        p->yAcc += NMULI(PL.initialJumpAcc, 2);
        p->yVel -= N(3);
        p->xAcc += NDIV(p->xVel, N(2));
        PL.state = JUMPING;
        PL.jumpButtonReleased = 0;
        PL.jumpTime = 0;
        p->grav = PL.gravNorm;
    } else if (PG.hasCape && PL.kJumpPressed && PL.kJumped && platformCharacterIs(IN_AIR)) {
        PUNTR(2005);
    } else if (PG.hasJetpack && PL.kJump && PL.kJumped && platformCharacterIs(IN_AIR) && PL.jetpackFuel > 0) {
        PUNTR(2006);
    } else if (platformCharacterIs(ON_GROUND) && PL.kJumpPressed && PL.fallTimer == 0) {   /* :352 */
        if (NGT(p->xVel, N(3)) || NLT(p->xVel, N(-3))) {
            p->yAcc += NMULI(PL.initialJumpAcc, 2);
            p->xAcc += NMULI(p->xVel, 2);
        } else {
            p->yAcc += NMULI(PL.initialJumpAcc, 2);
            p->xAcc += NDIV(p->xVel, N(2));
        }
        if (PG.hasJordans) {
            p->yAcc = NMULI(p->yAcc, 3);
            PL.yAccLimit = N(12);
            p->grav = N(0.5);
        } else if (PG.hasSpringShoes)
            p->yAcc = NMUL(p->yAcc, N(1.5));
        else {
            PL.yAccLimit = N(6);
            p->grav = PL.gravNorm;
        }
        PL.pushTimer = 0;
        PL.state = FALLING;
        PL.jumpButtonReleased = 0;
        PL.jumpTime = 0;
    }

    if (PL.jumpTime < PL.jumpTimeTotal) PL.jumpTime += 1;                       /* :389 */
    if (PL.kJump == 0) PL.jumpButtonReleased = 1;
    if (PL.jumpButtonReleased) PL.jumpTime = PL.jumpTimeTotal;
    /* gravityIntensity = (jumpTime / jumpTimeTotal) * grav :394 */
    PL.gravityIntensity = NMUL(NDIV(NI(PL.jumpTime), NI(PL.jumpTimeTotal)), p->grav);
    NOPS(2);

    if (PL.kUp && platformCharacterIs(ON_GROUND) && !PL.colLadder) {           /* :396 */
        PL.looking = UP;
        if (NEQ(p->xVel, N(0)) && NEQ(p->xAcc, N(0))) PL.state = LOOKING_UP;
    } else
        PL.looking = 0;
    if (!PL.kUp && PL.state == LOOKING_UP)
        PL.state = STANDING;

    /* HANGING :414 */
    x = PTOD(p->x);
    y = PTOD(p->y);
    if (!PL.colTop) {
        if (PG.hasGloves && NGT(p->yVel, N(0))) {
            if (PL.hangCount == 0 && y > 16 && !platformCharacterIs(ON_GROUND) && PL.kRight && PL.colRight &&
                (collision_point_p(x + 9, y - 5, OBJ_oSolid, 0, NOONE) != NOONE ||
                 collision_point_p(x + 9, y - 6, OBJ_oSolid, 0, NOONE) != NOONE))
                set_hang(p, i);
            else if (PL.hangCount == 0 && y > 16 && !platformCharacterIs(ON_GROUND) && PL.kLeft && PL.colLeft &&
                (collision_point_p(x - 9, y - 5, OBJ_oSolid, 0, NOONE) != NOONE ||
                 collision_point_p(x - 9, y - 6, OBJ_oSolid, 0, NOONE) != NOONE))
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
                 (collision_point_p(x + 9, y - 5, OBJ_oSolid, 0, NOONE) != NOONE ||
                  collision_point_p(x + 9, y - 6, OBJ_oSolid, 0, NOONE) != NOONE) &&
                 collision_point_p(x + 9, y - 9, OBJ_oSolid, 0, NOONE) == NOONE &&
                 collision_point_p(x, y + 9, OBJ_oSolid, 0, NOONE) == NOONE)
            set_hang(p, i);
        else if (PL.hangCount == 0 && y > 16 && !platformCharacterIs(ON_GROUND) && PL.kLeft && PL.colLeft &&
                 (collision_point_p(x - 9, y - 5, OBJ_oSolid, 0, NOONE) != NOONE ||
                  collision_point_p(x - 9, y - 6, OBJ_oSolid, 0, NOONE) != NOONE) &&
                 collision_point_p(x - 9, y - 9, OBJ_oSolid, 0, NOONE) == NOONE &&
                 collision_point_p(x, y + 9, OBJ_oSolid, 0, NOONE) == NOONE)
            set_hang(p, i);
        x = PTOD(p->x);
        y = PTOD(p->y);
        if (PL.hangCount == 0 && y > 16 && !platformCharacterIs(ON_GROUND) && PL.state == FALLING &&   /* :476 */
            (collision_point_p(x, y - 5, OBJ_oArrow, 0, NOONE) != NOONE ||
             collision_point_p(x, y - 6, OBJ_oArrow, 0, NOONE) != NOONE) &&
            collision_point_p(x, y - 9, OBJ_oArrow, 0, NOONE) == NOONE &&
            collision_point_p(x, y + 9, OBJ_oArrow, 0, NOONE) == NOONE) {
            int obj = instance_nearest_p(x, y - 5, OBJ_oArrow);
            if (PX(obj).stuck) {
                PL.state = HANGING;
                p->yVel = 0;
                p->yAcc = 0;
                p->grav = 0;
            }
        }
    }
    if (PL.hangCount > 0) PL.hangCount -= 1;                                    /* :506 */

    if (PL.state == HANGING) {                                                 /* :508 */
        if (instance_exists_p(OBJ_oCape)) PUNTR(2007);
        PL.kJumped = 0;
        if (PL.kDown && PL.kJumpPressed) {
            p->grav = PL.gravNorm;
            PL.state = FALLING;
            p->yAcc -= p->grav;
            PL.hangCount = 5;
            if (PG.hasGloves) PL.hangCount = 10;
        } else if (PL.kJumpPressed) {
            p->grav = PL.gravNorm;
            if ((PL.facing == RIGHT && PL.kLeft) || (PL.facing == LEFT && PL.kRight)) {
                PL.state = FALLING;
                p->yAcc -= p->grav;
            } else {
                PL.state = JUMPING;
                p->yAcc += NMULI(PL.initialJumpAcc, 2);
                if (PL.facing == RIGHT) p->x -= PI(2);
                else p->x += PI(2);
            }
            PL.hangCount = PL.hangCountMax;
        }
        if ((PL.facing == LEFT && !isCollisionLeft(i, 2)) || (PL.facing == RIGHT && !isCollisionRight(i, 2))) {
            p->grav = PL.gravNorm;
            PL.state = FALLING;
            p->yAcc -= p->grav;
            PL.hangCount = 4;
        }
    } else
        p->grav = PL.gravNorm;

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
                    if (NABS(NP(p->x - (PX(ladder).x + PI(8)))) < N(4)) {
                        p->x = PX(ladder).x + PI(8);
                        p->xVel = p->yVel = p->xAcc = p->yAcc = 0;
                        PL.state = CLIMBING;
                    }
                } else {
                    p->y += PI(1);
                    PL.state = FALLING;
                    p->yAcc += p->grav;
                }
            } else
                PL.state = RUNNING;
        }
    }
    if (!PL.kDown && PL.state == DUCKING) {                                    /* :595 */
        PL.state = STANDING;
        p->xVel = 0;
        p->xAcc = 0;
    }
    if (NEQ(p->xVel, N(0)) && NEQ(p->xAcc, N(0)) && PL.state == RUNNING) PL.state = STANDING;
    if (NNE(p->xAcc, N(0)) && PL.state == STANDING) PL.state = RUNNING;
    if (NLT(p->yVel, N(0)) && platformCharacterIs(IN_AIR) && PL.state != HANGING) PL.state = JUMPING;
    if (NGT(p->yVel, N(0)) && platformCharacterIs(IN_AIR) && PL.state != HANGING) {   /* :613 */
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
         collision_point_p(x, y + 9, OBJ_oLadderTop, 0, NOONE) != NOONE && NEQ(p->xVel, N(0)))) {
        int ladder = instance_place_p(i, x, y - 8, OBJ_oLadder);
        PL.ladder = ladder;
        if (ladder != NOONE) {
            if (NABS(NP(p->x - (PX(ladder).x + PI(8)))) < N(4)) {
                p->x = PX(ladder).x + PI(8);
                if (collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oLadder, 0, NOONE) == NOONE &&
                    collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oLadderTop, 0, NOONE) == NOONE)
                    p->y = PX(ladder).y + PI(14 - (gsprcol[p->spr].yo - 1));      /* :637 sprite_yoffset */
                p->xVel = p->yVel = p->xAcc = p->yAcc = 0;
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
                p->xVel -= N(0.1);
                PL.xVelLimit = N(6);
                PL.xFric = PL.frictionRunningFastX;
            } else if (PL.kRight) {
                p->xVel += N(0.1);
                PL.xVelLimit = N(6);
                PL.xFric = PL.frictionRunningFastX;
            }
        } else if (PL.state == DUCKING) {
            if (NLT(p->xVel, N(2)) && NGT(p->xVel, N(-2))) {
                PL.xFric = N(0.2);
                PL.xVelLimit = N(3);
                p->ispd = (img_t)0.8;
            } else if ((PL.kLeft || PL.kRight) && PG.downToRun) {
                PUNTR(2008);
            } else {
                p->xVel = NMUL(p->xVel, N(0.8));
                if (NLT(p->xVel, N(0.5))) p->xVel = 0;
                PL.xFric = N(0.2);
                PL.xVelLimit = N(3);
                p->ispd = (img_t)0.8;
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
        } else if (collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oWater, 1, NOONE) != NOONE) {
            PUNTR(2009);
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
        if (PL.state == DUCKING && NLT(NABS(p->xVel), N(3)) && PL.facing == LEFT &&                   /* :818 */
            collision_point_p(x, y + 9, OBJ_oSolid, 0, NOONE) != NOONE &&
            collision_point_p(x - 1, y + 9, OBJ_oSolid, 0, NOONE) == NOONE && PL.kLeft) {
            PL.state = DUCKTOHANG;
            if (PL.holdItem != NOONE) {
                PX(PL.holdItem).held = 0;
                if (PX(PL.holdItem).type == T_GOLDIDOL) PX(PL.holdItem).y -= PI(8);
                scrDropItem(N(-1), N(-4));
            }
            if (instance_exists_p(OBJ_oMonkey)) PUNTR(2010);
        } else if (PL.state == DUCKING && NLT(NABS(p->xVel), N(3)) && PL.facing == RIGHT &&
                   collision_point_p(x, y + 9, OBJ_oSolid, 0, NOONE) != NOONE &&
                   collision_point_p(x + 1, y + 9, OBJ_oSolid, 0, NOONE) == NOONE && PL.kRight) {
            PL.state = DUCKTOHANG;
            if (PL.holdItem != NOONE) {
                if (PX(PL.holdItem).type == T_GOLDIDOL) PX(PL.holdItem).y -= PI(8);
                scrDropItem(N(1), N(-4));
            }
            if (instance_exists_p(OBJ_oMonkey)) PUNTR(2010);
        }
    }
    if (PL.state == DUCKTOHANG) {                                              /* :870 */
        p->x = PL.xPrev;
        p->y = PL.yPrev;
        p->xVel = p->yVel = p->xAcc = p->yAcc = 0;
        p->grav = 0;
    }
    if (instance_exists_p(OBJ_oParachute)) PL.yFric = N(0.5);                  /* :882 */
    if (instance_exists_p(OBJ_oCape)) PUNTR(2011);
    if (PL.pushTimer > 100) PL.pushTimer = 100;

    /* limits the acceleration :894 */
    if (NGT(p->xAcc, PL.xAccLimit)) p->xAcc = PL.xAccLimit;
    else if (NLT(p->xAcc, -PL.xAccLimit)) p->xAcc = -PL.xAccLimit;
    if (NGT(p->yAcc, PL.yAccLimit)) p->yAcc = PL.yAccLimit;
    else if (NLT(p->yAcc, -PL.yAccLimit)) p->yAcc = -PL.yAccLimit;
    p->xVel += p->xAcc;                                                        /* :900 */
    if (PL.dead || PL.stunned) p->yVel += N(0.6);
    else p->yVel += p->yAcc;
    p->xAcc = 0;
    p->yAcc = 0;
    p->xVel = NMUL(p->xVel, PL.xFric);                                         /* :909 */
    p->yVel = NMUL(p->yVel, PL.yFric);
    NOPS(10);
    if (instance_exists_p(OBJ_oBall)) PUNTR(2012);                             /* :913 */
    if (!PL.dead && !PL.stunned) {                                             /* :943 */
        if (NGT(p->xVel, PL.xVelLimit)) p->xVel = PL.xVelLimit;
        else if (NLT(p->xVel, -PL.xVelLimit)) p->xVel = -PL.xVelLimit;
    }
    if (NGT(p->yVel, PL.yVelLimit)) p->yVel = PL.yVelLimit;
    else if (NLT(p->yVel, -PL.yVelLimit)) p->yVel = -PL.yVelLimit;
    if (approximatelyZero(p->xVel)) p->xVel = 0;                               /* :952 */
    if (approximatelyZero(p->yVel)) p->yVel = 0;
    if (approximatelyZero(p->xAcc)) p->xAcc = 0;
    if (approximatelyZero(p->yAcc)) p->yAcc = 0;
    NOPS(12);

    /* slopes :960: colTop is the value computed above, so the loop stops at once or runs maxSlope + 1 times */
    {
        pos slopeYPrev = 0;
        int32_t slopeChangeInY;
        if (PL.maxSlope > 0 && platformCharacterIs(ON_GROUND) && NNE(p->xVel, N(0))) {
            slopeYPrev = p->y;
            for (; p->y >= slopeYPrev - PI(PL.maxSlope); p->y -= PI(1))
                if (PL.colTop)
                    break;
            slopeChangeInY = PFLOOR(slopeYPrev - p->y);
        } else
            slopeChangeInY = 0;
        if (NGT(NMULI(NABS(p->xVel), PL.maxSlope), N(0)) && platformCharacterIs(ON_GROUND)) {   /* :973 */
            pos xPrev2 = p->x, yPrev2 = slopeYPrev, yPrevHigh = p->y;
            double dist;
            PL.xPrev = xPrev2;
            PL.yPrev = yPrev2;
            moveTo(i, p->xVel, p->yVel + NI(slopeChangeInY), &xVelInteger, &yVelInteger);
            {
                double dx = PTOD(p->x) - PTOD(xPrev2), dy = PTOD(p->y) - PTOD(yPrev2), d2 = dx * dx + dy * dy;
                double s = d2, prev = 0;
                int it;
                for (it = 0; it < 64 && s != prev && d2 > 0; it++) { prev = s; s = 0.5 * (s + d2 / s); }
                dist = d2 > 0 ? s : 0;                                         /* point_distance */
            }
            if (DGT(dist, (xVelInteger < 0 ? -xVelInteger : xVelInteger))) {
                double ratio;
                int32_t axi = xVelInteger < 0 ? -xVelInteger : xVelInteger;
                p->x = xPrev2;
                p->y = yPrevHigh;
                ratio = axi / dist * 0.9;
                moveTo(i, NI(dround(xVelInteger * ratio)), NI(dround(yVelInteger * ratio + slopeChangeInY)),
                       &xVelInteger, &yVelInteger);
            }
        } else
            moveTo(i, p->xVel, p->yVel, &xVelInteger, &yVelInteger);           /* :1000 */
    }
    if (!PL.colBot && PL.maxDownSlope > 0 && xVelInteger != 0 && platformCharacterIs(ON_GROUND)) {   /* :1004 */
        pos upYPrev = p->y;
        for (; p->y <= upYPrev + PI(PL.maxDownSlope); p->y += PI(1))
            if (PL.colBot) {
                upYPrev = p->y;
                break;
            }
        p->y = upYPrev;
    }
    characterSprite(i);                                                        /* :1018 */
    PL.statePrevPrev = PL.statePrev;
    PL.statePrev = PL.state;
    if (PL.state == RUNNING || PL.state == DUCKING || PL.state == LOOKING_UP) {   /* :1025 */
        if (PL.state == RUNNING || PL.state == LOOKING_UP)
            p->ispd = (img_t)(NTOD(NABS(p->xVel)) * NTOD(PL.runAnimSpeed) + 0.1);
    }
    if (PL.state == CLIMBING) {                                                /* :1030 */
        double ax = NTOD(NABS(p->xVel)), ay = NTOD(NABS(p->yVel)), s2 = ax * ax + ay * ay, s = s2, prev = 0;
        int it;
        for (it = 0; it < 64 && s != prev && s2 > 0; it++) { prev = s; s = 0.5 * (s + s2 / s); }
        p->ispd = (img_t)((s2 > 0 ? s : 0) * NTOD(PL.climbAnimSpeed));
    }
    if (NGE(p->xVel, N(4)) || NLE(p->xVel, N(-4))) {
        p->ispd = 1;
        if (platformCharacterIs(ON_GROUND)) setCollisionBounds(i, -8, -8, 8, 8);
        else setCollisionBounds(i, -5, -8, 5, 8);
    } else
        setCollisionBounds(i, -5, -8, 5, 8);
    if (PL.whipping) p->ispd = 1;
    if (PL.state == DUCKTOHANG) {
        p->img = 0;
        p->ispd = (img_t)0.8;
    }
    if (DGT(p->ispd, 1)) p->ispd = 1;
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
                PX(obj).xVel = NI(a - b);
            }
            PX(obj).yVel = N(-2);
            pin_set_sprite(obj, GSPR_sBombArmed);
            PX(obj).alarm[1] = 40;
        } else {
            int reps = RAND(3, 4);
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
                    PX(obj).xVel = NI(a - b);
                }
                PX(obj).yVel = N(-2);
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
                    PX(obj).xVel = NI(a - b);
                }
                PX(obj).yVel = N(-2);
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
    PX(obj).cost = 0;
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
            PG.money += PX(h).value * (G.levelType + 1);
            if (PX(h).spr == GSPR_sCrystalSkull) PG.skulls += 1;
            else PG.idols += 1;
            pin_create(p->x, p->y - PI(8), OBJ_oBigCollect);
            pin_destroy(h);
            PL.holdItem = NOONE;
        } else if (PX(h).type == T_DAMSEL) {
            PUNTR(2015);
        } else if (PX(h).heavy) {
            PX(h).held = 0;
            PL.holdItem = NOONE;
            PL.pickupItemType = T_NONE;
        } else if (PX(h).type == T_BOMB) {
            if (PX(h).armed) PX(h).held = 0;
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
            PX(h).breakPieces = 0;
            pin_destroy(h);
        }
        PL.holdItem = NOONE;
        PL.pickupItemType = T_NONE;
    }
    door = instance_place_p(i, PTOD(p->x), PTOD(p->y), OBJ_oExit);              /* :859 */
    if (door != NOONE) {
        p->x = PX(door).x + PI(8);
        p->y = PX(door).y + PI(8);
    }
    PG.money += PG.collect;                                                    /* :867 */
    PG.xmoney += PG.collect;
    PG.collect = 0;
    pin_set_sprite(i, GSPR_sPExit);
    p->ispd = (img_t)0.5;
    PL.active = 0;
    PL.invincible = 999;
    p->depth = 999;
    if (G.thiefLevel > 0) G.thiefLevel -= 1;
    if (G.currLevel == 1) G.currLevel += PL.firstLevelSkip;
    else G.currLevel += PL.levelSkip;
    if (collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oXMarket, 0, NOONE) != NOONE) G.genBlackMarket = 1;
    if (collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oXGold, 0, NOONE) != NOONE) G.cityOfGold = 1;
    if (instance_exists_p(OBJ_oMonkey)) PUNTR(2016);
}

static void hurt_logic(int i)
{
    struct pin *p = &PX(i);
    double x = PTOD(p->x), y = PTOD(p->y);
    int obj;
    if (PG.plife < -10000) PG.plife = -10000;                                  /* :1452 */
    if (PG.plife < -99 && p->visible) {
        scrCreateBlood(i, p->x, p->y, 3);
        p->visible = 0;
    }
    if (!(PG.plife >= -99 && p->visible && !spr_is_exit(p->spr)))
        return;
    if (collision_point_p(x, y, OBJ_oSolid, 0, NOONE) != NOONE) {              /* :1463 crushed */
        PG.plife -= 99;
        PL.active = 0;
        p->yVel = N(-3);
        scrCreateBlood(i, p->x, p->y, 3);
        p->visible = 0;
    }
    if (collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oArrow, 0, NOONE) != NOONE) {   /* :1483 */
        obj = instance_nearest_p(x, y, OBJ_oArrow);
        if (obj != NOONE && NGT(NABS(PX(obj).xVel), N(3)) && !PX(obj).safe) {
            if (PG.plife > 0) PG.plife -= 2;
            p->xVel = PX(obj).xVel;
            p->yVel = N(-4);
            scrCreateBlood(i, p->x, p->y, 3);
            pin_destroy(obj);
            PL.stunned = 1;
            PL.stunTimer = 20;
        }
    }
    if (collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oRock, 0, NOONE) != NOONE) {    /* :1508 */
        obj = instance_nearest_p(x, y, OBJ_oRock);
        if (obj != NOONE && NGT(NABS(PX(obj).xVel), N(4)) && !PX(obj).safe && !PL.stunned && !PL.dead) {
            if (PG.hasMitt && PL.holdItem == NOONE) PUNTR(2017);
            else {
                if (PG.plife > 0) PG.plife -= 2;
                p->xVel = PX(obj).xVel;
                p->yVel = N(-4);
                scrCreateBlood(i, p->x, p->y, 3);
                PL.stunned = 1;
                PL.stunTimer = 20;
            }
        }
    }
    if (collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oLaser, 0, NOONE) != NOONE) PUNTR(2018);
    if (collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oPsychicWave, 0, NOONE) != NOONE && !PL.stunned && !PL.dead)
        PUNTR(2019);
    if (collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oExplosion, 0, NOONE) != NOONE) {   /* :1584 */
        int ex;
        PG.plife -= 10;
        ex = instance_nearest_p(x, y, OBJ_oExplosion);
        if (PX(ex).x < p->x) p->xVel = NI(RAND(4, 6));
        else p->xVel = NI(-RAND(4, 6));
        p->yVel = N(-6);
        PL.burning = 50;
        PL.stunned = 1;
        PL.stunTimer = 100;
        scrCreateBlood(i, p->x, p->y, 1);
    }
    x = PTOD(p->x);
    y = PTOD(p->y);
    obj = collision_rect_p(x - 6, y - 6, x + 6, y + 7, OBJ_oSpearsLeft, 0, NOONE);  /* :1599 */
    if (obj != NOONE) PUNTR(2020);
    if (collision_rect_p(x - 6, y - 6, x + 6, y + 7, OBJ_oSmashTrap, 0, NOONE) != NOONE) PUNTR(2021);
    obj = collision_rect_p(x - 2, y - 9, x + 2, y - 7, OBJ_oCeilingTrap, 0, NOONE);
    if (obj != NOONE && PX(obj).status > 0) PUNTR(2022);
    PL.colSpikes = 0;                                                          /* :1652 */
    if (collision_rect_p(x - 4, y - 4, x + 4, y + 8, OBJ_oSpikes, 0, NOONE) != NOONE) PL.colSpikes = 1;
    if (PL.colSpikes && PL.dead) {
        if (collision_point_p(x, y + 9, OBJ_oSolid, 0, NOONE) == NOONE) PADDN(p->y, N(0.05));
        else p->myGrav = N(0.6);
    } else
        p->myGrav = N(0.6);
    if (PL.colSpikes && NGT(p->yVel, N(0)) && (PL.fallTimer > 4 || PL.stunned)) {     /* :1663 */
        if (!PL.dead) {
            scrCreateBlood(i, p->x, p->y, 3);
            PG.plife -= 99;
            p->xVel = 0;
            p->yVel = 0;
            p->myGrav = 0;
        }
        obj = instance_place_p(i, PTOD(p->x), PTOD(p->y), OBJ_oSpikes);
        if (obj != NOONE) pin_set_sprite(obj, GSPR_sSpikesBlood);
    }
}

/* objects/oPlayer1/Step_0.gml */
void pl_step(int i)
{
    struct pin *p = &PX(i);
    if (PG.plife > 99) PG.plife = 99;                                          /* :8 */
    if (PG.bombs > 99) PG.bombs = 99;
    if (PG.rope > 99) PG.rope = 99;
    if (PG.hasCape) PUNTR(2030);
    if (PL.redColor > 0) {                                                     /* :20 kapala */
        if (PL.redToggle) PL.redColor -= 5;
        else if (PL.redColor < 20) PL.redColor += 5;
        else PL.redToggle = 1;
    } else
        PL.redColor = 0;
    if (PL.holdArrow == ARROW_BOMB) PUNTR(2031);                               /* :28 */
    if (PL.dead && !p->visible) {                                              /* :63 */
        p->xVel = 0;
        p->yVel = 0;
        p->grav = 0;
        p->myGrav = 0;
        PL.bounced = 1;
    }
    /* :74-147 distToNearestLightSource: read only by dark levels' drawing; no state */
    /* WHOA :150 */
    if (p->spr == GSPR_sWhoaLeft || p->spr == GSPR_sDamselWhoaL || p->spr == GSPR_sTunnelWhoaL) {
        if (PL.whoaTimer > 0) PL.whoaTimer -= 1;
        else if (PL.holdItem != NOONE) {
            int h = PL.holdItem;
            PX(h).held = 0;
            if (PL.facing == LEFT) PX(h).xVel = N(-2);
            else PX(h).xVel = N(2);
            if (PX(h).type == T_BOW && PL.bowArmed) scrFireBow();
            drop_or_switch();
        }
    } else
        PL.whoaTimer = PL.whoaTimerMax;
    if (PL.firing > 0) PL.firing -= 1;                                         /* :174 */
    if (collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oWaterSwim, 1, NOONE) != NOONE) PUNTR(2032);
    if (PL.burning > 0) {                                                      /* :188 */
        if (RAND(1, 5) == 1) {
            int yb = RAND(4, 12);                                              /* arguments: last first */
            int xb = RAND(4, 12);
            pin_create(p->x - PI(8) + PI(xb), p->y - PI(8) + PI(yb), OBJ_oBurn);
        }
        PL.burning -= 1;
    }
    if (collision_point_p(PTOD(p->x), PTOD(p->y) + 6, OBJ_oLava, 0, NOONE) != NOONE) PUNTR(2033);
    if (PG.hasJetpack && platformCharacterIs(ON_GROUND)) PL.jetpackFuel = 50;
    if (PTOD(p->y) > PW.room_h + 16 && !PL.dead) {                             /* :221 */
        PG.plife -= 99;
        p->xVel = 0;
        p->yVel = 0;
        p->grav = 0;
        p->myGrav = 0;
        PL.bounced = 1;
        if (PL.holdItem != NOONE) {
            PX(PL.holdItem).visible = 1;
            PX(PL.holdItem).held = 0;
            PL.holdItem = NOONE;
            PL.pickupItemType = T_NONE;
        }
    }
    if (PL.active) {                                                           /* :241 */
        if (PL.stunTimer > 0 && (p->spr == GSPR_sStunL || p->spr == GSPR_sDamselStunL || p->spr == GSPR_sTunnelStunL)) {
            p->ispd = (img_t)0.4;
            PL.stunTimer -= 1;
        }
        if (PL.stunTimer < 1 && (p->spr == GSPR_sStunL || p->spr == GSPR_sDamselStunL || p->spr == GSPR_sTunnelStunL))
            PL.stunned = 0;
        if (instance_exists_p(OBJ_oParachute)) PL.fallTimer = 0;
        if (NGT(p->yVel, N(0)) && PL.state != CLIMBING) {                             /* :254 */
            PL.fallTimer += 1;
            if (PL.fallTimer > 16) PL.wallHurt = 0;
            if (PG.hasParachute && !PL.stunned && PL.fallTimer > 14) PUNTR(2034);
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
            p->yVel = N(-3);
            PL.fallTimer = 0;
            obj = pin_create(p->x - PI(4), p->y + PI(6), OBJ_oPoof);
            PX(obj).xVel = N(-0.4);
            obj = pin_create(p->x + PI(4), p->y + PI(6), OBJ_oPoof);
            PX(obj).xVel = N(0.4);
        } else {
            PL.fallTimer = 0;
            if (instance_exists_p(OBJ_oParachute)) PUNTR(2035);
        }
        if (PL.swimming && collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oLava, 0, NOONE) == NOONE) PUNTR(2036);
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
            PX(h).visible = 1;
            PX(h).held = 0;
            drop_or_switch();
        }
        if (PL.bounced) p->yVel += N(1);
        else p->yVel += N(0.6);
        if (isCollisionTop(i, 1) && NLT(p->yVel, N(0))) p->yVel = NMUL(-p->yVel, N(0.8));
        if (isCollisionLeft(i, 1) || isCollisionRight(i, 1)) p->xVel = NMUL(-p->xVel, N(0.5));
        if (isCollisionBottom(i, 1) || isCollisionPlatformBottom(i, 1)) {
            if (NGT(p->yVel, N(1))) p->yVel = NMUL(-p->yVel, N(0.5));
            else p->yVel = 0;
            if (NLT(NABS(p->xVel), N(0.1))) p->xVel = 0;
            else if (NNE(NABS(p->xVel), N(0)) && collision_point_p(PTOD(p->x), PTOD(p->y) + 16, OBJ_oIce, 0, NOONE) != NOONE)
                p->xVel = NMUL(p->xVel, N(0.8));
            else if (NNE(NABS(p->xVel), N(0))) p->xVel = NMUL(p->xVel, N(0.3));
            PL.bounced = 1;
        }
        PL.xVelLimit = N(10);
        if (NGT(p->xVel, PL.xVelLimit)) p->xVel = PL.xVelLimit;
        else if (NLT(p->xVel, -PL.xVelLimit)) p->xVel = -PL.xVelLimit;
        if (NGT(p->yVel, PL.yVelLimit)) p->yVel = PL.yVelLimit;
        else if (NLT(p->yVel, -PL.yVelLimit)) p->yVel = -PL.yVelLimit;
        NOPS(10);
        moveTo(i, p->xVel, p->yVel, 0, 0);
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
            if (PX(PL.holdItem).type == T_MACHETE || PX(PL.holdItem).type == T_MATTOCK) PUNTR(2037);
        } else if (G.isTunnelMan) PUNTR(2038);
        else {
            int obj = pin_create(p->x - PI(16), p->y, OBJ_oWhip);
            pin_set_sprite(obj, GSPR_sWhipLeft);
        }
    } else if (spr_is_attack(p->spr) && PL.facing == RIGHT && DGT(p->img, 4) && instance_number_p(OBJ_oWhip) == 0) {
        if (PL.holdItem != NOONE) {
            if (PX(PL.holdItem).type == T_MACHETE || PX(PL.holdItem).type == T_MATTOCK) PUNTR(2037);
        } else if (G.isTunnelMan) PUNTR(2038);
        else {
            int obj = pin_create(p->x + PI(16), p->y, OBJ_oWhip);
            pin_set_sprite(obj, GSPR_sWhipRight);
        }
    }
    if (PL.holdItem != NOONE) {                                                /* :484 */
        if (PX(PL.holdItem).type == T_MACHETE || PX(PL.holdItem).type == T_MATTOCK) {
            if (spr_is_attack(p->spr) && DLT(p->img, 2)) PUNTR(2039);
        }
    } else if (p->spr == GSPR_sTunnelAttackL && DLT(p->img, 2) && instance_number_p(OBJ_oMattockPre) == 0) {
        PUNTR(2040);
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
        if (PX(PL.holdItem).cost > 0 && isLevel()) {
            int rp = G.roomPath[scrGetRoomX(PFLOOR(p->x))][scrGetRoomY(PFLOOR(p->y))];
            if (rp != 4 && rp != 5) PUNTR(2041);
        } else if (PX(PL.holdItem).cost > 0)
            PUNTR(2041);
    }
    {
        double x = PTOD(p->x), y = PTOD(p->y);
        if (PL.kUp && PL.kAttackPressed && collision_point_p(x, y, OBJ_oChest, 0, NOONE) != NOONE)   /* :563 */
            open_chest(i);
        if (PL.kUp && PL.kAttackPressed && collision_point_p(x, y, OBJ_oCrate, 0, NOONE) != NOONE)   /* :614 */
            open_crate(i);
        if (PL.kUp && PL.kAttackPressed && collision_point_p(x, y, OBJ_oFlareCrate, 0, NOONE) != NOONE)
            PUNTR(2042);
        if (!PL.dead && !PL.stunned && !PL.whipping && collision_point_p(x, y, OBJ_oXStart, 0, NOONE) != NOONE &&
            PL.kUp && platformCharacterIs(ON_GROUND) && !spr_is_exit(p->spr))
            PUNTR(2043);
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
                    PX(PL.holdItem).held = 1;
                    PG.rope -= 1;
                    PL.whoaTimer = PL.whoaTimerMax;
                } else
                    scrHoldItem(PL.pickupItemType);
            } else if (PX(h).spr == GSPR_sRopeEnd) {
                PG.rope += 1;
                pin_destroy(h);
                scrHoldItem(PL.pickupItemType);
            } else if (!PX(h).heavy && PX(h).cost == 0) {
                if (PG.bombs > 0 || PG.rope > 0) {
                    PL.pickupItemType = PX(h).type;
                    if (PX(h).type == T_BOW && PL.bowArmed) scrFireBow();
                    PX(h).breakPieces = 0;
                    pin_destroy(h);
                }
                if (PG.bombs > 0) {
                    PL.holdItem = pin_create(p->x, p->y, OBJ_oBomb);
                    if (PG.hasStickyBombs) PX(PL.holdItem).sticky = 1;
                    PX(PL.holdItem).held = 1;
                    PG.bombs -= 1;
                    PL.whoaTimer = PL.whoaTimerMax;
                } else if (PG.rope > 0) {
                    PL.holdItem = pin_create(p->x, p->y, OBJ_oRopeThrow);
                    PX(PL.holdItem).held = 1;
                    PG.rope -= 1;
                    PL.whoaTimer = PL.whoaTimerMax;
                }
            }
        } else {
            if (PG.bombs > 0) {
                PL.holdItem = pin_create(p->x, p->y, OBJ_oBomb);
                if (PG.hasStickyBombs) PX(PL.holdItem).sticky = 1;
                PX(PL.holdItem).held = 1;
                PG.bombs -= 1;
                PL.whoaTimer = PL.whoaTimerMax;
            } else if (PG.rope > 0) {
                PL.holdItem = pin_create(p->x, p->y, OBJ_oRopeThrow);
                PX(PL.holdItem).held = 1;
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
                    if (collision_point_p(px1 + 8, py1, OBJ_oSolid, 0, NOONE) == NOONE) {
                        double ox = PTOD(o->x), oy = PTOD(o->y);
                        if (collision_rect_p(ox - 8, oy, ox - 7, oy + 16, OBJ_oSolid, 0, NOONE) == NOONE) o->x -= PI(8);
                        else if (collision_rect_p(ox + 7, oy, ox + 8, oy + 16, OBJ_oSolid, 0, NOONE) == NOONE) o->x += PI(8);
                        else t = 0;
                    } else t = 0;
                } else if (collision_point_p(px1 - 8, py1, OBJ_oSolid, 0, NOONE) == NOONE) {
                    double ox = PTOD(o->x), oy = PTOD(o->y);
                    if (collision_rect_p(ox + 7, oy, ox + 8, oy + 16, OBJ_oSolid, 0, NOONE) == NOONE) o->x += PI(8);
                    else if (collision_rect_p(ox - 8, oy, ox - 7, oy + 16, OBJ_oSolid, 0, NOONE) == NOONE) o->x -= PI(8);
                    else t = 0;
                } else t = 0;
                if (!t)
                    pin_destroy(obj);
                else {
                    pin_create(o->x, o->y, OBJ_oRopeTop);
                    o = &PX(obj);
                    o->armed = 0;
                    o->falling = 1;
                    o->xVel = 0;
                    o->yVel = 0;
                    PG.rope -= 1;
                }
            }
        } else {
            int obj = pin_create(p->x, p->y, OBJ_oRopeThrow);
            PX(obj).armed = 1;
            PX(obj).px = NP(p->x);
            PX(obj).py = NP(p->y);
            PX(obj).xVel = 0;
            PX(obj).yVel = N(-12);
            PG.rope -= 1;
        }
    } else if (isLevel() && PL.kBombPressed && PG.bombs > 0 && !PL.whipping && PL.bowArmed) {
        PUNTR(2044);
    } else if (isLevel() && PL.kBombPressed && PG.bombs > 0 && !PL.whipping) {  /* :1141 */
        int obj = pin_create(p->x, p->y, OBJ_oBomb);
        struct pin *o = &PX(obj);
        if (PG.hasStickyBombs) o->sticky = 1;
        pin_set_sprite(obj, GSPR_sBombArmed);
        o->armed = 1;
        o->alarm[0] = 80;
        o->ispd = (img_t)0.2;
        o->safe = 1;
        o->alarm[2] = 10;
        if (PL.facing == LEFT) o->xVel = N(-8) + p->xVel;
        else if (PL.facing == RIGHT) o->xVel = N(8) + p->xVel;
        o->yVel = N(-3);
        if (PL.kUp) o->yVel = N(-9);
        if (PL.kDown) {
            if (platformCharacterIs(ON_GROUND)) o->xVel = NMUL(o->xVel, N(0.1));
            o->yVel = N(3);
        }
        PG.bombs -= 1;
    } else if (PL.holdItem == NOONE) {                                         /* :1180 */
        if (PL.kAttackPressed && PL.state != DUCKING && PL.state != DUCKTOHANG && !PL.whipping &&
            p->spr != GSPR_sPExit && p->spr != GSPR_sDamselExit) {
            p->ispd = (img_t)0.6;
            if (G.isTunnelMan || G.isDamsel) PUNTR(2045);
            else {
                pin_set_sprite(i, GSPR_sAttackLeft);
                p->img = 0;
                PL.whipping = 1;
            }
        } else if (PL.kAttackPressed && PL.kDown) {                            /* :1209 pick up */
            double x = PTOD(p->x), y = PTOD(p->y);
            if (collision_rect_p(x - 8, y, x + 8, y + 8, OBJ_oItem, 0, NOONE) != NOONE) {
                int obj = instance_nearest_p(x, y, OBJ_oItem);
                if (PX(obj).canPickUp && collision_point_p(PTOD(PX(obj).x), PTOD(PX(obj).y), OBJ_oSolid, 0, NOONE) == NOONE) {
                    int h = obj;
                    PL.holdItem = h;
                    PX(h).held = 1;
                    PL.whoaTimer = PL.whoaTimerMax;
                    PL.pickupItemType = PX(h).type;
                    if (PX(h).type == T_BOW && PX(h).New) {
                        PX(h).New = 0;
                        PG.arrows += 6;
                    }
                    if (PX(h).type == T_GOLDIDOL && PX(h).trigger && !isRoomIs(R_rLoadLevel)) {
                        if (G.levelType == 0) {
                            int trap = instance_nearest_p(x, y - 64, OBJ_oGiantTikiHead);
                            if (trap != NOONE) PX(trap).alarm[0] = 100;
                            scrShake(100);
                            PX(h).trigger = 0;
                        } else
                            PUNTR(2046);
                    } else if (PX(h).type == T_DAMSEL) {
                        PUNTR(2047);
                    } else if (PX(h).cost == 0)
                        scrStealItem();
                }
            } else if (collision_rect_p(x - 8, y, x + 8, y + 8, OBJ_oEnemy, 0, NOONE) != NOONE)
                PUNTR(2048);
        }
    } else if (PL.kAttackPressed) {                                            /* :1319 */
        if (PL.holdItem != NOONE) {
            extern void scrUseItem(void);
            scrUseItem();
        }
    }
    if (isLevel() && PL.active && PL.kPayPressed && !PL.dead && !PL.stunned) { /* :1327 */
        if (isInShop(PFLOOR(p->x), PFLOOR(p->y)) && instance_exists_p(OBJ_oShopkeeper)) PUNTR(2049);
    }
    if (PL.kAttack && PL.bowArmed && NLT(PL.bowStrength, N(12))) PUNTR(2050);      /* :1431 */
    if (PL.kAttackReleased && PL.bowArmed) scrFireBow();
    hurt_logic(i);                                                             /* :1450 */
    if ((PL.dead || PL.stunned) && PL.holdItem != NOONE) {                     /* :1684 */
        int h = PL.holdItem;
        PX(h).held = 0;
        PX(h).xVel = p->xVel;
        PX(h).yVel = N(-6);
        PX(h).armed = 1;
        if (PX(h).type == T_DAMSEL) PUNTR(2051);
        else if (PX(h).type == T_BOW) scrFireBow();
        drop_or_switch();
    }
    if (PL.dead || PL.stunned) {                                               /* :1708 */
        if (instance_exists_p(OBJ_oParachute)) PUNTR(2052);
        if (PL.whipping) {
            int16_t w[64];
            int n, k;
            PL.whipping = 0;
            n = pw_with(OBJ_oWhip, w, 64);
            for (k = 0; k < n; k++) if (PX(w[k]).alive) pin_destroy(w[k]);
        }
        if (G.isDamsel || G.isTunnelMan) PUNTR(2053);
        else {
            if (NEQ(p->xVel, N(0))) {
                if (PL.dead) pin_set_sprite(i, GSPR_sDieL);
                else if (PL.stunned) pin_set_sprite(i, GSPR_sStunL);
            } else if (PL.bounced) {
                if (NLT(p->yVel, N(0))) pin_set_sprite(i, GSPR_sDieLBounce);
                else pin_set_sprite(i, GSPR_sDieLFall);
            } else {
                if (NLT(p->xVel, N(0))) pin_set_sprite(i, GSPR_sDieLL);
                else pin_set_sprite(i, GSPR_sDieLR);
            }
        }
        if (collision_point_p(PTOD(p->x), PTOD(p->y), OBJ_oSpikes, 0, NOONE) != NOONE && PL.dead && NNE(p->yVel, N(0))) {
            if (RAND(1, 8) == 1) scrCreateBlood(i, p->x, p->y, 1);
        }
        if (isCollisionRight(i, 1) || isCollisionLeft(i, 1) || isCollisionBottom(i, 1)) {   /* :1785 */
            if (PL.wallHurt > 0) {
                int k;
                for (k = 0; k < 3; k++) pin_create(p->x, p->y, OBJ_oBlood);
                PG.plife -= 1;
                PL.wallHurt -= 1;
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
        if (!PL.dead && PG.plife < 1) {
            if (PG.hasAnkh) PUNTR(2054);
            else {
                PG.plife = 0;
                PG.drawHUD = 0;
                PL.dead = 1;
            }
        }
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
        if (PL.holdItem != NOONE && PX(PL.holdItem).type == T_BOW) PUNTR(2055);
        if (collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oTreasure, 0, NOONE) != NOONE && !PL.dead && !PL.stunned) {   /* :1964 */
            int gem = instance_nearest_p(x, y, OBJ_oTreasure);
            if (PX(gem).canCollect) {
                int v = PX(gem).value;
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
                pin_destroy(gem);
            }
        }
        if (collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oBombBag, 0, NOONE) != NOONE && !PL.dead && !PL.stunned) {   /* :1994 */
            int obj = collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oBombBag, 0, NOONE);
            if (!PX(obj).held && PX(obj).cost == 0 && collision_point_p(PTOD(PX(obj).x), PTOD(PX(obj).y), OBJ_oSolid, 0, NOONE) == NOONE) {
                int d;
                PG.bombs += 3;
                d = pin_create(PX(obj).x, PX(obj).y - PI(14), OBJ_oItemsGet);
                pin_set_sprite(d, GSPR_sBombsGet);
                pin_destroy(obj);
            }
        }
        if (collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oBombBox, 0, NOONE) != NOONE && !PL.dead && !PL.stunned) {
            int obj = collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oBombBox, 0, NOONE);
            if (!PX(obj).held && PX(obj).cost == 0 && collision_point_p(PTOD(PX(obj).x), PTOD(PX(obj).y), OBJ_oSolid, 0, NOONE) == NOONE) {
                int d;
                PG.bombs += 12;
                d = pin_create(PX(obj).x, PX(obj).y - PI(14), OBJ_oItemsGet);
                pin_set_sprite(d, GSPR_sBombsGet);
                pin_destroy(obj);
            }
        }
        if (collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oRopePile, 0, NOONE) != NOONE && !PL.dead && !PL.stunned) {
            int obj = collision_rect_p(x - 8, y - 8, x + 8, y + 8, OBJ_oRopePile, 0, NOONE);
            if (!PX(obj).held && PX(obj).cost == 0 && collision_point_p(PTOD(PX(obj).x), PTOD(PX(obj).y), OBJ_oSolid, 0, NOONE) == NOONE) {
                int d;
                PG.rope += 3;
                d = pin_create(PX(obj).x, PX(obj).y - PI(15), OBJ_oItemsGet);
                pin_set_sprite(d, GSPR_sRopeGet);
                pin_destroy(obj);
            }
        }
        if (collision_point_p(x, y, OBJ_oExit, 0, NOONE) != NOONE) {           /* :2042 */
            if (PL.holdItem != NOONE) {
                int h = PL.holdItem;
                if (PX(h).type == T_GOLDIDOL) {
                    PG.collect += PX(h).value * (G.levelType + 1);
                    PG.collectCounter += 20;
                    if (PG.collectCounter > 100) PG.collectCounter = 100;
                    if (PX(h).spr == GSPR_sCrystalSkull) PG.skulls += 1;
                    else PG.idols += 1;
                    pin_create(p->x, p->y - PI(8), OBJ_oBigCollect);
                    pin_destroy(h);
                    PL.holdItem = NOONE;
                } else if (PX(h).type == T_DAMSEL)
                    PUNTR(2056);
            }
        }
    }
    PG.xmoney += PG.money - PL.money;
}

/* objects/oPlayer1/Step_2.gml */
void pl_end_step(int i)
{
    struct pin *p = &PX(i);
    if (PL.holdItem != NOONE) {
        if (PL.state == CLIMBING && (PG.hasJetpack || PG.hasCape)) PX(PL.holdItem).depth = 51;
        else PX(PL.holdItem).depth = 0;
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
            if (G.darkLevel || G.blackMarket) p->alarm[1] = 210;
            else if (G.snakePit) {
            } else if (G.cemetary) { if (G.lake) p->alarm[1] = 210; }
            else if (G.lake || G.yetiLair || G.alienCraft) {
            } else if (G.cityOfGold) { if (G.sacrificePit) p->alarm[1] = 210; }
        }
        break;
    case 1:
        if (!isRoomIs(R_rTutorial)) {
            if (G.snakePit) {
            } else if (G.cemetary && G.darkLevel) { if (G.lake) p->alarm[4] = 210; }
            else if (G.lake || G.yetiLair || G.alienCraft) {
            } else if (G.cityOfGold) { if (G.sacrificePit) p->alarm[4] = 210; }
        }
        break;
    case 2: PL.climbSndToggle = !PL.climbSndToggle; break;
    case 3: PL.walkSndToggle = !PL.walkSndToggle; break;
    case 4: break;
    case 10: PUNTR(2060); break;
    case 11:
        if (PL.holdArrow > 0) {
            PL.holdArrowToggle = !PL.holdArrowToggle;
            p->alarm[11] = 1;
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
        if (PL.holdItem != NOONE) PX(PL.holdItem).visible = 1;
    } else if (p->spr == GSPR_sDuckToHangL || p->spr == GSPR_sDamselDtHL || p->spr == GSPR_sTunnelDtHL) {
        int obj;
        p->y = p->y + PI(16);
        move_snap(i, 1, 8);
        p->xVel = p->yVel = p->xAcc = p->yAcc = 0;
        p->grav = 0;
        if (PL.facing == LEFT) {
            obj = collision_point_p(PTOD(p->x) - 8, PTOD(p->y), OBJ_oLadder, 0, NOONE);
            if (obj == NOONE) obj = collision_point_p(PTOD(p->x) - 8, PTOD(p->y), OBJ_oLadderTop, 0, NOONE);
        } else {
            obj = collision_point_p(PTOD(p->x) + 8, PTOD(p->y), OBJ_oLadder, 0, NOONE);
            if (obj == NOONE) obj = collision_point_p(PTOD(p->x) + 8, PTOD(p->y), OBJ_oLadderTop, 0, NOONE);
        }
        if (obj != NOONE) {
            PL.state = CLIMBING;
            p->x = PX(obj).x + PI(8);
        } else if (PL.facing == LEFT) {
            PL.state = HANGING;
            PL.facing = RIGHT;
            p->x = p->x - PI(6);
            p->x += PI(1);
        } else {
            PL.state = HANGING;
            PL.facing = LEFT;
            p->x = p->x + PI(6);
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
            PUNTR(2061);
        G.cleanSolids = 1;
    }
}

/* objects/oPlayer1/Collision_oBlood.gml, Collision_oPushBlock.gml */
void pl_collision(int i, int other)
{
    struct pin *p = &PX(i);
    if (obj_is(PX(other).obj, OBJ_oBlood)) {
        if (PG.hasKapala && PX(other).collectible) PUNTR(2062);
    } else if (obj_is(PX(other).obj, OBJ_oPushBlock)) {
        double dx = PTOD(p->x) - (PTOD(PX(other).x) + 8), dy = PTOD(p->y) - (PTOD(PX(other).y) + 8);
        if (dx * dx + dy * dy < 121 && p->y >= PX(other).y)
            p->x = p->xprev;
    }
}

/* scripts/characterDrawEvent: the drawing sets image_xscale from facing */
void pl_draw(int i)
{
    PX(i).xscale = PL.facing == RIGHT ? -1 : 1;
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
        o->armed = 1;
        o->alarm[0] = 80;
        o->ispd = (img_t)0.2;
    } else if (o->spr == GSPR_sRopeEnd) {                                      /* :31 */
        if (!PL.kDown && PL.colTop) {
        } else {
            o->held = 0;
            o->armed = 1;
            o->px = NP(p->x);
            o->py = NP(p->y);
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
                    if (p->x < r->x && collision_point_p(PTOD(p->x) + 2, PTOD(p->y), OBJ_oSolid, 0, NOONE) == NOONE) {
                        if (collision_rect_p(ox - 8, oy, ox - 7, oy + 16, OBJ_oSolid, 0, NOONE) == NOONE) r->x -= PI(8);
                        else if (collision_rect_p(ox + 7, oy, ox + 8, oy + 16, OBJ_oSolid, 0, NOONE) == NOONE) r->x += PI(8);
                        else t = 0;
                    } else if (collision_point_p(PTOD(p->x) - 2, PTOD(p->y), OBJ_oSolid, 0, NOONE) == NOONE) {
                        if (collision_rect_p(ox + 7, oy, ox + 8, oy + 16, OBJ_oSolid, 0, NOONE) == NOONE) r->x += PI(8);
                        else if (collision_rect_p(ox - 8, oy, ox - 7, oy + 16, OBJ_oSolid, 0, NOONE) == NOONE) r->x -= PI(8);
                        else t = 0;
                    }
                    if (!t) {
                        int o2 = pin_create(PX(PL.holdItem).x, PX(PL.holdItem).y, OBJ_oRopeThrow);
                        if (PL.facing == 18) PX(o2).xVel = N(-3.2);
                        else PX(o2).xVel = N(3.2);
                        PX(o2).yVel = N(0.5);
                        pin_destroy(obj);
                    } else {
                        pin_create(PX(obj).x, PX(obj).y, OBJ_oRopeTop);
                        r = &PX(obj);
                        r->armed = 0;
                        r->falling = 1;
                        r->xVel = 0;
                        r->yVel = 0;
                    }
                }
                pin_destroy(PL.holdItem);
                PL.holdItem = NOONE;
            } else {
                o->x = p->x;
                o->xVel = 0;
                o->yVel = N(-12);
            }
            scrHoldItem(PL.pickupItemType);
        }
    } else if (o->type == T_MACHETE || o->type == T_MATTOCK || o->type == T_PISTOL || o->type == T_SCEPTRE ||
               o->type == T_WEBCANNON || o->type == T_TELEPORTER || o->type == T_BOW || o->type == T_SHOTGUN) {
        PUNTR(2070);
        return;
    } else {                                                                   /* :594 throw */
        if (o->type == T_DAMSEL) PUNTR(2071);
        o->held = 0;
        o->safe = 1;
        o->alarm[2] = 10;
        if (PL.facing == LEFT) {
            if (o->heavy) o->xVel = N(-4) + p->xVel;
            else o->xVel = N(-8) + p->xVel;
            if (collision_point_p(PTOD(p->x) - 8, PTOD(p->y), OBJ_oSolid, 0, NOONE) != NOONE) o->x += PI(8);
        } else if (PL.facing == RIGHT) {
            if (o->heavy) o->xVel = N(4) + p->xVel;
            else o->xVel = N(8) + p->xVel;
            if (collision_point_p(PTOD(p->x) + 8, PTOD(p->y), OBJ_oSolid, 0, NOONE) != NOONE) o->x -= PI(8);
        }
        if (o->heavy) o->yVel = N(-2);
        else o->yVel = N(-3);
        if (PL.kUp) {
            if (o->heavy) o->yVel = N(-4);
            else o->yVel = N(-9);
        }
        if (PL.kDown) {
            if (platformCharacterIs(ON_GROUND)) {
                o->y -= PI(2);
                o->xVel = NMUL(o->xVel, N(0.6));
                o->yVel = N(0.5);
            } else
                o->yVel = N(3);
        } else if (!PG.hasMitt) {
            if (PL.facing == LEFT) {
                if (collision_point_p(PTOD(p->x) - 8, PTOD(p->y) - 10, OBJ_oSolid, 0, NOONE) != NOONE) {
                    o->yVel = 0;
                    o->xVel -= N(1);
                }
            } else if (PL.facing == RIGHT) {
                if (collision_point_p(PTOD(p->x) + 8, PTOD(p->y) - 10, OBJ_oSolid, 0, NOONE) != NOONE) {
                    o->yVel = 0;
                    o->xVel += N(1);
                }
            }
        }
        if (PG.hasMitt && !scrPlayerIsDucking(i)) PUNTR(2072);
        if (o->spr == GSPR_sBombArmed) scrHoldItem(PL.pickupItemType);
        else PL.holdItem = NOONE;
    }
    if (PL.kDown && PL.holdItem != NOONE) {                                    /* :679 */
        PX(PL.holdItem).x = p->x;
        PX(PL.holdItem).y = p->y;
    }
    if (PL.holdItem == NOONE) PL.pickupItemType = T_NONE;
}
