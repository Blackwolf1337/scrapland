/*
 * The render device: the methods of Direct3D 8 that the game uses, as plain functions on opaque objects (the
 * IDirect3D8, the device, its textures, surfaces and buffers, a D3DX mesh), so the renderer behind them can change.
 * The first argument is always the object. They pass the call to the backend of src/game/engine3d/Render/Rdev.c
 * (rdevbackend.h); the device calls are written to the D3D log of the tests first (RdevLog.c).
 */
#ifndef SCRAP_RDEV_H
#define SCRAP_RDEV_H

#include "scrap/shared.h"

/* the IDirect3D8 of the backend in use (Direct3DCreate8), NULL when it cannot start */
void *Rd_Direct3DCreate8(uint32_t sdk);

/* IUnknown of every object */
uint32_t Rd_AddRef(void *obj);
uint32_t Rd_Release(void *obj);

int Rd_D3DCheckDepthStencilMatch(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t rt, uint32_t ds);
int Rd_D3DCheckDeviceFormat(void *d3d, uint32_t adapter, int devType, int adapterFmt, uint32_t usage, int rtype,
	int fmt);
int Rd_D3DCheckDeviceMultiSampleType(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, int windowed,
	uint32_t ms);
int Rd_D3DCheckDeviceType(void *d3d, uint32_t adapter, uint32_t type, uint32_t dfmt, uint32_t bfmt, int windowed);
int Rd_D3DCreateDevice(void *d3d, uint32_t adapter, uint32_t type, void *hwnd, uint32_t behavior, void *pp, void **dev);
int Rd_D3DEnumAdapterModes(void *d3d, uint32_t adapter, uint32_t mode, void *out);
uint32_t Rd_D3DGetAdapterCount(void *d3d);
int Rd_D3DGetAdapterDisplayMode(void *d3d, uint32_t adapter, void *out);
int Rd_D3DGetAdapterIdentifier(void *d3d, uint32_t adapter, uint32_t flags, void *id);
uint32_t Rd_D3DGetAdapterModeCount(void *d3d, uint32_t adapter);
int Rd_D3DGetDeviceCaps(void *d3d, uint32_t adapter, uint32_t type, void *caps);
int Rd_DevBeginScene(void *dev);
int Rd_DevClear(void *dev, uint32_t count, const void *rects, uint32_t flags, uint32_t color, float z,
	uint32_t stencil);
int Rd_DevCopyRects(void *dev, void *src, const void *rects, uint32_t n, void *dst, const void *points);
int Rd_DevCreateDepthStencilSurface(void *dev, uint32_t w, uint32_t h, int fmt, int multisample, void **surf);
int Rd_DevCreateImageSurface(void *dev, uint32_t w, uint32_t h, int fmt, void **surf);
int Rd_DevCreateIndexBuffer(void *dev, uint32_t len, uint32_t usage, int fmt, int pool, void **ib);
/* name: the file of the shader (bmp/<name>.psh), which picks the GLSL port under Vulkan; code: its D3D8 tokens */
int Rd_DevCreatePixelShader(void *dev, const char *name, const void *code, uint32_t *handle);
int Rd_DevCreateTexture(void *dev, uint32_t w, uint32_t h, uint32_t levels, uint32_t usage, int fmt, int pool,
	void **tex);
int Rd_DevCreateVertexBuffer(void *dev, uint32_t len, uint32_t usage, uint32_t fvf, int pool, void **vb);
int Rd_DevDeletePixelShader(void *dev, uint32_t handle);
int Rd_DevDrawIndexedPrimitive(void *dev, int type, uint32_t minIndex, uint32_t numVertices, uint32_t startIndex,
	uint32_t primCount);
int Rd_DevDrawIndexedPrimitiveUP(void *dev, int type, uint32_t minIndex, uint32_t numVertices, uint32_t primCount,
	const void *idx, int idxFmt, const void *data, uint32_t stride);
