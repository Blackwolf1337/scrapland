/*
 * Engine/SistPart.cpp of the engine3d: CParticleSystem and its particles, see include/scrap/particlesystem.h. Also the small
 * functions of Engine/grafico2d.cpp in front of it.
 */
#include <stddef.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/fonts.h"
#include "scrap/texture.h"
#include "scrap/e3dworld.h"
#include "scrap/particlesystem.h"
#include "scrap/misc.h"
#include "scrap/staticinit.h"

#define PARTICLESYSTEM_DEFAULT_COLOR 0xffffff

#define PARTICLE_UPDATE(p, sp, dt, ms) ((p)->vftable->Update((p), (sp), (dt), (ms)))

/* a random value between a and b, the way the exe computes it */
static float ParticleSystem_RandLerp(float a, float b) {
	int r = crt_rand();
	return (float)((double)(r & 0x7fff) * (1.0 / 32767.0) * (double)(b - a) + (double)a);
}

/* the time of ms milliseconds in seconds */
static float ParticleSystem_Seconds(uint32_t ms) {
	return (float)(double)ms * 0.001f;
}

CParticle *CParticleActive_ctor(CParticle *self) {
	self->vftable = CPARTICLEACTIVE_VFTABLE;
	CParticleActive_Init(self);
	return self;
}

CParticleStatic *CParticleStatic_ctor(CParticleStatic *self) {
	self->vftable = CPARTICLESTATIC_VFTABLE;
	CParticleStatic_Init(self);
	return self;
}

int CParticleActive_Type(CParticle *self) {
	(void)self;
	return PARTICLE_ACTIVE;
}

int CParticleStatic_Type(CParticleStatic *self) {
	(void)self;
	return PARTICLE_STATIC;
}

/* the members of both kinds */
static void Particle_InitCommon(CParticle *p) {
	p->pos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	p->uv0[0] = 0.0f;
	p->uv0[1] = 0.0f;
	p->uv1[0] = 1.0f;
	p->active = 0;
	p->scale = 1.0f;
}

void CParticleActive_Init(CParticle *self) {
	CParticleActive *a = (CParticleActive *)self;

	Particle_InitCommon(self);
	a->vel = (vec3_t){ 0.0f, 0.0f, 0.0f };
	a->acc = (vec3_t){ 0.0f, 0.0f, 0.0f };
	a->sparks = 0;
	a->manual = 0;
	a->t = 0.0f;
	a->life = 1.0f;
	a->delay = 0.0f;
	a->sizeIni = 1.0f;
	a->sizeEnd = 1.0f;
	a->sizeLife = 1.0f;
	self->size = 1.0f;
	a->colorIni = PARTICLESYSTEM_DEFAULT_COLOR;
	a->colorEnd = PARTICLESYSTEM_DEFAULT_COLOR;
	self->color = PARTICLESYSTEM_DEFAULT_COLOR;
	a->rotIni = 0.0f;
	a->rotEnd = 0.0f;
	self->rot = 0.0f;
	self->uv1[1] = 1.0f;
	a->spriteIni = 0xff;
	a->spriteEnd = 0xff;
	a->spriteCur = 0xff;
	a->colorRamp = NULL;
	a->sparkVelScale = 1.0f;
	a->sparkMaxSize = 3.4028234663852886e+38f;
	a->bounce = 0;
}

void CParticleStatic_Init(CParticleStatic *self) {
	Particle_InitCommon(self);
	self->size = 1.0f;
	self->color = PARTICLESYSTEM_DEFAULT_COLOR;
	self->rot = 0.0f;
	self->uv1[1] = 1.0f;
}

/* the members both constructors clear before CParticleSystem_Init */
static void ParticleSystem_Clear(CParticleSystem *self) {
	self->class = 0;
	self->node = NULL;
	self->vftable = CSISTPART_VFTABLE;
	cString_ctor(&self->params.texture);
	memset(&self->vis, 0, sizeof(self->vis));
}

CParticleSystem *CParticleSystem_ctorCopy(CParticleSystem *self, const CParticleSystem *orig) {
	ParticleSystem_Clear(self);
	CParticleSystem_Init(self, orig->node, orig->type, &orig->params, NULL);
	self->enabled = orig->enabled;
	self->visible = orig->visible;
	self->scale = orig->scale;
	self->alpha = orig->alpha;
	self->mat = orig->mat;
	if (orig->posList) {
		self->posList = New_Array((uint32_t)self->params.nParticles, sizeof(vec3_t));
		memcpy(self->posList, orig->posList, (uint32_t)self->params.nParticles * sizeof(vec3_t));
	}
	CParticleSystem_CreateMat(self);
	CParticleSystem_CreateParticles(self);
	return self;
}

CParticleSystem *CParticleSystem_ctorParams(CParticleSystem *self, CNode3D *node, const char *type, float scale, float alpha,
	const vec3_t *posList, CIniFile *ini) {
	ParticleSystem_Clear(self);
	CParticleSystem_Init(self, node, type, NULL, ini);
	self->scale = scale;
	self->alpha = alpha;
	if (posList) {
		self->posList = New_Array((uint32_t)self->params.nParticles, sizeof(vec3_t));
		memcpy(self->posList, posList, (uint32_t)self->params.nParticles * sizeof(vec3_t));
	}
	CParticleSystem_CreateMat(self);
	CParticleSystem_CreateParticles(self);
	return self;
}

void CObjBase3D_dtor(CObjBase3D *self) {
	self->base.vftable = COBJBASE3D_VFTABLE;
	CObjBase3D_SetNode(self, NULL);
	self->base.vftable = IOBJECT3D_VFTABLE;
}

void ParticleSystemParams_dtor(ParticleSystemParams *self) {
	cString_dtor(&self->texture);
}

