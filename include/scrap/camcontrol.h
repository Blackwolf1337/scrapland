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
	cEntity base;
	CGameCamera cam;		/* "camPos", "camAng" (rot), "clipNear", "clipFar", "Fov" */
	int slot;			/* the view it shows ("UsrControl"), -1 for none */
	char link[0x80];		/* "EntityLink": the object whose camera node it uses, "" for none */
} cCamControl;

/* a point of the path ("NameNN" variables of the CamPath, NN 00 to 32: the index 32 is past the array) */
typedef struct cCamPathPoint {
	char cam[0x80];			/* "TgtCamNN": the "Cam" entity the point copies when the path reaches it */
	vec3_t pos;			/* "PosNN" */
	vec3_t ang;			/* "AngNN" (degrees outside) */
	float zNear;			/* "NearNN" 10 */
	float zFar;			/* "FarNN" 40000 */
	float fov;			/* "FOVNN" pi/2 */
	float time;			/* "TimeNN" 1: the seconds to the next point */
	byte fadeIn;			/* "FadeInNN" 1: the move starts slow */
	byte fadeOut;			/* "FadeOutNN" 1: the move ends slow */
	byte pad_aa[2];
	float quake;			/* "QuakeNN" 0: the random shake of the angles */
} cCamPathPoint;

#define CAMPATH_POINTS 32

typedef struct cCamPathEntity {
	cCamControl base;
	cPy_Function onEnd;		/* "OnEnd" (name), once at the end */
	float time;			/* "Time" since the start */
	int numPoints;			/* "nWayPoint" */
	cCamPathPoint points[CAMPATH_POINTS];
} cCamPathEntity;

typedef struct cFollowCamEntity {
	cCamControl base;
	cEntity *target;		/* "Target": an "Obj", its cam field points back */
	byte freeCam;			/* "FreeCam": no target, the input of the slot moves the camera */
	char mainTarget[0x80];		/* "MainTarget": the target when FreeCam goes off */
	byte pad_1c1[3];
	float hSpeedTo;			/* 5: HSpeed approaches it */
	float vSpeedTo;			/* 5 */
	float hSpeedRate;		/* 2 per second */
	float vSpeedRate;		/* 2 */
	float retSpeed;			/* "RetSpd" 1000: the distance comes back this fast */
	float prevAng[2];		/* the angles (x, y) of the slot before */
	float curAng[2];		/* of the last slot */
	float ang[2];			/* the angles lerped between them */
	float dist;			/* the distance now, shorter where the world is in the way */
	vec3_t center;			/* the point it looks at */
	int slowFrames;			/* frames over 1/15 s, counted down again */
	float tiltTo;			/* the walk tilt approaches it (WalkCameraCenter) */
	float tilt;
	float addTurn;			/* "AddTurn" */
	float addTurnSpeed;		/* "AddTurnSpeed" */
	float camDist;			/* "CamDist" VehicleCamDist */
	float camAng;			/* "CamAng" -15 degrees */
	float camRot;			/* "CamRot" */
	float quakeTime;		/* "QuakeTime" */
	float quakeFactor;		/* "QuakeFactor" */
	float hSpeed;			/* "HSpeed" 5 */
	float vSpeed;			/* "VSpeed" 5 */
	cPy_Function onSwitch;		/* "OnSwitch" (name, slot), on every target change */
} cFollowCamEntity;

/* the slots of the camera vtables after the 23 of cEntity */
typedef struct cCamControlVtbl {
	cEntityVtbl base;
	void(*GetCamPos)(cEntity *self, float *x, float *y, float *z);
	void(*SetCamPos)(cEntity *self, float x, float y, float z);
	void(*Lerp)(cEntity *self, float f);				/* between two slots */
	void(*Slot)(cEntity *self, float dt);
	byte(*SetTarget)(cEntity *self, const char *name);			/* "EntityLink" */
	void(*UpdateOff)(cEntity *self, float dt);
} cCamControlVtbl;

/* calls through the slots of cCamControlVtbl */
static inline void CamCtl_GetCamPos(void *self, float *x, float *y, float *z) {
	VT(cCamControlVtbl, self)->GetCamPos(self, x, y, z);
}

static inline void CamCtl_Lerp(void *self, float f) {
	VT(cCamControlVtbl, self)->Lerp(self, f);
}

static inline byte CamCtl_SetTarget(void *self, const char *name) {
	return VT(cCamControlVtbl, self)->SetTarget(self, name);
}

static inline void CamCtl_Slot(void *self, float dt) {
	VT(cCamControlVtbl, self)->Slot(self, dt);
}

static inline void CamCtl_UpdateOff(void *self, float dt) {
	VT(cCamControlVtbl, self)->UpdateOff(self, dt);
}

extern cGVar GVar_WalkCameraCenter;
#define g_WalkCameraCenter (GVar_WalkCameraCenter.value.f)	/* WalkCameraCenter */
extern cGVar GVar_VehicleCamDist;
#define g_VehicleCamDist (GVar_VehicleCamDist.value.f)
extern cGVar GVar_VehicleCamDist2;
#define g_VehicleCamDist2 (GVar_VehicleCamDist2.value.f)	/* VehicleCamDist2, of player 1 */

/* the degrees of the script variables */
static inline float Cam_Deg2Rad(double v) {
	return (float)((v * M_PI_D) / 180.0);
}

/* ---- CamControl.c ---- */
void cCamControl_dtor(cEntity *self);
byte cCamControl_IsType(cEntity *self, const char *type);
byte cCamControl_VarI(cEntity *self, const char *name, int *i, int set);
byte cCamControl_VarF(cEntity *self, const char *name, double *d, int set);
byte cCamControl_VarS(cEntity *self, const char *name, char **s, int set);
byte cCamControl_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set);
void cCamControl_GetCamPos(cEntity *self, float *x, float *y, float *z);
void cCamControl_SetCamPos(cEntity *self, float x, float y, float z);
/* and do nothing for the plain and the path camera */
void cCamControl_NopF(cEntity *self, float f);
/* the camera node of the "Obj" of that name, 1; else none, 0 */
byte cCamControl_SetTarget(cEntity *self, const char *name);
/* the position of view slot i of the world when the camera shows that view */
void cCamControl_SetViewPos(cEntity *self, int i, float x, float y, float z);

#endif
