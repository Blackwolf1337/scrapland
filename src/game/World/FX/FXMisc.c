/*
 * World/FX/FXMisc.cpp of the game: the small effects of the world, see include/scrap/fxmisc.h.
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/crt.h"
#include "scrap/fxchar.h"
#include "scrap/fxcountermeasures.h"
#include "scrap/fxmisc.h"
#include "scrap/fxscene.h"
#include "scrap/objentity.h"
#include "scrap/scorer.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapworld.h"
#include "scrap/world.h"
#include "scrap/staticinit.h"

#define g_FXItemBlinkFile (GVar_FXItemBlinkFile.value.s)
#define g_FXItemFadeFile (GVar_FXItemFadeFile.value.s)
#define g_FXItemTakeFile (GVar_FXItemTakeFile.value.s)
#define g_FXShipEditBuildFile (GVar_FXShipEditBuildFile.value.s)
static cGVar GVar_FXShipEditBuildFile;
static cGVar GVar_FXItemTakeFile;
static cGVar GVar_FXItemFadeFile;
static cGVar GVar_FXItemBlinkFile;

/* the trail array members: CGameTrail by new[] style construction (rva) */

static const vec3_t fx_corners[8] = {
	{ -1.0f, -1.0f, -1.0f }, { -1.0f, 1.0f, -1.0f }, { 1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, -1.0f },
	{ -1.0f, -1.0f, 1.0f }, { -1.0f, 1.0f, 1.0f }, { 1.0f, -1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f },
};

/* rand() & 0x7fff as 0..1 (double) */
static inline double FX_Rnd(void) {
	return (double)(crt_rand() & 0x7fff) * (1.0 / 32767.0);
}

static inline float FX_RandF(double range, double base) {
	return (float)(FX_Rnd() * range + base);
}

/* lo..hi with the range taken in float */
static inline float FX_RandRange(float lo, float hi) {
	return (float)(FX_Rnd() * (double)(hi - lo) + (double)lo);
}

static inline uint32_t FX_Bits(float f) {
	uint32_t u;

	memcpy(&u, &f, sizeof(u));
	return u;
}

static inline int FX_Gone(cEntity *e) {
	return Entity_Removed(e) || !e->active;
}

/* the model effect of the item fades: 1 off and disabled, 2 off and enabled */
static void FX_EndFade(CGameModel *m, int mode) {
	if (mode == 1) {
		CGameModel_SetEffect(m, MODEL_EFFECT_NONE);
		CGameModel_Disable(m);
	} else if (mode == 2) {
		CGameModel_SetEffect(m, MODEL_EFFECT_NONE);
		CGameModel_Enable(m);
	}
}

void FX_AttachDummy(CGameParticles *sp, CGameModel *m) {
	uint32_t i = CGameModel_IsLoaded(m) ? (uint32_t)CGameModel_FindRenderNode(m, "Dummy01") : (uint32_t)(uintptr_t)m;

	if (i != 0xffffffffu) {
		CGameParticles_Attach(sp, m, i);
	}
}

/* the inline strcmp == 0 of two names: the first two characters as one word, then the rest */
static int FX_NameEq(const char *a, const char *b) {
	uint16_t wa, wb;

	memcpy(&wa, a, 2);
	memcpy(&wb, b, 2);
	return wa == wb && !strcmp(a + 2, b + 2);
}

void *FX_DropNew(const char *file, float life) {
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, file);
	cDebris *d = cDebrisSys_NewElement(sys);

	d->life = life;
	cDebrisSys_Drop(sys, d);
	return Debris_Element(d);
}

/* the free functions */

byte FX_IsVisible(float x, float y, float z, float radius, float maxDist, const CGameCamera *cam, byte sector) {
	float px, py, pz;

	if (g_SplitScreenGame) {
		return 1;
	}
	cWorld *w = g_pWorld;
	vec3_t cp = w->camPos[w->curCam];
	if (!cam) {
		cam = Camera_GetCurrent();
	}
	if (!cam) {
		px = cp.x;
		py = cp.y;
		pz = cp.z;
	} else {
		px = cam->pos.x;
		py = cam->pos.y;
		pz = cam->pos.z;
	}
	vec3_t d = { x - px, y - py, z - pz };
	float r2 = radius * radius;
	float d2 = d.y * d.y + d.x * d.x + d.z * d.z;
	float m2 = maxDist * maxDist;
	if (r2 >= d2) {
		return 1;
	}
	if (m2 > 0.0f && d2 - r2 > m2) {
		return 0;
	}
	if (cam) {
		vec3_t fwd, n;
		const vec3_t front = { 0.0f, 0.0f, 1.0f };
		Vec3_Rotate(&fwd, &front, &cam->rot);
		float fov = cam->fov * 0.75f;
		if (Vec3_Angle(&d, &fwd) >= (double)fov) {
			vec3_t v = { fwd.x * d2 - d.x, fwd.y * d2 - d.y, fwd.z * d2 - d.z };
			const vec3_t *nn = Vec3_NormalizeTo(&n, &v);
			vec3_t e = { (nn->x * radius + x) - px, (nn->y * radius + y) - py, (nn->z * radius + z) - pz };
			if (Vec3_Angle(&e, &fwd) >= (double)fov) {
				return 0;
			}
		}
	}
	if (sector) {
		vec3_t p = { x, y, z };
		int s = Sector_At(&p);
		if (s >= 0 && !Sector_Visible(s)) {
			return 0;
		}
	}
	return 1;
}

static uint32_t FX_ItemBlinkRamp[9] = { 0x80669eff, 0x84669eff, 0x94669eff, 0xa4669eff, 0xb9669eff, 0xcd669eff, 0xe8669eff, 0xff669eff, 0xff669eff };

void FX_ItemBlink(cEntity *e) {
	CGameModel *m = Obj_GetModel(e);

	CHECK(m && CGameModel_IsLoaded(m));
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXItemBlinkFile);
	cDebris *d = cDebrisSys_GetIdle(sys);
	CGameModel_SetSortZ(Debris_Model(d), -0.05f, 0.0f);
	d->life = 1.0f;
	if (!cDebrisSys_IsIdle(sys, d)) {
		cDebrisSys_Drop(sys, d);
	}
	cObjElementFXParticleSystem *el = Debris_Element(d);
	if (!el) {
		return;
	}
	CGameParticles *sp = &el->particles;
	if (!el->on) {
		el->move = 0;
		el->on = 1;
	}
	float r = CGameModel_Radius(m) * 4.0f;
	sp->birthRate = 0;
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, FX_ItemBlinkRamp);
	float life = cTimer_GetSpeed(&g_pWorld->timer) * 0.12f;
	sp->sizeLife = 0.4f;
	sp->sizeEnd = r;
	sp->spriteIni = 5;
	sp->spriteEnd = 5;
	sp->lifeTime = life;
	sp->sizeIni = r * 0.5f;
	sp->initVelMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	sp->initVelMax = sp->initVelMin;
	sp->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	sp->initAccMax = sp->initAccMin;
	sp->rotIni = sp->rotEnd = FX_RandF((double)6.28318548f, 0.0);
	float q = r / 10.0f;
	sp->rotVar = (float)(FX_Rnd() * (double)3.14159274f - (double)1.57079637f);
	for (int k = 0; k < 8; k++) {
		vec3_t tmp;
		float ox = fx_corners[k].x * q, oy = fx_corners[k].y * q, oz = q * fx_corners[k].z;
		const vec3_t *c = Obj_GetViewPos(e, &tmp);
		sp->initPosMin.x = c->x + ox;
		sp->initPosMin.y = c->y + oy;
		sp->initPosMin.z = c->z + oz;
		sp->initPosMax = sp->initPosMin;
		CGameParticles_Apply(sp);
		CGameParticles_EmitN(sp, 1, 1);
	}
}

static void cObjElementFXItemFade_Start(cObjElementFXItemFade *self, cEntity *e);
static void cObjElementFXItemTake_Start(cObjElementFXItemTake *self, cEntity *e);
void FX_ItemFade(cEntity *e, int mode, float scale, cEntity *target) {
	cObjElementFXItemFade *fade = FX_DropNew(g_FXItemFadeFile, 0.5f);

	if (fade) {
		if (!fade->started) {
			fade->started = 1;
		}
		fade->mode = mode;
		fade->duration = 0.15f;
		fade->scale = scale;
		cObjElementFXItemFade_Start(fade, e);
	}
	if (!target) {
		return;
	}
	CHECK(Entity_IsType(target, "Obj"));
	if (!g_IsMapOutDoor) {
		cObjElementFXItemTake *take = FX_DropNew(g_FXItemTakeFile, 1.25f);
		if (take) {
			if (!take->started) {
				take->started = 1;
			}
			take->mode = mode;
			take->unused_634 = 1.0f;
			take->scale = scale;
			cObjElementFXItemTake_Start(take, e);
			take->target = target;
		}
	}
	/* in split screen the scorer of the view whose camera is the camera of the target */
	int player = 0;
	if (g_SplitScreenGame && Obj(target)->cam) {
		for (player = 0;; player++) {
			if (player >= 4) {
				return;
			}
			const char *a = Obj(target)->cam->name;
			if (FX_NameEq(a, cWorld_GetCamName(g_pWorld, player))) {
				break;
			}
		}
	}
	cScorer *sc = Scorer_Get(player);
	if (sc) {
		ScorerFade_Start(&sc->fade, 0x40c0c0ff, 0x2020ff, 0.22f);
	}
}

