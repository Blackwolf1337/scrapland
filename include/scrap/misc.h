/*
 * Engine/varios.cpp of the engine3d: the logs (engine.log and the console, by level bits), the debug console and
 * file and string helpers.
 */
#ifndef SCRAP_MISC_H
#define SCRAP_MISC_H

#include "scrap/gvar.h"
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

#define LOG_LEVEL_RESET 10
#define LOG_CON_DEFAULT 0x91
/* the log of the engine ("FILE OPEN: ...") */
extern cLog g_FileLog;
extern int g_DbgConWindow;
extern int g_DbgConStd;

void E3D_LogPrint(cLog *log, const char *fmt, ...);
void E3D_LogDebug(cLog *log, const char *fmt, ...);
void E3D_LogDebug2(cLog *log, const char *fmt, ...);
void E3D_LogVerbose(cLog *log, const char *fmt, ...);
void E3D_LogBlue(cLog *log, const char *fmt, ...);
void E3D_LogWhite(cLog *log, const char *fmt, ...);
void E3D_LogInfo(cLog *log, const char *fmt, ...);
void E3D_Log(cLog *log, const char *fmt, ...);	/* orange */
/* to the debug console: stdout (the original had a dialog with a list box and a text console) */
void E3D_DebugPrintf(const char *fmt, ...);
/* the debug output on with the ini settings ConsolaWnd or ConsolaTxt */
void DbgCon_Init(void);

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
/* 1 / sqrt(x) from a table: the estimate and two Newton steps */
float RSqrt_Estimate(float x);
/* the path of the exe (GetModuleFileName) */
char *E3D_GetExePath(void);

#endif
