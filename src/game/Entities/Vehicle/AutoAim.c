/*
 * cAutoAim, the aim help of the vehicles, see include/scrap/autoaim.h.
 */
#include <string.h>

#include "scrap/autoaim.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/objentity.h"
#include "scrap/weapons.h"

#define AUTOAIM_NODIST 10.0f			/* dist without a target */

static inline float AA_Deg2Rad(double v) {
	return (float)((v * M_PI_D) / 180.0);
}

/* the drop of the target, the out of line instance of the Nto1Links.h unlink */
static inline void AA_Unlink(cAutoAim *self) {
	CHECK(self->target.data);
	Nto1Node_Unlink(&self->target);
}

cAutoAim *cAutoAim_ctor(cAutoAim *self) {
	Nto1Node_Init(&self->target);
	self->ax = 0.0f;
	self->ay = 0.0f;
	self->prevAx = 0.0f;
	self->prevAy = 0.0f;
	self->interval = 1.0f;
	self->time = 1.0f;
	self->target.data = self;
	self->speed = 1.5f;
	self->distLimit = 20000.0f;
	self->ang = 0.05f;
	float c = (float)Game_Cos((double)0.05f);
	self->status = AUTOAIM_OFF;
	self->dist = AUTOAIM_NODIST;
	self->loAng = 0.05f;
	self->hiAng = 0.05f;
	self->deltaAng = 0.0f;
	self->cosAng = c;
	self->on = 1;
	return self;
}

byte cAutoAim_VarF(cAutoAim *self, const char *name, double *d, int set) {
	float v;

	if (!(byte)set && !strcmp(name, "X")) {
		v = self->ax;
	} else if (!(byte)set && !strcmp(name, "Y")) {
		v = self->ay;
	} else if (!strcmp(name, "Speed")) {
		if ((byte)set) {
			self->speed = AA_Deg2Rad(*d);
			return 1;
		}
		v = self->speed;
	} else if (!strcmp(name, "DistLimit")) {
		if ((byte)set) {
			self->distLimit = (float)*d;
		} else {
			*d = self->distLimit;
		}
		return 1;
	} else if (!strcmp(name, "HiAng")) {
		if ((byte)set) {
			self->hiAng = AA_Deg2Rad(*d);
			return 1;
		}
		v = self->hiAng;
	} else if (!strcmp(name, "DeltaAng")) {
		if ((byte)set) {
			self->deltaAng = AA_Deg2Rad(*d);
			return 1;
		}
		v = self->deltaAng;
	} else if (!strcmp(name, "LoAng")) {
		if ((byte)set) {
			self->loAng = AA_Deg2Rad(*d);
			return 1;
		}
		v = self->loAng;
	} else if (!strcmp(name, "AngLimit")) {
		if ((byte)set) {
			float a = AA_Deg2Rad(*d);
			self->ang = a;
			self->cosAng = (float)Game_Cos((double)a);
			return 1;
		}
		v = self->ang;
	} else {
		return 0;
	}
	*d = ((double)v * 180.0) / M_PI_D;
	return 1;
}

void cAutoAim_SetSlot(cAutoAim *self, float interval, const float *ang) {
	self->prevAx = self->ax;
	self->prevAy = self->ay;
	self->interval = interval;
	self->time = Timer_GetTime(&g_pWorld->timer);
	if (ang) {
		self->ax = ang[0];
		self->ay = ang[1];
	}
}

