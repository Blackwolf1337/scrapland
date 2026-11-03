/*
 * Entities/EmbeddedFX/EmbeddedFXChar.cpp of the game: the effects embedded in the character entities, see
 * include/scrap/embeddedfxchar.h.
 */
#include <float.h>
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/chartype.h"
#include "scrap/crt.h"
#include "scrap/embeddedfxchar.h"
#include "scrap/fxmisc.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/scrapcollision.h"
#include "scrap/timer.h"
#include "scrap/withlifeentity.h"
#include "scrap/staticinit.h"


static const cEmbeddedFXVtbl cEmbeddedFX_vftable;
#define VT_EMBEDDEDFX (&cEmbeddedFX_vftable)
static const cEmbeddedFXVtbl cEmbeddedFXSentinel_vftable;
#define VT_SENTINEL (&cEmbeddedFXSentinel_vftable)
static const cEmbeddedFXVtbl cEmbeddedFXGear_vftable;
#define VT_GEAR (&cEmbeddedFXGear_vftable)
static const cEmbeddedFXVtbl cEmbeddedFXPolice_vftable;
#define VT_POLICE (&cEmbeddedFXPolice_vftable)
static const cEmbeddedFXVtbl cEmbeddedFXBankDirector_vftable;
#define VT_BANKDIRECTOR (&cEmbeddedFXBankDirector_vftable)
static const cEmbeddedFXVtbl cEmbeddedFXBankMaster_vftable;
#define VT_BANKMASTER (&cEmbeddedFXBankMaster_vftable)
static const cEmbeddedFXVtbl cEmbeddedFXCrazyGambler_vftable;
#define VT_CRAZYGAMBLER (&cEmbeddedFXCrazyGambler_vftable)
static const cEmbeddedFXVtbl cEmbeddedFXRusty_vftable;
#define VT_RUSTY (&cEmbeddedFXRusty_vftable)
static const cEmbeddedFXVtbl cEmbeddedFXMessenger_vftable;
#define VT_MESSENGER (&cEmbeddedFXMessenger_vftable)
static const cEmbeddedFXVtbl cEmbeddedFXMaintenance_vftable;
#define VT_MAINTENANCE (&cEmbeddedFXMaintenance_vftable)
static const cEmbeddedFXVtbl cEmbeddedFXMercenary_vftable;
#define VT_MERCENARY (&cEmbeddedFXMercenary_vftable)

#define TRAIL_COLOR 0xc0c0ff
#define LASER_COLOR 0x800000

/* the color ramps of the alarm and maintenance particles (9 colors) */

/* rand() & 0x7fff as 0..1 (double) */
static inline double EFX_Rnd(void) {
	return (double)(crt_rand() & 0x7fff) * (1.0 / 32767.0);
}

static inline void EFX_TrailsDtor(CGameTrail *trails, uint32_t n) {
	crt_eh_vector_dtor(trails, sizeof(CGameTrail), n, (void *)CGameTrail_dtor);
}

static inline cEmbeddedFX *EFX_Free(cEmbeddedFX *self, byte flags, uint32_t size) {
	if (flags & 1) {
		operator_delete_sized(self, size);
	}
	return self;
}

/* the model of the entity, CHECKed */
static CGameModel *EFX_Model(cEmbeddedFX *self, cEntity *e) {
	self->ent = e;
	CGameModel *model = Obj_GetModel(e);
	CHECK(CGameModel_IsLoaded(model));
	return model;
}

/* the particles let go and attached again to the render node name (if the model has it) */
static int EFX_AttachOpt(CGameParticles *p, int *dummy, CGameModel *model, const char *name) {
	*dummy = -1;
	CGameParticles_Release(p);
	int i = CGameModel_FindRenderNode(model, name);
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
	crt_eh_vector_ctor(trails, sizeof(CGameTrail), n, (void *)CGameTrail_ctor, (void *)CGameTrail_dtor);
}

/* the original clears the vtable pointer before it stores the class one */
static inline void EFX_CtorVtbl(void *self, const cEmbeddedFXVtbl *vt) {
	((cEmbeddedFX *)self)->vftable = NULL;
	EFX_SetVtbl(self, vt);
}

static cEmbeddedFXAlarm *cEmbeddedFXAlarm_ctor(cEmbeddedFXAlarm *self) {
	CGameParticles_ctor(&self->particles);
	self->dummy = -1;
	self->level = 0.0f;
	return self;
}

static cEmbeddedFX *cEmbeddedFXBankDirector_ctor(void *p) {
	cEmbeddedFXBankDirector *self = p;
	EFX_CtorVtbl(self, VT_BANKDIRECTOR);
	CGameParticles_ctor(&self->glow);
	self->base.ent = NULL;
	self->dummyGlow = -1;
	self->on = 0;
	return &self->base;
}

static cEmbeddedFX *cEmbeddedFXBankMaster_ctor(void *p) {
	cEmbeddedFXBankMaster *self = p;
	EFX_CtorVtbl(self, VT_BANKMASTER);
	CGameParticles_ctor(&self->glow);
	CGameParticles_ctor(&self->baton);
	self->base.ent = NULL;
	self->dummyGlow = -1;
	self->dummyBaton = -1;
	self->on = 0;
	return &self->base;
}

static cEmbeddedFX *cEmbeddedFXCrazyGambler_ctor(void *p) {
	cEmbeddedFXCrazyGambler *self = p;
	EFX_CtorVtbl(self, VT_CRAZYGAMBLER);
	CGameParticles_ctor(&self->particles);
	EFX_TrailsCtor(self->trails, FXGAMBLER_ARCS);
	self->unused_2188 = 0;
	self->base.ent = NULL;
	self->arcT = 0.0f;
	self->t = 0.0f;
	self->visible = 0;
	self->model = NULL;
	self->nodeHead = -1;
	self->nodeHand = -1;
	return &self->base;
}

static cEmbeddedFX *cEmbeddedFXGear_ctor(void *p) {
	cEmbeddedFXGear *self = p;
	EFX_CtorVtbl(self, VT_GEAR);
	cEmbeddedFXAlarm_ctor(&self->alarm);
	CGameParticles_ctor(&self->shoot);
	CGameTrail_ctor(&self->trail);
	self->base.ent = NULL;
	self->dummyShoot = -1;
	return &self->base;
}

static cEmbeddedFX *cEmbeddedFXMaintenance_ctor(void *p) {
	cEmbeddedFXMaintenance *self = p;
	EFX_CtorVtbl(self, VT_MAINTENANCE);
	CGameParticles_ctor(&self->light);
	self->base.ent = NULL;
	self->t = 0.0f;
	self->lastT = 0.0f;
	return &self->base;
}

static cEmbeddedFX *cEmbeddedFXMercenary_ctor(void *p) {
	cEmbeddedFXMercenary *self = p;
	EFX_CtorVtbl(self, VT_MERCENARY);
	CGameParticles_ctor(&self->glow);
	CGameParticles_ctor(&self->bubbles);
	CGameParticles_ctor(&self->pipe);
	self->base.ent = NULL;
	self->dummyGlow = -1;
	self->dummyBubbles = -1;
	self->visible = 0;
	self->t = 0.0f;
	self->isL = 0;
	return &self->base;
}

static cEmbeddedFX *cEmbeddedFXMessenger_ctor(void *p) {
	cEmbeddedFXMessenger *self = p;
	EFX_CtorVtbl(self, VT_MESSENGER);
	CGameParticles_ctor(&self->glow);
	EFX_TrailsCtor(self->trails, FXEMBMESSENGER_TRAILS);
	self->unused_334 = 0;
	self->base.ent = NULL;
	self->unused_08 = 0;
	self->on = 0;
	return &self->base;
}

static cEmbeddedFX *cEmbeddedFXPolice_ctor(void *p) {
	cEmbeddedFXPolice *self = p;
	EFX_CtorVtbl(self, VT_POLICE);
	cEmbeddedFXAlarm_ctor(&self->alarm);
	self->base.ent = NULL;
	return &self->base;
}

static cEmbeddedFX *cEmbeddedFXRusty_ctor(void *p) {
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
	self->visible = 0;
	self->sparkT = 0.0f;
	return &self->base;
}

static cEmbeddedFX *cEmbeddedFXSentinel_ctor(void *p) {
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
	self->state = -1;
	self->lastState = -1;
	return &self->base;
}

/* ---- destructors ---- */

/* EmbeddedFX.h, in the exe after WalkCharEntity.cpp */
void cEmbeddedFX_dtor(cEmbeddedFX *self) {
	EFX_SetVtbl(self, VT_EMBEDDEDFX);
}

/* vf00 of the base class; it frees 4 bytes, the size of the vtable pointer alone */
static cEmbeddedFX *cEmbeddedFX_Delete(cEmbeddedFX *self, byte flags) {
	cEmbeddedFX_dtor(self);
	return EFX_Free(self, flags, 4);
}

static void cEmbeddedFXBankDirector_dtor(cEmbeddedFXBankDirector *self) {
	EFX_SetVtbl(self, VT_BANKDIRECTOR);
	CGameParticles_Release(&self->glow);
	cEmbeddedFX_dtor(&self->base);
}

static void cEmbeddedFXBankMaster_dtor(cEmbeddedFXBankMaster *self) {
	EFX_SetVtbl(self, VT_BANKMASTER);
	CGameParticles_Release(&self->baton);
	CGameParticles_Release(&self->glow);
	cEmbeddedFX_dtor(&self->base);
}

static void cEmbeddedFXCrazyGambler_dtor(cEmbeddedFXCrazyGambler *self) {
	EFX_SetVtbl(self, VT_CRAZYGAMBLER);
	EFX_TrailsDtor(self->trails, FXGAMBLER_ARCS);
	CGameParticles_Release(&self->particles);
	cEmbeddedFX_dtor(&self->base);
}

static void cEmbeddedFXGear_dtor(cEmbeddedFXGear *self) {
	EFX_SetVtbl(self, VT_GEAR);
	CGameTrail_dtor(&self->trail);
	CGameParticles_Release(&self->shoot);
	CGameParticles_Release(&self->alarm.particles);
	cEmbeddedFX_dtor(&self->base);
}

static void cEmbeddedFXMaintenance_dtor(cEmbeddedFXMaintenance *self) {
	EFX_SetVtbl(self, VT_MAINTENANCE);
	CGameParticles_Release(&self->light);
	cEmbeddedFX_dtor(&self->base);
}

static void cEmbeddedFXMercenary_dtor(cEmbeddedFXMercenary *self) {
	EFX_SetVtbl(self, VT_MERCENARY);
	CGameParticles_Release(&self->pipe);
	CGameParticles_Release(&self->bubbles);
	CGameParticles_Release(&self->glow);
	cEmbeddedFX_dtor(&self->base);
}

static void cEmbeddedFXMessenger_dtor(cEmbeddedFXMessenger *self) {
	EFX_SetVtbl(self, VT_MESSENGER);
	EFX_TrailsDtor(self->trails, FXEMBMESSENGER_TRAILS);
	CGameParticles_Release(&self->glow);
	cEmbeddedFX_dtor(&self->base);
}

static void cEmbeddedFXPolice_dtor(cEmbeddedFXPolice *self) {
	EFX_SetVtbl(self, VT_POLICE);
	CGameParticles_Release(&self->alarm.particles);
	cEmbeddedFX_dtor(&self->base);
}

