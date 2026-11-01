/*
 * CD3DApplication (d3dapp.cpp of the DX8 SDK framework, g_Engine) in the engine3d range, see include/scrap/dxutil.h:
 * the constructor and destructor, the enumeration of the adapters, devices and modes, the window and the device
 * creation, the frame, the message procedure and loop, the window style of a mode change, pause, the back buffer
 * description, the device reset and the device dialog.
 */
#include <string.h>

#include "scrap/rdev.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/dxutil.h"
#include "scrap/render.h"
#include "scrap/scrapcamera.h"
#include "scrap/scrapmain.h"
#include "scrap/texture.h"
#include "scrap/misc.h"
#include "scrap/win32.h"

#define CD3DAPPLICATION_VFTABLE GAME_VFTABLE(CD3DApplication_vftable)
#define D3DAPP_TIMER_START 1
#define D3DAPP_TIMER_STOP 2

extern int g_AppPausedCount;


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

intptr_t __stdcall D3DApp_WndProc(void *hwnd, uint32_t msg, uintptr_t w, intptr_t l) {
	CD3DApplication *app = g_Engine;
	return app->vftable->MsgProc(app, hwnd, msg, w, l);
}


/* IDirect3D8::CheckDeviceFormat (as D3DUSAGE_DEPTHSTENCIL of a surface) and CheckDepthStencilMatch */
byte D3DApp_DepthFormatOk(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t ds) {
	if (Rd_D3DCheckDeviceFormat(d3d, adapter, type, fmt, 2, 1, ds) < 0) {
		return 0;
	}
	if (Rd_D3DCheckDepthStencilMatch(d3d, adapter, type, fmt, fmt, ds) < 0) {
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

static byte D3DApp_TryDepth(CD3DApplication *self, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t ds, uint32_t *out) {
	if (!D3DApp_DepthFormatOk(self->d3d, adapter, type, fmt, ds)) {
		return 0;
	}
	*out = ds;
	return 1;
}

/* the depth/stencil format for m_dwMinDepthBits and m_dwMinStencilBits, then the depth alone, then D16 */
int CD3DApp_FindDepthStencilFormat(CD3DApplication *self, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t *out) {
	uint32_t depth = self->minDepthBits, stencil = self->minStencilBits;

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

CD3DApplication *CD3DApp_ctor(CD3DApplication *self) {
	self->vftable = CD3DAPPLICATION_VFTABLE;
	g_Engine = self;
	self->numAdapters = 0;
	self->adapter = 0;
	self->d3d = NULL;
	self->device = NULL;
	self->hWnd = NULL;
	self->hWndFocus = NULL;
	self->active = 0;
	self->ready = 0;
	self->createFlags = 0;
	self->frameMoving = 1;
	self->singleStep = 0;
	self->fps = 0;
	self->deviceStats[0] = 0;
	self->frameStats[0] = 0;
	self->devId = 0;
	self->windowTitle = "Engine3D [DX8]";
	self->creationWidth = 400;
	self->creationHeight = 300;
	self->useDepthBuffer = 0;
	self->minDepthBits = 0x10;
	self->minStencilBits = 0;
	self->showCursorWhenFullscreen = 0;
	self->vsync = 1;
	self->fullscreen = 0;
	self->width = 800;
	self->height = 600;
	self->bpp = 32;
	return self;
}

void CD3DApp_dtor(CD3DApplication *self) {
	self->vftable = CD3DAPPLICATION_VFTABLE;
	g_Engine = NULL;
}

/* vf00: the window style (and in fullscreen the size of the mode) after a change */
int CD3DApp_AdjustWindowForChange(CD3DApplication *self) {
	if (self->windowed) {
		exe_SetWindowLongA(self->hWnd, -16, (int32_t)self->windowStyle);
	} else {
		exe_SetWindowLongA(self->hWnd, -16, (int32_t)0x90080000);	/* WS_POPUP | WS_SYSMENU | WS_VISIBLE */
		D3DAdapterInfo *ad = &self->adapters[self->adapter];
		D3DDeviceInfo *dev = &ad->devices[ad->dwCurrentDevice];
		D3DModeInfo *mode = &dev->modes[dev->dwCurrentMode];
		exe_SetWindowPos(self->hWnd, (void *)-1, 0, 0, (int)mode->Width, (int)mode->Height, 0);
	}
	if (!self->windowed) {
		exe_SetCursor(NULL);
	}
	return 0;
}

/* vf13: pause counted over the callers, the timer stopped with the first and started with the last */
void CD3DApp_Pause(CD3DApplication *self, int pause) {
	int n = g_AppPausedCount + (pause ? 1 : -1);

	g_AppPausedCount = n;
	self->ready = n == 0;
	if (pause && n == 1) {
		if (!self->frameMoving) {
			return;
		}
		DXUtil_Timer(D3DAPP_TIMER_STOP);
		n = g_AppPausedCount;
	}
	if (n == 0 && self->frameMoving) {
		DXUtil_Timer(D3DAPP_TIMER_START);
	}
}


/* the descriptions of the back buffer and the depth buffer, the aspect ratio of the back buffer */
void CD3DApp_UpdateBackBufferDesc(CD3DApplication *self) {
	void *surf;
	void *dev = self->device;

	Rd_DevGetBackBuffer(dev, 0, 0, &surf);
	Rd_SurfGetDesc(surf, &self->backBufferDesc);
	Rd_Release(surf);
	memset(&self->depthBufferDesc, 0, sizeof(self->depthBufferDesc));
	if (self->useDepthBuffer) {
		dev = self->device;
		Rd_DevGetDepthStencilSurface(dev, &surf);
		Rd_SurfGetDesc(surf, &self->depthBufferDesc);
		Rd_Release(surf);
	}
	uint32_t w = self->backBufferDesc.Width, h = self->backBufferDesc.Height;
	g_PhysicalAspectRatio = (float)(double)w / (float)(double)h;
	Con_Printf("Aspect: %f (%dx%d)\n", (double)g_PhysicalAspectRatio, (int)self->backBufferDesc.Width, (int)self->backBufferDesc.Height);
	self->devId++;
}


/* without frame moving one frame is stepped: the timer started and stopped */
static void D3DApp_SingleStep(CD3DApplication *self) {
	if (!self->frameMoving) {
		self->singleStep = 1;
		DXUtil_Timer(D3DAPP_TIMER_START);
		DXUtil_Timer(D3DAPP_TIMER_STOP);
	}
}

/* IDirect3DDevice8::Reset with the present parameters, logged when it fails; the gamma ramp set again */
static int D3DApp_DevReset(CD3DApplication *self) {
	void *dev = self->device;
	int r = Rd_DevReset(dev, &self->presentParams);

	if (r < 0) {
		E3D_LogBlue(g_FileLog, "ERROR: 'pd3dDevice->Reset()' failed: %08X", r);	/* "ERROR: 'pd3dDevice->Reset()' failed: %08X" */
	}
	RenderGamma_Begin();
	return r;
}

int CD3DApp_ResetDevice(CD3DApplication *self) {
	return D3DApp_DevReset(self);
}

int CD3DApp_Reset(CD3DApplication *self) {
	int r = self->vftable->InvalidateDeviceObjects(self);
	if (r < 0) {
		return r;
	}
	self->vftable->DeleteDeviceObjects(self);
	r = D3DApp_DevReset(self);
	if (r < 0) {
		E3D_LogBlue(g_FileLog, "ERROR: ResetDevice(): %08X", r);	/* "ERROR: ResetDevice(): %08X" */
	}
	CD3DApp_UpdateBackBufferDesc(self);
	Splash_Reset();
	r = self->vftable->InitDeviceObjects(self);
	if (r < 0) {
		return r;
	}
	r = self->vftable->RestoreDeviceObjects(self);
	if (r < 0) {
		return r;
	}
	D3DApp_SingleStep(self);
	return 0;
}

/* the default virtual methods: nothing to do, S_OK */
int CD3DApp_Nop(CD3DApplication *self) {
	(void)self;
	return 0;
}

/* vf01 ConfirmDevice: every device is fine */
int CD3DApp_ConfirmDevice(CD3DApplication *self, D3DCaps8 *caps, uint32_t behavior, uint32_t fmt) {
	(void)self;
	(void)caps;
	(void)behavior;
	(void)fmt;
	return 0;
}

/* the device released (an error in the log when something still holds it), then FinalCleanup (vtable 0x24) */
int CD3DApp_Cleanup(CD3DApplication *self) {
	self->active = 0;
	self->ready = 0;
	if (self->device) {
		self->vftable->InvalidateDeviceObjects(self);
		self->vftable->DeleteDeviceObjects(self);
		if (Rd_Release(self->device)) {
			E3D_LogBlue(g_FileLog, "A D3D object has a non-zero reference count.");
		}
		Rd_Release(self->d3d);
		self->device = NULL;
		self->d3d = NULL;
	}
	return self->vftable->FinalCleanup(self);
}

static D3DDeviceInfo *D3DApp_CurDevice(CD3DApplication *self) {
	D3DAdapterInfo *ad = &self->adapters[self->adapter];
	return &ad->devices[ad->dwCurrentDevice];
}

/* the device released and the environment made again; 0 and the result of the error shown on failures */
static byte D3DApp_Recreate(CD3DApplication *self, int *err) {
	self->vftable->InvalidateDeviceObjects(self);
	self->vftable->DeleteDeviceObjects(self);
	if (Rd_Release(self->device)) {
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
int CD3DApp_ForceWindowed(CD3DApplication *self) {
	D3DDeviceInfo *dev = D3DApp_CurDevice(self);

	if (!dev->bCanDoWindowed) {
		uint32_t a, d = 0;
		D3DAdapterInfo *ad = self->adapters;
		for (a = 0; a < self->numAdapters; a++, ad++) {
			for (d = 0; d < ad->dwNumDevices && !ad->devices[d].bCanDoWindowed; d++) {
			}
			if (d < ad->dwNumDevices) {
				break;
			}
		}
		if (a >= self->numAdapters) {
			return E_FAIL;
		}
		self->adapter = a;
		ad->dwCurrentDevice = (int)d;
		dev = &ad->devices[d];
	}
	dev->bWindowed = 1;
	self->windowed = 1;
	self->ready = 0;
	int err;
	if (!D3DApp_Recreate(self, &err)) {
		return err;
	}
	self->ready = 1;
	return 0;
}

int CD3DApp_ToggleFullscreen(CD3DApplication *self) {
	D3DAdapterInfo *ad = &self->adapters[self->adapter];
	D3DDeviceInfo *dev = &ad->devices[ad->dwCurrentDevice];
	D3DModeInfo *mode = &dev->modes[dev->dwCurrentMode];
	D3DPresentParams *pp = &self->presentParams;

	if (!self->windowed && !dev->bCanDoWindowed) {
		return CD3DApp_ForceWindowed(self);
	}
	self->ready = 0;
	self->windowed = !self->windowed;
	dev->bWindowed = self->windowed;
	self->vftable->AdjustWindowForChange(self);
	pp->Windowed = dev->bWindowed;
	pp->MultiSampleType = dev->MultiSampleType;
	pp->AutoDepthStencilFormat = mode->DepthStencilFormat;
	pp->hDeviceWindow = self->hWnd;
	if (self->windowed) {
		/* the client rectangle of the window */
		pp->BackBufferWidth = self->windowClient.right - self->windowClient.left;
		pp->BackBufferHeight = self->windowClient.bottom - self->windowClient.top;
		pp->BackBufferFormat = ad->desktopMode.Format;
		pp->FullScreen_PresentationInterval = 0;
	} else {
		pp->BackBufferWidth = mode->Width;
		pp->BackBufferHeight = mode->Height;
		pp->BackBufferFormat = mode->Format;
		pp->FullScreen_PresentationInterval = self->vsync ? 0 : D3DPRESENT_INTERVAL_IMMEDIATE;
	}
	if (CD3DApp_Reset(self) < 0) {
		return CD3DApp_DisplayErrorMsg(self, 0x8200000c, 1);	/* D3DAPPERR_RESIZEFAILED */
	}
	if (self->windowed) {
		/* the window rectangle, not topmost */
		int x = self->windowBounds.left, y = self->windowBounds.top;
		exe_SetWindowPos(self->hWnd, (void *)-2, x, y, self->windowBounds.right - x,
			self->windowBounds.bottom - y, 0x40);
	}
	self->ready = 1;
	return 0;
}

typedef void *(__stdcall *getmodulehandle_t)(const char *name);
#define exe_GetModuleHandleA GAME_IMPORT(getmodulehandle_t, GetModuleHandleA, 4)

int CD3DApp_UserSelectNewDevice(CD3DApplication *self) {
	if (!self->windowed && CD3DApp_ToggleFullscreen(self) < 0) {
		CD3DApp_DisplayErrorMsg(self, 0x8200000c, 1);
		return E_FAIL;
	}
	/* the device dialog (resource 0x90), SelectDeviceProc */
	if (exe_DialogBoxParamA(exe_GetModuleHandleA(NULL), (const char *)0x90, self->hWnd, (void *)D3DApp_SelectDeviceProc,
		(intptr_t)self) != 1) {
		return 0;
	}
	self->windowed = D3DApp_CurDevice(self)->bWindowed;
	int err;
	if (!D3DApp_Recreate(self, &err)) {
		return err;
	}
	D3DApp_SingleStep(self);
	return 0;
}

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
#define D3DCREATE_FPU_PRESERVE 0x2

extern uint32_t g_D3DAppFrames;
extern float g_D3DAppLastTime;

typedef int(__stdcall *adjustwindowrect_t)(exe_rect *r, uint32_t style, int menu);
typedef void *(__stdcall *loadmenu_t)(void *inst, const char *name);
typedef int(__stdcall *setrect_t)(exe_rect *r, int left, int top, int right, int bottom);
typedef int(__stdcall *destroymenu_t)(void *menu);
typedef void(__stdcall *postquitmessage_t)(int code);
typedef int(__stdcall *getcursorpos_t)(exe_point *pt);
typedef int(__stdcall *screentoclient_t)(void *hwnd, exe_point *pt);
typedef int(__stdcall *enablewindow_t)(void *hwnd, int on);
#define exe_AdjustWindowRect GAME_IMPORT(adjustwindowrect_t, AdjustWindowRect, 12)
#define exe_LoadMenuA GAME_IMPORT(loadmenu_t, LoadMenuA, 8)
#define exe_SetRect GAME_IMPORT(setrect_t, SetRect, 20)
#define exe_DestroyMenu GAME_IMPORT(destroymenu_t, DestroyMenu, 4)
#define exe_PostQuitMessage GAME_IMPORT(postquitmessage_t, PostQuitMessage, 4)
#define exe_GetCursorPos GAME_IMPORT(getcursorpos_t, GetCursorPos, 4)
#define exe_ScreenToClient GAME_IMPORT(screentoclient_t, ScreenToClient, 8)
#define exe_EnableWindow GAME_IMPORT(enablewindow_t, EnableWindow, 8)

/* IDirect3D8 and IDirect3DDevice8 */

/* the bounded lstrcatA of the stats strings */
static void D3DApp_Cat(char *dst, int size, const char *src) {
	size_t n = strlen(dst);
	if ((int)n < size - 1) {
		Str_Copy(dst + n, src, size - (int)n);
	}
}

int CD3DApp_Create(CD3DApplication *self, void *inst) {
	exe_wndclass wc;
	exe_rect r;
	int hr;

	self->d3d = Rd_Direct3DCreate8(0xdc);	/* D3D_SDK_VERSION */
	if (!self->d3d) {
		return CD3DApp_DisplayErrorMsg(self, D3DAPPERR_NODIRECT3D, 1);
	}
	hr = CD3DApp_BuildDeviceList(self);
	if (hr < 0) {
		goto fail;
	}
	if (!self->hWnd) {
		wc.style = 0;
		wc.wndProc = (void *)D3DApp_WndProc;	/* WndProc */
		wc.clsExtra = 0;
		wc.wndExtra = 0;
		wc.hInstance = inst;
		wc.hIcon = exe_LoadIconA(inst, (const char *)0x7f00);
		wc.hCursor = exe_LoadCursorA(NULL, (const char *)0x7f00);
		wc.hbrBackground = exe_GetStockObject(0);
		wc.menuName = NULL;
		wc.className = "Class_Engine3D_DX8";
		exe_RegisterClassA(&wc);
		self->windowStyle = 0x90ce0000;	/* WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | ... */
		exe_SetRect(&r, 0, 0, (int)self->creationWidth, (int)self->creationHeight);
		exe_AdjustWindowRect(&r, self->windowStyle, 1);
		void *menu = exe_LoadMenuA(inst, (const char *)0x8d);
		self->hWnd = exe_CreateWindowExA(0, "Class_Engine3D_DX8", self->windowTitle, self->windowStyle,
			(int)0x80000000, (int)0x80000000, r.right - r.left, r.bottom - r.top, NULL, menu, inst, NULL);
	}
	if (!self->hWndFocus) {
		self->hWndFocus = self->hWnd;
	}
	self->windowStyle = (uint32_t)exe_GetWindowLongA(self->hWnd, -16);	/* GWL_STYLE */
	exe_GetWindowRect(self->hWnd, &self->windowBounds);
	exe_GetClientRect(self->hWnd, &self->windowClient);
	hr = self->vftable->OneTimeSceneInit(self);
	if (hr >= 0) {
		hr = CD3DApp_Initialize3DEnvironment(self);
		if (hr >= 0) {
			DXUtil_Timer(DXUTIL_TIMER_START);
			self->ready = 1;
			return 0;
		}
	}
fail:
	if (self->d3d) {
		Rd_Release(self->d3d);
		self->d3d = NULL;
	}
	return CD3DApp_DisplayErrorMsg(self, hr, 1);
}

int CD3DApp_DisplayErrorMsg(CD3DApplication *self, int hr, int type) {
	char msg[0x200];
	const char *s;

	/* the texts of the exe */
	switch ((uint32_t)hr) {
	case D3DAPPERR_NODIRECT3D:
		s = "Could not initialize Direct3D. You may\nwant to check that DirectX 8.1 or better\nis correctly installed on your system.\n";
		break;
	case 0x8007000e:		/* E_OUTOFMEMORY */
		s = GAME_PTR(CD3DApp_DisplayErrorMsg_3dd84c, 0);
		break;
	case 0x8876017c:		/* D3DERR_OUTOFVIDEOMEMORY */
		s = GAME_PTR(CD3DApp_DisplayErrorMsg_3dd860, 0);
		break;
	case D3DAPPERR_NOCOMPATIBLEDEVICES:
		s = "Could not find any compatible Direct3D\ndevices.";
		break;
	case D3DAPPERR_NOWINDOWABLEDEVICES:
		s = "This program cannot run in a desktop\nwindow with the current display settings.\nPlease change your desktop settings to a\n16- or 32-bit display mode and re-run this\nprogram.";
		break;
	case D3DAPPERR_NOHARDWAREDEVICE:
		s = GAME_PTR(CD3DApp_DisplayErrorMsg_3dd5e4, 0);
		break;
	case D3DAPPERR_HALNOTCOMPATIBLE:
		s = "This program requires functionality that is\nnot available on your Direct3D hardware\naccelerator.";
		break;
	case D3DAPPERR_NOWINDOWEDHAL:
		s = GAME_PTR(CD3DApp_DisplayErrorMsg_3dd688, 0);
		break;
	case D3DAPPERR_NODESKTOPHAL:
		s = "Your Direct3D hardware accelerator cannot\nrender into a window with the current\ndesktop display settings.\n";
		break;
	case D3DAPPERR_NOHALTHISMODE:
		s = "This program requires functionality that is\nnot available on your Direct3D hardware\naccelerator with the current desktop display\nsettings.\n";
		break;
	case 0x8200000a:		/* D3DAPPERR_NONZEROREFCOUNT */
		s = "A D3D object has a non-zero reference\ncount (meaning things were not properly\ncleaned up).";
		break;
	case 0x8200000b:		/* D3DAPPERR_MEDIANOTFOUND */
		s = GAME_PTR(CD3DApp_DisplayErrorMsg_3dd7cc, 0);
		break;
	case D3DAPPERR_RESIZEFAILED:
		s = GAME_PTR(CD3DApp_DisplayErrorMsg_3d69f8, 0);
		break;
	default:
		s = GAME_PTR(CD3DApp_DisplayErrorMsg_3dd87c, 0);	/* "Generic application error." */
		break;
	}
	Str_Copy(msg, s, sizeof(msg));
	int fatal = 1;
	if (type != 1) {
		if (self->fullscreen) {
			Err_Printf(1, "%s", msg);
		}
		D3DApp_Cat(msg, sizeof(msg), "\nUsing REF (allowed only in windowed)");
		fatal = 0;
	}
	Err_Printf(fatal, "%s", msg);
	return hr;
}

/* the modes, depth formats and behaviors of the HAL and REF devices of all adapters, then the first fullscreen HAL
 * device (with fullscreen) or the first windowed one; the reasons a windowed REF had to be taken shown */
int CD3DApp_BuildDeviceList(CD3DApplication *self) {
	static const uint32_t types[2] = { D3DDEVTYPE_HAL, D3DDEVTYPE_REF };
	const char *names[2] = { "HAL", "REF" };	/* "HAL", "REF" */
	D3DDisplayMode modes[100];
	uint32_t fmts[20], behavior[20], depth[20];
	int fmtOk[20];
	int halExists = 0, halWindowed = 0, halDesktop = 0, halSample = 0;
	void *d3d = self->d3d;

	for (uint32_t a = 0; a < Rd_D3DGetAdapterCount(d3d);
		a++, d3d = self->d3d) {
		if (self->numAdapters >= MAX_D3DADAPTERS) {
			continue;
		}
		D3DAdapterInfo *ad = &self->adapters[self->numAdapters];
		Rd_D3DGetAdapterIdentifier(d3d, a, 0, &ad->id);
		d3d = self->d3d;
		Rd_D3DGetAdapterDisplayMode(d3d, a, &ad->desktopMode);
		ad->dwNumDevices = 0;
		ad->dwCurrentDevice = 0;
		/* the modes without the refresh rates, and their formats with the desktop one first */
		uint32_t nModes = 0;
		d3d = self->d3d;
		uint32_t count = Rd_D3DGetAdapterModeCount(d3d, a);
		uint32_t nFmts = 1;
		fmts[0] = ad->desktopMode.Format;
		for (uint32_t m = 0; m < count; m++) {
			D3DDisplayMode dm;
			d3d = self->d3d;
			Rd_D3DEnumAdapterModes(d3d, a, m, &dm);
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
		crt_qsort(modes, nModes, sizeof(D3DDisplayMode), (void *)D3DApp_SortModes);	/* D3DApp_SortModes */
		for (int t = 0; t < 2; t++) {
			D3DDeviceInfo *dev = &ad->devices[ad->dwNumDevices];
			dev->DeviceType = types[t];
			d3d = self->d3d;
			Rd_D3DGetDeviceCaps(d3d, a, types[t], &dev->d3dCaps);
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
				d3d = self->d3d;
				if (Rd_D3DCheckDeviceType(d3d, a, dev->DeviceType, fmt, fmt, 0) < 0) {
					continue;
				}
				if (dev->DeviceType == D3DDEVTYPE_HAL) {
					halExists = 1;
					if (dev->d3dCaps.Caps2 & D3DCAPS2_CANRENDERWINDOWED) {
						halWindowed = 1;
						if (f == 0) {
							halDesktop = 1;
						}
					}
				}
				typeof(self->vftable->ConfirmDevice) confirm = self->vftable->ConfirmDevice;
				if (dev->d3dCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) {
					if (dev->d3dCaps.DevCaps & D3DDEVCAPS_PUREDEVICE) {
						behavior[f] = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE;
						if (confirm(self, &dev->d3dCaps, behavior[f], fmt) >= 0) {
							fmtOk[f] = 1;
						}
					}
					if (!fmtOk[f]) {
						behavior[f] = D3DCREATE_HARDWARE_VERTEXPROCESSING;
						if (confirm(self, &dev->d3dCaps, behavior[f], fmt) >= 0) {
							fmtOk[f] = 1;
						}
						if (!fmtOk[f]) {
							behavior[f] = D3DCREATE_MIXED_VERTEXPROCESSING;
							if (confirm(self, &dev->d3dCaps, behavior[f], fmt) >= 0) {
								fmtOk[f] = 1;
							}
						}
					}
				}
				if (!fmtOk[f]) {
					behavior[f] = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
					if (confirm(self, &dev->d3dCaps, behavior[f], fmt) >= 0) {
						fmtOk[f] = 1;
					}
					if (!fmtOk[f]) {
						continue;
					}
				}
				if (!self->useDepthBuffer) {
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
				if (dev->modes[m].Width == (uint32_t)self->width &&
					dev->modes[m].Height == (uint32_t)self->height) {
					dev->dwCurrentMode = (int)m;
					if (dev->modes[m].Format == D3DFMT_X8R8G8B8 || dev->modes[m].Format == D3DFMT_A8R8G8B8) {
						break;
					}
				}
			}
			for (uint32_t m = 0; m < n; m++) {
				if (dev->modes[m].Width != (uint32_t)self->width ||
					dev->modes[m].Height != (uint32_t)self->height) {
					continue;
				}
				uint32_t f = dev->modes[m].Format;
				if (self->bpp == 16 ? f == D3DFMT_R5G6B5 : (f == D3DFMT_X8R8G8B8 || f == D3DFMT_A8R8G8B8)) {
					dev->dwCurrentMode = (int)m;
					break;
				}
			}
			if (fmtOk[0] && (dev->d3dCaps.Caps2 & D3DCAPS2_CANRENDERWINDOWED)) {
				dev->bCanDoWindowed = 1;
				dev->bWindowed = 1;
			}
			if (n) {
				ad->dwNumDevices++;
			}
		}
		if (ad->dwNumDevices) {
			self->numAdapters++;
		}
	}
	if (!self->numAdapters) {
		return (int)D3DAPPERR_NOCOMPATIBLEDEVICES;
	}
	D3DAdapterInfo *ads = self->adapters;
	if (self->fullscreen) {
		for (uint32_t a = 0; a < self->numAdapters; a++) {
			for (uint32_t d = 0; d < ads[a].dwNumDevices; d++) {
				if (ads[a].devices[d].DeviceType == D3DDEVTYPE_HAL) {
					ads[a].devices[d].bWindowed = 0;
					ads[a].dwCurrentDevice = (int)d;
					self->adapter = a;
					self->windowed = 0;
					return 0;
				}
			}
		}
	}
	for (uint32_t a = 0; a < self->numAdapters; a++) {
		for (uint32_t d = 0; d < ads[a].dwNumDevices; d++) {
			if (!ads[a].devices[d].bWindowed) {
				continue;
			}
			ads[a].dwCurrentDevice = (int)d;
			self->adapter = a;
			self->windowed = 1;
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

int CD3DApp_Initialize3DEnvironment(CD3DApplication *self) {
	D3DAdapterInfo *ad = &self->adapters[self->adapter];
	D3DDeviceInfo *dev = &ad->devices[ad->dwCurrentDevice];
	D3DModeInfo *mode = &dev->modes[dev->dwCurrentMode];
	D3DPresentParams *pp = &self->presentParams;
	int hr;

	self->vftable->AdjustWindowForChange(self);
	memset(pp, 0, sizeof(*pp));
	pp->Windowed = dev->bWindowed;
	pp->BackBufferCount = 1;
	pp->MultiSampleType = dev->MultiSampleType;
	pp->EnableAutoDepthStencil = self->useDepthBuffer;
	pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
	pp->AutoDepthStencilFormat = mode->DepthStencilFormat;
	pp->hDeviceWindow = self->hWnd;
	if (self->windowed) {
		pp->BackBufferWidth = (uint32_t)(self->windowClient.right - self->windowClient.left);
		pp->BackBufferHeight = (uint32_t)(self->windowClient.bottom - self->windowClient.top);
		pp->BackBufferFormat = ad->desktopMode.Format;
		pp->FullScreen_PresentationInterval = 0;
	} else {
		pp->BackBufferWidth = mode->Width;
		pp->BackBufferHeight = mode->Height;
		pp->BackBufferFormat = mode->Format;
		pp->FullScreen_PresentationInterval = self->vsync ? 0 : D3DPRESENT_INTERVAL_IMMEDIATE;
	}
	void *d3d = self->d3d;
	/* differs from the original on purpose: the x87 keeps its double precision (the libm runs on the x87; without
	 * FPU_PRESERVE Direct3D sets it to 24 bits and sin, cos ... lose most of their digits) */
	hr = Rd_D3DCreateDevice(d3d, self->adapter, dev->DeviceType,
		self->hWndFocus, mode->dwBehavior | D3DCREATE_FPU_PRESERVE, pp, &self->device);
	if (hr >= 0) {
		if (self->windowed) {
			exe_rect *r = &self->windowBounds;
			exe_SetWindowPos(self->hWnd, (void *)-2, r->left, r->top, r->right - r->left, r->bottom - r->top,
				0x40);
		}
		void *d = self->device;
		Rd_DevGetDeviceCaps(d, &self->caps);
		self->createFlags = mode->dwBehavior;
		char *stats = self->deviceStats;
		if (dev->DeviceType == D3DDEVTYPE_REF) {
			Str_Copy(stats, "REF", sizeof(self->deviceStats));
		} else if (dev->DeviceType == D3DDEVTYPE_HAL) {
			Str_Copy(stats, "HAL", sizeof(self->deviceStats));
		} else if (dev->DeviceType == D3DDEVTYPE_SW) {
			Str_Copy(stats, "SW", sizeof(self->deviceStats));
		}
		uint32_t b = mode->dwBehavior;
		int hal = dev->DeviceType == D3DDEVTYPE_HAL;
		if ((b & 0x50) == 0x50) {
			D3DApp_Cat(stats, sizeof(self->deviceStats), hal ? " (pure hw vp)" : " (simulated pure hw vp)");
		} else if (b & D3DCREATE_HARDWARE_VERTEXPROCESSING) {
			D3DApp_Cat(stats, sizeof(self->deviceStats), hal ? " (hw vp)" : " (simulated hw vp)");
		} else if (b & D3DCREATE_MIXED_VERTEXPROCESSING) {
			D3DApp_Cat(stats, sizeof(self->deviceStats), hal ? " (mixed vp)" : " (simulated mixed vp)");
		} else if (b & D3DCREATE_SOFTWARE_VERTEXPROCESSING) {
			D3DApp_Cat(stats, sizeof(self->deviceStats), " (sw vp)");
		}
		if (dev->DeviceType == D3DDEVTYPE_HAL) {
			D3DApp_Cat(stats, sizeof(self->deviceStats), ": ");
			D3DApp_Cat(stats, sizeof(self->deviceStats), ad->id.Description);
		}
		CD3DApp_UpdateBackBufferDesc(self);
		hr = self->vftable->InitDeviceObjects(self);
		if (hr >= 0) {
			hr = self->vftable->RestoreDeviceObjects(self);
			if (hr >= 0) {
				self->active = 1;
				return 0;
			}
		}
		self->vftable->InvalidateDeviceObjects(self);
		self->vftable->DeleteDeviceObjects(self);
		if (self->device) {
			Rd_Release(self->device);
			self->device = NULL;
		}
	}
	/* a failed HAL: the error shown, then the first REF device of the first adapter */
	if (dev->DeviceType == D3DDEVTYPE_HAL) {
		CD3DApp_DisplayErrorMsg(self, hr, 1);
		self->adapter = 0;
		D3DAdapterInfo *a0 = self->adapters;
		for (uint32_t d = 0; d < a0->dwNumDevices; d++) {
			if (a0->devices[d].DeviceType == D3DDEVTYPE_REF) {
				a0->dwCurrentDevice = (int)d;
				self->windowed = a0->devices[d].bWindowed;
				break;
			}
		}
		if (a0->devices[a0->dwCurrentDevice].DeviceType == D3DDEVTYPE_REF) {
			hr = CD3DApp_Initialize3DEnvironment(self);
		}
	}
	return hr;
}

intptr_t CD3DApp_MsgProc(CD3DApplication *self, void *hwnd, uint32_t msg, uintptr_t w, intptr_t l) {
	switch (msg) {
	case 0x5:			/* WM_SIZE */
		self->active = !(w == 4 || w == 1);	/* SIZE_MAXHIDE, SIZE_MINIMIZED */
		break;
	case 0xf:			/* WM_PAINT */
		if (self->device && !self->ready && self->windowed) {
			void *d = self->device;
			Rd_DevPresent(d, NULL, NULL, NULL, NULL);
		}
		break;
	case 0x10:			/* WM_CLOSE */
		CD3DApp_Cleanup(self);
		exe_DestroyMenu(exe_GetMenu(hwnd));
		exe_DestroyWindow(hwnd);
		exe_PostQuitMessage(0);
		return 0;
	case 0x20:			/* WM_SETCURSOR */
		if (self->active && self->ready && !self->windowed) {
			exe_SetCursor(NULL);
			if (self->showCursorWhenFullscreen) {
				void *d = self->device;
				Rd_DevShowCursor(d, 1);
			}
			return 1;
		}
		break;
	case 0x24:			/* WM_GETMINMAXINFO: ptMinTrackSize 100 x 100 */
		((int *)l)[6] = 100;
		((int *)l)[7] = 100;
		return exe_DefWindowProcA(hwnd, msg, w, l);
	case 0x84:			/* WM_NCHITTEST */
		if (!self->windowed) {
			return 1;
		}
		break;
	case 0x111:			/* WM_COMMAND */
		switch (w & 0xffff) {
		case 0x9c42:		/* IDM_CHANGEDEVICE */
			if (!self->active || !self->ready) {
				return 0;
			}
			self->vftable->Pause(self, 1);
			if (CD3DApp_UserSelectNewDevice(self) < 0) {
				return 0;
			}
			self->vftable->Pause(self, 0);
			return 0;
		case 0x9c43:		/* IDM_TOGGLEFULLSCREEN */
			if (!self->active || !self->ready) {
				return 0;
			}
			if (CD3DApp_ToggleFullscreen(self) < 0) {
				CD3DApp_DisplayErrorMsg(self, (int)D3DAPPERR_RESIZEFAILED, 1);
			}
			return 0;
		case 0x9c44: {		/* IDM_TOGGLESTART */
			int moving = self->frameMoving;
			self->frameMoving = !moving;
			DXUtil_Timer(moving ? DXUTIL_TIMER_STOP : DXUTIL_TIMER_START);
			break;
		}
		case 0x9c45:		/* IDM_SINGLESTEP */
			DXUtil_Timer(self->frameMoving ? DXUTIL_TIMER_STOP : DXUTIL_TIMER_ADVANCE);
			self->frameMoving = 0;
			self->singleStep = 1;
			break;
		case 0x9c46:		/* IDM_EXIT */
			exe_SendMessageA(hwnd, 0x10, 0, 0);
			return 0;
		}
		break;
	case 0x112:			/* WM_SYSCOMMAND: no moving, sizing, maximizing, menu and power off when fullscreen */
		if ((w == 0xf010 || w == 0xf000 || w == 0xf030 || w == 0xf100 || w == 0xf170) && !self->windowed) {
			return 1;
		}
		break;
	case 0x200:			/* WM_MOUSEMOVE */
		if (self->active && self->ready && self->device) {
			exe_point pt;
			exe_GetCursorPos(&pt);
			exe_ScreenToClient(self->hWnd, &pt);
			void *d = self->device;
			Rd_DevSetCursorPosition(d, pt.x, pt.y, 0);
		}
		break;
	case 0x211:			/* WM_ENTERMENULOOP */
		self->vftable->Pause(self, 1);
		break;
	case 0x212:			/* WM_EXITMENULOOP */
		self->vftable->Pause(self, 0);
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
		if (self->frameMoving) {
			DXUtil_Timer(DXUTIL_TIMER_STOP);
		}
		break;
	case 0x232:			/* WM_EXITSIZEMOVE: the back buffer of a new client size */
		if (self->frameMoving) {
			DXUtil_Timer(DXUTIL_TIMER_START);
		}
		if (self->active && self->windowed) {
			exe_rect old = self->windowClient;
			exe_GetWindowRect(self->hWnd, &self->windowBounds);
			exe_GetClientRect(self->hWnd, &self->windowClient);
			exe_rect *c = &self->windowClient;
			int nw = c->right - c->left;
			if (old.right - old.left != nw || old.bottom - old.top != c->bottom - c->top) {
				self->ready = 0;
				self->presentParams.BackBufferWidth = (uint32_t)nw;
				self->presentParams.BackBufferHeight = (uint32_t)(c->bottom - c->top);
				if (CD3DApp_Reset(self) < 0) {
					CD3DApp_DisplayErrorMsg(self, (int)D3DAPPERR_RESIZEFAILED, 1);
					return 0;
				}
				self->ready = 1;
			}
		}
		break;
	}
	return exe_DefWindowProcA(hwnd, msg, w, l);
}

/* the frames per second (each second) and the frame statistics with the mode and the depth format */
static void D3DApp_FrameStats(CD3DApplication *self) {
	float t = DXUtil_Timer(DXUTIL_TIMER_GETABSOLUTETIME);
	uint32_t frames = ++g_D3DAppFrames;
	float d = t - g_D3DAppLastTime;
	if (!(d > 1.0f)) {
		return;
	}
	g_D3DAppFrames = 0;
	g_D3DAppLastTime = t;
	self->fps = (float)(double)frames / d;
	if (g_pApp->flags & APPF_DEDICATED) {
		return;
	}
	D3DDisplayMode mode;
	void *d3d = self->d3d;
	Rd_D3DGetAdapterDisplayMode(d3d, self->adapter, &mode);
	/* differs from the original on purpose: bounded by the 42 bytes of frameStats, the original used 90 and could
	 * write over devId */
	Bprintf(self->frameStats, sizeof(self->frameStats), "%.02f fps (%dx%dx%d)", (double)self->fps,
		self->backBufferDesc.Width, self->backBufferDesc.Height, mode.Format == D3DFMT_X8R8G8B8 ? 32 : 16);
	if (!self->useDepthBuffer) {
		return;
	}
	D3DAdapterInfo *ad = &self->adapters[self->adapter];
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
	D3DApp_Cat(self->frameStats, sizeof(self->frameStats), s);
}

/* a lost device reset when it can be; the scene moved (while the time runs or for a single step) and drawn */
int CD3DApp_Render(CD3DApplication *self) {
	int hr = 0;

	/* differs from the original on purpose (no sleeps): no frame while the frame limiter holds it back */
	if (!(g_pApp->flags & APPF_DEDICATED) && !Engine_FrameDue(self)) {
		return 0;
	}
	if (!(g_pApp->flags & APPF_DEDICATED)) {
		void *d = self->device;
		hr = Rd_DevTestCooperativeLevel(d);
		if (hr < 0) {
			if (hr == D3DERR_DEVICELOST) {
				return 0;
			}
			if (hr != D3DERR_DEVICENOTRESET) {
				return hr;
			}
			if (self->windowed) {
				D3DAdapterInfo *ad = &self->adapters[self->adapter];
				void *d3d = self->d3d;
				Rd_D3DGetAdapterDisplayMode(d3d,
					self->adapter, &ad->desktopMode);
				self->presentParams.BackBufferFormat = ad->desktopMode.Format;
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
	if (elapsed == 0.0f && self->frameMoving) {
		return 0;
	}
	if (self->frameMoving || self->singleStep) {
		self->time = appTime;
		self->elapsedTime = elapsed;
		hr = self->vftable->FrameMove(self);
		if (hr < 0) {
			return hr;
		}
		self->singleStep = 0;
	}
	if (!(g_pApp->flags & APPF_DEDICATED)) {
		hr = self->vftable->Render(self);
		if (hr < 0) {
			return hr;
		}
	}
	if (hr == 1) {
		exe_SendMessageA(self->hWnd, 0x10, 0, 0);
		return 0;
	}
	D3DApp_FrameStats(self);
	if (!(g_pApp->flags & APPF_DEDICATED)) {
		void *d = self->device;
		Rd_DevPresent(d, NULL, NULL, NULL, NULL);
	}
	return 0;
}

/* the message loop of the framework with the frame of CD3DApp_Render inside; until WM_QUIT */
int CD3DApp_Run(CD3DApplication *self) {
	exe_msg msg;
	void *acc = exe_LoadAcceleratorsA(NULL, (const char *)0x71);
	int hr = 0;

	exe_PeekMessageA(&msg, NULL, 0, 0, 0);
	while (msg.message != 0x12) {	/* WM_QUIT */
		/* differs from the original on purpose (no blocking waits): PeekMessage also while inactive, where the original
		 * waited in GetMessage; without a message the inactive app goes round again */
		int got = exe_PeekMessageA(&msg, NULL, 0, 0, 1);
		if (got) {
			if (!exe_TranslateAcceleratorA(self->hWnd, acc, &msg)) {
				exe_TranslateMessage(&msg);
				exe_DispatchMessageA(&msg);
			}
			continue;
		}
		if (!self->active || !self->ready) {
			continue;
		}
		/* differs from the original on purpose (no sleeps): the frame limiter of CEngineApp_Render holds the frame
		 * back here, before the clock and FrameMove, instead of sleeping inside Render */
		if (!(g_pApp->flags & APPF_DEDICATED) && !Engine_FrameDue(self)) {
			continue;
		}
		int r = 0;
		if (!(g_pApp->flags & APPF_DEDICATED)) {
			void *d = self->device;
			hr = Rd_DevTestCooperativeLevel(d);
			if (hr < 0) {
				if (hr == D3DERR_DEVICELOST) {
					continue;
				}
				r = hr;
				if (hr == D3DERR_DEVICENOTRESET) {
					if (self->windowed) {
						D3DAdapterInfo *ad = &self->adapters[self->adapter];
						void *d3d = self->d3d;
						Rd_D3DGetAdapterDisplayMode(d3d,
							self->adapter, &ad->desktopMode);
						self->presentParams.BackBufferFormat = ad->desktopMode.Format;
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
		if (elapsed == 0.0f && self->frameMoving) {
			continue;
		}
		if (self->frameMoving || self->singleStep) {
			self->time = appTime;
			self->elapsedTime = elapsed;
			hr = self->vftable->FrameMove(self);
			r = hr;
			if (hr < 0) {
				goto failed;
			}
			self->singleStep = 0;
		}
		if (!(g_pApp->flags & APPF_DEDICATED)) {
			hr = self->vftable->Render(self);
			r = hr;
			if (hr < 0) {
				goto failed;
			}
		}
		if (hr == 1) {
			exe_SendMessageA(self->hWnd, 0x10, 0, 0);
			continue;
		}
		D3DApp_FrameStats(self);
		if (!(g_pApp->flags & APPF_DEDICATED)) {
			void *d = self->device;
			Rd_DevPresent(d, NULL, NULL, NULL, NULL);
		}
		continue;
failed:
		/* the first error ends the loop through WM_CLOSE */
		if (r < 0) {
			exe_SendMessageA(self->hWnd, 0x10, 0, 0);
		}
	}
	return (int)msg.wParam;
}

/* the choice of the device dialog: the original values and those selected */
#define g_SelApp GAME_GLOBAL(CD3DApplication *, g_SelApp)
extern uint32_t g_OrigAdapter;
extern uint32_t g_SelAdapter;
extern uint32_t g_OrigDevice;
extern uint32_t g_SelDevice;
extern uint32_t g_OrigMode;
extern uint32_t g_SelMode;
extern int g_OrigWindowed;
extern int g_SelWindowed;
extern uint32_t g_OrigMultiSample;
extern uint32_t g_SelMultiSample;

typedef int(__cdecl *wsprintf_t)(char *buf, const char *fmt, ...);
#define exe_wsprintfA GAME_IMPORT_CDECL(wsprintf_t, wsprintfA)

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
	return &g_SelApp->adapters[g_SelAdapter];
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
		CD3DApplication *app = (CD3DApplication *)l;
		g_SelApp = app;
		D3DAdapterInfo *ad = &app->adapters[app->adapter];
		g_OrigAdapter = g_SelAdapter = app->adapter;
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
			g_SelApp->adapter = g_SelAdapter;
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
	for (uint32_t a = 0; a < g_SelApp->numAdapters; a++) {
		D3DApp_ComboAdd(hAdapter, g_SelApp->adapters[a].id.Description, a, a == g_SelAdapter);
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
		uint32_t fmt = g_SelWindowed ? g_SelApp->adapters[g_SelAdapter].desktopMode.Format :
			dev->modes[g_SelMode].Format;
		if (t == 1) {
			continue;
		}
		void *d3d = g_SelApp->d3d;
		if (Rd_D3DCheckDeviceMultiSampleType(d3d, g_SelAdapter,
			dev->DeviceType, fmt, g_SelWindowed, t) < 0) {
			continue;
		}
		if (!t) {
			Str_Copy(ms, "none", sizeof(ms));	/* "none" */
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
