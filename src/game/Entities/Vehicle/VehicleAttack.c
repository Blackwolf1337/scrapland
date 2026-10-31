/*
 * cVehicleAttack, the shots and the attacker of an entity with life, see include/scrap/vehicleattack.h.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/hashtable.h"
#include "scrap/net.h"
#include "scrap/objentity.h"
#include "scrap/vehicleattack.h"
#include "scrap/weapons.h"

#define VATT_MAX_NETCOUNT 15

cVehicleAttack *__fastcall cVehicleAttack_ctor(cVehicleAttack *self) {
	Nto1Node_Init(&self->target);
	Nto1Node_Init(&self->sent);
	Nto1Node_Init(&self->last);
	Nto1Node_Init(&self->hitBy);
	self->sent.data = self;
	self->target.data = self;
	self->last.data = self;
	self->point.x = 0.0f;
	self->point.y = 0.0f;
	self->point.z = 0.0f;
	self->lastPoint.x = 0.0f;
	self->lastPoint.y = 0.0f;
	self->lastPoint.z = 0.0f;
	self->owner = NULL;
	self->count = 0;
	self->netCount = 0;
	self->netSeq = 0;
	self->netFlags = 0;
	self->hasLast = 1;
	self->hitBy.data = self;
	self->hitTime = 0.0f;
	self->hitCount = 0;
	self->resetTime = g_VehicleAttackReset * -2.0f;
	return self;
}

void __fastcall cVehicleAttack_Expire(cVehicleAttack *self) {
	float t = Timer_GetTime(&g_pWorld->timer);

	if (t > self->resetTime) {
		CHECK(self->target.data);
		Nto1Node_Unlink(&self->target);
		self->count = 0;
		self->hasLast = 0;
		self->resetTime = g_VehicleAttackReset + t;
	}
}

const char *__thiscall cHitInfo_GetName(cVehicleAttack *info) {
	return Nto1Node_TargetName(&info->target);
}

/* the shots and the target into the network flags; 0 when nothing changed since the last flags */
static byte VAtt_NetFlags(cVehicleAttack *self) {
	if (self->netCount <= 0 && self->sent.target == self->target.target) {
		if (!self->target.target) {
			self->netFlags &= 0x7f;
		} else {
			self->netFlags |= 0x80;
		}
		return 0;
	}
	if (self->netCount > VATT_MAX_NETCOUNT) {
		self->netCount = VATT_MAX_NETCOUNT;
	}
	self->netSeq++;
	self->netFlags = (self->target.target ? 0x80 : 0) | (byte)((self->netSeq & 7) << 4) | (byte)self->netCount;
	self->netCount = 0;
	return 1;
}

byte __thiscall cVehicleAttack_NetChanged(cVehicleAttack *self, byte *out) {
	byte changed = VAtt_NetFlags(self);

	if (out) {
		*out = self->netFlags;
	}
	if (!changed) {
		return 0;
	}
	Nto1Node_LinkAs(&self->sent, &self->target);
	return 1;
}

vec3_t *__thiscall cVehicleAttack_GetPoint(cVehicleAttack *self) {
	return &self->point;
}

byte __thiscall cVehicleAttack_VarI(cVehicleAttack *self, const char *name, int *i, int set) {
	(void)self;
	(void)name;
	(void)i;
	(void)set;
	return 0;
}

void __thiscall cVehicleAttack_HitBy(cVehicleAttack *self, const char *name) {
	if (!strcmp(self->owner->name, name)) {
		CHECK(self->hitBy.data);
		Nto1Node_Unlink(&self->hitBy);
	} else {
		Nto1Node_LinkByName(&self->hitBy, name);
	}
	self->hitTime = Timer_GetTime(&((cObjEntity *)self->owner)->timer);
	if (!(g_pApp->flags & APPF_CLIENT)) {
		self->hitCount++;
	}
}

/* the first shot since the last OnHit starts the line there */
static inline void VAtt_Start(cVehicleAttack *self) {
	if (!self->hasLast) {
		self->lastPoint = self->point;
		Nto1Node_LinkAs(&self->last, &self->target);
		self->hasLast = 1;
	}
}

