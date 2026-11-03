/*
 * The move control of a character that walks (AI/AICharControl2D.cpp, include/scrap/aicharcontrol.h): the paths on
 * the graph CNodesWorld2D, the floor tests, the turn of the view to the goal, the walk to or away from the target,
 * the strafe around a character in the way.
 */
#include <float.h>
#include <string.h>

#include "scrap/aicharcontrol.h"
#include "scrap/fxelements.h"
#include "scrap/fxmisc.h"
#include "scrap/nodesworld2d.h"
#include "scrap/scrapcollision.h"
#include "scrap/staticinit.h"

#define CAICHARCONTROL2D_VFTABLE (&cAICharControl2D_vftable.base)
#define g_FactorDistTestColBackCharAI (GVar_FactorDistTestColBackCharAI.value.f)	/* "FactorDistTestColBackCharAI" */
static const cAICharControl2DVtbl cAICharControl2D_vftable;
static cGVar GVar_FactorDistTestColBackCharAI;

/* the distance on the floor (x and z), the sum of the squares from z */
static inline float AICC2D_FlatDist(float dx, float dz) {
	return (float)__builtin_sqrt((double)(dz * dz + dx * dx));
}

/* the distance on the floor from the character to (tx, tz): its position read twice (x, then z) */
static float AICC2D_DistTo(cAICharControl *self, float tx, float tz) {
	vec3_t b1, b2;

	float px = Entity_GetPos(self->ent, &b1)->x;
	const vec3_t *q = Entity_GetPos(self->ent, &b2);
	return AICC2D_FlatDist(px - tx, q->z - tz);
}

static inline float AICC_FloorRadius(cAICharControl *self) {
	cActCtrl *ac = WithLife_GetActCtrl(self->ent);
	return ac->cls->sRadius;
}

/* (x, 0, z) made a unit vector, the length from the squares of x and z */
static inline vec3_t AICC2D_FlatNorm(float x, float z) {
	vec3_t r;
	float len = (float)__builtin_sqrt((double)(x * x + 0.0f + z * z));

	if (len == 0.0f) {
		r.x = x;
		r.y = 0.0f;
		r.z = z;
	} else {
		float inv = 1.0f / len;
		r.x = x * inv;
		r.y = inv * 0.0f;
		r.z = z * inv;
	}
	return r;
}

/* ---- construction ---- */

cAICharControl *cAICharControl2D_ctor(void *mem, cEntity *ent, cAIChar *ai, int *arg) {
	cAICharControl *self = cAICharControl_ctor(mem, ent, ai, arg);

	self->vftable = CAICHARCONTROL2D_VFTABLE;
	return self;
}

void cAICharControl2D_dtor(cAICharControl2D *self) {
	self->vftable = CAICHARCONTROL2D_VFTABLE;
	cAICharControl_dtor(self);
}

static cAICharControl2D *cAICharControl2D_Delete(cAICharControl2D *self, byte flags) {
	self->vftable = CAICHARCONTROL2D_VFTABLE;
	cAICharControl_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAICHARCONTROL_SIZE);
	}
	return self;
}

void cAICharControl2D_Draw(cAICharControl2D *self) {
	cAICharControl_Draw(self);
}

/* ---- the tests of the way ---- */

byte cAICharControl2D_Facing(cAICharControl2D *self, cEntity *hitEnt, vec3_t *toE, vec3_t *toGoal) {
	vec3_t b1, b2;

	CHECK(hitEnt != NULL);
	const vec3_t *s = AICC_Pos(self, &b1);
	const vec3_t *e = AICC_EntPos(hitEnt, &b2);
	float x = e->x - s->x;
	float z = e->z - s->z;
	toE->y = 0.0f;
	toE->x = x;
	toE->z = z;
	*toE = AICC2D_FlatNorm(x, z);
	s = AICC_Pos(self, &b1);
	x = self->goal.x - s->x;
	z = self->goal.z - s->z;
	toGoal->y = 0.0f;
	toGoal->x = x;
	toGoal->z = z;
	*toGoal = AICC2D_FlatNorm(x, z);
	if (self->walkDir == 2) {
		toGoal->x = -toGoal->x;
		toGoal->y = -toGoal->y;
		toGoal->z = -toGoal->z;
	}
	return Vec3_Angle(toE, toGoal) < 1.5707963267948966;
}

byte cAICharControl2D_TestLine(cAICharControl2D *self, const vec3_t *a, const vec3_t *b) {
	if (Engine_TestLine(a, b, NULL, NULL)) {
		return 0;
	}
	return AICC2D_FloorAhead(self, b) != 0;
}

byte cAICharControl2D_SideBlocked(cAICharControl2D *self, int side) {
	vec3_t d, p, buf;

	if (side == 3 || side == 4) {
		return 0;
	}
	AICC_SideDir(self, &d, side);
	if (d.x == 0.0f && d.y == 0.0f && d.z == 0.0f) {
		return 0;
	}
	float r = Obj_GetRadius(self->ent);
	r = r + r;
	float ox = d.x * r;
	float oy = d.y * r;
	float oz = d.z * r;
	const vec3_t *s = AICC_Pos(self, &buf);
	p.y = s->y + oy;
	p.z = s->z + oz;
	p.x = s->x + ox;
	float rr = Obj_GetRadius(self->ent);
	return AICC_TestPath(self, AICC_Pos(self, &buf), &p, rr) == 0;
}

