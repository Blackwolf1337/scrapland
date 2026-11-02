/*
 * The DX8 SDK framework inside the engine3d: the helpers of dxutil.cpp (src/game/engine3d/Engine/DXUtil.c) and the
 * CD3DApplication of d3dapp.cpp (g_Engine, D3DApp.c; CMotorApp of the engine around it in scrapmain.h).
 */
#ifndef SCRAP_DXUTIL_H
#define SCRAP_DXUTIL_H

#include "scrap/d3d8.h"
#include "scrap/game.h"
#include "scrap/win32.h"

typedef struct D3DGuid {
	uint32_t Data1;
	uint16_t Data2;
	uint16_t Data3;
	byte Data4[8];
} D3DGuid;
SD_SIZE(D3DGuid, 0x10);

/* the adapters of CD3DApplication with their devices and modes */
typedef struct D3DModeInfo {
	uint32_t Width;			/* 0x00 */
	uint32_t Height;		/* 0x04 */
	uint32_t Format;		/* 0x08 D3DFORMAT */
	uint32_t dwBehavior;		/* 0x0c */
	uint32_t DepthStencilFormat;	/* 0x10 */
} D3DModeInfo;
SD_SIZE(D3DModeInfo, 0x14);

#define MAX_D3DMODES 150
#define MAX_D3DDEVICES 5

typedef struct D3DDeviceInfo {
	uint32_t DeviceType;		/* 0x00 1 HAL, 2 REF */
	D3DCaps8 d3dCaps;		/* 0x04 */
	const char *strDesc;		/* 0xd8 "HAL", "REF" (strings of the exe) */
	int bCanDoWindowed;		/* 0xdc */
	int dwNumModes;			/* 0xe0 */
	D3DModeInfo modes[MAX_D3DMODES];	/* 0xe4 */
	int dwCurrentMode;		/* 0xc9c */
	int bWindowed;			/* 0xca0 */
	uint32_t MultiSampleType;	/* 0xca4 */
} D3DDeviceInfo;
SD_OFFSET(D3DDeviceInfo, dwCurrentMode, 0xc9c);
SD_SIZE(D3DDeviceInfo, 0xca8);

typedef struct D3DAdapterInfo {
	D3DAdapterId id;			/* 0x0000 */
	D3DDisplayMode desktopMode;		/* 0x042c d3ddmDesktop */
	uint32_t dwNumDevices;			/* 0x043c */
	D3DDeviceInfo devices[MAX_D3DDEVICES];	/* 0x0440 */
	int dwCurrentDevice;			/* 0x4388 */
} D3DAdapterInfo;
SD_OFFSET(D3DAdapterInfo, desktopMode, 0x42c);
SD_OFFSET(D3DAdapterInfo, dwNumDevices, 0x43c);
SD_OFFSET(D3DAdapterInfo, dwCurrentDevice, 0x4388);
SD_SIZE(D3DAdapterInfo, 0x438c);

#define MAX_D3DADAPTERS 10

typedef struct CD3DApplication CD3DApplication;

/* the virtual methods of CD3DApplication (the HRESULT unless noted; the slot tables of Vtables.c) */
typedef struct CD3DApplicationVtbl {
	int(*AdjustWindowForChange)(CD3DApplication *self);	/* 0x00 */
	int(*ConfirmDevice)(CD3DApplication *self, D3DCaps8 *caps, uint32_t behavior, uint32_t fmt);	/* 0x04 */
	int(*OneTimeSceneInit)(CD3DApplication *self);	/* 0x08 */
	int(*InitDeviceObjects)(CD3DApplication *self);	/* 0x0c */
	int(*RestoreDeviceObjects)(CD3DApplication *self);	/* 0x10 */
	int(*FrameMove)(CD3DApplication *self);		/* 0x14 */
	int(*Render)(CD3DApplication *self);			/* 0x18 */
	int(*InvalidateDeviceObjects)(CD3DApplication *self);	/* 0x1c */
	int(*DeleteDeviceObjects)(CD3DApplication *self);	/* 0x20 */
	int(*FinalCleanup)(CD3DApplication *self);		/* 0x24 */
	int(*Create)(CD3DApplication *self, void *hInstance);	/* 0x28 */
	int(*Run)(CD3DApplication *self);			/* 0x2c the wParam of WM_QUIT */
	void *msgProc;				/* 0x30 the window procedure, gone (src/game/App/Platform.c) */
	void(*Pause)(CD3DApplication *self, int pause);	/* 0x34 */
} CD3DApplicationVtbl;

