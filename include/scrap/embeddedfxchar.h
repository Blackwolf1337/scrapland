/*
 * Entities/EmbeddedFX/EmbeddedFXChar.cpp and EmbeddedFXCharHead.cpp of the game: the effects embedded in the
 * character entities (cWithLifeEntity, the pointer at cEntity + 0x224): glows, alarm lights, laser scanners,
 * bubbles and lightning on the particle systems and trails of render nodes of the character model, made by the
 * model name in EmbeddedFXChar_Create; and the hair glow of the head model of the Crazy Gambler.
 *
 * The vtable of cEmbeddedFX: 0 deleting destructor, 1 Reset (after the constructor, returns 1),
 * 2 Release (the particle nodes let go before the delete), 3 Update (each frame, dt), 4 Tick (each frame, only
 * the head has one), 5 Stop (keep: 0 the particles and trails cleared).
 * The constructors and destructors are in EmbeddedFXChar.c, also the base destructor (rva 0xd68f0, EmbeddedFX.h).
 */
#ifndef SCRAP_EMBEDDEDFXCHAR_H
#define SCRAP_EMBEDDEDFXCHAR_H

#include "scrap/entity.h"
#include "scrap/filesys.h"
#include "scrap/objentity.h"
#include "scrap/scrapmodel.h"
#include "scrap/scrapparticles.h"
#include "scrap/scraptrails.h"

typedef struct cEmbeddedFX cEmbeddedFX;

typedef struct cEmbeddedFXVtbl {
	cEmbeddedFX *(*Delete)(cEmbeddedFX *self, byte flags);	/* 0x00 */
	byte(*Reset)(cEmbeddedFX *self);				/* 0x04 */
	void(*Release)(cEmbeddedFX *self);				/* 0x08 */
	void(*Update)(cEmbeddedFX *self, float dt);			/* 0x0c */
	void(*Tick)(cEmbeddedFX *self);				/* 0x10 */
	void(*Stop)(cEmbeddedFX *self, byte keep);			/* 0x14 */
} cEmbeddedFXVtbl;

struct cEmbeddedFX {
	const cEmbeddedFXVtbl *vftable;	/* 0x00 */
	cEntity *ent;			/* 0x04 the character */
};

/* stores the vtable of a class of the family */
static inline void EFX_SetVtbl(void *self, const cEmbeddedFXVtbl *vt) {
	((cEmbeddedFX *)self)->vftable = vt;
}

/* the embedded effect of an entity with life */

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

typedef struct cEmbeddedFXBankDirector {
	cEmbeddedFX base;
	int dummyGlow;			/* 0x08 */
	CGameParticles glow;		/* 0x0c */
	byte on;			/* 0xbc */
	byte wasOn;			/* 0xbd */
	byte pad_be[2];
} cEmbeddedFXBankDirector;

typedef struct cEmbeddedFXBankMaster {
	cEmbeddedFX base;
	byte on;			/* 0x08 */
	byte pad_09[3];
	int dummyGlow;			/* 0x0c */
	CGameParticles glow;		/* 0x10 */
	int dummyBaton;		/* 0xc0 */
	CGameParticles baton;		/* 0xc4 the stick */
} cEmbeddedFXBankMaster;

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
	byte unused_2188;			/* 0x2188 */
	byte pad_2189[3];
} cEmbeddedFXCrazyGambler;

typedef struct cEmbeddedFXGear {
	cEmbeddedFX base;
	cEmbeddedFXAlarm alarm;		/* 0x008 */
	int dummyShoot;			/* 0x0d0 */
	CGameParticles shoot;		/* 0x0d4 */
	byte pad_184[4];
	CGameTrail trail;		/* 0x188 */
} cEmbeddedFXGear;

typedef struct cEmbeddedFXMaintenance {
	cEmbeddedFX base;
	CGameParticles light;		/* 0x08 */
	float t;			/* 0xb8 */
	float lastT;			/* 0xbc */
} cEmbeddedFXMaintenance;

typedef struct cEmbeddedFXMercenary {
	cEmbeddedFX base;
	byte visible;			/* 0x008 */
	byte pad_09[3];
	float t;			/* 0x00c the bubbles and the smoke every 0.05 s */
	int dummyGlow;			/* 0x010 */
	CGameParticles glow;		/* 0x014 */
	int dummyBubbles;		/* 0x0c4 */
	CGameParticles bubbles;		/* 0x0c8 */
	byte isL;			/* 0x178 model MercenaryL.M3D (the one with the pipe) */
	byte pad_179[3];
	int dummyPipe;			/* 0x17c */
	CGameParticles pipe;		/* 0x180 */
} cEmbeddedFXMercenary;

#define FXMESSENGER_RAYS 8
#define FXEMBMESSENGER_TRAILS 12

