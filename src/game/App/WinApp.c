/*
 * WinMain.cpp of the game: CWinApp, the Win32 part of the application (window, message loop, timer, paths, registry).
 * See include/scrap/app.h. The console window of the original (RichEdit output, edit line, history) is gone: the
 * console output goes to the log and OutputDebugString, and the in-game console takes the commands.
 */
#include <stdio.h>
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/filesys.h"
#include "scrap/mathlib.h"
#include "scrap/scrapmain.h"
#include "scrap/video.h"
#include "scrap/win32.h"

#define WM_ACTIVATE 0x0006
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_ACTIVATEAPP 0x001c
#define WM_HELP 0x0053
#define WM_CONTEXTMENU 0x007b
#define WM_NCHITTEST 0x0084
#define WM_KEYFIRST 0x0100
#define WM_CHAR 0x0102
#define WM_KEYLAST 0x0109
#define WM_COMMAND 0x0111
#define WM_SYSCOMMAND 0x0112
#define WM_MOUSEFIRST 0x0200
#define WM_MOUSELAST 0x020e
#define WM_ENTERMENULOOP 0x0211
#define WM_EXITMENULOOP 0x0212
#define WM_SIZING 0x0214
#define WM_MOVING 0x0216
#define WM_ENTERSIZEMOVE 0x0231
#define WM_EXITSIZEMOVE 0x0232
#define SC_SIZE 0xf000
#define SC_MOVE 0xf010
#define SC_MAXIMIZE 0xf030
#define SC_KEYMENU 0xf100
#define SC_MONITORPOWER 0xf170

#define APP_WNDPROC GAME_FUNC(App_WndProc)

CWinApp *__thiscall CWinApp_ctor(CWinApp *self, void *hInst, void *hPrev, const char *cmdLine, int nCmdShow) {
	char path[0x104];

	CApp_ctor(&self->base, cmdLine);
	self->base.vftable = g_CWinAppVftable;
	self->hInstance = hInst;
	self->hPrevInstance = hPrev;
	self->nCmdShow = nCmdShow;
	self->dedicatedWin = 0;
	exe_GetModuleFileNameA(NULL, path, 0x103);
	Dir_Change(path, "../");
	if ((byte)CWinApp_HasArg(self, "txf_helper")) {
		cString_Set((cString *)&g_LogName, "logtxf.html");
	}
	return self;
}

void __fastcall CWinApp_dtor(CWinApp *self) {
	self->base.vftable = g_CWinAppVftable;
	CApp_dtor(&self->base);
}

/* vf03 */
CWinApp *__thiscall CWinApp_Delete(CWinApp *self, byte flags) {
	CWinApp_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CWinApp));
	}
	return self;
}

/* the window procedure of "ScrapClass" (App_WndProc with g_pApp); the rest goes to the D3D application */
intptr_t __thiscall CWinApp_WndProc(CWinApp *self, void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam) {
	char title[0x40];

	switch (msg) {
	case WM_ACTIVATE:
		if ((uint16_t)wParam && !(wParam >> 16)) {
			Input_ReleaseAll(0);
		}
		break;
	case WM_CLOSE:
		exe_PostMessageA(hwnd, WM_QUIT, 0, 0);
		return 0;
	case WM_ACTIVATEAPP:
		if (g_InactiveBackgroundApp || wParam) {
			g_pApp->active = wParam != 0;
		}
		D3DApp_OnActivate();
		return 0;
	case WM_HELP:
		return 0;
	case WM_CONTEXTMENU:
		if ((byte)Engine_IsFullScreen()) {
			return 0;
		}
		break;
	case WM_NCHITTEST:
		if ((byte)Engine_IsFullScreen()) {
			return 1;
		}
		break;
	case WM_CHAR:
		Input_OnChar((int)wParam);
		return 0;
	case WM_COMMAND:
		if ((uint16_t)wParam == 0x9c46) {
			exe_SendMessageA(hwnd, WM_CLOSE, 0, 0);
			return 0;
		}
		break;
	case WM_SYSCOMMAND:
		switch (wParam) {
		case SC_SIZE:
		case SC_MOVE:
		case SC_MAXIMIZE:
		case SC_KEYMENU:
		case SC_MONITORPOWER:
			if ((byte)Engine_IsFullScreen()) {
				return 0;
			}
			break;
		}
		break;
	case WM_ENTERMENULOOP:
	case WM_ENTERSIZEMOVE:
		Input_ReleaseAll(0);
		break;
	case WM_EXITMENULOOP:
	case WM_EXITSIZEMOVE:
		if (exe_GetActiveWindow() != hwnd || exe_IsIconic(hwnd)) {
			Input_ReleaseAll(0);
		}
		break;
	case WM_SIZING: {
		const exe_rect *r = (const exe_rect *)lParam;
		Str_Printf(title, sizeof(title), "Size %dx%d", r->right - r->left, r->bottom - r->top);
		exe_SetWindowTextA(self->hwnd, title);
		return 1;
	}
	case WM_MOVING:
		return 1;
	}
	return D3DApp_MsgProc(hwnd, msg, wParam, lParam);
}

