/*
 * Engine/SueloPortal.cpp of the engine3d: the sectors and their lookups, see include/scrap/groundportal.h.
 */
#include <string.h>

#include "scrap/rdev.h"
#include "scrap/gvar.h"
#include "scrap/camera.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/fonts.h"
#include "scrap/e3dworld.h"
#include "scrap/portal.h"
#include "scrap/render.h"
#include "scrap/particlesystem.h"
#include "scrap/groundportal.h"
#include "scrap/text.h"
#include "scrap/misc.h"
#include "scrap/staticinit.h"

#define g_SectorCache (GVar_R_PortalPos.value.i)	/* the cached sector is used when the position is the same */
#define g_PortalTest (GVar_R_PortalExt.value.i)	/* the screen rectangle test; 2: drawn */
#define g_PortalFront (GVar_R_PortalNorm.value.i)	/* portals the camera is in front of beyond the near distance are skipped */
#define g_PortalExterior (GVar_R_PortalEnd.value.i)	/* exterior sectors do not look into interior ones */
#define g_PortalNear (GVar_R_PortalDist.value.f)	/* the near distance of the portals, times zNear / 256 * 128 */
static cGVar GVar_R_PortalPos;
static cGVar GVar_R_PortalNorm;
static cGVar GVar_R_PortalExt;
static cGVar GVar_R_PortalEnd;
static cGVar GVar_R_PortalDist;
#define PORTAL_ASPECT 1.33f			/* the screen height of a sphere */
#define PORTAL_EPS 8.0e-6f			/* the step out of a portal, times the distance to the origin */
#define MAX_ZONES 0x80

static CZone *g_Zones[128];
static int g_NumZones;
static int g_PortalUnion;
static int g_NumEndSectors;
static int g_NumOutSectors;

/* the sector counts by their name tags (the sky ones are g_PortalUnion) */
static cGVar GVar_R_PortalClip;

/* the projection and the viewport narrowed to the rectangle of each seen sector */
#define g_PortalClip (GVar_R_PortalClip.value.i)
static cGVar GVar_R_PortalBand;
#define g_PortalBand (GVar_R_PortalBand.value.i)	/* pixels added around it */

