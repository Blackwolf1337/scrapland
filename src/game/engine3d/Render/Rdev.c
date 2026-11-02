/*
 * The public functions of the render device (include/scrap/rdev.h): every call goes to the Vulkan backend
 * (vk/RdevVk.c).
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdevbackend.h"
#include "scrap/rdev.h"

void *Rd_Direct3DCreate8(uint32_t sdk) {
	return Vk_Direct3DCreate8(sdk);
}

uint32_t Rd_AddRef(void *obj) {
	return Vk_AddRef(obj);
}

uint32_t Rd_Release(void *obj) {
	return Vk_Release(obj);
}

int Rd_D3DCheckDepthStencilMatch(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t rt, uint32_t ds) {
	return Vk_D3DCheckDepthStencilMatch(d3d, adapter, type, fmt, rt, ds);
}

int Rd_D3DCheckDeviceFormat(void *d3d, uint32_t adapter, int devType, int adapterFmt, uint32_t usage, int rtype,
	int fmt) {
	return Vk_D3DCheckDeviceFormat(d3d, adapter, devType, adapterFmt, usage, rtype, fmt);
}

int Rd_D3DCheckDeviceMultiSampleType(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, int windowed,
	uint32_t ms) {
	return Vk_D3DCheckDeviceMultiSampleType(d3d, adapter, type, fmt, windowed, ms);
}

int Rd_D3DCheckDeviceType(void *d3d, uint32_t adapter, uint32_t type, uint32_t dfmt, uint32_t bfmt, int windowed) {
	return Vk_D3DCheckDeviceType(d3d, adapter, type, dfmt, bfmt, windowed);
}

int Rd_D3DCreateDevice(void *d3d, uint32_t adapter, uint32_t type, void *hwnd, uint32_t behavior, void *pp,
	void **dev) {
	return Vk_D3DCreateDevice(d3d, adapter, type, hwnd, behavior, pp, dev);
}

int Rd_D3DEnumAdapterModes(void *d3d, uint32_t adapter, uint32_t mode, void *out) {
	return Vk_D3DEnumAdapterModes(d3d, adapter, mode, out);
}

uint32_t Rd_D3DGetAdapterCount(void *d3d) {
	return Vk_D3DGetAdapterCount(d3d);
}

int Rd_D3DGetAdapterDisplayMode(void *d3d, uint32_t adapter, void *out) {
	return Vk_D3DGetAdapterDisplayMode(d3d, adapter, out);
}

int Rd_D3DGetAdapterIdentifier(void *d3d, uint32_t adapter, uint32_t flags, void *id) {
	return Vk_D3DGetAdapterIdentifier(d3d, adapter, flags, id);
}

uint32_t Rd_D3DGetAdapterModeCount(void *d3d, uint32_t adapter) {
	return Vk_D3DGetAdapterModeCount(d3d, adapter);
}

int Rd_D3DGetDeviceCaps(void *d3d, uint32_t adapter, uint32_t type, void *caps) {
	return Vk_D3DGetDeviceCaps(d3d, adapter, type, caps);
}

int Rd_DevBeginScene(void *dev) {
	return Vk_DevBeginScene(dev);
}

int Rd_DevClear(void *dev, uint32_t count, const void *rects, uint32_t flags, uint32_t color, float z,
	uint32_t stencil) {
	return Vk_DevClear(dev, count, rects, flags, color, z, stencil);
}

int Rd_DevCopyRects(void *dev, void *src, const void *rects, uint32_t n, void *dst, const void *points) {
	return Vk_DevCopyRects(dev, src, rects, n, dst, points);
}

int Rd_DevCreateDepthStencilSurface(void *dev, uint32_t w, uint32_t h, int fmt, int multisample, void **surf) {
	return Vk_DevCreateDepthStencilSurface(dev, w, h, fmt, multisample, surf);
}

int Rd_DevCreateImageSurface(void *dev, uint32_t w, uint32_t h, int fmt, void **surf) {
	return Vk_DevCreateImageSurface(dev, w, h, fmt, surf);
}

int Rd_DevCreateIndexBuffer(void *dev, uint32_t len, uint32_t usage, int fmt, int pool, void **ib) {
	return Vk_DevCreateIndexBuffer(dev, len, usage, fmt, pool, ib);
}

int Rd_DevCreatePixelShader(void *dev, const char *name, uint32_t *handle) {
	return Vk_DevCreatePixelShader(dev, name, handle);
}

int Rd_DevCreateTexture(void *dev, uint32_t w, uint32_t h, uint32_t levels, uint32_t usage, int fmt, int pool,
	void **tex) {
	return Vk_DevCreateTexture(dev, w, h, levels, usage, fmt, pool, tex);
}

int Rd_DevCreateVertexBuffer(void *dev, uint32_t len, uint32_t usage, uint32_t fvf, int pool, void **vb) {
	return Vk_DevCreateVertexBuffer(dev, len, usage, fvf, pool, vb);
}

int Rd_DevDeletePixelShader(void *dev, uint32_t handle) {
	return Vk_DevDeletePixelShader(dev, handle);
}

int Rd_DevDrawIndexedPrimitive(void *dev, int type, uint32_t minIndex, uint32_t numVertices, uint32_t startIndex,
	uint32_t primCount) {
	return Vk_DevDrawIndexedPrimitive(dev, type, minIndex, numVertices, startIndex, primCount);
}

int Rd_DevDrawIndexedPrimitiveUP(void *dev, int type, uint32_t minIndex, uint32_t numVertices, uint32_t primCount,
	const void *idx, int idxFmt, const void *data, uint32_t stride) {
	return Vk_DevDrawIndexedPrimitiveUP(dev, type, minIndex, numVertices, primCount, idx, idxFmt, data, stride);
}

int Rd_DevDrawPrimitive(void *dev, int type, uint32_t start, uint32_t count) {
	return Vk_DevDrawPrimitive(dev, type, start, count);
}

int Rd_DevDrawPrimitiveUP(void *dev, int type, uint32_t count, const void *data, uint32_t stride) {
	return Vk_DevDrawPrimitiveUP(dev, type, count, data, stride);
}

int Rd_DevEndScene(void *dev) {
	return Vk_DevEndScene(dev);
}

uint32_t Rd_DevGetAvailableTextureMem(void *dev) {
	return Vk_DevGetAvailableTextureMem(dev);
}

int Rd_DevGetBackBuffer(void *dev, uint32_t i, int type, void **surf) {
	return Vk_DevGetBackBuffer(dev, i, type, surf);
}

int Rd_DevGetDepthStencilSurface(void *dev, void **surf) {
	return Vk_DevGetDepthStencilSurface(dev, surf);
}

int Rd_DevGetDeviceCaps(void *dev, void *caps) {
	return Vk_DevGetDeviceCaps(dev, caps);
}

int Rd_DevGetDirect3D(void *dev, void **d3d) {
	return Vk_DevGetDirect3D(dev, d3d);
}

int Rd_DevGetDisplayMode(void *dev, void *mode) {
	return Vk_DevGetDisplayMode(dev, mode);
}

int Rd_DevGetRenderState(void *dev, uint32_t state, uint32_t *value) {
	return Vk_DevGetRenderState(dev, state, value);
}

int Rd_DevGetRenderTarget(void *dev, void **surf) {
	return Vk_DevGetRenderTarget(dev, surf);
}

int Rd_DevGetTextureStageState(void *dev, uint32_t stage, uint32_t type, uint32_t *value) {
	return Vk_DevGetTextureStageState(dev, stage, type, value);
}

int Rd_DevGetTransform(void *dev, uint32_t state, void *m) {
	return Vk_DevGetTransform(dev, state, m);
}

int Rd_DevGetViewport(void *dev, uint32_t *vp) {
	return Vk_DevGetViewport(dev, vp);
}

int Rd_DevLightEnable(void *dev, uint32_t idx, int enable) {
	return Vk_DevLightEnable(dev, idx, enable);
}

/* the back buffer as a 32 bit BMP (bottom up) through a copy in system memory, as the game's screenshots do it */
static void Rd_DumpFrame(void *dev) {
	static uint32_t frame;
	const char *dir = getenv("SCRAP_RD_DUMP");
	const char *every = getenv("SCRAP_RD_DUMP_EVERY");
	uint32_t n = every ? (uint32_t)atoi(every) : 100;
	void *back, *img;
	uint32_t desc[8];
	struct {
		int pitch;
		byte *bits;
	} lr;

	frame++;
	if (!dir || !n || frame % n || Vk_DevGetBackBuffer(dev, 0, 0, &back) < 0) {
		return;
	}
	Vk_SurfGetDesc(back, desc);
	uint32_t fmt = desc[0], w = desc[6], h = desc[7];
	if (Vk_DevCreateImageSurface(dev, w, h, (int)fmt, &img) >= 0) {
		if (Vk_DevCopyRects(dev, back, NULL, 0, img, NULL) >= 0 && Vk_SurfLockRect(img, &lr, NULL, 0x10) >= 0) {
			char path[512];
			snprintf(path, sizeof(path), "%s/%s%05u.bmp", dir, "vulkan", frame);
			FILE *f = fopen(path, "wb");
			if (f) {
				byte hdr[54] = { 'B', 'M' };
				uint32_t v[] = { 54 + w * h * 4, 54, 40, w, h };
				memcpy(hdr + 2, &v[0], 4);
				memcpy(hdr + 10, &v[1], 4);
				memcpy(hdr + 14, &v[2], 4);
				memcpy(hdr + 18, &v[3], 4);
				memcpy(hdr + 22, &v[4], 4);
				hdr[26] = 1;
				hdr[28] = 32;
				fwrite(hdr, 1, 54, f);
				for (uint32_t y = h; y-- > 0;) {
					const byte *row = lr.bits + (size_t)y * (uint32_t)lr.pitch;
					for (uint32_t x = 0; x < w; x++) {
						uint32_t c;
						if (fmt == 23) {	/* R5G6B5 */
							uint32_t p = (uint32_t)(row[2 * x] | row[2 * x + 1] << 8);
							c = (p >> 11) * 255 / 31 << 16 | (p >> 5 & 63) * 255 / 63 << 8 | (p & 31) * 255 / 31;
						} else {
							memcpy(&c, row + 4 * x, 4);
						}
						c |= 0xff000000u;
						fwrite(&c, 4, 1, f);
					}
				}
				fclose(f);
			}
			Vk_SurfUnlockRect(img);
		}
		Vk_Release(img);
	}
	Vk_Release(back);
}

