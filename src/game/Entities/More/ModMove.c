/*
 * Entities/More/ModMove.cpp: cObjMove and cModMove, the movers of the "Obj" entities, see include/scrap/modmove.h.
 */
#include <string.h>

#include "scrap/crt.h"
#include "scrap/hashtable.h"
#include "scrap/objentity.h"

#define MODMOVE_WALKCHAR_SETANG 0x10c	/* the vtable slot of a character that takes the angles of its model */

static inline cObjEntity *ModMove_Obj(cEntity *e) {
	return (cObjEntity *)e;
}

cModMove *cModMove_ctor(cModMove *self) {
	self->head.target = NULL;
	self->head.data = NULL;
	self->head.next = &self->head;
	self->head.prev = &self->head;
	self->link.target = NULL;
	self->link.data = NULL;
	self->link.next = &self->link;
	self->link.prev = &self->link;
	self->offPos.x = 0.0f;
	self->offPos.y = 0.0f;
	self->offPos.z = 0.0f;
	self->offMul = 0.0f;
	self->deltaOffMul = 0.0f;
	self->parent = NULL;
	self->useLinkAng = 0;
	self->offPosAsPos = 0;
	return self;
}

void cModMove_dtor(cModMove *self) {
	Nto1Node_Done(&self->link);
	Nto1Node_Done(&self->head);
}

byte cModMove_VarF(cModMove *self, const char *name, double *d, int set) {
	if (!strcmp(name, "OffMul")) {
		if ((byte)set) {
			self->offMul = (float)*d;
		} else {
			*d = self->offMul;
		}
		return 1;
	}
	if (!strcmp(name, "DeltaOffMul")) {
		if ((byte)set) {
			self->deltaOffMul = (float)*d;
		} else {
			*d = self->deltaOffMul;
		}
		return 1;
	}
	return 0;
}

byte cModMove_VarI(cModMove *self, const char *name, int *i, int set) {
	if (!strcmp(name, "OffModPosAsPos")) {
		if ((byte)set) {
			self->offPosAsPos = *i != 0;
		} else {
			*i = self->offPosAsPos != 0;
		}
		return 1;
	}
	if (!strcmp(name, "UseLinkAng")) {
		if ((byte)set) {
			self->useLinkAng = *i != 0;
		} else {
			*i = self->useLinkAng != 0;
		}
		return 1;
	}
	return 0;
}

void cModMove_Link(cModMove *self, const char *name, const char *node) {
	if (!*name) {
		CHECK(self->link.data);
		Nto1Node_Unlink(&self->link);
		self->nodeIndex = -1;
		return;
	}
	HashTable *t = &g_pWorld->entities;
	cPtrItem *it = PtrTable_Find(t, name, HashTable_Slot(t, name));
	cEntity *link = NULL;
	if (!it || !(link = it->value)) {
		CHECK(!"Link");
	}
	if (!link->vftable->IsType(link, TYEN_OBJECT)) {
		CHECK(!"Link->IsType(TYEN_OBJECT)");
	}
	CGameModel *mod = Obj_Vtbl(link)->GetModel(link);
	CHECK(mod);
	CHECK(CGameModel_IsLoaded(mod));
	self->nodeIndex = CGameModel_FindRenderNode(mod, node);
	CHECK(self->nodeIndex != -1);
	Nto1Node_LinkTo(&ModMove_Obj(link)->mod.head, &self->link);
}

void cModMove_Update(cModMove *self, float dt) {
	cObjEntity *p = ModMove_Obj(self->parent);
	vec3_t to, a, e;
	matrix_t mat;

	if (self->offPosAsPos) {
		p->renderPos = self->offPos;
		return;
	}
	float mul = self->deltaOffMul * dt + self->offMul;
	self->offMul = mul;
	if (0.0f >= mul) {
		self->offMul = 0.0f;
		return;
	}
	cEntity *link = self->link.target;
	if (!link) {
		self->offMul = 0.0f;
		return;
	}
	if (CGameModel_IsLoaded(Obj_Vtbl(link)->GetModel(link))) {
		uint32_t slot = (uint32_t)Obj_Vtbl(link)->GetNodeSlot(link);
		CGameModel_NodeMatrix(Obj_Vtbl(link)->GetModel(link), &mat, (uint32_t)self->nodeIndex, 1, NULL, slot);
		to.z = self->offPos.z + mat.m[14];
		to.x = self->offPos.x + mat.m[12];
		to.y = self->offPos.y + mat.m[13];
		a = *Matrix_GetEuler(&mat, &e);
	} else {
		to.x = self->offPos.x + p->renderPos.x;
		to.y = p->renderPos.y + self->offPos.y;
		to.z = p->renderPos.z + self->offPos.z;
		a = p->renderAng;
	}
	mul = self->offMul;
	if (mul > 1.0f) {
		self->offMul = 1.0f;
		p->renderPos = to;
		if (!self->useLinkAng) {
			return;
		}
		p->renderAng = a;
	} else {
		float g = 1.0f - mul;
		p->renderPos.x = to.x * mul + p->base.pos.x * g;
		p->renderPos.y = to.y * mul + p->base.pos.y * g;
		p->renderPos.z = to.z * mul + p->base.pos.z * g;
		if (!self->useLinkAng) {
			return;
		}
		vec3_t d;
		d.x = a.x - p->ang.x;
		d.y = a.y - p->ang.y;
		d.z = a.z - p->ang.z;
		float m = self->offMul;
		const vec3_t *n = Vec3_NormAng(&e, &d);
		p->renderAng.x = m * n->x + p->ang.x;
		p->renderAng.z = p->ang.z + n->z * m;
		p->renderAng.y = p->ang.y + n->y * m;
	}
	if (self->parent->vftable->IsType(self->parent, TYEN_WALKCHAR)) {
		(*(void(**)(cEntity *, const vec3_t *))((const byte *)self->parent->vftable +
			MODMOVE_WALKCHAR_SETANG))(self->parent, &p->renderAng);
	}
}