int Rd_DevDrawPrimitive(void *dev, int type, uint32_t start, uint32_t count);
int Rd_DevDrawPrimitiveUP(void *dev, int type, uint32_t count, const void *data, uint32_t stride);
int Rd_DevEndScene(void *dev);
uint32_t Rd_DevGetAvailableTextureMem(void *dev);
int Rd_DevGetBackBuffer(void *dev, uint32_t i, int type, void **surf);
int Rd_DevGetDepthStencilSurface(void *dev, void **surf);
int Rd_DevGetDeviceCaps(void *dev, void *caps);
int Rd_DevGetDirect3D(void *dev, void **d3d);
int Rd_DevGetDisplayMode(void *dev, void *mode);
int Rd_DevGetRenderState(void *dev, uint32_t state, uint32_t *value);
int Rd_DevGetRenderTarget(void *dev, void **surf);
int Rd_DevGetTextureStageState(void *dev, uint32_t stage, uint32_t type, uint32_t *value);
int Rd_DevGetTransform(void *dev, uint32_t state, void *m);
int Rd_DevGetViewport(void *dev, uint32_t *vp);
int Rd_DevLightEnable(void *dev, uint32_t idx, int enable);
int Rd_DevPresent(void *dev, const void *src, const void *dst, void *hwnd, const void *dirty);
int Rd_DevReset(void *dev, void *pp);
void Rd_DevSetCursorPosition(void *dev, int x, int y, uint32_t flags);
int Rd_DevSetGammaRamp(void *dev, uint32_t flags, const void *ramp);
int Rd_DevSetIndices(void *dev, void *ib, uint32_t baseVertex);
int Rd_DevSetLight(void *dev, uint32_t idx, const void *l);
int Rd_DevSetMaterial(void *dev, const void *mat);
int Rd_DevSetPixelShader(void *dev, uint32_t handle);
int Rd_DevSetPixelShaderConstant(void *dev, uint32_t reg, const void *data, uint32_t count);
int Rd_DevSetRenderState(void *dev, uint32_t state, uint32_t value);
int Rd_DevSetRenderTarget(void *dev, void *rt, void *zs);
int Rd_DevSetStreamSource(void *dev, uint32_t stream, void *vb, uint32_t stride);
int Rd_DevSetTexture(void *dev, uint32_t stage, void *tex);
int Rd_DevSetTextureStageState(void *dev, uint32_t stage, uint32_t type, uint32_t value);
int Rd_DevSetTransform(void *dev, uint32_t state, const void *m);
int Rd_DevSetVertexShader(void *dev, uint32_t fvf);
int Rd_DevSetViewport(void *dev, const uint32_t *vp);
int Rd_DevShowCursor(void *dev, int show);
int Rd_DevTestCooperativeLevel(void *dev);
int Rd_DevUpdateTexture(void *dev, void *src, void *dst);
int Rd_TexGetDevice(void *tex, void **dev);
uint32_t Rd_TexGetLevelCount(void *tex);
int Rd_TexGetLevelDesc(void *tex, uint32_t level, void *desc);
int Rd_TexGetSurfaceLevel(void *tex, uint32_t level, void **surf);
int Rd_TexLockRect(void *tex, uint32_t level, void *locked, const void *rect, uint32_t flags);
uint32_t Rd_TexSetLOD(void *tex, uint32_t lod);
int Rd_TexUnlockRect(void *tex, uint32_t level);
int Rd_SurfGetDesc(void *surf, void *desc);
int Rd_SurfGetDevice(void *surf, void **dev);
int Rd_SurfLockRect(void *surf, void *lr, const void *rect, uint32_t flags);
int Rd_SurfUnlockRect(void *surf);
int Rd_BufLock(void *buf, uint32_t off, uint32_t size, void *data, uint32_t flags);
int Rd_BufUnlock(void *buf);
void *Rd_XBufGetBufferPointer(void *buf);
int Rd_XMeshDrawSubset(void *mesh, uint32_t attrib);
int Rd_XMeshGetAttributeTable(void *mesh, void *table, uint32_t *size);
uint32_t Rd_XMeshGetNumFaces(void *mesh);
uint32_t Rd_XMeshGetNumVertices(void *mesh);

#endif
