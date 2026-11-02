/*
 * The screen capture of the engine3d, original source Engine/CaptPant.cpp: a locked surface into the next free
 * ShotNNNN.pcx. The original also wrote the frames into an AVI file (Video for Windows) while the DVF name was set;
 * that recorder is gone.
 */
#include <string.h>

#include "scrap/rdev.h"
#include "scrap/bsp.h"
#include "scrap/crt.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/rendertarget.h"
#include "scrap/scrapmain.h"
#include "scrap/misc.h"
#include "scrap/platform.h"

extern char g_ShotPath[260];	/* MAX_FILEPATH */
extern int g_ShotNum;

/* IDirect3DSurface8 and IDirect3DDevice8 */
#define D3DLOCK_READONLY 0x10
typedef struct CPLockedRect {
	int Pitch;
	byte *pBits;
} CPLockedRect;

/* the doubling of the gamma2x setting, 0 stays 0 */
static byte Shot_Double(byte c) {
	uint32_t v = (uint32_t)c * 2;
	return v >= 0xff ? 0xff : (byte)v;
}

/* the name of the next shot: "Shot*" (or the name set) with the first free number 1..9999 for the '*' */
static void Shot_NextName(void) {
	char buf[MAX_FILEPATH];

	String_Copy(g_ShotPath, g_ScreenshotName, MAX_FILEPATH);
	if (!g_ShotPath[0]) {
		String_Copy(g_ShotPath, "Shot*", MAX_FILEPATH);
	}
	char *star = strchr(g_ShotPath, '*');
	if (!star) {
		Path_Format(g_ShotPath, g_ShotPath, "&U&D&N.pcx");
	} else {
		*star = 0;
		int n = ++g_ShotNum;
		while (n < 10000) {
			Bprintf(buf, sizeof(buf), "%s%04d.pcx", g_ShotPath, n);
			if (crt_access(buf, 0)) {
				String_Copy(g_ShotPath, buf, MAX_FILEPATH);
				n = g_ShotNum;
				break;
			}
			n = ++g_ShotNum;
		}
		if (n == 10000) {
			g_ShotPath[0] = 0;
		}
	}
	if (!g_ShotPath[0]) {
		String_Copy(g_ShotPath, "ScreenShot.pcx", MAX_FILEPATH);
	}
}

byte Surface_SaveShot(void *surf) {
	uint32_t desc[8];
	CPLockedRect lr;
	uint32_t r, g, b;
	int bits;

	if (!surf) {
		return 0;
	}
	Shot_NextName();
	Rd_SurfGetDesc(surf, desc);
	if (Rd_SurfLockRect(surf, &lr, NULL, D3DLOCK_READONLY)) {
		return 0;
	}
	switch (desc[0]) {
	case 0x14:			/* D3DFMT_R8G8B8 */
		r = 0xff0000, g = 0xff00, b = 0xff, bits = 24;
		break;
	case 0x15:			/* D3DFMT_A8R8G8B8, X8R8G8B8 */
	case 0x16:
		r = 0xff0000, g = 0xff00, b = 0xff, bits = 32;
		break;
	case 0x17:			/* D3DFMT_R5G6B5 */
		r = 0xf800, g = 0x7e0, b = 0x1f, bits = 16;
		break;
	case 0x18:			/* D3DFMT_X1R5G5B5, A1R5G5B5 */
	case 0x19:
		r = 0x7c00, g = 0x3e0, b = 0x1f, bits = 16;
		break;
	default:
		E3D_LogBlue(g_FileLog, "capturar_pantalla(): formato no v\xe1lido: %d", desc[0]);
		Rd_SurfUnlockRect(surf);
		return 0;
	}
	Pal16_SetMasks(r, g, b);
	int w = (int)desc[6], h = (int)desc[7];
	int bpp = bits >> 3;
	byte *rgb = operator_new_array((uint32_t)(h * w * 3));
	int rowBytes = w * bpp;
	byte *row = operator_new_array((uint32_t)(rowBytes + 4));
	const Pal16Shift *sh = &g_Pal16Shift;
	for (int y = 0; y < h; y++) {
		memcpy(row, lr.pBits + lr.Pitch * y, (uint32_t)rowBytes);
		byte *p = rgb + y * w * 3;
		const byte *s = row;
		for (int x = 0; x < w; x++) {
			uint32_t c = *(const uint32_t *)s;
			p[0] = (byte)((byte)(c >> sh->rShr) << sh->rShl);
			p[1] = (byte)((byte)(c >> sh->gShr) << sh->gShl);
			p[2] = (byte)((byte)(c >> sh->bShr) << sh->bShl);
			if (g_Gamma2x) {
				p[0] = Shot_Double(p[0]);
				p[1] = Shot_Double(p[1]);
				p[2] = Shot_Double(p[2]);
			}
			s += bpp;
			p += 3;
		}
	}
	Rd_SurfUnlockRect(surf);
	void *f = crt_fopen(g_ShotPath, "w+b");
	if (f) {
		Pcx_Write24(f, rgb, w, h);
		crt_fclose(f);
	}
	if (row) {
		operator_delete_sized(row, 1);
	}
	if (rgb) {
		operator_delete_sized(rgb, 1);
	}
	return 1;
}

byte Surface_ShotCopy(void *surf) {
	uint32_t desc[8];
	void *dev, *copy;

	if (!surf || Rd_SurfGetDesc(surf, desc)) {
		return 0;
	}
	if (Rd_SurfGetDevice(surf, &dev) < 0) {
		return 0;
	}
	if (Rd_DevCreateImageSurface(dev, desc[6], desc[7], (int)desc[0],
		&copy)) {
		Rd_Release(dev);
		return 0;
	}
	if (Rd_DevCopyRects(dev, surf, NULL, 0, copy, NULL)) {
		Rd_Release(copy);
		Rd_Release(dev);
		return 0;
	}
	byte r = Surface_SaveShot(copy);
	Rd_Release(copy);
	Rd_Release(dev);
	return r;
}