void CParticleSystem_dtor(CParticleSystem *self) {
	self->vftable = CSISTPART_VFTABLE;
	for (int i = 0; i < self->params.nParticles; i++) {
		if (self->Particles[i]) {
			operator_delete_sized(self->Particles[i], sizeof(CParticle));
		}
	}
	if (self->Particles) {
		operator_delete_array(self->Particles);
		self->Particles = NULL;
	}
	CHECK(self->type);
	if (self->type) {
		operator_delete_sized(self->type, 1);
		self->type = NULL;
	}
	g_ParticleSystemCount -= self->params.nParticles;
	if (self->posList) {
		operator_delete_sized(self->posList, sizeof(vec3_t));
		self->posList = NULL;
	}
	ParticleSystemParams_dtor(&self->params);
	CObjBase3D_dtor((CObjBase3D *)self);
}

ParticleSystemParams *ParticleSystemParams_Assign(ParticleSystemParams *self, const ParticleSystemParams *src) {
	/* the members one by one: the padding bytes stay */
	memcpy(self, src, offsetof(ParticleSystemParams, spriteRand) + 1);
	cString_Assign(&self->texture, &src->texture);
	self->additiveAlpha = src->additiveAlpha;
	self->alphaMod = src->alphaMod;
	self->sparkVelScale = src->sparkVelScale;
	self->sparkMaxSize = src->sparkMaxSize;
	self->alphaDist = src->alphaDist;
	self->bounce = src->bounce;
	self->zBias = src->zBias;
	return self;
}

IObject3D *CObjBase3D_Delete(IObject3D *base, byte flags) {
	CObjBase3D *self = (CObjBase3D *)base;
	CObjBase3D_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return base;
}

IObject3D *CParticleSystem_Delete(IObject3D *base, byte flags) {
	CParticleSystem *self = (CParticleSystem *)base;
	CParticleSystem_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CParticleSystem));
	}
	return base;
}

CIniFile g_ParticleSystemIni;

void CParticleSystem_Init(CParticleSystem *self, CNode3D *node, const char *type, const ParticleSystemParams *params,
	CIniFile *ini) {
	CHECK(node);
	CObjBase3D_SetNode((CObjBase3D *)self, node);
	self->lastPos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->Particles = NULL;
	self->type = NULL;
	self->class = CLASS_OBJ3D_PARTICLESYSTEM;
	self->lastTime = 0;
	self->birthTime = 0;
	self->lastFrame = 0;
	self->enabled = 1;
	self->visible = 1;
	self->inView = 0;
	self->mat = NULL;
	self->MultiSpr = NULL;
	self->scale = 1.0f;
	self->alpha = 1.0f;
	self->posList = NULL;
	self->colorRamp = NULL;
	self->emitPos = NULL;
	self->nEmitPos = 0;
	self->emitIdx = 0;
	self->numActive = 0;
	self->nextFree = 0;
	uint32_t len = (uint32_t)strlen(type) + 1;
	self->type = operator_new_array(len);
	CHECK(self->type);
	String_Copy(self->type, type ? type : "Default", (int)len);
	if (params) {
		ParticleSystemParams_Assign(&self->params, params);
	} else if (ini) {
		CParticleSystem_ReadParams(self, ini);
	} else {
		CParticleSystem_ReadParams(self, (&g_ParticleSystemIni));
	}
}

/* p made active (counted) */
static void ParticleSystem_Activate(CParticleSystem *self, CParticle *p) {
	if (p->active != 1) {
		p->active = 1;
		self->numActive++;
	}
}

CParticle *CParticleSystem_GetFree(CParticleSystem *self, byte force) {
	float best = -3.4028234663852886e+38f;
	CParticle *oldest = NULL;

	if (!self->enabled) {
		return NULL;
	}
	int n = self->params.nParticles;
	int i = self->nextFree;
	if (n <= i) {
		i = 0;
	}
	for (int k = 0; k < n; k++) {
		CParticle *p = self->Particles[i];
		if (!p->active) {
			ParticleSystem_Activate(self, p);
			self->nextFree = (byte)k;
			return p;
		}
		/* a static particle has no t: the float after it is read, as in the exe */
		float t = ((CParticleActive *)p)->t;
		if (force && (PARTICLE_TYPE(p) == PARTICLE_STATIC || best <= t)) {
			best = t;
			oldest = p;
		}
		i = i + 1 < n ? i + 1 : i + 1 - n;
	}
	if (force && oldest) {
		ParticleSystem_Activate(self, oldest);
		self->nextFree = 0;
		return oldest;
	}
	return NULL;
}

CParticleSystem *ParticleSystem_FromNode(CNode3D *node, CIniFile *ini) {
	vec3_t pos[0x100];
	char key[0x20], type[0x20];

	CIniFile *prop = node->ini;
	CHECK(prop);
	float scale = CIniFile_GetFloat(prop, "Particles", "Scale", 1.0f);
	float alpha = CIniFile_GetFloat(prop, "Particles", "Alpha", 1.0f);
	byte posList = CIniFile_GetBool(prop, "Particles", "PosList", 0);
	if (posList) {
		for (int i = 1; i <= 0x100; i++) {
			vec3_t def = { 0.0f, 0.0f, 0.0f }, v;
			Bprintf(key, sizeof(key), "Pos_%d", i);
			pos[i - 1] = *CIniFile_GetVec3(prop, &v, "Particles", key, &def);
		}
	}
	if (!CIniFile_GetText(prop, "Particles", "Type", type, sizeof(type), NULL)) {
		return NULL;
	}
	void *p = operator_new(sizeof(CParticleSystem));
	if (!p) {
		return NULL;
	}
	if (!ini || CIniFile_Count(ini) < 1) {
		ini = NULL;
	}
	return CParticleSystem_ctorParams(p, node, type, scale, alpha, posList ? pos : NULL, ini);
}

