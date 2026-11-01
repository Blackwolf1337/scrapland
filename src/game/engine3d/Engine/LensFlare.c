/*
 * The lens flares of the engine3d, original source Engine/LensFlare.cpp (include/scrap/lensflare.h).
 */
#include <string.h>

#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/flare.h"
#include "scrap/lensflare.h"
#include "scrap/light.h"
#include "scrap/e3dworld.h"
#include "scrap/render.h"
#include "scrap/texture.h"

CLensFlare *CLensFlare_ctor(CLensFlare *self) {
	self->hit = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->pos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->on = 0;
	self->RenderIndex = -1;
	self->intens = 1.0f;
	self->scale = 1.0f;
	self->margin = 0.2f;
	self->type = 0;
	self->traceInterval = 0.0f;
	self->traceTimer = 0.0f;
	self->transp = 1.0f;
	self->visible = 0;
	self->relative = 0;
	self->lastTime = 0;
	return self;
}

void CLensFlare_dtor(CLensFlare *self) {
	if (self->on) {
		self->on = 0;
		LensFlare_Update(self);
	}
}

void LensFlare_Update(CLensFlare *lf) {
	if (!lf) {
		return;
	}
	int index = lf->RenderIndex;
	if (lf->on) {
		if (index == -1 && g_nLensFlareRender < MAX_LENSFLARE_RENDER) {
			g_LensFlareRender[g_nLensFlareRender] = lf;
			lf->RenderIndex = g_nLensFlareRender;
			g_nLensFlareRender++;
		}
		return;
	}
	if (index == -1) {
		return;
	}
	CHECK(index < MAX_LENSFLARE_RENDER);
	CHECK(g_nLensFlareRender <= MAX_LENSFLARE_RENDER);
	CHECK(g_LensFlareRender[index] == lf);
	/* the last one of the list takes its place */
	CLensFlare *last = g_LensFlareRender[g_nLensFlareRender - 1];
	g_nLensFlareRender--;
	g_LensFlareRender[index] = last;
	last->RenderIndex = index;
	lf->RenderIndex = -1;
}

void CLensFlare_SetOn(CLensFlare *self, byte on) {
	if (self->on == on) {
		return;
	}
	self->on = on;
	LensFlare_Update(self);
	if (on) {
		self->lastTime = Float_ToU32(g_TimeMsF);
	}
}

int LensFlare_ClearAll(void) {
	for (int i = 0; i < g_nLensFlareRender; i++) {
		CLensFlare *lf = g_LensFlareRender[i];
		if (lf->on) {
			lf->on = 0;
			LensFlare_Update(lf);
		}
	}
	g_nLensFlareRender = 0;
	return 0;
}

void LensFlare_Set(CLensFlare *lf, byte on) {
	lf->on = on;
	LensFlare_Update(lf);
}

int LensFlare_Release(void) {
	return 0;
}

int LensFlare_Init(void) {
	CMaterial m;

	CMaterial_ctor(&m);
	CMaterial_SetMap(&m, MAP_DIFFUSE, "BMP/LensFlare.bmp");
	m.params.lighting = 0;
	m.params.transp = 1;
	m.params.srcBlend = 5;
	m.params.dstBlend = 2;
	m.params.zWrite = 0;
	m.params.zFunc = 2;
	m.map[MAP_DIFFUSE]->filter = 0;
	CMaterial_Prepare(&m, g_Engine->device);
	g_LensFlareMat = Mat_New(&m, 1);
	CMaterial_GetMultiSprite(g_LensFlareMat, 0);
	CMaterial_dtor(&m);
	return 0;
}

int LensFlare_InitList(void) {
	g_nLensFlareRender = 0;
	memset(g_LensFlareRender, 0, MAX_LENSFLARE_RENDER * sizeof(CLensFlare *));
	return 0;
}

void LensFlare_RenderAll(void) {
	void *dev = g_Engine->device;
	matrix_t view, proj;

	if (!g_LensFlares || g_nLensFlareRender <= 0) {
		return;
	}
	Render_SetMaterial(g_LensFlareMat, 0);
	D3D_METHOD(dev, d3dGetTransform_t, D3DDEV_GETTRANSFORM)(dev, D3DTS_VIEW, &view);
	D3D_METHOD(dev, d3dGetTransform_t, D3DDEV_GETTRANSFORM)(dev, D3DTS_PROJECTION, &proj);
	Render_SetWorldMatrix(g_MatIdentity, 0);
	D3D_METHOD(dev, d3dSetTransform_t, D3DDEV_SETTRANSFORM)(dev, D3DTS_VIEW, g_MatIdentity);
	D3D_METHOD(dev, d3dSetTransform_t, D3DDEV_SETTRANSFORM)(dev, D3DTS_PROJECTION, g_MatIdentity);
	D3D_METHOD(dev, d3dSetVertexShader_t, D3DDEV_SETVERTEXSHADER)(dev, D3DFVF_LENSFLARE);
	for (int i = 0; i < g_nLensFlareRender; i++) {
		CLensFlare *lf = g_LensFlareRender[i];
		CHECK(lf->RenderIndex == i);
		if (lf->on) {
			CLensFlare_Render(lf, view, proj);
		}
	}
	D3D_METHOD(dev, d3dSetTransform_t, D3DDEV_SETTRANSFORM)(dev, D3DTS_VIEW, &view);
	D3D_METHOD(dev, d3dSetTransform_t, D3DDEV_SETTRANSFORM)(dev, D3DTS_PROJECTION, &proj);
}

