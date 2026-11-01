/*
 * App/ of the game: CApp (App.cpp: the game application, its level loading and frame, the command line and the
 * console commands) and CWinApp (WinMain.cpp: the Win32 window, the message loop, the timer, the paths) with the
 * console window of the Win32 build. The structs are CWinApp and its vtable in include/scrap/game.h.
 */
#ifndef SCRAP_APP_H
#define SCRAP_APP_H

#include "scrap/cstring.h"
#include "scrap/game.h"
#include "scrap/pyfunction.h"
#include "scrap/scrap.h"

#define g_CAppVftable ((const CAppVtbl *)GAME_VFTABLE(CApp_vftable, 0x3cf190))
#define g_CWinAppVftable ((const CAppVtbl *)GAME_VFTABLE(CWinApp_vftable, 0x3cf7d4))

/* engine variables (Gvars) */
#define g_LoadMsgIndex GAME_GLOBAL_IN(int, GVar_iLoadMsgString, 16, 0x4679c0)		/* iLoadMsgString: the next loading tip */
#define g_iMap (GAME_ARRAY_IN(cString, GVar_iMap, 16, 0x4679d8))		/* iMap: the first level */
#define g_iMapDefault (GAME_ARRAY_IN(cString, GVar_iMap, 20, 0x4679dc))	/* its default value */
#define g_ReRunAtExitParams (GAME_ARRAY_IN(cString, GVar_ReRunAtExitParams, 16, 0x4679f0))	/* RunApp of the game with them at the end */
#define g_AfterDrawCallback (GAME_ARRAY_IN(cPy_Function, GVar_AfterDrawCallback, 16, 0x467a08))
#define g_DrawNextFrame GAME_GLOBAL_IN(int, GVar_DrawNextFrame, 16, 0x467a2c)
#define g_DefaultMaxPlayers GAME_GLOBAL_IN(int, GVar_DefaultMaxPlayersOnServer, 16, 0x467a44)	/* DefaultMaxPlayersOnServer */
#define g_ModFileName GAME_GLOBAL_IN(const char *, GVar_ModFileName, 16, 0x467a5c)	/* a pack opened after Pack1 */
#define g_DisableSkipSlot GAME_GLOBAL_IN(int, GVar_DisableSkipSlot, 16, 0x467aa4)	/* frames longer than 0.1 s are not cut */
#define g_DisableSkipSlotDefault GAME_GLOBAL_IN(int, GVar_DisableSkipSlot, 20, 0x467aa8)
#define g_ForceReload GAME_GLOBAL_IN(int, GVar_ForceReload, 16, 0x467abc)		/* reload the floor of the same level */
#define g_ModPathName GAME_GLOBAL_IN(const char *, GVar_ModPathName, 16, 0x467b04)	/* a directory of packs after "mods" */
#define g_LoadMsgString (GAME_ARRAY_IN(cString, GVar_LoadMsgString, 16, 0x467b4c))	/* the loading tip */
#define g_DefaultServerPort GAME_GLOBAL_IN(uint16_t, GVar_DefaultServerPort, 16, 0x467b64)
#define g_InactiveBackgroundApp GAME_GLOBAL_IN(int, GVar_InactiveBackgroundApp, 16, 0x467bb4)	/* the frames go on without the focus */
#define g_ServerMapList (GAME_ARRAY_IN(cString, GVar_ServerMapList, 16, 0x46759c))
#define g_PlayerName (GAME_ARRAY_IN(cString, GVar_PlayerName, 16, 0x448148))
#define g_SoundInit GAME_GLOBAL_IN(int, GVar_SoundInit, 16, 0x466d78)
#define g_LoadTextures GAME_GLOBAL_IN(int, GVar_LoadTextures, 16, 0x4aa348)
#define g_GVarMasterServerAddress (GAME_ARRAY(cGVar, g_GVarMasterServerAddress, 0x463c9c))
#define g_GVarMasterServerPort (GAME_ARRAY(cGVar, g_GVarMasterServerPort, 0x463d98))
#define g_LangName (GAME_ARRAY_IN(cString, GVar_SelectedLanguage, 16, 0x44b618))		/* the language asked for */
#define g_LangDefault GAME_GLOBAL_IN(const char *, GVar_SelectedLanguage, 20, 0x44b61c)	/* the language when that file is missing */
#define g_LogName GAME_GLOBAL_IN(const char *, GVar_LogFile, 16, 0x445a5c)

