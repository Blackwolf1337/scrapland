/*
 * The scene and model lists of the engine3d, original source Engine/util_escena.cpp (include/scrap/model.h):
 * crear_escena and crear_modelo with the shared scenes, gModelosRender and the list dump.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/readm3d.h"
#include "scrap/fonts.h"
#include "scrap/material.h"
#include "scrap/model.h"
#include "scrap/misc.h"

#define g_SortModels GAME_GLOBAL_IN(int, GVar_R_SortModels, 16)	/* R_SortModels */
#define g_SortShow GAME_GLOBAL_IN(int, GVar_R_SortShow, 16)	/* R_SortShow */
#define g_SortInvert GAME_GLOBAL_IN(int, GVar_R_SortInvert, 16)	/* R_SortInvert */

#define SORT_TEXT_X 200.0f
#define SORT_TEXT_Y 10.0f
#define SORT_TEXT_COLOR 0xff00

static void SceneList_Free(CSceneList *le) {
	if (le->scene) {
		Scene_Unref(&le->scene);
	}
	if (le->name2) {
		String_Set(&le->name2, NULL);
	}
	if (le->name) {
		String_Set(&le->name, NULL);
	}
	operator_delete_sized(le, sizeof(*le));
}

CSceneList *CSceneList_ctor(CSceneList *self, CScene *scn, const char *name, const char *name2,
	const vec3_t *scale, uint32_t flags, CSceneList **list) {
	self->name = NULL;
	self->name2 = NULL;
	String_Set(&self->name, name);
	String_Set(&self->name2, name2);
	self->scale = *scale;
	self->flags = flags;
	self->scene = scn;
	self->next = NULL;
	if (scn) {
		scn->Refs++;
	}
	CHECK(list);
	while (*list) {
		list = &(*list)->next;
	}
	*list = self;
	return self;
}

CModelList *CModelList_ctor(CModelList *self, CModel *m, const char *name, const char *name2,
	uint32_t flags, CModelList **list) {
	self->name = NULL;
	self->name2 = NULL;
	String_Set(&self->name, name);
	String_Set(&self->name2, name2);
	self->flags = flags;
	self->model = m;
	self->next = NULL;
	CHECK(list);
	while (*list) {
		list = &(*list)->next;
	}
	*list = self;
	self->sortKey = 0.0f;
	self->renderIndex = -1;
	m->list = self;
	Model_UpdateRender(m);
	return self;
}

void CModelList_dtor(CModelList *self) {
	CHECK(self->flags & SCENE_AUTOINIT);
	CHECK(self->model);
	CModel *m = self->model;
	if (m->enable) {
		m->enable = 0;
		m->visible = 0;
		Model_UpdateRender(m);
	}
	if (self->model) {
		((modelDelete_t)((void **)self->model->vftable)[0])(self->model, 1);
		self->model = NULL;
	}
	if (self->name2) {
		String_Set(&self->name2, NULL);
	}
	if (self->name) {
		String_Set(&self->name, NULL);
	}
}

void Model_UpdateRender(CModel *m) {
	CModelList *lm = m->list;

	if (!lm) {
		return;
	}
	int idx = lm->renderIndex;
	if (m->enable) {
		if (idx != -1) {
			return;
		}
		CHECK(g_NumModelsRender < MAX_MODELS_RENDER);
		int n = g_NumModelsRender;
		g_ModelsRender[n] = lm;
		lm->renderIndex = n;
		n++;
		g_NumModelsRender = n;
		g_MaxModelsRender = g_MaxModelsRender > n ? g_MaxModelsRender : n;
		return;
	}
	if (idx == -1) {
		return;
	}
	CHECK(idx < MAX_MODELS_RENDER);
	CHECK(g_NumModelsRender <= MAX_MODELS_RENDER);
	CHECK(g_ModelsRender[idx] == lm);
	CHECK(lm->model == m);
	int n = g_NumModelsRender;
	g_NumModelsRender = n - 1;
	CModelList *last = g_ModelsRender[n - 1];
	g_ModelsRender[idx] = last;
	last->renderIndex = idx;
	lm->renderIndex = -1;
}

