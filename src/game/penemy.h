/* P5 mines content: the enemies (penemy.c), the damsel (pdamsel.c), the shopkeeper and shops (pshop.c), the boulder
 * trap (penemy.c). Each entry returns 1 when the instance's object is one of theirs (the event was handled). */
#ifndef PENEMY_H
#define PENEMY_H

int pen_create(int i, int fromgen);           /* Create (fromgen: the generator ran it; only its variables) */
int pen_step(int i);
int pen_alarm(int i, int a);
int pen_animend(int i);
int pen_collision(int self, int other);
int pen_draw(int i);
int pen_outside(int i);
int pen_destroy(int i);
void pen_motion(void);                        /* built-in speed / direction motion, after the Step events */
void pen_moving_solids(void);                 /* gameStepEvent's oMovingSolid part */

/* a thrown item hitting an enemy / the damsel (oItem Step :233-370, oJar / oSkull Step :104-170) */
void pen_item_hit_enemy(int item);
void pen_item_hit_damsel(int item);
int pen_jar_hit(int jar, int skull);          /* 1: destroy the jar */
void pen_sight_speed(double dir, double *h, double *v);   /* oEnemySight's speed 10 at dir 0 / 180 (kept) */
void pen_player_pickup_enemy(int pl);         /* oPlayer1 Step :1306 */

int pdam_create(int i, int fromgen);
int pdam_step(int i);
int pdam_animend(int i);
int pdam_collision(int self, int other);
int pdam_draw(int i);
int pdam_destroy(int i);
int pdam_alarm(int i, int a);
void pdam_room_end(void);                     /* oDamselKiss's Room End */
int pdam_jar_hit(int jar);                    /* oJar Step :148 (1: the jar breaks) */

int pshop_create(int i, int fromgen);
int pshop_step(int i);
int pshop_alarm(int i, int a);
int pshop_animend(int i);
int pshop_collision(int self, int other);
int pshop_draw(int i);
void scrShopkeeperAnger(int self, int k);    /* self: the caller (instance_nearest(x, y, ...)) */
void pshop_pay(int pl);                       /* oPlayer1 Step :1327 (kPayPressed in a shop) */
void pshop_item_left_shop(int item);          /* oItem Step :18-35 (a for-sale item outside the shop) */

int pitem_create(int i, int fromgen);
int pitem_step(int i);
int pitem_collision(int self, int other);

/* the enemy's shared state from oEnemy Create */
void pen_enemy_create(int i);
void pen_parent_step(int i);                  /* objects/oEnemy/Step_0.gml */
void scrCheckCollisions(int i);
int pen_hit_common(int e, int kind);
/* oWhip / oWhipPre and their children: other.damage (oSlash, oMachetePre, oMattockHit, oMattockPre: 2) and
   other.type == "Machete" (oSlash, oMachetePre) */
static inline int whip_damage(int w) { return PX(w).type == T_MACHETE || PX(w).type == T_MATTOCK ? 2 : 1; }
static inline int whip_machete(int w) { return PX(w).type == T_MACHETE; }          /* oItem / oEnemy projectile hits on enemy e (see penemy.c) */

#endif
