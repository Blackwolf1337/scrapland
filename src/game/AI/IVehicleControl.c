/*
 * The move control of the vehicle AI, original source AI/AIVehicleControl.cpp (include/scrap/aivehiclecontrol.h):
 * the moves of SetMove (to a point over a path of the visibility graph, direct, the chase of the target, the
 * orientation, a path along a direction), the line tests of the corridor the vehicle needs, the smoothing of the
 * path, the turn, the strafe and the acceleration the control gives to the controls of the vehicle, and the debug
 * view. At the end the helpers of cAIVehicleGroup of the same source (the elevators, the repositioning clock).
 */
#include <float.h>
#include <string.h>

#include "scrap/aivehicle.h"
#include "scrap/autoaim.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/fxmisc.h"
#include "scrap/mathlib.h"
#include "scrap/scrap.h"
#include "scrap/scrapcollision.h"
#include "scrap/scrapfont.h"
#include "scrap/scrapmain.h"
#include "scrap/timer.h"
#include "scrap/vehicleentity.h"

/* the engine variables */
#define g_ShowPathAI GAME_GLOBAL_IN(int, GVar_ShowPathAI, 16)
#define g_DepthPathRouteOutDoorAI GAME_GLOBAL_IN(int, GVar_DepthPathRouteOutDoorAI, 16)
#define g_ShowVehAI (&GAME_GLOBAL_IN(cString, GVar_ShowVehAI, 16))
#define g_LimVelTurnObjectiveAI GAME_GLOBAL_IN(float, GVar_LimVelTurnObjectiveAI, 16)
#define g_DistBoostAI GAME_GLOBAL_IN(float, GVar_DistBoostAI, 16)
#define g_DepthSearchPosGoAI GAME_GLOBAL_IN(int, GVar_DepthSearchPosGoAI, 16)
#define g_TimeReactTurnAI GAME_GLOBAL_IN(float, GVar_TimeReactTurnAI, 16)
#define g_DistBrakeObjAI GAME_GLOBAL_IN(float, GVar_DistBrakeObjAI, 16)
#define g_MinAngleSearchPointAI GAME_GLOBAL_IN(float, GVar_MinAngleSearchPointAI, 16)
#define g_TimeMaxReachBoostAI GAME_GLOBAL_IN(float, GVar_TimeMaxReachBoostAI, 16)
#define g_VelTurnAI GAME_GLOBAL_IN(float, GVar_VelTurnAI, 16)
#define g_DecVelBrakeAI GAME_GLOBAL_IN(float, GVar_DecVelBrakeAI, 16)
#define g_ShowStopShipAI GAME_GLOBAL_IN(int, GVar_ShowStopShipAI, 16)
#define g_MinTimeReposAI GAME_GLOBAL_IN(float, GVar_MinTimeReposAI, 16)

#define AIVC_MAX_FREE 50		/* the free nodes the path keeps */
#define AIVC_MAX_LEN 10000.0f		/* the corridor tested ahead at most */
#define AIVC_NEAR 1600.0
#define AIVC_SAME 400.0
#define AIVC_STRAFE_FAR2 400000000.0f
#define AIVC_STRAFE_AHEAD 6.0f
#define AIVC_REL_SPEED 700.0f
#define AIVC_RAND_INV (1.0 / 32767.0)

#define AIVC_STRAFE_RADIUS 1000.0f	/* the strafe to the point only for smaller vehicles */
#define AIVC_DRAW_LEN 1000.0f		/* the lines of the nose and the velocity */
#define AIVC_DRAW_VEL2 10000.0f

int Vec3_Differ(const float *a, const float *b);

static const vec3_t aivc_fwd = { 0.0f, 0.0f, -1.0f };

static inline float VC_Radius(cAIVehicleControl *self) {
	return Obj_Vtbl(self->vehicle)->GetRadius(self->vehicle);
}

static inline const vec3_t *VC_Pos(cAIVehicleControl *self, vec3_t *buf) {
	return self->vehicle->vftable->GetPos(self->vehicle, buf);
}

static inline cEntity *VC_Target(cAIVehicleControl *self) {
	return self->target->target;
}

static inline const vec3_t *VC_TargetPos(cAIVehicleControl *self, vec3_t *buf) {
	cEntity *t = VC_Target(self);
	return t->vftable->GetPos(t, buf);
}

static inline cAutoAim *VC_AutoAim(cAIVehicleControl *self) {
	return &Veh(self->vehicle)->autoAim;
}

/* the three coordinates equal (ucomiss: NaN is unequal) */
static inline int VC_Same(const vec3_t *a, const vec3_t *b) {
	return a->x == b->x && a->y == b->y && a->z == b->z;
}

static inline float VC_Len(float x, float y, float z) {
	return (float)__builtin_sqrt((double)(y * y + x * x + z * z));
}

/* |q - p| */
static inline float VC_Dist(const vec3_t *p, const vec3_t *q) {
	return VC_Len(q->x - p->x, q->y - p->y, q->z - p->z);
}

/* the point of the node in front of head (the first one of a path), the iterator on it */
static vec3_t *VC_PathFirst(cAIVehicleControl *self) {
	cNWVecNode *h = self->path.head;

	if (h) {
		cNWVecNode *n = h->prev;
		self->path.iter = n;
		if (n) {
			return &n->v;
		}
	}
	return NULL;
}

/* the corridor of the vehicle from its position to p free */
static byte VC_Free(cAIVehicleControl *self, const vec3_t *p) {
	vec3_t buf;
	float r = VC_Radius(self);
	const vec3_t *pos = VC_Pos(self, &buf);

	return cAIVehicleControl_TestCorridor(self, pos, p, r);
}

static inline void VC_LifeMark(cAIVehicleControl *self) {
	int mark = Veh(self->vehicle)->wl.life + 1;

	self->strafeWait = 0.0f;
	self->lifeMark = mark;
}

static inline void VC_Release(cAIVehicleControl *self) {
	cVehicleControls *c = self->ctl;

	c->acc = 0.0f;
	c->vRot = 0.0f;
	c->hRot = 0.0f;
	c->boostKey = 0;
	c = self->ctl;
	c->str = 0.0f;
	c->vStr = 0.0f;
}

/* ---- construction ---- */

cAIVehicleControl *cAIVehicleControl_ctor(void *mem, cEntity *v, cAIVehicle *ai, void *params) {
	cAIVehicleControl *self = mem;

	self->vftable = CAIVEHICLECONTROL_VFTABLE;
	cSlotController_ctor(&self->slotStrafe, 0.2f);
	cSlotController_ctor(&self->slotArrive, 0.2f);
	cSlotController_ctor(&self->slotPath, 0.2f);
	memset(&self->path, 0, sizeof(self->path));
	cSlotController_ctor(&self->slotChase, 0.2f);
	cSlotController_ctor(&self->slotChaseSee, 0.33333334f);
	cSlotController_ctor(&self->slotChasePath, 5.0f);
	cSlotController_ctor(&self->slotChaseNear, 0.2f);
	cSlotController_ctor(&self->slotRepath, 3.0f);
	if (!v || !ai) {
		Con_Bluef("ERROR: Inicializaci\xf3n AI Vehicle Control. Entidad = NULL.\n");
		CheckFailed("cAIVehicleControl_Fail_Entity_NULL", __FILE__, __TIMESTAMP__, __LINE__);
	}
	self->vehicle = v;
	self->ai = ai;
	self->dest.y = 0.0f;
	self->dest.z = 0.0f;
	self->dest.x = 0.0f;
	self->point = self->dest;
	self->v168 = self->point;
	self->ctl = &Veh(self->vehicle)->controls;
	self->radius = VC_Radius(self);
	self->params = *(const cAIVehicleParams2 *)params;
	self->paramsInit = *(const cAIVehicleParams2 *)params;
	VC_AutoAim(self)->on = self->params.aim >= 4;
	self->inSight = 0;
	int r = crt_rand();
	self->turnX = 0.0f;
	self->turnY = 0.0f;
	self->strafeTime = 0.0f;
	self->strafeX = 0.0f;
	self->strafeY = 0.0f;
	self->strafeNext = (float)(r % (self->params.maxTimeStrafe + 1));
	self->path.maxFree = AIVC_MAX_FREE;
	self->boost = 0;
	self->target = NULL;
	self->angY = 0.0f;
	self->angX = 0.0f;
	self->rotB = 0.0f;
	self->rotA = 0.0f;
	self->turnYWant = 0.0f;
	self->turnXWant = 0.0f;
	self->accel = 0.0f;
	self->brake = 0.0f;
	self->prevMode = 0;
	self->mode = 0;
	self->prevNode = NULL;
	self->node = NULL;
	self->braking = 0;
	self->seen = 0;
	self->tooNear = 0;
	self->tooFar = 0;
	self->pathDirect = 0;
	self->pathFlat = 1;
	self->dist = 0.0f;
	self->distMin = 0.0f;
	self->dt = 0.0f;
	self->chaseMin = 0.0f;
	self->chaseMax = 0.0f;
	self->dir.x = -1.0f;
	self->dir.y = -1.0f;
	self->dir.z = -1.0f;
	self->pathNode = NULL;
	self->pathState = 0;
	self->lost = 0;
	self->unused_191 = 0;
	self->strafeOn = 0;
	self->strafeElapsed = 0.0f;
	self->strafeWait = 0.0f;
	self->lifeMark = Veh(self->vehicle)->wl.life;
	self->strafing = 0;
	self->direct = 1;
	self->inertia = 1;
	self->targetPos.x = -FLT_MAX;
	self->targetPos.y = -FLT_MAX;
	self->targetPos.z = -FLT_MAX;
	memset(self->corridor, 0, sizeof(self->corridor));
	memset(self->endCap, 0, sizeof(self->endCap));
	return self;
}

