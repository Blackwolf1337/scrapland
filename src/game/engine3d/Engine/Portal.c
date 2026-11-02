/*
 * Engine/portal.cpp of the engine3d: CPortal, see include/scrap/portal.h.
 */
#include "scrap/rdev.h"
#include "scrap/e3dutil.h"
#include "scrap/e3dworld.h"
#include "scrap/portal.h"
#include "scrap/render.h"
#include "scrap/particlesystem.h"
#include "scrap/ground.h"
#include "scrap/utild3d.h"
#include "scrap/misc.h"

#define CPORTAL_VFTABLE (&CPortal_vftable)
static const IObject3DVtbl CPortal_vftable;

CPortal *CPortal_ctor(CPortal *self) {
	self->node = NULL;
	self->vftable = CPORTAL_VFTABLE;
	self->class = CLASS_OBJ3D_PORTAL;
	self->width = 1.0f;
	self->height = 1.0f;
	self->zones[0] = NULL;
	self->zones[1] = NULL;
	self->idZone[0] = -1;
	self->idZone[1] = -1;
	self->calculated = 0;
	return self;
}

static void CPortal_dtor(CPortal *self) {
	self->vftable = CPORTAL_VFTABLE;
	for (int i = 0; i < 2; i++) {
		ObjRef_Drop(&self->zones[i]);
		self->zones[i] = NULL;
	}
	CObjBase3D_dtor((CObjBase3D *)self);
}

static IObject3D *CPortal_Delete(IObject3D *base, byte flags) {
	CPortal *self = (CPortal *)base;
	CPortal_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CPortal));
	}
	return base;
}

void CPortal_LinkZones(CPortal *self) {
	if (self->zones[0] || self->zones[1]) {
		return;
	}
	for (CNode3D *n = self->node->scene->Node3D; n; n = n->next) {
		if (n->slot < 0) {
			continue;
		}
		for (int i = 0; i < 2; i++) {
			if (n->slot == self->idZone[i]) {
				n->Refs++;
				ObjRef_Drop(&self->zones[i]);
				self->zones[i] = n;
			}
		}
	}
}

static byte CPortal_Read(IObject3D *base, CFileIO *f, MeshD3D_FileCtx *ctx) {
	CPortal *self = (CPortal *)base;
	CObjBase3D_SetNode((CObjBase3D *)self, ctx->node);
	CFileIO_BeginBlock(f, "PORT");
	CHECK(CFileIO_ReadInt(f) == 1);
	self->idZone[0] = CFileIO_ReadInt(f);
	self->idZone[1] = CFileIO_ReadInt(f);
	CFileIO_Read(f, &self->width, 4);
	CFileIO_Read(f, &self->height, 4);
	self->calculated = 0;
	CFileIO_Error(f, "PORT");
	return 1;
}

static byte CPortal_Write(IObject3D *base, CFileIO *f, MeshD3D_FileCtx *ctx) {
	CPortal *self = (CPortal *)base;
	CHECK(self->node == ctx->node);
	CFileIO_BeginBlockWrite(f, "PORT");
	CFileIO_WriteInt(f, 1);
	CFileIO_WriteInt(f, self->zones[0] ? self->zones[0]->slot : -1);
	CFileIO_WriteInt(f, self->zones[1] ? self->zones[1]->slot : -1);
	CFileIO_Write(f, &self->width, 4);
	CFileIO_Write(f, &self->height, 4);
	CFileIO_EndBlockWrite(f, "PORT");
	return 1;
}

static void CPortal_Render(IObject3D *base, void *dev, CRenderNode *r) {
	CPortal *self = (CPortal *)base;
	(void)dev;
	CHECK(!r);
	CHECK(self->node);
	if (g_ShowPortals) {
		CPortal_DrawDebug(self, 0x808080);
	}
}

/* the material of the debug drawing, a static of CPortal_DrawDebug */

static CMaterial g_PortalMat;

/* the atexit destructor of it */
static void Portal_FiniMat(void) {
	CMaterial_dtor(&g_PortalMat);
}

static int g_PortalMatGuard[2];

