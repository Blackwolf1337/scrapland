/*
 * The video player of the engine3d (the source between CaptPant.cpp and bsp.cpp, no CHECKs): a .avi or .vds file
 * played through the multimedia streams of DirectShow (amstream) into DirectDraw surfaces and blitted onto the
 * engine window, until a key, the pad or the end of the stream.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/filesys.h"
#include "scrap/scrapmain.h"
#include "scrap/varios.h"
#include "scrap/video.h"
#include "scrap/win32.h"

/* ole32, ddraw and xinput through the import table */
typedef int(__stdcall *coCreateInstance_t)(const void *clsid, void *outer, uint32_t ctx, const void *iid, void **out);
typedef int(__stdcall *coInitialize_t)(void *reserved);
typedef void(__stdcall *coUninitialize_t)(void);
typedef int(__stdcall *directDrawCreate_t)(void *guid, void **dd, void *outer);
typedef uint32_t(__stdcall *xinputGetState_t)(uint32_t user, void *state);
#define exe_CoCreateInstance GAME_DATA(coCreateInstance_t, 0x3a158c)
#define exe_CoInitialize GAME_DATA(coInitialize_t, 0x3a1594)
#define exe_CoUninitialize GAME_DATA(coUninitialize_t, 0x3a1590)
#define exe_DirectDrawCreate GAME_DATA(directDrawCreate_t, 0x3a105c)
#define exe_XInputGetState GAME_DATA(xinputGetState_t, 0x3a14bc)

/* the GUIDs of the exe */
#define CLSID_AMMultiMediaStream Game_Ptr(0x3f7b80)
#define IID_IAMMultiMediaStream Game_Ptr(0x3f7c00)
#define IID_IDirectDrawMediaStream Game_Ptr(0x3f7b30)
#define MSPID_PrimaryVideo Game_Ptr(0x3f5ee0)
#define MSPID_PrimaryAudio Game_Ptr(0x3f5ed0)

/* COM methods by their vtable offset (IAMMultiMediaStream, IMediaStream, IDirectDrawMediaStream,
 * IDirectDrawStreamSample, IDirectDraw, IDirectDrawSurface) */
#define AMMS_GETMEDIASTREAM 0x10
#define AMMS_SETSTATE 0x1c
#define AMMS_INITIALIZE 0x30
#define AMMS_ADDMEDIASTREAM 0x3c
#define AMMS_OPENFILE 0x40
#define DDMS_CREATESAMPLE 0x34
#define SAMPLE_UPDATE 0x18
#define SAMPLE_GETSURFACE 0x20
#define DD_CREATESURFACE 0x18
#define DD_GETDISPLAYMODE 0x30
#define DD_SETCOOPERATIVELEVEL 0x50
#define DDS_BLT 0x14
#define DDS_GETDC 0x44
#define DDS_GETSURFACEDESC 0x58
#define DDS_RELEASEDC 0x68
typedef uint32_t(__stdcall *comUnk_t)(void *obj);
typedef int(__stdcall *comQI_t)(void *obj, const void *iid, void **out);
typedef int(__stdcall *ammsInitialize_t)(void *obj, uint32_t type, uint32_t flags, void *graph);
typedef int(__stdcall *ammsAddMediaStream_t)(void *obj, void *src, const void *purpose, uint32_t flags, void **out);
typedef int(__stdcall *ammsOpenFile_t)(void *obj, const uint16_t *name, uint32_t flags);
typedef int(__stdcall *ammsGetMediaStream_t)(void *obj, const void *purpose, void **out);
typedef int(__stdcall *ammsSetState_t)(void *obj, int state);
typedef int(__stdcall *ddmsCreateSample_t)(void *obj, void *surf, const void *rect, uint32_t flags, void **out);
typedef int(__stdcall *sampleGetSurface_t)(void *obj, void **surf, exe_rect *rect);
typedef int(__stdcall *sampleUpdate_t)(void *obj, uint32_t flags, void *event, void *apc, uintptr_t ctx);
typedef int(__stdcall *ddGetDesc_t)(void *obj, void *ddsd);
typedef int(__stdcall *ddCreateSurface_t)(void *obj, void *ddsd, void **surf, void *outer);
typedef int(__stdcall *ddSetCoop_t)(void *obj, void *hwnd, uint32_t flags);
typedef int(__stdcall *ddsBlt_t)(void *obj, const exe_rect *dst, void *src, const exe_rect *srcRect, uint32_t flags,
	void *fx);
typedef int(__stdcall *ddsGetDC_t)(void *obj, void **dc);
typedef int(__stdcall *ddsReleaseDC_t)(void *obj, void *dc);
#define COM_CALL(obj, type, off) D3D_METHOD(obj, type, off)

