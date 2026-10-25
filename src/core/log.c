/*
 * scrapdecomp.log next to Scrap.exe, written new at every start, plus OutputDebugString for a debugger.
 */
#include <stdarg.h>
#include <stdio.h>
#include <windows.h>

#include "scrap/shared.h"

static HANDLE log_file = INVALID_HANDLE_VALUE;

void Log_Open(const char *dir);

void Log_Open(const char *dir) {
	char path[MAX_PATH];

	if (snprintf(path, sizeof(path), "%sscrapdecomp.log", dir) >= (int)sizeof(path)) {
		return;
	}
	log_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

void Com_Printf(const char *fmt, ...) {
	char buf[1024];
	va_list ap;
	DWORD written;

	va_start(ap, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n < 0) {
		return;
	}
	if (n >= (int)sizeof(buf)) {
		n = sizeof(buf) - 1;
	}
	OutputDebugStringA(buf);
	if (log_file != INVALID_HANDLE_VALUE) {
		WriteFile(log_file, buf, (DWORD)n, &written, NULL);
	}
}
