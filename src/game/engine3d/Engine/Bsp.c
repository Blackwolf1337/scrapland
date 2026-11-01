/*
 * Engine/bsp.cpp of the engine3d: the light maps of the floor, see include/scrap/bsp.h. The map compiler (CBsp_Calculate),
 * its lights, texels, vertex lists, direct light and radiosity passes and the .EMI writer are in BspCalc.c.
 */
#include <string.h>

#include "scrap/bsp.h"
#include "scrap/console.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/scene.h"
#include "scrap/fonts.h"
#include "scrap/readm3d.h"
#include "scrap/light.h"
#include "scrap/meshd3d.h"
#include "scrap/portal.h"
#include "scrap/prof.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapworld.h"
#include "scrap/ground.h"
#include "scrap/groundportal.h"
#include "scrap/text.h"
#include "scrap/utild3d.h"
#include "scrap/misc.h"
#include "scrap/win32.h"

typedef uint32_t(__stdcall *getenvvar_t)(const char *name, char *buf, uint32_t size);
#define exe_GetEnvironmentVariableA GAME_IMPORT(getenvvar_t, GetEnvironmentVariableA, 12)

/* the "uint as double" conversions of the original: the int plus 2^32 for a set top bit */
static inline float Uint_ToFloat(uint32_t u) {
	return (float)(double)u;
}


/* engine3d/engine/bmp.h, bmpflt.h, bmpflt2.h */

void CBmpFlt2_dtor(CBmpFlt2 *self) {
	Img_Free(self, 0xc);
}

void CBmpFlt_dtor(CBmpFlt *self) {
	Img_Free(self, 0x10);
}

void CBmp_Assign(CBmp *self, const CBmp *orig) {
	Img_Free(self, 4);
	CBmp_Resample(self, orig, 0xf);
}

void CBmpFlt2_Assign(CBmpFlt2 *self, const CBmpFlt2 *orig) {
	Img_Free(self, 0xc);
	CBmpFlt2_Resample(self, orig, 0xf);
}

void CBmp_CreateFill(CBmp *self, int width, int height, uint32_t color) {
	Img_Create(self, width, height, 4);
	CBmp_Fill(self, color);
}

void CBmpFlt2_CreateFill(CBmpFlt2 *self, int width, int height, const float *color) {
	Img_Create(self, width, height, 0xc);
	CBmpFlt2_Fill(self, color);
}

void CBmpFlt_CreateFill(CBmpFlt *self, int width, int height, const float *color) {
	Img_Create(self, width, height, 0x10);
	CBmpFlt_Fill(self, color);
}

/* pixel (x, y) of self is pixel (alto - 1 - y, x) of orig */
static void Img_Rotate(CImage *self, const CImage *orig, int bytes) {
	uint32_t width = (uint32_t)orig->width;
	uint32_t height = (uint32_t)orig->height;

	Img_Create(self, (int)height, (int)width, bytes);
	for (int y = 0; y < self->height; y++) {
		for (int x = 0; x < self->width; x++) {
			const void *src = Img_Pixel(orig, (uint32_t)(self->height - y - 1), (uint32_t)x, bytes);
			memcpy(Img_Pixel(self, (uint32_t)x, (uint32_t)y, bytes), src, (size_t)bytes);
		}
	}
}

void CBmp_Rotate(CBmp *self, const CBmp *orig) {
	CHECK(orig);
	Img_Rotate(self, orig, 4);
}

void CBmpFlt2_Rotate(CBmpFlt2 *self, const CBmpFlt2 *orig) {
	CHECK(orig);
	Img_Rotate(self, orig, 0xc);
}


/* the TXF cache */

BspTxf *BspTxf_ctor(BspTxf *self, const char *map, const char *name, int n, int on) {
	self->on = on && (g_ShadowMapUseTxf || g_TxfMode);
	self->base[0] = 0;
	self->name[0] = 0;
	self->team[0] = 0;
	self->FileTXF = NULL;
	self->written = 0;
	self->skipped = 0;
	self->locked = 0;
	self->writeRetries = 0;
	self->readRetries = 0;
	self->deleted = 0;
	self->rebuild = 1;
	self->start = exe_timeGetTime();
	CHECK(map && name);
	Bprintf(self->base, sizeof(self->base), "&u&d_TXF/&n.%s.%d", name, n);
	Path_Format(self->base, map, self->base);
	Path_MakePath(self->base);
	memset(self->team, 0, sizeof(self->team));
	exe_GetEnvironmentVariableA("COMPUTERNAME", self->team, sizeof(self->team));
	return self;
}

void BspTxf_dtor(BspTxf *self) {
	CHECK(!self->FileTXF);
}

void BspTxf_SetName(BspTxf *self, int i) {
	Bprintf(self->name, sizeof(self->name), "%s.%05d.TXF", self->base, i + 1);
}

void BspTxf_Info(BspTxf *self) {
	int secs = (int)((double)(exe_timeGetTime() - self->start) / 1000.0);
	int m = secs / 60 % 60;
	int s = secs % 60;

	E3D_DebugPrintf("\nTXF reloading finished: %s", Path_FileName(self->base));
	E3D_DebugPrintf("\ntime: %d:%02d:%02d hms", secs / 3600, m, s);
	E3D_DebugPrintf("\nwritten: %d, skipped: %d, already locked: %d", self->written, self->skipped, self->locked);
	E3D_DebugPrintf("\nwrite retries: %d, read retries: %d, auto deleted: %d\n", self->writeRetries,
		self->readRetries, self->deleted);
	E3D_LogPrint(g_FileLog, "TXF info: %s", Path_FileName(self->base));
	E3D_LogPrint(g_FileLog, " time: %d:%02d:%02d hms", secs / 3600, m, s);
	E3D_LogPrint(g_FileLog, " written: %d", self->written);
	E3D_LogPrint(g_FileLog, " skipped: %d", self->skipped);
	E3D_LogPrint(g_FileLog, " already locked: %d", self->locked);
	E3D_LogPrint(g_FileLog, " write retries: %d", self->writeRetries);
	E3D_LogPrint(g_FileLog, " read retries: %d", self->readRetries);
	E3D_LogPrint(g_FileLog, " auto deleted: %d", self->deleted);
}


/* the lists */

BspTri *BspTri_ctor(BspTri *self) {
	self->flags = 0;
	self->light.img = NULL;
	self->light2.img = NULL;
	self->lightA.img = NULL;
	self->rad.img = NULL;
	self->rad2.img = NULL;
	self->mask.img = NULL;
	self->mask2.img = NULL;
	self->name = NULL;
	self->radColor = 0;
	self->names = NULL;
	self->radInt = 1.0f;
	self->col144[0] = 0.0f;
	self->col144[2] = 0.0f;
	self->col144[1] = 0.0f;
	self->col144[3] = 1.0f;
	for (int i = 0; i < 4; i++) {
		self->areas[i].p = NULL;
		self->areas[i].n = 0;
	}
	CVertList_ctor(&self->lv);
	CVertList_ctor(&self->lv2);
	CVertList_ctor(&self->lv3);
	self->next = NULL;
	self->Mat = NULL;
	self->LMap = NULL;
	self->node = NULL;
	self->pTri = NULL;
	self->pVertVB = NULL;
	self->pVertIB = NULL;
	self->zone = -1;
	self->zone_next = NULL;
	self->region = NULL;
	self->maskColor = 0;
	return self;
}

void BspTri_dtor(BspTri *self) {
	if (self->next) {
		BspTri *next = self->next;
		BspTri_dtor(next);
		operator_delete_sized(next, sizeof(BspTri));
		self->next = NULL;
	}
	if (self->pTri) {
		operator_delete_sized(self->pTri, 6);
		self->pTri = NULL;
	}
	CHECK(!self->pVertVB);
	CHECK(!self->pVertIB);
	if (self->region) {
		BspRegion *r = self->region;
		BspRegion_dtor(r);
		operator_delete_sized(r, sizeof(BspRegion));
		self->region = NULL;
	}
	CVertList_Free(&self->lv3);
	CVertList_Free(&self->lv2);
	CVertList_Free(&self->lv);
	for (int i = 3; i >= 0; i--) {
		Table_Free(&self->areas[i]);
	}
	if (self->names) {
		BspName *nb = self->names;
		BspName_dtor(nb);
		operator_delete_sized(nb, sizeof(BspName));
		self->names = NULL;
	}
	if (self->name) {
		String_Set(&self->name, NULL);
	}
	Img_Free(&self->mask2, 4);
	Img_Free(&self->mask, 4);
	Img_Free(&self->rad2, 0xc);
	Img_Free(&self->rad, 0xc);
	Img_Free(&self->lightA, 0x10);
	Img_Free(&self->light2, 0xc);
	Img_Free(&self->light, 0xc);
}

void BspName_dtor(BspName *self) {
	if (self->next) {
		BspName *next = self->next;
		BspName_dtor(next);
		operator_delete_sized(next, sizeof(BspName));
		self->next = NULL;
	}
	if (self->name) {
		String_Set(&self->name, NULL);
	}
}

void BspNode_dtor(BspNode *self) {
	if (self->next) {
		BspNode *next = self->next;
		BspNode_dtor(next);
		operator_delete_sized(next, sizeof(BspNode));
		self->next = NULL;
	}
	if (self->pTri) {
		operator_delete_sized(self->pTri, 6);
		self->pTri = NULL;
	}
	CHECK(!self->pVertVB);
	CVertList_Free(&self->lv);
}

void BspLMap_dtor(BspLMap *self) {
	if (self->next) {
		BspLMap *next = self->next;
		BspLMap_dtor(next);
		operator_delete_sized(next, sizeof(BspLMap));
		self->next = NULL;
	}
	if (self->lmap2) {
		BspLMap *l2 = self->lmap2;
		BspLMap_dtor(l2);
		operator_delete_sized(l2, sizeof(BspLMap));
		self->lmap2 = NULL;
	}
	Img_Free(&self->info, 4);
	Img_Free(&self->bmp, 4);
}

void BspLMap_Init(BspLMap *self, int width, int height) {
	self->minX = width + 1;
	self->minY = height + 1;
	CBmp_CreateFill(&self->bmp, width, height, 0xffff0000);
	if (g_LmapInfo2 && g_LmapInfo) {
		CBmp_CreateFill(&self->info, width, height, 0x100020);
	}
}

CMaterial *BspLMap_GetMat(const BspLMap *self) {
	int second = g_R_ForceLMap;

	if (second < 0) {
		second = g_LmapSecond;
	}
	if (second && self->lmap2) {
		return self->lmap2->Mat;
	}
	return self->Mat;
}

BspZone *BspZone_ctor(BspZone *self) {
	for (int i = 0; i < 8; i++) {
		self->vb[i].pVertVB = NULL;
		CRenderVB_Reset(&self->vb[i]);
	}
	self->ib.pVertIB = NULL;
	CRenderIB_Reset(&self->ib);
	self->tris = NULL;
	return self;
}

void BspZone_dtor(BspZone *self) {
	CRenderIB_Reset(&self->ib);
	for (int i = 7; i >= 0; i--) {
		CRenderVB_Reset(&self->vb[i]);
	}
}

/* the unwind funclets of the BspZone constructor: one CRenderVB member */
void BspVB_Unwind(CRenderVB *self) {
	CRenderVB_Reset(self);
}

#define BSPZONE_DTOR 0x2824b0

BspZone *BspZone_Delete(BspZone *self, uint32_t flags) {
	if (flags & 2) {
		uint32_t *mem = (uint32_t *)self - 1;
		crt_eh_vector_dtor(self, sizeof(BspZone), *mem, (void *)BspZone_dtor);
		if (flags & 1) {
			operator_delete_array(mem);
		}
		return (BspZone *)mem;
	}
	BspZone_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(BspZone));
	}
	return self;
}

void Table_FreeB4(CTable *t) {
	if (t->p) {
		Delete_Objects(t->p, sizeof(BspZone), (void *)BspZone_dtor);
		t->p = NULL;
		t->n = 0;
	}
}

CRenderVB *BspZone_GetVB(BspZone *self, int frmt) {
	switch (frmt) {
	case VERT_LTEX1:
		return &self->vb[0];
	case VERT_LTEX1N:
		return &self->vb[1];
	case VERT_LTEX2:
		return &self->vb[2];
	case VERT_LTEX2N:
		return &self->vb[3];
	}
	CHECK(0);
	return NULL;
}

void CBsp_dtor(CBsp *self) {
	if (self->lights) {
		void *lights = self->lights;
		BspLights_dtor(lights);
		operator_delete_sized(lights, 4);
		self->lights = NULL;
	}
	if (self->nodes) {
		BspNode *nodes = self->nodes;
		BspNode_dtor(nodes);
		operator_delete_sized(nodes, sizeof(BspNode));
		self->nodes = NULL;
	}
	Model_Destroy(&self->model, 0);
	for (int i = 0; i < self->num_lists; i++) {
		BspTri *tri = self->lists[i];
		if (tri) {
			BspTri_dtor(tri);
			operator_delete_sized(tri, sizeof(BspTri));
			self->lists[i] = NULL;
		}
	}
	CHECK(!self->num_nolmap);
	BspTri *tmp = g_BspTriTmp;
	if (tmp) {
		BspTri_dtor(tmp);
		operator_delete_sized(tmp, sizeof(BspTri));
		g_BspTriTmp = NULL;
	}
	BspLMap *lm = g_BspLMaps;
	if (lm) {
		BspLMap_dtor(lm);
		operator_delete_sized(lm, sizeof(BspLMap));
		g_BspLMaps = NULL;
	}
	Table_FreeB4(&self->zones);
	if (self->name) {
		String_Set(&self->name, NULL);
	}
}


/* the geometry of the lists */

byte Plane_Calc(plane_t *plane, const vec3_t *v1, const vec3_t *v2, const vec3_t *v3) {
	float ax = v2->x - v1->x, ay = v2->y - v1->y, az = v2->z - v1->z;
	float bx = v3->x - v1->x, by = v3->y - v1->y, bz = v3->z - v1->z;
	vec3_t n;

	n.x = ay * bz - az * by;
	n.y = az * bx - ax * bz;
	n.z = ax * by - ay * bx;
	plane->n = n;
	if (n.x == 0.0f && n.y == 0.0f && n.z == 0.0f) {
		plane->d = 0.0f;
		return 0;
	}
	float len = (float)__builtin_sqrt((double)(n.x * n.x + n.y * n.y + n.z * n.z));
	if ((double)len != 0.0) {
		float inv = (float)(1.0 / (double)len);
		n.x = plane->n.x * inv;
		n.y = plane->n.y * inv;
		n.z = plane->n.z * inv;
	}
	plane->n = n;
	plane->d = -(n.y * v1->y + n.x * v1->x + n.z * v1->z);
	return 1;
}

