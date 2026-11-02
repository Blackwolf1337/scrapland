/*
 * The floor and the collision meshes of the engine3d, original source Engine/suelo.cpp (include/scrap/ground.h), with
 * the table.h instances it makes.
 */
#include <string.h>

#include "scrap/rdev.h"
#include "scrap/gvar.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/bsp.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/scene.h"
#include "scrap/trimesh.h"
#include "scrap/material.h"
#include "scrap/e3dworld.h"
#include "scrap/render.h"
#include "scrap/renderground.h"
#include "scrap/ground.h"
#include "scrap/groundportal.h"
#include "scrap/utild3d.h"
#include "scrap/scrapmain.h"
#include "scrap/platform.h"

extern const IObject3DVtbl CGround_vftable;
#define CGROUND_VFTABLE (&CGround_vftable)
#define CCOLLIDEMESH_CTOR 0x2586b0
#define CCOLLIDEMESH_DTOR 0x20eb70
#define CCOLLIDEQUAD_DTOR 0x258ce0
#define CNODO3D_DTOR 0x23d070
#define GROUND_FLT_MAX 3.4028234663852886e+38f

/* the statistics of the queries, cleared with the query number */
extern uint32_t g_CollisionStats[6];	/* 6 */
extern uint32_t g_CollisionStats2;
extern uint32_t g_CollisionCache;
#define g_CollisionCache2 (GAME_ARRAY_IN(uint32_t, g_BspCacheStats, 0))	/* 2 */
/* while a .amc file is read: the meshes by their Param (256), while one is written the next Param */
extern CCollideMesh ** g_LoadMeshList;
extern int g_SaveCollideMeshParam;

/* the accessor of table.h */

typedef void(*objCall_t)(void *self, void *arg);


static void *Ground_Obj(const CGround *self, uint32_t i) {
	TABLE_CHECK(self->nodes, i, self->nNodes);
	return self->nodes[i].obj3D;
}

static CCollideQuad *Heights_Quad(CHeights *alt, uint32_t i) {
	TABLE_CHECK(alt->quads, i, alt->nQuads);
	return &alt->quads[i];
}

CCollideQuad *CHeights_GetCell(CHeights *self, uint32_t x, uint32_t z) {
	return Heights_Cell(self, x, z);
}

static float Vec3_Dist2(const vec3_t *a, const vec3_t *b) {
	float dy = a->y - b->y, dx = a->x - b->x, dz = a->z - b->z;
	return dy * dy + dx * dx + dz * dz;
}

static float Vec3_Len(float x, float y, float z) {
	return (float)__builtin_sqrt((double)(y * y + x * x + z * z));
}

/* v / len, unchanged for length 0 */
static vec3_t Vec3_Scaled(vec3_t v, float len) {
	if ((double)len != 0.0) {
		float k = (float)(1.0 / (double)len);
		v.x = v.x * k;
		v.y = v.y * k;
		v.z = v.z * k;
	}
	return v;
}

/* (a - b) normalized */
static vec3_t Vec3_Dir(const vec3_t *a, const vec3_t *b) {
	vec3_t d = { a->x - b->x, a->y - b->y, a->z - b->z };
	return Vec3_Scaled(d, Vec3_Len(d.x, d.y, d.z));
}

/* table.h */

static void Table_Write(CFileIO *f, CTable *t, int size) {
	CFileIO_WriteInt(f, t->n);
	CFileIO_WriteInt(f, size);
	CFileIO_Write(f, t->p, t->n * size);
}

void Table_Write2(CFileIO *f, CTable *t) {
	Table_Write(f, t, 2);
}

void Table_Write1C(CFileIO *f, CTable *t) {
	Table_Write(f, t, 0x1c);
}

void Table_WriteC(CFileIO *f, CTable *t) {
	Table_Write(f, t, 0xc);
}

/* the floor nodes: an array of CNode3D after its count */
static void Table_DeleteObjs(void *p) {
	ObjRef *first = p;

	if (((int *)p)[-1]) {
		first->vftable->Delete(first, 3);
	} else {
		operator_delete_array((int *)p - 1);
	}
}

void Table_FreeObjs(CTable *t) {
	if (t->p) {
		Table_DeleteObjs(t->p);
		t->p = NULL;
		t->n = 0;
	}
}


/* engine3d/engine/ground.h */

CCollideMesh *CCollideMesh_ctor(CCollideMesh *self) {
	self->Refs = 0;
	memset(&self->Ext, 0, sizeof(self->Ext));
	self->nVert = 0;
	self->pVert = NULL;
	self->nTri = 0;
	self->pTri = NULL;
	self->flags = 0;
	self->sector = 0;
	self->MeshID = 0;
	self->outSector = 0;
	return self;
}

CCollideQuad *CCollideQuad_ctor(CCollideQuad *self) {
	self->pTri = NULL;
	self->nTri = 0;
	self->next = NULL;
	self->Mesh = NULL;
	g_CollisionQuadCount++;
	self->gapBottom = -GROUND_FLT_MAX;
	self->gapTop = -GROUND_FLT_MAX;
	return self;
}

static CCollideQuad *CCollideQuad_New(void) {
	CCollideQuad *q = operator_new(sizeof(CCollideQuad));
	return q ? CCollideQuad_ctor(q) : NULL;
}

void CCollideQuad_dtor(CCollideQuad *self) {
	CCollideQuad *next = self->next;
	if (next) {
		CCollideQuad_dtor(next);
		operator_delete_sized(next, sizeof(*next));
		self->next = NULL;
	}
	if (self->Mesh) {
		self->Mesh->Refs--;
		self->Mesh = NULL;
	}
	g_CollisionQuadCount--;
	Table_Free((CTable *)&self->nTri);
}

CCollideQuad *CCollideQuad_Delete(CCollideQuad *self, uint32_t flags) {
	if (flags & 2) {
		uint32_t *mem = (uint32_t *)self - 1;
		crt_eh_vector_dtor(self, sizeof(*self), *mem, (void *)CCollideQuad_dtor);
		if (flags & 1) {
			operator_delete_array(mem);
		}
		return (CCollideQuad *)mem;
	}
	CCollideQuad_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* the vector deleting destructor of the floor nodes (CNode3D, 0xfc bytes) */
ObjRef *CNode3D_VecDelete(ObjRef *base, byte flags) {
	void *self = (void *)base;
	if (flags & 2) {
		uint32_t *mem = (uint32_t *)self - 1;
		crt_eh_vector_dtor(self, sizeof(CNode3D), *mem, (void *)CNode3D_dtor);
		if (flags & 1) {
			operator_delete_array(mem);
		}
		return (ObjRef *)mem;
	}
	CNode3D_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CNode3D));
	}
	return base;
}

CHeights *CHeights_ctor(CHeights *self) {
	crt_eh_vector_ctor(self->_CollideMesh1, sizeof(CCollideMesh), MAX_COLLIDE_SECTORS, (void *)CCollideMesh_ctor,
		(void *)CCollideMesh_dtor);
	crt_eh_vector_ctor(self->_CollideMesh2, sizeof(CCollideMesh), MAX_COLLIDE_SECTORS, (void *)CCollideMesh_ctor,
		(void *)CCollideMesh_dtor);
	CCollideMesh_ctor(&self->CollideMesh1);
	CCollideMesh_ctor(&self->CollideMesh2);
	memset(&self->ext, 0, sizeof(self->ext) + sizeof(self->dim));
	self->quads = NULL;
	self->nQuads = 0;
	self->MeshID = 0;
	self->nx = 0;
	self->nz = 0;
	self->sizeX = 0.0f;
	self->sizeZ = 0.0f;
	self->invX = 0.0f;
	self->invZ = 0.0f;
	self->outSector = NULL;
	return self;
}

static void Heights_FreeQuads(CHeights *self) {
	if (self->quads) {
		Delete_Objects(self->quads, sizeof(CCollideQuad), (void *)CCollideQuad_dtor);
		self->quads = NULL;
		self->nQuads = 0;
	}
}

void CHeights_dtor(CHeights *self) {
	Heights_FreeQuads(self);
	if (self->outSector) {
		CCollideQuad *q = self->outSector;
		CCollideQuad_dtor(q);
		operator_delete_sized(q, sizeof(*q));
		self->outSector = NULL;
	}
	CHECK(self->CollideMesh1.Refs == 0);
	CHECK(self->CollideMesh2.Refs == 0);
	for (int i = 0; i < MAX_COLLIDE_SECTORS; i++) {
		CHECK(self->_CollideMesh1[i].Refs == 0);
		CHECK(self->_CollideMesh2[i].Refs == 0);
	}
	CHECK(g_CollisionQuadCount == 0);
	Heights_FreeQuads(self);
	CCollideMesh_dtor(&self->CollideMesh2);
	CCollideMesh_dtor(&self->CollideMesh1);
	crt_eh_vector_dtor(self->_CollideMesh2, sizeof(CCollideMesh), MAX_COLLIDE_SECTORS, (void *)CCollideMesh_dtor);
	crt_eh_vector_dtor(self->_CollideMesh1, sizeof(CCollideMesh), MAX_COLLIDE_SECTORS, (void *)CCollideMesh_dtor);
}

/* CGround */

CGround *CGround_ctor(CGround *self) {
	self->class = 0;
	self->vftable = CGROUND_VFTABLE;
	memset(&self->cache, 0, sizeof(self->cache));
	self->nodes = NULL;
	self->nNodes = 0;
	memset(&self->Ext, 0, sizeof(self->Ext));
	memset(&self->CollideExt, 0, sizeof(self->CollideExt));
	CHeights_ctor(&self->heights);
	self->class = CLASS_OBJ3D_GROUND;
	memset(self->traceStats, 0, sizeof(self->traceStats));
	g_QuadTriCacheID = 1;
	memset(&self->cache, 0, sizeof(self->cache));
	memset(g_CollisionStats, 0, 6 * sizeof(uint32_t));
	g_CollisionNComp = 0;
	memset(g_CollisionCache2, 0, 2 * sizeof(uint32_t));
	g_CollisionTestsMax = 0;
	g_CollisionTests = 0;
	g_CollisionTestsTotal = 0;
	g_CollisionStats2 = 0;
	g_CollisionCache = 0;
	E3D_LogDebug(g_FileLog, "CollisionEpsilon: %f", (double)g_CollisionEpsilon);
	E3D_LogDebug(g_FileLog, "CollisionBalance: %f", (double)g_CollisionBalance);
	E3D_LogDebug(g_FileLog, "CollisionCacheSize: %d", 1);
	return self;
}

void CGround_dtor(CGround *self) {
	self->vftable = CGROUND_VFTABLE;
	CHeights_dtor(&self->heights);
	Table_FreeObjs((CTable *)&self->nNodes);
	self->vftable = IOBJECT3D_VFTABLE;
}

IObject3D *CGround_Delete(IObject3D *base, byte flags) {
	CGround *self = (CGround *)base;
	CGround_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return base;
}

void CGround_Nop(CGround *self, void *arg) {
	(void)self;
	(void)arg;
}

static void Ground_NodesCall(CGround *self, void *arg, int slot) {
	for (int i = 0; i < self->nNodes; i++) {
		void *obj = Ground_Obj(self, i);
		((objCall_t)(*(void ***)obj)[slot])(obj, arg);
	}
}

void CGround_NodesVf3(IObject3D *base, void *arg) {
	CGround *self = (CGround *)base;
	Ground_NodesCall(self, arg, 3);
}

void CGround_NodesVf4(IObject3D *base, void *arg) {
	CGround *self = (CGround *)base;
	Ground_NodesCall(self, arg, 4);
}

/* the triangles of the collision meshes */

plane_t *CCollideMesh_TriPlane(CCollideMesh *self, plane_t *out, uint32_t i) {
	CHECK((uint32_t)i < (uint32_t)self->nTri);
	const CCollideTri *tri = &self->pTri[i];
	const vec3_t *v0 = &self->pVert[tri->v[0]], *v1 = &self->pVert[tri->v[1]], *v2 = &self->pVert[tri->v[2]];
	float dz1 = v1->z - v0->z, dy1 = v1->y - v0->y, dx1 = v1->x - v0->x;
	float dy2 = v2->y - v0->y, dz2 = v2->z - v0->z, dx2 = v2->x - v0->x;

	out->n.x = dy1 * dz2 - dz1 * dy2;
	out->n.y = dz1 * dx2 - dx1 * dz2;
	out->n.z = dx1 * dy2 - dy1 * dx2;
	out->n = Vec3_Scaled(out->n, (float)__builtin_sqrt((double)(out->n.x * out->n.x + out->n.y * out->n.y +
		out->n.z * out->n.z)));
	out->d = -(v0->x * out->n.x + v0->y * out->n.y + v0->z * out->n.z);
	return out;
}

