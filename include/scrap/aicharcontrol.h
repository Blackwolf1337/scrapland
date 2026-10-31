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

#include "scrap/ai.h"
#include "scrap/fxmisc.h"
#include "scrap/nodesworld.h"
#include "scrap/objentity.h"
#include "scrap/scrapcollision.h"
#include "scrap/slotcontroller.h"
#include "scrap/walkcharentity.h"

#define CAICHARCONTROL_SIZE 0x360
#define CAICHARCONTROL2D_VFTABLE GAME_VFTABLE(cAICharControl2D_vftable, 0x3a5acc)
#define CAICHARCONTROL3D_VFTABLE GAME_VFTABLE(cAICharControl3D_vftable, 0x3a5d18)

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
	cAICharControl *(__thiscall *Delete)(cAICharControl *self, byte flags);			/* 0x00 */
	void(__thiscall *Draw)(cAICharControl *self);						/* 0x04 */
	int(__thiscall *GetWalkDir)(cAICharControl *self);					/* 0x08 */
	int(__thiscall *Busy)(cAICharControl *self);						/* 0x0c the strafe side */
	int(__thiscall *GetOrbitSide)(cAICharControl *self);					/* 0x10 */
	/* the frame: the checks of the AI, then the handler of the mode; 0 when the target is gone */
	byte(__thiscall *Update)(cAICharControl *self, float dt);				/* 0x14 */
	byte (*SetMove)(cAICharControl *self, int mode, ...);					/* 0x18 */
	void(__thiscall *Reset)(cAICharControl *self);						/* 0x1c */
	void(__thiscall *SetHold)(cAICharControl *self, byte on);				/* 0x20 */
	/* the turn, walk, strafe and jump values of the control 0 */
	void(__thiscall *Stop)(cAICharControl *self);						/* 0x24 */
	byte(__thiscall *IsDirect)(cAICharControl *self);					/* 0x28 */
	byte(__thiscall *IsDirect2)(cAICharControl *self);					/* 0x2c */
	void(__thiscall *GetGoal)(cAICharControl *self, vec3_t *out);				/* 0x30 */
	void(__thiscall *GetDest)(cAICharControl *self, vec3_t *out);				/* 0x34 */
	/* the corridor of the body from a to b free (the radius r) */
	byte(__thiscall *TestPath)(cAICharControl *self, const vec3_t *a, const vec3_t *b, float r);	/* 0x38 */
	byte(__thiscall *TestLine)(cAICharControl *self, const vec3_t *a, const vec3_t *b);		/* 0x3c */
	byte(__thiscall *TestSides)(cAICharControl *self, const vec3_t *a, const vec3_t *b, float r);	/* 0x40 */
	void(__thiscall *GetAvoidA)(cAICharControl *self, vec3_t *out);				/* 0x44 */
	void(__thiscall *GetAvoidB)(cAICharControl *self, vec3_t *out);				/* 0x48 */
	void(__thiscall *ClearAvoid)(cAICharControl *self);					/* 0x4c */
	void(__thiscall *SetSpeed)(cAICharControl *self, float speed);				/* 0x50 */
	void(__thiscall *RestartSlots)(cAICharControl *self);					/* 0x54 */
	byte(__thiscall *IsBlocked)(cAICharControl *self);					/* 0x58 */
	byte(__thiscall *GetE4)(cAICharControl *self);						/* 0x5c */
	void *(__thiscall *GetAvoidEnt)(cAICharControl *self);					/* 0x60 */
	void(__thiscall *Nop)(cAICharControl *self);						/* 0x64 */
	/* the point walked to is the last one before the end (none: no point) */
	byte(__thiscall *AtPathEnd)(cAICharControl *self);					/* 0x68 */
	byte(__thiscall *GoalAtStart)(cAICharControl *self);					/* 0x6c */
	/* the target of a follow or away move gone: the event 3, 0 */
	byte(__thiscall *CheckTarget)(cAICharControl *self);					/* 0x70 */
	void(__thiscall *SetTurnSpeed)(cAICharControl *self, float speed);			/* 0x74 */
	/* the strafe around a character at pos (the two sides of TryStrafe) */
	byte(__thiscall *StrafeBack)(cAICharControl *self, const vec3_t *pos, const vec3_t *away, const vec3_t *dir,
		float dist, byte left, byte flag);						/* 0x78 */
	byte(__thiscall *StrafeFwd)(cAICharControl *self, const vec3_t *pos, const vec3_t *away, const vec3_t *dir,
		float dist, byte left, byte flag);						/* 0x7c */
	byte(__thiscall *TryStrafe)(cAICharControl *self, int side, cEntity *e, byte a, float b, byte c, float d,
		byte e2);									/* 0x80 */
	byte(__thiscall *Facing)(cAICharControl *self, cEntity *e, vec3_t *toE, vec3_t *toGoal);	/* 0x84 */
	byte(__thiscall *InWay)(cAICharControl *self, vec3_t dest, cEntity *e);			/* 0x88 */
	byte(__thiscall *Near)(cAICharControl *self, cEntity *e);				/* 0x8c */
	float(__thiscall *KeepDist)(cAICharControl *self, cEntity *e);				/* 0x90 */
	float(__thiscall *StrafeDist)(cAICharControl *self, cEntity *e);				/* 0x94 */
	byte(__thiscall *OnFloor)(cAICharControl *self, const vec3_t *p);			/* 0x98 */
	byte(__thiscall *GoalAhead)(cAICharControl *self);					/* 0x9c */
	byte(__thiscall *TestPathEnd)(cAICharControl *self, const vec3_t *a, const vec3_t *b, float r);	/* 0xa0 */
	float(__thiscall *Dist)(cAICharControl *self, const vec3_t *a, const vec3_t *b);		/* 0xa4 */
	float(__thiscall *DistSq)(cAICharControl *self, const vec3_t *a, const vec3_t *b);		/* 0xa8 */
	void(__thiscall *Away)(cAICharControl *self, Nto1Node *target, byte flag);		/* 0xac */
	byte(__thiscall *UpdateAway)(cAICharControl *self, float dt);				/* 0xb0 */
	/* the view near the way to the target: the event 6 */
	void(__thiscall *AwayCheck)(cAICharControl *self);					/* 0xb4 */
	/* the walk back from the target while it comes near, none while another character sees it */
	void(__thiscall *AwayWalk)(cAICharControl *self);					/* 0xb8 */
	/* e within ang of the back (the front) of the view, on the floor */
	byte(__thiscall *Behind)(cAICharControl *self, cEntity *e, float ang);			/* 0xbc */
	byte(__thiscall *InFront)(cAICharControl *self, cEntity *e, float ang);			/* 0xc0 */
	byte(__thiscall *Route)(cAICharControl *self, const vec3_t *p, float dist, float speed, byte a, byte b);	/* 0xc4 */
	void(__thiscall *SetDir)(cAICharControl *self, const vec3_t *dir);			/* 0xc8 */
	byte(__thiscall *UpdateDir)(cAICharControl *self, float dt);				/* 0xcc */
	/* the view near dir: the event 6 */
	void(__thiscall *DirCheck)(cAICharControl *self);					/* 0xd0 */
	/* the view turned to the goal (turnGain, turnStep) */
	void(__thiscall *DirTurn)(cAICharControl *self);					/* 0xd4 */
	void(__thiscall *CheckArrived)(cAICharControl *self);					/* 0xd8 */
	/* 0 (and the control stopped) when the next segment of the path is cut near the character */
	byte(__thiscall *CanMove)(cAICharControl *self);					/* 0xdc */
	byte(__thiscall *GoTo)(cAICharControl *self, const vec3_t *p, float dist, float speed);	/* 0xe0 */
	byte(__thiscall *GoToDirect)(cAICharControl *self, const vec3_t *p, float dist, float speed);	/* 0xe4 */
	byte(__thiscall *FindPath)(cAICharControl *self, const vec3_t *p);			/* 0xe8 */
	/* the path is the line to p */
	byte(__thiscall *DirectPath)(cAICharControl *self, const vec3_t *p);			/* 0xec */
	/* p moved a random step while the way stays free */
	void(__thiscall *StepGoal)(cAICharControl *self, vec3_t *p);				/* 0xf0 */
	byte(__thiscall *UpdateGoTo)(cAICharControl *self, float dt);				/* 0xf4 */
	byte(__thiscall *UpdateRoute)(cAICharControl *self, float dt);				/* 0xf8 */
	byte(__thiscall *UpdateGoToDirect)(cAICharControl *self, float dt);			/* 0xfc */
	/* the goal on the first of NumPointsSearchSegmentCharAI points from a to b the body reaches */
	byte(__thiscall *GoalOnLine)(cAICharControl *self, const vec3_t *a, const vec3_t *b);	/* 0x100 */
	/* near the goal (or with the goal lost) the goal on to the next point of the path, or between */
	void(__thiscall *NextGoal)(cAICharControl *self);					/* 0x104 */
	/* the goal on to the last point of the path the body reaches, or between it and the next */
	void(__thiscall *NextGoalPath)(cAICharControl *self);					/* 0x108 */
	/* the goal on along the path while the way is cut; the path searched again when no point is free */
	byte(__thiscall *UpdatePath)(cAICharControl *self);					/* 0x10c */
	/* at the end of a path that does not reach dest: the line to dest (stuck, pathRes 2), 0 then */
	byte(__thiscall *CheckPathEnd)(cAICharControl *self);					/* 0x110 */
	byte(__thiscall *UpdateRoutePath)(cAICharControl *self);				/* 0x114 */
	void(__thiscall *Turn)(cAICharControl *self);						/* 0x118 */
	/* the strafe around the characters in the way (slotStrafe) */
	void(__thiscall *Strafe)(cAICharControl *self);						/* 0x11c */
	/* the characters near: link to the first one in the way that can be passed */
	byte(__thiscall *FindAvoid)(cAICharControl *self);					/* 0x120 */
	byte(__thiscall *Avoid)(cAICharControl *self, cEntity *e, byte *found);			/* 0x124 */
	/* the strafe around the linked character (or the one the character ran into) */
	void(__thiscall *UpdateAvoid)(cAICharControl *self);					/* 0x128 */
	/* walking characters within MarginDetectCollCharAI */
	byte(__thiscall *CharsNear)(cAICharControl *self);					/* 0x12c */
	byte(__thiscall *SideBlocked)(cAICharControl *self, int side);				/* 0x130 */
	/* the unit vector to the side (out returned) */
	vec3_t *(__thiscall *SideDir)(cAICharControl *self, vec3_t *out, int side);		/* 0x134 */
	void(__thiscall *WalkFwd)(cAICharControl *self);					/* 0x138 */
	byte(__thiscall *CheckGoTo)(cAICharControl *self);					/* 0x13c */
	byte(__thiscall *CheckRoute)(cAICharControl *self);					/* 0x140 */
	byte(__thiscall *CheckFollow)(cAICharControl *self);					/* 0x144 */
	byte(__thiscall *FindRoute)(cAICharControl *self);					/* 0x148 */
	/* the view turned to the target (or along avoidDir) */
	void(__thiscall *TurnFollow)(cAICharControl *self);					/* 0x14c */
	byte(__thiscall *Follow)(cAICharControl *self, Nto1Node *target, float near, float far, float speed);	/* 0x150 */
	byte(__thiscall *UpdateFollowMode)(cAICharControl *self, float dt);			/* 0x154 */
	/* the strafe of CheckWait instead of the walk; 0 when there is none */
	byte(__thiscall *Wait)(cAICharControl *self, float dt);					/* 0x158 */
	/* the strafe while the target goes out of sight (waitTime up to TimeStrafeLostObjCharAI) */
	byte(__thiscall *CheckWait)(cAICharControl *self);					/* 0x15c */
	void(__thiscall *StrafeWalk)(cAICharControl *self);					/* 0x160 */
	byte(__thiscall *Vf89)(cAICharControl *self);						/* 0x164 0 */
	void(__thiscall *Strafe2)(cAICharControl *self);					/* 0x168 */
	void(__thiscall *Walk)(cAICharControl *self);						/* 0x16c */
	void(__thiscall *Walk2)(cAICharControl *self);						/* 0x170 */
	byte(__thiscall *UpdateFollow)(cAICharControl *self);					/* 0x174 */
	byte(__thiscall *UpdateFollowB)(cAICharControl *self);					/* 0x178 */
	byte(__thiscall *FollowPath)(cAICharControl *self, Nto1Node *target, float near, float far, float speed);	/* 0x17c */
	byte(__thiscall *UpdateFollowPath)(cAICharControl *self, float dt);			/* 0x180 */
	/* the strafe around the target while it looks at the character (orbitSide) */
	void(__thiscall *Orbit)(cAICharControl *self);						/* 0x184 */
	/* the side of the orbit at dist from the target */
	byte(__thiscall *StartOrbit)(cAICharControl *self, float dist);				/* 0x188 */
	void(__thiscall *Stop2)(cAICharControl *self);						/* 0x18c */
} cAICharControlVtbl;
SD_OFFSET(cAICharControlVtbl, Update, 0x14);
SD_OFFSET(cAICharControlVtbl, SetMove, 0x18);
SD_OFFSET(cAICharControlVtbl, Stop, 0x24);
SD_OFFSET(cAICharControlVtbl, TestPath, 0x38);
SD_OFFSET(cAICharControlVtbl, GetAvoidA, 0x44);
SD_OFFSET(cAICharControlVtbl, ClearAvoid, 0x4c);
SD_OFFSET(cAICharControlVtbl, GetAvoidEnt, 0x60);
SD_OFFSET(cAICharControlVtbl, Nop, 0x64);
SD_OFFSET(cAICharControlVtbl, CheckTarget, 0x70);
SD_OFFSET(cAICharControlVtbl, SetTurnSpeed, 0x74);
SD_OFFSET(cAICharControlVtbl, StrafeFwd, 0x7c);
SD_OFFSET(cAICharControlVtbl, TryStrafe, 0x80);
SD_OFFSET(cAICharControlVtbl, TestPathEnd, 0xa0);
SD_OFFSET(cAICharControlVtbl, Away, 0xac);
SD_OFFSET(cAICharControlVtbl, AwayWalk, 0xb8);
SD_OFFSET(cAICharControlVtbl, Behind, 0xbc);
SD_OFFSET(cAICharControlVtbl, UpdateDir, 0xcc);
SD_OFFSET(cAICharControlVtbl, CheckArrived, 0xd8);
SD_OFFSET(cAICharControlVtbl, CanMove, 0xdc);
SD_OFFSET(cAICharControlVtbl, GoTo, 0xe0);
SD_OFFSET(cAICharControlVtbl, UpdateGoTo, 0xf4);
SD_OFFSET(cAICharControlVtbl, NextGoal, 0x104);
SD_OFFSET(cAICharControlVtbl, CheckPathEnd, 0x110);
SD_OFFSET(cAICharControlVtbl, Turn, 0x118);
SD_OFFSET(cAICharControlVtbl, Strafe, 0x11c);
SD_OFFSET(cAICharControlVtbl, Avoid, 0x124);
SD_OFFSET(cAICharControlVtbl, CharsNear, 0x12c);
SD_OFFSET(cAICharControlVtbl, SideBlocked, 0x130);
SD_OFFSET(cAICharControlVtbl, WalkFwd, 0x138);
SD_OFFSET(cAICharControlVtbl, FindRoute, 0x148);
SD_OFFSET(cAICharControlVtbl, TurnFollow, 0x14c);
SD_OFFSET(cAICharControlVtbl, Follow, 0x150);
SD_OFFSET(cAICharControlVtbl, Wait, 0x158);
SD_OFFSET(cAICharControlVtbl, Vf89, 0x164);
SD_OFFSET(cAICharControlVtbl, Walk, 0x16c);
SD_OFFSET(cAICharControlVtbl, UpdateFollow, 0x174);
SD_OFFSET(cAICharControlVtbl, FollowPath, 0x17c);
SD_OFFSET(cAICharControlVtbl, Orbit, 0x184);
SD_OFFSET(cAICharControlVtbl, Stop2, 0x18c);
SD_SIZE(cAICharControlVtbl, 100 * 4);