void cAIVehicleControl_dtor(cAIVehicleControl *self) {
	cNWVecList *l = &self->path;

	self->vftable = CAIVEHICLECONTROL_VFTABLE;
	DListVec3_Clear(l);
	while (l->freeNodes) {
		cNWVecNode *n = l->freeNodes;
		l->freeNodes = n->next;
		operator_delete_sized(n, sizeof(cNWVecNode));
	}
	l->numFree = 0;
}

cAIVehicleControl *cAIVehicleControl_Delete(cAIVehicleControl *self, byte flags) {
	cAIVehicleControl_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAIVEHICLECONTROL_SIZE);
	}
	return self;
}

void cAIVehicleControl_ApplyParams(cAIVehicleControl *self) {
	VC_AutoAim(self)->on = self->params.aim >= 4;
}

/* ---- the line tests ---- */

/* the segment a b and 8 segments around it at the radius of the vehicle (sideways, up and down, diagonal), each
 * at most AIVC_MAX_LEN long, free of the level; the segments kept for the debug view */
byte cAIVehicleControl_TestCorridor(cAIVehicleControl *self, const vec3_t *a, const vec3_t *b,
	float unused) {
	vec3_t a0 = *a, b0 = *b;
	vec3_t u, s, ns, t, nt, d1, d2, in, ul;
	vec3_t p[8], e[8];

	(void)unused;
	if (Engine_TestLine(a, b, NULL, NULL)) {
		return 0;
	}
	float r = VC_Radius(self);
	float len = NW_Normalize(&u, b->x - a->x, b->y - a->y, b->z - a->z);
	float lc = Float_MinSS(len, AIVC_MAX_LEN);
	float sx = u.y * 0.0f - u.z, sy = u.z * 0.0f - u.x * 0.0f, sz = u.x - u.y * 0.0f;
	if (sx == 0.0f && sy == 0.0f && sz == 0.0f) {
		sx = 1.0f;
		sy = 0.0f;
		sz = 0.0f;
	}
	NW_Normalize(&s, sx, sy, sz);
	ul.x = u.x * lc;
	ul.y = u.y * lc;
	ul.z = u.z * lc;

	p[0].x = s.x * r + a->x;
	p[0].y = s.y * r + a->y;
	p[0].z = s.z * r + a->z;
	e[0].x = ul.x + p[0].x;
	e[0].y = ul.y + p[0].y;
	e[0].z = ul.z + p[0].z;
	if (Engine_TestLine(&p[0], &e[0], NULL, NULL)) {
		return 0;
	}
	ns.x = -s.x;
	ns.y = -s.y;
	ns.z = -s.z;
	p[1].x = a->x + ns.x * r;
	p[1].y = a->y + ns.y * r;
	p[1].z = ns.z * r + a->z;
	e[1].x = ul.x + p[1].x;
	e[1].y = ul.y + p[1].y;
	e[1].z = ul.z + p[1].z;
	if (Engine_TestLine(&p[1], &e[1], NULL, NULL)) {
		return 0;
	}
	in.x = ns.z * u.y - ns.y * u.z;
	in.y = ns.x * u.z - ns.z * u.x;
	in.z = ns.y * u.x - ns.x * u.y;
	Vec3_NormalizeTo(&t, &in);
	p[2].x = t.x * r + a->x;
	p[2].y = t.y * r + a->y;
	p[2].z = t.z * r + a->z;
	e[2].x = ul.x + p[2].x;
	e[2].y = ul.y + p[2].y;
	e[2].z = ul.z + p[2].z;
	if (Engine_TestLine(&p[2], &e[2], NULL, NULL)) {
		return 0;
	}
	nt.x = -t.x;
	nt.y = -t.y;
	nt.z = -t.z;
	p[3].x = a->x + nt.x * r;
	p[3].y = nt.y * r + a->y;
	p[3].z = nt.z * r + a->z;
	e[3].x = ul.x + p[3].x;
	e[3].y = ul.y + p[3].y;
	e[3].z = ul.z + p[3].z;
	if (Engine_TestLine(&p[3], &e[3], NULL, NULL)) {
		return 0;
	}
	in.x = nt.x + ns.x;
	in.y = nt.y + ns.y;
	in.z = nt.z + ns.z;
	Vec3_NormalizeTo(&d1, &in);
	p[4].x = d1.x * r + a->x;
	p[4].y = d1.y * r + a->y;
	p[4].z = d1.z * r + a->z;
	e[4].x = ul.x + p[4].x;
	e[4].y = ul.y + p[4].y;
	e[4].z = ul.z + p[4].z;
	if (Engine_TestLine(&p[4], &e[4], NULL, NULL)) {
		return 0;
	}
	p[5].x = a->x + -d1.x * r;
	p[5].y = -d1.y * r + a->y;
	p[5].z = -d1.z * r + a->z;
	e[5].x = ul.x + p[5].x;
	e[5].y = ul.y + p[5].y;
	e[5].z = ul.z + p[5].z;
	if (Engine_TestLine(&p[5], &e[5], NULL, NULL)) {
		return 0;
	}
	in.x = ns.x - nt.x;
	in.y = ns.y - nt.y;
	in.z = ns.z - nt.z;
	Vec3_NormalizeTo(&d2, &in);
	p[6].x = d2.x * r + a->x;
	p[6].y = d2.y * r + a->y;
	p[6].z = d2.z * r + a->z;
	e[6].x = ul.x + p[6].x;
	e[6].y = ul.y + p[6].y;
	e[6].z = ul.z + p[6].z;
	if (Engine_TestLine(&p[6], &e[6], NULL, NULL)) {
		return 0;
	}
	p[7].x = a->x + -d2.x * r;
	p[7].y = -d2.y * r + a->y;
	p[7].z = -d2.z * r + a->z;
	e[7].x = ul.x + p[7].x;
	e[7].y = ul.y + p[7].y;
	e[7].z = ul.z + p[7].z;
	if (Engine_TestLine(&p[7], &e[7], NULL, NULL)) {
		return 0;
	}
	self->corridor[0][0] = a0;
	self->corridor[0][1] = b0;
	for (int i = 0; i < 8; i++) {
		self->corridor[i + 1][0] = p[i];
		self->corridor[i + 1][1] = e[i];
	}
	return 1;
}

/* the two diagonals across the end of the corridor along dir (len at most the radius) free of the level */
byte cAIVehicleControl_TestEnd(cAIVehicleControl *self, const vec3_t *a, const vec3_t *dir, float len) {
	vec3_t e, s, w, m, q[4];
	float r;

	r = VC_Radius(self);
	if (!(r > len)) {
		len = VC_Radius(self);
	}
	e.x = a->x + dir->x * len;
	e.y = a->y + dir->y * len;
	e.z = a->z + dir->z * len;
	float k = dir->y * 0.0f;
	float sx = k - dir->z, sy = dir->z * 0.0f - dir->x * 0.0f, sz = dir->x - k;
	if (sx == 0.0f && sy == 0.0f && sz == 0.0f) {
		sx = 1.0f;
		sy = 0.0f;
		sz = 0.0f;
	}
	NW_Normalize(&s, sx, sy, sz);
	NW_Normalize(&w, dir->y * s.z - dir->z * s.y, dir->z * s.x - dir->x * s.z, dir->x * s.y - dir->y * s.x);
	NW_Normalize(&m, w.x + s.x, w.y + s.y, w.z + s.z);
	r = VC_Radius(self);
	q[0].x = m.x * r + e.x;
	q[0].y = m.y * r + e.y;
	q[0].z = m.z * r + e.z;
	r = VC_Radius(self);
	q[1].x = -m.x * r + e.x;
	q[1].y = -m.y * r + e.y;
	q[1].z = -m.z * r + e.z;
	if (Engine_TestLine(&q[0], &q[1], NULL, NULL)) {
		return 0;
	}
	NW_Normalize(&m, s.x - w.x, s.y - w.y, s.z - w.z);
	r = VC_Radius(self);
	q[2].x = m.x * r + e.x;
	q[2].y = m.y * r + e.y;
	q[2].z = m.z * r + e.z;
	r = VC_Radius(self);
	q[3].x = -m.x * r + e.x;
	q[3].y = -m.y * r + e.y;
	q[3].z = -m.z * r + e.z;
	if (Engine_TestLine(&q[2], &q[3], NULL, NULL)) {
		return 0;
	}
	self->endCap[0][0] = q[0];
	self->endCap[0][1] = q[1];
	self->endCap[1][0] = q[2];
	self->endCap[1][1] = q[3];
	return 1;
}

byte cAIVehicleControl_TestEndTo(cAIVehicleControl *self, const vec3_t *a, const vec3_t *b) {
	vec3_t dir;
	float len = NW_Normalize(&dir, b->x - a->x, b->y - a->y, b->z - a->z);

	return cAIVehicleControl_TestEnd(self, a, &dir, len);
}

/* ---- the checks ---- */

/* the target of the chase or of the orientation still there; else the event 3 (target gone) */
byte cAIVehicleControl_CheckTarget(cAIVehicleControl *self) {
	if (self->mode == AIVC_CHASE || self->mode == AIVC_ORIENT) {
		cEntity *t = VC_Target(self);
		if (!t || !t->active) {
			const char *name = self->vehicle->name;
			int in = cAIVehicle_GetInternState(self->ai);
			int st = cAIVehicle_GetState(self->ai);
			Con_Bluef("OBJECTIVE_DIED - cAIVehicleControl::CheckIntensive - Entidad: %s - State: %d - StateIntern: "
				"%d\n", name, st, in);
			self->ai->vftable->Event(self->ai, 3);
			return 0;
		}
	}
	return 1;
}

/* on the last node of a path: the corridor to the goal blocked makes the path direct and lost */
byte cAIVehicleControl_CheckPathFree(cAIVehicleControl *self) {
	if (self->pathState != 0 && cAIVehicleControl_NodeIsLast(self) && !VC_Free(self, &self->dest)) {
		self->lost = 1;
		cAIVehicleControl_SetDirectPath(self, &self->dest);
		self->pathState = 2;
		return 0;
	}
	return 1;
}

/* the point to steer to: the goal when the graph sees it, else the point or the node when they are free, else one
 * of the next two nodes; a new path when none is */
