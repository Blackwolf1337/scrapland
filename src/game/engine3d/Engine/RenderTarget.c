/*
 * Render targets of the engine3d, original source Engine/RenderTarget.cpp (include/scrap/rendertarget.h).
 */
#include <string.h>

#include "scrap/archio.h"
#include "scrap/console.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/graphic2d.h"
#include "scrap/material.h"
#include "scrap/model.h"
#include "scrap/render.h"
#include "scrap/rendertarget.h"
#include "scrap/scrapmodel.h"
#include "scrap/trails.h"

/* IDirect3DDevice8, IDirect3DTexture8 and IDirect3DSurface8 methods not in d3d8.h */
#define D3DDEV_CREATEDEPTHSTENCILSURFACE 0x68
#define D3DDEV_BEGINSCENE 0x88
#define D3DDEV_ENDSCENE 0x8c
#define D3DSURF_GETDESC 0x20
#define D3DSURF_LOCKRECT 0x24
typedef int(__stdcall *rtCreateTexture_t)(void *dev, uint32_t w, uint32_t h, uint32_t levels, uint32_t usage, int fmt,
	int pool, void **tex);
typedef int(__stdcall *rtCreateDepthStencil_t)(void *dev, uint32_t w, uint32_t h, int fmt, int multisample,
	void **surf);
typedef int(__stdcall *rtSetRenderTarget_t)(void *dev, void *surf, void *z);
typedef int(__stdcall *rtGetSurface_t)(void *dev, void **surf);
typedef int(__stdcall *rtGetRenderState_t)(void *dev, uint32_t state, uint32_t *value);
typedef int(__stdcall *rtGetSurfaceLevel_t)(void *tex, uint32_t level, void **surf);
typedef int(__stdcall *rtCreateImageSurface_t)(void *dev, uint32_t w, uint32_t h, int fmt, void **surf);
typedef int(__stdcall *rtCopyRects_t)(void *dev, void *src, const int *rects, uint32_t n, void *dst, const void *pts);
typedef int(__stdcall *rtScene_t)(void *dev);
typedef int(__stdcall *rtGetDesc_t)(void *surf, uint32_t *desc);	/* D3DSURFACE_DESC, 8 words */
typedef struct RTLockedRect {
	int Pitch;
	byte *pBits;
} RTLockedRect;
typedef int(__stdcall *rtLockRect_t)(void *surf, RTLockedRect *lr, const int *rect, uint32_t flags);

#define D3DFMT_A8R8G8B8 0x15
#define D3DFILL_SOLID 3
#define D3DLOCK_READONLY 0x10

/* the classes of the scene nodes drawn (the DuDv pass draws only its own) */

static void *RT_Dev(void) {
	return g_pd3dDevice;
}

/* a render target texture of w x h, A8R8G8B8 (HRESULT) */
static int RT_CreateTarget(uint32_t w, uint32_t h, void **tex) {
	void *dev = RT_Dev();
	return D3D_METHOD(dev, rtCreateTexture_t, D3DDEV_CREATETEXTURE)(dev, w, h, 1, D3DUSAGE_RENDERTARGET,
		D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, tex);
}

static void RT_SetRenderTarget(void *surf, void *z) {
	void *dev = RT_Dev();
	D3D_METHOD(dev, rtSetRenderTarget_t, D3DDEV_SETRENDERTARGET)(dev, surf, z);
}

static void RT_GetSurfaceLevel(void *tex, void **surf) {
	D3D_METHOD(tex, rtGetSurfaceLevel_t, D3DTEX_GETSURFACELEVEL)(tex, 0, surf);
}

static void RT_GetTSS(uint32_t stage, uint32_t type, uint32_t *value) {
	void *dev = RT_Dev();
	D3D_METHOD(dev, d3dGetTextureStageState_t, D3DDEV_GETTEXTURESTAGESTATE)(dev, stage, type, value);
}

/* surface 0 of tex as the render target without a depth buffer */
static void RT_SetTargetTexture(void *tex) {
	void *surf = NULL;
	RT_GetSurfaceLevel(tex, &surf);
	RT_SetRenderTarget(surf, NULL);
	COM_RELEASE(surf);
}

static void RT_SetRS(uint32_t state, uint32_t value) {
	void *dev = RT_Dev();
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, state, value);
}

static void RT_Release(void **obj) {
	if (*obj) {
		COM_RELEASE(*obj);
		*obj = NULL;
	}
}

static void RT_TSS(uint32_t stage, uint32_t type, uint32_t value) {
	void *dev = RT_Dev();
	D3D_METHOD(dev, d3dSetTextureStageState_t, D3DDEV_SETTEXTURESTAGESTATE)(dev, stage, type, value);
}

static void RT_SetTexture(uint32_t stage, void *tex) {
	void *dev = RT_Dev();
	D3D_METHOD(dev, d3dSetTexture_t, D3DDEV_SETTEXTURE)(dev, stage, tex);
}

static void RT_GetViewport(uint32_t *vp) {
	void *dev = RT_Dev();
	D3D_METHOD(dev, d3dGetViewport_t, D3DDEV_GETVIEWPORT)(dev, vp);
}

/* an unsigned viewport word as the exe converts it (through double) */
static float RT_UToF(uint32_t u) {
	return (float)(double)u;
}

/* FVF 0x1c4: XYZRHW, DIFFUSE, SPECULAR, TEX1 */
#define RT_FVF 0x1c4
typedef struct RTVert {
	float x, y, z, rhw;
	uint32_t diffuse;
	uint32_t specular;
	float u, v;
} RTVert;
SD_SIZE(RTVert, 0x20);

/* a screen rectangle as a strip of four vertices: x0 y0, x1 y0, x0 y1, x1 y1 */
static void RT_Quad(RTVert *v, float x0, float y0, float x1, float y1, uint32_t color, float u0, float v0, float u1,
	float v1) {
	for (int i = 0; i < 4; i++) {
		v[i].x = i & 1 ? x1 : x0;
		v[i].y = i & 2 ? y1 : y0;
		v[i].z = 0.0f;
		v[i].rhw = 1.0f;
		v[i].diffuse = color;
		v[i].specular = 0;
		v[i].u = i & 1 ? u1 : u0;
		v[i].v = i & 2 ? v1 : v0;
	}
}

/* the texture coordinates in texels of a texture of su x sv (g_RTLinPow2, g_RTLinNonPow2) */
static void RT_QuadTexels(RTVert *v, float su, float sv) {
	for (int i = 0; i < 4; i++) {
		v[i].u = su * v[i].u;
		v[i].v = v[i].v * sv;
	}
}

/* the viewport as a quad from 0, 0 with the texture coordinates 0..1 moved by du, dv (the texel centers), in
 * texels of the scene size with g_RTLinNonPow2 */
static void RT_ViewQuad(RTVert *v, const uint32_t *vp, uint32_t color, float du, float dv) {
	RT_Quad(v, 0.0f, 0.0f, RT_UToF(vp[2]), RT_UToF(vp[3]), color, du + 0.0f, dv + 0.0f, du + 1.0f, dv + 1.0f);
	if (g_RTLinNonPow2) {
		RT_QuadTexels(v, g_SceneW, g_SceneH);
	}
}

static void RT_DrawQuad(const RTVert *v) {
	void *dev = RT_Dev();
	D3D_METHOD(dev, d3dSetVertexShader_t, D3DDEV_SETVERTEXSHADER)(dev, RT_FVF);
	dev = RT_Dev();
	D3D_METHOD(dev, d3dDrawPrimitiveUP_t, D3DDEV_DRAWPRIMITIVEUP)(dev, D3DPT_TRIANGLESTRIP, 2, v, sizeof(RTVert));
}

/* FVF 0x2c4: XYZRHW, DIFFUSE, SPECULAR, TEX2 */
#define RT_FVF2 0x2c4
typedef struct RTVert2 {
	float x, y, z, rhw;
	uint32_t diffuse;
	uint32_t specular;
	float t[2][2];
} RTVert2;
SD_SIZE(RTVert2, 0x28);

/* FVF 0x444: XYZRHW, DIFFUSE, TEX4 */
#define RT_FVF4 0x444
typedef struct RTVert4 {
	float x, y, z, rhw;
	uint32_t diffuse;
	float t[4][2];
} RTVert4;
SD_SIZE(RTVert4, 0x34);

/* the viewport w x h from 0, 0 with two texture sets, each 0..1 moved by the texel center of its texture size;
 * in texels with g_RTLinNonPow2 */
static void RT_Quad2(RTVert2 *v, float w, float h, uint32_t color, float w0, float h0, float w1, float h1) {
	float du0 = 0.5f / w0, dv0 = 0.5f / h0;
	float du1 = 0.5f / w1, dv1 = 0.5f / h1;
	for (int i = 0; i < 4; i++) {
		int cx = i & 1;
		int cy = i >> 1;
		v[i].x = cx ? w : 0.0f;
		v[i].y = cy ? h : 0.0f;
		v[i].z = 0.0f;
		v[i].rhw = 1.0f;
		v[i].diffuse = color;
		v[i].specular = 0;
		v[i].t[0][0] = du0 + (float)cx;
		v[i].t[0][1] = dv0 + (float)cy;
		v[i].t[1][0] = du1 + (float)cx;
		v[i].t[1][1] = dv1 + (float)cy;
	}
	if (g_RTLinNonPow2) {
		for (int i = 0; i < 4; i++) {
			v[i].t[0][0] = v[i].t[0][0] * w0;
			v[i].t[0][1] = v[i].t[0][1] * h0;
			v[i].t[1][0] = v[i].t[1][0] * w1;
			v[i].t[1][1] = v[i].t[1][1] * h1;
		}
	}
	void *dev = RT_Dev();
	D3D_METHOD(dev, d3dSetVertexShader_t, D3DDEV_SETVERTEXSHADER)(dev, RT_FVF2);
	dev = RT_Dev();
	D3D_METHOD(dev, d3dDrawPrimitiveUP_t, D3DDEV_DRAWPRIMITIVEUP)(dev, D3DPT_TRIANGLESTRIP, 2, v, sizeof(RTVert2));
}

/* a quad from 0, y0 to w, y1 with four texture sets: u[cx][set] on the left and right side, t[cy][set] on the top
 * and bottom; the coordinates in texels of su x sv when texels is set */
static void RT_Quad4Y(float w, float y0, float y1, const float u[2][4], const float t[2][4], byte texels, float su,
	float sv) {
	RTVert4 v[4];

	for (int i = 0; i < 4; i++) {
		int cx = i & 1;
		int cy = i >> 1;
		v[i].x = cx ? w : 0.0f;
		v[i].y = cy ? y1 : y0;
		v[i].z = 0.0f;
		v[i].rhw = 1.0f;
		v[i].diffuse = 0xffffffff;
		for (int k = 0; k < 4; k++) {
			v[i].t[k][0] = u[cx][k];
			v[i].t[k][1] = t[cy][k];
		}
	}
	if (texels) {
		for (int i = 0; i < 4; i++) {
			for (int k = 0; k < 4; k++) {
				v[i].t[k][0] = v[i].t[k][0] * su;
				v[i].t[k][1] = v[i].t[k][1] * sv;
			}
		}
	}
	void *dev = RT_Dev();
	D3D_METHOD(dev, d3dSetVertexShader_t, D3DDEV_SETVERTEXSHADER)(dev, RT_FVF4);
	dev = RT_Dev();
	D3D_METHOD(dev, d3dDrawPrimitiveUP_t, D3DDEV_DRAWPRIMITIVEUP)(dev, D3DPT_TRIANGLESTRIP, 2, v, sizeof(RTVert4));
}