/* App.cpp state */
#define g_AppBlock GAME_GLOBAL(int *, g_AppBlock, 0x467978)		/* 16 bytes of VirtualAlloc, [3] = 0x16 */
#define g_AppPyOne GAME_GLOBAL(void *, g_AppPyOne, 0x46797c)		/* Py_BuildValue("i", 1) of each level */
/* the Xbox loading screen (Loading_Init, isXbox) */
#define g_XLoadIcon GAME_GLOBAL(void *, g_XLoadIcon, 0x467b84)	/* CPsaImage (0x44 bytes) */
#define g_XLoadFrames GAME_GLOBAL(int, g_XLoadFrames, 0x467b88)
#define g_XLoadIconFrame GAME_GLOBAL(int, g_XLoadIconFrame, 0x467b8c)
#define g_XLoadStart GAME_GLOBAL(float, g_XLoadStart, 0x467b90)
#define g_XLoadLastStep GAME_GLOBAL(float, g_XLoadLastStep, 0x467b94)
#define g_XLoadOn GAME_GLOBAL(byte, g_XLoadOn, 0x467b98)

/* WinMain.cpp state */
#define g_TimeQPPause GAME_GLOBAL(uint32_t, g_TimeQPPause, 0x467bbc)	/* subtracted from the counter of GetTime */
#define g_TimeMsPause GAME_GLOBAL(uint32_t, g_TimeMsPause, 0x467bc0)	/* the same for timeGetTime */
#define g_RegValue (GAME_ARRAY(char, g_RegValue, 0x467bc8))		/* 0x200, App_RegGetString */
#define g_TimeInit GAME_GLOBAL(byte, g_TimeInit, 0x467dc8)
#define g_TimeUseQP GAME_GLOBAL(int, g_TimeUseQP, 0x467dcc)
#define g_TimeQPFreq GAME_GLOBAL(int64_t, g_TimeQPFreq, 0x467dd0)
#define g_TimeQPStart GAME_GLOBAL(int64_t, g_TimeQPStart, 0x467dd8)
#define g_TimeMsStart GAME_GLOBAL(uint32_t, g_TimeMsStart, 0x467de0)
#define g_WinWidth GAME_GLOBAL(int, g_WinWidth, 0x431ab0)
#define g_WinHeight GAME_GLOBAL(int, g_WinHeight, 0x431ab4)
#define g_WideWinWidth GAME_GLOBAL(int, g_WideWinWidth, 0x431ab8)
#define g_WideWinHeight GAME_GLOBAL(int, g_WideWinHeight, 0x431abc)
#define g_ScrapTitle GAME_STR(str_431a98, 0x431a98)	/* "Scrapland" */
#define g_ScrapClass GAME_STR(str_431aa4, 0x431aa4)	/* "ScrapClass" */

/* App.c */
CApp *__thiscall CApp_ctor(CApp *self, const char *cmdLine);
void __fastcall CApp_dtor(CApp *self);
CApp *__thiscall CApp_Delete(CApp *self, byte flags);
void __thiscall CApp_DoneGame(CApp *self);
void __thiscall CApp_InitGame(CApp *self, const char *level);
void __thiscall CApp_Nop(CApp *self);
byte __thiscall CApp_Init(CApp *self);
/* the rest of CApp_Init when it returned while the videos of Engine_Init play (Engine_VideoWait); 0 while they
 * still play */
byte CApp_InitResume(CApp *self);
byte __thiscall CApp_Frame(CApp *self);
/* the rest of a frame that waits (the level load after the start video, the dedicated server's packet wait), from
 * the message loop; 0 while none waits */
int CApp_FrameResume(void);
void __thiscall CApp_Shutdown(CApp *self);
void __thiscall CApp_LoadLevelNow(CApp *self, const char *level);
void __thiscall CApp_Update(CApp *self, float dt);
void __thiscall CApp_Render(CApp *self);
byte __thiscall CApp_ParseParam(CApp *self, const char *param, const char *ext);
void __thiscall CApp_ResetParams(CApp *self);
double __thiscall CApp_GetGameTime(CApp *self);
byte __thiscall CApp_RunApp(CApp *self, const char *exe, const char *cmdLine, int arg);
byte __thiscall CApp_IsDedicatedWin(CApp *self);
void __fastcall CApp_ParseCmdLine(CApp *self);
void CApp_SFLB_0(CApp *self);
byte App_CheckBlock(void);
void Console_Execute(const char *cmd);
void *App_GetObjData(const char *name);
void __thiscall CWinApp_LoadLevel(CApp *app, const char *level);
void __thiscall App_PickLoadingTip(CApp *self, const char *level);
void Loading_Init(void);
void Loading_End(void);
void __fastcall Loading_Start(void *unused);
byte Loading_DrawXbox(void);
byte Loading_Draw(void);