void __thiscall cVehicleAttack_ShotAt(cVehicleAttack *self, const char *name, int n) {
	vec3_t pos;

	if (!*name) {
		return;
	}
	HashTable *t = &g_pWorld->entities;
	cPtrItem *it = PtrTable_Find(t, name, HashTable_Slot(t, name));
	cEntity *e;
	if (!it || !(e = it->value)) {
		return;
	}
	if (!e->vftable->IsType(e, TYEN_WITH_LIFE)) {
		cVehicleAttack_ShotAtPoint(self, e->vftable->GetPos(e, &pos), n);
		return;
	}
	cEntity_AddLink(e, &self->target);
	cVehicleAttack_HitBy(&WLife(e)->attack, self->owner->name);
	self->count += n;
	self->netCount += n;
	self->point = *e->vftable->GetPos(e, &pos);
	VAtt_Start(self);
}

void __thiscall cVehicleAttack_ShotAtPoint(cVehicleAttack *self, const vec3_t *p, int n) {
	self->point.x = p->x;
	self->point.y = p->y;
	self->point.z = p->z;
	CHECK(self->target.data);
	Nto1Node_Unlink(&self->target);
	self->count += n;
	self->netCount += n;
	VAtt_Start(self);
}

void __thiscall cVehicleAttack_Update(cVehicleAttack *self) {
	cEntity *owner = self->owner;

	if (self->count <= 0 || !owner->vftable->IsType(owner, TYEN_VEHICLE) || !Veh(self->owner)->isInit) {
		return;
	}
	cPy_Function *f = &TABLE_AT(g_pWorld->weapons, Veh(self->owner)->weapons.curWeap)->onHit;
	for (int i = 1; i < self->count; i++) {
		int n = self->count;
		float a = (float)(n - i);
		float k = 1.0f / (float)n;
		float b = (float)i;
		float x = b * self->lastPoint.x * k + self->point.x * a * k;
		float y = self->lastPoint.y * b * k + self->point.y * a * k;
		float z = self->lastPoint.z * b * k + self->point.z * a * k;
		cPy_Function_AddParamS(f, self->owner->name);
		cPy_Function_AddParamF(f, x);
		cPy_Function_AddParamF(f, y);
		cPy_Function_AddParamF(f, z);
		cPy_Function_AddParamS(f, Nto1Node_TargetName(&self->last));
		cPy_Function_AddParamI(f, i);
		cPy_Function_Execute(f, 0);
	}
	cPy_Function_AddParamS(f, self->owner->name);
	cPy_Function_AddParamF(f, self->point.x);
	cPy_Function_AddParamF(f, self->point.y);
	cPy_Function_AddParamF(f, self->point.z);
	cPy_Function_AddParamS(f, Nto1Node_TargetName(&self->target));
	cPy_Function_AddParamI(f, 0);
	cPy_Function_Execute(f, 0);
	self->count = 0;
	self->lastPoint = self->point;
	Nto1Node_LinkAs(&self->last, &self->target);
	if ((g_pApp->flags & APPF_SERVER) && !Net_GetSendRate() && VAtt_NetFlags(self)) {
		Nto1Node_LinkAs(&self->sent, &self->target);
	}
}

byte __thiscall cVehicleAttack_VarS(cVehicleAttack *self, const char *name, char **s, int set) {
	if (strcmp(name, "Target")) {
		return 0;
	}
	if (!(byte)set) {
		*s = (char *)Nto1Node_TargetName(&self->target);
		return 1;
	}
	if (!**s) {
		Con_Print(0x8f008f, "WARNING: Vehicle.HiT_Target invalid value (empty string)\n", 0);
		return 1;
	}
	cVehicleAttack_ShotAt(self, *s, 0);
	return 1;
}

byte __thiscall cVehicleAttack_VarV(cVehicleAttack *self, const char *name, double *x, double *y, double *z, int set) {
	vec3_t p;

	if (strcmp(name, "Pos")) {
		return 0;
	}
	if (!(byte)set) {
		*x = self->point.x;
		*y = self->point.y;
		*z = self->point.z;
		return 1;
	}
	p.x = (float)*x;
	p.y = (float)*y;
	p.z = (float)*z;
	cVehicleAttack_ShotAtPoint(self, &p, 0);
	return 1;
}

int __thiscall cVehicleAttack_NetFlags(cVehicleAttack *self, byte flags, byte *changed) {
	*changed = flags != self->netFlags;
	self->netFlags = flags;
	if (*changed) {
		self->count = flags & 0xf;
	}
	return flags >> 7;
}
