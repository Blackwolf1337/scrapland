/*
 * App.cpp of the game: CApp, the game part of the application (see include/scrap/app.h). Its vtable slots are the
 * vf numbers of the exe; the C calls them through the vtable where the exe does.
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/ai.h"
#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/filepak.h"
#include "scrap/filesys.h"
#include "scrap/input.h"
#include "scrap/model.h"
#include "scrap/net.h"
#include "scrap/python.h"
#include "scrap/scrapfont.h"
#include "scrap/scrapmain.h"
#include "scrap/sound.h"
#include "scrap/platform.h"
#include "scrap/world.h"
#include "scrap/scoreritems.h"

#define APP_EMPTY GAME_STR(str_Empty)	/* the one "" the code compares by address */

/* the engine callbacks of the exe (Engine_SetCallback) */
#define APP_CB_WORLDFRAME ((void (*)(void))(void *)Scorer_Render)
#define APP_CB_LOADINGXBOX ((void (*)(void))(void *)Loading_DrawXbox)
#define APP_CB_LOADING ((void (*)(void))(void *)Loading_Draw)

#define PSAIMAGE_SIZE sizeof(CPsaImage)

static const vec3_t app_one = { 1.0f, 1.0f, 1.0f };

/* delete of an object through its scalar deleting destructor (slot 0) and the pointer cleared */
static void App_DeleteObj(void **pp) {
	void *p = *pp;
	if (p) {
		(*(void(**)(void *, int)) * (void ***)p)(p, 1);
		*pp = NULL;
	}
}

/* strcpy of the original, cut at size */
static void App_CopyStr(char *dst, const char *src, size_t size) {
	size_t n = 0;
	while (n + 1 < size && src[n]) {
		dst[n] = src[n];
		n++;
	}
	dst[n] = 0;
}

CApp *CApp_ctor(CApp *self, const char *cmdLine) {
	self->vftable = g_CAppVftable;
	cString_ctor(&self->levelPath);
	cString_ctor(&self->newLevelPath);
	crt_srand((unsigned int)crt_time32(NULL));
	GVars_Init();
	size_t n = strlen(cmdLine) + 1;
	char *p = Mem_Alloc(n);
	self->cmdLine = p;
	memcpy(p, cmdLine, n);
	self->active = 1;
	self->time = 0.0;
	self->lastFrame = 0.0;
	self->flags = 0;
	self->gameTime = 0.0;
	self->loadPending = 1;
	self->onLevelLoaded = NULL;
	g_NodesWorld = NULL;
	g_NodesWorld2D = NULL;
	g_AStar = NULL;
	g_AStar2D = NULL;
	self->dedicated = 0;
	static int block[4];
	g_AppBlock = block;
	g_AppBlock[3] = 0x16;
	g_DrawNextFrame = 1;
	Net_Init();
	return self;
}

void CApp_dtor(CApp *self) {
	self->vftable = g_CAppVftable;
	Net_Done();
	Mem_Free(self->cmdLine);
	Python_Shutdown();
	SaveVars_Delete("");
	Lang_Free();
	GVars_Done();
	cString_dtor(&self->newLevelPath);
	cString_dtor(&self->levelPath);
}

/* vf03 */
CApp *CApp_Delete(CApp *self, byte flags) {
	CApp_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, 0x48);
	}
	return self;
}

/* vf02, vf08, vf09: empty */
void CApp_Nop(CApp *self) {
	(void)self;
}

/* vf00: the world and the subsystems of the level closed */
void CApp_DoneGame(CApp *self) {
	cGVar *port = (&g_GVarMasterServerPort), *addr = (&g_GVarMasterServerAddress);

	(void)self;
	InputMaps_Clear();
	Net_DoneBrowser();
	if (!(port->base.flags & 1)) {
		port->value.i = port->def.i;
	}
	if (!(addr->base.flags & 1)) {
		cString_Set(&addr->value.s, addr->def.s);
	}
	cWorld_Clear(g_pWorld);
	Con_Print(0, "World system clean.\n", 0);
	Python_Close();
	Con_Print(0, "Python script subsystem closed.\n", 0);
	App_DeleteObj((void **)&g_NodesWorld);
	Con_Print(0, "NodesWorld closed.\n", 0);
	App_DeleteObj((void **)&g_NodesWorld2D);
	Con_Print(0, "NodesWorld2D closed.\n", 0);
	App_DeleteObj((void **)&g_NodesTraffic);
	Con_Print(0, "NodesTraffic closed.\n", 0);
	App_DeleteObj((void **)&g_NodesTrafficInDoor);
	Con_Print(0, "NodesTrafficInDoor closed.\n", 0);
	App_DeleteObj((void **)&g_AStar);
	Con_Print(0, "AStar closed.\n", 0);
	App_DeleteObj((void **)&g_AStar2D);
	Con_Print(0, "AStar2D closed.\n", 0);
	App_DeleteObj((void **)&g_VehicleGroup);
	Con_Print(0, "VehicleGroup closed.\n", 0);
	App_DeleteObj((void **)&g_CharGroup);
	Con_Print(0, "CharGroup closed.\n", 0);
	Scorer_Done();
	Con_Print(0, "Scorer closed.\n", 0);
	Input_ResetActions();
	App_DeleteObj((void **)&g_pWorld);
	Con_Print(0, "World closed.\n", 0);
	ActClassList_Done();
	Con_Print(0, "Action class list closed.\n", 0);
}

