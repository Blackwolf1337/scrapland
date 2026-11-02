/*
 * FXElements.cpp of the game: the fire drops, lens flares and lightnings of the levels, see
 * include/scrap/fxelements.h.
 */
#include <string.h>

#include "scrap/fxelements.h"
#include "scrap/fxweapons.h"
#include "scrap/fxmisc.h"
#include "scrap/mathlib.h"
#include "scrap/scrap.h"
#include "scrap/sound.h"
#include "scrap/staticinit.h"

#define CGAMEPARTICLES_CTOR 0x21d1b0
#define CGAMETRAIL_CTOR 0x21ddb0
#define CGAMETRAIL_DTOR 0x21de40

/* the colour ramps of the fire drops, 9 colours each: 0 and 1 fire, 2 smoke, 3 sparks */

/* ---- construction ---- */

cFXElementsFireDrop *cFXElementsFireDrop_ctor(cFXElementsFireDrop *self, const char *name,
	const vec3_t *pos, const vec3_t *rot) {
	cMapElement_ctor(&self->base, name, pos, rot);
	self->base.vftable = CFXELEMENTSFIREDROP_VFTABLE;
	CGameParticles_ctor(&self->drops);
	return self;
}

cFXElementsLensFlare *cFXElementsLensFlare_ctor(cFXElementsLensFlare *self, const char *name,
	const vec3_t *pos, const vec3_t *rot) {
	cMapElement_ctor(&self->base, name, pos, rot);
	self->base.vftable = CFXELEMENTSLENSFLARE_VFTABLE;
	CGameLensFlare_ctor(self->flare);
	CGameLensFlare_SetType(self->flare, 0);
	CGameLensFlare_SetPos(self->flare, *pos);
	CGameLensFlare_SetIntensity(self->flare, 0.65f);
	CGameLensFlare_SetRelative(self->flare, 1);
	CGameLensFlare_SetTraceInterval(self->flare, 0.3f);
	CGameLensFlare_SetOn(self->flare, 1);
	return self;
}

cFXElementsLighting *cFXElementsLighting_ctor(cFXElementsLighting *self, const char *name,
	const vec3_t *pos, const vec3_t *rot) {
	cMapElement_ctor(&self->base, name, pos, rot);
	self->base.vftable = CFXELEMENTSLIGHTING_VFTABLE;
	CGameParticles_ctor(&self->glows);
	crt_eh_vector_ctor(self->trails, sizeof(CGameTrail), FXLIGHTINGBOLTS, (void *)CGameTrail_ctor,
		(void *)CGameTrail_dtor);
	return self;
}

/* the particle members are released twice, by the class and by the member destructor */
void cFXElementsFireDrop_dtor(cFXElementsFireDrop *self) {
	self->base.vftable = CFXELEMENTSFIREDROP_VFTABLE;
	CGameParticles_Release(&self->drops);
	CGameParticles_Release(&self->drops);
	cMapElement_dtor(&self->base);
}

void cFXElementsLensFlare_dtor(cFXElementsLensFlare *self) {
	self->base.vftable = CFXELEMENTSLENSFLARE_VFTABLE;
	CGameLensFlare_dtor(self->flare);
	cMapElement_dtor(&self->base);
}

void cFXElementsLighting_dtor(cFXElementsLighting *self) {
	self->base.vftable = CFXELEMENTSLIGHTING_VFTABLE;
	for (int i = 0; i < FXLIGHTINGBOLTS; i++) {
		CGameTrail_Clear(&self->trails[i]);
	}
	CGameParticles_Release(&self->glows);
	crt_eh_vector_dtor(self->trails, sizeof(CGameTrail), FXLIGHTINGBOLTS, (void *)CGameTrail_dtor);
	CGameParticles_Release(&self->glows);
	cMapElement_dtor(&self->base);
}

cMapElement *cFXElementsFireDrop_Delete(cMapElement *base, byte flags) {
	cFXElementsFireDrop *self = (cFXElementsFireDrop *)base;
	cFXElementsFireDrop_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return base;
}

cMapElement *cFXElementsLensFlare_Delete(cMapElement *base, byte flags) {
	cFXElementsLensFlare *self = (cFXElementsLensFlare *)base;
	cFXElementsLensFlare_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return base;
}

cMapElement *cFXElementsLighting_Delete(cMapElement *base, byte flags) {
	cFXElementsLighting *self = (cFXElementsLighting *)base;
	cFXElementsLighting_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return base;
}

void cFXElementsFireDrop_Release(cFXElementsFireDrop *self) {
	CGameParticles_Release(&self->drops);
}

void cFXElementsLensFlare_Release(cFXElementsLensFlare *self) {
	(void)self;
}

void cFXElementsLighting_Release(cFXElementsLighting *self) {
	for (int i = 0; i < FXLIGHTINGBOLTS; i++) {
		CGameTrail_Clear(&self->trails[i]);
	}
	CGameParticles_Release(&self->glows);
}

/* ---- cFXElementsFireDrop ---- */

