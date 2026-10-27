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

#include "scrap/objentity.h"
#include "scrap/scrapcamera.h"
#include "scrap/world.h"

#define TYEN_CAM "Cam"
#define TYEN_CAMPATH "CamPath"
#define TYEN_FCAM "FCam"

#define CCAMCONTROL_VFTABLE 0x3acbdc
#define CCAMPATHENTITY_VFTABLE 0x3acc9c
#define CFOLLOWCAMENTITY_VFTABLE 0x3acfa0
#define CCAMCONTROL_SIZE 0x13c
#define CCAMPATHENTITY_SIZE 0x1758
#define CFOLLOWCAMENTITY_SIZE 0x244

/* the strings of the exe the camera functions hand out or compare */
#define CAM_CLASSNAME ((const char *)Game_Ptr(0x3acc54))	/* "Cam" */
#define CAMPATH_CLASSNAME ((const char *)Game_Ptr(0x3acd10))	/* "CamPath" */
#define FCAM_CLASSNAME ((const char *)Game_Ptr(0x3a8de0))	/* "FCam" */
#define CAM_EXE_EMPTY ((char *)Game_Ptr(0x3a3228))		/* "" of the exe */
#define FCAM_ONSWITCH_DESC ((char *)Game_Ptr(0x3ad078))		/* "<OnSwtich func>" */

typedef struct cCamControl {
	cEntity base;			/* 0x000 */
	CGameCamera cam;		/* 0x07c "camPos", "camAng" (rot), "clipNear", "clipFar", "Fov" */
	int slot;			/* 0x0b8 the view it shows ("UsrControl"), -1 for none */
	char link[0x80];		/* 0x0bc "EntityLink": the object whose camera node it uses, "" for none */
} cCamControl;
SD_OFFSET(cCamControl, cam, 0x7c);
SD_OFFSET(cCamControl, slot, 0xb8);
SD_OFFSET(cCamControl, link, 0xbc);
SD_SIZE(cCamControl, CCAMCONTROL_SIZE);

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
SD_SIZE(cCamPathEntity, CCAMPATHENTITY_SIZE);

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
SD_SIZE(cFollowCamEntity, CFOLLOWCAMENTITY_SIZE);

/* the slots of the camera vtables after the 23 of cEntity */
typedef struct cCamControlVtbl {
	cEntityVtbl base;
	void(__thiscall *GetCamPos)(cEntity *self, float *x, float *y, float *z);	/* 0x5c vf23 */
	void(__thiscall *SetCamPos)(cEntity *self, float x, float y, float z);		/* 0x60 vf24 */
	void(__thiscall *Lerp)(cEntity *self, float f);				/* 0x64 vf25 between two slots */
	void(__thiscall *Slot)(cEntity *self, float dt);				/* 0x68 vf26 */
	byte(__thiscall *SetTarget)(cEntity *self, const char *name);			/* 0x6c vf27 "EntityLink" */
	void(__thiscall *UpdateOff)(cEntity *self, float dt);				/* 0x70 vf28 */
	void(__thiscall *GetAng2)(cEntity *self, float *ang);				/* 0x74 vf29 FCam only */
	void(__thiscall *SetAng2)(cEntity *self, const float *ang);			/* 0x78 vf30 FCam only */
} cCamControlVtbl;
SD_OFFSET(cCamControlVtbl, GetCamPos, 0x5c);
SD_OFFSET(cCamControlVtbl, SetTarget, 0x6c);
SD_OFFSET(cCamControlVtbl, SetAng2, 0x78);

static inline const cCamControlVtbl *CamCtl_Vtbl(cEntity *e) {
	return (const cCamControlVtbl *)e->vftable;
}

/* engine variables */
#define g_FreeCameraCollide GAME_DATA(int, 0x445ef0)	/* FreeCameraCollide */
#define g_FullCameraTest GAME_DATA(int, 0x445ea8)	/* FullCameraTest */
#define g_WalkCameraCenter GAME_DATA(float, 0x4472b4)	/* WalkCameraCenter */
#define g_VehicleCamDist GAME_DATA(float, 0x445f08)
#define g_VehicleCamNear GAME_DATA(float, 0x445f20)
#define g_VehicleCamFar GAME_DATA(float, 0x445ec0)
#define g_WalkCamNear GAME_DATA(float, 0x445f80)
#define g_WalkCamFar GAME_DATA(float, 0x445f38)
#define g_OutWalkCamNear GAME_DATA(float, 0x445f68)
#define g_OutWalkCamFar GAME_DATA(float, 0x445f50)

/* the degrees of the script variables */
static inline float Cam_Deg2Rad(double v) {
	return (float)((v * M_PI_D) / 180.0);
}

