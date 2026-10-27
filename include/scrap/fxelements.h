/*
 * The effect elements of the levels (World/FX/FXElements*.cpp of the game): map elements (cMapElement, a model of
 * the level in the list g_pWorld->elements) made at the level start from the dummies "DM_Element_*" of the level.
 * Implemented here: FXElementsOrbit.cpp (the orbit buoys and their controller) and FXElementsPress.cpp (the mental
 * controllers of the Press building).
 */
#ifndef SCRAP_FXELEMENTS_H
#define SCRAP_FXELEMENTS_H

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

/* rva 0x202ca0: the model of the element name at pos and rot (radians), 0x202e20 the destructor */
cMapElement *__thiscall cMapElement_ctor(cMapElement *self, const char *name, const vec3_t *pos, const vec3_t *rot);
void __thiscall cMapElement_dtor(cMapElement *self);
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

#endif
