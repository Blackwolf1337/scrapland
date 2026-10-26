/*
 * Data files of the engine3d as lists of lines, original source Util/Arch_dat.cpp (include/scrap/e3dutil.h).
 */
#include <string.h>

#include "scrap/e3dutil.h"

char *crt_strncat(char *dst, const char *src, size_t n);
int crt_strncmp(const char *a, const char *b, size_t n);
int crt_strnicmp(const char *a, const char *b, size_t n);
int *crt_errno(void);
void *crt_iob_func(unsigned index);
int crt_stdio_common_vfprintf(uint64_t options, void *f, const char *fmt, void *locale, va_list ap);
#define g_PrintfOptions GAME_DATA(uint64_t, 0x444210)

#define g_ArchErr GAME_DATA(int, 0x432a04)	/* the code of "Faltan datos", -1 until the first Arch_New */

#define IS_BLANK(c) ((c) == '\t' || (c) == ' ')

/* the inlined check at the start of every function */
static void Arch_Check(const cArchDatos *dat) {
	CHECK(dat);
	CHECK(dat->id == ARCHDATOS_ID);
}

/* Arch_LineType as the original inlines it in the searches, with only the second check */
static int Arch_TypeOf(cArchDatos *dat, int idx) {
	CHECK(dat->id == ARCHDATOS_ID);
	cLineaDat *lin = Arch_Line(dat, idx);
	if (!lin) {
		return LINEA_UNKNOWN;
	}
	if (lin->type == LINEA_UNKNOWN) {
		Arch_Classify(dat, idx);
	}
	return lin->type;
}

static const char *Arch_SkipBlanks(const char *s) {
	while (IS_BLANK(*s)) {
		s++;
	}
	return s;
}

static int Arch_Compare(const cArchDatos *dat, const char *a, const char *b, size_t n) {
	return dat->caseSensitive ? crt_strncmp(a, b, n) : crt_strnicmp(a, b, n);
}

void Arch_AddSection(cArchDatos *dat, const char *name, int idx) {
	char buf[MAX_RUTA];

	Arch_Check(dat);
	buf[0] = '[';
	buf[1] = '\0';
	crt_strncat(buf, name, 0xfe);
	memcpy(buf + strlen(buf), "]", 2);
	Arch_InsertAt(dat, buf, idx);
}

void Arch_AddLine(cArchDatos *dat, const char *text, int idx) {
	Arch_Check(dat);
	Arch_InsertAt(dat, text, idx);
}

void Arch_InsertAt(cArchDatos *dat, const char *text, int idx) {
	Arch_Check(dat);
	int last = dat->count - 1;
	CHECK(dat->id == ARCHDATOS_ID);
	if (idx > last) {
		idx = last;
	}
	if (idx < 0) {
		idx = 0;
	}
	cLineaDat *lin = Arch_Line(dat, idx);
	CHECK(lin);
	cLineaDat *p = Mem_Alloc(sizeof(*p));
	p->prev = lin;
	p->next = lin->next;
	p->type = LINEA_UNKNOWN;
	p->text = Cad_Dup(text);
	if (p->next) {
		p->next->prev = p;
	}
	if (p->prev) {
		p->prev->next = p;
	}
	dat->count++;
	dat->cur = p;
	dat->curIdx = idx + 1;
}

int Arch_StrnCmp(cArchDatos *dat, const char *a, const char *b, size_t n) {
	return Arch_Compare(dat, a, b, n);
}

int Arch_SectionEnd(cArchDatos *dat, int idx) {
	Arch_Check(dat);
	int last = dat->count - 1;
	int end = idx < 1 ? 0 : idx < last ? idx : last;

	for (int i = end + 1; i <= last; i++) {
		if (Arch_TypeOf(dat, i) == LINEA_SECTION) {
			return end;
		}
		if (Arch_LineType(dat, i) == LINEA_KEY) {
			end = i;
		}
	}
	return end;
}

int Arch_FindSection(cArchDatos *dat, const char *name, int from) {
	Arch_Check(dat);
	int last = dat->count - 1;
	size_t n = strlen(name);
	CHECK(dat->id == ARCHDATOS_ID);

	for (int i = from > 0 ? from : 1; i <= last; i++) {
		cLineaDat *lin = Arch_Line(dat, i);
		CHECK(lin);
		if (Arch_TypeOf(dat, i) != LINEA_SECTION) {
			continue;
		}
		const char *txt = lin->text;
		CHECK(txt);
		txt = Arch_SkipBlanks(txt);
		CHECK(*txt == '[');
		txt = Arch_SkipBlanks(txt + 1);
		CHECK(*txt != 0);
		if (!Arch_Compare(dat, txt, name, n) && *Arch_SkipBlanks(txt + n) == ']') {
			return i;
		}
	}
	return 0;
}