void CParticleSystem_SetColorRamp(CParticleSystem *self, const uint32_t *ramp) {
	self->colorRamp = ramp;
}

void CParticleSystem_SetEmitPos(CParticleSystem *self, vec3_t *pos, byte n) {
	self->emitPos = pos;
	if (!pos) {
		self->nEmitPos = 0;
		self->emitIdx = 0;
		return;
	}
	self->nEmitPos = n;
	if (n <= self->emitIdx) {
		self->emitIdx = 0;
	}
	if (!n) {
		self->emitPos = NULL;
	}
}

void CParticleSystem_GetScale2(CParticleSystem *self, float *out) {
	if (self->node) {
		out[0] = self->node->scale.x;
		out[1] = self->node->scale.y;
		return;
	}
	out[0] = 1.0f;
	out[1] = 1.0f;
}

void CParticleSystem_GetScale(CParticleSystem *self, vec3_t *out) {
	if (self->node) {
		*out = self->node->scale;
		return;
	}
	*out = (vec3_t){ 1.0f, 1.0f, 1.0f };
}

/* the corner c into box, as the exe compares */
static void ParticleSystem_BoxAdd(box_t *box, float x, float y, float z) {
	box->min.x = x <= box->min.x ? x : box->min.x;
	box->min.y = y <= box->min.y ? y : box->min.y;
	box->min.z = z <= box->min.z ? z : box->min.z;
	box->max.x = box->max.x <= x ? x : box->max.x;
	box->max.y = box->max.y <= y ? y : box->max.y;
	box->max.z = box->max.z <= z ? z : box->max.z;
}

byte CParticleSystem_GetBox(CParticleSystem *self, box_t *box) {
	if (!(self->params.flags & PARTICLESYSTEM_STATIC) || !self->params.birthRate) {
		return 0;
	}
	box->min = (vec3_t){ 3.4028234663852886e+38f, 3.4028234663852886e+38f, 3.4028234663852886e+38f };
	box->max = (vec3_t){ -3.4028234663852886e+38f, -3.4028234663852886e+38f, -3.4028234663852886e+38f };
	if (!(self->params.flags & PARTICLESYSTEM_LOCAL)) {
		E3D_LogInfo((&g_FileLog), "INFO: Non-local static particle system: BBox not calculated: %s", self->type);
		return 0;
	}
	for (int i = 0; i < self->params.nParticles; i++) {
		CParticle *p = self->Particles[i];
		if (!p->active) {
			continue;
		}
		float r = p->size * p->scale * 0.5f;
		ParticleSystem_BoxAdd(box, p->pos.x - r, p->pos.y - r, p->pos.z - r);
		ParticleSystem_BoxAdd(box, p->pos.x + r, p->pos.y + r, p->pos.z + r);
	}
	CHECK(!(box->max.x <= box->min.x) && !(box->max.y <= box->min.y) && !(box->max.z <= box->min.z));
	return 1;
}

