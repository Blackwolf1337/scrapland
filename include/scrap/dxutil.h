/*
 * The DX8 SDK framework inside the engine3d: the helpers of dxutil.cpp (src/game/engine3d/Engine/DXUtil.c) and the
 * CD3DApplication of d3dapp.cpp (g_Engine, D3DApp.c; more of it in e3dutil.h and scrapmain.h).
 */
#ifndef SCRAP_DXUTIL_H
#define SCRAP_DXUTIL_H

#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"

typedef struct D3DDisplayMode {
	uint32_t Width;			/* 0x0 */
	uint32_t Height;		/* 0x4 */
	uint32_t RefreshRate;		/* 0x8 */
	uint32_t Format;		/* 0xc */
} D3DDisplayMode;
SD_SIZE(D3DDisplayMode, 0x10);

typedef struct D3DGuid {
	uint32_t Data1;
	uint16_t Data2;
	uint16_t Data3;
	byte Data4[8];
} D3DGuid;
SD_SIZE(D3DGuid, 0x10);

/* more of g_Engine */
#define ENGINE_U32(m, off) (*(uint32_t *)((m) + (off)))
#define ENGINE_ACTIVE(m) ENGINE_U32(m, 0x2a388)
#define ENGINE_READY(m) ENGINE_U32(m, 0x2a38c)
#define ENGINE_FRAMEMOVING(m) ENGINE_U32(m, 0x2a390)
#define ENGINE_SINGLESTEP(m) ENGINE_U32(m, 0x2a394)
#define ENGINE_D3D(m) (*(void **)((m) + 0x2a3d4))	/* IDirect3D8 */
#define ENGINE_USEDEPTH(m) ENGINE_U32(m, 0x2a5b0)

/* dxutil.cpp */
/* cch characters with the end (-1: the whole string); the generic strings are ANSI (two copies in the exe) */
void DXUtil_AnsiToGeneric(char *dst, const char *src, int cch);
void DXUtil_AnsiToWide(uint16_t *dst, const char *src, int cch);
void DXUtil_GenericToWide(uint16_t *dst, const char *src, int cch);
void DXUtil_WideToAnsi(char *dst, const uint16_t *src, int cch);
void DXUtil_WideToGeneric(char *dst, const uint16_t *src, int cch);
/* "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}" (39 bytes) */
void DXUtil_GUIDToString(const D3DGuid *g, char *buf);
/* 1 parsed, else 0 and the GUID cleared */
int DXUtil_StringToGUID(const char *s, D3DGuid *g);
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
float __stdcall DXUtil_Timer(int cmd);
/* the distinct values of timeGetTime during ms (the original busy waited, the C answers ms) */
int Timer_CountTicks(int ms);
/* the performance counter ticks during ms (the original busy waited from the start of a timeGetTime tick on, the C
 * takes them from the frequency) */
int64_t Timer_MeasureQP(int ms);

/* d3dapp.cpp */
/* qsort order of the display modes: width, height, then the format (the higher first) */
int D3DApp_SortModes(const D3DDisplayMode *a, const D3DDisplayMode *b);
/* the window procedure of the application window: the MsgProc of g_Engine (vtable 0x30) */
intptr_t __stdcall D3DApp_WndProc(void *hwnd, uint32_t msg, uintptr_t w, intptr_t l);
/* ds is a depth/stencil format the adapter supports with the format fmt (IDirect3D8 d3d) */
byte D3DApp_DepthFormatOk(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t ds);
/* the depth/stencil format (D3DFMT_*) of a device for the minimum bits of g_Engine; 0 for none */
int __thiscall CD3DApp_FindDepthStencilFormat(byte *self, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t *out);
/* the defaults, g_Engine set (cleared by the destructor) */
byte *__fastcall CD3DApp_ctor(byte *self);
void __fastcall CD3DApp_dtor(byte *self);
int __thiscall CD3DApp_AdjustWindowForChange(byte *self);
void __thiscall CD3DApp_Pause(byte *self, int pause);
/* the back buffer and depth buffer descriptions and the aspect ratio, a new device id */
void __fastcall CD3DApp_UpdateBackBufferDesc(byte *self);
/* the default virtual methods (seven copies), S_OK */
int __thiscall CD3DApp_Nop(byte *self);
int __thiscall CD3DApp_ConfirmDevice(byte *self, void *caps, uint32_t behavior, uint32_t fmt);
/* windowed on a device that can, the environment made again */
int __thiscall CD3DApp_ForceWindowed(byte *self);
int __thiscall CD3DApp_ToggleFullscreen(byte *self);
/* the device dialog, the environment made again for the choice */
int __thiscall CD3DApp_UserSelectNewDevice(byte *self);
/* the window messages of the framework (rva 0x274b60) */
intptr_t __thiscall CD3DApp_MsgProc(byte *self, void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam);
/* CD3DApp_Cleanup, CD3DApp_ResetDevice and CD3DApp_Reset: scrapmain.h */

/* the error text of hr reported (type 1: fatal; else also fatal while fullscreen, then as a warning with the
 * note about the REF device), hr */
int __thiscall CD3DApp_DisplayErrorMsg(byte *self, int hr, int type);
/* the device of the current adapter, device and mode created and its objects made; a failed HAL falls back to the
 * REF device */
int __thiscall CD3DApp_Initialize3DEnvironment(byte *self);
/* Direct3D, the device list, the window (if there is none yet) and the environment; the error shown */
int __thiscall CD3DApp_Create(byte *self, void *inst);
int __fastcall CD3DApp_BuildDeviceList(byte *self);
intptr_t __thiscall CD3DApp_MsgProc(byte *self, void *hwnd, uint32_t msg, uintptr_t w, intptr_t l);
/* the message loop with the frames, the wParam of WM_QUIT */
int __thiscall CD3DApp_Run(byte *self);
/* the device dialog (DialogBoxParam with the application) */
intptr_t __stdcall D3DApp_SelectDeviceProc(void *dlg, uint32_t msg, uintptr_t w, intptr_t l);

#endif
