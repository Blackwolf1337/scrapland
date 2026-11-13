/*
 * The console view (not in the original, which had a full screen console of 23 lines drawn by Scorer.c): the
 * console the way VisionJK and the other idTech 3 engines have it. The key next to 1 slides it down over the top
 * half of the screen; the scorer stays visible below. While it is open the platform layer hands it the keys and the
 * typed text (Plat_Key, Plat_Text) instead of the game, and the input set of the game is "Inactive" (Scorer_SetConsole).
 *
 * The input line: a cursor (Left, Right, Home, End, Ctrl with Left and Right by words), Backspace and Delete (with
 * Ctrl by words), Ctrl+U the line, Ctrl+K to the end, Ctrl+V paste, Ctrl+C copy the line. Up and Down walk the
 * history (kept in ConsoleHistory.txt next to Scrap.cfg). Tab completes (Console_Complete). Enter runs the line
 * (Console_Execute). PgUp and PgDn (with Ctrl or Shift faster) and the wheel scroll the text, Ctrl+Home and Ctrl+End
 * jump to its top and bottom, Ctrl+L clears it. Escape or the console key close the console.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/consoleview.h"
#include "scrap/crt.h"
#include "scrap/debugmenu.h"
#include "scrap/global.h"
#include "scrap/platform.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/scrapfont.h"
#include "scrap/scrapmain.h"
#include "scrap/world.h"

#define CV_LINE 256			/* the input line */
#define CV_HISTORY 64
#define CV_HISTORY_FILE "ConsoleHistory.txt"
#define CV_W 640.0f			/* the screen of the scorer */
#define CV_H 240.0f			/* the height of the open console */
#define CV_MENU_H 400.0f		/* with the debug menu */
#define CV_MARGIN 6.0f
#define CV_ROW 9.0f			/* the height of a text row */
#define CV_SLIDE_MS 150			/* the slide down */
#define CV_BLINK_MS 500
#define CV_WHEEL_ROWS 3
#define CV_PAGE_ROWS 6
#define CV_BG 0xf6080a0e		/* ARGB of the panel */
#define CV_EDGE 0xffd07818		/* the line under it */
#define CV_INPUT_BG 0x60000000
#define CV_TEXT 0xd8d8d8		/* RGB of the plain text (color 0) */
#define CV_INPUT 0xffffff
#define CV_HINT 0x707070
#define CV_SCROLL 0xffd040

static struct {
	char line[CV_LINE];
	int cursor;
	int back;			/* rows scrolled back */
	uint32_t openTime;		/* Plat_Ticks of the open */
	char hist[CV_HISTORY][CV_LINE];
	int nhist;			/* lines in hist, hist[0] the newest */
	int histPos;			/* -1: the line being typed; else the history line shown */
	char typed[CV_LINE];		/* the line being typed while the history is shown */
	byte histLoaded;
	byte menu;			/* the debug menu instead of the text (debug builds) */
	CPsaImage *white;
} cv = { .histPos = -1 };

byte ConsoleView_IsOpen(void) {
	return Scorer_ConsoleOn();
}

/* ---- the history ---- */

static void CV_HistoryPath(char *path, int size) {
	App_GetConfigDir(path, size);
	int n = (int)strlen(path);
	Str_Copy(path + n, CV_HISTORY_FILE, size - n);
}

static void CV_HistoryLoad(void) {
	char path[0x200], buf[CV_LINE];

	cv.histLoaded = 1;
	CV_HistoryPath(path, sizeof(path));
	void *f = crt_fopen(path, "rt");
	if (!f) {
		return;
	}
	/* the file has the oldest line first */
	while (cv.nhist < CV_HISTORY && crt_fgets(buf, sizeof(buf), f)) {
		buf[strcspn(buf, "\r\n")] = 0;
		if (!*buf) {
			continue;
		}
		memmove(cv.hist[1], cv.hist[0], sizeof(cv.hist[0]) * (size_t)(CV_HISTORY - 1));
		Str_Copy(cv.hist[0], buf, CV_LINE);
		cv.nhist++;
	}
	crt_fclose(f);
}

static void CV_HistorySave(void) {
	char path[0x200];

	CV_HistoryPath(path, sizeof(path));
	void *f = crt_fopen(path, "wt");
	if (!f) {
		return;
	}
	for (int i = cv.nhist - 1; i >= 0; i--) {
		crt_fputs(cv.hist[i], f);
		crt_fputs("\n", f);
	}
	crt_fclose(f);
}