/* the distance of two points the way the lists measure it: y first */
static inline float Bsp_Dist(float dx, float dy, float dz) {
	return (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
}

void BspTri_CalcCenter(BspTri *tri) {
	const vec3_t *v = tri->v;
	float cx = (v[0].x + v[1].x + v[2].x) / 3.0f;
	float cy = (v[0].y + v[1].y + v[2].y) / 3.0f;
	float cz = (v[0].z + v[1].z + v[2].z) / 3.0f;
	float d0 = Bsp_Dist(cx - v[0].x, cy - v[0].y, cz - v[0].z);
	float d1 = Bsp_Dist(cx - v[1].x, cy - v[1].y, cz - v[1].z);
	float d2 = Bsp_Dist(cx - v[2].x, cy - v[2].y, cz - v[2].z);

	tri->center = (vec3_t){ cx, cy, cz };
	tri->radio = Float_MaxSS(d0, Float_MaxSS(d1, d2));
	float ax = v[1].x - v[0].x, ay = v[1].y - v[0].y, az = v[1].z - v[0].z;
	float bx = v[2].x - v[0].x, by = v[2].y - v[0].y, bz = v[2].z - v[0].z;
	float nx = ay * bz - az * by;
	float ny = az * bx - ax * bz;
	float nz = ax * by - ay * bx;
	tri->area = (float)__builtin_sqrt((double)(ny * ny + nx * nx + nz * nz)) * 0.5f;
}

/* the vertices of tri into box (minss and maxss with the box first) */
box_t *BspTri_ListBox(box_t *out, const BspTri *list) {
	out->min = (vec3_t){ 3.4028235e38f, 3.4028235e38f, 3.4028235e38f };
	out->max = (vec3_t){ -3.4028235e38f, -3.4028235e38f, -3.4028235e38f };
	for (; list; list = list->next) {
		Box_AddTri(out, list);
	}
	return out;
}

box_t *BspTri_AddBox(box_t *out, const box_t *in, const BspTri *tri) {
	*out = *in;
	CHECK(tri);
	Box_AddTri(out, tri);
	return out;
}

void BspTri_CalcPlanes(BspTri *list) {
	for (; list; list = list->next) {
		CHECK(Plane_Calc(&list->plane, &list->v[0], &list->v[1], &list->v[2]));
		float ax = (float)__builtin_fabs((double)list->plane.n.x);
		float ay = (float)__builtin_fabs((double)list->plane.n.y);
		float az = (float)__builtin_fabs((double)list->plane.n.z);
		if (ax >= ay && ax >= az) {
			list->axis = 0;
		} else if (ay >= ax && ay >= az) {
			list->axis = 1;
		} else if (az >= ax && az >= ay) {
			list->axis = 2;
		} else {
			CHECK(0);
		}
	}
}

/* the component of the normal along the axis of the list */
static inline float BspTri_AxisN(const BspTri *tri) {
	if (tri->axis == 0) {
		return tri->plane.n.x;
	}
	if (tri->axis == 1) {
		return tri->plane.n.y;
	}
	return tri->plane.n.z;
}

int BspTri_Side(const BspTri *tri) {
	float s = BspTri_AxisN(tri);

	if (0.0f > s) {
		return -1;
	}
	int r = s > 0.0f;
	CHECK(r);
	return r;
}

void Bsp_TexelToWorld(vec3_t *out, float u, float v, int axis, float scale, const plane_t *pl) {
	double inv = 1.0 / (double)scale;
	float x = (float)((double)(axis == 0 ? 0.0f : u) * inv);
	float y = (float)((double)(axis == 0 ? u : axis == 1 ? 0.0f : v) * inv);
	float z = (float)((double)(axis == 0 || axis == 1 ? v : 0.0f) * inv);

	if (axis == 0) {
		if (pl->n.x != 0.0f) {
			out->y = y;
			out->z = z;
			out->x = -((pl->n.z * z + pl->n.y * y + pl->d) / pl->n.x);
			return;
		}
	} else if (axis == 1) {
		if (pl->n.y != 0.0f) {
			out->x = x;
			out->z = z;
			out->y = -((pl->n.z * z + pl->n.x * x + pl->d) / pl->n.y);
			return;
		}
	} else if (pl->n.z != 0.0f) {
		z = -((pl->n.y * y + pl->n.x * x + pl->d) / pl->n.z);
	}
	out->x = x;
	out->y = y;
	out->z = z;
}

void Bsp_WorldToTexel(float *out, const vec3_t *v, int axis, float scale) {
	if (axis == 0) {
		out[0] = v->y * scale;
		out[1] = v->z * scale;
	} else if (axis == 1) {
		out[0] = v->x * scale;
		out[1] = v->z * scale;
	} else {
		out[0] = v->x * scale;
		out[1] = v->y * scale;
	}
}

void BspTri_Interp(const BspTri *tri, const vec3_t *p, vec3_t *n, float *uv, byte lmap) {
	CHECK(tri);
	vec3_t v0 = tri->v[0], v1 = tri->v[1], v2 = tri->v[2];
	vec3_t n0 = tri->n[0], n1 = tri->n[1], n2 = tri->n[2];
	const float (*t)[2] = lmap ? tri->uvl : tri->uv;
	float e1x = v1.x - v0.x, e1y = v1.y - v0.y, e1z = v1.z - v0.z;
	float e2x = v2.x - v0.x, e2y = v2.y - v0.y, e2z = v2.z - v0.z;
	float px = tri->plane.n.x, py = tri->plane.n.y, pz = tri->plane.n.z;
	float a = py * e2z - pz * e2y;
	float b = pz * e2x - px * e2z;
	float c = px * e2y - py * e2x;
	float det = b * e1y + a * e1x + c * e1z;
	double inv = 1.0 / (double)det;

	CHECK((double)det != 0.0);
	float dx = p->x - v0.x, dy = p->y - v0.y, dz = p->z - v0.z;
	float s = (float)((double)(dy * b + dx * a + dz * c) * inv);
	float r = (float)((double)((dy * e1z - dz * e1y) * px + (dz * e1x - dx * e1z) * py + (dx * e1y - dy * e1x) * pz) *
		inv);
	if (uv) {
		uv[0] = (t[1][0] - t[0][0]) * s + (t[2][0] - t[0][0]) * r + t[0][0];
		uv[1] = (t[1][1] - t[0][1]) * s + (t[2][1] - t[0][1]) * r + t[0][1];
	}
	if (n) {
		float nx = (n1.x - n0.x) * s + (n2.x - n0.x) * r + n0.x;
		float ny = (n1.y - n0.y) * s + (n2.y - n0.y) * r + n0.y;
		float nz = (n1.z - n0.z) * s + (n2.z - n0.z) * r + n0.z;
		float len = (float)__builtin_sqrt((double)(ny * ny + nx * nx + nz * nz));
		if ((double)len != 0.0) {
			float k = (float)(1.0 / (double)len);
			nx = nx * k;
			ny = ny * k;
			nz = nz * k;
		}
		*n = (vec3_t){ nx, ny, nz };
	}
}

/* the radical inverse of i in base 2: its bits reversed */
static inline uint32_t Bits_Reverse(uint32_t v) {
	v = v << 16 | v >> 16;
	v = ((v >> 1 ^ v << 1) & 0x55555555) ^ v << 1;
	v = ((v >> 2 ^ v << 2) & 0x33333333) ^ v << 2;
	v = ((v >> 4 ^ v << 4) & 0x0f0f0f0f) ^ v << 4;
	v = ((v >> 8 ^ v << 8) & 0x00ff00ff) ^ v << 8;
	return v;
}

float *Bsp_Hammersley(float *out, uint32_t i, uint32_t n) {
	out[0] = Uint_ToFloat(i) / Uint_ToFloat(n);
	out[1] = (float)((double)Uint_ToFloat(Bits_Reverse(i)) * 2.3283064365386963e-10);
	return out;
}

void Bsp_ColorIntens(uint32_t *out, const float *rgb) {
	float r = rgb[0], g = rgb[1], b = rgb[2];
	float m = Float_MaxSS(b, Float_MaxSS(g, r));
	byte c[4] = { 0, 0, 0, 0 };
	float i = 0.0f;

	if (m > 0.0f) {
		c[0] = (byte)(int)((double)(r / m * 255.0f) + 0.5);
		c[1] = (byte)(int)((double)(g / m * 255.0f) + 0.5);
		c[2] = (byte)(int)((double)(b / m * 255.0f) + 0.5);
		i = m;
	}
	memcpy(&out[0], c, 4);
	memcpy(&out[1], &i, 4);
}

void BspTri_GetExt(const BspTri *tri, int *x0, int *y0, int *x1, int *y1) {
	*x0 = tri->ext[0];
	*y0 = tri->ext[1];
	*x1 = tri->ext[2];
	*y1 = tri->ext[3];
}

void BspTri_GetExt2(const BspTri *tri, int *x0, int *y0, int *x1, int *y1) {
	*x0 = tri->ext2[0];
	*y0 = tri->ext2[1];
	*x1 = tri->ext2[2];
	*y1 = tri->ext2[3];
}

byte BspTri_IsTexel(int x0, int y0, int x, int y, uint32_t color, const BspTri *tri) {
	uint32_t dx = (uint32_t)(x - x0);
	uint32_t dy = (uint32_t)(y - y0);

	if (dx >= (uint32_t)g_ShadowMapTexX || dy >= (uint32_t)g_ShadowMapTexY) {
		return 0;
	}
	return Color_SwapRB(*CBmp_Pixel(&tri->mask, dx, dy)) == color;
}

int BspTri_Zone(CNode3D *node) {
	if (!node || node->flags >> 28 & 1) {
		return -1;
	}
	return Sector_OfNode(node);
}

const char *BspTri_ZoneName(const BspTri *tri) {
	if (tri->zone == -1) {
		return "_unasigned_";
	}
	return Sector_GetName(tri->zone);
}

int BspTri_Count(const BspTri *list) {
	int n = 0;

	for (; list; list = list->next) {
		n++;
	}
	return n;
}

int BspTri_CmpId(const void *a, const void *b) {
	return ((const int *)a)[1] - ((const int *)b)[1];
}

/* a node out of the map: unlinked (release) and, when nothing else holds it, marked to be deleted */
void Bsp_MarkNode(CNode3D *node, byte release) {
	if (release) {
		CNode3D_FreeObj3D(node);
	}
	if (node->Refs - (node->prev != NULL) - (node->next != NULL) == 0) {
		node->flags |= 0x20008;
	}
}


/* drawing the map */

/* the pass of a material: 0 solid, 1 transparent with attrib 0x10, 2 transparent */
static byte Bsp_MatPass(const CMaterial *m) {
	if (!Mat_IsTransp(m) || m->params.attrib & 0x800) {
		return 0;
	}
	return m->params.attrib & 0x10 ? 1 : 2;
}

void __stdcall BspTri_Draw(BspTri *tri) {
	if (tri->pVertVB && g_UseVB) {
		Render_DrawVBIBFmt(tri->pVertVB, tri->pVertIB, tri->lv.frmt, (uint32_t)tri->lv.nVert, (uint32_t)tri->numTri,
			(uint32_t)tri->baseVert, (uint32_t)tri->startTri);
	} else {
		Render_DrawListIdx(&tri->lv, tri->pTri, tri->numTri, 0);
	}
	g_RenderTris += tri->numTri;
	g_RenderVerts += tri->lv.nVert;
	g_BspTris += tri->numTri;
	g_BspVerts += tri->lv.nVert;
}

/* the modulation of the light maps: 4x with modulate4x on a device without gamma2x */
static inline int Bsp_LmapMode(void) {
	return !g_Gamma2x && g_ShadowMapModulate4x ? 4 : 1;
}

void CBsp_DrawTri(CBsp *self, BspTri *tri, byte pass) {
	CMaterial *mat = tri->Mat;

	if (Bsp_MatPass(mat) != pass) {
		return;
	}
	if (tri->flags & BSPTRI_NOLMAP && g_RenderPass == 2) {
		return;
	}
	if (*g_MapRenderMat && !Wild_Match(g_MapRenderMat, mat->Name)) {
		return;
	}
	if (*g_MapRenderMap) {
		if (!CMaterial_GetMapName(tri->Mat, MAP_DIFFUSE)) {
			return;
		}
		const char *pat = g_MapRenderMap;
		if (!Wild_Match(pat, Path_FileName(CMaterial_GetMapName(tri->Mat, MAP_DIFFUSE)))) {
			return;
		}
	}
	if (g_LightmapMode2 && !(tri->flags & BSPTRI_NOLMAP)) {
		int mode = Bsp_LmapMode();
		Render_SetLightmapMat(BspLMap_GetMat(tri->LMap), mode);
		Render_SetMaterial(self->matFX, 0);
		BspTri_Draw(tri);
		Render_SetLightmapMat(NULL, 1);
		return;
	}
	CHECK(!tri->Mat->params.lighting);
	CHECK(!tri->LMap || !tri->LMap->Mat->params.lighting);
	if (!g_ShadowMapMultitext) {
		CHECK(0);
		Render_SetMaterial(tri->Mat, 0);
		return;
	}
	if (!(tri->flags & BSPTRI_NOLMAP) && !g_LightmapOff && g_MaxLightmap > 0) {
		int mode = Bsp_LmapMode();
		Render_SetLightmapMat(BspLMap_GetMat(tri->LMap), mode);
	}
	Render_SetMaterial(tri->Mat, 0);
	BspTri_Draw(tri);
	Render_SetLightmapMat(NULL, 1);
}

/* the zone of sector (-1 for the lists without one) */
static BspZone *CBsp_Zone(CBsp *self, int sector) {
	uint32_t i = (uint32_t)(sector + 1);

	TABLE_CHECK(self->zones.p, i, self->zones.n);
	return &((BspZone *)self->zones.p)[i];
}

void CBsp_DrawZone(CBsp *self, int sector, byte pass) {
	BspTri *tri = CBsp_Zone(self, sector)->tris;

	if (g_MapDrawSector >= -1 && g_MapDrawSector != sector) {
		return;
	}
	for (; tri; tri = tri->zone_next) {
		CHECK(tri->zone == sector);
		CBsp_DrawTri(self, tri, pass);
	}
}

/* the lists of the pass in a zone into g_TriList */
static int CBsp_ListZone(BspTri *tri, int sector, byte pass, int n) {
	for (; tri; tri = tri->zone_next) {
		CHECK(tri->zone == sector);
		if (Bsp_MatPass(tri->Mat) == pass) {
			CHECK(n < BSP_MAX_TRILIST);
			g_TriList[n++] = tri;
		}
	}
	return n;
}

void CBsp_DrawPass(CBsp *self, byte pass) {
	byte perSector = g_MapDrawBySector != 0;

	Render_SetWorldMatrix(g_MatIdentity, 0);
	if (!g_RenderSectors) {
		return;
	}
	if (g_RenderSectors == 2) {
		for (int i = 0; i < self->num_lists; i++) {
			CBsp_DrawTri(self, self->lists[i], pass);
		}
		return;
	}
	if (perSector) {
		if (g_PortalsOn) {
			for (int i = 0; i < g_NumPaintZones; i++) {
				const PaintZone *zp = &g_PaintZones[pass ? g_NumPaintZones - i - 1 : i];
				if (Portals_SetView(zp)) {
					CBsp_DrawZone(self, zp->zone, pass);
				}
			}
		}
		Portals_SetView(NULL);
		CBsp_DrawZone(self, -1, pass);
		return;
	}
	int n = 0;
	for (int i = 0; i < g_NumPaintZones; i++) {
		const PaintZone *zp = &g_PaintZones[pass ? g_NumPaintZones - i - 1 : i];
		n = CBsp_ListZone(CBsp_Zone(self, zp->zone)->tris, zp->zone, pass, n);
	}
	TABLE_CHECK(self->zones.p, 0, self->zones.n);
	n = CBsp_ListZone(((BspZone *)self->zones.p)->tris, -1, pass, n);
	crt_qsort(g_TriList, (size_t)n, sizeof(BspTri *), (int (*)(const void *, const void *))(void *)BspTri_CmpMat);
	for (int i = 0; i < n; i++) {
		CBsp_DrawTri(self, g_TriList[i], pass);
	}
}

void CBsp_DrawWire(CBsp *self) {
	if (g_KeyF8 && g_KeyX) {
		g_KeyF8 = 0;
		g_WireFrame = (g_WireFrame + 1) % 3;
	}
	if (!g_WireFrame) {
		return;
	}
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_FILLMODE, 2);
	g_MatFX1 = self->matWire;
	g_MatFX2 = g_WireFrame == 2 ? self->matWire : NULL;
	g_LightmapMode2++;
	if (g_PortalsOn) {
		for (int i = 0; i < g_NumPaintZones; i++) {
			CBsp_DrawZone(self, g_PaintZones[i].zone, 0);
			CBsp_DrawZone(self, g_PaintZones[i].zone, 1);
			CBsp_DrawZone(self, g_PaintZones[i].zone, 2);
		}
	}
	CBsp_DrawZone(self, -1, 0);
	CBsp_DrawZone(self, -1, 1);
	CBsp_DrawZone(self, -1, 2);
	dev = g_pd3dDevice;
	g_LightmapMode2--;
	g_MatFX1 = NULL;
	g_MatFX2 = NULL;
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_FILLMODE, 3);
}

void CBsp_DrawFX(CBsp *self) {
	if (g_RenderOff) {
		return;
	}
	Render_SetWorldMatrix(g_MatIdentity, 0);
	CBsp_DrawWire(self);
	if (g_ProfMode == 2) {
		Font_PrintfXY(g_FntComic14, 10.0f, 260.0f, "tri: %d %d", g_BspTris, g_BspVerts);
	}
	g_BspTris = 0;
	g_BspVerts = 0;
}

void CBsp_Render(CBsp *self) {
	if (g_RenderOff) {
		return;
	}
	Render_SetWorldMatrix(g_MatIdentity, 0);
	if (!g_PortalsOn) {
		g_NumPaintZones = 0;
	} else if (g_ShowPortals) {
		Text_Printf(10, 0xdc, NULL);
	}
	if (g_RenderPass != 3 && g_RenderPass != 0xd) {
		CBsp_DrawPass(self, 0);
	}
	if (g_RenderPass == 0 || g_RenderPass == 3 || g_RenderPass == 0xd) {
		CBsp_DrawFX(g_Bsp);
		CBsp_DrawPass(self, 1);
	}
}

