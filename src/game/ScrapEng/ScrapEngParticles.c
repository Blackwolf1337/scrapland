/*
 * ScrapEngParticles.cpp of the game: CGameParticles, see include/scrap/scrapparticles.h.
 */
#include "scrap/scrapparticles.h"
#include "scrap/scrapmain.h"
#include "scrap/textura.h"

#define SP_CHECK(self) \
	do { \
		if (!(self)->SistPart) { \
			CheckFailed("SistPart", __FILE__, __TIMESTAMP__, __LINE__); \
		} \
	} while (0)

#define SP_CHECK_INDEX(self, i) \
	do { \
		if ((i) >= (self)->SistPart->params.nParticulas) { \
			CheckFailed("index<SistPart->GetNParticles()", __FILE__, __TIMESTAMP__, __LINE__); \
		} \
	} while (0)

#define SP_CHECK_PARTICLE(p) \
	do { \
		if (!(p)) { \
			CheckFailed("Particle", __FILE__, __TIMESTAMP__, __LINE__); \
		} \
	} while (0)

static const vec3_t sp_zero = { 0.0f, 0.0f, 0.0f };

CGameParticles *__fastcall CGameParticles_ctor(CGameParticles *self) {
	CGameParticles_Init(self);
	return self;
}

void __fastcall CGameParticles_Init(CGameParticles *self) {
	self->Node = NULL;
	self->SistPart = NULL;
	self->colorIni = 0xffffff;
	self->colorEnd = 0xffffff;
	self->sizeIni = 1.0f;
	self->sizeEnd = 1.0f;
	self->sizeVar = 0.0f;
	self->sizeLife = 1.0f;
	self->birthRate = 100;
	self->lifeTime = 1.0f;
	self->initPos = sp_zero;
	self->initPosMin = sp_zero;
	self->initPosMax = sp_zero;
	self->initVel = sp_zero;
	self->initVelMin = sp_zero;
	self->initVelMax = sp_zero;
	self->initAcc = sp_zero;
	self->initAccMin = sp_zero;
	self->initAccMax = sp_zero;
	self->rotIni = 0.0f;
	self->rotEnd = 0.0f;
	self->rotVar = 0.0f;
	self->spriteIni = 1;
	self->spriteEnd = 1;
	self->sparkVelScale = 1.0f;
	self->sparkMaxSize = 0x1.fffffep+127f;
	self->bounce = 0;
}

void __fastcall CGameParticles_Release(CGameParticles *self) {
	NodoR_Unref(&self->Node);
	self->SistPart = NULL;
}

