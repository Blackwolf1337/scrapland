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
	self->ciclo = -1;
	self->dt = 0.0f;
	self->modo = ANIMMAT_FIN;
	self->lastFrame = -1;
	self->frame = 0;
	self->framef = 0.0f;
}

CAnimMat *__thiscall CAnimMat_ctorCopy(CAnimMat *self, const CAnimMat *src) {
	AnimMat_Init(self);
	self->fps = src->fps;
	self->frames = src->frames;
	self->ciclo = src->ciclo;
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

byte AnimMat_Set(CAnimMat *a, float fps, int ciclo) {
	if (!a) {
		return 0;
	}
	a->ciclo = ciclo;
	a->fps = fps;
	return 1;
}

const char *AnimMat_ModoName(int modo) {
	switch (modo) {
	case ANIMMAT_REPR:
		return Game_Ptr(0x3daf58);
	case ANIMMAT_PAUSA:
		return Game_Ptr(0x3daf60);
	case ANIMMAT_FUND_PAUSA:
		return Game_Ptr(0x3daf68);
	case ANIMMAT_FIN:
		return Game_Ptr(0x3daf74);
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
	self->modo = ANIMMAT_REPR;
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
	self->modo = ANIMMAT_REPR;
}

void __thiscall CAnimMat_Stop(CAnimMat *self) {
	self->modo = ANIMMAT_FIN;
}

void __thiscall CAnimMat_Pause(CAnimMat *self) {
	self->modo = ANIMMAT_PAUSA;
}

void __thiscall CAnimMat_FundPausa(CAnimMat *self, float ms) {
	if (self->modo != ANIMMAT_FUND_PAUSA) {
		return;
	}
	self->modo = ANIMMAT_FUND_PAUSA;
	self->fundK = 1.0f;
	self->fundRate = (float)(1000.0 / (double)ms);
}

void __fastcall CAnimMat_Update(CAnimMat *self) {
	int modo = self->modo;

	if (modo == ANIMMAT_FIN) {
		return;
	}
	uint32_t now = g_TimeMs;
	uint32_t ms = now - self->lastTime;
	self->lastTime = now;
	float dt = Float_MinSS(0.1f, (float)(double)ms * 0.001f);
	if (modo == ANIMMAT_FUND_PAUSA) {
		float k = Float_MaxSS(self->fundK - self->fundRate * dt, 0.2f);
		self->fundK = k;
		dt = dt * k;
	}
	float step = 0.0f;
	if (modo == ANIMMAT_REPR) {
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
	if (self->ciclo >= 0) {
		f = (f - (float)frames) + (float)self->ciclo;
		self->framef = f;
		self->frame = (int)f;
		return;
	}
	self->modo = ANIMMAT_FIN;
	self->frame = frames - 1;
	self->framef = (float)frames - 0.0001f;
}