/* the sector of the eye for the portals (info: the counters on the screen instead) */
void Bsp_PortalInfo(byte info) {
	if (!g_PortalsOn) {
		return;
	}
	if (info) {
		Font_SetColor(g_FntComic14, g_BspSectorOk ? 0xffffff : 0x800000);
		Font_Printf(g_FntComic14, 10.0f, 200.0f, 2, "%d %d", g_BspSectorSeen, g_BspSectorErrors);
		Font_Printf(g_FntComic14, 10.0f, 220.0f, 2, "%d", g_NumPaintZones);
		Font_Printf(g_FntComic14, 10.0f, 240.0f, 2, "%d", g_PaintNum);
		return;
	}
	CGround *ground = World_Get()->ObjGround;
	CHECK(ground);
	vec3_t eye = *Render_GetEyePos();
	GroundRay res;
	int s;

	g_BspSectorOk = 0;
	res.flags = 0x202;
	res.y = eye.y;
	if (CGround_RayVert(ground, eye.x, eye.z, &res) && res.sector != -1) {
		vec3_t a = { eye.x, res.hitY, eye.z };
		g_BspSectorOk = 1;
		s = Sector_Traverse(res.sector, &a, &eye, 0);
		g_BspSectorSeen = s;
	} else {
		s = g_BspSectorSeen;
	}
	if (!g_BspSectorOk) {
		s = Sector_Traverse(s, &g_BspEyePrev, &eye, 0);
		g_BspSectorSeen = s;
		if (!g_BspSectorOk) {
			g_BspSectorErrors++;
			g_ProfMode = g_ShowInfoOnSectorError ? 2 : g_ProfMode;
		}
	}
	g_BspEyePrev = eye;
	Portals_Cull((uint32_t)s);
}

void CBsp_ReleaseD3D(CBsp *self) {
	for (int i = 0; i < self->num_lists; i++) {
		if (self->lists[i]->pVertVB) {
			COM_RELEASE(self->lists[i]->pVertVB);
			self->lists[i]->pVertVB = NULL;
		}
		if (self->lists[i]->pVertIB) {
			COM_RELEASE(self->lists[i]->pVertIB);
			self->lists[i]->pVertIB = NULL;
		}
	}
	for (BspNode *n = self->nodes; n; n = n->next) {
		if (n->pVertVB) {
			COM_RELEASE(n->pVertVB);
			n->pVertVB = NULL;
		}
	}
	for (int i = 0; i < self->zones.n; i++) {
		TABLE_CHECK(self->zones.p, i, self->zones.n);
		BspZone *z = &((BspZone *)self->zones.p)[i];
		for (int k = 0; k < 8; k++) {
			if (z->vb[k].pVertVB) {
				COM_RELEASE(z->vb[k].pVertVB);
				z->vb[k].pVertVB = NULL;
			}
		}
		if (z->ib.pVertIB) {
			COM_RELEASE(z->ib.pVertIB);
			z->ib.pVertIB = NULL;
		}
	}
	CModel_ReleaseD3D(self->model, g_Engine->device, 1);
}


/* the global functions called by mundo.cpp */

void Bsp_Free(void) {
	if (!g_ShadowMapUse) {
		return;
	}
	CBsp *bsp = g_Bsp;
	if (bsp) {
		CBsp_dtor(bsp);
		operator_delete_sized(bsp, sizeof(CBsp));
		g_Bsp = NULL;
	}
}

void Bsp_ReleaseD3D(void) {
	if (!g_ShadowMapUse) {
		return;
	}
	void *vb = g_pBSP_VB;
	if (vb) {
		COM_RELEASE(vb);
		g_pBSP_VB = NULL;
	}
	CBsp_ReleaseD3D(g_Bsp);
}

void Bsp_InitD3D(void) {
	if (!g_ShadowMapUse) {
		return;
	}
	CBsp_InitD3D(g_Bsp);
	if (!g_ShadowMapMultitext) {
		CHECK(g_Engine);
		CHECK(!g_pBSP_VB);
	}
}

/* the floor drawn again for the stencil shadows (the callback of g_StencilRender) */
void Bsp_RenderStencil(void) {
	CHECK(g_ShadowsStencil);
	if (!g_CalcLmap2 && g_RenderPass == 2) {
		return;
	}
	if (g_RenderPass != 0xc) {
		g_LmapSecond = g_RenderPass == 2;
		CBsp_Render(g_Bsp);
		g_LmapSecond = 0;
	}
	CModel_Render(g_Bsp->model);
	E3D_FlushBatch();
}

void Bsp_Render(void) {
	if (!g_ShadowMapUse || !g_RenderMap) {
		return;
	}
	g_StencilRender = g_ShadowsStencil ? (void *)Bsp_RenderStencil : g_StencilRender;
	CBsp_Render(g_Bsp);
	CModel_UpdateAnims(g_Bsp->model, 0.0f, -1);
	CModel_Render(g_Bsp->model);
	E3D_FlushBatch();
}

void Bsp_RenderTransp(void) {
	if (g_RenderOff || !g_ShadowMapUse || !g_RenderMap) {
		return;
	}
	CBsp_DrawPass(g_Bsp, 2);
	if (g_ShowPortals) {
		Bsp_PortalInfo(0);
	}
	if (g_ProfMode == 2) {
		Bsp_PortalInfo(1);
	}
	if (g_Bsp->lights) {
		BspLights_Render(g_Bsp->lights);
	}
}

void Bsp_RenderPortals(void) {
	if (g_ShadowMapUse && g_RenderMap && !g_ShowPortals) {
		Bsp_PortalInfo(0);
	}
}


/* the light map computation helpers */

/* a direction on the unit sphere: height z, angle u turns */
float *Bsp_DirSphere(float *out, float z, float u) {
	float r = (float)__builtin_sqrt((double)(1.0f - z * z));
	double a = (double)(float)((double)u * M_2PI_D);

	out[0] = (float)Game_Cos(a) * r;
	out[1] = (float)Game_Sin(a) * r;
	out[2] = z;
	return out;
}

/* the texels a box covers on the plane of axis at scale */
void Bsp_TexelBox(const box_t *b, int axis, float scale, int *x0, int *y0, int *x1, int *y1) {
	float u0, v0, u1, v1;

	if (axis == 0) {
		u0 = b->min.y * scale;
		v0 = b->min.z * scale;
		u1 = b->max.y * scale;
		v1 = b->max.z * scale;
	} else if (axis == 1) {
		u0 = b->min.x * scale;
		v0 = b->min.z * scale;
		u1 = b->max.x * scale;
		v1 = b->max.z * scale;
	} else {
		u0 = b->min.x * scale;
		v0 = b->min.y * scale;
		u1 = b->max.x * scale;
		v1 = b->max.y * scale;
	}
	*x0 = (int)crt_floor((double)u0 - 0.5);
	*y0 = (int)crt_floor((double)v0 - 0.5);
	*x1 = (int)crt_floor((double)u1 + 0.5);
	*y1 = (int)crt_floor((double)v1 + 0.5);
}

byte Bsp_SameSize(const box_t *a, int axis, float scale, const box_t *b) {
	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;

	Bsp_TexelBox(a, axis, scale, &x0, &y0, &x1, &y1);
	int k = g_LightmapLimitUV ? 4 : 1;
	int w = x1 - x0;
	int h = y1 - y0;
	byte okW = w * k < g_ShadowMapTexX;
	byte okH = h * k < g_ShadowMapTexY;
	x0 = y0 = x1 = y1 = 0;
	Bsp_TexelBox(b, axis, scale, &x0, &y0, &x1, &y1);
	if (w == x1 - x0 && h == y1 - y0) {
		return 1;
	}
	return okW && okH;
}

void BspTri_SetExt(BspTri *tri, int x0, int y0, int x1, int y1, byte notify) {
	char buf[0x100];

	tri->ext[2] = x1;
	tri->ext[3] = y1;
	tri->ext[1] = y0;
	tri->ext[0] = x0;
	int w = x1 - x0 + 1;
	int h = y1 - y0 + 1;
	if ((w > g_ShadowMapTexX || h > g_ShadowMapTexY) && notify) {
		if (g_TXF_Helper) {
			crt_exit_now(-1);
		}
		Bprintf(buf, sizeof(buf), "EXTENS_UV: %dx%d", w, h);
		exe_MessageBoxA(NULL, buf, "Info", 0x2040);
	}
	if (w < 1) {
		w = 0;
	} else if (w >= g_ShadowMapTexX) {
		w = g_ShadowMapTexX;
	}
	tri->lx = w;
	if (h < 1) {
		h = 0;
	} else if (h >= g_ShadowMapTexY) {
		h = g_ShadowMapTexY;
	}
	tri->ly = h;
}

/* the texel of the light map at p */
byte BspTri_TexelOf(const BspTri *tri, const vec3_t *p, int *x, int *y) {
	float uv[2];

	BspTri_Interp(tri, p, NULL, uv, 1);
	*x = (int)crt_floor((double)((float)g_ShadowMapTexX * uv[0]));
	*y = (int)crt_floor((double)((float)g_ShadowMapTexY * uv[1]));
	return 1;
}

#define BSP_EPS_NORMAL 9.999999747378752e-05	/* 1e-4f as double */

/* vertex nv of act is a vertex of tri with the same normal */
byte BspTri_VertEqual(const BspTri *act, uint32_t nv, const BspTri *tri) {
	int idx;

	CHECK(act);
	CHECK(tri);
	CHECK(nv < 3);
	int id = act->vid[nv];
	if (id == tri->vid[0]) {
		idx = 0;
	} else if (id == tri->vid[1]) {
		idx = 1;
	} else if (id == tri->vid[2]) {
		idx = 2;
	} else if (Bsp_Vec3Equal(&act->v[nv], &tri->v[0])) {
		idx = 0;
	} else if (Bsp_Vec3Equal(&act->v[nv], &tri->v[1])) {
		idx = 1;
	} else if (Bsp_Vec3Equal(&act->v[nv], &tri->v[2])) {
		idx = 2;
	} else {
		return 0;
	}
	CHECK(Bsp_Vec3Equal(&act->v[nv], &tri->v[idx]));
	return __builtin_fabs((double)(act->n[nv].x - tri->n[idx].x)) < BSP_EPS_NORMAL &&
		__builtin_fabs((double)(act->n[nv].y - tri->n[idx].y)) < BSP_EPS_NORMAL &&
		__builtin_fabs((double)(act->n[nv].z - tri->n[idx].z)) < BSP_EPS_NORMAL;
}

/* the color of the texture tex at the texel (u, v) of the light map, 0..1 */
float *Bsp_SampleTex(float *out, float u, float v, int axis, float scale, const BspTri *tri, const CBmp *tex) {
	vec3_t p;
	float uv[2];
	uint32_t c;

	Bsp_TexelToWorld(&p, u, v, axis, scale, &tri->plane);
	BspTri_Interp(tri, &p, NULL, uv, 0);
	float s = uv[0], t = uv[1];
	if (!(s > -3.4028235e38f && 3.4028235e38f > s && t > -3.4028235e38f && 3.4028235e38f > t)) {
		s = 0.0f;
		t = 0.0f;
	}
	s = s - (float)crt_floor((double)s);
	t = t - (float)crt_floor((double)t);
	CBmp_Sample(tex, &c, s, t);
	out[0] = (float)(int)(c & 0xff) / 255.0f;
	out[1] = (float)(int)(c >> 8 & 0xff) / 255.0f;
	out[2] = (float)(int)(c >> 16 & 0xff) / 255.0f;
	return out;
}

/* the average of (n + 1)^2 samples of the texel x, y */
float *Bsp_SampleTexAA(float *out, int x, int y, int axis, float scale, const BspTri *tri, const CBmp *tex, int n) {
	float r = 0.0f, g = 0.0f, b = 0.0f;
	float tmp[3];

	if (n >= 0) {
		float fn = (float)n;
		for (int i = 0; i <= n; i++) {
			float u = (float)i / fn + (float)x;
			for (int j = 0; j <= n; j++) {
				const float *c = Bsp_SampleTex(tmp, u, (float)j / fn + (float)y, axis, scale, tri, tex);
				r = r + c[0];
				g = c[1] + g;
				b = c[2] + b;
			}
		}
	}
	float k = (float)((n + 1) * (n + 1));
	out[0] = r / k;
	out[1] = g / k;
	out[2] = b / k;
	return out;
}

/* the areas of the level of detail for a distance (grande: the lists of large area lights, counted) */
CTable *BspTri_GetAreas(BspTri *tri, float dist, byte big) {
	float lod = g_BspLodLevel;

	if (!big) {
		if (g_BspLodDist[0] * lod > dist) {
			return &tri->areas[0];
		}
		if (g_BspLodDist[2] * lod > dist) {
			return &tri->areas[1];
		}
		if (g_BspLodDist[4] * lod > dist) {
			return &tri->areas[2];
		}
		return &tri->areas[3];
	}
	float k = tri->glowMax;
	float c = 0.25f;
	if (!(0.25f >= k)) {
		c = Float_MinSS(10.0f, k);
	}
	for (int i = 0; i < 3; i++) {
		if (g_BspLodDist[i * 2 + 1] * lod * c > dist) {
			g_BspLodCount[i]++;
			return &tri->areas[i];
		}
	}
	g_BspLodCount[3]++;
	return &tri->areas[3];
}

/* the form factor of the patch b (point, normal, area) seen from a */
static float Bsp_FF(const vec3_t *pa, const vec3_t *na, const vec3_t *pb, const vec3_t *nb, float area) {
	float dy = pa->y - pb->y;
	float dx = pa->x - pb->x;
	float dz = pa->z - pb->z;
	float len = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));

	if (len == 0.0f) {
		return 0.0f;
	}
	float ca = -((na->y * dy + na->x * dx + na->z * dz) / len);
	if (0.0f >= ca) {
		return 0.0f;
	}
	float cb = (nb->y * dy + nb->x * dx + nb->z * dz) / len;
	if (0.0f >= cb) {
		return 0.0f;
	}
	return (float)((double)area / ((double)len * M_PI_D * (double)len + (double)area)) * (cb * ca);
}

float Bsp_FormFactor(const vec3_t *pa, const vec3_t *na, const vec3_t *pb, const vec3_t *nb, float area) {
	return Bsp_FF(pa, na, pb, nb, area);
}

float BspRegion_FormFactor(const BspRegion *a, const BspRegion *b) {
	return Bsp_FF(&a->center, &a->plane.n, &b->center, &b->plane.n, b->area);
}

/* add plus the light of the links into the tree */
void BspRegion_Accumulate(BspRegion *r, const float *add) {
	for (; r; r = r->child[3]) {
		float *accum = r->accum;
		accum[0] = add[0] + accum[0];
		accum[1] = add[1] + accum[1];
		accum[2] = add[2] + accum[2];
		accum[3] = add[3] + accum[3];
		for (BspLink *l = r->links; l; l = l->next) {
			const BspRegion *o = l->region;
			float ff = l->ff;
			accum[0] = o->color[0] * ff + accum[0];
			accum[1] = accum[1] + o->color[1] * ff;
			accum[2] = accum[2] + o->color[2] * ff;
			accum[3] = accum[3] + o->color[3] * ff;
		}
		BspRegion_Accumulate(r->child[0], accum);
		BspRegion_Accumulate(r->child[1], accum);
		BspRegion_Accumulate(r->child[2], accum);
		add = accum;
	}
}

/* the light at texel x, y into acc and count it, for a texel of the list */
void BspTri_SumTexel(const BspTri *tri, uint32_t x, uint32_t y, float *acc, int *n) {
	if (x >= (uint32_t)tri->light.width || y >= (uint32_t)tri->light.height) {
		return;
	}
	if (!*CBmp_Pixel(&tri->mask, x, y)) {
		return;
	}
	const float *p = CBmpFlt2_Pixel(&tri->light, x, y);
	float r = p[0], g = p[1], b = p[2];
	acc[0] = acc[0] + r;
	acc[1] = acc[1] + g;
	acc[2] = acc[2] + b;
	acc[3] = acc[3] + 1.0f;
	(*n)++;
}

/* the average light of the texels of the list around x, y (5 x 5) */
float *BspTri_Average(float *out, const BspTri *tri, int x, int y) {
	int n = 0;
	float one = 1.0f;

	out[2] = 0.0f;
	out[1] = 0.0f;
	out[0] = 0.0f;
	out[3] = 1.0f;
	for (int j = 0; j < 5; j++) {
		uint32_t yy = (uint32_t)(y - 2 + j);
		for (int i = -2; i <= 2; i++) {
			uint32_t xx = (uint32_t)(x + i);
			if (xx >= (uint32_t)tri->light.width || yy >= (uint32_t)tri->light.height) {
				continue;
			}
			if (!*CBmp_Pixel(&tri->mask, xx, yy)) {
				continue;
			}
			const float *p = CBmpFlt2_Pixel(&tri->light, xx, yy);
			float r = p[0], g = p[1], b = p[2];
			n++;
			out[0] = out[0] + r;
			out[1] = out[1] + g;
			out[2] = b + out[2];
			out[3] = out[3] + one;
		}
	}
	if (n) {
		float k = one / (float)n;
		out[0] = out[0] * k;
		out[1] = out[1] * k;
		out[2] = out[2] * k;
		out[3] = out[3] * k;
	}
	return out;
}