void CCollideMesh_TriWithPlane(CCollideMesh *self, CCollideTri *out, int i) {
	const CCollideTri *tri = &self->pTri[i];
	plane_t pl;

	out->ncomp = 0;
	out->plane = *CCollideMesh_TriPlane(self, &pl, i);
	memcpy(out->v, tri->v, sizeof(out->v));
	out->flags = tri->flags;
	out->ncomp = 0;
}

void CCollideMesh_GetTri(CCollideMesh *self, int i, CCollideTri *tri, vec3_t *v0, vec3_t *v1, vec3_t *v2) {
	if (tri) {
		*tri = self->pTri[i];
	}
	if (v0) {
		*v0 = self->pVert[self->pTri[i].v[0]];
	}
	if (v1) {
		*v1 = self->pVert[self->pTri[i].v[1]];
	}
	if (v2) {
		*v2 = self->pVert[self->pTri[i].v[2]];
	}
}

/* the files */

void CCollideMesh_Read(CCollideMesh *self, CFileIO *f) {
	char name[128];

	CFileIO_BeginBlock(f, "CMSH");
	CHECK(CFileIO_ReadInt(f) == 2);
	CHECK(CFileIO_ReadInt(f) == CCOLLIDEMESH_FILESIZE);
	CFileIO_ReadString(f, name, sizeof(name));
	int zone = Sector_Find(name);
	CFileIO_Read(f, &self->flags, 2);
	CFileIO_Read(f, &self->sector, 2);
	CFileIO_Read(f, &self->MeshID, 2);
	CFileIO_Read(f, &self->Param, 1);
	CFileIO_Read(f, &self->outSector, 1);
	CFileIO_Read(f, &self->Ext, sizeof(self->Ext));
	CHECK((uint32_t)(zone + 1) == self->sector);
	CHECK(self->Param < 256);
	CHECK(!g_LoadMeshList[self->Param]);
	g_LoadMeshList[self->Param] = self;
	Table_ReadC(f, (CTable *)&self->nVert);
	Table_Read1C(f, (CTable *)&self->nTri);
	CFileIO_Error(f, "CMSH");
}

void CCollideMesh_Write(CCollideMesh *self, CFileIO *f) {
	CHECK(g_SaveCollideMeshParam < 256);
	self->Param = (byte)g_SaveCollideMeshParam++;
	CFileIO_BeginBlockWrite(f, "CMSH");
	CFileIO_WriteInt(f, 2);
	CFileIO_WriteInt(f, CCOLLIDEMESH_FILESIZE);
	CFileIO_WriteString(f, Sector_GetName(self->sector - 1));
	CFileIO_Write(f, &self->flags, 2);
	CFileIO_Write(f, &self->sector, 2);
	CFileIO_Write(f, &self->MeshID, 2);
	CFileIO_Write(f, &self->Param, 1);
	CFileIO_Write(f, &self->outSector, 1);
	CFileIO_Write(f, &self->Ext, sizeof(self->Ext));
	Table_WriteC(f, (CTable *)&self->nVert);
	Table_Write1C(f, (CTable *)&self->nTri);
	CFileIO_EndBlockWrite(f, "CMSH");
}

/* the "!Mesh && mesh" link of ground.h */
static void CCollideQuad_SetMesh(CCollideQuad *self, CCollideMesh *mesh) {
	if (self->Mesh || !mesh) {
		CHECK(!self->Mesh && mesh);
		if (!mesh) {
			self->Mesh = mesh;
			return;
		}
	}
	mesh->Refs++;
	self->Mesh = mesh;
}

void CCollideQuad_Read(CCollideQuad *self, CFileIO *f) {
	CFileIO_BeginBlock(f, "QUAD");
	CHECK(CFileIO_ReadInt(f) == 1);
	int mesh = CFileIO_ReadInt(f);
	CHECK(mesh < 256);
	CCollideQuad_SetMesh(self, g_LoadMeshList[mesh]);
	self->dwParam = 0;
	Table_Read2(f, (CTable *)&self->nTri);
	if (self->nTri > 0x400) {
		Con_Warnf("WARN: Quad with %d triangles\n", self->nTri);
	}
	CFileIO_Read4(f, &self->maxY);
	CFileIO_Read4(f, &self->minY);
	CFileIO_Read4(f, &self->gapTop);
	CFileIO_Read4(f, &self->gapBottom);
	if (CFileIO_ReadInt(f)) {
		self->next = CCollideQuad_New();
		CCollideQuad_Read(self->next, f);
	}
	CFileIO_Error(f, "QUAD");
}

/* a float as the bits CFileIO_WriteInt writes */
static int32_t Ground_FloatBits(float f) {
	int32_t i;
	memcpy(&i, &f, sizeof(i));
	return i;
}

void CCollideQuad_Write(CCollideQuad *self, CFileIO *f) {
	CFileIO_BeginBlockWrite(f, "QUAD");
	CFileIO_WriteInt(f, 1);
	CFileIO_WriteInt(f, self->Mesh->Param);
	Table_Write2(f, (CTable *)&self->nTri);
	CFileIO_WriteInt(f, Ground_FloatBits(self->maxY));
	CFileIO_WriteInt(f, Ground_FloatBits(self->minY));
	CFileIO_WriteInt(f, Ground_FloatBits(self->gapTop));
	CFileIO_WriteInt(f, Ground_FloatBits(self->gapBottom));
	CFileIO_WriteInt(f, self->next != NULL);
	if (self->next) {
		CCollideQuad_Write(self->next, f);
	}
	CFileIO_EndBlockWrite(f, "QUAD");
}

byte CGround_Read(IObject3D *base, CFileIO *f, MeshD3D_FileCtx *ctx) {
	CGround *self = (CGround *)base;
	(void)ctx;
	CFileIO_BeginBlock(f, "SUEL");
	CHECK(CFileIO_ReadInt(f) == 1);
	CFileIO_Read(f, &self->Ext, sizeof(self->Ext));
	CFileIO_Read(f, &self->Dim, sizeof(self->Dim));
	CFileIO_Read(f, &self->nx, 4);
	CFileIO_Read(f, &self->nz, 4);
	CHECK(!self->nNodes);
	CFileIO_Read(f, &self->CollideTri, 4);
	CFileIO_Read(f, &self->CollideExt, sizeof(self->CollideExt));
	CFileIO_Error(f, "SUEL");
	return 1;
}

byte CGround_Write(IObject3D *base, CFileIO *f, MeshD3D_FileCtx *ctx) {
	CGround *self = (CGround *)base;
	(void)ctx;
	CFileIO_BeginBlockWrite(f, "SUEL");
	CFileIO_WriteInt(f, 1);
	CFileIO_Write(f, &self->Ext, sizeof(self->Ext));
	CFileIO_Write(f, &self->Dim, sizeof(self->Dim));
	CFileIO_Write(f, &self->nx, 4);
	CFileIO_Write(f, &self->nz, 4);
	CHECK(!self->nNodes);
	CFileIO_Write(f, &self->CollideTri, 4);
	CFileIO_Write(f, &self->CollideExt, sizeof(self->CollideExt));
	CFileIO_EndBlockWrite(f, "SUEL");
	return 1;
}

static CFileIO *Ground_Open(const char *name, const char *mode) {
	CFileIO *f = operator_new(sizeof(CFileIO));
	return f ? CFileIO_ctor(f, name, mode) : NULL;
}

static void Ground_Close(CFileIO *f) {
	CFileIO_dtor(f);
	operator_delete_sized(f, sizeof(*f));
}

int CGround_LoadCollision(CGround *self, const char *name) {
	char amc[MAX_FILEPATH];
	CCollideMesh *list[256];
	CHeights *alt = &self->heights;

	Path_Merge(amc, ".amc", name);
	if (File_TimeDiff(amc, name, 0) < 0) {
		E3D_LogBlue(g_FileLog, "WARN: [%s] is older than [%s]", amc, name);
		return 1;
	}
	memset(list, 0, sizeof(list));
	g_LoadMeshList = list;
	CFileIO *f = Ground_Open(amc, "rb");
	if (!f->error) {
		CFileIO_BeginBlock(f, "AMC");
		CHECK(CFileIO_ReadInt(f) == AMC_VER);
		CHECK(CFileIO_ReadInt(f) == 0);
		CFileIO_Read(f, &self->CollideExt, sizeof(self->CollideExt));
		CFileIO_Read(f, &self->CollideTri, 4);
		CFileIO_Read(f, &alt->ext, sizeof(alt->ext));
		CFileIO_Read(f, &alt->dim, sizeof(alt->dim));
		CCollideMesh_Read(&alt->CollideMesh1, f);
		CCollideMesh_Read(&alt->CollideMesh2, f);
		int sectors = Sector_Num() + 1;
		CHECK(CFileIO_ReadInt(f) == sectors);
		for (int i = 0; i < sectors; i++) {
			CHECK(i < MAX_COLLIDE_SECTORS);
			CCollideMesh_Read(&alt->_CollideMesh1[i], f);
			CCollideMesh_Read(&alt->_CollideMesh2[i], f);
		}
		CFileIO_Read4(f, &alt->nx);
		CFileIO_Read4(f, &alt->nz);
		CFileIO_Read4(f, &alt->sizeX);
		CFileIO_Read4(f, &alt->sizeZ);
		CFileIO_Read4(f, &alt->invX);
		CFileIO_Read4(f, &alt->invZ);
		Table_New24((CTable *)&alt->nQuads, CFileIO_ReadInt(f));
		for (int i = 0; i < alt->nQuads; i++) {
			CCollideQuad_Read(Heights_Quad(alt, i), f);
		}
		if (CFileIO_ReadInt(f)) {
			alt->outSector = CCollideQuad_New();
			CCollideQuad_Read(alt->outSector, f);
		}
		CFileIO_Error(f, "AMC");
		CFileIO_BeginBlock(f, "AMC");
		CFileIO_Error(f, "AMC");
		if (!f->error) {
			Con_Verbosef("Collision data loaded: %s\n", amc);
			Con_Verbosef("QuadGrid: %d x %d  (NumTri: %d)  (size: %.2f %.2f)\n", alt->nx, alt->nz,
				self->CollideTri, (double)alt->sizeX, (double)alt->sizeZ);
			Ground_Close(f);
			return 0;
		}
	}
	Ground_Close(f);
	return -1;
}

int CGround_SaveCollision(CGround *self, const char *name) {
	char amc[MAX_FILEPATH];
	CHeights *alt = &self->heights;

	g_SaveCollideMeshParam = 0;
	Path_Merge(amc, ".amc", name);
	CFileIO *f = Ground_Open(amc, "wb");
	if (!f->error) {
		CFileIO_BeginBlockWrite(f, "AMC");
		CFileIO_WriteInt(f, AMC_VER);
		CFileIO_WriteInt(f, 0);
		CFileIO_Write(f, &self->CollideExt, sizeof(self->CollideExt));
		CFileIO_Write(f, &self->CollideTri, 4);
		CFileIO_Write(f, &alt->ext, sizeof(alt->ext));
		CFileIO_Write(f, &alt->dim, sizeof(alt->dim));
		CCollideMesh_Write(&alt->CollideMesh1, f);
		CCollideMesh_Write(&alt->CollideMesh2, f);
		int sectors = Sector_Num() + 1;
		CFileIO_WriteInt(f, sectors);
		for (int i = 0; i < sectors; i++) {
			CHECK(i < MAX_COLLIDE_SECTORS);
			CCollideMesh_Write(&alt->_CollideMesh1[i], f);
			CCollideMesh_Write(&alt->_CollideMesh2[i], f);
		}
		CFileIO_WriteInt(f, alt->nx);
		CFileIO_WriteInt(f, alt->nz);
		CFileIO_WriteInt(f, Ground_FloatBits(alt->sizeX));
		CFileIO_WriteInt(f, Ground_FloatBits(alt->sizeZ));
		CFileIO_WriteInt(f, Ground_FloatBits(alt->invX));
		CFileIO_WriteInt(f, Ground_FloatBits(alt->invZ));
		CFileIO_WriteInt(f, alt->nQuads);
		for (int i = 0; i < alt->nQuads; i++) {
			CCollideQuad_Write(Heights_Quad(alt, i), f);
		}
		CFileIO_WriteInt(f, alt->outSector != NULL);
		if (alt->outSector) {
			CCollideQuad_Write(alt->outSector, f);
		}
		CFileIO_EndBlockWrite(f, "AMC");
		CFileIO_BeginBlockWrite(f, "AMC");
		CFileIO_EndBlockWrite(f, "AMC");
		if (!f->error) {
			Ground_Close(f);
			return 0;
		}
	}
	Con_Bluef("SaveCollision: Could not write to file: '%s'\n", amc);
	Ground_Close(f);
	return -1;
}

