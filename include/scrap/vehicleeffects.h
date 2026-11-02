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

/* the speed trails at the nodes DF_SpeedTrail_01..04 (0x60 bytes) */
typedef struct cVehicleTrails {
	cVehicleEntity *owner;		/* 0x00 m_Owner */
	int count;			/* 0x04 the nodes found */
	float fade;			/* 0x08 approaches the horizontal speed factor */
	CGameTrail trails[VEFF_TRAILS];	/* 0x0c */
	int nodes[VEFF_TRAILS];		/* 0x1c -1 for none */
	float dist;			/* 0x2c from the last point */
	vec3_t last;			/* 0x30 the center at the last point */
	byte pad_3c[0x48 - 0x3c];
	float time;			/* 0x48 since the last shrink */
	uint32_t color;			/* 0x4c */
	float segment;			/* 0x50 approaches the segment length */
	vec3_t root;			/* 0x54 the node DC_Root */
} cVehicleTrails;

/* the escape tubes of the SLifeBoat at the nodes DF_EscapeTube_01, 02 (0x17c bytes) */
typedef struct cVehicleTubes {
	cVehicleEntity *owner;		/* 0x00 m_Owner */
	int count;			/* 0x04 the nodes found */
	int unused_08;			/* 0x08 */
	int kind;			/* 0x0c VEFF_TUBES_ */
	float time;			/* 0x10 since the last emission */
	int nodes[VEFF_TUBES];		/* 0x14 -1 for none */
	CGameParticles particles[VEFF_TUBES];	/* 0x1c */
} cVehicleTubes;

typedef struct cVehicleEffects {
	cVehicleEntity *owner;		/* 0x00 m_Owner */
	byte damageOn;			/* 0x04 */
	byte pad_05[3];
	float damageTime;		/* 0x08 since the life went low */
	byte emiOn;			/* 0x0c */
	byte pad_0d[3];
	float emiTime;			/* 0x10 */
	byte fireOn;			/* 0x14 */
	byte pad_15[3];
	float fireTime;			/* 0x18 */
	cVehicleTrails *trails;		/* 0x1c NULL for none */
	cVehicleTubes *tubes;		/* 0x20 NULL for none */
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
