/*
 * Logs and helpers of the engine3d, original source Engine/varios.cpp (include/scrap/misc.h). The dialog procedure
 * of the debug console stays in the exe.
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "scrap/archio.h"
#include "scrap/crt.h"
#include "scrap/bsp.h"
#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/filepak.h"
#include "scrap/nodefx.h"
#include "scrap/platform.h"
#include "scrap/staticinit.h"

#define g_LogLevel (GVar_EngineDebugLevel.value.i)	/* 10: the console mask goes back to 0x91 */
static cGVar GVar_EngineDebugLevel;

#define LOG_LINE 0x400
#define LOG_NAME "engine.log"
#define CRT_LC_TIME 5
#define LOG_TIME 0x80

/* the st_mtime of a struct _stat32 */
#define STAT32_SIZE 0x24
#define STAT32_MTIME 0x1c

static void cLog_Open(cLog *self) {
	char path[MAX_FILEPATH];
	char date[LOG_TIME];
	int32_t t;

	if (self->f || self->failed) {
		return;
	}
	self->f = File_OpenRel(Path_Copy(path, LOG_NAME, -1), "wb");
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
static void cLog_dtor(cLog *self) {
	if (self->f) {
		crt_fprintf(self->f, "\r\n");
		if (self->f) {
			crt_fclose(self->f);
			self->f = NULL;
		}
	}
}

/* the atexit destructor of (&g_FileLog) (rva 0x38ece0, the destructor inlined) */
static void Misc_FiniFileLog(void) {
	cLog_dtor((&g_FileLog));
}

static uint32_t g_LogFileMask;
static uint32_t g_LogConMask = 65535;

/* text to the console printers of the bits in mask and to the file */
static void cLog_Write(cLog *self, uint32_t mask, const char *text) {
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
/* the debug console: on, as a dialog window, as a text console */
static int g_DbgCon;

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
	fputs(buf, stdout);
	fflush(stdout);
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
	E3D_LogVerbose((&g_FileLog), "WARN: FileTime changed: %s (Saved:%d / Now:%d / Diff:%d)", name, saved, now, diff);
	return 1;
}

int File_Backup(const char *name, byte inDir) {
	char bak[2 * MAX_FILEPATH];

	if (inDir) {
		Path_Merge(bak, ".bak", name);
	} else {
		Bprintf(bak, sizeof(bak), "%s.bak", name);
	}
	crt_remove(bak);
	crt_rename(name, bak);
	return 0;
}

int Wild_MatchLower(const char *pat, const char *name) {
	char low[2 * MAX_FILEPATH];

	String_Copy(low, name, sizeof(low));
	crt_strlwr(low);
	return Wild_Match(pat, low);
}

char *Path_Copy(char *dest, const char *src, int size) {
	if (size == -1) {
		size = MAX_FILEPATH;
	}
	CHECK(dest && size > 0);
	if (dest != src) {
		String_Copy(dest, src, size);
	}
	return dest;
}

/* where the relative paths start: the game directory with a "\\" at the end */
static char g_E3DDir[264];

void Path_Normalize(char *dest, const char *path, int size) {
	char drive[4], dir[0x100];
	char b[2 * MAX_FILEPATH];

	if (size == -1) {
		size = MAX_FILEPATH;
	}
	CHECK(dest && size > 0);
	crt_splitpath(path, drive, dir, NULL, NULL);
	if (drive[0] || Path_IsSep(dir[0])) {
		if (dest != path) {
			String_Copy(dest, path, size);
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
			if ((Path_IsSep(prev) || (n == 1 && (prev == '\0' || prev == ':'))) &&
				(next == '\0' || Path_IsSep(next))) {
				r += n - 1 + Path_IsSep(next);
				for (int i = 1; i < n; i++) {
					if (w > 1 && Path_IsSep(b[w - 1]) && b[w - 2] != ':') {
						for (w--; w > 0 && b[w - 1] != ':' && !Path_IsSep(b[w - 1]); w--) {
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
	String_Copy(dest, b, size);
}

static char g_ExePath[264];

char *E3D_GetExePath(void) {
	return g_ExePath;
}

/* the static initializer: the path of the exe, g_E3DDir from its directory and "../" */
void Misc_InitE3DDir(void) {
	char exe[sizeof(g_ExePath)];
	Plat_ExePath(exe, sizeof(exe));
	crt_gamepath(exe, g_ExePath, sizeof(g_ExePath));
	Path_Part(g_E3DDir, g_ExePath, PATH_DRIVE | PATH_DIR);
	Path_Normalize(g_E3DDir, "../", -1);	/* "../" */
}

static byte g_RSqrtTable[144];

/* the bits 15..22 (rounded) of 1 / sqrt of the 128 mantissa steps of the exponents 0x7f (0..63) and 0x7e */
static void RSqrt_InitTable(void) {
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

/* the debug output on when the ini asks for the console window or the text console; it goes to stdout */
void DbgCon_Init(void) {
	g_DbgCon = g_DbgConWindow || g_DbgConStd;
}

static cGVar GVar_EngineDebugLevel;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_EngineDebugLevel, &GVar_EngineDebugLevel, "EngineDebugLevel", 0, "Extra engine debug info (0/1/2)", 0)
INIT_ATEXIT(Init_FileLog, Misc_FiniFileLog)
INIT_JUMP(Init_RSqrtTable, RSqrt_InitTable)
