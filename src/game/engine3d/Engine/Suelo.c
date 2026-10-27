/*
 * The floor and the collision meshes of the engine3d, original source Engine/suelo.cpp (include/scrap/suelo.h), with
 * the tabla.h instances it makes.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/bsp.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/escena.h"
#include "scrap/mallatri.h"
#include "scrap/material.h"
#include "scrap/mundo.h"
#include "scrap/render.h"
#include "scrap/rendersuelo.h"
#include "scrap/suelo.h"
#include "scrap/sueloportal.h"
#include "scrap/utild3d.h"
#include "scrap/scrapmain.h"
#include "scrap/win32.h"

#define CSUELO_VFTABLE 0x3db13c
#define IOBJETO3D_VFTABLE 0x3d79dc
#define CCOLLIDEMESH_CTOR 0x2586b0
#define CCOLLIDEMESH_DTOR 0x20eb70
#define CCOLLIDEQUAD_DTOR 0x258ce0
#define CNODO3D_DTOR 0x23d070
#define SUELO_FLT_MAX 3.4028234663852886e+38f

/* the statistics of the queries, cleared with the query number */
#define g_CollisionStats ((uint32_t *)Game_Ptr(0x4af888))	/* 6 */
#define g_CollisionStats2 GAME_DATA(uint32_t, 0x4af8a0)
#define g_CollisionCache GAME_DATA(uint32_t, 0x4af7d0)
#define g_CollisionCache2 ((uint32_t *)Game_Ptr(0x4af7e0))	/* 2 */
/* while a .amc file is read: the meshes by their Param (256), while one is written the next Param */
#define g_LoadMeshList GAME_DATA(CCollideMesh **, 0x4af8a8)
#define g_SaveCollideMeshParam GAME_DATA(int, 0x4af8a4)

/* the accessor of tabla.h */

typedef void(__thiscall *objCall_t)(void *self, void *arg);

double crt_ceil(double x);
void __fastcall CSuelo_DrawDebug(CSuelo *self);

static void *Suelo_Obj(const CSuelo *self, uint32_t i) {
	TABLA_CHECK(self->nodos, i, self->nNodos);
	return self->nodos[i].obj3D;
}

static CCollideQuad *Alturas_Cuad(CAlturas *alt, uint32_t i) {
	TABLA_CHECK(alt->cuads, i, alt->nCuads);
	return &alt->cuads[i];
}

