/*
 * The vertex buffer meshes of the engine3d, original source Engine/MallaD3D.cpp (include/scrap/meshd3d.h), with
 * the inline functions of MeshD3D.h it instances.
 */
#include <string.h>

#include "scrap/rdev.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/trimesh.h"
#include "scrap/misc.h"
#include "scrap/model.h"

#define IOBJECT3D_VFTABLE GAME_VFTABLE(IObject3D_vftable)

/* the engine (g_pD3DApp) and its device */

#define NODE_NOPOS 0xa8			/* flags of nodes whose meshes keep no positions */
#define NODE_NO_RENDER 0x80

/* params.attrib: no back faces for two sided materials */
#define MAT_ONESIDED 0x4000

/* the render node of RenderCtx.h as CMeshD3D_SetRender fills it */
typedef struct MeshD3D_RenderNode {
	byte pad_0[0x94];
	void *eva;			/* 0x94 the vertex animation data, NULL for none */
	int16_t channel;			/* 0x98 its channel */
	int16_t s9a;			/* 0x9a */
} MeshD3D_RenderNode;
SD_OFFSET(MeshD3D_RenderNode, s9a, 0x9a);

typedef CMeshD3D *(*meshD3DDelete_t)(CMeshD3D *self, byte flags);
typedef int(*meshD3DRestore_t)(CMeshD3D *self, void *dev);
typedef void(*meshD3DInvalidate_t)(CMeshD3D *self, void *dev);
typedef byte(*meshD3DFile_t)(CMeshD3D *self, CFileIO *f, MeshD3D_FileCtx *ctx);
typedef void(*nodeDelete_t)(CNode3D *self, int flags);

/* IUnknown::Release of a COM object, the pointer cleared */
static void MeshD3D_Release(void **p) {
	void *obj = *p;
	if (obj) {
		Rd_Release(obj);
		*p = NULL;
	}
}

/* the tests of the inline delete[] of the destructor */
static void MeshD3D_DeleteArray(void **p) {
	if (*p) {
		operator_delete_array(*p);
		*p = NULL;
	}
}

void CMeshD3D_Init(CMeshD3D *self) {
	self->center.x = 0.0f;
	self->center.y = 0.0f;
	self->class = CLASS_OBJ3D_MESHD3D;
	self->next = NULL;
	self->nTri = 0;
	self->pTri = NULL;
	self->vertOrig = NULL;
	self->triBase = 0;
	self->nSeg = 0;
	self->numSegs = 0;
	self->nPos = 0;
	self->pSeg = NULL;
	self->segs = NULL;
	self->pPos = NULL;
	self->triFlags = NULL;
	self->pVertVB = NULL;
	self->pVertIB = NULL;
	self->vbFrmt = 0;
	self->BaseIndex = 0;
	self->StartIndex = 0;
	self->StripList = 0;
	self->node = NULL;
	self->mat = NULL;
	self->weight = 0;
	self->weight2 = 0;
	self->center.z = 0.0f;
	self->nSkin = 0;
	self->pSkin = NULL;
	self->skinRef = 0;
	self->skinNode = 0;
	self->skinFrame = 0;
	self->pc0 = NULL;
	self->pc4 = NULL;
}

/* the part of both constructors before Init */
static void MeshD3D_Construct(CMeshD3D *self) {
	self->class = 0;
	self->vftable = CMESHD3D_VFTABLE;
	CVertList_ctor(&self->vert);
	memset(&self->box, 0, sizeof(self->box));
	memset(&self->ext, 0, sizeof(self->ext));
}

CMeshD3D *CMeshD3D_ctorMesh(CMeshD3D *self, CTriMesh *mesh, uint32_t flags, CNode3D *node) {
	MeshD3D_Construct(self);
	CHECK(mesh);
	CMeshD3D_Init(self);
	if (flags & MESHD3D_WEIGHT) {
		self->weight = 1;
	}
	if (flags & MESHD3D_WEIGHT2) {
		self->weight2 = 1;
	}
	if (node && (node->flags & NODE_NOPOS)) {
		flags &= ~MESHD3D_POS;
	}
	/* the checked cast of objeto3d.h */
	if (mesh->class != CLASS_OBJ3D_TRIMESH) {
		CHECK(!"devNULL");
		mesh = NULL;
	}
	CMeshD3D_Build(self, mesh, flags);
	if (node) {
		CNode3D_FreeObj3D(node);
		CNode3D_SetObj3D(node, (IObject3D *)self);
		CMeshD3D_SetNode(self, node);
	}
	CMeshD3D_CalcBox(self, NULL, 1);
	return self;
}

