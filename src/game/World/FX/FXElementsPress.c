/*
 * FXElementsPress.cpp of the game: the mental controllers of the Press building and the main controller that feeds
 * them, see include/scrap/fxelements.h.
 */
#include <string.h>

#include "scrap/fxelements.h"
#include "scrap/fxmisc.h"
#include "scrap/mathlib.h"
#include "scrap/scrap.h"
#include "scrap/sound.h"

#define g_RandScale GAME_GLOBAL(double, g_FxRandScale)	/* 1/32767 */
#define PRESS_NAME_SKIP 11			/* "DM_Element_" before the name of the element in the dummy */
#define CGAMETRAIL_CTOR 0x21ddb0
#define CGAMETRAIL_DTOR 0x21de40

/* ---- construction ---- */

cFXElementsMentalController *__thiscall cFXElementsMentalController_ctor(cFXElementsMentalController *self,
	const char *name, const vec3_t *pos, const vec3_t *rot) {
	cMapElement_ctor(&self->base, name, pos, rot);
	self->base.vftable = CFXELEMENTSMENTALCONTROLLER_VFTABLE;
	return self;
}

cFXElementsMentalControllerDamageFX *__thiscall cFXElementsMentalControllerDamageFX_ctor(
	cFXElementsMentalControllerDamageFX *self, const char *name, const vec3_t *pos, const vec3_t *rot) {
	cMapElement_ctor(&self->base.base, name, pos, rot);
	self->base.base.vftable = CFXELEMENTSMENTALCONTROLLERDAMAGEFX_VFTABLE;
	CGameParticles_ctor(&self->smoke);
	CGameParticles_ctor(&self->flash);
	return self;
}

cFXElementsMentalControllerMainFX *__thiscall cFXElementsMentalControllerMainFX_ctor(
	cFXElementsMentalControllerMainFX *self, const char *name, const vec3_t *pos, const vec3_t *rot) {
	cMapElement_ctor(&self->base, name, pos, rot);
	self->base.vftable = CFXELEMENTSMENTALCONTROLLERMAINFX_VFTABLE;
	crt_eh_vector_ctor(self->tubeTrails, sizeof(CGameTrail), FXMENTALMAINTRAILS, GAME_FUNC(CGameTrail_ctor),
		GAME_FUNC(CGameTrail_dtor));
	CGameParticles_ctor(&self->dataFluid);
	crt_eh_vector_ctor(self->glowTrails, sizeof(CGameTrail), FXMENTALMAINTRAILS, GAME_FUNC(CGameTrail_ctor),
		GAME_FUNC(CGameTrail_dtor));
	CGameParticles_ctor(&self->tubeGlow);
	crt_eh_vector_ctor(self->topTrails, sizeof(CGameTrail), 2, GAME_FUNC(CGameTrail_ctor), GAME_FUNC(CGameTrail_dtor));
	for (int i = 0; i < FXMENTALCONTROLLERMAINCOUNT; i++) {
		self->controllers[i] = NULL;
	}
	self->scale = 1.0f;
	self->last = -2;
	return self;
}

void __fastcall cFXElementsMentalController_dtor(cFXElementsMentalController *self) {
	self->base.vftable = CFXELEMENTSMENTALCONTROLLER_VFTABLE;
	cMapElement_dtor(&self->base);
}

void __fastcall cFXElementsMentalControllerDamageFX_dtor(cFXElementsMentalControllerDamageFX *self) {
	self->base.base.vftable = CFXELEMENTSMENTALCONTROLLERDAMAGEFX_VFTABLE;
	CGameParticles_Release(&self->smoke);
	CGameParticles_Release(&self->flash);
	CGameParticles_Release(&self->flash);
	CGameParticles_Release(&self->smoke);
	cMapElement_dtor(&self->base.base);
}

