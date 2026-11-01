/*
 * The vertex and pixel shader assembler of D3DX 8.1 ("D3DX8 Shader Assembler Version 0.91", D3DXAssembleShader at rva
 * 0x00b2c2 of Scrap.exe), for the pixel shaders of the game (the .psh files of bmp/, ps.1.1). Its parser follows the Berkeley yacc
 * parser of the exe (see Asm_Parse), so a text with errors stops at the same token with the same message.
 *
 * The lexer gives names, numbers and single characters; a name is a keyword (vs, ps, def), an opcode with its
 * modifiers (_x2, _sat ...) or anything else (a register, a mask, a swizzle). The rules work on stacks of their own:
 * names with their lines, integers, floats, register tokens, opcodes, the def instructions and the code. At the end
 * the code buffer is the version token, a comment with the name of the assembler, the def instructions (pixel
 * shaders), the code and the end token, then d3d8.dll (d3d8d.dll with the registry value LoadDebugRuntime) checks it
 * with ValidatePixelShader or ValidateVertexShader. The vertex constants of def go to the constant buffer instead.
 *
 * The buffers are ID3DXBuffer objects of this file. The hook DLL runs this too.
 */
#include <string.h>

/* before windows.h: d3d8.h (through e3dutil.h) has its own E_FAIL */
#include "scrap/crt.h"
#include "scrap/e3dutil.h"

#include <windows.h>

#define D3DERR_INVALIDCALL ((int)0x8876086c)
#define D3DXERR_INVALIDDATA ((int)0x88760b59)
#define ASM_E_OUTOFMEMORY ((int)0x8007000e)
#define ASM_E_FAIL ((int)0x80004005)

#define D3DXASM_DEBUG 1
#define D3DXASM_SKIPVALIDATION 2

enum {
	TK_VS = 0x101, TK_PS, TK_DEF, TK_OP0, TK_OP1, TK_OP2, TK_OP3, TK_OP4, TK_NAME, TK_INT, TK_FLOAT, TK_END
};

/* register token fields (d3d8types.h) */
#define REG_TYPE 0x70000000
#define REG_CONST 0x20000000
#define REG_RELATIVE 0x2000
#define REG_MOD 0x0f000000
#define VERSION_PS14 0xffff0104

static void *Asm_Alloc(size_t n) {
	return HeapAlloc(GetProcessHeap(), 0, n ? n : 1);
}

static void Asm_Free(void *p) {
	if (p) {
		HeapFree(GetProcessHeap(), 0, p);
	}
}

/* ---- ID3DXBuffer ---- */

typedef struct d3dxBuffer_s d3dxBuffer_t;
typedef struct {
	int(__stdcall *QueryInterface)(d3dxBuffer_t *self, const GUID *iid, void **out);
	ULONG(__stdcall *AddRef)(d3dxBuffer_t *self);
	ULONG(__stdcall *Release)(d3dxBuffer_t *self);
	void *(__stdcall *GetBufferPointer)(d3dxBuffer_t *self);
	uint32_t(__stdcall *GetBufferSize)(d3dxBuffer_t *self);
} d3dxBufferVtbl_t;

struct d3dxBuffer_s {
	const d3dxBufferVtbl_t *vtbl;
	ULONG refs;
	uint32_t size;
	void *data;
};