byte cAICharControl2D_UpdateFollow(cAICharControl2D *self) {
	cEntity *te = AICC_TargetEnt(self);
	vec3_t T, buf;
	float f;

	AICC_EntPos(te, &T);
	if (self->direct) {
		if (!cSlotController_Tick(&self->slotDirect, self->dt) || !cSlotController_FireReset(&self->slotDirect, &f)) {
			return 1;
		}
		do {
			float d = AICC2D_DistTo(self, T.x, T.z);
			self->stuck = 0;
			if (d > self->farDist || AICC_Vf89(self)) {
				float r = self->radius;
				if (!AICC_TestPathEnd(self, AICC_Pos(self, &buf), &T, r)) {
					goto lost;
				}
			} else {
				if (Engine_TestLine(AICC_Pos(self, &buf), &T, NULL, NULL)) {
					goto lost;
				}
				float r = self->radius;
				if (!AICC_TestPathEnd(self, AICC_Pos(self, &buf), &T, r)) {
					self->stuck = 1;
				}
			}
			AICC_DirectPath(self, &T);
			continue;
		lost:
			self->direct = 0;
			if (!AICC_FindPath(self, &T)) {
				return 0;
			}
			cSlotController_Restart(&self->slotNear);
			cSlotController_Restart(&self->slotSee);
			cSlotController_Restart(&self->slotPath);
		} while (cSlotController_FireReset(&self->slotDirect, &f));
		return 1;
	}
	if (cSlotController_Tick(&self->slotNear, self->dt) && cSlotController_FireReset(&self->slotNear, &f)) {
		do {
			float dx0 = self->dest.x;
			float dz0 = self->dest.z;
			float px = AICC_Pos(self, &buf)->x;
			const vec3_t *q = AICC_Pos(self, &buf);
			if (200.0f > __builtin_fabs((double)(px - dx0)) && 200.0f > __builtin_fabs((double)(q->z - dz0))) {
				if (!AICC_FindPath(self, &T)) {
					return 0;
				}
				cSlotController_Restart(&self->slotPath);
			}
		} while (cSlotController_FireReset(&self->slotNear, &f));
	}
	if (cSlotController_Tick(&self->slotSee, self->dt) && cSlotController_FireReset(&self->slotSee, &f)) {
		do {
			float d = AICC2D_DistTo(self, T.x, T.z);
			if (d > self->farDist || AICC_Vf89(self)) {
				float r = self->radius;
				if (!AICC_TestPathEnd(self, AICC_Pos(self, &buf), &T, r)) {
					continue;
				}
			} else if (Engine_TestLine(AICC_Pos(self, &buf), &T, NULL, NULL)) {
				continue;
			}
			self->direct = 1;
			AICC_DirectPath(self, &T);
			cSlotController_Restart(&self->slotDirect);
		} while (cSlotController_FireReset(&self->slotSee, &f));
	}
	if (cSlotController_Tick(&self->slotRepath, self->dt) && cSlotController_FireReset(&self->slotRepath, &f)) {
		do {
			if (!AICC_FindPath(self, &T)) {
				return 0;
			}
			cSlotController_Restart(&self->slotPath);
			cSlotController_Restart(&self->slotNear);
		} while (cSlotController_FireReset(&self->slotRepath, &f));
	}
	return 1;
}

void cAICharControl2D_CheckArrived(cAICharControl2D *self) {
	cAIChar *ai;
	int ev;

	if (self->goal.x != self->dest.x || self->goal.y != self->dest.y || self->goal.z != self->dest.z) {
		return;
	}
	vec3_t b1, b2;
	float px = AICC_Pos(self, &b1)->x;
	const vec3_t *q = AICC_Pos(self, &b2);
	float d = AICC2D_FlatDist(self->dest.x - px, self->dest.z - q->z);
	if (self->arriveDist > d) {
		ev = 2;
	} else if (self->arriveDist * 4.0f > d) {
		ev = 4;
	} else {
		goto none;
	}
	ai = self->ai;
	if (AIMove_Event(ai, ev)) {
		return;
	}
none:
	ai = self->ai;
	AIMove_Event(ai, AIEV_OBJECTIVE_VIS);
}

void cAICharControl2D_Turn(cAICharControl2D *self) {
	vec3_t view, ang, d, rot, buf;

	view = WChar(self->ent)->view;
	Vec3_NormAng(&ang, &view);
	if (self->avoidDir.x == 0.0f && self->avoidDir.y == 0.0f && self->avoidDir.z == 0.0f) {
		const vec3_t *s = AICC_Pos(self, &buf);
		d.x = s->x - self->goal.x;
		d.z = s->z - self->goal.z;
	} else {
		d.x = self->avoidDir.x;
		d.z = self->avoidDir.z;
	}
	d.y = 0.0f;
	Vec3_GetRotAng(&rot, &d);
	float a = NormAng(ang.x - rot.x);
	self->diffX = a;
	float t = a * self->turnGain / M_PI_F;
	self->turnXNew = t;
	if (__builtin_fabs((double)(t - self->turnX)) > (double)self->turnStep) {
		self->turnXNew = t > self->turnX ? self->turnStep + self->turnX : self->turnX - self->turnStep;
	}
	self->turnX = self->turnXNew;
	self->ctrl->vRot = -self->turnXNew;
	a = NormAng(ang.y - rot.y);
	self->diffY = a;
	t = a * self->turnGain / M_PI_F;
	self->turnYNew = t;
	if (__builtin_fabs((double)(t - self->turnY)) > (double)self->turnStep) {
		self->turnYNew = t > self->turnY ? self->turnY + self->turnStep : self->turnY - self->turnStep;
	}
	self->turnY = self->turnYNew;
	self->ctrl->hRot = -self->turnYNew;
}

/* walkDir from the distance to the target: away when nearer than nearDist and the way back is free, to it when
 * further than farDist, stopped in between */
void cAICharControl2D_Walk(cAICharControl2D *self) {
	cEntity *te = AICC_TargetEnt(self);
	vec3_t b1, b2;
	float f;

	float px = AICC_Pos(self, &b1)->x;
	float pz = AICC_Pos(self, &b2)->z;
	float tx = AICC_EntPos(te, &b1)->x;
	const vec3_t *t = AICC_EntPos(te, &b2);
	float d = AICC2D_FlatDist(tx - px, t->z - pz);
	switch (self->walkDir) {
	case 0:
		if (d > self->farDist) {
			self->walkDir = 1;
			break;
		}
		if (!cSlotController_Tick(&self->slotWalk, self->dt) || !cSlotController_FireReset(&self->slotWalk, &f)) {
			break;
		}
		do {
			if (self->nearDist > d) {
				cEntity *e = AICC_TargetEnt(self);
				cEntity *me = self->ent;
				float r = Obj_GetRadius(me);
				const vec3_t *tp = AICC_EntPos(e, &b1);
				if (AICC_TestSides(self, AICC_EntPos(me, &b2), tp, r)) {
					self->walkDir = 2;
				}
			}
		} while (cSlotController_FireReset(&self->slotWalk, &f));
		break;
	case 1:
		if ((self->nearDist + self->farDist) * 0.5f > d) {
			self->walkDir = 0;
		}
		break;
	case 2:
		if (d > (self->nearDist + self->farDist) * 0.5f) {
			self->walkDir = 0;
			break;
		}
		if (!cSlotController_Tick(&self->slotWalk, self->dt) || !cSlotController_FireReset(&self->slotWalk, &f)) {
			break;
		}
		do {
			cEntity *e = AICC_TargetEnt(self);
			cEntity *me = self->ent;
			float r = Obj_GetRadius(me);
			const vec3_t *tp = AICC_EntPos(e, &b1);
			if (!AICC_TestSides(self, AICC_EntPos(me, &b2), tp, r)) {
				self->walkDir = 0;
			}
		} while (cSlotController_FireReset(&self->slotWalk, &f));
		break;
	}
	switch (self->walkDir) {
	case 0:
		self->ctrl->walk = 0.0f;
		break;
	case 1:
		self->ctrl->walk = self->speed;
		break;
	case 2:
		self->ctrl->walk = -self->speed;
		break;
	}
	if (self->blocked || self->stuck || self->orbitSide) {
		self->ctrl->walk = 0.0f;
	}
}

