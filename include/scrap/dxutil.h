/*
 * The helpers of the DX8 SDK framework inside the engine3d (dxutil.cpp, d3dutil.cpp and the static parts of
 * d3dapp.cpp, src/game/engine3d/Engine/DXUtil.c). The rest of CD3DApplication (g_Motor, scrapmain.h) is still the
 * exe's.
 */
#ifndef SCRAP_DXUTIL_H
#define SCRAP_DXUTIL_H

#include "scrap/d3d8.h"

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

/* qsort order of the display modes: width, height, then the format (the higher first) */
int D3DApp_SortModes(const D3DDisplayMode *a, const D3DDisplayMode *b);
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
/* the window procedure of the application window: the MsgProc of g_Motor (vtable 0x30) */
intptr_t __stdcall D3DApp_WndProc(void *hwnd, uint32_t msg, uintptr_t w, intptr_t l);
/* ds is a depth/stencil format the adapter supports with the format fmt (IDirect3D8 d3d) */
byte D3DApp_DepthFormatOk(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t ds);

#endif
