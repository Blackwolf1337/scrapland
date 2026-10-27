/*
 * ScrapEngLensFlares.cpp of the game: CGameLensFlare, the holder of one lens flare object (LensFlare, still the
 * exe's) for the effects of the scenes and weapons; the setters and getters go to the flare.
 */
#include "scrap/fxmisc.h"
#include "scrap/fxweapons.h"

#define LF(self) (*(LensFlare **)(self))

void *__thiscall CGameLensFlare_ctor(void *self) {
	void *mem;

	LF(self) = NULL;
	mem = operator_new(sizeof(LensFlare));
	LF(self) = mem ? LensFlare_ctor(mem) : NULL;
	CHECK(LF(self));
	return self;
}

void __thiscall CGameLensFlare_dtor(void *self) {
	LensFlare *lf = LF(self);

	if (lf) {
		LensFlare_dtor(lf);
		operator_delete_sized(lf, sizeof(*lf));
	}
}

void __thiscall CGameLensFlare_SetOn(void *self, byte on) {
	LensFlare_SetOn(LF(self), on);
}

float __thiscall CGameLensFlare_GetIntensity(const void *self) {
	return LF(self)->intensity;
}

float __thiscall CGameLensFlare_GetF30(const void *self) {
	return LF(self)->f30;
}

float __thiscall CGameLensFlare_GetF18(const void *self) {
	return LF(self)->f18;
}

int __thiscall CGameLensFlare_GetI1c(const void *self) {
	return LF(self)->i1c;
}

void __thiscall CGameLensFlare_SetIntensity(void *self, float v) {
	LensFlare_SetIntensity(LF(self), v);
}

void __thiscall CGameLensFlare_SetF30(void *self, float v) {
	LensFlare_SetF30(LF(self), v);
}

void __thiscall CGameLensFlare_SetPos(void *self, vec3_t pos) {
	LensFlare_SetPos(LF(self), pos);
}

void __thiscall CGameLensFlare_SetF18(void *self, float v) {
	LensFlare_SetF18(LF(self), v);
}

void __thiscall CGameLensFlare_SetI1c(void *self, int v) {
	LensFlare_SetI1c(LF(self), v);
}

void __thiscall CGameLensFlare_SetB2c(void *self, byte v) {
	LensFlare_SetB2c(LF(self), v);
}
