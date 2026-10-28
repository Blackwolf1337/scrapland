/*
 * Entities/EmbeddedFX/EmbeddedFXChar.cpp of the game: the effects embedded in the character entities, see
 * include/scrap/embeddedfxchar.h.
 */
#include <string.h>

#include "scrap/crt.h"
#include "scrap/embeddedfxchar.h"
#include "scrap/fxmisc.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/timer.h"

#define CGAMETRAIL_CTOR 0x21ddb0
#define CGAMETRAIL_DTOR 0x21de40

#define VT_EMBEDDEDFX 0x3b08a0
#define VT_SENTINEL 0x3b09e8
#define VT_GEAR 0x3b0a04
#define VT_POLICE 0x3b0a20
#define VT_BANKDIRECTOR 0x3b0a3c
#define VT_BANKMASTER 0x3b0a58
#define VT_CRAZYGAMBLER 0x3b0a74
#define VT_RUSTY 0x3b0a90
#define VT_MESSENGER 0x3b0aac
#define VT_MAINTENANCE 0x3b0ac8
#define VT_MERCENARY 0x3b0ae4

#define TRAIL_COLOR 0xc0c0ff
#define LASER_COLOR 0x800000

/* the color ramps of the alarm and maintenance particles (9 colors) */
#define g_RampAlarmA ((uint32_t *)Game_Ptr(0x428354))
#define g_RampAlarmB ((uint32_t *)Game_Ptr(0x428330))
#define g_RampAlarmC ((uint32_t *)Game_Ptr(0x42839c))
#define g_RampAlarmD ((uint32_t *)Game_Ptr(0x428378))
#define g_RampMaintenance ((uint32_t *)Game_Ptr(0x42842c))
#define g_RampMaintenanceEnd ((uint32_t *)Game_Ptr(0x428408))

/* rand() & 0x7fff as 0..1 (double) */
static inline double EFX_Rnd(void) {
	return (double)(crt_rand() & 0x7fff) * (1.0 / 32767.0);
}

static inline void EFX_SetVtbl(void *self, uint32_t vt) {
	*(void **)self = Game_Ptr(vt);
}

static inline void EFX_TrailsDtor(CGameTrail *trails, uint32_t n) {
	crt_eh_vector_dtor(trails, sizeof(CGameTrail), n, Game_Ptr(CGAMETRAIL_DTOR));
}

static inline cEmbeddedFX *EFX_Delete(cEmbeddedFX *self, byte flags, uint32_t size) {
	if (flags & 1) {
		operator_delete_sized(self, size);
	}
	return self;
}

/* the model of the entity, CHECKed */
static CGameModel *EFX_Model(cEmbeddedFX *self, cEntity *e) {
	self->ent = e;
	CGameModel *model = FX_Vtbl(e)->GetModel(e);
	CHECK(CGameModel_IsLoaded(model));
	return model;
}

/* the particles let go and attached again to the render node name (if the model has it) */
static int EFX_AttachOpt(CGameParticles *p, int *dummy, CGameModel *model, const char *name) {
	*dummy = -1;
	CGameParticles_Release(p);
	int i = CGameModel_FindNodoR(model, name);
	*dummy = i;
	if (i != -1) {
		CGameParticles_Attach(p, model, i);
	}
	return i;
}

/* a particle system still at the dummy: no birth, no spread of position, velocity and acceleration */
static void EFX_Still(CGameParticles *p) {
	p->birthRate = 0;
	p->initPosMin.x = 0.0f;
	p->initPosMin.y = 0.0f;
	p->initPosMin.z = 0.0f;
	p->initPosMax = p->initPosMin;
	p->initVelMin.x = 0.0f;
	p->initVelMin.y = 0.0f;
	p->initVelMin.z = 0.0f;
	p->initVelMax = p->initVelMin;
	p->initAccMin.x = 0.0f;
	p->initAccMin.y = 0.0f;
	p->initAccMin.z = 0.0f;
	p->initAccMax = p->initAccMin;
}

static inline void EFX_TrailStyle(CGameTrail *t, byte segments, byte smooth, int texture, float tail, byte fade) {
	CGameTrail_SetMaxTrails(t, segments);
	CGameTrail_SetSmooth(t, smooth);
	CGameTrail_SetColor(t, TRAIL_COLOR);
	CGameTrail_SetTexture(t, texture);
	CGameTrail_SetScale(t, 1.0f, tail);
	CGameTrail_SetFade(t, fade);
}

/* ---- constructors ---- */

static inline void EFX_TrailsCtor(CGameTrail *trails, uint32_t n) {
	crt_eh_vector_ctor(trails, sizeof(CGameTrail), n, Game_Ptr(CGAMETRAIL_CTOR), Game_Ptr(CGAMETRAIL_DTOR));
}

/* the original clears the vtable pointer before it stores the class one */
static inline void EFX_CtorVtbl(void *self, uint32_t vt) {
	*(void **)self = NULL;
	EFX_SetVtbl(self, vt);
}

cEmbeddedFXAlarm *__fastcall cEmbeddedFXAlarm_ctor(cEmbeddedFXAlarm *self) {
	CGameParticles_ctor(&self->particles);
	self->dummy = -1;
	self->level = 0.0f;
	return self;
}

cEmbeddedFX *__fastcall cEmbeddedFXBankDirector_ctor(void *p) {
	cEmbeddedFXBankDirector *self = p;
	EFX_CtorVtbl(self, VT_BANKDIRECTOR);
	CGameParticles_ctor(&self->glow);
	self->base.ent = NULL;
	self->dummyGlow = -1;
	self->on = 0;
	return &self->base;
}

cEmbeddedFX *__fastcall cEmbeddedFXBankMaster_ctor(void *p) {
	cEmbeddedFXBankMaster *self = p;
	EFX_CtorVtbl(self, VT_BANKMASTER);
	CGameParticles_ctor(&self->glow);
	CGameParticles_ctor(&self->baston);
	self->base.ent = NULL;
	self->dummyGlow = -1;
	self->dummyBaston = -1;
	self->on = 0;
	return &self->base;
}

cEmbeddedFX *__fastcall cEmbeddedFXCrazyGambler_ctor(void *p) {
	cEmbeddedFXCrazyGambler *self = p;
	EFX_CtorVtbl(self, VT_CRAZYGAMBLER);
	CGameParticles_ctor(&self->particles);
	EFX_TrailsCtor(self->trails, FXGAMBLER_ARCS);
	self->b2188 = 0;
	self->base.ent = NULL;
	self->arcT = 0.0f;
	self->t = 0.0f;
	self->visible = 0;
	self->model = NULL;
	self->nodeHead = -1;
	self->nodeHand = -1;
	return &self->base;
}