byte SceneList_Release(CScene *scn, byte force) {
	CSceneList *le = g_SceneList;

	if (!scn || !le) {
		return 0;
	}
	CHECK(scn->Refs > 1);
	if (force) {
		if (scn->Refs != 2) {
			Con_Warnf("WARN: Trying to release a nonzero reference scene: %s\n", scn->name);
			return 1;
		}
	} else if (scn->Refs != 2) {
		return 0;
	}
	if (le->scene == scn) {
		if ((le->flags & SCENE_SHARED) && !force) {
			return 0;
		}
		g_SceneList = le->next;
	} else {
		for (;;) {
			if (!le->next) {
				return 0;
			}
			if (le->next->scene == scn) {
				break;
			}
			le = le->next;
		}
		CSceneList **slot = &le->next;
		le = *slot;
		if ((le->flags & SCENE_SHARED) && !force) {
			return 0;
		}
		*slot = le->next;
	}
	if (!le) {
		return 0;
	}
	SceneList_Free(le);
	return 1;
}

void Model_Destroy(CModel **pm, byte force) {
	CHECK(pm);
	if (!*pm) {
		return;
	}
	if (g_SceneList) {
		CScene *scn = (*pm)->scene;

		CHECK(scn);
		CHECK(scn->Refs > 1);
		SceneList_Release(scn, force);
	}
	CModelList *lm = g_ModelList;
	if (lm) {
		CModel *m = *pm;
		CModelList *found = NULL;

		if (lm->model == m) {
			g_ModelList = lm->next;
			found = lm;
		} else {
			while (lm->next) {
				if (lm->next->model == m) {
					CModelList **slot = &lm->next;
					found = *slot;
					*slot = found->next;
					break;
				}
				lm = lm->next;
			}
		}
		if (found) {
			CModelList_dtor(found);
			operator_delete_sized(found, sizeof(*found));
			*pm = NULL;
			return;
		}
	}
	if (*pm) {
		((modelDelete_t)((void **)(*pm)->vftable)[0])(*pm, 1);
		*pm = NULL;
	}
}

void Model_SetEnable(CModel *m, byte on) {
	if (m->enable != on) {
		m->enable = on;
		m->visible = 0;
		Model_UpdateRender(m);
	}
}

/* the CHECKs on an entry of gModelosRender */
static CModelList *Models_Render_Get(int i) {
	CModelList *lm = g_ModelsRender[i];

	CHECK(lm->flags & SCENE_AUTOINIT);
	CHECK(lm->renderIndex == i);
	CHECK(lm->model->enable);
	return lm;
}

void Models_UpdateAnims(float dt) {
	for (int i = 0; i < g_NumModelsRender; i++) {
		CModel_UpdateAnims(Models_Render_Get(i)->model, dt, -1);
	}
}

void UtilScene_FreeAll(void) {
	if (g_ModelList) {
		Con_Warnf("WARN: Model list not empty.\n");
	}
	for (CSceneList *le = g_SceneList; le;) {
		CSceneList *next = le->next;

		CHECK(le->scene->Refs == 1);
		SceneList_Free(le);
		le = next;
	}
	for (CModelList *lm = g_ModelList; lm;) {
		CModelList *next = lm->next;

		CModelList_dtor(lm);
		operator_delete_sized(lm, sizeof(*lm));
		lm = next;
	}
	g_ModelList = NULL;
	g_SceneList = NULL;
}

void UtilScene_ReleaseD3D(void) {
	void *dev = g_Engine->device;

	CHECK((g_pApp->flags >> 2 & 1) || dev);
	for (CSceneList *le = g_SceneList; le; le = le->next) {
		if (le->flags & SCENE_AUTOINIT) {
			CScene_ReleaseD3D(le->scene, dev);
		}
	}
	for (CModelList *lm = g_ModelList; lm; lm = lm->next) {
		CHECK(lm->flags & SCENE_AUTOINIT);
		CModel_ReleaseD3D(lm->model, dev, 0);
	}
}

