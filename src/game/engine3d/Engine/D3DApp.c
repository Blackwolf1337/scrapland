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

typedef int(__stdcall *d3dGetBackBuffer_t)(void *dev, uint32_t i, uint32_t type, void **surf);
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
	if (!MOTOR_FRAMEMOVING(self)) {
		MOTOR_SINGLESTEP(self) = 1;
		DXUtil_Timer(D3DAPP_TIMER_START);
		DXUtil_Timer(D3DAPP_TIMER_STOP);
	}
	return 0;
}