/* vf17: the console window is gone */
int __thiscall CWinApp_ConsoleAdd(CWinApp *self, const char *text, uint32_t color) {
	(void)self, (void)text, (void)color;
	return 0;
}

/* vf16: the console window is gone */
int __thiscall CWinApp_ShowConsole(CWinApp *self, byte show) {
	(void)self, (void)show;
	return 0;
}

/* the class "ScrapClass" and the main window */
byte __fastcall CWinApp_CreateMainWindow(CWinApp *self) {
	self->wc.hCursor = exe_LoadCursorA(NULL, (const char *)0x7f00);
	self->wc.hIcon = exe_LoadIconA(self->hInstance, (const char *)0x65);
	self->wc.menuName = NULL;
	self->wc.className = g_ScrapClass;
	self->wc.hbrBackground = exe_GetStockObject(4);
	self->wc.hInstance = self->hInstance;
	self->wc.style = 0x20;
	self->wc.wndProc = APP_WNDPROC;
	self->wc.clsExtra = 0;
	self->wc.wndExtra = 0;
	if (!exe_RegisterClassA(&self->wc)) {
		return 0;
	}
	self->hwnd = CWinApp_CreateWindow(self);
	return self->hwnd != NULL;
}

/* 640 x 480 (-widewindow 640 x 360), -dedicated a 350 x 1 window without the maximize box */
void *__fastcall CWinApp_CreateWindow(CWinApp *self) {
	exe_rect r;
	int w, h;

	byte wide = (byte)CWinApp_HasArg(self, "widewindow");
	r.right = wide ? g_WideWinWidth : g_WinWidth;
	r.bottom = wide ? g_WideWinHeight : g_WinHeight;
	r.left = 0;
	r.top = 0;
	self->dedicatedWin = (byte)CWinApp_HasArg(self, "dedicated");
	if (self->dedicatedWin) {
		r.right = 0x15e;
		r.bottom = 1;
	}
	if (exe_AdjustWindowRectEx(&r, 0x80cc0000, 0, 0)) {
		w = r.right - r.left;
		h = r.bottom - r.top;
	} else {
		w = g_WinWidth;
		h = g_WinHeight;
	}
	return exe_CreateWindowExA(0, g_ScrapClass, g_ScrapTitle, self->dedicatedWin ? 0x80ca0000 : 0x80ce0000, 0, 0, w,
		h, NULL, NULL, self->hInstance, NULL);
}

/* vf18: the original asked in a modal dialog of the console window, which is gone: no text (None for Python) */
const char *__thiscall CWinApp_DebugInput(CWinApp *self, const char *prompt) {
	(void)self, (void)prompt;
	return NULL;
}

/* vf08: no textures and no sound for -dedicated */
void __thiscall CWinApp_InitDedicated(CWinApp *self) {
	(void)self;
	g_LoadTextures = 0;
	g_SoundInit = 0;
}

