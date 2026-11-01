/*
 * Engine/varios.cpp of the engine3d: the logs (engine.log and the console, by level bits), the debug console and
 * file and string helpers.
 */
#ifndef SCRAP_MISC_H
#define SCRAP_MISC_H

#include "scrap/game.h"

/* a log file, opened on the first line */
typedef struct cLog {
	void *f;			/* 0x0 FILE of the exe */
	byte failed;			/* 0x4 the open failed, not tried again */
} cLog;

/* level bits: each has its printer on the console; the masks say what goes to the console and to the file */
enum {
	LOG_PRINT = 0x01,
	LOG_DEBUG = 0x02,
	LOG_DEBUG2 = 0x04,
	LOG_VERBOSE = 0x08,
	LOG_BLUE = 0x10,
	LOG_WHITE = 0x20,		/* only from log level 2 */
	LOG_INFO = 0x40,		/* from log level 1 */
	LOG_ORANGE = 0x80,		/* from log level 3 */
};

#define g_LogFileMask GAME_GLOBAL(uint32_t, g_LogFileMask, 0x4ab8e8)
#define g_LogConMask GAME_GLOBAL(uint32_t, g_LogConMask, 0x431d4c)
#define g_LogLevel GAME_GLOBAL_IN(int, GVar_EngineDebugLevel, 16, 0x4ab90c)	/* 10: the console mask goes back to 0x91 */
#define LOG_LEVEL_RESET 10
#define LOG_CON_DEFAULT 0x91
/* the log of the engine ("FILE OPEN: ...") */
#define g_FileLog (GAME_ARRAY(cLog, g_FileLog, 0x4ab914))
/* the debug console: on, as a dialog window, as a text console */
#define g_DbgCon GAME_GLOBAL(int, g_DbgCon, 0x4ab8ec)
#define g_DbgConWindow GAME_GLOBAL(int, g_DbgConWindow, 0x4ab8f0)
#define g_DbgConStd GAME_GLOBAL(int, g_DbgConStd, 0x4ab8f4)
/* the handler of the lines typed into the debug dialog (E3D_Command), cdecl void(char *line) */
#define g_E3DCmdProc GAME_GLOBAL(void *, g_E3DCmdProc, 0x4ab8f8)

void __fastcall cLog_Open(cLog *self);
/* text to the console printers of the bits in mask and to the file */
void __thiscall cLog_Write(cLog *self, uint32_t mask, const char *text);
void E3D_LogPrint(cLog *log, const char *fmt, ...);
void E3D_LogDebug(cLog *log, const char *fmt, ...);
void E3D_LogDebug2(cLog *log, const char *fmt, ...);
void E3D_LogVerbose(cLog *log, const char *fmt, ...);
void E3D_LogBlue(cLog *log, const char *fmt, ...);
void E3D_LogWhite(cLog *log, const char *fmt, ...);
void E3D_LogInfo(cLog *log, const char *fmt, ...);
void E3D_Log(cLog *log, const char *fmt, ...);	/* orange */
/* to the debug console (rva 0x237950 window, 0x237830 text console) */
void E3D_DebugPrintf(const char *fmt, ...);
void DbgWin_Printf(const char *fmt, ...);
void DbgStd_Printf(const char *fmt, ...);
void DbgStd_Open(void *hwnd);
void DbgWin_AddLine(char *line, int len, int size);
void DbgWin_Open(void *hwnd);
/* the debug output of the ini settings (DbgConWindow, DbgConStd) */
void DbgWin_Init(void *hwnd);
void __fastcall cLog_dtor(cLog *self);
int Window_MoveRel(void *hwnd, const int32_t *rect, int x, int y, int w, int h);
void Window_GetNormalRect(void *hwnd, int32_t *rect);
/* the window procedure of the debug dialog: a list box, a command line and its button, resized with the dialog */
intptr_t __stdcall DbgWin_DlgProc(void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam);

/* where the relative paths start: the game directory with a "\\" at the end */
#define g_E3DDir (GAME_ARRAY(char, g_E3DDir, 0x4abe80))

/* a file of the packs or below the game directory */
byte File_Exists(const char *name);
/* the modification time, 0 if there is no file */
int File_Time(const char *name);
/* the time of a minus the time of b, def without one of them */
int File_TimeDiff(const char *a, const char *b, int def);
/* 1 with a warning when the time of name is more than tol away from saved (not while the packs are in use) */
byte File_TimeChanged(int saved, const char *name, int tol);
/* name renamed to name.bak (into the same directory with inDir) */
int File_Backup(const char *name, byte inDir);
/* Wild_Match on the name in lower case */
int Wild_MatchLower(const char *pat, const char *name);
/* a copy of the path (size -1: MAX_FILEPATH) */
char *Path_Copy(char *dest, const char *src, int size);
/* the path relative to the game directory without "." and "..", "\" as separator (size -1: MAX_FILEPATH) */
void Path_Normalize(char *dest, const char *path, int size);
/* what is between quotes at the start of src (after blanks), else all of src; NULL for nothing */
char *String_Unquote(char *dest, const char *src);
/* 1 / sqrt(x) from a table: the estimate and two Newton steps */
float RSqrt_Estimate(float x);
float RSqrt(float x);
/* SetWindowTextA of a VBprintf (1 KB): 1 when set */
int Win_SetTextf(void *hwnd, const char *fmt, ...);
/* the table of RSqrt_Estimate made (rva 0x238b10) */
void RSqrt_InitTable(void);
/* the path of the exe (GetModuleFileName) */
char *E3D_GetExePath(void);

#endif