vec3_t *cAutoAim_GetAimPos(cAutoAim *self, vec3_t *out, cEntity *owner) {
	vec3_t cp, ca, v, r1, r2, tp, q, e;
	cEntity *t;

	if ((self->status == AUTOAIM_SELECT || self->status == AUTOAIM_KEEP) && (t = self->target.target) &&
		t->vftable->IsType(t, TYEN_OBJECT)) {
		Obj_Vtbl(t)->GetViewPos(t, out);
		return out;
	}
	Obj_Vtbl(owner)->GetCamera(owner, &cp, &ca);
	float dt = Timer_GetTime(&g_pWorld->timer) - self->time;
	float f = dt / self->interval;
	if (self->status == AUTOAIM_MAGNET && (t = self->target.target) && t->vftable->IsType(t, TYEN_OBJECT)) {
		Obj_Vtbl(t)->GetViewPos(t, &tp);
		float dy = tp.y - cp.y;
		float dx = tp.x - cp.x;
		float dz = tp.z - cp.z;
		float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
		v.x = 0.0f;
		v.y = 0.0f;
		v.z = dist;
		const vec3_t *r = Vec3_RotateXY(&r1, &v, ca.x, ca.y);
		q.x = r->x + cp.x;
		q.y = r->y + cp.y;
		q.z = r->z + cp.z;
		e.x = tp.x - q.x;
		e.y = tp.y - q.y;
		e.z = tp.z - q.z;
		float l = (float)__builtin_sqrt((double)(e.y * e.y + e.x * e.x + e.z * e.z));
		float a = (float)Game_Atan2((double)l, (double)dist);
		float k = (self->ang - a) / self->ang;
		out->x = e.x * k + q.x;
		out->y = e.y * k + q.y;
		out->z = e.z * k + q.z;
		return out;
	}
	float m = Float_MinSS(1.0f, f);
	float g = 1.0f - m;
	v.x = 0.0f;
	v.y = 0.0f;
	v.z = self->dist;
	float ay = self->ay * m + self->prevAy * g;
	float ax = self->ax * m + self->prevAx * g;
	const vec3_t *r = Vec3_RotateXY(&r1, &v, ax, ay);
	r = Vec3_RotateXY(&r2, r, ca.x, ca.y);
	out->x = cp.x + r->x;
	out->y = r->y + cp.y;
	out->z = r->z + cp.z;
	return out;
}

