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
#include "scrap/nodesworld.h"
#include "scrap/objentity.h"
#include "scrap/slotcontroller.h"
#include "scrap/walkcharentity.h"

#define CAICHARCONTROL_SIZE 0x360
#define CAICHARCONTROL2D_VFTABLE 0x3a5acc
#define CAICHARCONTROL3D_VFTABLE 0x3a5d18

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

/* the slots of cAICharControl::vftable (100); the 2D control adds three, the 3D control one */
typedef struct cAICharControlVtbl {
	cAICharControl *(__thiscall *Delete)(cAICharControl *self, byte flags);			/* 0x00 */
	void(__thiscall *Draw)(cAICharControl *self);						/* 0x04 */
	void *vf02;
	int(__thiscall *Busy)(cAICharControl *self);						/* 0x0c */
	void *vf04_05[2];
	byte (*SetMove)(cAICharControl *self, int mode, ...);					/* 0x18 */
	void(__thiscall *Reset)(cAICharControl *self);						/* 0x1c */
	void(__thiscall *SetHold)(cAICharControl *self, byte on);				/* 0x20 */
	void(__thiscall *Stop)(cAICharControl *self);						/* 0x24 */
	void *vf0a_0d[4];
	/* the corridor of the body from a to b free (the radius r) */
	byte(__thiscall *TestPath)(cAICharControl *self, const vec3_t *a, const vec3_t *b, float r);	/* 0x38 */
	byte(__thiscall *TestLine)(cAICharControl *self, const vec3_t *a, const vec3_t *b);		/* 0x3c */
	byte(__thiscall *TestSides)(cAICharControl *self, const vec3_t *a, const vec3_t *b, float r);	/* 0x40 */
	void *vf11_12[2];
	void(__thiscall *ClearAvoid)(cAICharControl *self);					/* 0x4c */
	void(__thiscall *SetSpeed)(cAICharControl *self, float speed);				/* 0x50 */
	void *vf15;
	byte(__thiscall *IsBlocked)(cAICharControl *self);					/* 0x58 */
	byte(__thiscall *GetE4)(cAICharControl *self);						/* 0x5c */
	void *vf18;
	void(__thiscall *Nop)(cAICharControl *self);						/* 0x64 */
	void *vf1a_1c[3];
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
	void *vf2c_2e[3];
	/* e within ang of the back (the front) of the view, on the floor */
	byte(__thiscall *Behind)(cAICharControl *self, cEntity *e, float ang);			/* 0xbc */
	byte(__thiscall *InFront)(cAICharControl *self, cEntity *e, float ang);			/* 0xc0 */
	byte(__thiscall *Route)(cAICharControl *self, const vec3_t *p, float dist, float speed, byte a, byte b);	/* 0xc4 */
	void(__thiscall *SetDir)(cAICharControl *self, const vec3_t *dir);			/* 0xc8 */
	void *vf33_35[3];
	void(__thiscall *CheckArrived)(cAICharControl *self);					/* 0xd8 */
	void *vf37;
	byte(__thiscall *GoTo)(cAICharControl *self, const vec3_t *p, float dist, float speed);	/* 0xe0 */
	byte(__thiscall *GoToDirect)(cAICharControl *self, const vec3_t *p, float dist, float speed);	/* 0xe4 */
	byte(__thiscall *FindPath)(cAICharControl *self, const vec3_t *p);			/* 0xe8 */
	/* the path is the line to p */
	byte(__thiscall *DirectPath)(cAICharControl *self, const vec3_t *p);			/* 0xec */
	/* p moved a random step while the way stays free */
	void(__thiscall *StepGoal)(cAICharControl *self, vec3_t *p);				/* 0xf0 */
	void *vf3d_40[4];
	void(__thiscall *Vf41)(cAICharControl *self);						/* 0x104 */
	void *vf42_45[4];
	void(__thiscall *Turn)(cAICharControl *self);						/* 0x118 */
	void *vf47_48[2];
	byte(__thiscall *Avoid)(cAICharControl *self, cEntity *e, byte *found);			/* 0x124 */
	void *vf4a_4b[2];
	byte(__thiscall *SideBlocked)(cAICharControl *self, int side);				/* 0x130 */
	/* the unit vector to the side (out returned) */
	vec3_t *(__thiscall *SideDir)(cAICharControl *self, vec3_t *out, int side);		/* 0x134 */
	void *vf4e_51[4];
	byte(__thiscall *FindRoute)(cAICharControl *self);					/* 0x148 */
	void *vf53;
	byte(__thiscall *Follow)(cAICharControl *self, Nto1Node *target, float near, float far, float speed);	/* 0x150 */
	void *vf55_58[4];
	byte(__thiscall *Vf89)(cAICharControl *self);						/* 0x164 */
	void *vf5a;
	void(__thiscall *Walk)(cAICharControl *self);						/* 0x16c */
	void *vf5c;
	byte(__thiscall *UpdateFollow)(cAICharControl *self);					/* 0x174 */
	void *vf5e;
	byte(__thiscall *FollowPath)(cAICharControl *self, Nto1Node *target, float near, float far, float speed);	/* 0x17c */
	void *vf60_62[3];
	void(__thiscall *Stop2)(cAICharControl *self);						/* 0x18c */
} cAICharControlVtbl;
SD_OFFSET(cAICharControlVtbl, SetMove, 0x18);
SD_OFFSET(cAICharControlVtbl, TestPath, 0x38);
SD_OFFSET(cAICharControlVtbl, ClearAvoid, 0x4c);
SD_OFFSET(cAICharControlVtbl, Nop, 0x64);
SD_OFFSET(cAICharControlVtbl, SetTurnSpeed, 0x74);
SD_OFFSET(cAICharControlVtbl, TryStrafe, 0x80);
SD_OFFSET(cAICharControlVtbl, TestPathEnd, 0xa0);
SD_OFFSET(cAICharControlVtbl, Away, 0xac);
SD_OFFSET(cAICharControlVtbl, Behind, 0xbc);
SD_OFFSET(cAICharControlVtbl, StrafeFwd, 0x7c);
SD_OFFSET(cAICharControlVtbl, CheckArrived, 0xd8);
SD_OFFSET(cAICharControlVtbl, GoTo, 0xe0);
SD_OFFSET(cAICharControlVtbl, Vf41, 0x104);
SD_OFFSET(cAICharControlVtbl, Turn, 0x118);
SD_OFFSET(cAICharControlVtbl, Avoid, 0x124);
SD_OFFSET(cAICharControlVtbl, SideBlocked, 0x130);
SD_OFFSET(cAICharControlVtbl, FindRoute, 0x148);
SD_OFFSET(cAICharControlVtbl, Follow, 0x150);
SD_OFFSET(cAICharControlVtbl, Vf89, 0x164);
SD_OFFSET(cAICharControlVtbl, Walk, 0x16c);
SD_OFFSET(cAICharControlVtbl, UpdateFollow, 0x174);
SD_OFFSET(cAICharControlVtbl, FollowPath, 0x17c);
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