/* the queries */

void CHeights_SetParam(CHeights *self, byte param) {
	for (int i = 0; i < self->nQuads; i++) {
		for (CCollideQuad *q = Heights_Quad(self, i); q; q = q->next) {
			q->Mesh->Param = param;
		}
	}
	for (CCollideQuad *q = self->outSector; q; q = q->next) {
		q->Mesh->Param = param;
	}
}

static void Ground_ClearNComp(CCollideMesh *mesh) {
	for (int i = 0; i < mesh->nTri; i++) {
		mesh->pTri[i].ncomp = 0;
	}
}

void CGround_NewQuery(CGround *self) {
	CHeights *alt = &self->heights;
	uint32_t tests = g_CollisionTests;

	g_CollisionTestsTotal += tests;
	g_CollisionNComp++;
	if (g_CollisionTestsMax > tests) {
		tests = g_CollisionTestsMax;
	}
	g_CollisionTestsMax = tests;
	g_CollisionTests = 0;
	if (g_CollisionNComp) {
		return;
	}
	g_CollisionNComp = 1;
	int quads = 0, meshes = 0;
	memset(&g_CollisionStats[4], 0, 2 * sizeof(uint32_t));
	g_CollisionStats2 = 0;
	memset(g_CollisionStats, 0, 4 * sizeof(uint32_t));
	CHeights_SetParam(alt, 0);
	for (int i = 0; i < alt->nQuads; i++) {
		for (CCollideQuad *q = Heights_Quad(alt, i); q; q = q->next) {
			quads++;
			if (!q->Mesh->Param) {
				q->Mesh->Param = 1;
				meshes++;
				Ground_ClearNComp(q->Mesh);
			}
		}
	}
	for (CCollideQuad *q = alt->outSector; q; q = q->next) {
		quads++;
		CHECK(!q->Mesh->Param);
		meshes++;
		q->Mesh->Param = 1;
		Ground_ClearNComp(q->Mesh);
	}
	CHECK(quads == g_CollisionQuadCount);
	E3D_LogDebug(g_FileLog, "Reset NCOMP of collision triangles [%d Quads, %d Meshes]", quads, meshes);
}

/* the geometry */

byte Ground_PointInTri(const vec3_t *p, const vec3_t *a, const vec3_t *b, const vec3_t *c) {
	vec3_t n = Ground_EdgeCross(p, a, b);
	vec3_t m = Ground_EdgeCross(p, b, c);

	float d1 = n.x * m.x + n.y * m.y + n.z * m.z;
	if (0.0f > d1) {
		return 0;
	}
	vec3_t k = Ground_EdgeCross(p, c, a);
	if (0.0f > k.x * n.x + k.y * n.y + k.z * n.z) {
		return 0;
	}
	/* p on the line of a b: the third side decides */
	if (d1 == 0.0f && n.x == 0.0f && n.y == 0.0f && n.z == 0.0f && 0.0f > k.x * m.x + k.y * m.y + k.z * m.z) {
		return 0;
	}
	return 1;
}

float Ground_DistPointSeg(const vec3_t *a, const vec3_t *b, const vec3_t *p, vec3_t *out) {
	float ax = a->x, ay = a->y, az = a->z;
	float dx = b->x - ax, dy = b->y - ay, dz = b->z - az;
	vec3_t u = Vec3_Scaled((vec3_t){ dx, dy, dz }, Vec3_Len(dx, dy, dz));

	float pd = -(p->y * u.y + p->x * u.x + p->z * u.z);
	float t = -(ay * u.y + ax * u.x + az * u.z + pd);
	if (0.0f >= t) {
		if (out) {
			*out = *a;
		}
		return (float)__builtin_sqrt((double)Vec3_Dist2(p, a));
	}
	if (t * t > dy * dy + dx * dx + dz * dz) {
		if (out) {
			*out = *b;
		}
		return (float)__builtin_sqrt((double)Vec3_Dist2(p, b));
	}
	vec3_t c = { ax + u.x * t, ay + u.y * t, az + u.z * t };
	if (out) {
		*out = c;
	}
	return (float)__builtin_sqrt((double)Vec3_Dist2(p, &c));
}

float Ground_Dist2PointSeg(const vec3_t *a, const vec3_t *b, const vec3_t *p, vec3_t *out) {
	float dx = b->x - a->x, dy = b->y - a->y, dz = b->z - a->z;
	float len2 = dy * dy + dx * dx + dz * dz;

	if (len2 == 0.0f) {
		if (out) {
			*out = *a;
		}
		return Vec3_Dist2(p, a);
	}
	float pd = -(p->x * dx + p->y * dy + p->z * dz);
	float t = -(a->y * dy + a->x * dx + a->z * dz + pd);
	if (0.0f >= t) {
		if (out) {
			*out = *a;
		}
		return Vec3_Dist2(p, a);
	}
	if (t > len2) {
		if (out) {
			*out = *b;
		}
		return Vec3_Dist2(p, b);
	}
	vec3_t c = { dx * t / len2 + a->x, dy * t / len2 + a->y, dz * t / len2 + a->z };
	if (out) {
		*out = c;
	}
	return Vec3_Dist2(p, &c);
}

static float Plane_Dist(const plane_t *pl, const vec3_t *v) {
	return pl->n.y * v->y + v->x * pl->n.x + pl->n.z * v->z + pl->d;
}

byte Ground_SegPlane(const vec3_t *a, const vec3_t *b, float r, const plane_t *pl, float *t) {
	CHECK(t);
	float da = Plane_Dist(pl, a);
	float db = Plane_Dist(pl, b);
	if (!((double)r > __builtin_fabs((double)da)) && !((double)r > __builtin_fabs((double)db)) &&
		(0.0f > da) == (0.0f > db)) {
		return 0;
	}
	float diff = da - db;
	if ((double)g_CollisionEpsilon > __builtin_fabs((double)diff)) {
		*t = 0.0f;
		return 1;
	}
	if (!(0.0f > da)) {
		r = -r;
	}
	*t = (r + da) / diff;
	return 1;
}

/* the smaller root of a t^2 + b t + c (b already doubled), when it is in [0, 1] */
static byte Ground_Root01(float a, float b, float c, float *t) {
	float disc = b * b - c * (a * 4.0f);
	if (0.0f > disc) {
		return 2;
	}
	float s = (float)__builtin_sqrt((double)disc);
	float t1 = (s - b) / (a + a);
	float t2 = (-b - s) / (a + a);
	float tm = t1 < t2 ? t1 : t2;
	if (0.0f > tm || tm > 1.0f) {
		return 0;
	}
	*t = tm;
	return 1;
}

byte Ground_SegEdge(const vec3_t *a, const vec3_t *b, const vec3_t *p, const vec3_t *q, float r, float *t) {
	CHECK(t);
	float ex = b->x - a->x, ey = b->y - a->y, ez = b->z - a->z;
	float mx = q->x - p->x, my = q->y - p->y, mz = q->z - p->z;
	float wx = p->x - a->x, wy = p->y - a->y, wz = p->z - a->z;
	float ee = ey * ey + ex * ex + ez * ez;
	float mm = my * my + mx * mx + mz * mz;
	float ww = wy * wy + wx * wx + wz * wz;
	float me = my * ey + mx * ex + mz * ez;
	float we = wy * ey + wx * ex + wz * ez;
	float wm = wy * my + wx * mx + wz * mz;
	float d = mm * ee - me * me;
	float b2 = wm * ee - we * me;
	b2 = b2 + b2;

	/* the sphere path against the line of the edge, the touch point inside the edge */
	if (!(9.999999747378752e-05 > __builtin_fabs((double)d))) {
		float tm;
		byte r1 = Ground_Root01(d, b2, (ww - r * r) * ee - we * we, &tm);
		if (r1 == 2) {
			return 0;
		}
		if (r1) {
			float u = ((my * tm + wy) * ey + (mx * tm + wx) * ex + (mz * tm + wz) * ez) / ee;
			if (u >= 0.0f && 1.0f >= u) {
				*t = tm;
				return 1;
			}
		}
	}
	/* against the end a */
	wm = wm + wm;
	if (9.999999747378752e-05 > __builtin_fabs((double)mm)) {
		return 0;
	}
	return Ground_Root01(mm, wm, ww - r * r, t) == 1;
}

byte Ground_SphereTri(const vec3_t *c, float r, const CCollideTri *tri, const CCollideMesh *mesh, vec3_t *res) {
	CHECK(tri && mesh && res);
	float r2 = r * r;
	const plane_t *pl = &tri->plane;
	float d = Plane_Dist(pl, c);
	if (!(r2 > d * d)) {
		return 0;
	}
	vec3_t q = { c->x - pl->n.x * d, c->y - pl->n.y * d, c->z - pl->n.z * d };
	vec3_t v0 = mesh->pVert[tri->v[0]], v1 = mesh->pVert[tri->v[1]], v2 = mesh->pVert[tri->v[2]];
	if (Ground_PointInTri(&q, &v0, &v1, &v2)) {
		*res = q;
		return 1;
	}
	float dd[3];
	vec3_t cp[3];
	dd[0] = Ground_Dist2PointSeg(&v0, &v1, c, &cp[0]);
	dd[1] = Ground_Dist2PointSeg(&v1, &v2, c, &cp[1]);
	dd[2] = Ground_Dist2PointSeg(&v2, &v0, c, &cp[2]);
	int i = dd[1] > dd[0] ? 0 : 1;
	int best = dd[2] > dd[i] ? i : 2;
	if (r2 > dd[best]) {
		*res = cp[best];
		return 1;
	}
	return 0;
}

/* a sphere of radius r around p within the edges of the triangle */
static byte Ground_NearEdges(const vec3_t *p, float r2, const vec3_t *v0, const vec3_t *v1, const vec3_t *v2) {
	return r2 > Ground_Dist2PointSeg(v0, v1, p, NULL) || r2 > Ground_Dist2PointSeg(v1, v2, p, NULL) ||
		r2 > Ground_Dist2PointSeg(v2, v0, p, NULL);
}

static vec3_t Vec3_Lerp(const vec3_t *a, const vec3_t *b, float t) {
	return (vec3_t){ a->x + (b->x - a->x) * t, a->y + (b->y - a->y) * t, a->z + (b->z - a->z) * t };
}

