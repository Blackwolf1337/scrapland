/*
 * Entities/More/ArrowIndicator.cpp of the game (src/game/Entities/More/ArrowIndicator.c): cArrowIndicator, the
 * arrow over an entity with life (at 0x314) that the player can use or look at, with its options ("ArrOptN",
 * "name:text") and the Python functions for the use and the view. The arrow model comes from a pool of free models;
 * a mission arrow ("ArrowCanMission") has its own model and a glow of particles (cArrowGlow) on its node
 * DOB_<B><S|M>Mission_Glow. The selection takes the nearest entity in front of the player within the global
 * distance.
 */
#ifndef SCRAP_ARROWINDICATOR_H
#define SCRAP_ARROWINDICATOR_H

#include "scrap/cstring.h"
#include "scrap/entity.h"
#include "scrap/pyfunction.h"
#include "scrap/scrapparticles.h"

#define ARROW_MAX_OPTS 8

/* the glow of a mission arrow: particles with a timer, a color ramp and a scale from the model name */
typedef struct cArrowGlow {
	CGameParticles base;		/* 0x00 */
	float timer;			/* 0xb0 an emit every 0.66 s */
	int ramp;			/* 0xb4 1 for "/SM" models */
	float scale;			/* 0xb8 1 for "/B" models, else 0.1 */
} cArrowGlow;
SD_SIZE(cArrowGlow, 0xbc);

/* a model of the pool: the next free one after the CGameModel */
typedef struct cArrowModel {
	CGameModel model;		/* 0x00 */
	struct cArrowModel *next;	/* 0x1c */
} cArrowModel;
SD_SIZE(cArrowModel, 0x20);

typedef struct cArrowIndicator {
	cEntity *parent;		/* 0x00 */
	cArrowModel *arrow;		/* 0x04 from the pool */
	CGameModel *mission;		/* 0x08 */
	cArrowGlow *glow;		/* 0x0c */
	byte missionOn;			/* 0x10 the mission model is the "on" one */
	byte pad_11[3];
	float alpha;			/* 0x14 fades in and out at 4 per second */
	byte shown;			/* 0x18 near enough (or a mission arrow) */
	byte pad_19[3];
	int nOpt;			/* 0x1c "ArrnOpt" */
	int iOpt;			/* 0x20 "ArriOpt" */
	cString opts[ARROW_MAX_OPTS];	/* 0x24 "ArrOptN" */
	cPy_Function onUse;		/* 0x44 "UseFunc" */
	cPy_Function onView;		/* 0x58 "ViewFunc" */
	byte selected;			/* 0x6c the selection of the player */
	byte canUse;			/* 0x6d "ArrowCanUse" */
	byte canView;			/* 0x6e "ArrowCanView" */
	byte canMission;		/* 0x6f "ArrowCanMission" */
	byte alwaysUse;			/* 0x70 "AlwaysUse" */
	byte b71;			/* 0x71 */
	byte pad_72[2];
	int tgType;			/* 0x74 "TgType": red, blue, green, none, killer */
	int coneStatus;			/* 0x78 "ConeStatus": off, relax, alert, danger */
	float coneFov;			/* 0x7c "ConeFOV", radians */
	float coneDist;			/* 0x80 "ConeDist" */
	byte canPhoto;			/* 0x84 "CanPhoto" */
	byte useAngSel;			/* 0x85 "UseAngSel" */
	byte useAbsAngSel;		/* 0x86 "UseAbsAngSel" */
	byte useMapCheck;		/* 0x87 "UseMapCheck" */
} cArrowIndicator;
SD_OFFSET(cArrowIndicator, opts, 0x24);
SD_OFFSET(cArrowIndicator, onView, 0x58);
SD_OFFSET(cArrowIndicator, selected, 0x6c);
SD_OFFSET(cArrowIndicator, coneFov, 0x7c);
SD_SIZE(cArrowIndicator, 0x88);

/* the arrow and the collision sphere of an entity with life */