void *g_AStar2D;
CNodesWorld2D *g_NodesWorld2D;

/* the path of pathfinding to p; the point after the start becomes the goal */
byte cAICharControl2D_FindPath(cAICharControl2D *self, const vec3_t *p) {
	vec3_t pos;
	byte noPath, reach;

	CHECK((g_NodesWorld2D != NULL) && (g_AStar2D != NULL));
	CHECK(NodesWorld_IsCreated(NW2D_SUP(g_NodesWorld2D)));
	self->dest = *p;
	AICC_Pos(self, &pos);
	float fr = AICC_FloorRadius(self);
	byte ok = CAStar2D_FindPath(g_AStar2D, &self->path, &self->dest, &pos, self->radius, self->arriveDist,
		WChar(self->ent)->floorDist, fr, self->soft, &noPath, &reach, &self->pathRes);
	if (!ok) {
		if (self->soft) {
			self->dest = *p;
			fr = AICC_FloorRadius(self);
			ok = CAStar2D_FindPath(g_AStar2D, &self->path, &self->dest, &pos, self->radius, self->arriveDist,
				WChar(self->ent)->floorDist, fr, 0, &noPath, &reach, &self->pathRes);
		}
		if (!ok) {
			if (!reach) {
				AIMove_Event(self->ai, AIEV_OBJECTIVE_NOT_FOUND);
				return 0;
			}
			if (!noPath) {
				AIMove_Event(self->ai, AIEV_LOST_AGENT);
			}
			return 0;
		}
	}
	vec3_t *pt = NULL;
	if (self->path.head) {
		self->path.iter = self->path.head->prev;
		if (self->path.iter) {
			pt = &self->path.iter->v;
		}
	}
	self->pathPoint = pt;
	if (self->path.count == 2 && pt->x == pos.x && pt->y == pos.y && pt->z == pos.z) {
		self->goal = self->dest;
	} else {
		self->goal = *pt;
	}
	self->newPath = 0;
	return 1;
}

byte cAICharControl2D_FindRoute(cAICharControl2D *self) {
	vec3_t pos;

	CHECK(g_NodesWorld2D != NULL);
	CHECK(NodesWorld_IsCreated(NW2D_SUP(g_NodesWorld2D)));
	AICC_Pos(self, &pos);
	float fr = AICC_FloorRadius(self);
	if (!CNodesWorld2D_BuildPath(g_NodesWorld2D, &self->path, g_DepthPathRouteInDoorAI, &self->routePoint, &pos,
			self->routeA, &self->routeNode, self->radius, WChar(self->ent)->floorDist, fr, self->routeB)) {
		AIMove_Event(self->ai, AIEV_LOST_AGENT);
		return 0;
	}
	self->path.iter = self->path.head;
	vec3_t *pt = self->path.iter ? &self->path.iter->v : NULL;
	self->dest = *pt;
	pt = NULL;
	if (self->path.head) {
		self->path.iter = self->path.head->prev;
		if (self->path.iter) {
			pt = &self->path.iter->v;
		}
	}
	self->pathPoint = pt;
	self->goal = *pt;
	self->newPath = 0;
	AICC_NextGoal(self);
	return 1;
}

float cAICharControl2D_Dist(cAICharControl2D *self, const vec3_t *a, const vec3_t *b) {
	(void)self;
	float dz = a->z - b->z;
	float dx = a->x - b->x;
	return (float)__builtin_sqrt((double)(dz * dz + dx * dx));
}

float cAICharControl2D_DistSq(cAICharControl2D *self, const vec3_t *a, const vec3_t *b) {
	(void)self;
	float dz = a->z - b->z;
	float dx = a->x - b->x;
	return dz * dz + dx * dx;
}

float cAICharControl2D_KeepDist(cAICharControl2D *self, cEntity *hitEnt) {
	CHECK(hitEnt != NULL);
	float er = WChar(hitEnt)->wl.prim.radius;
	float shirk = g_DistShirkCharAI;
	float t = (float)(((Obj_GetRadius(self->ent)) + er) * shirk);
	er = WChar(hitEnt)->wl.prim.radius;
	float c = (float)((Obj_GetRadius(self->ent)) + er + t);
	float d = WChar(self->ent)->wl.prim.radius + WChar(hitEnt)->wl.prim.radius + t;
	return Float_MaxSS(c, d);
}

float cAICharControl2D_StrafeDist(cAICharControl2D *self, cEntity *hitEnt) {
	vec3_t e, s;

	CHECK(hitEnt != NULL);
	AICC_EntPos(hitEnt, &e);
	e.y = 0.0f;
	AICC_Pos(self, &s);
	float dx = e.x - s.x;
	float dy = e.y - 0.0f;
	float dz = e.z - s.z;
	return (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) + g_MarginCollCharAI;
}

/* the angle between the view (the axis fwd turned by it) and the way to e on the floor below ang */
static byte AICC2D_InCone(cAICharControl *self, cEntity *e, float ang, float fwd) {
	vec3_t sbuf, ebuf, dir, v, rot, o;

	const vec3_t *s = AICC_Pos(self, &sbuf);
	const vec3_t *p = AICC_EntPos(e, &ebuf);
	dir.y = 0.0f;
	dir.x = p->x - s->x;
	dir.z = p->z - s->z;
	v.x = 0.0f;
	v.y = 0.0f;
	v.z = fwd;
	rot = WChar(self->ent)->view;
	Vec3_Rotate(&o, &v, &rot);
	o.y = 0.0f;
	return (double)ang > Vec3_Angle(&o, &dir);
}

byte cAICharControl2D_InFront(cAICharControl2D *self, cEntity *e, float ang) {
	return AICC2D_InCone(self, e, ang, 1.0f);
}

byte cAICharControl2D_Behind(cAICharControl2D *self, cEntity *e, float ang) {
	return AICC2D_InCone(self, e, ang, -1.0f);
}

byte cAICharControl2D_GoalAhead(cAICharControl2D *self) {
	vec3_t buf, a, b;

	const vec3_t *s = AICC_Pos(self, &buf);
	a.x = self->dest.x - s->x;
	a.y = 0.0f;
	a.z = self->dest.z - s->z;
	const vec3_t *pp = self->pathPoint;
	b.y = 0.0f;
	b.x = self->dest.x - pp->x;
	b.z = self->dest.z - pp->z;
	return 0.39269909262657166 > Vec3_Angle(&a, &b);
}

byte cAICharControl2D_OnFloor(cAICharControl2D *self, const vec3_t *p) {
	const vec3_t *list;
	int count = 0;

	float fr = AICC_FloorRadius(self);
	float fd = WChar(self->ent)->floorDist;
	float r = Obj_GetRadius(self->ent);
	CAStar2D_GetPointsVis(g_AStar2D, p, &list, 1, &count, r, fd, fr);
	return count != 0;
}

