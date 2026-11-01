/*
 * The Direct3D 8 backend of the render device: each function calls the COM method of its name through the vtable
 * of the object.
 */
#include "rdevbackend.h"

static uint32_t D3D8_AddRef(void *obj) {
	return ((uint32_t(__stdcall *)(void *))(*(void ***)obj)[0x4 / 4])(obj);
}

static uint32_t D3D8_Release(void *obj) {
	return ((uint32_t(__stdcall *)(void *))(*(void ***)obj)[0x8 / 4])(obj);
}

static int D3D8_D3DCheckDepthStencilMatch(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t rt,
	uint32_t ds) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, uint32_t, uint32_t,
		uint32_t))(*(void ***)d3d)[0x30 / 4])(d3d, adapter, type, fmt, rt, ds);
}

static int D3D8_D3DCheckDeviceFormat(void *d3d, uint32_t adapter, int devType, int adapterFmt, uint32_t usage,
	int rtype, int fmt) {
	return ((int(__stdcall *)(void *, uint32_t, int, int, uint32_t, int, int))(*(void ***)d3d)[0x28 / 4])(d3d,
		adapter, devType, adapterFmt, usage, rtype, fmt);
}

static int D3D8_D3DCheckDeviceMultiSampleType(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, int windowed,
	uint32_t ms) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, uint32_t, int, uint32_t))(*(void ***)d3d)[0x2c / 4])(d3d,
		adapter, type, fmt, windowed, ms);
}

static int D3D8_D3DCheckDeviceType(void *d3d, uint32_t adapter, uint32_t type, uint32_t dfmt, uint32_t bfmt,
	int windowed) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, uint32_t, uint32_t, int))(*(void ***)d3d)[0x24 / 4])(d3d,
		adapter, type, dfmt, bfmt, windowed);
}

static int D3D8_D3DCreateDevice(void *d3d, uint32_t adapter, uint32_t type, void *hwnd, uint32_t behavior, void *pp,
	void **dev) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, void *, uint32_t, void *,
		void **))(*(void ***)d3d)[0x3c / 4])(d3d, adapter, type, hwnd, behavior, pp, dev);
}

static int D3D8_D3DEnumAdapterModes(void *d3d, uint32_t adapter, uint32_t mode, void *out) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, void *))(*(void ***)d3d)[0x1c / 4])(d3d, adapter, mode,
		out);
}

static uint32_t D3D8_D3DGetAdapterCount(void *d3d) {
	return ((uint32_t(__stdcall *)(void *))(*(void ***)d3d)[0x10 / 4])(d3d);
}

static int D3D8_D3DGetAdapterDisplayMode(void *d3d, uint32_t adapter, void *out) {
	return ((int(__stdcall *)(void *, uint32_t, void *))(*(void ***)d3d)[0x20 / 4])(d3d, adapter, out);
}

static int D3D8_D3DGetAdapterIdentifier(void *d3d, uint32_t adapter, uint32_t flags, void *id) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, void *))(*(void ***)d3d)[0x14 / 4])(d3d, adapter, flags,
		id);
}

static uint32_t D3D8_D3DGetAdapterModeCount(void *d3d, uint32_t adapter) {
	return ((uint32_t(__stdcall *)(void *, uint32_t))(*(void ***)d3d)[0x18 / 4])(d3d, adapter);
}

static int D3D8_D3DGetDeviceCaps(void *d3d, uint32_t adapter, uint32_t type, void *caps) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, void *))(*(void ***)d3d)[0x34 / 4])(d3d, adapter, type,
		caps);
}

static int D3D8_DevBeginScene(void *dev) {
	return ((int(__stdcall *)(void *))(*(void ***)dev)[0x88 / 4])(dev);
}

static int D3D8_DevClear(void *dev, uint32_t count, const void *rects, uint32_t flags, uint32_t color, float z,
	uint32_t stencil) {
	return ((int(__stdcall *)(void *, uint32_t, const void *, uint32_t, uint32_t, float,
		uint32_t))(*(void ***)dev)[0x90 / 4])(dev, count, rects, flags, color, z, stencil);
}