byte cFXElementsFireDrop_Init(cMapElement *elem) {
	cFXElementsFireDrop *self = (cFXElementsFireDrop *)elem;
	CGameModel *model = &self->base.Mod;
	vec3_t tmp;

	cMapElement_Init(&self->base);
	CHECK(CGameModel_IsLoaded(model));
	self->pos = *CGameModel_GetPos(model, &tmp);
	self->dcRoot = CGameModel_FindRenderNode(model, "DC_Root");
	CHECK(self->dcRoot != -1);
	CGameParticles_Attach(&self->drops, model, self->dcRoot);
	CHECK(self->drops.ParticleSystem);
	self->scale = CGameModel_NodeIniFloat(model, self->dcRoot, "FXFireDrop", "Scale", 6.0f);
	self->size = CGameModel_NodeIniFloat(model, self->dcRoot, "FXFireDrop", "Size", 9.5f);
	self->speed = CGameModel_NodeIniFloat(model, self->dcRoot, "FXFireDrop", "Speed", 1150.0f);
	self->area = CGameModel_NodeIniFloat(model, self->dcRoot, "FXFireDrop", "Area", 120.0f);
	self->life = CGameModel_NodeIniFloat(model, self->dcRoot, "FXFireDrop", "Life", 1.9f);
	self->burstTime = 0.0f;
	self->sleepTime = 0.0f;
	self->emitTime[0] = 0.0f;
	self->emitTime[1] = 0.0f;
	self->emitTime[2] = 0.0f;
	return 1;
}

enum { FIREDROP_FIRE, FIREDROP_FIRE_OR_SPARK, FIREDROP_SMOKE };

static void FireDrop_SetPos(CGameParticles *p, float x, float y, float z) {
	p->initPosMin.x = x;
	p->initPosMin.y = y;
	p->initPosMin.z = z;
	p->initPosMax = p->initPosMin;
}

static void FireDrop_SetVel(CGameParticles *p, float x, float y, float z) {
	p->initVelMin.x = x;
	p->initVelMin.y = y;
	p->initVelMin.z = z;
	p->initVelMax = p->initVelMin;
}

/* the acceleration straight up or down */
static void FireDrop_SetAcc(CGameParticles *p, float y) {
	p->initAccMin.x = 0.0f;
	p->initAccMin.y = y;
	p->initAccMin.z = 0.0f;
	p->initAccMax = p->initAccMin;
}

static void FireDrop_SetSprite(CGameParticles *p, byte sprite) {
	p->spriteIni = sprite;
	p->spriteEnd = sprite;
}

uint32_t g_FireDropRamps[4][9] = {
	{ 0xff3b00, 0xff4302, 0xff5203, 0xff5c05, 0x20ff5a09, 0x80c04b04, 0xc4601901, 0xff200800 },
	{ 0x40fffaf0, 0xffeb80, 0xf3cd50, 0xcd9136, 0x20a44a18, 0x80871005, 0xc4800000, 0xff000000 },
	{ 0xff000000, 0x80824c0e, 0x40ef641e, 0x20ff732e, 0x40b47848, 0x80876d2e, 0xc45e401a, 0xff000000 },
	{ 0xff4b20, 0xff3e10, 0xff2d08, 0xff2000, 0x20c01400, 0x80800a00, 0xc4400500, 0xff000000 },
};

/* the colour ramp picked after the ramp is switched on, the parameters applied, one particle emitted */
static void FireDrop_Emit(cFXElementsFireDrop *self, int kind, float t) {
	CGameParticles *p = &self->drops;
	uint32_t *ramp;

	CGameParticles_SetColorRampOn(p, 1);
	if (kind == FIREDROP_SMOKE) {
		ramp = g_FireDropRamps[2];
	} else if (kind == FIREDROP_FIRE_OR_SPARK && crt_rand() % 3 == 0) {
		ramp = g_FireDropRamps[3];
	} else {
		ramp = g_FireDropRamps[crt_rand() & 1];
	}
	CGameParticles_SetColorRamp(p, ramp);
	CGameParticles_Apply(p);
	CGameParticles_EmitDt(p, 1, 0, t);
}

/* the first drop of each step of the burst start */
static void FireDrop_StartA(cFXElementsFireDrop *self, float t) {
	CGameParticles *p = &self->drops;
	vec3_t tmp;

	p->birthRate = 0;
	float a = FxElem_Rand(self->area, 0.0);
	float b = FxElem_Rand(self->area * -0.5f, 0.0);
	float c = FxElem_Rand(self->area, 0.0);
	const vec3_t *d = FX_RandomDir(&tmp);
	float s = self->scale;
	FireDrop_SetPos(p, d->x * c * s, d->y * b * s, d->z * a * s);
	FireDrop_SetVel(p, self->scale * 0.0f, self->speed * self->scale, self->scale * 0.0f);
	FireDrop_SetAcc(p, p->initVelMin.y * -0.25f);
	p->lifeTime = FxElem_Rand(0.06999999284744263, 0.5099999904632568) * self->life;
	p->sizeIni = self->size * FxElem_Rand(5.0, 10.0) * self->scale;
	p->sizeEnd = self->size * FxElem_Rand(5.0, 55.0) * self->scale;
	p->sizeLife = FxElem_Rand(0.02000001072883606, 0.3199999928474426);
	float rot = FxElem_Rand(1.5707963705062866, -0.7853981852531433);
	p->rotIni = rot;
	p->rotEnd = rot;
	p->rotVar = FxElem_Rand(2.094395160675049, -1.0471975803375244);
	FireDrop_SetSprite(p, (byte)((crt_rand() & 1) + 0x1c));
	FireDrop_Emit(self, FIREDROP_FIRE, t * 0.5f);
}

