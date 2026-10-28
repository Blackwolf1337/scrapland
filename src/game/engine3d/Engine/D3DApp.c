/*
 * CD3DApplication (d3dapp.cpp of the DX8 SDK framework, g_Motor) in the engine3d range, see include/scrap/dxutil.h:
 * the constructor and destructor, the enumeration of the adapters, devices and modes, the window and the device
 * creation, the frame, the message procedure and loop, the window style of a mode change, pause, the back buffer
 * description, the device reset and the device dialog.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/dxutil.h"
#include "scrap/render.h"
#include "scrap/scrapcamera.h"
#include "scrap/scrapmain.h"
#include "scrap/textura.h"
#include "scrap/varios.h"
#include "scrap/win32.h"

#define CD3DAPPLICATION_VFTABLE GAME_VFTABLE(CD3DApplication_vftable, 0x3dd208)
#define D3DAPP_TIMER_START 1
#define D3DAPP_TIMER_STOP 2

#define g_AppPausedCount GAME_DATA(int, 0x4b1c3c)


int D3DApp_SortModes(const D3DDisplayMode *a, const D3DDisplayMode *b) {
	if (a->Width < b->Width) {
		return -1;
	}
	if (a->Width > b->Width) {
		return 1;
	}
	if (a->Height < b->Height) {
		return -1;
	}
	if (a->Height > b->Height) {
		return 1;
	}
	if ((int)a->Format > (int)b->Format) {
		return -1;
	}
	return (int)a->Format < (int)b->Format;
}

typedef intptr_t(__thiscall *d3dappMsgProc_t)(byte *self, void *hwnd, uint32_t msg, uintptr_t w, intptr_t l);

intptr_t __stdcall D3DApp_WndProc(void *hwnd, uint32_t msg, uintptr_t w, intptr_t l) {
	byte *app = g_Motor;
	return ((d3dappMsgProc_t)(*(void ***)app)[0x30 / 4])(app, hwnd, msg, w, l);
}

typedef int(__stdcall *d3dCheckDeviceFormat_t)(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t usage,
	uint32_t rtype, uint32_t check);
typedef int(__stdcall *d3dCheckDepthStencilMatch_t)(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt,
	uint32_t rt, uint32_t ds);

/* IDirect3D8::CheckDeviceFormat (as D3DUSAGE_DEPTHSTENCIL of a surface) and CheckDepthStencilMatch */
byte D3DApp_DepthFormatOk(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t ds) {
	if (D3D_METHOD(d3d, d3dCheckDeviceFormat_t, 0x28)(d3d, adapter, type, fmt, 2, 1, ds) < 0) {
		return 0;
	}
	if (D3D_METHOD(d3d, d3dCheckDepthStencilMatch_t, 0x30)(d3d, adapter, type, fmt, fmt, ds) < 0) {
		return 0;
	}
	return 1;
}

/* D3DFMT_* of the depth buffers */
#define FMT_D16 0x50
#define FMT_D15S1 0x49
#define FMT_D24X8 0x4d
#define FMT_D24S8 0x4b
#define FMT_D24X4S4 0x4f
#define FMT_D32 0x47

static byte D3DApp_TryDepth(byte *self, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t ds, uint32_t *out) {
	if (!D3DApp_DepthFormatOk(MOTOR_D3D(self), adapter, type, fmt, ds)) {
		return 0;
	}
	*out = ds;
	return 1;
}

/* the depth/stencil format for m_dwMinDepthBits and m_dwMinStencilBits, then the depth alone, then D16 */
int __thiscall CD3DApp_FindDepthStencilFormat(byte *self, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t *out) {
	uint32_t depth = MOTOR_U32(self, 0x2a5b4), stencil = MOTOR_U32(self, 0x2a5b8);

	if (depth <= 16 && stencil == 0 && D3DApp_TryDepth(self, adapter, type, fmt, FMT_D16, out)) {
		return 1;
	}
	if (depth <= 15 && stencil <= 1 && D3DApp_TryDepth(self, adapter, type, fmt, FMT_D15S1, out)) {
		return 1;
	}
	if (depth <= 24) {
		if (stencil == 0 && D3DApp_TryDepth(self, adapter, type, fmt, FMT_D24X8, out)) {
			return 1;
		}
		if (stencil <= 8 && D3DApp_TryDepth(self, adapter, type, fmt, FMT_D24S8, out)) {
			return 1;
		}
		if (stencil <= 4 && D3DApp_TryDepth(self, adapter, type, fmt, FMT_D24X4S4, out)) {
			return 1;
		}
	}
	if (depth <= 32 && stencil == 0 && D3DApp_TryDepth(self, adapter, type, fmt, FMT_D32, out)) {
		return 1;
	}
	if (depth <= 16 && D3DApp_TryDepth(self, adapter, type, fmt, FMT_D16, out)) {
		return 1;
	}
	if (depth <= 24 && D3DApp_TryDepth(self, adapter, type, fmt, FMT_D24X8, out)) {
		return 1;
	}
	if (depth <= 32 && D3DApp_TryDepth(self, adapter, type, fmt, FMT_D32, out)) {
		return 1;
	}
	return D3DApp_TryDepth(self, adapter, type, fmt, FMT_D16, out);
}

byte *__fastcall CD3DApp_ctor(byte *self) {
	*(void **)self = CD3DAPPLICATION_VFTABLE;
	g_Motor = self;
	MOTOR_U32(self, 0x2a37c) = 0;		/* m_dwNumAdapters */
	MOTOR_ADAPTER(self) = 0;
	MOTOR_D3D(self) = NULL;
	MOTOR_DEV(self) = NULL;
	MOTOR_HWND(self) = NULL;
	MOTOR_U32(self, 0x2a3d0) = 0;		/* m_hWndFocus */
	MOTOR_ACTIVE(self) = 0;
	MOTOR_READY(self) = 0;
	MOTOR_U32(self, 0x2a4f0) = 0;
	MOTOR_FRAMEMOVING(self) = 1;
	MOTOR_SINGLESTEP(self) = 0;
	MOTOR_U32(self, 0x2a520) = 0;
	self[0x2a524] = 0;			/* the device name */
	self[0x2a57e] = 0;
	MOTOR_DEVID(self) = 0;
	*(const char **)(self + 0x2a5ac) = GAME_STR(str_3dd240, 0x3dd240);	/* m_strWindowTitle "Engine3D [DX8]" */
	MOTOR_U32(self, 0x2a5bc) = 400;		/* m_dwCreationWidth */
	MOTOR_U32(self, 0x2a5c0) = 300;
	MOTOR_USEDEPTH(self) = 0;
	MOTOR_U32(self, 0x2a5b4) = 0x10;	/* m_dwMinDepthBits */
	MOTOR_U32(self, 0x2a5b8) = 0;		/* m_dwMinStencilBits */
	MOTOR_U32(self, 0x2a5c4) = 0;		/* m_bShowCursorWhenFullscreen */
	MOTOR_VSYNC(self) = 1;
	MOTOR_FULLSCREEN(self) = 0;
	MOTOR_WIDTH(self) = 800;
	MOTOR_HEIGHT(self) = 600;
	MOTOR_BPP(self) = 32;
	return self;
}

void __fastcall CD3DApp_dtor(byte *self) {
	*(void **)self = CD3DAPPLICATION_VFTABLE;
	g_Motor = NULL;
}

/* vf00: the window style (and in fullscreen the size of the mode) after a change */
int __thiscall CD3DApp_AdjustWindowForChange(byte *self) {
	if (MOTOR_WINDOWED(self)) {
		exe_SetWindowLongA(MOTOR_HWND(self), -16, (int32_t)MOTOR_U32(self, 0x2a4f4));	/* m_dwWindowStyle */
	} else {
		exe_SetWindowLongA(MOTOR_HWND(self), -16, (int32_t)0x90080000);	/* WS_POPUP | WS_SYSMENU | WS_VISIBLE */
		D3DAdapterInfo *ad = MOTOR_ADAPTERINFO(self);
		D3DDeviceInfo *dev = &ad->devices[ad->dwCurrentDevice];
		D3DModeInfo *mode = &dev->modes[dev->dwCurrentMode];
		exe_SetWindowPos(MOTOR_HWND(self), (void *)-1, 0, 0, (int)mode->Width, (int)mode->Height, 0);
	}
	if (!MOTOR_WINDOWED(self)) {
		exe_SetCursor(NULL);
	}
	return 0;
}

/* vf13: pause counted over the callers, the timer stopped with the first and started with the last */
void __thiscall CD3DApp_Pause(byte *self, int pause) {
	int n = g_AppPausedCount + (pause ? 1 : -1);

	g_AppPausedCount = n;
	MOTOR_READY(self) = n == 0;
	if (pause && n == 1) {
		if (!MOTOR_FRAMEMOVING(self)) {
			return;
		}
		DXUtil_Timer(D3DAPP_TIMER_STOP);
		n = g_AppPausedCount;
	}
	if (n == 0 && MOTOR_FRAMEMOVING(self)) {
		DXUtil_Timer(D3DAPP_TIMER_START);
	}
}

typedef int(__stdcall *d3dGetDepthStencil_t)(void *dev, void **surf);
typedef int(__stdcall *d3dGetDesc_t)(void *surf, void *desc);

