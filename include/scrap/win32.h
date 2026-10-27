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
typedef uint32_t(__stdcall *settextcolor_t)(void *dc, uint32_t color);
typedef int(__stdcall *setbkmode_t)(void *dc, int mode);
typedef int(__stdcall *exttextout_t)(void *dc, int x, int y, unsigned options, const exe_rect *rect, const char *s,
	unsigned n, const int *dx);
#define exe_SetTextColor GAME_DATA(settextcolor_t, 0x3a1070)
#define exe_SetBkMode GAME_DATA(setbkmode_t, 0x3a106c)
#define exe_ExtTextOutA GAME_DATA(exttextout_t, 0x3a1094)

/* kernel32 */
typedef uint32_t(__stdcall *getfileattributes_t)(const char *name);
typedef int(__stdcall *setfileattributes_t)(const char *name, uint32_t attr);

#define exe_GetFileAttributesA GAME_DATA(getfileattributes_t, 0x3a1284)
typedef uint64_t(__stdcall *gettickcount64_t)(void);
#define exe_GetTickCount64 GAME_DATA(gettickcount64_t, 0x3a1154)
#define exe_SetFileAttributesA GAME_DATA(setfileattributes_t, 0x3a1288)
typedef int(__stdcall *lstrlen_t)(const char *s);
#define exe_lstrlenA GAME_DATA(lstrlen_t, 0x3a12b8)

/* winmm */
typedef uint32_t(__stdcall *timegettime_t)(void);
#define exe_timeGetTime GAME_DATA(timegettime_t, 0x3a1468)

/* user32: the key state (negative while held) and a modeless dialog */
typedef int16_t(__stdcall *getasynckeystate_t)(int vk);
typedef void *(__stdcall *createdialogparam_t)(void *inst, const char *tmpl, void *parent, void *proc, intptr_t param);
#define exe_GetAsyncKeyState GAME_DATA(getasynckeystate_t, 0x3a1424)
#define exe_CreateDialogParamA GAME_DATA(createdialogparam_t, 0x3a133c)

/* the application window, the message loop and the console window (src/game/App/) */
typedef struct exe_msg {
	void *hwnd;
	uint32_t message;
	uintptr_t wParam;
	intptr_t lParam;
	uint32_t time;
	int32_t ptX, ptY;
} exe_msg;

typedef intptr_t(__stdcall *wndproc_t)(void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam);
typedef int(__stdcall *peekmessage_t)(exe_msg *msg, void *hwnd, uint32_t min, uint32_t max, uint32_t remove);
typedef intptr_t(__stdcall *dispatchmessage_t)(const exe_msg *msg);
typedef int(__stdcall *translatemessage_t)(const exe_msg *msg);
typedef int(__stdcall *translateaccelerator_t)(void *hwnd, void *acc, exe_msg *msg);
typedef void *(__stdcall *loadres_t)(void *inst, const char *name);	/* LoadAcceleratorsA, LoadIconA, LoadCursorA */
typedef intptr_t(__stdcall *sendmessage_t)(void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam);
typedef int(__stdcall *hwndfn_t)(void *hwnd);		/* IsIconic, DestroyWindow, UpdateWindow, SetForegroundWindow */
typedef void *(__stdcall *gethwnd_t)(void);		/* GetForegroundWindow, GetActiveWindow */
typedef void *(__stdcall *sethwnd_t)(void *hwnd);	/* SetFocus */
typedef int(__stdcall *showwindow_t)(void *hwnd, int cmd);
typedef int(__stdcall *setwindowtext_t)(void *hwnd, const char *text);
typedef int(__stdcall *getwindowtext_t)(void *hwnd, char *buf, int size);
typedef int32_t(__stdcall *getwindowlong_t)(void *hwnd, int index);
typedef int32_t(__stdcall *setwindowlong_t)(void *hwnd, int index, int32_t value);
typedef int(__stdcall *getrect_t)(void *hwnd, exe_rect *rect);	/* GetClientRect, GetWindowRect */
typedef int(__stdcall *adjustwindowrectex_t)(exe_rect *rect, uint32_t style, int menu, uint32_t exStyle);
typedef void *(__stdcall *createwindowex_t)(uint32_t exStyle, const char *cls, const char *name, uint32_t style, int x,
	int y, int w, int h, void *parent, void *menu, void *inst, void *param);
