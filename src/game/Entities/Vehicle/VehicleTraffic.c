/*
 * Entities/Vehicle/VehicleTraffic.cpp of the game: cVehicleTraffic, see include/scrap/vehicletraffic.h.
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/crt.h"
#include "scrap/vehicletraffic.h"
#include "scrap/staticinit.h"

/* the type names of the exe */
#define VT_HEAVY "Heavy"
#define VT_MEDIUM "Medium"
#define VT_LIGHT "Light"
#define VT_ERROR "[Error]"

cVehicleTraffic *cVehicleTraffic_ctor(cVehicleTraffic *self) {
	self->begin.y = 0.0f;
	self->begin.z = 0.0f;
	self->begin.x = 0.0f;
	self->end.x = 0.0f;
	self->end.y = self->begin.y;
	self->end.z = self->begin.z;
	self->parent = NULL;
	self->active = 0;
	self->outside = 1;
	self->speed = g_TrafficSpeed;
	self->type = 0;
	self->off = 0;
	return self;
}

byte cVehicleTraffic_CollideEntities(cVehicleTraffic *self) {
	if (!self->active) {
		return 1;
	}
	void *ai = self->parent->vftable->GetAI(self->parent);
	if (ai && cAIVehicle_GetState(ai) != 4 && cAIVehicle_GetState(ai) != 9) {
		return 1;
	}
	return g_UseTrafficCollisionSkip == 0;
}

byte cVehicleTraffic_VarF(cVehicleTraffic *self, const char *name, double *v, byte set) {
	if (strcmp(name, "Speed")) {
		return 0;
	}
	if (set) {
		self->speed = (float)*v;
	} else {
		*v = (double)self->speed;
	}
	return 1;
}

void cVehicleTraffic_SetOff(cVehicleTraffic *self, byte off) {
	self->off = off;
}

void cVehicleTraffic_SetParent(cVehicleTraffic *self, cEntity *parent) {
	CHECK(self->parent == NULL);
	self->parent = parent;
}

byte cVehicleTraffic_VarI(cVehicleTraffic *self, const char *name, int *v, byte set) {
	if (strcmp(name, "Active")) {
		return 0;
	}
	if (set) {
		self->active = *v != 0;
	} else {
		*v = self->active != 0;
	}
	return 1;
}

byte cVehicleTraffic_Move(cVehicleTraffic *self, const vec3_t *from, const vec3_t *to, float radius,
	vec3_t *out, float len, CSphereBox *cache) {
	byte cut = 0;

	if (self->off) {
		*out = *to;
		return 0;
	}
	self->outside = 0;
	if (self->active) {
		void *ai = self->parent->vftable->GetAI(self->parent);
		float r = radius;
		if (ai && cAIVehicle_GetState(ai) == 4) {
			r = (float)((cAIVehicle_GetPathWidth(ai)) + radius);
		}
		if (r == radius) {
			switch (self->type) {
			case 0:
				r += g_NTrafficCapsule;
				break;
			case 1:
				r += g_MTrafficCapsule;
				break;
			case 2:
				r += g_HTrafficCapsule;
				break;
			}
		}
		float r2 = r * r;
		float speed = self->speed;
		cut = len > speed;
		if (cut) {
			float k = speed / len;
			out->y = (to->y - from->y) * k + from->y;
			out->x = k * (to->x - from->x) + from->x;
			out->z = (to->z - from->z) * k + from->z;
		} else {
			*out = *to;
		}
		if (!(Dist2_PointSegment(&self->begin, &self->end, to, NULL) > r2)) {
			return cut;
		}
	}
	self->outside = 1;
	if (!cut) {
		return Collision_Slide(from, to, radius, out, cache);
	}
	vec3_t p = *out;
	Collision_Slide(from, &p, radius, out, cache);
	return cut;
}

byte cVehicleTraffic_VarS(cVehicleTraffic *self, const char *name, char **v, byte set) {
	if (strcmp(name, "Type")) {
		return 0;
	}
	if (!set) {
		switch (self->type) {
		case 0:
			*v = VT_LIGHT;
			break;
		case 1:
			*v = VT_MEDIUM;
			break;
		case 2:
			*v = VT_HEAVY;
			break;
		default:
			*v = VT_ERROR;
			break;
		}
		return 1;
	}
	if (!crt_stricmp(VT_HEAVY, *v)) {
		self->type = 2;
	} else if (!crt_stricmp(VT_MEDIUM, *v)) {
		self->type = 1;
	} else if (!crt_stricmp(VT_LIGHT, *v)) {
		self->type = 0;
	} else {
		return 0;
	}
	return 1;
}

static byte VT_Vec(vec3_t *p, double *x, double *y, double *z, byte set) {
	if (set) {
		double vy = *y, vz = *z;
		p->x = (float)*x;
		p->y = (float)vy;
		p->z = (float)vz;
	} else {
		*x = (double)p->x;
		*y = (double)p->y;
		*z = (double)p->z;
	}
	return 1;
}

byte cVehicleTraffic_VarV(cVehicleTraffic *self, const char *name, double *x, double *y, double *z,
	byte set) {
	if (!strcmp(name, "Begin")) {
		return VT_Vec(&self->begin, x, y, z, set);
	}
	if (!strcmp(name, "End")) {
		return VT_Vec(&self->end, x, y, z, set);
	}
	return 0;
}

cGVar GVar_UseTrafficCollisionSkip;

cGVar GVar_TrafficSpeed;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_TrafficSpeed, &GVar_TrafficSpeed, "TrafficSpeed", 0, "Terminal speed for the traffic", 5600.0f)
INIT_GVAR_INT(Init_GVar_UseTrafficCollisionSkip, &GVar_UseTrafficCollisionSkip, "UseTrafficCollisionSkip", 0, "1 if uses 0 if not", 1)