void __thiscall CGameParticles_Attach(CGameParticles *self, CGameModel *model, uint32_t i) {
	if (!model) {
		CheckFailed("Model", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (self->Node || self->SistPart) {
		CheckFailed("!Node && !SistPart", __FILE__, __TIMESTAMP__, __LINE__);
	}
	self->Node = CGameModel_NodoRRef(model, i);
	if (!self->Node || !self->Node->obj3D) {
		CheckFailed("Node && Node->obj3D", __FILE__, __TIMESTAMP__, __LINE__);
	}
	IObjeto3D *obj = self->Node->obj3D;
	if (obj->clase != CLASE_OBJ3D_SISTPART) {
		CheckFailed("Obj3D->obtClase() == CLASE_OBJ3D_SISTPART", "objeto3d.h", __TIMESTAMP__, __LINE__);
	}
	self->SistPart = (CSistPart *)obj;
}

void __fastcall CGameParticles_Apply(CGameParticles *self) {
	SP_CHECK(self);
	SistPartParams *p = &self->SistPart->params;
	p->colorIni = self->colorIni;
	p->colorEnd = self->colorEnd;
	p->sizeIni = self->sizeIni;
	p->sizeEnd = self->sizeEnd;
	p->sizeVar = self->sizeVar;
	p->sizeLife = self->sizeLife;
	p->birthRate = self->birthRate;
	p->lifeTime = self->lifeTime;
	p->initPos = self->initPos;
	p->initPosMin = self->initPosMin;
	p->initPosMax = self->initPosMax;
	p->initVel = self->initVel;
	p->initVelMin = self->initVelMin;
	p->initVelMax = self->initVelMax;
	p->initAcc = self->initAcc;
	p->initAccMin = self->initAccMin;
	p->initAccMax = self->initAccMax;
	p->spriteIni = self->spriteIni;
	p->spriteEnd = self->spriteEnd;
	p->rotIni = self->rotIni;
	p->rotEnd = self->rotEnd;
	p->rotVar = self->rotVar;
	p->sparkVelScale = self->sparkVelScale;
	p->sparkMaxSize = self->sparkMaxSize;
	p->bounce = self->bounce;
}

/* p made inactive, an active one at the end of its life */
static void CGameParticles_KillParticle(CGameParticles *self, CParticula *p) {
	CSistPart_SetActive(self->SistPart, p, 0);
	if (PARTICULA_TYPE(p) == PARTICULA_ACTIVE) {
		CParticulaActive *a = (CParticulaActive *)p;
		a->b34 = 0;
		a->t = 1.0f;
		a->life = 0.0f;
	}
}

void __fastcall CGameParticles_KillAll(CGameParticles *self) {
	SP_CHECK(self);
	for (int i = 0; i < self->SistPart->params.nParticulas; i++) {
		CParticula *p = self->SistPart->Particles[i];
		SP_CHECK_PARTICLE(p);
		CGameParticles_KillParticle(self, p);
	}
	self->SistPart->numActive = 0;
}

void __thiscall CGameParticles_Kill(CGameParticles *self, int i) {
	SP_CHECK(self);
	SP_CHECK_INDEX(self, i);
	CParticula *p = self->SistPart->Particles[i];
	SP_CHECK_PARTICLE(p);
	CGameParticles_KillParticle(self, p);
}

void __thiscall CGameParticles_KillRange(CGameParticles *self, int from, int to) {
	SP_CHECK(self);
	if (from < 0 || to < from || to > self->SistPart->params.nParticulas) {
		CheckFailed("index_from>=0 && index_to>=index_from && index_to<=SistPart->GetNParticles()", __FILE__,
			__TIMESTAMP__, __LINE__);
	}
	for (; from < to; from++) {
		CParticula *p = self->SistPart->Particles[from];
		SP_CHECK_PARTICLE(p);
		CGameParticles_KillParticle(self, p);
	}
}

void __thiscall CGameParticles_SetParticle(CGameParticles *self, int i, float x, float y, float z, float size,
	uint32_t color, int sprite, float rot) {
	SP_CHECK(self);
	SP_CHECK_INDEX(self, i);
	CParticula *p = self->SistPart->Particles[i];
	SP_CHECK_PARTICLE(p);
	CSistPart_SetActive(self->SistPart, p, 1);
	p->pos.x = x;
	p->pos.y = y;
	p->pos.z = z;
	p->color = color;
	p->size = size;
	p->rot = rot;
	if (PARTICULA_TYPE(p) == PARTICULA_ACTIVE) {
		CParticulaActive *a = (CParticulaActive *)p;
		a->b34 = 1;
		a->t = 0.0f;
	}
	CMultiSprite *ms = self->SistPart->MultiSpr;
	if (ms) {
		if (sprite < 0 || sprite >= ms->n) {
			CheckFailed("spriteIndex>=0 && spriteIndex<MultiSpr->GetSpriteCount()", __FILE__, __TIMESTAMP__,
				__LINE__);
		}
		CMultiSprite_GetUV(ms, (uint32_t)sprite, p->uv0, p->uv1);
	}
}

byte __thiscall CGameParticles_GetParticle(CGameParticles *self, int i, vec3_t *pos, vec3_t *vel, float *t,
	uint32_t *color, float *size) {
	SP_CHECK(self);
	SP_CHECK_INDEX(self, i);
	CParticula *p = self->SistPart->Particles[i];
	if (pos) {
		*pos = p->pos;
	}
	if (color) {
		*color = p->color;
	}
	if (size) {
		*size = p->size;
	}
	if (PARTICULA_TYPE(p) == PARTICULA_ACTIVE) {
		CParticulaActive *a = (CParticulaActive *)p;
		if (t) {
			*t = a->t;
		}
		if (vel) {
			*vel = a->vel;
		}
	}
	return p->active;
}

int __fastcall CGameParticles_NumActive(const CGameParticles *self) {
	return self->SistPart->numActive;
}

int __fastcall CGameParticles_NumParticles(const CGameParticles *self) {
	return self->SistPart->params.nParticulas;
}

float __fastcall CGameParticles_GetAlpha(const CGameParticles *self) {
	SP_CHECK(self);
	return self->SistPart->alpha;
}

void __thiscall CGameParticles_SetAlpha(CGameParticles *self, float alpha) {
	SP_CHECK(self);
	CSistPart_SetAlpha(self->SistPart, alpha);
}

byte __fastcall CGameParticles_IsEnabled(const CGameParticles *self) {
	return self->SistPart->enabled;
}

void __thiscall CGameParticles_SetEnabled(CGameParticles *self, byte on) {
	self->SistPart->enabled = on;
}

byte __fastcall CGameParticles_IsVisible(const CGameParticles *self) {
	return self->SistPart->visible;
}

void __thiscall CGameParticles_SetVisible(CGameParticles *self, byte on) {
	self->SistPart->visible = on;
}

/* flag of params.flags set to on (the flag is toggled when the bit differs from the byte) */
static void CGameParticles_SetFlag(CSistPart *sp, int shift, byte on) {
	uint32_t flags = sp->params.flags;
	if ((byte)((flags >> shift) & 1) != on) {
		sp->params.flags = flags ^ (1u << shift);
	}
}

void __thiscall CGameParticles_SetColorRampOn(CGameParticles *self, byte on) {
	SP_CHECK(self);
	CGameParticles_SetFlag(self->SistPart, 5, on);
}

void __thiscall CGameParticles_SetSparksOn(CGameParticles *self, byte on) {
	SP_CHECK(self);
	CGameParticles_SetFlag(self->SistPart, 3, on);
}

void __thiscall CGameParticles_SetColorRamp(CGameParticles *self, uint32_t *ramp) {
	SP_CHECK(self);
	CSistPart_SetColorRamp(self->SistPart, ramp);
}

void __thiscall CGameParticles_SetEmitPos(CGameParticles *self, vec3_t *pos, byte n) {
	SP_CHECK(self);
	CSistPart_SetEmitPos(self->SistPart, pos, n);
}

void __thiscall CGameParticles_EmitN(CGameParticles *self, int n, byte force) {
	CSistPart_EmitN(self->SistPart, n, force);
}

void __thiscall CGameParticles_EmitM(CGameParticles *self, int n, byte force, const matrix_t *m) {
	const matrix_t *d3d = Eng_Identity(m);
	CSistPart_EmitM(self->SistPart, n, force, d3d);
}

void __thiscall CGameParticles_EmitDt(CGameParticles *self, int n, byte force, float dt) {
	CSistPart_EmitDt(self->SistPart, n, force, dt);
}

void __thiscall CGameParticles_Emit(CGameParticles *self, int n, byte force, float dt, const matrix_t *m) {
	const matrix_t *d3d = Eng_Identity(m);
	CSistPart_Emit(self->SistPart, n, force, dt, d3d);
}