static void RT_Quad4(float w, float h, const float u[2][4], const float t[2][4], float su, float sv) {
	RT_Quad4Y(w, 0.0f, h, u, t, g_RTLinNonPow2, su, sv);
}

/* stages 0..3 with their own texture coordinates, filter, clamp; with setTex the texture of each */
static void RT_Stages4(uint32_t filter, byte setTex, void *tex) {
	for (uint32_t i = 0; i < 4; i++) {
		RT_TSS(i, D3DTSS_TEXCOORDINDEX, i);
		RT_TSS(i, D3DTSS_MINFILTER, filter);
		RT_TSS(i, D3DTSS_MAGFILTER, filter);
		RT_TSS(i, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
		RT_TSS(i, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
		if (setTex) {
			RT_SetTexture(i, tex);
		}
	}
}

/* the back buffer and the depth buffer of the device kept in g_RTBackBuffer, g_RTZBuffer */
static void RT_GetBackBuffer(void) {
	void *dev = RT_Dev();
	D3D_METHOD(dev, rtGetSurface_t, D3DDEV_GETRENDERTARGET)(dev, &g_RTBackBuffer);
	dev = RT_Dev();
	D3D_METHOD(dev, rtGetSurface_t, D3DDEV_GETDEPTHSTENCILSURFACE)(dev, &g_RTZBuffer);
}

/* the back buffer on again, released */
static void RT_RestoreBackBuffer(void) {
	RT_SetRenderTarget(g_RTBackBuffer, g_RTZBuffer);
	RT_Release(&g_RTBackBuffer);
	RT_Release(&g_RTZBuffer);
}

/* the viewports kept, the one of the target (surface 0 of tex, z) as both */
static void RT_BeginTarget(void *tex, void *z) {
	void *surf;

	Render_ClearForced();
	surf = NULL;
	RT_GetSurfaceLevel(tex, &surf);
	RT_GetBackBuffer();
	RT_SetRenderTarget(surf, z);
	COM_RELEASE(surf);
	memcpy(g_RTSavedViewport, g_Viewport, 6 * sizeof(uint32_t));
	memcpy(g_RTSavedViewportFull, g_ViewportFull, 6 * sizeof(uint32_t));
	void *dev = RT_Dev();
	D3D_METHOD(dev, d3dGetViewport_t, D3DDEV_GETVIEWPORT)(dev, g_ViewportFull);
	memcpy(g_Viewport, g_ViewportFull, 6 * sizeof(uint32_t));
}

static void RT_RestoreViewports(void) {
	memcpy(g_Viewport, g_RTSavedViewport, 6 * sizeof(uint32_t));
	memcpy(g_ViewportFull, g_RTSavedViewportFull, 6 * sizeof(uint32_t));
}

/*
 * The materials of the effects: map 0 (and 3 for the DuDv target) without mipmaps and wrap, the blend and the
 * attrib of each one (MAT_SHADER with a shader in the name), z test 8 (D3DCMP_ALWAYS), z write off.
 */
static CMaterial *RT_MakeMat(const CMaterial *cur, const char *name, const char *map, uint16_t attrib, byte transp,
	int srcBlend, int dstBlend, byte bump, byte share) {
	CMaterial m;
	CMaterial *r;

	CMaterial_ctor(&m);
	String_Set(&m.Name, name);
	CMaterial_SetMap(&m, MAP_DIFFUSE, map);
	if (bump) {
		CMaterial_SetMap(&m, MAP_BUMP, map);
	}
	m.params.attrib = attrib;
	m.params.lighting = 0;
	m.params.transp = transp;
	m.params.zWrite = 0;
	m.params.zFunc = 8;
	m.params.srcBlend = srcBlend;
	m.params.dstBlend = dstBlend;
	m.map[MAP_DIFFUSE]->tile = 0;
	m.map[MAP_DIFFUSE]->filter = 0;
	if (bump) {
		m.map[MAP_BUMP]->tile = 0;
		m.map[MAP_BUMP]->filter = 0;
	}
	CHECK(!cur);
	r = Mat_New(&m, share);
	CMaterial_dtor(&m);
	return r;
}

#define RT_WHITE "bmp/rgb_white.tga"
#define RT_BLACK "bmp/rgb_black.tga"
#define RT_ATTRIB (MAT_SHADER | MAT_LOADTEX)

void RenderTarget_MakeCreditsMatBlur(void) {
	g_CreditsMatBlur = RT_MakeMat(g_CreditsMatBlur, "Material_CreditsTargetBlur (+shader:MotionBlurAdd)", RT_WHITE,
		RT_ATTRIB, 0, 2, 1, 0, 0);
}

void RenderTarget_MakeCreditsMat(void) {
	g_CreditsMat = RT_MakeMat(g_CreditsMat, "Material_CreditsTarget", RT_WHITE, MAT_LOADTEX, 1, 5, 2, 0, 0);
}

void RenderTarget_MakeBloomMatBlur(void) {
	g_BloomMatBlur = RT_MakeMat(g_BloomMatBlur, "Material_BloomBlur (+shader:BloomBlur)", RT_WHITE, RT_ATTRIB, 0, 2, 1,
		0, 0);
}

void RenderTarget_MakeBloomMatFilter(void) {
	g_BloomMatFilter = RT_MakeMat(g_BloomMatFilter, "Material_BloomFilter (+shader:BloomFilter)", RT_WHITE, RT_ATTRIB,
		0, 2, 1, 0, 0);
}

void RenderTarget_MakeBloomMatShow(void) {
	g_BloomMatShow = RT_MakeMat(g_BloomMatShow, "Material_BloomShow", RT_BLACK, MAT_LOADTEX, 0, 2, 1, 0, 0);
}

void RenderTarget_MakeBloomMatTarget(void) {
	g_BloomMatTarget = RT_MakeMat(g_BloomMatTarget, "Material_BloomTarget (+shader:BloomTarget)", RT_BLACK, RT_ATTRIB,
		0, 2, 1, 0, 0);
}

void RenderTarget_MakeBlurMatCopy(void) {
	g_BlurMatCopy = RT_MakeMat(g_BlurMatCopy, "Material_BlurCopy", RT_WHITE, MAT_LOADTEX, 0, 2, 1, 0, 0);
}

void RenderTarget_MakeBlurMatTarget(void) {
	g_BlurMatTarget = RT_MakeMat(g_BlurMatTarget, "Material_BlurTarget (+shader:BlurTarget)", RT_WHITE, RT_ATTRIB, 0,
		2, 1, 0, 0);
}

void RenderTarget_MakeDuDvMatCopy(void) {
	g_SceneDuDvMatCopy = RT_MakeMat(g_SceneDuDvMatCopy, "Material_DuDvCopy", RT_WHITE, MAT_LOADTEX, 0, 2, 1, 0, 0);
}

void RenderTarget_MakeDuDvMatDebug(void) {
	g_SceneDuDvMatDebug = RT_MakeMat(g_SceneDuDvMatDebug, "Material_DuDvDebug", RT_WHITE, MAT_LOADTEX, 0, 2, 1, 0, 0);
}

void RenderTarget_MakeDuDvMatFilter(void) {
	g_SceneDuDvMatFilter = RT_MakeMat(g_SceneDuDvMatFilter, "Material_DuDvFilter (+shader:DUDVFilter)", RT_WHITE,
		RT_ATTRIB, 0, 2, 1, 0, 0);
}

void RenderTarget_MakeDuDvMatRTarget(void) {
	g_SceneDuDvMatRTarget = RT_MakeMat(g_SceneDuDvMatRTarget, "Material_DuDvTarget (+shader:DUDVTarget)", RT_WHITE,
		RT_ATTRIB, 0, 2, 1, 1, 0);
}

void RenderTarget_MakeMotionBlurMatAdd(void) {
	g_MotionBlurMatAdd = RT_MakeMat(g_MotionBlurMatAdd, "Material_BlurAdd (+shader:MotionBlurAdd)", RT_WHITE,
		RT_ATTRIB, 1, 5, 6, 0, 0);
}

void RenderTarget_MakeMotionBlurMatTarget(void) {
	g_MotionBlurMatTarget = RT_MakeMat(g_MotionBlurMatTarget, "Material_BlurTarget (+shader:MotionBlurTarget)",
		RT_WHITE, RT_ATTRIB, 0, 2, 1, 0, 0);
}

void RenderTarget_MakeRadialBlurMat(void) {
	g_RadialBlurMat = RT_MakeMat(g_RadialBlurMat, "Material_RadialBlurTarget (+shader:RadialBlurTarget)",
		"bmp/radialmask.bmp", RT_ATTRIB, 0, 2, 1, 0, 0);
}

void RenderTarget_MakeSceneMatCopy(void) {
	g_SceneMatCopy = RT_MakeMat(g_SceneMatCopy, "Material_SceneCopy", RT_WHITE, MAT_LOADTEX, 0, 2, 1, 0, 1);
}

/* ---- Credits ---- */

byte Credits_Begin(void) {
	CHECK(!g_RTBackBuffer && !g_RTZBuffer);
	if (g_CreditsRTarget && g_PSOn && !g_CreditsTarget) {
		Credits_Init();
	}
	if (!g_CreditsTarget || !g_CreditsRTarget || !g_PSOn || g_CreditsAlpha <= 0) {
		return 0;
	}
	RT_BeginTarget(g_CreditsTarget, NULL);
	Graphic2D_GetScreen(&g_CreditsSavedW, &g_CreditsSavedH);
	Graphic2D_SetScreen(g_CreditsSizeX, g_CreditsSizeY);
	if (g_CreditsClear) {
		Render_Clear(NULL, 1, 0, 1.0f, 0);
	}
	return 1;
}

void Credits_End(void) {
	if (!g_CreditsTarget || !g_CreditsRTarget || !g_PSOn || g_CreditsAlpha <= 0) {
		return;
	}
	Credits_Blur();
	Render_ClearForced();
	RT_RestoreBackBuffer();
	RT_RestoreViewports();
	Graphic2D_SetScreen(g_CreditsSavedW, g_CreditsSavedH);
	Credits_Draw();
}

void Credits_Release(void) {
	g_CreditsMat = NULL;
	g_CreditsMatBlur = NULL;
	RT_Release(&g_CreditsTarget);
}

void Credits_Init(void) {
	CHECK(!g_CreditsTarget);
	if (g_CreditsRTarget && g_PSOn && !g_CreditsTarget && !g_CreditsMat && !g_CreditsMatBlur) {
		RT_CreateTarget((uint32_t)g_CreditsSizeX, (uint32_t)g_CreditsSizeY, &g_CreditsTarget);
		RenderTarget_MakeCreditsMat();
		RenderTarget_MakeCreditsMatBlur();
	}
}

/* the four taps of a pass of the Credits trail, one texel row or column moved by the taps o, 2 o */
static void Credits_Taps(float *taps, float d, float b, float o, float o2) {
	taps[0] = d + (b - o2);
	taps[1] = d + (b - o);
	taps[2] = d + (o + b);
	taps[3] = d + (o2 + b);
}

void Credits_Blur(void) {
	uint32_t vp[6];
	float u[2][4], t[2][4];
	float c[4];

	CHECK(g_CreditsRTarget);
	CHECK(g_CreditsTarget);
	uint32_t now = g_TimeMs;
	float dt = RT_UToF(now - g_CreditsBlurTime) * 0.001f;
	if (!g_CreditsClear && World_TimeScale() * 0.04f > dt) {
		return;
	}
	g_CreditsBlurTime = now;
	Render_ClearForced();
	RT_SetTargetTexture(g_CreditsTarget);
	RT_GetViewport(vp);
	float w = RT_UToF(vp[2]);
	float h = RT_UToF(vp[3]);
	Render_SetMaterial(g_CreditsMatBlur, 0);
	RT_Stages4(2, 1, g_CreditsTarget);
	c[0] = c[1] = c[2] = c[3] = 1.0f;
	void *dev = RT_Dev();
	D3D_METHOD(dev, d3dSetPixelShaderConstant_t, D3DDEV_SETPIXELSHADERCONSTANT)(dev, 0, c, 1);
	float k = (float)g_CreditsAlpha / 255.0f * 0.05f + 0.2f;
	if (!g_CreditsPhase) {
		k = 0.25f;
	}
	c[0] = c[1] = c[2] = c[3] = k;
	dev = RT_Dev();
	D3D_METHOD(dev, d3dSetPixelShaderConstant_t, D3DDEV_SETPIXELSHADERCONSTANT)(dev, 1, c, 1);
	float sx = (float)g_CreditsSizeX;
	float sy = (float)g_CreditsSizeY;
	float du = 0.5f / sx;
	float dv = 0.5f / sy;
	/* phase 0: a horizontal and a vertical pass, twice; else two vertical passes moved by half a pixel up and down */
	float tap = g_CreditsPhase ? 1.5f : 0.75f;
	int passes = 2 - (g_CreditsPhase != 0);
	for (int pass = 0; pass < passes; pass++) {
		float yoff = 0.0f;
		int size = g_CreditsSizeX;
		if (g_CreditsPhase) {
			yoff = -0.5f;
			size = g_CreditsSizeY;
		}
		float o = tap / (float)size;
		float o2 = o + o;
		float y1;
		for (int b = 0; b < 2; b++) {
			if (!g_CreditsPhase) {
				Credits_Taps(u[b], du, (float)b, o, o2);
				t[b][0] = t[b][1] = t[b][2] = t[b][3] = dv + (float)b;
			} else {
				u[b][0] = u[b][1] = u[b][2] = u[b][3] = du + (float)b;
				Credits_Taps(t[b], dv, (float)b, o, o2);
			}
		}
		y1 = g_CreditsPhase ? yoff + h : h;
		RT_Quad4Y(w, yoff, y1, u, t, g_RTLinPow2, sx, sy);
		if (g_CreditsPhase == 1) {
			yoff = 0.5f;
		}
		o = tap / sy;
		o2 = o + o;
		for (int b = 0; b < 2; b++) {
			u[b][0] = u[b][1] = u[b][2] = u[b][3] = du + (float)b;
			Credits_Taps(t[b], dv, (float)b, o, o2);
		}
		t[0][0] = (0.0f - o2) + dv;
		t[0][3] = (o2 + 0.0f) + dv;
		RT_Quad4Y(w, yoff + 0.0f, yoff + h, u, t, g_RTLinPow2, sx, sy);
	}
}

void Credits_Draw(void) {
	uint32_t vp[6];
	RTVert v[4];

	CHECK(g_CreditsRTarget);
	CHECK(g_CreditsTarget);
	RT_GetViewport(vp);
	Render_SetMaterial(g_CreditsMat, 0);
	RT_TSS(0, D3DTSS_TEXCOORDINDEX, 0);
	RT_TSS(0, D3DTSS_MINFILTER, 2);
	RT_TSS(0, D3DTSS_MAGFILTER, 2);
	RT_TSS(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	RT_TSS(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	RT_SetTexture(0, g_CreditsTarget);
	float sx = (float)g_CreditsSizeX;
	float sy = (float)g_CreditsSizeY;
	float w = RT_UToF(vp[2]);
	float h = RT_UToF(vp[3]);
	/* the target in pixels of the 640 x 480 screen */
	float dw = w / 640.0f * sx;
	float dh = h / 480.0f * sy;
	float x = (float)g_CreditsX * w / 640.0f;
	float y = (float)g_CreditsY * h / 480.0f;
	RT_Quad(v, x + 0.0f, y + 0.0f, x + dw, y + dh, (uint32_t)g_CreditsAlpha << 24 | 0xffffff, 0.0f, 0.0f, 1.0f,
		1.0f);
	if (g_RTLinPow2) {
		RT_QuadTexels(v, sx, sy);
	}
	RT_DrawQuad(v);
}

int RenderTarget_VideoMemory(void) {
	void *tex[0x400];
	int n;

	for (n = 0; n < 0x400; n++) {
		void *dev = RT_Dev();
		if (D3D_METHOD(dev, rtCreateTexture_t, D3DDEV_CREATETEXTURE)(dev, 0x200, 0x200, 1, D3DUSAGE_RENDERTARGET,
			D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex[n]) < 0) {
			break;
		}
	}
	for (int i = n - 1; i >= 0; i--) {
		if ((int)COM_RELEASE(tex[i]) < 0) {
			break;
		}
	}
	return n << 20;
}

/* ---- NewsPanel ---- */

byte NewsPanel_Begin(void) {
	CHECK(!g_RTBackBuffer && !g_RTZBuffer);
	if (!g_NewsPanelTarget || !g_NewsPanelZ || !g_NewsPanelRTarget) {
		return 0;
	}
	RT_BeginTarget(g_NewsPanelTarget, g_NewsPanelZ);
	if (g_NewsPanelWire) {
		RT_SetRS(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
	}
	return 1;
}

void NewsPanel_End(void) {
	if (!g_NewsPanelTarget || !g_NewsPanelZ || !g_NewsPanelRTarget) {
		return;
	}
	Render_ClearForced();
	RT_RestoreBackBuffer();
	RT_RestoreViewports();
	if (g_NewsPanelWire) {
		RT_SetRS(D3DRS_FILLMODE, D3DFILL_SOLID);
	}
}

void NewsPanel_Release(void) {
	RT_Release(&g_NewsPanelTarget);
	RT_Release(&g_NewsPanelZ);
}

void *NewsPanel_GetTarget(void) {
	return g_NewsPanelTarget;
}

void NewsPanel_Init(void) {
	CHECK(!g_NewsPanelTarget);
	if (!g_NewsPanelRTarget) {
		return;
	}
	/* the formats of the display mode and of the depth buffer of the application */
	uint32_t size = (uint32_t)g_NewsPanelSize;
	uint32_t zfmt = g_Engine->depthBufferDesc.Format;
	void *dev = RT_Dev();
	D3D_METHOD(dev, rtCreateTexture_t, D3DDEV_CREATETEXTURE)(dev, size, size, 1, D3DUSAGE_RENDERTARGET,
		g_Engine->backBufferDesc.Format, D3DPOOL_DEFAULT, &g_NewsPanelTarget);
	dev = RT_Dev();
	D3D_METHOD(dev, rtCreateDepthStencil_t, D3DDEV_CREATEDEPTHSTENCILSURFACE)(dev, size, size, (int)zfmt, 0,
		&g_NewsPanelZ);
}

/* ---- the targets by name ---- */

int RenderTarget_Pow2Size(int n) {
	if (n > 0x200) {
		return 0x400;
	}
	return n > 0x100 ? 0x200 : 0x100;
}

int RenderTarget_Mode(const char *name) {
	if (!name) {
		return g_RTMode;
	}
	if (!strcmp(name, "Scene")) {
		return RTARGET_SCENE;
	}
	if (!strcmp(name, "NewsPanel")) {
		return RTARGET_NEWSPANEL;
	}
	if (!strcmp(name, "Credits")) {
		return RTARGET_CREDITS;
	}
	Con_FatalErrorf("RenderTarget not found: %s", name);
	return RTARGET_NONE;
}

byte RenderTarget_Begin(const char *name) {
	int mode = RenderTarget_Mode(name);
	CHECK(!g_RTMode);
	g_RTMode = mode;
	switch (mode) {
	case RTARGET_SCENE:
		return Scene_Begin();
	case RTARGET_NEWSPANEL:
		return NewsPanel_Begin();
	case RTARGET_CREDITS:
		return Credits_Begin();
	}
	CHECK(0);
	return 0;
}

byte RenderTarget_End(const char *name) {
	int mode = RenderTarget_Mode(name);
	CHECK(g_RTMode == mode);
	g_RTMode = RTARGET_NONE;
	switch (mode) {
	case RTARGET_SCENE:
		Scene_End();
		return 1;
	case RTARGET_NEWSPANEL:
		NewsPanel_End();
		return 1;
	case RTARGET_CREDITS:
		Credits_End();
		return 1;
	}
	CHECK(0);
	return 1;
}

void RenderTarget_ReleaseD3D(void) {
	NewsPanel_Release();
	Credits_Release();
	Scene_Release();
	DuDv_Release();
	Bloom_Release();
	MotionBlur_Release();
	RadialBlur_Release();
	Blur_Release();
}

void RenderTarget_InitD3D(void) {
	if (g_RenderOff) {
		return;
	}
	NewsPanel_Init();
	Credits_Init();
	Scene_Init();
	if (!g_SceneRTarget) {
		return;
	}
	DuDv_Init();
	Bloom_Init();
	MotionBlur_Init();
	Blur_Init();
}

/* ---- Bloom ---- */

void Bloom_Render(void) {
	Render_ClearForced();
	Bloom_Filter();
	Bloom_Blur();
	Bloom_Show();
	Render_ClearForced();
}

/* the source into the first bloom target through four taps, two horizontal and two vertical */
void Bloom_Filter(void) {
	uint32_t vp[6];
	float u[2][4], t[2][4];

	CHECK(g_SceneBloomRTarget);
	CHECK(g_BloomTarget1);
	RT_SetTargetTexture(g_BloomTarget1);
	RT_GetViewport(vp);
	float w = RT_UToF(vp[2]);
	float h = RT_UToF(vp[3]);
	Render_SetMaterial(g_BloomMatFilter, 0);
	RT_Stages4(2, 1, g_SceneSourceTexture);
	float du = 0.5f / g_SceneW;
	float dv = 0.5f / g_SceneH;
	float ou = g_SceneBloomBlurOffset / g_SceneW;
	float ov = g_SceneBloomBlurOffset / g_SceneH;
	for (int c = 0; c < 2; c++) {
		float b = (float)c;
		u[c][0] = (b - ou) + du;
		u[c][1] = (ou + b) + du;
		u[c][2] = du + b;
		u[c][3] = du + b;
		t[c][0] = dv + b;
		t[c][1] = dv + b;
		t[c][2] = (b - ov) + dv;
		t[c][3] = (ov + b) + dv;
	}
	RT_Quad4(w, h, u, t, g_SceneW, g_SceneH);
}

/* the passes of the blur between the two bloom targets: four taps horizontal into the second, four vertical back
 * into the first */
void Bloom_Blur(void) {
	uint32_t vp[6];
	float u[2][4], t[2][4];
	void *surf;

	CHECK(g_SceneBloomRTarget);
	CHECK(g_BloomTarget1);
	RT_Stages4(2, 0, NULL);
	RT_GetViewport(vp);
	float w = RT_UToF(vp[2]);
	float h = RT_UToF(vp[3]);
	Render_SetMaterial(g_BloomMatBlur, 0);
	float dv = 0.5f / g_BloomH;
	surf = NULL;
	float du = 0.5f / g_BloomW;
	if (g_SceneBloomBlurPasses <= 0) {
		return;
	}
	float dv1 = dv + 1.0f;
	float du1 = du + 1.0f;
	for (int pass = 0; pass < g_SceneBloomBlurPasses; pass++) {
		float o = g_SceneBloomBlurOffset / g_BloomW;
		RT_GetSurfaceLevel(g_BloomTarget2, &surf);
		RT_SetRenderTarget(surf, NULL);
		COM_RELEASE(surf);
		for (uint32_t i = 0; i < 4; i++) {
			RT_SetTexture(i, g_BloomTarget1);
		}
		float o2 = o + o;
		for (int c = 0; c < 2; c++) {
			float b = (float)c;
			u[c][0] = du + (b - o2);
			u[c][1] = (b - o) + du;
			u[c][2] = (o + b) + du;
			u[c][3] = du + (o2 + b);
		}
		for (int k = 0; k < 4; k++) {
			t[0][k] = dv + 0.0f;
			t[1][k] = dv1;
		}
		RT_Quad4(w, h, u, t, g_BloomW, g_BloomH);
		o = g_SceneBloomBlurOffset / g_BloomH;
		RT_GetSurfaceLevel(g_BloomTarget1, &surf);
		RT_SetRenderTarget(surf, NULL);
		COM_RELEASE(surf);
		for (uint32_t i = 0; i < 4; i++) {
			RT_SetTexture(i, g_BloomTarget2);
		}
		o2 = o + o;
		t[0][0] = dv + (0.0f - o2);
		t[0][1] = (0.0f - o) + dv;
		t[0][2] = (o + 0.0f) + dv;
		t[0][3] = dv + (o2 + 0.0f);
		t[1][0] = (1.0f - o2) + dv;
		t[1][1] = (1.0f - o) + dv;
		t[1][2] = (o + 1.0f) + dv;
		t[1][3] = o2 + 1.0f + dv;
		for (int k = 0; k < 4; k++) {
			u[0][k] = du + 0.0f;
			u[1][k] = du1;
		}
		RT_Quad4(w, h, u, t, g_BloomW, g_BloomH);
	}
}

/* the source with the first bloom target added into the target (R_SceneBloomRTarget 2: the bloom alone, dark) */
void Bloom_Show(void) {
	uint32_t vp[6];
	RTVert2 v[4];

	CHECK(g_SceneBloomRTarget);
	RT_SetRenderTarget(g_SceneTargetSurface, NULL);
	RT_GetViewport(vp);
	Render_SetMaterial(g_BloomMatTarget, 0);
	RT_SetTexture(0, g_SceneSourceTexture);
	RT_SetTexture(1, g_BloomTarget1);
	if (g_SceneBloomRTarget == 2) {
		Render_SetMaterial(g_BloomMatShow, 0);
		RT_SetTexture(0, g_BloomTarget1);
	}
	RT_TSS(0, D3DTSS_MINFILTER, 1);
	RT_TSS(0, D3DTSS_MAGFILTER, 1);
	RT_TSS(1, D3DTSS_MINFILTER, 2);
	RT_TSS(1, D3DTSS_MAGFILTER, 2);
	if (g_SceneBloomRTarget == 2) {
		RT_TSS(0, D3DTSS_MINFILTER, 2);
		RT_TSS(0, D3DTSS_MAGFILTER, 2);
	}
	RT_TSS(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	RT_TSS(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	RT_TSS(1, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	RT_TSS(1, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	RT_Quad2(v, RT_UToF(vp[2]), RT_UToF(vp[3]), g_SceneBloomRTarget == 2 ? 0x40404040 : 0xffffffff, g_SceneW,
		g_SceneH, g_BloomW, g_BloomH);
}

void Bloom_Release(void) {
	g_BloomMatShow = NULL;
	g_BloomMatFilter = NULL;
	g_BloomMatBlur = NULL;
	g_BloomMatTarget = NULL;
	RT_Release(&g_BloomTarget1);
	RT_Release(&g_BloomTarget2);
}

void Bloom_Init(void) {
	if (!g_SceneBloomRTarget) {
		return;
	}
	g_BloomW = g_SceneW * g_SceneBloomBlurSize;
	g_BloomH = g_SceneH * g_SceneBloomBlurSize;
	RT_CreateTarget((uint32_t)(int)g_BloomW, (uint32_t)(int)g_BloomH, &g_BloomTarget1);
	RT_CreateTarget((uint32_t)(int)g_BloomW, (uint32_t)(int)g_BloomH, &g_BloomTarget2);
	RenderTarget_MakeBloomMatShow();
	RenderTarget_MakeBloomMatFilter();
	RenderTarget_MakeBloomMatBlur();
	RenderTarget_MakeBloomMatTarget();
}

byte Bloom_IsOn(void) {
	return g_SceneBloomRTarget && g_BloomTarget1;
}

/* ---- Blur ---- */

void Blur_Render(void) {
	Render_ClearForced();
	Blur_Draw();
	Render_ClearForced();
}

/* the source blurred through four taps into the first bloom target, then copied to the target */
void Blur_Draw(void) {
	uint32_t vp[6];
	float u[2][4], t[2][4];
	RTVert v[4];

	CHECK(g_SceneBlurRTarget);
	RT_SetTargetTexture(g_BloomTarget1);
	RT_GetViewport(vp);
	float w = RT_UToF(vp[2]);
	float h = RT_UToF(vp[3]);
	Render_SetMaterial(g_BlurMatTarget, 0);
	RT_Stages4(2, 1, g_SceneSourceTexture);
	float ov = g_SceneBlurOffset / g_SceneH;
	float dv = 0.5f / g_SceneH;
	float du = 0.5f / g_SceneW;
	float ou = g_SceneBlurOffset / g_SceneW;
	for (int c = 0; c < 2; c++) {
		float b = (float)c;
		u[c][0] = du + (b - ou);
		u[c][1] = du + (ou + b);
		u[c][2] = du + b;
		u[c][3] = du + b;
		t[c][0] = dv + b;
		t[c][1] = dv + b;
		t[c][2] = dv + (b - ov);
		t[c][3] = dv + (ov + b);
	}
	RT_Quad4(w, h, u, t, g_SceneW, g_SceneH);
	RT_SetRenderTarget(g_SceneTargetSurface, NULL);
	RT_GetViewport(vp);
	Render_SetMaterial(g_BlurMatCopy, 0);
	RT_TSS(0, D3DTSS_TEXCOORDINDEX, 0);
	RT_TSS(0, D3DTSS_MINFILTER, 2);
	RT_TSS(0, D3DTSS_MAGFILTER, 2);
	RT_TSS(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	RT_TSS(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	RT_SetTexture(0, g_BloomTarget1);
	du = 0.5f / g_BloomW;
	dv = 0.5f / g_BloomH;
	RT_Quad(v, 0.0f, 0.0f, RT_UToF(vp[2]), RT_UToF(vp[3]), 0x40404040, du + 0.0f, dv + 0.0f, du + 1.0f, dv + 1.0f);
	if (g_RTLinNonPow2) {
		RT_QuadTexels(v, g_BloomW, g_BloomH);
	}
	RT_DrawQuad(v);
}

void Blur_Release(void) {
	g_BlurMatTarget = NULL;
	g_BlurMatCopy = NULL;
}

void Blur_Init(void) {
	if (g_SceneBlurRTarget) {
		RenderTarget_MakeBlurMatTarget();
		RenderTarget_MakeBlurMatCopy();
	}
}

byte Blur_IsOn(void) {
	return g_SceneBlurRTarget && g_BlurMatTarget && g_BlurMatCopy && g_BloomTarget1 && g_SceneBlurValue > 0.0f;
}

/* ---- DuDv ---- */

void DuDv_Render(void) {
	Render_ClearForced();
	DuDv_RenderBump();
	if (!g_SceneDuDvForced && (g_SceneDuDvFrame != g_FrameCount || (g_SceneSourceTexture != g_SceneDuDvTarget &&
		g_SceneRTargetSwap != 1))) {
		DuDv_Copy();
		g_SceneNoSwap = 1;
	} else {
		DuDv_Filter();
		if (g_SceneDuDvRTarget == 2) {
			DuDv_Debug();
		} else if (g_SceneDuDvRTarget == 1) {
			DuDv_Draw(0);
		}
	}
	if (!g_SceneRTargetSwap && g_SceneDuDvRTarget && g_SceneDuDvSavedTexture &&
		g_SceneSourceTexture == g_SceneDuDvTarget && g_SceneSourceSurface) {
		COM_RELEASE(g_SceneSourceSurface);
		g_SceneSourceTexture = g_SceneDuDvSavedTexture;
		g_SceneSourceSurface = g_SceneDuDvSavedSurface;
	}
	Render_ClearForced();
}

void DuDv_Release(void) {
	g_SceneDuDvMatRTarget = NULL;
	g_SceneDuDvMatFilter = NULL;
	g_SceneDuDvMatCopy = NULL;
	g_SceneDuDvMatDebug = NULL;
	RT_Release(&g_SceneDuDvBumpTarget);
	RT_Release(&g_SceneDuDvTarget);
}

/* the driver is the one the caps were checked with */
static byte RT_SameDriver(void) {
	if (!*Render_GetDriver()) {
		return 0;
	}
	const char *last = *g_DriverName;
	if (!*last) {
		return 0;
	}
	return !strcmp(Render_GetDriver(), last);
}

void DuDv_Init(void) {
	g_SceneDuDvFrame = 0;
	if (!g_SceneDuDvRTarget) {
		g_SceneDuDvCheckedMode = 0;
		return;
	}
	g_SceneDuDvBumpTarget = NULL;
	g_SceneDuDvTarget = NULL;
	RT_CreateTarget((uint32_t)(int)g_SceneW, (uint32_t)(int)g_SceneH, &g_SceneDuDvBumpTarget);
	if (!g_SceneRTargetSwap) {
		RT_CreateTarget((uint32_t)(int)g_SceneW, (uint32_t)(int)g_SceneH, &g_SceneDuDvTarget);
	}
	RenderTarget_MakeDuDvMatRTarget();
	RenderTarget_MakeDuDvMatFilter();
	RenderTarget_MakeDuDvMatCopy();
	RenderTarget_MakeDuDvMatDebug();
	if (g_SceneDuDvRTarget == 3 || (g_SceneDuDvCheckedMode != -1 && g_SceneDuDvCheckedMode != g_SceneDuDvRTarget) ||
		!RT_SameDriver()) {
		if (DuDv_CheckCaps()) {
			g_SceneDuDvRTarget = 1;
		} else {
			g_SceneDuDvRTarget = 0;
			DuDv_Release();
		}
		Con_Warnf("WARN: Checking DuDv Caps. Result: %d\n", g_SceneDuDvRTarget);
	}
	g_SceneDuDvCheckedMode = g_SceneDuDvRTarget;
}

byte DuDv_IsOn(void) {
	return g_SceneDuDvRTarget >= 1 && g_SceneDuDvBumpTarget;
}

void DuDv_RenderBump(void) {
	void *surf;

	CHECK(g_SceneDuDvRTarget);
	CHECK(g_SceneDuDvBumpTarget);
	surf = NULL;
	RT_GetSurfaceLevel(g_SceneDuDvBumpTarget, &surf);
	RT_SetRenderTarget(surf, g_RTZBuffer);
	COM_RELEASE(surf);
	if (!g_SceneDuDvFrame || g_SceneDuDvFrame + 1 >= g_FrameCount || g_SceneDuDvForced) {
		Render_Clear(NULL, 1, 0xff8080, 1.0f, 0);
	}
	int changes = g_MatChanges;
	uint32_t mask = g_RenderClassMask;
	int pass = g_RenderPass;
	g_RenderClassMask = 0;
	g_RenderPass = 100;
	Models_Render();
	E3D_FlushBatch();
	Trails_Render(0x10);
	int frame = g_SceneDuDvFrame;
	g_RenderClassMask = mask;
	if (g_MatChanges != changes) {
		frame = g_FrameCount;
	}
	g_RenderPass = pass;
	g_SceneDuDvFrame = frame;
	Render_ClearForced();
}

/* the source copied to the render target with the copy material (the stage 0 color op read twice and written back
 * twice, as the original does) */
static void DuDv_CopySource(void) {
	uint32_t vp[6];
	RTVert v[4];
	uint32_t op1, op2;

	Render_SetMaterial(g_SceneDuDvMatCopy, 0);
	RT_SetTexture(0, g_SceneSourceTexture);
	RT_TSS(0, D3DTSS_MINFILTER, 1);
	RT_TSS(0, D3DTSS_MAGFILTER, 1);
	RT_GetTSS(0, D3DTSS_COLOROP, &op1);
	RT_GetTSS(0, D3DTSS_COLOROP, &op2);
	RT_TSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	RT_TSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	RT_GetViewport(vp);
	RT_ViewQuad(v, vp, 0x40404040, 0.5f / g_SceneW, 0.5f / g_SceneH);
	RT_DrawQuad(v);
	RT_TSS(0, D3DTSS_COLOROP, op1);
	RT_TSS(0, D3DTSS_COLOROP, op2);
}

void DuDv_CopyToTarget(void) {
	CHECK(g_SceneDuDvRTarget);
	CHECK(g_SceneSourceTexture && g_SceneDuDvTarget);
	RT_SetTargetTexture(g_SceneDuDvTarget);
	DuDv_CopySource();
}

void DuDv_Debug(void) {
	uint32_t vp[6];
	RTVert v[4];

	CHECK(g_SceneDuDvRTarget);
	CHECK(g_SceneSourceTexture && g_SceneTargetSurface);
	RT_SetRenderTarget(g_SceneTargetSurface, NULL);
	Render_SetMaterial(g_SceneDuDvMatDebug, 0);
	RT_SetTexture(0, g_SceneDuDvBumpTarget);
	RT_TSS(0, D3DTSS_MINFILTER, (uint32_t)(g_SceneDuDvFilter != 0) + 1);
	RT_TSS(0, D3DTSS_MAGFILTER, (uint32_t)(g_SceneDuDvFilter != 0) + 1);
	RT_GetViewport(vp);
	RT_ViewQuad(v, vp, 0x404040, 0.5f / RT_UToF(vp[2]), 0.5f / RT_UToF(vp[3]));
	RT_DrawQuad(v);
}

void DuDv_Filter(void) {
	uint32_t vp[6];
	RTVert v[4];

	CHECK(g_SceneDuDvRTarget);
	CHECK(g_SceneSourceTexture && g_SceneTargetSurface);
	RT_SetTargetTexture(g_SceneDuDvBumpTarget);
	Render_SetMaterial(g_SceneDuDvMatFilter, 0);
	RT_SetTexture(0, g_SceneDuDvBumpTarget);
	RT_TSS(0, D3DTSS_MINFILTER, 1);
	RT_TSS(0, D3DTSS_MAGFILTER, 1);
	RT_GetViewport(vp);
	RT_ViewQuad(v, vp, 0xffffffff, 0.5f / g_SceneW, 0.5f / g_SceneH);
	RT_DrawQuad(v);
}

/* FVF 0x140344: XYZRHW, DIFFUSE, TEX3 with sets 1 and 2 of three coordinates */
#define DUDV_FVF 0x140344
typedef struct DuDvVert {
	float x, y, z, rhw;
	uint32_t diffuse;
	float t0[2];			/* the bump target */
	float t1[3];			/* the first row of the bump matrix (stage 1, no texture) */
	float t2[3];			/* the source, with the second row */
} DuDvVert;
SD_SIZE(DuDvVert, 0x34);

/* the source deformed by the bump target into the target; with check the scale of a 640 x 480 test */
void DuDv_Draw(byte check) {
	uint32_t vp[6];
	DuDvVert v[4];

	CHECK(g_SceneDuDvRTarget);
	CHECK(g_SceneSourceTexture && g_SceneTargetSurface);
	RT_SetRenderTarget(g_SceneTargetSurface, NULL);
	RT_GetViewport(vp);
	float vw = RT_UToF(vp[2]);
	float vh = RT_UToF(vp[3]);
	Render_SetMaterial(g_SceneDuDvMatRTarget, 0);
	RT_SetTexture(0, g_SceneDuDvBumpTarget);
	RT_SetTexture(1, NULL);
	RT_SetTexture(2, g_SceneSourceTexture);
	RT_TSS(0, D3DTSS_TEXCOORDINDEX, 0);
	RT_TSS(0, D3DTSS_MINFILTER, (uint32_t)(g_SceneDuDvFilter != 0) + 1);
	RT_TSS(0, D3DTSS_MAGFILTER, (uint32_t)(g_SceneDuDvFilter != 0) + 1);
	RT_TSS(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	RT_TSS(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	for (uint32_t i = 1; i < 3; i++) {
		RT_TSS(i, D3DTSS_TEXCOORDINDEX, i);
		RT_TSS(i, D3DTSS_MINFILTER, 1);
		RT_TSS(i, D3DTSS_MAGFILTER, 1);
		RT_TSS(i, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
		RT_TSS(i, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	}
	float w = g_SceneW;
	float h = g_SceneH;
	float du = 0.5f / w;
	float dv = 0.5f / h;
	float k0 = 1.0f;
	float k1 = 1.0f;
	if (check) {
		k0 = 640.0f / w;
		k1 = 480.0f / h;
	}
	/* the bias of the unsigned bump texels (-128.5 / 128 scaled to 1 / 256 texel) */
	float off = g_SceneDuDvScale * -1.00390625f * 0.00390625f;
	if (g_TexRepeat && g_TexNoPow2) {
		off = 0.0f;
	}
	k1 = k1 * g_SceneDuDvScale;
	k0 = k0 * g_SceneDuDvScale;
	float off0 = off + 0.0f;
	float u[2] = { du + 0.0f, du + 1.0f };
	float t[2] = { dv + 0.0f, dv + 1.0f };
	float mu[2] = { off0 + du, w / w + off + du };
	float mv[2] = { off0 + dv, h / h + off + dv };
	for (int i = 0; i < 4; i++) {
		int cx = i & 1;
		int cy = i >> 1;
		v[i].x = cx ? vw : 0.0f;
		v[i].y = cy ? vh : 0.0f;
		v[i].z = 0.0f;
		v[i].rhw = 1.0f;
		v[i].diffuse = 0xffffff;
		v[i].t0[0] = u[cx];
		v[i].t0[1] = t[cy];
		v[i].t1[0] = mu[cx];
		v[i].t1[1] = k0;
		v[i].t1[2] = 0.0f;
		v[i].t2[0] = mv[cy];
		v[i].t2[1] = 0.0f;
		v[i].t2[2] = k1;
	}
	if (g_RTLinNonPow2) {
		for (int i = 0; i < 4; i++) {
			v[i].t0[0] = v[i].t0[0] * w;
			v[i].t0[1] = v[i].t0[1] * h;
			v[i].t1[0] = v[i].t1[0] * w;
			v[i].t1[1] = v[i].t1[1] * w;
			v[i].t2[0] = v[i].t2[0] * h;
			v[i].t2[2] = v[i].t2[2] * h;
		}
	}
	if (!g_TexRepeat && g_TexNoPow2) {
		float r = h / w;
		for (int i = 0; i < 4; i++) {
			v[i].t1[0] = v[i].t1[0] * w;
			v[i].t1[1] = v[i].t1[1] * w;
			v[i].t2[0] = v[i].t2[0] * r;
			v[i].t2[2] = v[i].t2[2] * h;
		}
	}
	void *dev = RT_Dev();
	D3D_METHOD(dev, d3dSetVertexShader_t, D3DDEV_SETVERTEXSHADER)(dev, DUDV_FVF);
	dev = RT_Dev();
	D3D_METHOD(dev, d3dDrawPrimitiveUP_t, D3DDEV_DRAWPRIMITIVEUP)(dev, D3DPT_TRIANGLESTRIP, 2, v, sizeof(DuDvVert));
}

void DuDv_Copy(void) {
	CHECK(g_SceneDuDvRTarget);
	CHECK(g_SceneTargetSurface && g_SceneSourceTexture);
	RT_SetRenderTarget(g_SceneTargetSurface, NULL);
	DuDv_CopySource();
}

/* ---- Motion blur ---- */

void MotionBlur_Render(void) {
	Render_ClearForced();
	MotionBlur_Add();
	MotionBlur_Draw();
	Render_ClearForced();
}

#define g_MotionBlurTime GAME_GLOBAL(uint32_t, g_MotionBlurTime, 0x4a8eac)

/* the source added to the motion blur target through four taps, about every 1/25 s of game time; the shader
 * constants: the fade of the time passed since then, a quarter for each tap */
void MotionBlur_Add(void) {
	uint32_t vp[6];
	float u[2][4], t[2][4];
	float c0[4], c1[4];

	CHECK(g_SceneMotionBlurRTarget);
	CHECK(g_MotionBlurTarget);
	RT_SetTargetTexture(g_MotionBlurTarget);
	if (!g_MotionBlurFilled) {
		g_MotionBlurFilled = 1;
		Render_Clear(NULL, 1, 0, 1.0f, 0);
	}
	RT_GetViewport(vp);
	float w = RT_UToF(vp[2]);
	float h = RT_UToF(vp[3]);
	Render_SetMaterial(g_MotionBlurMatAdd, 0);
	RT_Stages4(2, 1, g_SceneSourceTexture);
	uint32_t now = g_TimeMs;
	float dt = RT_UToF(now - g_MotionBlurTime) * 0.001f;
	if (World_TimeScale() * 0.04f > dt) {
		return;
	}
	g_MotionBlurTime = now;
	float rest = dt - World_TimeScale() * 0.04f;
	c0[0] = 0.0f;
	c0[1] = 0.0f;
	c0[2] = 0.0f;
	c0[3] = rest + rest + 0.15f;
	void *dev = RT_Dev();
	D3D_METHOD(dev, d3dSetPixelShaderConstant_t, D3DDEV_SETPIXELSHADERCONSTANT)(dev, 0, c0, 1);
	c1[0] = c1[1] = c1[2] = c1[3] = 0.25f;
	dev = RT_Dev();
	D3D_METHOD(dev, d3dSetPixelShaderConstant_t, D3DDEV_SETPIXELSHADERCONSTANT)(dev, 1, c1, 1);
	float ov = g_SceneMotionBlurOffset / g_SceneH;
	float ou = g_SceneMotionBlurOffset / g_SceneW;
	float dv = 0.5f / g_SceneH;
	float du = 0.5f / g_SceneW;
	float off = g_SceneMotionBlurOff;
	/* the left and top side moved by +off, the right and bottom one by -off */
	u[0][0] = du + ((0.0f - ou) + off);
	u[0][1] = du + ((ou + 0.0f) + off);
	u[0][2] = du + (off + 0.0f);
	u[0][3] = u[0][2];
	u[1][0] = du + (1.0f - ou - off);
	u[1][1] = du + (ou + 1.0f - off);
	u[1][2] = du + (1.0f - off);
	u[1][3] = u[1][2];
	t[0][0] = dv + (off + 0.0f);
	t[0][1] = t[0][0];
	t[0][2] = dv + ((0.0f - ov) + off);
	t[0][3] = dv + ((ov + 0.0f) + off);
	t[1][0] = dv + (1.0f - off);
	t[1][1] = t[1][0];
	t[1][2] = dv + (1.0f - ov - off);
	t[1][3] = dv + (ov + 1.0f - off);
	RT_Quad4(w, h, u, t, g_SceneW, g_SceneH);
}

/* the source and the motion blur target mixed into the target by R_SceneMotionBlurValue (0..1) */
void MotionBlur_Draw(void) {
	uint32_t vp[6];
	RTVert2 v[4];
	float c0[4], c1[4];

	CHECK(g_SceneMotionBlurRTarget);
	CHECK(g_SceneTargetSurface);
	RT_SetRenderTarget(g_SceneTargetSurface, NULL);
	RT_GetViewport(vp);
	Render_SetMaterial(g_MotionBlurMatTarget, 0);
	RT_SetTexture(0, g_SceneSourceTexture);
	RT_SetTexture(1, g_MotionBlurTarget);
	float k;
	if (g_SceneMotionBlurValue > 0.0f) {
		k = 1.0f;
		if (g_SceneMotionBlurValue <= 1.0f) {
			k = g_SceneMotionBlurValue;
		}
	} else {
		k = 0.0f;
	}
	c0[0] = c0[1] = c0[2] = c0[3] = k;
	void *dev = RT_Dev();
	D3D_METHOD(dev, d3dSetPixelShaderConstant_t, D3DDEV_SETPIXELSHADERCONSTANT)(dev, 0, c0, 1);
	float r = 1.0f - k;
	c1[0] = c1[1] = c1[2] = c1[3] = r;
	dev = RT_Dev();
	D3D_METHOD(dev, d3dSetPixelShaderConstant_t, D3DDEV_SETPIXELSHADERCONSTANT)(dev, 1, c1, 1);
	RT_TSS(0, D3DTSS_MINFILTER, 1);
	RT_TSS(0, D3DTSS_MAGFILTER, 1);
	RT_TSS(1, D3DTSS_MINFILTER, 2);
	RT_TSS(1, D3DTSS_MAGFILTER, 2);
	RT_Quad2(v, RT_UToF(vp[2]), RT_UToF(vp[3]), 0xffffffff, g_SceneW, g_SceneH, g_MotionBlurW, g_MotionBlurH);
}

void MotionBlur_Release(void) {
	g_MotionBlurMatTarget = NULL;
	g_MotionBlurMatAdd = NULL;
	RT_Release(&g_MotionBlurTarget);
}

void MotionBlur_Init(void) {
	uint32_t vp[6];

	if (!g_SceneMotionBlurRTarget || g_SplitScreen) {
		return;
	}
	g_MotionBlurW = g_SceneW * g_SceneMotionBlurSize;
	g_MotionBlurH = g_SceneH * g_SceneMotionBlurSize;
	void *dev = RT_Dev();
	D3D_METHOD(dev, d3dGetViewport_t, D3DDEV_GETVIEWPORT)(dev, vp);
	RT_CreateTarget((uint32_t)(int)g_MotionBlurW, (uint32_t)(int)g_MotionBlurH, &g_MotionBlurTarget);
	g_MotionBlurFilled = 0;
	g_SceneMotionBlurValue = 0.0f;
	RenderTarget_MakeMotionBlurMatTarget();
	RenderTarget_MakeMotionBlurMatAdd();
}

byte MotionBlur_IsOn(void) {
	return g_SceneMotionBlurRTarget && g_MotionBlurTarget && g_SceneMotionBlurValue > 0.0f;
}

/* ---- Radial blur ---- */

void RadialBlur_Render(void) {
	Render_ClearForced();
	RadialBlur_Draw();
	Render_ClearForced();
}

/* the source three times, the copies moved by R_SceneRadialBlurValue * R_SceneRadialBlurOffset to the middle, through
 * the radial mask into the target */
void RadialBlur_Draw(void) {
	uint32_t vp[6];
	RTVert4 v[4];

	CHECK(g_SceneRadialBlurRTarget);
	RT_SetRenderTarget(g_SceneTargetSurface, NULL);
	RT_GetViewport(vp);
	Render_SetMaterial(g_RadialBlurMat, 0);
	for (uint32_t i = 0; i < 4; i++) {
		RT_TSS(i, D3DTSS_TEXCOORDINDEX, i);
		RT_TSS(i, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
		RT_TSS(i, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
		uint32_t filter = i < 2 ? 1 : 2;
		RT_TSS(i, D3DTSS_MINFILTER, filter);
		RT_TSS(i, D3DTSS_MAGFILTER, filter);
		RT_SetTexture(i, g_SceneSourceTexture);
	}
	CMap *mask = g_RadialBlurMat->map[MAP_DIFFUSE];
	RT_SetTexture(0, mask ? CMap_GetTexture(mask, 0) : NULL);
	RT_TSS(0, D3DTSS_MINFILTER, 2);
	RT_TSS(0, D3DTSS_MAGFILTER, 2);
	float du = 0.5f / g_SceneW;
	float dv = 0.5f / g_SceneH;
	float ov = g_SceneRadialBlurValue * g_SceneRadialBlurOffset / g_SceneH;
	float ou = g_SceneRadialBlurValue * g_SceneRadialBlurOffset / g_SceneW;
	float u[2][3] = { { du + 0.0f, ou + 0.0f + du, ou * 2.0f + 0.0f + du },
		{ du + 1.0f, (1.0f - ou) + du, (1.0f - ou * 2.0f) + du } };
	float t[2][3] = { { dv + 0.0f, ov + 0.0f + dv, ov * 2.0f + 0.0f + dv },
		{ dv + 1.0f, (1.0f - ov) + dv, (1.0f - ov * 2.0f) + dv } };
	for (int i = 0; i < 4; i++) {
		int cx = i & 1;
		int cy = i >> 1;
		v[i].x = cx ? RT_UToF(vp[2]) : 0.0f;
		v[i].y = cy ? RT_UToF(vp[3]) : 0.0f;
		v[i].z = 0.0f;
		v[i].rhw = 1.0f;
		v[i].diffuse = 0xffffffff;
		v[i].t[0][0] = (float)cx;
		v[i].t[0][1] = (float)cy;
		for (int k = 0; k < 3; k++) {
			v[i].t[1 + k][0] = u[cx][k];
			v[i].t[1 + k][1] = t[cy][k];
		}
	}
	if (g_RTLinNonPow2) {
		for (int i = 0; i < 4; i++) {
			for (int k = 1; k < 4; k++) {
				v[i].t[k][0] = v[i].t[k][0] * g_SceneW;
				v[i].t[k][1] = v[i].t[k][1] * g_SceneH;
			}
		}
	}
	void *dev = RT_Dev();
	D3D_METHOD(dev, d3dSetVertexShader_t, D3DDEV_SETVERTEXSHADER)(dev, RT_FVF4);
	dev = RT_Dev();
	D3D_METHOD(dev, d3dDrawPrimitiveUP_t, D3DDEV_DRAWPRIMITIVEUP)(dev, D3DPT_TRIANGLESTRIP, 2, v, sizeof(RTVert4));
}

void RadialBlur_Release(void) {
	g_RadialBlurMat = NULL;
}

byte RadialBlur_IsOn(void) {
	return g_SceneRadialBlurRTarget && g_SceneMotionBlurRTarget && g_RadialBlurMat && g_SceneRadialBlurValue > 0.0f;
}

/* ---- Scene ---- */

byte Scene_Begin(void) {
	CHECK(!g_RTBackBuffer && !g_RTZBuffer);
	if (!g_SceneOn || !g_SceneRTarget || !g_PSOn) {
		return 0;
	}
	g_SceneFxLast = -1;
	for (int i = 0; i < RTFX_TOTAL; i++) {
		byte on = g_RTFxOn[i]();
		g_SceneFxOn[i] = on;
		if (on) {
			g_SceneFxLast = i;
		}
	}
	Render_ClearForced();
	if (g_SceneFxLast <= -1) {
		return 0;
	}
	if (g_SplitScreen && g_SceneRT1 && g_SceneRT2) {
		g_ViewportFull[3] = (uint32_t)(int)g_SceneH;
	}
	RT_GetBackBuffer();
	/* the scene is drawn into the source; the halves of the split screen into their own textures */
	void *source = g_SceneRT1;
	if (g_SplitScreen) {
		if (!g_SplitScreenUpdate) {
			source = g_SceneRT3;
		} else if (g_SplitScreenCamera == 1) {
			source = g_SceneRT2;
		}
	}
	g_SceneSourceTexture = source;
	RT_GetSurfaceLevel(source, &g_SceneSourceSurface);
	void *target = g_SceneRTargetSwap ? g_SceneRT2 : g_SceneSourceTexture;
	g_SceneTargetTexture = target;
	RT_GetSurfaceLevel(target, &g_SceneTargetSurface);
	if (!g_SceneRTargetSwap && g_SceneDuDvRTarget && g_SceneDuDvTarget) {
		void *dudv = g_SceneDuDvTarget;
		g_SceneDuDvSavedTexture = g_SceneTargetTexture;
		g_SceneDuDvSavedSurface = g_SceneTargetSurface;
		g_SceneTargetTexture = g_SceneSourceTexture;
		g_SceneTargetSurface = g_SceneSourceSurface;
		g_SceneSourceTexture = dudv;
		RT_GetSurfaceLevel(dudv, &g_SceneSourceSurface);
	} else {
		g_SceneDuDvSavedTexture = NULL;
		g_SceneDuDvSavedSurface = NULL;
	}
	RT_SetRenderTarget(g_SceneSourceSurface, g_RTZBuffer);
	return 1;
}

void Scene_End(void) {
	uint32_t fill;

	if (g_SceneOn && g_SceneRTarget && g_PSOn) {
		if (g_SceneFxLast <= -1) {
			return;
		}
		void *dev = RT_Dev();
		D3D_METHOD(dev, rtGetRenderState_t, D3DDEV_GETRENDERSTATE)(dev, D3DRS_FILLMODE, &fill);
		RT_SetRS(D3DRS_FILLMODE, D3DFILL_SOLID);
		for (int i = 0; i < RTFX_TOTAL; i++) {
			if (!g_SceneFxOn[i]) {
				continue;
			}
			if (i != g_SceneFxLast) {
				g_RTFxRender[i]();
				Scene_Swap();
				continue;
			}
			if (g_SplitScreen && g_SceneRT1 && g_SceneRT2) {
				g_RTFxRender[i]();
				Scene_Swap();
				memcpy(g_ViewportFull, g_SceneViewport, 6 * sizeof(uint32_t));
				memcpy(g_Viewport, g_SceneViewport, 6 * sizeof(uint32_t));
				dev = RT_Dev();
				D3D_METHOD(dev, d3dSetViewport_t, D3DDEV_SETVIEWPORT)(dev, g_Viewport);
				Scene_DrawSplit();
			} else {
				Scene_TargetBackBuffer();
				g_RTFxRender[i]();
			}
			break;
		}
		RT_SetRenderTarget(g_RTBackBuffer, g_RTZBuffer);
		if (g_SceneSourceSurface == g_RTBackBuffer) {
			g_SceneSourceSurface = NULL;
		}
		RT_Release(&g_RTBackBuffer);
		RT_Release(&g_RTZBuffer);
		Render_ResetState();
		RT_SetRS(D3DRS_FILLMODE, fill);
	}
	if (g_SceneSourceSurface != g_RTBackBuffer && g_SceneSourceSurface) {
		COM_RELEASE(g_SceneSourceSurface);
		g_SceneSourceSurface = NULL;
	}
}

void Scene_Release(void) {
	g_SceneMatCopy = NULL;
	RT_Release(&g_SceneRT1);
	RT_Release(&g_SceneRT2);
	RT_Release(&g_SceneRT3);
}

void Scene_Init(void) {
	uint32_t vp[6];
	byte ok;

	g_SceneOn = 0;
	if (!g_SceneRTarget) {
		g_SceneCheckedMode = 0;
		return;
	}
	CHECK(!g_SceneRT1);
	g_RTLinNonPow2 = 0;
	g_RTLinPow2 = 0;
	if (!g_TexNoPow2 && g_TexPow2) {
		g_RTLinNonPow2 = 1;
		g_RTLinPow2 = 1;
	}
	if (g_SceneForceLinPow2 != -1) {
		g_RTLinPow2 = g_SceneForceLinPow2 != 0;
	}
	if (g_SceneForceLinNonPow2 != -1) {
		g_RTLinNonPow2 = g_SceneForceLinNonPow2 != 0;
	}
	g_SceneNoSwap = 0;
	void *dev = RT_Dev();
	D3D_METHOD(dev, d3dGetViewport_t, D3DDEV_GETVIEWPORT)(dev, vp);
	g_SceneW = RT_UToF(vp[2]);
	g_SceneH = RT_UToF(vp[3]);
	if (g_SplitScreen && !g_SceneRTargetSwap) {
		g_SceneH = g_SceneH * 0.5f;
		memcpy(g_SceneViewport, g_ViewportFull, 6 * sizeof(uint32_t));
	}
	ok = 1;
	CHECK(!g_SplitScreen || !g_SceneRTargetSwap);
	if (RT_CreateTarget((uint32_t)(int)g_SceneW, (uint32_t)(int)g_SceneH, &g_SceneRT1) < 0) {
		ok = 0;
	}
	if ((g_SceneRTargetSwap || g_SplitScreen) &&
		RT_CreateTarget((uint32_t)(int)g_SceneW, (uint32_t)(int)g_SceneH, &g_SceneRT2) < 0) {
		ok = 0;
	}
	g_SceneRT3 = NULL;
	if (g_SplitScreen && RT_CreateTarget((uint32_t)(int)g_SceneW, (uint32_t)(int)g_SceneH, &g_SceneRT3) < 0) {
		ok = 0;
	}
	RenderTarget_MakeSceneMatCopy();
	if (!ok) {
		g_SceneRTarget = 0;
		g_SceneCheckedMode = 0;
		return;
	}
	if (g_SceneRTarget > 0) {
		if (g_SceneRTarget == 3 || (g_SceneCheckedMode != -1 && g_SceneCheckedMode != g_SceneRTarget) ||
			!RT_SameDriver()) {
			if (Scene_CheckCaps()) {
				g_SceneRTarget = 1;
			} else {
				ok = 0;
				g_SceneRTarget = 0;
				g_SceneBloomRTarget = 0;
				g_SceneMotionBlurRTarget = 0;
				g_SceneDuDvRTarget = 0;
				Scene_Release();
			}
			Con_Warnf("WARN: Checking RenderTarget Caps. Result: %d\n", g_SceneRTarget);
		}
	}
	if (ok) {
		g_SceneOn = 1;
	}
	g_SceneCheckedMode = g_SceneRTarget;
}

void Scene_TargetBackBuffer(void) {
	if (g_SceneTargetSurface != g_RTBackBuffer && g_SceneTargetSurface) {
		COM_RELEASE(g_SceneTargetSurface);
	}
	g_SceneTargetSurface = g_RTBackBuffer;
	g_SceneTargetTexture = NULL;
}

void Scene_DrawSplit(void) {
	uint32_t vp[6];
	RTVert v[4];

	CHECK(g_SceneRTarget);
	CHECK(g_SceneRT1 && g_SceneRT2);
	Scene_TargetBackBuffer();
	if (g_SplitScreenUpdate) {
		g_SplitScreenCamera ^= 1;
	}
	RT_SetRenderTarget(g_RTBackBuffer, NULL);
	RT_GetViewport(vp);
	for (int i = 0; i < 2; i++) {
		float x = RT_UToF(vp[0]);
		float y = RT_UToF(vp[1]);
		float w = RT_UToF(vp[2]);
		float h = RT_UToF(vp[3]) * 0.5f;
		if (i > 0) {
			y = y + h;
		}
		Render_SetMaterial(g_SceneMatCopy, 0);
		RT_SetTexture(0, i == 1 ? g_SceneRT2 : g_SceneRT1);
		RT_TSS(0, D3DTSS_MINFILTER, 1);
		RT_TSS(0, D3DTSS_MAGFILTER, 1);
		/* the texel centers */
		float du = 0.5f / g_SceneW;
		float dv = 0.5f / g_SceneH;
		RT_Quad(v, x, y, w + x, h + y, 0x40404040, du + 0.0f, dv + 0.0f, du + 1.0f, dv + 1.0f);
		if (g_RTLinNonPow2) {
			RT_QuadTexels(v, g_SceneW, g_SceneH);
		}
		RT_DrawQuad(v);
	}
}

void Scene_Swap(void) {
	if (g_SceneRTargetSwap) {
		byte skip = g_SceneNoSwap;
		g_SceneNoSwap = 0;
		if (!skip) {
			void *tex = g_SceneSourceTexture;
			void *surf = g_SceneSourceSurface;
			g_SceneSourceTexture = g_SceneTargetTexture;
			g_SceneTargetTexture = tex;
			g_SceneSourceSurface = g_SceneTargetSurface;
			g_SceneTargetSurface = surf;
		}
		return;
	}
	g_SceneNoSwap = 0;
}

void RenderTarget_DrawTexQuad(const box_t *quad, uint32_t color, void *tex) {
	int r[4];
	RTVert v[4];

	CHECK(quad);
	CHECK(tex);
	RT_SetTexture(0, tex);
	RT_TSS(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	RT_TSS(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	RT_TSS(0, D3DTSS_MINFILTER, 1);
	RT_TSS(0, D3DTSS_MAGFILTER, 1);
	Render_ClipRect(r, g_ViewportFull, quad);
	/* the pixel centers */
	RT_Quad(v, (float)r[0] - 0.5f, (float)r[1] - 0.5f, (float)r[2] - 0.5f, (float)r[3] - 0.5f, color, 0.0f, 0.0f, 1.0f,
		1.0f);
	RT_DrawQuad(v);
	RT_SetTexture(0, NULL);
}

/*
 * The caps checks: a test texture drawn into the scene textures (with the deformation for the DuDv one), copied to
 * a 256 x 256 image surface and compared with the expected picture, first at eight points (exactly), then every pixel
 * (each channel within 32); more different pixels than the engine variable fail the check.
 */

/* a texture of a DDS file of the packs (*tex, NULL when it fails); whether it was made */
static byte RT_CapsLoad(const char *name, void **tex) {
	void *data;
	uint32_t size;

	FileIO_ReadAll(name, &data, &size);
	byte ok = D3DX_CreateTextureFromFileInMemoryEx(g_Engine->device, data, size, 0, 0, 1, 0, D3DFMT_A8R8G8B8,
		D3DPOOL_MANAGED, 1, 1, 0, NULL, NULL, tex) >= 0;
	Ptr_Delete(&data);
	return ok;
}

/* the test texture modulated by white without blending */
static void RT_CapsStates(void) {
	RT_SetRS(D3DRS_COLORWRITEENABLE, 0xf);
	RT_TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	RT_TSS(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	RT_TSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	RT_TSS(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	RT_TSS(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	RT_TSS(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	RT_SetRS(D3DRS_SRCBLEND, 2);
	RT_SetRS(D3DRS_DESTBLEND, 2);
	RT_TSS(0, D3DTSS_MINFILTER, 1);
	RT_TSS(0, D3DTSS_MAGFILTER, 1);
}

/* the source and target of the scene on the scene textures, the back buffer kept */
static void RT_CapsTargets(void) {
	g_SceneSourceTexture = g_SceneRT1;
	RT_GetSurfaceLevel(g_SceneRT1, &g_SceneSourceSurface);
	g_SceneTargetTexture = g_SceneRTargetSwap ? g_SceneRT2 : g_SceneRT1;
	RT_GetSurfaceLevel(g_SceneTargetTexture, &g_SceneTargetSurface);
	RT_GetBackBuffer();
}

static void RT_Scene(uint32_t off) {
	void *dev = RT_Dev();
	D3D_METHOD(dev, rtScene_t, off)(dev);
}

/* the scene target copied to a new 256 x 256 image surface *img; 0 when that fails */
static byte RT_CapsCopy(void **img) {
	int rect[4];

	void *dev = RT_Dev();
	if (D3D_METHOD(dev, rtCreateImageSurface_t, D3DDEV_CREATEIMAGESURFACE)(dev, 0x100, 0x100, D3DFMT_A8R8G8B8,
		img) < 0) {
		return 0;
	}
	rect[0] = 0;
	rect[1] = 0;
	rect[2] = (int)g_SceneW;
	if (rect[2] > 0x100) {
		rect[2] = 0x100;
	}
	rect[3] = (int)g_SceneH;
	if (rect[3] > 0x100) {
		rect[3] = 0x100;
	}
	dev = RT_Dev();
	return D3D_METHOD(dev, rtCopyRects_t, D3DDEV_COPYRECTS)(dev, g_SceneTargetSurface, rect, 1, *img, NULL) >= 0;
}

/* the scene surfaces released, the back buffer on again */
static void RT_CapsRelease(void) {
	RT_Release(&g_SceneSourceSurface);
	g_SceneSourceTexture = NULL;
	RT_Release(&g_SceneTargetSurface);
	g_SceneTargetTexture = NULL;
	RT_SetRenderTarget(g_RTBackBuffer, g_RTZBuffer);
	RT_Release(&g_RTBackBuffer);
	if (g_RTZBuffer) {
		COM_RELEASE(g_RTZBuffer);
	}
	g_RTBackBuffer = NULL;
	g_RTZBuffer = NULL;
}

/* surface 0 of want against the copy img (saved as a screenshot with shot); ok when it was made and both match */
static byte RT_CapsCompare(void *want, void *img, byte ok, byte shot, byte dudv, int maxPixels) {
	static const byte points[8][2] = { { 0, 0 }, { 255, 0 }, { 0, 255 }, { 255, 255 },
		{ 4, 4 }, { 251, 4 }, { 4, 251 }, { 251, 251 } };
	uint32_t da[8], db[8];
	RTLockedRect a, b;
	void *surf = NULL;

	if (D3D_METHOD(want, rtGetSurfaceLevel_t, D3DTEX_GETSURFACELEVEL)(want, 0, &surf) < 0) {
		ok = 0;
		goto done;
	}
	if (!ok) {
		goto done;
	}
	if (shot) {
		Surface_SaveShot(img);
	}
	if (D3D_METHOD(surf, rtGetDesc_t, D3DSURF_GETDESC)(surf, da) < 0) {
		ok = 0;
	}
	if (D3D_METHOD(surf, rtLockRect_t, D3DSURF_LOCKRECT)(surf, &a, NULL, D3DLOCK_READONLY) < 0) {
		ok = 0;
	}
	if (D3D_METHOD(img, rtGetDesc_t, D3DSURF_GETDESC)(img, db) < 0) {
		ok = 0;
	}
	if (D3D_METHOD(img, rtLockRect_t, D3DSURF_LOCKRECT)(img, &b, NULL, D3DLOCK_READONLY) < 0) {
		ok = 0;
	}
	/* the surfaces stay locked, the original never unlocks them before the release */
	if (da[0] != db[0] || da[0] != D3DFMT_A8R8G8B8) {
		ok = 0;
		goto done;
	}
	if (!ok) {
		goto done;
	}
	for (int k = 0; k < 8 && ok; k += 4) {
		for (int i = k; i < k + 4; i++) {
			if (!RenderTarget_PixelEqual(a.Pitch, a.pBits, b.Pitch, b.pBits, points[i][0], 4, points[i][1], 0)) {
				ok = 0;
			}
		}
	}
	if (!ok) {
		Con_Warnf(dudv ? "WARN: Checking DuDv Caps. Different Square Pixels\n" :
			"WARN: Checking RenderTarget Caps. Different Square Pixels\n");
		goto done;
	}
	int count = 0;
	for (int y = 0; y < (int)db[7]; y++) {
		for (int x = 0; x < (int)db[6]; x++) {
			if (!RenderTarget_PixelEqual(a.Pitch, a.pBits, b.Pitch, b.pBits, x, 4, y, 0x20)) {
				count++;
			}
		}
	}
	if (count > maxPixels) {
		ok = 0;
	}
	Con_Warnf(dudv ? "WARN: Checking DuDv Caps. PixelCount: %d\n" : "WARN: Checking RenderTarget Caps. PixelCount: %d\n",
		count);
done:
	if (surf) {
		COM_RELEASE(surf);
	}
	return ok;
}

/* the states of the end of a check, mat the material set */
static void RT_CapsEnd(CMaterial *mat) {
	RT_SetRS(D3DRS_SRCBLEND, 2);
	RT_SetRS(D3DRS_DESTBLEND, 5);
	RT_TSS(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	RT_TSS(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	RT_TSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	Render_ResetState();
	Render_SetMaterial(mat, 0);
	Render_ClearForced();
}

byte DuDv_CheckCaps(void) {
	void *test = NULL, *bump = NULL, *result = NULL, *img = NULL;
	void *surf;
	byte ok;

	CHECK(g_SceneDuDvRTarget);
	CHECK(g_SceneDuDvBumpTarget);
	ok = RT_CapsLoad("bmp/DuDvTest.dds", &test);
	if (ok) {
		ok = RT_CapsLoad("bmp/DuDvTestBump.dds", &bump);
	}
	if (ok) {
		ok = RT_CapsLoad("bmp/DuDvTestResult.dds", &result);
	}
	box_t quad = { { 0.0f, 0.0f, 0.0f }, { 256.0f / g_SceneW, 256.0f / g_SceneH, 0.0f } };
	if (ok) {
		RT_CapsStates();
	}
	RT_CapsTargets();
	if (ok) {
		RT_Scene(D3DDEV_BEGINSCENE);
		RT_SetRenderTarget(g_SceneSourceSurface, NULL);
		Render_Clear(NULL, 1, 0, 1.0f, 0);
		RenderTarget_DrawTexQuad(&quad, 0xffffffff, test);
		surf = NULL;
		RT_GetSurfaceLevel(g_SceneDuDvBumpTarget, &surf);
		RT_SetRenderTarget(surf, NULL);
		COM_RELEASE(surf);
		Render_Clear(NULL, 1, 0xff8080, 1.0f, 0);
		RenderTarget_DrawTexQuad(&quad, 0xffffffff, bump);
		DuDv_Filter();
		if (!g_SceneRTargetSwap) {
			DuDv_CopyToTarget();
		}
		DuDv_Draw(1);
		RT_Scene(D3DDEV_ENDSCENE);
	}
	RT_TSS(0, D3DTSS_MINFILTER, 1);
	RT_TSS(0, D3DTSS_MAGFILTER, 1);
	if (ok) {
		ok = RT_CapsCopy(&img);
	}
	RT_CapsRelease();
	ok = RT_CapsCompare(result, img, ok, g_SceneDuDvRTarget == 3, 1, g_SceneDuDvCheckPixels);
	RT_Release(&img);
	RT_Release(&test);
	RT_Release(&bump);
	RT_Release(&result);
	RT_CapsEnd(g_SceneDuDvMatCopy);
	return ok;
}

byte Scene_CheckCaps(void) {
	void *test = NULL, *img = NULL;
	uint32_t vp[6];
	RTVert v[4];
	byte ok;

	CHECK(g_SceneRTarget);
	ok = RT_CapsLoad("bmp/DuDvTest.dds", &test);
	box_t quad = { { 0.0f, 0.0f, 0.0f }, { 256.0f / g_SceneW, 256.0f / g_SceneH, 0.0f } };
	if (ok) {
		RT_CapsStates();
	}
	RT_CapsTargets();
	if (ok) {
		RT_Scene(D3DDEV_BEGINSCENE);
		RT_SetRenderTarget(g_SceneSourceSurface, NULL);
		Render_Clear(NULL, 1, 0, 1.0f, 0);
		RenderTarget_DrawTexQuad(&quad, 0xffffffff, test);
		RT_SetRenderTarget(g_SceneTargetSurface, NULL);
		RT_GetViewport(vp);
		Render_SetMaterial(g_SceneMatCopy, 0);
		RT_TSS(0, D3DTSS_TEXCOORDINDEX, 0);
		RT_TSS(0, D3DTSS_MINFILTER, 1);
		RT_TSS(0, D3DTSS_MAGFILTER, 1);
		RT_TSS(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
		RT_TSS(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
		RT_SetTexture(0, g_SceneSourceTexture);
		RT_Quad(v, 0.0f, 0.0f, RT_UToF(vp[2]) + 0.0f, RT_UToF(vp[3]) + 0.0f, 0xffffffff, 0.5f / g_SceneW + 0.0f,
			0.5f / g_SceneH + 0.0f, 0.5f / g_SceneW + 1.0f, 0.5f / g_SceneH + 1.0f);
		if (g_RTLinNonPow2) {
			RT_QuadTexels(v, g_SceneW, g_SceneH);
		}
		RT_DrawQuad(v);
		RT_Scene(D3DDEV_ENDSCENE);
	}
	RT_TSS(0, D3DTSS_MINFILTER, 1);
	RT_TSS(0, D3DTSS_MAGFILTER, 1);
	if (ok) {
		ok = RT_CapsCopy(&img);
	}
	RT_CapsRelease();
	ok = RT_CapsCompare(test, img, ok, g_SceneRTarget == 3, 0, g_SceneCheckPixels);
	RT_Release(&img);
	RT_Release(&test);
	RT_CapsEnd(g_SceneMatCopy);
	return ok;
}

byte RenderTarget_PixelEqual(int pitchA, const byte *bitsA, int pitchB, const byte *bitsB, int x, int bpp, int y,
	int tol) {
	const byte *a = bitsA + pitchA * y + x * bpp;
	const byte *b = bitsB + pitchB * y + x * bpp;

	if (tol < 1) {
		return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
	}
	for (int i = 0; i < 3; i++) {
		int d = a[i] - b[i];
		if ((d < 0 ? -d : d) > tol) {
			return 0;
		}
	}
	return 1;
}
