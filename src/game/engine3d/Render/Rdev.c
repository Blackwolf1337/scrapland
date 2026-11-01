/*
 * The public functions of the render device (include/scrap/rdev.h): a device call on the logged device is written to
 * the D3D log first (RdevLog.c), then every call goes to the backend.
 */
#include <stddef.h>

#include "rdevbackend.h"
#include "scrap/rdev.h"

const RdBackend *rd = &rd_d3d8;

uint32_t Rd_AddRef(void *obj) {
	return rd->AddRef(obj);
}

uint32_t Rd_Release(void *obj) {
	return rd->Release(obj);
}

int Rd_D3DCheckDepthStencilMatch(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t rt, uint32_t ds) {
	return rd->D3DCheckDepthStencilMatch(d3d, adapter, type, fmt, rt, ds);
}

int Rd_D3DCheckDeviceFormat(void *d3d, uint32_t adapter, int devType, int adapterFmt, uint32_t usage, int rtype,
	int fmt) {
	return rd->D3DCheckDeviceFormat(d3d, adapter, devType, adapterFmt, usage, rtype, fmt);
}

int Rd_D3DCheckDeviceMultiSampleType(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, int windowed,
	uint32_t ms) {
	return rd->D3DCheckDeviceMultiSampleType(d3d, adapter, type, fmt, windowed, ms);
}

int Rd_D3DCheckDeviceType(void *d3d, uint32_t adapter, uint32_t type, uint32_t dfmt, uint32_t bfmt, int windowed) {
	return rd->D3DCheckDeviceType(d3d, adapter, type, dfmt, bfmt, windowed);
}

int Rd_D3DCreateDevice(void *d3d, uint32_t adapter, uint32_t type, void *hwnd, uint32_t behavior, void *pp,
	void **dev) {
	return rd->D3DCreateDevice(d3d, adapter, type, hwnd, behavior, pp, dev);
}

int Rd_D3DEnumAdapterModes(void *d3d, uint32_t adapter, uint32_t mode, void *out) {
	return rd->D3DEnumAdapterModes(d3d, adapter, mode, out);
}

uint32_t Rd_D3DGetAdapterCount(void *d3d) {
	return rd->D3DGetAdapterCount(d3d);
}

int Rd_D3DGetAdapterDisplayMode(void *d3d, uint32_t adapter, void *out) {
	return rd->D3DGetAdapterDisplayMode(d3d, adapter, out);
}

int Rd_D3DGetAdapterIdentifier(void *d3d, uint32_t adapter, uint32_t flags, void *id) {
	return rd->D3DGetAdapterIdentifier(d3d, adapter, flags, id);
}

uint32_t Rd_D3DGetAdapterModeCount(void *d3d, uint32_t adapter) {
	return rd->D3DGetAdapterModeCount(d3d, adapter);
}

int Rd_D3DGetDeviceCaps(void *d3d, uint32_t adapter, uint32_t type, void *caps) {
	return rd->D3DGetDeviceCaps(d3d, adapter, type, caps);
}

int Rd_DevBeginScene(void *dev) {
	if (rd_logDevice && dev == rd_logDevice) {
		RdLog_Call(0x88 / 4, NULL);
		dev = rd_logReal;
	}
	return rd->DevBeginScene(dev);
}

int Rd_DevClear(void *dev, uint32_t count, const void *rects, uint32_t flags, uint32_t color, float z,
	uint32_t stencil) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)count, (uint32_t)(uintptr_t)rects, (uint32_t)flags, (uint32_t)color,
			Rd_FloatBits(z), (uint32_t)stencil };
		RdLog_Call(0x90 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevClear(dev, count, rects, flags, color, z, stencil);
}

int Rd_DevCopyRects(void *dev, void *src, const void *rects, uint32_t n, void *dst, const void *points) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)(uintptr_t)src, (uint32_t)(uintptr_t)rects, (uint32_t)n,
			(uint32_t)(uintptr_t)dst, (uint32_t)(uintptr_t)points };
		RdLog_Call(0x70 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevCopyRects(dev, src, rects, n, dst, points);
}

int Rd_DevCreateDepthStencilSurface(void *dev, uint32_t w, uint32_t h, int fmt, int multisample, void **surf) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)w, (uint32_t)h, (uint32_t)fmt, (uint32_t)multisample,
			(uint32_t)(uintptr_t)surf };
		RdLog_Call(0x68 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevCreateDepthStencilSurface(dev, w, h, fmt, multisample, surf);
}

int Rd_DevCreateImageSurface(void *dev, uint32_t w, uint32_t h, int fmt, void **surf) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)w, (uint32_t)h, (uint32_t)fmt, (uint32_t)(uintptr_t)surf };
		RdLog_Call(0x6c / 4, a);
		dev = rd_logReal;
	}
	return rd->DevCreateImageSurface(dev, w, h, fmt, surf);
}

