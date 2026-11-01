/*
 * AI/AIChar.cpp of the game: cAIChar, the AI of a walking character (0x380 bytes, a cAIMove; the struct and the
 * vtable are in include/scrap/ai.h). It owns the move control (cAICharControl 2D or 3D, AI/AICharControl*.cpp) and
 * runs the state machine of SAI.SetStateChar: the external state (STATIC, IN_ROUTE, CHASE_*, GO_TO, GET_AWAY,
 * PATROL_ZONE, ORIENT_*, LOST_PATH*, STATIC_*) and the internal state of the move (STOPPED, PAUSED_CONTROL,
 * FINISHING_LINE*, INT_CHASE_*, STOPPED_ORIENT_*, FOLLOW_THE_PATH, RELAX, TALK_TO). A change of state the character
 * group does not let through yet waits in "pending" (STATIC_CHANGE_STATE meanwhile). Far from the camera the AI
 * pauses, out of view it may be placed near the camera again (Repos). Two characters of IN_ROUTE or PATROL_ZONE
 * that run into each other talk for a while (TALK_TO). The subclasses (cAISentinel, cAIGear, ...) are in
 * AI/AICharTypes; src/game/AI/IChar.c.
 */
#ifndef SCRAP_AICHAR_H
#define SCRAP_AICHAR_H

#include <string.h>

#include "scrap/gvar.h"
#include "scrap/actions.h"
#include "scrap/ai.h"
#include "scrap/aicharcontrol.h"
#include "scrap/dlist.h"

#define CAICHAR_VFTABLE GAME_VFTABLE(cAIChar_vftable)
#define CAICHAR_SIZE 0x380

/* the external states (SAI.SetStateChar, the names of cAIChar_StateName) */
enum {
	AIC_STATIC, AIC_IN_ROUTE, AIC_CHASE_WITH_ACTION, AIC_CHASE_WITHOUT_ACTION, AIC_GO_TO, AIC_GET_AWAY,
	AIC_PATROL_ZONE, AIC_CHASE_WITH_ACTION_BACK, AIC_ORIENT_DYNAMIC_OBJECTIVE, AIC_ORIENT_DYNAMIC_OBJECTIVE_WITH_ACTION,
	AIC_LOST_PATH, AIC_LOST_PATH_CHASE, AIC_STATIC_LOST, AIC_STATIC_OBJ_NOT_ACCESS, AIC_STATIC_CHANGE_STATE,
	AIC_STATE_NONE
};

/* the internal states of the move (cAIChar_InternStateName) */
enum {
	AICI_STOPPED, AICI_PAUSED_CONTROL, AICI_FINISHING_LINE, AICI_FINISHING_LINE_DIRECT, AICI_INT_CHASE_WITH_ACTION,
	AICI_INT_CHASE_WITHOUT_ACTION, AICI_STOPPED_ORIENT_STATIC, AICI_INT_CHASE_WITH_ACTION_BACK,
	AICI_STOPPED_ORIENT_DYNAMIC, AICI_STOPPED_ORIENT_DYNAMIC_WITH_ACTION, AICI_FOLLOW_THE_PATH, AICI_RELAX,
	AICI_TALK_TO, AICI_NONE
};

/* ---- the character group (AI/AICharGroup.cpp, src/game/AI/ICharGroup.c, g_CharGroup): a cAIEntityGroup (a change to
 * a group state waits until the time reached the limit) with the points of the repositioning out of view, the dummies
 * "DM_NPC_Repos_<zone>_..." of the level by zone. The zone is the name of the action class of a character, a point
 * is taken by linking it to the head at 0x4bc of the character ---- */
#define CAICHARGROUP_VFTABLE GAME_VFTABLE(cAICharGroup_vftable)
#define CAICHARGROUP_SIZE 0x68

/* a point of a zone (0x1c bytes, new[] of the exe with the count before it) */
typedef struct cAICGPoint {
	Nto1Node link;			/* 0x00 data the point itself, target the character that took it */
	vec3_t pos;			/* 0x10 */
} cAICGPoint;
SD_SIZE(cAICGPoint, 0x1c);