void __fastcall cFXElementsMentalControllerMainFX_dtor(cFXElementsMentalControllerMainFX *self) {
	self->base.vftable = CFXELEMENTSMENTALCONTROLLERMAINFX_VFTABLE;
	CGameParticles_Release(&self->dataFluid);
	crt_eh_vector_dtor(self->topTrails, sizeof(CGameTrail), 2, GAME_FUNC(CGameTrail_dtor));
	CGameParticles_Release(&self->tubeGlow);
	crt_eh_vector_dtor(self->glowTrails, sizeof(CGameTrail), FXMENTALMAINTRAILS, GAME_FUNC(CGameTrail_dtor));
	CGameParticles_Release(&self->dataFluid);
	crt_eh_vector_dtor(self->tubeTrails, sizeof(CGameTrail), FXMENTALMAINTRAILS, GAME_FUNC(CGameTrail_dtor));
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
	*dcRoot = CGameModel_FindRenderNode(&e->Mod, "DC_Root");
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

	CGameModel_NodeMatrix(model, &mat, node, 0, NULL, 0);
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
	node = CGameModel_FindRenderNode(model, "DF_Smoke");
	CHECK(node > -1);
	CGameParticles_Attach(&self->smoke, model, node);
	CHECK(self->smoke.ParticleSystem);
	self->smokeTime = 0.0f;
	for (int i = 0; i < FXMENTALDAMAGEEMITTERS;) {
		i++;
		Str_PrintfQuiet(buf, 0xff, "DF_SmokeEmitter_%02d", i);
		int k = i - 1;
		self->smokeNode[k] = -1;
		self->smokeNode[k] = CGameModel_FindRenderNode(model, buf);
		if (self->smokeNode[k] <= -1) {
			break;
		}
		Press_NodePlace(model, self->smokeNode[k], &self->smokePos[k], &self->smokeRot[k]);
		def.x = -85.0f;
		def.y = -55.0f;
		def.z = 350.0f;
		self->smokeVelMin[k] = *CGameModel_NodeIniVec3(model, &tmp, self->smokeNode[k], "SmokeEmitter", "VelMin", &def);
		def.x = 85.0f;
		def.y = 55.0f;
		def.z = 650.0f;
		self->smokeVelMax[k] = *CGameModel_NodeIniVec3(model, &tmp, self->smokeNode[k], "SmokeEmitter", "VelMax", &def);
		def.x = -15.0f;
		def.y = 300.0f;
		def.z = -15.0f;
		self->smokeAccMin[k] = *CGameModel_NodeIniVec3(model, &tmp, self->smokeNode[k], "SmokeEmitter", "AccMin", &def);
		def.x = 15.0f;
		def.y = 950.0f;
		def.z = 15.0f;
		self->smokeAccMax[k] = *CGameModel_NodeIniVec3(model, &tmp, self->smokeNode[k], "SmokeEmitter", "AccMax", &def);
		self->smokeSizeIni[k] = CGameModel_NodeIniFloat(model, self->smokeNode[k], "SmokeEmitter", "SizeIni", 50.0f);
		self->smokeSizeEnd[k] = CGameModel_NodeIniFloat(model, self->smokeNode[k], "SmokeEmitter", "SizeEnd", 250.0f);
		self->smokeLifeMin[k] = CGameModel_NodeIniFloat(model, self->smokeNode[k], "SmokeEmitter", "LifeTimeMin",
			0.5f);
		self->smokeLifeMax[k] = CGameModel_NodeIniFloat(model, self->smokeNode[k], "SmokeEmitter", "LifeTimeMax",
			0.9f);
	}

	node = CGameModel_FindRenderNode(model, "DF_Flash");
	CHECK(node > -1);
	CGameParticles_Attach(&self->flash, model, node);
	CHECK(self->flash.ParticleSystem);
	self->flashTime = 0.0f;
	for (int i = 0; i < FXMENTALDAMAGEEMITTERS;) {
		i++;
		Str_PrintfQuiet(buf, 0xff, "DF_FlashEmitter_%02d", i);
		int k = i - 1;
		self->flashNode[k] = -1;
		self->flashNode[k] = CGameModel_FindRenderNode(model, buf);
		if (self->flashNode[k] <= -1) {
			break;
		}
		Press_NodePlace(model, self->flashNode[k], &self->flashPos[k], &self->flashRot[k]);
		self->flashSizeMin[k] = CGameModel_NodeIniFloat(model, self->flashNode[k], "FlashEmitter", "SizeMin", 0.8f);
		self->flashSizeMax[k] = CGameModel_NodeIniFloat(model, self->flashNode[k], "FlashEmitter", "SizeMax", 1.2f);
		self->flashSleepMin[k] = CGameModel_NodeIniFloat(model, self->flashNode[k], "FlashEmitter", "SleepTimeMin",
			0.1f);
		self->flashSleepMax[k] = CGameModel_NodeIniFloat(model, self->flashNode[k], "FlashEmitter", "SleepTimeMax",
			0.9f);
		self->flashSparksMin[k] = CGameModel_NodeIniInt(model, self->flashNode[k], "FlashEmitter", "SparksMin", 4);
		self->flashSparksMax[k] = CGameModel_NodeIniInt(model, self->flashNode[k], "FlashEmitter", "SparksMax", 8);
		float lo = self->flashSleepMin[k];
		float hi = self->flashSleepMax[k];
		double r = (double)(crt_rand() & 0x7fff) * g_RandScale;
		self->flashSleep[k] = (float)(r * (double)(hi - lo) + (double)lo);
	}
	return 1;
}