static const GUID iid_IUnknown = { 0x00000000, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
static const GUID iid_ID3DXBuffer = { 0x932e6a7e, 0xc68e, 0x45dd, { 0xa7, 0xbf, 0x53, 0xd1, 0x9c, 0x86, 0xdb, 0x1f } };

static int __stdcall Buffer_QueryInterface(d3dxBuffer_t *self, const GUID *iid, void **out) {
	*out = NULL;
	if (memcmp(iid, &iid_IUnknown, sizeof(GUID)) && memcmp(iid, &iid_ID3DXBuffer, sizeof(GUID))) {
		return (int)0x80004002;
	}
	*out = self;
	self->vtbl->AddRef(self);
	return 0;
}

static ULONG __stdcall Buffer_AddRef(d3dxBuffer_t *self) {
	return ++self->refs;
}

static ULONG __stdcall Buffer_Release(d3dxBuffer_t *self) {
	ULONG n = --self->refs;
	if (!n) {
		Asm_Free(self->data);
		Asm_Free(self);
	}
	return n;
}

static void *__stdcall Buffer_GetBufferPointer(d3dxBuffer_t *self) {
	return self->data;
}

static uint32_t __stdcall Buffer_GetBufferSize(d3dxBuffer_t *self) {
	return self->size;
}

static const d3dxBufferVtbl_t buffer_vtbl = {
	Buffer_QueryInterface, Buffer_AddRef, Buffer_Release, Buffer_GetBufferPointer, Buffer_GetBufferSize
};

static d3dxBuffer_t *Buffer_New(uint32_t size) {
	d3dxBuffer_t *b = Asm_Alloc(sizeof(*b));
	if (!b) {
		return NULL;
	}
	b->vtbl = &buffer_vtbl;
	b->refs = 1;
	b->size = size;
	b->data = Asm_Alloc(size);
	if (!b->data) {
		Asm_Free(b);
		return NULL;
	}
	return b;
}

/* ---- the stacks of the rules ---- */

typedef struct {
	uint32_t *v;
	uint32_t n, cap;
	int err;	/* the last failure, read and cleared after each rule */
} asmStack_t;

typedef struct {
	char **v;
	uint32_t n, cap;
	int err;
	uint32_t len;	/* all characters, for the error buffer */
} asmStrings_t;

static int Stack_Grow(void **v, uint32_t n, uint32_t *cap, int *err) {
	if (n < *cap) {
		return 1;
	}
	void *p = Asm_Alloc((*cap + 16) * sizeof(uint32_t));
	if (!p) {
		*err = ASM_E_OUTOFMEMORY;
		return 0;
	}
	if (*v) {
		memcpy(p, *v, n * sizeof(uint32_t));
		Asm_Free(*v);
	}
	*v = p;
	*cap += 16;
	return 1;
}

static void Stack_Push(asmStack_t *s, uint32_t x) {
	if (Stack_Grow((void **)&s->v, s->n, &s->cap, &s->err)) {
		s->v[s->n++] = x;
	}
}

static uint32_t Stack_Pop(asmStack_t *s) {
	if (!s->n) {
		s->err = ASM_E_FAIL;
		return 0;
	}
	return s->v[--s->n];
}

static void Stack_PushF(asmStack_t *s, float f) {
	uint32_t x;
	memcpy(&x, &f, 4);
	Stack_Push(s, x);
}

static float Stack_PopF(asmStack_t *s) {
	uint32_t x = Stack_Pop(s);
	float f;
	memcpy(&f, &x, 4);
	return f;
}

static int Stack_Error(asmStack_t *s) {
	int e = s->err;
	s->err = 0;
	return e;
}

static void Strings_Push(asmStrings_t *s, const char *str) {
	if (!Stack_Grow((void **)&s->v, s->n, &s->cap, &s->err)) {
		return;
	}
	size_t n = strlen(str);
	char *p = Asm_Alloc(n + 1);
	if (!p) {
		s->err = ASM_E_OUTOFMEMORY;
		return;
	}
	memcpy(p, str, n + 1);
	s->len += (uint32_t)n;
	s->v[s->n++] = p;
}

static char *Strings_Pop(asmStrings_t *s) {
	if (!s->n) {
		s->err = ASM_E_FAIL;
		return NULL;
	}
	return s->v[--s->n];
}

static int Strings_Error(asmStrings_t *s) {
	int e = s->err;
	s->err = 0;
	return e;
}

static void Strings_Free(asmStrings_t *s) {
	for (uint32_t i = 0; i < s->n; i++) {
		Asm_Free(s->v[i]);
	}
	Asm_Free(s->v);
}

/* the strings joined with a newline after each, NULL when they have no characters */
static int Strings_Buffer(const asmStrings_t *s, void **out) {
	*out = NULL;
	if (!s->len) {
		return 0;
	}
	d3dxBuffer_t *b = Buffer_New(s->n + 1 + s->len);
	if (!b) {
		return ASM_E_OUTOFMEMORY;
	}
	char *p = b->data;
	for (uint32_t i = 0; i < s->n; i++) {
		if (s->v[i]) {
			size_t n = strlen(s->v[i]);
			memcpy(p, s->v[i], n);
			p[n] = '\n';
			p += n + 1;
		}
	}
	*p = 0;
	*out = b;
	return 0;
}

/* ---- the assembler ---- */

typedef struct {
	asmStrings_t names;	/* names of the rules, with their lines in lines */
	asmStack_t lines;
	asmStack_t ints;
	asmStack_t floats;
	asmStack_t params;	/* register tokens */
	asmStack_t ops;		/* opcode, then its destination modifiers */
	asmStack_t defs;
	asmStack_t code;
	asmStrings_t errors;
	int failed;
	uint32_t defBytes, codeBytes;
	int errorCount;
	int pixel;
	int debug;
	uint32_t version;
	const char *src, *lineStart, *pos, *end;
	char tok[256];
	uint32_t op, opMods;	/* of the last opcode the lexer read */
	int line;
	char file[256];
	int lastLine;		/* of the debug comments */
	char lastFile[256];
	int stmtToken;		/* the last token starts a statement: a keyword, an opcode, '+' or the end */
	int stmtLine;		/* the line where the current statement began */
	int nameLine;		/* the line of the name a rule works on, for its errors */
	int useNameLine;
} d3dxAsm_t;

static void Asm_Error(d3dxAsm_t *a, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/* rva 0x01b1c7: "file(line) : (Assembly Error) message"; a syntax error at the start of a statement tells more */
static void Asm_Error(d3dxAsm_t *a, const char *fmt, ...) {
	char msg[256], line[256];
	va_list ap;

	va_start(ap, fmt);
	VBprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	if (a->stmtToken && !strcmp(msg, "syntax error")) {
		if (!a->stmtLine) {
			Bprintf(line, sizeof(line), "%s(%d) : (Assembly Error) shader version expected", a->file, a->line);
		} else if (a->codeBytes && !strcmp(a->tok, "def")) {
			Bprintf(line, sizeof(line), "%s(%d) : (Assembly Error) constant definition in shader body", a->file,
				a->line);
		} else {
			Bprintf(line, sizeof(line), "%s(%d) : (Assembly Error) statement incomplete", a->file, a->stmtLine);
		}
	} else {
		int n = a->useNameLine ? a->nameLine : a->line;
		Bprintf(line, sizeof(line), "%s(%d) : (Assembly Error) %s", a->file, n ? n : 1, msg);
	}
	Strings_Push(&a->errors, line);
	a->errorCount++;
	a->failed = 1;
}

static void Asm_PushCode(d3dxAsm_t *a, uint32_t x) {
	Stack_Push(&a->code, x);
	a->codeBytes += 4;
}

/* a comment token (tag, then the bytes of text, the last dword padded with zeros) */
static void Asm_Comment(d3dxAsm_t *a, uint32_t tag, const byte *text, uint32_t n, uint32_t sizeBytes) {
	Asm_PushCode(a, ((((sizeBytes + 3) & 0x3fffc) * 0x4000 + 0x10000) & 0x7fff0000) | 0xfffe);
	Asm_PushCode(a, tag);
	for (; n > 3; n -= 4, text += 4) {
		uint32_t x;
		memcpy(&x, text, 4);
		Asm_PushCode(a, x);
	}
	if (n) {
		uint32_t x = 0;
		memcpy(&x, text, n);
		Asm_PushCode(a, x);
	}
}

/* rva 0x01bfa8: with D3DXASM_DEBUG and no file name, the source text as a comment */
static void Asm_TextComment(d3dxAsm_t *a) {
	if (a->debug) {
		uint32_t n = (uint32_t)(a->end - a->src);
		Asm_Comment(a, 0x54584554, (const byte *)a->src, n, n);
	}
}

/* rva 0x01c07e: at each instruction the line; with D3DXASM_DEBUG the file and line comments when they change */
static void Asm_DebugLine(d3dxAsm_t *a) {
	a->stmtLine = a->line;
	if (!a->debug) {
		a->lastLine = a->line;
		return;
	}
	if (strcmp(a->lastFile, a->file)) {
		uint32_t n = (uint32_t)strlen(a->file);
		Asm_Comment(a, 0x454c4946, (const byte *)a->file, n + 1, n + 1);
		memcpy(a->lastFile, a->file, sizeof(a->file));
	}
	if (a->lastLine != a->line) {
		Asm_PushCode(a, 0x2fffe);
		Asm_PushCode(a, 0x454e494c);
		Asm_PushCode(a, (uint32_t)a->line);
		a->lastLine = a->line;
	}
}

static int Asm_Is(const char *s, const char *word) {
	return !strcmp(s, word);
}

typedef struct {
	const char *name;
	uint16_t op;
	uint16_t token;
} asmOpcode_t;

/* the opcodes in the order the exe tries them */
static const asmOpcode_t asm_common[] = {
	{ "nop", 0, TK_OP0 }, { "mov", 1, TK_OP2 }, { "add", 2, TK_OP3 }, { "sub", 3, TK_OP3 }, { "mad", 4, TK_OP4 },
	{ "mul", 5, TK_OP3 }, { "dp3", 8, TK_OP3 }, { "dp4", 9, TK_OP3 }, { "frc", 0x13, TK_OP2 },
};
static const asmOpcode_t asm_pixel[] = { { "lrp", 0x12, TK_OP4 }, { "cnd", 0x50, TK_OP4 }, { "cmp", 0x58, TK_OP4 } };
static const asmOpcode_t asm_ps10[] = {
	{ "texcoord", 0x40, TK_OP1 }, { "texkill", 0x41, TK_OP1 }, { "tex", 0x42, TK_OP1 }, { "texbem", 0x43, TK_OP2 },
	{ "texbeml", 0x44, TK_OP2 }, { "texreg2ar", 0x45, TK_OP2 }, { "texreg2gb", 0x46, TK_OP2 },
	{ "texm3x2pad", 0x47, TK_OP2 }, { "texm3x2tex", 0x48, TK_OP2 }, { "texm3x3pad", 0x49, TK_OP2 },
	{ "texm3x3tex", 0x4a, TK_OP2 }, { "texm3x3spec", 0x4c, TK_OP3 }, { "texm3x3vspec", 0x4d, TK_OP2 },
	{ "texm3x2depth", 0x54, TK_OP2 }, { "texdp3", 0x55, TK_OP2 }, { "texreg2rgb", 0x52, TK_OP2 },
	{ "texm3x3", 0x56, TK_OP2 }, { "texdp3tex", 0x53, TK_OP2 },
};
static const asmOpcode_t asm_ps14[] = {
	{ "phase", 0xfffd, TK_OP0 }, { "texcrd", 0x40, TK_OP2 }, { "texkill", 0x41, TK_OP1 },
	{ "texdepth", 0x57, TK_OP1 }, { "texld", 0x42, TK_OP2 }, { "bem", 0x59, TK_OP3 },
};
static const asmOpcode_t asm_vertex[] = {
	{ "rcp", 6, TK_OP2 }, { "rsq", 7, TK_OP2 }, { "min", 10, TK_OP3 }, { "max", 11, TK_OP3 }, { "slt", 12, TK_OP3 },
	{ "sge", 13, TK_OP3 }, { "exp", 14, TK_OP2 }, { "log", 15, TK_OP2 }, { "lit", 16, TK_OP2 }, { "dst", 17, TK_OP3 },
	{ "m4x4", 0x14, TK_OP3 }, { "m4x3", 0x15, TK_OP3 }, { "m3x4", 0x16, TK_OP3 }, { "m3x3", 0x17, TK_OP3 },
	{ "m3x2", 0x18, TK_OP3 }, { "expp", 0x4e, TK_OP2 }, { "logp", 0x4f, TK_OP2 },
};

static const asmOpcode_t *Asm_FindOp(const asmOpcode_t *t, size_t n, const char *s) {
	for (size_t i = 0; i < n; i++) {
		if (Asm_Is(s, t[i].name)) {
			return &t[i];
		}
	}
	return NULL;
}

#define FIND_OP(t, s) Asm_FindOp(t, sizeof(t) / sizeof(t[0]), s)

/* rva 0x01b2fd: the opcode of the name in tok with "_x2" and "_sat" style modifiers, or TK_NAME */
static int Asm_Opcode(d3dxAsm_t *a) {
	static const struct {
		const char *name;
		uint32_t mod;
	} shifts[] = {
		{ "x8", 0x3000000 }, { "x4", 0x2000000 }, { "x2", 0x1000000 }, { "d2", 0xf000000 }, { "d4", 0xe000000 },
		{ "d8", 0xd000000 },
	};
	const char *p = a->tok, *s1 = NULL, *s1e = NULL, *s2 = NULL, *s2e = NULL;
	char word[16];
	uint32_t mods = 0;

	while (*p && *p != '_') {
		p++;
	}
	if (*p) {
		s1 = s1e = p + 1;
		while (*s1e && *s1e != '_') {
			s1e++;
		}
		if (*s1e) {
			s2 = s2e = s1e + 1;
			while (*s2e) {
				s2e++;
			}
		}
	}
	size_t n = (size_t)(p - a->tok);
	if (n > 15) {
		return TK_NAME;
	}
	memcpy(word, a->tok, n);
	word[n] = 0;
	const asmOpcode_t *o = FIND_OP(asm_common, word);
	if (!o && a->pixel) {
		o = FIND_OP(asm_pixel, word);
		if (!o) {
			o = a->version < VERSION_PS14 ? FIND_OP(asm_ps10, word) : FIND_OP(asm_ps14, word);
		}
		if (!o) {
			return TK_NAME;
		}
	}
	if (!o) {
		o = FIND_OP(asm_vertex, word);
		if (!o) {
			return TK_NAME;
		}
	}
	if (s1) {
		if (!a->pixel || (size_t)(s1e - s1) > 15) {
			return TK_NAME;
		}
		memcpy(word, s1, (size_t)(s1e - s1));
		word[s1e - s1] = 0;
		size_t i = 0;
		while (i < sizeof(shifts) / sizeof(shifts[0]) && !Asm_Is(word, shifts[i].name)) {
			i++;
		}
		if (i < sizeof(shifts) / sizeof(shifts[0])) {
			mods = shifts[i].mod;
		} else {
			if (s2) {
				return TK_NAME;
			}
			s2 = s1;
			s2e = s1e;
		}
	}
	if (s2) {
		if (!a->pixel || (size_t)(s2e - s2) >= 16) {
			return TK_NAME;
		}
		memcpy(word, s2, (size_t)(s2e - s2));
		word[s2e - s2] = 0;
		if (!Asm_Is(word, "sat")) {
			return TK_NAME;
		}
		mods |= 0x100000;
	}
	a->op = o->op;
	a->opMods = mods;
	return o->token;
}

/* the token text from start to the lexer position, at most 255 characters */
static void Asm_SetTok(d3dxAsm_t *a, const char *start) {
	size_t n = (size_t)(a->pos - start);
	if (n > 255) {
		n = 255;
	}
	memcpy(a->tok, start, n);
	a->tok[n] = 0;
}

/* "#line n "file"" sets the line of the next line and the file of the messages */
static void Asm_LineDirective(d3dxAsm_t *a, const char *start) {
	const char *p = start, *eol = a->pos;

	while (p < eol && !crt_isspace(*p)) {
		p++;
	}
	while (p < eol && crt_isspace(*p)) {
		p++;
	}
	const char *num = p;
	while (p < eol && !crt_isspace(*p)) {
		p++;
	}
	const char *q1 = p;
	while (q1 < eol && *q1 != '"') {
		q1++;
	}
	const char *q2 = q1;
	if (q1 < eol && (q2 = q1 + 1) < eol) {
		while (q2 < eol && *q2 != '"') {
			q2++;
		}
	}
	size_t n = (size_t)(p - num);
	if (n > 255) {
		n = 255;
	}
	if (n) {
		char buf[256];
		memcpy(buf, num, n);
		buf[n] = 0;
		a->line = crt_atoi(buf) - 1;
	}
	if (q2 - q1 > 1) {
		char *d = a->file;
		for (const char *s = q1 + 1; s < q2 && d < a->file + sizeof(a->file) - 1;) {
			char c = *s++;
			*d++ = c;
			if (c == '\\' && s < q2 && *s == '\\') {
				s++;
			}
		}
		*d = 0;
	}
}


static int Asm_Lex(d3dxAsm_t *a) {
	a->stmtToken = 0;
	for (;;) {
		if (a->pos >= a->end) {
			if (a->pos == a->end) {
				a->pos++;
				a->stmtToken = 1;
				return TK_END;
			}
			return -1;
		}
		char c = *a->pos;
		if (c == '\n') {
			a->line++;
			a->lineStart = ++a->pos;
			continue;
		}
		if (crt_isspace(c)) {
			a->pos++;
			continue;
		}
		if (c == '#' && a->pos == a->lineStart) {
			const char *start = a->pos;
			for (a->pos++; a->pos < a->end && *a->pos != '\n'; a->pos++) {
			}
			if (a->pos - start > 5 && !memcmp(start, "#line", 5)) {
				Asm_LineDirective(a, start);
			} else {
				Asm_Error(a, "preprocessor directives are not supported");
			}
			continue;
		}
		if (c == ';' || (c == '/' && a->pos + 1 < a->end && a->pos[1] == '/')) {
			for (a->pos++; a->pos < a->end && *a->pos != '\n'; a->pos++) {
			}
			continue;
		}
		if (c == '/' && a->pos + 1 < a->end && a->pos[1] == '*') {
			int closed = 0;
			a->pos += 2;
			while (a->pos < a->end) {
				const char *p = a->pos;
				if (*p == '*' && p + 1 < a->end && p[1] == '/') {
					a->pos += 2;
					closed = 1;
					break;
				}
				if (*p == '\n') {
					a->line++;
					a->lineStart = p + 1;
				}
				a->pos = p + 1;
			}
			if (!closed) {
				Asm_Error(a, "comment extends past end-of-file");
			}
			continue;
		}
		const char *start = a->pos;
		if (crt_isalpha(c)) {
			for (a->pos++; a->pos < a->end && (crt_isalnum(*a->pos) || *a->pos == '_'); a->pos++) {
			}
			Asm_SetTok(a, start);
			a->stmtToken = 1;
			if (Asm_Is(a->tok, "vs")) {
				return TK_VS;
			}
			if (Asm_Is(a->tok, "ps")) {
				return TK_PS;
			}
			if (Asm_Is(a->tok, "def")) {
				return TK_DEF;
			}
			int t = Asm_Opcode(a);
			if (t != TK_NAME) {
				return t;
			}
			a->stmtToken = 0;
			return TK_NAME;
		}
		if (crt_isdigit(c)) {
			int t = TK_INT;
			for (a->pos++; a->pos < a->end; a->pos++) {
				char d = *a->pos;
				if (crt_isdigit(d)) {
					continue;
				}
				if (!(a->version && (d == '.' || d == 'e')) && !((d == '+' || d == '-') && a->pos[-1] == 'e')) {
					break;
				}
				t = TK_FLOAT;
			}
			Asm_SetTok(a, start);
			if (a->version && a->pos < a->end && *a->pos == 'f') {
				t = TK_FLOAT;
				a->pos++;
			}
			return t;
		}
		if (c == '+') {
			a->stmtToken = 1;
		}
		a->pos++;
		return c;
	}
}

static uint32_t Asm_Lane(char c) {
	switch (c) {
	case 'x':
	case 'r':
		return 0;
	case 'y':
	case 'g':
		return 1;
	case 'z':
	case 'b':
		return 2;
	case 'w':
	case 'a':
		return 3;
	}
	return 4;
}

/* rva 0x01ba14: a write mask in rgba or xyzw order */
static uint32_t Asm_Mask(d3dxAsm_t *a, const char *s) {
	uint32_t m = 0, prev = 0;

	if (!s || !*s) {
		return 0xf0000;
	}
	for (const char *p = s; *p; p++) {
		uint32_t i = Asm_Lane(*p);
		if (i > 3 || (p != s && i <= prev)) {
			Asm_Error(a, "invalid mask '%s'", s);
			a->failed = 1;
			return 0;
		}
		m |= 0x10000u << i;
		prev = i;
	}
	return m;
}

/* rva 0x01babd: a swizzle of up to four lanes, the last one repeated */
static uint32_t Asm_Swizzle(d3dxAsm_t *a, const char *s) {
	uint32_t r = 0, i = 0;
	const char *p = s;

	if (!s || !*s) {
		return 0xe40000;
	}
	for (int shift = 16; shift < 24; shift += 2) {
		if (*p) {
			i = Asm_Lane(*p++);
			if (i > 3) {
				goto bad;
			}
		}
		r |= i << shift;
	}
	if (!*p) {
		return r;
	}
bad:
	Asm_Error(a, "invalid swizzle '%s'", s);
	a->failed = 1;
	return 0;
}

/* rva 0x01bb5e: a register name (with a source modifier suffix in pixel shaders), or c[index] */
static uint32_t Asm_Register(d3dxAsm_t *a, char *s, uint32_t index, int indexed) {
	static const struct {
		const char *suffix;
		uint32_t mod;
	} mods[] = {
		{ "_bias", 0x2000000 }, { "_bx2", 0x4000000 }, { "_x2", 0x7000000 }, { "_dz", 0x9000000 },
		{ "_db", 0x9000000 }, { "_dw", 0xa000000 }, { "_da", 0xa000000 },
	};
	char *p = s;
	uint32_t num = 0, mod = 0;
	int ok = 1;

	while (*p && crt_isalpha(*p)) {
		p++;
	}
	char *q = p;
	while (*q && crt_isdigit(*q)) {
		q++;
	}
	int more = *p != 0;
	if (a->pixel && *q == '_') {
		for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
			if (Asm_Is(q, mods[i].suffix)) {
				mod = mods[i].mod;
				q += strlen(mods[i].suffix);
				break;
			}
		}
	}
	if (more) {
		num = (uint32_t)crt_atoi(p);
		if (num & 0xffffe000) {
			ok = 0;
		}
	}
	if (!*q && ok) {
		if (indexed) {
			if (Asm_Is(s, "c") && !more) {
				return mod | index | REG_CONST;
			}
			goto badIndexed;
		}
		char saved = *p;
		uint32_t r = 0;
		int found = 1;
		*p = 0;
		if (!a->pixel) {
			if (Asm_Is(s, "oPos") && !more) {
				r = 0x40000000;
			} else if (Asm_Is(s, "oPts") && !more) {
				r = 0x40000002;
			} else if (Asm_Is(s, "oFog") && !more) {
				r = 0x40000001;
			} else if (Asm_Is(s, "v") && more) {
				r = num | 0x10000000;
			} else if (Asm_Is(s, "r") && more) {
				r = num;
			} else if (Asm_Is(s, "c") && more) {
				r = num | REG_CONST;
			} else if (Asm_Is(s, "a") && more) {
				r = num | 0x30000000;
			} else if (Asm_Is(s, "oD") && more) {
				r = num | 0x50000000;
			} else if (Asm_Is(s, "oT") && more) {
				r = num | 0x60000000;
			} else {
				found = 0;
			}
		} else if (Asm_Is(s, "v") && more) {
			r = mod | num | 0x10000000;
		} else if (Asm_Is(s, "r") && more) {
			r = mod | num;
		} else if (Asm_Is(s, "c") && more) {
			r = mod | num | REG_CONST;
		} else if (Asm_Is(s, "t") && more) {
			r = mod | num | 0x30000000;
		} else {
			found = 0;
		}
		*p = saved;
		if (found) {
			return r;
		}
	} else if (indexed) {
		goto badIndexed;
	}
	Asm_Error(a, "invalid register '%s'", s);
	a->failed = 1;
	return 0;
badIndexed:
	if (index & REG_RELATIVE) {
		Asm_Error(a, "invalid register '%s[a0.x + %d]'", s, (int)(index & 0x1fff));
	} else {
		Asm_Error(a, "invalid register '%s[%d]'", s, (int)(index & 0x1fff));
	}
	a->failed = 1;
	return 0;
}

/* rva 0x01beff: def c#, x, y, z, w into the def list (pixel shaders) or the constants (vertex shaders) */
static void Asm_Def(d3dxAsm_t *a, uint32_t reg, const uint32_t v[4]) {
	if ((reg & REG_TYPE) != REG_CONST || (reg & REG_RELATIVE)) {
		Asm_Error(a, "invalid constant");
		a->failed = 1;
		return;
	}
	if (a->pixel) {
		a->defBytes += 0x18;
		Stack_Push(&a->defs, 0x51);
		Stack_Push(&a->defs, reg | 0xf0000);
	} else {
		a->defBytes += 0x14;
		Stack_Push(&a->defs, (reg & 0x1fff) | 0x82000000);
	}
	for (int i = 0; i < 4; i++) {
		Stack_Push(&a->defs, v[i]);
	}
}

static int Asm_ConstPS14(const d3dxAsm_t *a, uint32_t reg) {
	return a->version == VERSION_PS14 && (reg & REG_TYPE) == REG_CONST;
}

/* x87 fild, + 2^32 for a negative value, fstp: the integer read as unsigned */
static float Asm_IntFloat(uint32_t v) {
	long double f = (int32_t)v;
	if ((int32_t)v < 0) {
		f += 4294967296.0f;
	}
	return (float)f;
}

static void Asm_Version(d3dxAsm_t *a, int pixel) {
	uint32_t minor = Stack_Pop(&a->ints), major = Stack_Pop(&a->ints);
	if ((major | minor) & 0xffffff00) {
		Asm_Error(a, "invalid version");
		return;
	}
	a->pixel = pixel;
	a->version = ((major | (pixel ? 0xffffff00 : 0xfffffe00)) << 8) | minor;
	a->stmtLine = a->line;
}

/* the instruction of the opcode on the op stack and n register tokens */
static void Asm_Instruction(d3dxAsm_t *a, int n) {
	uint32_t p[4];
	for (int i = n - 1; i >= 0; i--) {
		p[i] = Stack_Pop(&a->params);
	}
	uint32_t mods = Stack_Pop(&a->ops), op = Stack_Pop(&a->ops);
	if (n == 3 && !a->pixel && (op & 0xffff) == 3) {
		/* sub in a vertex shader: add with the second source negated */
		op = (op & 0xffff0002) | 2;
		p[2] ^= 0x1000000;
	}
	Stack_Push(&a->code, op);
	if (n) {
		Stack_Push(&a->code, p[0] | mods);
	}
	for (int i = 1; i < n; i++) {
		Stack_Push(&a->code, p[i]);
	}
	a->codeBytes += 4 + 4 * (uint32_t)n;
}

/* rva 0x01c710: the action of a rule (the code of the exe is the rule number plus one from rule 3 on) */
static void Asm_Rule(d3dxAsm_t *a, int rule) {
	uint32_t v, w;
	char *s;

	switch (rule) {
	case 3:
		Asm_Version(a, 0);
		break;
	case 4:
		Asm_Version(a, 1);
		break;
	case 9: {
		uint32_t f[4];
		uint32_t reg = Stack_Pop(&a->params);
		for (int i = 3; i >= 0; i--) {
			f[i] = Stack_Pop(&a->floats);
		}
		Asm_Def(a, reg, f);
		break;
	}
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
		Asm_Instruction(a, rule - 12);
		break;
	case 17:
		Stack_Push(&a->params, Stack_Pop(&a->params) | 0xf0000);
		break;
	case 18:
		s = Strings_Pop(&a->names);
		a->nameLine = (int)Stack_Pop(&a->lines);
		a->useNameLine = 1;
		v = Stack_Pop(&a->params);
		v = Asm_Mask(a, s) | (v & 0xfff0ffff);
		Stack_Push(&a->params, v);
		Asm_Free(s);
		a->useNameLine = 0;
		break;
	case 19:
		v = Stack_Pop(&a->params);
		Stack_Push(&a->params, v);
		if (Asm_ConstPS14(a, v) && (v & REG_MOD)) {
			Asm_Error(a, "constant modifiers not supported in ps.1.4");
		}
		break;
	case 20:
		v = Stack_Pop(&a->params);
		if (Asm_ConstPS14(a, v)) {
			Asm_Error(a, "constant modifiers not supported in ps.1.4");
		} else {
			switch (v & REG_MOD) {
			case 0:
				v = (v & 0xf1ffffff) | 0x1000000;
				break;
			case 0x2000000:
				v = (v & 0xf3ffffff) | 0x3000000;
				break;
			case 0x4000000:
				v = (v & 0xf5ffffff) | 0x5000000;
				break;
			case 0x7000000:
				v = (v & 0xf8ffffff) | 0x8000000;
				break;
			case 0x9000000:
			case 0xa000000:
				Asm_Error(a, "negate and divide modifiers cannot be combined");
				break;
			}
		}
		Stack_Push(&a->params, v);
		break;
	case 21:
		Stack_Pop(&a->ints);
		v = Stack_Pop(&a->params);
		if (!a->pixel) {
			Asm_Error(a, "complement not supported in vertex shaders");
		} else if (Asm_ConstPS14(a, v)) {
			Asm_Error(a, "constant modifiers not supported in ps.1.4");
		} else if (v & REG_MOD) {
			Asm_Error(a, "complement cannot be used with other modifiers");
		}
		Stack_Push(&a->params, (v & 0xf6ffffff) | 0x6000000);
		break;
	case 22:
		Stack_Push(&a->params, (Stack_Pop(&a->params) & 0xffe4ffff) | 0xe40000);
		break;
	case 23:
		s = Strings_Pop(&a->names);
		a->nameLine = (int)Stack_Pop(&a->lines);
		a->useNameLine = 1;
		v = Stack_Pop(&a->params);
		v = Asm_Swizzle(a, s) | (v & 0xff00ffff);
		Stack_Push(&a->params, v);
		Asm_Free(s);
		a->useNameLine = 0;
		break;
	case 24:
	case 25:
		s = Strings_Pop(&a->names);
		a->nameLine = (int)Stack_Pop(&a->lines);
		a->useNameLine = 1;
		if (rule == 24) {
			v = Asm_Register(a, s ? s : (char[]){ 0 }, 0, 0);
		} else {
			w = Stack_Pop(&a->params);
			v = Asm_Register(a, s ? s : (char[]){ 0 }, w, 1);
		}
		Stack_Push(&a->params, v | 0x80000000);
		Asm_Free(s);
		a->useNameLine = 0;
		break;
	case 26:
		v = Stack_Pop(&a->params);
		if (a->pixel) {
			Asm_Error(a, "syntax error");
		} else if (v != 0xb0000000) {
			Asm_Error(a, "illegal register in index expression");
		}
		Stack_Push(&a->params, REG_RELATIVE);
		break;
	case 27:
		v = Stack_Pop(&a->ints);
		if (v & 0xffffe000) {
			Asm_Error(a, "illegal register number");
		}
		Stack_Push(&a->params, v);
		break;
	case 28:
		v = Stack_Pop(&a->params);
		w = Stack_Pop(&a->params);
		if (a->pixel) {
			Asm_Error(a, "syntax error");
		} else if (w != 0xb0000000) {
			Asm_Error(a, "illegal register in index expression");
		} else if (v & REG_RELATIVE) {
			Asm_Error(a, "a0.x can only appear once in an index expression");
		}
		Stack_Push(&a->params, v | REG_RELATIVE);
		break;
	case 29:
		w = Stack_Pop(&a->ints);
		v = Stack_Pop(&a->params);
		if (((v & 0x1fff) + w) & 0xffffe000) {
			Asm_Error(a, "illegal register number");
		}
		Stack_Push(&a->params, (((w + v) ^ v) & 0x1fff) ^ v);
		break;
	case 30:
		a->stmtLine = a->line;
		break;
	case 31:
	case 33:
	case 35:
	case 37:
	case 39:
		Stack_Push(&a->ops, a->op);
		Stack_Push(&a->ops, a->opMods);
		Asm_DebugLine(a);
		break;
	case 32:
	case 34:
	case 36:
	case 38:
	case 40:
		if (!a->pixel) {
			Asm_Error(a, "coissue not supported in vertex shaders");
		}
		Stack_Push(&a->ops, a->op | 0x40000000);
		Stack_Push(&a->ops, a->opMods);
		Asm_DebugLine(a);
		break;
	case 41:
	case 42:
		Stack_PushF(&a->floats, Asm_IntFloat(Stack_Pop(&a->ints)));
		break;
	case 43:
		Stack_PushF(&a->floats, -Asm_IntFloat(Stack_Pop(&a->ints)));
		break;
	case 46:
		Stack_PushF(&a->floats, -Stack_PopF(&a->floats));
		break;
	case 47:
		Strings_Push(&a->names, a->tok);
		Stack_Push(&a->lines, (uint32_t)a->line);
		break;
	case 48:
		Stack_Push(&a->ints, (uint32_t)crt_atoi(a->tok));
		break;
	case 49: {
		float f = 0.0f;
		crt_sscanf(a->tok, "%f", &f);
		Stack_PushF(&a->floats, f);
		break;
	}
	}
	if (Strings_Error(&a->names) < 0 || Stack_Error(&a->lines) < 0 || Stack_Error(&a->ints) < 0
		|| Stack_Error(&a->params) < 0 || Stack_Error(&a->code) < 0 || Stack_Error(&a->floats) < 0) {
		Asm_Error(a, "Internal assembler error! %d", 0x3bb);
	}
}

/*
 * rva 0x01d047: the parser. The exe has a Berkeley yacc parser; this one walks the same grammar by hand and keeps what
 * a text with errors can see of it: the rules run in the same order, a token is read only where the yacc tables need
 * one, a rule that waits for a token runs only on the tokens its LALR state accepts (the union over all places the
 * rule is used, so a wrong token can still run it before the syntax error), and the depth of the yacc stack is
 * counted so a long text stops with the same overflow.
 *
 *	program : header END | header body END
 *	header  : VS '.' int '.' int | PS '.' int '.' int
 *	body    : instrs | defs instrs
 *	defs    : def | def defs
 *	def     : defkw reg ',' fv ',' fv ',' fv ',' fv
 *	instrs  : instr | instr instrs
 *	instr   : op0 | op1 dst | op2 dst ',' src | op3 dst ',' src ',' src | op4 dst ',' src ',' src ',' src
 *	dst     : reg | reg '.' name
 *	src     : sreg | '-' sreg | int '-' sreg
 *	sreg    : reg | reg '.' name
 *	reg     : name | name '[' index ']'
 *	index   : sreg | int | sreg '+' index | int '+' index
 *	defkw   : DEF
 *	opN     : OPN | '+' OPN
 *	fv      : int | '+' int | '-' int | float | '+' float | '-' float
 *	name    : NAME
 *	int     : INT
 *	float   : FLOAT
 *
 * Every function returns 0 after an error, which ends the parse.
 */
#define ASM_STACKSIZE 500

typedef struct {
	d3dxAsm_t *a;
	int token;	/* the lookahead, -1 when none was read */
	int depth;	/* of the yacc stack of the exe */
} asmParser_t;

static int Parse_Peek(asmParser_t *p) {
	if (p->token < 0 && (p->token = Asm_Lex(p->a)) < 0) {
		p->token = 0;
	}
	return p->token;
}

static int Parse_Error(asmParser_t *p) {
	Asm_Error(p->a, "syntax error");
	return 0;
}

static int Parse_Shift(asmParser_t *p) {
	if (p->depth >= ASM_STACKSIZE - 1) {
		Asm_Error(p->a, "yacc stack overflow");
		return 0;
	}
	p->depth++;
	p->token = -1;
	return 1;
}

static int Parse_Expect(asmParser_t *p, int token) {
	return Parse_Peek(p) == token ? Parse_Shift(p) : Parse_Error(p);
}

static int Parse_Reduce(asmParser_t *p, int rule, int len) {
	Asm_Rule(p->a, rule);
	p->depth -= len - 1;
	return 1;
}

static int Parse_IsOp(int t) {
	return t >= TK_OP0 && t <= TK_OP4;
}

/* the tokens after an instruction */
static int Parse_InstrEnd(int t) {
	return t == '+' || t == TK_END || Parse_IsOp(t);
}

static int Parse_Int(asmParser_t *p) {
	return Parse_Expect(p, TK_INT) && Parse_Reduce(p, 48, 1);
}

static int Parse_Name(asmParser_t *p) {
	return Parse_Expect(p, TK_NAME) && Parse_Reduce(p, 47, 1);
}

static int Parse_Index(asmParser_t *p);

static int Parse_Reg(asmParser_t *p) {
	if (!Parse_Name(p)) {
		return 0;
	}
	int t = Parse_Peek(p);
	if (t == '[') {
		return Parse_Shift(p) && Parse_Index(p) && Parse_Expect(p, ']') && Parse_Reduce(p, 25, 4);
	}
	if (Parse_InstrEnd(t) || t == ',' || t == '.' || t == ']') {
		return Parse_Reduce(p, 24, 1);
	}
	return Parse_Error(p);
}

/* dst (rules 17, 18) or sreg (22, 23); sreg is also used in an index, so ']' may follow it */
static int Parse_Masked(asmParser_t *p, int rule) {
	if (!Parse_Reg(p)) {
		return 0;
	}
	int t = Parse_Peek(p);
	if (t == '.') {
		return Parse_Shift(p) && Parse_Name(p) && Parse_Reduce(p, rule + 1, 3);
	}
	if (Parse_InstrEnd(t) || t == ',' || (rule == 22 && t == ']')) {
		return Parse_Reduce(p, rule, 1);
	}
	return Parse_Error(p);
}

static int Parse_Index(asmParser_t *p) {
	int t = Parse_Peek(p), rule;
	if (t == TK_INT) {
		rule = 27;
		if (!Parse_Int(p)) {
			return 0;
		}
	} else if (t == TK_NAME) {
		rule = 26;
		if (!Parse_Masked(p, 22)) {
			return 0;
		}
	} else {
		return Parse_Error(p);
	}
	t = Parse_Peek(p);
	if (t == '+') {
		return Parse_Shift(p) && Parse_Index(p) && Parse_Reduce(p, rule + 2, 3);
	}
	if (t == ']') {
		return Parse_Reduce(p, rule, 1);
	}
	return Parse_Error(p);
}

static int Parse_Src(asmParser_t *p) {
	switch (Parse_Peek(p)) {
	case '-':
		return Parse_Shift(p) && Parse_Masked(p, 22) && Parse_Reduce(p, 20, 2);
	case TK_INT:
		return Parse_Int(p) && Parse_Expect(p, '-') && Parse_Masked(p, 22) && Parse_Reduce(p, 21, 3);
	case TK_NAME:
		return Parse_Masked(p, 22) && Parse_Reduce(p, 19, 1);
	}
	return Parse_Error(p);
}

static int Parse_Fv(asmParser_t *p) {
	int t = Parse_Peek(p), sign = 0;
	if (t == '+' || t == '-') {
		sign = t == '+' ? 1 : 2;
		if (!Parse_Shift(p)) {
			return 0;
		}
		t = Parse_Peek(p);
	}
	if (t == TK_INT) {
		return Parse_Int(p) && Parse_Reduce(p, 41 + sign, sign ? 2 : 1);
	}
	if (t == TK_FLOAT) {
		return Parse_Shift(p) && Parse_Reduce(p, 49, 1) && Parse_Reduce(p, 44 + sign, sign ? 2 : 1);
	}
	return Parse_Error(p);
}

static int Parse_Def(asmParser_t *p) {
	if (!Parse_Shift(p) || !Parse_Reduce(p, 30, 1) || !Parse_Reg(p)) {
		return 0;
	}
	for (int i = 0; i < 4; i++) {
		if (!Parse_Expect(p, ',') || !Parse_Fv(p)) {
			return 0;
		}
	}
	return Parse_Reduce(p, 9, 10);
}

static int Parse_Instr(asmParser_t *p) {
	int t = Parse_Peek(p), coissue = 0;
	if (t == '+') {
		coissue = 1;
		if (!Parse_Shift(p)) {
			return 0;
		}
		t = Parse_Peek(p);
	}
	if (!Parse_IsOp(t)) {
		return Parse_Error(p);
	}
	int n = t - TK_OP0;
	if (!Parse_Shift(p) || !Parse_Reduce(p, 31 + 2 * n + coissue, 1 + coissue)) {
		return 0;
	}
	if (n && !Parse_Masked(p, 17)) {
		return 0;
	}
	for (int i = 1; i < n; i++) {
		if (!Parse_Expect(p, ',') || !Parse_Src(p)) {
			return 0;
		}
	}
	return Parse_Reduce(p, 12 + n, n ? 2 * n : 1);
}

/* defs (rules 7, 8, ended by an instruction) and instrs (10, 11, ended by END) are right recursive, so every item
   stays on the yacc stack until the list ends */
static int Parse_List(asmParser_t *p, int (*item)(asmParser_t *), int rule) {
	int defs = rule == 7, n = 0, t;
	do {
		if (!item(p)) {
			return 0;
		}
		n++;
		t = Parse_Peek(p);
	} while (defs ? t == TK_DEF : t != TK_END && Parse_InstrEnd(t));
	if (defs ? t == TK_END || !Parse_InstrEnd(t) : t != TK_END) {
		return Parse_Error(p);
	}
	Parse_Reduce(p, rule, 1);
	while (--n) {
		Parse_Reduce(p, rule + 1, 2);
	}
	return 1;
}

static int Parse_Program(asmParser_t *p) {
	int t = Parse_Peek(p);
	if (t != TK_VS && t != TK_PS) {
		return Parse_Error(p);
	}
	if (!Parse_Shift(p) || !Parse_Expect(p, '.') || !Parse_Int(p) || !Parse_Expect(p, '.') || !Parse_Int(p)
		|| !Parse_Reduce(p, t == TK_VS ? 3 : 4, 5)) {
		return 0;
	}
	t = Parse_Peek(p);
	if (t == TK_END) {
		return Parse_Shift(p) && Parse_Reduce(p, 1, 2);
	}
	if (t == TK_DEF) {
		if (!Parse_List(p, Parse_Def, 7) || !Parse_List(p, Parse_Instr, 10) || !Parse_Reduce(p, 6, 2)) {
			return 0;
		}
	} else if (Parse_InstrEnd(t)) {
		if (!Parse_List(p, Parse_Instr, 10) || !Parse_Reduce(p, 5, 1)) {
			return 0;
		}
	} else {
		return Parse_Error(p);
	}
	return Parse_Expect(p, TK_END) && Parse_Reduce(p, 2, 3);
}

/* 0 when the text was read to its end */
static int Asm_Parse(d3dxAsm_t *a) {
	asmParser_t p = { a, -1, 0 };
	if (!Parse_Program(&p)) {
		return 1;
	}
	if (Parse_Peek(&p)) {
		Parse_Error(&p);
		return 1;
	}
	return 0;
}

typedef int(__stdcall *validateVS_t)(const uint32_t *code, const uint32_t *decl, const void *caps, BOOL errors,
	char **text);
typedef int(__stdcall *validatePS_t)(const uint32_t *code, const void *caps, BOOL errors, char **text);

/* the validator of the Direct3D runtime the assembler uses (d3d8d.dll with LoadDebugRuntime set, else d3d8.dll) */
static HMODULE Asm_Runtime(void) {
	HKEY key;
	int debug = 0;

	if (!RegOpenKeyA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Direct3D", &key)) {
		DWORD type, val, size = 4;
		if (!RegQueryValueExA(key, "LoadDebugRuntime", NULL, &type, (LPBYTE)&val, &size) && type == REG_DWORD
			&& size == 4) {
			debug = val != 0;
		}
		RegCloseKey(key);
		if (debug) {
			HMODULE m = GetModuleHandleA("d3d8d.dll");
			if (m || (m = LoadLibraryA("d3d8d.dll"))) {
				return m;
			}
		}
	}
	HMODULE m = GetModuleHandleA("d3d8.dll");
	return m ? m : LoadLibraryA("d3d8.dll");
}

static void Asm_Validate(d3dxAsm_t *a, d3dxBuffer_t *code, void **errors) {
	HMODULE m = Asm_Runtime();
	char *text = NULL;
	int hr = 0;

	if (!m || !code) {
		return;
	}
	if (!a->pixel) {
		validateVS_t fn = (validateVS_t)(void *)GetProcAddress(m, "ValidateVertexShader");
		if (fn) {
			hr = fn(code->data, NULL, NULL, errors != NULL, errors ? &text : NULL);
		}
	} else {
		validatePS_t fn = (validatePS_t)(void *)GetProcAddress(m, "ValidatePixelShader");
		if (fn) {
			hr = fn(code->data, NULL, errors != NULL, errors ? &text : NULL);
		}
	}
	if (hr < 0) {
		a->failed = 1;
	}
	if (text) {
		Strings_Push(&a->errors, text);
		a->errorCount++;
		HeapFree(GetProcessHeap(), 0, text);
	}
}

static const char asm_name[36] = "D3DX8 Shader Assembler Version 0.91";


static int Asm_Run(d3dxAsm_t *a, const char *src, uint32_t len, int flags, void **constants, void **code,
	void **errors) {
	if (!src || !len) {
		return D3DERR_INVALIDCALL;
	}
	a->src = a->lineStart = a->pos = src;
	a->end = src + len;
	a->line = 1;
	a->debug = flags & D3DXASM_DEBUG;
	a->file[0] = 0;
	Asm_TextComment(a);

	const char *cur = crt_setlocale(4, NULL);	/* LC_NUMERIC */
	char *saved = cur ? crt_strdup(cur) : NULL;
	if (!saved || strcmp(saved, "C")) {
		crt_setlocale(4, "C");
	}
	if (Asm_Parse(a)) {
		a->failed = 1;
	}
	if (saved) {
		if (strcmp(saved, "C")) {
			crt_setlocale(4, saved);
		}
		crt_free(saved);
	}
	if (!a->failed) {
		if (constants && !a->pixel) {
			d3dxBuffer_t *b = Buffer_New(a->defBytes);
			if (!b) {
				return ASM_E_OUTOFMEMORY;
			}
			for (uint32_t n = a->defBytes; n; n -= 4) {
				((uint32_t *)b->data)[n / 4 - 1] = Stack_Pop(&a->defs);
			}
			*constants = b;
		}
		d3dxBuffer_t *b = NULL;
		if (code) {
			uint32_t size = a->codeBytes + 0x30 + (a->pixel ? a->defBytes : 0);
			b = Buffer_New(size);
			if (!b) {
				return ASM_E_OUTOFMEMORY;
			}
			uint32_t *w = b->data;
			w[0] = a->version;
			w[1] = 0x9fffe;
			memcpy(&w[2], asm_name, sizeof(asm_name));
			w[size / 4 - 1] = 0xffff;
			uint32_t i = size / 4 - 2;
			for (uint32_t n = a->codeBytes; n; n -= 4) {
				w[i--] = Stack_Pop(&a->code);
			}
			if (a->pixel) {
				for (; a->defBytes; a->defBytes -= 4) {
					w[i--] = Stack_Pop(&a->defs);
				}
			}
			*code = b;
		}
		if (!a->failed && !(flags & D3DXASM_SKIPVALIDATION)) {
			if (!a->codeBytes) {
				Asm_Error(a, "statement expected");
			} else {
				Asm_Validate(a, b, errors);
			}
		}
	}
	if (a->errorCount && errors) {
		int hr = Strings_Buffer(&a->errors, errors);
		if (hr < 0) {
			return hr;
		}
	}
	return a->failed ? D3DXERR_INVALIDDATA : 0;
}

/* rva 0x00b2c2: D3DXAssembleShader */
int __stdcall D3DX_AssembleShader(const void *src, uint32_t len, uint32_t flags, void **constants, void **code,
	void **errors) {
	d3dxAsm_t *a = Asm_Alloc(sizeof(*a));
	if (constants) {
		*constants = NULL;
	}
	if (code) {
		*code = NULL;
	}
	if (errors) {
		*errors = NULL;
	}
	if (!a) {
		return ASM_E_OUTOFMEMORY;
	}
	memset(a, 0, sizeof(*a));
	int hr = Asm_Run(a, src, len, (int)flags, constants, code, errors);
	Strings_Free(&a->names);
	Strings_Free(&a->errors);
	asmStack_t *stacks[] = { &a->lines, &a->ints, &a->floats, &a->params, &a->ops, &a->defs, &a->code };
	for (size_t i = 0; i < sizeof(stacks) / sizeof(stacks[0]); i++) {
		Asm_Free(stacks[i]->v);
	}
	Asm_Free(a);
	return hr < 0 ? hr : 0;
}