byte cAICharControl2D_Near(cAICharControl2D *self, cEntity *e) {
	vec3_t b1, b2;

	const vec3_t *p = AICC_EntPos(e, &b1);
	const vec3_t *s = AICC_Pos(self, &b2);
	float dx = s->x - p->x;
	float dz = s->z - p->z;
	float d = (float)__builtin_sqrt((double)(dx * dx + 0.0f + dz * dz));
	float k = (float)((AICC_KeepDist(self, e)) + 1.0f);
	return d > k;
}

byte cAICharControl2D_InWay(cAICharControl2D *self, vec3_t dest, cEntity *hitEnt) {
	vec3_t buf;

	CHECK(hitEnt != NULL);
	const vec3_t *p = AICC_EntPos(hitEnt, &buf);
	float dx = dest.x - p->x;
	float dz = dest.z - p->z;
	float d = (float)__builtin_sqrt((double)(dx * dx + 0.0f + dz * dz));
	float k = AICC_KeepDist(self, hitEnt);
	return k >= d;
}

/* TestPath (end 0: the 7 lines along the way from a to b) and TestPathEnd (end 1: the lines from the points around
 * a to b, then the step of a walk forward) */
static byte AICC2D_Corridor(cAICharControl *self, const vec3_t *a, const vec3_t *b, float r, int end) {
	cEntity *me = self->ent;
	vec3_t hitA, hitB, A, D, N, P, s, U, V, nl, in, tmp;
	vec3_t p1, q1, p2, q2, p3, q3, p4, q4, p5, q5, p6, q6, p7, q7;
	const vec3_t up = { 0.0f, 1.0f, 0.0f };
	const vec3_t *to = end ? b : NULL;

	if (!NW2D_Probe(a, &hitA)) {
		return 0;
	}
	float fd = WChar(me)->floorDist;
	A.x = hitA.x;
	A.y = fd + hitA.y;
	A.z = hitA.z;
	float h = Float_MaxSS((float)(fd - (Obj_GetRadius(me))), g_NW2DHeight);
	hitA.y = hitA.y + h;
	if (!NW2D_Probe(b, &hitB)) {
		return 0;
	}
	hitB.y = hitB.y + h;
	if (Engine_TestLine(&hitA, &hitB, NULL, NULL)) {
		return 0;
	}
	D.y = hitB.y - hitA.y;
	D.x = hitB.x - hitA.x;
	D.z = hitB.z - hitA.z;
	float len = (float)__builtin_sqrt((double)(D.y * D.y + D.x * D.x + D.z * D.z));
	float L = Float_MinSS(len, (float)((Obj_GetRadius(me)) * g_FactorDistTestColCharAI));
	if (len == 0.0f) {
		N = D;
	} else {
		float inv = 1.0f / len;
		N.x = D.x * inv;
		N.y = D.y * inv;
		N.z = D.z * inv;
	}
	/* the side: N across the up axis */
	P.x = N.y * 0.0f - N.z;
	P.y = N.z * 0.0f - N.x * 0.0f;
	P.z = N.x - N.y * 0.0f;
	if (P.x == 0.0f && P.y == 0.0f && P.z == 0.0f) {
		return 1;
	}
	s = *Vec3_NormalizeTo(&tmp, &P);
	nl.x = N.x * L;
	nl.y = N.y * L;
	nl.z = N.z * L;
	if (!AICC_Seg(&p1, &q1, &A, &s, Obj_GetRadius(me), &nl, to, L)) {
		return 0;
	}
	s.x = -s.x;
	s.y = -s.y;
	s.z = -s.z;
	if (!AICC_Seg(&p2, &q2, &A, &s, Obj_GetRadius(me), &nl, to, L)) {
		return 0;
	}
	if (!AICC_Seg(&p3, &q3, &A, &up, Obj_GetRadius(me), &nl, to, L)) {
		return 0;
	}
	in.x = s.x + 0.0f;
	in.y = s.y + 1.0f;
	in.z = s.z + 0.0f;
	Vec3_NormalizeTo(&U, &in);
	if (!AICC_Seg(&p4, &q4, &A, &U, Obj_GetRadius(me), &nl, to, L)) {
		return 0;
	}
	U.x = -U.x;
	U.y = -U.y;
	U.z = -U.z;
	if (!AICC_Seg(&p5, &q5, &A, &U, Obj_GetRadius(me), &nl, to, L)) {
		return 0;
	}
	in.x = s.x - 0.0f;
	in.y = s.y - 1.0f;
	in.z = s.z - 0.0f;
	V = *Vec3_NormalizeTo(&tmp, &in);
	if (!AICC_Seg(&p6, &q6, &A, &V, Obj_GetRadius(me), &nl, to, L)) {
		return 0;
	}
	V.x = -V.x;
	V.y = -V.y;
	V.z = -V.z;
	if (!AICC_Seg(&p7, &q7, &A, &V, Obj_GetRadius(me), &nl, to, L)) {
		return 0;
	}
	if (end && self->walkDir == 1 && !AICC2D_TestStep(self, &A, &hitA, &hitB, r)) {
		return 0;
	}
	if (!AICC2D_FloorAhead(self, b)) {
		return 0;
	}
	/* the corridor for the debug view; segment 4 is not set here */
	self->debugSegs[0].a = hitA;
	self->debugSegs[0].b = hitB;
	self->debugSegs[1].a = p1;
	self->debugSegs[1].b = q1;
	self->debugSegs[2].a = p2;
	self->debugSegs[2].b = q2;
	self->debugSegs[3].a = p3;
	self->debugSegs[3].b = q3;
	self->debugSegs[5].a = p4;
	self->debugSegs[5].b = q4;
	self->debugSegs[6].a = p5;
	self->debugSegs[6].b = q5;
	self->debugSegs[7].a = p6;
	self->debugSegs[7].b = q6;
	self->debugSegs[8].a = p7;
	self->debugSegs[8].b = q7;
	return 1;
}

byte cAICharControl2D_TestPath(cAICharControl2D *self, const vec3_t *a, const vec3_t *b, float r) {
	if (b->x == self->dest.x && b->y == self->dest.y && b->z == self->dest.z) {
		return AICC_TestPathEnd(self, a, b, r);
	}
	return AICC2D_Corridor(self, a, b, r, 0);
}

byte cAICharControl2D_TestPathEnd(cAICharControl2D *self, const vec3_t *a, const vec3_t *b, float r) {
	return AICC2D_Corridor(self, a, b, r, 1);
}