int Arch_FindKey(cArchDatos *dat, const char *key, int from, char *value, int size) {
	Arch_Check(dat);
	int last = dat->count - 1;
	size_t n = strlen(key);
	CHECK(dat->id == ARCHDATOS_ID);

	for (int i = from > 0 ? from : 1; i <= last; i++) {
		cLineaDat *lin = Arch_Line(dat, i);
		CHECK(lin);
		if (Arch_TypeOf(dat, i) == LINEA_SECTION) {
			return 0;
		}
		if (Arch_LineType(dat, i) != LINEA_KEY) {
			continue;
		}
		const char *txt = lin->text;
		CHECK(txt);
		txt = Arch_SkipBlanks(txt);
		CHECK(*txt != 0);
		if (Arch_Compare(dat, txt, key, n)) {
			continue;
		}
		const char *eq = Arch_SkipBlanks(txt + n);
		if (*eq != '=') {
			continue;
		}
		if (!value) {
			return i;
		}
		Cad_Copy(value, Arch_SkipBlanks(eq + 1), size);
		char *end = strchr(value, ';');
		if (!end) {
			end = value + strlen(value);
		}
		while (end - 1 >= value && IS_BLANK(end[-1])) {
			end--;
		}
		*end = '\0';
		return i;
	}
	return 0;
}

void Arch_SectionName(char *dest, const char *header, int size) {
	CHECK(header && dest);
	const char *txt = Arch_SkipBlanks(header);
	CHECK(*txt == '[');
	txt = Arch_SkipBlanks(txt + 1);
	CHECK(*txt != 0);
	const char *close = strchr(txt, ']');
	CHECK(close != NULL);
	const char *e = close;
	do {
		e--;
	} while (e > txt && IS_BLANK(*e));
	*dest = '\0';
	int n = (int)(e + 1 - txt);
	if (size - 1 <= n) {
		n = size - 1;
	}
	crt_strncat(dest, txt, n);
	CHECK(*close == ']');
}

void Arch_Classify(cArchDatos *dat, int idx) {
	Arch_Check(dat);
	cLineaDat *lin = Arch_Line(dat, idx);
	if (!lin || lin->type == LINEA_HEAD) {
		return;
	}
	CHECK(lin->text);
	char c = *Arch_SkipBlanks(lin->text);
	if (!c) {
		lin->type = LINEA_EMPTY;
	} else if (c == ';') {
		lin->type = LINEA_COMMENT;
	} else {
		lin->type = c == '[' ? LINEA_SECTION : LINEA_KEY;
	}
}

void Arch_Delete(cArchDatos *dat, int idx) {
	Arch_Check(dat);
	int last = dat->count - 1;
	CHECK(dat->id == ARCHDATOS_ID);
	if (idx <= 0 || idx > last) {
		return;
	}
	cLineaDat *lin = Arch_Line(dat, idx);
	CHECK(lin);
	if (lin->text) {
		Mem_Free(lin->text);
	}
	if (lin->next) {
		lin->next->prev = lin->prev;
	}
	if (lin->prev) {
		lin->prev->next = lin->next;
	}
	dat->count--;
	dat->curIdx = idx - 1;
	dat->cur = lin->prev;
	Mem_Free(lin);
}

void Arch_Error(int code, int fatal, cArchDatos *dat, int idx) {
	if (idx < 1) {
		idx = dat->curIdx;
	}
	cLineaDat *lin = Arch_Line(dat, idx);
	Err_Report(code, fatal, " (l\xednea %d):\n%s", idx, lin->text);
}

int Arch_Write(cArchDatos *dat, void *f) {
	Arch_Check(dat);
	int last = dat->count - 1;
	CHECK(dat->id == ARCHDATOS_ID);
	for (int i = 1; i <= last; i++) {
		cLineaDat *lin = Arch_Line(dat, i);
		CHECK(lin);
		if (crt_fprintf(f, "%s\n", lin->text) < 0) {
			return -1;
		}
	}
	return 0;
}

void Arch_Free(cArchDatos *dat) {
	Arch_Check(dat);
	while (dat->count != 1 && dat->count - 1 >= 0) {
		Arch_Delete(dat, 1);
		CHECK(dat->id == ARCHDATOS_ID);
	}
	Mem_Free(dat->head);
	dat->id = 0;
	Mem_Free(dat);
}

cArchDatos *Arch_New(void) {
	if (g_ArchErr == -1) {
		g_ArchErr = Err_Register("Faltan datos");
	}
	cArchDatos *dat = Mem_Alloc(sizeof(*dat));
	cLineaDat *head = Mem_Alloc(sizeof(*head));
	dat->head = head;
	dat->id = ARCHDATOS_ID;
	dat->count = 1;
	dat->cur = NULL;
	dat->curIdx = -1;
	dat->caseSensitive = 0;
	head->prev = NULL;
	head->next = NULL;
	head->type = LINEA_HEAD;
	head->text = NULL;
	return dat;
}

