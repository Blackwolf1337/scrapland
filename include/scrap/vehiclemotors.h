/*
 * Entities/Vehicle/VehicleMotors.cpp and VehicleMotorsEffects.cpp of the game: the motors of a ship.
 * cVehicleMotors (at +0x720 of the vehicle, vtable rva 0x3b56f8 with the script variables VarI, VarF, VarS, VarV)
 * keeps up to four cVehicleMotor, the models of the motor kinds a ship carries (MEng0..3: the file of
 * EnginePathFmt), each hung at one or two render nodes DC_EngineA_01, DC_EngineA_02 (B for the second motor ...) of
 * the ship. cVehicleMotorsEffects are the exhaust effects (DF_MotorTrust nodes: two particle systems, the glow, and
 * a trail each) of the whole ship (the one of cVehicleMotors, nodes DF_MotorTrust_01, _02 of the ship) or of one
 * motor (nodes DF_MotorTrust, DF_MotorTrust_02 of the motor model).
 */
#ifndef SCRAP_VEHICLEMOTORS_H
#define SCRAP_VEHICLEMOTORS_H

#include "scrap/entity.h"
#include "scrap/scrapmodel.h"
#include "scrap/scrapparticles.h"
#include "scrap/scraptrails.h"

#define VM_MOTORS 4
#define VM_SIDES 2

#define CVEHICLEMOTORS_VFTABLE GAME_VFTABLE(cVehicleMotors_vftable)

typedef struct cVehicleMotor cVehicleMotor;
typedef struct cVehicleMotors cVehicleMotors;

/* the exhaust effects (0xa8 bytes) */
typedef struct cVehicleMotorsEffects {
	cEntity *owner;			/* 0x00 the vehicle */
	cVehicleMotors *motors;		/* 0x04 the effects of the ship: the nodes of the ship model */
	cVehicleMotor *motor;		/* 0x08 the effects of one motor: the nodes of its models */
	byte on;			/* 0x0c a DF_MotorTrust node was found */
	byte pad_0d[3];
	int type;			/* 0x10 FxClass, picks the color; 0 no glow */
	int node[VM_SIDES][2];		/* 0x14 the two DF_MotorTrust nodes per side, -1 none */
	CGameParticles *particleSystem[VM_SIDES][2];	/* 0x24 the exhaust particles of the nodes */
	byte visible;			/* 0x34 */
	byte pad_35[3];
	CGameTrail *trail[VM_SIDES];	/* 0x38 none for the traffic (type name T...) */
	float lastThrust;		/* 0x40 trailThrust of the last trail update */
	float trailThrust;		/* 0x44 approaches the thrust */
	vec3_t lastPos;			/* 0x48 the position of the last trail segment */
	float dist;			/* 0x54 from lastPos (squared at a low thrust) */
	float shrinkTime;		/* 0x58 the trails shrink every 0.05 s at a low thrust */
	float unused_5c;			/* 0x5c */
	float colorBlend;		/* 0x60 towards 1 while the color is not white */
	float segDist;			/* 0x64 the length of a trail segment, longer with the speed */
	byte trailsOn;			/* 0x68 the first trail update only sets it */
	byte pad_69[3];
	uint32_t color;			/* 0x6c ShipMotorColorR/G/B */
	uint32_t color2;		/* 0x70 color with each channel squared */
	float scale;			/* 0x74 ShipMotorScale */
	float offset;			/* 0x78 ShipMotorOffset, z of the glow */
	int numExhausts;		/* 0x7c */
	vec3_t exhaustPos[VM_SIDES];	/* 0x80 relative to the ship */
	int exhaustSide[VM_SIDES];	/* 0x98 the side of exhaust i */
	byte glowOn;			/* 0xa0 the glow particles are set */
	byte pad_a1[3];
	float smokeTime;		/* 0xa4 since the last smoke particles */
} cVehicleMotorsEffects;
SD_OFFSET(cVehicleMotorsEffects, type, 0x10);
SD_OFFSET(cVehicleMotorsEffects, particleSystem, 0x24);
SD_OFFSET(cVehicleMotorsEffects, trail, 0x38);
SD_OFFSET(cVehicleMotorsEffects, trailsOn, 0x68);
SD_OFFSET(cVehicleMotorsEffects, numExhausts, 0x7c);
SD_OFFSET(cVehicleMotorsEffects, exhaustSide, 0x98);
SD_OFFSET(cVehicleMotorsEffects, smokeTime, 0xa4);
SD_SIZE(cVehicleMotorsEffects, 0xa8);