void FX_Sputnik(cEntity *e, const char *name) {
	if (!crt_strncmp(name, "CharacterSputnik", 16)) {
		if (!CGameModel_IsLoaded(Obj_GetModel(e)) || strcmp(name, "CharacterSputnikSceneMecanicFinal")) {
			return;
		}
		cObjElementFXSputnik *el = FX_DropNew(g_FXCharacterSputnikFile, 4.0f);
		if (!el) {
			return;
		}
		if (!el->started) {
			el->started = 1;
		}
		FXSputnik_Start(el, e);
		FXSputnik_SetA(el, 3.0f);
		FXSputnik_SetB(el, 5.0f);
		return;
	}
	if (!crt_strncmp(name, "ShipEditBuild", 13)) {
		if (!CGameModel_IsLoaded(Obj_GetModel(e))) {
			return;
		}
		FXElemHead *el = FX_DropNew(g_FXShipEditBuildFile, 0.45f);
		if (el) {
			if (!el->started) {
				el->started = 1;
			}
			FXMisc_SetEntity(el, e);
			el->fC = 0.45f;
		}
	}
}

byte FX_InRange(float x, float from, float to, float *frac) {
	CHECK(from <= to);
	if (frac) {
		float q = (x - from) / (to - from);
		*frac = 0.0f > q ? 0.0f : Float_MinSS(1.0f, q);
	}
	if (from > x || x > to) {
		return 0;
	}
	return 1;
}

vec3_t *FX_RandomVec(vec3_t *out, const vec3_t *lo, const vec3_t *hi) {
	float x = lo->x, hx = hi->x;

	if (x != hx) {
		x = FX_RandRange(x, hx);
	}
	float y = lo->y, hy = hi->y;
	if (y != hy) {
		y = FX_RandRange(y, hy);
	}
	float z = lo->z, hz = hi->z;
	if (z != hz) {
		z = FX_RandRange(z, hz);
	}
	out->x = x;
	out->y = y;
	out->z = z;
	return out;
}

vec3_t *FX_RandomDir(vec3_t *out) {
	float a = FX_RandF((double)6.28318548f, 0.0);
	float b = FX_RandF((double)6.28318548f, 0.0);
	float c = FX_RandF((double)6.28318548f, 0.0);
	const vec3_t one = { 1.0f, 1.0f, 1.0f };
	vec3_t rot = { c, b, a };

	Vec3_Rotate(out, &one, &rot);
	return out;
}

/* the elements: Update Alive Attach Reset and the starts */

/* the Alive of the elements that only step the debris */
byte FXMisc_Alive(cObjElementFX *base, cDebris *d, cDebrisSys *sys) {
	void *self = (void *)base;
	(void)self;
	return cDebris_Update(d, sys);
}

/* the methods without code (ret 4) */
static void FXMisc_Nop(cObjElementFX *base, cDebris *arg) {
	void *self = (void *)base;
	(void)self;
	(void)arg;
}

static void cObjElementFXItemFade_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXItemFade *self = (cObjElementFXItemFade *)base;
	CGameModel *m;
	cEntity *e = self->ent;

	(void)dt;
	if (e && FX_Gone(e)) {
		m = Obj_GetModel(e);
		CHECK(CGameModel_IsLoaded(m));
		FX_EndFade(m, self->mode);
		self->ent = NULL;
	}
	if (!self->ent || !self->ent->active) {
		return;
	}
	float t = self->t + cTimer_GetDelta(&g_pWorld->timer);
	float f = 0.0f;
	self->t = t;
	float q = t / self->duration;
	if (!(0.0f > q)) {
		f = Float_MinSS(1.0f, q);
	}
	if (self->mode == 2) {
		f = 1.0f - f;
	}
	m = Obj_GetModel(self->ent);
	CHECK(CGameModel_IsLoaded(m));
	if (!self->done) {
		if (0.1f > self->t) {
			if (!CGameModel_IsEnabled(m)) {
				CGameModel_Enable(m);
			}
			CGameModel_SetEffect(m, MODEL_EFFECT_FADE_LIT);
			CGameModel_SetEffectParam(m, 0, FX_Bits(f));
		} else {
			FX_EndFade(m, self->mode);
			self->done = 1;
		}
	}
	vec3_t tmp;
	CGameModel_SetSortZ(Debris_Model(d), -6.0f, 0.0f);
	CGameModel_SetPos(Debris_Model(d), CGameModel_GetPos(m, &tmp));
}

static void cObjElementFXModelFade_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXModelFade *self = (cObjElementFXModelFade *)base;
	float f;

	if (!d || !CGameModel_IsLoaded(Debris_Model(d))) {
		return;
	}
	float t = cTimer_GetDelta(&g_pWorld->timer) + self->t;
	self->t = t;
	float rem = d->life - dt;
	if (self->duration > t) {
		f = t / self->duration;
	} else if (0.0f >= rem) {
		f = 0.0f;
	} else if (self->fadeOut > rem) {
		f = rem / self->fadeOut;
	} else {
		f = 1.0f;
	}
	CGameModel_SetEffect(Debris_Model(d), MODEL_EFFECT_FADE_LIT);
	CGameModel_SetEffectParam(Debris_Model(d), 0, FX_Bits(1.0f - f));
	cDebris_Move(d, dt);
}

static void cObjElementFXModelNoise_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXModelNoise *self = (cObjElementFXModelNoise *)base;
	cEntity *e = self->ent;

	(void)d;
	(void)dt;
	if (e && FX_Gone(e)) {
		Obj_SetEffect(e, MODEL_EFFECT_NONE);
		self->ent = NULL;
	}
	if (!self->ent || !self->ent->active) {
		return;
	}
	float t = self->t + cTimer_GetDelta(&g_pWorld->timer);
	self->t = t;
	float f = self->duration > t ? t / self->duration : 1.0f;
	Obj_SetEffect(self->ent, MODEL_EFFECT_NOISE);
	Obj_SetEffectParam(self->ent, 0, 0.45f - f * 0.45f);
}

/* ParticleSystem and SistPartDuDv */
static void cObjElementFXParticleSystem_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXParticleSystem *self = (cObjElementFXParticleSystem *)base;
	if (self->move) {
		cDebris_Move(d, dt);
	}
}

static void cObjElementFXTrail_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXTrail *self = (cObjElementFXTrail *)base;
	if (!self->on) {
		cDebris_Move(d, dt);
		return;
	}
	if (self->stopped) {
		CGameTrail_Shrink(&self->trail);
		return;
	}
	cDebris_Move(d, dt);
	const vec3_t *pos = &d->drop[0], *vel = &d->drop[2];
	vec3_t p = { pos->x + vel->x * dt, pos->y + vel->y * dt, pos->z + vel->z * dt };
	if (!self->stopped && !CGameTrail_IsRender(&self->trail)) {
		CGameTrail_AddPos(&self->trail, p);
		return;
	}
	float dx = p.x - self->last.x, dy = p.y - self->last.y, dz = p.z - self->last.z;
	float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	if (dist > self->minDist) {
		self->last = p;
		CGameTrail_AddPos(&self->trail, p);
	} else {
		CGameTrail_SetLastPos(&self->trail, p);
	}
}

static byte cObjElementFXFadeOutTrail_Alive(cObjElementFX *base, cDebris *d, cDebrisSys *sys) {
	cObjElementFXFadeOutTrail *self = (cObjElementFXFadeOutTrail *)base;
	(void)d;
	(void)sys;
	if (self->running || !(self->t >= self->duration)) {
		return 1;
	}
	if (self->on) {
		CGameTrail_Clear(&self->trail);
	}
	return 0;
}

static byte cObjElementFXModelFade_Alive(cObjElementFX *base, cDebris *d, cDebrisSys *sys) {
	cObjElementFXModelFade *self = (cObjElementFXModelFade *)base;
	(void)self;
	if (!cDebris_Update(d, sys)) {
		CGameModel_SetEffect(Debris_Model(d), MODEL_EFFECT_NONE);
		return 0;
	}
	return 1;
}

static byte cObjElementFXModelNoise_Alive(cObjElementFX *base, cDebris *d, cDebrisSys *sys) {
	cObjElementFXModelNoise *self = (cObjElementFXModelNoise *)base;
	if (!cDebris_Update(d, sys)) {
		if (self->ent) {
			Obj_SetEffect(self->ent, MODEL_EFFECT_NONE);
		}
		return 0;
	}
	return 1;
}

static byte cObjElementFXTrail_Alive(cObjElementFX *base, cDebris *d, cDebrisSys *sys) {
	cObjElementFXTrail *self = (cObjElementFXTrail *)base;
	if (!self->on) {
		return cDebris_Update(d, sys);
	}
	if (cDebris_Update(d, sys)) {
		return 1;
	}
	if (!self->stopped && CGameTrail_IsRender(&self->trail)) {
		CGameModel_Disable(Debris_Model(d));
		self->stopped = 1;
		return 1;
	}
	self->stopped = 0;
	CGameTrail_Clear(&self->trail);
	return 0;
}

static void cObjElementFXItemFade_Attach(cObjElementFX *base, cDebris *d) {
	cObjElementFXItemFade *self = (cObjElementFXItemFade *)base;
	FX_AttachDummy(&self->particles, Debris_Model(d));
}

static void cObjElementFXItemTake_Attach(cObjElementFX *base, cDebris *d) {
	cObjElementFXItemTake *self = (cObjElementFXItemTake *)base;
	FX_AttachDummy(&self->particles, Debris_Model(d));
}

/* Attach and Reset of ModelFade and ModelNoise: the time back to 0 */
static void FXMisc_Clear4(cObjElementFX *base, cDebris *arg) {
	void *self = (void *)base;
	(void)arg;
	((cObjElementFXModelFade *)self)->t = 0.0f;
}

static void FXMisc_Clear8(cObjElementFX *base, cDebris *arg) {
	void *self = (void *)base;
	(void)arg;
	((cObjElementFXModelNoise *)self)->t = 0.0f;
}

static void FX_ParticleSystemAttach(cObjElementFXParticleSystem *self, cDebris *d, const char *node) {
	CGameModel *m = Debris_Model(d);

	CHECK(CGameModel_IsLoaded(m));
	int i = CGameModel_FindRenderNode(m, node);
	CHECK(i > -1);
	CGameParticles_Attach(&self->particles, m, i);
	CHECK(self->particles.ParticleSystem);
}

