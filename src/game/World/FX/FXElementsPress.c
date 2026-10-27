/*
 * FXElementsPress.cpp of the game: the mental controllers of the Press building and the main controller that feeds
 * them, see include/scrap/fxelements.h.
 */
#include <string.h>

#include "scrap/fxelements.h"
#include "scrap/mathlib.h"
#include "scrap/scrap.h"

#define g_RandScale GAME_DATA(double, 0x3a3d60)	/* 1/32767 */
#define PRESS_NAME_SKIP 11			/* "DM_Element_" before the name of the element in the dummy */
#define CGAMETRAIL_CTOR 0x21ddb0
#define CGAMETRAIL_DTOR 0x21de40

/* ---- construction ---- */

cFXElementsMentalController *__thiscall cFXElementsMentalController_ctor(cFXElementsMentalController *self,
	const char *name, const vec3_t *pos, const vec3_t *rot) {
	cMapElement_ctor(&self->base, name, pos, rot);
	self->base.vftable = Game_Ptr(CFXELEMENTSMENTALCONTROLLER_VFTABLE);
	return self;
}

cFXElementsMentalControllerDamageFX *__thiscall cFXElementsMentalControllerDamageFX_ctor(
	cFXElementsMentalControllerDamageFX *self, const char *name, const vec3_t *pos, const vec3_t *rot) {
	cMapElement_ctor(&self->base.base, name, pos, rot);
	self->base.base.vftable = Game_Ptr(CFXELEMENTSMENTALCONTROLLERDAMAGEFX_VFTABLE);
	CGameParticles_ctor(&self->smoke);
	CGameParticles_ctor(&self->flash);
	return self;
}

cFXElementsMentalControllerMainFX *__thiscall cFXElementsMentalControllerMainFX_ctor(
	cFXElementsMentalControllerMainFX *self, const char *name, const vec3_t *pos, const vec3_t *rot) {
	cMapElement_ctor(&self->base, name, pos, rot);
	self->base.vftable = Game_Ptr(CFXELEMENTSMENTALCONTROLLERMAINFX_VFTABLE);
	crt_eh_vector_ctor(self->tubeTrails, sizeof(CGameTrail), FXMENTALMAINTRAILS, Game_Ptr(CGAMETRAIL_CTOR),
		Game_Ptr(CGAMETRAIL_DTOR));
	CGameParticles_ctor(&self->dataFluid);
	crt_eh_vector_ctor(self->glowTrails, sizeof(CGameTrail), FXMENTALMAINTRAILS, Game_Ptr(CGAMETRAIL_CTOR),
		Game_Ptr(CGAMETRAIL_DTOR));
	CGameParticles_ctor(&self->tubeGlow);
	crt_eh_vector_ctor(self->topTrails, sizeof(CGameTrail), 2, Game_Ptr(CGAMETRAIL_CTOR), Game_Ptr(CGAMETRAIL_DTOR));
	for (int i = 0; i < FXMENTALCONTROLLERMAINCOUNT; i++) {
		self->controllers[i] = NULL;
	}
	self->scale = 1.0f;
	self->last = -2;
	return self;
}

void __fastcall cFXElementsMentalController_dtor(cFXElementsMentalController *self) {
	self->base.vftable = Game_Ptr(CFXELEMENTSMENTALCONTROLLER_VFTABLE);
	cMapElement_dtor(&self->base);
}

void __fastcall cFXElementsMentalControllerDamageFX_dtor(cFXElementsMentalControllerDamageFX *self) {
	self->base.base.vftable = Game_Ptr(CFXELEMENTSMENTALCONTROLLERDAMAGEFX_VFTABLE);
	CGameParticles_Release(&self->smoke);
	CGameParticles_Release(&self->flash);
	CGameParticles_Release(&self->flash);
	CGameParticles_Release(&self->smoke);
	cMapElement_dtor(&self->base.base);
}

void __fastcall cFXElementsMentalControllerMainFX_dtor(cFXElementsMentalControllerMainFX *self) {
	self->base.vftable = Game_Ptr(CFXELEMENTSMENTALCONTROLLERMAINFX_VFTABLE);
	CGameParticles_Release(&self->dataFluid);
	crt_eh_vector_dtor(self->topTrails, sizeof(CGameTrail), 2, Game_Ptr(CGAMETRAIL_DTOR));
	CGameParticles_Release(&self->tubeGlow);
	crt_eh_vector_dtor(self->glowTrails, sizeof(CGameTrail), FXMENTALMAINTRAILS, Game_Ptr(CGAMETRAIL_DTOR));
	CGameParticles_Release(&self->dataFluid);
	crt_eh_vector_dtor(self->tubeTrails, sizeof(CGameTrail), FXMENTALMAINTRAILS, Game_Ptr(CGAMETRAIL_DTOR));
	cMapElement_dtor(&self->base);
}

