/*
 * Trails of the engine3d, original source Engine/trails.cpp (include/scrap/trails.h).
 */
#include <string.h>

#include "scrap/rdev.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/node3d.h"
#include "scrap/material.h"
#include "scrap/trails.h"
#include "scrap/staticinit.h"

#define g_TrailsOn (GVar_R_Trails.value.i)
static cGVar GVar_R_Trails;
#define TRAILS_FVF 0x1c2
#define TRAILS_PI4 0.78539819f

extern int g_RenderCounts[10];
#define g_TrailsDrawn g_RenderCounts[7]	/* statistics of the frame */

CTrail *CTrail_ctor(CTrail *self) {
	self->u0 = 0.0f;
	self->v0 = 0.0f;
	self->du = 1.0f;
	self->dv = 1.0f;
	self->uLen = 1.0f;
	self->unused_50 = 1.0f;
	self->orientVec = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->render = 0;
	self->RenderIndex = -1;
	self->texture = 0;
	self->fade = 0;
	self->smooth = 0;
	self->premul = 0;
	self->transp = 0;
	self->m_wMaxTrails = 0;
	self->numTrails = 0;
	self->segments = 0;
	self->last = 0;
	self->head = 0;
	self->scaleTail = 1.0f;
	self->scaleHead = 1.0f;
	self->color = 0xffffffff;
	self->width = 1.0f;
	self->type = TRAIL_RIBBON;
	self->orient = TRAIL_ORIENT_EYE;
	self->m_pTrails = NULL;
	return self;
}

void CTrail_dtor(CTrail *self) {
	if (self->m_pTrails) {
		operator_delete(self->m_pTrails);
		self->m_pTrails = NULL;
	}
	CTrail_Clear(self);
}

static CTrail *g_TrailsRender[17][MAX_TRAILS_RENDER];
static int g_nTrails;
static int g_nTrailsRender[17];

/* into or out of gTrailsRender as render says; the last of the list takes the free slot */
static void Trails_UpdateRender(CTrail *t) {
	if (!t) {
		return;
	}
	byte on = t->render;
	int index = t->RenderIndex;
	int texture = t->texture;
	CHECK(texture >= 0 && texture < MAX_TRAILS_TEXTURES);
	if (on) {
		if (index != -1) {
			return;
		}
		int n = g_nTrailsRender[texture];
		if (n >= MAX_TRAILS_RENDER) {
			return;
		}
		t->RenderIndex = n;
		g_nTrails++;
		g_nTrailsRender[texture] = n + 1;
		g_TrailsRender[texture][n] = t;
		return;
	}
	if (index == -1) {
		return;
	}
	CHECK(index < MAX_TRAILS_RENDER);
	CHECK(g_nTrailsRender[texture] <= MAX_TRAILS_RENDER);
	CHECK(g_TrailsRender[texture][index] == t);
	g_nTrails--;
	int n = g_nTrailsRender[texture];
	CTrail *moved = g_TrailsRender[texture][n - 1];
	g_TrailsRender[texture][index] = moved;
	moved->RenderIndex = index;
	g_nTrailsRender[texture] = n - 1;
	t->RenderIndex = -1;
}

void CTrail_Reset(CTrail *self) {
	self->last = 0;
	self->numTrails = 0;
	self->head = 0;
	if (self->m_pTrails) {
		self->m_pTrails[0].pos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	}
}

void CTrail_Clear(CTrail *self) {
	if (self->render) {
		self->render = 0;
		Trails_UpdateRender(self);
	}
	CTrail_Reset(self);
}

void CTrail_Copy(CTrail *self, const CTrail *from) {
	CTrail_Clear(self);
	self->texture = from->texture;
	self->fade = from->fade;
	self->smooth = from->smooth;
	self->premul = from->premul;
	self->transp = from->transp;
	self->color = from->color;
	self->width = from->width;
	self->scaleHead = from->scaleHead;
	self->scaleTail = from->scaleTail;
	CHECK(self->m_pTrails && self->m_wMaxTrails == from->m_wMaxTrails);
	self->m_wMaxTrails = from->m_wMaxTrails;
	self->numTrails = from->numTrails;
	self->head = from->head;
	self->segments = from->segments;
	self->last = from->last;
	self->type = from->type;
	self->u0 = from->u0;
	self->v0 = from->v0;
	self->du = from->du;
	self->dv = from->dv;
	self->orientVec = from->orientVec;
	self->orient = from->orient;
	memcpy(self->m_pTrails, from->m_pTrails, self->m_wMaxTrails * sizeof(TrailPoint));
	if (self->numTrails && self->render != 1) {
		self->render = 1;
		Trails_UpdateRender(self);
	}
}