byte cAICharControl2D_TestStep(cAICharControl2D *self, const vec3_t *a, const vec3_t *b, const vec3_t *c,
	float r) {
	cEntity *me = self->ent;
	vec3_t D, N, P, s, U, V, nl, in, tmp, p, q;
	const vec3_t up = { 0.0f, 1.0f, 0.0f };

	(void)r;
	D.y = c->y - b->y;
	D.x = c->x - b->x;
	D.z = c->z - b->z;
	float len = (float)__builtin_sqrt((double)(D.y * D.y + D.x * D.x + D.z * D.z));
	float L = Float_MinSS(len, (float)((Obj_GetRadius(me)) + 5.0f));
	if (len == 0.0f) {
		N = D;
	} else {
		float inv = 1.0f / len;
		N.x = D.x * inv;
		N.y = D.y * inv;
		N.z = D.z * inv;
	}
	P.x = N.y * 0.0f - N.z;
	P.y = N.z * 0.0f - N.x * 0.0f;
	P.z = N.x - N.y * 0.0f;
	if (P.x == 0.0f && P.y == 0.0f && P.z == 0.0f) {
		return 1;
	}
	s = *Vec3_NormalizeTo(&tmp, &P);
	nl.x = N.x * L;
	nl.y = N.y * L;
	nl.z = N.z * L;
	if (!AICC_Seg(&p, &q, a, &s, Obj_GetRadius(me), &nl, NULL, 0.0f)) {
		return 0;
	}
	s.x = -s.x;
	s.y = -s.y;
	s.z = -s.z;
	if (!AICC_Seg(&p, &q, a, &s, Obj_GetRadius(me), &nl, NULL, 0.0f)) {
		return 0;
	}
	if (!AICC_Seg(&p, &q, a, &up, Obj_GetRadius(me), &nl, NULL, 0.0f)) {
		return 0;
	}
	in.x = s.x + 0.0f;
	in.y = s.y + 1.0f;
	in.z = s.z + 0.0f;
	Vec3_NormalizeTo(&U, &in);
	if (!AICC_Seg(&p, &q, a, &U, Obj_GetRadius(me), &nl, NULL, 0.0f)) {
		return 0;
	}
	U.x = -U.x;
	U.y = -U.y;
	U.z = -U.z;
	if (!AICC_Seg(&p, &q, a, &U, Obj_GetRadius(me), &nl, NULL, 0.0f)) {
		return 0;
	}
	in.x = s.x - 0.0f;
	in.y = s.y - 1.0f;
	in.z = s.z - 0.0f;
	V = *Vec3_NormalizeTo(&tmp, &in);
	if (!AICC_Seg(&p, &q, a, &V, Obj_GetRadius(me), &nl, NULL, 0.0f)) {
		return 0;
	}
	V.x = -V.x;
	V.y = -V.y;
	V.z = -V.z;
	return AICC_Seg(&p, &q, a, &V, Obj_GetRadius(me), &nl, NULL, 0.0f) != 0;
}

byte cAICharControl2D_TestSides(cAICharControl2D *self, const vec3_t *a, const vec3_t *b, float r) {
	cEntity *me = self->ent;
	vec3_t hitA, hitB, A, D, N, M, P, s, U, W, ml, in, tmp;
	vec3_t p1, q1, p2, q2, p3, q3, p4, q4, p5, q5;
	const vec3_t up = { 0.0f, 1.0f, 0.0f };

	(void)r;
	if (!NW2D_Probe(a, &hitA)) {
		return 0;
	}
	float fd = WChar(me)->floorDist;
	A.x = hitA.x;
	A.y = fd + hitA.y;
	A.z = hitA.z;
	float h = Float_MaxSS((float)(fd - (Obj_GetRadius(me))), g_NW2DHeight);
	hitA.y = hitA.y + h;
	if (!NW2D_Probe(b, &hitB)) {
		return 0;
	}
	hitB.y = hitB.y + h;
	float L = (float)((Obj_GetRadius(me)) * g_FactorDistTestColBackCharAI);
	D.y = hitB.y - hitA.y;
	D.x = hitB.x - hitA.x;
	D.z = hitB.z - hitA.z;
	float len = (float)__builtin_sqrt((double)(D.y * D.y + D.x * D.x + D.z * D.z));
	if (len == 0.0f) {
		N = D;
	} else {
		float inv = 1.0f / len;
		N.x = D.x * inv;
		N.y = inv * D.y;
		N.z = D.z * inv;
	}
	/* back along the line, not down */
	M.x = -N.x;
	M.y = -N.y;
	M.z = -N.z;
	if (0.0f > M.y) {
		M.y = 0.0f;
	}
	P.x = M.y * 0.0f - M.z;
	P.y = M.z * 0.0f - M.x * 0.0f;
	P.z = M.x - M.y * 0.0f;
	if (P.x == 0.0f && P.y == 0.0f && P.z == 0.0f) {
		return 1;
	}
	Vec3_NormalizeTo(&s, &P);
	ml.x = M.x * L;
	ml.y = M.y * L;
	ml.z = M.z * L;
	if (!AICC_Seg(&p1, &q1, &A, &s, Obj_GetRadius(me), &ml, NULL, 0.0f)) {
		return 0;
	}
	s.x = -s.x;
	s.y = -s.y;
	s.z = -s.z;
	if (!AICC_Seg(&p2, &q2, &A, &s, Obj_GetRadius(me), &ml, NULL, 0.0f)) {
		return 0;
	}
	if (!AICC_Seg(&p3, &q3, &A, &up, Obj_GetRadius(me), &ml, NULL, 0.0f)) {
		return 0;
	}
	in.x = s.x + 0.0f;
	in.y = s.y + 1.0f;
	in.z = s.z + 0.0f;
	Vec3_NormalizeTo(&U, &in);
	if (!AICC_Seg(&p4, &q4, &A, &U, Obj_GetRadius(me), &ml, NULL, 0.0f)) {
		return 0;
	}
	in.x = s.x - 0.0f;
	in.y = s.y - 1.0f;
	in.z = s.z - 0.0f;
	W = *Vec3_NormalizeTo(&tmp, &in);
	W.x = -W.x;
	W.y = -W.y;
	W.z = -W.z;
	if (!AICC_Seg(&p5, &q5, &A, &W, Obj_GetRadius(me), &ml, NULL, 0.0f)) {
		return 0;
	}
	/* the corridor for the debug view; segment 4 is not set here */
	memset(&self->debugSegs[0], 0, sizeof(cAICCSeg));
	self->debugSegs[1].a = p1;
	self->debugSegs[1].b = q1;
	self->debugSegs[2].a = p2;
	self->debugSegs[2].b = q2;
	self->debugSegs[3].a = p3;
	self->debugSegs[3].b = q3;
	self->debugSegs[5].a = p4;
	self->debugSegs[5].b = q4;
	memset(&self->debugSegs[6], 0, 2 * sizeof(cAICCSeg));
	self->debugSegs[8].a = p5;
	self->debugSegs[8].b = q5;
	return 1;
}

