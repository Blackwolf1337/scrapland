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
#include "scrap/instance.h"
#include "scrap/mathlib.h"
#include "scrap/scrapmain.h"
#include "scrap/video.h"
#include "scrap/platform.h"
#include "scrap/platform.h"
#include "scrap/staticinit.h"
#include "scrap/testdet.h"

#define g_CWinAppVftable (&CWinApp_vftable)
#define g_ScrapTitle str_Scrapland	/* "Scrapland" */
static char str_Scrapland[];
static const CAppVtbl CWinApp_vftable;

/* WinApp.c */
static int CWinApp_HasArg(CWinApp *self, const char *name);
static CWinApp *CWinApp_ctor(CWinApp *self, void *hInst, void *hPrev, const char *cmdLine, int nCmdShow) {
	char path[0x104];

	CApp_ctor(&self->base, cmdLine);
	self->base.vftable = g_CWinAppVftable;
	self->hInstance = hInst;
	self->hPrevInstance = hPrev;
	self->nCmdShow = nCmdShow;
	self->dedicatedWin = 0;
	Plat_ExePath(path, sizeof(path));
	Dir_Change(path, "../");
	if ((byte)CWinApp_HasArg(self, "txf_helper")) {
		cString_Set((cString *)&g_LogName, "logtxf.html");
	}
	g_Instance = (byte)CWinApp_HasArg(self, "instance");
	return self;
}

static void CWinApp_dtor(CWinApp *self) {
	CApp_dtor(&self->base);
}

static void CWinApp_Free(CApp *base) {
	CWinApp *self = (CWinApp *)base;
	CWinApp_dtor(self);
	operator_delete(self);
}

/* the console window is gone */
static void CWinApp_ConsoleAdd(CApp *base, const char *text, uint32_t color) {
	CWinApp *self = (CWinApp *)base;
	(void)self, (void)text, (void)color;
}

/* the console window is gone */
static void CWinApp_ShowConsole(CApp *base, byte show) {
	CWinApp *self = (CWinApp *)base;
	(void)self, (void)show;
}

static int g_WinWidth = 640;
static int g_WinHeight = 480;
static int g_WideWinWidth = 640;
static int g_WideWinHeight = 360;