#define g_DamageFXMaxDist GAME_GLOBAL_IN(float, GVar_FXElementsMentalControllerDamageFXMaxDist, 16)		/* FXElementsMentalControllerDamageFXMaxDist */
#define g_DamageFXSound GAME_GLOBAL_IN(const char *, GVar_FXElementsMentalControllerDamageSound, 16)	/* FXElementsMentalControllerDamageSound */
#define g_DamageFXSoundVol GAME_GLOBAL_IN(float, GVar_FXElementsMentalControllerDamageSoundVol, 16)		/* FXElementsMentalControllerDamageSoundVol */
#define DAMAGEFX_SMOKE_RAMPS 0x4309f8				/* 3 color ramps of 9 colors */
#define DAMAGEFX_FLASH_RAMPS 0x430a68

static inline uint32_t *Press_Ramp(uint32_t *ramps, int i) {
	return ramps + i * 9;
}

/* one flash glow at the emitter: sizes and life random, scaled by s */
static void DamageFX_Glow(CGameParticles *p, float s, double iniRange, double iniAdd, double endRange, double endAdd,
	byte sprite, double lifeRange, double lifeAdd) {
	CGameParticles_SetColorRamp(p, Press_Ramp(GAME_ARRAY(uint32_t, DamageFX_Glow_430a68), 0));
	p->sizeIni = FxElem_Rand(iniRange, iniAdd) * s;
	p->sizeEnd = FxElem_Rand(endRange, endAdd) * s;
	float lt = FxElem_Rand(lifeRange, lifeAdd);
	p->sizeLife = 0.15f;
	p->spriteIni = sprite;
	p->spriteEnd = sprite;
	p->lifeTime = lt;
	CGameParticles_Apply(p);
	CGameParticles_EmitN(p, 1, 1);
}

