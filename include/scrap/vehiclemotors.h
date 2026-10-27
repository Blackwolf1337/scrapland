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

#include "scrap/embeddedfxvehicle.h"
#include "scrap/weaps2.h"

#define VM_MOTORS 4
#define VM_SIDES 2

#define CVEHICLEMOTORS_VFTABLE 0x3b56f8

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
	CGameParticles *sistPart[VM_SIDES][2];	/* 0x24 the exhaust particles of the nodes */
	byte visible;			/* 0x34 */
	byte pad_35[3];
	CGameTrail *trail[VM_SIDES];	/* 0x38 none for the traffic (type name T...) */
	float lastThrust;		/* 0x40 trailThrust of the last trail update */
	float trailThrust;		/* 0x44 approaches the thrust */
	vec3_t lastPos;			/* 0x48 the position of the last trail segment */
	float dist;			/* 0x54 from lastPos (squared at a low thrust) */
	float shrinkTime;		/* 0x58 the trails shrink every 0.05 s at a low thrust */
	float f5c;			/* 0x5c */
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
SD_OFFSET(cVehicleMotorsEffects, sistPart, 0x24);
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

#define Vehicle_Motors(e) ((cVehicleMotors *)((byte *)(e) + 0x720))

/* the vehicle fields the motors read */
#define VM_FIELD(e, type, off) (*(type *)((byte *)(e) + (off)))
#define VM_Timer(e) ((cTimer *)((byte *)(e) + 0x110))
#define VM_Thrust(e) VM_FIELD(e, float, 0x4b0)		/* negative backwards */
#define VM_F490(e) VM_FIELD(e, float, 0x490)
#define VM_F494(e) VM_FIELD(e, float, 0x494)
#define VM_F4bc(e) VM_FIELD(e, float, 0x4bc)
#define VM_F4c4(e) VM_FIELD(e, float, 0x4c4)
#define VM_F4c8(e) VM_FIELD(e, float, 0x4c8)
#define VM_LocalVel(e) VM_FIELD(e, vec3_t, 0x4e4)	/* z the forward speed */
#define VM_Hidden(e) VM_FIELD(e, byte, 0x588)		/* no exhaust effects */
#define VM_Team(e) ((void *)((byte *)(e) + 0x314))	/* for cArrowIndicator_GetColor */

/* the emit positions of the Calima particles of all motors (8) */
#define g_CalimaPos ((vec3_t *)Game_Ptr(0x4482b8))

/* the engine variables */
#define g_EnginePathFmt GAME_DATA(const char *, 0x4482b0)		/* EnginePathFmt */
#define g_FXEngineTrailOffset GAME_DATA(float, 0x448328)
#define g_FXVehicleDeathTrailFile GAME_DATA(const char *, 0x448340)
#define g_FXMotorSmokeFile GAME_DATA(const char *, 0x448358)
#define g_FXMotorCalimaOffset GAME_DATA(float, 0x448370)
#define g_FXEngineTrailHideTime GAME_DATA(float, 0x448388)
#define g_FXEngineTrailSize GAME_DATA(float, 0x4483a0)
#define g_FXEngineGlowDistFadeMid GAME_DATA(float, 0x4483b8)
#define g_FXEngineTrailDist GAME_DATA(float, 0x4483d0)
#define g_FXEngineGlowSizeAtt GAME_DATA(float, 0x4483e8)
#define g_FXMotorCalima GAME_DATA(int, 0x448400)
#define g_FXMotorCalimaFile GAME_DATA(const char *, 0x448418)
#define g_FXEngineGlowDistFadeMult GAME_DATA(float, 0x448430)
#define g_FXEngineGlowDistFadeFrom GAME_DATA(float, 0x448448)
#define g_FXEngineGlowDistFadeTo GAME_DATA(float, 0x448460)
#define g_FXEngineGlowSize GAME_DATA(float, 0x4484a8)
#define g_FXEngineGlowSizeMax GAME_DATA(float, 0x4484c0)
#define g_FXEngineTrailDistSpeedFactor GAME_DATA(float, 0x4484d8)