/* the descriptions of the back buffer and the depth buffer, the aspect ratio of the back buffer */
void __fastcall CD3DApp_UpdateBackBufferDesc(byte *self) {
	void *surf;
	void *dev = MOTOR_DEV(self);

	D3D_METHOD(dev, d3dGetBackBuffer_t, 0x40)(dev, 0, 0, &surf);
	D3D_METHOD(surf, d3dGetDesc_t, 0x20)(surf, self + 0x2a4b0);
	COM_RELEASE(surf);
	uint32_t *zbuf = (uint32_t *)MOTOR_ZBUF(self);
	for (int i = 0; i < 8; i++) {
		zbuf[i] = 0;
	}
	if (MOTOR_USEDEPTH(self)) {
		dev = MOTOR_DEV(self);
		D3D_METHOD(dev, d3dGetDepthStencil_t, 0x84)(dev, &surf);
		D3D_METHOD(surf, d3dGetDesc_t, 0x20)(surf, zbuf);
		COM_RELEASE(surf);
	}
	uint32_t w = MOTOR_MODE(self)[6], h = MOTOR_MODE(self)[7];
	g_PhysicalAspectRatio = (float)(double)w / (float)(double)h;
	Con_Printf("Aspect: %f (%dx%d)\n", (double)g_PhysicalAspectRatio, (int)MOTOR_MODE(self)[6], (int)MOTOR_MODE(self)[7]);
	MOTOR_DEVID(self)++;
}

typedef int(__stdcall *d3dReset_t)(void *dev, void *pp);

/* without frame moving one frame is stepped: the timer started and stopped */
static void D3DApp_SingleStep(byte *self) {
	if (!MOTOR_FRAMEMOVING(self)) {
		MOTOR_SINGLESTEP(self) = 1;
		DXUtil_Timer(D3DAPP_TIMER_START);
		DXUtil_Timer(D3DAPP_TIMER_STOP);
	}
}

/* IDirect3DDevice8::Reset with the present parameters, logged when it fails; the gamma ramp set again */
static int D3DApp_DevReset(byte *self) {
	void *dev = MOTOR_DEV(self);
	int r = D3D_METHOD(dev, d3dReset_t, 0x38)(dev, MOTOR_PP(self));

	if (r < 0) {
		E3D_LogBlue(g_FileLog, GAME_STR(str_3dd33c, 0x3dd33c), r);	/* "ERROR: 'pd3dDevice->Reset()' failed: %08X" */
	}
	RenderGamma_Begin();
	return r;
}

int __thiscall CD3DApp_ResetDevice(byte *self) {
	return D3DApp_DevReset(self);
}

int __thiscall CD3DApp_Reset(byte *self) {
	int r = MOTOR_CALL(self, 0x1c);
	if (r < 0) {
		return r;
	}
	MOTOR_CALL(self, MOTOR_VT_INVALIDATE);
	r = D3DApp_DevReset(self);
	if (r < 0) {
		E3D_LogBlue(g_FileLog, GAME_STR(str_3dd30c, 0x3dd30c), r);	/* "ERROR: ResetDevice(): %08X" */
	}
	CD3DApp_UpdateBackBufferDesc(self);
	Splash_Reset();
	r = MOTOR_CALL(self, 0x0c);
	if (r < 0) {
		return r;
	}
	r = MOTOR_CALL(self, 0x10);
	if (r < 0) {
		return r;
	}
	D3DApp_SingleStep(self);
	return 0;
}

/* the default virtual methods: nothing to do, S_OK */
int __thiscall CD3DApp_Nop(byte *self) {
	(void)self;
	return 0;
}

/* vf01 ConfirmDevice: every device is fine */
int __thiscall CD3DApp_ConfirmDevice(byte *self, void *caps, uint32_t behavior, uint32_t fmt) {
	(void)self;
	(void)caps;
	(void)behavior;
	(void)fmt;
	return 0;
}

/* the device released (an error in the log when something still holds it), then FinalCleanup (vtable 0x24) */
int __thiscall CD3DApp_Cleanup(byte *self) {
	MOTOR_ACTIVE(self) = 0;
	MOTOR_READY(self) = 0;
	if (MOTOR_DEV(self)) {
		MOTOR_CALL(self, 0x1c);
		MOTOR_CALL(self, MOTOR_VT_INVALIDATE);
		if (COM_RELEASE(MOTOR_DEV(self))) {
			E3D_LogBlue(g_FileLog, GAME_STR(str_3dd3dc, 0x3dd3dc));	/* "A D3D object has a non-zero reference count." */
		}
		COM_RELEASE(MOTOR_D3D(self));
		MOTOR_DEV(self) = NULL;
		MOTOR_D3D(self) = NULL;
	}
	return MOTOR_CALL(self, MOTOR_VT_DELETE);
}

static D3DDeviceInfo *D3DApp_CurDevice(byte *self) {
	D3DAdapterInfo *ad = MOTOR_ADAPTERINFO(self);
	return &ad->devices[ad->dwCurrentDevice];
}

/* the device released and the environment made again; 0 and the result of the error shown on failures */
static byte D3DApp_Recreate(byte *self, int *err) {
	MOTOR_CALL(self, 0x1c);
	MOTOR_CALL(self, MOTOR_VT_INVALIDATE);
	if (COM_RELEASE(MOTOR_DEV(self))) {
		*err = CD3DApp_DisplayErrorMsg(self, 0x8200000a, 1);	/* D3DAPPERR_NONZEROREFCOUNT */
		return 0;
	}
	int r = CD3DApp_Initialize3DEnvironment(self);
	if (r < 0) {
		*err = CD3DApp_DisplayErrorMsg(self, r, 1);
		return 0;
	}
	return 1;
}

/* windowed on the current device or the first one of all adapters that can, the environment made again */
int __thiscall CD3DApp_ForceWindowed(byte *self) {
	D3DDeviceInfo *dev = D3DApp_CurDevice(self);

	if (!dev->bCanDoWindowed) {
		uint32_t a, d = 0;
		D3DAdapterInfo *ad = (D3DAdapterInfo *)MOTOR_ADAPTERS(self);
		for (a = 0; a < MOTOR_U32(self, 0x2a37c); a++, ad++) {
			for (d = 0; d < ad->dwNumDevices && !ad->devices[d].bCanDoWindowed; d++) {
			}
			if (d < ad->dwNumDevices) {
				break;
			}
		}
		if (a >= MOTOR_U32(self, 0x2a37c)) {
			return E_FAIL;
		}
		MOTOR_ADAPTER(self) = (int)a;
		ad->dwCurrentDevice = (int)d;
		dev = &ad->devices[d];
	}
	dev->bWindowed = 1;
	MOTOR_WINDOWED(self) = 1;
	MOTOR_READY(self) = 0;
	int err;
	if (!D3DApp_Recreate(self, &err)) {
		return err;
	}
	MOTOR_READY(self) = 1;
	return 0;
}

int __thiscall CD3DApp_ToggleFullscreen(byte *self) {
	D3DAdapterInfo *ad = MOTOR_ADAPTERINFO(self);
	D3DDeviceInfo *dev = &ad->devices[ad->dwCurrentDevice];
	D3DModeInfo *mode = &dev->modes[dev->dwCurrentMode];
	uint32_t *pp = MOTOR_PP(self);

	if (!MOTOR_WINDOWED(self) && !dev->bCanDoWindowed) {
		return CD3DApp_ForceWindowed(self);
	}
	MOTOR_READY(self) = 0;
	MOTOR_WINDOWED(self) = !MOTOR_WINDOWED(self);
	dev->bWindowed = MOTOR_WINDOWED(self);
	MOTOR_CALL(self, 0x00);			/* AdjustWindowForChange */
	pp[7] = (uint32_t)dev->bWindowed;	/* Windowed */
	pp[4] = dev->MultiSampleType;
	pp[9] = mode->DepthStencilFormat;	/* AutoDepthStencilFormat */
	pp[6] = (uint32_t)(uintptr_t)MOTOR_HWND(self);	/* hDeviceWindow */
	if (MOTOR_WINDOWED(self)) {
		/* the client rectangle of the window */
		pp[0] = MOTOR_U32(self, 0x2a510) - MOTOR_U32(self, 0x2a508);
		pp[1] = MOTOR_U32(self, 0x2a514) - MOTOR_U32(self, 0x2a50c);
		pp[2] = ad->desktopMode[3];
		pp[12] = 0;			/* FullScreen_PresentationInterval */
	} else {
		pp[0] = mode->Width;
		pp[1] = mode->Height;
		pp[2] = mode->Format;
		pp[12] = MOTOR_VSYNC(self) ? 0 : 0x80000000;	/* D3DPRESENT_INTERVAL_IMMEDIATE without vsync */
	}
	if (CD3DApp_Reset(self) < 0) {
		return CD3DApp_DisplayErrorMsg(self, 0x8200000c, 1);	/* D3DAPPERR_RESIZEFAILED */
	}
	if (MOTOR_WINDOWED(self)) {
		/* the window rectangle, not topmost */
		int x = (int)MOTOR_U32(self, 0x2a4f8), y = (int)MOTOR_U32(self, 0x2a4fc);
		exe_SetWindowPos(MOTOR_HWND(self), (void *)-2, x, y, (int)MOTOR_U32(self, 0x2a500) - x,
			(int)MOTOR_U32(self, 0x2a504) - y, 0x40);
	}
	MOTOR_READY(self) = 1;
	return 0;
}

typedef void *(__stdcall *getmodulehandle_t)(const char *name);
#define exe_GetModuleHandleA GAME_DATA(getmodulehandle_t, 0x3a1160)

int __thiscall CD3DApp_UserSelectNewDevice(byte *self) {
	if (!MOTOR_WINDOWED(self) && CD3DApp_ToggleFullscreen(self) < 0) {
		CD3DApp_DisplayErrorMsg(self, 0x8200000c, 1);
		return E_FAIL;
	}
	/* the device dialog (resource 0x90), SelectDeviceProc */
	if (exe_DialogBoxParamA(exe_GetModuleHandleA(NULL), (const char *)0x90, MOTOR_HWND(self), Game_Ptr(0x275960),
		(intptr_t)self) != 1) {
		return 0;
	}
	MOTOR_WINDOWED(self) = D3DApp_CurDevice(self)->bWindowed;
	int err;
	if (!D3DApp_Recreate(self, &err)) {
		return err;
	}
	D3DApp_SingleStep(self);
	return 0;
}

