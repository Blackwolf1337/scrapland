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
SD_OFFSET(cVehicleTrails, nodes, 0x1c);
SD_OFFSET(cVehicleTrails, time, 0x48);
SD_SIZE(cVehicleTrails, 0x60);

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
SD_OFFSET(cVehicleTubes, particles, 0x1c);
SD_SIZE(cVehicleTubes, 0x17c);

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
SD_OFFSET(cVehicleEffects, trails, 0x1c);
SD_SIZE(cVehicleEffects, 0x24);

/* the engine variables of the effects */
#define g_FXVehicleDamageFile GAME_GLOBAL_IN(const char *, GVar_FXVehicleDamageFile, 16)
#define g_FXVehicleEMIFile GAME_GLOBAL_IN(const char *, GVar_FXVehicleEMIFile, 16)
#define g_FXVehicleSputnikFile GAME_GLOBAL_IN(const char *, GVar_FXVehicleSputnikFile, 16)
#define g_FXVehicleFireFile GAME_GLOBAL_IN(const char *, GVar_FXVehicleFireFile, 16)
#define g_FXVehicleSpeedTrailDist GAME_GLOBAL_IN(float, GVar_FXVehicleSpeedTrailDist, 16)
#define g_FXVehicleSpeedTrailFadeOutH GAME_GLOBAL_IN(float, GVar_FXVehicleSpeedTrailFadeOutH, 16)
#define g_FXVehicleSpeedTrailSize GAME_GLOBAL_IN(float, GVar_FXVehicleSpeedTrailSize, 16)
#define g_WindFxSmoke GAME_GLOBAL_IN(int, GVar_WindFxSmoke, 16)
#define g_WindFxSmokeFile GAME_GLOBAL_IN(const char *, GVar_WindFxSmokeFile, 16)
#define g_WindFXSmokeMaxDist GAME_GLOBAL_IN(float, GVar_WindFXSmokeMaxDist, 16)
#define g_WindFXSmokeMinVel GAME_GLOBAL_IN(float, GVar_WindFXSmokeMinVel, 16)
#define g_WindFXSmokeTimeStep GAME_GLOBAL_IN(float, GVar_WindFXSmokeTimeStep, 16)
#define g_WindFXSmokeSizeDiv GAME_GLOBAL_IN(float, GVar_WindFXSmokeSizeDiv, 16)
#define g_WindFXSmokeLast GAME_GLOBAL(float, g_WindFXSmokeLast)	/* the world time of the last wind smoke */

/* the parts before the range of the file in the exe */
cVehicleEffects *cVehicleEffects_ctor(cVehicleEffects *self);
/* the constructors of the optional parts (Init inlines them) */
cVehicleTubes *cVehicleTubes_ctor(cVehicleTubes *self);
cVehicleTrails *cVehicleTrails_ctor(cVehicleTrails *self);
void cVehicleEffects_dtor(cVehicleEffects *self);
/* the trails and tubes that found their nodes */
void cVehicleEffects_Update(cVehicleEffects *self, float dt);
/* the smoke puffs of the escape tubes every 0.025 s, stronger with the sink speed */
void cVehicleTubes_Update(cVehicleTubes *self, float dt);

void cVehicleEffects_Reset(cVehicleEffects *self);
void cVehicleTubes_Reset(cVehicleTubes *self);
void cVehicleTrails_Clear(cVehicleTrails *self);
/* the trails and tubes made or deleted for the model of the owner */
void cVehicleEffects_Init(cVehicleEffects *self);
void cVehicleTubes_Init(cVehicleTubes *self);
void cVehicleTrails_Init(cVehicleTrails *self);
void cVehicleEffects_SetActive(cVehicleEffects *self, byte on);
/* the effect of that name: VehicleDamage, VehicleEMI, VehicleSputnik(Make, Suck), VehicleFire, VehicleSpawn */
void cVehicleEffects_Start(cVehicleEffects *self, const char *name);
void cVehicleEffects_SetOwner(cVehicleEffects *self, cEntity *e);
void cVehicleTubes_SetOwner(cVehicleTubes *self, cEntity *e);
void cVehicleTrails_SetOwner(cVehicleTrails *self, cEntity *e);
/* the speed trails of a fast ship that sinks: a point every segment, the last point follows the nodes, faded
 * with the horizontal speed */
void cVehicleTrails_Update(cVehicleTrails *self, float dt);
/* the smoke the wind of a fast ship blows off the ground at x y z */
void cVehicleEffects_WindSmoke(cVehicleEffects *self, float x, float y, float z);
/* the damage, EMI, fire and wind smoke effects started and stopped */
void cVehicleEffects_UpdateState(cVehicleEffects *self, float dt);

#endif