byte Ground_SweepTri(const vec3_t *a, const vec3_t *b, float r, const CCollideTri *tri, const CCollideMesh *mesh,
	vec3_t *res) {
	const plane_t *pl = &tri->plane;
	float t;

	CHECK(res);
	if (!Ground_SegPlane(a, b, r, pl, &t)) {
		return 0;
	}
	float da = Plane_Dist(pl, a);
	float db = Plane_Dist(pl, b);
	vec3_t v0 = mesh->pVert[tri->v[0]], v1 = mesh->pVert[tri->v[1]], v2 = mesh->pVert[tri->v[2]];
	if ((double)r > __builtin_fabs((double)da)) {
		/* the sphere touches the plane at the start */
		vec3_t q = { a->x - pl->n.x * da, a->y - pl->n.y * da, a->z - pl->n.z * da };
		if (Ground_PointInTri(&q, &v0, &v1, &v2)) {
			*res = (0.0f > da ? da > db : db > da) ? *b : *a;
			return 1;
		}
		if (Ground_NearEdges(a, r * r, &v0, &v1, &v2)) {
			*res = *a;
			return 1;
		}
	} else {
		CHECK(t >= 0.0f && t <= 1.0f);
		vec3_t p = Vec3_Lerp(a, b, t);
		float dp = Plane_Dist(pl, &p);
		vec3_t q = { p.x - pl->n.x * dp, p.y - pl->n.y * dp, p.z - pl->n.z * dp };
		if (Ground_PointInTri(&q, &v0, &v1, &v2)) {
			Ground_SegPlane(a, b, g_CollisionEpsilon + r, pl, &t);
			if (0.0f > t) {
				t = 0.0f;
			} else if (t > 1.0f) {
				t = 1.0f;
			}
			*res = Vec3_Lerp(a, b, t);
			return 1;
		}
	}
	if (g_CollisionEpsilon * g_CollisionEpsilon > Vec3_Dist2(a, b)) {
		if (Ground_NearEdges(b, r * r, &v0, &v1, &v2)) {
			*res = *a;
			return 1;
		}
		return 0;
	}
	/* the first touch of an edge */
	float ta = 100.0f, tb = 100.0f, tc = 100.0f;
	Ground_SegEdge(&v0, &v1, a, b, r, &ta);
	Ground_SegEdge(&v1, &v2, a, b, r, &tb);
	Ground_SegEdge(&v2, &v0, a, b, r, &tc);
	float tm = ta < tb ? ta : tb;
	if (!(tc > tm)) {
		tm = tc;
	}
	CHECK(tm >= 0);
	if (0.0f > tm || tm > 1.0f) {
		return 0;
	}
	vec3_t p = Vec3_Lerp(a, b, tm);
	vec3_t back = { a->x - b->x, a->y - b->y, a->z - b->z }, n;
	Vec3_Normalize(&n, &back);
	res->x = n.x * g_CollisionEpsilon + p.x;
	res->y = n.y * g_CollisionEpsilon + p.y;
	res->z = n.z * g_CollisionEpsilon + p.z;
	return 1;
}

/* the traces through the grid of quads */

typedef struct {
	int x0, x1, z0, z1;
	float top, bottom;	/* of the sphere along the path */
} groundArea_t;

static void Ground_Area(CGround *self, groundArea_t *ar, float lox, float hix, float loz, float hiz, float r,
	const vec3_t *a, const vec3_t *b) {
	const CHeights *alt = &self->heights;
	int i;

	i = (int)(((lox - r) - alt->ext.min.x) * alt->invX);
	ar->x0 = i > 0 ? i : 0;
	i = (int)(((loz - r) - alt->ext.min.z) * alt->invZ);
	ar->z0 = i > 0 ? i : 0;
	i = (int)(((hix + r) - alt->ext.min.x) * alt->invX);
	ar->x1 = i >= alt->nx - 1 ? alt->nx - 1 : i;
	i = (int)(((hiz + r) - alt->ext.min.z) * alt->invZ);
	ar->z1 = i >= alt->nz - 1 ? alt->nz - 1 : i;
	if (a->y > b->y) {
		ar->top = a->y + r;
		ar->bottom = b->y - r;
	} else {
		ar->top = b->y + r;
		ar->bottom = a->y - r;
	}
}

static int Ground_SkipQuad(const CCollideQuad *q, const groundArea_t *ar) {
	return (q->Mesh->flags & TRI_ALT_NO_COLLISIONS) || q->minY > ar->top || ar->bottom > q->maxY ||
		(q->gapTop > ar->top && ar->bottom > q->gapBottom);
}

/* the triangle not tested yet in this query: marked */
static int Ground_MarkTri(CCollideTri *tri) {
	if ((tri->flags & TRI_ALT_NO_COLLISIONS) || tri->ncomp == g_CollisionNComp) {
		return 0;
	}
	tri->ncomp = g_CollisionNComp;
	tri->flags |= TRI_ALT_TESTED;
	g_CollisionTests++;
	return 1;
}

byte CGround_CollideSphere(CGround *self, const vec3_t *a, const vec3_t *b, float r, vec3_t *out, vec3_t *hit,
	vec3_t *n) {
	vec3_t A = *a, C = *b, P, best;
	groundArea_t ar;
	CCollideTri *bestTri = NULL;
	float bestDist = GROUND_FLT_MAX, bestCos = -1.0f;

	CGround_NewQuery(self);
	Ground_Area(self, &ar, C.x, C.x, C.z, C.z, r, a, b);
	best = a->y > b->y ? *b : *a;
	for (int z = ar.z0; z <= ar.z1; z++) {
		for (int x = ar.x0; x <= ar.x1; x++) {
			CCollideQuad *quad = Heights_Cell(&self->heights, x, z);
			CHECK(quad);
			for (; quad; quad = quad->next) {
				if (Ground_SkipQuad(quad, &ar)) {
					continue;
				}
				for (int k = 0; k < quad->nTri; k++) {
					CCollideTri *tri = &quad->Mesh->pTri[quad->pTri[k]];
					if (!Ground_MarkTri(tri) || !Ground_SphereTri(&C, r, tri, quad->Mesh, &P)) {
						continue;
					}
					float d = (float)__builtin_sqrt((double)Vec3_Dist2(&P, &C));
					float dx = C.x - P.x, dy = C.y - P.y, dz = C.z - P.z;
					float len = Vec3_Len(dx, dy, dz);
					float ny = (double)len != 0.0 ? (float)(1.0 / (double)len) * dy : dy;
					float c = 1.0f - ny * ny;
					if (g_CollisionEpsilon * 0.01f + bestDist > d && c > bestCos) {
						bestCos = c;
						bestTri = tri;
						bestDist = d;
						best = P;
					}
				}
			}
		}
	}
	if (!bestTri) {
		return 0;
	}
	bestTri->flags |= TRI_ALT_HIT;
	if (out) {
		vec3_t e = { C.x - best.x, C.y - best.y, C.z - best.z };
		float len = Vec3_Len(e.x, e.y, e.z);
		vec3_t u = Vec3_Scaled(e, len);
		float c = 1.0f - u.y * u.y;
		if (0.01f > c) {
			*out = A;
		} else {
			float s = ((r - len) + g_CollisionEpsilon * 0.25f) / c;
			out->x = u.x * s + C.x;
			out->y = C.y + s * 0.0f;
			out->z = u.z * s + C.z;
		}
	}
	if (hit) {
		*hit = best;
	}
	if (n) {
		*n = Vec3_Dir(&C, &best);
	}
	return 1;
}

byte CGround_SphereContact(CGround *self, const vec3_t *a, const vec3_t *b, float r, vec3_t *out,
	vec3_t *hit, vec3_t *n) {
	vec3_t C = *b, P, best;
	groundArea_t ar;
	CCollideTri *bestTri = NULL;
	float bestDist = GROUND_FLT_MAX;

	CGround_NewQuery(self);
	Ground_Area(self, &ar, C.x, C.x, C.z, C.z, r, a, b);
	best = a->y > b->y ? *b : *a;
	for (int z = ar.z0; z <= ar.z1; z++) {
		for (int x = ar.x0; x <= ar.x1; x++) {
			CCollideQuad *quad = Heights_Cell(&self->heights, x, z);
			CHECK(quad);
			for (; quad; quad = quad->next) {
				if (Ground_SkipQuad(quad, &ar)) {
					continue;
				}
				for (int k = 0; k < quad->nTri; k++) {
					CCollideTri *tri = &quad->Mesh->pTri[quad->pTri[k]];
					if (!Ground_MarkTri(tri) || !Ground_SphereTri(&C, r, tri, quad->Mesh, &P)) {
						continue;
					}
					float d = (float)__builtin_sqrt((double)Vec3_Dist2(&P, &C));
					if (bestDist > d) {
						bestDist = d;
						bestTri = tri;
						best = P;
					}
					if (!out && !hit && !n) {
						bestTri->flags |= TRI_ALT_HIT;
						return 1;
					}
				}
			}
		}
	}
	if (!bestTri) {
		return 0;
	}
	bestTri->flags |= TRI_ALT_HIT;
	if (out) {
		vec3_t e = { C.x - best.x, C.y - best.y, C.z - best.z };
		float s = r - Vec3_Len(e.x, e.y, e.z);
		vec3_t u = Vec3_Scaled(e, Vec3_Len(e.x, e.y, e.z));
		float k = s + g_CollisionEpsilon;
		out->x = u.x * k + C.x;
		out->y = u.y * k + C.y;
		out->z = u.z * k + C.z;
	}
	if (hit) {
		*hit = best;
	}
	if (n) {
		*n = Vec3_Dir(&C, &best);
	}
	return 1;
}

byte CGround_SweepSphere(CGround *self, const vec3_t *a, const vec3_t *b, float r, vec3_t *out, vec3_t *hit,
	vec3_t *n) {
	vec3_t A = *a, B = *b, P, best;
	groundArea_t ar;
	CCollideTri *bestTri = NULL;
	float bestDist = GROUND_FLT_MAX;

	CGround_NewQuery(self);
	Ground_Area(self, &ar, A.x < B.x ? A.x : B.x, A.x > B.x ? A.x : B.x, A.z < B.z ? A.z : B.z,
		A.z > B.z ? A.z : B.z, r, a, b);
	best = a->y > b->y ? *b : *a;
	for (int z = ar.z0; z <= ar.z1; z++) {
		for (int x = ar.x0; x <= ar.x1; x++) {
			CCollideQuad *quad = Heights_Cell(&self->heights, x, z);
			CHECK(quad);
			for (; quad; quad = quad->next) {
				if (Ground_SkipQuad(quad, &ar)) {
					continue;
				}
				for (int k = 0; k < quad->nTri; k++) {
					CCollideTri *tri = &quad->Mesh->pTri[quad->pTri[k]];
					if (!Ground_MarkTri(tri) || !Ground_SweepTri(&A, &B, r, tri, quad->Mesh, &P)) {
						continue;
					}
					float d = (float)__builtin_sqrt((double)Vec3_Dist2(&P, &A));
					if (bestDist > d) {
						bestDist = d;
						bestTri = tri;
						best = P;
					}
					if (!out && !hit && !n) {
						bestTri->flags |= TRI_ALT_HIT;
						return 1;
					}
				}
			}
		}
	}
	if (!bestTri) {
		return 0;
	}
	bestTri->flags |= TRI_ALT_HIT;
	if (out) {
		*out = best;
	}
	if (hit) {
		*hit = best;
	}
	if (n) {
		*n = Vec3_Dir(&B, &best);
	}
	return 1;
}

/* building the quads */

static const CCollideTri *Quad_Tri(const CCollideQuad *q, int i) {
	return &q->Mesh->pTri[q->pTri[i]];
}

float CCollideQuad_TriMaxY(CCollideQuad *self, const CCollideTri *tri) {
	const vec3_t *v = self->Mesh->pVert;
	float y0 = v[tri->v[0]].y, y1 = v[tri->v[1]].y, y2 = v[tri->v[2]].y;
	float m = y0 > y1 ? y0 : y1;
	return m > y2 ? m : y2;
}

float CCollideQuad_TriMinY(CCollideQuad *self, const CCollideTri *tri) {
	const vec3_t *v = self->Mesh->pVert;
	float y0 = v[tri->v[0]].y, y1 = v[tri->v[1]].y, y2 = v[tri->v[2]].y;
	float m = y0 < y1 ? y0 : y1;
	return y2 > m ? m : y2;
}

int CCollideQuad_LowestTri(CCollideQuad *self) {
	int best = -1;
	float low = self->maxY;

	for (int i = 0; i < self->nTri; i++) {
		const CCollideTri *tri = Quad_Tri(self, i);
		if (tri->flags & TRI_ALT_NO_COLLISIONS) {
			continue;
		}
		float m = CCollideQuad_TriMinY(self, tri);
		if (low > m) {
			low = m;
			best = i;
		}
	}
	return best;
}

