/*
 * World/FX/FXScene.cpp of the game: the effects of the cutscene models, see include/scrap/fxscene.h.
 */
#include <string.h>

#include "scrap/doors.h"
#include "scrap/embeddedfxmisc.h"
#include "scrap/fxelements.h"
#include "scrap/fxscene.h"
#include "scrap/fxweapons.h"
#include "scrap/mathlib.h"
#include "scrap/scrapcollision.h"
#include "scrap/timer.h"
#include "scrap/weapons.h"
#include "scrap/world.h"

/* the color ramps (9 colors) of the particles */
#define FXS_RAMP(ramps, i) ((ramps) + (i) * 9)
#define FXS_RAMP_HUMANCD 0x430c48	/* [2] */
#define FXS_RAMP_YAKUZZI 0x430dd8	/* [2] */

typedef void(*FXSUpdate_t)(void *self, cDebris *d, float dt);

static inline float FXS_Delta(void) {
	return cTimer_GetDelta(&g_pWorld->timer);
}

/* the entity let go once it is deleted or off; whether there is one that is on */
static int FXS_Alive(cEntity **ent) {
	cEntity *e = *ent;

	if (e && (Entity_Removed(e) || !e->active)) {
		*ent = NULL;
	}
	return *ent && (*ent)->active;
}

/* the model of the debris to the position and angles of the model of e (into pos and rot) */
static void FXS_Follow(cEntity *e, cDebris *d, float sortZ, vec3_t *pos, vec3_t *rot) {
	CGameModel *m = Obj_Vtbl(e)->GetModel(e);

	CHECK(CGameModel_IsLoaded(m));
	CGameModel_GetPos(m, pos);
	CGameModel_GetRot(m, rot);
	CGameModel_SetPos(Debris_Model(d), pos);
	CGameModel_SetRot(Debris_Model(d), rot);
	CGameModel_SetSortZ(Debris_Model(d), sortZ, 0.0f);
}

/* rand() of the exe in [0, 1] (double) */
static inline double FXS_Rnd(void) {
	return (double)(crt_rand() & 0x7fff) * g_FxRandScale;
}

/* the particles to the node of the debris model */
static void FXS_AttachNode(CGameParticles *sp, cDebris *d, const char *node) {
	CGameModel *m = Debris_Model(d);
	int i = CGameModel_FindRenderNode(m, node);

	CHECK(i > -1);
	CGameParticles_Attach(sp, m, (uint32_t)i);
}

/* the position of the first scene dummy of that name into out, out unchanged without one */
static void FXS_DummyPos(const char *name, vec3_t *out) {
	char buf[0x80];
	float ang[2];
	vec3_t pos;
	byte flag;

	DummySearch_Start(name, 0);
	if (DummySearch_Next(buf, sizeof(buf), &pos, ang, &flag)) {
		*out = pos;
	}
}

/* the shared part of SetEntity: an entity with life with a model */
static void FXS_SetEntity(cEntity **slot, cEntity *e) {
	CHECK(e);
	CHECK(Entity_IsType(e, TYEN_WITH_LIFE));
	*slot = e;
	CHECK(CGameModel_IsLoaded(Obj_Vtbl(e)->GetModel(e)));
}

/* a new debris of the system of file with that life, dropped */
static cDebris *FXS_Drop(const char *file, float life) {
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, file);
	cDebris *d = cDebrisSys_NewElement(sys);

	d->life = life;
	cDebrisSys_Drop(sys, d);
	return d;
}

void FXScene_Create(cEntity *e, const char *model) {
	cDebris *d;

	if (!strcmp(model, "Decontamination.m3d")) {
		d = FXS_Drop(g_FXSceneDecontaminationFile, 20.0f);
		cObjElementFXSceneDecontamination *el = Debris_Element(d);
		if (!el) {
			return;
		}
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXSceneDecontamination_SetEntity(el, e);
		cObjElementFXSceneDecontamination_SetNames(el, "DecontaminationDtritus", "Decon");
		el->height = 145.0f;
		return;
	}
	if (!strcmp(model, "Bishops.m3d")) {
		d = FXS_Drop(g_FXSceneBishopsFile, 30.0f);
		cObjElementFXSceneBishops *el = Debris_Element(d);
		if (!el) {
			return;
		}
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXSceneBishops_SetEntity(el, e);
		return;
	}
	if (!strcmp(model, "BillArrives_A.m3d")) {
		d = FXS_Drop(g_FXSceneBillArrivesAFile, 19.0f);
		cObjElementFXSceneBillArrives_A *el = Debris_Element(d);
		if (!el) {
			return;
		}
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXSceneBillArrives_A_SetEntity(el, e);
		return;
	}
	if (!strcmp(model, "BillArrives_C.m3d")) {
		d = FXS_Drop(g_FXSceneDecontaminationFile, 20.0f);
		cObjElementFXSceneDecontamination *el = Debris_Element(d);
		if (!el) {
			return;
		}
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXSceneDecontamination_SetEntity(el, e);
		cObjElementFXSceneDecontamination_SetNames(el, "BillArrives_CHuman", "Descontamination");
		el->height = 80.0f;
		return;
	}
	if (!strcmp(model, "Yakuzzi.m3d")) {
		d = FXS_Drop(g_FXSceneYakuzziFile, 42.0f);
		cObjElementFXSceneYakuzzi *el = Debris_Element(d);
		if (!el) {
			return;
		}
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXSceneYakuzzi_SetEntity(el, e);
		/* the first frame at once (through the vtable, as the original) */
		(*(FXSUpdate_t *)((byte *)el->vftable + 4))(el, d, 0.0f);
		return;
	}
	if (!strcmp(model, "Beating.m3d")) {
		d = FXS_Drop(g_FXSceneBeatingFile, 50.0f);
		cObjElementFXSceneBeatPickus *el = Debris_Element(d);
		if (!el) {
			return;
		}
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXSceneBeatPickus_SetEntity(el, e);
		return;
	}
	if (!strcmp(model, "HumanCD.m3d")) {
		d = FXS_Drop(g_FXSceneHumanCDFile, 46.0f);
		cObjElementFXSceneHumanCD *el = Debris_Element(d);
		if (!el) {
			return;
		}
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXSceneHumanCD_SetEntity(el, e);
		return;
	}
	if (!strcmp(model, "EnterMatrix.m3d")) {
		d = FXS_Drop(g_FXSceneEnterMatrixFile, 50.0f);
		cObjElementFXSceneEnterMatrix *el = Debris_Element(d);
		if (!el) {
			return;
		}
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXSceneEnterMatrix_SetEntity(el, e);
		return;
	}
	if (!strcmp(model, "TheEndB.m3d")) {
		d = FXS_Drop(g_FXSceneTheEndBFile, 40.0f);
		cObjElementFXSceneTheEndB *el = Debris_Element(d);
		if (!el) {
			return;
		}
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXSceneTheEndB_SetEntity(el, e);
	}
}

/* SetEntity */

void cObjElementFXSceneBeatPickus_SetEntity(cObjElementFXSceneBeatPickus *self, cEntity *e) {
	FXS_SetEntity(&self->ent, e);
}

void cObjElementFXSceneBillArrives_A_SetEntity(cObjElementFXSceneBillArrives_A *self, cEntity *e) {
	FXS_SetEntity(&self->ent, e);
}

void cObjElementFXSceneBishops_SetEntity(cObjElementFXSceneBishops *self, cEntity *e) {
	FXS_SetEntity(&self->ent, e);
}

void cObjElementFXSceneDecontamination_SetEntity(cObjElementFXSceneDecontamination *self, cEntity *e) {
	FXS_SetEntity(&self->ent, e);
}

void cObjElementFXSceneEnterMatrix_SetEntity(cObjElementFXSceneEnterMatrix *self, cEntity *e) {
	FXS_SetEntity(&self->ent, e);
}

void cObjElementFXSceneHumanCD_SetEntity(cObjElementFXSceneHumanCD *self, cEntity *e) {
	FXS_SetEntity(&self->ent, e);
	CHECK(g_MentalMainFX);
	g_MentalMainFX->last = -2;
	g_MentalMainFX->scale = 1.0f;
}

void cObjElementFXSceneTheEndB_SetEntity(cObjElementFXSceneTheEndB *self, cEntity *e) {
	FXS_SetEntity(&self->ent, e);
}

void cObjElementFXSceneYakuzzi_SetEntity(cObjElementFXSceneYakuzzi *self, cEntity *e) {
	FXS_SetEntity(&self->ent, e);
}

/* Attach */

void cObjElementFXSceneBeatPickus_Attach(cObjElementFXSceneBeatPickus *self, cDebris *d) {
	CHECK(CGameModel_IsLoaded(Debris_Model(d)));
	FXS_AttachNode(&self->sparks, d, "Dummy01");
	FXS_AttachNode(&self->debris, d, "Dummy02");
	FXS_AttachNode(&self->dust, d, "Dummy03");
	FXS_DummyPos("DM_Scene_Beating", &self->dummy);
}

void cObjElementFXSceneBillArrives_A_Attach(cObjElementFXSceneBillArrives_A *self, cDebris *d) {
	CHECK(CGameModel_IsLoaded(Debris_Model(d)));
	FXS_AttachNode(&self->particles, d, "Dummy01");
	FXS_DummyPos("DM_Effects_BillArrives_A_01", &self->dummy1);
	FXS_DummyPos("DM_Effects_BillArrives_A_02", &self->dummy2);
}

void cObjElementFXSceneBishops_Attach(cObjElementFXSceneBishops *self, cDebris *d) {
	CHECK(CGameModel_IsLoaded(Debris_Model(d)));
	FXS_AttachNode(&self->dataCodec, d, "Dummy01");
	CHECK(self->dataCodec.ParticleSystem);
	CHECK(CGameModel_IsLoaded(Debris_Model(d)));
	FXS_AttachNode(&self->glow, d, "Dummy02");
	CHECK(self->glow.ParticleSystem);
	for (int i = 0; i < FXBISHOPS_TRAILS; i++) {
		CGameTrail_SetMaxTrails(&self->trails[i], 0x1f);
		CGameTrail_SetTexture(&self->trails[i], 0xb);
		CGameTrail_SetType(&self->trails[i], 3);
	}
	self->glowT = 0.0f;
	self->master.x = 0.0f;
	self->master.y = 0.0f;
	self->master.z = 0.0f;
}

void cObjElementFXSceneDecontamination_Attach(cObjElementFXSceneDecontamination *self, cDebris *d) {
	CHECK(CGameModel_IsLoaded(Debris_Model(d)));
	FXS_AttachNode(&self->particles, d, "Dummy01");
	for (int i = 0; i < FXDECON_RAYS; i++) {
		CGameTrail *t = &self->trails[i];
		self->life[i] = 0.0f;
		self->wait[i] = FxElem_Rand(0.12f, 0.0);
		CGameTrail_SetMaxTrails(t, 0x14);
		CGameTrail_SetSmooth(t, 1);
		CGameTrail_SetColor(t, 0xc0c0ff);
		CGameTrail_SetTexture(t, 3);
		CGameTrail_SetScale(t, 1.0f, 1.0f);
		CGameTrail_SetFade(t, 0);
		CGameTrail_SetPremul(t, 1);
	}
}

void cObjElementFXSceneEnterMatrix_Attach(cObjElementFXSceneEnterMatrix *self, cDebris *d) {
	CHECK(CGameModel_IsLoaded(Debris_Model(d)));
	FXS_AttachNode(&self->particles, d, "Dummy01");
	for (int i = 0; i < FXMATRIX_TRAILS; i++) {
		CGameTrail *t = &self->trails[i];
		CGameTrail_SetMaxTrails(t, 0x10);
		CGameTrail_SetSmooth(t, 1);
		CGameTrail_SetTexture(t, (~crt_rand() & 1) | 8);
		CGameTrail_SetPremul(t, 1);
		self->life[i] = -1.0f;
		self->wait[i] = FxElem_Rand(0.45000002f, 0.15f);
	}
	self->lastKind = -1;
}

void cObjElementFXSceneHumanCD_Attach(cObjElementFXSceneHumanCD *self, cDebris *d) {
	CHECK(CGameModel_IsLoaded(Debris_Model(d)));
	FXS_AttachNode(&self->particles, d, "Dummy01");
	CHECK(CGameModel_IsLoaded(Debris_Model(d)));
	FXS_AttachNode(&self->sparks, d, "Dummy03");
	FXS_DummyPos("DM_Element_MentalControllerMainFX", &self->dummy);
	self->sparkT = 0.0f;
}

void cObjElementFXSceneTheEndB_Attach(cObjElementFXSceneTheEndB *self, cDebris *d) {
	CHECK(CGameModel_IsLoaded(Debris_Model(d)));
	FXS_AttachNode(&self->particles, d, "Dummy01");
}

void cObjElementFXSceneYakuzzi_Attach(cObjElementFXSceneYakuzzi *self, cDebris *d) {
	CHECK(CGameModel_IsLoaded(Debris_Model(d)));
	FXS_AttachNode(&self->particles, d, "Dummy01");
	FXS_DummyPos("DM_Element_TempleYakuzzi_00", &self->dummy);
}

/* Reset */

