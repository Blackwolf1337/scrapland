/*
 * The move control of a character that flies (AI/AICharControl3D.cpp, include/scrap/aicharcontrol.h): the paths on
 * the graph CNodesWorld, the tests of the way in all three axes, the turn of the view to the goal, the walk to or away
 * from the target, the strafe around a character in the way.
 */
#include <float.h>
#include <string.h>

#include "scrap/aicharcontrol.h"
#include "scrap/fxelements.h"
#include "scrap/fxmisc.h"
#include "scrap/scrapcollision.h"

static inline float AICC3D_Len(float x, float y, float z) {
	return (float)__builtin_sqrt((double)(y * y + x * x + z * z));
}

static inline float AICC3D_DistTo(cAICharControl *self, const vec3_t *t) {
	vec3_t buf;

	const vec3_t *p = AICC_Pos(self, &buf);
	return AICC3D_Len(p->x - t->x, p->y - t->y, p->z - t->z);
}

/* ---- construction ---- */

cAICharControl *__thiscall cAICharControl3D_ctor(void *mem, cEntity *ent, cAIChar *ai, int *arg) {
	cAICharControl *self = cAICharControl_ctor(mem, ent, ai, arg);

	self->vftable = CAICHARCONTROL3D_VFTABLE;
	return self;
}

void __fastcall cAICharControl3D_dtor(cAICharControl3D *self) {
	self->vftable = CAICHARCONTROL3D_VFTABLE;
	cAICharControl_dtor(self);
}

cAICharControl3D *__thiscall cAICharControl3D_Delete(cAICharControl3D *self, byte flags) {
	self->vftable = CAICHARCONTROL3D_VFTABLE;
	cAICharControl_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAICHARCONTROL_SIZE);
	}
	return self;
}

void __thiscall cAICharControl3D_Draw(cAICharControl3D *self) {
	cAICharControl_Draw(self);
}

/* ---- the tests of the way ---- */

byte __thiscall cAICharControl3D_Facing(cAICharControl3D *self, cEntity *hitEnt, vec3_t *toE, vec3_t *toGoal) {
	vec3_t b1, b2;

	CHECK(hitEnt != NULL);
	const vec3_t *s = AICC_Pos(self, &b1);
	const vec3_t *e = AICC_EntPos(hitEnt, &b2);
	toE->x = e->x - s->x;
	toE->y = e->y - s->y;
	toE->z = e->z - s->z;
	Vec3_NormalizeTo(toE, toE);
	s = AICC_Pos(self, &b1);
	toGoal->y = self->goal.y - s->y;
	toGoal->x = self->goal.x - s->x;
	toGoal->z = self->goal.z - s->z;
	Vec3_NormalizeTo(toGoal, toGoal);
	if (self->walkDir == 2) {
		toGoal->x = -toGoal->x;
		toGoal->y = -toGoal->y;
		toGoal->z = -toGoal->z;
	}
	return Vec3_Angle(toE, toGoal) < 1.5707963267948966;
}

byte __thiscall cAICharControl3D_TestLine(cAICharControl3D *self, const vec3_t *a, const vec3_t *b) {
	(void)self;
	return Engine_TestLine(a, b, NULL, NULL) == 0;
}

/* the 8 lines around a of the corridors: to the side of n (and up for n vertical), turned around n; each from
 * a + v * r along n * len (aim: to b) */
/* n x (0, 1, 0) as the original computes it (the products with 0 kept), (1, 0, 0) for a vertical n */
static vec3_t AICC3D_Side(const vec3_t *n) {
	vec3_t P;

	P.x = n->y * 0.0f - n->z;
	P.y = n->z * 0.0f - n->x * 0.0f;
	P.z = n->x - n->y * 0.0f;
	if (P.x == 0.0f && P.y == 0.0f && P.z == 0.0f) {
		P.x = 1.0f;
		P.y = 0.0f;
		P.z = 0.0f;
	}
	return P;
}

