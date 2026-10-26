/*
 * The vertex buffer meshes of the engine3d, original source Engine/MallaD3D.cpp (include/scrap/mallad3d.h), with
 * the inline functions of MallaD3D.h it instances.
 */
#include <string.h>

#include "scrap/e3dutil.h"
#include "scrap/mallatri.h"
#include "scrap/varios.h"

#define IOBJETO3D_VFTABLE 0x3d79dc
#define E_FAIL ((int)0x80004005)

/* the engine (g_pD3DApp) and its device */

#define NODO_NOPOS 0xa8			/* flags of nodes whose meshes keep no positions */
#define NODO_NO_RENDER 0x80

/* params.atrib: no back faces for two sided materials */
#define MAT_ONESIDED 0x4000

/* the render node of RenderCtx.h as CMallaD3D_SetRender fills it */
typedef struct MallaD3D_NodoR {
	byte pad_0[0x94];
	void *eva;			/* 0x94 the vertex animation data, NULL for none */
	int16_t canal;			/* 0x98 its channel */
	int16_t s9a;			/* 0x9a */
} MallaD3D_NodoR;
SD_OFFSET(MallaD3D_NodoR, s9a, 0x9a);

/* an entry of the animation channels of a node: the animation first */
typedef struct MallaD3D_Canal {
	byte *anim;			/* 0x0 bytes 0xc and 0xd: whether it moves the vertices */
	int i4;				/* 0x4 */
} MallaD3D_Canal;

typedef CMallaD3D *(__thiscall *mallaD3DDelete_t)(CMallaD3D *self, byte flags);
typedef int(__thiscall *mallaD3DRestore_t)(CMallaD3D *self, void *dev);
typedef void(__thiscall *mallaD3DInvalidate_t)(CMallaD3D *self, void *dev);
typedef byte(__thiscall *mallaD3DFile_t)(CMallaD3D *self, CArchIO *f, MallaD3D_FileCtx *ctx);
typedef void(__thiscall *nodoDelete_t)(CNodo3D *self, int flags);
typedef uint32_t(__stdcall *comRelease_t)(void *self);

/* IUnknown::Release of a COM object, the pointer cleared */
static void MallaD3D_Release(void **p) {
	void *obj = *p;
	if (obj) {
		((comRelease_t)(*(void ***)obj)[2])(obj);
		*p = NULL;
	}
}

/* the tests of the inline delete[] of the destructor */
static void MallaD3D_DeleteArray(void **p) {
	if (*p) {
		operator_delete_array(*p);
		*p = NULL;
	}
}

void __fastcall CMallaD3D_Init(CMallaD3D *self) {
	self->center.x = 0.0f;
	self->center.y = 0.0f;
	self->clase = CLASE_OBJ3D_MALLAD3D;
	self->next = NULL;
	self->nTri = 0;
	self->pTri = NULL;
	self->vertOrig = NULL;
	self->triBase = 0;
	self->nSeg = 0;
	self->n34 = 0;
	self->nPos = 0;
	self->pSeg = NULL;
	self->p40 = NULL;
	self->pPos = NULL;
	self->triFlags = NULL;
	self->pVertVB = NULL;
	self->pVertIB = NULL;
	self->vbFrmt = 0;
	self->i9c = 0;
	self->ia0 = 0;
	self->ia4 = 0;
	self->nodo = NULL;
	self->mat = NULL;
	self->b50 = 0;
	self->b51 = 0;
	self->center.z = 0.0f;
	self->nSkin = 0;
	self->pSkin = NULL;
	self->skinRef = 0;
	self->ib8 = 0;
	self->ibc = 0;
	self->pc0 = NULL;
	self->pc4 = NULL;
}

/* the part of both constructors before Init */
static void MallaD3D_Construct(CMallaD3D *self) {
	self->clase = 0;
	self->vftable = Game_Ptr(CMALLAD3D_VFTABLE);
	CListaVert_ctor(&self->vert);
	memset(&self->box, 0, sizeof(self->box));
	memset(&self->ext, 0, sizeof(self->ext));
}