static void cObjElementFXParticleSystem_Attach(cObjElementFX *base, cDebris *d) {
	cObjElementFXParticleSystem *self = (cObjElementFXParticleSystem *)base;
	FX_ParticleSystemAttach(self, d, "Dummy01");
}

static void cObjElementFXParticleSystemDuDv_Attach(cObjElementFX *base, cDebris *d) {
	cObjElementFXParticleSystemDuDv *self = (cObjElementFXParticleSystemDuDv *)base;
	FX_ParticleSystemAttach(self, d, "Dummy01 (+dudv_pass)");
}

/* the spark slots all free */
static void FXSparks_ResetSlots(cObjElementFXSparks *self) {
	self->numActive = 0;
	self->numFree = FXSPARKS_MAXSPARKS;
	for (int i = 0; i < FXSPARKS_MAXSPARKS; i++) {
		self->freeList[i] = i;
		self->activeList[i] = -1;
		self->active[i] = 0;
	}
}

static void cObjElementFXSparks_Attach(cObjElementFX *base, cDebris *d) {
	cObjElementFXSparks *self = (cObjElementFXSparks *)base;
	FX_AttachDummy(&self->particles, Debris_Model(d));
	FXSparks_ResetSlots(self);
	for (int i = 0; i < FXSPARKS_MAXSPARKS; i++) {
		CGameTrail *t = &self->trails[i];
		CGameTrail_SetMaxTrails(t, 12);
		CGameTrail_SetTexture(t, 0);
		CGameTrail_SetFade(t, 1);
		CGameTrail_SetScale(t, 1.0f, 0.0f);
		self->sprite[i] = -1;
	}
}

static void cObjElementFXFadeOutTrail_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXFadeOutTrail *self = (cObjElementFXFadeOutTrail *)base;
	(void)arg;
	self->t = 0.0f;
	if (self->on) {
		CGameTrail_Clear(&self->trail);
		CGameTrail_SetTransp(&self->trail, 0xff);
	}
	self->running = 1;
}

/* the original also checks the address of the particles member, which is never NULL */
static void cObjElementFXItemFade_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXItemFade *self = (cObjElementFXItemFade *)base;
	(void)arg;
	self->t = 0.0f;
	self->mode = 0;
	self->ent = NULL;
	CHECK(self->particles.ParticleSystem);
	self->done = 0;
}

static void cObjElementFXItemTake_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXItemTake *self = (cObjElementFXItemTake *)base;
	(void)arg;
	self->t = 0.0f;
	self->emitT = 0.0f;
	self->mode = 0;
	self->ent = NULL;
	CHECK(self->particles.ParticleSystem);
	self->done = 0;
}

static void cObjElementFXSparks_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXSparks *self = (cObjElementFXSparks *)base;
	(void)arg;
	self->time = 0.0f;
	self->dt = 0.0f;
	self->trailT = 0.0f;
	FXSparks_ResetSlots(self);
	for (int i = 0; i < FXSPARKS_MAXSPARKS; i++) {
		CGameTrail_Clear(&self->trails[i]);
		CGameParticles_Kill(&self->particles, i);
		self->sprite[i] = -1;
	}
}

static void cObjElementFXTrail_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXTrail *self = (cObjElementFXTrail *)base;
	(void)arg;
	CGameTrail_Clear(&self->trail);
}

/* the setters of the effects started by name (the start of their classes, FXElemHead) */
void FXMisc_SetEntity(void *self, cEntity *e) {
	((FXElemHead *)self)->ent = e;
	CHECK(CGameModel_IsLoaded(Obj_GetModel(e)));
}

void FXMisc_SetF8(void *self, float v) {
	((FXElemHead *)self)->w8.f = v;
}

void FXMisc_SetFC(void *self, float v) {
	((FXElemHead *)self)->fC = v;
}

/* a particle of the blink and fade effects: still, at pos, a random turn */
static void FX_StillAt(CGameParticles *sp, const vec3_t *pos) {
	sp->initPosMin = *pos;
	sp->initPosMax = sp->initPosMin;
	sp->initVelMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	sp->initVelMax = sp->initVelMin;
	sp->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	sp->initAccMax = sp->initAccMin;
	sp->rotVar = 0.0f;
	sp->rotIni = sp->rotEnd = FX_RandF((double)6.28318548f, 0.0);
}

static uint32_t FX_EmitFadeRamps[27] = {
	0x2043696d, 0x30436972, 0x40436976, 0x5c42687b, 0x8042687f, 0xa4426885, 0xc0426886, 0xff426886, 0xff426886,
	0x20000000, 0x30000000, 0x40102534, 0x5c2046d1, 0x80205aff, 0xa42048ff, 0xc0104489, 0xff000000, 0xff000000,
	0x30c8c8ff, 0x40f0f0ff, 0x20c4c4c4, 0x808080ff, 0xa44040ff, 0xc02020ff, 0xe10000ff, 0xff0000ff, 0xff0000ff,
};

/* the particles applied and one emitted with one of the three fade ramps */
static void FX_EmitFade(CGameParticles *sp) {
	CGameParticles_Apply(sp);
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, FX_EmitFadeRamps + crt_rand() % 3 * 9);
	CGameParticles_EmitN(sp, 1, 1);
}

/* the fade of the model of e starts with the blink particles around it */
static void cObjElementFXItemFade_Start(cObjElementFXItemFade *self, cEntity *e) {
	CGameParticles *sp = &self->particles;
	vec3_t c;

	self->ent = e;
	if (!e) {
		return;
	}
	CGameModel *m = Obj_GetModel(e);
	CHECK(CGameModel_IsLoaded(m));
	if (self->mode == 1) {
		CGameModel_SetEffect(m, MODEL_EFFECT_NONE);
		CGameModel_Enable(m);
	} else if (self->mode == 2) {
		CGameModel_SetEffect(m, MODEL_EFFECT_FADE_LIT);
		CGameModel_SetEffectParam(m, 0, 0);
		CGameModel_Enable(m);
	}
	Obj_GetViewPos(self->ent, &c);
	float r = CGameModel_Radius(m);
	sp->birthRate = 0;
	FX_StillAt(sp, &c);
	sp->lifeTime = FX_RandF((double)0.0600000024f, (double)0.379999995f);
	float s = FX_RandF((double)0.099999994f, (double)0.300000012f) * r;
	sp->sizeIni = s + s;
	s = FX_RandF((double)0.200000048f, 1.0) * r;
	sp->sizeLife = 0.2f;
	sp->spriteIni = 1;
	sp->spriteEnd = 1;
	sp->sizeEnd = s + s;
	FX_EmitFade(sp);

	FX_StillAt(sp, &c);
	sp->lifeTime = FX_RandF((double)0.0600000024f, (double)0.379999995f);
	s = FX_RandF((double)0.099999994f, (double)0.400000006f) * r;
	sp->sizeIni = s + s;
	s = FX_RandF(0.5, (double)1.29999995f) * r;
	sp->sizeLife = 0.2f;
	sp->spriteIni = 5;
	sp->spriteEnd = 5;
	sp->sizeEnd = s + s;
	FX_EmitFade(sp);

	float lo = r * 0.3f, range = r * 0.4f - lo;
	for (int k = 0; k < 8; k++) {
		float q = (float)(FX_Rnd() * (double)range + (double)lo);
		vec3_t p = { fx_corners[k].x * q + c.x, fx_corners[k].y * q + c.y, q * fx_corners[k].z + c.z };
		FX_StillAt(sp, &p);
		sp->lifeTime = FX_RandF((double)0.0499999821f, (double)0.400000006f);
		sp->sizeIni = FX_RandF((double)0.100000001f, (double)0.100000001f) * r * 1.8f;
		sp->sizeLife = 0.2f;
		sp->spriteIni = 5;
		sp->spriteEnd = 5;
		sp->sizeEnd = FX_RandF((double)0.399999976f, (double)1.20000005f) * r * 1.8f;
		FX_EmitFade(sp);
	}
}

/* the random lines of particles from e (to the target set after it) */
static void cObjElementFXItemTake_Start(cObjElementFXItemTake *self, cEntity *e) {
	vec3_t tmp;

	self->ent = e;
	if (!e) {
		return;
	}
	CGameModel *m = Obj_GetModel(e);
	CHECK(CGameModel_IsLoaded(m));
	CGameModel_Radius(m);
	for (int g = 0; g < FXITEMTAKE_LINES; g++) {
		float rr = FX_RandF(100.0, 50.0);
		const vec3_t *dir = FX_RandomDir(&tmp);
		float s = self->scale;
		self->dir[g].x = dir->x * rr * s;
		self->dir[g].y = dir->y * rr * s;
		self->dir[g].z = dir->z * rr * s;
		for (int j = 0; j < FXITEMTAKE_POINTS; j++) {
			int i = g * FXITEMTAKE_POINTS + j;
			if (j == 0) {
				self->phase[i] = 0.0f;
			} else {
				self->phase[i] = (float)(FX_Rnd() * (double)0.239999995f - (double)0.119999997f);
			}
			self->speed[i] = FX_RandF((double)0.149999976f, (double)1.45000005f);
			float r2 = FX_RandF(4.0, 1.5);
			s = self->scale;
			dir = FX_RandomDir(&tmp);
			self->offset[i].x = r2 * (dir->x * s);
			self->offset[i].y = r2 * (dir->y * s);
			self->offset[i].z = r2 * (dir->z * s);
			uint32_t a = (uint32_t)(crt_rand() % 64 + 0x28) << 8;
			a |= (uint32_t)(crt_rand() % 0x30 + 0x20) << 16;
			self->color[i] = (uint32_t)(crt_rand() % 0x5c + 0x5c) | a;
		}
	}
}