static int D3D8_DevCopyRects(void *dev, void *src, const void *rects, uint32_t n, void *dst, const void *points) {
	return ((int(__stdcall *)(void *, void *, const void *, uint32_t, void *,
		const void *))(*(void ***)dev)[0x70 / 4])(dev, src, rects, n, dst, points);
}

static int D3D8_DevCreateDepthStencilSurface(void *dev, uint32_t w, uint32_t h, int fmt, int multisample, void **surf) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, int, int, void **))(*(void ***)dev)[0x68 / 4])(dev, w, h,
		fmt, multisample, surf);
}

static int D3D8_DevCreateImageSurface(void *dev, uint32_t w, uint32_t h, int fmt, void **surf) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, int, void **))(*(void ***)dev)[0x6c / 4])(dev, w, h, fmt,
		surf);
}

static int D3D8_DevCreateIndexBuffer(void *dev, uint32_t len, uint32_t usage, int fmt, int pool, void **ib) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, int, int, void **))(*(void ***)dev)[0x60 / 4])(dev, len,
		usage, fmt, pool, ib);
}

static int D3D8_DevCreatePixelShader(void *dev, const void *code, uint32_t *handle) {
	return ((int(__stdcall *)(void *, const void *, uint32_t *))(*(void ***)dev)[0x15c / 4])(dev, code, handle);
}

static int D3D8_DevCreateTexture(void *dev, uint32_t w, uint32_t h, uint32_t levels, uint32_t usage, int fmt, int pool,
	void **tex) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, uint32_t, uint32_t, int, int,
		void **))(*(void ***)dev)[0x50 / 4])(dev, w, h, levels, usage, fmt, pool, tex);
}

static int D3D8_DevCreateVertexBuffer(void *dev, uint32_t len, uint32_t usage, uint32_t fvf, int pool, void **vb) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, uint32_t, int, void **))(*(void ***)dev)[0x5c / 4])(dev,
		len, usage, fvf, pool, vb);
}

static int D3D8_DevDeletePixelShader(void *dev, uint32_t handle) {
	return ((int(__stdcall *)(void *, uint32_t))(*(void ***)dev)[0x168 / 4])(dev, handle);
}

static int D3D8_DevDrawIndexedPrimitive(void *dev, int type, uint32_t minIndex, uint32_t numVertices,
	uint32_t startIndex, uint32_t primCount) {
	return ((int(__stdcall *)(void *, int, uint32_t, uint32_t, uint32_t, uint32_t))(*(void ***)dev)[0x11c / 4])(dev,
		type, minIndex, numVertices, startIndex, primCount);
}

static int D3D8_DevDrawIndexedPrimitiveUP(void *dev, int type, uint32_t minIndex, uint32_t numVertices,
	uint32_t primCount, const void *idx, int idxFmt, const void *data, uint32_t stride) {
	return ((int(__stdcall *)(void *, int, uint32_t, uint32_t, uint32_t, const void *, int, const void *,
		uint32_t))(*(void ***)dev)[0x124 / 4])(dev, type, minIndex, numVertices, primCount, idx, idxFmt, data,
		stride);
}

static int D3D8_DevDrawPrimitive(void *dev, int type, uint32_t start, uint32_t count) {
	return ((int(__stdcall *)(void *, int, uint32_t, uint32_t))(*(void ***)dev)[0x118 / 4])(dev, type, start,
		count);
}

static int D3D8_DevDrawPrimitiveUP(void *dev, int type, uint32_t count, const void *data, uint32_t stride) {
	return ((int(__stdcall *)(void *, int, uint32_t, const void *, uint32_t))(*(void ***)dev)[0x120 / 4])(dev, type,
		count, data, stride);
}

static int D3D8_DevEndScene(void *dev) {
	return ((int(__stdcall *)(void *))(*(void ***)dev)[0x8c / 4])(dev);
}

static uint32_t D3D8_DevGetAvailableTextureMem(void *dev) {
	return ((uint32_t(__stdcall *)(void *))(*(void ***)dev)[0x10 / 4])(dev);
}

