/*
 * Entities/EmbeddedFX/EmbeddedFXVehicle.cpp of the game: the effects embedded in the vehicles, see
 * include/scrap/embeddedfxvehicle.h.
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/crt.h"
#include "scrap/embeddedfxvehicle.h"
#include "scrap/fxmisc.h"
#include "scrap/mathlib.h"
#include "scrap/vehicleentity.h"
#include "scrap/world.h"

#define g_RandScale GAME_GLOBAL(double, g_FxRandScale, 0x3a3d60)	/* 1/32767 */
#define CGAMEPARTICLES_CTOR 0x21d1b0
#define CGAMEPARTICLES_DTOR 0x21d1c0
#define CGAMETRAIL_CTOR 0x21ddb0
#define CGAMETRAIL_DTOR 0x21de40
#define VEHICLE_FX_FADE 3		/* the model effect of the faded vehicle */

/* the color ramps of the particles (9 colors) */
#define RAMP_SIREN_A1 0x428574
#define RAMP_SIREN_A2 0x428550
#define RAMP_SIREN_B1 0x4285bc
#define RAMP_SIREN_B2 0x428598
#define RAMP_PROBE_TRANS 0x4285e0

/* vtable slot 0xf4 of the vehicle: the name of the pilot slot i ("" without one) */
typedef const char *(__thiscall *vehicleName_t)(cEntity *self, int i);

static float EFX_Rand(double scale, double add) {
	return (float)((double)(crt_rand() & 0x7fff) * g_RandScale * scale + add);
}

/* the inlined strcmp of the original: the first two characters as a word, then the rest */
static int Str_EqualW(const char *a, const char *b) {
	if (*(const uint16_t *)a != *(const uint16_t *)b) {
		return 0;
	}
	return strcmp(a + 2, b + 2) == 0;
}

static void Particles_SetPos(CGameParticles *p, const vec3_t *pos) {
	p->birthRate = 0;
	p->initPosMin = *pos;
	p->initPosMax = p->initPosMin;
	p->initVelMin = (vec3_t){0.0f, 0.0f, 0.0f};
	p->initVelMax = p->initVelMin;
	p->initAccMin = (vec3_t){0.0f, 0.0f, 0.0f};
	p->initAccMax = p->initAccMin;
}

static void Particles_Emit(CGameParticles *p, uint32_t *ramp) {
	CGameParticles_Apply(p);
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, ramp);
	CGameParticles_EmitN(p, 1, 0);
}

static void Particles_SetSize(CGameParticles *p, float ini, float end, float life, byte sprite) {
	p->sizeIni = ini;
	p->sizeEnd = end;
	p->sizeLife = 0.5f;
	p->lifeTime = life;
	p->spriteIni = sprite;
	p->spriteEnd = sprite;
}

/* ---- cEmbeddedFXVehicle ---- */

cEmbeddedFXVehicle *__fastcall cEmbeddedFXVehicle_ctor(cEmbeddedFXVehicle *self) {
	self->vftable = CEMBEDDEDFXVEHICLE_VFTABLE;
	CGameModel_ctor(&self->model, NULL);
	CGameParticles_ctor(&self->follow);
	CGameParticles_ctor(&self->follow2);
	self->owner = NULL;
	self->traffic = 0;
	self->attached = NULL;
	self->sirens = NULL;
	self->other = NULL;
	return self;
}

void __fastcall cEmbeddedFXVehicle_dtor(cEmbeddedFXVehicle *self) {
	self->vftable = CEMBEDDEDFXVEHICLE_VFTABLE;
	CHECK(!self->sirens);
	CHECK(!self->other);
	CGameParticles_Release(&self->follow2);
	CGameParticles_Release(&self->follow);
	CGameModel_Reset(&self->model);
	cEmbeddedFX_dtor((cEmbeddedFX *)self);
}

