/*
 * World/FX/FXMisc.cpp of the game: the small effects of the world, see include/scrap/fxmisc.h.
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/crt.h"
#include "scrap/fxmisc.h"
#include "scrap/scorer.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapworld.h"
#include "scrap/world.h"

/* the trail array members: CGameTrail by new[] style construction (rva) */
#define CGAMETRAIL_CTOR 0x21ddb0
#define CGAMETRAIL_DTOR 0x21de40

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
	return Entity_Removed(e) || !e->flag_4c;
}

/* the model effect of the item fades: 1 off and disabled, 2 off and enabled */
static void FX_EndFade(CGameModel *m, int mode) {
	if (mode == 1) {
		CGameModel_SetEffect(m, 0);
		CGameModel_Disable(m);
	} else if (mode == 2) {
		CGameModel_SetEffect(m, 0);
		CGameModel_Enable(m);
	}
}

/* the particles to "Dummy01" of the model, to the node index of the pointer itself without a model (as the
 * original does) */
static void FX_AttachDummy(CGameParticles *sp, CGameModel *m) {
	uint32_t i = CGameModel_IsLoaded(m) ? (uint32_t)CGameModel_FindNodoR(m, "Dummy01") : (uint32_t)(uintptr_t)m;

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

/* a new debris of the system of file, life set, dropped; its element */
static void *FX_DropNew(const char *file, float life) {
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, file);
	cDebris *d = cDebrisSys_NewElement(sys);

	Debris_Life(d) = life;
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
		cam = Camera_GetActual();
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

void FX_ItemBlink(cEntity *e) {
	CGameModel *m = FX_Vtbl(e)->GetModel(e);

	CHECK(m && CGameModel_IsLoaded(m));
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXItemBlinkFile);
	cDebris *d = cDebrisSys_GetIdle(sys);
	CGameModel_SetSortZ(Debris_Model(d), -0.05f, 0.0f);
	Debris_Life(d) = 1.0f;
	if (!cDebrisSys_IsIdle(sys, d)) {
		cDebrisSys_Drop(sys, d);
	}
	cObjElementFXSistPart *el = Debris_Element(d);
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
	CGameParticles_SetColorRamp(sp, Game_Ptr(FX_RAMP_BLINK));
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
		const vec3_t *c = FX_Vtbl(e)->GetCenter(e, &tmp);
		sp->initPosMin.x = c->x + ox;
		sp->initPosMin.y = c->y + oy;
		sp->initPosMin.z = c->z + oz;
		sp->initPosMax = sp->initPosMin;
		CGameParticles_Apply(sp);
		CGameParticles_EmitN(sp, 1, 1);
	}
}

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
	CHECK(target->vftable->IsType(target, "Obj"));
	if (!g_IsMapOutDoor) {
		cObjElementFXItemTake *take = FX_DropNew(g_FXItemTakeFile, 1.25f);
		if (take) {
			if (!take->started) {
				take->started = 1;
			}
			take->mode = mode;
			take->f634 = 1.0f;
			take->scale = scale;
			cObjElementFXItemTake_Start(take, e);
			take->target = target;
		}
	}
	/* in split screen the scorer of the view whose camera is the owner of the target (at 0xf0) */
	int player = 0;
	if (g_SplitScreenGame && *(cEntity **)((byte *)target + 0xf0)) {
		for (player = 0;; player++) {
			if (player >= 4) {
				return;
			}
			const char *a = (*(cEntity **)((byte *)target + 0xf0))->name;
			if (FX_NameEq(a, cWorld_GetCamName(g_pWorld, player))) {
				break;
			}
		}
	}
	cScorer *sc = Scorer_Get(player);
	if (sc) {
		ScorerFade_Start((byte *)sc + 0xb0, 0x40c0c0ff, 0x2020ff, 0.22f);
	}
}