void CCollideQuad_CalcGap(CCollideQuad *self) {
	self->gapBottom = self->minY;
	self->gapTop = self->maxY;
	int cur = CCollideQuad_LowestTri(self);
	if (cur < 0) {
		return;
	}
	self->gapTop = CCollideQuad_TriMinY(self, Quad_Tri(self, cur));
	/* from the lowest triangle up: the triangles that overlap in height are joined, the gap above them is the
	 * distance to the lowest triangle that starts higher */
	CTable used = { 0, NULL };
	int count = 0;
	Table_Alloc1(&used, self->nTri);
	byte *p = used.p;
	CHECK(p);
	memset(p, 0, used.n);
	for (;;) {
		TABLE_CHECK(p, cur, used.n);
		p[cur] = 1;
		count++;
		float top = CCollideQuad_TriMaxY(self, Quad_Tri(self, cur));
		if (count >= self->nTri) {
			if (self->maxY - top > self->gapTop - self->gapBottom) {
				self->gapBottom = top;
				self->gapTop = self->maxY;
			}
			break;
		}
		float best;
	restart:
		best = self->maxY;
		for (int i = 0; i < self->nTri; i++) {
			TABLE_CHECK(p, i, used.n);
			if (p[i]) {
				continue;
			}
			const CCollideTri *tri = Quad_Tri(self, i);
			if (tri->flags & TRI_ALT_NO_COLLISIONS) {
				TABLE_CHECK(p, i, used.n);
				count++;
				p[i] = 1;
				continue;
			}
			float mn = CCollideQuad_TriMinY(self, tri);
			float mx = CCollideQuad_TriMaxY(self, tri);
			if (top >= mx) {
				TABLE_CHECK(p, i, used.n);
				count++;
				p[i] = 1;
				continue;
			}
			if (top >= mn) {
				top = mx;
				TABLE_CHECK(p, i, used.n);
				count++;
				p[i] = 1;
				goto restart;
			}
			if (best > mn) {
				best = mn;
				cur = i;
			}
		}
		if (best - top > self->gapTop - self->gapBottom) {
			self->gapBottom = top;
			self->gapTop = best;
		}
		if (count >= self->nTri) {
			break;
		}
	}
	if (p) {
		operator_delete_array(p);
	}
}

void CCollideQuad_CalcExt(CHeights *self, CCollideQuad *q) {
	(void)self;
	box_t b = { { GROUND_FLT_MAX, GROUND_FLT_MAX, GROUND_FLT_MAX }, { -GROUND_FLT_MAX, -GROUND_FLT_MAX, -GROUND_FLT_MAX } };

	for (int i = 0; i < q->nTri; i++) {
		const CCollideTri *tri = Quad_Tri(q, i);
		for (int k = 0; k < 3; k++) {
			const vec3_t *v = &q->Mesh->pVert[tri->v[k]];
			b.min.x = b.min.x < v->x ? b.min.x : v->x;
			b.min.y = b.min.y < v->y ? b.min.y : v->y;
			b.min.z = b.min.z < v->z ? b.min.z : v->z;
			b.max.x = b.max.x > v->x ? b.max.x : v->x;
			b.max.y = b.max.y > v->y ? b.max.y : v->y;
			b.max.z = b.max.z > v->z ? b.max.z : v->z;
		}
	}
	CHECK(b.max.x >= b.min.x && b.max.y >= b.min.y && b.max.z >= b.min.z);
	float e = g_CollisionEpsilon * 0.1f;
	q->minY = b.min.y - e;
	q->maxY = e + b.max.y;
	CCollideQuad_CalcGap(q);
}

void CCollideQuad_AddAllTris(CHeights *self, CCollideQuad *q) {
	(void)self;
	Table_Alloc2((CTable *)&q->nTri, q->Mesh->nTri);
	for (int i = 0; i < q->nTri; i++) {
		CHECK(i <= CNDX_MAX);
		TABLE_CHECK(q->pTri, i, q->nTri);
		q->pTri[i] = (uint16_t)i;
	}
}

CCollideQuad *CCollideQuad_Find(CHeights *self, CCollideQuad *quad, uint32_t id, CCollideMesh *mesh) {
	(void)self;
	CHECK(quad && mesh);
	if (quad->Mesh == mesh && quad->Mesh->MeshID == id) {
		return quad;
	}
	if (!quad->Mesh) {
		CHECK(id == 0);
		CCollideQuad_SetMesh(quad, mesh);
		quad->dwParam = 0;
		return quad;
	}
	for (; quad->next; quad = quad->next) {
		if (quad->next->Mesh == mesh && mesh->MeshID == id) {
			return quad->next;
		}
	}
	quad->next = CCollideQuad_New();
	CCollideQuad_SetMesh(quad->next, mesh);
	quad->next->dwParam = 0;
	return quad->next;
}

void CHeights_QuadInit(CHeights *self, int unused, const box_t *box, int numTri) {
	(void)unused;
	CHECK(!self->CollideMesh1.pTri && !self->CollideMesh1.pVert);
	CHECK(!self->CollideMesh2.pTri && !self->CollideMesh2.pVert);
	CHECK(!self->MeshID);
	float balance = g_CollisionBalance;
	self->ext = *box;
	self->dim.x = self->ext.max.x - self->ext.min.x;
	self->dim.y = self->ext.max.y - self->ext.min.y;
	self->dim.z = self->ext.max.z - self->ext.min.z;
	double size = __builtin_sqrt((double)(((self->dim.x * self->dim.z) / (float)numTri) * balance));
	double k = 1.0 / size;
	int nx = (int)ceil((double)self->dim.x * k);
	self->nx = nx;
	int nz = (int)ceil((double)self->dim.z * k);
	self->nz = nz;
	self->sizeX = self->dim.x / (float)nx;
	self->sizeZ = self->dim.z / (float)nz;
	self->invX = (float)nx / self->dim.x;
	self->invZ = (float)nz / self->dim.z;
	Table_New24((CTable *)&self->nQuads, nz * nx);
	Con_Verbosef("QuadInit: %d x %d  (NumTri: %d)  (size: %.2f)\n", self->nx, self->nz, numTri, size);
}

void CHeights_QuadAddMesh(CHeights *self, CCollideMesh *mesh, int initTri) {
	int included = 0, discarded = 0, maxTri = 0;

	CHECK(!initTri);
	if (mesh->outSector) {
		if (!self->outSector) {
			self->outSector = CCollideQuad_New();
		}
		CCollideQuad *q = CCollideQuad_Find(self, self->outSector, self->MeshID, mesh);
		CCollideQuad_AddAllTris(self, q);
		CCollideQuad_CalcExt(self, q);
		E3D_LogWhite(g_FileLog, "QuadAddMesh: OutSector: %d included", q->nTri);
		return;
	}
	for (int i = 0; i < self->nQuads; i++) {
		for (CCollideQuad *q = Heights_Quad(self, i); q; q = q->next) {
			q->dwParam = q->nTri;
		}
	}
	for (int t = initTri; t < mesh->nTri; t++) {
		const CCollideTri *src = &mesh->pTri[t];
		vec3_t v0 = mesh->pVert[src->v[0]], v1 = mesh->pVert[src->v[1]], v2 = mesh->pVert[src->v[2]];
		CCollideTri tri;
		plane_t pl;
		tri.ncomp = 0;
		tri.plane = *CCollideMesh_TriPlane(mesh, &pl, t);
		memcpy(tri.v, src->v, sizeof(tri.v));
		tri.flags = src->flags;
		tri.ncomp = 0;
		float m;
		m = v1.x < v2.x ? v1.x : v2.x;
		float minx = v0.x < m ? v0.x : m;
		m = v1.x > v2.x ? v1.x : v2.x;
		float maxx = v0.x > m ? v0.x : m;
		m = v1.z < v2.z ? v1.z : v2.z;
		float minz = v0.z < m ? v0.z : m;
		m = v1.z > v2.z ? v1.z : v2.z;
		float maxz = v0.z > m ? v0.z : m;
		int x0 = (int)((minx - self->ext.min.x) * self->invX);
		int x1 = (int)((maxx - self->ext.min.x) * self->invX);
		int z1 = (int)((maxz - self->ext.min.z) * self->invZ);
		int z0 = (int)((minz - self->ext.min.z) * self->invZ);
		for (int z = z0; z <= z1; z++) {
			for (int x = x0; x <= x1; x++) {
				CCollideQuad *quad = Heights_Cell(self, x, z);
				if (!quad) {
					CHECK(self->MeshID);
					discarded++;
					continue;
				}
				float e = g_CollisionEpsilon * 0.1f;
				vec3_t bmin = { ((float)x * self->sizeX + self->ext.min.x) - e, self->ext.min.y - e,
					((float)z * self->sizeZ + self->ext.min.z) - e };
				vec3_t bmax = { (float)(x + 1) * self->sizeX + self->ext.min.x + e, self->ext.max.y + e,
					(float)(z + 1) * self->sizeZ + self->ext.min.z + e };
				if (!BoxTri(&bmin, &bmax, &tri, mesh->pVert)) {
					discarded++;
					continue;
				}
				CCollideQuad *q = CCollideQuad_Find(self, quad, self->MeshID, mesh);
				if (!q) {
					continue;
				}
				included++;
				CHECK(q->Mesh == mesh);
				CHECK(q->Mesh->MeshID == self->MeshID);
				q->dwParam++;
				if (q->dwParam > (uint32_t)q->nTri) {
					Table_Resize2((CTable *)&q->nTri, q->dwParam * 2);
				}
				CHECK(t <= CNDX_MAX);
				uint32_t k = q->dwParam - 1;
				TABLE_CHECK(q->pTri, k, q->nTri);
				q->pTri[k] = (uint16_t)t;
			}
		}
	}
	for (int i = 0; i < self->nQuads; i++) {
		for (CCollideQuad *q = Heights_Quad(self, i); q; q = q->next) {
			if (q->Mesh != mesh) {
				continue;
			}
			CHECK(q->dwParam < 65536);
			Table_Resize2((CTable *)&q->nTri, q->dwParam);
			CCollideQuad_CalcExt(self, q);
			maxTri = maxTri > q->nTri ? maxTri : q->nTri;
		}
	}
	E3D_LogWhite(g_FileLog, "QuadAddMesh: %d included, %d discarded (%d MaxTriQuad)", included, discarded, maxTri);
	if (!included) {
		Con_Warnf("QuadAddMesh: No triangles included in the collision system.\n");
	}
}

/* the meshes */

static void Ground_NewCacheID(CGround *self) {
	g_QuadTriCacheID++;
	CHECK(g_QuadTriCacheID != 0);
	memset(&self->cache, 0, sizeof(self->cache));
}

/* through the import table of the exe (winmm) */

static const char *const groundVertExpr[3] = { "(tri->v[0] + NumVert <= CNDX_MAX)", "(tri->v[1] + NumVert <= CNDX_MAX)",
	"(tri->v[2] + NumVert <= CNDX_MAX)" };

