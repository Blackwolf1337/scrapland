/*
 * Models of the engine3d, original source Engine/Modelo.cpp (include/scrap/modelo.h), with the drawing, the choice
 * of the LOD and the visibility test (through the profiler, the debug texts and the sector visibility of the
 * renderer). Not here: 0x255d00 (CHECK(0), no callers).
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/escena.h"
#include "scrap/fuentes.h"
#include "scrap/luz.h"
#include "scrap/material.h"
#include "scrap/modelo.h"
#include "scrap/prof.h"
#include "scrap/render.h"
#include "scrap/stencil.h"
#include "scrap/sueloportal.h"
#include "scrap/utild3d.h"
#include "scrap/varios.h"

#define CLOD_DTOR 0x252d80
#define LUZ_OMNI_NAME GAME_STR(str_3d598c, 0x3d598c)
#define LUZ_FOCO_NAME GAME_STR(str_3d5994, 0x3d5994)
#define LUZ_DIREC_NAME GAME_STR(str_3d599c, 0x3d599c)
#define MORPH_SPEED_UNIT 0.01f
#define MORPH_SPEED_MAX 0.08f
#define STENCIL_DIST 4000.0f

#define g_AnimPausa GAME_DATA(int, 0x4ae638)	/* the animations stand still */
#define g_AnimSpeed GAME_DATA(float, 0x4ae770)	/* the speed of all animations */
#define g_FrameTime GAME_DATA(float, 0x4a8ed4)	/* the time of the frame */

static CAnimSlot *Modelo_Slot(CModelo *self, uint32_t i) {
	CHECK(self->slots.p && i < (uint32_t)self->slots.n);
	return &((CAnimSlot *)self->slots.p)[i];
}

static CLod *Modelo_Lod(CModelo *self, uint32_t i) {
	CHECK(self->lods.p && i < (uint32_t)self->lods.n);
	return &((CLod *)self->lods.p)[i];
}

static CNodoR *Modelo_NodoR(CTabla *t, uint32_t i) {
	CHECK(t->p && i < (uint32_t)t->n);
	return &((CNodoR *)t->p)[i];
}

static void Vec3_Zero(vec3_t *v) {
	v->x = 0.0f;
	v->y = 0.0f;
	v->z = 0.0f;
}

static void Vec3_One(vec3_t *v) {
	v->x = 1.0f;
	v->y = 1.0f;
	v->z = 1.0f;
}

/* the inline identity of the original (the stores of Matrix_LoadIdentity) */
static void Modelo_Identity(matrix_t *m) {
	memset(m->m, 0, sizeof(m->m));
	m->m[15] = 1.0f;
	m->m[10] = 1.0f;
	m->m[5] = 1.0f;
	m->m[0] = 1.0f;
}

/* m (identity) times the pivot, the scale, the second rotation, the animation offset, the rotation, the position and
 * the local matrix */
static void Modelo_Transform(CModelo *self, matrix_t *m) {
	matrix_t t;

	Matrix_AddPos(m, &self->pivot);
	Matrix_PostScale(m, &self->scale);
	Matrix_PostRotate(m, &self->rot2);
	Matrix_AddPos(m, &self->animOfs);
	Matrix_PostRotate(m, &self->rot);
	Matrix_AddPos(m, &self->pos);
	*m = *Matrix_MulD3D(&t, m, &self->local);
}

static void Modelo_AddMove(CModelo *self, const vec3_t *r) {
	self->pos.x = self->pos.x + r->x;
	self->pos.y = r->y + self->pos.y;
	self->pos.z = r->z + self->pos.z;
}

static void Lods_Delete(CTabla *t) {
	if (t->p) {
		Delete_Objects(t->p, sizeof(CLod), CLOD_DTOR);
		t->p = NULL;
		t->n = 0;
	}
}

CModelo *__fastcall CModelo_ctor(CModelo *self) {
	self->vftable = CMODELO_VFTABLE;
	self->name = NULL;
	self->slots.p = NULL;
	self->slots.n = 0;
	self->lods.p = NULL;
	self->lods.n = 0;
	CAnimGrupos_ctor(&self->anims);
	self->sector.flags = 0;
	self->sector.sector = 0;
	self->sector.pos.z = 0.0f;
	self->sector.pos.y = 0.0f;
	self->sector.pos.x = 0.0f;
	Cad_Set(&self->name, "Modelo");
	self->escena = NULL;
	self->mainLOD = NULL;
	Tabla_Alloc10Z(&self->slots, MAX_ANIM_CHANNEL);
	Vec3_Zero(&self->pos);
	Vec3_Zero(&self->rot);
	Vec3_One(&self->scale);
	Vec3_Zero(&self->animOfs);
	Vec3_Zero(&self->pivot);
	Vec3_Zero(&self->rot2);
	Vec3_One(&self->escala);
	Matrix_LoadIdentity(&self->local);
	self->enable = 1;
	self->visible = 0;
	self->showBox = 0;
	self->animate = 1;
	self->dynLight = 1;
	self->alwaysVisible = 0;
	self->hasMesh = 0;
	self->hasShadow = 0;
	self->nodosR = NULL;
	Matrix_LoadIdentity(&self->mRender);
	self->callback = NULL;
	self->cbArg = NULL;
	self->cbArg2 = NULL;
	self->lista = NULL;
	self->lodDist = 1.0f;
	self->f10c = 0;
	self->f110 = 0;
	self->sortZBias = 0.0f;
	self->sortZMult = 1.0f;
	self->stencilDist = STENCIL_DIST;
	return self;
}

void __fastcall Tabla_DeleteLods(CTabla *t) {
	Lods_Delete(t);
}

CLod *__fastcall CLod_ctor(CLod *self) {
	self->model = NULL;
	self->f4 = 0;
	return self;
}

void __fastcall CLod_dtor(CLod *self) {
	CHECK(!self->model);
}

