/*
 * Engine/grafico3d.cpp of the engine3d: CGraphic3D, see include/scrap/graphic3d.h.
 */
#include <string.h>

#include "scrap/rdev.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/fonts.h"
#include "scrap/graphic3d.h"
#include "scrap/trimesh.h"
#include "scrap/particlesystem.h"
#include "scrap/staticinit.h"

#define CGRAPHIC3D_VFTABLE (&CGraphic3D_vftable)
#define g_ShowSprites (GVar_R_SpriteSector.value.i)	/* 1: visibility, 2: drawn, 3: with the name */
static cGVar GVar_R_SpriteSector;
static const IObject3DVtbl CGraphic3D_vftable;
#define D3DFVF_SPRITE 0x1c2		/* XYZ | DIFFUSE | SPECULAR | TEX1 */

/* the members both constructors clear */
static void Graphic3D_Clear(CGraphic3D *self) {
	self->vftable = CGRAPHIC3D_VFTABLE;
	memset(&self->vis, 0, sizeof(self->vis));
	self->pos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->width = 1.0f;
	self->height = 1.0f;
	self->class = CLASS_OBJ3D_GRAPHIC3D;
	self->Mat = NULL;
	self->AnimMat = NULL;
	self->node = NULL;
}

/* with the texture and its opacity map (CGraphic3D_SetTexture) */
/* the constructor of the vertices (nothing to do), the static vertex arrays of the exe call it */
Graphic3DVert *Graphic3DVert_ctor(Graphic3DVert *self) {
	return self;
}

CGraphic3D *CGraphic3D_ctor(CGraphic3D *self) {
	Graphic3D_Clear(self);
	return self;
}

static void CGraphic3D_dtor(CGraphic3D *self) {
	self->vftable = CGRAPHIC3D_VFTABLE;
	ObjRef_Drop(&self->node);
	self->node = NULL;
	AnimMat_Free(&self->AnimMat);
	self->vftable = IOBJECT3D_VFTABLE;
}

static void CGraphic3D_Free(IObject3D *base) {
	CGraphic3D *self = (CGraphic3D *)base;
	CGraphic3D_dtor(self);
	operator_delete(self);
}

static byte CGraphic3D_Read(IObject3D *base, CFileIO *f, MeshD3D_FileCtx *ctx) {
	CGraphic3D *self = (CGraphic3D *)base;
	char tex[0x100], opac[0x100];
	byte diffuse[4];

	CGraphic3D_SetNode(self, ctx->node);
	CFileIO_BeginBlock(f, "SPR3");
	CHECK(CFileIO_ReadInt(f) == 1);
	CFileIO_Read(f, &self->pos, 0xc);
	CFileIO_Read(f, &self->width, 8);
	CFileIO_ReadString(f, tex, sizeof(tex));
	CFileIO_ReadString(f, opac, sizeof(opac));
	CFileIO_Read(f, diffuse, 4);
	CHECK(!g_Engine->device);
	CGraphic3D_CreateMat(self, tex[0] ? tex : NULL, opac[0] ? opac : NULL, 5, 2, diffuse[3]);
	CFileIO_Error(f, "SPR3");
	return 1;
}

static void CGraphic3D_Draw(CGraphic3D *self) {
	void *dev = g_Engine->device;
	CHECK(dev);
	CHECK(self->Mat);
	Render_SetMaterialAnim(self->Mat, self->AnimMat);
	matrix_t m = *Render_GetBillboard();
	m.m[12] = self->pos.x;
	m.m[13] = self->pos.y;
	m.m[14] = self->pos.z;
	Render_SetWorldMatrix(&m, 0);
	float hw = self->width * 0.5f;
	float hh = self->height * 0.5f;
	uint32_t a = (uint32_t)self->Mat->params.diffuse[3] << 24;
	uint32_t color = g_DiffuseMod4x ? a | 0x3f3f3f : a | 0xffffff;
	Graphic3DVert v[4] = {
		{ { -hw, hh, 0.0f }, color, 0, 0.0f, 0.0f },
		{ { hw, hh, 0.0f }, color, 0, 1.0f, 0.0f },
		{ { -hw, -hh, 0.0f }, color, 0, 0.0f, 1.0f },
		{ { hw, -hh, 0.0f }, color, 0, 1.0f, 1.0f },
	};
	Rd_DevSetVertexShader(dev, D3DFVF_SPRITE);
	Rd_DevDrawPrimitiveUP(dev, D3DPT_TRIANGLESTRIP, 2, v, sizeof(v[0]));
	g_RenderTris += 2;
	g_RenderVerts += 4;
}