typedef struct cAICGZone {
	char name[0x80];		/* 0x00 */
	cAICGPoint *points;		/* 0x80 */
	int numPoints;			/* 0x84 */
} cAICGZone;
SD_SIZE(cAICGZone, 0x88);

/* the lists of the dummies while the zones are made (Global/DList.h instances) */
typedef struct cAICGPosNode {
	struct cAICGPosNode *next;	/* 0x00 */
	struct cAICGPosNode *prev;	/* 0x04 */
	vec3_t pos;			/* 0x08 */
} cAICGPosNode;
SD_SIZE(cAICGPosNode, 0x14);
typedef DLIST(cAICGPosNode) cAICGPosList;

typedef struct cAICGZoneData {
	char name[0x80];		/* 0x00 */
	cAICGPosList points;		/* 0x80 */
} cAICGZoneData;
SD_SIZE(cAICGZoneData, 0x98);

typedef struct cAICGZoneNode {
	struct cAICGZoneNode *next;	/* 0x00 */
	struct cAICGZoneNode *prev;	/* 0x04 */
	cAICGZoneData zone;		/* 0x08 */
} cAICGZoneNode;
SD_SIZE(cAICGZoneNode, 0xa0);
typedef DLIST(cAICGZoneNode) cAICGZoneList;

/* the slots of cAIEntityGroup with the group as self */
typedef struct cAICharGroupVtbl {
	cAICharGroup *(*Delete)(cAICharGroup *self, byte flags);	/* 0x00 */
	void *vf01_04[4];
	byte(*CanChange)(cAICharGroup *self);	/* 0x14 */
	void(*ResetChange)(cAICharGroup *self);	/* 0x18 the time to 0 */
	void(*AllowChange)(cAICharGroup *self);	/* 0x1c the time to the limit */
	void(*Update)(cAICharGroup *self, float dt);	/* 0x20 */
} cAICharGroupVtbl;
SD_OFFSET(cAICharGroupVtbl, AllowChange, 0x1c);

struct cAICharGroup {
	union {
		cAIEntityGroup base;			/* 0x00 */
		const cAICharGroupVtbl *vftable;
	};
	cAICGZone *zones;		/* 0x60 Mem_Alloc */
	int numZones;			/* 0x64 */
};
SD_OFFSET(cAICharGroup, zones, 0x60);
SD_SIZE(cAICharGroup, CAICHARGROUP_SIZE);

/* the constructor and GetReposPos (SAI.GetReposPos) are declared in ai.h */
cAICGPoint *cAICGPoint_ctor(cAICGPoint *self);
void cAICGPoint_dtor(cAICGPoint *self);
/* the deleting destructor of a point or (flags 2) of the new[] array it starts */
void *cAICGPoint_Delete(cAICGPoint *self, uint32_t flags);
/* the out of line Nto1Node_Unlink with the CHECK of Node for a node that may be NULL (stdcall) */
void Nto1Node_UnlinkOpt(Nto1Node *node);
void cAICharGroup_dtor(cAICharGroup *self);
cAICharGroup *cAICharGroup_Delete(cAICharGroup *self, byte flags);
/* the zones and their points from the dummies of the level */
void cAICharGroup_Init(cAICharGroup *self);
cAICGZone *cAICharGroup_GetZones(cAICharGroup *self);
int cAICharGroup_NumZones(cAICharGroup *self);
/* a point for the repositioning of e out of view between minDist and maxDist from the camera into out; with flag 0 a
 * free point beyond maxDist is taken too, with flag 1 only one the graph does not see from the camera (or in front of
 * it beyond minDist). The point e held before is given up */
byte cAICharGroup_FindRepos(cAICharGroup *self, vec3_t *out, cEntity *e, float minDist, float maxDist,
	byte flag);
/* the lists of the dummies: emptied (the free nodes kept), the free nodes deleted, both (the destructors) */
void cAICGZoneList_Clear(cAICGZoneList *list);
void cAICGZoneList_FreePool(cAICGZoneList *list);
void cAICGZoneList_dtor(cAICGZoneList *list);
void cAICGPosList_Clear(cAICGPosList *list);
void cAICGZoneData_dtor(cAICGZoneData *zone);
/* the action of layer 0 of a character and its name, as the AIs read them (one GetActCtrl each) */
static inline cAction *AIChar_Action0(cEntity *e) {
	return ((cActCtrl *)WithLife_Vtbl(e)->GetActCtrl(e))->layers[0].action;
}

