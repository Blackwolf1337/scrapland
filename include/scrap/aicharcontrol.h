/*
 * AI/AICharControl.cpp, AICharControl2D.cpp and AICharControl3D.cpp of the game: the move control of the AI of a
 * walking character (cAICharControl, 0x360 bytes, made by the constructor of cAIChar at 0x19c of the AI). It steers
 * the cWalkCharControl of the character (the turn, walk and strafe values a player would give) towards a goal: a
 * point (GO_TO), a direction, an entity it follows or keeps away from, or a route of the graph. The goal is reached
 * along a path of the graph (CNodesWorld2D on foot, CNodesWorld outdoors in 3D), the control walks to the next point
 * of the path (goal) and tells the AI of the character by an event when it arrives or when the way is blocked. The
 * 2D control walks on the floor, the 3D one flies. src/game/AI/ICharControl.c (the base part), ICharControl2D.c,
 * ICharControl3D.c.
 */
#ifndef SCRAP_AICHARCONTROL_H
#define SCRAP_AICHARCONTROL_H

#include <stdarg.h>

#include "scrap/gvar.h"
#include "scrap/ai.h"
#include "scrap/fxmisc.h"
#include "scrap/nodesworld.h"
#include "scrap/objentity.h"
#include "scrap/scrapcollision.h"
#include "scrap/slotcontroller.h"
#include "scrap/walkcharentity.h"

#define CAICHARCONTROL_SIZE sizeof(cAICharControl)

/* the moves of cAICharControl_SetMove (mode at 0x74) */
enum {
	AICC_STOP,		/* 0 */
	AICC_GO_TO,		/* 1 vec3_t dest, dist, speed: along the path of the graph */
	AICC_GO_TO_DIRECT,	/* 2 vec3_t dest, dist, speed */
	AICC_FOLLOW,		/* 3 dist near, dist far, speed, byte: the target of the AI */
	AICC_FOLLOW_PATH,	/* 4 dist near, dist far, speed, byte */
	AICC_DIR,		/* 5 vec3_t dir */
	AICC_AWAY,		/* 6 int: keep away from the target of the AI */
	AICC_ROUTE		/* 7 vec3_t point, dist, speed, byte, byte: a route of the graph */
};

typedef struct cAICharControl cAICharControl;

/* the slots of cAICharControl::vftable (100); the 2D control adds three, the 3D control one. The handlers of the
 * modes (UpdateGoTo ..) take the time of the frame */
