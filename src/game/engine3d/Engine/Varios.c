/*
 * Logs and helpers of the engine3d, original source Engine/varios.cpp (include/scrap/varios.h). The dialog procedure
 * of the debug console stays in the exe.
 */
#include <stdarg.h>
#include <string.h>

#include "scrap/archio.h"
#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/filepak.h"
#include "scrap/nodefx.h"
#include "scrap/win32.h"

#define LOG_LINE 0x400
#define LOG_NAME "engine.log"
#define CRT_LC_TIME 5
#define LOG_TIME 0x80

/* the st_mtime of a struct _stat32 */
#define STAT32_SIZE 0x24
#define STAT32_MTIME 0x1c

void __fastcall cLog_Open(cLog *self) {
	char path[MAX_RUTA];
	char date[LOG_TIME];
	int32_t t;

	if (self->f || self->failed) {
		return;
	}
	self->f = File_OpenRel(Ruta_Copy(path, LOG_NAME, -1), "wb");
	if (!self->f) {
		self->failed = 1;
		return;
	}
	crt_time32(&t);
	void *tm = crt_gmtime32(&t);
	crt_setlocale(CRT_LC_TIME, "");
	if (crt_strftime(date, sizeof(date), "%#c", tm)) {
		crt_fprintf(self->f, "\r\n(%s)", date);
	}
}

/* the end of the log file: a line end and closed */
void __fastcall cLog_dtor(cLog *self) {
	if (self->f) {
		crt_fprintf(self->f, "\r\n");
		if (self->f) {
			crt_fclose(self->f);
			self->f = NULL;
		}
	}
}

void __fastcall cLog_Close(cLog *self) {
	if (self->f) {
		crt_fclose(self->f);
		self->f = NULL;
	}
}

void __thiscall cLog_Write(cLog *self, uint32_t mask, const char *text) {
	if (g_LogLevel == LOG_LEVEL_RESET) {
		g_LogConMask = LOG_CON_DEFAULT;
	}
	if ((mask & g_LogConMask) && text) {
		if (mask & LOG_PRINT) {
			Con_Printf("%s\n", text);
		}
		if (mask & LOG_DEBUG) {
			Con_Debugf("%s\n", text);
		}
		if (mask & LOG_DEBUG2) {
			Con_Debugf("%s\n", text);
		}
		if (mask & LOG_VERBOSE) {
			Con_Verbosef("%s\n", text);
		}
		if (mask & LOG_BLUE) {
			Con_Bluef("%s\n", text);
		}
		if (mask & LOG_WHITE) {
			Con_ColorPrintf(0xffffff, "%s\n", text);
		}
		if (mask & LOG_INFO) {
			Con_ColorPrintf(0xc0ffff, "%s\n", text);
		}
		if (mask & LOG_ORANGE) {
			Con_ColorPrintf(0xff8000, "%s\n", text);
		}
	}
	if ((g_LogFileMask & mask) && text) {
		cLog_Open(self);
		if (self->f) {
			crt_fprintf(self->f, "\r\n%s", text);
			crt_fflush(self->f);
		}
	}
}

/* the E3D_Log functions: one level bit each */
static void E3D_LogV(cLog *log, uint32_t bit, const char *fmt, va_list ap) {
	char buf[LOG_LINE];

	if (!((g_LogFileMask | g_LogConMask) & bit) || !fmt) {
		return;
	}
	VBprintf(buf, sizeof(buf), fmt, ap);
	cLog_Write(log, bit, buf);
}

#define E3D_LOG(name, bit, minLevel) \
	void name(cLog *log, const char *fmt, ...) { \
		va_list ap; \
		if (g_LogLevel < (minLevel)) { \
			return; \
		} \
		va_start(ap, fmt); \
		E3D_LogV(log, bit, fmt, ap); \
		va_end(ap); \
	}
/* INT32_MIN: at any log level */
E3D_LOG(E3D_LogPrint, LOG_PRINT, INT32_MIN)
E3D_LOG(E3D_LogDebug, LOG_DEBUG, INT32_MIN)
E3D_LOG(E3D_LogDebug2, LOG_DEBUG2, INT32_MIN)
E3D_LOG(E3D_LogVerbose, LOG_VERBOSE, INT32_MIN)
E3D_LOG(E3D_LogBlue, LOG_BLUE, INT32_MIN)
E3D_LOG(E3D_LogWhite, LOG_WHITE, 2)
E3D_LOG(E3D_LogInfo, LOG_INFO, 1)
E3D_LOG(E3D_Log, LOG_ORANGE, 3)