/* the slots of the AI of the character (cAIChar) the control calls */
typedef struct cAICharCtlOwnerVtbl {
	void *vf00_0d[14];
	Nto1Node *(__thiscall *GetTarget)(cAIChar *self);	/* 0x38 the link to the target entity */
	void *vf0f_15[7];
	byte(__thiscall *Event)(cAIChar *self, int ev);		/* 0x58 the move tells the AI (arrived, blocked, ..) */
} cAICharCtlOwnerVtbl;
SD_OFFSET(cAICharCtlOwnerVtbl, GetTarget, 0x38);
SD_OFFSET(cAICharCtlOwnerVtbl, Event, 0x58);

static inline const cAICharCtlOwnerVtbl *AICC_OwnerVtbl(cAIChar *ai) {
	return *(const cAICharCtlOwnerVtbl **)ai;
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
	cAICCSlot slotWalk;	/* 0x004 the distance checks of Walk (0.2 s) */
	cAICCSlot slot14;		/* 0x014 */
	cAICCSlot slot24;		/* 0x024 */
	cAICCSlot slot34;		/* 0x034 */
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
	byte b88;			/* 0x088 */
	byte pad_89[3];
	float turnGain;			/* 0x08c the turn for an angle of pi */
	float turnStep;			/* 0x090 the change of the turn in a frame at most */
	int walkDir;			/* 0x094 0 stop, 1 forward, 2 back */
	int side;			/* 0x098 the strafe: 0 none, 1, 2 */
	int i9c;			/* 0x09c */
	byte flying;			/* 0x0a0 the argument of FOLLOW */
	byte ba1;			/* 0x0a1 */
	byte pad_a2[2];
	vec3_t avoidA;			/* 0x0a4 -FLT_MAX after ClearAvoid */
	vec3_t avoidB;			/* 0x0b0 */
	byte hold;			/* 0x0bc */
	byte newPath;			/* 0x0bd */
	byte pad_be[2];
	int ic0;			/* 0x0c0 */
	Nto1Node link;			/* 0x0c4 data the AI */
	byte blocked;			/* 0x0d4 */
	byte bd5;			/* 0x0d5 */
	byte pad_d6[2];
	vec3_t avoidDir;		/* 0x0d8 0 for none */
	byte be4;			/* 0x0e4 */
	byte pad_e5[3];
	float fe8;			/* 0x0e8 engine variables at the start */
	float fec;			/* 0x0ec */
	float ff0;			/* 0x0f0 */
	float speed;			/* 0x0f4 the walk value */
	int lifeMark;			/* 0x0f8 the life of the character */
	cAICCSlot slotPath;	/* 0x0fc the path is searched again (0.133 s) */
	vec3_t goal;			/* 0x10c the next point of the path */
	cNWVecList path;		/* 0x118 */
	vec3_t *pathPoint;		/* 0x130 the point of the path walked to */
	int i134;			/* 0x134 */
	float turnXNew;			/* 0x138 */
	float turnYNew;			/* 0x13c */
	byte b140;			/* 0x140 */
	byte b141;			/* 0x141 */
	byte pad_142[2];
	cAICCSlot slotDirect;	/* 0x144 the direct follow checks the way (0.2 s) */
	cAICCSlot slotSee;	/* 0x154 the path follow looks for the direct way (0.2 s) */
	cAICCSlot slotRepath;	/* 0x164 the path to the target searched again (5 s) */
	cAICCSlot slotNear;	/* 0x174 the end of the path near: searched again (0.2 s) */
	cAICCSlot slot184;	/* 0x184 (0.2 s) */
	cAICCSlot slot194;	/* 0x194 (3 s) */
	cAICCSlot slot1a4;	/* 0x1a4 (0.2 s) */
	Nto1Node *target;		/* 0x1b4 the link to the entity to follow (objDin) */
	cEntity *targetEnt;		/* 0x1b8 its entity when the move started */
	float farDist;			/* 0x1bc */
	float nearDist;			/* 0x1c0 */
	vec3_t v1c4;			/* 0x1c4 */
	byte direct;			/* 0x1d0 the follow walks straight to the target */
	byte b1d1;			/* 0x1d1 */
	byte pad_1d2[2];
	int i1d4;			/* 0x1d4 */
	vec3_t v1d8;			/* 0x1d8 */
	vec3_t v1e4;			/* 0x1e4 */
	vec3_t v1f0;			/* 0x1f0 */
	int pathRes;			/* 0x1fc the result of the path search (2: the end is not reachable) */
	byte stuck;			/* 0x200 the follow sees no way */
	byte b201;			/* 0x201 */
	byte pad_202[2];
	float f204;			/* 0x204 */
	float f208;			/* 0x208 */
	int i20c;			/* 0x20c */
	byte b210;			/* 0x210 */
	byte pad_211[3];
	int i214;			/* 0x214 */
	int i218;			/* 0x218 */
	int i21c;			/* 0x21c */
	vec3_t dir;			/* 0x220 the direction of AICC_DIR */
	byte awayFlag;			/* 0x22c */
	byte b22d;			/* 0x22d */
	byte routeA;			/* 0x22e */
	byte routeB;			/* 0x22f */
	vec3_t routePoint;		/* 0x230 */
	cNWNode *routeNode;		/* 0x23c the node of the route BuildPath reached */
	cAICCSeg debugSegs[9];		/* 0x240 the corridor of the last path test */
	cNWVecList debug318;		/* 0x318 */
	cNWVecList debug330;		/* 0x330 */
	cNWSegList debug348;		/* 0x348 */
};
SD_OFFSET(cAICharControl, ai, 0x44);
SD_OFFSET(cAICharControl, dest, 0x68);
SD_OFFSET(cAICharControl, turnGain, 0x8c);
SD_OFFSET(cAICharControl, avoidA, 0xa4);
SD_OFFSET(cAICharControl, link, 0xc4);
SD_OFFSET(cAICharControl, avoidDir, 0xd8);
SD_OFFSET(cAICharControl, speed, 0xf4);
SD_OFFSET(cAICharControl, slotPath, 0xfc);
SD_OFFSET(cAICharControl, goal, 0x10c);
SD_OFFSET(cAICharControl, path, 0x118);
SD_OFFSET(cAICharControl, pathPoint, 0x130);
SD_OFFSET(cAICharControl, slotDirect, 0x144);
SD_OFFSET(cAICharControl, slot1a4, 0x1a4);
SD_OFFSET(cAICharControl, target, 0x1b4);
SD_OFFSET(cAICharControl, direct, 0x1d0);
SD_OFFSET(cAICharControl, pathRes, 0x1fc);
SD_OFFSET(cAICharControl, dir, 0x220);
SD_OFFSET(cAICharControl, routePoint, 0x230);
SD_OFFSET(cAICharControl, debugSegs, 0x240);
SD_OFFSET(cAICharControl, debug318, 0x318);
SD_OFFSET(cAICharControl, debug348, 0x348);
SD_SIZE(cAICharControl, CAICHARCONTROL_SIZE);

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