static int D3D8_DevGetBackBuffer(void *dev, uint32_t i, int type, void **surf) {
	return ((int(__stdcall *)(void *, uint32_t, int, void **))(*(void ***)dev)[0x40 / 4])(dev, i, type, surf);
}

static int D3D8_DevGetDepthStencilSurface(void *dev, void **surf) {
	return ((int(__stdcall *)(void *, void **))(*(void ***)dev)[0x84 / 4])(dev, surf);
}

static int D3D8_DevGetDeviceCaps(void *dev, void *caps) {
	return ((int(__stdcall *)(void *, void *))(*(void ***)dev)[0x1c / 4])(dev, caps);
}

static int D3D8_DevGetDirect3D(void *dev, void **d3d) {
	return ((int(__stdcall *)(void *, void **))(*(void ***)dev)[0x18 / 4])(dev, d3d);
}

static int D3D8_DevGetDisplayMode(void *dev, void *mode) {
	return ((int(__stdcall *)(void *, void *))(*(void ***)dev)[0x20 / 4])(dev, mode);
}

static int D3D8_DevGetRenderState(void *dev, uint32_t state, uint32_t *value) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t *))(*(void ***)dev)[0xcc / 4])(dev, state, value);
}

static int D3D8_DevGetRenderTarget(void *dev, void **surf) {
	return ((int(__stdcall *)(void *, void **))(*(void ***)dev)[0x80 / 4])(dev, surf);
}

static int D3D8_DevGetTextureStageState(void *dev, uint32_t stage, uint32_t type, uint32_t *value) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, uint32_t *))(*(void ***)dev)[0xf8 / 4])(dev, stage, type,
		value);
}

static int D3D8_DevGetTransform(void *dev, uint32_t state, void *m) {
	return ((int(__stdcall *)(void *, uint32_t, void *))(*(void ***)dev)[0x98 / 4])(dev, state, m);
}

static int D3D8_DevGetViewport(void *dev, uint32_t *vp) {
	return ((int(__stdcall *)(void *, uint32_t *))(*(void ***)dev)[0xa4 / 4])(dev, vp);
}

static int D3D8_DevLightEnable(void *dev, uint32_t idx, int enable) {
	return ((int(__stdcall *)(void *, uint32_t, int))(*(void ***)dev)[0xb8 / 4])(dev, idx, enable);
}

static int D3D8_DevPresent(void *dev, const void *src, const void *dst, void *hwnd, const void *dirty) {
	return ((int(__stdcall *)(void *, const void *, const void *, void *,
		const void *))(*(void ***)dev)[0x3c / 4])(dev, src, dst, hwnd, dirty);
}

static int D3D8_DevReset(void *dev, void *pp) {
	return ((int(__stdcall *)(void *, void *))(*(void ***)dev)[0x38 / 4])(dev, pp);
}

static void D3D8_DevSetCursorPosition(void *dev, int x, int y, uint32_t flags) {
	((void(__stdcall *)(void *, int, int, uint32_t))(*(void ***)dev)[0x2c / 4])(dev, x, y, flags);
}

static int D3D8_DevSetGammaRamp(void *dev, uint32_t flags, const void *ramp) {
	return ((int(__stdcall *)(void *, uint32_t, const void *))(*(void ***)dev)[0x48 / 4])(dev, flags, ramp);
}

static int D3D8_DevSetIndices(void *dev, void *ib, uint32_t baseVertex) {
	return ((int(__stdcall *)(void *, void *, uint32_t))(*(void ***)dev)[0x154 / 4])(dev, ib, baseVertex);
}

static int D3D8_DevSetLight(void *dev, uint32_t idx, const void *l) {
	return ((int(__stdcall *)(void *, uint32_t, const void *))(*(void ***)dev)[0xb0 / 4])(dev, idx, l);
}

static int D3D8_DevSetMaterial(void *dev, const void *mat) {
	return ((int(__stdcall *)(void *, const void *))(*(void ***)dev)[0xa8 / 4])(dev, mat);
}

static int D3D8_DevSetPixelShader(void *dev, uint32_t handle) {
	return ((int(__stdcall *)(void *, uint32_t))(*(void ***)dev)[0x160 / 4])(dev, handle);
}

