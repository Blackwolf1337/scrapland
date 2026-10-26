/*
 * The engine console and log (original source: Global/). Colors are 0xRRGGBB-like values of the engine; the
 * output goes to the console window, log.html and the debug output depending on the debug level (rva 0x445a74).
 */
#ifndef SCRAP_CONSOLE_H
#define SCRAP_CONSOLE_H

#include "scrap/shared.h"

#define CON_COLOR_ERROR 0x8f
#define CON_COLOR_DEBUG 0x606060

/* prints text in a color; flag set: only into the second log (rva 0xbca50) */
void Con_Print(int color, const char *text, int flag);
/* printf in the error color (rva 0xbc580) */
void Con_Errorf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* printf in the warning color (rva 0xbc8b0) */
void Con_Warnf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* printf into the console lines, while the debug level is not 0 (rva 0xbbef0) */
void Con_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* the "Scrap Fatal Error" message box, then the end of the process (rva 0xbbd80) */
void Con_Fatal(const char *msg);
/* like Con_Printf in color 0xff, keeps GetLastError for the log, then Con_Fatal ends the game (rva 0xbbba0) */
void Con_FatalErrorf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* like Con_Printf in color 0x707000, only above debug level 1 (rva 0xbc240) */
void Con_Verbosef(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* like Con_Printf in color 0xff (rva 0xbc0b0) */
void Con_Bluef(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* printf in color 0xRRGGBB (rva 0xbcbf0) */
void Con_ColorPrintf(uint32_t color, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
/* printf in the debug color, only shown above debug level 2 (rva 0xbc710) */
void Con_Debugf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
