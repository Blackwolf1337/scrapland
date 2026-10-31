/*
 * Engine/escena.cpp of the engine3d: CScene, see include/scrap/scene.h.
 */
#include <string.h>

#include "scrap/animation.h"
#include "scrap/camera.h"
#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/scene.h"
#include "scrap/graphic3d.h"
#include "scrap/light.h"
#include "scrap/meshd3d.h"
#include "scrap/trimesh.h"
#include "scrap/nodefx.h"
#include "scrap/portal.h"
#include "scrap/material.h"
#include "scrap/model.h"
#include "scrap/ground.h"
#include "scrap/misc.h"
#include "scrap/win32.h"

/* the time since t0 in seconds, for the debug output */
static double Scene_Seconds(uint32_t t0) {
	return (double)(uint32_t)(exe_timeGetTime() - t0) / 1000.0;
}

CScene *__fastcall CScene_ctor(CScene *self) {
	self->name = NULL;
	self->Refs = 0;
	self->vftable = CSCENE_VFTABLE;
	self->box = (box_t){ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	self->node = NULL;
	self->mainLOD = NULL;
	self->mats.p = NULL;
	self->mats.n = 0;
	self->ambient.rgba = 0xb0b0b;
	self->ambient.intens = 1.0f;
	self->fog.rgba = 0;
	self->fog.intens = 1.0f;
	self->Node3D = NULL;
	self->anim = NULL;
	self->AnimM3D = NULL;
	self->vertWeldThreshold = -1.0f;
	self->ini = NULL;
	self->userProp = NULL;
	self->CollideMesh = NULL;
	self->fromAmc = 0;
	return self;
}

static void Scene_DeleteIni(CIniFile **ini) {
	if (*ini) {
		CIniFile_dtor(*ini);
		operator_delete_sized(*ini, 4);
		*ini = NULL;
	}
}

void __fastcall CScene_dtor(CScene *self) {
	self->vftable = CSCENE_VFTABLE;
	E3D_DebugPrintf("\n> Escena::~Escena(): '%s' (%08Xh)", self->name, self);
	self->anim = NULL;
	E3D_LogDebug2(g_FileLog, " Scene deleted: %s", self->name);
	void *dev = g_pd3dDevice;
	if (dev) {
		CScene_ReleaseD3D(self, dev);
	}
	for (int i = 0; i < self->mats.n; i++) {
		TABLE_CHECK(self->mats.p, i, self->mats.n);
		CMatList_Delete(g_MatList, ((CMaterial **)self->mats.p)[i]);
		TABLE_CHECK(self->mats.p, i, self->mats.n);
		((CMaterial **)self->mats.p)[i] = NULL;
	}
	CScene_DeleteNodes(self);
	if (self->AnimM3D) {
		CAnimation *a = self->AnimM3D;
		CAnimation_dtor(a);
		operator_delete_sized(a, sizeof(CAnimation));
		self->AnimM3D = NULL;
	}
	Scene_DeleteIni(&self->ini);
	Scene_DeleteIni(&self->userProp);
	if (self->CollideMesh) {
		OBJ3D_CALL(self->CollideMesh, OBJ3D_VF_DELETE, objDelete_t, 1);
		self->CollideMesh = NULL;
	}
	E3D_DebugPrintf("\n< Escena::~Escena(): '%s' (%08Xh)", self->name, self);
	Table_Free(&self->mats);
	if (self->mainLOD) {
		String_Set(&self->mainLOD, NULL);
	}
	if (self->node) {
		String_Set(&self->node, NULL);
	}
	CHECK(!self->Refs);
	if (self->name) {
		String_Set(&self->name, NULL);
	}
}

void __thiscall CScene_Shared(CScene *self) {
	(void)self;
}

CScene *__thiscall CScene_Delete(CScene *self, byte flags) {
	CScene_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CScene));
	}
	return self;
}

