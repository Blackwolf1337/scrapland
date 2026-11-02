/*
 * String helpers of Global/StrUtils.cpp: bounded copy and printf through the exe's CRT, path parts, trimming and
 * the UTF-8 conversion of texts for the engine fonts.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/game.h"
#include "scrap/scorer.h"

/* __local_stdio_printf_options of the UCRT in the exe */

char *Str_Copy(char *dst, const char *src, int size) {
	CHECK(dst && src && size > 0);
	*dst = 0;
	crt_strncat(dst, src, size - 1);
	return dst;
}

/* the printf of the exe; a cut text is terminated and its length returned, with a log line if warn */
static int Str_Format(char *dst, int size, const char *fmt, va_list ap, int warn) {
	if (!dst || !fmt || size < 1) {
		return 0;
	}
	int n = vsnprintf(dst, (size_t)size, fmt, ap);
	if (n < 0 || n >= size) {
		dst[size - 1] = 0;
		n = (int)strlen(dst);
		if (warn) {
			Con_FatalErrorf("String size error: vcbprintf(): '%s'\n", dst);
		}
	}
	return n;
}

int Str_VPrintf(char *dst, int size, const char *fmt, va_list ap) {
	return Str_Format(dst, size, fmt, ap, 0);
}

static int Str_VPrintfWarn(char *dst, int size, const char *fmt, va_list ap) {
	return Str_Format(dst, size, fmt, ap, 1);
}

int Str_Printf(char *dst, int size, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int n = Str_VPrintfWarn(dst, size, fmt, ap);
	va_end(ap);
	return n;
}

/* like Str_Printf without the log line */
int Str_PrintfQuiet(char *dst, int size, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int n = Str_VPrintf(dst, size, fmt, ap);
	va_end(ap);
	return n;
}

/* the directory (with its separator), the base name without extension and the extension (with the dot) */
void Path_Split(const char *path, char *name, char *dir, char *ext) {
	const char *base = path, *end = path;

	for (const char *p = path; *p; p++) {
		if (*p == '/' || *p == ':' || *p == '\\') {
			base = p + 1;
		}
		end = p + 1;
	}
	/* end is the last '.' of the base name, else the end of the path */
	for (const char *p = base; *p; p++) {
		if (*p == '.') {
			end = p;
		}
	}
	if (dir) {
		memcpy(dir, path, base - path);
		dir[base - path] = 0;
	}
	if (name) {
		memcpy(name, base, end - base);
		name[end - base] = 0;
	}
	if (ext) {
		memcpy(ext, end, strlen(end) + 1);
	}
}

/*
 * breaks text into at most maxLines lines no wider than maxWidth in the font, at blanks it overwrites with 0;
 * lines gets the start of each line, returns the number of lines
 */
int Str_WrapText(char *text, char **lines, float maxWidth, int maxLines, const char *fontName) {
	void *font = Font_Get(fontName);
	int n = 1;
	float size[2];

	lines[0] = text;
	char *sp = strchr(text, ' ');
	if (!sp) {
		return n;
	}
	while (n < maxLines && *sp) {
		char *next = strchr(sp + 1, ' ');
		if (!next) {
			next = sp + strlen(sp);
		}
		Font_TextSize(font, NULL, size, 0.0f, 0.0f, "%.*s", (int)(next - *lines), *lines);
		if (size[0] > maxWidth) {
			*sp = '\0';
			n++;
			lines[1] = sp + 1;
			lines++;
		}
		sp = next;
	}
	return n;
}

/* drops the blanks and control characters at both ends; a text starting with a byte from 0x80 is cleared too */
void Str_TrimBlanks(char *s) {
	int i = 0, j = 0, last = 0;

	while ((byte)s[i] < 0x21 && s[i]) {
		i++;
	}
	for (const char *p = s + i; *p; p++) {
		s[j] = *p;
		if ((byte)*p > 0x20) {
			last = j;
		}
		j++;
	}
	s[last + 1] = 0;
	if ((signed char)s[0] < '!') {
		s[0] = 0;
	}
}

static char g_VersionBuf[256];

const char *Version_Get(void) {
	Str_Printf(g_VersionBuf, 0x100, "%d.%d%s  %s  %s", 1, 1, " (GMC3)", "Nov 24 2021", "15:24:17");
	return g_VersionBuf;
}
