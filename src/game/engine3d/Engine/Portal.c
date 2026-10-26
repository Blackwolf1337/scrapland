/*
 * Engine/portal.cpp of the engine3d: CPortal, see include/scrap/portal.h.
 */
#include "scrap/portal.h"
#include "scrap/sistpart.h"
#include "scrap/suelo.h"
#include "scrap/varios.h"

CPortal *__fastcall CPortal_ctor(CPortal *self) {
	self->nodo = NULL;
	self->vftable = Game_Ptr(CPORTAL_VFTABLE);
	self->clase = CLASE_OBJ3D_PORTAL;
	self->ancho = 1.0f;
	self->alto = 1.0f;
	self->zonas[0] = NULL;
	self->zonas[1] = NULL;
	self->idZona[0] = -1;
	self->idZona[1] = -1;
	self->calculado = 0;
	return self;
}

void __fastcall CPortal_dtor(CPortal *self) {
	self->vftable = Game_Ptr(CPORTAL_VFTABLE);
	for (int i = 0; i < 2; i++) {
		ObjRef_Drop(&self->zonas[i]);
		self->zonas[i] = NULL;
	}
	CObjBase3D_dtor((IObjeto3D *)self);
}

CPortal *__thiscall CPortal_Delete(CPortal *self, byte flags) {
	CPortal_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CPortal));
	}
	return self;
}

void __fastcall CPortal_LinkZonas(CPortal *self) {
	if (self->zonas[0] || self->zonas[1]) {
		return;
	}
	for (CNodo3D *n = self->nodo->escena->Nodo3D; n; n = n->sig) {
		if (n->i48 < 0) {
			continue;
		}
		for (int i = 0; i < 2; i++) {
			if (n->i48 == self->idZona[i]) {
				n->Refs++;
				ObjRef_Drop(&self->zonas[i]);
				self->zonas[i] = n;
			}
		}
	}
}

byte __thiscall CPortal_Read(CPortal *self, CArchIO *f, MallaD3D_FileCtx *ctx) {
	CObjBase3D_SetNodo((IObjeto3D *)self, ctx->nodo);
	CArchIO_BeginBlock(f, "PORT");
	CHECK(CArchIO_ReadInt(f) == 1);
	self->idZona[0] = CArchIO_ReadInt(f);
	self->idZona[1] = CArchIO_ReadInt(f);
	CArchIO_Read(f, &self->ancho, 4);
	CArchIO_Read(f, &self->alto, 4);
	self->calculado = 0;
	CArchIO_Error(f, "PORT");
	return 1;
}

byte __thiscall CPortal_Write(CPortal *self, CArchIO *f, MallaD3D_FileCtx *ctx) {
	CHECK(self->nodo == ctx->nodo);
	CArchIO_BeginBlockWrite(f, "PORT");
	CArchIO_WriteInt(f, 1);
	CArchIO_WriteInt(f, self->zonas[0] ? self->zonas[0]->i48 : -1);
	CArchIO_WriteInt(f, self->zonas[1] ? self->zonas[1]->i48 : -1);
	CArchIO_Write(f, &self->ancho, 4);
	CArchIO_Write(f, &self->alto, 4);
	CArchIO_EndBlockWrite(f, "PORT");
	return 1;
}

void __thiscall CPortal_Render(CPortal *self, void *dev, CNodoR *r) {
	(void)dev;
	CHECK(!r);
	CHECK(self->nodo);
	if (g_ShowPortals) {
		CPortal_DrawDebug(self, 0x808080);
	}
}

/* the signed distance of p to the cached plane */
static float Portal_Plano(const CPortal *self, const vec3_t *p) {
	const plane_t *pl = &self->cache_plano;
	return pl->n.y * p->y + p->x * pl->n.x + pl->n.z * p->z + pl->d;
}

/* p is in the rectangle (its two triangles) */
static byte Portal_Dentro(const CPortal *self, const vec3_t *p) {
	const vec3_t *c = self->cache_vert;
	return Suelo_PointInTri(p, &c[0], &c[1], &c[2]) || Suelo_PointInTri(p, &c[0], &c[2], &c[3]);
}

float __thiscall CPortal_PlaneDist(CPortal *self, const vec3_t *p) {
	CPortal_UpdateCache(self, &self->nodo->m);
	const plane_t *pl = &self->cache_plano;
	return pl->n.y * p->y + pl->n.x * p->x + pl->n.z * p->z + pl->d;
}

