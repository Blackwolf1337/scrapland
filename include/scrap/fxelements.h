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
	cMapElement *(*Delete)(cMapElement *self, byte flags);	/* 0x0 */
	byte(*Init)(cMapElement *self);			/* 0x4 vf01 after the creation, 1 */
	void(*Update)(cMapElement *self, float dt);		/* 0x8 vf02 every frame */
} cMapElementVtbl;

/* calls through the slots of cMapElementVtbl */
static inline cMapElement *MapElem_Delete(void *self, byte flags) {
	return VT(cMapElementVtbl, self)->Delete(self, flags);
}

static inline byte MapElem_Init(void *self) {
	return VT(cMapElementVtbl, self)->Init(self);
}

static inline void MapElem_Update(void *self, float dt) {
	VT(cMapElementVtbl, self)->Update(self, dt);
}

/* World/MapElement: an element of the level with its model (0x2c bytes) */
struct cMapElement {
	const cMapElementVtbl *vftable;	/* 0x00 */
	CGameModel Mod;			/* 0x04 */
	int anim;			/* 0x20 the animation of the model */
	char *name;			/* 0x24 cString */
	cMapElement *next;		/* 0x28 g_pWorld->elements */
};

#define World_Elements(w) ((w)->elements)

/* a new element initialized and put first into the list of the world */
static inline void MapElement_Add(cMapElement *e) {
	MapElem_Init(e);
	e->next = World_Elements(g_pWorld);
	World_Elements(g_pWorld) = e;
}

/* rva 0x202ca0: the model of the element name at pos and rot (radians), 0x202e20 the destructor */
cMapElement *cMapElement_ctor(cMapElement *self, const char *name, const vec3_t *pos, const vec3_t *rot);
void cMapElement_dtor(cMapElement *self);
/* rva 0x203310 (1), 0x203520 (the animation of the visible model) */
byte cMapElement_Init(cMapElement *self);
void cMapElement_Update(cMapElement *self, float dt);
byte cMapElement_InitTrue(cMapElement *self);
void cMapElement_UpdateNop(cMapElement *self, float dt);

/* ---- FXElementsOrbit.cpp ---- */

#define FXORBITBOYACOLUMNS 4
#define FXORBITBOYACOLUMNCOUNT 8

/* a buoy: a light particle over its model (0xf0 bytes) */
typedef struct cFXElementsOrbitBuoy {
	cMapElement base;
	int dcRoot;			/* 0x2c the node DC_Root */
	vec3_t pos;			/* 0x30 */
	byte pad_3c[4];
	CGameParticles light;		/* 0x40 DF_Light */
} cFXElementsOrbitBuoy;