/* the lines of the console window: "/f a" QuickConsole.f(a), ":var value" sets, ":var" shows, "?filter" lists */
void Console_Execute(const char *cmd) {
	/* the characters after ':var ' that make the value a Python expression ('"' to '9') */
	static const byte numeric[0x18] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
	char code[0x400], name[0x400];
	char *val;

	switch (cmd[0]) {
	case '/':
		Str_Copy(name, cmd + 1, 0x400);
		val = strchr(name, ' ');
		if (val) {
			*val++ = 0;
			Str_TrimBlanks(val);
			Str_TrimBlanks(name);
			Str_PrintfQuiet(code, 0x400, "import QuickConsole;QuickConsole.%s(%s)", name, val);
		} else {
			Str_PrintfQuiet(code, 0x400, "import QuickConsole;QuickConsole.%s()", name);
		}
		break;
	case ':':
		Str_Copy(name, cmd + 1, 0x400);
		val = strchr(name, ' ');
		if (val) {
			*val++ = 0;
			Str_TrimBlanks(val);
			Str_TrimBlanks(name);
			uint32_t c = (uint32_t)((int)(signed char)*val - '"');
			if (c < sizeof(numeric) && numeric[c]) {
				Str_PrintfQuiet(code, 0x400, "if Scrap.Set('%s',%s):\n print '%s = %s'", name, val, name, val);
			} else {
				Str_PrintfQuiet(code, 0x400, "if Scrap.Set('%s','%s'):\n print '%s = %s'", name, val, name, val);
			}
		} else {
			Str_TrimBlanks(name);
			Str_PrintfQuiet(code, 0x400,
				"Scrap.Print('%s = '+`Scrap.Get('%s')`+' (default = '+`Scrap.Def('%s')`+') ### '+Scrap.Des('%s')+'\\n')",
				name, name, name, name);
		}
		break;
	case '?':
		Str_PrintfQuiet(name, 0x400, "Scrap.ShowGVars(\"%s\")", cmd + 1);
		PyRun_ScrapString(name);
		return;
	default:
		PyRun_ScrapString(cmd);
		return;
	}
	PyRun_ScrapString(code);
}

/* rva 0x20acd0, no callers: the block of the constructor still intact */
byte App_CheckBlock(void) {
	return g_AppBlock[3] == 0x16;
}

/* vf11 (CWinApp vf11 jumps here too) */
void CApp_Render(CApp *self) {
	(void)self;
	Engine_Render();
}

/* vf06 */
void CApp_Shutdown(CApp *self) {
	char path[0x80];

	if (Net_IsServer()) {
		Net_CloseServer("");
	}
	if (Net_IsClient()) {
		Net_CloseClientMsg("");
	}
	self->vftable->DoneGame(self);
	Sound_ResetVolumes();
	Sound_Done();
	Engine_Close();
	Input_CloseDevices();
	InputMaps_Done();
	Con_Print(0, "Input devices closed.\n", 0);
	Pack_CloseAll();
	Config_Save();
	DebugLog_CloseAll();
	Con_Print(0, "Debug subsystem closed.\n", 0);
	if (*g_ReRunAtExitParams && **g_ReRunAtExitParams) {
		App_GetPath(path, 0x80);
		self->vftable->RunApp(self, path, *g_ReRunAtExitParams, 0);
	}
}

static void Loading_Stop(void) {
	Engine_SetCallback(NULL, 4);
	void *img = g_XLoadIcon;
	if (img) {
		CPsaImage_dtor(img);
		operator_delete_sized(img, PSAIMAGE_SIZE);
	}
	g_XLoadIcon = NULL;
	g_XLoadOn = 0;
}