void CTrail_Shrink(CTrail *self) {
	if (self->numTrails && !--self->numTrails && self->render) {
		self->render = 0;
		Trails_UpdateRender(self);
	}
}

void CTrail_AddPos(CTrail *self, vec3_t pos) {
	CTrail_Add(self, pos, self->width, self->color);
}

void CTrail_AddPosColor(CTrail *self, vec3_t pos, uint32_t color) {
	CTrail_Add(self, pos, self->width, color);
}

void CTrail_Add(CTrail *self, vec3_t pos, float width, uint32_t color) {
	CHECK(self->m_pTrails);
	self->last = self->head;
	self->m_pTrails[self->last].pos = pos;
	self->m_pTrails[self->last].width = width;
	self->m_pTrails[self->last].color = color;
	if ((uint32_t)self->numTrails + 1 < self->m_wMaxTrails) {
		self->numTrails++;
	}
	if ((uint32_t)self->head + 1 < self->m_wMaxTrails) {
		self->head++;
	} else {
		self->head = 0;
	}
	if (!self->render) {
		self->render = 1;
		Trails_UpdateRender(self);
	}
}

/* the geometry of the trail into the locked buffers, the pointers moved on; S_OK or E_FAIL for an unknown type */
static int CTrail_BuildCross(CTrail *self, TrailVertex **pVertex, uint16_t **pIndex);
static int CTrail_BuildFan(CTrail *self, TrailVertex **pVertex, uint16_t **pIndex);
static int CTrail_BuildQuads(CTrail *self, TrailVertex **pVertex, uint16_t **pIndex);
static int CTrail_BuildRibbon(CTrail *self, TrailVertex **pVertex, uint16_t **pIndex);
static int CTrail_Build(CTrail *self, TrailVertex **pVertex, uint16_t **pIndex) {
	CHECK(self->m_pTrails);
	switch (self->type) {
	case TRAIL_RIBBON:
		return CTrail_BuildRibbon(self, pVertex, pIndex);
	case TRAIL_FAN:
		return CTrail_BuildFan(self, pVertex, pIndex);
	case TRAIL_CROSS:
		return CTrail_BuildCross(self, pVertex, pIndex);
	case TRAIL_QUADS:
		return CTrail_BuildQuads(self, pVertex, pIndex);
	default:
		return E_FAIL;
	}
}

/* the shared parts of the builders */

static const TrailPoint *Trail_Point(const CTrail *t, int i) {
	return &t->m_pTrails[i < 0 ? i + t->m_wMaxTrails : i];
}

/* 1 - transp / 255 */
static float Trail_AlphaK(const CTrail *t) {
	return 1.0f - (float)t->transp / 255.0f;
}

/* the points drawn: segments, or fewer while the trail is shorter */
static int Trail_Count(const CTrail *t) {
	return t->numTrails > t->segments ? t->segments : t->numTrails;
}

/* the alpha of a vertex: a 0..255 level times alphaK times the inverted alpha of the point */
static uint32_t Trail_Alpha(uint32_t level, float k, uint32_t color) {
	float a = (float)(int)((float)(int)level * k) / 255.0f;
	return (uint32_t)(int)(a * (float)(int)(~(color >> 24) & 0xff)) & 0xff;
}

static uint32_t Trail_Color(const CTrail *t, uint32_t c, uint32_t alpha) {
	if (t->premul) {
		float fa = (float)(int)alpha;
		uint32_t r = (uint32_t)(int)((float)(int)((c >> 16) & 0xff) * fa / 255.0f);
		uint32_t g = (uint32_t)(int)((float)(int)((c >> 8) & 0xff) * fa / 255.0f);
		uint32_t b = (uint32_t)(int)((float)(int)(c & 0xff) * fa / 255.0f);
		return (((r | alpha << 8) << 8 | g) << 8) | b;
	}
	uint32_t col = alpha << 24 | (c & 0xffffff);
	if (g_DiffuseMod4x) {
		col = (col & 0xff000000) | ((col >> 2) & 0x3f3f3f);
	}
	return col;
}

static void Trail_Eye(const CTrail *t, vec3_t *eye) {
	if (t->orient == TRAIL_ORIENT_EYE) {
		*eye = *Render_GetEyePos();
	} else if (t->orient == TRAIL_ORIENT_POINT) {
		*eye = t->orientVec;
	} else if (t->orient == TRAIL_ORIENT_DIR) {
		D3DX_Vec3Normalize(eye, &t->orientVec);
	}
}

