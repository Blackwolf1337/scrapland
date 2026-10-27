/*
 * Entities/EmbeddedFX/EmbeddedFXMisc.cpp of the game: the effects embedded in the object entities ("Obj", the
 * pointer at cEntity + 0x224): glows, flares and trails on the particle systems of render nodes of the item and
 * misc models, made by the model name in EmbeddedFXMisc_Create (DataPack, DataCube, Capsule, Matrix, Amplifier,
 * DTritusDesktop, MeetingPoint, ExtraLife, Flag, Moto, BossShield, Conversor, Battery, Treasure, the coins and the
 * items).
 * The vtable is the one of cEmbeddedFX (embeddedfxchar.h): 0 deleting destructor, 1 Reset, 2 Release, 3 Update (dt),
 * 4 Tick (empty in all of them), 5 Stop (keep: 0 the trails cleared).
 * Most Updates draw only while the model is loaded, visible and without a model effect; visible at 0x08 keeps
 * whether they drew in the last frame.
 */
#ifndef SCRAP_EMBEDDEDFXMISC_H
#define SCRAP_EMBEDDEDFXMISC_H

#include "scrap/embeddedfxchar.h"

#define TYEN_OBJECT "Obj"

#define VT_EFX_DATAPACK 0x3b14b8
#define VT_EFX_DATACUBE 0x3b14d4
#define VT_EFX_CAPSULE 0x3b14f0
#define VT_EFX_MATRIX 0x3b150c
#define VT_EFX_AMPLIFIER 0x3b1528
#define VT_EFX_MEETINGPOINT 0x3b1544
#define VT_EFX_EXTRALIFE 0x3b1560
#define VT_EFX_BATTERY 0x3b157c
#define VT_EFX_TREASURE 0x3b1598
#define VT_EFX_FLAG 0x3b15b4
#define VT_EFX_DTRITUSDESKTOP 0x3b15d0
#define VT_EFX_MOTO 0x3b15ec
#define VT_EFX_BOSSSHIELD 0x3b1608
#define VT_EFX_CONVERSOR 0x3b1624
#define VT_EFX_COIN 0x3b1640
#define VT_EFX_ITEM 0x3b165c

typedef struct cEmbeddedFXAmplifier {
	cEmbeddedFX base;
	byte visible;			/* 0x08 */
	byte pad_09[3];
	int dummyFlares;		/* 0x0c DOB_Amplificador_Flares */
	CGameParticles flares;		/* 0x10 */
	int dummyRoot;			/* 0xc0 OB_Amplificador_Cuerpo */
	CGameTrail trail;		/* 0xc4 */
	float t;			/* 0xc8 */
} cEmbeddedFXAmplifier;
SD_OFFSET(cEmbeddedFXAmplifier, trail, 0xc4);
SD_SIZE(cEmbeddedFXAmplifier, 0xcc);

#define FXBATTERY_TRAILS 3

/* a ray of the battery, between two random points of the root */
typedef struct cEmbeddedFXBatteryRay {
	vec3_t a;
	vec3_t b;
} cEmbeddedFXBatteryRay;

typedef struct cEmbeddedFXBattery {
	cEmbeddedFX base;
	byte visible;			/* 0x008 */
	byte pad_09[3];
	int dummyGlow;			/* 0x00c DOB_Batery_Glow */
	CGameParticles glow;		/* 0x010 the ray ends and the sparks */
	int dummyRoot;			/* 0x0c0 DC_Root */
	float sparkT;			/* 0x0c4 to the next sparks */
	CGameTrail trails[FXBATTERY_TRAILS];	/* 0x0c8 the rays */
	cEmbeddedFXBatteryRay rays[FXBATTERY_TRAILS];	/* 0x0d4 */
	float life[FXBATTERY_TRAILS];	/* 0x11c of the ray */
	float delay[FXBATTERY_TRAILS];	/* 0x128 to the next ray, 0.4..1.2 after the Reset */
} cEmbeddedFXBattery;
SD_OFFSET(cEmbeddedFXBattery, trails, 0xc8);
SD_OFFSET(cEmbeddedFXBattery, rays, 0xd4);
SD_OFFSET(cEmbeddedFXBattery, life, 0x11c);
SD_SIZE(cEmbeddedFXBattery, 0x134);