/* the light map pages */

/* a new empty page (the inline constructor clears only these) */
static BspLMap *BspLMap_New(void) {
	BspLMap *lm = operator_new(sizeof(BspLMap));

	if (!lm) {
		return NULL;
	}
	lm->bmp.img = NULL;
	lm->info.img = NULL;
	lm->next = NULL;
	lm->Mat = NULL;
	lm->lmap2 = NULL;
	lm->id = 0;
	return lm;
}

/* the alpha of a texel of a page: 0xff while free */
static inline byte BspLMap_Free(const CBmp *bmp, uint32_t x, uint32_t y) {
	return ((const byte *)CBmp_Pixel(bmp, x, y))[3] == 0xff;
}

byte BspLMap_Find(const CBmp *bmp, int w, int h, int *rx, int *ry) {
	CHECK(bmp);
	CHECK(rx && ry);
	byte rot = w > h;
	if (g_LightmapPackMode == 1) {
		rot = h > w;
	} else if (g_LightmapPackMode == 2) {
		rot = 0;
	} else if (g_LightmapPackMode == 3) {
		rot = 1;
	}
	int pad = g_LightmapDxtPadding ? g_LightmapDxtPadding : 1;
	w = w - 1 + pad;
	w -= w % pad;
	h = h - 1 + pad;
	h -= h % pad;
	int a = bmp->width - w;
	int b = bmp->height - h;
	int nx = (rot ? b : a) + 1;
	int ny = (rot ? a : b) + 1;
	for (int j = 0; j < ny; j += pad) {
		for (int i = 0; i < nx; i += pad) {
			int x = rot ? j : i;
			int y = rot ? i : j;
			if (!BspLMap_Free(bmp, (uint32_t)x, (uint32_t)y)) {
				continue;
			}
			byte ok = 1;
			for (int k = 0; k < h; k++) {
				for (int m = 0; m < w; m++) {
					if (!BspLMap_Free(bmp, (uint32_t)(x + m), (uint32_t)(y + k))) {
						ok = 0;
						break;
					}
				}
			}
			if (ok) {
				*rx = x;
				*ry = y;
				return 1;
			}
		}
	}
	return 0;
}

/* the light orig at (ox, oy) size w x h into the page at (dx, dy), scaled down to the modulation range; the texels
 * of no list (mask 0) inverted */
void BspLMap_Copy(CBmp *dst, int dx, int dy, const CBmpFlt2 *orig, const CBmp *mask, uint32_t ox, uint32_t oy,
	int w, int h) {
	CHECK(orig);
	CHECK(dst);
	if (!orig->img) {
		return;
	}
	for (int j = 0; j < h; j++) {
		uint32_t y = oy + (uint32_t)j;
		for (int i = 0; i < w; i++) {
			uint32_t x = ox + (uint32_t)i;
			const float *p = CBmpFlt2_Pixel(orig, x, y);
			float pr = p[0], pg = p[1], pb = p[2];
			float m = g_ShadowMapModulate4x ? 0.25f : 1.0f;
			float c[4];
			c[0] = m * pr;
			c[1] = m * pg;
			c[2] = m * pb;
			float mx = Float_MaxSS(c[0], Float_MaxSS(c[1], c[2]));
			if (mx > (g_ShadowMapModulate4x ? 1.0f : 1.2f)) {
				float k = (g_ShadowMapModulate4x ? 1.0f : 1.2f) / mx;
				c[0] = k * c[0];
				c[1] = k * c[1];
				c[2] = k * c[2];
			}
			c[3] = 0.0f;
			uint32_t v = Color_SwapRB(Color_FromFloat(c)) & 0xffffff;
			if (!*CBmp_Pixel(mask, x, y)) {
				v = ~v;
			}
			*CBmp_Pixel(dst, (uint32_t)(dx - (int)ox) + x, (uint32_t)(dy - (int)oy) + y) = v;
		}
	}
}

byte BspLMap_Insert(BspTri *list, BspLMap *lm, int w, int h) {
	int x, y;

	if (!lm) {
		return 0;
	}
	if (w >= lm->minX && h >= lm->minY) {
		return 0;
	}
	if (!BspLMap_Find(&lm->bmp, w, h, &x, &y)) {
		lm->minX = w;
		lm->minY = h;
		return 0;
	}
	BspLMap_Copy(&lm->bmp, x, y, &list->light, &list->mask, 0, 0, w, h);
	if (!lm->lmap2) {
		lm->lmap2 = BspLMap_New();
		BspLMap_Init(lm->lmap2, g_ShadowMapTexX, g_ShadowMapTexY);
	}
	BspLMap_Copy(&lm->lmap2->bmp, x, y, &list->light2, &list->mask, 0, 0, w, h);
	if (lm->info.img) {
		uint32_t color = Color_SwapRB(g_BspColors[(list->zone + 1) & 0xf]);
		CBmp tmp;
		tmp.width = w;
		tmp.height = h;
		tmp.img = New_Array((uint32_t)(w * h), 4);
		CBmp_Fill(&tmp, color);
		CBmp_Copy(&lm->info, x, y, &tmp, 0, 0, w, h, 0xf);
		Img_Free(&tmp, 4);
	}
	list->LMap = lm;
	Img_Free(&list->light, 0xc);
	Img_Free(&list->light2, 0xc);
	float fx = (float)x, fy = (float)y;
	for (BspTri *t = list; t; t = t->next) {
		for (int i = 0; i < 3; i++) {
			t->uvl[i][0] = fx / (float)g_ShadowMapTexX + t->uvl[i][0];
			t->uvl[i][1] = fy / (float)g_ShadowMapTexY + t->uvl[i][1];
		}
	}
	return 1;
}


void CBsp_GroupMaps(CBsp *self) {
	char name1[0x104], name2[0x104];
	CScene *scn = self->model->scene;

	CHECK(scn);
	BspLMap *old = g_BspLMaps;
	if (old) {
		BspLMap_dtor(old);
		operator_delete_sized(old, sizeof(BspLMap));
		g_BspLMaps = NULL;
	}
	g_BspLMaps = BspLMap_New();
	BspLMap_Init(g_BspLMaps, g_ShadowMapTexX, g_ShadowMapTexY);
	BspLMap *cur = g_BspLMaps;
	while (self->num_nolmap) {
		CHECK(self->num_lists < BSP_MAX_LISTS);
		self->num_nolmap--;
		self->lists[self->num_lists++] = self->nolmap[self->num_nolmap];
	}
	crt_qsort(self->lists, (size_t)self->num_lists, sizeof(BspTri *),
		(int (*)(const void *, const void *))(void *)BspTri_CmpZone);
	for (int i = 0; i < self->num_lists; i++) {
		BspTri *list = self->lists[i];
		E3D_DebugPrintf("agrupar mapas:  %d / %d   mem: %d Kb  \r", i + 1, self->num_lists, g_MemStats->bytes / 1024);
		if (list->flags & BSPTRI_NOLMAP) {
			CHECK(list->LMap == NULL);
			Img_Free(&list->light, 0xc);
			Img_Free(&list->light2, 0xc);
			continue;
		}
		BspTri_Rotate(list);
		int w = list->lx, h = list->ly;
		/* another material or sector starts on the last page */
		if (i > 0) {
			const BspTri *prev = self->lists[i - 1];
			if (prev->Mat != list->Mat || prev->zone != list->zone) {
				while (cur->next) {
					cur = cur->next;
				}
			}
		}
		for (BspLMap *lm = cur; lm; lm = lm->next) {
			if (BspLMap_Insert(list, lm, w, h)) {
				break;
			}
			if (!lm->next) {
				lm->next = BspLMap_New();
				BspLMap_Init(lm->next, g_ShadowMapTexX, g_ShadowMapTexY);
			}
		}
	}
	E3D_DebugPrintf("\n");
	int k = 0;
	for (BspLMap *lm = g_BspLMaps; lm; lm = lm->next) {
		BspLMap *l2 = lm->lmap2;
		lm->id = ++k;
		l2->id = k;
		BspLMap_Save(name1, &lm->bmp, scn->name, k, ".LMAP1.tga");
		BspLMap_Save(name2, &l2->bmp, scn->name, k, ".LMAP2.tga");
		if (lm->info.img) {
			CBmp_Fill(&l2->info, Color_SwapRB(g_BspColors[1 + (k - 1) % 15]));
			BspLMap_Save(NULL, &lm->info, scn->name, k, ".LMAP1.INFO.tga");
			BspLMap_Save(NULL, &l2->info, scn->name, k, ".LMAP2.INFO.tga");
		}
		Img_Free(&lm->bmp, 4);
		Img_Free(&l2->bmp, 4);
		Img_Free(&lm->info, 4);
		Img_Free(&l2->info, 4);
		BspLMap_CreateMat(lm, name1, 1);
		BspLMap_CreateMat(l2, name2, 2);
	}
	crt_qsort(self->lists, (size_t)self->num_lists, sizeof(BspTri *),
		(int (*)(const void *, const void *))(void *)BspTri_CmpZone);
}


/* the lists of the map */

void CBsp_CreateZones(CBsp *self) {
	int n = Sector_Num() + 1;

	crt_qsort(self->lists, (size_t)self->num_lists, sizeof(BspTri *),
		(int (*)(const void *, const void *))(void *)BspTri_CmpZone);
	Table_NewB4(&self->zones, n);
	CHECK(self->zones.p);
	memset(self->zones.p, 0, (size_t)self->zones.n * sizeof(BspZone));
	E3D_DebugPrintf("listas: %d\n", self->num_lists);
	E3D_DebugPrintf("zonas: %d\n", n);
	for (int i = 0; i < self->num_lists; i++) {
		BspTri *tri = self->lists[i];
		BspZone *z = CBsp_Zone(self, tri->zone);
		BspTri *t = z->tris;
		CHECK(!tri->zone_next);
		if (!t) {
			CBsp_Zone(self, tri->zone)->tris = tri;
		} else {
			while (t->zone_next) {
				t = t->zone_next;
			}
			t->zone_next = tri;
		}
	}
}

/* the transparent lists without a diffuse map take the color of the material and no light map */
void CBsp_ColorSinLmap(CBsp *self) {
	for (int i = 0; i < self->num_lists; i++) {
		BspTri *list = self->lists[i];
		const CMaterial *m = list->Mat;
		if (!Mat_IsTransp(m) || m->map[MAP_DIFFUSE]) {
			continue;
		}
		const byte *d = m->params.diffuse;
		uint32_t c = (uint32_t)d[3] << 24 | (uint32_t)d[0] << 16 | (uint32_t)d[1] << 8 | d[2];
		if (g_DiffuseMod4x) {
			c = (c & 0xff000000) | (c >> 2 & 0x3f3f3f);
		}
		for (BspTri *t = list; t; t = t->next) {
			t->color[0] = c;
			t->color[1] = c;
			t->color[2] = c;
		}
		list->flags |= BSPTRI_NOLMAP;
	}
}

/* the lists of the area lights out of the map */
void CBsp_RemoveLights(CBsp *self) {
	int i = 0;

	while (i < self->num_lists) {
		BspTri *tri = self->lists[i];
		if (!(tri->node->flags >> 7 & 1)) {
			i++;
			continue;
		}
		CHECK(tri->radlight_ok || !g_ShadowMapRadiosity);
		CHECK(tri->node->flags >> 29 & 1);
		tri = self->lists[i];
		if (tri) {
			BspTri_dtor(tri);
			operator_delete_sized(tri, sizeof(BspTri));
			self->lists[i] = NULL;
		}
		self->lists[i] = self->lists[self->num_lists - 1];
		self->num_lists--;
	}
}

/* the areas of src averaged in boxes of dim x dim texels over w x h into dst */
void BspTri_AreasLod(CTable *dst, const CTable *src, int w, int h, int dim) {
	CTable tmp;
	int n = 0;

	Table_Free(dst);
	if (!src->n) {
		return;
	}
	tmp.p = NULL;
	tmp.n = 0;
	Table_Alloc18(&tmp, src->n);
	for (int yb = 0; yb < h; yb += dim) {
		for (int xb = 0; xb < w; xb += dim) {
			float sx = 0.0f, sy = 0.0f, sz = 0.0f, weight = 0.0f, glow = 0.0f;
			uint16_t ux = 0, uy = 0;
			int cnt = 0;
			for (int i = 0; i < src->n; i++) {
				TABLE_CHECK(src->p, i, src->n);
				const BspArea *a = &((const BspArea *)src->p)[i];
				if (a->x >= xb && a->x < dim + xb && a->y >= yb && a->y < dim + yb) {
					sx = sx + a->pos.x;
					sy = sy + a->pos.y;
					sz = sz + a->pos.z;
					weight = weight + a->weight;
					glow = glow + a->glow;
					ux += a->x;
					uy += a->y;
					cnt++;
				}
			}
			if (!cnt) {
				continue;
			}
			CHECK(cnt <= dim * dim);
			float fc = (float)cnt;
			TABLE_CHECK(tmp.p, n, tmp.n);
			BspArea *o = &((BspArea *)tmp.p)[n++];
			o->pos.x = sx / fc;
			o->pos.y = sy / fc;
			o->pos.z = sz / fc;
			o->weight = weight;
			o->x = (uint16_t)((int)ux / cnt);
			o->y = (uint16_t)((int)uy / cnt);
			o->glow = glow / fc;
		}
	}
	if (n) {
		Table_Alloc18(dst, n);
		for (int i = 0; i < n; i++) {
			TABLE_CHECK(tmp.p, i, tmp.n);
			TABLE_CHECK(dst->p, i, dst->n);
			((BspArea *)dst->p)[i] = ((const BspArea *)tmp.p)[i];
		}
	}
	if (tmp.p) {
		operator_delete_array(tmp.p);
	}
}


/* the region trees of the radiosity */

/* the region of the texels (x, y) size lx x ly with the areas in it; NULL without any */
BspRegion *BspRegion_New(int x, int y, int lx, int ly, BspRegion *parent, const CTable *areas) {
	if (lx <= 0 || ly <= 0) {
		return NULL;
	}
	BspRegion *r = operator_new(sizeof(BspRegion));
	if (r) {
		r = BspRegion_ctor(r, x, y, lx, ly, parent);
	}
	int n = 0;
	r->center = (vec3_t){ 0.0f, 0.0f, 0.0f };
	r->area = 0.0f;
	for (int i = 0; i < areas->n; i++) {
		TABLE_CHECK(areas->p, i, areas->n);
		const BspArea *a = &((const BspArea *)areas->p)[i];
		if (a->x >= x && a->x < x + lx && a->y >= y && a->y < y + ly) {
			float e = g_ShadowMapScale;
			r->area = a->weight / (e * e) + r->area;
			r->center.x = r->center.x + a->pos.x;
			r->center.y = a->pos.y + r->center.y;
			r->center.z = a->pos.z + r->center.z;
			n++;
		}
	}
	if (n) {
		float fn = (float)n;
		r->center.x = r->center.x / fn;
		r->center.y = r->center.y / fn;
		r->center.z = r->center.z / fn;
		return r;
	}
	BspRegion_dtor(r);
	operator_delete_sized(r, sizeof(BspRegion));
	return NULL;
}

void BspRegion_Split(BspRegion *r, const CTable *areas) {
	while (r) {
		uint32_t lx = r->lx;
		if (lx <= 1 && r->ly <= 1) {
			r->interior = 0;
			Debug_Point(&r->center, 0xffff00, 1.0f);
			return;
		}
		uint32_t ly = r->ly;
		int hx = (int)(lx >> 1);
		int hy = (int)(ly >> 1);
		int a = (int)lx - hx;
		int b = (int)ly - hy;
		r->child[0] = BspRegion_New(r->x, r->y, a, b, r, areas);
		r->child[1] = BspRegion_New(r->x + a, r->y, hx, b, r, areas);
		r->child[2] = BspRegion_New(r->x, r->y + b, a, hy, r, areas);
		r->child[3] = BspRegion_New(r->x + a, r->y + b, hx, hy, r, areas);
		r->interior = r->child[0] || r->child[1] || r->child[2] || r->child[3];
		BspRegion_Split(r->child[0], areas);
		BspRegion_Split(r->child[1], areas);
		BspRegion_Split(r->child[2], areas);
		r = r->child[3];
	}
}

/* the light of the tree (leaves: received times the material, also into the texels of the list of color), the
 * emitted color times the area into out */
