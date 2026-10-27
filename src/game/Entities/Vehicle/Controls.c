/*
 * The controls of a vehicle, see include/scrap/controls.h.
 */
#include <string.h>

#include "scrap/controls.h"
#include "scrap/mathlib.h"

#define CONTROLS_MIN_FADE 0.01f		/* rva 0x3b10d8 */

static inline cVehicleControls *Ctl(byte *p) {
	return (cVehicleControls *)p;
}

byte *__fastcall Controls_ctor(byte *self) {
	Controls_Reset(self);
	Ctl(self)->firePrev = 0;
	Ctl(self)->cmPrev = 0;
	return self;
}

void __fastcall Controls_Reset(byte *self) {
	cVehicleControls *c = Ctl(self);

	c->weapon = g_pWorld->firstWeapon;
	c->str = 0.0f;
	c->acc = 0.0f;
	c->vRot = 0.0f;
	c->hRot = 0.0f;
	c->vStr = 0.0f;
	c->cm = 0;
	c->b24 = 0;
	c->b25 = 1;
	c->b26 = 0;
	c->fire = 0;
	c->i30 = 0;
	c->netFire = 0;
	c->b28 = 0;
	c->i18 = 0;
	c->i1c = 0;
	c->rumbleA = 0.0f;
	c->rumbleB = 0.0f;
	c->rumbleARate = 0.0f;
	c->rumbleBRate = 0.0f;
	c->i2c = 0;
}

/* a float of the controls from the double variable */
static inline void Ctl_Var(float *f, double *d, int set) {
	if ((byte)set) {
		*f = (float)*d;
	} else {
		*d = *f;
	}
}

byte __thiscall Controls_VarF(byte *self, const char *name, double *d, int set) {
	cVehicleControls *c = Ctl(self);

	if (!strcmp(name, "Acc")) {
		Ctl_Var(&c->acc, d, set);
		return 1;
	}
	if (!strcmp(name, "VStr")) {
		Ctl_Var(&c->vStr, d, set);
		return 1;
	}
	if (!strcmp(name, "Str")) {
		Ctl_Var(&c->str, d, set);
		return 1;
	}
	if (!strcmp(name, "HRot")) {
		Ctl_Var(&c->hRot, d, set);
		return 1;
	}
	if (!strcmp(name, "VRot")) {
		Ctl_Var(&c->vRot, d, set);
		return 1;
	}
	return 0;
}

int __thiscall Controls_GetWeapon(byte *self) {
	return Ctl(self)->weapon;
}

void __thiscall Controls_FromNet(byte *self, cEntity *v, float dt) {
	cVehicleControls *c = Ctl(self);
	const byte *net = *(byte **)((byte *)v + 0xf4);
	uint16_t bits = *(const uint16_t *)(net + 0x74);
	byte toggle = (byte)(bits >> 15) & 1;

	(void)dt;
	if (c->netFire == toggle) {
		c->fire = (byte)bits & 1;
	} else {
		c->fire = 1;
		if (c->firePrev) {
			c->netFire = toggle;
		}
	}
	c->cm = ((byte)bits >> 1) & 1;
	if (bits & 0x100) {
		c->acc = 1.0f;
	} else if (bits & 0x200) {
		c->acc = -1.0f;
	} else {
		c->acc = 0.0f;
	}
	if (bits & 0x800) {
		c->str = 1.0f;
	} else if (bits & 0x400) {
		c->str = -1.0f;
	} else {
		c->str = 0.0f;
	}
}

void __thiscall Controls_SetWeapon(byte *self, int w) {
	cVehicleControls *c = Ctl(self);

	if (c->weapon != w) {
		c->weapon = w;
		c->fire = 0;
		c->cm = 0;
		c->i18 = 0;
	}
}

void __thiscall Controls_RumbleA(byte *self, float strength, float time) {
	float t = Float_MaxSS(CONTROLS_MIN_FADE, time);

	Ctl(self)->rumbleA = strength;
	Ctl(self)->rumbleARate = strength / t;
}

void __thiscall Controls_RumbleB(byte *self, float strength, float time) {
	float t = Float_MaxSS(CONTROLS_MIN_FADE, time);

	Ctl(self)->rumbleB = strength;
	Ctl(self)->rumbleBRate = strength / t;
}