cEmbeddedFX *__fastcall cEmbeddedFXGear_ctor(void *p) {
	cEmbeddedFXGear *self = p;
	EFX_CtorVtbl(self, VT_GEAR);
	cEmbeddedFXAlarm_ctor(&self->alarm);
	CGameParticles_ctor(&self->shoot);
	CGameTrail_ctor(&self->trail);
	self->base.ent = NULL;
	self->dummyShoot = -1;
	return &self->base;
}

cEmbeddedFX *__fastcall cEmbeddedFXMaintenance_ctor(void *p) {
	cEmbeddedFXMaintenance *self = p;
	EFX_CtorVtbl(self, VT_MAINTENANCE);
	CGameParticles_ctor(&self->light);
	self->base.ent = NULL;
	self->t = 0.0f;
	self->lastT = 0.0f;
	return &self->base;
}

cEmbeddedFX *__fastcall cEmbeddedFXMercenary_ctor(void *p) {
	cEmbeddedFXMercenary *self = p;
	EFX_CtorVtbl(self, VT_MERCENARY);
	CGameParticles_ctor(&self->glow);
	CGameParticles_ctor(&self->bubbles);
	CGameParticles_ctor(&self->pipe);
	self->base.ent = NULL;
	self->dummyGlow = -1;
	self->dummyBubbles = -1;
	self->b08 = 0;
	self->i0c = 0;
	self->isL = 0;
	return &self->base;
}

cEmbeddedFX *__fastcall cEmbeddedFXMessenger_ctor(void *p) {
	cEmbeddedFXMessenger *self = p;
	EFX_CtorVtbl(self, VT_MESSENGER);
	CGameParticles_ctor(&self->glow);
	EFX_TrailsCtor(self->trails, FXMESSENGER_TRAILS);
	self->b334 = 0;
	self->base.ent = NULL;
	self->i08 = 0;
	self->b0c = 0;
	return &self->base;
}

cEmbeddedFX *__fastcall cEmbeddedFXPolice_ctor(void *p) {
	cEmbeddedFXPolice *self = p;
	EFX_CtorVtbl(self, VT_POLICE);
	cEmbeddedFXAlarm_ctor(&self->alarm);
	self->base.ent = NULL;
	return &self->base;
}

cEmbeddedFX *__fastcall cEmbeddedFXRusty_ctor(void *p) {
	cEmbeddedFXRusty *self = p;
	EFX_CtorVtbl(self, VT_RUSTY);
	CGameParticles_ctor(&self->front);
	CGameParticles_ctor(&self->back);
	CGameParticles_ctor(&self->sparks);
	CGameParticles_ctor(&self->glowLighting);
	EFX_TrailsCtor(self->trails, FXRUSTY_TRAILS);
	self->base.ent = NULL;
	self->dummyFront = -1;
	self->dummyBack = -1;
	self->dummySparks = -1;
	self->b08 = 0;
	self->i228 = 0;
	return &self->base;
}

cEmbeddedFX *__fastcall cEmbeddedFXSentinel_ctor(void *p) {
	cEmbeddedFXSentinel *self = p;
	EFX_CtorVtbl(self, VT_SENTINEL);
	CGameParticles_ctor(&self->status);
	CGameParticles_ctor(&self->laser);
	EFX_TrailsCtor(self->beams, FXSENTINEL_LASERS);
	EFX_TrailsCtor(self->edgesA, FXSENTINEL_LASERS);
	EFX_TrailsCtor(self->edgesB, FXSENTINEL_LASERS);
	cEmbeddedFXAlarm_ctor(&self->alarm);
	self->base.ent = NULL;
	self->dummyStatus = -1;
	self->dummyLaser = -1;
	self->i1f0 = -1;
	self->i1ec = -1;
	return &self->base;
}

/* ---- destructors ---- */

/* EmbeddedFX.h, in the exe after WalkCharEntity.cpp */
void __fastcall cEmbeddedFX_dtor(cEmbeddedFX *self) {
	EFX_SetVtbl(self, VT_EMBEDDEDFX);
}

/* vf00 of the base class (rva 0xd6900); it frees 4 bytes, the size of the vtable pointer alone */
cEmbeddedFX *__thiscall cEmbeddedFX_Delete(cEmbeddedFX *self, byte flags) {
	cEmbeddedFX_dtor(self);
	return EFX_Delete(self, flags, 4);
}

void __fastcall cEmbeddedFXBankDirector_dtor(cEmbeddedFXBankDirector *self) {
	EFX_SetVtbl(self, VT_BANKDIRECTOR);
	CGameParticles_Release(&self->glow);
	cEmbeddedFX_dtor(&self->base);
}

void __fastcall cEmbeddedFXBankMaster_dtor(cEmbeddedFXBankMaster *self) {
	EFX_SetVtbl(self, VT_BANKMASTER);
	CGameParticles_Release(&self->baston);
	CGameParticles_Release(&self->glow);
	cEmbeddedFX_dtor(&self->base);
}

void __fastcall cEmbeddedFXCrazyGambler_dtor(cEmbeddedFXCrazyGambler *self) {
	EFX_SetVtbl(self, VT_CRAZYGAMBLER);
	EFX_TrailsDtor(self->trails, FXGAMBLER_ARCS);
	CGameParticles_Release(&self->particles);
	cEmbeddedFX_dtor(&self->base);
}

void __fastcall cEmbeddedFXGear_dtor(cEmbeddedFXGear *self) {
	EFX_SetVtbl(self, VT_GEAR);
	CGameTrail_dtor(&self->trail);
	CGameParticles_Release(&self->shoot);
	CGameParticles_Release(&self->alarm.particles);
	cEmbeddedFX_dtor(&self->base);
}

void __fastcall cEmbeddedFXMaintenance_dtor(cEmbeddedFXMaintenance *self) {
	EFX_SetVtbl(self, VT_MAINTENANCE);
	CGameParticles_Release(&self->light);
	cEmbeddedFX_dtor(&self->base);
}

void __fastcall cEmbeddedFXMercenary_dtor(cEmbeddedFXMercenary *self) {
	EFX_SetVtbl(self, VT_MERCENARY);
	CGameParticles_Release(&self->pipe);
	CGameParticles_Release(&self->bubbles);
	CGameParticles_Release(&self->glow);
	cEmbeddedFX_dtor(&self->base);
}

