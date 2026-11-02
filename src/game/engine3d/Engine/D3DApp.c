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
#include "scrap/platform.h"
#include "scrap/platform.h"

extern const CD3DApplicationVtbl CD3DApplication_vftable;
#define CD3DAPPLICATION_VFTABLE (&CD3DApplication_vftable)
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
/* the window over the screen for fullscreen (the present scales the back buffer to it), the cursor only in a window */
int CD3DApp_AdjustWindowForChange(CD3DApplication *self) {
	Plat_SetFullscreen(!self->windowed);
	Plat_ShowCursor(self->windowed || self->showCursorWhenFullscreen);
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
		*err = CD3DApp_DisplayErrorMsg(0x8200000a);	/* D3DAPPERR_NONZEROREFCOUNT */
		return 0;
	}
	int r = CD3DApp_Initialize3DEnvironment(self);
	if (r < 0) {
		*err = CD3DApp_DisplayErrorMsg(r);
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
		return CD3DApp_DisplayErrorMsg(0x8200000c);	/* D3DAPPERR_RESIZEFAILED */
	}
	self->ready = 1;
	return 0;
}

/* D3DAPPERR_* */
#define D3DAPPERR_NODIRECT3D 0x82000001
#define D3DAPPERR_NOCOMPATIBLEDEVICES 0x82000003
#define D3DAPPERR_NOWINDOWABLEDEVICES 0x82000004
#define D3DAPPERR_RESIZEFAILED 0x8200000c
#define D3DERR_DEVICELOST ((int)0x88760868)
#define D3DERR_DEVICENOTRESET ((int)0x88760869)

#define D3DDEVTYPE_HAL 1
#define D3DFMT_R8G8B8 0x14
#define D3DFMT_A8R8G8B8 0x15
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


/* IDirect3D8 and IDirect3DDevice8 */

/* the bounded lstrcatA of the stats strings */
static void D3DApp_Cat(char *dst, int size, const char *src) {
	size_t n = strlen(dst);
	if ((int)n < size - 1) {
		Str_Copy(dst + n, src, size - (int)n);
	}
}

int CD3DApp_Create(CD3DApplication *self, void *inst) {
	int hr;

	(void)inst;

	self->d3d = Rd_Direct3DCreate8(0xdc);	/* D3D_SDK_VERSION */
	if (!self->d3d) {
		return CD3DApp_DisplayErrorMsg(D3DAPPERR_NODIRECT3D);
	}
	hr = CD3DApp_BuildDeviceList(self);
	if (hr < 0) {
		goto fail;
	}
	if (!self->hWndFocus) {
		self->hWndFocus = self->hWnd;
	}
	int wr[4], cw, ch;
	Plat_WindowRect(wr);
	Plat_ClientSize(&cw, &ch);
	self->windowBounds = (exe_rect) { wr[0], wr[1], wr[2], wr[3] };
	self->windowClient = (exe_rect) { 0, 0, cw, ch };
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
	return CD3DApp_DisplayErrorMsg(hr);
}

int CD3DApp_DisplayErrorMsg(int hr) {
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
	Err_Printf(1, "%s", msg);
	return hr;
}

/* the modes, depth formats and behaviors of the device of each adapter, then the first fullscreen one (with
 * fullscreen) or the first windowed one */
int CD3DApp_BuildDeviceList(CD3DApplication *self) {
	D3DDisplayMode modes[100];
	uint32_t fmts[20], behavior[20], depth[20];
	int fmtOk[20];
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
		{
			D3DDeviceInfo *dev = &ad->devices[ad->dwNumDevices];
			dev->DeviceType = D3DDEVTYPE_HAL;
			d3d = self->d3d;
			Rd_D3DGetDeviceCaps(d3d, a, D3DDEVTYPE_HAL, &dev->d3dCaps);
			dev->dwNumModes = 0;
			dev->dwCurrentMode = 0;
			dev->bCanDoWindowed = 0;
			dev->strDesc = "HAL";
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
				} else {
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
				ads[a].devices[d].bWindowed = 0;
				ads[a].dwCurrentDevice = (int)d;
				self->adapter = a;
				self->windowed = 0;
				return 0;
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
		Plat_ShowWindow();
		void *d = self->device;
		Rd_DevGetDeviceCaps(d, &self->caps);
		self->createFlags = mode->dwBehavior;
		char *stats = self->deviceStats;
		Str_Copy(stats, "HAL", sizeof(self->deviceStats));
		uint32_t b = mode->dwBehavior;
		if ((b & 0x50) == 0x50) {
			D3DApp_Cat(stats, sizeof(self->deviceStats), " (pure hw vp)");
		} else if (b & D3DCREATE_HARDWARE_VERTEXPROCESSING) {
			D3DApp_Cat(stats, sizeof(self->deviceStats), " (hw vp)");
		} else if (b & D3DCREATE_MIXED_VERTEXPROCESSING) {
			D3DApp_Cat(stats, sizeof(self->deviceStats), " (mixed vp)");
		} else if (b & D3DCREATE_SOFTWARE_VERTEXPROCESSING) {
			D3DApp_Cat(stats, sizeof(self->deviceStats), " (sw vp)");
		}
		D3DApp_Cat(stats, sizeof(self->deviceStats), ": ");
		D3DApp_Cat(stats, sizeof(self->deviceStats), ad->id.Description);
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
	CD3DApp_DisplayErrorMsg(hr);
	return hr;
}

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
				return CD3DApp_DisplayErrorMsg((int)D3DAPPERR_RESIZEFAILED);
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
		Plat_Quit();
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

/* the vtable of CD3DApplication */
const CD3DApplicationVtbl CD3DApplication_vftable = {
	.AdjustWindowForChange = CD3DApp_AdjustWindowForChange,
	.ConfirmDevice = CD3DApp_ConfirmDevice,
	.OneTimeSceneInit = CD3DApp_Nop,
	.InitDeviceObjects = CD3DApp_Nop,
	.RestoreDeviceObjects = CD3DApp_Nop,
	.FrameMove = CD3DApp_Nop,
	.Render = CD3DApp_Nop,
	.InvalidateDeviceObjects = CD3DApplication_Nothing,
	.DeleteDeviceObjects = CD3DApp_Nop,
	.FinalCleanup = CD3DApp_Nop,
	.Create = CD3DApp_Create,
	.Run = NULL,
	.msgProc = NULL,
	.Pause = CD3DApp_Pause,
};