void __thiscall cFXElementsMentalControllerDamageFX_Update(cFXElementsMentalControllerDamageFX *self, float dt) {
	CGameModel *model = &self->base.base.Mod;
	vec3_t tmp, vel, acc, ang, dir, out;

	cMapElement_Update(&self->base.base, dt);
	CHECK(CGameModel_IsLoaded(model));
	if (!CGameModel_IsVisible(model)) {
		goto far;
	}
	vec3_t cam = g_pWorld->camPos[g_pWorld->curCam];
	const vec3_t *p = CGameModel_GetPos(model, &tmp);
	float max2 = g_DamageFXMaxDist * g_DamageFXMaxDist;
	float dy = p->y - cam.y;
	float dx = p->x - cam.x;
	float dz = p->z - cam.z;
	float d2 = dy * dy + dx * dx + dz * dz;
	if (d2 > max2) {
		goto far;
	}
	if (!self->base.nearCam) {
		self->base.nearCam = 1;
	}

	/* the smoke: more often the nearer the camera */
	float step = ((d2 / max2) * 3.0f + 1.0f) * 0.12f;
	float st = self->smokeTime + dt;
	self->smokeTime = st;
	if (st > step) {
		CGameParticles *sm = &self->smoke;
		sm->birthRate = 0;
		CGameParticles_SetColorRampOn(sm, 1);
		CGameParticles_SetColorRamp(sm, Press_Ramp(GAME_ARRAY(uint32_t, cFXElementsMentalControllerDamageFX_Update_4309f8), crt_rand() % 3));
		for (int k = 0; k < FXMENTALDAMAGEEMITTERS; k++) {
			if (self->smokeNode[k] <= -1) {
				break;
			}
			float t = 0.0f;
			if (!(self->smokeTime > 0.0f)) {
				continue;
			}
			do {
				Vec3_Rotate(&vel, FX_RandomVec(&tmp, &self->smokeVelMin[k], &self->smokeVelMax[k]),
					&self->smokeRot[k]);
				FX_RandomVec(&acc, &self->smokeAccMin[k], &self->smokeAccMax[k]);
				sm->initPosMin = self->smokePos[k];
				sm->initPosMax = sm->initPosMin;
				sm->initVelMin = vel;
				sm->initVelMax = sm->initVelMin;
				sm->initAccMin = acc;
				sm->initAccMax = sm->initAccMin;
				sm->sizeIni = FxElem_Rand(0.20000004768371582, 0.89999997615814209) * self->smokeSizeIni[k];
				sm->sizeEnd = self->smokeSizeEnd[k] * FxElem_Rand(0.20000004768371582, 0.89999997615814209);
				float lo = self->smokeLifeMin[k], hi = self->smokeLifeMax[k];
				sm->lifeTime = FxElem_Rand((double)(hi - lo), (double)lo);
				float r = FxElem_Rand(6.2831854820251465, 0.0);
				sm->rotIni = r;
				sm->rotEnd = r;
				sm->sizeLife = 1.0f;
				sm->rotVar = FxElem_Rand(1.2566370964050293, -0.62831854820251465);
				byte sprite = (byte)((crt_rand() & 1) + 0x11);
				sm->spriteIni = sprite;
				sm->spriteEnd = sprite;
				CGameParticles_Apply(sm);
				CGameParticles_EmitDt(sm, 1, 0, t);
				t = t + step;
			} while (self->smokeTime > t);
		}
		self->smokeTime = 0.0f;
	}

	/* the flashes: each emitter sleeps a random time, sparks and a sound when the camera is near */
	float ft = self->flashTime + dt;
	self->flashTime = ft;
	if (!(ft > 0.04f)) {
		return;
	}
	CGameParticles *fl = &self->flash;
	fl->birthRate = 0;
	CGameParticles_SetColorRampOn(fl, 1);
	float half = g_DamageFXMaxDist * 0.5f;
	float half2 = half * half;
	for (int k = 0; k < FXMENTALDAMAGEEMITTERS; k++) {
		if (self->flashNode[k] <= -1) {
			break;
		}
		float sl = self->flashSleep[k] - self->flashTime;
		self->flashSleep[k] = sl;
		if (!(0.0f >= sl)) {
			continue;
		}
		float lo = self->flashSleepMin[k], hi = self->flashSleepMax[k];
		self->flashSleep[k] = FxElem_Rand((double)(hi - lo), (double)lo);
		const vec3_t *fpos = &self->flashPos[k];
		fl->initPosMin = *fpos;
		fl->initPosMax = fl->initPosMin;
		fl->initVelMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
		fl->initVelMax = fl->initVelMin;
		fl->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
		fl->initAccMax = fl->initAccMin;
		lo = self->flashSizeMin[k];
		hi = self->flashSizeMax[k];
		float s = FxElem_Rand((double)(hi - lo), (double)lo);
		DamageFX_Glow(fl, s, 10.0, 10.0, 15.0, 95.0, 6, 0.019999995827674866, 0.18000000715255737);
		DamageFX_Glow(fl, s, 2.0, 4.0, 15.0, 55.0, 5, 0.02000001072883606, 0.15999999642372131);
		if (!(half2 > d2)) {
			continue;
		}
		int lo2 = self->flashSparksMin[k];
		int n = lo2 + crt_rand() % (self->flashSparksMax[k] - lo2);
		CGameParticles_SetSparksOn(fl, 1);
		for (; n > 0; n--) {
			CGameParticles_SetColorRamp(fl, Press_Ramp(GAME_ARRAY(uint32_t, DamageFX_Glow_430a68), crt_rand() % 3));
			float az = FxElem_Rand(3.1415927410125732, -1.5707963705062866);
			float ay = FxElem_Rand(3.1415927410125732, -1.5707963705062866);
			ang.x = 0.0f;
			ang.y = ay;
			ang.z = az;
			tmp = (vec3_t){ 1.0f, 0.0f, 0.0f };
			Vec3_Rotate(&dir, &tmp, &ang);
			dir = *Vec3_Rotate(&out, &dir, &self->flashRot[k]);
			float r = FxElem_Rand(5.0, 3.0);
			fl->initPosMin.x = r * dir.x + fpos->x;
			fl->initPosMin.y = fpos->y + r * dir.y;
			fl->initPosMin.z = r * dir.z + fpos->z;
			fl->initPosMax = fl->initPosMin;
			r = FxElem_Rand(670.0, 280.0);
			fl->initVelMin.x = r * dir.x * s;
			fl->initVelMin.y = r * dir.y * s;
			fl->initVelMin.z = r * dir.z * s;
			fl->initVelMax = fl->initVelMin;
			r = FxElem_Rand(0.099999994039535522, 0.30000001192092896);
			fl->initAccMin.x = r * -fl->initVelMin.x + 0.0f;
			fl->initAccMin.y = r * -fl->initVelMin.y - 2950.0f;
			fl->initAccMin.z = r * -fl->initVelMin.z + 0.0f;
			fl->initAccMax = fl->initAccMin;
			float e = FxElem_Rand(0.029999999329447746, 0.0);
			float lt = FxElem_Rand(0.15000000596046448, 0.25);
			CGameParticles_SetSparksOn(fl, 1);
			fl->lifeTime = lt;
			float size = FxElem_Rand(20.0, 15.0) * s;
			fl->sizeLife = 1.0f;
			fl->rotIni = 0.0f;
			fl->rotEnd = 0.0f;
			fl->rotVar = 0.0f;
			fl->sizeIni = size;
			fl->sizeEnd = size;
			byte sprite = (byte)((crt_rand() & 1) + 0x41);
			fl->spriteEnd = 0x44;
			fl->sparkVelScale = 1.0f;
			fl->spriteIni = sprite;
			fl->sparkMaxSize = FxElem_Rand(0.59999996423721313, 0.80000001192092896);
			CGameParticles_Apply(fl);
			CGameParticles_EmitDt(fl, 1, 1, e + 0.03f);
		}
		CGameParticles_SetSparksOn(fl, 0);
		if (crt_rand() & 1) {
			CSoundSys *snd = g_pSound;
			const char *name = g_DamageFXSound;
			snd->vftable->PlayAt(snd, name, fpos, Snd_Volume(g_DamageFXSoundVol));
		}
	}
	self->flashTime = 0.0f;
	return;

far:
	if (self->base.nearCam) {
		self->base.nearCam = 0;
	}
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
	node = CGameModel_FindRenderNode(model, "DF_DataFluid");
	CHECK(node > -1);
	CGameParticles_Attach(&self->dataFluid, model, node);
	CHECK(self->dataFluid.ParticleSystem);

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
		self->fluidPhase[j] = 10.0f;
		self->fluidTube[j] = j % 15;
		int g = crt_rand() % 0x30;
		int b = crt_rand() % 32;
		self->fluidColor[j] = (uint32_t)(b + 0x80) << 16 | (uint32_t)(g + 0x80) << 8 | 0xff;
	}
	node = CGameModel_FindRenderNode(model, "DF_TubeGlow");
	CHECK(node > -1);
	CGameParticles_Attach(&self->tubeGlow, model, node);
	CHECK(self->tubeGlow.ParticleSystem);
	self->glowTime = 0.0f;
	self->glowDuration = 0.0f;
	self->glowPause = 0.0f;
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