float *BspRegion_Color(float *out, BspRegion *r, BspTri *tri, uint32_t color) {
	float t[4];

	if (!r) {
		out[2] = 0.0f;
		out[1] = 0.0f;
		out[0] = 0.0f;
		out[3] = 1.0f;
		return out;
	}
	if (!r->interior) {
		float mc[17];
		CMaterial_CalcColors(tri->Mat, mc);
		t[0] = mc[0] * r->accum[0];
		t[1] = r->accum[1] * mc[1];
		t[2] = r->accum[2] * mc[2];
		t[3] = r->accum[3] * mc[3];
		memcpy(r->accum, t, sizeof(t));
		CHECK(r->lx == 1);
		CHECK(r->ly == 1);
		for (int j = 0; j < r->ly; j++) {
			for (int i = 0; i < r->lx; i++) {
				int x = r->x + i;
				int y = r->y + j;
				if (x >= tri->rad.width || y >= tri->rad.height) {
					continue;
				}
				if (Color_SwapRB(*CBmp_Pixel(&tri->mask, (uint32_t)x, (uint32_t)y)) != color) {
					continue;
				}
				float *p = CBmpFlt2_Pixel(&tri->rad, (uint32_t)x, (uint32_t)y);
				p[0] = r->accum[0] * 0.6f + p[0];
				p[1] = r->accum[1] * 0.6f + p[1];
				p[2] = r->accum[2] * 0.6f + p[2];
			}
		}
		for (int i = 0; i < 4; i++) {
			r->color[i] = r->accum[i] * 0.4f;
		}
		for (int i = 0; i < 4; i++) {
			t[i] = r->color[i] + r->color[i];
		}
	} else {
		r->color[0] = 0.0f;
		r->color[1] = 0.0f;
		r->color[2] = 0.0f;
		r->color[3] = 1.0f;
		for (int k = 0; k < 4; k++) {
			const float *s = BspRegion_Color(t, r->child[k], tri, color);
			for (int i = 0; i < 4; i++) {
				r->color[i] = s[i] + r->color[i];
			}
		}
		CHECK(r->area > 0.0f);
		float inv = 1.0f / r->area;
		for (int i = 0; i < 4; i++) {
			t[i] = r->color[i] * inv;
		}
	}
	float area = r->area;
	memcpy(r->color, t, sizeof(t));
	r->accum[0] = 0.0f;
	r->accum[1] = 0.0f;
	r->accum[2] = 0.0f;
	r->accum[3] = 1.0f;
	for (int i = 0; i < 4; i++) {
		out[i] = r->color[i] * area;
	}
	return out;
}

/* a list taller than wide is turned for the packing (pack mode 2 and up) */
void BspTri_Rotate(BspTri *list) {
	if (g_LightmapPackMode < 2) {
		return;
	}
	int w = list->lx;
	int h = list->ly;
	if (h <= w) {
		return;
	}
	list->lx = h;
	list->ly = w;
	CBmpFlt2 t = { 0, 0, NULL };
	CBmp m;
	m.img = NULL;
	CBmpFlt2_Resample(&t, &list->light, 0xf);
	CBmpFlt2_Rotate(&list->light, &t);
	if (list->light2.img) {
		CBmpFlt2_Assign(&t, &list->light2);
		CBmpFlt2_Rotate(&list->light2, &t);
	}
	Img_Free(&m, 4);
	CBmp_Resample(&m, &list->mask, 0xf);
	CBmp_Rotate(&list->mask, &m);
	if (list->mask2.img) {
		CBmp_Assign(&m, &list->mask2);
		CBmp_Rotate(&list->mask2, &m);
	}
	float fw = (float)w;
	for (BspTri *tri = list; tri; tri = tri->next) {
		for (int i = 0; i < 3; i++) {
			float u = tri->uvl[i][0];
			tri->uvl[i][0] = tri->uvl[i][1];
			tri->uvl[i][1] = fw / (float)g_ShadowMapTexX - u;
		}
	}
	Img_Free(&t, 0xc);
	Img_Free(&m, 4);
}

/* the qsort orders of the lists: for drawing (shader, material, page, sector) and for the sectors */
int BspTri_CmpMat(const void *pa, const void *pb) {
	const BspTri *a = *(BspTri *const *)pa;
	const BspTri *b = *(BspTri *const *)pb;
	CMaterial *ma = a->Mat;
	CMaterial *mb = b->Mat;

	if (g_MapSortByShader && ma != mb) {
		int ib = CMaterial_GetId(mb, NULL);
		int d = CMaterial_GetId(ma, NULL) - ib;
		if (d) {
			return d;
		}
	}
	int r = Mat_Compare(ma, mb);
	if (r) {
		return r;
	}
	r = (int)((intptr_t)a->LMap - (intptr_t)b->LMap);
	if (r) {
		return r;
	}
	if (a->zone != b->zone) {
		return a->zone - b->zone;
	}
	return 0;
}

int BspTri_CmpZone(const void *pa, const void *pb) {
	const BspTri *a = *(BspTri *const *)pa;
	const BspTri *b = *(BspTri *const *)pb;

	if (a->zone != b->zone) {
		return a->zone - b->zone;
	}
	const CMaterial *ma = a->Mat;
	const CMaterial *mb = b->Mat;
	const BspLMap *la = a->LMap;
	const BspLMap *lb = b->LMap;
	byte ta = Mat_IsTransp(ma);
	if (ta != Mat_IsTransp(mb)) {
		return ta ? 1 : -1;
	}
	if ((la == NULL) != (lb == NULL)) {
		return (la == NULL) * 2 - 1;
	}
	int r = Mat_Compare(ma, mb);
	if (r) {
		return r;
	}
	if (la && lb) {
		return la->id - lb->id;
	}
	return (la == NULL) - (lb == NULL);
}


/* the map compiler: debug output, the TXF cache, the area sums */

#define g_BspDebugCnt GAME_GLOBAL(int, g_BspDebugCnt)
#define g_BspDebugSel GAME_GLOBAL(int, g_BspDebugSel)		/* the region drawn without links */
/* the collision cache counters of suelo.cpp: hit, miss, peak */
#define g_BspCacheStats (GAME_ARRAY(const int, g_BspCacheStats))	/* [3], the hits, the misses and g_CollisionTestsMax */

void Bsp_MatInfo(CMaterial *m) {
	uint32_t attrib = m->params.attrib;
	const char *transp = Mat_IsTransp(m) ? "Transp  " : "";
	const char *g = m->map[4] ? "G" : "";
	const char *b = m->map[3] ? "B" : "";
	const char *e = m->map[2] ? "E" : "";
	const char *o = m->map[1] ? "M" : "";
	const char *d = m->map[0] ? "D" : "";
	const char *glow = CMaterial_GetMapName(m, MAP_GLOW);
	const char *dif = CMaterial_GetMapName(m, MAP_DIFFUSE);

	Con_Verbosef("  %-20s  D:[%s]  G:[%s]  %s%s%s%s%s  %s  %04X\n", m->Name, dif, glow, d, o, e, b, g, transp, attrib);
}

/* the boxes of the regions with links (and the selected one) and their links; the regions drawn */
int BspRegion_Debug(BspRegion *r) {
	if (!r) {
		return 0;
	}
	int n = BspRegion_Debug(r->child[0]);
	n += BspRegion_Debug(r->child[1]);
	n += BspRegion_Debug(r->child[2]);
	n += BspRegion_Debug(r->child[3]);
	if (++g_BspDebugCnt != g_BspDebugSel && !n) {
		return 0;
	}
	float s = g_ShadowMapScale * g_ShadowMapRes;
	float hx = (float)r->lx / s * 0.5f;
	float hy = (float)r->ly / s * 0.5f;
	vec3_t mx = { r->center.x + hx, r->center.y + hy, r->center.z + 0.0f };
	vec3_t mn = { r->center.x - hx, r->center.y - hy, r->center.z - 0.0f };
	Debug_DrawBox(&mn, &mx, n ? 0xc0c0c0 : 0xffffff);
	for (BspLink *l = r->links; l; l = l->next) {
		Debug_DrawLine(&r->center, &l->region->center, g_BspColors[n % 15 + 1]);
	}
	Text_PrintfRel(0, 0x10, "%d:  px py: %d %d  lx ly: %d %d", n, r->x, r->y, r->lx, r->ly);
	return n + 1;
}

/* the material of a page from its file (level: the light map level of the map) */
void BspLMap_CreateMat(BspLMap *self, const char *name, byte level) {
	CMaterial m;

	CMaterial_ctor(&m);
	String_Set(&m.Name, Path_FileName(name));
	CMaterial_SetMap(&m, MAP_DIFFUSE, name);
	m.params.lighting = 0;
	m.map[MAP_DIFFUSE]->lightmap = level;
	m.params.zWrite = 0;
	if (!g_ShadowMapMultitext) {
		m.params.dstBlend = 3;
		m.params.transp = 1;
		m.params.srcBlend = (g_Gamma2x == 0) * 8 + 1;
	}
	CHECK(!self->Mat);
	self->Mat = Mat_New(&m, 1);
	CMaterial_dtor(&m);
}

/* bmp saved as "<ruta>.%04d<ext>" (overwriting); the file name into copy */
void BspLMap_Save(char *copy, CBmp *bmp, const char *path, int n, const char *ext) {
	char buf[0x104];

	Bprintf(buf, sizeof(buf), "&u&d&n.%04d%s", n, ext);
	Path_Format(buf, path, buf);
	CBmp_Save(bmp, buf, IMG_OVERWRITE);
	if (copy) {
		memcpy(copy, buf, strlen(buf) + 1);
	}
}

/* the glow of the texel of each area (rad2) and the coarser levels of the areas */
void CBsp_AreasGlow(CBsp *self) {
	for (int i = 0; i < self->num_lists; i++) {
		BspTri *list = self->lists[i];
		for (BspTri *t = list; t; t = t->next) {
			CTable *a0 = &t->areas[0];
			for (int k = 0; k < a0->n; k++) {
				TABLE_CHECK(a0->p, k, a0->n);
				BspArea *a = &((BspArea *)a0->p)[k];
				const float *p = CBmpFlt2_Pixel(&list->rad2, a->x, a->y);
				float r = p[0], g = p[1], b = p[2];
				a->glow = (g * 0.05f + r * 0.05f + b * 0.05f) / 3.0f;
			}
			BspTri_AreasLod(&t->areas[1], a0, list->mask.width, list->mask.height, 2);
			BspTri_AreasLod(&t->areas[2], a0, list->mask.width, list->mask.height, 4);
			BspTri_AreasLod(&t->areas[3], a0, list->mask.width, list->mask.height, 8);
		}
	}
}

/* the sum and the largest channel of the glow at the areas, by triangle and by list */
void CBsp_AreasMax(CBsp *self) {
	for (int i = 0; i < self->num_lists; i++) {
		BspTri *list = self->lists[i];
		list->listGlowSum[0] = 0.0f;
		list->listGlowSum[1] = 0.0f;
		list->listGlowSum[2] = 0.0f;
		list->listGlowSum[3] = 1.0f;
		list->listGlowAreas = 0;
		for (BspTri *t = list; t; t = t->next) {
			t->glowAreas = 0;
			t->glowSum[0] = 0.0f;
			t->glowSum[1] = 0.0f;
			t->glowSum[2] = 0.0f;
			t->glowSum[3] = 1.0f;
			t->glowMax = 0.0f;
			for (int k = 0; k < t->areas[0].n; k++) {
				TABLE_CHECK(t->areas[0].p, k, t->areas[0].n);
				const BspArea *a = &((const BspArea *)t->areas[0].p)[k];
				const float *p = CBmpFlt2_Pixel(&list->rad2, a->x, a->y);
				float r = p[0], g = p[1], b = p[2];
				t->glowSum[0] = t->glowSum[0] + r;
				t->glowSum[1] = t->glowSum[1] + g;
				float m = Float_MaxSS(b, Float_MaxSS(g, r));
				t->glowSum[2] = t->glowSum[2] + b;
				t->glowSum[3] = t->glowSum[3] + 1.0f;
				t->glowAreas++;
				t->glowMax = Float_MaxSS(t->glowMax, m);
			}
			list->listGlowSum[0] = list->listGlowSum[0] + t->glowSum[0];
			list->listGlowSum[1] = t->glowSum[1] + list->listGlowSum[1];
			list->listGlowSum[2] = t->glowSum[2] + list->listGlowSum[2];
			list->listGlowSum[3] = t->glowSum[3] + list->listGlowSum[3];
			list->listGlowAreas += t->glowAreas;
		}
	}
}