/* the sprites of a pattern as the bits of their floats: dist, sprite, size, r, g, b and the factor of the
 * intensity for the alpha (0: the intensity itself) */
typedef struct LensFlareSprite {
	uint32_t dist, sprite, size, r, g, b, k;
} LensFlareSprite;

#define LF_ONE 0x3f800000
#define LF_HALF 0x3f000000

static const LensFlareSprite lfSprites0[] = {
	{ 0, 9, 0x3ea3d70a, LF_ONE, LF_ONE, LF_ONE, 0x3e3851ec },
	{ 0, 1, 0x3ecccccd, 0x3e4ccccd, 0x3e4ccccd, 0x3dcccccd, 0x3e6147ae },
	{ 0, 6, 0x3eb33333, 0x3e4ccccd, 0x3e4ccccd, 0x3dcccccd, 0x3e8f5c29 },
	{ 0x3e800000, 7, 0x3ccccccd, 0x3dcccccd, 0x3dcccccd, 0x3da3d70a, 0x3ed70a3d },
	{ 0x3f0ccccd, 7, 0x3cf5c28f, 0x3e99999a, 0x3e99999a, 0x3e75c28f, 0x3ec28f5c },
	{ 0x3f47ae14, 6, 0x3c75c28f, LF_HALF, LF_HALF, 0x3ecccccd, 0x3ef5c28f },
	{ 0x3f3851ec, 5, 0x3d4ccccd, 0x3e99999a, 0x3e99999a, 0x3e4ccccd, 0x3eb33333 },
	{ 0x3f6b851f, 7, 0x3ccccccd, 0x3e4ccccd, 0x3e4ccccd, 0x3dcccccd, 0x3ec28f5c },
	{ 0x3f933333, 6, 0x3ccccccd, LF_HALF, LF_HALF, 0x3ecccccd, 0x3f19999a },
	{ 0x3faccccd, 4, 0x3e800000, 0x3e4ccccd, 0x3e4ccccd, 0x3dcccccd, 0x3e99999a },
};

static const LensFlareSprite lfSprites1[] = {
	{ 0, 6, 0x3f19999a, 0x3f333333, 0x3ecccccd, 0x3ecccccd, 0 },
	{ 0, 1, 0x3e800000, LF_ONE, LF_ONE, LF_ONE, 0 },
	{ 0, 2, 0x3e4ccccd, 0x3e99999a, 0x3dcccccd, 0, 0 },
	{ 0x3ecccccd, 7, 0x3e4ccccd, 0x3d8f5c29, 0x3d8f5c29, 0x3dcccccd, 0 },
	{ 0x3ea3d70a, 7, 0x3d4ccccd, 0x3d8f5c29, 0x3d8f5c29, 0x3dcccccd, 0 },
	{ 0x3eeb851f, 7, 0x3dcccccd, 0x3d8f5c29, 0x3d8f5c29, 0x3dcccccd, 0 },
	{ 0x3f4ccccd, 7, 0x3d75c28f, 0x3e99999a, 0x3e4ccccd, 0x3e051eb8, 0 },
	{ 0x3f7ae148, 8, 0x3c23d70a, LF_HALF, LF_ONE, LF_HALF, 0 },
	{ 0x3f933333, 6, 0x3cf5c28f, LF_HALF, LF_ONE, LF_HALF, 0 },
	{ 0x3fa66666, 5, 0x3e4ccccd, 0x3e99999a, 0x3e4ccccd, 0x3e051eb8, 0 },
	{ 0x3fa00000, 5, 0x3dcccccd, 0x3e99999a, 0x3e4ccccd, 0x3e051eb8, 0x3f333333 },
	{ 0x3fa8f5c3, 7, 0x3d4ccccd, 0x3e99999a, 0x3e4ccccd, 0x3e051eb8, 0x3f333333 },
	{ 0x3fb0a3d7, 7, 0x3d4ccccd, 0x3dcccccd, 0x3e4ccccd, 0x3dcccccd, LF_HALF },
	/* the original keeps the intensity times 0.4 for the last two */
	{ 0x3fc00000, 4, 0x3e99999a, 0x3dcccccd, 0x3e4ccccd, 0x3dcccccd, 0x3ecccccd },
	{ 0x3fd9999a, 4, LF_HALF, 0x3dcccccd, 0x3e4ccccd, 0x3dcccccd, 0x3ecccccd },
};

