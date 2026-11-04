/*
 * The DX8 SDK framework inside the engine3d: the helpers of dxutil.cpp (src/game/engine3d/Engine/DXUtil.c) and the
 * CD3DApplication of d3dapp.cpp (g_Engine, D3DApp.c; CMotorApp of the engine around it in scrapmain.h).
 */
#ifndef SCRAP_DXUTIL_H
#define SCRAP_DXUTIL_H

#include "scrap/d3d8.h"
#include "scrap/game.h"
#include "scrap/platform.h"

/* a RECT of Windows */
typedef struct exe_rect {
	int32_t left, top, right, bottom;
} exe_rect;

typedef struct D3DGuid {
	uint32_t Data1;
	uint16_t Data2;
	uint16_t Data3;
	byte Data4[8];
} D3DGuid;

/* the adapters of CD3DApplication with their devices and modes */
typedef struct D3DModeInfo {
	uint32_t Width;
	uint32_t Height;
	uint32_t Format;		/* D3DFORMAT */
	uint32_t dwBehavior;
	uint32_t DepthStencilFormat;
} D3DModeInfo;

#define MAX_D3DMODES 150
#define MAX_D3DDEVICES 5

typedef struct D3DDeviceInfo {
	uint32_t DeviceType;		/* 1 HAL, 2 REF */
	D3DCaps8 d3dCaps;
	const char *strDesc;		/* "HAL", "REF" (strings of the exe) */
	int bCanDoWindowed;
	int dwNumModes;
	D3DModeInfo modes[MAX_D3DMODES];
	int dwCurrentMode;
	int bWindowed;
	uint32_t MultiSampleType;
} D3DDeviceInfo;

typedef struct D3DAdapterInfo {
	D3DAdapterId id;
	D3DDisplayMode desktopMode;		/* d3ddmDesktop */
	uint32_t dwNumDevices;
	D3DDeviceInfo devices[MAX_D3DDEVICES];
	int dwCurrentDevice;
} D3DAdapterInfo;

#define MAX_D3DADAPTERS 10

typedef struct CD3DApplication CD3DApplication;

/* the virtual methods of CD3DApplication (the HRESULT unless noted) */
typedef struct CD3DApplicationVtbl {
	int(*AdjustWindowForChange)(CD3DApplication *self);
	int(*ConfirmDevice)(CD3DApplication *self, D3DCaps8 *caps, uint32_t behavior, uint32_t fmt);
	int(*OneTimeSceneInit)(CD3DApplication *self);
	int(*InitDeviceObjects)(CD3DApplication *self);
	int(*RestoreDeviceObjects)(CD3DApplication *self);
	int(*FrameMove)(CD3DApplication *self);
	int(*Render)(CD3DApplication *self);
	int(*InvalidateDeviceObjects)(CD3DApplication *self);
	int(*DeleteDeviceObjects)(CD3DApplication *self);
	int(*FinalCleanup)(CD3DApplication *self);
	int(*Create)(CD3DApplication *self, void *hInstance);
	int(*Run)(CD3DApplication *self);			/* the wParam of WM_QUIT */
	void *msgProc;				/* the window procedure, gone (src/game/App/Platform.c) */
	void(*Pause)(CD3DApplication *self, int pause);
} CD3DApplicationVtbl;

/* calls through the slots of CD3DApplicationVtbl */
static inline int D3DApp_AdjustWindowForChange(void *self) {
	return VT(CD3DApplicationVtbl, self)->AdjustWindowForChange(self);
}

static inline int D3DApp_Create(void *self, void *hInstance) {
	return VT(CD3DApplicationVtbl, self)->Create(self, hInstance);
}

static inline int D3DApp_DeleteDeviceObjects(void *self) {
	return VT(CD3DApplicationVtbl, self)->DeleteDeviceObjects(self);
}

static inline int D3DApp_FinalCleanup(void *self) {
	return VT(CD3DApplicationVtbl, self)->FinalCleanup(self);
}

static inline int D3DApp_FrameMove(void *self) {
	return VT(CD3DApplicationVtbl, self)->FrameMove(self);
}

static inline int D3DApp_InitDeviceObjects(void *self) {
	return VT(CD3DApplicationVtbl, self)->InitDeviceObjects(self);
}