/* the virtual methods of CD3DApplication by their vtable offset */
#define D3DAPP_VT_ADJUSTWINDOW 0x00
#define D3DAPP_VT_CONFIRMDEVICE 0x04
#define D3DAPP_VT_ONETIMESCENEINIT 0x08
#define D3DAPP_VT_INITDEVICEOBJECTS 0x0c
#define D3DAPP_VT_RESTOREDEVICEOBJECTS 0x10
#define D3DAPP_VT_FRAMEMOVE 0x14
#define D3DAPP_VT_RENDER 0x18
#define D3DAPP_VT_INVALIDATEDEVICEOBJECTS 0x1c
#define D3DAPP_VT_DELETEDEVICEOBJECTS 0x20
#define D3DAPP_VT_PAUSE 0x34
typedef int(__thiscall *d3dappConfirm_t)(byte *self, void *caps, uint32_t behavior, uint32_t fmt);
typedef void(__thiscall *d3dappPause_t)(byte *self, int pause);
#define D3DAPP_PAUSE(m, p) (((d3dappPause_t)(*(void ***)(m))[D3DAPP_VT_PAUSE / 4])(m, p))

/* D3DAPPERR_* */
#define D3DAPPERR_NODIRECT3D 0x82000001
#define D3DAPPERR_NOCOMPATIBLEDEVICES 0x82000003
#define D3DAPPERR_NOWINDOWABLEDEVICES 0x82000004
#define D3DAPPERR_NOHARDWAREDEVICE 0x82000005
#define D3DAPPERR_HALNOTCOMPATIBLE 0x82000006
#define D3DAPPERR_NOWINDOWEDHAL 0x82000007
#define D3DAPPERR_NODESKTOPHAL 0x82000008
#define D3DAPPERR_NOHALTHISMODE 0x82000009
#define D3DAPPERR_RESIZEFAILED 0x8200000c
#define D3DERR_DEVICELOST ((int)0x88760868)
#define D3DERR_DEVICENOTRESET ((int)0x88760869)

#define D3DDEVTYPE_HAL 1
#define D3DDEVTYPE_REF 2
#define D3DDEVTYPE_SW 3
#define D3DFMT_R8G8B8 0x14
#define D3DFMT_A8R8G8B8 0x15
#define D3DFMT_X8R8G8B8 0x16
#define D3DFMT_R5G6B5 0x17
#define D3DCAPS2_CANRENDERWINDOWED 0x80000
#define D3DDEVCAPS_HWTRANSFORMANDLIGHT 0x10000
#define D3DDEVCAPS_PUREDEVICE 0x100000
#define D3DCREATE_SOFTWARE_VERTEXPROCESSING 0x20
#define D3DCREATE_HARDWARE_VERTEXPROCESSING 0x40
#define D3DCREATE_MIXED_VERTEXPROCESSING 0x80
#define D3DCREATE_PUREDEVICE 0x10

#define MOTOR_D3DCAPS(m) ((void *)((m) + 0x2a3dc))
#define MOTOR_BEHAVIOR(m) MOTOR_U32(m, 0x2a4f0)
#define MOTOR_WINSTYLE(m) MOTOR_U32(m, 0x2a4f4)
#define MOTOR_WINRECT(m) ((exe_rect *)((m) + 0x2a4f8))
#define MOTOR_CLIENTRECT(m) ((exe_rect *)((m) + 0x2a508))
#define MOTOR_TIME(m) (*(float *)((m) + 0x2a518))
#define MOTOR_ELAPSED(m) (*(float *)((m) + 0x2a51c))
#define MOTOR_FPS(m) (*(float *)((m) + 0x2a520))
#define MOTOR_DEVICESTATS(m) ((char *)((m) + 0x2a524))	/* 90 */
#define MOTOR_FRAMESTATS(m) ((char *)((m) + 0x2a57e))	/* 90 */
#define MOTOR_TITLE(m) (*(const char **)((m) + 0x2a5ac))
#define MOTOR_SHOWCURSOR(m) MOTOR_U32(m, 0x2a5c4)	/* the cursor of the device while fullscreen */
#define MOTOR_CREATEW(m) MOTOR_U32(m, 0x2a5bc)
#define MOTOR_CREATEH(m) MOTOR_U32(m, 0x2a5c0)
#define MOTOR_HWNDFOCUS(m) (*(void **)((m) + 0x2a3d0))
#define MOTOR_NUMADAPTERS(m) MOTOR_U32(m, 0x2a37c)
#define D3DAPP_STATS 90

#define g_D3DAppFrames GAME_DATA(uint32_t, 0x4b1c38)
#define g_D3DAppLastTime GAME_DATA(float, 0x4b1c34)

typedef void *(__stdcall *direct3dcreate8_t)(uint32_t version);
typedef int(__stdcall *adjustwindowrect_t)(exe_rect *r, uint32_t style, int menu);
typedef void *(__stdcall *loadmenu_t)(void *inst, const char *name);
typedef int(__stdcall *setrect_t)(exe_rect *r, int left, int top, int right, int bottom);
typedef int(__stdcall *destroymenu_t)(void *menu);
typedef void(__stdcall *postquitmessage_t)(int code);
typedef int(__stdcall *getcursorpos_t)(exe_point *pt);
typedef int(__stdcall *screentoclient_t)(void *hwnd, exe_point *pt);
typedef int(__stdcall *enablewindow_t)(void *hwnd, int on);
#define exe_Direct3DCreate8 GAME_DATA(direct3dcreate8_t, 0x3a14c4)
#define exe_AdjustWindowRect GAME_DATA(adjustwindowrect_t, 0x3a1380)
#define exe_LoadMenuA GAME_DATA(loadmenu_t, 0x3a13c4)
#define exe_SetRect GAME_DATA(setrect_t, 0x3a138c)
#define exe_DestroyMenu GAME_DATA(destroymenu_t, 0x3a137c)
#define exe_PostQuitMessage GAME_DATA(postquitmessage_t, 0x3a1370)
#define exe_GetCursorPos GAME_DATA(getcursorpos_t, 0x3a1384)
#define exe_ScreenToClient GAME_DATA(screentoclient_t, 0x3a1388)
#define exe_EnableWindow GAME_DATA(enablewindow_t, 0x3a1374)

/* IDirect3D8 and IDirect3DDevice8 */
typedef uint32_t(__stdcall *d3dGetAdapterCount_t)(void *d3d);
typedef int(__stdcall *d3dGetAdapterIdentifier_t)(void *d3d, uint32_t adapter, uint32_t flags, void *id);
typedef uint32_t(__stdcall *d3dGetAdapterModeCount_t)(void *d3d, uint32_t adapter);
typedef int(__stdcall *d3dEnumAdapterModes_t)(void *d3d, uint32_t adapter, uint32_t mode, D3DDisplayMode *out);
typedef int(__stdcall *d3dGetAdapterDisplayMode_t)(void *d3d, uint32_t adapter, void *out);
typedef int(__stdcall *d3dCheckDeviceType_t)(void *d3d, uint32_t adapter, uint32_t type, uint32_t dfmt, uint32_t bfmt,
	int windowed);
typedef int(__stdcall *d3dCheckDeviceMultiSampleType_t)(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt,
	int windowed, uint32_t ms);
typedef int(__stdcall *d3dGetDeviceCaps_t)(void *d3d, uint32_t adapter, uint32_t type, void *caps);
typedef int(__stdcall *d3dCreateDevice_t)(void *d3d, uint32_t adapter, uint32_t type, void *hwnd, uint32_t behavior,
	void *pp, void **dev);
typedef int(__stdcall *devTestCooperativeLevel_t)(void *dev);
typedef int(__stdcall *devGetDeviceCaps_t)(void *dev, void *caps);
typedef void(__stdcall *devSetCursorPosition_t)(void *dev, int x, int y, uint32_t flags);
typedef int(__stdcall *devShowCursor_t)(void *dev, int show);
typedef int(__stdcall *devPresent_t)(void *dev, const void *src, const void *dst, void *hwnd, const void *dirty);
#define D3D8_GETADAPTERCOUNT 0x10
#define D3D8_GETADAPTERIDENTIFIER 0x14
#define D3D8_GETADAPTERMODECOUNT 0x18
#define D3D8_ENUMADAPTERMODES 0x1c
#define D3D8_GETADAPTERDISPLAYMODE 0x20
#define D3D8_CHECKDEVICETYPE 0x24
#define D3D8_CHECKDEVICEMULTISAMPLETYPE 0x2c
#define D3D8_GETDEVICECAPS 0x34
#define D3D8_CREATEDEVICE 0x3c
#define D3DDEV_TESTCOOPERATIVELEVEL 0xc
#define D3DDEV_GETDEVICECAPS 0x1c
#define D3DDEV_SETCURSORPOSITION 0x2c
#define D3DDEV_SHOWCURSOR 0x30
#define D3DDEV_PRESENT 0x3c

/* the bounded lstrcatA of the stats strings */
static void D3DApp_Cat(char *dst, int size, const char *src) {
	size_t n = strlen(dst);
	if ((int)n < size - 1) {
		Str_Copy(dst + n, src, size - (int)n);
	}
}