static const LensFlareSprite lfSprites2[] = {
	{ 0, 9, 0x3e99999a, 0x3f19999a, 0x3f19999a, 0x3ecccccd, 0x3f733333 },
	{ 0, 1, 0x3e000000, LF_ONE, LF_ONE, LF_ONE, 0x3f733333 },
	{ 0, 2, 0x3dcccccd, 0x3e4ccccd, 0x3e4ccccd, 0x3dcccccd, 0x3f733333 },
	{ 0x3e800000, 7, 0x3ccccccd, 0x3dcccccd, 0x3dcccccd, 0x3da3d70a, 0x3f666666 },
	{ 0x3f0ccccd, 7, 0x3cf5c28f, 0x3e99999a, 0x3e99999a, 0x3e75c28f, 0x3f59999a },
	{ 0x3f47ae14, 6, 0x3c75c28f, LF_HALF, LF_HALF, 0x3ecccccd, 0x3f4ccccd },
	{ 0x3f3851ec, 5, 0x3d4ccccd, 0x3e99999a, 0x3e99999a, 0x3e4ccccd, 0x3f333333 },
	{ 0x3f6b851f, 7, 0x3ccccccd, 0x3e4ccccd, 0x3e4ccccd, 0x3dcccccd, LF_HALF },
	{ 0x3faccccd, 4, 0x3e800000, 0x3e4ccccd, 0x3e4ccccd, 0x3dcccccd, 0x3ecccccd },
};

static const LensFlareSprite lfSprites3[] = {
	{ 0, 1, 0x3f400000, 0x3f19999a, 0x3f666666, LF_ONE, 0x3f400000 },
	{ 0, 10, 0x3fb33333, 0x3f19999a, 0x3f666666, LF_ONE, 0x3f733333 },
	{ 0x3d4ccccd, 2, 0x3e800000, 0x3dcccccd, 0x3e4ccccd, 0x3e4ccccd, 0x3e800000 },
	{ 0x3e800000, 7, 0x3ccccccd, 0x3da3d70a, 0x3dcccccd, 0x3dcccccd, 0x3f666666 },
	{ 0x3f0ccccd, 7, 0x3cf5c28f, 0x3e75c28f, 0x3e99999a, 0x3e99999a, 0x3f59999a },
	{ 0x3f47ae14, 6, 0x3c75c28f, 0x3ecccccd, LF_HALF, LF_HALF, 0x3f4ccccd },
	{ 0x3f3851ec, 5, 0x3d4ccccd, 0x3e4ccccd, 0x3e99999a, 0x3e99999a, 0x3f333333 },
	{ 0x3f6b851f, 7, 0x3ccccccd, 0x3dcccccd, 0x3e4ccccd, 0x3e4ccccd, LF_HALF },
	{ 0x3faccccd, 4, 0x3e19999a, 0x3dcccccd, 0x3e4ccccd, 0x3e4ccccd, 0x3ecccccd },
};

static const struct {
	const LensFlareSprite *s;
	int n;
} lfPatterns[4] = {
	{ lfSprites0, sizeof(lfSprites0) / sizeof(lfSprites0[0]) },
	{ lfSprites1, sizeof(lfSprites1) / sizeof(lfSprites1[0]) },
	{ lfSprites2, sizeof(lfSprites2) / sizeof(lfSprites2[0]) },
	{ lfSprites3, sizeof(lfSprites3) / sizeof(lfSprites3[0]) },
};

static float LF_Float(uint32_t bits) {
	float f;

	memcpy(&f, &bits, sizeof(f));
	return f;
}