void __thiscall CScene_Msg(CScene *self, int msg, int a, int b, int c, int d) {
	(void)c;
	(void)d;
	switch (msg) {
	case SCENE_MSG_JOIN:
		if (a || b) {
			CScene_Join(self, b != 0, NULL);
		}
		break;
	case SCENE_MSG_CONVERTD3D:
		CScene_ConvertD3D(self, (0.0f > self->vertWeldThreshold ? 0 : 8) | (uint32_t)a | 0x12);
		break;
	case SCENE_MSG_CALCWEIGHTS:
		CScene_CalcWeights(self);
		break;
	case SCENE_MSG_CAMERAS: {
		float fa, fb;
		memcpy(&fa, &a, 4);
		memcpy(&fb, &b, 4);
		for (CNode3D *n = NULL;;) {
			n = CScene_FindClass(self, n, CLASS_OBJ3D_CAMERA);
			if (!n) {
				break;
			}
			if (fa != 0.0f) {
				CCamera *cam = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_CAMERA);
				cam->zNear = fa;
			}
			if (fb != 0.0f) {
				CCamera *cam = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_CAMERA);
				cam->zFar = fb;
			}
		}
		break;
	}
	}
}

void __thiscall CScene_ReleaseD3D(CScene *self, void *dev) {
	for (CNode3D *n = self->Node3D; n; n = n->next) {
		if (n->obj3D) {
			OBJ3D_CALL(n->obj3D, OBJ3D_VF_RELEASED3D, objDev_t, dev);
		}
	}
}

void __thiscall CScene_InitD3D(CScene *self, void *dev) {
	for (CNode3D *n = self->Node3D; n; n = n->next) {
		if (n->obj3D) {
			OBJ3D_CALL(n->obj3D, OBJ3D_VF_INITD3D, objDev_t, dev);
		}
	}
}

/* the object of a node of the class read from a "SCN" block, NULL for none (CHECK on an unknown class) */
static IObject3D *Scene_NewObj(CNode3D *node, uint32_t class, byte *set) {
	void *p;

	*set = 1;
	switch (class) {
	case CLASS_OBJ3D_GROUND:
		p = operator_new(sizeof(CGround));
		return p ? (IObject3D *)CGround_ctor(p) : NULL;
	case CLASS_OBJ3D_CAMERA:
		p = operator_new(sizeof(CCamera));
		return p ? (IObject3D *)CCamera_ctor(p) : NULL;
	case CLASS_OBJ3D_TRIMESH:
		p = operator_new(sizeof(CTriMesh));
		return p ? (IObject3D *)CTriMesh_ctor(p) : NULL;
	case CLASS_OBJ3D_MESHD3D:
		p = operator_new(sizeof(CMeshD3D));
		return p ? (IObject3D *)CMeshD3D_ctor(p) : NULL;
	case CLASS_OBJ3D_LIGHT:
		p = operator_new(sizeof(CLight));
		return p ? (IObject3D *)CLight_ctor(p) : NULL;
	case CLASS_OBJ3D_GRAPHIC3D:
		p = operator_new(sizeof(CGraphic3D));
		return p ? (IObject3D *)CGraphic3D_ctor(p) : NULL;
	case CLASS_OBJ3D_PORTAL:
		p = operator_new(sizeof(CPortal));
		return p ? (IObject3D *)CPortal_ctor(p) : NULL;
	case CLASS_OBJ3D_PARTICLESYSTEM:
		CHECK(CIniFile_FindSection(node->ini, "Particles", 1));
		break;
	default:
		CHECK(0);
	}
	*set = 0;
	return NULL;
}

