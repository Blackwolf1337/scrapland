/*
 * The effect elements of the levels (World/FX/FXElements*.cpp of the game): map elements (cMapElement, a model of
 * the level in the list g_pWorld->elements) made at the level start from the dummies "DM_Element_*" of the level.
 * Implemented here: FXElements.cpp (fire drops, lens flares, lightnings), FXElementsAB.cpp (the fake limit and the
 * meteors of the level AB), FXElementsOrbit.cpp (the orbit buoys and their controller) and FXElementsPress.cpp (the
 * mental controllers of the Press building).
 */
#ifndef SCRAP_FXELEMENTS_H
#define SCRAP_FXELEMENTS_H

#include "scrap/crt.h"
#include "scrap/scrapparticles.h"
#include "scrap/scraptrails.h"

typedef struct cMapElement cMapElement;

/* cMapElement::vftable (rva 0x3cde68) and the ones of the elements (thiscall) */
typedef struct cMapElementVtbl {
	cMapElement *(__thiscall *Delete)(cMapElement *self, byte flags);	/* 0x0 */
	byte(__thiscall *Init)(cMapElement *self);			/* 0x4 vf01 after the creation, 1 */
	void(__thiscall *Update)(cMapElement *self, float dt);		/* 0x8 vf02 every frame */
} cMapElementVtbl;
SD_SIZE(cMapElementVtbl, 0xc);

/* World/MapElement: an element of the level with its model (0x2c bytes) */
struct cMapElement {
	const cMapElementVtbl *vftable;	/* 0x00 */
	CGameModel Mod;			/* 0x04 */
	int anim;			/* 0x20 the animation of the model */
	char *name;			/* 0x24 cString */
	cMapElement *next;		/* 0x28 g_pWorld->elements */
};
SD_OFFSET(cMapElement, next, 0x28);
SD_SIZE(cMapElement, 0x2c);

#define World_Elements(w) (*(cMapElement **)(w)->elements)

/* a new element initialized and put first into the list of the world */
static inline void MapElement_Add(cMapElement *e) {
	e->vftable->Init(e);
	e->next = World_Elements(g_pWorld);
	World_Elements(g_pWorld) = e;
}

/* rva 0x202ca0: the model of the element name at pos and rot (radians), 0x202e20 the destructor */
cMapElement *__thiscall cMapElement_ctor(cMapElement *self, const char *name, const vec3_t *pos, const vec3_t *rot);
void __thiscall cMapElement_dtor(cMapElement *self);
cMapElement *__thiscall cMapElement_Delete(cMapElement *self, byte flags);
/* rva 0x203310 (1), 0x203520 (the animation of the visible model) */
byte __thiscall cMapElement_Init(cMapElement *self);
void __thiscall cMapElement_Update(cMapElement *self, float dt);

#define CFXELEMENTSORBITBOYA_VFTABLE 0x3cc574
#define CFXELEMENTSORBITBOYACONTROLLER_VFTABLE 0x3cc584
#define CFXELEMENTSMENTALCONTROLLER_VFTABLE 0x3cc7f4
#define CFXELEMENTSMENTALCONTROLLERDAMAGEFX_VFTABLE 0x3cc804
#define CFXELEMENTSMENTALCONTROLLERMAINFX_VFTABLE 0x3cc814

/* ---- FXElementsOrbit.cpp ---- */

#define FXORBITBOYACOLUMNS 4
#define FXORBITBOYACOLUMNCOUNT 8

/* a buoy: a light particle over its model (0xf0 bytes) */
typedef struct cFXElementsOrbitBoya {
	cMapElement base;
	int dcRoot;			/* 0x2c the node DC_Root */
	vec3_t pos;			/* 0x30 */
	byte pad_3c[4];
	CGameParticles light;		/* 0x40 DF_Light */
} cFXElementsOrbitBoya;
SD_OFFSET(cFXElementsOrbitBoya, light, 0x40);
SD_SIZE(cFXElementsOrbitBoya, 0xf0);

/* the buoys by column and number, their lights blink in turns (0x1ac bytes, constructor rva 0x1d5820) */
typedef struct cFXElementsOrbitBoyaController {
	cMapElement base;
	cFXElementsOrbitBoya *boya[FXORBITBOYACOLUMNS][FXORBITBOYACOLUMNCOUNT];	/* 0x02c */
	int type[FXORBITBOYACOLUMNS][FXORBITBOYACOLUMNCOUNT];			/* 0x0ac the look of the light, 0 or 1 */
	float fade[FXORBITBOYACOLUMNS][FXORBITBOYACOLUMNCOUNT];			/* 0x12c 1 down to 0, again */
} cFXElementsOrbitBoyaController;
SD_OFFSET(cFXElementsOrbitBoyaController, type, 0xac);
SD_OFFSET(cFXElementsOrbitBoyaController, fade, 0x12c);
SD_SIZE(cFXElementsOrbitBoyaController, 0x1ac);

