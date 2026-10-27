/*
 * World/FX/FXWeapons.cpp of the game: the effects of the weapon shots, see include/scrap/fxweapons.h.
 * Not here (too large): the Update of ATPC (rva 0x1f9f00) and Tesla (0x1fc0a0) and the emitters at 0x1fde50,
 * 0x1ff190 and 0x1ffea0.
 */
#include "scrap/fxweapons.h"
#include "scrap/mathlib.h"
#include "scrap/scrapcollision.h"
#include "scrap/weaps2.h"
#include "scrap/timer.h"
#include "scrap/world.h"

#define g_RandScale GAME_DATA(double, 0x3a3d60)	/* 1/32767 */
#define FX_PI 3.1415927f				/* 0x40490fdb */

/* rand() in [0, 1] times range, plus add (all in double) */
static inline float FXW_Rand(double range, double add) {
	return (float)((double)(crt_rand() & 0x7fff) * g_RandScale * range + add);
}

static inline float FXW_Delta(void) {
	return cTimer_GetDelta(&g_pWorld->timer);
}

/* the node of name in the model, CHECKed */
static int FXW_Node(CGameModel *model, const char *name) {
	int node = CGameModel_FindNodoR(model, name);
	CHECK(node > -1);
	return node;
}

/* the point 50 in front of the model (its z axis turned by its angles) */
static void FXW_Front(CGameModel *model, vec3_t *out, vec3_t *pos) {
	vec3_t rot, off, r;
	CGameModel_GetPos(model, pos);
	CGameModel_GetRot(model, &rot);
	off.x = 0.0f;
	off.y = 0.0f;
	off.z = 50.0f;
	const vec3_t *p = Vec3_Rotate(&r, &off, &rot);
	out->x = p->x + pos->x;
	out->y = p->y + pos->y;
	out->z = p->z + pos->z;
}

/* the smoke trails of the missiles: the end of the fade (0 ends the debris), the debris hidden when it starts */
static byte FXW_MissileAlive(cDebris *debris, cDebrisSys *sys, const float *time, float *fadeStart,
	float *lastShrink, const float *fadeTime) {
	if (cDebris_Update(debris, sys) && CGameModel_IsEnabled(Debris_Model(debris))) {
		return 1;
	}
	float t = *time;
	float fs = *fadeStart;
	if (0.0f > fs) {
		*fadeStart = t;
		*lastShrink = t;
		CGameModel_Disable(Debris_Model(debris));
		return 1;
	}
	if (t - fs >= *fadeTime) {
		return 0;
	}
	return 1;
}

/* ---- cObjElementFXATPC ---- */

void __thiscall cObjElementFXATPC_Start(cObjElementFXATPC *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	CHECK(CGameModel_IsLoaded(model));
	CGameParticles_Attach(&self->particles, model, FXW_Node(model, "Dummy01"));
	CGameTrail_SetMaxTrails(&self->trail1c4, 5);
	CGameTrail_SetType(&self->trail1c4, 3);
	CGameTrail_SetTexture(&self->trail1c4, 0);
	CGameTrail_SetMaxTrails(&self->trail1c8, 0x10);
	CGameTrail_SetType(&self->trail1c8, 0);
	CGameTrail_SetTexture(&self->trail1c8, 1);
	self->n1d4 = 8;
	CGameTrail_SetMaxTrails(&self->trail1d8, 0x11);
	CGameTrail_SetType(&self->trail1d8, 3);
	CGameTrail_SetTexture(&self->trail1d8, 6);
	self->n1cc = 0xc;
	CGameTrail_SetMaxTrails(&self->trail1d0, 0x19);
	CGameTrail_SetType(&self->trail1d0, 3);
	CGameTrail_SetTexture(&self->trail1d0, 7);
	CGameTrail_SetMaxTrails(&self->trail1e0, (byte)(self->n1cc * 2 + 1));
	CGameTrail_SetType(&self->trail1e0, 3);
	CGameTrail_SetTexture(&self->trail1e0, 0x10);
}

void __thiscall cObjElementFXATPC_Reset(cObjElementFXATPC *self, cDebris *debris) {
	(void)debris;
	self->f18c = 0;
	CGameTrail_Clear(&self->trail1c4);
	CGameTrail_Clear(&self->trail1d8);
	CGameTrail_Clear(&self->trail1d0);
	CGameTrail_Clear(&self->trail1c8);
	CGameTrail_Clear(&self->trail1e0);
	self->f180 = 0;
	self->f1dc = 0;
}

/* ---- cObjElementFXDevastatorShoot ---- */