byte cAIVehicleControl_FindPoint(cAIVehicleControl *self) {
	vec3_t buf;

	self->seen = 0;
	self->prevNode = NULL;
	self->lost = 0;
	float r2 = VC_Radius(self);
	float r = VC_Radius(self);
	const vec3_t *pos = VC_Pos(self, &buf);
	if (CNodesWorld_TestPath(g_NodesWorld, pos, &self->dest, r, r2, NULL, 0)) {
		return cAIVehicleControl_SetDirectPath(self, &self->dest);
	}
	if (VC_Free(self, &self->point)) {
		return 1;
	}
	if (VC_Free(self, self->node)) {
		self->point = *self->node;
		self->seen = 1;
		return 1;
	}
	cNWVecNode *n = self->path.iter->next;
	self->path.iter = n;
	if (n != self->path.head) {
		vec3_t *p = &n->v;
		vec3_t *prev = p;
		int k = 0;
		while (p) {
			if (k >= 2) {
				goto found;
			}
			if (VC_Free(self, p)) {
				goto found;
			}
			prev = p;
			n = self->path.iter->next;
			self->path.iter = n;
			if (n == self->path.head) {
				break;
			}
			k++;
			p = &n->v;
		}
		goto repath;
found:
		if (p && k != 2) {
			self->node = p;
			self->prevNode = prev;
			self->point = *p;
			self->seen = 1;
			return 1;
		}
	}
repath:
	return cAIVehicleControl_SetPath(self, &self->dest);
}

/* FindPoint of the path along a direction: a new path from the nose of the vehicle when no point is free */
byte cAIVehicleControl_FindPointDir(cAIVehicleControl *self) {
	vec3_t rot, out;

	self->seen = 0;
	self->prevNode = NULL;
	if (VC_Free(self, &self->point)) {
		return 1;
	}
	if (VC_Free(self, self->node)) {
		self->point = *self->node;
		self->seen = 1;
		return 1;
	}
	cNWVecNode *n = self->path.iter->next;
	self->path.iter = n;
	if (n != self->path.head) {
		vec3_t *p = &n->v;
		vec3_t *prev = p;
		int k = 0;
		while (p) {
			if (k >= 2) {
				goto found;
			}
			if (VC_Free(self, p)) {
				goto found;
			}
			prev = p;
			n = self->path.iter->next;
			self->path.iter = n;
			if (n == self->path.head) {
				break;
			}
			k++;
			p = &n->v;
		}
		goto rebuild;
found:
		if (p && k != 2) {
			self->node = p;
			self->prevNode = prev;
			self->point = *p;
			self->seen = 1;
			return 1;
		}
	}
rebuild:
	self->pathDirect = 0;
	self->dir = *Vec3_Rotate(&out, &aivc_fwd, Obj_Vtbl(self->vehicle)->GetAng(self->vehicle, &rot));
	return cAIVehicleControl_RebuildPath(self);
}

/* the target seen from the vehicle: beyond chaseMax the corridor, nearer the segment */
static byte VC_SeesTarget(cAIVehicleControl *self, const vec3_t *tp) {
	vec3_t a, b;
	const vec3_t *p = VC_Pos(self, &a);

	if (VC_Dist(tp, p) > self->chaseMax) {
		float r = self->radius;
		const vec3_t *q = VC_Pos(self, &b);
		return cAIVehicleControl_TestCorridor(self, q, tp, r);
	}
	return !Engine_TestLine(VC_Pos(self, &b), tp, NULL, NULL);
}

/* the chase: directly at the target while it is seen, else a path to it, renewed now and then */
byte cAIVehicleControl_ChasePath(cAIVehicleControl *self) {
	vec3_t tp, buf;
	float t;

	VC_TargetPos(self, &tp);
	if (self->inSight) {
		cSlotController *slot = &self->slotChase;
		if (!(byte)cSlotController_Tick(slot, self->dt) || !cSlotController_FireReset(slot, &t)) {
			return 1;
		}
		do {
			if (VC_SeesTarget(self, &tp)) {
				cAIVehicleControl_SetDirectPath(self, &tp);
			} else {
				self->inSight = 0;
				if (!cAIVehicleControl_SetPath(self, &tp)) {
					return 0;
				}
				cSlotController_Restart(&self->slotChaseNear);
				cSlotController_Restart(&self->slotChaseSee);
				cSlotController_Restart(&self->slotPath);
			}
		} while (cSlotController_FireReset(slot, &t));
		return 1;
	}
	cSlotController *slot = &self->slotChaseNear;
	if ((byte)cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &t)) {
		do {
			const vec3_t *p = VC_Pos(self, &buf);
			if (AIVC_NEAR > __builtin_fabs((double)(p->x - self->dest.x)) &&
				AIVC_NEAR > __builtin_fabs((double)(p->y - self->dest.y)) &&
				AIVC_NEAR > __builtin_fabs((double)(p->z - self->dest.z)) && !cAIVehicleControl_SetPath(self, &tp)) {
				return 0;
			}
		} while (cSlotController_FireReset(slot, &t));
	}
	slot = &self->slotChaseSee;
	if ((byte)cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &t)) {
		do {
			if (VC_SeesTarget(self, &tp)) {
				self->inSight = 1;
				cAIVehicleControl_SetDirectPath(self, &tp);
				cSlotController_Restart(&self->slotChase);
				return 1;
			}
		} while (cSlotController_FireReset(slot, &t));
	}
	slot = &self->slotChasePath;
	if ((byte)cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &t)) {
		do {
			if (!cAIVehicleControl_SetPath(self, &tp)) {
				return 0;
			}
			cSlotController_Restart(&self->slotPath);
			cSlotController_Restart(&self->slotChaseNear);
		} while (cSlotController_FireReset(slot, &t));
	}
	return 1;
}

/* the orientation: the target point while the segment to it is free */
byte cAIVehicleControl_ChaseLine(cAIVehicleControl *self) {
	vec3_t tp, buf;
	float t;

	VC_TargetPos(self, &tp);
	if (self->inSight) {
		cSlotController *slot = &self->slotChase;
		if ((byte)cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &t)) {
			do {
				if (Engine_TestLine(VC_Pos(self, &buf), &tp, NULL, NULL)) {
					self->inSight = 0;
					cSlotController_Restart(&self->slotChaseSee);
				} else {
					cAIVehicleControl_SetDirectPath(self, &tp);
				}
			} while (cSlotController_FireReset(slot, &t));
		}
		return 1;
	}
	cSlotController *slot = &self->slotChaseSee;
	if ((byte)cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &t)) {
		do {
			if (!Engine_TestLine(VC_Pos(self, &buf), &tp, NULL, NULL)) {
				self->inSight = 1;
				cAIVehicleControl_SetDirectPath(self, &tp);
				cSlotController_Restart(&self->slotChase);
			}
		} while (cSlotController_FireReset(slot, &t));
	}
	return 1;
}

/* at the goal: the event 2 (arrived) within dist, 4 within four times dist, else 5 (or when the AI does not take
 * it) */
void cAIVehicleControl_CheckArrive(cAIVehicleControl *self) {
	cSlotController *slot = &self->slotArrive;
	vec3_t buf;
	float t;
	int ev;

	if ((byte)cSlotController_Tick(slot, self->dt)) {
		while (cSlotController_FireReset(slot, &t)) {
		}
	}
	if (!VC_Same(&self->point, &self->dest)) {
		return;
	}
	const vec3_t *p = VC_Pos(self, &buf);
	float d = VC_Dist(p, &self->dest);
	if (self->dist > d) {
		ev = 2;
	} else if (self->dist * 4.0f > d) {
		ev = 4;
	} else {
		goto other;
	}
	if (self->ai->vftable->Event(self->ai, ev)) {
		return;
	}
other:
	self->ai->vftable->Event(self->ai, 5);
}

/* ---- the update ---- */

void cAIVehicleControl_Update(cAIVehicleControl *self, float dt) {
	if (!cAIVehicle_GetChanged(self->ai) &&
		(!cAIVehicle_Think(self->ai) || !cAIVehicleControl_CheckTarget(self))) {
		return;
	}
	self->dt = dt;
	switch (self->mode) {
	case AIVC_POINT:
		cAIVehicleControl_UpdatePoint(self, dt);
		return;
	case AIVC_DIRECT:
		cAIVehicleControl_UpdateDirect(self, dt);
		return;
	case AIVC_CHASE:
		cAIVehicleControl_UpdateChase(self, dt);
		return;
	case AIVC_ORIENT:
		cAIVehicleControl_UpdateOrient(self, dt);
		return;
	case AIVC_PATH:
		cAIVehicleControl_UpdatePath(self, dt);
		return;
	}
}

/* the turn wanted for the angle left, at most lim from the last one; the control gets it negated */
static float VC_Turn(float want, float *cur, float *store, float lim) {
	float c = *cur;

	*store = want;
	if (__builtin_fabs((double)(want - c)) > (double)lim) {
		*store = want > c ? c + lim : c - lim;
	}
	*cur = *store;
	return -*store;
}

/* the turn to the target of the chase (factor 2 * CONTROL_AIM) */
void cAIVehicleControl_TurnToTarget(cAIVehicleControl *self) {
	vec3_t a, b, r, d, want, cur;
	const vec3_t *tp = VC_TargetPos(self, &a);
	const vec3_t *p = VC_Pos(self, &b);

	d.x = p->x - tp->x;
	d.y = p->y - tp->y;
	d.z = p->z - tp->z;
	Vec3_GetRotAng(&want, &d);
	Vec3_NormAng(&cur, Obj_Vtbl(self->vehicle)->GetAng(self->vehicle, &r));
	if (self->direct) {
		Obj_Vtbl(self->vehicle)->SetAng(self->vehicle, &want);
		return;
	}
	float ax = NormAng(cur.x - want.x);
	float f = (float)self->params.aim + (float)self->params.aim;
	self->angX = ax;
	self->ctl->vRot = VC_Turn((ax * f) / M_PI_F, &self->turnX, &self->turnXWant, g_LimVelTurnObjectiveAI);
	float ay = NormAng(cur.y - want.y);
	self->angY = ay;
	self->ctl->hRot = VC_Turn((ay * f) / M_PI_F, &self->turnY, &self->turnYWant, g_LimVelTurnObjectiveAI);
}