void CParticleSystem_ReadParams(CParticleSystem *self, CIniFile *ini) {
	ParticleSystemParams *pa = &self->params;
	const char *sect;
	char texture[0x100], key[0x20];
	vec3_t zero, v;

	E3D_LogWhite((&g_FileLog), "Particle system: %s [%s]", self->type, self->node->name);
	if (!CIniFile_FindSection(ini, self->type, 1)) {
		Con_Warnf("ERROR: Particle system data not found: %s\n", self->type);
		self->enabled = 0;
	}
	sect = self->type;
	pa->flags = 0;
	if (CIniFile_GetBool(ini, sect, "Local", 0)) {
		pa->flags |= PARTICLESYSTEM_LOCAL;
	}
	if (CIniFile_GetBool(ini, sect, "LocalPos", 0)) {
		pa->flags |= PARTICLESYSTEM_LOCALPOS;
	}
	if (CIniFile_GetBool(ini, sect, "Static", 0)) {
		pa->flags |= PARTICLESYSTEM_STATIC;
	}
	if (CIniFile_GetBool(ini, sect, "Sparks", 0)) {
		pa->flags |= PARTICLESYSTEM_SPARKS;
	}
	if (CIniFile_GetBool(ini, sect, "MultiSprite", 0)) {
		pa->flags |= PARTICLESYSTEM_MULTISPRITE;
	}
	if (CIniFile_GetBool(ini, sect, "ColorRamp", 0)) {
		pa->flags |= PARTICLESYSTEM_COLORRAMP;
	}
	if (CIniFile_GetBool(ini, sect, "Mipmap", 1)) {
		pa->flags |= PARTICLESYSTEM_MIPMAP;
	}
	pa->nParticles = CIniFile_GetInt(ini, sect, "Particles", 0x20);
	pa->birthRate = CIniFile_GetInt(ini, sect, "BirthRate", 10);
	pa->lifeTime = CIniFile_GetFloat(ini, sect, "LifeTime", 1.0f);
	pa->scale = CIniFile_GetFloat(ini, sect, "Scale", 1.0f);
	pa->sizeIni = CIniFile_GetFloat(ini, sect, "SizeIni", 1.0f);
	pa->sizeEnd = CIniFile_GetFloat(ini, sect, "SizeEnd", 1.0f);
	pa->sizeVar = CIniFile_GetFloat(ini, sect, "SizeVar", 0.0f);
	pa->sizeLife = CIniFile_GetFloat(ini, sect, "SizeLife", 1.0f);
	pa->colorIni = CIniFile_GetColor(ini, sect, "ColorIni", PARTICLESYSTEM_DEFAULT_COLOR);
	pa->colorEnd = CIniFile_GetColor(ini, sect, "ColorEnd", PARTICLESYSTEM_DEFAULT_COLOR);
	zero = (vec3_t){ 0.0f, 0.0f, 0.0f };
	pa->initPos = *CIniFile_GetVec3(ini, &v, sect, "InitPos", &zero);
	pa->initPosMin = *CIniFile_GetVec3(ini, &v, sect, "InitPosMin", &pa->initPos);
	pa->initPosMax = *CIniFile_GetVec3(ini, &v, sect, "InitPosMax", &pa->initPos);
	zero = (vec3_t){ 0.0f, 0.0f, 0.0f };
	pa->initVel = *CIniFile_GetVec3(ini, &v, sect, "InitVel", &zero);
	pa->initVelMin = *CIniFile_GetVec3(ini, &v, sect, "InitVelMin", &pa->initVel);
	pa->initVelMax = *CIniFile_GetVec3(ini, &v, sect, "InitVelMax", &pa->initVel);
	zero = (vec3_t){ 0.0f, 0.0f, 0.0f };
	pa->initAcc = *CIniFile_GetVec3(ini, &v, sect, "InitAcc", &zero);
	pa->initAccMin = *CIniFile_GetVec3(ini, &v, sect, "InitAccMin", &pa->initAcc);
	pa->initAccMax = *CIniFile_GetVec3(ini, &v, sect, "InitAccMax", &pa->initAcc);
	pa->rotIni = (float)((double)CIniFile_GetFloat(ini, sect, "RotIni", 0.0f) * 3.141592653589793 / 180.0);
	pa->rotEnd = (float)((double)CIniFile_GetFloat(ini, sect, "RotEnd", 0.0f) * 3.141592653589793 / 180.0);
	pa->rotVar = (float)((double)CIniFile_GetFloat(ini, sect, "RotVar", 0.0f) * 3.141592653589793 / 180.0);
	CIniFile_GetText(ini, sect, "Texture", texture, sizeof(texture), "bmp/partic1.bmp");
	cString_Set(&pa->texture, texture);
	pa->spriteIni = (byte)CIniFile_GetInt(ini, sect, "SpriteIni", 0xff);
	pa->spriteEnd = (byte)CIniFile_GetInt(ini, sect, "SpriteEnd", 0xff);
	pa->spriteRand = CIniFile_GetBool(ini, sect, "SpriteRand", 0);
	for (int i = 0; i < 9; i++) {
		Bprintf(key, sizeof(key), "ColorRamp_%02d", i + 1);
		pa->colorRamp[i] = CIniFile_GetColor(ini, sect, key, i < 1 ? PARTICLESYSTEM_DEFAULT_COLOR : pa->colorRamp[i - 1]);
	}
	pa->additiveAlpha = CIniFile_GetBool(ini, sect, "AdditiveAlpha", 1);
	pa->alphaMod = CIniFile_GetBool(ini, sect, "AlphaMod", 1);
	pa->sparkVelScale = CIniFile_GetFloat(ini, sect, "SparkVelScale", 1.0f);
	pa->sparkMaxSize = CIniFile_GetFloat(ini, sect, "SparkMaxSize", 3.4028234663852886e+38f);
	pa->alphaDist = CIniFile_GetFloat(ini, sect, "AlphaDist", 0.0f);
	if (pa->alphaDist > 0.0f && !pa->additiveAlpha) {
		pa->alphaDist = 0.0f;
		Con_Warnf("WARN: Only Additive Alpha Particle Systems can use Alpha Distance parameter\n");
	}
	pa->bounce = CIniFile_GetBool(ini, sect, "Bounce", 0);
	if (pa->bounce) {
		pa->bounce = 0;
		Con_Warnf("WARN: Only World Particle Systems can use Bounce parameter\n");
	}
	pa->zBias = CIniFile_GetFloat(ini, sect, "ZBias", 0.0f);
}

/* v times k */
static vec3_t ParticleSystem_Scaled(const vec3_t *v, float k) {
	return (vec3_t){ v->x * k, v->y * k, v->z * k };
}