int __thiscall CD3DApp_Create(byte *self, void *inst) {
	exe_wndclass wc;
	exe_rect r;
	int hr;

	MOTOR_D3D(self) = exe_Direct3DCreate8(0xdc);	/* D3D_SDK_VERSION */
	if (!MOTOR_D3D(self)) {
		return CD3DApp_DisplayErrorMsg(self, D3DAPPERR_NODIRECT3D, 1);
	}
	hr = CD3DApp_BuildDeviceList(self);
	if (hr < 0) {
		goto fail;
	}
	if (!MOTOR_HWND(self)) {
		wc.style = 0;
		wc.wndProc = Game_Ptr(0x276460);	/* WndProc */
		wc.clsExtra = 0;
		wc.wndExtra = 0;
		wc.hInstance = inst;
		wc.hIcon = exe_LoadIconA(inst, (const char *)0x7f00);
		wc.hCursor = exe_LoadCursorA(NULL, (const char *)0x7f00);
		wc.hbrBackground = exe_GetStockObject(0);
		wc.menuName = NULL;
		wc.className = "Class_Engine3D_DX8";
		exe_RegisterClassA(&wc);
		MOTOR_WINSTYLE(self) = 0x90ce0000;	/* WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | ... */
		exe_SetRect(&r, 0, 0, (int)MOTOR_CREATEW(self), (int)MOTOR_CREATEH(self));
		exe_AdjustWindowRect(&r, MOTOR_WINSTYLE(self), 1);
		void *menu = exe_LoadMenuA(inst, (const char *)0x8d);
		MOTOR_HWND(self) = exe_CreateWindowExA(0, "Class_Engine3D_DX8", MOTOR_TITLE(self), MOTOR_WINSTYLE(self),
			(int)0x80000000, (int)0x80000000, r.right - r.left, r.bottom - r.top, NULL, menu, inst, NULL);
	}
	if (!MOTOR_HWNDFOCUS(self)) {
		MOTOR_HWNDFOCUS(self) = MOTOR_HWND(self);
	}
	MOTOR_WINSTYLE(self) = (uint32_t)exe_GetWindowLongA(MOTOR_HWND(self), -16);	/* GWL_STYLE */
	exe_GetWindowRect(MOTOR_HWND(self), MOTOR_WINRECT(self));
	exe_GetClientRect(MOTOR_HWND(self), MOTOR_CLIENTRECT(self));
	hr = MOTOR_CALL(self, D3DAPP_VT_ONETIMESCENEINIT);
	if (hr >= 0) {
		hr = CD3DApp_Initialize3DEnvironment(self);
		if (hr >= 0) {
			DXUtil_Timer(DXUTIL_TIMER_START);
			MOTOR_READY(self) = 1;
			return 0;
		}
	}
fail:
	if (MOTOR_D3D(self)) {
		COM_RELEASE(MOTOR_D3D(self));
		MOTOR_D3D(self) = NULL;
	}
	return CD3DApp_DisplayErrorMsg(self, hr, 1);
}

int __thiscall CD3DApp_DisplayErrorMsg(byte *self, int hr, int type) {
	char msg[0x200];
	const char *s;

	/* the texts of the exe */
	switch ((uint32_t)hr) {
	case D3DAPPERR_NODIRECT3D:
		s = GAME_STR(str_3dd490, 0x3dd490);
		break;
	case 0x8007000e:		/* E_OUTOFMEMORY */
		s = Game_Ptr(0x3dd84c);
		break;
	case 0x8876017c:		/* D3DERR_OUTOFVIDEOMEMORY */
		s = Game_Ptr(0x3dd860);
		break;
	case D3DAPPERR_NOCOMPATIBLEDEVICES:
		s = GAME_STR(str_3dd508, 0x3dd508);
		break;
	case D3DAPPERR_NOWINDOWABLEDEVICES:
		s = GAME_STR(str_3dd538, 0x3dd538);
		break;
	case D3DAPPERR_NOHARDWAREDEVICE:
		s = Game_Ptr(0x3dd5e4);
		break;
	case D3DAPPERR_HALNOTCOMPATIBLE:
		s = GAME_STR(str_3dd620, 0x3dd620);
		break;
	case D3DAPPERR_NOWINDOWEDHAL:
		s = Game_Ptr(0x3dd688);
		break;
	case D3DAPPERR_NODESKTOPHAL:
		s = GAME_STR(str_3dd6d0, 0x3dd6d0);
		break;
	case D3DAPPERR_NOHALTHISMODE:
		s = GAME_STR(str_3dd740, 0x3dd740);
		break;
	case 0x8200000a:		/* D3DAPPERR_NONZEROREFCOUNT */
		s = GAME_STR(str_3dd7f0, 0x3dd7f0);
		break;
	case 0x8200000b:		/* D3DAPPERR_MEDIANOTFOUND */
		s = Game_Ptr(0x3dd7cc);
		break;
	case D3DAPPERR_RESIZEFAILED:
		s = Game_Ptr(0x3d69f8);
		break;
	default:
		s = Game_Ptr(0x3dd87c);	/* "Generic application error." */
		break;
	}
	Str_Copy(msg, s, sizeof(msg));
	int fatal = 1;
	if (type != 1) {
		if (MOTOR_FULLSCREEN(self)) {
			Err_Printf(1, "%s", msg);
		}
		D3DApp_Cat(msg, sizeof(msg), "\nUsing REF (allowed only in windowed)");
		fatal = 0;
	}
	Err_Printf(fatal, "%s", msg);
	return hr;
}

/* the modes, depth formats and behaviors of the HAL and REF devices of all adapters, then the first fullscreen HAL
 * device (with MOTOR_FULLSCREEN) or the first windowed one; the reasons a windowed REF had to be taken shown */