static inline int D3DApp_InvalidateDeviceObjects(void *self) {
	return VT(CD3DApplicationVtbl, self)->InvalidateDeviceObjects(self);
}

static inline int D3DApp_OneTimeSceneInit(void *self) {
	return VT(CD3DApplicationVtbl, self)->OneTimeSceneInit(self);
}

static inline int D3DApp_Render(void *self) {
	return VT(CD3DApplicationVtbl, self)->Render(self);
}

static inline int D3DApp_RestoreDeviceObjects(void *self) {
	return VT(CD3DApplicationVtbl, self)->RestoreDeviceObjects(self);
}

/* CD3DApplication of d3dapp.cpp: the DX8 SDK members with the device id and the mode of the game */
struct CD3DApplication {
	const CD3DApplicationVtbl *vftable;
	D3DAdapterInfo adapters[MAX_D3DADAPTERS];
	uint32_t numAdapters;
	uint32_t adapter;
	int windowed;
	int active;
	int ready;
	int frameMoving;
	int singleStep;
	D3DPresentParams presentParams;
	void *hWnd;
	void *hWndFocus;
	void *d3d;				/* IDirect3D8 */
	void *device;				/* IDirect3DDevice8, NULL before it is made */
	D3DCaps8 caps;
	d3dSurfaceDesc_t backBufferDesc;
	d3dSurfaceDesc_t depthBufferDesc;
	uint32_t createFlags;			/* the behavior of CreateDevice */
	uint32_t windowStyle;
	exe_rect windowBounds;
	exe_rect windowClient;
	float time;				/* s */
	float elapsedTime;
	float fps;
	char deviceStats[90];			/* the device name */
	char frameStats[42];
	int devId;				/* a new one with each back buffer description */
	const char *windowTitle;
	int useDepthBuffer;
	uint32_t minDepthBits;
	uint32_t minStencilBits;
	uint32_t creationWidth;
	uint32_t creationHeight;
	int showCursorWhenFullscreen;
	int vsync;
	int fullscreen;
	int borderless;			/* windowed over the whole desktop (FullScreen 2) */
	int width;
	int height;
	int bpp;
};

/* g_pD3DApp of d3dapp.cpp: the application of the engine3d (a CMotorApp, g_Engine3D of Engine.c) */
extern struct CD3DApplication *g_Engine;

/* dxutil.cpp */
/* the counter of the engine advanced by 1/fps seconds per call (frames of a fixed length), 0: the real counter
 * again; Timer_GetFreq (scrapmain.h) reads it */
int Timer_FixedStep(float fps);
/* the timer of the framework (DXUtil_Timer of the SDK, stdcall), on the performance counter of Timer_GetFreq or
 * else timeGetTime */
#define DXUTIL_TIMER_RESET 0
#define DXUTIL_TIMER_START 1
#define DXUTIL_TIMER_STOP 2
#define DXUTIL_TIMER_ADVANCE 3		/* 0.1 s while stopped */
#define DXUTIL_TIMER_GETABSOLUTETIME 4
#define DXUTIL_TIMER_GETAPPTIME 5
#define DXUTIL_TIMER_GETELAPSEDTIME 6
float DXUtil_Timer(int cmd);

/* d3dapp.cpp */
/* the defaults, g_Engine set (cleared by the destructor) */
CD3DApplication *CD3DApp_ctor(CD3DApplication *self);
void CD3DApp_dtor(CD3DApplication *self);
int CD3DApp_AdjustWindowForChange(CD3DApplication *self);
void CD3DApp_Pause(CD3DApplication *self, int pause);
int CD3DApp_ToggleFullscreen(CD3DApplication *self);
/* the display mode (PLAT_* of platform.h) for the next reset: the window and the present parameters; fullscreen when
 * the device cannot render in a window */
void CD3DApp_SetWindowMode(CD3DApplication *self, int mode);
/* the window messages of the framework */
/* CD3DApp_Cleanup, CD3DApp_ResetDevice and CD3DApp_Reset: scrapmain.h */

/* the error text of hr reported as fatal, hr */
int CD3DApp_DisplayErrorMsg(int hr);
/* Direct3D, the device list, the window (if there is none yet) and the environment; the error shown */
int CD3DApp_Create(CD3DApplication *self, void *inst);
/* the message loop with the frames, the wParam of WM_QUIT */

#endif