/* the floor ahead towards p (on the floor) within the reach of a step: free, or what is hit leaves a floor point
 * before it (at most 20 back, half the way) with room for the body */
byte cAICharControl2D_FloorAhead(cAICharControl2D *self, const vec3_t *p) {
	cEntity *me = self->ent;
	vec3_t S, H, Q, B, out, N;

	AICC_Pos(self, &S);
	S.y = (float)(S.y - (Obj_GetRadius(me)));
	float dx = p->x - S.x;
	float dz = p->z - S.z;
	float len = (float)__builtin_sqrt((double)(dx * dx + 0.0f + dz * dz));
	if (len == 0.0f) {
		N.x = dx;
		N.y = 0.0f;
		N.z = dz;
	} else {
		float inv = 1.0f / len;
		N.x = dx * inv;
		N.y = inv * 0.0f;
		N.z = dz * inv;
	}
	float r = Obj_GetRadius(me);
	r = r + r;
	float q = (float)__builtin_sqrt(((double)(Obj_GetRadius(me)) * r));
	float m = (float)((Obj_GetRadius(me)) + q + 5.0f);
	float w = Float_MinSS(m, len);
	Q.x = N.x * w + S.x;
	Q.y = N.y * w + S.y;
	Q.z = N.z * w + S.z;
	if (!Engine_TestLine(&S, &Q, &H, NULL)) {
		return 1;
	}
	float hx = S.x - H.x;
	float hy = S.y - H.y;
	float hz = S.z - H.z;
	float d = (float)__builtin_sqrt((double)(hy * hy + hx * hx + hz * hz));
	float back = 20.0f > d ? d * 0.5f : 20.0f;
	B.x = H.x - N.x * back;
	B.y = H.y - N.y * back;
	B.z = H.z - N.z * back;
	if (!AICC2D_FloorAt(self, &B, &out)) {
		return 0;
	}
	r = Obj_GetRadius(me);
	return Engine_TestSphere(&out, &out, r, NULL, NULL) == 0;
}

byte cAICharControl2D_FloorAt(cAICharControl2D *self, const vec3_t *p, vec3_t *out) {
	if (!NW2D_Probe(p, out)) {
		*out = *p;
		return 0;
	}
	out->y = WChar(self->ent)->floorDist + out->y;
	return 1;
}

/* p moved by a random step of at most 150 on the floor (3 tries) while the way from the point of the path and to
 * the next one stays free */
void cAICharControl2D_StepGoal(cAICharControl2D *self, vec3_t *p) {
	if (p->x == self->dest.x && p->y == self->dest.y && p->z == self->dest.z) {
		return;
	}
	cNWVecNode *save = self->path.iter;
	cNWVecNode *n = save->prev;
	self->path.iter = n;
	vec3_t *next = n == self->path.head->prev ? NULL : &n->v;
	for (int i = 0; i < 3;) {
		vec3_t q;

		i++;
		float k = 150.0f;
		float nk = -k;
		double range = (double)(k - nk);
		double base = (double)nk;
		float rx = FxElem_Rand(range, base);
		float rz = FxElem_Rand(range, base);
		q.x = p->x + rx;
		q.y = p->y + 0.0f;
		q.z = p->z + rz;
		if (Engine_TestSphere(&q, &q, self->radius, NULL, NULL)) {
			continue;
		}
		float fr = AICC_FloorRadius(self);
		if (!CNodesWorld2D_TestPath(g_NodesWorld2D, &q, self->pathPoint, self->radius, self->radius,
				WChar(self->ent)->floorDist, fr, NULL)) {
			continue;
		}
		fr = AICC_FloorRadius(self);
		if (!CNodesWorld2D_TestPath(g_NodesWorld2D, &q, next, self->radius, self->radius,
				WChar(self->ent)->floorDist, fr, NULL)) {
			continue;
		}
		*p = q;
		break;
	}
	self->path.iter = save;
}

/* the point at dist from pos along the away direction a turned by ang around y (the sign of the side) */
static void AICC2D_ArcPoint(vec3_t *out, const vec3_t *pos, const vec3_t *a, float ang, float dist) {
	vec3_t rot, v, n;

	rot.x = 0.0f;
	rot.z = 0.0f;
	rot.y = ang;
	Vec3_NormalizeTo(&n, Vec3_Rotate(&v, a, &rot));
	out->x = pos->x + n.x * dist;
	out->y = pos->y + n.y * dist;
	out->z = pos->z + n.z * dist;
}

/* the way of the character from s to a free and the corner s a b on the floor */
static int AICC2D_ArcStep(cAICharControl *self, const vec3_t *s, const vec3_t *a, const vec3_t *b, byte left) {
	cEntity *me = self->ent;

	float fr = AICC_FloorRadius(self);
	float fd = WChar(me)->floorDist;
	float r2 = Obj_GetRadius(me);
	float r = Obj_GetRadius(me);
	if (!CNodesWorld2D_TestCorner(g_NodesWorld2D, s, a, b, r, r2, fd, fr, left, &self->debugPath)) {
		return 0;
	}
	fr = AICC_FloorRadius(self);
	fd = WChar(me)->floorDist;
	r2 = Obj_GetRadius(me);
	r = Obj_GetRadius(me);
	return CNodesWorld2D_TestPath(g_NodesWorld2D, a, b, r, r2, fd, fr, &self->debugPath) != 0;
}

/* the walk around pos on the circle of radius dist from the way away to the way dir (the other way round without
 * flag), in steps of the radius of the character: each step and corner free on the floor */
byte cAICharControl2D_StrafeBack(cAICharControl2D *self, const vec3_t *pos, const vec3_t *away,
	const vec3_t *dir, float dist, byte left, byte flag) {
	cEntity *me = self->ent;
	vec3_t a, d, prev, cur, S, buf;

	DListSeg_Clear(&self->debugPath);
	a = AICC2D_FlatNorm(away->x, away->z);
	d = AICC2D_FlatNorm(dir->x, dir->z);
	float sign = left ? 1.0f : -1.0f;
	double ang = Vec3_Angle(&a, &d);
	if (!flag) {
		ang = 6.2831854820251465 - ang;
	}
	float step = (float)atan2(Obj_GetRadius(me), dist);
	prev.x = pos->x + a.x * dist;
	prev.y = pos->y + a.y * dist;
	prev.z = pos->z + a.z * dist;
	if (g_ShowShirkAI == 1) {
		DListVec3_Add(&self->debugStrafe, AICC_Pos(self, &buf));
		DListVec3_Add(&self->debugStrafe, &prev);
	}
	float fr = AICC_FloorRadius(self);
	float fd = WChar(me)->floorDist;
	float r2 = Obj_GetRadius(me);
	float r = Obj_GetRadius(me);
	if (!CNodesWorld2D_TestPath(g_NodesWorld2D, AICC_Pos(self, &buf), &prev, r, r2, fd, fr, &self->debugPath)) {
		return 0;
	}
	AICC_Pos(self, &S);
	float t = step;
	while (ang > (double)t) {
		AICC2D_ArcPoint(&cur, pos, &a, t * sign, dist);
		if (g_ShowShirkAI == 1) {
			DListVec3_Add(&self->debugStrafe, &cur);
		}
		if (!AICC2D_ArcStep(self, &S, &prev, &cur, left)) {
			return 0;
		}
		t = t + step;
		S = prev;
		prev = cur;
	}
	AICC2D_ArcPoint(&cur, pos, &a, (float)ang * sign, dist);
	if (g_ShowShirkAI == 1) {
		DListVec3_Add(&self->debugStrafe, &cur);
	}
	if (!AICC2D_ArcStep(self, &S, &prev, &cur, left)) {
		return 0;
	}
	return 1;
}

