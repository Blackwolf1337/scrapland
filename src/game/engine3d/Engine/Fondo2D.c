/*
 * The backgrounds of the engine3d, original source Engine/fondo2d.cpp (include/scrap/fondo2d.h).
 */
#include "scrap/crt.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/fondo2d.h"
#include "scrap/niebla.h"
#include "scrap/render.h"
#include "scrap/varios.h"

CFondo2D *__thiscall CFondo2D_ctorFile(CFondo2D *self, const char *name) {
	self->bmp.img = NULL;
	CFondo2D_Init(self);
	CBmp_Load(&self->bmp, name, 0);
	return self;
}

CFondo2D *__fastcall CFondo2D_ctor(CFondo2D *self) {
	self->bmp.img = NULL;
	CFondo2D_Init(self);
	return self;
}

void __fastcall CFondo2D_Init(CFondo2D *self) {
	CMaterial m;

	self->color.rgba = 0xffffff;
	self->color.intens = 1.0f;
	CMaterial_ctor(&m);
	Cad_Set(&m.Nombre, "CFondo2D");
	CMaterial_SetMap(&m, MAPA_DIFFUSE, "bmp/fondo_256x256.bmp");
	m.params.b39 = 0;
	m.params.w3e = 0;
	m.params.i40 = 8;
	m.mapa[MAPA_DIFFUSE]->b8 = 0;
	m.mapa[MAPA_DIFFUSE]->bc = 0;
	m.mapa[MAPA_DIFFUSE]->ba = 1;
	self->Mat = CListaMat_Find(g_ListaMat, &m);
	if (!self->Mat) {
		CMaterial *p = operator_new(sizeof(CMaterial));
		self->Mat = p ? CMaterial_ctor(p) : NULL;
		CMaterial_Assign(self->Mat, &m);
		CMaterial_Prepare(self->Mat, MOTOR_DEV(g_Motor));
		CListaMat_Add(g_ListaMat, self->Mat);
	}
	CMaterial_dtor(&m);
}

void __fastcall CFondo2D_Render(CFondo2D *self) {
	void *dev = MOTOR_DEV(g_Motor);
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
		CMapa_CopyBmp(self->Mat->mapa[MAPA_DIFFUSE], 0, 0, &self->bmp, (i & 1) << 8, (i >> 1) << 8, FONDO2D_PIECE,
			FONDO2D_PIECE);
		D3D_METHOD(dev, d3dSetVertexShader_t, D3DDEV_SETVERTEXSHADER)(dev, D3DFVF_2D);
		D3D_METHOD(dev, d3dDrawPrimitiveUP_t, D3DDEV_DRAWPRIMITIVEUP)(dev, D3DPT_TRIANGLESTRIP, 2, v, sizeof(Vert2D));
	}
}

void Fondo2D_Reset(void) {
	g_FondoActual[0] = 0;
}

void Fondo2D_Render(void) {
	if (!g_FondoPendiente) {
		return;
	}
	CHECK(g_FondoFrame != g_FrameCount);
	g_FondoFrame = g_FrameCount;
	g_FondoPendiente = 0;
	g_FondoColor = g_FondoColorNuevo;
	if (crt_stricmp(g_FondoNuevo, g_FondoActual)) {
		/* both FONDO2D_NAME: the unbounded copy of the original */
		Str_Copy(g_FondoActual, g_FondoNuevo, FONDO2D_NAME);
		if (!g_FondoActual[0]) {
			return;
		}
		E3D_LogDebug(g_FileLog, "fondo2d_Render(): nuevo fondo: '%s' (frame: %d)", g_FondoActual, g_FrameCount);
		if (!g_FondoGrf) {
			int w, h;
			Grafico2D_GetPantalla(&w, &h);
			CGrafico2D *p = operator_new(sizeof(CGrafico2D));
			g_FondoGrf = p ? CGrafico2D_ctorTex(p, g_FondoActual, NULL, GRAF2D_PANTALLA | GRAF2D_TAM_PANTALLA) : NULL;
			g_FondoGrf->sinTextura = 1;
			g_FondoGrf->x = 0.0f;
			g_FondoGrf->y = 0.0f;
			g_FondoGrf->escala[0] = (float)w;
			g_FondoGrf->escala[1] = (float)h;
		}
	}
	if (g_FondoActual[0] && g_FondoGrf) {
		g_FondoGrf->color = g_FondoColor;
		CGrafico2D_Render(g_FondoGrf);
	}
}

void Fondo2D_Release(void) {
	if (g_FondoGrf) {
		CGrafico2D_dtor(g_FondoGrf);
		operator_delete_sized(g_FondoGrf, sizeof(CGrafico2D));
		g_FondoGrf = NULL;
	}
}

void Fondo2D_Set(const char *name, uint32_t color, byte deferred) {
	g_FondoPendiente = 1;
	Str_Copy(g_FondoNuevo, name ? name : "", FONDO2D_NAME);
	g_FondoColorNuevo = color;
	if (!deferred) {
		Fondo2D_Render();
	}
}
