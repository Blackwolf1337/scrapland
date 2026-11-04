/*
 * The two controllers of the vehicle AI (cAIVehicle, include/scrap/aivehicle.h): cAIVehicleControl
 * (AI/AIVehicleControl.cpp, src/game/AI/IVehicleControl.c) steers the vehicle through its controls (+0x414): the
 * path over the visibility graph to a point, the chase of the target node at a distance, the strafe and the
 * acceleration, the line tests of the corridor of the vehicle; cAIVehicleShootControl (AI/AIVehicleShootControl.cpp,
 * src/game/AI/IVehicleShootControl.c) picks the weapon and the countermeasure of the vehicle and pulls the fire and
 * the ammo buttons. The vehicle group helpers (cAIVehicleGroup) of the same source are at the end.
 * The functions the vehicle AI calls are declared in aivehicle.h (their names stay), the rest here.
 */
#ifndef SCRAP_AIVEHICLECONTROL_H
#define SCRAP_AIVEHICLECONTROL_H

#include "scrap/ai.h"
#include "scrap/controls.h"
#include "scrap/nodesworld.h"
#include "scrap/slotcontroller.h"

#define CAIVEHICLEGROUP_SIZE sizeof(cAIVehicleGroup)

/* the moves of SetMove (cAIVehicleControl.mode) */
enum {
	AIVC_STOP = 0, AIVC_POINT = 1, AIVC_DIRECT = 2, AIVC_CHASE = 3, AIVC_ORIENT = 6, AIVC_PATH = 7
};

typedef struct cAIVehicleControl {
	byte boost;			/* the boost of the vehicle on */
	byte pad_5[3];
	cSlotController slotStrafe;	/* 0.2 s */
	cSlotController slotArrive;	/* 0.2 s */
	cAIVehicle *ai;
	cEntity *vehicle;
	cVehicleControls *ctl;		/* the controls of the vehicle (+0x414) */
	float radius;			/* of the vehicle */
	float dist;			/* the distance that counts as arrived */
	float distMin;			/* dist, at most the radius */
	float dt;
	float angX;			/* the angle left to the point (pitch) */
	float angY;			/* (yaw) */
	cAIVehicleParams2 params;
	cAIVehicleParams2 paramsInit;
	vec3_t dest;			/* the goal of the path */
	int mode;			/* AIVC_ */
	int prevMode;
	float turnX;			/* the turn given to the controls, smoothed */
	float turnY;
	vec3_t targetPos;		/* -FLT_MAX each after ClearDest */
	float rotA;			/* the turn factor */
	float rotB;			/* the most the turn changes in a step */
	int lifeMark;			/* the life of the vehicle + 1 */
	byte direct;			/* the angles set directly (LOST_PATH) */
	byte inertia;
	byte pad_b2[2];
	cSlotController slotPath;	/* 0.2 s */
	vec3_t point;			/* the point steered to */
	cNWVecList path;
	vec3_t *node;			/* the path node steered to */
	vec3_t *prevNode;
	float turnXWant;
	float turnYWant;
	byte braking;
	byte pad_f9[3];
	float accel;
	byte seen;			/* point is a later node of the path that is seen */
	byte pad_101[3];
	float brake;
	byte stopAtGoal;
	byte pad_109[3];
	cSlotController slotChase;	/* 0.2 s while the target is seen */
	cSlotController slotChaseSee;	/* 1/3 s */
	cSlotController slotChasePath;	/* 5 s */
	cSlotController slotChaseNear;	/* 0.2 s */
	cSlotController slotRepath;	/* 3 s while lost */
	Nto1Node *target;		/* the target node of the AI (its target at +8) */
	float chaseMax;
	float chaseMin;
	vec3_t v168;
	byte inSight;			/* the target is seen (chase) */
	byte pad_175[3];
	float strafeTime;
	float strafeNext;
	float strafeX;
	float strafeY;
	byte tooNear;
	byte tooFar;
	byte pad_18a[2];
	int pathState;
	byte lost;			/* the path is blocked, a new one is searched */
	byte unused_191;
	byte strafeOn;
	byte strafing;
	float strafeElapsed;
	float strafeWait;
	byte pathDirect;		/* the direct argument of CNodesWorld_BuildPath */
	byte pathFlat;			/* its flat argument */
	byte pad_19e[2];
	vec3_t dir;			/* the direction of the path (mode 7) */
	cNWNode *pathNode;
	vec3_t corridor[9][2];		/* the segments of the last free TestCorridor */
	vec3_t endCap[2][2];		/* the segments of the last free TestEnd */
} cAIVehicleControl;

/* the weapons of the shoot control: LASER (the hook), VULCAN, DEVASTATOR, ATPC, TESLA, SWARM, INFERNO */
enum {
	AIVS_LASER, AIVS_VULCAN, AIVS_DEVASTATOR, AIVS_ATPC, AIVS_TESLA, AIVS_SWARM, AIVS_INFERNO, AIVS_NUM
};

typedef struct cAIVSWeap {
	int kind;			/* 0, 1, 2 the upgraded vulcan */
	int unused_04;
	int unused_08;
	int weapon;			/* the weapon id, -1 none */
	int upgrade;
	byte unused_14;
	byte avail;			/* the vehicle has slots for it */
	byte pad_16[2];
} cAIVSWeap;

typedef struct cAIVehicleShoot {
	int cadenceMul;			/* 1 */
	int cadenceMulCM;		/* 2 */
	cAIVehicle *ai;
	cSlotController slotFire;	/* 0.2 s */
	cSlotController slotFireCM;	/* 0.2 s */
	cSlotController slotWeap;	/* 0.5 s */
	float dt;
	cEntity *vehicle;
	cVehicleControls *ctl;
	cAIVSWeap weaps[AIVS_NUM];
	cAIVSWeap weap;			/* the weapon in use */
	cAIVSWeap cm;			/* the countermeasure in use */
	int mode;			/* 0 off, 1 fire */
	int modeCM;			/* 0 off, 1 countermeasures, 2 the hook */
	cAIVehicleParams3 params;
	cAIVehicleParams3 paramsInit;
	float fireTime;
	float fireTimeCM;
	float weapTime;
	float weapTimeCM;
	Nto1Node *target;		/* the target node of the AI */
	byte hook;
	byte pad_179[3];
	int lifeMark;
	byte damaged;			/* the vehicle lost life since the last fire step */
	byte pad_181[3];
} cAIVehicleShoot;
/* the class name in the exe (the constructor of cAIVehicle) */
typedef cAIVehicleShoot cAIVehicleShootControl;

/* the dodge of StrafeDodge runs */
byte cAIVehicleControl_GetStrafing(cAIVehicleControl *self);

byte cAIVehicleGroup_ReposReady(cAIVehicleGroup *self);
void cAIVehicleGroup_Draw(cAIVehicleGroup *self);
void cAIVehicleGroup_ResetRepos(cAIVehicleGroup *self);
/* the base class cAIEntityGroup: ai.h */

void cAIVehicleControl_Free(cAIVehicleControl *self);
void cAIVehicleShoot_Free(cAIVehicleShoot *self);

#endif