void E3D_DebugPrintf(const char *fmt, ...) {
	char buf[LOG_LINE];
	va_list ap;

	if (!g_DbgCon || !fmt) {
		return;
	}
	va_start(ap, fmt);
	VBprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (g_DbgConWindow) {
		DbgWin_Printf("%s", buf);
	}
	if (g_DbgConStd) {
		DbgStd_Printf("%s", buf);
	}
}

byte File_Exists(const char *name) {
	return File_AccessPack(name, 0) == 0;
}

static int File_MTime(const char *name, int def) {
	byte st[STAT32_SIZE];

	if (File_StatPack(name, st)) {
		return def;
	}
	return *(int32_t *)(st + STAT32_MTIME);
}

int File_Time(const char *name) {
	return File_MTime(name, 0);
}

int File_TimeDiff(const char *a, const char *b, int def) {
	byte sa[STAT32_SIZE], sb[STAT32_SIZE];

	if (File_StatPack(a, sa) || File_StatPack(b, sb)) {
		return def;
	}
	return *(int32_t *)(sa + STAT32_MTIME) - *(int32_t *)(sb + STAT32_MTIME);
}

byte File_TimeChanged(int saved, const char *name, int tol) {
	if (PackLog_Closed()) {
		return 0;
	}
	int now = File_Time(name);
	int diff = now - saved;
	/* the abs of the exe: INT_MIN stays negative */
	uint32_t ad = diff < 0 ? 0u - (uint32_t)diff : (uint32_t)diff;
	if ((int)ad <= tol) {
		return 0;
	}
	E3D_LogVerbose(g_FileLog, "WARN: FileTime changed: %s (Saved:%d / Now:%d / Diff:%d)", name, saved, now, diff);
	return 1;
}

int File_Backup(const char *name, byte inDir) {
	char bak[2 * MAX_RUTA];

	if (inDir) {
		Ruta_Merge(bak, ".bak", name);
	} else {
		Bprintf(bak, sizeof(bak), "%s.bak", name);
	}
	crt_remove(bak);
	crt_rename(name, bak);
	return 0;
}

int Wild_MatchLower(const char *pat, const char *name) {
	char low[2 * MAX_RUTA];

	Cad_Copy(low, name, sizeof(low));
	crt_strlwr(low);
	return Wild_Match(pat, low);
}

char *Ruta_Copy(char *dest, const char *src, int size) {
	if (size == -1) {
		size = MAX_RUTA;
	}
	CHECK(dest && size > 0);
	if (dest != src) {
		Cad_Copy(dest, src, size);
	}
	return dest;
}

void Ruta_Normalize(char *dest, const char *path, int size) {
	char drive[4], dir[0x100];
	char b[2 * MAX_RUTA];

	if (size == -1) {
		size = MAX_RUTA;
	}
	CHECK(dest && size > 0);
	crt_splitpath(path, drive, dir, NULL, NULL);
	if (drive[0] || Ruta_IsSep(dir[0])) {
		if (dest != path) {
			Cad_Copy(dest, path, size);
		}
		return;
	}
	Bprintf(b, sizeof(b), "%s%s", g_E3DDir, path);
	/* in place: r reads, w writes; "." and ".." after a separator (or at the start, after a drive) are resolved */
	int r = 0, w = 0;
	for (;;) {
		char c = b[r++];
		if (c == '.') {
			int n = 1;
			while (b[r + n - 1] == '.') {
				n++;
			}
			char prev = w > 0 ? b[w - 1] : '\0';
			char next = b[r + n - 1];
			if ((Ruta_IsSep(prev) || (n == 1 && (prev == '\0' || prev == ':'))) &&
				(next == '\0' || Ruta_IsSep(next))) {
				r += n - 1 + Ruta_IsSep(next);
				for (int i = 1; i < n; i++) {
					if (w > 1 && Ruta_IsSep(b[w - 1]) && b[w - 2] != ':') {
						for (w--; w > 0 && b[w - 1] != ':' && !Ruta_IsSep(b[w - 1]); w--) {
						}
					}
				}
				continue;
			}
		} else if (c == '/') {
			c = '\\';
		}
		b[w++] = c;
		if (!c) {
			break;
		}
	}
	Cad_Copy(dest, b, size);
}

