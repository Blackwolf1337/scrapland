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

#define g_CAppVftable ((const CAppVtbl *)GAME_VFTABLE(CApp_vftable))
#define g_CWinAppVftable ((const CAppVtbl *)GAME_VFTABLE(CWinApp_vftable))

/* engine variables (Gvars) */
extern cGVar GVar_iLoadMsgString;
#define g_LoadMsgIndex (GVar_iLoadMsgString.value.i)		/* iLoadMsgString: the next loading tip */
#define g_iMap (GAME_ARRAY_IN(cString, GVar_iMap, 16))		/* iMap: the first level */
#define g_iMapDefault (GAME_ARRAY_IN(cString, GVar_iMap, 20))	/* its default value */
#define g_ReRunAtExitParams (GAME_ARRAY_IN(cString, GVar_ReRunAtExitParams, 16))	/* RunApp of the game with them at the end */
#define g_AfterDrawCallback (GAME_ARRAY_IN(cPy_Function, GVar_AfterDrawCallback, 16))
extern cGVar GVar_DrawNextFrame;
#define g_DrawNextFrame (GVar_DrawNextFrame.value.i)
extern cGVar GVar_DefaultMaxPlayersOnServer;
#define g_DefaultMaxPlayers (GVar_DefaultMaxPlayersOnServer.value.i)	/* DefaultMaxPlayersOnServer */
extern cGVar GVar_ModFileName;
#define g_ModFileName (GVar_ModFileName.value.s)	/* a pack opened after Pack1 */
extern cGVar GVar_DisableSkipSlot;
#define g_DisableSkipSlot (GVar_DisableSkipSlot.value.i)	/* frames longer than 0.1 s are not cut */
#define g_DisableSkipSlotDefault (GVar_DisableSkipSlot.def.i)
extern cGVar GVar_ForceReload;
#define g_ForceReload (GVar_ForceReload.value.i)		/* reload the floor of the same level */
extern cGVar GVar_ModPathName;
#define g_ModPathName (GVar_ModPathName.value.s)	/* a directory of packs after "mods" */
#define g_LoadMsgString (GAME_ARRAY_IN(cString, GVar_LoadMsgString, 16))	/* the loading tip */
#define g_DefaultServerPort GAME_GLOBAL_IN(uint16_t, GVar_DefaultServerPort, 16)
extern cGVar GVar_InactiveBackgroundApp;
#define g_InactiveBackgroundApp (GVar_InactiveBackgroundApp.value.i)	/* the frames go on without the focus */
#define g_ServerMapList (GAME_ARRAY_IN(cString, GVar_ServerMapList, 16))
#define g_PlayerName (GAME_ARRAY_IN(cString, GVar_PlayerName, 16))
extern cGVar GVar_SoundInit;
#define g_SoundInit (GVar_SoundInit.value.i)
extern cGVar GVar_LoadTextures;
#define g_LoadTextures (GVar_LoadTextures.value.i)
#define g_GVarMasterServerAddress (GAME_ARRAY(cGVar, g_GVarMasterServerAddress))
#define g_GVarMasterServerPort (GAME_ARRAY(cGVar, g_GVarMasterServerPort))
#define g_LangName (GAME_ARRAY_IN(cString, GVar_SelectedLanguage, 16))		/* the language asked for */
extern cGVar GVar_SelectedLanguage;
#define g_LangDefault (GVar_SelectedLanguage.def.s)	/* the language when that file is missing */
extern cGVar GVar_LogFile;
#define g_LogName (GVar_LogFile.value.s)

/* App.cpp state */
extern int *g_AppBlock;		/* 16 bytes of VirtualAlloc, [3] = 0x16 */
extern void *g_AppPyOne;		/* Py_BuildValue("i", 1) of each level */
/* the Xbox loading screen (Loading_Init, isXbox) */
extern void *g_XLoadIcon;	/* CPsaImage (0x44 bytes) */
extern int g_XLoadFrames;
extern int g_XLoadIconFrame;
extern float g_XLoadStart;
extern float g_XLoadLastStep;
extern byte g_XLoadOn;

