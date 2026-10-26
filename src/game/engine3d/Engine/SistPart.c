/*
 * Engine/SistPart.cpp of the engine3d: CSistPart and its particles, see include/scrap/sistpart.h. Also the small
 * functions of Engine/grafico2d.cpp in front of it.
 */
#include <stddef.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/textura.h"
#include "scrap/mundo.h"
#include "scrap/sistpart.h"
#include "scrap/varios.h"

#define SISTPART_DEFAULT_COLOR 0xffffff

/* the triangle a segment hit (CSuelo_TraceSeg): its index into the index list of the mesh */
typedef struct SistPartHit {
	int i0;
	int tri;
	byte *mesh;
} SistPartHit;

typedef int(__thiscall *partType_t)(CParticula *self);
typedef void(__thiscall *partUpdate_t)(CParticula *self, CSistPart *sp, float dt, void *multiSpr);
#define PARTICULA_TYPE(p) (((partType_t)(p)->vftable[PARTICULA_VF_TYPE])(p))
#define PARTICULA_UPDATE(p, sp, dt, ms) (((partUpdate_t)(p)->vftable[PARTICULA_VF_UPDATE])((p), (sp), (dt), (ms)))

/* a random value between a and b, the way the exe computes it */
static float SistPart_RandLerp(float a, float b) {
	int r = crt_rand();
	return (float)((double)(r & 0x7fff) * (1.0 / 32767.0) * (double)(b - a) + (double)a);
}

/* the time of ms milliseconds in seconds */
static float SistPart_Seconds(uint32_t ms) {
	return (float)(double)ms * 0.001f;
}

CParticula *__fastcall CParticulaActive_ctor(CParticula *self) {
	self->vftable = Game_Ptr(CPARTICULAACTIVE_VFTABLE);
	CParticulaActive_Init(self);
	return self;
}

CParticula *__fastcall CParticulaStatic_ctor(CParticula *self) {
	self->vftable = Game_Ptr(CPARTICULASTATIC_VFTABLE);
	CParticulaStatic_Init(self);
	return self;
}

void __fastcall CParticulaActive_dtor(CParticula *self) {
	self->vftable = Game_Ptr(CPARTICULAACTIVE_VFTABLE);
}

int __fastcall CParticulaActive_Type(CParticula *self) {
	(void)self;
	return PARTICULA_ACTIVE;
}

int __fastcall CParticulaStatic_Type(CParticula *self) {
	(void)self;
	return PARTICULA_STATIC;
}

/* the members of both kinds */
static void Particula_InitCommon(CParticula *p) {
	p->pos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	p->uv0[0] = 0.0f;
	p->uv0[1] = 0.0f;
	p->uv1[0] = 1.0f;
	p->active = 0;
	p->escala = 1.0f;
}