static inline int AIChar_ActionIs(cEntity *e, const char *name) {
	return !strcmp(cActCtrl_GetActionName(WithLife_Vtbl(e)->GetActCtrl(e), 0), name);
}

/* the distance from the character to its target (the positions in this order) as the subclasses sum it: on the floor
 * (x, + 0, z) or in space (y, x, z) */
static inline float AIChar_TargetDistFlat(cAIChar *c) {
	vec3_t a, b;
	cEntity *t = c->base.target.target;
	const vec3_t *op = c->ent->vftable->GetPos(c->ent, &a);
	const vec3_t *tp = t->vftable->GetPos(t, &b);
	float dx = tp->x - op->x;
	float dz = tp->z - op->z;

	return (float)__builtin_sqrt((double)(dx * dx + 0.0f + dz * dz));
}

static inline float AIChar_TargetDist(cAIChar *c) {
	vec3_t a, b;
	cEntity *t = c->base.target.target;
	const vec3_t *op = c->ent->vftable->GetPos(c->ent, &a);
	const vec3_t *tp = t->vftable->GetPos(t, &b);
	float dy = tp->y - op->y;
	float dx = tp->x - op->x;
	float dz = tp->z - op->z;

	return (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
}

/* the debug text of a demand of a change of state (the name of the state, 0x100 bytes) */
static inline void AIChar_DebugState(cAIChar *c, const char *fmt, int state) {
	char name[0x100];

	c->vftable->StateName(c, state, name);
	c->vftable->Debugf(c, fmt, name);
}

/* the engine variables of the character AI */
extern cGVar GVar_DistMaxCheckPauseAI;
#define g_DistMaxCheckPauseAI (GVar_DistMaxCheckPauseAI.value.f)	/* "DistMaxCheckPauseAI" */
extern cGVar GVar_DistMinCheckPauseAI;
#define g_DistMinCheckPauseAI (GVar_DistMinCheckPauseAI.value.f)	/* "DistMinCheckPauseAI" */
extern cGVar GVar_DistRadPauseCharAI;
#define g_DistRadPauseCharAI (GVar_DistRadPauseCharAI.value.f)		/* "DistRadPauseCharAI" */
extern cGVar GVar_DistMaxCheckReposAI;
#define g_DistMaxCheckReposAI (GVar_DistMaxCheckReposAI.value.f)	/* "DistMaxCheckReposAI" */
extern cGVar GVar_DistMinCheckReposAI;
#define g_DistMinCheckReposAI (GVar_DistMinCheckReposAI.value.f)	/* "DistMinCheckReposAI" */
extern cGVar GVar_MinTalkCharAI;
#define g_MinTalkCharAI (GVar_MinTalkCharAI.value.f)		/* "MinTalkCharAI": the height difference */
extern cGVar GVar_MinNumTalksCharAI;
#define g_MinNumTalksCharAI (GVar_MinNumTalksCharAI.value.i)		/* "MinNumTalksCharAI" */
extern cGVar GVar_MinTimeStoppedCharAI;
#define g_MinTimeStoppedCharAI (GVar_MinTimeStoppedCharAI.value.f)	/* "MinTimeStoppedCharAI" */
extern cGVar GVar_MinTimeStopTempCharAI;
#define g_MinTimeStopTempCharAI (GVar_MinTimeStopTempCharAI.value.f)	/* "MinTimeStopTempCharAI" */

/* ---- AI/AIChar.cpp (src/game/AI/IChar.c); the constructor is declared in walkcharentity.h, GetControl in
 * aicharcontrol.h ---- */
void cAIChar_dtor(cAIChar *self);
cAIChar *cAIChar_Delete(cAIChar *self, byte flags);
/* 1 if the vectors differ in a component (or one is NaN) */
int Vec3_Differ(const float *a, const float *b);
void cAIChar_Nop(cAIChar *self);
byte cAIChar_EventNone(cAIChar *self, int ev);
void cAIChar_Update(cAIChar *self, float dt);
void cAIChar_UpdateOff(cAIChar *self, float dt);
void cAIChar_Draw(cAIChar *self);
void cAIChar_Debugf(cAIChar *self, const char *fmt, ...);
byte cAIChar_VarI(cAIChar *self, const char *name, int *i, byte set);
byte cAIChar_VarF(cAIChar *self, const char *name, double *d, byte set);
byte cAIChar_VarS(cAIChar *self, const char *name, char **s, byte set);
byte cAIChar_VarV(cAIChar *self, const char *name, double *x, double *y, double *z, byte set);
byte cAIChar_GetChanged(cAIChar *self);
byte cAIChar_IsPaused(cAIChar *self);
byte cAIChar_Event(cAIChar *self, int ev);
/* the internal state moves the character (and the control is not blocked) */
byte cAIChar_IsMoving(cAIChar *self);
void cAIChar_CheckParams(cAIChar *self, int stupidPathfinding);
/* the external state from SetState (the arguments by state, floats as double, bytes as int):
 * STATIC, LOST_PATH*, STATIC_*: none; IN_ROUTE: speed, pauses; CHASE_*, GET_AWAY: cEntity *target, speed;
 * GO_TO: vec3_t dest, vec3_t dir, dist, speed; PATROL_ZONE: vec3_t center, radius, speed, pauses;
 * ORIENT_DINAMIC_*: cEntity *target, orient. 0 if the arguments did not fit */
byte cAIChar_ChangeState(cAIChar *self, int state, va_list ap);
byte cAIChar_SetState(cAIChar *self, int state, ...);
/* the change of state from inside the AI (the group lets the next one through at once) */
byte cAIChar_SetStateIntern(cAIChar *self, int state, ...);
byte cAIChar_RestoreState(cAIChar *self, const cAICharSaved *s);
byte cAIChar_RestoreStateIntern(cAIChar *self, const cAICharSaved *s);
/* the states the default change of state takes (LOST_PATH*, STATIC_*); 0 and an error for the others */
byte cAIChar_ChangeStateMore(cAIChar *self, int state, int allowed, va_list ap);
/* a change to the state asks the character group (not STATIC*, LOST_PATH*) */
byte cAIChar_IsGroupState(cAIChar *self, int state);
/* the internal state (AICI_) and its move; the arguments by state, floats as double, bytes as int:
 * STOPPED, PAUSED_CONTROL, RELAX: none; FINISHING_LINE*: vec3_t dest, dist, speed; INT_CHASE_*: speed;
 * STOPPED_ORIENT_STATIC: vec3_t dir; STOPPED_ORIENT_DINAMIC*: orient; FOLLOW_THE_PATH: vec3_t dir, dist, speed,
 * pathA, pathB; TALK_TO: cEntity *other */
void cAIChar_SetInternStateV(cAIChar *self, int state, va_list ap);
void cAIChar_SetInternState(cAIChar *self, int state, ...);
int cAIChar_GetState(cAIChar *self);
int cAIChar_GetInternState(cAIChar *self);
/* the name of the state into buf; buf unchanged for a state out of the range */
void cAIChar_StateName(cAIChar *self, int state, char *buf);
void cAIChar_GetStateName(cAIChar *self, char *buf);
void cAIChar_InternStateName(cAIChar *self, int state, char *buf);
void cAIChar_GetInternStateName(cAIChar *self, char *buf);
/* the follow of the control goes straight to the target (two slots) */
byte cAIChar_GetDirect(cAIChar *self);
byte cAIChar_GetDirect2(cAIChar *self);
cAICharSaved *cAIChar_GetLast(cAIChar *self);
/* SaveState into s, s returned */
cAICharSaved *cAIChar_SaveTo(cAIChar *self, cAICharSaved *s);
/* "Repos" on or off; with now also the repositioning at once (0 to DistMaxCheckReposAI) */
byte cAIChar_SetRepos(cAIChar *self, byte on, byte now);
byte cAIChar_GetRepos(cAIChar *self);
byte cAIChar_IsTalking(cAIChar *self);
byte cAIChar_CanTalk(cAIChar *self);
/* the entity the character ran into ("CollideEntity") or NULL */
cEntity *cAIChar_GetCollide(cAIChar *self);
/* the reaction of the state to its target (gone: event 3); 1 if the state did not change */
byte cAIChar_React(cAIChar *self);
/* with action 1 the target must be there and active, else OBJECTIVE_DIED (event 3) and 0 */
byte cAIChar_CheckAction(cAIChar *self);
/* the character placed out of view between minDist and maxDist from the camera, turned away from it; 0 if the
 * group found no place */
byte cAIChar_Repos(cAIChar *self, float minDist, float maxDist, byte flag);
/* a TALK_TO of IN_ROUTE or PATROL_ZONE given up after the repositioning */
void cAIChar_AfterRepos(cAIChar *self);
/* the distances of the chase and of the action by state */
void cAIChar_SetDists(cAIChar *self);
void cAIChar_SetDists2(cAIChar *self);
/* the state kept in last, the change starts */
void cAIChar_BeginChange(cAIChar *self, int state);
void cAIChar_SetPauseCheck(cAIChar *self, byte on);
void cAIChar_SaveState(cAIChar *self, cAICharSaved *s);
/* the fields back from s (the target by name, a "WithLife" entity) without a change of state */
void cAIChar_RestoreSaved(cAIChar *self, const cAICharSaved *s);
/* paused: the control stopped, the action button up */
void cAIChar_SetPaused(cAIChar *self, byte on);
/* action 1: the times of the standing target and of the action button, the distance check every 0.2 s */
void cAIChar_UpdateAction1(cAIChar *self);
/* action 0: the action button up */
void cAIChar_UpdateAction0(cAIChar *self);
/* the action button down within actMin..actMax of the target (seen, priority of the action at most 0x65) */
void cAIChar_CheckActDist(cAIChar *self);
void cAIChar_SetAction(cAIChar *self, int action);
/* the action of the frame (dt not used) */
void cAIChar_UpdateMove(cAIChar *self, float dt);
/* the reactions to the events of the move by state; 0 if the state does not take the event */
byte cAIChar_EventDefault(cAIChar *self, int ev);
byte cAIChar_EventInRoute(cAIChar *self, int ev);
byte cAIChar_EventPatrol(cAIChar *self, int ev);
byte cAIChar_EventGoTo(cAIChar *self, int ev);
byte cAIChar_EventGetAway(cAIChar *self, int ev);
byte cAIChar_EventChaseBack(cAIChar *self, int ev);
byte cAIChar_EventLostPath(cAIChar *self, int ev);
/* IN_ROUTE and PATROL_ZONE set again (after a talk) */
void cAIChar_Resume(cAIChar *self);
void cAIChar_UpdateThink(cAIChar *self);
/* every second: paused while out of view and far from the camera (pauseCheck) */
void cAIChar_CheckPause(cAIChar *self);
/* every second: the repositioning when the camera is in the graph and the character out of view and far */
void cAIChar_CheckRepos(cAIChar *self);
byte cAIChar_CanRepos(cAIChar *self, int state);
/* the think of the state */
void cAIChar_Think(cAIChar *self);
void cAIChar_ThinkInRoute(cAIChar *self);
void cAIChar_ThinkLostPath(cAIChar *self);
void cAIChar_ThinkLostPathChase(cAIChar *self);
/* LOST_PATH (two copies): back to the kept state once the character is on the graph again */
void cAIChar_ReactLostPath(cAIChar *self);
void cAIChar_ReactPatrol(cAIChar *self);
void cAIChar_ReactInRoute(cAIChar *self);
void cAIChar_ThinkPatrol(cAIChar *self);
/* a talk with the character run into, when both walk (IN_ROUTE or PATROL_ZONE) at about the same height */
void cAIChar_CheckTalk(cAIChar *self);
/* a looks within 45 degrees at b or b at a */
byte cAIChar_Facing(cAIChar *self, cEntity *a, cEntity *b);

#endif
