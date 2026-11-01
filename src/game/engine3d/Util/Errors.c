/*
 * Error codes and reports of the engine3d, original source Util/Errores.cpp (include/scrap/e3dutil.h): a table of
 * 200 texts, 50 and up free for Err_Register. The debug rectangles draw on the window of the engine.
 */
#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/win32.h"

#define ERR_COUNT 200
#define ERR_FIRST_FREE 50
#define ERR_CUSTOM -2	/* Err_Printf: no text of its own (g_ErrCustom) */

extern const char * g_ErrTable[200];
#define g_ErrUnknown GAME_STR(str_432608)	/* "Unknown error" */
#define g_ErrCustom (GAME_ARRAY(const char, g_ErrCustom))
extern int g_ErrLastCode;
extern int g_ErrLastFatal;

/* color 0xRRGGBB, the GDI wants 0xBBGGRR */
static void Dbg_Rect(int x, int y, int w, int h, uint32_t color, rectbrush_t draw) {
	if (!g_Engine) {
		return;
	}
	void *dc = exe_GetDC(g_Engine->hWnd);
	if (!dc) {
		return;
	}
	void *brush = exe_CreateSolidBrush((color >> 16 & 0xff) | (color & 0xff00) | (color & 0xff) << 16);
	if (brush) {
		exe_rect r = { x, y, x + w, y + h };
		draw(dc, &r, brush);
		exe_DeleteObject(brush);
	}
	exe_ReleaseDC(g_Engine->hWnd, dc);
}

void Dbg_FrameRect(int x, int y, int w, int h, uint32_t color) {
	Dbg_Rect(x, y, w, h, color, exe_FrameRect);
}

void Dbg_FillRect(int x, int y, int w, int h, uint32_t color) {
	Dbg_Rect(x, y, w, h, color, exe_FillRect);
}

void Debug_Line(int x0, int y0, int x1, int y1, uint32_t color, int width) {
	if (!g_Engine) {
		return;
	}
	void *dc = exe_GetDC(g_Engine->hWnd);
	if (!dc) {
		return;
	}
	void *pen = exe_CreatePen(0, width, ((color >> 8 & 0xff) | (color & 0xff) << 8) << 8 | (color >> 16 & 0xff));
	if (pen) {
		void *old = exe_SelectObject(dc, pen);
		exe_MoveToEx(dc, x0, y0, NULL);
		exe_LineTo(dc, x1, y1);
		exe_SelectObject(dc, old);
		exe_DeleteObject(pen);
	}
	exe_ReleaseDC(g_Engine->hWnd, dc);
}

const char *Err_Unregister(int code) {
	if ((unsigned)(code - ERR_FIRST_FREE) < ERR_COUNT - ERR_FIRST_FREE) {
		const char *s = g_ErrTable[code];
		g_ErrTable[code] = NULL;
		return s;
	}
	return NULL;
}

const char *Err_Str(int code) {
	if ((unsigned)code < ERR_COUNT) {
		return g_ErrTable[code] ? g_ErrTable[code] : g_ErrUnknown;
	}
	return code == ERR_CUSTOM ? g_ErrCustom : g_ErrUnknown;
}

int Err_Register(const char *text) {
	for (int i = ERR_FIRST_FREE; i < ERR_COUNT; i++) {
		if (!g_ErrTable[i]) {
			g_ErrTable[i] = text;
			return i;
		}
	}
	return -1;
}

void Err_Printf(int fatal, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	Err_VReport(ERR_CUSTOM, fatal, fmt, ap);
	va_end(ap);
}

void Err_Report(int code, int fatal, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	Err_VReport(code, fatal, fmt, ap);
	va_end(ap);
}

/* the text of the code and fmt after it; fatal into the log, else a message box */
void Err_VReport(int code, int fatal, const char *fmt, va_list ap) {
	char buf[0x100];

	g_ErrLastCode = code;
	g_ErrLastFatal = fatal;
	int n = Bprintf(buf, sizeof(buf), "%s", Err_Str(code));
	if (fmt) {
		VBprintf(buf + n, sizeof(buf) - n, fmt, ap);
	}
	if (fatal) {
		Con_FatalErrorf("%s", buf);
		return;
	}
	Con_Warnf("WARN: %s\n", buf);
	exe_ShowCursor(1);
	exe_MessageBoxA(NULL, buf, "Warning", 0x11010);
	exe_ShowCursor(0);
}