static uint32_t cObjElementFXItemTake_UpdateTable[56] = {
	0x2040406d, 0x30404072, 0x40273476, 0x5c23307b, 0x801c2a7f, 0xa4162285, 0xc0101986, 0xff0c1086, 0xff080886,
	0x20050540, 0x30101080, 0x402020b4, 0x5c3030d1, 0x804040ff, 0xa44040ff, 0xc0202089, 0xff000000, 0xff000000,
	0x306464ff, 0x405050ff, 0x204242c4, 0x803030ff, 0xa41818ff, 0xc00606ff, 0xe10000ff, 0xff0000ff, 0xff0000ff, 0,
	0x40ff801e, 0x4bff6019, 0x60c0400a, 0x70802005, 0x80400a00, 0xa40d0500, 0xc0050000, 0xff000000, 0xff000000,
	0x40ffeb5a, 0x4bffd741, 0x60c0a41e, 0x70805c0f, 0x80402305, 0xa40d0500, 0xc0050000, 0xff000000, 0xff000000,
	0x20dedeff, 0x4bbebeeb, 0x60a4a4dc, 0x705c5cbe, 0x8040406e, 0xa4202055, 0xc00f0f40, 0xff000000, 0xff000000,
};

static void cObjElementFXItemTake_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXItemTake *self = (cObjElementFXItemTake *)base;
	CGameParticles *sp = &self->particles;
	vec3_t c1, c2, tmp, coefs[7], pts[7], first;
	byte found = 0;

	(void)dt;
	if (self->ent && FX_Gone(self->ent)) {
		self->ent = NULL;
	}
	if (!self->ent || !self->ent->active) {
		return;
	}
	if (self->target && FX_Gone(self->target)) {
		self->target = NULL;
	}
	if (!self->target || !self->target->active) {
		return;
	}
	float delta = cTimer_GetDelta(&g_pWorld->timer);
	self->t = self->t + delta;
	self->emitT = self->emitT + delta;
	CGameModel *m = Obj_GetModel(self->ent);
	CHECK(m && CGameModel_IsLoaded(m));
	CGameModel_SetSortZ(Debris_Model(d), -7.0f, 0.0f);
	CGameModel_SetPos(Debris_Model(d), CGameModel_GetPos(m, &tmp));
	sp->birthRate = 0;
	int np = CGameParticles_NumParticles(sp);
	CGameModel_Radius(m);
	Obj_GetRadius(self->ent);
	Obj_GetRadius(self->target);
	Obj_GetViewPos(self->ent, &c1);
	Obj_GetViewPos(self->target, &c2);
	for (int k = 0; k < FXITEMTAKE_LINES; k++) {
		static const vec3_t arms[4] = {
			{ -225.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 225.0f }, { 225.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -225.0f }
		};
		vec3_t ang;
		float a = (float)(k - 2) * 15.0f;
		Obj_GetViewAng(self->target, &ang);
		pts[0] = c1;
		for (int n = 0; n < 4; n++) {
			vec3_t v = arms[n], o;
			v.y = a;
			float s = self->scale;
			const vec3_t *r = Vec3_Rotate(&o, &v, &ang);
			if (n == 0) {
				pts[1].x = r->x * s + c2.x;
			} else {
				pts[n + 1].x = s * r->x + c2.x;
			}
			pts[n + 1].y = r->y * s + c2.y;
			pts[n + 1].z = r->z * s + c2.z;
		}
		pts[5] = c2;
		float s0 = self->scale * 0.0f;
		pts[6].x = s0 + c2.x;
		pts[6].y = self->scale * 350.0f + c2.y;
		pts[6].z = s0 + c2.z;
		CGameTrail_BezierCoefs(&self->trail, 7, pts, coefs);
		CGameTrail_Reset(&self->trail);
		uint32_t col = 0x8000;
		for (int i = 0; i < CGameTrail_GetMaxTrails(&self->trail); i++, col += 0x1000) {
			vec3_t p;
			float t = (float)i / (float)CGameTrail_GetMaxTrails(&self->trail);
			CGameTrail_BezierPoint(&self->trail, 7, coefs, &p, t * t * (2.0f - t));
			CGameTrail_Add(&self->trail, p, 8.0f, col | 0x40000000);
		}
		for (int j = 0; j < FXITEMTAKE_POINTS; j++) {
			int i = k * FXITEMTAKE_POINTS + j;
			int pi = np - k * FXITEMTAKE_POINTS - j - 1;
			float v = self->speed[i] * delta + self->phase[i];
			self->phase[i] = v;
			if (0.0f > v || v >= 1.0f) {
				CGameParticles_Kill(sp, pi);
				continue;
			}
			int alpha = 0;
			if (0.1f >= v) {
				alpha = 0xff - (int)(v * 10.0f * 255.0f);
			} else if (v >= 0.8f) {
				alpha = (int)((v - 0.8f) * 5.0f * 255.0f);
			}
			vec3_t p;
			CGameTrail_BezierPoint(&self->trail, 7, coefs, &p, v * v * (2.0f - v));
			float px = p.x + self->offset[i].x;
			float py = self->offset[i].y + p.y;
			float pz = self->offset[i].z + p.z;
			if (k == 0 && j == 0 && 1.0f > v) {
				first = p;
				found = 1;
			}
			float rot = (float)j / 14.0f * 6.2831855f;
			float mult = 1.0f;
			int sprite;
			switch (pi % 5) {
			case 1:
				mult = 1.75f;
				sprite = 5;
				break;
			case 2:
				sprite = 6;
				break;
			case 3:
				sprite = (pi & 1) + 9;
				break;
			case 4:
				sprite = 0x2b;
				break;
			default:
				sprite = 2;
				break;
			}
			CGameParticles_SetParticle(sp, pi, px, py, pz, self->scale * 35.0f * mult,
				(self->color[i] & 0xffffff) | (uint32_t)alpha << 24, sprite, rot);
		}
	}
	if (!found || !(self->emitT > 0.032f)) {
		return;
	}
	self->emitT = 0.0f;
	for (int n = 4; n; n--) {
		float rr = FX_RandF(12.0, 6.0);
		const vec3_t *dir = FX_RandomDir(&tmp);
		float s = self->scale;
		sp->initPosMin.x = first.x + rr * dir->x * s;
		sp->initPosMin.y = first.y + dir->y * rr * s;
		sp->initPosMin.z = first.z + dir->z * rr * s;
		sp->initPosMax = sp->initPosMin;
		float r1 = FX_RandF(4.0, -2.0);
		float r2 = FX_RandF(10.0, 15.0);
		float r3 = FX_RandF(4.0, -2.0);
		s = self->scale;
		sp->initVelMin.x = r3 * s;
		sp->initVelMin.y = r2 * s;
		sp->initVelMin.z = r1 * s;
		sp->initVelMax = sp->initVelMin;
		float az = self->scale * 0.0f;
		sp->initAccMin.y = self->scale * -2180.0f;
		sp->initAccMin.x = az;
		sp->initAccMin.z = az;
		sp->initAccMax = sp->initAccMin;
		sp->lifeTime = FX_RandF((double)0.169999987f, (double)0.280000001f);
		sp->rotIni = sp->rotEnd = FX_RandF((double)6.28318548f, 0.0);
		sp->rotVar = (float)(FX_Rnd() * (double)3.14159274f - (double)1.57079637f);
		sp->sizeIni = FX_RandF((double)0.450000048f, (double)0.649999976f) * 32.0f * self->scale;
		float se = FX_RandF((double)0.219999969f, (double)0.360000014f) * 32.0f * self->scale;
		sp->sizeLife = 1.0f;
		sp->sizeEnd = se;
		switch (crt_rand() % 3) {
		case 0:
			sp->spriteIni = 2;
			break;
		case 1:
			sp->spriteIni = 5;
			break;
		case 2:
			sp->spriteIni = 9;
			break;
		}
		sp->spriteEnd = sp->spriteIni;
		CGameParticles_Apply(sp);
		CGameParticles_SetColorRampOn(sp, 1);
		CGameParticles_SetColorRamp(sp, cObjElementFXItemTake_UpdateTable + crt_rand() % 3 * 9);
		CGameParticles_EmitDt(sp, 1, 1, FX_RandF((double)0.100000001f, 0.0));
	}
}

static void cObjElementFXSparks_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXSparks *self = (cObjElementFXSparks *)base;
	(void)d;
	(void)dt;
	float delta = cTimer_GetDelta(&g_pWorld->timer);
	float fdt = self->dt + delta;
	self->trailT = self->trailT + delta;
	self->dt = fdt;
	self->time = fdt + self->time;
	for (int k = 0; k < self->numActive; k++) {
		int i = self->activeList[k];
		CHECK(i >= 0 && i < FXSPARKS_MAXSPARKS);
		if (self->delay[i] > 0.0f) {
			self->delay[i] = self->delay[i] - self->dt;
			continue;
		}
		if (!(self->life[i] > 0.0f)) {
			if (self->active[i]) {
				self->active[i] = 0;
				self->activeList[k] = self->activeList[self->numActive - 1];
				self->numActive--;
				self->freeList[self->numFree] = i;
				self->numFree++;
			}
			continue;
		}
		float life = self->life[i] - self->dt;
		self->life[i] = life;
		CGameTrail *trail = &self->trails[i];
		if (!(life > 0.0f)) {
			CGameTrail_Clear(trail);
			if (self->sprite[i] >= 0) {
				CGameParticles_Kill(&self->particles, i);
			}
			continue;
		}
		float t = self->dt;
		vec3_t *acc = &self->acc[i], *vel = &self->vel[i], *pos = &self->pos[i];
		acc->x = acc->x + self->jerk[i].x * t;
		acc->y = acc->y + self->jerk[i].y * t;
		acc->z = acc->z + self->jerk[i].z * t;
		t = self->dt;
		float vx = t * acc->x + vel->x;
		vel->x = vx;
		vel->y = vel->y + acc->y * t;
		vel->z = vel->z + acc->z * t;
		t = self->dt;
		float py = vel->y * t + pos->y;
		float pz = vel->z * t + pos->z;
		pos->x = pos->x + vx * t;
		pos->y = py;
		pos->z = pz;
		if (self->trailT > 0.016f) {
			if (self->noise[i] > 0.0f) {
				vec3_t n;
				FX_RandomDir(&n);
				float a = self->noise[i], tt = self->trailT;
				pos->x = n.x * a * tt + pos->x;
				pos->y = n.y * a * tt + pos->y;
				pos->z = n.z * a * tt + pos->z;
			}
			CGameTrail_Add(trail, *pos, self->size[i], self->color[i]);
		} else {
			CGameTrail_SetLast(trail, pos, NULL, NULL);
		}
		float f = self->life[i] / self->lifeMax[i];
		float fa = 0.0f;
		if (!(f > 1.0f)) {
			fa = 255.0f - (0.0f > f ? 0.0f : f * 255.0f);
		}
		int alpha = (int)fa;
		CGameTrail_SetTransp(trail, (byte)alpha);
		if (self->sprite[i] >= 0) {
			CGameParticles_SetParticle(&self->particles, i, pos->x, pos->y, pos->z, self->size[i],
				(self->color[i] & 0xffffff) | (uint32_t)alpha << 24, self->sprite[i], 0.0f);
		}
	}
	self->dt = 0.0f;
	if (self->trailT > 0.016f) {
		self->trailT = 0.0f;
	}
}

