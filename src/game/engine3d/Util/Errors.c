/*
 * Error codes and reports of the engine3d, original source Util/Errores.cpp (include/scrap/e3dutil.h): a table of
 * 200 texts, 50 and up free for Err_Register, and the debug lines over the frame.
 */
#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/render.h"
#include "scrap/platform.h"

#define ERR_COUNT 200
#define ERR_FIRST_FREE 50
#define ERR_CUSTOM -2	/* Err_Printf: no text of its own (g_ErrCustom) */

extern const char * g_ErrTable[200];
extern char str_432608[];
#define g_ErrUnknown str_432608	/* "Unknown error" */
extern char g_ErrCustom[4];
extern int g_ErrLastCode;
extern int g_ErrLastFatal;

/* a line in pixels over the frame (the original drew it with GDI on the window), color 0xRRGGBB */
void Debug_Line(int x0, int y0, int x1, int y1, uint32_t color, int width) {
	(void)width;
	if (g_Engine && g_Engine->device) {
		Render_Line2D((float)x0, (float)y0, (float)x1, (float)y1, color | 0xff000000);
	}
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
	Plat_MessageBox(buf, "Warning", 0x11010);
}