static int D3D8_DevSetPixelShaderConstant(void *dev, uint32_t reg, const void *data, uint32_t count) {
	return ((int(__stdcall *)(void *, uint32_t, const void *, uint32_t))(*(void ***)dev)[0x16c / 4])(dev, reg, data,
		count);
}

static int D3D8_DevSetRenderState(void *dev, uint32_t state, uint32_t value) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t))(*(void ***)dev)[0xc8 / 4])(dev, state, value);
}

static int D3D8_DevSetRenderTarget(void *dev, void *rt, void *zs) {
	return ((int(__stdcall *)(void *, void *, void *))(*(void ***)dev)[0x7c / 4])(dev, rt, zs);
}

static int D3D8_DevSetStreamSource(void *dev, uint32_t stream, void *vb, uint32_t stride) {
	return ((int(__stdcall *)(void *, uint32_t, void *, uint32_t))(*(void ***)dev)[0x14c / 4])(dev, stream, vb,
		stride);
}

static int D3D8_DevSetTexture(void *dev, uint32_t stage, void *tex) {
	return ((int(__stdcall *)(void *, uint32_t, void *))(*(void ***)dev)[0xf4 / 4])(dev, stage, tex);
}

static int D3D8_DevSetTextureStageState(void *dev, uint32_t stage, uint32_t type, uint32_t value) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, uint32_t))(*(void ***)dev)[0xfc / 4])(dev, stage, type,
		value);
}

static int D3D8_DevSetTransform(void *dev, uint32_t state, const void *m) {
	return ((int(__stdcall *)(void *, uint32_t, const void *))(*(void ***)dev)[0x94 / 4])(dev, state, m);
}

static int D3D8_DevSetVertexShader(void *dev, uint32_t fvf) {
	return ((int(__stdcall *)(void *, uint32_t))(*(void ***)dev)[0x130 / 4])(dev, fvf);
}

static int D3D8_DevSetViewport(void *dev, const uint32_t *vp) {
	return ((int(__stdcall *)(void *, const uint32_t *))(*(void ***)dev)[0xa0 / 4])(dev, vp);
}

static int D3D8_DevShowCursor(void *dev, int show) {
	return ((int(__stdcall *)(void *, int))(*(void ***)dev)[0x30 / 4])(dev, show);
}

static int D3D8_DevTestCooperativeLevel(void *dev) {
	return ((int(__stdcall *)(void *))(*(void ***)dev)[0xc / 4])(dev);
}

static int D3D8_DevUpdateTexture(void *dev, void *src, void *dst) {
	return ((int(__stdcall *)(void *, void *, void *))(*(void ***)dev)[0x74 / 4])(dev, src, dst);
}

static int D3D8_TexGetDevice(void *tex, void **dev) {
	return ((int(__stdcall *)(void *, void **))(*(void ***)tex)[0xc / 4])(tex, dev);
}

static uint32_t D3D8_TexGetLevelCount(void *tex) {
	return ((uint32_t(__stdcall *)(void *))(*(void ***)tex)[0x34 / 4])(tex);
}

static int D3D8_TexGetLevelDesc(void *tex, uint32_t level, void *desc) {
	return ((int(__stdcall *)(void *, uint32_t, void *))(*(void ***)tex)[0x38 / 4])(tex, level, desc);
}

static int D3D8_TexGetSurfaceLevel(void *tex, uint32_t level, void **surf) {
	return ((int(__stdcall *)(void *, uint32_t, void **))(*(void ***)tex)[0x3c / 4])(tex, level, surf);
}

static int D3D8_TexLockRect(void *tex, uint32_t level, void *locked, const void *rect, uint32_t flags) {
	return ((int(__stdcall *)(void *, uint32_t, void *, const void *, uint32_t))(*(void ***)tex)[0x40 / 4])(tex,
		level, locked, rect, flags);
}

static uint32_t D3D8_TexSetLOD(void *tex, uint32_t lod) {
	return ((uint32_t(__stdcall *)(void *, uint32_t))(*(void ***)tex)[0x2c / 4])(tex, lod);
}

