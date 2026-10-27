/*
 * Entities/EmbeddedFX/EmbeddedFXChar.cpp and EmbeddedFXCharHead.cpp of the game: the effects embedded in the
 * character entities (cWithLifeEntity, the pointer at cEntity + 0x224): glows, alarm lights, laser scanners,
 * bubbles and lightning on the particle systems and trails of render nodes of the character model, made by the
 * model name in EmbeddedFXChar_Create; and the hair glow of the head model of the Crazy Gambler.
 *
 * The vtable of cEmbeddedFX (rva 0x3b08a0): 0 deleting destructor, 1 Reset (after the constructor, returns 1),
 * 2 Release (the particle nodes let go before the delete), 3 Update (each frame, dt), 4 Tick (each frame, only
 * the head has one), 5 Stop (keep: 0 the particles and trails cleared).
 * The constructors and destructors are in EmbeddedFXChar.c, also the base destructor (rva 0xd68f0, EmbeddedFX.h).
 */
#ifndef SCRAP_EMBEDDEDFXCHAR_H
#define SCRAP_EMBEDDEDFXCHAR_H

#include "scrap/entity.h"
#include "scrap/filesys.h"
#include "scrap/scrapmodel.h"
#include "scrap/scrapparticles.h"
#include "scrap/scraptrails.h"

typedef struct cEmbeddedFX cEmbeddedFX;

typedef struct cEmbeddedFXVtbl {
	cEmbeddedFX *(__thiscall *Delete)(cEmbeddedFX *self, byte flags);	/* 0x00 */
	byte(__thiscall *Reset)(cEmbeddedFX *self);				/* 0x04 */
	void(__thiscall *Release)(cEmbeddedFX *self);				/* 0x08 */
	void(__thiscall *Update)(cEmbeddedFX *self, float dt);			/* 0x0c */
	void(__thiscall *Tick)(cEmbeddedFX *self);				/* 0x10 */
	void(__thiscall *Stop)(cEmbeddedFX *self, byte keep);			/* 0x14 */
} cEmbeddedFXVtbl;
SD_OFFSET(cEmbeddedFXVtbl, Stop, 0x14);

struct cEmbeddedFX {
	const cEmbeddedFXVtbl *vftable;	/* 0x00 */
	cEntity *ent;			/* 0x04 the character */
};
SD_SIZE(cEmbeddedFX, 8);

/* the embedded effect of an entity with life */
#define Entity_EmbeddedFX(e) (*(cEmbeddedFX **)((byte *)(e) + 0x224))

/* the blinking alarm light of the police, the gears and the sentinels (no vtable, a member) */
typedef struct cEmbeddedFXAlarm {
	int dummy;			/* 0x00 render node DF_PoliceAlarm */
	CGameParticles particles;	/* 0x04 */
	byte on;			/* 0xb4 wanted */
	byte cur;			/* 0xb5 the state level fades to */
	byte pad_b6[2];
	float level;			/* 0xb8 0..1 */
	float t;			/* 0xbc time in the blink period */
	float lastT;			/* 0xc0 */
	byte lights;			/* 0xc4 the rotating lights shown */
	byte pad_c5[3];
} cEmbeddedFXAlarm;
SD_OFFSET(cEmbeddedFXAlarm, on, 0xb4);
SD_OFFSET(cEmbeddedFXAlarm, lights, 0xc4);
SD_SIZE(cEmbeddedFXAlarm, 0xc8);

typedef struct cEmbeddedFXBankDirector {
	cEmbeddedFX base;
	int dummyGlow;			/* 0x08 */
	CGameParticles glow;		/* 0x0c */
	byte on;			/* 0xbc */
	byte wasOn;			/* 0xbd */
	byte pad_be[2];
} cEmbeddedFXBankDirector;
SD_OFFSET(cEmbeddedFXBankDirector, on, 0xbc);
SD_SIZE(cEmbeddedFXBankDirector, 0xc0);

typedef struct cEmbeddedFXBankMaster {
	cEmbeddedFX base;
	byte on;			/* 0x08 */
	byte pad_09[3];
	int dummyGlow;			/* 0x0c */
	CGameParticles glow;		/* 0x10 */
	int dummyBaston;		/* 0xc0 */
	CGameParticles baston;		/* 0xc4 the stick */
} cEmbeddedFXBankMaster;
SD_OFFSET(cEmbeddedFXBankMaster, baston, 0xc4);
SD_SIZE(cEmbeddedFXBankMaster, 0x174);

