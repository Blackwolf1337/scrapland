/*
 * The lexer of the engine3d text files, original source Util/Lex.cpp (include/scrap/lex.h). The file is read
 * with fgetc of the exe's CRT, one character ahead in g_LexCh; the token text grows in g_LexToken.
 */
#include <string.h>

#include "scrap/e3dutil.h"
#include "scrap/lex.h"

int crt_fgetc(void *f);
int crt_ungetc(int c, void *f);
void *crt_fopen(const char *name, const char *mode);
int crt_fclose(void *f);
int *crt_errno(void);
int crt_isalpha(int c);
int crt_isalnum(int c);
int crt_isdigit(int c);
int crt_isxdigit(int c);
int crt_tolower(int c);
[[noreturn]] void crt_exit(int code);
/* the text of an error code of Util/Errores.cpp (rva 0x27cdd0) */
const char *Err_Str(int code);

/* the inlined read of the next character */
static int Lex_Getc(void) {
	CHECK(g_Lex);
	return crt_fgetc(g_Lex->f);
}

/* closes the current file and goes on with the including one */
static void Lex_Pop(void) {
	cLexFile *lex = g_Lex;
	cLexFile *parent = lex->parent;

	crt_fclose(lex->f);
	Mem_Free(lex->name);
	Mem_Free(lex->path);
	if (parent) {
		parent->included += lex->included + lex->line;
	}
	Mem_Free(lex);
	g_Lex = parent;
	if (parent) {
		g_LexCh = parent->savedCh;
	}
}

void Lex_String(void) {
	CHECK(g_LexCh == '"');
	g_LexCh = Lex_Getc();
	for (;;) {
		if (g_LexCh == '"') {
			g_LexCh = Lex_Getc();
			return;
		}
		if (g_LexCh == -1 || g_LexCh == '\r' || g_LexCh == '\n') {
			Lex_Error(0, "Unterminated string constant");
			return;
		}
		if (g_Lex->escapes && g_LexCh == '\\') {
			g_LexCh = Lex_Getc();
			if (crt_isdigit(g_LexCh)) {
				Lex_Octal();
				continue;
			}
			switch (g_LexCh) {
			case 'a':
				g_LexCh = '\a';
				break;
			case 'b':
				g_LexCh = '\b';
				break;
			case 'f':
				g_LexCh = '\f';
				break;
			case 'n':
				g_LexCh = '\n';
				break;
			case 'r':
				g_LexCh = '\r';
				break;
			case 't':
				g_LexCh = '\t';
				break;
			case 'v':
				g_LexCh = '\v';
				break;
			case 'x':
				Lex_Hex();
				continue;
			}
		}
		Lex_Add(g_LexCh);
		g_LexCh = Lex_Getc();
	}
}

/* "#inc "file"" */
void Lex_Directive(void) {
	CHECK(g_LexCh == '#');
	g_LexCh = Lex_Getc();
	if (crt_isalpha(g_LexCh) || g_LexCh == '_') {
		Lex_Ident();
	}
	Lex_SkipBlanks();
	if (strcmp(g_LexToken, "inc") != 0) {
		Lex_Error(0, "Unknown '%s' directive", g_LexToken);
		Lex_SkipLine();
		return;
	}
	g_LexToken[0] = '\0';
	g_LexTokenLen = 0;
	if (g_LexCh == '"') {
		g_LexCh = Lex_Getc();
		Lex_ReadUntil('"');
		if (g_LexCh == '"') {
			Lex_SkipLine();
			Lex_Open(g_LexToken, g_Lex->escapes);
			return;
		}
	}
	Lex_Error(0, "Invalid 'inc' directive");
	Lex_SkipLine();
}

