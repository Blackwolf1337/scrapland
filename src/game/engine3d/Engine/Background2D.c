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

static char g_BackgroundCurrent[264];

void Background2D_Reset(void) {
	g_BackgroundCurrent[0] = 0;
}

static byte g_BackgroundPending;
static int g_BackgroundFrame;
static CGraphic2D *g_BackgroundGraphic;
static char g_BackgroundNew[264];
static uint32_t g_BackgroundColor = 0xffffff;
static uint32_t g_BackgroundColorNew = 0xffffff;

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
		operator_delete(g_BackgroundGraphic);
		g_BackgroundGraphic = NULL;
	}
}

