/*
 * Models of the engine3d, original source Engine/Modelo.cpp (include/scrap/model.h), with the drawing, the choice
 * of the LOD and the visibility test (through the profiler, the debug texts and the sector visibility of the
 * renderer). Not here: 0x255d00 (CHECK(0), no callers).
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/scene.h"
#include "scrap/fonts.h"
#include "scrap/light.h"
#include "scrap/material.h"
#include "scrap/model.h"
#include "scrap/prof.h"
#include "scrap/render.h"
#include "scrap/stencil.h"
#include "scrap/groundportal.h"
#include "scrap/utild3d.h"
#include "scrap/misc.h"

#define CLOD_DTOR 0x252d80
#define LIGHT_OMNI_NAME GAME_STR(str_3d598c, 0x3d598c)
#define LIGHT_SPOT_NAME GAME_STR(str_3d5994, 0x3d5994)
#define LIGHT_DIR_NAME GAME_STR(str_3d599c, 0x3d599c)
#define MORPH_SPEED_UNIT 0.01f
#define MORPH_SPEED_MAX 0.08f
#define STENCIL_DIST 4000.0f

#define g_AnimPause GAME_GLOBAL_IN(int, GVar_R_SkipAnimInterp, 16, 0x4ae638)	/* the animations stand still */
#define g_AnimSpeed GAME_GLOBAL_IN(float, GVar_R_AnimInterpMult, 16, 0x4ae770)	/* the speed of all animations */
#define g_FrameTime GAME_GLOBAL(float, g_FrameMs, 0x4a8ed4)	/* the time of the frame */

static CAnimSlot *Model_Slot(CModel *self, uint32_t i) {
	CHECK(self->slots.p && i < (uint32_t)self->slots.n);
	return &((CAnimSlot *)self->slots.p)[i];
}

static CLod *Model_Lod(CModel *self, uint32_t i) {
	CHECK(self->lods.p && i < (uint32_t)self->lods.n);
	return &((CLod *)self->lods.p)[i];
}

static CRenderNode *Model_RenderNode(CTable *t, uint32_t i) {
	CHECK(t->p && i < (uint32_t)t->n);
	return &((CRenderNode *)t->p)[i];
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
static void Model_Identity(matrix_t *m) {
	memset(m->m, 0, sizeof(m->m));
	m->m[15] = 1.0f;
	m->m[10] = 1.0f;
	m->m[5] = 1.0f;
	m->m[0] = 1.0f;
}

/* m (identity) times the pivot, the scale, the second rotation, the animation offset, the rotation, the position and
 * the local matrix */
static void Model_Transform(CModel *self, matrix_t *m) {
	matrix_t t;

	Matrix_AddPos(m, &self->pivot);
	Matrix_PostScale(m, &self->scale);
	Matrix_PostRotate(m, &self->rot2);
	Matrix_AddPos(m, &self->animOfs);
	Matrix_PostRotate(m, &self->rot);
	Matrix_AddPos(m, &self->pos);
	*m = *Matrix_MulD3D(&t, m, &self->local);
}

static void Model_AddMove(CModel *self, const vec3_t *r) {
	self->pos.x = self->pos.x + r->x;
	self->pos.y = r->y + self->pos.y;
	self->pos.z = r->z + self->pos.z;
}

static void Lods_Delete(CTable *t) {
	if (t->p) {
		Delete_Objects(t->p, sizeof(CLod), GAME_FUNC(CLod_dtor, CLOD_DTOR));
		t->p = NULL;
		t->n = 0;
	}
}

CModel *__fastcall CModel_ctor(CModel *self) {
	self->vftable = CMODEL_VFTABLE;
	self->name = NULL;
	self->slots.p = NULL;
	self->slots.n = 0;
	self->lods.p = NULL;
	self->lods.n = 0;
	CAnimGroups_ctor(&self->anims);
	self->sector.flags = 0;
	self->sector.sector = 0;
	self->sector.pos.z = 0.0f;
	self->sector.pos.y = 0.0f;
	self->sector.pos.x = 0.0f;
	String_Set(&self->name, "Modelo");
	self->scene = NULL;
	self->mainLOD = NULL;
	Table_Alloc10Z(&self->slots, MAX_ANIM_CHANNEL);
	Vec3_Zero(&self->pos);
	Vec3_Zero(&self->rot);
	Vec3_One(&self->scale);
	Vec3_Zero(&self->animOfs);
	Vec3_Zero(&self->pivot);
	Vec3_Zero(&self->rot2);
	Vec3_One(&self->scaleBase);
	Matrix_LoadIdentity(&self->local);
	self->enable = 1;
	self->visible = 0;
	self->showBox = 0;
	self->animate = 1;
	self->dynLight = 1;
	self->alwaysVisible = 0;
	self->hasMesh = 0;
	self->hasShadow = 0;
	self->renderNodes = NULL;
	Matrix_LoadIdentity(&self->mRender);
	self->callback = NULL;
	self->cbArg = NULL;
	self->cbArg2 = NULL;
	self->list = NULL;
	self->lodDist = 1.0f;
	self->renderEffect = 0;
	self->effectParam = 0;
	self->sortZBias = 0.0f;
	self->sortZMult = 1.0f;
	self->stencilDist = STENCIL_DIST;
	return self;
}

void __fastcall Table_DeleteLods(CTable *t) {
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

void __fastcall CModel_dtor(CModel *self) {
	self->vftable = CMODEL_VFTABLE;
	for (uint32_t i = 0; (int)i < self->lods.n; i++) {
		Model_Destroy(&Model_Lod(self, i)->model, 0);
	}
	CModel_SetScene(self, NULL);
	CAnimGroups_Clear(&self->anims);
	Lods_Delete(&self->lods);
	Table_Free(&self->slots);
	if (self->name) {
		String_Set(&self->name, NULL);
	}
}

/* the array branch of the vector deleting destructors: the count before the objects, the destructor on each */
static void *DeleteVecArray(void *self, uint32_t flags, uint32_t size, void *dtor) {
	uint32_t *mem = (uint32_t *)self - 1;

	crt_eh_vector_dtor(self, size, *mem, dtor);
	if (flags & 1) {
		operator_delete_array(mem);
	}
	return mem;
}

void *__thiscall CLod_DeleteVec(CLod *self, uint32_t flags) {
	if (flags & 2) {
		return DeleteVecArray(self, flags, sizeof(CLod), GAME_FUNC(CLod_dtor, CLOD_DTOR));
	}
	CLod_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CLod));
	}
	return self;
}

