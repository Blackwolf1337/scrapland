/*
 * The effect elements of the levels (World/FX/FXElements*.cpp of the game): map elements (cMapElement, a model of
 * the level in the list g_pWorld->elements) made at the level start from the dummies "DM_Element_*" of the level.
 * Implemented here: FXElements.cpp (fire drops, lens flares, lightnings), FXElementsAB.cpp (the fake limit and the
 * meteors of the level AB), FXElementsOrbit.cpp (the orbit buoys and their controller) and FXElementsPress.cpp (the
 * mental controllers of the Press building).
 */
#ifndef SCRAP_FXELEMENTS_H
#define SCRAP_FXELEMENTS_H

#include "scrap/gvar.h"
#include "scrap/crt.h"
#include "scrap/scrapparticles.h"
#include "scrap/scraptrails.h"

typedef struct cMapElement cMapElement;

/* cMapElement::vftable and the ones of the elements */
typedef struct cMapElementVtbl {
	void (*Free)(cMapElement *self);
	byte(*Init)(cMapElement *self);			/* after the creation, 1 */
	void(*Update)(cMapElement *self, float dt);		/* every frame */
} cMapElementVtbl;

/* calls through the slots of cMapElementVtbl */
static inline void MapElem_Free(void *self) {
	VT(cMapElementVtbl, self)->Free(self);
}

static inline byte MapElem_Init(void *self) {
	return VT(cMapElementVtbl, self)->Init(self);
}

static inline void MapElem_Update(void *self, float dt) {
	VT(cMapElementVtbl, self)->Update(self, dt);
}

/* World/MapElement: an element of the level with its model */
struct cMapElement {
	const cMapElementVtbl *vftable;
	CGameModel Mod;
	int anim;			/* the animation of the model */
	char *name;			/* cString */
	cMapElement *next;		/* g_pWorld->elements */
};

#define World_Elements(w) ((w)->elements)

/* a new element initialized and put first into the list of the world */
static inline void MapElement_Add(cMapElement *e) {
	MapElem_Init(e);
	e->next = World_Elements(g_pWorld);
	World_Elements(g_pWorld) = e;
}

/* the model of the element name at pos and rot (radians) */
cMapElement *cMapElement_ctor(cMapElement *self, const char *name, const vec3_t *pos, const vec3_t *rot);
void cMapElement_dtor(cMapElement *self);
/* (1), 0x203520 (the animation of the visible model) */
byte cMapElement_Init(cMapElement *self);
void cMapElement_Update(cMapElement *self, float dt);
byte cMapElement_InitTrue(cMapElement *self);
void cMapElement_UpdateNop(cMapElement *self, float dt);

/* ---- FXElementsOrbit.cpp ---- */

#define FXORBITBOYACOLUMNS 4
#define FXORBITBOYACOLUMNCOUNT 8

/* a buoy: a light particle over its model */
typedef struct cFXElementsOrbitBuoy {
	cMapElement base;
	int dcRoot;			/* the node DC_Root */
	vec3_t pos;
	byte pad_3c[4];
	CGameParticles light;		/* DF_Light */
} cFXElementsOrbitBuoy;

/* the buoys by column and number, their lights blink in turns (constructor) */
typedef struct cFXElementsOrbitBuoyController {
	cMapElement base;
	cFXElementsOrbitBuoy *buoy[FXORBITBOYACOLUMNS][FXORBITBOYACOLUMNCOUNT];
	int type[FXORBITBOYACOLUMNS][FXORBITBOYACOLUMNCOUNT];			/* the look of the light, 0 or 1 */
	float fade[FXORBITBOYACOLUMNS][FXORBITBOYACOLUMNCOUNT];			/* 1 down to 0, again */
} cFXElementsOrbitBuoyController;

/* the controller and the buoys of the dummies DM_Element_OrbitBoya* of the level */
void FXElementsOrbit_Create(void);

/* ---- FXElementsPress.cpp ---- */

#define FXMENTALCONTROLLERMAINCOUNT 9
#define FXMENTALCONTROLLERLEVELS 8
#define FXMENTALDAMAGEEMITTERS 4
#define FXMENTALMAINTRAILS 15
#define FXMENTALMAINFLUID 225
#define FXMENTALMAINGLOWS 30