cFXElementsMentalController *__thiscall cFXElementsMentalController_Delete(cFXElementsMentalController *self,
	byte flags) {
	cFXElementsMentalController_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

cFXElementsMentalControllerDamageFX *__thiscall cFXElementsMentalControllerDamageFX_Delete(
	cFXElementsMentalControllerDamageFX *self, byte flags) {
	cFXElementsMentalControllerDamageFX_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

cFXElementsMentalControllerMainFX *__thiscall cFXElementsMentalControllerMainFX_Delete(
	cFXElementsMentalControllerMainFX *self, byte flags) {
	cFXElementsMentalControllerMainFX_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

void __fastcall cFXElementsMentalControllerDamageFX_ReleaseParticles(cFXElementsMentalControllerDamageFX *self) {
	CGameParticles_Release(&self->smoke);
	CGameParticles_Release(&self->flash);
}

void __fastcall cFXElementsMentalControllerMainFX_ReleaseFluid(cFXElementsMentalControllerMainFX *self) {
	CGameParticles_Release(&self->dataFluid);
}

void __fastcall cFXElementsMentalControllerMainFX_Clear(cFXElementsMentalControllerMainFX *self) {
	for (int i = 0; i < FXMENTALMAINTRAILS; i++) {
		CGameTrail_Clear(&self->tubeTrails[i]);
		CGameTrail_Clear(&self->glowTrails[i]);
	}
	for (int i = 0; i < 2; i++) {
		CGameTrail_Clear(&self->topTrails[i]);
	}
	CGameParticles_KillAll(&self->dataFluid);
	CGameParticles_KillAll(&self->tubeGlow);
}

/* ---- cFXElementsMentalController ---- */

/* the model checked, its position and the node DC_Root; shared start of the three Inits */
static void MentalController_InitBase(cMapElement *e, int *dcRoot, vec3_t *pos) {
	vec3_t tmp;

	cMapElement_Init(e);
	CHECK(CGameModel_IsLoaded(&e->Mod));
	*pos = *CGameModel_GetPos(&e->Mod, &tmp);
	*dcRoot = CGameModel_FindNodoR(&e->Mod, "DC_Root");
	CHECK(*dcRoot != -1);
}

byte __fastcall cFXElementsMentalController_Init(cFXElementsMentalController *self) {
	MentalController_InitBase(&self->base, &self->dcRoot, &self->pos);
	return 1;
}

void __thiscall cFXElementsMentalController_Update(cFXElementsMentalController *self, float dt) {
	cMapElement_Update(&self->base, dt);
	CHECK(CGameModel_IsLoaded(&self->base.Mod));
	if (CGameModel_IsVisible(&self->base.Mod)) {
		vec3_t cam = g_pWorld->camPos[g_pWorld->curCam];
		vec3_t tmp;
		const vec3_t *p = CGameModel_GetPos(&self->base.Mod, &tmp);
		float dy = p->y - cam.y;
		float dx = p->x - cam.x;
		float dz = p->z - cam.z;
		if (!(dy * dy + dx * dx + dz * dz > g_MentalControllerMaxDist * g_MentalControllerMaxDist)) {
			if (!self->nearCam) {
				self->nearCam = 1;
			}
			return;
		}
	}
	if (self->nearCam) {
		self->nearCam = 0;
	}
}

/* ---- cFXElementsMentalControllerDamageFX ---- */

/* the matrix of node into pos and its angles into rot */
static void Press_NodePlace(CGameModel *model, int node, vec3_t *pos, vec3_t *rot) {
	matrix_t mat;
	vec3_t tmp;

	CGameModel_NodoMatrix(model, &mat, node, 0, NULL, 0);
	pos->x = mat.m[12];
	pos->y = mat.m[13];
	pos->z = mat.m[14];
	*rot = *Matrix_GetEuler(&mat, &tmp);
}

byte __fastcall cFXElementsMentalControllerDamageFX_Init(cFXElementsMentalControllerDamageFX *self) {
	CGameModel *model = &self->base.base.Mod;
	char buf[0x100];
	vec3_t tmp, def;
	int node;

	MentalController_InitBase(&self->base.base, &self->base.dcRoot, &self->base.pos);
	node = CGameModel_FindNodoR(model, "DF_Smoke");
	CHECK(node > -1);
	CGameParticles_Attach(&self->smoke, model, node);
	CHECK(self->smoke.SistPart);
	self->smokeTime = 0;
	for (int i = 0; i < FXMENTALDAMAGEEMITTERS;) {
		i++;
		Str_PrintfQuiet(buf, 0xff, "DF_SmokeEmitter_%02d", i);
		int k = i - 1;
		self->smokeNode[k] = -1;
		self->smokeNode[k] = CGameModel_FindNodoR(model, buf);
		if (self->smokeNode[k] <= -1) {
			break;
		}
		Press_NodePlace(model, self->smokeNode[k], &self->smokePos[k], &self->smokeRot[k]);
		def.x = -85.0f;
		def.y = -55.0f;
		def.z = 350.0f;
		self->smokeVelMin[k] = *CGameModel_NodoIniVec3(model, &tmp, self->smokeNode[k], "SmokeEmitter", "VelMin", &def);
		def.x = 85.0f;
		def.y = 55.0f;
		def.z = 650.0f;
		self->smokeVelMax[k] = *CGameModel_NodoIniVec3(model, &tmp, self->smokeNode[k], "SmokeEmitter", "VelMax", &def);
		def.x = -15.0f;
		def.y = 300.0f;
		def.z = -15.0f;
		self->smokeAccMin[k] = *CGameModel_NodoIniVec3(model, &tmp, self->smokeNode[k], "SmokeEmitter", "AccMin", &def);
		def.x = 15.0f;
		def.y = 950.0f;
		def.z = 15.0f;
		self->smokeAccMax[k] = *CGameModel_NodoIniVec3(model, &tmp, self->smokeNode[k], "SmokeEmitter", "AccMax", &def);
		self->smokeSizeIni[k] = CGameModel_NodoIniFloat(model, self->smokeNode[k], "SmokeEmitter", "SizeIni", 50.0f);
		self->smokeSizeEnd[k] = CGameModel_NodoIniFloat(model, self->smokeNode[k], "SmokeEmitter", "SizeEnd", 250.0f);
		self->smokeLifeMin[k] = CGameModel_NodoIniFloat(model, self->smokeNode[k], "SmokeEmitter", "LifeTimeMin",
			0.5f);
		self->smokeLifeMax[k] = CGameModel_NodoIniFloat(model, self->smokeNode[k], "SmokeEmitter", "LifeTimeMax",
			0.9f);
	}

	node = CGameModel_FindNodoR(model, "DF_Flash");
	CHECK(node > -1);
	CGameParticles_Attach(&self->flash, model, node);
	CHECK(self->flash.SistPart);
	self->flashTime = 0;
	for (int i = 0; i < FXMENTALDAMAGEEMITTERS;) {
		i++;
		Str_PrintfQuiet(buf, 0xff, "DF_FlashEmitter_%02d", i);
		int k = i - 1;
		self->flashNode[k] = -1;
		self->flashNode[k] = CGameModel_FindNodoR(model, buf);
		if (self->flashNode[k] <= -1) {
			break;
		}
		Press_NodePlace(model, self->flashNode[k], &self->flashPos[k], &self->flashRot[k]);
		self->flashSizeMin[k] = CGameModel_NodoIniFloat(model, self->flashNode[k], "FlashEmitter", "SizeMin", 0.8f);
		self->flashSizeMax[k] = CGameModel_NodoIniFloat(model, self->flashNode[k], "FlashEmitter", "SizeMax", 1.2f);
		self->flashSleepMin[k] = CGameModel_NodoIniFloat(model, self->flashNode[k], "FlashEmitter", "SleepTimeMin",
			0.1f);
		self->flashSleepMax[k] = CGameModel_NodoIniFloat(model, self->flashNode[k], "FlashEmitter", "SleepTimeMax",
			0.9f);
		self->flashSparksMin[k] = CGameModel_NodoIniInt(model, self->flashNode[k], "FlashEmitter", "SparksMin", 4);
		self->flashSparksMax[k] = CGameModel_NodoIniInt(model, self->flashNode[k], "FlashEmitter", "SparksMax", 8);
		float lo = self->flashSleepMin[k];
		float hi = self->flashSleepMax[k];
		double r = (double)(crt_rand() & 0x7fff) * g_RandScale;
		self->flashSleep[k] = (float)(r * (double)(hi - lo) + (double)lo);
	}
	return 1;
}

/* ---- cFXElementsMentalControllerMainFX ---- */

/* the trail of a tube: 31 points along up, texture tex */
static void MainFX_TubeTrail(CGameTrail *t, const vec3_t *up, byte mode, int tex) {
	CGameTrail_SetMaxTrails(t, 0x1f);
	CGameTrail_SetType(t, 3);
	CGameTrail_SetOrient(t, *up, mode);
	CGameTrail_SetTexture(t, tex);
}

byte __fastcall cFXElementsMentalControllerMainFX_Init(cFXElementsMentalControllerMainFX *self) {
	CGameModel *model = &self->base.Mod;
	vec3_t up;
	int node;

	MentalController_InitBase(&self->base, &self->dcRoot, &self->pos);
	self->top.x = self->pos.x + 0.0f;
	self->top.y = self->pos.y + 350.0f;
	self->top.z = self->pos.z + 0.0f;
	node = CGameModel_FindNodoR(model, "DF_DataFluid");
	CHECK(node > -1);
	CGameParticles_Attach(&self->dataFluid, model, node);
	CHECK(self->dataFluid.SistPart);

	up.x = 0.0f;
	up.y = 1.0f;
	up.z = 0.0f;
	for (int i = 0; i < FXMENTALMAINTRAILS; i++) {
		MainFX_TubeTrail(&self->tubeTrails[i], &up, 2, 0xd);
		MainFX_TubeTrail(&self->glowTrails[i], &up, 2, 0xe);
		float du = (float)((double)(crt_rand() & 0x7fff) * g_RandScale * 3.0 + 3.0);
		CGameTrail_SetUVScale(&self->glowTrails[i], du, 1.0f);
		/* the quarter of the texture of the first 8 tubes */
		if (i < 8) {
			CGameTrail_SetUV(&self->tubeTrails[i], (float)(i & 3) * 0.25f, (i & 4) ? 0.5f : 0.0f);
		}
		CGameTrail_SetUVScale(&self->tubeTrails[i], 0.25f, 0.5f);
	}
	for (uint32_t j = 0; j < FXMENTALMAINFLUID; j++) {
		self->fluidSpeed[j] = 10.0f;
		self->fluidTube[j] = j % 15;
		int g = crt_rand() % 0x30;
		int b = crt_rand() % 32;
		self->fluidColor[j] = (uint32_t)(b + 0x80) << 16 | (uint32_t)(g + 0x80) << 8 | 0xff;
	}
	node = CGameModel_FindNodoR(model, "DF_TubeGlow");
	CHECK(node > -1);
	CGameParticles_Attach(&self->tubeGlow, model, node);
	CHECK(self->tubeGlow.SistPart);
	self->glow3f14 = 0;
	self->glow3f18 = 0;
	self->glow3f1c = 0;
	for (int k = 0; k < FXMENTALMAINGLOWS; k++) {
		self->glowPhase[k] = (float)((double)(crt_rand() & 0x7fff) * g_RandScale - 1.0);
		double r = (double)(crt_rand() & 0x7fff);
		self->glowSide[k] = k & 1;
		self->glowSpeed[k] = (float)(r * g_RandScale * 0.19999998807907104 + 0.800000011920929);
		int g = crt_rand() % 32;
		int r2 = crt_rand() % 32;
		int b = crt_rand() % 64;
		self->glowColor[k] = (uint32_t)(b + 0x80) | (uint32_t)(r2 + 0x80) << 16 | (uint32_t)(g + 0xd7) << 8;
	}
	for (int m = 0; m < 2; m++) {
		CGameTrail *t = &self->topTrails[m];
		MainFX_TubeTrail(t, &self->top, 1, 0xc);
		if (m == 0) {
			CGameTrail_SetUV(t, 0.0f, 0.0f);
		} else {
			CGameTrail_SetUV(t, 0.5f, 0.5f);
		}
		CGameTrail_SetUVScale(t, 1.0f, 0.5f);
	}
	return 1;
}

void __thiscall cFXElementsMentalControllerMainFX_SetController(cFXElementsMentalControllerMainFX *self, int index,
	cFXElementsMentalController *ctrl) {
	CHECK(index >= 0 && index < FXMENTALCONTROLLERMAINCOUNT);
	self->controllers[index] = ctrl;
	CHECK(CGameModel_IsLoaded(&ctrl->base.Mod));
}

void __thiscall cFXElementsMentalControllerMainFX_SetPhase(cFXElementsMentalControllerMainFX *self, int phase) {
	self->phase = phase;
	CHECK(phase >= 1 && phase <= 5);
	switch (phase) {
	case 0:
	case 1:
	case 2:
	case 5:
		for (int i = FXMENTALCONTROLLERLEVELS - 1; i >= 0; i--) {
			self->levels[i] = 0.0f;
		}
		break;
	case 3:
		for (int i = 0; i < FXMENTALCONTROLLERLEVELS; i++) {
			self->levels[i] = i < 4 ? 1.0f : 0.0f;
		}
		break;
	case 4:
		for (int i = 0; i < FXMENTALCONTROLLERLEVELS; i++) {
			self->levels[i] = 1.0f;
		}
		break;
	}
}

/* ---- the elements of the level ---- */

static void PressElement_Add(cMapElement *e) {
	e->vftable->Init(e);
	e->next = World_Elements(g_pWorld);
	World_Elements(g_pWorld) = e;
}

/* the phase of the story: 1 prologue, 2..4 the murders, 4 epilogue, 5 epilogue with the controller damaged */
static int Press_Phase(const char *act, const char *status) {
	if (!strcmp(act, "Prologue")) {
		return 1;
	}
	if (!strcmp(act, "1stMurder")) {
		return 2;
	}
	if (!strcmp(act, "2ndMurder")) {
		return 3;
	}
	if (!strcmp(act, "3rdMurder")) {
		return 4;
	}
	if (!strcmp(act, "Epilogue")) {
		return status && !strcmp(status, "1") ? 5 : 4;
	}
	return 0;
}

void FXElementsPress_Create(void) {
	char buf[0x80];
	char *name = buf + PRESS_NAME_SKIP;
	vec3_t pos, rot;
	float ang[2];
	byte flag;

	const char *act = SaveVar_Get("GameAct");
	const char *status = SaveVar_Get("Press.MentalControllerStatus");
	int phase = Press_Phase(act, status);
	g_MentalMainFX = NULL;
	if (phase >= 2 && phase <= 4) {
		DummySearch_Start("DM_Element_MentalControllerMainFX", 0);
		while (DummySearch_Next(buf, sizeof(buf), &pos, ang, &flag)) {
			char *p = strrchr(name, '_');
			if (p) {
				*p = 0;
			}
			cFXElementsMentalControllerMainFX *e = NULL;
			if (strstr(name, "MentalControllerMainFX")) {
				void *mem = operator_new(sizeof(cFXElementsMentalControllerMainFX));
				if (mem) {
					rot.x = ang[0];
					rot.y = ang[1];
					rot.z = 0.0f;
					e = cFXElementsMentalControllerMainFX_ctor(mem, name, &pos, &rot);
				}
				g_MentalMainFX = e;
			}
			CHECK(e);
			PressElement_Add(&e->base);
		}
	}

	DummySearch_Start("DM_Element_MentalController_*", 1);
	while (DummySearch_Next(buf, sizeof(buf), &pos, ang, &flag)) {
		char *p = strrchr(name, '_');
		if (p) {
			*p = 0;
			p++;
		}
		int index = crt_atoi(p);
		cMapElement *e = NULL;
		void *mem;
		rot.x = ang[0];
		rot.y = ang[1];
		rot.z = 0.0f;
		if (phase == 5) {
			mem = operator_new(sizeof(cFXElementsMentalControllerDamageFX));
			if (mem) {
				e = &cFXElementsMentalControllerDamageFX_ctor(mem, "MentalController0", &pos, &rot)->base.base;
			}
		} else if (phase == 4 || (phase == 3 && index >= 5)) {
			mem = operator_new(sizeof(cFXElementsMentalController));
			if (mem) {
				e = &cFXElementsMentalController_ctor(mem, "MentalController1", &pos, &rot)->base;
			}
		} else {
			mem = operator_new(sizeof(cMapElement));
			if (mem) {
				e = cMapElement_ctor(mem, "MentalController2", &pos, &rot);
			}
		}
		cFXElementsMentalControllerMainFX *main = g_MentalMainFX;
		if (main) {
			cFXElementsMentalControllerMainFX_SetController(main, index, (cFXElementsMentalController *)e);
		}
		CHECK(e);
		PressElement_Add(e);
	}
	if (g_MentalMainFX) {
		cFXElementsMentalControllerMainFX_SetPhase(g_MentalMainFX, phase);
	}
}
