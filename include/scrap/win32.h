/*
 * Windows functions the game imports, called through an import slot as in the original (GAME_IMPORT of game.h): in
 * ScrapC.exe its own imports of the mingw-w64 import libraries, in the hook DLL the import table of the exe. The types
 * are the game's own, so this header needs no windows.h.
 */
#ifndef SCRAP_WIN32_H
#define SCRAP_WIN32_H

#include "scrap/game.h"

typedef struct exe_rect {
	int32_t left, top, right, bottom;
} exe_rect;

typedef struct exe_point {
	int32_t x, y;
} exe_point;

typedef int(__stdcall *messagebox_t)(void *hwnd, const char *text, const char *caption, unsigned type);
typedef int(__stdcall *showcursor_t)(int show);
typedef void *(__stdcall *getdc_t)(void *hwnd);
typedef int(__stdcall *releasedc_t)(void *hwnd, void *dc);
typedef void *(__stdcall *createsolidbrush_t)(uint32_t color);
typedef int(__stdcall *deleteobject_t)(void *obj);
typedef int(__stdcall *rectbrush_t)(void *dc, const exe_rect *rect, void *brush);

#define exe_MessageBoxA GAME_IMPORT(messagebox_t, MessageBoxA, 16, 0x3a142c)
#define exe_ShowCursor GAME_IMPORT(showcursor_t, ShowCursor, 4, 0x3a13a0)
#define exe_GetDC GAME_IMPORT(getdc_t, GetDC, 4, 0x3a1390)
#define exe_ReleaseDC GAME_IMPORT(releasedc_t, ReleaseDC, 8, 0x3a1394)
#define exe_FillRect GAME_IMPORT(rectbrush_t, FillRect, 12, 0x3a1398)
#define exe_FrameRect GAME_IMPORT(rectbrush_t, FrameRect, 12, 0x3a139c)
#define exe_CreateSolidBrush GAME_IMPORT(createsolidbrush_t, CreateSolidBrush, 4, 0x3a107c)
#define exe_DeleteObject GAME_IMPORT(deleteobject_t, DeleteObject, 4, 0x3a1080)
typedef uint32_t(__stdcall *settextcolor_t)(void *dc, uint32_t color);
typedef int(__stdcall *setbkmode_t)(void *dc, int mode);
typedef int(__stdcall *exttextout_t)(void *dc, int x, int y, unsigned options, const exe_rect *rect, const char *s,
	unsigned n, const int *dx);
#define exe_SetTextColor GAME_IMPORT(settextcolor_t, SetTextColor, 8, 0x3a1070)
#define exe_SetBkColor GAME_IMPORT(settextcolor_t, SetBkColor, 8, 0x3a1074)
#define exe_SetBkMode GAME_IMPORT(setbkmode_t, SetBkMode, 8, 0x3a106c)
#define exe_ExtTextOutA GAME_IMPORT(exttextout_t, ExtTextOutA, 32, 0x3a1094)
typedef int(__stdcall *clienttoscreen_t)(void *hwnd, exe_point *pt);
typedef int(__stdcall *setstretchbltmode_t)(void *dc, int mode);
typedef int(__stdcall *stretchblt_t)(void *dc, int x, int y, int w, int h, void *src, int sx, int sy, int sw, int sh,
	uint32_t rop);
#define exe_ClientToScreen GAME_IMPORT(clienttoscreen_t, ClientToScreen, 8, 0x3a13a4)
#define exe_SetStretchBltMode GAME_IMPORT(setstretchbltmode_t, SetStretchBltMode, 8, 0x3a10a4)
#define exe_StretchBlt GAME_IMPORT(stretchblt_t, StretchBlt, 44, 0x3a10a0)
typedef void *(__stdcall *createpen_t)(int style, int width, uint32_t color);
typedef void *(__stdcall *selectobject_t)(void *dc, void *obj);
typedef int(__stdcall *movetoex_t)(void *dc, int x, int y, void *old);
typedef int(__stdcall *lineto_t)(void *dc, int x, int y);
#define exe_CreatePen GAME_IMPORT(createpen_t, CreatePen, 12, 0x3a1078)
#define exe_SelectObject GAME_IMPORT(selectobject_t, SelectObject, 8, 0x3a1088)
#define exe_MoveToEx GAME_IMPORT(movetoex_t, MoveToEx, 16, 0x3a108c)
#define exe_LineTo GAME_IMPORT(lineto_t, LineTo, 12, 0x3a1084)

