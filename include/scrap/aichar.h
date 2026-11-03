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

#define CAICHAR_SIZE sizeof(cAIChar)

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

#define CAICHARGROUP_SIZE sizeof(cAICharGroup)

/* a point of a zone (0x1c bytes, new[] of the exe with the count before it) */
typedef struct cAICGPoint {
	Nto1Node link;			/* 0x00 data the point itself, target the character that took it */
	vec3_t pos;			/* 0x10 */
} cAICGPoint;

typedef struct cAICGZone {
	char name[0x80];		/* 0x00 */
	cAICGPoint *points;		/* 0x80 */
	int numPoints;			/* 0x84 */
} cAICGZone;

/* the lists of the dummies while the zones are made (Global/DList.h instances) */
typedef struct cAICGPosNode {
	struct cAICGPosNode *next;	/* 0x00 */
	struct cAICGPosNode *prev;	/* 0x04 */
	vec3_t pos;			/* 0x08 */
} cAICGPosNode;
typedef DLIST(cAICGPosNode) cAICGPosList;

typedef struct cAICGZoneData {
	char name[0x80];		/* 0x00 */
	cAICGPosList points;		/* 0x80 */
} cAICGZoneData;

typedef struct cAICGZoneNode {
	struct cAICGZoneNode *next;	/* 0x00 */
	struct cAICGZoneNode *prev;	/* 0x04 */
	cAICGZoneData zone;		/* 0x08 */
} cAICGZoneNode;
typedef DLIST(cAICGZoneNode) cAICGZoneList;

/* the slots of cAIEntityGroup with the group as self */
struct cAICharGroup {
	union {
		cAIEntityGroup base;			/* 0x00 */
		const cAIEntityGroupVtbl *vftable;
	};
	cAICGZone *zones;		/* 0x60 Mem_Alloc */
	int numZones;			/* 0x64 */
};

/* a point for the repositioning of e out of view between minDist and maxDist from the camera into out; with flag 0 a
 * free point beyond maxDist is taken too, with flag 1 only one the graph does not see from the camera (or in front of
 * it beyond minDist). The point e held before is given up */
byte cAICharGroup_FindRepos(cAICharGroup *self, vec3_t *out, cEntity *e, float minDist, float maxDist,
	byte flag);
/* the action of layer 0 of a character and its name, as the AIs read them (one GetActCtrl each) */
static inline cAction *AIChar_Action0(cEntity *e) {
	return ((cActCtrl *)WithLife_GetActCtrl(e))->layers[0].action;
}

static inline int AIChar_ActionIs(cEntity *e, const char *name) {
	return !strcmp(cActCtrl_GetActionName(WithLife_GetActCtrl(e), 0), name);
}

/* the distance from the character to its target (the positions in this order) as the subclasses sum it: on the floor
 * (x, + 0, z) or in space (y, x, z) */
static inline float AIChar_TargetDistFlat(cAIChar *c) {
	vec3_t a, b;
	cEntity *t = c->base.target.target;
	const vec3_t *op = Entity_GetPos(c->ent, &a);
	const vec3_t *tp = Entity_GetPos(t, &b);
	float dx = tp->x - op->x;
	float dz = tp->z - op->z;

	return (float)__builtin_sqrt((double)(dx * dx + 0.0f + dz * dz));
}