void CParticleSystem_GenParticle(CParticleSystem *self, CParticle *p) {
	ParticleSystemParams *pa = &self->params;
	vec3_t v;

	CHECK(p);
	ParticleSystem_Activate(self, p);
	float k = self->scale * pa->scale * g_ParticleSystemPosScale;
	if (!self->emitPos) {
		p->pos = ParticleSystem_Scaled(ParticleSystem_RandVec(&v, &pa->initPosMin, &pa->initPosMax), k);
	} else {
		byte i = self->emitIdx;
		p->pos = self->emitPos[i];
		uint32_t next = (uint32_t)self->emitIdx + 1;
		self->emitIdx = next < self->nEmitPos ? (byte)next : 0;
	}
	p->scale = self->scale * pa->scale * g_ParticleSystemSizeScale;
	p->size = pa->sizeIni;
	p->color = pa->colorIni;
	p->rot = pa->rotIni;
	p->uv0[0] = 0.0f;
	p->uv0[1] = 0.0f;
	p->uv1[0] = 1.0f;
	p->uv1[1] = 1.0f;
	byte sprite = pa->spriteIni;
	if (pa->spriteRand) {
		if (pa->spriteIni < pa->spriteEnd) {
			byte lo = pa->spriteIni;
			sprite = (byte)(crt_rand() % (int)((uint32_t)pa->spriteEnd - lo + 1)) + lo;
		} else if (pa->spriteEnd < pa->spriteIni) {
			byte lo = pa->spriteEnd;
			sprite = (byte)(crt_rand() % (int)((uint32_t)pa->spriteIni - lo + 1)) + lo;
		}
	}
	if (PARTICLE_TYPE(p) == PARTICLE_STATIC && self->MultiSpr) {
		CHECK((int)sprite < ((CMultiSprite *)self->MultiSpr)->n);
		CMultiSprite_GetUV(self->MultiSpr, sprite, p->uv0, p->uv1);
	}
	if (PARTICLE_TYPE(p) != PARTICLE_ACTIVE) {
		return;
	}
	CParticleActive *a = (CParticleActive *)p;
	a->manual = 0;
	a->t = 0.0f;
	a->life = pa->lifeTime * g_ParticleSystemLifeScale;
	a->delay = 0.0f;
	a->sparks = (pa->flags & PARTICLESYSTEM_SPARKS) != 0;
	a->vel = ParticleSystem_Scaled(ParticleSystem_RandVec(&v, &pa->initVelMin, &pa->initVelMax), k);
	a->acc = ParticleSystem_Scaled(ParticleSystem_RandVec(&v, &pa->initAccMin, &pa->initAccMax), k);
	float f = 1.0f;
	if (pa->sizeVar != 0.0f) {
		f = ParticleSystem_RandLerp(0.0f, pa->sizeVar) + 1.0f;
	}
	a->sizeIni = f * pa->sizeIni;
	f = 1.0f;
	if (pa->sizeVar != 0.0f) {
		f = ParticleSystem_RandLerp(0.0f, pa->sizeVar) + 1.0f;
	}
	a->sizeEnd = pa->sizeEnd * f;
	a->sizeLife = 1.0f / pa->sizeLife;
	a->colorIni = pa->colorIni;
	a->colorEnd = pa->colorEnd;
	f = 0.0f;
	if (pa->rotVar != 0.0f) {
		f = ParticleSystem_RandLerp(-pa->rotVar, pa->rotVar);
	}
	a->rotIni = pa->rotIni + f;
	f = 0.0f;
	if (pa->rotVar != 0.0f) {
		f = ParticleSystem_RandLerp(-pa->rotVar, pa->rotVar);
	}
	a->spriteCur = 0xff;
	a->rotEnd = pa->rotEnd + f;
	if (!pa->spriteRand) {
		a->spriteIni = pa->spriteIni;
		a->spriteEnd = pa->spriteEnd;
	} else {
		a->spriteIni = sprite;
		a->spriteEnd = sprite;
	}
	if (pa->flags & PARTICLESYSTEM_COLORRAMP) {
		a->colorRamp = self->colorRamp ? self->colorRamp : pa->colorRamp;
	} else {
		a->colorRamp = NULL;
	}
	a->sparkVelScale = pa->sparkVelScale;
	a->sparkMaxSize = pa->sparkMaxSize;
	a->bounce = pa->bounce;
}

void CParticleSystem_PlaceParticle(CParticleSystem *self, CParticle *p, const matrix_t *m) {
	if (PARTICLE_TYPE(p) == PARTICLE_ACTIVE && !(self->params.flags & PARTICLESYSTEM_LOCAL)) {
		CParticleActive *a = (CParticleActive *)p;
		Vec3_TransformDir0(&p->pos, &p->pos, m);
		Vec3_TransformDir0(&a->vel, &a->vel, m);
		Vec3_TransformDir0(&a->acc, &a->acc, m);
	}
	if (!(self->params.flags & (PARTICLESYSTEM_LOCAL | PARTICLESYSTEM_LOCALPOS))) {
		p->pos.x = p->pos.x + m->m[12];
		p->pos.y = m->m[13] + p->pos.y;
		p->pos.z = m->m[14] + p->pos.z;
	}
}

void CParticleSystem_Emit(CParticleSystem *self, int n, byte force, float dt, const matrix_t *m) {
	for (int i = 0; i < n;) {
		CParticle *p = CParticleSystem_GetFree(self, force);
		if (!p) {
			return;
		}
		CParticleSystem_GenParticle(self, p);
		CParticleSystem_PlaceParticle(self, p, m);
		PARTICLE_UPDATE(p, self, 0.0f, self->MultiSpr);
		i++;
		((CParticleActive *)p)->delay = (float)i * dt;
	}
}

void CParticleSystem_EmitN(CParticleSystem *self, int n, byte force) {
	CParticleSystem_Emit(self, n, force, 0.0f, (&g_MatIdentity));
}

void CParticleSystem_EmitM(CParticleSystem *self, int n, byte force, const matrix_t *m) {
	CParticleSystem_Emit(self, n, force, 0.0f, m);
}

void CParticleSystem_EmitDt(CParticleSystem *self, int n, byte force, float dt) {
	CParticleSystem_Emit(self, n, force, dt, (&g_MatIdentity));
}

byte CParticleSystem_Read(IObject3D *base, CFileIO *f, MeshD3D_FileCtx *ctx) {
	CParticleSystem *self = (CParticleSystem *)base;
	(void)self;
	(void)f;
	CHECK(CIniFile_FindSection(ctx->node->ini, "Particles", 1));
	return 1;
}

byte CParticleSystem_Write(IObject3D *base, CFileIO *f, MeshD3D_FileCtx *ctx) {
	CParticleSystem *self = (CParticleSystem *)base;
	(void)self;
	(void)f;
	CHECK(CIniFile_FindSection(ctx->node->ini, "Particles", 1));
	CHECK(!ctx->node->scene->mainLOD);
	return 1;
}

