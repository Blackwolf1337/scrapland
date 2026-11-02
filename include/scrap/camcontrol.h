/*
 * The camera entities of the world (no CHECK of their own, the code sits before Entities/Char/Type/Banker.cpp):
 * cCamControl ("Cam", src/game/Entities/CamControl.c) with a CGameCamera and the view slot it shows,
 * cCamPathEntity ("CamPath", CamPathEntity.c) that moves its camera along up to 32 points (each one following a
 * "Cam" entity by name) and cFollowCamEntity ("FCam", FollowCamEntity.c) that follows an object ("Obj") from behind,
 * or flies free with the input of its view slot. The world makes them in cWorld_CreateEntity; their constructors
 * and the first functions of the follow camera are declared in include/scrap/world.h under the older cCamEntity_
 * names.
 */
#ifndef SCRAP_CAMCONTROL_H
#define SCRAP_CAMCONTROL_H

#include "scrap/gvar.h"
#include "scrap/objentity.h"
#include "scrap/scrapcamera.h"
#include "scrap/world.h"

#define TYEN_CAM "Cam"
#define TYEN_CAMPATH "CamPath"
#define TYEN_FCAM "FCam"

#define CCAMCONTROL_SIZE sizeof(cCamControl)
#define CCAMPATHENTITY_SIZE sizeof(cCamPathEntity)
#define CFOLLOWCAMENTITY_SIZE sizeof(cFollowCamEntity)

/* the strings of the exe the camera functions hand out or compare */
#define CAM_CLASSNAME "Cam"
#define CAMPATH_CLASSNAME "CamPath"
#define FCAM_CLASSNAME "FCam"
#define FCAM_ONSWITCH_DESC "<OnSwtich func>"

typedef struct cCamControl {
	cEntity base;			/* 0x000 */
	CGameCamera cam;		/* 0x07c "camPos", "camAng" (rot), "clipNear", "clipFar", "Fov" */
	int slot;			/* 0x0b8 the view it shows ("UsrControl"), -1 for none */
	char link[0x80];		/* 0x0bc "EntityLink": the object whose camera node it uses, "" for none */
} cCamControl;
SD_OFFSET(cCamControl, cam, 0x7c);
SD_OFFSET(cCamControl, slot, 0xb8);
SD_OFFSET(cCamControl, link, 0xbc);
SD_SIZE(cCamControl, 0x13c);

/* a point of the path ("NameNN" variables of the CamPath, NN 00 to 32: the index 32 is past the array) */
typedef struct cCamPathPoint {
	char cam[0x80];			/* 0x00 "TgtCamNN": the "Cam" entity the point copies when the path reaches it */
	vec3_t pos;			/* 0x80 "PosNN" */
	vec3_t ang;			/* 0x8c "AngNN" (degrees outside) */
	float zNear;			/* 0x98 "NearNN" 10 */
	float zFar;			/* 0x9c "FarNN" 40000 */
	float fov;			/* 0xa0 "FOVNN" pi/2 */
	float time;			/* 0xa4 "TimeNN" 1: the seconds to the next point */
	byte fadeIn;			/* 0xa8 "FadeInNN" 1: the move starts slow */
	byte fadeOut;			/* 0xa9 "FadeOutNN" 1: the move ends slow */
	byte pad_aa[2];
	float quake;			/* 0xac "QuakeNN" 0: the random shake of the angles */
} cCamPathPoint;
SD_OFFSET(cCamPathPoint, pos, 0x80);
SD_OFFSET(cCamPathPoint, fadeIn, 0xa8);
SD_SIZE(cCamPathPoint, 0xb0);

#define CAMPATH_POINTS 32

typedef struct cCamPathEntity {
	cCamControl base;		/* 0x000 */
	cPy_Function onEnd;		/* 0x13c "OnEnd" (name), once at the end */
	float time;			/* 0x150 "Time" since the start */
	int numPoints;			/* 0x154 "nWayPoint" */
	cCamPathPoint points[CAMPATH_POINTS];	/* 0x158 */
} cCamPathEntity;
SD_OFFSET(cCamPathEntity, onEnd, 0x13c);
SD_OFFSET(cCamPathEntity, points, 0x158);
SD_SIZE(cCamPathEntity, 0x1758);