void CGround_AddMesh(CGround *self, CCollideMesh *mesh, CTriMesh *tm, uint32_t *outID, int *outIni) {
	int degenerate = 0, invalid = 0;

	CHECK(self->heights.nQuads);
	CHECK(tm && (tm->nTri > 0) && (tm->nVert > 0));
	Ground_NewCacheID(self);
	if (outID) {
		self->heights.MeshID++;
		CHECK((uint16_t)self->heights.MeshID != 0);
		*outID = self->heights.MeshID;
		*outIni = mesh->nTri;
		mesh->flags |= TRI_ALT_MESH;
		mesh->MeshID = (uint16_t)self->heights.MeshID;
	}
	int nv = mesh->nVert, nt = mesh->nTri;
	CHECK(mesh->pTri == NULL);
	Table_ResizeC((CTable *)&mesh->nVert, tm->nVert + nv);
	Table_Resize1C((CTable *)&mesh->nTri, tm->nTri + nt);
	for (int i = 0; i < tm->nVert; i++) {
		vec3_t v;
		mesh->pVert[nv + i] = *CTriMesh_GetVert(tm, &v, i);
	}
	for (int i = 0; i < tm->nTri; i++) {
		const CMeshTri *t = CTriMesh_GetTri(tm, i);
		CCollideTri *ct = &mesh->pTri[nt + i];
		ct->ncomp = 0;
		for (int k = 0; k < 3; k++) {
			if ((uint32_t)(t->v[k] + nv) > CNDX_MAX) {
				Con_FatalErrorf("\nFATAL ERROR: Check failed!\n\nError: %s\n\nExpression: %s\n\nFile: '%s'\n"
					"Date: %s\nLine: %d", "Too many vertexes in a sector", groundVertExpr[k], __FILE__,
					__TIMESTAMP__, __LINE__);
			}
		}
		CHECK((uint32_t)(t->v[0] + nv) <= CNDX_MAX);
		CHECK((uint32_t)(t->v[1] + nv) <= CNDX_MAX);
		CHECK((uint32_t)(t->v[2] + nv) <= CNDX_MAX);
		for (int k = 0; k < 3; k++) {
			ct->v[k] = (uint16_t)((uint16_t)t->v[k] + (uint16_t)nv);
		}
		ct->flags = (uint16_t)t->flags;
		/* a material without collisions needs the flag on the triangle */
		CHECK(!(t->mat && (((CMaterial *)t->mat)->params.attrib & MAT_NOCOLLIDE)) || (ct->flags & TRI_ALT_NO_COLLISIONS));
		if ((ct->flags & TRI_ALT_INVALID) == TRI_ALT_INVALID) {
			invalid++;
		}
		plane_t pl;
		CCollideMesh_TriPlane(mesh, &pl, nt + i);
		ct->plane = pl;
		if (pl.n.x == 0.0f && pl.n.y == 0.0f && pl.n.z == 0.0f) {
			ct->flags |= TRI_ALT_INVALID;
			degenerate++;
		}
		if (outID) {
			ct->flags |= TRI_ALT_MESH;
			CHECK((ct->flags & TRI_ALT_NO_COLLISIONS) == 0);
		}
	}
	if (tm->nTri > 0 && (degenerate || invalid)) {
		E3D_LogVerbose(g_FileLog, "ERROR: CollisionMeshAdd: %d caras degeneradas, %d caras no validas  [sector: %d]",
			degenerate, invalid, mesh->sector - 1);
	}
	box_t *b = &mesh->Ext;
	b->min.x = b->min.y = b->min.z = GROUND_FLT_MAX;
	b->max.x = b->max.y = b->max.z = -GROUND_FLT_MAX;
	for (int i = 0; i < mesh->nVert; i++) {
		const vec3_t *v = &mesh->pVert[i];
		b->min.x = b->min.x < v->x ? b->min.x : v->x;
		b->min.y = b->min.y < v->y ? b->min.y : v->y;
		b->min.z = b->min.z < v->z ? b->min.z : v->z;
		b->max.x = b->max.x > v->x ? b->max.x : v->x;
		b->max.y = b->max.y > v->y ? b->max.y : v->y;
		b->max.z = b->max.z > v->z ? b->max.z : v->z;
	}
	CHECK(b->max.x >= b->min.x && b->max.y >= b->min.y && b->max.z >= b->min.z);
	int mem = g_MemStats->bytes;
	uint32_t t0 = Plat_Ticks();
	CHeights_QuadAddMesh(&self->heights, mesh, nt);
	uint32_t t1 = Plat_Ticks();
	E3D_LogWhite(g_FileLog, "QuadAddMesh: %d Kb  [%d tri, %d vtx]  [sector: %d]  [%.2f sec.]",
		(g_MemStats->bytes - mem) / 1024, tm->nTri, tm->nVert, mesh->sector - 1, (double)(t1 - t0) / 1000.0);
}

void CGround_RemoveMesh(CGround *self, CCollideMesh *mesh, int a2, int a3) {
	(void)a2;
	(void)a3;
	Ground_NewCacheID(self);
	for (int i = 0; i < self->heights.nQuads; i++) {
		CCollideQuad *q = Heights_Quad(&self->heights, i);
		while (q) {
			CHECK(q->Mesh != mesh);
			CCollideQuad *next = q->next;
			if (next && next->Mesh == mesh) {
				q->next = next->next;
				next->next = NULL;
				CCollideQuad_dtor(next);
				operator_delete_sized(next, sizeof(*next));
			}
			q = q->next;
		}
	}
}

void CGround_SetMeshCollide(CGround *self, CCollideMesh *mesh, byte on, int MeshID, int IniTri) {
	CHECK(!IniTri);
	CHECK(MeshID);
	Ground_NewCacheID(self);
	if (on) {
		CHECK(mesh->flags & TRI_ALT_NO_COLLISIONS);
		mesh->flags &= ~TRI_ALT_NO_COLLISIONS;
		return;
	}
	CHECK(!(mesh->flags & TRI_ALT_NO_COLLISIONS));
	mesh->flags |= TRI_ALT_NO_COLLISIONS;
}

void CGround_CalcGaps(CGround *self) {
	for (int i = 0; i < self->heights.nQuads; i++) {
		for (CCollideQuad *q = Heights_Quad(&self->heights, i); q; q = q->next) {
			CCollideQuad_CalcGap(q);
		}
	}
}

/* objeto3D.h: the object of a node as the class wanted, NULL (a failed CHECK) for another */
static CTriMesh *Ground_ObjMesh(void *obj) {
	if (((CTriMesh *)obj)->class != CLASS_OBJ3D_TRIMESH) {
		CheckFailed("devNULL", "objeto3D.h", __TIMESTAMP__, __LINE__);
		return NULL;
	}
	return obj;
}

void CGround_BuildSectorMesh(CGround *self, CCollideMesh *mesh, int mode, uint32_t sector) {
	CTriMesh *m = operator_new(sizeof(CTriMesh));
	m = m ? CTriMesh_ctor(m) : NULL;
	if (mode == 2) {
		mesh->flags |= TRI_ALT_NO_COLLISIONS;
	}
	mesh->sector = (uint16_t)sector;
	mesh->outSector = Sector_IsOutside(sector - 1);
	for (int i = 0; i < self->nNodes; i++) {
		CTriMesh *obj = Ground_ObjMesh(Ground_Obj(self, i));
		for (int t = 0; t < obj->nTri; t++) {
			CMeshTri *tri = CTriMesh_GetTri(obj, t);
			uint32_t flags = tri->flags & 0xffff;
			byte skip;
			if ((tri->flags >> 16) != sector && sector != 0xffffffffu) {
				skip = 1;
			} else if (mode == 1 && (flags & TRI_ALT_NO_COLLISIONS)) {
				skip = 1;
			} else if (mode == 2 && !(flags & TRI_ALT_NO_COLLISIONS)) {
				skip = 1;
			} else {
				skip = (flags & TRI_ALT_INVALID) == TRI_ALT_INVALID;
			}
			if (skip) {
				continue;
			}
			CMeshVert v[3];
			memset(v, 0, sizeof(v));
			CTriMesh_GetTriVert(obj, &v[0], t, 0);
			CTriMesh_GetTriVert(obj, &v[1], t, 1);
			CTriMesh_GetTriVert(obj, &v[2], t, 2);
			CTriMesh_AddTriVerts(m, &v[0], &v[1], &v[2], tri->mat, 0, tri->flags);
		}
	}
	if (m->nTri) {
		if (m->pTVert) {
			m->pTVert[0].u = 0.0f;
			m->pTVert[0].v = 0.0f;
		}
		for (int t = 0; t < m->nTri; t++) {
			CMeshTri *tri = CTriMesh_GetTri(m, t);
			tri->mat = NULL;
			memset(tri->n, 0, sizeof(tri->n));
			memset(tri->tv, 0, sizeof(tri->tv));
		}
		CMeshD3D *d3d = operator_new(sizeof(CMeshD3D));
		d3d = d3d ? CMeshD3D_ctorMesh(d3d, m, 0x21, 0) : NULL;
		CTriMesh_Clear(m);
		CTriMesh_AddD3D(m, d3d, 1);
		CGround_AddMesh(self, mesh, m, NULL, NULL);
		if (d3d) {
			d3d->vftable->Delete(&d3d->base, 1);
		}
	}
	m->vftable->Delete(&m->base, 1);
}

void CGround_CreateQuadCollision(CGround *self) {
	CHeights *alt = &self->heights;
	int empty = 0;

	Con_Verbosef("CreateQuadCollision: Building AMC\n");
	CHeights_QuadInit(alt, 0, &self->CollideExt, self->CollideTri);
	int sectors = Sector_Num() + 1;
	for (int i = 0; i < sectors; i++) {
		CHECK(i < MAX_COLLIDE_SECTORS);
		CGround_BuildSectorMesh(self, &alt->_CollideMesh1[i], 1, i);
	}
	for (int z = 0; z < alt->nz; z++) {
		for (int x = 0; x < alt->nx; x++) {
			CCollideQuad *q = Heights_Cell(alt, x, z);
			if (q->Mesh) {
				continue;
			}
			empty++;
			CCollideQuad_SetMesh(q, &alt->CollideMesh1);
			q->dwParam = 0;
			q->minY = self->Ext.max.y;
			q->maxY = self->Ext.max.y;
			q->gapBottom = q->minY;
			q->gapTop = q->minY;
		}
	}
	for (int j = 0; j < alt->nQuads; j++) {
		CHECK(Heights_Quad(alt, j)->Mesh);
	}
	E3D_LogWhite(g_FileLog, "Empty collision quads: %d", empty);
}

/* the floor nodes and the debug drawing */

void CGround_NodesToD3D(CGround *self, int fvf) {
	for (int i = 0; i < self->nNodes; i++) {
		TABLE_CHECK(self->nodes, i, self->nNodes);
		CNode3D_BuildD3D(&self->nodes[i], fvf);
	}
}

void CGround_Draw(IObject3D *base, void *dev, CRenderNode *unused) {
	CGround *self = (CGround *)base;
	(void)unused;
	Render_SetWorldMatrix(g_MatIdentity, 0);
	for (int i = 0; i < self->nNodes; i++) {
		if (!Ground_Obj(self, i)) {
			continue;
		}
		void *obj = Ground_Obj(self, i);
		if (((CTriMesh *)obj)->class != CLASS_OBJ3D_MESHD3D) {
			CheckFailed("devNULL", "objeto3D.h", __TIMESTAMP__, __LINE__);
			obj = NULL;
		}
		Batch_AddMesh(obj, dev);
	}
	Batch_Flush(dev);
}

void Ground_DrawBox(const vec3_t *c, float r, uint32_t color) {
	vec3_t mn = { c->x - r, c->y - r, c->z - r };
	vec3_t mx = { c->x + r, c->y + r, c->z + r };
	Debug_DrawBox(&mn, &mx, color);
}

void Ground_DrawDebug(void) {
	CHECK(g_World);
	CGround *ground = g_World->ObjGround;
	if (ground) {
		CGround_DrawDebug(ground);
	}
}

/* building the floor of a scene */

/* the node flags read here */
#define NODE_FLAG_NOCOLLIDE (1u << 7)	/* its triangles collide only without colision_opt */
#define NODE_FLAG_NOCOLLIDE2 (1u << 13)
#define NODE_FLAG_HIDDEN (1u << 25)
#define NODE_FLAG_EMPTY (1u << 1)
#define NODE_FLAG_GROUND (1u << 3)	/* taken into the floor */
/* the triangles per floor node */
extern int g_GroundTrisPerNode;

/* the mesh of a scene node: its own collision mesh at 0x30 when it has one */
static CTriMesh *Ground_NodeMesh(CNode3D *node) {
	CTriMesh *m = Ground_ObjMesh(node->obj3D);
	if (m->d3d) {
		m = (CTriMesh *)m->d3d;
	}
	return m;
}

static void Ground_LogMemory(int mem, uint32_t t0) {
	uint32_t t1 = Plat_Ticks();
	Con_Verbosef("Map collision memory: %d Kb  [Quads: %d]  [%.2f sec.]\n", (g_MemStats->bytes - mem) / 1024,
		g_CollisionQuadCount, (double)(t1 - t0) / 1000.0);
}

static void Box_Add(box_t *b, const vec3_t *v) {
	b->min.x = b->min.x < v->x ? b->min.x : v->x;
	b->min.y = b->min.y < v->y ? b->min.y : v->y;
	b->min.z = b->min.z < v->z ? b->min.z : v->z;
	b->max.x = b->max.x > v->x ? b->max.x : v->x;
	b->max.y = b->max.y > v->y ? b->max.y : v->y;
	b->max.z = b->max.z > v->z ? b->max.z : v->z;
}

static void Box_Empty(box_t *b) {
	b->min.x = b->min.y = b->min.z = GROUND_FLT_MAX;
	b->max.x = b->max.y = b->max.z = -GROUND_FLT_MAX;
}