/* the turn to the point (factor rotA, at most rotB) */
void cAIVehicleControl_TurnToPoint(cAIVehicleControl *self) {
	vec3_t b, r, d, want, cur;
	const vec3_t *p = VC_Pos(self, &b);

	d.x = p->x - self->point.x;
	d.y = p->y - self->point.y;
	d.z = p->z - self->point.z;
	Vec3_GetRotAng(&want, &d);
	Vec3_NormAng(&cur, Obj_Vtbl(self->vehicle)->GetAng(self->vehicle, &r));
	if (self->direct) {
		Obj_Vtbl(self->vehicle)->SetAng(self->vehicle, &want);
		return;
	}
	float ax = NormAng(cur.x - want.x);
	self->angX = ax;
	self->ctl->vRot = VC_Turn((self->rotA * ax) / M_PI_F, &self->turnX, &self->turnXWant, self->rotB);
	float ay = NormAng(cur.y - want.y);
	self->angY = ay;
	self->ctl->hRot = VC_Turn((self->rotA * ay) / M_PI_F, &self->turnY, &self->turnYWant, self->rotB);
}

/* the point and the target position after a step of the path slot */
static void VC_StepDone(cAIVehicleControl *self) {
	vec3_t buf;

	if (!VC_Same(&self->point, &self->dest)) {
		cAIVehicleControl_Smooth(self);
	}
	self->targetPos = *VC_Pos(self, &buf);
}

/* the path slot: the point of the path; while lost a new path to the goal every 3 s */
byte cAIVehicleControl_PathStep(cAIVehicleControl *self) {
	cSlotController *slot = &self->slotPath;
	float t;

	if ((byte)cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &t)) {
		do {
			if (cAIVehicleControl_CheckPathFree(self)) {
				if (!cAIVehicleControl_FindPoint(self)) {
					return 0;
				}
				VC_StepDone(self);
			}
		} while (cSlotController_FireReset(slot, &t));
	}
	if (self->lost) {
		slot = &self->slotRepath;
		if ((byte)cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &t)) {
			do {
				if (!cAIVehicleControl_SetPath(self, &self->dest)) {
					return 0;
				}
				if (cAIVehicleControl_CheckPathFree(self)) {
					if (!cAIVehicleControl_FindPoint(self)) {
						return 0;
					}
					VC_StepDone(self);
				}
			} while (cSlotController_FireReset(slot, &t));
		}
	}
	return 1;
}

/* PathStep of the chase: the new paths lead to the target */
byte cAIVehicleControl_PathStepChase(cAIVehicleControl *self) {
	cSlotController *slot = &self->slotPath;
	vec3_t buf;
	float t;

	if ((byte)cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &t)) {
		do {
			if (cAIVehicleControl_CheckPathFree(self)) {
				if (!cAIVehicleControl_FindPoint(self)) {
					return 0;
				}
				VC_StepDone(self);
			}
		} while (cSlotController_FireReset(slot, &t));
	}
	if (self->lost) {
		slot = &self->slotRepath;
		if ((byte)cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &t)) {
			do {
				if (!cAIVehicleControl_SetPath(self, VC_TargetPos(self, &buf))) {
					return 0;
				}
				if (cAIVehicleControl_CheckPathFree(self)) {
					if (!cAIVehicleControl_FindPoint(self)) {
						return 0;
					}
					VC_StepDone(self);
				}
			} while (cSlotController_FireReset(slot, &t));
		}
	}
	return 1;
}

byte cAIVehicleControl_PathStepDir(cAIVehicleControl *self) {
	cSlotController *slot = &self->slotPath;
	float t;

	if ((byte)cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &t)) {
		do {
			if (!cAIVehicleControl_FindPointDir(self)) {
				return 0;
			}
			VC_StepDone(self);
		} while (cSlotController_FireReset(slot, &t));
	}
	return 1;
}

/* ---- the strafe ---- */

/* the strafe toward the point in the frame of the vehicle (small vehicles only; lost: at random) */
void cAIVehicleControl_StrafeToPoint(cAIVehicleControl *self) {
	vec3_t b, d, rot, loc, n;

	if (self->lost) {
		cAIVehicleControl_StrafeRandom(self);
		return;
	}
	if (!(AIVC_STRAFE_RADIUS > VC_Radius(self))) {
		self->ctl->str = 0.0f;
		self->ctl->vStr = 0.0f;
		return;
	}
	const vec3_t *p = VC_Pos(self, &b);
	d.x = self->point.x - p->x;
	d.y = self->point.y - p->y;
	d.z = self->point.z - p->z;
	Obj_Vtbl(self->vehicle)->GetAng(self->vehicle, &rot);
	rot.x = -rot.x;
	rot.y = rot.y + M_PI_F;
	Vec3_RotateInv(&loc, &d, &rot);
	NW_Normalize(&n, loc.x, loc.y, loc.z);
	self->ctl->str = -n.x;
	self->ctl->vStr = -n.y;
}

/* the point ahead along the velocity (6 times the radius away) blocked */
static byte VC_AheadBlocked(cAIVehicleControl *self) {
	vec3_t v, n, end, b, c;

	Obj_Vtbl(self->vehicle)->GetVel(self->vehicle, &v);
	NW_Normalize(&n, v.x, v.y, v.z);
	float ox = n.x * AIVC_STRAFE_AHEAD, oy = n.y * AIVC_STRAFE_AHEAD, oz = n.z * AIVC_STRAFE_AHEAD;
	float r = VC_Radius(self);
	ox = ox * r;
	oy = oy * r;
	oz = oz * r;
	const vec3_t *p = VC_Pos(self, &b);
	end.x = p->x + ox;
	end.y = p->y + oy;
	end.z = p->z + oz;
	return Engine_TestLine(VC_Pos(self, &c), &end, NULL, NULL);
}

/* the dodge while the vehicle loses life (and now and then at random): sideways for a random time; 0 while it
 * does not dodge */
byte cAIVehicleControl_StrafeDodge(cAIVehicleControl *self) {
	cSlotController *slot = &self->slotStrafe;
	vec3_t b;
	float t;

	if (!self->strafeOn) {
		VC_LifeMark(self);
		return 0;
	}
	float dt = self->dt;
	self->strafeTime = self->strafeTime + dt;
	self->strafeElapsed = self->strafeElapsed + dt;
	if (!(byte)cSlotController_Tick(slot, dt) || !cSlotController_FireReset(slot, &t)) {
		return 1;
	}
	do {
		byte rnd = 0;
		byte go = 1;
		if (self->strafeElapsed >= self->strafeWait) {
			rnd = crt_rand() % 25 == 0;
		}
		cVehicleEntity *v = Veh(self->vehicle);
		if (self->strafeElapsed >= self->strafeWait && v->wl.life == self->lifeMark && !rnd) {
			self->strafeX = 0.0f;
			go = 0;
		} else if (v->wl.life < self->lifeMark || rnd) {
			float next = self->strafeNext + 1.0f;
			self->strafeElapsed = 0.0f;
			self->strafeWait = 10.0f;
			self->strafeTime = next;
		}
		self->lifeMark = v->wl.life;
		if (go) {
			const vec3_t *p = VC_Pos(self, &b);
			float dx = p->x - self->point.x, dy = p->y - self->point.y, dz = p->z - self->point.z;
			if (AIVC_STRAFE_FAR2 > dy * dy + dx * dx + dz * dz) {
				self->strafeX = 0.0f;
				go = 0;
			} else if (VC_AheadBlocked(self)) {
				self->strafeX = 0.0f;
				go = 0;
				self->strafeTime = 0.0f;
			} else if (self->strafeTime > self->strafeNext) {
				int r = crt_rand() % 101;
				self->strafeTime = 0.0f;
				self->strafeNext = (float)r / 100.0f + 0.5f;
				float sx = self->ctl->str;
				if (0.0f > sx) {
					self->strafeX = 1.0f;
				} else if (sx > 0.0f) {
					self->strafeX = -1.0f;
				} else {
					self->strafeX = crt_rand() % 2 ? 1.0f : -1.0f;
				}
			}
		}
		self->strafeY = 0.0f;
		self->ctl->str = self->strafeX;
		self->ctl->vStr = self->strafeY;
		if (!go) {
			return 0;
		}
	} while (cSlotController_FireReset(slot, &t));
	return 1;
}

/* the strafe of the chase between chaseMin / 2 and chaseMax: random sideways speeds of MAX_VEL_STRAFE */
void cAIVehicleControl_StrafeChase(cAIVehicleControl *self) {
	cSlotController *slot = &self->slotStrafe;
	vec3_t a, b;
	float t;

	self->strafeTime = self->dt + self->strafeTime;
	if (!(byte)cSlotController_Tick(slot, self->dt) || !cSlotController_FireReset(slot, &t)) {
		return;
	}
	do {
		const vec3_t *tp = VC_TargetPos(self, &a);
		const vec3_t *p = VC_Pos(self, &b);
		if (!(VC_Dist(tp, p) > self->chaseMin * 0.5f)) {
			goto zero;
		}
		tp = VC_TargetPos(self, &a);
		p = VC_Pos(self, &b);
		if (!(self->chaseMax > VC_Dist(tp, p))) {
			goto zero;
		}
		if (VC_AheadBlocked(self)) {
			self->strafeX = -self->ctl->str;
		}
		if (!(self->strafeTime > self->strafeNext)) {
			goto write;
		}
		{
			int r = crt_rand() % (self->params.maxTimeStrafe + 1);
			self->strafeTime = 0.0f;
			self->strafeNext = (float)r;
			r = crt_rand() % 6;
			float vs = self->params.maxVelStrafe;
			self->strafeX = ((vs * 0.75f) / 5.0f) * (float)r + vs * 0.25f;
			float sx = self->strafeX;
			if (crt_rand() % 2 == 0) {
				sx = -sx;
			}
			self->strafeX = sx;
		}
		goto y0;
zero:
		self->strafeX = 0.0f;
		self->strafeTime = 0.0f;
y0:
		self->strafeY = 0.0f;
write:
		self->ctl->str = self->strafeX;
		self->ctl->vStr = self->strafeY;
	} while (cSlotController_FireReset(slot, &t));
}