#define FXBOSSSHIELD_TRAILS 16
#define FXBOSSSHIELD_SLOTS 0xff

typedef struct cEmbeddedFXBossShield {
	cEmbeddedFX base;
	CGameParticles particles;	/* 0x0008 Dummy01, a glow per slot */
	float t[FXBOSSSHIELD_SLOTS];	/* 0x00b8 0..1, -1 after the Reset */
	float speed[FXBOSSSHIELD_SLOTS];	/* 0x04b4 */
	vec3_t dir[FXBOSSSHIELD_SLOTS];	/* 0x08b0 on the shield */
	float size[FXBOSSSHIELD_SLOTS];	/* 0x14a4 */
	uint32_t color[FXBOSSSHIELD_SLOTS];	/* 0x18a0 */
	int mode;			/* 0x1c9c 1, 2 shown (the color set), else hidden; the network message */
	float regen;			/* 0x1ca0 Value of the RegenerationBar of the scorer */
	float hit;			/* 0x1ca4 1 on a hit, falls to 0 */
	float spin;			/* 0x1ca8 of the rings */
	float f1cac;			/* 0x1cac */
	CGameTrail trails[FXBOSSSHIELD_TRAILS];	/* 0x1cb0 the rings */
} cEmbeddedFXBossShield;
SD_OFFSET(cEmbeddedFXBossShield, dir, 0x8b0);
SD_OFFSET(cEmbeddedFXBossShield, color, 0x18a0);
SD_OFFSET(cEmbeddedFXBossShield, mode, 0x1c9c);
SD_OFFSET(cEmbeddedFXBossShield, trails, 0x1cb0);
SD_SIZE(cEmbeddedFXBossShield, 0x1cf0);

#define FXCAPSULE_MOTORS 3

typedef struct cEmbeddedFXCapsule {
	cEmbeddedFX base;
	CGameParticles motors[FXCAPSULE_MOTORS];	/* 0x08 DS_CapsulaBill_Motor_01.. */
} cEmbeddedFXCapsule;
SD_SIZE(cEmbeddedFXCapsule, 0x218);

/* the coins; the items use the same layout and Init */
typedef struct cEmbeddedFXCoin {
	cEmbeddedFX base;
	byte visible;			/* 0x08 */
	byte pad_09[3];
	int dummyGlow;			/* 0x0c */
	CGameParticles glow;		/* 0x10 */
	float scale;			/* 0xc0 */
	uint32_t color;			/* 0xc4 */
} cEmbeddedFXCoin;
SD_SIZE(cEmbeddedFXCoin, 0xc8);

typedef struct cEmbeddedFXItem {
	cEmbeddedFXCoin coin;
	int phase;			/* 0xc8 0..511 */
} cEmbeddedFXItem;
SD_SIZE(cEmbeddedFXItem, 0xcc);

typedef struct cEmbeddedFXConversor {
	cEmbeddedFX base;
	byte visible;			/* 0x08 */
	byte pad_09[3];
	int dummyGlow;			/* 0x0c DOB_Conversor-Mandos_Glow */
	CGameParticles glow;		/* 0x10 */
	float t;			/* 0xc0 */
} cEmbeddedFXConversor;
SD_SIZE(cEmbeddedFXConversor, 0xc4);

typedef struct cEmbeddedFXDTritusDesktop {
	cEmbeddedFX base;
	byte visible;			/* 0x08 */
	byte pad_09[3];
	float t;			/* 0x0c the blink of the calling light */
	int dummyGlow;			/* 0x10 DOB_Interfono_Calling */
	CGameParticles glow;		/* 0x14 */
	int dummyRoot;			/* 0xc4 OB_Interfono_Base */
	float arcT;			/* 0xc8 0..1 */
	CGameTrail trail;		/* 0xcc */
} cEmbeddedFXDTritusDesktop;
SD_OFFSET(cEmbeddedFXDTritusDesktop, trail, 0xcc);
SD_SIZE(cEmbeddedFXDTritusDesktop, 0xd0);