CMallaD3D *__thiscall CMallaD3D_ctorMalla(CMallaD3D *self, CMallaTri *malla, uint32_t flags, CNodo3D *nodo) {
	MallaD3D_Construct(self);
	CHECK(malla);
	CMallaD3D_Init(self);
	if (flags & MALLAD3D_WEIGHT) {
		self->b50 = 1;
	}
	if (flags & MALLAD3D_WEIGHT2) {
		self->b51 = 1;
	}
	if (nodo && (nodo->flags & NODO_NOPOS)) {
		flags &= ~MALLAD3D_POS;
	}
	/* the checked cast of objeto3d.h */
	if (malla->clase != CLASE_OBJ3D_MALLATRI) {
		CHECK(!"devNULL");
		malla = NULL;
	}
	CMallaD3D_Build(self, malla, flags);
	if (nodo) {
		CNodo3D_FreeObj3D(nodo);
		CNodo3D_SetObj3D(nodo, (IObjeto3D *)self);
		CMallaD3D_SetNodo(self, nodo);
	}
	CMallaD3D_CalcBox(self, NULL, 1);
	return self;
}

CMallaD3D *__fastcall CMallaD3D_ctor(CMallaD3D *self) {
	MallaD3D_Construct(self);
	CMallaD3D_Init(self);
	return self;
}

void __fastcall CMallaD3D_dtor(CMallaD3D *self) {
	self->vftable = Game_Ptr(CMALLAD3D_VFTABLE);
	if (self->pVertVB || self->pVertIB) {
		CHECK(g_Motor && MOTOR_DEV(g_Motor));
		CMallaD3D_Invalidate(self, MOTOR_DEV(g_Motor));
	}
	if (self->skinRef) {
		self->pSkin = NULL;
	}
	CMallaD3D_SetNodo(self, NULL);
	if (self->next) {
		((mallaD3DDelete_t)self->next->vftable[MALLAD3D_VF_DELETE])(self->next, 1);
		self->next = NULL;
	}
	MallaD3D_DeleteArray((void **)&self->pTri);
	MallaD3D_DeleteArray((void **)&self->vertOrig);
	MallaD3D_DeleteArray(&self->p40);
	MallaD3D_DeleteArray(&self->pSeg);
	MallaD3D_DeleteArray((void **)&self->pPos);
	MallaD3D_DeleteArray((void **)&self->triFlags);
	MallaD3D_DeleteArray((void **)&self->pSkin);
	CListaVert_Free(&self->vert);
	self->vftable = Game_Ptr(IOBJETO3D_VFTABLE);
}