void cObjElementFXSparks_Emit(cObjElementFXSparks *self, int n, float px, float py, float pz, float dx,
	float dy, float dz, float sx, float sy, float sz, float jx, float jy, float jz, float speedMin, float speedMax,
	float sizeMin, float sizeMax, float scaleHead, float scaleTail, float lifeMin, float lifeMax, float lmMin,
	float lmMax, float delayMin, float delayMax, uint32_t color, float noiseMin, float noiseMax, float segMin,
	float segMax, int sprite) {
	float len = (float)__builtin_sqrt((double)(dx * dx + dy * dy + dz * dz));
	vec3_t dir;

	if (len == 0.0f) {
		dir = (vec3_t){ dx, dy, dz };
	} else {
		float inv = 1.0f / len;
		dir = (vec3_t){ dx * inv, dy * inv, dz * inv };
	}
	for (; n > 0; n--) {
		if (self->numFree <= 0) {
			continue;
		}
		self->numFree--;
		int i = self->freeList[self->numFree];
		CHECK(i >= 0 && i < FXSPARKS_MAXSPARKS);
		self->activeList[self->numActive] = i;
		self->numActive++;
		self->active[i] = 1;
		self->pos[i] = (vec3_t){ px, py, pz };
		float rz = FX_RandRange(-sz, sz);
		float ry = FX_RandRange(-sy, sy);
		float rx = FX_RandRange(-sx, sx);
		vec3_t rot = { rx, ry, rz }, v;
		Vec3_Rotate(&v, &dir, &rot);
		float speed = FX_RandRange(speedMin, speedMax);
		self->vel[i] = (vec3_t){ v.x * speed, v.y * speed, v.z * speed };
		self->acc[i] = (vec3_t){ 0.0f, 0.0f, 0.0f };
		self->jerk[i] = (vec3_t){ jx, jy, jz };
		self->size[i] = FX_RandRange(sizeMin, sizeMax);
		self->life[i] = FX_RandRange(lifeMin, lifeMax);
		self->lifeMax[i] = FX_RandRange(lmMin, lmMax);
		float delay = FX_RandRange(delayMin, delayMax);
		self->color[i] = color;
		self->delay[i] = delay;
		self->noise[i] = FX_RandRange(noiseMin, noiseMax);
		self->segments[i] = (int)FX_RandRange(segMin, segMax);
		if (self->segments[i] > 12) {
			self->segments[i] = 12;
		}
		CGameTrail_SetSegments(&self->trails[i], (byte)self->segments[i]);
		CGameTrail_SetScale(&self->trails[i], scaleHead, scaleTail);
		self->sprite[i] = sprite;
	}
}

/*
 * The constructors and destructors of the FX elements of the character effects (after
 * World/CollisionGrid.cpp) and of the FX scenes (..). Only their members are known: particle systems
 * arrays of trails and lens flares, the fields they clear.
 */

static inline void FX_TrailsCtor(CGameTrail *t, uint32_t n) {
	crt_eh_vector_ctor(t, sizeof(CGameTrail), n, (void *)CGameTrail_ctor, (void *)CGameTrail_dtor);
}

static inline void FX_TrailsDtor(CGameTrail *t, uint32_t n) {
	crt_eh_vector_dtor(t, sizeof(CGameTrail), n, (void *)CGameTrail_dtor);
}

void cObjElementAbstract_Free(cObjElementFX *base) {
	cObjElementAbstract *self = (cObjElementAbstract *)base;
	cObjElement_dtor(self);
	operator_delete(self);
}

/* World/FX character effects */

static const cObjElementFXVtbl cObjElementFXCharacterConversion_vftable;
#define VT_FXCHARACTERCONVERSION (&cObjElementFXCharacterConversion_vftable)
static const cObjElementFXVtbl cObjElementFXCharacterConversor_vftable;
#define VT_FXCHARACTERCONVERSOR (&cObjElementFXCharacterConversor_vftable)
static const cObjElementFXVtbl cObjElementFXCharacterDeath_vftable;
#define VT_FXCHARACTERDEATH (&cObjElementFXCharacterDeath_vftable)
static const cObjElementFXVtbl cObjElementFXCharacterTeleport_vftable;
#define VT_FXCHARACTERTELEPORT (&cObjElementFXCharacterTeleport_vftable)
static const cObjElementFXVtbl cObjElementFXMaintenanceWork_vftable;
#define VT_FXMAINTENANCEWORK (&cObjElementFXMaintenanceWork_vftable)
static const cObjElementFXVtbl cObjElementFXMoneyTransfer_vftable;
#define VT_FXMONEYTRANSFER (&cObjElementFXMoneyTransfer_vftable)
static const cObjElementFXVtbl cObjElementFXSputnik_vftable;
#define VT_FXSPUTNIK (&cObjElementFXSputnik_vftable)

void *cObjElementFXCharacterConversion_ctor(void *self) {
	cObjElementFXCharacterConversion *s = self;

	FXObj_SetVtbl(self, VT_FXCHARACTERCONVERSION);
	CGameParticles_ctor(&s->particles);
	CGameParticles_ctor(&s->particles2);
	FX_TrailsCtor(s->trails, 5);
	s->started = 0;
	s->time = 0.0f;
	s->emitT = 0.0f;
	s->attacker = NULL;
	s->attacked = NULL;
	return self;
}

void *cObjElementFXCharacterConversor_ctor(void *self) {
	cObjElementFXCharacterConversor *s = self;

	FXObj_SetVtbl(self, VT_FXCHARACTERCONVERSOR);
	CGameParticles_ctor(&s->particles);
	FX_TrailsCtor(s->trails, 3);
	s->started = 0;
	s->time = 0.0f;
	s->emitT = 0.0f;
	s->phase = 0;
	s->duration = 0.0f;
	s->numTrails = 3;
	return self;
}

void *cObjElementFXCharacterDeath_ctor(void *self) {
	cObjElementFXCharacterDeath *s = self;

	FXObj_SetVtbl(self, VT_FXCHARACTERDEATH);
	CGameParticles_ctor(&s->particles);
	CGameParticles_ctor(&s->particles2);
	CGameParticles_ctor(&s->particles3);
	FX_TrailsCtor(s->trails, 8);
	s->started = 0;
	s->ent = NULL;
	return self;
}

void *cObjElementFXCharacterTeleport_ctor(void *self) {
	cObjElementFXCharacterTeleport *s = self;

	FXObj_SetVtbl(self, VT_FXCHARACTERTELEPORT);
	CGameParticles_ctor(&s->particles);
	s->started = 0;
	s->t = 0.0f;
	s->emitT = 0.0f;
	s->mode = 0;
	s->duration = 0.0f;
	return self;
}

void *cObjElementFXMaintenanceWork_ctor(void *self) {
	cObjElementFXMaintenanceWork *s = self;

	FXObj_SetVtbl(self, VT_FXMAINTENANCEWORK);
	CGameParticles_ctor(&s->particles);
	FX_TrailsCtor(s->trails, 2);
	s->started = 0;
	s->maintenance = NULL;
	s->ship = NULL;
	s->time = 0.0f;
	s->emitTime = 0.0f;
	s->nodeLaser = 0xffffffffu;
	s->sound = 0;
	return self;
}

void *cObjElementFXMoneyTransfer_ctor(void *self) {
	cObjElementFXMoneyTransfer *s = self;

	FXObj_SetVtbl(self, VT_FXMONEYTRANSFER);
	CGameParticles_ctor(&s->particles);
	s->started = 0;
	s->from = NULL;
	s->to = NULL;
	s->emitT = 0.0f;
	s->unused_c0 = 0.0f;
	s->unused_c4 = 0xffffffffu;
	return self;
}

void *cObjElementFXSputnik_ctor(void *self) {
	cObjElementFXSputnik *s = self;

	FXObj_SetVtbl(self, VT_FXSPUTNIK);
	CGameParticles_ctor(&s->particles);
	s->started = 0;
	s->ent = NULL;
	s->t = 0.0f;
	s->emitT = 0.0f;
	s->duration = 0.0f;
	s->minMove = 0.0f;
	return self;
}

static void cObjElementFXCharacterConversion_dtor(void *self) {
	cObjElementFXCharacterConversion *s = self;

	FX_TrailsDtor(s->trails, 5);
	CGameParticles_Release(&s->particles2);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(self);
}

static void cObjElementFXCharacterConversor_dtor(void *self) {
	cObjElementFXCharacterConversor *s = self;

	FX_TrailsDtor(s->trails, 3);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(self);
}

static void cObjElementFXCharacterDeath_dtor(void *self) {
	cObjElementFXCharacterDeath *s = self;

	FX_TrailsDtor(s->trails, 8);
	CGameParticles_Release(&s->particles3);
	CGameParticles_Release(&s->particles2);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(self);
}