void __fastcall cEmbeddedFXMessenger_dtor(cEmbeddedFXMessenger *self) {
	EFX_SetVtbl(self, VT_MESSENGER);
	EFX_TrailsDtor(self->trails, FXMESSENGER_TRAILS);
	CGameParticles_Release(&self->glow);
	cEmbeddedFX_dtor(&self->base);
}

void __fastcall cEmbeddedFXPolice_dtor(cEmbeddedFXPolice *self) {
	EFX_SetVtbl(self, VT_POLICE);
	CGameParticles_Release(&self->alarm.particles);
	cEmbeddedFX_dtor(&self->base);
}

void __fastcall cEmbeddedFXAlarm_dtor(cEmbeddedFXAlarm *self) {
	CGameParticles_Release(&self->particles);
}

void __fastcall cEmbeddedFXRusty_dtor(cEmbeddedFXRusty *self) {
	EFX_SetVtbl(self, VT_RUSTY);
	EFX_TrailsDtor(self->trails, FXRUSTY_TRAILS);
	CGameParticles_Release(&self->glowLighting);
	CGameParticles_Release(&self->sparks);
	CGameParticles_Release(&self->back);
	CGameParticles_Release(&self->front);
	cEmbeddedFX_dtor(&self->base);
}

void __fastcall cEmbeddedFXSentinel_dtor(cEmbeddedFXSentinel *self) {
	EFX_SetVtbl(self, VT_SENTINEL);
	CGameParticles_Release(&self->alarm.particles);
	EFX_TrailsDtor(self->edgesB, FXSENTINEL_LASERS);
	EFX_TrailsDtor(self->edgesA, FXSENTINEL_LASERS);
	EFX_TrailsDtor(self->beams, FXSENTINEL_LASERS);
	CGameParticles_Release(&self->laser);
	CGameParticles_Release(&self->status);
	cEmbeddedFX_dtor(&self->base);
}

/* ---- vf00: deleting destructors ---- */

