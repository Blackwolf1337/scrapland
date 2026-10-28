/*
 * Engine/grafico2d.cpp of the engine3d: CGrafico2D, see include/scrap/grafico2d.h.
 */
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/grafico2d.h"

/* the corner of the texture of each corner of the rectangle (u0 v0, u1 v0, u0 v1, u1 v1) by rot */
#define g_Grafico2DRot (GAME_ARRAY(const int[4], g_Grafico2DRot, 0x4325b0))

CGrafico2D *__thiscall CGrafico2D_ctorTex(CGrafico2D *self, const char *tex, const char *opac, uint32_t flags) {
	CGrafico2D_Init(self);
	CGrafico2D_CreateMat(self, tex, opac, 5, 6, 0, flags);
	if (flags & GRAF2D_PANTALLA) {
		self->pantalla = 1;
	}
	if (flags & GRAF2D_TAM_PANTALLA) {
		self->tamPantalla = 1;
	}
	if (flags & GRAF2D_PROPIO) {
		self->propio = 1;
	}
	return self;
}

CGrafico2D *__fastcall CGrafico2D_ctor(CGrafico2D *self) {
	CGrafico2D_Init(self);
	return self;
}

void __fastcall CGrafico2D_dtor(CGrafico2D *self) {
	AnimMat_Free(&self->AnimMat);
	if (self->propio && self->Mat) {
		CListaMat_Delete(g_ListaMat, self->Mat);
	}
}

