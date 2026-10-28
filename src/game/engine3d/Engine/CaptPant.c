/*
 * The screen capture of the engine3d, original source Engine/CaptPant.cpp: a locked surface into the next free
 * ShotNNNN.pcx, or while the DVF name (g_DVFName) is set as a frame of an AVI file through the Video for Windows
 * functions of the exe's imports (the format chosen in the AVISaveOptions dialog).
 */
#include <string.h>

#include "scrap/bsp.h"
#include "scrap/crt.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/rendertarget.h"
#include "scrap/scrapmain.h"
#include "scrap/varios.h"
#include "scrap/win32.h"

/* avifil32 and msvfw32 through the import table */
typedef uint32_t(__stdcall *vfwVersion_t)(void);
typedef void(__stdcall *aviFileInit_t)(void);
typedef uint32_t(__stdcall *aviRelease_t)(void *obj);
typedef int(__stdcall *aviFileOpen_t)(void **file, const char *name, uint32_t mode, void *handler);
typedef int(__stdcall *aviFileCreateStream_t)(void *file, void **stream, void *info);
typedef int(__stdcall *aviSaveOptions_t)(void *hwnd, uint32_t flags, int n, void **streams, void **opts);
typedef int(__stdcall *aviSaveOptionsFree_t)(int n, void **opts);
typedef int(__stdcall *aviMakeCompressedStream_t)(void **out, void *src, void *opts, void *handler);
typedef int(__stdcall *aviStreamSetFormat_t)(void *stream, int pos, void *fmt, int size);
typedef int(__stdcall *aviStreamWrite_t)(void *stream, int start, int n, const void *buf, int size, uint32_t flags,
	int *written, int *bytes);
typedef int(__stdcall *setrect_t)(exe_rect *r, int left, int top, int right, int bottom);
#define exe_VideoForWindowsVersion GAME_DATA(vfwVersion_t, 0x3a132c)
#define exe_AVIFileInit GAME_DATA(aviFileInit_t, 0x3a1030)
#define exe_AVIFileExit GAME_DATA(aviFileInit_t, 0x3a102c)
#define exe_AVIFileRelease GAME_DATA(aviRelease_t, 0x3a1028)
#define exe_AVIFileOpenA GAME_DATA(aviFileOpen_t, 0x3a1040)
#define exe_AVIFileCreateStreamA GAME_DATA(aviFileCreateStream_t, 0x3a1020)
#define exe_AVIStreamRelease GAME_DATA(aviRelease_t, 0x3a101c)
#define exe_AVIStreamSetFormat GAME_DATA(aviStreamSetFormat_t, 0x3a1044)
#define exe_AVIStreamWrite GAME_DATA(aviStreamWrite_t, 0x3a1024)
#define exe_AVIMakeCompressedStream GAME_DATA(aviMakeCompressedStream_t, 0x3a1034)
#define exe_AVISaveOptions GAME_DATA(aviSaveOptions_t, 0x3a1038)
#define exe_AVISaveOptionsFree GAME_DATA(aviSaveOptionsFree_t, 0x3a103c)
#define exe_SetRect GAME_DATA(setrect_t, 0x3a138c)

#define OF_WRITE_CREATE 0x1001
#define STREAMTYPE_VIDEO 0x73646976	/* "vids" */
#define AVIIF_KEYFRAME 0x10

/* AVISTREAMINFOA */
typedef struct AviStreamInfo {
	uint32_t fccType;
	uint32_t fccHandler;
	uint32_t dwFlags;
	uint32_t dwCaps;
	uint16_t wPriority;
	uint16_t wLanguage;
	uint32_t dwScale;
	uint32_t dwRate;
	uint32_t dwStart;
	uint32_t dwLength;
	uint32_t dwInitialFrames;
	uint32_t dwSuggestedBufferSize;
	uint32_t dwQuality;
	uint32_t dwSampleSize;
	exe_rect rcFrame;
	uint32_t dwEditCount;
	uint32_t dwFormatChangeCount;
	char szName[64];
} AviStreamInfo;
SD_SIZE(AviStreamInfo, 0x8c);