void CParticleSystem_CreateMat(CParticleSystem *self) {
	ParticleSystemParams *pa = &self->params;

	if (!self->mat) {
		CMaterial m;
		CMaterial_ctor(&m);
		String_Set(&m.Name, "MatSistPart");
		CMaterial_SetMap(&m, 0, pa->texture);
		if (m.map[0]) {
			m.map[0]->filter = (pa->flags & PARTICLESYSTEM_MIPMAP) != 0;
			m.map[0]->tile = 0;
		}
		if (!pa->additiveAlpha) {
			m.params.srcBlend = 2;
			m.params.dstBlend = 6;
		} else {
			m.params.srcBlend = 5;
			m.params.dstBlend = 2;
		}
		if (!pa->alphaMod) {
			m.params.attrib |= 0x400;
		}
		m.params.transp = 1;
		m.params.zWrite = 0;
		if (strstr(pa->texture, ".DUDV")) {
			m.params.srcBlend = 2;
			m.params.dstBlend = 2;
			m.params.transp = 1;
			m.params.zWrite = 0;
		}
		m.params.lighting = 0;
		self->mat = Mat_New(&m, 1);
		CMaterial_dtor(&m);
	}
	if (pa->flags & PARTICLESYSTEM_MULTISPRITE) {
		self->MultiSpr = CMaterial_GetMultiSprite(self->mat, 0);
		CHECK(!self->MultiSpr || ((CMultiSprite *)self->MultiSpr)->n > 1);
		if (pa->spriteIni == 0xff) {
			pa->spriteIni = 1;
		}
		if (pa->spriteEnd == 0xff) {
			pa->spriteEnd = (byte)(((CMultiSprite *)self->MultiSpr)->n - 1);
		}
	}
}

void CParticleSystem_SetActive(CParticleSystem *self, CParticle *p, byte active) {
	CHECK(p);
	if (p->active != active) {
		p->active = active;
		if (active) {
			self->numActive++;
		} else {
			self->numActive--;
		}
	}
}

void CParticleSystem_CreateParticles(CParticleSystem *self) {
	ParticleSystemParams *pa = &self->params;

	CHECK(!self->Particles);
	self->Particles = operator_new_array((uint32_t)pa->nParticles * sizeof(*self->Particles));
	for (int i = 0; i < pa->nParticles; i++) {
		CParticle *p;
		if (!(pa->flags & PARTICLESYSTEM_STATIC)) {
			p = operator_new(sizeof(CParticleActive));
			if (p) {
				p = CParticleActive_ctor(p);
			}
		} else {
			p = operator_new(sizeof(CParticle));
			if (p) {
				p = CParticleStatic_ctor(p);
			}
		}
		self->Particles[i] = p;
	}
	g_ParticleSystemCount += pa->nParticles;
	if (!(pa->flags & PARTICLESYSTEM_STATIC) || !pa->birthRate) {
		return;
	}
	/* a static system: each particle where a moving one would be at its part of the life time */
	CParticleActive tmp;
	tmp.p.vftable = CPARTICLEACTIVE_VFTABLE;
	CParticleActive_Init(&tmp.p);
	for (int i = 0; i < pa->nParticles; i++) {
		CParticleSystem_GenParticle(self, &tmp.p);
		CParticleActive_Update(&tmp.p, self, ((float)i / (float)(double)(uint32_t)pa->nParticles) * pa->lifeTime,
			self->MultiSpr);
		CParticle *p = self->Particles[i];
		CParticleSystem_GenParticle(self, p);
		p->active = tmp.p.active;
		memcpy(&p->pos, &tmp.p.pos, sizeof(CParticle) - offsetof(CParticle, pos));
		if (self->posList) {
			p->pos = self->posList[i];
		}
		CHECK(p->active);
	}
}

vec3_t *ParticleSystem_RandVec(vec3_t *out, const vec3_t *a, const vec3_t *b) {
	float x = a->x, y, z;
	if (x != b->x) {
		x = ParticleSystem_RandLerp(x, b->x);
	}
	y = a->y;
	if (y != b->y) {
		y = ParticleSystem_RandLerp(y, b->y);
	}
	z = a->z;
	if (z != b->z) {
		z = ParticleSystem_RandLerp(z, b->z);
	}
	out->x = x;
	out->y = y;
	out->z = z;
	return out;
}

void ParticleSystem_LoadIni(const char *name, const char *dir) {
	if (!name) {
		CIniFile_Clear((&g_ParticleSystemIni));
		return;
	}
	CIniFile_Load((&g_ParticleSystemIni), name, dir, 1);
}

void CParticleSystem_Render(IObject3D *base, void *dev, CRenderNode *r) {
	CParticleSystem *self = (CParticleSystem *)base;
	(void)dev;
	if (!g_ParticleSystemOn) {
		return;
	}
	self->inView = 1;
	if (!self->visible || !self->numActive) {
		return;
	}
	if (!r && g_ParticleSystemDebug) {
		const matrix_t *m = &self->node->m;
		vec3_t pos = { m->m[12], m->m[13], m->m[14] };
		box_t local = { { -100.0f, -100.0f, -100.0f }, { 100.0f, 100.0f, 100.0f } }, w;
		Box_Transform(&w, &local, m);
		float dx = w.max.x - w.min.x, dy = w.max.y - w.min.y, dz = w.max.z - w.min.z;
		vec3_t center = { (w.max.x + w.min.x) * 0.5f, (w.min.y + w.max.y) * 0.5f, (w.min.z + w.max.z) * 0.5f };
		float radius = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) * 0.5f;
		byte vis = SectorVis_Sphere(&self->vis, &pos, &center, radius, self->node->name);
		self->inView = vis;
		if (g_ParticleSystemDebug > 1) {
			SectorVis_Draw(&self->vis, &pos, vis, 0, self->node->name);
		}
		if (g_ParticleSystemDebug > 2) {
			Debug_Text3D(g_FntTerm8, 4.0f, -12.0f, &pos, 0x8000, "%s", self->type);
		}
	}
	if (self->inView) {
		ParticleSystem_Draw(self, r);
	}
}

void CParticleSystem_Render2(CParticleSystem *self, void *dev) {
	if (g_ParticleSystemOn && self->visible && self->numActive) {
		ParticleSystem_Draw2(self, dev);
	}
}

void CParticleSystem_SetAlpha(CParticleSystem *self, float alpha) {
	self->alpha = alpha;
}