void __thiscall cObjElementFXDevastatorShoot_Update(cObjElementFXDevastatorShoot *self, cDebris *debris, float dt) {
	(void)debris;
	(void)dt;
	float t = self->time + FXW_Delta();
	self->time = t;
	if (!self->first && t > self->duration) {
		return;
	}
	int a = 0;
	if (!self->first) {
		a = (int)(t / self->duration * 255.0f);
		if (a > 0xff) {
			CGameTrail_SetTransp(&self->trail, 0xff);
			self->first = 0;
			return;
		}
		if (a < 0) {
			a = 0;
		}
	}
	CGameTrail_SetTransp(&self->trail, (byte)a);
	self->first = 0;
}

byte __thiscall cObjElementFXDevastatorShoot_IsAlive(cObjElementFXDevastatorShoot *self, cDebris *debris,
	cDebrisSys *sys) {
	(void)debris;
	(void)sys;
	if (!self->first && self->time >= self->duration) {
		CGameTrail_Clear(&self->trail);
		return 0;
	}
	return 1;
}

void __thiscall cObjElementFXDevastatorShoot_Start(cObjElementFXDevastatorShoot *self, cDebris *debris) {
	(void)debris;
	CGameTrail *t = &self->trail;
	CGameTrail_SetMaxTrails(t, 6);
	CGameTrail_SetTexture(t, 3);
	CGameTrail_SetFade(t, 0);
	CGameTrail_SetWidth(t, 200.0f);
	CGameTrail_SetPremul(t, 1);
}

void __thiscall cObjElementFXDevastatorShoot_Reset(cObjElementFXDevastatorShoot *self, cDebris *debris) {
	(void)debris;
	self->time = 0.0f;
	CGameTrail_Clear(&self->trail);
	CGameTrail_SetTransp(&self->trail, 0xff);
	self->first = 1;
}

/* ---- cObjElementFXInfernoExplosion ---- */

static inline uint32_t FXW_Gray(int a) {
	uint32_t u = (uint32_t)a;
	return ((~u << 8 | u) << 8 | u) << 8 | u;
}

void __thiscall cObjElementFXInfernoExplosion_Update(cObjElementFXInfernoExplosion *self, cDebris *debris,
	float dt) {
	(void)debris;
	(void)dt;
	float d = FXW_Delta();
	float step = self->stepTime + d;
	float time = self->time + d;
	self->time = time;
	self->stepTime = step;
	if (0.01f > step) {
		return;
	}

	/* the rays fade out over the fade time before the end */
	float u = time - ((g_FXInfernoExplosionTime - 0.5f) - g_FXInfernoExplosionFadeTime);
	float c;
	if (0.0f > u) {
		c = 0.0f;
	} else {
		c = Float_MinSS(1.0f, u);
	}
	float s = (3.0f - (c + c)) * (c * c);
	int a = (int)((3.0f - (s + s)) * (s * s) * 255.0f);
	for (int i = 0; i < FXINFERNO_TRAILS; i++) {
		CGameTrail_Reset(&self->trails[i]);
		CGameTrail_SetTransp(&self->trails[i], (byte)a);
	}

	int inTrail = 0;
	int trail = 0;
	for (int i = 0; i < self->numRays; i++) {
		float len = self->rayLen[i];
		if (len > g_FXInfernoRayMinSize && trail < FXINFERNO_TRAILS) {
			const vec3_t *b = &self->rayBase[i];
			const vec3_t *dir = &self->rayDir[i];
			vec3_t end;
			end.x = b->x + dir->x * len;
			end.y = b->y + dir->y * len;
			end.z = b->z + dir->z * len;
			float w = len * g_FXInfernoRayRadius;
			CGameTrail_Add(&self->trails[trail], *b, w * 0.8f, 0x20f5aa19);
			CGameTrail_Add(&self->trails[trail], end, w, 0xffe12d0a);
			inTrail++;
			if (inTrail >= FXINFERNO_TRAILS) {
				trail++;
				inTrail = 0;
			}
		}
		float speed = self->raySpeed[i];
		self->rayLen[i] = self->stepTime * speed + self->rayLen[i];
		self->raySpeed[i] = self->rayAcc[i] * self->stepTime + speed;
		float max = self->rayMax[i];
		if (self->rayLen[i] > max && 3.4028235e38f > max) {
			self->rayLen[i] = max;
			self->raySpeed[i] = 0.0f;
			self->rayAcc[i] = 0.0f;
		}
	}

	if (g_SceneDuDvRTarget) {
		/* the core: grows, then fades out after 0.6 */
		float v = (self->time * 1.1f - 0.25f) / g_FXInfernoExplosionTime;
		if (v > 1.0f) {
			c = 1.0f;
		} else {
			c = Float_MaxSS(0.0f, v);
		}
		float q = c * 1.6666666f;
		s = q * q * (2.0f - q);
		a = (int)(s * s * (2.0f - s) * 255.0f);
		if (c > 0.6f) {
			q = (c - 0.6f) * 2.5f;
			s = q * q * (2.0f - q);
			a = 0xff - (int)(s * s * (s - 2.0f) * -255.0f);
		}
		float r = 1.0f - c;
		CGameParticles_SetParticle(&self->cores, 0, self->pos.x, self->pos.y, self->pos.z,
			r * r * (2.0f - r) * (g_FXInfernoCoreSize * 60.0f), FXW_Gray(a), 1, FX_PI);

		/* the flash */
		v = 1.0f - self->time / g_FXInfernoExplosionTime;
		if (v > 1.0f) {
			c = 1.0f;
		} else {
			c = Float_MaxSS(0.0f, v);
		}
		r = 1.0f - c;
		s = c * c * (2.0f - c);
		a = (int)((3.0f - (s + s)) * (s * s) * 255.0f);
		float size = (3.0f - (r + r)) * (r * r) * (g_FXInfernoCoreSize * 80.0f) * 1.5f;
		CGameParticles_SetParticle(&self->cores, 1, self->pos.x, self->pos.y, self->pos.z, size, FXW_Gray(a), 1,
			0.0f);
	}
	self->stepTime = 0.0f;
}