cFXElementsOrbitBoyaController *__thiscall cFXElementsOrbitBoyaController_ctor(cFXElementsOrbitBoyaController *self,
	const char *name, const vec3_t *pos, const vec3_t *rot);

byte __fastcall cFXElementsOrbitBoya_Init(cFXElementsOrbitBoya *self);
void __thiscall cFXElementsOrbitBoya_Update(cFXElementsOrbitBoya *self, float dt);
byte __fastcall cFXElementsOrbitBoyaController_Init(cFXElementsOrbitBoyaController *self);
void __thiscall cFXElementsOrbitBoyaController_Update(cFXElementsOrbitBoyaController *self, float dt);
void __thiscall cFXElementsOrbitBoyaController_SetBoya(cFXElementsOrbitBoyaController *self, int type, int column,
	int number, cFXElementsOrbitBoya *boya);
/* the controller and the buoys of the dummies DM_Element_OrbitBoya* of the level */
void FXElementsOrbit_Create(void);
cFXElementsOrbitBoya *__thiscall cFXElementsOrbitBoya_ctor(cFXElementsOrbitBoya *self, const char *name,
	const vec3_t *pos, const vec3_t *rot);
void __fastcall cFXElementsOrbitBoya_dtor(cFXElementsOrbitBoya *self);
void __fastcall cFXElementsOrbitBoyaController_dtor(cFXElementsOrbitBoyaController *self);
cFXElementsOrbitBoya *__thiscall cFXElementsOrbitBoya_Delete(cFXElementsOrbitBoya *self, byte flags);
cFXElementsOrbitBoyaController *__thiscall cFXElementsOrbitBoyaController_Delete(cFXElementsOrbitBoyaController *self,
	byte flags);
/* the light released; the controller has nothing to release */
void __fastcall cFXElementsOrbitBoya_ReleaseLight(cFXElementsOrbitBoya *self);
void __fastcall cFXElementsOrbitBoyaController_Release(cFXElementsOrbitBoyaController *self);

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
SD_SIZE(cFXElementsMentalController, 0x40);