static void cObjElementFXCharacterTeleport_dtor(void *self) {
	cObjElementFXCharacterTeleport *s = self;

	CGameParticles_Release(&s->particles);
	cObjElement_dtor(self);
}

static void cObjElementFXMaintenanceWork_dtor(void *self) {
	cObjElementFXMaintenanceWork *s = self;

	FX_TrailsDtor(s->trails, 2);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(self);
}

static void cObjElementFXMoneyTransfer_dtor(void *self) {
	cObjElementFXMoneyTransfer *s = self;

	CGameParticles_Release(&s->particles);
	cObjElement_dtor(self);
}

static void cObjElementFXSputnik_dtor(void *self) {
	cObjElementFXSputnik *s = self;

	CGameParticles_Release(&s->particles);
	cObjElement_dtor(self);
}

static void cObjElementFXCharacterConversion_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXCharacterConversion_dtor(self);
	operator_delete(self);
}

static void cObjElementFXCharacterConversor_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXCharacterConversor_dtor(self);
	operator_delete(self);
}

static void cObjElementFXCharacterDeath_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXCharacterDeath_dtor(self);
	operator_delete(self);
}

static void cObjElementFXCharacterTeleport_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXCharacterTeleport_dtor(self);
	operator_delete(self);
}

static void cObjElementFXMaintenanceWork_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXMaintenanceWork_dtor(self);
	operator_delete(self);
}

static void cObjElementFXMoneyTransfer_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXMoneyTransfer_dtor(self);
	operator_delete(self);
}

static void cObjElementFXSputnik_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXSputnik_dtor(self);
	operator_delete(self);
}

/* the FX scenes */

static const cObjElementFXVtbl cObjElementFXSceneBeatPickus_vftable;
#define VT_FXSCENEBEATPICKUS (&cObjElementFXSceneBeatPickus_vftable)
static const cObjElementFXVtbl cObjElementFXSceneBillArrives_A_vftable;
#define VT_FXSCENEBILLARRIVES_A (&cObjElementFXSceneBillArrives_A_vftable)
static const cObjElementFXVtbl cObjElementFXSceneBishops_vftable;
#define VT_FXSCENEBISHOPS (&cObjElementFXSceneBishops_vftable)
static const cObjElementFXVtbl cObjElementFXSceneDecontamination_vftable;
#define VT_FXSCENEDECONTAMINATION (&cObjElementFXSceneDecontamination_vftable)
static const cObjElementFXVtbl cObjElementFXSceneEnterMatrix_vftable;
#define VT_FXSCENEENTERMATRIX (&cObjElementFXSceneEnterMatrix_vftable)
static const cObjElementFXVtbl cObjElementFXSceneHumanCD_vftable;
#define VT_FXSCENEHUMANCD (&cObjElementFXSceneHumanCD_vftable)
static const cObjElementFXVtbl cObjElementFXSceneTheEndB_vftable;
#define VT_FXSCENETHEENDB (&cObjElementFXSceneTheEndB_vftable)
static const cObjElementFXVtbl cObjElementFXSceneYakuzzi_vftable;
#define VT_FXSCENEYAKUZZI (&cObjElementFXSceneYakuzzi_vftable)

void *cObjElementFXSceneBeatPickus_ctor(void *self) {
	cObjElementFXSceneBeatPickus *s = self;

	FXObj_SetVtbl(self, VT_FXSCENEBEATPICKUS);
	CGameParticles_ctor(&s->dust);
	CGameParticles_ctor(&s->debris);
	CGameParticles_ctor(&s->sparks);
	s->started = 0;
	s->ent = NULL;
	s->t = 0.0f;
	s->tPrev = 0.0f;
	s->dustT = 0.0f;
	s->debrisT = 0.0f;
	s->sparkT = 0.0f;
	s->hitT = 0.0f;
	return self;
}

void *cObjElementFXSceneBillArrives_A_ctor(void *self) {
	cObjElementFXSceneBillArrives_A *s = self;

	FXObj_SetVtbl(self, VT_FXSCENEBILLARRIVES_A);
	CGameLensFlare_ctor(&s->flare);
	CGameParticles_ctor(&s->particles);
	s->started = 0;
	s->ent = NULL;
	s->capsule = NULL;
	s->t = 0.0f;
	s->tPrev = 0.0f;
	s->jetT = 0.0f;
	s->smokeT = 0.0f;
	return self;
}

void *cObjElementFXSceneBishops_ctor(void *self) {
	cObjElementFXSceneBishops *s = self;

	FXObj_SetVtbl(self, VT_FXSCENEBISHOPS);
	CGameParticles_ctor(&s->dataCodec);
	CGameParticles_ctor(&s->glow);
	FX_TrailsCtor(s->trails, 12);
	s->started = 0;
	s->ent = NULL;
	s->detritus = NULL;
	s->t = 0.0f;
	s->master.x = 0.0f;
	s->master.y = 0.0f;
	s->master.z = 0.0f;
	return self;
}

void *cObjElementFXSceneDecontamination_ctor(void *self) {
	cObjElementFXSceneDecontamination *s = self;

	FXObj_SetVtbl(self, VT_FXSCENEDECONTAMINATION);
	CGameParticles_ctor(&s->particles);
	FX_TrailsCtor(s->trails, 40);
	s->started = 0;
	s->ent = NULL;
	s->target = NULL;
	s->t2 = 0.0f;
	s->t = 0.0f;
	return self;
}

void *cObjElementFXSceneEnterMatrix_ctor(void *self) {
	cObjElementFXSceneEnterMatrix *s = self;

	FXObj_SetVtbl(self, VT_FXSCENEENTERMATRIX);
	CGameParticles_ctor(&s->particles);
	FX_TrailsCtor(s->trails, 16);
	CGameLensFlare_ctor(&s->flare);
	s->started = 0;
	s->ent = NULL;
	s->t = 0.0f;
	s->tPrev = 0.0f;
	return self;
}

void *cObjElementFXSceneHumanCD_ctor(void *self) {
	cObjElementFXSceneHumanCD *s = self;

	FXObj_SetVtbl(self, VT_FXSCENEHUMANCD);
	CGameParticles_ctor(&s->particles);
	CGameParticles_ctor(&s->sparks);
	s->started = 0;
	s->ent = NULL;
	s->t = 0.0f;
	s->tPrev = 0.0f;
	return self;
}

void *cObjElementFXSceneTheEndB_ctor(void *self) {
	cObjElementFXSceneTheEndB *s = self;

	FXObj_SetVtbl(self, VT_FXSCENETHEENDB);
	CGameParticles_ctor(&s->particles);
	CGameLensFlare_ctor(&s->flare);
	s->started = 0;
	s->ent = NULL;
	s->bike = NULL;
	s->t = 0.0f;
	s->tPrev = 0.0f;
	return self;
}

void *cObjElementFXSceneYakuzzi_ctor(void *self) {
	cObjElementFXSceneYakuzzi *s = self;

	FXObj_SetVtbl(self, VT_FXSCENEYAKUZZI);
	CGameParticles_ctor(&s->particles);
	s->started = 0;
	s->ent = NULL;
	s->t = 0.0f;
	s->tPrev = 0.0f;
	s->emitT = 0.0f;
	return self;
}

static void cObjElementFXSceneBeatPickus_dtor(void *self) {
	cObjElementFXSceneBeatPickus *s = self;

	CGameParticles_Release(&s->sparks);
	CGameParticles_Release(&s->debris);
	CGameParticles_Release(&s->dust);
	cObjElement_dtor(self);
}

static void cObjElementFXSceneBillArrives_A_dtor(void *self) {
	cObjElementFXSceneBillArrives_A *s = self;

	FXObj_SetVtbl(self, VT_FXSCENEBILLARRIVES_A);
	CGameParticles_Release(&s->particles);
	CGameLensFlare_dtor(&s->flare);
	cObjElement_dtor(self);
}

static void cObjElementFXSceneBishops_dtor(void *self) {
	cObjElementFXSceneBishops *s = self;

	FX_TrailsDtor(s->trails, 12);
	CGameParticles_Release(&s->glow);
	CGameParticles_Release(&s->dataCodec);
	cObjElement_dtor(self);
}

static void cObjElementFXSceneDecontamination_dtor(void *self) {
	cObjElementFXSceneDecontamination *s = self;

	FX_TrailsDtor(s->trails, 40);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(self);
}

/* the particles are released twice, as in the original */
static void cObjElementFXSceneEnterMatrix_dtor(void *self) {
	cObjElementFXSceneEnterMatrix *s = self;

	FXObj_SetVtbl(self, VT_FXSCENEENTERMATRIX);
	CGameParticles_Release(&s->particles);
	CGameLensFlare_dtor(&s->flare);
	FX_TrailsDtor(s->trails, 16);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(self);
}

static void cObjElementFXSceneHumanCD_dtor(void *self) {
	cObjElementFXSceneHumanCD *s = self;

	CGameParticles_Release(&s->sparks);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(self);
}

static void cObjElementFXSceneTheEndB_dtor(void *self) {
	cObjElementFXSceneTheEndB *s = self;

	FXObj_SetVtbl(self, VT_FXSCENETHEENDB);
	CGameLensFlare_dtor(&s->flare);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(self);
}

static void cObjElementFXSceneYakuzzi_dtor(void *self) {
	cObjElementFXSceneYakuzzi *s = self;

	CGameParticles_Release(&s->particles);
	cObjElement_dtor(self);
}

static void cObjElementFXSceneBeatPickus_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXSceneBeatPickus_dtor(self);
	operator_delete(self);
}

static void cObjElementFXSceneBillArrives_A_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXSceneBillArrives_A_dtor(self);
	operator_delete(self);
}

static void cObjElementFXSceneBishops_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXSceneBishops_dtor(self);
	operator_delete(self);
}

static void cObjElementFXSceneDecontamination_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXSceneDecontamination_dtor(self);
	operator_delete(self);
}