void __thiscall CScene_Read(CScene *self, CFileIO *f, byte bAnim) {
	char buf[0x100];

	CHECK(!self->Node3D);
	CFileIO_BeginBlock(f, "SCN");
	CHECK(CFileIO_ReadInt(f) == 1);
	String_Read(&self->mainLOD, f);
	String_Read(&self->node, f);
	IniFile_Read(f, &self->ini);
	CFileIO_Read(f, &self->ambient, sizeof(self->ambient));
	CFileIO_Read(f, &self->fog, sizeof(self->fog));
	CFileIO_Read(f, &self->box, sizeof(box_t));
	CFileIO_Read(f, &self->vertWeldThreshold, 4);
	IniFile_Read(f, &self->userProp);
	CHECK(!self->CollideMesh);
	Table_Resize4(&self->mats, CFileIO_ReadInt(f));
	for (int i = 0; i < self->mats.n; i++) {
		void *p = operator_new(sizeof(CMaterial));
		CMaterial *mat = p ? CMaterial_ctor(p) : NULL;
		TABLE_CHECK(self->mats.p, i, self->mats.n);
		((CMaterial **)self->mats.p)[i] = mat;
		TABLE_CHECK(self->mats.p, i, self->mats.n);
		CMaterial_Read(((CMaterial **)self->mats.p)[i], f);
		TABLE_CHECK(self->mats.p, i, self->mats.n);
		CHECK(((CMaterial **)self->mats.p)[i]->params.attrib & MAT_NOSHARE);
		TABLE_CHECK(self->mats.p, i, self->mats.n);
		CMatList_Add(g_MatList, ((CMaterial **)self->mats.p)[i]);
	}
	CHECK(CFileIO_ReadInt(f) == 1);
	int count = CFileIO_ReadInt(f);
	if (count > 0) {
		do {
			void *p = operator_new(sizeof(CNode3D));
			CNode3D *node = p ? CNode3D_ctor(p) : NULL;
			CScene_AddNode(self, node);
			CFileIO_Read(f, &node->index, 4);
			CFileIO_Read(f, &node->slot, 4);
			CFileIO_Read(f, &node->renderNode, 4);
			CFileIO_Read(f, &node->flags, 4);
			CFileIO_Read(f, &node->aniLocked, 4);
			String_Read(&node->name, f);
			CFileIO_ReadString(f, buf, sizeof(buf));
			CNode3D_SetParentName(node, buf[0] ? buf : NULL);
			CFileIO_Read(f, &node->pos, 0xc);
			CFileIO_Read(f, &node->rot, 0x10);
			CFileIO_Read(f, &node->visible, 4);
			CFileIO_Read(f, &node->m, 0x40);
			CFileIO_Read(f, &node->mObj, 0x40);
			CFileIO_Read(f, &node->qLook, 0x10);
			CFileIO_Read(f, &node->scale, 0xc);
			IniFile_Read(f, &node->ini);
			if ((node->flags & NODE_FX) && node->renderNode == -1) {
				node->obj58 = NodeFX_New(0, node);
			}
			MeshD3D_FileCtx ctx = { &self->mats, node };
			if (CFileIO_ReadInt(f)) {
				byte set;
				IObject3D *obj = Scene_NewObj(node, (uint32_t)CFileIO_ReadInt(f), &set);
				if (set) {
					CNode3D_SetObj3D(node, obj);
				}
				if (node->obj3D) {
					CHECK(OBJ3D_CALL(node->obj3D, OBJ3D_VF_READ, objFile_t, f, &ctx));
				}
			}
		} while (--count);
	}
	if (CFileIO_ReadInt(f)) {
		Path_Part(buf, self->name, 4);
		void *p = operator_new(sizeof(CAnimation));
		self->AnimM3D = p ? CAnimation_ctor(p) : NULL;
		String_Set((char **)&self->AnimM3D->name, buf);
		CAnimation_Read(self->AnimM3D, f);
	}
	CHECK(self->AnimM3D || !bAnim);
	CFileIO_Error(f, "SCN");
}