typedef struct cAICharControlVtbl {
	void (*Free)(cAICharControl *self);
	void(*Draw)(cAICharControl *self);
	int(*GetWalkDir)(cAICharControl *self);
	int(*Busy)(cAICharControl *self);						/* the strafe side */
	int(*GetOrbitSide)(cAICharControl *self);
	/* the frame: the checks of the AI, then the handler of the mode; 0 when the target is gone */
	byte(*Update)(cAICharControl *self, float dt);
	byte (*SetMove)(cAICharControl *self, int mode, ...);
	void(*Reset)(cAICharControl *self);
	void(*SetHold)(cAICharControl *self, byte on);
	/* the turn, walk, strafe and jump values of the control 0 */
	void(*Stop)(cAICharControl *self);
	byte(*IsDirect)(cAICharControl *self);
	byte(*IsDirect2)(cAICharControl *self);
	void(*GetGoal)(cAICharControl *self, vec3_t *out);
	void(*GetDest)(cAICharControl *self, vec3_t *out);
	/* the corridor of the body from a to b free (the radius r) */
	byte(*TestPath)(cAICharControl *self, const vec3_t *a, const vec3_t *b, float r);
	byte(*TestLine)(cAICharControl *self, const vec3_t *a, const vec3_t *b);
	byte(*TestSides)(cAICharControl *self, const vec3_t *a, const vec3_t *b, float r);
	void(*GetAvoidA)(cAICharControl *self, vec3_t *out);
	void(*GetAvoidB)(cAICharControl *self, vec3_t *out);
	void(*ClearAvoid)(cAICharControl *self);
	void(*SetSpeed)(cAICharControl *self, float speed);
	void(*RestartSlots)(cAICharControl *self);
	byte(*IsBlocked)(cAICharControl *self);
	byte(*GetE4)(cAICharControl *self);
	void *(*GetAvoidEnt)(cAICharControl *self);
	void(*Nop)(cAICharControl *self);
	/* the point walked to is the last one before the end (none: no point) */
	byte(*AtPathEnd)(cAICharControl *self);
	byte(*GoalAtStart)(cAICharControl *self);
	/* the target of a follow or away move gone: the event 3, 0 */
	byte(*CheckTarget)(cAICharControl *self);
	void(*SetTurnSpeed)(cAICharControl *self, float speed);
	/* the strafe around a character at pos (the two sides of TryStrafe) */
	byte(*StrafeBack)(cAICharControl *self, const vec3_t *pos, const vec3_t *away, const vec3_t *dir,
		float dist, byte left, byte flag);
	byte(*StrafeFwd)(cAICharControl *self, const vec3_t *pos, const vec3_t *away, const vec3_t *dir,
		float dist, byte left, byte flag);
	byte(*TryStrafe)(cAICharControl *self, int side, cEntity *e, byte a, float b, byte c, float d,
		byte e2);
	byte(*Facing)(cAICharControl *self, cEntity *e, vec3_t *toE, vec3_t *toGoal);
	byte(*InWay)(cAICharControl *self, vec3_t dest, cEntity *e);
	byte(*Near)(cAICharControl *self, cEntity *e);
	float(*KeepDist)(cAICharControl *self, cEntity *e);
	float(*StrafeDist)(cAICharControl *self, cEntity *e);
	byte(*OnFloor)(cAICharControl *self, const vec3_t *p);
	byte(*GoalAhead)(cAICharControl *self);
	byte(*TestPathEnd)(cAICharControl *self, const vec3_t *a, const vec3_t *b, float r);
	float(*Dist)(cAICharControl *self, const vec3_t *a, const vec3_t *b);
	float(*DistSq)(cAICharControl *self, const vec3_t *a, const vec3_t *b);
	void(*Away)(cAICharControl *self, Nto1Node *target, byte flag);
	byte(*UpdateAway)(cAICharControl *self, float dt);
	/* the view near the way to the target: the event 6 */
	void(*AwayCheck)(cAICharControl *self);
	/* the walk back from the target while it comes near, none while another character sees it */
	void(*AwayWalk)(cAICharControl *self);
	/* e within ang of the back (the front) of the view, on the floor */
	byte(*Behind)(cAICharControl *self, cEntity *e, float ang);
	byte(*InFront)(cAICharControl *self, cEntity *e, float ang);
	byte(*Route)(cAICharControl *self, const vec3_t *p, float dist, float speed, byte a, byte b);
	void(*SetDir)(cAICharControl *self, const vec3_t *dir);
	byte(*UpdateDir)(cAICharControl *self, float dt);
	/* the view near dir: the event 6 */
	void(*DirCheck)(cAICharControl *self);
	/* the view turned to the goal (turnGain, turnStep) */
	void(*DirTurn)(cAICharControl *self);
	void(*CheckArrived)(cAICharControl *self);
	/* 0 (and the control stopped) when the next segment of the path is cut near the character */
	byte(*CanMove)(cAICharControl *self);
	byte(*GoTo)(cAICharControl *self, const vec3_t *p, float dist, float speed);
	byte(*GoToDirect)(cAICharControl *self, const vec3_t *p, float dist, float speed);
	byte(*FindPath)(cAICharControl *self, const vec3_t *p);
	/* the path is the line to p */
	byte(*DirectPath)(cAICharControl *self, const vec3_t *p);
	/* p moved a random step while the way stays free */
	void(*StepGoal)(cAICharControl *self, vec3_t *p);
	byte(*UpdateGoTo)(cAICharControl *self, float dt);
	byte(*UpdateRoute)(cAICharControl *self, float dt);
	byte(*UpdateGoToDirect)(cAICharControl *self, float dt);
	/* the goal on the first of NumPointsSearchSegmentCharAI points from a to b the body reaches */
	byte(*GoalOnLine)(cAICharControl *self, const vec3_t *a, const vec3_t *b);
	/* near the goal (or with the goal lost) the goal on to the next point of the path, or between */
	void(*NextGoal)(cAICharControl *self);
	/* the goal on to the last point of the path the body reaches, or between it and the next */
	void(*NextGoalPath)(cAICharControl *self);
	/* the goal on along the path while the way is cut; the path searched again when no point is free */
	byte(*UpdatePath)(cAICharControl *self);
	/* at the end of a path that does not reach dest: the line to dest (stuck, pathRes 2), 0 then */
	byte(*CheckPathEnd)(cAICharControl *self);
	byte(*UpdateRoutePath)(cAICharControl *self);
	void(*Turn)(cAICharControl *self);
	/* the strafe around the characters in the way (slotStrafe) */
	void(*Strafe)(cAICharControl *self);
	/* the characters near: link to the first one in the way that can be passed */
	byte(*FindAvoid)(cAICharControl *self);
	byte(*Avoid)(cAICharControl *self, cEntity *e, byte *found);
	/* the strafe around the linked character (or the one the character ran into) */
	void(*UpdateAvoid)(cAICharControl *self);
	/* walking characters within MarginDetectCollCharAI */
	byte(*CharsNear)(cAICharControl *self);
	byte(*SideBlocked)(cAICharControl *self, int side);
	/* the unit vector to the side (out returned) */
	vec3_t *(*SideDir)(cAICharControl *self, vec3_t *out, int side);
	void(*WalkFwd)(cAICharControl *self);
	byte(*CheckGoTo)(cAICharControl *self);
	byte(*CheckRoute)(cAICharControl *self);
	byte(*CheckFollow)(cAICharControl *self);
	byte(*FindRoute)(cAICharControl *self);
	/* the view turned to the target (or along avoidDir) */
	void(*TurnFollow)(cAICharControl *self);
	byte(*Follow)(cAICharControl *self, Nto1Node *target, float near, float far, float speed);
	byte(*UpdateFollowMode)(cAICharControl *self, float dt);
	/* the strafe of CheckWait instead of the walk; 0 when there is none */
	byte(*Wait)(cAICharControl *self, float dt);
	/* the strafe while the target goes out of sight (waitTime up to TimeStrafeLostObjCharAI) */
	byte(*CheckWait)(cAICharControl *self);
	void(*StrafeWalk)(cAICharControl *self);
	byte(*AIBlocked)(cAICharControl *self);						/* the AI cannot go on (gear), 0 */
	void(*Strafe2)(cAICharControl *self);
	void(*Walk)(cAICharControl *self);
	void(*Walk2)(cAICharControl *self);
	byte(*UpdateFollow)(cAICharControl *self);
	byte(*UpdateFollowB)(cAICharControl *self);
	byte(*FollowPath)(cAICharControl *self, Nto1Node *target, float near, float far, float speed);
	byte(*UpdateFollowPath)(cAICharControl *self, float dt);
	/* the strafe around the target while it looks at the character (orbitSide) */
	void(*Orbit)(cAICharControl *self);
	/* the side of the orbit at dist from the target */
	byte(*StartOrbit)(cAICharControl *self, float dist);
	void(*Stop2)(cAICharControl *self);
} cAICharControlVtbl;

/* calls through the slots of cAICharControlVtbl */
static inline void AICC_Away(void *self, Nto1Node *target, byte flag) {
	VT(cAICharControlVtbl, self)->Away(self, target, flag);
}

static inline byte AICC_AtPathEnd(void *self) {
	return VT(cAICharControlVtbl, self)->AtPathEnd(self);
}

static inline byte AICC_Avoid(void *self, cEntity *e, byte *found) {
	return VT(cAICharControlVtbl, self)->Avoid(self, e, found);
}

static inline void AICC_AwayCheck(void *self) {
	VT(cAICharControlVtbl, self)->AwayCheck(self);
}

static inline void AICC_AwayWalk(void *self) {
	VT(cAICharControlVtbl, self)->AwayWalk(self);
}

static inline int AICC_Busy(void *self) {
	return VT(cAICharControlVtbl, self)->Busy(self);
}

static inline byte AICC_CanMove(void *self) {
	return VT(cAICharControlVtbl, self)->CanMove(self);
}

static inline byte AICC_CharsNear(void *self) {
	return VT(cAICharControlVtbl, self)->CharsNear(self);
}

static inline void AICC_CheckArrived(void *self) {
	VT(cAICharControlVtbl, self)->CheckArrived(self);
}

static inline byte AICC_CheckFollow(void *self) {
	return VT(cAICharControlVtbl, self)->CheckFollow(self);
}

static inline byte AICC_CheckPathEnd(void *self) {
	return VT(cAICharControlVtbl, self)->CheckPathEnd(self);
}

static inline byte AICC_CheckTarget(void *self) {
	return VT(cAICharControlVtbl, self)->CheckTarget(self);
}

static inline byte AICC_CheckWait(void *self) {
	return VT(cAICharControlVtbl, self)->CheckWait(self);
}

static inline void AICC_ClearAvoid(void *self) {
	VT(cAICharControlVtbl, self)->ClearAvoid(self);
}

static inline void AICC_Free(void *self) {
	VT(cAICharControlVtbl, self)->Free(self);
}