/* "\x" with up to 2 hex digits; "x" alone without digits */
void Lex_Hex(void) {
	int v = 0;
	int n = 1;

	g_LexCh = Lex_Getc();
	if (!crt_isxdigit(g_LexCh)) {
		CHECK(g_LexTokenLen < MAX_ULT_CLEX - 1);
		*(uint16_t *)(g_LexToken + g_LexTokenLen++) = 'x';
		return;
	}
	while (++n != 4) {
		v = v * 16 + (crt_isdigit(g_LexCh) ? g_LexCh - '0' : crt_tolower(g_LexCh) - ('a' - 10));
		g_LexCh = Lex_Getc();
		if (v > 0xff) {
			Lex_Error(0, "Hexadecimal constant too large");
			Lex_Add(0xff);
			return;
		}
		if (!crt_isxdigit(g_LexCh)) {
			Lex_Add(v);
			return;
		}
	}
	Lex_Error(0, "Hexadecimal constant too large");
	Lex_Add(v);
}

void Lex_Ident(void) {
	CHECK(crt_isalpha(g_LexCh) || g_LexCh == '_');
	/* the end of the file (byte 0xff) does not end it either, the original then stops on the CHECK of Lex_Add */
	do {
		Lex_Add(g_LexCh);
		g_LexCh = Lex_Getc();
	} while (crt_isalnum(g_LexCh) || g_LexCh == '_' || g_LexCh == '#' || (byte)g_LexCh >= 0x80);
}

void Lex_ReadUntil(int end) {
	while (g_LexCh != end && g_LexCh != -1 && g_LexCh != '\r' && g_LexCh != '\n') {
		Lex_Add(g_LexCh);
		g_LexCh = Lex_Getc();
	}
}

/* the digits from g_LexCh on */
static void Lex_AddDigits(void) {
	while (crt_isdigit(g_LexCh)) {
		Lex_Add(g_LexCh);
		g_LexCh = Lex_Getc();
	}
}

int Lex_Number(void) {
	int type = CLEX_INT;

	if (g_LexCh == '0') {
		Lex_Add('0');
		g_LexCh = Lex_Getc();
		if (g_LexCh == 'x' || g_LexCh == 'X') {
			Lex_Add(g_LexCh);
			g_LexCh = Lex_Getc();
			while (crt_isxdigit(g_LexCh)) {
				Lex_Add(g_LexCh);
				g_LexCh = Lex_Getc();
			}
			return CLEX_INT;
		}
	}
	Lex_AddDigits();
	if (g_LexCh == '.' || g_LexCh == 'e' || g_LexCh == 'E') {
		type = CLEX_FLOAT;
		if (g_LexCh == '.') {
			Lex_Add('.');
			g_LexCh = Lex_Getc();
			Lex_AddDigits();
		}
		if (g_LexCh == 'e' || g_LexCh == 'E') {
			Lex_Add(g_LexCh);
			g_LexCh = Lex_Getc();
			if (g_LexCh == '+' || g_LexCh == '-') {
				Lex_Add(g_LexCh);
				g_LexCh = Lex_Getc();
			}
			if (!crt_isdigit(g_LexCh)) {
				Lex_Error(0, "Invalid floating point constant: '%s'", g_LexToken);
			} else {
				Lex_AddDigits();
				return CLEX_FLOAT;
			}
		}
	}
	return type;
}

/* "\0".."\377": up to 3 octal digits, '8' and '9' give 0 */
void Lex_Octal(void) {
	int v = 0;
	int n = 0;

	while (g_LexCh >= '0' && g_LexCh <= '7') {
		if (++n == 5) {
			Lex_Error(0, "Octal constant too large");
			break;
		}
		v = v * 8 + g_LexCh - '0';
		g_LexCh = Lex_Getc();
		if (v > 0xff) {
			Lex_Error(0, "Octal constant too large");
			Lex_Add(0xff);
			return;
		}
	}
	Lex_Add(v);
}

void Lex_Print(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	Lex_VPrint(fmt, ap);
	va_end(ap);
}

void Lex_VPrint(const char *fmt, va_list ap) {
	char buf[0x400];

	VBprintf(buf, sizeof(buf), fmt, ap);
	if (g_LexOut) {
		g_LexOut(buf);
	} else {
		Lex_Print("%s", buf);
	}
}

