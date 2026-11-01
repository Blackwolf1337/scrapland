/*
 * Console and log of the engine (Global/, the original file is not known). Every text goes to the on-screen
 * console, to log.html and logAI.html (HTML with a font color per text) and into a ring of the last 23 lines.
 * The debug level selects what is shown: 0 nothing, 1 errors, 2 also warnings and verbose texts, 3 also debug
 * texts. A failed CHECK shows a message box and ends the game.
 */
#include <stdarg.h>
#include <string.h>
#include <windows.h>

#include "scrap/gvar.h"
#include "scrap/console.h"
#include "scrap/game.h"
#include "scrap/win32.h"

extern void *g_LogFile;
extern void *g_LogAIFile;
extern int g_ConCol;
extern cGVar GVar_LogFile;
#define g_LogName (GVar_LogFile.value.s)
extern uint32_t g_LastError;
extern char g_ConBuf[1024];		/* 0x400, the text of the printf functions */
extern cGVar GVar_AlwaysFlushLog;
#define g_LogFlush (GVar_AlwaysFlushLog.value.i)		/* flush the logs after every text */

#define CON_COLOR_BLUE 0xff
#define CON_COLOR_VERBOSE 0x707000
#define CON_COLOR_RED 0xff0000
#define CON_COLOR_WARNING 0x8f008f
#define CON_COLOR_AI 0xffffff

void *File_OpenRel(const char *name, const char *mode);
void App_GetPath(char *buf, int size);
const char *Version_Get(void);
void ConsoleWin_Add(const char *text, uint32_t color);	/* the on-screen console, rva 0x20d550 */

/* one text into a log: HTML escaped, CR dropped, at most 511 characters */
void DebugLog_Write(const char *text, uint32_t color, void *f) {
	char buf[0x200];

	if (!f) {
		return;
	}
	char *p = buf;
	int i = 0;
	do {
		char c = *text;
		switch (c) {
		case '\r':
			i--;
			p--;
			break;
		case '&':
			if (i < 0x1fb) {
				memcpy(p, "&amp;", 6);
				i += 4;
				p += 4;
			}
			break;
		case '<':
			if (i < 0x1fc) {
				memcpy(p, "&lt;", 5);
				i += 3;
				p += 3;
			}
			break;
		case '>':
			if (i < 0x1fc) {
				memcpy(p, "&gt;", 5);
				i += 3;
				p += 3;
			}
			break;
		default:
			*p = c;
			break;
		}
		if (!c) {
			break;
		}
		i++;
		text++;
		p++;
	} while (i < 0x200);
	buf[sizeof(buf) - 1] = 0;
	/* the engine colors are 0xBBGGRR, HTML wants RRGGBB */
	crt_fprintf(f, "</font><font color=\"#%06X\">%s", (color >> 16 & 0xff) | (color & 0xff) << 16 | (color & 0xff00),
		buf);
	if (color == CON_COLOR_BLUE || color == CON_COLOR_ERROR || g_LogFlush) {
		crt_fflush(f);
	}
}

/* the text into the ring of console lines; a line takes up to 127 characters, more are cut */
static void Con_AddLines(const char *text, uint32_t color) {
	cConLine *lines = g_ConLines;

	lines[g_ConLine].text[g_ConCol] = 0;
	for (const char *nl = strchr(text, '\n'); nl; nl = strchr(text, '\n')) {
		int end = g_ConCol + (int)(nl - text);
		if (end > CON_LINE_LEN) {
			end = CON_LINE_LEN - 1;
		}
		lines[g_ConLine].color = color;
		Str_Copy(&lines[g_ConLine].text[g_ConCol], text, CON_LINE_LEN - g_ConCol);
		text = nl + 1;
		g_ConCol = 0;
		/* end 0x80 is the low byte of the color, like in the original */
		((char *)&lines[g_ConLine])[end] = 0;
		g_ConLine = (g_ConLine + 1) % CON_LINES;
	}
	char *rest = &lines[g_ConLine].text[g_ConCol];
	Str_Copy(rest, text, CON_LINE_LEN - g_ConCol);
	g_ConCol += (int)strlen(rest);
}

static void Con_Output(const char *text, uint32_t color) {
	ConsoleWin_Add(text, color);
	DebugLog_Write(text, color, g_LogFile);
	DebugLog_Write(text, color, g_LogAIFile);
	Con_AddLines(text, color);
}

/* printf into g_ConBuf and out in color */
static void Con_VOutput(uint32_t color, const char *fmt, va_list ap) {
	Str_VPrintf(g_ConBuf, 0x400, fmt, ap);
	Con_Output(g_ConBuf, color);
}

#define CON_PRINTF(name, color, cond) \
	void name(const char *fmt, ...) { \
		if (cond) { \
			va_list ap; \
			va_start(ap, fmt); \
			Con_VOutput(color, fmt, ap); \
			va_end(ap); \
		} \
	}