void CLensFlare_Render(CLensFlare *self, matrix_t view, matrix_t proj) {
	CCamera *cam = World_Get()->cam;
	matrix_t rel;
	const matrix_t *m = &view;
	float v[4], s[4];

	CHECK(cam);
	vec3_t pos = self->pos;
	if (self->relative) {
		rel = view;
		rel.m[12] = 0.0f;
		rel.m[13] = 0.0f;
		rel.m[14] = 0.0f;
		m = &rel;
	}
	D3DX_Vec3Transform((vec3_t *)v, &pos, m);
	if (cam->rangeNear > v[2]) {
		return;
	}
	D3DX_Vec3Transform((vec3_t *)s, (const vec3_t *)v, &proj);
	float margin = self->margin;
	float lo = -1.0f - margin;
	float sx = s[0];
	float sy = s[1];
	float f = 1.0f;
	self->sx = sx;
	self->sy = sy;
	if (lo > sx) {
		return;
	}
	float hi = margin + 1.0f;
	if (sx > hi || lo > sy || sy > hi) {
		return;
	}
	/* faded out beyond the edges of the screen */
	float k = 1.0f / margin;
	if (-1.0f > sx) {
		f = (sx + 1.0f) * k + 1.0f;
	}
	if (sx > 1.0f) {
		f = f - (sx - 1.0f) * k;
	}
	if (-1.0f > sy) {
		f = f + (sy + 1.0f) * k;
	}
	if (sy > 1.0f) {
		f = f - (sy - 1.0f) * k;
	}
	if (self->traceInterval > 0.0f) {
		float now = g_TimeMsF;
		float dt = (now - (float)(double)self->lastTime) * 0.001f;
		self->lastTime = Float_ToU32(now);
		float t = self->traceTimer - dt;
		self->traceTimer = t;
		if (t <= 0.0f) {
			self->traceTimer = self->traceInterval;
			CGround *ground = World_Get()->ObjGround;
			vec3_t from = *Render_GetEyePos();
			vec3_t to;
			if (self->relative) {
				to.x = from.x + self->pos.x;
				to.y = self->pos.y + from.y;
				to.z = self->pos.z + from.z;
			} else {
				to = self->pos;
			}
			self->visible = !ground || !CGround_Trace(ground, &from, &to, NULL, 0.0f, &self->hit);
		}
		float d = dt / self->traceInterval;
		if (self->visible) {
			self->transp = d + self->transp;
		} else {
			self->transp = self->transp - d;
		}
		float tr = self->transp;
		tr = 0.0f < tr ? Float_MinSS(1.0f, tr) : 0.0f;
		self->transp = tr;
		f = tr * f;
	}
	if (0.0f >= f || 0.0f >= self->intens) {
		return;
	}
	if ((uint32_t)self->type > 3) {
		return;
	}
	const LensFlareSprite *sp = lfPatterns[self->type].s;
	for (int i = 0; i < lfPatterns[self->type].n; i++) {
		float a = sp[i].k ? f * LF_Float(sp[i].k) : f;
		CLensFlare_DrawSprite(self, LF_Float(sp[i].dist), sp[i].sprite, LF_Float(sp[i].size), LF_Float(sp[i].r),
			LF_Float(sp[i].g), LF_Float(sp[i].b), a);
	}
}

void CLensFlare_DrawSprite(CLensFlare *self, float dist, uint32_t sprite, float size, float r, float g,
	float b, float a) {
	void *dev = g_Engine->device;
	CCamera *cam = World_Get()->cam;
	float texMin[2], texMax[2];

	CHECK(cam);
	float cx = -self->sx * dist + self->sx;
	float cy = self->sy + -self->sy * dist;
	const CMultiSprite *ms = g_LensFlareMat->map[MAP_DIFFUSE]->MultiSprite;
	float hx = self->scale * size;
	float hy = cam->aspectScreen * size * self->scale;
	byte ok = CMultiSprite_GetUV(ms, sprite, texMin, texMax);
	CHECK(ok);
	uint32_t color = Float_ToU32(self->intens * a * 255.0f);
	color = color << 8 | (Float_ToU32(r * 255.0f) & 0xff);
	color = color << 8 | (Float_ToU32(g * 255.0f) & 0xff);
	color = color << 8 | (Float_ToU32(b * 255.0f) & 0xff);
	VertLensFlare *v = g_LensFlareVerts;
	v[0] = (VertLensFlare){ cx - hx, cy - hy, 0.0f, color, 0, texMin[0], texMin[1] };
	v[1] = (VertLensFlare){ cx - hx, cy + hy, 0.0f, color, 0, texMin[0], texMax[1] };
	v[2] = (VertLensFlare){ cx + hx, cy - hy, 0.0f, color, 0, texMax[0], texMin[1] };
	v[3] = (VertLensFlare){ cx + hx, cy + hy, 0.0f, color, 0, texMax[0], texMax[1] };
	D3D_METHOD(dev, d3dDrawPrimitiveUP_t, D3DDEV_DRAWPRIMITIVEUP)(dev, D3DPT_TRIANGLESTRIP, 2, v, sizeof(*v));
}

void CLensFlare_SetIntens(CLensFlare *self, float v) {
	self->intens = v;
}

void CLensFlare_SetTraceInterval(CLensFlare *self, float v) {
	self->traceInterval = v;
}

void CLensFlare_SetMargin(CLensFlare *self, float v) {
	self->margin = v;
}

void CLensFlare_SetPos(CLensFlare *self, vec3_t pos) {
	self->pos = pos;
}

void CLensFlare_SetScale(CLensFlare *self, float v) {
	self->scale = v;
}

void CLensFlare_SetType(CLensFlare *self, int type) {
	self->type = type;
}

void CLensFlare_SetRelative(CLensFlare *self, byte relative) {
	self->relative = relative;
}
