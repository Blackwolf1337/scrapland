/*
 * Entities/More/ModMove.cpp of the game (src/game/Entities/More/ModMove.c): the two movers of every "Obj" (cObjEntity).
 * cObjMove (at 0x174 of the object, "MovePos", "MoveAng", "MoveTime") moves the entity in MoveTime seconds from where it
 * is to a target, with a fade curve, and calls OnEndMove at the end. cModMove (at 0x1e4, "LinkModel") puts the model of
 * the entity on a node of the model of another object, blended in by OffMul, and holds the head of the links of the
 * others that are linked to its own model.
 */
#ifndef SCRAP_MODMOVE_H
#define SCRAP_MODMOVE_H

#include "scrap/entity.h"
#include "scrap/pyfunction.h"

/* the fade curves of cObjMove (MoveFade) */
#define OBJMOVE_LINEAR 0
#define OBJMOVE_EASEIN 1		/* f * f * (2 - f) */
#define OBJMOVE_EASEOUT 2
#define OBJMOVE_SMOOTH 3		/* f * f * (3 - 2 f) */

typedef struct cObjMove {
	vec3_t startPos;		/* 0x00 where the move began */
	vec3_t startAng;		/* 0x0c */
	vec3_t lastPos;			/* 0x18 of the last tick, the velocity from it */
	vec3_t lastAng;			/* 0x24 */
	float time;			/* 0x30 -1 before the first tick of a move */
	float moveTime;			/* 0x34 "MoveTime" */
	byte active;			/* 0x38 */
	byte pad_39[3];
	vec3_t movePos;			/* 0x3c "MovePos" */
	vec3_t moveAng;			/* 0x48 "MoveAng", radians */
	cPy_Function onEnd;		/* 0x54 "OnEndMove" (entity) */
	int fade;			/* 0x68 "MoveFade", OBJMOVE_; back to 0 at the end */
	byte viewAng;			/* 0x6c "MoveViewAng" */
	byte pad_6d[3];
} cObjMove;
SD_OFFSET(cObjMove, active, 0x38);
SD_OFFSET(cObjMove, onEnd, 0x54);
SD_SIZE(cObjMove, 0x70);

typedef struct cModMove {
	cEntity *parent;		/* 0x00 the object */
	Nto1Node head;			/* 0x04 the links of the others to this object, target the object */
	Nto1Node link;			/* 0x14 in the head of the object linked to, data the object */
	int nodeIndex;			/* 0x24 the node of the model linked to, -1 */
	vec3_t offPos;			/* 0x28 "OffModPos" */
	float offMul;			/* 0x34 "OffMul": 0 own position .. 1 the node */
	float deltaOffMul;		/* 0x38 "DeltaOffMul" per second */
	byte useLinkAng;		/* 0x3c "UseLinkAng" */
	byte offPosAsPos;		/* 0x3d "OffModPosAsPos": the model at offPos */
	byte pad_3e[2];
} cModMove;
SD_OFFSET(cModMove, link, 0x14);
SD_OFFSET(cModMove, offPos, 0x28);
SD_SIZE(cModMove, 0x40);

cModMove *cModMove_ctor(cModMove *self);
void cModMove_dtor(cModMove *self);
byte cModMove_VarF(cModMove *self, const char *name, double *d, int set);
byte cModMove_VarI(cModMove *self, const char *name, int *i, int set);
byte cModMove_VarS(cModMove *self, const char *name, char **s, int set);
byte cModMove_VarV(cModMove *self, const char *name, double *x, double *y, double *z, int set);
/* links to the node of the model of the object of that name; "" unlinks */
void cModMove_Link(cModMove *self, const char *name, const char *node);
/* the render position (and angles with UseLinkAng) of the parent */
void cModMove_Update(cModMove *self, float dt);
void cModMove_SetParent(cModMove *self, cEntity *parent);

cObjMove *cObjMove_ctor(cObjMove *self);
void cObjMove_dtor(cObjMove *self);
/* the position, angles and velocities of e (a cObjEntity) for this tick of the move */
void cObjMove_Tick(cObjMove *self, cEntity *e, float dt);
/* a move of t seconds starts (t > 0) */
void cObjMove_SetTime(cObjMove *self, float t);

#endif
