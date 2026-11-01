/*
 * The Direct3D 8 backend of the render device (include/scrap/rdev.h): each function calls the COM method of its name
 * through the vtable of the object.
 */
#include "scrap/rdev.h"

uint32_t Rd_AddRef(void *obj) {
	return ((uint32_t(__stdcall *)(void *))(*(void ***)obj)[0x4 / 4])(obj);
}

uint32_t Rd_Release(void *obj) {
	return ((uint32_t(__stdcall *)(void *))(*(void ***)obj)[0x8 / 4])(obj);
}

int Rd_D3DCheckDepthStencilMatch(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t rt, uint32_t ds) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, uint32_t, uint32_t,
		uint32_t))(*(void ***)d3d)[0x30 / 4])(d3d, adapter, type, fmt, rt, ds);
}

int Rd_D3DCheckDeviceFormat(void *d3d, uint32_t adapter, int devType, int adapterFmt, uint32_t usage, int rtype,
	int fmt) {
	return ((int(__stdcall *)(void *, uint32_t, int, int, uint32_t, int, int))(*(void ***)d3d)[0x28 / 4])(d3d,
		adapter, devType, adapterFmt, usage, rtype, fmt);
}

int Rd_D3DCheckDeviceMultiSampleType(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, int windowed,
	uint32_t ms) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, uint32_t, int, uint32_t))(*(void ***)d3d)[0x2c / 4])(d3d,
		adapter, type, fmt, windowed, ms);
}

int Rd_D3DCheckDeviceType(void *d3d, uint32_t adapter, uint32_t type, uint32_t dfmt, uint32_t bfmt, int windowed) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, uint32_t, uint32_t, int))(*(void ***)d3d)[0x24 / 4])(d3d,
		adapter, type, dfmt, bfmt, windowed);
}

int Rd_D3DCreateDevice(void *d3d, uint32_t adapter, uint32_t type, void *hwnd, uint32_t behavior, void *pp,
	void **dev) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, void *, uint32_t, void *,
		void **))(*(void ***)d3d)[0x3c / 4])(d3d, adapter, type, hwnd, behavior, pp, dev);
}

int Rd_D3DEnumAdapterModes(void *d3d, uint32_t adapter, uint32_t mode, void *out) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, void *))(*(void ***)d3d)[0x1c / 4])(d3d, adapter, mode,
		out);
}

uint32_t Rd_D3DGetAdapterCount(void *d3d) {
	return ((uint32_t(__stdcall *)(void *))(*(void ***)d3d)[0x10 / 4])(d3d);
}

int Rd_D3DGetAdapterDisplayMode(void *d3d, uint32_t adapter, void *out) {
	return ((int(__stdcall *)(void *, uint32_t, void *))(*(void ***)d3d)[0x20 / 4])(d3d, adapter, out);
}

int Rd_D3DGetAdapterIdentifier(void *d3d, uint32_t adapter, uint32_t flags, void *id) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, void *))(*(void ***)d3d)[0x14 / 4])(d3d, adapter, flags,
		id);
}

uint32_t Rd_D3DGetAdapterModeCount(void *d3d, uint32_t adapter) {
	return ((uint32_t(__stdcall *)(void *, uint32_t))(*(void ***)d3d)[0x18 / 4])(d3d, adapter);
}

int Rd_D3DGetDeviceCaps(void *d3d, uint32_t adapter, uint32_t type, void *caps) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, void *))(*(void ***)d3d)[0x34 / 4])(d3d, adapter, type,
		caps);
}

int Rd_DevBeginScene(void *dev) {
	return ((int(__stdcall *)(void *))(*(void ***)dev)[0x88 / 4])(dev);
}

int Rd_DevClear(void *dev, uint32_t count, const void *rects, uint32_t flags, uint32_t color, float z,
	uint32_t stencil) {
	return ((int(__stdcall *)(void *, uint32_t, const void *, uint32_t, uint32_t, float,
		uint32_t))(*(void ***)dev)[0x90 / 4])(dev, count, rects, flags, color, z, stencil);
}

int Rd_DevCopyRects(void *dev, void *src, const void *rects, uint32_t n, void *dst, const void *points) {
	return ((int(__stdcall *)(void *, void *, const void *, uint32_t, void *,
		const void *))(*(void ***)dev)[0x70 / 4])(dev, src, rects, n, dst, points);
}

int Rd_DevCreateDepthStencilSurface(void *dev, uint32_t w, uint32_t h, int fmt, int multisample, void **surf) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, int, int, void **))(*(void ***)dev)[0x68 / 4])(dev, w, h,
		fmt, multisample, surf);
}