void __fastcall CGrafico2D_Render(CGrafico2D *self) {
	CGrafico2D_Draw(self, NULL, NULL, NULL, 0, 0);
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

static void Draw_Verts(void *dev, int tipo, uint32_t nPrim, const Vert2D *v, uint32_t nVert) {
	D3D_METHOD(dev, d3dSetVertexShader_t, D3DDEV_SETVERTEXSHADER)(dev, D3DFVF_2D);
	D3D_METHOD(dev, d3dDrawPrimitiveUP_t, D3DDEV_DRAWPRIMITIVEUP)(dev, tipo, nPrim, v, sizeof(Vert2D));
	g_RenderTris += (int)nPrim;
	g_RenderVerts += (int)nVert;
}

void __thiscall CGrafico2D_Draw(CGrafico2D *self, const float *pos, const float *uv, const uint32_t *colors,
	uint32_t nVert, int tipo) {
	if (g_RenderOff) {
		return;
	}
	void *dev = g_MotorDev;
	CHECK(dev);
	CHECK(self->Mat);
	Render_SetMaterialAnim(self->Mat, self->AnimMat);
	int tw, th;
	if (self->sinTextura) {
		th = 1;
		tw = 1;
	} else if (!CMaterial_GetMapSize(self->Mat, 0, &tw, &th)) {
		return;
	}
	float sx = (float)((double)Render_GetViewportW() * g_Pantalla2DInvW);
	float sy = (float)((double)Render_GetViewportH() * g_Pantalla2DInvH);
	float x = self->pantalla ? sx * self->x : self->x;
	x -= 0.5f;
	float y = self->y;
	if (self->pantalla) {
		y *= sy;
	}
	y -= 0.5f;
	float w = (float)tw * self->escala[0] * self->escala2[0];
	float h = (float)th * self->escala[1] * self->escala2[1];
	if (!tipo) {
		w *= self->u1 > self->u0 ? self->u1 - self->u0 : self->u0 - self->u1;
		h *= self->v1 > self->v0 ? self->v1 - self->v0 : self->v0 - self->v1;
	}
	if (self->rot & 1) {
		float t = w;
		w = h;
		h = t;
	}
	if (self->tamPantalla) {
		w *= sx;
		h *= sy;
	}
	if (self->centrado) {
		x -= w * 0.5f;
		y -= h * 0.5f;
	} else {
		x -= self->alignX * w;
		y -= self->alignY * h;
	}
	if (tipo) {
		CHECK(pos && nVert < MAX_VERT_2D);
		uint32_t nPrim = Grafico2D_NumPrims(tipo, nVert);
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
		Draw_Verts(dev, tipo, nPrim, g_Verts2D, nVert);
		return;
	}
	float x2 = w + x, y2 = h + y;
	const float uvs[4][2] = { { self->u0, self->v0 }, { self->u1, self->v0 }, { self->u0, self->v1 },
		{ self->u1, self->v1 } };
	const float xy[4][2] = { { x, y }, { x2, y }, { x, y2 }, { x2, y2 } };
	uint32_t c = Color_Mod4x(self->color);
	Vert2D v[4];
	for (int i = 0; i < 4; i++) {
		const float *t = uvs[g_Grafico2DRot[self->rot][i]];
		v[i] = (Vert2D){ xy[i][0], xy[i][1], 0.0f, 1.0f, c, 0, t[0], t[1] };
	}
	Verts2D_ScaleX(v, 4);
	Draw_Verts(dev, D3DPT_TRIANGLESTRIP, 2, v, 4);
}

void Grafico2D_SetPantalla(int w, int h) {
	g_Pantalla2DW = w;
	g_Pantalla2DH = h;
	g_Pantalla2DInvW = 1.0 / (double)w;
	g_Pantalla2DInvH = 1.0 / (double)h;
}

void __thiscall CGrafico2D_SetTexture(CGrafico2D *self, const char *tex, const char *opac, uint32_t flags) {
	CGrafico2D_CreateMat(self, tex, opac, 5, 6, 0, flags);
}

void __thiscall CGrafico2D_CreateMat(CGrafico2D *self, const char *tex, const char *opac, int src, int dst, byte alpha,
	uint32_t flags) {
	CMaterial m;

	CMaterial_ctor(&m);
	Cad_Set(&m.Nombre, tex);
	CMaterial_SetMap(&m, 0, tex);
	CMaterial_SetMap(&m, 1, opac);
	m.params.srcBlend = src;
	m.params.dstBlend = dst;
	m.params.transp = alpha != 0;
	m.params.diffuse[3] = alpha;
	m.params.b39 = 0;
	((byte *)&m.params.w3e)[0] = 0;
	((byte *)&m.params.w3e)[1] = flags >> 3 & 1;
	m.params.i40 = flags & GRAF2D_MAT_I40 ? 2 : 8;
	m.params.atrib = flags & GRAF2D_ATRIB_20 ? 0x60 : 0x40;
	if (m.mapa[0]) {
		m.mapa[0]->b8 = 0;
		if (!(flags & GRAF2D_MAPA_BC)) {
			m.mapa[0]->bc = 0;
		}
	}
	if (flags & GRAF2D_MAPA_BA && m.mapa[0]) {
		m.mapa[0]->ba = 1;
	}
	if (flags & GRAF2D_PROPIO) {
		m.params.atrib |= 8;
		self->propio = 1;
	}
	if (!self->propio) {
		self->Mat = CListaMat_Find(g_ListaMat, &m);
	}
	if (!self->Mat) {
		self->Mat = Mat_AddCopy(&m);
	}
	CMaterial_Prepare(self->Mat, g_MotorDev);
	AnimMat_Free(&self->AnimMat);
	CMapa *mapa = self->Mat->mapa[0];
	if (mapa) {
		AnimMat_FromMapa(&self->AnimMat, mapa);
		if (flags & GRAF2D_MULTISPRITE) {
			CMapa_BuildMultiSprite(mapa);
		}
	}
	CMaterial_dtor(&m);
}

void __fastcall CGrafico2D_Init(CGrafico2D *self) {
	self->x = 0.0f;
	self->y = 0.0f;
	self->alignX = 0.0f;
	self->alignY = 0.0f;
	self->escala[0] = 1.0f;
	self->escala[1] = 1.0f;
	self->escala2[0] = 1.0f;
	self->escala2[1] = 1.0f;
	self->u0 = 0.0f;
	self->v0 = 0.0f;
	self->u1 = 1.0f;
	self->pantalla = 0;
	self->tamPantalla = 0;
	self->centrado = 0;
	self->sinTextura = 0;
	self->v1 = 1.0f;
	self->color = 0xffffff;
	self->Mat = NULL;
	self->AnimMat = NULL;
	self->propio = 0;
	self->rot = 0;
}

uint32_t Grafico2D_NumPrims(int tipo, uint32_t nVert) {
	CHECK(nVert > 0);
	if (tipo == 4) {
		CHECK((nVert % 3) == 0);
		return nVert / 3;
	}
	if (tipo == 5) {
		CHECK(nVert > 2);
	} else if (tipo == 6) {
		CHECK(nVert > 2);
	} else {
		CHECK(0);
		return 0;
	}
	return nVert - 2;
}

void Grafico2D_GetPantalla(int *w, int *h) {
	if (w) {
		*w = g_Pantalla2DW;
	}
	if (h) {
		*h = g_Pantalla2DH;
	}
}