int Rd_DevCreateIndexBuffer(void *dev, uint32_t len, uint32_t usage, int fmt, int pool, void **ib) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)len, (uint32_t)usage, (uint32_t)fmt, (uint32_t)pool,
			(uint32_t)(uintptr_t)ib };
		RdLog_Call(0x60 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevCreateIndexBuffer(dev, len, usage, fmt, pool, ib);
}

int Rd_DevCreatePixelShader(void *dev, const void *code, uint32_t *handle) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)(uintptr_t)code, (uint32_t)(uintptr_t)handle };
		RdLog_Call(0x15c / 4, a);
		dev = rd_logReal;
	}
	return rd->DevCreatePixelShader(dev, code, handle);
}

int Rd_DevCreateTexture(void *dev, uint32_t w, uint32_t h, uint32_t levels, uint32_t usage, int fmt, int pool,
	void **tex) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)w, (uint32_t)h, (uint32_t)levels, (uint32_t)usage, (uint32_t)fmt,
			(uint32_t)pool, (uint32_t)(uintptr_t)tex };
		RdLog_Call(0x50 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevCreateTexture(dev, w, h, levels, usage, fmt, pool, tex);
}

int Rd_DevCreateVertexBuffer(void *dev, uint32_t len, uint32_t usage, uint32_t fvf, int pool, void **vb) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)len, (uint32_t)usage, (uint32_t)fvf, (uint32_t)pool,
			(uint32_t)(uintptr_t)vb };
		RdLog_Call(0x5c / 4, a);
		dev = rd_logReal;
	}
	return rd->DevCreateVertexBuffer(dev, len, usage, fvf, pool, vb);
}

int Rd_DevDeletePixelShader(void *dev, uint32_t handle) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)handle };
		RdLog_Call(0x168 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevDeletePixelShader(dev, handle);
}

int Rd_DevDrawIndexedPrimitive(void *dev, int type, uint32_t minIndex, uint32_t numVertices, uint32_t startIndex,
	uint32_t primCount) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)type, (uint32_t)minIndex, (uint32_t)numVertices, (uint32_t)startIndex,
			(uint32_t)primCount };
		RdLog_Call(0x11c / 4, a);
		dev = rd_logReal;
	}
	return rd->DevDrawIndexedPrimitive(dev, type, minIndex, numVertices, startIndex, primCount);
}

int Rd_DevDrawIndexedPrimitiveUP(void *dev, int type, uint32_t minIndex, uint32_t numVertices, uint32_t primCount,
	const void *idx, int idxFmt, const void *data, uint32_t stride) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)type, (uint32_t)minIndex, (uint32_t)numVertices, (uint32_t)primCount,
			(uint32_t)(uintptr_t)idx, (uint32_t)idxFmt, (uint32_t)(uintptr_t)data, (uint32_t)stride };
		RdLog_Call(0x124 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevDrawIndexedPrimitiveUP(dev, type, minIndex, numVertices, primCount, idx, idxFmt, data, stride);
}

int Rd_DevDrawPrimitive(void *dev, int type, uint32_t start, uint32_t count) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)type, (uint32_t)start, (uint32_t)count };
		RdLog_Call(0x118 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevDrawPrimitive(dev, type, start, count);
}

int Rd_DevDrawPrimitiveUP(void *dev, int type, uint32_t count, const void *data, uint32_t stride) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)type, (uint32_t)count, (uint32_t)(uintptr_t)data, (uint32_t)stride };
		RdLog_Call(0x120 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevDrawPrimitiveUP(dev, type, count, data, stride);
}

int Rd_DevEndScene(void *dev) {
	if (rd_logDevice && dev == rd_logDevice) {
		RdLog_Call(0x8c / 4, NULL);
		dev = rd_logReal;
	}
	return rd->DevEndScene(dev);
}

uint32_t Rd_DevGetAvailableTextureMem(void *dev) {
	if (rd_logDevice && dev == rd_logDevice) {
		RdLog_Call(0x10 / 4, NULL);
		dev = rd_logReal;
	}
	return rd->DevGetAvailableTextureMem(dev);
}

int Rd_DevGetBackBuffer(void *dev, uint32_t i, int type, void **surf) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)i, (uint32_t)type, (uint32_t)(uintptr_t)surf };
		RdLog_Call(0x40 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevGetBackBuffer(dev, i, type, surf);
}

int Rd_DevGetDepthStencilSurface(void *dev, void **surf) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)(uintptr_t)surf };
		RdLog_Call(0x84 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevGetDepthStencilSurface(dev, surf);
}