/* the second drop of each step of the burst start */
static void FireDrop_StartB(cFXElementsFireDrop *self, float t) {
	CGameParticles *p = &self->drops;
	vec3_t tmp;

	p->birthRate = 0;
	float a = FxElem_Rand(self->area, 0.0);
	float b = FxElem_Rand(self->area * -0.5f, 0.0);
	float c = FxElem_Rand(self->area, 0.0);
	const vec3_t *d = FX_RandomDir(&tmp);
	float s = self->scale;
	FireDrop_SetPos(p, d->x * c * s, d->y * b * s, d->z * a * s);
	FireDrop_SetVel(p, self->scale * 0.0f, self->speed * self->scale, self->scale * 0.0f);
	FireDrop_SetAcc(p, p->initVelMin.y * -0.25f);
	p->lifeTime = FxElem_Rand(0.06000000238418579, 0.6200000047683716) * self->life;
	p->sizeIni = FxElem_Rand(5.0, 15.0) * self->size * self->scale;
	p->sizeEnd = FxElem_Rand(10.0, 55.0) * self->size * self->scale;
	p->sizeLife = FxElem_Rand(0.02000001072883606, 0.3199999928474426);
	float rot = FxElem_Rand(1.5707963705062866, -0.7853981852531433);
	p->rotIni = rot;
	p->rotEnd = rot;
	p->rotVar = FxElem_Rand(2.094395160675049, -1.0471975803375244);
	FireDrop_SetSprite(p, 0x37);
	FireDrop_Emit(self, FIREDROP_FIRE, t);
}

/* the fire of the burst, while it lasts more than 0.08 seconds */
static void FireDrop_Fire(cFXElementsFireDrop *self, float t) {
	CGameParticles *p = &self->drops;
	vec3_t tmp;

	p->birthRate = 0;
	float a = FxElem_Rand(self->area, 0.0);
	float b = FxElem_Rand(0.30000001192092896, -0.20000000298023224);
	float c = FxElem_Rand(self->area, 0.0);
	b = self->area * b;
	const vec3_t *d = FX_RandomDir(&tmp);
	float s = self->scale;
	FireDrop_SetPos(p, c * d->x * s, d->y * b * s, d->z * a * s);
	FireDrop_SetVel(p, self->scale * 0.0f, self->speed * self->scale, self->scale * 0.0f);
	FireDrop_SetAcc(p, p->initVelMin.y * -0.25f);
	p->lifeTime = FxElem_Rand(0.12000000476837158, 0.7400000095367432) * self->life;
	p->sizeIni = FxElem_Rand(4.0, 14.0) * self->size * self->scale;
	p->sizeEnd = FxElem_Rand(4.0, 42.0) * self->size * self->scale;
	p->sizeLife = FxElem_Rand(0.02000001072883606, 0.1599999964237213);
	float rot = FxElem_Rand(0.7853981852531433, -0.39269909262657166);
	p->rotIni = rot;
	p->rotEnd = rot;
	p->rotVar = FxElem_Rand(2.094395160675049, -1.0471975803375244);
	FireDrop_SetSprite(p, 0x37);
	FireDrop_Emit(self, FIREDROP_FIRE_OR_SPARK, t);
}

/* the slower drops, while the burst lasts more than 0.1 seconds */
static void FireDrop_Drip(cFXElementsFireDrop *self, float t) {
	CGameParticles *p = &self->drops;
	vec3_t tmp;

	p->birthRate = 0;
	float a = FxElem_Rand(self->area * 0.7f, 0.0);
	float b = FxElem_Rand(0.20000000298023224, -0.10000000149011612);
	float c = FxElem_Rand(self->area * 0.7f, 0.0);
	b = b * self->area;
	const vec3_t *d = FX_RandomDir(&tmp);
	float s = self->scale;
	FireDrop_SetPos(p, c * d->x * s, d->y * b * s, d->z * a * s);
	float v = FxElem_Rand(0.20000004768371582, 0.8999999761581421);
	FireDrop_SetVel(p, self->scale * 0.0f, self->speed * v * self->scale, self->scale * 0.0f);
	FireDrop_SetAcc(p, p->initVelMin.y * -0.25f);
	p->lifeTime = FxElem_Rand(0.10000002384185791, 0.6399999856948853) * self->life;
	p->sizeIni = FxElem_Rand(4.0, 14.0) * self->size * self->scale;
	p->sizeEnd = FxElem_Rand(6.0, 40.0) * self->size * self->scale;
	p->sizeLife = FxElem_Rand(0.019999995827674866, 0.20000000298023224);
	float rot = FxElem_Rand(3.1415927410125732, -1.5707963705062866);
	p->rotIni = rot;
	p->rotEnd = rot;
	p->rotVar = FxElem_Rand(2.094395160675049, -1.0471975803375244);
	FireDrop_SetSprite(p, (byte)((crt_rand() & 1) + 0x1c));
	FireDrop_Emit(self, FIREDROP_FIRE_OR_SPARK, t);
}