void __fastcall CParticulaActive_Init(CParticula *self) {
	CParticulaActive *a = (CParticulaActive *)self;

	Particula_InitCommon(self);
	a->vel = (vec3_t){ 0.0f, 0.0f, 0.0f };
	a->acc = (vec3_t){ 0.0f, 0.0f, 0.0f };
	a->sparks = 0;
	a->b34 = 0;
	a->t = 0.0f;
	a->life = 1.0f;
	a->delay = 0.0f;
	a->sizeIni = 1.0f;
	a->sizeEnd = 1.0f;
	a->sizeLife = 1.0f;
	self->size = 1.0f;
	a->colorIni = SISTPART_DEFAULT_COLOR;
	a->colorEnd = SISTPART_DEFAULT_COLOR;
	self->color = SISTPART_DEFAULT_COLOR;
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

void __fastcall CParticulaStatic_Init(CParticula *self) {
	Particula_InitCommon(self);
	self->size = 1.0f;
	self->color = SISTPART_DEFAULT_COLOR;
	self->rot = 0.0f;
	self->uv1[1] = 1.0f;
}

/* the members both constructors clear before CSistPart_Init */
static void SistPart_Clear(CSistPart *self) {
	self->clase = 0;
	self->nodo = NULL;
	self->vftable = Game_Ptr(CSISTPART_VFTABLE);
	cString_ctor(&self->params.texture);
	memset(&self->vis, 0, sizeof(self->vis));
}

CSistPart *__thiscall CSistPart_ctorCopy(CSistPart *self, const CSistPart *orig) {
	SistPart_Clear(self);
	CSistPart_Init(self, orig->nodo, orig->type, &orig->params, NULL);
	self->enabled = orig->enabled;
	self->visible = orig->visible;
	self->escala = orig->escala;
	self->alpha = orig->alpha;
	self->mat = orig->mat;
	if (orig->posList) {
		self->posList = New_Array((uint32_t)self->params.nParticulas, sizeof(vec3_t));
		memcpy(self->posList, orig->posList, (uint32_t)self->params.nParticulas * sizeof(vec3_t));
	}
	CSistPart_CreateMat(self);
	CSistPart_CreateParticles(self);
	return self;
}

CSistPart *__thiscall CSistPart_ctorParams(CSistPart *self, CNodo3D *nodo, const char *type, float escala, float alpha,
	const vec3_t *posList, CArchINI *ini) {
	SistPart_Clear(self);
	CSistPart_Init(self, nodo, type, NULL, ini);
	self->escala = escala;
	self->alpha = alpha;
	if (posList) {
		self->posList = New_Array((uint32_t)self->params.nParticulas, sizeof(vec3_t));
		memcpy(self->posList, posList, (uint32_t)self->params.nParticulas * sizeof(vec3_t));
	}
	CSistPart_CreateMat(self);
	CSistPart_CreateParticles(self);
	return self;
}

CSistPart *__fastcall CSistPart_ctor(CSistPart *self) {
	SistPart_Clear(self);
	CSistPart_Init(self, NULL, NULL, NULL, NULL);
	CSistPart_CreateMat(self);
	CSistPart_CreateParticles(self);
	return self;
}

void __fastcall CObjBase3D_dtor(IObjeto3D *self) {
	self->vftable = Game_Ptr(COBJBASE3D_VFTABLE);
	CObjBase3D_SetNodo(self, NULL);
	self->vftable = Game_Ptr(IOBJETO3D_VFTABLE);
}

void __fastcall SistPartParams_dtor(SistPartParams *self) {
	cString_dtor(&self->texture);
}

void __fastcall CSistPart_dtor(CSistPart *self) {
	self->vftable = Game_Ptr(CSISTPART_VFTABLE);
	for (int i = 0; i < self->params.nParticulas; i++) {
		if (self->Particles[i]) {
			operator_delete_sized(self->Particles[i], sizeof(CParticula));
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
	g_SistPartCount -= self->params.nParticulas;
	if (self->posList) {
		operator_delete_sized(self->posList, sizeof(vec3_t));
		self->posList = NULL;
	}
	SistPartParams_dtor(&self->params);
	CObjBase3D_dtor((IObjeto3D *)self);
}

SistPartParams *__thiscall SistPartParams_Assign(SistPartParams *self, const SistPartParams *src) {
	/* the members one by one: the padding bytes stay */
	memcpy(self, src, offsetof(SistPartParams, spriteRand) + 1);
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

IObjeto3D *__thiscall CObjBase3D_Delete(IObjeto3D *self, byte flags) {
	CObjBase3D_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, 0xc);
	}
	return self;
}

CSistPart *__thiscall CSistPart_Delete(CSistPart *self, byte flags) {
	CSistPart_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CSistPart));
	}
	return self;
}

void __thiscall CSistPart_Init(CSistPart *self, CNodo3D *nodo, const char *type, const SistPartParams *params,
	CArchINI *ini) {
	CHECK(nodo);
	CObjBase3D_SetNodo((IObjeto3D *)self, nodo);
	self->lastPos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->Particles = NULL;
	self->type = NULL;
	self->clase = CLASE_OBJ3D_SISTPART;
	self->lastTime = 0;
	self->birthTime = 0;
	self->lastFrame = 0;
	self->enabled = 1;
	self->visible = 1;
	self->inView = 0;
	self->mat = NULL;
	self->MultiSpr = NULL;
	self->escala = 1.0f;
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
	Cad_Copy(self->type, type ? type : "Default", (int)len);
	if (params) {
		SistPartParams_Assign(&self->params, params);
	} else if (ini) {
		CSistPart_ReadParams(self, ini);
	} else {
		CSistPart_ReadParams(self, g_SistPartIni);
	}
}

/* p made active (counted) */
static void SistPart_Activate(CSistPart *self, CParticula *p) {
	if (p->active != 1) {
		p->active = 1;
		self->numActive++;
	}
}