#define LEVEL g_DebugLevel
CON_PRINTF(Con_Printf, 0, LEVEL != 0)
CON_PRINTF(Con_Bluef, CON_COLOR_BLUE, LEVEL != 0)
CON_PRINTF(Con_Errorf, CON_COLOR_ERROR, LEVEL != 0)
CON_PRINTF(Con_Verbosef, CON_COLOR_VERBOSE, LEVEL != 0 && LEVEL != 1)
CON_PRINTF(Con_Redf, CON_COLOR_RED, LEVEL != 0 && LEVEL != 1)
CON_PRINTF(Con_Warnf, CON_COLOR_WARNING, LEVEL != 0 && LEVEL != 1)
CON_PRINTF(Con_Debugf, CON_COLOR_DEBUG, LEVEL != 0 && LEVEL != 1 && LEVEL != 2)

/* level 1 shows only errors and plain texts, level 2 all but debug texts */
static int Con_Shows(uint32_t color) {
	if (!LEVEL) {
		return 0;
	}
	if (LEVEL == 1) {
		return color == CON_COLOR_BLUE || color == CON_COLOR_ERROR || color == 0;
	}
	return LEVEL != 2 || color == CON_COLOR_WARNING || color != CON_COLOR_DEBUG;
}

/* flag: only into logAI.html */
void Con_Print(int color, const char *text, int flag) {
	if ((byte)flag) {
		DebugLog_Write(text, color, g_LogAIFile);
		return;
	}
	if (Con_Shows(color)) {
		Con_Output(text, color);
	}
}

void Con_ColorPrintf(uint32_t color, const char *fmt, ...) {
	if (!Con_Shows(color)) {
		return;
	}
	va_list ap;
	va_start(ap, fmt);
	Con_VOutput(color, fmt, ap);
	va_end(ap);
}

/* in blue, keeps GetLastError for the log */
void Con_FatalErrorf(const char *fmt, ...) {
	char buf[0x400];
	va_list ap;

	g_LastError = GetLastError();
	va_start(ap, fmt);
	Str_VPrintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (LEVEL) {
		Con_Output(buf, CON_COLOR_BLUE);
	}
	Con_Fatal(buf);
}

void DebugLog_Close(void *f) {
	if (f) {
		crt_fprintf(f, "</font><font color=\"#%06X\">%s", CON_COLOR_VERBOSE, "\nby Alien Duce...");
		crt_fputs("</font></p></pre></body></html>\n", f);
		crt_fclose(f);
	}
}

void DebugLog_CloseAll(void) {
	DebugLog_Close(g_LogFile);
	DebugLog_Close(g_LogAIFile);
	g_LogFile = NULL;
	g_LogAIFile = NULL;
}

void DebugLog_Open(const char *name, void **f) {
	char date[0x80];
	int32_t t;

	if (*f) {
		return;
	}
	*f = File_OpenRel(name, "wt");
	if (!*f) {
		return;
	}
	crt_fputs("<html><head><title>Scrapland debug log</title></head><body bgcolor=\"#C0C0C0\"><pre><p>"
		"<font color=\"0\">\n", *f);
	crt_fputs("-= SCRAPLAND log file =-\n", *f);
	crt_time32(&t);
	if (crt_strftime(date, sizeof(date), "%#c", crt_gmtime32(&t))) {
		crt_fprintf(*f, "(%s)\n\n", date);
	}
}

void DebugLog_Init(void) {
	DebugLog_Open(g_LogName, &g_LogFile);
	DebugLog_Open("logAI.html", &g_LogAIFile);
	int level = LEVEL;
	LEVEL = 1;
	Con_Printf("SCRAP Ver: %s\n(c) MercurySteam Entertainment\n\n", Version_Get());
	LEVEL = level;
	for (int i = 0; i < CON_LINES; i++) {
		g_ConLines[i].text[0] = 0;
	}
}

/* the message box of a fatal error; with a debugger attached it offers to break into it */
void Con_Fatal(const char *msg) {
	char text[0x420];
	BOOL(WINAPI * isDebugger)(void) = (BOOL(WINAPI *)(void))(void *)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),
		"IsDebuggerPresent");

	if (isDebugger && isDebugger()) {
		Str_PrintfQuiet(text, sizeof(text), "%s\n\nDebug the program?", msg);
		if (exe_MessageBoxA(NULL, text, "Scrap Fatal Error", 0x11114) != IDYES) {
			DebugLog_CloseAll();
			crt_exit_now(-1);
		}
		__asm__ volatile("int3");
		return;
	}
	Str_PrintfQuiet(text, sizeof(text), "%s\n\nPress the button to exit.", msg);
	exe_MessageBoxA(NULL, text, "Scrap Fatal Error", 0x11010);
	if (isDebugger && isDebugger()) {
		return;
	}
	DebugLog_CloseAll();
	crt_exit_now(-1);
}

void CheckFailed(const char *expr, const char *file, const char *date, int line) {
	char msg[0x400];
	char exe[0x80];

	g_LastError = GetLastError();
	LEVEL = 2;
	App_GetPath(exe, sizeof(exe));
	Str_PrintfQuiet(msg, sizeof(msg), "\nFATAL ERROR: Check failed!\n\nExpression: %s\n\nCompiled file info:\nFile: '%s' "
		"\nDate: %s  %s\n\nSource code info:\nFile: '%s'\nDate: %s\nLine: %d", expr, exe, "Nov 24 2021", "15:24:13",
		file, date, line);
	if (LEVEL) {
		Con_Output(msg, CON_COLOR_BLUE);
	}
	Con_Fatal(msg);
}