CCollideQuad *__thiscall CAlturas_GetCell(CAlturas *self, uint32_t x, uint32_t z) {
	return Alturas_Cell(self, x, z);
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

/* tabla.h */

static void Tabla_Write(CArchIO *f, CTabla *t, int size) {
	CArchIO_WriteInt(f, t->n);
	CArchIO_WriteInt(f, size);
	CArchIO_Write(f, t->p, t->n * size);
}

void Tabla_Write2(CArchIO *f, CTabla *t) {
	Tabla_Write(f, t, 2);
}

void Tabla_Write1C(CArchIO *f, CTabla *t) {
	Tabla_Write(f, t, 0x1c);
}

void Tabla_WriteC(CArchIO *f, CTabla *t) {
	Tabla_Write(f, t, 0xc);
}

static void Tabla_DeleteObjs(void **p) {
	if (((int *)p)[-1]) {
		((objDelete_t)(*(void ***)p)[0])(p, 3);
	} else {
		operator_delete_array((int *)p - 1);
	}
}

void __fastcall Tabla_FreeObjs(CTabla *t) {
	if (t->p) {
		Tabla_DeleteObjs(t->p);
		t->p = NULL;
		t->n = 0;
	}
}


/* engine3d/engine/suelo.h */

CCollideMesh *__fastcall CCollideMesh_ctor(CCollideMesh *self) {
	self->Refs = 0;
	memset(&self->Extens, 0, sizeof(self->Extens));
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

CCollideQuad *__fastcall CCollideQuad_ctor(CCollideQuad *self) {
	self->pTri = NULL;
	self->nTri = 0;
	self->next = NULL;
	self->Mesh = NULL;
	g_CollisionQuadCount++;
	self->gapBottom = -SUELO_FLT_MAX;
	self->gapTop = -SUELO_FLT_MAX;
	return self;
}

static CCollideQuad *CCollideQuad_New(void) {
	CCollideQuad *q = operator_new(sizeof(CCollideQuad));
	return q ? CCollideQuad_ctor(q) : NULL;
}

void __fastcall CCollideQuad_dtor(CCollideQuad *self) {
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
	Tabla_Free((CTabla *)&self->nTri);
}

CCollideQuad *__thiscall CCollideQuad_Delete(CCollideQuad *self, uint32_t flags) {
	if (flags & 2) {
		uint32_t *mem = (uint32_t *)self - 1;
		crt_eh_vector_dtor(self, sizeof(*self), *mem, Game_Ptr(CCOLLIDEQUAD_DTOR));
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

/* the vector deleting destructor of the floor nodes (CNodo3D, 0xfc bytes) */
void *__thiscall CNodo3D_VecDelete(void *self, uint32_t flags) {
	if (flags & 2) {
		uint32_t *mem = (uint32_t *)self - 1;
		crt_eh_vector_dtor(self, sizeof(CNodo3D), *mem, Game_Ptr(CNODO3D_DTOR));
		if (flags & 1) {
			operator_delete_array(mem);
		}
		return mem;
	}
	CNodo3D_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CNodo3D));
	}
	return self;
}

CAlturas *__fastcall CAlturas_ctor(CAlturas *self) {
	crt_eh_vector_ctor(self->_CollideMesh1, sizeof(CCollideMesh), MAX_COLLIDE_SECTORS, Game_Ptr(CCOLLIDEMESH_CTOR),
		Game_Ptr(CCOLLIDEMESH_DTOR));
	crt_eh_vector_ctor(self->_CollideMesh2, sizeof(CCollideMesh), MAX_COLLIDE_SECTORS, Game_Ptr(CCOLLIDEMESH_CTOR),
		Game_Ptr(CCOLLIDEMESH_DTOR));
	CCollideMesh_ctor(&self->CollideMesh1);
	CCollideMesh_ctor(&self->CollideMesh2);
	memset(&self->ext, 0, sizeof(self->ext) + sizeof(self->dim));
	self->cuads = NULL;
	self->nCuads = 0;
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

static void Alturas_FreeCuads(CAlturas *self) {
	if (self->cuads) {
		Delete_Objects(self->cuads, sizeof(CCollideQuad), CCOLLIDEQUAD_DTOR);
		self->cuads = NULL;
		self->nCuads = 0;
	}
}

void __fastcall CAlturas_dtor(CAlturas *self) {
	Alturas_FreeCuads(self);
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
	Alturas_FreeCuads(self);
	CCollideMesh_dtor(&self->CollideMesh2);
	CCollideMesh_dtor(&self->CollideMesh1);
	crt_eh_vector_dtor(self->_CollideMesh2, sizeof(CCollideMesh), MAX_COLLIDE_SECTORS, Game_Ptr(CCOLLIDEMESH_DTOR));
	crt_eh_vector_dtor(self->_CollideMesh1, sizeof(CCollideMesh), MAX_COLLIDE_SECTORS, Game_Ptr(CCOLLIDEMESH_DTOR));
}

/* CSuelo */

CSuelo *__fastcall CSuelo_ctor(CSuelo *self) {
	self->clase = 0;
	self->vftable = Game_Ptr(CSUELO_VFTABLE);
	memset(self->cache, 0, sizeof(self->cache));
	self->nodos = NULL;
	self->nNodos = 0;
	memset(&self->Extens, 0, sizeof(self->Extens));
	memset(&self->CollideExt, 0, sizeof(self->CollideExt));
	CAlturas_ctor(&self->alturas);
	self->clase = CLASE_OBJ3D_SUELO;
	memset(self->traceStats, 0, sizeof(self->traceStats));
	g_QuadTriCacheID = 1;
	memset(self->cache, 0, sizeof(self->cache));
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

void __fastcall CSuelo_dtor(CSuelo *self) {
	self->vftable = Game_Ptr(CSUELO_VFTABLE);
	CAlturas_dtor(&self->alturas);
	Tabla_FreeObjs((CTabla *)&self->nNodos);
	self->vftable = Game_Ptr(IOBJETO3D_VFTABLE);
}

CSuelo *__thiscall CSuelo_Delete(CSuelo *self, byte flags) {
	CSuelo_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

void __thiscall CSuelo_Nop(CSuelo *self, void *arg) {
	(void)self;
	(void)arg;
}

static void Suelo_NodosCall(CSuelo *self, void *arg, int slot) {
	for (int i = 0; i < self->nNodos; i++) {
		void *obj = Suelo_Obj(self, i);
		((objCall_t)(*(void ***)obj)[slot])(obj, arg);
	}
}

void __thiscall CSuelo_NodosVf3(CSuelo *self, void *arg) {
	Suelo_NodosCall(self, arg, 3);
}

void __thiscall CSuelo_NodosVf4(CSuelo *self, void *arg) {
	Suelo_NodosCall(self, arg, 4);
}

/* the triangles of the collision meshes */

plane_t *__thiscall CCollideMesh_TriPlane(CCollideMesh *self, plane_t *out, uint32_t i) {
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

void __thiscall CCollideMesh_TriWithPlane(CCollideMesh *self, CCollideTri *out, int i) {
	const CCollideTri *tri = &self->pTri[i];
	plane_t pl;

	out->ncomp = 0;
	out->plane = *CCollideMesh_TriPlane(self, &pl, i);
	memcpy(out->v, tri->v, sizeof(out->v));
	out->flags = tri->flags;
	out->ncomp = 0;
}

void __thiscall CCollideMesh_GetTri(CCollideMesh *self, int i, CCollideTri *tri, vec3_t *v0, vec3_t *v1, vec3_t *v2) {
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

void __thiscall CCollideMesh_Read(CCollideMesh *self, CArchIO *f) {
	char name[128];

	CArchIO_BeginBlock(f, "CMSH");
	CHECK(CArchIO_ReadInt(f) == 2);
	CHECK(CArchIO_ReadInt(f) == sizeof(CCollideMesh));
	CArchIO_ReadString(f, name, sizeof(name));
	int zona = Sector_Find(name);
	CArchIO_Read(f, &self->flags, 2);
	CArchIO_Read(f, &self->sector, 2);
	CArchIO_Read(f, &self->MeshID, 2);
	CArchIO_Read(f, &self->Param, 1);
	CArchIO_Read(f, &self->outSector, 1);
	CArchIO_Read(f, &self->Extens, sizeof(self->Extens));
	CHECK((uint32_t)(zona + 1) == self->sector);
	CHECK(self->Param < 256);
	CHECK(!g_LoadMeshList[self->Param]);
	g_LoadMeshList[self->Param] = self;
	Tabla_ReadC(f, (CTabla *)&self->nVert);
	Tabla_Read1C(f, (CTabla *)&self->nTri);
	CArchIO_Error(f, "CMSH");
}

void __thiscall CCollideMesh_Write(CCollideMesh *self, CArchIO *f) {
	CHECK(g_SaveCollideMeshParam < 256);
	self->Param = (byte)g_SaveCollideMeshParam++;
	CArchIO_BeginBlockWrite(f, "CMSH");
	CArchIO_WriteInt(f, 2);
	CArchIO_WriteInt(f, sizeof(CCollideMesh));
	CArchIO_WriteString(f, Sector_GetName(self->sector - 1));
	CArchIO_Write(f, &self->flags, 2);
	CArchIO_Write(f, &self->sector, 2);
	CArchIO_Write(f, &self->MeshID, 2);
	CArchIO_Write(f, &self->Param, 1);
	CArchIO_Write(f, &self->outSector, 1);
	CArchIO_Write(f, &self->Extens, sizeof(self->Extens));
	Tabla_WriteC(f, (CTabla *)&self->nVert);
	Tabla_Write1C(f, (CTabla *)&self->nTri);
	CArchIO_EndBlockWrite(f, "CMSH");
}

/* the "!Mesh && mesh" link of suelo.h */
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

void __thiscall CCollideQuad_Read(CCollideQuad *self, CArchIO *f) {
	CArchIO_BeginBlock(f, "QUAD");
	CHECK(CArchIO_ReadInt(f) == 1);
	int mesh = CArchIO_ReadInt(f);
	CHECK(mesh < 256);
	CCollideQuad_SetMesh(self, g_LoadMeshList[mesh]);
	self->dwParam = 0;
	Tabla_Read2(f, (CTabla *)&self->nTri);
	if (self->nTri > 0x400) {
		Con_Warnf("WARN: Quad with %d triangles\n", self->nTri);
	}
	CArchIO_Read4(f, &self->maxY);
	CArchIO_Read4(f, &self->minY);
	CArchIO_Read4(f, &self->gapTop);
	CArchIO_Read4(f, &self->gapBottom);
	if (CArchIO_ReadInt(f)) {
		self->next = CCollideQuad_New();
		CCollideQuad_Read(self->next, f);
	}
	CArchIO_Error(f, "QUAD");
}

/* a float as the bits CArchIO_WriteInt writes */
static int32_t Suelo_FloatBits(float f) {
	int32_t i;
	memcpy(&i, &f, sizeof(i));
	return i;
}

void __thiscall CCollideQuad_Write(CCollideQuad *self, CArchIO *f) {
	CArchIO_BeginBlockWrite(f, "QUAD");
	CArchIO_WriteInt(f, 1);
	CArchIO_WriteInt(f, self->Mesh->Param);
	Tabla_Write2(f, (CTabla *)&self->nTri);
	CArchIO_WriteInt(f, Suelo_FloatBits(self->maxY));
	CArchIO_WriteInt(f, Suelo_FloatBits(self->minY));
	CArchIO_WriteInt(f, Suelo_FloatBits(self->gapTop));
	CArchIO_WriteInt(f, Suelo_FloatBits(self->gapBottom));
	CArchIO_WriteInt(f, self->next != NULL);
	if (self->next) {
		CCollideQuad_Write(self->next, f);
	}
	CArchIO_EndBlockWrite(f, "QUAD");
}

byte __thiscall CSuelo_Read(CSuelo *self, CArchIO *f, void *ctx) {
	(void)ctx;
	CArchIO_BeginBlock(f, "SUEL");
	CHECK(CArchIO_ReadInt(f) == 1);
	CArchIO_Read(f, &self->Extens, sizeof(self->Extens));
	CArchIO_Read(f, &self->Dim, sizeof(self->Dim));
	CArchIO_Read(f, &self->nx, 4);
	CArchIO_Read(f, &self->nz, 4);
	CHECK(!self->nNodos);
	CArchIO_Read(f, &self->CollideTri, 4);
	CArchIO_Read(f, &self->CollideExt, sizeof(self->CollideExt));
	CArchIO_Error(f, "SUEL");
	return 1;
}

byte __thiscall CSuelo_Write(CSuelo *self, CArchIO *f, void *ctx) {
	(void)ctx;
	CArchIO_BeginBlockWrite(f, "SUEL");
	CArchIO_WriteInt(f, 1);
	CArchIO_Write(f, &self->Extens, sizeof(self->Extens));
	CArchIO_Write(f, &self->Dim, sizeof(self->Dim));
	CArchIO_Write(f, &self->nx, 4);
	CArchIO_Write(f, &self->nz, 4);
	CHECK(!self->nNodos);
	CArchIO_Write(f, &self->CollideTri, 4);
	CArchIO_Write(f, &self->CollideExt, sizeof(self->CollideExt));
	CArchIO_EndBlockWrite(f, "SUEL");
	return 1;
}

static CArchIO *Suelo_Open(const char *name, const char *mode) {
	CArchIO *f = operator_new(sizeof(CArchIO));
	return f ? CArchIO_ctor(f, name, mode) : NULL;
}

static void Suelo_Close(CArchIO *f) {
	CArchIO_dtor(f);
	operator_delete_sized(f, sizeof(*f));
}

int __thiscall CSuelo_LoadCollision(CSuelo *self, const char *name) {
	char amc[MAX_RUTA];
	CCollideMesh *list[256];
	CAlturas *alt = &self->alturas;

	Ruta_Merge(amc, ".amc", name);
	if (File_TimeDiff(amc, name, 0) < 0) {
		E3D_LogBlue(g_FileLog, "WARN: [%s] is older than [%s]", amc, name);
		return 1;
	}
	memset(list, 0, sizeof(list));
	g_LoadMeshList = list;
	CArchIO *f = Suelo_Open(amc, "rb");
	if (!f->error) {
		CArchIO_BeginBlock(f, "AMC");
		CHECK(CArchIO_ReadInt(f) == AMC_VER);
		CHECK(CArchIO_ReadInt(f) == 0);
		CArchIO_Read(f, &self->CollideExt, sizeof(self->CollideExt));
		CArchIO_Read(f, &self->CollideTri, 4);
		CArchIO_Read(f, &alt->ext, sizeof(alt->ext));
		CArchIO_Read(f, &alt->dim, sizeof(alt->dim));
		CCollideMesh_Read(&alt->CollideMesh1, f);
		CCollideMesh_Read(&alt->CollideMesh2, f);
		int sectors = Sector_Num() + 1;
		CHECK(CArchIO_ReadInt(f) == sectors);
		for (int i = 0; i < sectors; i++) {
			CHECK(i < MAX_COLLIDE_SECTORS);
			CCollideMesh_Read(&alt->_CollideMesh1[i], f);
			CCollideMesh_Read(&alt->_CollideMesh2[i], f);
		}
		CArchIO_Read4(f, &alt->nx);
		CArchIO_Read4(f, &alt->nz);
		CArchIO_Read4(f, &alt->sizeX);
		CArchIO_Read4(f, &alt->sizeZ);
		CArchIO_Read4(f, &alt->invX);
		CArchIO_Read4(f, &alt->invZ);
		Tabla_New24((CTabla *)&alt->nCuads, CArchIO_ReadInt(f));
		for (int i = 0; i < alt->nCuads; i++) {
			CCollideQuad_Read(Alturas_Cuad(alt, i), f);
		}
		if (CArchIO_ReadInt(f)) {
			alt->outSector = CCollideQuad_New();
			CCollideQuad_Read(alt->outSector, f);
		}
		CArchIO_Error(f, "AMC");
		CArchIO_BeginBlock(f, "AMC");
		CArchIO_Error(f, "AMC");
		if (!f->error) {
			Con_Verbosef("Collision data loaded: %s\n", amc);
			Con_Verbosef("QuadGrid: %d x %d  (NumTri: %d)  (size: %.2f %.2f)\n", alt->nx, alt->nz,
				self->CollideTri, (double)alt->sizeX, (double)alt->sizeZ);
			Suelo_Close(f);
			return 0;
		}
	}
	Suelo_Close(f);
	return -1;
}

int __thiscall CSuelo_SaveCollision(CSuelo *self, const char *name) {
	char amc[MAX_RUTA];
	CAlturas *alt = &self->alturas;

	g_SaveCollideMeshParam = 0;
	Ruta_Merge(amc, ".amc", name);
	CArchIO *f = Suelo_Open(amc, "wb");
	if (!f->error) {
		CArchIO_BeginBlockWrite(f, "AMC");
		CArchIO_WriteInt(f, AMC_VER);
		CArchIO_WriteInt(f, 0);
		CArchIO_Write(f, &self->CollideExt, sizeof(self->CollideExt));
		CArchIO_Write(f, &self->CollideTri, 4);
		CArchIO_Write(f, &alt->ext, sizeof(alt->ext));
		CArchIO_Write(f, &alt->dim, sizeof(alt->dim));
		CCollideMesh_Write(&alt->CollideMesh1, f);
		CCollideMesh_Write(&alt->CollideMesh2, f);
		int sectors = Sector_Num() + 1;
		CArchIO_WriteInt(f, sectors);
		for (int i = 0; i < sectors; i++) {
			CHECK(i < MAX_COLLIDE_SECTORS);
			CCollideMesh_Write(&alt->_CollideMesh1[i], f);
			CCollideMesh_Write(&alt->_CollideMesh2[i], f);
		}
		CArchIO_WriteInt(f, alt->nx);
		CArchIO_WriteInt(f, alt->nz);
		CArchIO_WriteInt(f, Suelo_FloatBits(alt->sizeX));
		CArchIO_WriteInt(f, Suelo_FloatBits(alt->sizeZ));
		CArchIO_WriteInt(f, Suelo_FloatBits(alt->invX));
		CArchIO_WriteInt(f, Suelo_FloatBits(alt->invZ));
		CArchIO_WriteInt(f, alt->nCuads);
		for (int i = 0; i < alt->nCuads; i++) {
			CCollideQuad_Write(Alturas_Cuad(alt, i), f);
		}
		CArchIO_WriteInt(f, alt->outSector != NULL);
		if (alt->outSector) {
			CCollideQuad_Write(alt->outSector, f);
		}
		CArchIO_EndBlockWrite(f, "AMC");
		CArchIO_BeginBlockWrite(f, "AMC");
		CArchIO_EndBlockWrite(f, "AMC");
		if (!f->error) {
			Suelo_Close(f);
			return 0;
		}
	}
	Con_Bluef("SaveCollision: Could not write to file: '%s'\n", amc);
	Suelo_Close(f);
	return -1;
}

/* the queries */

void __thiscall CAlturas_SetParam(CAlturas *self, byte param) {
	for (int i = 0; i < self->nCuads; i++) {
		for (CCollideQuad *q = Alturas_Cuad(self, i); q; q = q->next) {
			q->Mesh->Param = param;
		}
	}
	for (CCollideQuad *q = self->outSector; q; q = q->next) {
		q->Mesh->Param = param;
	}
}

static void Suelo_ClearNComp(CCollideMesh *mesh) {
	for (int i = 0; i < mesh->nTri; i++) {
		mesh->pTri[i].ncomp = 0;
	}
}

void __fastcall CSuelo_NewQuery(CSuelo *self) {
	CAlturas *alt = &self->alturas;
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
	CAlturas_SetParam(alt, 0);
	for (int i = 0; i < alt->nCuads; i++) {
		for (CCollideQuad *q = Alturas_Cuad(alt, i); q; q = q->next) {
			quads++;
			if (!q->Mesh->Param) {
				q->Mesh->Param = 1;
				meshes++;
				Suelo_ClearNComp(q->Mesh);
			}
		}
	}
	for (CCollideQuad *q = alt->outSector; q; q = q->next) {
		quads++;
		CHECK(!q->Mesh->Param);
		meshes++;
		q->Mesh->Param = 1;
		Suelo_ClearNComp(q->Mesh);
	}
	CHECK(quads == g_CollisionQuadCount);
	E3D_LogDebug(g_FileLog, "Reset NCOMP of collision triangles [%d Quads, %d Meshes]", quads, meshes);
}

/* the geometry */

byte Suelo_PointInTri(const vec3_t *p, const vec3_t *a, const vec3_t *b, const vec3_t *c) {
	vec3_t n = Suelo_EdgeCross(p, a, b);
	vec3_t m = Suelo_EdgeCross(p, b, c);

	float d1 = n.x * m.x + n.y * m.y + n.z * m.z;
	if (0.0f > d1) {
		return 0;
	}
	vec3_t k = Suelo_EdgeCross(p, c, a);
	if (0.0f > k.x * n.x + k.y * n.y + k.z * n.z) {
		return 0;
	}
	/* p on the line of a b: the third side decides */
	if (d1 == 0.0f && n.x == 0.0f && n.y == 0.0f && n.z == 0.0f && 0.0f > k.x * m.x + k.y * m.y + k.z * m.z) {
		return 0;
	}
	return 1;
}

float Suelo_DistPointSeg(const vec3_t *a, const vec3_t *b, const vec3_t *p, vec3_t *out) {
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

float Suelo_Dist2PointSeg(const vec3_t *a, const vec3_t *b, const vec3_t *p, vec3_t *out) {
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

byte Suelo_SegPlane(const vec3_t *a, const vec3_t *b, float r, const plane_t *pl, float *t) {
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
static byte Suelo_Root01(float a, float b, float c, float *t) {
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

byte Suelo_SegEdge(const vec3_t *a, const vec3_t *b, const vec3_t *p, const vec3_t *q, float r, float *t) {
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
		byte r1 = Suelo_Root01(d, b2, (ww - r * r) * ee - we * we, &tm);
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
	return Suelo_Root01(mm, wm, ww - r * r, t) == 1;
}

byte Suelo_SphereTri(const vec3_t *c, float r, const CCollideTri *tri, const CCollideMesh *mesh, vec3_t *res) {
	CHECK(tri && mesh && res);
	float r2 = r * r;
	const plane_t *pl = &tri->plane;
	float d = Plane_Dist(pl, c);
	if (!(r2 > d * d)) {
		return 0;
	}
	vec3_t q = { c->x - pl->n.x * d, c->y - pl->n.y * d, c->z - pl->n.z * d };
	vec3_t v0 = mesh->pVert[tri->v[0]], v1 = mesh->pVert[tri->v[1]], v2 = mesh->pVert[tri->v[2]];
	if (Suelo_PointInTri(&q, &v0, &v1, &v2)) {
		*res = q;
		return 1;
	}
	float dd[3];
	vec3_t cp[3];
	dd[0] = Suelo_Dist2PointSeg(&v0, &v1, c, &cp[0]);
	dd[1] = Suelo_Dist2PointSeg(&v1, &v2, c, &cp[1]);
	dd[2] = Suelo_Dist2PointSeg(&v2, &v0, c, &cp[2]);
	int i = dd[1] > dd[0] ? 0 : 1;
	int best = dd[2] > dd[i] ? i : 2;
	if (r2 > dd[best]) {
		*res = cp[best];
		return 1;
	}
	return 0;
}

/* a sphere of radius r around p within the edges of the triangle */
static byte Suelo_NearEdges(const vec3_t *p, float r2, const vec3_t *v0, const vec3_t *v1, const vec3_t *v2) {
	return r2 > Suelo_Dist2PointSeg(v0, v1, p, NULL) || r2 > Suelo_Dist2PointSeg(v1, v2, p, NULL) ||
		r2 > Suelo_Dist2PointSeg(v2, v0, p, NULL);
}

static vec3_t Vec3_Lerp(const vec3_t *a, const vec3_t *b, float t) {
	return (vec3_t){ a->x + (b->x - a->x) * t, a->y + (b->y - a->y) * t, a->z + (b->z - a->z) * t };
}

byte Suelo_SweepTri(const vec3_t *a, const vec3_t *b, float r, const CCollideTri *tri, const CCollideMesh *mesh,
	vec3_t *res) {
	const plane_t *pl = &tri->plane;
	float t;

	CHECK(res);
	if (!Suelo_SegPlane(a, b, r, pl, &t)) {
		return 0;
	}
	float da = Plane_Dist(pl, a);
	float db = Plane_Dist(pl, b);
	vec3_t v0 = mesh->pVert[tri->v[0]], v1 = mesh->pVert[tri->v[1]], v2 = mesh->pVert[tri->v[2]];
	if ((double)r > __builtin_fabs((double)da)) {
		/* the sphere touches the plane at the start */
		vec3_t q = { a->x - pl->n.x * da, a->y - pl->n.y * da, a->z - pl->n.z * da };
		if (Suelo_PointInTri(&q, &v0, &v1, &v2)) {
			*res = (0.0f > da ? da > db : db > da) ? *b : *a;
			return 1;
		}
		if (Suelo_NearEdges(a, r * r, &v0, &v1, &v2)) {
			*res = *a;
			return 1;
		}
	} else {
		CHECK(t >= 0.0f && t <= 1.0f);
		vec3_t p = Vec3_Lerp(a, b, t);
		float dp = Plane_Dist(pl, &p);
		vec3_t q = { p.x - pl->n.x * dp, p.y - pl->n.y * dp, p.z - pl->n.z * dp };
		if (Suelo_PointInTri(&q, &v0, &v1, &v2)) {
			Suelo_SegPlane(a, b, g_CollisionEpsilon + r, pl, &t);
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
		if (Suelo_NearEdges(b, r * r, &v0, &v1, &v2)) {
			*res = *a;
			return 1;
		}
		return 0;
	}
	/* the first touch of an edge */
	float ta = 100.0f, tb = 100.0f, tc = 100.0f;
	Suelo_SegEdge(&v0, &v1, a, b, r, &ta);
	Suelo_SegEdge(&v1, &v2, a, b, r, &tb);
	Suelo_SegEdge(&v2, &v0, a, b, r, &tc);
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
} sueloArea_t;

static void Suelo_Area(CSuelo *self, sueloArea_t *ar, float lox, float hix, float loz, float hiz, float r,
	const vec3_t *a, const vec3_t *b) {
	const CAlturas *alt = &self->alturas;
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

static int Suelo_SkipQuad(const CCollideQuad *q, const sueloArea_t *ar) {
	return (q->Mesh->flags & TRI_ALT_NO_COLISIONES) || q->minY > ar->top || ar->bottom > q->maxY ||
		(q->gapTop > ar->top && ar->bottom > q->gapBottom);
}

/* the triangle not tested yet in this query: marked */
static int Suelo_MarkTri(CCollideTri *tri) {
	if ((tri->flags & TRI_ALT_NO_COLISIONES) || tri->ncomp == g_CollisionNComp) {
		return 0;
	}
	tri->ncomp = g_CollisionNComp;
	tri->flags |= TRI_ALT_TESTED;
	g_CollisionTests++;
	return 1;
}

byte __thiscall CSuelo_CollideSphere(CSuelo *self, const vec3_t *a, const vec3_t *b, float r, vec3_t *out, vec3_t *hit,
	vec3_t *n) {
	vec3_t A = *a, C = *b, P, best;
	sueloArea_t ar;
	CCollideTri *bestTri = NULL;
	float bestDist = SUELO_FLT_MAX, bestCos = -1.0f;

	CSuelo_NewQuery(self);
	Suelo_Area(self, &ar, C.x, C.x, C.z, C.z, r, a, b);
	best = a->y > b->y ? *b : *a;
	for (int z = ar.z0; z <= ar.z1; z++) {
		for (int x = ar.x0; x <= ar.x1; x++) {
			CCollideQuad *cuad = Alturas_Cell(&self->alturas, x, z);
			CHECK(cuad);
			for (; cuad; cuad = cuad->next) {
				if (Suelo_SkipQuad(cuad, &ar)) {
					continue;
				}
				for (int k = 0; k < cuad->nTri; k++) {
					CCollideTri *tri = &cuad->Mesh->pTri[cuad->pTri[k]];
					if (!Suelo_MarkTri(tri) || !Suelo_SphereTri(&C, r, tri, cuad->Mesh, &P)) {
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

byte __thiscall CSuelo_SphereContact(CSuelo *self, const vec3_t *a, const vec3_t *b, float r, vec3_t *out,
	vec3_t *hit, vec3_t *n) {
	vec3_t C = *b, P, best;
	sueloArea_t ar;
	CCollideTri *bestTri = NULL;
	float bestDist = SUELO_FLT_MAX;

	CSuelo_NewQuery(self);
	Suelo_Area(self, &ar, C.x, C.x, C.z, C.z, r, a, b);
	best = a->y > b->y ? *b : *a;
	for (int z = ar.z0; z <= ar.z1; z++) {
		for (int x = ar.x0; x <= ar.x1; x++) {
			CCollideQuad *cuad = Alturas_Cell(&self->alturas, x, z);
			CHECK(cuad);
			for (; cuad; cuad = cuad->next) {
				if (Suelo_SkipQuad(cuad, &ar)) {
					continue;
				}
				for (int k = 0; k < cuad->nTri; k++) {
					CCollideTri *tri = &cuad->Mesh->pTri[cuad->pTri[k]];
					if (!Suelo_MarkTri(tri) || !Suelo_SphereTri(&C, r, tri, cuad->Mesh, &P)) {
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

byte __thiscall CSuelo_SweepSphere(CSuelo *self, const vec3_t *a, const vec3_t *b, float r, vec3_t *out, vec3_t *hit,
	vec3_t *n) {
	vec3_t A = *a, B = *b, P, best;
	sueloArea_t ar;
	CCollideTri *bestTri = NULL;
	float bestDist = SUELO_FLT_MAX;

	CSuelo_NewQuery(self);
	Suelo_Area(self, &ar, A.x < B.x ? A.x : B.x, A.x > B.x ? A.x : B.x, A.z < B.z ? A.z : B.z,
		A.z > B.z ? A.z : B.z, r, a, b);
	best = a->y > b->y ? *b : *a;
	for (int z = ar.z0; z <= ar.z1; z++) {
		for (int x = ar.x0; x <= ar.x1; x++) {
			CCollideQuad *cuad = Alturas_Cell(&self->alturas, x, z);
			CHECK(cuad);
			for (; cuad; cuad = cuad->next) {
				if (Suelo_SkipQuad(cuad, &ar)) {
					continue;
				}
				for (int k = 0; k < cuad->nTri; k++) {
					CCollideTri *tri = &cuad->Mesh->pTri[cuad->pTri[k]];
					if (!Suelo_MarkTri(tri) || !Suelo_SweepTri(&A, &B, r, tri, cuad->Mesh, &P)) {
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

float __thiscall CCollideQuad_TriMaxY(CCollideQuad *self, const CCollideTri *tri) {
	const vec3_t *v = self->Mesh->pVert;
	float y0 = v[tri->v[0]].y, y1 = v[tri->v[1]].y, y2 = v[tri->v[2]].y;
	float m = y0 > y1 ? y0 : y1;
	return m > y2 ? m : y2;
}

float __thiscall CCollideQuad_TriMinY(CCollideQuad *self, const CCollideTri *tri) {
	const vec3_t *v = self->Mesh->pVert;
	float y0 = v[tri->v[0]].y, y1 = v[tri->v[1]].y, y2 = v[tri->v[2]].y;
	float m = y0 < y1 ? y0 : y1;
	return y2 > m ? m : y2;
}

int __fastcall CCollideQuad_LowestTri(CCollideQuad *self) {
	int best = -1;
	float low = self->maxY;

	for (int i = 0; i < self->nTri; i++) {
		const CCollideTri *tri = Quad_Tri(self, i);
		if (tri->flags & TRI_ALT_NO_COLISIONES) {
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

void __fastcall CCollideQuad_CalcGap(CCollideQuad *self) {
	self->gapBottom = self->minY;
	self->gapTop = self->maxY;
	int cur = CCollideQuad_LowestTri(self);
	if (cur < 0) {
		return;
	}
	self->gapTop = CCollideQuad_TriMinY(self, Quad_Tri(self, cur));
	/* from the lowest triangle up: the triangles that overlap in height are joined, the gap above them is the
	 * distance to the lowest triangle that starts higher */
	CTabla used = { 0, NULL };
	int count = 0;
	Tabla_Alloc1(&used, self->nTri);
	byte *p = used.p;
	CHECK(p);
	memset(p, 0, used.n);
	for (;;) {
		TABLA_CHECK(p, cur, used.n);
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
			TABLA_CHECK(p, i, used.n);
			if (p[i]) {
				continue;
			}
			const CCollideTri *tri = Quad_Tri(self, i);
			if (tri->flags & TRI_ALT_NO_COLISIONES) {
				TABLA_CHECK(p, i, used.n);
				count++;
				p[i] = 1;
				continue;
			}
			float mn = CCollideQuad_TriMinY(self, tri);
			float mx = CCollideQuad_TriMaxY(self, tri);
			if (top >= mx) {
				TABLA_CHECK(p, i, used.n);
				count++;
				p[i] = 1;
				continue;
			}
			if (top >= mn) {
				top = mx;
				TABLA_CHECK(p, i, used.n);
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

void __thiscall CCollideQuad_CalcExtens(CAlturas *self, CCollideQuad *q) {
	(void)self;
	box_t b = { { SUELO_FLT_MAX, SUELO_FLT_MAX, SUELO_FLT_MAX }, { -SUELO_FLT_MAX, -SUELO_FLT_MAX, -SUELO_FLT_MAX } };

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

void __thiscall CCollideQuad_AddAllTris(CAlturas *self, CCollideQuad *q) {
	(void)self;
	Tabla_Alloc2((CTabla *)&q->nTri, q->Mesh->nTri);
	for (int i = 0; i < q->nTri; i++) {
		CHECK(i <= CNDX_MAX);
		TABLA_CHECK(q->pTri, i, q->nTri);
		q->pTri[i] = (uint16_t)i;
	}
}

CCollideQuad *__thiscall CCollideQuad_Find(CAlturas *self, CCollideQuad *cuad, uint32_t id, CCollideMesh *mesh) {
	(void)self;
	CHECK(cuad && mesh);
	if (cuad->Mesh == mesh && cuad->Mesh->MeshID == id) {
		return cuad;
	}
	if (!cuad->Mesh) {
		CHECK(id == 0);
		CCollideQuad_SetMesh(cuad, mesh);
		cuad->dwParam = 0;
		return cuad;
	}
	for (; cuad->next; cuad = cuad->next) {
		if (cuad->next->Mesh == mesh && mesh->MeshID == id) {
			return cuad->next;
		}
	}
	cuad->next = CCollideQuad_New();
	CCollideQuad_SetMesh(cuad->next, mesh);
	cuad->next->dwParam = 0;
	return cuad->next;
}

void __thiscall CAlturas_QuadInit(CAlturas *self, int unused, const box_t *box, int numTri) {
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
	int nx = (int)crt_ceil((double)self->dim.x * k);
	self->nx = nx;
	int nz = (int)crt_ceil((double)self->dim.z * k);
	self->nz = nz;
	self->sizeX = self->dim.x / (float)nx;
	self->sizeZ = self->dim.z / (float)nz;
	self->invX = (float)nx / self->dim.x;
	self->invZ = (float)nz / self->dim.z;
	Tabla_New24((CTabla *)&self->nCuads, nz * nx);
	Con_Verbosef("QuadInit: %d x %d  (NumTri: %d)  (size: %.2f)\n", self->nx, self->nz, numTri, size);
}

void __thiscall CAlturas_QuadAddMesh(CAlturas *self, CCollideMesh *mesh, int initTri) {
	int included = 0, discarded = 0, maxTri = 0;

	CHECK(!initTri);
	if (mesh->outSector) {
		if (!self->outSector) {
			self->outSector = CCollideQuad_New();
		}
		CCollideQuad *q = CCollideQuad_Find(self, self->outSector, self->MeshID, mesh);
		CCollideQuad_AddAllTris(self, q);
		CCollideQuad_CalcExtens(self, q);
		E3D_LogWhite(g_FileLog, "QuadAddMesh: OutSector: %d included", q->nTri);
		return;
	}
	for (int i = 0; i < self->nCuads; i++) {
		for (CCollideQuad *q = Alturas_Cuad(self, i); q; q = q->next) {
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
				CCollideQuad *cuad = Alturas_Cell(self, x, z);
				if (!cuad) {
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
				CCollideQuad *q = CCollideQuad_Find(self, cuad, self->MeshID, mesh);
				if (!q) {
					continue;
				}
				included++;
				CHECK(q->Mesh == mesh);
				CHECK(q->Mesh->MeshID == self->MeshID);
				q->dwParam++;
				if (q->dwParam > (uint32_t)q->nTri) {
					Tabla_Resize2((CTabla *)&q->nTri, q->dwParam * 2);
				}
				CHECK(t <= CNDX_MAX);
				uint32_t k = q->dwParam - 1;
				TABLA_CHECK(q->pTri, k, q->nTri);
				q->pTri[k] = (uint16_t)t;
			}
		}
	}
	for (int i = 0; i < self->nCuads; i++) {
		for (CCollideQuad *q = Alturas_Cuad(self, i); q; q = q->next) {
			if (q->Mesh != mesh) {
				continue;
			}
			CHECK(q->dwParam < 65536);
			Tabla_Resize2((CTabla *)&q->nTri, q->dwParam);
			CCollideQuad_CalcExtens(self, q);
			maxTri = maxTri > q->nTri ? maxTri : q->nTri;
		}
	}
	E3D_LogWhite(g_FileLog, "QuadAddMesh: %d included, %d discarded (%d MaxTriQuad)", included, discarded, maxTri);
	if (!included) {
		Con_Warnf("QuadAddMesh: No triangles included in the collision system.\n");
	}
}

/* the meshes */

static void Suelo_NewCacheID(CSuelo *self) {
	g_QuadTriCacheID++;
	CHECK(g_QuadTriCacheID != 0);
	memset(self->cache, 0, sizeof(self->cache));
}

/* through the import table of the exe (winmm) */

static const char *const sueloVertExpr[3] = { "(tri->v[0] + NumVert <= CNDX_MAX)", "(tri->v[1] + NumVert <= CNDX_MAX)",
	"(tri->v[2] + NumVert <= CNDX_MAX)" };

void __thiscall CSuelo_AddMesh(CSuelo *self, CCollideMesh *mesh, CMallaTri *tm, uint32_t *outID, int *outIni) {
	int degenerate = 0, invalid = 0;

	CHECK(self->alturas.nCuads);
	CHECK(tm && (tm->nTri > 0) && (tm->nVert > 0));
	Suelo_NewCacheID(self);
	if (outID) {
		self->alturas.MeshID++;
		CHECK((uint16_t)self->alturas.MeshID != 0);
		*outID = self->alturas.MeshID;
		*outIni = mesh->nTri;
		mesh->flags |= TRI_ALT_MESH;
		mesh->MeshID = (uint16_t)self->alturas.MeshID;
	}
	int nv = mesh->nVert, nt = mesh->nTri;
	CHECK(mesh->pTri == NULL);
	Tabla_ResizeC((CTabla *)&mesh->nVert, tm->nVert + nv);
	Tabla_Resize1C((CTabla *)&mesh->nTri, tm->nTri + nt);
	for (int i = 0; i < tm->nVert; i++) {
		vec3_t v;
		mesh->pVert[nv + i] = *CMallaTri_GetVert(tm, &v, i);
	}
	for (int i = 0; i < tm->nTri; i++) {
		const CTriMalla *t = CMallaTri_GetTri(tm, i);
		CCollideTri *ct = &mesh->pTri[nt + i];
		ct->ncomp = 0;
		for (int k = 0; k < 3; k++) {
			if ((uint32_t)(t->v[k] + nv) > CNDX_MAX) {
				Con_FatalErrorf("\nFATAL ERROR: Check failed!\n\nError: %s\n\nExpression: %s\n\nFile: '%s'\n"
					"Date: %s\nLine: %d", "Too many vertexes in a sector", sueloVertExpr[k], __FILE__,
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
		/* a material without collisions (byte 0x3c bit 0) needs the flag on the triangle */
		CHECK(!(t->mat && (*((byte *)t->mat + 0x3c) & 1)) || (ct->flags & TRI_ALT_NO_COLISIONES));
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
			CHECK((ct->flags & TRI_ALT_NO_COLISIONES) == 0);
		}
	}
	if (tm->nTri > 0 && (degenerate || invalid)) {
		E3D_LogVerbose(g_FileLog, "ERROR: CollisionMeshAdd: %d caras degeneradas, %d caras no validas  [sector: %d]",
			degenerate, invalid, mesh->sector - 1);
	}
	box_t *b = &mesh->Extens;
	b->min.x = b->min.y = b->min.z = SUELO_FLT_MAX;
	b->max.x = b->max.y = b->max.z = -SUELO_FLT_MAX;
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
	uint32_t t0 = exe_timeGetTime();
	CAlturas_QuadAddMesh(&self->alturas, mesh, nt);
	uint32_t t1 = exe_timeGetTime();
	E3D_LogWhite(g_FileLog, "QuadAddMesh: %d Kb  [%d tri, %d vtx]  [sector: %d]  [%.2f sec.]",
		(g_MemStats->bytes - mem) / 1024, tm->nTri, tm->nVert, mesh->sector - 1, (double)(t1 - t0) / 1000.0);
}

void __thiscall CSuelo_RemoveMesh(CSuelo *self, CCollideMesh *mesh, int a2, int a3) {
	(void)a2;
	(void)a3;
	Suelo_NewCacheID(self);
	for (int i = 0; i < self->alturas.nCuads; i++) {
		CCollideQuad *q = Alturas_Cuad(&self->alturas, i);
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

void __thiscall CSuelo_SetMeshCollide(CSuelo *self, CCollideMesh *mesh, byte on, int MeshID, int IniTri) {
	CHECK(!IniTri);
	CHECK(MeshID);
	Suelo_NewCacheID(self);
	if (on) {
		CHECK(mesh->flags & TRI_ALT_NO_COLISIONES);
		mesh->flags &= ~TRI_ALT_NO_COLISIONES;
		return;
	}
	CHECK(!(mesh->flags & TRI_ALT_NO_COLISIONES));
	mesh->flags |= TRI_ALT_NO_COLISIONES;
}

void __fastcall CSuelo_CalcGaps(CSuelo *self) {
	for (int i = 0; i < self->alturas.nCuads; i++) {
		for (CCollideQuad *q = Alturas_Cuad(&self->alturas, i); q; q = q->next) {
			CCollideQuad_CalcGap(q);
		}
	}
}

/* objeto3D.h: the object of a node as the class wanted, NULL (a failed CHECK) for another */
static CMallaTri *Suelo_ObjMalla(void *obj) {
	if (((CMallaTri *)obj)->clase != CLASE_OBJ3D_MALLATRI) {
		CheckFailed("devNULL", "objeto3D.h", __TIMESTAMP__, __LINE__);
		return NULL;
	}
	return obj;
}

void __thiscall CSuelo_BuildSectorMesh(CSuelo *self, CCollideMesh *mesh, int mode, uint32_t sector) {
	CMallaTri *m = operator_new(sizeof(CMallaTri));
	m = m ? CMallaTri_ctor(m) : NULL;
	if (mode == 2) {
		mesh->flags |= TRI_ALT_NO_COLISIONES;
	}
	mesh->sector = (uint16_t)sector;
	mesh->outSector = Sector_IsOutside(sector - 1);
	for (int i = 0; i < self->nNodos; i++) {
		CMallaTri *obj = Suelo_ObjMalla(Suelo_Obj(self, i));
		for (int t = 0; t < obj->nTri; t++) {
			CTriMalla *tri = CMallaTri_GetTri(obj, t);
			uint32_t flags = tri->flags & 0xffff;
			byte skip;
			if ((tri->flags >> 16) != sector && sector != 0xffffffffu) {
				skip = 1;
			} else if (mode == 1 && (flags & TRI_ALT_NO_COLISIONES)) {
				skip = 1;
			} else if (mode == 2 && !(flags & TRI_ALT_NO_COLISIONES)) {
				skip = 1;
			} else {
				skip = (flags & TRI_ALT_INVALID) == TRI_ALT_INVALID;
			}
			if (skip) {
				continue;
			}
			CVertMalla v[3];
			memset(v, 0, sizeof(v));
			CMallaTri_GetTriVert(obj, &v[0], t, 0);
			CMallaTri_GetTriVert(obj, &v[1], t, 1);
			CMallaTri_GetTriVert(obj, &v[2], t, 2);
			CMallaTri_AddTriVerts(m, &v[0], &v[1], &v[2], tri->mat, 0, tri->flags);
		}
	}
	if (m->nTri) {
		if (m->pTVert) {
			m->pTVert[0].u = 0.0f;
			m->pTVert[0].v = 0.0f;
		}
		for (int t = 0; t < m->nTri; t++) {
			CTriMalla *tri = CMallaTri_GetTri(m, t);
			tri->mat = NULL;
			memset(tri->n, 0, sizeof(tri->n));
			memset(tri->tv, 0, sizeof(tri->tv));
		}
		CMallaD3D *d3d = operator_new(200);
		d3d = d3d ? CMallaD3D_ctorMalla(d3d, m, 0x21, 0) : NULL;
		CMallaTri_Clear(m);
		CMallaTri_AddD3D(m, d3d, 1);
		CSuelo_AddMesh(self, mesh, m, NULL, NULL);
		if (d3d) {
			((objDelete_t)d3d->vftable[0])(d3d, 1);
		}
	}
	((objDelete_t)(*(void ***)m)[0])(m, 1);
}

void __fastcall CSuelo_CreateQuadCollision(CSuelo *self) {
	CAlturas *alt = &self->alturas;
	int empty = 0;

	Con_Verbosef("CreateQuadCollision: Building AMC\n");
	CAlturas_QuadInit(alt, 0, &self->CollideExt, self->CollideTri);
	int sectors = Sector_Num() + 1;
	for (int i = 0; i < sectors; i++) {
		CHECK(i < MAX_COLLIDE_SECTORS);
		CSuelo_BuildSectorMesh(self, &alt->_CollideMesh1[i], 1, i);
	}
	for (int z = 0; z < alt->nz; z++) {
		for (int x = 0; x < alt->nx; x++) {
			CCollideQuad *q = Alturas_Cell(alt, x, z);
			if (q->Mesh) {
				continue;
			}
			empty++;
			CCollideQuad_SetMesh(q, &alt->CollideMesh1);
			q->dwParam = 0;
			q->minY = self->Extens.max.y;
			q->maxY = self->Extens.max.y;
			q->gapBottom = q->minY;
			q->gapTop = q->minY;
		}
	}
	for (int j = 0; j < alt->nCuads; j++) {
		CHECK(Alturas_Cuad(alt, j)->Mesh);
	}
	E3D_LogWhite(g_FileLog, "Empty collision quads: %d", empty);
}

/* the floor nodes and the debug drawing */

void __thiscall CSuelo_NodosToD3D(CSuelo *self, int fvf) {
	for (int i = 0; i < self->nNodos; i++) {
		TABLA_CHECK(self->nodos, i, self->nNodos);
		CNodo3D_BuildD3D(&self->nodos[i], fvf);
	}
}

void __thiscall CSuelo_Draw(CSuelo *self, void *dev, int unused) {
	(void)unused;
	Render_SetWorldMatrix(g_MatIdentity, 0);
	for (int i = 0; i < self->nNodos; i++) {
		if (!Suelo_Obj(self, i)) {
			continue;
		}
		void *obj = Suelo_Obj(self, i);
		if (((CMallaTri *)obj)->clase != CLASE_OBJ3D_MALLAD3D) {
			CheckFailed("devNULL", "objeto3D.h", __TIMESTAMP__, __LINE__);
			obj = NULL;
		}
		Batch_AddMalla(obj, dev);
	}
	Batch_Flush(dev);
}

void Suelo_DrawBox(const vec3_t *c, float r, uint32_t color) {
	vec3_t mn = { c->x - r, c->y - r, c->z - r };
	vec3_t mx = { c->x + r, c->y + r, c->z + r };
	Debug_DrawBox(&mn, &mx, color);
}

void Suelo_DrawDebug(void) {
	CHECK(g_Mundo);
	CSuelo *suelo = g_Mundo->ObjSuelo;
	if (suelo) {
		CSuelo_DrawDebug(suelo);
	}
}

/* building the floor of a scene */

/* the node flags read here */
#define NODO_FLAG_NOCOLLIDE (1u << 7)	/* its triangles collide only without colision_opt */
#define NODO_FLAG_NOCOLLIDE2 (1u << 13)
#define NODO_FLAG_HIDDEN (1u << 25)
#define NODO_FLAG_EMPTY (1u << 1)
#define NODO_FLAG_SUELO (1u << 3)	/* taken into the floor */
/* the material bits that make a triangle not collide or mark it (material.h) */
#define MAT_FLAGS 0x3c
#define MAT_B38 0x38
#define MAT_B3A 0x3a
#define MAT_B3B 0x3b
#define MAT_A0 0xa0
/* the triangles per floor node */
#define g_SueloTrisPerNode GAME_DATA(int, 0x431bc0)

#define Suelo_Field(p, off, type) (*(type *)((byte *)(p) + (off)))

/* the mesh of a scene node: its own collision mesh at 0x30 when it has one */
static CMallaTri *Suelo_NodoMalla(CNodo3D *node) {
	CMallaTri *m = Suelo_ObjMalla(node->obj3D);
	if (m->d3d) {
		m = (CMallaTri *)m->d3d;
	}
	return m;
}

static void Suelo_LogMemory(int mem, uint32_t t0) {
	uint32_t t1 = exe_timeGetTime();
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
	b->min.x = b->min.y = b->min.z = SUELO_FLT_MAX;
	b->max.x = b->max.y = b->max.z = -SUELO_FLT_MAX;
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
static uint32_t Suelo_TriFlags(uint32_t nodeFlags, const void *mat, byte opt) {
	uint32_t fl = 0;
	if (opt && !(nodeFlags & NODO_FLAG_NOCOLLIDE)) {
		fl = 1;
	}
	if (nodeFlags & NODO_FLAG_NOCOLLIDE) {
		fl |= 2;
	}
	if (nodeFlags & NODO_FLAG_NOCOLLIDE2) {
		fl |= 1;
	}
	uint16_t mf = Suelo_Field(mat, MAT_FLAGS, uint16_t);
	if (mf & 1) {
		fl |= 1;
	}
	if (Suelo_Field(mat, MAT_B38, byte)) {
		fl |= 0x10;
	}
	if (Suelo_Field(mat, MAT_B3A, byte) || (Suelo_Field(mat, MAT_A0, void *) && !Suelo_Field(mat, MAT_B3B, byte))) {
		fl |= 0x20;
	}
	if (mf & 2) {
		fl |= 0x40;
	}
	return fl;
}

void __thiscall CSuelo_Build(CSuelo *self, CEscena *esc) {
	const char *name = esc->name;
	char nodeName[128];

	if (esc->fromAmc) {
		int mem = g_MemStats->bytes;
		uint32_t t0 = exe_timeGetTime();
		if (CSuelo_LoadCollision(self, name)) {
			CheckFailed("!LoadCollision( Escena->Nombre )", __FILE__, __TIMESTAMP__, __LINE__);
		}
		Suelo_LogMemory(mem, t0);
		Con_Verbosef("Level loaded from SM3+AMC\n");
		return;
	}
	CHECK(esc->Nodo3D);
	CEscena_UpdateNodes(esc);
	/* the box of all meshes and the grid of floor nodes over it */
	Box_Empty(&self->Extens);
	int nTri = 0;
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		if (CNodo3D_Clase(n) != CLASE_OBJ3D_MALLATRI || (n->flags & NODO_FLAG_HIDDEN)) {
			continue;
		}
		CMallaTri *m = Suelo_NodoMalla(n);
		CMallaTri_Transform(m, &n->m);
		CMallaTri_CalcBox(m, &self->Extens, 0);
		nTri += m->nTri;
	}
	box_t *ext = &self->Extens;
	CHECK(ext->max.x >= ext->min.x && ext->max.y >= ext->min.y && ext->max.z >= ext->min.z);
	Box_Widen(ext, g_CollisionEpsilon * 10.0f);
	self->Dim.x = ext->max.x - ext->min.x;
	self->Dim.y = ext->max.y - ext->min.y;
	self->Dim.z = ext->max.z - ext->min.z;
	CHECK(2000000.0 > __builtin_fabs((double)self->Dim.x));
	CHECK(2000000.0 > __builtin_fabs((double)self->Dim.y));
	CHECK(2000000.0 > __builtin_fabs((double)self->Dim.z));
	double k = 1.0 / __builtin_sqrt((double)(((self->Dim.z * self->Dim.x) / (float)nTri) * (float)g_SueloTrisPerNode));
	int nx = (int)crt_ceil((double)self->Dim.x * k);
	self->nx = nx;
	int nz = (int)crt_ceil((double)self->Dim.z * k);
	self->nz = nz;
	Tabla_NewFC((CTabla *)&self->nNodos, nz * nx);
	for (int i = 0; i < self->nNodos; i++) {
		Bprintf(nodeName, sizeof(nodeName), "suelo_%d", i);
		TABLA_CHECK(self->nodos, i, self->nNodos);
		Cad_Set(&self->nodos[i].name, nodeName);
		CMallaTri *m = operator_new(sizeof(CMallaTri));
		m = m ? CMallaTri_ctor(m) : NULL;
		TABLA_CHECK(self->nodos, i, self->nNodos);
		CNodo3D_SetObj3D(&self->nodos[i], (IObjeto3D *)m);
	}
	/* the triangles into the floor node of the cell of their center */
	self->CollideTri = 0;
	Box_Empty(&self->CollideExt);
	CArchINI *ini = esc->ini;
	byte opt = ini && CArchINI_GetBool(ini, "suelo", "colision_opt", 0) ? 1 : 0;
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		int sector = Sector_OfNode(n);
		byte outside = Sector_IsOutside(sector);
		uint32_t *nodeFlags = &n->flags;
		if (CNodo3D_Clase(n) != CLASE_OBJ3D_MALLATRI) {
			if (*nodeFlags & NODO_FLAG_EMPTY) {
				CHECK(CNodo3D_Clase(n) == 0);
				*nodeFlags |= NODO_FLAG_SUELO;
			}
			continue;
		}
		if (*nodeFlags & NODO_FLAG_HIDDEN) {
			continue;
		}
		CMallaTri *m = Suelo_NodoMalla(n);
		for (int t = 0; t < m->nTri; t++) {
			CVertMalla v[3];
			memset(v, 0, sizeof(v));
			CMallaTri_GetTriVert(m, &v[0], t, 0);
			CMallaTri_GetTriVert(m, &v[1], t, 1);
			CMallaTri_GetTriVert(m, &v[2], t, 2);
			void *mat = m->pTri[t].mat;
			uint32_t cx = (uint32_t)(int)((((v[0].pos.x + v[1].pos.x + v[2].pos.x) / 3.0f - ext->min.x) /
				self->Dim.x) * (float)self->nx);
			uint32_t cz = (uint32_t)(int)((((v[0].pos.z + v[1].pos.z + v[2].pos.z) / 3.0f - ext->min.z) /
				self->Dim.z) * (float)self->nz);
			if (cx >= (uint32_t)self->nx || cz >= (uint32_t)self->nz) {
				continue;
			}
			CMallaTri *dst = Suelo_ObjMalla(Suelo_Obj(self, self->nx * cz + cx));
			uint32_t sec = sector + 1;
			uint32_t flags = Suelo_TriFlags(*nodeFlags, mat, opt);
			CHECK(flags <= 0xffff);
			CHECK(sec <= 0xffff);
			if (!(flags & TRI_ALT_NO_COLISIONES) && !outside) {
				self->CollideTri++;
				Box_Add(&self->CollideExt, &v[0].pos);
				Box_Add(&self->CollideExt, &v[1].pos);
				Box_Add(&self->CollideExt, &v[2].pos);
			}
			CMallaTri_AddTriVerts(dst, &v[0], &v[1], &v[2], mat, 1, sec << 16 | flags);
		}
		CNodo3D_FreeObj3D(n);
		*nodeFlags |= NODO_FLAG_SUELO;
	}
	CHECK(self->CollideTri);
	box_t *ce = &self->CollideExt;
	CHECK(ce->max.x >= ce->min.x && ce->max.y >= ce->min.y && ce->max.z >= ce->min.z);
	Box_Widen(ce, g_CollisionEpsilon * 10.0f);
	int mem = g_MemStats->bytes;
	uint32_t t0 = exe_timeGetTime();
	if (CSuelo_LoadCollision(self, name)) {
		CSuelo_CreateQuadCollision(self);
		CSuelo_SaveCollision(self, name);
	}
	Suelo_LogMemory(mem, t0);
}

void __fastcall CSuelo_FreeNodos(CSuelo *self) {
	Tabla_FreeObjs((CTabla *)&self->nNodos);
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

/* the lighting of the device meshes of the nodes; nodo and esc go through unchanged */
void __thiscall CSuelo_CalcIlum(void *suelo, CNodo3D *nodo, CEscena *esc) {
	CSuelo *self = suelo;

	for (int i = 0; i < self->nNodos; i++) {
		TABLA_CHECK(self->nodos, i, self->nNodos);
		if (CNodo3D_Clase(&self->nodos[i]) == CLASE_OBJ3D_MALLAD3D) {
			TABLA_CHECK(self->nodos, i, self->nNodos);
			CMallaD3D_CalcIlum(Obj3D_Cast(self->nodos[i].obj3D, CLASE_OBJ3D_MALLAD3D), nodo, esc);
		}
	}
}

/* the debug view of the collision meshes (c_show and ShowMapSolid) */
#define g_ShowMapSolid GAME_DATA(int, 0x4af850)		/* 0 wireframe, 1 solid, 2 solid with fog */
#define g_CollideAddColor ((float *)Game_Ptr(0x432420))	/* added to the shaded colors, rgba */
#define g_CollideVerts ((CollideVert *)Game_Ptr(0x4af8b0))	/* the triangle drawn */
#define g_SectorColors ((const uint32_t *)Game_Ptr(0x432a34))

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
		float s = g_MapaSombrasEscala * g_MapaSombrasResol / 10.0f;
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

	D3D_METHOD(dev, d3dSetVertexShader_t, D3DDEV_SETVERTEXSHADER)(dev, 0x142);	/* XYZ | DIFFUSE | TEX1 */
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
		D3D_METHOD(dev, d3dDrawPrimitiveUP_t, D3DDEV_DRAWPRIMITIVEUP)(dev, 4, 1, g_CollideVerts, sizeof(CollideVert));
		t->flags &= 0xfff;
		mode = g_ShowCollision;
	}
	if (nested) {
		return;
	}
	if (!g_ShowMapSolid) {
		dev = g_pd3dDevice;
		D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_FILLMODE, 2);
		CCollideMesh_DrawDebug(self, 1);
		dev = g_pd3dDevice;
		D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_FILLMODE, 3);
		if (!g_ShowMapSolid) {
			return;
		}
	}
	dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_CULLMODE, 2);
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
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_CULLMODE, 3);
}