void __thiscall cObjElementFXInfernoExplosion_Start(cObjElementFXInfernoExplosion *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	CHECK(CGameModel_IsLoaded(model));
	CGameParticles_Attach(&self->particles, model, FXW_Node(model, "Dummy01"));
	CGameParticles_Attach(&self->cores, model, FXW_Node(model, "Dummy02(+dudv_pass)"));
	for (int i = 0; i < FXINFERNO_TRAILS; i++) {
		CGameTrail_SetMaxTrails(&self->trails[i], 0x1f);
		CGameTrail_SetTexture(&self->trails[i], 0);
		CGameTrail_SetType(&self->trails[i], 3);
	}
	self->numRays = 0;
}

void __thiscall cObjElementFXInfernoExplosion_Reset(cObjElementFXInfernoExplosion *self, cDebris *debris) {
	(void)debris;
	self->time = 0.0f;
	self->stepTime = 0.0f;
	CGameParticles_KillAll(&self->particles);
	CGameParticles_KillAll(&self->cores);
	for (int i = 0; i < FXINFERNO_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
	}
	self->numRays = 0;
}

/* a random direction: the unit vector (1, 1, 1) turned by three random angles (0..2 pi, the first rand is z) */
static void FXW_RandDir(vec3_t *out) {
	static const vec3_t one = { 1.0f, 1.0f, 1.0f };
	vec3_t rot;

	rot.z = FXW_Rand((double)M_2PI_F, 0.0);
	rot.y = FXW_Rand((double)M_2PI_F, 0.0);
	rot.x = FXW_Rand((double)M_2PI_F, 0.0);
	Vec3_Rotate(out, &one, &rot);
}