/* CR LF and LF CR count as one line end */
void Lex_NewLine(void) {
	int other = g_LexCh == '\r' ? '\n' : '\r';

	g_Lex->line++;
	g_Lex->tokens = 0;
	g_LexCh = Lex_Getc();
	if (g_LexCh == other) {
		g_LexCh = Lex_Getc();
	}
}

void Lex_Comment(void) {
	int start = g_Lex->line;
	int depth = 1;

	g_LexCh = crt_fgetc(g_Lex->f);
	do {
		if (g_LexCh == -1) {
			if (depth > 0) {
				Lex_Error(0, "Unexpected end of file in comment started on line %d", start);
			}
			return;
		}
		switch (g_LexCh) {
		case '\n':
		case '\r':
			Lex_NewLine();
			break;
		case '*':
			g_LexCh = Lex_Getc();
			if (g_LexCh == '/') {
				depth--;
				g_LexCh = Lex_Getc();
			}
			break;
		case '/':
			g_LexCh = Lex_Getc();
			if (g_LexCh == '*') {
				depth++;
				g_LexCh = Lex_Getc();
			}
			break;
		default:
			g_LexCh = Lex_Getc();
			break;
		}
	} while (depth > 0);
}

void Lex_SkipBlanks(void) {
	while (g_LexCh == '\t' || g_LexCh == ' ') {
		g_LexCh = Lex_Getc();
	}
}

void Lex_SkipLine(void) {
	while (g_LexCh != -1 && g_LexCh != '\n' && g_LexCh != '\r') {
		g_LexCh = Lex_Getc();
	}
}

void Lex_NextCh(void) {
	g_LexCh = Lex_Getc();
}

/* the character and a terminator after it */
void Lex_Add(int ch) {
	CHECK(ch >= 0 && ch <= 255);
	CHECK(g_LexTokenLen < MAX_ULT_CLEX - 1);
	*(uint16_t *)(g_LexToken + g_LexTokenLen++) = (uint16_t)ch;
}

/* the character after g_LexCh */
int Lex_Peek(void) {
	CHECK(g_Lex);
	int ch = crt_fgetc(g_Lex->f);
	int ch2 = crt_ungetc(ch, g_Lex->f);
	CHECK(ch == ch2);
	return ch;
}

/* relative to the current file first, then to g_LexDir */
int Lex_Open(const char *name, int escapes) {
	char path[MAX_RUTA];
	void *f = NULL;

	if (g_Lex) {
		Ruta_Part(path, g_Lex->path, RUTA_DRIVE | RUTA_DIR);
		if (Ruta_Full(path, name, MAX_RUTA, path)) {
			f = crt_fopen(path, "rt");
		}
	}
	if (!f && Ruta_Full(path, name, MAX_RUTA, g_LexDir)) {
		f = crt_fopen(path, "rt");
	}
	if (!f) {
		Lex_Error(0, "%s (%s)", Err_Str(*crt_errno()), name);
		return -1;
	}
	if (!g_Lex) {
		g_LexErrors = 0;
		g_LexWarnings = 0;
	} else {
		g_Lex->savedCh = g_LexCh;
	}
	cLexFile *lex = Mem_Alloc(sizeof(*lex));
	lex->name = Cad_Dup(name);
	lex->path = Cad_Dup(path);
	lex->f = f;
	lex->line = 1;
	lex->tokens = 0;
	lex->included = 0;
	lex->savedCh = -1;
	lex->parent = g_Lex;
	lex->escapes = escapes;
	g_Lex = lex;
	g_LexTokenLen = 0;
	g_LexToken[0] = '\0';
	g_LexCh = crt_fgetc(lex->f);
	return 0;
}

void Lex_Close(int summary) {
	CHECK(g_Lex);
	if (summary == 2 || (summary == 1 && (g_LexErrors || g_LexWarnings))) {
		Lex_Summary();
	}
	Lex_Pop();
}

