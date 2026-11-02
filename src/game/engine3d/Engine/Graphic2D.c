/*
 * Engine/grafico2d.cpp of the engine3d: CGraphic2D, see include/scrap/graphic2d.h.
 */
#include "scrap/rdev.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/graphic2d.h"

/* the corner of the texture of each corner of the rectangle (u0 v0, u1 v0, u0 v1, u1 v1) by rot */

CGraphic2D *CGraphic2D_ctorTex(CGraphic2D *self, const char *tex, const char *opac, uint32_t flags) {
	CGraphic2D_Init(self);
	CGraphic2D_CreateMat(self, tex, opac, 5, 6, 0, flags);
	if (flags & GRAPHIC2D_SCREEN) {
		self->screen = 1;
	}
	if (flags & GRAPHIC2D_SCREEN_SIZE) {
		self->screenSize = 1;
	}
	if (flags & GRAPHIC2D_OWN) {
		self->own = 1;
	}
	return self;
}

CGraphic2D *CGraphic2D_ctor(CGraphic2D *self) {
	CGraphic2D_Init(self);
	return self;
}

void CGraphic2D_dtor(CGraphic2D *self) {
	AnimMat_Free(&self->AnimMat);
	if (self->own && self->Mat) {
		CMatList_Delete((&g_MatList), self->Mat);
	}
}

void CGraphic2D_Render(CGraphic2D *self) {
	CGraphic2D_Draw(self, NULL, NULL, NULL, 0, 0);
}

static uint32_t Color_Mod4x(uint32_t c) {
	return g_DiffuseMod4x ? (c >> 2 & 0x3f3f3f) | (c & 0xff000000) : c;
}

/* x of the vertices scaled by SpriteScaleWidth around the middle of the viewport */
static void Verts2D_ScaleX(Vert2D *v, uint32_t n) {
	float k = g_SpriteScaleWidth;
	float c = (0.5f - k * 0.5f) * (float)Render_GetViewportW();
	for (uint32_t i = 0; i < n; i++) {
		v[i].x = v[i].x * k + c;
	}
}

static void Draw_Verts(void *dev, int type, uint32_t nPrim, const Vert2D *v, uint32_t nVert) {
	Rd_DevSetVertexShader(dev, D3DFVF_2D);
	Rd_DevDrawPrimitiveUP(dev, type, nPrim, v, sizeof(Vert2D));
	g_RenderTris += (int)nPrim;
	g_RenderVerts += (int)nVert;
}

Vert2D g_Verts2D[64];

double g_Screen2DInvW = 0.00125;

double g_Screen2DInvH = 0.0016666666666666668;

int g_Graphic2DRot[4][4] = {
	{ 0, 1, 2, 3 },
	{ 2, 0, 3, 1 },
	{ 3, 2, 1 },
	{ 1, 3, 0, 2 },
};

void CGraphic2D_Draw(CGraphic2D *self, const float *pos, const float *uv, const uint32_t *colors,
	uint32_t nVert, int type) {
	if (g_RenderOff) {
		return;
	}
	void *dev = g_Engine->device;
	CHECK(dev);
	CHECK(self->Mat);
	Render_SetMaterialAnim(self->Mat, self->AnimMat);
	int tw, th;
	if (self->noTexture) {
		th = 1;
		tw = 1;
	} else if (!CMaterial_GetMapSize(self->Mat, 0, &tw, &th)) {
		return;
	}
	float sx = (float)((double)Render_GetViewportW() * g_Screen2DInvW);
	float sy = (float)((double)Render_GetViewportH() * g_Screen2DInvH);
	float x = self->screen ? sx * self->x : self->x;
	x -= 0.5f;
	float y = self->y;
	if (self->screen) {
		y *= sy;
	}
	y -= 0.5f;
	float w = (float)tw * self->scale[0] * self->scale2[0];
	float h = (float)th * self->scale[1] * self->scale2[1];
	if (!type) {
		w *= self->u1 > self->u0 ? self->u1 - self->u0 : self->u0 - self->u1;
		h *= self->v1 > self->v0 ? self->v1 - self->v0 : self->v0 - self->v1;
	}
	if (self->rot & 1) {
		float t = w;
		w = h;
		h = t;
	}
	if (self->screenSize) {
		w *= sx;
		h *= sy;
	}
	if (self->centered) {
		x -= w * 0.5f;
		y -= h * 0.5f;
	} else {
		x -= self->alignX * w;
		y -= self->alignY * h;
	}
	if (type) {
		CHECK(pos && nVert < MAX_VERT_2D);
		uint32_t nPrim = Graphic2D_NumPrims(type, nVert);
		float ku = 1.0f, kv = 1.0f;
		if (!uv) {
			ku = 1.0f / (float)Render_GetViewportW();
			kv = 1.0f / (float)Render_GetViewportH();
		}
		for (uint32_t i = 0; i < nVert; i++) {
			Vert2D *v = &g_Verts2D[i];
			v->x = pos[2 * i] * w + x;
			v->y = pos[2 * i + 1] * h + y;
			v->z = 0.5f;
			v->rhw = 0.5f;
			uint32_t c = colors ? colors[i] : self->color;
			v->color = c;
			v->specular = 0;
			if (uv) {
				v->u = uv[2 * i];
				v->v = uv[2 * i + 1];
			} else {
				v->u = v->x * ku;
				v->v = v->y * kv;
			}
			v->color = Color_Mod4x(c);
		}
		Verts2D_ScaleX(g_Verts2D, nVert);
		Draw_Verts(dev, type, nPrim, g_Verts2D, nVert);
		return;
	}
	float x2 = w + x, y2 = h + y;
	const float uvs[4][2] = { { self->u0, self->v0 }, { self->u1, self->v0 }, { self->u0, self->v1 },
		{ self->u1, self->v1 } };
	const float xy[4][2] = { { x, y }, { x2, y }, { x, y2 }, { x2, y2 } };
	uint32_t c = Color_Mod4x(self->color);
	Vert2D v[4];
	for (int i = 0; i < 4; i++) {
		const float *t = uvs[g_Graphic2DRot[self->rot][i]];
		v[i] = (Vert2D){ xy[i][0], xy[i][1], 0.0f, 1.0f, c, 0, t[0], t[1] };
	}
	Verts2D_ScaleX(v, 4);
	Draw_Verts(dev, D3DPT_TRIANGLESTRIP, 2, v, 4);
}