void __thiscall CScene_Write(CScene *self, CFileIO *f, byte bAnim) {
	if (bAnim) {
		CHECK(self->AnimM3D);
		CHECK(!self->ini);
		CHECK(!self->Node3D);
		CHECK(!self->mats.n);
	}
	CFileIO_BeginBlockWrite(f, "SCN");
	CFileIO_WriteInt(f, 1);
	String_Write(&self->mainLOD, f);
	String_Write(&self->node, f);
	IniFile_Write(f, self->ini);
	CFileIO_Write(f, &self->ambient, sizeof(self->ambient));
	CFileIO_Write(f, &self->fog, sizeof(self->fog));
	CFileIO_Write(f, &self->box, sizeof(box_t));
	CFileIO_Write(f, &self->vertWeldThreshold, 4);
	IniFile_Write(f, self->userProp);
	CHECK(!self->CollideMesh);
	CFileIO_WriteInt(f, self->mats.n);
	for (int i = 0; i < self->mats.n; i++) {
		TABLE_CHECK(self->mats.p, i, self->mats.n);
		CMaterial_Write(((CMaterial **)self->mats.p)[i], f);
	}
	CFileIO_WriteInt(f, 1);
	CFileIO_WriteInt(f, CScene_NumNodes(self));
	for (CNode3D *n = self->Node3D; n; n = n->next) {
		CFileIO_Write(f, &n->index, 4);
		CFileIO_Write(f, &n->slot, 4);
		CFileIO_Write(f, &n->renderNode, 4);
		CFileIO_Write(f, &n->flags, 4);
		CFileIO_Write(f, &n->aniLocked, 4);
		String_Write(&n->name, f);
		CFileIO_WriteString(f, n->parent ? n->parent->name : NULL);
		CFileIO_Write(f, &n->pos, 0xc);
		CFileIO_Write(f, &n->rot, 0x10);
		CFileIO_Write(f, &n->visible, 4);
		CFileIO_Write(f, &n->m, 0x40);
		CFileIO_Write(f, &n->mObj, 0x40);
		CFileIO_Write(f, &n->qLook, 0x10);
		CFileIO_Write(f, &n->scale, 0xc);
		IniFile_Write(f, n->ini);
		MeshD3D_FileCtx ctx = { &self->mats, n };
		CFileIO_WriteInt(f, n->obj3D != NULL);
		if (n->obj3D) {
			CFileIO_WriteInt(f, (int)CNode3D_Class(n));
			CHECK(OBJ3D_CALL(n->obj3D, OBJ3D_VF_WRITE, objFile_t, f, &ctx));
		}
	}
	CFileIO_WriteInt(f, self->AnimM3D != NULL);
	if (self->AnimM3D) {
		CAnimation_Write(self->AnimM3D, f);
	}
	CFileIO_EndBlockWrite(f, "SCN");
}

/* the render nodes of the model of the context, up to the first of a NODE_NO_RENDERNODE node */
static CRenderNode *Scene_RenderNode(CScene *self, uint32_t i) {
	CTable *t = self->anim->model->renderNodes;
	TABLE_CHECK(t->p, i, t->n);
	CRenderNode *r = &((CRenderNode *)t->p)[i];
	return (r->node->flags & NODE_NO_RENDERNODE) ? NULL : r;
}

void __thiscall CScene_Render(CScene *self, NodeAnimCtx *ctx) {
	self->anim = ctx;
	if (!ctx->renderNodes) {
		for (CNode3D *n = self->Node3D; n; n = n->next) {
			if (!(CNode3D_Class(n) & 0xe0)) {
				CNode3D_Render(n, g_pd3dDevice, NULL);
			}
		}
		for (CNode3D *n = self->Node3D; n; n = n->next) {
			if (CNode3D_Class(n) & 0xe0) {
				CNode3D_Render(n, g_pd3dDevice, NULL);
			}
		}
	} else {
		int num = ctx->model->renderNodes->n;
		for (int i = 0; i < num; i++) {
			CRenderNode *r = Scene_RenderNode(self, (uint32_t)i);
			if (!r) {
				break;
			}
			CNode3D_Render(r->node, g_pd3dDevice, r);
		}
	}
	self->anim = NULL;
}

