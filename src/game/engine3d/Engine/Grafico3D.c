/*
 * Engine/grafico3d.cpp of the engine3d: CGrafico3D, see include/scrap/grafico3d.h.
 */
#include <string.h>

#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/fuentes.h"
#include "scrap/grafico3d.h"
#include "scrap/mallatri.h"
#include "scrap/sistpart.h"

#define D3DFVF_SPRITE 0x1c2		/* XYZ | DIFFUSE | SPECULAR | TEX1 */

/* the members both constructors clear */
static void Grafico3D_Clear(CGrafico3D *self) {
	self->vftable = CGRAFICO3D_VFTABLE;
	memset(&self->vis, 0, sizeof(self->vis));
	self->pos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->ancho = 1.0f;
	self->alto = 1.0f;
	self->clase = CLASE_OBJ3D_GRAFICO3D;
	self->Mat = NULL;
	self->AnimMat = NULL;
	self->nodo = NULL;
}

CGrafico3D *__thiscall CGrafico3D_ctorTex(CGrafico3D *self, const char *tex, const vec3_t *pos, const float *size) {
	Grafico3D_Clear(self);
	CGrafico3D_CreateMat(self, tex, NULL, 5, 2, 0xff);
	self->pos = *pos;
	self->ancho = size[0];
	self->alto = size[1];
	return self;
}

CGrafico3D *__thiscall CGrafico3D_ctorTex2(CGrafico3D *self, const char *tex, const char *opac) {
	Grafico3D_Clear(self);
	CGrafico3D_SetTexture(self, tex, opac);
	return self;
}

/* the constructor of the vertices (nothing to do), the static vertex arrays of the exe call it */
Grafico3DVert *__fastcall Grafico3DVert_ctor(Grafico3DVert *self) {
	return self;
}

CGrafico3D *__fastcall CGrafico3D_ctor(CGrafico3D *self) {
	Grafico3D_Clear(self);
	return self;
}

void __fastcall CGrafico3D_dtor(CGrafico3D *self) {
	self->vftable = CGRAFICO3D_VFTABLE;
	ObjRef_Drop(&self->nodo);
	self->nodo = NULL;
	AnimMat_Free(&self->AnimMat);
	self->vftable = IOBJETO3D_VFTABLE;
}

CGrafico3D *__thiscall CGrafico3D_Delete(CGrafico3D *self, byte flags) {
	CGrafico3D_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CGrafico3D));
	}
	return self;
}

byte __thiscall CGrafico3D_Read(CGrafico3D *self, CArchIO *f, MallaD3D_FileCtx *ctx) {
	char tex[0x100], opac[0x100];
	byte diffuse[4];

	CGrafico3D_SetNodo(self, ctx->nodo);
	CArchIO_BeginBlock(f, "SPR3");
	CHECK(CArchIO_ReadInt(f) == 1);
	CArchIO_Read(f, &self->pos, 0xc);
	CArchIO_Read(f, &self->ancho, 8);
	CArchIO_ReadString(f, tex, sizeof(tex));
	CArchIO_ReadString(f, opac, sizeof(opac));
	CArchIO_Read(f, diffuse, 4);
	CHECK(!g_MotorDev);
	CGrafico3D_CreateMat(self, tex[0] ? tex : NULL, opac[0] ? opac : NULL, 5, 2, diffuse[3]);
	CArchIO_Error(f, "SPR3");
	return 1;
}

void __fastcall CGrafico3D_Draw(CGrafico3D *self) {
	void *dev = g_MotorDev;
	CHECK(dev);
	CHECK(self->Mat);
	Render_SetMaterialAnim(self->Mat, self->AnimMat);
	matrix_t m = *Render_GetBillboard();
	m.m[12] = self->pos.x;
	m.m[13] = self->pos.y;
	m.m[14] = self->pos.z;
	Render_SetWorldMatrix(&m, 0);
	float hw = self->ancho * 0.5f;
	float hh = self->alto * 0.5f;
	uint32_t a = (uint32_t)self->Mat->params.diffuse[3] << 24;
	uint32_t color = g_DiffuseMod4x ? a | 0x3f3f3f : a | 0xffffff;
	Grafico3DVert v[4] = {
		{ { -hw, hh, 0.0f }, color, 0, 0.0f, 0.0f },
		{ { hw, hh, 0.0f }, color, 0, 1.0f, 0.0f },
		{ { -hw, -hh, 0.0f }, color, 0, 0.0f, 1.0f },
		{ { hw, -hh, 0.0f }, color, 0, 1.0f, 1.0f },
	};
	D3D_METHOD(dev, d3dSetVertexShader_t, D3DDEV_SETVERTEXSHADER)(dev, D3DFVF_SPRITE);
	D3D_METHOD(dev, d3dDrawPrimitiveUP_t, D3DDEV_DRAWPRIMITIVEUP)(dev, D3DPT_TRIANGLESTRIP, 2, v, sizeof(v[0]));
	g_RenderTris += 2;
	g_RenderVerts += 4;
}