/* ---- the base part, AI/AICharControl.cpp (src/game/AI/ICharControl.c) ---- */
/* Vec3_NormalizeTo (rva 0x072340): fxmisc.h */
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

/* ---- the base part not in C (rva 0x06cba0..0x072340) ---- */
cAICharControl *__thiscall cAICharControl_ctor(void *mem, cEntity *ent, cAIChar *ai, int *arg);
void __fastcall cAICharControl_dtor(cAICharControl *self);
void __thiscall cAICharControl_Draw(cAICharControl *self);	/* rva 0x070330 */
/* the segment list (0x20 byte nodes) emptied (fastcall) */
void __fastcall DListSeg_Clear(cNWSegList *list);
/* the control of the AI of a character (0x19c of cAIChar) */
cAICharControl *__thiscall cAIChar_GetControl(cAIChar *ai);

/* the engine variables of the character AI */
#define g_DepthPathRouteInDoorAI GAME_DATA(int, 0x444300)	/* "DepthPathRouteInDoorAI" */
#define g_DistShirkCharAI GAME_DATA(float, 0x4443d0)		/* "DistShirkCharAI" */
#define g_MarginCollCharAI GAME_DATA(float, 0x444508)		/* "MarginCollCharAI" */
#define g_ShowShirkAI GAME_DATA(int, 0x4443e8)			/* "ShowShirkAI" */
#define g_ShowPathAI GAME_DATA(int, 0x444744)			/* "ShowPathAI" */
#define g_FactorDistTestColCharAI GAME_DATA(float, 0x444478)	/* "FactorDistTestColCharAI" */
#define g_FactorDistTestColBackCharAI GAME_DATA(float, 0x4444f0)	/* "FactorDistTestColBackCharAI" */