/* a mental controller, near while the camera is within FXElementsMentalControllerMaxDist */
typedef struct cFXElementsMentalController {
	cMapElement base;
	int dcRoot;			/* the node DC_Root */
	vec3_t pos;
	byte nearCam;
	byte pad_3d[3];
} cFXElementsMentalController;

/* the damaged controller of the epilogue: smoke and flashes from emitter nodes */
typedef struct cFXElementsMentalControllerDamageFX {
	cFXElementsMentalController base;
	CGameParticles smoke;		/* DF_Smoke */
	float smokeTime;		/* since the last smoke */
	int smokeNode[FXMENTALDAMAGEEMITTERS];		/* DF_SmokeEmitter_nn, -1 */
	vec3_t smokePos[FXMENTALDAMAGEEMITTERS];
	vec3_t smokeRot[FXMENTALDAMAGEEMITTERS];
	vec3_t smokeVelMin[FXMENTALDAMAGEEMITTERS];
	vec3_t smokeVelMax[FXMENTALDAMAGEEMITTERS];
	vec3_t smokeAccMin[FXMENTALDAMAGEEMITTERS];
	vec3_t smokeAccMax[FXMENTALDAMAGEEMITTERS];
	float smokeSizeIni[FXMENTALDAMAGEEMITTERS];
	float smokeSizeEnd[FXMENTALDAMAGEEMITTERS];
	float smokeLifeMin[FXMENTALDAMAGEEMITTERS];
	float smokeLifeMax[FXMENTALDAMAGEEMITTERS];
	CGameParticles flash;		/* DF_Flash */
	float flashTime;		/* since the last flash step */
	int flashNode[FXMENTALDAMAGEEMITTERS];		/* DF_FlashEmitter_nn, -1 */
	vec3_t flashPos[FXMENTALDAMAGEEMITTERS];
	vec3_t flashRot[FXMENTALDAMAGEEMITTERS];
	float flashSizeMin[FXMENTALDAMAGEEMITTERS];
	float flashSizeMax[FXMENTALDAMAGEEMITTERS];
	float flashSleepMin[FXMENTALDAMAGEEMITTERS];
	float flashSleepMax[FXMENTALDAMAGEEMITTERS];
	float flashSleep[FXMENTALDAMAGEEMITTERS];
	int flashSparksMin[FXMENTALDAMAGEEMITTERS];
	int flashSparksMax[FXMENTALDAMAGEEMITTERS];
} cFXElementsMentalControllerDamageFX;

/* the main controller: the data fluid in the tubes to the controllers */
typedef struct cFXElementsMentalControllerMainFX {
	cMapElement base;
	int dcRoot;
	vec3_t pos;
	byte nearCam;
	byte pad_3d[3];
	int phase;			/* 1..5, the act of the story */
	vec3_t top;			/* pos raised by 350 */
	cFXElementsMentalController *controllers[FXMENTALCONTROLLERMAINCOUNT];
	float levels[FXMENTALCONTROLLERLEVELS];
	/* the drops of data that run from one controller to the next (or up the tube) */
	vec3_t fluidPos[FXMENTALMAINFLUID];	/* the start */
	vec3_t fluidEnd[FXMENTALMAINFLUID];
	vec3_t fluidDir[FXMENTALMAINFLUID];	/* end - start */
	vec3_t fluidNormal[FXMENTALMAINFLUID];	/* the unit of fluidDir */
	float fluidPhase[FXMENTALMAINFLUID];	/* 0..1 along the way, over 1 a new drop */
	float fluidTime[FXMENTALMAINFLUID];	/* the seconds of the way */
	float fluidWidth[FXMENTALMAINFLUID];
	int fluidTube[FXMENTALMAINFLUID];
	uint32_t fluidColor[FXMENTALMAINFLUID];
	CGameTrail tubeTrails[FXMENTALMAINTRAILS];
	CGameParticles dataFluid;		/* DF_DataFluid */
	CGameTrail glowTrails[FXMENTALMAINTRAILS];
	byte fluidActive[FXMENTALMAINFLUID];
	byte pad_3e61[3];
	CGameParticles tubeGlow;		/* DF_TubeGlow */
	float glowTime;				/* the glow of the tube left */
	float glowDuration;
	float glowPause;
	float glowPhase[FXMENTALMAINGLOWS];
	float glowSpeed[FXMENTALMAINGLOWS];
	vec3_t glowOffset[FXMENTALMAINGLOWS];
	CGameTrail topTrails[2];
	int glowSide[FXMENTALMAINGLOWS];
	uint32_t glowColor[FXMENTALMAINGLOWS];
	float scale;
	int last;
} cFXElementsMentalControllerMainFX;

