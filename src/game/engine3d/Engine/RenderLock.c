/*
 * The frame lock of the engine3d, original source Engine/RenderLock.cpp (include/scrap/render.h): small render
 * targets the GPU must finish one of the last 1 to 3 frames into before the CPU goes on, so the driver can not queue
 * more frames. Mode 1 copies the back buffer, mode 2 a system memory texture (noise with RenderLockNoise), mode 3
 * clears the target.
 */
#include "scrap/rdev.h"
#include "scrap/gvar.h"
#include "scrap/crt.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/render.h"
#include "scrap/staticinit.h"

#define RENDERLOCK_MAX 3
#define RENDERLOCK_SIZE 4
#define D3DFMT_R5G6B5 23
#define D3DBACKBUFFER_TYPE_MONO 0

static cGVar GVar_R_VideoLockMode;

/* the engine variables: RenderLock (the mode), RenderLockFrames (1..3) and RenderLockNoise */
#define g_RenderLock (GVar_R_VideoLockMode.value.i)
static cGVar GVar_R_VideoLockBuf;

#define g_RenderLockFrames (GVar_R_VideoLockBuf.value.i)
static cGVar GVar_R_VideoLockShow;

#define g_RenderLockNoise (GVar_R_VideoLockShow.value.i)
static void *g_TextLock[7];

#define g_TextMem (g_TextLock[3])		/* system memory */
#define g_TextBack (g_TextLock[4])		/* in the format of the back buffer */
#define g_TextLockIdx (*(int *)&g_TextLock[5])

/* D3DLOCKED_RECT */
typedef struct LockedRect {
	int Pitch;
	uint32_t *pBits;
} LockedRect;

static void RenderLock_Free(void **tex) {
	if (*tex) {
		Rd_Release(*tex);
		*tex = NULL;
	}
}

void RenderLock_ReleaseD3D(void) {
	RenderLock_Free(&g_TextLock[0]);
	RenderLock_Free(&g_TextLock[1]);
	RenderLock_Free(&g_TextLock[2]);
	RenderLock_Free(&g_TextMem);
	RenderLock_Free(&g_TextBack);
}

static void RenderLock_Create(uint32_t usage, int fmt, int pool, void **tex) {
	void *dev = g_pd3dDevice;
	Rd_DevCreateTexture(dev, RENDERLOCK_SIZE, RENDERLOCK_SIZE, 1, usage, fmt,
		pool, tex);
}

void RenderLock_InitD3D(void) {
	if (g_RenderOff) {
		return;
	}
	CHECK(!g_TextLock[0] && !g_TextLock[1] && !g_TextLock[2] && !g_TextMem && !g_TextBack);
	int fmt = g_Engine->backBufferDesc.Format;
	RenderLock_Create(D3DUSAGE_RENDERTARGET, D3DFMT_R5G6B5, D3DPOOL_DEFAULT, &g_TextLock[0]);
	RenderLock_Create(D3DUSAGE_RENDERTARGET, D3DFMT_R5G6B5, D3DPOOL_DEFAULT, &g_TextLock[1]);
	RenderLock_Create(D3DUSAGE_RENDERTARGET, D3DFMT_R5G6B5, D3DPOOL_DEFAULT, &g_TextLock[2]);
	RenderLock_Create(0, D3DFMT_R5G6B5, D3DPOOL_SYSTEMMEM, &g_TextMem);
	RenderLock_Create(0, fmt, D3DPOOL_SYSTEMMEM, &g_TextBack);
}

static const int32_t g_RenderLockRect[4] = { 0, 0, 4, 4 };

/* mode 1: a part of the back buffer into the system texture */
static void RenderLock_CopyBack(void) {
	if (!g_TextBack) {
		return;
	}
	void *surf = NULL;
	Rd_TexGetSurfaceLevel(g_TextBack, 0, &surf);
	if (!surf) {
		return;
	}
	void *back = NULL;
	void *dev = g_pd3dDevice;
	if (Rd_DevGetBackBuffer(dev, 0, D3DBACKBUFFER_TYPE_MONO, &back) >= 0) {
		int32_t rect[4] = { g_RenderLockRect[0], g_RenderLockRect[1], g_RenderLockRect[2], g_RenderLockRect[3] };
		dev = g_pd3dDevice;
		Rd_DevCopyRects(dev, back, rect, 1, surf, NULL);
		Rd_Release(back);
	}
	if (surf) {
		Rd_Release(surf);
	}
}

static int RenderLock_Ready(void) {
	return (uint32_t)(g_RenderLockFrames - 1) <= RENDERLOCK_MAX - 1 && g_TextLock[0] && g_TextLock[1] &&
		g_TextLock[2] && g_TextMem;
}