/* the rays (up to the floor within range) and the core particles of an explosion at x y z; bees widens it */
void __thiscall cObjElementFXInfernoExplosion_Explode(cObjElementFXInfernoExplosion *self, float x, float y, float z,
	float range, int bees, byte flag) {
	static const vec3_t one = { 1.0f, 1.0f, 1.0f };
	CGameParticles *p = &self->particles;
	float k, ax, ay;
	int i, c;

	self->pos.x = x;
	self->pos.y = y;
	self->pos.z = z;
	self->bees = bees;
	self->flag = flag;
	self->range = range;
	k = ((float)bees * g_InfernoRangeBay + g_InfernoRangeMin) / g_InfernoRangeMin;
	self->numRays = 0;
	k = k * 0.3f;
	for (i = 0; i < FXINFERNO_RAYS; i++) {
		vec3_t dir, base, to, hit;
		float lo, hi, t;
		int n;

		if (self->numRays >= FXINFERNO_RAYS) {
			break;
		}
		FXW_RandDir(&dir);
		FXW_RandDir(&base);
		hi = g_FXInfernoCoreSize * 0.25f;
		lo = g_FXInfernoCoreSize * 0.05f;
		t = FXW_Rand((double)(hi - lo), (double)lo);
		n = self->numRays;
		self->rayBase[n].x = base.x * t * k + self->pos.x;
		self->rayBase[n].y = self->pos.y + base.y * t * k;
		self->rayBase[n].z = self->pos.z + base.z * t * k;
		self->rayDir[n] = dir;
		self->rayLen[n] = FXW_Rand((double)(g_FXInfernoRayInitSizeMax - g_FXInfernoRayInitSizeMin),
			(double)g_FXInfernoRayInitSizeMin);
		self->raySpeed[n] = FXW_Rand((double)(g_FXInfernoRaySpeedMax - g_FXInfernoRaySpeedMin),
			(double)g_FXInfernoRaySpeedMin) * k;
		self->rayAcc[n] = -(FXW_Rand((double)(g_FXInfernoRayAccMax - g_FXInfernoRayAccMin),
			(double)g_FXInfernoRayAccMin) * k);
		self->rayMax[n] = 3.4028235e38f;
		to.x = self->range * self->rayDir[n].x + self->rayBase[n].x;
		to.y = self->rayBase[n].y + self->rayDir[n].y * self->range;
		to.z = self->rayBase[n].z + self->rayDir[n].z * self->range;
		if (Engine_TestLine(&self->rayBase[n], &to, &hit, NULL)) {
			float dy = hit.y - self->rayBase[n].y, dx = hit.x - self->rayBase[n].x, dz = hit.z - self->rayBase[n].z;

			self->rayMax[n] = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
			self->rayMax[n] = self->rayMax[n] - self->rayMax[n] * g_FXInfernoRayRadius;
		}
		if (self->rayMax[n] >= g_FXInfernoRayMinSize) {
			self->numRays++;
		}
	}
	/* three big particles around the center */
	p->birthRate = 0;
	for (c = 3; c; c--) {
		vec3_t dir;
		float b1, b2, b3, lo, hi, t;

		FXW_RandDir(&dir);
		b1 = FXW_Rand(30.0, -15.0);
		b2 = FXW_Rand(30.0, -15.0);
		b3 = FXW_Rand(30.0, -15.0) * k;
		b2 = b2 * k;
		b1 = b1 * k;
		hi = g_FXInfernoCoreSize * 0.5f;
		lo = g_FXInfernoCoreSize * 0.1f;
		t = FXW_Rand((double)(hi - lo), (double)lo);
		p->initPosMin.x = dir.x * t + self->pos.x + b3;
		p->initPosMin.y = dir.y * t + self->pos.y + b2;
		p->initPosMin.z = dir.z * t + self->pos.z + b1;
		p->initPosMax = p->initPosMin;
		p->initVelMin.x = 0.0f;
		p->initVelMin.y = 0.0f;
		p->initVelMin.z = 0.0f;
		p->initVelMax = p->initVelMin;
		p->initAccMin.x = 0.0f;
		p->initAccMin.y = 0.0f;
		p->initAccMin.z = 0.0f;
		p->initAccMax = p->initAccMin;
		p->rotIni = 0.0f;
		p->rotEnd = 0.0f;
		p->rotVar = 0.0f;
		t = FXW_Rand(0x1.999998p-4, 0x1.99999ap-2) * g_FXInfernoExplosionTime;
		p->sizeLife = 0.25f;
		p->spriteIni = 2;
		p->spriteEnd = 2;
		p->lifeTime = t;
		p->sizeIni = FXW_Rand(0x1.99999p-4, 0x1.99999ap-1) * g_FXInfernoCoreSize * k;
		p->sizeEnd = FXW_Rand(0.5, 7.5) * g_FXInfernoCoreSize * k;
		CGameParticles_SetColorRampOn(p, 1);
		CGameParticles_SetColorRamp(p, g_FXInfernoRamp);
		CGameParticles_Apply(p);
		CGameParticles_EmitN(p, 1, 1);
	}
	/* a shell of particles every 45 degrees, moving out */
	p->birthRate = 0;
	p->rotIni = 0.0f;
	p->rotEnd = 0.0f;
	p->rotVar = 0.0f;
	{
		float t = FXW_Rand(0x1.99999p-4, 0x1.333334p-1) * g_FXInfernoExplosionTime;

		p->sizeLife = 0.2f;
		p->spriteIni = 2;
		p->spriteEnd = 2;
		p->lifeTime = t;
	}
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, g_FXInfernoRamp);
	for (ax = 0.0f; M_2PI_D > (double)ax; ax = ax + 0.7853982f) {
		for (ay = 0.0f; M_2PI_D > (double)ay; ay = ay + 0.7853982f) {
			vec3_t rot = { ax, ay, 0.0f }, d;
			float b1, b2, b3;

			Vec3_Rotate(&d, &one, &rot);
			b1 = FXW_Rand(30.0, -15.0);
			b2 = FXW_Rand(30.0, -15.0);
			b3 = FXW_Rand(30.0, -15.0) * k;
			b2 = b2 * k;
			b1 = b1 * k;
			p->initPosMin.y = self->pos.y + d.y * g_FXInfernoCoreSize * 0.5f + b2;
			p->initPosMin.x = d.x * g_FXInfernoCoreSize * 0.5f + self->pos.x + b3;
			p->initPosMin.z = self->pos.z + d.z * g_FXInfernoCoreSize * 0.5f + b1;
			p->initPosMax = p->initPosMin;
			p->initVelMin.x = d.x * g_FXInfernoCoreSize * 0.5f * k;
			p->initVelMin.y = d.y * g_FXInfernoCoreSize * 0.5f * k;
			p->initVelMin.z = d.z * g_FXInfernoCoreSize * 0.5f * k;
			p->initVelMax = p->initVelMin;
			p->initAccMin.x = 0.0f;
			p->initAccMin.y = 0.0f;
			p->initAccMin.z = 0.0f;
			p->initAccMax = p->initAccMin;
			p->sizeIni = FXW_Rand(0x1.99999p-4, 0x1.99999ap-1) * g_FXInfernoCoreSize * k;
			p->sizeEnd = FXW_Rand(0.5, 2.5) * g_FXInfernoCoreSize * k;
			CGameParticles_Apply(p);
			CGameParticles_EmitDt(p, 1, 1, 0.05f);
		}
	}
}