static inline double Cam_Rad2Deg(float v) {
	return ((double)v * 180.0) / M_PI_D;
}

/* ---- CamControl.c ---- */
void __fastcall cCamControl_dtor(cEntity *self);
cEntity *__thiscall cCamControl_Delete(cEntity *self, uint32_t flags);
const char *__thiscall cCamControl_GetClassName(cEntity *self);
int __thiscall cCamControl_IsType(cEntity *self, const char *type);
byte __thiscall cCamControl_VarI(cEntity *self, const char *name, int *i, int set);
byte __thiscall cCamControl_VarF(cEntity *self, const char *name, double *d, int set);
byte __thiscall cCamControl_VarS(cEntity *self, const char *name, char **s, int set);
byte __thiscall cCamControl_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set);
void __thiscall cCamControl_GetCamPos(cEntity *self, float *x, float *y, float *z);
void __thiscall cCamControl_SetCamPos(cEntity *self, float x, float y, float z);
/* vf25, vf26 and vf28 do nothing for the plain and the path camera */
void __thiscall cCamControl_NopF(cEntity *self, float f);
/* the camera node of the "Obj" of that name, 1; else none, 0 */
byte __thiscall cCamControl_SetTarget(cEntity *self, const char *name);
/* the position of view slot i of the world when the camera shows that view */
void __thiscall cCamControl_SetViewPos(cEntity *self, int i, float x, float y, float z);

/* ---- CamPathEntity.c ---- */
void __fastcall cCamPathEntity_dtor(cEntity *self);
cEntity *__thiscall cCamPathEntity_Delete(cEntity *self, uint32_t flags);
cCamPathPoint *__fastcall cCamPathPoint_ctor(cCamPathPoint *self);
const char *__thiscall cCamPathEntity_GetClassName(cEntity *self);
int __thiscall cCamPathEntity_IsType(cEntity *self, const char *type);
/* the index NN of name = prefix + two digits, -1 for none or over 32 */
int __thiscall cCamPathEntity_PointIndex(cEntity *self, const char *name, const char *prefix, uint32_t len);
byte __thiscall cCamPathEntity_VarI(cEntity *self, const char *name, int *i, int set);
byte __thiscall cCamPathEntity_VarF(cEntity *self, const char *name, double *d, int set);
byte __thiscall cCamPathEntity_VarS(cEntity *self, const char *name, char **s, int set);
byte __thiscall cCamPathEntity_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set);
/* the values of the "Cam" entity the point names */
void __fastcall cCamPathPoint_Refresh(cCamPathPoint *self);
/* the camera at f (0..1) of the way from cur to next, shaken by the quake of the way */
void __thiscall cCamPathEntity_Interp(cEntity *self, cCamPathPoint *next, cCamPathPoint *cur, float f);
void __thiscall cCamPathEntity_Update(cEntity *self);

/* ---- FollowCamEntity.c ---- */
void __fastcall cFollowCamEntity_dtor(cEntity *self);
cEntity *__thiscall cFollowCamEntity_Delete(cEntity *self, uint32_t flags);
const char *__thiscall cFollowCamEntity_GetClassName(cEntity *self);
int __thiscall cFollowCamEntity_IsType(cEntity *self, const char *type);
byte __thiscall cFollowCamEntity_VarI(cEntity *self, const char *name, int *i, int set);
byte __thiscall cFollowCamEntity_VarF(cEntity *self, const char *name, double *d, int set);
byte __thiscall cFollowCamEntity_VarS(cEntity *self, const char *name, char **s, int set);
byte __thiscall cFollowCamEntity_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set);
void __thiscall cFollowCamEntity_Update(cEntity *self);
void __thiscall cFollowCamEntity_Lerp(cEntity *self, float f);
void __thiscall cFollowCamEntity_Slot(cEntity *self, float dt);
byte __thiscall cFollowCamEntity_SetTargetName(cEntity *self, const char *name);
void __thiscall cFollowCamEntity_UpdateOff(cEntity *self, float dt);
void __thiscall cFollowCamEntity_GetAng2(cEntity *self, float *ang);
void __thiscall cFollowCamEntity_SetAng2(cEntity *self, const float *ang);
/* the random shake of the angles while QuakeTime runs */
void __thiscall cFollowCamEntity_Quake(cEntity *self, float dt);
/* the three angle pairs from the target (y plus AddTurn) */
void __fastcall cFollowCamEntity_ResetAngles(cEntity *self);
/* on: no target; off: the MainTarget "Obj" again (free when there is none) */
void __thiscall cFollowCamEntity_SetFreeCam(cEntity *self, byte on);
/* the camera moved by the input actions of its view slot (sets 1 and 3) */
void __fastcall cFollowCamEntity_FreeMove(cEntity *self);

#endif