#define FXGAMBLER_ARCS 8
#define FXGAMBLER_POINTS 0x37		/* the ring of points of an arc (60 slots) */
#define FXGAMBLER_SLOTS 60

typedef struct cEmbeddedFXCrazyGambler {
	cEmbeddedFX base;
	CGameModel *model;		/* 0x0008 the model of the head node */
	CGameParticles particles;	/* 0x000c */
	byte visible;			/* 0x00bc */
	byte pad_bd[3];
	int nodeHead;			/* 0x00c0 */
	int nodeHand;			/* 0x00c4 */
	float t;			/* 0x00c8 */
	float arcT;			/* 0x00cc */
	byte pad_d0[8];
	float life[FXGAMBLER_ARCS];	/* 0x00d8 */
	vec3_t points[FXGAMBLER_ARCS][FXGAMBLER_SLOTS];	/* 0x00f8 */
	int vertex[FXGAMBLER_ARCS][FXGAMBLER_SLOTS];	/* 0x1778 */
	int head[FXGAMBLER_ARCS];	/* 0x1ef8 */
	int tail[FXGAMBLER_ARCS];	/* 0x1f18 */
	int count[FXGAMBLER_ARCS];	/* 0x1f38 */
	uint16_t used[FXGAMBLER_ARCS][0x21];	/* 0x1f58 bit set of the vertices in the arc */
	CGameTrail trails[FXGAMBLER_ARCS];	/* 0x2168 */
	byte b2188;			/* 0x2188 */
	byte pad_2189[3];
} cEmbeddedFXCrazyGambler;
SD_OFFSET(cEmbeddedFXCrazyGambler, life, 0xd8);
SD_OFFSET(cEmbeddedFXCrazyGambler, vertex, 0x1778);
SD_OFFSET(cEmbeddedFXCrazyGambler, used, 0x1f58);
SD_OFFSET(cEmbeddedFXCrazyGambler, trails, 0x2168);
SD_SIZE(cEmbeddedFXCrazyGambler, 0x218c);

typedef struct cEmbeddedFXGear {
	cEmbeddedFX base;
	cEmbeddedFXAlarm alarm;		/* 0x008 */
	int dummyShoot;			/* 0x0d0 */
	CGameParticles shoot;		/* 0x0d4 */
	byte pad_184[4];
	CGameTrail trail;		/* 0x188 */
} cEmbeddedFXGear;
SD_OFFSET(cEmbeddedFXGear, shoot, 0xd4);
SD_SIZE(cEmbeddedFXGear, 0x18c);

typedef struct cEmbeddedFXMaintenance {
	cEmbeddedFX base;
	CGameParticles light;		/* 0x08 */
	float t;			/* 0xb8 */
	float lastT;			/* 0xbc */
} cEmbeddedFXMaintenance;
SD_SIZE(cEmbeddedFXMaintenance, 0xc0);

typedef struct cEmbeddedFXMercenary {
	cEmbeddedFX base;
	byte b08;			/* 0x008 */
	byte pad_09[3];
	int i0c;			/* 0x00c */
	int dummyGlow;			/* 0x010 */
	CGameParticles glow;		/* 0x014 */
	int dummyBubbles;		/* 0x0c4 */
	CGameParticles bubbles;		/* 0x0c8 */
	byte isL;			/* 0x178 model MercenaryL.M3D (the one with the pipe) */
	byte pad_179[3];
	int dummyPipe;			/* 0x17c */
	CGameParticles pipe;		/* 0x180 */
} cEmbeddedFXMercenary;
SD_OFFSET(cEmbeddedFXMercenary, isL, 0x178);
SD_SIZE(cEmbeddedFXMercenary, 0x230);

#define FXMESSENGER_RAYS 8
#define FXMESSENGER_TRAILS 12