/* the normal of the plane through the eye and both points, or the direction; other modes keep n */
static void Trail_Normal(const CTrail *t, const vec3_t *eye, const vec3_t *p0, const vec3_t *p1, vec3_t *n) {
	if (t->orient == TRAIL_ORIENT_EYE || t->orient == TRAIL_ORIENT_POINT) {
		float ax = p0->x - eye->x, ay = p0->y - eye->y, az = p0->z - eye->z;
		float bx = p1->x - eye->x, by = p1->y - eye->y, bz = p1->z - eye->z;

		n->x = bz * ay - by * az;
		n->y = az * bx - bz * ax;
		n->z = by * ax - ay * bx;
		D3DX_Vec3Normalize(n, n);
	} else if (t->orient == TRAIL_ORIENT_DIR) {
		*n = *eye;
	}
}

/* the two sides p - n * w * s and p + n * w * s */
static void Trail_Sides(vec3_t *l, vec3_t *r, const vec3_t *p, const vec3_t *n, float w, float s) {
	vec3_t e = { n->x * w * s, n->y * w * s, n->z * w * s };

	l->x = p->x - e.x;
	l->y = p->y - e.y;
	l->z = p->z - e.z;
	r->x = e.x + p->x;
	r->y = e.y + p->y;
	r->z = e.z + p->z;
}

/* v half way back to from */
static void Trail_Half(vec3_t *v, const vec3_t *from) {
	v->x = (v->x - from->x) * 0.5f + from->x;
	v->y = (v->y - from->y) * 0.5f + from->y;
	v->z = (v->z - from->z) * 0.5f + from->z;
}

static TrailVertex *Trail_Vertex(TrailVertex *v, const vec3_t *p, uint32_t color, float u, float tv) {
	v->pos = *p;
	v->color = color;
	v->specular = 0;
	v->u = u;
	v->v = tv;
	return v + 1;
}

/* a cross section: l at v0, r at v0 + dv */
static TrailVertex *Trail_Section(const CTrail *t, TrailVertex *v, const vec3_t *l, const vec3_t *r, uint32_t color,
	float u) {
	v = Trail_Vertex(v, l, color, u, t->v0);
	return Trail_Vertex(v, r, color, u, t->dv + t->v0);
}

static float Trail_U(const CTrail *t, int k) {
	return (float)k / t->uLen * t->du + t->u0;
}

/* two triangles from the section before (back vertices back) to the section ending at vtx */
static uint16_t *Trail_Quad(uint16_t *ib, int vtx, int back) {
	ib[0] = vtx - back;
	ib[1] = vtx - back + 1;
	ib[2] = vtx - 2;
	ib[3] = vtx - back + 1;
	ib[4] = vtx - 2;
	ib[5] = vtx - 1;
	return ib + 6;
}

/* the start of the ribbon and cross builders: the scale of the oldest point drawn and the step to the next */
static float Trail_Scale(const CTrail *t, int cnt, float *step) {
	float tail = t->scaleTail;
	float scale = tail;

	*step = (t->scaleHead - tail) / (float)(int)t->numTrails;
	if (t->numTrails != cnt) {
		scale = (float)(int)(t->numTrails - cnt) * *step + tail;
	}
	return scale;
}

/* the alpha levels of point i and i + 1 of cnt */
static void Trail_FadeLevels(const CTrail *t, int cnt, int i, uint32_t *a0, uint32_t *a1) {
	*a0 = 255;
	*a1 = 255;
	if (t->fade) {
		uint32_t q = 255u / (uint32_t)cnt;
		*a0 = (q * i) & 0xff;
		*a1 = ((i + 1) * q) & 0xff;
	}
}

static int g_TrailsVertex;
static int g_TrailsPrims;
static int g_TrailsIndex;