float __thiscall CPortal_Dist(CPortal *self, const vec3_t *p) {
	CHECK(self->nodo);
	CPortal_UpdateCache(self, &self->nodo->m);
	const plane_t *pl = &self->cache_plano;
	float d = Portal_Plano(self, p);
	vec3_t q = { p->x - pl->n.x * d, p->y - pl->n.y * d, p->z - pl->n.z * d };
	if (Portal_Dentro(self, &q)) {
		return (float)__builtin_fabs((double)d);
	}
	const vec3_t *c = self->cache_vert;
	float m = Suelo_Dist2PointSeg(&c[3], &c[0], p, NULL);
	for (int i = 0; i < 3; i++) {
		float e = Suelo_Dist2PointSeg(&c[i], &c[i + 1], p, NULL);
		m = e <= m ? e : m;
	}
	return (float)__builtin_sqrt((double)m);
}

byte __thiscall CPortal_SegCross(CPortal *self, const vec3_t *a, const vec3_t *b, vec3_t *out) {
	CHECK(self->nodo);
	CPortal_UpdateCache(self, &self->nodo->m);
	const plane_t *pl = &self->cache_plano;
	float da = pl->n.x * a->x + a->y * pl->n.y + a->z * pl->n.z + pl->d;
	float db = b->x * pl->n.x + b->y * pl->n.y + b->z * pl->n.z + pl->d;
	if ((db < 0.0f) == (da < 0.0f)) {
		return 0;
	}
	float t = da / (da - db);
	vec3_t q = { t * (b->x - a->x) + a->x, t * (b->y - a->y) + a->y, (b->z - a->z) * t + a->z };
	if (!Portal_Dentro(self, &q)) {
		return 0;
	}
	if (out) {
		*out = q;
	}
	return 1;
}

void __thiscall CPortal_SetZona(CPortal *self, uint32_t i, CNodo3D *zona) {
	CNodo3D **slot = &self->zonas[i & 1];
	if (zona) {
		zona->Refs++;
	}
	ObjRef_Drop(slot);
	*slot = zona;
}

void __thiscall CPortal_UpdateCache(CPortal *self, const matrix_t *unused) {
	(void)unused;
	if (self->calculado) {
		return;
	}
	float hx = self->ancho * 0.5f;
	float hz = self->alto * 0.5f;
	self->calculado = 1;
	const matrix_t *m = &self->nodo->m;
	vec3_t v = { -hx, 0.0f, hz };
	Vec3_TransformH(&self->cache_vert[0], &v, m);
	v = (vec3_t){ hx, 0.0f, hz };
	Vec3_TransformH(&self->cache_vert[1], &v, m);
	v = (vec3_t){ hx, 0.0f, -hz };
	Vec3_TransformH(&self->cache_vert[2], &v, m);
	v = (vec3_t){ -hx, 0.0f, -hz };
	Vec3_TransformH(&self->cache_vert[3], &v, m);
	CHECK(Plane_Calc(&self->cache_plano, &self->cache_vert[0], &self->cache_vert[1], &self->cache_vert[2]));
	float a = (float)(Game_Acos(__builtin_fabs((double)self->cache_plano.n.y)) * 180.0 / 3.141592653589793);
	if (a > 89.9f && 91.1f > a && a != 90.0f) {
		E3D_LogInfo(g_FileLog, "WARN: unaligned vertical portal: %s", self->nodo->name);
	}
}

static int Portal_Clamp(int v, int n) {
	if (v < 1) {
		return 0;
	}
	return n - 1 <= v ? n - 1 : v;
}

void Portal_DrawRect(const int *vp, const float *rect, uint32_t color) {
	int w = vp[2], h = vp[3];
	int x0 = Portal_Pixel(rect[0] + 1.0f, (uint32_t)w, (uint32_t)vp[0]);
	int y0 = Portal_Pixel(1.0f - rect[1], (uint32_t)h, (uint32_t)vp[1]) - 1;
	int y1 = Portal_Pixel(1.0f - rect[4], (uint32_t)h, (uint32_t)vp[1]);
	int x1 = Portal_Pixel(rect[3] + 1.0f, (uint32_t)w, (uint32_t)vp[0]) - 1;
	x0 = Portal_Clamp(x0, w);
	y0 = Portal_Clamp(y0, h);
	x1 = Portal_Clamp(x1, w);
	y1 = Portal_Clamp(y1, h);
	Debug_Line(x0, y0, x1, y0, color, 0);
	Debug_Line(x1, y0, x1, y1, color, 0);
	Debug_Line(x1, y1, x0, y1, color, 0);
	Debug_Line(x0, y1, x0, y0, color, 0);
}