/* the strafe while lost: small random sideways speeds */
void cAIVehicleControl_StrafeRandom(cAIVehicleControl *self) {
	cSlotController *slot = &self->slotStrafe;
	float t;

	self->strafeTime = self->dt + self->strafeTime;
	if (!(byte)cSlotController_Tick(slot, self->dt) || !cSlotController_FireReset(slot, &t)) {
		return;
	}
	do {
		if (VC_AheadBlocked(self)) {
			self->strafeX = -self->ctl->str;
		}
		if (self->strafeTime > self->strafeNext) {
			int r = crt_rand() % 3;
			self->strafeTime = 0.0f;
			self->strafeNext = (float)r;
			r = crt_rand() % 6;
			self->strafeX = (float)r * 0.0375f + 0.0625f;
			float sx = self->strafeX;
			if (crt_rand() % 2 == 0) {
				sx = -sx;
			}
			self->strafeX = sx;
			self->strafeY = 0.0f;
		}
		self->ctl->str = self->strafeX;
		self->ctl->vStr = self->strafeY;
	} while (cSlotController_FireReset(slot, &t));
}

/* ---- the acceleration ---- */

/* the velocity turned along the nose of the vehicle (fwd), its speed kept */
static void VC_VelAlongNose(cAIVehicleControl *self, const vec3_t *fwd, float speed) {
	vec3_t n, v;
	const vec3_t *u = Vec3_NormalizeTo(&n, fwd);

	v.x = u->x * speed;
	v.y = u->y * speed;
	v.z = u->z * speed;
	Obj_Vtbl(self->vehicle)->SetVel(self->vehicle, &v);
}

static inline float VC_Speed(cAIVehicleControl *self) {
	vec3_t v;
	const vec3_t *p = Obj_Vtbl(self->vehicle)->GetVel(self->vehicle, &v);

	return VC_Len(p->x, p->y, p->z);
}

/* the nose of the vehicle and the angle between it and the point */
static float VC_NoseAngle(cAIVehicleControl *self, vec3_t *fwd) {
	vec3_t rot, b, d;

	Vec3_Rotate(fwd, &aivc_fwd, Obj_Vtbl(self->vehicle)->GetAng(self->vehicle, &rot));
	const vec3_t *p = VC_Pos(self, &b);
	d.x = self->point.x - p->x;
	d.y = self->point.y - p->y;
	d.z = self->point.z - p->z;
	return (float)Vec3_Angle(fwd, &d);
}

/* the boost button: on near the goal of the point (when given) or far from it with the nose on it */
static void VC_Boost(cAIVehicleControl *self, byte nearGoal, float distP, float ang) {
	self->ctl->boostKey = Veh(self->vehicle)->boost == 0.0f && self->boost &&
		(nearGoal || (distP > g_DistBoostAI && M_PI_F / 16.0f > ang));
}

/* the acceleration of the chase while the target is not seen: by the distance to it, the brakes before the point */
void cAIVehicleControl_AccelToTarget(cAIVehicleControl *self) {
	vec3_t a, b, fwd;
	byte slow = 0;
	float val = 0.0f;

	if (self->lost) {
		self->ctl->acc = 0.0f;
		return;
	}
	const vec3_t *tp = VC_TargetPos(self, &a);
	const vec3_t *p = VC_Pos(self, &b);
	float f = VC_Dist(tp, p) / self->chaseMax;
	self->accel = f;
	if (f > 1.0f) {
		self->accel = 1.0f;
	}
	p = VC_Pos(self, &b);
	float distP = VC_Dist(p, &self->point);
	float ang = VC_NoseAngle(self, &fwd);
	if (self->params.brake >= 1 && !VC_Same(&self->point, &self->dest)) {
		float speed = VC_Speed(self);
		if (speed > g_VelTurnAI) {
			float tt = distP / speed;
			if (g_TimeReactTurnAI > tt) {
				val = tt / g_TimeReactTurnAI;
				slow = 1;
			}
		}
	} else if (VC_Same(&self->point, &self->dest) && g_DistBrakeObjAI > distP) {
		float speed = VC_Speed(self);
		if (speed > 0.0f) {
			VC_VelAlongNose(self, &fwd, speed);
		}
	}
	switch (self->params.brake) {
	case 1:
		if (slow) {
			self->ctl->acc = val;
			break;
		}
		/* fall through */
	case 0:
		self->ctl->acc = self->accel;
		break;
	}
	VC_Boost(self, 0, distP, ang);
}

/* the acceleration of the chase: back below chaseMin, ahead beyond chaseMax (with a hysteresis to the middle); the
 * boost when the target cannot be reached in TIME_MAX_REACH_BOOST */
void cAIVehicleControl_AccelChase(cAIVehicleControl *self) {
	vec3_t a, b, fwd;

	self->braking = 0;
	cEntity *tg = VC_Target(self);
	const vec3_t *p = VC_Pos(self, &a);
	const vec3_t *tp = tg->vftable->GetPos(tg, &b);
	float dist = VC_Dist(p, tp);
	float mx = self->chaseMax, mn = self->chaseMin;
	float mid = (mx + mn) * 0.5f;
	if (mn > dist || (self->tooNear && mid > dist)) {
		self->ctl->acc = -Float_MinSS(1.0f, Float_MaxSS(0.0f, (mid - dist) / (mid - mn)));
		self->tooNear = 1;
	} else if (dist > mx || (self->tooFar && dist > mid)) {
		self->ctl->acc = Float_MinSS(1.0f, Float_MaxSS(0.0f, (dist - mid) / (mx - mid)));
		self->tooFar = 1;
	} else {
		self->ctl->acc = 0.0f;
		self->tooNear = 0;
		self->tooFar = 0;
	}
	vec3_t rot, d;
	Vec3_Rotate(&fwd, &aivc_fwd, Obj_Vtbl(self->vehicle)->GetAng(self->vehicle, &rot));
	p = VC_Pos(self, &a);
	d.x = self->point.x - p->x;
	d.y = self->point.y - p->y;
	d.z = self->point.z - p->z;
	double ang = Vec3_Angle(&fwd, &d);
	if (Veh(self->vehicle)->boost == 0.0f && (double)(M_PI_F / 16.0f) > ang && dist > g_DistBoostAI) {
		vec3_t v;
		tg = VC_Target(self);
		const vec3_t *tv = Obj_Vtbl(tg)->GetVel(tg, &v);
		float ts = VC_Len(tv->x, tv->y, tv->z);
		float rel = VC_Speed(self) - ts;
		if (!(rel > AIVC_REL_SPEED) || dist / rel > g_TimeMaxReachBoostAI) {
			self->ctl->boostKey = 1;
			return;
		}
	}
	self->ctl->boostKey = 0;
}

/* the acceleration along a path: the brakes before the point (TIME_REACT_TURN) and at the goal, the velocity along
 * the nose without inertia */
void cAIVehicleControl_AccelPath(cAIVehicleControl *self) {
	vec3_t b, fwd;
	byte slow = 0, slowA = 0;
	float val = 0.0f, lim = 0.0f;

	if (self->lost) {
		self->ctl->acc = 0.0f;
		return;
	}
	const vec3_t *p = VC_Pos(self, &b);
	float distP = VC_Dist(p, &self->point);
	if (VC_Same(&self->point, &self->dest)) {
		lim = Game_Atan2F((double)self->dist, (double)distP);
	}
	float ang = VC_NoseAngle(self, &fwd);
	if (VC_Same(&self->point, &self->dest)) {
		if (self->stopAtGoal) {
			slow = slowA = self->dist > distP;
			val = 0.0f;
			goto set;
		}
		if (g_DistBrakeObjAI > distP) {
			float speed = VC_Speed(self);
			if (speed > 0.0f) {
				VC_VelAlongNose(self, &fwd, speed);
			}
			if (self->braking) {
				if (ang > lim * 0.9f) {
					self->brake = Float_MaxSS(0.0f, self->brake - g_DecVelBrakeAI);
					val = self->brake;
					slow = 1;
				} else {
					self->braking = 0;
				}
			} else if (ang > lim) {
				self->braking = 1;
				self->brake = self->accel;
			}
		}
	} else if (self->params.brake < 1 || !(byte)Vec3_Differ(&self->point.x, &self->dest.x)) {
		self->braking = 1;
	} else {
		float speed = VC_Speed(self);
		if (speed > g_VelTurnAI) {
			float tt = distP / speed;
			if (g_TimeReactTurnAI > tt) {
				slowA = 1;
				val = tt / g_TimeReactTurnAI;
			}
		}
		self->braking = 1;
	}
set:
	switch (self->params.brake) {
	case 0:
		self->ctl->acc = slow ? val : self->accel;
		break;
	case 1:
		self->ctl->acc = slow || slowA ? val : self->accel;
		break;
	}
	if (!self->inertia) {
		float speed = VC_Speed(self);
		if (speed > 0.0f) {
			VC_VelAlongNose(self, &fwd, speed);
		}
	}
	VC_Boost(self, VC_Same(&self->point, &self->dest) && lim > ang, distP, ang);
}

/* ---- the paths ---- */

