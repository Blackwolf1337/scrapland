/*
 * Data files of the engine3d as lists of lines, original source Util/Arch_dat.cpp (include/scrap/e3dutil.h).
 */
#include <stdio.h>
#include <string.h>

#include "scrap/archio.h"
#include "scrap/e3dutil.h"



#define IS_BLANK(c) ((c) == '\t' || (c) == ' ')

/* the inlined check at the start of every function */
static void DataFile_Check(const cDataFile *dat) {
	CHECK(dat);
	CHECK(dat->id == DATAFILE_ID);
}

/* DataFile_LineType as the original inlines it in the searches, with only the second check */
static int DataFile_TypeOf(cDataFile *dat, int idx) {
	CHECK(dat->id == DATAFILE_ID);
	cDatLine *lin = DataFile_Line(dat, idx);
	if (!lin) {
		return LINE_UNKNOWN;
	}
	if (lin->type == LINE_UNKNOWN) {
		DataFile_Classify(dat, idx);
	}
	return lin->type;
}

static const char *DataFile_SkipBlanks(const char *s) {
	while (IS_BLANK(*s)) {
		s++;
	}
	return s;
}

static int DataFile_Compare(const cDataFile *dat, const char *a, const char *b, size_t n) {
	return dat->caseSensitive ? crt_strncmp(a, b, n) : crt_strnicmp(a, b, n);
}

void DataFile_AddSection(cDataFile *dat, const char *name, int idx) {
	char buf[MAX_FILEPATH];

	DataFile_Check(dat);
	buf[0] = '[';
	buf[1] = '\0';
	crt_strncat(buf, name, 0xfe);
	memcpy(buf + strlen(buf), "]", 2);
	DataFile_InsertAt(dat, buf, idx);
}

void DataFile_AddLine(cDataFile *dat, const char *text, int idx) {
	DataFile_Check(dat);
	DataFile_InsertAt(dat, text, idx);
}