typedef struct cEmbeddedFXMessenger {
	cEmbeddedFX base;
	int unused_08;			/* 0x008 */
	byte on;			/* 0x00c the lightning was shown (visible, near the camera, not in Relax) */
	byte pad_0d[3];
	int dummyGlow;			/* 0x010 */
	CGameParticles glow;		/* 0x014 4 glows per bolt, at both ends */
	int nodeRays[FXMESSENGER_RAYS];	/* 0x0c4 DF_Rayos_01.. */
	int order[FXMESSENGER_RAYS];	/* 0x0e4 the pairs of rays a bolt joins */
	float life[FXEMBMESSENGER_TRAILS];	/* 0x104 of the bolts */
	int rays[FXEMBMESSENGER_TRAILS][2];	/* 0x134 the two rays of a bolt */
	int verts[FXEMBMESSENGER_TRAILS][2];	/* 0x194 the vertex on each ray */
	CGameTrail trails[FXEMBMESSENGER_TRAILS];	/* 0x1f4 */
	vec3_t offs[FXEMBMESSENGER_TRAILS];	/* 0x224 the bend of the middle of a bolt */
	byte used[FXMESSENGER_RAYS][16];	/* 0x2b4 the vertices of the rays a bolt ends at */
	byte unused_334;			/* 0x334 */
	byte pad_335[3];
} cEmbeddedFXMessenger;

typedef struct cEmbeddedFXPolice {
	cEmbeddedFX base;
	cEmbeddedFXAlarm alarm;		/* 0x08 */
} cEmbeddedFXPolice;

#define FXRUSTY_TRAILS 4

typedef struct cEmbeddedFXRusty {
	cEmbeddedFX base;
	byte visible;			/* 0x008 */
	byte pad_09[3];
	int dummyFront;			/* 0x00c */
	CGameParticles front;		/* 0x010 DR_Mecanico_PechoGlowA */
	int dummyBack;			/* 0x0c0 */
	CGameParticles back;		/* 0x0c4 DR_Mecanico_PechoGlowB */
	int dummySparks;		/* 0x174 */
	CGameParticles sparks;		/* 0x178 DR_Mecanico_ColetaChispas */
	float sparkT;			/* 0x228 the time to the next burst of sparks */
	int dummyLighting[3];		/* 0x22c DF_LightingA, C, B */
	int dummyGlowLighting;		/* 0x238 */
	CGameParticles glowLighting;	/* 0x23c */
	float life[FXRUSTY_TRAILS];	/* 0x2ec of the lightning bolts */
	float pause[FXRUSTY_TRAILS];	/* 0x2fc before the next bolt (never set by Reset) */
	CGameTrail trails[FXRUSTY_TRAILS];	/* 0x30c */
} cEmbeddedFXRusty;

#define FXSENTINEL_LASERS 4

typedef struct cEmbeddedFXSentinel {
	cEmbeddedFX base;
	int dummyStatus;		/* 0x008 */
	CGameParticles status;		/* 0x00c DF_StatusLight */
	int dummyLaser;			/* 0x0bc */
	CGameParticles laser;		/* 0x0c0 DF_LaserScaner */
	float unused_170;			/* 0x170 */
	CGameTrail beams[FXSENTINEL_LASERS];	/* 0x174 */
	float alpha;			/* 0x184 of the lasers, fades to 0 */
	CGameTrail edgesA[FXSENTINEL_LASERS];	/* 0x188 */
	CGameTrail edgesB[FXSENTINEL_LASERS];	/* 0x198 */
	int numLasers;			/* 0x1a8 */
	vec3_t phase[FXSENTINEL_LASERS];	/* 0x1ac x, y: the cosine phases of the sweep */
	float edgeT[FXSENTINEL_LASERS];	/* 0x1dc the edge trails take a point every 0.016 s */
	int lastState;			/* 0x1ec state of the last Update */
	int state;			/* 0x1f0 the cone status of the arrow (off, relax, alert, danger) */
	cEmbeddedFXAlarm alarm;		/* 0x1f4 */
} cEmbeddedFXSentinel;

/* EmbeddedFXCharHead.cpp: on the head model, +4 is the model of the character */
typedef struct cEmbeddedFXHeadCrazyGambler {
	const cEmbeddedFXVtbl *vftable;	/* 0x00 */
	CGameModel *owner;		/* 0x04 */
	CGameParticles particles;	/* 0x08 */
	int nodeHead;			/* 0xb8 R_Gambler_Cabeza-Pelos of the owner */
	float t;			/* 0xbc */
	byte unused_c0;			/* 0xc0 */
	byte pad_c1[3];
} cEmbeddedFXHeadCrazyGambler;

/* the constructors and destructors (EmbeddedFXChar.c) */
void cEmbeddedFX_dtor(cEmbeddedFX *self);
byte cEmbeddedFX_ResetTrue(cEmbeddedFX *self);
void cEmbeddedFX_StopNop(cEmbeddedFX *self, byte keep);
void cEmbeddedFX_TickNop(cEmbeddedFX *self);
void cEmbeddedFX_UpdateNop(cEmbeddedFX *self, float dt);

/* EmbeddedFXChar.c */
void EmbeddedFXChar_Create(cEntity *withLife, const char *model, cEmbeddedFX **fx);
/* the hair glow on the node of the head model (Entities/Char/Type/CrazyGambler.cpp moves it to another head) */
void cEmbeddedFXCrazyGambler_SetHead(cEmbeddedFXCrazyGambler *self, CGameModel *model, const char *node);

/* EmbeddedFXCharHead.c: the effect of a head model or NULL */
cEmbeddedFX *EmbeddedFXCharHead_Create(CGameModel *head);

#endif