static inline void AICC_DirCheck(void *self) {
	VT(cAICharControlVtbl, self)->DirCheck(self);
}

static inline void AICC_DirTurn(void *self) {
	VT(cAICharControlVtbl, self)->DirTurn(self);
}

static inline byte AICC_DirectPath(void *self, const vec3_t *p) {
	return VT(cAICharControlVtbl, self)->DirectPath(self, p);
}

static inline float AICC_Dist(void *self, const vec3_t *a, const vec3_t *b) {
	return VT(cAICharControlVtbl, self)->Dist(self, a, b);
}

static inline float AICC_DistSq(void *self, const vec3_t *a, const vec3_t *b) {
	return VT(cAICharControlVtbl, self)->DistSq(self, a, b);
}

static inline void AICC_Draw(void *self) {
	VT(cAICharControlVtbl, self)->Draw(self);
}

static inline byte AICC_Facing(void *self, cEntity *e, vec3_t *toE, vec3_t *toGoal) {
	return VT(cAICharControlVtbl, self)->Facing(self, e, toE, toGoal);
}

static inline byte AICC_FindAvoid(void *self) {
	return VT(cAICharControlVtbl, self)->FindAvoid(self);
}

static inline byte AICC_FindPath(void *self, const vec3_t *p) {
	return VT(cAICharControlVtbl, self)->FindPath(self, p);
}

static inline byte AICC_FindRoute(void *self) {
	return VT(cAICharControlVtbl, self)->FindRoute(self);
}

static inline byte AICC_Follow(void *self, Nto1Node *target, float near, float far, float speed) {
	return VT(cAICharControlVtbl, self)->Follow(self, target, near, far, speed);
}

static inline byte AICC_FollowPath(void *self, Nto1Node *target, float near, float far, float speed) {
	return VT(cAICharControlVtbl, self)->FollowPath(self, target, near, far, speed);
}

static inline void AICC_GetAvoidA(void *self, vec3_t *out) {
	VT(cAICharControlVtbl, self)->GetAvoidA(self, out);
}

static inline void AICC_GetAvoidB(void *self, vec3_t *out) {
	VT(cAICharControlVtbl, self)->GetAvoidB(self, out);
}

static inline void AICC_GetDest(void *self, vec3_t *out) {
	VT(cAICharControlVtbl, self)->GetDest(self, out);
}

static inline int AICC_GetOrbitSide(void *self) {
	return VT(cAICharControlVtbl, self)->GetOrbitSide(self);
}

static inline int AICC_GetWalkDir(void *self) {
	return VT(cAICharControlVtbl, self)->GetWalkDir(self);
}

static inline byte AICC_GoTo(void *self, const vec3_t *p, float dist, float speed) {
	return VT(cAICharControlVtbl, self)->GoTo(self, p, dist, speed);
}

static inline byte AICC_GoToDirect(void *self, const vec3_t *p, float dist, float speed) {
	return VT(cAICharControlVtbl, self)->GoToDirect(self, p, dist, speed);
}

static inline byte AICC_GoalAhead(void *self) {
	return VT(cAICharControlVtbl, self)->GoalAhead(self);
}

static inline byte AICC_InFront(void *self, cEntity *e, float ang) {
	return VT(cAICharControlVtbl, self)->InFront(self, e, ang);
}

static inline byte AICC_InWay(void *self, vec3_t dest, cEntity *e) {
	return VT(cAICharControlVtbl, self)->InWay(self, dest, e);
}

static inline byte AICC_IsBlocked(void *self) {
	return VT(cAICharControlVtbl, self)->IsBlocked(self);
}

static inline byte AICC_IsDirect(void *self) {
	return VT(cAICharControlVtbl, self)->IsDirect(self);
}

static inline byte AICC_IsDirect2(void *self) {
	return VT(cAICharControlVtbl, self)->IsDirect2(self);
}

static inline float AICC_KeepDist(void *self, cEntity *e) {
	return VT(cAICharControlVtbl, self)->KeepDist(self, e);
}

static inline byte AICC_Near(void *self, cEntity *e) {
	return VT(cAICharControlVtbl, self)->Near(self, e);
}

static inline void AICC_NextGoal(void *self) {
	VT(cAICharControlVtbl, self)->NextGoal(self);
}

static inline void AICC_NextGoalPath(void *self) {
	VT(cAICharControlVtbl, self)->NextGoalPath(self);
}

static inline void AICC_Nop(void *self) {
	VT(cAICharControlVtbl, self)->Nop(self);
}

static inline void AICC_Orbit(void *self) {
	VT(cAICharControlVtbl, self)->Orbit(self);
}

static inline void AICC_Reset(void *self) {
	VT(cAICharControlVtbl, self)->Reset(self);
}

static inline byte AICC_Route(void *self, const vec3_t *p, float dist, float speed, byte a, byte b) {
	return VT(cAICharControlVtbl, self)->Route(self, p, dist, speed, a, b);
}

static inline void AICC_SetDir(void *self, const vec3_t *dir) {
	VT(cAICharControlVtbl, self)->SetDir(self, dir);
}

static inline void AICC_SetHold(void *self, byte on) {
	VT(cAICharControlVtbl, self)->SetHold(self, on);
}

#define AICC_SetMove(self, ...) (VT(cAICharControlVtbl, self)->SetMove((void *)(self), __VA_ARGS__))

static inline void AICC_SetSpeed(void *self, float speed) {
	VT(cAICharControlVtbl, self)->SetSpeed(self, speed);
}

static inline void AICC_SetTurnSpeed(void *self, float speed) {
	VT(cAICharControlVtbl, self)->SetTurnSpeed(self, speed);
}

static inline byte AICC_SideBlocked(void *self, int side) {
	return VT(cAICharControlVtbl, self)->SideBlocked(self, side);
}

static inline vec3_t *AICC_SideDir(void *self, vec3_t *out, int side) {
	return VT(cAICharControlVtbl, self)->SideDir(self, out, side);
}

static inline byte AICC_StartOrbit(void *self, float dist) {
	return VT(cAICharControlVtbl, self)->StartOrbit(self, dist);
}

static inline void AICC_StepGoal(void *self, vec3_t *p) {
	VT(cAICharControlVtbl, self)->StepGoal(self, p);
}

static inline void AICC_Stop(void *self) {
	VT(cAICharControlVtbl, self)->Stop(self);
}

static inline void AICC_Stop2(void *self) {
	VT(cAICharControlVtbl, self)->Stop2(self);
}

static inline void AICC_Strafe(void *self) {
	VT(cAICharControlVtbl, self)->Strafe(self);
}

static inline void AICC_Strafe2(void *self) {
	VT(cAICharControlVtbl, self)->Strafe2(self);
}