void cAutoAim_Update(cAutoAim *self, cEntity *owner, float dt) {
	vec3_t cp, ca, pos, d, tmp;
	float dot, found;

	if (!self->on || WithLife_Vtbl(owner)->IsEMIed(owner)) {
		self->prevAx = self->ax;
		self->prevAy = self->ay;
		self->interval = dt;
		self->time = Timer_GetTime(&g_pWorld->timer);
		self->ax = 0.0f;
		self->ay = 0.0f;
		AA_Unlink(self);
		self->dist = AUTOAIM_NODIST;
		return;
	}
	if (self->deltaAng != 0.0f) {
		float a = Float_MinSS(self->hiAng, Float_MaxSS(self->loAng, self->deltaAng * dt + self->ang));
		self->ang = a;
		self->cosAng = (float)Game_Cos((double)a);
	}
	float a2 = self->ang * self->ang;
	self->prevAx = self->ax;
	self->prevAy = self->ay;
	self->interval = dt;
	float now = Timer_GetTime(&g_pWorld->timer);
	self->dist = AUTOAIM_NODIST;
	self->time = now;
	switch (self->status) {
	case AUTOAIM_OFF:
		self->ax = 0.0f;
		self->ay = 0.0f;
		AA_Unlink(self);
		return;
	case AUTOAIM_SEARCH:
	case AUTOAIM_SELECT:
	case AUTOAIM_MAGNET:
		break;
	default:
		return;
	}
	dot = -1.0f;
	cEntity *t = Entity_FindInView(owner, self->distLimit, &dot, &found, 1, self->cosAng);
	if (!t && !owner->vftable->GetAI(owner) && !g_pApp->flags) {
		t = Entity_FindInView(owner, self->distLimit, &dot, &found, 2, self->cosAng);
	}
	if (dot > self->cosAng) {
		cEntity_AddLink(t, &self->target);
	} else {
		t = NULL;
	}
	float rx = 0.0f, ry = 0.0f, d2 = 0.0f;
	if (!t) {
		AA_Unlink(self);
	} else {
		Obj_Vtbl(owner)->GetCamera(owner, &cp, &ca);
		const vec3_t *tp = t->vftable->GetPos(t, &pos);
		d.z = tp->z - cp.z;
		d.x = tp->x - cp.x;
		d.y = tp->y - cp.y;
		d = *Vec3_RotateInv(&tmp, &d, &ca);
		const vec3_t *ang = Vec3_GetRotAng(&tmp, &d);
		rx = ang->x;
		ry = ang->y;
		d2 = rx * rx + ry * ry;
		if (d2 > a2) {
			rx = 0.0f;
			ry = 0.0f;
			d2 = 0.0f;
			AA_Unlink(self);
		} else {
			self->dist = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z));
		}
	}
	if (self->status == AUTOAIM_MAGNET) {
		if (!(d2 > a2) && self->target.target) {
			float l = (float)__builtin_sqrt((double)d2);
			float k = (self->ang - l) / self->ang;
			self->prevAx = self->ax;
			self->prevAy = self->ay;
			self->ax = rx * k;
			self->ay = ry * k;
			self->dist = found;
			return;
		}
		self->ay = 0.0f;
		self->ax = 0.0f;
		self->prevAx = 0.0f;
		self->prevAy = self->ay;
		return;
	}
	if (self->status == AUTOAIM_SEARCH) {
		float dy = ry - self->ay;
		float dx = rx - self->ax;
		float l = (float)__builtin_sqrt((double)(dy * dy + dx * dx));
		float nx = dx, ny = dy;
		if (l != 0.0f) {
			float k = 1.0f / l;
			nx = dx * k;
			ny = dy * k;
		}
		float step = self->speed * dt;
		if (rx != 0.0f || ry != 0.0f) {
			float u = 1.0f - (self->ax * self->ax + self->ay * self->ay) / a2;
			float w = 1.0f - (ry * ry + rx * rx) / a2;
			float mx = Float_MaxSS(u, w);
			step = step * (mx * mx);
		}
		if (l > step) {
			self->ax = nx * step + self->ax;
			self->ay = self->ay + ny * step;
		} else {
			self->ax = rx;
			self->ay = ry;
		}
		return;
	}
	self->ay = ry;
	self->ax = rx;
	self->prevAx = rx;
	self->prevAy = self->ay;
}

void cAutoAim_SetAngle(cAutoAim *self, float ang) {
	self->ang = ang;
	self->cosAng = (float)Game_Cos((double)ang);
}

void cAutoAim_SetRange(cAutoAim *self, float range) {
	self->distLimit = range;
}

byte cAutoAim_VarS(cAutoAim *self, const char *name, char **s, int set) {
	if (!(byte)set && !strcmp(name, "Target")) {
		*s = (char *)Nto1Node_TargetName(&self->target);
		return 1;
	}
	if (strcmp(name, "Status")) {
		return 0;
	}
	if (!(byte)set) {
		switch (self->status) {
		case AUTOAIM_OFF:
			*s = "Off";
			break;
		case AUTOAIM_SEARCH:
			*s = "Search";
			break;
		case AUTOAIM_SELECT:
			*s = "Select";
			break;
		case AUTOAIM_KEEP:
			*s = "Keep";
			break;
		case AUTOAIM_MAGNET:
			*s = "Magnet";
			break;
		default:
			break;
		}
		return 1;
	}
	if (!crt_stricmp(*s, "Select")) {
		self->status = AUTOAIM_SELECT;
	} else if (!crt_stricmp(*s, "Off")) {
		self->status = AUTOAIM_OFF;
	} else if (!crt_stricmp(*s, "Keep")) {
		self->status = AUTOAIM_KEEP;
	} else if (!crt_stricmp(*s, "Magnet")) {
		self->status = AUTOAIM_MAGNET;
	} else if (!crt_stricmp(*s, "Search")) {
		self->status = AUTOAIM_SEARCH;
	} else {
		Con_Errorf("ERROR : '%s' invalid value for <Entity>.Can_Status \n", *s);
		return 0;
	}
	return 1;
}