void CPortal_DrawDebug(CPortal *self, uint32_t color) {
	CHECK(g_ShowPortals);
	float hw = self->width * 0.5f, hh = self->height * 0.5f;
	if (*g_PortalMatGuard > crt_InitEpoch()) {
		crt_Init_thread_header(g_PortalMatGuard);
		if (*g_PortalMatGuard == -1) {
			CMaterial_ctor(&g_PortalMat);
			crt_atexit(Portal_FiniMat);
			crt_Init_thread_footer(g_PortalMatGuard);
		}
	}
	g_PortalMat.params.lighting = 0;
	g_PortalMat.params.zFunc = 8;
	g_MatFX1 = &g_PortalMat;
	vec3_t mn = { -hw, 0.0f, -hh }, mx = { hw, 0.0f, hh };
	Debug_DrawBoxM(&self->node->m, &mn, &mx, color);
	g_MatFX1 = NULL;
}

/* the signed distance of p to the cached plane */
static float Portal_Plane(const CPortal *self, const vec3_t *p) {
	const plane_t *pl = &self->cache_plane;
	return pl->n.y * p->y + p->x * pl->n.x + pl->n.z * p->z + pl->d;
}

/* p is in the rectangle (its two triangles) */
static byte Portal_Inside(const CPortal *self, const vec3_t *p) {
	const vec3_t *c = self->cache_vert;
	return Ground_PointInTri(p, &c[0], &c[1], &c[2]) || Ground_PointInTri(p, &c[0], &c[2], &c[3]);
}

static void CPortal_UpdateCache(CPortal *self, const matrix_t *unused);
float CPortal_PlaneDist(CPortal *self, const vec3_t *p) {
	CPortal_UpdateCache(self, &self->node->m);
	const plane_t *pl = &self->cache_plane;
	return pl->n.y * p->y + pl->n.x * p->x + pl->n.z * p->z + pl->d;
}

float CPortal_Dist(CPortal *self, const vec3_t *p) {
	CHECK(self->node);
	CPortal_UpdateCache(self, &self->node->m);
	const plane_t *pl = &self->cache_plane;
	float d = Portal_Plane(self, p);
	vec3_t q = { p->x - pl->n.x * d, p->y - pl->n.y * d, p->z - pl->n.z * d };
	if (Portal_Inside(self, &q)) {
		return (float)__builtin_fabs((double)d);
	}
	const vec3_t *c = self->cache_vert;
	float m = Ground_Dist2PointSeg(&c[3], &c[0], p, NULL);
	for (int i = 0; i < 3; i++) {
		float e = Ground_Dist2PointSeg(&c[i], &c[i + 1], p, NULL);
		m = e <= m ? e : m;
	}
	return (float)__builtin_sqrt((double)m);
}

byte CPortal_SegCross(CPortal *self, const vec3_t *a, const vec3_t *b, vec3_t *out) {
	CHECK(self->node);
	CPortal_UpdateCache(self, &self->node->m);
	const plane_t *pl = &self->cache_plane;
	float da = pl->n.x * a->x + a->y * pl->n.y + a->z * pl->n.z + pl->d;
	float db = b->x * pl->n.x + b->y * pl->n.y + b->z * pl->n.z + pl->d;
	if ((db < 0.0f) == (da < 0.0f)) {
		return 0;
	}
	float t = da / (da - db);
	vec3_t q = { t * (b->x - a->x) + a->x, t * (b->y - a->y) + a->y, (b->z - a->z) * t + a->z };
	if (!Portal_Inside(self, &q)) {
		return 0;
	}
	if (out) {
		*out = q;
	}
	return 1;
}

void CPortal_SetZone(CPortal *self, uint32_t i, CNode3D *zone) {
	CNode3D **slot = &self->zones[i & 1];
	if (zone) {
		zone->Refs++;
	}
	ObjRef_Drop(slot);
	*slot = zone;
}

/* m: the matrix of the node, passed but not used */
static void CPortal_UpdateCache(CPortal *self, const matrix_t *unused) {
	(void)unused;
	if (self->calculated) {
		return;
	}
	float hx = self->width * 0.5f;
	float hz = self->height * 0.5f;
	self->calculated = 1;
	const matrix_t *m = &self->node->m;
	vec3_t v = { -hx, 0.0f, hz };
	Vec3_TransformH(&self->cache_vert[0], &v, m);
	v = (vec3_t){ hx, 0.0f, hz };
	Vec3_TransformH(&self->cache_vert[1], &v, m);
	v = (vec3_t){ hx, 0.0f, -hz };
	Vec3_TransformH(&self->cache_vert[2], &v, m);
	v = (vec3_t){ -hx, 0.0f, -hz };
	Vec3_TransformH(&self->cache_vert[3], &v, m);
	CHECK(Plane_Calc(&self->cache_plane, &self->cache_vert[0], &self->cache_vert[1], &self->cache_vert[2]));
	float a = (float)(acos(__builtin_fabs((double)self->cache_plane.n.y)) * 180.0 / 3.141592653589793);
	if (a > 89.9f && 91.1f > a && a != 90.0f) {
		E3D_LogInfo((&g_FileLog), "WARN: unaligned vertical portal: %s", self->node->name);
	}
}