void Loading_End(void) {
	if (g_XLoadOn) {
		Loading_Stop();
	}
}

/* vf13 */
void CApp_ResetParams(CApp *self) {
	self->build = 0;
}

/* vf15 */
double CApp_GetGameTime(CApp *self) {
	return self->gameTime;
}

void *App_GetObjData(const char *name) {
	CHECK(g_pWorld);
	return cWorld_GetObjData(g_pWorld, name);
}

float World_TimeScale(void) {
	if (g_pWorld) {
		return cTimer_GetSpeed(&g_pWorld->timer);
	}
	return 1.0f;
}

/* the start time of the level load of CApp_Init, for CApp_InitEnd */
static double app_initT0;

/* the rest of CApp_Init after the videos of Engine_Init: the sound and the first level */
static byte CApp_InitEnd(CApp *self) {
	Engine_SetCallback(APP_CB_WORLDFRAME, 1);
	if (g_IsXbox) {
		Engine_SetCallback(APP_CB_LOADINGXBOX, 3);
	}
	Sound_Init();
	Sound_ResetVolumes();
	self->vftable->InitGame(self, *g_iMap);
	Loading_Fade();
	double t1 = self->vftable->GetTime(self);
	Con_Printf("Level loaded in %.2f seconds\n", t1 - app_initT0);
	return 1;
}

/* vf04: the debug logs, Scrap.cfg, the command line, the packs, the input, the engine and the first level */
byte CApp_Init(CApp *self) {
	vec3_t one;

	DebugLog_Init();
	self->vftable->InitDebug(self);
	Config_Load("Scrap.cfg");
	cString_Set(g_ReRunAtExitParams, "");
	self->vftable->ResetParams(self);
	CApp_ParseCmdLine(self);
	Con_Print(0, "Debug subsystem initialized.\n", 0);
	if (self->build) {
		Pack_Build(".", "Data", self->build);
	}
	if (Pack_Open("Data", "", 0x4000)) {
		Pack_Open("Data00", "", 0x4000);
		Pack_Open("Data01", "", 0x4000);
		Pack_Open("Data02", "", 0x4000);
		Pack_Open("Data03", "", 0x4000);
		cString lang;
		cString_ctorStr(&lang, *g_LangName);
		if (!Pack_Open(lang, "", 0x400) && !Pack_Open(g_LangDefault, "", 0x400)) {
			Pack_Open("Voices", "", 0x400);
		}
		Pack_Open("Pack1", "", 0x400);
		if (g_ModFileName && *g_ModFileName) {
			Pack_Open(g_ModFileName, "", 0x4000);
		}
		Pack_OpenDir("mods");
		if (g_ModPathName && *g_ModPathName) {
			Pack_OpenDir(g_ModPathName);
		}
		cString_dtor(&lang);
	}
	app_initT0 = self->vftable->GetTime(self);
	Lang_Set(NULL, 1);
	self->flags = (self->dedicated ? APPF_DEDICATED : 0) | (self->flags & ~APPF_DEDICATED);
	self->vftable->ShowConsole(self, (byte)Engine_IsFullScreen() == 0);
	InputMaps_Init();
	Input_OpenDevices();
	Con_Print(0, "Input devices initialized.\n", 0);
	one = app_one;
	Engine_SetGround(*g_iMap, "Map/Map3D.m3d", &one);
	Engine_Init();
	/* differs from the original on purpose (no blocking waits): while the videos of Engine_Init play from the message
	 * loop, the rest is CApp_InitResume */
	if (Engine_VideoWait()) {
		return 1;
	}
	return CApp_InitEnd(self);
}

byte CApp_InitResume(CApp *self) {
	if (!Engine_VideoResume()) {
		return 0;
	}
	CApp_InitEnd(self);
	return 1;
}

/* vf01: the level path, the network flags, the world and Python */
void CApp_InitGame(CApp *self, const char *level) {
	cString_Set(&self->levelPath, level);
	if (cString_CmpStr(&self->newLevelPath, level)) {
		cString_Set(&self->newLevelPath, level);
	}
	self->loadPending = 0;
	self->flags = self->dedicated ? APPF_DEDICATED : 0;
	self->flags |= Net_IsServer() ? APPF_SERVER : 0;
	self->flags |= Net_IsClient() ? APPF_CLIENT : 0;
	CApp_SFLB_0(self);
	g_AppPyOne = Py_BuildValue("i", 1);
	g_DisableSkipSlot = g_DisableSkipSlotDefault;
	Python_Init();
	Con_Print(0, "Python script subsystem initialized.\n", 0);
}