void __fastcall CModelo_dtor(CModelo *self) {
	self->vftable = CMODELO_VFTABLE;
	for (uint32_t i = 0; (int)i < self->lods.n; i++) {
		Modelo_Destroy(&Modelo_Lod(self, i)->model, 0);
	}
	CModelo_SetEscena(self, NULL);
	CAnimGrupos_Clear(&self->anims);
	Lods_Delete(&self->lods);
	Tabla_Free(&self->slots);
	if (self->name) {
		Cad_Set(&self->name, NULL);
	}
}

/* the array branch of the vector deleting destructors: the count before the objects, the destructor on each */
static void *DeleteVecArray(void *self, uint32_t flags, uint32_t size, uint32_t dtor) {
	uint32_t *mem = (uint32_t *)self - 1;

	crt_eh_vector_dtor(self, size, *mem, Game_Ptr(dtor));
	if (flags & 1) {
		operator_delete_array(mem);
	}
	return mem;
}

void *__thiscall CLod_DeleteVec(CLod *self, uint32_t flags) {
	if (flags & 2) {
		return DeleteVecArray(self, flags, sizeof(CLod), CLOD_DTOR);
	}
	CLod_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CLod));
	}
	return self;
}

void *__thiscall CNodoR_DeleteVec(CNodoR *self, uint32_t flags) {
	if (flags & 2) {
		return DeleteVecArray(self, flags, sizeof(CNodoR), CNODOR_DTOR);
	}
	CNodoR_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CNodoR));
	}
	return self;
}