typedef struct cEmbeddedFXDataCube {
	cEmbeddedFX base;
	int dummyGlow;			/* 0x08 DOB_DataCube_Glow */
	CGameParticles glow;		/* 0x0c */
	int dummyFlares;		/* 0xbc DOB_DataCube_Flares */
	CGameParticles flares;		/* 0xc0 */
} cEmbeddedFXDataCube;
SD_SIZE(cEmbeddedFXDataCube, 0x170);

typedef struct cEmbeddedFXDataPack {
	cEmbeddedFX base;
	byte visible;			/* 0x008 */
	byte on;			/* 0x009 the laser (the global DataPack switch) */
	byte shown;			/* 0x00a the state level last reached */
	byte pad_0b;
	float level;			/* 0x00c 0..1 the laser faded in */
	int dummyRoot;			/* 0x010 DC_Root */
	vec3_t rootPos;			/* 0x014 */
	int dummyGlow;			/* 0x020 DOB_DataPack_Glow */
	CGameParticles glow;		/* 0x024 */
	int dummyCodec;			/* 0x0d4 DOB_DataPack_Letras */
	CGameParticles codec;		/* 0x0d8 */
	int dummyLaser;			/* 0x188 DF_Laser */
	CGameParticles laser;		/* 0x18c */
	CGameTrail trail;		/* 0x23c */
} cEmbeddedFXDataPack;
SD_OFFSET(cEmbeddedFXDataPack, codec, 0xd8);
SD_OFFSET(cEmbeddedFXDataPack, trail, 0x23c);
SD_SIZE(cEmbeddedFXDataPack, 0x240);

typedef struct cEmbeddedFXExtraLife {
	cEmbeddedFX base;
	byte visible;			/* 0x08 */
	byte pad_09[3];
	int dummyGlow;			/* 0x0c DOB_ExtraLife_Glow */
	CGameParticles glow;		/* 0x10 */
	int dummyRoot;			/* 0xc0 DC_Root */
	int dummyBarra;			/* 0xc4 OB_ExtraLife_Barra_01 */
	CGameTrail trail;		/* 0xc8 the rays */
	float scale;			/* 0xcc */
	float t;			/* 0xd0 the pulse of the bar glows */
} cEmbeddedFXExtraLife;
SD_OFFSET(cEmbeddedFXExtraLife, trail, 0xc8);
SD_SIZE(cEmbeddedFXExtraLife, 0xd4);

typedef struct cEmbeddedFXFlag {
	cEmbeddedFX base;
	byte visible;			/* 0x08 */
	byte pad_09[3];
	int dummy;			/* 0x0c DOB_FlapCTF_Glow */
	CGameParticles particles;	/* 0x10 */
	float t;			/* 0xc0 */
	float scale;			/* 0xc4 */
	uint32_t color;			/* 0xc8 */
	uint32_t color2;		/* 0xcc */
} cEmbeddedFXFlag;
SD_SIZE(cEmbeddedFXFlag, 0xd0);

typedef struct cEmbeddedFXMatrix {
	cEmbeddedFX base;
	byte visible;			/* 0x008 */
	byte isMatrix;			/* 0x009 model Matrix.m3d */
	byte pad_0a[2];
	int dummyGlow;			/* 0x00c DOB_DataCube_Glow */
	CGameParticles glow;		/* 0x010 */
	int dummyFlares;		/* 0x0c0 DOB_Matrix_Flares */
	CGameParticles flares;		/* 0x0c4 */
	int dummyRoot;			/* 0x174 DC_Root */
	float timer[3];			/* 0x178 of the three rays */
	float dur[3];			/* 0x184 */
	vec3_t off[3];			/* 0x190 the random ends of the rays */
	CGameTrail trail;		/* 0x1b4 */
	float scale;			/* 0x1b8 1, MatrixExt 6 */
} cEmbeddedFXMatrix;
SD_OFFSET(cEmbeddedFXMatrix, flares, 0xc4);
SD_OFFSET(cEmbeddedFXMatrix, off, 0x190);
SD_OFFSET(cEmbeddedFXMatrix, trail, 0x1b4);
SD_SIZE(cEmbeddedFXMatrix, 0x1bc);