/* the slots the 2D and the 3D control add */
typedef struct cAICharControl2DVtbl {
	cAICharControlVtbl base;
	byte(__thiscall *FloorAhead)(cAICharControl *self, const vec3_t *p);			/* 0x190 */
	byte(__thiscall *FloorAt)(cAICharControl *self, const vec3_t *p, vec3_t *out);		/* 0x194 */
	byte(__thiscall *TestStep)(cAICharControl *self, const vec3_t *a, const vec3_t *b, const vec3_t *c, float r);	/* 0x198 */
} cAICharControl2DVtbl;
SD_SIZE(cAICharControl2DVtbl, 103 * 4);

/* the AI of the character (cAIChar, include/scrap/ai.h): the control calls GetTarget (0x38) and Event (0x58) */
typedef cAICharVtbl cAICharCtlOwnerVtbl;

static inline const cAICharCtlOwnerVtbl *AICC_OwnerVtbl(cAIChar *ai) {
	return ai->vftable;
}

/* a cSlotController inside the control (0x10 bytes) */
typedef struct cAICCSlot {
	byte raw[CSLOTCONTROLLER_SIZE];
} cAICCSlot;

static inline cSlotController *AICC_Slot(cAICCSlot *s) {
	return (cSlotController *)s->raw;
}