int Rd_DevPresent(void *dev, const void *src, const void *dst, void *hwnd, const void *dirty) {
	Rd_DumpFrame(dev);
	return Vk_DevPresent(dev, src, dst, hwnd, dirty);
}

int Rd_DevReset(void *dev, void *pp) {
	return Vk_DevReset(dev, pp);
}

void Rd_DevSetCursorPosition(void *dev, int x, int y, uint32_t flags) {
	Vk_DevSetCursorPosition(dev, x, y, flags);
}

int Rd_DevSetGammaRamp(void *dev, uint32_t flags, const void *ramp) {
	return Vk_DevSetGammaRamp(dev, flags, ramp);
}

int Rd_DevSetIndices(void *dev, void *ib, uint32_t baseVertex) {
	return Vk_DevSetIndices(dev, ib, baseVertex);
}

int Rd_DevSetLight(void *dev, uint32_t idx, const void *l) {
	return Vk_DevSetLight(dev, idx, l);
}

int Rd_DevSetMaterial(void *dev, const void *mat) {
	return Vk_DevSetMaterial(dev, mat);
}

int Rd_DevSetPixelShader(void *dev, uint32_t handle) {
	return Vk_DevSetPixelShader(dev, handle);
}

int Rd_DevSetPixelShaderConstant(void *dev, uint32_t reg, const void *data, uint32_t count) {
	return Vk_DevSetPixelShaderConstant(dev, reg, data, count);
}

