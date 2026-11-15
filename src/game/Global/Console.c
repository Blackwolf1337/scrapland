/*
 * Console and log of the engine (Global/, the original file is not known). Every text goes to the on-screen
 * console, to log.html and logAI.html (HTML with a font color per text) and into a ring of the last 23 lines.
 * The debug level selects what is shown: 0 nothing, 1 errors, 2 also warnings and verbose texts, 3 also debug
 * texts. A failed CHECK shows a message box and ends the game.
 */
#include <stdarg.h>
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/console.h"
#include "scrap/game.h"
#include "scrap/platform.h"
#include "scrap/staticinit.h"

cGVar GVar_LogFile;

#define g_LogName (GVar_LogFile.value.s)
static cGVar GVar_AlwaysFlushLog;
#define g_LogFlush (GVar_AlwaysFlushLog.value.i)		/* flush the logs after every text */

#define CON_COLOR_BLUE 0xff
#define CON_COLOR_VERBOSE 0x707000
#define CON_COLOR_RED 0xff0000
#define CON_COLOR_WARNING 0x8f008f
#define CON_COLOR_AI 0xffffff

void *File_OpenRel(const char *name, const char *mode);
void App_GetPath(char *buf, int size);
const char *Version_Get(void);
void ConsoleWin_Add(const char *text, uint32_t color);	/* the on-screen console */

/* one text into a log: HTML escaped, CR dropped, at most 511 characters */
static void DebugLog_Write(const char *text, uint32_t color, void *f) {
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

cConLine g_ConLines[23];
int g_ConLine;
static int g_ConCol;

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

/* the scrollback of the console view (src/game/Scorer/ConsoleView.c): not in the original, which kept only the 23
 * lines above. A ring of whole lines; the newest is the one being written (no newline yet) */
#define CON_BACK_LINES 2048
#define CON_BACK_LEN 256

static struct {
	char text[CON_BACK_LEN];
	uint32_t color;
} g_ConBack[CON_BACK_LINES];
static int g_ConBackHead;	/* the line written now */
static int g_ConBackCount = 1;	/* lines in the ring, the one written now with them */
static int g_ConBackCol;

static void Con_BackAdd(const char *text, uint32_t color) {
	for (; *text; text++) {
		if (*text == '\n') {
			g_ConBack[g_ConBackHead].text[g_ConBackCol] = 0;
			g_ConBackHead = (g_ConBackHead + 1) % CON_BACK_LINES;
			g_ConBack[g_ConBackHead].text[0] = 0;
			g_ConBackCol = 0;
			if (g_ConBackCount < CON_BACK_LINES) {
				g_ConBackCount++;
			}
			continue;
		}
		if (*text == '\r') {
			continue;
		}
		if (!g_ConBackCol) {
			g_ConBack[g_ConBackHead].color = color;
		}
		if (g_ConBackCol < CON_BACK_LEN - 1) {
			g_ConBack[g_ConBackHead].text[g_ConBackCol++] = *text;
			g_ConBack[g_ConBackHead].text[g_ConBackCol] = 0;
		}
	}
}

int Con_BackCount(void) {
	return g_ConBackCount;
}

const char *Con_BackLine(int fromNewest, uint32_t *color) {
	if (fromNewest < 0 || fromNewest >= g_ConBackCount) {
		*color = 0;
		return "";
	}
	int i = (g_ConBackHead - fromNewest + CON_BACK_LINES) % CON_BACK_LINES;
	*color = g_ConBack[i].color;
	return g_ConBack[i].text;
}

void Con_BackClear(void) {
	g_ConBackHead = 0;
	g_ConBackCount = 1;
	g_ConBackCol = 0;
	g_ConBack[0].text[0] = 0;
}

static void *g_LogFile;
static void *g_LogAIFile;

void (*g_ConCapture)(const char *text);

static void Con_Output(const char *text, uint32_t color) {
	if (g_ConCapture) {
		g_ConCapture(text);
	}
	ConsoleWin_Add(text, color);
	DebugLog_Write(text, color, g_LogFile);
	DebugLog_Write(text, color, g_LogAIFile);
	Con_AddLines(text, color);
	Con_BackAdd(text, color);
}

static char g_ConBuf[1024];

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
static uint32_t g_LastError;

static void Con_Fatal(const char *msg);
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

/* the output of the console view and its commands: shown at any debug level above 0, in any color */
void Con_Echo(uint32_t color, const char *text) {
	if (LEVEL) {
		Con_Output(text, color);
	}
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

	g_LastError = Plat_LastError();
	va_start(ap, fmt);
	Str_VPrintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (LEVEL) {
		Con_Output(buf, CON_COLOR_BLUE);
	}
	Con_Fatal(buf);
}

static void DebugLog_Close(void *f) {
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

static void DebugLog_Open(const char *name, void **f) {
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
static void Con_Fatal(const char *msg) {
	char text[0x420];
	byte debugger = Plat_DebuggerPresent();

	if (debugger) {
		Str_PrintfQuiet(text, sizeof(text), "%s\n\nDebug the program?", msg);
		if (Plat_MessageBox(text, "Scrap Fatal Error", 0x11114) != 6) {
			DebugLog_CloseAll();
			crt_exit_now(-1);
		}
		Plat_DebugBreak();
		return;
	}
	Str_PrintfQuiet(text, sizeof(text), "%s\n\nPress the button to exit.", msg);
	Plat_MessageBox(text, "Scrap Fatal Error", 0x11010);
	if (debugger) {
		return;
	}
	DebugLog_CloseAll();
	crt_exit_now(-1);
}

void CheckFailed(const char *expr, const char *file, const char *date, int line) {
	char msg[0x400];
	char exe[0x80];

	g_LastError = Plat_LastError();
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

cGVar GVar_debug;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_debug, &GVar_debug, "debug", 4, "0 = Nothing, 1 = Something, 2 = Everything, 3 = Everything and much more", 1)
FINI_GVAR_STRING(Fini_GVar_LogFile, &GVar_LogFile)
INIT_GVAR_STRING(Init_GVar_LogFile, Fini_GVar_LogFile, &GVar_LogFile, "LogFile", 1, "Log File to write the console debug log. ONLY works as argument in command line.", "log.html")
INIT_GVAR_INT(Init_GVar_AlwaysFlushLog, &GVar_AlwaysFlushLog, "AlwaysFlushLog", 0, "Always flush log file (0/1)", 0)
