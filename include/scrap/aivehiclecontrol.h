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

typedef struct cAIVehicleControlVtbl {
	struct cAIVehicleControl *(*Delete)(struct cAIVehicleControl *self, byte flags);	/* 0x00 */
} cAIVehicleControlVtbl;

/* calls through the slots of cAIVehicleControlVtbl */
static inline struct cAIVehicleControl *AIVC_Delete(void *self, byte flags) {
	return VT(cAIVehicleControlVtbl, self)->Delete(self, flags);
}

typedef struct cAIVehicleControl {
	const cAIVehicleControlVtbl *vftable;	/* 0x000 */
	byte boost;			/* 0x004 the boost of the vehicle on */
	byte pad_5[3];
	cSlotController slotStrafe;	/* 0x008 0.2 s */
	cSlotController slotArrive;	/* 0x018 0.2 s */
	cAIVehicle *ai;			/* 0x028 */
	cEntity *vehicle;		/* 0x02c */
	cVehicleControls *ctl;		/* 0x030 the controls of the vehicle (+0x414) */
	float radius;			/* 0x034 of the vehicle */
	float dist;			/* 0x038 the distance that counts as arrived */
	float distMin;			/* 0x03c dist, at most the radius */
	float dt;			/* 0x040 */
	float angX;			/* 0x044 the angle left to the point (pitch) */
	float angY;			/* 0x048 (yaw) */
	cAIVehicleParams2 params;	/* 0x04c */
	cAIVehicleParams2 paramsInit;	/* 0x064 */
	vec3_t dest;			/* 0x07c the goal of the path */
	int mode;			/* 0x088 AIVC_ */
	int prevMode;			/* 0x08c */
	float turnX;			/* 0x090 the turn given to the controls, smoothed */
	float turnY;			/* 0x094 */
	vec3_t targetPos;		/* 0x098 -FLT_MAX each after ClearDest */
	float rotA;			/* 0x0a4 the turn factor */
	float rotB;			/* 0x0a8 the most the turn changes in a step */
	int lifeMark;			/* 0x0ac the life of the vehicle + 1 */
	byte direct;			/* 0x0b0 the angles set directly (LOST_PATH) */
	byte inertia;			/* 0x0b1 */
	byte pad_b2[2];
	cSlotController slotPath;	/* 0x0b4 0.2 s */
	vec3_t point;			/* 0x0c4 the point steered to */
	cNWVecList path;		/* 0x0d0 */
	vec3_t *node;			/* 0x0e8 the path node steered to */
	vec3_t *prevNode;		/* 0x0ec */
	float turnXWant;		/* 0x0f0 */
	float turnYWant;		/* 0x0f4 */
	byte braking;			/* 0x0f8 */
	byte pad_f9[3];
	float accel;			/* 0x0fc */
	byte seen;			/* 0x100 point is a later node of the path that is seen */
	byte pad_101[3];
	float brake;			/* 0x104 */
	byte stopAtGoal;		/* 0x108 */
	byte pad_109[3];
	cSlotController slotChase;	/* 0x10c 0.2 s while the target is seen */
	cSlotController slotChaseSee;	/* 0x11c 1/3 s */
	cSlotController slotChasePath;	/* 0x12c 5 s */
	cSlotController slotChaseNear;	/* 0x13c 0.2 s */
	cSlotController slotRepath;	/* 0x14c 3 s while lost */
	Nto1Node *target;		/* 0x15c the target node of the AI (its target at +8) */
	float chaseMax;			/* 0x160 */
	float chaseMin;			/* 0x164 */
	vec3_t v168;			/* 0x168 */
	byte inSight;			/* 0x174 the target is seen (chase) */
	byte pad_175[3];
	float strafeTime;		/* 0x178 */
	float strafeNext;		/* 0x17c */
	float strafeX;			/* 0x180 */
	float strafeY;			/* 0x184 */
	byte tooNear;			/* 0x188 */
	byte tooFar;			/* 0x189 */
	byte pad_18a[2];
	int pathState;			/* 0x18c */
	byte lost;			/* 0x190 the path is blocked, a new one is searched */
	byte unused_191;			/* 0x191 */
	byte strafeOn;			/* 0x192 */
	byte strafing;			/* 0x193 */
	float strafeElapsed;		/* 0x194 */
	float strafeWait;		/* 0x198 */
	byte pathDirect;		/* 0x19c the direct argument of CNodesWorld_BuildPath */
	byte pathFlat;			/* 0x19d its flat argument */
	byte pad_19e[2];
	vec3_t dir;			/* 0x1a0 the direction of the path (mode 7) */
	cNWNode *pathNode;		/* 0x1ac */
	vec3_t corridor[9][2];		/* 0x1b0 the segments of the last free TestCorridor */
	vec3_t endCap[2][2];		/* 0x288 the segments of the last free TestEnd */
} cAIVehicleControl;