/* the main controller of the level, NULL before the Press elements are made */
extern cFXElementsMentalControllerMainFX *g_MentalMainFX;

/* the main controller and the controllers of the dummies of the level, by the act of the story (SaveVar GameAct) */
void FXElementsPress_Create(void);

/* rand() of the exe in [0, 1] times range, plus add (all in double), the way the effects pick their values */
static inline float FxElem_Rand(double range, double add) {
	return (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * range + add);
}

/* ---- FXElements.cpp ---- */

/* drops of fire from the node DC_Root while the camera is within FXElementsFireDropMaxDist: bursts with pauses,
 * the parameters from the section FXFireDrop of the node (constructor) */
typedef struct cFXElementsFireDrop {
	cMapElement base;
	int dcRoot;
	vec3_t pos;
	CGameParticles drops;		/* the particles of DC_Root */
	byte nearCam;
	byte pad_ed[3];
	float burstTime;		/* left of the burst, the pause below 0 */
	float sleepTime;		/* the pause to the next burst */
	float emitTime[3];		/* the time since the last drops of the three kinds */
	float scale;			/* Scale */
	float size;			/* Size */
	float speed;			/* Speed */
	float area;			/* Area */
	float life;			/* Life */
} cFXElementsFireDrop;

/* a lens flare (its CGameLensFlare at 0x2c) */
typedef struct cFXElementsLensFlare {
	cMapElement base;
	byte flare[4];			/* CGameLensFlare */
} cFXElementsLensFlare;

#define FXLIGHTINGBOLTS 10
#define FXLIGHTINGLINKS 4
#define FXLIGHTINGPOINTS 10

/* lightnings between the nodes DF_Link_<bolt><A..D> of the model: each bolt jumps between two of its links, a
 * trail with glow particles at both ends; the parameters from the section Lighting of the first link (0xe88 bytes,
 * constructor) */
typedef struct cFXElementsLighting {
	cMapElement base;
	int dcRoot;
	vec3_t pos;
	CGameParticles glows;				/* 4 per bolt: 2 at each end */
	byte visible;
	byte pad_ed[3];
	float animDur;					/* animation 0 */
	float animFps;
	float prevTime;					/* of channel 0 */
	float time;
	int link[FXLIGHTINGBOLTS][FXLIGHTINGLINKS];	/* render nodes, -1 after the last */
	int linkCount[FXLIGHTINGBOLTS];
	int pair[FXLIGHTINGBOLTS][2];			/* the links of the bolt now */
	byte glow[FXLIGHTINGBOLTS][FXLIGHTINGLINKS];	/* Glow: particles at the link */
	uint32_t glowColor[FXLIGHTINGBOLTS][FXLIGHTINGLINKS];
	float glowSizeMin[FXLIGHTINGBOLTS][FXLIGHTINGLINKS];
	float glowSizeMax[FXLIGHTINGBOLTS][FXLIGHTINGLINKS];
	uint32_t pointColor[FXLIGHTINGBOLTS][FXLIGHTINGLINKS];	/* the trail colour at the link */
	float pointSizeMin[FXLIGHTINGBOLTS][FXLIGHTINGLINKS];
	float pointSizeMax[FXLIGHTINGBOLTS][FXLIGHTINGLINKS];
	float sizeMin[FXLIGHTINGBOLTS];
	float sizeMax[FXLIGHTINGBOLTS];
	float sizeVar[FXLIGHTINGBOLTS];
	float lifeTimeMin[FXLIGHTINGBOLTS];
	float lifeTimeMax[FXLIGHTINGBOLTS];
	float sleepTimeMin[FXLIGHTINGBOLTS];
	float sleepTimeMax[FXLIGHTINGBOLTS];
	float tensorMin[FXLIGHTINGBOLTS];
	float tensorMax[FXLIGHTINGBOLTS];
	float randVarMin[FXLIGHTINGBOLTS];
	float randVarMax[FXLIGHTINGBOLTS];
	float anmFollow[FXLIGHTINGBOLTS];		/* picked between the two below */
	float anmFollowMin[FXLIGHTINGBOLTS];
	float anmFollowMax[FXLIGHTINGBOLTS];
	byte anmLoopReset[FXLIGHTINGBOLTS];
	byte saturate[FXLIGHTINGBOLTS];			/* Saturate4X */
	float offsetSpeed[FXLIGHTINGBOLTS];
	float life[FXLIGHTINGBOLTS];
	float sleep[FXLIGHTINGBOLTS];
	float size[FXLIGHTINGBOLTS];
	float pointSize[FXLIGHTINGBOLTS][2];
	float offset[FXLIGHTINGBOLTS];			/* the u of the trail texture */
	float glowSize[FXLIGHTINGBOLTS][2];
	CGameTrail trails[FXLIGHTINGBOLTS];
	vec3_t pts[FXLIGHTINGBOLTS][FXLIGHTINGPOINTS];	/* the control points of the bolt */
	int numPts;					/* 6 */
} cFXElementsLighting;