/* a motor kind of the ship (0x11c bytes, constructor rva 0x10bb90, destructor 0x10bcb0) */
struct cVehicleMotor {
	CGameModel model[VM_SIDES];	/* 0x00 a copy at each node */
	int node[VM_SIDES];		/* 0x38 DC_Engine<A + motor>_0<1 + side> of the ship, -1 none */
	char name[0x20];		/* 0x40 the file */
	byte animMOn;			/* 0x60 */
	byte pad_61[3];
	float durM;			/* 0x64 the animation of channel 1 (EngAnmM) */
	float timeM;			/* 0x68 */
	byte animFOn;			/* 0x6c */
	byte pad_6d[3];
	float durF;			/* 0x70 the animation of channel 0 (EngAnmF) */
	cVehicleMotorsEffects effects;	/* 0x74 */
};
SD_OFFSET(cVehicleMotor, node, 0x38);
SD_OFFSET(cVehicleMotor, name, 0x40);
SD_OFFSET(cVehicleMotor, durF, 0x70);
SD_OFFSET(cVehicleMotor, effects, 0x74);
SD_SIZE(cVehicleMotor, 0x11c);

/* the motors of a ship (0xc8 bytes at +0x720 of the vehicle, constructor rva 0x10bc20) */
struct cVehicleMotors {
	const void *vftable;		/* 0x00 */
	cEntity *owner;			/* 0x04 */
	cVehicleMotor *motor[VM_MOTORS];	/* 0x08 */
	byte stopped;			/* 0x18 the ship stands still, the effects are not updated */
	byte lastVisible;		/* 0x19 */
	byte pad_1a[2];
	float speed;			/* 0x1c approaches the speed of the ship */
	cVehicleMotorsEffects effects;	/* 0x20 */
};
SD_OFFSET(cVehicleMotors, motor, 0x8);
SD_OFFSET(cVehicleMotors, speed, 0x1c);
SD_OFFSET(cVehicleMotors, effects, 0x20);
SD_SIZE(cVehicleMotors, 0xc8);

#define Vehicle_Motors(e) (&((cVehicleEntity *)(e))->motors)

/* the vehicle fields the motors read */
#define VM_Team(e) ((void *)&WLife(e)->arrow)	/* for cArrowIndicator_GetColor */

/* the emit positions of the Calima particles of all motors (8) */
#define g_HazePos (GAME_ARRAY(vec3_t, g_HazePos))

/* the engine variables */
#define g_EnginePathFmt GAME_GLOBAL_IN(const char *, GVar_EnginePathFmt, 16)		/* EnginePathFmt */
#define g_FXEngineTrailOffset GAME_GLOBAL_IN(float, GVar_FXEngineTrailOffset, 16)
#define g_FXVehicleDeathTrailFile GAME_GLOBAL_IN(const char *, GVar_FXVehicleDeathTrailFile, 16)
#define g_FXMotorSmokeFile GAME_GLOBAL_IN(const char *, GVar_FXMotorSmokeFile, 16)
#define g_FXMotorHazeOffset GAME_GLOBAL_IN(float, GVar_FXMotorHazeOffset, 16)
#define g_FXEngineTrailHideTime GAME_GLOBAL_IN(float, GVar_FXEngineTrailHideTime, 16)
#define g_FXEngineTrailSize GAME_GLOBAL_IN(float, GVar_FXEngineTrailSize, 16)
#define g_FXEngineGlowDistFadeMid GAME_GLOBAL_IN(float, GVar_FXEngineGlowDistFadeMid, 16)
#define g_FXEngineTrailDist GAME_GLOBAL_IN(float, GVar_FXEngineTrailDist, 16)
#define g_FXEngineGlowSizeAtt GAME_GLOBAL_IN(float, GVar_FXEngineGlowSizeAtt, 16)
#define g_FXMotorHaze GAME_GLOBAL_IN(int, GVar_FXMotorHaze, 16)
#define g_FXMotorHazeFile GAME_GLOBAL_IN(const char *, GVar_FXMotorHazeFile, 16)
#define g_FXEngineGlowDistFadeMult GAME_GLOBAL_IN(float, GVar_FXEngineGlowDistFadeMult, 16)
#define g_FXEngineGlowDistFadeFrom GAME_GLOBAL_IN(float, GVar_FXEngineGlowDistFadeFrom, 16)
#define g_FXEngineGlowDistFadeTo GAME_GLOBAL_IN(float, GVar_FXEngineGlowDistFadeTo, 16)
#define g_FXEngineBirthRate GAME_GLOBAL_IN(int, GVar_FXEngineBirthRate, 16)	/* of the exhaust at thrust 0, 100 at full thrust */
#define g_FXEngineGlowSize GAME_GLOBAL_IN(float, GVar_FXEngineGlowSize, 16)
#define g_FXEngineGlowSizeMax GAME_GLOBAL_IN(float, GVar_FXEngineGlowSizeMax, 16)
#define g_FXEngineTrailDistSpeedFactor GAME_GLOBAL_IN(float, GVar_FXEngineTrailDistSpeedFactor, 16)

