/*
 * The controls of a vehicle (at 0x414, "Ctrl" variables; Entities/Vehicle, src/game/Entities/Vehicle/Controls.c): the
 * selected weapon, the axes, the fire buttons of this and the last frame and the two force feedback motors. The
 * functions take the controls as bytes like the field macros of include/scrap/weapons.h.
 */
#ifndef SCRAP_CONTROLS_H
#define SCRAP_CONTROLS_H

#include "scrap/entity.h"

typedef struct cVehicleControls {
	int weapon;			/* 0x00 */
	float hRot;			/* 0x04 "HRot" */
	float vRot;			/* 0x08 "VRot" */
	float acc;			/* 0x0c "Acc" */
	float str;			/* 0x10 "Str" */
	float vStr;			/* 0x14 "VStr" */
	float fireWait;			/* 0x18 the fire is held 0.2 s after the button, cleared with a new weapon */
	float padBounce;		/* 0x1c "VehicleCenterPadBounce" grows back to the pitch of the stick */
	byte firePrev;			/* 0x20 */
	byte cmPrev;			/* 0x21 */
	byte fire;			/* 0x22 */
	byte cm;			/* 0x23 the ammo button (countermeasures) */
	byte boostKey;			/* 0x24 the boost button */
	byte fullThrust;		/* 0x25 1 after Reset: the second accelerate action counts, the thrust follows it at once */
	byte viewTarget;		/* 0x26 the view-target or next-target button */
	byte netFire;			/* 0x27 the toggle bit of the fire of the network state */
	byte follow;			/* 0x28 the follow button: the aim follows the target */
	byte pad_29[3];
	float attackTime;		/* 0x2c the target is followed */
	float emptyTime;		/* 0x30 without ammo, the next weapon after TimeWSwitchNo */
	float rumbleA;			/* 0x34 */
	float rumbleB;			/* 0x38 */
	float rumbleARate;		/* 0x3c per second */
	float rumbleBRate;		/* 0x40 */
} cVehicleControls;
SD_OFFSET(cVehicleControls, fire, 0x22);
SD_OFFSET(cVehicleControls, netFire, 0x27);
SD_OFFSET(cVehicleControls, rumbleA, 0x34);
SD_SIZE(cVehicleControls, 0x44);

cVehicleControls *__fastcall Controls_ctor(cVehicleControls *self);
/* the first weapon of the world, everything else 0 */
void __fastcall Controls_Reset(cVehicleControls *self);
byte __thiscall Controls_VarF(cVehicleControls *self, const char *name, double *d, int set);
int __thiscall Controls_GetWeapon(cVehicleControls *self);
/* the buttons and axes from the network state of the vehicle v (its +0xf4, bits at +0x74) */
void __thiscall Controls_FromNet(cVehicleControls *self, cEntity *v, float dt);
/* clears both fire buttons and +0x18 when the weapon changes */
void __thiscall Controls_SetWeapon(cVehicleControls *self, int w);
/* the two force feedback motors: strength and fade time */
void __thiscall Controls_RumbleA(cVehicleControls *self, float strength, float time);
void __thiscall Controls_RumbleB(cVehicleControls *self, float strength, float time);

/* the native functions of the items a ship takes (item name, ship name): the ammo or life of the Item...Adds
 * engine variables, the item hidden for its Item...RespawnTime */
struct cAbstractFile;
void Controls_TakeEnerNative(struct cAbstractFile *f);	/* "TakeEner" */
void Controls_TakeLifeNative(struct cAbstractFile *f);	/* "TakeLife", OnLifeItemTaken (ship) */
void Controls_TakeMissNative(struct cAbstractFile *f);	/* "TakeMiss" */
void Controls_TakePartNative(struct cAbstractFile *f);	/* "TakePart" */

#endif