/* a path to p over the graph (A*), the point its first node; else the event 1 (not reachable) or 0 (lost) */
byte cAIVehicleControl_SetPath(cAIVehicleControl *self, const vec3_t *p) {
	vec3_t buf;
	byte startOk, goalOk;

	CHECK(g_NodesWorld != NULL && g_AStar != NULL);
	CHECK(g_NodesWorld->sup.vftable->IsCreated(g_NodesWorld));
	self->dest = *p;
	VC_Pos(self, &buf);
	if (!CAStar_FindPath(g_AStar, &self->path, &self->dest, &buf, self->radius, self->dist,
		self->params.stupidPathfinding, &startOk, &goalOk, &self->pathState)) {
		if (self->params.stupidPathfinding == 0) {
			goto fail;
		}
		self->dest = *p;
		if (!CAStar_FindPath(g_AStar, &self->path, &self->dest, &buf, self->radius, self->dist, 0, &startOk,
			&goalOk, &self->pathState)) {
			goto fail;
		}
	}
	vec3_t *n = VC_PathFirst(self);
	self->node = n;
	self->point = self->path.count == 2 ? self->dest : *n;
	self->seen = 0;
	cAIVehicleControl_Smooth(self);
	return 1;
fail:
	if (!goalOk) {
		self->ai->vftable->Event(self->ai, 1);
		return 0;
	}
	if (!startOk) {
		self->ai->vftable->Event(self->ai, 0);
	}
	return 0;
}

/* the path from the vehicle straight to p */
byte cAIVehicleControl_SetDirectPath(cAIVehicleControl *self, const vec3_t *p) {
	vec3_t buf;

	self->dest = *p;
	VC_Pos(self, &buf);
	DListVec3_Clear(&self->path);
	DListVec3_Add(&self->path, &buf);
	DListVec3_Add(&self->path, &self->dest);
	self->node = VC_PathFirst(self);
	self->point = self->dest;
	return 1;
}

/* the path along dir over the graph (DepthPathRouteOutDoorAI nodes), its last node the goal; else the event 0 */
byte cAIVehicleControl_RebuildPath(cAIVehicleControl *self) {
	vec3_t buf;

	CHECK(g_NodesWorld != NULL);
	CHECK(g_NodesWorld->sup.vftable->IsCreated(g_NodesWorld));
	VC_Pos(self, &buf);
	if (!CNodesWorld_BuildPath(g_NodesWorld, &self->path, g_DepthPathRouteOutDoorAI, &self->dir, &buf,
		self->pathDirect, &self->pathNode, self->radius, self->pathFlat)) {
		self->ai->vftable->Event(self->ai, 0);
		return 0;
	}
	cNWVecNode *h = self->path.head;
	self->path.iter = h;
	self->dest = h->v;
	vec3_t *n = VC_PathFirst(self);
	self->node = n;
	self->point = *n;
	self->seen = 0;
	cAIVehicleControl_Smooth(self);
	return 1;
}

/* the debug view: the corridor, the nose, the point, the velocity and the path (ShowPathAI), the controls of the
 * vehicle named by ShowVehAI */
void cAIVehicleControl_Draw(cAIVehicleControl *self) {
	vec3_t pos, rot, fwd, n, tip, v;

	if (g_ShowPathAI > 0) {
		for (int i = 0; i < 9; i++) {
			DebugUtils_DrawLine(&self->corridor[i][0], &self->corridor[i][1], 0xff0000);
		}
		DebugUtils_DrawLine(&self->endCap[0][0], &self->endCap[0][1], 0xff0000);
		DebugUtils_DrawLine(&self->endCap[1][0], &self->endCap[1][1], 0xff0000);
		VC_Pos(self, &pos);
		Obj_Vtbl(self->vehicle)->GetAng(self->vehicle, &rot);
		Vec3_Rotate(&fwd, &aivc_fwd, &rot);
		NW_Normalize(&n, fwd.x, fwd.y, fwd.z);
		tip.x = pos.x + n.x * AIVC_DRAW_LEN;
		tip.y = pos.y + n.y * AIVC_DRAW_LEN;
		tip.z = pos.z + n.z * AIVC_DRAW_LEN;
		DebugUtils_DrawLine(&pos, &tip, 0xff00ff);
		if (self->point.x != -FLT_MAX || self->point.y != -FLT_MAX || self->point.z != -FLT_MAX) {
			DebugUtils_DrawLine(&pos, &self->point, 0xffff);
		}
		Obj_Vtbl(self->vehicle)->GetVel(self->vehicle, &v);
		if (v.y * v.y + v.x * v.x + v.z * v.z > AIVC_DRAW_VEL2) {
			NW_Normalize(&n, v.x, v.y, v.z);
			tip.x = n.x * AIVC_DRAW_LEN + pos.x;
			tip.y = n.y * AIVC_DRAW_LEN + pos.y;
			tip.z = n.z * AIVC_DRAW_LEN + pos.z;
			DebugUtils_DrawLine(&pos, &tip, 0xffff00);
		}
		if (g_ShowPathAI == 2) {
			g_FontTerminal8->color = 0xffffff;
			g_FontTerminal8->align = 4;
		}
		cNWVecNode *h = self->path.head;
		if (h) {
			cNWVecNode *saved = self->path.iter;
			vec3_t prev = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
			self->path.iter = h;
			vec3_t *p = &h->v;
			prev = *p;
			while (p) {
				DebugUtils_DrawSphere(p, 50.0f, 0xffffff, 2, 2);
				if (g_ShowPathAI == 2) {
					vec3_t q;
					q.x = p->x + 0.0f;
					q.y = p->y - 25.0f;
					q.z = p->z + 0.0f;
					CFont_Printf3D(g_FontTerminal8, &q, 3, "%.3f", (double)p->x);
					q.x = p->x + 0.0f;
					q.y = p->y - 50.0f;
					q.z = p->z + 0.0f;
					CFont_Printf3D(g_FontTerminal8, &q, 3, "%.3f", (double)p->y);
					q.x = p->x + 0.0f;
					q.y = p->y - 75.0f;
					q.z = p->z + 0.0f;
					CFont_Printf3D(g_FontTerminal8, &q, 3, "%.3f", (double)p->z);
				}
				if (!VC_Same(p, &prev)) {
					DebugUtils_DrawLine(&prev, p, 0xffffff);
				}
				prev = *p;
				cNWVecNode *nx = self->path.iter->next;
				self->path.iter = nx;
				if (nx == self->path.head) {
					break;
				}
				p = &nx->v;
			}
			self->path.iter = saved;
		}
	}
	if (cString_EqStr(g_ShowVehAI, self->vehicle->name)) {
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 10.0f, 0xffffff, "ENTIDAD: %s", self->vehicle->name);
		float speed = VC_Speed(self);
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 30.0f, 0xffff00, "Velocity (Km/h): %.2f", (double)(speed * 0.036f));
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 50.0f, 0xffff00, "Control Turn Horizontal: %.2f",
			(double)self->ctl->hRot);
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 70.0f, 0xffff00, "Control Turn Vertical: %.2f",
			(double)self->ctl->vRot);
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 90.0f, 0xffff00, "Control Strafe Horizontal: %.2f",
			(double)self->ctl->str);
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 110.0f, 0xffff00, "Control Strafe Vertical: %.2f",
			(double)self->ctl->vStr);
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 130.0f, 0xffff00, "Control Acceleration: %.2f",
			(double)self->ctl->acc);
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 150.0f, 0xffff00, "Control Boost: %s",
			self->ctl->boostKey ? "ACTIVO" : "INACTIVO");
	}
}

void cAIVehicleControl_Stop(cAIVehicleControl *self) {
	VC_Release(self);
}

/* the point as far along the path as the corridor is free; to a blocked node by halving between the last free
 * point and it (DepthSearchPosGoAI times, while the angle to them is above MinAngleSearchPointAI) */
void cAIVehicleControl_Smooth(cAIVehicleControl *self) {
	cNWVecNode *saved = self->path.iter;
	cNWVecNode *n = saved->prev;
	vec3_t *p;
	byte moved = 0;
	vec3_t b1, b2, a, d;

	self->path.iter = n;
	p = n != self->path.head->prev ? &n->v : NULL;
	while (p) {
		if (!VC_Free(self, p)) {
			goto blocked;
		}
		self->node = p;
		self->point = *p;
		saved = self->path.iter;
		n = saved->prev;
		self->path.iter = n;
		if (n == self->path.head->prev) {
			break;
		}
		p = &n->v;
		moved = 1;
	}
	self->path.iter = saved;
	self->point = self->dest;
	return;
blocked:
	if (moved) {
		cAIVehicleControl_RandomNear(self, p);
	}
	vec3_t hi = self->point;
	const vec3_t *q = VC_Pos(self, &b1);
	a.x = self->point.x - q->x;
	a.y = self->point.y - q->y;
	a.z = self->point.z - q->z;
	q = VC_Pos(self, &b2);
	d.x = p->x - q->x;
	d.y = p->y - q->y;
	d.z = p->z - q->z;
	if (!(Vec3_Angle(&d, &a) > (double)g_MinAngleSearchPointAI)) {
		goto end;
	}
	if (AIVC_SAME > __builtin_fabs((double)(p->x - self->point.x)) &&
		AIVC_SAME > __builtin_fabs((double)(p->y - self->point.y)) &&
		AIVC_SAME > __builtin_fabs((double)(p->z - self->point.z))) {
		goto end;
	}
	vec3_t lo = *p;
	int k = 0;
	while (k < g_DepthSearchPosGoAI) {
		self->point.x = (hi.x + lo.x) * 0.5f;
		self->point.y = (hi.y + lo.y) * 0.5f;
		self->point.z = (hi.z + lo.z) * 0.5f;
		q = VC_Pos(self, &b2);
		d.x = self->point.x - q->x;
		d.y = self->point.y - q->y;
		d.z = self->point.z - q->z;
		q = VC_Pos(self, &b1);
		a.x = hi.x - q->x;
		a.y = hi.y - q->y;
		a.z = hi.z - q->z;
		if (!(Vec3_Angle(&a, &d) > (double)g_MinAngleSearchPointAI)) {
			goto restore;
		}
		if (AIVC_SAME > __builtin_fabs((double)(hi.x - self->point.x)) &&
			AIVC_SAME > __builtin_fabs((double)(hi.y - self->point.y)) &&
			AIVC_SAME > __builtin_fabs((double)(hi.z - self->point.z))) {
			goto restore;
		}
		if (!VC_Free(self, &self->point)) {
			lo = self->point;
		} else {
			hi = self->point;
			if (!self->seen) {
				k++;
			}
		}
	}
	goto end;
restore:
	self->point = hi;
end:
	self->path.iter = saved;
}