/* the 8 ways of a drop: from controller, to controller; from -1 is the way up the tube to the controller */
static const signed char MainFX_FluidWays[8][2] = {
	{ -1, 8 }, { 8, 7 }, { 7, 6 }, { 6, 5 }, { 5, 4 }, { 4, 3 }, { 0, 1 }, { 1, 2 },
};

static CGameModel *MainFX_Model(cFXElementsMentalControllerMainFX *self, int i) {
	return &self->controllers[i]->base.Mod;
}

/* p = the position of controller i raised by 275 plus off turned by rot */
static void MainFX_Place(cFXElementsMentalControllerMainFX *self, int i, const vec3_t *off, const vec3_t *rot,
	vec3_t *p) {
	vec3_t base, tmp;

	CGameModel_GetPos(MainFX_Model(self, i), &base);
	float bx = base.x + 0.0f;
	float by = base.y + 275.0f;
	float bz = base.z + 0.0f;
	const vec3_t *r = Vec3_Rotate(&tmp, off, rot);
	p->x = bx + r->x;
	p->y = r->y + by;
	p->z = r->z + bz;
}

/* a new drop j: its time, width, color and way; active when the level of the way is high enough and one end seen */
static void MainFX_FluidNew(cFXElementsMentalControllerMainFX *self, int j) {
	vec3_t v, rot, tmp;
	uint32_t color;

	self->fluidPhase[j] = 0.0f;
	self->fluidTime[j] = FxElem_Rand(0.59999996423721313, 0.30000001192092896);
	self->fluidWidth[j] = FxElem_Rand(25.0, 35.0);
	int k = crt_rand() % 8;
	int from = MainFX_FluidWays[k][0];
	int to = MainFX_FluidWays[k][1];
	int last = self->last;
	if ((last >= 0 && (from == last || to == last) && (crt_rand() & 1)) || !(2.92f > self->scale)) {
		int g = crt_rand() % 64;
		int r = crt_rand() % 32;
		int b = crt_rand() % 64;
		color = (uint32_t)(b + 0x18) | ((uint32_t)(r + 0xdc) << 16 | (uint32_t)(g + 0x18) << 8);
	} else {
		int g = crt_rand() % 0x30;
		int r = crt_rand() % 32;
		color = (uint32_t)(r + 0x80) << 16 | (uint32_t)(g + 0x80) << 8 | 0xff;
	}
	self->fluidColor[j] = color;
	float chance = FxElem_Rand(1.0, 0.0);
	self->fluidActive[j] = 0;
	if (!(self->levels[k] > chance)) {
		CGameParticles_Kill(&self->dataFluid, j);
		return;
	}
	self->fluidActive[j] = 1;

	vec3_t lo = { 0.0f, from == -1 ? -350.0f : -450.0f, -625.0f };
	vec3_t hi = { 0.0f, 400.0f - (1.0f - self->levels[k]) * 850.0f, 525.0f };
	FX_RandomVec(&v, &lo, &hi);
	v.y = (float)(int)(v.y / 60.0f) * 60.0f;
	v.z = (float)(int)(v.z / 20.0f) * 20.0f;
	vec3_t *pos = &self->fluidPos[j];
	vec3_t *end = &self->fluidEnd[j];
	if (from == -1) {
		/* up the tube: from around the top to the controller */
		CGameModel_GetRot(MainFX_Model(self, to), &rot);
		vec3_t off = { 0.0f, v.y, -600.0f };
		MainFX_Place(self, to, &off, &rot, end);
		float s = FxElem_Rand(90.0, 0.0);
		const vec3_t *d = FX_RandomDir(&tmp);
		pos->x = s * d->x + self->top.x + 0.0f;
		pos->z = self->top.z + d->z * s + 0.0f;
		pos->y = end->y + (self->top.y + d->y * s) * 0.0f;
		if (!CGameModel_IsVisible(MainFX_Model(self, to)) &&
			!FX_IsVisible(self->top.x, self->top.y, self->top.z, 500.0f, 0.0f, NULL, 0)) {
			self->fluidActive[j] = 0;
		}
	} else {
		/* the side of the controller that faces the next one */
		vec3_t pa, pb;
		CGameModel_GetPos(MainFX_Model(self, from), &pa);
		CGameModel_GetPos(MainFX_Model(self, to), &pb);
		vec3_t n = FX_Unit((vec3_t){ pb.x - pa.x, pb.y - pa.y, pb.z - pa.z });
		CGameModel_GetRot(MainFX_Model(self, from), &rot);
		const vec3_t zAxis = { 0.0f, 0.0f, 1.0f };
		float ang = (float)Vec3_Angle(Vec3_Rotate(&tmp, &zAxis, &rot), &n);
		v.x = ang > 0.0f ? -100.0f : (0.0f > ang ? 100.0f : 0.0f);
		CGameModel_GetRot(MainFX_Model(self, from), &rot);
		MainFX_Place(self, from, &v, &rot, pos);
		CGameModel_GetRot(MainFX_Model(self, to), &rot);
		vec3_t off = { -v.x, v.y, v.z };
		MainFX_Place(self, to, &off, &rot, end);
		if (!CGameModel_IsVisible(MainFX_Model(self, from)) && !CGameModel_IsVisible(MainFX_Model(self, to))) {
			self->fluidActive[j] = 0;
		}
	}
	vec3_t d = { end->x - pos->x, end->y - pos->y, end->z - pos->z };
	self->fluidDir[j] = d;
	self->fluidNormal[j] = FX_Unit(d);
	if (!self->fluidActive[j]) {
		CGameParticles_Kill(&self->dataFluid, j);
	}
}