/* kernel32 */
typedef uint32_t(__stdcall *getfileattributes_t)(const char *name);
typedef int(__stdcall *setfileattributes_t)(const char *name, uint32_t attr);

#define exe_GetFileAttributesA GAME_IMPORT(getfileattributes_t, GetFileAttributesA, 4, 0x3a1284)
typedef uint64_t(__stdcall *gettickcount64_t)(void);
#define exe_GetTickCount64 GAME_IMPORT(gettickcount64_t, GetTickCount64, 0, 0x3a1154)
#define exe_SetFileAttributesA GAME_IMPORT(setfileattributes_t, SetFileAttributesA, 8, 0x3a1288)
typedef int(__stdcall *lstrlen_t)(const char *s);
#define exe_lstrlenA GAME_IMPORT(lstrlen_t, lstrlenA, 4, 0x3a12b8)
typedef int(__stdcall *multibytetowidechar_t)(uint32_t cp, uint32_t flags, const char *s, int n, uint16_t *w, int wn);
typedef int(__stdcall *widechartomultibyte_t)(uint32_t cp, uint32_t flags, const uint16_t *w, int wn, char *s, int n,
	const char *defChar, int *usedDef);
#define exe_MultiByteToWideChar GAME_IMPORT(multibytetowidechar_t, MultiByteToWideChar, 24, 0x3a12b0)
#define exe_WideCharToMultiByte GAME_IMPORT(widechartomultibyte_t, WideCharToMultiByte, 32, 0x3a12b4)
typedef int(__stdcall *queryperformancecounter_t)(int64_t *count);
#define exe_QueryPerformanceCounter GAME_IMPORT(queryperformancecounter_t, QueryPerformanceCounter, 4, 0x3a12a8)
/* the text console of the debug output, the unhandled exceptions, the local time */
typedef void *(__stdcall *getstdhandle_t)(uint32_t which);
typedef int(__stdcall *writeconsole_t)(void *h, const void *buf, uint32_t n, uint32_t *written, void *reserved);
typedef int(__stdcall *allocconsole_t)(void);
typedef int(__stdcall *setconsoletextattribute_t)(void *h, uint16_t attr);
typedef int(__stdcall *fillconsoleoutputattribute_t)(void *h, uint16_t attr, uint32_t n, uint32_t coord,
	uint32_t *written);
typedef void *(__stdcall *setunhandledexceptionfilter_t)(void *filter);
typedef void(__stdcall *getlocaltime_t)(uint16_t *st);	/* SYSTEMTIME: year, month, ... */
#define exe_GetStdHandle GAME_IMPORT(getstdhandle_t, GetStdHandle, 4, 0x3a128c)
#define exe_WriteConsoleA GAME_IMPORT(writeconsole_t, WriteConsoleA, 20, 0x3a129c)
#define exe_AllocConsole GAME_IMPORT(allocconsole_t, AllocConsole, 0, 0x3a1298)
#define exe_SetConsoleTextAttribute GAME_IMPORT(setconsoletextattribute_t, SetConsoleTextAttribute, 8, 0x3a1294)
typedef int(__stdcall *setconsoletitle_t)(const char *title);
#define exe_SetConsoleTitleA GAME_IMPORT(setconsoletitle_t, SetConsoleTitleA, 4, 0x3a12cc)
typedef void *(__stdcall *getcurrentthread_t)(void);
typedef int(__stdcall *setthreadpriority_t)(void *thread, int priority);
#define exe_GetCurrentThread GAME_IMPORT(getcurrentthread_t, GetCurrentThread, 0, 0x3a12c4)
#define exe_SetThreadPriority GAME_IMPORT(setthreadpriority_t, SetThreadPriority, 8, 0x3a12c8)
#define exe_FillConsoleOutputAttribute \
	GAME_IMPORT(fillconsoleoutputattribute_t, FillConsoleOutputAttribute, 20, 0x3a1290)
