/*
 * Windows functions called through the import table of the exe (user32, gdi32, kernel32), so the C code needs no
 * import libraries of its own and goes the same way as the original.
 */
#ifndef SCRAP_WIN32_H
#define SCRAP_WIN32_H

#include "scrap/game.h"

typedef struct exe_rect {
	int32_t left, top, right, bottom;
} exe_rect;

typedef int(__stdcall *messagebox_t)(void *hwnd, const char *text, const char *caption, unsigned type);
typedef int(__stdcall *showcursor_t)(int show);
typedef void *(__stdcall *getdc_t)(void *hwnd);
typedef int(__stdcall *releasedc_t)(void *hwnd, void *dc);
typedef void *(__stdcall *createsolidbrush_t)(uint32_t color);
typedef int(__stdcall *deleteobject_t)(void *obj);
typedef int(__stdcall *rectbrush_t)(void *dc, const exe_rect *rect, void *brush);

#define exe_MessageBoxA GAME_DATA(messagebox_t, 0x3a142c)
#define exe_ShowCursor GAME_DATA(showcursor_t, 0x3a13a0)
#define exe_GetDC GAME_DATA(getdc_t, 0x3a1390)
#define exe_ReleaseDC GAME_DATA(releasedc_t, 0x3a1394)
#define exe_FillRect GAME_DATA(rectbrush_t, 0x3a1398)
#define exe_FrameRect GAME_DATA(rectbrush_t, 0x3a139c)
#define exe_CreateSolidBrush GAME_DATA(createsolidbrush_t, 0x3a107c)
#define exe_DeleteObject GAME_DATA(deleteobject_t, 0x3a1080)

/* kernel32 */
typedef uint32_t(__stdcall *getfileattributes_t)(const char *name);
typedef int(__stdcall *setfileattributes_t)(const char *name, uint32_t attr);

#define exe_GetFileAttributesA GAME_DATA(getfileattributes_t, 0x3a1284)
#define exe_SetFileAttributesA GAME_DATA(setfileattributes_t, 0x3a1288)

/* winmm */
typedef uint32_t(__stdcall *timegettime_t)(void);
#define exe_timeGetTime GAME_DATA(timegettime_t, 0x3a1468)

#endif
