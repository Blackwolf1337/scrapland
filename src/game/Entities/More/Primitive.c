/*
 * Entities/More/Primitive.cpp: cPrimitive, the collision sphere of the entities with life, see
 * include/scrap/primitive.h.
 */
#include <string.h>

#include "scrap/arrowindicator.h"
#include "scrap/console.h"
#include "scrap/primitive.h"
#include "scrap/scrapcollision.h"
#include "scrap/world.h"

/* the entity callback of the sphere test: other touched self at point */

cPrimitive *__fastcall cPrimitive_ctor(cPrimitive *self) {
	self->on = 0;
	self->owner = NULL;
	self->radius = 0.0f;
	return self;
}

byte __thiscall cPrimitive_VarF(cPrimitive *self, const char *name, double *d, int set) {
	if (!*name || strcmp(name, "VRadius")) {
		return 0;
	}
	if ((byte)set) {
		cPrimitive_SetSize(self, (float)*d);
	} else {
		*d = self->radius;
	}
	return 1;
}

byte __thiscall cPrimitive_TestSegment(cPrimitive *self, const vec3_t *a, vec3_t *p) {
	if (self->on && self->radius > 0.0f) {
		return cPrimitive_SegHit(self, a, p);
	}
	return 0;
}

byte __thiscall cPrimitive_SegHit(cPrimitive *self, const vec3_t *a, vec3_t *p) {
	vec3_t hit, pos;

	if (!self->on) {
		return 0;
	}
	hit = *p;
	const vec3_t *c = self->owner->vftable->GetPos(self->owner, &pos);
	if (!Collision_SegSphere(a, p, c->x, c->y, c->z, self->radius, &hit)) {
		return 0;
	}
	*p = hit;
	return 1;
}

void __thiscall cPrimitive_SetOn(cPrimitive *self, byte on) {
	self->on = self->radius > 0.0f ? on : 0;
}

void __thiscall cPrimitive_SetOwner(cPrimitive *self, cEntity *owner) {
	self->owner = owner;
}

void __thiscall cPrimitive_SetSize(cPrimitive *self, float size) {
	CHECK(self->owner != NULL);
	CHECK(size >= 0.0f);
	if (size == 0.0f) {
		self->radius = 0.0f;
		self->on = 0;
		return;
	}
	self->radius = size;
	self->on = 1;
}

byte __thiscall cPrimitive_TestSphere(cPrimitive *self, cEntity *other, const vec3_t *c, float r, const vec3_t *prev) {
	if (self->on && self->radius > 0.0f) {
		return cPrimitive_SphereHit(self, other, c, r, prev);
	}
	return 0;
}

byte __thiscall cPrimitive_SphereHit(cPrimitive *self, cEntity *other, const vec3_t *c, float r, const vec3_t *prev) {
	vec3_t pos, n, q;
	float d2;

	if (!self->on) {
		return 0;
	}
	const vec3_t *p = self->owner->vftable->GetPos(self->owner, &pos);
	if (prev) {
		d2 = Dist2_PointSegment(c, prev, p, NULL);
	} else {
		float dy = c->y - p->y;
		float dx = c->x - p->x;
		float dz = c->z - p->z;
		d2 = dy * dy + dx * dx + dz * dz;
	}
	float rr = self->radius + r;
	if (!(rr * rr >= d2)) {
		return 0;
	}
	if (!g_pWorld->testing && other) {
		p = self->owner->vftable->GetPos(self->owner, &pos);
		float dx = c->x - p->x;
		float dy = c->y - p->y;
		float dz = c->z - p->z;
		float l2 = dx * dx + dy * dy + dz * dz;
		if (1.0f >= l2) {
			n.x = 0.0f;
			n.y = 1.0f;
			n.z = 0.0f;
		} else {
			float k = 1.0f / (float)__builtin_sqrt((double)l2);
			n.x = dx * k;
			n.y = dy * k;
			n.z = dz * k;
		}
		float rad = self->radius;
		float ox = n.x * rad;
		float oy = n.y * rad;
		float oz = n.z * rad;
		p = self->owner->vftable->GetPos(self->owner, &pos);
		q.x = p->x + ox;
		q.y = p->y + oy;
		q.z = p->z + oz;
		other->vftable->Touch(other, self->owner, &q, &n);
	}
	return 1;
}

void __thiscall cPrimitive_UnelasticDisp(cPrimitive *self, vec3_t *out, const vec3_t *from, const cPrimitive *other,
	byte xz) {
	float v[2], n[2];

	if (!self->on) {
		return;
	}
	if (!(self->radius > 0.0f) || !(other->radius > 0.0f)) {
		Con_Print(0x8f008f, "UnelasticDisp not implemented if not sphere!\n", 0);
		return;
	}
	float d = other->radius + self->radius + 1.0f;
	float fx = from->x, fy = from->y, fz = from->z;
	float dx = out->x - fx;
	float dy = out->y - fy;
	float dz = out->z - fz;
	if (!xz) {
		float l = (float)__builtin_sqrt((double)(dx * dx + dy * dy + dz * dz));
		if (l == 0.0f) {
			out->y = fy + 0.0f;
			out->x = fx + d;
			out->z = fz + 0.0f;
			return;
		}
		float k = d / l;
		out->x = k * dx + fx;
		out->y = k * dy + fy;
		out->z = k * dz + fz;
		return;
	}
	float t = d * d - dy * dy;
	if (0.0f > t) {
		Con_Print(0x606060, "UnelasticDisp() : The XZ displacement is impossible.\n", 0);
		return;
	}
	float s = (float)__builtin_sqrt((double)t);
	v[0] = dx;
	v[1] = dz;
	const float *r = Vec2_Normalize(n, v);
	out->x = fx + r[0] * s;
	out->y = fy + dy;
	out->z = fz + r[1] * s;
}