void cObjElementFXSceneBeatPickus_Reset(cObjElementFXSceneBeatPickus *self, int arg) {
	(void)arg;
	self->ent = NULL;
	self->t = 0.0f;
	self->tPrev = 0.0f;
	self->dustT = 0.0f;
	self->debrisT = 0.0f;
	self->sparkT = 0.0f;
	self->hitT = 0.0f;
}

void cObjElementFXSceneBillArrives_A_Reset(cObjElementFXSceneBillArrives_A *self, int arg) {
	(void)arg;
	self->ent = NULL;
	self->capsule = NULL;
	self->t = 0.0f;
	self->tPrev = 0.0f;
	self->jetT = 0.0f;
	self->smokeT = 0.0f;
}

void cObjElementFXSceneBishops_Reset(cObjElementFXSceneBishops *self, int arg) {
	(void)arg;
	self->t = 0.0f;
	self->ent = NULL;
	self->detritus = NULL;
	for (int i = 0; i < FXBISHOPS_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
	}
	CGameParticles_KillAll(&self->dataCodec);
	CGameParticles_KillAll(&self->glow);
	self->glowT = 0.0f;
	self->master.x = 0.0f;
	self->master.y = 0.0f;
	self->master.z = 0.0f;
}

void cObjElementFXSceneDecontamination_Reset(cObjElementFXSceneDecontamination *self, int arg) {
	(void)arg;
	self->ent = NULL;
	self->target = NULL;
	self->t2 = 0.0f;
	self->t = 0.0f;
	for (int i = 0; i < FXDECON_RAYS; i++) {
		self->life[i] = 0.0f;
		self->wait[i] = FxElem_Rand(0.12f, 0.0);
		CGameTrail_Clear(&self->trails[i]);
	}
	CGameParticles_KillAll(&self->particles);
}

void cObjElementFXSceneEnterMatrix_Reset(cObjElementFXSceneEnterMatrix *self, int arg) {
	(void)arg;
	self->ent = NULL;
	self->t = 0.0f;
	self->tPrev = 0.0f;
	for (int i = 0; i < FXMATRIX_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
		self->life[i] = -1.0f;
		self->wait[i] = FxElem_Rand(0.45000002f, 0.15f);
	}
}

void cObjElementFXSceneHumanCD_Reset(cObjElementFXSceneHumanCD *self, int arg) {
	(void)arg;
	self->ent = NULL;
	self->t = 0.0f;
	self->tPrev = 0.0f;
	self->sparkT = 0.0f;
	CGameParticles_KillAll(&self->particles);
}

void cObjElementFXSceneTheEndB_Reset(cObjElementFXSceneTheEndB *self, int arg) {
	(void)arg;
	self->ent = NULL;
	self->bike = NULL;
	self->t = 0.0f;
	self->tPrev = 0.0f;
}

void cObjElementFXSceneYakuzzi_Reset(cObjElementFXSceneYakuzzi *self, int arg) {
	(void)arg;
	self->ent = NULL;
	self->t = 0.0f;
	self->tPrev = 0.0f;
	self->emitT = 0.0f;
}

/* Update */

/* one burst of sparks of HumanCD from the dummy, up by dy times yScale: sizes by scale, the rest of the ranges */
static void FXS_HumanCDSparks(cObjElementFXSceneHumanCD *self, double scaleRange, double scaleAdd, float yScale,
	int scaleY, double lifeRange, double lifeAdd, uint16_t sprites) {
	CGameParticles *sp = &self->sparks;
	vec3_t dir;

	sp->birthRate = 0;
	float scale = FxElem_Rand(scaleRange, scaleAdd);
	FX_RandomDir(&dir);
	float len = FxElem_Rand(90.0f, 35.0f);
	float dx = len * dir.x;
	float dy = len * dir.y;
	float dz = len * dir.z;
	float up = FxElem_Rand(4800.0f, 200.0f);
	if (scaleY) {
		up = up * yScale;
	}
	sp->initPosMin.x = (self->dummy.x + 0.0f) + dx;
	sp->initPosMin.y = (self->dummy.y + up) + dy;
	sp->initPosMin.z = (self->dummy.z + 0.0f) + dz;
	sp->initPosMax = sp->initPosMin;
	sp->initVelMin.x = 0.0f;
	sp->initVelMin.y = 0.0f;
	sp->initVelMin.z = 0.0f;
	sp->initVelMax = sp->initVelMin;
	sp->initAccMin.x = 0.0f;
	sp->initAccMin.y = 0.0f;
	sp->initAccMin.z = 0.0f;
	sp->initAccMax = sp->initAccMin;
	sp->sizeIni = FxElem_Rand(50.0f, 50.0f) * scale;
	sp->sizeEnd = FxElem_Rand(145.0f, 805.0f) * scale;
	sp->lifeTime = FxElem_Rand(lifeRange, lifeAdd);
	float rot = FxElem_Rand(6.2831855f, 0.0);
	sp->rotIni = rot;
	sp->rotEnd = rot;
	sp->rotVar = FxElem_Rand(3.1415927f, -1.5707964f);
	if (sprites) {
		sp->sizeLife = FxElem_Rand(0.07f, 0.1f);
		memcpy(&sp->spriteIni, &sprites, 2);
	} else {
		/* the sprites by a rand() after the one of sizeLife */
		sp->sizeLife = FxElem_Rand(0.07f, 0.1f);
		sprites = (crt_rand() & 1) ? 0x6151 : 0x7161;
		memcpy(&sp->spriteIni, &sprites, 2);
	}
	CGameParticles_Apply(sp);
	CGameParticles_SetColorRamp(sp, FXS_RAMP(GAME_ARRAY(uint32_t, FXS_HumanCDSparks_430c48), crt_rand() & 1));
	CGameParticles_EmitDt(sp, 1, 1, FxElem_Rand(0.1f, 0.0));
}

void cObjElementFXSceneHumanCD_Update(cObjElementFXSceneHumanCD *self, cDebris *d, float dt) {
	vec3_t pos, rot;
	float f;

	(void)dt;
	if (!FXS_Alive(&self->ent)) {
		d->life = 0.0f;
		return;
	}
	CHECK(g_MentalMainFX);
	float delta = FXS_Delta();
	self->t = self->t + delta;
	FXS_Follow(self->ent, d, -102.0f, &pos, &rot);
	if (FX_InRange(self->t, 17.0f, 30.0f, &f)) {
		float v = f * f * (2.0f - f);
		g_MentalMainFX->scale = (v + v) + 1.0f;
		g_MentalMainFX->last = -2;
		if (FX_InRange(self->t, 17.333334f, 21.333334f, &f)) {
			g_MentalMainFX->last = 8 - (int)(f * 3.9f);
		}
		if (FX_InRange(self->t, 21.333334f, 25.666666f, &f)) {
			g_MentalMainFX->last = 5 - (int)(f * 2.9f);
		}
		if (FX_InRange(self->t, 25.666666f, 30.333334f, &f)) {
			g_MentalMainFX->last = (int)(f * 2.9f);
		}
	} else if (FX_InRange(self->t, 30.0f, 37.0f, &f)) {
		g_MentalMainFX->scale = f * f * (2.0f - f) * 3.0f + 3.0f;
	}
	float st = self->sparkT + delta;
	self->sparkT = st;
	if (st > 0.03f && FX_InRange(self->t, 36.0f, 37.0f, &f)) {
		float yScale = f * 0.5f + 0.5f;
		self->sparkT = 0.0f;
		FXS_HumanCDSparks(self, 0.6f, 1.1f, yScale, 0, 0.060000002f, 0.76f, 0);
		FXS_HumanCDSparks(self, 0.5999999f, 1.2f, yScale, 1, 0.029999971f, 0.93f, 0x2101);
	}
	self->tPrev = self->t;
}

void cObjElementFXSceneTheEndB_Update(cObjElementFXSceneTheEndB *self, cDebris *d, float dt) {
	vec3_t pos, rot;
	float f;

	(void)dt;
	if (!FXS_Alive(&self->ent)) {
		d->life = 0.0f;
		return;
	}
	if (!self->bike) {
		cEntity *m = World_FindEntity("TheEndBMoto");
		self->bike = m;
		if (m) {
			CHECK(Entity_IsType(m, TYEN_WITH_LIFE));
		}
	}
	self->t = self->t + FXS_Delta();
	FXS_Follow(self->ent, d, -107.0f, &pos, &rot);
	if (!self->bike) {
		self->tPrev = self->t;
		return;
	}
	cEmbeddedFXBike *efx = (cEmbeddedFXBike *)Obj(self->bike)->fx;
	CHECK(efx);
	/* the engine of the bike by the time of the scene */
	if (FX_InRange(self->t, 0.0f, 8.0f, &f)) {
		cEmbeddedFXBike_SetThrottle(efx, f * 0.5f, 0.0f);
	} else if (FX_InRange(self->t, 8.0f, 11.0f, &f)) {
		cEmbeddedFXBike_SetThrottle(efx, f * 0.5f + 0.5f, 0.0f);
	} else if (FX_InRange(self->t, 15.0f, 16.0f, &f)) {
		cEmbeddedFXBike_SetThrottle(efx, 1.0f - f * 0.8f, 0.0f);
	} else if (FX_InRange(self->t, 18.0f, 22.0f, &f)) {
		cEmbeddedFXBike_SetThrottle(efx, f * 0.5f, 0.0f);
	} else if (FX_InRange(self->t, 22.0f, 23.0f, &f)) {
		cEmbeddedFXBike_SetThrottle(efx, f * 0.5f + 0.5f, 0.0f);
	} else if (FX_InRange(self->t, 23.0f, 25.0f, &f)) {
		cEmbeddedFXBike_SetThrottle(efx, 1.0f, 0.2f - f * 0.2f);
	} else if (FX_InRange(self->t, 25.0f, 26.0f, &f)) {
		cEmbeddedFXBike_SetThrottle(efx, 1.0f, 0.0f);
	}
	if (!FX_InRange(self->t, 23.0f, 25.0f, &f)) {
		CGameLensFlare_SetOn(&self->flare, 0);
		self->tPrev = self->t;
		return;
	}
	vec3_t tmp;
	CGameLensFlare_SetPos(&self->flare, *cEmbeddedFXBike_EnginePos(efx, &tmp));
	CGameLensFlare_SetOn(&self->flare, 1);
	float v;
	if (0.1f > f) {
		float g = f * 10.0f;
		v = g * g * (2.0f - g) * 0.5f;
	} else {
		float g = (f - 0.1f) * 1.1f;
		v = 0.5f - (3.0f - (g + g)) * (g * g) * 0.5f;
	}
	CGameLensFlare_SetIntensity(&self->flare, v);
	CGameLensFlare_SetScale(&self->flare, 1.0f);
	CGameLensFlare_SetType(&self->flare, 3);
	self->tPrev = self->t;
}

void cObjElementFXSceneYakuzzi_Update(cObjElementFXSceneYakuzzi *self, cDebris *d, float dt) {
	CGameParticles *sp = &self->particles;
	vec3_t pos, rot, dir;

	(void)dt;
	if (!FXS_Alive(&self->ent)) {
		d->life = 0.0f;
		return;
	}
	float delta = FXS_Delta();
	self->t = self->t + delta;
	FXS_Follow(self->ent, d, -102.0f, &pos, &rot);
	float et = delta + self->emitT;
	self->emitT = et;
	if (0.03f > et && self->t > delta) {
		return;
	}
	self->emitT = 0.0f;
	sp->birthRate = 0;
	float px = self->dummy.x + 0.0f;
	float py = self->dummy.y + FxElem_Rand(30.0f, 45.0f);
	float pz = self->dummy.z + 0.0f;
	for (int n = 6; n; n--) {
		const vec3_t *r = FX_RandomDir(&dir);
		float rx = r->x, ry = r->y * 0.0f, rz = r->z;
		float len = FxElem_Rand(61.0f, 45.0f);
		sp->initPosMin.x = px + len * rx;
		sp->initPosMin.y = py + len * ry;
		sp->initPosMin.z = pz + len * rz;
		sp->initPosMax = sp->initPosMin;
		float vz = FxElem_Rand(16.0f, -8.0f);
		float vy = FxElem_Rand(50.0f, 20.0f);
		sp->initVelMin.x = FxElem_Rand(16.0f, -8.0f);
		sp->initVelMin.y = vy;
		sp->initVelMin.z = vz;
		sp->initVelMax = sp->initVelMin;
		float az = FxElem_Rand(8.0f, -4.0f);
		float ay = FxElem_Rand(18.0f, 12.0f);
		float ax = FxElem_Rand(8.0f, -4.0f);
		float drag = FxElem_Rand(0.08f, 0.24f);
		sp->initAccMin.x = ax + drag * -sp->initVelMin.x;
		sp->initAccMin.y = ay + drag * -sp->initVelMin.y;
		sp->initAccMin.z = az + drag * -sp->initVelMin.z;
		sp->initAccMax = sp->initAccMin;
		sp->sizeIni = FxElem_Rand(20.0f, 35.0f);
		sp->sizeEnd = FxElem_Rand(20.0f, 85.0f);
		sp->lifeTime = FxElem_Rand(0.6f, 1.3f);
		sp->sizeLife = 1.0f;
		float rot = FxElem_Rand(6.2831855f, 0.0);
		sp->rotIni = rot;
		sp->rotEnd = rot;
		sp->rotVar = FxElem_Rand(1.0471976f, -0.5235988f);
		byte sprite = (byte)((crt_rand() & 1) + 0x11);
		sp->spriteIni = sprite;
		sp->spriteEnd = sprite;
		CGameParticles_Apply(sp);
		CGameParticles_SetColorRampOn(sp, 1);
		CGameParticles_SetColorRamp(sp, FXS_RAMP(GAME_ARRAY(uint32_t, cObjElementFXSceneYakuzzi_Update_430dd8), crt_rand() & 1));
		CGameParticles_EmitDt(sp, 1, 0, FxElem_Rand(0.05f, 0.0));
	}
	self->tPrev = self->t;
}