int Rd_DevSetRenderState(void *dev, uint32_t state, uint32_t value) {
	return Vk_DevSetRenderState(dev, state, value);
}

int Rd_DevSetRenderTarget(void *dev, void *rt, void *zs) {
	return Vk_DevSetRenderTarget(dev, rt, zs);
}

int Rd_DevSetStreamSource(void *dev, uint32_t stream, void *vb, uint32_t stride) {
	return Vk_DevSetStreamSource(dev, stream, vb, stride);
}

int Rd_DevSetTexture(void *dev, uint32_t stage, void *tex) {
	return Vk_DevSetTexture(dev, stage, tex);
}

int Rd_DevSetTextureStageState(void *dev, uint32_t stage, uint32_t type, uint32_t value) {
	return Vk_DevSetTextureStageState(dev, stage, type, value);
}

int Rd_DevSetTransform(void *dev, uint32_t state, const void *m) {
	return Vk_DevSetTransform(dev, state, m);
}

int Rd_DevSetVertexShader(void *dev, uint32_t fvf) {
	return Vk_DevSetVertexShader(dev, fvf);
}

int Rd_DevSetViewport(void *dev, const uint32_t *vp) {
	return Vk_DevSetViewport(dev, vp);
}

int Rd_DevShowCursor(void *dev, int show) {
	return Vk_DevShowCursor(dev, show);
}

