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
#include "scrap/timer.h"
#include "scrap/weapons.h"
#include "scrap/world.h"

/* the color ramps (9 colors) of the particles */
#define FXS_RAMP(rva, i) ((uint32_t *)Game_Ptr(rva) + (i) * 9)
#define FXS_RAMP_HUMANCD 0x430c48	/* [2] */
#define FXS_RAMP_YAKUZZI 0x430dd8	/* [2] */

typedef void(__thiscall *FXSUpdate_t)(void *self, cDebris *d, float dt);

static inline float FXS_Delta(void) {
	return cTimer_GetDelta(&g_pWorld->timer);
}

/* the entity let go once it is deleted or off; whether there is one that is on */
static int FXS_Alive(cEntity **ent) {
	cEntity *e = *ent;

	if (e && (Entity_Removed(e) || !e->flag_4c)) {
		*ent = NULL;
	}
	return *ent && (*ent)->flag_4c;
}

/* the model of the debris to the position and angles of the model of e (into pos and rot) */
static void FXS_Follow(cEntity *e, cDebris *d, float sortZ, vec3_t *pos, vec3_t *rot) {
	CGameModel *m = FX_Vtbl(e)->GetModel(e);

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
	int i = CGameModel_FindNodoR(m, node);

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
	CHECK(CGameModel_IsLoaded(FX_Vtbl(e)->GetModel(e)));
}

/* a new debris of the system of file with that life, dropped */
static cDebris *FXS_Drop(const char *file, float life) {
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, file);
	cDebris *d = cDebrisSys_NewElement(sys);

	Debris_Life(d) = life;
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
		cObjElementFXSceneDecontamination_SetTarget(el, "DecontaminationDtritus", "Decon");
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
		cObjElementFXSceneDecontamination_SetTarget(el, "BillArrives_CHuman", "Descontamination");
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

void __thiscall cObjElementFXSceneBeatPickus_SetEntity(cObjElementFXSceneBeatPickus *self, cEntity *e) {
	FXS_SetEntity(&self->ent, e);
}

void __thiscall cObjElementFXSceneBillArrives_A_SetEntity(cObjElementFXSceneBillArrives_A *self, cEntity *e) {
	FXS_SetEntity(&self->ent, e);
}

void __thiscall cObjElementFXSceneBishops_SetEntity(cObjElementFXSceneBishops *self, cEntity *e) {
	FXS_SetEntity(&self->ent, e);
}

void __thiscall cObjElementFXSceneDecontamination_SetEntity(cObjElementFXSceneDecontamination *self, cEntity *e) {
	FXS_SetEntity(&self->ent, e);
}

void __thiscall cObjElementFXSceneEnterMatrix_SetEntity(cObjElementFXSceneEnterMatrix *self, cEntity *e) {
	FXS_SetEntity(&self->ent, e);
}

void __thiscall cObjElementFXSceneHumanCD_SetEntity(cObjElementFXSceneHumanCD *self, cEntity *e) {
	FXS_SetEntity(&self->ent, e);
	CHECK(g_MentalMainFX);
	g_MentalMainFX->last = -2;
	g_MentalMainFX->scale = 1.0f;
}

void __thiscall cObjElementFXSceneTheEndB_SetEntity(cObjElementFXSceneTheEndB *self, cEntity *e) {
	FXS_SetEntity(&self->ent, e);
}

void __thiscall cObjElementFXSceneYakuzzi_SetEntity(cObjElementFXSceneYakuzzi *self, cEntity *e) {
	FXS_SetEntity(&self->ent, e);
}

/* Attach */

void __thiscall cObjElementFXSceneBeatPickus_Attach(cObjElementFXSceneBeatPickus *self, cDebris *d) {
	CHECK(CGameModel_IsLoaded(Debris_Model(d)));
	FXS_AttachNode(&self->parts184, d, "Dummy01");
	FXS_AttachNode(&self->partsD0, d, "Dummy02");
	FXS_AttachNode(&self->parts1c, d, "Dummy03");
	FXS_DummyPos("DM_Scene_Beating", &self->dummy);
}