/* n passes of the light through the region links: gathered into the trees, then into the light maps */
void CBsp_IllumRegions(CBsp *self, int n) {
	float out[4];

	for (int pass = 1; pass <= n; pass++) {
		E3D_DebugPrintf("iluminacion_regiones (%d):\n", pass);
		int nl = self->num_lists;
		for (int i = 0; i < nl; nl = self->num_lists) {
			BspTri *list = self->lists[i];
			E3D_DebugPrintf("region: %d / %d   \r", ++i, nl);
			if (list) {
				float add[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
				for (BspTri *t = list; t; t = t->next) {
					BspRegion_Accumulate(t->region, add);
				}
			}
		}
		nl = self->num_lists;
		for (int i = 0; i < nl; nl = self->num_lists) {
			BspTri *list = self->lists[i];
			E3D_DebugPrintf("region: %d / %d   \r", ++i, nl);
			for (BspTri *t = list; t; t = t->next) {
				BspRegion_Color(out, t->region, list, (uint32_t)(uintptr_t)t);
			}
		}
	}
}

static inline void Vec3_Accum(vec3_t *s, const vec3_t *p) {
	s->x = s->x + p->x;
	s->y = s->y + p->y;
	s->z = p->z + s->z;
}

/* a triangle of list other than tri has p */
static byte BspTri_OtherHas(const BspTri *tri, const BspTri *list, const vec3_t *p) {
	for (const BspTri *o = list; o; o = o->next) {
		if (o != tri && Ground_PointInTri(p, &o->v[0], &o->v[1], &o->v[2])) {
			return 1;
		}
	}
	return 0;
}

/* the world point of the texel x, y of tri: without weight the center (else the average of the corners inside tri),
 * with weight the average of the 7 x 7 samples inside the list and their share in weight */
vec3_t *BspTri_Sample(vec3_t *out, int x, int y, int axis, float scale, const BspTri *tri, const BspTri *list,
	float *weight) {
	vec3_t p;

	if (!weight) {
		vec3_t c;
		Bsp_TexelToWorld(&c, (float)x + 0.5f, (float)y + 0.5f, axis, scale, &tri->plane);
		if (Ground_PointInTri(&c, &tri->v[0], &tri->v[1], &tri->v[2]) || BspTri_OtherHas(tri, list, &c)) {
			*out = c;
			return out;
		}
		vec3_t q[4];
		float fx = (float)x, fy = (float)y;
		float x0 = fx + 0.0f, y0 = fy + 0.0f;
		Bsp_TexelToWorld(&q[0], x0, y0, axis, scale, &tri->plane);
		float x1 = fx + 1.0f;
		Bsp_TexelToWorld(&q[1], x1, y0, axis, scale, &tri->plane);
		float y1 = fy + 1.0f;
		Bsp_TexelToWorld(&q[2], x0, y1, axis, scale, &tri->plane);
		Bsp_TexelToWorld(&q[3], x1, y1, axis, scale, &tri->plane);
		vec3_t s = { 0.0f, 0.0f, 0.0f };
		int n = 0;
		for (int i = 0; i < 4; i++) {
			if (Ground_PointInTri(&q[i], &tri->v[0], &tri->v[1], &tri->v[2])) {
				s.x = q[i].x + s.x;
				s.y = q[i].y + s.y;
				s.z = q[i].z + s.z;
				n++;
			}
		}
		if (!n) {
			*out = c;
			return out;
		}
		float fn = (float)n;
		*out = (vec3_t){ s.x / fn, s.y / fn, s.z / fn };
		return out;
	}
	vec3_t s = { 0.0f, 0.0f, 0.0f };
	int n = 0;
	for (int i = 0; i <= 6; i++) {
		float u = (float)i / 6.0f + (float)x;
		for (int j = 0; j <= 6; j++) {
			Bsp_TexelToWorld(&p, u, (float)j / 6.0f + (float)y, axis, scale, &tri->plane);
			if (Ground_PointInTri(&p, &tri->v[0], &tri->v[1], &tri->v[2]) || BspTri_OtherHas(tri, list, &p)) {
				Vec3_Accum(&s, &p);
				n++;
			}
		}
	}
	if (!n) {
		*weight = 0.0f;
		Bsp_TexelToWorld(out, (float)x + 0.5f, (float)y + 0.5f, axis, scale, &tri->plane);
		return out;
	}
	float fn = (float)n;
	*weight = fn / 49.0f;
	out->x = s.x / fn;
	out->y = s.y / fn;
	out->z = s.z / fn;
	return out;
}

/* the TXF of list i (nl: a line feed after the messages): 1 when it is to be computed here, 0 when it exists or
 * another computer computes it (locked), def without the cache. ESC with SHIFT asks to cancel the compilation */
byte BspTxf_Check(BspTxf *self, int i, byte def, byte nl) {
	char buf[0x104];
	const char *lf = nl ? "\n" : "";

	if (exe_GetAsyncKeyState(0x1b) < 0 && exe_GetAsyncKeyState(0x10) < 0 &&
		exe_MessageBoxA(NULL, "Cancel map compilation?", "Message", 0x2034) == 6) {
		crt_exit(-1);
	}
	if (!self->on) {
		return def;
	}
	Bprintf(self->name, sizeof(self->name), "%s.%05d.TXF", self->base, i + 1);
	if (File_Exists(self->name)) {
		self->skipped++;
		E3D_DebugPrintf("Skipping...   %s", lf);
		return 0;
	}
	Path_Format(buf, self->name, "&n");
	const char *s = strrchr(buf, '.') ? strrchr(buf, '.') + 1 : buf;
	E3D_DebugPrintf("%s   ", s);
	CHECK(!self->FileTXF);
	self->FileTXF = crt_fsopen(self->name, "wb", 0x10);
	if (!self->FileTXF) {
		self->locked++;
		E3D_DebugPrintf("Already locked...   %s", lf);
		return 0;
	}
	crt_fclose(self->FileTXF);
	self->FileTXF = NULL;
	return 1;
}

/* the computed map into the TXF; a failed write is computed again (up to 100 in all) */
void BspTxf_Save(BspTxf *self, CBmpFlt2 *bmp) {
	if (!self->on) {
		return;
	}
	if (self->FileTXF) {
		crt_fclose(self->FileTXF);
	}
	self->FileTXF = NULL;
	if (CBmpFlt2_Save(bmp, self->name, TXF_OVERWRITE, self->team, sizeof(self->team))) {
		/* differs from the original on purpose (no sleeps): the original tried again 10 seconds later, up to 100
		 * times for each map; here BspTxf_Load finds the TXF missing or unreadable and the rebuild computes it
		 * again */
		CHECK(self->writeRetries < 100);
		self->writeRetries++;
		E3D_LogPrint(g_FileLog, "Retry %d of %d", self->writeRetries, 100);
		return;
	}
	self->written++;
}

/* the light maps (which 1 light, 2 light2, 3 rad, 4 rad2) of n lists from the TXF cache; a missing one makes the map
 * be computed again (rebuild), a bad one is magenta */
void BspTxf_Load(BspTxf *self, BspTri **lists, int n, int which) {
	static const float magenta[3] = { 1.0f, 0.0f, 1.0f };

	if (!self->on) {
		return;
	}
	E3D_DebugPrintf("\n");
	for (int i = 0, k = 1; i < n; i++, k++) {
		BspTri *tri = lists[i];
		CBmpFlt2 *bmp = NULL;
		switch (which) {
		case 1:
			bmp = &tri->light;
			break;
		case 2:
			bmp = &tri->light2;
			break;
		case 3:
			bmp = &tri->rad;
			break;
		case 4:
			bmp = &tri->rad2;
			break;
		default:
			CHECK(0);
		}
		Bprintf(self->name, sizeof(self->name), "%s.%05d.TXF", self->base, k);
		char info[0x20];
		memset(info, 0, sizeof(info));
		if (!g_TxfMode && !File_Exists(self->name)) {
			E3D_DebugPrintf("TXF not found: %s   rebuilding...   \n", Path_FileName(self->name));
			self->rebuild = 1;
			break;
		}
		if (!CBmpFlt2_Load(bmp, self->name, 0, info, sizeof(info))) {
			if (bmp->width != tri->lx || bmp->height != tri->ly) {
				E3D_DebugPrintf("Bad TXF: %s  [%s]   \n", Path_FileName(self->name), info);
				E3D_LogBlue(g_FileLog, "Bad TXF: %s  [%s]  size: %dx%d  required: %dx%d", Path_FileName(self->name),
					info, bmp->width, bmp->height, tri->lx, tri->ly);
				CHECK(g_TxfMode);
				CBmpFlt2_CreateFill(bmp, tri->lx, tri->ly, magenta);
				if (g_TxfMode == 2) {
					crt_remove(self->name);
				}
			}
			E3D_DebugPrintf("Reloading TXF:  %d / %d  %s  [%s]   \r", k, n, Path_FileName(self->name), info);
			continue;
		}
		CBmpFlt2_CreateFill(bmp, tri->lx, tri->ly, magenta);
		if (g_TxfMode) {
			E3D_DebugPrintf("TXF not found: %s   \n", Path_FileName(self->name));
			E3D_LogBlue(g_FileLog, "TXF not found: %s", Path_FileName(self->name));
			if (g_TxfMode == 2 && File_Exists(self->name)) {
				crt_remove(self->name);
			}
			continue;
		}
		int32_t t = crt_time32(NULL);
		if (t - File_Time(self->name) > 0x708) {
			E3D_DebugPrintf("Deleting file '%s'...   \n", Path_FileName(self->name));
			crt_remove(self->name);
			self->deleted++;
			self->rebuild = 1;
			continue;
		}
		/* differs from the original on purpose (no sleeps): the original waited 30 seconds and read it again (another
		 * computer still computes it, or ESC with SHIFT deleted it), here it is computed again like an old one */
		self->readRetries++;
		E3D_DebugPrintf("Deleting file '%s'...   \n", Path_FileName(self->name));
		crt_remove(self->name);
		self->rebuild = 1;
	}
	BspTxf_Info(self);
}

/* the end of the compilation: the time and the collision cache counters; yes exits the game */
void Bsp_EndMessage(CBsp *self, int secs) {
	char buf[0x400];

	Bprintf(buf, sizeof(buf),
		"Map compilation finished in %dh %02dm %02ds\n\npath: %s\n\nhit: %d  miss: %d  peak: %d\n\nExit now?",
		secs / 3600, secs / 60 % 60, secs % 60, self->name, g_BspCacheStats[0], g_BspCacheStats[1],
		g_BspCacheStats[2]);
	if (g_TxfMode) {
		Bprintf(buf, sizeof(buf), "TFX loading finished (%d)\n\n%s\n\npath: %s\n\nExit now?", g_TxfMode,
			"written .EMI .TGA and .INI files", self->name);
	}
	if (exe_MessageBoxA(NULL, buf, "Message", 0x2024) == 6) {
		crt_fflush(NULL);
		crt_exit_now(-1);
	}
}


/* the map: made from the floor of the world */

#define g_LmapSaveIni GAME_GLOBAL_IN(int, g_BspLodDist, 24)	/* the settings back into the .ini of the map */
#define g_ShadowMapDlgCalc ((void *)Bsp_DlgProc)	/* Bsp_DlgProc */

typedef int(__stdcall *isdlgbuttonchecked_t)(void *hwnd, int id);
typedef int(__stdcall *checkdlgbutton_t)(void *hwnd, int id, unsigned check);
typedef int(__stdcall *enablewindow_t)(void *hwnd, int enable);
#define exe_IsDlgButtonChecked GAME_IMPORT(isdlgbuttonchecked_t, IsDlgButtonChecked, 8)
#define exe_CheckDlgButton GAME_IMPORT(checkdlgbutton_t, CheckDlgButton, 12)
#define exe_EnableWindow GAME_IMPORT(enablewindow_t, EnableWindow, 8)

/* the obj3D of a node as its class (the inline cast of objeto3D.h, NULL with a failed CHECK) */
static void *Obj3D_As(CNode3D *n, uint32_t class) {
	IObject3D *o = n->obj3D;

	if (o->class != class) {
		CheckFailed("devNULL", "objeto3D.h", __TIMESTAMP__, __LINE__);
		return NULL;
	}
	return o;
}

CBsp *CBsp_ctor(CBsp *self, const char *name, const vec3_t *scale) {
	char buf[0x104];

	self->name = NULL;
	self->zones.p = NULL;
	self->zones.n = 0;
	String_Set(&self->name, name);
	self->model = NULL;
	memset(self->lists, 0, sizeof(self->lists));
	self->num_lists = 0;
	memset(self->nolmap, 0, sizeof(self->nolmap));
	self->num_nolmap = 0;
	self->nodes = NULL;
	self->lights = NULL;
	self->matFX = NULL;
	self->matWire = NULL;
	int n = Sector_Num();
	g_BspSectorSeen = 0;
	g_PortalsOn = n > 0;
	g_BspEyePrev = (vec3_t){ 0.0f, 0.0f, 0.0f };
	if (g_LmapInfo) {
		Tex_AddExtension("<U><D><N>.INFO<E>");
	}
	if (!g_ShadowMapCalculate) {
		if (CBsp_ReadEMI(self)) {
			Con_FatalErrorf("No hay iluminaci\xf3n precalculada para:\n%s", self->name);
		}
	} else {
		self->model = Create_Model3(name, NULL, scale, 4);
		self->model->dynLight = 0;
		self->model->alwaysVisible = 1;
		CBsp_Calculate(self);
	}
	CBsp_CreateZones(self);
	CBsp_Prepare(self);
	if (!self->model) {
		if (World_Get()->GroundScale->fromAmc) {
			Path_Format(buf, name, "&U&D&N_2._SM3_");
			self->model = Create_Model3(buf, NULL, scale, 0x14);
		} else if (!self->model) {
			self->model = Create_Model3(name, NULL, scale, 4);
		}
	}
	self->model->dynLight = 0;
	self->model->alwaysVisible = 1;
	return self;
}

/* the node class of the log lines */
static const char *Bsp_NodeClass(CNode3D *n) {
	return Obj3D_ClassName(CNode3D_Class(n));
}

/* the nodes of a scene into the file log; done: the ones out of the map (both flags 0x20008) too */
static void Bsp_LogNodes(CScene *scn, const char *fmt, byte done) {
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		uint32_t f = n->flags;
		if ((f & 0x20008) == 0x20008) {
			if (done) {
				E3D_LogInfo(g_FileLog, "Map node: %s  [c:%s]", n->name, Bsp_NodeClass(n));
			}
			continue;
		}
		const char *cls = Bsp_NodeClass(n);
		E3D_LogWhite(g_FileLog, fmt, n->name, f >> 17 & 1 ? "" : "  [TRANS]", f >> 3 & 1 ? "" : "  [RENDER]", cls);
	}
}

void Bsp_Create(void) {
	char buf[0x104];

	if (!g_ShadowMapUse) {
		return;
	}
	if (g_ShadowMapDlg) {
		void *hwnd = g_Engine->hWnd;
		void *inst = (void *)exe_GetWindowLongA(hwnd, -6);
		if (!exe_DialogBoxParamA(inst, (const char *)0xa2, hwnd, g_ShadowMapDlgCalc, 0)) {
			g_ShadowMapUse = 0;
			return;
		}
	}
	if (!g_ShadowMapUse) {
		return;
	}
	CWorld *world = World_Get();
	CHECK(world->Ground);
	CBsp *bsp = operator_new(sizeof(CBsp));
	g_Bsp = bsp ? CBsp_ctor(bsp, world->GroundScale->name, &world->Ground->scaleBase) : NULL;
	CGround_FreeNodes(world->ObjGround);
	/* the world keeps no lights and no meshes: they are in the map now */
	for (CNode3D *n = world->GroundScale->Node3D; n; n = n->next) {
		if (CNode3D_Class(n) == CLASS_OBJ3D_LIGHT) {
			CLight *light = Obj3D_As(n, CLASS_OBJ3D_LIGHT);
			if (n->flags & 0x6000000) {
				continue;
			}
			CHECK(!light->active);
			n->flags |= 0x20008;
			Bsp_MarkNode(n, 1);
			E3D_LogWhite(g_FileLog, "World: Light object DELETED: %s", n->name);
			continue;
		}
		if (CNode3D_Class(n) != CLASS_OBJ3D_GROUND && CNode3D_Class(n) != CLASS_OBJ3D_PORTAL &&
			CNode3D_Class(n) != CLASS_OBJ3D_CAMERA) {
			Bsp_MarkNode(n, 1);
			n->flags |= 0x20008;
		}
		Bsp_MarkNode(n, 0);
		if (CNode3D_Class(n) == CLASS_OBJ3D_PORTAL) {
			n->flags |= 0x20008;
		}
	}
	CScene_CompactMats(world->GroundScale);
	CScene_DeleteEmpty(world->GroundScale, 0, 0, 1);
	/* the map keeps only its meshes */
	for (CNode3D *n = g_Bsp->model->scene->Node3D; n; n = n->next) {
		if (World_Get()->GroundScale->fromAmc) {
			break;
		}
		CHECK(CNode3D_Class(n) != CLASS_OBJ3D_MESHD3D);
		if (CNode3D_Class(n) == CLASS_OBJ3D_TRIMESH) {
			uint32_t f = n->flags;
			if (f >> 7 & 1) {
				if (f >> 13 & 1 && !(f >> 29 & 1)) {
					Con_Warnf("WARN: Only area lights can be both NO_RENDER and NO_COLLIDE: %s\n", n->name);
				}
				if (n->flags >> 29 & 1 && !(n->flags >> 13 & 1)) {
					Con_Warnf("WARN: Hidden area light without NO_COLLIDE: %s\n", n->name);
				}
			}
			if (n->flags >> 25 & 1 && !(n->flags >> 7 & 1)) {
				CNode3D_BuildD3D(n, 2);
				CScene *scn = World_Get()->GroundScale;
				CMeshD3D_CalcIllum(Obj3D_As(n, CLASS_OBJ3D_MESHD3D), n, scn);
				Con_Warnf("MAP NODE: NODO_RT_LIGHTING: %s\n", n->name);
			} else {
				Bsp_MarkNode(n, 1);
				n->flags |= 8;
			}
		}
		if (CNode3D_Class(n) == CLASS_OBJ3D_LIGHT || CNode3D_Class(n) == CLASS_OBJ3D_CAMERA ||
			CNode3D_Class(n) == CLASS_OBJ3D_PORTAL || !CNode3D_Class(n)) {
			Bsp_MarkNode(n, 1);
			n->flags |= 8;
		}
	}
	CScene_CompactMats(g_Bsp->model->scene);
	CScene_DeleteEmpty(g_Bsp->model->scene, 0, 0, 1);
	Bsp_LogNodes(world->GroundScale, "World node: %s%s%s  [c:%s]", 0);
	Bsp_LogNodes(g_Bsp->model->scene, "Map node: %s%s%s  [c:%s]", 1);
	CScene *mscn = g_Bsp->model->scene;
	E3D_LogInfo(g_FileLog, "Total: %d world nodes, %d map nodes", CScene_NumNodes(world->GroundScale),
		CScene_NumNodes(mscn));
	if (!World_Get()->GroundScale->fromAmc) {
		Path_Format(buf, g_Bsp->name, "&U&D&N_2.SM3");
		SM3_Save(buf, g_Bsp->model->scene);
	}
}

/* the switches of the map compiler dialog */
void Bsp_DlgEnable(void *hwnd) {
	static const int ids[] = { 0x412, 0x413, 0x40f, 0x410, 0x411, 0x414, 0x41f, 0x41e };
	int on = exe_IsDlgButtonChecked(hwnd, 0x409);

	for (int i = 0; i < 8; i++) {
		exe_EnableWindow(exe_GetDlgItem(hwnd, ids[i]), on);
	}
}