static void cEmbeddedFXRusty_dtor(cEmbeddedFXRusty *self) {
	EFX_SetVtbl(self, VT_RUSTY);
	EFX_TrailsDtor(self->trails, FXRUSTY_TRAILS);
	CGameParticles_Release(&self->glowLighting);
	CGameParticles_Release(&self->sparks);
	CGameParticles_Release(&self->back);
	CGameParticles_Release(&self->front);
	cEmbeddedFX_dtor(&self->base);
}

static void cEmbeddedFXSentinel_dtor(cEmbeddedFXSentinel *self) {
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

static cEmbeddedFX *cEmbeddedFXBankDirector_Delete(cEmbeddedFX *elem, byte flags) {
	cEmbeddedFXBankDirector *self = (cEmbeddedFXBankDirector *)elem;
	cEmbeddedFXBankDirector_dtor(self);
	return EFX_Free(&self->base, flags, sizeof(*self));
}

static cEmbeddedFX *cEmbeddedFXBankMaster_Delete(cEmbeddedFX *elem, byte flags) {
	cEmbeddedFXBankMaster *self = (cEmbeddedFXBankMaster *)elem;
	cEmbeddedFXBankMaster_dtor(self);
	return EFX_Free(&self->base, flags, sizeof(*self));
}

static cEmbeddedFX *cEmbeddedFXCrazyGambler_Delete(cEmbeddedFX *elem, byte flags) {
	cEmbeddedFXCrazyGambler *self = (cEmbeddedFXCrazyGambler *)elem;
	cEmbeddedFXCrazyGambler_dtor(self);
	return EFX_Free(&self->base, flags, sizeof(*self));
}

static cEmbeddedFX *cEmbeddedFXGear_Delete(cEmbeddedFX *elem, byte flags) {
	cEmbeddedFXGear *self = (cEmbeddedFXGear *)elem;
	cEmbeddedFXGear_dtor(self);
	return EFX_Free(&self->base, flags, sizeof(*self));
}

static cEmbeddedFX *cEmbeddedFXMaintenance_Delete(cEmbeddedFX *elem, byte flags) {
	cEmbeddedFXMaintenance *self = (cEmbeddedFXMaintenance *)elem;
	cEmbeddedFXMaintenance_dtor(self);
	return EFX_Free(&self->base, flags, sizeof(*self));
}

static cEmbeddedFX *cEmbeddedFXMercenary_Delete(cEmbeddedFX *elem, byte flags) {
	cEmbeddedFXMercenary *self = (cEmbeddedFXMercenary *)elem;
	cEmbeddedFXMercenary_dtor(self);
	return EFX_Free(&self->base, flags, sizeof(*self));
}

static cEmbeddedFX *cEmbeddedFXMessenger_Delete(cEmbeddedFX *elem, byte flags) {
	cEmbeddedFXMessenger *self = (cEmbeddedFXMessenger *)elem;
	cEmbeddedFXMessenger_dtor(self);
	return EFX_Free(&self->base, flags, sizeof(*self));
}

static cEmbeddedFX *cEmbeddedFXPolice_Delete(cEmbeddedFX *elem, byte flags) {
	cEmbeddedFXPolice *self = (cEmbeddedFXPolice *)elem;
	cEmbeddedFXPolice_dtor(self);
	return EFX_Free(&self->base, flags, sizeof(*self));
}

static cEmbeddedFX *cEmbeddedFXRusty_Delete(cEmbeddedFX *elem, byte flags) {
	cEmbeddedFXRusty *self = (cEmbeddedFXRusty *)elem;
	cEmbeddedFXRusty_dtor(self);
	return EFX_Free(&self->base, flags, sizeof(*self));
}

static cEmbeddedFX *cEmbeddedFXSentinel_Delete(cEmbeddedFX *elem, byte flags) {
	cEmbeddedFXSentinel *self = (cEmbeddedFXSentinel *)elem;
	cEmbeddedFXSentinel_dtor(self);
	return EFX_Free(&self->base, flags, sizeof(*self));
}

/* ---- vf02: Release ---- */

static void cEmbeddedFXBankDirector_Release(cEmbeddedFX *base) {
	cEmbeddedFXBankDirector *self = (cEmbeddedFXBankDirector *)base;
	self->dummyGlow = -1;
	CGameParticles_Release(&self->glow);
}

static void cEmbeddedFXBankMaster_Release(cEmbeddedFX *base) {
	cEmbeddedFXBankMaster *self = (cEmbeddedFXBankMaster *)base;
	self->dummyGlow = -1;
	CGameParticles_Release(&self->glow);
	self->dummyBaton = -1;
	CGameParticles_Release(&self->baton);
}

static void cEmbeddedFXCrazyGambler_Release(cEmbeddedFX *base) {
	cEmbeddedFXCrazyGambler *self = (cEmbeddedFXCrazyGambler *)base;
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

static void cEmbeddedFXGear_Release(cEmbeddedFX *base) {
	cEmbeddedFXGear *self = (cEmbeddedFXGear *)base;
	self->alarm.dummy = -1;
	CGameParticles_Release(&self->alarm.particles);
	self->dummyShoot = -1;
	CGameParticles_Release(&self->shoot);
}

static void cEmbeddedFXMaintenance_Release(cEmbeddedFX *base) {
	cEmbeddedFXMaintenance *self = (cEmbeddedFXMaintenance *)base;
	CGameParticles_Release(&self->light);
}

static void cEmbeddedFXMercenary_Release(cEmbeddedFX *base) {
	cEmbeddedFXMercenary *self = (cEmbeddedFXMercenary *)base;
	self->dummyGlow = -1;
	CGameParticles_Release(&self->glow);
	self->dummyBubbles = -1;
	CGameParticles_Release(&self->bubbles);
}

static void cEmbeddedFXMessenger_Release(cEmbeddedFX *base) {
	cEmbeddedFXMessenger *self = (cEmbeddedFXMessenger *)base;
	self->unused_08 = 0;
	for (int i = 0; i < FXEMBMESSENGER_TRAILS; i++) {
		self->life[i] = 0.0f;
		CGameTrail_Clear(&self->trails[i]);
	}
	CGameParticles_Release(&self->glow);
}

static void cEmbeddedFXPolice_Release(cEmbeddedFX *base) {
	cEmbeddedFXPolice *self = (cEmbeddedFXPolice *)base;
	self->alarm.dummy = -1;
	CGameParticles_Release(&self->alarm.particles);
}

static void cEmbeddedFXRusty_Release(cEmbeddedFX *base) {
	cEmbeddedFXRusty *self = (cEmbeddedFXRusty *)base;
	self->dummyFront = -1;
	CGameParticles_Release(&self->front);
	self->dummyBack = -1;
	CGameParticles_Release(&self->back);
	self->dummySparks = -1;
	CGameParticles_Release(&self->sparks);
}

static void cEmbeddedFXSentinel_Release(cEmbeddedFX *base) {
	cEmbeddedFXSentinel *self = (cEmbeddedFXSentinel *)base;
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

static void cEmbeddedFXCrazyGambler_Clear(cEmbeddedFXCrazyGambler *self) {
	for (int i = 0; i < FXGAMBLER_ARCS; i++) {
		self->count[i] = 0;
		self->tail[i] = 0;
		self->head[i] = 0;
		memset(self->used[i], 0, 0x20);
		CGameTrail_Clear(&self->trails[i]);
	}
	CGameParticles_KillAll(&self->particles);
}

static void cEmbeddedFXMessenger_Clear(cEmbeddedFXMessenger *self) {
	for (int i = 0; i < FXEMBMESSENGER_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
	}
	CGameParticles_KillAll(&self->glow);
	memset(self->used, 0, sizeof(self->used));
}

static void cEmbeddedFXRusty_Clear(cEmbeddedFXRusty *self) {
	for (int i = 0; i < FXRUSTY_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
	}
	CGameParticles_KillAll(&self->sparks);
}

static void cEmbeddedFXSentinel_Clear(cEmbeddedFXSentinel *self) {
	for (int i = 0; i < FXSENTINEL_LASERS; i++) {
		CGameTrail_Clear(&self->beams[i]);
		CGameTrail_Clear(&self->edgesA[i]);
		CGameTrail_Clear(&self->edgesB[i]);
	}
	CGameParticles_KillAll(&self->laser);
	CGameParticles_KillAll(&self->status);
}

/* ---- vf05: Stop ---- */

static void cEmbeddedFXCrazyGambler_Stop(cEmbeddedFX *base, byte keep) {
	cEmbeddedFXCrazyGambler *self = (cEmbeddedFXCrazyGambler *)base;
	if (!keep) {
		cEmbeddedFXCrazyGambler_Clear(self);
	}
}

static void cEmbeddedFXMessenger_Stop(cEmbeddedFX *base, byte keep) {
	cEmbeddedFXMessenger *self = (cEmbeddedFXMessenger *)base;
	if (!keep) {
		cEmbeddedFXMessenger_Clear(self);
	}
}

static void cEmbeddedFXRusty_Stop(cEmbeddedFX *base, byte keep) {
	cEmbeddedFXRusty *self = (cEmbeddedFXRusty *)base;
	if (!keep) {
		cEmbeddedFXRusty_Clear(self);
	}
}

static void cEmbeddedFXSentinel_Stop(cEmbeddedFX *base, byte keep) {
	cEmbeddedFXSentinel *self = (cEmbeddedFXSentinel *)base;
	if (keep) {
		self->lastState = 0;
		return;
	}
	cEmbeddedFXSentinel_Clear(self);
}

/* ---- vf01: Reset ---- */

static byte cEmbeddedFXAlarm_Reset(cEmbeddedFXAlarm *self) {
	self->on = 0;
	self->cur = 0;
	self->level = 0.0f;
	self->t = 0.0f;
	self->lastT = 0.0f;
	self->lights = 1;
	return 1;
}

static byte cEmbeddedFXCrazyGambler_Reset(cEmbeddedFX *base) {
	cEmbeddedFXCrazyGambler *self = (cEmbeddedFXCrazyGambler *)base;
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
static byte cEmbeddedFXPolice_Reset(cEmbeddedFX *base) {
	cEmbeddedFXPolice *self = (cEmbeddedFXPolice *)base;
	return cEmbeddedFXAlarm_Reset(&self->alarm);
}

static byte cEmbeddedFXMercenary_Reset(cEmbeddedFX *base) {
	cEmbeddedFXMercenary *self = (cEmbeddedFXMercenary *)base;
	self->t = 0.0f;
	return 1;
}

static byte cEmbeddedFXMessenger_Reset(cEmbeddedFX *base) {
	cEmbeddedFXMessenger *self = (cEmbeddedFXMessenger *)base;
	for (int i = 0; i < FXEMBMESSENGER_TRAILS; i++) {
		self->life[i] = 0.0f;
		EFX_TrailStyle(&self->trails[i], 8, 0, 3, 1.0f, 0);
	}
	memset(self->used, 0, sizeof(self->used));
	return 1;
}

static byte cEmbeddedFXRusty_Reset(cEmbeddedFX *base) {
	cEmbeddedFXRusty *self = (cEmbeddedFXRusty *)base;
	for (int i = 0; i < FXRUSTY_TRAILS; i++) {
		self->life[i] = 0.0f;
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

static byte cEmbeddedFXSentinel_Reset(cEmbeddedFX *base) {
	cEmbeddedFXSentinel *self = (cEmbeddedFXSentinel *)base;
	self->numLasers = FXSENTINEL_LASERS;
	for (int i = 0; i < self->numLasers; i++) {
		EFX_LaserTrail(&self->beams[i], 3, 14.0f, 0);
		CGameTrail_SetType(&self->edgesA[i], TRAIL_FAN);
		EFX_LaserTrail(&self->edgesA[i], 11, 8.0f, 1);
		EFX_LaserTrail(&self->edgesB[i], 11, 8.0f, 1);
	}
	for (int i = 0; i < FXSENTINEL_LASERS; i++) {
		self->edgeT[i] = 0.0f;
	}
	for (int i = 0; i < FXSENTINEL_LASERS; i++) {
		self->phase[i].x = 0.0f;
		self->phase[i].y = 0.0f;
	}
	self->phase[0].z = 0.0f;
	self->phase[1].z = 1.5707964f;
	self->phase[2].z = 3.1415927f;
	self->phase[3].z = 4.712389f;
	self->unused_170 = 0.0f;
	self->alpha = 1.0f;
	cEmbeddedFXAlarm_Reset(&self->alarm);
	self->alarm.lights = 0;
	return 1;
}

/* ---- the alarm ---- */

static void cEmbeddedFXAlarm_SetOn(cEmbeddedFXAlarm *self, byte on) {
	self->cur = self->on;
	self->on = on;
}

static void cEmbeddedFXAlarm_Init(cEmbeddedFXAlarm *self, CGameModel *model) {
	if (!model) {
		CHECK(model);
		return;
	}
	if (!CGameModel_IsLoaded(model)) {
		return;
	}
	EFX_AttachOpt(&self->particles, &self->dummy, model, "DF_PoliceAlarm");
	CHECK(self->particles.ParticleSystem);
	EFX_Still(&self->particles);
	CGameParticles_Apply(&self->particles);
}

/* ---- Init: the effect on the model of the character ---- */

static void cEmbeddedFXBankDirector_Init(cEmbeddedFXBankDirector *self, cEntity *e) {
	CGameModel *model = EFX_Model(&self->base, e);

	self->dummyGlow = -1;
	CGameParticles_Release(&self->glow);
	self->dummyGlow = CGameModel_FindRenderNode(model, "DF_Glow");
	CHECK(self->dummyGlow > -1);
	CGameParticles_Attach(&self->glow, model, self->dummyGlow);
	CHECK(self->glow.ParticleSystem);
}

static void cEmbeddedFXBankMaster_Init(cEmbeddedFXBankMaster *self, cEntity *e) {
	CGameModel *model = EFX_Model(&self->base, e);

	self->dummyGlow = -1;
	CGameParticles_Release(&self->glow);
	self->dummyGlow = CGameModel_FindRenderNode(model, "DF_Glow");
	CHECK(self->dummyGlow > -1);
	CGameParticles_Attach(&self->glow, model, self->dummyGlow);
	CHECK(self->glow.ParticleSystem);
	self->dummyBaton = CGameModel_FindRenderNode(model, "DF_Glow_Baston");
	CHECK(self->dummyBaton > -1);
	CGameParticles_Attach(&self->baton, model, self->dummyBaton);
	CHECK(self->baton.ParticleSystem);
	/* a row of glows along the stick */
	for (int i = 0; i < 16; i++) {
		CGameParticles_SetParticle(&self->baton, i, 0.0f, (float)i * 8.0f - 74.0f, 0.0f, 65.0f, 0xe1f7d255, 2,
			0.0f);
	}
}

void cEmbeddedFXCrazyGambler_SetHead(cEmbeddedFXCrazyGambler *self, CGameModel *model,
	const char *node) {
	CHECK(model && CGameModel_IsLoaded(model));
	self->model = model;
	self->nodeHead = -1;
	self->nodeHead = CGameModel_FindRenderNode(model, node);
	CHECK(self->nodeHead > -1);
	cEmbeddedFXCrazyGambler_Clear(self);
	EFX_Update(self, 0.5f);
}

static void cEmbeddedFXCrazyGambler_Init(cEmbeddedFXCrazyGambler *self, cEntity *e) {
	CGameModel *model = EFX_Model(&self->base, e);

	self->nodeHand = CGameModel_FindRenderNode(model, "R_Gambler_mano_Izq");
	CHECK(self->nodeHand > -1);
	int glow = CGameModel_FindRenderNode(model, "DF_Glow");
	CHECK(glow > -1);
	CGameParticles_Attach(&self->particles, model, glow);
	CHECK(self->particles.ParticleSystem);
	cEmbeddedFXCrazyGambler_SetHead(self, model, "R_Gambler_Cabeza-Pelos");
}

static void cEmbeddedFXGear_Init(cEmbeddedFXGear *self, cEntity *e) {
	CGameModel *model = EFX_Model(&self->base, e);

	EFX_AttachOpt(&self->shoot, &self->dummyShoot, model, "DF_GearShoot");
	CHECK(self->shoot.ParticleSystem);
	cEmbeddedFXAlarm_Init(&self->alarm, model);
}

static void cEmbeddedFXMaintenance_Init(cEmbeddedFXMaintenance *self, cEntity *e) {
	CGameModel *model = EFX_Model(&self->base, e);

	CGameParticles_Attach(&self->light, model, CGameModel_FindRenderNode(model, "DF_Light"));
	CHECK(self->light.ParticleSystem);
}

static void cEmbeddedFXMercenary_Init(cEmbeddedFXMercenary *self, cEntity *e) {
	CGameModel *model = EFX_Model(&self->base, e);

	self->isL = crt_stricmp(CGameModel_Name(model), "MercenaryL.M3D") == 0;
	byte isL = self->isL;
	EFX_AttachOpt(&self->glow, &self->dummyGlow, model, "DF_Glow");
	CHECK(self->glow.ParticleSystem);
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
	CHECK(self->bubbles.ParticleSystem);
	self->dummyPipe = -1;
	if (self->isL) {
		EFX_AttachOpt(&self->pipe, &self->dummyPipe, model, "DF_Burbujas_Pipa");
		CHECK(self->pipe.ParticleSystem);
	}
}

static void cEmbeddedFXMessenger_Init(cEmbeddedFXMessenger *self, cEntity *e) {
	CGameModel *model = EFX_Model(&self->base, e);
	char name[256];

	self->dummyGlow = CGameModel_FindRenderNode(model, "DF_GlowLighting");
	CHECK(self->dummyGlow > -1);
	if (self->dummyGlow != -1) {
		CGameParticles_Attach(&self->glow, model, self->dummyGlow);
	}
	CHECK(self->glow.ParticleSystem);
	for (int i = 0; i < FXMESSENGER_RAYS; i++) {
		Str_PrintfQuiet(name, 0xff, "DF_Rayos_%02d", i + 1);
		self->nodeRays[i] = CGameModel_FindRenderNode(model, name);
		CHECK(self->nodeRays[i] > -1);
	}
	for (int i = 0; i < FXMESSENGER_RAYS; i++) {
		self->order[i] = i;
	}
}

static void cEmbeddedFXPolice_Init(cEmbeddedFXPolice *self, cEntity *e) {
	CGameModel *model = EFX_Model(&self->base, e);

	cEmbeddedFXAlarm_Init(&self->alarm, model);
}

static void cEmbeddedFXRusty_Init(cEmbeddedFXRusty *self, cEntity *e) {
	CGameModel *model = EFX_Model(&self->base, e);

	EFX_AttachOpt(&self->sparks, &self->dummySparks, model, "DR_Mecanico_ColetaChispas");
	CHECK(self->sparks.ParticleSystem);
	EFX_AttachOpt(&self->front, &self->dummyFront, model, "DR_Mecanico_PechoGlowA");
	CHECK(self->front.ParticleSystem);
	EFX_AttachOpt(&self->back, &self->dummyBack, model, "DR_Mecanico_PechoGlowB");
	CHECK(self->back.ParticleSystem);
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
	self->dummyGlowLighting = CGameModel_FindRenderNode(model, "DF_GlowLighting");
	CHECK(self->dummyGlowLighting > -1);
	if (self->dummyGlowLighting != -1) {
		CGameParticles_Attach(&self->glowLighting, model, self->dummyGlowLighting);
	}
	CHECK(self->glowLighting.ParticleSystem);
	self->dummyLighting[0] = CGameModel_FindRenderNode(model, "DF_LightingA");
	CHECK(self->dummyLighting[0] > -1);
	self->dummyLighting[1] = CGameModel_FindRenderNode(model, "DF_LightingC");
	CHECK(self->dummyLighting[1] > -1);
	self->dummyLighting[2] = CGameModel_FindRenderNode(model, "DF_LightingB");
	CHECK(self->dummyLighting[2] > -1);
}

static void cEmbeddedFXSentinel_Init(cEmbeddedFXSentinel *self, cEntity *e) {
	self->base.ent = e;
	self->dummyStatus = -1;
	CGameParticles_Release(&self->status);
	self->dummyLaser = -1;
	CGameParticles_Release(&self->laser);
	CGameModel *model = Obj_GetModel(self->base.ent);
	CHECK(CGameModel_IsLoaded(model));
	self->dummyLaser = CGameModel_FindRenderNode(model, "DF_LaserScaner");
	CHECK(self->dummyLaser > -1);
	CGameParticles_Attach(&self->laser, model, self->dummyLaser);
	CHECK(self->laser.ParticleSystem);
	self->dummyStatus = CGameModel_FindRenderNode(model, "DF_StatusLight");
	CHECK(self->dummyStatus > -1);
	CGameParticles_Attach(&self->status, model, self->dummyStatus);
	CHECK(self->status.ParticleSystem);
	cEmbeddedFXAlarm_Init(&self->alarm, model);
}

/* ---- creation ---- */

typedef cEmbeddedFX *(*EFX_Ctor)(void *self);
typedef void(*EFX_Init)(cEmbeddedFX *self, cEntity *e);

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
			EFX_Release(cur);
			EFX_Delete(cur, 1);
		}
		void *p = operator_new(efx_kinds[i].size);
		cur = p ? efx_kinds[i].ctor(p) : NULL;
		EFX_Reset(cur);
		efx_kinds[i].init(cur, withLife);
		break;
	}
	*fx = cur;
}

/* the alarm of the police or gear character name */
static void EmbeddedFXChar_SetAlarm(const char *name, byte on) {
	cEntity *v = World_FindEntity(name);

	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_WITH_LIFE));
	CHECK(CGameModel_IsLoaded(Obj_GetModel(v)));
	if (!strstr(CGameModel_Name(Obj_GetModel(v)), "Police")
		&& !strstr(CGameModel_Name(Obj_GetModel(v)), "Gear")) {
		return;
	}
	cEmbeddedFXAlarm_SetOn(&((cEmbeddedFXPolice *)Obj(v)->fx)->alarm, on);
}

/* the network messages: the character name, a second string and 4 bytes */
static void EFX_ReadAlarm(cAbstractFile *f, byte on) {
	char name[128];
	char other[128];
	int unused;

	cAbstractFile_ReadString(f, name, sizeof(name));
	cAbstractFile_ReadString(f, other, sizeof(other));
	VFile_Read(f, &unused, 4);
	EmbeddedFXChar_SetAlarm(name, on);
}

static void EmbeddedFXChar_AlarmOff(cAbstractFile *f) {
	EFX_ReadAlarm(f, 0);
}

static void EmbeddedFXChar_AlarmOn(cAbstractFile *f) {
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

static void cEmbeddedFXBankDirector_Update(cEmbeddedFX *elem, float dt) {
	cEmbeddedFXBankDirector *self = (cEmbeddedFXBankDirector *)elem;
	CGameModel *model = Obj_GetModel(self->base.ent);
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

static void cEmbeddedFXBankMaster_Update(cEmbeddedFX *elem, float dt) {
	cEmbeddedFXBankMaster *self = (cEmbeddedFXBankMaster *)elem;
	CGameModel *model = Obj_GetModel(self->base.ent);
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
	CGameParticles_SetAlpha(&self->baton, (float)(EFX_Rnd() * 0.1499999761581421 + 0.8500000238418579));
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

static uint32_t g_RampAlarmB[9] = { 0xff0000ff, 0x400000ff, 0x400000ff, 0x5c0000ff, 0x800000ff, 0xa40000ff, 0xc40000ff, 0xff0000ff, 0xff0000ff };

static uint32_t g_RampAlarmA[9] = { 0xff8080ff, 0x408080ff, 0x408080ff, 0x5c8080ff, 0x808080ff, 0xa48080ff, 0xc48080ff, 0xff8080ff, 0xff8080ff };

static uint32_t g_RampAlarmD[9] = { 0xffff0000, 0x40ff0000, 0x40ff0000, 0x5cff0000, 0x80ff0000, 0xa4ff0000, 0xc4ff0000, 0xffff0000, 0xffff0000 };

static uint32_t g_RampAlarmC[27] = {
	0xffff8080, 0x40ff8080, 0x40ff8080, 0x5cff8080, 0x80ff8080, 0xa4ff8080, 0xc4ff8080, 0xffff8080, 0xffff8080,
	0xff0000ff, 0x400000ff, 0x200000ff, 0x400000ff, 0x800000ff, 0xa40000ff, 0xc40000ff, 0xe00000ff, 0xff0000ff,
	0xffc4c4ff, 0x80c4c4ff, 0x40c4c4ff, 0x60c4c4ff, 0x80c4c4ff, 0xa4c4c4ff, 0xc4c4c4ff, 0xe0c4c4ff, 0xffc4c4ff,
};

/* blinks twice in the period of 0.9 s while on; the level fades to the state */
static void cEmbeddedFXAlarm_Update(cEmbeddedFXAlarm *self, float dt) {
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

static void cEmbeddedFXPolice_Update(cEmbeddedFX *base, float dt) {
	cEmbeddedFXPolice *self = (cEmbeddedFXPolice *)base;
	cEmbeddedFXAlarm_Update(&self->alarm, dt);
}

static uint32_t g_RampMaintenanceEnd[9] = { 0x30ff8000, 0x40ff8000, 0x4bff8000, 0x5cff8000, 0x80ff8000, 0xa4ff8000, 0xc4ff8000, 0xffff8000, 0xffff8000 };

static uint32_t g_RampMaintenance[9] = { 0x10ffc480, 0x20ffc464, 0x30ffba5a, 0x5cffaa46, 0x80ffa032, 0xa4ff9626, 0xc4ff8c12, 0xffff8700, 0xffff8000 };

/* a light blinks every 1.2 s */
static void cEmbeddedFXMaintenance_Update(cEmbeddedFX *base, float dt) {
	cEmbeddedFXMaintenance *self = (cEmbeddedFXMaintenance *)base;
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

static cGVar GVar_SentinelLaserScanerDist;
#define g_SentinelLaserScanerDist (GVar_SentinelLaserScanerDist.value.f)	/* "SentinelLaserScanerDist" */
cGVar GVar_SentinelStdFar;

#define g_SentinelStdFar (GVar_SentinelStdFar.value.f)
cGVar GVar_SentinelStdFovY;

#define g_SentinelStdFovY (GVar_SentinelStdFovY.value.f)
cGVar GVar_SentinelStdFovX;

#define g_SentinelStdFovX (GVar_SentinelStdFovX.value.f)

static void EFX_SentinelClear(cEmbeddedFXSentinel *self) {
	for (int i = 0; i < self->numLasers; i++) {
		CGameTrail_Clear(&self->beams[i]);
		CGameTrail_Clear(&self->edgesA[i]);
		CGameTrail_Clear(&self->edgesB[i]);
	}
}

/*
 * The alarm by the alarm of the world (not for a sentinel with a camera), the status light by the cone state and the
 * laser sweep of the scanner while it searches (alert: one laser, danger: four) and the camera is near or it is seen.
 */
static void cEmbeddedFXSentinel_Update(cEmbeddedFX *elem, float dt) {
	cEmbeddedFXSentinel *self = (cEmbeddedFXSentinel *)elem;
	cEntity *e = self->base.ent;
	byte on = g_pWorld->alarm.on;

	if (WLife(e)->obj.cam) {
		on = 0;
	}
	cEmbeddedFXAlarm_SetOn(&self->alarm, on);
	if (on) {
		cEmbeddedFXAlarm_Update(&self->alarm, dt);
	}
	if (self->dummyStatus == -1 || self->dummyLaser == -1) {
		return;
	}
	self->state = WLife(e)->arrow.coneStatus;
	/* a player sentinel in its "Action..." actions: danger with a target, else alert */
	if (Entity_IsType(e, TYEN_WALKCHAR) && cWalkCharEntity_GetOwnerSlot(e) != -1
		&& !crt_strncmp(cActCtrl_GetActionName(WithLife_GetActCtrl(e), 0), "Action", 6)) {
		self->state = (ChType_SentinelTarget(e) != NULL) + 2;
	} else if (self->state == 0 && Entity_GetAI(e)) {
		cAIChar *ai = Entity_GetAI(e);
		if (AIChar_GetState(ai) == AIC_CHASE_WITH_ACTION) {
			self->state = 3;
		}
	}
	if (WLife(e)->life < 1) {
		self->state = 0;
	}
	int state = self->state;
	uint32_t base = state == 2 ? 0xffe620 : 0xff0000;
	if (state != self->lastState) {
		/* the colors of the two status particles; the original leaves them unset for other states */
		static const uint32_t colors[4][2] = {
			{ 0x800000ff, 0xff000000 }, { 0x10ffffff, 0x3000ff00 }, { 0x10ffffff, 0x30ffff00 },
			{ 0x10ffffff, 0x30ff0000 },
		};
		uint32_t c0 = 0, c1 = 0;
		if ((uint32_t)state <= 3) {
			c0 = colors[state][0];
			c1 = colors[state][1];
		}
		if (state == 0) {
			CGameParticles_KillRange(&self->status, 0, 2);
		} else {
			CGameParticles_SetParticle(&self->status, 0, 0.0f, 0.0f, 0.0f, 32.0f, c0, 1, 0.0f);
			CGameParticles_SetParticle(&self->status, 1, 0.0f, 0.0f, 0.0f, 135.0f, c1, 5, 0.0f);
		}
		if (self->state == 2 || self->state == 3) {
			CGameParticles_SetParticle(&self->laser, 0, 0.0f, 0.0f, 0.0f, 65.0f, base | 0x30000000, 5, 0.0f);
		} else {
			CGameParticles_Kill(&self->laser, 0);
		}
	}
	CGameModel *model = Obj_GetModel(e);
	CHECK(CGameModel_IsLoaded(model));
	vec3_t cam = g_pWorld->camPos[g_pWorld->curCam];
	vec3_t tmp;
	const vec3_t *p = CGameModel_GetPos(model, &tmp);
	float dx = cam.x - p->x;
	float dy = cam.y - p->y;
	float dz = cam.z - p->z;
	float d2 = dy * dy + dx * dx + dz * dz;
	byte sweep = 0;
	if (self->state == 2 || self->state == 3) {
		if (CGameModel_IsVisible(model)) {
			sweep = 1;
		} else if (!CGameModel_IsVisible(model) && g_SentinelLaserScanerDist * g_SentinelLaserScanerDist > d2) {
			sweep = 1;
		}
	}
	if (!sweep) {
		if (CGameTrail_IsRender(&self->beams[0])) {
			EFX_SentinelClear(self);
		}
		self->lastState = self->state;
		return;
	}
	float step = dt * 5.0f;
	Float_Approach(&self->alpha, 0.0f, step);
	if (self->state != self->lastState) {
		self->alpha = 1.0f;
		EFX_SentinelClear(self);
		self->phase[0].x = 0.0f;
		self->phase[1].x = 0.0f;
		self->phase[2].x = 3.1415927f;
		self->phase[3].x = 3.1415927f;
		self->phase[0].y = 0.0f;
		self->phase[1].y = 3.1415927f;
		self->phase[2].y = 0.0f;
		self->phase[3].y = 3.1415927f;
		self->phase[0].z = 0.0f;
		self->phase[1].z = 1.5707964f;
		self->phase[2].z = 0.0f;
		self->phase[3].z = 1.5707964f;
	}
	model = Obj_GetModel(e);
	CHECK(CGameModel_IsLoaded(model));
	matrix_t mbuf;
	const matrix_t *m = CGameModel_NodeMatrix(model, &mbuf, self->dummyLaser, 1, NULL, 0);
	vec3_t org = { m->m[12], m->m[13], m->m[14] };
	vec3_t viewAng, ang;
	Obj_GetViewAng(e, &viewAng);
	Obj_GetSndAng(e, &ang);
	uint32_t colBeam = base | 0x78000000;
	uint32_t colEnd = base | 0x96000000;
	for (int i = 0; i < (self->state == 2 ? 1 : FXSENTINEL_LASERS); i++) {
		vec3_t *ph = &self->phase[i];
		if (self->state == 3) {
			ph->x = ph->x + dt;
			ph->y = step + ph->y;
		} else if (self->state == 2) {
			ph->x = 1.5707964f;
			ph->y = step + ph->y;
		}
		float ax = (float)cos((double)ph->x) * g_SentinelStdFovX * 0.25f;
		float ay = (float)cos((double)ph->y) * g_SentinelStdFovY * 0.25f;
		vec3_t rot = { ax + 0.0f, ay + 0.0f, 0.0f };
		const vec3_t fwd = { 0.0f, 0.0f, -1.0f };
		vec3_t v, dir, to, hit;
		Vec3_Rotate(&v, &fwd, &rot);
		Vec3_Rotate(&dir, &v, &ang);
		vec3_t from = org;
		float len = g_SentinelStdFar;
		to.x = dir.x * len + org.x;
		to.y = dir.y * len + org.y;
		to.z = dir.z * len + org.z;
		if (Engine_TestLine(&from, &to, &hit, NULL)) {
			float hx = from.x - hit.x;
			float hy = from.y - hit.y;
			float hz = from.z - hit.z;
			len = (float)__builtin_sqrt((double)(hy * hy + hx * hx + hz * hz));
		}
		CGameTrail_Add(&self->beams[i], from, 4.0f, colBeam);
		vec3_t end = { dir.x * len + from.x, dir.y * len + from.y, dir.z * len + from.z };
		CGameTrail_Add(&self->beams[i], end, 4.0f, colEnd);
		CGameTrail_SetTransp(&self->beams[i], (byte)(int)(self->alpha * 255.0f));
		float t = self->edgeT[i] + dt;
		self->edgeT[i] = t;
		if (t > 0.016f) {
			self->edgeT[i] = 0.0f;
			CGameTrail_SetLastPos(&self->edgesA[i], end);
			CGameTrail_Add(&self->edgesA[i], from, 0.0f, base | 0xbe000000);
			CGameTrail_AddPosColor(&self->edgesB[i], end,
				g_SentinelStdFar > len ? base | 0x80000000 : base | 0xc8000000);
		} else {
			CGameTrail_SetLastPos(&self->edgesA[i], from);
			CGameTrail_SetPoint(&self->edgesA[i], 1, &end, NULL, NULL);
			CGameTrail_SetLastPos(&self->edgesB[i], end);
		}
		CGameTrail_SetTransp(&self->edgesA[i], (byte)(int)(self->alpha * 255.0f));
		CGameTrail_SetTransp(&self->edgesB[i], (byte)(int)(self->alpha * 255.0f));
	}
	self->lastState = self->state;
}

/* the vertex at the head of arc i out of its set, the head one on in the ring of 0x37 */
static void EFX_GamblerDrop(cEmbeddedFXCrazyGambler *self, int i) {
	uint32_t v = (uint32_t)self->vertex[i][self->head[i]];

	if (v <= 0xff) {
		self->used[i][v >> 4] &= (uint16_t)~(1u << (v & 0xf));
	}
	self->head[i] = self->head[i] + 1 < FXGAMBLER_POINTS ? self->head[i] + 1 : 0;
}

/* a vertex vp of the model as a point of an arc: into the model frame, pushed out from the center by 1.2..1.8 */
static vec3_t EFX_GamblerPoint(const vec3_t *vp, const vec3_t *pos, const vec3_t *rot) {
	vec3_t d, q;

	d.x = vp->x - pos->x;
	d.y = vp->y - pos->y;
	d.z = vp->z - pos->z;
	vec3_t dir = FX_Unit(d);
	Vec3_RotateInv(&q, &d, rot);
	float r = FX_Rand(0.5999999046325684, 1.2000000476837158);
	q.x = q.x + r * dir.x;
	q.y = q.y + r * dir.y;
	q.z = q.z + r * dir.z;
	return q;
}

/*
 * The lightning of the crazy gambler: 8 arcs (5 on the hair, 3 on the left hand while it plays Relax1 between 1.5
 * and 5 s) that crawl every 0.02 s to the next free vertex of the node, drawn as Bezier trails of their last points
 * with two glows at the end. An arc lives 0.6..1.4 s, then starts again at a random vertex.
 */
static void cEmbeddedFXCrazyGambler_Update(cEmbeddedFX *elem, float dt) {
	cEmbeddedFXCrazyGambler *self = (cEmbeddedFXCrazyGambler *)elem;
	cEntity *e = self->base.ent;
	CGameModel *model = Obj_GetModel(e);
	vec3_t pos, rot, last = { 0.0f, 0.0f, 0.0f };
	vec3_t ctrl[GAMETRAIL_MAX_BEZIER], coefs[GAMETRAIL_MAX_BEZIER];
	vec3_t verts[0x20];
	int idx[0x20];

	if (!CGameModel_IsLoaded(model)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(model));
	byte vis = CGameModel_IsVisible(model);
	if (!vis) {
		if (self->visible) {
			cEmbeddedFXCrazyGambler_Clear(self);
		}
		return;
	}
	self->visible = vis;
	byte hand = cActCtrl_IsActive(WithLife_GetActCtrl(e), "Relax1");
	if (hand) {
		const cActLayer *l = cActCtrl_GetLayer(WithLife_GetActCtrl(e), 0);
		if (1.5f > l->time || l->time > 5.0f) {
			hand = 0;
		}
	}
	CGameModel_GetPos(model, &pos);
	CGameModel_GetRot(model, &rot);
	float arcT = self->arcT + dt;
	float t = self->t + dt;
	self->arcT = arcT;
	self->t = t;
	if (arcT > 0.05f) {
		CGameModel *head = self->model;
		CHECK(head && CGameModel_IsLoaded(head));
		CHECK(self->nodeHead > -1);
		for (int i = 0; i < FXGAMBLER_ARCS; i++) {
			CGameTrail *trail = &self->trails[i];
			int node = i > 4 ? self->nodeHand : self->nodeHead;
			if (!(self->life[i] > 0.0f)) {
				if (i > 4 && !hand) {
					continue;
				}
				self->life[i] = FX_Rand(0.7999999523162842, 0.6000000238418579);
				int v;
				vec3_t vp;
				CGameModel_Vertices(head, &vp, NULL, NULL, 1, -1, &v, node, NULL);
				vec3_t q = EFX_GamblerPoint(&vp, &pos, &rot);
				self->points[i][0] = q;
				self->vertex[i][0] = v;
				self->count[i] = 0;
				self->tail[i] = 0;
				self->head[i] = 0;
				memset(self->used[i], 0, 0x20);
				continue;
			}
			float life = self->life[i] - dt;
			self->life[i] = life;
			if (life > 0.0f && self->count[i] > 1) {
				int fade = 0xff - (int)(Float_MinSS(1.0f, life * 4.0f) * 255.0f);
				CGameTrail_Reset(trail);
				CGameTrail_SetTransp(trail, (byte)fade);
				int n = self->count[i] > 6 ? 6 : self->count[i] - 1;
				int k = self->tail[i] - n;
				if (k <= 0) {
					k = k + 0x36;
				}
				for (int j = 0; j < n; j++) {
					vec3_t r;
					const vec3_t *p = Vec3_Rotate(&r, &self->points[i][k], &rot);
					k++;
					ctrl[j].x = pos.x + p->x;
					ctrl[j].y = p->y + pos.y;
					ctrl[j].z = p->z + pos.z;
					if (k >= FXGAMBLER_POINTS) {
						k = 1;
					}
				}
				CGameTrail_BezierCoefs(trail, n, ctrl, coefs);
				int segs = CGameTrail_GetMaxTrails(trail) - 1;
				if (segs > 0) {
					int lastSeg = segs - 1;
					float span = (float)lastSeg;
					for (int j = 0; j < segs; j++) {
						vec3_t p, dir;
						CGameTrail_BezierPoint(trail, n, coefs, &p, (float)j / span);
						FX_RandomDir(&dir);
						float r = FX_Rand(1.5, 0.0);
						p.x = p.x + dir.x * r;
						p.y = p.y + dir.y * r;
						p.z = p.z + dir.z * r;
						float w = FX_Rand(0.5, 2.0);
						CGameTrail_Add(trail, p, w, 0x1080ff80);
						if (j == lastSeg) {
							last = p;
						}
					}
				}
				int a = fade + 0xa4 > 0xff ? 0xff : fade + 0xa4;
				CGameParticles_SetParticle(&self->particles, 2 * i, last.x, last.y, last.z, 32.0f,
					((uint32_t)a << 24) | 0x20ff20, 5, 0.0f);
				a = fade + 0x20 > 0xff ? 0xff : fade + 0x20;
				CGameParticles_SetParticle(&self->particles, 2 * i + 1, last.x, last.y, last.z, 3.0f,
					((uint32_t)a << 24) | 0x40ff40, 2, 0.0f);
			}
			if (self->life[i] <= 0.0f) {
				CGameTrail_Clear(trail);
				CGameParticles_KillRange(&self->particles, 2 * i, 2 * i + 2);
			}
			if (!(self->life[i] > 0.0f) || !(self->t > 0.02f)) {
				continue;
			}
			/* the step: the nearest free vertex of 32 random ones, further than 10 from the tail */
			vec3_t from = { 0.0f, 0.0f, 0.0f };
			if (self->count[i] > 0) {
				from = self->points[i][self->tail[i]];
			}
			int best = -1, bestV = 0;
			float bestD = FLT_MAX;
			vec3_t bestQ = { 0.0f, 0.0f, 0.0f };
			CGameModel_Vertices(head, verts, NULL, NULL, 0x20, -1, idx, node, NULL);
			for (int s = 0; s < 0x20; s++) {
				int v = idx[s];
				if (v > 0x100 || (self->used[i][v >> 4] & (1 << (v & 0xf)))) {
					continue;
				}
				vec3_t q = EFX_GamblerPoint(&verts[s], &pos, &rot);
				float dx = q.x - from.x;
				float dy = q.y - from.y;
				float dz = q.z - from.z;
				float dd = dy * dy + dx * dx + dz * dz;
				if (bestD >= dd && dd > 100.0f) {
					best = s;
					bestD = dd;
					bestV = idx[s];
					bestQ = q;
				}
			}
			if (best < 0) {
				if (self->count[i] > 0) {
					EFX_GamblerDrop(self, i);
					self->count[i]--;
				}
				continue;
			}
			self->vertex[i][self->tail[i] + 1] = bestV;
			self->points[i][self->tail[i] + 1] = bestQ;
			if (self->count[i] < FXGAMBLER_POINTS) {
				self->count[i]++;
			} else {
				EFX_GamblerDrop(self, i);
			}
			self->tail[i] = self->tail[i] + 1 < FXGAMBLER_POINTS ? self->tail[i] + 1 : 0;
			int v = self->vertex[i][self->tail[i]];
			self->used[i][v >> 4] |= (uint16_t)(1u << (v & 0xf));
		}
	}
	if (self->t > 0.02f) {
		self->t = 0.0f;
	}
}

static cGVar GVar_MessengerLightingDist;
#define g_MessengerLightingDist (GVar_MessengerLightingDist.value.f)	/* "MessengerLightingDist" */

/* the flag of vertex v of ray r (v may be -1: the byte before the ray) */
static inline byte *EFX_MessengerUsed(cEmbeddedFXMessenger *self, int r, int v) {
	return &self->used[0][r * 16 + v];
}

/* the lightning is shown: alive, visible, no model effect, near the camera and not in the Relax action */
static byte EFX_MessengerShown(cEmbeddedFXMessenger *self, CGameModel *model, byte vis) {
	cEntity *e = self->base.ent;
	vec3_t buf;

	if (WLife(e)->life <= 0 || !vis || CGameModel_GetEffect(model, NULL)) {
		return 0;
	}
	cWorld *w = g_pWorld;
	vec3_t cam = w->camPos[w->curCam];
	const vec3_t *p = Obj_GetViewPos(e, &buf);
	float dx = p->x - cam.x;
	float dy = p->y - cam.y;
	float dz = p->z - cam.z;
	if (dy * dy + dx * dx + dz * dz > g_MessengerLightingDist * g_MessengerLightingDist) {
		return 0;
	}
	if (WithLife_GetActCtrl(e)
		&& !strcmp(cActCtrl_GetActionName(WithLife_GetActCtrl(e), 0), "Relax")) {
		return 0;
	}
	return 1;
}

/* a new bolt i between a random vertex of one ray and the nearest free one of the first 10 of its pair; dead (life
 * 0) when there is none further than sqrt 80, all are nearer than 10 or one end is taken */
static void EFX_MessengerBolt(cEmbeddedFXMessenger *self, CGameModel *model, int i) {
	int *ray = self->rays[i];
	int *vert = self->verts[i];
	vec3_t a, cand[10];

	self->life[i] = FX_Rand(0.18000000715255737, 0.11999999731779099);
	int k = crt_rand() % 4;
	if (crt_rand() & 1) {
		ray[0] = self->order[2 * k];
		ray[1] = self->order[2 * k + 1];
	} else {
		ray[0] = self->order[2 * k + 1];
		ray[1] = self->order[2 * k];
	}
	CGameModel_Vertices(model, &a, NULL, NULL, 1, -1, &vert[0], self->nodeRays[ray[0]], NULL);
	CGameModel_Vertices(model, cand, NULL, NULL, 10, 0, NULL, self->nodeRays[ray[1]], NULL);
	int best = -1;
	float minD = FLT_MAX, bestD = FLT_MAX;
	for (int j = 0; j < 10; j++) {
		if (*EFX_MessengerUsed(self, ray[1], j)) {
			continue;
		}
		float dx = a.x - cand[j].x;
		float dy = a.y - cand[j].y;
		float dz = a.z - cand[j].z;
		float d = dy * dy + dx * dx + dz * dz;
		minD = Float_MinSS(d, minD);
		if (d > 80.0f && bestD >= d) {
			bestD = d;
			best = j;
		}
	}
	vert[1] = best;
	float r1 = FX_Rand(6.2831854820251465, 0.0);
	float r2 = FX_Rand(6.2831854820251465, 0.0);
	float r3 = FX_Rand(6.2831854820251465, 0.0);
	const vec3_t one = { 1.0f, 1.0f, 1.0f };
	vec3_t ang = { r3, r2, r1 }, v;
	Vec3_Rotate(&v, &one, &ang);
	float s = FX_Rand(5.0, 4.0);
	self->offs[i].x = v.x * s;
	self->offs[i].y = v.y * s;
	self->offs[i].z = v.z * s;
	if (best == -1 || 100.0f > minD || *EFX_MessengerUsed(self, ray[0], vert[0])
		|| *EFX_MessengerUsed(self, ray[1], vert[1])) {
		self->life[i] = 0.0f;
	}
}

/* the 12 lightning bolts of the messenger between the vertices of the pairs of its rays, bent Bezier trails that
 * live 0.12..0.3 s, with glows at both ends */
static void cEmbeddedFXMessenger_Update(cEmbeddedFX *elem, float dt) {
	cEmbeddedFXMessenger *self = (cEmbeddedFXMessenger *)elem;
	cEntity *e = self->base.ent;
	CGameModel *model = Obj_GetModel(e);
	vec3_t pts[3], coefs[GAMETRAIL_MAX_BEZIER];

	byte vis = 0;
	if (CGameModel_IsLoaded(model)) {
		vis = CGameModel_IsVisible(model);
	}
	if (!EFX_MessengerShown(self, model, vis)) {
		if (self->on) {
			cEmbeddedFXMessenger_Clear(self);
			self->on = 0;
		}
		return;
	}
	self->on = vis;
	for (int i = 0; i < FXEMBMESSENGER_TRAILS; i++) {
		CGameTrail *trail = &self->trails[i];
		int *ray = self->rays[i];
		int *vert = self->verts[i];
		if (!(self->life[i] > 0.0f)) {
			EFX_MessengerBolt(self, model, i);
			continue;
		}
		float life = self->life[i] - dt;
		self->life[i] = life;
		if (life > 0.0f) {
			int fade = 0xff - (int)(Float_MinSS(1.0f, life / 0.15f) * 255.0f);
			CGameTrail_Reset(trail);
			CGameTrail_SetTransp(trail, (byte)fade);
			*EFX_MessengerUsed(self, ray[0], vert[0]) = 1;
			*EFX_MessengerUsed(self, ray[1], vert[1]) = 1;
			CGameModel_Vertices(model, &pts[0], NULL, NULL, 1, vert[0], NULL, self->nodeRays[ray[0]], NULL);
			CGameModel_Vertices(model, &pts[2], NULL, NULL, 1, vert[1], NULL, self->nodeRays[ray[1]], NULL);
			pts[1].z = pts[0].z + (pts[2].z - pts[0].z) * 0.5f + self->offs[i].z;
			pts[1].y = self->offs[i].y + (pts[0].y + (pts[2].y - pts[0].y) * 0.5f);
			pts[1].x = self->offs[i].x + (pts[0].x + (pts[2].x - pts[0].x) * 0.5f);
			CGameTrail_BezierCoefs(trail, 3, pts, coefs);
			int m = CGameTrail_GetMaxTrails(trail);
			if (m - 1 > 0) {
				float span = (float)(m - 2);
				for (int j = 0; j < m - 1; j++) {
					vec3_t p, buf;
					CGameTrail_BezierPoint(trail, 3, coefs, &p, (float)j / span);
					if (j > 0 && j < m - 2) {
						float r = FX_Rand(1.0999999046325684, 0.30000001192092896);
						const vec3_t *d = FX_RandomDir(&buf);
						p.x = p.x + d->x * r;
						p.y = p.y + d->y * r;
						p.z = p.z + d->z * r;
					}
					float w = FX_Rand(1.0, 2.0);
					CGameTrail_Add(trail, p, w, 0x18a4a4ff);
				}
			}
			int a = fade + 0xc0 > 0xff ? 0xff : fade + 0xc0;
			int b = fade + 0x20 > 0xff ? 0xff : fade + 0x20;
			for (int k = 0; k < 2; k++) {
				const vec3_t *p = &pts[2 * k];
				CGameParticles_SetParticle(&self->glow, 4 * i + 2 * k, p->x, p->y, p->z, 44.0f,
					((uint32_t)a << 24) | 0x5c5cff, 2, 0.0f);
				CGameParticles_SetParticle(&self->glow, 4 * i + 2 * k + 1, p->x, p->y, p->z, 12.0f,
					((uint32_t)b << 24) | 0x8080ff, 2, 0.0f);
			}
		}
		if (self->life[i] <= 0.0f) {
			CGameTrail_Clear(trail);
			CGameParticles_KillRange(&self->glow, 4 * i, 4 * i + 4);
			*EFX_MessengerUsed(self, ray[0], vert[0]) = 0;
			*EFX_MessengerUsed(self, ray[1], vert[1]) = 0;
		}
	}
}

static cGVar GVar_FXMercenaryDrinkSmokeFile;
#define g_FXMercenaryDrinkSmokeFile (GVar_FXMercenaryDrinkSmokeFile.value.s)	/* "FXMercenaryDrinkSmokeFile" */
/* the color ramps of the bubbles and of the smoke, two of 9 colors each */

static inline byte EFX_MercSprite(void) {
	return (crt_rand() & 1) ? 0x09 : 0x48;
}

/* the acceleration of an emission: a random push against the velocity */
static void EFX_MercAcc(CGameParticles *p, double range, double add, double brake, double brakeAdd, vec3_t *buf) {
	float r = FX_Rand(range, add);
	const vec3_t *d = FX_RandomDir(buf);
	float ax = r * d->x;
	float ay = d->y * r;
	float az = d->z * r;
	float k = FX_Rand(brake, brakeAdd);
	p->initAccMin.x = ax + -p->initVelMin.x * k;
	p->initAccMin.y = ay + -p->initVelMin.y * k;
	p->initAccMin.z = az + -p->initVelMin.z * k;
	p->initAccMax = p->initAccMin;
}

/* the end of an emission: random rotation, the ramp of the two and one particle */
static void EFX_MercEmit(CGameParticles *p, uint32_t *ramps) {
	p->rotVar = FX_Rand(3.1415927410125732, -1.5707963705062866);
	CGameParticles_Apply(p);
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, ramps + (crt_rand() & 1) * 9);
	CGameParticles_EmitN(p, 1, 0);
}

static uint32_t g_RampMercenaryBubbles[18] = {
	0x30a0ff8c, 0x40a0ff8c, 0x4ba0ff8c, 0x5ca0ff8c, 0x80a0ff8c, 0xa4a0ff8c, 0xc4408037, 0xff000000, 0xff000000,
	0x10c4ff80, 0x20c4ff64, 0x30baff5a, 0x5caaff46, 0x80a0ff32, 0xa496ff26, 0xc4408012, 0xff000000, 0xff000000,
};

/* a bubble out of the tank (the pipe: higher up, smaller and slower) */
static void EFX_MercBubble(CGameParticles *p, byte pipe) {
	vec3_t buf;

	p->birthRate = 0;
	float r = pipe ? FX_Rand(0.5999999642372131, 0.30000001192092896) : FX_Rand(6.0, 0.5);
	const vec3_t *d = FX_RandomDir(&buf);
	if (pipe) {
		p->initPosMin.x = r * d->x + 0.0f;
		p->initPosMin.z = r * d->z + 0.0f;
		p->initPosMin.y = r * (d->y * 0.8f) + 1.0f;
	} else {
		p->initPosMin.x = d->x * r;
		p->initPosMin.y = d->y * 0.8f * r;
		p->initPosMin.z = d->z * r;
	}
	p->initPosMax = p->initPosMin;
	p->initVelMin.x = 0.0f;
	p->initVelMin.z = 0.0f;
	p->initVelMin.y = pipe ? FX_Rand(10.0, 12.0) : FX_Rand(25.0, 15.0);
	p->initVelMax = p->initVelMin;
	EFX_MercAcc(p, 1.100000023841858, 0.10000000149011612, 0.19999998807907104, 0.30000001192092896, &buf);
	p->sizeLife = 1.0f;
	float s = pipe ? FX_Rand(2.200000047683716, 0.800000011920929) : FX_Rand(1.5, 2.5);
	p->sizeIni = s;
	p->sizeEnd = s;
	p->lifeTime = pipe ? FX_Rand(0.3999999761581421, 0.550000011920929) : FX_Rand(0.6000000238418579,
		0.8500000238418579);
	byte sprite = EFX_MercSprite();
	p->spriteIni = sprite;
	p->spriteEnd = sprite;
	p->rotEnd = 0.0f;
	p->rotIni = FX_Rand(6.2831854820251465, 0.0);
	EFX_MercEmit(p, g_RampMercenaryBubbles);
}

/* the actions the mercenary does not smoke in */
static const char *const efx_mercQuiet[] = { "Drink", "Cheer", "Relax0", "Relax1", "TalkHappy" };

static uint32_t g_RampMercenarySmoke[18] = {
	0xc8a0ff8c, 0xd2a0ff8c, 0xdea0ff8c, 0xe6a0ff8c, 0xeea0ff8c, 0xf5a0ff8c, 0xfa408037, 0xff000000, 0xff000000,
	0xc8c4ff80, 0xd2c4ff64, 0xdebaff5a, 0xe6aaff46, 0xeea0ff32, 0xf596ff26, 0xfa408012, 0xff000000, 0xff000000,
};

/* a puff of the smoke of the cigar: a particle of a debris of the drink smoke system, at the mouth */
static void EFX_MercSmoke(cEmbeddedFXMercenary *self, CGameModel *model) {
	cEntity *e = self->base.ent;
	vec3_t pos, rot, buf, o;

	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXMercenaryDrinkSmokeFile);
	cDebris *d = cDebrisSys_GetIdle(sys);
	cWorld *w = g_pWorld;
	vec3_t cam = w->camPos[w->curCam];
	CGameModel_SetPos(Debris_Mod(d), &cam);
	d->life = 1.0f;
	if (!cDebrisSys_IsIdle(sys, d)) {
		cDebrisSys_Drop(sys, d);
	}
	cObjElementFXParticleSystem *el = (cObjElementFXParticleSystem *)d->fx;
	if (!el) {
		return;
	}
	CGameParticles *p = &el->particles;
	if (!el->on) {
		el->move = 0;
		el->on = 1;
		CGameModel_SetSortZ(Debris_Mod(d), -0.03f, 0.0f);
	}
	if (!e || !WithLife_GetActCtrl(e)) {
		return;
	}
	for (uint32_t i = 0; i < sizeof(efx_mercQuiet) / sizeof(efx_mercQuiet[0]); i++) {
		if (cActCtrl_IsActive(WithLife_GetActCtrl(e), efx_mercQuiet[i])) {
			return;
		}
	}
	if (!(crt_rand() & 1)) {
		return;
	}
	CGameModel_GetPos(model, &pos);
	CGameModel_GetRot(model, &rot);
	p->birthRate = 0;
	float r = FX_Rand(3.6000001430511475, 1.2999999523162842);
	const vec3_t *dir = FX_RandomDir(&buf);
	float dx = r * dir->x;
	float dy = dir->y * r;
	float dz = dir->z * r;
	const vec3_t mouth = { 15.0f, 25.0f, -55.0f };
	const vec3_t *m = Vec3_Rotate(&o, &mouth, &rot);
	p->initPosMin.x = pos.x + m->x + dx;
	p->initPosMin.y = m->y + pos.y + dy;
	p->initPosMin.z = m->z + pos.z + dz;
	p->initPosMax = p->initPosMin;
	float vz = FX_Rand(40.0, -20.0);
	float vy = FX_Rand(57.0, 38.0);
	p->initVelMin.x = FX_Rand(40.0, -20.0);
	p->initVelMin.y = vy;
	p->initVelMin.z = vz;
	p->initVelMax = p->initVelMin;
	EFX_MercAcc(p, 1.5999999046325684, 0.20000000298023224, 0.05000000447034836, 0.10000000149011612, &o);
	p->sizeIni = FX_Rand(9.5, 35.5);
	p->sizeLife = 1.0f;
	p->sizeEnd = FX_Rand(2.0, 4.0) * p->sizeIni;
	p->lifeTime = FX_Rand(1.4000000953674316, 2.5999999046325684);
	byte sprite = (crt_rand() & 1) + 0x11;
	p->spriteIni = sprite;
	p->spriteEnd = sprite;
	float a = FX_Rand(6.2831854820251465, 0.0);
	p->rotIni = a;
	p->rotEnd = a;
	EFX_MercEmit(p, g_RampMercenarySmoke);
}

/* every 0.05 s: the 7 glows of the tank flicker, a bubble rises (and one in four from the pipe of MercenaryL), and
 * the cigar smokes unless it drinks, cheers, relaxes or talks */
static void cEmbeddedFXMercenary_Update(cEmbeddedFX *elem, float dt) {
	cEmbeddedFXMercenary *self = (cEmbeddedFXMercenary *)elem;
	static const struct {
		float x, y, z;
		double range, add;
		uint32_t color;
		int sprite;
	} glows[7] = {
		{ -4.0f, 2.8f, -4.0f, 2.0, 14.0, 0x20deffc0, 1 }, { 4.0f, 2.8f, -4.0f, 2.0, 14.0, 0x20deffc0, 1 },
		{ -4.0f, 2.8f, 4.0f, 2.0, 14.0, 0x20deffc0, 1 }, { 4.0f, 2.8f, 4.0f, 2.0, 14.0, 0x20deffc0, 1 },
		{ 0.0f, 3.2f, 0.0f, 2.0, 20.0, 0x2aa4ff6e, 1 }, { 0.0f, 3.4f, 0.0f, 2.0, 22.0, 0x3090ff5c, 2 },
		{ 0.0f, 3.5f, 0.0f, 1.0, 25.0, 0x347aff58, 5 },
	};
	cEntity *e = self->base.ent;
	CGameModel *model = Obj_GetModel(e);

	if (!CGameModel_IsLoaded(model)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(model));
	byte vis = CGameModel_IsVisible(model);
	if (!vis || CGameModel_GetEffect(model, NULL)) {
		if (self->visible) {
			self->visible = 0;
		}
		return;
	}
	float t = self->t + dt;
	self->visible = vis;
	self->t = t;
	if (0.05f > t) {
		return;
	}
	self->t = 0.0f;
	for (int i = 0; i < 7; i++) {
		float size = FX_Rand(glows[i].range, glows[i].add);
		CGameParticles_SetParticle(&self->bubbles, i, glows[i].x, glows[i].y, glows[i].z, size, glows[i].color,
			glows[i].sprite, 0.0f);
	}
	EFX_MercBubble(&self->bubbles, 0);
	if (self->isL && self->dummyPipe != -1 && crt_rand() % 4 == 0) {
		EFX_MercBubble(&self->pipe, 1);
	}
	EFX_MercSmoke(self, model);
}

static cGVar GVar_FXRustySparksSound;
#define g_FXRustySparksSound (GVar_FXRustySparksSound.value.s)	/* "FXRustyChispasSound" */
static cGVar GVar_FXRustySparksSoundVol;
#define g_FXRustySparksSoundVol (GVar_FXRustySparksSoundVol.value.f)	/* "FXRustyChispasSoundVol" */
/* the color ramps of the sparks of the pigtail (9 colors each) */

static inline void EFX_RustyStill(CGameParticles *p) {
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

/* a random direction: (1, 1, 1) turned by three random angles (z, y, x picked in that order) */
static vec3_t EFX_RustyDir(void) {
	vec3_t ang, out;
	const vec3_t one = { 1.0f, 1.0f, 1.0f };

	float a1 = FX_Rand(6.2831854820251465, 0.0);
	float a2 = FX_Rand(6.2831854820251465, 0.0);
	ang.x = FX_Rand(6.2831854820251465, 0.0);
	ang.y = a2;
	ang.z = a1;
	return *Vec3_Rotate(&out, &one, &ang);
}

static uint32_t g_RampRustySparks[28] = {
	0x809696ff, 0x20d2d2ff, 0x406e6ee6, 0x803737be, 0x96191996, 0xa4030378, 0xc000006e, 0xff000032, 0xff000000,
	0x80d2d2ff, 0x209696ff, 0x406411e1, 0x803232b4, 0x961e1e55, 0xa40a0a2d, 0xc0050519, 0xff00000a, 0xff000000,
	0x80d2d2ff, 0x205a5aff, 0x401111e6, 0x800d0dab, 0x9608084c, 0xa4030338, 0xc000001e, 0xff00000a, 0xff000000,
};

/* the burst of sparks out of the pigtail: two flashes and 4..8 sparks, and their sound */
static void EFX_RustySparks(cEmbeddedFXRusty *self) {
	CGameParticles *p = &self->sparks;
	vec3_t buf;

	self->sparkT = FX_Rand(0.699999988079071, 0.10000000149011612);
	p->birthRate = 0;
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, g_RampRustySparks);
	EFX_RustyStill(p);
	p->sizeIni = FX_Rand(10.0, 10.0);
	p->sizeEnd = FX_Rand(10.0, 70.0);
	p->sizeLife = 0.15f;
	p->spriteIni = 2;
	p->spriteEnd = 2;
	p->lifeTime = FX_Rand(0.019999995827674866, 0.18000000715255737);
	CGameParticles_Apply(p);
	CGameParticles_EmitN(p, 1, 1);
	CGameParticles_SetColorRamp(p, g_RampRustySparks);
	EFX_RustyStill(p);
	p->sizeIni = FX_Rand(2.0, 4.0);
	p->sizeEnd = FX_Rand(5.0, 25.0);
	p->sizeLife = 0.15f;
	p->spriteIni = 1;
	p->spriteEnd = 1;
	p->lifeTime = FX_Rand(0.02000001072883606, 0.1599999964237213);
	CGameParticles_Apply(p);
	CGameParticles_EmitN(p, 1, 1);
	int n = (int)FX_Rand(4.0, 4.0);
	CGameParticles_SetSparksOn(p, 1);
	for (; n > 0; n--) {
		CGameParticles_SetColorRamp(p, g_RampRustySparks + (crt_rand() % 3) * 9);
		vec3_t d = EFX_RustyDir();
		float s = FX_Rand(3.0, 2.0);
		p->initPosMin.x = d.x * s;
		p->initPosMin.y = d.y * s;
		p->initPosMin.z = d.z * s;
		p->initPosMax = p->initPosMin;
		s = FX_Rand(430.0, 120.0);
		p->initVelMin.y = d.y * s;
		p->initVelMin.z = d.z * s;
		p->initVelMin.x = d.x * s;
		p->initVelMax = p->initVelMin;
		p->initAccMin.y = p->initVelMin.y * 0.2f - 950.0f;
		p->initAccMin.x = p->initVelMin.x * 0.2f + 0.0f;
		p->initAccMin.z = p->initVelMin.z * 0.2f + 0.0f;
		p->initAccMax = p->initAccMin;
		float dt = FX_Rand(0.029999999329447746, 0.0);
		float life = FX_Rand(0.25, 0.15000000596046448);
		CGameParticles_SetSparksOn(p, 1);
		p->lifeTime = life;
		p->sizeLife = 1.0f;
		p->rotIni = 0.0f;
		p->rotEnd = 0.0f;
		p->rotVar = 0.0f;
		float size = FX_Rand(20.0, 15.0);
		p->sizeIni = size;
		p->sizeEnd = size;
		p->spriteEnd = 0x44;
		p->spriteIni = (crt_rand() & 1) + 0x41;
		p->sparkVelScale = 1.0f;
		p->sparkMaxSize = FX_Rand(0.699999988079071, 0.30000001192092896);
		CGameParticles_Apply(p);
		CGameParticles_EmitDt(p, 1, 1, dt + 0.03f);
	}
	CGameParticles_SetSparksOn(p, 0);
	cEntity *e = self->base.ent;
	const char *name = g_FXRustySparksSound;
	int volume = Snd_Volume(g_FXRustySparksSoundVol);
	const vec3_t *pos = Obj_GetViewPos(e, &buf);
	SoundSys_PlayAt(g_pSound, name, pos, volume);
}

/* a control point of a bolt moved out from the center by m along its way and by a random offset of k */
static void EFX_RustyBend(vec3_t *out, const vec3_t *v, const vec3_t *pos, double kr, double ka, double mr, double ma) {
	vec3_t o = EFX_RustyDir();
	float k = FX_Rand(kr, ka);
	o.x = o.x * k;
	o.y = o.y * k;
	o.z = o.z * k;
	float m = FX_Rand(mr, ma);
	vec3_t d = { v->x - pos->x, v->y - pos->y, v->z - pos->z };
	vec3_t n = FX_Unit(d);
	out->x = v->x + n.x * m + o.x;
	out->y = v->y + n.y * m + o.y;
	out->z = v->z + n.z * m + o.z;
}

/*
 * Rusty the mechanic while visible: the sparks of the pigtail every 0.1..0.8 s, and 4 lightning bolts between the
 * first vertices of DF_LightingA, B and C that live 0.17..0.34 s with a pause of 0.1..0.65 s.
 */
static void cEmbeddedFXRusty_Update(cEmbeddedFX *elem, float dt) {
	cEmbeddedFXRusty *self = (cEmbeddedFXRusty *)elem;
	cEntity *e = self->base.ent;
	CGameModel *model = Obj_GetModel(e);
	vec3_t pos, pts[5], coefs[GAMETRAIL_MAX_BEZIER];

	if (!CGameModel_IsLoaded(model)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(model));
	byte vis = CGameModel_IsVisible(model);
	if (!vis || CGameModel_GetEffect(model, NULL)) {
		if (self->visible) {
			cEmbeddedFXRusty_Clear(self);
			self->visible = 0;
		}
		return;
	}
	float t = self->sparkT - dt;
	self->visible = vis;
	self->sparkT = t;
	if (t <= 0.0f) {
		EFX_RustySparks(self);
	}
	CGameModel_GetPos(model, &pos);
	for (int i = 0; i < FXRUSTY_TRAILS; i++) {
		CGameTrail *trail = &self->trails[i];
		if (!(self->life[i] > 0.0f)) {
			if (self->pause[i] > 0.0f) {
				self->pause[i] = self->pause[i] - dt;
			} else {
				self->life[i] = FX_Rand(0.16999998688697815, 0.18000000715255737);
				self->pause[i] = FX_Rand(0.5499999523162842, 0.10000000149011612);
			}
			continue;
		}
		float life = self->life[i] - dt;
		self->life[i] = life;
		if (life > 0.0f) {
			int fade = 0xff - (int)(Float_MinSS(1.0f, life / 0.2f) * 255.0f);
			CGameTrail_Reset(trail);
			CGameTrail_SetTransp(trail, (byte)fade);
			CGameModel_Vertices(model, &pts[0], NULL, NULL, 1, 0, NULL, self->dummyLighting[0], NULL);
			CGameModel_Vertices(model, &pts[2], NULL, NULL, 1, 0, NULL, self->dummyLighting[1], NULL);
			CGameModel_Vertices(model, &pts[4], NULL, NULL, 1, 0, NULL, self->dummyLighting[2], NULL);
			EFX_RustyBend(&pts[1], &pts[0], &pos, 2.0, 2.0, 2.0, 4.0);
			EFX_RustyBend(&pts[2], &pts[2], &pos, 4.0, 2.0, 2.0, 2.0);
			EFX_RustyBend(&pts[3], &pts[4], &pos, 2.0, 2.0, 2.0, 4.0);
			CGameTrail_BezierCoefs(trail, 5, pts, coefs);
			int m = CGameTrail_GetMaxTrails(trail);
			if (m - 1 > 0) {
				float span = (float)(m - 2);
				for (int j = 0; j < m - 1; j++) {
					vec3_t p, buf;
					CGameTrail_BezierPoint(trail, 5, coefs, &p, (float)j / span);
					float w = FX_Rand(1.0, 2.0);
					if (j > 0 && j < m - 2) {
						float r = FX_Rand(2.0, 0.5);
						const vec3_t *d = FX_RandomDir(&buf);
						p.x = p.x + r * d->x;
						p.y = p.y + d->y * r;
						p.z = p.z + d->z * r;
					} else {
						w = w + w;
					}
					CGameTrail_Add(trail, p, w, 0x108080ff);
				}
			}
			int a = fade + 0x60 > 0xff ? 0xff : fade + 0x60;
			int b = fade + 6 > 0xff ? 0xff : fade + 6;
			for (int k = 0; k < 2; k++) {
				const vec3_t *q = &pts[4 * k];
				CGameParticles_SetParticle(&self->glowLighting, 4 * i + 2 * k, q->x, q->y, q->z, 50.0f,
					((uint32_t)a << 24) | 0x6060ff, 5, 0.0f);
				CGameParticles_SetParticle(&self->glowLighting, 4 * i + 2 * k + 1, q->x, q->y, q->z, 10.0f,
					((uint32_t)b << 24) | 0xa4a4ff, 1, 0.0f);
			}
		}
		if (self->life[i] <= 0.0f) {
			CGameTrail_Clear(trail);
			CGameParticles_KillRange(&self->glowLighting, 4 * i, 4 * i + 4);
		}
	}
}

/* the slot Reset of a class that does only return 1 */
byte cEmbeddedFX_ResetTrue(cEmbeddedFX *self) {
	(void)self;
	return 1;
}

/* the slot Stop of a class that does nothing there */
void cEmbeddedFX_StopNop(cEmbeddedFX *self, byte keep) {
	(void)self;
	(void)keep;
}

/* the slot Tick of a class that does nothing there */
void cEmbeddedFX_TickNop(cEmbeddedFX *self) {
	(void)self;
}

/* the slot Update of a class that does nothing there */
void cEmbeddedFX_UpdateNop(cEmbeddedFX *self, float dt) {
	(void)self;
	(void)dt;
}

/* the vtable of cEmbeddedFX */
static const cEmbeddedFXVtbl cEmbeddedFX_vftable = {
	.Delete = cEmbeddedFX_Delete,
	.Reset = (void *)crt_purecall,
	.Release = (void *)crt_purecall,
	.Update = (void *)crt_purecall,
	.Tick = (void *)crt_purecall,
	.Stop = (void *)crt_purecall,
};

/* the vtable of cEmbeddedFXSentinel */
static const cEmbeddedFXVtbl cEmbeddedFXSentinel_vftable = {
	.Delete = cEmbeddedFXSentinel_Delete,
	.Reset = cEmbeddedFXSentinel_Reset,
	.Release = cEmbeddedFXSentinel_Release,
	.Update = cEmbeddedFXSentinel_Update,
	.Tick = cEmbeddedFX_TickNop,
	.Stop = cEmbeddedFXSentinel_Stop,
};

/* the vtable of cEmbeddedFXGear */
static const cEmbeddedFXVtbl cEmbeddedFXGear_vftable = {
	.Delete = cEmbeddedFXGear_Delete,
	.Reset = cEmbeddedFXPolice_Reset,
	.Release = cEmbeddedFXGear_Release,
	.Update = cEmbeddedFXPolice_Update,
	.Tick = cEmbeddedFX_TickNop,
	.Stop = cEmbeddedFX_StopNop,
};

/* the vtable of cEmbeddedFXPolice */
static const cEmbeddedFXVtbl cEmbeddedFXPolice_vftable = {
	.Delete = cEmbeddedFXPolice_Delete,
	.Reset = cEmbeddedFXPolice_Reset,
	.Release = cEmbeddedFXPolice_Release,
	.Update = cEmbeddedFXPolice_Update,
	.Tick = cEmbeddedFX_TickNop,
	.Stop = cEmbeddedFX_StopNop,
};

/* the vtable of cEmbeddedFXBankDirector */
static const cEmbeddedFXVtbl cEmbeddedFXBankDirector_vftable = {
	.Delete = cEmbeddedFXBankDirector_Delete,
	.Reset = cEmbeddedFX_ResetTrue,
	.Release = cEmbeddedFXBankDirector_Release,
	.Update = cEmbeddedFXBankDirector_Update,
	.Tick = cEmbeddedFX_TickNop,
	.Stop = cEmbeddedFX_StopNop,
};

/* the vtable of cEmbeddedFXBankMaster */
static const cEmbeddedFXVtbl cEmbeddedFXBankMaster_vftable = {
	.Delete = cEmbeddedFXBankMaster_Delete,
	.Reset = cEmbeddedFX_ResetTrue,
	.Release = cEmbeddedFXBankMaster_Release,
	.Update = cEmbeddedFXBankMaster_Update,
	.Tick = cEmbeddedFX_TickNop,
	.Stop = cEmbeddedFX_StopNop,
};

/* the vtable of cEmbeddedFXCrazyGambler */
static const cEmbeddedFXVtbl cEmbeddedFXCrazyGambler_vftable = {
	.Delete = cEmbeddedFXCrazyGambler_Delete,
	.Reset = cEmbeddedFXCrazyGambler_Reset,
	.Release = cEmbeddedFXCrazyGambler_Release,
	.Update = cEmbeddedFXCrazyGambler_Update,
	.Tick = cEmbeddedFX_TickNop,
	.Stop = cEmbeddedFXCrazyGambler_Stop,
};

/* the vtable of cEmbeddedFXRusty */
static const cEmbeddedFXVtbl cEmbeddedFXRusty_vftable = {
	.Delete = cEmbeddedFXRusty_Delete,
	.Reset = cEmbeddedFXRusty_Reset,
	.Release = cEmbeddedFXRusty_Release,
	.Update = cEmbeddedFXRusty_Update,
	.Tick = cEmbeddedFX_TickNop,
	.Stop = cEmbeddedFXRusty_Stop,
};

/* the vtable of cEmbeddedFXMessenger */
static const cEmbeddedFXVtbl cEmbeddedFXMessenger_vftable = {
	.Delete = cEmbeddedFXMessenger_Delete,
	.Reset = cEmbeddedFXMessenger_Reset,
	.Release = cEmbeddedFXMessenger_Release,
	.Update = cEmbeddedFXMessenger_Update,
	.Tick = cEmbeddedFX_TickNop,
	.Stop = cEmbeddedFXMessenger_Stop,
};

/* the vtable of cEmbeddedFXMaintenance */
static const cEmbeddedFXVtbl cEmbeddedFXMaintenance_vftable = {
	.Delete = cEmbeddedFXMaintenance_Delete,
	.Reset = cEmbeddedFX_ResetTrue,
	.Release = cEmbeddedFXMaintenance_Release,
	.Update = cEmbeddedFXMaintenance_Update,
	.Tick = cEmbeddedFX_TickNop,
	.Stop = cEmbeddedFX_StopNop,
};

/* the vtable of cEmbeddedFXMercenary */
static const cEmbeddedFXVtbl cEmbeddedFXMercenary_vftable = {
	.Delete = cEmbeddedFXMercenary_Delete,
	.Reset = cEmbeddedFXMercenary_Reset,
	.Release = cEmbeddedFXMercenary_Release,
	.Update = cEmbeddedFXMercenary_Update,
	.Tick = cEmbeddedFX_TickNop,
	.Stop = cEmbeddedFX_StopNop,
};

static cPy_NativeFunc NativeFunc_FXPoliceAlarmInit;
static cPy_NativeFunc NativeFunc_FXPoliceAlarmEnd;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_GVAR_STRING(Fini_GVar_FXMercenaryDrinkSmokeFile, &GVar_FXMercenaryDrinkSmokeFile)
INIT_GVAR_STRING(Init_GVar_FXMercenaryDrinkSmokeFile, Fini_GVar_FXMercenaryDrinkSmokeFile, &GVar_FXMercenaryDrinkSmokeFile, "FXMercenaryDrinkSmokeFile", 0, "Default Model for Mercenary Drink Smoke Effects", "Models/GFX/Mercenary/DrinkSmoke.M3D")
INIT_GVAR_FLOAT(Init_GVar_SentinelLaserScanerDist, &GVar_SentinelLaserScanerDist, "SentinelLaserScanerDist", 0, "Max Distance to show Sentinel Laser Scaner", 4500.0f)
FINI_GVAR_STRING(Fini_GVar_FXRustySparksSound, &GVar_FXRustySparksSound)
INIT_GVAR_STRING(Init_GVar_FXRustySparksSound, Fini_GVar_FXRustySparksSound, &GVar_FXRustySparksSound, "FXRustyChispasSound", 0, "Rusty efecto de chispas sound name", "FXRustyChispasSound")
INIT_GVAR_FLOAT(Init_GVar_FXRustySparksSoundVol, &GVar_FXRustySparksSoundVol, "FXRustyChispasSoundVol", 0, "Rusty efecto de chispas sound volume", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_MessengerLightingDist, &GVar_MessengerLightingDist, "MessengerLightingDist", 0, "Max Distance to show Messenger Lighting Effects", 2000.0f)
INIT_GVAR_FLOAT(Init_GVar_SentinelStdFovX, &GVar_SentinelStdFovX, "SentinelStdFovX", 0, "Standard Sentinel FieldOfView", 2.3561945f)
INIT_GVAR_FLOAT(Init_GVar_SentinelStdFovY, &GVar_SentinelStdFovY, "SentinelStdFovY", 0, "Standard Sentinel FieldOfView", 1.5707964f)
INIT_GVAR_FLOAT(Init_GVar_SentinelStdFar, &GVar_SentinelStdFar, "SentinelStdFar", 0, "Standard Sentinel Distance of View", 1000.0f)
INIT_NATIVE_FUNC(Init_NativeFunc_FXPoliceAlarmInit, &NativeFunc_FXPoliceAlarmInit, "FXPoliceAlarmInit", EmbeddedFXChar_AlarmOn)
INIT_NATIVE_FUNC(Init_NativeFunc_FXPoliceAlarmEnd, &NativeFunc_FXPoliceAlarmEnd, "FXPoliceAlarmEnd", EmbeddedFXChar_AlarmOff)