void UtilScene_InitD3D(void) {
	void *dev = g_Engine->device;

	CHECK(dev);
	for (CModelList *lm = g_ModelList; lm; lm = lm->next) {
		CHECK(lm->flags & SCENE_AUTOINIT);
		CModel_InitD3D(lm->model, dev, 0);
	}
	for (CSceneList *le = g_SceneList; le; le = le->next) {
		if (le->flags & SCENE_AUTOINIT) {
			CScene_InitD3D(le->scene, dev);
		}
	}
}

static const char *Dump_Flag(uint32_t flags, uint32_t bit, const char *text) {
	return flags & bit ? text : "";
}

static void Dump_Model(utilPrintf_t fn, CModelList *lm) {
	uint32_t f = lm->flags;
	CModel *m = lm->model;

	fn("\n  '%s' (%08Xh %08Xh)%s%s%s", lm->name, m, m->scene, Dump_Flag(f, SCENE_SHARED, "  COMPARTIR"),
		Dump_Flag(f, SCENE_AUTOINIT, "  AUTOINIC"), Dump_Flag(f, SCENE_USEANIM, "  USARANIM"));
	fn("   Render: %d  Vis: %d", lm->model->enable, lm->model->visible);
}

void UtilScene_Dump(utilPrintf_t fn) {
	int nModels = 0, nRender = 0, nVis = 0, nScenes = 0, nAnim = 0, nVisRender = 0;

	if (!fn) {
		fn = E3D_DebugPrintf;
	}
	fn("\nModelos:");
	for (CModelList *lm = g_ModelList; lm; lm = lm->next) {
		Dump_Model(fn, lm);
		nModels++;
		if (lm->model->enable) {
			nRender++;
		}
		if (lm->model->visible & 1) {
			nVis++;
		}
	}
	fn("\n\nEscenas:");
	for (CSceneList *le = g_SceneList; le; le = le->next) {
		uint32_t f = le->flags;
		CScene *e = le->scene;

		fn("\n  '%s' (%08Xh)%s%s%s%s%s", le->name, e, Dump_Flag(f, SCENE_SHARED, "  COMPARTIR"),
			Dump_Flag(f, SCENE_AUTOINIT, "  AUTOINIC"), Dump_Flag(f, SCENE_USEANIM, "  USARANIM"),
			Dump_Flag(f, SCENE_IGNORE_SCALE, "  IGN_ESCALA"), e->AnimM3D ? "  [AnimM3D]" : "");
		fn("   escala: %s", Vec3_Str(&le->scale, 0, 6));
		nScenes++;
		if (le->scene->AnimM3D && !le->scene->Node3D) {
			nAnim++;
		}
	}
	fn("\n\nModelos activos:");
	for (int i = 0; i < g_NumModelsRender; i++) {
		CModelList *lm = g_ModelsRender[i];

		Dump_Model(fn, lm);
		if (lm->model->visible & 1) {
			nVisRender++;
		}
	}
	fn("\n\n%d modelos (%d render, %d vis), %d escenas (%d anim)\n\n", nModels, nRender, nVis, nScenes, nAnim);
	CHECK(nRender == g_NumModelsRender);
	CHECK(nVis == nVisRender);
}

void Scenes_Msg(int msg, int a, int b, int c, int d) {
	for (CSceneList *le = g_SceneList; le; le = le->next) {
		if (le->flags & SCENE_AUTOINIT) {
			CScene_Msg(le->scene, msg, a, b, c, d);
		}
	}
}

CScene *Scene_Next(CScene *scn) {
	CSceneList *le = g_SceneList;

	if (scn) {
		for (;;) {
			if (!le) {
				return NULL;
			}
			CScene *e = le->scene;
			le = le->next;
			if (e == scn) {
				break;
			}
		}
	}
	if (!le) {
		return NULL;
	}
	return le->scene;
}

