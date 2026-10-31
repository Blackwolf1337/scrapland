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

#include "scrap/actions.h"
#include "scrap/ai.h"
#include "scrap/aicharcontrol.h"
#include "scrap/dlist.h"

#define CAICHAR_VFTABLE GAME_VFTABLE(cAIChar_vftable, 0x3a49b4)
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
#define CAICHARGROUP_VFTABLE GAME_VFTABLE(cAICharGroup_vftable, 0x3a5f24)
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
	cAICharGroup *(__thiscall *Delete)(cAICharGroup *self, byte flags);	/* 0x00 */
	void *vf01_04[4];
	byte(__thiscall *CanChange)(cAICharGroup *self);	/* 0x14 */
	void(__thiscall *ResetChange)(cAICharGroup *self);	/* 0x18 the time to 0 */
	void(__thiscall *AllowChange)(cAICharGroup *self);	/* 0x1c the time to the limit */
	void(__thiscall *Update)(cAICharGroup *self, float dt);	/* 0x20 */
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
cAICGPoint *__fastcall cAICGPoint_ctor(cAICGPoint *self);
void __fastcall cAICGPoint_dtor(cAICGPoint *self);
/* the deleting destructor of a point or (flags 2) of the new[] array it starts */
void *__thiscall cAICGPoint_Delete(cAICGPoint *self, uint32_t flags);
/* the out of line Nto1Node_Unlink with the CHECK of Node for a node that may be NULL (stdcall) */
void __stdcall Nto1Node_UnlinkOpt(Nto1Node *node);
void __fastcall cAICharGroup_dtor(cAICharGroup *self);
cAICharGroup *__thiscall cAICharGroup_Delete(cAICharGroup *self, byte flags);
/* the zones and their points from the dummies of the level */
void __thiscall cAICharGroup_Init(cAICharGroup *self);
cAICGZone *__thiscall cAICharGroup_GetZones(cAICharGroup *self);
int __thiscall cAICharGroup_NumZones(cAICharGroup *self);
/* a point for the repositioning of e out of view between minDist and maxDist from the camera into out; with flag 0 a
 * free point beyond maxDist is taken too, with flag 1 only one the graph does not see from the camera (or in front of
 * it beyond minDist). The point e held before is given up (rva 0x80ee0) */
byte __thiscall cAICharGroup_FindRepos(cAICharGroup *self, vec3_t *out, cEntity *e, float minDist, float maxDist,
	byte flag);
/* the lists of the dummies: emptied (the free nodes kept), the free nodes deleted, both (the destructors) */
void __fastcall cAICGZoneList_Clear(cAICGZoneList *list);
void __fastcall cAICGZoneList_FreePool(cAICGZoneList *list);
void __fastcall cAICGZoneList_dtor(cAICGZoneList *list);
void __fastcall cAICGPosList_Clear(cAICGPosList *list);
void __fastcall cAICGZoneData_dtor(cAICGZoneData *zone);
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
	cEntity *t = c->target.target;
	const vec3_t *op = c->ent->vftable->GetPos(c->ent, &a);
	const vec3_t *tp = t->vftable->GetPos(t, &b);
	float dx = tp->x - op->x;
	float dz = tp->z - op->z;

	return (float)__builtin_sqrt((double)(dx * dx + 0.0f + dz * dz));
}