int Rd_DevGetDeviceCaps(void *dev, void *caps) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)(uintptr_t)caps };
		RdLog_Call(0x1c / 4, a);
		dev = rd_logReal;
	}
	return rd->DevGetDeviceCaps(dev, caps);
}

int Rd_DevGetDirect3D(void *dev, void **d3d) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)(uintptr_t)d3d };
		RdLog_Call(0x18 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevGetDirect3D(dev, d3d);
}

int Rd_DevGetDisplayMode(void *dev, void *mode) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)(uintptr_t)mode };
		RdLog_Call(0x20 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevGetDisplayMode(dev, mode);
}

int Rd_DevGetRenderState(void *dev, uint32_t state, uint32_t *value) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)state, (uint32_t)(uintptr_t)value };
		RdLog_Call(0xcc / 4, a);
		dev = rd_logReal;
	}
	return rd->DevGetRenderState(dev, state, value);
}

int Rd_DevGetRenderTarget(void *dev, void **surf) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)(uintptr_t)surf };
		RdLog_Call(0x80 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevGetRenderTarget(dev, surf);
}

int Rd_DevGetTextureStageState(void *dev, uint32_t stage, uint32_t type, uint32_t *value) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)stage, (uint32_t)type, (uint32_t)(uintptr_t)value };
		RdLog_Call(0xf8 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevGetTextureStageState(dev, stage, type, value);
}

int Rd_DevGetTransform(void *dev, uint32_t state, void *m) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)state, (uint32_t)(uintptr_t)m };
		RdLog_Call(0x98 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevGetTransform(dev, state, m);
}

int Rd_DevGetViewport(void *dev, uint32_t *vp) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)(uintptr_t)vp };
		RdLog_Call(0xa4 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevGetViewport(dev, vp);
}

int Rd_DevLightEnable(void *dev, uint32_t idx, int enable) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)idx, (uint32_t)enable };
		RdLog_Call(0xb8 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevLightEnable(dev, idx, enable);
}

int Rd_DevPresent(void *dev, const void *src, const void *dst, void *hwnd, const void *dirty) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)(uintptr_t)src, (uint32_t)(uintptr_t)dst, (uint32_t)(uintptr_t)hwnd,
			(uint32_t)(uintptr_t)dirty };
		RdLog_Call(0x3c / 4, a);
		dev = rd_logReal;
	}
	return rd->DevPresent(dev, src, dst, hwnd, dirty);
}

int Rd_DevReset(void *dev, void *pp) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)(uintptr_t)pp };
		RdLog_Call(0x38 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevReset(dev, pp);
}

void Rd_DevSetCursorPosition(void *dev, int x, int y, uint32_t flags) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)x, (uint32_t)y, (uint32_t)flags };
		RdLog_Call(0x2c / 4, a);
		dev = rd_logReal;
	}
	rd->DevSetCursorPosition(dev, x, y, flags);
}

int Rd_DevSetGammaRamp(void *dev, uint32_t flags, const void *ramp) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)flags, (uint32_t)(uintptr_t)ramp };
		RdLog_Call(0x48 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevSetGammaRamp(dev, flags, ramp);
}

int Rd_DevSetIndices(void *dev, void *ib, uint32_t baseVertex) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)(uintptr_t)ib, (uint32_t)baseVertex };
		RdLog_Call(0x154 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevSetIndices(dev, ib, baseVertex);
}

int Rd_DevSetLight(void *dev, uint32_t idx, const void *l) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)idx, (uint32_t)(uintptr_t)l };
		RdLog_Call(0xb0 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevSetLight(dev, idx, l);
}

int Rd_DevSetMaterial(void *dev, const void *mat) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)(uintptr_t)mat };
		RdLog_Call(0xa8 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevSetMaterial(dev, mat);
}

int Rd_DevSetPixelShader(void *dev, uint32_t handle) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)handle };
		RdLog_Call(0x160 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevSetPixelShader(dev, handle);
}

int Rd_DevSetPixelShaderConstant(void *dev, uint32_t reg, const void *data, uint32_t count) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)reg, (uint32_t)(uintptr_t)data, (uint32_t)count };
		RdLog_Call(0x16c / 4, a);
		dev = rd_logReal;
	}
	return rd->DevSetPixelShaderConstant(dev, reg, data, count);
}

int Rd_DevSetRenderState(void *dev, uint32_t state, uint32_t value) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)state, (uint32_t)value };
		RdLog_Call(0xc8 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevSetRenderState(dev, state, value);
}

int Rd_DevSetRenderTarget(void *dev, void *rt, void *zs) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)(uintptr_t)rt, (uint32_t)(uintptr_t)zs };
		RdLog_Call(0x7c / 4, a);
		dev = rd_logReal;
	}
	return rd->DevSetRenderTarget(dev, rt, zs);
}