typedef struct cFollowCamEntity {
	cCamControl base;		/* 0x000 */
	cEntity *target;		/* 0x13c "Target": an "Obj", its cam field points back */
	byte freeCam;			/* 0x140 "FreeCam": no target, the input of the slot moves the camera */
	char mainTarget[0x80];		/* 0x141 "MainTarget": the target when FreeCam goes off */
	byte pad_1c1[3];
	float hSpeedTo;			/* 0x1c4 5: HSpeed approaches it */
	float vSpeedTo;			/* 0x1c8 5 */
	float hSpeedRate;		/* 0x1cc 2 per second */
	float vSpeedRate;		/* 0x1d0 2 */
	float retSpeed;			/* 0x1d4 "RetSpd" 1000: the distance comes back this fast */
	float prevAng[2];		/* 0x1d8 the angles (x, y) of the slot before */
	float curAng[2];		/* 0x1e0 of the last slot */
	float ang[2];			/* 0x1e8 the angles lerped between them (vf29, vf30) */
	float dist;			/* 0x1f0 the distance now, shorter where the world is in the way */
	vec3_t center;			/* 0x1f4 the point it looks at */
	int slowFrames;			/* 0x200 frames over 1/15 s, counted down again */
	float tiltTo;			/* 0x204 the walk tilt approaches it (WalkCameraCenter) */
	float tilt;			/* 0x208 */
	float addTurn;			/* 0x20c "AddTurn" */
	float addTurnSpeed;		/* 0x210 "AddTurnSpeed" */
	float camDist;			/* 0x214 "CamDist" VehicleCamDist */
	float camAng;			/* 0x218 "CamAng" -15 degrees */
	float camRot;			/* 0x21c "CamRot" */
	float quakeTime;		/* 0x220 "QuakeTime" */
	float quakeFactor;		/* 0x224 "QuakeFactor" */
	float hSpeed;			/* 0x228 "HSpeed" 5 */
	float vSpeed;			/* 0x22c "VSpeed" 5 */
	cPy_Function onSwitch;		/* 0x230 "OnSwitch" (name, slot), on every target change */
} cFollowCamEntity;
SD_OFFSET(cFollowCamEntity, target, 0x13c);
SD_OFFSET(cFollowCamEntity, mainTarget, 0x141);
SD_OFFSET(cFollowCamEntity, hSpeedTo, 0x1c4);
SD_OFFSET(cFollowCamEntity, dist, 0x1f0);
SD_OFFSET(cFollowCamEntity, slowFrames, 0x200);
SD_OFFSET(cFollowCamEntity, camDist, 0x214);
SD_OFFSET(cFollowCamEntity, vSpeed, 0x22c);
SD_OFFSET(cFollowCamEntity, onSwitch, 0x230);
SD_SIZE(cFollowCamEntity, 0x244);

/* the slots of the camera vtables after the 23 of cEntity */
typedef struct cCamControlVtbl {
	cEntityVtbl base;
	void(*GetCamPos)(cEntity *self, float *x, float *y, float *z);	/* 0x5c vf23 */
	void(*SetCamPos)(cEntity *self, float x, float y, float z);		/* 0x60 vf24 */
	void(*Lerp)(cEntity *self, float f);				/* 0x64 vf25 between two slots */
	void(*Slot)(cEntity *self, float dt);				/* 0x68 vf26 */
	byte(*SetTarget)(cEntity *self, const char *name);			/* 0x6c vf27 "EntityLink" */
	void(*UpdateOff)(cEntity *self, float dt);				/* 0x70 vf28 */
} cCamControlVtbl;
SD_OFFSET(cCamControlVtbl, GetCamPos, 0x5c);
SD_OFFSET(cCamControlVtbl, SetTarget, 0x6c);
extern const cCamControlVtbl cCamPathEntity_vftable;
#define CCAMPATHENTITY_VFTABLE (&cCamPathEntity_vftable.base)
extern const cCamControlVtbl cCamControl_vftable;
#define CCAMCONTROL_VFTABLE (&cCamControl_vftable.base)

/* the follow camera adds two */
typedef struct cFollowCamVtbl {
	cCamControlVtbl base;
	void(*GetAng2)(cEntity *self, float *ang);				/* 0x74 vf29 */
	void(*SetAng2)(cEntity *self, const float *ang);			/* 0x78 vf30 */
} cFollowCamVtbl;
SD_OFFSET(cFollowCamVtbl, SetAng2, 0x78);
extern const cFollowCamVtbl cFollowCamEntity_vftable;
#define CFOLLOWCAMENTITY_VFTABLE (&cFollowCamEntity_vftable.base.base)

static inline const cCamControlVtbl *CamCtl_Vtbl(cEntity *e) {
	return (const cCamControlVtbl *)e->vftable;
}

/* engine variables */
extern cGVar GVar_FreeCameraCollide;
#define g_FreeCameraCollide (GVar_FreeCameraCollide.value.i)	/* FreeCameraCollide */
extern cGVar GVar_FullCameraTest;
#define g_FullCameraTest (GVar_FullCameraTest.value.i)	/* FullCameraTest */
extern cGVar GVar_WalkCameraCenter;
#define g_WalkCameraCenter (GVar_WalkCameraCenter.value.f)	/* WalkCameraCenter */
extern cGVar GVar_VehicleCamDist;
#define g_VehicleCamDist (GVar_VehicleCamDist.value.f)
extern cGVar GVar_VehicleCamDist2;
#define g_VehicleCamDist2 (GVar_VehicleCamDist2.value.f)	/* VehicleCamDist2, of player 1 */
extern cGVar GVar_VehicleCamNear;
#define g_VehicleCamNear (GVar_VehicleCamNear.value.f)
extern cGVar GVar_VehicleCamFar;
#define g_VehicleCamFar (GVar_VehicleCamFar.value.f)
extern cGVar GVar_WalkCamNear;
#define g_WalkCamNear (GVar_WalkCamNear.value.f)
extern cGVar GVar_WalkCamFar;
#define g_WalkCamFar (GVar_WalkCamFar.value.f)
extern cGVar GVar_OutWalkCamNear;
#define g_OutWalkCamNear (GVar_OutWalkCamNear.value.f)
extern cGVar GVar_OutWalkCamFar;
#define g_OutWalkCamFar (GVar_OutWalkCamFar.value.f)