int __fastcall CD3DApp_BuildDeviceList(byte *self) {
	static const uint32_t types[2] = { D3DDEVTYPE_HAL, D3DDEVTYPE_REF };
	const char *names[2] = { GAME_STR(str_3dd264, 0x3dd264), GAME_STR(str_3dd268, 0x3dd268) };	/* "HAL", "REF" */
	D3DDisplayMode modes[100];
	uint32_t fmts[20], behavior[20], depth[20];
	int fmtOk[20];
	int halExists = 0, halWindowed = 0, halDesktop = 0, halSample = 0;
	void *d3d = MOTOR_D3D(self);

	for (uint32_t a = 0; a < D3D_METHOD(d3d, d3dGetAdapterCount_t, D3D8_GETADAPTERCOUNT)(d3d);
		a++, d3d = MOTOR_D3D(self)) {
		if (MOTOR_NUMADAPTERS(self) >= 10) {
			continue;
		}
		D3DAdapterInfo *ad = (D3DAdapterInfo *)MOTOR_ADAPTERS(self) + MOTOR_NUMADAPTERS(self);
		D3D_METHOD(d3d, d3dGetAdapterIdentifier_t, D3D8_GETADAPTERIDENTIFIER)(d3d, a, 0, ad);
		d3d = MOTOR_D3D(self);
		D3D_METHOD(d3d, d3dGetAdapterDisplayMode_t, D3D8_GETADAPTERDISPLAYMODE)(d3d, a, ad->desktopMode);
		ad->dwNumDevices = 0;
		ad->dwCurrentDevice = 0;
		/* the modes without the refresh rates, and their formats with the desktop one first */
		uint32_t nModes = 0;
		d3d = MOTOR_D3D(self);
		uint32_t count = D3D_METHOD(d3d, d3dGetAdapterModeCount_t, D3D8_GETADAPTERMODECOUNT)(d3d, a);
		uint32_t nFmts = 1;
		fmts[0] = ad->desktopMode[3];
		for (uint32_t m = 0; m < count; m++) {
			D3DDisplayMode dm;
			d3d = MOTOR_D3D(self);
			D3D_METHOD(d3d, d3dEnumAdapterModes_t, D3D8_ENUMADAPTERMODES)(d3d, a, m, &dm);
			uint32_t i;
			for (i = 0; i < nModes; i++) {
				if (modes[i].Width == dm.Width && modes[i].Height == dm.Height && modes[i].Format == dm.Format) {
					break;
				}
			}
			if (i != nModes || nModes >= 100) {
				continue;
			}
			modes[nModes].Width = dm.Width;
			modes[nModes].Height = dm.Height;
			modes[nModes].Format = dm.Format;
			modes[nModes].RefreshRate = 0;
			nModes++;
			uint32_t f;
			for (f = 0; f < nFmts; f++) {
				if (dm.Format == fmts[f]) {
					break;
				}
			}
			if (f == nFmts && nFmts < 20) {
				fmts[nFmts++] = dm.Format;
			}
		}
		crt_qsort(modes, nModes, sizeof(D3DDisplayMode), Game_Ptr(0x276090));	/* D3DApp_SortModes */
		for (int t = 0; t < 2; t++) {
			D3DDeviceInfo *dev = &ad->devices[ad->dwNumDevices];
			dev->DeviceType = types[t];
			d3d = MOTOR_D3D(self);
			D3D_METHOD(d3d, d3dGetDeviceCaps_t, D3D8_GETDEVICECAPS)(d3d, a, types[t], dev->d3dCaps);
			dev->dwNumModes = 0;
			dev->dwCurrentMode = 0;
			dev->bCanDoWindowed = 0;
			dev->strDesc = names[t];
			dev->bWindowed = 0;
			dev->MultiSampleType = 0;
			for (uint32_t f = 0; f < nFmts; f++) {
				uint32_t fmt = fmts[f];
				fmtOk[f] = 0;
				depth[f] = 0;
				d3d = MOTOR_D3D(self);
				if (D3D_METHOD(d3d, d3dCheckDeviceType_t, D3D8_CHECKDEVICETYPE)(d3d, a, dev->DeviceType, fmt, fmt, 0) < 0) {
					continue;
				}
				if (dev->DeviceType == D3DDEVTYPE_HAL) {
					halExists = 1;
					if (dev->d3dCaps[3] & D3DCAPS2_CANRENDERWINDOWED) {
						halWindowed = 1;
						if (f == 0) {
							halDesktop = 1;
						}
					}
				}
				d3dappConfirm_t confirm = (d3dappConfirm_t)(*(void ***)self)[D3DAPP_VT_CONFIRMDEVICE / 4];
				if (dev->d3dCaps[7] & D3DDEVCAPS_HWTRANSFORMANDLIGHT) {
					if (dev->d3dCaps[7] & D3DDEVCAPS_PUREDEVICE) {
						behavior[f] = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE;
						if (confirm(self, dev->d3dCaps, behavior[f], fmt) >= 0) {
							fmtOk[f] = 1;
						}
					}
					if (!fmtOk[f]) {
						behavior[f] = D3DCREATE_HARDWARE_VERTEXPROCESSING;
						if (confirm(self, dev->d3dCaps, behavior[f], fmt) >= 0) {
							fmtOk[f] = 1;
						}
						if (!fmtOk[f]) {
							behavior[f] = D3DCREATE_MIXED_VERTEXPROCESSING;
							if (confirm(self, dev->d3dCaps, behavior[f], fmt) >= 0) {
								fmtOk[f] = 1;
							}
						}
					}
				}
				if (!fmtOk[f]) {
					behavior[f] = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
					if (confirm(self, dev->d3dCaps, behavior[f], fmt) >= 0) {
						fmtOk[f] = 1;
					}
					if (!fmtOk[f]) {
						continue;
					}
				}
				if (!MOTOR_USEDEPTH(self)) {
					continue;
				}
				if (!CD3DApp_FindDepthStencilFormat(self, a, dev->DeviceType, fmt, &depth[f])) {
					fmtOk[f] = 0;
				} else if (dev->DeviceType == D3DDEVTYPE_HAL) {
					const char *ds = D3DFmt_Name((int)depth[f]);
					E3D_LogDebug(g_FileLog, "Backbuffer format valid: %-8s %-8s", D3DFmt_Name((int)fmt), ds);
				}
			}
			/* the modes of the formats the device takes */
			for (uint32_t m = 0; m < nModes; m++) {
				for (uint32_t f = 0; f < nFmts; f++) {
					if (modes[m].Format != fmts[f] || fmtOk[f] != 1 || (uint32_t)dev->dwNumModes >= MAX_D3DMODES) {
						continue;
					}
					D3DModeInfo *mi = &dev->modes[dev->dwNumModes];
					mi->Width = modes[m].Width;
					mi->Height = modes[m].Height;
					mi->Format = modes[m].Format;
					mi->dwBehavior = behavior[f];
					mi->DepthStencilFormat = depth[f];
					dev->dwNumModes++;
					if (dev->DeviceType == D3DDEVTYPE_HAL) {
						halSample = 1;
					}
				}
			}
			/* the default mode: 640x480, then the size asked for, then that size in the bits asked for */
			uint32_t n = (uint32_t)dev->dwNumModes;
			for (uint32_t m = 0; m < n; m++) {
				if (dev->modes[m].Width == 640 && dev->modes[m].Height == 480) {
					dev->dwCurrentMode = (int)m;
					if (dev->modes[m].Format == D3DFMT_X8R8G8B8 || dev->modes[m].Format == D3DFMT_A8R8G8B8) {
						break;
					}
				}
			}
			for (uint32_t m = 0; m < n; m++) {
				if (dev->modes[m].Width == (uint32_t)MOTOR_WIDTH(self) &&
					dev->modes[m].Height == (uint32_t)MOTOR_HEIGHT(self)) {
					dev->dwCurrentMode = (int)m;
					if (dev->modes[m].Format == D3DFMT_X8R8G8B8 || dev->modes[m].Format == D3DFMT_A8R8G8B8) {
						break;
					}
				}
			}
			for (uint32_t m = 0; m < n; m++) {
				if (dev->modes[m].Width != (uint32_t)MOTOR_WIDTH(self) ||
					dev->modes[m].Height != (uint32_t)MOTOR_HEIGHT(self)) {
					continue;
				}
				uint32_t f = dev->modes[m].Format;
				if (MOTOR_BPP(self) == 16 ? f == D3DFMT_R5G6B5 : (f == D3DFMT_X8R8G8B8 || f == D3DFMT_A8R8G8B8)) {
					dev->dwCurrentMode = (int)m;
					break;
				}
			}
			if (fmtOk[0] && (dev->d3dCaps[3] & D3DCAPS2_CANRENDERWINDOWED)) {
				dev->bCanDoWindowed = 1;
				dev->bWindowed = 1;
			}
			if (n) {
				ad->dwNumDevices++;
			}
		}
		if (ad->dwNumDevices) {
			MOTOR_NUMADAPTERS(self)++;
		}
	}
	if (!MOTOR_NUMADAPTERS(self)) {
		return (int)D3DAPPERR_NOCOMPATIBLEDEVICES;
	}
	D3DAdapterInfo *ads = (D3DAdapterInfo *)MOTOR_ADAPTERS(self);
	if (MOTOR_FULLSCREEN(self)) {
		for (uint32_t a = 0; a < MOTOR_NUMADAPTERS(self); a++) {
			for (uint32_t d = 0; d < ads[a].dwNumDevices; d++) {
				if (ads[a].devices[d].DeviceType == D3DDEVTYPE_HAL) {
					ads[a].devices[d].bWindowed = 0;
					ads[a].dwCurrentDevice = (int)d;
					MOTOR_ADAPTER(self) = (int)a;
					MOTOR_WINDOWED(self) = 0;
					return 0;
				}
			}
		}
	}
	for (uint32_t a = 0; a < MOTOR_NUMADAPTERS(self); a++) {
		for (uint32_t d = 0; d < ads[a].dwNumDevices; d++) {
			if (!ads[a].devices[d].bWindowed) {
				continue;
			}
			ads[a].dwCurrentDevice = (int)d;
			MOTOR_ADAPTER(self) = (int)a;
			MOTOR_WINDOWED(self) = 1;
			if (ads[a].devices[d].DeviceType == D3DDEVTYPE_REF) {
				int why;
				if (!halExists) {
					why = (int)D3DAPPERR_NOHARDWAREDEVICE;
				} else if (!halSample) {
					why = (int)D3DAPPERR_HALNOTCOMPATIBLE;
				} else if (!halWindowed) {
					why = (int)D3DAPPERR_NOWINDOWEDHAL;
				} else if (!halDesktop) {
					why = (int)D3DAPPERR_NODESKTOPHAL;
				} else {
					why = (int)D3DAPPERR_NOHALTHISMODE;
				}
				CD3DApp_DisplayErrorMsg(self, why, 2);
			}
			return 0;
		}
	}
	return (int)D3DAPPERR_NOWINDOWABLEDEVICES;
}

int __thiscall CD3DApp_Initialize3DEnvironment(byte *self) {
	D3DAdapterInfo *ad = MOTOR_ADAPTERINFO(self);
	D3DDeviceInfo *dev = &ad->devices[ad->dwCurrentDevice];
	D3DModeInfo *mode = &dev->modes[dev->dwCurrentMode];
	uint32_t *pp = MOTOR_PP(self);
	int hr;

	MOTOR_CALL(self, D3DAPP_VT_ADJUSTWINDOW);
	memset(pp, 0, 0x34);
	pp[7] = (uint32_t)dev->bWindowed;		/* Windowed */
	pp[3] = 1;					/* BackBufferCount */
	pp[4] = dev->MultiSampleType;
	pp[8] = MOTOR_USEDEPTH(self);			/* EnableAutoDepthStencil */
	pp[5] = 1;					/* D3DSWAPEFFECT_DISCARD */
	pp[9] = mode->DepthStencilFormat;
	pp[6] = (uint32_t)(uintptr_t)MOTOR_HWND(self);
	if (MOTOR_WINDOWED(self)) {
		pp[0] = (uint32_t)(MOTOR_CLIENTRECT(self)->right - MOTOR_CLIENTRECT(self)->left);
		pp[1] = (uint32_t)(MOTOR_CLIENTRECT(self)->bottom - MOTOR_CLIENTRECT(self)->top);
		pp[2] = ad->desktopMode[3];
		pp[12] = 0;
	} else {
		pp[0] = mode->Width;
		pp[1] = mode->Height;
		pp[2] = mode->Format;
		pp[12] = MOTOR_VSYNC(self) ? 0 : 0x80000000;	/* D3DPRESENT_INTERVAL_IMMEDIATE without vsync */
	}
	void *d3d = MOTOR_D3D(self);
	hr = D3D_METHOD(d3d, d3dCreateDevice_t, D3D8_CREATEDEVICE)(d3d, (uint32_t)MOTOR_ADAPTER(self), dev->DeviceType,
		MOTOR_HWNDFOCUS(self), mode->dwBehavior, pp, &MOTOR_DEV(self));
	if (hr >= 0) {
		if (MOTOR_WINDOWED(self)) {
			exe_rect *r = MOTOR_WINRECT(self);
			exe_SetWindowPos(MOTOR_HWND(self), (void *)-2, r->left, r->top, r->right - r->left, r->bottom - r->top,
				0x40);
		}
		void *d = MOTOR_DEV(self);
		D3D_METHOD(d, devGetDeviceCaps_t, D3DDEV_GETDEVICECAPS)(d, MOTOR_D3DCAPS(self));
		MOTOR_BEHAVIOR(self) = mode->dwBehavior;
		char *stats = MOTOR_DEVICESTATS(self);
		if (dev->DeviceType == D3DDEVTYPE_REF) {
			Str_Copy(stats, "REF", D3DAPP_STATS);
		} else if (dev->DeviceType == D3DDEVTYPE_HAL) {
			Str_Copy(stats, "HAL", D3DAPP_STATS);
		} else if (dev->DeviceType == D3DDEVTYPE_SW) {
			Str_Copy(stats, "SW", D3DAPP_STATS);
		}
		uint32_t b = mode->dwBehavior;
		int hal = dev->DeviceType == D3DDEVTYPE_HAL;
		if ((b & 0x50) == 0x50) {
			D3DApp_Cat(stats, D3DAPP_STATS, hal ? " (pure hw vp)" : " (simulated pure hw vp)");
		} else if (b & D3DCREATE_HARDWARE_VERTEXPROCESSING) {
			D3DApp_Cat(stats, D3DAPP_STATS, hal ? " (hw vp)" : " (simulated hw vp)");
		} else if (b & D3DCREATE_MIXED_VERTEXPROCESSING) {
			D3DApp_Cat(stats, D3DAPP_STATS, hal ? " (mixed vp)" : " (simulated mixed vp)");
		} else if (b & D3DCREATE_SOFTWARE_VERTEXPROCESSING) {
			D3DApp_Cat(stats, D3DAPP_STATS, " (sw vp)");
		}
		if (dev->DeviceType == D3DDEVTYPE_HAL) {
			D3DApp_Cat(stats, D3DAPP_STATS, ": ");
			D3DApp_Cat(stats, D3DAPP_STATS, ad->Description);
		}
		CD3DApp_UpdateBackBufferDesc(self);
		hr = MOTOR_CALL(self, D3DAPP_VT_INITDEVICEOBJECTS);
		if (hr >= 0) {
			hr = MOTOR_CALL(self, D3DAPP_VT_RESTOREDEVICEOBJECTS);
			if (hr >= 0) {
				MOTOR_ACTIVE(self) = 1;
				return 0;
			}
		}
		MOTOR_CALL(self, D3DAPP_VT_INVALIDATEDEVICEOBJECTS);
		MOTOR_CALL(self, D3DAPP_VT_DELETEDEVICEOBJECTS);
		if (MOTOR_DEV(self)) {
			COM_RELEASE(MOTOR_DEV(self));
			MOTOR_DEV(self) = NULL;
		}
	}
	/* a failed HAL: the error shown, then the first REF device of the first adapter */
	if (dev->DeviceType == D3DDEVTYPE_HAL) {
		CD3DApp_DisplayErrorMsg(self, hr, 1);
		MOTOR_ADAPTER(self) = 0;
		D3DAdapterInfo *a0 = (D3DAdapterInfo *)MOTOR_ADAPTERS(self);
		for (uint32_t d = 0; d < a0->dwNumDevices; d++) {
			if (a0->devices[d].DeviceType == D3DDEVTYPE_REF) {
				a0->dwCurrentDevice = (int)d;
				MOTOR_WINDOWED(self) = a0->devices[d].bWindowed;
				break;
			}
		}
		if (a0->devices[a0->dwCurrentDevice].DeviceType == D3DDEVTYPE_REF) {
			hr = CD3DApp_Initialize3DEnvironment(self);
		}
	}
	return hr;
}