char *Cad_Unquote(char *dest, const char *src) {
	if (!dest) {
		return NULL;
	}
	*dest = '\0';
	if (!src) {
		return NULL;
	}
	const char *p = src;
	while (*p == '\t' || *p == ' ') {
		p++;
	}
	if (!*p) {
		return NULL;
	}
	if (*p == '"') {
		const char *s = ++p;
		while (*p && *p != '"') {
			p++;
		}
		Cad_Copy(dest, s, p - s + 1);
		return dest;
	}
	/* the whole string, blanks before the word too */
	size_t i = 0;
	do {
		dest[i] = src[i];
	} while (src[i++]);
	return dest;
}

#define g_RSqrtTable ((const byte *)Game_Ptr(0x4abf88))
#define g_ExePath ((char *)Game_Ptr(0x4abd78))	/* GetModuleFileName at the start, 0x103 */

char *E3D_GetExePath(void) {
	return g_ExePath;
}

/* the bits 15..22 (rounded) of 1 / sqrt of the 128 mantissa steps of the exponents 0x7f (0..63) and 0x7e */
void RSqrt_InitTable(void) {
	byte *t = (byte *)g_RSqrtTable;
	int i;

	for (i = 0; i < 0x80; i++) {
		uint32_t u = (uint32_t)(i | 0x1f80) << 17, r;
		float f, inv;

		memcpy(&f, &u, sizeof(f));
		inv = (float)(1.0 / __builtin_sqrt((double)f));
		memcpy(&r, &inv, sizeof(r));
		t[i] = (byte)((r + 0x2000) >> 15);
	}
	t[0x40] = 0xff;
}

float RSqrt_Estimate(float x) {
	uint32_t u, r;
	float f;

	memcpy(&u, &x, sizeof(u));
	r = ((0x17c - (u >> 23 & 0xff)) & 0x3fe) << 22 | (uint32_t)g_RSqrtTable[u >> 17 & 0x7f] << 15;
	memcpy(&f, &r, sizeof(f));
	return f;
}

float Ang_Wrap(float a) {
	if (a > M_2PI_F) {
		a = a - M_2PI_F;
	}
	if (-M_2PI_F > a) {
		a = a + M_2PI_F;
	}
	return a;
}

float RSqrt(float x) {
	float y = RSqrt_Estimate(x);

	y = (3.0f - y * y * x) * y * 0.5f;
	return (3.0f - y * y * x) * y * 0.5f;
}

/* ---- the debug consoles: a list box in a dialog and a text console ---- */

#define g_DbgStdOpen GAME_DATA(byte, 0x4ab91c)
#define g_DbgWinOpen GAME_DATA(byte, 0x4ab91d)
#define g_DbgWnd GAME_DATA(void *, 0x4ab920)
#define g_DbgList GAME_DATA(void *, 0x4ab924)	/* the list box of the dialog */
#define g_DbgLine ((char *)Game_Ptr(0x4ab970))	/* 0x400, the line being written */
#define g_DbgLinePos GAME_DATA(int, 0x4abd70)
#define g_DbgLineLen GAME_DATA(int, 0x4abd74)
#define g_DbgCharMap ((const char *)Game_Ptr(0x431c70))	/* the characters from 0xe1 for the console */
#define DBGWIN_DIALOG 0x9a
#define DBGWIN_PROC 0x2373d0
#define DBGWIN_LINE 0x400
#define LB_ADDSTRING 0x180
#define LB_DELETESTRING 0x182
#define LB_SETCURSEL 0x186
#define LB_GETCOUNT 0x18b
#define GWL_HINSTANCE (-6)

/* the window moved to x, y and sized w, h relative to rect (left, top, right, bottom) */
int Window_MoveRel(void *hwnd, const int32_t *rect, int x, int y, int w, int h) {
	return exe_MoveWindow(hwnd, x + rect[0], y + rect[1], rect[2] - rect[0] + w, rect[3] - rect[1] + h, 1);
}

void DbgStd_Printf(const char *fmt, ...) {
	char buf[0x400];
	uint32_t written, n;
	va_list ap;

	if (!g_DbgCon || !g_DbgStdOpen || !fmt || exe_GetStdHandle((uint32_t)STD_OUTPUT) == (void *)-1) {
		return;
	}
	va_start(ap, fmt);
	VBprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	for (n = 0; buf[n]; n++) {
		if ((byte)buf[n] >= 0xe1) {
			buf[n] = g_DbgCharMap[(byte)buf[n]];
		}
	}
	exe_WriteConsoleA(exe_GetStdHandle((uint32_t)STD_OUTPUT), buf, n, &written, NULL);
}

