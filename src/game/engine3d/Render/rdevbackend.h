/*
 * The Vulkan backend of the render device (vk/RdevVk.c): one function for each public function of
 * include/scrap/rdev.h, with its arguments; Rdev.c calls them after the D3D log.
 */
#ifndef RDEVBACKEND_H
#define RDEVBACKEND_H

#include "scrap/shared.h"

void *Vk_Direct3DCreate8(uint32_t sdk);
uint32_t Vk_AddRef(void *obj);
uint32_t Vk_Release(void *obj);
int Vk_D3DCheckDepthStencilMatch(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t rt, uint32_t ds);
int Vk_D3DCheckDeviceFormat(void *d3d, uint32_t adapter, int devType, int adapterFmt, uint32_t usage, int rtype,
	int fmt);
int Vk_D3DCheckDeviceMultiSampleType(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, int windowed,
	uint32_t ms);
int Vk_D3DCheckDeviceType(void *d3d, uint32_t adapter, uint32_t type, uint32_t dfmt, uint32_t bfmt, int windowed);
int Vk_D3DCreateDevice(void *d3d, uint32_t adapter, uint32_t type, void *hwnd, uint32_t behavior, void *pp, void **dev);
int Vk_D3DEnumAdapterModes(void *d3d, uint32_t adapter, uint32_t mode, void *out);
uint32_t Vk_D3DGetAdapterCount(void *d3d);
int Vk_D3DGetAdapterDisplayMode(void *d3d, uint32_t adapter, void *out);
int Vk_D3DGetAdapterIdentifier(void *d3d, uint32_t adapter, uint32_t flags, void *id);
uint32_t Vk_D3DGetAdapterModeCount(void *d3d, uint32_t adapter);
int Vk_D3DGetDeviceCaps(void *d3d, uint32_t adapter, uint32_t type, void *caps);
int Vk_DevBeginScene(void *dev);
int Vk_DevClear(void *dev, uint32_t count, const void *rects, uint32_t flags, uint32_t color, float z,
	uint32_t stencil);
int Vk_DevCopyRects(void *dev, void *src, const void *rects, uint32_t n, void *dst, const void *points);
int Vk_DevCreateDepthStencilSurface(void *dev, uint32_t w, uint32_t h, int fmt, int multisample, void **surf);
int Vk_DevCreateImageSurface(void *dev, uint32_t w, uint32_t h, int fmt, void **surf);
int Vk_DevCreateIndexBuffer(void *dev, uint32_t len, uint32_t usage, int fmt, int pool, void **ib);
int Vk_DevCreatePixelShader(void *dev, const char *name, const void *code, uint32_t *handle);
int Vk_DevCreateTexture(void *dev, uint32_t w, uint32_t h, uint32_t levels, uint32_t usage, int fmt, int pool,
	void **tex);
int Vk_DevCreateVertexBuffer(void *dev, uint32_t len, uint32_t usage, uint32_t fvf, int pool, void **vb);
int Vk_DevDeletePixelShader(void *dev, uint32_t handle);
int Vk_DevDrawIndexedPrimitive(void *dev, int type, uint32_t minIndex, uint32_t numVertices, uint32_t startIndex,
	uint32_t primCount);
int Vk_DevDrawIndexedPrimitiveUP(void *dev, int type, uint32_t minIndex, uint32_t numVertices, uint32_t primCount,
	const void *idx, int idxFmt, const void *data, uint32_t stride);
int Vk_DevDrawPrimitive(void *dev, int type, uint32_t start, uint32_t count);
int Vk_DevDrawPrimitiveUP(void *dev, int type, uint32_t count, const void *data, uint32_t stride);
int Vk_DevEndScene(void *dev);
uint32_t Vk_DevGetAvailableTextureMem(void *dev);
int Vk_DevGetBackBuffer(void *dev, uint32_t i, int type, void **surf);
int Vk_DevGetDepthStencilSurface(void *dev, void **surf);
int Vk_DevGetDeviceCaps(void *dev, void *caps);
int Vk_DevGetDirect3D(void *dev, void **d3d);
int Vk_DevGetDisplayMode(void *dev, void *mode);
int Vk_DevGetRenderState(void *dev, uint32_t state, uint32_t *value);
int Vk_DevGetRenderTarget(void *dev, void **surf);
int Vk_DevGetTextureStageState(void *dev, uint32_t stage, uint32_t type, uint32_t *value);
int Vk_DevGetTransform(void *dev, uint32_t state, void *m);
int Vk_DevGetViewport(void *dev, uint32_t *vp);
int Vk_DevLightEnable(void *dev, uint32_t idx, int enable);
int Vk_DevPresent(void *dev, const void *src, const void *dst, void *hwnd, const void *dirty);
int Vk_DevReset(void *dev, void *pp);
void Vk_DevSetCursorPosition(void *dev, int x, int y, uint32_t flags);
int Vk_DevSetGammaRamp(void *dev, uint32_t flags, const void *ramp);
int Vk_DevSetIndices(void *dev, void *ib, uint32_t baseVertex);
int Vk_DevSetLight(void *dev, uint32_t idx, const void *l);
int Vk_DevSetMaterial(void *dev, const void *mat);
int Vk_DevSetPixelShader(void *dev, uint32_t handle);
int Vk_DevSetPixelShaderConstant(void *dev, uint32_t reg, const void *data, uint32_t count);
int Vk_DevSetRenderState(void *dev, uint32_t state, uint32_t value);
int Vk_DevSetRenderTarget(void *dev, void *rt, void *zs);
int Vk_DevSetStreamSource(void *dev, uint32_t stream, void *vb, uint32_t stride);
int Vk_DevSetTexture(void *dev, uint32_t stage, void *tex);
int Vk_DevSetTextureStageState(void *dev, uint32_t stage, uint32_t type, uint32_t value);
int Vk_DevSetTransform(void *dev, uint32_t state, const void *m);
int Vk_DevSetVertexShader(void *dev, uint32_t fvf);
int Vk_DevSetViewport(void *dev, const uint32_t *vp);
int Vk_DevShowCursor(void *dev, int show);
int Vk_DevTestCooperativeLevel(void *dev);
int Vk_DevUpdateTexture(void *dev, void *src, void *dst);
int Vk_TexGetDevice(void *tex, void **dev);
uint32_t Vk_TexGetLevelCount(void *tex);
int Vk_TexGetLevelDesc(void *tex, uint32_t level, void *desc);
int Vk_TexGetSurfaceLevel(void *tex, uint32_t level, void **surf);
int Vk_TexLockRect(void *tex, uint32_t level, void *locked, const void *rect, uint32_t flags);
uint32_t Vk_TexSetLOD(void *tex, uint32_t lod);
int Vk_TexUnlockRect(void *tex, uint32_t level);
int Vk_SurfGetDesc(void *surf, void *desc);
int Vk_SurfGetDevice(void *surf, void **dev);
int Vk_SurfLockRect(void *surf, void *lr, const void *rect, uint32_t flags);
int Vk_SurfUnlockRect(void *surf);
int Vk_BufLock(void *buf, uint32_t off, uint32_t size, void *data, uint32_t flags);
int Vk_BufUnlock(void *buf);
void *Vk_XBufGetBufferPointer(void *buf);
int Vk_XMeshDrawSubset(void *mesh, uint32_t attrib);
int Vk_XMeshGetAttributeTable(void *mesh, void *table, uint32_t *size);
uint32_t Vk_XMeshGetNumFaces(void *mesh);
uint32_t Vk_XMeshGetNumVertices(void *mesh);


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