intptr_t __stdcall Bsp_DlgProc(void *hwnd, uint32_t msg, uintptr_t wparam, intptr_t lparam) {
	/* the checkboxes of the dialog and their switches */
	const struct {
		int id;
		int *var;
	} checks[] = {
		{ 0x40a, &GAME_GLOBAL(int, g_ShadowMapUse) },
		{ 0x409, &GAME_GLOBAL(int, g_ShadowMapCalculate) },
		{ 0x40d, &GAME_GLOBAL(int, g_LmapOption58) },
		{ 0x40e, &GAME_GLOBAL(int, g_LmapInfo) },
		{ 0x411, &GAME_GLOBAL_IN(int, g_BspLodDist, 24) },
		{ 0x414, &GAME_GLOBAL(int, g_ShadowMapSoft) },
		{ 0x417, &GAME_GLOBAL(int, g_ShadowMapRadiosity) },
		{ 0x41a, &GAME_GLOBAL(int, g_ShadowMapIllumGlobal) },
		{ 0x418, &GAME_GLOBAL(int, g_ShadowMapNoLightAmb) },
		{ 0x41b, &GAME_GLOBAL(int, g_ShadowMapModulate4x) },
		{ 0x420, &GAME_GLOBAL(int, g_CalcLmap2) },
		{ 0x419, &GAME_GLOBAL(int, g_ShadowMapUseTxf) },
	};
	char buf[0x100];

	(void)lparam;
	switch (msg) {
	case 0x10:	/* WM_CLOSE */
		break;
	case 0x110:	/* WM_INITDIALOG */
		Bprintf(buf, sizeof(buf),
			" brillo = %.2f\n gamma = %.2f\n texturas = %d x %d\n rebotes = %d\n coef. rad = %.2f\n",
			(double)GAME_GLOBAL(float, g_ShadowMapBrightness), (double)GAME_GLOBAL(float, g_ShadowMapGamma), g_ShadowMapTexX,
			g_ShadowMapTexY, GAME_GLOBAL(int, g_ShadowMapBounces), (double)GAME_GLOBAL(float, g_ShadowMapCoefRad));
		exe_SetDlgItemTextA(hwnd, 0x40b, buf);
		for (int i = 0; i < 12; i++) {
			exe_CheckDlgButton(hwnd, checks[i].id, (unsigned)*checks[i].var);
		}
		Dlg_SetFloat(hwnd, 0x410, g_ShadowMapScale, 3);
		Dlg_SetFloat(hwnd, 0x40f, g_ShadowMapRes, 3);
		Dlg_SetFloat(hwnd, 0x41e, GAME_GLOBAL(float, g_ShadowMapGamma), 2);
		Bsp_DlgEnable(hwnd);
		return 1;
	case 0x111:	/* WM_COMMAND */
		if (wparam == 1) {
			for (int i = 0; i < 12; i++) {
				*checks[i].var = exe_IsDlgButtonChecked(hwnd, checks[i].id);
			}
			if (Dlg_GetFloat(hwnd, 0x410, &g_ShadowMapScale) && Dlg_GetFloat(hwnd, 0x40f, &g_ShadowMapRes) &&
				Dlg_GetFloat(hwnd, 0x41e, &GAME_GLOBAL(float, g_ShadowMapGamma)) && (double)g_ShadowMapScale > 0.001 &&
				(double)g_ShadowMapRes > 0.001) {
				exe_EndDialog(hwnd, 1);
				return 1;
			}
			Err_Printf(0, "N\xfamero no v\xe1lido");
			return 1;
		}
		if (wparam == 2) {
			break;
		}
		Bsp_DlgEnable(hwnd);
		return 0;
	default:
		return 0;
	}
	exe_EndDialog(hwnd, 0);
	return 1;
}


/* the radiosity: the areas and region trees of the lists, the links between regions */

#define g_LightmapDebugTex GAME_GLOBAL_IN(int, GVar_LightmapDebugTex, 16)	/* gvar */
#define g_BspLinks GAME_GLOBAL(int, g_BspNumLinks)		/* the region links (radiosity code) */

/* exe functions of other files with their prototypes there */
double crt_ceil(double x);

/* brightness and gamma on the light maps, then the vertex colors from them */
void CBsp_AdjustGamma(CBsp *self) {
	int n = self->num_lists;

	for (int i = 0; i < n; n = self->num_lists) {
		BspTri *list = self->lists[i];
		E3D_DebugPrintf("ajustar gamma:  %d / %d  \r", ++i, n);
		if (list->light2.img) {
			CBmpFlt2_Blend(&list->light2, 0, 0, &list->light, 0, 0, list->light2.width, list->light2.height, 0x100, -0x100);
			CBmpFlt2_Scale(&list->light2, GAME_GLOBAL(float, g_ShadowMapBrightness));
			CBmpFlt2_Gamma(&list->light2, GAME_GLOBAL(float, g_ShadowMapGamma), 1.0f);
		}
		CBmpFlt2_Scale(&list->light, GAME_GLOBAL(float, g_ShadowMapBrightness));
		CBmpFlt2_Gamma(&list->light, GAME_GLOBAL(float, g_ShadowMapGamma), 1.0f);
		if (g_LightmapDebugTex) {
			Img_Free(&list->light2, 0xc);
			CBmpFlt2_Resample(&list->light2, &list->light, 0xf);
			for (int k = 0; k < list->light2.height * list->light2.width; k++) {
				float c[3] = { 0.0f, 0.0f, 0.0f };
				if (list->lightA.img) {
					const float *p = (const float *)list->lightA.img + 4 * k;
					c[0] = p[0] * 4.0f;
					c[1] = p[1] * 4.0f;
					c[2] = p[2] * 4.0f;
				}
				memcpy((float *)list->light2.img + 3 * k, c, sizeof(c));
			}
			CBmpFlt2_Gamma(&list->light2, 2.0f, 1.0f);
		}
		for (BspTri *t = list; t; t = t->next) {
			for (int v = 0; v < 3; v++) {
				float c[4];
				int y = (int)((float)g_ShadowMapTexY * t->uvl[v][1]);
				BspTri_Average(c, list, (int)((float)g_ShadowMapTexX * t->uvl[v][0]), y);
				float k = g_DiffuseMod4x ? 0.25f : 1.0f;
				c[2] = c[2] * k;
				c[0] = k * c[0];
				c[1] = c[1] * k;
				c[3] = (float)(int)(t->color[v] >> 24) / 255.0f;
				t->color[v] = Color_FromFloat(c);
			}
		}
	}
	E3D_DebugPrintf("\n");
}

static inline byte BspRegion_Children(const BspRegion *r) {
	return r->child[0] || r->child[1] || r->child[2] || r->child[3];
}

/* the light between the regions a and b with the form factors over threshold: linked when the floor does not block
 * them, else the larger one split into its children; 1 when linked */
int BspRegion_Link(CBsp *self, BspRegion *a, BspRegion *b, int link, float threshold) {
	if (!a || !b) {
		return 0;
	}
	CHECK(!link);
	if (!b->parent && !b->interior) {
		return 0;
	}
	if (!a->parent && !a->interior) {
		return 0;
	}
	if (a->color[1] + a->color[0] + a->color[2] == 0.0f && b->color[1] + b->color[0] + b->color[2] == 0.0f) {
		return 0;
	}
	float ffa = BspRegion_FormFactor(a, b);
	float ffb = BspRegion_FormFactor(b, a);
	if (link && threshold > ffa && threshold > ffb) {
		return 1;
	}
	CHECK(a->interior == BspRegion_Children(a));
	CHECK(b->interior == BspRegion_Children(b));
	if (ffa > threshold && ffa >= ffb && b->interior) {
		for (int k = 0; k < 4; k++) {
			BspRegion_Link(self, a, b->child[k], 0, threshold);
		}
		return 0;
	}
	float d = a->center.y * b->plane.n.y + a->center.x * b->plane.n.x + a->center.z * b->plane.n.z + b->plane.d;
	float v = ffb;
	if (a->interior && a->area > d * d) {
		v = threshold + threshold;
	}
	if (v > threshold && a->interior) {
		for (int k = 0; k < 4; k++) {
			BspRegion_Link(self, a->child[k], b, 0, threshold);
		}
		return 0;
	}
	CGround *ground = World_Get()->ObjGround;
	CHECK(ground);
	vec3_t dir = { a->center.x - b->center.x, a->center.y - b->center.y, a->center.z - b->center.z };
	vec3_t n;
	Vec3_Normalize(&n, &dir);
	float k = 0.3f / g_ShadowMapScale;
	vec3_t p1 = { b->center.x + n.x * k, b->center.y + n.y * k, b->center.z + n.z * k };
	vec3_t p0 = { a->center.x - n.x * k, a->center.y - n.y * k, a->center.z - n.z * k };
	if (CGround_Trace(ground, &p0, &p1, NULL, 0.0f, NULL)) {
		return 0;
	}
	for (int i = 0; i < 4; i++) {
		a->accum[i] = a->accum[i] + b->color[i] * ffa;
	}
	for (int i = 0; i < 4; i++) {
		b->accum[i] = b->accum[i] + a->color[i] * v;
	}
	return 1;
}

/* the sample points of the texels of each triangle of the lists and their levels of detail */
void CBsp_InitAreas(CBsp *self) {
	CTable tmp;

	E3D_DebugPrintf("rad_inic_areas()\n");
	for (int i = 0; i < 4; i++) {
		g_BspAreasTotal[i] = 0;
	}
	tmp.p = NULL;
	tmp.n = 0;
	Table_Alloc18(&tmp, g_ShadowMapTexY * g_ShadowMapTexX);
	int nl = self->num_lists;
	for (int i = 0; i < nl; nl = self->num_lists) {
		BspTri *list = self->lists[i];
		E3D_DebugPrintf("%d / %d   \r", ++i, nl);
		int x0 = list->ext[0];
		int y0 = list->ext[1];
		list->big = 0;
		list->numAreas = 0;
		for (BspTri *t = list; t; t = t->next) {
			int n = 0;
			if (!(__builtin_fabs((double)(list->plane.n.x - t->plane.n.x)) < BSP_EPS_NORMAL &&
				__builtin_fabs((double)(list->plane.n.y - t->plane.n.y)) < BSP_EPS_NORMAL &&
				__builtin_fabs((double)(list->plane.n.z - t->plane.n.z)) < BSP_EPS_NORMAL)) {
				list->big = 1;
			}
			BspTri_CalcCenter(t);
			for (int y = 0; y < list->mask.height; y++) {
				for (int x = 0; x < list->mask.width; x++) {
					if (Color_SwapRB(*CBmp_Pixel(&list->mask, (uint32_t)x, (uint32_t)y)) != (uint32_t)(uintptr_t)t) {
						continue;
					}
					vec3_t p;
					float weight = 1.0f;
					BspTri_Sample(&p, x + x0, y + y0, list->axis, g_ShadowMapScale * g_ShadowMapRes, t, list,
						&weight);
					BspArea *a = &((BspArea *)tmp.p)[n];
					TABLE_CHECK(tmp.p, n, tmp.n);
					a->pos = p;
					TABLE_CHECK(tmp.p, n, tmp.n);
					a->weight = weight / (g_ShadowMapRes * g_ShadowMapRes);
					TABLE_CHECK(tmp.p, n, tmp.n);
					a->x = (uint16_t)x;
					TABLE_CHECK(tmp.p, n, tmp.n);
					a->y = (uint16_t)y;
					TABLE_CHECK(tmp.p, n, tmp.n);
					a->glow = 0.0f;
					n++;
				}
			}
			if (n) {
				CHECK(!t->areas[0].p && n > 0);
				t->areas[0].n = n;
				t->areas[0].p = New_Array((uint32_t)n, sizeof(BspArea));
				CHECK(t->areas[0].p);
				for (int k = 0; k < n; k++) {
					TABLE_CHECK(tmp.p, k, tmp.n);
					TABLE_CHECK(t->areas[0].p, k, t->areas[0].n);
					((BspArea *)t->areas[0].p)[k] = ((const BspArea *)tmp.p)[k];
				}
			}
			BspTri_AreasLod(&t->areas[1], &t->areas[0], list->mask.width, list->mask.height, 2);
			BspTri_AreasLod(&t->areas[2], &t->areas[0], list->mask.width, list->mask.height, 4);
			BspTri_AreasLod(&t->areas[3], &t->areas[0], list->mask.width, list->mask.height, 8);
			g_BspAreasTotal[0] += n;
			g_BspAreasTotal[1] += t->areas[1].n;
			g_BspAreasTotal[2] += t->areas[2].n;
			g_BspAreasTotal[3] += t->areas[3].n;
			list->numAreas += n;
		}
	}
	if (tmp.p) {
		operator_delete_array(tmp.p);
		tmp.p = NULL;
		tmp.n = 0;
	}
	E3D_DebugPrintf("total areas: %d %d %d %d   \n", g_BspAreasTotal[0], g_BspAreasTotal[1], g_BspAreasTotal[2],
		g_BspAreasTotal[3]);
	E3D_DebugPrintf("memoria usada: %-5d Kb\n", g_MemStats->bytes / 1024);
}


/* the glow of the lists and the shadows of the area lights */

#define g_LightmapGlowMult GAME_GLOBAL_IN(float, GVar_LightmapGlowMult, 16)	/* gvar */