intptr_t __thiscall CD3DApp_MsgProc(byte *self, void *hwnd, uint32_t msg, uintptr_t w, intptr_t l) {
	switch (msg) {
	case 0x5:			/* WM_SIZE */
		MOTOR_ACTIVE(self) = !(w == 4 || w == 1);	/* SIZE_MAXHIDE, SIZE_MINIMIZED */
		break;
	case 0xf:			/* WM_PAINT */
		if (MOTOR_DEV(self) && !MOTOR_READY(self) && MOTOR_WINDOWED(self)) {
			void *d = MOTOR_DEV(self);
			D3D_METHOD(d, devPresent_t, D3DDEV_PRESENT)(d, NULL, NULL, NULL, NULL);
		}
		break;
	case 0x10:			/* WM_CLOSE */
		CD3DApp_Cleanup(self);
		exe_DestroyMenu(exe_GetMenu(hwnd));
		exe_DestroyWindow(hwnd);
		exe_PostQuitMessage(0);
		return 0;
	case 0x20:			/* WM_SETCURSOR */
		if (MOTOR_ACTIVE(self) && MOTOR_READY(self) && !MOTOR_WINDOWED(self)) {
			exe_SetCursor(NULL);
			if (MOTOR_SHOWCURSOR(self)) {
				void *d = MOTOR_DEV(self);
				D3D_METHOD(d, devShowCursor_t, D3DDEV_SHOWCURSOR)(d, 1);
			}
			return 1;
		}
		break;
	case 0x24:			/* WM_GETMINMAXINFO: ptMinTrackSize 100 x 100 */
		((int *)l)[6] = 100;
		((int *)l)[7] = 100;
		return exe_DefWindowProcA(hwnd, msg, w, l);
	case 0x84:			/* WM_NCHITTEST */
		if (!MOTOR_WINDOWED(self)) {
			return 1;
		}
		break;
	case 0x111:			/* WM_COMMAND */
		switch (w & 0xffff) {
		case 0x9c42:		/* IDM_CHANGEDEVICE */
			if (!MOTOR_ACTIVE(self) || !MOTOR_READY(self)) {
				return 0;
			}
			D3DAPP_PAUSE(self, 1);
			if (CD3DApp_UserSelectNewDevice(self) < 0) {
				return 0;
			}
			D3DAPP_PAUSE(self, 0);
			return 0;
		case 0x9c43:		/* IDM_TOGGLEFULLSCREEN */
			if (!MOTOR_ACTIVE(self) || !MOTOR_READY(self)) {
				return 0;
			}
			if (CD3DApp_ToggleFullscreen(self) < 0) {
				CD3DApp_DisplayErrorMsg(self, (int)D3DAPPERR_RESIZEFAILED, 1);
			}
			return 0;
		case 0x9c44: {		/* IDM_TOGGLESTART */
			int moving = (int)MOTOR_FRAMEMOVING(self);
			MOTOR_FRAMEMOVING(self) = !moving;
			DXUtil_Timer(moving ? DXUTIL_TIMER_STOP : DXUTIL_TIMER_START);
			break;
		}
		case 0x9c45:		/* IDM_SINGLESTEP */
			DXUtil_Timer(MOTOR_FRAMEMOVING(self) ? DXUTIL_TIMER_STOP : DXUTIL_TIMER_ADVANCE);
			MOTOR_FRAMEMOVING(self) = 0;
			MOTOR_SINGLESTEP(self) = 1;
			break;
		case 0x9c46:		/* IDM_EXIT */
			exe_SendMessageA(hwnd, 0x10, 0, 0);
			return 0;
		}
		break;
	case 0x112:			/* WM_SYSCOMMAND: no moving, sizing, maximizing, menu and power off when fullscreen */
		if ((w == 0xf010 || w == 0xf000 || w == 0xf030 || w == 0xf100 || w == 0xf170) && !MOTOR_WINDOWED(self)) {
			return 1;
		}
		break;
	case 0x200:			/* WM_MOUSEMOVE */
		if (MOTOR_ACTIVE(self) && MOTOR_READY(self) && MOTOR_DEV(self)) {
			exe_point pt;
			exe_GetCursorPos(&pt);
			exe_ScreenToClient(MOTOR_HWND(self), &pt);
			void *d = MOTOR_DEV(self);
			D3D_METHOD(d, devSetCursorPosition_t, D3DDEV_SETCURSORPOSITION)(d, pt.x, pt.y, 0);
		}
		break;
	case 0x211:			/* WM_ENTERMENULOOP */
		D3DAPP_PAUSE(self, 1);
		break;
	case 0x212:			/* WM_EXITMENULOOP */
		D3DAPP_PAUSE(self, 0);
		break;
	case 0x218:			/* WM_POWERBROADCAST */
		if (w == 0) {		/* PBT_APMQUERYSUSPEND */
			return 0x424d5144;	/* BROADCAST_QUERY_DENY */
		}
		if (w == 7) {		/* PBT_APMRESUMESUSPEND */
			return 1;
		}
		break;
	case 0x231:			/* WM_ENTERSIZEMOVE */
		if (MOTOR_FRAMEMOVING(self)) {
			DXUtil_Timer(DXUTIL_TIMER_STOP);
		}
		break;
	case 0x232:			/* WM_EXITSIZEMOVE: the back buffer of a new client size */
		if (MOTOR_FRAMEMOVING(self)) {
			DXUtil_Timer(DXUTIL_TIMER_START);
		}
		if (MOTOR_ACTIVE(self) && MOTOR_WINDOWED(self)) {
			exe_rect old = *MOTOR_CLIENTRECT(self);
			exe_GetWindowRect(MOTOR_HWND(self), MOTOR_WINRECT(self));
			exe_GetClientRect(MOTOR_HWND(self), MOTOR_CLIENTRECT(self));
			exe_rect *c = MOTOR_CLIENTRECT(self);
			int nw = c->right - c->left;
			if (old.right - old.left != nw || old.bottom - old.top != c->bottom - c->top) {
				MOTOR_READY(self) = 0;
				MOTOR_PP(self)[0] = (uint32_t)nw;
				MOTOR_PP(self)[1] = (uint32_t)(c->bottom - c->top);
				if (CD3DApp_Reset(self) < 0) {
					CD3DApp_DisplayErrorMsg(self, (int)D3DAPPERR_RESIZEFAILED, 1);
					return 0;
				}
				MOTOR_READY(self) = 1;
			}
		}
		break;
	}
	return exe_DefWindowProcA(hwnd, msg, w, l);
}