/* the floor below p raised (FloorAt into out) free of the other characters */
static int AICC2D_ArcFloor(cAICharControl *self, const vec3_t *p, vec3_t *out) {
	cEntity *hit;

	if (!AICC2D_FloorAt(self, p, out)) {
		return -1;
	}
	if (g_ShowShirkAI == 1) {
		DListVec3_Add(&self->debugStrafeInner, out);
	}
	float r = WChar(self->ent)->wl.prim.radius + 1.0f - g_MarginCollCharAI;
	return !cWorld_TestSphereEntities(g_pWorld, self->ent, 0x10, out, r, &hit, 1, 0);
}

/* StrafeBack on the floor in half steps: each point of the circle on the floor and free of the other characters */
byte cAICharControl2D_StrafeFwd(cAICharControl2D *self, const vec3_t *pos, const vec3_t *away,
	const vec3_t *dir, float dist, byte left, byte flag) {
	cEntity *me = self->ent;
	vec3_t a, d, cur, out;

	a = AICC2D_FlatNorm(away->x, away->z);
	d = AICC2D_FlatNorm(dir->x, dir->z);
	float sign = left ? 1.0f : -1.0f;
	double ang = Vec3_Angle(&a, &d);
	if (!flag) {
		ang = 6.2831854820251465 - ang;
	}
	float step = (float)atan2(Obj_GetRadius(me), dist) * 0.5f;
	cur.x = pos->x + a.x * dist;
	cur.y = pos->y + a.y * dist;
	cur.z = pos->z + a.z * dist;
	if (AICC2D_ArcFloor(self, &cur, &out) <= 0) {
		return 0;
	}
	float t = step;
	while (ang > (double)t) {
		AICC2D_ArcPoint(&cur, pos, &a, t * sign, dist);
		if (AICC2D_ArcFloor(self, &cur, &out) <= 0) {
			return 0;
		}
		t = t + step;
	}
	AICC2D_ArcPoint(&cur, pos, &a, (float)ang * sign, dist);
	return AICC2D_ArcFloor(self, &cur, &out) > 0;
}

byte cAICharControl2D_TryStrafe(cAICharControl2D *self, int side, cEntity *hitEnt, byte back, float keep,
	byte fwd, float sd, byte e2) {
	vec3_t b1, b2, A, B, C, D, Dc, E;

	CHECK(hitEnt != NULL);
	const vec3_t *ep = AICC_EntPos(hitEnt, &b1);
	const vec3_t *sp = AICC_Pos(self, &b2);
	A.x = sp->x - ep->x;
	A.y = 0.0f;
	A.z = sp->z - ep->z;
	sp = AICC_Pos(self, &b1);
	float gx = self->goal.x - sp->x;
	float gz = self->goal.z - sp->z;
	float R = (float)__builtin_sqrt((double)(A.x * A.x + A.y * A.y + A.z * A.z)) + WChar(hitEnt)->wl.prim.radius;
	vec3_t N = AICC2D_FlatNorm(gx, gz);
	float ox = N.x * R;
	float oz = N.z * R;
	sp = AICC_Pos(self, &b1);
	float px = sp->x + ox;
	float pz = sp->z + oz;
	ep = AICC_EntPos(hitEnt, &b2);
	B.x = px - ep->x;
	B.y = 0.0f;
	B.z = pz - ep->z;
	ep = AICC_EntPos(hitEnt, &b1);
	C.x = self->goal.x - ep->x;
	C.y = 0.0f;
	C.z = self->goal.z - ep->z;
	float lenB = (float)__builtin_sqrt((double)(B.x * B.x + 0.0f + B.z * B.z));
	float lenC = (float)__builtin_sqrt((double)(C.x * C.x + 0.0f + C.z * C.z));
	D = lenC > lenB ? B : C;
	E = *AICC_EntPos(hitEnt, &b2);
	float y;
	if (AICC_Pos(self, &b1)->y > self->goal.y - 10.0f) {
		y = AICC_Pos(self, &b1)->y;
	} else {
		y = keep * 1.5f * 0.5f + AICC_EntPos(hitEnt, &b1)->y;
	}
	E.y = y;
	byte left = side == 2;
	byte res = 1;
	if (fwd) {
		Dc = D;
		byte r = AICC_StrafeFwd(self, &E, &A, &Dc, sd, left, e2);
		if (!r) {
			return r;
		}
	}
	if (back) {
		res = AICC_StrafeBack(self, &E, &A, &D, keep, left, e2) ? res : 0;
	}
	return res;
}