#define FXS_RAMP_BISHOPS_GLOW 0x430c90
#define FXS_RAMP_BISHOPS_CODEC 0x430cb4	/* [2] */
#define FXBISHOPS_VERTS 0x100

/* a glow and a data codec particle at vertex v of the detritus, the codec one up at y0 */
static void FXS_BishopsEmit(cObjElementFXSceneBishops *self, const vec3_t *v, float y0) {
	CGameParticles *sp = &self->glow;

	sp->birthRate = 0;
	sp->initPosMin = *v;
	sp->initPosMax = sp->initPosMin;
	sp->initVelMin.x = 0.0f;
	sp->initVelMin.y = 0.0f;
	sp->initVelMin.z = 0.0f;
	sp->initVelMax = sp->initVelMin;
	sp->initAccMin.x = 0.0f;
	sp->initAccMin.y = 0.0f;
	sp->initAccMin.z = 0.0f;
	sp->initAccMax = sp->initAccMin;
	sp->lifeTime = FxElem_Rand(0.020000003f, 0.12f);
	double r = FXS_Rnd();
	sp->sizeIni = (float)(r + r + 8.0);
	sp->sizeEnd = FxElem_Rand(5.0f, 40.0f);
	sp->sizeLife = FxElem_Rand(0.019999996f, 0.22f);
	float rot = FxElem_Rand(6.2831855f, 0.0);
	sp->rotIni = rot;
	sp->rotEnd = rot;
	sp->spriteIni = 5;
	sp->spriteEnd = 5;
	sp->rotVar = FxElem_Rand(1.5707964f, -0.7853982f);
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, GAME_ARRAY(uint32_t, cObjElementFXSceneBishops_Update_430c90));
	CGameParticles_Apply(sp);
	CGameParticles_EmitN(sp, 1, 0);

	sp = &self->dataCodec;
	sp->birthRate = 0;
	sp->initPosMin.x = v->x;
	sp->initPosMin.y = FxElem_Rand(40.0f, -20.0f) + y0;
	sp->initPosMin.z = v->z;
	sp->initPosMax = sp->initPosMin;
	sp->initVelMin.x = 0.0f;
	sp->initVelMin.y = 0.0f;
	sp->initVelMin.z = 0.0f;
	sp->initVelMax = sp->initVelMin;
	sp->initAccMin.x = 0.0f;
	sp->initAccMin.y = 1500.0f;
	sp->initAccMin.z = 0.0f;
	sp->initAccMax = sp->initAccMin;
	sp->lifeTime = FxElem_Rand(0.040000007f, 0.14f);
	sp->sizeIni = (float)(FXS_Rnd() + 4.0);
	sp->sizeEnd = (float)(FXS_Rnd() + 7.0);
	sp->rotIni = 0.0f;
	sp->rotEnd = 0.0f;
	sp->rotVar = 0.0f;
	sp->sizeLife = FxElem_Rand(0.10000001f, 0.2f);
	byte sprite = (byte)(crt_rand() % 8);
	sp->spriteIni = sprite;
	sp->spriteEnd = sprite;
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, FXS_RAMP(GAME_ARRAY(uint32_t, cObjElementFXSceneBishops_Update_430cb4), crt_rand() & 1));
	CGameParticles_Apply(sp);
	CGameParticles_EmitN(sp, 1, 0);
}

void cObjElementFXSceneBishops_Update(cObjElementFXSceneBishops *self, cDebris *d, float dt) {
	vec3_t pos, rot;
	vec3_t verts[FXBISHOPS_VERTS];
	float q, f;
	int alpha;
	float k;

	(void)dt;
	if (!FXS_Alive(&self->ent)) {
		d->life = 0.0f;
		return;
	}
	FXS_Follow(self->ent, d, -101.0f, &pos, &rot);
	if (!self->detritus) {
		cEntity *e = World_FindEntity("BishopsDtritus");
		self->detritus = e;
		if (e) {
			CGameModel *dm = Obj_Vtbl(e)->GetModel(e);
			CHECK(CGameModel_IsLoaded(dm));
			self->node = CGameModel_FindRenderNode(dm, "R_Detritus_Tronco");
			CHECK(self->node > -1);
		}
	}
	float delta = FXS_Delta();
	float t = self->t + delta;
	self->t = t;
	if (!FX_InRange(t, 5.2f, 9.7f, &q)) {
		return;
	}
	cEntity *det = self->detritus;
	CGameModel *dm = Obj_Vtbl(det)->GetModel(det);
	CHECK(CGameModel_IsLoaded(dm));
	float radius = Obj_Vtbl(det)->GetRadius(det);
	CGameModel_Vertices(dm, &self->master, NULL, NULL, 1, 0, NULL, (uint32_t)self->node, NULL);
	vec3_t mp = self->master;
	float r18 = radius * 0.7f;
	self->glowT = delta + self->glowT;
	/* the glow column: grows, stays, shrinks at the end */
	if (0.2f > q) {
		k = q * 5.0f;
		alpha = 0xff - (int)(k * 128.0f);
	} else if (q > 0.9f) {
		if (1.0f > q) {
			float e = q - 0.9f;
			alpha = 0x80 - (int)(e * -10.0f * 128.0f);
			k = 1.0f - e * 10.0f;
		} else {
			alpha = 0xff;
			k = 0.0f;
		}
	} else {
		alpha = 0x80;
		k = 1.0f;
	}
	float top = r18 * 4.0f;
	float mx0 = mp.x + 0.0f;
	float mz0 = mp.z + 0.0f;
	uint32_t color = (uint32_t)alpha << 24 | 0x101080;
	float size = r18 * 5.0f;
	for (int i = 0; i < 7; i++) {
		CGameParticles_SetParticle(&self->glow, i, mx0, mp.y + (top - (float)i * r18 * 1.4f * k), mz0, size, color,
			2, 0.0f);
	}
	if (self->glowT > 0.02f) {
		CGameModel_Vertices(dm, verts, NULL, NULL, FXBISHOPS_VERTS, -1, NULL, 0xffffffffu, NULL);
	}
	float r48 = radius * 1.8f;
	for (int i = 0; i < FXBISHOPS_TRAILS; i++) {
		CGameTrail *tr = &self->trails[i];
		float from = (float)i * 0.3f;
		FX_InRange(self->t - 5.2f, from, from + 1.15f, &f);
		float w = f * 2.0f - 1.0f;
		float off = r48 * w;
		int a = (int)__builtin_fabs((double)(w * 255.0f));
		CGameTrail_SetTransp(tr, (byte)a);
		if (a >= 0xff) {
			CGameTrail_Clear(tr);
			continue;
		}
		CGameTrail_Reset(tr);
		vec3_t o = { mx0, mp.y + 500.0f, mz0 };
		CGameTrail_SetOrient(tr, o, 1);
		double r1 = FXS_Rnd();
		double r2 = FXS_Rnd();
		CGameTrail_SetUV(tr, (float)(r2 + r2 + 0.0), (float)(r1 + r1 + 0.0));
		CGameTrail_SetULen(tr, 2.0f, 2.0f);
		float width = radius * 3.5f;
		float zFront = width * -0.5f;
		float zBack = width * 0.5f;
		/* two points a row, the middle row brightest */
		for (int n = 0, h = -0x69; h < 0x78; n++, h += 0xf) {
			int dn = n - 7;
			uint32_t c = (uint32_t)(0x7f00 - (dn < 0 ? -dn : dn) * 0xf00) | 0x80200020u;
			float y = (float)h * 0.1f + off;
			vec3_t v, r;
			v.x = 0.0f;
			v.y = y;
			v.z = zFront;
			const vec3_t *p = Vec3_Rotate(&r, &v, &rot);
			vec3_t pt = { mp.x + p->x, p->y + mp.y, p->z + mp.z };
			CGameTrail_Add(tr, pt, width, c);
			v.x = 0.0f;
			v.y = y;
			v.z = zBack;
			p = Vec3_Rotate(&r, &v, &rot);
			pt.x = mp.x + p->x;
			pt.y = p->y + mp.y;
			pt.z = p->z + mp.z;
			CGameTrail_Add(tr, pt, width, c);
		}
		if (a >= 0x8c || !(self->glowT > 0.02f)) {
			continue;
		}
		/* particles at the vertices at the height of the row */
		float y0 = mp.y + off;
		int count = 0;
		for (int j = 0; j < FXBISHOPS_VERTS; j++) {
			if (8.0f > (float)__builtin_fabs((double)(verts[j].y - y0))) {
				FXS_BishopsEmit(self, &verts[j], y0);
				count++;
			}
			if (count > 12) {
				break;
			}
		}
	}
	if (self->glowT > 0.02f) {
		self->glowT = 0.0f;
	}
}

#define FXDECON_VERTS 0x30

/* the rays of a live slot i: the curve from a vertex of the target to the end point, the glow particles at both
 * ends (4 a ray) */
static void FXS_DeconRay(cObjElementFXSceneDecontamination *self, int i, CGameModel *target, float delta) {
	CGameTrail *tr = &self->trails[i];
	vec3_t P[5], coefs[GAMETRAIL_MAX_BEZIER], pt, tmp;

	float q = self->life[i] / 0.1f;
	int a = 0xff - (int)(Float_MinSS(1.0f, q) * 255.0f);
	CGameTrail_Reset(tr);
	CGameTrail_SetTransp(tr, (byte)a);
	CGameModel_Vertices(target, &P[0], NULL, NULL, 1, self->vertex[i], NULL, 0xffffffffu, NULL);
	P[4] = self->end[i];
	float sx = (P[4].x - P[0].x) * 0.2f;
	float sy = (P[4].y - P[0].y) * 0.2f;
	float sz = (P[4].z - P[0].z) * 0.2f;
	for (int j = 1; j < 4; j++) {
		float rr = FxElem_Rand(10.0f, 2.0f);
		const vec3_t *dir = FX_RandomDir(&tmp);
		float dx = rr * dir->x, dy = dir->y * rr, dz = dir->z * rr;
		float fj = (float)j;
		vec3_t *p = &self->pts[i][j - 1];
		const vec3_t *v = &self->vel[i][j - 1];
		P[j].z = p->z + ((P[0].z + sz * fj) + dz);
		P[j].x = p->x + ((sx * fj + P[0].x) + dx);
		P[j].y = p->y + ((sy * fj + P[0].y) + dy);
		p->x = v->x * delta + p->x;
		p->y = v->y * delta + p->y;
		p->z = delta * v->z + p->z;
	}
	CGameTrail_BezierCoefs(tr, 5, P, coefs);
	int fade = 0x10;
	int n = CGameTrail_GetMaxTrails(tr);
	if (n - 1 > 0) {
		float last = (float)(n - 2);
		for (int s = 0; s < n - 1; s++) {
			CGameTrail_BezierPoint(tr, 5, coefs, &pt, (float)s / last);
			if (s > 0 && s < n - 2) {
				float rr = FxElem_Rand(2.8f, 1.0f);
				const vec3_t *dir = FX_RandomDir(&tmp);
				pt.x = pt.x + dir->x * rr;
				pt.y = pt.y + dir->y * rr;
				pt.z = pt.z + dir->z * rr;
			}
			fade += (int)((uint32_t)s >> 1);
			float w = self->width[i] + FxElem_Rand(0.5f, -0.25f);
			CGameTrail_Add(tr, pt, w, (self->color[i] & 0xffffff) | (uint32_t)fade << 24);
		}
	}
	CGameParticles *sp = &self->particles;
	int k = i * 4;
	int c = a + 0xc0 > 0xff ? 0xff : a + 0xc0;
	CGameParticles_SetParticle(sp, k, P[0].x, P[0].y, P[0].z, 170.0f, (uint32_t)c << 24 | 0x4040ff, 2, 0.0f);
	c = a + 8 > 0xff ? 0xff : a + 8;
	CGameParticles_SetParticle(sp, k + 1, P[0].x, P[0].y, P[0].z, 45.0f, (uint32_t)c << 24 | 0xa4a4ff, 2, 0.0f);
	c = fade + 0xc0 + a > 0xff ? 0xff : fade + 0xc0 + a;
	CGameParticles_SetParticle(sp, k + 2, P[4].x, P[4].y, P[4].z, 60.0f, (uint32_t)c << 24 | 0x5c5cff, 2, 0.0f);
	c = fade + 0x10 + a > 0xff ? 0xff : fade + 0x10 + a;
	CGameParticles_SetParticle(sp, k + 3, P[4].x, P[4].y, P[4].z, 15.0f, (uint32_t)c << 24 | 0xa4a4ff, 2, 0.0f);
}