static int AICC3D_Lines(const vec3_t *a, const vec3_t *n, float r, float len, const vec3_t *aim, cAICCSeg *segs) {
	vec3_t P, s, u, w, in, nl, tmp;

	P = AICC3D_Side(n);
	Vec3_NormalizeTo(&s, &P);
	nl.x = n->x * len;
	nl.y = n->y * len;
	nl.z = n->z * len;
	if (!AICC_Seg(&segs[0].a, &segs[0].b, a, &s, r, &nl, aim, len)) {
		return 0;
	}
	s.x = -s.x;
	s.y = -s.y;
	s.z = -s.z;
	if (!AICC_Seg(&segs[1].a, &segs[1].b, a, &s, r, &nl, aim, len)) {
		return 0;
	}
	in.x = s.z * n->y - s.y * n->z;
	in.y = s.x * n->z - s.z * n->x;
	in.z = s.y * n->x - s.x * n->y;
	u = *Vec3_NormalizeTo(&tmp, &in);
	if (!AICC_Seg(&segs[2].a, &segs[2].b, a, &u, r, &nl, aim, len)) {
		return 0;
	}
	u.x = -u.x;
	u.y = -u.y;
	u.z = -u.z;
	if (!AICC_Seg(&segs[3].a, &segs[3].b, a, &u, r, &nl, aim, len)) {
		return 0;
	}
	in.x = u.x + s.x;
	in.y = u.y + s.y;
	in.z = u.z + s.z;
	Vec3_NormalizeTo(&w, &in);
	if (!AICC_Seg(&segs[4].a, &segs[4].b, a, &w, r, &nl, aim, len)) {
		return 0;
	}
	w.x = -w.x;
	w.y = -w.y;
	w.z = -w.z;
	if (!AICC_Seg(&segs[5].a, &segs[5].b, a, &w, r, &nl, aim, len)) {
		return 0;
	}
	in.x = s.x - u.x;
	in.y = s.y - u.y;
	in.z = s.z - u.z;
	w = *Vec3_NormalizeTo(&tmp, &in);
	if (!AICC_Seg(&segs[6].a, &segs[6].b, a, &w, r, &nl, aim, len)) {
		return 0;
	}
	w.x = -w.x;
	w.y = -w.y;
	w.z = -w.z;
	return AICC_Seg(&segs[7].a, &segs[7].b, a, &w, r, &nl, aim, len);
}

/* the way from a to b and its unit vector */
static float AICC3D_Way(const vec3_t *a, const vec3_t *b, vec3_t *n) {
	vec3_t d;

	d.y = b->y - a->y;
	d.x = b->x - a->x;
	d.z = b->z - a->z;
	float len = AICC3D_Len(d.x, d.y, d.z);
	if (len == 0.0f) {
		*n = d;
	} else {
		float inv = 1.0f / len;
		n->x = d.x * inv;
		n->y = d.y * inv;
		n->z = d.z * inv;
	}
	return len;
}

static byte AICC3D_Corridor(cAICharControl *self, const vec3_t *a, const vec3_t *b, int end) {
	cEntity *me = self->ent;
	cAICCSeg segs[8];
	vec3_t A0 = *a, B0 = *b, n;

	if (Engine_TestLine(a, b, NULL, NULL)) {
		return 0;
	}
	float r = Obj_Vtbl(me)->GetRadius(me);
	float t = AICC_X87Mul(Obj_Vtbl(me)->GetRadius(me), g_FactorDistTestColCharAI);
	float len = Float_MinSS(AICC3D_Way(a, b, &n), t);
	if (!AICC3D_Lines(a, &n, r, len, end ? b : NULL, segs)) {
		return 0;
	}
	self->debugSegs[0].a = A0;
	self->debugSegs[0].b = B0;
	memcpy(&self->debugSegs[1], segs, sizeof(segs));
	return 1;
}

byte __thiscall cAICharControl3D_TestPath(cAICharControl3D *self, const vec3_t *a, const vec3_t *b, float r) {
	if (b->x == self->dest.x && b->y == self->dest.y && b->z == self->dest.z) {
		return AICC_Vtbl(self)->TestPathEnd(self, a, b, r);
	}
	return AICC3D_Corridor(self, a, b, 0);
}

byte __thiscall cAICharControl3D_TestPathEnd(cAICharControl3D *self, const vec3_t *a, const vec3_t *b, float r) {
	(void)r;
	return AICC3D_Corridor(self, a, b, 1);
}