/* vf06 */
void __thiscall CWinApp_Shutdown(CWinApp *self) {
	CApp_Shutdown(&self->base);
	exe_DestroyWindow(self->hwnd);
	exe_timeEndPeriod(1);
	App_SetExceptionFilter(0);
}

/* vf09 */
void __thiscall CWinApp_Exit(CWinApp *self) {
	exe_PostMessageA(self->hwnd, WM_QUIT, 0, 0);
}

/* vf13 */
void __thiscall CWinApp_ResetParams(CWinApp *self) {
	self->console = 0;
	CApp_ResetParams(&self->base);
}

/* "-name" in the command line (case-insensitive), followed by ':', ' ' or the end */
int __thiscall CWinApp_HasArg(CWinApp *self, const char *name) {
	char buf[0x400];

	Str_Copy(buf, self->base.cmdLine, 0x400);
	crt_strlwr(buf);
	const char *p = strstr(buf, name);
	if (!p) {
		return 0;
	}
	char prev = p > buf ? p[-1] : 0;
	char next = p[strlen(name)];
	if (prev == '-' && strchr(": ", next)) {
		return 1;
	}
	return 0;
}

void App_GetPath(char *buf, int size) {
	exe_GetModuleFileNameA(NULL, buf, (uint32_t)(size - 1));
}

/* a string value of HKEY_LOCAL_MACHINE\Software\MercurySteam Entertainment\Scrapland, NULL when missing or empty */
const char *__thiscall App_RegGetString(CWinApp *self, const char *name) {
	void *key;
	uint32_t type, size;

	(void)self;
	g_RegValue[0] = 0;
	if (exe_RegOpenKeyA(0x80000002, "Software\\MercurySteam Entertainment\\Scrapland", &key)) {
		return NULL;
	}
	size = 0x200;
	exe_RegQueryValueExA(key, name, NULL, &type, g_RegValue, &size);
	exe_RegCloseKey(key);
	if (type != 1) {
		return NULL;
	}
	return g_RegValue[0] ? g_RegValue : NULL;
}

void *App_GetHInstance(void) {
	return g_pWinApp->hInstance;
}

/* vf14: seconds since the first call, by the performance counter if there is one, else by timeGetTime */
double __thiscall CWinApp_GetTime(CWinApp *self) {
	(void)self;
	if (!g_TimeInit) {
		g_TimeInit = 1;
		g_TimeUseQP = (byte)Timer_GetQPFreq64(&g_TimeQPFreq);
		Timer_GetFreq64(&g_TimeQPStart);
		g_TimeMsStart = exe_timeGetTime();
	}
	if (g_TimeUseQP) {
		int64_t now;
		Timer_GetFreq64(&now);
		uint64_t c = (uint64_t)now - g_TimeQPPause - (uint64_t)g_TimeQPStart;
		double t = Int64_ToDouble((int64_t)c);
		return t / Int64_ToDouble(g_TimeQPFreq);
	}
	uint32_t ms = exe_timeGetTime() - g_TimeMsPause - g_TimeMsStart;
	double d = (double)(int32_t)ms + ((int32_t)ms < 0 ? 4294967296.0 : 0.0);
	return d / 1000.0;
}

void *App_GetHWnd(void) {
	return g_pWinApp->hwnd;
}

/* the part of CWinApp_Init that waits, driven by the message loop (CWinApp_InitStep) */
enum {
	APPINIT_DONE,
	APPINIT_VIDEOS,			/* for the videos of Engine_Init in CApp_Init */
};
static int app_init;

/* the end of the init. Differs from the original on purpose (no store features): the player name stays the one of the
 * config, the original took the GOG or Steam name */
static void CWinApp_InitEnd(CWinApp *self) {
	exe_SetForegroundWindow(self->hwnd);
	Con_Print(0, "Scrap engine initialized!\n\n", 0);
}

/* one pass of the message loop while the init waits for the videos of CApp_Init */
static void CWinApp_InitStep(CWinApp *self) {
	if (CApp_InitResume(&self->base)) {
		app_init = APPINIT_DONE;
		CWinApp_InitEnd(self);
	}
}

