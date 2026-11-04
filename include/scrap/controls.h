/*
 * The controls of a vehicle (at 0x414, "Ctrl" variables; Entities/Vehicle, src/game/Entities/Vehicle/Controls.c): the
 * selected weapon, the axes, the fire buttons of this and the last frame and the two force feedback motors. The
 * functions take the controls as bytes like the field macros of include/scrap/weapons.h.
 */
#ifndef SCRAP_CONTROLS_H
#define SCRAP_CONTROLS_H

#include "scrap/entity.h"

typedef struct cVehicleControls {
	int weapon;
	float hRot;			/* "HRot" */
	float vRot;			/* "VRot" */
	float acc;			/* "Acc" */
	float str;			/* "Str" */
	float vStr;			/* "VStr" */
	float fireWait;			/* the fire is held 0.2 s after the button, cleared with a new weapon */
	float padBounce;		/* "VehicleCenterPadBounce" grows back to the pitch of the stick */
	byte firePrev;
	byte cmPrev;
	byte fire;
	byte cm;			/* the ammo button (countermeasures) */
	byte boostKey;			/* the boost button */
	byte fullThrust;		/* 1 after Reset: the second accelerate action counts, the thrust follows it at once */
	byte viewTarget;		/* the view-target or next-target button */
	byte netFire;			/* the toggle bit of the fire of the network state */
	byte follow;			/* the follow button: the aim follows the target */
	byte pad_29[3];
	float attackTime;		/* the target is followed */
	float emptyTime;		/* without ammo, the next weapon after TimeWSwitchNo */
	float rumbleA;
	float rumbleB;
	float rumbleARate;		/* per second */
	float rumbleBRate;
} cVehicleControls;

cVehicleControls *Controls_ctor(cVehicleControls *self);
/* the first weapon of the world, everything else 0 */
void Controls_Reset(cVehicleControls *self);
byte Controls_VarF(cVehicleControls *self, const char *name, double *d, int set);
int Controls_GetWeapon(cVehicleControls *self);
/* the buttons and axes from the network state of the vehicle v (its +0xf4, bits at +0x74) */
void Controls_FromNet(cVehicleControls *self, cEntity *v, float dt);
/* clears both fire buttons and +0x18 when the weapon changes */
void Controls_SetWeapon(cVehicleControls *self, int w);
/* the two force feedback motors: strength and fade time */
void Controls_RumbleA(cVehicleControls *self, float strength, float time);
void Controls_RumbleB(cVehicleControls *self, float strength, float time);

/* the native functions of the items a ship takes (item name, ship name): the ammo or life of the Item...Adds
 * engine variables, the item hidden for its Item...RespawnTime */
struct cAbstractFile;

#endif