typedef struct cEmbeddedFXMeetingPoint {
	cEmbeddedFX base;
	byte visible;			/* 0x008 */
	byte pad_09[3];
	int dummy;			/* 0x00c DF_Light */
	CGameParticles light;		/* 0x010 */
	float t;			/* 0x0c0 the flash of the light */
	CGameParticles flares;		/* 0x0c4 DOB_MeetingPoint_Flares */
	float countT;			/* 0x174 -1: no count down */
	CGameParticles countDown;	/* 0x178 DF_Light_02 */
	float countA;			/* 0x228 */
	float countB;			/* 0x22c */
} cEmbeddedFXMeetingPoint;
SD_OFFSET(cEmbeddedFXMeetingPoint, flares, 0xc4);
SD_OFFSET(cEmbeddedFXMeetingPoint, countDown, 0x178);
SD_SIZE(cEmbeddedFXMeetingPoint, 0x230);

#define FXMOTO_ENGINES 3
#define FXMOTO_DOWN 2
#define FXMOTO_TUBES 6

typedef struct cEmbeddedFXMoto {
	cEmbeddedFX base;
	float t[FXMOTO_ENGINES];	/* 0x008 the times of the engines (the Update is in the exe) */
	vec3_t pos[FXMOTO_ENGINES];	/* 0x014 */
	int nodeEngine[FXMOTO_ENGINES];	/* 0x038 DF_MotorTrust_01..03 */
	CGameParticles engine[FXMOTO_ENGINES];	/* 0x044 */
	CGameParticles down[FXMOTO_DOWN];	/* 0x254 DF_MotorTrust_04, 05 */
	CGameParticles tube[FXMOTO_TUBES];	/* 0x3b4 DF_EscapeTube_01..06 */
	float f7d4;			/* 0x7d4 */
	float f7d8;			/* 0x7d8 */
	byte b7dc;			/* 0x7dc */
	byte pad_7dd[3];
} cEmbeddedFXMoto;
SD_OFFSET(cEmbeddedFXMoto, nodeEngine, 0x38);
SD_OFFSET(cEmbeddedFXMoto, engine, 0x44);
SD_OFFSET(cEmbeddedFXMoto, down, 0x254);
SD_OFFSET(cEmbeddedFXMoto, tube, 0x3b4);
SD_OFFSET(cEmbeddedFXMoto, f7d4, 0x7d4);
SD_SIZE(cEmbeddedFXMoto, 0x7e0);

typedef struct cEmbeddedFXTreasure {
	cEmbeddedFX base;
	byte visible;			/* 0x08 */
	byte pad_09[3];
	int dummyGlow;			/* 0x0c DOB_Treasure_Glow */
	CGameParticles glow;		/* 0x10 */
	int dummyRoot;			/* 0xc0 DC_Root */
	float timer;			/* 0xc4 to the next spark */
	float dur;			/* 0xc8 of the spark */
	float life;			/* 0xcc of the spark */
	vec3_t spark;			/* 0xd0 the position of the spark */
	float rot;			/* 0xdc */
	float rotSpeed;			/* 0xe0 */
	CGameTrail trail;		/* 0xe4 */
	int type;			/* 0xe8 1 A, 2 B, 3 C */
	float scale;			/* 0xec */
} cEmbeddedFXTreasure;
SD_OFFSET(cEmbeddedFXTreasure, rotSpeed, 0xe0);
SD_OFFSET(cEmbeddedFXTreasure, trail, 0xe4);
SD_SIZE(cEmbeddedFXTreasure, 0xf0);

/* the effect of the object model replaces *fx; other models leave *fx as it is */
void EmbeddedFXMisc_Create(cEntity *obj, const char *model, cEmbeddedFX **fx);

#endif