/* CD3DApplication of d3dapp.cpp, 0x2a5dc bytes: the DX8 SDK members with the device id and the mode of the game */
struct CD3DApplication {
	const CD3DApplicationVtbl *vftable;	/* 0x00000 */
	D3DAdapterInfo adapters[MAX_D3DADAPTERS];	/* 0x00004 */
	uint32_t numAdapters;			/* 0x2a37c */
	uint32_t adapter;			/* 0x2a380 */
	int windowed;				/* 0x2a384 */
	int active;				/* 0x2a388 */
	int ready;				/* 0x2a38c */
	int frameMoving;			/* 0x2a390 */
	int singleStep;				/* 0x2a394 */
	D3DPresentParams presentParams;		/* 0x2a398 */
	void *hWnd;				/* 0x2a3cc */
	void *hWndFocus;			/* 0x2a3d0 */
	void *d3d;				/* 0x2a3d4 IDirect3D8 */
	void *device;				/* 0x2a3d8 IDirect3DDevice8, NULL before it is made */
	D3DCaps8 caps;				/* 0x2a3dc */
	d3dSurfaceDesc_t backBufferDesc;	/* 0x2a4b0 */
	d3dSurfaceDesc_t depthBufferDesc;	/* 0x2a4d0 */
	uint32_t createFlags;			/* 0x2a4f0 the behavior of CreateDevice */
	uint32_t windowStyle;			/* 0x2a4f4 */
	exe_rect windowBounds;			/* 0x2a4f8 */
	exe_rect windowClient;			/* 0x2a508 */
	float time;				/* 0x2a518 s */
	float elapsedTime;			/* 0x2a51c */
	float fps;				/* 0x2a520 */
	char deviceStats[90];			/* 0x2a524 the device name */
	char frameStats[42];			/* 0x2a57e */
	int devId;				/* 0x2a5a8 a new one with each back buffer description */
	const char *windowTitle;		/* 0x2a5ac */
	int useDepthBuffer;			/* 0x2a5b0 */
	uint32_t minDepthBits;			/* 0x2a5b4 */
	uint32_t minStencilBits;		/* 0x2a5b8 */
	uint32_t creationWidth;			/* 0x2a5bc */
	uint32_t creationHeight;		/* 0x2a5c0 */
	int showCursorWhenFullscreen;		/* 0x2a5c4 */
	int vsync;				/* 0x2a5c8 */
	int fullscreen;				/* 0x2a5cc */
	int width;				/* 0x2a5d0 */
	int height;				/* 0x2a5d4 */
	int bpp;				/* 0x2a5d8 */
};
SD_OFFSET(CD3DApplication, numAdapters, 0x2a37c);
SD_OFFSET(CD3DApplication, presentParams, 0x2a398);
SD_OFFSET(CD3DApplication, caps, 0x2a3dc);
SD_OFFSET(CD3DApplication, backBufferDesc, 0x2a4b0);
SD_OFFSET(CD3DApplication, createFlags, 0x2a4f0);
SD_OFFSET(CD3DApplication, time, 0x2a518);
SD_OFFSET(CD3DApplication, frameStats, 0x2a57e);
SD_OFFSET(CD3DApplication, devId, 0x2a5a8);
SD_OFFSET(CD3DApplication, vsync, 0x2a5c8);
SD_SIZE(CD3DApplication, 0x2a5dc);

/* g_pD3DApp of d3dapp.cpp: the application of the engine3d (a CMotorApp, g_Engine3D of Engine.c) */
#define g_Engine GAME_GLOBAL(CD3DApplication *, g_Engine)

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
/* the distinct values of timeGetTime during ms (the original busy waited, the C answers ms) */
int Timer_CountTicks(int ms);
/* the performance counter ticks during ms (the original busy waited from the start of a timeGetTime tick on, the C
 * takes them from the frequency) */
int64_t Timer_MeasureQP(int ms);

/* d3dapp.cpp */
/* qsort order of the display modes: width, height, then the format (the higher first) */
int D3DApp_SortModes(const D3DDisplayMode *a, const D3DDisplayMode *b);
/* ds is a depth/stencil format the adapter supports with the format fmt (IDirect3D8 d3d) */
byte D3DApp_DepthFormatOk(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t ds);
/* the depth/stencil format (D3DFMT_*) of a device for the minimum bits of g_Engine; 0 for none */
int CD3DApp_FindDepthStencilFormat(CD3DApplication *self, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t *out);
/* the defaults, g_Engine set (cleared by the destructor) */
CD3DApplication *CD3DApp_ctor(CD3DApplication *self);
void CD3DApp_dtor(CD3DApplication *self);
int CD3DApp_AdjustWindowForChange(CD3DApplication *self);
void CD3DApp_Pause(CD3DApplication *self, int pause);
/* the back buffer and depth buffer descriptions and the aspect ratio, a new device id */
void CD3DApp_UpdateBackBufferDesc(CD3DApplication *self);
/* the default virtual methods (seven copies), S_OK */
int CD3DApp_Nop(CD3DApplication *self);
int CD3DApp_ConfirmDevice(CD3DApplication *self, D3DCaps8 *caps, uint32_t behavior, uint32_t fmt);
/* windowed on a device that can, the environment made again */
int CD3DApp_ForceWindowed(CD3DApplication *self);
int CD3DApp_ToggleFullscreen(CD3DApplication *self);
/* the window messages of the framework */
/* CD3DApp_Cleanup, CD3DApp_ResetDevice and CD3DApp_Reset: scrapmain.h */

/* the error text of hr reported as fatal, hr */
int CD3DApp_DisplayErrorMsg(int hr);
/* the device of the current adapter, device and mode created and its objects made */
int CD3DApp_Initialize3DEnvironment(CD3DApplication *self);
/* Direct3D, the device list, the window (if there is none yet) and the environment; the error shown */
int CD3DApp_Create(CD3DApplication *self, void *inst);
int CD3DApp_BuildDeviceList(CD3DApplication *self);
/* the message loop with the frames, the wParam of WM_QUIT */

#endif
