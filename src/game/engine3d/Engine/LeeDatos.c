/*
 * Data file reader of the engine3d, original source Engine/LeeDatos.cpp (include/scrap/leedatos.h).
 */
#include <stdarg.h>
#include <string.h>

#include "scrap/archio.h"
#include "scrap/e3dutil.h"
#include "scrap/leedatos.h"

#define LEE_BLANK(c) ((c) == '\t' || (c) == ' ')

static void Lee_SkipBlanks(void) {
	while (*g_LeeCad && LEE_BLANK(*g_LeeCad)) {
		g_LeeCad++;
	}
}

void Lee_String(char *dest, int size) {
	char *p = g_LeeCad;

	CHECK(dest);
	CHECK(size > 0);
	while (*p && LEE_BLANK(*p)) {
		p++;
	}
	CHECK(*p == '"');
	g_LeeCad = p + 1;
	for (p = g_LeeCad; *p != '"'; p++) {
		if (*p == '\0' || *p == '\r' || *p == '\n') {
			Lee_Error("Cadena de caracteres sin cerrar: %s", g_LeeCad);
			break;
		}
	}
	int n = p - g_LeeCad < size - 1 ? p - g_LeeCad : size - 1;
	memcpy(dest, g_LeeCad, n);
	dest[n] = '\0';
	g_LeeCad = p + 1;
}

void Lee_Pop(void) {
	CHECK(g_LeePila);
	cLeePila *p = g_LeePila;
	g_LeeCad = p->cad;
	*crt_errno() = p->err;
	g_LeePila = p->prev;
	Mem_Free(p);
}

void Lee_Expect(const char *tok) {
	size_t len = strlen(tok);

	CHECK(g_LeePila && g_LeePila->arch);
	if (Lee_NextLine()) {
		Lee_Error("EOF no esperado");
	}
	if (crt_strncmp(g_LeeCad, tok, len)) {
		Lee_Error("\nesperado: '%s'\nencontrado: '%s'", tok, g_LeeCad);
	}
	g_LeeCad += len;
	if (*g_LeeCad) {
		CHECK(LEE_BLANK(*g_LeeCad));
		Lee_SkipBlanks();
	}
}

int Lee_NextLine(void) {
	CHECK(g_LeePila && g_LeePila->arch);
	for (;;) {
		g_LeeCad = g_LeePila->line;
		*g_LeeCad = '\0';
		if (!ArchIO_Gets(g_LeeCad, LEE_LINE, g_LeePila->arch)) {
			return -1;
		}
		if (!*g_LeeCad) {
			continue;
		}
		Lee_SkipBlanks();
		if (!*g_LeeCad || *g_LeeCad == '#') {
			continue;
		}
		int i = 0;
		while (g_LeeCad[i] && g_LeeCad[i] != '#') {
			i++;
		}
		g_LeeCad[i] = '\0';
		/* signed chars: bytes from 0x80 count as blanks too */
		for (i = (int)strlen(g_LeeCad) - 1; i >= 0 && (signed char)g_LeeCad[i] <= ' '; i--) {
			g_LeeCad[i] = '\0';
		}
		if (*g_LeeCad) {
			return 0;
		}
	}
}

void Lee_Open(const char *dir, const char *name, int binary) {
	cLeePila *p = Mem_Calloc(sizeof(*p));

	p->prev = g_LeePila;
	g_LeePila = p;
	p->err = *crt_errno();
	p->cad = g_LeeCad;
	Bprintf(p->name, sizeof(p->name), "%s%s", dir ? dir : "", name);
	*crt_errno() = 0;
	g_LeePila->arch = ArchIO_Open(g_LeePila->name, binary ? "rb" : "rt");
	if (!g_LeePila->arch) {
		Err_Report(*crt_errno(), 1, " (%s)", g_LeePila->name);
	}
	g_LeePila->line[0] = '\0';
	g_LeeCad = g_LeePila->line;
}

void Lee_Text(const char *tok, char *dest, int size) {
	if (tok) {
		Lee_Expect(tok);
	}
	Bprintf(dest, size, "%s", g_LeeCad);
}

void Lee_QText(const char *tok, char *dest, int size) {
	if (tok) {
		Lee_Expect(tok);
	}
	Lee_String(dest, size);
}

int Lee_Close(void) {
	CHECK(g_LeePila && g_LeePila->arch);
	ArchIO_Close(g_LeePila->arch);
	int err = *crt_errno();
	Lee_Pop();
	return err;
}

void Lee_Error(const char *fmt, ...) {
	char buf[0x100];
	va_list ap;

	if (!fmt) {
		return;
	}
	va_start(ap, fmt);
	VBprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	Err_Printf(1, "error: %s: %s", g_LeePila->name, buf);
}

void Lee_Float(const char *tok, float *dest) {
	CHECK(dest);
	Lee_Expect(tok);
	CHECK(crt_sscanf(g_LeeCad, "%f", dest) == 1);
}

void Lee_Bytes(void *dest, int n) {
	CHECK(g_LeePila && g_LeePila->arch);
	CHECK(dest);
	if (n) {
		CHECK(n > 0);
		CHECK(ArchIO_Read(dest, n, 1, g_LeePila->arch) == 1);
	}
}

int Lee_Byte(void) {
	CHECK(g_LeePila && g_LeePila->arch);
	int c = ArchIO_Getc(g_LeePila->arch);
	CHECK((unsigned)c <= 255);
	return c;
}

void Lee_Int(const char *tok, int *dest) {
	CHECK(dest);
	Lee_Expect(tok);
	CHECK(crt_sscanf(g_LeeCad, "%d", dest) == 1);
}