CModelo *__thiscall CModelo_Delete(CModelo *self, byte flags) {
	CModelo_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

void __fastcall CModelo_Disable(CModelo *self) {
	Modelo_SetEnable(self, 0);
}

void __fastcall CModelo_Enable(CModelo *self) {
	Modelo_SetEnable(self, 1);
}

void __thiscall CModelo_ReleaseD3D(CModelo *self, void *dev, byte escena) {
	CHECK(self->escena);
	if (escena) {
		CEscena_ReleaseD3D(self->escena, dev);
	}
}

CAnimSlot *__thiscall CModelo_GetSlot(CModelo *self, uint32_t i) {
	return Modelo_Slot(self, i);
}

double CAnim_GetSpeed(CAnimGrupo *a) {
	if (a && !g_AnimPausa) {
		return (double)a->ctrl.speed * (double)g_AnimSpeed;
	}
	return 0.0;
}

void __thiscall CModelo_InitD3D(CModelo *self, void *dev, byte escena) {
	CHECK(self->escena);
	if (escena) {
		CEscena_InitD3D(self->escena, dev);
	}
}

void __thiscall CModelo_SetupRender(CModelo *self, NodoAnimCtx *ctx) {
	vec3_t one;

	Vec3_One(&one);
	RenderCtx_Init(ctx, self, &one);
	for (uint32_t i = 0; i < MAX_ANIM_CHANNEL; i++) {
		if (Modelo_Slot(self, i)->anim) {
			CAnimSlot *s = Modelo_Slot(self, i);

			/* RenderCtx.h */
			CHECK(i < MAX_ANIM_CHANNEL);
			RenderCtx_SetCanal(&ctx->chan[i], s);
		}
	}
	CModelo_AnimOffset(self, ctx);
	Modelo_Identity(&ctx->m);
	Modelo_Transform(self, &ctx->m);
	Vec3_Zero(&self->animOfs);
}

CNodoR *__thiscall CModelo_GetNodoR(CModelo *self, CNodo3D *nodo) {
	CHECK(0);
	return Modelo_NodoR(self->nodosR, nodo->i48);
}

matrix_t *__thiscall CModelo_GetNodeMatrix(CModelo *self, matrix_t *m, uint32_t inodo, byte anim, uint32_t slot,
	byte *visible, float *fov) {
	CNodo3D *nodo = Modelo_NodoR(self->nodosR, inodo)->nodo;
	vec3_t pos = nodo->pos;
	quat_t q = nodo->rot;
	matrix_t local, t;

	if (anim) {
		CAnimGrupo *a = Modelo_Slot(self, slot)->anim;

		if (a) {
			float f = a->ctrl.frame;
			int frame = (int)f;
			float frac = f - (float)frame;
			CAnimNodo *track = CAnimacion_GetNodoTabla(a->Anim->AnimM3D, nodo->indice);

			if (track) {
				CAnimNodo_GetPosRot(track, &pos, &q, frame, frac);
			}
			if (visible && track) {
				CAnimNodo_GetVisible(track, visible, frame, frac);
			}
			if (fov && track) {
				CAnimNodo_GetFOV(track, fov, frame, frac);
			}
		}
	}
	Quat_ToMatrixD3D(m, &q);
	Matrix_AddPos(m, &pos);
	CModelo_GetMatrix(self, &local);
	*m = *Matrix_MulD3D(&t, m, &local);
	return m;
}

void __thiscall CModelo_SetNumLods(CModelo *self, int n) {
	Tabla_New8(&self->lods, n);
}

void __thiscall CModelo_SetLod(CModelo *self, uint32_t i, CModelo *model) {
	CHECK(model);
	Modelo_Lod(self, i)->model = model;
	Modelo_Lod(self, i)->f4 = 0;
	CHECK(model->escena->mainLOD);
	const char *mainLOD = model->escena->mainLOD;
	const char *name = Ruta_FileName(self->escena->name);
	CHECK(!crt_stricmp(Ruta_FileName(mainLOD), name));
	if (!self->escena->fromAmc) {
		CHECK(!strcmp(model->escena->mainLOD, self->escena->name));
	}
	CHECK(!model->mainLOD);
	model->mainLOD = self;
}

byte __thiscall CModelo_SetMorphSubset(CModelo *self, uint32_t slot, const char *tag) {
	CAnimGrupo *a = Modelo_Slot(self, slot)->anim;

	CHECK(a && a->morph && a->Anim->AnimM3D->flag);
	E3D_LogWhite(g_FileLog, "SetMorphSubset: [%s] [%s]", a->name, tag);
	uint32_t i = (uint32_t)MorphData_Find(a->morph, tag);
	if (i >= MAX_MORPHDATA) {
		E3D_LogBlue(g_FileLog, "Morph tag not found: %s", tag);
		return 0;
	}
	float k = MORPH_SPEED_MAX, s = a->ctrl.speed;
	a->iMorph = (byte)i;
	a->morphCount = a->morph[i].param3 + 1;
	a->ctrl.speed = k < s ? k : s;
	return 1;
}

int __fastcall CModelo_GetSector(CModelo *self) {
	SectorVis_Update(&self->sector, &self->pos, self->name);
	return self->sector.sector;
}

CAnimGrupo *__thiscall CModelo_AddAnim(CModelo *self, CEscena *anim, const char *nombre) {
	CAnimGrupo *a = AnimGrupo_New(self->escena, anim, nombre);

	if (CAnimGrupos_Find(&self->anims, a->name, NULL)) {
		Con_Warnf("WARN: Animation '%s' already in model '%s'\n", a->name, self->name);
	}
	CAnimGrupos_Add(&self->anims, a);
	return a;
}

byte __thiscall CModelo_AddAnimEvents(CModelo *self, const char *name, float start, float end) {
	CAnimGrupo *a = CAnimGrupos_Find(&self->anims, name, NULL);
	CAnimEvent *ev, *first;

	if (!a) {
		return 0;
	}
	if (end < 0.0f) {
		end = (float)(int)((uint32_t)a->ctrl.frames - 1 + (uint32_t)a->ctrl.primero);
	}
	if (start < 0.0f) {
		start = (float)a->ctrl.primero;
	}
	ev = operator_new(0x14);
	if (ev) {
		CAnimEvent_ctor(ev, start, 0, 0);
	}
	first = AnimEventos_Add(&a->eventos, ev);
	ev = operator_new(0x14);
	if (ev) {
		CAnimEvent_ctor(ev, end, 0xc, (int)first);
	}
	AnimEventos_Add(&a->eventos, ev);
	return 1;
}

void __thiscall CModelo_AnimPlay(CModelo *self, uint32_t slot) {
	CAnimGrupo *a = Modelo_Slot(self, slot)->anim;

	if (a) {
		CAnimCtrl_Play(&a->ctrl);
	}
}

int __thiscall CModelo_AnimState(CModelo *self, uint32_t slot) {
	CAnimGrupo *a = Modelo_Slot(self, slot)->anim;

	if (a) {
		return a->ctrl.state;
	}
	return ANIM_FIN;
}

void __thiscall CModelo_AnimStop(CModelo *self, uint32_t slot) {
	CAnimGrupo *a = Modelo_Slot(self, slot)->anim;

	if (a) {
		CAnimCtrl_Stop(&a->ctrl);
	}
}

void __thiscall CModelo_AnimPause(CModelo *self, uint32_t slot) {
	CAnimGrupo *a = Modelo_Slot(self, slot)->anim;

	if (a) {
		CAnimCtrl_Pause(&a->ctrl);
	}
}

void __thiscall CModelo_AnimFade(CModelo *self, float t, uint32_t slot) {
	CAnimGrupo *a = Modelo_Slot(self, slot)->anim;

	if (a) {
		CAnimCtrl_Fade(&a->ctrl, t);
	}
}

void __thiscall CModelo_MoveForward(CModelo *self, float d) {
	vec3_t v = { 0.0f, 0.0f, -d }, t;

	Modelo_AddMove(self, Vec3_Rotate(&t, &v, &self->rot));
}

void __thiscall CModelo_UpdateAnims(CModelo *self, float dt, int iAnim) {
	(void)dt;
	CHECK(iAnim == -1);
	for (uint32_t i = 0; (int)i < MAX_ANIM_CHANNEL; i++) {
		CAnimSlot *s = Modelo_Slot(self, i);
		CAnimGrupo *a = s->anim, *prev = s->prev;
		byte keep = 0;

		if (a && self->animate) {
			CAnimCtrl_Update(&a->ctrl, &self->rot2, -1.0f);
			if (i == 0) {
				vec3_t t;

				Modelo_AddMove(self, Vec3_Rotate(&t, &a->ctrl.move, &self->rot));
			}
		}
		if (prev) {
			if (self->animate) {
				CAnimCtrl_Update(&prev->ctrl, &self->rot2, -1.0f);
			}
			if (a && !g_AnimPausa) {
				float v = a->ctrl.speed * g_AnimSpeed;

				if (v != 0.0f) {
					float ts = World_TimeScale();
					float k = 1.0f / v * g_FrameTime;
					float b = s->blend - ts * k;

					s->blend = b;
					keep = 0.0f < b;
				}
			}
		}
		if (!keep) {
			s->prev = NULL;
			s->blend = 0.0f;
		}
		a = s->anim;
		if (a && !s->prev && a->Anim->AnimM3D->flag) {
			byte subset = s->subset;
			s->prev = a;
			s->blend = 1.0f;
			s->prevSubset = subset;
			uint32_t im = a->iMorph;
			CHECK(im < MAX_MORPHDATA);
			MorphData *md = s->anim->morph;
			s->anim->ctrl.speed = (float)md[im].Interp * MORPH_SPEED_UNIT;
			s->subset = md[im].Target;
			byte count = s->anim->morphCount;
			if (count) {
				count--;
			} else {
				if (crt_rand() % 100 >= md[im].param7) {
					continue;
				}
				int n = Rand_Other(md[im].NextMin, md[im].NextMax, im, 3);
				md = s->anim->morph;
				s->subset = md[n].Target;
				s->anim->iMorph = (byte)n;
				count = md[n].param3;
			}
			s->anim->morphCount = count;
		}
	}
}

CNodo3D *__thiscall CModelo_FindClase(CModelo *self, CNodo3D *from, uint32_t clase) {
	if (self->escena) {
		return CEscena_FindClase(self->escena, from, clase);
	}
	return NULL;
}

CNodo3D *__thiscall CModelo_FindNodo(CModelo *self, const char *name) {
	if (self->escena && self->escena->Nodo3D) {
		return CNodo3D_Find(self->escena->Nodo3D, name, 1);
	}
	return NULL;
}

CNodoR *__thiscall CModelo_FindNodoR(CModelo *self, const char *name, uint32_t *idx, byte wild, byte nocase) {
	char tmp[1024], tmp2[1024];

	CHECK(self->nodosR);
	CHECK(name);
	if (wild && nocase) {
		CHECK(strlen(name) < sizeof(tmp));
		Cad_Copy(tmp, name, sizeof(tmp));
		crt_strlwr(tmp);
	}
	CTabla *tab = self->nodosR;
	for (uint32_t i = 0; (int)i < tab->n; i++, tab = self->nodosR) {
		CNodoR *e = Modelo_NodoR(tab, i);
		const char *nombre = e->nodo->name;
		int eq;

		if (wild) {
			if (nocase) {
				CHECK(strlen(nombre) < sizeof(tmp2));
				Cad_Copy(tmp2, e->nodo->name, sizeof(tmp2));
				crt_strlwr(tmp2);
				eq = Wild_Match(tmp, tmp2);
			} else {
				eq = Wild_Match(name, nombre);
			}
		} else if (nocase) {
			eq = !crt_stricmp(name, nombre);
		} else {
			eq = !strcmp(name, nombre);
		}
		if (eq) {
			if (idx) {
				*idx = i;
			}
			return e;
		}
	}
	return NULL;
}

CNodo3D *__thiscall CModelo_FindNodoWild(CModelo *self, const char *pat) {
	if (self->escena && self->escena->Nodo3D) {
		return CNodo3D_FindWild(self->escena->Nodo3D, pat, 1);
	}
	return NULL;
}

void __thiscall CModelo_AnimOffset(CModelo *self, NodoAnimCtx *ctx) {
	CHECK(ctx);
	Vec3_Zero(&self->animOfs);
	if (ctx->chan[0].anim && ctx->chan[0].anim->anim[0]) {
		self->animOfs = ((CAnimGrupo *)ctx->chan[0].anim->anim[0])->ctrl.ofs;
		CAnimGrupo *p = ctx->chan[0].anim->anim[1];
		if (p) {
			vec3_t o = p->ctrl.ofs;
			float t = ctx->chan[0].blend;
			float w = 1.0f - t;

			self->animOfs.x = self->animOfs.x * w + o.x * t;
			self->animOfs.y = self->animOfs.y * w + o.y * t;
			self->animOfs.z = self->animOfs.z * w + o.z * t;
		}
	}
}

void __thiscall CModelo_GetMatrix(CModelo *self, matrix_t *m) {
	Matrix_LoadIdentity(m);
	Modelo_Transform(self, m);
}

void __thiscall CModelo_SetAnim(CModelo *self, int idx, uint32_t slot) {
	CAnimGrupo *a = CAnimGrupos_Get(&self->anims, idx);

	if (a == Modelo_Slot(self, slot)->anim) {
		return;
	}
	if (a && !g_AnimPausa && a->ctrl.speed * g_AnimSpeed != 0.0f) {
		CAnimSlot *s = CModelo_GetSlot(self, slot);

		if (!s->prev) {
			s->prev = s->anim;
			s->blend = 1.0f;
		} else if (0.5f > s->blend) {
			float b = s->blend;
			s->prev = s->anim;
			s->blend = 1.0f - b;
		}
		s->anim = a;
		if (a == s->prev) {
			s->prev = NULL;
		}
	} else {
		CAnimSlot *s = Modelo_Slot(self, slot);

		s->anim = a;
		s->prev = NULL;
		s->blend = 0.0f;
	}
	if (a) {
		CAnimCtrl_Start(&a->ctrl);
	}
}

void __thiscall CModelo_SetAnimName(CModelo *self, const char *name, uint32_t slot) {
	CHECK(0);
	CAnimGrupo *cur = Modelo_Slot(self, slot)->anim;
	if (cur && name && !crt_stricmp(cur->Anim->AnimM3D->name, name)) {
		return;
	}
	CAnimGrupo *a = CAnimGrupos_Find(&self->anims, name, NULL);
	if (a != cur) {
		if (a) {
			CAnimCtrl_Start(&a->ctrl);
		}
		CAnimSlot *s = Modelo_Slot(self, slot);
		s->anim = a;
		s->prev = NULL;
		s->blend = 0.0f;
	}
}

void __thiscall CModelo_SetEscena(CModelo *self, CEscena *esc) {
	CTabla *tab = self->nodosR;

	if (tab) {
		if (tab->p) {
			Delete_Objects(tab->p, sizeof(CNodoR), CNODOR_DTOR);
			tab->p = NULL;
			tab->n = 0;
		}
		operator_delete_sized(tab, sizeof(*tab));
		self->nodosR = NULL;
	}
	if (esc) {
		esc->Refs++;
	}
	if (self->escena) {
		Escena_Unref(&self->escena);
	}
	self->escena = esc;
	if (!esc) {
		return;
	}
	if (esc->ini) {
		if (CArchINI_GetBool(esc->ini, "Model", "StaticLighting", 0)) {
			self->dynLight = 0;
			E3D_LogWhite(g_FileLog, "MODEL: StaticLighting: %s", self->name);
		}
		if (CArchINI_GetBool(self->escena->ini, "Model", "AlwaysVisible", 0)) {
			self->alwaysVisible = 1;
			E3D_LogWhite(g_FileLog, "MODEL: AlwaysVisible: %s", self->name);
		}
		float r = CArchINI_GetFloat(self->escena->ini, "Model", "BBoxRadius", 0.0f);
		if (r != 0.0f) {
			box_t *b = &self->escena->box;
			Vec3_Zero(&b->min);
			Vec3_Zero(&b->max);
			b = &self->escena->box;
			b->min.x = b->min.x - r;
			b->min.y = b->min.y - r;
			b->min.z = b->min.z - r;
			b->max.x = r + b->max.x;
			b->max.y = b->max.y + r;
			b->max.z = b->max.z + r;
			E3D_LogWhite(g_FileLog, "MODEL: BBoxRadius: %f [%s]", (double)r, self->name);
		}
		self->sortZBias = CArchINI_GetFloat(self->escena->ini, "Model", "SortZBias", self->sortZBias);
		self->sortZMult = CArchINI_GetFloat(self->escena->ini, "Model", "SortZMult", self->sortZMult);
		self->stencilDist = CArchINI_GetFloat(self->escena->ini, "Model", "StencilDist", self->stencilDist);
	}
	CEscena *e = self->escena;
	float dy = e->box.max.y - e->box.min.y;
	float dx = e->box.max.x - e->box.min.x;
	float dz = e->box.max.z - e->box.min.z;
	if ((float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) * 0.5f == 0.0f) {
		if (self->dynLight || !self->alwaysVisible) {
			Con_Warnf("ERROR: Model with invalid BBox: %s [%s]\n", self->name, e->name);
		}
		self->dynLight = 0;
		self->alwaysVisible = 1;
	}
	self->hasMesh = 0;
	for (CNodo3D *n = self->escena->Nodo3D; n; n = n->sig) {
		if ((CNodo3D_Clase(n) == CLASE_OBJ3D_MALLATRI || CNodo3D_Clase(n) == CLASE_OBJ3D_MALLAD3D) && !(n->flags & 0xa8)) {
			self->hasMesh = 1;
			break;
		}
	}
	self->hasShadow = 0;
	for (CNodo3D *n = self->escena->Nodo3D; n; n = n->sig) {
		if ((n->flags >> 30 & 1) && (CNodo3D_Clase(n) == CLASE_OBJ3D_MALLATRI || CNodo3D_Clase(n) == CLASE_OBJ3D_MALLAD3D ||
			CNodo3D_Clase(n) == CLASE_OBJ3D_SISTPART) && !(n->flags & 0x88)) {
			self->hasShadow = 1;
			return;
		}
	}
}

void __thiscall CModelo_SetCallback(CModelo *self, modeloCallback_t fn, void *arg, void *arg2) {
	self->callback = fn;
	self->cbArg = arg;
	self->cbArg2 = arg2;
}

void __thiscall CModelo_SetPivot(CModelo *self, const vec3_t *v, byte rel, byte update) {
	if (rel) {
		CHECK(self->escena);
		if (update) {
			CEscena_UpdateBox(self->escena, NULL, 1);
		}
		const box_t *b = &self->escena->box;
		float dx = b->max.x - b->min.x;
		float dy = b->max.y - b->min.y;
		float dz = b->max.z - b->min.z;
		self->pivot.x = -(b->min.x + v->x * dx);
		self->pivot.y = -(b->min.y + v->y * dy);
		self->pivot.z = -(b->min.z + v->z * dz);
		return;
	}
	self->pivot.x = -v->x;
	self->pivot.y = -v->y;
	self->pivot.z = -v->z;
}

byte __thiscall CModelo_SetPivotNodo(CModelo *self, const char *name) {
	if (self->escena && self->escena->Nodo3D) {
		CNodo3D *n = CNodo3D_Find(self->escena->Nodo3D, name, 1);

		if (n) {
			self->pivot.x = -n->pos.x;
			self->pivot.y = -n->pos.y;
			self->pivot.z = -n->pos.z;
			return 1;
		}
	}
	return 0;
}

void __thiscall CModelo_SetSlotAnim(CModelo *self, uint32_t slot, CAnimGrupo *a) {
	CAnimSlot *s = Modelo_Slot(self, slot);

	s->anim = a;
	s->prev = NULL;
	s->blend = 0.0f;
}

/* the render nodes of the nodes with (pass 1) or without (pass 0) NODO_NO_TABNODOR, from index k on */
static int Modelo_AddNodosR(CModelo *self, int k, uint32_t pass) {
	for (CNodo3D *n = self->escena->Nodo3D; n; n = n->sig) {
		if ((n->flags >> 22 & 1) == pass) {
			CHECK(n->i48 == k);
			CNodoR *e = &((CNodoR *)self->nodosR->p)[k++];
			e->owner = (byte *)self;
			CNodoR_SetNodo(e, n);
			e->flags |= 2;
		}
	}
	return k;
}

void __thiscall CModelo_BuildNodosR(CModelo *self, int unused) {
	(void)unused;
	CHECK(self->escena);
	for (CNodo3D *n = self->escena->Nodo3D; n; n = n->sig) {
		if (CNodo3D_Clase(n) == CLASE_OBJ3D_LUZ) {
			CLuz *l = (CLuz *)n->obj3D;

			/* objeto3d.h */
			if (l->clase != CLASE_OBJ3D_LUZ) {
				CheckFailed("devNULL", __FILE__, __TIMESTAMP__, __LINE__);
				l = NULL;
			}
			if (l->activa) {
				Con_Warnf("WARN: Active light [%s] in model: %s\n", n->name, self->name);
			} else {
				E3D_LogVerbose(g_FileLog, "WARN: Unhidden light [%s] in model: %s", n->name, self->name);
			}
		}
	}
	int num = CEscena_NumNodos(self->escena);
	CTabla *t = operator_new(sizeof(CTabla));
	if (t) {
		t->p = NULL;
		t->n = 0;
	}
	self->nodosR = t;
	Tabla_NewA8(t, num);
	int k = Modelo_AddNodosR(self, 0, 0);
	k = Modelo_AddNodosR(self, k, 1);
	CHECK(k == self->nodosR->n);
}

void Modelo_Check(CModelo *m) {
	CHECK(m);
}

CAnimacion *__thiscall CModelo_GetSlotAnimM3D(CModelo *self, uint32_t slot) {
	CAnimGrupo *a = Modelo_Slot(self, slot)->anim;

	if (a) {
		return a->Anim->AnimM3D;
	}
	return NULL;
}

CAnimGrupo *__thiscall CModelo_GetAnim(CModelo *self, int i) {
	return CAnimGrupos_Get(&self->anims, i);
}

int __thiscall CModelo_FindAnim(CModelo *self, const char *name) {
	int idx = -1;

	CAnimGrupos_Find(&self->anims, name, &idx);
	return idx;
}

const char *__thiscall CModelo_GetAnimName(CModelo *self, int i) {
	CAnimGrupo *a = CAnimGrupos_Get(&self->anims, i);

	if (a) {
		return a->name;
	}
	return NULL;
}

CAnimGrupo *__thiscall CModelo_GetSlotAnim(CModelo *self, uint32_t slot) {
	return Modelo_Slot(self, slot)->anim;
}

float __thiscall CModelo_GetSlotFrame(CModelo *self, uint32_t slot) {
	CAnimGrupo *a = Modelo_Slot(self, slot)->anim;

	if (a) {
		return a->ctrl.frame;
	}
	return 0.0f;
}

const char *__fastcall CLuz_TipoNombre(const CLuz *l) {
	switch (l->tipo) {
	case LUZ_OMNI:
		return LUZ_OMNI_NAME;
	case LUZ_FOCO:
		return LUZ_FOCO_NAME;
	case LUZ_DIREC:
		return LUZ_DIREC_NAME;
	}
	/* luz.h */
	CHECK(0);
	return NULL;
}

/* the render variables of the models */
#define g_UseMLOD GAME_DATA(int, 0x4ae6e0)		/* R_UseMLOD */
#define g_DistMLOD1 GAME_DATA(float, 0x4ae608)		/* R_DistMLOD1, the distance of LOD i is i times it */
#define g_DrawMLODMask GAME_DATA(uint32_t, 0x4ae680)	/* R_DrawMLODMask: the LODs drawn as the main model */
#define g_DrawMLODMin GAME_DATA(int, 0x4ae6c8)		/* R_DrawMLODMin */
#define g_DrawMLODPart GAME_DATA(int, 0x4ae5f0)		/* R_DrawMLODPart: the particles of the main model with a LOD */
#define g_ModelFastIsVis GAME_DATA(int, 0x4ae620)	/* R_ModelFastIsVis */
#define g_ModelPortalVis GAME_DATA(int, 0x4ae6b0)	/* R_ModelPortalVis */
#define g_SphereVisMult GAME_DATA(float, 0x4ae698)	/* R_SphereVisMult */
#define g_DistMStencil GAME_DATA(float, 0x4ae758)	/* R_DistMStencil */
#define g_SkipUpdateVis GAME_DATA(int, 0x4ae740)	/* R_SkipUpdateVis */
#define g_ShowModelSector GAME_DATA(int, 0x4ae728)	/* R_ShowModelSector */
#define g_LightLimit GAME_DATA(int, 0x4ae710)		/* R_LightLimit, -1: no lights chosen */
#define g_ShowModelLights GAME_DATA(int, 0x4ae6f8)	/* R_ShowModelLights */
#define g_ShowModelBox GAME_DATA(int, 0x4ae668)		/* R_ShowModelBox */
#define g_ShowModelPos GAME_DATA(int, 0x4ae650)		/* R_ShowModelPos */
#define PROF_MODELVIS 0x1e

/* the passes that draw only the models seen in the last frame */
static int Modelo_PassKeepsVis(int pass) {
	return (pass > 1 && pass < 10) || (uint32_t)(pass - 12) <= 7;
}

/* the squared distance of pos to the eye: (y y + x x) + z z */
static float Modelo_EyeDist2(const vec3_t *pos) {
	const vec3_t *eye = Render_GetEyePos();
	float dy = pos->y - eye->y;
	float dx = pos->x - eye->x;
	float dz = pos->z - eye->z;
	return dy * dy + dx * dx + dz * dz;
}

/* the box of the scene through m: its center and half its diagonal */
static float Modelo_Sphere(CModelo *self, const matrix_t *m, vec3_t *c) {
	box_t b;

	Box_Transform(&b, &self->escena->box, m);
	float dx = b.max.x - b.min.x;
	float dy = b.max.y - b.min.y;
	float dz = b.max.z - b.min.z;
	c->x = (b.max.x + b.min.x) * 0.5f;
	c->y = (b.min.y + b.max.y) * 0.5f;
	c->z = (b.min.z + b.max.z) * 0.5f;
	return (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) * 0.5f;
}

byte __thiscall CModelo_Visibility(CModelo *self, const matrix_t *m) {
	int pass = g_RenderPass;
	int sec = PROF_MODELVIS;
	vec3_t c;

	if (Modelo_PassKeepsVis(pass)) {
		return self->visible;
	}
	if (self->alwaysVisible) {
		return 3;
	}
	Prof_Begin(PROF_MODELVIS);
	if (g_ModelFastIsVis == 2 && !SectorVis_Check(&self->sector, &self->pos, self->name)) {
		goto hidden;
	}
	byte r = 3;
	if (!Box_IsVisible(m, &self->escena->box, self->name, g_SphereVisMult)) {
		/* the stencil shadow of a mesh near enough is still drawn */
		pass = g_RenderPass;
		if ((pass != 1 && pass != 11) || !self->hasMesh) {
			goto hidden;
		}
		if (Modelo_EyeDist2(&self->pos) > g_DistMStencil * g_DistMStencil) {
			goto hidden;
		}
		r = 2;
	}
	if (g_ModelPortalVis) {
		float radius = Modelo_Sphere(self, m, &c);
		if (!SectorVis_Sphere(&self->sector, &self->pos, &c, radius, self->name)) {
			goto hidden;
		}
	}
	ProfScope_End(&sec);
	return r;
hidden:
	ProfScope_End(&sec);
	return 0;
}

byte __fastcall CModelo_UpdateLOD(CModelo *self) {
	int i;
	CLod *l = NULL;

	if (!g_UseMLOD || !self->lods.p) {
		return 0;
	}
	for (i = self->lods.n; i > 0; i--) {
		l = Modelo_Lod(self, (uint32_t)(i - 1));
		CHECK(l->model);
		float d2 = Modelo_EyeDist2(&self->pos);
		float r = (float)i * g_DistMLOD1 * self->lodDist;
		if (g_DrawMLODMin >= i) {
			r = -1.0f;
		}
		if (d2 > r * r) {
			goto found;
		}
	}
	return (byte)(g_DrawMLODMask & 1);
found:
	if (g_DrawMLODMask & (1u << i)) {
		return 1;
	}
	CModelo *m = l->model;
	CHECK(!m->enable);
	Modelo_SetEnable(m, 1);
	m->pos = self->pos;
	m->rot = self->rot;
	m->pivot = self->pivot;
	m->rot2 = self->rot2;
	m->dynLight = self->dynLight;
	m->alwaysVisible = self->alwaysVisible;
	m->visible = self->visible;
	m->f10c = self->f10c;
	m->f110 = self->f110;
	/* the first two animation slots */
	memcpy(m->slots.p, self->slots.p, 2 * sizeof(CAnimSlot));
	CModelo_Render(m);
	if (m->visible && g_DrawMLODPart) {
		int n = m->nodosR->n;
		for (int j = 0; j < n; j++) {
			CNodo3D *nodo = Modelo_NodoR(m->nodosR, (uint32_t)j)->nodo;
			if (nodo->nodoR < 0) {
				continue;
			}
			CHECK(!(nodo->flags & NODO_NO_TABNODOR));
			CNodoR *mainR = Modelo_NodoR(self->nodosR, (uint32_t)nodo->nodoR);
			CNodo3D *mn = mainR->nodo;
			CHECK(mainR->frame == g_FrameCount);
			if (CNodo3D_Clase(mn) == CLASE_OBJ3D_SISTPART) {
				CNodo3D_Render(mn, g_pd3dDevice, mainR);
			}
		}
	}
	self->mRender = m->mRender;
	self->visible = m->visible;
	Modelo_SetEnable(m, 0);
	return 1;
}

/* R_ShowModelLights 2 to 4: a text and a line to each light chosen for the model */
static void Modelo_ShowLights(CModelo *self, CLuz **luces, int n, int limit) {
	const vec3_t *pos = &self->pos;

	for (int i = 0; i < n; i++) {
		CLuz *l = luces[i];
		float dy = l->d3d.Position.y - pos->y;
		float dx = l->d3d.Position.x - pos->x;
		float dz = l->d3d.Position.z - pos->z;
		float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
		if (g_ShowModelLights == 3) {
			Debug_Text3D(g_FntComic14, -200.0f, (float)i * 16.0f - 80.0f, pos, i >= limit ? 0x800000 : 0x8000,
				"%s [%.2f] [%.2f] [%s] [%.2f] [%.2f]", l->nodo->name, (double)d, (double)l->peso,
				CLuz_TipoNombre(l), (double)l->intensidad, (double)l->mult);
		}
		if (g_ShowModelLights == 4) {
			float y = (float)i * 16.0f;
			Debug_Text3D(g_FntTerm8, -200.0f, y - 90.0f, pos, 0xff, "%s", l->nodo->escena->name);
			l = luces[i];
			Debug_Text3D(g_FntTerm8, -200.0f, y - 80.0f, pos, i >= limit ? 0xff0000 : 0xff00,
				"%s [%.2f] [%.2f] [%s] [%.2f] [%.2f]", l->nodo->name, (double)d, (double)l->peso,
				CLuz_TipoNombre(l), (double)l->intensidad, (double)l->mult);
		}
		vec3_t lp = luces[i]->d3d.Position;
		Debug_DrawLine(pos, &lp, i >= limit ? 0x808000 : 0x800080);
	}
}

/* R_ShowModelBox 3: the animation slots */
static void Modelo_ShowSlots(CModelo *self) {
	float x = 40.0f, y = -80.0f;

	for (uint32_t i = 0; i < MAX_ANIM_CHANNEL; i++) {
		CAnimSlot *s = Modelo_Slot(self, i);
		CAnimGrupo *a = s->anim, *b = s->prev;
		if (!a && !b) {
			continue;
		}
		uint32_t color = i != 1 ? 0xffffff : 0xff00;
		if (i == 2) {
			color = 0xff;
		} else if (i == 3) {
			color = 0xff0000;
		}
		Debug_Text3D(g_FntComic14, x, y, &self->pos, color, "%d %d %.2f %d %d", i, a->morphCount, (double)s->blend,
			s->prevSubset, s->subset);
		Debug_Text3D(g_FntComic14, x, y + 16.0f, &self->pos, color, "%s %.2f", a->name, (double)a->ctrl.frame);
		Debug_Text3D(g_FntComic14, x, y + 32.0f, &self->pos, color, "%s %.2f", b ? b->name : "[none]",
			b ? (double)b->ctrl.frame : 0.0);
		x = x + 10.0f;
		y = y + 56.0f;
	}
}

void __fastcall CModelo_Render(CModelo *self) {
	NodoAnimCtx ctx;
	CLuz *luces[MAX_LUCES_OBJ];
	vec3_t c;

	if (self->callback) {
		self->callback(self, 0, self->cbArg, self->cbArg2);
	}
	if (!self->enable) {
		self->visible = 0;
		return;
	}
	int pass = g_RenderPass;
	if (!(self->visible & 1) && Modelo_PassKeepsVis(pass)) {
		return;
	}
	if (pass == 100 && !self->hasShadow) {
		return;
	}
	CHECK(self->escena);
	if (!self->alwaysVisible && g_ModelFastIsVis == 1) {
		byte seen = SectorVis_Check(&self->sector, &self->pos, self->name);
		if (!seen && !g_SkipUpdateVis) {
			self->visible = seen;
			if (g_ShowModelSector == 2) {
				SectorVis_Draw(&self->sector, &self->pos, 0, 1, self->name);
			}
			return;
		}
	}
	if (g_RenderPass == 2) {
		/* the shadow pass: only the lit models near enough */
		if (!self->dynLight) {
			return;
		}
		float d2 = Modelo_EyeDist2(&self->pos);
		float r = self->stencilDist * g_StencilMultFrame * 0.9f;
		if (d2 > r * r) {
			return;
		}
	}
	if (CModelo_UpdateLOD(self)) {
		return;
	}
	for (int i = 0; i < MAX_ANIM_CHANNEL; i++) {
		RenderCtx_SetCanal(&ctx.chan[i], NULL);
	}
	CModelo_SetupRender(self, &ctx);
	if (!g_SkipUpdateVis) {
		self->visible = CModelo_Visibility(self, &ctx.m);
	}
	if (!self->visible) {
		self->mRender = ctx.m;
		if (g_ShowModelSector == 2) {
			SectorVis_Draw(&self->sector, &self->pos, 0, 0, self->name);
		}
		return;
	}
	int limit = g_LightLimit;
	int nLuces = 0;
	if (limit != -1) {
		E3D_FlushBatch();
		E3D_SetShadowLight(NULL, 0);
		if (self->dynLight) {
			CHECK(self->nodosR);
			/* the box through the matrix of the last frame */
			float radius = Modelo_Sphere(self, &self->mRender, &c);
			int sector = CModelo_GetSector(self);
			nLuces = Luz_Select(&self->pos, sector, &c, radius, luces, limit);
		}
	}
	CEscena_Update(self->escena, &ctx);
	if (self->f10c) {
		ModelFX_Render(self, &ctx);
	} else {
		CEscena_Render(self->escena, &ctx);
	}
	if (limit != -1) {
		E3D_FlushBatch();
		E3D_SetShadowLight(NULL, 0);
		if (self->nodosR && g_ShowModelLights) {
			uint32_t color = nLuces > limit ? 0xff0000 : self->dynLight ? 0xff00 : 0x808080;
			Debug_Text3D(g_FntComic14, 0.0f, -100.0f, &self->pos, color, "%d", nLuces);
		}
	}
	if (self->dynLight && g_ShowModelLights > 1 && nLuces > 0) {
		Modelo_ShowLights(self, luces, nLuces, limit);
	}
	self->mRender = ctx.m;
	int showPos;
	if (self->showBox || g_ShowModelBox || g_ShowModelPos == 1) {
		CEscena *e = self->escena;
		vec3_t mn, mx, v;
		Debug_DrawBoxM(&self->mRender, &e->box.min, &e->box.max, 0x800000);
		e = self->escena;
		mx.x = e->box.max.x + self->pos.x;
		mx.y = e->box.max.y + self->pos.y;
		mx.z = e->box.max.z + self->pos.z;
		mn.x = self->pos.x + e->box.min.x;
		mn.y = e->box.min.y + self->pos.y;
		mn.z = e->box.min.z + self->pos.z;
		Debug_DrawBox(&mn, &mx, 0x80);
		Debug_DrawPoint(&self->pos, 20.0f, 0xff);
		v.x = 0.0f;
		v.y = 0.0f;
		v.z = 0.0f;
		Debug_DrawPointM(&self->mRender, &v, 20.0f, 0xff00);
		v.x = -self->pivot.x;
		v.y = -self->pivot.y;
		v.z = -self->pivot.z;
		Debug_DrawPointM(&self->mRender, &v, 20.0f, 0xff0000);
		if (g_ShowModelBox > 1) {
			Debug_Text3D(g_FntTerm8, 4.0f, -12.0f, &self->pos, 0xc0c0c0, "%s", self->name);
		}
		if (g_ShowModelPos) {
			Debug_Text3D(g_FntTerm8, 4.0f, -12.0f, &self->pos, 0xff00, "%s", self->name);
			Debug_Text3D(g_FntTerm8, 20.0f, 0.0f, &self->pos, 0xc0c0c0, "%s", Vec3_Str(&self->pos, 0, 6));
			Debug_Text3D(g_FntTerm8, 20.0f, 12.0f, &self->pos, 0xc0c0c0, "%s", Vec3_StrDeg(&self->rot, 0, 2));
		}
	}
	showPos = g_ShowModelPos;
	if (showPos > 1) {
		int draw = 1;
		if (showPos <= 2 && !self->dynLight) {
			const box_t *b = &self->escena->box;
			float dy = b->max.y - b->min.y;
			float dx = b->max.x - b->min.x;
			float dz = b->max.z - b->min.z;
			float radius = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) * 0.5f;
			draw = radius != 0.0f;
		}
		if (draw) {
			uint32_t color = 0xff00;
			if (Wild_MatchLower("*.lod1.*", self->name)) {
				color = 0xff;
			}
			if (Wild_MatchLower("*.lod2.*", self->name)) {
				color = 0xff0000;
			}
			if (Wild_MatchLower("*.lod3.*", self->name)) {
				color = 0xffff00;
			}
			Debug_Text3D(g_FntTerm8, 4.0f, -12.0f, &self->pos, color, "%s", self->name);
		}
	}
	if (g_ShowModelBox > 2) {
		Modelo_ShowSlots(self);
	}
	if (g_ShowModelSector) {
		SectorVis_Draw(&self->sector, &self->pos, 1, 0, self->name);
	}
	if (self->callback) {
		self->callback(self, 1, self->cbArg, self->cbArg2);
	}
}