void *__thiscall CRenderNode_DeleteVec(CRenderNode *self, uint32_t flags) {
	if (flags & 2) {
		return DeleteVecArray(self, flags, sizeof(CRenderNode), GAME_FUNC(CRenderNode_dtor, CNODOR_DTOR));
	}
	CRenderNode_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CRenderNode));
	}
	return self;
}

CModel *__thiscall CModel_Delete(CModel *self, byte flags) {
	CModel_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

void __fastcall CModel_Disable(CModel *self) {
	Model_SetEnable(self, 0);
}

void __fastcall CModel_Enable(CModel *self) {
	Model_SetEnable(self, 1);
}

void __thiscall CModel_ReleaseD3D(CModel *self, void *dev, byte scene) {
	CHECK(self->scene);
	if (scene) {
		CScene_ReleaseD3D(self->scene, dev);
	}
}

CAnimSlot *__thiscall CModel_GetSlot(CModel *self, uint32_t i) {
	return Model_Slot(self, i);
}

double CAnim_GetSpeed(CAnimGroup *a) {
	if (a && !g_AnimPause) {
		return (double)a->ctrl.speed * (double)g_AnimSpeed;
	}
	return 0.0;
}

void __thiscall CModel_InitD3D(CModel *self, void *dev, byte scene) {
	CHECK(self->scene);
	if (scene) {
		CScene_InitD3D(self->scene, dev);
	}
}

void __thiscall CModel_SetupRender(CModel *self, NodeAnimCtx *ctx) {
	vec3_t one;

	Vec3_One(&one);
	RenderCtx_Init(ctx, self, &one);
	for (uint32_t i = 0; i < MAX_ANIM_CHANNEL; i++) {
		if (Model_Slot(self, i)->anim) {
			CAnimSlot *s = Model_Slot(self, i);

			/* RenderCtx.h */
			CHECK(i < MAX_ANIM_CHANNEL);
			RenderCtx_SetChannel(&ctx->chan[i], s);
		}
	}
	CModel_AnimOffset(self, ctx);
	Model_Identity(&ctx->m);
	Model_Transform(self, &ctx->m);
	Vec3_Zero(&self->animOfs);
}

CRenderNode *__thiscall CModel_GetRenderNode(CModel *self, CNode3D *node) {
	CHECK(0);
	return Model_RenderNode(self->renderNodes, node->slot);
}

matrix_t *__thiscall CModel_GetNodeMatrix(CModel *self, matrix_t *m, uint32_t inodo, byte anim, uint32_t slot,
	byte *visible, float *fov) {
	CNode3D *node = Model_RenderNode(self->renderNodes, inodo)->node;
	vec3_t pos = node->pos;
	quat_t q = node->rot;
	matrix_t local, t;

	if (anim) {
		CAnimGroup *a = Model_Slot(self, slot)->anim;

		if (a) {
			float f = a->ctrl.frame;
			int frame = (int)f;
			float frac = f - (float)frame;
			CAnimNode *track = CAnimation_GetNodeTable(a->Anim->AnimM3D, node->index);

			if (track) {
				CAnimNode_GetPosRot(track, &pos, &q, frame, frac);
			}
			if (visible && track) {
				CAnimNode_GetVisible(track, visible, frame, frac);
			}
			if (fov && track) {
				CAnimNode_GetFOV(track, fov, frame, frac);
			}
		}
	}
	Quat_ToMatrixD3D(m, &q);
	Matrix_AddPos(m, &pos);
	CModel_GetMatrix(self, &local);
	*m = *Matrix_MulD3D(&t, m, &local);
	return m;
}

void __thiscall CModel_SetNumLods(CModel *self, int n) {
	Table_New8(&self->lods, n);
}

void __thiscall CModel_SetLod(CModel *self, uint32_t i, CModel *model) {
	CHECK(model);
	Model_Lod(self, i)->model = model;
	Model_Lod(self, i)->f4 = 0;
	CHECK(model->scene->mainLOD);
	const char *mainLOD = model->scene->mainLOD;
	const char *name = Path_FileName(self->scene->name);
	CHECK(!crt_stricmp(Path_FileName(mainLOD), name));
	if (!self->scene->fromAmc) {
		CHECK(!strcmp(model->scene->mainLOD, self->scene->name));
	}
	CHECK(!model->mainLOD);
	model->mainLOD = self;
}

byte __thiscall CModel_SetMorphSubset(CModel *self, uint32_t slot, const char *tag) {
	CAnimGroup *a = Model_Slot(self, slot)->anim;

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

int __fastcall CModel_GetSector(CModel *self) {
	SectorVis_Update(&self->sector, &self->pos, self->name);
	return self->sector.sector;
}

CAnimGroup *__thiscall CModel_AddAnim(CModel *self, CScene *anim, const char *name) {
	CAnimGroup *a = AnimGroup_New(self->scene, anim, name);

	if (CAnimGroups_Find(&self->anims, a->name, NULL)) {
		Con_Warnf("WARN: Animation '%s' already in model '%s'\n", a->name, self->name);
	}
	CAnimGroups_Add(&self->anims, a);
	return a;
}

byte __thiscall CModel_AddAnimEvents(CModel *self, const char *name, float start, float end) {
	CAnimGroup *a = CAnimGroups_Find(&self->anims, name, NULL);
	CAnimEvent *ev, *first;

	if (!a) {
		return 0;
	}
	if (end < 0.0f) {
		end = (float)(int)((uint32_t)a->ctrl.frames - 1 + (uint32_t)a->ctrl.firstFrame);
	}
	if (start < 0.0f) {
		start = (float)a->ctrl.firstFrame;
	}
	ev = operator_new(0x14);
	if (ev) {
		CAnimEvent_ctor(ev, start, 0, 0);
	}
	first = AnimEvents_Add(&a->events, ev);
	ev = operator_new(0x14);
	if (ev) {
		CAnimEvent_ctor(ev, end, 0xc, (int)first);
	}
	AnimEvents_Add(&a->events, ev);
	return 1;
}

void __thiscall CModel_AnimPlay(CModel *self, uint32_t slot) {
	CAnimGroup *a = Model_Slot(self, slot)->anim;

	if (a) {
		CAnimCtrl_Play(&a->ctrl);
	}
}

int __thiscall CModel_AnimState(CModel *self, uint32_t slot) {
	CAnimGroup *a = Model_Slot(self, slot)->anim;

	if (a) {
		return a->ctrl.state;
	}
	return ANIM_END;
}

void __thiscall CModel_AnimStop(CModel *self, uint32_t slot) {
	CAnimGroup *a = Model_Slot(self, slot)->anim;

	if (a) {
		CAnimCtrl_Stop(&a->ctrl);
	}
}

void __thiscall CModel_AnimPause(CModel *self, uint32_t slot) {
	CAnimGroup *a = Model_Slot(self, slot)->anim;

	if (a) {
		CAnimCtrl_Pause(&a->ctrl);
	}
}

void __thiscall CModel_AnimFade(CModel *self, float t, uint32_t slot) {
	CAnimGroup *a = Model_Slot(self, slot)->anim;

	if (a) {
		CAnimCtrl_Fade(&a->ctrl, t);
	}
}

void __thiscall CModel_MoveForward(CModel *self, float d) {
	vec3_t v = { 0.0f, 0.0f, -d }, t;

	Model_AddMove(self, Vec3_Rotate(&t, &v, &self->rot));
}

void __thiscall CModel_UpdateAnims(CModel *self, float dt, int iAnim) {
	(void)dt;
	CHECK(iAnim == -1);
	for (uint32_t i = 0; (int)i < MAX_ANIM_CHANNEL; i++) {
		CAnimSlot *s = Model_Slot(self, i);
		CAnimGroup *a = s->anim, *prev = s->prev;
		byte keep = 0;

		if (a && self->animate) {
			CAnimCtrl_Update(&a->ctrl, &self->rot2, -1.0f);
			if (i == 0) {
				vec3_t t;

				Model_AddMove(self, Vec3_Rotate(&t, &a->ctrl.move, &self->rot));
			}
		}
		if (prev) {
			if (self->animate) {
				CAnimCtrl_Update(&prev->ctrl, &self->rot2, -1.0f);
			}
			if (a && !g_AnimPause) {
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

CNode3D *__thiscall CModel_FindClass(CModel *self, CNode3D *from, uint32_t class) {
	if (self->scene) {
		return CScene_FindClass(self->scene, from, class);
	}
	return NULL;
}

CNode3D *__thiscall CModel_FindNode(CModel *self, const char *name) {
	if (self->scene && self->scene->Node3D) {
		return CNode3D_Find(self->scene->Node3D, name, 1);
	}
	return NULL;
}

CRenderNode *__thiscall CModel_FindRenderNode(CModel *self, const char *name, uint32_t *idx, byte wild, byte nocase) {
	char tmp[1024], tmp2[1024];

	CHECK(self->renderNodes);
	CHECK(name);
	if (wild && nocase) {
		CHECK(strlen(name) < sizeof(tmp));
		String_Copy(tmp, name, sizeof(tmp));
		crt_strlwr(tmp);
	}
	CTable *tab = self->renderNodes;
	for (uint32_t i = 0; (int)i < tab->n; i++, tab = self->renderNodes) {
		CRenderNode *e = Model_RenderNode(tab, i);
		const char *nodeName = e->node->name;
		int eq;

		if (wild) {
			if (nocase) {
				CHECK(strlen(nodeName) < sizeof(tmp2));
				String_Copy(tmp2, e->node->name, sizeof(tmp2));
				crt_strlwr(tmp2);
				eq = Wild_Match(tmp, tmp2);
			} else {
				eq = Wild_Match(name, nodeName);
			}
		} else if (nocase) {
			eq = !crt_stricmp(name, nodeName);
		} else {
			eq = !strcmp(name, nodeName);
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

CNode3D *__thiscall CModel_FindNodeWild(CModel *self, const char *pat) {
	if (self->scene && self->scene->Node3D) {
		return CNode3D_FindWild(self->scene->Node3D, pat, 1);
	}
	return NULL;
}

void __thiscall CModel_AnimOffset(CModel *self, NodeAnimCtx *ctx) {
	CHECK(ctx);
	Vec3_Zero(&self->animOfs);
	if (ctx->chan[0].anim && ctx->chan[0].anim->anim[0]) {
		self->animOfs = ((CAnimGroup *)ctx->chan[0].anim->anim[0])->ctrl.ofs;
		CAnimGroup *p = ctx->chan[0].anim->anim[1];
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

void __thiscall CModel_GetMatrix(CModel *self, matrix_t *m) {
	Matrix_LoadIdentity(m);
	Model_Transform(self, m);
}

void __thiscall CModel_SetAnim(CModel *self, int idx, uint32_t slot) {
	CAnimGroup *a = CAnimGroups_Get(&self->anims, idx);

	if (a == Model_Slot(self, slot)->anim) {
		return;
	}
	if (a && !g_AnimPause && a->ctrl.speed * g_AnimSpeed != 0.0f) {
		CAnimSlot *s = CModel_GetSlot(self, slot);

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
		CAnimSlot *s = Model_Slot(self, slot);

		s->anim = a;
		s->prev = NULL;
		s->blend = 0.0f;
	}
	if (a) {
		CAnimCtrl_Start(&a->ctrl);
	}
}

void __thiscall CModel_SetAnimName(CModel *self, const char *name, uint32_t slot) {
	CHECK(0);
	CAnimGroup *cur = Model_Slot(self, slot)->anim;
	if (cur && name && !crt_stricmp(cur->Anim->AnimM3D->name, name)) {
		return;
	}
	CAnimGroup *a = CAnimGroups_Find(&self->anims, name, NULL);
	if (a != cur) {
		if (a) {
			CAnimCtrl_Start(&a->ctrl);
		}
		CAnimSlot *s = Model_Slot(self, slot);
		s->anim = a;
		s->prev = NULL;
		s->blend = 0.0f;
	}
}

void __thiscall CModel_SetScene(CModel *self, CScene *scn) {
	CTable *tab = self->renderNodes;

	if (tab) {
		if (tab->p) {
			Delete_Objects(tab->p, sizeof(CRenderNode), GAME_FUNC(CRenderNode_dtor, CNODOR_DTOR));
			tab->p = NULL;
			tab->n = 0;
		}
		operator_delete_sized(tab, sizeof(*tab));
		self->renderNodes = NULL;
	}
	if (scn) {
		scn->Refs++;
	}
	if (self->scene) {
		Scene_Unref(&self->scene);
	}
	self->scene = scn;
	if (!scn) {
		return;
	}
	if (scn->ini) {
		if (CIniFile_GetBool(scn->ini, "Model", "StaticLighting", 0)) {
			self->dynLight = 0;
			E3D_LogWhite(g_FileLog, "MODEL: StaticLighting: %s", self->name);
		}
		if (CIniFile_GetBool(self->scene->ini, "Model", "AlwaysVisible", 0)) {
			self->alwaysVisible = 1;
			E3D_LogWhite(g_FileLog, "MODEL: AlwaysVisible: %s", self->name);
		}
		float r = CIniFile_GetFloat(self->scene->ini, "Model", "BBoxRadius", 0.0f);
		if (r != 0.0f) {
			box_t *b = &self->scene->box;
			Vec3_Zero(&b->min);
			Vec3_Zero(&b->max);
			b = &self->scene->box;
			b->min.x = b->min.x - r;
			b->min.y = b->min.y - r;
			b->min.z = b->min.z - r;
			b->max.x = r + b->max.x;
			b->max.y = b->max.y + r;
			b->max.z = b->max.z + r;
			E3D_LogWhite(g_FileLog, "MODEL: BBoxRadius: %f [%s]", (double)r, self->name);
		}
		self->sortZBias = CIniFile_GetFloat(self->scene->ini, "Model", "SortZBias", self->sortZBias);
		self->sortZMult = CIniFile_GetFloat(self->scene->ini, "Model", "SortZMult", self->sortZMult);
		self->stencilDist = CIniFile_GetFloat(self->scene->ini, "Model", "StencilDist", self->stencilDist);
	}
	CScene *e = self->scene;
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
	for (CNode3D *n = self->scene->Node3D; n; n = n->next) {
		if ((CNode3D_Class(n) == CLASS_OBJ3D_TRIMESH || CNode3D_Class(n) == CLASS_OBJ3D_MESHD3D) && !(n->flags & 0xa8)) {
			self->hasMesh = 1;
			break;
		}
	}
	self->hasShadow = 0;
	for (CNode3D *n = self->scene->Node3D; n; n = n->next) {
		if ((n->flags >> 30 & 1) && (CNode3D_Class(n) == CLASS_OBJ3D_TRIMESH || CNode3D_Class(n) == CLASS_OBJ3D_MESHD3D ||
			CNode3D_Class(n) == CLASS_OBJ3D_PARTICLESYSTEM) && !(n->flags & 0x88)) {
			self->hasShadow = 1;
			return;
		}
	}
}

void __thiscall CModel_SetCallback(CModel *self, modelCallback_t fn, void *arg, void *arg2) {
	self->callback = fn;
	self->cbArg = arg;
	self->cbArg2 = arg2;
}

void __thiscall CModel_SetPivot(CModel *self, const vec3_t *v, byte rel, byte update) {
	if (rel) {
		CHECK(self->scene);
		if (update) {
			CScene_UpdateBox(self->scene, NULL, 1);
		}
		const box_t *b = &self->scene->box;
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

byte __thiscall CModel_SetPivotNode(CModel *self, const char *name) {
	if (self->scene && self->scene->Node3D) {
		CNode3D *n = CNode3D_Find(self->scene->Node3D, name, 1);

		if (n) {
			self->pivot.x = -n->pos.x;
			self->pivot.y = -n->pos.y;
			self->pivot.z = -n->pos.z;
			return 1;
		}
	}
	return 0;
}

void __thiscall CModel_SetSlotAnim(CModel *self, uint32_t slot, CAnimGroup *a) {
	CAnimSlot *s = Model_Slot(self, slot);

	s->anim = a;
	s->prev = NULL;
	s->blend = 0.0f;
}

/* the render nodes of the nodes with (pass 1) or without (pass 0) NODE_NO_RENDERNODE, from index k on */
static int Model_AddRenderNodes(CModel *self, int k, uint32_t pass) {
	for (CNode3D *n = self->scene->Node3D; n; n = n->next) {
		if ((n->flags >> 22 & 1) == pass) {
			CHECK(n->slot == k);
			CRenderNode *e = &((CRenderNode *)self->renderNodes->p)[k++];
			e->owner = (byte *)self;
			CRenderNode_SetNode(e, n);
			e->flags |= 2;
		}
	}
	return k;
}

void __thiscall CModel_BuildRenderNodes(CModel *self, int unused) {
	(void)unused;
	CHECK(self->scene);
	for (CNode3D *n = self->scene->Node3D; n; n = n->next) {
		if (CNode3D_Class(n) == CLASS_OBJ3D_LIGHT) {
			CLight *l = (CLight *)n->obj3D;

			/* objeto3d.h */
			if (l->class != CLASS_OBJ3D_LIGHT) {
				CheckFailed("devNULL", __FILE__, __TIMESTAMP__, __LINE__);
				l = NULL;
			}
			if (l->active) {
				Con_Warnf("WARN: Active light [%s] in model: %s\n", n->name, self->name);
			} else {
				E3D_LogVerbose(g_FileLog, "WARN: Unhidden light [%s] in model: %s", n->name, self->name);
			}
		}
	}
	int num = CScene_NumNodes(self->scene);
	CTable *t = operator_new(sizeof(CTable));
	if (t) {
		t->p = NULL;
		t->n = 0;
	}
	self->renderNodes = t;
	Table_NewA8(t, num);
	int k = Model_AddRenderNodes(self, 0, 0);
	k = Model_AddRenderNodes(self, k, 1);
	CHECK(k == self->renderNodes->n);
}

void Model_Check(CModel *m) {
	CHECK(m);
}

CAnimation *__thiscall CModel_GetSlotAnimM3D(CModel *self, uint32_t slot) {
	CAnimGroup *a = Model_Slot(self, slot)->anim;

	if (a) {
		return a->Anim->AnimM3D;
	}
	return NULL;
}

CAnimGroup *__thiscall CModel_GetAnim(CModel *self, int i) {
	return CAnimGroups_Get(&self->anims, i);
}

int __thiscall CModel_FindAnim(CModel *self, const char *name) {
	int idx = -1;

	CAnimGroups_Find(&self->anims, name, &idx);
	return idx;
}

const char *__thiscall CModel_GetAnimName(CModel *self, int i) {
	CAnimGroup *a = CAnimGroups_Get(&self->anims, i);

	if (a) {
		return a->name;
	}
	return NULL;
}

CAnimGroup *__thiscall CModel_GetSlotAnim(CModel *self, uint32_t slot) {
	return Model_Slot(self, slot)->anim;
}

float __thiscall CModel_GetSlotFrame(CModel *self, uint32_t slot) {
	CAnimGroup *a = Model_Slot(self, slot)->anim;

	if (a) {
		return a->ctrl.frame;
	}
	return 0.0f;
}

const char *__fastcall CLight_TypeName(const CLight *l) {
	switch (l->type) {
	case LIGHT_OMNI:
		return LIGHT_OMNI_NAME;
	case LIGHT_SPOT:
		return LIGHT_SPOT_NAME;
	case LIGHT_DIR:
		return LIGHT_DIR_NAME;
	}
	/* light.h */
	CHECK(0);
	return NULL;
}

/* the render variables of the models */
#define g_UseMLOD GAME_GLOBAL_IN(int, GVar_R_UseMLOD, 16, 0x4ae6e0)		/* R_UseMLOD */
#define g_DistMLOD1 GAME_GLOBAL_IN(float, GVar_R_DistMLOD1, 16, 0x4ae608)		/* R_DistMLOD1, the distance of LOD i is i times it */
#define g_DrawMLODMask GAME_GLOBAL_IN(uint32_t, GVar_R_DrawMLODMask, 16, 0x4ae680)	/* R_DrawMLODMask: the LODs drawn as the main model */
#define g_DrawMLODMin GAME_GLOBAL_IN(int, GVar_R_DrawMLODMin, 16, 0x4ae6c8)		/* R_DrawMLODMin */
#define g_DrawMLODPart GAME_GLOBAL_IN(int, GVar_R_DrawMLODPart, 16, 0x4ae5f0)		/* R_DrawMLODPart: the particles of the main model with a LOD */
#define g_ModelFastIsVis GAME_GLOBAL_IN(int, GVar_R_ModelFastIsVis, 16, 0x4ae620)	/* R_ModelFastIsVis */
#define g_ModelPortalVis GAME_GLOBAL_IN(int, GVar_R_ModelPortalVis, 16, 0x4ae6b0)	/* R_ModelPortalVis */
#define g_SphereVisMult GAME_GLOBAL_IN(float, GVar_R_SphereVisMult, 16, 0x4ae698)	/* R_SphereVisMult */
#define g_DistMStencil GAME_GLOBAL_IN(float, GVar_R_DistMStencil, 16, 0x4ae758)	/* R_DistMStencil */
#define g_SkipUpdateVis GAME_GLOBAL_IN(int, GVar_R_SkipUpdateVis, 16, 0x4ae740)	/* R_SkipUpdateVis */
#define g_ShowModelSector GAME_GLOBAL_IN(int, GVar_R_ShowModelSector, 16, 0x4ae728)	/* R_ShowModelSector */
#define g_LightLimit GAME_GLOBAL_IN(int, GVar_R_LightLimit, 16, 0x4ae710)		/* R_LightLimit, -1: no lights chosen */
#define g_ShowModelLights GAME_GLOBAL_IN(int, GVar_R_ShowModelLights, 16, 0x4ae6f8)	/* R_ShowModelLights */
#define g_ShowModelBox GAME_GLOBAL_IN(int, GVar_R_ShowModelBox, 16, 0x4ae668)		/* R_ShowModelBox */
#define g_ShowModelPos GAME_GLOBAL_IN(int, GVar_R_ShowModelPos, 16, 0x4ae650)		/* R_ShowModelPos */
#define PROF_MODELVIS 0x1e

/* the passes that draw only the models seen in the last frame */
static int Model_PassKeepsVis(int pass) {
	return (pass > 1 && pass < 10) || (uint32_t)(pass - 12) <= 7;
}

/* the squared distance of pos to the eye: (y y + x x) + z z */
static float Model_EyeDist2(const vec3_t *pos) {
	const vec3_t *eye = Render_GetEyePos();
	float dy = pos->y - eye->y;
	float dx = pos->x - eye->x;
	float dz = pos->z - eye->z;
	return dy * dy + dx * dx + dz * dz;
}

/* the box of the scene through m: its center and half its diagonal */
static float Model_Sphere(CModel *self, const matrix_t *m, vec3_t *c) {
	box_t b;

	Box_Transform(&b, &self->scene->box, m);
	float dx = b.max.x - b.min.x;
	float dy = b.max.y - b.min.y;
	float dz = b.max.z - b.min.z;
	c->x = (b.max.x + b.min.x) * 0.5f;
	c->y = (b.min.y + b.max.y) * 0.5f;
	c->z = (b.min.z + b.max.z) * 0.5f;
	return (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) * 0.5f;
}

byte __thiscall CModel_Visibility(CModel *self, const matrix_t *m) {
	int pass = g_RenderPass;
	int sec = PROF_MODELVIS;
	vec3_t c;

	if (Model_PassKeepsVis(pass)) {
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
	if (!Box_IsVisible(m, &self->scene->box, self->name, g_SphereVisMult)) {
		/* the stencil shadow of a mesh near enough is still drawn */
		pass = g_RenderPass;
		if ((pass != 1 && pass != 11) || !self->hasMesh) {
			goto hidden;
		}
		if (Model_EyeDist2(&self->pos) > g_DistMStencil * g_DistMStencil) {
			goto hidden;
		}
		r = 2;
	}
	if (g_ModelPortalVis) {
		float radius = Model_Sphere(self, m, &c);
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

byte __fastcall CModel_UpdateLOD(CModel *self) {
	int i;
	CLod *l = NULL;

	if (!g_UseMLOD || !self->lods.p) {
		return 0;
	}
	for (i = self->lods.n; i > 0; i--) {
		l = Model_Lod(self, (uint32_t)(i - 1));
		CHECK(l->model);
		float d2 = Model_EyeDist2(&self->pos);
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
	CModel *m = l->model;
	CHECK(!m->enable);
	Model_SetEnable(m, 1);
	m->pos = self->pos;
	m->rot = self->rot;
	m->pivot = self->pivot;
	m->rot2 = self->rot2;
	m->dynLight = self->dynLight;
	m->alwaysVisible = self->alwaysVisible;
	m->visible = self->visible;
	m->renderEffect = self->renderEffect;
	m->effectParam = self->effectParam;
	/* the first two animation slots */
	memcpy(m->slots.p, self->slots.p, 2 * sizeof(CAnimSlot));
	CModel_Render(m);
	if (m->visible && g_DrawMLODPart) {
		int n = m->renderNodes->n;
		for (int j = 0; j < n; j++) {
			CNode3D *node = Model_RenderNode(m->renderNodes, (uint32_t)j)->node;
			if (node->renderNode < 0) {
				continue;
			}
			CHECK(!(node->flags & NODE_NO_RENDERNODE));
			CRenderNode *mainR = Model_RenderNode(self->renderNodes, (uint32_t)node->renderNode);
			CNode3D *mn = mainR->node;
			CHECK(mainR->frame == g_FrameCount);
			if (CNode3D_Class(mn) == CLASS_OBJ3D_PARTICLESYSTEM) {
				CNode3D_Render(mn, g_pd3dDevice, mainR);
			}
		}
	}
	self->mRender = m->mRender;
	self->visible = m->visible;
	Model_SetEnable(m, 0);
	return 1;
}

/* R_ShowModelLights 2 to 4: a text and a line to each light chosen for the model */
static void Model_ShowLights(CModel *self, CLight **lights, int n, int limit) {
	const vec3_t *pos = &self->pos;

	for (int i = 0; i < n; i++) {
		CLight *l = lights[i];
		float dy = l->d3d.Position.y - pos->y;
		float dx = l->d3d.Position.x - pos->x;
		float dz = l->d3d.Position.z - pos->z;
		float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
		if (g_ShowModelLights == 3) {
			Debug_Text3D(g_FntComic14, -200.0f, (float)i * 16.0f - 80.0f, pos, i >= limit ? 0x800000 : 0x8000,
				"%s [%.2f] [%.2f] [%s] [%.2f] [%.2f]", l->node->name, (double)d, (double)l->weight,
				CLight_TypeName(l), (double)l->intensity, (double)l->mult);
		}
		if (g_ShowModelLights == 4) {
			float y = (float)i * 16.0f;
			Debug_Text3D(g_FntTerm8, -200.0f, y - 90.0f, pos, 0xff, "%s", l->node->scene->name);
			l = lights[i];
			Debug_Text3D(g_FntTerm8, -200.0f, y - 80.0f, pos, i >= limit ? 0xff0000 : 0xff00,
				"%s [%.2f] [%.2f] [%s] [%.2f] [%.2f]", l->node->name, (double)d, (double)l->weight,
				CLight_TypeName(l), (double)l->intensity, (double)l->mult);
		}
		vec3_t lp = lights[i]->d3d.Position;
		Debug_DrawLine(pos, &lp, i >= limit ? 0x808000 : 0x800080);
	}
}

/* R_ShowModelBox 3: the animation slots */
static void Model_ShowSlots(CModel *self) {
	float x = 40.0f, y = -80.0f;

	for (uint32_t i = 0; i < MAX_ANIM_CHANNEL; i++) {
		CAnimSlot *s = Model_Slot(self, i);
		CAnimGroup *a = s->anim, *b = s->prev;
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

void __fastcall CModel_Render(CModel *self) {
	NodeAnimCtx ctx;
	CLight *lights[MAX_LIGHTS_OBJ];
	vec3_t c;

	if (self->callback) {
		self->callback(self, 0, self->cbArg, self->cbArg2);
	}
	if (!self->enable) {
		self->visible = 0;
		return;
	}
	int pass = g_RenderPass;
	if (!(self->visible & 1) && Model_PassKeepsVis(pass)) {
		return;
	}
	if (pass == 100 && !self->hasShadow) {
		return;
	}
	CHECK(self->scene);
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
		float d2 = Model_EyeDist2(&self->pos);
		float r = self->stencilDist * g_StencilMultFrame * 0.9f;
		if (d2 > r * r) {
			return;
		}
	}
	if (CModel_UpdateLOD(self)) {
		return;
	}
	for (int i = 0; i < MAX_ANIM_CHANNEL; i++) {
		RenderCtx_SetChannel(&ctx.chan[i], NULL);
	}
	CModel_SetupRender(self, &ctx);
	if (!g_SkipUpdateVis) {
		self->visible = CModel_Visibility(self, &ctx.m);
	}
	if (!self->visible) {
		self->mRender = ctx.m;
		if (g_ShowModelSector == 2) {
			SectorVis_Draw(&self->sector, &self->pos, 0, 0, self->name);
		}
		return;
	}
	int limit = g_LightLimit;
	int nLights = 0;
	if (limit != -1) {
		E3D_FlushBatch();
		E3D_SetShadowLight(NULL, 0);
		if (self->dynLight) {
			CHECK(self->renderNodes);
			/* the box through the matrix of the last frame */
			float radius = Model_Sphere(self, &self->mRender, &c);
			int sector = CModel_GetSector(self);
			nLights = Light_Select(&self->pos, sector, &c, radius, lights, limit);
		}
	}
	CScene_Update(self->scene, &ctx);
	if (self->renderEffect) {
		ModelFX_Render(self, &ctx);
	} else {
		CScene_Render(self->scene, &ctx);
	}
	if (limit != -1) {
		E3D_FlushBatch();
		E3D_SetShadowLight(NULL, 0);
		if (self->renderNodes && g_ShowModelLights) {
			uint32_t color = nLights > limit ? 0xff0000 : self->dynLight ? 0xff00 : 0x808080;
			Debug_Text3D(g_FntComic14, 0.0f, -100.0f, &self->pos, color, "%d", nLights);
		}
	}
	if (self->dynLight && g_ShowModelLights > 1 && nLights > 0) {
		Model_ShowLights(self, lights, nLights, limit);
	}
	self->mRender = ctx.m;
	int showPos;
	if (self->showBox || g_ShowModelBox || g_ShowModelPos == 1) {
		CScene *e = self->scene;
		vec3_t mn, mx, v;
		Debug_DrawBoxM(&self->mRender, &e->box.min, &e->box.max, 0x800000);
		e = self->scene;
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
			const box_t *b = &self->scene->box;
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
		Model_ShowSlots(self);
	}
	if (g_ShowModelSector) {
		SectorVis_Draw(&self->sector, &self->pos, 1, 0, self->name);
	}
	if (self->callback) {
		self->callback(self, 1, self->cbArg, self->cbArg2);
	}
}