static int CTrail_BuildRibbon(CTrail *self, TrailVertex **pVertex, uint16_t **pIndex) {
	if (self->numTrails <= 1 || !self->segments) {
		return 0;
	}
	uint16_t *ib = *pIndex;
	TrailVertex *vb = *pVertex;
	int cnt = Trail_Count(self);
	float step;
	float scale = Trail_Scale(self, cnt, &step);
	float alphaK = Trail_AlphaK(self);
	CHECK(vb);
	CHECK(ib);
	vec3_t eye, n = { 0.0f, 0.0f, 0.0f };
	Trail_Eye(self, &eye);

	/* with one point drawn the original adds the smooth end from registers never set; zeros here */
	vec3_t p1 = { 0.0f, 0.0f, 0.0f }, e1 = { 0.0f, 0.0f, 0.0f };
	uint32_t col1 = 0;
	int vtx = g_TrailsVertex;
	int k = 0;
	for (int i = 0; i < cnt - 1; i++) {
		int a = self->head - cnt + i;
		const TrailPoint *q0 = Trail_Point(self, a), *q1 = Trail_Point(self, a + 1);
		k++;
		vec3_t p0 = q0->pos;
		p1 = q1->pos;
		float w0 = q0->width, w1 = q1->width;
		uint32_t a0, a1;
		Trail_FadeLevels(self, cnt, i, &a0, &a1);
		a0 = Trail_Alpha(a0, alphaK, q0->color);
		a1 = Trail_Alpha(a1, alphaK, q1->color);
		uint32_t col0 = Trail_Color(self, q0->color, a0);
		col1 = Trail_Color(self, q1->color, a1);
		Trail_Normal(self, &eye, &p0, &p1, &n);

		float s = scale * 0.5f;
		vec3_t l0, r0, l1, r1;
		Trail_Sides(&l0, &r0, &p0, &n, w0, s);
		Trail_Sides(&l1, &r1, &p1, &n, w1, s);
		e1 = (vec3_t){ n.x * w1 * s, n.y * w1 * s, n.z * w1 * s };
		if (self->smooth) {
			Trail_Half(&l1, &l0);
			Trail_Half(&r1, &r0);
		}
		if (k == 1) {
			vb = Trail_Section(self, vb, &l0, &r0, col0, self->u0);
			vtx += 2;
		}
		vtx += 2;
		g_TrailsPrims += 2;
		scale += step;
		g_TrailsIndex += 6;
		g_TrailsVertex = vtx;
		vb = Trail_Section(self, vb, &l1, &r1, col1, Trail_U(self, k));
		ib = Trail_Quad(ib, vtx, 4);
	}
	if (self->smooth) {
		vec3_t l = { p1.x - e1.x, p1.y - e1.y, p1.z - e1.z };
		vec3_t r = { p1.x + e1.x, e1.y + p1.y, e1.z + p1.z };

		g_TrailsPrims += 2;
		vtx += 2;
		g_TrailsVertex = vtx;
		vb = Trail_Section(self, vb, &l, &r, col1, Trail_U(self, k + 1));
		ib = Trail_Quad(ib, vtx, 4);
		g_TrailsIndex += 6;
	}
	*pVertex = vb;
	*pIndex = ib;
	return 0;
}

/* the cross builder joins its second band to the section two back after the first piece (as the first band) */
static int Trail_CrossBack(int k) {
	return k == 1 ? 4 : k == 2 ? 8 : 6;
}

static int CTrail_BuildCross(CTrail *self, TrailVertex **pVertex, uint16_t **pIndex) {
	if (self->numTrails <= 1 || !self->segments) {
		return 0;
	}
	int cnt = Trail_Count(self);
	TrailVertex *vb = *pVertex;
	uint16_t *ib = *pIndex;
	float step;
	float scale = Trail_Scale(self, cnt, &step);
	float alphaK = Trail_AlphaK(self);
	CHECK(vb);
	CHECK(ib);
	vec3_t eye, n = { 0.0f, 0.0f, 0.0f };
	Trail_Eye(self, &eye);
	matrix_t ma, mb;
	D3DX_MatrixRotationYPR(&ma, 0.0f, TRAILS_PI4, 0.0f);
	D3DX_MatrixRotationYPR(&mb, 0.0f, -TRAILS_PI4, 0.0f);

	int k = 0;
	for (int i = 0; i < cnt - 1; i++) {
		int a = self->head - cnt + i;
		const TrailPoint *q0 = Trail_Point(self, a), *q1 = Trail_Point(self, a + 1);
		k++;
		vec3_t p0 = q0->pos, p1 = q1->pos;
		float w0 = q0->width, w1 = q1->width;
		uint32_t a0, a1;
		Trail_FadeLevels(self, cnt, i, &a0, &a1);
		a0 = Trail_Alpha(a0, alphaK, q0->color);
		a1 = Trail_Alpha(a1, alphaK, q1->color);
		uint32_t col0 = Trail_Color(self, q0->color, a0);
		uint32_t col1 = Trail_Color(self, q1->color, a1);
		Trail_Normal(self, &eye, &p0, &p1, &n);
		vec3_t na, nb;
		D3DX_Vec3Transform(&na, &n, &ma);
		D3DX_Vec3Transform(&nb, &n, &mb);

		float s = scale * 0.5f;
		vec3_t l0, r0, l1, r1;
		Trail_Sides(&l0, &r0, &p0, &na, w0, s);
		Trail_Sides(&l1, &r1, &p1, &na, w1, s);
		int vtx = g_TrailsVertex;
		if (k == 1) {
			vb = Trail_Section(self, vb, &l0, &r0, col0, self->u0);
			vtx += 2;
		}
		vtx += 2;
		vb = Trail_Section(self, vb, &l1, &r1, col1, Trail_U(self, k));
		ib = Trail_Quad(ib, vtx, Trail_CrossBack(k));

		Trail_Sides(&l0, &r0, &p0, &nb, w0, s);
		Trail_Sides(&l1, &r1, &p1, &nb, w1, s);
		int prims = g_TrailsPrims + 2;
		int index = g_TrailsIndex + 6;
		if (k == 1) {
			vb = Trail_Section(self, vb, &l0, &r0, col0, self->u0);
			vtx += 2;
		}
		vtx += 2;
		g_TrailsVertex = vtx;
		vb = Trail_Section(self, vb, &l1, &r1, col1, Trail_U(self, k));
		ib = Trail_Quad(ib, vtx, Trail_CrossBack(k));
		scale += step;
		g_TrailsPrims = prims + 2;
		g_TrailsIndex = index + 6;
	}
	*pVertex = vb;
	*pIndex = ib;
	return 0;
}

