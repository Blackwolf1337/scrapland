/*
 * Entities/EmbeddedFX/EmbeddedFXChar.cpp and EmbeddedFXCharHead.cpp of the game: the effects embedded in the
 * character entities (cWithLifeEntity, the pointer at cEntity + 0x224): glows, alarm lights, laser scanners,
 * bubbles and lightning on the particle systems and trails of render nodes of the character model, made by the
 * model name in EmbeddedFXChar_Create; and the hair glow of the head model of the Crazy Gambler.
 *
 * The vtable of cEmbeddedFX: 0 Free, 1 Reset (after the constructor, returns 1),
 * 2 Release (the particle nodes let go before the delete), 3 Update (each frame, dt), 4 Tick (each frame, only
 * the head has one), 5 Stop (keep: 0 the particles and trails cleared).
 * The constructors and destructors are in EmbeddedFXChar.c, also the base destructor (EmbeddedFX.h).
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
	void (*Free)(cEmbeddedFX *self);
	byte(*Reset)(cEmbeddedFX *self);
	void(*Release)(cEmbeddedFX *self);
	void(*Update)(cEmbeddedFX *self, float dt);
	void(*Tick)(cEmbeddedFX *self);
	void(*Stop)(cEmbeddedFX *self, byte keep);
} cEmbeddedFXVtbl;

/* calls through the slots of cEmbeddedFXVtbl */
static inline void EFX_Free(void *self) {
	VT(cEmbeddedFXVtbl, self)->Free(self);
}

static inline void EFX_Release(void *self) {
	VT(cEmbeddedFXVtbl, self)->Release(self);
}

static inline byte EFX_Reset(void *self) {
	return VT(cEmbeddedFXVtbl, self)->Reset(self);
}

static inline void EFX_Stop(void *self, byte keep) {
	VT(cEmbeddedFXVtbl, self)->Stop(self, keep);
}

static inline void EFX_Tick(void *self) {
	VT(cEmbeddedFXVtbl, self)->Tick(self);
}

static inline void EFX_Update(void *self, float dt) {
	VT(cEmbeddedFXVtbl, self)->Update(self, dt);
}

struct cEmbeddedFX {
	const cEmbeddedFXVtbl *vftable;
	cEntity *ent;			/* the character */
};

/* stores the vtable of a class of the family */
static inline void EFX_SetVtbl(void *self, const cEmbeddedFXVtbl *vt) {
	((cEmbeddedFX *)self)->vftable = vt;
}

/* the embedded effect of an entity with life */

/* the blinking alarm light of the police, the gears and the sentinels (no vtable, a member) */
typedef struct cEmbeddedFXAlarm {
	int dummy;			/* render node DF_PoliceAlarm */
	CGameParticles particles;
	byte on;			/* wanted */
	byte cur;			/* the state level fades to */
	byte pad_b6[2];
	float level;			/* 0..1 */
	float t;			/* time in the blink period */
	float lastT;
	byte lights;			/* the rotating lights shown */
	byte pad_c5[3];
} cEmbeddedFXAlarm;

typedef struct cEmbeddedFXBankDirector {
	cEmbeddedFX base;
	int dummyGlow;
	CGameParticles glow;
	byte on;
	byte wasOn;
	byte pad_be[2];
} cEmbeddedFXBankDirector;

typedef struct cEmbeddedFXBankMaster {
	cEmbeddedFX base;
	byte on;
	byte pad_09[3];
	int dummyGlow;
	CGameParticles glow;
	int dummyBaton;
	CGameParticles baton;		/* the stick */
} cEmbeddedFXBankMaster;

#define FXGAMBLER_ARCS 8
#define FXGAMBLER_POINTS 0x37		/* the ring of points of an arc (60 slots) */
#define FXGAMBLER_SLOTS 60

typedef struct cEmbeddedFXCrazyGambler {
	cEmbeddedFX base;
	CGameModel *model;		/* the model of the head node */
	CGameParticles particles;
	byte visible;
	byte pad_bd[3];
	int nodeHead;
	int nodeHand;
	float t;
	float arcT;
	byte pad_d0[8];
	float life[FXGAMBLER_ARCS];
	vec3_t points[FXGAMBLER_ARCS][FXGAMBLER_SLOTS];
	int vertex[FXGAMBLER_ARCS][FXGAMBLER_SLOTS];
	int head[FXGAMBLER_ARCS];
	int tail[FXGAMBLER_ARCS];
	int count[FXGAMBLER_ARCS];
	uint16_t used[FXGAMBLER_ARCS][0x21];	/* bit set of the vertices in the arc */
	CGameTrail trails[FXGAMBLER_ARCS];
	byte unused_2188;
	byte pad_2189[3];
} cEmbeddedFXCrazyGambler;