/* a new ray in slot i: life, width, color, the end on a circle around the capsule at a random height, the vertex
 * of the target nearest to its height and the moving inner points */
static void FXS_DeconSpawn(cObjElementFXSceneDecontamination *self, int i, CGameModel *target) {
	vec3_t verts[FXDECON_VERTS], tmp;
	int idx[FXDECON_VERTS];

	self->wait[i] = 0.0f;
	self->life[i] = FxElem_Rand(0.17000002f, 0.13f);
	self->width[i] = FxElem_Rand(2.5f, 2.0f);
	float g = FxElem_Rand(13.0f, 32.0f);
	float b = FxElem_Rand(13.0f, 32.0f);
	float r = FxElem_Rand(59.0f, 196.0f);
	self->color[i] = (uint32_t)(int)r | ((((uint32_t)(int)g | 0x1000) << 8 | (uint32_t)(int)b) << 8);
	double ang = (double)FxElem_Rand(6.2831855f, 0.0);
	float cx = (float)Game_Cos(ang) * 125.0f;
	float y = Rand_FloatRange(-self->height, self->height);
	float sz = (float)Game_Sin(ang) * 125.0f;
	vec3_t *e = &self->end[i];
	e->x = cx - 20861.0f;
	e->y = y + 1065.0f;
	e->z = sz - 27482.0f;
	float ey = e->y;
	CGameModel_Vertices(target, verts, NULL, NULL, FXDECON_VERTS, -1, idx, 0xffffffffu, NULL);
	float best = 3.4028235e+38f;
	for (int k = 0; k < FXDECON_VERTS; k++) {
		float dy = (float)__builtin_fabs((double)(verts[k].y - ey));
		if (best >= dy) {
			self->vertex[i] = idx[k];
			best = dy;
		}
	}
	for (int j = 0; j < FXDECON_POINTS; j++) {
		float rr = FxElem_Rand(15.0f, 5.0f);
		const vec3_t *dir = FX_RandomDir(&tmp);
		self->pts[i][j].x = rr * dir->x;
		self->pts[i][j].y = dir->y * rr;
		self->pts[i][j].z = dir->z * rr;
		rr = FxElem_Rand(7.5f, 1.0f);
		dir = FX_RandomDir(&tmp);
		self->vel[i][j].x = dir->x * rr;
		self->vel[i][j].y = dir->y * rr;
		self->vel[i][j].z = dir->z * rr;
	}
}

void cObjElementFXSceneDecontamination_Update(cObjElementFXSceneDecontamination *self, cDebris *d,
	float dt) {
	vec3_t pos, rot;

	(void)dt;
	if (!FXS_Alive(&self->ent)) {
		d->life = 0.0f;
		return;
	}
	FXS_Follow(self->ent, d, -100.0f, &pos, &rot);
	float delta = FXS_Delta();
	self->t2 = delta + self->t2;
	self->t = delta + self->t;
	if (!self->target) {
		cEntity *e = World_FindEntity(self->targetName);
		self->target = e;
		if (!e) {
			return;
		}
		CHECK(CGameModel_IsLoaded(Obj_Vtbl(e)->GetModel(e)));
	}
	cEntity *te = self->target;
	CHECK(WithLife_Vtbl(te)->GetActCtrl(te));
	byte active = cActCtrl_IsActive(WithLife_Vtbl(te)->GetActCtrl(te), self->action);
	float actT = cActCtrl_GetLayer(WithLife_Vtbl(te)->GetActCtrl(te), 0)->time;
	CHECK(CGameModel_IsLoaded(Obj_Vtbl(self->ent)->GetModel(self->ent)));
	CGameModel *target = Obj_Vtbl(te)->GetModel(te);
	CHECK(CGameModel_IsLoaded(target));
	for (int i = 0; i < FXDECON_RAYS; i++) {
		if (self->life[i] > 0.0f) {
			float life = self->life[i] - delta;
			self->life[i] = life;
			if (life > 0.0f) {
				FXS_DeconRay(self, i, target, delta);
			}
			if (!(0.0f < self->life[i])) {
				CGameTrail_Clear(&self->trails[i]);
				CGameParticles_KillRange(&self->particles, i * 4, i * 4 + 4);
			}
			continue;
		}
		if (!active) {
			continue;
		}
		/* the rays while the action of the target is in its time window */
		float lo = self->height == 80.0f ? 4.0f : 3.8f;
		if (!(actT >= lo)) {
			continue;
		}
		float hi = self->height == 80.0f ? 7.1f : 8.0f;
		if (!(hi >= actT)) {
			continue;
		}
		if (self->wait[i] > 0.0f) {
			self->wait[i] = self->wait[i] - delta;
			continue;
		}
		FXS_DeconSpawn(self, i, target);
	}
}

/* ---- BeatPickus: the beating of Pickus, dust, smoke, sparks and the hits ---- */

#define FXS_RAMP_BEAT_SMOKE 0x430e20	/* [2] */
#define FXS_RAMP_BEAT_DUST 0x430e68	/* [2] */
#define FXS_RAMP_BEAT_SPARKS 0x430eb0	/* [2] */
#define FXS_RAMP_BEAT_HIT 0x430ef8
#define FXS_RAMP_BEAT_HITSPARKS 0x430f1c	/* [2] */

/* the offsets of the puffs of the hits (rva 0x3cd720, 0x3cd730, 0x3cd710 in that order) */
static const vec3_t fxs_beatPuffs[4] = {
	{ 8.0f, 5.0f, -15.0f }, { -25.0f, 2.0f, -30.0f }, { -45.0f, -35.0f, 45.0f }, { 30.0f, 14.0f, 45.0f },
};

static inline void FXS_Pos(CGameParticles *sp, float x, float y, float z) {
	sp->initPosMin.x = x;
	sp->initPosMin.y = y;
	sp->initPosMin.z = z;
	sp->initPosMax = sp->initPosMin;
}

static inline void FXS_Vel(CGameParticles *sp, float x, float y, float z) {
	sp->initVelMin.x = x;
	sp->initVelMin.y = y;
	sp->initVelMin.z = z;
	sp->initVelMax = sp->initVelMin;
}

static inline void FXS_Acc(CGameParticles *sp, float x, float y, float z) {
	sp->initAccMin.x = x;
	sp->initAccMin.y = y;
	sp->initAccMin.z = z;
	sp->initAccMax = sp->initAccMin;
}

static inline void FXS_Sprites(CGameParticles *sp, byte ini, byte end) {
	sp->spriteIni = ini;
	sp->spriteEnd = end;
}

/* the dust under Pickus, 10 particles */
static void FXS_BeatDust(cObjElementFXSceneBeatPickus *self) {
	CGameParticles *sp = &self->dust;
	vec3_t dir;

	sp->birthRate = 0;
	float py = self->dummy.y - 520.0f;
	float px = (self->dummy.x + 0.0f) + 0.0f;
	float pz = (self->dummy.z - 580.0f) + 0.0f;
	for (int n = 10; n; n--) {
		const vec3_t *r = FX_RandomDir(&dir);
		float a = r->x, b = r->y * 0.15f, c = r->z;
		float len = FxElem_Rand(40.0f, 25.0f);
		a = len * a;
		b = len * b;
		c = len * c;
		float ry = FxElem_Rand(130.0f, 0.0);
		FXS_Pos(sp, a + px, b + (ry + py), c + pz);
		float sx = (crt_rand() & 1) ? 1.0f : -1.0f;
		float sy = (crt_rand() & 1) ? 1.0f : -1.0f;
		float vz = FxElem_Rand(130.0f, 65.0f);
		float vy = FxElem_Rand(120.0f, 25.0f);
		vz = sx * vz;
		FXS_Vel(sp, FxElem_Rand(130.0f, 65.0f) * sy, vy, vz);
		float az = FxElem_Rand(30.0f, -15.0f);
		float ay = FxElem_Rand(25.0f, 20.0f);
		float ax = FxElem_Rand(30.0f, -15.0f);
		float s = FxElem_Rand(0.08f, 0.24f);
		FXS_Acc(sp, ax + -sp->initVelMin.x * s, ay + -sp->initVelMin.y * s, az + -sp->initVelMin.z * s);
		sp->sizeIni = FxElem_Rand(10.0f, 30.0f);
		sp->sizeEnd = FxElem_Rand(90.0f, 145.0f);
		sp->sizeLife = 1.0f;
		sp->lifeTime = FxElem_Rand(0.6f, 0.6f);
		float rot = FxElem_Rand(6.2831855f, 0.0);
		sp->rotIni = rot;
		sp->rotEnd = rot;
		sp->rotVar = FxElem_Rand(1.0471976f, -0.5235988f);
		byte spr = (byte)((crt_rand() & 1) + 0x11);
		FXS_Sprites(sp, spr, spr);
		CGameParticles_Apply(sp);
		CGameParticles_SetColorRampOn(sp, 1);
		CGameParticles_SetColorRamp(sp, FXS_RAMP(GAME_ARRAY(uint32_t, cObjElementFXSceneBeatPickus_Update_430e20), crt_rand() & 1));
		CGameParticles_EmitDt(sp, 1, 0, FxElem_Rand(0.05f, 0.0));
	}
}

/* the debris flying off, 2 to 9 particles */
static void FXS_BeatDebris(cObjElementFXSceneBeatPickus *self) {
	CGameParticles *sp = &self->debris;
	vec3_t dir;

	sp->birthRate = 0;
	float bx = self->dummy.x + 0.0f;
	float by = self->dummy.y - 500.0f;
	float bz = self->dummy.z - 580.0f;
	if (crt_rand() % 8 + 2 <= 0) {
		return;
	}
	bx = bx + 0.0f;
	bz = bz + 0.0f;
	int i = 0;
	do {
		const vec3_t *r = FX_RandomDir(&dir);
		float a = r->x, b = r->y * 0.0f, c = r->z;
		float len = FxElem_Rand(25.0f, 5.0f);
		a = a * len;
		b = b * len;
		c = c * len;
		float ry = FxElem_Rand(130.0f, 0.0);
		FXS_Pos(sp, bx + a, (by + ry) + b, bz + c);
		float v1 = FxElem_Rand(180.0f, -90.0f);
		float v2 = FxElem_Rand(180.0f, -90.0f);
		float v3 = FxElem_Rand(180.0f, -90.0f);
		r = FX_RandomDir(&dir);
		FXS_Vel(sp, v3 * r->x, r->y * v2, r->z * v1);
		float az = FxElem_Rand(24.0f, -12.0f);
		float ay = FxElem_Rand(18.0f, 12.0f);
		float ax = FxElem_Rand(24.0f, -12.0f);
		float s = FxElem_Rand(0.08f, 0.24f);
		FXS_Acc(sp, ax + -sp->initVelMin.x * s, ay + -sp->initVelMin.y * s, az + -sp->initVelMin.z * s);
		sp->sizeIni = FxElem_Rand(45.0f, 45.0f);
		sp->sizeEnd = FxElem_Rand(20.0f, 135.0f);
		sp->lifeTime = FxElem_Rand(0.10000001f, 0.2f);
		sp->sizeLife = FxElem_Rand(0.10000001f, 0.2f);
		float rot = FxElem_Rand(6.2831855f, 0.0);
		sp->rotIni = rot;
		sp->rotEnd = rot;
		sp->rotVar = FxElem_Rand(3.1415927f, -1.5707964f);
		byte spr = (byte)(crt_rand() % 3 + 0xd);
		FXS_Sprites(sp, spr, spr);
		CGameParticles_Apply(sp);
		CGameParticles_SetColorRampOn(sp, 1);
		CGameParticles_SetColorRamp(sp, FXS_RAMP(GAME_ARRAY(uint32_t, cObjElementFXSceneBeatPickus_Update_430e68), crt_rand() & 1));
		CGameParticles_EmitDt(sp, 1, 0, FxElem_Rand(0.05f, 0.0));
		i++;
	} while (i < crt_rand() % 8 + 2);
}

/* the dir from c to p (unchanged at length 0) turned by three random angles of +-0.2 */
static void FXS_BeatScatter(vec3_t *out, const vec3_t *p, float cx, float cy, float cz) {
	vec3_t ang, n;

	float a1 = FxElem_Rand(0.4f, -0.2f);
	float a2 = FxElem_Rand(0.4f, -0.2f);
	ang.x = FxElem_Rand(0.4f, -0.2f);
	ang.y = a2;
	ang.z = a1;
	float dx = p->x - cx, dz = p->z - cz, dy = p->y - cy;
	float len = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	if (len == 0.0f) {
		n.x = dx;
		n.y = dy;
		n.z = dz;
	} else {
		float inv = 1.0f / len;
		n.x = inv * dx;
		n.y = inv * dy;
		n.z = inv * dz;
	}
	Vec3_Rotate(out, &n, &ang);
}