void __thiscall CScene_Update(CScene *self, NodeAnimCtx *ctx) {
	self->anim = ctx;
	if (!ctx->renderNodes) {
		for (CNode3D *n = self->Node3D; n; n = n->next) {
			CNode3D_Update(n, NULL);
		}
	} else {
		int num = ctx->model->renderNodes->n;
		for (int i = 0; i < num; i++) {
			CRenderNode *r = Scene_RenderNode(self, (uint32_t)i);
			if (!r) {
				break;
			}
			CNode3D_Update(r->node, r);
		}
	}
	self->anim = NULL;
}

void __fastcall CScene_UpdateNodes(CScene *self) {
	for (CNode3D *n = self->Node3D; n; n = n->next) {
		CNode3D_Update(n, NULL);
	}
}

void __thiscall CScene_AddNode(CScene *self, CNode3D *node) {
	CHECK(node && !node->next && !node->prev && !node->scene);
	CNode3D *last = self->Node3D;
	if (!last) {
		node->Refs++;
		self->Node3D = node;
	} else {
		while (last->next) {
			last = last->next;
		}
		CNode3D_SetPrev(node, last);
		CNode3D_SetNext(last, node);
	}
	node->scene = self;
}

void __thiscall CScene_RemoveNode(CScene *self, CNode3D *node) {
	CHECK(node && node->scene == self);
	CHECK((node != self->Node3D) != (node->prev == NULL));
	if (self->Node3D == node) {
		CNode3D *next = node->next;
		if (next) {
			next->Refs++;
		}
		self->Node3D = next;
		node->Refs--;
	}
	node->scene = NULL;
	CNode3D_Unlink(node);
}

void __fastcall CScene_CompactMats(CScene *self) {
	int j = 0;

	for (int i = 0; i < self->mats.n; i++) {
		TABLE_CHECK(self->mats.p, i, self->mats.n);
		if (!(((CMaterial **)self->mats.p)[i]->params.attrib & 4)) {
			TABLE_CHECK(self->mats.p, i, self->mats.n);
			CMatList_Delete(g_MatList, ((CMaterial **)self->mats.p)[i]);
			TABLE_CHECK(self->mats.p, i, self->mats.n);
			((CMaterial **)self->mats.p)[i] = NULL;
		} else {
			TABLE_CHECK(self->mats.p, i, self->mats.n);
			CMaterial *mat = ((CMaterial **)self->mats.p)[i];
			TABLE_CHECK(self->mats.p, i, self->mats.n);
			((CMaterial **)self->mats.p)[i] = NULL;
			TABLE_CHECK(self->mats.p, j, self->mats.n);
			((CMaterial **)self->mats.p)[j] = mat;
			j++;
		}
	}
	Table_Resize4(&self->mats, j);
}

void __fastcall CScene_DeleteNodes(CScene *self) {
	for (CNode3D *n = self->Node3D; n; n = n->next) {
		CNode3D_Free(n);
	}
	while (self->Node3D) {
		CNode3D *n = self->Node3D;
		CScene_RemoveNode(self, n);
		((objDelete_t)n->vftable[0])(n, 1);
	}
}

CNode3D *__thiscall CScene_NodeObject(CScene *self, int index) {
	for (CNode3D *n = self->Node3D; n; n = n->next) {
		if (n->index == index) {
			return n;
		}
	}
	return NULL;
}

CNode3D *__thiscall CScene_FindClass(CScene *self, CNode3D *from, uint32_t class) {
	CNode3D *n = from ? from->next : self->Node3D;
	if (class) {
		while (n && CNode3D_Class(n) != class) {
			n = n->next;
		}
	}
	return n;
}

CNode3D *__thiscall CScene_FindNode(CScene *self, const char *name) {
	CHECK(self->Node3D);
	return CNode3D_Find(self->Node3D, name, 1);
}