/* the weapons of the shoot control: LASER (the hook), VULCAN, DEVASTATOR, ATPC, TESLA, SWARM, INFERNO */
enum {
	AIVS_LASER, AIVS_VULCAN, AIVS_DEVASTATOR, AIVS_ATPC, AIVS_TESLA, AIVS_SWARM, AIVS_INFERNO, AIVS_NUM
};

typedef struct cAIVSWeap {
	int kind;			/* 0x00 0, 1, 2 the upgraded vulcan */
	int unused_04;			/* 0x04 */
	int unused_08;			/* 0x08 */
	int weapon;			/* 0x0c the weapon id, -1 none */
	int upgrade;			/* 0x10 */
	byte unused_14;			/* 0x14 */
	byte avail;			/* 0x15 the vehicle has slots for it */
	byte pad_16[2];
} cAIVSWeap;

typedef struct cAIVehicleShootVtbl {
	struct cAIVehicleShoot *(*Delete)(struct cAIVehicleShoot *self, byte flags);	/* 0x00 */
} cAIVehicleShootVtbl;

/* calls through the slots of cAIVehicleShootVtbl */
static inline struct cAIVehicleShoot *AIVShoot_Delete(void *self, byte flags) {
	return VT(cAIVehicleShootVtbl, self)->Delete(self, flags);
}

typedef struct cAIVehicleShoot {
	const cAIVehicleShootVtbl *vftable;	/* 0x000 */
	int cadenceMul;			/* 0x004 1 */
	int cadenceMulCM;		/* 0x008 2 */
	cAIVehicle *ai;			/* 0x00c */
	cSlotController slotFire;	/* 0x010 0.2 s */
	cSlotController slotFireCM;	/* 0x020 0.2 s */
	cSlotController slotWeap;	/* 0x030 0.5 s */
	float dt;			/* 0x040 */
	cEntity *vehicle;		/* 0x044 */
	cVehicleControls *ctl;		/* 0x048 */
	cAIVSWeap weaps[AIVS_NUM];	/* 0x04c */
	cAIVSWeap weap;			/* 0x0f4 the weapon in use */
	cAIVSWeap cm;			/* 0x10c the countermeasure in use */
	int mode;			/* 0x124 0 off, 1 fire */
	int modeCM;			/* 0x128 0 off, 1 countermeasures, 2 the hook */
	cAIVehicleParams3 params;	/* 0x12c */
	cAIVehicleParams3 paramsInit;	/* 0x148 */
	float fireTime;			/* 0x164 */
	float fireTimeCM;		/* 0x168 */
	float weapTime;			/* 0x16c */
	float weapTimeCM;		/* 0x170 */
	Nto1Node *target;		/* 0x174 the target node of the AI */
	byte hook;			/* 0x178 */
	byte pad_179[3];
	int lifeMark;			/* 0x17c */
	byte damaged;			/* 0x180 the vehicle lost life since the last fire step */
	byte pad_181[3];
} cAIVehicleShoot;
/* the class name in the exe (0x184 bytes, the constructor of cAIVehicle) */
typedef cAIVehicleShoot cAIVehicleShootControl;

/* the dodge of StrafeDodge runs */
byte cAIVehicleControl_GetStrafing(cAIVehicleControl *self);

byte cAIVehicleGroup_ReposReady(cAIVehicleGroup *self);
void cAIVehicleGroup_Draw(cAIVehicleGroup *self);
void cAIVehicleGroup_ResetRepos(cAIVehicleGroup *self);
/* the base class cAIEntityGroup: ai.h */

#endif
