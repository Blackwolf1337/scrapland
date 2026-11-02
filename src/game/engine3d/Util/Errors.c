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

char str_432608[] = "Unknown error";

#define g_ErrUnknown str_432608	/* "Unknown error" */

/* a line in pixels over the frame (the original drew it with GDI on the window), color 0xRRGGBB */
void Debug_Line(int x0, int y0, int x1, int y1, uint32_t color, int width) {
	(void)width;
	if (g_Engine && g_Engine->device) {
		Render_Line2D((float)x0, (float)y0, (float)x1, (float)y1, color | 0xff000000);
	}
}

SD_SYM(str_3de0c4);
SD_SYM(str_3de0cc);
SD_SYM(str_3ca58c);
SD_SYM(str_3de0e4);
SD_SYM(str_3de0f4);
SD_SYM(str_3de110);
SD_SYM(str_3de124);
SD_SYM(str_3de140);
SD_SYM(str_3de154);
SD_SYM(str_3de168);
SD_SYM(str_3de17c);
SD_SYM(str_3de190);
SD_SYM(str_3de1b4);
SD_SYM(str_3de1c8);
SD_SYM(str_3de1dc);
SD_SYM(str_3ca704);
SD_SYM(str_3de1e8);
SD_SYM(str_3de1f8);
SD_SYM(str_3de20c);
SD_SYM(str_3de21c);
SD_SYM(str_3de22c);
SD_SYM(str_3de23c);
SD_SYM(str_3de24c);
SD_SYM(str_3de260);
SD_SYM(str_3de280);
SD_SYM(str_3de294);
SD_SYM(str_3de2b8);
SD_SYM(str_3de2c8);
SD_SYM(str_3de2e0);
SD_SYM(str_3de2f0);
SD_SYM(str_3de308);
SD_SYM(str_3de318);
SD_SYM(str_3de324);
SD_SYM(str_3de334);
SD_SYM(str_3de348);
SD_SYM(str_3de364);
SD_SYM(str_3de378);
SD_SYM(str_3de38c);
SD_SYM(str_3de3a8);
SD_SYM(str_3de3bc);
SD_SYM(str_3de3d4);
SD_SYM(str_3de3e8);
SD_SYM(str_3de3fc);
SD_SYM(str_3de40c);
const char *g_ErrTable[200] = {
	(void *)sd_str_3de0c4, (void *)sd_str_3de0cc, (void *)sd_str_3ca58c, (void *)sd_str_3de0e4,
	(void *)sd_str_3de0f4, (void *)sd_str_3de110, (void *)sd_str_3de124, (void *)sd_str_3de140,
	(void *)sd_str_3de154, (void *)sd_str_3de168, (void *)sd_str_3de17c, (void *)sd_str_3de190,
	(void *)sd_str_3de1b4, (void *)sd_str_3de1c8, (void *)sd_str_3de1dc, (void *)sd_str_3ca704,
	(void *)sd_str_3de1e8, (void *)sd_str_3de1f8, (void *)sd_str_3de20c, (void *)sd_str_3de21c,
	(void *)sd_str_3de22c, (void *)sd_str_3de23c, (void *)sd_str_3de24c, (void *)sd_str_3de260,
	(void *)sd_str_3de280, (void *)sd_str_3de294, (void *)sd_str_3ca704, (void *)sd_str_3de2b8,
	(void *)sd_str_3de2c8, (void *)sd_str_3de2e0, (void *)sd_str_3de2f0, (void *)sd_str_3de308,
	(void *)sd_str_3de318, (void *)sd_str_3de324, (void *)sd_str_3de334, (void *)sd_str_3ca704,
	(void *)sd_str_3de348, (void *)sd_str_3ca704, (void *)sd_str_3de364, (void *)sd_str_3de378,
	(void *)sd_str_3de38c, (void *)sd_str_3de3a8, (void *)sd_str_3de3bc, (void *)sd_str_3ca704, 0, 0, 0, 0, 0, 0,
	(void *)sd_str_3de3d4, (void *)sd_str_3de3e8, (void *)sd_str_3de3fc, (void *)sd_str_3de40c,
};

const char *Err_Unregister(int code) {
	if ((unsigned)(code - ERR_FIRST_FREE) < ERR_COUNT - ERR_FIRST_FREE) {
		const char *s = g_ErrTable[code];
		g_ErrTable[code] = NULL;
		return s;
	}
	return NULL;
}

char g_ErrCustom[4];

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

int g_ErrLastCode;

int g_ErrLastFatal;

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

const char str_3de0c4[] = "Error 0";

const char str_3de0cc[] = "Operation not permitted";

const char str_3de0e4[] = "No such process";

const char str_3de0f4[] = "Interrupted function call";

const char str_3de110[] = "Input/output error";

const char str_3de124[] = "No such device or address";

const char str_3de140[] = "Arg list too long";

const char str_3de154[] = "Exec format error";

const char str_3de168[] = "Bad file descriptor";

const char str_3de17c[] = "No child processes";

const char str_3de190[] = "Resource temporarily unavailable";

const char str_3de1b4[] = "Not enough memory";

const char str_3de1c8[] = "Permission denied";

const char str_3de1dc[] = "Bad address";

const char str_3de1e8[] = "Resource device";

const char str_3de1f8[] = "File already exists";

const char str_3de20c[] = "Improper link";

const char str_3de21c[] = "No such device";

const char str_3de22c[] = "Not a directory";

const char str_3de23c[] = "Is a directory";

const char str_3de24c[] = "Invalid argument";

const char str_3de260[] = "Too many open files in system";

const char str_3de280[] = "Too many open files";

const char str_3de294[] = "Inappropriate I/O control operation";

const char str_3de2b8[] = "File too large";

const char str_3de2c8[] = "No space left on device";

const char str_3de2e0[] = "Invalid seek";

const char str_3de2f0[] = "Read-only file system";

const char str_3de308[] = "Too many links";

const char str_3de318[] = "Broken pipe";

const char str_3de324[] = "Domain error";

const char str_3de334[] = "Result too large";

const char str_3de348[] = "Resource deadlock avoided";

const char str_3de364[] = "Filename too long";

const char str_3de378[] = "No locks available";

const char str_3de38c[] = "Function not implemented";

const char str_3de3a8[] = "Directory not empty";

const char str_3de3bc[] = "Illegal byte sequence";

const char str_3de3d4[] = "Invalid file format";

const char str_3de3e8[] = "File has zero size";

const char str_3de3fc[] = "Invalid path";

const char str_3de40c[] = "Invalid parameter";

const char str_3ca58c[] = "File not found";

const char str_3ca704[] = "Unknown error";