static inline float AIChar_TargetDist(cAIChar *c) {
	vec3_t a, b;
	cEntity *t = c->target.target;
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
#define g_DistMaxCheckPauseAI GAME_GLOBAL_IN(float, GVar_DistMaxCheckPauseAI, 16, 0x4442b8)	/* "DistMaxCheckPauseAI" */
#define g_DistMinCheckPauseAI GAME_GLOBAL_IN(float, GVar_DistMinCheckPauseAI, 16, 0x444228)	/* "DistMinCheckPauseAI" */
#define g_DistRadPauseCharAI GAME_GLOBAL_IN(float, GVar_DistRadPauseCharAI, 16, 0x444240)		/* "DistRadPauseCharAI" */
#define g_DistMaxCheckReposAI GAME_GLOBAL_IN(float, GVar_DistMaxCheckReposAI, 16, 0x444348)	/* "DistMaxCheckReposAI" */
#define g_DistMinCheckReposAI GAME_GLOBAL_IN(float, GVar_DistMinCheckReposAI, 16, 0x444330)	/* "DistMinCheckReposAI" */
#define g_MinTalkCharAI GAME_GLOBAL_IN(float, GVar_MinTalkCharAI, 16, 0x4442a0)		/* "MinTalkCharAI": the height difference */
#define g_MinNumTalksCharAI GAME_GLOBAL_IN(int, GVar_MinNumTalksCharAI, 16, 0x444258)		/* "MinNumTalksCharAI" */
#define g_MinTimeStoppedCharAI GAME_GLOBAL_IN(float, GVar_MinTimeStoppedCharAI, 16, 0x444270)	/* "MinTimeStoppedCharAI" */
#define g_MinTimeStopTempCharAI GAME_GLOBAL_IN(float, GVar_MinTimeStopTempCharAI, 16, 0x444318)	/* "MinTimeStopTempCharAI" */

/* ---- AI/AIChar.cpp (src/game/AI/IChar.c); the constructor is declared in walkcharentity.h, GetControl in
 * aicharcontrol.h ---- */
void __fastcall cAIChar_dtor(cAIChar *self);
cAIChar *__thiscall cAIChar_Delete(cAIChar *self, byte flags);
/* 1 if the vectors differ in a component (or one is NaN) */
int Vec3_Differ(const float *a, const float *b);
void __thiscall cAIChar_Nop(cAIChar *self);
byte __thiscall cAIChar_EventNone(cAIChar *self, int ev);
void __thiscall cAIChar_Update(cAIChar *self, float dt);
void __thiscall cAIChar_UpdateOff(cAIChar *self, float dt);
void __thiscall cAIChar_Draw(cAIChar *self);
void cAIChar_Debugf(cAIChar *self, const char *fmt, ...);
byte __thiscall cAIChar_VarI(cAIChar *self, const char *name, int *i, byte set);
byte __thiscall cAIChar_VarF(cAIChar *self, const char *name, double *d, byte set);
byte __thiscall cAIChar_VarS(cAIChar *self, const char *name, char **s, byte set);
byte __thiscall cAIChar_VarV(cAIChar *self, const char *name, double *x, double *y, double *z, byte set);
byte __thiscall cAIChar_GetChanged(cAIChar *self);
byte __thiscall cAIChar_IsPaused(cAIChar *self);
byte __thiscall cAIChar_Event(cAIChar *self, int ev);
/* the internal state moves the character (and the control is not blocked) */
byte __thiscall cAIChar_IsMoving(cAIChar *self);
void __thiscall cAIChar_CheckParams(cAIChar *self, int stupidPathfinding);
/* the external state from SetState (the arguments by state, floats as double, bytes as int):
 * STATIC, LOST_PATH*, STATIC_*: none; IN_ROUTE: speed, pauses; CHASE_*, GET_AWAY: cEntity *target, speed;
 * GO_TO: vec3_t dest, vec3_t dir, dist, speed; PATROL_ZONE: vec3_t center, radius, speed, pauses;
 * ORIENT_DINAMIC_*: cEntity *target, orient. 0 if the arguments did not fit */
byte __thiscall cAIChar_ChangeState(cAIChar *self, int state, va_list ap);
byte cAIChar_SetState(cAIChar *self, int state, ...);
/* the change of state from inside the AI (the group lets the next one through at once) */
byte cAIChar_SetStateIntern(cAIChar *self, int state, ...);
byte __thiscall cAIChar_RestoreState(cAIChar *self, const cAICharSaved *s);
byte __thiscall cAIChar_RestoreStateIntern(cAIChar *self, const cAICharSaved *s);
/* the states the default change of state takes (LOST_PATH*, STATIC_*); 0 and an error for the others */
byte __thiscall cAIChar_ChangeStateMore(cAIChar *self, int state, int allowed, va_list ap);
/* a change to the state asks the character group (not STATIC*, LOST_PATH*) */
byte __thiscall cAIChar_IsGroupState(cAIChar *self, int state);
/* the internal state (AICI_) and its move; the arguments by state, floats as double, bytes as int:
 * STOPPED, PAUSED_CONTROL, RELAX: none; FINISHING_LINE*: vec3_t dest, dist, speed; INT_CHASE_*: speed;
 * STOPPED_ORIENT_STATIC: vec3_t dir; STOPPED_ORIENT_DINAMIC*: orient; FOLLOW_THE_PATH: vec3_t dir, dist, speed,
 * pathA, pathB; TALK_TO: cEntity *other */
void __thiscall cAIChar_SetInternStateV(cAIChar *self, int state, va_list ap);
void cAIChar_SetInternState(cAIChar *self, int state, ...);
int __thiscall cAIChar_GetState(cAIChar *self);
int __thiscall cAIChar_GetInternState(cAIChar *self);
/* the name of the state into buf; buf unchanged for a state out of the range */
void __thiscall cAIChar_StateName(cAIChar *self, int state, char *buf);
void __thiscall cAIChar_GetStateName(cAIChar *self, char *buf);
void __thiscall cAIChar_InternStateName(cAIChar *self, int state, char *buf);
void __thiscall cAIChar_GetInternStateName(cAIChar *self, char *buf);
/* the follow of the control goes straight to the target (two slots) */
byte __thiscall cAIChar_GetDirect(cAIChar *self);
byte __thiscall cAIChar_GetDirect2(cAIChar *self);
cAICharSaved *__thiscall cAIChar_GetLast(cAIChar *self);
/* SaveState into s, s returned */
cAICharSaved *__thiscall cAIChar_SaveTo(cAIChar *self, cAICharSaved *s);
/* "Repos" on or off; with now also the repositioning at once (0 to DistMaxCheckReposAI) */
byte __thiscall cAIChar_SetRepos(cAIChar *self, byte on, byte now);
byte __thiscall cAIChar_GetRepos(cAIChar *self);
byte __thiscall cAIChar_IsTalking(cAIChar *self);
byte __thiscall cAIChar_CanTalk(cAIChar *self);
/* the entity the character ran into ("CollideEntity") or NULL */
cEntity *__thiscall cAIChar_GetCollide(cAIChar *self);
/* the reaction of the state to its target (gone: event 3); 1 if the state did not change */
byte __thiscall cAIChar_React(cAIChar *self);
/* with action 1 the target must be there and active, else OBJECTIVE_DIED (event 3) and 0 */
byte __thiscall cAIChar_CheckAction(cAIChar *self);
/* the character placed out of view between minDist and maxDist from the camera, turned away from it; 0 if the
 * group found no place */
byte __thiscall cAIChar_Repos(cAIChar *self, float minDist, float maxDist, byte flag);
/* a TALK_TO of IN_ROUTE or PATROL_ZONE given up after the repositioning */
void __thiscall cAIChar_AfterRepos(cAIChar *self);
/* the distances of the chase and of the action by state */
void __thiscall cAIChar_SetDists(cAIChar *self);
void __thiscall cAIChar_SetDists2(cAIChar *self);
/* the state kept in last, the change starts */
void __thiscall cAIChar_BeginChange(cAIChar *self, int state);
void __thiscall cAIChar_SetPauseCheck(cAIChar *self, byte on);
void __thiscall cAIChar_SaveState(cAIChar *self, cAICharSaved *s);
/* the fields back from s (the target by name, a "WithLife" entity) without a change of state */
void __thiscall cAIChar_RestoreSaved(cAIChar *self, const cAICharSaved *s);
/* paused: the control stopped, the action button up */
void __thiscall cAIChar_SetPaused(cAIChar *self, byte on);
/* action 1: the times of the standing target and of the action button, the distance check every 0.2 s */
void __thiscall cAIChar_UpdateAction1(cAIChar *self);
/* action 0: the action button up */
void __thiscall cAIChar_UpdateAction0(cAIChar *self);
/* the action button down within actMin..actMax of the target (seen, priority of the action at most 0x65) */
void __thiscall cAIChar_CheckActDist(cAIChar *self);
void __thiscall cAIChar_SetAction(cAIChar *self, int action);
/* the action of the frame (dt not used) */
void __thiscall cAIChar_UpdateMove(cAIChar *self, float dt);
/* the reactions to the events of the move by state; 0 if the state does not take the event */
byte __thiscall cAIChar_EventDefault(cAIChar *self, int ev);
byte __thiscall cAIChar_EventInRoute(cAIChar *self, int ev);
byte __thiscall cAIChar_EventPatrol(cAIChar *self, int ev);
byte __thiscall cAIChar_EventGoTo(cAIChar *self, int ev);
byte __thiscall cAIChar_EventGetAway(cAIChar *self, int ev);
byte __thiscall cAIChar_EventChaseBack(cAIChar *self, int ev);
byte __thiscall cAIChar_EventLostPath(cAIChar *self, int ev);
/* IN_ROUTE and PATROL_ZONE set again (after a talk) */
void __thiscall cAIChar_Resume(cAIChar *self);
void __thiscall cAIChar_UpdateThink(cAIChar *self);
/* every second: paused while out of view and far from the camera (pauseCheck) */
void __thiscall cAIChar_CheckPause(cAIChar *self);
/* every second: the repositioning when the camera is in the graph and the character out of view and far */
void __thiscall cAIChar_CheckRepos(cAIChar *self);
byte __thiscall cAIChar_CanRepos(cAIChar *self, int state);
/* the think of the state */
void __thiscall cAIChar_Think(cAIChar *self);
void __thiscall cAIChar_ThinkInRoute(cAIChar *self);
void __thiscall cAIChar_ThinkLostPath(cAIChar *self);
void __thiscall cAIChar_ThinkLostPathChase(cAIChar *self);
/* LOST_PATH (two copies): back to the kept state once the character is on the graph again */
void __thiscall cAIChar_ReactLostPath(cAIChar *self);
void __thiscall cAIChar_ReactPatrol(cAIChar *self);
void __thiscall cAIChar_ReactInRoute(cAIChar *self);
void __thiscall cAIChar_ThinkPatrol(cAIChar *self);
/* a talk with the character run into, when both walk (IN_ROUTE or PATROL_ZONE) at about the same height */
void __thiscall cAIChar_CheckTalk(cAIChar *self);
/* a looks within 45 degrees at b or b at a */
byte __thiscall cAIChar_Facing(cAIChar *self, cEntity *a, cEntity *b);

#endif