static void cObjElementFXSceneEnterMatrix_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXSceneEnterMatrix_dtor(self);
	operator_delete(self);
}

static void cObjElementFXSceneHumanCD_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXSceneHumanCD_dtor(self);
	operator_delete(self);
}

static void cObjElementFXSceneTheEndB_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXSceneTheEndB_dtor(self);
	operator_delete(self);
}

static void cObjElementFXSceneYakuzzi_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXSceneYakuzzi_dtor(self);
	operator_delete(self);
}

/* the elements of this file */

static const cObjElementFXVtbl cObjElementFXFadeOutTrail_vftable;
#define VT_FXFADEOUTTRAIL (&cObjElementFXFadeOutTrail_vftable)
static const cObjElementFXVtbl cObjElementFXItemFade_vftable;
#define VT_FXITEMFADE (&cObjElementFXItemFade_vftable)
static const cObjElementFXVtbl cObjElementFXItemTake_vftable;
#define VT_FXITEMTAKE (&cObjElementFXItemTake_vftable)
static const cObjElementFXVtbl cObjElementFXModelFade_vftable;
#define VT_FXMODELFADE (&cObjElementFXModelFade_vftable)
static const cObjElementFXVtbl cObjElementFXModelNoise_vftable;
#define VT_FXMODELNOISE (&cObjElementFXModelNoise_vftable)
static const cObjElementFXVtbl cObjElementFXParticleSystem_vftable;
#define VT_FXPARTICLESYSTEM (&cObjElementFXParticleSystem_vftable)
static const cObjElementFXVtbl cObjElementFXParticleSystemDuDv_vftable;
#define VT_FXPARTICLESYSTEMDUDV (&cObjElementFXParticleSystemDuDv_vftable)
static const cObjElementFXVtbl cObjElementFXSparks_vftable;
#define VT_FXSPARKS (&cObjElementFXSparks_vftable)
static const cObjElementFXVtbl cObjElementFXTrail_vftable;
#define VT_FXTRAIL (&cObjElementFXTrail_vftable)

void *cObjElementFXFadeOutTrail_ctor(void *self) {
	cObjElementFXFadeOutTrail *s = self;

	FXObj_SetVtbl(self, VT_FXFADEOUTTRAIL);
	CGameTrail_ctor(&s->trail);
	s->running = 1;
	s->on = 0;
	s->t = 0.0f;
	s->duration = 0.0f;
	return self;
}

void *cObjElementFXItemFade_ctor(void *self) {
	cObjElementFXItemFade *s = self;

	FXObj_SetVtbl(self, VT_FXITEMFADE);
	CGameParticles_ctor(&s->particles);
	s->done = 0;
	s->started = 0;
	s->t = 0.0f;
	s->mode = 0;
	s->duration = 0.0f;
	s->scale = 1.0f;
	return self;
}

void *cObjElementFXItemTake_ctor(void *self) {
	cObjElementFXItemTake *s = self;

	FXObj_SetVtbl(self, VT_FXITEMTAKE);
	CGameParticles_ctor(&s->particles);
	CGameTrail_ctor(&s->trail);
	s->done = 0;
	s->started = 0;
	s->t = 0.0f;
	s->emitT = 0.0f;
	s->mode = 0;
	s->unused_634 = 0.0f;
	s->scale = 1.0f;
	return self;
}

void *cObjElementFXModelFade_ctor(void *self) {
	cObjElementFXModelFade *s = self;

	FXObj_SetVtbl(self, VT_FXMODELFADE);
	s->unused_10 = 0;
	s->t = 0.0f;
	s->duration = 0.0f;
	s->fadeOut = 0.0f;
	return self;
}

void *cObjElementFXModelNoise_ctor(void *self) {
	cObjElementFXModelNoise *s = self;

	FXObj_SetVtbl(self, VT_FXMODELNOISE);
	s->unused_10 = 0;
	s->t = 0.0f;
	s->duration = 0.0f;
	return self;
}

void *cObjElementFXParticleSystem_ctor(void *self) {
	cObjElementFXParticleSystem *s = self;

	FXObj_SetVtbl(self, VT_FXPARTICLESYSTEM);
	CGameParticles_ctor(&s->particles);
	s->move = 1;
	s->on = 0;
	return self;
}

cObjElementFXParticleSystemDuDv *cObjElementFXParticleSystemDuDv_ctor(
	cObjElementFXParticleSystemDuDv *self) {
	FXObj_SetVtbl(self, VT_FXPARTICLESYSTEMDUDV);
	CGameParticles_ctor(&self->particles);
	self->move = 1;
	self->on = 0;
	return self;
}

void *cObjElementFXSparks_ctor(void *self) {
	cObjElementFXSparks *s = self;

	FXObj_SetVtbl(self, VT_FXSPARKS);
	FX_TrailsCtor(s->trails, FXSPARKS_MAXSPARKS);
	CGameParticles_ctor(&s->particles);
	s->on = 0;
	s->time = 0.0f;
	s->dt = 0.0f;
	s->trailT = 0.0f;
	return self;
}

void *cObjElementFXTrail_ctor(void *self) {
	cObjElementFXTrail *s = self;

	FXObj_SetVtbl(self, VT_FXTRAIL);
	CGameTrail_ctor(&s->trail);
	s->on = 1;
	s->unused_1a = 0;
	s->minDist = 200.0f;	/* minDist 200 */
	s->stopped = 0;
	return self;
}

static void cObjElementFXFadeOutTrail_dtor(void *self) {
	CGameTrail_dtor(&((cObjElementFXFadeOutTrail *)self)->trail);
	cObjElement_dtor(self);
}

static void cObjElementFXItemFade_dtor(void *self) {
	cObjElementFXItemFade *s = self;

	CGameParticles_Release(&s->particles);
	cObjElement_dtor(self);
}

static void cObjElementFXItemTake_dtor(void *self) {
	cObjElementFXItemTake *s = self;

	CGameTrail_dtor(&s->trail);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(self);
}

static void cObjElementFXParticleSystem_dtor(void *self) {
	cObjElementFXParticleSystem *s = self;

	CGameParticles_Release(&s->particles);
	cObjElement_dtor(self);
}

static void cObjElementFXParticleSystemDuDv_dtor(cObjElementFXParticleSystemDuDv *self) {
	CGameParticles_Release(&self->particles);
	cObjElement_dtor(self);
}

static void cObjElementFXSparks_dtor(void *self) {
	cObjElementFXSparks *s = self;

	CGameParticles_Release(&s->particles);
	FX_TrailsDtor(s->trails, FXSPARKS_MAXSPARKS);
	cObjElement_dtor(self);
}

static void cObjElementFXTrail_dtor(void *self) {
	CGameTrail_dtor(&((cObjElementFXTrail *)self)->trail);
	cObjElement_dtor(self);
}

static void cObjElementFXModelFade_dtor(void *self) {
	cObjElement_dtor(self);
}

static void cObjElementFXModelNoise_dtor(void *self) {
	cObjElement_dtor(self);
}

static void cObjElementFXFadeOutTrail_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXFadeOutTrail_dtor(self);
	operator_delete(self);
}

static void cObjElementFXItemFade_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXItemFade_dtor(self);
	operator_delete(self);
}

static void cObjElementFXItemTake_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXItemTake_dtor(self);
	operator_delete(self);
}

static void cObjElementFXModelFade_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXModelFade_dtor(self);
	operator_delete(self);
}

static void cObjElementFXModelNoise_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXModelNoise_dtor(self);
	operator_delete(self);
}

static void cObjElementFXParticleSystem_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXParticleSystem_dtor(self);
	operator_delete(self);
}

static void cObjElementFXParticleSystemDuDv_Free(cObjElementFX *base) {
	cObjElementFXParticleSystemDuDv *self = (cObjElementFXParticleSystemDuDv *)base;
	cObjElementFXParticleSystemDuDv_dtor(self);
	operator_delete(self);
}

static void cObjElementFXSparks_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXSparks_dtor(self);
	operator_delete(self);
}

static void cObjElementFXTrail_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXTrail_dtor(self);
	operator_delete(self);
}

/* the alpha of the trail goes up with t over the duration while it is not running */
static void cObjElementFXFadeOutTrail_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXFadeOutTrail *self = (cObjElementFXFadeOutTrail *)base;
	(void)d;
	(void)dt;
	float t = self->t + cTimer_GetDelta(&g_pWorld->timer);
	self->t = t;
	if (!self->running && t > self->duration) {
		return;
	}
	int a = 0;
	if (!self->running) {
		a = (int)((t / self->duration) * 255.0f);
		if (a > 0xff) {
			a = 0xff;
		} else if (a < 0) {
			a = 0;
		}
	}
	if (self->on) {
		CGameTrail_SetTransp(&self->trail, (byte)a);
	}
	self->running = 0;
}

/* the vtable of cObjElementFXMoneyTransfer */
static const cObjElementFXVtbl cObjElementFXMoneyTransfer_vftable = {
	.Free = cObjElementFXMoneyTransfer_Free,
	.Update = cObjElementFXMoneyTransfer_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXMoneyTransfer_Attach,
	.Reset = cObjElementFXMoneyTransfer_Reset,
};

/* the vtable of cObjElementFXCharacterTeleport */
static const cObjElementFXVtbl cObjElementFXCharacterTeleport_vftable = {
	.Free = cObjElementFXCharacterTeleport_Free,
	.Update = cObjElementFXCharacterTeleport_Update,
	.IsAlive = cObjElementFXCharacterTeleport_Alive,
	.Start = cObjElementFXCharacterTeleport_Attach,
	.Reset = cObjElementFXCharacterTeleport_Reset,
};

/* the vtable of cObjElementFXCharacterConversor */
static const cObjElementFXVtbl cObjElementFXCharacterConversor_vftable = {
	.Free = cObjElementFXCharacterConversor_Free,
	.Update = cObjElementFXCharacterConversor_Update,
	.IsAlive = cObjElementFXCharacterConversor_Alive,
	.Start = cObjElementFXCharacterConversor_Attach,
	.Reset = cObjElementFXCharacterConversor_Reset,
};