static int CTrail_BuildQuads(CTrail *self, TrailVertex **pVertex, uint16_t **pIndex) {
	if (self->numTrails <= 1 || !self->segments) {
		return 0;
	}
	TrailVertex *vb = *pVertex;
	uint16_t *ib = *pIndex;
	float alphaK = Trail_AlphaK(self);
	CHECK(vb);
	CHECK(ib);
	vec3_t eye, n = { 0.0f, 0.0f, 0.0f };
	Trail_Eye(self, &eye);

	int k = 1;
	if ((self->numTrails & 0xfe) >= 2) {
		float ak = (float)(int)(alphaK * 255.0f) / 255.0f;
		do {
			int a = self->head - 2 * k;
			const TrailPoint *q0 = Trail_Point(self, a), *q1 = Trail_Point(self, a + 1);
			vec3_t p0 = q0->pos, p1 = q1->pos;
			float w0 = q0->width, w1 = q1->width;
			uint32_t a0 = (uint32_t)(int)((float)(int)(~(q0->color >> 24) & 0xff) * ak) & 0xff;
			uint32_t a1 = (uint32_t)(int)((float)(int)(~(q1->color >> 24) & 0xff) * ak) & 0xff;
			uint32_t col0 = Trail_Color(self, q0->color, a0);
			uint32_t col1 = Trail_Color(self, q1->color, a1);
			Trail_Normal(self, &eye, &p0, &p1, &n);

			vec3_t l0, r0, l1, r1;
			Trail_Sides(&l0, &r0, &p0, &n, w0, 0.5f);
			Trail_Sides(&l1, &r1, &p1, &n, w1, 0.5f);
			vb = Trail_Section(self, vb, &l0, &r0, col0, self->u0);
			vb = Trail_Section(self, vb, &l1, &r1, col1, self->du + self->u0);
			int vtx = g_TrailsVertex + 4;
			g_TrailsPrims += 2;
			g_TrailsIndex += 6;
			g_TrailsVertex = vtx;
			ib = Trail_Quad(ib, vtx, 4);
			k++;
		} while (k <= self->numTrails >> 1);
	}
	*pVertex = vb;
	*pIndex = ib;
	return 0;
}

static int CTrail_BuildFan(CTrail *self, TrailVertex **pVertex, uint16_t **pIndex) {
	if (self->numTrails <= 1 || !self->segments) {
		return 0;
	}
	uint16_t *ib = *pIndex;
	int cnt = Trail_Count(self);
	float alphaK = Trail_AlphaK(self);
	TrailVertex *vb = *pVertex;
	CHECK(vb);
	CHECK(ib);
	int center = g_TrailsVertex;
	for (int i = 0; i < cnt - 2; i++) {
		int a = self->head - i;
		const TrailPoint *q0 = Trail_Point(self, a - 1), *q1 = Trail_Point(self, a - 2);
		vec3_t p0 = q0->pos, p1 = q1->pos;
		uint32_t level = 255;
		if (self->fade) {
			level = ((cnt - i - 3) * (255u / (uint32_t)cnt)) & 0xff;
		}
		uint32_t col = Trail_Color(self, q1->color, Trail_Alpha(level, alphaK, q1->color));
		if (i == 0) {
			vb = Trail_Vertex(vb, &p0, col, 0.5f, 0.5f);
			g_TrailsVertex += 2;
			vb = Trail_Vertex(vb, &p1, col, 0.5f, 0.5f);
		} else {
			int vtx = g_TrailsVertex + 1;
			g_TrailsIndex += 3;
			g_TrailsVertex = vtx;
			vb = Trail_Vertex(vb, &p1, col, 0.5f, 0.5f);
			ib[0] = center;
			ib[1] = vtx - 2;
			ib[2] = vtx - 1;
			ib += 3;
			g_TrailsPrims++;
		}
	}
	*pVertex = vb;
	*pIndex = ib;
	return 0;
}

