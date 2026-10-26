/*
 * Engine/SueloPortal.cpp of the engine3d: the sectors and their lookups, see include/scrap/sueloportal.h.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/portal.h"
#include "scrap/sistpart.h"
#include "scrap/sueloportal.h"
#include "scrap/varios.h"

#define g_Mundo GAME_DATA(byte *, 0x4afde8)	/* gMundo of mundo.h */
#define MUNDO_SUELO 0xb0			/* gMundo->obtSuelo() */
#define PORTAL_ASPECT 1.33f			/* the screen height of a sphere */
#define PORTAL_EPS 8.0e-6f			/* the step out of a portal, times the distance to the origin */

typedef long(__stdcall *d3dGetViewport_t)(void *dev, uint32_t *vp);
#define D3D_METHOD_VP(dev) ((d3dGetViewport_t)((*(void ***)(dev))[0xa4 / 4]))

CZona *__fastcall CZona_ctor(CZona *self) {
	self->rect = (box_t){ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	self->id = -1;
	self->bloq = 0;
	self->Frame3D = 0;
	self->nodo = NULL;
	self->numNodos = 0;
	self->numPortales = 0;
	self->exterior = 0;
	self->b139 = 0;
	self->b13a = 0;
	self->nodos[0] = NULL;
	self->nodos[1] = NULL;
	memset(self->portales, 0, sizeof(self->portales));
	memset(self->zonas, 0, sizeof(self->zonas));
	return self;
}

void __fastcall CZona_dtor(CZona *self) {
	if (self->nodo) {
		ObjRef_Release(&self->nodo);
	}
	self->nodo = NULL;
	for (int i = 0; i < self->numNodos; i++) {
		if (self->nodos[i]) {
			ObjRef_Release(&self->nodos[i]);
		}
		self->nodos[i] = NULL;
	}
	for (int i = 0; i < self->numPortales; i++) {
		if (self->portales[i]) {
			ObjRef_Release(&self->portales[i]);
		}
		self->portales[i] = NULL;
	}
}

void __thiscall SectorVis_Draw(SectorVis *self, const vec3_t *pos, byte visible, byte cached, const char *name) {
	uint32_t off, on;

	(void)name;
	if (!(self->flags & SECTORVIS_VALID)) {
		Debug_Text3D(g_DebugFont2, 0.0f, 0.0f, pos, 0x808080, Game_Ptr(0x3dd0ec));	/* "[-]" */
		return;
	}
	uint32_t color;
	if (self->sector < 0) {
		off = 0x800080;
		on = 0xff0000;
	} else {
		if (visible) {
			color = 0xff;
			goto draw;
		}
		off = 0x808080;
		on = 0x8000;
		visible = cached;
	}
	color = visible ? on : off;
draw:
	Debug_Text3D(g_DebugFont2, 0.0f, 0.0f, pos, color, Game_Ptr(0x3dd0f0), self->sector,	/* "%d%s" */
		(self->flags & SECTORVIS_CACHED) ? Game_Ptr(0x3a3228) : Game_Ptr(0x3ca94c));	/* "", "*" */
}

/* the cached sector is seen (none counts as seen) */
static byte SectorVis_Seen(SectorVis *self) {
	self->flags |= SECTORVIS_CACHED;
	if (self->sector < 0) {
		return 1;
	}
	return Sector_Get((uint32_t)self->sector)->Frame3D == g_FrameCount;
}

static byte SectorVis_SamePos(const SectorVis *self, const vec3_t *pos) {
	return pos->x == self->pos.x && pos->y == self->pos.y && pos->z == self->pos.z && g_SectorCache;
}

byte __thiscall SectorVis_Check(SectorVis *self, const vec3_t *pos, const char *name) {
	if (!(self->flags & SECTORVIS_VALID)) {
		return SectorVis_Update(self, pos, name);
	}
	if (SectorVis_SamePos(self, pos)) {
		return SectorVis_Seen(self);
	}
	self->flags &= ~SECTORVIS_CACHED;
	return 1;
}

byte __thiscall SectorVis_Update(SectorVis *self, const vec3_t *pos, const char *name) {
	if ((self->flags & SECTORVIS_VALID) && SectorVis_SamePos(self, pos)) {
		return SectorVis_Seen(self);
	}
	self->sector = -2;
	self->flags = (self->flags & ~SECTORVIS_CACHED) | SECTORVIS_VALID;
	self->pos = *pos;
	return Sector_FindPos(&self->pos, &self->sector, name);
}

byte Sector_FindPos(const vec3_t *pos, int *sector, const char *name) {
	SueloRayo res;

	(void)name;
	if (!g_PortalesOn) {
		return 1;
	}
	CHECK(g_Mundo);
	void *suelo = *(void **)(g_Mundo + MUNDO_SUELO);
	CHECK(suelo);
	res.flags = 0x202;
	res.y = pos->y;
	if (!CSuelo_RayoVert(suelo, pos->x, pos->z, &res)) {
		return 1;
	}
	int s = res.sector;
	byte seen = 1;
	if (s != -1) {
		vec3_t a = { pos->x, res.hitY, pos->z };
		s = Sector_Recorrer(s, &a, pos, 0);
		seen = Sector_Get((uint32_t)s)->Frame3D == g_FrameCount;
	}
	if (sector) {
		*sector = s;
	}
	return seen;
}

const char *Sector_GetName(int sector) {
	if (sector == -1) {
		return Game_Ptr(0x3dd0a4);	/* "_unasigned_" */
	}
	return Sector_Get((uint32_t)sector)->nodo->name;
}

byte Sector_IsExterior(int sector) {
	return Sector_Get((uint32_t)sector)->exterior;
}

byte Sector_IsOutside(int sector) {
	if (sector == -1) {
		return 0;
	}
	return Sector_Get((uint32_t)sector)->b13a;
}

byte Sector_Get139(int sector) {
	return Sector_Get((uint32_t)sector)->b139;
}

byte Sector_IsVisible(int sector) {
	return Sector_Get((uint32_t)sector)->Frame3D == g_FrameCount;
}

CZona *Zona_FindNodo(CNodo3D *nodo, byte main) {
	for (int i = 0; i < g_NumZonas; i++) {
		CZona *z = g_Zonas[i];
		if (!main) {
			for (int k = 0; k < z->numNodos; k++) {
				if (z->nodos[k] == nodo) {
					return z;
				}
			}
		} else if (nodo == z->nodo) {
			return z;
		}
	}
	return NULL;
}

int Sector_OfNode(CNodo3D *nodo) {
	for (; nodo; nodo = nodo->padre) {
		int s = Sector_Find(nodo->name);
		if (s != -1) {
			return s;
		}
	}
	return -1;
}

int Sector_Find(const char *name) {
	for (int i = 0; i < g_NumZonas; i++) {
		if (!strcmp(name, g_Zonas[i]->nodo->name)) {
			return g_Zonas[i]->id;
		}
	}
	return -1;
}

CZona *Sector_Get(uint32_t sector) {
	CHECK(sector < (uint32_t)g_NumZonas);
	CHECK(g_Zonas[sector]);
	return g_Zonas[sector];
}

int Sector_Num(void) {
	return g_NumZonas;
}

int Sector_OfZonaNodo(CNodo3D *nodo) {
	CZona *z = Zona_FindNodo(nodo, 0);
	return z ? z->id : -1;
}

void Sectores_Free(void) {
	for (int i = 0; i < g_NumZonas; i++) {
		CHECK(g_Zonas[i]);
		CZona *z = g_Zonas[i];
		if (z) {
			CZona_dtor(z);
			operator_delete_sized(z, sizeof(CZona));
			g_Zonas[i] = NULL;
		}
	}
	g_NumZonas = 0;
}

byte Portal_Project(const vec3_t *p, float *out, float *w) {
	matrix_t vp;
	Matrix_MulD3D(&vp, Render_GetView(), Render_GetProj());
	const float *m = vp.m;
	float x = p->x * m[0] + p->y * m[4] + p->z * m[8] + m[12];
	float y = p->x * m[1] + p->y * m[5] + p->z * m[9] + m[13];
	float h = p->x * m[3] + p->y * m[7] + p->z * m[11] + m[15];
	float ax = (float)__builtin_fabs((double)x);
	float ay = (float)__builtin_fabs((double)y);
	byte front = 1;
	if (!(ax >= h) && !(ay >= h)) {
		x = x / h;
		y = y / h;
	} else {
		front = 0;
		if (ay + ax == 0.0f) {
			x = 0.0f;
			y = 1.0f;
		} else {
			float k = 1.0f / (ax > ay ? ax : ay);
			y = y * k;
			x = x * k;
		}
	}
	if (out) {
		out[0] = x;
		out[1] = y;
	}
	if (w) {
		*w = h;
	}
	return front;
}

byte __thiscall SectorVis_Sphere(SectorVis *self, const vec3_t *pos, const vec3_t *center, float radius,
	const char *name) {
	float scr[2], w;

	if (0.0f >= radius) {
		E3D_LogVerbose(g_FileLog, "WARN: Portals_IsVisible: Invalid radius: %f [%s]", (double)radius, name);
	}
	CHECK(radius > 0.0f);
	if (!SectorVis_Update(self, pos, name)) {
		return 0;
	}
	if (self->sector < 0 || !g_PortalTest) {
		return 1;
	}
	Portal_Project(center, scr, &w);
	byte seen = 1;
	box_t r = g_Zonas[self->sector]->rect;
	if (w > radius) {
		float k = radius / w;
		float x0 = scr[0] - k, x1 = scr[0] + k;
		float ky = k * PORTAL_ASPECT;
		float y1 = ky + scr[1], y0 = scr[1] - ky;
		box_t esfera = { { x0, y0, 0.0f }, { x1, y1, 1.0f } };
		r.min.x = r.min.x > x0 ? r.min.x : x0;
		r.max.x = r.max.x < x1 ? r.max.x : x1;
		r.max.y = r.max.y < y1 ? r.max.y : y1;
		r.min.y = r.min.y > y0 ? r.min.y : y0;
		if (r.max.x - r.min.x > 0.0f && r.max.y - r.min.y > 0.0f) {
			seen = 1;
		} else {
			seen = 0;
			r = esfera;
		}
	}
	if (g_PortalTest > 1) {
		uint32_t view[6];
		void *dev = g_pd3dDevice;
		D3D_METHOD_VP(dev)(dev, view);
		int x0 = Portal_Pixel(r.min.x + 1.0f, view[2], view[0]);
		int y0 = Portal_Pixel(1.0f - r.min.y, view[3], view[1]) - 1;
		int y1 = Portal_Pixel(1.0f - r.max.y, view[3], view[1]);
		int x1 = Portal_Pixel(r.max.x + 1.0f, view[2], view[0]) - 1;
		uint32_t color = !seen ? 0xffff00 : radius < w ? 0xffffff : 0xff00ff;
		Debug_Line(x0, y0, x1, y0, color, 0);
		Debug_Line(x1, y0, x1, y1, color, 0);
		Debug_Line(x1, y1, x0, y1, color, 0);
		Debug_Line(x0, y1, x0, y0, color, 0);
	}
	return seen;
}

int Sector_Recorrer(int sector, const vec3_t *a, const vec3_t *b, byte front) {
	CHECK((uint32_t)sector < (uint32_t)g_NumZonas);
	CHECK(g_Zonas[sector]);
	if (a->x == b->x && a->y == b->y && a->z == b->z) {
		return sector;
	}
	for (int i = 0; i < g_Zonas[sector]->numPortales; i++) {
		CZona *z = g_Zonas[sector];
		CPortal *po = Obj3D_Cast(z->portales[i]->obj3D, CLASE_OBJ3D_PORTAL);
		vec3_t h;
		if (!CPortal_SegCross(po, a, b, &h)) {
			continue;
		}
		if (front) {
			float sign = po->zonas[0] == g_Zonas[sector]->nodo ? -1.0f : 1.0f;
			if (CPortal_PlaneDist(po, a) * sign > 0.0f) {
				continue;
			}
		}
		CZona *dest = g_Zonas[sector]->zonas[i];
		if (!dest) {
			continue;
		}
		vec3_t d = { b->x - a->x, b->y - a->y, b->z - a->z };
		float len = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z));
		vec3_t dir = d;
		if ((double)len != 0.0) {
			float k = (float)(1.0 / (double)len);
			dir = (vec3_t){ d.x * k, d.y * k, d.z * k };
		}
		/* out of the portal by a step that grows with the distance to the origin */
		float eps = (float)__builtin_sqrt((double)(h.y * h.y + h.x * h.x + h.z * h.z)) * PORTAL_EPS;
		h = (vec3_t){ dir.x * eps + h.x, dir.y * eps + h.y, dir.z * eps + h.z };
		byte n = 0;
		while (CPortal_SegCross(po, &h, b, NULL)) {
			if (n++ >= 10) {
				break;
			}
			eps = (float)((double)eps + (double)eps);
			h = (vec3_t){ dir.x * eps + h.x, dir.y * eps + h.y, dir.z * eps + h.z };
			const char *sh = Vec3_Str(&h, 3, 6), *sd = Vec3_Str(&dir, 2, 6), *sb = Vec3_Str(b, 1, 6);
			const char *sa = Vec3_Str(a, 0, 6);
			Con_Warnf("WARN: Portal_calc_corte: c=%f v1=%s v2=%s d=%s p=%s n=%s\n", (double)eps, sa, sb, sd, sh,
				po->nodo->name);
		}
		if (n >= 10) {
			return dest->id;
		}
		float dx = h.x - b->x, dy = h.y - b->y, dz = h.z - b->z;
		float dist = dy * dy + dx * dx + dz * dz;
		if ((double)(eps * eps) * 4.0 > (double)dist) {
			return dest->id;
		}
		return Sector_Recorrer(dest->id, &h, b, front);
	}
	return sector;
}