/* a segment of the debug view */
typedef struct cAICCSeg {
	vec3_t a, b;
} cAICCSeg;

struct cAICharControl {
	const cAICharControlVtbl *vftable;	/* 0x000 */
	cAICCSlot slotWalk;	/* 0x004 the distance checks of Walk and AwayWalk (0.2 s) */
	cAICCSlot slotSee2;	/* 0x014 AwayWalk: the other characters that see it (0.2 s) */
	cAICCSlot slotStrafe;	/* 0x024 the strafe around the characters in the way (0.133 s) */
	cAICCSlot slotStop;	/* 0x034 CanMove (0.2 s) */
	cAIChar *ai;			/* 0x044 the AI of the character */
	cEntity *ent;			/* 0x048 the character (cWalkCharEntity) */
	cWalkCharControl *ctrl;		/* 0x04c the control of the character the AI steers */
	float radius;			/* 0x050 the radius of the character */
	float arriveDist;		/* 0x054 the goal is reached that near */
	float arriveR;			/* 0x058 the smaller of radius and arriveDist */
	float dt;			/* 0x05c the time of the frame */
	float diffX;			/* 0x060 the angles to the goal (Turn) */
	float diffY;			/* 0x064 */
	vec3_t dest;			/* 0x068 the end of the move */
	int mode;			/* 0x074 AICC_ */
	int prevMode;			/* 0x078 */
	int soft;			/* 0x07c the argument of the constructor: the path search may give up first */
	float turnX;			/* 0x080 the vertical turn of the last frame */
	float turnY;			/* 0x084 the horizontal turn of the last frame */
	byte targetChanged;		/* 0x088 a follow move got a new target */
	byte pad_89[3];
	float turnGain;			/* 0x08c the turn for an angle of pi */
	float turnStep;			/* 0x090 the change of the turn in a frame at most */
	int walkDir;			/* 0x094 0 stop, 1 forward, 2 back */
	int side;			/* 0x098 the strafe around a character: 0 none, 1, 2 */
	int strafeDir;			/* 0x09c the strafe of CheckWait: 0 none, 1, 2 */
	byte flying;			/* 0x0a0 the argument of FOLLOW */
	byte stopped;			/* 0x0a1 CanMove stopped the control */
	byte pad_a2[2];
	vec3_t avoidA;			/* 0x0a4 the segment of the last path check (the character, the goal), */
	vec3_t avoidB;			/* 0x0b0 -FLT_MAX after ClearAvoid */
	byte hold;			/* 0x0bc */
	byte newPath;			/* 0x0bd */
	byte pad_be[2];
	float avoidTime;		/* 0x0c0 the time of the strafe around a character */
	Nto1Node link;			/* 0x0c4 to the character avoided (the head at 0x254 of it), data the AI */
	byte blocked;			/* 0x0d4 */
	byte blockedByChar;		/* 0x0d5 other characters are in the way (the collision event may fire) */
	byte pad_d6[2];
	vec3_t avoidDir;		/* 0x0d8 0 for none */
	byte avoidFound;		/* 0x0e4 FindAvoid found a character */
	byte pad_e5[3];
	float collEventTime;		/* 0x0e8 the times since the last collision event, stop, test (TimeEventCollCharAI ..) */
	float collStopTime;		/* 0x0ec */
	float collTestTime;		/* 0x0f0 */
	float speed;			/* 0x0f4 the walk value */
	int lifeMark;			/* 0x0f8 the life of the character */
	cAICCSlot slotPath;	/* 0x0fc the path is searched again (0.133 s) */
	vec3_t goal;			/* 0x10c the next point of the path */
	cNWVecList path;		/* 0x118 */
	vec3_t *pathPoint;		/* 0x130 the point of the path walked to */
	vec3_t *prevPoint;		/* 0x134 the one before it UpdatePath skipped */
	float turnXNew;			/* 0x138 */
	float turnYNew;			/* 0x13c */
	byte unused_140;			/* 0x140 */
	byte goalLost;			/* 0x141 the goal was not reachable at the last UpdatePath */
	byte pad_142[2];
	cAICCSlot slotDirect;	/* 0x144 the direct follow checks the way (0.2 s) */
	cAICCSlot slotSee;	/* 0x154 the path follow looks for the direct way (0.2 s) */
	cAICCSlot slotRepath;	/* 0x164 the path to the target searched again (5 s) */
	cAICCSlot slotNear;	/* 0x174 the end of the path near: searched again (0.2 s) */
	cAICCSlot slotOrbit;	/* 0x184 the side of the orbit blocked (0.2 s) */
	cAICCSlot slotLost;	/* 0x194 the path searched again while stuck (3 s) */
	cAICCSlot slotWait;	/* 0x1a4 CheckWait (0.2 s) */
	Nto1Node *target;		/* 0x1b4 the link to the entity to follow (objDin) */
	cEntity *targetEnt;		/* 0x1b8 its entity when the move started */
	float farDist;			/* 0x1bc */
	float nearDist;			/* 0x1c0 */
	vec3_t v1c4;			/* 0x1c4 */
	byte direct;			/* 0x1d0 the follow walks straight to the target */
	byte waitStart;			/* 0x1d1 CheckWait has its start positions */
	byte pad_1d2[2];
	float waitTime;			/* 0x1d4 the time of the strafe of CheckWait */
	vec3_t waitPos;			/* 0x1d8 the character and the target at the start of the wait */
	vec3_t waitTargetPos;		/* 0x1e4 */
	vec3_t lastTargetPos;		/* 0x1f0 the target at the last Orbit */
	int pathRes;			/* 0x1fc the result of the path search (2: the end is not reachable) */
	byte stuck;			/* 0x200 the follow sees no way */
	byte avoidChar;			/* 0x201 FindAvoid linked a character */
	byte pad_202[2];
	float viewGain;			/* 0x204 the turn of the view to the dest (VelRotDynamic...) */
	float viewStep;			/* 0x208 (LimVelRotDynamic...) */
	int orbitSide;			/* 0x20c the strafe around the target: 0 none, 1, 2 */
	byte orbitStart;		/* 0x210 StartOrbit may run */
	byte pad_211[3];
	vec3_t orbitDir;		/* 0x214 the back of the target at the start of the orbit (y 0) */
	vec3_t dir;			/* 0x220 the direction of AICC_DIR */
	byte awayFlag;			/* 0x22c */
	byte seen;			/* 0x22d AwayWalk: another character sees it */
	byte routeA;			/* 0x22e */
	byte routeB;			/* 0x22f */
	vec3_t routePoint;		/* 0x230 */
	cNWNode *routeNode;		/* 0x23c the node of the route BuildPath reached */
	cAICCSeg debugSegs[9];		/* 0x240 the corridor of the last path test */
	cNWVecList debug318;		/* 0x318 the points of the strafe tests */
	cNWVecList debug330;		/* 0x330 */
	cNWSegList debug348;		/* 0x348 the segments of the path tests (ShowTestCanGoAI) */
};
SD_OFFSET(cAICharControl, ai, 0x44);
SD_OFFSET(cAICharControl, dest, 0x68);
SD_OFFSET(cAICharControl, turnGain, 0x8c);
SD_OFFSET(cAICharControl, strafeDir, 0x9c);
SD_OFFSET(cAICharControl, avoidA, 0xa4);
SD_OFFSET(cAICharControl, avoidTime, 0xc0);
SD_OFFSET(cAICharControl, link, 0xc4);
SD_OFFSET(cAICharControl, avoidDir, 0xd8);
SD_OFFSET(cAICharControl, collEventTime, 0xe8);
SD_OFFSET(cAICharControl, speed, 0xf4);
SD_OFFSET(cAICharControl, slotPath, 0xfc);
SD_OFFSET(cAICharControl, goal, 0x10c);
SD_OFFSET(cAICharControl, path, 0x118);
SD_OFFSET(cAICharControl, pathPoint, 0x130);
SD_OFFSET(cAICharControl, goalLost, 0x141);
SD_OFFSET(cAICharControl, slotDirect, 0x144);
SD_OFFSET(cAICharControl, slotWait, 0x1a4);
SD_OFFSET(cAICharControl, target, 0x1b4);
SD_OFFSET(cAICharControl, direct, 0x1d0);
SD_OFFSET(cAICharControl, waitTime, 0x1d4);
SD_OFFSET(cAICharControl, lastTargetPos, 0x1f0);
SD_OFFSET(cAICharControl, pathRes, 0x1fc);
SD_OFFSET(cAICharControl, orbitSide, 0x20c);
SD_OFFSET(cAICharControl, orbitDir, 0x214);
SD_OFFSET(cAICharControl, dir, 0x220);
SD_OFFSET(cAICharControl, seen, 0x22d);
SD_OFFSET(cAICharControl, routePoint, 0x230);
SD_OFFSET(cAICharControl, debugSegs, 0x240);
SD_OFFSET(cAICharControl, debug318, 0x318);
SD_OFFSET(cAICharControl, debug348, 0x348);
SD_SIZE(cAICharControl, CAICHARCONTROL_SIZE);
/* the 2D and the 3D control add only vtable slots (both 0x360 bytes, the constructor of cAIChar) */
typedef cAICharControl cAICharControl2D;
typedef cAICharControl cAICharControl3D;