static inline float AIChar_TargetDist(cAIChar *c) {
	vec3_t a, b;
	cEntity *t = c->base.target.target;
	const vec3_t *op = Entity_GetPos(c->ent, &a);
	const vec3_t *tp = Entity_GetPos(t, &b);
	float dy = tp->y - op->y;
	float dx = tp->x - op->x;
	float dz = tp->z - op->z;

	return (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
}

/* the debug text of a demand of a change of state (the name of the state, 0x100 bytes) */
static inline void AIChar_DebugState(cAIChar *c, const char *fmt, int state) {
	char name[0x100];

	AIChar_StateName(c, state, name);
	AI_Debugf(c, fmt, name);
}

/* ---- AI/AIChar.cpp (src/game/AI/IChar.c); the constructor is declared in walkcharentity.h, GetControl in
 * aicharcontrol.h ---- */
void cAIChar_dtor(cAIChar *self);
/* 1 if the vectors differ in a component (or one is NaN) */
int Vec3_Differ(const float *a, const float *b);
void cAIChar_Update(cAI *elem, float dt);
void cAIChar_UpdateOff(cAI *elem, float dt);
void cAIChar_Draw(cAI *base);
void cAIChar_Debugf(cAI *base, const char *fmt, ...);
byte cAIChar_VarI(cAI *elem, const char *name, int *i, byte set);
byte cAIChar_VarF(cAI *elem, const char *name, double *d, byte set);
byte cAIChar_VarS(cAI *elem, const char *name, char **s, byte set);
byte cAIChar_VarV(cAI *elem, const char *name, double *x, double *y, double *z, byte set);
byte cAIChar_GetChanged(cAI *elem);
byte cAIChar_IsPaused(cAI *elem);
byte cAIChar_Event(cAI *base, int ev);
/* the internal state moves the character (and the control is not blocked) */
byte cAIChar_IsMoving(cAI *base);
void cAIChar_CheckParams(cAI *base, int stupidPathfinding);
/* the external state from SetState (the arguments by state, floats as double, bytes as int):
 * STATIC, LOST_PATH*, STATIC_*: none; IN_ROUTE: speed, pauses; CHASE_*, GET_AWAY: cEntity *target, speed;
 * GO_TO: vec3_t dest, vec3_t dir, dist, speed; PATROL_ZONE: vec3_t center, radius, speed, pauses;
 * ORIENT_DINAMIC_*: cEntity *target, orient. 0 if the arguments did not fit */
byte cAIChar_ChangeState(cAI *elem, int state, va_list ap);
byte cAIChar_SetState(cAI *base, int state, ...);
/* the change of state from inside the AI (the group lets the next one through at once) */
byte cAIChar_SetStateIntern(cAI *base, int state, ...);
byte cAIChar_RestoreState(cAI *base, const cAICharSaved *s);
byte cAIChar_RestoreStateIntern(cAI *base, const cAICharSaved *s);
/* the states the default change of state takes (LOST_PATH*, STATIC_*); 0 and an error for the others */
byte cAIChar_ChangeStateMore(cAI *elem, int state, int allowed, va_list ap);
/* a change to the state asks the character group (not STATIC*, LOST_PATH*) */
byte cAIChar_IsGroupState(cAI *base, int state);
/* the internal state (AICI_) and its move; the arguments by state, floats as double, bytes as int:
 * STOPPED, PAUSED_CONTROL, RELAX: none; FINISHING_LINE*: vec3_t dest, dist, speed; INT_CHASE_*: speed;
 * STOPPED_ORIENT_STATIC: vec3_t dir; STOPPED_ORIENT_DINAMIC*: orient; FOLLOW_THE_PATH: vec3_t dir, dist, speed,
 * pathA, pathB; TALK_TO: cEntity *other */
void cAIChar_SetInternStateV(cAI *elem, int state, va_list ap);
void cAIChar_SetInternState(cAI *base, int state, ...);
int cAIChar_GetState(cAI *base);
int cAIChar_GetInternState(cAI *base);
/* the name of the state into buf; buf unchanged for a state out of the range */
void cAIChar_StateName(cAI *base, int state, char *buf);
void cAIChar_GetStateName(cAI *base, char *buf);
void cAIChar_InternStateName(cAI *base, int state, char *buf);
void cAIChar_GetInternStateName(cAI *base, char *buf);
/* the follow of the control goes straight to the target (two slots) */
byte cAIChar_GetDirect(cAI *base);
byte cAIChar_GetDirect2(cAI *base);
cAICharSaved *cAIChar_GetLast(cAI *base);
/* SaveState into s, s returned */
cAICharSaved *cAIChar_SaveTo(cAI *base, cAICharSaved *s);
/* "Repos" on or off; with now also the repositioning at once (0 to DistMaxCheckReposAI) */
byte cAIChar_SetRepos(cAI *elem, byte on, byte now);
byte cAIChar_GetRepos(cAI *elem);
byte cAIChar_IsTalking(cAI *base);
byte cAIChar_CanTalk(cAI *base);
/* the entity the character ran into ("CollideEntity") or NULL */
cEntity *cAIChar_GetCollide(cAI *base);
/* the reaction of the state to its target (gone: event 3); 1 if the state did not change */
byte cAIChar_React(cAI *elem);
/* with action 1 the target must be there and active, else OBJECTIVE_DIED (event 3) and 0 */
byte cAIChar_CheckAction(cAI *base);
/* the character placed out of view between minDist and maxDist from the camera, turned away from it; 0 if the
 * group found no place */
byte cAIChar_Repos(cAI *elem, float minDist, float maxDist, byte flag);
/* a TALK_TO of IN_ROUTE or PATROL_ZONE given up after the repositioning */
void cAIChar_AfterRepos(cAI *base);
/* the distances of the chase and of the action by state */
void cAIChar_SetDists(cAI *base);
void cAIChar_SetDists2(cAI *base);
/* the state kept in last, the change starts */
void cAIChar_BeginChange(cAI *elem, int state);
void cAIChar_SetPauseCheck(cAI *base, byte on);
void cAIChar_SaveState(cAI *elem, cAICharSaved *s);
/* the fields back from s (the target by name, a "WithLife" entity) without a change of state */
void cAIChar_RestoreSaved(cAI *elem, const cAICharSaved *s);
/* paused: the control stopped, the action button up */
void cAIChar_SetPaused(cAI *elem, byte on);
/* action 1: the times of the standing target and of the action button, the distance check every 0.2 s */
void cAIChar_UpdateAction1(cAI *elem);
/* action 0: the action button up */
void cAIChar_UpdateAction0(cAI *base);
/* the action button down within actMin..actMax of the target (seen, priority of the action at most 0x65) */
void cAIChar_CheckActDist(cAI *base);
void cAIChar_SetAction(cAI *base, int action);
/* the action of the frame (dt not used) */
void cAIChar_UpdateMove(cAI *elem, float dt);
/* the reactions to the events of the move by state; 0 if the state does not take the event */
byte cAIChar_EventDefault(cAI *base, int ev);
byte cAIChar_EventInRoute(cAI *elem, int ev);
byte cAIChar_EventPatrol(cAI *elem, int ev);
byte cAIChar_EventGoTo(cAI *elem, int ev);
byte cAIChar_EventGetAway(cAI *elem, int ev);
byte cAIChar_EventChaseBack(cAI *base, int ev);
byte cAIChar_EventLostPath(cAI *elem, int ev);
/* IN_ROUTE and PATROL_ZONE set again (after a talk) */
void cAIChar_Resume(cAI *base);
void cAIChar_UpdateThink(cAI *elem);
/* every second: paused while out of view and far from the camera (pauseCheck) */
void cAIChar_CheckPause(cAI *elem);
/* every second: the repositioning when the camera is in the graph and the character out of view and far */
void cAIChar_CheckRepos(cAI *elem);
byte cAIChar_CanRepos(cAI *base, int state);
/* the think of the state */
void cAIChar_Think(cAI *base);
void cAIChar_ThinkInRoute(cAI *elem);
void cAIChar_ThinkLostPath(cAI *elem);
void cAIChar_ThinkLostPathChase(cAI *elem);
/* LOST_PATH (two copies): back to the kept state once the character is on the graph again */
void cAIChar_ReactLostPath(cAI *elem);
void cAIChar_ReactPatrol(cAI *base);
void cAIChar_ReactInRoute(cAI *base);
void cAIChar_ThinkPatrol(cAI *elem);
/* a talk with the character run into, when both walk (IN_ROUTE or PATROL_ZONE) at about the same height */
void cAIChar_CheckTalk(cAI *base);
/* a looks within 45 degrees at b or b at a */
byte cAIChar_Facing(cAI *elem, cEntity *a, cEntity *b);

#endif