/* vf04: the window, the language, then CApp_Init, which may leave its videos to the message loop
 * (CWinApp_InitStep) */
byte __thiscall CWinApp_Init(CWinApp *self) {
	App_SetExceptionFilter(1);
	exe_timeBeginPeriod(1);
	if (!CWinApp_CreateMainWindow(self)) {
		CheckFailed("CreateMainWindow()", __FILE__, __TIMESTAMP__, __LINE__);
	}
	/* differs from the original on purpose: without the registry LANGUAGE it is english (the original took the one of
	 * GOG later, english in a normal install), not the "French" default of SelectedLanguage */
	const char *lang = App_RegGetString(self, "LANGUAGE");
	cString_Set(g_LangName, lang ? lang : "english");
	Font_SetLanguage(*g_LangName);
	self->console = (byte)CWinApp_HasArg(self, "console");
	self->dedicatedWin = (byte)CWinApp_HasArg(self, "dedicated");
	byte shown = 0;
	if (self->console || self->dedicatedWin) {
		shown = 1;
		exe_ShowWindow(self->hwnd, 5);
	}
	if (shown) {
		exe_BringWindowToTop(self->hwnd);
	}
	/* differs from the original on purpose (no store features): no Steam or GOG init and no sign-in */
	if (!CApp_Init(&self->base)) {
		return 0;
	}
	if (Engine_VideoWait()) {
		app_init = APPINIT_VIDEOS;
		return 1;
	}
	CWinApp_InitEnd(self);
	return 1;
}

/* vf05: CApp_Frame (the original ran the GOG and Steam callbacks first) */
byte __thiscall CWinApp_Frame(CWinApp *self) {
	/* differs from the original on purpose (no sleeps): no Sleep(250) while the app is in the background; CApp_Frame
	 * then only takes the time and returns 0, the PeekMessage loop of App_WinMain calls it again */
	return CApp_Frame(&self->base);
}

/* the console output besides the log: OutputDebugString (the console window of the original is gone) */
void ConsoleWin_Add(const char *text, uint32_t color) {
	(void)color;
	exe_OutputDebugStringA(text);
}

/* strcat of the original, cut at size */
static void App_StrCat(char *buf, int size, const char *s) {
	size_t n = strlen(buf);
	while (*s && (int)n + 1 < size) {
		buf[n++] = *s++;
	}
	buf[n] = 0;
}

/* My Documents\MercurySteam\Scrapland\ (made if missing), "" with the dedicated window; differs from the original on
 * purpose (no store features): no subfolder of the GOG id */
void App_GetConfigDir(char *buf, int size) {
	if (g_pApp->vftable->IsDedicatedWin(g_pApp)) {
		*buf = 0;
		return;
	}
	exe_SHGetFolderPathA(NULL, 0x8005, NULL, 0, buf);
	App_StrCat(buf, size, "\\MercurySteam");
	crt_mkdir(buf);
	App_StrCat(buf, size, "\\Scrapland");
	crt_mkdir(buf);
	App_StrCat(buf, size, "\\");
}

/* vf12: -console, -inifile:name, else CApp_ParseParam */
byte __thiscall CWinApp_ParseParam(CWinApp *self, const char *param, const char *ext) {
	if (!crt_stricmp("console", param)) {
		self->console = 1;
		return 1;
	}
	if (!crt_stricmp("inifile", param)) {
		Engine_SetIniName(ext);
		return 1;
	}
	return CApp_ParseParam(&self->base, param, ext);
}

/* vf19: starts exe (from the game directory) with the command line, without waiting */
byte __thiscall CWinApp_RunApp(CWinApp *self, const char *exe, const char *cmdLine, int arg) {
	char path[0x104], cmd[0x208], args[0x208];

	(void)self;
	(void)arg;
	exe_GetModuleFileNameA(NULL, path, 0x103);
	Dir_Change(path, ".");
	snprintf(cmd, sizeof(cmd), "%s", exe);
	snprintf(args, sizeof(args), "%s %s", exe, cmdLine);
	if (crt_spawnlp(1, cmd, args, NULL) == -1) {
		/* the original sprintf has no limit; cut at the buffer */
		snprintf(args, sizeof(args), "RunApp Error!\n Problem opening: \"%.480s\"\n", cmd);
		Con_Print(0xff, args, 0);
		return 0;
	}
	return 1;
}