/* ---- cObjElementFXInfernoMissile ---- */

void __thiscall cObjElementFXInfernoMissile_Update(cObjElementFXInfernoMissile *self, cDebris *debris, float dt) {
	cDebris_Move(debris, dt);
	float d = FXW_Delta();
	float time = self->time + d;
	self->time = time;
	float fs = self->fadeStart;
	if (fs > 0.0f) {
		/* the trail fades out */
		int a = (int)((time - fs) / self->fadeTime * 255.0f);
		if (a > 0xff) {
			a = 0xff;
		}
		CGameTrail_SetTransp(&self->trail, (byte)a);
		if (self->flareOn) {
			CGameLensFlare_SetIntensity(&self->flare,
				(1.0f - (self->time - self->fadeStart) / self->fadeTime) * 0.2f);
			CGameLensFlare_SetOn(&self->flare, 1);
		}
		time = self->time;
		if (time - self->lastShrink > 0.1f) {
			self->lastShrink = time;
			CGameTrail_Shrink(&self->trail);
		}
		return;
	}

	CGameModel *model = Debris_Model(debris);
	vec3_t pos, p;
	FXW_Front(model, &p, &pos);
	if (self->flareOn) {
		CGameLensFlare_SetPos(&self->flare, pos);
		CGameLensFlare_SetIntensity(&self->flare, 0.2f);
		CGameLensFlare_SetOn(&self->flare, 1);
	}
	float dy = p.y - self->last.y;
	float dx = p.x - self->last.x;
	float dz = p.z - self->last.z;
	float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	if (!(dist > self->minDist) && CGameTrail_IsRender(&self->trail)) {
		CGameTrail_SetLastPosColor(&self->trail, p, 0x80ffdc5a);
		return;
	}
	self->last = p;
	CGameTrail_Add(&self->trail, p, FXW_Rand(10.0, 160.0), 0x80ffdc5a);
}

byte __thiscall cObjElementFXInfernoMissile_IsAlive(cObjElementFXInfernoMissile *self, cDebris *debris,
	cDebrisSys *sys) {
	return FXW_MissileAlive(debris, sys, &self->time, &self->fadeStart, &self->lastShrink, &self->fadeTime);
}

void __thiscall cObjElementFXInfernoMissile_Start(cObjElementFXInfernoMissile *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	CHECK(CGameModel_IsLoaded(model));
	CGameParticles_Attach(&self->particles, model, FXW_Node(model, "Dummy01"));
	CHECK(self->particles.SistPart);
	CGameParticles_SetParticle(&self->particles, 0, 0.0f, 0.0f, 0.0f, 6000.0f, 0xc0ff2805, 6, 0.0f);
	CGameParticles_SetParticle(&self->particles, 1, 0.0f, 0.0f, 0.0f, 2650.0f, 0x40ffbe32, 5, 0.0f);
	CGameParticles_SetParticle(&self->particles, 2, 0.0f, 0.0f, 0.0f, 450.0f, 0x10fff050, 1, 0.0f);
	CGameParticles_SetParticle(&self->particles, 3, 0.0f, 0.0f, 0.0f, 250.0f, 0x08fff580, 1, 0.0f);
	CGameTrail *t = &self->trail;
	CGameTrail_SetMaxTrails(t, 0x1e);
	CGameTrail_SetFade(t, 1);
	CGameTrail_SetSmooth(t, 1);
	CGameTrail_SetTexture(t, 0);
	CGameTrail_SetColor(t, 0x3cfafafa);
	CGameTrail_SetWidth(t, 65.0f);
	CGameTrail_SetScale(t, 1.0f, 8.0f);
	CGameTrail_SetType(t, 2);
	CGameTrail_SetPremul(t, 1);
	self->minDist = 700.0f;
	self->fadeTime = 0.75f;
	self->time = 0.0f;
	self->fadeStart = -1.0f;
	self->lastShrink = 0.0f;
	self->flareOn = 0;
	CGameLensFlare_SetF30(&self->flare, 0.2f);
	CGameLensFlare_SetI1c(&self->flare, 2);
}