byte __thiscall cAICharControl3D_TestSides(cAICharControl3D *self, const vec3_t *a, const vec3_t *b, float r) {
	cEntity *me = self->ent;
	cAICCSeg segs[8];
	vec3_t A0 = *a, Q0, n, m, ml;

	(void)r;
	float rad = Obj_Vtbl(me)->GetRadius(me);
	float len = AICC_X87Add(Obj_Vtbl(me)->GetRadius(me), 5.0f);
	AICC3D_Way(a, b, &n);
	m.x = -n.x;
	m.y = -n.y;
	m.z = -n.z;
	ml.x = m.x * len;
	ml.y = m.y * len;
	ml.z = m.z * len;
	Q0.x = a->x + ml.x;
	Q0.y = a->y + ml.y;
	Q0.z = a->z + ml.z;
	if (Engine_TestLine(&A0, &Q0, NULL, NULL)) {
		return 0;
	}
	if (!AICC3D_Lines(a, &m, rad, len, NULL, segs)) {
		return 0;
	}
	self->debugSegs[0].a = A0;
	self->debugSegs[0].b = Q0;
	memcpy(&self->debugSegs[1], segs, sizeof(segs));
	return 1;
}

byte __thiscall cAICharControl3D_TestStep(cAICharControl3D *self, const vec3_t *a, const vec3_t *b, float r) {
	cEntity *me = self->ent;
	cAICCSeg segs[8];
	vec3_t n;

	(void)r;
	float rad = Obj_Vtbl(me)->GetRadius(me);
	float t = AICC_X87Add(Obj_Vtbl(me)->GetRadius(me), 5.0f);
	float len = Float_MinSS(AICC3D_Way(a, b, &n), t);
	return AICC3D_Lines(a, &n, rad, len, NULL, segs) != 0;
}

byte __thiscall cAICharControl3D_SideBlocked(cAICharControl3D *self, int side) {
	vec3_t d, p, buf;

	if (side == 3) {
		d.x = 0.0f;
		d.y = 1.0f;
		d.z = 0.0f;
	} else if (side == 4) {
		d.x = 0.0f;
		d.y = -1.0f;
		d.z = 0.0f;
	} else {
		d = *AICC_Vtbl(self)->SideDir(self, &buf, side);
		if (d.x == 0.0f && d.y == 0.0f && d.z == 0.0f) {
			return 0;
		}
	}
	float r = AICC_X87Mul(Obj_Vtbl(self->ent)->GetRadius(self->ent), 1.5f);
	d.x = d.x * r;
	d.y = d.y * r;
	d.z = d.z * r;
	const vec3_t *s = AICC_Pos(self, &buf);
	p.y = s->y + d.y;
	p.z = s->z + d.z;
	p.x = s->x + d.x;
	float rr = Obj_Vtbl(self->ent)->GetRadius(self->ent);
	return AICC_Vtbl(self)->TestPath(self, AICC_Pos(self, &buf), &p, rr) == 0;
}

byte __thiscall cAICharControl3D_UpdateFollow(cAICharControl3D *self) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);
	cEntity *te = AICC_TargetEnt(self);
	vec3_t T, buf;
	float f;

	AICC_EntPos(te, &T);
	if (self->direct) {
		if (!cSlotController_Tick(AICC_Slot(&self->slotDirect), self->dt) ||
				!cSlotController_FireReset(AICC_Slot(&self->slotDirect), &f)) {
			return 1;
		}
		do {
			float d = AICC3D_DistTo(self, &T);
			self->stuck = 0;
			if (d > self->farDist) {
				float r = self->radius;
				if (!vt->TestPathEnd(self, AICC_Pos(self, &buf), &T, r)) {
					goto lost;
				}
			} else {
				if (Engine_TestLine(AICC_Pos(self, &buf), &T, NULL, NULL)) {
					goto lost;
				}
				float r = self->radius;
				if (!vt->TestPathEnd(self, AICC_Pos(self, &buf), &T, r)) {
					self->stuck = 1;
				}
			}
			vt->DirectPath(self, &T);
			continue;
		lost:
			self->direct = 0;
			if (!vt->FindPath(self, &T)) {
				return 0;
			}
			cSlotController_Restart(AICC_Slot(&self->slotNear));
			cSlotController_Restart(AICC_Slot(&self->slotSee));
			cSlotController_Restart(AICC_Slot(&self->slotPath));
		} while (cSlotController_FireReset(AICC_Slot(&self->slotDirect), &f));
		return 1;
	}
	if (cSlotController_Tick(AICC_Slot(&self->slotNear), self->dt) &&
			cSlotController_FireReset(AICC_Slot(&self->slotNear), &f)) {
		do {
			float k = 200.0f;
			const vec3_t *p = AICC_Pos(self, &buf);
			if ((double)k > __builtin_fabs((double)(p->x - self->dest.x)) &&
					(double)k > __builtin_fabs((double)(p->y - self->dest.y)) &&
					(double)k > __builtin_fabs((double)(p->z - self->dest.z))) {
				if (!vt->FindPath(self, &T)) {
					return 0;
				}
				cSlotController_Restart(AICC_Slot(&self->slotPath));
			}
		} while (cSlotController_FireReset(AICC_Slot(&self->slotNear), &f));
	}
	if (cSlotController_Tick(AICC_Slot(&self->slotSee), self->dt) &&
			cSlotController_FireReset(AICC_Slot(&self->slotSee), &f)) {
		do {
			float d = AICC3D_DistTo(self, &T);
			if (d > self->farDist) {
				float r = self->radius;
				if (!vt->TestPathEnd(self, AICC_Pos(self, &buf), &T, r)) {
					continue;
				}
			} else if (Engine_TestLine(AICC_Pos(self, &buf), &T, NULL, NULL)) {
				continue;
			}
			self->direct = 1;
			vt->DirectPath(self, &T);
			cSlotController_Restart(AICC_Slot(&self->slotDirect));
		} while (cSlotController_FireReset(AICC_Slot(&self->slotSee), &f));
	}
	if (cSlotController_Tick(AICC_Slot(&self->slotRepath), self->dt) &&
			cSlotController_FireReset(AICC_Slot(&self->slotRepath), &f)) {
		do {
			if (!vt->FindPath(self, &T)) {
				return 0;
			}
			cSlotController_Restart(AICC_Slot(&self->slotPath));
			cSlotController_Restart(AICC_Slot(&self->slotNear));
		} while (cSlotController_FireReset(AICC_Slot(&self->slotRepath), &f));
	}
	return 1;
}