static void Loading_Begin(void) {
	if (g_IsXbox) {
		Loading_Start(NULL);
		Engine_SetCallback(APP_CB_LOADING, 4);
		g_XLoadOn = 1;
	} else {
		g_XLoadOn = 0;
	}
}

void Loading_Init(void) {
	Loading_Begin();
}

/* the loading icon of the Xbox loading screen; the original has an unused ecx */
void Loading_Start(void *unused) {
	(void)unused;
	Render_CopyBackBuffer();
	g_XLoadIcon = NULL;
	g_XLoadFrames = 0;
	g_XLoadIconFrame = 0;
	g_XLoadStart = 0.0f;
	g_XLoadLastStep = 0.0f;
	void *img = operator_new(sizeof(CPsaImage));
	if (img) {
		g_XLoadIcon = CPsaImage_ctorFlags(img, "2D/Loading/00Icon.alpha.dds", 0, 3, 0x40024);
		return;
	}
	g_XLoadIcon = NULL;
}

/* the level load that waits for the start video of Engine_LevelEnd (CApp_FrameResume finishes it and the frame) */
static struct {
	byte wait;
	CApp *self;
	const char *level;
	double t0;
} app_load;

/* the rest of CApp_LoadLevelNow after Engine_LevelEnd: the new world, then the game */
static void CApp_LoadLevelEnd(CApp *self, const char *level, double t0, byte reload) {
	vec3_t one;

	if (reload) {
		one = app_one;
		Engine_SetGround(level, "Map/Map3D.m3d", &one);
		Engine_LevelInit();
		Engine_SetCallback(APP_CB_WORLDFRAME, 1);
		g_ForceReload = 0;
	}
	self->vftable->InitGame(self, level);
	if (Net_ClientReady()) {
		Con_Print(0, "'Ready to build' message send to server\n", 0);
	}
	if (self->onLevelLoaded) {
		self->onLevelLoaded();
		self->onLevelLoaded = NULL;
	}
	Engine_LevelLoaded();
	double t1 = self->vftable->GetTime(self);
	Con_Printf("Level loaded in %.2f seconds\n", t1 - t0);
	if (g_XLoadOn) {
		Loading_Stop();
	}
	g_DrawNextFrame = 0;
}

/* vf07: closes the level and loads level (only the logic again for the same level without ForceReload) */
void CApp_LoadLevelNow(CApp *self, const char *level) {
	double t0 = self->vftable->GetTime(self);
	Con_Printf("Loading level at '%s'...\n", level);
	Engine_SetCallback(NULL, 4);
	self->vftable->DoneGame(self);
	Sound_ResetVolumes();
	g_pSound->vftable->StopAllSounds(g_pSound);
	g_pSound->vftable->ResetListeners(g_pSound);
	if (!cString_ICmpStr(&self->levelPath, level) && !g_ForceReload) {
		Loading_Begin();
		CApp_LoadLevelEnd(self, level, t0, 0);
		return;
	}
	g_pSound->vftable->StopMusic(g_pSound);
	g_pSound->vftable->DeleteAllSounds(g_pSound);
	Engine_LevelEnd();
	/* differs from the original on purpose (no blocking waits): while the start video of Engine_LevelEnd plays from
	 * the message loop, the rest of the load (and of the frame) waits for CApp_FrameResume */
	if (Engine_VideoWait()) {
		app_load.wait = 1;
		app_load.self = self;
		app_load.level = level;
		app_load.t0 = t0;
		return;
	}
	CApp_LoadLevelEnd(self, level, t0, 1);
}

/* engine callback 3 with isXbox: the loading tip in a frame */
byte Loading_DrawXbox(void) {
	byte white[PSAIMAGE_SIZE] __attribute__((aligned(4)));
	byte tabs[PSAIMAGE_SIZE] __attribute__((aligned(4)));
	char *lines[16];
	char text[0x400];

	CPsaImage_ctorTex(white, "2D/white.tga", 0, 0x24);
	CPsaImage_ctorFlags(tabs, "2D/Menu/Tabs.alpha.tga", 0, 3, 0x10060);
	Str_Copy(text, *g_LoadMsgString, 0x400);
	int n = Str_WrapText(text, lines, 500.0f, 0x10, "Horatio");
	((CPsaImage *)tabs)->color = 0xa4d7e7ff;
	float h = (float)(n + 1) * 22.0f;
	Menu_DrawFrame(tabs, 1, 52.0f, 420.0f - h, 536.0f, h);
	for (int i = 0, k = n; i < n; i++, k--) {
		CFont_PrintfAlignColorShadow(g_FontHoratio, 320.0f, 420.0f - (float)k * 22.0f, 4, 0xffd7e7ff, "%s",
			lines[i]);
	}
	const char *s = Lang_GetStr("XBox_Loading");
	CFont_PrintfAlignColorShadow(g_FontHoratio, 54.0f, 394.0f - h, 0, 0xffd7e7ff, "%s", s);
	CPsaImage_dtor((CPsaImage *)tabs);
	CPsaImage_dtor((CPsaImage *)white);
	return 1;
}