static inline byte AICC_StrafeBack(void *self, const vec3_t *pos, const vec3_t *away, const vec3_t *dir, float dist, byte left, byte flag) {
	return VT(cAICharControlVtbl, self)->StrafeBack(self, pos, away, dir, dist, left, flag);
}

static inline float AICC_StrafeDist(void *self, cEntity *e) {
	return VT(cAICharControlVtbl, self)->StrafeDist(self, e);
}

static inline byte AICC_StrafeFwd(void *self, const vec3_t *pos, const vec3_t *away, const vec3_t *dir, float dist, byte left, byte flag) {
	return VT(cAICharControlVtbl, self)->StrafeFwd(self, pos, away, dir, dist, left, flag);
}

static inline void AICC_StrafeWalk(void *self) {
	VT(cAICharControlVtbl, self)->StrafeWalk(self);
}

static inline byte AICC_TestLine(void *self, const vec3_t *a, const vec3_t *b) {
	return VT(cAICharControlVtbl, self)->TestLine(self, a, b);
}

static inline byte AICC_TestPath(void *self, const vec3_t *a, const vec3_t *b, float r) {
	return VT(cAICharControlVtbl, self)->TestPath(self, a, b, r);
}

static inline byte AICC_TestPathEnd(void *self, const vec3_t *a, const vec3_t *b, float r) {
	return VT(cAICharControlVtbl, self)->TestPathEnd(self, a, b, r);
}

static inline byte AICC_TestSides(void *self, const vec3_t *a, const vec3_t *b, float r) {
	return VT(cAICharControlVtbl, self)->TestSides(self, a, b, r);
}

static inline byte AICC_TryStrafe(void *self, int side, cEntity *e, byte a, float b, byte c, float d, byte e2) {
	return VT(cAICharControlVtbl, self)->TryStrafe(self, side, e, a, b, c, d, e2);
}

static inline void AICC_Turn(void *self) {
	VT(cAICharControlVtbl, self)->Turn(self);
}

static inline void AICC_TurnFollow(void *self) {
	VT(cAICharControlVtbl, self)->TurnFollow(self);
}

static inline byte AICC_Update(void *self, float dt) {
	return VT(cAICharControlVtbl, self)->Update(self, dt);
}

static inline void AICC_UpdateAvoid(void *self) {
	VT(cAICharControlVtbl, self)->UpdateAvoid(self);
}

static inline byte AICC_UpdateAway(void *self, float dt) {
	return VT(cAICharControlVtbl, self)->UpdateAway(self, dt);
}

static inline byte AICC_UpdateDir(void *self, float dt) {
	return VT(cAICharControlVtbl, self)->UpdateDir(self, dt);
}

static inline byte AICC_UpdateFollow(void *self) {
	return VT(cAICharControlVtbl, self)->UpdateFollow(self);
}

static inline byte AICC_UpdateFollowB(void *self) {
	return VT(cAICharControlVtbl, self)->UpdateFollowB(self);
}

static inline byte AICC_UpdateFollowMode(void *self, float dt) {
	return VT(cAICharControlVtbl, self)->UpdateFollowMode(self, dt);
}

static inline byte AICC_UpdateFollowPath(void *self, float dt) {
	return VT(cAICharControlVtbl, self)->UpdateFollowPath(self, dt);
}

static inline byte AICC_UpdateGoTo(void *self, float dt) {
	return VT(cAICharControlVtbl, self)->UpdateGoTo(self, dt);
}

static inline byte AICC_UpdateGoToDirect(void *self, float dt) {
	return VT(cAICharControlVtbl, self)->UpdateGoToDirect(self, dt);
}

static inline byte AICC_UpdatePath(void *self) {
	return VT(cAICharControlVtbl, self)->UpdatePath(self);
}

static inline byte AICC_UpdateRoute(void *self, float dt) {
	return VT(cAICharControlVtbl, self)->UpdateRoute(self, dt);
}

static inline byte AICC_UpdateRoutePath(void *self) {
	return VT(cAICharControlVtbl, self)->UpdateRoutePath(self);
}

static inline byte AICC_AIBlocked(void *self) {
	return VT(cAICharControlVtbl, self)->AIBlocked(self);
}

static inline byte AICC_Wait(void *self, float dt) {
	return VT(cAICharControlVtbl, self)->Wait(self, dt);
}

static inline void AICC_Walk(void *self) {
	VT(cAICharControlVtbl, self)->Walk(self);
}

static inline void AICC_Walk2(void *self) {
	VT(cAICharControlVtbl, self)->Walk2(self);
}

static inline void AICC_WalkFwd(void *self) {
	VT(cAICharControlVtbl, self)->WalkFwd(self);
}

/* the slots the 2D and the 3D control add */
typedef struct cAICharControl2DVtbl {
	cAICharControlVtbl base;
	byte(*FloorAhead)(cAICharControl *self, const vec3_t *p);
	byte(*FloorAt)(cAICharControl *self, const vec3_t *p, vec3_t *out);
	byte(*TestStep)(cAICharControl *self, const vec3_t *a, const vec3_t *b, const vec3_t *c, float r);
} cAICharControl2DVtbl;

/* calls through the slots of cAICharControl2DVtbl */
static inline byte AICC2D_FloorAhead(void *self, const vec3_t *p) {
	return VT(cAICharControl2DVtbl, self)->FloorAhead(self, p);
}

static inline byte AICC2D_FloorAt(void *self, const vec3_t *p, vec3_t *out) {
	return VT(cAICharControl2DVtbl, self)->FloorAt(self, p, out);
}

static inline byte AICC2D_TestStep(void *self, const vec3_t *a, const vec3_t *b, const vec3_t *c, float r) {
	return VT(cAICharControl2DVtbl, self)->TestStep(self, a, b, c, r);
}

typedef struct cAICharControl3DVtbl {
	cAICharControlVtbl base;
	byte(*TestStep)(cAICharControl *self, const vec3_t *a, const vec3_t *b, float r);
} cAICharControl3DVtbl;

/* the AI of the character (cAIChar, include/scrap/ai.h): the control calls GetTarget (0x38) and Event (0x58) */
typedef cAICharVtbl cAICharCtlOwnerVtbl;

/* a segment of the debug view */
typedef struct cAICCSeg {
	vec3_t a, b;
} cAICCSeg;