void cModMove_SetParent(cModMove *self, cEntity *parent) {
	CHECK(self->parent == NULL);
	self->parent = parent;
	self->head.target = parent;
	self->link.data = parent;
	self->nodeIndex = -1;
}

byte cModMove_VarS(cModMove *self, const char *name, char **s, int set) {
	char buf[0x80];

	if (!(byte)set || strcmp(name, "LinkModel")) {
		return 0;
	}
	/* "object,node" */
	const char *comma = strchr(*s, ',');
	uint32_t n = (uint32_t)(comma - *s);
	if (n >= sizeof(buf)) {
		crt_report_rangecheckfailure();
	}
	memcpy(buf, *s, n);
	buf[n] = 0;
	cModMove_Link(self, buf, comma + 1);
	return 1;
}

byte cModMove_VarV(cModMove *self, const char *name, double *x, double *y, double *z, int set) {
	if (strcmp(name, "OffModPos")) {
		return 0;
	}
	if ((byte)set) {
		self->offPos.x = (float)*x;
		self->offPos.y = (float)*y;
		self->offPos.z = (float)*z;
	} else {
		*x = self->offPos.x;
		*y = self->offPos.y;
		*z = self->offPos.z;
	}
	return 1;
}

cObjMove *cObjMove_ctor(cObjMove *self) {
	cPy_Function_ctor(&self->onEnd);
	memset(self, 0, offsetof(cObjMove, time));
	self->moveAng.x = 0.0f;
	self->moveAng.y = 0.0f;
	self->moveAng.z = 0.0f;
	self->movePos = self->moveAng;
	self->moveTime = -1.0f;
	self->time = -1.0f;
	self->active = 0;
	self->viewAng = 1;
	self->fade = 0;
	return self;
}

void cObjMove_dtor(cObjMove *self) {
	cPy_Function_dtor(&self->onEnd);
}

void cObjMove_Tick(cObjMove *self, cEntity *e, float dt) {
	cObjEntity *o = ModMove_Obj(e);
	vec3_t d, tmp;

	if (!self->active) {
		return;
	}
	float t = self->time;
	if (t >= self->moveTime) {
		e->pos = self->movePos;
		memset(&o->vel, 0, sizeof(o->vel));
		o->ang = self->moveAng;
		memset(&o->rotVel, 0, sizeof(o->rotVel));
		return;
	}
	if (0.0f > t) {
		self->time = 0.0f;
		self->startPos = e->pos;
		self->startAng = o->ang;
		self->lastPos.x = e->pos.x - o->vel.x * dt;
		self->lastPos.y = e->pos.y - o->vel.y * dt;
		self->lastPos.z = e->pos.z - o->vel.z * dt;
		self->lastAng.x = o->ang.x - o->rotVel.x * dt;
		self->lastAng.y = o->ang.y - o->rotVel.y * dt;
		self->lastAng.z = o->ang.z - o->rotVel.z * dt;
	} else {
		self->time = t + dt;
	}
	float mt = self->moveTime;
	byte done = self->time >= mt;
	if (done) {
		self->time = mt;
	}
	float f = self->time / mt;
	switch (self->fade) {
	case OBJMOVE_EASEIN:
		f = f * f * (2.0f - f);
		break;
	case OBJMOVE_EASEOUT: {
		float g = 1.0f - f;
		f = 1.0f - g * g * (2.0f - g);
		break;
	}
	case OBJMOVE_SMOOTH:
		f = f * f * (3.0f - (f + f));
		break;
	default:
		break;
	}
	float g = 1.0f - f;
	float pz = self->startPos.z * g + self->movePos.z * f;
	float px = self->startPos.x * g + self->movePos.x * f;
	float py = self->startPos.y * g + self->movePos.y * f;
	e->pos.z = pz;
	e->pos.x = px;
	e->pos.y = py;
	float inv = 1.0f / dt;
	o->vel.x = (self->lastPos.x - px) * inv;
	o->vel.z = (self->lastPos.z - pz) * inv;
	o->vel.y = (self->lastPos.y - py) * inv;
	d.x = self->moveAng.x - self->startAng.x;
	d.y = self->moveAng.y - self->startAng.y;
	d.z = self->moveAng.z - self->startAng.z;
	const vec3_t *n = Vec3_NormAng(&tmp, &d);
	float ax = self->startAng.x + n->x * f;
	float az = self->startAng.z + n->z * f;
	float ay = self->startAng.y + n->y * f;
	o->ang.x = ax;
	o->ang.z = az;
	o->ang.y = ay;
	d.x = self->lastAng.x - ax;
	d.y = self->lastAng.y - ay;
	d.z = self->lastAng.z - o->ang.z;
	n = Vec3_NormAng(&tmp, &d);
	o->rotVel.x = n->x * inv;
	o->rotVel.y = n->y * inv;
	o->rotVel.z = n->z * inv;
	self->lastPos = e->pos;
	self->lastAng = o->ang;
	if (done) {
		cPy_Function_AddParamS(&self->onEnd, e->name);
		cPy_Function_Execute(&self->onEnd, 1);
		self->fade = 0;
	}
}

void cObjMove_SetTime(cObjMove *self, float t) {
	byte on = t > 0.0f;

	self->active = on;
	if (on) {
		self->moveTime = t;
		self->time = -1.0f;
	}
}