void __thiscall cObjElementFXInfernoMissile_Reset(cObjElementFXInfernoMissile *self, cDebris *debris) {
	(void)debris;
	CGameTrail_Clear(&self->trail);
	CGameTrail_SetTransp(&self->trail, 0);
	self->time = 0.0f;
	self->fadeStart = -1.0f;
	self->lastShrink = 0.0f;
	self->flareOn = 0;
	CGameLensFlare_SetOn(&self->flare, 0);
}

void __thiscall cObjElementFXInfernoMissile_SetFlare(cObjElementFXInfernoMissile *self, byte on) {
	self->flareOn = on;
}

/* ---- cObjElementFXLaser ---- */

void __thiscall cObjElementFXLaser_Update(cObjElementFXLaser *self, cDebris *debris, float dt) {
	(void)dt;
	CGameModel *model = Debris_Model(debris);
	vec3_t *pos = &self->pos;
	float d = FXW_Delta();
	self->time = self->time + d;
	CGameModel_SetPos(model, pos);
	float dy = pos->y - self->start.y;
	float dx = pos->x - self->start.x;
	float dz = pos->z - self->start.z;
	self->dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	byte vis = CGameModel_IsVisible(model);
	if (!vis) {
		if (self->drawn) {
			CGameTrail_Clear(&self->trail);
			CGameParticles_KillAll(&self->particles);
			self->drawn = 0;
		}
		float vx = self->vel.x * d;
		float vy = self->vel.y * d;
		float vz = self->vel.z * d;
		pos->x = pos->x + vx;
		pos->y = pos->y + vy;
		pos->z = pos->z + vz;
		return;
	}
	if (self->dist > self->len) {
		return;
	}

	dy = pos->y - self->end.y;
	dx = pos->x - self->end.x;
	dz = pos->z - self->end.z;
	float rest = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	self->rest = rest;
	/* the beam is g_FXLaserBeamSize long, shorter (and fading) at the end */
	float size = g_FXLaserBeamSize;
	float len;
	int a;
	if (size > rest) {
		len = rest;
		a = (int)(255.0f - rest / size * 255.0f);
	} else {
		len = size;
		if (size > self->dist) {
			a = (int)(255.0f - size / size * 255.0f);
		} else {
			a = 0;
		}
	}
	CGameTrail_SetTransp(&self->trail, (byte)a);
	vec3_t tip;
	float tx = len * self->dir.x;
	float ty = self->dir.y * len;
	float tz = self->dir.z * len;
	tip.x = pos->x + tx;
	tip.y = pos->y + ty;
	tip.z = pos->z + tz;
	CGameTrail_Reset(&self->trail);
	CGameTrail_Add(&self->trail, *pos, g_FXLaserBeamHeight, 0x40ffc814);
	CGameTrail_Add(&self->trail, tip, g_FXLaserBeamHeight, 0x40ffc814);
	int n = (int)(len / (g_FXLaserBeamSize / (float)CGameParticles_NumParticles(&self->particles)));
	float step = len / (float)(n - 1);
	CGameParticles_KillRange(&self->particles, n, CGameParticles_NumParticles(&self->particles));
	for (int i = 0; i < n; i++) {
		float f = (float)i;
		float px = self->dir.x * step * f;
		float py = self->dir.y * step * f;
		float pz = self->dir.z * step * f;
		CGameParticles_SetParticle(&self->particles, i, pos->x + px, py + pos->y, pz + pos->z,
			g_FXLaserBeamHeight * 2.25f, 0x5cff5514, 0, 0.0f);
	}
	float vx = self->vel.x * d;
	float vy = self->vel.y * d;
	float vz = self->vel.z * d;
	pos->x = vx + pos->x;
	pos->y = pos->y + vy;
	pos->z = pos->z + vz;
	self->drawn = vis;
}

byte __thiscall cObjElementFXLaser_IsAlive(cObjElementFXLaser *self, cDebris *debris, cDebrisSys *sys) {
	(void)debris;
	(void)sys;
	return !(self->dist > self->len);
}

void __thiscall cObjElementFXLaser_Start(cObjElementFXLaser *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	if (CGameModel_IsLoaded(model)) {
		int node = CGameModel_FindNodoR(model, "Dummy01");
		if (node != -1) {
			CGameParticles_Attach(&self->particles, model, node);
		}
	}
	CHECK(self->particles.SistPart);
	CGameTrail_SetMaxTrails(&self->trail, 3);
	CGameTrail_SetTexture(&self->trail, 0);
	CGameTrail_SetType(&self->trail, 2);
	self->time = 0.0f;
	self->drawn = 0;
}

void __thiscall cObjElementFXLaser_Reset(cObjElementFXLaser *self, cDebris *debris) {
	(void)debris;
	CGameTrail_Clear(&self->trail);
	CGameTrail_SetTransp(&self->trail, 0);
	CGameParticles_KillAll(&self->particles);
	self->time = 0.0f;
	self->drawn = 0;
}