#define VK_ESCAPE 0x1b
#define VK_SPACE 0x20
#define VK_RETURN 0xd
#define XINPUT_GAMEPAD_A_B 0x3000
#define DDSCL_NORMAL 8
#define DDBLT_DDFX 0x800
#define DDBLT_WAIT 0x1000000

/* DDSURFACEDESC */
typedef struct VideoDDSD {
	uint32_t dwSize;		/* 0x00 */
	uint32_t dwFlags;		/* 0x04 */
	int dwHeight;			/* 0x08 */
	int dwWidth;			/* 0x0c */
	byte pad10[0x44];
	int dwRGBBitCount;		/* 0x54 of the pixel format */
	byte pad58[0x10];
	uint32_t dwCaps;		/* 0x68 */
} VideoDDSD;
SD_SIZE(VideoDDSD, 0x6c);

#define g_VideoPadHeld GAME_DATA(int, 0x4b2af4)
#define g_VideoMirror GAME_DATA(uint32_t, 0x4b2aec)	/* 1: up-down, 2: left-right */

byte Video_PadPressed(void) {
	uint32_t state[4] = { 0, 0, 0, 0 };

	if (exe_XInputGetState(0, state)) {
		return 0;
	}
	if (state[1] & XINPUT_GAMEPAD_A_B) {
		if (!g_VideoPadHeld) {
			g_VideoPadHeld = 1;
		}
		return 0;
	}
	if (g_VideoPadHeld == 1) {
		g_VideoPadHeld = 0;
		return 1;
	}
	return 0;
}

int Video_OpenStream(const char *name, void *dd, void **out) {
	uint16_t wpath[MAX_RUTA];
	void *ms = NULL;
	const char *what;

	*out = NULL;
	int hr = exe_CoCreateInstance(CLSID_AMMultiMediaStream, NULL, 1, IID_IAMMultiMediaStream, &ms);
	if (hr < 0) {
		what = "CoCreateInstance(CLSID_AMMultiMediaStream, NULL, CLSCTX_INPROC_SERVER, IID_IAMMultiMediaStream, "
			"(void **)&pAMStream)";
		goto failed;
	}
	hr = COM_CALL(ms, ammsInitialize_t, AMMS_INITIALIZE)(ms, 0, 0, NULL);
	if (hr < 0) {
		what = "pAMStream->Initialize(STREAMTYPE_READ, 0, NULL)";
		goto failed;
	}
	hr = COM_CALL(ms, ammsAddMediaStream_t, AMMS_ADDMEDIASTREAM)(ms, dd, MSPID_PrimaryVideo, 0, NULL);
	if (hr < 0) {
		what = "pAMStream->AddMediaStream(pDD, &MSPID_PrimaryVideo, 0, NULL)";
		goto failed;
	}
	/* the sound with the default renderer; MS_E_PURPOSEID (no audio) is fine */
	hr = COM_CALL(ms, ammsAddMediaStream_t, AMMS_ADDMEDIASTREAM)(ms, NULL, MSPID_PrimaryAudio, 1, NULL);
	if (hr < 0 && hr != (int)0x80040256) {
		E3D_LogBlue(g_FileLog, "pAMStream->AddMediaStream()  failed with HRESULT(0x%8.8X)", hr);
		goto done;
	}
	exe_MultiByteToWideChar(0, 0, name, -1, wpath, MAX_RUTA);
	hr = COM_CALL(ms, ammsOpenFile_t, AMMS_OPENFILE)(ms, wpath, 0);
	if (hr < 0) {
		what = "pAMStream->OpenFile(wPath, 0)";
		goto failed;
	}
	*out = ms;
	COM_CALL(ms, comUnk_t, 0x4)(ms);
	goto done;
failed:
	E3D_LogBlue(g_FileLog, "%s  failed with HRESULT(0x%8.8X)", what, hr);
done:
	if (!ms) {
		E3D_LogBlue(g_FileLog, "ERROR: Could not create a CLSID_MultiMediaStream object\n"
			"Check you have run regsvr32 amstream.dll");
	}
	if (ms) {
		COM_RELEASE(ms);
	}
	return hr;
}

/* the width / height ratio of the log lines, through unsigned or signed values as the original */
static double Video_RatioU(int w, int h) {
	return (double)((float)(double)(uint32_t)w / (float)(double)(uint32_t)h);
}

static void Video_Release(void **obj) {
	if (*obj) {
		COM_RELEASE(*obj);
		*obj = NULL;
	}
}

