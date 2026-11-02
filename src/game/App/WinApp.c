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
#include "scrap/platform.h"
#include "scrap/win32.h"

CWinApp *CWinApp_ctor(CWinApp *self, void *hInst, void *hPrev, const char *cmdLine, int nCmdShow) {
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

void CWinApp_dtor(CWinApp *self) {
	self->base.vftable = g_CWinAppVftable;
	CApp_dtor(&self->base);
}

/* vf03 */
CWinApp *CWinApp_Delete(CWinApp *self, byte flags) {
	CWinApp_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CWinApp));
	}
	return self;
}

/* vf17: the console window is gone */
int CWinApp_ConsoleAdd(CWinApp *self, const char *text, uint32_t color) {
	(void)self, (void)text, (void)color;
	return 0;
}

/* vf16: the console window is gone */
int CWinApp_ShowConsole(CWinApp *self, byte show) {
	(void)self, (void)show;
	return 0;
}

/* the main window through SDL: 640 x 480 (-widewindow 640 x 360), -dedicated 350 x 1 and not resizable */
byte CWinApp_CreateMainWindow(CWinApp *self) {
	byte wide = (byte)CWinApp_HasArg(self, "widewindow");
	self->dedicatedWin = (byte)CWinApp_HasArg(self, "dedicated");
	int w = wide ? g_WideWinWidth : g_WinWidth, h = wide ? g_WideWinHeight : g_WinHeight;
	if (self->dedicatedWin) {
		w = 0x15e;
		h = 1;
	}
	if (!Plat_Init() || !Plat_CreateWindow(g_ScrapTitle, w, h, self->dedicatedWin)) {
		return 0;
	}
	self->hwnd = Plat_NativeWindow();
	return 1;
}

/* vf18: the original asked in a modal dialog of the console window, which is gone: no text (None for Python) */
const char *CWinApp_DebugInput(CWinApp *self, const char *prompt) {
	(void)self, (void)prompt;
	return NULL;
}

/* vf08: no textures and no sound for -dedicated */
void CWinApp_InitDedicated(CWinApp *self) {
	(void)self;
	g_LoadTextures = 0;
	g_SoundInit = 0;
}

/* vf06 */
void CWinApp_Shutdown(CWinApp *self) {
	CApp_Shutdown(&self->base);
	Plat_Shutdown();
	App_SetExceptionFilter(0);
}

/* vf09 */
void CWinApp_Exit(CWinApp *self) {
	(void)self;
	Plat_Quit();
}

/* vf13 */
void CWinApp_ResetParams(CWinApp *self) {
	self->console = 0;
	CApp_ResetParams(&self->base);
}

/* "-name" in the command line (case-insensitive), followed by ':', ' ' or the end */
int CWinApp_HasArg(CWinApp *self, const char *name) {
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
const char *App_RegGetString(CWinApp *self, const char *name) {
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
double CWinApp_GetTime(CWinApp *self) {
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
	(void)self;
	Plat_ShowWindow();
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
byte CWinApp_Init(CWinApp *self) {
	App_SetExceptionFilter(1);
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
	if (self->console || self->dedicatedWin) {
		Plat_ShowWindow();
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
byte CWinApp_Frame(CWinApp *self) {
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
byte CWinApp_ParseParam(CWinApp *self, const char *param, const char *ext) {
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
byte CWinApp_RunApp(CWinApp *self, const char *exe, const char *cmdLine, int arg) {
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

/* vf20 */
byte CWinApp_IsDedicatedWin(CWinApp *self) {
	return self->dedicatedWin;
}

/* the init, a video or the rest of Engine_LevelEnd waits (the original blocked in them) */
static byte App_Waiting(void) {
	return app_init != APPINIT_DONE || Video_Busy() || Engine_VideoWait();
}

/*
 * The main loop: the events, then a frame (or one pass of what the original waited for). Differs from the original on
 * purpose (no blocking waits): while the app waits, the keyboard and mouse input stays queued (the original handled no
 * message while it waited, so the input came after the wait), the window events are handled, so the window answers
 * and can be moved or closed; closed while it waits, the quit comes after the wait.
 */
int __stdcall App_WinMain(void *hInst, void *hPrev, char *cmdLine, int nCmdShow) {
	byte quitLater = 0;

	CWinApp *app = operator_new(sizeof(CWinApp));
	g_pApp = app ? &CWinApp_ctor(app, hInst, hPrev, cmdLine, nCmdShow)->base : NULL;
	if (!g_pApp->vftable->Init(g_pApp)) {
		return 0;
	}
	for (;;) {
		byte waiting = App_Waiting();
		if (Plat_PumpEvents(waiting)) {
			if (!waiting) {
				break;
			}
			quitLater = 1;
		}
		if (waiting) {
			/* one pass of what the original waited for: the video, the rest of the init or of the level load */
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
	}
	g_pApp->vftable->Shutdown(g_pApp);
	if (g_pApp) {
		g_pApp->vftable->Delete(g_pApp, 1);
	}
	g_pApp = NULL;
	return 0;
}