vec3_t *cAIVehicleControl_GetGoal(cAIVehicleControl *self, vec3_t *out) {
	*out = self->point;
	return out;
}

vec3_t *cAIVehicleControl_GetDest(cAIVehicleControl *self, vec3_t *out) {
	*out = self->targetPos;
	return out;
}

cAIVehicleParams2 *cAIVehicleControl_GetParams(cAIVehicleControl *self) {
	return &self->params;
}

cAIVehicleParams2 *cAIVehicleControl_GetParamsInit(cAIVehicleControl *self) {
	return &self->paramsInit;
}

/* the node is the first one of the path (the one in front of head) */
byte cAIVehicleControl_NodeIsFirst(cAIVehicleControl *self) {
	cNWVecNode *saved = self->path.iter;
	vec3_t *p = VC_PathFirst(self);

	self->path.iter = saved;
	return p == self->node;
}

/* the node is the one behind head */
byte cAIVehicleControl_NodeIsLast(cAIVehicleControl *self) {
	cNWVecNode *saved = self->path.iter;
	cNWVecNode *h = self->path.head;
	cNWVecNode *n;

	self->path.iter = h;
	n = h->next;
	self->path.iter = n;
	vec3_t *p = n == self->path.head ? NULL : &n->v;
	self->path.iter = saved;
	return p == self->node;
}

byte cAIVehicleControl_GetStrafing(cAIVehicleControl *self) {
	return self->strafing;
}

/* the goal seen from the vehicle and from the node at less than pi / 8 apart */
byte cAIVehicleControl_NodeOnWay(cAIVehicleControl *self) {
	vec3_t buf, a, b;
	const vec3_t *p = VC_Pos(self, &buf);

	a.x = self->dest.x - p->x;
	a.y = self->dest.y - p->y;
	a.z = self->dest.z - p->z;
	b.x = self->dest.x - self->node->x;
	b.y = self->dest.y - self->node->y;
	b.z = self->dest.z - self->node->z;
	return (double)(M_PI_F / 8.0f) > Vec3_Angle(&a, &b);
}

byte cAIVehicleControl_GetFlag(cAIVehicleControl *self) {
	return self->inSight;
}

byte cAIVehicleControl_InSight(cAIVehicleControl *self) {
	return self->inSight;
}

/* ---- the moves ---- */

byte cAIVehicleControl_UpdateChase(cAIVehicleControl *self, float dt) {
	(void)dt;
	if (self->path.count > 0) {
		if (!cAIVehicleControl_ChasePath(self)) {
			return 0;
		}
		if (!self->inSight) {
			if (!cAIVehicleControl_PathStepChase(self)) {
				return 0;
			}
		} else {
			self->targetPos.x = -FLT_MAX;
			self->targetPos.y = -FLT_MAX;
			self->targetPos.z = -FLT_MAX;
		}
		if (self->inSight) {
			cAIVehicleControl_TurnToTarget(self);
			cAIVehicleControl_StrafeChase(self);
			cAIVehicleControl_AccelChase(self);
			return 1;
		}
		cAIVehicleControl_TurnToPoint(self);
		cAIVehicleControl_StrafeToPoint(self);
		cAIVehicleControl_AccelToTarget(self);
	}
	return 1;
}

byte cAIVehicleControl_UpdateOrient(cAIVehicleControl *self, float dt) {
	(void)dt;
	if (!cAIVehicleControl_ChaseLine(self)) {
		return 0;
	}
	cAIVehicleControl_TurnToTarget(self);
	self->ctl->str = 0.0f;
	self->ctl->vStr = 0.0f;
	self->ctl->acc = 0.0f;
	self->ctl->boostKey = 0;
	return 1;
}

byte cAIVehicleControl_UpdatePoint(cAIVehicleControl *self, float dt) {
	(void)dt;
	if (self->path.count > 0) {
		cAIVehicleControl_CheckArrive(self);
		if (!self->ai->vftable->GetChanged(self->ai)) {
			if (!cAIVehicleControl_PathStep(self)) {
				return 0;
			}
			cAIVehicleControl_TurnToPoint(self);
			self->strafing = cAIVehicleControl_StrafeDodge(self);
			if (!self->strafing) {
				cAIVehicleControl_StrafeToPoint(self);
			}
			cAIVehicleControl_AccelPath(self);
		}
	}
	return 1;
}

byte cAIVehicleControl_UpdateDirect(cAIVehicleControl *self, float dt) {
	(void)dt;
	if (self->path.count > 0) {
		cAIVehicleControl_CheckArrive(self);
		if (!self->ai->vftable->GetChanged(self->ai)) {
			cAIVehicleControl_TurnToPoint(self);
			cAIVehicleControl_StrafeToPoint(self);
			cAIVehicleControl_AccelPath(self);
		}
	}
	return 1;
}

byte cAIVehicleControl_UpdatePath(cAIVehicleControl *self, float dt) {
	(void)dt;
	if (self->path.count > 0) {
		cAIVehicleControl_CheckArrive(self);
		if (!self->ai->vftable->GetChanged(self->ai)) {
			if (!cAIVehicleControl_PathStepDir(self)) {
				return 0;
			}
			cAIVehicleControl_TurnToPoint(self);
			self->strafing = cAIVehicleControl_StrafeDodge(self);
			if (!self->strafing) {
				cAIVehicleControl_StrafeToPoint(self);
			}
			cAIVehicleControl_AccelPath(self);
		}
	}
	return 1;
}

void cAIVehicleControl_ClearDest(cAIVehicleControl *self) {
	self->targetPos.x = -FLT_MAX;
	self->targetPos.y = -FLT_MAX;
	self->targetPos.z = -FLT_MAX;
}

void cAIVehicleControl_ResetLife(cAIVehicleControl *self) {
	VC_LifeMark(self);
}

/* the chase of the target of node between chaseMin and chaseMax: a path to it unless the same target is chased */
byte cAIVehicleControl_SetChase(cAIVehicleControl *self, Nto1Node *node, float chaseMin, float chaseMax,
	byte boost, float rotA, float rotB) {
	vec3_t buf;

	CHECK(node != NULL);
	CHECK(node->target != NULL);
	int mark = Veh(self->vehicle)->wl.life + 1;
	self->chaseMin = chaseMin;
	self->lifeMark = mark;
	self->chaseMax = chaseMax;
	self->boost = boost;
	self->rotA = rotA;
	self->strafeWait = 0.0f;
	self->rotB = rotB;
	if (self->mode == self->prevMode && self->target->target == node->target) {
		return 1;
	}
	self->target = node;
	cEntity *t = node->target;
	float r = Obj_Vtbl(t)->GetRadius(t);
	self->dist = r;
	self->distMin = Float_MinSS(self->radius, r);
	if (!cAIVehicleControl_SetPath(self, VC_TargetPos(self, &buf))) {
		return 0;
	}
	self->inSight = VC_Same(&self->point, &self->dest);
	self->tooNear = 0;
	self->tooFar = 0;
	self->strafeTime = 0.0f;
	return 1;
}

void cAIVehicleControl_SetInertia(cAIVehicleControl *self, int on) {
	self->inertia = (byte)on;
}

byte cAIVehicleControl_SetMove(cAIVehicleControl *self, int mode, ...) {
	va_list ap;
	byte r = 1;

	va_start(ap, mode);
	self->prevMode = self->mode;
	self->braking = 0;
	self->mode = mode;
	self->lost = 0;
	self->strafeOn = 0;
	self->stopAtGoal = 0;
	cSlotController_Restart(&self->slotArrive);
	switch (self->mode) {
	case AIVC_STOP:
		VC_LifeMark(self);
		cAIVehicleControl_ClearAll(self);
		break;
	case AIVC_POINT: {
		vec3_t dest = va_arg(ap, vec3_t);
		double dist = va_arg(ap, double);
		int boost = va_arg(ap, int);
		double accel = va_arg(ap, double);
		double rotA = va_arg(ap, double);
		double rotB = va_arg(ap, double);
		int stop = va_arg(ap, int);
		int strafe = va_arg(ap, int);
		self->inSight = 0;
		r = cAIVehicleControl_MovePoint(self, &dest, (float)dist, (byte)boost, (float)accel, (float)rotA,
			(float)rotB, (byte)stop, (byte)strafe);
		break;
	}
	case AIVC_DIRECT: {
		vec3_t dest = va_arg(ap, vec3_t);
		double dist = va_arg(ap, double);
		int boost = va_arg(ap, int);
		double accel = va_arg(ap, double);
		double rotA = va_arg(ap, double);
		double rotB = va_arg(ap, double);
		self->inSight = 0;
		r = cAIVehicleControl_MoveDirect(self, &dest, (float)dist, (byte)boost, (float)accel, (float)rotA,
			(float)rotB);
		break;
	}
	case AIVC_CHASE: {
		Nto1Node *node = self->ai->vftable->GetTargetNode(self->ai);
		double mn = va_arg(ap, double);
		double mx = va_arg(ap, double);
		int boost = va_arg(ap, int);
		double rotA = va_arg(ap, double);
		double rotB = va_arg(ap, double);
		r = cAIVehicleControl_SetChase(self, node, (float)mn, (float)mx, (byte)boost, (float)rotA, (float)rotB);
		break;
	}
	case AIVC_ORIENT:
		r = cAIVehicleControl_SetOrient(self, self->ai->vftable->GetTargetNode(self->ai));
		break;
	case AIVC_PATH: {
		vec3_t dir = va_arg(ap, vec3_t);
		double dist = va_arg(ap, double);
		int boost = va_arg(ap, int);
		double accel = va_arg(ap, double);
		int direct = va_arg(ap, int);
		int flat = va_arg(ap, int);
		double rotA = va_arg(ap, double);
		double rotB = va_arg(ap, double);
		int strafe = va_arg(ap, int);
		self->inSight = 0;
		r = cAIVehicleControl_MovePath(self, &dir, (float)dist, (byte)boost, (float)accel, (byte)direct, (byte)flat,
			(float)rotA, (float)rotB, (byte)strafe);
		break;
	}
	}
	va_end(ap);
	return r;
}

