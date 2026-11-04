/*
 * Entities/Vehicle/VehicleEffects.cpp of the game: the effects a vehicle (cVehicleEntity, "Car") drives itself, the
 * cVehicleEffects at vehicle + 0x7f0. It starts the damage, EMI and fire effects (debris elements of
 * World/FX/FXVehicle.cpp) when the life is low, the EMI counter runs or the ship burns, the wind smoke under a fast
 * player ship, and keeps two optional parts: the speed trails of the ships (not for the traffic "T..." and the
 * SMosquito) and the escape tubes of the SLifeBoat. src/game/Entities/Vehicle/VehicleEffects.c. The constructor,
 * destructor, Update and the escape tubes sit before the CHECK range of the file in the exe.
 * The shoot effects (cVehicleEffects_Shoot, declared in include/scrap/weapons.h) are particles of m_FollowSistPart2
 * of the embedded FX at the weapon node of the slot.
 */
#ifndef SCRAP_VEHICLEEFFECTS_H
#define SCRAP_VEHICLEEFFECTS_H

#include "scrap/gvar.h"
#include "scrap/entity.h"
#include "scrap/scrapparticles.h"
#include "scrap/scraptrails.h"

typedef struct cVehicleEntity cVehicleEntity;

#define VEFF_TRAILS 4
#define VEFF_TUBES 2

#define VEFF_TUBES_LIFEBOAT 0
#define VEFF_TUBES_MERC 1
#define VEFF_TUBES_NONE 2

/* the speed trails at the nodes DF_SpeedTrail_01..04 */
typedef struct cVehicleTrails {
	cVehicleEntity *owner;		/* m_Owner */
	int count;			/* the nodes found */
	float fade;			/* approaches the horizontal speed factor */
	CGameTrail trails[VEFF_TRAILS];
	int nodes[VEFF_TRAILS];		/* -1 for none */
	float dist;			/* from the last point */
	vec3_t last;			/* the center at the last point */
	float time;			/* since the last shrink */
	uint32_t color;
	float segment;			/* approaches the segment length */
	vec3_t root;			/* the node DC_Root */
} cVehicleTrails;

/* the escape tubes of the SLifeBoat at the nodes DF_EscapeTube_01, 02 */
typedef struct cVehicleTubes {
	cVehicleEntity *owner;		/* m_Owner */
	int count;			/* the nodes found */
	int unused_08;
	int kind;			/* VEFF_TUBES_ */
	float time;			/* since the last emission */
	int nodes[VEFF_TUBES];		/* -1 for none */
	CGameParticles particles[VEFF_TUBES];
} cVehicleTubes;

typedef struct cVehicleEffects {
	cVehicleEntity *owner;		/* m_Owner */
	byte damageOn;
	byte pad_05[3];
	float damageTime;		/* since the life went low */
	byte emiOn;
	byte pad_0d[3];
	float emiTime;
	byte fireOn;
	byte pad_15[3];
	float fireTime;
	cVehicleTrails *trails;		/* NULL for none */
	cVehicleTubes *tubes;		/* NULL for none */
} cVehicleEffects;

/* the parts before the range of the file in the exe */
cVehicleEffects *cVehicleEffects_ctor(cVehicleEffects *self);
void cVehicleEffects_dtor(cVehicleEffects *self);
/* the trails and tubes that found their nodes */
void cVehicleEffects_Update(cVehicleEffects *self, float dt);

void cVehicleEffects_Reset(cVehicleEffects *self);
/* the trails and tubes made or deleted for the model of the owner */
void cVehicleEffects_Init(cVehicleEffects *self);
void cVehicleEffects_SetActive(cVehicleEffects *self, byte on);
/* the effect of that name: VehicleDamage, VehicleEMI, VehicleSputnik(Make, Suck), VehicleFire, VehicleSpawn */
void cVehicleEffects_Start(cVehicleEffects *self, const char *name);
void cVehicleEffects_SetOwner(cVehicleEffects *self, cEntity *e);
/* the damage, EMI, fire and wind smoke effects started and stopped */
void cVehicleEffects_UpdateState(cVehicleEffects *self, float dt);

#endif