static void Box_Widen(box_t *b, float e) {
	b->min.x = b->min.x - e;
	b->min.y = b->min.y - e;
	b->min.z = b->min.z - e;
	b->max.x = b->max.x + e;
	b->max.y = e + b->max.y;
	b->max.z = e + b->max.z;
}

/* the collision flags of a triangle of node with the material mat */
static uint32_t Ground_TriFlags(uint32_t nodeFlags, const CMaterial *mat, byte opt) {
	uint32_t fl = 0;
	if (opt && !(nodeFlags & NODE_FLAG_NOCOLLIDE)) {
		fl = 1;
	}
	if (nodeFlags & NODE_FLAG_NOCOLLIDE) {
		fl |= 2;
	}
	if (nodeFlags & NODE_FLAG_NOCOLLIDE2) {
		fl |= 1;
	}
	uint16_t mf = mat->params.attrib;
	if (mf & 1) {
		fl |= 1;
	}
	if (mat->params.twoSided) {
		fl |= 0x10;
	}
	if (mat->params.transp || (mat->map[1] && !mat->params.reflection)) {
		fl |= 0x20;
	}
	if (mf & 2) {
		fl |= 0x40;
	}
	return fl;
}

void CGround_Build(CGround *self, CScene *scn) {
	const char *name = scn->name;
	char nodeName[128];

	if (scn->fromAmc) {
		int mem = g_MemStats->bytes;
		uint32_t t0 = Plat_Ticks();
		if (CGround_LoadCollision(self, name)) {
			CheckFailed("!LoadCollision( Escena->Nombre )", __FILE__, __TIMESTAMP__, __LINE__);
		}
		Ground_LogMemory(mem, t0);
		Con_Verbosef("Level loaded from SM3+AMC\n");
		return;
	}
	CHECK(scn->Node3D);
	CScene_UpdateNodes(scn);
	/* the box of all meshes and the grid of floor nodes over it */
	Box_Empty(&self->Ext);
	int nTri = 0;
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		if (CNode3D_Class(n) != CLASS_OBJ3D_TRIMESH || (n->flags & NODE_FLAG_HIDDEN)) {
			continue;
		}
		CTriMesh *m = Ground_NodeMesh(n);
		CTriMesh_Transform(m, &n->m);
		CTriMesh_CalcBox(m, &self->Ext, 0);
		nTri += m->nTri;
	}
	box_t *ext = &self->Ext;
	CHECK(ext->max.x >= ext->min.x && ext->max.y >= ext->min.y && ext->max.z >= ext->min.z);
	Box_Widen(ext, g_CollisionEpsilon * 10.0f);
	self->Dim.x = ext->max.x - ext->min.x;
	self->Dim.y = ext->max.y - ext->min.y;
	self->Dim.z = ext->max.z - ext->min.z;
	CHECK(2000000.0 > __builtin_fabs((double)self->Dim.x));
	CHECK(2000000.0 > __builtin_fabs((double)self->Dim.y));
	CHECK(2000000.0 > __builtin_fabs((double)self->Dim.z));
	double k = 1.0 / __builtin_sqrt((double)(((self->Dim.z * self->Dim.x) / (float)nTri) * (float)g_GroundTrisPerNode));
	int nx = (int)ceil((double)self->Dim.x * k);
	self->nx = nx;
	int nz = (int)ceil((double)self->Dim.z * k);
	self->nz = nz;
	Table_NewFC((CTable *)&self->nNodes, nz * nx);
	for (int i = 0; i < self->nNodes; i++) {
		Bprintf(nodeName, sizeof(nodeName), "suelo_%d", i);
		TABLE_CHECK(self->nodes, i, self->nNodes);
		String_Set(&self->nodes[i].name, nodeName);
		CTriMesh *m = operator_new(sizeof(CTriMesh));
		m = m ? CTriMesh_ctor(m) : NULL;
		TABLE_CHECK(self->nodes, i, self->nNodes);
		CNode3D_SetObj3D(&self->nodes[i], (IObject3D *)m);
	}
	/* the triangles into the floor node of the cell of their center */
	self->CollideTri = 0;
	Box_Empty(&self->CollideExt);
	CIniFile *ini = scn->ini;
	byte opt = ini && CIniFile_GetBool(ini, "suelo", "colision_opt", 0) ? 1 : 0;
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		int sector = Sector_OfNode(n);
		byte outside = Sector_IsOutside(sector);
		uint32_t *nodeFlags = &n->flags;
		if (CNode3D_Class(n) != CLASS_OBJ3D_TRIMESH) {
			if (*nodeFlags & NODE_FLAG_EMPTY) {
				CHECK(CNode3D_Class(n) == 0);
				*nodeFlags |= NODE_FLAG_GROUND;
			}
			continue;
		}
		if (*nodeFlags & NODE_FLAG_HIDDEN) {
			continue;
		}
		CTriMesh *m = Ground_NodeMesh(n);
		for (int t = 0; t < m->nTri; t++) {
			CMeshVert v[3];
			memset(v, 0, sizeof(v));
			CTriMesh_GetTriVert(m, &v[0], t, 0);
			CTriMesh_GetTriVert(m, &v[1], t, 1);
			CTriMesh_GetTriVert(m, &v[2], t, 2);
			void *mat = m->pTri[t].mat;
			uint32_t cx = (uint32_t)(int)((((v[0].pos.x + v[1].pos.x + v[2].pos.x) / 3.0f - ext->min.x) /
				self->Dim.x) * (float)self->nx);
			uint32_t cz = (uint32_t)(int)((((v[0].pos.z + v[1].pos.z + v[2].pos.z) / 3.0f - ext->min.z) /
				self->Dim.z) * (float)self->nz);
			if (cx >= (uint32_t)self->nx || cz >= (uint32_t)self->nz) {
				continue;
			}
			CTriMesh *dst = Ground_ObjMesh(Ground_Obj(self, self->nx * cz + cx));
			uint32_t sec = sector + 1;
			uint32_t flags = Ground_TriFlags(*nodeFlags, mat, opt);
			CHECK(flags <= 0xffff);
			CHECK(sec <= 0xffff);
			if (!(flags & TRI_ALT_NO_COLLISIONS) && !outside) {
				self->CollideTri++;
				Box_Add(&self->CollideExt, &v[0].pos);
				Box_Add(&self->CollideExt, &v[1].pos);
				Box_Add(&self->CollideExt, &v[2].pos);
			}
			CTriMesh_AddTriVerts(dst, &v[0], &v[1], &v[2], mat, 1, sec << 16 | flags);
		}
		CNode3D_FreeObj3D(n);
		*nodeFlags |= NODE_FLAG_GROUND;
	}
	CHECK(self->CollideTri);
	box_t *ce = &self->CollideExt;
	CHECK(ce->max.x >= ce->min.x && ce->max.y >= ce->min.y && ce->max.z >= ce->min.z);
	Box_Widen(ce, g_CollisionEpsilon * 10.0f);
	int mem = g_MemStats->bytes;
	uint32_t t0 = Plat_Ticks();
	if (CGround_LoadCollision(self, name)) {
		CGround_CreateQuadCollision(self);
		CGround_SaveCollision(self, name);
	}
	Ground_LogMemory(mem, t0);
}

void CGround_FreeNodes(CGround *self) {
	Table_FreeObjs((CTable *)&self->nNodes);
}

vec3_t *Vec3_Normalize(vec3_t *out, const vec3_t *in) {
	float len = (float)__builtin_sqrt((double)(in->x * in->x + in->y * in->y + in->z * in->z));

	if ((double)len != 0.0) {
		float k = (float)(1.0 / (double)len);
		out->x = in->x * k;
		out->y = in->y * k;
		out->z = in->z * k;
		return out;
	}
	*out = *in;
	return out;
}

/* the lighting of the device meshes of the nodes; node and esc go through unchanged */
void CGround_CalcIllum(void *ground, CNode3D *node, CScene *scn) {
	CGround *self = ground;

	for (int i = 0; i < self->nNodes; i++) {
		TABLE_CHECK(self->nodes, i, self->nNodes);
		if (CNode3D_Class(&self->nodes[i]) == CLASS_OBJ3D_MESHD3D) {
			TABLE_CHECK(self->nodes, i, self->nNodes);
			CMeshD3D_CalcIllum(Obj3D_Cast(self->nodes[i].obj3D, CLASS_OBJ3D_MESHD3D), node, scn);
		}
	}
}

/* the debug view of the collision meshes (c_show and ShowMapSolid) */
extern cGVar GVar_ShowMapSolid;
#define g_ShowMapSolid (GVar_ShowMapSolid.value.i)		/* 0 wireframe, 1 solid, 2 solid with fog */
extern float g_CollideAddColor[4];	/* added to the shaded colors, rgba */
extern CollideVert g_CollideVerts[3];	/* the triangle drawn */
#define g_SectorColors (GAME_ARRAY_IN(const uint32_t, g_BspColors, 4))

/* the shading of the solid view: (0 < s) ? min(1, s) : 0 the way the original branches (NaN to the minss) */
static float Collide_Clamp01(float s) {
	return 0.0f >= s ? 0.0f : Float_MinSS(1.0f, s);
}

void Collide_TriVerts(CollideVert *out, const vec3_t *a, const vec3_t *b, const vec3_t *c, uint32_t color) {
	if (g_ShowMapSolid) {
		float e2x = c->x - a->x, e2y = c->y - a->y, e2z = c->z - a->z;
		float e1y = b->y - a->y, e1z = b->z - a->z, e1x = b->x - a->x;
		vec3_t n;
		n.x = e1y * e2z - e1z * e2y;
		n.y = e1z * e2x - e1x * e2z;
		n.z = e1x * e2y - e1y * e2x;
		float len = (float)__builtin_sqrt((double)(n.y * n.y + n.x * n.x + n.z * n.z));
		if ((double)len != 0.0) {
			float k = (float)(1.0 / (double)len);
			n.x = k * n.x;
			n.y = k * n.y;
			n.z = k * n.z;
		}
		/* the axis of the largest normal component: the texture is projected along it */
		float ax = __builtin_fabsf(n.x), ay = __builtin_fabsf(n.y), az = __builtin_fabsf(n.z);
		int axis = 2;
		if (ax >= ay && ax >= az) {
			axis = 0;
		} else if (ay >= ax && ay >= az) {
			axis = 1;
		}
		const float k255 = 0.003921568859368563f;
		float r = (float)(int)(color >> 16 & 0xff) * k255;
		float g = (float)(int)(color >> 8 & 0xff) * k255;
		float bl = (float)(int)(color & 0xff) * k255;
		/* three directional lights */
		float l1 = Collide_Clamp01((n.x * 0.2f + n.y + n.z * 0.4f) * 0.4f);
		r = l1 + r;
		g = l1 * 0.5f + g;
		bl = l1 * Float_Keep(0.0f) + bl;
		float l2 = Collide_Clamp01((n.z * -0.2f - (n.y * 0.4f + n.x * 0.6f)) * 0.5f);
		r = l2 * 0.5f + r;
		g = l2 * 0.5f + g;
		bl = l2 * 1.2f + bl;
		float l3 = Collide_Clamp01((n.y * Float_Keep(0.0f) + n.x * 0.5f - n.z * 0.5f) * 0.1f);
		float rgba[4];
		rgba[0] = l3 + r + 0.01f + g_CollideAddColor[0];
		rgba[1] = l3 + g + 0.01f + g_CollideAddColor[1];
		rgba[2] = l3 + bl + 0.01f + g_CollideAddColor[2];
		rgba[3] = l3 + (l2 + (l1 + k255)) + 1.0f + g_CollideAddColor[3];
		color = Color_FromFloat(rgba);
		float s = g_ShadowMapScale * g_ShadowMapRes / 10.0f;
		const vec3_t *v[3] = { a, b, c };
		for (int i = 0; i < 3; i++) {
			float u, t;
			if (axis == 0) {
				t = v[i]->z;
				u = v[i]->y;
			} else {
				t = axis == 1 ? v[i]->z : v[i]->y;
				u = v[i]->x;
			}
			out[i].u = u * s;
			out[i].v = t * s;
		}
	}
	out[0].pos = *a;
	out[1].pos = *b;
	out[2].pos = *c;
	out[0].color = color;
	out[1].color = color;
	out[2].color = color;
}