void Models_Render(void) {
	if (!g_ModelsOn) {
		return;
	}
	vec3_t cam = *Render_GetEyePos();
	int n = g_NumModelsRender;
	for (int i = 0; i < n; i++) {
		CModelList *lm = Models_Render_Get(i);
		CModel *m = lm->model;
		float dy = m->pos.y - cam.y;
		float dx = m->pos.x - cam.x;
		float dz = m->pos.z - cam.z;
		float key = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) * m->sortZMult + m->sortZBias;

		lm->sortKey = key;
		int pass = g_RenderPass;
		if (pass == 1 || pass == 2 || pass == 0xb || pass == 0xc) {
			lm->sortKey = -key;
		}
		if (g_SortInvert) {
			lm->sortKey = -lm->sortKey;
		}
	}
	if (g_SortModels) {
		crt_qsort(g_ModelsRender, n, sizeof(CModelList *), ModelList_CompareKey);
	}
	for (int i = 0; i < g_NumModelsRender; i++) {
		g_ModelsRender[i]->renderIndex = i;
		CModel_Render(g_ModelsRender[i]->model);
	}
	int pass = g_RenderPass;
	if (!g_SortShow || (pass != 0 && pass != 3 && pass != 0xd)) {
		return;
	}
	int y = 0x34;
	n = g_NumModelsRender;
	for (int k = 0; k < n; k++, y += 4) {
		CModelList *lm = g_ModelsRender[n - k - 1];
		CModel *m = lm->model;
		byte vis = m->visible;

		if (!vis && g_SortShow < 2) {
			continue;
		}
		Font_SetColor(g_FntTerm8, vis & 1 ? 0xffffff : vis ? 0xffff00 : 0xff);
		Font_Printf(g_FntTerm8, SORT_TEXT_X, (float)y + SORT_TEXT_Y, 2, "%s %.2f", m->name, (double)lm->sortKey);
		if (m->sortZBias != 0.0f || m->sortZMult != 1.0f) {
			Font_SetColor(g_FntTerm8, SORT_TEXT_COLOR);
			CFont3 *f = g_FntTerm8;
			Font_Printf(f, f->x, f->y, FNT_PRINT_COLOR, " %.2f %.2f", (double)m->sortZBias, (double)m->sortZMult);
		}
		n = g_NumModelsRender;
		y += 6;
	}
}

CSceneList *CSceneList_Find(CSceneList *self, const char *name, const char *name2,
	const vec3_t *scale, uint32_t flags, uint32_t mask) {
	for (CSceneList *le = self; le; le = le->next) {
		if ((le->flags & mask) == (flags & mask) && String_EqualNoCase(le->name, name) &&
			String_EqualNoCase(le->name2, name2) && ((flags & SCENE_IGNORE_SCALE) || (le->scale.x == scale->x &&
			le->scale.y == scale->y && le->scale.z == scale->z))) {
			return le;
		}
	}
	return NULL;
}

CModelList *CModelList_Find(CModelList *self, const char *name, const char *name2, uint32_t flags,
	uint32_t mask) {
	for (CModelList *lm = self; lm; lm = lm->next) {
		if ((lm->flags & mask) == (flags & mask) && String_EqualNoCase(lm->name, name) &&
			String_EqualNoCase(lm->name2, name2)) {
			return lm;
		}
	}
	return NULL;
}

CModel *Model_FindName(const char *name) {
	for (CModelList *lm = g_ModelList; lm; lm = lm->next) {
		CHECK(lm->model);
		const char *n = lm->model->name;
		if (n && !strcmp(name, n)) {
			return lm->model;
		}
	}
	return NULL;
}

int String_EqualNoCase(const char *a, const char *b) {
	if (a) {
		if (b) {
			return !crt_stricmp(a, b);
		}
		if (*a) {
			return 0;
		}
	}
	return !b || !*b;
}

void Create_Animation(CModel *m, const char *name, uint32_t flags) {
	CScene *scn = m ? m->scene : NULL;
	byte shared;

	CHECK(scn);
	const char *name2 = scn->node;
	E3D_DebugPrintf("\ncrear_animacion(): '%s'", name);
	g_SceneAnimParent = scn;
	CScene *anim = Create_Scene(&shared, name, name2, &m->scaleBase, scn->vertWeldThreshold, flags);
	g_SceneAnimParent = NULL;
	CHECK(anim && anim->AnimM3D);
	CModel_AddAnim(m, anim, NULL);
}