int Video_RenderStream(void *dd, void *primary, void *ms, byte *played) {
	VideoDDSD ddsd;
	exe_rect src, dst;
	uint32_t fx[25];
	void *vid = NULL, *dds = NULL, *sample = NULL, *surf = NULL, *sec = NULL;
	const char *what;
	int hr;

	E3D_LogDebug(g_FileLog, "RenderStreamToSurface():");
	*played = 0;
	hr = COM_CALL(ms, ammsGetMediaStream_t, AMMS_GETMEDIASTREAM)(ms, MSPID_PrimaryVideo, &vid);
	if (hr < 0) {
		what = "pMMStream->GetMediaStream(MSPID_PrimaryVideo, &pPrimaryVidStream)";
		goto failed;
	}
	hr = COM_CALL(vid, comQI_t, 0)(vid, IID_IDirectDrawMediaStream, &dds);
	if (hr < 0) {
		what = "pPrimaryVidStream->QueryInterface(IID_IDirectDrawMediaStream, (void **)&pDDStream)";
		goto failed;
	}
	hr = COM_CALL(dds, ddmsCreateSample_t, DDMS_CREATESAMPLE)(dds, NULL, NULL, 0, &sample);
	if (hr < 0) {
		what = "pDDStream->CreateSample(NULL, NULL, 0, &pSample)";
		goto failed;
	}
	hr = COM_CALL(sample, sampleGetSurface_t, SAMPLE_GETSURFACE)(sample, &surf, &src);
	if (hr < 0) {
		what = "pSample->GetSurface(&pSurface, &rect)";
		goto failed;
	}
	ddsd.dwSize = sizeof(ddsd);
	COM_CALL(surf, ddGetDesc_t, DDS_GETSURFACEDESC)(surf, &ddsd);
	E3D_LogDebug(g_FileLog, "  orig: %d x %d x %d (%f)", ddsd.dwWidth, ddsd.dwHeight, ddsd.dwRGBBitCount,
		Video_RatioU(ddsd.dwWidth, ddsd.dwHeight));
	int origW = ddsd.dwWidth;
	int origH = ddsd.dwHeight;
	ddsd.dwSize = sizeof(ddsd);
	if (primary) {
		COM_CALL(primary, ddGetDesc_t, DDS_GETSURFACEDESC)(primary, &ddsd);
	} else {
		COM_CALL(dd, ddGetDesc_t, DD_GETDISPLAYMODE)(dd, &ddsd);
	}
	E3D_LogDebug(g_FileLog, "  disp: %d x %d x %d (%f)", ddsd.dwWidth, ddsd.dwHeight, ddsd.dwRGBBitCount,
		Video_RatioU(ddsd.dwWidth, ddsd.dwHeight));
	dst.right = ddsd.dwWidth;
	dst.left = 0;
	dst.top = 0;
	dst.bottom = ddsd.dwHeight;
	if (!Engine_IsFullScreen()) {
		exe_point pt = { 0, 0 };
		if (primary) {
			exe_ClientToScreen(MOTOR_HWND(g_Motor), &pt);
		}
		const int *cr = (const int *)(g_Motor + 0x2a508);
		dst.left = cr[0] + pt.x;
		dst.top = cr[1] + pt.y;
		dst.right = cr[2] + pt.x;
		dst.bottom = cr[3] + pt.y;
	}
	int h = dst.bottom - dst.top;
	int w = dst.right - dst.left;
	E3D_LogDebug(g_FileLog, "  dest: %d x %d (%f)", w, h, (double)((float)w / (float)h));
	/* the bars that keep the ratio of the video */
	int bx = (int)(((float)w - (float)(h * origW) / (float)origH) * 0.5f);
	if (bx <= 0) {
		bx = 0;
	}
	int by = (int)(((float)h - (float)(w * origH) / (float)origW) * 0.5f);
	if (by <= 0) {
		by = 0;
	}
	E3D_LogDebug(g_FileLog, "  rect: (%d+%d,%d+%d)x(%d-%d,%d-%d)", dst.left, bx, dst.top, by, dst.right, bx,
		dst.bottom, by);
	dst.left += bx;
	dst.right -= bx;
	dst.top += by;
	dst.bottom -= by;
	if (bx || by) {
		Dbg_FillRect(0, 0, w, h, 0);
	}
	ddsd.dwWidth = src.right - src.left;
	ddsd.dwHeight = src.bottom - src.top;
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = 0x1007;		/* DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT */
	ddsd.dwCaps = 0x4040;		/* DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY */
	hr = COM_CALL(dd, ddCreateSurface_t, DD_CREATESURFACE)(dd, &ddsd, &sec, NULL);
	if (hr < 0) {
		what = "pDD->CreateSurface(&ddsd, &pSecSurf, NULL)";
		goto failed;
	}
	Video_WaitKeysUp(1000);
	hr = COM_CALL(ms, ammsSetState_t, AMMS_SETSTATE)(ms, 1);
	if (hr < 0) {
		what = "pMMStream->SetState(STREAMSTATE_RUN)";
		goto failed;
	}
	/* DDBLTFX with the mirror flags */
	memset(fx, 0, sizeof(fx));
	fx[0] = sizeof(fx);
	fx[1] = (g_VideoMirror & 1) << 2 | (g_VideoMirror & 2);
	uint32_t bltFlags = g_VideoMirror ? DDBLT_DDFX : 0;
	while (!exe_GetAsyncKeyState(VK_ESCAPE)) {
		if (exe_GetAsyncKeyState(VK_SPACE) || exe_GetAsyncKeyState(VK_RETURN) || Video_PadPressed()) {
			break;
		}
		if (COM_CALL(sample, sampleUpdate_t, SAMPLE_UPDATE)(sample, 0, NULL, NULL, 0)) {
			goto stop;
		}
		if (!primary) {
			Video_BltToWindow(MOTOR_HWND(g_Motor), &dst, surf, &src);
		} else {
			COM_CALL(sec, ddsBlt_t, DDS_BLT)(sec, &src, surf, &src, bltFlags | DDBLT_WAIT, fx);
			COM_CALL(primary, ddsBlt_t, DDS_BLT)(primary, &dst, sec, &src, DDBLT_WAIT, NULL);
		}
	}
	*played = 1;
stop:
	Video_WaitKeysUp(1000);
	COM_CALL(ms, ammsSetState_t, AMMS_SETSTATE)(ms, 0);
	goto done;
failed:
	E3D_LogBlue(g_FileLog, "%s  failed with HRESULT(0x%8.8X)", what, hr);
done:
	Video_Release(&sec);
	Video_Release(&surf);
	Video_Release(&sample);
	Video_Release(&dds);
	if (vid) {
		COM_RELEASE(vid);
	}
	return hr;
}