typedef struct cEmbeddedFXMessenger {
	cEmbeddedFX base;
	int i08;			/* 0x008 */
	byte b0c;			/* 0x00c */
	byte pad_0d[3];
	int dummyGlow;			/* 0x010 */
	CGameParticles glow;		/* 0x014 */
	int nodeRays[FXMESSENGER_RAYS];	/* 0x0c4 DF_Rayos_01.. */
	int order[FXMESSENGER_RAYS];	/* 0x0e4 */
	int trailState[FXMESSENGER_TRAILS];	/* 0x104 */
	byte pad_134[0x1f4 - 0x134];
	CGameTrail trails[FXMESSENGER_TRAILS];	/* 0x1f4 */
	byte pad_224[0x2b4 - 0x224];
	uint32_t u2b4[0x20];		/* 0x2b4 */
	byte b334;			/* 0x334 */
	byte pad_335[3];
} cEmbeddedFXMessenger;
SD_OFFSET(cEmbeddedFXMessenger, trailState, 0x104);
SD_OFFSET(cEmbeddedFXMessenger, trails, 0x1f4);
SD_OFFSET(cEmbeddedFXMessenger, u2b4, 0x2b4);
SD_SIZE(cEmbeddedFXMessenger, 0x338);

typedef struct cEmbeddedFXPolice {
	cEmbeddedFX base;
	cEmbeddedFXAlarm alarm;		/* 0x08 */
} cEmbeddedFXPolice;
SD_SIZE(cEmbeddedFXPolice, 0xd0);

#define FXRUSTY_TRAILS 4

typedef struct cEmbeddedFXRusty {
	cEmbeddedFX base;
	byte b08;			/* 0x008 */
	byte pad_09[3];
	int dummyFront;			/* 0x00c */
	CGameParticles front;		/* 0x010 DR_Mecanico_PechoGlowA */
	int dummyBack;			/* 0x0c0 */
	CGameParticles back;		/* 0x0c4 DR_Mecanico_PechoGlowB */
	int dummySparks;		/* 0x174 */
	CGameParticles sparks;		/* 0x178 DR_Mecanico_ColetaChispas */
	int i228;			/* 0x228 */
	int dummyLighting[3];		/* 0x22c DF_LightingA, C, B */
	int dummyGlowLighting;		/* 0x238 */
	CGameParticles glowLighting;	/* 0x23c */
	int trailState[FXRUSTY_TRAILS];	/* 0x2ec */
	byte pad_2fc[0x30c - 0x2fc];
	CGameTrail trails[FXRUSTY_TRAILS];	/* 0x30c */
} cEmbeddedFXRusty;
SD_OFFSET(cEmbeddedFXRusty, sparks, 0x178);
SD_OFFSET(cEmbeddedFXRusty, glowLighting, 0x23c);
SD_OFFSET(cEmbeddedFXRusty, trails, 0x30c);
SD_SIZE(cEmbeddedFXRusty, 0x31c);

#define FXSENTINEL_LASERS 4

typedef struct cEmbeddedFXSentinel {
	cEmbeddedFX base;
	int dummyStatus;		/* 0x008 */
	CGameParticles status;		/* 0x00c DF_StatusLight */
	int dummyLaser;			/* 0x0bc */
	CGameParticles laser;		/* 0x0c0 DF_LaserScaner */
	float f170;			/* 0x170 */
	CGameTrail beams[FXSENTINEL_LASERS];	/* 0x174 */
	float f184;			/* 0x184 */
	CGameTrail edgesA[FXSENTINEL_LASERS];	/* 0x188 */
	CGameTrail edgesB[FXSENTINEL_LASERS];	/* 0x198 */
	int numLasers;			/* 0x1a8 */
	vec3_t phase[FXSENTINEL_LASERS];	/* 0x1ac */
	float f1dc[FXSENTINEL_LASERS];	/* 0x1dc */
	int i1ec;			/* 0x1ec */
	int i1f0;			/* 0x1f0 */
	cEmbeddedFXAlarm alarm;		/* 0x1f4 */
} cEmbeddedFXSentinel;
SD_OFFSET(cEmbeddedFXSentinel, beams, 0x174);
SD_OFFSET(cEmbeddedFXSentinel, numLasers, 0x1a8);
SD_OFFSET(cEmbeddedFXSentinel, alarm, 0x1f4);
SD_SIZE(cEmbeddedFXSentinel, 0x2bc);