static CZone *CZone_ctor(CZone *self) {
	self->rect = (box_t){ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	self->id = -1;
	self->locked = 0;
	self->Frame3D = 0;
	self->node = NULL;
	self->numNodes = 0;
	self->numPortals = 0;
	self->exterior = 0;
	self->sky = 0;
	self->outside = 0;
	self->nodes[0] = NULL;
	self->nodes[1] = NULL;
	memset(self->portals, 0, sizeof(self->portals));
	memset(self->zones, 0, sizeof(self->zones));
	return self;
}

static void CZone_dtor(CZone *self) {
	if (self->node) {
		ObjRef_Release(&self->node);
	}
	self->node = NULL;
	for (int i = 0; i < self->numNodes; i++) {
		if (self->nodes[i]) {
			ObjRef_Release(&self->nodes[i]);
		}
		self->nodes[i] = NULL;
	}
	for (int i = 0; i < self->numPortals; i++) {
		if (self->portals[i]) {
			ObjRef_Release(&self->portals[i]);
		}
		self->portals[i] = NULL;
	}
}

void SectorVis_Draw(SectorVis *self, const vec3_t *pos, byte visible, byte cached, const char *name) {
	uint32_t off, on;

	(void)name;
	if (!(self->flags & SECTORVIS_VALID)) {
		Debug_Text3D(g_FntComic14, 0.0f, 0.0f, pos, 0x808080, "[-]");
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
	Debug_Text3D(g_FntComic14, 0.0f, 0.0f, pos, color, "%d%s", self->sector,	/* "%d%s" */
		(self->flags & SECTORVIS_CACHED) ? str_Empty : "*");	/* "", "*" */
}

/* the cached sector is seen (none counts as seen) */
static CZone *Sector_Get(uint32_t sector);
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

byte SectorVis_Check(SectorVis *self, const vec3_t *pos, const char *name) {
	if (!(self->flags & SECTORVIS_VALID)) {
		return SectorVis_Update(self, pos, name);
	}
	if (SectorVis_SamePos(self, pos)) {
		return SectorVis_Seen(self);
	}
	self->flags &= ~SECTORVIS_CACHED;
	return 1;
}

byte SectorVis_Update(SectorVis *self, const vec3_t *pos, const char *name) {
	if ((self->flags & SECTORVIS_VALID) && SectorVis_SamePos(self, pos)) {
		return SectorVis_Seen(self);
	}
	self->sector = -2;
	self->flags = (self->flags & ~SECTORVIS_CACHED) | SECTORVIS_VALID;
	self->pos = *pos;
	return Sector_FindPos(&self->pos, &self->sector, name);
}

byte Sector_FindPos(const vec3_t *pos, int *sector, const char *name) {
	GroundRay res;

	(void)name;
	if (!g_PortalsOn) {
		return 1;
	}
	CHECK(g_World);
	void *ground = g_World->ObjGround;
	CHECK(ground);
	res.flags = 0x202;
	res.y = pos->y;
	if (!CGround_RayVert(ground, pos->x, pos->z, &res)) {
		return 1;
	}
	int s = res.sector;
	byte seen = 1;
	if (s != -1) {
		vec3_t a = { pos->x, res.hitY, pos->z };
		s = Sector_Traverse(s, &a, pos, 0);
		seen = Sector_Get((uint32_t)s)->Frame3D == g_FrameCount;
	}
	if (sector) {
		*sector = s;
	}
	return seen;
}

const char *Sector_GetName(int sector) {
	if (sector == -1) {
		return "_unasigned_";
	}
	return Sector_Get((uint32_t)sector)->node->name;
}

byte Sector_IsOutside(int sector) {
	if (sector == -1) {
		return 0;
	}
	return Sector_Get((uint32_t)sector)->outside;
}

byte Sector_IsVisible(int sector) {
	return Sector_Get((uint32_t)sector)->Frame3D == g_FrameCount;
}

int Sector_OfNode(CNode3D *node) {
	for (; node; node = node->parent) {
		int s = Sector_Find(node->name);
		if (s != -1) {
			return s;
		}
	}
	return -1;
}

int Sector_Find(const char *name) {
	for (int i = 0; i < g_NumZones; i++) {
		if (!strcmp(name, g_Zones[i]->node->name)) {
			return g_Zones[i]->id;
		}
	}
	return -1;
}

static CZone *Sector_Get(uint32_t sector) {
	CHECK(sector < (uint32_t)g_NumZones);
	CHECK(g_Zones[sector]);
	return g_Zones[sector];
}

int Sector_Num(void) {
	return g_NumZones;
}

void Sectors_FreeAll(void) {
	for (int i = 0; i < g_NumZones; i++) {
		CHECK(g_Zones[i]);
		CZone *z = g_Zones[i];
		if (z) {
			CZone_dtor(z);
			operator_delete_sized(z, sizeof(CZone));
			g_Zones[i] = NULL;
		}
	}
	g_NumZones = 0;
}

/* the screen position (-1..1) of p and its w; 0 outside the view (out then the direction to p on the border) */
static byte Portal_Project(const vec3_t *p, float *out, float *w) {
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

byte SectorVis_Sphere(SectorVis *self, const vec3_t *pos, const vec3_t *center, float radius,
	const char *name) {
	float scr[2], w;

	if (0.0f >= radius) {
		E3D_LogVerbose((&g_FileLog), "WARN: Portals_IsVisible: Invalid radius: %f [%s]", (double)radius, name);
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
	box_t r = g_Zones[self->sector]->rect;
	if (w > radius) {
		float k = radius / w;
		float x0 = scr[0] - k, x1 = scr[0] + k;
		float ky = k * PORTAL_ASPECT;
		float y1 = ky + scr[1], y0 = scr[1] - ky;
		box_t sphere = { { x0, y0, 0.0f }, { x1, y1, 1.0f } };
		r.min.x = r.min.x > x0 ? r.min.x : x0;
		r.max.x = r.max.x < x1 ? r.max.x : x1;
		r.max.y = r.max.y < y1 ? r.max.y : y1;
		r.min.y = r.min.y > y0 ? r.min.y : y0;
		if (r.max.x - r.min.x > 0.0f && r.max.y - r.min.y > 0.0f) {
			seen = 1;
		} else {
			seen = 0;
			r = sphere;
		}
	}
	if (g_PortalTest > 1) {
		uint32_t view[6];
		void *dev = g_pd3dDevice;
		Rd_DevGetViewport(dev, view);
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

int Sector_Traverse(int sector, const vec3_t *a, const vec3_t *b, byte front) {
	CHECK((uint32_t)sector < (uint32_t)g_NumZones);
	CHECK(g_Zones[sector]);
	if (a->x == b->x && a->y == b->y && a->z == b->z) {
		return sector;
	}
	for (int i = 0; i < g_Zones[sector]->numPortals; i++) {
		CZone *z = g_Zones[sector];
		CPortal *po = Obj3D_Cast(z->portals[i]->obj3D, CLASS_OBJ3D_PORTAL);
		vec3_t h;
		if (!CPortal_SegCross(po, a, b, &h)) {
			continue;
		}
		if (front) {
			float sign = po->zones[0] == g_Zones[sector]->node ? -1.0f : 1.0f;
			if (CPortal_PlaneDist(po, a) * sign > 0.0f) {
				continue;
			}
		}
		CZone *dest = g_Zones[sector]->zones[i];
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
				po->node->name);
		}
		if (n >= 10) {
			return dest->id;
		}
		float dx = h.x - b->x, dy = h.y - b->y, dz = h.z - b->z;
		float dist = dy * dy + dx * dx + dz * dz;
		if ((double)(eps * eps) * 4.0 > (double)dist) {
			return dest->id;
		}
		return Sector_Traverse(dest->id, &h, b, front);
	}
	return sector;
}

int g_NumPaintZones;

/* sector seen through rect, and the sectors behind its portals through their part of it */
static void Zone_Paint(uint32_t zone, const box_t *rect) {
	CHECK(zone < (uint32_t)g_NumZones);
	CHECK(g_Zones[zone]);
	CHECK(g_Zones[zone]->locked == 0);
	g_Zones[zone]->locked = 1;
	int i;
	for (i = 0; i < g_NumPaintZones; i++) {
		if ((uint32_t)g_PaintZones[i].zone == zone) {
			break;
		}
	}
	if (i < g_NumPaintZones) {
		PaintZone *e = &g_PaintZones[i];
		CHECK(g_Zones[zone]->Frame3D == g_FrameCount);
		e->rect.min.x = Float_MinSS(e->rect.min.x, rect->min.x);
		e->rect.min.z = Float_MinSS(e->rect.min.z, rect->min.z);
		e->rect.min.y = Float_MinSS(e->rect.min.y, rect->min.y);
		e->rect.max.z = Float_MaxSS(e->rect.max.z, rect->max.z);
		e->rect.max.x = Float_MaxSS(e->rect.max.x, rect->max.x);
		e->rect.max.y = Float_MaxSS(e->rect.max.y, rect->max.y);
		e->times++;
		g_Zones[zone]->rect = e->rect;
	} else {
		CHECK(g_NumPaintZones < MAX_PAINT_ZONES);
		CHECK(g_Zones[zone]->Frame3D != g_FrameCount);
		g_Zones[zone]->Frame3D = g_FrameCount;
		g_Zones[zone]->rect = *rect;
		PaintZone *e = &g_PaintZones[g_NumPaintZones++];
		e->zone = (int)zone;
		e->rect = *rect;
		e->times = 1;
		if (g_ShowPortals) {
			CZone *z = g_Zones[zone];
			uint32_t color = z->exterior ? 0x80 : 0x800000;
			for (int k = 0; k < g_Zones[zone]->numPortals; k++) {
				CPortal_DrawDebug(Obj3D_Cast(g_Zones[zone]->portals[k]->obj3D, CLASS_OBJ3D_PORTAL), color);
			}
		}
	}
	for (int k = 0; k < g_Zones[zone]->numPortals; k++) {
		CPortal *po = Obj3D_Cast(g_Zones[zone]->portals[k]->obj3D, CLASS_OBJ3D_PORTAL);
		CZone *dest = g_Zones[zone]->zones[k];
		CHECK(dest);
		if (g_Zones[zone]->exterior && !dest->exterior && g_PortalExterior) {
			continue;
		}
		if (dest->locked) {
			continue;
		}
		box_t r = { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
		CHECK(g_World);
		CCamera *cam = g_World->cam;
		float nearDist = (float)((double)(cam->zNear * (1.0f / 256.0f)) * 128.0 * (double)g_PortalNear);
		if (g_PortalFront) {
			float sign = po->zones[0] == g_Zones[zone]->node ? -1.0f : 1.0f;
			if (CPortal_PlaneDist(po, Render_GetEyePos()) * sign > nearDist) {
				continue;
			}
		}
		float dist = CPortal_Dist(po, Render_GetEyePos());
		if (nearDist > dist) {
			r = *rect;
		} else {
			if (!CPortal_ClipRect(po, &r)) {
				continue;
			}
			r.min.x = Float_MaxSS(r.min.x, rect->min.x);
			r.min.y = Float_MaxSS(r.min.y, rect->min.y);
			r.min.z = Float_MaxSS(r.min.z, rect->min.z);
			r.max.y = Float_MinSS(r.max.y, rect->max.y);
			r.max.z = Float_MinSS(r.max.z, rect->max.z);
			r.max.x = Float_MinSS(r.max.x, rect->max.x);
		}
		if (!(0.0f >= r.max.x - r.min.x) && !(0.0f >= r.max.y - r.min.y)) {
			Zone_Paint((uint32_t)dest->id, &r);
			if (g_ShowPortals) {
				CPortal_DrawDebug(po, 0x8000);
			}
		}
	}
	g_Zones[zone]->locked = 0;
}

static void Portals_DebugDraw(void);
void Portals_Cull(uint32_t sector) {
	if (!g_NumZones) {
		return;
	}
	CHECK(sector < (uint32_t)g_NumZones);
	box_t all = { { -1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } };
	g_NumPaintZones = 0;
	Zone_Paint(sector, &all);
	if (g_PortalUnion) {
		g_PaintNum = 0;
		g_PaintI148 = -1;
		g_PaintRect = (box_t){ { 1.0f, 1.0f, 1.0f }, { -1.0f, -1.0f, 0.0f } };
		int n = 0;
		for (int i = 0; i < g_NumPaintZones; i++) {
			PaintZone *e = &g_PaintZones[i];
			if (!g_Zones[e->zone]->sky) {
				continue;
			}
			box_t *u = &g_PaintRect;
			g_PaintNum = ++n;
			u->min.x = Float_MinSS(u->min.x, e->rect.min.x);
			u->min.y = Float_MinSS(u->min.y, e->rect.min.y);
			u->min.z = Float_MinSS(u->min.z, e->rect.min.z);
			u->max.y = Float_MaxSS(u->max.y, e->rect.max.y);
			u->max.z = Float_MaxSS(u->max.z, e->rect.max.z);
			u->max.x = Float_MaxSS(u->max.x, e->rect.max.x);
		}
	}
	Portals_DebugDraw();
}

PaintZone *PaintZone_ctor(PaintZone *self) {
	self->rect = (box_t){ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	return self;
}

void PaintZone_dtor(PaintZone *self) {
	(void)self;
}

PaintZone g_PaintAll;

/* the static initializer (rva 0x05d6f0, the constructor inlined) and the atexit destructor of
 * (&g_PaintAll) */
static void GroundPortal_FiniPaintAll(void) {
	PaintZone_dtor((&g_PaintAll));
}

void GroundPortal_InitPaintAll(void) {
	PaintZone_ctor((&g_PaintAll));
	crt_atexit(GroundPortal_FiniPaintAll);
}

/* the debug drawing of the seen sectors (gvRenderShowPortals 2, 3 also their rectangles as text) */
static void Portals_DebugDraw(void) {
	uint32_t vp[6];
	void *dev;

	if (g_ShowPortals < 2) {
		return;
	}
	dev = g_pd3dDevice;
	Rd_DevGetViewport(dev, vp);
	Text_Printf(300, 0x14, "zonas: %d", g_NumPaintZones);	/* "zonas: %d" */
	for (int i = 0; i < g_NumPaintZones; i++) {
		const PaintZone *zp = &g_PaintZones[i];
		const box_t *r = &zp->rect;
		if (g_ShowPortals > 2) {
			Text_Printf(300, 0x22 + i * 0xe, "  %d: %d %d   %.3f %.3f %.3f   %.3f %.3f %.3f", i, zp->zone, zp->times, (double)r->min.x,
				(double)r->min.y, (double)r->min.z, (double)r->max.x, (double)r->max.y, (double)r->max.z);
		}
		int x0 = Portal_Pixel(r->min.x + 1.0f, vp[2], vp[0]) + 1;
		int y0 = Portal_Pixel(1.0f - r->min.y, vp[3], vp[1]) - 2;
		int x1 = Portal_Pixel(r->max.x + 1.0f, vp[2], vp[0]) - 2;
		int y1 = Portal_Pixel(1.0f - r->max.y, vp[3], vp[1]) + 1;
		if (i) {
			Debug_Line(x0, y0, x1, y0, 0xff, 0);
			Debug_Line(x1, y0, x1, y1, 0xff, 0);
			Debug_Line(x1, y1, x0, y1, 0xff, 0);
			Debug_Line(x0, y1, x0, y0, 0xff, 0);
		}
	}
}

/* the sector of the node, NULL */
static CZone *Zone_OfNode(const CNode3D *node) {
	for (int i = 0; i < g_NumZones; i++) {
		if (g_Zones[i]->node == node) {
			return g_Zones[i];
		}
	}
	return NULL;
}

static const char *Node_NameOrNull(const CNode3D *n) {
	return n ? n->name : "(null)";
}

void Sectors_Create(CScene *scn) {
	CNode3D *n;

	g_PaintRect = (box_t){ { -1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } };
	g_NumZones = 0;
	g_NumPaintZones = 0;
	g_PortalUnion = 0;
	g_NumEndSectors = 0;
	g_NumOutSectors = 0;
	g_PaintNum = 1;
	g_PaintI148 = -1;
	/* a sector for each node a portal joins */
	for (n = scn->Node3D; n; n = n->next) {
		if (CNode3D_Class(n) != CLASS_OBJ3D_PORTAL) {
			continue;
		}
		CPortal *po = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_PORTAL);
		CPortal_LinkZones(po);
		for (int side = 0; side < 2; side++) {
			CNode3D *zn = po->zones[side & 1];
			if (!zn || Zone_OfNode(zn)) {
				continue;
			}
			CZone *z = operator_new(sizeof(CZone));
			z = z ? CZone_ctor(z) : NULL;
			z->id = g_NumZones;
			zn->Refs++;
			ObjRef_Release(&z->node);
			z->node = zn;
			CHECK(g_NumZones < MAX_ZONES);
			g_Zones[g_NumZones++] = z;
			if (strstr(zn->name, "(+end_sector)")) {
				g_NumEndSectors++;
				z->exterior = 1;
			} else {
				z->exterior = 0;
			}
			if (strstr(zn->name, "(+sky_sector)")) {
				g_PortalUnion++;
				z->sky = 1;
			} else {
				z->sky = 0;
			}
			if (strstr(zn->name, "(+out_sector)")) {
				g_NumOutSectors++;
				z->outside = 1;
			} else {
				z->outside = 0;
			}
		}
	}
	/* the portals into the sectors on both sides */
	for (n = scn->Node3D; n; n = n->next) {
		if (CNode3D_Class(n) != CLASS_OBJ3D_PORTAL) {
			continue;
		}
		CPortal *po = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_PORTAL);
		if (po->zones[0] == po->zones[1]) {
			continue;
		}
		for (uint32_t side = 0; side < 2; side++) {
			CNode3D *zn = po->zones[side & 1];
			if (!zn) {
				continue;
			}
			CZone *z = Zone_OfNode(zn);
			if (!z || z->numPortals >= MAX_PORTALS) {
				CheckFailed("zona && zona->num_portales < MAX_PORTALES_ZONA", __FILE__, __TIMESTAMP__, __LINE__);
			}
			int k = z->numPortals;
			n->Refs++;
			ObjRef_Release(&z->portals[k]);
			z->portals[k] = n;
			z->zones[z->numPortals] = Zone_OfNode(po->zones[~side & 1]);
			z->numPortals++;
		}
	}
	Con_Verbosef("Sectors: %d\n", g_NumZones);
	Con_Verbosef("SkySectors: %d, EndSectors: %d, OutSectors: %d\n", g_PortalUnion, g_NumEndSectors, g_NumOutSectors);
	for (int i = 0; i < g_NumZones; i++) {
		CZone *z = g_Zones[i];
		CHECK(i == z->id);
		E3D_LogDebug2((&g_FileLog), "Sector: %s  Ident: %d", z->node->name, z->id);
		for (int k = 0; k < z->numNodes; k++) {
			E3D_LogDebug2((&g_FileLog), "  Node: %d  (%s)", k, z->nodes[k]->name);
		}
		for (int k = 0; k < z->numPortals; k++) {
			CNode3D *s1 = ((CPortal *)Obj3D_Cast(z->portals[k]->obj3D, CLASS_OBJ3D_PORTAL))->zones[0];
			CNode3D *s2 = ((CPortal *)Obj3D_Cast(z->portals[k]->obj3D, CLASS_OBJ3D_PORTAL))->zones[1];
			E3D_LogDebug2((&g_FileLog), "  Portal: %d  (%s)  Side1: '%s'  Side2: '%s'", k, z->portals[k]->name,
				Node_NameOrNull(s1), Node_NameOrNull(s2));
			if (!s1 || !s2) {
				E3D_LogBlue((&g_FileLog), "  WARN: Portal: %d  (%s)  Side1: '%s'  Side2: '%s'", k,
					z->portals[k]->name, Node_NameOrNull(s1), Node_NameOrNull(s2));
			}
		}
	}
}

/* the portal rectangle side r (-1..1) as a pixel of the viewport size n */
static int Portal_ViewPixel(float r, int n) {
	return Portal_Pixel(r, (uint32_t)n, 0);
}

static int Int_Clamp0(int v, int n) {
	if (v <= 0) {
		return 0;
	}
	return v >= n ? n : v;
}

byte Portals_SetView(const PaintZone *zp) {
	void *dev;
	matrix_t m;

	if (!g_PortalClip) {
		return 1;
	}
	if (!zp) {
		uint32_t vp[6];
		m = *Render_GetProj();
		dev = g_pd3dDevice;
		Rd_DevSetTransform(dev, D3DTS_PROJECTION, &m);
		dev = g_pd3dDevice;
		Rd_DevGetViewport(dev, vp);
		vp[0] = 0;
		vp[1] = 0;
		vp[2] = (uint32_t)Render_GetViewportW();
		vp[3] = (uint32_t)Render_GetViewportH();
		dev = g_pd3dDevice;
		Rd_DevSetViewport(dev, vp);
		return 1;
	}
	int w = Render_GetViewportW();
	int h = Render_GetViewportH();
	int pad = g_PortalBand;
	int x0 = Portal_ViewPixel(zp->rect.min.x + 1.0f, w) - pad;
	int y1 = pad + 1 + Portal_ViewPixel(1.0f - zp->rect.min.y, h);
	int x1 = Portal_ViewPixel(zp->rect.max.x + 1.0f, w) + 1 + pad;
	int y0 = Portal_ViewPixel(1.0f - zp->rect.max.y, h) - pad;
	x0 = Int_Clamp0(x0, w);
	y0 = Int_Clamp0(y0, h);
	x1 = Int_Clamp0(x1, w);
	y1 = Int_Clamp0(y1, h);
	int lx = x1 - x0;
	if (lx <= 0) {
		return 0;
	}
	int ly = y1 - y0;
	if (ly <= 0) {
		return 0;
	}
	if (g_ShowPortals > 2) {
		Text_PrintfRel(0, 0xe, "zona: %d  LX: %d  LY: %d", zp->zone, lx, ly);	/* "zona: %d  LX: %d  LY: %d" */
		Text_PrintfRel(0, 0xe, "  %d %d  %d %d", x0, y0, x1, y1);	/* "  %d %d  %d %d" */
	}
	uint32_t vp[6] = { (uint32_t)x0, (uint32_t)y0, (uint32_t)lx, (uint32_t)ly, 0, 0x3f800000 };
	dev = g_pd3dDevice;
	Rd_DevSetViewport(dev, vp);
	float fy0 = -((float)y0 / (float)h - 0.5f);
	float fx0 = (float)x0 / (float)w - 0.5f;
	float fx1 = (float)x1 / (float)w - 0.5f;
	float fy1 = -((float)y1 / (float)h - 0.5f);
	m = *Render_GetProj();
	m.m[0] = m.m[0] / (fx1 - fx0);
	m.m[5] = m.m[5] / (fy0 - fy1);
	m.m[8] = (fx1 + fx0) / (fx0 - fx1);
	m.m[9] = (fy1 + fy0) / (fy1 - fy0);
	dev = g_pd3dDevice;
	Rd_DevSetTransform(dev, D3DTS_PROJECTION, &m);
	return 1;
}

static cGVar GVar_R_PortalNorm;
static cGVar GVar_R_PortalPos;
static cGVar GVar_R_PortalDist;
static cGVar GVar_R_PortalExt;
static cGVar GVar_R_PortalEnd;
cGVar GVar_R_ShowPortals;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_R_PortalDist, &GVar_R_PortalDist, "R_PortalDist", 0, "Portal min. distance factor", 20.0f)
INIT_GVAR_INT(Init_GVar_R_PortalNorm, &GVar_R_PortalNorm, "R_PortalNorm", 0, "Use portal plane normal (0/1)", 1)
INIT_GVAR_INT(Init_GVar_R_PortalPos, &GVar_R_PortalPos, "R_PortalPos", 0, "Use sector position cache (0/1)", 1)
INIT_GVAR_INT(Init_GVar_R_PortalExt, &GVar_R_PortalExt, "R_PortalExt", 0, "Use sector visibility extension (0/1/2)", 1)
INIT_GVAR_INT(Init_GVar_R_PortalEnd, &GVar_R_PortalEnd, "R_PortalEnd", 0, "Use end sectors (0/1)", 1)
INIT_GVAR_INT(Init_GVar_R_PortalClip, &GVar_R_PortalClip, "R_PortalClip", 0, "Use portal viewport for clipping (0/1)", 1)
INIT_GVAR_INT(Init_GVar_R_PortalBand, &GVar_R_PortalBand, "R_PortalBand", 0, "Portal clipping viewport margin", 0)
INIT_GVAR_INT(Init_GVar_R_ShowPortals, &GVar_R_ShowPortals, "R_ShowPortals", 0, "Show portals (0/1/2/3)", 0)