/* the dir turned by three random angles in -half..range - half */
static const vec3_t *FXS_BeatTurn(vec3_t *out, const vec3_t *dir, double range, double add) {
	vec3_t ang;

	float b1 = FxElem_Rand(range, add);
	float b2 = FxElem_Rand(range, add);
	ang.x = FxElem_Rand(range, add);
	ang.y = b2;
	ang.z = b1;
	return Vec3_Rotate(out, dir, &ang);
}

/* the sparks of a blow, 8 to 13 particles from a point near the dummy */
static void FXS_BeatSparks(cObjElementFXSceneBeatPickus *self) {
	CGameParticles *sp = &self->sparks;
	vec3_t dir, rdir, p, tmp;

	sp->birthRate = 0;
	CGameParticles_SetSparksOn(sp, 1);
	float cx = self->dummy.x + 0.0f;
	float cy = self->dummy.y - 500.0f;
	float cz = self->dummy.z - 580.0f;
	float len = FxElem_Rand(20.0f, 15.0f);
	const vec3_t *r = FX_RandomDir(&dir);
	float px = len * r->x, py = r->y * len, pz = r->z * len;
	float ry = FxElem_Rand(130.0f, 0.0);
	p.x = px + (cx + 0.0f);
	p.y = py + (ry + cy);
	p.z = pz + (cz + 0.0f);
	FXS_BeatScatter(&rdir, &p, cx, cy, cz);
	if (crt_rand() % 6 + 8 <= 0) {
		return;
	}
	int i = 0;
	do {
		FXS_Pos(sp, p.x, p.y, p.z);
		float spd = FxElem_Rand(550.0f, 250.0f);
		r = FXS_BeatTurn(&tmp, &rdir, 0.4f, -0.2f);
		FXS_Vel(sp, spd * r->x, r->y * spd, r->z * spd);
		float s = FxElem_Rand(0.060000002f, 0.62f);
		FXS_Acc(sp, s * -sp->initVelMin.x, s * -sp->initVelMin.y, s * -sp->initVelMin.z);
		float size = FxElem_Rand(15.0f, 5.0f);
		sp->sizeIni = size;
		sp->sizeEnd = size;
		sp->sizeLife = 1.0f;
		sp->rotIni = 0.0f;
		sp->rotEnd = 0.0f;
		sp->rotVar = 0.0f;
		sp->sparkVelScale = 1.0f;
		sp->lifeTime = FxElem_Rand(0.20000002f, 0.4f);
		sp->sparkMaxSize = (float)(FXS_Rnd() + 2.0);
		FXS_Sprites(sp, (byte)((crt_rand() & 1) + 0x41), 0x44);
		CGameParticles_Apply(sp);
		CGameParticles_SetColorRampOn(sp, 1);
		CGameParticles_SetColorRamp(sp, FXS_RAMP(GAME_ARRAY(uint32_t, cObjElementFXSceneBeatPickus_Update_430eb0), crt_rand() & 1));
		CGameParticles_EmitDt(sp, 1, 0, FxElem_Rand(0.1f, 0.0));
		i++;
	} while (i < crt_rand() % 6 + 8);
}

/* a hit: two flashes and 8 to 13 sparks falling down */
static void FXS_BeatHit(cObjElementFXSceneBeatPickus *self) {
	CGameParticles *sp = &self->debris;
	vec3_t dir, rdir, ang, tmp, p;

	self->hitT = FxElem_Rand(0.59999996f, 0.3f);
	sp->birthRate = 0;
	CGameParticles_SetSparksOn(sp, 0);
	float cx = self->dummy.x + 0.0f;
	float cy = self->dummy.y - 460.0f;
	float cz = self->dummy.z - 580.0f;
	float len = FxElem_Rand(5.0f, 4.0f);
	const vec3_t *r = FX_RandomDir(&dir);
	float px = len * r->x, py = r->y * len, pz = r->z * len;
	float j1 = FxElem_Rand(44.0f, -22.0f);
	float j2 = FxElem_Rand(7.0f, 8.0f);
	float j3 = FxElem_Rand(44.0f, -22.0f);
	p.x = px + (j3 + cx);
	p.y = py + (j2 + cy);
	p.z = pz + (j1 + cz);
	float a1 = FxElem_Rand(0.4f, -0.2f);
	float a2 = FxElem_Rand(0.4f, -0.2f);
	const vec3_t up = { 0.0f, 1.0f, 0.0f };
	ang.x = FxElem_Rand(0.4f, -0.2f);
	ang.y = a2;
	ang.z = a1;
	Vec3_Rotate(&rdir, &up, &ang);
	FXS_Pos(sp, p.x, p.y, p.z);
	FXS_Vel(sp, 0.0f, 0.0f, 0.0f);
	FXS_Acc(sp, 0.0f, 0.0f, 0.0f);
	CGameParticles_SetColorRamp(sp, GAME_ARRAY(uint32_t, cObjElementFXSceneBeatPickus_Update_430ef8));
	sp->sizeIni = FxElem_Rand(10.0f, 10.0f);
	sp->sizeEnd = FxElem_Rand(15.0f, 95.0f);
	sp->sizeLife = 0.15f;
	FXS_Sprites(sp, 6, 6);
	sp->lifeTime = FxElem_Rand(0.019999996f, 0.18f);
	CGameParticles_Apply(sp);
	CGameParticles_EmitN(sp, 1, 1);
	CGameParticles_SetColorRamp(sp, GAME_ARRAY(uint32_t, cObjElementFXSceneBeatPickus_Update_430ef8));
	double rr = FXS_Rnd();
	sp->sizeIni = (float)(rr + rr + 4.0);
	sp->sizeEnd = FxElem_Rand(15.0f, 55.0f);
	sp->sizeLife = 0.15f;
	FXS_Sprites(sp, 5, 5);
	sp->lifeTime = FxElem_Rand(0.02000001f, 0.16f);
	CGameParticles_Apply(sp);
	CGameParticles_EmitN(sp, 1, 1);
	CGameParticles_SetSparksOn(sp, 1);
	sp->birthRate = 0;
	if (crt_rand() % 6 + 8 <= 0) {
		return;
	}
	int i = 0;
	do {
		FXS_Pos(sp, p.x, p.y, p.z);
		float spd = FxElem_Rand(250.0f, 150.0f);
		r = FXS_BeatTurn(&tmp, &rdir, 0.2f, -0.1f);
		FXS_Vel(sp, spd * r->x, r->y * spd, r->z * spd);
		float s = FxElem_Rand(0.060000002f, 0.62f);
		FXS_Acc(sp, -sp->initVelMin.x * s + 0.0f, -sp->initVelMin.y * s - 280.0f, -sp->initVelMin.z * s + 0.0f);
		float size = FxElem_Rand(4.0f, 4.0f);
		sp->sizeIni = size;
		sp->sizeEnd = size;
		sp->sizeLife = 1.0f;
		sp->rotIni = 0.0f;
		sp->rotEnd = 0.0f;
		sp->rotVar = 0.0f;
		sp->sparkVelScale = 1.0f;
		sp->lifeTime = FxElem_Rand(0.20000002f, 0.4f);
		sp->sparkMaxSize = FxElem_Rand(1.9f, 0.6f);
		FXS_Sprites(sp, (byte)((crt_rand() & 1) + 0x41), 0x44);
		CGameParticles_Apply(sp);
		CGameParticles_SetColorRampOn(sp, 1);
		CGameParticles_SetColorRamp(sp, FXS_RAMP(GAME_ARRAY(uint32_t, cObjElementFXSceneBeatPickus_Update_430f1c), crt_rand() & 1));
		CGameParticles_EmitDt(sp, 1, 0, FxElem_Rand(0.1f, 0.0));
		i++;
	} while (i < crt_rand() % 6 + 8);
}

/* the puffs of smoke at four points around Pickus, two each */
static void FXS_BeatPuffs(cObjElementFXSceneBeatPickus *self) {
	CGameParticles *sp = &self->dust;
	vec3_t dir;

	sp->birthRate = 0;
	for (int k = 0; k < 4; k++) {
		const vec3_t *o = &fxs_beatPuffs[k];
		float bx = self->dummy.x + 0.0f;
		float by = self->dummy.y - 470.0f;
		float bz = self->dummy.z - 600.0f;
		float X = o->x + bx, Y = o->y + by, Z = bz + o->z;
		for (int n = 2; n; n--) {
			double rr = FXS_Rnd();
			float r = (float)(rr + rr + 0.0);
			const vec3_t *d = FX_RandomDir(&dir);
			FXS_Pos(sp, r * d->x + X, d->y * r + Y, d->z * r + Z);
			float vz = FxElem_Rand(20.0f, -10.0f);
			float vy = FxElem_Rand(95.0f, 25.0f);
			FXS_Vel(sp, FxElem_Rand(20.0f, -10.0f), vy, vz);
			float az = FxElem_Rand(6.0f, -3.0f);
			float ay = FxElem_Rand(6.0f, 2.0f);
			float ax = FxElem_Rand(6.0f, -3.0f);
			float s = FxElem_Rand(0.059999987f, 0.18f);
			FXS_Acc(sp, -sp->initVelMin.x * s + ax, -sp->initVelMin.y * s + ay, -sp->initVelMin.z * s + az);
			rr = FXS_Rnd();
			sp->sizeIni = (float)(rr + rr + 3.0);
			sp->sizeEnd = FxElem_Rand(12.0f, 12.0f);
			sp->sizeLife = 1.0f;
			sp->lifeTime = FxElem_Rand(0.6999999f, 0.6f);
			float rot = FxElem_Rand(6.2831855f, 0.0);
			sp->rotIni = rot;
			sp->rotEnd = rot;
			sp->rotVar = FxElem_Rand(0.7853982f, -0.3926991f);
			byte spr = (byte)((crt_rand() & 1) + 0x11);
			FXS_Sprites(sp, spr, spr);
			CGameParticles_Apply(sp);
			CGameParticles_SetColorRampOn(sp, 1);
			CGameParticles_SetColorRamp(sp, FXS_RAMP(GAME_ARRAY(uint32_t, cObjElementFXSceneBeatPickus_Update_430e20), crt_rand() & 1));
			CGameParticles_EmitDt(sp, 1, 0, FxElem_Rand(0.05f, 0.0));
		}
	}
}

void cObjElementFXSceneBeatPickus_Update(cObjElementFXSceneBeatPickus *self, cDebris *d, float dt) {
	vec3_t pos, rot;
	float q;

	(void)dt;
	if (!FXS_Alive(&self->ent)) {
		d->life = 0.0f;
		return;
	}
	float delta = FXS_Delta();
	self->t = self->t + delta;
	FXS_Follow(self->ent, d, -104.0f, &pos, &rot);
	/* each part at most every 0.03 (the first frame always); a frame that skips one skips the rest too */
	if (FX_InRange(self->t, 13.0f, 34.0f, &q)) {
		float acc = self->dustT + delta;
		self->dustT = acc;
		if (0.03f > acc && self->t > delta) {
			return;
		}
		self->dustT = 0.0f;
		FXS_BeatDust(self);
	}
	if (FX_InRange(self->t, 13.5f, 33.0f, &q)) {
		float acc = delta + self->debrisT;
		self->debrisT = acc;
		if (0.03f > acc && self->t > delta) {
			return;
		}
		self->debrisT = 0.0f;
		FXS_BeatDebris(self);
	}
	if (FX_InRange(self->t, 13.5f, 33.0f, &q)) {
		self->sparkT = delta + self->sparkT;
		float thr = FxElem_Rand(0.02f, 0.0) + 0.02f;
		if (thr > self->sparkT && self->t > delta) {
			return;
		}
		self->sparkT = 0.0f;
		FXS_BeatSparks(self);
	}
	if (FX_InRange(self->t, 36.0f, 47.0f, &q)) {
		float w = self->hitT - delta;
		self->hitT = w;
		if (!(0.0f < w)) {
			FXS_BeatHit(self);
		}
		float c = self->dustT + delta;
		self->dustT = c;
		if (c > 0.025f) {
			self->dustT = 0.0f;
			FXS_BeatPuffs(self);
		}
	}
	self->tPrev = self->t;
}

/* ---- BillArrives_A: Bill arrives in the capsule ---- */

#define FXS_RAMP_BILL_SMOKE 0x430d00	/* [2] */
#define FXS_RAMP_BILL_CAPSULE 0x430d48	/* [2] */
#define FXS_RAMP_BILL_JETA 0x430d90	/* [2] */
extern uint32_t FXS_BILL_JETA_RAMPS[18];	/* [2 * 9] */
#define FXS_RAMP_BILL_JETB 0x430db4
#define FXBILL_JETS 3
/* the jets of the capsule: the particles of its embedded effect (3 from + 0x8) */
#define FXS_CapsuleJet(efx, k) (&((cEmbeddedFXCapsule *)(efx))->motors[k])

static inline const vec3_t *FXS_Neg(vec3_t *out, const vec3_t *v) {
	out->x = -v->x;
	out->y = -v->y;
	out->z = -v->z;
	return out;
}

/* the jets off: no birth, all particles but the first killed */
static void FXS_BillJetsOff(void *efx) {
	for (int k = 0; k < FXBILL_JETS; k++) {
		CGameParticles *sp = FXS_CapsuleJet(efx, k);
		sp->birthRate = 0;
		CGameParticles_Apply(sp);
		int i = 1;
		float f = 0.0f;
		do {
			CGameParticles_Kill(sp, i);
			i++;
			f = f + 0.41887903f;
		} while (6.2831855f > f);
	}
}