struct cAICharControl {
	const cAICharControlVtbl *vftable;
	cSlotController slotWalk;	/* the distance checks of Walk and AwayWalk (0.2 s) */
	cSlotController slotSee2;	/* AwayWalk: the other characters that see it (0.2 s) */
	cSlotController slotStrafe;	/* the strafe around the characters in the way (0.133 s) */
	cSlotController slotStop;	/* CanMove (0.2 s) */
	cAIChar *ai;			/* the AI of the character */
	cEntity *ent;			/* the character (cWalkCharEntity) */
	cWalkCharControl *ctrl;		/* the control of the character the AI steers */
	float radius;			/* the radius of the character */
	float arriveDist;		/* the goal is reached that near */
	float arriveR;			/* the smaller of radius and arriveDist */
	float dt;			/* the time of the frame */
	float diffX;			/* the angles to the goal (Turn) */
	float diffY;
	vec3_t dest;			/* the end of the move */
	int mode;			/* AICC_ */
	int prevMode;
	int soft;			/* the argument of the constructor: the path search may give up first */
	float turnX;			/* the vertical turn of the last frame */
	float turnY;			/* the horizontal turn of the last frame */
	byte targetChanged;		/* a follow move got a new target */
	byte pad_89[3];
	float turnGain;			/* the turn for an angle of pi */
	float turnStep;			/* the change of the turn in a frame at most */
	int walkDir;			/* 0 stop, 1 forward, 2 back */
	int side;			/* the strafe around a character: 0 none, 1, 2 */
	int strafeDir;			/* the strafe of CheckWait: 0 none, 1, 2 */
	byte flying;			/* the argument of FOLLOW */
	byte stopped;			/* CanMove stopped the control */
	byte pad_a2[2];
	vec3_t avoidA;			/* the segment of the last path check (the character, the goal) */
	vec3_t avoidB;			/* -FLT_MAX after ClearAvoid */
	byte hold;
	byte newPath;
	byte pad_be[2];
	float avoidTime;		/* the time of the strafe around a character */
	Nto1Node link;			/* to the character avoided (the head at 0x254 of it), data the AI */
	byte blocked;
	byte blockedByChar;		/* other characters are in the way (the collision event may fire) */
	byte pad_d6[2];
	vec3_t avoidDir;		/* 0 for none */
	byte avoidFound;		/* FindAvoid found a character */
	byte pad_e5[3];
	float collEventTime;		/* the times since the last collision event, stop, test (TimeEventCollCharAI ..) */
	float collStopTime;
	float collTestTime;
	float speed;			/* the walk value */
	int lifeMark;			/* the life of the character */
	cSlotController slotPath;	/* the path is searched again (0.133 s) */
	vec3_t goal;			/* the next point of the path */
	cNWVecList path;
	vec3_t *pathPoint;		/* the point of the path walked to */
	vec3_t *prevPoint;		/* the one before it UpdatePath skipped */
	float turnXNew;
	float turnYNew;
	byte unused_140;
	byte goalLost;			/* the goal was not reachable at the last UpdatePath */
	byte pad_142[2];
	cSlotController slotDirect;	/* the direct follow checks the way (0.2 s) */
	cSlotController slotSee;	/* the path follow looks for the direct way (0.2 s) */
	cSlotController slotRepath;	/* the path to the target searched again (5 s) */
	cSlotController slotNear;	/* the end of the path near: searched again (0.2 s) */
	cSlotController slotOrbit;	/* the side of the orbit blocked (0.2 s) */
	cSlotController slotLost;	/* the path searched again while stuck (3 s) */
	cSlotController slotWait;	/* CheckWait (0.2 s) */
	Nto1Node *target;		/* the link to the entity to follow (objDin) */
	cEntity *targetEnt;		/* its entity when the move started */
	float farDist;
	float nearDist;
	vec3_t v1c4;
	byte direct;			/* the follow walks straight to the target */
	byte waitStart;			/* CheckWait has its start positions */
	byte pad_1d2[2];
	float waitTime;			/* the time of the strafe of CheckWait */
	vec3_t waitPos;			/* the character and the target at the start of the wait */
	vec3_t waitTargetPos;
	vec3_t lastTargetPos;		/* the target at the last Orbit */
	int pathRes;			/* the result of the path search (2: the end is not reachable) */
	byte stuck;			/* the follow sees no way */
	byte avoidChar;			/* FindAvoid linked a character */
	byte pad_202[2];
	float viewGain;			/* the turn of the view to the dest (VelRotDynamic...) */
	float viewStep;			/* (LimVelRotDynamic...) */
	int orbitSide;			/* the strafe around the target: 0 none, 1, 2 */
	byte orbitStart;		/* StartOrbit may run */
	byte pad_211[3];
	vec3_t orbitDir;		/* the back of the target at the start of the orbit (y 0) */
	vec3_t dir;			/* the direction of AICC_DIR */
	byte awayFlag;
	byte seen;			/* AwayWalk: another character sees it */
	byte routeA;
	byte routeB;
	vec3_t routePoint;
	cNWNode *routeNode;		/* the node of the route BuildPath reached */
	cAICCSeg debugSegs[9];		/* the corridor of the last path test */
	cNWVecList debugStrafe;		/* the points of the strafe tests */
	cNWVecList debugStrafeInner;	/* the same with a smaller radius */
	cNWSegList debugPath;		/* the segments of the path tests (ShowTestCanGoAI) */
};
/* the 2D and the 3D control add only vtable slots (both the constructor of cAIChar) */
typedef cAICharControl cAICharControl2D;
typedef cAICharControl cAICharControl3D;

static inline const cAICharControlVtbl *AICC_Vtbl(const cAICharControl *self) {
	return self->vftable;
}

/* the entity the target link points to (objDin->GetRoot()) */
static inline cEntity *AICC_TargetEnt(const cAICharControl *self) {
	return (cEntity *)self->target->target;
}

static inline const vec3_t *AICC_Pos(cAICharControl *self, vec3_t *buf) {
	return Entity_GetPos(self->ent, buf);
}

static inline const vec3_t *AICC_EntPos(cEntity *e, vec3_t *buf) {
	return Entity_GetPos(e, buf);
}

/* a segment of the corridor: p = a + v * r, q = p + nl (aim: q = p + normalize(b - p) * len); 1 when it is free */
static inline int AICC_Seg(vec3_t *p, vec3_t *q, const vec3_t *a, const vec3_t *v, float r, const vec3_t *nl,
	const vec3_t *b, float len) {
	vec3_t d, e;

	p->x = a->x + v->x * r;
	p->y = a->y + v->y * r;
	p->z = a->z + v->z * r;
	if (b) {
		d.x = b->x - p->x;
		d.y = b->y - p->y;
		d.z = b->z - p->z;
		Vec3_NormalizeTo(&e, &d);
		q->x = e.x * len + p->x;
		q->y = e.y * len + p->y;
		q->z = e.z * len + p->z;
	} else {
		q->x = nl->x + p->x;
		q->y = nl->y + p->y;
		q->z = nl->z + p->z;
	}
	return !Engine_TestLine(p, q, NULL, NULL);
}