/* a random goal around dest within dist where the vehicle fits (3 tries) */
void cAIVehicleControl_RandomDest(cAIVehicleControl *self, vec3_t *out) {
	for (int k = 0; k < 3;) {
		vec3_t n, c;
		k++;
		double r1 = (double)(crt_rand() & 0x7fff) * AIVC_RAND_INV;
		double r2 = (double)(crt_rand() & 0x7fff) * AIVC_RAND_INV;
		float x = (float)(r1 + r1 - 1.0);
		float y = (float)(r2 + r2 - 1.0);
		double r3 = (double)(crt_rand() & 0x7fff) * AIVC_RAND_INV;
		float z = (float)(r3 + r3 - 1.0);
		NW_Normalize(&n, x, y, z);
		float d = self->dist;
		float s = (float)((double)(crt_rand() & 0x7fff) * AIVC_RAND_INV * (double)d + 0.0);
		c.x = self->dest.x + n.x * s;
		c.y = self->dest.y + n.y * s;
		c.z = self->dest.z + n.z * s;
		if (!Engine_TestSphere(&c, &c, self->radius, NULL, NULL)) {
			*out = c;
			self->dest = c;
			return;
		}
	}
}

/* a random point within 3000 of the blocked node p that the vehicle fits in and the graph sees from the node and
 * from the next one (3 tries) */
void cAIVehicleControl_RandomNear(cAIVehicleControl *self, vec3_t *p) {
	if (VC_Same(p, &self->dest)) {
		return;
	}
	cNWVecNode *saved = self->path.iter;
	cNWVecNode *n = saved->prev;
	self->path.iter = n;
	const vec3_t *next = n == self->path.head->prev ? NULL : &n->v;
	for (int k = 0; k < 3;) {
		vec3_t c;
		k++;
		float dx = (float)((double)(crt_rand() & 0x7fff) * AIVC_RAND_INV * 6000.0 - 3000.0);
		float dy = (float)((double)(crt_rand() & 0x7fff) * AIVC_RAND_INV * 6000.0 - 3000.0);
		float dz = (float)((double)(crt_rand() & 0x7fff) * AIVC_RAND_INV * 6000.0 - 3000.0);
		c.x = p->x + dx;
		c.y = p->y + dy;
		c.z = p->z + dz;
		if (Engine_TestSphere(&c, &c, self->radius, NULL, NULL) ||
			!CNodesWorld_TestPath(g_NodesWorld, &c, self->node, self->radius, self->radius, NULL, 0) ||
			!CNodesWorld_TestPath(g_NodesWorld, &c, next, self->radius, self->radius, NULL, 0)) {
			continue;
		}
		*p = c;
		break;
	}
	self->path.iter = saved;
}

byte cAIVehicleControl_SetOrient(cAIVehicleControl *self, Nto1Node *node) {
	CHECK(node != NULL);
	CHECK(node->target != NULL);
	int mark = Veh(self->vehicle)->wl.life + 1;
	self->target = node;
	self->lifeMark = mark;
	self->strafeWait = 0.0f;
	return 1;
}

byte cAIVehicleControl_MovePath(cAIVehicleControl *self, const vec3_t *dir, float dist, byte boost,
	float accel, byte direct, byte flat, float rotA, float rotB, byte strafe) {
	self->boost = boost;
	self->pathDirect = direct;
	self->pathFlat = flat;
	self->distMin = Float_MinSS(self->radius, dist);
	self->accel = accel;
	self->dist = dist;
	self->dir = *dir;
	self->rotA = rotA;
	self->rotB = rotB;
	self->strafeOn = strafe;
	return cAIVehicleControl_RebuildPath(self);
}

void cAIVehicleControl_SetDirect(cAIVehicleControl *self, byte on) {
	self->direct = on;
}

void cAIVehicleControl_SetRotStatic(cAIVehicleControl *self, float a, float b) {
	self->rotA = a;
	self->rotB = b;
}

byte cAIVehicleControl_MovePoint(cAIVehicleControl *self, const vec3_t *dest, float dist, byte boost,
	float accel, float rotA, float rotB, byte stopAtGoal, byte strafe) {
	self->distMin = Float_MinSS(self->radius, dist);
	VC_LifeMark(self);
	self->boost = boost;
	self->accel = accel;
	self->stopAtGoal = stopAtGoal;
	self->rotA = rotA;
	self->strafeOn = strafe;
	self->dist = dist;
	self->rotB = rotB;
	if (self->mode == self->prevMode && VC_Same(&self->dest, dest)) {
		return 1;
	}
	return cAIVehicleControl_SetPath(self, dest);
}

byte cAIVehicleControl_MoveDirect(cAIVehicleControl *self, const vec3_t *dest, float dist, byte boost,
	float accel, float rotA, float rotB) {
	self->distMin = Float_MinSS(self->radius, dist);
	self->lifeMark = Veh(self->vehicle)->wl.life + 1;
	self->accel = accel;
	self->boost = boost;
	self->rotA = rotA;
	self->strafeWait = 0.0f;
	self->dist = dist;
	self->rotB = rotB;
	if (self->mode == self->prevMode && VC_Same(&self->dest, dest)) {
		return 1;
	}
	return cAIVehicleControl_SetDirectPath(self, dest);
}

void cAIVehicleControl_ResetClear(cAIVehicleControl *self) {
	VC_LifeMark(self);
	cAIVehicleControl_ClearAll(self);
}

/* the controls let go, the goal and the point -FLT_MAX */
void cAIVehicleControl_ClearAll(cAIVehicleControl *self) {
	VC_Release(self);
	self->dest.x = -FLT_MAX;
	self->dest.y = -FLT_MAX;
	self->dest.z = -FLT_MAX;
	self->point.x = -FLT_MAX;
	self->point.y = -FLT_MAX;
	self->point.z = -FLT_MAX;
}

/* ---- cAIVehicleGroup ---- */

cAIVehicleGroup *cAIVehicleGroup_ctor(void *mem) {
	cAIVehicleGroup *self = mem;

	cAIEntityGroup_ctor(self);
	self->vftable = CAIVEHICLEGROUP_VFTABLE;
	float now = Timer_GetTime(&g_pWorld->timer);
	self->numStopped = 0;
	self->reposTime = now;
	self->numStoppedHeavy = 0;
	self->numStoppedMedium = 0;
	self->numStoppedLight = 0;
	self->numStoppedRoute = 0;
	self->numElevators = 0;
	cAIVehicleGroup_FindElevators(self);
	return self;
}

void cAIVehicleGroup_dtor(cAIVehicleGroup *self) {
	self->vftable = CAIVEHICLEGROUP_VFTABLE;
	cAIEntityGroup_dtor(self);
}

cAIVehicleGroup *cAIVehicleGroup_Delete(cAIVehicleGroup *self, byte flags) {
	cAIVehicleGroup_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAIVEHICLEGROUP_SIZE);
	}
	return self;
}

/* the time since ResetRepos reached MinTimeReposAI */
byte cAIVehicleGroup_ReposReady(cAIVehicleGroup *self) {
	float d = Timer_GetTime(&g_pWorld->timer) - self->reposTime;

	return d >= g_MinTimeReposAI;
}

void cAIVehicleGroup_Draw(cAIVehicleGroup *self) {
	if (g_ShowStopShipAI) {
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 10.0f, 0xffffff, "Num. Ships Stopped: %d", self->numStopped);
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 30.0f, 0xffffff, "Num. Ships Stopped Traffic Heavy: %d",
			self->numStoppedHeavy);
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 50.0f, 0xffffff, "Num. Ships Stopped Traffic Medium: %d",
			self->numStoppedMedium);
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 70.0f, 0xffffff, "Num. Ships Stopped Traffic Light: %d",
			self->numStoppedLight);
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 90.0f, 0xffffff, "Num. Ships Stopped In Route: %d",
			self->numStoppedRoute);
	}
}

byte cAIVehicleGroup_ElevatorNear(cAIVehicleGroup *self, const vec3_t *pos, float dist) {
	float r2 = dist * dist;

	for (int i = 0; i < self->numElevators; i++) {
		const vec3_t *e = &self->elevators[i];
		float dx = e->x - pos->x, dy = e->y - pos->y, dz = e->z - pos->z;
		if (r2 >= dy * dy + dx * dx + dz * dz) {
			return 1;
		}
	}
	return 0;
}

static void VC_FindDummies(cAIVehicleGroup *self, const char *pattern, size_t size) {
	char name[100];
	vec3_t pos;
	float ang[2];
	byte flag;

	memcpy(name, pattern, size);
	DummySearch_Start(name, 1);
	while (DummySearch_Next(name, sizeof(name), &pos, ang, &flag)) {
		self->elevators[self->numElevators] = pos;
		self->numElevators++;
	}
}

/* the positions of the elevator and metro doors of the level (DM_Door_Elevator_*, DM_Door_MetroExt_*) */
void cAIVehicleGroup_FindElevators(cAIVehicleGroup *self) {
	self->numElevators = 0;
	VC_FindDummies(self, "DM_Door_Elevator_*", sizeof("DM_Door_Elevator_*"));
	VC_FindDummies(self, "DM_Door_MetroExt_*", sizeof("DM_Door_MetroExt_*"));
}

void cAIVehicleGroup_ResetRepos(cAIVehicleGroup *self) {
	self->reposTime = Timer_GetTime(&g_pWorld->timer);
}
