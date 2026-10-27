/*
 * The small parts of CD3DApplication (d3dapp.cpp of the DX8 SDK framework, g_Motor) in the engine3d range, see
 * include/scrap/dxutil.h: the constructor and destructor, the window style of a mode change, pause, the back buffer
 * description, the device reset, the window procedure and the helpers of the enumeration. The enumeration, the
 * creation, the frame and the device dialog are still the exe's.
 */
#include "scrap/console.h"
#include "scrap/dxutil.h"
#include "scrap/render.h"
#include "scrap/scrapcamera.h"
#include "scrap/scrapmain.h"
#include "scrap/varios.h"
#include "scrap/win32.h"

#define CD3DAPPLICATION_VFTABLE 0x3dd208
#define D3DAPP_TIMER_START 1
#define D3DAPP_TIMER_STOP 2

#define g_AppPausedCount GAME_DATA(int, 0x4b1c3c)

typedef void *(__stdcall *setcursor_t)(void *cursor);
#define exe_SetCursor GAME_DATA(setcursor_t, 0x3a135c)

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
	*(void **)self = Game_Ptr(CD3DAPPLICATION_VFTABLE);
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
	*(const char **)(self + 0x2a5ac) = Game_Ptr(0x3dd240);	/* m_strWindowTitle "Engine3D [DX8]" */
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
	*(void **)self = Game_Ptr(CD3DAPPLICATION_VFTABLE);
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
		E3D_LogBlue(g_FileLog, Game_Ptr(0x3dd33c), r);	/* "ERROR: 'pd3dDevice->Reset()' failed: %08X" */
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
		E3D_LogBlue(g_FileLog, Game_Ptr(0x3dd30c), r);	/* "ERROR: ResetDevice(): %08X" */
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
			E3D_LogBlue(g_FileLog, Game_Ptr(0x3dd3dc));	/* "A D3D object has a non-zero reference count." */
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
