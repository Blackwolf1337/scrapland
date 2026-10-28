/*
 * Engine/bsp.cpp of the engine3d: the light maps of the floor, see include/scrap/bsp.h. The map compiler (CBsp_Calcular),
 * its lights, texels, vertex lists, direct light and radiosity passes and the .EMI writer are in BspCalc.c.
 */
#include <string.h>

#include "scrap/bsp.h"
#include "scrap/console.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/escena.h"
#include "scrap/fuentes.h"
#include "scrap/leem3d.h"
#include "scrap/luz.h"
#include "scrap/mallad3d.h"
#include "scrap/portal.h"
#include "scrap/prof.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapworld.h"
#include "scrap/suelo.h"
#include "scrap/sueloportal.h"
#include "scrap/texto.h"
#include "scrap/utild3d.h"
#include "scrap/varios.h"
#include "scrap/win32.h"

typedef uint32_t(__stdcall *getenvvar_t)(const char *name, char *buf, uint32_t size);
#define exe_GetEnvironmentVariableA GAME_DATA(getenvvar_t, 0x3a12c0)

/* the "uint as double" conversions of the original: the int plus 2^32 for a set top bit */
static inline float Uint_ToFloat(uint32_t u) {
	return (float)(double)u;
}


/* engine3d/engine/bmp.h, bmpflt.h, bmpflt2.h */

void __fastcall CBmpFlt2_dtor(CBmpFlt2 *self) {
	Img_Free(self, 0xc);
}

void __fastcall CBmpFlt_dtor(CBmpFlt *self) {
	Img_Free(self, 0x10);
}

void __thiscall CBmp_Assign(CBmp *self, const CBmp *orig) {
	Img_Free(self, 4);
	CBmp_Resample(self, orig, 0xf);
}

void __thiscall CBmpFlt2_Assign(CBmpFlt2 *self, const CBmpFlt2 *orig) {
	Img_Free(self, 0xc);
	CBmpFlt2_Resample(self, orig, 0xf);
}

void __thiscall CBmp_CreateFill(CBmp *self, int ancho, int alto, uint32_t color) {
	Img_Create(self, ancho, alto, 4);
	CBmp_Fill(self, color);
}

void __thiscall CBmpFlt2_CreateFill(CBmpFlt2 *self, int ancho, int alto, const float *color) {
	Img_Create(self, ancho, alto, 0xc);
	CBmpFlt2_Fill(self, color);
}

void __thiscall CBmpFlt_CreateFill(CBmpFlt *self, int ancho, int alto, const float *color) {
	Img_Create(self, ancho, alto, 0x10);
	CBmpFlt_Fill(self, color);
}

/* pixel (x, y) of self is pixel (alto - 1 - y, x) of orig */
static void Img_Rotate(CImagen *self, const CImagen *orig, int bytes) {
	uint32_t ancho = (uint32_t)orig->ancho;
	uint32_t alto = (uint32_t)orig->alto;

	Img_Create(self, (int)alto, (int)ancho, bytes);
	for (int y = 0; y < self->alto; y++) {
		for (int x = 0; x < self->ancho; x++) {
			const void *src = Img_Pixel(orig, (uint32_t)(self->alto - y - 1), (uint32_t)x, bytes);
			memcpy(Img_Pixel(self, (uint32_t)x, (uint32_t)y, bytes), src, (size_t)bytes);
		}
	}
}

void __thiscall CBmp_Rotate(CBmp *self, const CBmp *orig) {
	CHECK(orig);
	Img_Rotate(self, orig, 4);
}

void __thiscall CBmpFlt2_Rotate(CBmpFlt2 *self, const CBmpFlt2 *orig) {
	CHECK(orig);
	Img_Rotate(self, orig, 0xc);
}


/* the TXF cache */

BspTxf *__thiscall BspTxf_ctor(BspTxf *self, const char *mapa, const char *name, int n, int on) {
	self->on = on && (g_MapaSombrasUsarTxf || g_TxfMode);
	self->base[0] = 0;
	self->nombre[0] = 0;
	self->equipo[0] = 0;
	self->FileTXF = NULL;
	self->written = 0;
	self->skipped = 0;
	self->locked = 0;
	self->writeRetries = 0;
	self->readRetries = 0;
	self->deleted = 0;
	self->rebuild = 1;
	self->inicio = exe_timeGetTime();
	CHECK(mapa && name);
	Bprintf(self->base, sizeof(self->base), "&u&d_TXF/&n.%s.%d", name, n);
	Ruta_Format(self->base, mapa, self->base);
	Ruta_MakePath(self->base);
	memset(self->equipo, 0, sizeof(self->equipo));
	exe_GetEnvironmentVariableA("COMPUTERNAME", self->equipo, sizeof(self->equipo));
	return self;
}

void __fastcall BspTxf_dtor(BspTxf *self) {
	CHECK(!self->FileTXF);
}

void __thiscall BspTxf_SetName(BspTxf *self, int i) {
	Bprintf(self->nombre, sizeof(self->nombre), "%s.%05d.TXF", self->base, i + 1);
}

void __fastcall BspTxf_Info(BspTxf *self) {
	int secs = (int)((double)(exe_timeGetTime() - self->inicio) / 1000.0);
	int m = secs / 60 % 60;
	int s = secs % 60;

	E3D_DebugPrintf("\nTXF reloading finished: %s", Ruta_FileName(self->base));
	E3D_DebugPrintf("\ntime: %d:%02d:%02d hms", secs / 3600, m, s);
	E3D_DebugPrintf("\nwritten: %d, skipped: %d, already locked: %d", self->written, self->skipped, self->locked);
	E3D_DebugPrintf("\nwrite retries: %d, read retries: %d, auto deleted: %d\n", self->writeRetries,
		self->readRetries, self->deleted);
	E3D_LogPrint(g_FileLog, "TXF info: %s", Ruta_FileName(self->base));
	E3D_LogPrint(g_FileLog, " time: %d:%02d:%02d hms", secs / 3600, m, s);
	E3D_LogPrint(g_FileLog, " written: %d", self->written);
	E3D_LogPrint(g_FileLog, " skipped: %d", self->skipped);
	E3D_LogPrint(g_FileLog, " already locked: %d", self->locked);
	E3D_LogPrint(g_FileLog, " write retries: %d", self->writeRetries);
	E3D_LogPrint(g_FileLog, " read retries: %d", self->readRetries);
	E3D_LogPrint(g_FileLog, " auto deleted: %d", self->deleted);
}


/* the lists */

BspTri *__fastcall BspTri_ctor(BspTri *self) {
	self->flags = 0;
	self->luz.img = NULL;
	self->luz2.img = NULL;
	self->luzA.img = NULL;
	self->rad.img = NULL;
	self->rad2.img = NULL;
	self->mask.img = NULL;
	self->mask2.img = NULL;
	self->nombre = NULL;
	self->radColor = 0;
	self->nombres = NULL;
	self->radInt = 1.0f;
	self->col144[0] = 0.0f;
	self->col144[2] = 0.0f;
	self->col144[1] = 0.0f;
	self->col144[3] = 1.0f;
	for (int i = 0; i < 4; i++) {
		self->areas[i].p = NULL;
		self->areas[i].n = 0;
	}
	CListaVert_ctor(&self->lv);
	CListaVert_ctor(&self->lv2);
	CListaVert_ctor(&self->lv3);
	self->sig = NULL;
	self->Mat = NULL;
	self->LMap = NULL;
	self->nodo = NULL;
	self->pTri = NULL;
	self->pVertVB = NULL;
	self->pVertIB = NULL;
	self->zona = -1;
	self->zona_sig = NULL;
	self->region = NULL;
	self->i12c = 0;
	return self;
}