static int D3D8_TexUnlockRect(void *tex, uint32_t level) {
	return ((int(__stdcall *)(void *, uint32_t))(*(void ***)tex)[0x44 / 4])(tex, level);
}

static int D3D8_SurfGetDesc(void *surf, void *desc) {
	return ((int(__stdcall *)(void *, void *))(*(void ***)surf)[0x20 / 4])(surf, desc);
}

static int D3D8_SurfGetDevice(void *surf, void **dev) {
	return ((int(__stdcall *)(void *, void **))(*(void ***)surf)[0xc / 4])(surf, dev);
}

static int D3D8_SurfLockRect(void *surf, void *lr, const void *rect, uint32_t flags) {
	return ((int(__stdcall *)(void *, void *, const void *, uint32_t))(*(void ***)surf)[0x24 / 4])(surf, lr, rect,
		flags);
}

static int D3D8_SurfUnlockRect(void *surf) {
	return ((int(__stdcall *)(void *))(*(void ***)surf)[0x28 / 4])(surf);
}

static int D3D8_BufLock(void *buf, uint32_t off, uint32_t size, void *data, uint32_t flags) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, void *, uint32_t))(*(void ***)buf)[0x2c / 4])(buf, off,
		size, data, flags);
}

static int D3D8_BufUnlock(void *buf) {
	return ((int(__stdcall *)(void *))(*(void ***)buf)[0x30 / 4])(buf);
}

static void *D3D8_XBufGetBufferPointer(void *buf) {
	return ((void *(__stdcall *)(void *))(*(void ***)buf)[0xc / 4])(buf);
}

static int D3D8_XMeshDrawSubset(void *mesh, uint32_t attrib) {
	return ((int(__stdcall *)(void *, uint32_t))(*(void ***)mesh)[0xc / 4])(mesh, attrib);
}

static int D3D8_XMeshGetAttributeTable(void *mesh, void *table, uint32_t *size) {
	return ((int(__stdcall *)(void *, void *, uint32_t *))(*(void ***)mesh)[0x48 / 4])(mesh, table, size);
}

static uint32_t D3D8_XMeshGetNumFaces(void *mesh) {
	return ((uint32_t(__stdcall *)(void *))(*(void ***)mesh)[0x10 / 4])(mesh);
}

static uint32_t D3D8_XMeshGetNumVertices(void *mesh) {
	return ((uint32_t(__stdcall *)(void *))(*(void ***)mesh)[0x14 / 4])(mesh);
}