/* the smoke rising, while the burst lasts more than 1.1 seconds */
static void FireDrop_Smoke(cFXElementsFireDrop *self, float t) {
	CGameParticles *p = &self->drops;
	vec3_t tmp;

	p->birthRate = 0;
	float a = FxElem_Rand(self->area / 5.0f, 0.0);
	float b = FxElem_Rand(self->area * 1.9f, 0.0);
	float c = FxElem_Rand(self->area * 1.9f, 0.0);
	const vec3_t *d = FX_RandomDir(&tmp);
	float s = self->scale;
	FireDrop_SetPos(p, s * (c * d->x + 0.0f), s * (d->y * 0.0f + a), s * (d->z * b + 0.0f));
	float vz = FxElem_Rand(60.0, -30.0);
	float hi = self->speed * 1.2f;
	float lo = self->speed * 0.8f;
	float vy = FxElem_Rand(hi - lo, lo);
	float vx = FxElem_Rand(60.0, -30.0);
	s = self->scale;
	FireDrop_SetVel(p, s * vx, s * vy, s * vz);
	float r = FxElem_Rand(0.019999995827674866, 0.14000000059604645);
	FireDrop_SetAcc(p, -(r * p->initVelMin.y));
	p->lifeTime = FxElem_Rand(0.10000002384185791, 1.149999976158142) * self->life;
	p->sizeIni = FxElem_Rand(10.0, 55.0) * self->size * self->scale;
	p->sizeEnd = FxElem_Rand(10.0, 55.0) * self->size * self->scale;
	p->sizeLife = FxElem_Rand(0.2200000286102295, 0.5199999809265137);
	float rot = FxElem_Rand(6.2831854820251465, 0.0);
	p->rotIni = rot;
	p->rotEnd = rot;
	p->rotVar = FxElem_Rand(1.0471975803375244, -0.5235987901687622);
	FireDrop_SetSprite(p, 0x33);
	FireDrop_Emit(self, FIREDROP_SMOKE, t);
}

/* the drops due since the last ones, interval apart; emitTime the time since then */
static void FireDrop_EmitDue(cFXElementsFireDrop *self, float *emitTime, float interval,
	void (*emit)(cFXElementsFireDrop *, float)) {
	if (*emitTime > interval) {
		if (*emitTime > 0.0f) {
			float t = 0.0f;
			do {
				emit(self, t);
				t += interval;
			} while (*emitTime > t);
		}
		*emitTime = 0.0f;
	}
}

/* near the camera: a burst of drops (more the nearer) with the sound, then a pause */
void cFXElementsFireDrop_Update(cMapElement *elem, float dt) {
	cFXElementsFireDrop *self = (cFXElementsFireDrop *)elem;
	CGameModel *model = &self->base.Mod;
	vec3_t tmp;

	cMapElement_Update(&self->base, dt);
	CHECK(CGameModel_IsLoaded(model));
	if (CGameModel_IsVisible(model)) {
		vec3_t cam = g_pWorld->camPos[g_pWorld->curCam];
		const vec3_t *pos = CGameModel_GetPos(model, &tmp);
		float dy = pos->y - cam.y;
		float dx = pos->x - cam.x;
		float dz = pos->z - cam.z;
		float d2 = dy * dy + dx * dx + dz * dz;
		float max2 = g_FireDropMaxDist * g_FireDropMaxDist;
		if (!(d2 > max2)) {
			if (!self->nearCam) {
				self->nearCam = 1;
			}
			float rate = d2 / max2 * g_FireDropRateAtt + 1.0f;
			float burst = self->burstTime - dt;
			self->burstTime = burst;
			if (!(burst <= 0.0f)) {
				if (burst > 0.08f) {
					self->emitTime[0] = self->emitTime[0] + dt;
					FireDrop_EmitDue(self, &self->emitTime[0], rate * 0.05f, FireDrop_Fire);
				}
				if (self->burstTime > 0.1f) {
					self->emitTime[1] = dt + self->emitTime[1];
					FireDrop_EmitDue(self, &self->emitTime[1], rate * 0.08f, FireDrop_Drip);
				}
				if (self->burstTime > 1.1f) {
					float interval = rate * 0.09f;
					self->emitTime[2] = self->emitTime[2] + dt;
					FireDrop_EmitDue(self, &self->emitTime[2], interval, FireDrop_Smoke);
				}
				return;
			}
			float sleep = self->sleepTime - dt;
			self->sleepTime = sleep;
			if (!(sleep <= 0.0f)) {
				return;
			}
			self->burstTime = FxElem_Rand(2.0, 0.8999999761581421);
			self->sleepTime = FxElem_Rand(3.299999952316284, 0.20000000298023224);
			float t = 0.0f;
			do {
				FireDrop_StartA(self, t);
				FireDrop_StartB(self, t);
				t += 0.05f;
			} while (0.2f > t);
			float att = g_FireDropSoundAttEnd;
			if (!(att * att >= d2)) {
				return;
			}
			cSoundDesc desc;
			cSoundDesc_Init(&desc);
			desc.param1 = g_FireDropSoundAttIni;
			desc.param2 = g_FireDropSoundAttEnd;
			desc.param3 = 0.0f;
			cSoundDesc_SetPos(&desc, CGameModel_GetPos(model, &tmp));
			desc.volume += Snd_Volume(g_FireDropSoundVol);
			g_pSound->vftable->PlaySound(g_pSound, g_FireDropSound, &desc, NULL);
			return;
		}
	}
	if (self->nearCam) {
		self->nearCam = 0;
	}
}

/* ---- cFXElementsLensFlare ---- */

byte cFXElementsLensFlare_Init(cFXElementsLensFlare *self) {
	(void)self;
	return 1;
}

void cFXElementsLensFlare_Update(cFXElementsLensFlare *self, float dt) {
	(void)self;
	(void)dt;
}

/* ---- cFXElementsLighting ---- */