void CTrail_SetTransp(CTrail *self, byte transp) {
	self->transp = transp;
}

void CTrail_SetColor(CTrail *self, uint32_t color) {
	self->color = color;
}

void CTrail_SetSmooth(CTrail *self, byte on) {
	self->smooth = on;
}

void CTrail_SetMaxTrails(CTrail *self, byte segments) {
	if (segments == self->m_wMaxTrails) {
		self->segments = self->m_wMaxTrails;
		CTrail_Clear(self);
		return;
	}
	CHECK(!self->m_pTrails);
	CHECK(segments > 0 && segments < TRAILS_MAXSIZE);
	self->m_wMaxTrails = segments;
	self->segments = segments;
	self->m_pTrails = New_Array(segments, sizeof(TrailPoint));
}

void CTrail_SetUV(CTrail *self, float u0, float v0) {
	self->u0 = u0;
	self->v0 = v0;
}

void CTrail_SetOrient(CTrail *self, vec3_t v, byte mode) {
	self->orientVec = v;
	self->orient = mode;
}

void CTrail_SetType(CTrail *self, byte type) {
	self->type = type;
}

void CTrail_SetSegments(CTrail *self, byte segments) {
	CHECK(segments <= self->m_wMaxTrails);
	self->segments = segments;
}

void CTrail_SetPremul(CTrail *self, byte on) {
	self->premul = on;
}

void CTrail_SetWidth(CTrail *self, float width) {
	self->width = width;
}

void CTrail_SetScale(CTrail *self, float head, float tail) {
	self->scaleHead = head;
	self->scaleTail = tail;
}

void CTrail_SetTexture(CTrail *self, int texture) {
	if (self->texture == texture) {
		return;
	}
	CHECK(texture >= 0);
	CHECK(texture < MAX_TRAILS_TEXTURES);
	byte on = self->render;
	if (on) {
		self->render = 0;
		Trails_UpdateRender(self);
	}
	self->texture = texture;
	if (on) {
		self->render = 1;
		Trails_UpdateRender(self);
	}
}

void CTrail_SetUVScale(CTrail *self, float du, float dv) {
	self->du = du;
	self->dv = dv;
}

void CTrail_SetFade(CTrail *self, byte on) {
	self->fade = on;
}

void CTrail_SetULen(CTrail *self, float uLen, float f50) {
	self->uLen = uLen;
	self->unused_50 = f50;
}

int Trails_ResetAll(void) {
	for (int tex = 0; tex < MAX_TRAILS_TEXTURES; tex++) {
		/* the clear moves the last trail of the list into the slot, which the loop then skips (as the original) */
		for (int i = 0; i < g_nTrailsRender[tex]; i++) {
			CTrail_Clear(g_TrailsRender[tex][i]);
		}
		g_nTrailsRender[tex] = 0;
	}
	g_nTrails = 0;
	return 0;
}

static void *g_TrailsVB;
static void *g_TrailsIB;

int Trails_Release(void) {
	if (!App_Headless()) {
		CHECK(g_TrailsIB);
		CHECK(g_TrailsVB);
	}
	if (g_TrailsVB) {
		Rd_Release(g_TrailsVB);
		g_TrailsVB = NULL;
	}
	if (g_TrailsIB) {
		Rd_Release(g_TrailsIB);
		g_TrailsIB = NULL;
	}
	return 0;
}