#define exe_SetUnhandledExceptionFilter \
	GAME_IMPORT(setunhandledexceptionfilter_t, SetUnhandledExceptionFilter, 4, 0x3a1280)
#define exe_GetLocalTime GAME_IMPORT(getlocaltime_t, GetLocalTime, 4, 0x3a1188)
#define STD_OUTPUT (-11)
#define exe_QueryPerformanceFrequency GAME_IMPORT(queryperformancecounter_t, QueryPerformanceFrequency, 4, 0x3a12ac)

/* wsock32 (Winsock 1.1 by ordinal) */
typedef int(__stdcall *wsock_bind_t)(uint32_t s, const void *addr, int len);
typedef int(__stdcall *wsock_socketfn_t)(uint32_t s);
typedef int(__stdcall *wsock_ioctlsocket_t)(uint32_t s, int32_t cmd, uint32_t *arg);
typedef uint32_t(__stdcall *wsock_htonl_t)(uint32_t v);
typedef uint16_t(__stdcall *wsock_htons_t)(uint16_t v);
typedef int(__stdcall *wsock_recvfrom_t)(uint32_t s, void *buf, int len, int flags, void *from, int *fromlen);
typedef int(__stdcall *wsock_select_t)(int n, void *readfds, void *writefds, void *exceptfds, const void *timeout);
typedef int(__stdcall *wsock_sendto_t)(uint32_t s, const void *buf, int len, int flags, const void *to, int tolen);
typedef int(__stdcall *wsock_setsockopt_t)(uint32_t s, int level, int name, const void *val, int len);
typedef uint32_t(__stdcall *wsock_socket_t)(int af, int type, int protocol);
typedef void *(__stdcall *wsock_gethostbyaddr_t)(const void *addr, int len, int type);
typedef void *(__stdcall *wsock_gethostbyname_t)(const char *name);
typedef int(__stdcall *wsock_wsastartup_t)(uint16_t version, void *data);
typedef int(__stdcall *wsock_wsacleanup_t)(void);
#define exe_bind GAME_IMPORT(wsock_bind_t, bind, 12, 0x3a14a8)			/* ordinal 2 */
#define exe_closesocket GAME_IMPORT(wsock_socketfn_t, closesocket, 4, 0x3a14a4)		/* 3 */
#define exe_ioctlsocket GAME_IMPORT(wsock_ioctlsocket_t, ioctlsocket, 12, 0x3a14a0)	/* 12 */
#define exe_htonl GAME_IMPORT(wsock_htonl_t, htonl, 4, 0x3a149c)			/* 8 */
#define exe_htons GAME_IMPORT(wsock_htons_t, htons, 4, 0x3a1498)			/* 9 */
#define exe_ntohs GAME_IMPORT(wsock_htons_t, ntohs, 4, 0x3a14b4)			/* 15 */
#define exe_recvfrom GAME_IMPORT(wsock_recvfrom_t, recvfrom, 24, 0x3a14b0)		/* 17 */
#define exe_select GAME_IMPORT(wsock_select_t, select, 20, 0x3a148c)		/* 18 */
#define exe_sendto GAME_IMPORT(wsock_sendto_t, sendto, 24, 0x3a1488)		/* 20 */
#define exe_setsockopt GAME_IMPORT(wsock_setsockopt_t, setsockopt, 20, 0x3a1484)	/* 21 */
#define exe_socket GAME_IMPORT(wsock_socket_t, socket, 12, 0x3a1480)		/* 23 */
#define exe_gethostbyaddr GAME_IMPORT(wsock_gethostbyaddr_t, gethostbyaddr, 12, 0x3a147c)	/* 51 */
#define exe_gethostbyname GAME_IMPORT(wsock_gethostbyname_t, gethostbyname, 4, 0x3a1478)	/* 52 */
#define exe_WSAStartup GAME_IMPORT(wsock_wsastartup_t, WSAStartup, 8, 0x3a1490)	/* 115 */
#define exe_WSACleanup GAME_IMPORT(wsock_wsacleanup_t, WSACleanup, 0, 0x3a14ac)	/* 116 */

/* winmm */
typedef uint32_t(__stdcall *timegettime_t)(void);
#define exe_timeGetTime GAME_IMPORT(timegettime_t, timeGetTime, 0, 0x3a1468)