static void CV_HistoryAdd(const char *line) {
	if (cv.nhist && !strcmp(cv.hist[0], line)) {
		return;
	}
	memmove(cv.hist[1], cv.hist[0], sizeof(cv.hist[0]) * (size_t)(CV_HISTORY - 1));
	Str_Copy(cv.hist[0], line, CV_LINE);
	if (cv.nhist < CV_HISTORY) {
		cv.nhist++;
	}
	CV_HistorySave();
}

static void CV_SetLine(const char *s) {
	Str_Copy(cv.line, s, CV_LINE);
	cv.cursor = (int)strlen(cv.line);
}

static void CV_History(int step) {
	int pos = cv.histPos + step;
	if (pos < -1 || pos >= cv.nhist) {
		return;
	}
	if (cv.histPos == -1) {
		Str_Copy(cv.typed, cv.line, CV_LINE);
	}
	cv.histPos = pos;
	CV_SetLine(pos == -1 ? cv.typed : cv.hist[pos]);
}

/* ---- open and close ---- */

void ConsoleView_Toggle(void) {
	cv.menu = 0;
	if (Scorer_ConsoleOn()) {
		Scorer_SetConsole(0);
		return;
	}
	Scorer_SetConsole(1);
	if (!Scorer_ConsoleOn()) {
		return;
	}
	if (!cv.histLoaded) {
		CV_HistoryLoad();
	}
	cv.openTime = Plat_Ticks();
	cv.back = 0;
}

void ConsoleView_ToggleMenu(void) {
#ifdef SCRAP_DEBUG_MENU
	if (Scorer_ConsoleOn() && cv.menu) {
		cv.menu = 0;
		return;
	}
	if (!Scorer_ConsoleOn()) {
		ConsoleView_Toggle();
		if (!Scorer_ConsoleOn()) {
			return;
		}
	}
	cv.menu = 1;
	DebugMenu_Open();
#else
	Con_Echo(CON_COLOR_ERROR, "The debug menu is in debug builds only (CMake option SCRAP_DEBUG_MENU)\n");
#endif
}

/* ---- the input line ---- */

static byte CV_IsWord(char c) {
	return c && c != ' ' && c != ',' && c != '.' && c != '(' && c != ')' && c != '"';
}

static int CV_WordLeft(int i) {
	while (i > 0 && !CV_IsWord(cv.line[i - 1])) {
		i--;
	}
	while (i > 0 && CV_IsWord(cv.line[i - 1])) {
		i--;
	}
	return i;
}

static int CV_WordRight(int i) {
	int n = (int)strlen(cv.line);
	while (i < n && !CV_IsWord(cv.line[i])) {
		i++;
	}
	while (i < n && CV_IsWord(cv.line[i])) {
		i++;
	}
	return i;
}

/* the characters from a up to b out of the line, the cursor at a */
static void CV_Cut(int a, int b) {
	if (b <= a) {
		return;
	}
	memmove(cv.line + a, cv.line + b, strlen(cv.line + b) + 1);
	cv.cursor = a;
}

static void CV_Insert(const char *s) {
	int n = (int)strlen(cv.line), k = (int)strlen(s);
	if (n + k >= CV_LINE) {
		k = CV_LINE - 1 - n;
	}
	if (k <= 0) {
		return;
	}
	memmove(cv.line + cv.cursor + k, cv.line + cv.cursor, (size_t)(n - cv.cursor + 1));
	memcpy(cv.line + cv.cursor, s, (size_t)k);
	cv.cursor += k;
}

static void CV_Run(void) {
	char line[CV_LINE + 4];

	Str_PrintfQuiet(line, sizeof(line), "]%s\n", cv.line);
	Con_Echo(0, line);
	Str_Copy(line, cv.line, sizeof(line));
	Str_TrimBlanks(line);
	cv.histPos = -1;
	cv.back = 0;
	CV_SetLine("");
	if (*line) {
		CV_HistoryAdd(line);
		Console_Execute(line);
	}
}