/* the A* search on CNodesWorld2D (AI/AStar2D.cpp, not in C): the path from pos to dest into path; noPath and
 * reach tell the AI why there is none, res the kind of the path */
byte __thiscall CAStar2D_FindPath(void *astar, cNWVecList *path, const vec3_t *dest, const vec3_t *pos, float r,
	float dist, float h1, float h2, int soft, byte *noPath, byte *reach, int *res);
/* the points of visibility the body sees from pos (CNodesWorld2D_GetPointsVis) */
void __thiscall CAStar2D_GetPointsVis(void *astar, const vec3_t *pos, const vec3_t **list, int max, int *count,
	float r, float h1, float h2);

/* ---- AI/AICharControl2D.cpp (src/game/AI/ICharControl2D.c) ---- */
cAICharControl *__thiscall cAICharControl2D_ctor(void *mem, cEntity *ent, cAIChar *ai, int *arg);
void __fastcall cAICharControl2D_dtor(cAICharControl *self);
cAICharControl *__thiscall cAICharControl2D_Delete(cAICharControl *self, byte flags);
void __thiscall cAICharControl2D_Draw(cAICharControl *self);
/* toE and toGoal the ways on the floor to hitEnt and to the goal (back: from the goal); 1 below 90 degrees */
byte __thiscall cAICharControl2D_Facing(cAICharControl *self, cEntity *hitEnt, vec3_t *toE, vec3_t *toGoal);
byte __thiscall cAICharControl2D_TestLine(cAICharControl *self, const vec3_t *a, const vec3_t *b);
/* the way of twice the radius to the side not free */
byte __thiscall cAICharControl2D_SideBlocked(cAICharControl *self, int side);
/* the follow of the target: straight while the way is free, else along the path; 0 when no path is found */
byte __thiscall cAICharControl2D_UpdateFollow(cAICharControl *self);
/* at the end of the path: the event 2 (arrived), 4 (near), else 5 */
void __thiscall cAICharControl2D_CheckArrived(cAICharControl *self);
/* the turn of the control to the goal (or along avoidDir) */
void __thiscall cAICharControl2D_Turn(cAICharControl *self);
void __thiscall cAICharControl2D_Walk(cAICharControl *self);
byte __thiscall cAICharControl2D_FindPath(cAICharControl *self, const vec3_t *p);
byte __thiscall cAICharControl2D_FindRoute(cAICharControl *self);
float __thiscall cAICharControl2D_Dist(cAICharControl *self, const vec3_t *a, const vec3_t *b);
float __thiscall cAICharControl2D_DistSq(cAICharControl *self, const vec3_t *a, const vec3_t *b);
/* the distance to keep from hitEnt (the radii and DistShirkCharAI) */
float __thiscall cAICharControl2D_KeepDist(cAICharControl *self, cEntity *hitEnt);
/* the distance on the floor to hitEnt plus MarginCollCharAI */
float __thiscall cAICharControl2D_StrafeDist(cAICharControl *self, cEntity *hitEnt);
byte __thiscall cAICharControl2D_InFront(cAICharControl *self, cEntity *e, float ang);
byte __thiscall cAICharControl2D_Behind(cAICharControl *self, cEntity *e, float ang);
/* the angle at dest between the character and the point of the path below pi / 8 */
byte __thiscall cAICharControl2D_GoalAhead(cAICharControl *self);
/* a point of visibility of the graph seen from p */
byte __thiscall cAICharControl2D_OnFloor(cAICharControl *self, const vec3_t *p);
/* e further than the distance to keep plus 1 */
byte __thiscall cAICharControl2D_Near(cAICharControl *self, cEntity *e);
/* dest within the distance to keep from hitEnt */
byte __thiscall cAICharControl2D_InWay(cAICharControl *self, vec3_t dest, cEntity *hitEnt);
/* the corridor of the body from the floor below a to the floor below b free: the line between them raised, and 7
 * lines of the length FactorDistTestColCharAI * radius around it (to the sides, up, the diagonals), then
 * FloorAhead(b); the corridor into debugSegs. To dest the test of TestPathEnd */