/* user32: the key state (negative while held) and a modeless dialog */
typedef int16_t(__stdcall *getasynckeystate_t)(int vk);
typedef void *(__stdcall *createdialogparam_t)(void *inst, const char *tmpl, void *parent, void *proc, intptr_t param);
#define exe_GetAsyncKeyState GAME_IMPORT(getasynckeystate_t, GetAsyncKeyState, 4, 0x3a1424)
#define exe_CreateDialogParamA GAME_IMPORT(createdialogparam_t, CreateDialogParamA, 20, 0x3a133c)

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
#define exe_PeekMessageA GAME_IMPORT(peekmessage_t, PeekMessageA, 20, 0x3a1418)
#define exe_DispatchMessageA GAME_IMPORT(dispatchmessage_t, DispatchMessageA, 4, 0x3a141c)
#define exe_TranslateMessage GAME_IMPORT(translatemessage_t, TranslateMessage, 4, 0x3a1420)
#define exe_TranslateAcceleratorA GAME_IMPORT(translateaccelerator_t, TranslateAcceleratorA, 12, 0x3a13e8)
#define exe_LoadAcceleratorsA GAME_IMPORT(loadres_t, LoadAcceleratorsA, 8, 0x3a13ec)
#define exe_LoadIconA GAME_IMPORT(loadres_t, LoadIconA, 8, 0x3a13c8)
#define exe_LoadCursorA GAME_IMPORT(loadres_t, LoadCursorA, 8, 0x3a13cc)
#define exe_SendMessageA GAME_IMPORT(sendmessage_t, SendMessageA, 16, 0x3a1414)
#define exe_PostMessageA GAME_IMPORT(sendmessage_t, PostMessageA, 16, 0x3a1410)
#define exe_DefWindowProcA GAME_IMPORT(wndproc_t, DefWindowProcA, 16, 0x3a140c)
#define exe_IsIconic GAME_IMPORT(hwndfn_t, IsIconic, 4, 0x3a13f8)
#define exe_DestroyWindow GAME_IMPORT(hwndfn_t, DestroyWindow, 4, 0x3a1400)
#define exe_UpdateWindow GAME_IMPORT(hwndfn_t, UpdateWindow, 4, 0x3a1454)
#define exe_SetForegroundWindow GAME_IMPORT(hwndfn_t, SetForegroundWindow, 4, 0x3a13dc)
#define exe_GetForegroundWindow GAME_IMPORT(gethwnd_t, GetForegroundWindow, 0, 0x3a1428)
#define exe_GetActiveWindow GAME_IMPORT(gethwnd_t, GetActiveWindow, 0, 0x3a13f0)
#define exe_SetFocus GAME_IMPORT(sethwnd_t, SetFocus, 4, 0x3a144c)
#define exe_ShowWindow GAME_IMPORT(showwindow_t, ShowWindow, 8, 0x3a13fc)
#define exe_SetWindowTextA GAME_IMPORT(setwindowtext_t, SetWindowTextA, 8, 0x3a13d8)
#define exe_GetWindowTextA GAME_IMPORT(getwindowtext_t, GetWindowTextA, 12, 0x3a1458)
#define exe_GetWindowLongA GAME_IMPORT(getwindowlong_t, GetWindowLongA, 8, 0x3a1460)
#define exe_SetWindowLongA GAME_IMPORT(setwindowlong_t, SetWindowLongA, 12, 0x3a13c0)
#define exe_GetClientRect GAME_IMPORT(getrect_t, GetClientRect, 8, 0x3a145c)
#define exe_GetWindowRect GAME_IMPORT(getrect_t, GetWindowRect, 8, 0x3a1378)
#define exe_AdjustWindowRectEx GAME_IMPORT(adjustwindowrectex_t, AdjustWindowRectEx, 16, 0x3a13d4)
#define exe_CreateWindowExA GAME_IMPORT(createwindowex_t, CreateWindowExA, 48, 0x3a1404)
#define exe_RegisterClassA GAME_IMPORT(registerclass_t, RegisterClassA, 4, 0x3a1408)
#define exe_UnregisterClassA GAME_IMPORT(unregisterclass_t, UnregisterClassA, 8, 0x3a1434)
#define exe_MoveWindow GAME_IMPORT(movewindow_t, MoveWindow, 24, 0x3a1438)
typedef int(__stdcall *getwindowplacement_t)(void *hwnd, uint32_t *wp);	/* WINDOWPLACEMENT, 0x2c bytes */
#define exe_GetWindowPlacement GAME_IMPORT(getwindowplacement_t, GetWindowPlacement, 8, 0x3a1364)
#define exe_IsWindow GAME_IMPORT(hwndfn_t, IsWindow, 4, 0x3a1360)
typedef int(__stdcall *menuitem_t)(void *menu, uint32_t id, uint32_t flags);	/* EnableMenuItem, CheckMenuItem */
#define exe_EnableMenuItem GAME_IMPORT(menuitem_t, EnableMenuItem, 12, 0x3a1350)
typedef void *(__stdcall *getmenu_t)(void *hwnd);
typedef void *(__stdcall *getsubmenu_t)(void *menu, int pos);
#define exe_GetMenu GAME_IMPORT(getmenu_t, GetMenu, 4, 0x3a1348)
#define exe_GetSubMenu GAME_IMPORT(getsubmenu_t, GetSubMenu, 8, 0x3a1354)
#define exe_DeleteMenu GAME_IMPORT(menuitem_t, DeleteMenu, 12, 0x3a1358)
#define exe_CheckMenuItem GAME_IMPORT(menuitem_t, CheckMenuItem, 12, 0x3a134c)
typedef void *(__stdcall *setcursor_t)(void *cursor);
#define exe_SetCursor GAME_IMPORT(setcursor_t, SetCursor, 4, 0x3a135c)
#define exe_SetWindowPos GAME_IMPORT(setwindowpos_t, SetWindowPos, 28, 0x3a143c)
#define exe_DialogBoxParamA GAME_IMPORT(dialogboxparam_t, DialogBoxParamA, 20, 0x3a1440)
#define exe_EndDialog GAME_IMPORT(enddialog_t, EndDialog, 8, 0x3a1444)
#define exe_GetDlgItemTextA GAME_IMPORT(getdlgitemtext_t, GetDlgItemTextA, 16, 0x3a1448)
typedef void *(__stdcall *getdlgitem_t)(void *hdlg, int id);
typedef int(__stdcall *setdlgitemtext_t)(void *hdlg, int id, const char *text);
#define exe_SetDlgItemTextA GAME_IMPORT(setdlgitemtext_t, SetDlgItemTextA, 12, 0x3a1344)
#define exe_GetDlgItem GAME_IMPORT(getdlgitem_t, GetDlgItem, 8, 0x3a1340)
#define exe_GetSystemMetrics GAME_IMPORT(getsystemmetrics_t, GetSystemMetrics, 4, 0x3a1450)
#define exe_GetSystemMenu GAME_IMPORT(getsystemmenu_t, GetSystemMenu, 8, 0x3a13e4)
#define exe_AppendMenuA GAME_IMPORT(appendmenu_t, AppendMenuA, 16, 0x3a13e0)
#define exe_CallWindowProcA GAME_IMPORT(callwindowproc_t, CallWindowProcA, 20, 0x3a1430)
#define exe_BringWindowToTop GAME_IMPORT(bringwindowtotop_t, BringWindowToTop, 4, 0x3a13f4)