/* the vtable of cAICharControl2D */
static const cAICharControl2DVtbl cAICharControl2D_vftable = {
	.base.Delete = cAICharControl2D_Delete,
	.base.Draw = cAICharControl2D_Draw,
	.base.GetWalkDir = cAICharControl_GetWalkDir,
	.base.Busy = cAICharControl_Busy,
	.base.GetOrbitSide = cAICharControl_GetOrbitSide,
	.base.Update = cAICharControl_Update,
	.base.SetMove = cAICharControl_SetMove,
	.base.Reset = cAICharControl_Reset,
	.base.SetHold = cAICharControl_SetHold,
	.base.Stop = cAICharControl_Stop,
	.base.IsDirect = cAICharControl_IsDirect,
	.base.IsDirect2 = cAICharControl_IsDirect,
	.base.GetGoal = cAICharControl_GetGoal,
	.base.GetDest = cAICharControl_GetDest,
	.base.TestPath = cAICharControl2D_TestPath,
	.base.TestLine = cAICharControl2D_TestLine,
	.base.TestSides = cAICharControl2D_TestSides,
	.base.GetAvoidA = cAICharControl_GetAvoidA,
	.base.GetAvoidB = cAICharControl_GetAvoidB,
	.base.ClearAvoid = cAICharControl_ClearAvoid,
	.base.SetSpeed = cAICharControl_SetSpeed,
	.base.RestartSlots = cAICharControl_RestartSlots,
	.base.IsBlocked = cAICharControl_IsBlocked,
	.base.GetE4 = cAICharControl_GetAvoidFound,
	.base.GetAvoidEnt = cAICharControl_GetAvoidEnt,
	.base.Nop = cAICharControl_Nop,
	.base.AtPathEnd = cAICharControl_AtPathEnd,
	.base.GoalAtStart = cAICharControl_GoalAtStart,
	.base.CheckTarget = cAICharControl_CheckTarget,
	.base.SetTurnSpeed = cAICharControl_SetTurnSpeed,
	.base.StrafeBack = cAICharControl2D_StrafeBack,
	.base.StrafeFwd = cAICharControl2D_StrafeFwd,
	.base.TryStrafe = cAICharControl2D_TryStrafe,
	.base.Facing = cAICharControl2D_Facing,
	.base.InWay = cAICharControl2D_InWay,
	.base.Near = cAICharControl2D_Near,
	.base.KeepDist = cAICharControl2D_KeepDist,
	.base.StrafeDist = cAICharControl2D_StrafeDist,
	.base.OnFloor = cAICharControl2D_OnFloor,
	.base.GoalAhead = cAICharControl2D_GoalAhead,
	.base.TestPathEnd = cAICharControl2D_TestPathEnd,
	.base.Dist = cAICharControl2D_Dist,
	.base.DistSq = cAICharControl2D_DistSq,
	.base.Away = cAICharControl_Away,
	.base.UpdateAway = cAICharControl_UpdateAway,
	.base.AwayCheck = cAICharControl_AwayCheck,
	.base.AwayWalk = cAICharControl_AwayWalk,
	.base.Behind = cAICharControl2D_Behind,
	.base.InFront = cAICharControl2D_InFront,
	.base.Route = cAICharControl_Route,
	.base.SetDir = cAICharControl_SetDir,
	.base.UpdateDir = cAICharControl_UpdateDir,
	.base.DirCheck = cAICharControl_DirCheck,
	.base.DirTurn = cAICharControl_DirTurn,
	.base.CheckArrived = cAICharControl2D_CheckArrived,
	.base.CanMove = cAICharControl_CanMove,
	.base.GoTo = cAICharControl_GoTo,
	.base.GoToDirect = cAICharControl_GoToDirect,
	.base.FindPath = cAICharControl2D_FindPath,
	.base.DirectPath = cAICharControl_DirectPath,
	.base.StepGoal = cAICharControl2D_StepGoal,
	.base.UpdateGoTo = cAICharControl_UpdateGoTo,
	.base.UpdateRoute = cAICharControl_UpdateRoute,
	.base.UpdateGoToDirect = cAICharControl_UpdateGoToDirect,
	.base.GoalOnLine = cAICharControl_GoalOnLine,
	.base.NextGoal = cAICharControl_NextGoal,
	.base.NextGoalPath = cAICharControl_NextGoalPath,
	.base.UpdatePath = cAICharControl_UpdatePath,
	.base.CheckPathEnd = cAICharControl_CheckPathEnd,
	.base.UpdateRoutePath = cAICharControl_UpdateRoutePath,
	.base.Turn = cAICharControl2D_Turn,
	.base.Strafe = cAICharControl_Strafe,
	.base.FindAvoid = cAICharControl_FindAvoid,
	.base.Avoid = cAICharControl_Avoid,
	.base.UpdateAvoid = cAICharControl_UpdateAvoid,
	.base.CharsNear = cAICharControl_CharsNear,
	.base.SideBlocked = cAICharControl2D_SideBlocked,
	.base.SideDir = cAICharControl_SideDir,
	.base.WalkFwd = cAICharControl_WalkFwd,
	.base.CheckGoTo = cAICharControl_CheckGoTo,
	.base.CheckRoute = cAICharControl_CheckRoute,
	.base.CheckFollow = cAICharControl_CheckFollow,
	.base.FindRoute = cAICharControl2D_FindRoute,
	.base.TurnFollow = cAICharControl_TurnFollow,
	.base.Follow = cAICharControl_Follow,
	.base.UpdateFollowMode = cAICharControl_UpdateFollowMode,
	.base.Wait = cAICharControl_Wait,
	.base.CheckWait = cAICharControl_CheckWait,
	.base.StrafeWalk = cAICharControl_StrafeWalk,
	.base.Vf89 = cAICharControl_Vf89,
	.base.Strafe2 = cAICharControl_Strafe2,
	.base.Walk = cAICharControl2D_Walk,
	.base.Walk2 = cAICharControl_Walk2,
	.base.UpdateFollow = cAICharControl2D_UpdateFollow,
	.base.UpdateFollowB = cAICharControl_UpdateFollowB,
	.base.FollowPath = cAICharControl_FollowPath,
	.base.UpdateFollowPath = cAICharControl_UpdateFollowPath,
	.base.Orbit = cAICharControl_Orbit,
	.base.StartOrbit = cAICharControl_StartOrbit,
	.base.Stop2 = cAICharControl_Stop2,
	.FloorAhead = cAICharControl2D_FloorAhead,
	.FloorAt = cAICharControl2D_FloorAt,
	.TestStep = cAICharControl2D_TestStep,
};

cGVar GVar_DepthPathRouteInDoorAI;
cGVar GVar_DistShirkCharAI;
cGVar GVar_FactorDistTestColCharAI;
static cGVar GVar_FactorDistTestColBackCharAI;
cGVar GVar_MarginCollCharAI;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_DepthPathRouteInDoorAI, &GVar_DepthPathRouteInDoorAI, "DepthPathRouteInDoorAI", 4, "Depth Path in Route.\n", 6)
INIT_GVAR_FLOAT(Init_GVar_DistShirkCharAI, &GVar_DistShirkCharAI, "DistShirkCharAI", 4, "Factor distance change shirk to move.\n", 0.1f)
INIT_GVAR_FLOAT(Init_GVar_MarginCollCharAI, &GVar_MarginCollCharAI, "MarginCollCharAI", 4, "Margin to test collision with characters.\n", 5.0f)
INIT_GVAR_FLOAT(Init_GVar_FactorDistTestColCharAI, &GVar_FactorDistTestColCharAI, "FactorDistTestColCharAI", 4, "Factor distance to check collision.\n", 6.0f)
INIT_GVAR_FLOAT(Init_GVar_FactorDistTestColBackCharAI, &GVar_FactorDistTestColBackCharAI, "FactorDistTestColBackCharAI", 4, "Factor distance to check collision while backward.\n", 1.5f)