/* drop j at phase ph: a piece of its tube, the drop itself near the ends and its glow */
static void MainFX_FluidMove(cFXElementsMentalControllerMainFX *self, int j, float ph) {
	const vec3_t *p = &self->fluidPos[j];
	const vec3_t *d = &self->fluidDir[j];
	const vec3_t *n = &self->fluidNormal[j];
	CGameParticles *fl = &self->dataFluid;
	vec3_t a, b;
	float f;

	float w = self->fluidWidth[j];
	float nx = n->x * w;
	float ny = n->y * w;
	float nz = n->z * w;
	a.x = d->x * ph + p->x - nx;
	a.y = d->y * ph + p->y - ny;
	a.z = p->z + d->z * ph - nz;
	b.x = nx + a.x;
	b.y = ny + a.y;
	b.z = nz + a.z;
	int alpha = 0x60;
	if (ph > 0.9f) {
		alpha = 0x60 - (int)((ph - 0.9f) * -10.0f * 158.0f);
	}
	if (alpha > 0xff) {
		alpha = 0xff;
	}
	uint32_t color = (uint32_t)alpha << 24 | (self->fluidColor[j] & 0xffffff);
	CGameTrail *tube = &self->tubeTrails[self->fluidTube[j]];
	CGameTrail_Add(tube, a, w, color);
	CGameTrail_Add(tube, b, w, color);

	ph = self->fluidPhase[j];
	if (0.4f > ph) {
		FX_InRange(ph, 0.0f, 0.4f, &f);
		color = (uint32_t)(int)(f * 255.0f) << 24 | (self->fluidColor[j] & 0xffffff);
		CGameParticles_SetParticle(fl, j, p->x, p->y, p->z, (1.0f - f) * w * 10.0f, color, 6, 0.0f);
	} else if (ph > 0.8f) {
		FX_InRange(ph, 0.8f, 1.0f, &f);
		color = (uint32_t)(int)(f * 255.0f) << 24 | (self->fluidColor[j] & 0xffffff);
		const vec3_t *e = &self->fluidEnd[j];
		CGameParticles_SetParticle(fl, j, e->x, e->y, e->z, f * w * 10.0f, color, 6, 0.0f);
	} else {
		CGameParticles_Kill(fl, j);
	}
	FX_InRange(self->fluidPhase[j], 0.2f, 1.0f, &f);
	f = (3.0f - f * 2.0f) * (f * f);
	CGameTrail *glow = &self->glowTrails[self->fluidTube[j]];
	color = (uint32_t)(0x60 - (int)(f * -159.0f)) << 24 | (self->fluidColor[j] & 0xffffff);
	CGameTrail_Add(glow, *p, w, color);
	color = (uint32_t)(0x60 - (int)(f * -159.0f)) << 24 | (self->fluidColor[j] & 0xffffff);
	CGameTrail_Add(glow, b, w, color);
}