/* the main window through SDL: 640 x 480 (-widewindow 640 x 360), -dedicated 350 x 1 and not resizable */
static byte CWinApp_CreateMainWindow(CWinApp *self) {
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

/* the original asked in a modal dialog of the console window, which is gone: no text (None for Python) */
static const char *CWinApp_DebugInput(CApp *base, const char *prompt) {
	CWinApp *self = (CWinApp *)base;
	(void)self, (void)prompt;
	return NULL;
}

/* no textures and no sound for -dedicated */
static void CWinApp_InitDedicated(CApp *base) {
	CWinApp *self = (CWinApp *)base;
	(void)self;
	g_LoadTextures = 0;
	g_SoundInit = 0;
}

static void CWinApp_Shutdown(CApp *elem) {
	CWinApp *self = (CWinApp *)elem;
	CApp_Shutdown(&self->base);
	Plat_Shutdown();
	App_SetExceptionFilter(0);
}

static void CWinApp_Exit(CApp *base) {
	CWinApp *self = (CWinApp *)base;
	(void)self;
	Plat_Quit();
}

static void CWinApp_ResetParams(CApp *elem) {
	CWinApp *self = (CWinApp *)elem;
	self->console = 0;
	CApp_ResetParams(&self->base);
}

/* "-name" in the command line (case-insensitive), followed by ':', ' ' or the end */
static int CWinApp_HasArg(CWinApp *self, const char *name) {
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
	char exe[0x400];
	Plat_ExePath(exe, sizeof(exe));
	crt_gamepath(exe, buf, size);
}

void *App_GetHInstance(void) {
	return g_pWinApp->hInstance;
}

/* WinMain.cpp state */
static uint32_t g_TimeQPPause;
static uint32_t g_TimeMsPause;
static byte g_TimeInit;
static int g_TimeUseQP;
static int64_t g_TimeQPFreq;
static int64_t g_TimeQPStart;
static uint32_t g_TimeMsStart;

/* seconds since the first call, by the performance counter if there is one, else by timeGetTime */
static double CWinApp_GetTime(CApp *base) {
	CWinApp *self = (CWinApp *)base;
	(void)self;
	if (!g_TimeInit) {
		g_TimeInit = 1;
		g_TimeUseQP = (byte)Timer_GetQPFreq64(&g_TimeQPFreq);
		Timer_GetFreq64(&g_TimeQPStart);
		g_TimeMsStart = Plat_Ticks();
	}
	if (g_Instance) {
		return Instance_Time();
	}
	if (g_TimeUseQP) {
		int64_t now;
		Timer_GetFreq64(&now);
		uint64_t c = (uint64_t)now - g_TimeQPPause - (uint64_t)g_TimeQPStart;
		double t = Int64_ToDouble((int64_t)c);
		return t / Int64_ToDouble(g_TimeQPFreq);
	}
	uint32_t ms = Plat_Ticks() - g_TimeMsPause - g_TimeMsStart;
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

/* the window, the language, then CApp_Init, which may leave its videos to the message loop
 * (CWinApp_InitStep) */
static byte CWinApp_Init(CApp *elem) {
	CWinApp *self = (CWinApp *)elem;
	App_SetExceptionFilter(1);
	if (g_Instance) {
		if (!Plat_InitHeadless()) {
			CheckFailed("Plat_InitHeadless()", __FILE__, __TIMESTAMP__, __LINE__);
		}
	} else if (!CWinApp_CreateMainWindow(self)) {
		CheckFailed("CreateMainWindow()", __FILE__, __TIMESTAMP__, __LINE__);
	}
	/* differs from the original on purpose: without the registry LANGUAGE it is english (the original took the one of
	 * GOG later, english in a normal install), not the "French" default of SelectedLanguage */
	const char *lang = Plat_InstallLanguage();
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

/* CApp_Frame (the original ran the GOG and Steam callbacks first) */
static byte CWinApp_Frame(CApp *elem) {
	CWinApp *self = (CWinApp *)elem;
	/* differs from the original on purpose (no sleeps): no Sleep(250) while the app is in the background; CApp_Frame
	 * then only takes the time and returns 0, the PeekMessage loop of App_WinMain calls it again */
	return CApp_Frame(&self->base);
}

/* the console output besides the log: stdout (the console window of the original is gone) */
void ConsoleWin_Add(const char *text, uint32_t color) {
	(void)color;
	fputs(text, stdout);
	fflush(stdout);
}

/* the folder of the configuration (Plat_ConfigDir), "" with the dedicated window and in the headless instance;
 * differs from the original on purpose (no store features): no subfolder of the GOG id */
void App_GetConfigDir(char *buf, int size) {
	if (App_IsDedicatedWin(g_pApp) || g_Instance) {
		*buf = 0;
		return;
	}
	Plat_ConfigDir(buf, size);
}

/* -console, -inifile:name, else CApp_ParseParam */
static byte CWinApp_ParseParam(CApp *elem, const char *param, const char *ext) {
	CWinApp *self = (CWinApp *)elem;
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

/* starts exe (from the game directory) with the command line, without waiting */
static byte CWinApp_RunApp(CApp *base, const char *exe, const char *cmdLine, int arg) {
	CWinApp *self = (CWinApp *)base;
	char path[0x104], cmd[0x208], args[0x208];

	(void)self;
	(void)arg;
	Plat_ExePath(path, sizeof(path));
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

static byte CWinApp_IsDedicatedWin(CApp *base) {
	CWinApp *self = (CWinApp *)base;
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
	if (!App_Init(g_pApp)) {
		return 0;
	}
	if (TestDet_On()) {
		Con_Printf("Deterministic test mode on: %d steps a second\n", TestDet_Fps());
		/* the level of the start is loaded */
		TestDet_Seed(1);
	}
	for (;;) {
		byte waiting = App_Waiting();
		/* the deterministic test mode: one step of its clock per pass; none in the pass that loads a level (the load
		 * comes first in the frame, whether a command file waits or not) and none while the test channel holds the
		 * game */
		if (TestDet_On() && (waiting || !(g_pApp->loadPending || TestChannel_Hold()))) {
			TestDet_Tick();
		}
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
		if (g_Instance) {
			Instance_Pass();
		} else if (!g_Engine || Engine_FrameDue(g_Engine)) {
			App_Frame(g_pApp);
		}
	}
	App_Shutdown(g_pApp);
	if (g_pApp) {
		App_Free(g_pApp);
	}
	g_pApp = NULL;
	return 0;
}

/* the vtable of CWinApp */
static const CAppVtbl CWinApp_vftable = {
	.DoneGame = CApp_DoneGame,
	.InitGame = CApp_InitGame,
	.InitDebug = CApp_Nop,
	.Free = CWinApp_Free,
	.Init = CWinApp_Init,
	.Frame = CWinApp_Frame,
	.Shutdown = CWinApp_Shutdown,
	.LoadLevelNow = CApp_LoadLevelNow,
	.InitDedicated = CWinApp_InitDedicated,
	.Exit = CWinApp_Exit,
	.Update = CApp_Update,
	.Render = CApp_Render,
	.ParseParam = CWinApp_ParseParam,
	.ResetParams = CWinApp_ResetParams,
	.GetTime = CWinApp_GetTime,
	.GetGameTime = CApp_GetGameTime,
	.ShowConsole = CWinApp_ShowConsole,
	.ConsoleAdd = CWinApp_ConsoleAdd,
	.DebugInput = CWinApp_DebugInput,
	.RunApp = CWinApp_RunApp,
	.IsDedicatedWin = CWinApp_IsDedicatedWin,
};

static char str_Scrapland[] = "Scrapland";
cGVar GVar_SoundInit;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_SoundInit, &GVar_SoundInit, "SoundInit", 0, "Initialize sound system", 1)