/* ---- the base part, AI/AICharControl.cpp (src/game/AI/ICharControl.c) ---- */
void cAICharControl_ClearAvoid(cAICharControl *self);
/* the strafe around e (a character in the way): 1 when the control strafes, *found 1 when e is in the way */
byte cAICharControl_Avoid(cAICharControl *self, cEntity *e, byte *found);
byte cAICharControl_FollowPath(cAICharControl *self, Nto1Node *target, float near, float far, float speed);
byte cAICharControl_Follow(cAICharControl *self, Nto1Node *target, float near, float far, float speed);
/* the move (AICC_, the arguments by mode, floats as double); 0 if it was refused */
byte cAICharControl_SetMove(cAICharControl *self, int mode, ...);
void cAICharControl_Away(cAICharControl *self, Nto1Node *target, byte flag);
/* the result of FindRoute */
byte cAICharControl_Route(cAICharControl *self, const vec3_t *p, float dist, float speed, byte a, byte b);
/* the goal 10000 ahead along dir */
void cAICharControl_SetDir(cAICharControl *self, const vec3_t *dir);
void cAICharControl_Nop(cAICharControl *self);
byte cAICharControl_GoTo(cAICharControl *self, const vec3_t *p, float dist, float speed);
byte cAICharControl_GoToDirect(cAICharControl *self, const vec3_t *p, float dist, float speed);
void cAICharControl_SetSpeed(cAICharControl *self, float speed);
void cAICharControl_SetTurnSpeed(cAICharControl *self, float speed);
void cAICharControl_Stop2(cAICharControl *self);
/* the control stopped, dest and goal -FLT_MAX */
void cAICharControl_Reset(cAICharControl *self);
byte cAICharControl_IsBlocked(cAICharControl *self);
byte cAICharControl_GetAvoidFound(cAICharControl *self);
void cAICharControl_SetHold(cAICharControl *self, byte on);

/* construction, the frame and the slots of the first part of the file */
cAICharControl *cAICharControl_ctor(void *mem, cEntity *ent, cAIChar *ai, int *arg);
void cAICharControl_dtor(cAICharControl *self);
/* the segment list (0x20 byte nodes) emptied, and with its free nodes deleted */
void DListSeg_Clear(cNWSegList *list);
/* the components equal (cdecl, al) */
byte Vec3_Equal(const vec3_t *a, const vec3_t *b);
byte cAICharControl_CheckTarget(cAICharControl *self);
byte cAICharControl_CheckPathEnd(cAICharControl *self);
byte cAICharControl_UpdatePath(cAICharControl *self);
byte cAICharControl_UpdateRoutePath(cAICharControl *self);
byte cAICharControl_UpdateFollowB(cAICharControl *self);
byte cAICharControl_CharsNear(cAICharControl *self);
void cAICharControl_AwayCheck(cAICharControl *self);
void cAICharControl_DirCheck(cAICharControl *self);
byte cAICharControl_CanMove(cAICharControl *self);
byte cAICharControl_Update(cAICharControl *self, float dt);
void cAICharControl_TurnFollow(cAICharControl *self);
byte cAICharControl_CheckRoute(cAICharControl *self);
void cAICharControl_DirTurn(cAICharControl *self);
byte cAICharControl_CheckGoTo(cAICharControl *self);
byte cAICharControl_CheckFollow(cAICharControl *self);
void cAICharControl_UpdateAvoid(cAICharControl *self);
void cAICharControl_Strafe2(cAICharControl *self);
void cAICharControl_StrafeWalk(cAICharControl *self);
byte cAICharControl_FindAvoid(cAICharControl *self);
void cAICharControl_Strafe(cAICharControl *self);
void cAICharControl_Walk2(cAICharControl *self);
void cAICharControl_AwayWalk(cAICharControl *self);
void cAICharControl_WalkFwd(cAICharControl *self);
byte cAICharControl_CheckWait(cAICharControl *self);
void cAICharControl_Orbit(cAICharControl *self);
byte cAICharControl_StartOrbit(cAICharControl *self, float dist);
void cAICharControl_Draw(cAICharControl *self);
byte cAICharControl_DirectPath(cAICharControl *self, const vec3_t *p);
void cAICharControl_Stop(cAICharControl *self);
byte cAICharControl_AIBlocked(cAICharControl *self);
void cAICharControl_RestartSlots(cAICharControl *self);
int cAICharControl_GetWalkDir(cAICharControl *self);
void *cAICharControl_GetAvoidEnt(cAICharControl *self);
void cAICharControl_NextGoal(cAICharControl *self);
void cAICharControl_NextGoalPath(cAICharControl *self);
void cAICharControl_GetGoal(cAICharControl *self, vec3_t *out);
void cAICharControl_GetDest(cAICharControl *self, vec3_t *out);
byte cAICharControl_GoalOnLine(cAICharControl *self, const vec3_t *a, const vec3_t *b);
void cAICharControl_GetAvoidB(cAICharControl *self, vec3_t *out);
void cAICharControl_GetAvoidA(cAICharControl *self, vec3_t *out);
int cAICharControl_Busy(cAICharControl *self);
int cAICharControl_GetOrbitSide(cAICharControl *self);
vec3_t *cAICharControl_SideDir(cAICharControl *self, vec3_t *out, int side);
byte cAICharControl_GoalAtStart(cAICharControl *self);
byte cAICharControl_AtPathEnd(cAICharControl *self);
byte cAICharControl_IsDirect(cAICharControl *self);
byte cAICharControl_UpdateFollowPath(cAICharControl *self, float dt);
byte cAICharControl_UpdateFollowMode(cAICharControl *self, float dt);
byte cAICharControl_UpdateAway(cAICharControl *self, float dt);
byte cAICharControl_UpdateRoute(cAICharControl *self, float dt);
byte cAICharControl_UpdateDir(cAICharControl *self, float dt);
byte cAICharControl_UpdateGoTo(cAICharControl *self, float dt);
byte cAICharControl_UpdateGoToDirect(cAICharControl *self, float dt);
byte cAICharControl_Wait(cAICharControl *self, float dt);
/* the control of the AI of a character (0x19c of cAIChar) */
cAICharControl *cAIChar_GetControl(cAI *base);

