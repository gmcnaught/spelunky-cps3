/* P5: items the P4 play loop left out that the Mines reach (refs/hd/src/objects/<obj>/<event>.gml): the locked
 * chest and its key, the Udjat eye it holds, the weapons held (sprite by facing), the
 * flare crate lying in a level or a shop (not their use), Kali's altar standing. */
#include "pint.h"
#include "penemy.h"
#include "pcontent.h"                            /* P7 content packages (docs/CONTENT.md) */

int pitem_create(int i, int fromgen)
{
    struct pin *p = &PX(i);
    (void)fromgen;
    if (p->obj == OBJ_oUdjatEye) {                                     /* objects/oUdjatEye/Create_0.gml */
        create_item(p);
        p->type = T_UDJATEYE;
        PE(p)->xVel = PE(p)->yVel = PE(p)->xAcc = PE(p)->yAcc = 0;
        setCollisionBounds(i, -6, -6, 6, 6);
        PE(p)->cost = 0;
        return 1;
    }
    return 0;
}

int pitem_step(int i)
{
    switch (PX(i).obj) {
    case OBJ_oLockedChest: case OBJ_oUdjatEye:                         /* Step: oItem's only */
        item_step(i);
        return 1;
    case OBJ_oWebCannon:                                               /* objects/oWebCannon/Step_0.gml */
        item_step(i);
        if (PE(&PX(i))->held) pin_set_sprite(i, PL.facing == 18 ? GSPR_sWebCannonL : GSPR_sWebCannonR);
        return 1;
    case OBJ_oMattock:                                                 /* objects/oMattock/Step_0.gml */
        item_step(i);
        if (PE(&PX(i))->held) pin_set_sprite(i, PL.facing == 18 ? GSPR_sMattockLeft : GSPR_sMattockRight);
        return 1;
    case OBJ_oMachete: case OBJ_oPistol: case OBJ_oShotgun: case OBJ_oSceptre:   /* <obj>/Step_0.gml */
        item_step(i);
        if (PX(i).alive && PE(&PX(i))->held) {
            int l = PX(i).obj == OBJ_oMachete ? GSPR_sMacheteLeft : PX(i).obj == OBJ_oPistol ? GSPR_sPistolLeft :
                    PX(i).obj == OBJ_oShotgun ? GSPR_sShotgunLeft : GSPR_sSceptreLeft;
            int r = PX(i).obj == OBJ_oMachete ? GSPR_sMacheteRight : PX(i).obj == OBJ_oPistol ? GSPR_sPistolRight :
                    PX(i).obj == OBJ_oShotgun ? GSPR_sShotgunRight : GSPR_sSceptreRight;
            pin_set_sprite(i, PL.facing == 18 ? l : r);
        }
        return 1;
    case OBJ_oBow:                                                     /* objects/oBow/Step_0.gml */
        item_step(i);
        if (PX(i).alive && PE(&PX(i))->held) {
            pin_set_sprite(i, PL.facing == 18 ? GSPR_sBowLeft : GSPR_sBowRight);
            if (NGE(PL.bowStrength, N(10))) pin_setimg(&PX(i), 3);
            else if (NGT(PL.bowStrength, N(6))) pin_setimg(&PX(i), 2);
            else if (NGT(PL.bowStrength, N(2))) pin_setimg(&PX(i), 1);
            else pin_setimg(&PX(i), 0);
        } else if (PX(i).alive)
            pin_setimg(&PX(i), 0);
        return 1;
    case OBJ_oSacAltarLeft: case OBJ_oSacAltarRight: {                 /* objects/oSacAltarLeft/Step_0.gml */
        double x = PTOD(PX(i).x), y = PTOD(PX(i).y);
        view_read();
        if (DGT(x, PW.xview - 20) && DLT(x, PW.xview + 320 + 4) && DGT(y, PW.yview - 20) && DLT(y, PW.yview + 240 + 4) &&
            collision_point_p(x, y + 16, OBJ_oSolid, 0, NOONE) == NOONE)
            pitems_world(8002, i, 0);                                               /* its Destroy (Kali's punishment): P7 */
        return 1;
    }
    case OBJ_oFlareCrate:                                              /* objects/oFlareCrate/Step_0.gml */
        item_step(i);
        if (collision_point_p(PTOD(PX(i).x), PTOD(PX(i).y), OBJ_oWater, 1, i) != NOONE) pswamp_world(8001, i, 0);
        return 1;
    }
    return 0;
}

/* objects/oLockedChest/Collision_oKey.gml */
int pitem_collision(int self, int other)
{
    struct pin *p = &PX(self);
    if (p->obj != OBJ_oLockedChest) return 0;
    if (PE(&PX(other))->held && p->spr == GSPR_sLockedChest) {
        int obj;
        PE(&PX(other))->held = 0;
        PL.holdItem = NOONE;
        PL.pickupItemType = T_NONE;
        pin_destroy(other);
        pin_set_sprite(self, GSPR_sLockedChestOpen);
        obj = pin_create(PX(self).x, PX(self).y, OBJ_oUdjatEye);
        {
            int a = RAND(0, 3), b = RAND(0, 3);
            PE(&PX(obj))->xVel = NI(a - b);
        }
        PE(&PX(obj))->yVel = N(-2);
        obj = pin_create(PX(self).x, PX(self).y, OBJ_oPoof);
        PE(&PX(obj))->xVel = N(-0.4);
        obj = pin_create(PX(self).x, PX(self).y, OBJ_oPoof);
        PE(&PX(obj))->xVel = N(0.4);
        pin_destroy(self);
    }
    return 1;
}