/* the loading tip of the level into g_LoadMsgString: LoadingTips_Ind_n inside, LoadingTips_Out_n outside, their
 * [set.action] replaced by the key of Pad1 */
void App_PickLoadingTip(CApp *self, const char *level) {
	static const char *const indoor[] = { "ABIndoor", "Orbit", "Bank", "Police", "Press", "Temple", "TownHall" };
	static const char *const mixed[] = { "ShipEdit", "Menu", "Junkyard", "Fake", "GamblinDen" };
	const char *ind = "LoadingTips_Ind_%d", *out = "LoadingTips_Out_%d";
	char buf[0x400];
	const char *fmt;

	(void)self;
	if (strlen(level) <= 7) {
		return;
	}
	if (g_SplitScreenGame) {
		fmt = out;
	} else {
		const char *name = level + 7;
		size_t i;
		fmt = NULL;
		for (i = 0; i < sizeof(indoor) / sizeof(indoor[0]) && !fmt; i++) {
			if (!crt_stricmp(name, indoor[i])) {
				fmt = ind;
			}
		}
		for (i = 0; i < sizeof(mixed) / sizeof(mixed[0]) && !fmt; i++) {
			if (!crt_stricmp(name, mixed[i])) {
				int r = crt_rand() & 0x7fff;
				double d = (double)r * RAND_SCALE;
				d = d + d;
				float f = (float)(d - 1.0);
				fmt = !(f > 0.0f) ? out : ind;
			}
		}
		if (!fmt) {
			fmt = out;
		}
	}
	/* "LoadingTips_X_N": the number of tips */
	size_t len = strlen(fmt);
	memcpy(buf, fmt, len + 1);
	buf[len - 2] = 'N';
	buf[len - 1] = 0;
	const char *count = Lang_GetStr(buf);
	if (!count) {
		return;
	}
	int n = crt_atoi(count);
	int idx = g_LoadMsgIndex % n;
	g_LoadMsgIndex++;
	crt_sprintf(buf, fmt, idx);
	const char *tip = Lang_GetStr(buf);
	if (!tip) {
		return;
	}
	cString_Set(g_LoadMsgString, tip);
	App_CopyStr(buf, *g_LoadMsgString, sizeof(buf));
	char *a = strchr(buf, '['), *dot = strchr(buf, '.'), *b = strchr(buf, ']');
	while (a && dot && b) {
		*dot = 0;
		*b = 0;
		const char *key = Input_GetDefinedEntry(0, a + 1, dot + 1, "Pad1", 0);
		App_CopyStr(a, key, (size_t)(buf + sizeof(buf) - a));
		size_t klen = strlen(key);
		if (klen >= (size_t)(buf + sizeof(buf) - a)) {
			klen = (size_t)(buf + sizeof(buf) - a) - 1;
		}
		size_t rest = strlen(b + 1) + 1;
		if (rest > (size_t)(buf + sizeof(buf) - (a + klen))) {
			rest = (size_t)(buf + sizeof(buf) - (a + klen));
		}
		memmove(a + klen, b + 1, rest);
		buf[sizeof(buf) - 1] = 0;
		a = strchr(buf, '[');
		dot = strchr(buf, '.');
		b = strchr(buf, ']');
	}
	cString_Set(g_LoadMsgString, buf);
	Con_Debugf("Loading string:\n     '%s'\n", *g_LoadMsgString);
}

/* set while the dedicated server's frame waits for a packet (CApp_FrameResume finishes it) */
static int g_AppNetPending;

/* the end of a frame: the client's net frames and the net update */
static void CApp_FrameNet(void) {
	if (Net_IsClient()) {
		while (Net_ClientFrame()) {
		}
	}
	Net_Update();
}

