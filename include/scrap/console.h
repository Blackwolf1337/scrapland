/*
 * The engine console and log (original source: Global/). Colors are 0xRRGGBB-like values of the engine; the
 * output goes to the console window, log.html and the debug output depending on the debug level.
 */
#ifndef SCRAP_CONSOLE_H
#define SCRAP_CONSOLE_H

#include "scrap/gvar.h"
#include "scrap/shared.h"

#define CON_COLOR_ERROR 0x8f
#define CON_COLOR_DEBUG 0x606060

/* the ring of the last console lines (src/game/Global/Console.c; drawn by Scorer_DrawConsole) */
#define CON_LINES 23
#define CON_LINE_LEN 0x80

typedef struct cConLine {
	char text[CON_LINE_LEN];
	uint32_t color;			/* 0xBBGGRR, set when the line ends */
} cConLine;

extern cConLine g_ConLines[23];	/* CON_LINES */
extern int g_ConLine;		/* the line written now */
extern cGVar GVar_debug;
#define g_DebugLevel (GVar_debug.value.i)

/* set while the test channel (TestChannel.c) runs a console line: gets a copy of every text the console shows */
extern void (*g_ConCapture)(const char *text);

/* prints text in a color; flag set: only into the second log */
void Con_Print(int color, const char *text, int flag);
/* printf in the error color */
void Con_Errorf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* printf in the warning color */
void Con_Warnf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* printf into the console lines, while the debug level is not 0 */
void Con_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* like Con_Printf in color 0xff, keeps GetLastError for the log, then Con_Fatal ends the game */
void Con_FatalErrorf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* like Con_Printf in color 0x707000, only above debug level 1 */
void Con_Verbosef(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* like Con_Printf in color 0xff */
void Con_Bluef(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* printf in color 0xRRGGBB */
void Con_ColorPrintf(uint32_t color, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
/* printf in the debug color, only shown above debug level 2 */
void Con_Debugf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