byte __thiscall cAICharControl2D_TestPath(cAICharControl *self, const vec3_t *a, const vec3_t *b, float r);
/* TestPath with the 7 lines aimed at b, the step of a forward walk (TestStep with r), then FloorAhead(b) */
byte __thiscall cAICharControl2D_TestPathEnd(cAICharControl *self, const vec3_t *a, const vec3_t *b, float r);
/* the 7 lines around a along the way from b to c (at most the radius plus 5 long); r not used */
byte __thiscall cAICharControl2D_TestStep(cAICharControl *self, const vec3_t *a, const vec3_t *b, const vec3_t *c,
	float r);
/* the corridor of the walk back from a to b: 5 lines of the length FactorDistTestColBackCharAI * radius around
 * the line between the floors, back along it (not down); r not used */
byte __thiscall cAICharControl2D_TestSides(cAICharControl *self, const vec3_t *a, const vec3_t *b, float r);
byte __thiscall cAICharControl2D_FloorAhead(cAICharControl *self, const vec3_t *p);
/* the floor below p raised by the floor distance of the character into out; 0 (out = p) without one */
byte __thiscall cAICharControl2D_FloorAt(cAICharControl *self, const vec3_t *p, vec3_t *out);
void __thiscall cAICharControl2D_StepGoal(cAICharControl *self, vec3_t *p);
byte __thiscall cAICharControl2D_TryStrafe(cAICharControl *self, int side, cEntity *hitEnt, byte back, float keep,
	byte fwd, float sd, byte e2);