static int Portal_Clamp(int v, int n) {
	if (v < 1) {
		return 0;
	}
	return n - 1 <= v ? n - 1 : v;
}

/* a screen rectangle (-1..1) drawn in the viewport vp (x, y, w, h) for the debug output */
static void Portal_DrawRect(const int *vp, const float *rect, uint32_t color) {
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

/* the screen box of the clip space points: x, y and z divided by w */
static void Portal_Extend(box_t *b, float x, float y, float z, float w) {
	x = x / w;
	y = y / w;
	z = z / w;
	b->min.x = Float_MinSS(b->min.x, x);
	b->min.y = Float_MinSS(b->min.y, y);
	b->min.z = Float_MinSS(b->min.z, z);
	b->max.x = Float_MaxSS(b->max.x, x);
	b->max.y = Float_MaxSS(b->max.y, y);
	b->max.z = Float_MaxSS(b->max.z, z);
}

byte CPortal_ClipRect(CPortal *self, box_t *rect) {
	vec3_t c[4], p[4];
	float w[4];
	byte back[4];

	CHECK(self->node);
	CPortal_UpdateCache(self, &self->node->m);
	for (int i = 0; i < 4; i++) {
		c[i] = self->cache_vert[i];
	}
	/* the projection to the near plane of the world camera, scaled down, and to its far plane */
	matrix_t proj = *Render_GetProj();
	CCamera *cam = World_Get()->cam;
	float zfar = cam->zFar;
	float znear = cam->zNear * 0.00390625f;
	proj.m[10] = zfar / (zfar - znear);
	proj.m[14] = -(proj.m[10] * znear);
	matrix_t tmp;
	const float *m = Matrix_MulD3D(&tmp, Render_GetView(), &proj)->m;
	box_t b = { { 3.40282347e+38f, 3.40282347e+38f, 3.40282347e+38f },
		{ -3.40282347e+38f, -3.40282347e+38f, -3.40282347e+38f } };
	int nback = 0, nfar = 0;
	for (int i = 0; i < 4; i++) {
		float x = c[i].x, y = c[i].y, z = c[i].z;
		p[i].x = y * m[4] + x * m[0] + z * m[8] + m[12];
		p[i].y = m[5] * y + m[1] * x + m[9] * z + m[13];
		p[i].z = m[6] * y + m[2] * x + m[10] * z + m[14];
		w[i] = m[7] * y + m[3] * x + m[11] * z + m[15];
		back[i] = 0.0f >= p[i].z;
		nback += back[i];
	}
	for (int i = 0; i < 4; i++) {
		nfar += p[i].z > zfar;
	}
	if (nback == 4 || nfar == 4) {
		return 0;
	}
	/* the corners in front, and where the edges cross the near plane */
	for (int i = 0; i < 4; i++) {
		int j = (i + 1) & 3;
		const vec3_t *a = &p[i], *n = &p[j];
		if (!back[i]) {
			CHECK(w[i] > 0.0f);
			Portal_Extend(&b, a->x, a->y, a->z, w[i]);
		} else if (back[j]) {
			continue;
		}
		if (back[i] == back[j]) {
			continue;
		}
		float t = a->z / (a->z - n->z);
		float x = (n->x - a->x) * t, y = (n->y - a->y) * t, z = (n->z - a->z) * t;
		CHECK(znear > 0.0f);
		Portal_Extend(&b, x + a->x, y + a->y, z + a->z, znear);
	}
	if (rect) {
		*rect = b;
	}
	if (g_ShowPortals > 1) {
		int vp[6];
		void *dev = g_pd3dDevice;
		Rd_DevGetViewport(dev, (uint32_t *)vp);
		Portal_DrawRect(vp, &b.min.x, 0xff00);
	}
	return 1;
}

/* the vtable of CPortal */
static const IObject3DVtbl CPortal_vftable = {
	.Delete = CPortal_Delete,
	.Update = IObject3D_UpdateNop,
	.Render = CPortal_Render,
	.InitD3D = IObject3D_InitD3DNop,
	.ReleaseD3D = IObject3D_ReleaseD3DNop,
	.Write = CPortal_Write,
	.Read = CPortal_Read,
};