/* the glow up the tube (particles 0..39): on for glowTime, then a pause */
static void MainFX_TubeGlow(cFXElementsMentalControllerMainFX *self, float dt) {
	CGameParticles *tg = &self->tubeGlow;
	vec3_t tmp;
	float s;

	if (self->phase == 4) {
		self->glowTime = self->glowDuration;
	}
	if (!(self->glowTime > 0.0f)) {
		float p = self->glowPause - dt;
		self->glowPause = p;
		if (0.0f >= p) {
			self->glowDuration = FxElem_Rand(0.54999995231628418, 0.15000000596046448);
			self->glowTime = self->glowDuration;
			self->glowPause = FxElem_Rand(0.25, 0.05000000074505806);
		}
		self->glowPause = 0.0f;
		return;
	}
	float t = self->glowTime - dt;
	self->glowTime = t;
	if (!(t > 0.0f)) {
		CGameParticles_KillRange(tg, 0, 40);
		return;
	}
	if (self->phase == 4) {
		s = 0.8f;
	} else {
		float lim = self->glowDuration * 0.2f;
		if (FX_InRange(t, 0.0f, lim, &s)) {
			s = s * s * (2.0f - s);
		} else if (FX_InRange(self->glowTime, lim, self->glowDuration, &s)) {
			s = 1.0f - (3.0f - s * 2.0f) * (s * s);
		}
	}
	uint32_t alpha = (uint32_t)(int)(255.0f - s * 222.0f) << 24;
	for (int i = 0; i < 40; i++) {
		float rr = FxElem_Rand(50.0, 0.0);
		const vec3_t *d = FX_RandomDir(&tmp);
		float x = self->top.x + 0.0f + d->x * rr;
		float y = self->top.y + ((float)i * 100.0f - 275.0f) + d->y * rr;
		float z = self->top.z + 0.0f + d->z * rr;
		float size = FxElem_Rand(390.0, 390.0) * s;
		if (self->scale > 1.0f) {
			size = (self->scale * 0.2f + 1.0f) * size;
		}
		uint32_t color;
		int last = self->last;
		if (last >= 0 && last <= 2) {
			int g = crt_rand() % 0x18;
			int r = crt_rand() % 16;
			int b = crt_rand() % 32;
			color = (uint32_t)(b + 0x10) | ((uint32_t)(r + 0xdc) << 16 | (uint32_t)(g + 0x20) << 8) | alpha;
		} else {
			int g = crt_rand() % 0x18;
			int r = crt_rand() % 16;
			int b = crt_rand() % 32;
			color = (uint32_t)(b + 0xdc) | alpha | ((uint32_t)(r + 0x18) << 16 | (uint32_t)(g + 0x60) << 8);
		}
		CGameParticles_SetParticle(tg, i, x, y, z, size, color, 2, 0.0f);
	}
}