void FX_Sputnik(cEntity *e, const char *name) {
	if (!crt_strncmp(name, "CharacterSputnik", 16)) {
		if (!CGameModel_IsLoaded(FX_Vtbl(e)->GetModel(e)) || strcmp(name, "CharacterSputnikSceneMecanicFinal")) {
			return;
		}
		byte *el = FX_DropNew(g_FXCharacterSputnikFile, 4.0f);
		if (!el) {
			return;
		}
		if (!el[0xdc]) {
			el[0xdc] = 1;
		}
		FXSputnik_Start(el, e);
		FXSputnik_SetA(el, 3.0f);
		FXSputnik_SetB(el, 5.0f);
		return;
	}
	if (!crt_strncmp(name, "ShipEditBuild", 13)) {
		if (!CGameModel_IsLoaded(FX_Vtbl(e)->GetModel(e))) {
			return;
		}
		byte *el = FX_DropNew(g_FXShipEditBuildFile, 0.45f);
		if (el) {
			if (!el[0x10]) {
				el[0x10] = 1;
			}
			FXMisc_SetEntity(el, e);
			*(float *)(el + 0xc) = 0.45f;
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

/* the elements: Update (vf01), Alive (vf02), Attach (vf03), Reset (vf04) and the starts */

/* the Alive of the elements that only step the debris */
byte __thiscall FXMisc_Alive(void *self, cDebris *d, cDebrisSys *sys) {
	(void)self;
	return cDebris_Step(d, sys);
}

/* the methods without code (ret 4) */
void __thiscall FXMisc_Nop(void *self, int arg) {
	(void)self;
	(void)arg;
}

void __thiscall cObjElementFXItemFade_Update(cObjElementFXItemFade *self, cDebris *d, float dt) {
	CGameModel *m;
	cEntity *e = self->ent;

	(void)dt;
	if (e && FX_Gone(e)) {
		m = FX_Vtbl(e)->GetModel(e);
		CHECK(CGameModel_IsLoaded(m));
		FX_EndFade(m, self->mode);
		self->ent = NULL;
	}
	if (!self->ent || !self->ent->flag_4c) {
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
	m = FX_Vtbl(self->ent)->GetModel(self->ent);
	CHECK(CGameModel_IsLoaded(m));
	if (!self->done) {
		if (0.1f > self->t) {
			if (!CGameModel_IsEnabled(m)) {
				CGameModel_Enable(m);
			}
			CGameModel_SetEffect(m, 2);
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

void __thiscall cObjElementFXModelFade_Update(cObjElementFXModelFade *self, cDebris *d, float dt) {
	float f;

	if (!d || !CGameModel_IsLoaded(Debris_Model(d))) {
		return;
	}
	float t = cTimer_GetDelta(&g_pWorld->timer) + self->t;
	self->t = t;
	float rem = Debris_Life(d) - dt;
	if (self->duration > t) {
		f = t / self->duration;
	} else if (0.0f >= rem) {
		f = 0.0f;
	} else if (self->fadeOut > rem) {
		f = rem / self->fadeOut;
	} else {
		f = 1.0f;
	}
	CGameModel_SetEffect(Debris_Model(d), 2);
	CGameModel_SetEffectParam(Debris_Model(d), 0, FX_Bits(1.0f - f));
	cDebris_Move(d, dt);
}

void __thiscall cObjElementFXModelNoise_Update(cObjElementFXModelNoise *self, cDebris *d, float dt) {
	cEntity *e = self->ent;

	(void)d;
	(void)dt;
	if (e && FX_Gone(e)) {
		FX_Vtbl(e)->SetFX(e, 0);
		self->ent = NULL;
	}
	if (!self->ent || !self->ent->flag_4c) {
		return;
	}
	float t = self->t + cTimer_GetDelta(&g_pWorld->timer);
	self->t = t;
	float f = self->duration > t ? t / self->duration : 1.0f;
	FX_Vtbl(self->ent)->SetFX(self->ent, 8);
	FX_Vtbl(self->ent)->SetFXParam(self->ent, 0, 0.45f - f * 0.45f);
}

/* SistPart and SistPartDuDv */
void __thiscall cObjElementFXSistPart_Update(cObjElementFXSistPart *self, cDebris *d, float dt) {
	if (self->move) {
		cDebris_Move(d, dt);
	}
}

void __thiscall cObjElementFXTrail_Update(cObjElementFXTrail *self, cDebris *d, float dt) {
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

byte __thiscall cObjElementFXFadeOutTrail_Alive(cObjElementFXFadeOutTrail *self, cDebris *d, cDebrisSys *sys) {
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

byte __thiscall cObjElementFXModelFade_Alive(cObjElementFXModelFade *self, cDebris *d, cDebrisSys *sys) {
	(void)self;
	if (!cDebris_Step(d, sys)) {
		CGameModel_SetEffect(Debris_Model(d), 0);
		return 0;
	}
	return 1;
}

byte __thiscall cObjElementFXModelNoise_Alive(cObjElementFXModelNoise *self, cDebris *d, cDebrisSys *sys) {
	if (!cDebris_Step(d, sys)) {
		if (self->ent) {
			FX_Vtbl(self->ent)->SetFX(self->ent, 0);
		}
		return 0;
	}
	return 1;
}

byte __thiscall cObjElementFXTrail_Alive(cObjElementFXTrail *self, cDebris *d, cDebrisSys *sys) {
	if (!self->on) {
		return cDebris_Step(d, sys);
	}
	if (cDebris_Step(d, sys)) {
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

void __thiscall cObjElementFXItemFade_Attach(cObjElementFXItemFade *self, cDebris *d) {
	FX_AttachDummy(&self->particles, Debris_Model(d));
}

void __thiscall cObjElementFXItemTake_Attach(cObjElementFXItemTake *self, cDebris *d) {
	FX_AttachDummy(&self->particles, Debris_Model(d));
}

/* ModelFade (t at 0x4) and ModelNoise (t at 0x8): Attach and Reset */
void __thiscall FXMisc_Clear4(void *self, int arg) {
	(void)arg;
	*(float *)((byte *)self + 4) = 0.0f;
}

void __thiscall FXMisc_Clear8(void *self, int arg) {
	(void)arg;
	*(float *)((byte *)self + 8) = 0.0f;
}

static void FX_SistPartAttach(cObjElementFXSistPart *self, cDebris *d, const char *node) {
	CGameModel *m = Debris_Model(d);

	CHECK(CGameModel_IsLoaded(m));
	int i = CGameModel_FindNodoR(m, node);
	CHECK(i > -1);
	CGameParticles_Attach(&self->particles, m, i);
	CHECK(self->particles.SistPart);
}

void __thiscall cObjElementFXSistPart_Attach(cObjElementFXSistPart *self, cDebris *d) {
	FX_SistPartAttach(self, d, "Dummy01");
}

void __thiscall cObjElementFXSistPartDuDv_Attach(cObjElementFXSistPart *self, cDebris *d) {
	FX_SistPartAttach(self, d, "Dummy01 (+dudv_pass)");
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

void __thiscall cObjElementFXSparks_Attach(cObjElementFXSparks *self, cDebris *d) {
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

void __thiscall cObjElementFXFadeOutTrail_Reset(cObjElementFXFadeOutTrail *self, int arg) {
	(void)arg;
	self->t = 0.0f;
	if (self->on) {
		CGameTrail_Clear(&self->trail);
		CGameTrail_SetTransp(&self->trail, 0xff);
	}
	self->running = 1;
}

/* the original also checks the address of the particles member, which is never NULL */
void __thiscall cObjElementFXItemFade_Reset(cObjElementFXItemFade *self, int arg) {
	(void)arg;
	self->t = 0.0f;
	self->mode = 0;
	self->ent = NULL;
	CHECK(self->particles.SistPart);
	self->done = 0;
}

void __thiscall cObjElementFXItemTake_Reset(cObjElementFXItemTake *self, int arg) {
	(void)arg;
	self->t = 0.0f;
	self->emitT = 0.0f;
	self->mode = 0;
	self->ent = NULL;
	CHECK(self->particles.SistPart);
	self->done = 0;
}

void __thiscall cObjElementFXSparks_Reset(cObjElementFXSparks *self, int arg) {
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

void __thiscall cObjElementFXTrail_Reset(cObjElementFXTrail *self, int arg) {
	(void)arg;
	CGameTrail_Clear(&self->trail);
}

/* the setters of the effects started by name (rva 0x1ddc40..0x1ddce0) */
void __thiscall FXMisc_SetEntity(void *self, cEntity *e) {
	*(cEntity **)((byte *)self + 4) = e;
	CHECK(CGameModel_IsLoaded(FX_Vtbl(e)->GetModel(e)));
}

void __thiscall FXMisc_SetF8(void *self, float v) {
	*(float *)((byte *)self + 8) = v;
}

void __thiscall FXMisc_SetFC(void *self, float v) {
	*(float *)((byte *)self + 0xc) = v;
}

void __thiscall FXMisc_SetI8(void *self, int v) {
	*(int *)((byte *)self + 8) = v;
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

/* the particles applied and one emitted with one of the three fade ramps */
static void FX_EmitFade(CGameParticles *sp) {
	CGameParticles_Apply(sp);
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, (uint32_t *)Game_Ptr(FX_RAMP_FADE) + crt_rand() % 3 * 9);
	CGameParticles_EmitN(sp, 1, 1);
}

void __thiscall cObjElementFXItemFade_Start(cObjElementFXItemFade *self, cEntity *e) {
	CGameParticles *sp = &self->particles;
	vec3_t c;

	self->ent = e;
	if (!e) {
		return;
	}
	CGameModel *m = FX_Vtbl(e)->GetModel(e);
	CHECK(CGameModel_IsLoaded(m));
	if (self->mode == 1) {
		CGameModel_SetEffect(m, 0);
		CGameModel_Enable(m);
	} else if (self->mode == 2) {
		CGameModel_SetEffect(m, 2);
		CGameModel_SetEffectParam(m, 0, 0);
		CGameModel_Enable(m);
	}
	FX_Vtbl(self->ent)->GetCenter(self->ent, &c);
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

void __thiscall cObjElementFXItemTake_Start(cObjElementFXItemTake *self, cEntity *e) {
	vec3_t tmp;

	self->ent = e;
	if (!e) {
		return;
	}
	CGameModel *m = FX_Vtbl(e)->GetModel(e);
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

void __thiscall cObjElementFXItemTake_Update(cObjElementFXItemTake *self, cDebris *d, float dt) {
	CGameParticles *sp = &self->particles;
	vec3_t c1, c2, tmp, coefs[7], pts[7], first;
	byte found = 0;

	(void)dt;
	if (self->ent && FX_Gone(self->ent)) {
		self->ent = NULL;
	}
	if (!self->ent || !self->ent->flag_4c) {
		return;
	}
	if (self->target && FX_Gone(self->target)) {
		self->target = NULL;
	}
	if (!self->target || !self->target->flag_4c) {
		return;
	}
	float delta = cTimer_GetDelta(&g_pWorld->timer);
	self->t = self->t + delta;
	self->emitT = self->emitT + delta;
	CGameModel *m = FX_Vtbl(self->ent)->GetModel(self->ent);
	CHECK(m && CGameModel_IsLoaded(m));
	CGameModel_SetSortZ(Debris_Model(d), -7.0f, 0.0f);
	CGameModel_SetPos(Debris_Model(d), CGameModel_GetPos(m, &tmp));
	sp->birthRate = 0;
	int np = CGameParticles_NumParticles(sp);
	CGameModel_Radius(m);
	FX_Vtbl(self->ent)->GetRadius(self->ent);
	FX_Vtbl(self->target)->GetRadius(self->target);
	FX_Vtbl(self->ent)->GetCenter(self->ent, &c1);
	FX_Vtbl(self->target)->GetCenter(self->target, &c2);
	for (int k = 0; k < FXITEMTAKE_LINES; k++) {
		static const vec3_t arms[4] = {
			{ -225.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 225.0f }, { 225.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -225.0f }
		};
		vec3_t ang;
		float a = (float)(k - 2) * 15.0f;
		FX_Vtbl(self->target)->GetAngles(self->target, &ang);
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
		CGameParticles_SetColorRamp(sp, (uint32_t *)Game_Ptr(FX_RAMP_TAKE) + crt_rand() % 3 * 9);
		CGameParticles_EmitDt(sp, 1, 1, FX_RandF((double)0.100000001f, 0.0));
	}
}

void __thiscall cObjElementFXSparks_Update(cObjElementFXSparks *self, cDebris *d, float dt) {
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

void __thiscall cObjElementFXSparks_Emit(cObjElementFXSparks *self, int n, float px, float py, float pz, float dx,
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
 * The constructors and destructors of the FX elements of the character effects (rva 0x1a7f20..0x1a88c0, after
 * World/CollisionGrid.cpp) and of the FX scenes (rva 0x1de230..). Only their members are known: particle systems,
 * arrays of trails and lens flares, the fields they clear.
 */

#define FX_U32(self, off) (*(uint32_t *)((byte *)(self) + (off)))
#define FX_U8(self, off) (*(byte *)((byte *)(self) + (off)))

static inline void FX_PartsCtor(void *self, uint32_t off) {
	CGameParticles_ctor((CGameParticles *)((byte *)self + off));
}

static inline void FX_PartsRelease(void *self, uint32_t off) {
	CGameParticles_Release((CGameParticles *)((byte *)self + off));
}

static inline void FX_TrailsCtor(void *self, uint32_t off, uint32_t n) {
	crt_eh_vector_ctor((byte *)self + off, sizeof(CGameTrail), n, Game_Ptr(CGAMETRAIL_CTOR), Game_Ptr(CGAMETRAIL_DTOR));
}

static inline void FX_TrailsDtor(void *self, uint32_t off, uint32_t n) {
	crt_eh_vector_dtor((byte *)self + off, sizeof(CGameTrail), n, Game_Ptr(CGAMETRAIL_DTOR));
}

/* the start of every constructor: the base class leaves 0, then the vtable of the class */
static inline void FX_SetVtbl(void *self, uint32_t vt) {
	*(void **)self = NULL;
	*(void **)self = Game_Ptr(vt);
}

static inline void *FX_Delete(void *self, uint32_t flags, uint32_t size) {
	if (flags & 1) {
		operator_delete_sized(self, size);
	}
	return self;
}

void *__thiscall cObjElementAbstract_Delete(void *self, uint32_t flags) {
	cObjElement_dtor(self);
	return FX_Delete(self, flags, 4);
}

/* World/FX character effects */

#define VT_FXCHARACTERCONVERSION 0x3cb174
#define VT_FXCHARACTERCONVERSOR 0x3cb12c
#define VT_FXCHARACTERDEATH 0x3cb144
#define VT_FXCHARACTERTELEPORT 0x3cb114
#define VT_FXMAINTENANCEWORK 0x3cb18c
#define VT_FXMONEYTRANSFER 0x3cb0fc
#define VT_FXSPUTNIK 0x3cb15c

void *__thiscall cObjElementFXCharacterConversion_ctor(void *self) {
	FX_SetVtbl(self, VT_FXCHARACTERCONVERSION);
	FX_PartsCtor(self, 0xc);
	FX_PartsCtor(self, 0xbc);
	FX_TrailsCtor(self, 0x17c, 5);
	FX_U8(self, 0x898) = 0;
	FX_U32(self, 0x16c) = 0;
	FX_U32(self, 0x170) = 0;
	FX_U32(self, 0x4) = 0;
	FX_U32(self, 0x8) = 0;
	return self;
}

void *__thiscall cObjElementFXCharacterConversor_ctor(void *self) {
	FX_SetVtbl(self, VT_FXCHARACTERCONVERSOR);
	FX_PartsCtor(self, 0xc);
	FX_TrailsCtor(self, 0x110, 3);
	FX_U8(self, 0x294) = 0;
	FX_U32(self, 0xcc) = 0;
	FX_U32(self, 0xd0) = 0;
	FX_U32(self, 0xbc) = 0;
	FX_U32(self, 0xc0) = 0;
	FX_U32(self, 0x100) = 3;
	return self;
}

void *__thiscall cObjElementFXCharacterDeath_ctor(void *self) {
	FX_SetVtbl(self, VT_FXCHARACTERDEATH);
	FX_PartsCtor(self, 0x8);
	FX_PartsCtor(self, 0xb8);
	FX_PartsCtor(self, 0x168);
	FX_TrailsCtor(self, 0x224, 8);
	FX_U8(self, 0xf64) = 0;
	FX_U32(self, 0x4) = 0;
	return self;
}

void *__thiscall cObjElementFXCharacterTeleport_ctor(void *self) {
	FX_SetVtbl(self, VT_FXCHARACTERTELEPORT);
	FX_PartsCtor(self, 0xc);
	FX_U8(self, 0xd4) = 0;
	FX_U32(self, 0xcc) = 0;
	FX_U32(self, 0xd0) = 0;
	FX_U32(self, 0xbc) = 0;
	FX_U32(self, 0xc0) = 0;
	return self;
}

void *__thiscall cObjElementFXMaintenanceWork_ctor(void *self) {
	FX_SetVtbl(self, VT_FXMAINTENANCEWORK);
	FX_PartsCtor(self, 0xc);
	FX_TrailsCtor(self, 0xc8, 2);
	FX_U8(self, 0xdc) = 0;
	FX_U32(self, 0x4) = 0;
	FX_U32(self, 0x8) = 0;
	FX_U32(self, 0xc0) = 0;
	FX_U32(self, 0xbc) = 0;
	FX_U32(self, 0xd4) = 0xffffffffu;
	FX_U32(self, 0xd8) = 0;
	return self;
}

void *__thiscall cObjElementFXMoneyTransfer_ctor(void *self) {
	FX_SetVtbl(self, VT_FXMONEYTRANSFER);
	FX_PartsCtor(self, 0xc);
	FX_U8(self, 0xc8) = 0;
	FX_U32(self, 0x4) = 0;
	FX_U32(self, 0x8) = 0;
	FX_U32(self, 0xbc) = 0;
	FX_U32(self, 0xc0) = 0;
	FX_U32(self, 0xc4) = 0xffffffffu;
	return self;
}

void *__thiscall cObjElementFXSputnik_ctor(void *self) {
	FX_SetVtbl(self, VT_FXSPUTNIK);
	FX_PartsCtor(self, 0x8);
	FX_U8(self, 0xdc) = 0;
	FX_U32(self, 0x4) = 0;
	FX_U32(self, 0xbc) = 0;
	FX_U32(self, 0xb8) = 0;
	FX_U32(self, 0xc0) = 0;
	FX_U32(self, 0xc8) = 0;
	return self;
}

void __thiscall cObjElementFXCharacterConversion_dtor(void *self) {
	*(void **)self = Game_Ptr(VT_FXCHARACTERCONVERSION);
	FX_TrailsDtor(self, 0x17c, 5);
	FX_PartsRelease(self, 0xbc);
	FX_PartsRelease(self, 0xc);
	cObjElement_dtor(self);
}

void __thiscall cObjElementFXCharacterConversor_dtor(void *self) {
	*(void **)self = Game_Ptr(VT_FXCHARACTERCONVERSOR);
	FX_TrailsDtor(self, 0x110, 3);
	FX_PartsRelease(self, 0xc);
	cObjElement_dtor(self);
}

void __thiscall cObjElementFXCharacterDeath_dtor(void *self) {
	*(void **)self = Game_Ptr(VT_FXCHARACTERDEATH);
	FX_TrailsDtor(self, 0x224, 8);
	FX_PartsRelease(self, 0x168);
	FX_PartsRelease(self, 0xb8);
	FX_PartsRelease(self, 0x8);
	cObjElement_dtor(self);
}

void __thiscall cObjElementFXCharacterTeleport_dtor(void *self) {
	*(void **)self = Game_Ptr(VT_FXCHARACTERTELEPORT);
	FX_PartsRelease(self, 0xc);
	cObjElement_dtor(self);
}

void __thiscall cObjElementFXMaintenanceWork_dtor(void *self) {
	*(void **)self = Game_Ptr(VT_FXMAINTENANCEWORK);
	FX_TrailsDtor(self, 0xc8, 2);
	FX_PartsRelease(self, 0xc);
	cObjElement_dtor(self);
}

void __thiscall cObjElementFXMoneyTransfer_dtor(void *self) {
	*(void **)self = Game_Ptr(VT_FXMONEYTRANSFER);
	FX_PartsRelease(self, 0xc);
	cObjElement_dtor(self);
}

void __thiscall cObjElementFXSputnik_dtor(void *self) {
	*(void **)self = Game_Ptr(VT_FXSPUTNIK);
	FX_PartsRelease(self, 0x8);
	cObjElement_dtor(self);
}

void *__thiscall cObjElementFXCharacterConversion_Delete(void *self, uint32_t flags) {
	cObjElementFXCharacterConversion_dtor(self);
	return FX_Delete(self, flags, 0x89c);
}

void *__thiscall cObjElementFXCharacterConversor_Delete(void *self, uint32_t flags) {
	cObjElementFXCharacterConversor_dtor(self);
	return FX_Delete(self, flags, 0x298);
}

void *__thiscall cObjElementFXCharacterDeath_Delete(void *self, uint32_t flags) {
	cObjElementFXCharacterDeath_dtor(self);
	return FX_Delete(self, flags, 0xf68);
}

void *__thiscall cObjElementFXCharacterTeleport_Delete(void *self, uint32_t flags) {
	cObjElementFXCharacterTeleport_dtor(self);
	return FX_Delete(self, flags, 0xd8);
}

void *__thiscall cObjElementFXMaintenanceWork_Delete(void *self, uint32_t flags) {
	cObjElementFXMaintenanceWork_dtor(self);
	return FX_Delete(self, flags, 0xe0);
}

void *__thiscall cObjElementFXMoneyTransfer_Delete(void *self, uint32_t flags) {
	cObjElementFXMoneyTransfer_dtor(self);
	return FX_Delete(self, flags, 0xcc);
}

void *__thiscall cObjElementFXSputnik_Delete(void *self, uint32_t flags) {
	cObjElementFXSputnik_dtor(self);
	return FX_Delete(self, flags, 0xe0);
}

/* the FX scenes */

#define VT_FXSCENEBEATPICKUS 0x3cd278
#define VT_FXSCENEBILLARRIVES_A 0x3cd248
#define VT_FXSCENEBISHOPS 0x3cd230
#define VT_FXSCENEDECONTAMINATION 0x3cd218
#define VT_FXSCENEENTERMATRIX 0x3cd2a8
#define VT_FXSCENEHUMANCD 0x3cd290
#define VT_FXSCENETHEENDB 0x3cd2c0
#define VT_FXSCENEYAKUZZI 0x3cd260

void *__thiscall cObjElementFXSceneBeatPickus_ctor(void *self) {
	FX_SetVtbl(self, VT_FXSCENEBEATPICKUS);
	FX_PartsCtor(self, 0x1c);
	FX_PartsCtor(self, 0xd0);
	FX_PartsCtor(self, 0x184);
	FX_U8(self, 0x23c) = 0;
	FX_U32(self, 0xc) = 0;
	FX_U32(self, 0x4) = 0;
	FX_U32(self, 0x8) = 0;
	FX_U32(self, 0xcc) = 0;
	FX_U32(self, 0x180) = 0;
	FX_U32(self, 0x234) = 0;
	FX_U32(self, 0x238) = 0;
	return self;
}

void *__thiscall cObjElementFXSceneBillArrives_A_ctor(void *self) {
	FX_SetVtbl(self, VT_FXSCENEBILLARRIVES_A);
	CGameLensFlare_ctor((byte *)self + 0x14);
	FX_PartsCtor(self, 0x1c);
	FX_U8(self, 0xe8) = 0;
	FX_U32(self, 0xc) = 0;
	FX_U32(self, 0x10) = 0;
	FX_U32(self, 0x4) = 0;
	FX_U32(self, 0x8) = 0;
	FX_U32(self, 0x18) = 0;
	FX_U32(self, 0xcc) = 0;
	return self;
}

void *__thiscall cObjElementFXSceneBishops_ctor(void *self) {
	FX_SetVtbl(self, VT_FXSCENEBISHOPS);
	FX_PartsCtor(self, 0xc);
	FX_PartsCtor(self, 0xbc);
	FX_TrailsCtor(self, 0x184, 12);
	FX_U8(self, 0x1b4) = 0;
	FX_U32(self, 0x4) = 0;
	FX_U32(self, 0x8) = 0;
	FX_U32(self, 0x16c) = 0;
	FX_U32(self, 0x174) = 0;
	FX_U32(self, 0x178) = 0;
	FX_U32(self, 0x17c) = 0;
	return self;
}

void *__thiscall cObjElementFXSceneDecontamination_ctor(void *self) {
	FX_SetVtbl(self, VT_FXSCENEDECONTAMINATION);
	FX_PartsCtor(self, 0x4);
	FX_TrailsCtor(self, 0x248, 40);
	FX_U8(self, 0x11e8) = 0;
	FX_U32(self, 0xb4) = 0;
	FX_U32(self, 0xb8) = 0;
	FX_U32(self, 0x104) = 0;
	FX_U32(self, 0x100) = 0;
	return self;
}

void *__thiscall cObjElementFXSceneEnterMatrix_ctor(void *self) {
	FX_SetVtbl(self, VT_FXSCENEENTERMATRIX);
	FX_PartsCtor(self, 0x10);
	FX_TrailsCtor(self, 0x28c, 16);
	CGameLensFlare_ctor((byte *)self + 0x790);
	FX_U8(self, 0x794) = 0;
	FX_U32(self, 0xc) = 0;
	FX_U32(self, 0x4) = 0;
	FX_U32(self, 0x8) = 0;
	return self;
}

void *__thiscall cObjElementFXSceneHumanCD_ctor(void *self) {
	FX_SetVtbl(self, VT_FXSCENEHUMANCD);
	FX_PartsCtor(self, 0x10);
	FX_PartsCtor(self, 0xc0);
	FX_U8(self, 0x180) = 0;
	FX_U32(self, 0xc) = 0;
	FX_U32(self, 0x4) = 0;
	FX_U32(self, 0x8) = 0;
	return self;
}

void *__thiscall cObjElementFXSceneTheEndB_ctor(void *self) {
	FX_SetVtbl(self, VT_FXSCENETHEENDB);
	FX_PartsCtor(self, 0x14);
	CGameLensFlare_ctor((byte *)self + 0xc4);
	FX_U8(self, 0xc8) = 0;
	FX_U32(self, 0xc) = 0;
	FX_U32(self, 0x10) = 0;
	FX_U32(self, 0x4) = 0;
	FX_U32(self, 0x8) = 0;
	return self;
}

void *__thiscall cObjElementFXSceneYakuzzi_ctor(void *self) {
	FX_SetVtbl(self, VT_FXSCENEYAKUZZI);
	FX_PartsCtor(self, 0x10);
	FX_U8(self, 0xd0) = 0;
	FX_U32(self, 0xc) = 0;
	FX_U32(self, 0x4) = 0;
	FX_U32(self, 0x8) = 0;
	FX_U32(self, 0xc0) = 0;
	return self;
}

void __thiscall cObjElementFXSceneBeatPickus_dtor(void *self) {
	*(void **)self = Game_Ptr(VT_FXSCENEBEATPICKUS);
	FX_PartsRelease(self, 0x184);
	FX_PartsRelease(self, 0xd0);
	FX_PartsRelease(self, 0x1c);
	cObjElement_dtor(self);
}

void __thiscall cObjElementFXSceneBillArrives_A_dtor(void *self) {
	*(void **)self = Game_Ptr(VT_FXSCENEBILLARRIVES_A);
	FX_PartsRelease(self, 0x1c);
	CGameLensFlare_dtor((byte *)self + 0x14);
	cObjElement_dtor(self);
}

void __thiscall cObjElementFXSceneBishops_dtor(void *self) {
	*(void **)self = Game_Ptr(VT_FXSCENEBISHOPS);
	FX_TrailsDtor(self, 0x184, 12);
	FX_PartsRelease(self, 0xbc);
	FX_PartsRelease(self, 0xc);
	cObjElement_dtor(self);
}

void __thiscall cObjElementFXSceneDecontamination_dtor(void *self) {
	*(void **)self = Game_Ptr(VT_FXSCENEDECONTAMINATION);
	FX_TrailsDtor(self, 0x248, 40);
	FX_PartsRelease(self, 0x4);
	cObjElement_dtor(self);
}

/* the particles are released twice, as in the original */
void __thiscall cObjElementFXSceneEnterMatrix_dtor(void *self) {
	*(void **)self = Game_Ptr(VT_FXSCENEENTERMATRIX);
	FX_PartsRelease(self, 0x10);
	CGameLensFlare_dtor((byte *)self + 0x790);
	FX_TrailsDtor(self, 0x28c, 16);
	FX_PartsRelease(self, 0x10);
	cObjElement_dtor(self);
}

void __thiscall cObjElementFXSceneHumanCD_dtor(void *self) {
	*(void **)self = Game_Ptr(VT_FXSCENEHUMANCD);
	FX_PartsRelease(self, 0xc0);
	FX_PartsRelease(self, 0x10);
	cObjElement_dtor(self);
}

void __thiscall cObjElementFXSceneTheEndB_dtor(void *self) {
	*(void **)self = Game_Ptr(VT_FXSCENETHEENDB);
	CGameLensFlare_dtor((byte *)self + 0xc4);
	FX_PartsRelease(self, 0x14);
	cObjElement_dtor(self);
}

void __thiscall cObjElementFXSceneYakuzzi_dtor(void *self) {
	*(void **)self = Game_Ptr(VT_FXSCENEYAKUZZI);
	FX_PartsRelease(self, 0x10);
	cObjElement_dtor(self);
}

void *__thiscall cObjElementFXSceneBeatPickus_Delete(void *self, uint32_t flags) {
	cObjElementFXSceneBeatPickus_dtor(self);
	return FX_Delete(self, flags, 0x240);
}

void *__thiscall cObjElementFXSceneBillArrives_A_Delete(void *self, uint32_t flags) {
	cObjElementFXSceneBillArrives_A_dtor(self);
	return FX_Delete(self, flags, 0xec);
}

void *__thiscall cObjElementFXSceneBishops_Delete(void *self, uint32_t flags) {
	cObjElementFXSceneBishops_dtor(self);
	return FX_Delete(self, flags, 0x1b8);
}

void *__thiscall cObjElementFXSceneDecontamination_Delete(void *self, uint32_t flags) {
	cObjElementFXSceneDecontamination_dtor(self);
	return FX_Delete(self, flags, 0x11ec);
}

void *__thiscall cObjElementFXSceneEnterMatrix_Delete(void *self, uint32_t flags) {
	cObjElementFXSceneEnterMatrix_dtor(self);
	return FX_Delete(self, flags, 0x798);
}

void *__thiscall cObjElementFXSceneHumanCD_Delete(void *self, uint32_t flags) {
	cObjElementFXSceneHumanCD_dtor(self);
	return FX_Delete(self, flags, 0x184);
}

void *__thiscall cObjElementFXSceneTheEndB_Delete(void *self, uint32_t flags) {
	cObjElementFXSceneTheEndB_dtor(self);
	return FX_Delete(self, flags, 0xcc);
}

void *__thiscall cObjElementFXSceneYakuzzi_Delete(void *self, uint32_t flags) {
	cObjElementFXSceneYakuzzi_dtor(self);
	return FX_Delete(self, flags, 0xd4);
}