/* the glow map of the material sampled into lightA, squared */
void BspTri_CalcGlow(BspTri *list) {
	static const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	CBmp tex;
	float tmp[3];

	tex.img = NULL;
	if (list->Mat && CMaterial_GetMapName(list->Mat, MAP_GLOW) &&
		!CBmp_Load(&tex, CMaterial_GetMapName(list->Mat, MAP_GLOW), 0)) {
		CBmpFlt_CreateFill(&list->lightA, list->lx, list->ly, zero);
		int x0 = list->ext[0];
		int y0 = list->ext[1];
		for (BspTri *t = list; t; t = t->next) {
			for (int y = 0; y < list->mask.height; y++) {
				for (int x = 0; x < list->mask.width; x++) {
					if ((uint32_t)x >= (uint32_t)g_ShadowMapTexX || (uint32_t)y >= (uint32_t)g_ShadowMapTexY) {
						continue;
					}
					if (Color_SwapRB(*CBmp_Pixel(&list->mask, (uint32_t)x, (uint32_t)y)) != (uint32_t)(uintptr_t)t) {
						continue;
					}
					float c[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
					if (tex.img) {
						const float *s = Bsp_SampleTexAA(tmp, x0 + x, y + y0, list->axis,
							g_ShadowMapScale * g_ShadowMapRes, t, &tex, 4);
						float k = g_LightmapGlowMult;
						float r = s[0], g = s[1], b = s[2];
						c[0] = r * r * k;
						c[1] = g * g * k;
						c[2] = b * b * k;
					}
					memcpy(CBmpFlt_Pixel(&list->lightA, (uint32_t)x, (uint32_t)y), c, sizeof(c));
				}
			}
		}
	}
	Img_Free(&tex, 4);
}

/* the share of samples rays from the sphere of radius r at p (moved along n) to the sphere of the light at light
 * that the floor does not block */
float Bsp_Occlusion(const vec3_t *p, const vec3_t *n, float r, int samples, const vec3_t *light, CGround *ground) {
	vec3_t lp = *light;
	float dy = lp.y - p->y, dx = lp.x - p->x, dz = lp.z - p->z;
	float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	float rad = dist * 0.004638f * 4.0f;
	int hits = 0;
	int info[3] = { 0, 0, 0 };

	if (samples > 0) {
		float fn = Uint_ToFloat((uint32_t)samples);
		for (int i = 0; i < samples; i++) {
			float d[3];
			float z = Uint_ToFloat((uint32_t)i) / fn * 2.0f - 1.0f;
			float v = (float)((double)Uint_ToFloat(Bits_Reverse((uint32_t)i)) * 2.3283064365386963e-10);
			Bsp_DirSphere(d, z, v);
			vec3_t from = { d[0] * r + (p->x + n->x * r), d[1] * r + (n->y * r + p->y), d[2] * r + (n->z * r + p->z) };
			float s1 = (float)((double)(crt_rand() & 0x7fff) * 3.051850947599719e-05 + 0.0);
			float z2 = (float)((double)(crt_rand() & 0x7fff) * 3.051850947599719e-05 + 0.0) * 2.0f - 1.0f;
			Bsp_DirSphere(d, z2, s1);
			vec3_t to = { d[0] * rad + lp.x, d[1] * rad + lp.y, d[2] * rad + lp.z };
			float threshold = 0.8f / (g_ShadowMapRes * g_ShadowMapScale);
			if (CGround_Trace(ground, &from, &to, NULL, threshold, info)) {
				hits++;
			}
		}
	}
	return 1.0f - (float)hits / (float)samples;
}


/* the texels of no list (mask 0) take the weighted average of the neighbours of a list (sides 16, corners 1);
 * without any they are marked (4, 0, 4, 0) */
void CBmpFlt_FillGaps(CBmpFlt *img, const CBmp *mask) {
	static const int dx[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
	static const int dy[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
	static const int weight[8] = { 1, 16, 1, 16, 16, 1, 16, 1 };

	if (!img->img) {
		return;
	}
	uint32_t w = (uint32_t)img->width;
	int h = img->height;
	for (int y = 0; y < h; y++) {
		const uint32_t *pm = CBmp_Pixel(mask, 0, (uint32_t)y);
		float *pf = CBmpFlt_Pixel(img, 0, (uint32_t)y);
		for (int x = 0; x < (int)w; x++, pm++, pf += 4) {
			if (*pm) {
				continue;
			}
			float sw = 0.0f, sr = 0.0f, sg = 0.0f, sb = 0.0f, sa = 0.0f;
			for (int k = 0; k < 8; k++) {
				uint32_t xx = (uint32_t)(dx[k] + x);
				uint32_t yy = (uint32_t)(dy[k] + y);
				if (xx >= w || yy >= (uint32_t)h || !*CBmp_Pixel(mask, xx, yy)) {
					continue;
				}
				const float *p = CBmpFlt_Pixel(img, xx, yy);
				float f = (float)weight[k];
				float p0 = p[0], p1 = p[1], p2 = p[2], p3 = p[3];
				sr = f * p0 + sr;
				sg = f * p1 + sg;
				sb = f * p2 + sb;
				sw = f + sw;
				sa = p3 * f + sa;
			}
			if (sw > 0.0f) {
				pf[0] = sr / sw;
				pf[1] = sg / sw;
				pf[2] = sb / sw;
				pf[3] = sa / sw;
			} else {
				pf[0] = 4.0f;
				pf[1] = 0.0f;
				pf[2] = 4.0f;
				pf[3] = 0.0f;
			}
		}
	}
}


/* the light map settings back into "<map>.ini" when one changed (a backup of the old file first) */
void CBsp_SaveIni(CBsp *self) {
	char name[0x104];
	CIniFile ini;
	const char *s = "mapa_sombras";

	if (!g_LmapSaveIni) {
		return;
	}
	Path_Merge(name, ".ini", self->name);
	CIniFile_ctorLoad(&ini, name, NULL);
	int n = CIniFile_SetFloat(&ini, s, "escala", g_ShadowMapScale, 3);
	n += CIniFile_SetFloat(&ini, s, "resolucion", g_ShadowMapRes, 3);
	n += CIniFile_SetFloat(&ini, s, "gamma", GAME_GLOBAL(float, g_ShadowMapGamma), 2);
	n += CIniFile_SetFloat(&ini, s, "brillo", GAME_GLOBAL(float, g_ShadowMapBrightness), 2);
	n += CIniFile_SetBool(&ini, s, "sombras_suaves", GAME_GLOBAL(int, g_ShadowMapSoft));
	n += CIniFile_SetInt(&ini, s, "text_tam_x", g_ShadowMapTexX);
	n += CIniFile_SetInt(&ini, s, "text_tam_y", g_ShadowMapTexY);
	n += CIniFile_SetBool(&ini, s, "radiosidad", g_ShadowMapRadiosity);
	n += CIniFile_SetBool(&ini, s, "ilum_global", GAME_GLOBAL(int, g_ShadowMapIllumGlobal));
	n += CIniFile_SetBool(&ini, s, "no_luz_amb", GAME_GLOBAL(int, g_ShadowMapNoLightAmb));
	n += CIniFile_SetBool(&ini, s, "modulate4x", g_ShadowMapModulate4x);
	n += CIniFile_SetFloat(&ini, s, "modulate_coef", g_ShadowMapModulate4x ? 0.25f : 1.0f, 3);
	n += CIniFile_SetFloat(&ini, s, "modulate_max", g_ShadowMapModulate4x ? 1.0f : 1.2f, 3);
	n += CIniFile_SetInt(&ini, s, "rebotes", GAME_GLOBAL(int, g_ShadowMapBounces));
	n += CIniFile_SetFloat(&ini, s, "coef_rad", GAME_GLOBAL(float, g_ShadowMapCoefRad), 3);
	n += CIniFile_SetBool(&ini, s, "calc_lmap2", g_CalcLmap2);
	n += CIniFile_SetBool(&ini, s, "usar_txf", g_ShadowMapUseTxf);
	if (n) {
		File_Backup(name, 0);
		if (CIniFile_Save(&ini, name, NULL, 0)) {
			Err_Report(*crt_errno(), 0, "\n'%s'", name);
		}
	}
	CIniFile_dtor(&ini);
}


/* the .EMI file of the map: the materials, the light map pages and the lists */

int CBsp_ReadEMI(CBsp *self) {
	char path[0x104], dir[0x104], texPath[0x104], buf[0x104];
	char sector[0x80];
	CHashTable tabMat, tabLMap;

	Path_Merge(path, ".emi", self->name);
	if (File_TimeDiff(path, self->name, 0) < 0) {
		Con_Warnf("WARN: [%s] is older than [%s]\n", path, self->name);
	}
	CFileIO *mem = operator_new(sizeof(CFileIO));
	CFileIO *f = mem ? CFileIO_ctor(mem, path, "rb") : NULL;
	if (f->error) {
		CFileIO_dtor(f);
		operator_delete_sized(f, sizeof(CFileIO));
		return -1;
	}
	Path_Part(dir, path, 3);
	CHECK(CFileIO_BeginBlock(f, "EMI") == 0);
	int version = CFileIO_ReadInt(f);
	CHECK(version >= 103 && version <= 105);
	E3D_LogDebug2(g_FileLog, "EMI: Version: %d", version);
	CHashTable_ctor(&tabMat, 0x1f);
	CHashTable_ctor(&tabLMap, 0x1f);
	int n = CFileIO_ReadInt(f);
	E3D_LogDebug2(g_FileLog, "EMI: Materials: %d", n);
	String_Copy(texPath, dir, sizeof(texPath));
	CIniFile *ini = World_Get()->GroundScale->ini;
	CHECK(ini);
	CIniFile_GetText(ini, "Model", "TexturePath", texPath, sizeof(texPath), NULL);
	if (n > 0) {
		do {
			CMaterial *mm = operator_new(sizeof(CMaterial));
			CMaterial *mat = mm ? CMaterial_ctor(mm) : NULL;
			CHashTable_AddInt(&tabMat, CFileIO_ReadInt(f), mat, 0);
			CMatList_Add(g_MatList, mat);
			CMaterial_Read(mat, f);
			mat->params.attrib |= MAT_NOSHARE;
			/* the maps are looked for in the texture path of the map */
			for (int i = 0; i < MAP_TOTAL; i++) {
				CHECK(i < MAP_TOTAL);
				CMap *map = mat->map[i];
				if (map) {
					Path_Part(buf, map->Name, 0xc);
					Path_Merge(buf, buf, texPath);
					String_Set(&map->Name, buf);
				}
			}
			CHECK(!f->error);
		} while (--n);
	}
	BspLMap *old = g_BspLMaps;
	if (old) {
		BspLMap_dtor(old);
		operator_delete_sized(old, sizeof(BspLMap));
		g_BspLMaps = NULL;
	}
	/* without light maps every page is the grey texture */
	const char *pre = g_MaxLightmap ? "" : "bmp/rgb_grey.tga";
	int id = 0;
	BspLMap **pp = &g_BspLMaps;
	for (int key = CFileIO_ReadInt(f); key; key = CFileIO_ReadInt(f)) {
		*pp = BspLMap_New();
		CHashTable_AddInt(&tabLMap, key, *pp, 0);
		(*pp)->id = ++id;
		CFileIO_ReadString(f, buf, sizeof(buf));
		Path_Merge(buf, buf, dir);
		Path_Merge(buf, pre, buf);
		BspLMap_CreateMat(*pp, buf, 1);
		if (version >= 104) {
			CHECK(CFileIO_ReadInt(f) == 1);
			CFileIO_ReadString(f, buf, sizeof(buf));
			Path_Merge(buf, buf, dir);
			Path_Merge(buf, pre, buf);
			if (g_LmapInfo || g_CalcLmap2) {
				(*pp)->lmap2 = BspLMap_New();
				(*pp)->lmap2->id = id;
				BspLMap_CreateMat((*pp)->lmap2, buf, 2);
			}
		}
		pp = &(*pp)->next;
	}
	CHECK(!self->num_lists);
	self->num_lists = CFileIO_ReadInt(f);
	E3D_LogDebug2(g_FileLog, "EMI: List count: %d", self->num_lists);
	CHECK(self->num_lists < BSP_MAX_LISTS);
	for (int i = 0; i < self->num_lists; i++) {
		BspTri *tm = operator_new(sizeof(BspTri));
		BspTri *tri = tm ? BspTri_ctor(tm) : NULL;
		self->lists[i] = tri;
		CFileIO_BeginBlock(f, "TRI");
		tri->flags = (uint32_t)CFileIO_ReadInt(f);
		CFileIO_ReadString(f, buf, sizeof(buf));
		tri->zone = CFileIO_ReadInt(f);
		if (version >= 105) {
			CFileIO_ReadString(f, sector, sizeof(sector));
			tri->zone = Sector_Find(sector);
			if (!strcmp(sector, "_nosector_")) {
				E3D_LogInfo(g_FileLog, "WARN: Mesh without sector [%s]", buf);
			} else if (!strcmp(sector, "_unasigned_")) {
				Con_Warnf("WARN: Mesh without sector [%s]\n", buf);
			}
		}
		CHECK(tri->zone < Sector_Num());
		CHECK(!CHashTable_GetInt(&tabMat, CFileIO_ReadInt(f), (void **)&tri->Mat));
		CHashTable_GetInt(&tabLMap, CFileIO_ReadInt(f), (void **)&tri->LMap);
		CHECK(tri->LMap || tri->flags & BSPTRI_NOLMAP);
		tri->numTri = CFileIO_ReadInt(f);
		CHECK(!f->error);
		tri->pTri = New_Array((uint32_t)tri->numTri, 6);
		CFileIO_Read(f, tri->pTri, tri->numTri * 6);
		CVertList_Read(&tri->lv, f);
		CVertList_Read(&tri->lv2, f);
		if (!tri->Mat->params.transp) {
			CVertList_SetColor(&tri->lv, 0xffffff, 0);
		}
		CFileIO_Error(f, "TRI");
	}
	CFileIO_Error(f, "EMI");
	CHECK(!f->error);
	CFileIO_dtor(f);
	operator_delete_sized(f, sizeof(CFileIO));
	CHashTable_dtor(&tabLMap);
	CHashTable_dtor(&tabMat);
	return 0;
}

/* the triangles of the map by material and vertex format, for the log */
typedef struct BspMatStat {
	CMaterial *Mat;			/* 0x00 */
	int numTri;			/* 0x04 the sort key (BspTri_CmpId) */
	int numVert;			/* 0x08 */
	int count;			/* 0x0c lists */
	int frmt;			/* 0x10 */
} BspMatStat;
SD_SIZE(BspMatStat, 0x14);

static BspMatStat *BspMatStat_Get(CTable *t, int i) {
	TABLE_CHECK(t->p, i, t->n);
	return &((BspMatStat *)t->p)[i];
}

void CBsp_Prepare(CBsp *self) {
	CTable tab = { 0, NULL };

	if (g_DebugLevel < 3) {
		return;
	}
	E3D_LogDebug(g_FileLog, "\nMap triangles by material/sector:");
	for (int zn = 0; zn < self->zones.n; zn++) {
		TABLE_CHECK(self->zones.p, zn, self->zones.n);
		BspTri *tri = ((BspZone *)self->zones.p)[zn].tris;
		const char *node = tri && tri->node ? tri->node->name : "(null)";
		Con_Verbosef("  Sector: %d  name: %s  node: %s\n", zn - 1, zn ? Sector_GetName(zn - 1) : "_unasigned_",
			node);
		if (!tri) {
			Con_Warnf("    sector without triangles\n");
			continue;
		}
		for (; tri; tri = tri->zone_next) {
			CHECK(tri->zone + 1 == zn);
			const char *lmap = tri->LMap ? Path_FileName(CMaterial_GetMapName(tri->LMap->Mat, MAP_DIFFUSE)) : "-";
			Con_Debugf("    tri: %-5d  %-5d  %-6s    %-20s  ", tri->numTri, tri->lv.nVert, CVertList_FormatName(&tri->lv),
				lmap);
			Bsp_MatInfo(tri->Mat);
			if (tri->numTri < 16) {
				Con_Bluef("    ============\n");
			}
			int i = 0;
			for (; i < tab.n; i++) {
				if (BspMatStat_Get(&tab, i)->Mat == tri->Mat && BspMatStat_Get(&tab, i)->frmt == tri->lv.frmt) {
					break;
				}
			}
			if (i == tab.n) {
				Table_Resize14(&tab, i + 1);
				memset(&((BspMatStat *)tab.p)[i], 0, sizeof(BspMatStat));
			}
			BspMatStat *e = BspMatStat_Get(&tab, i);
			e->Mat = tri->Mat;
			e->numTri += tri->numTri;
			e->numVert += tri->lv.nVert;
			e->count++;
			CHECK(e->frmt == tri->lv.frmt || e->frmt == 0);
			e->frmt = tri->lv.frmt;
		}
	}
	E3D_LogDebug(g_FileLog, "\nTotal map triangles by material:");
	crt_qsort(tab.p, (size_t)tab.n, sizeof(BspMatStat), (int (*)(const void *, const void *))(void *)BspTri_CmpId);
	int totTri = 0, totVert = 0;
	for (int i = 0; i < tab.n; i++) {
		BspMatStat *e = BspMatStat_Get(&tab, i);
		uint32_t bytes = (uint32_t)(VertList_FormatSize(e->frmt) * e->numVert);
		Con_Debugf("  tri: %-5d  %-5d  %-6s %6.1fk   dis: %-3d", e->numTri, e->numVert, VertList_FormatName(e->frmt),
			(double)bytes * 0.0009765625, e->count);
		Bsp_MatInfo(e->Mat);
		totTri += e->numTri;
		totVert += e->numVert;
	}
	E3D_LogDebug(g_FileLog, "\nMap total: %d materials, %d triangles, %d vertexes\n", tab.n, totTri, totVert);
	if (tab.p) {
		operator_delete_array(tab.p);
	}
}

void CBsp_InitD3D(CBsp *self) {
	/* the vertex size and FVF of the buffers of a sector (BspZone_GetVB: the first four) */
	static const struct {
		int size;
		uint32_t fvf;
	} fmts[8] = {
		{ 0x1c, 0x1c2 }, { 0x24, 0x152 }, { 0x20, 0x242 }, { 0x2c, 0x252 },
		{ 0x12, 0 }, { 0x16, 0 }, { 0x16, 0 }, { 0x1a, 0 },
	};
	CMaterial mat;

	CHECK(g_Engine);
	/* one vertex buffer of each format and one index buffer a sector, as large as its lists */
	for (int zn = 0; zn < self->zones.n; zn++) {
		TABLE_CHECK(self->zones.p, zn, self->zones.n);
		BspZone *z = &((BspZone *)self->zones.p)[zn];
		BspTri *tri = z->tris;
		CHECK(tri || !zn);
		for (int k = 0; k < 8; k++) {
			CRenderVB_Reset(&z->vb[k]);
		}
		CRenderIB_Reset(&z->ib);
		for (; tri; tri = tri->zone_next) {
			BspZone_GetVB(z, tri->lv.frmt)->Used += tri->lv.nVert;
			z->ib.Used += tri->numTri;
		}
		for (int k = 0; k < 8; k++) {
			CRenderVB_Create(&z->vb[k], (uint32_t)z->vb[k].Used, fmts[k].size, fmts[k].fvf);
		}
		CRenderIB_Create(&z->ib, (uint32_t)z->ib.Used);
	}
	/* the lists into the buffers of their sector, or buffers of their own */
	for (int i = 0; i < self->num_lists; i++) {
		BspTri *tri = self->lists[i];
		CHECK(!tri->pVertVB);
		CHECK(!tri->pVertIB);
		BspZone *z = CBsp_Zone(self, tri->zone);
		CRenderVB *vb = BspZone_GetVB(z, tri->lv.frmt);
		if (vb->pVertVB) {
			tri->pVertVB = CRenderVB_Get(vb, &tri->baseVert);
			CRenderVB_Add(vb, tri->lv.nVert, tri->lv.pVert);
			tri->pVertIB = CRenderIB_Get(&z->ib, &tri->startTri);
			CRenderIB_Add(&z->ib, tri->numTri, tri->pTri);
			continue;
		}
		tri->baseVert = 0;
		tri->startTri = 0;
		if (tri->lv.nVert < 6 || tri->numTri < 3) {
			E3D_LogInfo(g_FileLog, "EMI: List index: %d   tri: %d   vert: %d", i, tri->numTri, tri->lv.nVert);
		}
		CHECK(Render_CreateVB(&tri->pVertVB, &tri->lv, !g_ShadowMapMultitext) == 0);
		CHECK(Render_CreateIB(&tri->pVertIB, tri->pTri, tri->numTri, !g_ShadowMapMultitext) == 0);
	}
	/* the materials of the debug views: the light maps alone and the wire frame */
	CMaterial_ctor(&mat);
	String_Set(&mat.Name, "MatVerMapas");
	CMaterial_SetMap(&mat, MAP_DIFFUSE, "bmp/rgb_grey.tga");
	mat.params.lighting = 0;
	self->matFX = Mat_New(&mat, 1);
	String_Set(&mat.Name, "MatVerMalla");
	CMaterial_SetMap(&mat, MAP_DIFFUSE, "bmp/rgb_white.tga");
	mat.params.lighting = 0;
	mat.params.attrib = 0x800;
	self->matWire = Mat_New(&mat, 1);
	CModel_InitD3D(self->model, g_Engine->device, 1);
	CMaterial_dtor(&mat);
}

/* the static initializer: the trace caches of the lights cleared */
void Bsp_InitLights(void) {
	for (int i = 0; i < BSP_MAX_LIGHTS; i++) {
		g_BspLights[i].cache[0] = 0;
		g_BspLights[i].cache[1] = 0;
		g_BspLights[i].cache[2] = 0;
	}
}