byte cFXElementsLighting_Init(cMapElement *elem) {
	cFXElementsLighting *self = (cFXElementsLighting *)elem;
	CGameModel *model = &self->base.Mod;
	char buf[0x100];
	vec3_t tmp;

	cMapElement_Init(&self->base);
	self->numPts = 6;
	for (int i = 0; i < FXLIGHTINGBOLTS; i++) {
		CGameTrail *trail = &self->trails[i];
		CGameTrail_SetMaxTrails(trail, 0x14);
		CGameTrail_SetSmooth(trail, 1);
		CGameTrail_SetTexture(trail, 3);
		CGameTrail_SetPremul(trail, 0);
		self->life[i] = -1.0f;
		self->sleep[i] = -1.0f;
		self->pair[i][0] = -1;
		self->pair[i][1] = -1;
	}
	/* the original cleared 16 bytes for each pair, the last store reached the first 8 glow flags */
	memset(self->glow, 0xff, 8);
	self->visible = 0;
	self->time = 0.0f;
	CHECK(CGameModel_IsLoaded(model));
	self->pos = *CGameModel_GetPos(model, &tmp);
	CGameModel_AnimInfo(model, 0, &self->animDur, &self->animFps);
	self->dcRoot = CGameModel_FindRenderNode(model, "DC_Root");
	CHECK(self->dcRoot != -1);
	CGameParticles_Attach(&self->glows, model, self->dcRoot);
	CHECK(self->glows.ParticleSystem);
	for (int i = 0; i < FXLIGHTINGBOLTS; i++) {
		int j;
		for (j = 0; j < FXLIGHTINGLINKS; j++) {
			Str_PrintfQuiet(buf, 0xff, "DF_Link_%d%c", i + 1, j + 'A');
			int node = CGameModel_FindRenderNode(model, buf);
			self->link[i][j] = node;
			if (node == -1) {
				break;
			}
			self->glow[i][j] = CGameModel_NodeIniInt(model, node, "Lighting", "Glow", 1) != 0;
			self->glowColor[i][j] = CGameModel_NodeIniColor(model, node, "Lighting", "GlowColor", 0x20c0c0ff);
			self->glowSizeMin[i][j] = CGameModel_NodeIniFloat(model, node, "Lighting", "GlowSizeMin", 1150.0f);
			self->glowSizeMax[i][j] = CGameModel_NodeIniFloat(model, node, "Lighting", "GlowSizeMax", 1250.0f);
			self->pointColor[i][j] = CGameModel_NodeIniColor(model, node, "Lighting", "PointColor", 0x20c0c0ff);
			self->pointSizeMin[i][j] = CGameModel_NodeIniFloat(model, node, "Lighting", "PointSizeMin", 200.0f);
			self->pointSizeMax[i][j] = CGameModel_NodeIniFloat(model, node, "Lighting", "PointSizeMax", 220.0f);
		}
		self->linkCount[i] = j;
		int node = self->link[i][0];
		if (node == -1) {
			continue;
		}
		self->sizeMin[i] = CGameModel_NodeIniFloat(model, node, "Lighting", "SizeMin", 165.0f);
		self->sizeMax[i] = CGameModel_NodeIniFloat(model, node, "Lighting", "SizeMax", 195.0f);
		self->sizeVar[i] = CGameModel_NodeIniFloat(model, node, "Lighting", "SizeVar", 80.0f);
		self->lifeTimeMin[i] = CGameModel_NodeIniFloat(model, node, "Lighting", "LifeTimeMin", 0.08f);
		self->lifeTimeMax[i] = CGameModel_NodeIniFloat(model, node, "Lighting", "LifeTimeMax", 0.55f);
		self->sleepTimeMin[i] = CGameModel_NodeIniFloat(model, node, "Lighting", "SleepTimeMin", 0.02f);
		self->sleepTimeMax[i] = CGameModel_NodeIniFloat(model, node, "Lighting", "SleepTimeMax", 0.4f);
		self->tensorMin[i] = CGameModel_NodeIniFloat(model, node, "Lighting", "TensorMin", 1800.0f);
		self->tensorMax[i] = CGameModel_NodeIniFloat(model, node, "Lighting", "TensorMax", 3500.0f);
		self->randVarMin[i] = CGameModel_NodeIniFloat(model, node, "Lighting", "RandVarMin", 0.12f);
		self->randVarMax[i] = CGameModel_NodeIniFloat(model, node, "Lighting", "RandVarMax", 0.25f);
		self->anmFollowMin[i] = CGameModel_NodeIniFloat(model, node, "Lighting", "AnmFollowMin", 0.06f);
		self->anmFollowMax[i] = CGameModel_NodeIniFloat(model, node, "Lighting", "AnmFollowMax", 0.09f);
		self->anmLoopReset[i] = CGameModel_NodeIniInt(model, node, "Lighting", "AnmLoopReset", 0) != 0;
		self->saturate[i] = CGameModel_NodeIniInt(model, node, "Lighting", "Saturate4X", 0) != 0;
		self->offsetSpeed[i] = CGameModel_NodeIniFloat(model, node, "Lighting", "OffsetSpeed", 1.0f);
		CGameTrail_SetTexture(&self->trails[i], self->sizeMax[i] > 150.0f ? 8 : 9);
		CGameTrail_SetPremul(&self->trails[i], self->saturate[i]);
	}
	return 1;
}

/* a value between lo and hi */
static inline float Lighting_Rand(float lo, float hi) {
	return FxElem_Rand(hi - lo, lo);
}