/* the materials of the textures: map, blend factors, and a DUDV map for the last */
static const struct {
	const char *map;
	int srcBlend, dstBlend;
	byte dudv;
} trailsMats[MAX_TRAILS_TEXTURES] = {
	{ "2d/gfx/beamtrail.alpha.tga", 5, 2, 0 },
	{ "2d/gfx/smoketrail.alpha.tga", 5, 6, 0 },
	{ "2d/gfx/smoketrail.alpha.tga", 5, 2, 0 },
	{ "2d/gfx/thundertrail.alpha.tga", 5, 2, 0 },
	{ "2d/gfx/noisetrail.alpha.tga", 5, 2, 0 },
	{ "2d/gfx/teslatrail.alpha.tga", 5, 2, 0 },
	{ "2d/gfx/atpctrail_A.alpha.tga", 5, 2, 0 },
	{ "2d/gfx/atpctrail_B.alpha.tga", 5, 2, 0 },
	{ "2d/gfx/ELEMENTLIGHTING1.ALPHA.TGA", 5, 2, 0 },
	{ "2d/gfx/ELEMENTLIGHTING2.ALPHA.TGA", 5, 2, 0 },
	{ "2d/gfx/ATTACKTRAIL.ALPHA.TGA", 5, 2, 0 },
	{ "2d/gfx/NOISEQUAD.ALPHA.TGA", 5, 2, 0 },
	{ "2d/gfx/DATATRAIL.ALPHA.TGA", 5, 2, 0 },
	{ "2d/gfx/DATACODEC.bmp", 5, 2, 0 },
	{ "2d/gfx/SPEEDTRAIL.ALPHA.TGA", 5, 2, 0 },
	{ "2d/gfx/CIRCLETRAIL.BMP", 5, 2, 0 },
	{ "2d/gfx/NOISETRAIL.DUDV.BMP", 2, 2, 1 },
};

/* the lists of the active trails and the shared buffers (IDirect3DVertexBuffer8, IDirect3DIndexBuffer8) */
static struct CMaterial *g_TrailsMat[17];

int Trails_Init(void) {
	void *dev = g_Engine->device;
	if (Rd_DevCreateVertexBuffer(dev, MAX_TRAILS_VB * sizeof(TrailVertex),
		D3DUSAGE_WRITEONLY_DYNAMIC, TRAILS_FVF, 0, &g_TrailsVB) < 0) {
		return E_FAIL;
	}
	dev = g_Engine->device;
	if (Rd_DevCreateIndexBuffer(dev, MAX_TRAILS_IB * sizeof(uint16_t),
		D3DUSAGE_WRITEONLY_DYNAMIC, D3DFMT_INDEX16, 0, &g_TrailsIB) < 0) {
		return E_FAIL;
	}
	CMaterial mat;
	CMaterial_ctor(&mat);
	for (int i = 0; i < MAX_TRAILS_TEXTURES; i++) {
		CMaterial_SetMap(&mat, 0, trailsMats[i].map);
		mat.params.lighting = 0;
		mat.params.transp = 1;
		mat.params.srcBlend = trailsMats[i].srcBlend;
		mat.params.dstBlend = trailsMats[i].dstBlend;
		mat.params.zWrite = 0;
		mat.params.zFunc = 2;
		mat.map[0]->filter = trailsMats[i].dudv;
		CMaterial_Prepare(&mat, g_Engine->device);
		g_TrailsMat[i] = Mat_New(&mat, 0);
	}
	CMaterial_dtor(&mat);
	return 0;
}

int Trails_InitLists(void) {
	g_nTrails = 0;
	memset(g_nTrailsRender, 0, MAX_TRAILS_TEXTURES * sizeof(int));
	return 0;
}

/* the buffers unlocked and the geometry since the start drawn */
static void Trails_Flush(void *dev, int startV, int startP, int startI) {
	Rd_BufUnlock(g_TrailsVB);
	Rd_BufUnlock(g_TrailsIB);
	int nv = g_TrailsVertex - startV;
	int np = g_TrailsPrims - startP;
	int ni = g_TrailsIndex - startI;
	if (nv > 0 && ni > 0 && np > 0) {
		CHECK(startV < MAX_TRAILS_VB);
		CHECK(startV + nv < MAX_TRAILS_VB);
		CHECK(startI < MAX_TRAILS_IB);
		CHECK(startI + ni < MAX_TRAILS_IB);
		Rd_DevDrawIndexedPrimitive(dev, D3DPT_TRIANGLELIST, startV, nv,
			startI, np);
	}
}

static int Trails_Lock(void *buf, uint32_t off, uint32_t size, void *data, uint32_t flags) {
	return Rd_BufLock(buf, off, size, data, flags);
}