int Rd_DevCreateImageSurface(void *dev, uint32_t w, uint32_t h, int fmt, void **surf) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, int, void **))(*(void ***)dev)[0x6c / 4])(dev, w, h, fmt,
		surf);
}

int Rd_DevCreateIndexBuffer(void *dev, uint32_t len, uint32_t usage, int fmt, int pool, void **ib) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, int, int, void **))(*(void ***)dev)[0x60 / 4])(dev, len,
		usage, fmt, pool, ib);
}

int Rd_DevCreatePixelShader(void *dev, const void *code, uint32_t *handle) {
	return ((int(__stdcall *)(void *, const void *, uint32_t *))(*(void ***)dev)[0x15c / 4])(dev, code, handle);
}

int Rd_DevCreateTexture(void *dev, uint32_t w, uint32_t h, uint32_t levels, uint32_t usage, int fmt, int pool,
	void **tex) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, uint32_t, uint32_t, int, int,
		void **))(*(void ***)dev)[0x50 / 4])(dev, w, h, levels, usage, fmt, pool, tex);
}

int Rd_DevCreateVertexBuffer(void *dev, uint32_t len, uint32_t usage, uint32_t fvf, int pool, void **vb) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, uint32_t, int, void **))(*(void ***)dev)[0x5c / 4])(dev,
		len, usage, fvf, pool, vb);
}

int Rd_DevDeletePixelShader(void *dev, uint32_t handle) {
	return ((int(__stdcall *)(void *, uint32_t))(*(void ***)dev)[0x168 / 4])(dev, handle);
}

int Rd_DevDrawIndexedPrimitive(void *dev, int type, uint32_t minIndex, uint32_t numVertices, uint32_t startIndex,
	uint32_t primCount) {
	return ((int(__stdcall *)(void *, int, uint32_t, uint32_t, uint32_t, uint32_t))(*(void ***)dev)[0x11c / 4])(dev,
		type, minIndex, numVertices, startIndex, primCount);
}

int Rd_DevDrawIndexedPrimitiveUP(void *dev, int type, uint32_t minIndex, uint32_t numVertices, uint32_t primCount,
	const void *idx, int idxFmt, const void *data, uint32_t stride) {
	return ((int(__stdcall *)(void *, int, uint32_t, uint32_t, uint32_t, const void *, int, const void *,
		uint32_t))(*(void ***)dev)[0x124 / 4])(dev, type, minIndex, numVertices, primCount, idx, idxFmt, data,
		stride);
}

int Rd_DevDrawPrimitive(void *dev, int type, uint32_t start, uint32_t count) {
	return ((int(__stdcall *)(void *, int, uint32_t, uint32_t))(*(void ***)dev)[0x118 / 4])(dev, type, start,
		count);
}

int Rd_DevDrawPrimitiveUP(void *dev, int type, uint32_t count, const void *data, uint32_t stride) {
	return ((int(__stdcall *)(void *, int, uint32_t, const void *, uint32_t))(*(void ***)dev)[0x120 / 4])(dev, type,
		count, data, stride);
}

int Rd_DevEndScene(void *dev) {
	return ((int(__stdcall *)(void *))(*(void ***)dev)[0x8c / 4])(dev);
}

uint32_t Rd_DevGetAvailableTextureMem(void *dev) {
	return ((uint32_t(__stdcall *)(void *))(*(void ***)dev)[0x10 / 4])(dev);
}

int Rd_DevGetBackBuffer(void *dev, uint32_t i, int type, void **surf) {
	return ((int(__stdcall *)(void *, uint32_t, int, void **))(*(void ***)dev)[0x40 / 4])(dev, i, type, surf);
}

int Rd_DevGetDepthStencilSurface(void *dev, void **surf) {
	return ((int(__stdcall *)(void *, void **))(*(void ***)dev)[0x84 / 4])(dev, surf);
}

int Rd_DevGetDeviceCaps(void *dev, void *caps) {
	return ((int(__stdcall *)(void *, void *))(*(void ***)dev)[0x1c / 4])(dev, caps);
}

int Rd_DevGetDirect3D(void *dev, void **d3d) {
	return ((int(__stdcall *)(void *, void **))(*(void ***)dev)[0x18 / 4])(dev, d3d);
}

int Rd_DevGetDisplayMode(void *dev, void *mode) {
	return ((int(__stdcall *)(void *, void *))(*(void ***)dev)[0x20 / 4])(dev, mode);
}

int Rd_DevGetRenderState(void *dev, uint32_t state, uint32_t *value) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t *))(*(void ***)dev)[0xcc / 4])(dev, state, value);
}