CMeshD3D *CMeshD3D_ctor(CMeshD3D *self) {
	MeshD3D_Construct(self);
	CMeshD3D_Init(self);
	return self;
}

void CMeshD3D_dtor(CMeshD3D *self) {
	self->vftable = CMESHD3D_VFTABLE;
	if (self->pVertVB || self->pVertIB) {
		CHECK(g_Engine && g_Engine->device);
		CMeshD3D_Invalidate(self, g_Engine->device);
	}
	if (self->skinRef) {
		self->pSkin = NULL;
	}
	CMeshD3D_SetNode(self, NULL);
	if (self->next) {
		((meshD3DDelete_t)self->next->vftable[MESHD3D_VF_DELETE])(self->next, 1);
		self->next = NULL;
	}
	MeshD3D_DeleteArray((void **)&self->pTri);
	MeshD3D_DeleteArray((void **)&self->vertOrig);
	MeshD3D_DeleteArray(&self->segs);
	MeshD3D_DeleteArray(&self->pSeg);
	MeshD3D_DeleteArray((void **)&self->pPos);
	MeshD3D_DeleteArray((void **)&self->triFlags);
	MeshD3D_DeleteArray((void **)&self->pSkin);
	CVertList_Free(&self->vert);
	self->vftable = IOBJECT3D_VFTABLE;
}

