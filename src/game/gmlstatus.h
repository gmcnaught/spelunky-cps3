/* Status and facing constants of the GML objects, as their Create events define them, shared by the generator
 * (gen*.c sets a generated instance's status) and the play loop (its events read it). */
#ifndef GMLSTATUS_H
#define GMLSTATUS_H

/* objects/oEnemy/Create_0.gml (and the enemies that inherit it): facing LEFT / RIGHT, status STUNNED / DEAD */
enum { E_LEFT = 0, E_RIGHT = 1, E_STUNNED = 98, E_DEAD = 99 };
/* objects/oDamsel/Create_0.gml */
enum { D_IDLE = 0, D_RUN = 1, D_THROWN = 2, D_YELL = 3, D_EXIT = 4, D_SLAVE = 5, D_KISS = 6, D_DEAD = 99 };
/* objects/oShopkeeper/Create_0.gml (its LEFT / RIGHT are oEnemy's) */
enum { S_IDLE = 0, S_WALK = 1, S_ATTACK = 2, S_THROW = 3, S_PATROL = 4, S_FOLLOW = 5, S_STUNNED = 98, S_DEAD = 99 };

#endif