void __thiscall CScene_UpdateBox(CScene *self, box_t *out, byte first) {
	box_t *box = &self->box;

	box->min = (vec3_t){ 3.4028234663852886e+38f, 3.4028234663852886e+38f, 3.4028234663852886e+38f };
	box->max = (vec3_t){ -3.4028234663852886e+38f, -3.4028234663852886e+38f, -3.4028234663852886e+38f };
	for (CNode3D *n = self->Node3D; n; n = n->next) {
		CNode3D_Update(n, NULL);
	}
	for (CNode3D *n = self->Node3D; n; n = n->next) {
		matrix_t m;
		box_t bb = { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } }, t;
		if (!CNode3D_GetBBox(n, &m, &bb, 1)) {
			continue;
		}
		const box_t *w = Box_Transform(&t, &bb, &m);
		box->min.x = box->min.x < w->min.x ? box->min.x : w->min.x;
		box->min.y = box->min.y < w->min.y ? box->min.y : w->min.y;
		box->min.z = box->min.z < w->min.z ? box->min.z : w->min.z;
		box->max.x = box->max.x > w->max.x ? box->max.x : w->max.x;
		box->max.y = box->max.y > w->max.y ? box->max.y : w->max.y;
		box->max.z = box->max.z > w->max.z ? box->max.z : w->max.z;
	}
	if (box->min.x > box->max.x && box->min.y > box->max.y && box->min.z > box->max.z) {
		if (!self->AnimM3D || self->Node3D) {
			E3D_LogInfo(g_FileLog, "ERROR: CEscena: Invalid BBox: %s", self->name);
		}
		if (first) {
			*box = (box_t){ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
		} else {
			CHECK(first);
		}
	}
	Box_Merge(out, box, first);
}

void __fastcall CScene_CalcWeights(CScene *self) {
	if (!CScene_CountClass(self, CLASS_OBJ3D_MESHD3D)) {
		return;
	}
	int n = 0;
	uint32_t t0 = exe_timeGetTime();
	CScene_UpdateNodes(self);
	for (CNode3D *node = self->Node3D; node; node = node->next) {
		if (CNode3D_Class(node) == CLASS_OBJ3D_MESHD3D) {
			n += CMeshD3D_CalcWeights(Obj3D_Cast(node->obj3D, CLASS_OBJ3D_MESHD3D), node) != 0;
		}
	}
	if (n) {
		E3D_DebugPrintf("\n CEscena::calcular_pesos(): %s (%08Xh) [%d nodos]", Path_FileName(self->name), self, n);
		E3D_DebugPrintf(" [%.2f seg.]", Scene_Seconds(t0));
	}
}

int __thiscall CScene_CountClass(CScene *self, uint32_t class) {
	int n = 0;
	for (CNode3D *node = self->Node3D; node; node = node->next) {
		if (CNode3D_Class(node) == class) {
			n++;
		}
	}
	return n;
}

int __fastcall CScene_NumNodes(CScene *self) {
	int n = 0;
	for (CNode3D *node = self->Node3D; node; node = node->next) {
		n++;
	}
	return n;
}

void __thiscall CScene_ConvertD3D(CScene *self, uint32_t flags) {
	int n = 0;

	for (CNode3D *node = self->Node3D; node; node = node->next) {
		n += CNode3D_BuildD3D(node, flags) != 0;
	}
	if (self->ini && CIniFile_GetBool(self->ini, "Model", "StaticLighting", 0)) {
		CScene_PrecalcIllum(self, 1, 0);
	}
	if (n) {
		E3D_DebugPrintf("\n CEscena::convertir_a_D3D(): %s (%08Xh) (f: %Xh) (n: %d)", Path_FileName(self->name), self,
			flags, n);
	}
}

