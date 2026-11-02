/*
 * scrapdecomp.log next to the exe, written new at every start.
 */
#include <stdarg.h>
#include <stdio.h>

#include "scrap/shared.h"

static FILE *log_file;

void Log_Open(const char *dir);

void Log_Open(const char *dir) {
	char path[1024];

	if (snprintf(path, sizeof(path), "%sscrapdecomp.log", dir) >= (int)sizeof(path)) {
		return;
	}
	log_file = fopen(path, "wb");
}

void Com_Printf(const char *fmt, ...) {
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n < 0 || !log_file) {
		return;
	}
	if (n >= (int)sizeof(buf)) {
		n = sizeof(buf) - 1;
	}
	fwrite(buf, 1, (size_t)n, log_file);
	fflush(log_file);
}
