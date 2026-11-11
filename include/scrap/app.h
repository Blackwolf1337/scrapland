/*
 * App/ of the game: CApp (App.cpp: the game application, its level loading and frame, the command line and the
 * console commands) and CWinApp (WinMain.cpp: the Win32 window, the message loop, the timer, the paths) with the
 * console window of the Win32 build. The structs are CWinApp and its vtable in include/scrap/game.h.
 */
#ifndef SCRAP_APP_H
#define SCRAP_APP_H

#include "scrap/gvar.h"
#include "scrap/cstring.h"
#include "scrap/game.h"
#include "scrap/pyfunction.h"
#include "scrap/scrap.h"

extern cGVar GVar_SoundInit;
#define g_SoundInit (GVar_SoundInit.value.i)
extern cGVar GVar_LoadTextures;
#define g_LoadTextures (GVar_LoadTextures.value.i)
extern cGVar g_GVarMasterServerAddress;
extern cGVar g_GVarMasterServerPort;
#define g_LangName (((cString *)&GVar_SelectedLanguage.value))		/* the language asked for */
extern cGVar GVar_SelectedLanguage;
#define g_LangDefault (GVar_SelectedLanguage.def.s)	/* the language when that file is missing */
extern cGVar GVar_LogFile;
#define g_LogName (GVar_LogFile.value.s)

extern void *g_AppPyOne;		/* Py_BuildValue("i", 1) of each level */

/* App.c */
CApp *CApp_ctor(CApp *self, const char *cmdLine);
void CApp_dtor(CApp *self);
void CApp_DoneGame(CApp *self);
void CApp_InitGame(CApp *self, const char *level);
void CApp_Nop(CApp *self);
byte CApp_Init(CApp *self);
/* the rest of CApp_Init when it returned while the videos of Engine_Init play (Engine_VideoWait); 0 while they
 * still play */
byte CApp_InitResume(CApp *self);
byte CApp_Frame(CApp *self);
/* the rest of a frame that waits (the level load after the start video, the dedicated server's packet wait), from
 * the message loop; 0 while none waits */
int CApp_FrameResume(void);
void CApp_Shutdown(CApp *self);
void CApp_LoadLevelNow(CApp *self, const char *level);
void CApp_Update(CApp *self, float dt);
void CApp_Render(CApp *self);
byte CApp_ParseParam(CApp *self, const char *param, const char *ext);
void CApp_ResetParams(CApp *self);
double CApp_GetGameTime(CApp *self);
void Console_Execute(const char *cmd);
/* the forms of ConsoleCmd.c for the test channel: Python 1.5.2 repr() of a float and of a string, and of an engine
 * variable as Scrap.Get gives it (0 when there is none) */
void Console_ReprFloat(char *buf, int size, double d);
void Console_ReprString(char *buf, int size, const char *s);
byte Console_ReprVar(const char *name, char *buf, int size);
/* the test channel (TestChannel.c): on by -TestChannel or SCRAP_TEST_CHANNEL=1; the check once a frame (now: the
 * real time in seconds, at most one check in 5 ms); while the world clock runs the check is TestChannel_WorldFrame,
 * called by cWorld_Update right after the scheduled functions */
void TestChannel_Enable(void);
void TestChannel_Frame(double now);
void TestChannel_WorldFrame(void);
/* SCRAP_TEST_CHANNEL=step in the deterministic test mode (scrap/testdet.h): 1 while the clock of the mode has to stand:
 * no command file waits, or the one that waits has had its pace */
int TestChannel_Hold(void);
void CWinApp_LoadLevel(CApp *app, const char *level);
void Loading_Init(void);
void Loading_End(void);

/* the path of the game into buf */
void App_GetPath(char *buf, int size);
void *App_GetHInstance(void);
void *App_GetHWnd(void);
void ConsoleWin_Add(const char *text, uint32_t color);
void App_GetConfigDir(char *buf, int size);
int __stdcall App_WinMain(void *hInst, void *hPrev, char *cmdLine, int nCmdShow);

/* C functions declared only in their files */
void DebugLog_Init(void);
void DebugLog_CloseAll(void);
void Pack_Build(const char *dir, const char *outName, int mode);
void Pack_CloseAll(void);
void Pack_OpenDir(const char *dir);
void GVars_Init(void);
void GVars_Done(void);
const char *GVar_Complete(const char *prefix);
byte cGVar_SetFlag(cGVar *self, byte bit, byte on);
void Lang_Free(void);
void Python_Init(void);
void Python_Close(void);
void Python_Shutdown(void);
int Str_WrapText(char *text, char **lines, float maxWidth, int maxLines, const char *fontName);

/* other modules of the exe (src/imports.inc) */
void InputMaps_Done(void);
void InputMaps_Init(void);
void InputMaps_Clear(void);
void Input_ResetActions(void);
void Input_ResetStates(void);
void Input_Update(float dt);
void Input_ReleaseAll(int arg);
void Input_CloseDevices(void);
void Input_OpenDevices(void);
void Input_OnChar(int c);
void Scorer_Done(void);
void Scorer_Init(void);
void Menu_DrawFrame(void *img, int arg, float x, float y, float w, float h);
/* a bar of frac (0..1) of the frame rectangle u0..v1 over the rectangle bu0..bv1, colors main and black */
void Menu_DrawBar(void *img, float x, float y, float frac, int mode, float f, float u0, float v0, float u1, float v1,
	float bu0, float bv0, float bu1, float bv1, uint32_t color, uint32_t black);
void Font_SetLanguage(const char *lang);
void *CPsaImage_ctorTex(void *self, const char *name, const char *a, int b);
void *CPsaImage_ctorFlags(void *self, const char *name, const char *a, int b, int flags);
/* a polygon of n corners (pos, uv and colors optional, NULL for the image ones); mode 6 */
void CPsaImage_DrawPoly(void *self, const float *pos, const float *uv, const uint32_t *colors, int n,
	int mode);
void D3DApp_OnActivate(void);
/* ScrapEngWin32.cpp (src/game/ScrapEng/ScrapEngWin32.c): the filter of the unhandled exceptions on or off */
void App_SetExceptionFilter(byte on);
/* the expiry of the test version of 2004 (not called) */

#endif