CParticula *__thiscall CSistPart_GetFree(CSistPart *self, byte force) {
	float best = -3.4028234663852886e+38f;
	CParticula *oldest = NULL;

	if (!self->enabled) {
		return NULL;
	}
	int n = self->params.nParticulas;
	int i = self->nextFree;
	if (n <= i) {
		i = 0;
	}
	for (int k = 0; k < n; k++) {
		CParticula *p = self->Particles[i];
		if (!p->active) {
			SistPart_Activate(self, p);
			self->nextFree = (byte)k;
			return p;
		}
		/* a static particle has no t: the float after it is read, as in the exe */
		float t = *(float *)((byte *)p + 0x38);
		if (force && (PARTICULA_TYPE(p) == PARTICULA_STATIC || best <= t)) {
			best = t;
			oldest = p;
		}
		i = i + 1 < n ? i + 1 : i + 1 - n;
	}
	if (force && oldest) {
		SistPart_Activate(self, oldest);
		self->nextFree = 0;
		return oldest;
	}
	return NULL;
}

CSistPart *SistPart_FromNode(CNodo3D *nodo, CArchINI *ini) {
	vec3_t pos[0x100];
	char key[0x20], type[0x20];

	CArchINI *prop = nodo->ini;
	CHECK(prop);
	float escala = CArchINI_GetFloat(prop, "Particles", "Scale", 1.0f);
	float alpha = CArchINI_GetFloat(prop, "Particles", "Alpha", 1.0f);
	byte posList = CArchINI_GetBool(prop, "Particles", "PosList", 0);
	if (posList) {
		for (int i = 1; i <= 0x100; i++) {
			vec3_t def = { 0.0f, 0.0f, 0.0f }, v;
			Bprintf(key, sizeof(key), "Pos_%d", i);
			pos[i - 1] = *CArchINI_GetVec3(prop, &v, "Particles", key, &def);
		}
	}
	if (!CArchINI_GetText(prop, "Particles", "Type", type, sizeof(type), NULL)) {
		return NULL;
	}
	void *p = operator_new(sizeof(CSistPart));
	if (!p) {
		return NULL;
	}
	if (!ini || CArchINI_Count(ini) < 1) {
		ini = NULL;
	}
	return CSistPart_ctorParams(p, nodo, type, escala, alpha, posList ? pos : NULL, ini);
}

void __thiscall CSistPart_SetColorRamp(CSistPart *self, uint32_t *ramp) {
	self->colorRamp = ramp;
}

