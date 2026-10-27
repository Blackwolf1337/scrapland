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

/* the CApp part of a CWinApp (0x48 bytes) */
typedef CWinApp CApp;

#define g_CAppVftable ((const CWinAppVtbl *)Game_Ptr(0x3cf190))
#define g_CWinAppVftable ((const CWinAppVtbl *)Game_Ptr(0x3cf7d4))

/* engine variables (Gvars) */
#define g_LoadMsgIndex GAME_DATA(int, 0x4679c0)		/* iLoadMsgString: the next loading tip */
#define g_iMap ((cString *)Game_Ptr(0x4679d8))		/* iMap: the first level */
#define g_iMapDefault ((cString *)Game_Ptr(0x4679dc))	/* its default value */
#define g_ReRunAtExitParams ((cString *)Game_Ptr(0x4679f0))	/* RunApp of the game with them at the end */
#define g_AfterDrawCallback ((cPy_Function *)Game_Ptr(0x467a08))
#define g_DrawNextFrame GAME_DATA(int, 0x467a2c)
#define g_DefaultMaxPlayers GAME_DATA(int, 0x467a44)	/* DefaultMaxPlayersOnServer */
#define g_ModFileName GAME_DATA(const char *, 0x467a5c)	/* a pack opened after Pack1 */
#define g_DisableSkipSlot GAME_DATA(int, 0x467aa4)	/* frames longer than 0.1 s are not cut */
#define g_DisableSkipSlotDefault GAME_DATA(int, 0x467aa8)
#define g_ForceReload GAME_DATA(int, 0x467abc)		/* reload the floor of the same level */
#define g_ModPathName GAME_DATA(const char *, 0x467b04)	/* a directory of packs after "mods" */
#define g_LoadMsgString ((cString *)Game_Ptr(0x467b4c))	/* the loading tip */
#define g_DefaultServerPort GAME_DATA(uint16_t, 0x467b64)
#define g_InactiveBackgroundApp GAME_DATA(int, 0x467bb4)	/* the frames go on without the focus */
#define g_ServerMapList ((cString *)Game_Ptr(0x46759c))
#define g_PlayerName ((cString *)Game_Ptr(0x448148))
#define g_SoundInit GAME_DATA(int, 0x466d78)
#define g_LoadTextures GAME_DATA(int, 0x4aa348)
#define g_GVarMasterServerAddress ((cGVar *)Game_Ptr(0x463c9c))
#define g_GVarMasterServerPort ((cGVar *)Game_Ptr(0x463d98))
#define g_LangName ((cString *)Game_Ptr(0x44b618))		/* the language asked for */
#define g_LangDefault GAME_DATA(const char *, 0x44b61c)	/* the language when that file is missing */
#define g_LogName GAME_DATA(const char *, 0x445a5c)
#define g_GogReady GAME_DATA(byte, 0x44c108)		/* set by the GOG sign in listener */

/* App.cpp state */
#define g_AppBlock GAME_DATA(int *, 0x467978)		/* 16 bytes of VirtualAlloc, [3] = 0x16 */
#define g_AppPyOne GAME_DATA(void *, 0x46797c)		/* Py_BuildValue("i", 1) of each level */
/* the Xbox loading screen (Loading_Init, isXbox) */
#define g_XLoadIcon GAME_DATA(void *, 0x467b84)	/* CPsaImage (0x44 bytes) */
#define g_XLoadFrames GAME_DATA(int, 0x467b88)
#define g_XLoadIconFrame GAME_DATA(int, 0x467b8c)
#define g_XLoadStart GAME_DATA(float, 0x467b90)
#define g_XLoadLastStep GAME_DATA(float, 0x467b94)
#define g_XLoadOn GAME_DATA(byte, 0x467b98)

/* WinMain.cpp state */
#define g_TimeQPPause GAME_DATA(uint32_t, 0x467bbc)	/* subtracted from the counter of GetTime */
#define g_TimeMsPause GAME_DATA(uint32_t, 0x467bc0)	/* the same for timeGetTime */
#define g_RegValue ((char *)Game_Ptr(0x467bc8))		/* 0x200, App_RegGetString */
#define g_TimeInit GAME_DATA(byte, 0x467dc8)
#define g_TimeUseQP GAME_DATA(int, 0x467dcc)
#define g_TimeQPFreq GAME_DATA(int64_t, 0x467dd0)
#define g_TimeQPStart GAME_DATA(int64_t, 0x467dd8)
#define g_TimeMsStart GAME_DATA(uint32_t, 0x467de0)
#define g_WinWidth GAME_DATA(int, 0x431ab0)
#define g_WinHeight GAME_DATA(int, 0x431ab4)
#define g_WideWinWidth GAME_DATA(int, 0x431ab8)
#define g_WideWinHeight GAME_DATA(int, 0x431abc)
#define g_ScrapTitle ((const char *)Game_Ptr(0x431a98))	/* "Scrapland" */
#define g_ScrapClass ((const char *)Game_Ptr(0x431aa4))	/* "ScrapClass" */

