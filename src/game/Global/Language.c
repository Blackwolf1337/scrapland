/*
 * The texts of the game (Global/Language.cpp): Language/<lang>.txt holds "id<TAB>text" lines. The whole file
 * stays in g_LangBuffer and the table points into it; every entry also keeps the id of the line after it, so the
 * entries can be walked in file order.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/scrap.h"

int crt_stricmp(const char *a, const char *b);
int crt_strncmp(const char *a, const char *b, size_t n);

HashTable *__thiscall HashTable_ctor(HashTable *self, int numSlots, byte caseSensitive);
uint32_t __thiscall HashTable_Slot(HashTable *self, const char *key);
void HashItem_Free(void *p);

typedef struct cLangText {
	char *text;			/* 0x0 */
	char *nextId;			/* 0x4 the id of the next line, NULL after the last */
} cLangText;

typedef struct cLangItem {
	cLangText value;		/* 0x0 */
	const char *key;		/* 0x8 */
	struct cLangItem *next;		/* 0xc */
} cLangItem;
SD_SIZE(cLangItem, 0x10);

#define g_LangTable (&GAME_DATA(HashTable, 0x428a20))	/* 457 slots, case-insensitive */
#define g_LangName ((cString *)Game_Ptr(0x44b618))		/* the language asked for */
#define g_LangDefault GAME_DATA(const char *, 0x44b61c)	/* the language when that file is missing */
#define g_LangBuffer GAME_DATA(char *, 0x44b620)
#define g_IdLang GAME_DATA(char *, 0x44b624)		/* the language loaded */

static int KeyEq(HashTable *t, const char *a, const char *b) {
	return t->caseSensitive ? strcmp(a, b) == 0 : crt_stricmp(a, b) == 0;
}

cLangItem *__thiscall LangTable_Find(HashTable *self, const char *key, uint32_t slot) {
	for (cLangItem *it = (cLangItem *)self->slots[slot]; it; it = it->next) {
		if (KeyEq(self, it->key, key)) {
			return it;
		}
	}
	return NULL;
}

cLangItem *__thiscall LangTable_Insert(HashTable *self, const char *key, uint32_t slot, const cLangText *value) {
	CHECK(!self->locked);
	if (!g_HashItemPool) {
		cMemPool *mem = operator_new(sizeof(*mem));
		g_HashItemPool = mem ? cMemPool_ctor(mem, "HashItem", g_HashItemValues) : NULL;
	}
	g_HashItemRefs++;
	cLangItem *it = cMemPool_Alloc(g_HashItemPool, sizeof(*it));
	if (it) {
		it->value = *value;
		it->next = NULL;
		it->key = NULL;
	}
	it->next = (cLangItem *)self->slots[slot];
	it->key = key;
	self->slots[slot] = (HashNode *)it;
	return it;
}

byte __thiscall LangTable_Add(HashTable *self, const char *key, const cLangText *value, byte warn) {
	CHECK(!self->locked);
	uint32_t slot = HashTable_Slot(self, key);
	if (LangTable_Find(self, key, slot)) {
		if (warn) {
			Con_Warnf("CHashTable3: Symbol already defined: '%s'\n", key);
		}
		return 0;
	}
	LangTable_Insert(self, key, slot, value);
	return 1;
}

byte __thiscall LangTable_Get(HashTable *self, const char *key, cLangText *out) {
	cLangItem *it = LangTable_Find(self, key, HashTable_Slot(self, key));
	if (!it) {
		return 0;
	}
	*out = it->value;
	return 1;
}

/* takes the item of key out of its slot, NULL if there is none */
cLangItem *__thiscall LangTable_Unlink(HashTable *self, const char *key, uint32_t slot) {
	cLangItem **link = (cLangItem **)&self->slots[slot];
	CHECK(!self->locked);
	for (; *link; link = &(*link)->next) {
		if (KeyEq(self, (*link)->key, key)) {
			cLangItem *it = *link;
			*link = it->next;
			it->next = NULL;
			return it;
		}
	}
	return NULL;
}

byte __thiscall LangTable_Delete(HashTable *self, const char *key) {
	CHECK(!self->locked);
	cLangItem *it = LangTable_Unlink(self, key, HashTable_Slot(self, key));
	if (!it) {
		return 0;
	}
	HashItem_Free(it);
	return 1;
}

void __fastcall LangTable_Clear(HashTable *self) {
	CHECK(!self->locked);
	if (!self->slots) {
		return;
	}
	for (int i = 0; i < (int)self->numSlots; i++) {
		while (self->slots[i]) {
			cLangItem *it = (cLangItem *)self->slots[i];
			self->slots[i] = (HashNode *)it->next;
			HashItem_Free(it);
		}
	}
	operator_delete_array(self->slots);
	self->numSlots = 0;
	self->slots = NULL;
}

void __fastcall LangTable_dtor(HashTable *self) {
	LangTable_Clear(self);
}

void Lang_Free(void) {
	HashTable *t = g_LangTable;

	for (;;) {
		int i = 0;
		if ((int)t->numSlots <= 0) {
			break;
		}
		while (!t->slots[i]) {
			if (++i >= (int)t->numSlots) {
				goto done;
			}
		}
		cLangItem *it = (cLangItem *)t->slots[i];
		if (!it) {
			break;
		}
		CHECK(LangTable_Delete(t, it->key));
	}
done:
	if (g_LangBuffer) {
		Mem_Free(g_LangBuffer);
		g_LangBuffer = NULL;
	}
	if (g_IdLang) {
		Mem_Free(g_IdLang);
		g_IdLang = NULL;
	}
}