static inline void Lighting_NodePos(const matrix_t *m, vec3_t *out) {
	out->x = m->m[12];
	out->y = m->m[13];
	out->z = m->m[14];
}

/* the line from pts[0] to pts[n - 1] in n - 1 steps of d, the inner points from the line plus jitter * a random
 * direction */
static void Lighting_SetLine(cFXElementsLighting *self, vec3_t *pts, float dx, float dy, float dz, float jitter) {
	vec3_t cur = {dx, dy, dz};
	vec3_t tmp;

	for (int k = 1; k < self->numPts - 1; k++) {
		pts[k].x = cur.x + pts[0].x;
		pts[k].y = cur.y + pts[0].y;
		pts[k].z = cur.z + pts[0].z;
		cur.x = cur.x + dx;
		cur.z = cur.z + dz;
		cur.y = cur.y + dy;
		const vec3_t *r = FX_RandomDir(&tmp);
		pts[k].x = jitter * r->x + pts[k].x;
		pts[k].y = r->y * jitter + pts[k].y;
		pts[k].z = r->z * jitter + pts[k].z;
	}
}

/* the pause is over: two links of the bolt, its times and sizes, its points on the line between them */
static void Lighting_Start(cFXElementsLighting *self, int i) {
	CGameModel *model = &self->base.Mod;
	vec3_t *pts = self->pts[i];
	matrix_t mat;
	const matrix_t *m;
	int from = 0, to = 1;

	if (self->linkCount[i] > 2) {
		from = crt_rand() % self->linkCount[i];
		to = crt_rand() % self->linkCount[i];
		if (from == to) {
			for (int k = 0; k < 2; k++) {
				to = crt_rand() % self->linkCount[i];
				if (to != from) {
					break;
				}
			}
			if (from == to) {
				crt_rand();
				to--;
				if (to < 0) {
					to = self->linkCount[i] - 1;
				} else if (to >= self->linkCount[i]) {
					to = 0;
				}
			}
		}
	}
	self->pair[i][0] = from;
	self->pair[i][1] = to;
	self->life[i] = Lighting_Rand(self->lifeTimeMin[i], self->lifeTimeMax[i]);
	self->sleep[i] = Lighting_Rand(self->sleepTimeMin[i], self->sleepTimeMax[i]);
	self->glowSize[i][0] = Lighting_Rand(self->glowSizeMin[i][from], self->glowSizeMax[i][from]);
	self->glowSize[i][1] = Lighting_Rand(self->glowSizeMin[i][to], self->glowSizeMax[i][to]);
	self->size[i] = Lighting_Rand(self->sizeMin[i], self->sizeMax[i]);
	self->pointSize[i][0] = Lighting_Rand(self->pointSizeMin[i][from], self->pointSizeMax[i][from]);
	self->pointSize[i][1] = Lighting_Rand(self->pointSizeMin[i][to], self->pointSizeMax[i][to]);
	self->anmFollow[i] = Lighting_Rand(self->anmFollowMin[i], self->anmFollowMax[i]);
	self->offset[i] = FxElem_Rand(8.0, -4.0);

	vec3_t p0, p1;
	m = CGameModel_NodeMatrix(model, &mat, self->link[i][from], 1, NULL, 0);
	Lighting_NodePos(m, &p0);
	m = CGameModel_NodeMatrix(model, &mat, self->link[i][to], 1, NULL, 0);
	Lighting_NodePos(m, &p1);
	pts[0] = p0;
	pts[self->numPts - 1] = p1;
	int n = self->numPts;
	float step = 1.0f / (float)(n - 1);
	float dx = (pts[n - 1].x - pts[0].x) * step;
	float dy = (pts[n - 1].y - pts[0].y) * step;
	float dz = (pts[n - 1].z - pts[0].z) * step;
	float len = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	if (n - 1 > 1) {
		Lighting_SetLine(self, pts, dx, dy, dz, len * 0.1f);
	}
}

/* the bolt drawn: its points pulled to the line between the links (they follow the animation), shaken, the trail
 * along the Bezier curve through them, the glows at both ends */