/* EmbeddedFXCharHead.cpp: on the head model, +4 is the model of the character */
typedef struct cEmbeddedFXHeadCrazyGambler {
	const cEmbeddedFXVtbl *vftable;	/* 0x00 */
	CGameModel *owner;		/* 0x04 */
	CGameParticles particles;	/* 0x08 */
	int nodeHead;			/* 0xb8 R_Gambler_Cabeza-Pelos of the owner */
	float t;			/* 0xbc */
	byte bc0;			/* 0xc0 */
	byte pad_c1[3];
} cEmbeddedFXHeadCrazyGambler;
SD_SIZE(cEmbeddedFXHeadCrazyGambler, 0xc4);

/* the constructors and destructors (EmbeddedFXChar.c) */
void __fastcall cEmbeddedFX_dtor(cEmbeddedFX *self);			/* rva 0xd68f0 */
cEmbeddedFX *__thiscall cEmbeddedFX_Delete(cEmbeddedFX *self, byte flags);	/* rva 0xd6900 */
cEmbeddedFX *__fastcall cEmbeddedFXBankDirector_ctor(void *self);	/* rva 0xd6930 */
cEmbeddedFX *__fastcall cEmbeddedFXBankMaster_ctor(void *self);	/* rva 0xd69a0 */
cEmbeddedFX *__fastcall cEmbeddedFXCrazyGambler_ctor(void *self);	/* rva 0xd6a30 */
cEmbeddedFX *__fastcall cEmbeddedFXGear_ctor(void *self);		/* rva 0xd6af0 */
cEmbeddedFX *__fastcall cEmbeddedFXMaintenance_ctor(void *self);	/* rva 0xd6b90 */
cEmbeddedFX *__fastcall cEmbeddedFXMercenary_ctor(void *self);	/* rva 0xd6c10 */
cEmbeddedFX *__fastcall cEmbeddedFXMessenger_ctor(void *self);	/* rva 0xd6cb0 */
cEmbeddedFX *__fastcall cEmbeddedFXPolice_ctor(void *self);		/* rva 0xd6d40 */
cEmbeddedFXAlarm *__fastcall cEmbeddedFXAlarm_ctor(cEmbeddedFXAlarm *self);	/* rva 0xd6dc0 */
cEmbeddedFX *__fastcall cEmbeddedFXRusty_ctor(void *self);		/* rva 0xd6df0 */
cEmbeddedFX *__fastcall cEmbeddedFXSentinel_ctor(void *self);	/* rva 0xd6ed0 */
void __fastcall cEmbeddedFXBankDirector_dtor(cEmbeddedFXBankDirector *self);	/* rva 0xd6ff0 */

/* EmbeddedFXChar.c */
void EmbeddedFXChar_Create(cEntity *withLife, const char *model, cEmbeddedFX **fx);
void EmbeddedFXChar_SetAlarm(const char *name, byte on);
void EmbeddedFXChar_AlarmOff(cAbstractFile *f);
void EmbeddedFXChar_AlarmOn(cAbstractFile *f);
void __fastcall cEmbeddedFXAlarm_dtor(cEmbeddedFXAlarm *self);
void __fastcall cEmbeddedFXAlarm_Release(cEmbeddedFXAlarm *self);
byte __fastcall cEmbeddedFXAlarm_Reset(cEmbeddedFXAlarm *self);
void __thiscall cEmbeddedFXAlarm_SetOn(cEmbeddedFXAlarm *self, byte on);
void __thiscall cEmbeddedFXAlarm_SetLights(cEmbeddedFXAlarm *self, byte on);
void __thiscall cEmbeddedFXAlarm_Init(cEmbeddedFXAlarm *self, CGameModel *model);
void __thiscall cEmbeddedFXAlarm_Update(cEmbeddedFXAlarm *self, float dt);
/* the hair glow on the node of the head model (Entities/Char/Type/CrazyGambler.cpp moves it to another head) */
void __thiscall cEmbeddedFXCrazyGambler_SetHead(cEmbeddedFXCrazyGambler *self, CGameModel *model, const char *node);

/* EmbeddedFXCharHead.c: the effect of a head model or NULL */
cEmbeddedFX *EmbeddedFXCharHead_Create(CGameModel *head);

#endif