/* the frame after the level load */
static byte CApp_FrameRun(CApp *self) {
	double t = self->vftable->GetTime(self);
	uint32_t flags = self->flags;
	self->time = t;
	if (!flags && !self->active) {
		self->lastFrame = t;
		return 0;
	}
	float dt = (float)(t - self->lastFrame);
	if (1e-6f >= dt) {
		return 0;
	}
	self->lastFrame = t;
	if (!flags && dt > 0.1f && !g_DisableSkipSlot) {
		dt = 0.1f;
	}
	self->gameTime = (double)dt + self->gameTime;
	if (!(flags >> 2 & 1)) {
		Input_Update(dt);
	}
	if (g_DrawNextFrame) {
		self->vftable->Render(self);
	}
	g_DrawNextFrame = 1;
	cPy_Function_Execute(g_AfterDrawCallback, 1);
	if (dt > 0.1f) {
		dt -= 0.1f;
		self->vftable->Update(self, 0.1f);
		Input_ResetStates();
		while (dt > 0.1f) {
			dt -= 0.1f;
			self->vftable->Update(self, 0.1f);
		}
	}
	self->vftable->Update(self, dt);
	if (Net_IsServer() && (self->flags & APPF_SERVER)) {
		if (!(self->flags & APPF_DEDICATED)) {
			while (Net_ServerFrame()) {
			}
		} else {
			Net_ServerFrameHead();
			/* differs from the original on purpose (no blocking waits): the original waited here up to 1 s for a
			 * packet; now the rest of the frame runs from App_WinMain (CApp_FrameResume) once the wait is over */
			if (!Transport_Wait(1000)) {
				g_AppNetPending = 1;
				return 1;
			}
			Net_ServerFrameTail();
		}
	}
	CApp_FrameNet();
	return 1;
}

/* vf05: one frame; 0 when the time did not go on (or the app waits in the background) */
byte CApp_Frame(CApp *self) {
	if (self->loadPending) {
		self->vftable->LoadLevelNow(self, self->newLevelPath);
		/* the rest of the frame after the start video, from CApp_FrameResume */
		if (app_load.wait) {
			return 0;
		}
	}
	return CApp_FrameRun(self);
}

/* the rest of a frame that waits: the level load and the frame after the start video, a video of
 * Video_PlayStart, the dedicated server's wait in Transport_Wait; 0 while there is none */
int CApp_FrameResume(void) {
	if (app_load.wait) {
		if (Engine_VideoResume()) {
			app_load.wait = 0;
			CApp_LoadLevelEnd(app_load.self, app_load.level, app_load.t0, 1);
			CApp_FrameRun(app_load.self);
		}
		return 1;
	}
	if (Engine_VideoWait()) {
		Engine_VideoResume();
		return 1;
	}
	if (!g_AppNetPending) {
		return 0;
	}
	if (Transport_Wait(1000)) {
		g_AppNetPending = 0;
		Net_ServerFrameTail();
		CApp_FrameNet();
	}
	return 1;
}

void CWinApp_LoadLevel(CApp *app, const char *level) {
	cString_Set(&app->newLevelPath, level);
	app->loadPending = 1;
	g_DrawNextFrame = 0;
	App_PickLoadingTip(app, level);
}

/* the clamp of DefaultMaxPlayersOnServer to 1..16 */
static int App_MaxPlayers(void) {
	int n = g_DefaultMaxPlayers;
	if (n <= 1) {
		return 1;
	}
	return n >= 16 ? 16 : n;
}

/* vf12: a -param[:ext] of the command line: an engine variable of the config file, build, dedicated or server */
byte CApp_ParseParam(CApp *self, const char *param, const char *ext) {
	cGVar *v = GVar_Find(param);
	if (v && (v->base.flags & 6)) {
		CHECK(ext[0] != 0);
		byte ro = v->base.flags & 1;
		cGVar_SetFlag(v, 1, 0);
		switch (v->base.type) {
		case 1:
			if (v->base.flags & 1) {
				return 0;
			}
			cString_Set(&v->value.s, ext);
			return 1;
		case 2: {
			int i = crt_atoi(ext);
			if (v->base.flags & 1) {
				return 0;
			}
			v->value.i = i;
			return 1;
		}
		case 4: {
			float f = (float)crt_atof(ext);
			if (v->base.flags & 1) {
				return 0;
			}
			v->value.f = f;
			return 1;
		}
		}
		CHECK(0);
		cGVar_SetFlag(v, 1, ro);
	}
	if (!crt_stricmp("build", param)) {
		if (ext && *ext) {
			self->build = crt_atoi(ext);
		} else {
			self->build = 1;
		}
		return 1;
	}
	if (!crt_stricmp("dedicated", param)) {
		Net_InitServer(*g_iMap, App_MaxPlayers(), g_DefaultServerPort, 1);
		self->dedicated = 1;
		self->vftable->InitDedicated(self);
		return 1;
	}
	if (crt_stricmp("server", param)) {
		return 0;
	}
	if (cString_Eq(g_iMap, g_iMapDefault)) {
		Con_Printf("Using default server map\n");
		cString_Set(g_iMap, "Levels/FZ");
		cString_Set(g_ServerMapList, "['FZ']");
	}
	Con_Printf("Init Server at %s\n", *g_iMap);
	Net_InitServer(*g_iMap, App_MaxPlayers(), g_DefaultServerPort, 0);
	return 1;
}