/* the engine variables of the character AI */
extern cGVar GVar_DepthPathRouteInDoorAI;
#define g_DepthPathRouteInDoorAI (GVar_DepthPathRouteInDoorAI.value.i)	/* "DepthPathRouteInDoorAI" */
extern cGVar GVar_DistShirkCharAI;
#define g_DistShirkCharAI (GVar_DistShirkCharAI.value.f)		/* "DistShirkCharAI" */
extern cGVar GVar_MarginCollCharAI;
#define g_MarginCollCharAI (GVar_MarginCollCharAI.value.f)		/* "MarginCollCharAI" */
extern cGVar GVar_ShowShirkAI;
#define g_ShowShirkAI (GVar_ShowShirkAI.value.i)			/* "ShowShirkAI" */
extern cGVar GVar_ShowPathAI;
#define g_ShowPathAI (GVar_ShowPathAI.value.i)			/* "ShowPathAI" */
extern cGVar GVar_FactorDistTestColCharAI;
#define g_FactorDistTestColCharAI (GVar_FactorDistTestColCharAI.value.f)	/* "FactorDistTestColCharAI" */
extern cGVar GVar_AngAttackBackCharAI;
#define g_AngAttackBackCharAI (GVar_AngAttackBackCharAI.value.f)	/* "AngAttackBackCharAI" */

/* the A* search on CNodesWorld2D (AI/AStar2D.cpp, not in C): the path from pos to dest into path; noPath and
 * reach tell the AI why there is none, res the kind of the path */
byte CAStar2D_FindPath(void *astar, cNWVecList *path, const vec3_t *dest, const vec3_t *pos, float r,
	float dist, float h1, float h2, int soft, byte *noPath, byte *reach, int *res);
/* the points of visibility the body sees from pos (CNodesWorld2D_GetPointsVis) */
void CAStar2D_GetPointsVis(void *astar, const vec3_t *pos, const vec3_t **list, int max, int *count,
	float r, float h1, float h2);

/* ---- AI/AICharControl2D.cpp (src/game/AI/ICharControl2D.c) ---- */
cAICharControl *cAICharControl2D_ctor(void *mem, cEntity *ent, cAIChar *ai, int *arg);
void cAICharControl2D_dtor(cAICharControl2D *self);
void cAICharControl2D_Draw(cAICharControl2D *self);
/* toE and toGoal the ways on the floor to hitEnt and to the goal (back: from the goal); 1 below 90 degrees */
byte cAICharControl2D_Facing(cAICharControl2D *self, cEntity *hitEnt, vec3_t *toE, vec3_t *toGoal);
byte cAICharControl2D_TestLine(cAICharControl2D *self, const vec3_t *a, const vec3_t *b);
/* the way of twice the radius to the side not free */
byte cAICharControl2D_SideBlocked(cAICharControl2D *self, int side);
/* the follow of the target: straight while the way is free, else along the path; 0 when no path is found */
byte cAICharControl2D_UpdateFollow(cAICharControl2D *self);
/* at the end of the path: the event 2 (arrived), 4 (near), else 5 */
void cAICharControl2D_CheckArrived(cAICharControl2D *self);
/* the turn of the control to the goal (or along avoidDir) */
void cAICharControl2D_Turn(cAICharControl2D *self);
void cAICharControl2D_Walk(cAICharControl2D *self);
byte cAICharControl2D_FindPath(cAICharControl2D *self, const vec3_t *p);
byte cAICharControl2D_FindRoute(cAICharControl2D *self);
float cAICharControl2D_Dist(cAICharControl2D *self, const vec3_t *a, const vec3_t *b);
float cAICharControl2D_DistSq(cAICharControl2D *self, const vec3_t *a, const vec3_t *b);
/* the distance to keep from hitEnt (the radii and DistShirkCharAI) */
float cAICharControl2D_KeepDist(cAICharControl2D *self, cEntity *hitEnt);
/* the distance on the floor to hitEnt plus MarginCollCharAI */
float cAICharControl2D_StrafeDist(cAICharControl2D *self, cEntity *hitEnt);
byte cAICharControl2D_InFront(cAICharControl2D *self, cEntity *e, float ang);
byte cAICharControl2D_Behind(cAICharControl2D *self, cEntity *e, float ang);
/* the angle at dest between the character and the point of the path below pi / 8 */
byte cAICharControl2D_GoalAhead(cAICharControl2D *self);
/* a point of visibility of the graph seen from p */
byte cAICharControl2D_OnFloor(cAICharControl2D *self, const vec3_t *p);
/* e further than the distance to keep plus 1 */
byte cAICharControl2D_Near(cAICharControl2D *self, cEntity *e);
/* dest within the distance to keep from hitEnt */
byte cAICharControl2D_InWay(cAICharControl2D *self, vec3_t dest, cEntity *hitEnt);
/* the corridor of the body from the floor below a to the floor below b free: the line between them raised, and 7
 * lines of the length FactorDistTestColCharAI * radius around it (to the sides, up, the diagonals), then
 * FloorAhead(b); the corridor into debugSegs. To dest the test of TestPathEnd */
byte cAICharControl2D_TestPath(cAICharControl2D *self, const vec3_t *a, const vec3_t *b, float r);
/* TestPath with the 7 lines aimed at b, the step of a forward walk (TestStep with r), then FloorAhead(b) */
byte cAICharControl2D_TestPathEnd(cAICharControl2D *self, const vec3_t *a, const vec3_t *b, float r);
/* the 7 lines around a along the way from b to c (at most the radius plus 5 long); r not used */
byte cAICharControl2D_TestStep(cAICharControl2D *self, const vec3_t *a, const vec3_t *b, const vec3_t *c,
	float r);
/* the corridor of the walk back from a to b: 5 lines of the length FactorDistTestColBackCharAI * radius around
 * the line between the floors, back along it (not down); r not used */
byte cAICharControl2D_TestSides(cAICharControl2D *self, const vec3_t *a, const vec3_t *b, float r);
byte cAICharControl2D_FloorAhead(cAICharControl2D *self, const vec3_t *p);
/* the floor below p raised by the floor distance of the character into out; 0 (out = p) without one */
byte cAICharControl2D_FloorAt(cAICharControl2D *self, const vec3_t *p, vec3_t *out);
void cAICharControl2D_StepGoal(cAICharControl2D *self, vec3_t *p);
byte cAICharControl2D_TryStrafe(cAICharControl2D *self, int side, cEntity *hitEnt, byte back, float keep,
	byte fwd, float sd, byte e2);
byte cAICharControl2D_StrafeBack(cAICharControl2D *self, const vec3_t *pos, const vec3_t *away,
	const vec3_t *dir, float dist, byte left, byte flag);
byte cAICharControl2D_StrafeFwd(cAICharControl2D *self, const vec3_t *pos, const vec3_t *away,
	const vec3_t *dir, float dist, byte left, byte flag);

/* CAStar_FindPath and CAStar_GetPointsVis (the same arguments on CNodesWorld): nodesworld.h */