void __thiscall cObjElementFXLaser_Shoot(cObjElementFXLaser *self, float x0, float y0, float z0, float x1, float y1,
	float z1, float time) {
	self->start.x = x0;
	self->start.y = y0;
	self->start.z = z0;
	self->end.x = x1;
	self->end.y = y1;
	self->end.z = z1;
	self->duration = time;
	float dx = self->end.x - self->start.x;
	float dy = self->end.y - self->start.y;
	float dz = self->end.z - self->start.z;
	self->delta.x = dx;
	self->delta.y = dy;
	self->delta.z = dz;
	self->len = (float)__builtin_sqrt((double)(dx * dx + dy * dy + dz * dz));
	dx = self->delta.x;
	dy = self->delta.y;
	dz = self->delta.z;
	float l = (float)__builtin_sqrt((double)(dx * dx + dy * dy + dz * dz));
	vec3_t dir;
	if (l == 0.0f) {
		dir = self->delta;
	} else {
		float inv = 1.0f / l;
		dir.x = self->delta.x * inv;
		dir.y = self->delta.y * inv;
		dir.z = self->delta.z * inv;
	}
	float invt = 1.0f / time;
	self->dir = dir;
	self->vel.x = self->delta.x * invt;
	self->vel.z = self->delta.z * invt;
	self->vel.y = self->delta.y * invt;
	self->pos = self->start;
	dx = self->pos.x - self->end.x;
	dy = self->pos.y - self->end.y;
	dz = self->pos.z - self->end.z;
	self->dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
}

/* ---- cObjElementFXSwarmMissile ---- */

void __thiscall cObjElementFXSwarmMissile_Update(cObjElementFXSwarmMissile *self, cDebris *debris, float dt) {
	cDebris_Move(debris, dt);
	if (self->node == -1) {
		return;
	}
	self->time = self->time + FXW_Delta();
	vec3_t pos, p;
	FXW_Front(Debris_Model(debris), &p, &pos);
	float fs = self->fadeStart;
	if (fs > 0.0f) {
		int a = (int)((self->time - fs) / self->fadeTime * 255.0f);
		if (a > 0xff) {
			a = 0xff;
		}
		CGameTrail_SetTransp(&self->trail, (byte)a);
		CGameTrail_SetLastPos(&self->trail, p);
		float time = self->time;
		if (time - self->lastShrink > 0.1f) {
			self->lastShrink = time;
			CGameTrail_Shrink(&self->trail);
		}
		return;
	}

	/* the color goes from orange to white with the time */
	float s = self->time * 255.0f;
	int r = (int)(s + 255.0f);
	if (r > 0xff) {
		r = 0xff;
	}
	int g = (int)(s + 200.0f);
	int b = (int)(s + 90.0f);
	if (g > 0xff) {
		g = 0xff;
	}
	if (b > 0xff) {
		b = 0xff;
	}
	uint32_t color = (uint32_t)((r | 0xc000) >> 2);
	color = color << 8 | (uint32_t)(g >> 2);
	color = color << 8 | (uint32_t)(b >> 2);
	float dx = p.x - self->last.x;
	float dy = p.y - self->last.y;
	float dz = p.z - self->last.z;
	float dist2 = dy * dy + dx * dx + dz * dz;
	if (!(dist2 > self->minDist2) && CGameTrail_IsRender(&self->trail)) {
		CGameTrail_SetLastPosColor(&self->trail, p, color);
		return;
	}
	self->last = p;
	CGameTrail_Add(&self->trail, p, FXW_Rand(10.0, 70.0), color);
}

byte __thiscall cObjElementFXSwarmMissile_IsAlive(cObjElementFXSwarmMissile *self, cDebris *debris, cDebrisSys *sys) {
	return FXW_MissileAlive(debris, sys, &self->time, &self->fadeStart, &self->lastShrink, &self->fadeTime);
}

void __thiscall cObjElementFXSwarmMissile_Start(cObjElementFXSwarmMissile *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	CGameParticles *p = &self->particles;
	self->node = -1;
	if (CGameModel_IsLoaded(model)) {
		self->node = CGameModel_FindNodoR(model, "DF_MissileMotor");
	}
	if (self->node != -1) {
		CGameParticles_Attach(p, model, self->node);
	}
	CHECK(p->SistPart);
	CGameParticles_SetParticle(p, 0, 0.0f, 25.0f, 0.0f, FXW_Rand(250.0, 725.0), 0x80ff5514, 5, 0.0f);
	CGameParticles_SetParticle(p, 1, 0.0f, 40.0f, 0.0f, FXW_Rand(25.0, 150.0), 0x50ffc33c, 2, 0.0f);
	CGameParticles_SetParticle(p, 2, 0.0f, 120.0f, 0.0f, FXW_Rand(25.0, 125.0), 0x6effa528, 2, 0.0f);
	CGameParticles_SetParticle(p, 3, 0.0f, 190.0f, 0.0f, FXW_Rand(20.0, 92.0), 0x8cff7d14, 2, 0.0f);
	CGameTrail *t = &self->trail;
	CGameTrail_SetMaxTrails(t, 0x1e);
	CGameTrail_SetFade(t, 1);
	CGameTrail_SetSmooth(t, 1);
	CGameTrail_SetTexture(t, 2);
	CGameTrail_SetColor(t, 0x3cfafafa);
	CGameTrail_SetWidth(t, 65.0f);
	CGameTrail_SetScale(t, 1.0f, 8.0f);
	self->minDist2 = 490000.0f;
	self->fadeTime = 0.6f;
	self->time = 0.0f;
	self->fadeStart = -1.0f;
	self->lastShrink = 0.0f;
}