void DataFile_InsertAt(cDataFile *dat, const char *text, int idx) {
	DataFile_Check(dat);
	int last = dat->count - 1;
	CHECK(dat->id == DATAFILE_ID);
	if (idx > last) {
		idx = last;
	}
	if (idx < 0) {
		idx = 0;
	}
	cDatLine *lin = DataFile_Line(dat, idx);
	CHECK(lin);
	cDatLine *p = Mem_Alloc(sizeof(*p));
	p->prev = lin;
	p->next = lin->next;
	p->type = LINE_UNKNOWN;
	p->text = String_Dup(text);
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

int DataFile_StrnCmp(cDataFile *dat, const char *a, const char *b, size_t n) {
	return DataFile_Compare(dat, a, b, n);
}

int DataFile_SectionEnd(cDataFile *dat, int idx) {
	DataFile_Check(dat);
	int last = dat->count - 1;
	int end = idx < 1 ? 0 : idx < last ? idx : last;

	for (int i = end + 1; i <= last; i++) {
		if (DataFile_TypeOf(dat, i) == LINE_SECTION) {
			return end;
		}
		if (DataFile_LineType(dat, i) == LINE_KEY) {
			end = i;
		}
	}
	return end;
}

int DataFile_FindSection(cDataFile *dat, const char *name, int from) {
	DataFile_Check(dat);
	int last = dat->count - 1;
	size_t n = strlen(name);
	CHECK(dat->id == DATAFILE_ID);

	for (int i = from > 0 ? from : 1; i <= last; i++) {
		cDatLine *lin = DataFile_Line(dat, i);
		CHECK(lin);
		if (DataFile_TypeOf(dat, i) != LINE_SECTION) {
			continue;
		}
		const char *txt = lin->text;
		CHECK(txt);
		txt = DataFile_SkipBlanks(txt);
		CHECK(*txt == '[');
		txt = DataFile_SkipBlanks(txt + 1);
		CHECK(*txt != 0);
		if (!DataFile_Compare(dat, txt, name, n) && *DataFile_SkipBlanks(txt + n) == ']') {
			return i;
		}
	}
	return 0;
}

int DataFile_FindKey(cDataFile *dat, const char *key, int from, char *value, int size) {
	DataFile_Check(dat);
	int last = dat->count - 1;
	size_t n = strlen(key);
	CHECK(dat->id == DATAFILE_ID);

	for (int i = from > 0 ? from : 1; i <= last; i++) {
		cDatLine *lin = DataFile_Line(dat, i);
		CHECK(lin);
		if (DataFile_TypeOf(dat, i) == LINE_SECTION) {
			return 0;
		}
		if (DataFile_LineType(dat, i) != LINE_KEY) {
			continue;
		}
		const char *txt = lin->text;
		CHECK(txt);
		txt = DataFile_SkipBlanks(txt);
		CHECK(*txt != 0);
		if (DataFile_Compare(dat, txt, key, n)) {
			continue;
		}
		const char *eq = DataFile_SkipBlanks(txt + n);
		if (*eq != '=') {
			continue;
		}
		if (!value) {
			return i;
		}
		String_Copy(value, DataFile_SkipBlanks(eq + 1), size);
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

void DataFile_SectionName(char *dest, const char *header, int size) {
	CHECK(header && dest);
	const char *txt = DataFile_SkipBlanks(header);
	CHECK(*txt == '[');
	txt = DataFile_SkipBlanks(txt + 1);
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

void DataFile_Classify(cDataFile *dat, int idx) {
	DataFile_Check(dat);
	cDatLine *lin = DataFile_Line(dat, idx);
	if (!lin || lin->type == LINE_HEAD) {
		return;
	}
	CHECK(lin->text);
	char c = *DataFile_SkipBlanks(lin->text);
	if (!c) {
		lin->type = LINE_EMPTY;
	} else if (c == ';') {
		lin->type = LINE_COMMENT;
	} else {
		lin->type = c == '[' ? LINE_SECTION : LINE_KEY;
	}
}

void DataFile_Delete(cDataFile *dat, int idx) {
	DataFile_Check(dat);
	int last = dat->count - 1;
	CHECK(dat->id == DATAFILE_ID);
	if (idx <= 0 || idx > last) {
		return;
	}
	cDatLine *lin = DataFile_Line(dat, idx);
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

int DataFile_Write(cDataFile *dat, void *f) {
	DataFile_Check(dat);
	int last = dat->count - 1;
	CHECK(dat->id == DATAFILE_ID);
	for (int i = 1; i <= last; i++) {
		cDatLine *lin = DataFile_Line(dat, i);
		CHECK(lin);
		if (crt_fprintf(f, "%s\n", lin->text) < 0) {
			return -1;
		}
	}
	return 0;
}

void DataFile_Free(cDataFile *dat) {
	DataFile_Check(dat);
	while (dat->count != 1 && dat->count - 1 >= 0) {
		DataFile_Delete(dat, 1);
		CHECK(dat->id == DATAFILE_ID);
	}
	Mem_Free(dat->head);
	dat->id = 0;
	Mem_Free(dat);
}

int g_FileErr = -1;

cDataFile *DataFile_New(void) {
	if (g_FileErr == -1) {
		g_FileErr = Err_Register("Faltan datos");
	}
	cDataFile *dat = Mem_Alloc(sizeof(*dat));
	cDatLine *head = Mem_Alloc(sizeof(*head));
	dat->head = head;
	dat->id = DATAFILE_ID;
	dat->count = 1;
	dat->cur = NULL;
	dat->curIdx = -1;
	dat->caseSensitive = 0;
	head->prev = NULL;
	head->next = NULL;
	head->type = LINE_HEAD;
	head->text = NULL;
	return dat;
}

/* a line ends at the first control character other than tab; an errno left by the reads is fatal */
int DataFile_Load(cDataFile *dat, const char *name, int idx) {
	char buf[0x104];

	DataFile_Check(dat);
	void *f = FileIO_Open(name, "rb");
	if (!f) {
		return -1;
	}
	*crt_errno() = 0;
	while (FileIO_Gets(buf, 0x101, f)) {
		unsigned i = 0;
		while ((byte)buf[i] >= 0x20 || buf[i] == '\t') {
			i++;
		}
		if (i >= 0x101) {
			crt_report_rangecheckfailure();
		}
		buf[i] = '\0';
		DataFile_InsertAt(dat, buf, idx++);
	}
	FileIO_Close(f);
	if (*crt_errno()) {
		Err_Report(*crt_errno(), 1, " (%s)", name);
	}
	return 0;
}

const char *DataFile_Value(cDataFile *dat, int idx, int *len) {
	cDatLine *lin = DataFile_Line(dat, idx);
	if (!lin || DataFile_LineType(dat, idx) != LINE_KEY) {
		return NULL;
	}
	const char *txt = lin->text;
	CHECK(txt);
	txt = DataFile_SkipBlanks(txt);
	if (*txt) {
		while (*txt && !IS_BLANK(*txt) && *txt != '=') {
			txt++;
		}
		txt = DataFile_SkipBlanks(txt);
	}
	if (*txt != '=') {
		return NULL;
	}
	const char *v = DataFile_SkipBlanks(txt + 1);
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

void DataFile_SetValue(cDataFile *dat, const char *value, int idx) {
	char buf[0x104];
	int len;

	const char *v = DataFile_Value(dat, idx, &len);
	if (!v) {
		return;
	}
	cDatLine *lin = DataFile_Line(dat, idx);
	const char *rest = DataFile_SkipBlanks(v + len);
	if (*rest) {
		int pad = (int)(rest - v) - (int)strlen(value);
		Bprintf(buf, 0x101, "%.*s%s%*s%s", (int)(v - lin->text), lin->text, value, pad >= 0 ? pad : 0, "", rest);
	} else {
		Bprintf(buf, 0x101, "%.*s%s", (int)(v - lin->text), lin->text, value);
	}
	DataFile_Delete(dat, idx);
	DataFile_Check(dat);
	DataFile_InsertAt(dat, buf, idx - 1);
}

int DataFile_Count(cDataFile *dat) {
	DataFile_Check(dat);
	return dat->count - 1;
}

void DataFile_Dump(cDataFile *dat, int from, int to) {
	DataFile_Check(dat);
	int i = from > 0 ? from : 0;
	CHECK(dat->id == DATAFILE_ID);
	if (to > dat->count - 1) {
		CHECK(dat->id == DATAFILE_ID);
		to = dat->count - 1;
	}
	for (; i <= to; i++) {
		cDatLine *lin = DataFile_Line(dat, i);
		CHECK(lin);
		CHECK(!lin->next || lin->next->prev == lin);
		CHECK(!lin->prev || lin->prev->next == lin);
		DataFile_Printf("\n%3d %d  %s", i, DataFile_TypeOf(dat, i), lin->text);
	}
}

/* the cached line is the start when it is nearer than the head */
cDatLine *DataFile_Line(cDataFile *dat, int idx) {
	DataFile_Check(dat);
	if (idx < 0) {
		return NULL;
	}
	CHECK(dat->id == DATAFILE_ID);
	if (idx > dat->count - 1) {
		return NULL;
	}
	int i = dat->curIdx;
	if (i == idx) {
		return dat->cur;
	}
	cDatLine *lin = dat->cur;
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

int DataFile_LineType(cDataFile *dat, int idx) {
	DataFile_Check(dat);
	cDatLine *lin = DataFile_Line(dat, idx);
	if (!lin) {
		return LINE_UNKNOWN;
	}
	if (lin->type == LINE_UNKNOWN) {
		DataFile_Classify(dat, idx);
	}
	return lin->type;
}

void DataFile_Printf(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
}