cEmbeddedFXVehicle *__thiscall cEmbeddedFXVehicle_Delete(cEmbeddedFXVehicle *self, byte flags) {
	cEmbeddedFXVehicle_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

byte __fastcall cEmbeddedFXVehicle_Init(cEmbeddedFXVehicle *self) {
	int node;

	self->attached = NULL;
	self->rootPos = (vec3_t){0.0f, 0.0f, 0.0f};
	self->fading = 0;
	self->aiRange = 0;
	self->alpha = 1.0f;
	self->time = 0.0f;
	self->visible = 0;
	self->followed = 0;
	CGameModel_LoadName(&self->model, self->traffic ? g_FXTrafficEmbeddedFile : g_FXVehicleEmbeddedFile);
	CHECK(CGameModel_IsLoaded(&self->model));
	CGameModel_Disable(&self->model);
	node = CGameModel_FindRenderNode(&self->model, "Dummy01");
	CHECK(node > -1);
	CGameParticles_Attach(&self->follow, &self->model, node);
	CHECK(self->follow.ParticleSystem);
	if (!self->traffic) {
		node = CGameModel_FindRenderNode(&self->model, "Dummy02");
		CHECK(node > -1);
		CGameParticles_Attach(&self->follow2, &self->model, node);
		CHECK(self->follow2.ParticleSystem);
	}
	return 1;
}

static void Sirens_Delete(cEmbeddedFXVehicle *self) {
	cFXVehicleSirens_dtor(self->sirens);
	if (self->sirens) {
		operator_delete_sized(self->sirens, sizeof(cFXVehicleSirens));
		self->sirens = NULL;
	}
}

static void Other_Delete(cEmbeddedFXVehicle *self) {
	if (self->other) {
		self->other->vftable->Done(self->other);
		if (self->other) {
			self->other->vftable->Delete(self->other, 1);
			self->other = NULL;
		}
	}
}

void __fastcall cEmbeddedFXVehicle_Done(cEmbeddedFXVehicle *self) {
	self->attached = NULL;
	self->traffic = 0;
	CGameModel_Disable(&self->model);
	CGameParticles_Release(&self->follow);
	CGameParticles_Release(&self->follow2);
	if (self->sirens) {
		Sirens_Delete(self);
	}
	Other_Delete(self);
}

void __thiscall cEmbeddedFXVehicle_Configure(cEmbeddedFXVehicle *self, int unused) {
	cEntity *pl;
	void *ai;

	(void)unused;
	CHECK(self->owner);
	self->followed = 0;
	if (!g_SplitScreenGame) {
		/* World.h: the entity of player 0 */
		pl = g_pWorld->owners[0];
		if (pl && !pl->vftable->IsType(pl, TYEN_WITH_LIFE)) {
			CheckFailed("UsrEntities[i]->IsType(\"WithLife\")", "World.h", __TIMESTAMP__, __LINE__);
		}
		pl = g_pWorld->owners[0];
		if (pl && pl->vftable->IsType(pl, TYEN_CAR) && Vehicle_FollowName(pl)[0]) {
			self->followed = Str_EqualW(self->owner->name, Vehicle_FollowName(pl));
		}
	}
	if (g_SplitScreenGame) {
		self->traffic = 0;
		return;
	}
	ai = self->owner->vftable->GetAI(self->owner);
	if (!ai) {
		self->traffic = Vehicle_IsTraffic(self->owner);
		return;
	}
	int state = cAIVehicle_GetState(ai);
	if (state == 4 || state == 9) {
		self->traffic = 1;
	}
}

void __fastcall cEmbeddedFXVehicle_Update(cEmbeddedFXVehicle *self) {
	CGameModel *model;
	vec3_t pos, rot;
	float dt;

	CHECK(self->owner);
	model = Obj_Vtbl(self->owner)->GetModel(self->owner);
	CHECK(CGameModel_IsLoaded(model));
	if (CGameModel_IsEnabled(model) && CGameModel_IsVisible(model) && self->owner->flag_4c) {
		if (!self->visible || !CGameModel_IsEnabled(&self->model)) {
			CGameModel_Enable(&self->model);
			self->visible = 1;
			self->time = 0.0f;
		}
	} else if (self->visible || CGameModel_IsEnabled(&self->model)) {
		CGameModel_Disable(&self->model);
		self->visible = 0;
		self->time = 0.0f;
	}
	dt = cTimer_GetDelta(&g_pWorld->timer);
	self->time = self->time + dt;
	if (!self->visible && self->time > 1.0f && self->visible == CGameModel_IsVisible(model)) {
		return;
	}
	if (self->visible != CGameModel_IsVisible(model)) {
		self->visible = CGameModel_IsVisible(model);
	}
	CGameModel_SetSortZ(model, self->visible && Vehicle_Player(self->owner) ? 200.0f : 0.0f, 1.0f);
	CGameModel_GetPos(model, &pos);
	CGameModel_GetRot(model, &rot);
	CGameModel_SetPos(&self->model, &pos);
	CGameModel_SetRot(&self->model, &rot);

	int cam = g_pWorld->curCam;
	if (g_SplitScreenGame) {
		cam ^= 1;
	}
	vec3_t c = g_pWorld->camPos[cam];
	float dy = pos.y - c.y;
	float dx = pos.x - c.x;
	float dz = pos.z - c.z;
	float d2 = dy * dy + dx * dx;
	d2 = d2 + dz * dz;
	self->camDist = (float)__builtin_sqrt((double)d2);

	if (g_SplitScreenGame || !self->fading) {
		self->alpha = 1.0f;
	} else {
		float nearDist = g_MinDistReposAI;
		float farDist = g_MaxDistReposAI;
		if (self->aiRange && self->owner->vftable->GetAI(self->owner)) {
			cAIVehicle_GetFadeDists(self->owner->vftable->GetAI(self->owner), &nearDist, &farDist);
		}
		if (!self->aiRange || !(self->camDist >= nearDist)) {
			Float_Approach(&self->alpha, 1.0f, dt);
		} else if (self->camDist > farDist) {
			self->alpha = 0.0f;
		} else {
			self->alpha = Float_MinSS(self->time, 1.0f - (self->camDist - nearDist) / (farDist - nearDist));
		}
		uint32_t fx = CGameModel_GetEffect(Vehicle_Model(self->owner), NULL);
		if (self->alpha >= 1.0f) {
			if (fx == VEHICLE_FX_FADE) {
				Obj_Vtbl(self->owner)->SetEffect(self->owner, 0);
			}
		} else if (fx == 0 || fx == VEHICLE_FX_FADE) {
			if (fx == 0) {
				Obj_Vtbl(self->owner)->SetEffect(self->owner, VEHICLE_FX_FADE);
			}
			Obj_Vtbl(self->owner)->SetEffectParam(self->owner, 0, 1.0f - self->alpha);
		}
	}
	if (self->sirens) {
		cFXVehicleSirens_Update(self->sirens);
	}
	if (self->other) {
		self->other->vftable->Update(self->other);
	}
}

void __thiscall cEmbeddedFXVehicle_SetActive(cEmbeddedFXVehicle *self, byte on) {
	if (!on && self->visible) {
		cEmbeddedFXVehicle_Hide(self);
		self->attached = NULL;
	}
}

void __fastcall cEmbeddedFXVehicle_Hide(cEmbeddedFXVehicle *self) {
	self->attached = NULL;
	CGameModel_Disable(&self->model);
	self->visible = 0;
	self->alpha = 1.0f;
	self->time = 0.0f;
	self->fading = 0;
	if (CGameModel_GetEffect(Vehicle_Model(self->owner), NULL) == VEHICLE_FX_FADE) {
		Obj_Vtbl(self->owner)->SetEffect(self->owner, 0);
		Obj_Vtbl(self->owner)->SetEffectParam(self->owner, 0, 0.0f);
	}
}

void __fastcall cEmbeddedFXVehicle_Render(cEmbeddedFXVehicle *self) {
	CGameParticles_SetVisible(&self->follow, 0);
	CGameModel_RenderNow(&self->model, 0);
	CGameParticles_SetVisible(&self->follow, 1);
	if (!self->followed) {
		self->attached = NULL;
		return;
	}
	if (self->attached && self->owner && Vehicle_Life(self->owner) > 0) {
		FXVehicle_RenderAttached(self->attached);
		self->attached = NULL;
	}
	if (self->sirens && CGameModel_IsLoaded(g_SirenModel)) {
		CGameModel_RenderNow(g_SirenModel, 0);
	}
}

void __thiscall cEmbeddedFXVehicle_SetAttached(cEmbeddedFXVehicle *self, void *element) {
	self->attached = element;
}

void __thiscall cEmbeddedFXVehicle_SetOwner(cEmbeddedFXVehicle *self, cEntity *owner) {
	CGameModel *model;
	vec3_t t;
	void *p;

	self->attached = NULL;
	self->owner = owner;
	model = Obj_Vtbl(owner)->GetModel(owner);
	CHECK(CGameModel_IsLoaded(model));
	CGameModel_SetSortZ(&self->model, -Obj_Vtbl(self->owner)->GetRadius(self->owner), 1.0f);
	int root = CGameModel_FindRenderNode(model, "DC_Root");
	CHECK(root != -1);
	self->rootPos = (vec3_t){0.0f, 0.0f, 0.0f};
	self->rootPos = *CGameModel_NodePos(model, &t, root, 0);
	if (crt_strncmp(Vehicle_TypeName(self->owner), "SPoli", 5) == 0) {
		if (!self->sirens) {
			cFXVehicleSirens *s = operator_new(sizeof(cFXVehicleSirens));
			if (s) {
				s->owner = self;
			}
			self->sirens = s;
			cFXVehicleSirens_Init(s);
		} else {
			cFXVehicleSirens_dtor(self->sirens);
			cFXVehicleSirens_Init(self->sirens);
		}
	} else if (self->sirens) {
		Sirens_Delete(self);
	}
	Other_Delete(self);
	if (crt_stricmp(Vehicle_TypeName(self->owner), "TProbe") == 0) {
		p = operator_new(sizeof(cEmbeddedFXVehicleProbe));
		self->other = p ? (cEmbeddedFXVehicleSub *)cEmbeddedFXVehicleProbe_ctor(p, self) : NULL;
	} else if (crt_stricmp(Vehicle_TypeName(self->owner), "TVoteMayor") == 0) {
		p = operator_new(sizeof(cEmbeddedFXVehicleVote));
		cEmbeddedFXVehicleVote *v = p ? cEmbeddedFXVehicleVote_ctor(p, self) : NULL;
		self->other = (cEmbeddedFXVehicleSub *)v;
		v->type = VOTE_MAYOR;
	} else if (crt_stricmp(Vehicle_TypeName(self->owner), "TVoteBishop") == 0) {
		p = operator_new(sizeof(cEmbeddedFXVehicleVote));
		cEmbeddedFXVehicleVote *v = p ? cEmbeddedFXVehicleVote_ctor(p, self) : NULL;
		self->other = (cEmbeddedFXVehicleSub *)v;
		v->type = VOTE_BISHOP;
	}
	if (self->other) {
		self->other->vftable->Init(self->other);
	}
}

void __thiscall cEmbeddedFXVehicle_SetTraffic(cEmbeddedFXVehicle *self, byte on) {
	self->traffic = on;
}

void __fastcall cEmbeddedFXVehicle_StartFade(cEmbeddedFXVehicle *self) {
	self->fading = 1;
	self->alpha = 0.0f;
	self->time = 0.0f;
	Obj_Vtbl(self->owner)->SetEffect(self->owner, VEHICLE_FX_FADE);
	Obj_Vtbl(self->owner)->SetEffectParam(self->owner, 0, 1.0f);
}

void __thiscall cEmbeddedFXVehicle_SetAIRange(cEmbeddedFXVehicle *self, byte on) {
	self->aiRange = on;
}

/* ---- the police sirens ---- */

cFXVehicleSirens *__thiscall cFXVehicleSirens_ctor(cFXVehicleSirens *self, cEmbeddedFXVehicle *owner) {
	self->owner = owner;
	CHECK(owner);
	return self;
}

void __fastcall cFXVehicleSirens_dtor(cFXVehicleSirens *self) {
	(void)self;
	if (--g_SirenRefs <= 0 && CGameModel_IsLoaded(g_SirenModel)) {
		CGameParticles_Release(g_SirenParticles);
		CGameModel_LoadName(g_SirenModel, NULL);
	}
}

byte __fastcall cFXVehicleSirens_Init(cFXVehicleSirens *self) {
	char name[0x100];
	vec3_t t;
	float time = EFX_Rand(0.89999997615814209, 0.0);
	int refs = ++g_SirenRefs;

	self->time = time;
	self->lastTime = time;
	if (refs <= 1 && !CGameModel_IsLoaded(g_SirenModel)) {
		CGameModel_LoadName(g_SirenModel, g_FXVehicleEmbeddedFile);
		CHECK(CGameModel_IsLoaded(g_SirenModel));
		CGameModel_Disable(g_SirenModel);
		int node = CGameModel_FindRenderNode(g_SirenModel, "Dummy01");
		CHECK(node > -1);
		CGameParticles_Attach(g_SirenParticles, g_SirenModel, node);
		CHECK(g_SirenParticles->ParticleSystem);
	}
	CHECK(self->owner->owner);
	CGameModel *vm = Obj_Vtbl(self->owner->owner)->GetModel(self->owner->owner);
	for (int i = 0; i < 2; i++) {
		Str_PrintfQuiet(name, 0xff, "DF_SpeedTrail_%02d", i + 1);
		int node = CGameModel_FindRenderNode(vm, name);
		if (node != -1) {
			vec3_t root = self->owner->rootPos;
			const vec3_t *p = CGameModel_NodePos(vm, &t, node, 0);
			self->pos[i].x = p->x - root.x;
			self->pos[i].y = p->y - root.y;
			self->pos[i].z = p->z - root.z;
		}
	}
	return 1;
}

/* the two flashes at pos: the particles of the vehicle, the big ones of the shared model for the followed vehicle */
static void Sirens_Flash(cFXVehicleSirens *self, const vec3_t *pos, byte alarm, uint32_t *ramp1, uint32_t *ramp2) {
	cEmbeddedFXVehicle *fx = self->owner;
	float s = fx->camDist / g_FXVehicleSirenDistGrow;

	if (g_FXVehicleSirenSizeMin > s) {
		s = g_FXVehicleSirenSizeMin;
	} else if (!(g_FXVehicleSirenSizeMax > s)) {
		s = g_FXVehicleSirenSizeMax;
	}
	float k = (alarm ? 1.5f : 1.0f) * s;
	CGameParticles *p = &fx->follow;
	Particles_SetPos(p, pos);
	float mid = k * 75.0f;
	Particles_SetSize(p, k * 10.0f, mid, 0.08f, 2);
	Particles_Emit(p, ramp1);
	Particles_SetSize(p, mid, k * 350.0f, 0.1f, 5);
	Particles_Emit(p, ramp2);
	if (self->owner->followed) {
		p = g_SirenParticles;
		Particles_SetPos(p, pos);
		Particles_SetSize(p, 60.0f, 450.0f, 0.08f, 2);
		Particles_Emit(p, ramp1);
		Particles_SetSize(p, 450.0f, 2700.0f, 0.1f, 5);
		Particles_Emit(p, ramp2);
	}
}

static int Sirens_IsPolice(cEntity *v) {
	return Str_EqualW(((vehicleName_t)(*(void ***)v)[0xf4 / 4])(v, 0), "P_Police");
}

void __fastcall cFXVehicleSirens_Update(cFXVehicleSirens *self) {
	cEntity *v = self->owner->owner;
	vec3_t pos, rot;

	CHECK(v);
	if (!self->owner->visible || !(Vehicle_Speed(v) > 0.0f) || !(self->owner->alpha >= 0.8f)) {
		return;
	}
	if (Vehicle_Player(v) && !g_pApp->flags) {
		return;
	}
	byte alarm = g_pWorld->alarm.on;
	float dt = cTimer_GetDelta(&g_pWorld->timer);
	cEmbeddedFXVehicle *fx = self->owner;
	if (fx->followed) {
		CGameModel_GetPos(&fx->model, &pos);
		CGameModel_GetRot(&fx->model, &rot);
		CGameModel_SetPos(g_SirenModel, &pos);
		CGameModel_SetRot(g_SirenModel, &rot);
	}
	float t = self->time + dt;
	self->time = t;
	if (t > 0.9f) {
		self->time = 0.0f;
	}
	if (((0.1f > self->lastTime && self->time > 0.1f) || (0.2f > self->lastTime && self->time > 0.2f)) &&
		Sirens_IsPolice(v)) {
		Sirens_Flash(self, &self->pos[0], alarm, GAME_ARRAY(uint32_t, data_428574, RAMP_SIREN_A1), GAME_ARRAY(uint32_t, data_428550, RAMP_SIREN_A2));
	}
	if (((0.6f > self->lastTime && self->time > 0.6f) || (0.7f > self->lastTime && self->time > 0.7f)) &&
		Sirens_IsPolice(v)) {
		Sirens_Flash(self, &self->pos[1], alarm, GAME_ARRAY(uint32_t, data_4285bc, RAMP_SIREN_B1), GAME_ARRAY(uint32_t, data_428598, RAMP_SIREN_B2));
	}
	self->lastTime = self->time;
}

void cFXVehicleSirens_RenderModel(void) {
	if (CGameModel_IsLoaded(g_SirenModel)) {
		CGameModel_RenderNow(g_SirenModel, 0);
	}
}

/* ---- cEmbeddedFXVehicleProbe ---- */

cEmbeddedFXVehicleProbe *__thiscall cEmbeddedFXVehicleProbe_ctor(cEmbeddedFXVehicleProbe *self,
	cEmbeddedFXVehicle *fx) {
	self->vftable = CEMBEDDEDFXVEHICLEPROBE_VFTABLE;
	crt_eh_vector_ctor(self->lights, sizeof(CGameParticles), PROBE_LIGHTS, GAME_FUNC(CGameParticles_ctor, CGAMEPARTICLES_CTOR),
		GAME_FUNC(CGameParticles_Release, CGAMEPARTICLES_DTOR));
	CGameParticles_ctor(&self->trans);
	CGameTrail_ctor(&self->trail);
	self->fx = fx;
	CHECK(fx);
	self->lightTime = 0.0f;
	CGameTrail_SetMaxTrails(&self->trail, 0x14);
	CGameTrail_SetType(&self->trail, 3);
	CGameTrail_SetTexture(&self->trail, 0xf);
	self->transTime = 0.0f;
	self->ringTime = 0.0f;
	self->visible = 0;
	return self;
}

void __fastcall cEmbeddedFXVehicleProbe_dtor(cEmbeddedFXVehicleProbe *self) {
	self->vftable = CEMBEDDEDFXVEHICLEPROBE_VFTABLE;
	CGameParticles_Release(&self->trans);
	for (int i = 0; i < PROBE_LIGHTS; i++) {
		CGameParticles_Release(&self->lights[i]);
	}
	CGameTrail_dtor(&self->trail);
	CGameParticles_Release(&self->trans);
	crt_eh_vector_dtor(self->lights, sizeof(CGameParticles), PROBE_LIGHTS, GAME_FUNC(CGameParticles_Release, CGAMEPARTICLES_DTOR));
	cEmbeddedFX_dtor((cEmbeddedFX *)self);
}

cEmbeddedFXVehicleProbe *__thiscall cEmbeddedFXVehicleProbe_Delete(cEmbeddedFXVehicleProbe *self, byte flags) {
	cEmbeddedFXVehicleProbe_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* the particles of the model, CHECKed */
static int Particles_AttachNode(CGameParticles *p, CGameModel *model, const char *name) {
	int node = CGameModel_FindRenderNode(model, name);

	CHECK(node > -1);
	CGameParticles_Attach(p, model, node);
	CHECK(p->ParticleSystem);
	return node;
}

/* the first n particles of a light colored at random, alpha 8 */
static void Probe_InitLight(CGameParticles *p, int n, int redMod, int redBase, int mod, float size) {
	vec3_t pos;

	for (int i = 0; i < n; i++) {
		CGameParticles_GetParticle(p, i, &pos, NULL, NULL, NULL, NULL);
		uint32_t r = (uint32_t)(crt_rand() % redMod + redBase) << 16;
		uint32_t g = (uint32_t)(crt_rand() % mod) << 8;
		uint32_t b = (uint32_t)(crt_rand() % mod);
		CGameParticles_SetParticle(p, i, pos.x, pos.y, pos.z, size, 0x08000000 | r | g | b, 2, 0.0f);
	}
}

byte __fastcall cEmbeddedFXVehicleProbe_Init(cEmbeddedFXVehicleProbe *self) {
	CGameModel *model = Obj_Vtbl(self->fx->owner)->GetModel(self->fx->owner);

	if (!model || !CGameModel_IsLoaded(model)) {
		CheckFailed("model && model->HasModel()", __FILE__, __TIMESTAMP__, __LINE__);
	}
	self->transNode = Particles_AttachNode(&self->trans, model, "DF_Sonda_Transmision");
	Particles_AttachNode(&self->lights[0], model, "DF_Sonda_Glows_A");
	Particles_AttachNode(&self->lights[1], model, "DF_Sonda_Glows_B");
	Particles_AttachNode(&self->lights[2], model, "DF_Sonda_Glows_C");
	Particles_AttachNode(&self->lights[3], model, "DF_Sonda_Glows_D");
	Probe_InitLight(&self->lights[0], 5, 30, 0xdc, 10, 125.0f);
	Probe_InitLight(&self->lights[1], 4, 20, 0xe6, 20, 150.0f);
	Probe_InitLight(&self->lights[2], 5, 20, 0xe6, 20, 150.0f);
	Probe_InitLight(&self->lights[3], 4, 30, 0xdc, 10, 125.0f);
	return 1;
}

void __fastcall cEmbeddedFXVehicleProbe_Done(cEmbeddedFXVehicleProbe *self) {
	(void)self;
}

void __thiscall cEmbeddedFXVehicleProbe_Configure(cEmbeddedFXVehicleProbe *self, int unused) {
	(void)self;
	(void)unused;
}

/* a light flash in frac 0..1: two particles at particle src of light p */
static void Probe_Flash(CGameParticles *p, int src, int i1, int i2, float frac, float s) {
	vec3_t pos;
	float a = 1.0f - frac;
	int alpha = 0xff - (int)(((a + a) - 3.0f) * (a * a) * -255.0f);

	CGameParticles_GetParticle(p, src, &pos, NULL, NULL, NULL, NULL);
	int c = alpha + 8;
	if (c > 0xff) {
		c = 0xff;
	}
	CGameParticles_SetParticle(p, i1, pos.x, pos.y, pos.z, (1.0f - frac * 0.5f) * s * 250.0f,
		((uint32_t)c << 24) | 0xff8080, 2, 0.0f);
	c = alpha + 0x10;
	if (c > 0xff) {
		c = 0xff;
	}
	CGameParticles_SetParticle(p, i2, pos.x, pos.y, pos.z, (1.0f - frac * 0.5f) * s * 950.0f,
		((uint32_t)c << 24) | 0xff0000, 5, 0.0f);
}

void __fastcall cEmbeddedFXVehicleProbe_Update(cEmbeddedFXVehicleProbe *self) {
	CGameModel *model = Obj_Vtbl(self->fx->owner)->GetModel(self->fx->owner);
	vec3_t tmp, mrot, mpos, mrot2;
	float frac, s, t;

	if (!CGameModel_IsLoaded(model)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(model));
	byte vis = CGameModel_IsVisible(model);
	if (!vis) {
		if (self->visible) {
			CGameTrail_Clear(&self->trail);
			self->visible = 0;
		}
		return;
	}
	self->visible = vis;
	float dt = cTimer_GetDelta(&g_pWorld->timer);
	self->lightTime = dt + self->lightTime;
	if (self->lightTime > 1.8f) {
		self->lightTime = 0.0f;
	}
	s = self->fx->camDist / g_FXVehicleProbeLightDistGrow;
	if (g_FXVehicleProbeLightSizeMin > s) {
		s = g_FXVehicleProbeLightSizeMin;
	} else if (!(g_FXVehicleProbeLightSizeMax > s)) {
		s = g_FXVehicleProbeLightSizeMax;
	}
	if (FX_InRange(self->lightTime, 0.0f, 0.4f, &frac)) {
		Probe_Flash(&self->lights[0], 1, 5, 6, frac, s);
	}
	if (FX_InRange(self->lightTime, 0.6f, 1.0f, &frac)) {
		Probe_Flash(&self->lights[3], 3, 4, 5, frac, s);
	}

	self->transTime = self->transTime + dt;
	if (self->transTime > 1.0f) {
		CGameParticles *p = &self->trans;
		self->transTime = 0.0f;
		p->birthRate = 0;
		for (int i = 0; i < 3; i++) {
			vec3_t rot = {0.0f, (float)i * 2.094395160675049f, 0.0f};
			vec3_t v = {0.0f, -130.0f, 850.0f};
			p->initPosMin = *Vec3_Rotate(&tmp, &v, &rot);
			p->initPosMax = p->initPosMin;
			for (int j = 0; j < 3; j++) {
				p->sizeIni = 100.0f;
				p->sizeEnd = 450.0f;
				p->rotIni = 0.0f;
				p->rotEnd = 0.0f;
				p->rotVar = 0.0f;
				p->lifeTime = 0.3f;
				p->sizeLife = 0.2f;
				p->spriteIni = 2;
				p->spriteEnd = 2;
				CGameParticles_SetColorRampOn(p, 1);
				CGameParticles_SetColorRamp(p, GAME_ARRAY(uint32_t, cEmbeddedFXVehicleProbe_Update_4285e0, RAMP_PROBE_TRANS));
				CGameParticles_Apply(p);
				CGameParticles_EmitDt(p, 1, 0, (float)j / 3.0f);
			}
		}
	}

	/* the rings: three trails of three segments around the dish, turned with the animation */
	CGameModel_ChanTime(model, 0, &t);
	float ang = (t / 5.0f) * 6.2831854820251465f;
	vec3_t off = {0.0f, -850.0f, 0.0f};
	const vec3_t *o = Vec3_Rotate(&tmp, &off, CGameModel_GetRot(model, &mrot));
	const vec3_t *mp = CGameModel_GetPos(model, &mpos);
	float cx = mp->x + o->x;
	float cy = mp->y + o->y;
	float cz = mp->z + o->z;
	CGameModel_GetRot(model, &mrot2);
	CGameTrail_Reset(&self->trail);
	CGameTrail_SetOrient(&self->trail, (vec3_t){cx, cy, cz}, 1);
	self->ringTime = dt * 4.0f + self->ringTime;
	if (self->ringTime >= 1.0f) {
		self->ringTime = 0.0f;
	}
	for (int k = 0; k < 3; k++) {
		vec3_t r3 = {0.0f, (float)k * 2.094395160675049f + ang, 0.0f};
		vec3_t up = {0.0f, 0.0f, 1.0f};
		vec3_t side = {1.0f, 0.0f, 0.0f};
		vec3_t t1, dir, t2, sd;
		Vec3_Rotate(&dir, Vec3_Rotate(&t1, &up, &r3), &mrot2);
		Vec3_Rotate(&sd, Vec3_Rotate(&t2, &side, &r3), &mrot2);
		float px = dir.x * 850.0f + cx;
		float py = dir.y * 850.0f + cy;
		float pz = dir.z * 850.0f + cz;
		for (int m = 0; m < 3; m++) {
			float rt = self->ringTime;
			float w = (float)m + rt + 0.2f;
			int a = 0x30;
			if (m == 0) {
				a = 0xff - (int)(rt * 207.0f);
			} else if (m == 2) {
				a = 0x30 - (int)(rt * -207.0f);
			}
			uint32_t color = ((uint32_t)a << 24) | 0xff1405;
			float width = w * 360.0f;
			CGameTrail_Add(&self->trail, (vec3_t){px - sd.x * 180.0f * w, py - sd.y * 180.0f * w,
				pz - sd.z * 180.0f * w}, width, color);
			CGameTrail_Add(&self->trail, (vec3_t){sd.x * 180.0f * w + px, sd.y * 180.0f * w + py,
				sd.z * 180.0f * w + pz}, width, color);
		}
	}
}

void __thiscall cEmbeddedFXVehicleProbe_SetActive(cEmbeddedFXVehicleProbe *self, byte on) {
	if (!on) {
		CGameTrail_Clear(&self->trail);
	}
}

void __fastcall cEmbeddedFXVehicleProbe_ClearTrail(cEmbeddedFXVehicleProbe *self) {
	CGameTrail_Clear(&self->trail);
}

/* ---- cEmbeddedFXVehicleVote ---- */

cEmbeddedFXVehicleVote *__thiscall cEmbeddedFXVehicleVote_ctor(cEmbeddedFXVehicleVote *self,
	cEmbeddedFXVehicle *fx) {
	self->vftable = CEMBEDDEDFXVEHICLEVOTE_VFTABLE;
	CGameParticles_ctor(&self->glow);
	crt_eh_vector_ctor(self->trails, sizeof(CGameTrail), 2, GAME_FUNC(CGameTrail_ctor, CGAMETRAIL_CTOR), GAME_FUNC(CGameTrail_dtor, CGAMETRAIL_DTOR));
	crt_eh_vector_ctor(self->motor, sizeof(CGameParticles), VOTE_MOTORS, GAME_FUNC(CGameParticles_ctor, CGAMEPARTICLES_CTOR),
		GAME_FUNC(CGameParticles_Release, CGAMEPARTICLES_DTOR));
	self->fx = fx;
	CHECK(fx);
	self->visible = 0;
	self->type = 0;
	return self;
}

void __fastcall cEmbeddedFXVehicleVote_dtor(cEmbeddedFXVehicleVote *self) {
	self->vftable = CEMBEDDEDFXVEHICLEVOTE_VFTABLE;
	crt_eh_vector_dtor(self->motor, sizeof(CGameParticles), VOTE_MOTORS, GAME_FUNC(CGameParticles_Release, CGAMEPARTICLES_DTOR));
	crt_eh_vector_dtor(self->trails, sizeof(CGameTrail), 2, GAME_FUNC(CGameTrail_dtor, CGAMETRAIL_DTOR));
	CGameParticles_Release(&self->glow);
	cEmbeddedFX_dtor((cEmbeddedFX *)self);
}

cEmbeddedFXVehicleVote *__thiscall cEmbeddedFXVehicleVote_Delete(cEmbeddedFXVehicleVote *self, byte flags) {
	cEmbeddedFXVehicleVote_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

byte __fastcall cEmbeddedFXVehicleVote_Init(cEmbeddedFXVehicleVote *self) {
	CGameModel *model = Obj_Vtbl(self->fx->owner)->GetModel(self->fx->owner);

	if (!model || !CGameModel_IsLoaded(model)) {
		CheckFailed("model && model->HasModel()", __FILE__, __TIMESTAMP__, __LINE__);
	}
	self->glowNode = CGameModel_FindRenderNode(model, "DS_Electoral_ArzobispoGlow");
	CHECK(self->glowNode > -1);
	CGameParticles_Attach(&self->glow, model, self->glowNode);
	CHECK(self->glow.ParticleSystem);
	Particles_AttachNode(&self->motor[0], model, "DF_MotorTrust_01");
	Particles_AttachNode(&self->motor[1], model, "DF_MotorTrust_02");
	Particles_AttachNode(&self->motor[2], model, "DF_MotorTrust_03");
	self->rootNode = CGameModel_FindRenderNode(model, "DC_Root");
	CHECK(self->rootNode > -1);
	for (int i = 0; i < 2; i++) {
		CGameTrail_SetMaxTrails(&self->trails[i], 0x1f);
		CGameTrail_SetType(&self->trails[i], 3);
		CGameTrail_SetPremul(&self->trails[i], 1);
	}
	return 1;
}

void __fastcall cEmbeddedFXVehicleVote_Done(cEmbeddedFXVehicleVote *self) {
	self->glowNode = -1;
	CGameParticles_Release(&self->glow);
	for (int i = 0; i < VOTE_MOTORS; i++) {
		CGameParticles_Release(&self->motor[i]);
	}
}

void __thiscall cEmbeddedFXVehicleVote_Configure(cEmbeddedFXVehicleVote *self, int unused) {
	(void)self;
	(void)unused;
}

void __fastcall cEmbeddedFXVehicleVote_Update(cEmbeddedFXVehicleVote *self) {
	CGameModel *model = Obj_Vtbl(self->fx->owner)->GetModel(self->fx->owner);
	vec3_t pos, rot, tmp, tmp2;
	uint32_t color;

	if (!CGameModel_IsLoaded(model)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(model));
	byte vis = CGameModel_IsVisible(model);
	if (!vis) {
		if (self->visible) {
			cEmbeddedFXVehicleVote_ClearTrails(self);
			self->visible = 0;
		}
		return;
	}
	self->visible = vis;
	CGameModel_GetPos(model, &pos);
	CGameModel_GetRot(model, &rot);
	CGameParticles *glow = &self->glow;
	CGameParticles_NumParticles(glow);
	int pi = 0;
	for (int side = 0; side < 2; side++) {
		vec3_t off = {0.0f, side ? -180.0f : -40.0f, 0.0f};
		const vec3_t *o = Vec3_Rotate(&tmp, &off, &rot);
		float px = pos.x + o->x;
		float py = o->y + pos.y;
		float pz = o->z + pos.z;
		float sz = EFX_Rand(16.0, 32.0);
		int type = self->type;
		if (type == VOTE_MAYOR || type == VOTE_BISHOP) {
			color = ((uint32_t)(crt_rand() % 16 + 8) << 24) | (type == VOTE_MAYOR ? 0xe8e8ff : 0xffe8ff);
			CGameParticles_SetParticle(glow, pi, px, py, pz, sz + sz, color, 1, 0.0f);
			pi++;
			color = ((uint32_t)(crt_rand() % 16 + 0x18) << 24) | (type == VOTE_MAYOR ? 0x8080ff : 0xff80ff);
			CGameParticles_SetParticle(glow, pi, px, py, pz, sz * 6.0f, color, 5, 0.0f);
			pi++;
			color = ((uint32_t)(crt_rand() % 4 + 0x40) << 24) | (type == VOTE_MAYOR ? 0x2020ff : 0xff20ff);
			CGameParticles_SetParticle(glow, pi, px, py, pz, sz * 18.0f, color, 5, 0.0f);
			pi++;
		}
		CGameTrail *tr = &self->trails[side];
		CGameTrail_Reset(tr);
		for (int n = 15; n; n--) {
			uint32_t a = (uint32_t)(crt_rand() % 64 + 0x20) << 24;
			float k = side ? 1280.0f : 2280.0f;
			float d = (float)((double)(crt_rand() & 0x7fff) * g_RandScale * (double)(k - 540.0f) + 540.0);
			float k2 = side ? -0.8999999761581421f : 1.0f;
			vec3_t ra = {0.0f, 0.0f, 0.0f};
			ra.y = EFX_Rand(6.2831854820251465, 0.0);
			vec3_t rb = {0.0f, k2, 0.0f};
			rb.x = EFX_Rand(0.5999999642372131, 0.30000001192092896);
			vec3_t t1, dir;
			Vec3_Rotate(&dir, Vec3_Rotate(&t1, &rb, &ra), &rot);
			float qx = dir.x * d + px;
			float qy = dir.y * d + py;
			float qz = dir.z * d + pz;
			float w = EFX_Rand(28.0, 54.0);
			if (self->type == VOTE_MAYOR) {
				CGameTrail_Add(tr, (vec3_t){px, py, pz}, w, a | 0x2040);
				color = a | 0x810;
			} else if (self->type == VOTE_BISHOP) {
				CGameTrail_Add(tr, (vec3_t){px, py, pz}, w, a | 0x402040);
				color = a | 0x100810;
			} else {
				continue;
			}
			CGameTrail_Add(tr, (vec3_t){qx, qy, qz}, w, color);
		}
	}

	/* the glow around the vehicle */
	for (int n = 20; n; n--) {
		float r = EFX_Rand(225.0, 300.0);
		const vec3_t *dv = FX_RandomDir(&tmp);
		float dx = dv->x * r;
		float dy = dv->y * r;
		float dz = dv->z * r;
		vec3_t up = {0.0f, 1850.0f, 0.0f};
		const vec3_t *u = Vec3_Rotate(&tmp2, &up, &rot);
		float qx = u->x + pos.x + dx;
		float qy = u->y + pos.y + dy;
		float qz = u->z + pos.z + dz;
		float sz = EFX_Rand(400.0, 2200.0);
		if (self->type == VOTE_MAYOR) {
			uint32_t g = (uint32_t)(crt_rand() % 32 + 0x40) << 8;
			uint32_t rd = (uint32_t)(crt_rand() % 32 + 0x40) << 16;
			color = rd | g | 0xff;
		} else {
			color = ((uint32_t)(crt_rand() % 32 + 0x40) << 8) | 0xff00ff;
		}
		float spin = EFX_Rand(6.2831854820251465, 0.0);
		uint32_t al = (uint32_t)(crt_rand() % 16 - 0x2c) << 24;
		CGameParticles_SetParticle(glow, pi, qx, qy, qz, sz, al | (color & 0xffffff), 5, spin);
		pi++;
	}

	/* the motors: two particles placed and the emission parameters */
	vec3_t m0 = {0.0f, -30.0f, 0.0f};
	vec3_t m1 = {0.0f, -25.0f, 0.0f};
	for (int i = 0; i < VOTE_MOTORS; i++) {
		CGameParticles *p = &self->motor[i];
		float s0 = EFX_Rand(50.0, 400.0);
		color = ((uint32_t)(crt_rand() % 32 + 0x40) << 24) | 0xfff078;
		CGameParticles_SetParticle(p, 0, m0.x, m0.y, m0.z, s0, color, 1, 0.0f);
		float s1 = EFX_Rand(100.0, 550.0);
		color = ((uint32_t)(crt_rand() % 32 - 0x80) << 24) | 0xf0780a;
		CGameParticles_SetParticle(p, 1, m1.x, m1.y, m1.z, s1, color, 6, 0.0f);
		p->birthRate = 15;
		p->initPosMin = (vec3_t){-15.0f, 16.0f, -15.0f};
		p->initPosMax = (vec3_t){15.0f, 24.0f, 15.0f};
		p->sizeIni = EFX_Rand(50.0, 310.0);
		p->sizeEnd = EFX_Rand(50.0, 120.0);
		uint32_t g = (uint32_t)(crt_rand() % 16 + 0x78) << 8;
		uint32_t rd = (uint32_t)(crt_rand() % 16 + 0xe1) << 16;
		uint32_t b = (uint32_t)(crt_rand() % 8 + 0x1e);
		p->colorIni = b | rd | g | 0x18000000;
		g = (uint32_t)(crt_rand() % 8 + 0x19) << 8;
		rd = (uint32_t)(crt_rand() % 16 + 0xcd) << 16;
		p->colorEnd = rd | g | 0xc0000000;
		p->initVelMin = (vec3_t){-200.0f, 2325.0f, -200.0f};
		p->initVelMax = (vec3_t){200.0f, 3000.0f, 200.0f};
		p->spriteIni = 9;
		p->spriteEnd = 12;
		p->rotIni = EFX_Rand(6.2831854820251465, 0.0);
		p->rotVar = 6.2831854820251465f;
		p->rotEnd = p->rotIni +
			(float)((double)(crt_rand() & 0x7fff) * g_RandScale * 2.094395160675049 - 1.0471975803375244);
		p->lifeTime = 0.2f - EFX_Rand(0.030000001192092896, 0.019999999552965164);
		CGameParticles_Apply(p);
	}
}

void __thiscall cEmbeddedFXVehicleVote_SetActive(cEmbeddedFXVehicleVote *self, byte on) {
	if (!on) {
		cEmbeddedFXVehicleVote_ClearTrails(self);
	}
}

void __fastcall cEmbeddedFXVehicleVote_ClearTrails(cEmbeddedFXVehicleVote *self) {
	for (int i = 0; i < 2; i++) {
		CGameTrail_Clear(&self->trails[i]);
	}
}

void __thiscall cEmbeddedFXVehicleVote_SetType(cEmbeddedFXVehicleVote *self, int type) {
	self->type = type;
}