void __thiscall cObjElementFXSwarmMissile_Reset(cObjElementFXSwarmMissile *self, cDebris *debris) {
	(void)debris;
	CGameTrail_Clear(&self->trail);
	CGameTrail_SetTransp(&self->trail, 0);
	self->time = 0.0f;
	self->fadeStart = -1.0f;
	self->lastShrink = 0.0f;
}

/* ---- cObjElementFXTesla ---- */

byte __thiscall cObjElementFXTesla_IsAlive(cObjElementFXTesla *self, cDebris *debris, cDebrisSys *sys) {
	(void)debris;
	(void)sys;
	cEntity *e = self->emitter;
	if (!e) {
		return 0;
	}
	/* the fade starts when the emitter stops shooting the Tesla (or has no ammo left) */
	if (!self->shooting || !Entity_B636(e) || !Entity_B436(e) || Entity_CurWeapon(e) != g_TeslaIDWeap
		|| 0.0f >= cTargetLock_GetAmmo(Entity_TargetLock(e), g_TeslaIDAmmo)) {
		self->shooting = 0;
		if (0.0f > self->fadeStart) {
			self->fadeStart = self->time;
		}
	}
	if (self->fadeStart >= 0.0f && self->time - self->fadeStart > g_FXTeslaFadeOutTime) {
		return 0;
	}
	return 1;
}

void __thiscall cObjElementFXTesla_Start(cObjElementFXTesla *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	/* the original keeps the debris pointer as the node when the model has none */
	int node = (int)(intptr_t)debris;
	if (CGameModel_IsLoaded(model)) {
		node = CGameModel_FindNodoR(model, "Dummy01");
	}
	if (node != -1) {
		CGameParticles_Attach(&self->particles, model, node);
	}
	CHECK(self->particles.SistPart);
	for (int i = 0; i < FXTESLA_SLOTS; i++) {
		self->a750[i] = 0;
		self->a734[i] = 0;
		for (int j = 0; j < FXTESLA_TRAILS; j++) {
			CGameTrail *t = &self->trails[i * FXTESLA_TRAILS + j];
			CGameTrail_SetMaxTrails(t, 0x1e);
			CGameTrail_SetSmooth(t, 1);
			CGameTrail_SetTexture(t, j ? 5 : 0);
			CGameTrail_SetPremul(t, j != 0);
			CGameTrail_SetULen(t, 1.0f, 0.0f);
		}
	}
}

void __thiscall cObjElementFXTesla_Reset(cObjElementFXTesla *self, cDebris *debris) {
	(void)debris;
	self->time = 0.0f;
	self->fadeStart = -1.0f;
	self->followTime = g_FXTeslaFollowTime;
	self->b8 = 0;
	for (int i = 0; i < FXTESLA_SLOTS; i++) {
		self->a750[i] = 0;
		self->a734[i] = 0;
		if (self->slotOn[i]) {
			self->slotOn[i] = 0;
			for (int j = 0; j < FXTESLA_TRAILS; j++) {
				CGameTrail_Clear(&self->trails[i * FXTESLA_TRAILS + j]);
			}
		}
	}
	CGameParticles_KillAll(&self->particles);
}

void __thiscall cObjElementFXTesla_SetEmitter(cObjElementFXTesla *self, cEntity *e) {
	self->emitter = e;
	self->f88 = 0;
	self->b8 = 0;
	self->shooting = 1;
	self->b74d = 1;
	CHECK(e);
	CHECK(CGameModel_IsLoaded(FX_Vtbl(self->emitter)->GetModel(self->emitter)));
	self->numSlots = 0;
	for (int i = 0; i < FXTESLA_SLOTS; i++) {
		if (cTargetLock_HasSlot(Entity_TargetLock(self->emitter), g_TeslaIDWeap, i)) {
			self->numSlots++;
			self->slotOn[i] = 1;
		} else {
			self->slotOn[i] = 0;
		}
	}
}