CMallaD3D *__thiscall CMallaD3D_Delete(CMallaD3D *self, byte flags) {
	CMallaD3D_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

int MallaD3D_NewObj(void) {
	CHECK(g_NumD3DObj < MAX_D3D_OBJ);
	int i = g_NumD3DObj;
	memset(&g_D3DObj[i], 0, sizeof(MallaD3DObj));
	g_NumD3DObj = i + 1;
	return i;
}

void MallaD3DObj_SetTri(MallaD3DObj *obj, int i, const MallaD3DTri *t) {
	CHECK(obj && i < obj->nTri && obj->pTri && t);
	CHECK(t->v[0] < obj->nVert && t->v[1] < obj->nVert && t->v[2] < obj->nVert);
	obj->pTri[i] = *t;
}

void MallaD3DObj_SetVert(MallaD3DObj *obj, int i, const MallaD3DVert *v) {
	CHECK(obj && i < obj->nVert && obj->pVert && v);
	obj->pVert[i] = *v;
}

void MallaD3DObj_SetMat(MallaD3DObj *obj, CMaterial *mat) {
	CHECK(obj);
	obj->mat = mat;
}

void MallaD3DObj_AllocTri(MallaD3DObj *obj, int n) {
	CHECK(obj && !obj->nTri && !obj->pTri);
	obj->nTri = n;
	obj->pTri = Mem_Alloc(n * sizeof(MallaD3DTri));
}

void MallaD3DObj_AllocVert(MallaD3DObj *obj, int n) {
	CHECK(obj && !obj->nVert && !obj->pVert);
	obj->nVert = n;
	obj->pVert = Mem_Alloc(n * sizeof(MallaD3DVert));
}

void MallaD3DObj_Free(MallaD3DObj *obj) {
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

void __thiscall CMallaD3D_Invalidate(CMallaD3D *self, void *dev) {
	if (self->next) {
		((mallaD3DInvalidate_t)self->next->vftable[MALLAD3D_VF_INVALIDATE])(self->next, dev);
	}
	MallaD3D_Release(&self->pVertVB);
	MallaD3D_Release(&self->pVertIB);
	MallaD3D_Release(&self->pc0);
	MallaD3D_Release(&self->pc4);
}

void __thiscall CMallaD3D_GetBox(CMallaD3D *self, box_t *out, byte recalc) {
	if (recalc) {
		CMallaD3D_CalcBox(self, out, 1);
		return;
	}
	*out = self->box;
}

int __thiscall CMallaD3D_Restore(CMallaD3D *self, void *dev) {
	int hr = CMallaD3D_CreateVB(self);
	if (self->next) {
		return ((mallaD3DRestore_t)self->next->vftable[MALLAD3D_VF_RESTORE])(self->next, dev);
	}
	return hr;
}

byte __thiscall CMallaD3D_Read(CMallaD3D *self, CArchIO *f, MallaD3D_FileCtx *ctx) {
	char tmp[256];
	uint32_t n;

	CMallaD3D_SetNodo(self, ctx->nodo);
	CArchIO_BeginBlock(f, "MD3D");
	CHECK(CArchIO_ReadInt(f) == 1);
	CArchIO_ReadString(f, tmp, sizeof(tmp));
	CHECK(!strcmp(tmp, self->nodo->name));
	Array_Read6(f, (void **)&self->pTri, &self->nTri);
	CListaVert_Read(&self->vert, f);
	Array_Read2(f, (void **)&self->vertOrig, &n);
	CHECK(!self->vertOrig || n == (uint32_t)self->vert.nVert);
	CArchIO_Read4(f, &self->triBase);
	/* the copy at rva 0x2af4e0 */
	Array_Read10(f, &self->pSeg, &self->nSeg);
	Array_Read8(f, &self->p40, &self->n34);
	Array_ReadC(f, (void **)&self->pPos, &self->nPos);
	Array_Read4(f, (void **)&self->triFlags, &n);
	CHECK(!self->triFlags || n == self->nTri);
	self->skinRef = CArchIO_ReadBool(f);
	if (!self->skinRef) {
		Array_Read10(f, (void **)&self->pSkin, &self->nSkin);
	} else {
		/* the skin of the first mesh of the node */
		CMallaD3D *first = (CMallaD3D *)self->nodo->obj3D;
		if (first->clase != CLASE_OBJ3D_MALLAD3D) {
			CHECK(!"devNULL");
			first = NULL;
		}
		self->pSkin = first->pSkin;
		self->nSkin = first->nSkin;
		CHECK(self->skinRef && self->pSkin);
	}
	int i = CArchIO_ReadInt(f);
	if (i != -1) {
		CTabla *mats = ctx->mats;
		CHECK(mats->p && (uint32_t)i < (uint32_t)mats->n);
		self->mat = ((CMaterial **)mats->p)[i];
	}
	self->b50 = CArchIO_ReadBool(f);
	self->b51 = CArchIO_ReadBool(f);
	CArchIO_Read(f, &self->box, sizeof(self->box));
	CArchIO_Read(f, &self->ext, sizeof(self->ext));
	CArchIO_Read(f, &self->center, sizeof(self->center));
	if (CArchIO_ReadInt(f)) {
		CMallaD3D *m = operator_new(sizeof(CMallaD3D));
		if (m) {
			m = CMallaD3D_ctor(m);
		}
		self->next = m;
		((mallaD3DFile_t)m->vftable[MALLAD3D_VF_READ])(m, f, ctx);
	}
	CArchIO_Error(f, "MD3D");
	return 1;
}

void __fastcall CMallaD3D_CheckList(CMallaD3D *self) {
	do {
		CHECK(self->vert.nVert > 0);
		CHECK((int)self->nTri > 0);
		self = self->next;
	} while (self);
}

int __thiscall CMallaD3D_Draw(CMallaD3D *self, void *dev, void *ctx) {
	(void)dev;
	return MallaD3D_Render(self, ctx);
}

byte __thiscall CMallaD3D_Write(CMallaD3D *self, CArchIO *f, MallaD3D_FileCtx *ctx) {
	CHECK(self->nodo == ctx->nodo);
	CArchIO_BeginBlockWrite(f, "MD3D");
	CArchIO_WriteInt(f, 1);
	CArchIO_WriteString(f, self->nodo->name);
	Array_Write6(f, self->pTri, self->nTri);
	CListaVert_Write(&self->vert, f);
	Array_Write2(f, self->vertOrig, self->vert.nVert);
	CArchIO_WriteInt(f, self->triBase);
	Array_Write10(f, self->pSeg, self->nSeg);
	Array_Write8(f, self->p40, self->n34);
	Array_WriteC(f, self->pPos, self->nPos);
	CHECK(!self->triFlags);
	Array_Write4(f, self->triFlags, self->nTri);
	CArchIO_WriteInt(f, self->skinRef);
	if (!self->skinRef) {
		Array_Write10(f, self->pSkin, self->nSkin);
	}
	/* the index of the material in the table, -1 for none */
	CTabla *mats = ctx->mats;
	int i;
	for (i = 0; i < mats->n; i++) {
		if (((CMaterial **)mats->p)[i] == self->mat) {
			break;
		}
	}
	if (i >= mats->n) {
		i = -1;
	}
	CArchIO_WriteInt(f, i);
	if (i < 0) {
		E3D_LogInfo(g_FileLog, "WARN: Null material in object: %s", self->nodo->name);
		CHECK(self->nodo->flags & NODO_NO_RENDER);
		self->mat = NULL;
	}
	CArchIO_WriteInt(f, self->b50);
	CArchIO_WriteInt(f, self->b51);
	CArchIO_Write(f, &self->box, sizeof(self->box));
	CArchIO_Write(f, &self->ext, sizeof(self->ext));
	CArchIO_Write(f, &self->center, sizeof(self->center));
	CArchIO_WriteInt(f, self->next != NULL);
	if (self->next) {
		((mallaD3DFile_t)self->next->vftable[MALLAD3D_VF_WRITE])(self->next, f, ctx);
	}
	CArchIO_EndBlockWrite(f, "MD3D");
	return 1;
}

void __thiscall CMallaD3D_SetRender(CMallaD3D *self, void *nodoR) {
	MallaD3D_NodoR *r = nodoR;
	int canal = 3;
	void *eva = CNodo3D_TrackData(self->nodo, 3);

	if (!eva) {
		canal = 2;
		eva = CNodo3D_TrackData(self->nodo, 2);
	}
	if (!r) {
		CHECK(!eva || r);
		return;
	}
	r->eva = eva;
	if (!eva) {
		return;
	}
	MallaD3D_Canal *c = (MallaD3D_Canal *)CNodo3D_Channel(self->nodo, canal);
	r->canal = canal;
	r->s9a = (int16_t)c->i4;
	if (!c->anim[0xc] && !c->anim[0xd]) {
		r->eva = NULL;
	}
	CHECK(!self->next);
}

int MallaD3D_CompareVert(const void *a, const void *b) {
	const MallaD3DVert *va = a, *vb = b;
	if (va->bone != vb->bone) {
		return (int)va->bone - (int)vb->bone;
	}
	int r = memcmp(&va->v, &vb->v, sizeof(va->v));
	return r < 0 ? -1 : r > 0;
}

vec3_t *__thiscall CMallaD3D_TriNormal(CMallaD3D *self, vec3_t *out, uint32_t i) {
	vec3_t a, b, c, n;

	CHECK(i < self->nTri);
	CHECK(self->pTri);
	const uint16_t *t = self->pTri[i];
	CListaVert_GetPos(&self->vert, &a, t[0]);
	CListaVert_GetPos(&self->vert, &b, t[1]);
	CListaVert_GetPos(&self->vert, &c, t[2]);
	float e1x = b.x - a.x, e1y = b.y - a.y, e1z = b.z - a.z;
	float e2x = c.x - a.x, e2y = c.y - a.y, e2z = c.z - a.z;
	n.x = e1y * e2z - e1z * e2y;
	n.y = e1z * e2x - e1x * e2z;
	n.z = e1x * e2y - e1y * e2x;
	Malla_Normalize(&n);
	*out = n;
	return out;
}

void __thiscall CMallaD3D_CalcBox(CMallaD3D *self, box_t *out, byte first) {
	CMallaD3D_CalcCenter(self);
	self->box = self->ext;
	if (self->next) {
		CMallaD3D_CalcBox(self->next, &self->box, 0);
	}
	/* an identical copy of Box_Merge, rva 0x2b8870 */
	Box_Merge(out, &self->box, first);
}

void __fastcall CMallaD3D_CalcCenter(CMallaD3D *self) {
	CListaVert_GetExtents(&self->vert, &self->ext, 1);
	self->center.x = (self->ext.min.x + self->ext.max.x) * 0.5f;
	self->center.y = (self->ext.max.y + self->ext.min.y) * 0.5f;
	self->center.z = (self->ext.max.z + self->ext.min.z) * 0.5f;
	if (self->pSkin) {
		Vec3_TransformH(&self->center, &self->center, &self->nodo->mObj);
	}
}

/* -1, 0 or 1 like the inline strcmp of the original */
static int MallaD3D_StrCmp(const char *a, const char *b) {
	int r = strcmp(a, b);
	return r < 0 ? -1 : r > 0;
}

int MallaD3D_CompareGrupo(const void *a, const void *b) {
	const MallaD3DGrupo *ga = a, *gb = b;
	CHECK(ga != gb);
	const char *na = ga->mat ? CMaterial_GetMapName(ga->mat, 0) : NULL;
	const char *nb = gb->mat ? CMaterial_GetMapName(gb->mat, 0) : NULL;
	int r;
	if (na && nb) {
		r = MallaD3D_StrCmp(na, nb);
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
static float MallaD3D_PlaneSide(const plane_t *p) {
	return p->n.y * 0.0f + p->n.x * 0.0f + p->n.z * 0.0f + p->d;
}

static float MallaD3D_Len2(const vec3_t *v) {
	return v->x * v->x + v->y * v->y + v->z * v->z;
}

int MallaD3D_CompareTri(const void *a, const void *b) {
	vec3_t ca, cb;
	plane_t pa, pb;

	MallaD3D_TriPlane(a, &ca, &pa);
	MallaD3D_TriPlane(b, &cb, &pb);
	float sa = MallaD3D_PlaneSide(&pa);
	float sb = MallaD3D_PlaneSide(&pb);
	if ((0.0f > sa) != (0.0f > sb)) {
		return (int)(sb - sa);
	}
	return (int)(MallaD3D_Len2(&cb) - MallaD3D_Len2(&ca));
}

int MallaD3D_CompareVertIdx(const void *a, const void *b) {
	return MallaD3D_CompareVert(&g_WeldVert[*(const int *)a], &g_WeldVert[*(const int *)b]);
}

void __thiscall CMallaD3D_Build(CMallaD3D *self, CMallaTri *malla, uint32_t flags) {
	CHECK(malla);
	CHECK(self->vert.nVert == 0 && !self->pTri);
	/* a mesh at 0x30 is used in its place */
	CMallaTri *src = malla->d3d ? (CMallaTri *)malla->d3d : malla;
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
		if (mat && !(mat->params.atrib & MAT_ONESIDED) && !(flags & MALLAD3D_ONESIDED) && mat->params.b38) {
			nBack++;
		}
	}
	int nTmp = nTri + nBack;
	MallaD3DTriTmp *tmp = Mem_Calloc((nTmp + nBack) * sizeof(MallaD3DTriTmp));
	int n = 0;
	for (int i = src->nTri - 1; i >= 0 && nBack; i--) {
		CMaterial *mat = src->pTri[i].mat;
		if (mat && mat->params.b38) {
			MallaD3D_FillTri(&tmp[n], src, i, 0);
			n++;
		}
	}
	for (int i = 0; i < src->nTri; i++) {
		MallaD3D_FillTri(&tmp[n], src, i, 1);
		n++;
	}
	CHECK(n == nTmp);

	/* the groups: the last one while it fits, else the first that fits, else a new one */
	cListaDin *din = LDin_NewBlock(sizeof(MallaD3DGrupo), 0x10);
	MallaD3DGrupo *g = NULL;
	int gi = 0;
	for (int k = 0; k < nTmp; k++) {
		MallaD3DTriTmp *t = &tmp[k];
		if (!MallaD3D_GrupoFits(g, t)) {
			g = NULL;
			for (gi = 0; gi < (int)din->used; gi++) {
				MallaD3DGrupo *c = (MallaD3DGrupo *)(din->mem + gi * din->elemSize);
				if (MallaD3D_GrupoFits(c, t)) {
					g = c;
					break;
				}
			}
			if (!g) {
				g = LDin_AddOne(din);
				memset(g, 0, sizeof(*g));
				g->mat = t->mat;
				gi = din->used - 1;
			}
		}
		g->nTri++;
		t->grupo = gi;
	}
	MallaD3DGrupo *grp = (MallaD3DGrupo *)din->mem;
	for (int k = 0; k < (int)din->used; k++) {
		grp[k].idx = k;
	}
	crt_qsort(din->mem, din->used, din->elemSize, MallaD3D_CompareGrupo);
	int *remap = New_Array(din->used, sizeof(int));
	for (int k = 0; k < (int)din->used; k++) {
		remap[grp[k].idx] = k;
	}
	for (int k = 0; k < nTmp; k++) {
		tmp[k].grupo = remap[tmp[k].grupo];
	}
	operator_delete_sized(remap, sizeof(int));

	/* one object a group, the corners of each triangle as new vertices */
	for (int k = 0; k < (int)din->used; k++) {
		grp[k].idx = MallaD3D_NewObj();
		MallaD3DObj_AllocVert(&g_D3DObj[grp[k].idx], grp[k].nTri * 3);
		MallaD3DObj_AllocTri(&g_D3DObj[grp[k].idx], grp[k].nTri);
		MallaD3DObj_SetMat(&g_D3DObj[grp[k].idx], grp[k].mat);
	}
	for (int k = 0; k < nTmp; k++) {
		MallaD3DTriTmp *t = &tmp[k];
		MallaD3DGrupo *gk = &grp[t->grupo];
		int j = gk->used++;
		MallaD3DObj *o = &g_D3DObj[gk->idx];
		MallaD3DObj_SetVert(o, j * 3, &t->v[0]);
		MallaD3DObj_SetVert(o, j * 3 + 1, &t->v[1]);
		MallaD3DObj_SetVert(o, j * 3 + 2, &t->v[2]);
		MallaD3DTri tri = { t->flags, { (uint16_t)(j * 3), (uint16_t)(j * 3 + 1), (uint16_t)(j * 3 + 2) } };
		MallaD3DObj_SetTri(o, j, &tri);
	}
	LDin_Free(din);
	for (int k = first; k < g_NumD3DObj; k++) {
		MallaD3DObj_Weld(&g_D3DObj[k]);
	}
	Mem_Free(tmp);

	/* one mesh of the list an object */
	CHECK(!self->next);
	CMallaD3D *cur = self;
	for (int k = 0; k < g_NumD3DObj; k++) {
		MallaD3DObj *o = &g_D3DObj[k];
		if (k) {
			CMallaD3D *m = operator_new(sizeof(CMallaD3D));
			if (m) {
				m = CMallaD3D_ctor(m);
			}
			cur->next = m;
			m->b50 = cur->b50;
			cur->next->b51 = cur->b51;
			cur = cur->next;
		}
		cur->mat = o->mat;
		CMallaD3D_AllocTri(cur, o->nTri);
		if (!cur->mat || !cur->mat->params.b3b) {
			CListaVert_SetFormat(&cur->vert, (self->b50 || self->b51) ? VERT_TEX1_B1 : VERT_TEX1);
		} else {
			CListaVert_SetFormat(&cur->vert, g_UsarHWEnvMap ? VERT_TEX1 : VERT_TEX2);
			self->b50 = 0;
			self->b51 = 0;
		}
		CListaVert_Alloc(&cur->vert, o->nVert, 0);
		CHECK(!cur->vertOrig);
		if (flags & MALLAD3D_VERTORIG) {
			cur->vertOrig = New_Array(cur->vert.nVert, sizeof(uint16_t));
		}
		for (int v = 0; v < cur->vert.nVert; v++) {
			const MallaD3DVert *s = &o->pVert[v];
			if (cur->vertOrig) {
				cur->vertOrig[v] = s->index;
			}
			switch (cur->vert.frmt) {
			case VERT_TEX1:
				CListaVert_SetTex1(&cur->vert, v, &s->v);
				break;
			case VERT_TEX2: {
				float x[10] = { s->v.pos.x, s->v.pos.y, s->v.pos.z, s->v.n.x, s->v.n.y, s->v.n.z, s->v.uv.u,
					s->v.uv.v, 0.0f, 0.0f };
				CListaVert_SetTex2(&cur->vert, v, x);
				break;
			}
			case VERT_TEX1_B1: {
				/* a weight of 0 or 1 at random */
				int r = crt_rand();
				float x[9] = { s->v.pos.x, s->v.pos.y, s->v.pos.z, (float)(r & 1), s->v.n.x, s->v.n.y, s->v.n.z,
					s->v.uv.u, s->v.uv.v };
				CListaVert_SetTex1B1(&cur->vert, v, x);
				break;
			}
			default:
				CHECK(0);
				break;
			}
		}
		CHECK(!cur->triFlags);
		if (flags & MALLAD3D_TRIFLAGS) {
			cur->triFlags = New_Array(cur->nTri, sizeof(uint32_t));
		}
		for (uint32_t j = 0; (int)j < (int)cur->nTri; j++) {
			const MallaD3DTri *s = &o->pTri[j];
			CMallaD3D_SetTri(cur, j, s->v[0], s->v[1], s->v[2]);
			if (cur->triFlags) {
				cur->triFlags[j] = s->flags;
			}
		}
		MallaD3DObj_Free(o);
	}
	g_NumD3DObj = 0;

	if (flags & MALLAD3D_POS) {
		self->pPos = New_Array(src->nVert, sizeof(vec3_t));
		self->nPos = src->nVert;
		for (int v = 0; v < src->nVert; v++) {
			vec3_t p;
			self->pPos[v] = *CMallaTri_GetVert(src, &p, v);
		}
		CMallaD3D_CalcSegmentos(self, 1);
	}
	if (src->pSkin) {
		CHECK(self->pSkin == NULL);
		self->pSkin = New_Array(src->nVert, 16);
		self->nSkin = src->nVert;
		self->skinRef = 0;
		for (int v = 0; v < src->nVert; v++) {
			memcpy(self->pSkin + v * 16, src->pSkin + v * 16, 16);
		}
		for (CMallaD3D *m = self->next; m; m = m->next) {
			CHECK(m->pSkin == NULL);
			m->pSkin = self->pSkin;
			m->nSkin = self->nSkin;
			m->skinRef = 1;
		}
	}
}

int __fastcall CMallaD3D_CreateVB(CMallaD3D *self) {
	CHECK(self->vert.nVert > 0);
	CHECK((int)self->nTri > 0);
	if (!self->vert.nVert || !self->pTri) {
		return E_FAIL;
	}
	CHECK(self->vert.nVert > 0);
	CHECK((int)self->nTri > 0);
	CHECK(g_Motor);
	CHECK(!self->pVertVB);
	CHECK(!self->pVertIB);
	self->vbFrmt = self->vert.frmt;
	self->i9c = 0;
	self->ia0 = 0;
	self->ia4 = 0;
	CHECK(Render_CreateVB(&self->pVertVB, &self->vert, 0) == 0);
	CHECK(Render_CreateIB(&self->pVertIB, self->pTri, self->nTri, 0) == 0);
	if (self->pSkin) {
		MallaD3D_Release(&self->pc0);
		MallaD3D_Release(&self->pc4);
	}
	return 0;
}

void __thiscall CMallaD3D_SetNodo(CMallaD3D *self, CNodo3D *nodo) {
	for (CMallaD3D *m = self; m; m = m->next) {
		if (nodo) {
			nodo->Refs++;
		}
		if (m->nodo) {
			m->nodo->Refs--;
			CNodo3D *old = m->nodo;
			if (old->Refs == 0 && old) {
				((nodoDelete_t)old->vftable[0])(old, 1);
			}
			m->nodo = NULL;
		}
		m->nodo = nodo;
	}
}

void MallaD3D_TriPlane(const MallaD3DTriTmp *t, vec3_t *center, plane_t *plane) {
	vec3_t v0 = t->v[0].v.pos, v1 = t->v[1].v.pos, v2 = t->v[2].v.pos;

	CHECK(Plane_Calc(plane, &v0, &v1, &v2));
	center->x = (v0.x + v1.x + v2.x) / 3.0f;
	center->y = (v0.y + v1.y + v2.y) / 3.0f;
	center->z = (v0.z + v1.z + v2.z) / 3.0f;
}

void __thiscall CMallaD3D_AllocTri(CMallaD3D *self, int n) {
	CHECK(n > 0);
	CHECK(!self->pTri);
	self->nTri = n;
	self->pTri = New_Array(n, 6);
}

void MallaD3D_FillTri(MallaD3DTriTmp *t, CMallaTri *m, uint32_t i, byte keep) {
	for (int k = 0; k < 3; k++) {
		int v = CMallaTri_GetTri(m, i)->v[k];
		CMallaTri_GetTriVert(m, &t->v[k].v, i, k);
		t->v[k].index = (uint16_t)v;
		t->v[k].bone = m->pSkin ? ((const byte *)CMallaTri_GetSkin(m, v))[1] : 0;
		t->v[k].pad = 0;
	}
	if (!keep) {
		MallaD3DVert x = t->v[1];
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

void __thiscall CMallaD3D_SetTri(CMallaD3D *self, uint32_t i, uint16_t a, uint16_t b, uint16_t c) {
	CHECK(i < self->nTri);
	CHECK(self->pTri);
	self->pTri[i][0] = a;
	self->pTri[i][1] = b;
	self->pTri[i][2] = c;
}

int MallaD3D_GrupoFits(const MallaD3DGrupo *g, const MallaD3DTriTmp *t) {
	if (g && t && g->mat == t->mat) {
		return g->nTri < g_MaxTriGrupo;
	}
	return 0;
}

int __fastcall CMallaD3D_Check(CMallaD3D *self) {
	CHECK(self->vert.nVert > 0);
	CHECK((int)self->nTri > 0);
	return 0;
}

int MallaD3DObj_Weld(MallaD3DObj *obj) {
	CHECK(obj && obj->pVert && obj->nVert);
	g_WeldVert = obj->pVert;
	int *order = Mem_Alloc(obj->nVert * sizeof(int));
	for (int i = 0; i < obj->nVert; i++) {
		order[i] = i;
	}
	crt_qsort(order, obj->nVert, sizeof(int), MallaD3D_CompareVertIdx);
	MallaD3DVert *out = Mem_Alloc(obj->nVert * sizeof(MallaD3DVert));
	int *remap = Mem_Alloc(obj->nVert * sizeof(int));
	const MallaD3DVert *prev = NULL;
	int n = 0;
	for (int k = 0; k < obj->nVert; k++) {
		const MallaD3DVert *v = &obj->pVert[order[k]];
		if (!prev || MallaD3D_CompareVert(v, prev)) {
			out[n++] = *v;
			prev = v;
		}
		remap[order[k]] = n - 1;
	}
	int removed = obj->nVert - n;
	Mem_Free(obj->pVert);
	if (obj->nVert != n) {
		out = Mem_Realloc(out, n * sizeof(MallaD3DVert));
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
