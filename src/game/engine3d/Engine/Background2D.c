/*
 * The backgrounds of the engine3d, original source Engine/fondo2d.cpp (include/scrap/background2d.h).
 */
#include "scrap/rdev.h"
#include "scrap/crt.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/background2d.h"
#include "scrap/fog.h"
#include "scrap/render.h"
#include "scrap/misc.h"

CBackground2D *CBackground2D_ctorFile(CBackground2D *self, const char *name) {
	self->bmp.img = NULL;
	CBackground2D_Init(self);
	CBmp_Load(&self->bmp, name, 0);
	return self;
}

CBackground2D *CBackground2D_ctor(CBackground2D *self) {
	self->bmp.img = NULL;
	CBackground2D_Init(self);
	return self;
}

void CBackground2D_Init(CBackground2D *self) {
	CMaterial m;

	self->color.rgba = 0xffffff;
	self->color.intens = 1.0f;
	CMaterial_ctor(&m);
	String_Set(&m.Name, "CFondo2D");
	CMaterial_SetMap(&m, MAP_DIFFUSE, "bmp/fondo_256x256.bmp");
	m.params.lighting = 0;
	m.params.fog = 0;
	m.params.zWrite = 0;
	m.params.zFunc = 8;
	m.map[MAP_DIFFUSE]->filter = 0;
	m.map[MAP_DIFFUSE]->tile = 0;
	m.map[MAP_DIFFUSE]->ba = 1;
	self->Mat = CMatList_Find((&g_MatList), &m);
	if (!self->Mat) {
		CMaterial *p = operator_new(sizeof(CMaterial));
		self->Mat = p ? CMaterial_ctor(p) : NULL;
		CMaterial_Assign(self->Mat, &m);
		CMaterial_Prepare(self->Mat, g_Engine->device);
		CMatList_Add((&g_MatList), self->Mat);
	}
	CMaterial_dtor(&m);
}

void CBackground2D_Render(CBackground2D *self) {
	void *dev = g_Engine->device;
	float p[4][2];
	Vert2D v[4];

	CHECK(dev);
	Render_SetMaterial(self->Mat, 0);
	/* the corners of the four pieces: the half of the viewport each */
	p[0][0] = -0.5f;
	int w = Render_GetViewportW();
	p[2][0] = -0.5f;
	p[0][1] = -0.5f;
	p[1][1] = -0.5f;
	p[1][0] = (float)w * 0.5f - 0.5f;
	p[3][0] = p[1][0];
	int h = Render_GetViewportH();
	p[2][1] = (float)h * 0.5f - 0.5f;
	p[3][1] = p[2][1];
	uint32_t color = Color_ToD3D(&self->color);
	for (int i = 0; i < 4; i++) {
		float x = p[i][0];
		float x2 = (float)Render_GetViewportW() * 0.5f + x;
		int vh = Render_GetViewportH();
		float y = p[i][1];
		float y2 = (float)vh * 0.5f + y;
		v[0] = (Vert2D){ x, y, 0.5f, 0.5f, color, 0, 0.0f, 0.0f };
		v[1] = (Vert2D){ x2, y, 0.5f, 0.5f, color, 0, 1.0f, 0.0f };
		v[2] = (Vert2D){ x, y2, 0.5f, 0.5f, color, 0, 0.0f, 1.0f };
		v[3] = (Vert2D){ x2, y2, 0.5f, 0.5f, color, 0, 1.0f, 1.0f };
		CMap_CopyBmp(self->Mat->map[MAP_DIFFUSE], 0, 0, &self->bmp, (i & 1) << 8, (i >> 1) << 8, BACKGROUND2D_PIECE,
			BACKGROUND2D_PIECE);
		Rd_DevSetVertexShader(dev, D3DFVF_2D);
		Rd_DevDrawPrimitiveUP(dev, D3DPT_TRIANGLESTRIP, 2, v, sizeof(Vert2D));
	}
}

char g_BackgroundCurrent[264];

void Background2D_Reset(void) {
	g_BackgroundCurrent[0] = 0;
}

byte g_BackgroundPending;
int g_BackgroundFrame;
CGraphic2D *g_BackgroundGraphic;
char g_BackgroundNew[264];
uint32_t g_BackgroundColor = 0xffffff;
uint32_t g_BackgroundColorNew = 0xffffff;

void Background2D_Render(void) {
	if (!g_BackgroundPending) {
		return;
	}
	CHECK(g_BackgroundFrame != g_FrameCount);
	g_BackgroundFrame = g_FrameCount;
	g_BackgroundPending = 0;
	g_BackgroundColor = g_BackgroundColorNew;
	if (crt_stricmp(g_BackgroundNew, g_BackgroundCurrent)) {
		/* both BACKGROUND2D_NAME: the unbounded copy of the original */
		Str_Copy(g_BackgroundCurrent, g_BackgroundNew, BACKGROUND2D_NAME);
		if (!g_BackgroundCurrent[0]) {
			return;
		}
		E3D_LogDebug((&g_FileLog), "fondo2d_Render(): nuevo fondo: '%s' (frame: %d)", g_BackgroundCurrent, g_FrameCount);
		if (!g_BackgroundGraphic) {
			int w, h;
			Graphic2D_GetScreen(&w, &h);
			CGraphic2D *p = operator_new(sizeof(CGraphic2D));
			g_BackgroundGraphic = p ? CGraphic2D_ctorTex(p, g_BackgroundCurrent, NULL, GRAPHIC2D_SCREEN | GRAPHIC2D_SCREEN_SIZE) : NULL;
			g_BackgroundGraphic->noTexture = 1;
			g_BackgroundGraphic->x = 0.0f;
			g_BackgroundGraphic->y = 0.0f;
			g_BackgroundGraphic->scale[0] = (float)w;
			g_BackgroundGraphic->scale[1] = (float)h;
		}
	}
	if (g_BackgroundCurrent[0] && g_BackgroundGraphic) {
		g_BackgroundGraphic->color = g_BackgroundColor;
		CGraphic2D_Render(g_BackgroundGraphic);
	}
}

void Background2D_Release(void) {
	if (g_BackgroundGraphic) {
		CGraphic2D_dtor(g_BackgroundGraphic);
		operator_delete_sized(g_BackgroundGraphic, sizeof(CGraphic2D));
		g_BackgroundGraphic = NULL;
	}
}

void Background2D_Set(const char *name, uint32_t color, byte deferred) {
	g_BackgroundPending = 1;
	Str_Copy(g_BackgroundNew, name ? name : "", BACKGROUND2D_NAME);
	g_BackgroundColorNew = color;
	if (!deferred) {
		Background2D_Render();
	}
}
