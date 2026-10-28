/*
 * ScrapEngLensFlares.cpp of the game: CGameLensFlare, the holder of one lens flare object (CLensFlare, still the
 * exe's) for the effects of the scenes and weapons; the setters and getters go to the flare.
 */
#include "scrap/fxmisc.h"
#include "scrap/fxweapons.h"
#include "scrap/lensflare.h"

#define LF(self) (*(CLensFlare **)(self))

void *__thiscall CGameLensFlare_ctor(void *self) {
	void *mem;

	LF(self) = NULL;
	mem = operator_new(sizeof(CLensFlare));
	LF(self) = mem ? CLensFlare_ctor(mem) : NULL;
	CHECK(LF(self));
	return self;
}

void __thiscall CGameLensFlare_dtor(void *self) {
	CLensFlare *lf = LF(self);

	if (lf) {
		CLensFlare_dtor(lf);
		operator_delete_sized(lf, sizeof(*lf));
	}
}

void __thiscall CGameLensFlare_SetOn(void *self, byte on) {
	CLensFlare_SetOn(LF(self), on);
}

float __thiscall CGameLensFlare_GetIntensity(const void *self) {
	return LF(self)->intens;
}

float __thiscall CGameLensFlare_GetF30(const void *self) {
	return LF(self)->traceInterval;
}

float __thiscall CGameLensFlare_GetF18(const void *self) {
	return LF(self)->escala;
}

int __thiscall CGameLensFlare_GetI1c(const void *self) {
	return LF(self)->tipo;
}

void __thiscall CGameLensFlare_SetIntensity(void *self, float v) {
	CLensFlare_SetIntens(LF(self), v);
}

void __thiscall CGameLensFlare_SetF30(void *self, float v) {
	CLensFlare_SetTraceInterval(LF(self), v);
}

void __thiscall CGameLensFlare_SetPos(void *self, vec3_t pos) {
	CLensFlare_SetPos(LF(self), pos);
}

void __thiscall CGameLensFlare_SetF18(void *self, float v) {
	CLensFlare_SetEscala(LF(self), v);
}

void __thiscall CGameLensFlare_SetI1c(void *self, int v) {
	CLensFlare_SetTipo(LF(self), v);
}

void __thiscall CGameLensFlare_SetB2c(void *self, byte v) {
	CLensFlare_SetRelativo(LF(self), v);
}
