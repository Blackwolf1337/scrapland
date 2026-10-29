/*
 * CAnimMat of the engine3d, original source Engine/ControlAnimMat.cpp (include/scrap/material.h): the frame counter
 * of an animated texture, stepped by the engine time once per drawn frame. CAnimMat_SetFrame is in
 * ScrapEng/ScrapEngModel.c.
 */
#include "scrap/e3dutil.h"
#include "scrap/global.h"
#include "scrap/material.h"
#include "scrap/mathlib.h"

static void AnimMat_Init(CAnimMat *self) {
	self->fps = 25.0f;
	self->frames = 0;
	self->cycle = -1;
	self->dt = 0.0f;
	self->mode = ANIMMAT_END;
	self->lastFrame = -1;
	self->frame = 0;
	self->framef = 0.0f;
}

CAnimMat *__thiscall CAnimMat_ctorCopy(CAnimMat *self, const CAnimMat *src) {
	AnimMat_Init(self);
	self->fps = src->fps;
	self->frames = src->frames;
	self->cycle = src->cycle;
	return self;
}

CAnimMat *__thiscall CAnimMat_ctor(CAnimMat *self) {
	AnimMat_Init(self);
	return self;
}

void __thiscall CAnimMat_dtor(CAnimMat *self) {
	(void)self;
}

/* rva 0x2580d0, the same without the return */
void __thiscall CAnimMat_Init(CAnimMat *self) {
	AnimMat_Init(self);
}

byte AnimMat_Set(CAnimMat *a, float fps, int cycle) {
	if (!a) {
		return 0;
	}
	a->cycle = cycle;
	a->fps = fps;
	return 1;
}

const char *AnimMat_ModeName(int mode) {
	switch (mode) {
	case ANIMMAT_REPR:
		return GAME_STR(str_3daf58, 0x3daf58);
	case ANIMMAT_PAUSE:
		return GAME_STR(str_3daf60, 0x3daf60);
	case ANIMMAT_FADE_PAUSE:
		return GAME_STR(str_3daf68, 0x3daf68);
	case ANIMMAT_END:
		return GAME_STR(str_3daf74, 0x3daf74);
	}
	CHECK(0);
	return NULL;
}

int __fastcall CAnimMat_GetFrame(CAnimMat *self) {
	if (g_FrameCount != self->lastFrame) {
		CAnimMat_Update(self);
		self->lastFrame = g_FrameCount;
	}
	return self->frame;
}

void __thiscall CAnimMat_Play(CAnimMat *self) {
	self->mode = ANIMMAT_REPR;
}

void __thiscall CAnimMat_SetFrameF(CAnimMat *self, float frame) {
	self->framef = frame;
	self->frame = (int)frame;
}

void __thiscall CAnimMat_Start(CAnimMat *self) {
	self->lastTime = g_TimeMs;
	self->dt = 0.0f;
	self->frame = 0;
	self->framef = 0.0f;
	self->mode = ANIMMAT_REPR;
}

void __thiscall CAnimMat_Stop(CAnimMat *self) {
	self->mode = ANIMMAT_END;
}

void __thiscall CAnimMat_Pause(CAnimMat *self) {
	self->mode = ANIMMAT_PAUSE;
}

void __thiscall CAnimMat_FadePause(CAnimMat *self, float ms) {
	if (self->mode != ANIMMAT_FADE_PAUSE) {
		return;
	}
	self->mode = ANIMMAT_FADE_PAUSE;
	self->fadeK = 1.0f;
	self->fadeRate = (float)(1000.0 / (double)ms);
}

void __fastcall CAnimMat_Update(CAnimMat *self) {
	int mode = self->mode;

	if (mode == ANIMMAT_END) {
		return;
	}
	uint32_t now = g_TimeMs;
	uint32_t ms = now - self->lastTime;
	self->lastTime = now;
	float dt = Float_MinSS(0.1f, (float)(double)ms * 0.001f);
	if (mode == ANIMMAT_FADE_PAUSE) {
		float k = Float_MaxSS(self->fadeK - self->fadeRate * dt, 0.2f);
		self->fadeK = k;
		dt = dt * k;
	}
	float step = 0.0f;
	if (mode == ANIMMAT_REPR) {
		step = self->fps * dt;
	}
	float f = step + self->framef;
	int frames = self->frames;
	self->dt = dt;
	self->framef = f;
	self->frame = (int)f;
	if (self->frame < frames) {
		return;
	}
	if (self->cycle >= 0) {
		f = (f - (float)frames) + (float)self->cycle;
		self->framef = f;
		self->frame = (int)f;
		return;
	}
	self->mode = ANIMMAT_END;
	self->frame = frames - 1;
	self->framef = (float)frames - 0.0001f;
}