byte __thiscall cAICharControl2D_StrafeBack(cAICharControl *self, const vec3_t *pos, const vec3_t *away,
	const vec3_t *dir, float dist, byte left, byte flag);
byte __thiscall cAICharControl2D_StrafeFwd(cAICharControl *self, const vec3_t *pos, const vec3_t *away,
	const vec3_t *dir, float dist, byte left, byte flag);

/* the A* search on CNodesWorld (AI/AStar.cpp, not in C), as CAStar2D_FindPath and CAStar2D_GetPointsVis */
byte __thiscall CAStar_FindPath(void *astar, cNWVecList *path, const vec3_t *dest, const vec3_t *pos, float r,
	float dist, int soft, byte *noPath, byte *reach, int *res);
void __thiscall CAStar_GetPointsVis(void *astar, const vec3_t *pos, const vec3_t **list, int max, int *count, float r);

/* ---- AI/AICharControl3D.cpp (src/game/AI/ICharControl3D.c): the slots as in 2D, in all three axes ---- */
cAICharControl *__thiscall cAICharControl3D_ctor(void *mem, cEntity *ent, cAIChar *ai, int *arg);
void __fastcall cAICharControl3D_dtor(cAICharControl *self);
cAICharControl *__thiscall cAICharControl3D_Delete(cAICharControl *self, byte flags);
void __thiscall cAICharControl3D_Draw(cAICharControl *self);
byte __thiscall cAICharControl3D_Facing(cAICharControl *self, cEntity *hitEnt, vec3_t *toE, vec3_t *toGoal);
/* the segment free of the level */
byte __thiscall cAICharControl3D_TestLine(cAICharControl *self, const vec3_t *a, const vec3_t *b);
/* the way of 1.5 radius to the side (3 up, 4 down) not free */
byte __thiscall cAICharControl3D_SideBlocked(cAICharControl *self, int side);
byte __thiscall cAICharControl3D_UpdateFollow(cAICharControl *self);
void __thiscall cAICharControl3D_CheckArrived(cAICharControl *self);
void __thiscall cAICharControl3D_Turn(cAICharControl *self);
/* walkDir from the distance to the target alone */
void __thiscall cAICharControl3D_Walk(cAICharControl *self);
byte __thiscall cAICharControl3D_FindPath(cAICharControl *self, const vec3_t *p);
byte __thiscall cAICharControl3D_FindRoute(cAICharControl *self);
float __thiscall cAICharControl3D_Dist(cAICharControl *self, const vec3_t *a, const vec3_t *b);
float __thiscall cAICharControl3D_DistSq(cAICharControl *self, const vec3_t *a, const vec3_t *b);
/* the 2D distance to keep, at most the distance to hitEnt plus 10 */
float __thiscall cAICharControl3D_KeepDist(cAICharControl *self, cEntity *hitEnt);
/* the two radii plus MarginCollCharAI */
float __thiscall cAICharControl3D_StrafeDist(cAICharControl *self, cEntity *hitEnt);
byte __thiscall cAICharControl3D_InFront(cAICharControl *self, cEntity *e, float ang);
byte __thiscall cAICharControl3D_Behind(cAICharControl *self, cEntity *e, float ang);
byte __thiscall cAICharControl3D_GoalAhead(cAICharControl *self);
byte __thiscall cAICharControl3D_OnFloor(cAICharControl *self, const vec3_t *p);
byte __thiscall cAICharControl3D_Near(cAICharControl *self, cEntity *e);
byte __thiscall cAICharControl3D_InWay(cAICharControl *self, vec3_t dest, cEntity *hitEnt);
void __thiscall cAICharControl3D_StepGoal(cAICharControl *self, vec3_t *p);
byte __thiscall cAICharControl3D_TryStrafe(cAICharControl *self, int side, cEntity *hitEnt, byte back, float keep,
	byte fwd, float sd, byte e2);

#endif
