/*
 * Entities/EmbeddedFX/EmbeddedFXMisc.cpp of the game: the effects embedded in the object entities ("Obj", the
 * pointer at cEntity + 0x224): glows, flares and trails on the particle systems of render nodes of the item and
 * misc models, made by the model name in EmbeddedFXMisc_Create (DataPack, DataCube, Capsule, Matrix, Amplifier,
 * DTritusDesktop, MeetingPoint, ExtraLife, Flag, Moto, BossShield, Conversor, Battery, Treasure, the coins and the
 * items).
 * The vtable is the one of cEmbeddedFX (embeddedfxchar.h): 0 Free, 1 Reset, 2 Release, 3 Update (dt),
 * 4 Tick (empty in all of them), 5 Stop (keep: 0 the trails cleared).
 * Most Updates draw only while the model is loaded, visible and without a model effect; visible at 0x08 keeps
 * whether they drew in the last frame.
 */
#ifndef SCRAP_EMBEDDEDFXMISC_H
#define SCRAP_EMBEDDEDFXMISC_H

#include "scrap/embeddedfxchar.h"
#include "scrap/filesys.h"

#define TYEN_OBJECT "Obj"

typedef struct cEmbeddedFXAmplifier {
	cEmbeddedFX base;
	byte visible;
	byte pad_09[3];
	int dummyFlares;		/* DOB_Amplificador_Flares */
	CGameParticles flares;
	int dummyRoot;			/* OB_Amplificador_Cuerpo */
	CGameTrail trail;
	float t;
} cEmbeddedFXAmplifier;

#define FXBATTERY_TRAILS 3

/* a ray of the battery, between two random points of the root */
typedef struct cEmbeddedFXBatteryRay {
	vec3_t a;
	vec3_t b;
} cEmbeddedFXBatteryRay;

typedef struct cEmbeddedFXBattery {
	cEmbeddedFX base;
	byte visible;
	byte pad_09[3];
	int dummyGlow;			/* DOB_Batery_Glow */
	CGameParticles glow;		/* the ray ends and the sparks */
	int dummyRoot;			/* DC_Root */
	float sparkT;			/* to the next sparks */
	CGameTrail trails[FXBATTERY_TRAILS];	/* the rays */
	cEmbeddedFXBatteryRay rays[FXBATTERY_TRAILS];
	float life[FXBATTERY_TRAILS];	/* of the ray */
	float delay[FXBATTERY_TRAILS];	/* to the next ray, 0.4..1.2 after the Reset */
} cEmbeddedFXBattery;

#define FXBOSSSHIELD_TRAILS 16
#define FXBOSSSHIELD_SLOTS 0xff

typedef struct cEmbeddedFXBossShield {
	cEmbeddedFX base;
	CGameParticles particles;	/* Dummy01, a glow per slot */
	float t[FXBOSSSHIELD_SLOTS];	/* 0..1, -1 after the Reset */
	float speed[FXBOSSSHIELD_SLOTS];
	vec3_t dir[FXBOSSSHIELD_SLOTS];	/* on the shield */
	float size[FXBOSSSHIELD_SLOTS];
	uint32_t color[FXBOSSSHIELD_SLOTS];
	int mode;			/* 1, 2 shown (the color set), else hidden; the network message */
	float regen;			/* Value of the RegenerationBar of the scorer */
	float hit;			/* 1 on a hit, falls to 0 */
	float spin;			/* of the rings */
	float f1cac;
	CGameTrail trails[FXBOSSSHIELD_TRAILS];	/* the rings */
} cEmbeddedFXBossShield;

#define FXCAPSULE_MOTORS 3

typedef struct cEmbeddedFXCapsule {
	cEmbeddedFX base;
	CGameParticles motors[FXCAPSULE_MOTORS];	/* DS_CapsulaBill_Motor_01.. */
} cEmbeddedFXCapsule;

/* the coins; the items use the same layout and Init */
typedef struct cEmbeddedFXCoin {
	cEmbeddedFX base;
	byte visible;
	byte pad_09[3];
	int dummyGlow;
	CGameParticles glow;
	float scale;
	uint32_t color;
} cEmbeddedFXCoin;

typedef struct cEmbeddedFXItem {
	cEmbeddedFXCoin coin;
	int phase;			/* 0..511 */
} cEmbeddedFXItem;

typedef struct cEmbeddedFXConversor {
	cEmbeddedFX base;
	byte visible;
	byte pad_09[3];
	int dummyGlow;			/* DOB_Conversor-Mandos_Glow */
	CGameParticles glow;
	float t;
} cEmbeddedFXConversor;

typedef struct cEmbeddedFXDTritusDesktop {
	cEmbeddedFX base;
	byte visible;
	byte pad_09[3];
	float t;			/* the blink of the calling light */
	int dummyGlow;			/* DOB_Interfono_Calling */
	CGameParticles glow;
	int dummyRoot;			/* OB_Interfono_Base */
	float arcT;			/* 0..1 */
	CGameTrail trail;
} cEmbeddedFXDTritusDesktop;