/* the globals of ArrowIndicator.cpp */
#define g_ArrowPool GAME_GLOBAL(cArrowModel *, g_ArrowPool, 0x4479a8)
#define g_ArrowPoolCount GAME_GLOBAL(int, g_ArrowPoolCount, 0x4479ac)
#define g_ArrowMaxDist GAME_GLOBAL_IN(float, GVar_ArrowActiveRadius, 16, 0x4479a0)	/* of the selection */
#define g_ArrowNearDist GAME_GLOBAL_IN(float, GVar_ArrowViewRadius, 16, 0x4478c8)	/* shown nearer */
#define g_ArrowYMargin GAME_GLOBAL_IN(float, GVar_ArrowActiveHorizontal, 16, 0x4478e0)	/* the height difference allowed over both radii */
#define g_MissionArrowOn (&GAME_GLOBAL_IN(cString, GVar_ArrowMissionFile, 16, 0x4478f8))	/* the models of MissionArrows_Switch */
#define g_MissionArrowOff (&GAME_GLOBAL_IN(cString, GVar_ArrowTargetFile, 16, 0x447940))
#define g_ArrowUseModel GAME_GLOBAL_IN(char *, GVar_ArrowUseFile, 16, 0x447910)
#define g_ArrowGlowDist GAME_GLOBAL_IN(float, GVar_FXArrowIndicatorFlash_DistGrow, 16, 0x447970)
#define g_ArrowGlowMin GAME_GLOBAL_IN(float, GVar_FXArrowIndicatorFlash_SizeMin, 16, 0x447988)
#define g_ArrowGlowMax GAME_GLOBAL_IN(float, GVar_FXArrowIndicatorFlash_SizeMax, 16, 0x447958)
#define g_ArrowGlowRamps (GAME_ARRAY(uint32_t[9], g_ArrowGlowRamps, 0x428628))

/* in / |in| (in itself for a length 0), returns out */
float *Vec2_Normalize(float *out, const float *in);

cArrowIndicator *__fastcall cArrowIndicator_ctor(cArrowIndicator *self);
void __fastcall cArrowIndicator_dtor(cArrowIndicator *self);
/* "name:text" as the next option, 0 without ':' or with 8 options */
byte __thiscall cArrowIndicator_AddOpt(cArrowIndicator *self, const char *s);
/* the use function (the view function without ArrowCanUse) with the user, the parent and the option name and text */
void __thiscall cArrowIndicator_Execute(cArrowIndicator *self, cEntity *user);
/* other is the selection of self at angle ang within maxDist: its distance over both radii into *outDist; mark sets
 * the shown flag of other by g_ArrowNearDist, useAng the angle tests */
byte ArrowIndicator_Visible(cEntity *self, cEntity *other, float ang, float maxDist, float *outDist, byte mark,
	byte useAng);
/* the selected and shown flags of every entity with life cleared */
void ArrowIndicator_ClearSel(void);
void __fastcall cArrowIndicator_ClearOpts(cArrowIndicator *self);
/* the nearest visible entity with an arrow is selected and returned (NULL for none) */
cEntity *ArrowIndicator_FindSel(cEntity *self, float ang);
byte __thiscall cArrowIndicator_VarF(cArrowIndicator *self, const char *name, double *d, int set);
/* a model of the pool or a new one (*fresh then) */
cArrowModel *ArrowPool_Get(byte *fresh);
const char *__thiscall cArrowIndicator_GetOpt(cArrowIndicator *self, int i);
uint32_t __fastcall cArrowIndicator_GetColor(cArrowIndicator *self);
byte __thiscall cArrowIndicator_VarI(cArrowIndicator *self, const char *name, int *i, int set);
void __fastcall cArrowIndicator_NextOpt(cArrowIndicator *self);
void __fastcall cArrowIndicator_PrevOpt(cArrowIndicator *self);
void __fastcall cArrowIndicator_ReleaseMission(cArrowIndicator *self);
/* the arrow model back to the pool (6 at most) */
void __fastcall cArrowIndicator_ReleaseArrow(cArrowIndicator *self);
/* the model of the kind of arrow is made, the other released; on 0 (or faded out) both go */
void __thiscall cArrowIndicator_SetModels(cArrowIndicator *self, byte on);
void __thiscall cArrowIndicator_SetShown(cArrowIndicator *self, byte on);
void __thiscall cArrowIndicator_LoadMission(cArrowIndicator *self, const char *name);
void __thiscall cArrowIndicator_LoadArrow(cArrowIndicator *self, const char *name);
/* <name>.M3D with the animation <name>.anim.M3D running */
void __thiscall cArrowIndicator_LoadModel(cArrowIndicator *self, CGameModel *model, const char *name);
void __thiscall cArrowIndicator_SetUseFunc(cArrowIndicator *self, const char *name);
void __thiscall cArrowIndicator_SetViewFunc(cArrowIndicator *self, const char *name);
byte __thiscall cArrowIndicator_SetOpt(cArrowIndicator *self, int i, const char *s);
void __thiscall cArrowIndicator_SetParent(cArrowIndicator *self, cEntity *parent);
void __thiscall cArrowGlow_SetVisible(cArrowGlow *self, byte on);
void __thiscall cArrowGlow_Attach(cArrowGlow *self, CGameModel *model, const char *name);
byte __thiscall cArrowIndicator_SetIOpt(cArrowIndicator *self, int i);
void __fastcall cArrowIndicator_Update(cArrowIndicator *self);
void __thiscall cArrowGlow_Update(cArrowGlow *self, CGameModel *model);
byte __thiscall cArrowIndicator_VarS(cArrowIndicator *self, const char *name, char **s, int set);
void __fastcall cArrowIndicator_Reload(cArrowIndicator *self);

#endif