/* ---- AI/AICharControl3D.cpp (src/game/AI/ICharControl3D.c): the slots as in 2D, in all three axes ---- */
cAICharControl *cAICharControl3D_ctor(void *mem, cEntity *ent, cAIChar *ai, int *arg);
void cAICharControl3D_dtor(cAICharControl3D *self);
void cAICharControl3D_Draw(cAICharControl3D *self);
byte cAICharControl3D_Facing(cAICharControl3D *self, cEntity *hitEnt, vec3_t *toE, vec3_t *toGoal);
/* the segment free of the level */
byte cAICharControl3D_TestLine(cAICharControl3D *self, const vec3_t *a, const vec3_t *b);
/* the segment a b free and 8 lines around it from a: along it (at most FactorDistTestColCharAI * radius long),
 * aimed at b (TestPathEnd); the lines back from a (at most the radius plus 5) for TestSides. The corridor into
 * debugSegs. TestPath to dest is TestPathEnd */
byte cAICharControl3D_TestPath(cAICharControl3D *self, const vec3_t *a, const vec3_t *b, float r);
byte cAICharControl3D_TestPathEnd(cAICharControl3D *self, const vec3_t *a, const vec3_t *b, float r);
byte cAICharControl3D_TestSides(cAICharControl3D *self, const vec3_t *a, const vec3_t *b, float r);
/* slot 100 of the 3D control: the 8 lines around a along the way to b (at most the radius plus 5); r not used */
byte cAICharControl3D_TestStep(cAICharControl3D *self, const vec3_t *a, const vec3_t *b, float r);
/* the way of 1.5 radius to the side (3 up, 4 down) not free */
byte cAICharControl3D_SideBlocked(cAICharControl3D *self, int side);
byte cAICharControl3D_UpdateFollow(cAICharControl3D *self);
void cAICharControl3D_CheckArrived(cAICharControl3D *self);
void cAICharControl3D_Turn(cAICharControl3D *self);
/* walkDir from the distance to the target alone */
void cAICharControl3D_Walk(cAICharControl3D *self);
byte cAICharControl3D_FindPath(cAICharControl3D *self, const vec3_t *p);
byte cAICharControl3D_FindRoute(cAICharControl3D *self);
float cAICharControl3D_Dist(cAICharControl3D *self, const vec3_t *a, const vec3_t *b);
float cAICharControl3D_DistSq(cAICharControl3D *self, const vec3_t *a, const vec3_t *b);
/* the 2D distance to keep, at most the distance to hitEnt plus 10 */
float cAICharControl3D_KeepDist(cAICharControl3D *self, cEntity *hitEnt);
/* the two radii plus MarginCollCharAI */
float cAICharControl3D_StrafeDist(cAICharControl3D *self, cEntity *hitEnt);
byte cAICharControl3D_InFront(cAICharControl3D *self, cEntity *e, float ang);
byte cAICharControl3D_Behind(cAICharControl3D *self, cEntity *e, float ang);
byte cAICharControl3D_GoalAhead(cAICharControl3D *self);
byte cAICharControl3D_OnFloor(cAICharControl3D *self, const vec3_t *p);
byte cAICharControl3D_Near(cAICharControl3D *self, cEntity *e);
byte cAICharControl3D_InWay(cAICharControl3D *self, vec3_t dest, cEntity *hitEnt);
void cAICharControl3D_StepGoal(cAICharControl3D *self, vec3_t *p);
byte cAICharControl3D_TryStrafe(cAICharControl3D *self, int side, cEntity *hitEnt, byte back, float keep,
	byte fwd, float sd, byte e2);
byte cAICharControl3D_StrafeBack(cAICharControl3D *self, const vec3_t *pos, const vec3_t *away, const vec3_t *dir, float dist, byte left, byte flag);
byte cAICharControl3D_StrafeFwd(cAICharControl3D *self, const vec3_t *pos, const vec3_t *away, const vec3_t *dir, float dist, byte left, byte flag);

#define CAICONTROLSHOOT_SIZE sizeof(cAIControlShoot)		/* the bishop and the gear */
#define CAICONTROLSENTINEL_SIZE sizeof(cAIControlSentinel)		/* without strafeTime and strafeAng */

/* the bishop, the gear and the sentinel strafe while a Bishop, a Gear (or a Dtritus) aims at them, or (the sentinel)
 * when they were hit */
typedef struct cAIControlShoot {
	cAICharControl base;
	cSlotController slotSide;		/* the side of the strafe blocked (0.2 s) */
	int side;			/* the strafe: 0 none, 1, 2 */
	float time;			/* since the last strafe started */
	byte other;			/* the side was blocked: the other side next */
	byte pad_379[3];
	float strafeTime;		/* TimeStrafeUnderAttack<Class>AI (the sentinel: the gvar itself) */
	float strafeAng;		/* AngleStrafeUnderAttack<Class>AI */
} cAIControlShoot;
typedef cAIControlShoot cAIControlBishop;
typedef cAIControlShoot cAIControlGear;

/* the classes that add no members to the 2D or the 3D control */
typedef cAICharControl2D cAIControlBankDirector;
typedef cAICharControl2D cAIControlBankMaster;
typedef cAICharControl3D cAIControlMessenger;
typedef cAICharControl3D cAIControlNurse;

/* the sentinel: the 3D control with the strafe of cAIControlShoot, but the parameters are the gvars (0x37c bytes);
 * its functions work on the cAIControlShoot they share with the bishop and the gear */
typedef struct cAIControlSentinel {
	cAICharControl3D base;
	cSlotController slotSide;		/* the side of the strafe blocked (0.2 s) */
	int side;			/* the strafe: 0 none, 1, 2 */
	float time;			/* since the last strafe started */
	byte other;			/* the side was blocked: the other side next */
	byte pad_379[3];
} cAIControlSentinel;

cAICharControl *cAIControlBankDirector_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg);
cAICharControl *cAIControlBankMaster_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg);
cAICharControl *cAIControlBishop_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg);
cAICharControl *cAIControlGear_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg);
cAICharControl *cAIControlMessenger_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg);
cAICharControl *cAIControlNurse_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg);
cAICharControl *cAIControlSentinel_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg);

typedef cAICharControl *(*cAICharControlCtor_t)(void *mem, cEntity *e, cAIChar *ai, int *arg);

/* the constructors of the cAIChar subclasses: the move control of cAIChar deleted, one of the class in its place */
static inline void cAIChar_NewControl(cAIChar *self, uint32_t size, cAICharControlCtor_t ctor, cEntity *e, int *arg) {
	if (self->control) {
		AICC_Free(self->control);
	}
	void *mem = operator_new(size);
	cAICharControl *ControlMove = mem ? ctor(mem, e, self, arg) : NULL;
	self->control = ControlMove;
	CHECK(ControlMove);
}

#endif