/* a bounce on the floor of the world: the velocity mirrored at the hit triangle, halved */
static void Particle_Bounce(CParticleActive *a, const vec3_t *prev, float dt) {
	vec3_t hit;
	CGroundHit info = { 0, 0, NULL };

	CHECK(g_World);
	CGround *ground = g_World->ObjGround;
	if (!ground || !CGround_TraceSeg(ground, prev, &a->p.pos, &hit, &info, 0)) {
		return;
	}
	a->vel.x = -a->vel.x * 0.5f;
	a->vel.y = -a->vel.y * 0.5f;
	a->vel.z = -a->vel.z * 0.5f;
	const float *n = &info.quad->Mesh->pTri[info.quad->pTri[info.tri]].plane.n.x;
	float d = n[1] * a->vel.y + a->vel.x * n[0] + n[2] * a->vel.z;
	d = d + d;
	a->vel.x = n[0] * d - a->vel.x;
	a->vel.y = n[1] * d - a->vel.y;
	a->vel.z = n[2] * d - a->vel.z;
	a->p.pos.x = a->acc.x * dt * dt * 0.5f + hit.x + a->vel.x * dt;
	a->p.pos.y = a->acc.y * dt * dt * 0.5f + hit.y + a->vel.y * dt;
	a->p.pos.z = a->acc.z * dt * dt * 0.5f + hit.z + a->vel.z * dt;
}

/* the byte k of two colors mixed with the weight w of 256 */
static byte Color_Mix(uint32_t c0, uint32_t c1, int k, uint32_t w) {
	return (byte)((((c1 >> (8 * k)) & 0xff) * w + ((c0 >> (8 * k)) & 0xff) * (0x100 - w)) >> 8);
}

void CParticleActive_Update(CParticle *base, struct CParticleSystem *sp, float dt, void *multiSpr) {
	CParticleActive *self = (CParticleActive *)base;
	if (self->delay > 0.0f) {
		float d = self->delay - dt;
		self->delay = d;
		if (d > 0.0f) {
			return;
		}
		dt = -d;
	}
	float t = dt / self->life + self->t;
	self->t = t;
	if (t > 1.0f) {
		CHECK(sp);
		if (self->p.active) {
			self->p.active = 0;
			sp->numActive--;
		}
		return;
	}
	vec3_t prev;
	if (self->bounce) {
		prev = self->p.pos;
	}
	vec3_t *pos = &self->p.pos;
	pos->x = self->acc.x * dt * dt * 0.5f + dt * self->vel.x + pos->x;
	pos->z = self->acc.z * dt * dt * 0.5f + self->vel.z * dt + pos->z;
	pos->y = self->acc.y * dt * dt * 0.5f + self->vel.y * dt + pos->y;
	self->vel.x = self->vel.x + self->acc.x * dt;
	self->vel.y = self->acc.y * dt + self->vel.y;
	self->vel.z = self->acc.z * dt + self->vel.z;
	if (self->bounce) {
		Particle_Bounce(self, &prev, dt);
	}
	t = self->t;
	if (self->sizeLife > 1.0f) {
		float f = t * self->sizeLife;
		f = f < 1.0f ? f : 1.0f;
		float g = 1.0f - f;
		float k = g * g * (2.0f - g);
		self->p.size = (1.0f - k) * (self->sizeEnd - self->sizeIni) + self->sizeIni;
	} else {
		self->p.size = (self->sizeEnd - self->sizeIni) * t + self->sizeIni;
	}
	uint32_t u = (uint32_t)llrint((double)(t * 16777216.0f));
	u = u < 0xffffff ? u : 0xffffff;
	uint32_t c0, c1, w;
	if (!self->colorRamp) {
		w = (u >> 16) & 0xff;
		c0 = self->colorIni;
		c1 = self->colorEnd;
	} else {
		uint32_t i = (u >> 21) & 7;
		w = (u >> 13) & 0xff;
		c0 = self->colorRamp[i];
		c1 = self->colorRamp[i + 1];
	}
	uint32_t color = 0;
	for (int k = 0; k < 4; k++) {
		color |= (uint32_t)Color_Mix(c0, c1, k, w) << (8 * k);
	}
	self->p.color = color;
	t = self->t;
	self->p.rot = (self->rotEnd - self->rotIni) * t + self->rotIni;
	if (!multiSpr) {
		return;
	}
	int ini = self->spriteIni, cur = ini;
	if (self->spriteIni < self->spriteEnd) {
		cur = (int)llrint((double)((float)(self->spriteEnd - self->spriteIni) * t)) + self->spriteIni;
	} else if (self->spriteEnd < self->spriteIni) {
		cur = self->spriteIni - (int)llrint((double)((float)(self->spriteIni - self->spriteEnd) * t));
	}
	int s = ini;
	if (cur > ini) {
		s = cur >= self->spriteEnd ? self->spriteEnd : cur;
	}
	if (s != self->spriteCur) {
		self->spriteCur = (byte)s;
		CMultiSprite_GetUV(multiSpr, s, self->p.uv0, self->p.uv1);
	}
}