/* the frames per second (each second) and the frame statistics with the mode and the depth format */
static void D3DApp_FrameStats(byte *self) {
	float t = DXUtil_Timer(DXUTIL_TIMER_GETABSOLUTETIME);
	uint32_t frames = ++g_D3DAppFrames;
	float d = t - g_D3DAppLastTime;
	if (!(d > 1.0f)) {
		return;
	}
	g_D3DAppFrames = 0;
	g_D3DAppLastTime = t;
	MOTOR_FPS(self) = (float)(double)frames / d;
	if (g_pApp->flags & APPF_DEDICATED) {
		return;
	}
	D3DDisplayMode mode;
	void *d3d = MOTOR_D3D(self);
	D3D_METHOD(d3d, d3dGetAdapterDisplayMode_t, D3D8_GETADAPTERDISPLAYMODE)(d3d, (uint32_t)MOTOR_ADAPTER(self), &mode);
	Bprintf(MOTOR_FRAMESTATS(self), D3DAPP_STATS, "%.02f fps (%dx%dx%d)", (double)MOTOR_FPS(self),
		MOTOR_U32(self, 0x2a4c8), MOTOR_U32(self, 0x2a4cc), mode.Format == D3DFMT_X8R8G8B8 ? 32 : 16);
	if (!MOTOR_USEDEPTH(self)) {
		return;
	}
	D3DAdapterInfo *ad = MOTOR_ADAPTERINFO(self);
	D3DDeviceInfo *dev = &ad->devices[ad->dwCurrentDevice];
	const char *s;
	switch (dev->modes[dev->dwCurrentMode].DepthStencilFormat) {
	case 0x47:
		s = " (D32)";
		break;
	case 0x49:
		s = " (D15S1)";
		break;
	case 0x4b:
		s = " (D24S8)";
		break;
	case 0x4d:
		s = " (D24X8)";
		break;
	case 0x4f:
		s = " (D24X4S4)";
		break;
	case 0x50:
		s = " (D16)";
		break;
	default:
		return;
	}
	D3DApp_Cat(MOTOR_FRAMESTATS(self), D3DAPP_STATS, s);
}

/* a lost device reset when it can be; the scene moved (while the time runs or for a single step) and drawn */
int __thiscall CD3DApp_Render(byte *self) {
	int hr = 0;

	/* differs from the original on purpose (no sleeps): no frame while the frame limiter holds it back */
	if (!(g_pApp->flags & APPF_DEDICATED) && !Motor_FrameDue(self)) {
		return 0;
	}
	if (!(g_pApp->flags & APPF_DEDICATED)) {
		void *d = MOTOR_DEV(self);
		hr = D3D_METHOD(d, devTestCooperativeLevel_t, D3DDEV_TESTCOOPERATIVELEVEL)(d);
		if (hr < 0) {
			if (hr == D3DERR_DEVICELOST) {
				return 0;
			}
			if (hr != D3DERR_DEVICENOTRESET) {
				return hr;
			}
			if (MOTOR_WINDOWED(self)) {
				D3DAdapterInfo *ad = (D3DAdapterInfo *)MOTOR_ADAPTERS(self) + MOTOR_ADAPTER(self);
				void *d3d = MOTOR_D3D(self);
				D3D_METHOD(d3d, d3dGetAdapterDisplayMode_t, D3D8_GETADAPTERDISPLAYMODE)(d3d,
					(uint32_t)MOTOR_ADAPTER(self), ad->desktopMode);
				MOTOR_PP(self)[2] = ad->desktopMode[3];
			}
			hr = CD3DApp_Reset(self);
			if (hr < 0) {
				return CD3DApp_DisplayErrorMsg(self, (int)D3DAPPERR_RESIZEFAILED, 1);
			}
			return hr;
		}
	}
	float appTime = DXUtil_Timer(DXUTIL_TIMER_GETAPPTIME);
	float elapsed = DXUtil_Timer(DXUTIL_TIMER_GETELAPSEDTIME);
	if (elapsed == 0.0f && MOTOR_FRAMEMOVING(self)) {
		return 0;
	}
	if (MOTOR_FRAMEMOVING(self) || MOTOR_SINGLESTEP(self)) {
		MOTOR_TIME(self) = appTime;
		MOTOR_ELAPSED(self) = elapsed;
		hr = MOTOR_CALL(self, D3DAPP_VT_FRAMEMOVE);
		if (hr < 0) {
			return hr;
		}
		MOTOR_SINGLESTEP(self) = 0;
	}
	if (!(g_pApp->flags & APPF_DEDICATED)) {
		hr = MOTOR_CALL(self, D3DAPP_VT_RENDER);
		if (hr < 0) {
			return hr;
		}
	}
	if (hr == 1) {
		exe_SendMessageA(MOTOR_HWND(self), 0x10, 0, 0);
		return 0;
	}
	D3DApp_FrameStats(self);
	if (!(g_pApp->flags & APPF_DEDICATED)) {
		void *d = MOTOR_DEV(self);
		D3D_METHOD(d, devPresent_t, D3DDEV_PRESENT)(d, NULL, NULL, NULL, NULL);
	}
	return 0;
}

/* the message loop of the framework with the frame of CD3DApp_Render inside; until WM_QUIT */
int __thiscall CD3DApp_Run(byte *self) {
	exe_msg msg;
	void *acc = exe_LoadAcceleratorsA(NULL, (const char *)0x71);
	int hr = 0;

	exe_PeekMessageA(&msg, NULL, 0, 0, 0);
	while (msg.message != 0x12) {	/* WM_QUIT */
		/* differs from the original on purpose (no blocking waits): PeekMessage also while inactive, where the original
		 * waited in GetMessage; without a message the inactive app goes round again */
		int got = exe_PeekMessageA(&msg, NULL, 0, 0, 1);
		if (got) {
			if (!exe_TranslateAcceleratorA(MOTOR_HWND(self), acc, &msg)) {
				exe_TranslateMessage(&msg);
				exe_DispatchMessageA(&msg);
			}
			continue;
		}
		if (!MOTOR_ACTIVE(self) || !MOTOR_READY(self)) {
			continue;
		}
		/* differs from the original on purpose (no sleeps): the frame limiter of CMotorApp_Render holds the frame
		 * back here, before the clock and FrameMove, instead of sleeping inside Render */
		if (!(g_pApp->flags & APPF_DEDICATED) && !Motor_FrameDue(self)) {
			continue;
		}
		int r = 0;
		if (!(g_pApp->flags & APPF_DEDICATED)) {
			void *d = MOTOR_DEV(self);
			hr = D3D_METHOD(d, devTestCooperativeLevel_t, D3DDEV_TESTCOOPERATIVELEVEL)(d);
			if (hr < 0) {
				if (hr == D3DERR_DEVICELOST) {
					continue;
				}
				r = hr;
				if (hr == D3DERR_DEVICENOTRESET) {
					if (MOTOR_WINDOWED(self)) {
						D3DAdapterInfo *ad = (D3DAdapterInfo *)MOTOR_ADAPTERS(self) + MOTOR_ADAPTER(self);
						void *d3d = MOTOR_D3D(self);
						D3D_METHOD(d3d, d3dGetAdapterDisplayMode_t, D3D8_GETADAPTERDISPLAYMODE)(d3d,
							(uint32_t)MOTOR_ADAPTER(self), ad->desktopMode);
						MOTOR_PP(self)[2] = ad->desktopMode[3];
					}
					r = CD3DApp_Reset(self);
					hr = r;
					if (r < 0) {
						CD3DApp_DisplayErrorMsg(self, (int)D3DAPPERR_RESIZEFAILED, 1);
					}
				}
				goto failed;
			}
		}
		float appTime = DXUtil_Timer(DXUTIL_TIMER_GETAPPTIME);
		float elapsed = DXUtil_Timer(DXUTIL_TIMER_GETELAPSEDTIME);
		if (elapsed == 0.0f && MOTOR_FRAMEMOVING(self)) {
			continue;
		}
		if (MOTOR_FRAMEMOVING(self) || MOTOR_SINGLESTEP(self)) {
			MOTOR_TIME(self) = appTime;
			MOTOR_ELAPSED(self) = elapsed;
			hr = MOTOR_CALL(self, D3DAPP_VT_FRAMEMOVE);
			r = hr;
			if (hr < 0) {
				goto failed;
			}
			MOTOR_SINGLESTEP(self) = 0;
		}
		if (!(g_pApp->flags & APPF_DEDICATED)) {
			hr = MOTOR_CALL(self, D3DAPP_VT_RENDER);
			r = hr;
			if (hr < 0) {
				goto failed;
			}
		}
		if (hr == 1) {
			exe_SendMessageA(MOTOR_HWND(self), 0x10, 0, 0);
			continue;
		}
		D3DApp_FrameStats(self);
		if (!(g_pApp->flags & APPF_DEDICATED)) {
			void *d = MOTOR_DEV(self);
			D3D_METHOD(d, devPresent_t, D3DDEV_PRESENT)(d, NULL, NULL, NULL, NULL);
		}
		continue;
failed:
		/* the first error ends the loop through WM_CLOSE */
		if (r < 0) {
			exe_SendMessageA(MOTOR_HWND(self), 0x10, 0, 0);
		}
	}
	return (int)msg.wParam;
}

/* the choice of the device dialog: the original values and those selected */
#define g_SelApp GAME_DATA(byte *, 0x4b1c08)
#define g_OrigAdapter GAME_DATA(uint32_t, 0x4b1c0c)
#define g_SelAdapter GAME_DATA(uint32_t, 0x4b1c10)
#define g_OrigDevice GAME_DATA(uint32_t, 0x4b1c14)
#define g_SelDevice GAME_DATA(uint32_t, 0x4b1c18)
#define g_OrigMode GAME_DATA(uint32_t, 0x4b1c1c)
#define g_SelMode GAME_DATA(uint32_t, 0x4b1c20)
#define g_OrigWindowed GAME_DATA(int, 0x4b1c24)
#define g_SelWindowed GAME_DATA(int, 0x4b1c28)
#define g_OrigMultiSample GAME_DATA(uint32_t, 0x4b1c2c)
#define g_SelMultiSample GAME_DATA(uint32_t, 0x4b1c30)