/* the degrees of the script variables */
static inline float Cam_Deg2Rad(double v) {
	return (float)((v * M_PI_D) / 180.0);
}

/* ---- CamControl.c ---- */
void cCamControl_dtor(cEntity *self);
cEntity *cCamControl_Delete(cEntity *self, uint32_t flags);
const char *cCamControl_GetClassName(cEntity *self);
byte cCamControl_IsType(cEntity *self, const char *type);
byte cCamControl_VarI(cEntity *self, const char *name, int *i, int set);
byte cCamControl_VarF(cEntity *self, const char *name, double *d, int set);
byte cCamControl_VarS(cEntity *self, const char *name, char **s, int set);
byte cCamControl_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set);
void cCamControl_GetCamPos(cEntity *self, float *x, float *y, float *z);
void cCamControl_SetCamPos(cEntity *self, float x, float y, float z);
/* vf25, vf26 and vf28 do nothing for the plain and the path camera */
void cCamControl_NopF(cEntity *self, float f);
/* the camera node of the "Obj" of that name, 1; else none, 0 */
byte cCamControl_SetTarget(cEntity *self, const char *name);
/* the position of view slot i of the world when the camera shows that view */
void cCamControl_SetViewPos(cEntity *self, int i, float x, float y, float z);

/* ---- CamPathEntity.c ---- */
void cCamPathEntity_dtor(cEntity *self);
cEntity *cCamPathEntity_Delete(cEntity *self, uint32_t flags);
cCamPathPoint *cCamPathPoint_ctor(cCamPathPoint *self);
const char *cCamPathEntity_GetClassName(cEntity *self);
byte cCamPathEntity_IsType(cEntity *self, const char *type);
/* the index NN of name = prefix + two digits, -1 for none or over 32 */
int cCamPathEntity_PointIndex(cEntity *self, const char *name, const char *prefix, uint32_t len);
byte cCamPathEntity_VarI(cEntity *self, const char *name, int *i, int set);
byte cCamPathEntity_VarF(cEntity *self, const char *name, double *d, int set);
byte cCamPathEntity_VarS(cEntity *self, const char *name, char **s, int set);
byte cCamPathEntity_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set);
/* the values of the "Cam" entity the point names */
void cCamPathPoint_Refresh(cCamPathPoint *self);
/* the camera at f (0..1) of the way from cur to next, shaken by the quake of the way */
void cCamPathEntity_Interp(cEntity *self, cCamPathPoint *next, cCamPathPoint *cur, float f);
void cCamPathEntity_Update(cEntity *self);

/* ---- FollowCamEntity.c ---- */
void cFollowCamEntity_dtor(cEntity *self);
cEntity *cFollowCamEntity_Delete(cEntity *self, uint32_t flags);
const char *cFollowCamEntity_GetClassName(cEntity *self);
byte cFollowCamEntity_IsType(cEntity *self, const char *type);
byte cFollowCamEntity_VarI(cEntity *self, const char *name, int *i, int set);
byte cFollowCamEntity_VarF(cEntity *self, const char *name, double *d, int set);
byte cFollowCamEntity_VarS(cEntity *self, const char *name, char **s, int set);
byte cFollowCamEntity_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set);
void cFollowCamEntity_Update(cEntity *self);
void cFollowCamEntity_Lerp(cEntity *self, float f);
void cFollowCamEntity_Slot(cEntity *self, float dt);
byte cFollowCamEntity_SetTargetName(cEntity *self, const char *name);
void cFollowCamEntity_UpdateOff(cEntity *self, float dt);
void cFollowCamEntity_GetAng2(cEntity *self, float *ang);
void cFollowCamEntity_SetAng2(cEntity *self, const float *ang);
/* the random shake of the angles while QuakeTime runs */
void cFollowCamEntity_Quake(cEntity *self, float dt);
/* the three angle pairs from the target (y plus AddTurn) */
void cFollowCamEntity_ResetAngles(cEntity *self);
/* on: no target; off: the MainTarget "Obj" again (free when there is none) */
void cFollowCamEntity_SetFreeCam(cEntity *self, byte on);
/* the camera moved by the input actions of its view slot (sets 1 and 3) */
void cFollowCamEntity_FreeMove(cEntity *self);

#endif