void ConsoleView_Key(int key, int mods) {
	byte ctrl = (mods & CVK_CTRL) != 0;
	int n = (int)strlen(cv.line);
	int page = ctrl || (mods & CVK_SHIFT) ? CV_PAGE_ROWS * 4 : CV_PAGE_ROWS;

	if (key == CVK_ESC) {
		Scorer_SetConsole(0);
		return;
	}
#ifdef SCRAP_DEBUG_MENU
	if (cv.menu) {
		if (!DebugMenu_Key(key, mods)) {
			/* something started (or Backspace on the first page): the menu closes */
			cv.menu = 0;
			if (key != CVK_BACKSPACE) {
				Scorer_SetConsole(0);
			}
		}
		return;
	}
#endif
	switch (key) {
	case CVK_ENTER:
		CV_Run();
		return;
	case CVK_TAB:
		Console_Complete(cv.line, CV_LINE, &cv.cursor);
		cv.back = 0;
		return;
	case CVK_BACKSPACE:
		CV_Cut(ctrl ? CV_WordLeft(cv.cursor) : cv.cursor > 0 ? cv.cursor - 1 : 0, cv.cursor);
		return;
	case CVK_DELETE:
		CV_Cut(cv.cursor, ctrl ? CV_WordRight(cv.cursor) : cv.cursor < n ? cv.cursor + 1 : n);
		return;
	case CVK_LEFT:
		cv.cursor = ctrl ? CV_WordLeft(cv.cursor) : cv.cursor > 0 ? cv.cursor - 1 : 0;
		return;
	case CVK_RIGHT:
		cv.cursor = ctrl ? CV_WordRight(cv.cursor) : cv.cursor < n ? cv.cursor + 1 : n;
		return;
	case CVK_HOME:
		if (ctrl) {
			cv.back = Con_BackCount() * 4;
		} else {
			cv.cursor = 0;
		}
		return;
	case CVK_END:
		if (ctrl) {
			cv.back = 0;
		} else {
			cv.cursor = n;
		}
		return;
	case CVK_UP:
		CV_History(1);
		return;
	case CVK_DOWN:
		CV_History(-1);
		return;
	case CVK_PGUP:
		cv.back += page;
		return;
	case CVK_PGDN:
		cv.back = cv.back > page ? cv.back - page : 0;
		return;
	}
	if (!ctrl) {
		return;
	}
	switch (key) {
	case 'A':
		cv.cursor = 0;
		break;
	case 'E':
		cv.cursor = n;
		break;
	case 'U':
		CV_Cut(0, cv.cursor);
		break;
	case 'K':
		cv.line[cv.cursor] = 0;
		break;
	case 'W':
		CV_Cut(CV_WordLeft(cv.cursor), cv.cursor);
		break;
	case 'L':
		Con_BackClear();
		cv.back = 0;
		break;
	case 'P':
		CV_History(1);
		break;
	case 'N':
		CV_History(-1);
		break;
	case 'C':
		Plat_ClipboardSet(cv.line);
		break;
	case 'V': {
		char buf[CV_LINE];
		Plat_ClipboardGet(buf, sizeof(buf));
		/* one line: the first line of the clipboard */
		buf[strcspn(buf, "\r\n")] = 0;
		CV_Insert(buf);
		break;
	}
	}
}

void ConsoleView_Char(int c) {
	char s[2] = { (char)c, 0 };
	if ((byte)c < 0x20 || c == 0x7f) {
		return;
	}
#ifdef SCRAP_DEBUG_MENU
	if (cv.menu) {
		DebugMenu_Char(c);
		return;
	}
#endif
	CV_Insert(s);
}

void ConsoleView_Wheel(int notches) {
#ifdef SCRAP_DEBUG_MENU
	if (cv.menu) {
		DebugMenu_Wheel(notches);
		return;
	}
#endif
	cv.back += notches * CV_WHEEL_ROWS;
	if (cv.back < 0) {
		cv.back = 0;
	}
}

/* ---- drawing ---- */

/* an engine color (0xBBGGRR, 0 the plain text) as RGB, a bit lighter for the dark panel */
static uint32_t CV_Color(uint32_t c) {
	if (!c) {
		return CV_TEXT;
	}
	uint32_t r = c & 0xff, g = (c >> 8) & 0xff, b = (c >> 16) & 0xff;
	r += (255 - r) * 2 / 5;
	g += (255 - g) * 2 / 5;
	b += (255 - b) * 2 / 5;
	return r << 16 | g << 8 | b;
}

static void CV_Rect(float x, float y, float w, float h, uint32_t argb) {
	cv.white->color = argb;
	cv.white->scaleX = w;
	cv.white->scaleY = h;
	CPsaImage_Draw(cv.white, x, y);
}

static CFont *CV_Font(void) {
	CFont *f = Font_Get("Terminal8Fixed");
	return f ? f : g_FontTerminal8;
}

void ConsoleView_Rect(float x, float y, float w, float h, uint32_t argb) {
	if (cv.white) {
		CV_Rect(x, y, w, h, argb);
	}
}

void ConsoleView_Text(float x, float y, uint32_t rgb, const char *text) {
	CFont_PrintfColor(CV_Font(), x, y, rgb, "%s", text);
}