static inline const cAICharControlVtbl *AICC_Vtbl(const cAICharControl *self) {
	return self->vftable;
}

static inline const cAICharControl2DVtbl *AICC_Vtbl2D(const cAICharControl *self) {
	return (const cAICharControl2DVtbl *)self->vftable;
}

/* the entity the target link points to (objDin->GetRoot()) */
static inline cEntity *AICC_TargetEnt(const cAICharControl *self) {
	return (cEntity *)self->target->target;
}

static inline const vec3_t *AICC_Pos(cAICharControl *self, vec3_t *buf) {
	return Obj_Vtbl(self->ent)->base.GetPos(self->ent, buf);
}

static inline const vec3_t *AICC_EntPos(cEntity *e, vec3_t *buf) {
	return Obj_Vtbl(e)->base.GetPos(e, buf);
}

/* the x87 arithmetic of the originals on the float returns (st0), in the precision the game set: a + b, a * k,
 * (a + b) * k, (a + b) + c */
static inline float AICC_X87Add(float a, float b) {
	float r;

	__asm__("fadds %2" : "=t"(r) : "0"(a), "m"(b));
	return r;
}

static inline float AICC_X87Mul(float a, float k) {
	float r;

	__asm__("fmuls %2" : "=t"(r) : "0"(a), "m"(k));
	return r;
}

/* m - a */
static inline float AICC_X87SubR(float a, float m) {
	float r;

	__asm__("fsubrs %2" : "=t"(r) : "0"(a), "m"(m));
	return r;
}

/* a * b stored as a double */
static inline double AICC_X87MulD(float a, float b) {
	double r;

	__asm__("fmuls %2" : "=t"(r) : "0"(a), "m"(b));
	return r;
}

static inline float AICC_X87AddMul(float a, float b, float k) {
	float r;

	__asm__("fadds %2\n\tfmuls %3" : "=t"(r) : "0"(a), "m"(b), "m"(k));
	return r;
}