const char *Lang_GetStr(const char *id) {
	cLangItem *it = LangTable_Find(g_LangTable, id, HashTable_Slot(g_LangTable, id));
	return it ? it->value.text : NULL;
}

const char *Lang_GetLanguage(void) {
	return g_IdLang;
}

/* the id of the first line (in file order) with that text, NULL if none */
const char *Lang_FindId(const char *text) {
	const char *id = g_LangBuffer;
	for (;;) {
		cLangItem *it = LangTable_Find(g_LangTable, id, HashTable_Slot(g_LangTable, id));
		if (!it) {
			return NULL;
		}
		if (!strcmp(it->value.text, text)) {
			return id;
		}
		id = it->value.nextId;
	}
}

/*
 * GetLangEntries(id, n): the ids from id on in file order while they keep the prefix of id without its last 4
 * characters; of every group of n lines the first id goes into the list, cut to its length - 3 (the group
 * number). At most 10.
 */
PyObject *Lang_GetEntries(const char *name, int n) {
	char prefix[0x100];
	char key[0x100];
	PyObject *items[10];
	int len = (int)strlen(name);
	int count = 0, j = 0;

	CHECK(len < 255);
	int pre = len - 4;
	Str_Copy(prefix, name, sizeof(prefix));
	Str_Copy(key, prefix, sizeof(key));
	if ((uint32_t)pre >= 0xff) {
		crt_report_rangecheckfailure();
	}
	prefix[pre] = 0;
	for (;;) {
		cLangItem *it = LangTable_Find(g_LangTable, key, HashTable_Slot(g_LangTable, key));
		if (!it) {
			break;
		}
		const char *next = it->value.nextId;
		if (++count == 1) {
			if ((uint32_t)(len - 3) >= 0xff) {
				crt_report_rangecheckfailure();
			}
			key[len - 3] = 0;
			items[j++] = PyString_FromString(key);
			CHECK(j < 10);
		} else if (count == n) {
			count = 0;
			len = (int)strlen(next);
		}
		if (crt_strncmp(next, prefix, pre)) {
			break;
		}
		Str_Copy(key, next, sizeof(key));
	}
	PyObject *list = PyList_New(j);
	CHECK(list);
	for (int i = 0; i < j; i++) {
		PyList_SetItem(list, i, items[i]);
	}
	return list;
}

/* parses the file into the table; a line goes up to CR, or to LF when nothing follows the CR */
void Lang_Load(const char *lang, cAbstractFile *f) {
	cLangText value;

	Lang_Free();
	CHECK(g_LangBuffer == NULL);
	CHECK(g_IdLang == NULL);
	for (int i = 0; i < (int)g_LangTable->numSlots; i++) {
		if (g_LangTable->slots[i]) {
			CHECK(g_LangTable->slots[i] == NULL);
			break;
		}
	}
	size_t n = strlen(lang) + 1;
	g_IdLang = Mem_Alloc(n);
	memcpy(g_IdLang, lang, n);
	int size = f->vftable->Size(f);
	g_LangBuffer = Mem_Alloc(size + 1);
	f->vftable->Read(f, g_LangBuffer, size);
	g_LangBuffer[size] = 0;
	char *id = g_LangBuffer;
	for (char *tab = strchr(id, '\t'); tab; tab = strchr(id, '\t')) {
		char *cr = strchr(id, '\r');
		char *lf = strchr(id, '\n');
		*tab = 0;
		char *text = tab + 1;
		if (cr) {
			*cr++ = 0;
		}
		if (lf) {
			*lf++ = 0;
		}
		int valid = *id && *text;
		char *next;
		if (cr && *cr) {
			next = cr;
		} else if (lf && *lf) {
			next = lf;
		} else {
			next = lf < cr ? cr : lf;
		}
		value.text = text;
		value.nextId = next;
		if (valid) {
			CHECK(!g_LangTable->locked);
			uint32_t slot = HashTable_Slot(g_LangTable, id);
			if (!LangTable_Find(g_LangTable, id, slot)) {
				LangTable_Insert(g_LangTable, id, slot, &value);
			} else {
				Con_Bluef("ERROR in language (%s):\n\tCan't be included '%s':'%s' \n", g_IdLang, id, text);
			}
		}
		if (!next) {
			break;
		}
		id = next;
	}
}

/*
 * loads Language/<lang>.txt (lang NULL: the one asked for before), falling back to the default language and to
 * Strings.txt; 0 if that language is already loaded and force is 0
 */
byte Lang_Set(const char *lang, byte force) {
	char path[0x200];

	if (!force && g_IdLang && !strcmp(g_IdLang, lang)) {
		return 0;
	}
	const char *name = *g_LangName;
	if (lang) {
		cString_Set(g_LangName, lang);
		name = lang;
	}
	cFile f = { { cFile_vftable, NULL, 0 }, NULL };
	Str_PrintfQuiet(path, sizeof(path), "Language/%s.txt", name);
	if (!cFile_Exists(&f, path)) {
		name = g_LangDefault;
		Str_PrintfQuiet(path, sizeof(path), "Language/%s.txt", name);
	}
	if (!cFile_Exists(&f, path)) {
		name = "Strings";
		Str_PrintfQuiet(path, sizeof(path), "Language/%s.txt", name);
	}
	if (!cFile_Exists(&f, path)) {
		Con_SysErrorf("Language file not found: %s\n\nReinstalling Scrapland should solve this problem.\n",
			*g_LangName);
	}
	cFile_Open(&f, path, FILE_MODE_READ);
	Lang_Load(name, &f.base);
	cFile_Close(&f);
	cFile_dtor(&f);
	return 1;
}