cEmbeddedFX *__thiscall cEmbeddedFXBankDirector_Delete(cEmbeddedFXBankDirector *self, byte flags) {
	cEmbeddedFXBankDirector_dtor(self);
	return EFX_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXBankMaster_Delete(cEmbeddedFXBankMaster *self, byte flags) {
	cEmbeddedFXBankMaster_dtor(self);
	return EFX_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXCrazyGambler_Delete(cEmbeddedFXCrazyGambler *self, byte flags) {
	cEmbeddedFXCrazyGambler_dtor(self);
	return EFX_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXGear_Delete(cEmbeddedFXGear *self, byte flags) {
	cEmbeddedFXGear_dtor(self);
	return EFX_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXMaintenance_Delete(cEmbeddedFXMaintenance *self, byte flags) {
	cEmbeddedFXMaintenance_dtor(self);
	return EFX_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXMercenary_Delete(cEmbeddedFXMercenary *self, byte flags) {
	cEmbeddedFXMercenary_dtor(self);
	return EFX_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXMessenger_Delete(cEmbeddedFXMessenger *self, byte flags) {
	cEmbeddedFXMessenger_dtor(self);
	return EFX_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXPolice_Delete(cEmbeddedFXPolice *self, byte flags) {
	cEmbeddedFXPolice_dtor(self);
	return EFX_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXRusty_Delete(cEmbeddedFXRusty *self, byte flags) {
	cEmbeddedFXRusty_dtor(self);
	return EFX_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXSentinel_Delete(cEmbeddedFXSentinel *self, byte flags) {
	cEmbeddedFXSentinel_dtor(self);
	return EFX_Delete(&self->base, flags, sizeof(*self));
}

/* ---- vf02: Release ---- */

void __fastcall cEmbeddedFXBankDirector_Release(cEmbeddedFXBankDirector *self) {
	self->dummyGlow = -1;
	CGameParticles_Release(&self->glow);
}

void __fastcall cEmbeddedFXBankMaster_Release(cEmbeddedFXBankMaster *self) {
	self->dummyGlow = -1;
	CGameParticles_Release(&self->glow);
	self->dummyBaston = -1;
	CGameParticles_Release(&self->baston);
}

void __fastcall cEmbeddedFXCrazyGambler_Release(cEmbeddedFXCrazyGambler *self) {
	self->arcT = 0.0f;
	self->t = 0.0f;
	for (int i = 0; i < FXGAMBLER_ARCS; i++) {
		self->life[i] = 0.0f;
		CGameTrail_Clear(&self->trails[i]);
		self->count[i] = 0;
		self->tail[i] = 0;
		self->head[i] = 0;
		memset(self->used[i], 0, 0x20);
	}
	CGameParticles_Release(&self->particles);
}

void __fastcall cEmbeddedFXGear_Release(cEmbeddedFXGear *self) {
	self->alarm.dummy = -1;
	CGameParticles_Release(&self->alarm.particles);
	self->dummyShoot = -1;
	CGameParticles_Release(&self->shoot);
}

void __fastcall cEmbeddedFXMaintenance_Release(cEmbeddedFXMaintenance *self) {
	CGameParticles_Release(&self->light);
}

void __fastcall cEmbeddedFXMercenary_Release(cEmbeddedFXMercenary *self) {
	self->dummyGlow = -1;
	CGameParticles_Release(&self->glow);
	self->dummyBubbles = -1;
	CGameParticles_Release(&self->bubbles);
}

void __fastcall cEmbeddedFXMessenger_Release(cEmbeddedFXMessenger *self) {
	self->i08 = 0;
	for (int i = 0; i < FXMESSENGER_TRAILS; i++) {
		self->trailState[i] = 0;
		CGameTrail_Clear(&self->trails[i]);
	}
	CGameParticles_Release(&self->glow);
}

void __fastcall cEmbeddedFXPolice_Release(cEmbeddedFXPolice *self) {
	self->alarm.dummy = -1;
	CGameParticles_Release(&self->alarm.particles);
}

void __fastcall cEmbeddedFXAlarm_Release(cEmbeddedFXAlarm *self) {
	self->dummy = -1;
	CGameParticles_Release(&self->particles);
}

void __fastcall cEmbeddedFXRusty_Release(cEmbeddedFXRusty *self) {
	self->dummyFront = -1;
	CGameParticles_Release(&self->front);
	self->dummyBack = -1;
	CGameParticles_Release(&self->back);
	self->dummySparks = -1;
	CGameParticles_Release(&self->sparks);
}

void __fastcall cEmbeddedFXSentinel_Release(cEmbeddedFXSentinel *self) {
	CGameParticles_Release(&self->laser);
	CGameParticles_Release(&self->status);
	for (int i = 0; i < self->numLasers; i++) {
		CGameTrail_Clear(&self->beams[i]);
		CGameTrail_Clear(&self->edgesA[i]);
		CGameTrail_Clear(&self->edgesB[i]);
	}
	self->alarm.dummy = -1;
	CGameParticles_Release(&self->alarm.particles);
}

/* ---- the clears of vf05 ---- */

void __fastcall cEmbeddedFXCrazyGambler_Clear(cEmbeddedFXCrazyGambler *self) {
	for (int i = 0; i < FXGAMBLER_ARCS; i++) {
		self->count[i] = 0;
		self->tail[i] = 0;
		self->head[i] = 0;
		memset(self->used[i], 0, 0x20);
		CGameTrail_Clear(&self->trails[i]);
	}
	CGameParticles_KillAll(&self->particles);
}

void __fastcall cEmbeddedFXMessenger_Clear(cEmbeddedFXMessenger *self) {
	for (int i = 0; i < FXMESSENGER_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
	}
	CGameParticles_KillAll(&self->glow);
	memset(self->u2b4, 0, sizeof(self->u2b4));
}

void __fastcall cEmbeddedFXRusty_Clear(cEmbeddedFXRusty *self) {
	for (int i = 0; i < FXRUSTY_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
	}
	CGameParticles_KillAll(&self->sparks);
}

void __fastcall cEmbeddedFXSentinel_Clear(cEmbeddedFXSentinel *self) {
	for (int i = 0; i < FXSENTINEL_LASERS; i++) {
		CGameTrail_Clear(&self->beams[i]);
		CGameTrail_Clear(&self->edgesA[i]);
		CGameTrail_Clear(&self->edgesB[i]);
	}
	CGameParticles_KillAll(&self->laser);
	CGameParticles_KillAll(&self->status);
}

/* ---- vf05: Stop ---- */

void __thiscall cEmbeddedFXCrazyGambler_Stop(cEmbeddedFXCrazyGambler *self, byte keep) {
	if (!keep) {
		cEmbeddedFXCrazyGambler_Clear(self);
	}
}

void __thiscall cEmbeddedFXMessenger_Stop(cEmbeddedFXMessenger *self, byte keep) {
	if (!keep) {
		cEmbeddedFXMessenger_Clear(self);
	}
}

void __thiscall cEmbeddedFXRusty_Stop(cEmbeddedFXRusty *self, byte keep) {
	if (!keep) {
		cEmbeddedFXRusty_Clear(self);
	}
}

void __thiscall cEmbeddedFXSentinel_Stop(cEmbeddedFXSentinel *self, byte keep) {
	if (keep) {
		self->i1ec = 0;
		return;
	}
	cEmbeddedFXSentinel_Clear(self);
}

/* ---- vf01: Reset ---- */

byte __fastcall cEmbeddedFXAlarm_Reset(cEmbeddedFXAlarm *self) {
	self->on = 0;
	self->cur = 0;
	self->level = 0.0f;
	self->t = 0.0f;
	self->lastT = 0.0f;
	self->lights = 1;
	return 1;
}

byte __fastcall cEmbeddedFXCrazyGambler_Reset(cEmbeddedFXCrazyGambler *self) {
	for (int i = 0; i < FXGAMBLER_ARCS; i++) {
		self->life[i] = 0.0f;
		EFX_TrailStyle(&self->trails[i], 12, 1, 0, 0.6f, 1);
		self->count[i] = 0;
		self->tail[i] = 0;
		self->head[i] = 0;
		memset(self->used[i], 0, 0x20);
	}
	return 1;
}

/* Gear and Police: the reset of their alarm */
byte __fastcall cEmbeddedFXPolice_Reset(cEmbeddedFXPolice *self) {
	return cEmbeddedFXAlarm_Reset(&self->alarm);
}

byte __fastcall cEmbeddedFXMercenary_Reset(cEmbeddedFXMercenary *self) {
	self->i0c = 0;
	return 1;
}

byte __fastcall cEmbeddedFXMessenger_Reset(cEmbeddedFXMessenger *self) {
	for (int i = 0; i < FXMESSENGER_TRAILS; i++) {
		self->trailState[i] = 0;
		EFX_TrailStyle(&self->trails[i], 8, 0, 3, 1.0f, 0);
	}
	memset(self->u2b4, 0, sizeof(self->u2b4));
	return 1;
}

byte __fastcall cEmbeddedFXRusty_Reset(cEmbeddedFXRusty *self) {
	for (int i = 0; i < FXRUSTY_TRAILS; i++) {
		self->trailState[i] = 0;
		EFX_TrailStyle(&self->trails[i], 10, 1, 3, 1.0f, 0);
	}
	return 1;
}

static void EFX_LaserTrail(CGameTrail *t, byte segments, float width, byte fade) {
	CGameTrail_SetMaxTrails(t, segments);
	CGameTrail_SetTexture(t, 0);
	CGameTrail_SetColor(t, LASER_COLOR);
	CGameTrail_SetWidth(t, width);
	CGameTrail_SetFade(t, fade);
}

byte __fastcall cEmbeddedFXSentinel_Reset(cEmbeddedFXSentinel *self) {
	self->numLasers = FXSENTINEL_LASERS;
	for (int i = 0; i < self->numLasers; i++) {
		EFX_LaserTrail(&self->beams[i], 3, 14.0f, 0);
		CGameTrail_SetType(&self->edgesA[i], 1);
		EFX_LaserTrail(&self->edgesA[i], 11, 8.0f, 1);
		EFX_LaserTrail(&self->edgesB[i], 11, 8.0f, 1);
	}
	for (int i = 0; i < FXSENTINEL_LASERS; i++) {
		self->f1dc[i] = 0.0f;
	}
	for (int i = 0; i < FXSENTINEL_LASERS; i++) {
		self->phase[i].x = 0.0f;
		self->phase[i].y = 0.0f;
	}
	self->phase[0].z = 0.0f;
	self->phase[1].z = 1.5707964f;
	self->phase[2].z = 3.1415927f;
	self->phase[3].z = 4.712389f;
	self->f170 = 0.0f;
	self->f184 = 1.0f;
	cEmbeddedFXAlarm_Reset(&self->alarm);
	self->alarm.lights = 0;
	return 1;
}

/* ---- the alarm ---- */

void __thiscall cEmbeddedFXAlarm_SetOn(cEmbeddedFXAlarm *self, byte on) {
	self->cur = self->on;
	self->on = on;
}

void __thiscall cEmbeddedFXAlarm_SetLights(cEmbeddedFXAlarm *self, byte on) {
	self->lights = on;
}

/* Gear and Police: the alarm switched */
void __thiscall cEmbeddedFXPolice_SetAlarm(cEmbeddedFXPolice *self, byte on) {
	cEmbeddedFXAlarm_SetOn(&self->alarm, on);
}

void __thiscall cEmbeddedFXAlarm_Init(cEmbeddedFXAlarm *self, CGameModel *model) {
	if (!model) {
		CHECK(model);
		return;
	}
	if (!CGameModel_IsLoaded(model)) {
		return;
	}
	EFX_AttachOpt(&self->particles, &self->dummy, model, "DF_PoliceAlarm");
	CHECK(self->particles.SistPart);
	EFX_Still(&self->particles);
	CGameParticles_Apply(&self->particles);
}

/* ---- Init: the effect on the model of the character ---- */

void __thiscall cEmbeddedFXBankDirector_Init(cEmbeddedFXBankDirector *self, cEntity *e) {
	CGameModel *model = EFX_Model(&self->base, e);

	self->dummyGlow = -1;
	CGameParticles_Release(&self->glow);
	self->dummyGlow = CGameModel_FindNodoR(model, "DF_Glow");
	CHECK(self->dummyGlow > -1);
	CGameParticles_Attach(&self->glow, model, self->dummyGlow);
	CHECK(self->glow.SistPart);
}

void __thiscall cEmbeddedFXBankMaster_Init(cEmbeddedFXBankMaster *self, cEntity *e) {
	CGameModel *model = EFX_Model(&self->base, e);

	self->dummyGlow = -1;
	CGameParticles_Release(&self->glow);
	self->dummyGlow = CGameModel_FindNodoR(model, "DF_Glow");
	CHECK(self->dummyGlow > -1);
	CGameParticles_Attach(&self->glow, model, self->dummyGlow);
	CHECK(self->glow.SistPart);
	self->dummyBaston = CGameModel_FindNodoR(model, "DF_Glow_Baston");
	CHECK(self->dummyBaston > -1);
	CGameParticles_Attach(&self->baston, model, self->dummyBaston);
	CHECK(self->baston.SistPart);
	/* a row of glows along the stick */
	for (int i = 0; i < 16; i++) {
		CGameParticles_SetParticle(&self->baston, i, 0.0f, (float)i * 8.0f - 74.0f, 0.0f, 65.0f, 0xe1f7d255, 2,
			0.0f);
	}
}

void __thiscall cEmbeddedFXCrazyGambler_SetHead(cEmbeddedFXCrazyGambler *self, CGameModel *model,
	const char *node) {
	CHECK(model && CGameModel_IsLoaded(model));
	self->model = model;
	self->nodeHead = -1;
	self->nodeHead = CGameModel_FindNodoR(model, node);
	CHECK(self->nodeHead > -1);
	cEmbeddedFXCrazyGambler_Clear(self);
	self->base.vftable->Update(&self->base, 0.5f);
}

void __thiscall cEmbeddedFXCrazyGambler_Init(cEmbeddedFXCrazyGambler *self, cEntity *e) {
	CGameModel *model = EFX_Model(&self->base, e);

	self->nodeHand = CGameModel_FindNodoR(model, "R_Gambler_mano_Izq");
	CHECK(self->nodeHand > -1);
	int glow = CGameModel_FindNodoR(model, "DF_Glow");
	CHECK(glow > -1);
	CGameParticles_Attach(&self->particles, model, glow);
	CHECK(self->particles.SistPart);
	cEmbeddedFXCrazyGambler_SetHead(self, model, "R_Gambler_Cabeza-Pelos");
}

void __thiscall cEmbeddedFXGear_Init(cEmbeddedFXGear *self, cEntity *e) {
	CGameModel *model = EFX_Model(&self->base, e);

	EFX_AttachOpt(&self->shoot, &self->dummyShoot, model, "DF_GearShoot");
	CHECK(self->shoot.SistPart);
	cEmbeddedFXAlarm_Init(&self->alarm, model);
}

void __thiscall cEmbeddedFXMaintenance_Init(cEmbeddedFXMaintenance *self, cEntity *e) {
	CGameModel *model = EFX_Model(&self->base, e);

	CGameParticles_Attach(&self->light, model, CGameModel_FindNodoR(model, "DF_Light"));
	CHECK(self->light.SistPart);
}

void __thiscall cEmbeddedFXMercenary_Init(cEmbeddedFXMercenary *self, cEntity *e) {
	CGameModel *model = EFX_Model(&self->base, e);

	self->isL = crt_stricmp(CGameModel_Name(model), "MercenaryL.M3D") == 0;
	byte isL = self->isL;
	EFX_AttachOpt(&self->glow, &self->dummyGlow, model, "DF_Glow");
	CHECK(self->glow.SistPart);
	/* two blocks of 8 glows, mirrored in y */
	uint32_t color = isL ? 0xdccf5019 : 0xdccfaa2d;
	int n = 0;
	for (int x = -5; x < 15; x += 10) {
		for (int y = 18, ny = -18; y > 10; y -= 4, ny += 4) {
			for (int z = -5; z < 15; z += 10) {
				CGameParticles_SetParticle(&self->glow, n, (float)x, (float)ny, (float)z, 40.0f, color, 2,
					0.0f);
				CGameParticles_SetParticle(&self->glow, n + 8, (float)x, (float)y, (float)z, 40.0f, color, 2,
					0.0f);
				n++;
			}
		}
	}
	EFX_AttachOpt(&self->bubbles, &self->dummyBubbles, model, "DF_Burbujas");
	CHECK(self->bubbles.SistPart);
	self->dummyPipe = -1;
	if (self->isL) {
		EFX_AttachOpt(&self->pipe, &self->dummyPipe, model, "DF_Burbujas_Pipa");
		CHECK(self->pipe.SistPart);
	}
}

void __thiscall cEmbeddedFXMessenger_Init(cEmbeddedFXMessenger *self, cEntity *e) {
	CGameModel *model = EFX_Model(&self->base, e);
	char name[256];

	self->dummyGlow = CGameModel_FindNodoR(model, "DF_GlowLighting");
	CHECK(self->dummyGlow > -1);
	if (self->dummyGlow != -1) {
		CGameParticles_Attach(&self->glow, model, self->dummyGlow);
	}
	CHECK(self->glow.SistPart);
	for (int i = 0; i < FXMESSENGER_RAYS; i++) {
		Str_PrintfQuiet(name, 0xff, "DF_Rayos_%02d", i + 1);
		self->nodeRays[i] = CGameModel_FindNodoR(model, name);
		CHECK(self->nodeRays[i] > -1);
	}
	for (int i = 0; i < FXMESSENGER_RAYS; i++) {
		self->order[i] = i;
	}
}

void __thiscall cEmbeddedFXPolice_Init(cEmbeddedFXPolice *self, cEntity *e) {
	CGameModel *model = EFX_Model(&self->base, e);

	cEmbeddedFXAlarm_Init(&self->alarm, model);
}

void __thiscall cEmbeddedFXRusty_Init(cEmbeddedFXRusty *self, cEntity *e) {
	CGameModel *model = EFX_Model(&self->base, e);

	EFX_AttachOpt(&self->sparks, &self->dummySparks, model, "DR_Mecanico_ColetaChispas");
	CHECK(self->sparks.SistPart);
	EFX_AttachOpt(&self->front, &self->dummyFront, model, "DR_Mecanico_PechoGlowA");
	CHECK(self->front.SistPart);
	EFX_AttachOpt(&self->back, &self->dummyBack, model, "DR_Mecanico_PechoGlowB");
	CHECK(self->back.SistPart);
	/* the same 8 glows on the chest front and back */
	int n = 0;
	for (int x = -10; x < 30; x += 20) {
		for (int y = -8; y < 0; y += 4) {
			for (int z = -10; z < 30; z += 20) {
				CGameParticles_SetParticle(&self->front, n, (float)x, (float)y, (float)z, 75.0f, 0xe6f7d255, 2,
					0.0f);
				CGameParticles_SetParticle(&self->back, n, (float)x, (float)y, (float)z, 75.0f, 0xe6f7d255, 2,
					0.0f);
				n++;
			}
		}
	}
	self->dummyGlowLighting = CGameModel_FindNodoR(model, "DF_GlowLighting");
	CHECK(self->dummyGlowLighting > -1);
	if (self->dummyGlowLighting != -1) {
		CGameParticles_Attach(&self->glowLighting, model, self->dummyGlowLighting);
	}
	CHECK(self->glowLighting.SistPart);
	self->dummyLighting[0] = CGameModel_FindNodoR(model, "DF_LightingA");
	CHECK(self->dummyLighting[0] > -1);
	self->dummyLighting[1] = CGameModel_FindNodoR(model, "DF_LightingC");
	CHECK(self->dummyLighting[1] > -1);
	self->dummyLighting[2] = CGameModel_FindNodoR(model, "DF_LightingB");
	CHECK(self->dummyLighting[2] > -1);
}

void __thiscall cEmbeddedFXSentinel_Init(cEmbeddedFXSentinel *self, cEntity *e) {
	self->base.ent = e;
	self->dummyStatus = -1;
	CGameParticles_Release(&self->status);
	self->dummyLaser = -1;
	CGameParticles_Release(&self->laser);
	CGameModel *model = FX_Vtbl(self->base.ent)->GetModel(self->base.ent);
	CHECK(CGameModel_IsLoaded(model));
	self->dummyLaser = CGameModel_FindNodoR(model, "DF_LaserScaner");
	CHECK(self->dummyLaser > -1);
	CGameParticles_Attach(&self->laser, model, self->dummyLaser);
	CHECK(self->laser.SistPart);
	self->dummyStatus = CGameModel_FindNodoR(model, "DF_StatusLight");
	CHECK(self->dummyStatus > -1);
	CGameParticles_Attach(&self->status, model, self->dummyStatus);
	CHECK(self->status.SistPart);
	cEmbeddedFXAlarm_Init(&self->alarm, model);
}

/* ---- creation ---- */

typedef cEmbeddedFX *(__fastcall *EFX_Ctor)(void *self);
typedef void(__thiscall *EFX_Init)(cEmbeddedFX *self, cEntity *e);

/* in the order of the tests of the original */
static const struct {
	const char *model;
	uint32_t size;
	EFX_Ctor ctor;
	EFX_Init init;
} efx_kinds[] = {
	{ "Sentinel.M3D", sizeof(cEmbeddedFXSentinel), cEmbeddedFXSentinel_ctor, (EFX_Init)cEmbeddedFXSentinel_Init },
	{ "Gear.M3D", sizeof(cEmbeddedFXGear), cEmbeddedFXGear_ctor, (EFX_Init)cEmbeddedFXGear_Init },
	{ "Police.M3D", sizeof(cEmbeddedFXPolice), cEmbeddedFXPolice_ctor, (EFX_Init)cEmbeddedFXPolice_Init },
	{ "BankDirector.M3D", sizeof(cEmbeddedFXBankDirector), cEmbeddedFXBankDirector_ctor,
		(EFX_Init)cEmbeddedFXBankDirector_Init },
	{ "BankMaster.M3D", sizeof(cEmbeddedFXBankMaster), cEmbeddedFXBankMaster_ctor,
		(EFX_Init)cEmbeddedFXBankMaster_Init },
	{ "CrazyGambler.M3D", sizeof(cEmbeddedFXCrazyGambler), cEmbeddedFXCrazyGambler_ctor,
		(EFX_Init)cEmbeddedFXCrazyGambler_Init },
	{ "Rusty.M3D", sizeof(cEmbeddedFXRusty), cEmbeddedFXRusty_ctor, (EFX_Init)cEmbeddedFXRusty_Init },
	{ "MercenaryA.M3D", sizeof(cEmbeddedFXMercenary), cEmbeddedFXMercenary_ctor, (EFX_Init)cEmbeddedFXMercenary_Init },
	{ "MercenaryB.M3D", sizeof(cEmbeddedFXMercenary), cEmbeddedFXMercenary_ctor, (EFX_Init)cEmbeddedFXMercenary_Init },
	{ "MercenaryC.M3D", sizeof(cEmbeddedFXMercenary), cEmbeddedFXMercenary_ctor, (EFX_Init)cEmbeddedFXMercenary_Init },
	{ "MercenaryL.M3D", sizeof(cEmbeddedFXMercenary), cEmbeddedFXMercenary_ctor, (EFX_Init)cEmbeddedFXMercenary_Init },
	{ "Messenger.M3D", sizeof(cEmbeddedFXMessenger), cEmbeddedFXMessenger_ctor, (EFX_Init)cEmbeddedFXMessenger_Init },
	{ "Maintenance.M3D", sizeof(cEmbeddedFXMaintenance), cEmbeddedFXMaintenance_ctor,
		(EFX_Init)cEmbeddedFXMaintenance_Init },
};

/* the effect of the character model replaces *fx; other models leave *fx as it is */
void EmbeddedFXChar_Create(cEntity *withLife, const char *model, cEmbeddedFX **fx) {
	cEmbeddedFX *cur = *fx;

	CHECK(withLife);
	CHECK(Entity_IsType(withLife, TYEN_WITH_LIFE));
	for (uint32_t i = 0; i < sizeof(efx_kinds) / sizeof(efx_kinds[0]); i++) {
		if (crt_stricmp(model, efx_kinds[i].model)) {
			continue;
		}
		if (cur) {
			cur->vftable->Release(cur);
			cur->vftable->Delete(cur, 1);
		}
		void *p = operator_new(efx_kinds[i].size);
		cur = p ? efx_kinds[i].ctor(p) : NULL;
		cur->vftable->Reset(cur);
		efx_kinds[i].init(cur, withLife);
		break;
	}
	*fx = cur;
}

/* the alarm of the police or gear character name */
void EmbeddedFXChar_SetAlarm(const char *name, byte on) {
	cEntity *v = World_FindEntity(name);

	CHECK(v);
	CHECK(v->vftable->IsType(v, TYEN_WITH_LIFE));
	CHECK(CGameModel_IsLoaded(FX_Vtbl(v)->GetModel(v)));
	if (!strstr(CGameModel_Name(FX_Vtbl(v)->GetModel(v)), "Police")
		&& !strstr(CGameModel_Name(FX_Vtbl(v)->GetModel(v)), "Gear")) {
		return;
	}
	cEmbeddedFXAlarm_SetOn(&((cEmbeddedFXPolice *)Entity_EmbeddedFX(v))->alarm, on);
}

/* the network messages: the character name, a second string and 4 bytes */
static void EFX_ReadAlarm(cAbstractFile *f, byte on) {
	char name[128];
	char other[128];
	int unused;

	cAbstractFile_ReadString(f, name, sizeof(name));
	cAbstractFile_ReadString(f, other, sizeof(other));
	f->vftable->Read(f, &unused, 4);
	EmbeddedFXChar_SetAlarm(name, on);
}

void EmbeddedFXChar_AlarmOff(cAbstractFile *f) {
	EFX_ReadAlarm(f, 0);
}

void EmbeddedFXChar_AlarmOn(cAbstractFile *f) {
	EFX_ReadAlarm(f, 1);
}

/* ---- vf03: Update ---- */

static inline uint32_t EFX_Color(int a, int g) {
	return ((uint32_t)a << 24) | ((uint32_t)g << 16) | (uint32_t)g | 0xff00;
}

/*
 * The glow of the bank characters seen from the camera: (1 - |dy| / 1400) for the height to the camera times the
 * square of how much the model faces it ((pi - |yaw difference|) / pi).
 */
static float EFX_FaceCamera(CGameModel *model) {
	cWorld *w = g_pWorld;
	vec3_t cam = w->camPos[w->curCam];
	vec3_t pos, rot, d, dir, ang;

	CGameModel_GetPos(model, &pos);
	CGameModel_GetRot(model, &rot);
	d.y = pos.y - cam.y;
	d.x = pos.x - cam.x;
	d.z = pos.z - cam.z;
	float len = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z));
	if (len == 0.0f) {
		dir = d;
	} else {
		float inv = 1.0f / len;
		dir.x = d.x * inv;
		dir.y = d.y * inv;
		dir.z = d.z * inv;
	}
	float yaw = Vec3_GetRotAng(&ang, &dir)->y;
	float ry = NormAng(rot.y);
	float a = NormAng(yaw);
	float diff = (float)__builtin_fabs((double)((ry + M_2PI_F) - (a + M_2PI_F)));
	float dy = (float)__builtin_fabs((double)(pos.y - cam.y));
	float height = dy > 1400.0f ? 0.0f : 1.0f - dy / 1400.0f;
	float face = (float)((double)(3.1415927f - diff) / M_PI_D);
	return face * height * face;
}

void __thiscall cEmbeddedFXBankDirector_Update(cEmbeddedFXBankDirector *self, float dt) {
	CGameModel *model = FX_Vtbl(self->base.ent)->GetModel(self->base.ent);
	CGameParticles *p = &self->glow;

	(void)dt;
	if (!CGameModel_IsLoaded(model) || !CGameModel_IsVisible(model)) {
		return;
	}
	float v = EFX_FaceCamera(model);
	if (v > 0.3f && 1.1f > v) {
		v = (v - 0.3f) * 1.4f;
		self->on = 1;
		int e = (int)(v * -16.0f);
		CGameParticles_SetParticle(p, 0, 0.0f, 2.0f, v * 5.0f, v * 15.0f + 20.0f,
			EFX_Color(-0x80 - (int)(v * 92.0f), 0x60 - e), 1, 0.0f);
		CGameParticles_SetParticle(p, 1, 0.0f, 5.0f, v * 15.0f, v * 105.0f + 40.0f,
			EFX_Color(-1 - (int)(v * 164.0f), 0x20 - e), 2, 0.0f);
		CGameParticles_SetParticle(p, 2, 0.0f, 2.0f, v * 5.0f, v * 160.0f + 30.0f,
			EFX_Color(-1 - (int)(v * 128.0f), 0x10 - e), 5, 0.0f);
		return;
	}
	if (self->on) {
		self->on = 0;
		CGameParticles_KillRange(p, 0, 3);
	}
}

void __thiscall cEmbeddedFXBankMaster_Update(cEmbeddedFXBankMaster *self, float dt) {
	CGameModel *model = FX_Vtbl(self->base.ent)->GetModel(self->base.ent);
	CGameParticles *p = &self->glow;

	(void)dt;
	if (!CGameModel_IsLoaded(model) || !CGameModel_IsVisible(model)) {
		return;
	}
	float v = EFX_FaceCamera(model);
	if (v > 0.3f && 1.1f > v) {
		v = (v - 0.3f) * 1.4f;
		self->on = 1;
		int e = (int)(v * -16.0f);
		CGameParticles_SetParticle(p, 0, 0.0f, 2.0f, v * 4.0f, v * 15.0f + 20.0f,
			EFX_Color(-0x80 - (int)(v * 92.0f), 0x80 - e), 1, 0.0f);
		CGameParticles_SetParticle(p, 1, 0.0f, 5.0f, v * 8.0f, v * 65.0f + 30.0f,
			EFX_Color(-1 - (int)(v * 164.0f), 0x2a - e), 2, 0.0f);
		float r = (float)(EFX_Rnd() * 5.0 + 155.0);
		CGameParticles_SetParticle(p, 2, 0.0f, 2.0f, v * 5.0f, r * v + 30.0f,
			EFX_Color(-1 - (int)(v * 128.0f), 8 - (int)(v * -8.0f)), 5, 0.0f);
	} else if (self->on) {
		self->on = 0;
		CGameParticles_KillRange(p, 0, 3);
	}
	CGameParticles_SetAlpha(&self->baston, (float)(EFX_Rnd() * 0.1499999761581421 + 0.8500000238418579));
}

/* the burst of a blink: a small flash first when the rotating lights are off, then the big one */
static void EFX_AlarmFlash(cEmbeddedFXAlarm *self, uint32_t *rampSmall, uint32_t *rampBig) {
	CGameParticles *p = &self->particles;

	if (!self->lights) {
		p->initPosMin.x = 5.0f;
		p->initPosMin.y = 0.0f;
		p->initPosMin.z = -2.0f;
		p->initPosMax = p->initPosMin;
		p->sizeIni = 15.0f;
		p->sizeEnd = 75.0f;
		p->sizeLife = 0.2f;
		p->lifeTime = 0.12f;
		p->spriteIni = 2;
		p->spriteEnd = 2;
		CGameParticles_Apply(p);
		CGameParticles_SetColorRampOn(p, 1);
		CGameParticles_SetColorRamp(p, rampSmall);
		CGameParticles_EmitN(p, 1, 0);
		p->initPosMin.x = 0.0f;
		p->initPosMin.y = 0.0f;
		p->initPosMin.z = 0.0f;
		p->initPosMax = p->initPosMin;
	}
	p->sizeIni = 75.0f;
	p->sizeEnd = self->lights ? 850.0f : 450.0f;
	p->sizeLife = 0.2f;
	p->lifeTime = 0.1f;
	p->spriteIni = 5;
	p->spriteEnd = 5;
	CGameParticles_Apply(p);
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, rampBig);
	CGameParticles_EmitN(p, 1, 0);
}

/* the x offsets and sizes of the 12 rotating lights (6 to each side) */
static const float efx_alarmX[12] = { 6.0f, 8.0f, 38.0f, 65.0f, 95.0f, 135.0f,
	-6.0f, -8.0f, -38.0f, -65.0f, -95.0f, -135.0f };
static const float efx_alarmSize[6] = { 80.0f, 250.0f, 60.0f, 120.0f, 180.0f, 290.0f };
static const uint32_t efx_alarmRGB[12] = { 0x8080ff, 0x1010ff, 0x80, 0xc0, 0xde, 0xc0,
	0xff8080, 0xff1010, 0x800000, 0xc00000, 0xde0000, 0xc00000 };

/* blinks twice in the period of 0.9 s while on; the level fades to the state */
void __thiscall cEmbeddedFXAlarm_Update(cEmbeddedFXAlarm *self, float dt) {
	if (self->on) {
		float t = dt + self->t;
		self->t = t;
		if (t > 0.9f) {
			self->t = 0.0f;
		}
		if ((0.1f > self->lastT && self->t > 0.1f) || (0.2f > self->lastT && self->t > 0.2f)) {
			EFX_AlarmFlash(self, g_RampAlarmA, g_RampAlarmB);
		}
		if ((0.6f > self->lastT && self->t > 0.6f) || (0.7f > self->lastT && self->t > 0.7f)) {
			EFX_AlarmFlash(self, g_RampAlarmC, g_RampAlarmD);
		}
		self->lastT = self->t;
	}
	if (self->cur == self->on) {
		return;
	}
	float step = cTimer_GetDelta(&g_pWorld->timer) * 10.0f;
	Float_Approach(&self->level, (float)(self->on != 0), step);
	if (self->lights) {
		int a = (int)(255.0f - self->level * 64.0f);
		uint32_t alpha[3] = { (uint32_t)(a - 0x40) << 24, (uint32_t)(a - 0x20) << 24, (uint32_t)a << 24 };
		for (int i = 0; i < 12; i++) {
			int k = i % 6;
			CGameParticles_SetParticle(&self->particles, i, self->level * efx_alarmX[i], 0.0f, 0.0f,
				efx_alarmSize[k], alpha[k < 2 ? k : 2] | efx_alarmRGB[i], k == 1 ? 5 : 2, 0.0f);
		}
	}
	if (self->on) {
		if (self->level > 0.99f) {
			self->cur = self->on;
		}
	} else if (0.01f > self->level) {
		self->cur = 0;
		CGameParticles_KillAll(&self->particles);
	}
}

void __thiscall cEmbeddedFXPolice_Update(cEmbeddedFXPolice *self, float dt) {
	cEmbeddedFXAlarm_Update(&self->alarm, dt);
}

/* a light blinks every 1.2 s */
void __thiscall cEmbeddedFXMaintenance_Update(cEmbeddedFXMaintenance *self, float dt) {
	static const struct {
		float sizeIni, sizeEnd, lifeTime;
		byte sprite, spin;
	} flashes[4] = {
		{ 15.0f, 55.0f, 0.22f, 1, 0 },
		{ 25.0f, 85.0f, 0.22f, 2, 0 },
		{ 45.0f, 450.0f, 0.2f, 6, 1 },
		{ 75.0f, 550.0f, 0.2f, 5, 1 },
	};
	CGameParticles *p = &self->light;

	float t = dt + self->t;
	self->t = t;
	if (t > 1.2f) {
		self->t = 0.0f;
	}
	if (0.2f > self->lastT && self->t > 0.2f) {
		EFX_Still(p);
		for (int i = 0; i < 4; i++) {
			p->sizeIni = flashes[i].sizeIni;
			p->sizeEnd = flashes[i].sizeEnd;
			p->sizeLife = 0.2f;
			p->lifeTime = flashes[i].lifeTime;
			p->spriteIni = flashes[i].sprite;
			p->spriteEnd = flashes[i].sprite;
			if (flashes[i].spin) {
				float rot = (float)(EFX_Rnd() * 6.2831854820251465 + 0.0);
				p->rotVar = 0.0f;
				p->rotIni = rot;
				p->rotEnd = rot;
			} else {
				p->rotIni = 0.0f;
				p->rotEnd = 0.0f;
				p->rotVar = 0.0f;
			}
			CGameParticles_Apply(p);
			CGameParticles_SetColorRampOn(p, 1);
			if (i < 3) {
				CGameParticles_SetColorRamp(p, g_RampMaintenance);
				CGameParticles_EmitN(p, 1, 0);
			} else {
				CGameParticles_SetColorRamp(p, g_RampMaintenanceEnd);
				CGameParticles_EmitDt(p, 1, 0, 0.01f);
			}
		}
	}
	self->lastT = self->t;
}