int Rd_DevSetStreamSource(void *dev, uint32_t stream, void *vb, uint32_t stride) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)stream, (uint32_t)(uintptr_t)vb, (uint32_t)stride };
		RdLog_Call(0x14c / 4, a);
		dev = rd_logReal;
	}
	return rd->DevSetStreamSource(dev, stream, vb, stride);
}

int Rd_DevSetTexture(void *dev, uint32_t stage, void *tex) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)stage, (uint32_t)(uintptr_t)tex };
		RdLog_Call(0xf4 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevSetTexture(dev, stage, tex);
}

int Rd_DevSetTextureStageState(void *dev, uint32_t stage, uint32_t type, uint32_t value) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)stage, (uint32_t)type, (uint32_t)value };
		RdLog_Call(0xfc / 4, a);
		dev = rd_logReal;
	}
	return rd->DevSetTextureStageState(dev, stage, type, value);
}

int Rd_DevSetTransform(void *dev, uint32_t state, const void *m) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)state, (uint32_t)(uintptr_t)m };
		RdLog_Call(0x94 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevSetTransform(dev, state, m);
}

int Rd_DevSetVertexShader(void *dev, uint32_t fvf) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)fvf };
		RdLog_Call(0x130 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevSetVertexShader(dev, fvf);
}

int Rd_DevSetViewport(void *dev, const uint32_t *vp) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)(uintptr_t)vp };
		RdLog_Call(0xa0 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevSetViewport(dev, vp);
}

int Rd_DevShowCursor(void *dev, int show) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)show };
		RdLog_Call(0x30 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevShowCursor(dev, show);
}

int Rd_DevTestCooperativeLevel(void *dev) {
	if (rd_logDevice && dev == rd_logDevice) {
		RdLog_Call(0xc / 4, NULL);
		dev = rd_logReal;
	}
	return rd->DevTestCooperativeLevel(dev);
}

int Rd_DevUpdateTexture(void *dev, void *src, void *dst) {
	if (rd_logDevice && dev == rd_logDevice) {
		const uint32_t a[] = { (uint32_t)(uintptr_t)src, (uint32_t)(uintptr_t)dst };
		RdLog_Call(0x74 / 4, a);
		dev = rd_logReal;
	}
	return rd->DevUpdateTexture(dev, src, dst);
}

int Rd_TexGetDevice(void *tex, void **dev) {
	return rd->TexGetDevice(tex, dev);
}

uint32_t Rd_TexGetLevelCount(void *tex) {
	return rd->TexGetLevelCount(tex);
}

int Rd_TexGetLevelDesc(void *tex, uint32_t level, void *desc) {
	return rd->TexGetLevelDesc(tex, level, desc);
}

int Rd_TexGetSurfaceLevel(void *tex, uint32_t level, void **surf) {
	return rd->TexGetSurfaceLevel(tex, level, surf);
}

int Rd_TexLockRect(void *tex, uint32_t level, void *locked, const void *rect, uint32_t flags) {
	return rd->TexLockRect(tex, level, locked, rect, flags);
}

uint32_t Rd_TexSetLOD(void *tex, uint32_t lod) {
	return rd->TexSetLOD(tex, lod);
}

int Rd_TexUnlockRect(void *tex, uint32_t level) {
	return rd->TexUnlockRect(tex, level);
}

int Rd_SurfGetDesc(void *surf, void *desc) {
	return rd->SurfGetDesc(surf, desc);
}

int Rd_SurfGetDevice(void *surf, void **dev) {
	return rd->SurfGetDevice(surf, dev);
}

int Rd_SurfLockRect(void *surf, void *lr, const void *rect, uint32_t flags) {
	return rd->SurfLockRect(surf, lr, rect, flags);
}

int Rd_SurfUnlockRect(void *surf) {
	return rd->SurfUnlockRect(surf);
}

int Rd_BufLock(void *buf, uint32_t off, uint32_t size, void *data, uint32_t flags) {
	return rd->BufLock(buf, off, size, data, flags);
}

int Rd_BufUnlock(void *buf) {
	return rd->BufUnlock(buf);
}

void *Rd_XBufGetBufferPointer(void *buf) {
	return rd->XBufGetBufferPointer(buf);
}

int Rd_XMeshDrawSubset(void *mesh, uint32_t attrib) {
	return rd->XMeshDrawSubset(mesh, attrib);
}

int Rd_XMeshGetAttributeTable(void *mesh, void *table, uint32_t *size) {
	return rd->XMeshGetAttributeTable(mesh, table, size);
}

uint32_t Rd_XMeshGetNumFaces(void *mesh) {
	return rd->XMeshGetNumFaces(mesh);
}

uint32_t Rd_XMeshGetNumVertices(void *mesh) {
	return rd->XMeshGetNumVertices(mesh);
}