void Lex_Error(int level, const char *fmt, ...) {
	const char *label;
	va_list ap;

	if ((g_LexErrors == 20 && level == 0) || (g_LexWarnings == 100 && level == 1)) {
		if (g_Lex) {
			while (g_Lex->parent) {
				Lex_Pop();
			}
			Lex_Print("\n%s (%d): %s: ", g_Lex->name, g_Lex->line, "Error");
		}
		Lex_Print("Too many %s: analysis cancelled", level ? "warnings" : "errors");
		if (g_Lex) {
			Lex_Summary();
		}
		crt_exit(-1);
	}
	if (level == 0) {
		g_LexErrors++;
		label = "Error";
	} else if (level == 1) {
		g_LexWarnings++;
		label = "Warning";
	} else if (level == 2) {
		label = "Info";
	} else {
		label = NULL;
		CHECK(0);
	}
	if (!g_Lex) {
		Lex_Print("\n");
	} else {
		Lex_Print("\n%s (%d): %s: ", g_Lex->name, g_Lex->line, label);
	}
	if (fmt) {
		va_start(ap, fmt);
		Lex_VPrint(fmt, ap);
		va_end(ap);
	}
}

void Lex_Summary(void) {
	CHECK(g_Lex);
	Lex_Print("\n%s: ", g_Lex->name);
	Lex_Print("%d line%s", g_Lex->line, g_Lex->line == 1 ? "" : "s");
	if (g_Lex->included) {
		Lex_Print(", %d included", g_Lex->included, g_Lex->included == 1 ? "" : "s");
	}
	Lex_Print(", %d error%s", g_LexErrors, g_LexErrors == 1 ? "" : "s");
	Lex_Print(", %d warning%s", g_LexWarnings, g_LexWarnings == 1 ? "" : "s");
}

void Lex_SetOut(lexout_t out) {
	g_LexOut = out;
}

/* the directory of name, made absolute */
void Lex_SetDir(const char *name) {
	Ruta_Part(g_LexDir, name, RUTA_DRIVE | RUTA_DIR);
	Ruta_Full(g_LexDir, g_LexDir, MAX_RUTA, NULL);
}

int Lex_Next(void) {
	CHECK(g_Lex);
	for (;;) {
		g_LexTokenLen = 0;
		g_LexToken[0] = '\0';
		if (g_LexCh == -1) {
			if (!g_Lex->parent) {
				return CLEX_ERR;
			}
			Lex_Pop();
			continue;
		}
		if (g_LexCh == '\t' || g_LexCh == ' ') {
			Lex_SkipBlanks();
			continue;
		}
		if (crt_isdigit(g_LexCh) || (g_LexCh == '.' && crt_isdigit(Lex_Peek()))) {
			g_Lex->tokens++;
			return Lex_Number();
		}
		if (crt_isalpha(g_LexCh) || g_LexCh == '_') {
			Lex_Ident();
			g_Lex->tokens++;
			return CLEX_IDENT;
		}
		if (g_LexCh == '"') {
			Lex_String();
			g_Lex->tokens++;
			return CLEX_STRING;
		}
		if (g_LexCh == '#' && g_Lex->tokens == 0) {
			Lex_Directive();
		} else if (g_LexCh == '\n' || g_LexCh == '\r') {
			Lex_NewLine();
		} else if (g_LexCh != '/') {
			Lex_Add(g_LexCh);
			Lex_NextCh();
			g_Lex->tokens++;
			return CLEX_CHAR;
		} else {
			Lex_NextCh();
			if (g_LexCh == '*') {
				Lex_Comment();
			} else if (g_LexCh == '/') {
				do {
					Lex_NextCh();
				} while (g_LexCh != -1 && g_LexCh != '\n' && g_LexCh != '\r');
			} else {
				CHECK(0);	/* a '/' that starts no comment stops the original too */
			}
		}
	}
}
