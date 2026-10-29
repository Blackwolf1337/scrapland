/*
 * WinMain.cpp of the game: CWinApp, the Win32 part of the application (window, message loop, timer, paths, registry)
 * and the console window of the Win32 build (a RichEdit output, an edit line with a 64 line history and four
 * buttons). See include/scrap/app.h. Window procedures, dialog and stream callbacks are given to Windows by their exe
 * addresses, as the original does.
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

#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_SIZE 0x0005
#define WM_ACTIVATE 0x0006
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_ACTIVATEAPP 0x001c
#define WM_SETFONT 0x0030
#define WM_HELP 0x0053
#define WM_CONTEXTMENU 0x007b
#define WM_NCHITTEST 0x0084
#define WM_KEYFIRST 0x0100
#define WM_KEYDOWN 0x0100
#define WM_CHAR 0x0102
#define WM_SYSKEYDOWN 0x0104
#define WM_KEYLAST 0x0109
#define WM_INITDIALOG 0x0110
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
#define EM_SETSEL 0x00b1
#define EM_SCROLLCARET 0x00b7
#define EM_REPLACESEL 0x00c2
#define EM_LIMITTEXT 0x00c5
#define EM_SETBKGNDCOLOR 0x0443
#define EM_SETCHARFORMAT 0x0444
#define EM_SETEVENTMASK 0x0445
#define EM_STREAMOUT 0x044a
#define SC_SIZE 0xf000
#define SC_MOVE 0xf010
#define SC_MAXIMIZE 0xf030
#define SC_CLOSE 0xf060
#define SC_KEYMENU 0xf100
#define SC_MONITORPOWER 0xf170
#define VK_CONTROL 0x11
#define VK_UP 0x26
#define VK_DOWN 0x28
#define GWL_WNDPROC (-4)
#define GWL_HINSTANCE (-6)

/* the ids of the system menu entries and the console buttons */
#define ID_SWITCHCONSOLE 0x65
#define ID_SAVEOUTPUT 0xc9
#define ID_BTN_NEWER 0x65		/* "+" */
#define ID_BTN_OLDER 0x66		/* "-" */
#define ID_BTN_SIZE 0x67		/* "!" */
#define ID_BTN_ENTER 0x68		/* ">" */

#define APP_EMPTY GAME_STR(str_Empty, 0x3a3228)	/* "" of the exe */
#define APP_WNDPROC GAME_FUNC(App_WndProc, 0x20d840)
#define CONWIN_DLGPROC GAME_FUNC(ConsoleWin_DlgProc, 0x20df40)
#define CONWIN_EDITPROC GAME_FUNC(ConsoleWin_EditProc, 0x20dfc0)
#define CONWIN_STREAMOUT GAME_FUNC(ConsoleWin_StreamOut, 0x20e460)
#define CONWIN_WNDPROC GAME_FUNC(ConsoleWin_WndProc, 0x20e690)
#define CONSOLE_EXECUTE ((void (*)(const char *))GAME_FUNC(Console_Execute, 0x20aaf0))

/* the console window: 0x1a9 x 0x1f4 */
#define CONWIN_W 0x1a9
#define CONWIN_H 0x1f4

CWinApp *__thiscall CWinApp_ctor(CWinApp *self, void *hInst, void *hPrev, const char *cmdLine, int nCmdShow) {
	char path[0x104];

	CApp_ctor(self, cmdLine);
	self->vftable = g_CWinAppVftable;
	GogListeners_ctor(&self->gog);
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
	self->vftable = g_CWinAppVftable;
	GogListeners_dtor(&self->gog);
	CApp_dtor(self);
}