/* WinMain.cpp state */
extern uint32_t g_TimeQPPause;	/* subtracted from the counter of GetTime */
extern uint32_t g_TimeMsPause;	/* the same for timeGetTime */
extern byte g_TimeInit;
extern int g_TimeUseQP;
extern int64_t g_TimeQPFreq;
extern int64_t g_TimeQPStart;
extern uint32_t g_TimeMsStart;
extern int g_WinWidth;
extern int g_WinHeight;
extern int g_WideWinWidth;
extern int g_WideWinHeight;
#define g_ScrapTitle GAME_STR(str_431a98)	/* "Scrapland" */
#define g_ScrapClass GAME_STR(str_431aa4)	/* "ScrapClass" */

/* App.c */
CApp *CApp_ctor(CApp *self, const char *cmdLine);
void CApp_dtor(CApp *self);
CApp *CApp_Delete(CApp *self, byte flags);
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
byte CApp_RunApp(CApp *self, const char *exe, const char *cmdLine, int arg);
byte CApp_IsDedicatedWin(CApp *self);
void CApp_ParseCmdLine(CApp *self);
void CApp_SFLB_0(CApp *self);
byte App_CheckBlock(void);
void Console_Execute(const char *cmd);
void *App_GetObjData(const char *name);
void CWinApp_LoadLevel(CApp *app, const char *level);
void App_PickLoadingTip(CApp *self, const char *level);
void Loading_Init(void);
void Loading_End(void);
void Loading_Start(void *unused);
byte Loading_DrawXbox(void);
byte Loading_Draw(void);

/* WinApp.c */
CWinApp *CWinApp_ctor(CWinApp *self, void *hInst, void *hPrev, const char *cmdLine, int nCmdShow);
void CWinApp_dtor(CWinApp *self);
CWinApp *CWinApp_Delete(CWinApp *self, byte flags);
int CWinApp_ConsoleAdd(CWinApp *self, const char *text, uint32_t color);
int CWinApp_ShowConsole(CWinApp *self, byte show);
byte CWinApp_CreateMainWindow(CWinApp *self);
const char *CWinApp_DebugInput(CWinApp *self, const char *prompt);
void CWinApp_InitDedicated(CWinApp *self);
void CWinApp_Shutdown(CWinApp *self);
void CWinApp_Exit(CWinApp *self);
void CWinApp_ResetParams(CWinApp *self);
int CWinApp_HasArg(CWinApp *self, const char *name);
/* the path of the game into buf */
void App_GetPath(char *buf, int size);
void *App_GetHInstance(void);
double CWinApp_GetTime(CWinApp *self);
void *App_GetHWnd(void);
byte CWinApp_Init(CWinApp *self);
byte CWinApp_Frame(CWinApp *self);
void ConsoleWin_Add(const char *text, uint32_t color);
void App_GetConfigDir(char *buf, int size);
byte CWinApp_ParseParam(CWinApp *self, const char *param, const char *ext);
byte CWinApp_RunApp(CWinApp *self, const char *exe, const char *cmdLine, int arg);
byte CWinApp_IsDedicatedWin(CWinApp *self);
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
int PyRun_ScrapString(const char *code);

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
void *CPsaImage_ctorTex(void *self, const char *name, int a, int b);
void *CPsaImage_ctorFlags(void *self, const char *name, int a, int b, int flags);
/* a polygon of n corners (pos, uv and colors optional, NULL for the image ones); mode 6 */
void CPsaImage_DrawPoly(void *self, const float *pos, const float *uv, const uint32_t *colors, int n,
	int mode);
void D3DApp_OnActivate(void);
/* ScrapEngWin32.cpp (src/game/ScrapEng/ScrapEngWin32.c): the filter of the unhandled exceptions on or off */
void App_SetExceptionFilter(byte on);
int __stdcall App_ExceptionFilter(void **ep);
/* the expiry of the test version of 2004 (not called) */

#endif