void __thiscall cAICharControl3D_CheckArrived(cAICharControl3D *self) {
	vec3_t buf;
	cAIChar *ai;
	int ev;

	if (self->goal.x != self->dest.x || self->goal.y != self->dest.y || self->goal.z != self->dest.z) {
		return;
	}
	const vec3_t *p = AICC_Pos(self, &buf);
	float d = AICC3D_Len(self->dest.x - p->x, self->dest.y - p->y, self->dest.z - p->z);
	if (self->arriveDist > d) {
		ev = 2;
	} else if (self->arriveDist * 4.0f > d) {
		ev = 4;
	} else {
		goto none;
	}
	ai = self->ai;
	if (AICC_OwnerVtbl(ai)->Event(ai, ev)) {
		return;
	}
none:
	ai = self->ai;
	AICC_OwnerVtbl(ai)->Event(ai, 5);
}

void __thiscall cAICharControl3D_Turn(cAICharControl3D *self) {
	vec3_t view, ang, d, rot, buf;

	view = WChar(self->ent)->view;
	Vec3_NormAng(&ang, &view);
	if (self->avoidDir.x == 0.0f && self->avoidDir.y == 0.0f && self->avoidDir.z == 0.0f) {
		const vec3_t *s = AICC_Pos(self, &buf);
		d.x = s->x - self->goal.x;
		d.y = s->y - self->goal.y;
		d.z = s->z - self->goal.z;
	} else {
		d = self->avoidDir;
	}
	Vec3_GetRotAng(&rot, &d);
	float a = NormAng(ang.x - rot.x);
	float t = self->turnGain * a / M_PI_F;
	self->diffX = a;
	self->turnXNew = t;
	if (__builtin_fabs((double)(t - self->turnX)) > (double)self->turnStep) {
		self->turnXNew = t > self->turnX ? self->turnStep + self->turnX : self->turnX - self->turnStep;
	}
	self->turnX = self->turnXNew;
	self->ctrl->vRot = -self->turnXNew;
	a = NormAng(ang.y - rot.y);
	t = self->turnGain * a / M_PI_F;
	self->diffY = a;
	self->turnYNew = t;
	if (__builtin_fabs((double)(t - self->turnY)) > (double)self->turnStep) {
		self->turnYNew = t > self->turnY ? self->turnY + self->turnStep : self->turnY - self->turnStep;
	}
	self->turnY = self->turnYNew;
	self->ctrl->hRot = -self->turnYNew;
}

