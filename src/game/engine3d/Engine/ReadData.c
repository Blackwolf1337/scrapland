/*
 * Data file reader of the engine3d, original source Engine/LeeDatos.cpp (include/scrap/readdata.h).
 */
#include <stdarg.h>
#include <string.h>

#include "scrap/archio.h"
#include "scrap/e3dutil.h"
#include "scrap/readdata.h"

#define READ_BLANK(c) ((c) == '\t' || (c) == ' ')

char *g_ReadString;

static void Read_SkipBlanks(void) {
	while (*g_ReadString && READ_BLANK(*g_ReadString)) {
		g_ReadString++;
	}
}

/* "text" at g_ReadString */
static void Read_String(char *dest, int size) {
	char *p = g_ReadString;

	CHECK(dest);
	CHECK(size > 0);
	while (*p && READ_BLANK(*p)) {
		p++;
	}
	CHECK(*p == '"');
	g_ReadString = p + 1;
	for (p = g_ReadString; *p != '"'; p++) {
		if (*p == '\0' || *p == '\r' || *p == '\n') {
			Read_Error("Cadena de caracteres sin cerrar: %s", g_ReadString);
			break;
		}
	}
	int n = p - g_ReadString < size - 1 ? p - g_ReadString : size - 1;
	memcpy(dest, g_ReadString, n);
	dest[n] = '\0';
	g_ReadString = p + 1;
}

static cReadStack *g_ReadStack;

static void Read_Pop(void) {
	CHECK(g_ReadStack);
	cReadStack *p = g_ReadStack;
	g_ReadString = p->string;
	*crt_errno() = p->err;
	g_ReadStack = p->prev;
	Mem_Free(p);
}

void Read_Expect(const char *tok) {
	size_t len = strlen(tok);

	CHECK(g_ReadStack && g_ReadStack->file);
	if (Read_NextLine()) {
		Read_Error("EOF no esperado");
	}
	if (crt_strncmp(g_ReadString, tok, len)) {
		Read_Error("\nesperado: '%s'\nencontrado: '%s'", tok, g_ReadString);
	}
	g_ReadString += len;
	if (*g_ReadString) {
		CHECK(READ_BLANK(*g_ReadString));
		Read_SkipBlanks();
	}
}

int Read_NextLine(void) {
	CHECK(g_ReadStack && g_ReadStack->file);
	for (;;) {
		g_ReadString = g_ReadStack->line;
		*g_ReadString = '\0';
		if (!FileIO_Gets(g_ReadString, READ_LINE, g_ReadStack->file)) {
			return -1;
		}
		if (!*g_ReadString) {
			continue;
		}
		Read_SkipBlanks();
		if (!*g_ReadString || *g_ReadString == '#') {
			continue;
		}
		int i = 0;
		while (g_ReadString[i] && g_ReadString[i] != '#') {
			i++;
		}
		g_ReadString[i] = '\0';
		/* signed chars: bytes from 0x80 count as blanks too */
		for (i = (int)strlen(g_ReadString) - 1; i >= 0 && (signed char)g_ReadString[i] <= ' '; i--) {
			g_ReadString[i] = '\0';
		}
		if (*g_ReadString) {
			return 0;
		}
	}
}

void Read_Open(const char *dir, const char *name, int binary) {
	cReadStack *p = Mem_Calloc(sizeof(*p));

	p->prev = g_ReadStack;
	g_ReadStack = p;
	p->err = *crt_errno();
	p->string = g_ReadString;
	Bprintf(p->name, sizeof(p->name), "%s%s", dir ? dir : "", name);
	*crt_errno() = 0;
	g_ReadStack->file = FileIO_Open(g_ReadStack->name, binary ? "rb" : "rt");
	if (!g_ReadStack->file) {
		Err_Report(*crt_errno(), 1, " (%s)", g_ReadStack->name);
	}
	g_ReadStack->line[0] = '\0';
	g_ReadString = g_ReadStack->line;
}

void Read_Text(const char *tok, char *dest, int size) {
	if (tok) {
		Read_Expect(tok);
	}
	Bprintf(dest, size, "%s", g_ReadString);
}

void Read_QText(const char *tok, char *dest, int size) {
	if (tok) {
		Read_Expect(tok);
	}
	Read_String(dest, size);
}

int Read_Close(void) {
	CHECK(g_ReadStack && g_ReadStack->file);
	FileIO_Close(g_ReadStack->file);
	int err = *crt_errno();
	Read_Pop();
	return err;
}

void Read_Error(const char *fmt, ...) {
	char buf[0x100];
	va_list ap;

	if (!fmt) {
		return;
	}
	va_start(ap, fmt);
	VBprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	Err_Printf(1, "error: %s: %s", g_ReadStack->name, buf);
}

void Read_Bytes(void *dest, int n) {
	CHECK(g_ReadStack && g_ReadStack->file);
	CHECK(dest);
	if (n) {
		CHECK(n > 0);
		CHECK(FileIO_Read(dest, n, 1, g_ReadStack->file) == 1);
	}
}

int Read_Byte(void) {
	CHECK(g_ReadStack && g_ReadStack->file);
	int c = FileIO_Getc(g_ReadStack->file);
	CHECK((unsigned)c <= 255);
	return c;
}

void Read_Int(const char *tok, int *dest) {
	CHECK(dest);
	Read_Expect(tok);
	CHECK(crt_sscanf(g_ReadString, "%d", dest) == 1);
}