int Rd_DevTestCooperativeLevel(void *dev) {
	return Vk_DevTestCooperativeLevel(dev);
}

int Rd_DevUpdateTexture(void *dev, void *src, void *dst) {
	return Vk_DevUpdateTexture(dev, src, dst);
}

int Rd_TexGetDevice(void *tex, void **dev) {
	return Vk_TexGetDevice(tex, dev);
}

uint32_t Rd_TexGetLevelCount(void *tex) {
	return Vk_TexGetLevelCount(tex);
}

int Rd_TexGetLevelDesc(void *tex, uint32_t level, void *desc) {
	return Vk_TexGetLevelDesc(tex, level, desc);
}

int Rd_TexGetSurfaceLevel(void *tex, uint32_t level, void **surf) {
	return Vk_TexGetSurfaceLevel(tex, level, surf);
}

int Rd_TexLockRect(void *tex, uint32_t level, void *locked, const void *rect, uint32_t flags) {
	return Vk_TexLockRect(tex, level, locked, rect, flags);
}

uint32_t Rd_TexSetLOD(void *tex, uint32_t lod) {
	return Vk_TexSetLOD(tex, lod);
}

int Rd_TexUnlockRect(void *tex, uint32_t level) {
	return Vk_TexUnlockRect(tex, level);
}

int Rd_SurfGetDesc(void *surf, void *desc) {
	return Vk_SurfGetDesc(surf, desc);
}

int Rd_SurfGetDevice(void *surf, void **dev) {
	return Vk_SurfGetDevice(surf, dev);
}

int Rd_SurfLockRect(void *surf, void *lr, const void *rect, uint32_t flags) {
	return Vk_SurfLockRect(surf, lr, rect, flags);
}

int Rd_SurfUnlockRect(void *surf) {
	return Vk_SurfUnlockRect(surf);
}

int Rd_BufLock(void *buf, uint32_t off, uint32_t size, void *data, uint32_t flags) {
	return Vk_BufLock(buf, off, size, data, flags);
}

int Rd_BufUnlock(void *buf) {
	return Vk_BufUnlock(buf);
}


int Rd_XMeshDrawSubset(void *mesh, uint32_t attrib) {
	return Vk_XMeshDrawSubset(mesh, attrib);
}

int Rd_XMeshGetAttributeTable(void *mesh, void *table, uint32_t *size) {
	return Vk_XMeshGetAttributeTable(mesh, table, size);
}

uint32_t Rd_XMeshGetNumFaces(void *mesh) {
	return Vk_XMeshGetNumFaces(mesh);
}

uint32_t Rd_XMeshGetNumVertices(void *mesh) {
	return Vk_XMeshGetNumVertices(mesh);
}