CScene *Create_Scene(byte *shared, const char *name, const char *name2, const vec3_t *scale, float threshold,
	uint32_t flags) {
	CScene *scn;
	void *p;

	CHECK(name && shared);
	uint32_t isShared = flags & SCENE_SHARED;
	if (isShared && g_SceneList) {
		CSceneList *le = CSceneList_Find(g_SceneList, name, name2, scale, flags, SCENE_SHARED);

		if (le) {
			CHECK(le->scene);
			scn = le->scene;
			*shared = 1;
			if (scn) {
				goto made;
			}
		}
	}
	if ((flags & SCENE_SM3) && (scn = SM3_Load(name)) != NULL) {
		*shared = 0;
		E3D_DebugPrintf("  [sm3]");
		p = operator_new(sizeof(CSceneList));
		if (p) {
			CSceneList_ctor(p, scn, name, name2, scale, flags, &g_SceneList);
		}
		if (!g_Engine->device) {
			return scn;
		}
		CHECK(flags & SCENE_AUTOINIT);
		goto initD3D;
	}
	scn = Scene_Load(name, 1, name2);
	*shared = 0;
made:
	E3D_DebugPrintf("  [%s]", *shared ? "compartido" : "creado");
	if (!*shared) {
		p = operator_new(sizeof(CSceneList));
		if (p) {
			CSceneList_ctor(p, scn, name, name2, scale, flags, &g_SceneList);
		}
		scn->vertWeldThreshold = threshold;
		CScene_SetScale(scn, scale);
		CScene_UpdateBox(scn, 0, 1);
		if (isShared && !g_SceneAnimParent) {
			CScene_Shared(scn);
		}
	} else {
		CHECK(scn->vertWeldThreshold == threshold);
	}
	if (*shared) {
		return scn;
	}
	uint32_t autoinit = flags & SCENE_AUTOINIT;
	if (autoinit) {
		CScene_Msg(scn, 1, 0, 0, 0, 0);
		CScene_Msg(scn, 2, 0, 0, 0, 0);
		if (flags & SCENE_F20) {
			SM3_Save(name, scn);
		}
	}
	if (*shared || !g_Engine->device) {
		return scn;
	}
	CHECK(autoinit);
initD3D:
	CScene_InitD3D(scn, g_Engine->device);
	CMatList_Prepare(g_MatList, g_Engine->device);
	return scn;
}

CModel *Create_Model(const char *name, const char *name2, const vec3_t *scale, float threshold, uint32_t flags) {
	CModel *m = operator_new(sizeof(CModel));
	byte shared;

	m = m ? CModel_ctor(m) : NULL;
	E3D_DebugPrintf("\ncrear_modelo(): '%s'", name);
	CScene *scn = Create_Scene(&shared, name, name2, scale, threshold, flags);
	String_Set(&m->name, Path_FileName(scn->name));
	m->scaleBase = *scale;
	CHECK(scn->Node3D);
	CModel_SetScene(m, scn);
	if ((flags & SCENE_USEANIM) && scn->AnimM3D) {
		CModel_AddAnim(m, scn, NULL);
	}
	uint32_t autoinit = flags & SCENE_AUTOINIT;
	if (autoinit) {
		void *p = operator_new(sizeof(CModelList));

		if (p) {
			CModelList_ctor(p, m, name, name2, flags, &g_ModelList);
		}
	}
	if (g_Engine->device && autoinit) {
		CModel_InitD3D(m, g_Engine->device, 0);
	}
	return m;
}

CModel *Create_Model3(const char *name, const char *name2, const vec3_t *scale, uint32_t flags) {
	return Create_Model(name, name2, scale, -1.0f, flags);
}

CModel *Create_ModelLOD(const char *name, const char *name2, const vec3_t *scale, CModel *main, uint32_t flags) {
	g_SceneLODParent = main->scene;
	CModel *m = Create_Model(name, name2, scale, -1.0f, flags);
	g_SceneLODParent = NULL;
	CModel_Disable(m);
	return m;
}

CModel *Create_Model2(const char *name, const char *name2, uint32_t flags) {
	vec3_t one = { 1.0f, 1.0f, 1.0f };

	return Create_Model(name, name2, &one, -1.0f, flags);
}

int ModelList_CompareKey(const void *a, const void *b) {
	const CModelList *la = *(CModelList *const *)a;
	const CModelList *lb = *(CModelList *const *)b;

	return lb->sortKey > la->sortKey ? 1 : -1;
}