const RdBackend rd_d3d8 = {
	.name = "d3d8",
	.AddRef = D3D8_AddRef,
	.Release = D3D8_Release,
	.D3DCheckDepthStencilMatch = D3D8_D3DCheckDepthStencilMatch,
	.D3DCheckDeviceFormat = D3D8_D3DCheckDeviceFormat,
	.D3DCheckDeviceMultiSampleType = D3D8_D3DCheckDeviceMultiSampleType,
	.D3DCheckDeviceType = D3D8_D3DCheckDeviceType,
	.D3DCreateDevice = D3D8_D3DCreateDevice,
	.D3DEnumAdapterModes = D3D8_D3DEnumAdapterModes,
	.D3DGetAdapterCount = D3D8_D3DGetAdapterCount,
	.D3DGetAdapterDisplayMode = D3D8_D3DGetAdapterDisplayMode,
	.D3DGetAdapterIdentifier = D3D8_D3DGetAdapterIdentifier,
	.D3DGetAdapterModeCount = D3D8_D3DGetAdapterModeCount,
	.D3DGetDeviceCaps = D3D8_D3DGetDeviceCaps,
	.DevBeginScene = D3D8_DevBeginScene,
	.DevClear = D3D8_DevClear,
	.DevCopyRects = D3D8_DevCopyRects,
	.DevCreateDepthStencilSurface = D3D8_DevCreateDepthStencilSurface,
	.DevCreateImageSurface = D3D8_DevCreateImageSurface,
	.DevCreateIndexBuffer = D3D8_DevCreateIndexBuffer,
	.DevCreatePixelShader = D3D8_DevCreatePixelShader,
	.DevCreateTexture = D3D8_DevCreateTexture,
	.DevCreateVertexBuffer = D3D8_DevCreateVertexBuffer,
	.DevDeletePixelShader = D3D8_DevDeletePixelShader,
	.DevDrawIndexedPrimitive = D3D8_DevDrawIndexedPrimitive,
	.DevDrawIndexedPrimitiveUP = D3D8_DevDrawIndexedPrimitiveUP,
	.DevDrawPrimitive = D3D8_DevDrawPrimitive,
	.DevDrawPrimitiveUP = D3D8_DevDrawPrimitiveUP,
	.DevEndScene = D3D8_DevEndScene,
	.DevGetAvailableTextureMem = D3D8_DevGetAvailableTextureMem,
	.DevGetBackBuffer = D3D8_DevGetBackBuffer,
	.DevGetDepthStencilSurface = D3D8_DevGetDepthStencilSurface,
	.DevGetDeviceCaps = D3D8_DevGetDeviceCaps,
	.DevGetDirect3D = D3D8_DevGetDirect3D,
	.DevGetDisplayMode = D3D8_DevGetDisplayMode,
	.DevGetRenderState = D3D8_DevGetRenderState,
	.DevGetRenderTarget = D3D8_DevGetRenderTarget,
	.DevGetTextureStageState = D3D8_DevGetTextureStageState,
	.DevGetTransform = D3D8_DevGetTransform,
	.DevGetViewport = D3D8_DevGetViewport,
	.DevLightEnable = D3D8_DevLightEnable,
	.DevPresent = D3D8_DevPresent,
	.DevReset = D3D8_DevReset,
	.DevSetCursorPosition = D3D8_DevSetCursorPosition,
	.DevSetGammaRamp = D3D8_DevSetGammaRamp,
	.DevSetIndices = D3D8_DevSetIndices,
	.DevSetLight = D3D8_DevSetLight,
	.DevSetMaterial = D3D8_DevSetMaterial,
	.DevSetPixelShader = D3D8_DevSetPixelShader,
	.DevSetPixelShaderConstant = D3D8_DevSetPixelShaderConstant,
	.DevSetRenderState = D3D8_DevSetRenderState,
	.DevSetRenderTarget = D3D8_DevSetRenderTarget,
	.DevSetStreamSource = D3D8_DevSetStreamSource,
	.DevSetTexture = D3D8_DevSetTexture,
	.DevSetTextureStageState = D3D8_DevSetTextureStageState,
	.DevSetTransform = D3D8_DevSetTransform,
	.DevSetVertexShader = D3D8_DevSetVertexShader,
	.DevSetViewport = D3D8_DevSetViewport,
	.DevShowCursor = D3D8_DevShowCursor,
	.DevTestCooperativeLevel = D3D8_DevTestCooperativeLevel,
	.DevUpdateTexture = D3D8_DevUpdateTexture,
	.TexGetDevice = D3D8_TexGetDevice,
	.TexGetLevelCount = D3D8_TexGetLevelCount,
	.TexGetLevelDesc = D3D8_TexGetLevelDesc,
	.TexGetSurfaceLevel = D3D8_TexGetSurfaceLevel,
	.TexLockRect = D3D8_TexLockRect,
	.TexSetLOD = D3D8_TexSetLOD,
	.TexUnlockRect = D3D8_TexUnlockRect,
	.SurfGetDesc = D3D8_SurfGetDesc,
	.SurfGetDevice = D3D8_SurfGetDevice,
	.SurfLockRect = D3D8_SurfLockRect,
	.SurfUnlockRect = D3D8_SurfUnlockRect,
	.BufLock = D3D8_BufLock,
	.BufUnlock = D3D8_BufUnlock,
	.XBufGetBufferPointer = D3D8_XBufGetBufferPointer,
	.XMeshDrawSubset = D3D8_XMeshDrawSubset,
	.XMeshGetAttributeTable = D3D8_XMeshGetAttributeTable,
	.XMeshGetNumFaces = D3D8_XMeshGetNumFaces,
	.XMeshGetNumVertices = D3D8_XMeshGetNumVertices,
};