/* a line ends at the first control character other than tab; an errno left by the reads is fatal */
int Arch_Load(cArchDatos *dat, const char *name, int idx) {
	char buf[0x104];

	Arch_Check(dat);
	void *f = ArchIO_Open(name, "rb");
	if (!f) {
		return -1;
	}
	*crt_errno() = 0;
	while (ArchIO_Gets(buf, 0x101, f)) {
		unsigned i = 0;
		while ((byte)buf[i] >= 0x20 || buf[i] == '\t') {
			i++;
		}
		if (i >= 0x101) {
			crt_report_rangecheckfailure();
		}
		buf[i] = '\0';
		Arch_InsertAt(dat, buf, idx++);
	}
	ArchIO_Close(f);
	if (*crt_errno()) {
		Err_Report(*crt_errno(), 1, " (%s)", name);
	}
	return 0;
}

const char *Arch_Value(cArchDatos *dat, int idx, int *len) {
	cLineaDat *lin = Arch_Line(dat, idx);
	if (!lin || Arch_LineType(dat, idx) != LINEA_KEY) {
		return NULL;
	}
	const char *txt = lin->text;
	CHECK(txt);
	txt = Arch_SkipBlanks(txt);
	if (*txt) {
		while (*txt && !IS_BLANK(*txt) && *txt != '=') {
			txt++;
		}
		txt = Arch_SkipBlanks(txt);
	}
	if (*txt != '=') {
		return NULL;
	}
	const char *v = Arch_SkipBlanks(txt + 1);
	if (*v == '\0' || *v == ';') {
		if (len) {
			*len = 0;
		}
		return v;
	}
	if (!len) {
		return v;
	}
	const char *e = strchr(v, ';');
	if (!e) {
		e = v + strlen(v);
	}
	do {
		e--;
	} while (IS_BLANK(*e));
	*len = (int)(e + 1 - v);
	return v;
}

void Arch_SetValue(cArchDatos *dat, const char *value, int idx) {
	char buf[0x104];
	int len;

	const char *v = Arch_Value(dat, idx, &len);
	if (!v) {
		return;
	}
	cLineaDat *lin = Arch_Line(dat, idx);
	const char *rest = Arch_SkipBlanks(v + len);
	if (*rest) {
		int pad = (int)(rest - v) - (int)strlen(value);
		Bprintf(buf, 0x101, "%.*s%s%*s%s", (int)(v - lin->text), lin->text, value, pad >= 0 ? pad : 0, "", rest);
	} else {
		Bprintf(buf, 0x101, "%.*s%s", (int)(v - lin->text), lin->text, value);
	}
	Arch_Delete(dat, idx);
	Arch_Check(dat);
	Arch_InsertAt(dat, buf, idx - 1);
}

int Arch_Count(cArchDatos *dat) {
	Arch_Check(dat);
	return dat->count - 1;
}

void Arch_Dump(cArchDatos *dat, int from, int to) {
	Arch_Check(dat);
	int i = from > 0 ? from : 0;
	CHECK(dat->id == ARCHDATOS_ID);
	if (to > dat->count - 1) {
		CHECK(dat->id == ARCHDATOS_ID);
		to = dat->count - 1;
	}
	for (; i <= to; i++) {
		cLineaDat *lin = Arch_Line(dat, i);
		CHECK(lin);
		CHECK(!lin->next || lin->next->prev == lin);
		CHECK(!lin->prev || lin->prev->next == lin);
		Arch_Printf("\n%3d %d  %s", i, Arch_TypeOf(dat, i), lin->text);
	}
}

/* the cached line is the start when it is nearer than the head */
cLineaDat *Arch_Line(cArchDatos *dat, int idx) {
	Arch_Check(dat);
	if (idx < 0) {
		return NULL;
	}
	CHECK(dat->id == ARCHDATOS_ID);
	if (idx > dat->count - 1) {
		return NULL;
	}
	int i = dat->curIdx;
	if (i == idx) {
		return dat->cur;
	}
	cLineaDat *lin = dat->cur;
	if (i < 0 || idx * 2 < i) {
		lin = dat->head;
		i = 0;
	} else {
		while (idx < i) {
			if (!lin) {
				return NULL;
			}
			lin = lin->prev;
			i--;
		}
	}
	while (i < idx) {
		if (!lin) {
			return NULL;
		}
		lin = lin->next;
		i++;
	}
	if (lin) {
		dat->cur = lin;
		dat->curIdx = idx;
	}
	return lin;
}

int Arch_LineType(cArchDatos *dat, int idx) {
	Arch_Check(dat);
	cLineaDat *lin = Arch_Line(dat, idx);
	if (!lin) {
		return LINEA_UNKNOWN;
	}
	if (lin->type == LINEA_UNKNOWN) {
		Arch_Classify(dat, idx);
	}
	return lin->type;
}

void Arch_Printf(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	crt_stdio_common_vfprintf(g_PrintfOptions, crt_iob_func(1), fmt, NULL, ap);
	va_end(ap);
}