CMeshD3D *CMeshD3D_Delete(CMeshD3D *self, byte flags) {
	CMeshD3D_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

int MeshD3D_NewObj(void) {
	CHECK(g_NumD3DObj < MAX_D3D_OBJ);
	int i = g_NumD3DObj;
	memset(&g_D3DObj[i], 0, sizeof(MeshD3DObj));
	g_NumD3DObj = i + 1;
	return i;
}

void MeshD3DObj_SetTri(MeshD3DObj *obj, int i, const MeshD3DTri *t) {
	CHECK(obj && i < obj->nTri && obj->pTri && t);
	CHECK(t->v[0] < obj->nVert && t->v[1] < obj->nVert && t->v[2] < obj->nVert);
	obj->pTri[i] = *t;
}

void MeshD3DObj_SetVert(MeshD3DObj *obj, int i, const MeshD3DVert *v) {
	CHECK(obj && i < obj->nVert && obj->pVert && v);
	obj->pVert[i] = *v;
}

void MeshD3DObj_SetMat(MeshD3DObj *obj, CMaterial *mat) {
	CHECK(obj);
	obj->mat = mat;
}

void MeshD3DObj_AllocTri(MeshD3DObj *obj, int n) {
	CHECK(obj && !obj->nTri && !obj->pTri);
	obj->nTri = n;
	obj->pTri = Mem_Alloc(n * sizeof(MeshD3DTri));
}

void MeshD3DObj_AllocVert(MeshD3DObj *obj, int n) {
	CHECK(obj && !obj->nVert && !obj->pVert);
	obj->nVert = n;
	obj->pVert = Mem_Alloc(n * sizeof(MeshD3DVert));
}

void MeshD3DObj_Free(MeshD3DObj *obj) {
	CHECK(obj);
	if (obj->nVert) {
		obj->pVert = Mem_Free(obj->pVert);
		obj->nVert = 0;
	}
	if (obj->nTri) {
		obj->pTri = Mem_Free(obj->pTri);
		obj->nTri = 0;
	}
}

void CMeshD3D_Invalidate(CMeshD3D *self, void *dev) {
	if (self->next) {
		((meshD3DInvalidate_t)self->next->vftable[MESHD3D_VF_INVALIDATE])(self->next, dev);
	}
	MeshD3D_Release(&self->pVertVB);
	MeshD3D_Release(&self->pVertIB);
	MeshD3D_Release(&self->pc0);
	MeshD3D_Release(&self->pc4);
}

void CMeshD3D_GetBox(CMeshD3D *self, box_t *out, byte recalc) {
	if (recalc) {
		CMeshD3D_CalcBox(self, out, 1);
		return;
	}
	*out = self->box;
}

int CMeshD3D_Restore(CMeshD3D *self, void *dev) {
	int hr = CMeshD3D_CreateVB(self);
	if (self->next) {
		return ((meshD3DRestore_t)self->next->vftable[MESHD3D_VF_RESTORE])(self->next, dev);
	}
	return hr;
}

byte CMeshD3D_Read(CMeshD3D *self, CFileIO *f, MeshD3D_FileCtx *ctx) {
	char tmp[256];
	uint32_t n;

	CMeshD3D_SetNode(self, ctx->node);
	CFileIO_BeginBlock(f, "MD3D");
	CHECK(CFileIO_ReadInt(f) == 1);
	CFileIO_ReadString(f, tmp, sizeof(tmp));
	CHECK(!strcmp(tmp, self->node->name));
	Array_Read6(f, (void **)&self->pTri, &self->nTri);
	CVertList_Read(&self->vert, f);
	Array_Read2(f, (void **)&self->vertOrig, &n);
	CHECK(!self->vertOrig || n == (uint32_t)self->vert.nVert);
	CFileIO_Read4(f, &self->triBase);
	/* the copy at rva 0x2af4e0 */
	Array_Read10(f, &self->pSeg, &self->nSeg);
	Array_Read8(f, &self->segs, &self->numSegs);
	Array_ReadC(f, (void **)&self->pPos, &self->nPos);
	Array_Read4(f, (void **)&self->triFlags, &n);
	CHECK(!self->triFlags || n == self->nTri);
	self->skinRef = CFileIO_ReadBool(f);
	if (!self->skinRef) {
		Array_Read10(f, (void **)&self->pSkin, &self->nSkin);
	} else {
		/* the skin of the first mesh of the node */
		CMeshD3D *first = (CMeshD3D *)self->node->obj3D;
		if (first->class != CLASS_OBJ3D_MESHD3D) {
			CHECK(!"devNULL");
			first = NULL;
		}
		self->pSkin = first->pSkin;
		self->nSkin = first->nSkin;
		CHECK(self->skinRef && self->pSkin);
	}
	int i = CFileIO_ReadInt(f);
	if (i != -1) {
		CTable *mats = ctx->mats;
		CHECK(mats->p && (uint32_t)i < (uint32_t)mats->n);
		self->mat = ((CMaterial **)mats->p)[i];
	}
	self->weight = CFileIO_ReadBool(f);
	self->weight2 = CFileIO_ReadBool(f);
	CFileIO_Read(f, &self->box, sizeof(self->box));
	CFileIO_Read(f, &self->ext, sizeof(self->ext));
	CFileIO_Read(f, &self->center, sizeof(self->center));
	if (CFileIO_ReadInt(f)) {
		CMeshD3D *m = operator_new(sizeof(CMeshD3D));
		if (m) {
			m = CMeshD3D_ctor(m);
		}
		self->next = m;
		((meshD3DFile_t)m->vftable[MESHD3D_VF_READ])(m, f, ctx);
	}
	CFileIO_Error(f, "MD3D");
	return 1;
}

void CMeshD3D_CheckList(CMeshD3D *self) {
	do {
		CHECK(self->vert.nVert > 0);
		CHECK((int)self->nTri > 0);
		self = self->next;
	} while (self);
}

int CMeshD3D_Draw(CMeshD3D *self, void *dev, void *ctx) {
	(void)dev;
	return MeshD3D_Render(self, ctx);
}

byte CMeshD3D_Write(CMeshD3D *self, CFileIO *f, MeshD3D_FileCtx *ctx) {
	CHECK(self->node == ctx->node);
	CFileIO_BeginBlockWrite(f, "MD3D");
	CFileIO_WriteInt(f, 1);
	CFileIO_WriteString(f, self->node->name);
	Array_Write6(f, self->pTri, self->nTri);
	CVertList_Write(&self->vert, f);
	Array_Write2(f, self->vertOrig, self->vert.nVert);
	CFileIO_WriteInt(f, self->triBase);
	Array_Write10(f, self->pSeg, self->nSeg);
	Array_Write8(f, self->segs, self->numSegs);
	Array_WriteC(f, self->pPos, self->nPos);
	CHECK(!self->triFlags);
	Array_Write4(f, self->triFlags, self->nTri);
	CFileIO_WriteInt(f, self->skinRef);
	if (!self->skinRef) {
		Array_Write10(f, self->pSkin, self->nSkin);
	}
	/* the index of the material in the table, -1 for none */
	CTable *mats = ctx->mats;
	int i;
	for (i = 0; i < mats->n; i++) {
		if (((CMaterial **)mats->p)[i] == self->mat) {
			break;
		}
	}
	if (i >= mats->n) {
		i = -1;
	}
	CFileIO_WriteInt(f, i);
	if (i < 0) {
		E3D_LogInfo(g_FileLog, "WARN: Null material in object: %s", self->node->name);
		CHECK(self->node->flags & NODE_NO_RENDER);
		self->mat = NULL;
	}
	CFileIO_WriteInt(f, self->weight);
	CFileIO_WriteInt(f, self->weight2);
	CFileIO_Write(f, &self->box, sizeof(self->box));
	CFileIO_Write(f, &self->ext, sizeof(self->ext));
	CFileIO_Write(f, &self->center, sizeof(self->center));
	CFileIO_WriteInt(f, self->next != NULL);
	if (self->next) {
		((meshD3DFile_t)self->next->vftable[MESHD3D_VF_WRITE])(self->next, f, ctx);
	}
	CFileIO_EndBlockWrite(f, "MD3D");
	return 1;
}

void CMeshD3D_SetRender(CMeshD3D *self, void *renderNode) {
	MeshD3D_RenderNode *r = renderNode;
	int channel = 3;
	void *eva = CNode3D_TrackData(self->node, 3);

	if (!eva) {
		channel = 2;
		eva = CNode3D_TrackData(self->node, 2);
	}
	if (!r) {
		CHECK(!eva || r);
		return;
	}
	r->eva = eva;
	if (!eva) {
		return;
	}
	NodeAnimChannel *c = CNode3D_Channel(self->node, channel);
	/* the channel's animation is a model slot (RenderCtx_SetChannel); its morph targets move the vertices */
	const CAnimSlot *slot = (const CAnimSlot *)c->anim;
	r->channel = channel;
	r->s9a = (int16_t)c->frame;
	if (!slot->subset && !slot->prevSubset) {
		r->eva = NULL;
	}
	CHECK(!self->next);
}

int MeshD3D_CompareVert(const void *a, const void *b) {
	const MeshD3DVert *va = a, *vb = b;
	if (va->bone != vb->bone) {
		return (int)va->bone - (int)vb->bone;
	}
	int r = memcmp(&va->v, &vb->v, sizeof(va->v));
	return r < 0 ? -1 : r > 0;
}

vec3_t *CMeshD3D_TriNormal(CMeshD3D *self, vec3_t *out, uint32_t i) {
	vec3_t a, b, c, n;

	CHECK(i < self->nTri);
	CHECK(self->pTri);
	const uint16_t *t = self->pTri[i];
	CVertList_GetPos(&self->vert, &a, t[0]);
	CVertList_GetPos(&self->vert, &b, t[1]);
	CVertList_GetPos(&self->vert, &c, t[2]);
	float e1x = b.x - a.x, e1y = b.y - a.y, e1z = b.z - a.z;
	float e2x = c.x - a.x, e2y = c.y - a.y, e2z = c.z - a.z;
	n.x = e1y * e2z - e1z * e2y;
	n.y = e1z * e2x - e1x * e2z;
	n.z = e1x * e2y - e1y * e2x;
	Mesh_Normalize(&n);
	*out = n;
	return out;
}

void CMeshD3D_CalcBox(CMeshD3D *self, box_t *out, byte first) {
	CMeshD3D_CalcCenter(self);
	self->box = self->ext;
	if (self->next) {
		CMeshD3D_CalcBox(self->next, &self->box, 0);
	}
	/* an identical copy of Box_Merge, rva 0x2b8870 */
	Box_Merge(out, &self->box, first);
}

void CMeshD3D_CalcCenter(CMeshD3D *self) {
	CVertList_GetExtents(&self->vert, &self->ext, 1);
	self->center.x = (self->ext.min.x + self->ext.max.x) * 0.5f;
	self->center.y = (self->ext.max.y + self->ext.min.y) * 0.5f;
	self->center.z = (self->ext.max.z + self->ext.min.z) * 0.5f;
	if (self->pSkin) {
		Vec3_TransformH(&self->center, &self->center, &self->node->mObj);
	}
}

/* -1, 0 or 1 like the inline strcmp of the original */
static int MeshD3D_StrCmp(const char *a, const char *b) {
	int r = strcmp(a, b);
	return r < 0 ? -1 : r > 0;
}

int MeshD3D_CompareGroup(const void *a, const void *b) {
	const MeshD3DGroup *ga = a, *gb = b;
	CHECK(ga != gb);
	const char *na = ga->mat ? CMaterial_GetMapName(ga->mat, 0) : NULL;
	const char *nb = gb->mat ? CMaterial_GetMapName(gb->mat, 0) : NULL;
	int r;
	if (na && nb) {
		r = MeshD3D_StrCmp(na, nb);
	} else {
		r = (nb == NULL) - (na == NULL);
	}
	if (r) {
		return r;
	}
	r = (int)((uintptr_t)ga->mat - (uintptr_t)gb->mat);
	if (r) {
		return r;
	}
	return ga->idx - gb->idx;
}

/* n.p + d of the origin, by the zero vector as the original */
static float MeshD3D_PlaneSide(const plane_t *p) {
	return p->n.y * 0.0f + p->n.x * 0.0f + p->n.z * 0.0f + p->d;
}

static float MeshD3D_Len2(const vec3_t *v) {
	return v->x * v->x + v->y * v->y + v->z * v->z;
}

int MeshD3D_CompareTri(const void *a, const void *b) {
	vec3_t ca, cb;
	plane_t pa, pb;

	MeshD3D_TriPlane(a, &ca, &pa);
	MeshD3D_TriPlane(b, &cb, &pb);
	float sa = MeshD3D_PlaneSide(&pa);
	float sb = MeshD3D_PlaneSide(&pb);
	if ((0.0f > sa) != (0.0f > sb)) {
		return (int)(sb - sa);
	}
	return (int)(MeshD3D_Len2(&cb) - MeshD3D_Len2(&ca));
}

int MeshD3D_CompareVertIdx(const void *a, const void *b) {
	return MeshD3D_CompareVert(&g_WeldVert[*(const int *)a], &g_WeldVert[*(const int *)b]);
}

void CMeshD3D_Build(CMeshD3D *self, CTriMesh *mesh, uint32_t flags) {
	CHECK(mesh);
	CHECK(self->vert.nVert == 0 && !self->pTri);
	/* a mesh at 0x30 is used in its place */
	CTriMesh *src = mesh->d3d ? (CTriMesh *)mesh->d3d : mesh;
	int first = g_NumD3DObj;
	CHECK(!first);
	int nTri = src->nTri;
	if (!nTri || !src->nVert) {
		return;
	}

	/* the back faces of the two sided materials come first, from the last triangle down */
	int nBack = 0;
	for (int i = 0; i < nTri; i++) {
		CMaterial *mat = src->pTri[i].mat;
		if (mat && !(mat->params.attrib & MAT_ONESIDED) && !(flags & MESHD3D_ONESIDED) && mat->params.twoSided) {
			nBack++;
		}
	}
	int nTmp = nTri + nBack;
	MeshD3DTriTmp *tmp = Mem_Calloc((nTmp + nBack) * sizeof(MeshD3DTriTmp));
	int n = 0;
	for (int i = src->nTri - 1; i >= 0 && nBack; i--) {
		CMaterial *mat = src->pTri[i].mat;
		if (mat && mat->params.twoSided) {
			MeshD3D_FillTri(&tmp[n], src, i, 0);
			n++;
		}
	}
	for (int i = 0; i < src->nTri; i++) {
		MeshD3D_FillTri(&tmp[n], src, i, 1);
		n++;
	}
	CHECK(n == nTmp);

	/* the groups: the last one while it fits, else the first that fits, else a new one */
	cDynList *dyn = DynList_NewBlock(sizeof(MeshD3DGroup), 0x10);
	MeshD3DGroup *g = NULL;
	int gi = 0;
	for (int k = 0; k < nTmp; k++) {
		MeshD3DTriTmp *t = &tmp[k];
		if (!MeshD3D_GroupFits(g, t)) {
			g = NULL;
			for (gi = 0; gi < (int)dyn->used; gi++) {
				MeshD3DGroup *c = (MeshD3DGroup *)(dyn->mem + gi * dyn->elemSize);
				if (MeshD3D_GroupFits(c, t)) {
					g = c;
					break;
				}
			}
			if (!g) {
				g = DynList_AddOne(dyn);
				memset(g, 0, sizeof(*g));
				g->mat = t->mat;
				gi = dyn->used - 1;
			}
		}
		g->nTri++;
		t->group = gi;
	}
	MeshD3DGroup *grp = (MeshD3DGroup *)dyn->mem;
	for (int k = 0; k < (int)dyn->used; k++) {
		grp[k].idx = k;
	}
	crt_qsort(dyn->mem, dyn->used, dyn->elemSize, MeshD3D_CompareGroup);
	int *remap = New_Array(dyn->used, sizeof(int));
	for (int k = 0; k < (int)dyn->used; k++) {
		remap[grp[k].idx] = k;
	}
	for (int k = 0; k < nTmp; k++) {
		tmp[k].group = remap[tmp[k].group];
	}
	operator_delete_sized(remap, sizeof(int));

	/* one object a group, the corners of each triangle as new vertices */
	for (int k = 0; k < (int)dyn->used; k++) {
		grp[k].idx = MeshD3D_NewObj();
		MeshD3DObj_AllocVert(&g_D3DObj[grp[k].idx], grp[k].nTri * 3);
		MeshD3DObj_AllocTri(&g_D3DObj[grp[k].idx], grp[k].nTri);
		MeshD3DObj_SetMat(&g_D3DObj[grp[k].idx], grp[k].mat);
	}
	for (int k = 0; k < nTmp; k++) {
		MeshD3DTriTmp *t = &tmp[k];
		MeshD3DGroup *gk = &grp[t->group];
		int j = gk->used++;
		MeshD3DObj *o = &g_D3DObj[gk->idx];
		MeshD3DObj_SetVert(o, j * 3, &t->v[0]);
		MeshD3DObj_SetVert(o, j * 3 + 1, &t->v[1]);
		MeshD3DObj_SetVert(o, j * 3 + 2, &t->v[2]);
		MeshD3DTri tri = { t->flags, { (uint16_t)(j * 3), (uint16_t)(j * 3 + 1), (uint16_t)(j * 3 + 2) } };
		MeshD3DObj_SetTri(o, j, &tri);
	}
	DynList_Free(dyn);
	for (int k = first; k < g_NumD3DObj; k++) {
		MeshD3DObj_Weld(&g_D3DObj[k]);
	}
	Mem_Free(tmp);

	/* one mesh of the list an object */
	CHECK(!self->next);
	CMeshD3D *cur = self;
	for (int k = 0; k < g_NumD3DObj; k++) {
		MeshD3DObj *o = &g_D3DObj[k];
		if (k) {
			CMeshD3D *m = operator_new(sizeof(CMeshD3D));
			if (m) {
				m = CMeshD3D_ctor(m);
			}
			cur->next = m;
			m->weight = cur->weight;
			cur->next->weight2 = cur->weight2;
			cur = cur->next;
		}
		cur->mat = o->mat;
		CMeshD3D_AllocTri(cur, o->nTri);
		if (!cur->mat || !cur->mat->params.reflection) {
			CVertList_SetFormat(&cur->vert, (self->weight || self->weight2) ? VERT_TEX1_B1 : VERT_TEX1);
		} else {
			CVertList_SetFormat(&cur->vert, g_UseHWEnvMap ? VERT_TEX1 : VERT_TEX2);
			self->weight = 0;
			self->weight2 = 0;
		}
		CVertList_Alloc(&cur->vert, o->nVert, 0);
		CHECK(!cur->vertOrig);
		if (flags & MESHD3D_VERTORIG) {
			cur->vertOrig = New_Array(cur->vert.nVert, sizeof(uint16_t));
		}
		for (int v = 0; v < cur->vert.nVert; v++) {
			const MeshD3DVert *s = &o->pVert[v];
			if (cur->vertOrig) {
				cur->vertOrig[v] = s->index;
			}
			switch (cur->vert.frmt) {
			case VERT_TEX1:
				CVertList_SetTex1(&cur->vert, v, &s->v);
				break;
			case VERT_TEX2: {
				float x[10] = { s->v.pos.x, s->v.pos.y, s->v.pos.z, s->v.n.x, s->v.n.y, s->v.n.z, s->v.uv.u,
					s->v.uv.v, 0.0f, 0.0f };
				CVertList_SetTex2(&cur->vert, v, x);
				break;
			}
			case VERT_TEX1_B1: {
				/* a weight of 0 or 1 at random */
				int r = crt_rand();
				float x[9] = { s->v.pos.x, s->v.pos.y, s->v.pos.z, (float)(r & 1), s->v.n.x, s->v.n.y, s->v.n.z,
					s->v.uv.u, s->v.uv.v };
				CVertList_SetTex1B1(&cur->vert, v, x);
				break;
			}
			default:
				CHECK(0);
				break;
			}
		}
		CHECK(!cur->triFlags);
		if (flags & MESHD3D_TRIFLAGS) {
			cur->triFlags = New_Array(cur->nTri, sizeof(uint32_t));
		}
		for (uint32_t j = 0; (int)j < (int)cur->nTri; j++) {
			const MeshD3DTri *s = &o->pTri[j];
			CMeshD3D_SetTri(cur, j, s->v[0], s->v[1], s->v[2]);
			if (cur->triFlags) {
				cur->triFlags[j] = s->flags;
			}
		}
		MeshD3DObj_Free(o);
	}
	g_NumD3DObj = 0;

	if (flags & MESHD3D_POS) {
		self->pPos = New_Array(src->nVert, sizeof(vec3_t));
		self->nPos = src->nVert;
		for (int v = 0; v < src->nVert; v++) {
			vec3_t p;
			self->pPos[v] = *CTriMesh_GetVert(src, &p, v);
		}
		CMeshD3D_CalcSegments(self, 1);
	}
	if (src->pSkin) {
		CHECK(self->pSkin == NULL);
		self->pSkin = New_Array(src->nVert, 16);
		self->nSkin = src->nVert;
		self->skinRef = 0;
		for (int v = 0; v < src->nVert; v++) {
			memcpy(self->pSkin + v * 16, src->pSkin + v * 16, 16);
		}
		for (CMeshD3D *m = self->next; m; m = m->next) {
			CHECK(m->pSkin == NULL);
			m->pSkin = self->pSkin;
			m->nSkin = self->nSkin;
			m->skinRef = 1;
		}
	}
}

int CMeshD3D_CreateVB(CMeshD3D *self) {
	CHECK(self->vert.nVert > 0);
	CHECK((int)self->nTri > 0);
	if (!self->vert.nVert || !self->pTri) {
		return E_FAIL;
	}
	CHECK(self->vert.nVert > 0);
	CHECK((int)self->nTri > 0);
	CHECK(g_Engine);
	CHECK(!self->pVertVB);
	CHECK(!self->pVertIB);
	self->vbFrmt = self->vert.frmt;
	self->BaseIndex = 0;
	self->StartIndex = 0;
	self->StripList = 0;
	CHECK(Render_CreateVB(&self->pVertVB, &self->vert, 0) == 0);
	CHECK(Render_CreateIB(&self->pVertIB, self->pTri, self->nTri, 0) == 0);
	if (self->pSkin) {
		MeshD3D_Release(&self->pc0);
		MeshD3D_Release(&self->pc4);
	}
	return 0;
}

void CMeshD3D_SetNode(CMeshD3D *self, CNode3D *node) {
	for (CMeshD3D *m = self; m; m = m->next) {
		if (node) {
			node->Refs++;
		}
		if (m->node) {
			m->node->Refs--;
			CNode3D *old = m->node;
			if (old->Refs == 0 && old) {
				((nodeDelete_t)old->vftable[0])(old, 1);
			}
			m->node = NULL;
		}
		m->node = node;
	}
}

void MeshD3D_TriPlane(const MeshD3DTriTmp *t, vec3_t *center, plane_t *plane) {
	vec3_t v0 = t->v[0].v.pos, v1 = t->v[1].v.pos, v2 = t->v[2].v.pos;

	CHECK(Plane_Calc(plane, &v0, &v1, &v2));
	center->x = (v0.x + v1.x + v2.x) / 3.0f;
	center->y = (v0.y + v1.y + v2.y) / 3.0f;
	center->z = (v0.z + v1.z + v2.z) / 3.0f;
}

void CMeshD3D_AllocTri(CMeshD3D *self, int n) {
	CHECK(n > 0);
	CHECK(!self->pTri);
	self->nTri = n;
	self->pTri = New_Array(n, 6);
}

void MeshD3D_FillTri(MeshD3DTriTmp *t, CTriMesh *m, uint32_t i, byte keep) {
	for (int k = 0; k < 3; k++) {
		int v = CTriMesh_GetTri(m, i)->v[k];
		CTriMesh_GetTriVert(m, &t->v[k].v, i, k);
		t->v[k].index = (uint16_t)v;
		t->v[k].bone = m->pSkin ? ((const byte *)CTriMesh_GetSkin(m, v))[1] : 0;
		t->v[k].pad = 0;
	}
	if (!keep) {
		MeshD3DVert x = t->v[1];
		t->v[1] = t->v[2];
		t->v[2] = x;
		for (int k = 0; k < 3; k++) {
			t->v[k].v.n.x = -t->v[k].v.n.x;
			t->v[k].v.n.y = -t->v[k].v.n.y;
			t->v[k].v.n.z = -t->v[k].v.n.z;
		}
	}
	t->mat = m->pTri[i].mat;
	t->flags = m->pTri[i].flags;
}

void CMeshD3D_SetTri(CMeshD3D *self, uint32_t i, uint16_t a, uint16_t b, uint16_t c) {
	CHECK(i < self->nTri);
	CHECK(self->pTri);
	self->pTri[i][0] = a;
	self->pTri[i][1] = b;
	self->pTri[i][2] = c;
}

int MeshD3D_GroupFits(const MeshD3DGroup *g, const MeshD3DTriTmp *t) {
	if (g && t && g->mat == t->mat) {
		return g->nTri < g_MaxTriGroup;
	}
	return 0;
}

int CMeshD3D_Check(CMeshD3D *self) {
	CHECK(self->vert.nVert > 0);
	CHECK((int)self->nTri > 0);
	return 0;
}

int MeshD3DObj_Weld(MeshD3DObj *obj) {
	CHECK(obj && obj->pVert && obj->nVert);
	g_WeldVert = obj->pVert;
	int *order = Mem_Alloc(obj->nVert * sizeof(int));
	for (int i = 0; i < obj->nVert; i++) {
		order[i] = i;
	}
	crt_qsort(order, obj->nVert, sizeof(int), MeshD3D_CompareVertIdx);
	MeshD3DVert *out = Mem_Alloc(obj->nVert * sizeof(MeshD3DVert));
	int *remap = Mem_Alloc(obj->nVert * sizeof(int));
	const MeshD3DVert *prev = NULL;
	int n = 0;
	for (int k = 0; k < obj->nVert; k++) {
		const MeshD3DVert *v = &obj->pVert[order[k]];
		if (!prev || MeshD3D_CompareVert(v, prev)) {
			out[n++] = *v;
			prev = v;
		}
		remap[order[k]] = n - 1;
	}
	int removed = obj->nVert - n;
	Mem_Free(obj->pVert);
	if (obj->nVert != n) {
		out = Mem_Realloc(out, n * sizeof(MeshD3DVert));
	}
	obj->nVert = n;
	obj->pVert = out;
	for (int t = 0; t < obj->nTri; t++) {
		for (int k = 0; k < 3; k++) {
			obj->pTri[t].v[k] = (uint16_t)remap[obj->pTri[t].v[k]];
		}
	}
	Mem_Free(remap);
	Mem_Free(order);
	return removed;
}