void __fastcall CScene_ToGround(CScene *self) {
	E3D_DebugPrintf("\n CEscena::convertir_a_suelo(): %s (%08Xh)", Path_FileName(self->name), self);
	if (self->fromAmc) {
		CNode3D *n = self->Node3D;
		while (n && CNode3D_Class(n) != CLASS_OBJ3D_GROUND) {
			n = n->next;
		}
		CGround_Build(Obj3D_Cast(n->obj3D, CLASS_OBJ3D_GROUND), self);
		return;
	}
	void *p = operator_new(sizeof(CGround));
	CGround *ground = p ? CGround_ctor(p) : NULL;
	CGround_Build(ground, self);
	p = operator_new(sizeof(CNode3D));
	CNode3D *node = p ? CNode3D_ctor(p) : NULL;
	String_Set(&node->name, "suelo");
	CNode3D_SetObj3D(node, (IObject3D *)ground);
	CScene_AddNode(self, node);
	CScene_DeleteEmpty(self, 0, 0, 1);
}

void __thiscall CScene_DeleteEmpty(CScene *self, uint32_t class, byte bRefs, byte flagged) {
	byte deleted;

	CHECK(class == 0);
	CHECK(!bRefs);
	do {
		deleted = 0;
		CNode3D *n = self->Node3D;
		if (!n) {
			return;
		}
		do {
			CNode3D *next = n->next;
			if ((!flagged || (n->flags & NODE_DELETE)) && CNode3D_Class(n) == class) {
				int refs = n->next ? (n->prev != NULL) + 1 : (n->prev != NULL);
				if (n->Refs == refs) {
					if (!(n->flags & NODE_BONE)) {
						CHECK(n->parent);
						E3D_LogWhite(g_FileLog, "DEL_NODE: Node deleted: %s [parent: %s] [f:%08X]", n->name,
							n->parent->name, n->flags);
						deleted = 1;
						CScene_RemoveNode(self, n);
						((objDelete_t)n->vftable[0])(n, 1);
					} else {
						E3D_LogInfo(g_FileLog, "DEL_NODE: Node NOT deleted [Bone]: %s [parent: %s] [f:%08X]",
							n->name, n->parent->name, n->flags);
						Con_FatalErrorf("Hidden bone: %s", n->name);
					}
				}
			}
			n = next;
		} while (n);
	} while (deleted);
}

void __thiscall CScene_SetScale(CScene *self, const vec3_t *s) {
	if (s->x == 1.0f && s->y == 1.0f && s->z == 1.0f) {
		return;
	}
	const char *name = self->name;
	E3D_LogInfo(g_FileLog, "SCENE SCALE: %s [%s]", Vec3_Str(s, 0, 6), name);
	matrix_t m;
	Matrix_LoadIdentity(&m);
	m.m[0] = s->x;
	m.m[5] = s->y;
	m.m[10] = s->z;
	for (CNode3D *n = self->Node3D; n; n = n->next) {
		n->pos.x = n->pos.x * s->x;
		n->pos.y = s->y * n->pos.y;
		n->pos.z = s->z * n->pos.z;
		if (CNode3D_Class(n) == CLASS_OBJ3D_TRIMESH) {
			CTriMesh_Transform(Obj3D_Cast(n->obj3D, CLASS_OBJ3D_TRIMESH), &m);
		}
		if (CNode3D_Class(n) == CLASS_OBJ3D_LIGHT) {
			CLight *l = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_LIGHT);
			float k = (s->y + s->x + s->z) * 0.33333334f;
			l->pos.x = l->pos.x * s->x;
			l->pos.y = s->y * l->pos.y;
			l->pos.z = s->z * l->pos.z;
			Vec3_TransformDir0(&l->dir, &l->dir, &m);
			l->rangeStart = l->rangeStart * k;
			l->range = l->range * k;
		}
		if (CNode3D_Class(n) == CLASS_OBJ3D_GRAPHIC3D) {
			CGraphic3D *g = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_GRAPHIC3D);
			float k = (s->y + s->x + s->z) * 0.33333334f;
			g->width = g->width * k;
			g->height = g->height * k;
		}
	}
	if (self->AnimM3D) {
		CAnimation_Scale(self->AnimM3D, s);
	}
}