intptr_t __stdcall App_WndProc(void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam) {
	if (g_pApp) {
		return CWinApp_WndProc(g_pWinApp, hwnd, msg, wParam, lParam);
	}
	return exe_DefWindowProcA(hwnd, msg, wParam, lParam);
}

/* vf20 */
byte __thiscall CWinApp_IsDedicatedWin(CWinApp *self) {
	return self->dedicatedWin;
}

/* the init, a video or the rest of Engine_LevelEnd waits (the original blocked in them) */
static byte App_Waiting(void) {
	return app_init != APPINIT_DONE || Video_Busy() || Engine_VideoWait();
}

/*
 * differs from the original on purpose (no blocking waits): while the app waits, the keyboard and mouse input stays
 * in the queue (the original handled no message while it waited, so the input came after the wait); the other
 * messages are handled, so the window answers and can be moved or closed
 */
static int App_PeekMessage(exe_msg *msg, byte waiting) {
	if (!waiting) {
		return exe_PeekMessageA(msg, NULL, 0, 0, 1);
	}
	return exe_PeekMessageA(msg, NULL, 0, WM_KEYFIRST - 1, 1)
		|| exe_PeekMessageA(msg, NULL, WM_KEYLAST + 1, WM_MOUSEFIRST - 1, 1)
		|| exe_PeekMessageA(msg, NULL, WM_MOUSELAST + 1, 0xffffffffu, 1);
}

/* the message loop: a frame whenever no message waits */
int __stdcall App_WinMain(void *hInst, void *hPrev, char *cmdLine, int nCmdShow) {
	exe_msg msg;
	byte quitLater = 0;
	uintptr_t quitCode = 0;

	void *acc = exe_LoadAcceleratorsA(NULL, (const char *)0xc8);
	CWinApp *app = operator_new(sizeof(CWinApp));
	g_pApp = app ? &CWinApp_ctor(app, hInst, hPrev, cmdLine, nCmdShow)->base : NULL;
	if (!g_pApp->vftable->Init(g_pApp)) {
		return 0;
	}
	for (;;) {
		byte waiting = App_Waiting();
		if (!App_PeekMessage(&msg, waiting)) {
			if (waiting) {
				/* differs from the original on purpose (no blocking waits): one pass of what the original waited
				 * for: the video, the rest of the init or of the level load after a video */
				if (Video_Busy()) {
					Video_Step();
				} else if (app_init != APPINIT_DONE) {
					CWinApp_InitStep(g_pWinApp);
				} else {
					CApp_FrameResume();
				}
				continue;
			}
			if (quitLater) {
				msg.wParam = quitCode;
				break;
			}
			/* the rest of a dedicated server frame that waits for a packet (the original blocked in Transport_Wait) */
			if (CApp_FrameResume()) {
				continue;
			}
			/* differs from the original on purpose (no sleeps): no frame while the frame limiter holds it back, the
			 * original slept in CEngineApp_Render */
			if (!g_Engine || Engine_FrameDue(g_Engine)) {
				g_pApp->vftable->Frame(g_pApp);
			}
			continue;
		}
		if (msg.message == WM_QUIT) {
			if (!waiting) {
				break;
			}
			/* closed while it waits: the quit comes after the wait */
			quitLater = 1;
			quitCode = msg.wParam;
			continue;
		}
		if (!exe_TranslateAcceleratorA(g_pWinApp->hwnd, acc, &msg)) {
			exe_TranslateMessage(&msg);
			exe_DispatchMessageA(&msg);
		}
	}
	g_pApp->vftable->Shutdown(g_pApp);
	if (g_pApp) {
		g_pApp->vftable->Delete(g_pApp, 1);
	}
	g_pApp = NULL;
	return (int)msg.wParam;
}
