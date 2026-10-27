/*
 * Logs and helpers of the engine3d, original source Engine/varios.cpp (include/scrap/varios.h). The windows of the
 * debug console stay in the exe.
 */
#include <stdarg.h>
#include <string.h>

#include "scrap/archio.h"
#include "scrap/bsp.h"
#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/filepak.h"
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

float RSqrt_Estimate(float x) {
	uint32_t u, r;
	float f;

	memcpy(&u, &x, sizeof(u));
	r = ((0x17c - (u >> 23 & 0xff)) & 0x3fe) << 22 | (uint32_t)g_RSqrtTable[u >> 17 & 0x7f] << 15;
	memcpy(&f, &r, sizeof(f));
	return f;
}

float RSqrt(float x) {
	float y = RSqrt_Estimate(x);

	y = (3.0f - y * y * x) * y * 0.5f;
	return (3.0f - y * y * x) * y * 0.5f;
}

/* the window helpers between efec_malla.cpp and LensFlare.cpp (their source is not known) */
typedef int(__stdcall *setdlgitemtext_t)(void *hdlg, int id, const char *text);
#define exe_SetDlgItemTextA GAME_DATA(setdlgitemtext_t, 0x3a1344)

int Win_SetTextf(void *hwnd, const char *fmt, ...) {
	char buf[0x400];
	va_list ap;

	va_start(ap, fmt);
	VBprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	return exe_SetWindowTextA(hwnd, buf) != 0;
}

byte Dlg_SetFloat(void *hwnd, int id, float v, int prec) {
	char buf[0x40];

	Bprintf(buf, sizeof(buf), "%.*f", prec, (double)v);
	exe_SetDlgItemTextA(hwnd, id, buf);
	return 1;
}

byte Dlg_GetFloat(void *hwnd, int id, float *out) {
	char buf[0x40];

	exe_GetDlgItemTextA(hwnd, id, buf, sizeof(buf));
	return crt_sscanf(buf, "%g", out) == 1;
}