cFXElementsFireDrop *cFXElementsFireDrop_ctor(cFXElementsFireDrop *self, const char *name,
	const vec3_t *pos, const vec3_t *rot);
/* the flare at pos: intensity 0.65, 0x30 0.3, on (Elements_Create passes no name) */
cFXElementsLensFlare *cFXElementsLensFlare_ctor(cFXElementsLensFlare *self, const char *name,
	const vec3_t *pos, const vec3_t *rot);
cFXElementsLighting *cFXElementsLighting_ctor(cFXElementsLighting *self, const char *name,
	const vec3_t *pos, const vec3_t *rot);

/* ---- FXElementsAB.cpp ---- */

#define FXABBOLTS 10
#define FXABRINGS 4
#define FXABRINGTRAILS 14

/* the fake limit of the level AB around the dummy DM_Mission_Centre: rings of trails and lightnings that grow when
 * the ship of the player comes near (constructor) */
typedef struct cFXElementsABFakeLimit {
	cMapElement base;
	vec3_t centre;					/* DM_Mission_Centre */
	CGameParticles lighting;			/* Dummy01, 2 per bolt */
	float boltTime[FXABBOLTS];
	CGameTrail bolts[FXABBOLTS];
	float boltSize[FXABBOLTS];
	vec3_t boltStart[FXABBOLTS];			/* in the space of the model */
	vec3_t boltCtrl[FXABBOLTS][2];			/* the moving middle points */
	CGameParticles nearParts;			/* Dummy02 */
	float scroll[FXABRINGS];
	vec3_t ringOfs[FXABRINGS];
	CGameTrail rings[FXABRINGS][FXABRINGTRAILS];
} cFXElementsABFakeLimit;

#define FXABMETEORS 500
#define FXABMETEORSPLIT 250	/* the meteors after this one are in the second particle system */

/* meteors along a curve over the level AB (constructor) */
typedef struct cFXElementsABFakeMeteors {
	cMapElement base;
	float t[FXABMETEORS];			/* on the curve, 0..1 */
	float speed[FXABMETEORS];
	vec3_t ofs[FXABMETEORS];
	float size[FXABMETEORS];
	int sprite[FXABMETEORS];
	float rot[FXABMETEORS];
	float rotSpeed[FXABMETEORS];
	float fade[FXABMETEORS];
	float fadeSpeed[FXABMETEORS];
	CGameParticles parts[2];		/* Dummy01, Dummy02 */
	CGameTrail curve;			/* only its Bezier helpers are used */
} cFXElementsABFakeMeteors;

/* the meteors and the limit of the dummies DM_Element_ABFakeMeteors and DM_Element_ABFakeLimit of the level */
void FXElementsAB_Create(void);

#endif
