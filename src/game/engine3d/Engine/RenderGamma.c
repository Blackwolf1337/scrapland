/*
 * The gamma ramp of the engine3d, original source Engine/RenderGamma.cpp (include/scrap/render.h): from the video
 * variables VideoGammaRGB (also a black level above 1), VideoScaleRGB and the three channel scales, doubled with
 * gamma2x. The values of the last ramp are kept so it is only set again when one changes.
 */
#include "scrap/gvar.h"
#include "scrap/bsp.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/render.h"
#include "scrap/misc.h"

#define GAMMA_RAMP 256
extern cGVar GVar_VideoGammaRGB;
#define g_VideoGammaRGB (GVar_VideoGammaRGB.value.f)
extern cGVar GVar_VideoScaleRGB;
#define g_VideoScaleRGB (GVar_VideoScaleRGB.value.f)
extern cGVar GVar_VideoScaleRed;
#define g_VideoScaleRed (GVar_VideoScaleRed.value.f)
extern cGVar GVar_VideoScaleGreen;
#define g_VideoScaleGreen (GVar_VideoScaleGreen.value.f)
extern cGVar GVar_VideoScaleBlue;
#define g_VideoScaleBlue (GVar_VideoScaleBlue.value.f)
/* the values of the ramp set last */
extern int g_GammaLast2x;
extern float g_GammaLastRGB;
extern float g_GammaLastScale;
extern float g_GammaLastRed;
extern float g_GammaLastGreen;
extern float g_GammaLastBlue;

/* one channel through the curve, limited to 0..65535 */
static uint16_t Gamma_Channel(uint16_t w, float expo, float mult, float scale) {
	int v = (int)(Game_Pow((double)w * 0x1p-16, (double)expo) * 65536.0 * (double)mult * (double)scale);
	if (v < 1) {
		return 0;
	}
	return v > 0xfffe ? 0xffff : (uint16_t)v;
}

void RenderGamma_Begin(void) {
	void *dev = g_Engine->device;
	uint16_t ramp[3][GAMMA_RAMP];

	CHECK(dev);
	float gamma = g_VideoGammaRGB;
	float scale = g_VideoScaleRGB;
	float red = g_VideoScaleRed;
	float green = g_VideoScaleGreen;
	float blue = g_VideoScaleBlue;
	int x2 = g_Gamma2x;
	if (g_GammaLast2x == x2 && g_GammaLastRGB == gamma && g_GammaLastScale == scale && g_GammaLastRed == red &&
		g_GammaLastGreen == green && g_GammaLastBlue == blue) {
		return;
	}
	g_GammaLast2x = x2;
	g_GammaLastRGB = gamma;
	g_GammaLastScale = scale;
	g_GammaLastRed = red;
	g_GammaLastGreen = green;
	g_GammaLastBlue = blue;
	/* above 1 the gamma also lifts the black level */
	int black = gamma > 1.0f ? (int)(gamma * 4.0f) : 0;
	for (int i = 0; i < GAMMA_RAMP; i++) {
		uint16_t w = (uint16_t)((i < black ? black : i) << 8);
		ramp[0][i] = w;
		ramp[1][i] = w;
		ramp[2][i] = w;
	}
	float expo = 1.0f / gamma;
	float mult = (x2 ? 2.0f : 1.0f) * scale;
	for (int i = 0; i < GAMMA_RAMP; i++) {
		uint16_t r = Gamma_Channel(ramp[0][i], expo, mult, g_VideoScaleRed);
		uint16_t g = Gamma_Channel(ramp[1][i], expo, mult, g_VideoScaleGreen);
		uint16_t b = Gamma_Channel(ramp[2][i], expo, mult, g_VideoScaleBlue);
		ramp[0][i] = r;
		ramp[1][i] = g;
		ramp[2][i] = b;
	}
	/* the high byte also as the low one */
	for (int c = 0; c < 3; c++) {
		for (int i = 0; i < GAMMA_RAMP; i++) {
			ramp[c][i] = (ramp[c][i] & 0xff00) | ramp[c][i] >> 8;
		}
	}
	/* the first entry changed and back, so the device sets the ramp even when it holds it already */
	ramp[0][0] ^= 1;
	D3D_METHOD(dev, d3dSetGammaRamp_t, D3DDEV_SETGAMMARAMP)(dev, 0, ramp);
	ramp[0][0] ^= 1;
	D3D_METHOD(dev, d3dSetGammaRamp_t, D3DDEV_SETGAMMARAMP)(dev, 0, ramp);
	E3D_LogDebug(g_FileLog, "DBG: GAMMACONTROL: OK");
}

void RenderGamma_ReleaseD3D(void) {
	CHECK(g_pd3dDevice);
	g_GammaLast2x = 0;
	g_GammaLastRGB = 1.0f;
	g_GammaLastScale = 1.0f;
	g_GammaLastRed = 1.0f;
	g_GammaLastGreen = 1.0f;
	g_GammaLastBlue = 1.0f;
}