/* the color of a jet particle: the alpha 255 - (192..223) * fade */
static uint32_t FXS_BillJetColor(float fade, uint32_t rgb) {
	int a = crt_rand() % 32 + 0xc0;
	return (uint32_t)(-1 - (int)((float)a * fade)) << 24 | rgb;
}

/* the flames of the jets: a core and a ring of particles placed by hand */
static void FXS_BillJetsFlame(cObjElementFXSceneBillArrives_A *self, void *efx) {
	vec3_t ang, v, out;
	float frac = 0.0f;
	float fade = 1.0f;

	if (FX_InRange(self->t, 12.1f, 12.4f, &frac)) {
		fade = 1.0f - frac;
	}
	float scale = self->t >= 10.7f ? 1.25f : 1.0f;
	float coreY = scale * 16.0f;
	float ringSize = scale * 50.0f;
	for (int k = 0; k < FXBILL_JETS; k++) {
		CGameParticles *sp = FXS_CapsuleJet(efx, k);
		float rot = FxElem_Rand(6.2831855f, 0.0);
		float size = FxElem_Rand(20.0f, 170.0f);
		uint32_t c = FXS_BillJetColor(fade, 0xff460a);
		CGameParticles_SetParticle(sp, 0, 0.0f, coreY, 0.0f, size * scale, c, 5, rot);
		int i = 1;
		float f = 0.0f;
		do {
			ang.x = 0.0f;
			ang.z = 0.0f;
			ang.y = FxElem_Rand(0.6f, -0.3f) + f;
			float r = FxElem_Rand(10.0f, 0.0);
			v.z = 0.0f;
			v.x = FxElem_Rand(6.0f, 8.0f) * scale;
			v.y = r * scale + 14.0f;
			Vec3_Rotate(&out, &v, &ang);
			c = FXS_BillJetColor(fade, 0xff2805);
			CGameParticles_SetParticle(sp, i, out.x, out.y, out.z, ringSize, c, 2, 0.0f);
			i++;
			f = f + 0.41887903f;
		} while (6.2831855f > f);
	}
}

/* the smoke of the jets while the capsule comes down (3 < t < 10.9) */
static void FXS_BillJetsSmoke(void *efx) {
	vec3_t n;

	for (int k = 0; k < FXBILL_JETS; k++) {
		CGameParticles *sp = FXS_CapsuleJet(efx, k);
		double r = FXS_Rnd();
		FXS_Pos(sp, 0.0f, (float)(r + r + 16.0), 0.0f);
		float vy = FxElem_Rand(110.0f, 390.0f);
		float vxz = vy * 0.0f;
		FXS_Vel(sp, vxz, vy, vxz);
		float az = FxElem_Rand(90.0f, -45.0f);
		float ax = FxElem_Rand(90.0f, -45.0f);
		float s = FxElem_Rand(0.060000002f, 0.62f);
		FXS_Neg(&n, &sp->initVelMin);
		FXS_Acc(sp, ax + n.x * s, n.y * s + 0.0f, az + n.z * s);
		sp->sizeIni = FxElem_Rand(10.0f, 35.0f);
		sp->sizeEnd = FxElem_Rand(20.0f, 95.0f);
		sp->sizeLife = 1.0f;
		sp->lifeTime = FxElem_Rand(0.03999999f, 0.28f);
		float rot = FxElem_Rand(6.2831855f, 0.0);
		sp->rotIni = rot;
		sp->rotEnd = rot;
		FXS_Sprites(sp, 0x37, 0x37);
		sp->rotVar = FxElem_Rand(1.5707964f, -0.7853982f);
		CGameParticles_Apply(sp);
		CGameParticles_SetColorRampOn(sp, 1);
		CGameParticles_SetColorRamp(sp, FXS_RAMP(FXS_BILL_JETA_RAMPS, crt_rand() & 1));
		CGameParticles_EmitDt(sp, 1, 0, FxElem_Rand(0.04f, 0.0));
	}
}

/* the blast of the jets at the landing (10.7 <= t < 10.85) */
static void FXS_BillJetsBlast(void *efx) {
	vec3_t n;

	for (int k = 0; k < FXBILL_JETS; k++) {
		CGameParticles *sp = FXS_CapsuleJet(efx, k);
		float pz = FxElem_Rand(30.0f, -15.0f);
		float py = FxElem_Rand(12.0f, 13.0f);
		FXS_Pos(sp, FxElem_Rand(30.0f, -15.0f), py, pz);
		float vy = FxElem_Rand(150.0f, 440.0f);
		float vxz = vy * 0.0f;
		FXS_Vel(sp, vxz, vy, vxz);
		float az = FxElem_Rand(90.0f, -45.0f);
		float ay = FxElem_Rand(45.0f, -90.0f);
		float ax = FxElem_Rand(90.0f, -45.0f);
		float s = FxElem_Rand(0.060000002f, 0.62f);
		FXS_Neg(&n, &sp->initVelMin);
		FXS_Acc(sp, ax + s * n.x, ay + s * n.y, az + s * n.z);
		sp->sizeIni = FxElem_Rand(20.0f, 145.0f);
		sp->sizeEnd = FxElem_Rand(20.0f, 225.0f);
		sp->sizeLife = 1.0f;
		sp->lifeTime = FxElem_Rand(0.02000001f, 0.38f);
		float rot = FxElem_Rand(6.2831855f, 0.0);
		sp->rotIni = rot;
		sp->rotEnd = rot;
		sp->rotVar = FxElem_Rand(1.5707964f, -0.7853982f);
		FXS_Sprites(sp, 0x37, 0x37);
		CGameParticles_Apply(sp);
		CGameParticles_SetColorRampOn(sp, 1);
		CGameParticles_SetColorRamp(sp, GAME_ARRAY_IN(uint32_t, FXS_BILL_JETA_RAMPS, 36));
		CGameParticles_EmitDt(sp, 1, 0, FxElem_Rand(0.035f, 0.0));
	}
}

/* the smoke of the opening capsule, 5 particles around cp */
static void FXS_BillCapsuleSmoke(cObjElementFXSceneBillArrives_A *self, const vec3_t *cp) {
	CGameParticles *sp = &self->particles;
	vec3_t dir, n;

	sp->birthRate = 0;
	for (int k = 5; k; k--) {
		FX_RandomDir(&dir);
		float r = FxElem_Rand(105.0f, 0.0);
		FXS_Pos(sp, cp->x + r * dir.x, cp->y + r * dir.y, cp->z + r * dir.z);
		float v = FxElem_Rand(60.0f, 20.0f);
		FXS_Vel(sp, v * dir.x, v * dir.y, v * dir.z);
		float az = FxElem_Rand(30.0f, -15.0f);
		float ay = FxElem_Rand(30.0f, -15.0f);
		float ax = FxElem_Rand(30.0f, -15.0f);
		float s = FxElem_Rand(0.060000002f, 0.42f);
		FXS_Neg(&n, &sp->initVelMin);
		FXS_Acc(sp, ax + s * n.x, ay + s * n.y, az + s * n.z);
		sp->sizeIni = FxElem_Rand(30.0f, 465.0f);
		sp->sizeEnd = FxElem_Rand(30.0f, 655.0f);
		sp->sizeLife = 1.0f;
		sp->lifeTime = FxElem_Rand(0.05000007f, 1.3f);
		float rot = FxElem_Rand(6.2831855f, 0.0);
		sp->rotIni = rot;
		sp->rotEnd = rot;
		sp->rotVar = FxElem_Rand(1.5707964f, -0.7853982f);
		byte spr = (byte)((crt_rand() & 1) + 0x11);
		FXS_Sprites(sp, spr, spr);
		CGameParticles_Apply(sp);
		CGameParticles_SetColorRampOn(sp, 1);
		CGameParticles_SetColorRamp(sp, FXS_RAMP(GAME_ARRAY(uint32_t, cObjElementFXSceneBillArrives_A_Update_430d48), crt_rand() & 1));
		CGameParticles_EmitDt(sp, 1, 0, FxElem_Rand(0.04f, 0.0));
	}
}

static const vec3_t fxs_xAxis = { 1.0f, 0.0f, 0.0f };

/* the direction of a particle of a ring: the x axis turned around y by f plus a jitter (added in the order of the original) */
static const vec3_t *FXS_RingDir(vec3_t *out, float f, double jitRange, double jitAdd, int jitFirst) {
	vec3_t ang;

	ang.x = 0.0f;
	ang.z = 0.0f;
	float j = FxElem_Rand(jitRange, jitAdd);
	ang.y = jitFirst ? j + f : f + j;
	return Vec3_Rotate(out, &fxs_xAxis, &ang);
}

/* the dust of the landing at the first dummy of the scene, 20 particles in a circle */
static void FXS_BillDust(cObjElementFXSceneBillArrives_A *self) {
	CGameParticles *sp = &self->particles;
	vec3_t d, n;

	sp->birthRate = 0;
	vec3_t c = self->dummy1;
	float f = 0.0f;
	do {
		FXS_RingDir(&d, f, 1.0, -0.5f, 1);
		float r = FxElem_Rand(10.0f, 65.0f);
		FXS_Pos(sp, c.x + r * d.x, c.y + r * d.y, c.z + r * d.z);
		float v = FxElem_Rand(225.0f, 555.0f);
		FXS_Vel(sp, v * d.x, v * d.y, v * d.z);
		float ay = FxElem_Rand(50.0f, 215.0f);
		float s = FxElem_Rand(0.060000002f, 0.62f);
		FXS_Neg(&n, &sp->initVelMin);
		FXS_Acc(sp, s * n.x + 0.0f, ay + s * n.y, s * n.z + 0.0f);
		sp->sizeIni = FxElem_Rand(10.0f, 45.0f);
		sp->sizeEnd = FxElem_Rand(30.0f, 265.0f);
		sp->sizeLife = 1.0f;
		sp->lifeTime = FxElem_Rand(0.100000024f, 0.45f);
		float rot = FxElem_Rand(6.2831855f, 0.0);
		sp->rotIni = rot;
		sp->rotEnd = rot;
		sp->rotVar = FxElem_Rand(0.7853982f, -0.3926991f);
		byte spr = (byte)((crt_rand() & 1) + 0x11);
		FXS_Sprites(sp, spr, spr);
		CGameParticles_Apply(sp);
		CGameParticles_SetColorRampOn(sp, 1);
		CGameParticles_SetColorRamp(sp, FXS_RAMP(GAME_ARRAY(uint32_t, cObjElementFXSceneBillArrives_A_Update_430d00), crt_rand() & 1));
		CGameParticles_EmitDt(sp, 1, 0, FxElem_Rand(0.05f, 0.0));
		f = f + 0.31415927f;
	} while (6.2831855f > f);
}

/* the steam of the opening capsule: a row of puffs from cp, blown away from the point above it */
static void FXS_BillSteam(cObjElementFXSceneBillArrives_A *self, const vec3_t *cp) {
	CGameParticles *sp = &self->particles;
	vec3_t dir, n, nv;

	float k = -22.0f;
	do {
		float Y = cp->y + 130.0f;
		float X = k + cp->x;
		float Z = cp->z - 75.0f;
		float dy = Y - cp->y;
		float dx = X - cp->x;
		float dz = Z - cp->z;
		float len = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
		if (len == 0.0f) {
			nv.x = dx;
			nv.y = dy;
			nv.z = dz;
		} else {
			float inv = 1.0f / len;
			nv.x = dx * inv;
			nv.y = dy * inv;
			nv.z = dz * inv;
		}
		float r = FxElem_Rand(3.0f, 0.0);
		const vec3_t *d = FX_RandomDir(&dir);
		FXS_Pos(sp, r * d->x + X, d->y * r + Y, d->z * r + Z);
		float r2 = FxElem_Rand(20.0f, 0.0);
		d = FX_RandomDir(&dir);
		float jx = r2 * d->x, jy = d->y * r2, jz = d->z * r2;
		float vy = FxElem_Rand(60.0f, 220.0f);
		float w = FxElem_Rand(95.0f, 685.0f);
		FXS_Vel(sp, jx + (w * nv.x + 0.0f), jy + (vy + w * nv.y), jz + (w * nv.z + 0.0f));
		float ay = FxElem_Rand(20.0f, 75.0f);
		float s = FxElem_Rand(0.060000002f, 0.62f);
		FXS_Neg(&n, &sp->initVelMin);
		FXS_Acc(sp, n.x * s + 0.0f, n.y * s + ay, n.z * s + 0.0f);
		sp->sizeIni = FxElem_Rand(5.0f, 5.0f);
		sp->sizeEnd = FxElem_Rand(5.0f, 35.0f);
		sp->sizeLife = 1.0f;
		sp->lifeTime = FxElem_Rand(0.049999982f, 0.4f);
		float rot = FxElem_Rand(6.2831855f, 0.0);
		sp->rotIni = rot;
		sp->rotEnd = rot;
		sp->rotVar = FxElem_Rand(0.7853982f, -0.3926991f);
		byte spr = (byte)((crt_rand() & 1) + 0x11);
		FXS_Sprites(sp, spr, spr);
		CGameParticles_Apply(sp);
		CGameParticles_SetColorRampOn(sp, 1);
		CGameParticles_SetColorRamp(sp, FXS_RAMP(GAME_ARRAY(uint32_t, cObjElementFXSceneBillArrives_A_Update_430d48), crt_rand() & 1));
		CGameParticles_EmitDt(sp, 1, 0, FxElem_Rand(0.1f, 0.0));
		k = k + 2.0f;
	} while (22.0f > k);
}

