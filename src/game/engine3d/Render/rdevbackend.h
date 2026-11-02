/*
 * A backend of the render device (include/scrap/rdev.h): one function for each method, with the arguments of the
 * public function. rd is the backend in use.
 */
#ifndef RDEVBACKEND_H
#define RDEVBACKEND_H

#include "scrap/shared.h"

typedef struct RdBackend {
	const char *name;
	void *(*Direct3DCreate8)(uint32_t sdk);
	uint32_t(*AddRef)(void *obj);
	uint32_t(*Release)(void *obj);
	int(*D3DCheckDepthStencilMatch)(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t rt,
		uint32_t ds);
	int(*D3DCheckDeviceFormat)(void *d3d, uint32_t adapter, int devType, int adapterFmt, uint32_t usage, int rtype,
		int fmt);
	int(*D3DCheckDeviceMultiSampleType)(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, int windowed,
		uint32_t ms);
	int(*D3DCheckDeviceType)(void *d3d, uint32_t adapter, uint32_t type, uint32_t dfmt, uint32_t bfmt,
		int windowed);
	int(*D3DCreateDevice)(void *d3d, uint32_t adapter, uint32_t type, void *hwnd, uint32_t behavior, void *pp,
		void **dev);
	int(*D3DEnumAdapterModes)(void *d3d, uint32_t adapter, uint32_t mode, void *out);
	uint32_t(*D3DGetAdapterCount)(void *d3d);
	int(*D3DGetAdapterDisplayMode)(void *d3d, uint32_t adapter, void *out);
	int(*D3DGetAdapterIdentifier)(void *d3d, uint32_t adapter, uint32_t flags, void *id);
	uint32_t(*D3DGetAdapterModeCount)(void *d3d, uint32_t adapter);
	int(*D3DGetDeviceCaps)(void *d3d, uint32_t adapter, uint32_t type, void *caps);
	int(*DevBeginScene)(void *dev);
	int(*DevClear)(void *dev, uint32_t count, const void *rects, uint32_t flags, uint32_t color, float z,
		uint32_t stencil);
	int(*DevCopyRects)(void *dev, void *src, const void *rects, uint32_t n, void *dst, const void *points);
	int(*DevCreateDepthStencilSurface)(void *dev, uint32_t w, uint32_t h, int fmt, int multisample, void **surf);
	int(*DevCreateImageSurface)(void *dev, uint32_t w, uint32_t h, int fmt, void **surf);
	int(*DevCreateIndexBuffer)(void *dev, uint32_t len, uint32_t usage, int fmt, int pool, void **ib);
	int(*DevCreatePixelShader)(void *dev, const char *name, const void *code, uint32_t *handle);
	int(*DevCreateTexture)(void *dev, uint32_t w, uint32_t h, uint32_t levels, uint32_t usage, int fmt, int pool,
		void **tex);
	int(*DevCreateVertexBuffer)(void *dev, uint32_t len, uint32_t usage, uint32_t fvf, int pool, void **vb);
	int(*DevDeletePixelShader)(void *dev, uint32_t handle);
	int(*DevDrawIndexedPrimitive)(void *dev, int type, uint32_t minIndex, uint32_t numVertices, uint32_t startIndex,
		uint32_t primCount);
	int(*DevDrawIndexedPrimitiveUP)(void *dev, int type, uint32_t minIndex, uint32_t numVertices,
		uint32_t primCount, const void *idx, int idxFmt, const void *data, uint32_t stride);
	int(*DevDrawPrimitive)(void *dev, int type, uint32_t start, uint32_t count);
	int(*DevDrawPrimitiveUP)(void *dev, int type, uint32_t count, const void *data, uint32_t stride);
	int(*DevEndScene)(void *dev);
	uint32_t(*DevGetAvailableTextureMem)(void *dev);
	int(*DevGetBackBuffer)(void *dev, uint32_t i, int type, void **surf);
	int(*DevGetDepthStencilSurface)(void *dev, void **surf);
	int(*DevGetDeviceCaps)(void *dev, void *caps);
	int(*DevGetDirect3D)(void *dev, void **d3d);
	int(*DevGetDisplayMode)(void *dev, void *mode);
	int(*DevGetRenderState)(void *dev, uint32_t state, uint32_t *value);
	int(*DevGetRenderTarget)(void *dev, void **surf);
	int(*DevGetTextureStageState)(void *dev, uint32_t stage, uint32_t type, uint32_t *value);
	int(*DevGetTransform)(void *dev, uint32_t state, void *m);
	int(*DevGetViewport)(void *dev, uint32_t *vp);
	int(*DevLightEnable)(void *dev, uint32_t idx, int enable);
	int(*DevPresent)(void *dev, const void *src, const void *dst, void *hwnd, const void *dirty);
	int(*DevReset)(void *dev, void *pp);
	void(*DevSetCursorPosition)(void *dev, int x, int y, uint32_t flags);
	int(*DevSetGammaRamp)(void *dev, uint32_t flags, const void *ramp);
	int(*DevSetIndices)(void *dev, void *ib, uint32_t baseVertex);
	int(*DevSetLight)(void *dev, uint32_t idx, const void *l);
	int(*DevSetMaterial)(void *dev, const void *mat);
	int(*DevSetPixelShader)(void *dev, uint32_t handle);
	int(*DevSetPixelShaderConstant)(void *dev, uint32_t reg, const void *data, uint32_t count);
	int(*DevSetRenderState)(void *dev, uint32_t state, uint32_t value);
	int(*DevSetRenderTarget)(void *dev, void *rt, void *zs);
	int(*DevSetStreamSource)(void *dev, uint32_t stream, void *vb, uint32_t stride);
	int(*DevSetTexture)(void *dev, uint32_t stage, void *tex);
	int(*DevSetTextureStageState)(void *dev, uint32_t stage, uint32_t type, uint32_t value);
	int(*DevSetTransform)(void *dev, uint32_t state, const void *m);
	int(*DevSetVertexShader)(void *dev, uint32_t fvf);
	int(*DevSetViewport)(void *dev, const uint32_t *vp);
	int(*DevShowCursor)(void *dev, int show);
	int(*DevTestCooperativeLevel)(void *dev);
	int(*DevUpdateTexture)(void *dev, void *src, void *dst);
	int(*TexGetDevice)(void *tex, void **dev);
	uint32_t(*TexGetLevelCount)(void *tex);
	int(*TexGetLevelDesc)(void *tex, uint32_t level, void *desc);
	int(*TexGetSurfaceLevel)(void *tex, uint32_t level, void **surf);
	int(*TexLockRect)(void *tex, uint32_t level, void *locked, const void *rect, uint32_t flags);
	uint32_t(*TexSetLOD)(void *tex, uint32_t lod);
	int(*TexUnlockRect)(void *tex, uint32_t level);
	int(*SurfGetDesc)(void *surf, void *desc);
	int(*SurfGetDevice)(void *surf, void **dev);
	int(*SurfLockRect)(void *surf, void *lr, const void *rect, uint32_t flags);
	int(*SurfUnlockRect)(void *surf);
	int(*BufLock)(void *buf, uint32_t off, uint32_t size, void *data, uint32_t flags);
	int(*BufUnlock)(void *buf);
	void *(*XBufGetBufferPointer)(void *buf);
	int(*XMeshDrawSubset)(void *mesh, uint32_t attrib);
	int(*XMeshGetAttributeTable)(void *mesh, void *table, uint32_t *size);
	uint32_t(*XMeshGetNumFaces)(void *mesh);
	uint32_t(*XMeshGetNumVertices)(void *mesh);
} RdBackend;

extern const RdBackend *rd;
extern const RdBackend rd_d3d8;
extern const RdBackend rd_vk;

/* the D3D log of the tests (RdevLog.c): calls on rd_logDevice are written, then go to rd_logReal */
extern void *rd_logDevice;
extern void *rd_logReal;
void RdLog_Call(uint32_t slot, const uint32_t *a);

static inline uint32_t Rd_FloatBits(float f) {
	union {
		float f;
		uint32_t u;
	} x = { f };
	return x.u;
}

#endif