static inline float AICC_X87AddAdd(float a, float b, float c) {
	float r;

	__asm__("fadds %2\n\tfadds %3" : "=t"(r) : "0"(a), "m"(b), "m"(c));
	return r;
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
void __thiscall cAICharControl_ClearAvoid(cAICharControl *self);
/* the strafe around e (a character in the way): 1 when the control strafes, *found 1 when e is in the way */
byte __thiscall cAICharControl_Avoid(cAICharControl *self, cEntity *e, byte *found);
byte __thiscall cAICharControl_FollowPath(cAICharControl *self, Nto1Node *target, float near, float far, float speed);
byte __thiscall cAICharControl_Follow(cAICharControl *self, Nto1Node *target, float near, float far, float speed);
/* the move (AICC_, the arguments by mode, floats as double); 0 if it was refused */
byte cAICharControl_SetMove(cAICharControl *self, int mode, ...);
void __thiscall cAICharControl_Away(cAICharControl *self, Nto1Node *target, byte flag);
/* the result of FindRoute */
byte __thiscall cAICharControl_Route(cAICharControl *self, const vec3_t *p, float dist, float speed, byte a, byte b);
/* the goal 10000 ahead along dir */
void __thiscall cAICharControl_SetDir(cAICharControl *self, const vec3_t *dir);
void __thiscall cAICharControl_Nop(cAICharControl *self);
byte __thiscall cAICharControl_GoTo(cAICharControl *self, const vec3_t *p, float dist, float speed);
byte __thiscall cAICharControl_GoToDirect(cAICharControl *self, const vec3_t *p, float dist, float speed);
void __thiscall cAICharControl_SetSpeed(cAICharControl *self, float speed);
void __thiscall cAICharControl_SetTurnSpeed(cAICharControl *self, float speed);
void __thiscall cAICharControl_Stop2(cAICharControl *self);
/* the control stopped, dest and goal -FLT_MAX */
void __thiscall cAICharControl_Reset(cAICharControl *self);
byte __thiscall cAICharControl_IsBlocked(cAICharControl *self);
byte __thiscall cAICharControl_GetE4(cAICharControl *self);
void __thiscall cAICharControl_SetHold(cAICharControl *self, byte on);

/* construction, the frame and the slots of the first part of the file (rva 0x06cba0..0x072340) */
cAICharControl *__thiscall cAICharControl_ctor(void *mem, cEntity *ent, cAIChar *ai, int *arg);
void __fastcall cAICharControl_dtor(cAICharControl *self);
cAICharControl *__thiscall cAICharControl_Delete(cAICharControl *self, byte flags);
/* the segment list (0x20 byte nodes) emptied, and with its free nodes deleted */
void __fastcall DListSeg_Clear(cNWSegList *list);
void __fastcall DListSeg_Free(cNWSegList *list);
/* the components equal (cdecl, al) */
byte Vec3_Equal(const vec3_t *a, const vec3_t *b);
byte __thiscall cAICharControl_CheckTarget(cAICharControl *self);
byte __thiscall cAICharControl_CheckPathEnd(cAICharControl *self);
byte __thiscall cAICharControl_UpdatePath(cAICharControl *self);
byte __thiscall cAICharControl_UpdateRoutePath(cAICharControl *self);
byte __thiscall cAICharControl_UpdateFollowB(cAICharControl *self);
/* the native of the sphere test of CharsNear and FindAvoid (cdecl) */
void AICC_NearChar(cAbstractFile *params);
byte __thiscall cAICharControl_CharsNear(cAICharControl *self);
void __thiscall cAICharControl_AwayCheck(cAICharControl *self);
void __thiscall cAICharControl_DirCheck(cAICharControl *self);
byte __thiscall cAICharControl_CanMove(cAICharControl *self);
byte __thiscall cAICharControl_Update(cAICharControl *self, float dt);
void __thiscall cAICharControl_TurnFollow(cAICharControl *self);
byte __thiscall cAICharControl_CheckRoute(cAICharControl *self);
void __thiscall cAICharControl_DirTurn(cAICharControl *self);
byte __thiscall cAICharControl_CheckGoTo(cAICharControl *self);
byte __thiscall cAICharControl_CheckFollow(cAICharControl *self);
void __thiscall cAICharControl_UpdateAvoid(cAICharControl *self);
void __thiscall cAICharControl_Strafe2(cAICharControl *self);
void __thiscall cAICharControl_StrafeWalk(cAICharControl *self);
byte __thiscall cAICharControl_FindAvoid(cAICharControl *self);
void __thiscall cAICharControl_Strafe(cAICharControl *self);
void __thiscall cAICharControl_Walk2(cAICharControl *self);
void __thiscall cAICharControl_AwayWalk(cAICharControl *self);
void __thiscall cAICharControl_WalkFwd(cAICharControl *self);
byte __thiscall cAICharControl_CheckWait(cAICharControl *self);
void __thiscall cAICharControl_Orbit(cAICharControl *self);
byte __thiscall cAICharControl_StartOrbit(cAICharControl *self, float dist);
void __thiscall cAICharControl_Draw(cAICharControl *self);	/* rva 0x070330 */
byte __thiscall cAICharControl_DirectPath(cAICharControl *self, const vec3_t *p);
void __thiscall cAICharControl_Stop(cAICharControl *self);
byte __thiscall cAICharControl_Vf89(cAICharControl *self);
void __thiscall cAICharControl_RestartSlots(cAICharControl *self);
int __thiscall cAICharControl_GetWalkDir(cAICharControl *self);
void *__thiscall cAICharControl_GetAvoidEnt(cAICharControl *self);
void __thiscall cAICharControl_NextGoal(cAICharControl *self);
void __thiscall cAICharControl_NextGoalPath(cAICharControl *self);
void __thiscall cAICharControl_GetGoal(cAICharControl *self, vec3_t *out);
void __thiscall cAICharControl_GetDest(cAICharControl *self, vec3_t *out);
byte __thiscall cAICharControl_GoalOnLine(cAICharControl *self, const vec3_t *a, const vec3_t *b);
void __thiscall cAICharControl_GetAvoidB(cAICharControl *self, vec3_t *out);
void __thiscall cAICharControl_GetAvoidA(cAICharControl *self, vec3_t *out);
int __thiscall cAICharControl_Busy(cAICharControl *self);
int __thiscall cAICharControl_GetOrbitSide(cAICharControl *self);
vec3_t *__thiscall cAICharControl_SideDir(cAICharControl *self, vec3_t *out, int side);
byte __thiscall cAICharControl_GoalAtStart(cAICharControl *self);
byte __thiscall cAICharControl_AtPathEnd(cAICharControl *self);
byte __thiscall cAICharControl_IsDirect(cAICharControl *self);
byte __thiscall cAICharControl_UpdateFollowPath(cAICharControl *self, float dt);
byte __thiscall cAICharControl_UpdateFollowMode(cAICharControl *self, float dt);
byte __thiscall cAICharControl_UpdateAway(cAICharControl *self, float dt);
byte __thiscall cAICharControl_UpdateRoute(cAICharControl *self, float dt);
byte __thiscall cAICharControl_UpdateDir(cAICharControl *self, float dt);
byte __thiscall cAICharControl_UpdateGoTo(cAICharControl *self, float dt);
byte __thiscall cAICharControl_UpdateGoToDirect(cAICharControl *self, float dt);
byte __thiscall cAICharControl_Wait(cAICharControl *self, float dt);
/* the control of the AI of a character (0x19c of cAIChar) */
cAICharControl *__thiscall cAIChar_GetControl(cAIChar *ai);

#define CAICHARCONTROL_VFTABLE GAME_VFTABLE(cAICharControl_vftable, 0x3a572c)

/* the engine variables of the character AI */
#define g_DepthPathRouteInDoorAI GAME_GLOBAL_IN(int, GVar_DepthPathRouteInDoorAI, 16, 0x444300)	/* "DepthPathRouteInDoorAI" */
#define g_DistShirkCharAI GAME_GLOBAL_IN(float, GVar_DistShirkCharAI, 16, 0x4443d0)		/* "DistShirkCharAI" */
#define g_MarginCollCharAI GAME_GLOBAL_IN(float, GVar_MarginCollCharAI, 16, 0x444508)		/* "MarginCollCharAI" */
#define g_ShowShirkAI GAME_GLOBAL_IN(int, GVar_ShowShirkAI, 16, 0x4443e8)			/* "ShowShirkAI" */
#define g_ShowPathAI GAME_GLOBAL_IN(int, GVar_ShowPathAI, 16, 0x444744)			/* "ShowPathAI" */
#define g_FactorDistTestColCharAI GAME_GLOBAL_IN(float, GVar_FactorDistTestColCharAI, 16, 0x444478)	/* "FactorDistTestColCharAI" */
#define g_FactorDistTestColBackCharAI GAME_GLOBAL_IN(float, GVar_FactorDistTestColBackCharAI, 16, 0x4444f0)	/* "FactorDistTestColBackCharAI" */
#define g_MarginDetectCollCharAI GAME_GLOBAL_IN(float, GVar_MarginDetectCollCharAI, 16, 0x444550)	/* "MarginDetectCollCharAI" */
#define g_OffsetAngOrientCharAI GAME_GLOBAL_IN(float, GVar_OffsetAngOrientCharAI, 16, 0x444520)	/* "OffsetAngOrientCharAI" */
#define g_AngAttackBackCharAI GAME_GLOBAL_IN(float, GVar_AngAttackBackCharAI, 16, 0x444288)	/* "AngAttackBackCharAI" */
#define g_MinAngleSearchPointCharAI GAME_GLOBAL_IN(float, GVar_MinAngleSearchPointCharAI, 16, 0x444418)	/* "MinAngleSearchPointCharAI" */
#define g_DistNextPosGoTargetCharAI GAME_GLOBAL_IN(float, GVar_DistNextPosGoTargetCharAI, 16, 0x4444d8)	/* "DistNextPosGoTargetCharAI" */
#define g_NumPointsSearchSegmentCharAI GAME_GLOBAL_IN(int, GVar_NumPointsSearchSegmentCharAI, 16, 0x444448)	/* "NumPointsSearchSegmentCharAI" */
#define g_TimeEventCollCharAI GAME_GLOBAL_IN(float, GVar_TimeEventCollCharAI, 16, 0x444400)	/* "TimeEventCollCharAI" */
#define g_TimeStopCollCharAI GAME_GLOBAL_IN(float, GVar_TimeStopCollCharAI, 16, 0x444568)		/* "TimeStopCollCharAI" */
#define g_TimeTestCollCharAI GAME_GLOBAL_IN(float, GVar_TimeTestCollCharAI, 16, 0x4444a8)		/* "TimeTestCollCharAI" */
#define g_TimeBetweenEventCollCharAI GAME_GLOBAL_IN(float, GVar_TimeBetweenEventCollCharAI, 16, 0x444580)	/* "TimeBetweenEventCollCharAI" */
#define g_TimeStrafeLostObjCharAI GAME_GLOBAL_IN(float, GVar_TimeStrafeLostObjCharAI, 16, 0x4445b0)	/* "TimeStrafeLostObjCharAI" */
#define g_ShowCharAI GAME_GLOBAL_IN(cString, GVar_ShowCharAI, 16, 0x444490)		/* "ShowCharAI": the name of the character shown */
#define g_NameDebugShirkAI GAME_GLOBAL_IN(char *, GVar_NameDebugShirkAI, 16, 0x444538)		/* "NameDebugShirkAI" (a cString) */

/* the A* search on CNodesWorld2D (AI/AStar2D.cpp, not in C): the path from pos to dest into path; noPath and
 * reach tell the AI why there is none, res the kind of the path */
byte __thiscall CAStar2D_FindPath(void *astar, cNWVecList *path, const vec3_t *dest, const vec3_t *pos, float r,
	float dist, float h1, float h2, int soft, byte *noPath, byte *reach, int *res);
/* the points of visibility the body sees from pos (CNodesWorld2D_GetPointsVis) */
void __thiscall CAStar2D_GetPointsVis(void *astar, const vec3_t *pos, const vec3_t **list, int max, int *count,
	float r, float h1, float h2);

/* ---- AI/AICharControl2D.cpp (src/game/AI/ICharControl2D.c) ---- */
cAICharControl *__thiscall cAICharControl2D_ctor(void *mem, cEntity *ent, cAIChar *ai, int *arg);
void __fastcall cAICharControl2D_dtor(cAICharControl2D *self);
cAICharControl2D *__thiscall cAICharControl2D_Delete(cAICharControl2D *self, byte flags);
void __thiscall cAICharControl2D_Draw(cAICharControl2D *self);
/* toE and toGoal the ways on the floor to hitEnt and to the goal (back: from the goal); 1 below 90 degrees */
byte __thiscall cAICharControl2D_Facing(cAICharControl2D *self, cEntity *hitEnt, vec3_t *toE, vec3_t *toGoal);
byte __thiscall cAICharControl2D_TestLine(cAICharControl2D *self, const vec3_t *a, const vec3_t *b);
/* the way of twice the radius to the side not free */
byte __thiscall cAICharControl2D_SideBlocked(cAICharControl2D *self, int side);
/* the follow of the target: straight while the way is free, else along the path; 0 when no path is found */
byte __thiscall cAICharControl2D_UpdateFollow(cAICharControl2D *self);
/* at the end of the path: the event 2 (arrived), 4 (near), else 5 */
void __thiscall cAICharControl2D_CheckArrived(cAICharControl2D *self);
/* the turn of the control to the goal (or along avoidDir) */
void __thiscall cAICharControl2D_Turn(cAICharControl2D *self);
void __thiscall cAICharControl2D_Walk(cAICharControl2D *self);
byte __thiscall cAICharControl2D_FindPath(cAICharControl2D *self, const vec3_t *p);
byte __thiscall cAICharControl2D_FindRoute(cAICharControl2D *self);
float __thiscall cAICharControl2D_Dist(cAICharControl2D *self, const vec3_t *a, const vec3_t *b);
float __thiscall cAICharControl2D_DistSq(cAICharControl2D *self, const vec3_t *a, const vec3_t *b);
/* the distance to keep from hitEnt (the radii and DistShirkCharAI) */
float __thiscall cAICharControl2D_KeepDist(cAICharControl2D *self, cEntity *hitEnt);
/* the distance on the floor to hitEnt plus MarginCollCharAI */
float __thiscall cAICharControl2D_StrafeDist(cAICharControl2D *self, cEntity *hitEnt);
byte __thiscall cAICharControl2D_InFront(cAICharControl2D *self, cEntity *e, float ang);
byte __thiscall cAICharControl2D_Behind(cAICharControl2D *self, cEntity *e, float ang);
/* the angle at dest between the character and the point of the path below pi / 8 */
byte __thiscall cAICharControl2D_GoalAhead(cAICharControl2D *self);
/* a point of visibility of the graph seen from p */
byte __thiscall cAICharControl2D_OnFloor(cAICharControl2D *self, const vec3_t *p);
/* e further than the distance to keep plus 1 */
byte __thiscall cAICharControl2D_Near(cAICharControl2D *self, cEntity *e);
/* dest within the distance to keep from hitEnt */
byte __thiscall cAICharControl2D_InWay(cAICharControl2D *self, vec3_t dest, cEntity *hitEnt);
/* the corridor of the body from the floor below a to the floor below b free: the line between them raised, and 7
 * lines of the length FactorDistTestColCharAI * radius around it (to the sides, up, the diagonals), then
 * FloorAhead(b); the corridor into debugSegs. To dest the test of TestPathEnd */
byte __thiscall cAICharControl2D_TestPath(cAICharControl2D *self, const vec3_t *a, const vec3_t *b, float r);
/* TestPath with the 7 lines aimed at b, the step of a forward walk (TestStep with r), then FloorAhead(b) */
byte __thiscall cAICharControl2D_TestPathEnd(cAICharControl2D *self, const vec3_t *a, const vec3_t *b, float r);
/* the 7 lines around a along the way from b to c (at most the radius plus 5 long); r not used */
byte __thiscall cAICharControl2D_TestStep(cAICharControl2D *self, const vec3_t *a, const vec3_t *b, const vec3_t *c,
	float r);
/* the corridor of the walk back from a to b: 5 lines of the length FactorDistTestColBackCharAI * radius around
 * the line between the floors, back along it (not down); r not used */
byte __thiscall cAICharControl2D_TestSides(cAICharControl2D *self, const vec3_t *a, const vec3_t *b, float r);
byte __thiscall cAICharControl2D_FloorAhead(cAICharControl2D *self, const vec3_t *p);
/* the floor below p raised by the floor distance of the character into out; 0 (out = p) without one */
byte __thiscall cAICharControl2D_FloorAt(cAICharControl2D *self, const vec3_t *p, vec3_t *out);
void __thiscall cAICharControl2D_StepGoal(cAICharControl2D *self, vec3_t *p);
byte __thiscall cAICharControl2D_TryStrafe(cAICharControl2D *self, int side, cEntity *hitEnt, byte back, float keep,
	byte fwd, float sd, byte e2);
byte __thiscall cAICharControl2D_StrafeBack(cAICharControl2D *self, const vec3_t *pos, const vec3_t *away,
	const vec3_t *dir, float dist, byte left, byte flag);
byte __thiscall cAICharControl2D_StrafeFwd(cAICharControl2D *self, const vec3_t *pos, const vec3_t *away,
	const vec3_t *dir, float dist, byte left, byte flag);

/* CAStar_FindPath and CAStar_GetPointsVis (the same arguments on CNodesWorld): nodesworld.h */

/* ---- AI/AICharControl3D.cpp (src/game/AI/ICharControl3D.c): the slots as in 2D, in all three axes ---- */
cAICharControl *__thiscall cAICharControl3D_ctor(void *mem, cEntity *ent, cAIChar *ai, int *arg);
void __fastcall cAICharControl3D_dtor(cAICharControl3D *self);
cAICharControl3D *__thiscall cAICharControl3D_Delete(cAICharControl3D *self, byte flags);
void __thiscall cAICharControl3D_Draw(cAICharControl3D *self);
byte __thiscall cAICharControl3D_Facing(cAICharControl3D *self, cEntity *hitEnt, vec3_t *toE, vec3_t *toGoal);
/* the segment free of the level */
byte __thiscall cAICharControl3D_TestLine(cAICharControl3D *self, const vec3_t *a, const vec3_t *b);
/* the segment a b free and 8 lines around it from a: along it (at most FactorDistTestColCharAI * radius long),
 * aimed at b (TestPathEnd); the lines back from a (at most the radius plus 5) for TestSides. The corridor into
 * debugSegs. TestPath to dest is TestPathEnd */
byte __thiscall cAICharControl3D_TestPath(cAICharControl3D *self, const vec3_t *a, const vec3_t *b, float r);
byte __thiscall cAICharControl3D_TestPathEnd(cAICharControl3D *self, const vec3_t *a, const vec3_t *b, float r);
byte __thiscall cAICharControl3D_TestSides(cAICharControl3D *self, const vec3_t *a, const vec3_t *b, float r);
/* slot 100 of the 3D control: the 8 lines around a along the way to b (at most the radius plus 5); r not used */
byte __thiscall cAICharControl3D_TestStep(cAICharControl3D *self, const vec3_t *a, const vec3_t *b, float r);
/* the way of 1.5 radius to the side (3 up, 4 down) not free */
byte __thiscall cAICharControl3D_SideBlocked(cAICharControl3D *self, int side);
byte __thiscall cAICharControl3D_UpdateFollow(cAICharControl3D *self);
void __thiscall cAICharControl3D_CheckArrived(cAICharControl3D *self);
void __thiscall cAICharControl3D_Turn(cAICharControl3D *self);
/* walkDir from the distance to the target alone */
void __thiscall cAICharControl3D_Walk(cAICharControl3D *self);
byte __thiscall cAICharControl3D_FindPath(cAICharControl3D *self, const vec3_t *p);
byte __thiscall cAICharControl3D_FindRoute(cAICharControl3D *self);
float __thiscall cAICharControl3D_Dist(cAICharControl3D *self, const vec3_t *a, const vec3_t *b);
float __thiscall cAICharControl3D_DistSq(cAICharControl3D *self, const vec3_t *a, const vec3_t *b);
/* the 2D distance to keep, at most the distance to hitEnt plus 10 */
float __thiscall cAICharControl3D_KeepDist(cAICharControl3D *self, cEntity *hitEnt);
/* the two radii plus MarginCollCharAI */
float __thiscall cAICharControl3D_StrafeDist(cAICharControl3D *self, cEntity *hitEnt);
byte __thiscall cAICharControl3D_InFront(cAICharControl3D *self, cEntity *e, float ang);
byte __thiscall cAICharControl3D_Behind(cAICharControl3D *self, cEntity *e, float ang);
byte __thiscall cAICharControl3D_GoalAhead(cAICharControl3D *self);
byte __thiscall cAICharControl3D_OnFloor(cAICharControl3D *self, const vec3_t *p);
byte __thiscall cAICharControl3D_Near(cAICharControl3D *self, cEntity *e);
byte __thiscall cAICharControl3D_InWay(cAICharControl3D *self, vec3_t dest, cEntity *hitEnt);
void __thiscall cAICharControl3D_StepGoal(cAICharControl3D *self, vec3_t *p);
byte __thiscall cAICharControl3D_TryStrafe(cAICharControl3D *self, int side, cEntity *hitEnt, byte back, float keep,
	byte fwd, float sd, byte e2);

/* ---- the move controls of the character classes (AI/AIControl<Class>.cpp, src/game/AI/AIControls.c): the 2D
 * control for the bank director, the bank master, the bishop and the gear, the 3D one for the messenger, the nurse and
 * the sentinel, each with some slots of its own ---- */
#define CAICONTROLBANKDIRECTOR_VFTABLE GAME_VFTABLE(cAIControlBankDirector_vftable, 0x3a6000)
#define CAICONTROLBANKMASTER_VFTABLE GAME_VFTABLE(cAIControlBankMaster_vftable, 0x3a61a0)
#define CAICONTROLBISHOP_VFTABLE GAME_VFTABLE(cAIControlBishop_vftable, 0x3a63b0)
#define CAICONTROLGEAR_VFTABLE GAME_VFTABLE(cAIControlGear_vftable, 0x3a65a8)
#define CAICONTROLMESSENGER_VFTABLE GAME_VFTABLE(cAIControlMessenger_vftable, 0x3a67d0)
#define CAICONTROLNURSE_VFTABLE GAME_VFTABLE(cAIControlNurse_vftable, 0x3a6968)
#define CAICONTROLSENTINEL_VFTABLE GAME_VFTABLE(cAIControlSentinel_vftable, 0x3a6b7c)
#define CAICONTROLSHOOT_SIZE 0x384		/* the bishop and the gear */
#define CAICONTROLSENTINEL_SIZE 0x37c		/* without strafeTime and strafeAng */

/* the bishop, the gear and the sentinel strafe while a Bishop, a Gear (or a Dtritus) aims at them, or (the sentinel)
 * when they were hit */
typedef struct cAIControlShoot {
	cAICharControl base;		/* 0x000 */
	cAICCSlot slotSide;		/* 0x360 the side of the strafe blocked (0.2 s) */
	int side;			/* 0x370 the strafe: 0 none, 1, 2 */
	float time;			/* 0x374 since the last strafe started */
	byte other;			/* 0x378 the side was blocked: the other side next */
	byte pad_379[3];
	float strafeTime;		/* 0x37c TimeStrafeUnderAttack<Class>AI (the sentinel: the gvar itself) */
	float strafeAng;		/* 0x380 AngleStrafeUnderAttack<Class>AI */
} cAIControlShoot;
SD_OFFSET(cAIControlShoot, side, 0x370);
SD_OFFSET(cAIControlShoot, strafeTime, 0x37c);
SD_SIZE(cAIControlShoot, CAICONTROLSHOOT_SIZE);
typedef cAIControlShoot cAIControlBishop;
typedef cAIControlShoot cAIControlGear;

/* the classes that add no members to the 2D or the 3D control (0x360 bytes) */
typedef cAICharControl2D cAIControlBankDirector;
typedef cAICharControl2D cAIControlBankMaster;
typedef cAICharControl3D cAIControlMessenger;
typedef cAICharControl3D cAIControlNurse;

/* the sentinel: the 3D control with the strafe of cAIControlShoot, but the parameters are the gvars (0x37c bytes);
 * its functions work on the cAIControlShoot they share with the bishop and the gear */
typedef struct cAIControlSentinel {
	cAICharControl3D base;		/* 0x000 */
	cAICCSlot slotSide;		/* 0x360 the side of the strafe blocked (0.2 s) */
	int side;			/* 0x370 the strafe: 0 none, 1, 2 */
	float time;			/* 0x374 since the last strafe started */
	byte other;			/* 0x378 the side was blocked: the other side next */
	byte pad_379[3];
} cAIControlSentinel;
SD_OFFSET(cAIControlSentinel, side, 0x370);
SD_SIZE(cAIControlSentinel, CAICONTROLSENTINEL_SIZE);

/* the strafe parameters by the target (with an AI or not) */
#define g_TimeStrafeUnderAttackBishopAI GAME_GLOBAL_IN(float, GVar_TimeStrafeUnderAttackBishopAI, 16, 0x4445d8)
#define g_TimeStrafeUnderAttackBishopAIDef GAME_GLOBAL_IN(float, GVar_TimeStrafeUnderAttackBishopAI, 20, 0x4445dc)
#define g_AngleStrafeUnderAttackBishopAI GAME_GLOBAL_IN(float, GVar_AngleStrafeUnderAttackBishopAI, 16, 0x4445f0)
#define g_AngleStrafeUnderAttackBishopAIDef GAME_GLOBAL_IN(float, GVar_AngleStrafeUnderAttackBishopAI, 20, 0x4445f4)
#define g_TimeStrafeUnderAttackGearAI GAME_GLOBAL_IN(float, GVar_TimeStrafeUnderAttackGearAI, 16, 0x444608)
#define g_TimeStrafeUnderAttackGearAIDef GAME_GLOBAL_IN(float, GVar_TimeStrafeUnderAttackGearAI, 20, 0x44460c)
#define g_AngleStrafeUnderAttackGearAI GAME_GLOBAL_IN(float, GVar_AngleStrafeUnderAttackGearAI, 16, 0x444620)
#define g_AngleStrafeUnderAttackGearAIDef GAME_GLOBAL_IN(float, GVar_AngleStrafeUnderAttackGearAI, 20, 0x444624)
#define g_TimeStrafeUnderAttackSentinelAI GAME_GLOBAL_IN(float, GVar_TimeStrafeUnderAttackSentinelAI, 16, 0x444668)
#define g_AngleStrafeUnderAttackSentinelAI GAME_GLOBAL_IN(float, GVar_AngleStrafeUnderAttackSentinelAI, 16, 0x444680)
#define g_VelRotDynamicMessengerAI GAME_GLOBAL_IN(float, GVar_VelRotDynamicMessengerAI, 16, 0x444650)
#define g_LimVelRotDynamicMessengerAI GAME_GLOBAL_IN(float, GVar_LimVelRotDynamicMessengerAI, 16, 0x444638)

cAICharControl *__thiscall cAIControlBankDirector_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg);
void __fastcall cAIControlBankDirector_dtor(cAIControlBankDirector *self);
cAIControlBankDirector *__thiscall cAIControlBankDirector_Delete(cAIControlBankDirector *self, byte flags);
/* the Walk of the bank director and the master: forward beyond farDist, back within nearDist when the way back is
 * free */