/* VehicleEntity.cpp */
cVehicleMotor *__thiscall cVehicleMotor_ctor(cVehicleMotor *self);
void __fastcall cVehicleMotor_dtor(cVehicleMotor *self);
/* rva 0xf6b20 (Entities/ObjEntity range): the trail color of the team at +0x74 of the team info (0..3, else
 * white) */

/* VehicleMotors.cpp */
void __fastcall cVehicleMotors_Update(cVehicleMotors *self);
/* the models disabled, the nodes and the animations off, the effects cleared */
void __fastcall cVehicleMotor_Reset(cVehicleMotor *self);
void __fastcall cVehicleMotors_Reset(cVehicleMotors *self);
void __fastcall cVehicleMotors_ClearEffects(cVehicleMotors *self);
void __fastcall cVehicleMotors_InitEffects(cVehicleMotors *self);
byte __thiscall cVehicleMotors_VarF(cVehicleMotors *self, const char *name, double *v, byte set);
/* the position and the angles (radians) of the node of motor, side, in the ship model */
byte __thiscall cVehicleMotors_GetExhaust(cVehicleMotors *self, uint32_t motor, uint32_t side, vec3_t *pos,
	vec3_t *ang);
/* the same for the slot DC_Engine<A + motor>_0<1 + side> */
byte __thiscall cVehicleMotors_GetSlot(cVehicleMotors *self, uint32_t motor, uint32_t side, vec3_t *pos,
	vec3_t *ang);
/* the file of motor ("" for none), the number of its nodes into count */
const char *__thiscall cVehicleMotors_GetMotorName(cVehicleMotors *self, uint32_t motor, int *count);
/* the slots DC_Engine<A + motor>_01, _02 of the ship model (0..2) */
int __thiscall cVehicleMotors_NumSlots(cVehicleMotors *self, byte motor);
cEntity *__fastcall cVehicleMotors_GetOwner(cVehicleMotors *self);
byte __thiscall cVehicleMotors_VarI(cVehicleMotors *self, const char *name, int *v, byte set);
void __fastcall cVehicleMotor_Render(cVehicleMotor *self);
void __fastcall cVehicleMotors_Render(cVehicleMotors *self);
void __thiscall cVehicleMotor_SetEnabled(cVehicleMotor *self, byte on);
void __thiscall cVehicleMotors_SetEnabled(cVehicleMotors *self, byte on);
void __thiscall cVehicleMotors_StartAnimF(cVehicleMotors *self, uint32_t motor, const char *anim);
void __thiscall cVehicleMotors_StartAnimM(cVehicleMotors *self, uint32_t motor, const char *anim);
/* the models of file at the nodes of motor kind motor of owner; the number of nodes */
int __thiscall cVehicleMotor_Load(cVehicleMotor *self, cEntity *owner, byte motor, const char *file,
	const vec3_t *scale);
int __thiscall cVehicleMotors_SetMotor(cVehicleMotors *self, uint32_t motor, const char *file, float sx, float sy,
	float sz);
/* the animation anim (EnginePathFmt of the motor file) of channel 0, once */
void __thiscall cVehicleMotor_StartAnimF(cVehicleMotor *self, const char *anim);
/* channel 0 at k (-1..1) of its length */
void __thiscall cVehicleMotor_SetAnimF(cVehicleMotor *self, float k);
/* the animation of channel 1, looped by cVehicleMotor_UpdateAnimM */
void __thiscall cVehicleMotor_StartAnimM(cVehicleMotor *self, const char *anim);
void __thiscall cVehicleMotor_UpdateAnimM(cVehicleMotor *self, float dt);
void __thiscall cVehicleMotors_SetOwner(cVehicleMotors *self, cEntity *owner);
void __thiscall cVehicleMotor_SetEffect(cVehicleMotor *self, uint32_t fx);
void __thiscall cVehicleMotors_SetEffect(cVehicleMotors *self, uint32_t fx);
void __thiscall cVehicleMotor_SetEffectParam(cVehicleMotor *self, int i, float v);
void __thiscall cVehicleMotors_SetEffectParam(cVehicleMotors *self, int i, float v);
byte __thiscall cVehicleMotors_VarS(cVehicleMotors *self, const char *name, char **v, byte set);
byte __thiscall cVehicleMotors_VarV(cVehicleMotors *self, const char *name, double *x, double *y, double *z,
	byte set);