int g_Screen2DW = 800;

int g_Screen2DH = 600;

void Graphic2D_SetScreen(int w, int h) {
	g_Screen2DW = w;
	g_Screen2DH = h;
	g_Screen2DInvW = 1.0 / (double)w;
	g_Screen2DInvH = 1.0 / (double)h;
}

void CGraphic2D_SetTexture(CGraphic2D *self, const char *tex, const char *opac, uint32_t flags) {
	CGraphic2D_CreateMat(self, tex, opac, 5, 6, 0, flags);
}

void CGraphic2D_CreateMat(CGraphic2D *self, const char *tex, const char *opac, int src, int dst, byte alpha,
	uint32_t flags) {
	CMaterial m;

	CMaterial_ctor(&m);
	String_Set(&m.Name, tex);
	CMaterial_SetMap(&m, 0, tex);
	CMaterial_SetMap(&m, 1, opac);
	m.params.srcBlend = src;
	m.params.dstBlend = dst;
	m.params.transp = alpha != 0;
	m.params.diffuse[3] = alpha;
	m.params.lighting = 0;
	m.params.fog = 0;
	m.params.zWrite = flags >> 3 & 1;
	m.params.zFunc = flags & GRAPHIC2D_MAT_I40 ? 2 : 8;
	m.params.attrib = flags & GRAPHIC2D_ATTRIB_20 ? 0x60 : 0x40;
	if (m.map[0]) {
		m.map[0]->filter = 0;
		if (!(flags & GRAPHIC2D_MAP_BC)) {
			m.map[0]->tile = 0;
		}
	}
	if (flags & GRAPHIC2D_MAP_BA && m.map[0]) {
		m.map[0]->ba = 1;
	}
	if (flags & GRAPHIC2D_OWN) {
		m.params.attrib |= 8;
		self->own = 1;
	}
	if (!self->own) {
		self->Mat = CMatList_Find((&g_MatList), &m);
	}
	if (!self->Mat) {
		self->Mat = Mat_AddCopy(&m);
	}
	CMaterial_Prepare(self->Mat, g_Engine->device);
	AnimMat_Free(&self->AnimMat);
	CMap *map = self->Mat->map[0];
	if (map) {
		AnimMat_FromMap(&self->AnimMat, map);
		if (flags & GRAPHIC2D_MULTISPRITE) {
			CMap_BuildMultiSprite(map);
		}
	}
	CMaterial_dtor(&m);
}

void CGraphic2D_Init(CGraphic2D *self) {
	self->x = 0.0f;
	self->y = 0.0f;
	self->alignX = 0.0f;
	self->alignY = 0.0f;
	self->scale[0] = 1.0f;
	self->scale[1] = 1.0f;
	self->scale2[0] = 1.0f;
	self->scale2[1] = 1.0f;
	self->u0 = 0.0f;
	self->v0 = 0.0f;
	self->u1 = 1.0f;
	self->screen = 0;
	self->screenSize = 0;
	self->centered = 0;
	self->noTexture = 0;
	self->v1 = 1.0f;
	self->color = 0xffffff;
	self->Mat = NULL;
	self->AnimMat = NULL;
	self->own = 0;
	self->rot = 0;
}

uint32_t Graphic2D_NumPrims(int type, uint32_t nVert) {
	CHECK(nVert > 0);
	if (type == 4) {
		CHECK((nVert % 3) == 0);
		return nVert / 3;
	}
	if (type == 5) {
		CHECK(nVert > 2);
	} else if (type == 6) {
		CHECK(nVert > 2);
	} else {
		CHECK(0);
		return 0;
	}
	return nVert - 2;
}

void Graphic2D_GetScreen(int *w, int *h) {
	if (w) {
		*w = g_Screen2DW;
	}
	if (h) {
		*h = g_Screen2DH;
	}
}

cGVar GVar_SpriteScaleWidth;