static void CGraphic3D_Render(IObject3D *base, void *dev, CRenderNode *r) {
	CGraphic3D *self = (CGraphic3D *)base;
	(void)dev;
	CNode3D *n = self->node;
	if (n) {
		self->pos = (vec3_t){ n->m.m[12], n->m.m[13], n->m.m[14] };
	}
	if (!r && g_ShowSprites) {
		float s = self->width <= self->height ? self->height : self->width;
		byte vis = SectorVis_Sphere(&self->vis, &self->pos, &self->pos, s * 0.5f, n->name);
		if (g_ShowSprites > 1) {
			SectorVis_Draw(&self->vis, &self->pos, vis, 0, self->node->name);
		}
		if (g_ShowSprites > 2) {
			Debug_Text3D(g_FntTerm8, 4.0f, -12.0f, &self->pos, 0x8000, "%s", self->node->name);
		}
		if (!vis) {
			return;
		}
	}
	CGraphic3D_Draw(self);
}

static byte CGraphic3D_Write(IObject3D *base, CFileIO *f, MeshD3D_FileCtx *ctx) {
	CGraphic3D *self = (CGraphic3D *)base;
	CHECK(Wild_Match("sprite*", ctx->node->name));
	CHECK(self->node == ctx->node);
	CFileIO_BeginBlockWrite(f, "SPR3");
	CFileIO_WriteInt(f, 1);
	CFileIO_Write(f, &self->pos, 0xc);
	CFileIO_Write(f, &self->width, 8);
	CHECK(self->Mat->params.srcBlend == 5);
	CHECK(self->Mat->params.dstBlend == 2);
	CFileIO_WriteString(f, CMaterial_GetMapName(self->Mat, 0));
	CFileIO_WriteString(f, CMaterial_GetMapName(self->Mat, 1));
	CFileIO_Write(f, self->Mat->params.diffuse, 4);
	CFileIO_EndBlockWrite(f, "SPR3");
	return 1;
}

void CGraphic3D_SetNode(CGraphic3D *self, CNode3D *node) {
	if (node) {
		node->Refs++;
	}
	ObjRef_Drop(&self->node);
	self->node = node;
}

void CGraphic3D_CreateMat(CGraphic3D *self, const char *tex, const char *opac, int src, int dst, byte alpha) {
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
	m.params.zWrite = 0;
	if (m.map[0]) {
		m.map[0]->filter = 0;
		m.map[0]->tile = 0;
	}
	self->Mat = CMatList_Find((&g_MatList), &m);
	if (!self->Mat) {
		self->Mat = Mat_AddCopy(&m);
	}
	CMaterial_Prepare(self->Mat, g_Engine->device);
	AnimMat_Free(&self->AnimMat);
	AnimMat_FromMap(&self->AnimMat, self->Mat->map[0]);
	CMaterial_dtor(&m);
}

/* the vtable of CGraphic3D */
static const IObject3DVtbl CGraphic3D_vftable = {
	.Free = CGraphic3D_Free,
	.Update = IObject3D_UpdateNop,
	.Render = CGraphic3D_Render,
	.InitD3D = IObject3D_InitD3DNop,
	.ReleaseD3D = IObject3D_ReleaseD3DNop,
	.Write = CGraphic3D_Write,
	.Read = CGraphic3D_Read,
};

static cGVar GVar_R_SpriteSector;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_R_SpriteSector, &GVar_R_SpriteSector, "R_SpriteSector", 0, "Use sector visibility for sprites", 1)