void Trails_Render(int texture) {
	if (!g_TrailsOn || g_nTrails <= 0) {
		return;
	}
	void *dev = g_Engine->device;
	Render_SetWorldMatrix((&g_MatIdentity), 0);
	Rd_DevSetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
	Rd_DevSetVertexShader(dev, TRAILS_FVF);
	Rd_DevSetStreamSource(dev, 0, g_TrailsVB, sizeof(TrailVertex));
	Rd_DevSetIndices(dev, g_TrailsIB, 0);
	g_TrailsVertex = 0;
	g_TrailsPrims = 0;
	g_TrailsIndex = 0;
	TrailVertex *pV = NULL;
	uint16_t *pI = NULL;
	for (int tex = texture >= 0 ? texture : 0; tex < MAX_TRAILS_TEXTURES; tex++) {
		if (g_nTrailsRender[tex] <= 0 || (texture == -1 && tex >= MAX_TRAILS_TEXTURES - 1)) {
			continue;
		}
		Render_SetMaterial(g_TrailsMat[tex], 0);
		int startV = g_TrailsVertex, startP = g_TrailsPrims, startI = g_TrailsIndex;
		int drawn = 0;
		byte locked = 1;
		for (int i = 0; i < g_nTrailsRender[tex]; i++) {
			int nv = g_TrailsVertex, ni = g_TrailsIndex;
			if (!nv || !ni || nv + 0x100 > MAX_TRAILS_VB || ni + 0x180 > MAX_TRAILS_IB) {
				/* full (or the first texture): draw what is there and start again with discarded buffers */
				if (drawn > 0) {
					Trails_Flush(dev, startV, startP, startI);
					drawn = 0;
				}
				g_TrailsVertex = 0;
				g_TrailsPrims = 0;
				g_TrailsIndex = 0;
				startV = 0;
				startP = 0;
				startI = 0;
				if (Trails_Lock(g_TrailsVB, 0, 0, &pV, D3DLOCK_DISCARD) < 0
					|| Trails_Lock(g_TrailsIB, 0, 0, &pI, D3DLOCK_DISCARD) < 0) {
					locked = 0;
					break;
				}
			} else if (!drawn && nv > 0 && ni > 0) {
				startP = g_TrailsPrims;
				startI = ni;
				startV = nv;
				if (Trails_Lock(g_TrailsVB, nv * sizeof(TrailVertex), (MAX_TRAILS_VB - nv) * sizeof(TrailVertex), &pV,
					D3DLOCK_NOOVERWRITE) < 0
					|| Trails_Lock(g_TrailsIB, g_TrailsIndex * sizeof(uint16_t),
					(MAX_TRAILS_IB - g_TrailsIndex) * sizeof(uint16_t), &pI, D3DLOCK_NOOVERWRITE) < 0) {
					locked = 0;
					break;
				}
			}
			CTrail *t = g_TrailsRender[tex][i];
			CHECK(t->RenderIndex == i);
			if (t->render) {
				CTrail_Build(t, &pV, &pI);
				g_TrailsDrawn++;
				drawn++;
			}
		}
		if (locked && drawn > 0) {
			Trails_Flush(dev, startV, startP, startI);
		}
	}
	Rd_DevSetRenderState(dev, D3DRS_CULLMODE, D3DCULL_CCW);
}

void CTrail_SetLast(CTrail *self, const vec3_t *pos, const float *width, const uint32_t *color) {
	CHECK(self->m_pTrails);
	if (!self->numTrails) {
		return;
	}
	if (pos) {
		self->m_pTrails[self->last].pos = *pos;
	}
	if (width) {
		self->m_pTrails[self->last].width = *width;
	}
	if (color) {
		self->m_pTrails[self->last].color = *color;
	}
}

void CTrail_SetLastPos(CTrail *self, vec3_t pos) {
	CHECK(self->m_pTrails);
	if (self->numTrails) {
		self->m_pTrails[self->last].pos = pos;
	}
}

void CTrail_SetLastPosColor(CTrail *self, vec3_t pos, uint32_t color) {
	CHECK(self->m_pTrails);
	if (self->numTrails) {
		self->m_pTrails[self->last].pos = pos;
		self->m_pTrails[self->last].color = color;
	}
}

void CTrail_SetPoint(CTrail *self, int i, const vec3_t *pos, const float *width, const uint32_t *color) {
	CHECK(self->m_pTrails);
	if (i >= self->numTrails) {
		return;
	}
	int j = self->head - i - 1;
	if (j < 0) {
		j += self->m_wMaxTrails;
	}
	if (pos) {
		self->m_pTrails[j].pos = *pos;
	}
	if (width) {
		self->m_pTrails[j].width = *width;
	}
	if (color) {
		self->m_pTrails[j].color = *color;
	}
}

void CTrail_Move(CTrail *self, vec3_t d) {
	CHECK(self->m_pTrails);
	for (int i = 0; i < self->m_wMaxTrails; i++) {
		vec3_t *p = &self->m_pTrails[i].pos;
		p->x = p->x + d.x;
		p->y = p->y + d.y;
		p->z = p->z + d.z;
	}
}

static cGVar GVar_R_Trails;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_R_Trails, &GVar_R_Trails, "R_Trails", 0, "Render Trails (0/1)", 1)