void __thiscall cAIControlBank_Walk(cAICharControl *self);
cAICharControl *__thiscall cAIControlBankMaster_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg);
void __fastcall cAIControlBankMaster_dtor(cAIControlBankMaster *self);
cAIControlBankMaster *__thiscall cAIControlBankMaster_Delete(cAIControlBankMaster *self, byte flags);
/* the Orbit of the control, but not while the action button is down or its ActionEnd plays */
void __thiscall cAIControlBankMaster_Orbit(cAIControlBankMaster *self);
cAICharControl *__thiscall cAIControlBishop_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg);
void __fastcall cAIControlBishop_dtor(cAICharControl *self);
cAICharControl *__thiscall cAIControlBishop_Delete(cAICharControl *self, byte flags);
/* CheckWait, but not while the target plays its ActionEnd */
byte __thiscall cAIControlBishop_CheckWait(cAICharControl *self);
void __thiscall cAIControlBishop_Strafe2(cAIControlBishop *self);
void __thiscall cAIControlBishop_Strafe(cAIControlBishop *self);
/* the 2D Walk without a strafe of its own (the gear too) */
void __thiscall cAIControlShoot_Walk(cAIControlShoot *self);
/* Nop of the control: the strafe parameters by the target */
void __thiscall cAIControlBishop_SetParams(cAIControlBishop *self);
cAICharControl *__thiscall cAIControlGear_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg);
void __fastcall cAIControlGear_dtor(cAICharControl *self);
cAICharControl *__thiscall cAIControlGear_Delete(cAICharControl *self, byte flags);
/* CheckWait, but not while the target plays its ActionInit or ActionEnd */
byte __thiscall cAIControlGear_CheckWait(cAICharControl *self);
/* the turns stop during the PostFusion action of the gear */
void __thiscall cAIControlGear_TurnFollow(cAICharControl *self);
void __thiscall cAIControlGear_Turn(cAICharControl *self);
void __thiscall cAIControlGear_DirTurn(cAICharControl *self);
void __thiscall cAIControlGear_Strafe2(cAIControlGear *self);
void __thiscall cAIControlGear_Strafe(cAIControlGear *self);
/* vf89: the AI of the gear saw the way to the target blocked */
byte __thiscall cAIControlGear_Vf89(cAICharControl *self);
void __thiscall cAIControlGear_SetParams(cAIControlGear *self);
void __thiscall cAIControlGear_SetTurnSpeed(cAICharControl *self, float speed);
cAICharControl *__thiscall cAIControlMessenger_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg);
void __fastcall cAIControlMessenger_dtor(cAIControlMessenger *self);
cAIControlMessenger *__thiscall cAIControlMessenger_Delete(cAIControlMessenger *self, byte flags);
byte __thiscall cAIControlMessenger_UpdateFollowMode(cAIControlMessenger *self, float dt);
void __thiscall cAIControlMessenger_SetTurnSpeed(cAIControlMessenger *self, float speed);
cAICharControl *__thiscall cAIControlNurse_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg);
void __fastcall cAIControlNurse_dtor(cAIControlNurse *self);
cAIControlNurse *__thiscall cAIControlNurse_Delete(cAIControlNurse *self, byte flags);
/* forward beyond farDist only (3D) */
void __thiscall cAIControlNurse_Walk(cAIControlNurse *self);
cAICharControl *__thiscall cAIControlSentinel_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg);
void __fastcall cAIControlSentinel_dtor(cAICharControl *self);
cAICharControl *__thiscall cAIControlSentinel_Delete(cAICharControl *self, byte flags);
void __thiscall cAIControlSentinel_Strafe2(cAIControlShoot *self);
void __thiscall cAIControlSentinel_Strafe(cAIControlShoot *self);
/* the 3D Walk without a strafe of its own */
void __thiscall cAIControlSentinel_Walk(cAIControlShoot *self);

typedef cAICharControl *(__thiscall *cAICharControlCtor_t)(void *mem, cEntity *e, cAIChar *ai, int *arg);

/* the constructors of the cAIChar subclasses: the move control of cAIChar deleted, one of the class in its place */
static inline void cAIChar_NewControl(cAIChar *self, uint32_t size, cAICharControlCtor_t ctor, cEntity *e, int *arg) {
	if (self->control) {
		AICC_Vtbl(self->control)->Delete(self->control, 1);
	}
	void *mem = operator_new(size);
	cAICharControl *ControlMove = mem ? ctor(mem, e, self, arg) : NULL;
	self->control = ControlMove;
	CHECK(ControlMove);
}

#endif