void __thiscall cObjElementFXSceneBillArrives_A_Attach(cObjElementFXSceneBillArrives_A *self, cDebris *d) {
	CHECK(CGameModel_IsLoaded(Debris_Model(d)));
	FXS_AttachNode(&self->particles, d, "Dummy01");
	FXS_DummyPos("DM_Effects_BillArrives_A_01", &self->dummy1);
	FXS_DummyPos("DM_Effects_BillArrives_A_02", &self->dummy2);
}

void __thiscall cObjElementFXSceneBishops_Attach(cObjElementFXSceneBishops *self, cDebris *d) {
	CHECK(CGameModel_IsLoaded(Debris_Model(d)));
	FXS_AttachNode(&self->dataCodec, d, "Dummy01");
	CHECK(self->dataCodec.SistPart);
	CHECK(CGameModel_IsLoaded(Debris_Model(d)));
	FXS_AttachNode(&self->glow, d, "Dummy02");
	CHECK(self->glow.SistPart);
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

void __thiscall cObjElementFXSceneDecontamination_Attach(cObjElementFXSceneDecontamination *self, cDebris *d) {
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

void __thiscall cObjElementFXSceneEnterMatrix_Attach(cObjElementFXSceneEnterMatrix *self, cDebris *d) {
	CHECK(CGameModel_IsLoaded(Debris_Model(d)));
	FXS_AttachNode(&self->particles, d, "Dummy01");
	for (int i = 0; i < FXMATRIX_TRAILS; i++) {
		CGameTrail *t = &self->trails[i];
		CGameTrail_SetMaxTrails(t, 0x10);
		CGameTrail_SetSmooth(t, 1);
		CGameTrail_SetTexture(t, (~crt_rand() & 1) | 8);
		CGameTrail_SetPremul(t, 1);
		self->f18c[i] = -1.0f;
		self->f1cc[i] = FxElem_Rand(0.45f, 0.15f);
	}
	self->i78c = -1;
}

void __thiscall cObjElementFXSceneHumanCD_Attach(cObjElementFXSceneHumanCD *self, cDebris *d) {
	CHECK(CGameModel_IsLoaded(Debris_Model(d)));
	FXS_AttachNode(&self->parts10, d, "Dummy01");
	CHECK(CGameModel_IsLoaded(Debris_Model(d)));
	FXS_AttachNode(&self->sparks, d, "Dummy03");
	FXS_DummyPos("DM_Element_MentalControllerMainFX", &self->dummy);
	self->sparkT = 0.0f;
}

void __thiscall cObjElementFXSceneTheEndB_Attach(cObjElementFXSceneTheEndB *self, cDebris *d) {
	CHECK(CGameModel_IsLoaded(Debris_Model(d)));
	FXS_AttachNode(&self->particles, d, "Dummy01");
}

void __thiscall cObjElementFXSceneYakuzzi_Attach(cObjElementFXSceneYakuzzi *self, cDebris *d) {
	CHECK(CGameModel_IsLoaded(Debris_Model(d)));
	FXS_AttachNode(&self->particles, d, "Dummy01");
	FXS_DummyPos("DM_Element_TempleYakuzzi_00", &self->dummy);
}

/* Reset */

void __thiscall cObjElementFXSceneBeatPickus_Reset(cObjElementFXSceneBeatPickus *self, int arg) {
	(void)arg;
	self->ent = NULL;
	self->t = 0.0f;
	self->tPrev = 0.0f;
	self->fcc = 0.0f;
	self->f180 = 0.0f;
	self->f234 = 0.0f;
	self->f238 = 0.0f;
}

void __thiscall cObjElementFXSceneBillArrives_A_Reset(cObjElementFXSceneBillArrives_A *self, int arg) {
	(void)arg;
	self->ent = NULL;
	self->f10 = 0.0f;
	self->t = 0.0f;
	self->tPrev = 0.0f;
	self->f18 = 0.0f;
	self->fcc = 0.0f;
}

void __thiscall cObjElementFXSceneBishops_Reset(cObjElementFXSceneBishops *self, int arg) {
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

void __thiscall cObjElementFXSceneDecontamination_Reset(cObjElementFXSceneDecontamination *self, int arg) {
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

void __thiscall cObjElementFXSceneEnterMatrix_Reset(cObjElementFXSceneEnterMatrix *self, int arg) {
	(void)arg;
	self->ent = NULL;
	self->t = 0.0f;
	self->tPrev = 0.0f;
	for (int i = 0; i < FXMATRIX_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
		self->f18c[i] = -1.0f;
		self->f1cc[i] = FxElem_Rand(0.45f, 0.15f);
	}
}

void __thiscall cObjElementFXSceneHumanCD_Reset(cObjElementFXSceneHumanCD *self, int arg) {
	(void)arg;
	self->ent = NULL;
	self->t = 0.0f;
	self->tPrev = 0.0f;
	self->sparkT = 0.0f;
	CGameParticles_KillAll(&self->parts10);
}

void __thiscall cObjElementFXSceneTheEndB_Reset(cObjElementFXSceneTheEndB *self, int arg) {
	(void)arg;
	self->ent = NULL;
	self->moto = NULL;
	self->t = 0.0f;
	self->tPrev = 0.0f;
}

void __thiscall cObjElementFXSceneYakuzzi_Reset(cObjElementFXSceneYakuzzi *self, int arg) {
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
	CGameParticles_SetColorRamp(sp, FXS_RAMP(FXS_RAMP_HUMANCD, crt_rand() & 1));
	CGameParticles_EmitDt(sp, 1, 1, FxElem_Rand(0.1f, 0.0));
}

void __thiscall cObjElementFXSceneHumanCD_Update(cObjElementFXSceneHumanCD *self, cDebris *d, float dt) {
	vec3_t pos, rot;
	float f;

	(void)dt;
	if (!FXS_Alive(&self->ent)) {
		Debris_Life(d) = 0.0f;
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

void __thiscall cObjElementFXSceneTheEndB_Update(cObjElementFXSceneTheEndB *self, cDebris *d, float dt) {
	vec3_t pos, rot;
	float f;

	(void)dt;
	if (!FXS_Alive(&self->ent)) {
		Debris_Life(d) = 0.0f;
		return;
	}
	if (!self->moto) {
		cEntity *m = World_FindEntity("TheEndBMoto");
		self->moto = m;
		if (m) {
			CHECK(Entity_IsType(m, TYEN_WITH_LIFE));
		}
	}
	self->t = self->t + FXS_Delta();
	FXS_Follow(self->ent, d, -107.0f, &pos, &rot);
	if (!self->moto) {
		self->tPrev = self->t;
		return;
	}
	cEmbeddedFXMoto *efx = (cEmbeddedFXMoto *)Entity_EmbeddedFX(self->moto);
	CHECK(efx);
	/* the engine of the moto by the time of the scene */
	if (FX_InRange(self->t, 0.0f, 8.0f, &f)) {
		cEmbeddedFXMoto_SetF7d4(efx, f * 0.5f, 0.0f);
	} else if (FX_InRange(self->t, 8.0f, 11.0f, &f)) {
		cEmbeddedFXMoto_SetF7d4(efx, f * 0.5f + 0.5f, 0.0f);
	} else if (FX_InRange(self->t, 15.0f, 16.0f, &f)) {
		cEmbeddedFXMoto_SetF7d4(efx, 1.0f - f * 0.8f, 0.0f);
	} else if (FX_InRange(self->t, 18.0f, 22.0f, &f)) {
		cEmbeddedFXMoto_SetF7d4(efx, f * 0.5f, 0.0f);
	} else if (FX_InRange(self->t, 22.0f, 23.0f, &f)) {
		cEmbeddedFXMoto_SetF7d4(efx, f * 0.5f + 0.5f, 0.0f);
	} else if (FX_InRange(self->t, 23.0f, 25.0f, &f)) {
		cEmbeddedFXMoto_SetF7d4(efx, 1.0f, 0.2f - f * 0.2f);
	} else if (FX_InRange(self->t, 25.0f, 26.0f, &f)) {
		cEmbeddedFXMoto_SetF7d4(efx, 1.0f, 0.0f);
	}
	if (!FX_InRange(self->t, 23.0f, 25.0f, &f)) {
		CGameLensFlare_SetOn(&self->flare, 0);
		self->tPrev = self->t;
		return;
	}
	vec3_t tmp;
	CGameLensFlare_SetPos(&self->flare, *cEmbeddedFXMoto_EnginePos(efx, &tmp));
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
	CGameLensFlare_SetF18(&self->flare, 1.0f);
	CGameLensFlare_SetI1c(&self->flare, 3);
	self->tPrev = self->t;
}

void __thiscall cObjElementFXSceneYakuzzi_Update(cObjElementFXSceneYakuzzi *self, cDebris *d, float dt) {
	CGameParticles *sp = &self->particles;
	vec3_t pos, rot, dir;

	(void)dt;
	if (!FXS_Alive(&self->ent)) {
		Debris_Life(d) = 0.0f;
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
		CGameParticles_SetColorRamp(sp, FXS_RAMP(FXS_RAMP_YAKUZZI, crt_rand() & 1));
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
	CGameParticles_SetColorRamp(sp, (uint32_t *)Game_Ptr(FXS_RAMP_BISHOPS_GLOW));
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
	CGameParticles_SetColorRamp(sp, FXS_RAMP(FXS_RAMP_BISHOPS_CODEC, crt_rand() & 1));
	CGameParticles_Apply(sp);
	CGameParticles_EmitN(sp, 1, 0);
}

void __thiscall cObjElementFXSceneBishops_Update(cObjElementFXSceneBishops *self, cDebris *d, float dt) {
	vec3_t pos, rot;
	vec3_t verts[FXBISHOPS_VERTS];
	float q, f;
	int alpha;
	float k;

	(void)dt;
	if (!FXS_Alive(&self->ent)) {
		Debris_Life(d) = 0.0f;
		return;
	}
	FXS_Follow(self->ent, d, -101.0f, &pos, &rot);
	if (!self->detritus) {
		cEntity *e = World_FindEntity("BishopsDtritus");
		self->detritus = e;
		if (e) {
			CGameModel *dm = FX_Vtbl(e)->GetModel(e);
			CHECK(CGameModel_IsLoaded(dm));
			self->node = CGameModel_FindNodoR(dm, "R_Detritus_Tronco");
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
	CGameModel *dm = FX_Vtbl(det)->GetModel(det);
	CHECK(CGameModel_IsLoaded(dm));
	float radius = FX_Vtbl(det)->GetRadius(det);
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

void __thiscall cObjElementFXSceneDecontamination_Update(cObjElementFXSceneDecontamination *self, cDebris *d,
	float dt) {
	vec3_t pos, rot;

	(void)dt;
	if (!FXS_Alive(&self->ent)) {
		Debris_Life(d) = 0.0f;
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
		CHECK(CGameModel_IsLoaded(FX_Vtbl(e)->GetModel(e)));
	}
	cEntity *te = self->target;
	CHECK(WithLife_Vtbl(te)->GetActCtrl(te));
	byte active = cActCtrl_IsActive(WithLife_Vtbl(te)->GetActCtrl(te), self->action);
	float actT = *cActCtrl_GetLayer(WithLife_Vtbl(te)->GetActCtrl(te), 0);
	CHECK(CGameModel_IsLoaded(FX_Vtbl(self->ent)->GetModel(self->ent)));
	CGameModel *target = FX_Vtbl(te)->GetModel(te);
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