#define g_AviFrames GAME_GLOBAL(int, g_AviFrames, 0x4b29a0)		/* the frames written, -1 without a file */
#define g_AviFile GAME_GLOBAL(void *, g_AviFile, 0x4b29a4)
#define g_AviStream GAME_GLOBAL(void *, g_AviStream, 0x4b29a8)
#define g_AviCompressed GAME_GLOBAL(void *, g_AviCompressed, 0x4b29ac)
#define g_AviBih GAME_GLOBAL(AviBih, g_AviBih, 0x4b29b0)
#define g_AviRate GAME_GLOBAL_IN(uint32_t, GVar_MovieFramerate, 16, 0x4b2950)		/* the frames per second */
#define g_ShotPath (GAME_ARRAY(char, g_ShotPath, 0x4b29d8))	/* MAX_RUTA */
#define g_ShotNum GAME_GLOBAL(int, g_ShotNum, 0x432948)

/* IDirect3DSurface8 and IDirect3DDevice8 */
#define D3DSURF_GETDEVICE 0xc
#define D3DSURF_GETDESC 0x20
#define D3DSURF_LOCKRECT 0x24
#define D3DSURF_UNLOCKRECT 0x28
#define D3DLOCK_READONLY 0x10
typedef struct CPLockedRect {
	int Pitch;
	byte *pBits;
} CPLockedRect;
typedef int(__stdcall *cpGetDesc_t)(void *surf, uint32_t *desc);	/* D3DSURFACE_DESC: format, ..., width, height */
typedef int(__stdcall *cpLockRect_t)(void *surf, CPLockedRect *lr, const int *rect, uint32_t flags);
typedef int(__stdcall *cpUnlockRect_t)(void *surf);
typedef int(__stdcall *cpGetDevice_t)(void *surf, void **dev);
typedef int(__stdcall *cpCreateImageSurface_t)(void *dev, uint32_t w, uint32_t h, int fmt, void **surf);
typedef int(__stdcall *cpCopyRects_t)(void *dev, void *src, const int *rects, uint32_t n, void *dst, const void *pts);

byte Avi_Load(const char *name) {
	if (g_AviFrames >= 0) {
		Avi_Write(name, 0, 0, NULL, 0);
	}
	Avi_Write(NULL, 0, 0, NULL, 0);
	return 1;
}

byte Avi_Frame(const char *name, int frame) {
	if (g_AviFrames < frame) {
		return 0;
	}
	Avi_Write(name, 0, 0, NULL, 0);
	return 1;
}

byte Avi_Save(const char *name) {
	if (g_AviFrames >= 0) {
		Avi_Write(name, 0, 0, NULL, 0);
	}
	Avi_Write(NULL, 0, 0, NULL, 0);
	E3D_LogPrint(g_FileLog, "AVI closed.");
	return 1;
}

void Avi_Write(const char *name, int lx, int ly, const void *buf, int bufsize) {
	if (!name) {
		if (!g_AviFile) {
			return;
		}
		if (g_AviStream) {
			exe_AVIStreamRelease(g_AviStream);
			g_AviStream = NULL;
		}
		if (g_AviCompressed) {
			exe_AVIStreamRelease(g_AviCompressed);
			g_AviCompressed = NULL;
		}
		if (g_AviFile) {
			exe_AVIFileRelease(g_AviFile);
			g_AviFile = NULL;
		}
		exe_AVIFileExit();
		return;
	}
	if (!buf) {
		return;
	}
	if (!g_AviFile) {
		AviStreamInfo info;
		byte optsMem[0x2c];
		void *opts;

		E3D_LogPrint(g_FileLog, "Creating AVI: %s", name);
		CHECK((lx % 4) == 0);
		CHECK((ly % 4) == 0);
		AviBih *bih = &g_AviBih;
		memset(bih, 0, sizeof(*bih));
		bih->biSize = sizeof(*bih);
		bih->biWidth = lx;
		bih->biHeight = ly;
		bih->biPlanesBits = 0x180001;
		bih->biSizeImage = (uint32_t)((lx * 24 + 0x1f) / 32 * ly * 4);
		CHECK((exe_VideoForWindowsVersion() >> 16) >= 0x010a);
		exe_AVIFileInit();
		CHECK(exe_AVIFileOpenA(&g_AviFile, name, OF_WRITE_CREATE, NULL) == 0);
		memset(&info, 0, sizeof(info));
		info.dwRate = g_AviRate;
		info.dwSuggestedBufferSize = bih->biSizeImage;
		exe_SetRect(&info.rcFrame, 0, 0, bih->biWidth, bih->biHeight);
		info.fccType = STREAMTYPE_VIDEO;
		info.fccHandler = 0;
		info.dwScale = 1;
		CHECK(exe_AVIFileCreateStreamA(g_AviFile, &g_AviStream, &info) == 0);
		opts = optsMem;
		memset(optsMem, 0, sizeof(optsMem));
		if (!exe_AVISaveOptions(NULL, 0, 1, &g_AviStream, &opts)) {
			/* cancelled: the file closed, the capture off */
			exe_AVISaveOptionsFree(1, &opts);
			Avi_Load(name);
			g_DVFName[0] = 0;
			return;
		}
		CHECK(exe_AVIMakeCompressedStream(&g_AviCompressed, g_AviStream, optsMem, NULL) == 0);
		CHECK(exe_AVISaveOptionsFree(1, &opts) == 0);
		CHECK(exe_AVIStreamSetFormat(g_AviCompressed, 0, bih, (int)(bih->biSize + bih->biClrUsed * 4)) == 0);
		g_AviFrames = 0;
		if (!g_AviFile) {
			return;
		}
	}
	CHECK(bufsize == lx * ly * 3);
	CHECK(g_AviBih.biSizeImage == (uint32_t)bufsize);
	exe_AVIStreamWrite(g_AviCompressed, g_AviFrames, 1, buf, (int)g_AviBih.biSizeImage, AVIIF_KEYFRAME, NULL, NULL);
	g_AviFrames++;
}