typedef uint16_t(__stdcall *registerclass_t)(const exe_wndclass *wc);
typedef int(__stdcall *unregisterclass_t)(const char *cls, void *inst);
typedef int(__stdcall *movewindow_t)(void *hwnd, int x, int y, int w, int h, int repaint);
typedef int(__stdcall *setwindowpos_t)(void *hwnd, void *after, int x, int y, int w, int h, uint32_t flags);
typedef intptr_t(__stdcall *dialogboxparam_t)(void *inst, const char *tmpl, void *parent, void *proc, intptr_t param);
typedef int(__stdcall *enddialog_t)(void *hdlg, intptr_t result);
typedef uint32_t(__stdcall *getdlgitemtext_t)(void *hdlg, int id, char *buf, int size);
typedef int(__stdcall *getsystemmetrics_t)(int index);
typedef void *(__stdcall *getsystemmenu_t)(void *hwnd, int revert);
typedef int(__stdcall *appendmenu_t)(void *menu, uint32_t flags, uintptr_t id, const char *item);
typedef intptr_t(__stdcall *callwindowproc_t)(void *proc, void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam);
typedef int(__stdcall *bringwindowtotop_t)(void *hwnd);
#define exe_PeekMessageA GAME_DATA(peekmessage_t, 0x3a1418)
#define exe_DispatchMessageA GAME_DATA(dispatchmessage_t, 0x3a141c)
#define exe_TranslateMessage GAME_DATA(translatemessage_t, 0x3a1420)
#define exe_TranslateAcceleratorA GAME_DATA(translateaccelerator_t, 0x3a13e8)
#define exe_LoadAcceleratorsA GAME_DATA(loadres_t, 0x3a13ec)
#define exe_LoadIconA GAME_DATA(loadres_t, 0x3a13c8)
#define exe_LoadCursorA GAME_DATA(loadres_t, 0x3a13cc)
#define exe_SendMessageA GAME_DATA(sendmessage_t, 0x3a1414)
#define exe_PostMessageA GAME_DATA(sendmessage_t, 0x3a1410)
#define exe_DefWindowProcA GAME_DATA(wndproc_t, 0x3a140c)
#define exe_IsIconic GAME_DATA(hwndfn_t, 0x3a13f8)
#define exe_DestroyWindow GAME_DATA(hwndfn_t, 0x3a1400)
#define exe_UpdateWindow GAME_DATA(hwndfn_t, 0x3a1454)
#define exe_SetForegroundWindow GAME_DATA(hwndfn_t, 0x3a13dc)
#define exe_GetForegroundWindow GAME_DATA(gethwnd_t, 0x3a1428)
#define exe_GetActiveWindow GAME_DATA(gethwnd_t, 0x3a13f0)
#define exe_SetFocus GAME_DATA(sethwnd_t, 0x3a144c)
#define exe_ShowWindow GAME_DATA(showwindow_t, 0x3a13fc)
#define exe_SetWindowTextA GAME_DATA(setwindowtext_t, 0x3a13d8)
#define exe_GetWindowTextA GAME_DATA(getwindowtext_t, 0x3a1458)
#define exe_GetWindowLongA GAME_DATA(getwindowlong_t, 0x3a1460)
#define exe_SetWindowLongA GAME_DATA(setwindowlong_t, 0x3a13c0)
#define exe_GetClientRect GAME_DATA(getrect_t, 0x3a145c)
#define exe_GetWindowRect GAME_DATA(getrect_t, 0x3a1378)
#define exe_AdjustWindowRectEx GAME_DATA(adjustwindowrectex_t, 0x3a13d4)
#define exe_CreateWindowExA GAME_DATA(createwindowex_t, 0x3a1404)
#define exe_RegisterClassA GAME_DATA(registerclass_t, 0x3a1408)
#define exe_UnregisterClassA GAME_DATA(unregisterclass_t, 0x3a1434)
#define exe_MoveWindow GAME_DATA(movewindow_t, 0x3a1438)
#define exe_SetWindowPos GAME_DATA(setwindowpos_t, 0x3a143c)
#define exe_DialogBoxParamA GAME_DATA(dialogboxparam_t, 0x3a1440)
#define exe_EndDialog GAME_DATA(enddialog_t, 0x3a1444)
#define exe_GetDlgItemTextA GAME_DATA(getdlgitemtext_t, 0x3a1448)
#define exe_GetSystemMetrics GAME_DATA(getsystemmetrics_t, 0x3a1450)
#define exe_GetSystemMenu GAME_DATA(getsystemmenu_t, 0x3a13e4)
#define exe_AppendMenuA GAME_DATA(appendmenu_t, 0x3a13e0)
#define exe_CallWindowProcA GAME_DATA(callwindowproc_t, 0x3a1430)
#define exe_BringWindowToTop GAME_DATA(bringwindowtotop_t, 0x3a13f4)