/* VehicleEntity.cpp */
cVehicleMotor *cVehicleMotor_ctor(cVehicleMotor *self);
void cVehicleMotor_dtor(cVehicleMotor *self);
/* rva 0xf6b20 (Entities/ObjEntity range): the trail color of the team at +0x74 of the team info (0..3, else
 * white) */

/* VehicleMotors.cpp */
void cVehicleMotors_Update(cVehicleMotors *self);
/* the models disabled, the nodes and the animations off, the effects cleared */
void cVehicleMotor_Reset(cVehicleMotor *self);
void cVehicleMotors_Reset(cVehicleMotors *self);
void cVehicleMotors_ClearEffects(cVehicleMotors *self);
void cVehicleMotors_InitEffects(cVehicleMotors *self);
byte cVehicleMotors_VarF(cVehicleMotors *self, const char *name, double *v, byte set);
/* the position and the angles (radians) of the node of motor, side, in the ship model */
byte cVehicleMotors_GetExhaust(cVehicleMotors *self, uint32_t motor, uint32_t side, vec3_t *pos,
	vec3_t *ang);
/* the same for the slot DC_Engine<A + motor>_0<1 + side> */
byte cVehicleMotors_GetSlot(cVehicleMotors *self, uint32_t motor, uint32_t side, vec3_t *pos,
	vec3_t *ang);
/* the file of motor ("" for none), the number of its nodes into count */
const char *cVehicleMotors_GetMotorName(cVehicleMotors *self, uint32_t motor, int *count);
/* the slots DC_Engine<A + motor>_01, _02 of the ship model (0..2) */
int cVehicleMotors_NumSlots(cVehicleMotors *self, byte motor);
cEntity *cVehicleMotors_GetOwner(cVehicleMotors *self);
byte cVehicleMotors_VarI(cVehicleMotors *self, const char *name, int *v, byte set);
void cVehicleMotor_Render(cVehicleMotor *self);
void cVehicleMotors_Render(cVehicleMotors *self);
void cVehicleMotor_SetEnabled(cVehicleMotor *self, byte on);
void cVehicleMotors_SetEnabled(cVehicleMotors *self, byte on);
void cVehicleMotors_StartAnimF(cVehicleMotors *self, uint32_t motor, const char *anim);
void cVehicleMotors_StartAnimM(cVehicleMotors *self, uint32_t motor, const char *anim);
/* the models of file at the nodes of motor kind motor of owner; the number of nodes */
int cVehicleMotor_Load(cVehicleMotor *self, cEntity *owner, byte motor, const char *file,
	const vec3_t *scale);
int cVehicleMotors_SetMotor(cVehicleMotors *self, uint32_t motor, const char *file, float sx, float sy,
	float sz);