/* the vtable of cObjElementFXCharacterDeath */
static const cObjElementFXVtbl cObjElementFXCharacterDeath_vftable = {
	.Free = cObjElementFXCharacterDeath_Free,
	.Update = cObjElementFXCharacterDeath_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXCharacterDeath_Attach,
	.Reset = cObjElementFXCharacterDeath_Reset,
};

/* the vtable of cObjElementFXSputnik */
static const cObjElementFXVtbl cObjElementFXSputnik_vftable = {
	.Free = cObjElementFXSputnik_Free,
	.Update = cObjElementFXSputnik_Update,
	.IsAlive = cObjElementFXSputnik_Alive,
	.Start = cObjElementFXSputnik_Attach,
	.Reset = cObjElementFXSputnik_Reset,
};

/* the vtable of cObjElementFXCharacterConversion */
static const cObjElementFXVtbl cObjElementFXCharacterConversion_vftable = {
	.Free = cObjElementFXCharacterConversion_Free,
	.Update = cObjElementFXCharacterConversion_Update,
	.IsAlive = cObjElementFXCharacterConversion_Alive,
	.Start = cObjElementFXCharacterConversion_Attach,
	.Reset = cObjElementFXCharacterConversion_Reset,
};

/* the vtable of cObjElementFXMaintenanceWork */
static const cObjElementFXVtbl cObjElementFXMaintenanceWork_vftable = {
	.Free = cObjElementFXMaintenanceWork_Free,
	.Update = cObjElementFXMaintenanceWork_Update,
	.IsAlive = cObjElementFXMaintenanceWork_Alive,
	.Start = cObjElementFXMaintenanceWork_Attach,
	.Reset = cObjElementFXMaintenanceWork_Reset,
};

/* the vtable of cObjElementFXParticleSystem */
static const cObjElementFXVtbl cObjElementFXParticleSystem_vftable = {
	.Free = cObjElementFXParticleSystem_Free,
	.Update = cObjElementFXParticleSystem_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXParticleSystem_Attach,
	.Reset = FXMisc_Nop,
};

/* the vtable of cObjElementFXTrail */
static const cObjElementFXVtbl cObjElementFXTrail_vftable = {
	.Free = cObjElementFXTrail_Free,
	.Update = cObjElementFXTrail_Update,
	.IsAlive = cObjElementFXTrail_Alive,
	.Start = FXMisc_Nop,
	.Reset = cObjElementFXTrail_Reset,
};

/* the vtable of cObjElementFXFadeOutTrail */
static const cObjElementFXVtbl cObjElementFXFadeOutTrail_vftable = {
	.Free = cObjElementFXFadeOutTrail_Free,
	.Update = cObjElementFXFadeOutTrail_Update,
	.IsAlive = cObjElementFXFadeOutTrail_Alive,
	.Start = FXMisc_Nop,
	.Reset = cObjElementFXFadeOutTrail_Reset,
};

/* the vtable of cObjElementFXItemTake */
static const cObjElementFXVtbl cObjElementFXItemTake_vftable = {
	.Free = cObjElementFXItemTake_Free,
	.Update = cObjElementFXItemTake_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXItemTake_Attach,
	.Reset = cObjElementFXItemTake_Reset,
};

/* the vtable of cObjElementFXItemFade */
static const cObjElementFXVtbl cObjElementFXItemFade_vftable = {
	.Free = cObjElementFXItemFade_Free,
	.Update = cObjElementFXItemFade_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXItemFade_Attach,
	.Reset = cObjElementFXItemFade_Reset,
};

/* the vtable of cObjElementFXSparks */
static const cObjElementFXVtbl cObjElementFXSparks_vftable = {
	.Free = cObjElementFXSparks_Free,
	.Update = cObjElementFXSparks_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXSparks_Attach,
	.Reset = cObjElementFXSparks_Reset,
};

/* the vtable of cObjElementFXModelFade */
static const cObjElementFXVtbl cObjElementFXModelFade_vftable = {
	.Free = cObjElementFXModelFade_Free,
	.Update = cObjElementFXModelFade_Update,
	.IsAlive = cObjElementFXModelFade_Alive,
	.Start = FXMisc_Clear4,
	.Reset = FXMisc_Clear4,
};

/* the vtable of cObjElementFXModelNoise */
static const cObjElementFXVtbl cObjElementFXModelNoise_vftable = {
	.Free = cObjElementFXModelNoise_Free,
	.Update = cObjElementFXModelNoise_Update,
	.IsAlive = cObjElementFXModelNoise_Alive,
	.Start = FXMisc_Clear8,
	.Reset = FXMisc_Clear8,
};

/* the vtable of cObjElementFXParticleSystemDuDv */
static const cObjElementFXVtbl cObjElementFXParticleSystemDuDv_vftable = {
	.Free = cObjElementFXParticleSystemDuDv_Free,
	.Update = cObjElementFXParticleSystem_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXParticleSystemDuDv_Attach,
	.Reset = FXMisc_Nop,
};

/* the vtable of cObjElementFXSceneDecontamination */
static const cObjElementFXVtbl cObjElementFXSceneDecontamination_vftable = {
	.Free = cObjElementFXSceneDecontamination_Free,
	.Update = cObjElementFXSceneDecontamination_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXSceneDecontamination_Attach,
	.Reset = cObjElementFXSceneDecontamination_Reset,
};

/* the vtable of cObjElementFXSceneBishops */
static const cObjElementFXVtbl cObjElementFXSceneBishops_vftable = {
	.Free = cObjElementFXSceneBishops_Free,
	.Update = cObjElementFXSceneBishops_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXSceneBishops_Attach,
	.Reset = cObjElementFXSceneBishops_Reset,
};

/* the vtable of cObjElementFXSceneBillArrives_A */
static const cObjElementFXVtbl cObjElementFXSceneBillArrives_A_vftable = {
	.Free = cObjElementFXSceneBillArrives_A_Free,
	.Update = cObjElementFXSceneBillArrives_A_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXSceneBillArrives_A_Attach,
	.Reset = cObjElementFXSceneBillArrives_A_Reset,
};

/* the vtable of cObjElementFXSceneYakuzzi */
static const cObjElementFXVtbl cObjElementFXSceneYakuzzi_vftable = {
	.Free = cObjElementFXSceneYakuzzi_Free,
	.Update = cObjElementFXSceneYakuzzi_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXSceneYakuzzi_Attach,
	.Reset = cObjElementFXSceneYakuzzi_Reset,
};

/* the vtable of cObjElementFXSceneBeatPickus */
static const cObjElementFXVtbl cObjElementFXSceneBeatPickus_vftable = {
	.Free = cObjElementFXSceneBeatPickus_Free,
	.Update = cObjElementFXSceneBeatPickus_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXSceneBeatPickus_Attach,
	.Reset = cObjElementFXSceneBeatPickus_Reset,
};

/* the vtable of cObjElementFXSceneHumanCD */
static const cObjElementFXVtbl cObjElementFXSceneHumanCD_vftable = {
	.Free = cObjElementFXSceneHumanCD_Free,
	.Update = cObjElementFXSceneHumanCD_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXSceneHumanCD_Attach,
	.Reset = cObjElementFXSceneHumanCD_Reset,
};

/* the vtable of cObjElementFXSceneEnterMatrix */
static const cObjElementFXVtbl cObjElementFXSceneEnterMatrix_vftable = {
	.Free = cObjElementFXSceneEnterMatrix_Free,
	.Update = cObjElementFXSceneEnterMatrix_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXSceneEnterMatrix_Attach,
	.Reset = cObjElementFXSceneEnterMatrix_Reset,
};

/* the vtable of cObjElementFXSceneTheEndB */
static const cObjElementFXVtbl cObjElementFXSceneTheEndB_vftable = {
	.Free = cObjElementFXSceneTheEndB_Free,
	.Update = cObjElementFXSceneTheEndB_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXSceneTheEndB_Attach,
	.Reset = cObjElementFXSceneTheEndB_Reset,
};

static cGVar GVar_FXItemTakeFile;
static cGVar GVar_FXItemFadeFile;
/* the engine variables (Gvars): the files of the debris systems of the effects */
static cGVar GVar_FXItemBlinkFile;
static cGVar GVar_FXShipEditBuildFile;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_GVAR_STRING(Fini_GVar_FXShipEditBuildFile, &GVar_FXShipEditBuildFile)
INIT_GVAR_STRING(Init_GVar_FXShipEditBuildFile, Fini_GVar_FXShipEditBuildFile, &GVar_FXShipEditBuildFile, "FXShipEditBuildFile", 0, "Default Model for ShipEdit Build Effects", "Models/GFX/Sputnik/ShipEditBuild.M3D")
FINI_GVAR_STRING(Fini_GVar_FXItemFadeFile, &GVar_FXItemFadeFile)
INIT_GVAR_STRING(Init_GVar_FXItemFadeFile, Fini_GVar_FXItemFadeFile, &GVar_FXItemFadeFile, "FXItemFadeFile", 0, "Default Model for Item Fade In/Out Effect", "Models/GFX/ItemFade.M3D")
FINI_GVAR_STRING(Fini_GVar_FXItemTakeFile, &GVar_FXItemTakeFile)
INIT_GVAR_STRING(Init_GVar_FXItemTakeFile, Fini_GVar_FXItemTakeFile, &GVar_FXItemTakeFile, "FXItemTakeFile", 0, "Default Model for Item Take Effect", "Models/GFX/ItemTake.M3D")
FINI_GVAR_STRING(Fini_GVar_FXItemBlinkFile, &GVar_FXItemBlinkFile)
INIT_GVAR_STRING(Init_GVar_FXItemBlinkFile, Fini_GVar_FXItemBlinkFile, &GVar_FXItemBlinkFile, "FXItemBlinkFile", 0, "Item Blink FX Model File", "Models/GFX/ItemBlink.M3D")