/* the buoys by column and number, their lights blink in turns (0x1ac bytes, constructor rva 0x1d5820) */
typedef struct cFXElementsOrbitBuoyController {
	cMapElement base;
	cFXElementsOrbitBuoy *buoy[FXORBITBOYACOLUMNS][FXORBITBOYACOLUMNCOUNT];	/* 0x02c */
	int type[FXORBITBOYACOLUMNS][FXORBITBOYACOLUMNCOUNT];			/* 0x0ac the look of the light, 0 or 1 */
	float fade[FXORBITBOYACOLUMNS][FXORBITBOYACOLUMNCOUNT];			/* 0x12c 1 down to 0, again */
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

/* a mental controller, near while the camera is within FXElementsMentalControllerMaxDist (0x40 bytes) */
typedef struct cFXElementsMentalController {
	cMapElement base;
	int dcRoot;			/* 0x2c the node DC_Root */
	vec3_t pos;			/* 0x30 */
	byte nearCam;			/* 0x3c */
	byte pad_3d[3];
} cFXElementsMentalController;

/* the damaged controller of the epilogue: smoke and flashes from emitter nodes (0x3f8 bytes) */
typedef struct cFXElementsMentalControllerDamageFX {
	cFXElementsMentalController base;
	CGameParticles smoke;		/* 0x040 DF_Smoke */
	float smokeTime;		/* 0x0f0 since the last smoke */
	int smokeNode[FXMENTALDAMAGEEMITTERS];		/* 0x0f4 DF_SmokeEmitter_nn, -1 */
	vec3_t smokePos[FXMENTALDAMAGEEMITTERS];	/* 0x104 */
	vec3_t smokeRot[FXMENTALDAMAGEEMITTERS];	/* 0x134 */
	vec3_t smokeVelMin[FXMENTALDAMAGEEMITTERS];	/* 0x164 */
	vec3_t smokeVelMax[FXMENTALDAMAGEEMITTERS];	/* 0x194 */
	vec3_t smokeAccMin[FXMENTALDAMAGEEMITTERS];	/* 0x1c4 */
	vec3_t smokeAccMax[FXMENTALDAMAGEEMITTERS];	/* 0x1f4 */
	float smokeSizeIni[FXMENTALDAMAGEEMITTERS];	/* 0x224 */
	float smokeSizeEnd[FXMENTALDAMAGEEMITTERS];	/* 0x234 */
	float smokeLifeMin[FXMENTALDAMAGEEMITTERS];	/* 0x244 */
	float smokeLifeMax[FXMENTALDAMAGEEMITTERS];	/* 0x254 */
	CGameParticles flash;		/* 0x264 DF_Flash */
	float flashTime;		/* 0x314 since the last flash step */
	int flashNode[FXMENTALDAMAGEEMITTERS];		/* 0x318 DF_FlashEmitter_nn, -1 */
	vec3_t flashPos[FXMENTALDAMAGEEMITTERS];	/* 0x328 */
	vec3_t flashRot[FXMENTALDAMAGEEMITTERS];	/* 0x358 */
	float flashSizeMin[FXMENTALDAMAGEEMITTERS];	/* 0x388 */
	float flashSizeMax[FXMENTALDAMAGEEMITTERS];	/* 0x398 */
	float flashSleepMin[FXMENTALDAMAGEEMITTERS];	/* 0x3a8 */
	float flashSleepMax[FXMENTALDAMAGEEMITTERS];	/* 0x3b8 */
	float flashSleep[FXMENTALDAMAGEEMITTERS];	/* 0x3c8 */
	int flashSparksMin[FXMENTALDAMAGEEMITTERS];	/* 0x3d8 */
	int flashSparksMax[FXMENTALDAMAGEEMITTERS];	/* 0x3e8 */
} cFXElementsMentalControllerDamageFX;

/* the main controller: the data fluid in the tubes to the controllers (0x4278 bytes) */
typedef struct cFXElementsMentalControllerMainFX {
	cMapElement base;
	int dcRoot;			/* 0x002c */
	vec3_t pos;			/* 0x0030 */
	byte nearCam;			/* 0x003c */
	byte pad_3d[3];
	int phase;			/* 0x0040 1..5, the act of the story */
	vec3_t top;			/* 0x0044 pos raised by 350 */
	cFXElementsMentalController *controllers[FXMENTALCONTROLLERMAINCOUNT];	/* 0x0050 */
	float levels[FXMENTALCONTROLLERLEVELS];					/* 0x0074 */
	/* the drops of data that run from one controller to the next (or up the tube) */
	vec3_t fluidPos[FXMENTALMAINFLUID];	/* 0x0094 the start */
	vec3_t fluidEnd[FXMENTALMAINFLUID];	/* 0x0b20 */
	vec3_t fluidDir[FXMENTALMAINFLUID];	/* 0x15ac end - start */
	vec3_t fluidNormal[FXMENTALMAINFLUID];	/* 0x2038 the unit of fluidDir */
	float fluidPhase[FXMENTALMAINFLUID];	/* 0x2ac4 0..1 along the way, over 1 a new drop */
	float fluidTime[FXMENTALMAINFLUID];	/* 0x2e48 the seconds of the way */
	float fluidWidth[FXMENTALMAINFLUID];	/* 0x31cc */
	int fluidTube[FXMENTALMAINFLUID];	/* 0x3550 */
	uint32_t fluidColor[FXMENTALMAINFLUID];	/* 0x38d4 */
	CGameTrail tubeTrails[FXMENTALMAINTRAILS];	/* 0x3c58 */
	CGameParticles dataFluid;		/* 0x3c94 DF_DataFluid */
	CGameTrail glowTrails[FXMENTALMAINTRAILS];	/* 0x3d44 */
	byte fluidActive[FXMENTALMAINFLUID];	/* 0x3d80 */
	byte pad_3e61[3];
	CGameParticles tubeGlow;		/* 0x3e64 DF_TubeGlow */
	float glowTime;				/* 0x3f14 the glow of the tube left */
	float glowDuration;			/* 0x3f18 */
	float glowPause;			/* 0x3f1c */
	float glowPhase[FXMENTALMAINGLOWS];	/* 0x3f20 */
	float glowSpeed[FXMENTALMAINGLOWS];	/* 0x3f98 */
	vec3_t glowOffset[FXMENTALMAINGLOWS];	/* 0x4010 */
	CGameTrail topTrails[2];		/* 0x4178 */
	int glowSide[FXMENTALMAINGLOWS];	/* 0x4180 */
	uint32_t glowColor[FXMENTALMAINGLOWS];	/* 0x41f8 */
	float scale;				/* 0x4270 */
	int last;				/* 0x4274 */
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
 * the parameters from the section FXFireDrop of the node (0x118 bytes, constructor rva 0x1cf850) */
typedef struct cFXElementsFireDrop {
	cMapElement base;
	int dcRoot;			/* 0x02c */
	vec3_t pos;			/* 0x030 */
	CGameParticles drops;		/* 0x03c the particles of DC_Root */
	byte nearCam;			/* 0x0ec */
	byte pad_ed[3];
	float burstTime;		/* 0x0f0 left of the burst, the pause below 0 */
	float sleepTime;		/* 0x0f4 the pause to the next burst */
	float emitTime[3];		/* 0x0f8 the time since the last drops of the three kinds */
	float scale;			/* 0x104 Scale */
	float size;			/* 0x108 Size */
	float speed;			/* 0x10c Speed */
	float area;			/* 0x110 Area */
	float life;			/* 0x114 Life */
} cFXElementsFireDrop;

/* a lens flare (0x30 bytes, its CGameLensFlare at 0x2c) */
typedef struct cFXElementsLensFlare {
	cMapElement base;
	byte flare[4];			/* 0x2c CGameLensFlare */
} cFXElementsLensFlare;

#define FXLIGHTINGBOLTS 10
#define FXLIGHTINGLINKS 4
#define FXLIGHTINGPOINTS 10

/* lightnings between the nodes DF_Link_<bolt><A..D> of the model: each bolt jumps between two of its links, a
 * trail with glow particles at both ends; the parameters from the section Lighting of the first link (0xe88 bytes,
 * constructor rva 0x1cf980) */
typedef struct cFXElementsLighting {
	cMapElement base;
	int dcRoot;					/* 0x02c */
	vec3_t pos;					/* 0x030 */
	CGameParticles glows;				/* 0x03c 4 per bolt: 2 at each end */
	byte visible;					/* 0x0ec */
	byte pad_ed[3];
	float animDur;					/* 0x0f0 animation 0 */
	float animFps;					/* 0x0f4 */
	float prevTime;					/* 0x0f8 of channel 0 */
	float time;					/* 0x0fc */
	int link[FXLIGHTINGBOLTS][FXLIGHTINGLINKS];	/* 0x100 render nodes, -1 after the last */
	int linkCount[FXLIGHTINGBOLTS];			/* 0x1a0 */
	int pair[FXLIGHTINGBOLTS][2];			/* 0x1c8 the links of the bolt now */
	byte glow[FXLIGHTINGBOLTS][FXLIGHTINGLINKS];	/* 0x218 Glow: particles at the link */
	uint32_t glowColor[FXLIGHTINGBOLTS][FXLIGHTINGLINKS];	/* 0x240 */
	float glowSizeMin[FXLIGHTINGBOLTS][FXLIGHTINGLINKS];	/* 0x2e0 */
	float glowSizeMax[FXLIGHTINGBOLTS][FXLIGHTINGLINKS];	/* 0x380 */
	uint32_t pointColor[FXLIGHTINGBOLTS][FXLIGHTINGLINKS];	/* 0x420 the trail colour at the link */
	float pointSizeMin[FXLIGHTINGBOLTS][FXLIGHTINGLINKS];	/* 0x4c0 */
	float pointSizeMax[FXLIGHTINGBOLTS][FXLIGHTINGLINKS];	/* 0x560 */
	float sizeMin[FXLIGHTINGBOLTS];			/* 0x600 */
	float sizeMax[FXLIGHTINGBOLTS];			/* 0x628 */
	float sizeVar[FXLIGHTINGBOLTS];			/* 0x650 */
	float lifeTimeMin[FXLIGHTINGBOLTS];		/* 0x678 */
	float lifeTimeMax[FXLIGHTINGBOLTS];		/* 0x6a0 */
	float sleepTimeMin[FXLIGHTINGBOLTS];		/* 0x6c8 */
	float sleepTimeMax[FXLIGHTINGBOLTS];		/* 0x6f0 */
	float tensorMin[FXLIGHTINGBOLTS];		/* 0x718 */
	float tensorMax[FXLIGHTINGBOLTS];		/* 0x740 */
	float randVarMin[FXLIGHTINGBOLTS];		/* 0x768 */
	float randVarMax[FXLIGHTINGBOLTS];		/* 0x790 */
	float anmFollow[FXLIGHTINGBOLTS];		/* 0x7b8 picked between the two below */
	float anmFollowMin[FXLIGHTINGBOLTS];		/* 0x7e0 */
	float anmFollowMax[FXLIGHTINGBOLTS];		/* 0x808 */
	byte anmLoopReset[FXLIGHTINGBOLTS];		/* 0x830 */
	byte saturate[FXLIGHTINGBOLTS];			/* 0x83a Saturate4X */
	float offsetSpeed[FXLIGHTINGBOLTS];		/* 0x844 */
	float life[FXLIGHTINGBOLTS];			/* 0x86c */
	float sleep[FXLIGHTINGBOLTS];			/* 0x894 */
	float size[FXLIGHTINGBOLTS];			/* 0x8bc */
	float pointSize[FXLIGHTINGBOLTS][2];		/* 0x8e4 */
	float offset[FXLIGHTINGBOLTS];			/* 0x934 the u of the trail texture */
	float glowSize[FXLIGHTINGBOLTS][2];		/* 0x95c */
	CGameTrail trails[FXLIGHTINGBOLTS];		/* 0x9ac */
	vec3_t pts[FXLIGHTINGBOLTS][FXLIGHTINGPOINTS];	/* 0x9d4 the control points of the bolt */
	int numPts;					/* 0xe84 6 */
} cFXElementsLighting;

cFXElementsFireDrop *cFXElementsFireDrop_ctor(cFXElementsFireDrop *self, const char *name,
	const vec3_t *pos, const vec3_t *rot);
/* the flare at pos: intensity 0.65, 0x30 0.3, on (rva 0x1cf8b0, Elements_Create passes no name) */
cFXElementsLensFlare *cFXElementsLensFlare_ctor(cFXElementsLensFlare *self, const char *name,
	const vec3_t *pos, const vec3_t *rot);
cFXElementsLighting *cFXElementsLighting_ctor(cFXElementsLighting *self, const char *name,
	const vec3_t *pos, const vec3_t *rot);

/* ---- FXElementsAB.cpp ---- */

#define FXABBOLTS 10
#define FXABRINGS 4
#define FXABRINGTRAILS 14

/* the fake limit of the level AB around the dummy DM_Mission_Centre: rings of trails and lightnings that grow when
 * the ship of the player comes near (0x498 bytes, constructor rva 0x1d3260) */
typedef struct cFXElementsABFakeLimit {
	cMapElement base;
	vec3_t centre;					/* 0x02c DM_Mission_Centre */
	CGameParticles lighting;			/* 0x038 Dummy01, 2 per bolt */
	float boltTime[FXABBOLTS];			/* 0x0e8 */
	CGameTrail bolts[FXABBOLTS];			/* 0x110 */
	float boltSize[FXABBOLTS];			/* 0x138 */
	vec3_t boltStart[FXABBOLTS];			/* 0x160 in the space of the model */
	vec3_t boltCtrl[FXABBOLTS][2];			/* 0x1d8 the moving middle points */
	CGameParticles nearParts;			/* 0x2c8 Dummy02 */
	float scroll[FXABRINGS];			/* 0x378 */
	vec3_t ringOfs[FXABRINGS];			/* 0x388 */
	CGameTrail rings[FXABRINGS][FXABRINGTRAILS];	/* 0x3b8 */
} cFXElementsABFakeLimit;

#define FXABMETEORS 500
#define FXABMETEORSPLIT 250	/* the meteors after this one are in the second particle system */

/* meteors along a curve over the level AB (0x5780 bytes, constructor rva 0x1d3310) */
typedef struct cFXElementsABFakeMeteors {
	cMapElement base;
	float t[FXABMETEORS];			/* 0x002c on the curve, 0..1 */
	float speed[FXABMETEORS];		/* 0x07fc */
	vec3_t ofs[FXABMETEORS];		/* 0x0fcc */
	float size[FXABMETEORS];		/* 0x273c */
	int sprite[FXABMETEORS];		/* 0x2f0c */
	float rot[FXABMETEORS];			/* 0x36dc */
	float rotSpeed[FXABMETEORS];		/* 0x3eac */
	float fade[FXABMETEORS];		/* 0x467c */
	float fadeSpeed[FXABMETEORS];		/* 0x4e4c */
	CGameParticles parts[2];		/* 0x561c Dummy01, Dummy02 */
	CGameTrail curve;			/* 0x577c only its Bezier helpers are used */
} cFXElementsABFakeMeteors;

/* the meteors and the limit of the dummies DM_Element_ABFakeMeteors and DM_Element_ABFakeLimit of the level */
void FXElementsAB_Create(void);

#endif