void Video_BltToWindow(void *hwnd, const exe_rect *dst, void *surf, const exe_rect *src) {
	void *sdc;

	if (COM_CALL(surf, ddsGetDC_t, DDS_GETDC)(surf, &sdc) < 0) {
		return;
	}
	void *dc = exe_GetDC(hwnd);
	if (dc) {
		exe_SetStretchBltMode(dc, 3);	/* COLORONCOLOR */
		exe_StretchBlt(dc, dst->left, dst->top, dst->right - dst->left, dst->bottom - dst->top, sdc, src->left,
			src->top, src->right - src->left, src->bottom - src->top, 0xcc0020);
		exe_ReleaseDC(hwnd, dc);
	}
	COM_CALL(surf, ddsReleaseDC_t, DDS_RELEASEDC)(surf, sdc);
}

int Video_PlayFile(const char *name, const char *dir) {
	char buf[MAX_RUTA];

	/* "a|b": a first, then b */
	if (strchr(name, '|')) {
		Bprintf(buf, sizeof(buf), "%s", name);
		*strchr(buf, '|') = 0;
		Video_PlayFile(buf, dir);
		name = strchr(name, '|') + 1;
		while (*name && *name == ' ') {
			name++;
		}
	}
	Bprintf(buf, sizeof(buf), "&U&D%s/&N&E", *g_LangName);
	Ruta_Format(buf, name, buf);
	int r = Video_PlayPath(buf, dir, 0);
	if (!r) {
		r = Video_PlayPath(name, dir, 1);
	}
	return r;
}

int Video_PlayPath(const char *name, const char *dir, byte warn) {
	char path[MAX_RUTA];
	void *dd, *ms;
	byte played;

	Bprintf(path, sizeof(path), "%s%s", dir ? dir : "", name);
	if (File_Access(path, 0)) {
		Ruta_Format(path, path, "&U&D&N.vds");
		if (File_Access(path, 0)) {
			if (warn) {
				Con_Warnf("ERROR: video file not found [%s]\n", path);
			}
			return 0;
		}
	}
	E3D_LogPrint(g_FileLog, "VIDEO: %s", path);
	played = 0;
	exe_CoInitialize(NULL);
	if (exe_DirectDrawCreate(NULL, &dd, NULL) < 0) {
		E3D_LogBlue(g_FileLog, "ERROR: Could not open DirectDraw - check it is installed");
	} else {
		int hr = COM_CALL(dd, ddSetCoop_t, DD_SETCOOPERATIVELEVEL)(dd, MOTOR_HWND(g_Motor), DDSCL_NORMAL);
		if (hr < 0) {
			E3D_LogBlue(g_FileLog, "ERROR: SetCooperativeLevel(): 0x%08X", hr);
		} else if (Video_OpenStream(path, dd, &ms) >= 0) {
			Video_RenderStream(dd, NULL, ms, &played);
			COM_RELEASE(ms);
		}
		if (COM_RELEASE(dd)) {
			E3D_LogBlue(g_FileLog, "ERROR: A DirectDraw object has a non-zero reference count");
		}
	}
	exe_CoUninitialize();
	return played ? 2 : 1;
}