int Rd_DevGetRenderTarget(void *dev, void **surf) {
	return ((int(__stdcall *)(void *, void **))(*(void ***)dev)[0x80 / 4])(dev, surf);
}

int Rd_DevGetTextureStageState(void *dev, uint32_t stage, uint32_t type, uint32_t *value) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, uint32_t *))(*(void ***)dev)[0xf8 / 4])(dev, stage, type,
		value);
}

int Rd_DevGetTransform(void *dev, uint32_t state, void *m) {
	return ((int(__stdcall *)(void *, uint32_t, void *))(*(void ***)dev)[0x98 / 4])(dev, state, m);
}

int Rd_DevGetViewport(void *dev, uint32_t *vp) {
	return ((int(__stdcall *)(void *, uint32_t *))(*(void ***)dev)[0xa4 / 4])(dev, vp);
}

int Rd_DevLightEnable(void *dev, uint32_t idx, int enable) {
	return ((int(__stdcall *)(void *, uint32_t, int))(*(void ***)dev)[0xb8 / 4])(dev, idx, enable);
}

int Rd_DevPresent(void *dev, const void *src, const void *dst, void *hwnd, const void *dirty) {
	return ((int(__stdcall *)(void *, const void *, const void *, void *,
		const void *))(*(void ***)dev)[0x3c / 4])(dev, src, dst, hwnd, dirty);
}

int Rd_DevReset(void *dev, void *pp) {
	return ((int(__stdcall *)(void *, void *))(*(void ***)dev)[0x38 / 4])(dev, pp);
}

void Rd_DevSetCursorPosition(void *dev, int x, int y, uint32_t flags) {
	((void(__stdcall *)(void *, int, int, uint32_t))(*(void ***)dev)[0x2c / 4])(dev, x, y, flags);
}

int Rd_DevSetGammaRamp(void *dev, uint32_t flags, const void *ramp) {
	return ((int(__stdcall *)(void *, uint32_t, const void *))(*(void ***)dev)[0x48 / 4])(dev, flags, ramp);
}

int Rd_DevSetIndices(void *dev, void *ib, uint32_t baseVertex) {
	return ((int(__stdcall *)(void *, void *, uint32_t))(*(void ***)dev)[0x154 / 4])(dev, ib, baseVertex);
}

int Rd_DevSetLight(void *dev, uint32_t idx, const void *l) {
	return ((int(__stdcall *)(void *, uint32_t, const void *))(*(void ***)dev)[0xb0 / 4])(dev, idx, l);
}

int Rd_DevSetMaterial(void *dev, const void *mat) {
	return ((int(__stdcall *)(void *, const void *))(*(void ***)dev)[0xa8 / 4])(dev, mat);
}

int Rd_DevSetPixelShader(void *dev, uint32_t handle) {
	return ((int(__stdcall *)(void *, uint32_t))(*(void ***)dev)[0x160 / 4])(dev, handle);
}

int Rd_DevSetPixelShaderConstant(void *dev, uint32_t reg, const void *data, uint32_t count) {
	return ((int(__stdcall *)(void *, uint32_t, const void *, uint32_t))(*(void ***)dev)[0x16c / 4])(dev, reg, data,
		count);
}

int Rd_DevSetRenderState(void *dev, uint32_t state, uint32_t value) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t))(*(void ***)dev)[0xc8 / 4])(dev, state, value);
}

int Rd_DevSetRenderTarget(void *dev, void *rt, void *zs) {
	return ((int(__stdcall *)(void *, void *, void *))(*(void ***)dev)[0x7c / 4])(dev, rt, zs);
}

int Rd_DevSetStreamSource(void *dev, uint32_t stream, void *vb, uint32_t stride) {
	return ((int(__stdcall *)(void *, uint32_t, void *, uint32_t))(*(void ***)dev)[0x14c / 4])(dev, stream, vb,
		stride);
}

int Rd_DevSetTexture(void *dev, uint32_t stage, void *tex) {
	return ((int(__stdcall *)(void *, uint32_t, void *))(*(void ***)dev)[0xf4 / 4])(dev, stage, tex);
}

int Rd_DevSetTextureStageState(void *dev, uint32_t stage, uint32_t type, uint32_t value) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, uint32_t))(*(void ***)dev)[0xfc / 4])(dev, stage, type,
		value);
}

int Rd_DevSetTransform(void *dev, uint32_t state, const void *m) {
	return ((int(__stdcall *)(void *, uint32_t, const void *))(*(void ***)dev)[0x94 / 4])(dev, state, m);
}

int Rd_DevSetVertexShader(void *dev, uint32_t fvf) {
	return ((int(__stdcall *)(void *, uint32_t))(*(void ***)dev)[0x130 / 4])(dev, fvf);
}

