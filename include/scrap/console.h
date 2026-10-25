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
/* printf in the debug color, only shown above debug level 2 (rva 0xbc710) */
void Con_Debugf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