/* a puff of dust at the second dummy of the scene, 10 particles in a circle */
static void FXS_BillPuff(cObjElementFXSceneBillArrives_A *self, const vec3_t *off) {
	CGameParticles *sp = &self->particles;
	vec3_t d, n;

	sp->birthRate = 0;
	vec3_t c = self->dummy2;
	float px = c.x + off->x, py = c.y + off->y, pz = c.z + off->z;
	float f = 0.0f;
	do {
		FXS_RingDir(&d, f, 1.8f, -0.9f, 0);
		float r = FxElem_Rand(3.0f, 1.0f);
		FXS_Pos(sp, px + d.x * r, py + d.y * r, pz + d.z * r);
		float vy = FxElem_Rand(25.0f, 20.0f);
		float w = FxElem_Rand(35.0f, 105.0f);
		FXS_Vel(sp, d.x * w + 0.0f, d.y * w + vy, d.z * w + 0.0f);
		float ay = FxElem_Rand(20.0f, 75.0f);
		float s = FxElem_Rand(0.059999943f, 0.72f);
		FXS_Neg(&n, &sp->initVelMin);
		FXS_Acc(sp, n.x * s + 0.0f, n.y * s + ay, n.z * s + 0.0f);
		sp->sizeIni = FxElem_Rand(5.0f, 20.0f);
		sp->sizeEnd = FxElem_Rand(5.0f, 40.0f);
		sp->sizeLife = 1.0f;
		sp->lifeTime = FxElem_Rand(0.050000012f, 0.35f);
		float rot = FxElem_Rand(6.2831855f, 0.0);
		sp->rotIni = rot;
		sp->rotEnd = rot;
		sp->rotVar = FxElem_Rand(1.0471976f, -0.5235988f);
		byte spr = (byte)((crt_rand() & 1) + 0x11);
		FXS_Sprites(sp, spr, spr);
		CGameParticles_Apply(sp);
		CGameParticles_SetColorRampOn(sp, 1);
		CGameParticles_SetColorRamp(sp, FXS_RAMP(GAME_ARRAY(uint32_t, cObjElementFXSceneBillArrives_A_Update_430d00), crt_rand() & 1));
		CGameParticles_EmitDt(sp, 1, 0, FxElem_Rand(0.1f, 0.0));
		f = f + 0.62831855f;
	} while (6.2831855f > f);
}

void cObjElementFXSceneBillArrives_A_Update(cObjElementFXSceneBillArrives_A *self, cDebris *d,
	float dt) {
	vec3_t pos, rot, cp;
	float f;

	(void)dt;
	if (!FXS_Alive(&self->ent)) {
		d->life = 0.0f;
		return;
	}
	if (!self->capsule) {
		cEntity *c = World_FindEntity("BillArrives_ACapsule");
		self->capsule = c;
		CHECK(c && Obj(c)->fx);
	}
	float delta = FXS_Delta();
	self->t = delta + self->t;
	FXS_Follow(self->ent, d, -102.0f, &pos, &rot);
	void *efx = self->capsule ? Obj(self->capsule)->fx : NULL;
	if (efx) {
		float t = self->t;
		if (!(t >= 12.4f)) {
			FXS_BillJetsFlame(self, efx);
			t = self->t;
			if (t > 3.0f && 10.9f > t) {
				float a = delta + self->jetT;
				self->jetT = a;
				if (a > 0.03f) {
					self->jetT = 0.0f;
					FXS_BillJetsSmoke(efx);
				}
			} else if (t >= 10.7f && 10.85f > t) {
				float a = delta + self->jetT;
				self->jetT = a;
				if (a > 0.04f) {
					self->jetT = 0.0f;
					FXS_BillJetsBlast(efx);
				}
			}
		} else if (12.4f > self->tPrev) {
			FXS_BillJetsOff(efx);
		}
		CGameModel *cm = Obj_Vtbl(self->capsule)->GetModel(self->capsule);
		CGameModel_GetPos(cm, &cp);
		CHECK(CGameModel_IsLoaded(cm));
		if (FX_InRange(self->t, 3.0f, 4.0f, &f)) {
			CGameLensFlare_SetPos(&self->flare, cp);
			CGameLensFlare_SetOn(&self->flare, 1);
			CGameLensFlare_SetIntensity(&self->flare, 0.5f - f * 0.5f);
			CGameLensFlare_SetScale(&self->flare, 1.0f);
		} else {
			CGameLensFlare_SetOn(&self->flare, 0);
		}
		if (FX_InRange(self->t, 3.0f, 5.0f, &f)) {
			float a = self->smokeT + delta;
			self->smokeT = a;
			if (a > 0.025f) {
				self->smokeT = 0.0f;
				FXS_BillCapsuleSmoke(self, &cp);
			}
		}
	}
	/* a frame that skips one of the next parts (at most every 0.04) skips the rest too */
	if (FX_InRange(self->t, 11.0f, 12.3f, &f)) {
		float a = self->smokeT + delta;
		self->smokeT = a;
		if (0.04f > a) {
			return;
		}
		self->smokeT = 0.0f;
		FXS_BillDust(self);
	}
	if (self->capsule && FX_InRange(self->t, 13.35f, 13.7f, &f)) {
		float a = self->smokeT + delta;
		self->smokeT = a;
		if (0.04f > a) {
			return;
		}
		self->smokeT = 0.0f;
		self->particles.birthRate = 0;
		cEntity *c = self->capsule;
		CGameModel_GetPos(Obj_Vtbl(c)->GetModel(c), &cp);
		FXS_BillSteam(self, &cp);
	}
	float t = self->t;
	if ((t >= 18.13f && 18.13f >= self->tPrev) || (t >= 18.23f && 18.23f >= self->tPrev)) {
		static const vec3_t offA = { -60.0f, 0.0f, 0.0f }, offB = { 60.0f, 0.0f, -20.0f };
		FXS_BillPuff(self, (t >= 18.13f && 18.13f >= self->tPrev) ? &offA : &offB);
	}
	self->tPrev = self->t;
}

/* ---- EnterMatrix: the rays of the matrix ---- */

/* the paths of the rays of the second part (8, rva 0x430f68): two segments a path, A to C and B to D */
#define FXS_MATRIX_PATHS 0x430f68
#define FXMATRIX_RAYCOLOR 0xc3e1640
#define FXMATRIX_RAYCOLOR_END 0x2c3e1640

/* the inner control points 1..3 moved by random speeds of range + add */
static void FXS_MatrixJitter(vec3_t *P, double range, double add, float delta) {
	vec3_t dir;

	for (int j = 1; j < 4; j++) {
		float r = FxElem_Rand(range, add);
		const vec3_t *d = FX_RandomDir(&dir);
		float dx = d->x * r, dy = d->y * r, dz = d->z * r;
		P[j].y = dy * delta + P[j].y;
		P[j].z = dz * delta + P[j].z;
		P[j].x = P[j].x + dx * delta;
	}
}

/* the life of a live ray down, its end at the full path; its alpha factor (the fade of the last 0.05) */
static float FXS_MatrixFade(cObjElementFXSceneEnterMatrix *self, int k, float life, float delta) {
	if (self->prog[k] >= 1.0f) {
		self->prog[k] = 1.0f;
		if (life > 0.04f) {
			self->life[k] = 0.04f;
		}
	}
	float a = 0.0f;
	float l = self->life[k];
	if (0.05f > l) {
		float q = 1.0f - (l - delta) / 0.05f;
		a = 0.0f > q ? 0.0f : Float_MinSS(1.0f, q);
	}
	return a;
}

/* the trail of ray k along the curve of its control points and the 6 glows at its ends (from idx - 4); second the
 * second part of the scene */
static void FXS_MatrixDraw(cObjElementFXSceneEnterMatrix *self, int k, int idx, float a, int alpha, float delta,
	int second) {
	CGameTrail *tr = &self->trails[k];
	vec3_t *P = self->pts[k];
	vec3_t coefs[GAMETRAIL_MAX_BEZIER], pt, dir;

	int maxT = CGameTrail_GetMaxTrails(tr);
	float dx = P[0].x - P[4].x, dy = P[0].y - P[4].y, dz = P[0].z - P[4].z;
	float len = (float)__builtin_sqrt((double)(dx * dx + dy * dy + dz * dz));
	int n = (int)(len / self->segLen[k]);
	if (n < 5) {
		n = 5;
	} else if (n > maxT) {
		n = maxT;
	}
	CGameTrail_SetSegments(tr, (byte)n);
	CGameTrail_Reset(tr);
	CGameTrail_BezierCoefs(tr, 5, P, coefs);
	CGameTrail_SetUV(tr, self->uvOff[k] + 16.0f, 0.0f);
	CGameTrail_SetUVScale(tr, 1.0f, 1.0f);
	if (self->uvOff[k] > 0.0f) {
		self->uvOff[k] = FxElem_Rand(3.5f, 0.5f) * delta + self->uvOff[k];
		CGameTrail_SetULen(tr, second ? 8.0f : 4.0f, 1.0f);
	} else {
		self->uvOff[k] = self->uvOff[k] - FxElem_Rand(3.5f, 0.5f) * delta;
		CGameTrail_SetULen(tr, second ? (self->t > 30.5f ? 16.0f : 8.0f) : -4.0f, 1.0f);
	}
	if (n - 1 > 0) {
		float last = (float)(n - 2);
		for (int s = 0; s < n - 1; s++) {
			float w;
			CGameTrail_BezierPoint(tr, 5, coefs, &pt, (float)s / last);
			if (s == 0 || s == n - 2) {
				w = self->segLen[k] + self->segLen[k];
			} else {
				if (s < 3 || s > n - 5) {
					float r = second ? FxElem_Rand(140.0f, 145.0f) : FxElem_Rand(3.0f, 3.0f);
					const vec3_t *d = FX_RandomDir(&dir);
					pt.x = pt.x + d->x * r;
					pt.y = pt.y + d->y * r;
					pt.z = pt.z + d->z * r;
				} else {
					float r;
					if (!second) {
						r = FxElem_Rand(4.0f, 4.0f);
					} else if (self->t > 30.5f) {
						r = FxElem_Rand(300.0f, 645.0f);
					} else {
						r = FxElem_Rand(150.0f, 195.0f);
					}
					const vec3_t *d = FX_RandomDir(&dir);
					pt.x = d->x * r + pt.x;
					pt.y = d->y * r + pt.y;
					pt.z = d->z * r + pt.z;
				}
				w = self->segLen[k] * (second ? FxElem_Rand(0.79999995f, 1.2f) : FxElem_Rand(0.5f, 0.75f));
			}
			uint32_t c = (s <= 1 || s == n - 2) ? FXMATRIX_RAYCOLOR_END : FXMATRIX_RAYCOLOR;
			if (second && self->t > 30.0f) {
				uint32_t ca = (c >> 24) + 0x10;
				if (ca > 0xff) {
					ca = 0xff;
				}
				c = (c & 0xffffff) | ca << 24;
			}
			CGameTrail_Add(tr, pt, w, c);
		}
	}
	CGameParticles *sp = &self->particles;
	float w2 = self->segLen[k] + self->segLen[k];
	uint32_t c1 = (uint32_t)alpha << 24 | 0xf05cff;
	float s1 = w2 * 1.5f;
	float s2 = w2 * 2.5f;
	uint32_t c2 = (0x800000ffu - ((uint32_t)(int)(a * -127.0f) << 24)) | 0xf05cff;
	float s3 = w2 * 5.0f;
	CGameParticles_SetParticle(sp, idx - 4, P[0].x, P[0].y, P[0].z, s1, c1, 1, 0.0f);
	CGameParticles_SetParticle(sp, idx - 3, P[0].x, P[0].y, P[0].z, s2, c2, 2, 0.0f);
	CGameParticles_SetParticle(sp, idx - 2, P[0].x, P[0].y, P[0].z, s3, c2, 5, 0.0f);
	CGameParticles_SetParticle(sp, idx - 1, P[4].x, P[4].y, P[4].z, s1, c1, 1, 0.0f);
	CGameParticles_SetParticle(sp, idx, P[4].x, P[4].y, P[4].z, s2, c2, 2, 0.0f);
	CGameParticles_SetParticle(sp, idx + 1, P[4].x, P[4].y, P[4].z, s3, c2, 5, 0.0f);
}