void __thiscall CScene_Join(CScene *self, byte sel, const char *name) {
	uint32_t mask = sel ? NODE_JOIN1 : NODE_JOIN2;

	for (CNode3D *dst = self->Node3D; dst; dst = dst->next) {
		if (!(dst->flags & mask)) {
			continue;
		}
		if (dst->obj3D) {
			return;
		}
		if (name && !strstr(dst->name, name)) {
			continue;
		}
		CTriMesh *acc = NULL;
		Matrix_LoadIdentity(&dst->m);
		for (CNode3D *n = self->Node3D; n; n = n->next) {
			if (n != dst) {
				CNode3D_Update(n, NULL);
			}
		}
		for (CNode3D *n = self->Node3D; n; n = n->next) {
			if (CNode3D_Class(n) != CLASS_OBJ3D_TRIMESH) {
				continue;
			}
			for (CNode3D *p = n->parent; p; p = p->parent) {
				if (p != dst) {
					continue;
				}
				CTriMesh *m = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_TRIMESH);
				if (m->d3d) {
					m = (CTriMesh *)m->d3d;
				}
				CTriMesh_Transform(m, &n->m);
				if (m->pTri) {
					if (!acc) {
						void *q = operator_new(sizeof(CTriMesh));
						acc = q ? CTriMesh_ctor(q) : NULL;
					}
					CTriMesh_Add(acc, m, 0, 0);
				}
				CNode3D_FreeObj3D(n);
				n->flags |= NODE_DELETE;
				break;
			}
		}
		if (acc) {
			CNode3D_SetObj3D(dst, (IObject3D *)acc);
			dst->flags &= ~NODE_COLLIDE;
		}
	}
	CScene_DeleteEmpty(self, 0, 0, 1);
}

void __thiscall CScene_SetAlpha(CScene *self, float alpha, const char *name) {
	int v = (int)((double)alpha * 255.0);
	byte a = v < 1 ? 0 : v >= 0xff ? 0xff : (byte)v;

	for (CNode3D *n = self->Node3D; n; n = n->next) {
		if (CNode3D_Class(n) != CLASS_OBJ3D_MESHD3D) {
			continue;
		}
		if (name && !strcmp(n->name, name)) {
			continue;
		}
		CMeshD3D *m = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_MESHD3D);
		if (!m) {
			continue;
		}
		do {
			CHECK(m->mat);
			m->mat->params.diffuse[3] = (byte)~a;
			m->mat->params.transp = a != 0;
			m->mat->params.srcBlend = 5;
			m->mat->params.dstBlend = 6;
			CMaterial_Prepare(m->mat, NULL);
			m = m->next;
		} while (m);
	}
}

void __thiscall CScene_SetNodes54(CScene *self, int v) {
	for (CNode3D *n = self->Node3D; n; n = n->next) {
		n->mark = v;
	}
}

void __thiscall CScene_PrecalcIllum(CScene *self, byte noLights, byte visible) {
	E3D_DebugPrintf("\n CEscena::precalc_ilum(): %s (%08Xh)", Path_FileName(self->name), self);
	uint32_t t0 = exe_timeGetTime();
	for (CNode3D *n = self->Node3D; n; n = n->next) {
		if (visible && (n->flags & NODE_HIDDEN)) {
			continue;
		}
		switch (CNode3D_Class(n)) {
		case CLASS_OBJ3D_TRIMESH:
			CHECK(0);
			break;
		case CLASS_OBJ3D_MESHD3D:
			CMeshD3D_CalcIllum(Obj3D_Cast(n->obj3D, CLASS_OBJ3D_MESHD3D), n, noLights ? NULL : self);
			break;
		case CLASS_OBJ3D_GROUND:
			CGround_CalcIllum(Obj3D_Cast(n->obj3D, CLASS_OBJ3D_GROUND), n, noLights ? NULL : self);
			break;
		}
	}
	E3D_DebugPrintf("  [%.2f seg.]", Scene_Seconds(t0));
}