void __thiscall CGrafico3D_Render(CGrafico3D *self, void *dev, CNodoR *r) {
	(void)dev;
	CNodo3D *n = self->nodo;
	if (n) {
		self->pos = (vec3_t){ n->m.m[12], n->m.m[13], n->m.m[14] };
	}
	if (!r && g_ShowSprites) {
		float s = self->ancho <= self->alto ? self->alto : self->ancho;
		byte vis = SectorVis_Sphere(&self->vis, &self->pos, &self->pos, s * 0.5f, n->name);
		if (g_ShowSprites > 1) {
			SectorVis_Draw(&self->vis, &self->pos, vis, 0, self->nodo->name);
		}
		if (g_ShowSprites > 2) {
			Debug_Text3D(g_FntTerm8, 4.0f, -12.0f, &self->pos, 0x8000, GAME_STR(str_3bae88, 0x3bae88), self->nodo->name);
		}
		if (!vis) {
			return;
		}
	}
	CGrafico3D_Draw(self);
}

byte __thiscall CGrafico3D_Write(CGrafico3D *self, CArchIO *f, MallaD3D_FileCtx *ctx) {
	CHECK(Wild_Match("sprite*", ctx->nodo->name));
	CHECK(self->nodo == ctx->nodo);
	CArchIO_BeginBlockWrite(f, "SPR3");
	CArchIO_WriteInt(f, 1);
	CArchIO_Write(f, &self->pos, 0xc);
	CArchIO_Write(f, &self->ancho, 8);
	CHECK(self->Mat->params.srcBlend == 5);
	CHECK(self->Mat->params.dstBlend == 2);
	CArchIO_WriteString(f, CMaterial_GetMapName(self->Mat, 0));
	CArchIO_WriteString(f, CMaterial_GetMapName(self->Mat, 1));
	CArchIO_Write(f, self->Mat->params.diffuse, 4);
	CArchIO_EndBlockWrite(f, "SPR3");
	return 1;
}

void __thiscall CGrafico3D_SetNodo(CGrafico3D *self, CNodo3D *nodo) {
	if (nodo) {
		nodo->Refs++;
	}
	ObjRef_Drop(&self->nodo);
	self->nodo = nodo;
}

void __thiscall CGrafico3D_SetTexture(CGrafico3D *self, const char *tex, const char *opac) {
	CGrafico3D_CreateMat(self, tex, opac, 5, opac ? 6 : 2, opac ? 0 : 0xff);
}

void __thiscall CGrafico3D_CreateMat(CGrafico3D *self, const char *tex, const char *opac, int src, int dst, byte alpha) {
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
	((byte *)&m.params.w3e)[1] = 0;
	if (m.mapa[0]) {
		m.mapa[0]->b8 = 0;
		m.mapa[0]->bc = 0;
	}
	self->Mat = CListaMat_Find(g_ListaMat, &m);
	if (!self->Mat) {
		self->Mat = Mat_AddCopy(&m);
	}
	CMaterial_Prepare(self->Mat, g_MotorDev);
	AnimMat_Free(&self->AnimMat);
	AnimMat_FromMapa(&self->AnimMat, self->Mat->mapa[0]);
	CMaterial_dtor(&m);
}

void __fastcall CGrafico3D_Init(CGrafico3D *self) {
	self->pos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->ancho = 1.0f;
	self->clase = CLASE_OBJ3D_GRAFICO3D;
	self->alto = 1.0f;
	self->Mat = NULL;
	self->AnimMat = NULL;
	self->nodo = NULL;
}