/* gdi32, winmm */
typedef void *(__stdcall *getstockobject_t)(int i);
typedef void *(__stdcall *createfontindirect_t)(const void *logfont);
typedef uint32_t(__stdcall *timeperiod_t)(uint32_t ms);
#define exe_GetStockObject GAME_IMPORT(getstockobject_t, GetStockObject, 4, 0x3a10b0)
#define exe_CreateFontIndirectA GAME_IMPORT(createfontindirect_t, CreateFontIndirectA, 4, 0x3a1090)
#define exe_timeBeginPeriod GAME_IMPORT(timeperiod_t, timeBeginPeriod, 4, 0x3a1470)
#define exe_timeEndPeriod GAME_IMPORT(timeperiod_t, timeEndPeriod, 4, 0x3a146c)

/* kernel32 */
typedef void *(__stdcall *virtualalloc_t)(void *addr, uint32_t size, uint32_t type, uint32_t protect);
typedef int(__stdcall *virtualfree_t)(void *addr, uint32_t size, uint32_t type);
typedef uint32_t(__stdcall *getmodulefilename_t)(void *module, char *buf, uint32_t size);
typedef void(__stdcall *outputdebugstring_t)(const char *text);
typedef void *(__stdcall *loadlibrary_t)(const char *name);
typedef int(__stdcall *handlefn_t)(void *h);		/* FreeLibrary, CloseHandle */
typedef void *(__stdcall *createfile_t)(const char *name, uint32_t access, uint32_t share, void *sa, uint32_t disp,
	uint32_t attr, void *tmpl);