void __thiscall CSistPart_SetEmitPos(CSistPart *self, vec3_t *pos, byte n) {
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

void __thiscall CSistPart_GetEscala2(CSistPart *self, float *out) {
	if (self->nodo) {
		out[0] = self->nodo->escala.x;
		out[1] = self->nodo->escala.y;
		return;
	}
	out[0] = 1.0f;
	out[1] = 1.0f;
}

void __thiscall CSistPart_GetEscala(CSistPart *self, vec3_t *out) {
	if (self->nodo) {
		*out = self->nodo->escala;
		return;
	}
	*out = (vec3_t){ 1.0f, 1.0f, 1.0f };
}

/* the corner c into box, as the exe compares */
static void SistPart_BoxAdd(box_t *box, float x, float y, float z) {
	box->min.x = x <= box->min.x ? x : box->min.x;
	box->min.y = y <= box->min.y ? y : box->min.y;
	box->min.z = z <= box->min.z ? z : box->min.z;
	box->max.x = box->max.x <= x ? x : box->max.x;
	box->max.y = box->max.y <= y ? y : box->max.y;
	box->max.z = box->max.z <= z ? z : box->max.z;
}

byte __thiscall CSistPart_GetBox(CSistPart *self, box_t *box) {
	if (!(self->params.flags & SISTPART_STATIC) || !self->params.birthRate) {
		return 0;
	}
	box->min = (vec3_t){ 3.4028234663852886e+38f, 3.4028234663852886e+38f, 3.4028234663852886e+38f };
	box->max = (vec3_t){ -3.4028234663852886e+38f, -3.4028234663852886e+38f, -3.4028234663852886e+38f };
	if (!(self->params.flags & SISTPART_LOCAL)) {
		E3D_LogInfo(g_FileLog, "INFO: Non-local static particle system: BBox not calculated: %s", self->type);
		return 0;
	}
	for (int i = 0; i < self->params.nParticulas; i++) {
		CParticula *p = self->Particles[i];
		if (!p->active) {
			continue;
		}
		float r = p->size * p->escala * 0.5f;
		SistPart_BoxAdd(box, p->pos.x - r, p->pos.y - r, p->pos.z - r);
		SistPart_BoxAdd(box, p->pos.x + r, p->pos.y + r, p->pos.z + r);
	}
	CHECK(!(box->max.x <= box->min.x) && !(box->max.y <= box->min.y) && !(box->max.z <= box->min.z));
	return 1;
}

void __thiscall CSistPart_ReadParams(CSistPart *self, CArchINI *ini) {
	SistPartParams *pa = &self->params;
	const char *sect;
	char texture[0x100], key[0x20];
	vec3_t zero, v;

	E3D_LogWhite(g_FileLog, "Particle system: %s [%s]", self->type, self->nodo->name);
	if (!CArchINI_FindSection(ini, self->type, 1)) {
		Con_Warnf("ERROR: Particle system data not found: %s\n", self->type);
		self->enabled = 0;
	}
	sect = self->type;
	pa->flags = 0;
	if (CArchINI_GetBool(ini, sect, "Local", 0)) {
		pa->flags |= SISTPART_LOCAL;
	}
	if (CArchINI_GetBool(ini, sect, "LocalPos", 0)) {
		pa->flags |= SISTPART_LOCALPOS;
	}
	if (CArchINI_GetBool(ini, sect, "Static", 0)) {
		pa->flags |= SISTPART_STATIC;
	}
	if (CArchINI_GetBool(ini, sect, "Sparks", 0)) {
		pa->flags |= SISTPART_SPARKS;
	}
	if (CArchINI_GetBool(ini, sect, "MultiSprite", 0)) {
		pa->flags |= SISTPART_MULTISPRITE;
	}
	if (CArchINI_GetBool(ini, sect, "ColorRamp", 0)) {
		pa->flags |= SISTPART_COLORRAMP;
	}
	if (CArchINI_GetBool(ini, sect, "Mipmap", 1)) {
		pa->flags |= SISTPART_MIPMAP;
	}
	pa->nParticulas = CArchINI_GetInt(ini, sect, "Particles", 0x20);
	pa->birthRate = CArchINI_GetInt(ini, sect, "BirthRate", 10);
	pa->lifeTime = CArchINI_GetFloat(ini, sect, "LifeTime", 1.0f);
	pa->scale = CArchINI_GetFloat(ini, sect, "Scale", 1.0f);
	pa->sizeIni = CArchINI_GetFloat(ini, sect, "SizeIni", 1.0f);
	pa->sizeEnd = CArchINI_GetFloat(ini, sect, "SizeEnd", 1.0f);
	pa->sizeVar = CArchINI_GetFloat(ini, sect, "SizeVar", 0.0f);
	pa->sizeLife = CArchINI_GetFloat(ini, sect, "SizeLife", 1.0f);
	pa->colorIni = CArchINI_GetColor(ini, sect, "ColorIni", SISTPART_DEFAULT_COLOR);
	pa->colorEnd = CArchINI_GetColor(ini, sect, "ColorEnd", SISTPART_DEFAULT_COLOR);
	zero = (vec3_t){ 0.0f, 0.0f, 0.0f };
	pa->initPos = *CArchINI_GetVec3(ini, &v, sect, "InitPos", &zero);
	pa->initPosMin = *CArchINI_GetVec3(ini, &v, sect, "InitPosMin", &pa->initPos);
	pa->initPosMax = *CArchINI_GetVec3(ini, &v, sect, "InitPosMax", &pa->initPos);
	zero = (vec3_t){ 0.0f, 0.0f, 0.0f };
	pa->initVel = *CArchINI_GetVec3(ini, &v, sect, "InitVel", &zero);
	pa->initVelMin = *CArchINI_GetVec3(ini, &v, sect, "InitVelMin", &pa->initVel);
	pa->initVelMax = *CArchINI_GetVec3(ini, &v, sect, "InitVelMax", &pa->initVel);
	zero = (vec3_t){ 0.0f, 0.0f, 0.0f };
	pa->initAcc = *CArchINI_GetVec3(ini, &v, sect, "InitAcc", &zero);
	pa->initAccMin = *CArchINI_GetVec3(ini, &v, sect, "InitAccMin", &pa->initAcc);
	pa->initAccMax = *CArchINI_GetVec3(ini, &v, sect, "InitAccMax", &pa->initAcc);
	pa->rotIni = (float)((double)CArchINI_GetFloat(ini, sect, "RotIni", 0.0f) * 3.141592653589793 / 180.0);
	pa->rotEnd = (float)((double)CArchINI_GetFloat(ini, sect, "RotEnd", 0.0f) * 3.141592653589793 / 180.0);
	pa->rotVar = (float)((double)CArchINI_GetFloat(ini, sect, "RotVar", 0.0f) * 3.141592653589793 / 180.0);
	CArchINI_GetText(ini, sect, "Texture", texture, sizeof(texture), "bmp/partic1.bmp");
	cString_Set(&pa->texture, texture);
	pa->spriteIni = (byte)CArchINI_GetInt(ini, sect, "SpriteIni", 0xff);
	pa->spriteEnd = (byte)CArchINI_GetInt(ini, sect, "SpriteEnd", 0xff);
	pa->spriteRand = CArchINI_GetBool(ini, sect, "SpriteRand", 0);
	for (int i = 0; i < 9; i++) {
		Bprintf(key, sizeof(key), "ColorRamp_%02d", i + 1);
		pa->colorRamp[i] = CArchINI_GetColor(ini, sect, key, i < 1 ? SISTPART_DEFAULT_COLOR : pa->colorRamp[i - 1]);
	}
	pa->additiveAlpha = CArchINI_GetBool(ini, sect, "AdditiveAlpha", 1);
	pa->alphaMod = CArchINI_GetBool(ini, sect, "AlphaMod", 1);
	pa->sparkVelScale = CArchINI_GetFloat(ini, sect, "SparkVelScale", 1.0f);
	pa->sparkMaxSize = CArchINI_GetFloat(ini, sect, "SparkMaxSize", 3.4028234663852886e+38f);
	pa->alphaDist = CArchINI_GetFloat(ini, sect, "AlphaDist", 0.0f);
	if (pa->alphaDist > 0.0f && !pa->additiveAlpha) {
		pa->alphaDist = 0.0f;
		Con_Warnf("WARN: Only Additive Alpha Particle Systems can use Alpha Distance parameter\n");
	}
	pa->bounce = CArchINI_GetBool(ini, sect, "Bounce", 0);
	if (pa->bounce) {
		pa->bounce = 0;
		Con_Warnf("WARN: Only World Particle Systems can use Bounce parameter\n");
	}
	pa->zBias = CArchINI_GetFloat(ini, sect, "ZBias", 0.0f);
}

/* v times k */
static vec3_t SistPart_Scaled(const vec3_t *v, float k) {
	return (vec3_t){ v->x * k, v->y * k, v->z * k };
}

void __thiscall CSistPart_GenParticle(CSistPart *self, CParticula *p) {
	SistPartParams *pa = &self->params;
	vec3_t v;

	CHECK(p);
	SistPart_Activate(self, p);
	float k = self->escala * pa->scale * g_SistPartPosScale;
	if (!self->emitPos) {
		p->pos = SistPart_Scaled(SistPart_RandVec(&v, &pa->initPosMin, &pa->initPosMax), k);
	} else {
		byte i = self->emitIdx;
		p->pos = self->emitPos[i];
		uint32_t next = (uint32_t)self->emitIdx + 1;
		self->emitIdx = next < self->nEmitPos ? (byte)next : 0;
	}
	p->escala = self->escala * pa->scale * g_SistPartSizeScale;
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
	if (PARTICULA_TYPE(p) == PARTICULA_STATIC && self->MultiSpr) {
		CHECK((int)sprite < *(int *)((byte *)self->MultiSpr + 8));
		CMultiSprite_GetUV(self->MultiSpr, sprite, p->uv0, p->uv1);
	}
	if (PARTICULA_TYPE(p) != PARTICULA_ACTIVE) {
		return;
	}
	CParticulaActive *a = (CParticulaActive *)p;
	a->b34 = 0;
	a->t = 0.0f;
	a->life = pa->lifeTime * g_SistPartLifeScale;
	a->delay = 0.0f;
	a->sparks = (pa->flags & SISTPART_SPARKS) != 0;
	a->vel = SistPart_Scaled(SistPart_RandVec(&v, &pa->initVelMin, &pa->initVelMax), k);
	a->acc = SistPart_Scaled(SistPart_RandVec(&v, &pa->initAccMin, &pa->initAccMax), k);
	float f = 1.0f;
	if (pa->sizeVar != 0.0f) {
		f = SistPart_RandLerp(0.0f, pa->sizeVar) + 1.0f;
	}
	a->sizeIni = f * pa->sizeIni;
	f = 1.0f;
	if (pa->sizeVar != 0.0f) {
		f = SistPart_RandLerp(0.0f, pa->sizeVar) + 1.0f;
	}
	a->sizeEnd = pa->sizeEnd * f;
	a->sizeLife = 1.0f / pa->sizeLife;
	a->colorIni = pa->colorIni;
	a->colorEnd = pa->colorEnd;
	f = 0.0f;
	if (pa->rotVar != 0.0f) {
		f = SistPart_RandLerp(-pa->rotVar, pa->rotVar);
	}
	a->rotIni = pa->rotIni + f;
	f = 0.0f;
	if (pa->rotVar != 0.0f) {
		f = SistPart_RandLerp(-pa->rotVar, pa->rotVar);
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
	if (pa->flags & SISTPART_COLORRAMP) {
		a->colorRamp = self->colorRamp ? self->colorRamp : pa->colorRamp;
	} else {
		a->colorRamp = NULL;
	}
	a->sparkVelScale = pa->sparkVelScale;
	a->sparkMaxSize = pa->sparkMaxSize;
	a->bounce = pa->bounce;
}

void __thiscall CSistPart_PlaceParticle(CSistPart *self, CParticula *p, const matrix_t *m) {
	if (PARTICULA_TYPE(p) == PARTICULA_ACTIVE && !(self->params.flags & SISTPART_LOCAL)) {
		CParticulaActive *a = (CParticulaActive *)p;
		Vec3_TransformDir0(&p->pos, &p->pos, m);
		Vec3_TransformDir0(&a->vel, &a->vel, m);
		Vec3_TransformDir0(&a->acc, &a->acc, m);
	}
	if (!(self->params.flags & (SISTPART_LOCAL | SISTPART_LOCALPOS))) {
		p->pos.x = p->pos.x + m->m[12];
		p->pos.y = m->m[13] + p->pos.y;
		p->pos.z = m->m[14] + p->pos.z;
	}
}

void __thiscall CSistPart_Emit(CSistPart *self, int n, byte force, float dt, const matrix_t *m) {
	for (int i = 0; i < n;) {
		CParticula *p = CSistPart_GetFree(self, force);
		if (!p) {
			return;
		}
		CSistPart_GenParticle(self, p);
		CSistPart_PlaceParticle(self, p, m);
		PARTICULA_UPDATE(p, self, 0.0f, self->MultiSpr);
		i++;
		((CParticulaActive *)p)->delay = (float)i * dt;
	}
}

void __thiscall CSistPart_EmitN(CSistPart *self, int n, byte force) {
	CSistPart_Emit(self, n, force, 0.0f, g_MatIdentity);
}

void __thiscall CSistPart_EmitM(CSistPart *self, int n, byte force, const matrix_t *m) {
	CSistPart_Emit(self, n, force, 0.0f, m);
}

void __thiscall CSistPart_EmitDt(CSistPart *self, int n, byte force, float dt) {
	CSistPart_Emit(self, n, force, dt, g_MatIdentity);
}

byte __thiscall CSistPart_Read(CSistPart *self, CArchIO *f, MallaD3D_FileCtx *ctx) {
	(void)self;
	(void)f;
	CHECK(CArchINI_FindSection(ctx->nodo->ini, "Particles", 1));
	return 1;
}

byte __thiscall CSistPart_Write(CSistPart *self, CArchIO *f, MallaD3D_FileCtx *ctx) {
	(void)self;
	(void)f;
	CHECK(CArchINI_FindSection(ctx->nodo->ini, "Particles", 1));
	CHECK(!ctx->nodo->escena->mainLOD);
	return 1;
}

void __fastcall CSistPart_CreateMat(CSistPart *self) {
	SistPartParams *pa = &self->params;

	if (!self->mat) {
		CMaterial m;
		CMaterial_ctor(&m);
		Cad_Set(&m.Nombre, "MatSistPart");
		CMaterial_SetMap(&m, 0, pa->texture);
		if (m.mapa[0]) {
			m.mapa[0]->b8 = (pa->flags & SISTPART_MIPMAP) != 0;
			m.mapa[0]->bc = 0;
		}
		if (!pa->additiveAlpha) {
			m.params.srcBlend = 2;
			m.params.dstBlend = 6;
		} else {
			m.params.srcBlend = 5;
			m.params.dstBlend = 2;
		}
		if (!pa->alphaMod) {
			m.params.atrib |= 0x400;
		}
		m.params.transp = 1;
		((byte *)&m.params.w3e)[1] = 0;
		if (strstr(pa->texture, ".DUDV")) {
			m.params.srcBlend = 2;
			m.params.dstBlend = 2;
			m.params.transp = 1;
			((byte *)&m.params.w3e)[1] = 0;
		}
		m.params.b39 = 0;
		self->mat = Mat_New(&m, 1);
		CMaterial_dtor(&m);
	}
	if (pa->flags & SISTPART_MULTISPRITE) {
		self->MultiSpr = CMaterial_GetMultiSprite(self->mat, 0);
		CHECK(!self->MultiSpr || *(int *)((byte *)self->MultiSpr + 8) > 1);
		if (pa->spriteIni == 0xff) {
			pa->spriteIni = 1;
		}
		if (pa->spriteEnd == 0xff) {
			pa->spriteEnd = (byte)(*(int *)((byte *)self->MultiSpr + 8) - 1);
		}
	}
}

void __thiscall CSistPart_SetActive(CSistPart *self, CParticula *p, byte active) {
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

void __fastcall CSistPart_CreateParticles(CSistPart *self) {
	SistPartParams *pa = &self->params;

	CHECK(!self->Particles);
	self->Particles = operator_new_array((uint32_t)pa->nParticulas << 2);
	for (int i = 0; i < pa->nParticulas; i++) {
		CParticula *p;
		if (!(pa->flags & SISTPART_STATIC)) {
			p = operator_new(sizeof(CParticulaActive));
			if (p) {
				p = CParticulaActive_ctor(p);
			}
		} else {
			p = operator_new(sizeof(CParticula));
			if (p) {
				p = CParticulaStatic_ctor(p);
			}
		}
		self->Particles[i] = p;
	}
	g_SistPartCount += pa->nParticulas;
	if (!(pa->flags & SISTPART_STATIC) || !pa->birthRate) {
		return;
	}
	/* a static system: each particle where a moving one would be at its part of the life time */
	CParticulaActive tmp;
	tmp.p.vftable = Game_Ptr(CPARTICULAACTIVE_VFTABLE);
	CParticulaActive_Init(&tmp.p);
	for (int i = 0; i < pa->nParticulas; i++) {
		CSistPart_GenParticle(self, &tmp.p);
		CParticulaActive_Update(&tmp, self, ((float)i / (float)(double)(uint32_t)pa->nParticulas) * pa->lifeTime,
			self->MultiSpr);
		CParticula *p = self->Particles[i];
		CSistPart_GenParticle(self, p);
		p->active = tmp.p.active;
		memcpy(&p->pos, &tmp.p.pos, sizeof(CParticula) - offsetof(CParticula, pos));
		if (self->posList) {
			p->pos = self->posList[i];
		}
		CHECK(p->active);
	}
}

vec3_t *SistPart_RandVec(vec3_t *out, const vec3_t *a, const vec3_t *b) {
	float x = a->x, y, z;
	if (x != b->x) {
		x = SistPart_RandLerp(x, b->x);
	}
	y = a->y;
	if (y != b->y) {
		y = SistPart_RandLerp(y, b->y);
	}
	z = a->z;
	if (z != b->z) {
		z = SistPart_RandLerp(z, b->z);
	}
	out->x = x;
	out->y = y;
	out->z = z;
	return out;
}

void SistPart_LoadIni(const char *name, const char *dir) {
	if (!name) {
		CArchINI_Clear(g_SistPartIni);
		return;
	}
	CArchINI_Load(g_SistPartIni, name, dir, 1);
}

void __thiscall CSistPart_Render(CSistPart *self, void *dev, CNodoR *r) {
	(void)dev;
	if (!g_SistPartOn) {
		return;
	}
	self->inView = 1;
	if (!self->visible || !self->numActive) {
		return;
	}
	if (!r && g_SistPartDebug) {
		const matrix_t *m = &self->nodo->m;
		vec3_t pos = { m->m[12], m->m[13], m->m[14] };
		box_t local = { { -100.0f, -100.0f, -100.0f }, { 100.0f, 100.0f, 100.0f } }, w;
		Box_Transform(&w, &local, m);
		float dx = w.max.x - w.min.x, dy = w.max.y - w.min.y, dz = w.max.z - w.min.z;
		vec3_t center = { (w.max.x + w.min.x) * 0.5f, (w.min.y + w.max.y) * 0.5f, (w.min.z + w.max.z) * 0.5f };
		float radius = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) * 0.5f;
		byte vis = SectorVis_Sphere(&self->vis, &pos, &center, radius, self->nodo->name);
		self->inView = vis;
		if (g_SistPartDebug > 1) {
			SectorVis_Draw(&self->vis, &pos, vis, 0, self->nodo->name);
		}
		if (g_SistPartDebug > 2) {
			Debug_Text3D(g_DebugFont, 4.0f, -12.0f, &pos, 0x8000, Game_Ptr(0x3bae88), self->type);
		}
	}
	if (self->inView) {
		SistPart_Draw(self, r);
	}
}

void __thiscall CSistPart_Render2(CSistPart *self, void *dev) {
	if (g_SistPartOn && self->visible && self->numActive) {
		SistPart_Draw2(self, dev);
	}
}

void __thiscall CSistPart_SetAlpha(CSistPart *self, float alpha) {
	self->alpha = alpha;
}

/* a bounce on the floor of the world: the velocity mirrored at the hit triangle, halved */
static void Particula_Bounce(CParticulaActive *a, const vec3_t *prev, float dt) {
	vec3_t hit;
	SistPartHit info = { 0, 0, NULL };

	CHECK(g_Mundo);
	void *suelo = g_Mundo->ObjSuelo;
	if (!suelo || !CSuelo_TraceSeg(suelo, prev, &a->p.pos, &hit, &info, 0)) {
		return;
	}
	a->vel.x = -a->vel.x * 0.5f;
	a->vel.y = -a->vel.y * 0.5f;
	a->vel.z = -a->vel.z * 0.5f;
	uint32_t i = ((uint16_t *)*(void **)(info.mesh + 0x10))[info.tri];
	const float *n = (const float *)(*(byte **)(*(byte **)(info.mesh + 8) + 0x30) + 4 + i * 0x1c);
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

void __thiscall CParticulaActive_Update(CParticulaActive *self, CSistPart *sp, float dt, void *multiSpr) {
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
		Particula_Bounce(self, &prev, dt);
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
	uint32_t u = (uint32_t)X87_Fistp((double)(t * 16777216.0f));
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
		cur = (int)X87_Fistp((double)((float)(self->spriteEnd - self->spriteIni) * t)) + self->spriteIni;
	} else if (self->spriteEnd < self->spriteIni) {
		cur = self->spriteIni - (int)X87_Fistp((double)((float)(self->spriteIni - self->spriteEnd) * t));
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

void __thiscall CSistPart_Update(CSistPart *self, CNodoR *r) {
	if (!g_SistPartOn) {
		return;
	}
	if (g_SistPartFlags != -1) {
		self->params.flags = (uint32_t)g_SistPartFlags;
	}
	if (self->lastFrame == g_FrameCount) {
		return;
	}
	const matrix_t *m;
	if (!self->nodo && !r) {
		CHECK(self->nodo || r);
	}
	m = r ? &r->m : &self->nodo->m;
	uint32_t now = g_TimeMs;
	if ((self->params.flags & SISTPART_STATIC) || !self->inView) {
		return;
	}
	uint32_t last = self->lastTime;
	float dt = last ? SistPart_Seconds(now - last) : 0.0f;
	if (last + 100u < now) {
		dt = 0.1f;
		self->birthTime = now - 100;
	}
	self->lastTime = now;
	self->lastFrame = g_FrameCount;
	if (self->numActive) {
		for (int i = 0; i < self->params.nParticulas; i++) {
			CParticula *p = self->Particles[i];
			if (p->active && PARTICULA_TYPE(p) == PARTICULA_ACTIVE && !((CParticulaActive *)p)->b34) {
				PARTICULA_UPDATE(p, self, dt, self->MultiSpr);
			}
		}
	}
	if (!self->params.birthRate) {
		self->birthTime = self->lastTime;
		return;
	}
	uint32_t step = 1000u / (uint32_t)(self->params.birthRate + g_SistPartBirthAdd);
	while (self->birthTime + step < self->lastTime) {
		self->birthTime += step;
		CParticula *p = CSistPart_GetFree(self, 0);
		if (!p) {
			self->birthTime = self->lastTime;
			break;
		}
		CSistPart_GenParticle(self, p);
		CSistPart_PlaceParticle(self, p, m);
		PARTICULA_UPDATE(p, self, SistPart_Seconds(self->lastTime - self->birthTime), self->MultiSpr);
	}
	self->lastPos = (vec3_t){ m->m[12], m->m[13], m->m[14] };
}