/* the advance of a character of the fixed font in pixels of the scorer */
static float CV_CharWidth(CFont *f) {
	float pos[2], dim[2];
	Font_TextSize(f, pos, dim, 0.0f, 0.0f, "%s", "MMMMMMMMMMMMMMMMMMMM");
	return dim[0] > 0.0f ? dim[0] / 20.0f : 6.0f;
}

void ConsoleView_Draw(void) {
	char buf[CV_LINE + 2];

#ifdef SCRAP_DEBUG_MENU
	DebugMenu_Frame();
#endif
	if (!Scorer_ConsoleOn()) {
		cv.menu = 0;
		return;
	}
	if (!cv.white) {
		cv.white = operator_new(sizeof(CPsaImage));
		if (!cv.white || !CPsaImage_ctor(cv.white, "2D/white.tga", 0x24)) {
			cv.white = NULL;
			return;
		}
	}
	CFont *f = CV_Font();
	uint32_t now = Plat_Ticks();
	uint32_t t = now - cv.openTime;
	float frac = t >= CV_SLIDE_MS ? 1.0f : (float)t / (float)CV_SLIDE_MS;
	float full = cv.menu ? CV_MENU_H : CV_H;
	float h = full * frac;
	float top = h - full;			/* the panel slides in from above */
	float cw = CV_CharWidth(f);
	int cols = (int)((CV_W - 2.0f * CV_MARGIN) / cw);
	if (cols < 10) {
		cols = 10;
	}

	CV_Rect(0.0f, top, CV_W, full, CV_BG);
	CV_Rect(0.0f, h - 1.0f, CV_W, 1.0f, CV_EDGE);
#ifdef SCRAP_DEBUG_MENU
	if (cv.menu) {
		DebugMenu_Draw(top, h);
		return;
	}
#endif

	/* the input line at the bottom: "]" and the part of the line around the cursor */
	float yIn = h - CV_ROW - 4.0f;
	CV_Rect(0.0f, yIn - 2.0f, CV_W, CV_ROW + 3.0f, CV_INPUT_BG);
	int room = cols - 2;
	int first = cv.cursor > room ? cv.cursor - room : 0;
	Str_PrintfQuiet(buf, sizeof(buf), "]%.*s", room, cv.line + first);
	CFont_PrintfColor(f, CV_MARGIN, yIn, CV_INPUT, "%s", buf);
	if ((now / CV_BLINK_MS) % 2 == 0) {
		CFont_PrintfColor(f, CV_MARGIN + (float)(cv.cursor - first + 1) * cw, yIn + 1.0f, CV_INPUT, "%s", "_");
	}
	CFont_PrintfColor(f, CV_W - CV_MARGIN - 6.0f * cw, yIn - CV_ROW - 2.0f, CV_HINT, "%s", "ScrapC");

	/* the text, the newest line at the bottom, long lines wrapped */
	float y = yIn - CV_ROW - 4.0f;
	int skip = cv.back;
	if (skip > 0) {
		/* scrolled back: the row above the input says so */
		char marks[CV_LINE];
		int k = 0;
		for (int i = 0; i < cols && k < (int)sizeof(marks) - 1; i++) {
			marks[k++] = i % 8 == 0 ? '^' : ' ';
		}
		marks[k] = 0;
		CFont_PrintfColor(f, CV_MARGIN, y, CV_SCROLL, "%s", marks);
		y -= CV_ROW;
	}
	for (int i = 0; i < Con_BackCount() && y > top + CV_MARGIN - CV_ROW; i++) {
		uint32_t color;
		const char *text = Con_BackLine(i, &color);
		int tl = (int)strlen(text);
		int rows = tl ? (tl + cols - 1) / cols : 1;
		/* the rows of the line from its last one up */
		for (int r = rows - 1; r >= 0 && y > top + CV_MARGIN - CV_ROW; r--) {
			if (skip > 0) {
				skip--;
				continue;
			}
			Str_PrintfQuiet(buf, sizeof(buf), "%.*s", cols, text + r * cols);
			CFont_PrintfColor(f, CV_MARGIN, y, CV_Color(color), "%s", buf);
			y -= CV_ROW;
		}
	}
	/* no scrolling past the oldest line */
	if (skip > 0 && cv.back > 0) {
		cv.back -= skip;
	}
}

void ConsoleView_Closed(void) {
	if (cv.white) {
		CPsaImage_dtor(cv.white);
		operator_delete(cv.white);
		cv.white = NULL;
	}
}