/* the command line: -param[:ext] items to ParseParam, quotes keep spaces */
void CApp_ParseCmdLine(CApp *self) {
	char param[0x100], ext[0x100];
	int state = 0;
	uint32_t np = 0, ne = 0;
	char quote = 0;
	int i = 0;

	param[0] = 0;
	ext[0] = 0;
	Con_Printf("Command Line : %s\n", self->cmdLine);
	if (self->cmdLine[0]) {
		do {
			char c = self->cmdLine[i];
			if (quote) {
				if (quote != c) {
					goto store;
				}
				quote = 0;
				goto next;
			}
			switch (c) {
			case ' ':
				state = 0;
				if (np >= sizeof(param) || ne >= sizeof(ext)) {
					crt_report_rangecheckfailure();
				}
				param[np] = 0;
				ext[ne] = 0;
				if (param[0]) {
					self->vftable->ParseParam(self, param, ext);
				}
				ne = 0;
				param[0] = 0;
				np = 0;
				ext[0] = 0;
				i++;
				continue;
			case '"':
			case '\'':
				quote = c;
				goto next;
			case '-':
				np = 0;
				state = 1;
				ne = 0;
				param[0] = 0;
				ext[0] = 0;
				i++;
				continue;
			case ':':
				state = 2;
				ne = 0;
				ext[0] = 0;
				goto next;
			default:
				break;
			}
		store:
			/* the original writes before its CHECK; past the end only after a failed one */
			if (state == 1) {
				if (np < sizeof(param)) {
					param[np] = c;
				}
				np++;
			} else if (state == 2) {
				if (ne < sizeof(ext)) {
					ext[ne] = c;
				}
				ne++;
			}
		next:
			i++;
			if (np >= 0x100) {
				CheckFailed("iParam < sizeof(Param)", __FILE__, __TIMESTAMP__, __LINE__);
			}
			if (ne >= 0x100) {
				CheckFailed("iExt < sizeof(Ext)", __FILE__, __TIMESTAMP__, __LINE__);
			}
		} while (self->cmdLine[i]);
		if (np >= sizeof(param)) {
			crt_report_rangecheckfailure();
		}
	}
	param[np] = 0;
	if (ne >= sizeof(ext)) {
		crt_report_rangecheckfailure();
	}
	ext[ne] = 0;
	if (param[0]) {
		self->vftable->ParseParam(self, param, ext);
	}
}