typedef struct cEmbeddedFXGear {
	cEmbeddedFX base;
	cEmbeddedFXAlarm alarm;
	int dummyShoot;
	CGameParticles shoot;
	byte pad_184[4];
	CGameTrail trail;
} cEmbeddedFXGear;

typedef struct cEmbeddedFXMaintenance {
	cEmbeddedFX base;
	CGameParticles light;
	float t;
	float lastT;
} cEmbeddedFXMaintenance;

typedef struct cEmbeddedFXMercenary {
	cEmbeddedFX base;
	byte visible;
	byte pad_09[3];
	float t;			/* the bubbles and the smoke every 0.05 s */
	int dummyGlow;
	CGameParticles glow;
	int dummyBubbles;
	CGameParticles bubbles;
	byte isL;			/* model MercenaryL.M3D (the one with the pipe) */
	byte pad_179[3];
	int dummyPipe;
	CGameParticles pipe;
} cEmbeddedFXMercenary;

#define FXMESSENGER_RAYS 8
#define FXEMBMESSENGER_TRAILS 12

typedef struct cEmbeddedFXMessenger {
	cEmbeddedFX base;
	int unused_08;
	byte on;			/* the lightning was shown (visible, near the camera, not in Relax) */
	byte pad_0d[3];
	int dummyGlow;
	CGameParticles glow;		/* 4 glows per bolt, at both ends */
	int nodeRays[FXMESSENGER_RAYS];	/* DF_Rayos_01.. */
	int order[FXMESSENGER_RAYS];	/* the pairs of rays a bolt joins */
	float life[FXEMBMESSENGER_TRAILS];	/* of the bolts */
	int rays[FXEMBMESSENGER_TRAILS][2];	/* the two rays of a bolt */
	int verts[FXEMBMESSENGER_TRAILS][2];	/* the vertex on each ray */
	CGameTrail trails[FXEMBMESSENGER_TRAILS];
	vec3_t offs[FXEMBMESSENGER_TRAILS];	/* the bend of the middle of a bolt */
	byte used[FXMESSENGER_RAYS][16];	/* the vertices of the rays a bolt ends at */
	byte unused_334;
	byte pad_335[3];
} cEmbeddedFXMessenger;

typedef struct cEmbeddedFXPolice {
	cEmbeddedFX base;
	cEmbeddedFXAlarm alarm;
} cEmbeddedFXPolice;

#define FXRUSTY_TRAILS 4

typedef struct cEmbeddedFXRusty {
	cEmbeddedFX base;
	byte visible;
	byte pad_09[3];
	int dummyFront;
	CGameParticles front;		/* DR_Mecanico_PechoGlowA */
	int dummyBack;
	CGameParticles back;		/* DR_Mecanico_PechoGlowB */
	int dummySparks;
	CGameParticles sparks;		/* DR_Mecanico_ColetaChispas */
	float sparkT;			/* the time to the next burst of sparks */
	int dummyLighting[3];		/* DF_LightingA, C, B */
	int dummyGlowLighting;
	CGameParticles glowLighting;
	float life[FXRUSTY_TRAILS];	/* of the lightning bolts */
	float pause[FXRUSTY_TRAILS];	/* before the next bolt (set by the constructor only, not by Reset) */
	CGameTrail trails[FXRUSTY_TRAILS];
} cEmbeddedFXRusty;

#define FXSENTINEL_LASERS 4

typedef struct cEmbeddedFXSentinel {
	cEmbeddedFX base;
	int dummyStatus;
	CGameParticles status;		/* DF_StatusLight */
	int dummyLaser;
	CGameParticles laser;		/* DF_LaserScaner */
	float unused_170;
	CGameTrail beams[FXSENTINEL_LASERS];
	float alpha;			/* of the lasers, fades to 0 */
	CGameTrail edgesA[FXSENTINEL_LASERS];
	CGameTrail edgesB[FXSENTINEL_LASERS];
	int numLasers;
	vec3_t phase[FXSENTINEL_LASERS];	/* x, y: the cosine phases of the sweep */
	float edgeT[FXSENTINEL_LASERS];	/* the edge trails take a point every 0.016 s */
	int lastState;			/* state of the last Update */
	int state;			/* the cone status of the arrow (off, relax, alert, danger) */
	cEmbeddedFXAlarm alarm;
} cEmbeddedFXSentinel;

/* EmbeddedFXCharHead.cpp: on the head model, +4 is the model of the character */
typedef struct cEmbeddedFXHeadCrazyGambler {
	const cEmbeddedFXVtbl *vftable;
	CGameModel *owner;
	CGameParticles particles;
	int nodeHead;			/* R_Gambler_Cabeza-Pelos of the owner */
	float t;
	byte unused_c0;
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