/* gdi32, winmm */
typedef void *(__stdcall *getstockobject_t)(int i);
typedef void *(__stdcall *createfontindirect_t)(const void *logfont);
typedef uint32_t(__stdcall *timeperiod_t)(uint32_t ms);
#define exe_GetStockObject GAME_DATA(getstockobject_t, 0x3a10b0)
#define exe_CreateFontIndirectA GAME_DATA(createfontindirect_t, 0x3a1090)
#define exe_timeBeginPeriod GAME_DATA(timeperiod_t, 0x3a1470)
#define exe_timeEndPeriod GAME_DATA(timeperiod_t, 0x3a146c)

/* kernel32 */
typedef void *(__stdcall *virtualalloc_t)(void *addr, uint32_t size, uint32_t type, uint32_t protect);
typedef int(__stdcall *virtualfree_t)(void *addr, uint32_t size, uint32_t type);
typedef uint32_t(__stdcall *getmodulefilename_t)(void *module, char *buf, uint32_t size);
typedef void(__stdcall *outputdebugstring_t)(const char *text);
typedef void(__stdcall *sleep_t)(uint32_t ms);
typedef void *(__stdcall *loadlibrary_t)(const char *name);
typedef int(__stdcall *handlefn_t)(void *h);		/* FreeLibrary, CloseHandle */
typedef void *(__stdcall *createfile_t)(const char *name, uint32_t access, uint32_t share, void *sa, uint32_t disp,
	uint32_t attr, void *tmpl);
typedef int(__stdcall *writefile_t)(void *h, const void *buf, uint32_t n, uint32_t *written, void *ov);
#define exe_VirtualAlloc GAME_DATA(virtualalloc_t, 0x3a113c)
#define exe_VirtualFree GAME_DATA(virtualfree_t, 0x3a1138)
#define exe_GetModuleFileNameA GAME_DATA(getmodulefilename_t, 0x3a1180)
#define exe_OutputDebugStringA GAME_DATA(outputdebugstring_t, 0x3a1184)
#define exe_Sleep GAME_DATA(sleep_t, 0x3a118c)
#define exe_LoadLibraryA GAME_DATA(loadlibrary_t, 0x3a1278)
#define exe_FreeLibrary GAME_DATA(handlefn_t, 0x3a1274)
#define exe_CreateFileA GAME_DATA(createfile_t, 0x3a1268)
#define exe_WriteFile GAME_DATA(writefile_t, 0x3a126c)
#define exe_CloseHandle GAME_DATA(handlefn_t, 0x3a1270)

/* advapi32, shell32, comdlg32 */
typedef int32_t(__stdcall *regopenkey_t)(uintptr_t key, const char *sub, void **out);
typedef int32_t(__stdcall *regqueryvalueex_t)(void *key, const char *name, uint32_t *reserved, uint32_t *type,
	void *data, uint32_t *size);
typedef int32_t(__stdcall *regclosekey_t)(void *key);
typedef int32_t(__stdcall *shgetfolderpath_t)(void *hwnd, int csidl, void *token, uint32_t flags, char *path);
typedef int(__stdcall *getsavefilename_t)(void *ofn);
#define exe_RegOpenKeyA GAME_DATA(regopenkey_t, 0x3a100c)
#define exe_RegQueryValueExA GAME_DATA(regqueryvalueex_t, 0x3a1010)
#define exe_RegCloseKey GAME_DATA(regclosekey_t, 0x3a1008)
#define exe_SHGetFolderPathA GAME_DATA(shgetfolderpath_t, 0x3a1334)
#define exe_GetSaveFileNameA GAME_DATA(getsavefilename_t, 0x3a1054)

/* Galaxy.dll: galaxy::api::Friends() */
typedef void *(__cdecl *galaxyget_t)(void);
#define exe_GalaxyFriends GAME_DATA(galaxyget_t, 0x3a10d0)

#endif