/* the damaged controller of the epilogue: smoke and flashes from emitter nodes (0x3f8 bytes) */
typedef struct cFXElementsMentalControllerDamageFX {
	cFXElementsMentalController base;
	CGameParticles smoke;		/* 0x040 DF_Smoke */
	int smokeTime;			/* 0x0f0 */
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
	int flashTime;			/* 0x314 */
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
SD_OFFSET(cFXElementsMentalControllerDamageFX, smokeSizeIni, 0x224);
SD_OFFSET(cFXElementsMentalControllerDamageFX, flash, 0x264);
SD_OFFSET(cFXElementsMentalControllerDamageFX, flashRot, 0x358);
SD_OFFSET(cFXElementsMentalControllerDamageFX, flashSparksMax, 0x3e8);
SD_SIZE(cFXElementsMentalControllerDamageFX, 0x3f8);

/* the main controller: the data fluid in the tubes to the controllers (0x4278 bytes) */
typedef struct cFXElementsMentalControllerMainFX {
	cMapElement base;
	int dcRoot;			/* 0x002c */
	vec3_t pos;			/* 0x0030 */
	byte pad_3c[4];
	int phase;			/* 0x0040 1..5, the act of the story */
	vec3_t top;			/* 0x0044 pos raised by 350 */
	cFXElementsMentalController *controllers[FXMENTALCONTROLLERMAINCOUNT];	/* 0x0050 */
	float levels[FXMENTALCONTROLLERLEVELS];					/* 0x0074 */
	byte pad_94[0x2ac4 - 0x94];
	float fluidSpeed[FXMENTALMAINFLUID];	/* 0x2ac4 */
	byte pad_2e48[0x3550 - 0x2e48];
	int fluidTube[FXMENTALMAINFLUID];	/* 0x3550 */
	uint32_t fluidColor[FXMENTALMAINFLUID];	/* 0x38d4 */
	CGameTrail tubeTrails[FXMENTALMAINTRAILS];	/* 0x3c58 */
	CGameParticles dataFluid;		/* 0x3c94 DF_DataFluid */
	CGameTrail glowTrails[FXMENTALMAINTRAILS];	/* 0x3d44 */
	byte pad_3d80[0x3e64 - 0x3d80];
	CGameParticles tubeGlow;		/* 0x3e64 DF_TubeGlow */
	int glow3f14;				/* 0x3f14 */
	int glow3f18;				/* 0x3f18 */
	int glow3f1c;				/* 0x3f1c */
	float glowPhase[FXMENTALMAINGLOWS];	/* 0x3f20 */
	float glowSpeed[FXMENTALMAINGLOWS];	/* 0x3f98 */
	byte pad_4010[0x4178 - 0x4010];
	CGameTrail topTrails[2];		/* 0x4178 */
	int glowSide[FXMENTALMAINGLOWS];	/* 0x4180 */
	uint32_t glowColor[FXMENTALMAINGLOWS];	/* 0x41f8 */
	float scale;				/* 0x4270 */
	int last;				/* 0x4274 */
} cFXElementsMentalControllerMainFX;
SD_OFFSET(cFXElementsMentalControllerMainFX, levels, 0x74);
SD_OFFSET(cFXElementsMentalControllerMainFX, fluidSpeed, 0x2ac4);
SD_OFFSET(cFXElementsMentalControllerMainFX, tubeTrails, 0x3c58);
SD_OFFSET(cFXElementsMentalControllerMainFX, dataFluid, 0x3c94);
SD_OFFSET(cFXElementsMentalControllerMainFX, glowTrails, 0x3d44);
SD_OFFSET(cFXElementsMentalControllerMainFX, tubeGlow, 0x3e64);
SD_OFFSET(cFXElementsMentalControllerMainFX, glowSpeed, 0x3f98);
SD_OFFSET(cFXElementsMentalControllerMainFX, topTrails, 0x4178);
SD_OFFSET(cFXElementsMentalControllerMainFX, glowColor, 0x41f8);
SD_SIZE(cFXElementsMentalControllerMainFX, 0x4278);

/* the main controller of the level, NULL before the Press elements are made */
#define g_MentalMainFX GAME_DATA(cFXElementsMentalControllerMainFX *, 0x46719c)
#define g_MentalControllerMaxDist GAME_DATA(float, 0x4671f8)	/* FXElementsMentalControllerMaxDist */

cFXElementsMentalController *__thiscall cFXElementsMentalController_ctor(cFXElementsMentalController *self,
	const char *name, const vec3_t *pos, const vec3_t *rot);
cFXElementsMentalControllerDamageFX *__thiscall cFXElementsMentalControllerDamageFX_ctor(
	cFXElementsMentalControllerDamageFX *self, const char *name, const vec3_t *pos, const vec3_t *rot);
cFXElementsMentalControllerMainFX *__thiscall cFXElementsMentalControllerMainFX_ctor(
	cFXElementsMentalControllerMainFX *self, const char *name, const vec3_t *pos, const vec3_t *rot);
void __fastcall cFXElementsMentalController_dtor(cFXElementsMentalController *self);
void __fastcall cFXElementsMentalControllerDamageFX_dtor(cFXElementsMentalControllerDamageFX *self);
void __fastcall cFXElementsMentalControllerMainFX_dtor(cFXElementsMentalControllerMainFX *self);
cFXElementsMentalController *__thiscall cFXElementsMentalController_Delete(cFXElementsMentalController *self,
	byte flags);
cFXElementsMentalControllerDamageFX *__thiscall cFXElementsMentalControllerDamageFX_Delete(
	cFXElementsMentalControllerDamageFX *self, byte flags);
cFXElementsMentalControllerMainFX *__thiscall cFXElementsMentalControllerMainFX_Delete(
	cFXElementsMentalControllerMainFX *self, byte flags);
/* the particles of the damaged controller and the data fluid released (not referenced by the exe) */
void __fastcall cFXElementsMentalControllerDamageFX_ReleaseParticles(cFXElementsMentalControllerDamageFX *self);
void __fastcall cFXElementsMentalControllerMainFX_ReleaseFluid(cFXElementsMentalControllerMainFX *self);
/* every trail cleared, every particle out */
void __fastcall cFXElementsMentalControllerMainFX_Clear(cFXElementsMentalControllerMainFX *self);

byte __fastcall cFXElementsMentalController_Init(cFXElementsMentalController *self);
void __thiscall cFXElementsMentalController_Update(cFXElementsMentalController *self, float dt);
byte __fastcall cFXElementsMentalControllerDamageFX_Init(cFXElementsMentalControllerDamageFX *self);
byte __fastcall cFXElementsMentalControllerMainFX_Init(cFXElementsMentalControllerMainFX *self);
void __thiscall cFXElementsMentalControllerMainFX_SetController(cFXElementsMentalControllerMainFX *self, int index,
	cFXElementsMentalController *ctrl);
void __thiscall cFXElementsMentalControllerMainFX_SetPhase(cFXElementsMentalControllerMainFX *self, int phase);
/* the main controller and the controllers of the dummies of the level, by the act of the story (SaveVar GameAct) */
void FXElementsPress_Create(void);

/* rand() of the exe in [0, 1] times range, plus add (all in double), the way the effects pick their values */
#define g_FxRandScale GAME_DATA(double, 0x3a3d60)	/* 1/32767 */
static inline float FxElem_Rand(double range, double add) {
	return (float)((double)(crt_rand() & 0x7fff) * g_FxRandScale * range + add);
}

/* ---- FXElements.cpp ---- */

#define CFXELEMENTSLIGHTING_VFTABLE 0x3cc0fc
#define CFXELEMENTSFIREDROP_VFTABLE 0x3cc10c
#define CFXELEMENTSLENSFLARE_VFTABLE 0x3cc11c

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
SD_OFFSET(cFXElementsFireDrop, drops, 0x3c);
SD_OFFSET(cFXElementsFireDrop, burstTime, 0xf0);
SD_OFFSET(cFXElementsFireDrop, scale, 0x104);
SD_SIZE(cFXElementsFireDrop, 0x118);

/* a lens flare (0x30 bytes, its CGameLensFlare at 0x2c) */
typedef struct cFXElementsLensFlare {
	cMapElement base;
	byte flare[4];			/* 0x2c CGameLensFlare */
} cFXElementsLensFlare;
SD_SIZE(cFXElementsLensFlare, 0x30);

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
SD_OFFSET(cFXElementsLighting, link, 0x100);
SD_OFFSET(cFXElementsLighting, glow, 0x218);
SD_OFFSET(cFXElementsLighting, pointSizeMax, 0x560);
SD_OFFSET(cFXElementsLighting, randVarMax, 0x790);
SD_OFFSET(cFXElementsLighting, anmLoopReset, 0x830);
SD_OFFSET(cFXElementsLighting, offsetSpeed, 0x844);
SD_OFFSET(cFXElementsLighting, glowSize, 0x95c);
SD_OFFSET(cFXElementsLighting, trails, 0x9ac);
SD_OFFSET(cFXElementsLighting, numPts, 0xe84);
SD_SIZE(cFXElementsLighting, 0xe88);

#define g_FireDropMaxDist GAME_DATA(float, 0x467074)	/* FXElementsFireDropMaxDist */
#define g_FireDropRateAtt GAME_DATA(float, 0x4670d4)	/* FXElementsFireDropRateAtt */
#define g_FireDropSoundAttIni GAME_DATA(float, 0x46708c)	/* FXElementsFireDropSoundAttIni */
#define g_FireDropSoundAttEnd GAME_DATA(float, 0x4670bc)	/* FXElementsFireDropSoundAttEnd */
#define g_FireDropSoundVol GAME_DATA(float, 0x4670a4)	/* FXElementsFireDropSoundVol */
#define g_FireDropSound GAME_DATA(char *, 0x4670ec)	/* FXElementsFireDropSound, a cString */

cFXElementsFireDrop *__thiscall cFXElementsFireDrop_ctor(cFXElementsFireDrop *self, const char *name,
	const vec3_t *pos, const vec3_t *rot);
cFXElementsLighting *__thiscall cFXElementsLighting_ctor(cFXElementsLighting *self, const char *name,
	const vec3_t *pos, const vec3_t *rot);
/* rva 0x1cf8b0, Elements_Create passes no name */
cFXElementsLensFlare *__thiscall cFXElementsLensFlare_ctor(cFXElementsLensFlare *self, const char *name,
	const vec3_t *pos, const vec3_t *rot);
void __fastcall cFXElementsFireDrop_dtor(cFXElementsFireDrop *self);
void __fastcall cFXElementsLensFlare_dtor(cFXElementsLensFlare *self);
void __fastcall cFXElementsLighting_dtor(cFXElementsLighting *self);
cFXElementsFireDrop *__thiscall cFXElementsFireDrop_Delete(cFXElementsFireDrop *self, byte flags);
cFXElementsLensFlare *__thiscall cFXElementsLensFlare_Delete(cFXElementsLensFlare *self, byte flags);
cFXElementsLighting *__thiscall cFXElementsLighting_Delete(cFXElementsLighting *self, byte flags);
/* the particles (and trails) released; the lens flare has nothing to release */
void __fastcall cFXElementsFireDrop_Release(cFXElementsFireDrop *self);
void __fastcall cFXElementsLensFlare_Release(cFXElementsLensFlare *self);
void __fastcall cFXElementsLighting_Release(cFXElementsLighting *self);

byte __fastcall cFXElementsFireDrop_Init(cFXElementsFireDrop *self);
void __thiscall cFXElementsFireDrop_Update(cFXElementsFireDrop *self, float dt);
byte __fastcall cFXElementsLensFlare_Init(cFXElementsLensFlare *self);
void __thiscall cFXElementsLensFlare_Update(cFXElementsLensFlare *self, float dt);
byte __fastcall cFXElementsLighting_Init(cFXElementsLighting *self);
void __thiscall cFXElementsLighting_Update(cFXElementsLighting *self, float dt);

/* ---- FXElementsAB.cpp ---- */

#define CFXELEMENTSABFAKEMETEORS_VFTABLE 0x3cc3bc
#define CFXELEMENTSABFAKELIMIT_VFTABLE 0x3cc3cc

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
SD_OFFSET(cFXElementsABFakeLimit, lighting, 0x38);
SD_OFFSET(cFXElementsABFakeLimit, bolts, 0x110);
SD_OFFSET(cFXElementsABFakeLimit, boltCtrl, 0x1d8);
SD_OFFSET(cFXElementsABFakeLimit, nearParts, 0x2c8);
SD_OFFSET(cFXElementsABFakeLimit, rings, 0x3b8);
SD_SIZE(cFXElementsABFakeLimit, 0x498);

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
SD_OFFSET(cFXElementsABFakeMeteors, ofs, 0xfcc);
SD_OFFSET(cFXElementsABFakeMeteors, size, 0x273c);
SD_OFFSET(cFXElementsABFakeMeteors, fadeSpeed, 0x4e4c);
SD_OFFSET(cFXElementsABFakeMeteors, parts, 0x561c);
SD_OFFSET(cFXElementsABFakeMeteors, curve, 0x577c);
SD_SIZE(cFXElementsABFakeMeteors, 0x5780);

/* the engine variables FXABFakeLimit* */
#define g_ABLimitRingRotSpeed GAME_DATA(float, 0x46711c)	/* RingRotSpeed */
#define g_ABLimitSegmentStep GAME_DATA(float, 0x467104)	/* SegmentStep */
#define g_ABLimitSegmentSize GAME_DATA(float, 0x467164)	/* SegmentSize */
#define g_ABLimitRadius GAME_DATA(float, 0x467134)		/* Radius */
#define g_ABLimitDistLighting GAME_DATA(float, 0x46714c)	/* DistLighting */
#define g_ABLimitQuakeFactor GAME_DATA(float, 0x46717c)	/* QuakeFactor */
#define g_ABLimitQuakeTime GAME_DATA(float, 0x467194)		/* QuakeTime */

cFXElementsABFakeLimit *__thiscall cFXElementsABFakeLimit_ctor(cFXElementsABFakeLimit *self, const char *name,
	const vec3_t *pos, const vec3_t *rot);
cFXElementsABFakeMeteors *__thiscall cFXElementsABFakeMeteors_ctor(cFXElementsABFakeMeteors *self, const char *name,
	const vec3_t *pos, const vec3_t *rot);
void __fastcall cFXElementsABFakeLimit_dtor(cFXElementsABFakeLimit *self);
void __fastcall cFXElementsABFakeMeteors_dtor(cFXElementsABFakeMeteors *self);
cFXElementsABFakeLimit *__thiscall cFXElementsABFakeLimit_Delete(cFXElementsABFakeLimit *self, byte flags);
cFXElementsABFakeMeteors *__thiscall cFXElementsABFakeMeteors_Delete(cFXElementsABFakeMeteors *self, byte flags);
/* the bolt times cleared, every trail cleared */
void __fastcall cFXElementsABFakeLimit_Clear(cFXElementsABFakeLimit *self);

byte __fastcall cFXElementsABFakeLimit_Init(cFXElementsABFakeLimit *self);
void __thiscall cFXElementsABFakeLimit_Update(cFXElementsABFakeLimit *self, float dt);
byte __fastcall cFXElementsABFakeMeteors_Init(cFXElementsABFakeMeteors *self);
void __thiscall cFXElementsABFakeMeteors_Update(cFXElementsABFakeMeteors *self, float dt);
/* the meteors and the limit of the dummies DM_Element_ABFakeMeteors and DM_Element_ABFakeLimit of the level */
void FXElementsAB_Create(void);

#endif