cVehicleMotorsEffects *__fastcall cVehicleMotorsEffects_ctor(cVehicleMotorsEffects *self);
void __fastcall cVehicleMotorsEffects_dtor(cVehicleMotorsEffects *self);
vec3_t *__thiscall Vec3_Assign(vec3_t *self, const vec3_t *v);
/* the particles and trails freed, the nodes and owners cleared */
void __fastcall cVehicleMotorsEffects_Clear(cVehicleMotorsEffects *self);
/* the particles off; a trail of a dead ship goes on as a death trail debris */
void __fastcall cVehicleMotorsEffects_Stop(cVehicleMotorsEffects *self);
void __fastcall cVehicleMotorsEffects_HideTrails(cVehicleMotorsEffects *self);
/* the model of the nodes of side */
CGameModel *__thiscall cVehicleMotorsEffects_GetModel(cVehicleMotorsEffects *self, int side);

/* VehicleMotorsEffects.cpp */
void __fastcall cVehicleMotorsEffects_Init(cVehicleMotorsEffects *self);
/* Rand_FloatRange (rva 0x10e8f0) is declared in weapons.h */
/* the particles of the Calima effect (the hot air under the motors) for n emit positions at pos */
void __stdcall VMEffects_SetupCalima(float thrust, float speed, vec3_t *pos, int n, CGameParticles *ps);
void __thiscall cVehicleMotorsEffects_SetType(cVehicleMotorsEffects *self, int type);
/* the glow particles of the exhausts in ps from *first on (2 each), *first moved on */
void __thiscall cVehicleMotorsEffects_Glow(cVehicleMotorsEffects *self, float thrust, float k, float dist,
	int *first, CGameParticles *ps);
void __thiscall cVehicleMotorsEffects_StopGlow(cVehicleMotorsEffects *self, int *first, CGameParticles *ps);
void __thiscall cVehicleMotorsEffects_SetColor(cVehicleMotorsEffects *self, uint32_t color);
void __thiscall cVehicleMotorsEffects_SetOffset(cVehicleMotorsEffects *self, float v);
void __thiscall cVehicleMotorsEffects_SetScale(cVehicleMotorsEffects *self, float v);
void __thiscall cVehicleMotorsEffects_Set(cVehicleMotorsEffects *self, cEntity *owner, cVehicleMotors *motors,
	cVehicleMotor *motor);
void __thiscall cVehicleMotorsEffects_SetVisible(cVehicleMotorsEffects *self, byte on);
/* rva 0x10f210 (not in C, 34 KB): the exhaust particles of the FxClass type */
void __thiscall cVehicleMotorsEffects_Exhaust(cVehicleMotorsEffects *self, float thrust, float k, float f4bc,
	float speed, byte cloud, float fade);
/* the smoke particles at the exhausts while the ship leaves a cloud */
void __thiscall cVehicleMotorsEffects_Smoke(cVehicleMotorsEffects *self, float dt, CGameParticles *ps);
/* the trails of the exhausts: pos and rot of the ship, vel its local velocity */
void __thiscall cVehicleMotorsEffects_Trails(cVehicleMotorsEffects *self, float dt, vec3_t pos, vec3_t rot,
	vec3_t vel, float thrust, float k, uint32_t color);

#endif