/* engine callback 4 with isXbox: the "Loading..." text with its dots and the turning icon */
byte Loading_Draw(void) {
	byte tabs[PSAIMAGE_SIZE] __attribute__((aligned(4)));
	char text[0x20];
	float dim[2], uv[8], pos[8], in[2], out[2];
	const float *r;

	int f = g_XLoadFrames;
	g_XLoadFrames = f + 1;
	if (f <= 1) {
		g_XLoadStart = (float)g_pApp->vftable->GetTime(g_pApp);
	}
	float now = (float)g_pApp->vftable->GetTime(g_pApp);
	float t = now - g_XLoadStart;
	if (t - g_XLoadLastStep > 0.125f) {
		g_XLoadIconFrame++;
		g_XLoadLastStep = t;
	}
	if (!g_FontHoratio) {
		CheckFailed("fntHoratio", __FILE__, __TIMESTAMP__, __LINE__);
	}
	App_CopyStr(text, Lang_GetStr("XBox_Loading"), sizeof(text));
	Font_TextSize(g_FontHoratio, NULL, dim, 320.0f, 408.0f, text);
	if (g_XLoadFrames < 3) {
		CPsaImage_ctorFlags(tabs, "2D/Menu/Tabs.alpha.tga", 0, 6, 0x10060);
		float w = dim[0] + 30.0f;
		float h = dim[1] + 95.0f;
		((CPsaImage *)tabs)->color = 0xc06b737f;
		Menu_DrawFrame(tabs, 1, 320.0f - w * 0.5f, 382.0f - h * 0.5f, w, h);
		CPsaImage_dtor((CPsaImage *)tabs);
	}
	CFont_PrintfAlignColorShadow(g_FontHoratio, (320.0f - dim[0] * 0.5f) - 1.0f, 407.0f, 0, 0, text);
	CFont_PrintfAlignColorShadow(g_FontHoratio, (320.0f - dim[0] * 0.5f) + 1.0f, 407.0f, 0, 0, text);
	CFont_PrintfAlignColorShadow(g_FontHoratio, (320.0f - dim[0] * 0.5f) - 1.0f, 409.0f, 0, 0, text);
	CFont_PrintfAlignColorShadow(g_FontHoratio, (320.0f - dim[0] * 0.5f) + 1.0f, 409.0f, 0, 0, text);
	uint32_t k = (uint32_t)((int)(t * 3.0f) % 5 - 3 + (int)strlen(text));
	if (k >= sizeof(text)) {
		crt_report_rangecheckfailure();
	}
	text[k] = 0;
	CFont_PrintfAlignColorShadow(g_FontHoratio, 320.0f - dim[0] * 0.5f, 408.0f, 0, 0xffd7e7ff, text);
	if (g_XLoadIcon) {
		float u0 = (float)(g_XLoadIconFrame % 8) * 0.125f;
		float u1 = u0 + 0.125f;
		uv[0] = u0;
		uv[1] = 0.0f;
		uv[2] = u1;
		uv[3] = 0.0f;
		uv[4] = u1;
		uv[5] = 1.0f;
		uv[6] = u0;
		uv[7] = 1.0f;
		static const float corner[4][2] = { { -32.0f, -32.0f }, { 32.0f, -32.0f }, { 32.0f, 32.0f },
			{ -32.0f, 32.0f } };
		for (int i = 0; i < 4; i++) {
			in[0] = corner[i][0];
			in[1] = corner[i][1];
			r = Vec2_Rotate(out, in, 0.0f);
			pos[i * 2] = r[0] + 320.0f;
			pos[i * 2 + 1] = r[1] + 370.0f;
		}
		((CPsaImage *)g_XLoadIcon)->color = 0xffffffff;
		CPsaImage_DrawPoly(g_XLoadIcon, pos, uv, 0, 4, 6);
	}
	return 1;
}

/* vf19 */
byte CApp_RunApp(CApp *self, const char *exe, const char *cmdLine, int arg) {
	(void)self;
	(void)exe;
	(void)cmdLine;
	(void)arg;
	Con_Print(0xff, "RunApp Not supported\n", 0);
	return 0;
}

static void App_NewWorld(void) {
	cWorld *w = operator_new(sizeof(cWorld));
	g_pWorld = w ? cWorld_ctor(w) : NULL;
	cWorld_ResetLogic(g_pWorld);
	Con_Print(0, "World initialized.\n", 0);
}

/* the action classes, the world and the scorer (cdecl member, this on the stack) */
void CApp_SFLB_0(CApp *self) {
	(void)self;
	ActClassList_Init();
	Con_Print(0, "Action class list initialized.\n", 0);
	App_NewWorld();
	Scorer_Init();
	Con_Print(0, "Scorer initialized.\n", 0);
}

/* vf10 */
void CApp_Update(CApp *self, float dt) {
	(void)self;
	cWorld_Update(g_pWorld, dt);
}

/* vf20 */
byte CApp_IsDedicatedWin(CApp *self) {
	(void)self;
	return 0;
}

/* the vtable of CApp */
const CAppVtbl CApp_vftable = {
	.DoneGame = CApp_DoneGame,
	.InitGame = CApp_InitGame,
	.InitDebug = CApp_Nop,
	.Delete = CApp_Delete,
	.Init = CApp_Init,
	.Frame = CApp_Frame,
	.Shutdown = CApp_Shutdown,
	.LoadLevelNow = CApp_LoadLevelNow,
	.InitDedicated = CApp_Nop,
	.Exit = CApp_Nop,
	.Update = CApp_Update,
	.Render = CApp_Render,
	.ParseParam = CApp_ParseParam,
	.ResetParams = CApp_ResetParams,
	.GetTime = (void *)crt_purecall,
	.GetGameTime = CApp_GetGameTime,
	.ShowConsole = (void *)crt_purecall,
	.ConsoleAdd = (void *)crt_purecall,
	.DebugInput = (void *)crt_purecall,
	.RunApp = CApp_RunApp,
	.IsDedicatedWin = CApp_IsDedicatedWin,
};