/* the console window (RichEdit output, edit line, four buttons, 64 lines of history) */
#define CONWIN_HISTORY 64
#define CONWIN_LINE 0x1000
#define g_ConInput ((char *)Game_Ptr(0x467de8))		/* 0x200, the text of the input dialog */
typedef void (*conexec_t)(const char *cmd);
#define g_ConExec GAME_DATA(conexec_t, 0x467fe8)	/* Console_Execute */
#define g_ConInst GAME_DATA(void *, 0x467fec)
#define g_ConHwnd GAME_DATA(void *, 0x467ff0)
#define g_ConParent GAME_DATA(void *, 0x467ff4)
#define g_ConRich GAME_DATA(void *, 0x467ff8)
#define g_ConEdit GAME_DATA(void *, 0x467ffc)
#define g_ConBtnPlus GAME_DATA(void *, 0x468000)
#define g_ConBtnMinus GAME_DATA(void *, 0x468004)
#define g_ConBtnBang GAME_DATA(void *, 0x468008)
#define g_ConBtnGo GAME_DATA(void *, 0x46800c)
#define g_ConFont GAME_DATA(void *, 0x468010)
#define g_ConEditProc GAME_DATA(void *, 0x468014)	/* the window procedure of the edit before the subclass */
#define g_ConHistory ((char (*)[CONWIN_LINE])Game_Ptr(0x468018))	/* [0] the last entered line */
#define g_ConHistPos GAME_DATA(int, 0x4a8018)
#define g_ConOpen GAME_DATA(int, 0x4a801c)
#define g_ConRichLib GAME_DATA(void *, 0x4a8020)
#define g_ConDlgPrompt GAME_DATA(const char *, 0x431ac0)
#define g_ConTitle ((const char *)Game_Ptr(0x431ac4))		/* "Console" */
#define g_ConClass ((const char *)Game_Ptr(0x431acc))		/* "ScrapConsoleClass" */
#define g_ConInputH GAME_DATA(int, 0x431ae0)		/* 20, or 200 for the big input */

/* App.c */
CApp *__thiscall CApp_ctor(CApp *self, const char *cmdLine);
void __fastcall CApp_dtor(CApp *self);
CApp *__thiscall CApp_Delete(CApp *self, byte flags);
void __thiscall CApp_DoneGame(CApp *self);
void __thiscall CApp_InitGame(CApp *self, const char *level);
void __thiscall CApp_Nop(CApp *self);
byte __thiscall CApp_Init(CApp *self);
byte __thiscall CApp_Frame(CApp *self);
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
void CApp_SFLB_1(CApp *self);
void CApp_SFINIT0(void);
byte App_CheckBlock(void);
void Console_Execute(const char *cmd);
void *App_GetObjData(const char *name);
float World_TimeScale(void);
void __thiscall CWinApp_LoadLevel(CWinApp *app, const char *level);
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
void __fastcall GogListeners_dtor(cGogListeners *self);
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

/* the console window */
int ConsoleWin_Toggle(byte open, void *parent, byte bottom);
void ConsoleWin_Layout(void);
int ConsoleWin_Close(void);
const char *ConsoleWin_Input(const char *prompt);
int ConsoleWin_Print(const char *text, uint32_t color);
void ConsoleWin_ToggleSize(void);
void ConsoleWin_Focus(void *hwnd);
intptr_t __stdcall ConsoleWin_DlgProc(void *hdlg, uint32_t msg, uintptr_t wParam, intptr_t lParam);
intptr_t __stdcall ConsoleWin_EditProc(void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam);
void *ConsoleWin_GetHwnd(void);
int ConsoleWin_Open(void *parent, byte bottom);
int ConsoleWin_Create(void *inst, int x, int y, int nCmdShow, void *parent);
int ConsoleWin_RegisterClass(void *inst);
void ConsoleWin_HistoryNewer(void);
void ConsoleWin_HistoryOlder(void);
uint32_t __stdcall ConsoleWin_StreamOut(void *file, const void *buf, int32_t n, int32_t *written);
int ConsoleWin_SaveOutput(void);
int ConsoleWin_Show(byte show);
void ConsoleWin_Enter(void);
void ConsoleWin_ToggleInput(void);
intptr_t __stdcall ConsoleWin_WndProc(void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam);

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
void ActClassList_Done(void);					/* rva 0x062140 */
void ActClassList_Init(void);					/* rva 0x0623a0 */
cGogListeners *__thiscall GogListeners_ctor(cGogListeners *self);	/* rva 0x136f10 */
const char *Gog_GetLanguage(void);				/* rva 0x137140 */
void __thiscall Gog_Init(cGogListeners *self);			/* rva 0x137250 */
void __thiscall Gog_Done(cGogListeners *self);			/* rva 0x137430 */
void __thiscall Gog_ProcessData(cGogListeners *self);		/* rva 0x137570, galaxy::api::ProcessData */
void __thiscall PtrVector_Deallocate(void *self, void **p, int n);	/* rva 0x137c30 */
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
void Sound_ResetVolumes(void);					/* rva 0x1a2060 */
void Sound_Done(void);						/* rva 0x1a3150 */
void Sound_Init(void);						/* rva 0x1a3260 */
void Steam_Done(void);						/* rva 0x1a64d0 */
const char *Steam_GetLanguage(void);				/* rva 0x1a65f0 */
byte Steam_Init(int arg);					/* rva 0x1a6620 */
void Steam_RunCallbacks(void);					/* rva 0x1a66e0 */
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
void App_CheckExpiry(void);

#endif