void __thiscall cAICharControl3D_Walk(cAICharControl3D *self) {
	cEntity *te = AICC_TargetEnt(self);
	vec3_t b1, b2;

	const vec3_t *p = AICC_Pos(self, &b1);
	const vec3_t *t = AICC_EntPos(te, &b2);
	float d = AICC3D_Len(t->x - p->x, t->y - p->y, t->z - p->z);
	switch (self->walkDir) {
	case 0:
		if (d > self->farDist) {
			self->walkDir = 1;
		} else if (self->nearDist > d) {
			self->walkDir = 2;
		}
		break;
	case 1:
		if ((self->nearDist + self->farDist) * 0.5f > d) {
			self->walkDir = 0;
		}
		break;
	case 2:
		if (d > (self->nearDist + self->farDist) * 0.5f) {
			self->walkDir = 0;
		}
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
	if (self->blocked || self->stuck) {
		self->ctrl->walk = 0.0f;
	}
}

byte __thiscall cAICharControl3D_FindPath(cAICharControl3D *self, const vec3_t *p) {
	vec3_t pos;
	byte noPath, reach;

	CHECK((g_NodesWorld != NULL) && (g_AStar != NULL));
	CHECK(g_NodesWorld->sup.vftable->IsCreated(g_NodesWorld));
	self->dest = *p;
	AICC_Pos(self, &pos);
	byte ok = CAStar_FindPath(g_AStar, &self->path, &self->dest, &pos, self->radius, self->arriveDist, self->soft,
		&noPath, &reach, &self->pathRes);
	if (!ok) {
		if (self->soft) {
			self->dest = *p;
			ok = CAStar_FindPath(g_AStar, &self->path, &self->dest, &pos, self->radius, self->arriveDist, 0,
				&noPath, &reach, &self->pathRes);
		}
		if (!ok) {
			if (!reach) {
				AICC_OwnerVtbl(self->ai)->Event(self->ai, 1);
				return 0;
			}
			if (!noPath) {
				AICC_OwnerVtbl(self->ai)->Event(self->ai, 0);
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

byte __thiscall cAICharControl3D_FindRoute(cAICharControl3D *self) {
	vec3_t pos;

	CHECK(g_NodesWorld != NULL);
	CHECK(g_NodesWorld->sup.vftable->IsCreated(g_NodesWorld));
	AICC_Pos(self, &pos);
	if (!CNodesWorld_BuildPath(g_NodesWorld, &self->path, g_DepthPathRouteInDoorAI, &self->routePoint, &pos,
			self->routeA, &self->routeNode, self->radius, self->routeB)) {
		AICC_OwnerVtbl(self->ai)->Event(self->ai, 0);
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
	AICC_Vtbl(self)->NextGoal(self);
	return 1;
}

float __thiscall cAICharControl3D_Dist(cAICharControl3D *self, const vec3_t *a, const vec3_t *b) {
	(void)self;
	return AICC3D_Len(a->x - b->x, a->y - b->y, a->z - b->z);
}

float __thiscall cAICharControl3D_DistSq(cAICharControl3D *self, const vec3_t *a, const vec3_t *b) {
	(void)self;
	float x = a->x - b->x;
	float y = a->y - b->y;
	float z = a->z - b->z;
	return y * y + x * x + z * z;
}

float __thiscall cAICharControl3D_KeepDist(cAICharControl3D *self, cEntity *hitEnt) {
	vec3_t b1, b2;

	CHECK(hitEnt != NULL);
	float er = WChar(hitEnt)->wl.prim.radius;
	float shirk = g_DistShirkCharAI;
	float t = AICC_X87AddMul(Obj_Vtbl(self->ent)->GetRadius(self->ent), er, shirk);
	er = WChar(hitEnt)->wl.prim.radius;
	float c = AICC_X87AddAdd(Obj_Vtbl(self->ent)->GetRadius(self->ent), er, t);
	float d = WChar(self->ent)->wl.prim.radius + WChar(hitEnt)->wl.prim.radius + t;
	float k = Float_MaxSS(c, d);
	const vec3_t *s = AICC_Pos(self, &b1);
	const vec3_t *p = AICC_EntPos(hitEnt, &b2);
	float dist = AICC3D_Len(p->x - s->x, p->y - s->y, p->z - s->z);
	return Float_MinSS(dist + 10.0f, k);
}

float __thiscall cAICharControl3D_StrafeDist(cAICharControl3D *self, cEntity *hitEnt) {
	CHECK(hitEnt != NULL);
	return AICC_X87AddAdd(WChar(self->ent)->wl.prim.radius, WChar(hitEnt)->wl.prim.radius, g_MarginCollCharAI);
}

/* the angle between the view (the axis fwd turned by it) and the way to e below ang */
static byte AICC3D_InCone(cAICharControl *self, cEntity *e, float ang, float fwd) {
	vec3_t sbuf, ebuf, dir, v, rot, o;

	const vec3_t *s = AICC_Pos(self, &sbuf);
	const vec3_t *p = AICC_EntPos(e, &ebuf);
	dir.x = p->x - s->x;
	dir.y = p->y - s->y;
	dir.z = p->z - s->z;
	v.x = 0.0f;
	v.y = 0.0f;
	v.z = fwd;
	rot = WChar(self->ent)->view;
	Vec3_Rotate(&o, &v, &rot);
	return (double)ang > Vec3_Angle(&o, &dir);
}

byte __thiscall cAICharControl3D_InFront(cAICharControl3D *self, cEntity *e, float ang) {
	return AICC3D_InCone(self, e, ang, 1.0f);
}

byte __thiscall cAICharControl3D_Behind(cAICharControl3D *self, cEntity *e, float ang) {
	return AICC3D_InCone(self, e, ang, -1.0f);
}

byte __thiscall cAICharControl3D_GoalAhead(cAICharControl3D *self) {
	vec3_t buf, a, b;

	const vec3_t *s = AICC_Pos(self, &buf);
	a.x = self->dest.x - s->x;
	a.y = self->dest.y - s->y;
	a.z = self->dest.z - s->z;
	const vec3_t *pp = self->pathPoint;
	b.x = self->dest.x - pp->x;
	b.y = self->dest.y - pp->y;
	b.z = self->dest.z - pp->z;
	return 0.39269909262657166 > Vec3_Angle(&a, &b);
}

byte __thiscall cAICharControl3D_OnFloor(cAICharControl3D *self, const vec3_t *p) {
	const vec3_t *list;
	int count = 0;

	float r = Obj_Vtbl(self->ent)->GetRadius(self->ent);
	CAStar_GetPointsVis(g_AStar, p, &list, 1, &count, r);
	return count != 0;
}

/* the original measures from the entity of the link node, not from e */
byte __thiscall cAICharControl3D_Near(cAICharControl3D *self, cEntity *e) {
	vec3_t b1, b2;

	cEntity *l = (cEntity *)self->link.target;
	const vec3_t *p = AICC_EntPos(l, &b1);
	const vec3_t *s = AICC_Pos(self, &b2);
	float d = AICC3D_Len(s->x - p->x, s->y - p->y, s->z - p->z);
	float k = AICC_X87Add(AICC_Vtbl(self)->KeepDist(self, e), 1.0f);
	return d > k;
}

byte __thiscall cAICharControl3D_InWay(cAICharControl3D *self, vec3_t dest, cEntity *hitEnt) {
	vec3_t buf;

	CHECK(hitEnt != NULL);
	const vec3_t *p = AICC_EntPos(hitEnt, &buf);
	float d = AICC3D_Len(dest.x - p->x, dest.y - p->y, dest.z - p->z);
	float k = AICC_Vtbl(self)->KeepDist(self, hitEnt);
	return k >= d;
}

void __thiscall cAICharControl3D_StepGoal(cAICharControl3D *self, vec3_t *p) {
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
		float ry = FxElem_Rand(range, base);
		float rz = FxElem_Rand(range, base);
		q.x = p->x + rx;
		q.y = p->y + ry;
		q.z = p->z + rz;
		if (Engine_TestSphere(&q, &q, self->radius, NULL, NULL)) {
			continue;
		}
		if (!CNodesWorld_TestPath(g_NodesWorld, &q, self->pathPoint, self->radius, self->radius, NULL, 0)) {
			continue;
		}
		if (!CNodesWorld_TestPath(g_NodesWorld, &q, next, self->radius, self->radius, NULL, 0)) {
			continue;
		}
		*p = q;
		break;
	}
	self->path.iter = save;
}

byte __thiscall cAICharControl3D_TryStrafe(cAICharControl3D *self, int side, cEntity *hitEnt, byte back, float keep,
	byte fwd, float sd, byte e2) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);
	vec3_t b1, b2, A, B, C, D, E, N, G;

	CHECK(hitEnt != NULL);
	const vec3_t *ep = AICC_EntPos(hitEnt, &b1);
	const vec3_t *sp = AICC_Pos(self, &b2);
	A.y = sp->y - ep->y;
	A.z = sp->z - ep->z;
	A.x = sp->x - ep->x;
	sp = AICC_Pos(self, &b1);
	G.x = self->goal.x - sp->x;
	G.y = self->goal.y - sp->y;
	G.z = self->goal.z - sp->z;
	float R = AICC3D_Len(A.x, A.y, A.z) + WChar(hitEnt)->wl.prim.radius;
	Vec3_NormalizeTo(&N, &G);
	float ox = N.x * R;
	float oy = N.y * R;
	float oz = N.z * R;
	sp = AICC_Pos(self, &b2);
	float px = sp->x + ox;
	float py = sp->y + oy;
	float pz = sp->z + oz;
	ep = AICC_EntPos(hitEnt, &b1);
	B.x = px - ep->x;
	B.y = py - ep->y;
	B.z = pz - ep->z;
	ep = AICC_EntPos(hitEnt, &b1);
	C.x = self->goal.x - ep->x;
	C.y = self->goal.y - ep->y;
	C.z = self->goal.z - ep->z;
	float lenB = AICC3D_Len(B.x, B.y, B.z);
	float lenC = AICC3D_Len(C.x, C.y, C.z);
	D = lenC > lenB ? B : C;
	E = *AICC_EntPos(hitEnt, &b2);
	byte left = side == 2;
	byte res = 1;
	if (fwd) {
		byte r = vt->StrafeFwd(self, &E, &A, &D, sd, left, e2);
		if (!r) {
			return r;
		}
	}
	if (back) {
		res = vt->StrafeBack(self, &E, &A, &D, keep, left, e2) ? res : 0;
	}
	return res;
}

/* ---- the strafe around a character ---- */

/* the inline normalize of the strafes: v itself for length 0 */
static inline vec3_t AICC3D_Unit(vec3_t v) {
	float len = AICC3D_Len(v.x, v.y, v.z);

	if (len == 0.0f) {
		return v;
	}
	float k = 1.0f / len;
	v.x = v.x * k;
	v.y = v.y * k;
	v.z = v.z * k;
	return v;
}

/* the axis the strafes turn around: the up of the way to the goal (the way crossed with its side) */
static vec3_t AICC3D_StrafeAxis(cAICharControl *self) {
	vec3_t buf;

	const vec3_t *p = AICC_Pos(self, &buf);
	vec3_t g;
	g.y = self->goal.y - p->y;
	g.x = self->goal.x - p->x;
	g.z = self->goal.z - p->z;
	vec3_t n = AICC3D_Unit(g);
	vec3_t s = AICC3D_Side(&n);
	s = AICC3D_Unit(s);
	vec3_t u;
	u.x = s.z * n.y - s.y * n.z;
	u.y = s.x * n.z - s.z * n.x;
	u.z = s.y * n.x - s.x * n.y;
	return AICC3D_Unit(u);
}

/* the point at dist from pos along a turned by m; 0 when the transform fails */
static byte AICC3D_ArcPoint(vec3_t *out, const vec3_t *pos, const vec3_t *a, const matrix_t *m, float dist) {
	vec3_t v;

	if (!Matrix_TransformProject(&v, a, m)) {
		return 0;
	}
	vec3_t n = AICC3D_Unit(v);
	out->x = n.x * dist + pos->x;
	out->y = n.y * dist + pos->y;
	out->z = n.z * dist + pos->z;
	return 1;
}

static byte AICC3D_ArcCorner(cAICharControl *self, const vec3_t *s, const vec3_t *a, const vec3_t *b, byte left) {
	cEntity *me = self->ent;

	float r2 = Obj_Vtbl(me)->GetRadius(me);
	float r = Obj_Vtbl(me)->GetRadius(me);
	return CNodesWorld_TestCorner(g_NodesWorld, s, a, b, r, r2, left, &self->debugPath);
}

static byte AICC3D_ArcPath(cAICharControl *self, const vec3_t *a, const vec3_t *b) {
	cEntity *me = self->ent;

	float r2 = Obj_Vtbl(me)->GetRadius(me);
	float r = Obj_Vtbl(me)->GetRadius(me);
	return CNodesWorld_TestPath(g_NodesWorld, a, b, r, r2, &self->debugPath, 0);
}

/* the walk around pos on the circle of radius dist from the way away to the way dir (the other way round without
 * flag), turned around the up of the way to the goal in steps of the radius of the character: each step and corner
 * free in the graph */
byte __thiscall cAICharControl3D_StrafeBack(cAICharControl3D *self, const vec3_t *pos, const vec3_t *away,
	const vec3_t *dir, float dist, byte left, byte flag) {
	cEntity *me = self->ent;
	vec3_t buf, S, prev, cur;
	matrix_t m;

	DListSeg_Clear(&self->debugPath);
	vec3_t a = AICC3D_Unit(*away);
	vec3_t d = AICC3D_Unit(*dir);
	float sign = left ? 1.0f : -1.0f;
	float ang = (float)Vec3_Angle(&a, &d);
	if (!flag) {
		ang = 6.2831855f - ang;
	}
	float step = Game_Atan2F(Obj_Vtbl(me)->GetRadius(me), dist);
	prev.x = a.x * dist + pos->x;
	prev.y = a.y * dist + pos->y;
	prev.z = a.z * dist + pos->z;
	if (g_ShowShirkAI == 1) {
		DListVec3_Add(&self->debugStrafe, AICC_Pos(self, &buf));
		DListVec3_Add(&self->debugStrafe, &prev);
	}
	float r2 = Obj_Vtbl(me)->GetRadius(me);
	float r = Obj_Vtbl(me)->GetRadius(me);
	if (!CNodesWorld_TestPath(g_NodesWorld, AICC_Pos(self, &buf), &prev, r, r2, &self->debugPath, 0)) {
		return 0;
	}
	AICC_Pos(self, &S);
	byte res = 1;
	vec3_t axis = AICC3D_StrafeAxis(self);
	float t = step;
	while (ang > t) {
		Matrix_RotAxis(&m, &axis, t * sign);
		if (!AICC3D_ArcPoint(&cur, pos, &a, &m, dist)) {
			return 0;
		}
		if (g_ShowShirkAI == 1) {
			DListVec3_Add(&self->debugStrafe, &cur);
		}
		if (!AICC3D_ArcCorner(self, &S, &prev, &cur, left) || !AICC3D_ArcPath(self, &prev, &cur)) {
			return 0;
		}
		S = prev;
		prev = cur;
		t = t + step;
	}
	Matrix_RotAxis(&m, &axis, ang * sign);
	if (!AICC3D_ArcPoint(&cur, pos, &a, &m, dist)) {
		return 0;
	}
	if (g_ShowShirkAI == 1) {
		DListVec3_Add(&self->debugStrafe, &cur);
	}
	/* both tests run */
	res = AICC3D_ArcCorner(self, &S, &prev, &cur, left) ? res : 0;
	res = AICC3D_ArcPath(self, &prev, &cur) ? res : 0;
	return res;
}

/* p free of the other characters */
static byte AICC3D_ArcFree(cAICharControl *self, const vec3_t *p) {
	cEntity *hit;

	float r = WChar(self->ent)->wl.prim.radius + 1.0f - g_MarginCollCharAI;
	return !cWorld_TestSphereEntities(g_pWorld, self->ent, (void *)0x10, p, r, &hit, 1, 0);
}

/* StrafeBack in half steps with only the other characters tested at each point of the circle */
byte __thiscall cAICharControl3D_StrafeFwd(cAICharControl3D *self, const vec3_t *pos, const vec3_t *away,
	const vec3_t *dir, float dist, byte left, byte flag) {
	cEntity *me = self->ent;
	vec3_t cur;
	matrix_t m;

	vec3_t a = AICC3D_Unit(*away);
	vec3_t d = AICC3D_Unit(*dir);
	float sign = left ? 1.0f : -1.0f;
	float ang = (float)Vec3_Angle(&a, &d);
	if (!flag) {
		ang = 6.2831855f - ang;
	}
	float step = (float)Game_Atan2(Obj_Vtbl(me)->GetRadius(me), dist) * 0.5f;
	cur.x = a.x * dist + pos->x;
	cur.y = a.y * dist + pos->y;
	cur.z = a.z * dist + pos->z;
	if (g_ShowShirkAI == 1) {
		DListVec3_Add(&self->unused_330, &cur);
	}
	if (!AICC3D_ArcFree(self, &cur)) {
		return 0;
	}
	vec3_t axis = AICC3D_StrafeAxis(self);
	float t = step;
	while (ang > t) {
		Matrix_RotAxis(&m, &axis, t * sign);
		if (!AICC3D_ArcPoint(&cur, pos, &a, &m, dist)) {
			return 0;
		}
		if (g_ShowShirkAI == 1) {
			DListVec3_Add(&self->debugStrafe, &cur);
		}
		if (!AICC3D_ArcFree(self, &cur)) {
			return 0;
		}
		t = t + step;
	}
	Matrix_RotAxis(&m, &axis, ang * sign);
	if (!AICC3D_ArcPoint(&cur, pos, &a, &m, dist)) {
		return 0;
	}
	if (g_ShowShirkAI == 1) {
		DListVec3_Add(&self->debugStrafe, &cur);
	}
	return AICC3D_ArcFree(self, &cur);
}