/* the animation anim (EnginePathFmt of the motor file) of channel 0, once */
void cVehicleMotor_StartAnimF(cVehicleMotor *self, const char *anim);
/* channel 0 at k (-1..1) of its length */
void cVehicleMotor_SetAnimF(cVehicleMotor *self, float k);
/* the animation of channel 1, looped by cVehicleMotor_UpdateAnimM */
void cVehicleMotor_StartAnimM(cVehicleMotor *self, const char *anim);
void cVehicleMotor_UpdateAnimM(cVehicleMotor *self, float dt);
void cVehicleMotors_SetOwner(cVehicleMotors *self, cEntity *owner);
void cVehicleMotor_SetEffect(cVehicleMotor *self, uint32_t fx);
void cVehicleMotors_SetEffect(cVehicleMotors *self, uint32_t fx);
void cVehicleMotor_SetEffectParam(cVehicleMotor *self, int i, float v);
void cVehicleMotors_SetEffectParam(cVehicleMotors *self, int i, float v);
byte cVehicleMotors_VarS(cVehicleMotors *self, const char *name, char **v, byte set);
byte cVehicleMotors_VarV(cVehicleMotors *self, const char *name, double *x, double *y, double *z,
	byte set);
cVehicleMotorsEffects *cVehicleMotorsEffects_ctor(cVehicleMotorsEffects *self);
void cVehicleMotorsEffects_dtor(cVehicleMotorsEffects *self);
vec3_t *Vec3_Assign(vec3_t *self, const vec3_t *v);
/* the particles and trails freed, the nodes and owners cleared */
void cVehicleMotorsEffects_Clear(cVehicleMotorsEffects *self);
/* the particles off; a trail of a dead ship goes on as a death trail debris */
void cVehicleMotorsEffects_Stop(cVehicleMotorsEffects *self);
void cVehicleMotorsEffects_HideTrails(cVehicleMotorsEffects *self);
/* the model of the nodes of side */
CGameModel *cVehicleMotorsEffects_GetModel(cVehicleMotorsEffects *self, int side);

/* VehicleMotorsEffects.cpp */
void cVehicleMotorsEffects_Init(cVehicleMotorsEffects *self);
/* Rand_FloatRange is declared in weapons.h */
/* the particles of the Calima effect (the hot air under the motors) for n emit positions at pos */
void __stdcall VMEffects_SetupHaze(float thrust, float speed, vec3_t *pos, int n, CGameParticles *ps);
void cVehicleMotorsEffects_SetType(cVehicleMotorsEffects *self, int type);
/* the glow particles of the exhausts in ps from *first on (2 each), *first moved on */
void cVehicleMotorsEffects_Glow(cVehicleMotorsEffects *self, float thrust, float k, float dist,
	int *first, CGameParticles *ps);
void cVehicleMotorsEffects_StopGlow(cVehicleMotorsEffects *self, int *first, CGameParticles *ps);
void cVehicleMotorsEffects_SetColor(cVehicleMotorsEffects *self, uint32_t color);
void cVehicleMotorsEffects_SetOffset(cVehicleMotorsEffects *self, float v);
void cVehicleMotorsEffects_SetScale(cVehicleMotorsEffects *self, float v);
void cVehicleMotorsEffects_Set(cVehicleMotorsEffects *self, cEntity *owner, cVehicleMotors *motors,
	cVehicleMotor *motor);
void cVehicleMotorsEffects_SetVisible(cVehicleMotorsEffects *self, byte on);
/* the exhaust particles of the FxClass type at both sides: thrust * f4bc the thrust, speed the speed factor of
 * the motors (0 hides them), one frame in four while the ship leaves a cloud, fade the alpha */
void cVehicleMotorsEffects_Exhaust(cVehicleMotorsEffects *self, float thrust, float k, float f4bc,
	float speed, byte cloud, float fade);
/* the smoke particles at the exhausts while the ship leaves a cloud */
void cVehicleMotorsEffects_Smoke(cVehicleMotorsEffects *self, float dt, CGameParticles *ps);
/* the trails of the exhausts: pos and rot of the ship, vel its local velocity */
void cVehicleMotorsEffects_Trails(cVehicleMotorsEffects *self, float dt, vec3_t pos, vec3_t rot,
	vec3_t vel, float thrust, float k, uint32_t color);

#endif
