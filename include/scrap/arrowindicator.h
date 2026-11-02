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

#include "scrap/gvar.h"
#include "scrap/cstring.h"
#include "scrap/entity.h"
#include "scrap/pyfunction.h"
#include "scrap/scrapparticles.h"

extern cGVar GVar_ArrowMissionFile;
extern cGVar GVar_ArrowTargetFile;

#define ARROW_MAX_OPTS 8

/* the glow of a mission arrow: particles with a timer, a color ramp and a scale from the model name */
typedef struct cArrowGlow {
	CGameParticles base;		/* 0x00 */
	float timer;			/* 0xb0 an emit every 0.66 s */
	int ramp;			/* 0xb4 1 for "/SM" models */
	float scale;			/* 0xb8 1 for "/B" models, else 0.1 */
} cArrowGlow;

/* a model of the pool: the next free one after the CGameModel */
typedef struct cArrowModel {
	CGameModel model;		/* 0x00 */
	struct cArrowModel *next;	/* 0x1c */
} cArrowModel;

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
	byte visible;		/* 0x71 the line from the player to it is free, retested every testTime of the target scorer */
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

/* the arrow and the collision sphere of an entity with life */

extern cGVar GVar_ArrowActiveRadius;
#define g_ArrowMaxDist (GVar_ArrowActiveRadius.value.f)	/* of the selection */
#define g_MissionArrowOn (&GVar_ArrowMissionFile.value.s)	/* the models of MissionArrows_Switch */
#define g_MissionArrowOff (&GVar_ArrowTargetFile.value.s)

/* in / |in| (in itself for a length 0), returns out */
float *Vec2_Normalize(float *out, const float *in);

cArrowIndicator *cArrowIndicator_ctor(cArrowIndicator *self);
void cArrowIndicator_dtor(cArrowIndicator *self);
/* "name:text" as the next option, 0 without ':' or with 8 options */
byte cArrowIndicator_AddOpt(cArrowIndicator *self, const char *s);
/* the use function (the view function without ArrowCanUse) with the user, the parent and the option name and text */
void cArrowIndicator_Execute(cArrowIndicator *self, cEntity *user);
/* other is the selection of self at angle ang within maxDist: its distance over both radii into *outDist; mark sets
 * the shown flag of other by g_ArrowNearDist, useAng the angle tests */
byte ArrowIndicator_Visible(cEntity *self, cEntity *other, float ang, float maxDist, float *outDist, byte mark,
	byte useAng);
/* the selected and shown flags of every entity with life cleared */
void ArrowIndicator_ClearSel(void);
void cArrowIndicator_ClearOpts(cArrowIndicator *self);
/* the nearest visible entity with an arrow is selected and returned (NULL for none) */
cEntity *ArrowIndicator_FindSel(cEntity *self, float ang);
byte cArrowIndicator_VarF(cArrowIndicator *self, const char *name, double *d, int set);
const char *cArrowIndicator_GetOpt(cArrowIndicator *self, int i);
uint32_t cArrowIndicator_GetColor(cArrowIndicator *self);
byte cArrowIndicator_VarI(cArrowIndicator *self, const char *name, int *i, int set);
void cArrowIndicator_NextOpt(cArrowIndicator *self);
void cArrowIndicator_PrevOpt(cArrowIndicator *self);
/* the model of the kind of arrow is made, the other released; on 0 (or faded out) both go */
void cArrowIndicator_SetModels(cArrowIndicator *self, byte on);
void cArrowIndicator_LoadMission(cArrowIndicator *self, const char *name);
void cArrowIndicator_SetParent(cArrowIndicator *self, cEntity *parent);
void cArrowGlow_SetVisible(cArrowGlow *self, byte on);
void cArrowIndicator_Update(cArrowIndicator *self);
byte cArrowIndicator_VarS(cArrowIndicator *self, const char *name, char **s, int set);

#endif