typedef int(__cdecl *wsprintf_t)(char *buf, const char *fmt, ...);
#define exe_wsprintfA GAME_DATA(wsprintf_t, 0x3a1368)

#define IDC_ADAPTER_COMBO 0x3ea
#define IDC_DEVICE_COMBO 0x3e8
#define IDC_FULLSCREENMODES_COMBO 0x3e9
#define IDC_WINDOW 0x3f4
#define IDC_FULLSCREEN 0x3f8
#define IDC_MULTISAMPLE_COMBO 0x3eb
#define CB_ADDSTRING 0x143
#define CB_RESETCONTENT 0x14b
#define CB_GETCOUNT 0x146
#define CB_GETCURSEL 0x147
#define CB_SETCURSEL 0x14e
#define CB_GETITEMDATA 0x150
#define CB_SETITEMDATA 0x151
#define BM_GETCHECK 0xf0
#define BM_SETCHECK 0xf1

/* a combo box entry with its item data, the selection on it when sel */
static void D3DApp_ComboAdd(void *combo, const char *text, uint32_t data, int sel) {
	intptr_t i = exe_SendMessageA(combo, CB_ADDSTRING, 0, (intptr_t)text);
	exe_SendMessageA(combo, CB_SETITEMDATA, (uintptr_t)i, (intptr_t)data);
	if (sel) {
		exe_SendMessageA(combo, CB_SETCURSEL, (uintptr_t)i, 0);
	}
}

static D3DAdapterInfo *D3DApp_SelAdapter(void) {
	return (D3DAdapterInfo *)MOTOR_ADAPTERS(g_SelApp) + g_SelAdapter;
}

intptr_t __stdcall D3DApp_SelectDeviceProc(void *dlg, uint32_t msg, uintptr_t w, intptr_t l) {
	void *hAdapter = exe_GetDlgItem(dlg, IDC_ADAPTER_COMBO);
	void *hDevice = exe_GetDlgItem(dlg, IDC_DEVICE_COMBO);
	void *hMode = exe_GetDlgItem(dlg, IDC_FULLSCREENMODES_COMBO);
	void *hWindowed = exe_GetDlgItem(dlg, IDC_WINDOW);
	void *hFullscreen = exe_GetDlgItem(dlg, IDC_FULLSCREEN);
	void *hMultiSample = exe_GetDlgItem(dlg, IDC_MULTISAMPLE_COMBO);
	char buf[80], ms[52];

	if (msg == 0x110) {		/* WM_INITDIALOG: the application in lParam */
		byte *app = (byte *)l;
		g_SelApp = app;
		D3DAdapterInfo *ad = (D3DAdapterInfo *)MOTOR_ADAPTERS(app) + MOTOR_ADAPTER(app);
		g_OrigAdapter = g_SelAdapter = (uint32_t)MOTOR_ADAPTER(app);
		D3DDeviceInfo *dev = &ad->devices[ad->dwCurrentDevice];
		g_OrigDevice = g_SelDevice = (uint32_t)ad->dwCurrentDevice;
		g_OrigMode = g_SelMode = (uint32_t)dev->dwCurrentMode;
		g_OrigWindowed = g_SelWindowed = dev->bWindowed;
		g_OrigMultiSample = g_SelMultiSample = dev->MultiSampleType;
	} else if (msg == 0x111) {	/* WM_COMMAND */
		g_SelWindowed = (int)exe_SendMessageA(hWindowed, BM_GETCHECK, 0, 0);
		uint16_t id = (uint16_t)w;
		if (id == 1) {		/* IDOK: unchanged is a cancel */
			if (g_SelAdapter == g_OrigAdapter && g_SelDevice == g_OrigDevice && g_SelMode == g_OrigMode &&
				g_SelWindowed == g_OrigWindowed && g_SelMultiSample == g_OrigMultiSample) {
				exe_EndDialog(dlg, 2);
				return 1;
			}
			MOTOR_ADAPTER(g_SelApp) = (int)g_SelAdapter;
			D3DAdapterInfo *ad = D3DApp_SelAdapter();
			ad->dwCurrentDevice = (int)g_SelDevice;
			D3DDeviceInfo *dev = &ad->devices[g_SelDevice];
			dev->dwCurrentMode = (int)g_SelMode;
			dev->bWindowed = g_SelWindowed;
			dev->MultiSampleType = g_SelMultiSample;
			exe_EndDialog(dlg, 1);
			return 1;
		}
		if (id == 2) {		/* IDCANCEL */
			exe_EndDialog(dlg, 2);
			return 1;
		}
		if ((uint16_t)(w >> 16) == 9) {	/* CBN_SELENDOK */
			if (id == IDC_ADAPTER_COMBO) {
				g_SelAdapter = (uint32_t)exe_SendMessageA(hAdapter, CB_GETCURSEL, 0, 0);
				D3DAdapterInfo *ad = D3DApp_SelAdapter();
				g_SelDevice = (uint32_t)ad->dwCurrentDevice;
				g_SelMode = (uint32_t)ad->devices[g_SelDevice].dwCurrentMode;
				g_SelWindowed = ad->devices[g_SelDevice].bWindowed;
			} else if (id == IDC_DEVICE_COMBO) {
				D3DAdapterInfo *ad = D3DApp_SelAdapter();
				g_SelDevice = (uint32_t)exe_SendMessageA(hDevice, CB_GETCURSEL, 0, 0);
				g_SelMode = (uint32_t)ad->devices[g_SelDevice].dwCurrentMode;
				g_SelWindowed = ad->devices[g_SelDevice].bWindowed;
			} else if (id == IDC_FULLSCREENMODES_COMBO) {
				g_SelMode = (uint32_t)exe_SendMessageA(hMode, CB_GETCURSEL, 0, 0);
			} else if (id == IDC_MULTISAMPLE_COMBO) {
				intptr_t i = exe_SendMessageA(hMultiSample, CB_GETCURSEL, 0, 0);
				g_SelMultiSample = (uint32_t)exe_SendMessageA(hMultiSample, CB_GETITEMDATA, (uintptr_t)i, 0);
			}
		}
	} else {
		return 0;
	}
	/* the lists filled again for the selection */
	exe_SendMessageA(hAdapter, CB_RESETCONTENT, 0, 0);
	exe_SendMessageA(hDevice, CB_RESETCONTENT, 0, 0);
	exe_SendMessageA(hMode, CB_RESETCONTENT, 0, 0);
	exe_SendMessageA(hMultiSample, CB_RESETCONTENT, 0, 0);
	D3DAdapterInfo *ad = D3DApp_SelAdapter();
	D3DDeviceInfo *dev = &ad->devices[g_SelDevice];
	for (uint32_t a = 0; a < MOTOR_NUMADAPTERS(g_SelApp); a++) {
		D3DApp_ComboAdd(hAdapter, ((D3DAdapterInfo *)MOTOR_ADAPTERS(g_SelApp))[a].Description, a, a == g_SelAdapter);
	}
	for (uint32_t d = 0; d < ad->dwNumDevices; d++) {
		D3DApp_ComboAdd(hDevice, ad->devices[d].strDesc, d, d == g_SelDevice);
	}
	for (uint32_t m = 0; m < (uint32_t)dev->dwNumModes; m++) {
		uint32_t f = dev->modes[m].Format;
		int bpp = (f == D3DFMT_X8R8G8B8 || f == D3DFMT_A8R8G8B8 || f == D3DFMT_R8G8B8) ? 32 : 16;
		Bprintf(buf, sizeof(buf), "%ld x %ld x %ld", dev->modes[m].Width, dev->modes[m].Height, bpp);
		D3DApp_ComboAdd(hMode, buf, m, m == g_SelMode);
	}
	for (uint32_t t = 0; t <= 16; t++) {
		uint32_t fmt = g_SelWindowed ? ((D3DAdapterInfo *)MOTOR_ADAPTERS(g_SelApp))[g_SelAdapter].desktopMode[3] :
			dev->modes[g_SelMode].Format;
		if (t == 1) {
			continue;
		}
		void *d3d = MOTOR_D3D(g_SelApp);
		if (D3D_METHOD(d3d, d3dCheckDeviceMultiSampleType_t, D3D8_CHECKDEVICEMULTISAMPLETYPE)(d3d, g_SelAdapter,
			dev->DeviceType, fmt, g_SelWindowed, t) < 0) {
			continue;
		}
		if (!t) {
			Str_Copy(ms, GAME_STR(str_3b32ec, 0x3b32ec), sizeof(ms));	/* "none" */
		} else {
			exe_wsprintfA(ms, "%d samples", t);
		}
		D3DApp_ComboAdd(hMultiSample, ms, t, t == g_SelMultiSample || t == 0);
	}
	intptr_t i = exe_SendMessageA(hMultiSample, CB_GETCURSEL, 0, 0);
	g_SelMultiSample = (uint32_t)exe_SendMessageA(hMultiSample, CB_GETITEMDATA, (uintptr_t)i, 0);
	exe_EnableWindow(hMultiSample, exe_SendMessageA(hMultiSample, CB_GETCOUNT, 0, 0) > 1);
	exe_EnableWindow(hWindowed, dev->bCanDoWindowed);
	if (g_SelWindowed) {
		exe_SendMessageA(hWindowed, BM_SETCHECK, 1, 0);
		exe_SendMessageA(hFullscreen, BM_SETCHECK, 0, 0);
		exe_EnableWindow(hMode, 0);
	} else {
		exe_SendMessageA(hWindowed, BM_SETCHECK, 0, 0);
		exe_SendMessageA(hFullscreen, BM_SETCHECK, 1, 0);
		exe_EnableWindow(hMode, 1);
	}
	return 1;
}