/* the triangles of the mesh by their flags (g_ShowCollision 1: the marked ones, 3: those with flag 4 only, 4: the
 * marks ignored, 5: by sector), the marks cleared; top: the wireframe or the solid view with the back faces in red */
void CCollideMesh_DrawDebug(CCollideMesh *self, int nested) {
	void *dev = g_pd3dDevice;

	Rd_DevSetVertexShader(dev, 0x142);	/* XYZ | DIFFUSE | TEX1 */
	int mode = g_ShowCollision;
	for (int i = 0; i < self->nTri; i++) {
		CCollideTri *t = &self->pTri[i];
		const vec3_t *v = self->pVert;
		const vec3_t *a = &v[t->v[0]], *b = &v[t->v[1]], *c = &v[t->v[2]];
		uint32_t f = t->flags;
		uint32_t color;
		if (mode == 1) {
			if (!(f & 0xf000)) {
				continue;
			}
		} else if (mode == 3) {
			if (!(f & 4)) {
				continue;
			}
		} else if (mode == 4) {
			f &= 0xffff0fff;
		}
		if (f & 0x2000) {
			f &= 4;
			color = (f | 0x60202) << 3;
		} else if (f & 0x1000) {
			f &= 4;
			color = f ? 0x103030 : 0x303010;
		} else if (!(f & 1) && !(self->flags & 1)) {
			f &= 4;
			color = f ? 0x81010 : 0x101008;
		} else {
			f &= 4;
			color = f ? 0x103010 : 0x101030;
		}
		if (mode == 5) {
			if (f) {
				color = 0;
			} else if (!self->sector) {
				color = 0xffffff;
			} else {
				color = g_SectorColors[self->sector % 14];
			}
		}
		Collide_TriVerts(g_CollideVerts, a, b, c, color);
		dev = g_pd3dDevice;
		Rd_DevDrawPrimitiveUP(dev, 4, 1, g_CollideVerts, sizeof(CollideVert));
		t->flags &= 0xfff;
		mode = g_ShowCollision;
	}
	if (nested) {
		return;
	}
	if (!g_ShowMapSolid) {
		dev = g_pd3dDevice;
		Rd_DevSetRenderState(dev, D3DRS_FILLMODE, 2);
		CCollideMesh_DrawDebug(self, 1);
		dev = g_pd3dDevice;
		Rd_DevSetRenderState(dev, D3DRS_FILLMODE, 3);
		if (!g_ShowMapSolid) {
			return;
		}
	}
	dev = g_pd3dDevice;
	Rd_DevSetRenderState(dev, D3DRS_CULLMODE, 2);
	g_CollideAddColor[0] = 1.0f;
	g_CollideAddColor[1] = 0.0f;
	g_CollideAddColor[2] = 0.0f;
	g_CollideAddColor[3] = 1.0f;
	CCollideMesh_DrawDebug(self, 1);
	g_CollideAddColor[0] = 0.0f;
	g_CollideAddColor[1] = 0.0f;
	g_CollideAddColor[2] = 0.0f;
	g_CollideAddColor[3] = 1.0f;
	dev = g_pd3dDevice;
	Rd_DevSetRenderState(dev, D3DRS_CULLMODE, 3);
}

extern cGVar GVar_ShowMapQuads;
#define g_ShowMapQuads (GVar_ShowMapQuads.value.i)		/* ShowMapQuads: the boxes of the quads, 2..6 by kind */
extern cGVar GVar_ShowMapQDist;
#define g_ShowMapQDist (GVar_ShowMapQDist.value.f)	/* ShowMapQDist: only the quads this near the camera, 0 all */
extern cGVar GVar_AnalyzeQuads;
#define g_AnalyzeQuads (GVar_AnalyzeQuads.value.i)		/* AnalizeQuads: 1 the quads checked once */
/* the material of the debug view, a static of CGround_DrawDebug */
extern CMaterial g_CollideMat;
extern int g_CollideMatGuard[2];

/* the atexit destructor of it */
void Ground_FiniCollideMat(void) {
	CMaterial_dtor(&g_CollideMat);
}

#define g_QuadColors (GAME_ARRAY_IN(const uint32_t, g_BspColors, 4))

/* the box of a quad with its cell from x z of the grid */
static void Ground_QuadBox(const CHeights *alt, const CCollideQuad *q, float x0, float x1, float z0, float z1,
	vec3_t *mn, vec3_t *mx) {
	mn->x = x0 * alt->sizeX + alt->ext.min.x;
	mx->x = x1 * alt->sizeX + alt->ext.min.x;
	mn->y = q->minY;
	mx->y = q->maxY;
	mn->z = z0 * alt->sizeZ + alt->ext.min.z;
	mx->z = z1 * alt->sizeZ + alt->ext.min.z;
}

/* ShowMapQuads: the boxes of the quads of the cells */
static void Ground_ShowQuads(CGround *self) {
	CHeights *alt = &self->heights;
	vec3_t eye = World_Get()->cam->pos;
	float maxDist = g_ShowMapQDist;

	for (int z = 0; z < alt->nz; z++) {
		for (int x = 0; x < alt->nx; x++) {
			CCollideQuad *q = Heights_Cell(alt, (uint32_t)x, (uint32_t)z);
			int idx = 8;
			for (; q; q = q->next) {
				int mode = g_ShowMapQuads;
				vec3_t mn, mx;
				Ground_QuadBox(alt, q, (float)x, (float)(x + 1), (float)z, (float)(z + 1), &mn, &mx);
				if (maxDist != 0.0f) {
					float mxm = (mx.x + mn.x) * 0.5f;
					float mym = (mx.y + mn.y) * 0.5f;
					float mzm = (mx.z + mn.z) * 0.5f;
					float dx = eye.x - mxm;
					float dy = (eye.y - mym) * Float_Keep(0.0f);
					float dz = eye.z - mzm;
					if ((float)__builtin_sqrt((double)(dx * dx + dy * dy + dz * dz)) > maxDist) {
						continue;
					}
				}
				uint32_t color = g_QuadColors[idx % 15];
				if (mode == 2) {
					float e = g_CollisionEpsilon * -10.0f;
					mn.x = mn.x - e;
					mn.y = mn.y - e;
					mn.z = mn.z - e;
					mx.x = mx.x + e;
					mx.y = e + mx.y;
					mx.z = mx.z + e;
				} else if (mode == 3) {
					int c = q->nTri;
					color = c < 0x100 ? (uint32_t)c << 8 : ((uint32_t)c & ~0x1fu) << 11 | 0x80;
				} else if (mode == 4) {
					int c = q->nTri;
					if (c < 0x100) {
						continue;
					}
					color = c < 0x400 ? ((uint32_t)c & ~3u) << 14 | 0xff : (((uint32_t)c & ~0x1fu) | 0x1fe000) << 3;
				} else if (mode == 5) {
					int c = q->nTri;
					if (c < 0x100) {
						continue;
					}
					color = c < 0x400 ? 0xffff00 : 0xff0000;
				} else if (mode >= 6) {
					/* the free gap of the quad */
					float e = g_CollisionEpsilon * -10.0f;
					vec3_t gmn, gmx;
					gmn.x = mn.x - e;
					gmn.y = q->gapBottom;
					gmn.z = mn.z - e;
					gmx.x = mx.x + e;
					gmx.y = q->gapTop;
					gmx.z = mx.z + e;
					if (mode == 6) {
						Debug_DrawBox(&gmn, &gmx, color);
					}
					idx++;
					continue;
				}
				Debug_DrawBox(&mn, &mx, color);
				idx++;
			}
		}
	}
}

/* AnalizeQuads: the triangles of a quad crossing its free gap reported */
static void Ground_AnalyzeQuads(CGround *self) {
	CHeights *alt = &self->heights;
	int bad = 0;

	for (int z = 0; z < alt->nz; z++) {
		for (int x = 0; x < alt->nx; x++) {
			for (CCollideQuad *q = Heights_Cell(alt, (uint32_t)x, (uint32_t)z); q; q = q->next) {
				for (int i = 0; i < q->nTri; i++) {
					CCollideMesh *mesh = q->Mesh;
					const CCollideTri *t = &mesh->pTri[q->pTri[i]];
					if (t->flags & 1) {
						continue;
					}
					const vec3_t *v = mesh->pVert;
					float mnY = Float_MinSS(v[t->v[0]].y, v[t->v[1]].y);
					if (!(v[t->v[2]].y > mnY)) {
						mnY = v[t->v[2]].y;
					}
					float mxY = Float_MaxSS(v[t->v[0]].y, v[t->v[1]].y);
					if (!(mxY > v[t->v[2]].y)) {
						mxY = v[t->v[2]].y;
					}
					if (q->gapBottom >= mxY || mnY >= q->gapTop) {
						continue;
					}
					bad++;
					Con_Bluef("SECTOR INCORRECTO: X: %d Z: %d\n", x, z);
				}
			}
		}
	}
	if (bad > 0) {
		Con_Bluef("\nNUMERO SECTORES INCORRECTOS: %d\n", bad);
	} else {
		Con_Printf("SECTORES CORRECTOS\n");
	}
	g_AnalyzeQuads = 0;
}

/* the debug view of the collision: the mode of c_show (6, 7 the solid views, 7 with a blinking background), the
 * quads, the analysis, then the meshes of the sectors seen and the meshes added at run time once each */
void CGround_DrawDebug(CGround *self) {
	int show = g_ShowCollision;

	if (show == 0) {
		g_ShowMapSolid = 0;
	} else if (show == 6) {
		g_ShowMapSolid = 1;
	} else if (show == 7 || g_ShowMapSolid == 2) {
		g_ShowMapSolid = 2;
		Render_Clear(NULL, 1, ((Plat_Ticks() & 0x3fc) | 0x20000) >> 2, 1.0f, 0);
	}
	if (g_ShowMapQuads) {
		Ground_ShowQuads(self);
	}
	if (g_AnalyzeQuads && g_AnalyzeQuads == 1) {
		Ground_AnalyzeQuads(self);
	}
	if (!g_ShowCollision) {
		return;
	}
	Render_SetWorldMatrix(g_MatIdentity, 0);
	if (*g_CollideMatGuard > crt_InitEpoch()) {
		crt_Init_thread_header(g_CollideMatGuard);
		if (*g_CollideMatGuard == -1) {
			CMaterial_ctor(&g_CollideMat);
			crt_atexit(Ground_FiniCollideMat);
			crt_Init_thread_footer(g_CollideMatGuard);
		}
	}
	CMatParams *p = &g_CollideMat.params;
	p->srcBlend = 2;
	p->dstBlend = 2;
	p->lighting = 0;
	p->transp = 1;
	p->zWrite = 0;
	p->attrib = 0x800;
	Render_SetMaterial(&g_CollideMat, 0);
	if (g_ShowMapSolid) {
		Render_SetMaterial(g_MatGround, 0);
	}
	CHeights *alt = &self->heights;
	CCollideMesh_DrawDebug(&alt->CollideMesh1, 0);
	CCollideMesh_DrawDebug(&alt->CollideMesh2, 0);
	for (int s = 0; s < MAX_COLLIDE_SECTORS; s++) {
		if (!g_ShowMapSolid || s <= 0 || Sector_Num() < s || Sector_IsVisible(s - 1)) {
			CCollideMesh_DrawDebug(&alt->_CollideMesh1[s], 0);
			CCollideMesh_DrawDebug(&alt->_CollideMesh2[s], 0);
		}
	}
	CHeights_SetParam(alt, 0);
	for (int i = 0; i < alt->nQuads; i++) {
		TABLE_CHECK(alt->quads, i, alt->nQuads);
		for (CCollideQuad *q = &alt->quads[i]; q; q = q->next) {
			CCollideMesh *mesh = q->Mesh;
			if (mesh->MeshID && !mesh->Param) {
				mesh->Param = 1;
				CCollideMesh_DrawDebug(q->Mesh, 0);
			}
		}
	}
	Debug_DrawBox(&self->CollideExt.min, &self->CollideExt.max, 0xff00);
}

/* the vtable of CGround */
const IObject3DVtbl CGround_vftable = {
	.Delete = CGround_Delete,
	.Update = IObject3D_UpdateNop,
	.Render = CGround_Draw,
	.InitD3D = CGround_NodesVf3,
	.ReleaseD3D = CGround_NodesVf4,
	.Write = CGround_Write,
	.Read = CGround_Read,
};