int Rd_DevSetViewport(void *dev, const uint32_t *vp) {
	return ((int(__stdcall *)(void *, const uint32_t *))(*(void ***)dev)[0xa0 / 4])(dev, vp);
}

int Rd_DevShowCursor(void *dev, int show) {
	return ((int(__stdcall *)(void *, int))(*(void ***)dev)[0x30 / 4])(dev, show);
}

int Rd_DevTestCooperativeLevel(void *dev) {
	return ((int(__stdcall *)(void *))(*(void ***)dev)[0xc / 4])(dev);
}

int Rd_DevUpdateTexture(void *dev, void *src, void *dst) {
	return ((int(__stdcall *)(void *, void *, void *))(*(void ***)dev)[0x74 / 4])(dev, src, dst);
}

int Rd_TexGetDevice(void *tex, void **dev) {
	return ((int(__stdcall *)(void *, void **))(*(void ***)tex)[0xc / 4])(tex, dev);
}

uint32_t Rd_TexGetLevelCount(void *tex) {
	return ((uint32_t(__stdcall *)(void *))(*(void ***)tex)[0x34 / 4])(tex);
}

int Rd_TexGetLevelDesc(void *tex, uint32_t level, void *desc) {
	return ((int(__stdcall *)(void *, uint32_t, void *))(*(void ***)tex)[0x38 / 4])(tex, level, desc);
}

int Rd_TexGetSurfaceLevel(void *tex, uint32_t level, void **surf) {
	return ((int(__stdcall *)(void *, uint32_t, void **))(*(void ***)tex)[0x3c / 4])(tex, level, surf);
}

int Rd_TexLockRect(void *tex, uint32_t level, void *locked, const void *rect, uint32_t flags) {
	return ((int(__stdcall *)(void *, uint32_t, void *, const void *, uint32_t))(*(void ***)tex)[0x40 / 4])(tex,
		level, locked, rect, flags);
}

uint32_t Rd_TexSetLOD(void *tex, uint32_t lod) {
	return ((uint32_t(__stdcall *)(void *, uint32_t))(*(void ***)tex)[0x2c / 4])(tex, lod);
}

int Rd_TexUnlockRect(void *tex, uint32_t level) {
	return ((int(__stdcall *)(void *, uint32_t))(*(void ***)tex)[0x44 / 4])(tex, level);
}

int Rd_SurfGetDesc(void *surf, void *desc) {
	return ((int(__stdcall *)(void *, void *))(*(void ***)surf)[0x20 / 4])(surf, desc);
}

int Rd_SurfGetDevice(void *surf, void **dev) {
	return ((int(__stdcall *)(void *, void **))(*(void ***)surf)[0xc / 4])(surf, dev);
}

int Rd_SurfLockRect(void *surf, void *lr, const void *rect, uint32_t flags) {
	return ((int(__stdcall *)(void *, void *, const void *, uint32_t))(*(void ***)surf)[0x24 / 4])(surf, lr, rect,
		flags);
}

int Rd_SurfUnlockRect(void *surf) {
	return ((int(__stdcall *)(void *))(*(void ***)surf)[0x28 / 4])(surf);
}

int Rd_BufLock(void *buf, uint32_t off, uint32_t size, void *data, uint32_t flags) {
	return ((int(__stdcall *)(void *, uint32_t, uint32_t, void *, uint32_t))(*(void ***)buf)[0x2c / 4])(buf, off,
		size, data, flags);
}

int Rd_BufUnlock(void *buf) {
	return ((int(__stdcall *)(void *))(*(void ***)buf)[0x30 / 4])(buf);
}

void *Rd_XBufGetBufferPointer(void *buf) {
	return ((void *(__stdcall *)(void *))(*(void ***)buf)[0xc / 4])(buf);
}

int Rd_XMeshDrawSubset(void *mesh, uint32_t attrib) {
	return ((int(__stdcall *)(void *, uint32_t))(*(void ***)mesh)[0xc / 4])(mesh, attrib);
}

int Rd_XMeshGetAttributeTable(void *mesh, void *table, uint32_t *size) {
	return ((int(__stdcall *)(void *, void *, uint32_t *))(*(void ***)mesh)[0x48 / 4])(mesh, table, size);
}

uint32_t Rd_XMeshGetNumFaces(void *mesh) {
	return ((uint32_t(__stdcall *)(void *))(*(void ***)mesh)[0x10 / 4])(mesh);
}

uint32_t Rd_XMeshGetNumVertices(void *mesh) {
	return ((uint32_t(__stdcall *)(void *))(*(void ***)mesh)[0x14 / 4])(mesh);
}