/* the glows that run up over the top (particles 50..79 and the two top trails) */
static void MainFX_TopGlows(cFXElementsMentalControllerMainFX *self, float dt) {
	vec3_t tmp;

	for (int m = 0; m < 2; m++) {
		CGameTrail_Reset(&self->topTrails[m]);
	}
	for (int k = 0; k < FXMENTALMAINGLOWS; k++) {
		float p = self->glowSpeed[k] * dt + self->glowPhase[k];
		self->glowPhase[k] = p;
		if (p > 1.0f) {
			self->glowPhase[k] = 0.0f;
			float rr = FxElem_Rand(85.0, 75.0);
			const vec3_t *d = FX_RandomDir(&tmp);
			self->glowOffset[k].x = d->x * rr;
			self->glowOffset[k].y = d->y * 0.0f * rr;
			self->glowOffset[k].z = d->z * rr;
			CGameParticles_Kill(&self->tubeGlow, 50 + k);
			if (self->last >= 0 && self->last <= 2) {
				int g = crt_rand() % 32;
				int b = crt_rand() % 32;
				self->glowColor[k] = (uint32_t)(b + 0x18) | (uint32_t)(g + 0x18) << 8 | 0xff0000;
			}
		} else if (p > 0.0f) {
			const vec3_t *o = &self->glowOffset[k];
			float y = self->top.y + o->y + (p * 5300.0f - 800.0f);
			float x = o->x + self->top.x + 0.0f + 0.0f;
			float z = self->top.z + o->z + 0.0f + 0.0f;
			int a = 0;
			if (0.1f > p) {
				a = (int)((1.0f - p * 10.0f) * 128.0f);
			} else if (p > 0.9f) {
				a = (int)((p - 0.9f) * 10.0f * 128.0f);
			}
			a += 0x80;
			if (a > 0xff) {
				a = 0xff;
			}
			uint32_t color = (self->glowColor[k] & 0xffffff) | (uint32_t)a << 24;
			CGameTrail *t = &self->topTrails[self->glowSide[k]];
			CGameTrail_Add(t, (vec3_t){ x, y - 300.0f, z }, 90.0f, color);
			CGameTrail_Add(t, (vec3_t){ x, y + 300.0f, z }, 90.0f, color);
			float size = FxElem_Rand(1.0, 1.7999999523162842) * 90.0f;
			CGameParticles_SetParticle(&self->tubeGlow, 50 + k, x, y + 250.0f, z, size, self->glowColor[k], 5,
				0.0f);
		}
	}
}

void __thiscall cFXElementsMentalControllerMainFX_Update(cFXElementsMentalControllerMainFX *self, float dt) {
	CGameModel *model = &self->base.Mod;

	if (!self->controllers[0]) {
		return;
	}
	CHECK(CGameModel_IsLoaded(model));
	for (int i = 0; i < FXMENTALCONTROLLERMAINCOUNT; i++) {
		if (CGameModel_IsVisible(MainFX_Model(self, i))) {
			goto seen;
		}
	}
	{
		vec3_t cam = g_pWorld->camPos[g_pWorld->curCam];
		float dx = self->top.x - cam.x;
		float dz = self->top.z - cam.z;
		if (!(16000000.0f > dx * dx + 0.0f + dz * dz)) {
			CGameModel_Disable(model);
			if (self->nearCam) {
				cFXElementsMentalControllerMainFX_Clear(self);
				self->nearCam = 0;
			}
			return;
		}
	}
seen:
	CGameModel_Enable(model);
	dt = dt * self->scale;
	self->nearCam = 1;
	if (self->phase < 5) {
		for (int i = 0; i < FXMENTALMAINTRAILS; i++) {
			CGameTrail_Reset(&self->tubeTrails[i]);
			CGameTrail_Reset(&self->glowTrails[i]);
		}
		for (int j = 0; j < FXMENTALMAINFLUID; j++) {
			float ph = self->fluidPhase[j];
			if (ph > 1.0f || ph == -1.0f) {
				MainFX_FluidNew(self, j);
				continue;
			}
			ph = dt / self->fluidTime[j] + ph;
			self->fluidPhase[j] = ph;
			if (self->fluidActive[j]) {
				MainFX_FluidMove(self, j, ph);
			}
		}
	}
	if (self->phase >= 2 && self->phase < 5) {
		MainFX_TubeGlow(self, dt);
	}
	if (self->phase >= 2 && self->phase < 5) {
		MainFX_TopGlows(self, dt);
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