static void Lighting_Draw(cFXElementsLighting *self, int i, float dt, int from, int to) {
	CGameModel *model = &self->base.Mod;
	CGameTrail *trail = &self->trails[i];
	vec3_t *pts = self->pts[i];
	vec3_t coefs[GAMETRAIL_MAX_BEZIER];
	matrix_t mat0, mat1;
	vec3_t p0, p1, tmp;
	byte vis0 = 0, vis1 = 0;

	Lighting_NodePos(CGameModel_NodeMatrix(model, &mat0, self->link[i][from], 1, &vis0, 0), &p0);
	Lighting_NodePos(CGameModel_NodeMatrix(model, &mat1, self->link[i][to], 1, &vis1, 0), &p1);
	float fade = 0.0f;
	if (0.05f > self->life[i]) {
		float f = 1.0f - (self->life[i] - dt) / 0.05f;
		fade = 0.0f > f ? 0.0f : Float_MinSS(1.0f, f);
	}
	byte vis = vis0 < vis1 ? vis0 : vis1;
	CGameTrail_SetTransp(trail, (byte)(int)((float)(0xff - vis) / 255.0f * fade * 255.0f));

	pts[0] = p0;
	pts[self->numPts - 1] = p1;
	int n = self->numPts;
	float step = 1.0f / (float)(n - 1);
	float dx = (pts[n - 1].x - pts[0].x) * step;
	float dy = (pts[n - 1].y - pts[0].y) * step;
	float dz = (pts[n - 1].z - pts[0].z) * step;
	for (int k = 1; k < self->numPts - 1; k++) {
		float fk = (float)k;
		float tx = dx * fk + pts[0].x;
		float ty = dy * fk + pts[0].y;
		float tz = dz * fk + pts[0].z;
		float follow = self->anmFollow[i];
		pts[k].x = pts[k].x - (pts[k].x - tx) * follow;
		pts[k].y = pts[k].y - (pts[k].y - ty) * follow;
		pts[k].z = pts[k].z - (pts[k].z - tz) * follow;
		float shake = Lighting_Rand(self->tensorMin[i], self->tensorMax[i]) * dt;
		const vec3_t *r = FX_RandomDir(&tmp);
		pts[k].x = shake * r->x + pts[k].x;
		pts[k].z = r->z * shake + pts[k].z;
		pts[k].y = r->y * shake + pts[k].y;
	}

	int maxTrails = CGameTrail_GetMaxTrails(trail);
	float ey = p0.y - p1.y;
	float ex = p0.x - p1.x;
	float ez = p0.z - p1.z;
	float dist = (float)__builtin_sqrt((double)(ey * ey + ex * ex + ez * ez));
	int segs = self->numPts;
	int want = (int)(dist / self->size[i]);
	if (want >= segs) {
		segs = want > maxTrails ? maxTrails : want;
	}
	CGameTrail_SetSegments(trail, (byte)segs);
	CGameTrail_Reset(trail);

	/* the colour from the one of the first link to the one of the second */
	uint32_t c0 = self->pointColor[i][from];
	uint32_t c1 = self->pointColor[i][to];
	int r0 = c0 >> 16 & 0xff, g0 = c0 >> 8 & 0xff, b0 = c0 & 0xff, a0 = c0 >> 24;
	int r1 = c1 >> 16 & 0xff, g1 = c1 >> 8 & 0xff, b1 = c1 & 0xff, a1 = c1 >> 24;
	float fsegs = (float)segs;
	float sr = (float)(r1 - r0) / fsegs;
	float sg = (float)(g1 - g0) / fsegs;
	float sb = (float)(b1 - b0) / fsegs;
	float sa = (float)(a1 - a0) / fsegs;
	CGameTrail_BezierCoefs(trail, self->numPts, pts, coefs);
	CGameTrail_SetUV(trail, self->offset[i] + 16.0f, 0.0f);
	if (self->offset[i] > 0.0f) {
		float r = FxElem_Rand(3.5, 0.5);
		self->offset[i] = dt * r * self->offsetSpeed[i] + self->offset[i];
		CGameTrail_SetULen(trail, 8.0f, 1.0f);
	} else {
		float r = FxElem_Rand(3.5, 0.5);
		self->offset[i] = self->offset[i] - dt * r * self->offsetSpeed[i];
		CGameTrail_SetULen(trail, -8.0f, 1.0f);
	}

	vec3_t prev = {0.0f, 0.0f, 0.0f};
	int last = segs - 1;
	int segs2 = segs - 2;
	float fsegs2 = (float)segs2;
	for (int k = 0; k < last; k++) {
		vec3_t pt;
		CGameTrail_BezierPoint(trail, self->numPts, coefs, &pt, (float)k / fsegs2);
		if (k > 0 && k < segs2) {
			/* shaken by the length of the step (the bigger of x and y, of y and z when y is bigger) */
			float ax = (float)__builtin_fabs((double)(prev.x - pt.x));
			float ay = (float)__builtin_fabs((double)(prev.y - pt.y));
			float big = ax;
			if (!(ax > ay)) {
				big = Float_MaxSS(ay, (float)__builtin_fabs((double)(prev.z - pt.z)));
			}
			float shake = Lighting_Rand(self->randVarMin[i] * big, self->randVarMax[i] * big);
			const vec3_t *r = FX_RandomDir(&tmp);
			pt.x = shake * r->x + pt.x;
			pt.y = r->y * shake + pt.y;
			pt.z = r->z * shake + pt.z;
		}
		float width;
		if (k == 0) {
			width = self->pointSize[i][0];
		} else if (k == segs2) {
			width = self->pointSize[i][1];
		} else {
			float var = self->sizeVar[i];
			width = Lighting_Rand(-var, var) + self->size[i];
		}
		float fk = (float)k;
		uint32_t color = (uint32_t)(int)(sa * fk + (float)a0);
		color = color << 8 | (uint32_t)(int)(sr * fk + (float)r0);
		color = color << 8 | (uint32_t)(int)(sg * fk + (float)g0);
		color = color << 8 | (uint32_t)(int)(sb * fk + (float)b0);
		if (k <= 1 || k == segs2) {
			uint32_t a = (color >> 24) + 0x20;
			if (a > 0xff) {
				a = 0xff;
			}
			color = (color & 0xffffff) | a << 24;
		}
		CGameTrail_Add(trail, pt, width, color);
		prev = pt;
	}

	uint32_t alpha0 = (uint32_t)(int)(fade * 255.0f) << 24;
	uint32_t alpha1 = (uint32_t)(0x80 - (int)(fade * -127.0f)) << 24;
	if (self->glow[i][from]) {
		uint32_t c = self->glowColor[i][from] & 0xffffff;
		CGameParticles_SetParticle(&self->glows, 4 * i, p0.x, p0.y, p0.z, self->glowSize[i][0] * 0.75f, alpha0 | c, 1,
			0.0f);
		CGameParticles_SetParticle(&self->glows, 4 * i + 1, p0.x, p0.y, p0.z,
			self->glowSize[i][0] + self->glowSize[i][0], alpha1 | c, 1, 0.0f);
	}
	if (self->glow[i][to]) {
		uint32_t c = self->glowColor[i][to] & 0xffffff;
		CGameParticles_SetParticle(&self->glows, 4 * i + 2, p1.x, p1.y, p1.z, self->glowSize[i][1] * 0.75f,
			alpha0 | c, 1, 0.0f);
		CGameParticles_SetParticle(&self->glows, 4 * i + 3, p1.x, p1.y, p1.z,
			self->glowSize[i][1] + self->glowSize[i][1], alpha1 | c, 1, 0.0f);
	}
}