typedef struct cEmbeddedFXDataCube {
	cEmbeddedFX base;
	int dummyGlow;			/* DOB_DataCube_Glow */
	CGameParticles glow;
	int dummyFlares;		/* DOB_DataCube_Flares */
	CGameParticles flares;
} cEmbeddedFXDataCube;

typedef struct cEmbeddedFXDataPack {
	cEmbeddedFX base;
	byte visible;
	byte on;			/* the laser (the global DataPack switch) */
	byte shown;			/* the state level last reached */
	byte pad_0b;
	float level;			/* 0..1 the laser faded in */
	int dummyRoot;			/* DC_Root */
	vec3_t rootPos;
	int dummyGlow;			/* DOB_DataPack_Glow */
	CGameParticles glow;
	int dummyCodec;			/* DOB_DataPack_Letras */
	CGameParticles codec;
	int dummyLaser;			/* DF_Laser */
	CGameParticles laser;
	CGameTrail trail;
} cEmbeddedFXDataPack;

typedef struct cEmbeddedFXExtraLife {
	cEmbeddedFX base;
	byte visible;
	byte pad_09[3];
	int dummyGlow;			/* DOB_ExtraLife_Glow */
	CGameParticles glow;
	int dummyRoot;			/* DC_Root */
	int dummyBar;			/* OB_ExtraLife_Barra_01 */
	CGameTrail trail;		/* the rays */
	float scale;
	float t;			/* the pulse of the bar glows */
} cEmbeddedFXExtraLife;

typedef struct cEmbeddedFXFlag {
	cEmbeddedFX base;
	byte visible;
	byte pad_09[3];
	int dummy;			/* DOB_FlapCTF_Glow */
	CGameParticles particles;
	float t;
	float scale;
	uint32_t color;
	uint32_t color2;
} cEmbeddedFXFlag;

typedef struct cEmbeddedFXMatrix {
	cEmbeddedFX base;
	byte visible;
	byte isMatrix;			/* model Matrix.m3d */
	byte pad_0a[2];
	int dummyGlow;			/* DOB_DataCube_Glow */
	CGameParticles glow;
	int dummyFlares;		/* DOB_Matrix_Flares */
	CGameParticles flares;
	int dummyRoot;			/* DC_Root */
	float timer[3];			/* of the three rays */
	float dur[3];
	vec3_t off[3];			/* the random ends of the rays */
	CGameTrail trail;
	float scale;			/* 1, MatrixExt 6 */
} cEmbeddedFXMatrix;

typedef struct cEmbeddedFXMeetingPoint {
	cEmbeddedFX base;
	byte visible;
	byte pad_09[3];
	int dummy;			/* DF_Light */
	CGameParticles light;
	float t;			/* the flash of the light */
	CGameParticles flares;		/* DOB_MeetingPoint_Flares */
	float countT;			/* -1: no count down */
	CGameParticles countDown;	/* DF_Light_02 */
	float countA;
	float countB;
} cEmbeddedFXMeetingPoint;

#define FXBIKE_ENGINES 3
#define FXBIKE_DOWN 2
#define FXBIKE_TUBES 6

typedef struct cEmbeddedFXBike {
	cEmbeddedFX base;
	float t[FXBIKE_ENGINES];	/* the time since the last smoke of each engine */
	vec3_t pos[FXBIKE_ENGINES];	/* the engine at the last smoke (the smoke fills the way from there) */
	int nodeEngine[FXBIKE_ENGINES];	/* DF_MotorTrust_01..03 */
	CGameParticles engine[FXBIKE_ENGINES];
	CGameParticles down[FXBIKE_DOWN];	/* DF_MotorTrust_04, 05 */
	CGameParticles tube[FXBIKE_TUBES];	/* DF_EscapeTube_01..06 */
	float throttle;			/* 0..1, the size of the flames and the smoke */
	float boost;
	byte on;			/* the engines run (SetF7d4) */
	byte pad_7dd[3];
} cEmbeddedFXBike;

typedef struct cEmbeddedFXTreasure {
	cEmbeddedFX base;
	byte visible;
	byte pad_09[3];
	int dummyGlow;			/* DOB_Treasure_Glow */
	CGameParticles glow;
	int dummyRoot;			/* DC_Root */
	float timer;			/* to the next spark */
	float dur;			/* of the spark */
	float life;			/* of the spark */
	vec3_t spark;			/* the position of the spark */
	float rot;
	float rotSpeed;
	CGameTrail trail;
	int type;			/* 1 A, 2 B, 3 C */
	float scale;
} cEmbeddedFXTreasure;

/* the effect of the object model replaces *fx; other models leave *fx as it is */
void EmbeddedFXMisc_Create(cEntity *obj, const char *model, cEmbeddedFX **fx);
vec3_t *cEmbeddedFXBike_EnginePos(cEmbeddedFXBike *self, vec3_t *out);
void cEmbeddedFXBike_SetThrottle(cEmbeddedFXBike *self, float a, float b);
void cEmbeddedFXDataPack_SetOn(cEmbeddedFXDataPack *self, byte on);

#endif