/* mode 2: the system texture into the next target, drawn with the GPU */
static box_t *Box_Set(box_t *self, const vec3_t *min, const vec3_t *max);
static void RenderLock_DrawMem(void) {
	LockedRect lr;

	if (!RenderLock_Ready()) {
		return;
	}
	if (Rd_TexLockRect(g_TextMem, 0, &lr, NULL, 0) >= 0) {
		uint32_t c = (g_RenderLockNoise ? 0xaau : 0u) * 0x1010101u;
		for (int i = 0; i < 8; i++) {
			lr.pBits[i] = c;
		}
		Rd_TexUnlockRect(g_TextMem, 0);
	}
	void *src = NULL;
	void *dst = NULL;
	if (Rd_TexGetSurfaceLevel(g_TextMem, 0, &src) >= 0) {
		void *t = g_TextLock[g_TextLockIdx];
		if (Rd_TexGetSurfaceLevel(t, 0, &dst) >= 0) {
			void *dev = g_pd3dDevice;
			Rd_DevCopyRects(dev, src, NULL, 0, dst, NULL);
		}
	}
	if (src) {
		Rd_Release(src);
		src = NULL;
	}
	if (dst) {
		Rd_Release(dst);
		dst = NULL;
	}
	if (Rd_TexLockRect(g_TextMem, 0, &lr, NULL, 0) >= 0) {
		if (g_RenderLockNoise) {
			uint32_t c = (byte)crt_rand() * 0x1010101u;
			for (int i = 0; i < 8; i++) {
				lr.pBits[i] = c;
			}
		}
		Rd_TexUnlockRect(g_TextMem, 0);
	}
	box_t rect;
	vec3_t min = { 0.0f, 0.0f, 0.0f };
	vec3_t max = { 0.0f, 0.0f, 0.0f };
	if (g_RenderLockNoise) {
		max.x = 1.0f;
		max.y = 0.002f;
	}
	Box_Set(&rect, &min, &max);
	Render_DrawTexRect(&rect, 0xffffffffu, 0xffffffffu, g_TextLock[g_TextLockIdx]);
	g_TextLockIdx = (g_TextLockIdx + 1) % g_RenderLockFrames;
}

/* mode 3: the next target cleared as the render target, then copied into the system texture */
static void RenderLock_Clear(void) {
	if (!RenderLock_Ready()) {
		return;
	}
	void *rt = NULL;
	void *oldRT = NULL;
	void *oldZS = NULL;
	void *dev = g_pd3dDevice;
	Rd_DevGetRenderTarget(dev, &oldRT);
	dev = g_pd3dDevice;
	Rd_DevGetDepthStencilSurface(dev, &oldZS);
	void *t = g_TextLock[g_TextLockIdx];
	Rd_TexGetSurfaceLevel(t, 0, &rt);
	dev = g_pd3dDevice;
	Rd_DevSetRenderTarget(dev, rt, NULL);
	if (rt) {
		Rd_Release(rt);
		rt = NULL;
	}
	dev = g_pd3dDevice;
	Rd_DevClear(dev, 0, NULL, 1, 0xff00, 1.0f, 0);
	dev = g_pd3dDevice;
	Rd_DevSetRenderTarget(dev, oldRT, oldZS);
	if (oldRT) {
		Rd_Release(oldRT);
		oldRT = NULL;
	}
	if (oldZS) {
		Rd_Release(oldZS);
		oldZS = NULL;
	}
	g_TextLockIdx = (g_TextLockIdx + 1) % g_RenderLockFrames;
	rt = NULL;
	t = g_TextLock[g_TextLockIdx];
	Rd_TexGetSurfaceLevel(t, 0, &rt);
	void *mem = NULL;
	if (!rt) {
		return;
	}
	if (Rd_TexGetSurfaceLevel(g_TextMem, 0, &mem) >= 0) {
		dev = g_pd3dDevice;
		Rd_DevCopyRects(dev, rt, NULL, 0, mem, NULL);
	}
	if (mem) {
		Rd_Release(mem);
		mem = NULL;
	}
	if (rt) {
		Rd_Release(rt);
	}
}

void RenderLock_Frame(void) {
	switch (g_RenderLock) {
	case 1:
		RenderLock_CopyBack();
		break;
	case 2:
		RenderLock_DrawMem();
		break;
	case 3:
		RenderLock_Clear();
		break;
	}
}

/* the box constructor of two corners (rva 0x2a4830, src/game/engine3d/Engine/RenderLock.c) */
static box_t *Box_Set(box_t *self, const vec3_t *min, const vec3_t *max) {
	self->min = *min;
	self->max = *max;
	return self;
}

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_R_VideoLockMode, &GVar_R_VideoLockMode, "R_VideoLockMode", 2, "Video frame lock mode (0/1/2/3)", 2)
INIT_GVAR_INT(Init_GVar_R_VideoLockBuf, &GVar_R_VideoLockBuf, "R_VideoLockBuf", 2, "Video frame lock buffers (1/2/3)", 1)
INIT_GVAR_INT(Init_GVar_R_VideoLockShow, &GVar_R_VideoLockShow, "R_VideoLockShow", 0, "Video frame lock show quad", 0)