/* the doubling of the gamma2x setting, 0 stays 0 */
static byte Shot_Double(byte c) {
	uint32_t v = (uint32_t)c * 2;
	return v >= 0xff ? 0xff : (byte)v;
}

/* the name of the next shot: "Shot*" (or the name set) with the first free number 1..9999 for the '*' */
static void Shot_NextName(void) {
	char buf[MAX_RUTA];

	Cad_Copy(g_ShotPath, g_ScreenshotName, MAX_RUTA);
	if (!g_ShotPath[0]) {
		Cad_Copy(g_ShotPath, "Shot*", MAX_RUTA);
	}
	char *star = strchr(g_ShotPath, '*');
	if (!star) {
		Ruta_Format(g_ShotPath, g_ShotPath, "&U&D&N.pcx");
	} else {
		*star = 0;
		int n = ++g_ShotNum;
		while (n < 10000) {
			Bprintf(buf, sizeof(buf), "%s%04d.pcx", g_ShotPath, n);
			if (crt_access(buf, 0)) {
				Cad_Copy(g_ShotPath, buf, MAX_RUTA);
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
		Cad_Copy(g_ShotPath, "ScreenShot.pcx", MAX_RUTA);
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
	byte avi = g_DVFName[0] != 0;
	D3D_METHOD(surf, cpGetDesc_t, D3DSURF_GETDESC)(surf, desc);
	if (D3D_METHOD(surf, cpLockRect_t, D3DSURF_LOCKRECT)(surf, &lr, NULL, D3DLOCK_READONLY)) {
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
		D3D_METHOD(surf, cpUnlockRect_t, D3DSURF_UNLOCKRECT)(surf);
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
		int sy = avi ? h - 1 - y : y;
		memcpy(row, lr.pBits + lr.Pitch * sy, (uint32_t)rowBytes);
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
			if (avi) {
				byte t = p[2];
				p[2] = p[0];
				p[0] = t;
			}
			s += bpp;
			p += 3;
		}
	}
	D3D_METHOD(surf, cpUnlockRect_t, D3DSURF_UNLOCKRECT)(surf);
	if (!avi) {
		void *f = crt_fopen(g_ShotPath, "w+b");
		if (f) {
			Pcx_Write24(f, rgb, w, h);
			crt_fclose(f);
		}
	} else {
		Avi_Write(g_DVFName, w, h, rgb, h * w * 3);
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

	if (!surf || D3D_METHOD(surf, cpGetDesc_t, D3DSURF_GETDESC)(surf, desc)) {
		return 0;
	}
	if (D3D_METHOD(surf, cpGetDevice_t, D3DSURF_GETDEVICE)(surf, &dev) < 0) {
		return 0;
	}
	if (D3D_METHOD(dev, cpCreateImageSurface_t, D3DDEV_CREATEIMAGESURFACE)(dev, desc[6], desc[7], (int)desc[0],
		&copy)) {
		COM_RELEASE(dev);
		return 0;
	}
	if (D3D_METHOD(dev, cpCopyRects_t, D3DDEV_COPYRECTS)(dev, surf, NULL, 0, copy, NULL)) {
		COM_RELEASE(copy);
		COM_RELEASE(dev);
		return 0;
	}
	byte r = Surface_SaveShot(copy);
	COM_RELEASE(copy);
	COM_RELEASE(dev);
	return r;
}