void CParticleSystem_Update(IObject3D *base, CRenderNode *r) {
	CParticleSystem *self = (CParticleSystem *)base;
	if (!g_ParticleSystemOn) {
		return;
	}
	if (g_ParticleSystemFlags != -1) {
		self->params.flags = (uint32_t)g_ParticleSystemFlags;
	}
	if (self->lastFrame == g_FrameCount) {
		return;
	}
	const matrix_t *m;
	if (!self->node && !r) {
		CHECK(self->node || r);
	}
	m = r ? &r->m : &self->node->m;
	uint32_t now = g_TimeMs;
	if ((self->params.flags & PARTICLESYSTEM_STATIC) || !self->inView) {
		return;
	}
	uint32_t last = self->lastTime;
	float dt = last ? ParticleSystem_Seconds(now - last) : 0.0f;
	if (last + 100u < now) {
		dt = 0.1f;
		self->birthTime = now - 100;
	}
	self->lastTime = now;
	self->lastFrame = g_FrameCount;
	if (self->numActive) {
		for (int i = 0; i < self->params.nParticles; i++) {
			CParticle *p = self->Particles[i];
			if (p->active && PARTICLE_TYPE(p) == PARTICLE_ACTIVE && !((CParticleActive *)p)->manual) {
				PARTICLE_UPDATE(p, self, dt, self->MultiSpr);
			}
		}
	}
	if (!self->params.birthRate) {
		self->birthTime = self->lastTime;
		return;
	}
	uint32_t step = 1000u / (uint32_t)(self->params.birthRate + g_ParticleSystemBirthAdd);
	while (self->birthTime + step < self->lastTime) {
		self->birthTime += step;
		CParticle *p = CParticleSystem_GetFree(self, 0);
		if (!p) {
			self->birthTime = self->lastTime;
			break;
		}
		CParticleSystem_GenParticle(self, p);
		CParticleSystem_PlaceParticle(self, p, m);
		PARTICLE_UPDATE(p, self, ParticleSystem_Seconds(self->lastTime - self->birthTime), self->MultiSpr);
	}
	self->lastPos = (vec3_t){ m->m[12], m->m[13], m->m[14] };
}

/* the slot Update of a class that does nothing there */
void CParticle_UpdateNop(CParticle *self, struct CParticleSystem *sp, float dt, void *multiSpr) {
	(void)self;
	(void)sp;
	(void)dt;
	(void)multiSpr;
}

/* the vtable of IObject3D */
const IObject3DVtbl IObject3D_vftable = {
	.Delete = IObject3D_Delete,
	.Update = (void *)crt_purecall,
	.Render = (void *)crt_purecall,
	.InitD3D = (void *)crt_purecall,
	.ReleaseD3D = (void *)crt_purecall,
	.Write = IObject3D_WriteFalse,
	.Read = IObject3D_ReadFalse,
};

/* the vtable of CObjBase3D */
const IObject3DVtbl CObjBase3D_vftable = {
	.Delete = CObjBase3D_Delete,
	.Update = IObject3D_UpdateNop,
	.Render = IObject3D_RenderNop,
	.InitD3D = IObject3D_InitD3DNop,
	.ReleaseD3D = IObject3D_ReleaseD3DNop,
	.Write = IObject3D_WriteFalse,
	.Read = IObject3D_ReadFalse,
};

/* the vtable of CParticleSystem */
const IObject3DVtbl CParticleSystem_vftable = {
	.Delete = CParticleSystem_Delete,
	.Update = CParticleSystem_Update,
	.Render = CParticleSystem_Render,
	.InitD3D = IObject3D_InitD3DNop,
	.ReleaseD3D = IObject3D_ReleaseD3DNop,
	.Write = CParticleSystem_Write,
	.Read = CParticleSystem_Read,
};

/* the vtable of CParticleBase */
const CParticleVtbl CParticleBase_vftable = {
	.Type = (void *)crt_purecall,
	.Init = (void *)crt_purecall,
	.Update = (void *)crt_purecall,
};

/* the vtable of CParticleStatic */
const CParticleVtbl CParticleStatic_vftable = {
	.Type = CParticleStatic_Type,
	.Init = CParticleStatic_Init,
	.Update = CParticle_UpdateNop,
};

/* the vtable of CParticleActive */
const CParticleVtbl CParticleActive_vftable = {
	.Type = CParticleActive_Type,
	.Init = CParticleActive_Init,
	.Update = CParticleActive_Update,
};

cGVar GVar_R_Particles;

cGVar GVar_R_ParticleScale;

cGVar GVar_R_ParticleVelScale;

cGVar GVar_R_ParticleFlags;

cGVar GVar_R_ParticlesCount;

cGVar GVar_R_ParticleSector;

cGVar GVar_R_ParticleRate;

cGVar GVar_R_ParticleLife;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_R_Particles, &GVar_R_Particles, "R_Particles", 0, "Transform & Show Particles (0/1)", 1)
INIT_GVAR_FLOAT(Init_GVar_R_ParticleVelScale, &GVar_R_ParticleVelScale, "R_ParticleVelScale", 0, "Particle velocity scale 1", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_R_ParticleLife, &GVar_R_ParticleLife, "R_ParticleLife", 0, "Particle life", 1.0f)
INIT_GVAR_INT(Init_GVar_R_ParticleRate, &GVar_R_ParticleRate, "R_ParticleRate", 0, "Particle rate", 0)
INIT_GVAR_FLOAT(Init_GVar_R_ParticleScale, &GVar_R_ParticleScale, "R_ParticleScale", 0, "Particle scale", 1.0f)
INIT_GVAR_INT(Init_GVar_R_ParticleFlags, &GVar_R_ParticleFlags, "R_ParticleFlags", 0, "Particle flags", -1)
INIT_GVAR_INT(Init_GVar_R_ParticleSector, &GVar_R_ParticleSector, "R_ParticleSector", 0, "Use sector visibility", 1)
INIT_GVAR_INT(Init_GVar_R_ParticlesCount, &GVar_R_ParticlesCount, "R_ParticlesCount", 0, "Particles Count", 0)
FINI_CALL(Fini_CIniFile_ParticleSystemIni, &g_ParticleSystemIni, CIniFile_dtor)
INIT_OBJECT(Init_CIniFile_ParticleSystemIni, Fini_CIniFile_ParticleSystemIni, &g_ParticleSystemIni, CIniFile_ctor)
