/*
 * ScrapEngLensFlares.cpp of the game: CGameLensFlare, the holder of one lens flare object (CLensFlare, still the
 * exe's) for the effects of the scenes and weapons; the setters and getters go to the flare.
 */
#include "scrap/fxmisc.h"
#include "scrap/fxweapons.h"
#include "scrap/lensflare.h"

#define LF(self) (*(CLensFlare **)(self))

void *CGameLensFlare_ctor(void *self) {
	void *mem;

	LF(self) = NULL;
	mem = operator_new(sizeof(CLensFlare));
	LF(self) = mem ? CLensFlare_ctor(mem) : NULL;
	CHECK(LF(self));
	return self;
}

void CGameLensFlare_dtor(void *self) {
	CLensFlare *lf = LF(self);

	if (lf) {
		CLensFlare_dtor(lf);
		operator_delete(lf);
	}
}

void CGameLensFlare_SetOn(void *self, byte on) {
	CLensFlare_SetOn(LF(self), on);
}

void CGameLensFlare_SetIntensity(void *self, float v) {
	CLensFlare_SetIntens(LF(self), v);
}

void CGameLensFlare_SetTraceInterval(void *self, float v) {
	CLensFlare_SetTraceInterval(LF(self), v);
}

void CGameLensFlare_SetPos(void *self, vec3_t pos) {
	CLensFlare_SetPos(LF(self), pos);
}

void CGameLensFlare_SetScale(void *self, float v) {
	CLensFlare_SetScale(LF(self), v);
}

void CGameLensFlare_SetType(void *self, int v) {
	CLensFlare_SetType(LF(self), v);
}

void CGameLensFlare_SetRelative(void *self, byte v) {
	CLensFlare_SetRelative(LF(self), v);
}