/* also the thunk at rva 0x20c910 */
void __fastcall GogListeners_dtor(cGogListeners *self) {
	void **p = self->begin;
	if (!p) {
		return;
	}
	void **end = self->end;
	for (; p != end; p++) {
		void *o = *p;
		if (o) {
			(*(void(__thiscall **)(void *, int)) * (void ***)o)(o, 1);
		}
	}
	PtrVector_Deallocate(self, self->begin, (int)(self->cap - self->begin));
	self->begin = NULL;
	self->end = NULL;
	self->cap = NULL;
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
		switch ((uint16_t)wParam) {
		case 0x9c43: {
			void *fg = exe_GetForegroundWindow();
			if (fg == ConsoleWin_GetHwnd()) {
				ConsoleWin_ToggleSize();
			}
			break;
		}
		case 0x9c46:
			exe_SendMessageA(hwnd, WM_CLOSE, 0, 0);
			return 0;
		case 0x9ca6:
			ConsoleWin_Focus(g_pApp->hwnd);
			break;
		}
		break;
	case WM_SYSCOMMAND:
		switch (wParam) {
		case ID_SWITCHCONSOLE:
			if (!ConsoleWin_Open(self->hwnd, (byte)(self->flags >> 2 & 1))) {
				ConsoleWin_Close();
			}
			break;
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

/* vf17 */
int __thiscall CWinApp_ConsoleAdd(CWinApp *self, const char *text, uint32_t color) {
	(void)self;
	return ConsoleWin_Print(text, color);
}

/* vf16 */
int __thiscall CWinApp_ShowConsole(CWinApp *self, byte show) {
	(void)self;
	return ConsoleWin_Show(show);
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

/* vf18 */
const char *__thiscall CWinApp_DebugInput(CWinApp *self, const char *prompt) {
	(void)self;
	return ConsoleWin_Input(prompt);
}

/* vf08: no textures and no sound for -dedicated */
void __thiscall CWinApp_InitDedicated(CWinApp *self) {
	(void)self;
	g_LoadTextures = 0;
	g_SoundInit = 0;
}

/* vf06 */
void __thiscall CWinApp_Shutdown(CWinApp *self) {
	Gog_Done(&self->gog);
	if (g_IsSteam) {
		Steam_Done();
	}
	CApp_Shutdown(self);
	ConsoleWin_Close();
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
	CApp_ResetParams(self);
}

/* "-name" in the command line (case-insensitive), followed by ':', ' ' or the end */
int __thiscall CWinApp_HasArg(CWinApp *self, const char *name) {
	char buf[0x400];

	Str_Copy(buf, self->cmdLine, 0x400);
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
	return g_pApp->hInstance;
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
	return g_pApp->hwnd;
}

/* the part of CWinApp_Init that waits, driven by the message loop (CWinApp_InitStep) */
enum {
	APPINIT_DONE,
	APPINIT_GOG,			/* for the GOG sign-in */
	APPINIT_VIDEOS,			/* for the videos of Engine_Init in CApp_Init */
};
static int app_init;

/* the end of the init: the player name of GOG or Steam, the console window */
static void CWinApp_InitEnd(CWinApp *self) {
	if (Gog_GetID()) {
		/* IFriends::GetPersonaName */
		void *friends = exe_GalaxyFriends();
		const char *name = ((const char *(__thiscall *)(void *))(*(void ***)friends)[0x14 / 4])(friends);
		cString_Set(g_PlayerName, Str_FromUTF8(name));
	}
	if (g_IsSteam) {
		/* ISteamFriends::GetPersonaName of the context the exe keeps */
		void **ctx = g_SteamInternal_ContextInit(g_SteamFriendsContext);
		void *friends = ctx[2];
		const char *name = ((const char *(__thiscall *)(void *))(*(void ***)friends)[0])(friends);
		cString_Set(g_PlayerName, Str_FromUTF8(name));
	}
	ConsoleWin_Show((byte)Engine_IsFullScreen() == 0);
	exe_SetForegroundWindow(self->hwnd);
	Con_Print(0, "Scrap engine initialized!\n\n", 0);
}

/* the init after the GOG sign-in: CApp_Init, which may leave its videos to the message loop, then CWinApp_InitEnd;
 * 0 when CApp_Init failed */
static byte CWinApp_InitRest(CWinApp *self) {
	if (!CApp_Init(self)) {
		return 0;
	}
	if (Engine_VideoWait()) {
		app_init = APPINIT_VIDEOS;
		return 1;
	}
	CWinApp_InitEnd(self);
	return 1;
}

/* one pass of the message loop while the init waits; 0 when it failed (App_WinMain then returns 0 as after a failed
 * Init) */
static byte CWinApp_InitStep(CWinApp *self) {
	if (app_init == APPINIT_GOG) {
		Gog_ProcessData(&self->gog);
		if (!g_GogReady) {
			return 1;
		}
		app_init = APPINIT_DONE;
		return CWinApp_InitRest(self);
	}
	if (CApp_InitResume(self)) {
		app_init = APPINIT_DONE;
		CWinApp_InitEnd(self);
	}
	return 1;
}

/* vf04: the window, the language, the console window, Steam and GOG, then CApp_Init */
byte __thiscall CWinApp_Init(CWinApp *self) {
	App_SetExceptionFilter(1);
	exe_timeBeginPeriod(1);
	if (!CWinApp_CreateMainWindow(self)) {
		CheckFailed("CreateMainWindow()", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (App_RegGetString(self, "LANGUAGE")) {
		cString_Set(g_LangName, App_RegGetString(self, "LANGUAGE"));
		Font_SetLanguage(*g_LangName);
	}
	void *menu = exe_GetSystemMenu(self->hwnd, 0);
	exe_AppendMenuA(menu, 0x800, 0, "");
	exe_AppendMenuA(menu, 0, ID_SWITCHCONSOLE, "S&witch console");
	g_ConExec = CONSOLE_EXECUTE;
	self->console = (byte)CWinApp_HasArg(self, "console");
	self->dedicatedWin = (byte)CWinApp_HasArg(self, "dedicated");
	byte shown = 0;
	if (self->console || self->dedicatedWin) {
		shown = 1;
		exe_ShowWindow(self->hwnd, 5);
	}
	ConsoleWin_Toggle(self->console, self->hwnd, self->dedicatedWin);
	if (shown) {
		exe_BringWindowToTop(self->hwnd);
	}
	if (self->dedicatedWin) {
		g_IsSteam = 0;
	} else if (g_IsSteam) {
		if (!Steam_Init(0)) {
			return 0;
		}
		cString_Set(g_LangName, Steam_GetLanguage());
		Font_SetLanguage(*g_LangName);
	}
	Gog_Init(&self->gog);
	cString_Set(g_LangName, Gog_GetLanguage());
	Font_SetLanguage(*g_LangName);
	Gog_ProcessData(&self->gog);
	/* differs from the original on purpose (no blocking waits): the original called Gog_ProcessData in a loop until
	 * the sign-in; now the message loop calls it once per pass and then runs the rest (CWinApp_InitStep) */
	if (!g_GogReady) {
		app_init = APPINIT_GOG;
		return 1;
	}
	return CWinApp_InitRest(self);
}

/* vf05: GOG and Steam callbacks, then CApp_Frame */
byte __thiscall CWinApp_Frame(CWinApp *self) {
	Gog_ProcessData(&self->gog);
	if (g_IsSteam) {
		Steam_RunCallbacks();
	}
	/* differs from the original on purpose (no sleeps): no Sleep(250) while the app is in the background; CApp_Frame
	 * then only takes the time and returns 0, the PeekMessage loop of App_WinMain calls it again */
	return CApp_Frame(self);
}

void ConsoleWin_Add(const char *text, uint32_t color) {
	ConsoleWin_Print(text, color);
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

/* My Documents\MercurySteam\Scrapland\[GOG id\] (made if missing), "" with the dedicated window */
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
	const char *id = Gog_GetID();
	if (id) {
		App_StrCat(buf, size, "\\");
		App_StrCat(buf, size, id);
		crt_mkdir(buf);
	}
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
	return CApp_ParseParam(self, param, ext);
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
		return CWinApp_WndProc(g_pApp, hwnd, msg, wParam, lParam);
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

/* a line entered in the console window while the app waits, run once the wait is over */
static byte app_conEnterLater;

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
	g_pApp = app ? CWinApp_ctor(app, hInst, hPrev, cmdLine, nCmdShow) : NULL;
	if (!g_pApp->vftable->Init(g_pApp)) {
		return 0;
	}
	for (;;) {
		byte waiting = App_Waiting();
		if (!App_PeekMessage(&msg, waiting)) {
			if (waiting) {
				/* differs from the original on purpose (no blocking waits): one pass of what the original waited
				 * for: the video, the GOG sign-in, the rest of the init or of the level load after a video */
				if (Video_Busy()) {
					Video_Step();
				} else if (app_init != APPINIT_DONE) {
					if (!CWinApp_InitStep(g_pApp)) {
						return 0;
					}
				} else {
					CApp_FrameResume();
				}
				continue;
			}
			if (app_conEnterLater) {
				app_conEnterLater = 0;
				ConsoleWin_Enter();
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
			/* closed while it waits: before the GOG sign-in the start ends like a failed Init, else the quit comes
			 * after the wait */
			if (app_init == APPINIT_GOG) {
				return 0;
			}
			quitLater = 1;
			quitCode = msg.wParam;
			continue;
		}
		if (!exe_TranslateAcceleratorA(g_pApp->hwnd, acc, &msg)) {
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

/* the console window */

int ConsoleWin_Toggle(byte open, void *parent, byte bottom) {
	if (open) {
		return ConsoleWin_Open(parent, bottom);
	}
	return ConsoleWin_Close();
}

/* the controls in the client area: output above, the edit line and the buttons in a row (or a column for the big
 * input) at the bottom */
void ConsoleWin_Layout(void) {
	exe_rect rc;

	exe_GetClientRect(g_ConHwnd, &rc);
	int w = rc.right, b = rc.bottom;
	if (!w || !b) {
		return;
	}
	exe_MoveWindow(g_ConEdit, 0, 0, w, b, 1);
	int h = g_ConInputH;
	void *last = g_ConBtnGo;
	int lastY, lastH;
	if (h < 0x3c) {
		exe_MoveWindow(g_ConEdit, 0, b - h, w - 0x50, h, 1);
		h = g_ConInputH;
		exe_MoveWindow(g_ConBtnMinus, w - 0x50, b - h, 0x14, h, 1);
		h = g_ConInputH;
		exe_MoveWindow(g_ConBtnPlus, w - 0x3c, b - h, 0x14, h, 1);
		h = g_ConInputH;
		exe_MoveWindow(g_ConBtnBang, w - 0x28, b - h, 0x14, h, 1);
		h = g_ConInputH;
		lastY = b - h;
		lastH = h;
		last = g_ConBtnGo;
	} else {
		int x = w - 0x14;
		exe_MoveWindow(g_ConEdit, 0, b - h, x, h, 1);
		h = g_ConInputH;
		exe_MoveWindow(g_ConBtnMinus, x, b - (int)((uint32_t)h * 4) / 4, 0x14, h / 4, 1);
		h = g_ConInputH;
		exe_MoveWindow(g_ConBtnPlus, x, b - (int)((uint32_t)h * 3) / 4, 0x14, h / 4, 1);
		h = g_ConInputH;
		exe_MoveWindow(g_ConBtnBang, x, b - (int)((uint32_t)h * 2) / 4, 0x14, h / 4, 1);
		h = g_ConInputH / 4;
		lastY = b - h;
		lastH = h;
		last = g_ConBtnGo;
	}
	exe_MoveWindow(last, w - 0x14, lastY, 0x14, lastH, 1);
	exe_MoveWindow(g_ConRich, 0, 0, w, b - g_ConInputH, 1);
}

int ConsoleWin_Close(void) {
	if (!g_ConOpen) {
		return 0;
	}
	void *hwnd = g_ConHwnd;
	g_ConOpen = 0;
	exe_DestroyWindow(hwnd);
	g_ConHwnd = NULL;
	exe_UnregisterClassA(g_ConClass, g_ConInst);
	exe_FreeLibrary(g_ConRichLib);
	return 1;
}

/* the modal input dialog (resource 0xd3) with prompt as its title */
const char *ConsoleWin_Input(const char *prompt) {
	g_ConDlgPrompt = prompt;
	g_ConInput[0] = 0;
	exe_DialogBoxParamA(NULL, (const char *)0xd3, g_ConHwnd, CONWIN_DLGPROC, 0);
	g_ConDlgPrompt = APP_EMPTY;
	return g_ConInput;
}

/* the text at the end of the output in color (CHARFORMAT with CFM_COLOR); the open state */
int ConsoleWin_Print(const char *text, uint32_t color) {
	uint32_t cf[0x3c / 4];

	if (g_ConOpen) {
		memset(cf, 0, sizeof(cf));
		cf[0] = 0x3c;
		cf[1] = 0x40000000;
		cf[5] = color;
		exe_SendMessageA(g_ConRich, EM_SETSEL, (uintptr_t)-1, -1);
		exe_SendMessageA(g_ConRich, EM_SETCHARFORMAT, 1, (intptr_t)cf);
		exe_SendMessageA(g_ConRich, EM_SETSEL, (uintptr_t)-1, -1);
		exe_SendMessageA(g_ConRich, EM_REPLACESEL, 0, (intptr_t)text);
		exe_SendMessageA(g_ConRich, EM_SCROLLCARET, 0, 0);
	}
	return g_ConOpen;
}

void ConsoleWin_ToggleSize(void) {
	if (g_ConOpen) {
		ConsoleWin_ToggleInput();
	}
}

/* switches between the game window and the console window */
void ConsoleWin_Focus(void *hwnd) {
	if (!g_ConOpen) {
		return;
	}
	if (exe_GetForegroundWindow() == hwnd) {
		exe_SetForegroundWindow(g_ConHwnd);
		exe_SetFocus(g_ConEdit);
		return;
	}
	exe_SetForegroundWindow(hwnd);
}

intptr_t __stdcall ConsoleWin_DlgProc(void *hdlg, uint32_t msg, uintptr_t wParam, intptr_t lParam) {
	(void)lParam;
	if (msg == WM_INITDIALOG) {
		exe_SetWindowTextA(hdlg, g_ConDlgPrompt);
		exe_SetWindowPos(hdlg, NULL, 1, 1, 0, 0, 5);
		return 1;
	}
	if (msg != WM_COMMAND) {
		return 0;
	}
	if ((uint16_t)wParam == 1) {
		exe_GetDlgItemTextA(hdlg, 1000, g_ConInput, 0x200);
		exe_EndDialog(hdlg, 0);
	}
	return 1;
}

/* ":prefix" + tab: the variables of the prefix listed and the first completion */
static int ConsoleWin_Complete(void) {
	char line[0x80];

	exe_GetWindowTextA(g_ConEdit, line, 0x80);
	if (line[0] != ':') {
		return 0;
	}
	line[0] = '?';
	g_ConExec(line);
	if (GVar_Complete(line + 1)) {
		Str_PrintfQuiet(line, 0x80, ":%s", GVar_Complete(line + 1));
		exe_SetWindowTextA(g_ConEdit, line);
		exe_SendMessageA(g_ConEdit, EM_SETSEL, (uintptr_t)-2, -2);
	}
	return 1;
}

/* the subclass of the edit line: enter, tab completion and the history keys */
intptr_t __stdcall ConsoleWin_EditProc(void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam) {
	signed char c = (signed char)wParam;

	switch (msg) {
	case WM_KEYDOWN:
		if (c == VK_UP) {
			if (g_ConInputH < 0x3c || exe_GetAsyncKeyState(VK_CONTROL) < 0) {
				ConsoleWin_HistoryOlder();
				return 0;
			}
		} else if (c == VK_DOWN) {
			if (g_ConInputH < 0x3c || exe_GetAsyncKeyState(VK_CONTROL) < 0) {
				ConsoleWin_HistoryNewer();
				return 0;
			}
		}
		break;
	case WM_CHAR:
		if (c == '\t') {
			if (g_ConInputH < 0x3c && g_ConExec && ConsoleWin_Complete()) {
				return 0;
			}
		} else if (c == '\n' || (c == '\r' && g_ConInputH < 0x3c)) {
			ConsoleWin_Enter();
			return 0;
		}
		break;
	case WM_SYSKEYDOWN:
		if (c == '\r') {
			ConsoleWin_ToggleInput();
		} else if (c == VK_UP) {
			ConsoleWin_HistoryOlder();
		} else if (c == VK_DOWN) {
			ConsoleWin_HistoryNewer();
		}
		break;
	}
	return exe_CallWindowProcA(g_ConEditProc, hwnd, msg, wParam, lParam);
}

void *ConsoleWin_GetHwnd(void) {
	return g_ConHwnd;
}

/* opens the console window next to parent (below it with bottom); 0 when open already or without RICHED32.DLL */
int ConsoleWin_Open(void *parent, byte bottom) {
	exe_rect r;

	if (g_ConOpen) {
		return 0;
	}
	g_ConRichLib = exe_LoadLibraryA("RICHED32.DLL");
	if (!g_ConRichLib) {
		exe_MessageBoxA(NULL, "Unable to load the Rich Edit control!", "RETest", 0x30);
		return 0;
	}
	ConsoleWin_RegisterClass((void *)(intptr_t)exe_GetWindowLongA(parent, GWL_HINSTANCE));
	exe_GetWindowRect(parent, &r);
	int sw = exe_GetSystemMetrics(0);
	int sh = exe_GetSystemMetrics(1);
	int x = r.right, y = r.top;
	if ((int)((uint32_t)r.right + CONWIN_W) > sw) {
		x = sw - CONWIN_W;
		r.right = x;
	}
	/* the original corrects x here too */
	if ((int)((uint32_t)r.top + CONWIN_H) > sh) {
		x = sh - CONWIN_H;
		r.right = x;
	}
	if (bottom) {
		x = r.left;
		y = r.bottom;
	}
	return ConsoleWin_Create((void *)(intptr_t)exe_GetWindowLongA(parent, GWL_HINSTANCE), x, y, 1, parent) != 0;
}

int ConsoleWin_Create(void *inst, int x, int y, int nCmdShow, void *parent) {
	g_ConInst = inst;
	void *hwnd = exe_CreateWindowExA(0, g_ConClass, g_ConTitle, 0xcf0000, x, y, CONWIN_W, CONWIN_H, parent, NULL,
		inst, NULL);
	g_ConParent = parent;
	g_ConHwnd = hwnd;
	if (!hwnd) {
		return 0;
	}
	exe_ShowWindow(hwnd, nCmdShow);
	exe_UpdateWindow(g_ConHwnd);
	for (int i = 0; i < CONWIN_HISTORY; i++) {
		g_ConHistory[i][0] = 0;
	}
	g_ConHistPos = 0;
	g_ConOpen = 1;
	return 1;
}

int ConsoleWin_RegisterClass(void *inst) {
	exe_wndclass wc;

	wc.style = 0xb;
	wc.wndProc = CONWIN_WNDPROC;
	wc.clsExtra = 0;
	wc.wndExtra = 0;
	wc.hInstance = inst;
	wc.hIcon = exe_LoadIconA(NULL, (const char *)0x7f00);
	wc.hCursor = exe_LoadCursorA(NULL, (const char *)0x7f00);
	wc.hbrBackground = NULL;
	wc.menuName = NULL;
	wc.className = g_ConClass;
	return exe_RegisterClassA(&wc);
}

static void ConsoleWin_ShowHistory(void) {
	exe_SetWindowTextA(g_ConEdit, g_ConHistory[g_ConHistPos]);
	exe_SendMessageA(g_ConEdit, EM_SETSEL, 0, -2);
}

void ConsoleWin_HistoryNewer(void) {
	if (g_ConHistPos > 0) {
		g_ConHistPos--;
		ConsoleWin_ShowHistory();
	}
}

void ConsoleWin_HistoryOlder(void) {
	if (g_ConHistPos < CONWIN_HISTORY - 1) {
		g_ConHistPos++;
		ConsoleWin_ShowHistory();
	}
}

/* EDITSTREAM callback of EM_STREAMOUT: the RTF into the file */
uint32_t __stdcall ConsoleWin_StreamOut(void *file, const void *buf, int32_t n, int32_t *written) {
	if (!n) {
		return 1;
	}
	*written = 0;
	exe_WriteFile(file, buf, (uint32_t)n, (uint32_t *)written, NULL);
	return 0;
}

/* "Save output...": the output as an RTF file */
int ConsoleWin_SaveOutput(void) {
	/* OPENFILENAMEA (0x58 bytes) as words */
	uint32_t ofn[0x58 / 4];
	char name[0x100];
	struct {
		void *cookie;
		uint32_t error;
		void *callback;
	} es;

	memset(ofn, 0, sizeof(ofn));
	memcpy(name, "*.rtf", 6);
	ofn[0] = 0x58;
	ofn[1] = (uint32_t)(uintptr_t)g_ConHwnd;
	ofn[3] = (uint32_t)(uintptr_t)GAME_STR(str_3d2114, 0x3d2114);	/* "RTF File" and its pattern */
	ofn[7] = (uint32_t)(uintptr_t)name;
	ofn[8] = 0x100;
	ofn[12] = (uint32_t)(uintptr_t) "Save output as RTF File";
	ofn[13] = 2;
	if (!exe_GetSaveFileNameA(ofn)) {
		return 0;
	}
	void *f = exe_CreateFileA(name, 0xc0000000, 0, NULL, 2, 0x80, NULL);
	if (f == (void *)-1) {
		return 0;
	}
	es.cookie = f;
	es.error = 0;
	es.callback = CONWIN_STREAMOUT;
	exe_SendMessageA(g_ConRich, EM_STREAMOUT, 2, (intptr_t)&es);
	exe_CloseHandle(f);
	return 1;
}

int ConsoleWin_Show(byte show) {
	void *hwnd = g_ConHwnd;
	if (!hwnd) {
		return 0;
	}
	return exe_ShowWindow(hwnd, show ? 8 : 0);
}

/* the edit line into the history (the older lines one down) and to Console_Execute */
void ConsoleWin_Enter(void) {
	/* differs from the original on purpose (no blocking waits): the original handled no message while it waited for
	 * the GOG sign-in or a video, so the line runs after the wait (App_WinMain) */
	if (App_Waiting()) {
		app_conEnterLater = 1;
		return;
	}
	for (int i = CONWIN_HISTORY - 2; i >= 0; i--) {
		const char *src = g_ConHistory[i];
		char *dst = g_ConHistory[i + 1];
		size_t n = 0;
		while (n + 1 < CONWIN_LINE && src[n]) {
			dst[n] = src[n];
			n++;
		}
		dst[n] = 0;
	}
	exe_GetWindowTextA(g_ConEdit, g_ConHistory[0], CONWIN_LINE);
	exe_SetWindowTextA(g_ConEdit, "");
	g_ConHistPos = -1;
	if (g_ConExec) {
		g_ConExec(g_ConHistory[0]);
	}
}

/* the input line between 20 and 200 pixels */
void ConsoleWin_ToggleInput(void) {
	g_ConInputH = g_ConInputH == 0x14 ? 200 : 0x14;
	ConsoleWin_Layout();
}

intptr_t __stdcall ConsoleWin_WndProc(void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam) {
	/* LOGFONTA (0x3c bytes) */
	struct {
		int32_t height, width, escapement, orientation, weight;
		byte italic, underline, strikeOut, charSet, outPrecision, clipPrecision, quality, pitchAndFamily;
		char faceName[32];
	} lf;

	switch (msg) {
	case WM_CREATE: {
		void *inst = ((void *const *)lParam)[1];
		void *rich = exe_CreateWindowExA(0, "richedit", NULL, 0x50b008c4, 0, 0, 0, 0, hwnd, NULL, inst, NULL);
		g_ConRich = rich;
		exe_SendMessageA(rich, EM_LIMITTEXT, 0, 0);
		g_ConEdit = exe_CreateWindowExA(0, "edit", NULL, 0x508000c4, 0, 0, 0, 0, hwnd, NULL, inst, NULL);
		g_ConBtnBang = exe_CreateWindowExA(0, "button", "!", 0x50010000, 0, 0, 0, 0, hwnd, (void *)ID_BTN_SIZE, inst,
			NULL);
		g_ConBtnPlus = exe_CreateWindowExA(0, "button", "+", 0x50010000, 0, 0, 0, 0, hwnd, (void *)ID_BTN_NEWER, inst,
			NULL);
		g_ConBtnMinus = exe_CreateWindowExA(0, "button", "-", 0x50010000, 0, 0, 0, 0, hwnd, (void *)ID_BTN_OLDER, inst,
			NULL);
		g_ConBtnGo = exe_CreateWindowExA(0, "button", ">", 0x50010000, 0, 0, 0, 0, hwnd, (void *)ID_BTN_ENTER, inst,
			NULL);
		g_ConEditProc = (void *)(intptr_t)exe_GetWindowLongA(g_ConEdit, GWL_WNDPROC);
		exe_SetWindowLongA(g_ConEdit, GWL_WNDPROC, (int32_t)(intptr_t)CONWIN_EDITPROC);
		memset(&lf, 0, sizeof(lf));
		memcpy(lf.faceName, "Terminal", 9);
		lf.charSet = 1;
		lf.height = 0xc;
		void *font = exe_CreateFontIndirectA(&lf);
		g_ConFont = font;
		exe_SendMessageA(g_ConEdit, WM_SETFONT, (uintptr_t)font, 1);
		exe_SendMessageA(g_ConRich, WM_SETFONT, (uintptr_t)g_ConFont, 1);
		exe_SendMessageA(g_ConRich, EM_SETBKGNDCOLOR, 0, 0xc0c0c0);
		exe_SendMessageA(g_ConRich, EM_SETEVENTMASK, 0, 0x200000);
		exe_SetFocus(g_ConEdit);
		void *menu = exe_GetSystemMenu(hwnd, 0);
		exe_AppendMenuA(menu, 0x800, 0, "");
		exe_AppendMenuA(menu, 0, ID_SAVEOUTPUT, "&Save output...");
		return 0;
	}
	case WM_DESTROY:
		return exe_DefWindowProcA(hwnd, WM_DESTROY, wParam, lParam);
	case WM_SIZE:
		ConsoleWin_Layout();
		return 0;
	case WM_CLOSE:
		if (!g_ConOpen) {
			return exe_DefWindowProcA(hwnd, WM_CLOSE, wParam, lParam);
		}
		exe_SetWindowTextA(g_ConEdit, "import Scrap;Scrap.Exit()");
		ConsoleWin_Enter();
		return 0;
	case WM_COMMAND:
		switch ((uint16_t)wParam) {
		case ID_BTN_NEWER:
			ConsoleWin_HistoryNewer();
			break;
		case ID_BTN_OLDER:
			ConsoleWin_HistoryOlder();
			break;
		case ID_BTN_SIZE:
			ConsoleWin_ToggleInput();
			break;
		case ID_BTN_ENTER:
			ConsoleWin_Enter();
			break;
		case 0x9c46:
		case 0x9c54:
		case 0x9c56:
		case 0x9ca7:
		case 0x9ca8:
			exe_SendMessageA(g_ConParent, WM_COMMAND, wParam, lParam);
			break;
		}
		return 0;
	case WM_SYSCOMMAND:
		if ((uint16_t)wParam == ID_SAVEOUTPUT) {
			ConsoleWin_SaveOutput();
			return 0;
		}
		if ((uint16_t)wParam == SC_CLOSE && (uint32_t)lParam >> 16) {
			ConsoleWin_Close();
			return 0;
		}
		return exe_DefWindowProcA(hwnd, WM_SYSCOMMAND, wParam, lParam);
	}
	return exe_DefWindowProcA(hwnd, msg, wParam, lParam);
}

/* the message of the test version of 2004 and the exit after November 2004 (not called) */
void App_CheckExpiry(void) {
	uint16_t st[8];

	exe_GetLocalTime(st);
	if ((uint32_t)st[0] * 100 + st[1] < 200412) {
		exe_MessageBoxA(NULL, "This test version will expire Nov.30.2004", "Message", 0x2040);
		return;
	}
	exe_MessageBoxA(NULL, "This test version has expired.", "Message", 0x2030);
	crt_exit(-1);
}