/* WinApp.c */
CWinApp *__thiscall CWinApp_ctor(CWinApp *self, void *hInst, void *hPrev, const char *cmdLine, int nCmdShow);
void __fastcall CWinApp_dtor(CWinApp *self);
CWinApp *__thiscall CWinApp_Delete(CWinApp *self, byte flags);
intptr_t __thiscall CWinApp_WndProc(CWinApp *self, void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam);
int __thiscall CWinApp_ConsoleAdd(CWinApp *self, const char *text, uint32_t color);
int __thiscall CWinApp_ShowConsole(CWinApp *self, byte show);
byte __fastcall CWinApp_CreateMainWindow(CWinApp *self);
void *__fastcall CWinApp_CreateWindow(CWinApp *self);
const char *__thiscall CWinApp_DebugInput(CWinApp *self, const char *prompt);
void __thiscall CWinApp_InitDedicated(CWinApp *self);
void __thiscall CWinApp_Shutdown(CWinApp *self);
void __thiscall CWinApp_Exit(CWinApp *self);
void __thiscall CWinApp_ResetParams(CWinApp *self);
int __thiscall CWinApp_HasArg(CWinApp *self, const char *name);
/* the path of the game into buf (rva 0x20d100) */
void App_GetPath(char *buf, int size);
const char *__thiscall App_RegGetString(CWinApp *self, const char *name);
void *App_GetHInstance(void);
double __thiscall CWinApp_GetTime(CWinApp *self);
void *App_GetHWnd(void);
byte __thiscall CWinApp_Init(CWinApp *self);
byte __thiscall CWinApp_Frame(CWinApp *self);
void ConsoleWin_Add(const char *text, uint32_t color);
void App_GetConfigDir(char *buf, int size);
byte __thiscall CWinApp_ParseParam(CWinApp *self, const char *param, const char *ext);
byte __thiscall CWinApp_RunApp(CWinApp *self, const char *exe, const char *cmdLine, int arg);
intptr_t __stdcall App_WndProc(void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam);
byte __thiscall CWinApp_IsDedicatedWin(CWinApp *self);
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
byte __thiscall cGVar_SetFlag(cGVar *self, byte bit, byte on);
void Lang_Free(void);
void Python_Init(void);
void Python_Close(void);
void Python_Shutdown(void);
int Str_WrapText(char *text, char **lines, float maxWidth, int maxLines, const char *fontName);
int PyRun_ScrapString(const char *code);

/* other modules of the exe (src/imports.inc) */
void InputMaps_Done(void);					/* rva 0x139900 */
void InputMaps_Init(void);					/* rva 0x139ba0 */
void InputMaps_Clear(void);					/* rva 0x139cf0 */
void Input_ResetActions(void);					/* rva 0x13a380 */
void Input_ResetStates(void);					/* rva 0x13a4b0 */
void Input_Update(float dt);					/* rva 0x13bf20 */
void Input_ReleaseAll(int arg);					/* rva 0x13c610 */
void Input_CloseDevices(void);					/* rva 0x13c660 */
void Input_OpenDevices(void);					/* rva 0x13c6e0 */
void Input_OnChar(int c);					/* rva 0x13c900 */
void Scorer_Done(void);						/* rva 0x170ad0 */
void Scorer_Init(void);						/* rva 0x171120 */
void Menu_DrawFrame(void *img, int arg, float x, float y, float w, float h);	/* rva 0x184ef0 */
/* a bar of frac (0..1) of the frame rectangle u0..v1 over the rectangle bu0..bv1, colors main and black */
void Menu_DrawBar(void *img, float x, float y, float frac, int mode, float f, float u0, float v0, float u1, float v1,
	float bu0, float bv0, float bu1, float bv1, uint32_t color, uint32_t black);	/* rva 0x1847f0 */
void Font_SetLanguage(const char *lang);			/* rva 0x21b1e0 */
void *__thiscall CPsaImage_ctorTex(void *self, const char *name, int a, int b);	/* rva 0x22d0c0 */
void *__thiscall CPsaImage_ctorFlags(void *self, const char *name, int a, int b, int flags);	/* rva 0x22d0f0 */
/* a polygon of n corners (pos, uv and colors optional, NULL for the image ones); mode 6 */
void __thiscall CPsaImage_DrawPoly(void *self, const float *pos, const float *uv, const uint32_t *colors, int n,
	int mode);	/* rva 0x22d520 */
void D3DApp_OnActivate(void);					/* rva 0x2325e0 */
intptr_t D3DApp_MsgProc(void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam);	/* rva 0x233030 */
/* ScrapEngWin32.cpp (src/game/ScrapEng/ScrapEngWin32.c): the filter of the unhandled exceptions on or off */
void App_SetExceptionFilter(byte on);
int __stdcall App_ExceptionFilter(void **ep);
/* the expiry of the test version of 2004 (not called) */

#endif