/* the text console made, black on gray */
void DbgStd_Open(void *hwnd) {
	uint32_t written;

	(void)hwnd;
	if (!g_DbgCon || g_DbgStdOpen) {
		return;
	}
	g_DbgStdOpen = 1;
	exe_AllocConsole();
	exe_SetConsoleTextAttribute(exe_GetStdHandle((uint32_t)STD_OUTPUT), 0x70);
	exe_FillConsoleOutputAttribute(exe_GetStdHandle((uint32_t)STD_OUTPUT), 0x70, 24000, 0, &written);
}

/* the line into the list box (cut with a '>' at size), the list scrolled to it */
void DbgWin_AddLine(char *line, int len, int size) {
	if (len >= size) {
		len = size - 1;
		line[len - 1] = '>';
	}
	line[len] = 0;
	exe_SendMessageA(g_DbgList, LB_ADDSTRING, 0, (intptr_t)line);
	exe_SendMessageA(g_DbgList, LB_SETCURSEL, (uintptr_t)(exe_SendMessageA(g_DbgList, LB_GETCOUNT, 0, 0) - 1), 0);
}

/* the text into the list box: the last line written again, backspace, line feed and carriage return */
void DbgWin_Printf(const char *fmt, ...) {
	char buf[0x400];
	intptr_t count;
	int pos, len;
	const char *p;
	va_list ap;

	if (!g_DbgCon || !g_DbgWinOpen || !g_DbgWnd || !fmt) {
		return;
	}
	if (!exe_IsWindow(g_DbgWnd)) {
		g_DbgWnd = NULL;
		return;
	}
	va_start(ap, fmt);
	VBprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	count = exe_SendMessageA(g_DbgList, LB_GETCOUNT, 0, 0);
	if (count > 0) {
		exe_SendMessageA(g_DbgList, LB_DELETESTRING, (uintptr_t)(count - 1), (intptr_t)buf);
	}
	len = g_DbgLineLen;
	if (buf[0]) {
		pos = g_DbgLinePos;
		for (p = buf; *p; p++) {
			switch (*p) {
			case '\b':
				if (pos > 0) {
					pos--;
					g_DbgLinePos = pos;
				}
				break;
			case '\n':
				DbgWin_AddLine(g_DbgLine, len, DBGWIN_LINE);
				len = 0;
				g_DbgLineLen = 0;
				pos = 0;
				g_DbgLinePos = 0;
				break;
			case '\r':
				pos = 0;
				g_DbgLinePos = 0;
				break;
			default:
				if ((uint32_t)pos < DBGWIN_LINE - 1) {
					g_DbgLine[pos] = *p;
				}
				pos++;
				g_DbgLinePos = pos;
				if (pos > len) {
					len = pos;
					g_DbgLineLen = len;
				}
				break;
			}
		}
	}
	DbgWin_AddLine(g_DbgLine, len, DBGWIN_LINE);
}

/* the dialog of the debug output made once */
void DbgWin_Open(void *hwnd) {
	if (!g_DbgCon || g_DbgWinOpen || g_DbgWnd) {
		return;
	}
	g_DbgWinOpen = 1;
	g_DbgWnd = exe_CreateDialogParamA((void *)exe_GetWindowLongA(hwnd, GWL_HINSTANCE), (const char *)DBGWIN_DIALOG, hwnd,
		Game_Ptr(DBGWIN_PROC), 0);
}

/* the debug output on with the dialog or the text console of the ini */
void DbgWin_Init(void *hwnd) {
	if (!g_DbgConWindow && !g_DbgConStd) {
		g_DbgCon = g_DbgConStd;
		return;
	}
	g_DbgCon = 1;
	if (g_DbgConWindow && !g_DbgWinOpen && !g_DbgWnd) {
		g_DbgWinOpen = 1;
		g_DbgWnd = exe_CreateDialogParamA((void *)exe_GetWindowLongA(hwnd, GWL_HINSTANCE),
			(const char *)DBGWIN_DIALOG, hwnd, Game_Ptr(DBGWIN_PROC), 0);
	}
	if (g_DbgConStd) {
		DbgStd_Open(hwnd);
	}
}

/* the normal (restored) rectangle of the window */
void Window_GetNormalRect(void *hwnd, int32_t *rect) {
	uint32_t wp[11];

	if (rect) {
		wp[0] = 0x2c;
		exe_GetWindowPlacement(hwnd, wp);
		memcpy(rect, &wp[7], 16);
	}
}