typedef int(__stdcall *writefile_t)(void *h, const void *buf, uint32_t n, uint32_t *written, void *ov);
#define exe_VirtualAlloc GAME_IMPORT(virtualalloc_t, VirtualAlloc, 16, 0x3a113c)
#define exe_VirtualFree GAME_IMPORT(virtualfree_t, VirtualFree, 12, 0x3a1138)
#define exe_GetModuleFileNameA GAME_IMPORT(getmodulefilename_t, GetModuleFileNameA, 12, 0x3a1180)
#define exe_OutputDebugStringA GAME_IMPORT(outputdebugstring_t, OutputDebugStringA, 4, 0x3a1184)
/* no Sleep (0x3a118c): the C does not sleep or wait, see docs/standalone.md */
#define exe_LoadLibraryA GAME_IMPORT(loadlibrary_t, LoadLibraryA, 4, 0x3a1278)
#define exe_FreeLibrary GAME_IMPORT(handlefn_t, FreeLibrary, 4, 0x3a1274)
#define exe_CreateFileA GAME_IMPORT(createfile_t, CreateFileA, 28, 0x3a1268)
#define exe_WriteFile GAME_IMPORT(writefile_t, WriteFile, 20, 0x3a126c)
#define exe_CloseHandle GAME_IMPORT(handlefn_t, CloseHandle, 4, 0x3a1270)
typedef int(__stdcall *globalmemorystatusex_t)(void *status);	/* MEMORYSTATUSEX, 0x40 bytes */
#define exe_GlobalMemoryStatusEx GAME_IMPORT(globalmemorystatusex_t, GlobalMemoryStatusEx, 4, 0x3a127c)

/* advapi32, shell32, comdlg32 */
typedef int32_t(__stdcall *regopenkey_t)(uintptr_t key, const char *sub, void **out);
typedef int32_t(__stdcall *regqueryvalueex_t)(void *key, const char *name, uint32_t *reserved, uint32_t *type,
	void *data, uint32_t *size);
typedef int32_t(__stdcall *regclosekey_t)(void *key);
typedef int32_t(__stdcall *shgetfolderpath_t)(void *hwnd, int csidl, void *token, uint32_t flags, char *path);
typedef int(__stdcall *getsavefilename_t)(void *ofn);
#define exe_RegOpenKeyA GAME_IMPORT(regopenkey_t, RegOpenKeyA, 12, 0x3a100c)
#define exe_RegQueryValueExA GAME_IMPORT(regqueryvalueex_t, RegQueryValueExA, 24, 0x3a1010)
#define exe_RegCloseKey GAME_IMPORT(regclosekey_t, RegCloseKey, 4, 0x3a1008)
#define exe_SHGetFolderPathA GAME_IMPORT(shgetfolderpath_t, SHGetFolderPathA, 20, 0x3a1334)
#define exe_GetSaveFileNameA GAME_IMPORT(getsavefilename_t, GetSaveFileNameA, 4, 0x3a1054)

/*
 * dinput8: in ScrapC.exe the one of the system folder, which main.c loads at start. ScrapC.exe runs in game/Bin, where
 * dinput8.dll is the hook DLL, which an import of the exe would load.
 */
typedef int32_t(__stdcall *dinput8create_t)(void *hinst, uint32_t version, const void *iid, void **out, void *outer);
extern dinput8create_t sd_DirectInput8Create;
#define exe_DirectInput8Create sd_DirectInput8Create

#endif