void __fastcall BspTri_dtor(BspTri *self) {
	if (self->sig) {
		BspTri *sig = self->sig;
		BspTri_dtor(sig);
		operator_delete_sized(sig, sizeof(BspTri));
		self->sig = NULL;
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
	CListaVert_Free(&self->lv3);
	CListaVert_Free(&self->lv2);
	CListaVert_Free(&self->lv);
	for (int i = 3; i >= 0; i--) {
		Tabla_Free(&self->areas[i]);
	}
	if (self->nombres) {
		BspNombre *nb = self->nombres;
		BspNombre_dtor(nb);
		operator_delete_sized(nb, sizeof(BspNombre));
		self->nombres = NULL;
	}
	if (self->nombre) {
		Cad_Set(&self->nombre, NULL);
	}
	Img_Free(&self->mask2, 4);
	Img_Free(&self->mask, 4);
	Img_Free(&self->rad2, 0xc);
	Img_Free(&self->rad, 0xc);
	Img_Free(&self->luzA, 0x10);
	Img_Free(&self->luz2, 0xc);
	Img_Free(&self->luz, 0xc);
}

void __fastcall BspNombre_dtor(BspNombre *self) {
	if (self->sig) {
		BspNombre *sig = self->sig;
		BspNombre_dtor(sig);
		operator_delete_sized(sig, sizeof(BspNombre));
		self->sig = NULL;
	}
	if (self->nombre) {
		Cad_Set(&self->nombre, NULL);
	}
}

void __fastcall BspNodo_dtor(BspNodo *self) {
	if (self->sig) {
		BspNodo *sig = self->sig;
		BspNodo_dtor(sig);
		operator_delete_sized(sig, sizeof(BspNodo));
		self->sig = NULL;
	}
	if (self->pTri) {
		operator_delete_sized(self->pTri, 6);
		self->pTri = NULL;
	}
	CHECK(!self->pVertVB);
	CListaVert_Free(&self->lv);
}

void __fastcall BspLMap_dtor(BspLMap *self) {
	if (self->sig) {
		BspLMap *sig = self->sig;
		BspLMap_dtor(sig);
		operator_delete_sized(sig, sizeof(BspLMap));
		self->sig = NULL;
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

void __thiscall BspLMap_Init(BspLMap *self, int ancho, int alto) {
	self->minX = ancho + 1;
	self->minY = alto + 1;
	CBmp_CreateFill(&self->bmp, ancho, alto, 0xffff0000);
	if (g_LmapInfo2 && g_LmapInfo) {
		CBmp_CreateFill(&self->info, ancho, alto, 0x100020);
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

BspZona *__fastcall BspZona_ctor(BspZona *self) {
	for (int i = 0; i < 8; i++) {
		self->vb[i].pVertVB = NULL;
		CRenderVB_Reset(&self->vb[i]);
	}
	self->ib.pVertIB = NULL;
	CRenderIB_Reset(&self->ib);
	self->tris = NULL;
	return self;
}

void __fastcall BspZona_dtor(BspZona *self) {
	CRenderIB_Reset(&self->ib);
	for (int i = 7; i >= 0; i--) {
		CRenderVB_Reset(&self->vb[i]);
	}
}

/* the unwind funclets of the BspZona constructor: one CRenderVB member */
void __fastcall BspVB_Unwind(CRenderVB *self) {
	CRenderVB_Reset(self);
}

#define BSPZONA_DTOR 0x2824b0

BspZona *__thiscall BspZona_Delete(BspZona *self, uint32_t flags) {
	if (flags & 2) {
		uint32_t *mem = (uint32_t *)self - 1;
		crt_eh_vector_dtor(self, sizeof(BspZona), *mem, Game_Ptr(BSPZONA_DTOR));
		if (flags & 1) {
			operator_delete_array(mem);
		}
		return (BspZona *)mem;
	}
	BspZona_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(BspZona));
	}
	return self;
}

void __fastcall Tabla_FreeB4(CTabla *t) {
	if (t->p) {
		Delete_Objects(t->p, sizeof(BspZona), BSPZONA_DTOR);
		t->p = NULL;
		t->n = 0;
	}
}

CRenderVB *__thiscall BspZona_GetVB(BspZona *self, int frmt) {
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

void __fastcall CBsp_dtor(CBsp *self) {
	if (self->luces) {
		void *luces = self->luces;
		BspLuces_dtor(luces);
		operator_delete_sized(luces, 4);
		self->luces = NULL;
	}
	if (self->nodos) {
		BspNodo *nodos = self->nodos;
		BspNodo_dtor(nodos);
		operator_delete_sized(nodos, sizeof(BspNodo));
		self->nodos = NULL;
	}
	Modelo_Destroy(&self->modelo, 0);
	for (int i = 0; i < self->num_listas; i++) {
		BspTri *tri = self->listas[i];
		if (tri) {
			BspTri_dtor(tri);
			operator_delete_sized(tri, sizeof(BspTri));
			self->listas[i] = NULL;
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
	Tabla_FreeB4(&self->zonas);
	if (self->nombre) {
		Cad_Set(&self->nombre, NULL);
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

void BspTri_CalcCentro(BspTri *tri) {
	const vec3_t *v = tri->v;
	float cx = (v[0].x + v[1].x + v[2].x) / 3.0f;
	float cy = (v[0].y + v[1].y + v[2].y) / 3.0f;
	float cz = (v[0].z + v[1].z + v[2].z) / 3.0f;
	float d0 = Bsp_Dist(cx - v[0].x, cy - v[0].y, cz - v[0].z);
	float d1 = Bsp_Dist(cx - v[1].x, cy - v[1].y, cz - v[1].z);
	float d2 = Bsp_Dist(cx - v[2].x, cy - v[2].y, cz - v[2].z);

	tri->centro = (vec3_t){ cx, cy, cz };
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
	for (; list; list = list->sig) {
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

void BspTri_CalcPlanos(BspTri *list) {
	for (; list; list = list->sig) {
		CHECK(Plane_Calc(&list->plano, &list->v[0], &list->v[1], &list->v[2]));
		float ax = (float)__builtin_fabs((double)list->plano.n.x);
		float ay = (float)__builtin_fabs((double)list->plano.n.y);
		float az = (float)__builtin_fabs((double)list->plano.n.z);
		if (ax >= ay && ax >= az) {
			list->eje = 0;
		} else if (ay >= ax && ay >= az) {
			list->eje = 1;
		} else if (az >= ax && az >= ay) {
			list->eje = 2;
		} else {
			CHECK(0);
		}
	}
}

/* the component of the normal along the axis of the list */
static inline float BspTri_EjeN(const BspTri *tri) {
	if (tri->eje == 0) {
		return tri->plano.n.x;
	}
	if (tri->eje == 1) {
		return tri->plano.n.y;
	}
	return tri->plano.n.z;
}

int BspTri_Lado(const BspTri *tri) {
	float s = BspTri_EjeN(tri);

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
	float px = tri->plano.n.x, py = tri->plano.n.y, pz = tri->plano.n.z;
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

byte BspTri_EsTexel(int x0, int y0, int x, int y, uint32_t color, const BspTri *tri) {
	uint32_t dx = (uint32_t)(x - x0);
	uint32_t dy = (uint32_t)(y - y0);

	if (dx >= (uint32_t)g_MapaSombrasTexX || dy >= (uint32_t)g_MapaSombrasTexY) {
		return 0;
	}
	return Color_SwapRB(*CBmp_Pixel(&tri->mask, dx, dy)) == color;
}

int BspTri_Zona(CNodo3D *nodo) {
	if (!nodo || nodo->flags >> 28 & 1) {
		return -1;
	}
	return Sector_OfNode(nodo);
}

const char *BspTri_ZonaName(const BspTri *tri) {
	if (tri->zona == -1) {
		return Game_Ptr(0x3dd0a4);	/* "_unasigned_" */
	}
	return Sector_GetName(tri->zona);
}

int BspTri_Count(const BspTri *list) {
	int n = 0;

	for (; list; list = list->sig) {
		n++;
	}
	return n;
}

int BspTri_CmpId(const void *a, const void *b) {
	return ((const int *)a)[1] - ((const int *)b)[1];
}

/* a node out of the map: unlinked (liberar) and, when nothing else holds it, marked to be deleted */
void Bsp_NodoMarcar(CNodo3D *nodo, byte liberar) {
	if (liberar) {
		CNodo3D_FreeObj3D(nodo);
	}
	if (nodo->Refs - (nodo->ant != NULL) - (nodo->sig != NULL) == 0) {
		nodo->flags |= 0x20008;
	}
}


/* drawing the map */

/* the pass of a material: 0 solid, 1 transparent with atrib 0x10, 2 transparent */
static byte Bsp_MatPass(const CMaterial *m) {
	if (!Mat_IsTransp(m) || m->params.atrib & 0x800) {
		return 0;
	}
	return m->params.atrib & 0x10 ? 1 : 2;
}

void __stdcall BspTri_Draw(BspTri *tri) {
	if (tri->pVertVB && g_UsarVB) {
		Render_DrawVBIBFmt(tri->pVertVB, tri->pVertIB, tri->lv.frmt, (uint32_t)tri->lv.nVert, (uint32_t)tri->numTri,
			(uint32_t)tri->baseVert, (uint32_t)tri->startTri);
	} else {
		Render_DrawListaIdx(&tri->lv, tri->pTri, tri->numTri, 0);
	}
	g_RenderTris += tri->numTri;
	g_RenderVerts += tri->lv.nVert;
	g_BspTris += tri->numTri;
	g_BspVerts += tri->lv.nVert;
}

/* the modulation of the light maps: 4x with modulate4x on a device without gamma2x */
static inline int Bsp_LmapMode(void) {
	return !g_Gamma2x && g_MapaSombrasModulate4x ? 4 : 1;
}

void __thiscall CBsp_DrawTri(CBsp *self, BspTri *tri, byte pass) {
	CMaterial *mat = tri->Mat;

	if (Bsp_MatPass(mat) != pass) {
		return;
	}
	if (tri->flags & BSPTRI_NOLMAP && g_RenderPass == 2) {
		return;
	}
	if (*g_MapRenderMat && !Wild_Match(g_MapRenderMat, mat->Nombre)) {
		return;
	}
	if (*g_MapRenderMap) {
		if (!CMaterial_GetMapName(tri->Mat, MAPA_DIFFUSE)) {
			return;
		}
		const char *pat = g_MapRenderMap;
		if (!Wild_Match(pat, Ruta_FileName(CMaterial_GetMapName(tri->Mat, MAPA_DIFFUSE)))) {
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
	CHECK(!tri->Mat->params.b39);
	CHECK(!tri->LMap || !tri->LMap->Mat->params.b39);
	if (!g_MapaSombrasMultitext) {
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

/* the zona of sector (-1 for the lists without one) */
static BspZona *CBsp_Zona(CBsp *self, int sector) {
	uint32_t i = (uint32_t)(sector + 1);

	TABLA_CHECK(self->zonas.p, i, self->zonas.n);
	return &((BspZona *)self->zonas.p)[i];
}

void __thiscall CBsp_DrawZona(CBsp *self, int sector, byte pass) {
	BspTri *tri = CBsp_Zona(self, sector)->tris;

	if (g_MapDrawSector >= -1 && g_MapDrawSector != sector) {
		return;
	}
	for (; tri; tri = tri->zona_sig) {
		CHECK(tri->zona == sector);
		CBsp_DrawTri(self, tri, pass);
	}
}

/* the lists of the pass in a zona into g_ListaTri */
static int CBsp_ListZona(BspTri *tri, int sector, byte pass, int n) {
	for (; tri; tri = tri->zona_sig) {
		CHECK(tri->zona == sector);
		if (Bsp_MatPass(tri->Mat) == pass) {
			CHECK(n < BSP_MAX_LISTATRI);
			g_ListaTri[n++] = tri;
		}
	}
	return n;
}

void __thiscall CBsp_DrawPass(CBsp *self, byte pass) {
	byte porSector = g_MapDrawBySector != 0;

	Render_SetWorldMatrix(g_MatIdentity, 0);
	if (!g_RenderSectors) {
		return;
	}
	if (g_RenderSectors == 2) {
		for (int i = 0; i < self->num_listas; i++) {
			CBsp_DrawTri(self, self->listas[i], pass);
		}
		return;
	}
	if (porSector) {
		if (g_PortalesOn) {
			for (int i = 0; i < g_NumZonasPintar; i++) {
				const ZonaPintar *zp = &g_ZonasPintar[pass ? g_NumZonasPintar - i - 1 : i];
				if (Portales_SetView(zp)) {
					CBsp_DrawZona(self, zp->zona, pass);
				}
			}
		}
		Portales_SetView(NULL);
		CBsp_DrawZona(self, -1, pass);
		return;
	}
	int n = 0;
	for (int i = 0; i < g_NumZonasPintar; i++) {
		const ZonaPintar *zp = &g_ZonasPintar[pass ? g_NumZonasPintar - i - 1 : i];
		n = CBsp_ListZona(CBsp_Zona(self, zp->zona)->tris, zp->zona, pass, n);
	}
	TABLA_CHECK(self->zonas.p, 0, self->zonas.n);
	n = CBsp_ListZona(((BspZona *)self->zonas.p)->tris, -1, pass, n);
	crt_qsort(g_ListaTri, (size_t)n, sizeof(BspTri *), (int (*)(const void *, const void *))Game_Ptr(0x28e4a0));
	for (int i = 0; i < n; i++) {
		CBsp_DrawTri(self, g_ListaTri[i], pass);
	}
}

void __fastcall CBsp_DrawWire(CBsp *self) {
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
	if (g_PortalesOn) {
		for (int i = 0; i < g_NumZonasPintar; i++) {
			CBsp_DrawZona(self, g_ZonasPintar[i].zona, 0);
			CBsp_DrawZona(self, g_ZonasPintar[i].zona, 1);
			CBsp_DrawZona(self, g_ZonasPintar[i].zona, 2);
		}
	}
	CBsp_DrawZona(self, -1, 0);
	CBsp_DrawZona(self, -1, 1);
	CBsp_DrawZona(self, -1, 2);
	dev = g_pd3dDevice;
	g_LightmapMode2--;
	g_MatFX1 = NULL;
	g_MatFX2 = NULL;
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_FILLMODE, 3);
}

void __fastcall CBsp_DrawFX(CBsp *self) {
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

void __fastcall CBsp_Render(CBsp *self) {
	if (g_RenderOff) {
		return;
	}
	Render_SetWorldMatrix(g_MatIdentity, 0);
	if (!g_PortalesOn) {
		g_NumZonasPintar = 0;
	} else if (g_ShowPortals) {
		Texto_Printf(10, 0xdc, NULL);
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
	if (!g_PortalesOn) {
		return;
	}
	if (info) {
		Font_SetColor(g_FntComic14, g_BspSecOk ? 0xffffff : 0x800000);
		Font_Printf(g_FntComic14, 10.0f, 200.0f, 2, "%d %d", g_BspSecVisto, g_BspSecErrores);
		Font_Printf(g_FntComic14, 10.0f, 220.0f, 2, "%d", g_NumZonasPintar);
		Font_Printf(g_FntComic14, 10.0f, 240.0f, 2, "%d", g_PintarNum);
		return;
	}
	CSuelo *suelo = Mundo_Get()->ObjSuelo;
	CHECK(suelo);
	vec3_t ojo = *Render_GetEyePos();
	SueloRayo res;
	int s;

	g_BspSecOk = 0;
	res.flags = 0x202;
	res.y = ojo.y;
	if (CSuelo_RayoVert(suelo, ojo.x, ojo.z, &res) && res.sector != -1) {
		vec3_t a = { ojo.x, res.hitY, ojo.z };
		g_BspSecOk = 1;
		s = Sector_Recorrer(res.sector, &a, &ojo, 0);
		g_BspSecVisto = s;
	} else {
		s = g_BspSecVisto;
	}
	if (!g_BspSecOk) {
		s = Sector_Recorrer(s, &g_BspOjoAnt, &ojo, 0);
		g_BspSecVisto = s;
		if (!g_BspSecOk) {
			g_BspSecErrores++;
			g_ProfMode = g_ShowInfoOnSectorError ? 2 : g_ProfMode;
		}
	}
	g_BspOjoAnt = ojo;
	Portales_Cull((uint32_t)s);
}

void __fastcall CBsp_ReleaseD3D(CBsp *self) {
	for (int i = 0; i < self->num_listas; i++) {
		if (self->listas[i]->pVertVB) {
			COM_RELEASE(self->listas[i]->pVertVB);
			self->listas[i]->pVertVB = NULL;
		}
		if (self->listas[i]->pVertIB) {
			COM_RELEASE(self->listas[i]->pVertIB);
			self->listas[i]->pVertIB = NULL;
		}
	}
	for (BspNodo *n = self->nodos; n; n = n->sig) {
		if (n->pVertVB) {
			COM_RELEASE(n->pVertVB);
			n->pVertVB = NULL;
		}
	}
	for (int i = 0; i < self->zonas.n; i++) {
		TABLA_CHECK(self->zonas.p, i, self->zonas.n);
		BspZona *z = &((BspZona *)self->zonas.p)[i];
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
	CModelo_ReleaseD3D(self->modelo, MOTOR_DEV(g_Motor), 1);
}


/* the global functions called by mundo.cpp */

void Bsp_Free(void) {
	if (!g_MapaSombrasUsar) {
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
	if (!g_MapaSombrasUsar) {
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
	if (!g_MapaSombrasUsar) {
		return;
	}
	CBsp_InitD3D(g_Bsp);
	if (!g_MapaSombrasMultitext) {
		CHECK(g_Motor);
		CHECK(!g_pBSP_VB);
	}
}

/* the floor drawn again for the stencil shadows (the callback of g_StencilRender) */
void Bsp_RenderStencil(void) {
	CHECK(g_SombrasStencil);
	if (!g_CalcLmap2 && g_RenderPass == 2) {
		return;
	}
	if (g_RenderPass != 0xc) {
		g_LmapSecond = g_RenderPass == 2;
		CBsp_Render(g_Bsp);
		g_LmapSecond = 0;
	}
	CModelo_Render(g_Bsp->modelo);
	E3D_FlushBatch();
}

void Bsp_Render(void) {
	if (!g_MapaSombrasUsar || !g_RenderMap) {
		return;
	}
	g_StencilRender = g_SombrasStencil ? Game_Ptr(0x296d40) : g_StencilRender;
	CBsp_Render(g_Bsp);
	CModelo_UpdateAnims(g_Bsp->modelo, 0.0f, -1);
	CModelo_Render(g_Bsp->modelo);
	E3D_FlushBatch();
}

void Bsp_RenderTransp(void) {
	if (g_RenderOff || !g_MapaSombrasUsar || !g_RenderMap) {
		return;
	}
	CBsp_DrawPass(g_Bsp, 2);
	if (g_ShowPortals) {
		Bsp_PortalInfo(0);
	}
	if (g_ProfMode == 2) {
		Bsp_PortalInfo(1);
	}
	if (g_Bsp->luces) {
		BspLuces_Render(g_Bsp->luces);
	}
}

void Bsp_RenderPortales(void) {
	if (g_MapaSombrasUsar && g_RenderMap && !g_ShowPortals) {
		Bsp_PortalInfo(0);
	}
}


/* the light map computation helpers */

/* a direction on the unit sphere: height z, angle u turns */
float *Bsp_DirEsfera(float *out, float z, float u) {
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

byte Bsp_MismoTam(const box_t *a, int axis, float scale, const box_t *b) {
	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;

	Bsp_TexelBox(a, axis, scale, &x0, &y0, &x1, &y1);
	int k = g_LightmapLimitUV ? 4 : 1;
	int w = x1 - x0;
	int h = y1 - y0;
	byte okW = w * k < g_MapaSombrasTexX;
	byte okH = h * k < g_MapaSombrasTexY;
	x0 = y0 = x1 = y1 = 0;
	Bsp_TexelBox(b, axis, scale, &x0, &y0, &x1, &y1);
	if (w == x1 - x0 && h == y1 - y0) {
		return 1;
	}
	return okW && okH;
}

void BspTri_SetExt(BspTri *tri, int x0, int y0, int x1, int y1, byte avisar) {
	char buf[0x100];

	tri->ext[2] = x1;
	tri->ext[3] = y1;
	tri->ext[1] = y0;
	tri->ext[0] = x0;
	int w = x1 - x0 + 1;
	int h = y1 - y0 + 1;
	if ((w > g_MapaSombrasTexX || h > g_MapaSombrasTexY) && avisar) {
		if (g_TXF_Helper) {
			crt_exit_now(-1);
		}
		Bprintf(buf, sizeof(buf), "EXTENS_UV: %dx%d", w, h);
		exe_MessageBoxA(NULL, buf, "Info", 0x2040);
	}
	if (w < 1) {
		w = 0;
	} else if (w >= g_MapaSombrasTexX) {
		w = g_MapaSombrasTexX;
	}
	tri->lx = w;
	if (h < 1) {
		h = 0;
	} else if (h >= g_MapaSombrasTexY) {
		h = g_MapaSombrasTexY;
	}
	tri->ly = h;
}

/* the texel of the light map at p */
byte __thiscall BspTri_TexelOf(const BspTri *tri, const vec3_t *p, int *x, int *y) {
	float uv[2];

	BspTri_Interp(tri, p, NULL, uv, 1);
	*x = (int)crt_floor((double)((float)g_MapaSombrasTexX * uv[0]));
	*y = (int)crt_floor((double)((float)g_MapaSombrasTexY * uv[1]));
	return 1;
}

#define BSP_EPS_NORMAL 9.999999747378752e-05	/* 1e-4f as double */

/* vertex nv of act is a vertex of tri with the same normal */
byte BspTri_VertIgual(const BspTri *act, uint32_t nv, const BspTri *tri) {
	int ind;

	CHECK(act);
	CHECK(tri);
	CHECK(nv < 3);
	int id = act->vid[nv];
	if (id == tri->vid[0]) {
		ind = 0;
	} else if (id == tri->vid[1]) {
		ind = 1;
	} else if (id == tri->vid[2]) {
		ind = 2;
	} else if (Vec3_Equal(&act->v[nv], &tri->v[0])) {
		ind = 0;
	} else if (Vec3_Equal(&act->v[nv], &tri->v[1])) {
		ind = 1;
	} else if (Vec3_Equal(&act->v[nv], &tri->v[2])) {
		ind = 2;
	} else {
		return 0;
	}
	CHECK(Vec3_Equal(&act->v[nv], &tri->v[ind]));
	return __builtin_fabs((double)(act->n[nv].x - tri->n[ind].x)) < BSP_EPS_NORMAL &&
		__builtin_fabs((double)(act->n[nv].y - tri->n[ind].y)) < BSP_EPS_NORMAL &&
		__builtin_fabs((double)(act->n[nv].z - tri->n[ind].z)) < BSP_EPS_NORMAL;
}

/* the color of the texture tex at the texel (u, v) of the light map, 0..1 */
float *Bsp_SampleTex(float *out, float u, float v, int axis, float scale, const BspTri *tri, const CBmp *tex) {
	vec3_t p;
	float uv[2];
	uint32_t c;

	Bsp_TexelToWorld(&p, u, v, axis, scale, &tri->plano);
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
CTabla *BspTri_GetAreas(BspTri *tri, float dist, byte grande) {
	float lod = g_BspLodLevel;

	if (!grande) {
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
	float k = tri->f178;
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
	return Bsp_FF(&a->centro, &a->plano.n, &b->centro, &b->plano.n, b->area);
}

/* add plus the light of the links into the tree */
void BspRegion_Acumular(BspRegion *r, const float *add) {
	for (; r; r = r->hijo[3]) {
		float *acum = r->acum;
		acum[0] = add[0] + acum[0];
		acum[1] = add[1] + acum[1];
		acum[2] = add[2] + acum[2];
		acum[3] = add[3] + acum[3];
		for (BspEnlace *l = r->enlaces; l; l = l->sig) {
			const BspRegion *o = l->region;
			float ff = l->ff;
			acum[0] = o->color[0] * ff + acum[0];
			acum[1] = acum[1] + o->color[1] * ff;
			acum[2] = acum[2] + o->color[2] * ff;
			acum[3] = acum[3] + o->color[3] * ff;
		}
		BspRegion_Acumular(r->hijo[0], acum);
		BspRegion_Acumular(r->hijo[1], acum);
		BspRegion_Acumular(r->hijo[2], acum);
		add = acum;
	}
}

/* the light at texel x, y into acc and count it, for a texel of the list */
void BspTri_SumTexel(const BspTri *tri, uint32_t x, uint32_t y, float *acc, int *n) {
	if (x >= (uint32_t)tri->luz.ancho || y >= (uint32_t)tri->luz.alto) {
		return;
	}
	if (!*CBmp_Pixel(&tri->mask, x, y)) {
		return;
	}
	const float *p = CBmpFlt2_Pixel(&tri->luz, x, y);
	float r = p[0], g = p[1], b = p[2];
	acc[0] = acc[0] + r;
	acc[1] = acc[1] + g;
	acc[2] = acc[2] + b;
	acc[3] = acc[3] + 1.0f;
	(*n)++;
}

/* the average light of the texels of the list around x, y (5 x 5) */
float *BspTri_Media(float *out, const BspTri *tri, int x, int y) {
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
			if (xx >= (uint32_t)tri->luz.ancho || yy >= (uint32_t)tri->luz.alto) {
				continue;
			}
			if (!*CBmp_Pixel(&tri->mask, xx, yy)) {
				continue;
			}
			const float *p = CBmpFlt2_Pixel(&tri->luz, xx, yy);
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
	lm->sig = NULL;
	lm->Mat = NULL;
	lm->lmap2 = NULL;
	lm->id = 0;
	return lm;
}

/* the alpha of a texel of a page: 0xff while free */
static inline byte BspLMap_Libre(const CBmp *bmp, uint32_t x, uint32_t y) {
	return ((const byte *)CBmp_Pixel(bmp, x, y))[3] == 0xff;
}

byte BspLMap_Buscar(const CBmp *bmp, int w, int h, int *rx, int *ry) {
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
	int a = bmp->ancho - w;
	int b = bmp->alto - h;
	int nx = (rot ? b : a) + 1;
	int ny = (rot ? a : b) + 1;
	for (int j = 0; j < ny; j += pad) {
		for (int i = 0; i < nx; i += pad) {
			int x = rot ? j : i;
			int y = rot ? i : j;
			if (!BspLMap_Libre(bmp, (uint32_t)x, (uint32_t)y)) {
				continue;
			}
			byte ok = 1;
			for (int k = 0; k < h; k++) {
				for (int m = 0; m < w; m++) {
					if (!BspLMap_Libre(bmp, (uint32_t)(x + m), (uint32_t)(y + k))) {
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
void BspLMap_Copiar(CBmp *dst, int dx, int dy, const CBmpFlt2 *orig, const CBmp *mask, uint32_t ox, uint32_t oy,
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
			float m = g_MapaSombrasModulate4x ? 0.25f : 1.0f;
			float c[4];
			c[0] = m * pr;
			c[1] = m * pg;
			c[2] = m * pb;
			float mx = Float_MaxSS(c[0], Float_MaxSS(c[1], c[2]));
			if (mx > (g_MapaSombrasModulate4x ? 1.0f : 1.2f)) {
				float k = (g_MapaSombrasModulate4x ? 1.0f : 1.2f) / mx;
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

byte BspLMap_Insertar(BspTri *list, BspLMap *lm, int w, int h) {
	int x, y;

	if (!lm) {
		return 0;
	}
	if (w >= lm->minX && h >= lm->minY) {
		return 0;
	}
	if (!BspLMap_Buscar(&lm->bmp, w, h, &x, &y)) {
		lm->minX = w;
		lm->minY = h;
		return 0;
	}
	BspLMap_Copiar(&lm->bmp, x, y, &list->luz, &list->mask, 0, 0, w, h);
	if (!lm->lmap2) {
		lm->lmap2 = BspLMap_New();
		BspLMap_Init(lm->lmap2, g_MapaSombrasTexX, g_MapaSombrasTexY);
	}
	BspLMap_Copiar(&lm->lmap2->bmp, x, y, &list->luz2, &list->mask, 0, 0, w, h);
	if (lm->info.img) {
		uint32_t color = Color_SwapRB(g_BspColores[(list->zona + 1) & 0xf]);
		CBmp tmp;
		tmp.ancho = w;
		tmp.alto = h;
		tmp.img = New_Array((uint32_t)(w * h), 4);
		CBmp_Fill(&tmp, color);
		CBmp_Copy(&lm->info, x, y, &tmp, 0, 0, w, h, 0xf);
		Img_Free(&tmp, 4);
	}
	list->LMap = lm;
	Img_Free(&list->luz, 0xc);
	Img_Free(&list->luz2, 0xc);
	float fx = (float)x, fy = (float)y;
	for (BspTri *t = list; t; t = t->sig) {
		for (int i = 0; i < 3; i++) {
			t->uvl[i][0] = fx / (float)g_MapaSombrasTexX + t->uvl[i][0];
			t->uvl[i][1] = fy / (float)g_MapaSombrasTexY + t->uvl[i][1];
		}
	}
	return 1;
}


void __fastcall CBsp_AgruparMapas(CBsp *self) {
	char nombre1[0x104], nombre2[0x104];
	CEscena *esc = self->modelo->escena;

	CHECK(esc);
	BspLMap *old = g_BspLMaps;
	if (old) {
		BspLMap_dtor(old);
		operator_delete_sized(old, sizeof(BspLMap));
		g_BspLMaps = NULL;
	}
	g_BspLMaps = BspLMap_New();
	BspLMap_Init(g_BspLMaps, g_MapaSombrasTexX, g_MapaSombrasTexY);
	BspLMap *cur = g_BspLMaps;
	while (self->num_nolmap) {
		CHECK(self->num_listas < BSP_MAX_LISTAS);
		self->num_nolmap--;
		self->listas[self->num_listas++] = self->nolmap[self->num_nolmap];
	}
	crt_qsort(self->listas, (size_t)self->num_listas, sizeof(BspTri *),
		(int (*)(const void *, const void *))Game_Ptr(0x28e520));
	for (int i = 0; i < self->num_listas; i++) {
		BspTri *list = self->listas[i];
		E3D_DebugPrintf("agrupar mapas:  %d / %d   mem: %d Kb  \r", i + 1, self->num_listas, g_MemStats->bytes / 1024);
		if (list->flags & BSPTRI_NOLMAP) {
			CHECK(list->LMap == NULL);
			Img_Free(&list->luz, 0xc);
			Img_Free(&list->luz2, 0xc);
			continue;
		}
		BspTri_Rotar(list);
		int w = list->lx, h = list->ly;
		/* another material or sector starts on the last page */
		if (i > 0) {
			const BspTri *prev = self->listas[i - 1];
			if (prev->Mat != list->Mat || prev->zona != list->zona) {
				while (cur->sig) {
					cur = cur->sig;
				}
			}
		}
		for (BspLMap *lm = cur; lm; lm = lm->sig) {
			if (BspLMap_Insertar(list, lm, w, h)) {
				break;
			}
			if (!lm->sig) {
				lm->sig = BspLMap_New();
				BspLMap_Init(lm->sig, g_MapaSombrasTexX, g_MapaSombrasTexY);
			}
		}
	}
	E3D_DebugPrintf("\n");
	int k = 0;
	for (BspLMap *lm = g_BspLMaps; lm; lm = lm->sig) {
		BspLMap *l2 = lm->lmap2;
		lm->id = ++k;
		l2->id = k;
		BspLMap_Guardar(nombre1, &lm->bmp, esc->name, k, ".LMAP1.tga");
		BspLMap_Guardar(nombre2, &l2->bmp, esc->name, k, ".LMAP2.tga");
		if (lm->info.img) {
			CBmp_Fill(&l2->info, Color_SwapRB(g_BspColores[1 + (k - 1) % 15]));
			BspLMap_Guardar(NULL, &lm->info, esc->name, k, ".LMAP1.INFO.tga");
			BspLMap_Guardar(NULL, &l2->info, esc->name, k, ".LMAP2.INFO.tga");
		}
		Img_Free(&lm->bmp, 4);
		Img_Free(&l2->bmp, 4);
		Img_Free(&lm->info, 4);
		Img_Free(&l2->info, 4);
		BspLMap_CrearMat(lm, nombre1, 1);
		BspLMap_CrearMat(l2, nombre2, 2);
	}
	crt_qsort(self->listas, (size_t)self->num_listas, sizeof(BspTri *),
		(int (*)(const void *, const void *))Game_Ptr(0x28e520));
}


/* the lists of the map */

void __fastcall CBsp_CrearZonas(CBsp *self) {
	int n = Sector_Num() + 1;

	crt_qsort(self->listas, (size_t)self->num_listas, sizeof(BspTri *),
		(int (*)(const void *, const void *))Game_Ptr(0x28e520));
	Tabla_NewB4(&self->zonas, n);
	CHECK(self->zonas.p);
	memset(self->zonas.p, 0, (size_t)self->zonas.n * sizeof(BspZona));
	E3D_DebugPrintf("listas: %d\n", self->num_listas);
	E3D_DebugPrintf("zonas: %d\n", n);
	for (int i = 0; i < self->num_listas; i++) {
		BspTri *tri = self->listas[i];
		BspZona *z = CBsp_Zona(self, tri->zona);
		BspTri *t = z->tris;
		CHECK(!tri->zona_sig);
		if (!t) {
			CBsp_Zona(self, tri->zona)->tris = tri;
		} else {
			while (t->zona_sig) {
				t = t->zona_sig;
			}
			t->zona_sig = tri;
		}
	}
}

/* the transparent lists without a diffuse map take the color of the material and no light map */
void __fastcall CBsp_ColorSinLmap(CBsp *self) {
	for (int i = 0; i < self->num_listas; i++) {
		BspTri *list = self->listas[i];
		const CMaterial *m = list->Mat;
		if (!Mat_IsTransp(m) || m->mapa[MAPA_DIFFUSE]) {
			continue;
		}
		const byte *d = m->params.diffuse;
		uint32_t c = (uint32_t)d[3] << 24 | (uint32_t)d[0] << 16 | (uint32_t)d[1] << 8 | d[2];
		if (g_DiffuseMod4x) {
			c = (c & 0xff000000) | (c >> 2 & 0x3f3f3f);
		}
		for (BspTri *t = list; t; t = t->sig) {
			t->color[0] = c;
			t->color[1] = c;
			t->color[2] = c;
		}
		list->flags |= BSPTRI_NOLMAP;
	}
}

/* the lists of the area lights out of the map */
void __fastcall CBsp_QuitarLuces(CBsp *self) {
	int i = 0;

	while (i < self->num_listas) {
		BspTri *tri = self->listas[i];
		if (!(tri->nodo->flags >> 7 & 1)) {
			i++;
			continue;
		}
		CHECK(tri->radluz_ok || !g_MapaSombrasRadiosidad);
		CHECK(tri->nodo->flags >> 29 & 1);
		tri = self->listas[i];
		if (tri) {
			BspTri_dtor(tri);
			operator_delete_sized(tri, sizeof(BspTri));
			self->listas[i] = NULL;
		}
		self->listas[i] = self->listas[self->num_listas - 1];
		self->num_listas--;
	}
}

/* the areas of src averaged in boxes of dim x dim texels over w x h into dst */
void BspTri_AreasLod(CTabla *dst, const CTabla *src, int w, int h, int dim) {
	CTabla tmp;
	int n = 0;

	Tabla_Free(dst);
	if (!src->n) {
		return;
	}
	tmp.p = NULL;
	tmp.n = 0;
	Tabla_Alloc18(&tmp, src->n);
	for (int yb = 0; yb < h; yb += dim) {
		for (int xb = 0; xb < w; xb += dim) {
			float sx = 0.0f, sy = 0.0f, sz = 0.0f, peso = 0.0f, f14 = 0.0f;
			uint16_t ux = 0, uy = 0;
			int cnt = 0;
			for (int i = 0; i < src->n; i++) {
				TABLA_CHECK(src->p, i, src->n);
				const BspArea *a = &((const BspArea *)src->p)[i];
				if (a->x >= xb && a->x < dim + xb && a->y >= yb && a->y < dim + yb) {
					sx = sx + a->pos.x;
					sy = sy + a->pos.y;
					sz = sz + a->pos.z;
					peso = peso + a->peso;
					f14 = f14 + a->f14;
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
			TABLA_CHECK(tmp.p, n, tmp.n);
			BspArea *o = &((BspArea *)tmp.p)[n++];
			o->pos.x = sx / fc;
			o->pos.y = sy / fc;
			o->pos.z = sz / fc;
			o->peso = peso;
			o->x = (uint16_t)((int)ux / cnt);
			o->y = (uint16_t)((int)uy / cnt);
			o->f14 = f14 / fc;
		}
	}
	if (n) {
		Tabla_Alloc18(dst, n);
		for (int i = 0; i < n; i++) {
			TABLA_CHECK(tmp.p, i, tmp.n);
			TABLA_CHECK(dst->p, i, dst->n);
			((BspArea *)dst->p)[i] = ((const BspArea *)tmp.p)[i];
		}
	}
	if (tmp.p) {
		operator_delete_array(tmp.p);
	}
}


/* the region trees of the radiosity */

/* the region of the texels (x, y) size lx x ly with the areas in it; NULL without any */
BspRegion *BspRegion_New(int x, int y, int lx, int ly, BspRegion *padre, const CTabla *areas) {
	if (lx <= 0 || ly <= 0) {
		return NULL;
	}
	BspRegion *r = operator_new(sizeof(BspRegion));
	if (r) {
		r = BspRegion_ctor(r, x, y, lx, ly, padre);
	}
	int n = 0;
	r->centro = (vec3_t){ 0.0f, 0.0f, 0.0f };
	r->area = 0.0f;
	for (int i = 0; i < areas->n; i++) {
		TABLA_CHECK(areas->p, i, areas->n);
		const BspArea *a = &((const BspArea *)areas->p)[i];
		if (a->x >= x && a->x < x + lx && a->y >= y && a->y < y + ly) {
			float e = g_MapaSombrasEscala;
			r->area = a->peso / (e * e) + r->area;
			r->centro.x = r->centro.x + a->pos.x;
			r->centro.y = a->pos.y + r->centro.y;
			r->centro.z = a->pos.z + r->centro.z;
			n++;
		}
	}
	if (n) {
		float fn = (float)n;
		r->centro.x = r->centro.x / fn;
		r->centro.y = r->centro.y / fn;
		r->centro.z = r->centro.z / fn;
		return r;
	}
	BspRegion_dtor(r);
	operator_delete_sized(r, sizeof(BspRegion));
	return NULL;
}

void BspRegion_Dividir(BspRegion *r, const CTabla *areas) {
	while (r) {
		uint32_t lx = r->lx;
		if (lx <= 1 && r->ly <= 1) {
			r->interior = 0;
			Debug_Punto(&r->centro, 0xffff00, 1.0f);
			return;
		}
		uint32_t ly = r->ly;
		int hx = (int)(lx >> 1);
		int hy = (int)(ly >> 1);
		int a = (int)lx - hx;
		int b = (int)ly - hy;
		r->hijo[0] = BspRegion_New(r->x, r->y, a, b, r, areas);
		r->hijo[1] = BspRegion_New(r->x + a, r->y, hx, b, r, areas);
		r->hijo[2] = BspRegion_New(r->x, r->y + b, a, hy, r, areas);
		r->hijo[3] = BspRegion_New(r->x + a, r->y + b, hx, hy, r, areas);
		r->interior = r->hijo[0] || r->hijo[1] || r->hijo[2] || r->hijo[3];
		BspRegion_Dividir(r->hijo[0], areas);
		BspRegion_Dividir(r->hijo[1], areas);
		BspRegion_Dividir(r->hijo[2], areas);
		r = r->hijo[3];
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
		t[0] = mc[0] * r->acum[0];
		t[1] = r->acum[1] * mc[1];
		t[2] = r->acum[2] * mc[2];
		t[3] = r->acum[3] * mc[3];
		memcpy(r->acum, t, sizeof(t));
		CHECK(r->lx == 1);
		CHECK(r->ly == 1);
		for (int j = 0; j < r->ly; j++) {
			for (int i = 0; i < r->lx; i++) {
				int x = r->x + i;
				int y = r->y + j;
				if (x >= tri->rad.ancho || y >= tri->rad.alto) {
					continue;
				}
				if (Color_SwapRB(*CBmp_Pixel(&tri->mask, (uint32_t)x, (uint32_t)y)) != color) {
					continue;
				}
				float *p = CBmpFlt2_Pixel(&tri->rad, (uint32_t)x, (uint32_t)y);
				p[0] = r->acum[0] * 0.6f + p[0];
				p[1] = r->acum[1] * 0.6f + p[1];
				p[2] = r->acum[2] * 0.6f + p[2];
			}
		}
		for (int i = 0; i < 4; i++) {
			r->color[i] = r->acum[i] * 0.4f;
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
			const float *s = BspRegion_Color(t, r->hijo[k], tri, color);
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
	r->acum[0] = 0.0f;
	r->acum[1] = 0.0f;
	r->acum[2] = 0.0f;
	r->acum[3] = 1.0f;
	for (int i = 0; i < 4; i++) {
		out[i] = r->color[i] * area;
	}
	return out;
}

/* a list taller than wide is turned for the packing (pack mode 2 and up) */
void BspTri_Rotar(BspTri *list) {
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
	CBmpFlt2_Resample(&t, &list->luz, 0xf);
	CBmpFlt2_Rotate(&list->luz, &t);
	if (list->luz2.img) {
		CBmpFlt2_Assign(&t, &list->luz2);
		CBmpFlt2_Rotate(&list->luz2, &t);
	}
	Img_Free(&m, 4);
	CBmp_Resample(&m, &list->mask, 0xf);
	CBmp_Rotate(&list->mask, &m);
	if (list->mask2.img) {
		CBmp_Assign(&m, &list->mask2);
		CBmp_Rotate(&list->mask2, &m);
	}
	float fw = (float)w;
	for (BspTri *tri = list; tri; tri = tri->sig) {
		for (int i = 0; i < 3; i++) {
			float u = tri->uvl[i][0];
			tri->uvl[i][0] = tri->uvl[i][1];
			tri->uvl[i][1] = fw / (float)g_MapaSombrasTexX - u;
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
	if (a->zona != b->zona) {
		return a->zona - b->zona;
	}
	return 0;
}

int BspTri_CmpZona(const void *pa, const void *pb) {
	const BspTri *a = *(BspTri *const *)pa;
	const BspTri *b = *(BspTri *const *)pb;

	if (a->zona != b->zona) {
		return a->zona - b->zona;
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

#define g_BspDebugCnt GAME_DATA(int, 0x4d5d74)
#define g_BspDebugSel GAME_DATA(int, 0x432994)		/* the region drawn without links */
/* the collision cache counters of suelo.cpp: hit, miss, peak */
#define g_BspCacheStats ((const int *)Game_Ptr(0x4af7e0))

void Bsp_MatInfo(CMaterial *m) {
	uint32_t atrib = m->params.atrib;
	const char *transp = Mat_IsTransp(m) ? "Transp  " : "";
	const char *g = m->mapa[4] ? "G" : "";
	const char *b = m->mapa[3] ? "B" : "";
	const char *e = m->mapa[2] ? "E" : "";
	const char *o = m->mapa[1] ? "M" : "";
	const char *d = m->mapa[0] ? "D" : "";
	const char *glow = CMaterial_GetMapName(m, MAPA_GLOW);
	const char *dif = CMaterial_GetMapName(m, MAPA_DIFFUSE);

	Con_Verbosef("  %-20s  D:[%s]  G:[%s]  %s%s%s%s%s  %s  %04X\n", m->Nombre, dif, glow, d, o, e, b, g, transp, atrib);
}

/* the boxes of the regions with links (and the selected one) and their links; the regions drawn */
int BspRegion_Debug(BspRegion *r) {
	if (!r) {
		return 0;
	}
	int n = BspRegion_Debug(r->hijo[0]);
	n += BspRegion_Debug(r->hijo[1]);
	n += BspRegion_Debug(r->hijo[2]);
	n += BspRegion_Debug(r->hijo[3]);
	if (++g_BspDebugCnt != g_BspDebugSel && !n) {
		return 0;
	}
	float s = g_MapaSombrasEscala * g_MapaSombrasResol;
	float hx = (float)r->lx / s * 0.5f;
	float hy = (float)r->ly / s * 0.5f;
	vec3_t mx = { r->centro.x + hx, r->centro.y + hy, r->centro.z + 0.0f };
	vec3_t mn = { r->centro.x - hx, r->centro.y - hy, r->centro.z - 0.0f };
	Debug_DrawBox(&mn, &mx, n ? 0xc0c0c0 : 0xffffff);
	for (BspEnlace *l = r->enlaces; l; l = l->sig) {
		Debug_DrawLine(&r->centro, &l->region->centro, g_BspColores[n % 15 + 1]);
	}
	Texto_PrintfRel(0, 0x10, "%d:  px py: %d %d  lx ly: %d %d", n, r->x, r->y, r->lx, r->ly);
	return n + 1;
}

/* the material of a page from its file (nivel: the light map level of the map) */
void __thiscall BspLMap_CrearMat(BspLMap *self, const char *name, byte nivel) {
	CMaterial m;

	CMaterial_ctor(&m);
	Cad_Set(&m.Nombre, Ruta_FileName(name));
	CMaterial_SetMap(&m, MAPA_DIFFUSE, name);
	m.params.b39 = 0;
	m.mapa[MAPA_DIFFUSE]->lightmap = nivel;
	((byte *)&m.params.w3e)[1] = 0;
	if (!g_MapaSombrasMultitext) {
		m.params.dstBlend = 3;
		m.params.transp = 1;
		m.params.srcBlend = (g_Gamma2x == 0) * 8 + 1;
	}
	CHECK(!self->Mat);
	self->Mat = Mat_New(&m, 1);
	CMaterial_dtor(&m);
}

/* bmp saved as "<ruta>.%04d<ext>" (overwriting); the file name into copia */
void BspLMap_Guardar(char *copia, CBmp *bmp, const char *ruta, int n, const char *ext) {
	char buf[0x104];

	Bprintf(buf, sizeof(buf), "&u&d&n.%04d%s", n, ext);
	Ruta_Format(buf, ruta, buf);
	CBmp_Save(bmp, buf, IMG_OVERWRITE);
	if (copia) {
		memcpy(copia, buf, strlen(buf) + 1);
	}
}

/* the glow of the texel of each area (rad2) and the coarser levels of the areas */
void __fastcall CBsp_AreasGlow(CBsp *self) {
	for (int i = 0; i < self->num_listas; i++) {
		BspTri *list = self->listas[i];
		for (BspTri *t = list; t; t = t->sig) {
			CTabla *a0 = &t->areas[0];
			for (int k = 0; k < a0->n; k++) {
				TABLA_CHECK(a0->p, k, a0->n);
				BspArea *a = &((BspArea *)a0->p)[k];
				const float *p = CBmpFlt2_Pixel(&list->rad2, a->x, a->y);
				float r = p[0], g = p[1], b = p[2];
				a->f14 = (g * 0.05f + r * 0.05f + b * 0.05f) / 3.0f;
			}
			BspTri_AreasLod(&t->areas[1], a0, list->mask.ancho, list->mask.alto, 2);
			BspTri_AreasLod(&t->areas[2], a0, list->mask.ancho, list->mask.alto, 4);
			BspTri_AreasLod(&t->areas[3], a0, list->mask.ancho, list->mask.alto, 8);
		}
	}
}

/* the sum and the largest channel of the glow at the areas, by triangle and by list */
void __fastcall CBsp_AreasMax(CBsp *self) {
	for (int i = 0; i < self->num_listas; i++) {
		BspTri *list = self->listas[i];
		list->f168[0] = 0.0f;
		list->f168[1] = 0.0f;
		list->f168[2] = 0.0f;
		list->f168[3] = 1.0f;
		list->i180 = 0;
		for (BspTri *t = list; t; t = t->sig) {
			t->i17c = 0;
			t->f158[0] = 0.0f;
			t->f158[1] = 0.0f;
			t->f158[2] = 0.0f;
			t->f158[3] = 1.0f;
			t->f178 = 0.0f;
			for (int k = 0; k < t->areas[0].n; k++) {
				TABLA_CHECK(t->areas[0].p, k, t->areas[0].n);
				const BspArea *a = &((const BspArea *)t->areas[0].p)[k];
				const float *p = CBmpFlt2_Pixel(&list->rad2, a->x, a->y);
				float r = p[0], g = p[1], b = p[2];
				t->f158[0] = t->f158[0] + r;
				t->f158[1] = t->f158[1] + g;
				float m = Float_MaxSS(b, Float_MaxSS(g, r));
				t->f158[2] = t->f158[2] + b;
				t->f158[3] = t->f158[3] + 1.0f;
				t->i17c++;
				t->f178 = Float_MaxSS(t->f178, m);
			}
			list->f168[0] = list->f168[0] + t->f158[0];
			list->f168[1] = t->f158[1] + list->f168[1];
			list->f168[2] = t->f158[2] + list->f168[2];
			list->f168[3] = t->f158[3] + list->f168[3];
			list->i180 += t->i17c;
		}
	}
}

/* n passes of the light through the region links: gathered into the trees, then into the light maps */
void __thiscall CBsp_IlumRegiones(CBsp *self, int n) {
	float out[4];

	for (int pass = 1; pass <= n; pass++) {
		E3D_DebugPrintf("iluminacion_regiones (%d):\n", pass);
		int nl = self->num_listas;
		for (int i = 0; i < nl; nl = self->num_listas) {
			BspTri *list = self->listas[i];
			E3D_DebugPrintf("region: %d / %d   \r", ++i, nl);
			if (list) {
				float add[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
				for (BspTri *t = list; t; t = t->sig) {
					BspRegion_Acumular(t->region, add);
				}
			}
		}
		nl = self->num_listas;
		for (int i = 0; i < nl; nl = self->num_listas) {
			BspTri *list = self->listas[i];
			E3D_DebugPrintf("region: %d / %d   \r", ++i, nl);
			for (BspTri *t = list; t; t = t->sig) {
				BspRegion_Color(out, t->region, list, (uint32_t)(uintptr_t)t);
			}
		}
	}
}

static inline void Vec3_Acum(vec3_t *s, const vec3_t *p) {
	s->x = s->x + p->x;
	s->y = s->y + p->y;
	s->z = p->z + s->z;
}

/* a triangle of list other than tri has p */
static byte BspTri_OtroTiene(const BspTri *tri, const BspTri *list, const vec3_t *p) {
	for (const BspTri *o = list; o; o = o->sig) {
		if (o != tri && Suelo_PointInTri(p, &o->v[0], &o->v[1], &o->v[2])) {
			return 1;
		}
	}
	return 0;
}

/* the world point of the texel x, y of tri: without peso the center (else the average of the corners inside tri),
 * with peso the average of the 7 x 7 samples inside the list and their share in peso */
vec3_t *BspTri_Muestra(vec3_t *out, int x, int y, int axis, float scale, const BspTri *tri, const BspTri *list,
	float *peso) {
	vec3_t p;

	if (!peso) {
		vec3_t c;
		Bsp_TexelToWorld(&c, (float)x + 0.5f, (float)y + 0.5f, axis, scale, &tri->plano);
		if (Suelo_PointInTri(&c, &tri->v[0], &tri->v[1], &tri->v[2]) || BspTri_OtroTiene(tri, list, &c)) {
			*out = c;
			return out;
		}
		vec3_t q[4];
		float fx = (float)x, fy = (float)y;
		float x0 = fx + 0.0f, y0 = fy + 0.0f;
		Bsp_TexelToWorld(&q[0], x0, y0, axis, scale, &tri->plano);
		float x1 = fx + 1.0f;
		Bsp_TexelToWorld(&q[1], x1, y0, axis, scale, &tri->plano);
		float y1 = fy + 1.0f;
		Bsp_TexelToWorld(&q[2], x0, y1, axis, scale, &tri->plano);
		Bsp_TexelToWorld(&q[3], x1, y1, axis, scale, &tri->plano);
		vec3_t s = { 0.0f, 0.0f, 0.0f };
		int n = 0;
		for (int i = 0; i < 4; i++) {
			if (Suelo_PointInTri(&q[i], &tri->v[0], &tri->v[1], &tri->v[2])) {
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
			Bsp_TexelToWorld(&p, u, (float)j / 6.0f + (float)y, axis, scale, &tri->plano);
			if (Suelo_PointInTri(&p, &tri->v[0], &tri->v[1], &tri->v[2]) || BspTri_OtroTiene(tri, list, &p)) {
				Vec3_Acum(&s, &p);
				n++;
			}
		}
	}
	if (!n) {
		*peso = 0.0f;
		Bsp_TexelToWorld(out, (float)x + 0.5f, (float)y + 0.5f, axis, scale, &tri->plano);
		return out;
	}
	float fn = (float)n;
	*peso = fn / 49.0f;
	out->x = s.x / fn;
	out->y = s.y / fn;
	out->z = s.z / fn;
	return out;
}

/* the TXF of list i (nl: a line feed after the messages): 1 when it is to be computed here, 0 when it exists or
 * another computer computes it (locked), def without the cache. ESC with SHIFT asks to cancel the compilation */
byte __thiscall BspTxf_Check(BspTxf *self, int i, byte def, byte nl) {
	char buf[0x104];
	const char *lf = nl ? "\n" : "";

	if (exe_GetAsyncKeyState(0x1b) < 0 && exe_GetAsyncKeyState(0x10) < 0 &&
		exe_MessageBoxA(NULL, "Cancel map compilation?", "Message", 0x2034) == 6) {
		crt_exit(-1);
	}
	if (!self->on) {
		return def;
	}
	Bprintf(self->nombre, sizeof(self->nombre), "%s.%05d.TXF", self->base, i + 1);
	if (File_Exists(self->nombre)) {
		self->skipped++;
		E3D_DebugPrintf("Skipping...   %s", lf);
		return 0;
	}
	Ruta_Format(buf, self->nombre, "&n");
	const char *s = strrchr(buf, '.') ? strrchr(buf, '.') + 1 : buf;
	E3D_DebugPrintf("%s   ", s);
	CHECK(!self->FileTXF);
	self->FileTXF = crt_fsopen(self->nombre, "wb", 0x10);
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
void __thiscall BspTxf_Guardar(BspTxf *self, CBmpFlt2 *bmp) {
	if (!self->on) {
		return;
	}
	if (self->FileTXF) {
		crt_fclose(self->FileTXF);
	}
	self->FileTXF = NULL;
	if (CBmpFlt2_Save(bmp, self->nombre, TXF_OVERWRITE, self->equipo, sizeof(self->equipo))) {
		/* differs from the original on purpose (no sleeps): the original tried again 10 seconds later, up to 100
		 * times for each map; here BspTxf_Cargar finds the TXF missing or unreadable and the rebuild computes it
		 * again */
		CHECK(self->writeRetries < 100);
		self->writeRetries++;
		E3D_LogPrint(g_FileLog, "Retry %d of %d", self->writeRetries, 100);
		return;
	}
	self->written++;
}

/* the light maps (cual 1 luz, 2 luz2, 3 rad, 4 rad2) of n lists from the TXF cache; a missing one makes the map
 * be computed again (rebuild), a bad one is magenta */
void __thiscall BspTxf_Cargar(BspTxf *self, BspTri **listas, int n, int cual) {
	static const float magenta[3] = { 1.0f, 0.0f, 1.0f };

	if (!self->on) {
		return;
	}
	E3D_DebugPrintf("\n");
	for (int i = 0, k = 1; i < n; i++, k++) {
		BspTri *tri = listas[i];
		CBmpFlt2 *bmp = NULL;
		switch (cual) {
		case 1:
			bmp = &tri->luz;
			break;
		case 2:
			bmp = &tri->luz2;
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
		Bprintf(self->nombre, sizeof(self->nombre), "%s.%05d.TXF", self->base, k);
		char info[0x20];
		memset(info, 0, sizeof(info));
		if (!g_TxfMode && !File_Exists(self->nombre)) {
			E3D_DebugPrintf("TXF not found: %s   rebuilding...   \n", Ruta_FileName(self->nombre));
			self->rebuild = 1;
			break;
		}
		if (!CBmpFlt2_Load(bmp, self->nombre, 0, info, sizeof(info))) {
			if (bmp->ancho != tri->lx || bmp->alto != tri->ly) {
				E3D_DebugPrintf("Bad TXF: %s  [%s]   \n", Ruta_FileName(self->nombre), info);
				E3D_LogBlue(g_FileLog, "Bad TXF: %s  [%s]  size: %dx%d  required: %dx%d", Ruta_FileName(self->nombre),
					info, bmp->ancho, bmp->alto, tri->lx, tri->ly);
				CHECK(g_TxfMode);
				CBmpFlt2_CreateFill(bmp, tri->lx, tri->ly, magenta);
				if (g_TxfMode == 2) {
					crt_remove(self->nombre);
				}
			}
			E3D_DebugPrintf("Reloading TXF:  %d / %d  %s  [%s]   \r", k, n, Ruta_FileName(self->nombre), info);
			continue;
		}
		CBmpFlt2_CreateFill(bmp, tri->lx, tri->ly, magenta);
		if (g_TxfMode) {
			E3D_DebugPrintf("TXF not found: %s   \n", Ruta_FileName(self->nombre));
			E3D_LogBlue(g_FileLog, "TXF not found: %s", Ruta_FileName(self->nombre));
			if (g_TxfMode == 2 && File_Exists(self->nombre)) {
				crt_remove(self->nombre);
			}
			continue;
		}
		int32_t t = crt_time32(NULL);
		if (t - File_Time(self->nombre) > 0x708) {
			E3D_DebugPrintf("Deleting file '%s'...   \n", Ruta_FileName(self->nombre));
			crt_remove(self->nombre);
			self->deleted++;
			self->rebuild = 1;
			continue;
		}
		/* differs from the original on purpose (no sleeps): the original waited 30 seconds and read it again (another
		 * computer still computes it, or ESC with SHIFT deleted it), here it is computed again like an old one */
		self->readRetries++;
		E3D_DebugPrintf("Deleting file '%s'...   \n", Ruta_FileName(self->nombre));
		crt_remove(self->nombre);
		self->rebuild = 1;
	}
	BspTxf_Info(self);
}

/* the end of the compilation: the time and the collision cache counters; yes exits the game */
void __thiscall Bsp_FinMensaje(CBsp *self, int secs) {
	char buf[0x400];

	Bprintf(buf, sizeof(buf),
		"Map compilation finished in %dh %02dm %02ds\n\npath: %s\n\nhit: %d  miss: %d  peak: %d\n\nExit now?",
		secs / 3600, secs / 60 % 60, secs % 60, self->nombre, g_BspCacheStats[0], g_BspCacheStats[1],
		g_BspCacheStats[2]);
	if (g_TxfMode) {
		Bprintf(buf, sizeof(buf), "TFX loading finished (%d)\n\n%s\n\npath: %s\n\nExit now?", g_TxfMode,
			"written .EMI .TGA and .INI files", self->nombre);
	}
	if (exe_MessageBoxA(NULL, buf, "Message", 0x2024) == 6) {
		crt_fflush(NULL);
		crt_exit_now(-1);
	}
}


/* the map: made from the floor of the world */

#define g_LmapGuardarIni GAME_DATA(int, 0x43298c)	/* the settings back into the .ini of the map */
#define g_MapaSombrasDlgCalc ((void *)Game_Ptr(0x282e60))	/* Bsp_DlgProc */

typedef int(__stdcall *isdlgbuttonchecked_t)(void *hwnd, int id);
typedef int(__stdcall *checkdlgbutton_t)(void *hwnd, int id, unsigned check);
typedef int(__stdcall *enablewindow_t)(void *hwnd, int enable);
#define exe_IsDlgButtonChecked GAME_DATA(isdlgbuttonchecked_t, 0x3a13ac)
#define exe_CheckDlgButton GAME_DATA(checkdlgbutton_t, 0x3a13a8)
#define exe_EnableWindow GAME_DATA(enablewindow_t, 0x3a1374)

/* the obj3D of a node as its class (the inline cast of objeto3D.h, NULL with a failed CHECK) */
static void *Obj3D_As(CNodo3D *n, uint32_t clase) {
	IObjeto3D *o = n->obj3D;

	if (o->clase != clase) {
		CheckFailed("devNULL", "objeto3D.h", __TIMESTAMP__, __LINE__);
		return NULL;
	}
	return o;
}

CBsp *__thiscall CBsp_ctor(CBsp *self, const char *nombre, const vec3_t *escala) {
	char buf[0x104];

	self->nombre = NULL;
	self->zonas.p = NULL;
	self->zonas.n = 0;
	Cad_Set(&self->nombre, nombre);
	self->modelo = NULL;
	memset(self->listas, 0, sizeof(self->listas));
	self->num_listas = 0;
	memset(self->nolmap, 0, sizeof(self->nolmap));
	self->num_nolmap = 0;
	self->nodos = NULL;
	self->luces = NULL;
	self->matFX = NULL;
	self->matWire = NULL;
	int n = Sector_Num();
	g_BspSecVisto = 0;
	g_PortalesOn = n > 0;
	g_BspOjoAnt = (vec3_t){ 0.0f, 0.0f, 0.0f };
	if (g_LmapInfo) {
		Tex_AddExtension("<U><D><N>.INFO<E>");
	}
	if (!g_MapaSombrasCalcular) {
		if (CBsp_LeerEMI(self)) {
			Con_FatalErrorf("No hay iluminaci\xf3n precalculada para:\n%s", self->nombre);
		}
	} else {
		self->modelo = Crear_Modelo3(nombre, NULL, escala, 4);
		self->modelo->dynLight = 0;
		self->modelo->alwaysVisible = 1;
		CBsp_Calcular(self);
	}
	CBsp_CrearZonas(self);
	CBsp_Preparar(self);
	if (!self->modelo) {
		if (Mundo_Get()->EscSuelo->fromAmc) {
			Ruta_Format(buf, nombre, "&U&D&N_2._SM3_");
			self->modelo = Crear_Modelo3(buf, NULL, escala, 0x14);
		} else if (!self->modelo) {
			self->modelo = Crear_Modelo3(nombre, NULL, escala, 4);
		}
	}
	self->modelo->dynLight = 0;
	self->modelo->alwaysVisible = 1;
	return self;
}

/* the node class of the log lines */
static const char *Bsp_NodoClase(CNodo3D *n) {
	return Obj3D_ClaseName(CNodo3D_Clase(n));
}

/* the nodes of a scene into the file log; done: the ones out of the map (both flags 0x20008) too */
static void Bsp_LogNodos(CEscena *esc, const char *fmt, byte done) {
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		uint32_t f = n->flags;
		if ((f & 0x20008) == 0x20008) {
			if (done) {
				E3D_LogInfo(g_FileLog, "Map node: %s  [c:%s]", n->name, Bsp_NodoClase(n));
			}
			continue;
		}
		const char *cls = Bsp_NodoClase(n);
		E3D_LogWhite(g_FileLog, fmt, n->name, f >> 17 & 1 ? "" : "  [TRANS]", f >> 3 & 1 ? "" : "  [RENDER]", cls);
	}
}

void Bsp_Crear(void) {
	char buf[0x104];

	if (!g_MapaSombrasUsar) {
		return;
	}
	if (g_MapaSombrasDlg) {
		void *hwnd = MOTOR_HWND(g_Motor);
		void *inst = (void *)exe_GetWindowLongA(hwnd, -6);
		if (!exe_DialogBoxParamA(inst, (const char *)0xa2, hwnd, g_MapaSombrasDlgCalc, 0)) {
			g_MapaSombrasUsar = 0;
			return;
		}
	}
	if (!g_MapaSombrasUsar) {
		return;
	}
	CMundo *mundo = Mundo_Get();
	CHECK(mundo->Suelo);
	CBsp *bsp = operator_new(sizeof(CBsp));
	g_Bsp = bsp ? CBsp_ctor(bsp, mundo->EscSuelo->name, &mundo->Suelo->escala) : NULL;
	CSuelo_FreeNodos(mundo->ObjSuelo);
	/* the world keeps no lights and no meshes: they are in the map now */
	for (CNodo3D *n = mundo->EscSuelo->Nodo3D; n; n = n->sig) {
		if (CNodo3D_Clase(n) == CLASE_OBJ3D_LUZ) {
			CLuz *luz = Obj3D_As(n, CLASE_OBJ3D_LUZ);
			if (n->flags & 0x6000000) {
				continue;
			}
			CHECK(!luz->activa);
			n->flags |= 0x20008;
			Bsp_NodoMarcar(n, 1);
			E3D_LogWhite(g_FileLog, "World: Light object DELETED: %s", n->name);
			continue;
		}
		if (CNodo3D_Clase(n) != CLASE_OBJ3D_SUELO && CNodo3D_Clase(n) != CLASE_OBJ3D_PORTAL &&
			CNodo3D_Clase(n) != CLASE_OBJ3D_CAMARA) {
			Bsp_NodoMarcar(n, 1);
			n->flags |= 0x20008;
		}
		Bsp_NodoMarcar(n, 0);
		if (CNodo3D_Clase(n) == CLASE_OBJ3D_PORTAL) {
			n->flags |= 0x20008;
		}
	}
	CEscena_CompactMats(mundo->EscSuelo);
	CEscena_DeleteEmpty(mundo->EscSuelo, 0, 0, 1);
	/* the map keeps only its meshes */
	for (CNodo3D *n = g_Bsp->modelo->escena->Nodo3D; n; n = n->sig) {
		if (Mundo_Get()->EscSuelo->fromAmc) {
			break;
		}
		CHECK(CNodo3D_Clase(n) != CLASE_OBJ3D_MALLAD3D);
		if (CNodo3D_Clase(n) == CLASE_OBJ3D_MALLATRI) {
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
				CNodo3D_BuildD3D(n, 2);
				CEscena *esc = Mundo_Get()->EscSuelo;
				CMallaD3D_CalcIlum(Obj3D_As(n, CLASE_OBJ3D_MALLAD3D), n, esc);
				Con_Warnf("MAP NODE: NODO_RT_LIGHTING: %s\n", n->name);
			} else {
				Bsp_NodoMarcar(n, 1);
				n->flags |= 8;
			}
		}
		if (CNodo3D_Clase(n) == CLASE_OBJ3D_LUZ || CNodo3D_Clase(n) == CLASE_OBJ3D_CAMARA ||
			CNodo3D_Clase(n) == CLASE_OBJ3D_PORTAL || !CNodo3D_Clase(n)) {
			Bsp_NodoMarcar(n, 1);
			n->flags |= 8;
		}
	}
	CEscena_CompactMats(g_Bsp->modelo->escena);
	CEscena_DeleteEmpty(g_Bsp->modelo->escena, 0, 0, 1);
	Bsp_LogNodos(mundo->EscSuelo, "World node: %s%s%s  [c:%s]", 0);
	Bsp_LogNodos(g_Bsp->modelo->escena, "Map node: %s%s%s  [c:%s]", 1);
	CEscena *mesc = g_Bsp->modelo->escena;
	E3D_LogInfo(g_FileLog, "Total: %d world nodes, %d map nodes", CEscena_NumNodos(mundo->EscSuelo),
		CEscena_NumNodos(mesc));
	if (!Mundo_Get()->EscSuelo->fromAmc) {
		Ruta_Format(buf, g_Bsp->nombre, "&U&D&N_2.SM3");
		SM3_Save(buf, g_Bsp->modelo->escena);
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

/* the checkboxes of the dialog and their switches */
static const struct {
	int id;
	uint32_t var;
} g_BspDlgChecks[] = {
	{ 0x40a, 0x4b2af8 }, { 0x409, 0x4b2afc }, { 0x40d, 0x4b2b58 }, { 0x40e, 0x4b2b5c }, { 0x411, 0x43298c },
	{ 0x414, 0x4b2b04 }, { 0x417, 0x4b2b10 }, { 0x41a, 0x4b2b14 }, { 0x418, 0x4b2b18 }, { 0x41b, 0x4b2b1c },
	{ 0x420, 0x4b2b20 }, { 0x419, 0x4b2b24 },
};

intptr_t __stdcall Bsp_DlgProc(void *hwnd, uint32_t msg, uintptr_t wparam, intptr_t lparam) {
	char buf[0x100];

	(void)lparam;
	switch (msg) {
	case 0x10:	/* WM_CLOSE */
		break;
	case 0x110:	/* WM_INITDIALOG */
		Bprintf(buf, sizeof(buf),
			" brillo = %.2f\n gamma = %.2f\n texturas = %d x %d\n rebotes = %d\n coef. rad = %.2f\n",
			(double)GAME_DATA(float, 0x432964), (double)GAME_DATA(float, 0x432960), g_MapaSombrasTexX,
			g_MapaSombrasTexY, GAME_DATA(int, 0x432968), (double)GAME_DATA(float, 0x43296c));
		exe_SetDlgItemTextA(hwnd, 0x40b, buf);
		for (int i = 0; i < 12; i++) {
			exe_CheckDlgButton(hwnd, g_BspDlgChecks[i].id, (unsigned)GAME_DATA(int, g_BspDlgChecks[i].var));
		}
		Dlg_SetFloat(hwnd, 0x410, g_MapaSombrasEscala, 3);
		Dlg_SetFloat(hwnd, 0x40f, g_MapaSombrasResol, 3);
		Dlg_SetFloat(hwnd, 0x41e, GAME_DATA(float, 0x432960), 2);
		Bsp_DlgEnable(hwnd);
		return 1;
	case 0x111:	/* WM_COMMAND */
		if (wparam == 1) {
			for (int i = 0; i < 12; i++) {
				GAME_DATA(int, g_BspDlgChecks[i].var) = exe_IsDlgButtonChecked(hwnd, g_BspDlgChecks[i].id);
			}
			if (Dlg_GetFloat(hwnd, 0x410, &g_MapaSombrasEscala) && Dlg_GetFloat(hwnd, 0x40f, &g_MapaSombrasResol) &&
				Dlg_GetFloat(hwnd, 0x41e, &GAME_DATA(float, 0x432960)) && (double)g_MapaSombrasEscala > 0.001 &&
				(double)g_MapaSombrasResol > 0.001) {
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

#define g_LightmapDebugTex GAME_DATA(int, 0x4d5d38)	/* gvar */
#define g_BspEnlaces GAME_DATA(int, 0x4e73ac)		/* the region links (radiosity code) */

/* exe functions of other files with their prototypes there */
double crt_ceil(double x);				/* rva 0x345010 */

/* brightness and gamma on the light maps, then the vertex colors from them */
void __fastcall CBsp_AjustarGamma(CBsp *self) {
	int n = self->num_listas;

	for (int i = 0; i < n; n = self->num_listas) {
		BspTri *list = self->listas[i];
		E3D_DebugPrintf("ajustar gamma:  %d / %d  \r", ++i, n);
		if (list->luz2.img) {
			CBmpFlt2_Blend(&list->luz2, 0, 0, &list->luz, 0, 0, list->luz2.ancho, list->luz2.alto, 0x100, -0x100);
			CBmpFlt2_Scale(&list->luz2, GAME_DATA(float, 0x432964));
			CBmpFlt2_Gamma(&list->luz2, GAME_DATA(float, 0x432960), 1.0f);
		}
		CBmpFlt2_Scale(&list->luz, GAME_DATA(float, 0x432964));
		CBmpFlt2_Gamma(&list->luz, GAME_DATA(float, 0x432960), 1.0f);
		if (g_LightmapDebugTex) {
			Img_Free(&list->luz2, 0xc);
			CBmpFlt2_Resample(&list->luz2, &list->luz, 0xf);
			for (int k = 0; k < list->luz2.alto * list->luz2.ancho; k++) {
				float c[3] = { 0.0f, 0.0f, 0.0f };
				if (list->luzA.img) {
					const float *p = (const float *)list->luzA.img + 4 * k;
					c[0] = p[0] * 4.0f;
					c[1] = p[1] * 4.0f;
					c[2] = p[2] * 4.0f;
				}
				memcpy((float *)list->luz2.img + 3 * k, c, sizeof(c));
			}
			CBmpFlt2_Gamma(&list->luz2, 2.0f, 1.0f);
		}
		for (BspTri *t = list; t; t = t->sig) {
			for (int v = 0; v < 3; v++) {
				float c[4];
				int y = (int)((float)g_MapaSombrasTexY * t->uvl[v][1]);
				BspTri_Media(c, list, (int)((float)g_MapaSombrasTexX * t->uvl[v][0]), y);
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

static inline byte BspRegion_Hijos(const BspRegion *r) {
	return r->hijo[0] || r->hijo[1] || r->hijo[2] || r->hijo[3];
}

/* the light between the regions a and b with the form factors over umbral: linked when the floor does not block
 * them, else the larger one split into its children; 1 when linked */
int __thiscall BspRegion_Enlazar(CBsp *self, BspRegion *a, BspRegion *b, int enlace, float umbral) {
	if (!a || !b) {
		return 0;
	}
	CHECK(!enlace);
	if (!b->padre && !b->interior) {
		return 0;
	}
	if (!a->padre && !a->interior) {
		return 0;
	}
	if (a->color[1] + a->color[0] + a->color[2] == 0.0f && b->color[1] + b->color[0] + b->color[2] == 0.0f) {
		return 0;
	}
	float ffa = BspRegion_FormFactor(a, b);
	float ffb = BspRegion_FormFactor(b, a);
	if (enlace && umbral > ffa && umbral > ffb) {
		return 1;
	}
	CHECK(a->interior == BspRegion_Hijos(a));
	CHECK(b->interior == BspRegion_Hijos(b));
	if (ffa > umbral && ffa >= ffb && b->interior) {
		for (int k = 0; k < 4; k++) {
			BspRegion_Enlazar(self, a, b->hijo[k], 0, umbral);
		}
		return 0;
	}
	float d = a->centro.y * b->plano.n.y + a->centro.x * b->plano.n.x + a->centro.z * b->plano.n.z + b->plano.d;
	float v = ffb;
	if (a->interior && a->area > d * d) {
		v = umbral + umbral;
	}
	if (v > umbral && a->interior) {
		for (int k = 0; k < 4; k++) {
			BspRegion_Enlazar(self, a->hijo[k], b, 0, umbral);
		}
		return 0;
	}
	CSuelo *suelo = Mundo_Get()->ObjSuelo;
	CHECK(suelo);
	vec3_t dir = { a->centro.x - b->centro.x, a->centro.y - b->centro.y, a->centro.z - b->centro.z };
	vec3_t n;
	Vec3_Normalize(&n, &dir);
	float k = 0.3f / g_MapaSombrasEscala;
	vec3_t p1 = { b->centro.x + n.x * k, b->centro.y + n.y * k, b->centro.z + n.z * k };
	vec3_t p0 = { a->centro.x - n.x * k, a->centro.y - n.y * k, a->centro.z - n.z * k };
	if (CSuelo_Trace(suelo, &p0, &p1, NULL, 0.0f, NULL)) {
		return 0;
	}
	for (int i = 0; i < 4; i++) {
		a->acum[i] = a->acum[i] + b->color[i] * ffa;
	}
	for (int i = 0; i < 4; i++) {
		b->acum[i] = b->acum[i] + a->color[i] * v;
	}
	return 1;
}

/* the region trees of the lists (the area lights emit their color) and two rounds of links and light */
void __fastcall CBsp_IniciarRegiones(CBsp *self) {
	E3D_DebugPrintf("rad_inic_regiones()\n");
	for (int i = 0; i < self->num_listas; i++) {
		BspTri *list = self->listas[i];
		if (!list) {
			continue;
		}
		double l2 = Game_Log(2.0);
		for (BspTri *t = list; t; t = t->sig) {
			BspRegion *r = operator_new(sizeof(BspRegion));
			if (r) {
				r = BspRegion_ctor(r, 0, 0, 0, 0, NULL);
			}
			t->region = r;
			if (list->radluz_ok) {
				const byte *c = (const byte *)&list->radColor;
				float s = list->radInt * 0.003921568859368563f;
				for (int k = 0; k < 4; k++) {
					r->color[k] = (float)(int)c[k] * s * 80.0f;
				}
			}
			r->x = 0;
			r->y = 0;
			r->lx = (uint16_t)list->mask.ancho;
			r->ly = (uint16_t)list->mask.alto;
			r->area = t->area;
			r->centro = t->centro;
			r->plano = t->plano;
			int e = (int)crt_ceil(Game_Log((double)r->lx) / l2);
			r->lx = (uint16_t)(1u << (e & 31));
			e = (int)crt_ceil(Game_Log((double)r->ly) / l2);
			r->ly = (uint16_t)(1u << (e & 31));
			BspRegion_Dividir(r, &t->areas[0]);
			if (!r->interior && t->region) {
				BspRegion *old = t->region;
				BspRegion_dtor(old);
				operator_delete_sized(old, sizeof(BspRegion));
				t->region = NULL;
			}
		}
	}
	for (int pass = 2; pass; pass--) {
		E3D_DebugPrintf("enlazar_regiones:\n");
		for (int i = 0; i < self->num_listas;) {
			BspTri *list = self->listas[i];
			E3D_DebugPrintf("region: %d / %d   ", ++i, self->num_listas);
			for (BspTri *ta = list; ta; ta = ta->sig) {
				for (int j = i; j < self->num_listas; j++) {
					for (BspTri *tb = self->listas[j]; tb; tb = tb->sig) {
						BspRegion_Enlazar(self, ta->region, tb->region, 0, 0.001f);
					}
				}
			}
			E3D_DebugPrintf("Kb: %-5d   enlaces: %d   \r", g_MemStats->bytes / 1024, g_BspEnlaces);
		}
		E3D_DebugPrintf("\n");
		CBsp_IlumRegiones(self, 1);
	}
	E3D_DebugPrintf("\n");
}

/* the sample points of the texels of each triangle of the lists and their levels of detail */
void __fastcall CBsp_IniciarAreas(CBsp *self) {
	CTabla tmp;

	E3D_DebugPrintf("rad_inic_areas()\n");
	for (int i = 0; i < 4; i++) {
		g_BspAreasTotal[i] = 0;
	}
	tmp.p = NULL;
	tmp.n = 0;
	Tabla_Alloc18(&tmp, g_MapaSombrasTexY * g_MapaSombrasTexX);
	int nl = self->num_listas;
	for (int i = 0; i < nl; nl = self->num_listas) {
		BspTri *list = self->listas[i];
		E3D_DebugPrintf("%d / %d   \r", ++i, nl);
		int x0 = list->ext[0];
		int y0 = list->ext[1];
		list->grande = 0;
		list->numAreas = 0;
		for (BspTri *t = list; t; t = t->sig) {
			int n = 0;
			if (!(__builtin_fabs((double)(list->plano.n.x - t->plano.n.x)) < BSP_EPS_NORMAL &&
				__builtin_fabs((double)(list->plano.n.y - t->plano.n.y)) < BSP_EPS_NORMAL &&
				__builtin_fabs((double)(list->plano.n.z - t->plano.n.z)) < BSP_EPS_NORMAL)) {
				list->grande = 1;
			}
			BspTri_CalcCentro(t);
			for (int y = 0; y < list->mask.alto; y++) {
				for (int x = 0; x < list->mask.ancho; x++) {
					if (Color_SwapRB(*CBmp_Pixel(&list->mask, (uint32_t)x, (uint32_t)y)) != (uint32_t)(uintptr_t)t) {
						continue;
					}
					vec3_t p;
					float peso = 1.0f;
					BspTri_Muestra(&p, x + x0, y + y0, list->eje, g_MapaSombrasEscala * g_MapaSombrasResol, t, list,
						&peso);
					BspArea *a = &((BspArea *)tmp.p)[n];
					TABLA_CHECK(tmp.p, n, tmp.n);
					a->pos = p;
					TABLA_CHECK(tmp.p, n, tmp.n);
					a->peso = peso / (g_MapaSombrasResol * g_MapaSombrasResol);
					TABLA_CHECK(tmp.p, n, tmp.n);
					a->x = (uint16_t)x;
					TABLA_CHECK(tmp.p, n, tmp.n);
					a->y = (uint16_t)y;
					TABLA_CHECK(tmp.p, n, tmp.n);
					a->f14 = 0.0f;
					n++;
				}
			}
			if (n) {
				CHECK(!t->areas[0].p && n > 0);
				t->areas[0].n = n;
				t->areas[0].p = New_Array((uint32_t)n, sizeof(BspArea));
				CHECK(t->areas[0].p);
				for (int k = 0; k < n; k++) {
					TABLA_CHECK(tmp.p, k, tmp.n);
					TABLA_CHECK(t->areas[0].p, k, t->areas[0].n);
					((BspArea *)t->areas[0].p)[k] = ((const BspArea *)tmp.p)[k];
				}
			}
			BspTri_AreasLod(&t->areas[1], &t->areas[0], list->mask.ancho, list->mask.alto, 2);
			BspTri_AreasLod(&t->areas[2], &t->areas[0], list->mask.ancho, list->mask.alto, 4);
			BspTri_AreasLod(&t->areas[3], &t->areas[0], list->mask.ancho, list->mask.alto, 8);
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

#define g_LightmapGlowMult GAME_DATA(float, 0x4d5cc0)	/* gvar */

/* the glow map of the material sampled into luzA, squared */
void BspTri_CalcGlow(BspTri *list) {
	static const float cero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	CBmp tex;
	float tmp[3];

	tex.img = NULL;
	if (list->Mat && CMaterial_GetMapName(list->Mat, MAPA_GLOW) &&
		!CBmp_Load(&tex, CMaterial_GetMapName(list->Mat, MAPA_GLOW), 0)) {
		CBmpFlt_CreateFill(&list->luzA, list->lx, list->ly, cero);
		int x0 = list->ext[0];
		int y0 = list->ext[1];
		for (BspTri *t = list; t; t = t->sig) {
			for (int y = 0; y < list->mask.alto; y++) {
				for (int x = 0; x < list->mask.ancho; x++) {
					if ((uint32_t)x >= (uint32_t)g_MapaSombrasTexX || (uint32_t)y >= (uint32_t)g_MapaSombrasTexY) {
						continue;
					}
					if (Color_SwapRB(*CBmp_Pixel(&list->mask, (uint32_t)x, (uint32_t)y)) != (uint32_t)(uintptr_t)t) {
						continue;
					}
					float c[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
					if (tex.img) {
						const float *s = Bsp_SampleTexAA(tmp, x0 + x, y + y0, list->eje,
							g_MapaSombrasEscala * g_MapaSombrasResol, t, &tex, 4);
						float k = g_LightmapGlowMult;
						float r = s[0], g = s[1], b = s[2];
						c[0] = r * r * k;
						c[1] = g * g * k;
						c[2] = b * b * k;
					}
					memcpy(CBmpFlt_Pixel(&list->luzA, (uint32_t)x, (uint32_t)y), c, sizeof(c));
				}
			}
		}
	}
	Img_Free(&tex, 4);
}

/* the share of samples rays from the sphere of radius r at p (moved along n) to the sphere of the light at luz
 * that the floor does not block */
float Bsp_Oclusion(const vec3_t *p, const vec3_t *n, float r, int samples, const vec3_t *luz, CSuelo *suelo) {
	vec3_t lp = *luz;
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
			Bsp_DirEsfera(d, z, v);
			vec3_t from = { d[0] * r + (p->x + n->x * r), d[1] * r + (n->y * r + p->y), d[2] * r + (n->z * r + p->z) };
			float s1 = (float)((double)(crt_rand() & 0x7fff) * 3.051850947599719e-05 + 0.0);
			float z2 = (float)((double)(crt_rand() & 0x7fff) * 3.051850947599719e-05 + 0.0) * 2.0f - 1.0f;
			Bsp_DirEsfera(d, z2, s1);
			vec3_t to = { d[0] * rad + lp.x, d[1] * rad + lp.y, d[2] * rad + lp.z };
			float umbral = 0.8f / (g_MapaSombrasResol * g_MapaSombrasEscala);
			if (CSuelo_Trace(suelo, &from, &to, NULL, umbral, info)) {
				hits++;
			}
		}
	}
	return 1.0f - (float)hits / (float)samples;
}


/* the texels of no list (mask 0) take the weighted average of the neighbours of a list (sides 16, corners 1);
 * without any they are marked (4, 0, 4, 0) */
void CBmpFlt_Rellenar(CBmpFlt *img, const CBmp *mask) {
	static const int dx[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
	static const int dy[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
	static const int peso[8] = { 1, 16, 1, 16, 16, 1, 16, 1 };

	if (!img->img) {
		return;
	}
	uint32_t w = (uint32_t)img->ancho;
	int h = img->alto;
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
				float f = (float)peso[k];
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
void __fastcall CBsp_GuardarIni(CBsp *self) {
	char name[0x104];
	CArchINI ini;
	const char *s = "mapa_sombras";

	if (!g_LmapGuardarIni) {
		return;
	}
	Ruta_Merge(name, ".ini", self->nombre);
	CArchINI_ctorLoad(&ini, name, NULL);
	int n = CArchINI_SetFloat(&ini, s, "escala", g_MapaSombrasEscala, 3);
	n += CArchINI_SetFloat(&ini, s, "resolucion", g_MapaSombrasResol, 3);
	n += CArchINI_SetFloat(&ini, s, "gamma", GAME_DATA(float, 0x432960), 2);
	n += CArchINI_SetFloat(&ini, s, "brillo", GAME_DATA(float, 0x432964), 2);
	n += CArchINI_SetBool(&ini, s, "sombras_suaves", GAME_DATA(int, 0x4b2b04));
	n += CArchINI_SetInt(&ini, s, "text_tam_x", g_MapaSombrasTexX);
	n += CArchINI_SetInt(&ini, s, "text_tam_y", g_MapaSombrasTexY);
	n += CArchINI_SetBool(&ini, s, "radiosidad", g_MapaSombrasRadiosidad);
	n += CArchINI_SetBool(&ini, s, "ilum_global", GAME_DATA(int, 0x4b2b14));
	n += CArchINI_SetBool(&ini, s, "no_luz_amb", GAME_DATA(int, 0x4b2b18));
	n += CArchINI_SetBool(&ini, s, "modulate4x", g_MapaSombrasModulate4x);
	n += CArchINI_SetFloat(&ini, s, "modulate_coef", g_MapaSombrasModulate4x ? 0.25f : 1.0f, 3);
	n += CArchINI_SetFloat(&ini, s, "modulate_max", g_MapaSombrasModulate4x ? 1.0f : 1.2f, 3);
	n += CArchINI_SetInt(&ini, s, "rebotes", GAME_DATA(int, 0x432968));
	n += CArchINI_SetFloat(&ini, s, "coef_rad", GAME_DATA(float, 0x43296c), 3);
	n += CArchINI_SetBool(&ini, s, "calc_lmap2", g_CalcLmap2);
	n += CArchINI_SetBool(&ini, s, "usar_txf", g_MapaSombrasUsarTxf);
	if (n) {
		File_Backup(name, 0);
		if (CArchINI_Save(&ini, name, NULL, 0)) {
			Err_Report(*crt_errno(), 0, "\n'%s'", name);
		}
	}
	CArchINI_dtor(&ini);
}


/* the .EMI file of the map: the materials, the light map pages and the lists */

int __fastcall CBsp_LeerEMI(CBsp *self) {
	char ruta[0x104], dir[0x104], texPath[0x104], buf[0x104];
	char sector[0x80];
	CTablaHash tabMat, tabLMap;

	Ruta_Merge(ruta, ".emi", self->nombre);
	if (File_TimeDiff(ruta, self->nombre, 0) < 0) {
		Con_Warnf("WARN: [%s] is older than [%s]\n", ruta, self->nombre);
	}
	CArchIO *mem = operator_new(sizeof(CArchIO));
	CArchIO *f = mem ? CArchIO_ctor(mem, ruta, "rb") : NULL;
	if (f->error) {
		CArchIO_dtor(f);
		operator_delete_sized(f, sizeof(CArchIO));
		return -1;
	}
	Ruta_Part(dir, ruta, 3);
	CHECK(CArchIO_BeginBlock(f, "EMI") == 0);
	int version = CArchIO_ReadInt(f);
	CHECK(version >= 103 && version <= 105);
	E3D_LogDebug2(g_FileLog, "EMI: Version: %d", version);
	CTablaHash_ctor(&tabMat, 0x1f);
	CTablaHash_ctor(&tabLMap, 0x1f);
	int n = CArchIO_ReadInt(f);
	E3D_LogDebug2(g_FileLog, "EMI: Materials: %d", n);
	Cad_Copy(texPath, dir, sizeof(texPath));
	CArchINI *ini = Mundo_Get()->EscSuelo->ini;
	CHECK(ini);
	CArchINI_GetText(ini, "Model", "TexturePath", texPath, sizeof(texPath), NULL);
	if (n > 0) {
		do {
			CMaterial *mm = operator_new(sizeof(CMaterial));
			CMaterial *mat = mm ? CMaterial_ctor(mm) : NULL;
			CTablaHash_AddInt(&tabMat, CArchIO_ReadInt(f), mat, 0);
			CListaMat_Add(g_ListaMat, mat);
			CMaterial_Read(mat, f);
			mat->params.atrib |= MAT_NOSHARE;
			/* the maps are looked for in the texture path of the map */
			for (int i = 0; i < MAPA_TOTAL; i++) {
				CHECK(i < MAPA_TOTAL);
				CMapa *mapa = mat->mapa[i];
				if (mapa) {
					Ruta_Part(buf, mapa->Nombre, 0xc);
					Ruta_Merge(buf, buf, texPath);
					Cad_Set(&mapa->Nombre, buf);
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
	for (int key = CArchIO_ReadInt(f); key; key = CArchIO_ReadInt(f)) {
		*pp = BspLMap_New();
		CTablaHash_AddInt(&tabLMap, key, *pp, 0);
		(*pp)->id = ++id;
		CArchIO_ReadString(f, buf, sizeof(buf));
		Ruta_Merge(buf, buf, dir);
		Ruta_Merge(buf, pre, buf);
		BspLMap_CrearMat(*pp, buf, 1);
		if (version >= 104) {
			CHECK(CArchIO_ReadInt(f) == 1);
			CArchIO_ReadString(f, buf, sizeof(buf));
			Ruta_Merge(buf, buf, dir);
			Ruta_Merge(buf, pre, buf);
			if (g_LmapInfo || g_CalcLmap2) {
				(*pp)->lmap2 = BspLMap_New();
				(*pp)->lmap2->id = id;
				BspLMap_CrearMat((*pp)->lmap2, buf, 2);
			}
		}
		pp = &(*pp)->sig;
	}
	CHECK(!self->num_listas);
	self->num_listas = CArchIO_ReadInt(f);
	E3D_LogDebug2(g_FileLog, "EMI: List count: %d", self->num_listas);
	CHECK(self->num_listas < BSP_MAX_LISTAS);
	for (int i = 0; i < self->num_listas; i++) {
		BspTri *tm = operator_new(sizeof(BspTri));
		BspTri *tri = tm ? BspTri_ctor(tm) : NULL;
		self->listas[i] = tri;
		CArchIO_BeginBlock(f, "TRI");
		tri->flags = (uint32_t)CArchIO_ReadInt(f);
		CArchIO_ReadString(f, buf, sizeof(buf));
		tri->zona = CArchIO_ReadInt(f);
		if (version >= 105) {
			CArchIO_ReadString(f, sector, sizeof(sector));
			tri->zona = Sector_Find(sector);
			if (!strcmp(sector, "_nosector_")) {
				E3D_LogInfo(g_FileLog, "WARN: Mesh without sector [%s]", buf);
			} else if (!strcmp(sector, "_unasigned_")) {
				Con_Warnf("WARN: Mesh without sector [%s]\n", buf);
			}
		}
		CHECK(tri->zona < Sector_Num());
		CHECK(!CTablaHash_GetInt(&tabMat, CArchIO_ReadInt(f), (void **)&tri->Mat));
		CTablaHash_GetInt(&tabLMap, CArchIO_ReadInt(f), (void **)&tri->LMap);
		CHECK(tri->LMap || tri->flags & BSPTRI_NOLMAP);
		tri->numTri = CArchIO_ReadInt(f);
		CHECK(!f->error);
		tri->pTri = New_Array((uint32_t)tri->numTri, 6);
		CArchIO_Read(f, tri->pTri, tri->numTri * 6);
		CListaVert_Read(&tri->lv, f);
		CListaVert_Read(&tri->lv2, f);
		if (!tri->Mat->params.transp) {
			CListaVert_SetColor(&tri->lv, 0xffffff, 0);
		}
		CArchIO_Error(f, "TRI");
	}
	CArchIO_Error(f, "EMI");
	CHECK(!f->error);
	CArchIO_dtor(f);
	operator_delete_sized(f, sizeof(CArchIO));
	CTablaHash_dtor(&tabLMap);
	CTablaHash_dtor(&tabMat);
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

static BspMatStat *BspMatStat_Get(CTabla *t, int i) {
	TABLA_CHECK(t->p, i, t->n);
	return &((BspMatStat *)t->p)[i];
}

void __fastcall CBsp_Preparar(CBsp *self) {
	CTabla tab = { 0, NULL };

	if (g_DebugLevel < 3) {
		return;
	}
	E3D_LogDebug(g_FileLog, "\nMap triangles by material/sector:");
	for (int zn = 0; zn < self->zonas.n; zn++) {
		TABLA_CHECK(self->zonas.p, zn, self->zonas.n);
		BspTri *tri = ((BspZona *)self->zonas.p)[zn].tris;
		const char *nodo = tri && tri->nodo ? tri->nodo->name : "(null)";
		Con_Verbosef("  Sector: %d  name: %s  node: %s\n", zn - 1, zn ? Sector_GetName(zn - 1) : "_unasigned_",
			nodo);
		if (!tri) {
			Con_Warnf("    sector without triangles\n");
			continue;
		}
		for (; tri; tri = tri->zona_sig) {
			CHECK(tri->zona + 1 == zn);
			const char *lmap = tri->LMap ? Ruta_FileName(CMaterial_GetMapName(tri->LMap->Mat, MAPA_DIFFUSE)) : "-";
			Con_Debugf("    tri: %-5d  %-5d  %-6s    %-20s  ", tri->numTri, tri->lv.nVert, CListaVert_FormatName(&tri->lv),
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
				Tabla_Resize14(&tab, i + 1);
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
	crt_qsort(tab.p, (size_t)tab.n, sizeof(BspMatStat), (int (*)(const void *, const void *))Game_Ptr(0x28e650));
	int totTri = 0, totVert = 0;
	for (int i = 0; i < tab.n; i++) {
		BspMatStat *e = BspMatStat_Get(&tab, i);
		uint32_t bytes = (uint32_t)(ListaVert_FormatSize(e->frmt) * e->numVert);
		Con_Debugf("  tri: %-5d  %-5d  %-6s %6.1fk   dis: %-3d", e->numTri, e->numVert, ListaVert_FormatName(e->frmt),
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

void __fastcall CBsp_InitD3D(CBsp *self) {
	/* the vertex size and FVF of the buffers of a sector (BspZona_GetVB: the first four) */
	static const struct {
		int size;
		uint32_t fvf;
	} fmts[8] = {
		{ 0x1c, 0x1c2 }, { 0x24, 0x152 }, { 0x20, 0x242 }, { 0x2c, 0x252 },
		{ 0x12, 0 }, { 0x16, 0 }, { 0x16, 0 }, { 0x1a, 0 },
	};
	CMaterial mat;

	CHECK(g_Motor);
	/* one vertex buffer of each format and one index buffer a sector, as large as its lists */
	for (int zn = 0; zn < self->zonas.n; zn++) {
		TABLA_CHECK(self->zonas.p, zn, self->zonas.n);
		BspZona *z = &((BspZona *)self->zonas.p)[zn];
		BspTri *tri = z->tris;
		CHECK(tri || !zn);
		for (int k = 0; k < 8; k++) {
			CRenderVB_Reset(&z->vb[k]);
		}
		CRenderIB_Reset(&z->ib);
		for (; tri; tri = tri->zona_sig) {
			BspZona_GetVB(z, tri->lv.frmt)->Used += tri->lv.nVert;
			z->ib.Used += tri->numTri;
		}
		for (int k = 0; k < 8; k++) {
			CRenderVB_Create(&z->vb[k], (uint32_t)z->vb[k].Used, fmts[k].size, fmts[k].fvf);
		}
		CRenderIB_Create(&z->ib, (uint32_t)z->ib.Used);
	}
	/* the lists into the buffers of their sector, or buffers of their own */
	for (int i = 0; i < self->num_listas; i++) {
		BspTri *tri = self->listas[i];
		CHECK(!tri->pVertVB);
		CHECK(!tri->pVertIB);
		BspZona *z = CBsp_Zona(self, tri->zona);
		CRenderVB *vb = BspZona_GetVB(z, tri->lv.frmt);
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
		CHECK(Render_CreateVB(&tri->pVertVB, &tri->lv, !g_MapaSombrasMultitext) == 0);
		CHECK(Render_CreateIB(&tri->pVertIB, tri->pTri, tri->numTri, !g_MapaSombrasMultitext) == 0);
	}
	/* the materials of the debug views: the light maps alone and the wire frame */
	CMaterial_ctor(&mat);
	Cad_Set(&mat.Nombre, "MatVerMapas");
	CMaterial_SetMap(&mat, MAPA_DIFFUSE, "bmp/rgb_grey.tga");
	mat.params.b39 = 0;
	self->matFX = Mat_New(&mat, 1);
	Cad_Set(&mat.Nombre, "MatVerMalla");
	CMaterial_SetMap(&mat, MAPA_DIFFUSE, "bmp/rgb_white.tga");
	mat.params.b39 = 0;
	mat.params.atrib = 0x800;
	self->matWire = Mat_New(&mat, 1);
	CModelo_InitD3D(self->modelo, MOTOR_DEV(g_Motor), 1);
	CMaterial_dtor(&mat);
}

/* the static initializer (rva 0x05e6e0): the trace caches of the lights cleared */
void Bsp_InitLuces(void) {
	for (int i = 0; i < BSP_MAX_LUCES; i++) {
		g_BspLuces[i].cache[0] = 0;
		g_BspLuces[i].cache[1] = 0;
		g_BspLuces[i].cache[2] = 0;
	}
}