/* the first part (15 <= t <= 20): rays from the matrix to points near the scene */
static void FXS_MatrixRays1(cObjElementFXSceneEnterMatrix *self, float delta) {
	vec3_t tmp, ang, in, B, T, dir;

	cEntity *m = World_FindEntity("EnterMatrixMatrix");
	if (m && m->active && !Entity_Removed(m)) {
		self->center = *Obj_Vtbl(m)->GetViewPos(m, &tmp);
	}
	int idx = 4;
	for (int k = 0; k < FXMATRIX_TRAILS; k++, idx += 6) {
		vec3_t *P = self->pts[k];
		if (self->life[k] > 0.0f) {
			float life = self->life[k] - delta;
			self->life[k] = life;
			if (!(life > 0.0f)) {
				CGameTrail_Clear(&self->trails[k]);
				CGameParticles_KillRange(&self->particles, idx - 4, idx + 2);
				continue;
			}
			float a = FXS_MatrixFade(self, k, life, delta);
			FXS_MatrixJitter(P, 130.0f, 20.0f, delta);
			int alpha = (int)(a * 255.0f);
			CGameTrail_SetTransp(&self->trails[k], (byte)alpha);
			FXS_MatrixDraw(self, k, idx, a, alpha, delta, 0);
			continue;
		}
		float w = self->wait[k] - delta;
		self->wait[k] = w;
		if (0.0f < w) {
			continue;
		}
		/* a new ray from the matrix to a point near the scene, ended at the ground */
		self->life[k] = FxElem_Rand(0.19f, 0.16f);
		self->wait[k] = FxElem_Rand(0.45000002f, 0.15f);
		self->segLen[k] = FxElem_Rand(6.0f, 12.0f);
		self->uvOff[k] = FxElem_Rand(8.0f, -4.0f);
		float ry = FxElem_Rand(85.0f, -25.0f);
		B.x = self->center.x + 0.0f;
		B.y = self->center.y + ry;
		B.z = self->center.z + 0.0f;
		/* a random direction that is not used */
		FxElem_Rand(130.0f, 485.0f);
		float a1 = FxElem_Rand(1.6f, -0.8f);
		float a2 = FxElem_Rand(0.8f, -0.4f);
		ang.x = FxElem_Rand(1.6f, -0.8f);
		ang.y = a2;
		ang.z = a1;
		in.x = 0.0f;
		in.y = 0.0f;
		in.z = 1.0f;
		Vec3_Rotate(&tmp, &in, &ang);
		const vec3_t *d;
		float r;
		if (crt_rand() & 1) {
			r = FxElem_Rand(12.0f, 0.0);
			d = FX_RandomDir(&dir);
			T.x = d->x * r + 72629.7f;
		} else {
			r = FxElem_Rand(12.0f, 0.0);
			d = FX_RandomDir(&dir);
			T.x = d->x * r + 73177.5f;
		}
		T.z = d->z * r - 9327.0f;
		T.y = d->y * r + 37218.5f;
		Engine_TestLine(&B, &T, &T, NULL);
		P[0] = B;
		P[4] = T;
		float sx = (T.x - P[0].x) * 0.25f, sy = (T.y - P[0].y) * 0.25f, sz = (T.z - P[0].z) * 0.25f;
		for (int j = 1; j < 4; j++) {
			float fj = (float)j;
			P[j].x = P[0].x + sx * fj;
			P[j].z = sz * fj + P[0].z;
			P[j].y = P[0].y + sy * fj;
			float rr = FxElem_Rand(15.0f, 15.0f);
			const vec3_t *dd = FX_RandomDir(&dir);
			P[j].x = rr * dd->x + P[j].x;
			P[j].y = dd->y * rr + P[j].y;
			P[j].z = dd->z * rr + P[j].z;
		}
	}
}

/* the control points of ray k of the second part along path kind from its progress */
static void FXS_MatrixPath(cObjElementFXSceneEnterMatrix *self, int k) {
	vec3_t *P = self->pts[k];
	vec3_t N, v, tmp, dir;

	const vec3_t *tab = GAME_ARRAY(const vec3_t, cObjElementFXSceneEnterMatrix_Update_430f68) + self->kind[k] * 4;
	float e1x = tab[2].x - tab[0].x, e2x = tab[3].x - tab[1].x;
	float e1y = tab[2].y - tab[0].y, e1z = tab[2].z - tab[0].z;
	float e2y = tab[3].y - tab[1].y, e2z = tab[3].z - tab[1].z;
	float u = self->prog[k] + FxElem_Rand(0.004f, -0.002f);
	vec3_t S = { e1x * u + tab[0].x, tab[0].y + e1y * u, tab[0].z + e1z * u };
	float u2 = (self->spread[k] + self->prog[k]) + FxElem_Rand(0.004f, -0.002f);
	vec3_t F = { e2x * u2 + tab[1].x, tab[1].y + e2y * u2, tab[1].z + e2z * u2 };
	float dx = F.x - S.x, dy = F.y - S.y, dz = F.z - S.z;
	vec3_t M = { S.x + dx * 0.5f, S.y + dy * 0.5f, S.z + dz * 0.5f };
	if (self->t > 30.5f) {
		/* the end: the rays bend far up */
		float b1 = FxElem_Rand(0.4f, -0.2f);
		float b2 = (float)(FXS_Rnd() + 0.2f);
		v.x = FxElem_Rand(0.4f, -0.2f);
		v.y = b2;
		v.z = b1;
		Vec3_NormalizeTo(&N, &v);
		P[0] = S;
		float r = FxElem_Rand(200.0f, 175.0f);
		const vec3_t *d = FX_RandomDir(&dir);
		float jx = r * d->x * 5.0f, jy = d->y * r * 5.0f, jz = d->z * r * 5.0f;
		float w = FxElem_Rand(2700.0f, 7150.0f);
		float nx = N.x * w, ny = N.y * w, nz = N.z * w;
		float s = FxElem_Rand(0.10000001f, 0.2f);
		P[1].x = jx + (nx + (S.x + dx * s));
		P[1].y = jy + (ny + (S.y + dy * s));
		P[1].z = jz + (nz + (S.z + dz * s));
		r = FxElem_Rand(190.0f, 245.0f);
		d = FX_RandomDir(&dir);
		jx = r * d->x * 5.0f;
		jy = d->y * r * 5.0f;
		jz = d->z * r * 5.0f;
		w = FxElem_Rand(3500.0f, 13250.0f);
		nx = N.x * w;
		ny = N.y * w;
		nz = N.z * w;
		s = FxElem_Rand(0.100000024f, 0.45f);
		P[2].x = jx + (nx + (S.x + dx * s));
		P[2].y = jy + (ny + (S.y + dy * s));
		P[2].z = jz + (nz + (S.z + dz * s));
		r = FxElem_Rand(200.0f, 175.0f);
		d = FX_RandomDir(&dir);
		jx = r * d->x * 5.0f;
		jy = d->y * r * 5.0f;
		jz = d->z * r * 5.0f;
		w = FxElem_Rand(2700.0f, 7150.0f);
		nx = w * N.x;
		ny = w * N.y;
		nz = w * N.z;
		s = FxElem_Rand(0.100000024f, 0.7f);
		P[3].x = jx + (nx + (S.x + s * dx));
		P[3].y = jy + (ny + (S.y + s * dy));
		P[3].z = jz + (nz + (S.z + s * dz));
	} else {
		/* bent sideways: the first segment of the path turned by -90 degrees around y */
		vec3_t L = { tab[0].x - tab[1].x, tab[0].y - tab[1].y, tab[0].z - tab[1].z };
		const vec3_t ang = { 0.0f, -1.5707964f, 0.0f };
		Vec3_NormalizeTo(&N, Vec3_Rotate(&tmp, &L, &ang));
		P[0] = S;
		float r = FxElem_Rand(200.0f, 175.0f);
		const vec3_t *d = FX_RandomDir(&dir);
		float jx = r * d->x, jy = d->y * r, jz = d->z * r;
		float w = FxElem_Rand(300.0f, 2150.0f);
		P[1].x = (w * N.x + S.x) + jx;
		P[1].y = (w * N.y + S.y) + jy;
		P[1].z = (w * N.z + S.z) + jz;
		r = FxElem_Rand(190.0f, 245.0f);
		d = FX_RandomDir(&dir);
		jx = r * d->x;
		jy = d->y * r;
		jz = d->z * r;
		w = FxElem_Rand(500.0f, 2250.0f);
		P[2].x = (w * N.x + M.x) + jx;
		P[2].y = (w * N.y + M.y) + jy;
		P[2].z = (w * N.z + M.z) + jz;
		r = FxElem_Rand(200.0f, 175.0f);
		d = FX_RandomDir(&dir);
		jx = r * d->x;
		jy = d->y * r;
		jz = d->z * r;
		w = FxElem_Rand(300.0f, 2150.0f);
		P[3].x = (w * N.x + F.x) + jx;
		P[3].y = (w * N.y + F.y) + jy;
		P[3].z = (w * N.z + F.z) + jz;
	}
	P[4] = F;
}

/* the second part (20 <= t <= 33.2): rays along the paths of the table */
static void FXS_MatrixRays2(cObjElementFXSceneEnterMatrix *self, float delta) {
	if (20.0f > self->tPrev) {
		for (int k = 0; k < FXMATRIX_TRAILS; k++) {
			CGameTrail_Clear(&self->trails[k]);
			self->life[k] = -1.0f;
			self->wait[k] = FxElem_Rand(0.49999997f, 0.4f);
		}
	}
	int idx = 4;
	for (int k = 0; k < FXMATRIX_TRAILS; k++, idx += 6) {
		if (self->life[k] > 0.0f) {
			float life = self->life[k] - delta;
			self->life[k] = life;
			if (!(life > 0.0f)) {
				CGameTrail_Clear(&self->trails[k]);
				CGameParticles_KillRange(&self->particles, idx - 4, idx + 2);
				continue;
			}
			float a = FXS_MatrixFade(self, k, life, delta);
			int alpha = (int)(a * 255.0f);
			CGameTrail_SetTransp(&self->trails[k], (byte)alpha);
			float p = self->prog[k];
			if (1.0f > p) {
				self->prog[k] = self->speed[k] * delta + p;
			}
			FXS_MatrixPath(self, k);
			if (self->t > 30.5f) {
				FXS_MatrixJitter(self->pts[k], 450.0f, 150.0f, delta);
			}
			FXS_MatrixDraw(self, k, idx, a, alpha, delta, 1);
			continue;
		}
		float w = self->wait[k] - delta;
		self->wait[k] = w;
		if (0.0f < w) {
			continue;
		}
		/* a new ray on a path other than the last one */
		self->life[k] = self->t > 30.3f ? FxElem_Rand(0.08f, 0.16f) : FxElem_Rand(0.32f, 0.18f);
		self->wait[k] = FxElem_Rand(0.49999997f, 0.4f);
		self->segLen[k] = self->t > 30.3f ? FxElem_Rand(620.0f, 980.0f) : FxElem_Rand(260.0f, 360.0f);
		self->uvOff[k] = FxElem_Rand(8.0f, -4.0f);
		do {
			self->kind[k] = self->t > 30.3f ? crt_rand() % 4 + 4 : crt_rand() % 4;
		} while (self->lastKind == self->kind[k]);
		float t = self->t;
		self->prog[k] = (t > 26.3f && 30.6f > t) ? FxElem_Rand(0.19999999f, 0.68f) : FxElem_Rand(0.8f, 0.08f);
		self->spread[k] = FxElem_Rand(0.08f, -0.04f);
		self->speed[k] = (crt_rand() & 1) ? FxElem_Rand(0.04f, 0.1f) : FxElem_Rand(0.34999996f, 0.3f);
	}
}

void cObjElementFXSceneEnterMatrix_Update(cObjElementFXSceneEnterMatrix *self, cDebris *d, float dt) {
	float f;

	(void)dt;
	if (!FXS_Alive(&self->ent)) {
		d->life = 0.0f;
		return;
	}
	float delta = FXS_Delta();
	self->t = self->t + delta;
	if (FX_InRange(self->t, 15.0f, 20.0f, &f)) {
		FXS_MatrixRays1(self, delta);
	} else if (FX_InRange(self->t, 20.0f, 33.2f, &f)) {
		FXS_MatrixRays2(self, delta);
	}
	if (!FX_InRange(self->t, 30.3f, 33.0f, &f)) {
		CGameLensFlare_SetOn(&self->flare, 0);
		self->tPrev = self->t;
		return;
	}
	const vec3_t fp = { 72655.1f, 73667.0f, -10498.7f };
	CGameLensFlare_SetPos(&self->flare, fp);
	CGameLensFlare_SetOn(&self->flare, 1);
	float v;
	if (0.2f > f) {
		float g = f * 5.0f;
		v = g * g * (2.0f - g) * 0.5f;
	} else {
		float g = (f - 0.2f) * 1.25f;
		v = 0.5f - (3.0f - (g + g)) * (g * g) * 0.5f;
	}
	CGameLensFlare_SetIntensity(&self->flare, v);
	CGameLensFlare_SetScale(&self->flare, 1.0f);
	CGameLensFlare_SetType(&self->flare, 2);
	self->tPrev = self->t;
}

void cObjElementFXSceneDecontamination_SetNames(cObjElementFXSceneDecontamination *self,
	const char *target, const char *action) {
	Str_Copy(self->targetName, target, sizeof(self->targetName));
	Str_Copy(self->action, action, sizeof(self->action));
}

void cObjElementFXSceneDecontamination_SetFC(cObjElementFXSceneDecontamination *self, float v) {
	self->height = v;
}