/* every bolt with two links or more: drawn while its life lasts, then cleared until its pause is over */
void cFXElementsLighting_Update(cMapElement *elem, float dt) {
	cFXElementsLighting *self = (cFXElementsLighting *)elem;
	CGameModel *model = &self->base.Mod;

	cMapElement_Update(&self->base, dt);
	CHECK(CGameModel_IsLoaded(model));
	if (!CGameModel_IsVisible(model)) {
		if (self->visible) {
			self->visible = 0;
			for (int i = 0; i < FXLIGHTINGBOLTS; i++) {
				CGameTrail_Clear(&self->trails[i]);
			}
		}
		return;
	}
	self->visible = 1;
	self->prevTime = self->time;
	CGameModel_ChanTime(model, 0, &self->time);
	for (int i = 0; i < FXLIGHTINGBOLTS; i++) {
		if (self->linkCount[i] < 2) {
			continue;
		}
		/* the animation started again: the bolt ends now */
		if (self->anmLoopReset[i] && (self->prevTime > self->time || self->time >= self->animDur)) {
			self->life[i] = dt;
		}
		if (self->life[i] > 0.0f) {
			float life = self->life[i] - dt;
			self->life[i] = life;
			int from = self->pair[i][0];
			int to = self->pair[i][1];
			if (life > 0.0f) {
				Lighting_Draw(self, i, dt, from, to);
				continue;
			}
			CGameTrail_Clear(&self->trails[i]);
			if (self->glow[i][from]) {
				CGameParticles_KillRange(&self->glows, 4 * i, 4 * i + 2);
			}
			if (self->glow[i][to]) {
				CGameParticles_KillRange(&self->glows, 4 * i + 2, 4 * i + 4);
			}
		} else {
			float sleep = self->sleep[i] - dt;
			self->sleep[i] = sleep;
			if (sleep <= 0.0f) {
				Lighting_Start(self, i);
			}
		}
	}
}

/* the vtable of cFXElementsLighting */
const cMapElementVtbl cFXElementsLighting_vftable = {
	.Delete = cFXElementsLighting_Delete,
	.Init = cFXElementsLighting_Init,
	.Update = cFXElementsLighting_Update,
};

/* the vtable of cFXElementsFireDrop */
const cMapElementVtbl cFXElementsFireDrop_vftable = {
	.Delete = cFXElementsFireDrop_Delete,
	.Init = cFXElementsFireDrop_Init,
	.Update = cFXElementsFireDrop_Update,
};

/* the vtable of cFXElementsLensFlare */
const cMapElementVtbl cFXElementsLensFlare_vftable = {
	.Delete = cFXElementsLensFlare_Delete,
	.Init = cMapElement_InitTrue,
	.Update = cMapElement_UpdateNop,
};

cGVar GVar_FXElementsFireDropMaxDist;

cGVar GVar_FXElementsFireDropSoundAttIni;

cGVar GVar_FXElementsFireDropSoundVol;

cGVar GVar_FXElementsFireDropSoundAttEnd;

cGVar GVar_FXElementsFireDropRateAtt;

cGVar GVar_FXElementsFireDropSound;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_FXElementsFireDropMaxDist, &GVar_FXElementsFireDropMaxDist, "FXElementsFireDropMaxDist", 0, "Maximun Distance to show the Elements Fire Drop Effect", 200000.0f)
INIT_GVAR_FLOAT(Init_GVar_FXElementsFireDropRateAtt, &GVar_FXElementsFireDropRateAtt, "FXElementsFireDropRateAtt", 0, "Rate Attenuation Factor by Distance at Elements Fire Drop Effect", 1.25f)
FINI_GVAR_STRING(Fini_GVar_FXElementsFireDropSound, &GVar_FXElementsFireDropSound)
INIT_GVAR_STRING(Init_GVar_FXElementsFireDropSound, Fini_GVar_FXElementsFireDropSound, &GVar_FXElementsFireDropSound, "FXElementsFireDropSound", 0, "Element FireDrop Sound name", "FXElementsFireDropSound")
INIT_GVAR_FLOAT(Init_GVar_FXElementsFireDropSoundVol, &GVar_FXElementsFireDropSoundVol, "FXElementsFireDropSoundVol", 0, "Element FireDrop Sound volume", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_FXElementsFireDropSoundAttIni, &GVar_FXElementsFireDropSoundAttIni, "FXElementsFireDropSoundAttIni", 0, "Element FireDrop sound attenuation ini", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_FXElementsFireDropSoundAttEnd, &GVar_FXElementsFireDropSoundAttEnd, "FXElementsFireDropSoundAttEnd", 0, "Element FireDrop sound attenuation end", 0.0f)
